// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Shane Chien <shane.chien@mediatek.com>
 */

/*******************************************************************************
 *
 * Filename:
 * ---------
 *   mt_soc_voice.c
 *
 * Project:
 * --------
 *   voice call platform driver
 *
 * Description:
 * ------------
 *
 *
 * Author:
 * -------
 * Chipeng Chang
 *
 *------------------------------------------------------------------------------
 *
 *
 *******************************************************************************/


/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include <linux/dma-mapping.h>
#include "AudDrv_Common.h"
#include "AudDrv_Def.h"
#include "AudDrv_Afe.h"
#include "AudDrv_Ana.h"
#include "AudDrv_Clk.h"
#include "AudDrv_Kernel.h"
#include "mt_soc_afe_control.h"
#include "mt_soc_digital_type.h"
#include "mt_soc_pcm_common.h"
#include "AudDrv_Common_func.h"

/*
 *    declaration
 */

struct mtk_voice_property {
	/* speech mixctrl instead property usage */
	int speech_a2m_msg_id;
	int speech_md_status;
	int speech_md_ext_status;
	int speech_mic_mute;
	int speech_dl_mute;
	int speech_ul_mute;
	int speech_phone1_md_idx;
	int speech_phone2_md_idx;
	int speech_phone_id;
	int speech_md_epof;
	int speech_bt_sco_wb;
	int speech_md_active;
};

/*
 *    function implementation
 */

static int mtk_voice_probe(struct platform_device *pdev);
static int mtk_voice_close(struct snd_pcm_substream *substream);
static int mtk_voice_platform_probe(struct snd_soc_component *component);

static bool Voice_Status;
static struct AudioDigtalI2S mAudioDigitalI2S;

bool get_voice_status(void)
{
	return Voice_Status;
}
EXPORT_SYMBOL(get_voice_status);

static struct AudioDigitalPCM Voice1Pcm = {
	.mTxLchRepeatSel = Soc_Aud_TX_LCH_RPT_TX_LCH_NO_REPEAT,
	.mVbt16kModeSel = Soc_Aud_VBT_16K_MODE_VBT_16K_MODE_DISABLE,
	.mExtModemSel = Soc_Aud_EXT_MODEM_MODEM_2_USE_INTERNAL_MODEM,
	.mExtendBckSyncLength = 0,
	.mExtendBckSyncTypeSel = Soc_Aud_PCM_SYNC_TYPE_BCK_CYCLE_SYNC,
	.mSingelMicSel = Soc_Aud_BT_MODE_DUAL_MIC_ON_TX,
	.mAsyncFifoSel = Soc_Aud_BYPASS_SRC_SLAVE_USE_ASRC,
	.mSlaveModeSel = Soc_Aud_PCM_CLOCK_SOURCE_SALVE_MODE,
	.mPcmWordLength = Soc_Aud_PCM_WLEN_LEN_PCM_16BIT,
	.mPcmModeWidebandSel = false,
	.mPcmFormat = Soc_Aud_PCM_FMT_PCM_MODE_B,
	.mModemPcmOn = false,
};

static struct snd_pcm_hw_constraint_list constraints_sample_rates = {
	.count = ARRAY_SIZE(soc_voice_supported_sample_rates),
	.list = soc_voice_supported_sample_rates,
	.mask = 0,
};

static struct snd_pcm_hardware mtk_pcm_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP |
		 SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = SND_SOC_STD_MT_FMTS,
	.rates = SOC_NORMAL_USE_RATE,
	.rate_min = SOC_NORMAL_USE_RATE_MIN,
	.rate_max = SOC_NORMAL_USE_RATE_MAX,
	.channels_min = SOC_NORMAL_USE_CHANNELS_MIN,
	.channels_max = SOC_NORMAL_USE_CHANNELS_MAX,
	.buffer_bytes_max = MAX_BUFFER_SIZE,
	.period_bytes_max = MAX_PERIOD_SIZE,
	.periods_min = 1,
	.periods_max = 4096,
	.fifo_size = 0,
};

