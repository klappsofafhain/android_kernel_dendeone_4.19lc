/*
 *  linux/arch/arm/kernel/irq.c
 *
 *  Copyright (C) 1992 Linus Torvalds
 *  Modifications for ARM processor Copyright (C) 1995-2000 Russell King.
 *
 *  Support for Dynamic Tick Timer Copyright (C) 2004-2005 Nokia Corporation.
 *  Dynamic Tick Timer written by Tony Lindgren <tony@atomide.com> and
 *  Tuukka Tikkanen <tuukka.tikkanen@elektrobit.com>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  This file contains the code used by various IRQ handling routines:
 *  asking for different IRQ's should be done through these routines
 *  instead of just grabbing them. Thus setups with different IRQ numbers
 *  shouldn't result in any weird surprises, and installing new handlers
 *  should be easier.
 *
 *  IRQ's are in fact implemented a bit like signal handlers for the kernel.
 *  Naturally it's not a 1:1 relation, but there are similarities.
 */
#include <linux/kernel_stat.h>
#include <linux/signal.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqchip.h>
#include <linux/random.h>
#include <linux/smp.h>
#include <linux/init.h>
#include <linux/seq_file.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/kallsyms.h>
#include <linux/proc_fs.h>
#include <linux/export.h>

#include <asm/hardware/cache-l2x0.h>
#include <asm/hardware/cache-uniphier.h>
#include <asm/outercache.h>
#include <asm/exception.h>
#include <asm/mach/arch.h>
#include <asm/mach/irq.h>
#include <asm/mach/time.h>

unsigned long irq_err_count;

int arch_show_interrupts(struct seq_file *p, int prec)
{
#ifdef CONFIG_FIQ
	show_fiq_list(p, prec);
#endif
#ifdef CONFIG_SMP
	show_ipi_list(p, prec);
#endif
	seq_printf(p, "%*s: %10lu\n", prec, "Err", irq_err_count);
	return 0;
}

/*
 * handle_IRQ handles all hardware IRQ's.  Decoded IRQs should
 * not come via this function.  Instead, they should provide their
 * own 'handler'.  Used by platform code implementing C-based 1st
 * level decoding.
 */
void handle_IRQ(unsigned int irq, struct pt_regs *regs)
{
	__handle_domain_irq(NULL, irq, false, regs);
}

/*
 * asm_do_IRQ is the interface to be used from assembly code.
 */
asmlinkage void __exception_irq_entry
asm_do_IRQ(unsigned int irq, struct pt_regs *regs)
{
	handle_IRQ(irq, regs);
}

void __init init_IRQ(void)
{
	int ret;

	if (IS_ENABLED(CONFIG_OF) && !machine_desc->init_irq)
		irqchip_init();
	else
		machine_desc->init_irq();

	if (IS_ENABLED(CONFIG_OF) && IS_ENABLED(CONFIG_CACHE_L2X0) &&
	    (machine_desc->l2c_aux_mask || machine_desc->l2c_aux_val)) {
		if (!outer_cache.write_sec)
			outer_cache.write_sec = machine_desc->l2c_write_sec;
		ret = l2x0_of_init(machine_desc->l2c_aux_val,
				   machine_desc->l2c_aux_mask);
		if (ret && ret != -ENODEV)
			pr_err("L2C: failed to init: %d\n", ret);
	}

	uniphier_cache_init();
}

#ifdef CONFIG_SPARSE_IRQ
int __init arch_probe_nr_irqs(void)
{
	nr_irqs = machine_desc->nr_irqs ? machine_desc->nr_irqs : NR_IRQS;
	return nr_irqs;
}
#endif

#ifdef CONFIG_MTK_IRQ_NEW_DESIGN
#include <linux/slab.h>
#include <linux/bitmap.h>

static inline bool mt_cpumask_equal(const struct cpumask *src1p, const struct cpumask *src2p)
{
	return bitmap_equal(cpumask_bits(src1p), cpumask_bits(src2p), num_possible_cpus());
}

struct thread_safe_list irq_need_migrate_list[CONFIG_NR_CPUS];

void fixup_update_irq_need_migrate_list(struct irq_desc *desc)
{
	struct irq_data *d = irq_desc_get_irq_data(desc);
	struct irq_chip *c = irq_data_get_irq_chip(d);

	/* if this IRQ is not per-cpu IRQ => force to target all*/
	if (!irqd_is_per_cpu(d)) {
		/* gic affinity to target all and update the smp affinity */
		if (!c->irq_set_affinity)
			pr_err("IRQ%u: unable to set affinity\n", d->irq);
		else if (c->irq_set_affinity(d, cpu_possible_mask, true) == IRQ_SET_MASK_OK)
			cpumask_copy(irq_data_get_affinity_mask(d), cpu_possible_mask);
	}
}

