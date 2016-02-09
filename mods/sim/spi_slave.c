#include "spi_slave.h"

#include <linux/init.h>
#include <linux/module.h>

#include <asm/current.h>
#include <linux/sched.h>

#include <linux/gpio.h>
#include <plat/dmtimer.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <asm/io.h>


#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/semaphore.h>
#include <linux/list.h>
#include <linux/err.h>
#include <linux/kthread.h>
#include <asm/atomic.h>
#include <linux/clk.h>
#include <asm/system.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_runtime.h>

MODULE_LICENSE("GPL");

struct mcspi_slave_platform_config {
	unsigned int regs_offset;
	unsigned int num_cs;
	unsigned int pin_dir; // unsigned int pin_dir:1;
};

struct mcspi_slave_device {
	struct platform_device * pdev;
	const struct mcspi_slave_platform_config * pconfig;
	bool used;
	bool enabled;
	void __iomem *regs_base;
	unsigned long regs_phys_base;
	unsigned int pin_dir; // setup of pin direction
	unsigned int num_chipselect; // number of chipselects - is it important for slave mode?
	struct clk * clk;
	struct list_head node; // thanks to this field, instance can be inserted into a linked list

	u32 last_received_value;
	u32 last_sended_value;
};

static LIST_HEAD(mcspi_slave_list);
static DEFINE_SEMAPHORE(mcspi_slave_list_lock);

static struct mcspi_slave_platform_config omap2_pdata = {
	.regs_offset = -256//0,
};

#define OMAP4_MCSPI_REG_OFFSET 0x100
static struct mcspi_slave_platform_config omap4_pdata = {
	.regs_offset = 0 //OMAP4_MCSPI_REG_OFFSET,
};

static const struct of_device_id omap_mcspi_of_match[] = {
	{
		.compatible = "ti,omap2-mcspi-slave",
		.data = &omap2_pdata,
	},
	{
		.compatible = "ti,omap4-mcspi-slave",
		.data = &omap4_pdata,
	},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, omap_mcspi_of_match);


#define SYSCONFIG_OFFSET	0x110

#define SYSSTATUS_OFFSET	0x114
#define SYSSTATUS_RESETDONE_MASK	0x1 // says whether reset of the module is in progress

#define	SYST_OFFSET	0x124

#define SYST_SPIENDIR_MASK	0x400 // sets the direction of the SPIEN[3:0] lines and the SPICLK line - 0 = output/master mode, 1 = input/slave mode
#define SYST_SPIDATDIR0_MASK	0x100 // sets the direction of the SPIDAT[0], 0 = output, 1 = input
#define SYST_SPICLK_MASK	0x40 // if SYST[SPIENDIR] == 1, read from this bit reads the CLKSPI value (high/low), if SYST[SPIENDIR] == 0, write to this bit drives the CLKSPI value
#define SYST_SPIDAT_1_MASK	0x20 // similar to SYST[SPICLK] only this time for SPIDAT_1 and driven by SYST[SPIDIR1]
#define SYST_SPIDAT_0_MASK	0x10 // similar to SYST[SPICLK] only this time for SPIDAT_0 and driven by SYST[SPIDIR0]

#define MODULCTRL_OFFSET	0x128
#define	MODULCTRL_INITDLY_MASK	0x70
#define MODULCTRL_MS_MASK	0x4 // master/slave
#define MODULCTRL_PIN34_MASK	0x2 // pin mode selection, if asserted the contoller only uses SIMO, SOMI and SPICLK clock pin for SPI transfers

#define CH0CONF_OFFSET	0x12C

#define CHCONF_SBPOL_MASK	0x01000000 // start bit polarity, 0 = start bit polarity is held to zero during SPI transfer, 1 = one
#define CHCONF_SBE_MASK		0x00800000 // start bit enable for SPI transfer, 0 = SPI transfer length as specified by WL bit field, 1 = start bit D/CX added, polarity defined by SBPOL
#define CHCONF_SPIENSLV_0	0x00000000 // detection enabled on SPIEN[0]

