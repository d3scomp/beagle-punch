/*
 * Copyright (C) 2010, 2011 Texas Instruments Incorporated
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as  published by the
 * Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/bitops.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/mfd/core.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/pm_runtime.h>

#include "pruss.h"

struct pruss_priv {
    struct device *dev;
    spinlock_t lock;
    struct resource *res;
    struct clk *clk;
    void __iomem *ioaddr;
};

// Static function declarations
static s32 pruss_lcl_writel(struct pruss_priv *pruss, u32 offset, u32 pdatatowrite);
static s32 pruss_lcl_readl(struct pruss_priv *pruss, u32 offset, u32 *pdatatoread);
static s32 pruss_lcl_rmwl(struct pruss_priv *pruss, u32 offset, u32 mask, u32 val);
static s32 pruss_idx_lcl_writel(struct pruss_priv *pruss, u32 offset, u32 value);

s32 pruss_disable(struct device *dev, u8 pruss_num)
{
    struct pruss_priv *pruss = dev_get_drvdata(dev);
    struct prusscore_regs __iomem *h_pruss;
    struct pruss_map __iomem *pruss_mmap = pruss->ioaddr;
    u32 temp_reg;

//    __FN_IN

        if ((pruss_num != PRUCORE_0) && (pruss_num != PRUCORE_1))
            return -EINVAL;

    spin_lock(&pruss->lock);

    /* pruss deinit */
    iowrite32(0xFFFFFFFF, &pruss_mmap->intc.statclrint[pruss_num]);

    /* Disable PRU */
    h_pruss = &pruss_mmap->core[pruss_num];
    temp_reg = ioread32(&h_pruss->control);
    temp_reg = (temp_reg &
            ~PRUCORE_CONTROL_COUNTENABLE_MASK) |
        ((PRUCORE_CONTROL_COUNTENABLE_DISABLE <<
          PRUCORE_CONTROL_COUNTENABLE_SHIFT) &
         PRUCORE_CONTROL_COUNTENABLE_MASK);
    iowrite32(temp_reg, &h_pruss->control);

    temp_reg = ioread32(&h_pruss->control);
    temp_reg = (temp_reg &
            ~PRUCORE_CONTROL_ENABLE_MASK) |
        ((PRUCORE_CONTROL_ENABLE_DISABLE <<
          PRUCORE_CONTROL_ENABLE_SHIFT) &
         PRUCORE_CONTROL_ENABLE_MASK);
    iowrite32(temp_reg, &h_pruss->control);

    /* Reset PRU */
    iowrite32(PRUCORE_CONTROL_RESETVAL,
            &h_pruss->control);
    spin_unlock(&pruss->lock);
//    __FN_OUT
        return 0;
}
EXPORT_SYMBOL_GPL(pruss_disable);

s32 pruss_enable(struct device *dev, u8 pruss_num)
{
    struct pruss_priv *pruss = dev_get_drvdata(dev);
    struct prusscore_regs __iomem *h_pruss;
    struct pruss_map __iomem *pruss_mmap = pruss->ioaddr;
    u32 i;
//    __FN_IN

    if ((pruss_num != PRUCORE_0) && (pruss_num != PRUCORE_1))
        return -EINVAL;

    h_pruss = &pruss_mmap->core[pruss_num];

    /* Reset PRU  */
    spin_lock(&pruss->lock);
    iowrite32(PRUCORE_CONTROL_RESETVAL, &h_pruss->control);
    spin_unlock(&pruss->lock);

    /* Reset any garbage in the ram */
    if (pruss_num == PRUCORE_0)
        for (i = 0; i < PRUSS_PRU0_RAM_SZ; i++)
            iowrite32(0x0, &pruss_mmap->dram0[i]);
    else if (pruss_num == PRUCORE_1)
        for (i = 0; i < PRUSS_PRU1_RAM_SZ; i++)
            iowrite32(0x0, &pruss_mmap->dram1[i]);
//    __FN_OUT
        return 0;
}
EXPORT_SYMBOL_GPL(pruss_enable);

