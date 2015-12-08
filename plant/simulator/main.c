#include <linux/init.h>
#include <linux/module.h>

#include <asm/current.h>
#include <linux/sched.h>

#include <linux/gpio.h>
#include <plat/dmtimer.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <linux/kthread.h>

#include <linux/stat.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <asm/uaccess.h>
#include <linux/clk.h>

//#define DEBUG_SPI	1
#define PRU_SPI		1
#include "consumer.h"

#include "simulation.h"
#include "json.h"
#include "../spi_driver/spi_slave.h"
#include "userspace_interface.h"

#include "spi_controller_bin.h"
#include "../pru_driver/pruss.h"

MODULE_LICENSE("Dual BSD/GPL");

struct pp_t pp_sim; /* punch press simulator data */

#define UPDATE_FREQUENCY	3000
#define UPDATE_PERIOD_US	(1000000 / UPDATE_FREQUENCY)

#define RESET_PIN_NUM	44
#define ENC_CHANGE_PIN_NUM	26
#define SLAVE_SELECT_PIN_NUM	46

static struct mcspi_slave_device * spi_slave;

static struct omap_dm_timer *timer = 0;

static int timer_irq_number = -1;

static int spi_irq_number = -1;

const char *module_name = "simulator";

static struct task_struct * kthread;

#ifndef PRU_SPI
typedef enum {
	sc_none = 0,
	sc_encoders = 1,
	sc_errors = 2,
	sc_punch = 3,
	sc_pwrx = 4,
	sc_pwry = 5,
	sc_irq_en = 6,
	sc_count
} spi_command_t;

static spi_command_t cmd = sc_none;

static u8 spi_retval = 0;

static u8 pp_encoders = 0;
static u8 pp_errors = 0;

#ifdef DEBUG_SPI
static u8 * spi_buf;
static unsigned int sb_counter = 0;

static void add_spi_buf(u8 val)
{
	if (sb_counter < SPI_LOG_BUF_SIZE - 1)
	{
		spi_buf[sb_counter++] = val;
	}
}

static void add_spi(char type, u8 val)
{
	add_spi_buf(type);
	add_spi_buf(val);
	if (pp_sim.x_axis.velocity > 127)
		add_spi_buf(127);
	else if (pp_sim.x_axis.velocity < -127)
		add_spi_buf(-127);
	else
		add_spi_buf(pp_sim.x_axis.velocity);
	if (pp_sim.y_axis.velocity > 127)
		add_spi_buf(127);
	else if (pp_sim.y_axis.velocity < -127)
		add_spi_buf(-127);
	else
		add_spi_buf(pp_sim.y_axis.velocity);
	add_spi_buf(pp_fail(&pp_sim));
}

static void reset_spi_log(void)
{
	sb_counter = 0;
}
#else
#define add_spi_buf(val)
#define add_spi(type, val)
#define reset_spi_log()
#endif

#ifdef DEBUG_SPI
struct enc_seq_t
{
	uint8_t next_forward;
	uint8_t next_backwards;
};

struct enc_seq_t enc_sequence[] =
{
	{ .next_forward = 1, .next_backwards = 2 },
	{ .next_forward = 3, .next_backwards = 0 },
	{ .next_forward = 0, .next_backwards = 3 },
	{ .next_forward = 2, .next_backwards = 1 }
};

static int check_enc(unsigned int prev, unsigned int curr)
{
	struct enc_seq_t x = enc_sequence[prev];
	return prev == curr || curr == x.next_forward || curr == x.next_backwards;
}

static unsigned int prev_x_enc = 0;
static unsigned int prev_y_enc = 0;
#endif

static void spi_full_empty(void)
{
	u8 spi_input = mcspi_slave_read_rx(spi_slave) & 0xff;

	add_spi('W', spi_input);
	if (cmd == sc_none)
	{
		spi_command_t received_cmd = spi_input & 0x7f;
		if (received_cmd >= sc_none && received_cmd < sc_count)
		{
			switch (received_cmd)
			{
				case sc_encoders:
					spi_retval = pp_encoders;
#ifdef DEBUG_SPI
					if (!check_enc(prev_x_enc, spi_retval & 0x3))
						add_spi('e', 0x88);
					if (!check_enc(prev_y_enc, (spi_retval >> 2) & 0x3))
						add_spi('e', 0x11);

					prev_x_enc = spi_retval & 0x3;
					prev_y_enc = (spi_retval >> 2) & 0x3;
#endif
					break;
				case sc_errors:
					spi_retval = pp_errors;
					break;
				case sc_punch:
					pp_punch(&pp_sim);
					pp_errors |= 1;
					break;
				case sc_pwrx:
				case sc_pwry:
				case sc_irq_en:
					cmd = received_cmd;
					break;
			}
		}
		else
		{
			add_spi('B', spi_input);
			early_printk("BAD COMMAND RECEIVED %d.\n", (int)spi_input);
		}		
	}
	else
	{
		if (cmd == sc_pwrx)
		{
			pp_sim.x_axis.power = spi_input;
		}
		else if (cmd == sc_pwry)
		{
			pp_sim.y_axis.power = spi_input;
		}
		else // cmd == sc_irq_en
		{
			pp_sim.irq_enabled = spi_input ? 1 : 0;
		}

		cmd = sc_none;
	}

	add_spi('r', spi_retval);
	mcspi_slave_write_tx(spi_slave, spi_retval);
}

