// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (C) 2019  MediaTek Inc.
*
*/
#ifndef __MT_SMP_H
#define __MT_SMP_H

#include <linux/cpumask.h>
#include <mach/irqs.h>
#include <asm/cputype.h>

#include <mt-plat/sync_write.h>

#include "hotplug.h"

extern void irq_raise_softirq(const struct cpumask *mask, unsigned int irq);

/* use Soft IRQ1 as the IPI */
static inline void smp_cross_call(const struct cpumask *mask)
{
	irq_raise_softirq(mask, CPU_BRINGUP_SGI);
}

#ifdef CONFIG_ARM64
static inline int get_HW_cpuid(void)
{
	u64 mpidr;
	u32 id;

	mpidr = read_cpuid_mpidr();
	id = (mpidr & 0xff) + ((mpidr & 0xff00) >> 6);

	return id;
}
#else
static inline int get_HW_cpuid(void)
{
	int id;

	asm("mrc     p15, 0, %0, c0, c0, 5 @ Get CPUID\n":"=r"(id));
	return (id & 0x3) + ((id & 0xF00) >> 6);
}
#endif

static inline void mt_smp_set_boot_addr(u32 addr, int cpu)
{
	mt_reg_sync_writel(addr, BOOTROM_BOOT_ADDR);
}

#endif