/* Load the specified PRU with code */
s32 pruss_load(struct device *dev, u8 pruss_num,
        const u32 *pruss_code, u32 code_size_in_words)
{
    struct pruss_priv *pruss = dev_get_drvdata(dev);
    u32 *pruss_iram;
    u32 i;

    if (pruss_num == PRUCORE_0) {
        pruss_iram = (u32 *) ((u32) pruss->ioaddr + PRU0_IRAM);
    } else if (pruss_num == PRUCORE_1) {
        pruss_iram = (u32 *) ((u32) pruss->ioaddr + PRU1_IRAM);
    } else
        return -EINVAL;

//    printk("\npruss_code=%d\n",pruss_code[0]);
    pruss_enable(dev, pruss_num);
    spin_lock(&pruss->lock);
    /* Copy PRU code to its instruction RAM  */
    for (i = 0; i < code_size_in_words; i++) {
        pruss_iram[i]=pruss_code[i];
    }
    spin_unlock(&pruss->lock);
    return 0;
}
EXPORT_SYMBOL_GPL(pruss_load);

s32 pruss_run(struct device *dev, u8 pruss_num)
{
    struct pruss_priv *pruss = dev_get_drvdata(dev);
    struct prusscore_regs __iomem *h_pruss;
    struct pruss_map __iomem *pruss_mmap = pruss->ioaddr;
    u32 temp_reg = 0;
//    __FN_IN

    if ((pruss_num != PRUCORE_0) && (pruss_num != PRUCORE_1))
        return -EINVAL;
//    printk("\npruss_num=%d\n",pruss_num);
    h_pruss = &pruss_mmap->core[pruss_num];

//    printk(KERN_INFO "\n%s value=0x%08x,addr=0x%08x\n", __FUNCTION__, temp_reg,(u32)&h_pruss->control);
    /* Enable PRU, let it execute the code we just copied */
    spin_lock(&pruss->lock);
    temp_reg = ioread32(&h_pruss->control);
    temp_reg = (temp_reg &
            ~PRUCORE_CONTROL_COUNTENABLE_MASK) |
        ((PRUCORE_CONTROL_COUNTENABLE_ENABLE <<
          PRUCORE_CONTROL_COUNTENABLE_SHIFT) &
         PRUCORE_CONTROL_COUNTENABLE_MASK);
    iowrite32(temp_reg, &h_pruss->control);

//    printk(KERN_INFO "\n%s value=0x%08x,addr=0x%08x\n", __FUNCTION__, temp_reg,(u32)&h_pruss->control);

    temp_reg = ioread32(&h_pruss->control);
    temp_reg = (temp_reg &
            ~PRUCORE_CONTROL_ENABLE_MASK) |
        ((PRUCORE_CONTROL_ENABLE_ENABLE <<
          PRUCORE_CONTROL_ENABLE_SHIFT) &
         PRUCORE_CONTROL_ENABLE_MASK);
    iowrite32(temp_reg, &h_pruss->control);
//    printk(KERN_INFO "\n%s value=0x%08x,addr=0x%08x\n", __FUNCTION__, temp_reg,(u32)&h_pruss->control);
    spin_unlock(&pruss->lock);
//    __FN_OUT
        return 0;
}
EXPORT_SYMBOL_GPL(pruss_run);

s32 pruss_wait_for_halt(struct device *dev, u8 pruss_num, u32 timeout)
{
    struct pruss_priv *pruss = dev_get_drvdata(dev);
    struct prusscore_regs __iomem *h_pruss;
    struct pruss_map __iomem *pruss_mmap = pruss->ioaddr;
    u32 temp_reg;
    u32 cnt = timeout;

//    __FN_IN
    if ((pruss_num != PRUCORE_0) && (pruss_num != PRUCORE_1))
        return -EINVAL;
//    printk("\npruss_num=%d\n",pruss_num);
    h_pruss = &pruss_mmap->core[pruss_num];

    while (cnt--) {
        temp_reg = ioread32(&h_pruss->control);
        if (((temp_reg & PRUCORE_CONTROL_RUNSTATE_MASK) >>
                    PRUCORE_CONTROL_RUNSTATE_SHIFT) ==
                PRUCORE_CONTROL_RUNSTATE_HALT)
            break;
    }
    if (!cnt)
        return -EBUSY;
//    __FN_OUT
        return 0;
}
EXPORT_SYMBOL_GPL(pruss_wait_for_halt);

