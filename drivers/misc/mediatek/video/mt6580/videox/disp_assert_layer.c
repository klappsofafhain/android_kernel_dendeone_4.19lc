// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 *
 */

#include <linux/types.h>
#include "primary_display.h"
#include "ddp_hal.h"
#include "disp_assert_layer.h"
#include <linux/semaphore.h>
#include <linux/mutex.h>
#include "ddp_mmp.h"
#include "disp_drv_platform.h"
#include "disp_session.h"
#include "disp_log.h"
#include "disp_recorder.h"
#include <linux/string.h>
#include <linux/semaphore.h>
#include <asm/cacheflush.h>
#include <linux/module.h>

#define DAL_BPP             (2)
#define DAL_WIDTH           (DISP_GetScreenWidth())
#define DAL_HEIGHT          (DISP_GetScreenHeight())

#ifdef CONFIG_MTK_FB_SUPPORT_ASSERTION_LAYER
#include "mtkfb_console.h"
/* --------------------------------------------------------------------------- */
#define DAL_FORMAT          (DISP_FORMAT_RGB565)
#define DAL_BG_COLOR        (dal_bg_color)
#define DAL_FG_COLOR        (dal_fg_color)
#define RGB888_To_RGB565(x) ((((x) & 0xF80000) >> 8) |                      \
			     (((x) & 0x00FC00) >> 5) |                      \
			     (((x) & 0x0000F8) >> 3))
#define MAKE_TWO_RGB565_COLOR(high, low)  (((low) << 16) | (high))
#define DAL_LOG(fmt, arg...)	pr_debug("DISP/DAL " fmt, ##arg)
/* --------------------------------------------------------------------------- */
static MFC_HANDLE mfc_handle;
static void *dal_fb_addr;
static unsigned long dal_fb_pa;
static unsigned int dal_fg_color = RGB888_To_RGB565(DAL_COLOR_WHITE);
static unsigned int dal_bg_color = RGB888_To_RGB565(DAL_COLOR_RED);
static char dal_print_buffer[1024];
bool dal_shown = false;
unsigned int isAEEEnabled = 0;
DEFINE_SEMAPHORE(dal_sem);
/* --------------------------------------------------------------------------- */

uint32_t DAL_GetLayerSize(void)
{
	return DAL_WIDTH * DAL_HEIGHT * DAL_BPP + 4096;
}

enum DAL_STATUS DAL_SetScreenColor(enum DAL_COLOR color)
{
	uint32_t i;
	uint32_t size;
	uint32_t BG_COLOR;
	struct MFC_CONTEXT *ctxt = NULL;
	uint32_t offset;
	unsigned int *addr;

	color = RGB888_To_RGB565(color);
	BG_COLOR = MAKE_TWO_RGB565_COLOR(color, color);

	ctxt = (struct MFC_CONTEXT *) mfc_handle;
	if (!ctxt)
		return DAL_STATUS_FATAL_ERROR;
	if (ctxt->screen_color == color)
		return DAL_STATUS_OK;
	offset = MFC_Get_Cursor_Offset(mfc_handle);
	addr = (unsigned int *)(ctxt->fb_addr + offset);

	size = DAL_GetLayerSize() - offset;
	for (i = 0; i < size / sizeof(uint32_t); ++i)
		*addr++ = BG_COLOR;

	ctxt->screen_color = color;

	return DAL_STATUS_OK;
}
EXPORT_SYMBOL(DAL_SetScreenColor);

enum DAL_STATUS DAL_Init(unsigned long layerVA, unsigned long layerPA)
{
	enum MFC_STATUS ret;

	DISPMSG("%s, layerVA=0x%lx, layerPA=0x%lx\n", __func__, layerVA, layerPA);

	dal_fb_addr = (void *)layerVA;
	dal_fb_pa = layerPA;

	ret = MFC_Open(&mfc_handle, dal_fb_addr, DAL_WIDTH, DAL_HEIGHT, DAL_BPP, DAL_FG_COLOR, DAL_BG_COLOR);
	if (MFC_STATUS_OK != ret) {
		DISPMSG("DISP/DAL: Warning: call MFC_XXX function failed in %s(), line: %d, ret: %x\n",
			__func__, __LINE__, ret);
		return DAL_STATUS_NOT_READY;
	}

	DAL_SetScreenColor(DAL_COLOR_RED);

	return DAL_STATUS_OK;
}


enum DAL_STATUS DAL_SetColor(unsigned int fgColor, unsigned int bgColor)
{
	enum MFC_STATUS ret;

	if (NULL == mfc_handle)
		return DAL_STATUS_NOT_READY;

	if (down_interruptible(&dal_sem)) {
		DISPMSG("DISP/DAL " "Can't get semaphore in %s()\n", __func__);
		return DAL_STATUS_LOCK_FAIL;
	}

	dal_fg_color = RGB888_To_RGB565(fgColor);
	dal_bg_color = RGB888_To_RGB565(bgColor);

