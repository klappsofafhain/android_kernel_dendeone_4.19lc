/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */
#ifndef _HOTPLUG
#define _HOTPLUG

#include <linux/kernel.h>
#include <linux/atomic.h>

#define BOOTROM_BASE	(0x00000000)
#define BOOTSRAM_BASE	(0x00100000)

#define CCI400_BASE	(0xF0390000)
#define INFRACFG_AO_BASE (0xF0001000)
#define MCUCFG_BASE	(0xF0200000)

/* log */
#define HOTPLUG_LOG_NONE		0
#define HOTPLUG_LOG_WITH_DEBUG		1
#define HOTPLUG_LOG_WITH_WARN		2

#define HOTPLUG_LOG_PRINT		HOTPLUG_LOG_NONE

#if (HOTPLUG_LOG_PRINT == HOTPLUG_LOG_NONE)
#define HOTPLUG_INFO(fmt, args...)
#elif (HOTPLUG_LOG_PRINT == HOTPLUG_LOG_WITH_DEBUG)
#define HOTPLUG_INFO(fmt, args...)	pr_debug("[Power/hotplug] "fmt, ##args)
#elif (HOTPLUG_LOG_PRINT == HOTPLUG_LOG_WITH_WARN)
#define HOTPLUG_INFO(fmt, args...)	pr_warn("[Power/hotplug] "fmt, ##args)
#endif

/* profilling */
/* #define CONFIG_HOTPLUG_PROFILING */
#define CONFIG_HOTPLUG_PROFILING_COUNT		100

/* register address - bootrom power*/
#define BOOTROM_BOOT_ADDR		(INFRACFG_AO_BASE + 0x800)
#define BOOTROM_SEC_CTRL		(INFRACFG_AO_BASE + 0x804)

/* register address - CCI400 */
#define CCI400_STATUS			(CCI400_BASE + 0x000C)
#define CHANGE_PENDING			(1U << 0)
#define CCI400_SI4_BASE			(CCI400_BASE + 0x5000)
#define CCI400_SI4_SNOOP_CONTROL	(CCI400_SI4_BASE)
#define CCI400_SI3_BASE			(CCI400_BASE + 0x4000)
#define CCI400_SI3_SNOOP_CONTROL	(CCI400_SI3_BASE)
#define DVM_MSG_REQ			(1U << 1)
#define SNOOP_REQ			(1U << 0)

/* register address - acinactm */
#define MP0_AXI_CONFIG			(MCUCFG_BASE + 0x02C)
#define MP1_AXI_CONFIG			(MCUCFG_BASE + 0x22C)
#define ACINACTM			(1U << 4)

/* register address - aa64naa32 */
#define MP0_MISC_CONFIG3		(MCUCFG_BASE + 0x03C)
#define MP1_MISC_CONFIG3		(MCUCFG_BASE + 0x23C)

#define REG_WRITE(addr, value)		mt_reg_sync_writel(value, addr)

/* power on/off cpu*/
#define CONFIG_HOTPLUG_WITH_POWER_CTRL

#ifdef CONFIG_MACH_MT6580
static inline int arch_get_cluster_cpus(struct cpumask *cpus, int cluster_id)
{
	int cpu = 0;

	cpumask_clear(cpus);

	while (cpu < 4) {
		cpumask_set_cpu(cpu, cpus);
		cpu++;
	}
	return 0;
}
#endif


/* global variable */
extern atomic_t hotplug_cpu_count;

extern void __disable_dcache(void);
extern void __enable_dcache(void);
extern void __inner_clean_dcache_L2(void);
extern void inner_dcache_flush_L1(void);
extern void inner_dcache_flush_L2(void);
extern void __switch_to_smp(void);
extern void __switch_to_amp(void);
extern void __disable_dcache__inner_flush_dcache_L1(void);
extern void
__disable_dcache__inner_flush_dcache_L1__inner_clean_dcache_L2(void);
extern void
__disable_dcache__inner_flush_dcache_L1__inner_flush_dcache_L2(void);

/* mt cpu hotplug callback for smp_operations */
extern int mt_cpu_kill(unsigned int cpu);
extern void mt_cpu_die(unsigned int cpu);
extern int mt_cpu_disable(unsigned int cpu);

/* set the priority mask to 0x00 for masking all irqs to this cpu */
extern void gic_set_primask(void);
/* restore the priority mask value */
extern void gic_clear_primask(void);

#endif
