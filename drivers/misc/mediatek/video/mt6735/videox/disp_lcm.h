// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (C) 2018 MediaTek Inc.
*
*/

#ifndef _DISP_LCM_H_
#define _DISP_LCM_H_
#include "lcm_drv.h"

#define MAX_LCM_NUMBER	2

extern unsigned char lcm_name_list[][128];
extern struct LCM_DRIVER lcm_common_drv;
extern struct LCM_DRIVER *lcm_driver_list[];
extern unsigned int lcm_count;

typedef struct {
	struct LCM_PARAMS *params;
	struct LCM_DRIVER *drv;
	enum LCM_INTERFACE_ID lcm_if_id;
	int module;
	int is_inited;
	unsigned int lcm_original_width;
	unsigned int lcm_original_height;
	int index;
#if !defined(CONFIG_MTK_LEGACY)
	unsigned int driver_id;
	unsigned int module_id;
#endif
} disp_lcm_handle, *pdisp_lcm_handle;

disp_lcm_handle *disp_lcm_probe(char *plcm_name, enum LCM_INTERFACE_ID lcm_id, int is_lcm_inited);
int disp_lcm_init(disp_lcm_handle *plcm, int force);
struct LCM_PARAMS *disp_lcm_get_params(disp_lcm_handle *plcm);
enum LCM_INTERFACE_ID disp_lcm_get_interface_id(disp_lcm_handle *plcm);
int disp_lcm_update(disp_lcm_handle *plcm, int x, int y, int w, int h, int force);
int disp_lcm_esd_check(disp_lcm_handle *plcm);
int disp_lcm_esd_recover(disp_lcm_handle *plcm);
int disp_lcm_suspend(disp_lcm_handle *plcm);
int disp_lcm_resume(disp_lcm_handle *plcm);
int disp_lcm_set_backlight(disp_lcm_handle *plcm, int level);
int disp_lcm_read_fb(disp_lcm_handle *plcm);
int disp_lcm_ioctl(disp_lcm_handle *plcm, enum LCM_IOCTL ioctl, unsigned int arg);
int disp_lcm_is_video_mode(disp_lcm_handle *plcm);
int disp_lcm_is_inited(disp_lcm_handle *plcm);
unsigned int disp_lcm_ATA(disp_lcm_handle *plcm);
void *disp_lcm_switch_mode(disp_lcm_handle *plcm, int mode);
int disp_lcm_set_cmd(disp_lcm_handle *plcm, void *handle, int *lcm_cmd, unsigned int cmd_num);

#endif
