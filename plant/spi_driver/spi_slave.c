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
	int irq_num;
	void __iomem *regs_base;
	unsigned long regs_phys_base;
	unsigned int pin_dir; // setup of pin direction
	unsigned int num_chipselect; // number of chipselects - is it important for slave mode?
	struct clk * clk;
	struct list_head node; // thanks to this field, instance can be inserted into a linked list

	struct task_struct *test_thread; // thread used in testing mode
	atomic_t test_irq_status;
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

/******************************** REGISTER ACCESS METHODS **************************************/

// REVISION register - not necessary

// SYSCONFIG register
#define SYSCONFIG_OFFSET	0x110
#define SYSCONFIG_SOFTRESET_MASK	0x2 // set this bit to 1 to reset MCSPI module by sw, it is then automatically set to 0

// SYSSTATUS register
#define SYSSTATUS_OFFSET	0x114
#define SYSSTATUS_RESETDONE_MASK	0x1 // says whether reset of the module is in progress

// IRQSTATUS register
#define IRQSTATUS_OFFSET	0x118

// IRQENABLE register
#define IRQENABLE_OFFSET	0x11C

// SYST register
#define	SYST_OFFSET	0x124
#define SYST_SSB_MASK	0x800 // Set Status Bit - this bit must be cleared prior attempting to clear any status bit in IRQSTATUS
// writing 1 sets to 1 all status bits contained in the IRQSTATUS register

#define SYST_SPIENDIR_MASK	0x400 // sets the direction of the SPIEN[3:0] lines and the SPICLK line - 0 = output/master mode, 1 = input/slave mode
#define SYST_SPIDATDIR1_MASK	0x200 // sets the direction of the SPIDAT[1], 0 = output, 1 = input
#define SYST_SPIDATDIR0_MASK	0x100 // sets the direction of the SPIDAT[0], 0 = output, 1 = input
#define SYST_SPICLK_MASK	0x40 // if SYST[SPIENDIR] == 1, read from this bit reads the CLKSPI value (high/low), if SYST[SPIENDIR] == 0, write to this bit drives the CLKSPI value
#define SYST_SPIDAT_1_MASK	0x20 // similar to SYST[SPICLK] only this time for SPIDAT_1 and driven by SYST[SPIDIR1]
#define SYST_SPIDAT_0_MASK	0x10 // similar to SYST[SPICLK] only this time for SPIDAT_0 and driven by SYST[SPIDIR0]
#define SYST_SPIEN_3_MASK	0x08 // same as SYST[SPICLK] only this time for SPIEN3
#define SYST_SPIEN_2_MASK	0x04 // same as SYST[SPICLK] only this time for SPIEN2
#define SYST_SPIEN_1_MASK	0x02 // same as SYST[SPICLK] only this time for SPIEN1
#define SYST_SPIEN_0_MASK	0x01 // same as SYST[SPICLK] only this time for SPIEN0

// MODULCTRL register
#define MODULCTRL_OFFSET	0x128
#define MODULCTRL_FDAA_MASK	0x100
#define MODULCTRL_MOA_MASK	0x80
#define	MODULCTRL_INITDLY_MASK	0x70
#define MODULCTRL_SYSTEM_TEST_MASK	0x8 // enables the system test mode, 0 = functional mode, 1 = system test mode
#define MODULCTRL_MS_MASK	0x4 // master/slave
// 0 = master, the module generates the SPICLK and SPIEN[3:0]
// 1 = slave, the module receives the SPICLK and SPIEN[3:0]
#define MODULCTRL_PIN34_MASK	0x2 // pin mode selection, if asserted the contoller only uses SIMO, SOMI and SPICLK clock pin for SPI transfers
// 0 = SPIEN used as a chip select
// 1 = SPIEN is not used, in this mode all options related to chipselect have no meaning
#define MODULCTRL_SINGLE_MASK	0x1 // single/multi channel, master mode only

// CH0CONF register
#define CH0CONF_OFFSET	0x12C