#define IRQ_EF_MASK	(IRQ_RX0_FULL_MASK | IRQ_TX0_EMPTY_MASK | IRQ_TX0_UNDERFLOW_MASK)
static irqreturn_t spi_irq_handler(int a, void *b)
{
	u32 status = mcspi_slave_irq_status(spi_slave);
	u32 full_empty_status;
	u32 empty, full;
	// check errors first
	if (status & IRQ_RX0_OVERFLOW_MASK)
	{
		add_spi('O', 0);
	}

	if (status & IRQ_TX0_UNDERFLOW_MASK)
	{
		add_spi('U', 0);
	}

	full_empty_status = status & IRQ_EF_MASK;
	full = full_empty_status & IRQ_RX0_FULL_MASK;
	if (full)
	{
		if (!mcspi_slave_tx_empty(spi_slave))
			add_spi('E', 0x18);
		spi_full_empty();
		mcspi_slave_irq_enable(spi_slave, IRQ_EF_MASK);
	}

	mcspi_slave_irq_status_clear(spi_slave, status);
	return IRQ_HANDLED;
}
#else /* PRU_SPI */

static struct device * pru;
#define	PRU_SENSORS_ADDRESS	0x0
#define PRU_ACTUATORS_ADDRESS	0x4
#define PRU_PUNCH_IRQ		0x0	

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

#endif

static int test_kthread(void *unused)
{
	while (true)
	{
		//u32 actuators;
		// we may print simulator state here
		msleep(1000);

		/*pruss_readl(pru, PRU_ACTUATORS_ADDRESS, &actuators);
		printk(KERN_ALERT "acts: %x\n", (unsigned int)actuators);
		printk(KERN_ALERT "irq_en: %d\n", (int)pp_sim.irq_enabled);

		pruss_readl(pru, 0x10, &actuators);
		printk(KERN_ALERT "bad: %x\n", (unsigned int)actuators);*/


		if (kthread_should_stop())
		{
			break;
		}
	}

	return 0;
}

static irqreturn_t reset_line(int irq, void * data)
{
	pp_reinit(&pp_sim);
#ifdef PRU_SPI
	set_spi_sensor_values(&pp_sim, 0);
	reset_spi_actuator_values();
#else
#ifdef DEBUG_SPI
	prev_x_enc = pp_sim.x_axis.encoder;
	prev_y_enc = pp_sim.y_axis.encoder;
#endif
	cmd = sc_none;
	spi_retval = 0;
	pp_encoders = 0;
	pp_errors = 0;
	reset_spi_log();
#endif
	return IRQ_HANDLED;
}

#ifndef PRU_SPI
// FALLING EDGE
static irqreturn_t slave_select_line(int irq, void * data)
{
	mcspi_slave_irq_disable(spi_slave, ~0);
	mcspi_slave_reset_communication(spi_slave);
	while (!(mcspi_slave_tx_empty(spi_slave)))
	{
	}

	mcspi_slave_write_tx(spi_slave, 0);
	mcspi_slave_irq_status_clear(spi_slave, ~0);
	cmd = sc_none;

	if (mcspi_slave_tx_empty(spi_slave))
		early_printk("E\n");
	if (mcspi_slave_rx_full(spi_slave))
		early_printk("F\n");

	mcspi_slave_irq_enable(spi_slave, IRQ_RX0_FULL_MASK | IRQ_RX0_OVERFLOW_MASK | IRQ_TX0_UNDERFLOW_MASK);// | IRQ_TX0_EMPTY_MASK | IRQ_RX0_OVERFLOW_MASK | IRQ_TX0_UNDERFLOW_MASK);
	//early_printk("RI\n");
	return IRQ_HANDLED;
}
#endif


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
#ifdef PRU_SPI
		get_spi_actuator_values(&pp_sim);