#define CHCONF_IS_MASK		0x00040000 // input select, 0 = data line 0 (SPIDAT[0]) selected for reception, 1 = data line 1 (SPIDAT[1]) selected
#define CHCONF_DPE1_MASK	0x00020000 // transmission enabled for data line 1(SPIDATAGZEN[1])
#define CHCONF_DPE0_MASK	0x00010000 // same as DPE0 but for data line 0 (SPIDAT[0])
#define CHCONF_TRM_TRANSMIT_AND_RECEIVE	0x00000000 // 0 = transmit and receive mode

#define CHCONF_WL(length) (((length - 1) & 0x1F) << 7) // converts a length of an SPI word to a value which can be ORed into the CONF register (if it has zero there)

#define CHCONF_EPOL_MASK	0x00000040 // SPIEN polarity, 0 = SPIEN is held high during the active state, 1 = low
#define CHCONF_POL_MASK		0x00000002 // SPICLK polarity, 0 = SPICLK is held high during active state, 1 = low
#define CHCONF_PHA_MASK		0x00000001 // SPICLK phase, 0 = data are latched on odd numbered edges of SPICLK, 1 = even numbered edges

// CH0CTRL register
#define CH0CTRL_OFFSET	0x134

#define CHCTRL_EN_MASK		0x0001 // channel enable, 0 = not active, 1 = active

static u32 read_register(struct mcspi_slave_device * slave, unsigned int register_offset)
{
	return ioread32(slave->regs_base + register_offset);
}

static void write_register(struct mcspi_slave_device * slave, unsigned int register_offset, u32 value)
{
	iowrite32(value, slave->regs_base + register_offset);
}

/***********************************************************************************************/

int mcspi_slave_enable(struct mcspi_slave_device * slave, u8 word_length)
{
	int err;
	u32 reg_val;

	if (!slave)
	{
		return -ENODEV;
	}
	
	err = clk_prepare(slave->clk);
	if (err)
	{
		printk(KERN_ALERT "SPI: unable to prepare spi clock.\n");
		return err;
	}

	err = clk_enable(slave->clk);
	if (err)
	{
		clk_unprepare(slave->clk);
		printk(KERN_ALERT "SPI: unable to enable spi clock.\n");
		return err;
	}

	pm_runtime_get_sync(&slave->pdev->dev);
	mb();
	
	write_register(slave, SYSCONFIG_OFFSET, 0x0A);
	mb();
	while (!read_register(slave, SYSSTATUS_OFFSET) & SYSSTATUS_RESETDONE_MASK)
	{
		mb();
	}

	mb();
	reg_val = MODULCTRL_MS_MASK; // slave mode
		// SPIEN is used as chipselect - no MODULCTRL_PIN34_MASK
	write_register(slave, MODULCTRL_OFFSET, reg_val);
	mb();
	write_register(slave, CH0CTRL_OFFSET, 0); // disable channel 0 before loading configuration

	mb();

	reg_val = SYST_SPIENDIR_MASK // spien in input mode (slave)
		| SYST_SPIDATDIR0_MASK // SPIDAT[0] is in input mode
		// SPIDAT[1] is in output mode - no SYST_SPIDATDIR0_MASK
		| SYST_SPIDAT_0_MASK
		| SYST_SPIDAT_1_MASK
		| SYST_SPICLK_MASK; // high value on the SPIDAT[0]
	write_register(slave, SYST_OFFSET, reg_val);
	reg_val = // start bit polarity 0 // CHCONF_SBPOL_MASK - start bit polarity is 1
		// no start bit // CHCONF_SBE_MASK - start bit enabled
		CHCONF_SPIENSLV_0 // SPIEN[0] chosen for enabling
		// no turbo mode
		// | CHCONF_IS_MASK // data line 1 (SPIDAT[1]) selected for reception
		// | CHCONF_DPE1_MASK // disables transmission on data line 1
		| CHCONF_DPE0_MASK // disables transmission on data line 0
		| CHCONF_TRM_TRANSMIT_AND_RECEIVE // both transmission and reception enabled
		| CHCONF_WL(word_length) // word length
		| CHCONF_EPOL_MASK // SPIEN is held low during active state
		| CHCONF_POL_MASK // SPICLK is held low during active state
		| CHCONF_PHA_MASK; // data are latchen on even numbered edges of SPICLK

	write_register(slave, CH0CONF_OFFSET, reg_val);

	mb();
	write_register(slave, CH0CTRL_OFFSET, CHCTRL_EN_MASK); // enable channel 0 and thus slave mode on selected SPI
	slave->enabled = true;

	return 0;
}
EXPORT_SYMBOL_GPL(mcspi_slave_enable);