#define CHCONF_CLKG_MASK	0x20000000 // clock divider granularity
#define CHCONF_FFER_MASK	0x10000000 // FIFO enabled for receive (only one channel can have it set), 1 = enabled
#define CHCONF_FFEW_MASK	0x08000000 // FIFO enabled for transmit
#define CHCONF_TCS_MASK		0x06000000 // chip select time control
#define CHCONF_SBPOL_MASK	0x01000000 // start bit polarity, 0 = start bit polarity is held to zero during SPI transfer, 1 = one
#define CHCONF_SBE_MASK		0x00800000 // start bit enable for SPI transfer, 0 = SPI transfer length as specified by WL bit field, 1 = start bit D/CX added, polarity defined by SBPOL
#define CHCONF_SPIENSLV_MASK	0x00600000 // (channel 0 and slave mode only) SPI slave select signal detection - which SPIEN is used to detect slave select
#define CHCONF_SPIENSLV_0	0x00000000 // detection enabled on SPIEN[0]
#define CHCONF_SPIENSLV_1	0x00200000 // detection enabled on SPIEN[1]
#define CHCONF_SPIENSLV_2	0x00400000 // detection enabled on SPIEN[2]
#define CHCONF_SPIENSLV_3	0x00600000 // detection enabled on SPIEN[3]

#define CHCONF_FORCE_MASK	0x00100000 // (single channel master mode only) manual SPIEN assertion to keep SPIEN active between words
#define CHCONF_TURBO_MASK	0x00080000 // 0 = turbo deactivated (recommended for single word transfer), 1 = turbo on
#define CHCONF_IS_MASK		0x00040000 // input select, 0 = data line 0 (SPIDAT[0]) selected for reception, 1 = data line 1 (SPIDAT[1]) selected
#define CHCONF_DPE1_MASK	0x00020000 // transmission enabled for data line 1(SPIDATAGZEN[1])
// 0 = data line 1 (SPIDAT[1]) selected for transmission
// 1 = no transmision on data line 1
#define CHCONF_DPE0_MASK	0x00010000 // same as DPE0 but for data line 0 (SPIDAT[0])
#define CHCONF_DMAR_MASK	0x00008000 // DMA read request, 0 = disabled, 1 = enabled
#define CHCONF_DMAW_MASK	0x00004000 // DMA write request, 0 = disabled, 1 = enabled
#define CHCONF_TRM_MASK		0x00003000 // transmit/receive modes
#define CHCONF_TRM_TRANSMIT_AND_RECEIVE	0x00000000 // 0 = transmit and receive mode
#define CHCONF_TRM_RECEIVE_ONLY		0x00001000 // 1 = receive-only mode
#define CHCONF_TRM_TRANSMIT_ONLY	0x00002000 // 2 = transmit-only mode
// 3 = reserved

#define CHCONF_WL_MASK		0x00000F80 // SPI word length, 0, 1, 2 values are reserved, otherwise the word has length of WL + 1; 3 => 4 bit word, 1F = 32 bit word
#define CHCONF_WL(length) (((length - 1) & 0x1F) << 7) // converts a length of an SPI word to a value which can be ORed into the CONF register (if it has zero there)

#define CHCONF_EPOL_MASK	0x00000040 // SPIEN polarity, 0 = SPIEN is held high during the active state, 1 = low
#define CHCONF_CLKD_MASK	0x0000003C // frequency divider for SPICLK, only for master mode
#define CHCONF_POL_MASK		0x00000002 // SPICLK polarity, 0 = SPICLK is held high during active state, 1 = low
#define CHCONF_PHA_MASK		0x00000001 // SPICLK phase, 0 = data are latched on odd numbered edges of SPICLK, 1 = even numbered edges

// CH0STAT register
#define CH0STAT_OFFSET	0x130

#define CHSTAT_RXFFF_MASK 	0x40
#define CHSTAT_RXFFE_MASK 	0x20
#define CHSTAT_TXFFF_MASK 	0x10
#define CHSTAT_TXFFE_MASK 	0x08
#define CHSTAT_EOT_MASK 	0x04
#define CHSTAT_TXS_MASK 	0x02 // channel x transmitter register status, 0 = register full, 1 = empty, cleared when most significant byte of MCSPI_TXx is written, set after channel enable and when an SPI work is transferred from MCSPI_TXx register to the shift register
#define CHSTAT_RXS_MASK 	0x01 // channel x receiver register status, 0 = empty, 1 = full, cleared when enabling channel x and also when the msB of the MCSPI_RXx is read, set when the received SPI word is transferred from the shift register to the MCSPI_RXx

