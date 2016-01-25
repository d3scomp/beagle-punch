/*
 * Copyright (C) 2010, 2011 Texas Instruments Incorporated
 * Author: Jitendra Kumar <jitendra@mistralsolutions.com>
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

#ifndef _PRUSS_H_
#define _PRUSS_H_

#include <linux/types.h>
#include <linux/platform_device.h>
#include "pruss_core.h"

#define __FN_IN 	printk("\nIn %s\n",__FUNCTION__);
#define __FN_OUT 	printk("\nOut %s\n",__FUNCTION__);
#define __LINE 		printk("\nline=%d,in %s\n",__LINE__,__FUNCTION__);

#define PRUSS_NUM0			PRUCORE_0
#define PRUSS_NUM1			PRUCORE_1

#define PRUSS_PRU0_RAM_SZ		8192
#define PRUSS_PRU1_RAM_SZ		8192
#define PRUSS_PRU0_BASE_ADDRESS		0
#define PRUSS_PRU1_BASE_ADDRESS		0x2000
#define PRUSS_INTC_BASE_ADDRESS		(PRUSS_PRU0_BASE_ADDRESS + 0x20000)
#define PRUSS_INTC_GLBLEN		(PRUSS_INTC_BASE_ADDRESS + 0x10)
#define PRUSS_INTC_GLBLNSTLVL		(PRUSS_INTC_BASE_ADDRESS + 0x1C)
#define PRUSS_INTC_STATIDXSET		(PRUSS_INTC_BASE_ADDRESS + 0x20)
#define PRUSS_INTC_STATIDXCLR		(PRUSS_INTC_BASE_ADDRESS + 0x24)
#define PRUSS_INTC_ENIDXSET		(PRUSS_INTC_BASE_ADDRESS + 0x28)
#define PRUSS_INTC_ENIDXCLR		(PRUSS_INTC_BASE_ADDRESS + 0x2C)
#define PRUSS_INTC_HSTINTENIDXSET	(PRUSS_INTC_BASE_ADDRESS + 0x34)
#define PRUSS_INTC_HSTINTENIDXCLR	(PRUSS_INTC_BASE_ADDRESS + 0x38)
#define PRUSS_INTC_GLBLPRIIDX		(PRUSS_INTC_BASE_ADDRESS + 0x80)
#define PRUSS_INTC_STATSETINT0		(PRUSS_INTC_BASE_ADDRESS + 0x200)
#define PRUSS_INTC_STATSETINT1		(PRUSS_INTC_BASE_ADDRESS + 0x204)
#define PRUSS_INTC_STATCLRINT0		(PRUSS_INTC_BASE_ADDRESS + 0x280)
#define PRUSS_INTC_STATCLRINT1		(PRUSS_INTC_BASE_ADDRESS + 0x284)
#define PRUSS_INTC_ENABLESET0		(PRUSS_INTC_BASE_ADDRESS + 0x300)
#define PRUSS_INTC_ENABLESET1		(PRUSS_INTC_BASE_ADDRESS + 0x304)
#define PRUSS_INTC_ENABLECLR0		(PRUSS_INTC_BASE_ADDRESS + 0x380)
#define PRUSS_INTC_ENABLECLR1		(PRUSS_INTC_BASE_ADDRESS + 0x384)
#define PRUSS_INTC_CHANMAP0		(PRUSS_INTC_BASE_ADDRESS + 0x400)
#define PRUSS_INTC_CHANMAP1		(PRUSS_INTC_BASE_ADDRESS + 0x404)
#define PRUSS_INTC_CHANMAP2		(PRUSS_INTC_BASE_ADDRESS + 0x408)
#define PRUSS_INTC_CHANMAP3		(PRUSS_INTC_BASE_ADDRESS + 0x40C)
#define PRUSS_INTC_CHANMAP4		(PRUSS_INTC_BASE_ADDRESS + 0x410)
#define PRUSS_INTC_CHANMAP5		(PRUSS_INTC_BASE_ADDRESS + 0x414)
#define PRUSS_INTC_CHANMAP6		(PRUSS_INTC_BASE_ADDRESS + 0x418)
#define PRUSS_INTC_CHANMAP7		(PRUSS_INTC_BASE_ADDRESS + 0x41C)
#define PRUSS_INTC_CHANMAP8		(PRUSS_INTC_BASE_ADDRESS + 0x420)
#define PRUSS_INTC_CHANMAP9		(PRUSS_INTC_BASE_ADDRESS + 0x424)
#define PRUSS_INTC_CHANMAP10		(PRUSS_INTC_BASE_ADDRESS + 0x428)
#define PRUSS_INTC_CHANMAP11		(PRUSS_INTC_BASE_ADDRESS + 0x42C)
#define PRUSS_INTC_CHANMAP12		(PRUSS_INTC_BASE_ADDRESS + 0x430)
#define PRUSS_INTC_CHANMAP13		(PRUSS_INTC_BASE_ADDRESS + 0x434)
#define PRUSS_INTC_CHANMAP14		(PRUSS_INTC_BASE_ADDRESS + 0x438)
#define PRUSS_INTC_CHANMAP15		(PRUSS_INTC_BASE_ADDRESS + 0x43C)
#define PRUSS_INTC_HOSTMAP0		(PRUSS_INTC_BASE_ADDRESS + 0x800)
#define PRUSS_INTC_HOSTMAP1		(PRUSS_INTC_BASE_ADDRESS + 0x804)
#define PRUSS_INTC_HOSTMAP2		(PRUSS_INTC_BASE_ADDRESS + 0x808)
#define PRUSS_INTC_POLARITY0		(PRUSS_INTC_BASE_ADDRESS + 0xD00)
#define PRUSS_INTC_POLARITY1		(PRUSS_INTC_BASE_ADDRESS + 0xD04)
#define PRUSS_INTC_TYPE0		(PRUSS_INTC_BASE_ADDRESS + 0xD80)
#define PRUSS_INTC_TYPE1		(PRUSS_INTC_BASE_ADDRESS + 0xD84)
#define PRUSS_INTC_HOSTINTEN		(PRUSS_INTC_BASE_ADDRESS + 0x1500)

/* PRU0 DATA RAM base address */
#define PRU0_DATARAM_OFFSET                 (0x0000u)
/* PRU1 DATA RAM base address */
#define PRU1_DATARAM_OFFSET                 (0x2000u)

