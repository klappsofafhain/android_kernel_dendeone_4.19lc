// SPDX-License-Identifier: GPL-2.0

/*

 * Copyright (c) 2019 MediaTek Inc.

 */

#include <linux/kernel.h>
#include <linux/module.h>
#include "ged.h"
#include "ged_kpi.h"

void (*ged_vsync_notifier_fp)(void);
EXPORT_SYMBOL(ged_vsync_notifier_fp);


void ged_notification(GED_NOTIFICATION_TYPE eType)
{
	switch (eType) {
	case GED_NOTIFICATION_TYPE_SW_VSYNC:
		ged_kpi_sw_vsync();
		if (ged_vsync_notifier_fp)
			ged_vsync_notifier_fp();
		break;
	case GED_NOTIFICATION_TYPE_HW_VSYNC_PRIMARY_DISPLAY:
		ged_kpi_hw_vsync();
		break;
	}
}
EXPORT_SYMBOL(ged_notification);
int ged_set_target_fps(unsigned int target_fps, int mode)
{
	return 0;
}
EXPORT_SYMBOL(ged_set_target_fps);
void ged_get_latest_perf_state(long long *t_cpu_remained,
								long long *t_gpu_remained,
								long *t_cpu_target,
								long *t_gpu_target)
{
	ged_kpi_get_latest_perf_state(t_cpu_remained, t_gpu_remained, t_cpu_target, t_gpu_target);
}
unsigned int ged_get_cur_fps(void)
{
	return ged_kpi_get_cur_fps();
}