// CH0CTRL register
#define CH0CTRL_OFFSET	0x134

#define CHCTRL_EXTCLK_MASK	0xFF00 // clock ration extension
#define CHCTRL_EN_MASK		0x0001 // channel enable, 0 = not active, 1 = active

// tx and rx registers
#define TX0_OFFSET	0x138
#define RX0_OFFSET	0x13C

static u32 read_register(struct mcspi_slave_device * slave, unsigned int register_offset)
{
	// printk(KERN_ALERT "rr: slave: %p, regs_base: %p, offset: %x, base+offset: %p\n", slave, slave->regs_base, register_offset, slave->regs_base + register_offset);
	return ioread32(slave->regs_base + register_offset);
}

static void write_register(struct mcspi_slave_device * slave, unsigned int register_offset, u32 value)
{
	// printk(KERN_ALERT "rw: slave: %p, regs_base: %p, offset: %x, base+offset: %p, value: %x\n", slave, slave->regs_base, register_offset, slave->regs_base + register_offset, value);
	iowrite32(value, slave->regs_base + register_offset);
}

static u32 read_register_masked(struct mcspi_slave_device * slave, unsigned int register_offset, u32 mask)
{
	return read_register(slave, register_offset) & mask;
}

static void enable_register_bits(struct mcspi_slave_device * slave, unsigned int register_offset, u32 mask)
{
	u32 val = read_register(slave, register_offset);
	write_register(slave, register_offset, val | mask);
}

static void disable_register_bits(struct mcspi_slave_device * slave, unsigned int register_offset, u32 mask)
{
	u32 val = read_register(slave, register_offset);
	write_register(slave, register_offset, val & (~mask));
}

/*static void reset_module(struct mcspi_slave_device * slave)
{
	u32 val = read_register(slave, SYSCONFIG_OFFSET);
	write_register(slave, SYSCONFIG_OFFSET, val | SYSCONFIG_SOFTRESET_MASK);
}

static bool reset_in_progress(struct mcspi_slave_device * slave)
{
	return (read_register(slave, SYSSTATUS_OFFSET) & SYSSTATUS_RESETDONE_MASK) ? true : false;
}*/

void mcspi_slave_irq_enable(struct mcspi_slave_device *slave, u32 irq_mask)
{
	//u32 val = read_register(slave, IRQENABLE_OFFSET);
	//write_register(slave, IRQENABLE_OFFSET, val | irq_mask);
	enable_register_bits(slave, IRQENABLE_OFFSET, irq_mask);
}
EXPORT_SYMBOL_GPL(mcspi_slave_irq_enable);

void mcspi_slave_irq_disable(struct mcspi_slave_device *slave, u32 irq_mask)
{
	//u32 val = read_register(slave, IRQENABLE_OFFSET);
	//write_register(slave, IRQENABLE_OFFSET, val & (~irq_mask));
	disable_register_bits(slave, IRQENABLE_OFFSET, irq_mask);
}
EXPORT_SYMBOL_GPL(mcspi_slave_irq_disable);

u32 mcspi_slave_irq_status(struct mcspi_slave_device * slave)
{
	return read_register(slave, IRQSTATUS_OFFSET);
}
EXPORT_SYMBOL_GPL(mcspi_slave_irq_status);

void mcspi_slave_irq_status_clear(struct mcspi_slave_device * slave, u32 irq_mask)
{
	write_register(slave, IRQSTATUS_OFFSET, irq_mask);
}
EXPORT_SYMBOL_GPL(mcspi_slave_irq_status_clear);

bool mcspi_slave_tx_empty(struct mcspi_slave_device * slave)
{
	return read_register_masked(slave, CH0STAT_OFFSET, CHSTAT_TXS_MASK) ? true : false;
}
EXPORT_SYMBOL_GPL(mcspi_slave_tx_empty);