static struct mtk_voice_property voice_property = {
	.speech_a2m_msg_id = 0,
	.speech_md_status = 0,
	.speech_md_ext_status = 0,
	.speech_mic_mute = 0,
	.speech_dl_mute = 0,
	.speech_ul_mute = 0,
	.speech_phone1_md_idx = 0,
	.speech_phone2_md_idx = 0,
	.speech_phone_id = 0,
	.speech_md_epof = 0,
	.speech_bt_sco_wb = 0,
	.speech_md_active = 0,
};

/* speech mixctrl instead property usage */
static void *get_sph_property_by_name(struct mtk_voice_property *voice_priv,
				      const char *name)
{
	if (strcmp(name, "Speech_A2M_Msg_ID") == 0)
		return &(voice_priv->speech_a2m_msg_id);
	else if (strcmp(name, "Speech_MD_Status") == 0)
		return &(voice_priv->speech_md_status);
	else if (strcmp(name, "Speech_MD_Ext_Status") == 0)
		return &(voice_priv->speech_md_ext_status);
	else if (strcmp(name, "Speech_Mic_Mute") == 0)
		return &(voice_priv->speech_mic_mute);
	else if (strcmp(name, "Speech_DL_Mute") == 0)
		return &(voice_priv->speech_dl_mute);
	else if (strcmp(name, "Speech_UL_Mute") == 0)
		return &(voice_priv->speech_ul_mute);
	else if (strcmp(name, "Speech_Phone1_MD_Idx") == 0)
		return &(voice_priv->speech_phone1_md_idx);
	else if (strcmp(name, "Speech_Phone2_MD_Idx") == 0)
		return &(voice_priv->speech_phone2_md_idx);
	else if (strcmp(name, "Speech_Phone_ID") == 0)
		return &(voice_priv->speech_phone_id);
	else if (strcmp(name, "Speech_MD_EPOF") == 0)
		return &(voice_priv->speech_md_epof);
	else if (strcmp(name, "Speech_BT_SCO_WB") == 0)
		return &(voice_priv->speech_bt_sco_wb);
	else if (strcmp(name, "Speech_MD_USAGE") == 0)
		return &(voice_priv->speech_md_active);
	else
		return NULL;
}

static int speech_property_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	int *sph_property;

	sph_property = (int *)get_sph_property_by_name(&voice_property,
						       kcontrol->id.name);
	if (!sph_property) {
		pr_warn("%s(), sph_property == NULL\n", __func__);
		return -EINVAL;
	}
	ucontrol->value.integer.value[0] = *sph_property;

	pr_info("%s(), %s = 0x%x\n", __func__,
		 kcontrol->id.name, *sph_property);
	return 0;
}

static int speech_property_set(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	int *sph_property;

	sph_property = (int *)get_sph_property_by_name(&voice_property,
						       kcontrol->id.name);
	if (!sph_property) {
		pr_warn("%s(), sph_property == NULL\n", __func__);
		return -EINVAL;
	}
	*sph_property = ucontrol->value.integer.value[0];

	pr_info("%s(), %s = 0x%x\n", __func__,
		 kcontrol->id.name, *sph_property);
	return 0;
}

int is_speech_active(void)
{
	int *sph_property;

	sph_property = (int *)get_sph_property_by_name(&voice_property,
						       "Speech_MD_USAGE");
	if (!sph_property)
		return 0;

	return *sph_property;
}

static const struct snd_kcontrol_new mtk_voice_speech_controls[] = {
	SOC_SINGLE_EXT("Speech_A2M_Msg_ID",
		       SND_SOC_NOPM, 0, 0xFFFF, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_MD_Status",
		       SND_SOC_NOPM, 0, 0xFFFFFFFF, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_MD_Ext_Status",
		       SND_SOC_NOPM, 0, 0xFFFFFFFF, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_Mic_Mute",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_DL_Mute",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_UL_Mute",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_Phone1_MD_Idx",
		       SND_SOC_NOPM, 0, 0x2, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_Phone2_MD_Idx",
		       SND_SOC_NOPM, 0, 0x2, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_Phone_ID",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_MD_EPOF",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_BT_SCO_WB",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_MD_USAGE",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       speech_property_get, speech_property_set),
};