s32 pruss_writeb(struct device *dev, u32 offset, u8 pdatatowrite)
{
    //    __FN_IN
    struct pruss_priv *pruss = dev_get_drvdata(dev);
    void __iomem *paddresstowrite;

    paddresstowrite = pruss->ioaddr + offset;
    iowrite8(pdatatowrite, paddresstowrite);
    //    __FN_OUT
        return 0;
}
EXPORT_SYMBOL_GPL(pruss_writeb);

s32 pruss_writeb_multi(struct device *dev, u32 offset,
		u8 *pdatatowrite, u16 bytestowrite)
{
    struct pruss_priv *pruss = dev_get_drvdata(dev);
    void __iomem *paddresstowrite;
    u16 i;

    paddresstowrite = pruss->ioaddr + offset;

    for (i = 0; i < bytestowrite; i++)
        iowrite8(*pdatatowrite++, paddresstowrite++);

    return 0;
}
EXPORT_SYMBOL_GPL(pruss_writeb_multi);

s32 pruss_rmwb(struct device *dev, u32 offset, u8 mask, u8 val)
{
    struct pruss_priv *pruss = dev_get_drvdata(dev);
    void __iomem *paddress;
    u32 preg_data;

//    __FN_IN
    paddress = pruss->ioaddr + offset;

    spin_lock(&pruss->lock);
    preg_data = ioread8(paddress);
    preg_data &= ~mask;
    preg_data |= val;
    iowrite8(preg_data, paddress);
    spin_unlock(&pruss->lock);
//    __FN_OUT
        return 0;
}
EXPORT_SYMBOL_GPL(pruss_rmwb);

s32 pruss_readb(struct device *dev, u32 offset, u8 *pdatatoread)
{
    //    __FN_IN
    struct pruss_priv *pruss = dev_get_drvdata(dev);
    void __iomem *paddresstoread;

    paddresstoread = pruss->ioaddr + offset ;
    *pdatatoread = ioread8(paddresstoread);
    //    __FN_OUT
    return 0;
}
EXPORT_SYMBOL_GPL(pruss_readb);

u8 pruss_readb_ret(struct device *dev, u32 offset)
{
    struct pruss_priv *pruss = dev_get_drvdata(dev);
    void __iomem *paddresstoread;

    paddresstoread = pruss->ioaddr + offset ;
//    printk (KERN_INFO "%s 0x%04x 0x%02x\n", __FUNCTION__, offset, ioread8(paddresstoread));
    return ioread8(paddresstoread);
}
EXPORT_SYMBOL_GPL(pruss_readb_ret);

s32 pruss_readb_multi(struct device *dev, u32 offset,
        u8 *pdatatoread, u16 bytestoread)
{
    //    __FN_IN
    struct pruss_priv *pruss = dev_get_drvdata(dev);
    u8 __iomem *paddresstoread;
    u16 i;

    paddresstoread = pruss->ioaddr + offset;

    for (i = 0; i < bytestoread; i++)
        *pdatatoread++ = ioread8(paddresstoread++);
    //    __FN_OUT
    return 0;
}
EXPORT_SYMBOL_GPL(pruss_readb_multi);

s32 pruss_writel(struct device *dev, u32 offset,
        u32 pdatatowrite)
{
    //	__FN_IN
    struct pruss_priv *pruss = dev_get_drvdata(dev);
    void __iomem *paddresstowrite;

    paddresstowrite = pruss->ioaddr + offset;
    iowrite32(pdatatowrite, paddresstowrite);
    //	__FN_OUT
    return 0;
}
EXPORT_SYMBOL_GPL(pruss_writel);

s32 pruss_writel_multi(struct device *dev, u32 offset,
        u32 *pdatatowrite, u16 wordstowrite)
{
    //    __FN_IN
    struct pruss_priv *pruss = dev_get_drvdata(dev);
    u32 __iomem *paddresstowrite;
    u16 i;

    paddresstowrite = pruss->ioaddr + offset;

    for (i = 0; i < wordstowrite; i++)
        iowrite32(*pdatatowrite++, paddresstowrite++);
    //    __FN_OUT
    return 0;
}
EXPORT_SYMBOL_GPL(pruss_writel_multi);

