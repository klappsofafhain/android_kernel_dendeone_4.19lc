// SPDX-License-Identifier: GPL-2.0
//
// dsp-platform-mem-control.c--  Mediatek ADSP dmemory control
//
// Copyright (c) 2018 MediaTek Inc.
// Author: Chipeng <Chipeng.chang@mediatek.com>

#include <linux/genalloc.h>
#include <linux/string.h>
#include <linux/types.h>
#include <sound/pcm.h>

/* adsp or scp*/
#ifdef CONFIG_MTK_AUDIODSP_SUPPORT
#include <adsp_helper.h>
#else
#include <scp_helper.h>
#endif

#include "dsp-platform-mem-control.h"
#include "../mtk-dsp-mem-control.h"
#include "mt6779-afe-common.h"
#include "../mtk-base-dsp.h"
#include "../mtk-dsp-common.h"

/*
 * todo: let user space decide this
 * mapping dl ==> task
 */
static struct mtk_adsp_task_attr adsp_task_attr[AUDIO_TASK_DAI_NUM] = {
	[AUDIO_TASK_VOIP_ID] = {false, MT6779_MEMIF_DL12, -1, -1,
				VOIP_FEATURE_ID, false},
	[AUDIO_TASK_PRIMARY_ID] = {false, MT6779_MEMIF_DL1, -1, -1,
				   PRIMARY_FEATURE_ID, false},
	[AUDIO_TASK_OFFLOAD_ID] = {false, MT6779_MEMIF_DL3, -1, -1,
				   OFFLOAD_FEATURE_ID, false},
	[AUDIO_TASK_DEEPBUFFER_ID] = {false, -1, -1, -1,
				      DEEPBUF_FEATURE_ID, false},
	[AUDIO_TASK_PLAYBACK_ID] = {false, MT6779_MEMIF_DL4,
				    MT6779_MEMIF_VUL4,
				    MT6779_MEMIF_AWB2,
				    AUDIO_PLAYBACK_FEATURE_ID, false},
	[AUDIO_TASK_CAPTURE_UL1_ID] = {false, -1,
				       MT6779_MEMIF_VUL12,
				       MT6779_MEMIF_AWB2,
				       CAPTURE_UL1_FEATURE_ID, false},
	[AUDIO_TASK_A2DP_ID] = {false, -1,
				       -1,
				       -1,
				       A2DP_PLAYBACK_FEATURE_ID, false},
	[AUDIO_TASK_DATAPROVIDER_ID] = {false, -1,
				       MT6779_MEMIF_VUL4,
				       -1,
				       AUDIO_DATAPROVIDER_FEATURE_ID, false},
	[AUDIO_TASK_CALL_FINAL_ID] = {false, MT6779_MEMIF_DL4,
				      MT6779_MEMIF_VUL4,
				      MT6779_MEMIF_AWB2,
				      CALL_FINAL_FEATURE_ID, false},
	[AUDIO_TASK_KTV_ID] = {true, MT6779_MEMIF_DL8,
				      MT6779_MEMIF_VUL6,
				      -1,
				      KTV_FEATURE_ID, false},
};

/* task share mem block */
static struct audio_dsp_dram
	adsp_sharemem_primary_mblock[ADSP_TASK_SHAREMEM_NUM] = {
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
};

static struct audio_dsp_dram
	adsp_sharemem_playback_mblock[ADSP_TASK_SHAREMEM_NUM] = {
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
};
#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
static struct audio_dsp_dram
	adsp_sharemem_offload_mblock[ADSP_TASK_SHAREMEM_NUM] = {
		{
			.size = 0x400, // 1024 bytes
			.phy_addr = 0,
		},
		{
			.size = 0x400, // 1024 bytes
			.phy_addr = 0,
		},
};
#endif
static struct audio_dsp_dram
	adsp_sharemem_deepbuffer_mblock[ADSP_TASK_SHAREMEM_NUM] = {
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
};
static struct audio_dsp_dram
	adsp_sharemem_voip_mblock[ADSP_TASK_SHAREMEM_NUM] = {
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
};
static struct audio_dsp_dram
	adsp_sharemem_capture_ul1_mblock[ADSP_TASK_SHAREMEM_NUM] = {
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
};

static struct audio_dsp_dram
	adsp_sharemem_a2dp_mblock[ADSP_TASK_SHAREMEM_NUM] = {
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
};

static struct audio_dsp_dram
	adsp_sharemem_dataprovider_mblock[ADSP_TASK_SHAREMEM_NUM] = {
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
};