static int mtk_voice_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret = 0;

	AudDrv_Clk_On();
	AudDrv_ADC_Clk_On();

	runtime->hw = mtk_pcm_hardware;
	memcpy((void *)(&(runtime->hw)), (void *)&mtk_pcm_hardware,
	       sizeof(struct snd_pcm_hardware));

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &constraints_sample_rates);
	ret = snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);

	if (ret < 0)
		pr_warn("snd_pcm_hw_constraint_integer failed\n");

	/* print for hw pcm information */
	pr_warn("mtk_voice_pcm_open runtime rate = %d channels = %d\n", runtime->rate,
	       runtime->channels);

	runtime->hw.info |= SNDRV_PCM_INFO_INTERLEAVED;
	runtime->hw.info |= SNDRV_PCM_INFO_NONINTERLEAVED;
	runtime->rate = 16000;

	if (ret < 0) {
		pr_warn("mtk_voice_close\n");
		mtk_voice_close(substream);
		return ret;
	}
	return 0;
}

static void ConfigAdcI2S(struct snd_pcm_substream *substream)
{
	mAudioDigitalI2S.mLR_SWAP = Soc_Aud_LR_SWAP_NO_SWAP;
	mAudioDigitalI2S.mBuffer_Update_word = 8;
	mAudioDigitalI2S.mFpga_bit_test = 0;
	mAudioDigitalI2S.mFpga_bit = 0;
	mAudioDigitalI2S.mloopback = 0;
	mAudioDigitalI2S.mINV_LRCK = Soc_Aud_INV_LRCK_NO_INVERSE;
	mAudioDigitalI2S.mI2S_FMT = Soc_Aud_I2S_FORMAT_I2S;
	mAudioDigitalI2S.mI2S_WLEN = Soc_Aud_I2S_WLEN_WLEN_16BITS;
	mAudioDigitalI2S.mI2S_SAMPLERATE = (substream->runtime->rate);
}

static int mtk_voice_close(struct snd_pcm_substream *substream)
{
	pr_debug("%s(), substream->stream = %d\n", __func__, substream->stream);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		AudDrv_ADC_Clk_Off();
		AudDrv_Clk_Off();
		return 0;
	}
	/* todo : enable sidetone */
	/* here start digital part */
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I03,
		      Soc_Aud_InterConnectionOutput_O17);
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I04,
		      Soc_Aud_InterConnectionOutput_O18);
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I14,
		      Soc_Aud_InterConnectionOutput_O03);
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I14,
		      Soc_Aud_InterConnectionOutput_O04);

	SetI2SAdcEnable(false);
	SetI2SDacEnable(false);
	SetModemPcmEnable(MODEM_1, false);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, false);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_IN_ADC, false);

	EnableAfe(false);

	AudDrv_ADC_Clk_Off();
	AudDrv_Clk_Off();

	Voice_Status = false;

	return 0;
}

static int mtk_voice_trigger(struct snd_pcm_substream *substream, int cmd)
{
	pr_warn("mtk_voice_trigger cmd = %d\n", cmd);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		break;
	}
	return 0;
}

static int mtk_voice_pcm_copy(struct snd_pcm_substream *substream,
			      int channel, unsigned long pos,
			      void __user *dst, unsigned long count)
{
	return 0;
}

static int mtk_voice_pcm_silence(struct snd_pcm_substream *substream,
				 int channel, snd_pcm_uframes_t pos, snd_pcm_uframes_t count)
{
	pr_warn("mtk_voice_pcm_silence\n");
	return 0;		/* do nothing */
}

static void *dummy_page[2];
static struct page *mtk_pcm_page(struct snd_pcm_substream *substream, unsigned long offset)
{
	return virt_to_page(dummy_page[substream->stream]);	/* the same page */
}