s32 pruss_rmwl(struct device *dev, u32 offset, u32 mask, u32 val)
{
    //	__FN_IN
    struct pruss_priv *pruss = dev_get_drvdata(dev);
    void __iomem *paddress;
    u32 preg_data;

    paddress = pruss->ioaddr + offset;

    spin_lock(&pruss->lock);
    preg_data = ioread32(paddress);
    preg_data &= ~mask;
    preg_data |= val;
    iowrite32(preg_data, paddress);
    spin_unlock(&pruss->lock);
    //	__FN_OUT
    return 0;
}
EXPORT_SYMBOL_GPL(pruss_rmwl);

s32 pruss_readl(struct device *dev, u32 offset, u32 *pdatatoread)
{
    //    __FN_IN
    struct pruss_priv *pruss = dev_get_drvdata(dev);
    void __iomem *paddresstoread;

    paddresstoread = pruss->ioaddr + offset;
    *pdatatoread = ioread32(paddresstoread);
    //    __FN_OUT
    return 0;
}
EXPORT_SYMBOL_GPL(pruss_readl);

s32 pruss_readl_multi(struct device *dev, u32 offset,
        u32 *pdatatoread, u16 wordstoread)
{
    //    __FN_IN
    struct pruss_priv *pruss = dev_get_drvdata(dev);
    u32 __iomem *paddresstoread;
    u16 i;

    paddresstoread = pruss->ioaddr + offset;
    for (i = 0; i < wordstoread; i++)
        *pdatatoread++ = ioread32(paddresstoread++);
    //    __FN_OUT
    return 0;
}
EXPORT_SYMBOL_GPL(pruss_readl_multi);

s32 pruss_writew(struct device *dev, u32 offset, u16 pdatatowrite)
{
    //    __FN_IN
    struct pruss_priv *pruss = dev_get_drvdata(dev);
    void __iomem *paddresstowrite;

    paddresstowrite = pruss->ioaddr + offset;
    iowrite16(pdatatowrite, paddresstowrite);
    //    __FN_OUT
    return 0;
}
EXPORT_SYMBOL_GPL(pruss_writew);

s32 pruss_rmww(struct device *dev, u32 offset, u16 mask, u16 val)
{
    //    __FN_IN
    struct pruss_priv *pruss = dev_get_drvdata(dev);
    void __iomem *paddress;
    u32 preg_data;

    paddress = pruss->ioaddr + offset;

    spin_lock(&pruss->lock);
    preg_data = ioread16(paddress);
    preg_data &= ~mask;
    preg_data |= val;
    iowrite16(preg_data, paddress);
    spin_unlock(&pruss->lock);
    //    __FN_OUT
    return 0;
}
EXPORT_SYMBOL_GPL(pruss_rmww);

s32 pruss_readw(struct device *dev, u32 offset, u16 *pdatatoread)
{
    //	__FN_IN
    struct pruss_priv *pruss = dev_get_drvdata(dev);
    void __iomem *paddresstoread;

    paddresstoread = pruss->ioaddr + offset;
    *pdatatoread = ioread16(paddresstoread);
    //	__FN_OUT
    return 0;
}
EXPORT_SYMBOL_GPL(pruss_readw);

s32 pruss_idx_writel(struct device *dev, u32 offset, u32 value)
{
    struct pruss_priv *pruss = dev_get_drvdata(dev);
    void __iomem *paddresstowrite;

    paddresstowrite = pruss->ioaddr + offset;
    iowrite32(value, paddresstowrite);
    return 0;
}
EXPORT_SYMBOL_GPL(pruss_idx_writel);

s32 pruss_enable_system_interrupt(struct device *dev, u32 sysintrnum)
{
    struct pruss_priv *pruss = dev_get_drvdata(dev);

    if(sysintrnum < PRUSS_INTC_SYSINTR_MAX)
        pruss_idx_lcl_writel(pruss, PRUSS_INTC_ENIDXSET, sysintrnum);
    else
        return -1;

    return 0;
}
EXPORT_SYMBOL_GPL(pruss_enable_system_interrupt);