bool mcspi_slave_rx_full(struct mcspi_slave_device * slave)
{
	return read_register_masked(slave, CH0STAT_OFFSET, CHSTAT_RXS_MASK) ? true : false;
}
EXPORT_SYMBOL_GPL(mcspi_slave_rx_full);

u32 mcspi_slave_read_rx(struct mcspi_slave_device * slave)
{
	return read_register(slave, RX0_OFFSET);
}
EXPORT_SYMBOL_GPL(mcspi_slave_read_rx);

void mcspi_slave_write_tx(struct mcspi_slave_device * slave, u32 value)
{
	//value &= IRQ_TX0_EMPTY_MASK | IRQ_TX0_UNDERFLOW_MASK | IRQ_RX0_FULL_MASK | IRQ_RX0_OVERFLOW_MASK;
	write_register(slave, TX0_OFFSET, value);
}
EXPORT_SYMBOL_GPL(mcspi_slave_write_tx);

void mcspi_slave_reset_communication(struct mcspi_slave_device *slave)
{
	u32 value = read_register(slave, CH0CONF_OFFSET);
	write_register(slave, CH0CTRL_OFFSET, 0);
	write_register(slave, CH0CONF_OFFSET, value); // rewrite the length of word, that should reset the counter of bits in the transmitter/receiver
	write_register(slave, CH0CTRL_OFFSET, CHCTRL_EN_MASK);
}
EXPORT_SYMBOL_GPL(mcspi_slave_reset_communication);

