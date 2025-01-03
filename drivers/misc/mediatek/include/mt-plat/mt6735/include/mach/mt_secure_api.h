// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (C) 2019  MediaTek Inc.
*
*/


#ifndef _MT_SECURE_API_H_
#define _MT_SECURE_API_H_

#include <mt-plat/sync_write.h>

#if defined(CONFIG_ARM_PSCI) || defined(CONFIG_MTK_PSCI)
/* Error Code */
#define SIP_SVC_E_SUCCESS               0
#define SIP_SVC_E_NOT_SUPPORTED         -1
#define SIP_SVC_E_INVALID_PARAMS        -2
#define SIP_SVC_E_INVALID_Range         -3
#define SIP_SVC_E_PERMISSION_DENY       -4

#define MTK_SIP_KERNEL_MCUSYS_WRITE         0x82000287
#define MTK_SIP_KERNEL_MCUSYS_ACCESS_COUNT  0x82000288
#define MTK_SIP_KERNEL_L2_SHARING           0x82000286
#define MTK_SIP_KERNEL_WDT                  0x82000200

#define TBASE_SMC_AEE_DUMP                  (0xB200AEED)
#define MTK_SIP_KERNEL_GIC_DUMP         0x82000201
#define MTK_SIP_KERNEL_DAPC_INIT        0x8200026E
#define MTK_SIP_KERNEL_EMIMPU_WRITE         0x82000260
#define MTK_SIP_KERNEL_EMIMPU_READ          0x82000261
#define MTK_SIP_KERNEL_EMIMPU_SET           0x82000262
#define MTK_SIP_KERNEL_ICACHE_DUMP          0x82000284
#define MTK_SIP_KERNEL_TIME_SYNC		0x82000202
#define MTK_SIP_KERNEL_MSG                  0x82000214
#define MTK_SIP_KERNEL_GPIO_WRITE   0xC20002A1
#define MTK_SIP_KERNEL_GPIO_READ    0xC20002A2

#ifdef CONFIG_ARM64
/* SIP SMC Call 64 */
static noinline int mt_secure_call(u64 function_id,
	u64 arg0, u64 arg1, u64 arg2, u64 arg3)
{
	register u64 reg0 __asm__("x0") = function_id;
	register u64 reg1 __asm__("x1") = arg0;
	register u64 reg2 __asm__("x2") = arg1;
	register u64 reg3 __asm__("x3") = arg2;
	int ret = 0;

	asm volatile ("smc    #0\n" : "+r" (reg0) :
		"r"(reg1), "r"(reg2), "r"(reg3));

	ret = (int)reg0;
	return ret;
}

#else
#include <asm/opcodes-sec.h>
#include <asm/opcodes-virt.h>
/* SIP SMC Call 32 */
static noinline int mt_secure_call(u32 function_id,
	u32 arg0, u32 arg1, u32 arg2, u32 arg3)
{
	register u32 reg0 __asm__("r0") = function_id;
	register u32 reg1 __asm__("r1") = arg0;
	register u32 reg2 __asm__("r2") = arg1;
	register u32 reg3 __asm__("r3") = arg2;
	int ret = 0;

	asm volatile (__SMC(0) : "+r"(reg0),
		"+r"(reg1), "+r"(reg2), "+r"(reg3));

	ret = reg0;
	return ret;
}
#endif
#define tbase_trigger_aee_dump() \
	mt_secure_call(TBASE_SMC_AEE_DUMP, 0, 0, 0, 0)
#endif

#define emi_mpu_smc_write(offset, val) \
mt_secure_call(MTK_SIP_KERNEL_EMIMPU_WRITE, offset, val, 0, 0)

#define emi_mpu_smc_read(offset) \
mt_secure_call(MTK_SIP_KERNEL_EMIMPU_READ, offset, 0, 0, 0)

#define emi_mpu_smc_set(start, end, region_permission) \
mt_secure_call(MTK_SIP_KERNEL_EMIMPU_SET, start, end, region_permission, 0)

#define CONFIG_MCUSYS_WRITE_PROTECT

#if defined(CONFIG_MCUSYS_WRITE_PROTECT) && \
	(defined(CONFIG_ARM_PSCI) || defined(CONFIG_MTK_PSCI))

#ifdef CONFIG_ARM64		/* Kernel 64 */
#define mcusys_smc_write(virt_addr, val) \
	mt_reg_sync_writel(val, virt_addr)

#define mcusys_smc_write_phy(addr, val) \
mt_secure_call(MTK_SIP_KERNEL_MCUSYS_WRITE, addr, val, 0, 0)

#define mcusys_access_count() \
mt_secure_call(MTK_SIP_KERNEL_MCUSYS_ACCESS_COUNT, 0, 0, 0, 0)

#else				/* Kernel 32 */
#define SMC_IO_VIRT_TO_PHY(addr) (addr-0xF0000000+0x10000000)
#define mcusys_smc_write(virt_addr, val) \
	mt_secure_call(MTK_SIP_KERNEL_MCUSYS_WRITE, \
		SMC_IO_VIRT_TO_PHY(virt_addr), val, 0, 0)

#define mcusys_smc_write_phy(addr, val) \
mt_secure_call(MTK_SIP_KERNEL_MCUSYS_WRITE, addr, val, 0, 0)

#define mcusys_access_count() \
mt_secure_call(MTK_SIP_KERNEL_MCUSYS_ACCESS_COUNT, 0, 0, 0, 0)
#endif

#else
#define mcusys_smc_write(virt_addr, val) \
	mt_reg_sync_writel(val, virt_addr)
#define mcusys_access_count()               (0)
#endif

#define kernel_smc_msg(x1, x2, x3) \
	mt_secure_call(MTK_SIP_KERNEL_MSG, x1, x2, x3, 0)

#endif				/* _MT_SECURE_API_H_ */
