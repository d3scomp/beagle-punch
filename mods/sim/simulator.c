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

#define RESET_PIN_NUM	44
#define ENC_CHANGE_PIN_NUM	26

static struct mcspi_slave_device * spi_slave;

static struct omap_dm_timer *timer = 0;

static int timer_irq_number = -1;

const char *module_name = "simulator";

static struct device * pru;
#define	PRU_SENSORS_ADDRESS	0x0
#define PRU_ACTUATORS_ADDRESS	0x4

static void set_spi_sensor_values(struct pp_t * pp, update_state_t update_result)
{
	u32 pp_encoders = pp_sim.x_axis.encoder | (pp_sim.y_axis.encoder << 2);
	u32 pp_errors = (pp_punch_in_progress(&pp_sim) ? 1 : 0) | (pp_fail(&pp_sim) ? 2 : 0) |
		((pp_sim.x_axis.errors & 0x3) << 2) | ((pp_sim.y_axis.errors & 0x3) << 4);
	if (update_result & US_PUNCH_START)
	{
		pruss_writeb(pru, PRU_ACTUATORS_ADDRESS + 3, 0);
	}

	pruss_writel(pru, PRU_SENSORS_ADDRESS, pp_encoders | (pp_errors << 8));
}

static void get_spi_actuator_values(struct pp_t * pp)
{
	u32 actuators;
	pruss_readl(pru, PRU_ACTUATORS_ADDRESS, &actuators);
	pp->x_axis.power = actuators & 0xff;
	pp->y_axis.power = (actuators >> 8) & 0xff;
	pp->irq_enabled = ((actuators >> 16) & 0xff) ? 1 : 0;
	if (actuators & 0xff000000)
	{
		pp_punch(pp);
	}
}

static void reset_spi_actuator_values(void)
{
	pruss_writel(pru, PRU_ACTUATORS_ADDRESS, 0);
}


static irqreturn_t reset_line(int irq, void * data)
{
	pp_reinit(&pp_sim);
	set_spi_sensor_values(&pp_sim, 0);
	reset_spi_actuator_values();
	return IRQ_HANDLED;
}

static int prev_reset_value = 1;

static irqreturn_t timer_irq_handler(int a, void *b)
{
	static int enc_val = 0;
	update_state_t retval;
	omap_dm_timer_write_status(timer, OMAP_TIMER_INT_OVERFLOW); // clear interrupt flag
	
	if (!gpio_get_value(RESET_PIN_NUM)) // detect controller reset and reset the plant in that case
	{
		if (prev_reset_value)
		{
			reset_line(0, 0);
			prev_reset_value = 0;
		}
	}
	else
	{
		prev_reset_value = 1;
		get_spi_actuator_values(&pp_sim);

		retval = pp_update(&pp_sim, UPDATE_PERIOD_US);

		set_spi_sensor_values(&pp_sim, retval);
		// send encoder interrupt if enabled and encoder value changed
		if (pp_sim.irq_enabled && (retval & (US_PUNCH_START | US_PUNCH_END | US_ENC_CHANGE | US_ERR_CHANGE)))
		{
			enc_val = !enc_val;
			gpio_set_value(ENC_CHANGE_PIN_NUM, enc_val);
		}
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

#define PIN_COUNT	2
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
		.number = ENC_CHANGE_PIN_NUM,
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
	for (i = 0; i < PIN_COUNT; ++i)
	{
		it = &pins[i];
		err = gpio_request_one(it->number, it->flags, module_name);
		if (err)
		{
			printk(KERN_ALERT "Error allocating pin %d.\n", it->number);
			goto fail_gpio_request;
		}
		it->allocated = true;
	}

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

	mcspi_slave_enable(spi_slave, 8);//16);

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

	reset_line(0, 0);

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