s32 pruss_disable_system_interrupt(struct device *dev, u32 sysintrnum)
{
    struct pruss_priv *pruss = dev_get_drvdata(dev);

    if(sysintrnum < PRUSS_INTC_SYSINTR_MAX)
        pruss_idx_lcl_writel(pruss, PRUSS_INTC_ENIDXCLR, sysintrnum);
    else
        return -1;

    return 0;
}
EXPORT_SYMBOL_GPL(pruss_disable_system_interrupt);

s32 pruss_enable_system_interrupts(struct device *dev, u32 prunum)
{
    struct pruss_priv *pruss = dev_get_drvdata(dev);
//    __FN_IN

    switch(prunum) {
        case PRUSS_NUM0:
            pruss_idx_lcl_writel(pruss, PRUSS_INTC_ENIDXSET, 16);

            pruss_idx_lcl_writel(pruss, PRUSS_INTC_ENIDXSET, 20);
            pruss_idx_lcl_writel(pruss, PRUSS_INTC_ENIDXSET, 21);
            pruss_idx_lcl_writel(pruss, PRUSS_INTC_ENIDXSET, 22);
            pruss_idx_lcl_writel(pruss, PRUSS_INTC_ENIDXSET, 23);
            break;
        case PRUSS_NUM1:
            pruss_idx_lcl_writel(pruss, PRUSS_INTC_ENIDXSET, 17);

            pruss_idx_lcl_writel(pruss, PRUSS_INTC_ENIDXSET, 24);
            pruss_idx_lcl_writel(pruss, PRUSS_INTC_ENIDXSET, 25);
            pruss_idx_lcl_writel(pruss, PRUSS_INTC_ENIDXSET, 26);
            pruss_idx_lcl_writel(pruss, PRUSS_INTC_ENIDXSET, 27);
            break;
        default:
            return -1;
    }

//    __FN_OUT
        return 0;
}
EXPORT_SYMBOL_GPL(pruss_enable_system_interrupts);

s32 pruss_disable_system_interrupts(struct device *dev, u32 prunum)
{
    struct pruss_priv *pruss = dev_get_drvdata(dev);

    switch(prunum) {
        case PRUSS_NUM0:
            pruss_idx_lcl_writel(pruss, PRUSS_INTC_ENIDXCLR, 16);

            pruss_idx_lcl_writel(pruss, PRUSS_INTC_ENIDXCLR, 20);
            pruss_idx_lcl_writel(pruss, PRUSS_INTC_ENIDXCLR, 21);
            pruss_idx_lcl_writel(pruss, PRUSS_INTC_ENIDXCLR, 22);
            pruss_idx_lcl_writel(pruss, PRUSS_INTC_ENIDXCLR, 23);
            break;
        case PRUSS_NUM1:
            pruss_idx_lcl_writel(pruss, PRUSS_INTC_ENIDXCLR, 17);

            pruss_idx_lcl_writel(pruss, PRUSS_INTC_ENIDXCLR, 24);
            pruss_idx_lcl_writel(pruss, PRUSS_INTC_ENIDXCLR, 25);
            pruss_idx_lcl_writel(pruss, PRUSS_INTC_ENIDXCLR, 26);
            pruss_idx_lcl_writel(pruss, PRUSS_INTC_ENIDXCLR, 27);
            break;
        default:
            return -1;
    }

    return 0;
}
EXPORT_SYMBOL_GPL(pruss_disable_system_interrupts);

s32 pruss_system_interrupt_is_set(struct device *dev, u32 sysintrnum)
{
    struct pruss_priv *pruss = dev_get_drvdata(dev);
    u32                statusreg = 0;

    if(sysintrnum < 32) {
        pruss_lcl_readl(pruss, PRUSS_INTC_STATCLRINT0, &statusreg);
        return (statusreg & ((u32)(1 << sysintrnum)));
    }
    else if(sysintrnum < 64) {
        pruss_lcl_readl(pruss, PRUSS_INTC_STATCLRINT1, &statusreg);
        sysintrnum -= 32;
        return (statusreg & ((u32)(1 << sysintrnum)));
    }
    else
        return -1;

    return 0;
}
EXPORT_SYMBOL_GPL(pruss_system_interrupt_is_set);