void mcspi_slave_disable(struct mcspi_slave_device * slave)
{
	u32 val = read_register(slave, CH0CTRL_OFFSET);
	write_register(slave, CH0CTRL_OFFSET, val & (~CHCTRL_EN_MASK)); // disable channel 0
	pm_runtime_put_sync(&slave->pdev->dev);
	clk_disable(slave->clk);
	slave->enabled = false;
}
EXPORT_SYMBOL_GPL(mcspi_slave_disable);

int mcspi_slave_free(struct mcspi_slave_device * slave)
{
	if (!slave) {
		return -ENODEV;
	}

	if (slave->enabled)
	{
		mcspi_slave_disable(slave);
	}

	slave->used = false;
	return 0;
}
EXPORT_SYMBOL_GPL(mcspi_slave_free);

struct mcspi_slave_device * mcspi_slave_get_one(void)
{
	struct mcspi_slave_device * spi_slave = NULL;

	down(&mcspi_slave_list_lock);

	list_for_each_entry(spi_slave, &mcspi_slave_list, node) {
		if (!spi_slave->used) {
			spi_slave->used = 1;
			break;
		}
	}

	up(&mcspi_slave_list_lock);

	if (spi_slave != NULL)
	{
		printk(KERN_ALERT "SPI Slave get_one. num_chipselect %i, pin_dir: %i, regs_phys_base: 0x%lx, regs_base: %p\n",
			spi_slave->num_chipselect, spi_slave->pin_dir, spi_slave->regs_phys_base, spi_slave->regs_base);
	}

	return spi_slave;
}
EXPORT_SYMBOL_GPL(mcspi_slave_get_one);