#define PRINT_REG(slave, offset) \
	printk(KERN_ALERT #offset ": 0x%x\n", read_register(slave, offset))
static void print_register_state(struct mcspi_slave_device * slave)
{
	printk(KERN_ALERT "Dump of %s spi registers:\n", dev_name(&slave->pdev->dev));
	PRINT_REG(slave, SYSCONFIG_OFFSET);
	PRINT_REG(slave, SYSSTATUS_OFFSET);
	PRINT_REG(slave, IRQSTATUS_OFFSET);
	PRINT_REG(slave, IRQENABLE_OFFSET);
	PRINT_REG(slave, SYST_OFFSET);
	PRINT_REG(slave, MODULCTRL_OFFSET);
	PRINT_REG(slave, CH0CONF_OFFSET);
	PRINT_REG(slave, CH0STAT_OFFSET);
	PRINT_REG(slave, CH0CTRL_OFFSET);
}

/***********************************************************************************************/

static u32 next_value = 0; 

static const char * test_name = "spi test module";

static irqreturn_t test_irq_handler(int irq, void *data)
{
	struct mcspi_slave_device *slave = (struct mcspi_slave_device *)data;
	u32 status = mcspi_slave_irq_status(slave);
	atomic_set(&slave->test_irq_status, atomic_read(&slave->test_irq_status) | status);

	if (status & IRQ_TX0_EMPTY_MASK)
	{
		slave->last_sended_value = next_value;
		mcspi_slave_write_tx(slave, next_value);
		++next_value;
	}
	if (status & IRQ_TX0_UNDERFLOW_MASK)
	{
	}
	if (status & IRQ_RX0_FULL_MASK)
	{
		slave->last_received_value = mcspi_slave_read_rx(slave);
	}
	if (status & IRQ_RX0_OVERFLOW_MASK)
	{
	}

	mcspi_slave_irq_status_clear(slave, ~0);
	return IRQ_HANDLED;
}

static void print_irq_status(struct mcspi_slave_device * slave)
{
	int status = atomic_read(&slave->test_irq_status);
	if (status != 0)
	{
		atomic_set(&slave->test_irq_status, 0);
		printk(KERN_ALERT "IRQ status: 0x%x\n", status);
		if (status & IRQ_TX0_EMPTY_MASK)
		{
			printk(KERN_ALERT "last value sended: 0x%x\n", slave->last_sended_value);
		}
		if (status & IRQ_TX0_UNDERFLOW_MASK)
		{
			printk(KERN_ALERT "Underflow happended\n");
		}
		if (status & IRQ_RX0_FULL_MASK)
		{
			printk(KERN_ALERT "last value received: 0x%x\n", slave->last_received_value);
		}
		if (status & IRQ_RX0_OVERFLOW_MASK)
		{
			printk(KERN_ALERT "Overflow happened\n");
		}
	}
}

static int test_thread(void *data)
{
	struct mcspi_slave_device *slave = (struct mcspi_slave_device *)data;
	printk(KERN_ALERT "SPI test_thread started.");
	while (true)
	{
		msleep_interruptible(500);
		if (kthread_should_stop())
		{
			break;
		}

		print_irq_status(slave);
	}

	printk(KERN_ALERT "SPI test_thread ending.");

	return 0;
}

#define CLKCTRL_OFFSET 0x4c
#define CM_PER_OFFSET	0x44e00000
#define CLKCTRL_IDLEST_MASK	0x30000

int mcspi_slave_test_start(struct mcspi_slave_device * slave)
{
	int irq_num;
	int err;
	u32 reg_val;

	printk(KERN_ALERT "Slave test start invoked.\n");
	if (!slave)
	{
		return -ENODEV;
	}
	
	irq_num = mcspi_slave_get_irq(slave);
	err = request_irq(irq_num, test_irq_handler, IRQF_NO_THREAD, test_name, slave);
	if (err)
	{
		printk(KERN_ALERT "SPI: Interrupt line not acquired.\n");
		return err;
	}

	printk(KERN_ALERT "interrupt line acquired: %i\n", irq_num);

	err = clk_prepare(slave->clk);
	if (err)
	{
		printk(KERN_ALERT "SPI: unable to prepare spi clock.\n");
		return err;
	}

	err = clk_enable(slave->clk);
	if (err)
	{
		printk(KERN_ALERT "SPI: unable to enable spi clock.\n");
		return err;
	}

	printk(KERN_ALERT "clock rate: %lu\n", clk_get_rate(slave->clk));

	pm_runtime_get_sync(&slave->pdev->dev);
	mb();
	
	write_register(slave, SYSCONFIG_OFFSET, 0x0A);
	mb();
	while (!read_register(slave, SYSSTATUS_OFFSET) & SYSSTATUS_RESETDONE_MASK)
	{
		mb();
	}

	print_register_state(slave);

	mb();
	reg_val = MODULCTRL_MS_MASK; // slave mode
		// SPIEN is used as chipselect - no MODULCTRL_PIN34_MASK
	write_register(slave, MODULCTRL_OFFSET, reg_val);
	mb();

	write_register(slave, CH0CTRL_OFFSET, 0); // disable channel 0 before loading configuration
	mcspi_slave_irq_disable(slave, ~0); // disable all SPI irqs

	mb();
	

	atomic_set(&slave->test_irq_status, 0);
	slave->test_thread = kthread_run(test_thread, slave, "SPI test thread");
	if (IS_ERR_OR_NULL(slave->test_thread))
	{
		free_irq(irq_num, NULL);
		return -ENODEV;
	}

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
		| CHCONF_WL(8) // word length
		| CHCONF_EPOL_MASK // SPIEN is held low during active state
		| CHCONF_POL_MASK // SPICLK is held low during active state
		| CHCONF_PHA_MASK; // data are latchen on even numbered edges of SPICLK

	write_register(slave, CH0CONF_OFFSET, reg_val);

	mb();
	mcspi_slave_irq_status_clear(slave, ~0); // clear all irq statuses
	mcspi_slave_irq_enable(slave, IRQ_TX0_EMPTY_MASK | IRQ_TX0_UNDERFLOW_MASK | IRQ_RX0_FULL_MASK | IRQ_RX0_OVERFLOW_MASK); // enable channel 0 irqs

	mb();
	write_register(slave, CH0CTRL_OFFSET, CHCTRL_EN_MASK); // enable channel 0 and thus slave mode on selected SPI

	return 0;
}
EXPORT_SYMBOL_GPL(mcspi_slave_test_start);

void mcspi_slave_test_stop(struct mcspi_slave_device * slave)
{
	mcspi_slave_irq_disable(slave, ~0); // disable all SPI IRQs
	disable_register_bits(slave, CH0CTRL_OFFSET, CHCTRL_EN_MASK); // disable channel 0
	free_irq(mcspi_slave_get_irq(slave), slave);
	pm_runtime_put_sync(&slave->pdev->dev);
	clk_disable(slave->clk);
	kthread_stop(slave->test_thread);
	slave->test_thread = NULL;
}
EXPORT_SYMBOL_GPL(mcspi_slave_test_stop);

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
	mcspi_slave_irq_disable(slave, ~0); // disable all SPI irqs

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
	mcspi_slave_irq_status_clear(slave, ~0); // clear all irq statuses
	mb();
	write_register(slave, CH0CTRL_OFFSET, CHCTRL_EN_MASK); // enable channel 0 and thus slave mode on selected SPI
	slave->enabled = true;

	return 0;
}
EXPORT_SYMBOL_GPL(mcspi_slave_enable);

