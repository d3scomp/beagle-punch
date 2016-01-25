
#include "pruss.h"
#include "pru_test_bin.h"

#include <linux/init.h>
#include <linux/module.h>

MODULE_LICENSE("GPL");

static struct device * pru;

static int __init pru_test_init(void)
{
	int err;
	printk(KERN_ALERT "PRU tester init.\n");
	
	pru = pruss_get();
	if (pru != NULL)
	{
		printk(KERN_ALERT "PRU device acquired.\n");
		err = pruss_enable(pru, PRUSS_NUM0);
		if (err)
			printk(KERN_ALERT "pruss_enable failed: %d\n", err);
		err = pruss_load(pru, PRUSS_NUM0, (u32*)PRUcode, ARRAY_SIZE(PRUcode));
		if (err)
			printk(KERN_ALERT "pruss_enable failed: %d\n", err);
		err = pruss_run(pru, PRUSS_NUM0);
		if (err)
			printk(KERN_ALERT "pruss_enable failed: %d\n", err);

		return 0;
	}

	return -ENODEV;
}

static void __exit pru_test_exit(void)
{
	printk(KERN_ALERT "PRU tester exit.\n");
	if (pru != NULL)
	{
		pruss_disable(pru, PRUSS_NUM0);
	}
}

module_init(pru_test_init);
module_exit(pru_test_exit);