static struct audio_dsp_dram
	adsp_sharemem_call_final_mblock[ADSP_TASK_SHAREMEM_NUM] = {
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
};
static struct audio_dsp_dram
	adsp_sharemem_ktv_mblock[ADSP_TASK_SHAREMEM_NUM] = {
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
};

struct audio_dsp_dram *mtk_get_adsp_sharemem_block(int audio_task_id)
{
	switch (audio_task_id) {
	case AUDIO_TASK_VOIP_ID:
		return adsp_sharemem_voip_mblock;
	case AUDIO_TASK_PRIMARY_ID:
		return adsp_sharemem_primary_mblock;
#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
	case AUDIO_TASK_OFFLOAD_ID:
		return adsp_sharemem_offload_mblock;
#endif
	case AUDIO_TASK_DEEPBUFFER_ID:
		return adsp_sharemem_deepbuffer_mblock;
	case AUDIO_TASK_PLAYBACK_ID:
		return adsp_sharemem_playback_mblock;
	case AUDIO_TASK_CAPTURE_UL1_ID:
		return adsp_sharemem_capture_ul1_mblock;
	case AUDIO_TASK_A2DP_ID:
		return adsp_sharemem_a2dp_mblock;
	case AUDIO_TASK_DATAPROVIDER_ID:
		return adsp_sharemem_dataprovider_mblock;
	case AUDIO_TASK_CALL_FINAL_ID:
		return adsp_sharemem_call_final_mblock;
	case AUDIO_TASK_KTV_ID:
		return adsp_sharemem_ktv_mblock;
	default:
		pr_err("%s err audio_task_id = %d\n", __func__, audio_task_id);
	}

	return NULL;
}

struct mtk_adsp_task_attr *mtk_get_adsp_task_attr(int adsp_id)
{
	if (adsp_id >= AUDIO_TASK_DAI_NUM)
		pr_info("%s adsp_id = %d\n", __func__, adsp_id);

	switch (adsp_id) {
	case AUDIO_TASK_VOIP_ID:
		return &adsp_task_attr[AUDIO_TASK_VOIP_ID];
	case AUDIO_TASK_PRIMARY_ID:
		return &adsp_task_attr[AUDIO_TASK_PRIMARY_ID];
	case AUDIO_TASK_OFFLOAD_ID:
		return &adsp_task_attr[AUDIO_TASK_OFFLOAD_ID];
	case AUDIO_TASK_DEEPBUFFER_ID:
		return &adsp_task_attr[AUDIO_TASK_DEEPBUFFER_ID];
	case AUDIO_TASK_PLAYBACK_ID:
		return &adsp_task_attr[AUDIO_TASK_PLAYBACK_ID];
	case AUDIO_TASK_CAPTURE_UL1_ID:
		return &adsp_task_attr[AUDIO_TASK_CAPTURE_UL1_ID];
	case AUDIO_TASK_A2DP_ID:
		return &adsp_task_attr[AUDIO_TASK_A2DP_ID];
	case AUDIO_TASK_DATAPROVIDER_ID:
		return &adsp_task_attr[AUDIO_TASK_DATAPROVIDER_ID];
	case AUDIO_TASK_CALL_FINAL_ID:
		return &adsp_task_attr[AUDIO_TASK_CALL_FINAL_ID];
	case AUDIO_TASK_KTV_ID:
		return &adsp_task_attr[AUDIO_TASK_KTV_ID];
	default:
		break;
	}

	return NULL;
}

/* dai id support dsp <==> afe */
bool mtk_adsp_dai_id_support_share_mem(int dai_id)
{
	switch (dai_id) {
	case MT6779_MEMIF_DL1:
	case MT6779_MEMIF_DL12:
	case MT6779_MEMIF_DL3:
	case MT6779_MEMIF_DL4:
	case MT6779_MEMIF_DL5:
	case MT6779_MEMIF_DL6:
	case MT6779_MEMIF_DL7:
	case MT6779_MEMIF_DL8:
	case MT6779_MEMIF_VUL12:
	case MT6779_MEMIF_VUL2:
	case MT6779_MEMIF_VUL3:
	case MT6779_MEMIF_VUL4:
	case MT6779_MEMIF_VUL5:
	case MT6779_MEMIF_VUL6:
	case MT6779_MEMIF_AWB:
	case MT6779_MEMIF_AWB2:
	case MT6779_MEMIF_NUM:
		return true;
	case MT6779_MEMIF_DL2:
	case MT6779_MEMIF_DAI:
	case MT6779_MEMIF_DAI2:
	case MT6779_MEMIF_MOD_DAI:
	case MT6779_MEMIF_HDMI:
		return false;
	default:
		return false;
	}
}

