// SPDX-License-Identifier: GPL-2.0

/*

 * Copyright (c) 2019 MediaTek Inc.

 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/kthread.h>
//#include <linux/wakelock.h>
#include <asm-generic/bug.h>

#include "mt_hotplug_strategy_internal.h"

/*
 * hps task main loop
 */
static int _hps_task_main(void *data)
{
	int cnt = 0;
	void (*algo_func_ptr)(void);

	hps_ctxt_print_basic(1);

	if (hps_ctxt.is_hmp)
		algo_func_ptr = hps_algo_hmp;
	else
		algo_func_ptr = hps_algo_smp;

	while (1) {
		/* TODO: showld we do dvfs?
		 * struct cpufreq_policy *policy;
		 * policy = cpufreq_cpu_get(0);
		 * dbs_freq_increase(policy, policy->max);
		 * cpufreq_cpu_put(policy);
		 */

		(*algo_func_ptr) ();

		if (hps_ctxt.state == STATE_EARLY_SUSPEND)
			wait_event_timeout(hps_ctxt.wait_queue,
					   atomic_read(&hps_ctxt.is_ondemand) !=
					   0,
					   msecs_to_jiffies
					   (HPS_TIMER_INTERVAL_MS * 4));
		else
			wait_event_timeout(hps_ctxt.wait_queue,
					   atomic_read(&hps_ctxt.is_ondemand) !=
					   0,
					   msecs_to_jiffies
					   (HPS_TIMER_INTERVAL_MS));

		if (kthread_should_stop())
			break;
	}

	hps_warn("leave _hps_task_main, cnt:%08d\n", cnt++);
	return 0;
}

/*
 * hps task control interface
 */
int hps_task_start(void)
{
	struct sched_param param = { .sched_priority = HPS_TASK_PRIORITY };

	if (hps_ctxt.tsk_struct_ptr == NULL) {
		hps_ctxt.tsk_struct_ptr =
			kthread_create(_hps_task_main, NULL, "hps_main");
		if (IS_ERR(hps_ctxt.tsk_struct_ptr))
			return PTR_ERR(hps_ctxt.tsk_struct_ptr);

		sched_setscheduler_nocheck(
			hps_ctxt.tsk_struct_ptr, SCHED_FIFO, &param);
		get_task_struct(hps_ctxt.tsk_struct_ptr);
		wake_up_process(hps_ctxt.tsk_struct_ptr);
		hps_warn("hps_task_start success, ptr: %p, pid: %d\n",
			hps_ctxt.tsk_struct_ptr, hps_ctxt.tsk_struct_ptr->pid);
	} else {
		hps_warn("hps task already exist, ptr: %p, pid: %d\n",
			hps_ctxt.tsk_struct_ptr, hps_ctxt.tsk_struct_ptr->pid);
	}

	return 0;
}

void hps_task_stop(void)
{
	if (hps_ctxt.tsk_struct_ptr) {
		kthread_stop(hps_ctxt.tsk_struct_ptr);
		put_task_struct(hps_ctxt.tsk_struct_ptr);
		hps_ctxt.tsk_struct_ptr = NULL;
	}
}

void hps_task_wakeup_nolock(void)
{
	if (hps_ctxt.tsk_struct_ptr) {
		atomic_set(&hps_ctxt.is_ondemand, 1);
		wake_up(&hps_ctxt.wait_queue);
	}
}

void hps_task_wakeup(void)
{
	mutex_lock(&hps_ctxt.lock);

	hps_task_wakeup_nolock();

	mutex_unlock(&hps_ctxt.lock);
}


static int device_hotplug_notifier(unsigned long action, int cpu)
{
	struct device *dev = get_cpu_device(cpu);
	int ret;

	if(dev == NULL)
		return -1;

	switch (action) {
		case CPU_ONLINE:
			dev->offline = false;
			ret = NOTIFY_OK;
			break;

		case CPU_DEAD:
			dev->offline = true;
			ret = NOTIFY_OK;
			break;

		default:
			ret = NOTIFY_DONE;
			break;
	}
	return ret;
}

static int cpuhp_cpu_up(unsigned int cpu)
{
	device_hotplug_notifier(CPU_ONLINE, cpu);
	return 0;
}

static int cpuhp_cpu_dead(unsigned int cpu)
{
	device_hotplug_notifier(CPU_DEAD, cpu);
	return 0;
}


/*
 * init
 */
int hps_core_init(void)
{
	int r = 0;

	hps_warn("hps_core_init\n");

	cpuhp_setup_state_nocalls(CPUHP_BP_PREPARE_DYN,
				"hps/cpuhotplug",
				cpuhp_cpu_up,
				cpuhp_cpu_dead);

	/* init and start task */
	r = hps_task_start();
	if (r)
		hps_error("hps_task_start fail(%d)\n", r);

	return r;
}

/*
 * deinit
 */
int hps_core_deinit(void)
{
	int r = 0;

	hps_warn("hps_core_deinit\n");

	hps_task_stop();

	return r;
}