static int mtk_voice1_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtimeStream = substream->runtime;

	pr_warn("%s, rate = %d  channels = %d period_size = %lu, substream->stream = %d\n",
		__func__, runtimeStream->rate, runtimeStream->channels,
		runtimeStream->period_size, substream->stream);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		return 0;
	}
	/* here start digital part */
	SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I03,
		      Soc_Aud_InterConnectionOutput_O17);
	SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I04,
		      Soc_Aud_InterConnectionOutput_O18);
	SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I14,
		      Soc_Aud_InterConnectionOutput_O03);
	SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I14,
		      Soc_Aud_InterConnectionOutput_O04);

	/* start I2S DAC out */
	SetI2SDacOut(substream->runtime->rate, false, Soc_Aud_I2S_WLEN_WLEN_16BITS);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, true);

	ConfigAdcI2S(substream);
	SetI2SAdcIn(&mAudioDigitalI2S);
	SetI2SDacEnable(true);

#ifdef PMIC_RESET_WORKAROUND
	pr_warn("%s ,PMIC_RESET_WORKAROUND reset ABB_AFE_CON2/ABB_AFE_CON4\n", __func__);
	Ana_Set_Reg(ABB_AFE_CON2, 0x0, 0xffff);	/* reset to default value */
	Ana_Set_Reg(ABB_AFE_CON4, 0x0, 0xffff);	/* reset to default value */
#endif

	SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_IN_ADC, true);
	SetI2SAdcEnable(true);
	EnableAfe(true);
	Voice1Pcm.mPcmModeWidebandSel = (runtimeStream->rate == 8000) ?
	    Soc_Aud_PCM_MODE_PCM_MODE_8K : Soc_Aud_PCM_MODE_PCM_MODE_16K;
	Voice1Pcm.mAsyncFifoSel = Soc_Aud_BYPASS_SRC_SLAVE_USE_ASYNC_FIFO;
	SetModemPcmConfig(MODEM_1, Voice1Pcm);
	SetModemPcmEnable(MODEM_1, true);

	Voice_Status = true;

	return 0;
}

static int mtk_pcm_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *hw_params)
{
	int ret = 0;

	return ret;
}

static int mtk_voice_hw_free(struct snd_pcm_substream *substream)
{
	PRINTK_AUDDRV("mtk_voice_hw_free\n");
	return snd_pcm_lib_free_pages(substream);
}

static struct snd_pcm_ops mtk_voice_ops = {
	.open = mtk_voice_pcm_open,
	.close = mtk_voice_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mtk_pcm_hw_params,
	.hw_free = mtk_voice_hw_free,
	.prepare = mtk_voice1_prepare,
	.trigger = mtk_voice_trigger,
	.copy_user = mtk_voice_pcm_copy,
	.fill_silence = mtk_voice_pcm_silence,
	.page = mtk_pcm_page,
};

static struct snd_soc_component_driver mtk_soc_voice_platform = {
	.ops = &mtk_voice_ops,
	.probe = mtk_voice_platform_probe,
};

static int mtk_voice_probe(struct platform_device *pdev)
{
	pr_info("mtk_voice_probe\n");

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);

	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;


	if (pdev->dev.of_node) {
		dev_set_name(&pdev->dev, "%s", MT_SOC_VOICE_MD1);
		pdev->name = pdev->dev.kobj.name;
	}

	pr_warn("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_component(&pdev->dev,
					  &mtk_soc_voice_platform, NULL, 0);
}

static int mtk_voice_platform_probe(struct snd_soc_component *component)
{
	pr_info("mtk_voice_platform_probe\n");

	snd_soc_add_component_controls(component, mtk_voice_speech_controls,
				       ARRAY_SIZE(mtk_voice_speech_controls));

	return 0;
}

static int mtk_voice_remove(struct platform_device *pdev)
{
	pr_info("%s\n", __func__);
	snd_soc_unregister_component(&pdev->dev);
	return 0;
}


