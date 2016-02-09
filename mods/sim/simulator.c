#include <linux/init.h>
#include <linux/module.h>

#include <asm/current.h>
#include <linux/sched.h>

#include <linux/gpio.h>
#include <plat/dmtimer.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <asm/io.h>

#include <linux/stat.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <asm/uaccess.h>
#include <linux/clk.h>

#include "simulation.h"
#include "json.h"
#include "spi_slave.h"
#include "userspace_interface.h"

#include "spi_controller_bin.h"
#include "../pruss/pruss.h"

MODULE_LICENSE("GPL");

struct pp_t pp_sim; /* punch press simulator data */

#define UPDATE_FREQUENCY	3000
#define UPDATE_PERIOD_US	(1000000 / UPDATE_FREQUENCY)

#define PIN_NUM(GPIO, PIN)	(PIN + GPIO * 32)

// #define SPI_CS_PIN_NUM	PIN_NUM(3, 19)	// P9_27 - GPIO3_19 (9a4)

#define RESET_PIN_NUM		PIN_NUM(0, 10)	// P8_31 - GPIO0_10 (8d8)
#define IRQ_PIN_NUM		PIN_NUM(0, 11)	// P8_32 - GPIO0_11 (8dc)
#define FAIL_PIN_NUM		PIN_NUM(0, 9)	// P8_33 - GPIO0_9  (8d4)
#define HEAD_UP_PIN_NUM		PIN_NUM(2, 17)	// P8_34 - GPIO2_17 (8cc)

#define SAFE_L_PIN_NUM		PIN_NUM(2, 12)	// P8_39 - GPIO2_12 (8b8)
#define SAFE_R_PIN_NUM		PIN_NUM(2, 13)	// P8_40 - GPIO2_13 (8bc)
#define SAFE_T_PIN_NUM		PIN_NUM(2, 10)	// P8_41 - GPIO2_10 (8b0)
#define SAFE_B_PIN_NUM		PIN_NUM(2, 11)	// P8_42 - GPIO2_11 (8b4)

#define ENC_X0_PIN_NUM		PIN_NUM(2,  8)	// P8_43 - GPIO2_8  (8a8)
#define ENC_X1_PIN_NUM		PIN_NUM(2,  9)	// P8_44 - GPIO2_9  (8ac)
#define ENC_Y0_PIN_NUM		PIN_NUM(2,  6)	// P8_45 - GPIO2_6  (8a0)
#define ENC_Y1_PIN_NUM		PIN_NUM(2,  7)	// P8_46 - GPIO2_7  (8a4)

static struct mcspi_slave_device * spi_slave;

static struct omap_dm_timer *timer = 0;

static int timer_irq_number = -1;

const char *module_name = "simulator";

static struct device * pru;
#define PRU_SIM_STATE_VAR           0x0 // first byte contains renspose to the encoders command, second byte response to the errors command; the simulator writes this value
#define PRU_PWRX_VAR                0x4
#define PRU_PWRY_VAR                0x5
#define PRU_IRQ_EN_VAR              0x6
#define PRU_PUNCH_VAR               0x7

// CM_PER (Clock Module Peripheral Registers
#define CM_PER_START_ADDR 0x44e00000
#define CM_PER_SIZE       0x400
#define CM_PER_GPIO2_CLKCTRL 0xb0


static void set_spi_sensor_values(struct pp_t * pp, u32 update_result)
{
	u32 sim_state = (update_result & 0xf) | ((update_result << 4) & 0x3f00);

	if (update_result & US_HEAD_UP)
	{
		pruss_writeb(pru, PRU_PUNCH_VAR, 0);
	}

	pruss_writel(pru, PRU_SIM_STATE_VAR, sim_state);
}

static void get_spi_actuator_values(struct pp_t * pp)
{
	u32 actuators;
	pruss_readl(pru, PRU_PWRX_VAR, &actuators);
	pp->x_axis.power = actuators & 0xff;
	pp->y_axis.power = (actuators >> 8) & 0xff;
	pp->irq_enabled = ((actuators >> 16) & 0xff) ? 1 : 0;
	pp->punch = ((actuators >> 24) & 0xff) ? 1 : 0;
}

static void reset_spi_actuator_values(void)
{
	pruss_writel(pru, PRU_PWRX_VAR, 0);
}


static int prev_reset_value = 1;
static int counter = 0;

