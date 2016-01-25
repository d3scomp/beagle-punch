
#include "spi_slave.h"
#include <linux/init.h>
#include <linux/module.h>

MODULE_LICENSE("GPL");

static struct mcspi_slave_device *slave;

static int __init slave_test_init(void)
{
	int err;
	printk(KERN_ALERT "SPI slave tester init.\n");
	
	slave = mcspi_slave_get_one();
	if (slave != NULL)
	{
		printk(KERN_ALERT "SPI slave acquired.\n");
		err = mcspi_slave_test_start(slave);
		if (err)
		{
			mcspi_slave_free(slave);
			return err;
		}

		return 0;
	}

	return -ENODEV;
}

static void __exit slave_test_exit(void)
{
	printk(KERN_ALERT "SPI slave tester exit.\n");
	if (slave != NULL)
	{
		printk(KERN_ALERT "Returning SPI slave\n");
		mcspi_slave_test_stop(slave);
		mcspi_slave_free(slave);
	}
}

module_init(slave_test_init);
module_exit(slave_test_exit);