void mcspi_slave_disable(struct mcspi_slave_device * slave)
{
	mcspi_slave_irq_disable(slave, ~0); // disable all SPI IRQs
	disable_register_bits(slave, CH0CTRL_OFFSET, CHCTRL_EN_MASK); // disable channel 0
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

struct mcspi_slave_device * mcspi_slave_get_by_name(const char * name)
{
	struct mcspi_slave_device * spi_slave = NULL;

	down(&mcspi_slave_list_lock);

	list_for_each_entry(spi_slave, &mcspi_slave_list, node) {
		if (!spi_slave->used && !strcmp(dev_name(&spi_slave->pdev->dev), name)) {
			spi_slave->used = 1;
			break;
		}
	}

	up(&mcspi_slave_list_lock);

	return spi_slave;
}
EXPORT_SYMBOL_GPL(mcspi_slave_get_by_name);

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
		printk(KERN_ALERT "SPI Slave get_one. num_chipselect %i, pin_dir: %i, irq_num: %i, regs_phys_base: 0x%lx, regs_base: %p\n",
			spi_slave->num_chipselect, spi_slave->pin_dir, spi_slave->irq_num, spi_slave->regs_phys_base, spi_slave->regs_base);
	}

	return spi_slave;
}
EXPORT_SYMBOL_GPL(mcspi_slave_get_one);

int mcspi_slave_get_irq(const struct mcspi_slave_device * slave)
{
	if (!slave) {
		return -ENODEV;
	}

	return slave->irq_num;
}
EXPORT_SYMBOL_GPL(mcspi_slave_get_irq);

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

	spi_slave->test_thread = NULL;
	atomic_set(&spi_slave->test_irq_status, 0);

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

	r = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!r) {
		err = -ENODEV;
		printk(KERN_ALERT "IORESOURCE_IRQ failed\n");
		goto fail_get_platform_irq;
	}

	spi_slave->irq_num = r->start;

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

	printk(KERN_ALERT "SPI Slave probe. num_chipselect %i, pin_dir: %i, irq_num: %i, regs_offset: %i, regs_phys_base: 0x%lx, regs_base: %p\n",
		spi_slave->num_chipselect, spi_slave->pin_dir, spi_slave->irq_num, regs_offset, spi_slave->regs_phys_base, spi_slave->regs_base);

	// add spi device to the list
	down(&mcspi_slave_list_lock);
	printk(KERN_ALERT "SPI slave device added to list\n");
	list_add_tail(&spi_slave->node, &mcspi_slave_list);
	up(&mcspi_slave_list_lock);

	return 0;

fail_clk_get:
fail_get_platform_irq:
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

	printk(KERN_ALERT "SPI SLave Remove called.\n");

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

static int __init testmodule_init(void)
{
	int err;
	printk(KERN_ALERT "SPI Slave Module Init.\n");

	err = platform_driver_register(&mcspi_slave_driver);
	if (err)
		return err;

	return 0;
}

static void __exit testmodule_exit(void)
{
	printk(KERN_ALERT "SPI Slave Module Exit.\n");
	platform_driver_unregister(&mcspi_slave_driver);
}

module_init(testmodule_init);
module_exit(testmodule_exit);

