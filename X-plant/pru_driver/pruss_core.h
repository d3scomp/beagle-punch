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

#ifndef _PRUSS_CORE_H_
#define _PRUSS_CORE_H_

#include <linux/types.h>

#define PRUCORE_0		(0)
#define PRUCORE_1		(1)

#define PRUCORE_CONTROL_PCRESETVAL_MASK			(0xFFFF0000u)
#define PRUCORE_CONTROL_PCRESETVAL_SHIFT		(0x00000010u)
#define PRUCORE_CONTROL_PCRESETVAL_RESETVAL		(0x00000000u)
#define PRUCORE_CONTROL_RUNSTATE_MASK			(0x00008000u)
#define PRUCORE_CONTROL_RUNSTATE_SHIFT			(0x0000000Fu)
#define PRUCORE_CONTROL_RUNSTATE_RESETVAL		(0x00000000u)
#define PRUCORE_CONTROL_RUNSTATE_HALT			(0x00000000u)
#define PRUCORE_CONTROL_RUNSTATE_RUN			(0x00000001u)
#define PRUCORE_CONTROL_SINGLESTEP_MASK			(0x00000100u)
#define PRUCORE_CONTROL_SINGLESTEP_SHIFT		(0x00000008u)
#define PRUCORE_CONTROL_SINGLESTEP_RESETVAL		(0x00000000u)
#define PRUCORE_CONTROL_SINGLESTEP_FREERUN		(0x00000000u)
#define PRUCORE_CONTROL_SINGLESTEP_SINGLE		(0x00000001u)
#define PRUCORE_CONTROL_COUNTENABLE_MASK		(0x00000008u)
#define PRUCORE_CONTROL_COUNTENABLE_SHIFT		(0x00000003u)
#define PRUCORE_CONTROL_COUNTENABLE_RESETVAL		(0x00000000u)
#define PRUCORE_CONTROL_COUNTENABLE_DISABLE		(0x00000000u)
#define PRUCORE_CONTROL_COUNTENABLE_ENABLE		(0x00000001u)
#define PRUCORE_CONTROL_SLEEPING_MASK			(0x00000004u)
#define PRUCORE_CONTROL_SLEEPING_SHIFT			(0x00000002u)
#define PRUCORE_CONTROL_SLEEPING_RESETVAL		(0x00000000u)
#define PRUCORE_CONTROL_SLEEPING_NOTASLEEP		(0x00000000u)
#define PRUCORE_CONTROL_SLEEPING_ASLEEP			(0x00000001u)
#define PRUCORE_CONTROL_ENABLE_MASK			(0x00000002u)
#define PRUCORE_CONTROL_ENABLE_SHIFT			(0x00000001u)
#define PRUCORE_CONTROL_ENABLE_RESETVAL			(0x00000000u)
#define PRUCORE_CONTROL_ENABLE_DISABLE			(0x00000000u)
#define PRUCORE_CONTROL_ENABLE_ENABLE			(0x00000001u)
#define PRUCORE_CONTROL_SOFTRESET_MASK			(0x00000001u)
#define PRUCORE_CONTROL_SOFTRESET_SHIFT			(0x00000000u)
#define PRUCORE_CONTROL_SOFTRESET_RESETVAL		(0x00000000u)
#define PRUCORE_CONTROL_SOFTRESET_RESET			(0x00000000u)
#define PRUCORE_CONTROL_SOFTRESET_OUT_OF_RESET		(0x00000001u)
#define PRUCORE_CONTROL_RESETVAL			(0x00000000u)

struct prusscore_regs {
	u32 control;
	/*u32 status;
	u32 wakeup;
	u32 cyclecnt;
	u32 stallcnt;
	u8  rsvd0[12];
	u32 contabblkidx0;
	u32 contabblkidx1;
	u32 contabproptr0;
	u32 contabproptr1;
	u8  rsvd1[976];
	u32 intgpr[32];
	u32 intcter[32];
	u8  rsvd2[768];*/
	u8 res[8188];
};

struct pruss_intc_regs {
	u32 revid;
	u32 control;
	u8  res1[4];
	u32 hcr;
	u32 glblen;
	u8  res2[8];
	u32 glblnstlvl;
	u32 statidxset;
	u32 statidxclr;
	u32 enidxset;
	u32 enidxclr;
	u8  res3[4];
	u32 hostintenidxset;
	u32 hostintenidxclr;
	u8  res4[68];
	u32 glblpriidx;
	u8  res5[380];
	u32 statsetint[2];
	u8  res6[120];
	u32 statclrint[2];
	u8  res7[120];
	u32 enableset[2];
	u8  res8[120];
	u32 enableclr[2];
	u8  res9[120];
	u32 chanmap[16];
	u8  res10[960];
	u32 hostmap[2];
	u8  res11[248];
	u32 hostintpriidx[10];
	u8  res12[984];
	u32 polarity[2];
	u8  res13[120];
	u32 type[2];
	u8  res14[888];
	u32 hostintnstlvl[10];
	u8  res15[984];
	u32 hostinten;
	u8  res16[2812];
};

struct pruss_map {
	u8 dram0[0x2000];
	u8 dram1[0x2000];
	u8 res1[0xC000];
	u8 dsram[0x3000];
	u8 res2[0xD000];
	struct pruss_intc_regs intc;
	struct prusscore_regs core[2];
	u8 res3[0xE000];
	u8 iram0[0x2000];
	u8 res4[0x2000];
	u8 iram1[0x2000];
};

/* PRUSS offsets */
#define PRU0_DRAM	0x2000
#define PRU1_DRAM       0x4000
#define PRU_SRAM	0x10000
#define PRU0_IRAM       0x34000
#define PRU1_IRAM       0x38000
#define PRU0_CORE       0x22000
#define PRU1_CORE       0x24000

#endif