/* supend and resume function */
static int mtk_voice_pm_ops_suspend(struct device *device)
{
	/* if now in phone call state, not suspend!! */
	bool b_modem1_speech_on;
	bool b_modem2_speech_on;

	AudDrv_Clk_On();	/* should enable clk for access reg */
	b_modem1_speech_on = (bool) (Afe_Get_Reg(PCM2_INTF_CON) & 0x1);
	b_modem2_speech_on = (bool) (Afe_Get_Reg(PCM_INTF_CON) & 0x1);
	AudDrv_Clk_Off();	/* should enable clk for access reg */
	pr_debug("%s, b_modem1_speech_on=%d, b_modem2_speech_on=%d, speech_md_active=%d\n",
		 __func__, b_modem1_speech_on, b_modem2_speech_on,
		 voice_property.speech_md_active);
	if (b_modem1_speech_on == true || b_modem2_speech_on == true
	    || voice_property.speech_md_active == true) {
#ifndef CONFIG_FPGA_EARLY_PORTING
		/* select 26M */
		clkmux_sel(MT_CLKMUX_AUD_HF_26M_SEL, MT_CG_SYS_26M, "AUDIO ");
		clkmux_sel(MT_CLKMUX_AUD_INTBUS_SEL, MT_CG_SYS_26M, "AUDIO ");
		return 0;
#endif
	}
	return 0;
}

static int mtk_voice_pm_ops_resume(struct device *device)
{
	bool b_modem1_speech_on;
	bool b_modem2_speech_on;

	AudDrv_Clk_On();	/* should enable clk for access reg */
	b_modem1_speech_on = (bool) (Afe_Get_Reg(PCM2_INTF_CON) & 0x1);
	b_modem2_speech_on = (bool) (Afe_Get_Reg(PCM_INTF_CON) & 0x1);
	AudDrv_Clk_Off();
	if (b_modem1_speech_on == true || b_modem2_speech_on == true) {
#ifndef CONFIG_FPGA_EARLY_PORTING
		/* mainpll */
		clkmux_sel(MT_CLKMUX_AUD_HF_26M_SEL, MT_CG_SYS_TEMP, "AUDIO ");
		clkmux_sel(MT_CLKMUX_AUD_INTBUS_SEL, MT_CG_MPLL_D12, "AUDIO ");
		return 0;
#endif
	}

	return 0;
}

const struct dev_pm_ops mtk_voice_pm_ops = {
	.suspend = mtk_voice_pm_ops_suspend,
	.resume = mtk_voice_pm_ops_resume,
	.freeze = NULL,
	.thaw = NULL,
	.poweroff = NULL,
	.restore = NULL,
	.restore_noirq = NULL,
};

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_pcm_voice_md1_of_ids[] = {
	{.compatible = "mediatek,mt_soc_pcm_voice_md1",},
	{}
};
#endif

static struct platform_driver mtk_voice_driver = {
	.driver = {
		   .name = MT_SOC_VOICE_MD1,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = mt_soc_pcm_voice_md1_of_ids,
#endif
#ifdef CONFIG_PM
		   .pm = &mtk_voice_pm_ops,
#endif
		   },
	.probe = mtk_voice_probe,
	.remove = mtk_voice_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtk_voice_dev;
#endif

static int __init mtk_soc_voice_platform_init(void)
{
	int ret = 0;

	pr_warn("%s\n", __func__);
#ifndef CONFIG_OF
	soc_mtk_voice_dev = platform_device_alloc(MT_SOC_VOICE_MD1, -1);
	if (!soc_mtk_voice_dev)
		return -ENOMEM;

	ret = platform_device_add(soc_mtk_voice_dev);
	if (ret != 0) {
		platform_device_put(soc_mtk_voice_dev);
		return ret;
	}
#endif
	ret = platform_driver_register(&mtk_voice_driver);

	return ret;

}
module_init(mtk_soc_voice_platform_init);

static void __exit mtk_soc_voice_platform_exit(void)
{

	pr_warn("%s\n", __func__);
	platform_driver_unregister(&mtk_voice_driver);
}
module_exit(mtk_soc_voice_platform_exit);

MODULE_DESCRIPTION("AFE PCM module platform driver");
MODULE_LICENSE("GPL");