s32 pruss_system_interrupt_set(struct device *dev, u32 sysintrnum)
{
    struct pruss_priv *pruss = dev_get_drvdata(dev);

    if(sysintrnum < PRUSS_INTC_SYSINTR_MAX)
        pruss_lcl_writel(pruss, PRUSS_INTC_STATIDXSET, sysintrnum);
    else
        return -1;

    return 0;
}
EXPORT_SYMBOL_GPL(pruss_system_interrupt_set);

s32 pruss_system_interrupt_clr(struct device *dev, u32 sysintrnum)
{
    struct pruss_priv *pruss = dev_get_drvdata(dev);

    if(sysintrnum < PRUSS_INTC_SYSINTR_MAX)
        pruss_lcl_writel(pruss, PRUSS_INTC_STATIDXCLR, sysintrnum);
    else
        return -1;

    return 0;
}
EXPORT_SYMBOL_GPL(pruss_system_interrupt_clr);

s32 pruss_arm_to_pru_intr_set(struct device *dev, u32 prunum)
{
    struct pruss_priv *pruss = dev_get_drvdata(dev);

    switch(prunum) {
        case PRUSS_NUM0:
            pruss_lcl_writel(pruss, PRUSS_INTC_STATIDXSET, 16);
            break;
        case PRUSS_NUM1:
            pruss_lcl_writel(pruss, PRUSS_INTC_STATIDXSET, 17);
            break;
        default:
            return -1;
    }

    return 0;
}
EXPORT_SYMBOL_GPL(pruss_arm_to_pru_intr_set);

s32 pruss_arm_to_pru_intr_clr(struct device *dev, u32 prunum)
{
    struct pruss_priv *pruss = dev_get_drvdata(dev);

    switch(prunum) {
        case PRUSS_NUM0:
            pruss_lcl_writel(pruss, PRUSS_INTC_STATIDXCLR, 16);
            break;
        case PRUSS_NUM1:
            pruss_lcl_writel(pruss, PRUSS_INTC_STATIDXCLR, 17);
            break;
        default:
            return -1;
    }

    return 0;
}
EXPORT_SYMBOL_GPL(pruss_arm_to_pru_intr_clr);

/*static int pruss_mfd_add_devices(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct mfd_cell *cell = pdev->dev.platform_data;
    s32 err, i, num_devices = 0;

	printk(KERN_ALERT "a\n");
//    __FN_IN
    for (i = 0; cell[i].name; i++) {
	printk(KERN_ALERT "b\n");
        err = mfd_add_devices(dev, 0, &cell[i], 1, NULL, 0, NULL);
	printk(KERN_ALERT "c\n");
        if (err) {
	printk(KERN_ALERT "C\n");
            dev_err(dev, "cannot add mfd cell: %s\n",
                    cell[i].name);
            continue;
        }
        num_devices++;
        dev_info(dev, "mfd: added %s device\n", cell[i].name);
    }
//    __FN_OUT
        return num_devices;
}*/

static s32 pruss_lcl_writel(struct pruss_priv *pruss, u32 offset, u32 pdatatowrite)
{
    void __iomem *paddresstowrite;

    paddresstowrite = pruss->ioaddr + offset;
    iowrite32(pdatatowrite, paddresstowrite);
    return 0;
}

static s32 pruss_lcl_readl(struct pruss_priv *pruss, u32 offset, u32 *pdatatoread)
{
    void __iomem *paddresstoread;

    paddresstoread = pruss->ioaddr + offset;
    *pdatatoread = ioread32(paddresstoread);
    return 0;
}

static s32 pruss_lcl_rmwl(struct pruss_priv *pruss, u32 offset, u32 mask, u32 val)
{
    void __iomem *paddress;
    u32 preg_data;

    paddress = pruss->ioaddr + offset;

    spin_lock(&pruss->lock);
    preg_data = ioread32(paddress);
    preg_data &= ~mask;
    preg_data |= val;
    iowrite32(preg_data, paddress);
    spin_unlock(&pruss->lock);
    return 0;
}