/* PRU0 DATA RAM size */
#define PRU0_DATARAM_SIZE                   (0x2000u)
/* PRU1 DATA RAM size */
#define PRU1_DATARAM_SIZE                   (0x2000u)


#define PRUSS_INTC_HOSTINTLVL_MAX	9
#define PRUSS_INTC_SYSINTR_MAX	    64

#define PRU_INTC_HOSTMAP0_CHAN		(0x03020100)
#define PRU_INTC_HOSTMAP1_CHAN		(0x07060504)
#define PRU_INTC_HOSTMAP2_CHAN		(0x00000908)

/*#define PRU_INTC_CHANMAP7_SYS_EVT31	(0x00000000)
#define PRU_INTC_CHANMAP8_FULL		(0x02020100)
#define PRU_INTC_CHANMAP9_FULL		(0x04040303)
#define PRU_INTC_CHANMAP10_FULL		(0x06060505)
#define PRU_INTC_CHANMAP11_FULL		(0x08080707)
#define PRU_INTC_CHANMAP12_FULL		(0x00010909)*/
#define PRU_INTC_CHANMAP8_HALF		(0x03020100)
#define PRU_INTC_CHANMAP9_HALF		(0x07060504)
#define PRU_INTC_CHANMAP10_HALF		(0x03020908)
#define PRU_INTC_CHANMAP11_HALF		(0x07060504)
#define PRU_INTC_CHANMAP12_HALF		(0x00010908)

