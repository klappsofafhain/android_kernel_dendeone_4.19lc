// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (C) 2019  MediaTek Inc.
*
*/
#ifndef __EXTD_PLATFORM_H__
#define __EXTD_PLATFORM_H__

#include "ddp_hal.h"
#include "disp_drv_platform.h"


#define MAX_SESSION_COUNT		5

/* #define MTK_LCD_HW_3D_SUPPORT */
#define ALIGN_TO(x, n)  \
	(((x) + ((n) - 1)) & ~((n) - 1))


#define MTK_EXT_DISP_OVERLAY_SUPPORT

/* /#define EXTD_DBG_USE_INNER_BUF */

/*#define HW_OVERLAY_COUNT  4*/
#define EXTD_OVERLAY_CNT  0
#define HW_DPI_VSYNC_SUPPORT 0

#define DISP_MODULE_RDMA DISP_MODULE_RDMA1
#endif