static s32 pruss_idx_lcl_writel(struct pruss_priv *pruss, u32 offset, u32 value)
{
    void __iomem *paddresstowrite;

    paddresstowrite = pruss->ioaddr + offset;
    iowrite32(value, paddresstowrite);
    return 0;
}

static void arm_to_pru_intr_init(struct pruss_priv *pruss)
{
    u32 value;
    u32 int_offset;

//    __FN_IN
    /* Clear all the host interrupts */
    for (int_offset = 0; int_offset <= PRUSS_INTC_HOSTINTLVL_MAX;
            int_offset++)
        pruss_idx_lcl_writel(pruss, PRUSS_INTC_HSTINTENIDXCLR, int_offset);

    /* Enable the global s32errupt */
    pruss_lcl_rmwl(pruss, (u32) PRUSS_INTC_GLBLEN, 0, 1);

 
	/* Enable the Host interrupts for all host channels */
    for (int_offset = 0; int_offset <= PRUSS_INTC_HOSTINTLVL_MAX;
            int_offset++)
        pruss_lcl_rmwl(pruss, (u32) PRUSS_INTC_HSTINTENIDXSET,
                0, int_offset);

    /* Set the Host insterrupt to Channel number mapping */
    pruss_lcl_rmwl(pruss, (u32) PRUSS_INTC_HOSTMAP0,
            PRU_INTC_REGMAP_MASK, PRU_INTC_HOSTMAP0_CHAN);
    pruss_lcl_rmwl(pruss, (u32) PRUSS_INTC_HOSTMAP1,
            PRU_INTC_REGMAP_MASK, PRU_INTC_HOSTMAP1_CHAN);
    pruss_lcl_rmwl(pruss, (u32) PRUSS_INTC_HOSTMAP2,
            PRU_INTC_REGMAP_MASK, PRU_INTC_HOSTMAP2_CHAN);

    /* Set the Channel to System event mapping */
    pruss_lcl_rmwl(pruss, (u32) PRUSS_INTC_CHANMAP1,
            PRU_INTC_REGMAP_MASK, PRU_INTC_CHANMAP1_FULL);
    pruss_lcl_rmwl(pruss, (u32) PRUSS_INTC_CHANMAP4,
            PRU_INTC_REGMAP_MASK, PRU_INTC_CHANMAP4_FULL);
    pruss_lcl_rmwl(pruss, (u32) PRUSS_INTC_CHANMAP5,
            PRU_INTC_REGMAP_MASK, PRU_INTC_CHANMAP5_FULL);
    pruss_lcl_rmwl(pruss, (u32) PRUSS_INTC_CHANMAP6,
            PRU_INTC_REGMAP_MASK, PRU_INTC_CHANMAP6_FULL);
    pruss_lcl_rmwl(pruss, (u32) PRUSS_INTC_CHANMAP7,
            PRU_INTC_REGMAP_MASK, PRU_INTC_CHANMAP7_FULL);
    pruss_lcl_rmwl(pruss, (u32) PRUSS_INTC_CHANMAP8,
            PRU_INTC_REGMAP_MASK, PRU_INTC_CHANMAP8_FULL);
    pruss_lcl_rmwl(pruss, (u32) PRUSS_INTC_CHANMAP13,
            PRU_INTC_REGMAP_MASK, PRU_INTC_CHANMAP13_FULL);

    /* Clear required set of system events
     * and enable them using indexed register
     */
    for (int_offset = 0; int_offset < 16; int_offset++) {
        value = 18 + int_offset;
        pruss_idx_lcl_writel(pruss, PRUSS_INTC_STATIDXCLR, value);
    }

    pruss_idx_lcl_writel(pruss, PRUSS_INTC_STATIDXCLR, 54);

    pruss_lcl_rmwl(pruss, (u32) PRUSS_INTC_GLBLEN, 0, 1);

    /* Enable the Host interrupts for all host channels */
    for (int_offset = 0; int_offset <= PRUSS_INTC_HOSTINTLVL_MAX;
            int_offset++)
        pruss_idx_lcl_writel(pruss, PRUSS_INTC_HSTINTENIDXSET, int_offset);

//    __FN_OUT
}