	ret = MFC_SetColor(mfc_handle, dal_fg_color, dal_bg_color);
	if (MFC_STATUS_OK != ret) {
		DISPMSG("DISP/DAL: Warning: call MFC_XXX function failed in %s(), line: %d, ret: %x\n",
			__func__, __LINE__, ret);
		return DAL_STATUS_INVALID_ARGUMENT;
	}

	up(&dal_sem);

	return DAL_STATUS_OK;
}
EXPORT_SYMBOL(DAL_SetColor);

enum DAL_STATUS DAL_Dynamic_Change_FB_Layer(unsigned int isAEEEnabled)
{
	return DAL_STATUS_OK;
}

enum DAL_STATUS DAL_Clean(void)
{
	enum DAL_STATUS ret = DAL_STATUS_OK;
	enum MFC_STATUS r;

	static int dal_clean_cnt;
	struct MFC_CONTEXT *ctxt = (struct MFC_CONTEXT *) mfc_handle;

	DISPMSG("[DISP] %s\n", __func__);
	if (NULL == mfc_handle)
		return DAL_STATUS_NOT_READY;

	mmprofile_log_ex(ddp_mmp_get_events()->dal_clean, MMPROFILE_FLAG_START, 0,
		       0);

	if (down_interruptible(&dal_sem)) {
		DISPMSG("DISP/DAL " "Can't get semaphore in %s()\n", __func__);
		return DAL_STATUS_LOCK_FAIL;
	}

	r = MFC_ResetCursor(mfc_handle);
	if (MFC_STATUS_OK != r) {
		DISPMSG("DISP/DAL: Warning: call MFC_XXX function failed in %s(), line: %d, ret: %x\n",
			__func__, __LINE__, r);
		return r;
	}

	ctxt->screen_color = 0;
	DAL_SetScreenColor(DAL_COLOR_RED);

	/* TODO: if dal_shown=false, and 3D enabled, mtkfb may disable UI layer, please modify 3D driver */
	if (isAEEEnabled == 1) {
		struct disp_session_input_config session_input;
		struct disp_input_config *input;

		memset((void *)&session_input, 0, sizeof(session_input));

		session_input.setter = SESSION_USER_AEE;
		session_input.config_layer_num = 1;
		input = &session_input.config[0];

		input->src_phy_addr = (void *)dal_fb_pa;
		input->layer_id = primary_display_get_option("ASSERT_LAYER");
		input->layer_enable = 0;
		input->src_offset_x = 0;
		input->src_offset_y = 0;
		input->src_width = DAL_WIDTH;
		input->src_height = DAL_HEIGHT;
		input->tgt_offset_x = 0;
		input->tgt_offset_y = 0;
		input->tgt_width = DAL_WIDTH;
		input->tgt_height = DAL_HEIGHT;
		input->alpha = 0x80;
		input->alpha_enable = 1;
		input->sur_aen = 0;
		input->next_buff_idx = -1;
		input->src_pitch = DAL_WIDTH;
		input->src_fmt = DAL_FORMAT;
		input->next_buff_idx = -1;

		ret = primary_display_config_input_multiple(&session_input);

		/* DAL disable, switch UI layer to default layer 3 */
		DISPMSG("[DDP]* isAEEEnabled from 1 to 0, %d\n",
		       dal_clean_cnt++);
		isAEEEnabled = 0;
		DAL_Dynamic_Change_FB_Layer(isAEEEnabled);	/* restore UI layer to DEFAULT_UI_LAYER */
	}

	dal_shown = false;
	primary_display_trigger(0, NULL, 0);

	up(&dal_sem);
	mmprofile_log_ex(ddp_mmp_get_events()->dal_clean, MMPROFILE_FLAG_END, 0, 0);
	return ret;
}
EXPORT_SYMBOL(DAL_Clean);

int is_DAL_Enabled(void)
{
	int ret = 0;

	if (down_interruptible(&dal_sem)) {
		DISPMSG("DISP/DAL " "Can't get semaphore in %s()\n", __func__);
		return DAL_STATUS_LOCK_FAIL;
	}

	ret = isAEEEnabled;

	up(&dal_sem);

	return ret;
}

unsigned long get_Assert_Layer_PA(void)
{
	return dal_fb_pa;
}

enum DAL_STATUS DAL_Printf(const char *fmt, ...)
{
	va_list args;
	uint i;
	enum DAL_STATUS ret = DAL_STATUS_OK;
	struct disp_session_input_config session_input;
	struct disp_input_config *input;
	enum MFC_STATUS r;

	DISPMSG("[DISP] %s\n", __func__);
	if (NULL == mfc_handle)
		return DAL_STATUS_NOT_READY;

	if (NULL == fmt)
		return DAL_STATUS_INVALID_ARGUMENT;

	mmprofile_log_ex(ddp_mmp_get_events()->dal_printf, MMPROFILE_FLAG_START, 0,
		       0);
	if (down_interruptible(&dal_sem)) {
		DISPMSG("DISP/DAL " "Can't get semaphore in %s()\n",  __func__);
		return DAL_STATUS_LOCK_FAIL;
	}