static irqreturn_t timer_irq_handler(int a, void *b)
{
	static int irq_val = 0;
	static u32 last_retval = 0;
	u32 retval;
	int reset_value = gpio_get_value(RESET_PIN_NUM);

	omap_dm_timer_write_status(timer, OMAP_TIMER_INT_OVERFLOW); // clear interrupt flag
	
	if (!reset_value && prev_reset_value) // detect controller reset and reset the plant in that case
	{
		pp_reinit(&pp_sim);
		reset_spi_actuator_values();

		retval = pp_update(&pp_sim, UPDATE_PERIOD_US);
		set_spi_sensor_values(&pp_sim, retval);

		last_retval = retval;
	}

	prev_reset_value = reset_value;

	if (reset_value)
	{
		get_spi_actuator_values(&pp_sim);

		retval = pp_update(&pp_sim, UPDATE_PERIOD_US);



	if (counter++ % 1000 == 0) {
		printk(KERN_ALERT "Retval: %x\n", retval); 
	}


		set_spi_sensor_values(&pp_sim, retval);

		// send encoder interrupt if enabled and something has changed
		if (pp_sim.irq_enabled && retval != last_retval)
		{
			irq_val = !irq_val;
			gpio_set_value(IRQ_PIN_NUM, irq_val);
		}

		gpio_set_value(HEAD_UP_PIN_NUM, retval & US_HEAD_UP);
		gpio_set_value(FAIL_PIN_NUM, retval & US_FAIL);
		gpio_set_value(ENC_X0_PIN_NUM, retval & US_ENC_X0);
		gpio_set_value(ENC_X1_PIN_NUM, retval & US_ENC_X1);
		gpio_set_value(ENC_Y0_PIN_NUM, retval & US_ENC_Y0);
		gpio_set_value(ENC_Y1_PIN_NUM, retval & US_ENC_Y1);
		gpio_set_value(SAFE_L_PIN_NUM, retval & US_SAFE_L);
		gpio_set_value(SAFE_R_PIN_NUM, retval & US_SAFE_R);
		gpio_set_value(SAFE_T_PIN_NUM, retval & US_SAFE_T);
		gpio_set_value(SAFE_B_PIN_NUM, retval & US_SAFE_B);

		last_retval = retval;
	}

	return IRQ_HANDLED;
}

