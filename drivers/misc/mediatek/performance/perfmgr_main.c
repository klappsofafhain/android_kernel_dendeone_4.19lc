// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/proc_fs.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>

#include "tchbst.h"
#include "io_ctrl.h"
#include "boost_ctrl.h"
#include "mtk_perfmgr_internal.h"

int clstr_num;
static int perfmgr_probe(struct platform_device *dev)
{
	return 0;
}

struct platform_device perfmgr_device = {
	.name   = "perfmgr",
	.id        = -1,
};

static int perfmgr_suspend(struct device *dev)
{
#ifdef CONFIG_MTK_PERFMGR_TOUCH_BOOST
	ktch_suspend();
#endif
	return 0;
}

static int perfmgr_resume(struct device *dev)
{
	return 0;
}
static int perfmgr_remove(struct platform_device *dev)
{
	topo_ctrl_exit();
	return 0;
}
static struct platform_driver perfmgr_driver = {
	.probe      = perfmgr_probe,
	.remove     = perfmgr_remove,
	.driver     = {
		.name = "perfmgr",
		.pm = &(const struct dev_pm_ops){
			.suspend = perfmgr_suspend,
			.resume = perfmgr_resume,
		},
	},
};

static int perfmgr_main_data_init(void)
{
	clstr_num = (unsigned int)arch_nr_clusters();
	return 0;
}

/*--------------------INIT------------------------*/

static int __init init_perfmgr(void)
{
	struct proc_dir_entry *perfmgr_root = NULL;
	int ret = 0;

	pr_debug("__init init_perfmgr\n");

	ret = platform_device_register(&perfmgr_device);
	if (ret)
		return ret;
	ret = platform_driver_register(&perfmgr_driver);
	if (ret)
		return ret;

	perfmgr_main_data_init();

	perfmgr_root = proc_mkdir("perfmgr", NULL);

	init_boostctrl(perfmgr_root);

	return 0;
}
device_initcall(init_perfmgr);