static struct device * pruss_instance = 0;

struct device * pruss_get(void)
{
	return pruss_instance;
}
EXPORT_SYMBOL_GPL(pruss_get);

static int pruss_probe(struct platform_device *pdev)
{
    struct pruss_priv *pruss = NULL;
    int err;

//    __FN_IN
    pruss = kzalloc(sizeof(struct pruss_priv), GFP_KERNEL);
    if (!pruss)
        return -ENOMEM;

    pruss->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!pruss->res) {
        dev_err(&pdev->dev,
                "unable to get pruss memory resources!\n");
        err = -ENODEV;
        goto probe_exit_kfree;
    }

	pruss->ioaddr = devm_request_and_ioremap(&pdev->dev, pruss->res);
	if (IS_ERR(pruss->ioaddr)) {
		dev_err(&pdev->dev, "ioremap failed\n");
		err = -ENOMEM;
		goto probe_exit_kfree;
	}

	printk(KERN_ALERT "ioremap succeeded: phys: %p, virt: %p", (void *)pruss->res->start, pruss->ioaddr);

    spin_lock_init(&pruss->lock);

#ifdef CONFIG_ARCH_DAVINCI_DA850
    pruss->clk = clk_get(&pdev->dev, "pruss");
    if (IS_ERR(pruss->clk)) {
        dev_err(&pdev->dev, "no clock available: pruss\n");
        err = -ENODEV;
        pruss->clk = NULL;
        goto probe_exit_iounmap;
    }

    clk_enable(pruss->clk);
#endif

    pm_runtime_enable(&pdev->dev);
    err = pm_runtime_get_sync(&pdev->dev);
    if (IS_ERR_VALUE(err))
    {
    	dev_err(&pdev->dev, "unable to get sync\n");
    	goto probe_exit_clock;
    }

	pruss_instance = &pdev->dev;
    //err = pruss_mfd_add_devices(pdev);
	//printk(KERN_ALERT "A\n");
    //if (!err)
        //goto probe_exit_clock;

    platform_set_drvdata(pdev, pruss);
    pruss->dev = &pdev->dev;

    arm_to_pru_intr_init(pruss);

//    __FN_OUT
        return 0;

probe_exit_clock:
	pm_runtime_disable(&pdev->dev);
#ifdef CONFIG_ARCH_DAVINCI_DA850
    clk_put(pruss->clk);
    clk_disable(pruss->clk);
probe_exit_iounmap:
#endif
probe_exit_kfree:
    kfree(pruss);
    return err;
}

static int pruss_remove(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct pruss_priv *pruss = dev_get_drvdata(dev);

	pruss_instance = 0;
//    __FN_IN
    ////mfd_remove_devices(dev);
    pruss_disable(dev, PRUCORE_0);
    pruss_disable(dev, PRUCORE_1);
    pm_runtime_put_sync(dev);
    pm_runtime_disable(dev);
#ifdef CONFIG_ARCH_DAVINCI_DA850
    clk_disable(pruss->clk);
    clk_put(pruss->clk);
#endif
    kfree(pruss);
    dev_set_drvdata(dev, NULL);
//    __FN_OUT
        return 0;
}

static const struct of_device_id pruss_dt_ids[] = {
	{ .compatible = "ti,pruss-v2-pruss", .data = NULL, },
	{},
};
MODULE_DEVICE_TABLE(of, pruss_dt_ids);

static struct platform_driver pruss_driver = {
    .probe	= pruss_probe,
    .remove	= pruss_remove,
    .driver	= {
        .name	= "pruss_mfd",
        .owner	= THIS_MODULE,
	.of_match_table = pruss_dt_ids
    }
};

static int __init pruss_init(void)
{
    return platform_driver_register(&pruss_driver);
}
module_init(pruss_init);

static void __exit pruss_exit(void)
{
    platform_driver_unregister(&pruss_driver);
}
module_exit(pruss_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Programmable Realtime Unit (PRU) Driver");
MODULE_AUTHOR("Subhasish Ghosh");
MODULE_AUTHOR("http://www.smartembeddedsystems.com");