bool check_consistency_of_irq_settings(struct irq_desc *desc)
{
	struct irq_data *d = irq_desc_get_irq_data(desc);
	struct per_cpu_irq_desc *node;
	struct list_head *pos, *temp;
	cpumask_t per_cpu_list_affinity, gic_target_affinity, tmp_affinity;
	bool ret = true;
	int cpu;

	/* if this IRQ is per-cpu IRQ: only check gic setting */
	if (irqd_is_per_cpu(d))
		goto check_gic;

	/* get the setting in per-cpu irq-need-lists */
	cpumask_clear(&per_cpu_list_affinity);

	rcu_read_lock();
	for_each_cpu(cpu, cpu_possible_mask)
		list_for_each_safe(pos, temp, &(irq_need_migrate_list[cpu].list)) {
			node = list_entry_rcu(pos, struct per_cpu_irq_desc, list);
			if (node->desc == desc) {
				cpumask_set_cpu(cpu, &per_cpu_list_affinity);
				break;
			}
		}
	rcu_read_unlock();

	/* compare with the setting of smp affinity */
	if (mt_cpumask_equal(irq_data_get_affinity_mask(d), cpu_possible_mask)) {
		/*
		 * if smp affinity is set to all CPUs
		 * AND this IRQ is not be found in any per-cpu list -> success
		 */
		ret = (cpumask_empty(&per_cpu_list_affinity)) ? true : false;
	} else if (!mt_cpumask_equal(&per_cpu_list_affinity, irq_data_get_affinity_mask(d))) {
		/* smp affinity should be the same as per-cpu list */
		ret = false;
	}

	/* print out to error logs */
	if (!ret) {
		pr_err("[IRQ] IRQ %d: smp affinity is not consistent with per-cpu list\n", d->irq);
		cpumask_xor(&tmp_affinity, &per_cpu_list_affinity, irq_data_get_affinity_mask(d));

		/* iterates on cpus with inconsitent setting */
		for_each_cpu(cpu, &tmp_affinity)
			if (cpumask_test_cpu(cpu, irq_data_get_affinity_mask(d)))
				pr_err("[IRQ] @CPU%u: smp affinity is set, but per-cpu list is not set\n", cpu);
			else
				pr_err("[IRQ] @CPU%u: smp affinity is not set, but per-cpu list is set\n", cpu);
	}


check_gic:

	if (mt_is_secure_irq(d)) {
		/* no need to check WDT */
		ret = true;
		goto out;
	}

	/* get the setting in gic setting and compare with the setting in gic setting*/
	cpumask_clear(&gic_target_affinity);

	if (!mt_get_irq_gic_targets(d, &gic_target_affinity)) {
		/* failed to get GICD_ITARGETSR the setting */
		pr_err("[IRQ] unable to get GICD_ITARGETSR setting of IRQ %d\n", d->irq);
		ret = false;
	} else if (!mt_cpumask_equal(&gic_target_affinity, irq_data_get_affinity_mask(d))) {
		pr_err("[IRQ] IRQ %d: smp affinity is not consistent with GICD_ITARGETSR\n", d->irq);
		cpumask_xor(&tmp_affinity, &gic_target_affinity, irq_data_get_affinity_mask(d));

		/* iterates on cpus with inconsitent setting */
		for_each_cpu(cpu, &tmp_affinity)
			if (cpumask_test_cpu(cpu, irq_data_get_affinity_mask(d)))
				pr_err("[IRQ] @CPU%u: smp affinity is set, but gic reg is not set\n", cpu);
			else
				pr_err("[IRQ] @CPU%u: smp affinity is not set, but gic reg is set\n", cpu);
		ret = false;
	}

out:

	if (!ret)
		pr_err("[IRQ] IRQ %d: the affintiy setting is INCONSITENT\n", d->irq);

	return ret;
}

void dump_irq_need_migrate_list(const struct cpumask *mask)
{
	struct per_cpu_irq_desc *node;
	struct list_head *pos, *temp;
	int cpu;

	rcu_read_lock();
	for_each_cpu(cpu, mask) {
		pr_debug("[IRQ] dump per-cpu irq-need-migrate list of CPU%u\n", cpu);
		list_for_each_safe(pos, temp, &(irq_need_migrate_list[cpu].list)) {
			node = list_entry_rcu(pos, struct per_cpu_irq_desc, list);
			pr_debug("[IRQ] IRQ %d\n", (node->desc->irq_data).irq);
		}
	}
	rcu_read_unlock();
}

