// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 *
 */

#ifndef __MTKFB_VSYNC_H__
#define __MTKFB_VSYNC_H__


#define MTKFB_VSYNC_DEVNAME "mtkfb_vsync"

#define MTKFB_VSYNC_IOCTL_MAGIC      'V'

enum vsync_src {
	MTKFB_VSYNC_SOURCE_LCM = 0,
	MTKFB_VSYNC_SOURCE_HDMI = 1,
	MTKFB_VSYNC_SOURCE_EPD = 2,
};

#define MTKFB_VSYNC_IOCTL     _IOW(MTKFB_VSYNC_IOCTL_MAGIC, 1, enum vsync_src)

#if defined(CONFIG_MACH_MT6735)
	|| defined(CONFIG_MACH_MT6735M)
	|| defined(CONFIG_MACH_MT6753)
	|| defined(CONFIG_MACH_MT8167)
void mtkfb_vsync_log_enable(int enable);
#endif

#endif				/* MTKFB_VSYNC_H */
