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

MODULE_LICENSE("Dual BSD/GPL");

struct omap_dm_timer *timer = 0;

static int timer_irq_number = -1;

static volatile int irq_happened = 0;

struct omap_dm_timer *timer2 = 0;

static int timer2_irq_number = -1;

static unsigned int timer2_init_val;

static unsigned int timer2_counter_max = 0;

static const char *module_name = "hpirq_test";

static struct task_struct * kthread;

static int inside_timer1_irq = 0;
static unsigned int irq_in_irq_counter = 0;

static int test_kthread(void *unused)
{
	unsigned int prev_value = 0;
	unsigned int prev_counter = 0;
	while (true)
	{
		if (irq_in_irq_counter != prev_value)
		{
			prev_value = irq_in_irq_counter;
			printk(KERN_ALERT "NUMBER OF timer2 IRQ handlers during timer1 IRQ handler: %u\n", prev_value);
		}

		if (timer2_counter_max != prev_counter)
		{
			unsigned int us;
			prev_counter = timer2_counter_max;
			us = (prev_counter - timer2_init_val) / 24;
			printk(KERN_ALERT "prev_counter: %u, init_val: %u\n", prev_counter, timer2_init_val);
			printk(KERN_ALERT "Maximum delay of timer2 handler: %u ticks, %u us\n", prev_counter - timer2_init_val, us);
		}

		msleep(2000);

		if (kthread_should_stop())
		{
			break;
		}
	}

	return 0;
}

static volatile int work_counter = 0;
static irqreturn_t timer_irq_handler(int a, void *b)
{
	int i;
	inside_timer1_irq = 1;
	mb();
	irq_happened = 1;
	omap_dm_timer_write_status(timer, OMAP_TIMER_INT_OVERFLOW); // clear interrupt flag

	for (i = 0; i < 100000; ++i) // wait here for a moment so that the other timer has chance to interrupt us
	{
		++work_counter;
	}

	mb();
	inside_timer1_irq = 0;
	return IRQ_HANDLED;
}

static irqreturn_t timer2_irq_handler(int irq_num, void *data)
{
	unsigned int timer2_counter = omap_dm_timer_read_counter(timer2);
	if (timer2_counter > timer2_counter_max)
	{
		timer2_counter_max = timer2_counter;
	}

	omap_dm_timer_write_status(timer2, OMAP_TIMER_INT_OVERFLOW); // clear interrupt flag
	if (inside_timer1_irq)
	{
		++irq_in_irq_counter;
	}

	return IRQ_HANDLED;
}

static int testmodule_init(void)
{
	int err;
	unsigned int init_val;

	printk(KERN_ALERT "High priority IRQs tester module\n");

	// acquire timer
	timer = omap_dm_timer_request(); // requesting any timer
	if (!timer) {
		printk(KERN_ALERT "No timer acquired.\n");
		err = -EBUSY;
		goto fail_timer_request;
	}

	// acquire timer interrupt line
	timer_irq_number = omap_dm_timer_get_irq(timer);
	err = request_irq(timer_irq_number, timer_irq_handler, IRQF_NO_THREAD, module_name, NULL);
	if (err) {
		printk(KERN_ALERT "Timer interrupt line not acquired.\n");
		goto fail_timer_irq;
	}

	/*if (irq_set_priority(timer_irq_number, 50) || irq_get_priority(timer_irq_number) != 50)
	{
		printk(KERN_ALERT "UNABLE to set irq priority\n");
		goto fail_timer_irq;
	}*/

	// TIMER2 **********************************
	timer2 = omap_dm_timer_request(); // requesting a timer
	if (!timer2)
	{
		printk(KERN_ALERT "unable to get timer2\n");
		err = -EBUSY;
		goto fail_timer2_request;
	}

	timer2_irq_number = omap_dm_timer_get_irq(timer2);
	err = request_irq(timer2_irq_number, timer2_irq_handler, IRQF_NO_THREAD, module_name, NULL);
	if (err) {
		printk(KERN_ALERT "Timer interrupt line not acquired.\n");
		goto fail_timer2_irq;
	}

	irq_set_priority(timer2_irq_number, 10);
	// ****************************************

	// setup timer
	omap_dm_timer_set_prescaler(timer, -1); // no prescaler
	omap_dm_timer_set_source(timer, OMAP_TIMER_SRC_SYS_CLK); // 24 MHz
	init_val = 0xffffffff - (unsigned int)24000000;
	omap_dm_timer_set_load_start(timer, 1, init_val); // set load value and start

	omap_dm_timer_set_int_enable(timer, OMAP_TIMER_INT_OVERFLOW); // does enale interrupts!

	// setup timer2
	omap_dm_timer_set_prescaler(timer2, -1); // no prescaler
	omap_dm_timer_set_source(timer2, OMAP_TIMER_SRC_SYS_CLK); // 24MHz
	timer2_init_val = 0xffffffff - (unsigned int)74000;
	omap_dm_timer_set_load_start(timer2, 1, timer2_init_val); // set load value and start
	mb();

	omap_dm_timer_set_int_enable(timer2, OMAP_TIMER_INT_OVERFLOW); // does enale interrupts!

	kthread = kthread_run(test_kthread, NULL, module_name);
	if (IS_ERR_OR_NULL(kthread))
	{
		err = PTR_ERR(kthread);
		goto fail_kthread_run;
	}

	return 0;

fail_kthread_run:
	free_irq(timer2_irq_number, NULL);
fail_timer2_irq:
	omap_dm_timer_free(timer2);
fail_timer2_request:
	free_irq(timer_irq_number, NULL);
fail_timer_irq:
	omap_dm_timer_free(timer);
fail_timer_request:
	return err;
}

static void testmodule_exit(void)
{
	printk(KERN_ALERT "Test ending\n");

	kthread_stop(kthread);
	omap_dm_timer_stop(timer2);
	irq_set_priority(timer2_irq_number, LOWEST_IRQ_PRIORITY);
	free_irq(timer2_irq_number, NULL);
	if (omap_dm_timer_free(timer2)) {
		printk(KERN_ALERT "Unable to release timer2.\n");
	}

	omap_dm_timer_stop(timer);
	irq_set_priority(timer_irq_number, LOWEST_IRQ_PRIORITY);
	free_irq(timer_irq_number, NULL);

	if (omap_dm_timer_free(timer)) {
		printk(KERN_ALERT "Unable to release timer.\n");
	}
}

module_init(testmodule_init);
module_exit(testmodule_exit);