static int spi_slave_probe(struct platform_device *pdev)
{
	int err = 0;
	struct device *dev = &pdev->dev;
	struct mcspi_slave_device * spi_slave;
	const struct of_device_id * match;
	const struct mcspi_slave_platform_config *pconfig_data;
	struct device_node * node = pdev->dev.of_node;
	unsigned int regs_offset; // offset of registers from the base address
	struct resource * r;
	struct pinctrl * pinctrl;

	printk(KERN_ALERT "SPI Slave Probe called on: %s.\n", pdev->name);	

	spi_slave = devm_kzalloc(dev, sizeof(struct mcspi_slave_device), GFP_KERNEL); // kzalloc for device drivers?
	if (!spi_slave) {
		err = -ENOMEM;
		printk(KERN_ALERT "alloc failed\n");
		goto fail_device_data_alloc;
	}

	spi_slave->pdev = pdev;
	spi_slave->used = false;
	spi_slave->enabled = false;

	match = of_match_device(omap_mcspi_of_match, &pdev->dev);
	if (match) {
		u32 num_cs = 1; // default number of chipselect
		pconfig_data = match->data;

		of_property_read_u32(node, "ti,spi-num-cs", &num_cs);
		spi_slave->num_chipselect = num_cs;
		if (of_get_property(node, "ti,pindir-d0-out-d1-in", NULL))
			spi_slave->pin_dir = 1;
	} else {
		pconfig_data = dev_get_platdata(&pdev->dev);
		spi_slave->num_chipselect = pconfig_data->num_cs;
		spi_slave->pin_dir = pconfig_data->pin_dir;
	}

	spi_slave->pconfig = pconfig_data;
	regs_offset = pconfig_data->regs_offset;

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r) {
		err = -ENODEV;
		printk(KERN_ALERT "IORESOURCE_MEM failed\n");
		goto fail_get_platform_mem;
	}

	r->start += regs_offset;
	r->end += regs_offset;

	spi_slave->regs_phys_base = r->start;
	// spi_slave->regs_base = devm_ioremap_resource(dev, r); 3.18 kernel
	spi_slave->regs_base = devm_request_and_ioremap(dev, r); // 3.8 kernel
	if (IS_ERR(spi_slave->regs_base)) {
		err = PTR_ERR(spi_slave->regs_base);
		printk(KERN_ALERT "ioremap failed %p, err: %i\n", spi_slave->regs_base, err);
		goto fail_ioremap;
	}

	pinctrl = devm_pinctrl_get_select_default(&pdev->dev);
	if (IS_ERR(pinctrl))
		printk(KERN_ALERT "pins are not configured from the driver\n");

	spi_slave->clk = devm_clk_get(dev, "fck");
	if (IS_ERR(spi_slave->clk))
	{
		err = PTR_ERR(spi_slave->clk);
		printk(KERN_ALERT "clk_get failed.\n");
		goto fail_clk_get;
	}

	// power up the spi module
	pm_runtime_enable(dev);

	printk(KERN_ALERT "SPI Slave probe. num_chipselect %i, pin_dir: %i, regs_offset: %i, regs_phys_base: 0x%lx, regs_base: %p\n",
		spi_slave->num_chipselect, spi_slave->pin_dir, regs_offset, spi_slave->regs_phys_base, spi_slave->regs_base);

	// add spi device to the list
	down(&mcspi_slave_list_lock);
	printk(KERN_ALERT "SPI slave device added to list\n");
	list_add_tail(&spi_slave->node, &mcspi_slave_list);
	up(&mcspi_slave_list_lock);

	return 0;

fail_clk_get:
	devm_iounmap(dev, spi_slave->regs_base);
fail_ioremap:
fail_get_platform_mem:
	devm_kfree(dev, spi_slave);
fail_device_data_alloc:
	printk(KERN_ALERT "SPI Slave probe FAILED, err: %i.\n", err);
	return err;
}


static int spi_slave_remove(struct platform_device *pdev)
{
	struct mcspi_slave_device * spi_slave;
	int ret = -EINVAL;

	printk(KERN_ALERT "SPI Slave Remove called.\n");

	down(&mcspi_slave_list_lock);

	list_for_each_entry(spi_slave, &mcspi_slave_list, node) {
		if (!strcmp(dev_name(&spi_slave->pdev->dev),
			dev_name(&pdev->dev))) {
		// if (spi_slave->pdev == pdev) { // would this be incorrect?
			list_del(&spi_slave->node);
			pm_runtime_disable(&spi_slave->pdev->dev);
			// iounmap(spi_slave->regs_base);
			ret = 0;
			break;
		}
	}

	up(&mcspi_slave_list_lock);

	return ret;
}

static struct platform_driver mcspi_slave_driver = {
	.probe = spi_slave_probe,
	.remove = spi_slave_remove,
	.driver = {
		.name = "omap2_mcspi-slave",
		.owner = THIS_MODULE,
		.of_match_table = omap_mcspi_of_match,
	},
};

static int mcspi_slave_setup(void)
{
	int err;

	err = platform_driver_register(&mcspi_slave_driver);
	if (err)
		return err;

	return 0;
}

static void mcspi_slave_release(void)
{
	platform_driver_unregister(&mcspi_slave_driver);
}

module_init(mcspi_slave_setup);
module_exit(mcspi_slave_release);