	if (isAEEEnabled == 0) {
		enum MFC_STATUS r;

		DISPMSG("[DDP] isAEEEnabled from 0 to 1, ASSERT_LAYER=%d, dal_fb_pa %lx\n",
			primary_display_get_option("ASSERT_LAYER"), dal_fb_pa);

		isAEEEnabled = 1;
		DAL_Dynamic_Change_FB_Layer(isAEEEnabled);	/* default_ui_ layer coniig to changed_ui_layer */

		r = MFC_Open(&mfc_handle, dal_fb_addr, DAL_WIDTH, DAL_HEIGHT,
					  DAL_BPP, DAL_FG_COLOR, DAL_BG_COLOR);
		if (MFC_STATUS_OK != r) {
			DISPMSG("DISP/DAL: Warning: call MFC_XXX function failed in %s(), line: %d, ret: %x\n",
				__func__, __LINE__, r);
			return r;
		}

		session_input.setter = SESSION_USER_AEE;
		session_input.config_layer_num = 1;
		input = &session_input.config[0];

		input->src_phy_addr = (void *)dal_fb_pa;
		input->layer_id = primary_display_get_option("ASSERT_LAYER");
		input->layer_enable = 1;
		input->buffer_source = DISP_BUFFER_MVA;
		input->src_offset_x = 0;
		input->src_offset_y = 0;
		input->src_width = DAL_WIDTH;
		input->src_height = DAL_HEIGHT;
		input->tgt_offset_x = 0;
		input->tgt_offset_y = 0;
		input->tgt_width = DAL_WIDTH;
		input->tgt_height = DAL_HEIGHT;
		input->alpha = 0x80;
		input->alpha_enable = 1;
		input->sur_aen = 0;
		input->next_buff_idx = -1;
		input->src_pitch = DAL_WIDTH;
		input->src_fmt = DAL_FORMAT;
		input->next_buff_idx = -1;

		ret = primary_display_config_input_multiple(&session_input);
	}
	va_start(args, fmt);
	i = vsprintf(dal_print_buffer, fmt, args);
	BUG_ON(i >= ARRAY_SIZE(dal_print_buffer));
	va_end(args);

	r = MFC_Print(mfc_handle, dal_print_buffer);
	if (MFC_STATUS_OK != r) {
		DISPMSG("DISP/DAL: Warning: call MFC_XXX function failed in %s(), line: %d, ret: %x\n",
			__func__, __LINE__, r);
		return r;
	}

	//__inner_flush_dcache_all(); 4.19

	if (!dal_shown)
		dal_shown = true;

	ret = primary_display_trigger(0, NULL, 0);

	up(&dal_sem);

	mmprofile_log_ex(ddp_mmp_get_events()->dal_printf, MMPROFILE_FLAG_END, 0, 0);

	return ret;
}
EXPORT_SYMBOL(DAL_Printf);


enum DAL_STATUS DAL_OnDispPowerOn(void)
{
	return DAL_STATUS_OK;
}

/* ########################################################################## */
/* !CONFIG_MTK_FB_SUPPORT_ASSERTION_LAYER */
/* ########################################################################## */
#else

unsigned int isAEEEnabled = 0;

uint32_t DAL_GetLayerSize(void)
{
	/* avoid lcdc read buffersize+1 issue */
	return DAL_WIDTH * DAL_HEIGHT * DAL_BPP + 4096;
}

enum DAL_STATUS DAL_Init(uint32_t layerVA, uint32_t layerPA)
{
	NOT_REFERENCED(layerVA);
	NOT_REFERENCED(layerPA);

	return DAL_STATUS_OK;
}

enum DAL_STATUS DAL_SetColor(unsigned int fgColor, unsigned int bgColor)
{
	NOT_REFERENCED(fgColor);
	NOT_REFERENCED(bgColor);

	return DAL_STATUS_OK;
}
EXPORT_SYMBOL(DAL_SetColor);

enum DAL_STATUS DAL_Clean(void)
{
	DISPMSG("[MTKFB_DAL] DAL_Clean is not implemented\n");
	return DAL_STATUS_OK;
}
EXPORT_SYMBOL(DAL_Clean);

enum DAL_STATUS DAL_Printf(const char *fmt, ...)
{
	NOT_REFERENCED(fmt);
	DISPMSG("[MTKFB_DAL] DAL_Printf is not implemented\n");
	return DAL_STATUS_OK;
}
EXPORT_SYMBOL(DAL_Printf);

enum DAL_STATUS DAL_OnDispPowerOn(void)
{
	return DAL_STATUS_OK;
}

enum DAL_STATUS DAL_SetScreenColor(enum DAL_COLOR color)
{
	return DAL_STATUS_OK;
}
EXPORT_SYMBOL(DAL_SetScreenColor);
#endif	/* CONFIG_MTK_FB_SUPPORT_ASSERTION_LAYER */

