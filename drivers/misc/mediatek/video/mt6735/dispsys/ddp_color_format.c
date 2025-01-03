// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (C) 2019  MediaTek Inc.
*
*/

#define LOG_TAG "color_format"

#include "disp_log.h"
#include "DpDataType.h"

enum FORMAT_UNIQUE {
	FORMAT_UNIQUE_BGR565 = 0x00,	/* basic format */
	FORMAT_UNIQUE_RGB888 = 0x01,
	FORMAT_UNIQUE_RGBA8888 = 0x02,
	FORMAT_UNIQUE_ARGB8888 = 0x03,
	FORMAT_UNIQUE_VYUY = 0x04,
	FORMAT_UNIQUE_YVYU = 0x05,
	FORMAT_UNIQUE_YONLY = 0x07,
	FORMAT_UNIQUE_YV12 = 0x08,
	FORMAT_UNIQUE_NV21 = 0x0c,
	FORMAT_UNIQUE_UNKNOWN = 0x100,
};

int fmt_bpp(enum DP_COLOR_ENUM fmt)
{
	return DP_COLOR_BITS_PER_PIXEL(fmt) / 4;
}

int fmt_swap(enum DP_COLOR_ENUM fmt)
{
	return DP_COLOR_GET_SWAP_ENABLE(fmt);
}

int fmt_color_space(enum DP_COLOR_ENUM fmt)
{
	return DP_COLOR_GET_COLOR_GROUP(fmt);
}

int fmt_is_yuv422(enum DP_COLOR_ENUM fmt)
{
	return DP_COLOR_GET_H_SUBSAMPLE(fmt) && (!DP_COLOR_GET_V_SUBSAMPLE(fmt));
}

int fmt_is_yuv420(enum DP_COLOR_ENUM fmt)
{
	return DP_COLOR_GET_H_SUBSAMPLE(fmt) && DP_COLOR_GET_V_SUBSAMPLE(fmt);
}

int fmt_hw_value(enum DP_COLOR_ENUM fmt)
{
	return DP_COLOR_GET_HW_FORMAT(fmt);
}

char *fmt_string(enum DP_COLOR_ENUM fmt)
{
	switch (fmt) {
	case eBGR565:
		return "eBGR565";
	case eRGB565:
		return "eRGB565";
	case eRGB888:
		return "eRGB888";
	case eBGR888:
		return "eBGR888";
	case eRGBA8888:
		return "eRGBA8888";
	case eBGRA8888:
		return "eBGRA8888";
	case eARGB8888:
		return "eARGB8888";
	case eABGR8888:
		return "eABGR8888";
	case eVYUY:
		return "eVYUY";
	case eUYVY:
		return "eUYVY";
	case eYVYU:
		return "eYVYU";
	case eYUY2:
		return "eYUY2";
	case eY800:
		return "eY800";
	case eYV21:
		return "eYV21";
	case eYV12:
		return "eYV12";
	case eNV21:
		return "eNV21";
	case eNV12:
		return "eNV12";
	default:
		DISPERR("fmt_string unknown fmt=0x%x\n", fmt);
		break;
	}
	return "unknown";
}

enum DP_COLOR_ENUM fmt_type(int unique, int swap)
{
	switch (unique) {
	case FORMAT_UNIQUE_BGR565:
		return swap ? eBGR565 : eRGB565;
	case FORMAT_UNIQUE_RGB888:
		return swap ? eRGB888 : eBGR888;
	case FORMAT_UNIQUE_RGBA8888:
		return swap ? eRGBA8888 : eBGRA8888;
	case FORMAT_UNIQUE_ARGB8888:
		return swap ? eARGB8888 : eABGR8888;
	case FORMAT_UNIQUE_VYUY:
		return swap ? eVYUY : eUYVY;
	case FORMAT_UNIQUE_YVYU:
		return swap ? eYVYU : eYUY2;
	case FORMAT_UNIQUE_YONLY:
		return eY800;
	case FORMAT_UNIQUE_YV12:
		return swap ? eYV12 : eYV21;
	case FORMAT_UNIQUE_NV21:
		return swap ? eNV21 : eNV12;
	default:
		DISPERR("fmt_type unknown unique=%d, swap=%d\n", unique, swap);
		ASSERT(0);
		break;
	}
	return eBGR565;
}