#endif

		retval = pp_update(&pp_sim, UPDATE_PERIOD_US);

#ifdef PRU_SPI
		set_spi_sensor_values(&pp_sim, retval);
#else
		pp_encoders = pp_sim.x_axis.encoder | (pp_sim.y_axis.encoder << 2);
		pp_errors = (pp_punch_in_progress(&pp_sim) ? 1 : 0) | (pp_fail(&pp_sim) ? 2 : 0) |
			((pp_sim.x_axis.errors & 0x3) << 2) | ((pp_sim.y_axis.errors & 0x3) << 4);
#endif
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

#define PIN_COUNT	3
struct pin_def_t
{
	unsigned int number;
	unsigned int flags;
	int irq;
	unsigned int irq_type;
	irq_handler_t irq_handler;
	bool allocated;
};

static struct pin_def_t pins[PIN_COUNT] =
{
	{
		.number = RESET_PIN_NUM,
		.flags = GPIOF_IN,
		//.irq_type = IRQF_TRIGGER_RISING,
		//.irq_handler = reset_line,
		.allocated = false,
	},
	{
		.number = ENC_CHANGE_PIN_NUM,
		.flags = GPIOF_OUT_INIT_LOW,
		.irq_handler = 0,
		.allocated = false,
	},
#ifndef PRU_SPI
	{
		.number = SLAVE_SELECT_PIN_NUM,
		.flags = GPIOF_IN,
		.irq_type = IRQF_TRIGGER_FALLING,
		.irq_handler = slave_select_line,
		.allocated = false,
	}
#endif
};

static void free_pins(void)
{
	int i;
	for (i = 0; i < PIN_COUNT; ++i)
	{
		if (pins[i].allocated)
		{
			if (pins[i].irq_handler)
			{
				free_irq(pins[i].irq, NULL);
			}
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

		if (it->irq_handler)
		{
			it->irq = gpio_to_irq(it->number);
			if (it->irq >= 0)
			{
				err = request_irq(it->irq, it->irq_handler, IRQF_NO_THREAD | it->irq_type, module_name, NULL);
			}
			else
			{
				err = it->irq;
			}

			if (err)
			{
				gpio_free(it->number);
				goto fail_gpio_request;
			}
		}

		it->allocated = true;
	}

	return 0;
fail_gpio_request:
	free_pins();
	return err;
}

static int simulator_module_init(void)
{
	int err;
	unsigned int init_val;
	u32 clk_rate, timer_period;

	printk(KERN_ALERT "Simulator module\n");

#ifdef DEBUG_SPI
	spi_buf = (char *)kmalloc(SPI_LOG_BUF_SIZE * sizeof(char), GFP_KERNEL);
	if (spi_buf == 0)
	{
		return -ENOMEM;
	}
#endif

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

#ifndef PRU_SPI
	spi_irq_number = mcspi_slave_get_irq(spi_slave);
	err = request_irq(spi_irq_number, spi_irq_handler, IRQF_NO_THREAD, module_name, NULL);
	if (err)
	{
		goto fail_spi_irq;
	}

	err = irq_set_priority(spi_irq_number, 40);
	if (err)
	{
		printk(KERN_ALERT "UNABLE to set irq priority\n");
		goto fail_pru_request;
	}
#endif

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

	kthread = kthread_run(test_kthread, NULL, module_name);
	if (IS_ERR_OR_NULL(kthread))
	{
		err = PTR_ERR(kthread);
		goto fail_kthread_run;
	}

	reset_line(0, 0);
#ifndef PRU_SPI
	slave_select_line(0, 0);
#endif

	return 0;

fail_kthread_run:
	userinterface_release();
fail_cdev_setup:
	free_irq(timer_irq_number, NULL);
fail_timer_irq:
	omap_dm_timer_free(timer);
fail_timer_request:
	pruss_disable(pru, PRUSS_NUM0);
fail_pru_request:
#ifndef PRU_SPI
	free_irq(spi_irq_number, NULL);
fail_spi_irq:
#endif
	mcspi_slave_free(spi_slave);
fail_spi_get:
	free_pins();
fail_gpio_request:
#ifdef DEBUG_SPI
	kfree(spi_buf);
#endif
	return err;
}

static void simulator_module_exit(void)
{
	printk(KERN_ALERT "Simulator ending\n");

	kthread_stop(kthread);

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
	free_irq(spi_irq_number, NULL);
	mcspi_slave_free(spi_slave);
	free_pins();

#ifdef DEBUG_SPI
	kfree(spi_buf);
#endif
}

module_init(simulator_module_init);
module_exit(simulator_module_exit);