#define PRINT_TIMER_CAP(timer, cap) \
	do { \
		if ((timer)->capability & (cap)) \
		printk(KERN_ALERT "Timer has capability: " #cap "\n"); \
		else \
		printk(KERN_ALERT "Time does not have capability: " #cap "\n"); \
	} while (0)

#define PIN_COUNT	12
struct pin_def_t
{
	unsigned int number;
	unsigned int flags;
	bool allocated;
};

static struct pin_def_t pins[PIN_COUNT] =
{
	{
		.number = RESET_PIN_NUM,
		.flags = GPIOF_IN,
		.allocated = false,
	},
	{
		.number = IRQ_PIN_NUM,
		.flags = GPIOF_OUT_INIT_LOW,
		.allocated = false,
	},
	{
		.number = HEAD_UP_PIN_NUM,
		.flags = GPIOF_OUT_INIT_LOW,
		.allocated = false,
	},		
	{
		.number = FAIL_PIN_NUM,
		.flags = GPIOF_OUT_INIT_LOW,
		.allocated = false,
	},		
	{
		.number = ENC_X0_PIN_NUM,
		.flags = GPIOF_OUT_INIT_LOW,
		.allocated = false,
	},		
	{
		.number = ENC_X1_PIN_NUM,
		.flags = GPIOF_OUT_INIT_LOW,
		.allocated = false,
	},		
	{
		.number = ENC_Y0_PIN_NUM,
		.flags = GPIOF_OUT_INIT_LOW,
		.allocated = false,
	},		
	{
		.number = ENC_Y1_PIN_NUM,
		.flags = GPIOF_OUT_INIT_LOW,
		.allocated = false,
	},		
	{
		.number = SAFE_L_PIN_NUM,
		.flags = GPIOF_OUT_INIT_LOW,
		.allocated = false,
	},		
	{
		.number = SAFE_R_PIN_NUM,
		.flags = GPIOF_OUT_INIT_LOW,
		.allocated = false,
	},		
	{
		.number = SAFE_T_PIN_NUM,
		.flags = GPIOF_OUT_INIT_LOW,
		.allocated = false,
	},		
	{
		.number = SAFE_B_PIN_NUM,
		.flags = GPIOF_OUT_INIT_LOW,
		.allocated = false,
	},		
};

static void free_pins(void)
{
	int i;
	for (i = 0; i < PIN_COUNT; ++i)
	{
		if (pins[i].allocated)
		{
			gpio_free(pins[i].number);
		}
	}
}

#define INTERRUPT_PRIORITY	50

static int request_pins(void)
{
	int i, err;
	struct pin_def_t * it;
	void __iomem *cm_per;

	for (i = 0; i < PIN_COUNT; ++i)
	{
		it = &pins[i];
		err = gpio_request_one(it->number, it->flags, module_name);
		if (err)
		{
			printk(KERN_ALERT "Error allocating pin %d.\n", it->number);
			goto fail_gpio_request;
		}
		printk(KERN_ALERT "Allocated pin %d with flags %X result %d.\n", it->number, it->flags, err);
		it->allocated = true;
	}

	/*
	Enable GPIO2 clock
	*/
	cm_per = ioremap(CM_PER_START_ADDR, CM_PER_SIZE);
	if(!cm_per) {
		printk (KERN_ERR "HI: ERROR: Failed to remap memory for CM_PER.\n");
		goto fail_gpio_request;
	}
	iowrite32(0x02, cm_per + CM_PER_GPIO2_CLKCTRL);
	iounmap(cm_per);

	return 0;
fail_gpio_request:
	free_pins();
	return err;
}

static int simulator_init(void)
{
	int err;
	unsigned int init_val;
	u32 clk_rate, timer_period;

	printk(KERN_ALERT "Simulator module\n");

	/* Init simulator before starting other code (irq, thread, spi) */
	pp_init(&pp_sim);

	err = request_pins();
	if (err)
		goto fail_gpio_request;

	spi_slave = mcspi_slave_get_one();
	if (spi_slave == NULL)
	{
		err = -ENODEV;
		printk(KERN_ALERT "Unable to get spi slave device.\n");
		goto fail_spi_get;
	}

	mcspi_slave_enable(spi_slave, 8);

	// TODO: While mcspi uses the device tree to obtain a free mcspi (there are two on BBB), the PRU code (in spi_controller.p) plainly assumes that SPI0 is used.

	pru = pruss_get();
	if (!pru)
	{
		err = -ENODEV;
		goto fail_pru_request;
	}

	pruss_enable(pru, PRUSS_NUM0);
	pruss_load(pru, PRUSS_NUM0, PRUcode, ARRAY_SIZE(PRUcode));
	pruss_run(pru, PRUSS_NUM0);

	// acquire timer
	timer = omap_dm_timer_request(); // requesting any timer
	if (!timer) {
		printk(KERN_ALERT "No timer acquired.\n");
		err = -EBUSY;
		goto fail_timer_request;
	}

	printk(KERN_ALERT "Timer acquired. Name: %s\n", timer->pdev->name);


	// print timer information
	PRINT_TIMER_CAP(timer, OMAP_TIMER_NEEDS_RESET);
	PRINT_TIMER_CAP(timer, OMAP_TIMER_ALWON);
	PRINT_TIMER_CAP(timer, OMAP_TIMER_HAS_PWM);
	PRINT_TIMER_CAP(timer, OMAP_TIMER_SECURE);
	PRINT_TIMER_CAP(timer, OMAP_TIMER_HAS_DSP_IRQ);


	// acquire timer interrupt line
	timer_irq_number = omap_dm_timer_get_irq(timer);
	err = request_irq(timer_irq_number, timer_irq_handler, IRQF_NO_THREAD, module_name, NULL);
	if (err) {
		printk(KERN_ALERT "Timer interrupt line not acquired.\n");
		goto fail_timer_irq;
	}

	err = irq_set_priority(timer_irq_number, INTERRUPT_PRIORITY);
	if (err)
	{
		printk(KERN_ALERT "UNABLE to set irq priority\n");
		goto fail_cdev_setup;
	}

	printk(KERN_ALERT "Interrupt line acquired, numer: %d.\n", timer_irq_number);

	// setup timer
	omap_dm_timer_set_prescaler(timer, -1); // no prescaler
	omap_dm_timer_set_source(timer, OMAP_TIMER_SRC_SYS_CLK); // expected to be 24MHz
	clk_rate = clk_get_rate(omap_dm_timer_get_fclk(timer));
	timer_period = clk_rate / UPDATE_FREQUENCY;
	printk(KERN_ALERT "timer frequency: %uHz, timer period: %u ticks\n", clk_rate, timer_period);
	printk(KERN_ALERT "update period: %i us", UPDATE_PERIOD_US);

	init_val = 0xffffffff - (timer_period - 1);
	omap_dm_timer_set_load_start(timer, 1, init_val); // set load value and start

	omap_dm_timer_set_int_enable(timer, OMAP_TIMER_INT_OVERFLOW); // does enale interrupts!

	// setup character device
	err = userinterface_setup();
	if (err)
	{
		goto fail_cdev_setup;
	}

	reset_spi_actuator_values();

	return 0;

fail_cdev_setup:
	free_irq(timer_irq_number, NULL);
fail_timer_irq:
	omap_dm_timer_free(timer);
fail_timer_request:
	pruss_disable(pru, PRUSS_NUM0);
fail_pru_request:
	mcspi_slave_free(spi_slave);
fail_spi_get:
	free_pins();
fail_gpio_request:
	return err;
}

static void simulator_exit(void)
{
	printk(KERN_ALERT "Simulator ending\n");

	userinterface_release();

	local_irq_disable_all();
	omap_dm_timer_stop(timer);
	local_irq_enable_all();
	irq_set_priority(timer_irq_number, LOWEST_IRQ_PRIORITY);
	free_irq(timer_irq_number, NULL);

	if (omap_dm_timer_free(timer)) {
		printk(KERN_ALERT "Unable to release timer.\n");
	}
	
	pruss_disable(pru, PRUSS_NUM0);

	mcspi_slave_disable(spi_slave);
	mcspi_slave_free(spi_slave);
	free_pins();
}

module_init(simulator_init);
module_exit(simulator_exit);