static void del_from_irq_need_migrate_list(struct irq_desc *desc, const struct cpumask *cpumask_to_del)
{
	struct per_cpu_irq_desc *node, *next;
	int cpu;

	for_each_cpu(cpu, cpumask_to_del) {
		spin_lock(&(irq_need_migrate_list[cpu].lock));
		list_for_each_entry_safe(node, next,
			&(irq_need_migrate_list[cpu].list), list) {
			if (node->desc != desc)
				continue;
			pr_debug("[IRQ] list_del to cpu %d\n", cpu);
			list_del_rcu(&node->list);
			kfree(node);
			break;
		}
		spin_unlock(&(irq_need_migrate_list[cpu].lock));
	}
}

/* return false for error */
static bool add_to_irq_need_migrate_list(struct irq_desc *desc, const struct cpumask *cpumask_to_add)
{
	struct per_cpu_irq_desc *node;
	int cpu;
	bool ret = true;

	for_each_cpu(cpu, cpumask_to_add) {
		spin_lock(&(irq_need_migrate_list[cpu].lock));

		node = kmalloc(sizeof(struct per_cpu_irq_desc), GFP_ATOMIC);
		if (node == NULL) {
			spin_unlock(&(irq_need_migrate_list[cpu].lock));
			ret = false;
			break;
		}

		node->desc = desc;
		pr_debug("[IRQ] list_add to cpu %d\n", cpu);
		list_add_rcu(&node->list, &(irq_need_migrate_list[cpu].list));
		spin_unlock(&(irq_need_migrate_list[cpu].lock));
	}

	/* delete what we have added when failed */
	if (!ret) {
		pr_err("[IRQ] kmalloc failed: cannot add node into CPU%d per-cpu IRQ list\n", cpu);
		del_from_irq_need_migrate_list(desc, cpumask_to_add);
	}

	return ret;
}

/*
 * must be invoked before the cpumask_copy of irq_desc for getting the original smp affinity
 * return @true when success
 */
bool update_irq_need_migrate_list(struct irq_desc *desc, const struct cpumask *new_affinity)
{
	struct irq_data *d = irq_desc_get_irq_data(desc);
	cpumask_t need_update_affinity, tmp_affinity;

	pr_debug("[IRQ] update per-cpu list (IRQ %d)\n", d->irq);

	/* find out the per-cpu irq-need-migrate lists to be updated */
	cpumask_xor(&need_update_affinity, irq_data_get_affinity_mask(d), new_affinity);

	/* return if there is no need to update the per-cpu irq-need-migrate lists */
	if (cpumask_empty(&need_update_affinity))
		return true;

	/* special cases */
	if (mt_cpumask_equal(new_affinity, cpu_possible_mask)) {
		/*
		 * case 1: new affinity is to all cpus
		 * clear this IRQs from all per-cpu irq-need-migrate lists of old affinity
		 */
		del_from_irq_need_migrate_list(desc, irq_data_get_affinity_mask(d));
		return true;
	} else if (mt_cpumask_equal(irq_data_get_affinity_mask(d), cpu_possible_mask)) {
		/*
		 * case 2: old affinity is to all cpus
		 * add this IRQs to per-cpu irq-need-migrate lists of new affinity
		 */
		return add_to_irq_need_migrate_list(desc, new_affinity);
	}

	/* needs to be update AND is in new affinity -> list_add */
	cpumask_and(&tmp_affinity, &need_update_affinity, new_affinity);
	if (!add_to_irq_need_migrate_list(desc, &tmp_affinity))
		return false;

	/* needs to be update AND is in old affinity -> list_del */
	cpumask_and(&tmp_affinity, &need_update_affinity, irq_data_get_affinity_mask(d));
	del_from_irq_need_migrate_list(desc, &tmp_affinity);

	return true;
}

/* update smp affinity and per-cpu irq-need-migrate lists */
void update_affinity_settings(struct irq_desc *desc, const struct cpumask *new_affinity, bool update_smp_affinity)
{
	struct irq_data *d = irq_desc_get_irq_data(desc);
	bool need_fix = false;

	need_fix = !update_irq_need_migrate_list(desc, new_affinity);
	if (update_smp_affinity)
		cpumask_copy(irq_data_get_affinity_mask(d), new_affinity);
	if (need_fix)
		fixup_update_irq_need_migrate_list(desc);

#ifdef CONFIG_MTK_IRQ_NEW_DESIGN_DEBUG
	/* verify the consistency of IRQ setting after updating */
	BUG_ON(!check_consistency_of_irq_settings(desc));
#endif
}
#endif