#define PRU_INTC_CHANMAP3_SYS_EVT15 (0x00000000)
#define PRU_INTC_CHANMAP1_FULL		(0x00000000) // 07           - Timer Interrupt to PRU0
#define PRU_INTC_CHANMAP4_FULL		(0x00000100) // 19 18 17 16
#define PRU_INTC_CHANMAP5_FULL      (0x03030202) // 23 22 21 20
//#define PRU_INTC_CHANMAP5_FULL      (0x05040302) // 23 22 21 20  -- For I2C
#define PRU_INTC_CHANMAP6_FULL      (0x07070606) // 27 26 25 24
#define PRU_INTC_CHANMAP7_FULL      (0x09090808) // 31 30 29 28

#define PRU_INTC_CHANMAP8_FULL      (0x00000000) // 35 34 33 32 - McASP1 to PRU0
//#define PRU_INTC_CHANMAP8_FULL      (0x00010100) // 35 34 33 32 - McASP1 to PRU1

#define PRU_INTC_CHANMAP13_FULL     (0x01010000) // 55 54 53 52 - McASP0 to PRU1
//#define PRU_INTC_CHANMAP13_FULL     (0x00000000) // 55 54 53 52 - McASP0 to PRU0

#define PRU_INTC_REGMAP_MASK		(0xFFFFFFFF)

struct mfd_pruss_suart_data {
	u32 version;
	int (*setup)(void);
};

s32 pruss_enable(struct device *dev, u8 pruss_num);

s32 pruss_load(struct device *dev, u8 pruss_num,
	const u32 *pruss_code, u32 code_size_in_words);

s32 pruss_run(struct device *dev, u8 pruss_num);

s32 pruss_wait_for_halt(struct device *dev, u8 pruss_num, u32 timeout);

s32 pruss_disable(struct device *dev, u8 pruss_num);

s32 pruss_writeb(struct device *dev, u32 offset, u8 pdatatowrite);

s32 pruss_writeb_multi(struct device *dev, u32 offset,
		u8 *pdatatowrite, u16 bytestowrite);

s32 pruss_rmwb(struct device *dev, u32 offset, u8 mask, u8 val);

s32 pruss_readb(struct device *dev, u32 offset, u8 *pdatatoread);

u8 pruss_readb_ret(struct device *dev, u32 offset);

s32 pruss_readb_multi(struct device *dev, u32 offset,
		u8 *pdatatoread, u16 bytestoread);

s32 pruss_readl(struct device *dev, u32 offset, u32 *pdatatoread);

s32 pruss_readl_multi(struct device *dev, u32 offset,
		u32 *pdatatoread, u16 wordstoread);

s32 pruss_writel(struct device *dev, u32 offset, u32 pdatatowrite);

s32 pruss_writel_multi(struct device *dev, u32 offset,
		u32 *pdatatowrite, u16 wordstowrite);

s32 pruss_rmwl(struct device *dev, u32 offset, u32 mask, u32 val);

s32 pruss_idx_writel(struct device *dev, u32 offset, u32 value);

s32 pruss_writew(struct device *dev, u32 offset, u16 datatowrite);

s32 pruss_rmww(struct device *dev, u32 offset, u16 mask, u16 val);

s32 pruss_readw(struct device *dev, u32 offset, u16 *pdatatoread);

s32 pruss_enable_system_interrupt(struct device *dev, u32 sysintrnum);

s32 pruss_disable_system_interrupt(struct device *dev, u32 sysintrnum);

s32 pruss_enable_system_interrupts(struct device *dev, u32 prunum);

s32 pruss_disable_system_interrupts(struct device *dev, u32 prunum);

s32 pruss_arm_to_pru_intr_set(struct device *dev, u32 prunum);

s32 pruss_arm_to_pru_intr_clr(struct device *dev, u32 prunum);

s32 pruss_system_interrupt_set(struct device *dev, u32 sysintrnum);

s32 pruss_system_interrupt_clr(struct device *dev, u32 sysintrnum);

s32 pruss_system_interrupt_is_set(struct device *dev, u32 sysintrnum);

struct device * pruss_get(void);
#endif	/* End _PRUSS_H_ */
