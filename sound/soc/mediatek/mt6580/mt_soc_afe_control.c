// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Shane Chien <shane.chien@mediatek.com>
 */

/*******************************************************************************
 *
 * Filename:
 * ---------
 *  mt_sco_afe_control.c
 *
 * Project:
 * --------
 *   MT6583  Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio register
 *
 * Author:
 * -------
 * Chipeng Chang
 *
 *------------------------------------------------------------------------------
 *
 *******************************************************************************/


/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include "AudDrv_Common.h"
#include "AudDrv_Def.h"
#include "AudDrv_Afe.h"
#include "AudDrv_Ana.h"
#include "AudDrv_Clk.h"
#include "mt_soc_digital_type.h"
#include "AudDrv_Def.h"
#include "AudDrv_Kernel.h"
#include "mt_soc_afe_control.h"
#include "mt_soc_afe_connection.h"
#include "mt_soc_pcm_common.h"
#include "AudDrv_Common_func.h"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/completion.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/jiffies.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <asm/irq.h>
#include <linux/io.h>
#include <asm/div64.h>
#include <mt-plat/upmu_common.h>
#include <mt-plat/aee.h>

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/jack.h>
/* #include <asm/mach-types.h> */
#if !defined(CONFIG_MTK_LEGACY)
#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>
#else
#include <mt-plat/mt_gpio.h>
#endif

#include <mt-plat/mt_boot.h>
#include <mt-plat/mt_boot_common.h>

#include "AudDrv_Common.h"
#include "AudDrv_Common_func.h"
#include "AudDrv_Gpio.h"

static DEFINE_SPINLOCK(afe_control_lock);
static DEFINE_SPINLOCK(afe_sram_control_lock);

static DEFINE_SPINLOCK(afe_dl_abnormal_context_lock);

/* static  variable */
static bool AudioDaiBtStatus;
static bool AudioAdcI2SStatus;
static bool Audio2ndAdcI2SStatus;
static bool AudioMrgStatus;
static bool mAudioInit;
static bool mVOWStatus;
/* static unsigned int MCLKFS = 128; */
static struct AudioDigtalI2S *AudioAdcI2S;
static struct AudioDigtalI2S *m2ndI2S;	/* input */
static struct AudioDigtalI2S *m2ndI2Sout;	/* output */
static bool mFMEnable;
static bool mOffloadEnable;

static struct AudioHdmi *mHDMIOutput;
static struct AudioMrgIf *mAudioMrg;
static struct AudioDigitalDAIBT *AudioDaiBt;

static struct AFE_MEM_CONTROL_T *AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_HDMI + 1] = { NULL };
static struct snd_dma_buffer *Audio_dma_buf[Soc_Aud_Digital_Block_MEM_HDMI + 1] = { NULL };

static struct AudioMemIFAttribute *mAudioMEMIF[Soc_Aud_Digital_Block_NUM_OF_DIGITAL_BLOCK] = { NULL };

struct AFE_DL_ABNORMAL_CONTROL_T AFE_dL_Abnormal_context;

static struct AudioAfeRegCache mAudioRegCache;
static struct AudioSramManager mAudioSramManager;
const unsigned int AudioSramPlaybackFullSize = 1024 * 16;	/* only has 16K */

const unsigned int AudioSramPlaybackPartialSize = 1024 * 32;
const unsigned int AudioDramPlaybackSize = 1024 * 16;	/* use 16K */

const size_t AudioSramCaptureSize = 1024 * 16;
const size_t AudioDramCaptureSize = 1024 * 48;
const size_t AudioInterruptLimiter = 100;
static int irqcount;

static bool mExternalModemStatus;

static struct irq_manager irq_managers[Soc_Aud_IRQ_MCU_MODE_NUM_OF_IRQ_MODE];

#define IrqShortCounter  512

/* mutex lock */
static DEFINE_MUTEX(afe_control_mutex);
static DEFINE_SPINLOCK(auddrv_dl1_lock);
static DEFINE_SPINLOCK(auddrv_dl2_lock);
static DEFINE_SPINLOCK(auddrv_ul1_lock);


static const uint16_t kSideToneCoefficientTable16k[] = {
	0x049C, 0x09E8, 0x09E0, 0x089C,
	0xFF54, 0xF488, 0xEAFC, 0xEBAC,
	0xfA40, 0x17AC, 0x3D1C, 0x6028,
	0x7538
};

static const uint16_t kSideToneCoefficientTable32k[] = {
	0xff58, 0x0063, 0x0086, 0x00bf,
	0x0100, 0x013d, 0x0169, 0x0178,
	0x0160, 0x011c, 0x00aa, 0x0011,
	0xff5d, 0xfea1, 0xfdf6, 0xfd75,
	0xfd39, 0xfd5a, 0xfde8, 0xfeea,
	0x005f, 0x0237, 0x0458, 0x069f,
	0x08e2, 0x0af7, 0x0cb2, 0x0df0,
	0x0e96
};

static unsigned int LowLatencyDebug;

/*
 *    function implementation
 */
static irqreturn_t AudDrv_IRQ_handler(int irq, void *dev_id);
static bool SetIrqEnable(unsigned int Irqmode, bool bEnable);

static bool CheckSize(unsigned int size)
{
	if (size == 0) {
		pr_debug("CheckSize size = 0\n");
		return true;
	}
	return false;
}

static void AfeGlobalVarInit(void)
{
	AudioDaiBtStatus = false;
	AudioAdcI2SStatus = false;
	Audio2ndAdcI2SStatus = false;
	AudioMrgStatus = false;
	mAudioInit = false;
	mVOWStatus = false;
	AudioAdcI2S = NULL;
	m2ndI2S = NULL;		/* input */
	m2ndI2Sout = NULL;	/* output */
	mFMEnable = false;
	mOffloadEnable = false;
	mHDMIOutput = NULL;
	mAudioMrg = NULL;
	AudioDaiBt = NULL;
	mExternalModemStatus = false;
	irqcount = 0;
}

void AfeControlMutexLock(void)
{
	mutex_lock(&afe_control_mutex);
}

void AfeControlMutexUnLock(void)
{
	mutex_unlock(&afe_control_mutex);
}

void AfeControlSramLock(void)
{
	spin_lock(&afe_sram_control_lock);
}

void AfeControlSramUnLock(void)
{
	spin_unlock(&afe_sram_control_lock);
}


unsigned int GetSramState(void)
{
	return mAudioSramManager.mMemoryState;
}

void SetSramState(unsigned int State)
{
	/*pr_debug("%s state= %d\n", __func__, State);*/
	mAudioSramManager.mMemoryState |= State;
}

void ClearSramState(unsigned int State)
{
	/*pr_debug("%s state= %d\n", __func__, State);*/
	mAudioSramManager.mMemoryState &= (~State);
}

unsigned int GetPLaybackSramFullSize(void)
{
	return AudioSramPlaybackFullSize;
}

unsigned int GetPLaybackSramPartial(void)
{
	return AudioSramPlaybackPartialSize;
}

unsigned int GetPLaybackDramSize(void)
{
	return AudioDramPlaybackSize;
}


size_t GetCaptureSramSize(void)
{
	return AudioSramCaptureSize;
}

size_t GetCaptureDramSize(void)
{
	return AudioDramCaptureSize;
}

void SetOffloadEnableFlag(bool bEnable)
{
	mOffloadEnable = bEnable;
}

void SetFMEnableFlag(bool bEnable)
{
	mFMEnable = bEnable;
}

bool ConditionEnterSuspend(void)
{
	if ((mFMEnable == true) || (mOffloadEnable == true))
		return false;

	return true;
}

/* function get internal mode status. */
bool get_internalmd_status(void)
{
	bool ret = (get_voice_bt_status() || get_voice_status());

	return (mExternalModemStatus == true) ? false : ret;
}

#if 0
void DumpMemifSubStream(void)
{
	int i = 0;
	/* printk("+%s\n ", __func__); */
	for (i = 0; i <= Soc_Aud_Digital_Block_MEM_HDMI; i++) {
		struct substreamList *head = AFE_Mem_Control_context[i]->substreamL;

		while (head != NULL) {
			/* printk("%s AFE_Mem_Control_context[%d] head = %p head->substream = %p\n ",
			__func__,i, head,head->substream); */
			head = head->next;
		}
	}
	/* printk("-%s\n ", __func__); */
}
#endif


static void FillDatatoDlmemory(unsigned int *memorypointer,
			       unsigned int fillsize, unsigned short value)
{
	int addr = 0;
	unsigned int tempvalue = value;

	tempvalue = tempvalue << 16;
	tempvalue |= value;
	/* set memory to DC value */
	for (addr = 0; addr < (fillsize >> 2); addr++) {
		*memorypointer = tempvalue;
		memorypointer++;
	}
}

static struct snd_dma_buffer *Dl1_Playback_dma_buf;

static void SetDL1BufferwithBuf(void)
{
#define Dl1_MAX_BUFFER_SIZE     (16*1024)

	AudDrv_Allocate_mem_Buffer(NULL, Soc_Aud_Digital_Block_MEM_DL1, Dl1_MAX_BUFFER_SIZE);
	Dl1_Playback_dma_buf = Get_Mem_Buffer(Soc_Aud_Digital_Block_MEM_DL1);
	Afe_Set_Reg(AFE_DL1_BASE, Dl1_Playback_dma_buf->addr, 0xffffffff);
	Afe_Set_Reg(AFE_DL1_END,
		    Dl1_Playback_dma_buf->addr + (Dl1_MAX_BUFFER_SIZE - 1), 0xffffffff);
}


void OpenAfeDigitaldl1(bool bEnable)
{
	unsigned int *Sramdata;

	if (bEnable == true) {
		SetDL1BufferwithBuf();
		SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL1, AFE_WLEN_16_BIT);
		SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL2, AFE_WLEN_16_BIT);
		SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
					  Soc_Aud_InterConnectionOutput_O03);
		SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
					  Soc_Aud_InterConnectionOutput_O04);
		SetSampleRate(Soc_Aud_Digital_Block_MEM_I2S, 44100);
		SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I05,
			      Soc_Aud_InterConnectionOutput_O03);
		SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I06,
			      Soc_Aud_InterConnectionOutput_O04);
		SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, true);

		Sramdata = (unsigned int *)(Dl1_Playback_dma_buf->area);
		FillDatatoDlmemory(Sramdata, Dl1_Playback_dma_buf->bytes, 0);

		usleep_range(5*1000, 10*1000);

		if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC) == false) {
			SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, true);
			SetI2SDacOut(44100, false, Soc_Aud_I2S_WLEN_WLEN_16BITS);
			SetI2SDacEnable(true);
		} else {
			SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, true);
		}
		EnableAfe(true);
	} else {
		SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, false);
		SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, false);
		if (GetI2SDacEnable() == false)
			SetI2SDacEnable(false);

		EnableAfe(false);
	}
}

void SetExternalModemStatus(const bool bEnable)
{
	pr_debug("%s(), mExternalModemStatus : %d => %d\n", __func__, mExternalModemStatus, bEnable);
	mExternalModemStatus = bEnable;
}


/*****************************************************************************
 * FUNCTION
 *  InitAfeControl ,ResetAfeControl
 *
 * DESCRIPTION
 *  afe init function
 *
 *****************************************************************************
 */

bool InitAfeControl(void)
{
	int i = 0;

	pr_debug("InitAfeControl\n");
	/* first time to init , reg init. */
	Auddrv_Reg_map();
	AudDrv_Clk_Power_On();
	Auddrv_Bus_Init();

	/* Auddrv_Read_Efuse_HPOffset(); //Todo */

	AfeControlMutexLock();
	AfeGlobalVarInit();
	/* allocate memory for pointers */
	if (mAudioInit == false) {
		mAudioInit = true;
		mAudioMrg = kzalloc(sizeof(struct AudioMrgIf), GFP_KERNEL);
		AudioDaiBt = kzalloc(sizeof(struct AudioDigitalDAIBT), GFP_KERNEL);
		AudioAdcI2S = kzalloc(sizeof(struct AudioDigtalI2S), GFP_KERNEL);
		m2ndI2S = kzalloc(sizeof(struct AudioDigtalI2S), GFP_KERNEL);
		m2ndI2Sout = kzalloc(sizeof(struct AudioDigtalI2S), GFP_KERNEL);
		mHDMIOutput = kzalloc(sizeof(struct AudioHdmi), GFP_KERNEL);

		for (i = 0; i < Soc_Aud_Digital_Block_NUM_OF_DIGITAL_BLOCK; i++)
			mAudioMEMIF[i] = kzalloc(sizeof(struct AudioMemIFAttribute), GFP_KERNEL);

		for (i = 0; i <= Soc_Aud_Digital_Block_MEM_HDMI; i++) {
			AFE_Mem_Control_context[i] = kzalloc(sizeof(struct AFE_MEM_CONTROL_T), GFP_KERNEL);
			AFE_Mem_Control_context[i]->substreamL = NULL;
			spin_lock_init(&AFE_Mem_Control_context[i]->substream_lock);
		}
		for (i = 0; i <= Soc_Aud_Digital_Block_MEM_HDMI; i++)
			Audio_dma_buf[i] = kzalloc(sizeof(Audio_dma_buf), GFP_KERNEL);

		memset((void *)&AFE_dL_Abnormal_context, 0, sizeof(struct AFE_DL_ABNORMAL_CONTROL_T));
	}

	memset((void *)&mAudioSramManager, 0, sizeof(struct AudioSramManager));
	init_irq_manager();

	mAudioMrg->Mrg_I2S_SampleRate = SampleRateTransform(44100);

	/* set APLL clock setting */
	AfeControlMutexUnLock();
	return true;
}

bool ResetAfeControl(void)
{
	int i = 0;

	pr_debug("ResetAfeControl\n");
	AfeControlMutexLock();
	mAudioInit = false;
	memset((void *)(mAudioMrg), 0, sizeof(struct AudioMrgIf));
	memset((void *)(AudioDaiBt), 0, sizeof(struct AudioDigitalDAIBT));

	for (i = 0; i < Soc_Aud_Digital_Block_NUM_OF_DIGITAL_BLOCK; i++)
		memset((void *)(mAudioMEMIF[i]), 0, sizeof(struct AudioMemIFAttribute));

	for (i = 0; i < (Soc_Aud_Digital_Block_MEM_HDMI + 1); i++)
		memset((void *)(AFE_Mem_Control_context[i]), 0, sizeof(struct AFE_MEM_CONTROL_T));

	AfeControlMutexUnLock();
	return true;
}


/*****************************************************************************
 * FUNCTION
 *  Register_aud_irq
 *
 * DESCRIPTION
 *  IRQ handler
 *
 *****************************************************************************
 */

bool Register_Aud_Irq(void *dev, unsigned int afe_irq_number)
{
	int ret;
#ifdef CONFIG_OF
	ret = request_irq(afe_irq_number, AudDrv_IRQ_handler, IRQF_TRIGGER_LOW,
			  "Afe_ISR_Handle", dev);
	if (ret)
		pr_warn("Register_Aud_Irq AFE IRQ register fail!!!\n");
#else
	pr_debug("%s dev name =%s\n", __func__, dev_name(dev));
	ret = request_irq(MT6580_AFE_MCU_IRQ_LINE, AudDrv_IRQ_handler,
			  IRQF_TRIGGER_LOW /*IRQF_TRIGGER_FALLING */ , "Afe_ISR_Handle", dev);
#endif
	return ret;
}

/*****************************************************************************
 * FUNCTION
 *  AudDrv_IRQ_handler / AudDrv_magic_tasklet
 *
 * DESCRIPTION
 *  IRQ handler
 *
 *****************************************************************************
 */
irqreturn_t AudDrv_IRQ_handler(int irq, void *dev_id)
{
	/* unsigned long flags; */
	uint32_t u4RegValue;

	AudDrv_Clk_On();
	u4RegValue = Afe_Get_Reg(AFE_IRQ_MCU_STATUS);
	u4RegValue &= 0xff;

	/* here is error handle , for interrupt is trigger but not status , clear all interrupt with bit 6 */
	if (u4RegValue == 0) {
		pr_warn("u4RegValue == 0 irqcount =0\n");
		Afe_Set_Reg(AFE_IRQ_MCU_CLR, 1 << 6, 0xff);
		Afe_Set_Reg(AFE_IRQ_MCU_CLR, 1, 0xff);
		Afe_Set_Reg(AFE_IRQ_MCU_CLR, 1 << 1, 0xff);
		Afe_Set_Reg(AFE_IRQ_MCU_CLR, 1 << 2, 0xff);
		Afe_Set_Reg(AFE_IRQ_MCU_CLR, 1 << 3, 0xff);
		Afe_Set_Reg(AFE_IRQ_MCU_CLR, 1 << 4, 0xff);
		Afe_Set_Reg(AFE_IRQ_MCU_CLR, 1 << 5, 0xff);
		irqcount++;
		if (irqcount > AudioInterruptLimiter) {
			SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE, false);
			SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE, false);
			irqcount = 0;
		}
		goto AudDrv_IRQ_handler_exit;
	}

	if (u4RegValue & INTERRUPT_IRQ1_MCU) {
		if (mAudioMEMIF[Soc_Aud_Digital_Block_MEM_DL1]->mState == true)
			Auddrv_DL1_Interrupt_Handler();
		if (mAudioMEMIF[Soc_Aud_Digital_Block_MEM_DL2]->mState == true)
			Auddrv_DL2_Interrupt_Handler();
	}
	if (u4RegValue & INTERRUPT_IRQ2_MCU) {
		if (mAudioMEMIF[Soc_Aud_Digital_Block_MEM_VUL]->mState == true)
			Auddrv_UL1_Interrupt_Handler();

		if (mAudioMEMIF[Soc_Aud_Digital_Block_MEM_AWB]->mState == true)
			Auddrv_AWB_Interrupt_Handler();

		if (mAudioMEMIF[Soc_Aud_Digital_Block_MEM_DAI]->mState == true)
			Auddrv_DAI_Interrupt_Handler();

		if (mAudioMEMIF[Soc_Aud_Digital_Block_MEM_VUL_DATA2]->mState == true)
			Auddrv_UL2_Interrupt_Handler();

	}

	if (u4RegValue & INTERRUPT_IRQ5_MCU)
		Auddrv_HDMI_Interrupt_Handler();

	/* clear irq */
	Afe_Set_Reg(AFE_IRQ_MCU_CLR, u4RegValue, 0xff);
AudDrv_IRQ_handler_exit:
	AudDrv_Clk_Off();
	return IRQ_HANDLED;
}

unsigned int GetApllbySampleRate(unsigned int SampleRate)
{
	if (SampleRate == 176400 || SampleRate == 88200 || SampleRate == 44100 ||
	    SampleRate == 22050 || SampleRate == 11025) {
		return Soc_Aud_APLL1;
	} else {
		return Soc_Aud_APLL2;
	}
}

void SetckSel(unsigned int I2snum, unsigned int SampleRate)
{
#if 0				/* not support */

	unsigned int ApllSource = GetApllbySampleRate(SampleRate);

	switch (I2snum) {
	case Soc_Aud_I2S0:
		Afe_Set_Reg(AUDIO_CLK_AUDDIV_0, ApllSource << 4, 1 << 4);
		break;
	case Soc_Aud_I2S1:
		Afe_Set_Reg(AUDIO_CLK_AUDDIV_0, ApllSource << 5, 1 << 5);
		break;
	case Soc_Aud_I2S2:
		Afe_Set_Reg(AUDIO_CLK_AUDDIV_0, ApllSource << 6, 1 << 6);
		break;
	case Soc_Aud_I2S3:
		Afe_Set_Reg(AUDIO_CLK_AUDDIV_0, ApllSource << 7, 1 << 7);
		break;
	}
	pr_debug("%s I2snum = %d ApllSource = %d\n", __func__, I2snum, ApllSource);
#endif
}

unsigned int SetCLkMclk(unsigned int I2snum, unsigned int SampleRate)
{
	unsigned int I2s_ck_div = 0;
#if 0				/* not support */
	unsigned int I2S_APll = 0;

	if (GetApllbySampleRate(SampleRate) == Soc_Aud_APLL1) {
		/* not support */
		I2S_APll = 22579200 * 8;
	} else {
		/* not support */
		I2S_APll = 24576000 * 8;
	}

	SetckSel(I2snum, SampleRate);	/* set I2Sx mck source */

	switch (I2snum) {
	case Soc_Aud_I2S0:
		I2s_ck_div = (I2S_APll / MCLKFS / SampleRate) - 1;
		Afe_Set_Reg(AUDIO_CLK_AUDDIV_1, I2s_ck_div, 0x000000ff);
		Afe_Set_Reg(AUDIO_CLK_AUDDIV_2, I2s_ck_div, 0x000000ff);
		break;
	case Soc_Aud_I2S1:
		I2s_ck_div = (I2S_APll / MCLKFS / SampleRate) - 1;
		Afe_Set_Reg(AUDIO_CLK_AUDDIV_1, I2s_ck_div << 8, 0x0000ff00);
		Afe_Set_Reg(AUDIO_CLK_AUDDIV_2, I2s_ck_div << 8, 0x0000ff00);
		break;
	case Soc_Aud_I2S2:
		I2s_ck_div = (I2S_APll / MCLKFS / SampleRate) - 1;
		Afe_Set_Reg(AUDIO_CLK_AUDDIV_1, I2s_ck_div << 16, 0x00ff0000);
		Afe_Set_Reg(AUDIO_CLK_AUDDIV_2, I2s_ck_div << 16, 0x00ff0000);
		break;
	case Soc_Aud_I2S3:
		I2s_ck_div = (I2S_APll / MCLKFS / SampleRate) - 1;
		Afe_Set_Reg(AUDIO_CLK_AUDDIV_1, I2s_ck_div << 24, 0xff000000);
		Afe_Set_Reg(AUDIO_CLK_AUDDIV_2, I2s_ck_div << 24, 0xff000000);
		break;
	}
	pr_debug("%s I2snum = %d I2s_ck_div = %d I2S_APll = %d\n", __func__, I2snum,
	       I2s_ck_div, I2S_APll);
#endif
	return I2s_ck_div;
}

void SetCLkBclk(unsigned int MckDiv, unsigned int SampleRate, unsigned int Channels, unsigned int Wlength)
{
#if 0				/* not support */
	unsigned int I2S_APll = 0;
	unsigned int I2S_Bclk = 0;
	unsigned int I2s_Bck_div = 0;

	pr_warn("%s MckDiv = %dv SampleRate = %d  Channels = %d Wlength = %d\n",
	       __func__, MckDiv, SampleRate, Channels, Wlength);

	MckDiv++;

	if (GetApllbySampleRate(SampleRate) == Soc_Aud_APLL1) {
		/* not support */
		I2S_APll = 22579200 * 8;
	} else {
		/* not support */
		I2S_APll = 24576000 * 8;
	}
	I2S_Bclk = SampleRate * Channels * (Wlength + 1) * 16;
	I2s_Bck_div = (I2S_APll / MckDiv) / I2S_Bclk;

	pr_debug("%s I2S_APll = %dv I2S_Bclk = %d  I2s_Bck_div = %d\n", __func__,
	       I2S_APll, I2S_Bclk, I2s_Bck_div);
	I2s_Bck_div--;
	/* bclk dived is not in AUDDIV_3 */
	/* Afe_Set_Reg(AUDIO_CLK_AUDDIV_3, I2s_Bck_div, 0x0000000f); */
	/* Afe_Set_Reg(AUDIO_CLK_AUDDIV_3, I2s_Bck_div << 4, 0x000000f0); */
#endif
}


void EnableI2SDivPower(unsigned int Diveder_name, bool bEnable)
{
#if 0				/* Todo */
	if (bEnable) {
		/* AUDIO_APLL1_DIV0 */
		Afe_Set_Reg(AUDIO_CLK_AUDDIV_3, 0 << Diveder_name, 1 << Diveder_name);
	} else {
		Afe_Set_Reg(AUDIO_CLK_AUDDIV_3, 1 << Diveder_name, 1 << Diveder_name);
	}
#endif
}

void EnableApll1(bool bEnable)
{
#if 0				/* not support */

	if (bEnable) {
		Afe_Set_Reg(AUDIO_CLK_AUDDIV_0, 0x3, 0x3);
		Afe_Set_Reg(AUDIO_CLK_AUDDIV_0, 0x07000000, 0x0f000000);
		AudDrv_APLL22M_Clk_On();
		Afe_Set_Reg(AUDIO_CLK_AUDDIV_0, 0x0, 0x3);
		SetClkCfg(AUDIO_CLK_CFG_6, 0x00010000, 0x00810000);
	} else {
		Afe_Set_Reg(AUDIO_CLK_AUDDIV_0, 0x07000000, 0x0f000000);
		AudDrv_APLL22M_Clk_Off();
		Afe_Set_Reg(AUDIO_CLK_AUDDIV_0, 0x3, 0x3);
		SetClkCfg(AUDIO_CLK_CFG_6, 0x00810000, 0x00810000);
	}
#endif
}

void EnableApll2(bool bEnable)
{
#if 0				/* not support */

	if (bEnable) {
		Afe_Set_Reg(AUDIO_CLK_AUDDIV_0, 0xc, 0xc);
		Afe_Set_Reg(AUDIO_CLK_AUDDIV_0, 0x70000000, 0xf0000000);
		AudDrv_APLL24M_Clk_On();
		Afe_Set_Reg(AUDIO_CLK_AUDDIV_0, 0x0, 0xc);
		SetClkCfg(AUDIO_CLK_CFG_6, 0x01000000, 0x81000000);
	} else {
		SetClkCfg(AUDIO_CLK_CFG_7, 0x00000080, 0x00000083);
		AudDrv_APLL24M_Clk_Off();
		Afe_Set_Reg(AUDIO_CLK_AUDDIV_0, 0xc, 0xc);
		SetClkCfg(AUDIO_CLK_CFG_6, 0x81000000, 0x81000000);
	}
#endif
}

static bool CheckMemIfEnable(void)
{
	int i = 0;

	for (i = 0; i < Soc_Aud_Digital_Block_NUM_OF_DIGITAL_BLOCK; i++) {
		if ((mAudioMEMIF[i]->mState) == true) {
			/* printk("CheckMemIfEnable == true\n"); */
			return true;
		}
	}
	/* pr_warn("CheckMemIfEnable == false\n"); */
	return false;
}


/* record VOW status for AFE GPIO control */
void SetVOWStatus(bool bEnable)
{
	unsigned long flags;

	if (mVOWStatus != bEnable) {
		spin_lock_irqsave(&afe_control_lock, flags);
		mVOWStatus = bEnable;
		pr_debug("SetVOWStatus, mVOWStatus= %d\n", mVOWStatus);
		spin_unlock_irqrestore(&afe_control_lock, flags);
	}
}

/*****************************************************************************
 * FUNCTION
 *  Auddrv_Reg_map
 *
 * DESCRIPTION
 * Auddrv_Reg_map
 *
 *****************************************************************************
 */
#ifdef CONFIG_OF
#ifdef CONFIG_MTK_LEGACY
static unsigned int pin_audclk, pin_audmiso, pin_audmosi;
static unsigned int pin_mode_audclk, pin_mode_audmosi, pin_mode_audmiso;
#endif
#endif

void EnableAfe(bool bEnable)
{
	unsigned long flags;
	bool MemEnable = false;

#ifdef CONFIG_OF
#ifdef CONFIG_MTK_LEGACY
	int ret;

	ret = GetGPIO_Info(1, &pin_audclk, &pin_mode_audclk);
	if (ret < 0) {
		pr_err("EnableAfe GetGPIO_Info FAIL1!!!\n");
		return;
	}

	ret = GetGPIO_Info(2, &pin_audmiso, &pin_mode_audmiso);
	if (ret < 0) {
		pr_err("EnableAfe GetGPIO_Info FAIL2!!!\n");
		return;
	}

	ret = GetGPIO_Info(3, &pin_audmosi, &pin_mode_audmosi);
	if (ret < 0) {
		pr_err("EnableAfe GetGPIO_Info FAIL3!!!\n");
		return;
	}
#endif
#endif

	spin_lock_irqsave(&afe_control_lock, flags);
	MemEnable = CheckMemIfEnable();
	if (false == bEnable && false == MemEnable) {
		Afe_Set_Reg(AFE_DAC_CON0, 0x0, 0x1);
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF
#ifdef CONFIG_MTK_LEGACY
		mt_set_gpio_mode(pin_audclk, GPIO_MODE_00);	/* GPIO24, AUD_CLK_MOSI. */
		if (mVOWStatus != true) {	/* this GPIO only use in record and VOW */
			mt_set_gpio_mode(pin_audmiso, GPIO_MODE_00);	/* GPIO25, AUD_DAT_MISO */
		}
		mt_set_gpio_mode(pin_audmosi, GPIO_MODE_00);	/* GPIO26, AUD_DAT_MOSI */
#else
		AudDrv_GPIO_PMIC_Select(bEnable);
#endif
#else
		mt_set_gpio_mode(GPIO_AUD_CLK_MOSI_PIN, GPIO_MODE_00);	/* GPIO24, AUD_CLK_MOSI. */
		if (mVOWStatus != true) {	/* this GPIO only use in record and VOW */
			mt_set_gpio_mode(GPIO_AUD_DAT_MISO_PIN, GPIO_MODE_00);	/* GPIO25, AUD_DAT_MISO */
		}
		mt_set_gpio_mode(GPIO_AUD_DAT_MOSI_PIN, GPIO_MODE_00);	/* GPIO26, AUD_DAT_MOSI */
#endif
#endif
	} else if (true == bEnable && true == MemEnable) {
#ifndef CONFIG_FPGA_EARLY_PORTING		/* FPGA_EARLY_PORTING */

#ifdef CONFIG_OF
#ifdef CONFIG_MTK_LEGACY
		mt_set_gpio_mode(pin_audclk, GPIO_MODE_01);	/* GPIO24, AUD_CLK_MOSI */
		if (mVOWStatus != true) {	/* this GPIO only use in record and VOW */
			mt_set_gpio_mode(pin_audmiso, GPIO_MODE_01);	/* GPIO25, AUD_DAT_MISO */
		}
		mt_set_gpio_mode(pin_audmosi, GPIO_MODE_01);	/* GPIO26, AUD_DAT_MOSI */
#else
		AudDrv_GPIO_PMIC_Select(bEnable);
#endif
#else
		mt_set_gpio_mode(GPIO_AUD_CLK_MOSI_PIN, GPIO_MODE_01);	/* GPIO24, AUD_CLK_MOSI */
		if (mVOWStatus != true) {	/* this GPIO only use in record and VOW */
			mt_set_gpio_mode(GPIO_AUD_DAT_MISO_PIN, GPIO_MODE_01);	/* GPIO25, AUD_DAT_MISO */
		}
		mt_set_gpio_mode(GPIO_AUD_DAT_MOSI_PIN, GPIO_MODE_01);	/* GPIO26, AUD_DAT_MOSI */
#endif
#endif
		Afe_Set_Reg(AFE_DAC_CON0, 0x1, 0x1);
	}
	spin_unlock_irqrestore(&afe_control_lock, flags);
}

unsigned int SampleRateTransform(unsigned int SampleRate)
{
	switch (SampleRate) {
	case 8000:
		return Soc_Aud_I2S_SAMPLERATE_I2S_8K;
	case 11025:
		return Soc_Aud_I2S_SAMPLERATE_I2S_11K;
	case 12000:
		return Soc_Aud_I2S_SAMPLERATE_I2S_12K;
	case 16000:
		return Soc_Aud_I2S_SAMPLERATE_I2S_16K;
	case 22050:
		return Soc_Aud_I2S_SAMPLERATE_I2S_22K;
	case 24000:
		return Soc_Aud_I2S_SAMPLERATE_I2S_24K;
	case 32000:
		return Soc_Aud_I2S_SAMPLERATE_I2S_32K;
	case 44100:
		return Soc_Aud_I2S_SAMPLERATE_I2S_44K;
	case 48000:
		return Soc_Aud_I2S_SAMPLERATE_I2S_48K;
	case 88000:
		return Soc_Aud_I2S_SAMPLERATE_I2S_88K;
	case 96000:
		return Soc_Aud_I2S_SAMPLERATE_I2S_96K;
	case 174000:
		return Soc_Aud_I2S_SAMPLERATE_I2S_174K;
	case 192000:
		return Soc_Aud_I2S_SAMPLERATE_I2S_192K;
	case 260000:
		return Soc_Aud_I2S_SAMPLERATE_I2S_260K;
	default:
		break;
	}
	return Soc_Aud_I2S_SAMPLERATE_I2S_44K;
}

unsigned int SGENSampleRateTransform(unsigned int SampleRate)
{
	switch (SampleRate) {
	case 8000:
		return Soc_Aud_I2S_SAMPLERATE_SGEN_8K;
	case 11025:
		return Soc_Aud_I2S_SAMPLERATE_SGEN_11K;
	case 12000:
		return Soc_Aud_I2S_SAMPLERATE_SGEN_12K;
	case 16000:
		return Soc_Aud_I2S_SAMPLERATE_SGEN_16K;
	case 22050:
		return Soc_Aud_I2S_SAMPLERATE_SGEN_22K;
	case 24000:
		return Soc_Aud_I2S_SAMPLERATE_SGEN_24K;
	case 32000:
		return Soc_Aud_I2S_SAMPLERATE_SGEN_32K;
	case 44100:
		return Soc_Aud_I2S_SAMPLERATE_SGEN_44K;
	case 48000:
		return Soc_Aud_I2S_SAMPLERATE_SGEN_48K;
	default:
		break;
	}
	return Soc_Aud_I2S_SAMPLERATE_SGEN_44K;
}


bool SetSampleRate(unsigned int Aud_block, unsigned int SampleRate)
{
	/* printk("%s Aud_block = %d SampleRate = %d\n", __func__, Aud_block, SampleRate); */
	SampleRate = SampleRateTransform(SampleRate);
	switch (Aud_block) {
	case Soc_Aud_Digital_Block_MEM_DL1:{
			Afe_Set_Reg(AFE_DAC_CON1, SampleRate, 0x0000000f);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_DL2:{
			Afe_Set_Reg(AFE_DAC_CON1, SampleRate << 4, 0x000000f0);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_I2S:{
			Afe_Set_Reg(AFE_DAC_CON1, SampleRate << 8, 0x00000f00);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_AWB:{
			Afe_Set_Reg(AFE_DAC_CON1, SampleRate << 12, 0x0000f000);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_VUL:{
			Afe_Set_Reg(AFE_DAC_CON1, SampleRate << 16, 0x000f0000);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_DAI:{
#if 0				/* Todo:might need it */
			if (SampleRate == Soc_Aud_I2S_SAMPLERATE_I2S_8K) {
				/* set I2S using 8K */
				Afe_Set_Reg(AFE_DAC_CON0, 0 << 24, 3 << 24);
			} else if (SampleRate == Soc_Aud_I2S_SAMPLERATE_I2S_16K) {
				/* set I2S using 16K */
				Afe_Set_Reg(AFE_DAC_CON0, 1 << 24, 3 << 24);
			} else if (SampleRate == Soc_Aud_I2S_SAMPLERATE_I2S_32K) {
				/* set I2S using 32K */
				Afe_Set_Reg(AFE_DAC_CON0, 2 << 24, 3 << 24);
			} else {
				return false;
			}
#endif
			break;
		}
	case Soc_Aud_Digital_Block_MEM_MOD_DAI:{
			if (SampleRate == Soc_Aud_I2S_SAMPLERATE_I2S_8K) {
				/* set I2S using 8K */
				Afe_Set_Reg(AFE_DAC_CON1, 0 << 30, 3 << 30);
			} else if (SampleRate == Soc_Aud_I2S_SAMPLERATE_I2S_16K) {
				/* set I2S using 16K */
				Afe_Set_Reg(AFE_DAC_CON1, 1 << 30, 3 << 30);
			} else {
				return false;
			}
			break;
		}

		return true;
	}
	return false;
}

bool SetChannels(unsigned int Memory_Interface, unsigned int channel)
{
	const bool bMono = (channel == 1) ? true : false;

	/* pr_debug("SetChannels Memory_Interface = %d channels = %d\n", Memory_Interface, channel); */

	switch (Memory_Interface) {
	case Soc_Aud_Digital_Block_MEM_DL1:{
			Afe_Set_Reg(AFE_DAC_CON1, bMono << 21, 1 << 21);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_DL2:{
			Afe_Set_Reg(AFE_DAC_CON1, bMono << 21, 1 << 22);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_AWB:{
			Afe_Set_Reg(AFE_DAC_CON1, bMono << 24, 1 << 24);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_VUL:{
			Afe_Set_Reg(AFE_DAC_CON1, bMono << 27, 1 << 27);
			break;
		}

	default:
		pr_warn("SetChannels  Memory_Interface = %d, channel = %d, bMono = %d\n",
		       Memory_Interface, channel, bMono);
		return false;
	}
	return true;
}


bool Set2ndI2SOutAttribute(uint32_t sampleRate)
{
	pr_debug("+%s(), sampleRate = %d\n", __func__, sampleRate);
	m2ndI2Sout->mLR_SWAP = Soc_Aud_LR_SWAP_NO_SWAP;
	m2ndI2Sout->mI2S_SLAVE = Soc_Aud_I2S_SRC_MASTER_MODE;
	m2ndI2Sout->mINV_LRCK = Soc_Aud_INV_LRCK_NO_INVERSE;
	m2ndI2Sout->mI2S_FMT = Soc_Aud_I2S_FORMAT_I2S;
	m2ndI2Sout->mI2S_WLEN = Soc_Aud_I2S_WLEN_WLEN_16BITS;
	m2ndI2Sout->mI2S_HDEN = Soc_Aud_NORMAL_CLOCK;
	m2ndI2Sout->mI2S_SAMPLERATE = sampleRate;
	Set2ndI2SOut(m2ndI2Sout);
	return true;
}

bool Set2ndI2SOut(struct AudioDigtalI2S *DigtalI2S)
{
	unsigned int u32AudioI2S = 0;

	memcpy((void *)m2ndI2Sout, (void *)DigtalI2S, sizeof(struct AudioDigtalI2S));
	u32AudioI2S = SampleRateTransform(m2ndI2Sout->mI2S_SAMPLERATE) << 8;
	u32AudioI2S |= m2ndI2Sout->mLR_SWAP << 31;
	u32AudioI2S |= m2ndI2Sout->mI2S_HDEN << 12;
	u32AudioI2S |= m2ndI2Sout->mINV_LRCK << 5;
	u32AudioI2S |= m2ndI2Sout->mI2S_FMT << 3;
	u32AudioI2S |= m2ndI2Sout->mI2S_WLEN << 1;
	Afe_Set_Reg(AFE_I2S_CON3, u32AudioI2S, AFE_MASK_ALL);
	return true;
}

bool Set2ndI2SOutEnable(bool benable)
{
	if (benable)
		Afe_Set_Reg(AFE_I2S_CON3, 0x1, 0x1);
	else
		Afe_Set_Reg(AFE_I2S_CON3, 0x0, 0x1);

	return true;
}

bool SetDaiBt(struct AudioDigitalDAIBT *mAudioDaiBt)
{
	AudioDaiBt->mBT_LEN = mAudioDaiBt->mBT_LEN;
	AudioDaiBt->mUSE_MRGIF_INPUT = mAudioDaiBt->mUSE_MRGIF_INPUT;
	AudioDaiBt->mDAI_BT_MODE = mAudioDaiBt->mDAI_BT_MODE;
	AudioDaiBt->mDAI_DEL = mAudioDaiBt->mDAI_DEL;
	AudioDaiBt->mBT_LEN = mAudioDaiBt->mBT_LEN;
	AudioDaiBt->mDATA_RDY = mAudioDaiBt->mDATA_RDY;
	AudioDaiBt->mBT_SYNC = mAudioDaiBt->mBT_SYNC;
	return true;
}

bool SetDaiBtEnable(bool bEanble)
{
	pr_debug("%s bEanble = %d\n", __func__, bEanble);
#if 0				/* not support */
	if (bEanble == true) {	/* turn on dai bt */
		Afe_Set_Reg(AFE_DAIBT_CON0, AudioDaiBt->mDAI_BT_MODE << 9, 0x1 << 9);
		if (mAudioMrg->MrgIf_En == true) {
			Afe_Set_Reg(AFE_DAIBT_CON0, 0x1 << 12, 0x1 << 12);	/* use merge */
			Afe_Set_Reg(AFE_DAIBT_CON0, 0x1 << 3, 0x1 << 3);	/* data ready */
			Afe_Set_Reg(AFE_DAIBT_CON0, 0x3, 0x3);	/* Turn on DAIBT */
		} else {	/* turn on merge and daiBT */
			/* set Mrg_I2S Samping Rate */
			Afe_Set_Reg(AFE_MRGIF_CON, mAudioMrg->Mrg_I2S_SampleRate << 20, 0xF00000);
			Afe_Set_Reg(AFE_MRGIF_CON, 1 << 16, 1 << 16);	/* set Mrg_I2S enable */
			Afe_Set_Reg(AFE_MRGIF_CON, 1, 0x1);	/* Turn on Merge Interface */
			udelay(100);
			Afe_Set_Reg(AFE_DAIBT_CON0, 0x1 << 12, 0x1 << 12);	/* use merge */
			Afe_Set_Reg(AFE_DAIBT_CON0, 0x1 << 3, 0x1 << 3);	/* data ready */
			Afe_Set_Reg(AFE_DAIBT_CON0, 0x3, 0x3);	/* Turn on DAIBT */
		}
		AudioDaiBt->mBT_ON = true;
		AudioDaiBt->mDAIBT_ON = true;
		mAudioMrg->MrgIf_En = true;
	} else {
		if (mAudioMrg->Mergeif_I2S_Enable == true) {
			Afe_Set_Reg(AFE_DAIBT_CON0, 0, 0x3);	/* Turn off DAIBT */
		} else {
			Afe_Set_Reg(AFE_DAIBT_CON0, 0, 0x3);	/* Turn on DAIBT */
			udelay(100);
			Afe_Set_Reg(AFE_MRGIF_CON, 0 << 16, 1 << 16);	/* set Mrg_I2S enable */
			Afe_Set_Reg(AFE_MRGIF_CON, 0, 0x1);	/* Turn on Merge Interface */
			mAudioMrg->MrgIf_En = false;
		}
		AudioDaiBt->mBT_ON = false;
		AudioDaiBt->mDAIBT_ON = false;
	}
#endif
	return true;
}

bool GetMrgI2SEnable(void)
{
	return mAudioMEMIF[Soc_Aud_Digital_Block_MRG_I2S_OUT]->mState;
}

bool SetMrgI2SEnable(bool bEnable, unsigned int sampleRate)
{
	pr_warn("%s bEnable = %d\n", __func__, bEnable);
#if 0				/* not support */
	if (bEnable == true) {
		/* To enable MrgI2S */
		if (mAudioMrg->MrgIf_En == true) {
			/* Merge Interface already turn on. */
			/* if sample Rate change, then it need to restart with new setting; else do nothing. */
			if (mAudioMrg->Mrg_I2S_SampleRate != SampleRateTransform(sampleRate)) {
				/* Turn off Merge Interface first to switch I2S sampling rate */
				Afe_Set_Reg(AFE_MRGIF_CON, 0, 1 << 16);	/* Turn off I2S */
				if (AudioDaiBt->mDAIBT_ON == true) {
					/* Turn off DAIBT first */
					Afe_Set_Reg(AFE_DAIBT_CON0, 0, 0x1);
				}
				udelay(100);
				Afe_Set_Reg(AFE_MRGIF_CON, 0, 0x1);	/* Turn off Merge Interface */
				udelay(100);
				Afe_Set_Reg(AFE_MRGIF_CON, 1, 0x1);	/* Turn on Merge Interface */
				if (AudioDaiBt->mDAIBT_ON == true) {
					/* use merge */
					Afe_Set_Reg(AFE_DAIBT_CON0, AudioDaiBt->mDAI_BT_MODE << 9, 0x1 << 9);
					Afe_Set_Reg(AFE_DAIBT_CON0, 0x1 << 12, 0x1 << 12);	/* use merge */
					Afe_Set_Reg(AFE_DAIBT_CON0, 0x1 << 3, 0x1 << 3);	/* data ready */
					Afe_Set_Reg(AFE_DAIBT_CON0, 0x3, 0x3);	/* Turn on DAIBT */
				}
				mAudioMrg->Mrg_I2S_SampleRate = SampleRateTransform(sampleRate);
				/* set Mrg_I2S Samping Rate */
				Afe_Set_Reg(AFE_MRGIF_CON, mAudioMrg->Mrg_I2S_SampleRate << 20, 0xF00000);
				Afe_Set_Reg(AFE_MRGIF_CON, 1 << 16, 1 << 16);	/* set Mrg_I2S enable */
			}
		} else {
			/* turn on merge Interface from off state */
			mAudioMrg->Mrg_I2S_SampleRate = SampleRateTransform(sampleRate);
			/* set Mrg_I2S Samping rates */
			Afe_Set_Reg(AFE_MRGIF_CON, mAudioMrg->Mrg_I2S_SampleRate << 20, 0xF00000);
			Afe_Set_Reg(AFE_MRGIF_CON, 1 << 16, 1 << 16);	/* set Mrg_I2S enable */
			udelay(100);
			Afe_Set_Reg(AFE_MRGIF_CON, 1, 0x1);	/* Turn on Merge Interface */
			udelay(100);
			if (AudioDaiBt->mDAIBT_ON == true) {
				Afe_Set_Reg(AFE_DAIBT_CON0, AudioDaiBt->mDAI_BT_MODE << 9, 0x1 << 9);	/* use merge */
				Afe_Set_Reg(AFE_DAIBT_CON0, 0x1 << 12, 0x1 << 12);	/* use merge */
				Afe_Set_Reg(AFE_DAIBT_CON0, 0x1 << 3, 0x1 << 3);	/* data ready */
				Afe_Set_Reg(AFE_DAIBT_CON0, 0x3, 0x3);	/* Turn on DAIBT */
			}
		}
		mAudioMrg->MrgIf_En = true;
		mAudioMrg->Mergeif_I2S_Enable = true;
	} else {
		if (mAudioMrg->MrgIf_En == true) {
			Afe_Set_Reg(AFE_MRGIF_CON, 0, 1 << 16);	/* Turn off I2S */
			if (AudioDaiBt->mDAIBT_ON == false) {
				udelay(100);
				/* DAIBT also not using, then it's OK to disable Merge Interface */
				Afe_Set_Reg(AFE_MRGIF_CON, 0, 0x1);	/* Turn off Merge Interface */
				mAudioMrg->MrgIf_En = false;
			}
		}
		mAudioMrg->Mergeif_I2S_Enable = false;
	}
#endif
	return true;
}

bool Set2ndI2SAdcIn(struct AudioDigtalI2S *DigtalI2S)
{
	return true;
}

bool SetI2SAdcIn(struct AudioDigtalI2S *DigtalI2S)
{
	unsigned int Audio_I2S_Adc = 0;

	memcpy((void *)AudioAdcI2S, (void *)DigtalI2S, sizeof(struct AudioDigtalI2S));

	if (false == AudioAdcI2SStatus) {
		unsigned int eSamplingRate = SampleRateTransform(AudioAdcI2S->mI2S_SAMPLERATE);
		unsigned int dVoiceModeSelect = 0;

		Afe_Set_Reg(AFE_ADDA_TOP_CON0, 0, 0x1);	/* Using Internal ADC */
		if (eSamplingRate == Soc_Aud_I2S_SAMPLERATE_I2S_8K)
			dVoiceModeSelect = 0;
		else if (eSamplingRate == Soc_Aud_I2S_SAMPLERATE_I2S_16K)
			dVoiceModeSelect = 1;
		else if (eSamplingRate == Soc_Aud_I2S_SAMPLERATE_I2S_32K)
			dVoiceModeSelect = 2;
		else if (eSamplingRate == Soc_Aud_I2S_SAMPLERATE_I2S_48K)
			dVoiceModeSelect = 3;

		Afe_Set_Reg(AFE_ADDA_UL_SRC_CON0,
			    (dVoiceModeSelect << 19) | (dVoiceModeSelect << 17), 0x001E0000);
		Afe_Set_Reg(AFE_ADDA_NEWIF_CFG0, 0x0bF87201, 0xFFFFFFFF);	/* up8x txif sat on */
		Afe_Set_Reg(AFE_ADDA_NEWIF_CFG1, ((dVoiceModeSelect < 3) ? 1 : 3) << 10,
			    0x00000C00);
	} else {
		Afe_Set_Reg(AFE_ADDA_TOP_CON0, 1, 0x1);	/* Using External ADC */
		Audio_I2S_Adc |= (AudioAdcI2S->mLR_SWAP << 31);
		Audio_I2S_Adc |= (AudioAdcI2S->mBuffer_Update_word << 24);
		Audio_I2S_Adc |= (AudioAdcI2S->mINV_LRCK << 23);
		Audio_I2S_Adc |= (AudioAdcI2S->mFpga_bit_test << 22);
		Audio_I2S_Adc |= (AudioAdcI2S->mFpga_bit << 21);
		Audio_I2S_Adc |= (AudioAdcI2S->mloopback << 20);
		Audio_I2S_Adc |= (SampleRateTransform(AudioAdcI2S->mI2S_SAMPLERATE) << 8);
		Audio_I2S_Adc |= (AudioAdcI2S->mI2S_FMT << 3);
		Audio_I2S_Adc |= (AudioAdcI2S->mI2S_WLEN << 1);
		pr_warn("%s Audio_I2S_Adc = 0x%x", __func__, Audio_I2S_Adc);
		Afe_Set_Reg(AFE_I2S_CON2, Audio_I2S_Adc, MASK_ALL);
	}
	return true;
}

bool EnableSideGenHw(unsigned int connection, bool direction, bool Enable)
{
	pr_warn("+%s(), connection = %d, direction = %d, Enable= %d\n", __func__,
	       connection, direction, Enable);
	if (Enable && direction) {
		switch (connection) {
		case Soc_Aud_InterConnectionInput_I00:
		case Soc_Aud_InterConnectionInput_I01:
			Afe_Set_Reg(AFE_SGEN_CON0, 0x04662662, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionInput_I02:
			Afe_Set_Reg(AFE_SGEN_CON0, 0x14662662, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionInput_I03:
		case Soc_Aud_InterConnectionInput_I04:
			Afe_Set_Reg(AFE_SGEN_CON0, 0x24662662, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionInput_I05:
		case Soc_Aud_InterConnectionInput_I06:
			Afe_Set_Reg(AFE_SGEN_CON0, 0x34662662, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionInput_I07:
		case Soc_Aud_InterConnectionInput_I08:
			Afe_Set_Reg(AFE_SGEN_CON0, 0x44662662, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionInput_I09:
		case Soc_Aud_InterConnectionInput_I14:
			Afe_Set_Reg(AFE_SGEN_CON0, 0x54662662, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionInput_I10:
		case Soc_Aud_InterConnectionInput_I11:
		case Soc_Aud_InterConnectionInput_I12:
		case Soc_Aud_InterConnectionInput_I13:
			Afe_Set_Reg(AFE_SGEN_CON0, 0x64662662, 0xffffffff);
			break;
		default:
			pr_warn("+%s(), no corresponding input SGEN\n", __func__);
			break;
		}
	} else if (Enable) {
		switch (connection) {
		case Soc_Aud_InterConnectionOutput_O00:
		case Soc_Aud_InterConnectionOutput_O01:
			Afe_Set_Reg(AFE_SGEN_CON0, 0x746c26c2, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionOutput_O02:
			Afe_Set_Reg(AFE_SGEN_CON0, 0x846c26c2, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionOutput_O03:
		case Soc_Aud_InterConnectionOutput_O04:
			Afe_Set_Reg(AFE_SGEN_CON0, 0x946c26c2, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionOutput_O05:
		case Soc_Aud_InterConnectionOutput_O06:
			Afe_Set_Reg(AFE_SGEN_CON0, 0xa46c26c2, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionOutput_O07:
		case Soc_Aud_InterConnectionOutput_O08:
		case Soc_Aud_InterConnectionOutput_O17:
		case Soc_Aud_InterConnectionOutput_O18:
			Afe_Set_Reg(AFE_SGEN_CON0, 0xb46c26c2, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionOutput_O09:
		case Soc_Aud_InterConnectionOutput_O10:
			Afe_Set_Reg(AFE_SGEN_CON0, 0xc46c26c2, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionOutput_O11:
			Afe_Set_Reg(AFE_SGEN_CON0, 0xd46c26c2, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionOutput_O12:
			if (Soc_Aud_I2S_SAMPLERATE_I2S_8K ==
				mAudioMEMIF[Soc_Aud_Digital_Block_MEM_MOD_DAI]->mSampleRate) {
				/* MD connect BT Verify (8K SamplingRate) */
				Afe_Set_Reg(AFE_SGEN_CON0, 0xe40e80e8, 0xffffffff);
			} else if (Soc_Aud_I2S_SAMPLERATE_I2S_16K ==
				   mAudioMEMIF[Soc_Aud_Digital_Block_MEM_MOD_DAI]->mSampleRate) {
				/* MD connect BT Verify (16K SamplingRate) */
				Afe_Set_Reg(AFE_SGEN_CON0, 0xe40f00f0, 0xffffffff);
			} else {
				/* use default setting */
				Afe_Set_Reg(AFE_SGEN_CON0, 0xe46c26c2, 0xffffffff);	/* Default */
			}
			break;
		default:
			pr_warn("+%s(), no corresponding output SGEN\n", __func__);
			break;
		}
	} else {
	/* don't set [31:28] as 0 when disable sinetone HW, because it will repalce i00/i01 input
		with sine gen output. */
	/* Set 0xf is correct way to disconnect sinetone HW to any I/O. */
		Afe_Set_Reg(AFE_SGEN_CON0, 0xf0000000, 0xffffffff);
	}
	return true;
}

bool SetSideGenSampleRate(unsigned int SampleRate)
{
	unsigned int sine_mode_ch1 = 0;
	unsigned int sine_mode_ch2 = 0;

	pr_warn("+%s(), SampleRate = %d\n", __func__, SampleRate);

	sine_mode_ch1 = SGENSampleRateTransform(SampleRate) << 8;
	sine_mode_ch2 = SGENSampleRateTransform(SampleRate) << 20;

	Afe_Set_Reg(AFE_SGEN_CON0, sine_mode_ch1, 0x00000f00);
	Afe_Set_Reg(AFE_SGEN_CON0, sine_mode_ch2, 0x00f00000);
	return true;
}

bool Set2ndI2SAdcEnable(bool bEnable)
{
	return true;
}

bool SetI2SAdcEnable(bool bEnable)
{
	mAudioMEMIF[Soc_Aud_Digital_Block_I2S_IN_ADC]->mState = bEnable;
	if (bEnable == true) {
		/* Enable UL SRC order:
		* UL clock (AUDIO_TOP_CON0) -> AFE (AFE_DAC_CON0) ->
		* ADDA UL DL (AFE_ADDA_UL_DL_CON0) ->
		* ADDA UL SRC (AFE_ADDA_UL_SRC_CON0)
		*/
		EnableAfe(true);
		SetADDAEnable(bEnable);
		SetULSrcEnable(bEnable);
	} else if (mAudioMEMIF[Soc_Aud_Digital_Block_I2S_OUT_DAC]->mState == false &&
		   mAudioMEMIF[Soc_Aud_Digital_Block_I2S_IN_ADC]->mState == false &&
		   mAudioMEMIF[Soc_Aud_Digital_Block_I2S_IN_ADC_2]->mState == false) {
		SetULSrcEnable(bEnable);
		SetADDAEnable(bEnable);
	}
	return true;
}

bool Set2ndI2SEnable(bool bEnable)
{
	Afe_Set_Reg(AFE_I2S_CON, bEnable, 0x1);
	return true;
}

bool CleanPreDistortion(void)
{
	/* printk("%s\n", __FUNCTION__); */
	Afe_Set_Reg(AFE_ADDA_PREDIS_CON0, 0, MASK_ALL);
	Afe_Set_Reg(AFE_ADDA_PREDIS_CON1, 0, MASK_ALL);
	return true;
}

bool SetDLSrc2(unsigned int SampleRate)
{
	unsigned int AfeAddaDLSrc2Con0, AfeAddaDLSrc2Con1;

	if (SampleRate == 8000)
		AfeAddaDLSrc2Con0 = 0;
	else if (SampleRate == 11025)
		AfeAddaDLSrc2Con0 = 1;
	else if (SampleRate == 12000)
		AfeAddaDLSrc2Con0 = 2;
	else if (SampleRate == 16000)
		AfeAddaDLSrc2Con0 = 3;
	else if (SampleRate == 22050)
		AfeAddaDLSrc2Con0 = 4;
	else if (SampleRate == 24000)
		AfeAddaDLSrc2Con0 = 5;
	else if (SampleRate == 32000)
		AfeAddaDLSrc2Con0 = 6;
	else if (SampleRate == 44100)
		AfeAddaDLSrc2Con0 = 7;
	else if (SampleRate == 48000)
		AfeAddaDLSrc2Con0 = 8;
	else
		AfeAddaDLSrc2Con0 = 7;	/* Default 44100 */

	/* ASSERT(0); */
	if (AfeAddaDLSrc2Con0 == 0 || AfeAddaDLSrc2Con0 == 3) {	/* 8k or 16k voice mode */
		AfeAddaDLSrc2Con0 = (AfeAddaDLSrc2Con0 << 28) | (0x03 << 24) | (0x03 << 11) |
		    (0x01 << 5);
	} else {
		AfeAddaDLSrc2Con0 = (AfeAddaDLSrc2Con0 << 28) | (0x03 << 24) | (0x03 << 11);
	}
	/* SA suggest apply -0.3db to audio/speech path */
	AfeAddaDLSrc2Con0 = AfeAddaDLSrc2Con0 | (0x01 << 1);	/* 2013.02.22 for voice mode degrade 0.3 db */
	AfeAddaDLSrc2Con1 = 0xf74f0000;

	Afe_Set_Reg(AFE_ADDA_DL_SRC2_CON0, AfeAddaDLSrc2Con0, MASK_ALL);
	Afe_Set_Reg(AFE_ADDA_DL_SRC2_CON1, AfeAddaDLSrc2Con1, MASK_ALL);
	return true;

}

bool SetI2SDacOut(unsigned int SampleRate, bool lowjitter, bool I2SWLen)
{
	unsigned int Audio_I2S_Dac = 0;

	pr_warn("SetI2SDacOut SampleRate %d, lowjitter %d, I2SWLen %d\n", SampleRate,
	       lowjitter, I2SWLen);
	CleanPreDistortion();
	SetDLSrc2(SampleRate);
	Audio_I2S_Dac |= (Soc_Aud_LR_SWAP_NO_SWAP << 31);
	Audio_I2S_Dac |= (SampleRateTransform(SampleRate) << 8);
	Audio_I2S_Dac |= (Soc_Aud_INV_LRCK_NO_INVERSE << 5);
	Audio_I2S_Dac |= (Soc_Aud_I2S_FORMAT_I2S << 3);
	Audio_I2S_Dac |= (I2SWLen << 1);
#if 0				/* no  low jitter mode, but need to check it on MT6580 */
	Audio_I2S_Dac |= (lowjitter << 12);	/* low gitter mode */
#endif
	Afe_Set_Reg(AFE_I2S_CON1, Audio_I2S_Dac, MASK_ALL);
	return true;
}

bool SetHwDigitalGainMode(unsigned int GainType, unsigned int SampleRate, unsigned int SamplePerStep)
{
	/* pr_warn("SetHwDigitalGainMode GainType = %d, SampleRate = %d, SamplePerStep= %d\n",
	GainType, SampleRate, SamplePerStep); */
	unsigned int value = 0;

	value = (SamplePerStep << 8) | (SampleRateTransform(SampleRate) << 4);
	switch (GainType) {
	case Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN1:
		Afe_Set_Reg(AFE_GAIN1_CON0, value, 0xfff0);
		break;
	case Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN2:
		Afe_Set_Reg(AFE_GAIN2_CON0, value, 0xfff0);
		break;
	default:
		return false;
	}
	return true;
}

bool SetHwDigitalGainEnable(int GainType, bool Enable)
{
	pr_warn("+%s(), GainType = %d, Enable = %d\n", __func__, GainType, Enable);
	switch (GainType) {
	case Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN1:
		if (Enable)
			Afe_Set_Reg(AFE_GAIN1_CUR, 0, 0xFFFFFFFF);	/* Let current gain be 0 to ramp up */

		Afe_Set_Reg(AFE_GAIN1_CON0, Enable, 0x1);
		break;
	case Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN2:
		if (Enable)
			Afe_Set_Reg(AFE_GAIN2_CUR, 0, 0xFFFFFFFF);	/* Let current gain be 0 to ramp up */

		Afe_Set_Reg(AFE_GAIN2_CON0, Enable, 0x1);
		break;
	default:
		pr_warn("%s with no match type\n", __func__);
		return false;
	}
	return true;
}

bool SetHwDigitalGain(unsigned int Gain, int GainType)
{
	pr_warn("+%s(), Gain = 0x%x, gain type = %d\n", __func__, Gain, GainType);
	switch (GainType) {
	case Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN1:
		Afe_Set_Reg(AFE_GAIN1_CON1, Gain, 0xffffffff);
		break;
	case Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN2:
		Afe_Set_Reg(AFE_GAIN2_CON1, Gain, 0xffffffff);
		break;
	default:
		pr_warn("%s with no match type\n", __func__);
		return false;
	}
	return true;
}

bool SetModemPcmConfig(int modem_index, struct AudioDigitalPCM p_modem_pcm_attribute)
{
	unsigned int reg_pcm2_intf_con = 0;
	unsigned int reg_pcm_intf_con1 = 0;

	if (modem_index == MODEM_1) {
		reg_pcm2_intf_con |= (p_modem_pcm_attribute.mTxLchRepeatSel & 0x1) << 13;
		reg_pcm2_intf_con |= (p_modem_pcm_attribute.mVbt16kModeSel & 0x1) << 12;
		reg_pcm2_intf_con |= (p_modem_pcm_attribute.mSingelMicSel & 0x1) << 7;
		reg_pcm2_intf_con |= (p_modem_pcm_attribute.mAsyncFifoSel & 0x1) << 6;
		reg_pcm2_intf_con |= (p_modem_pcm_attribute.mPcmWordLength & 0x1) << 5;
		reg_pcm2_intf_con |= (p_modem_pcm_attribute.mPcmModeWidebandSel & 0x3) << 3;
		reg_pcm2_intf_con |= (p_modem_pcm_attribute.mPcmFormat & 0x3) << 1;
		pr_warn("%s(), PCM2_INTF_CON(0x%lx) = 0x%x\n", __func__, PCM2_INTF_CON,
		       reg_pcm2_intf_con);
		Afe_Set_Reg(PCM2_INTF_CON, reg_pcm2_intf_con, MASK_ALL);
		if (p_modem_pcm_attribute.mPcmModeWidebandSel == Soc_Aud_PCM_MODE_PCM_MODE_8K) {
#if 0				/* not support */
			Afe_Set_Reg(AFE_ASRC2_CON1, 0x00098580, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC2_CON2, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC2_CON3, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC2_CON4, 0x00098580, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC2_CON7, 0x0004c2c0, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC3_CON1, 0x00098580, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC3_CON2, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC3_CON3, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC3_CON4, 0x00098580, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC3_CON7, 0x0004c2c0, 0xffffffff);
#endif
		} else if (p_modem_pcm_attribute.mPcmModeWidebandSel ==
			   Soc_Aud_PCM_MODE_PCM_MODE_16K) {
#if 0				/* not support */
			Afe_Set_Reg(AFE_ASRC2_CON1, 0x0004c2c0, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC2_CON2, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC2_CON3, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC2_CON4, 0x0004c2c0, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC2_CON7, 0x00026160, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC3_CON1, 0x0004c2c0, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC3_CON2, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC3_CON3, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC3_CON4, 0x0004c2c0, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC3_CON7, 0x00026160, 0xffffffff);
#endif
		} else if (p_modem_pcm_attribute.mPcmModeWidebandSel ==
			   Soc_Aud_PCM_MODE_PCM_MODE_32K) {
#if 0				/* not support */
			Afe_Set_Reg(AFE_ASRC2_CON1, 0x00026160, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC2_CON2, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC2_CON3, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC2_CON4, 0x00026160, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC2_CON7, 0x000130b0, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC3_CON1, 0x00026160, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC3_CON2, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC3_CON3, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC3_CON4, 0x00026160, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC3_CON7, 0x000130b0, 0xffffffff);
#endif
		}

	} else if (modem_index == MODEM_2 || modem_index == MODEM_EXTERNAL) {
		/* MODEM_2 use PCM_INTF_CON1 (0x530) !!! */
		if (p_modem_pcm_attribute.mPcmModeWidebandSel == Soc_Aud_PCM_MODE_PCM_MODE_8K) {
			Afe_Set_Reg(AFE_ASRC_CON1, 0x00098580, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC_CON2, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC_CON3, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC_CON4, 0x00098580, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC_CON7, 0x0004c2c0, 0xffffffff);
		} else if (p_modem_pcm_attribute.mPcmModeWidebandSel ==
			   Soc_Aud_PCM_MODE_PCM_MODE_16K) {
			Afe_Set_Reg(AFE_ASRC_CON1, 0x0004c2c0, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC_CON2, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC_CON3, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC_CON4, 0x0004c2c0, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC_CON7, 0x00026160, 0xffffffff);
		} else if (p_modem_pcm_attribute.mPcmModeWidebandSel ==
			   Soc_Aud_PCM_MODE_PCM_MODE_32K) {
			Afe_Set_Reg(AFE_ASRC_CON1, 0x00026160, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC_CON2, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC_CON3, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC_CON4, 0x00026160, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC_CON7, 0x000130b0, 0xffffffff);
		}

		reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mBclkOutInv & 0x01) << 22;
		reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mTxLchRepeatSel & 0x01) << 19;
		reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mVbt16kModeSel & 0x01) << 18;
		reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mExtModemSel & 0x01) << 17;
		reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mExtendBckSyncLength & 0x1F) << 9;
		reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mExtendBckSyncTypeSel & 0x01) << 8;
		reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mSingelMicSel & 0x01) << 7;
		reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mAsyncFifoSel & 0x01) << 6;
		reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mSlaveModeSel & 0x01) << 5;
		reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mPcmModeWidebandSel & 0x03) << 3;
		reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mPcmFormat & 0x03) << 1;

		pr_warn("%s(), PCM_INTF_CON1(0x%lx) = 0x%x", __func__, PCM_INTF_CON,
		       reg_pcm_intf_con1);
		Afe_Set_Reg(PCM_INTF_CON, reg_pcm_intf_con1, MASK_ALL);

	}
	return true;
}

bool SetModemPcmEnable(int modem_index, bool modem_pcm_on)
{
	unsigned int dNeedDisableASM = 0, mPcm1AsyncFifo;

	pr_warn("+%s(), modem_index = %d, modem_pcm_on = %d\n", __func__,
	       modem_index, modem_pcm_on);

	if (modem_index == MODEM_1) {	/* MODEM_1 use PCM2_INTF_CON (0x53C) !!! */
		/* todo:: temp for use fifo */
		Afe_Set_Reg(PCM2_INTF_CON, modem_pcm_on, 0x1);
		mAudioMEMIF[Soc_Aud_Digital_Block_MODEM_PCM_1_O]->mState = modem_pcm_on;
	} else if (modem_index == MODEM_2 || modem_index == MODEM_EXTERNAL) {
		/* MODEM_2 use PCM_INTF_CON1 (0x530) !!! */
		if (modem_pcm_on == true) {	/* turn on ASRC before Modem PCM on */
			mPcm1AsyncFifo = (Afe_Get_Reg(PCM_INTF_CON) & 0x0040) >> 6;
			if (mPcm1AsyncFifo == 0) {
				Afe_Set_Reg(AFE_ASRC_CON6, 0x005f188f, MASK_ALL);
				Afe_Set_Reg(AFE_ASRC_CON0, 0x86083031, MASK_ALL);
#if 0				/* not support */
				Afe_Set_Reg(AFE_ASRC4_CON6, 0x005f188f, MASK_ALL);
				Afe_Set_Reg(AFE_ASRC4_CON0, 0x06003031, MASK_ALL);
#endif
			}
			Afe_Set_Reg(PCM_INTF_CON, 0x1, 0x1);
		} else if (modem_pcm_on == false) {	/* turn off ASRC after Modem PCM off */
			Afe_Set_Reg(PCM_INTF_CON, 0x0, 0x1);

			Afe_Set_Reg(AFE_ASRC_CON6, 0x00000000, MASK_ALL);
			dNeedDisableASM = (Afe_Get_Reg(AFE_ASRC_CON0) & 0x1) ? 1 : 0;
			Afe_Set_Reg(AFE_ASRC_CON0, 0, (1 << 4 | 1 << 5 | dNeedDisableASM));
			Afe_Set_Reg(AFE_ASRC_CON0, 0x0, 0x1);
#if 0				/* not support */
			Afe_Set_Reg(AFE_ASRC4_CON6, 0x00000000, MASK_ALL);
			Afe_Set_Reg(AFE_ASRC4_CON0, 0, (1 << 4 | 1 << 5));
			Afe_Set_Reg(AFE_ASRC4_CON0, 0x0, 0x1);
#endif
		}
		mAudioMEMIF[Soc_Aud_Digital_Block_MODEM_PCM_2_O]->mState = modem_pcm_on;
	} else {
		pr_warn("%s(), no such modem_index: %d!!", __func__, modem_index);
		return false;
	}
	return true;
}


bool EnableSideToneFilter(bool stf_on)
{

	/* MD max support 16K sampling rate */
	const uint8_t kSideToneHalfTapNum = sizeof(kSideToneCoefficientTable16k) / sizeof(uint16_t);

	pr_debug("+%s(), stf_on = %d\n", __func__, stf_on);
	AudDrv_Clk_On();

	if (stf_on == false) {
		/* bypass STF result & disable */
		const bool bypass_stf_on = true;
		uint32_t reg_value = (bypass_stf_on << 31) | (stf_on << 8);

		Afe_Set_Reg(AFE_SIDETONE_CON1, reg_value, MASK_ALL);
		pr_debug("%s(), AFE_SIDETONE_CON1[0x%lx] = 0x%x\n", __func__,
		       AFE_SIDETONE_CON1, reg_value);

		/* set side tone gain = 0 */
		Afe_Set_Reg(AFE_SIDETONE_GAIN, 0, MASK_ALL);
		pr_debug("%s(), AFE_SIDETONE_GAIN[0x%lx] = 0x%x\n", __func__,
		       AFE_SIDETONE_GAIN, 0);
	} else {
		const bool bypass_stf_on = false;
		/* using STF result & enable & set half tap num */
		uint32_t write_reg_value = (bypass_stf_on << 31) | (stf_on << 8) |
		    kSideToneHalfTapNum;

		/* set side tone coefficient */
		const bool enable_read_write = true;	/* enable read/write side tone coefficient */
		const bool read_write_sel = true;	/* for write case */
		const bool sel_ch2 = false;	/* using uplink ch1 as STF input */
		uint32_t read_reg_value = Afe_Get_Reg(AFE_SIDETONE_CON0);
		size_t coef_addr = 0;

		pr_debug("%s(), AFE_SIDETONE_GAIN[0x%lx] = 0x%x\n", __func__,
		       AFE_SIDETONE_GAIN, 0);
		/* set side tone gain */
		Afe_Set_Reg(AFE_SIDETONE_GAIN, 0, MASK_ALL);
		Afe_Set_Reg(AFE_SIDETONE_CON1, write_reg_value, MASK_ALL);
		pr_debug("%s(), AFE_SIDETONE_CON1[0x%lx] = 0x%x\n", __func__,
		       AFE_SIDETONE_CON1, write_reg_value);

		for (coef_addr = 0; coef_addr < kSideToneHalfTapNum; coef_addr++) {
			bool old_write_ready = (read_reg_value >> 29) & 0x1;
			bool new_write_ready = 0;
			int try_cnt = 0;

			write_reg_value = enable_read_write << 25 |
			    read_write_sel << 24 |
			    sel_ch2 << 23 |
			    coef_addr << 16 | kSideToneCoefficientTable16k[coef_addr];
			Afe_Set_Reg(AFE_SIDETONE_CON0, write_reg_value, 0x39FFFFF);
			pr_debug("%s(), AFE_SIDETONE_CON0[0x%lx] = 0x%x\n", __func__,
			       AFE_SIDETONE_CON0, write_reg_value);

			/* wait until flag write_ready changed (means write done) */
			for (try_cnt = 0; try_cnt < 10; try_cnt++) {	/* max try 10 times */
				/* usleep_range(3*1000, 10*1000); */
				read_reg_value = Afe_Get_Reg(AFE_SIDETONE_CON0);
				new_write_ready = (read_reg_value >> 29) & 0x1;
				if (new_write_ready == old_write_ready) { /* flip => ok */
					udelay(3);
					if (try_cnt == 9) {
						pr_err("%s, ERROR in writing filter coefficients\n", __func__);
						AudDrv_Clk_Off();
						return false;
					}
				} else {
					break;
				}

			}
		}
	}
	AudDrv_Clk_Off();
	pr_debug("-%s(), stf_on = %d\n", __func__, stf_on);
	return true;
}


bool SetMemoryPathEnable(unsigned int Aud_block, bool bEnable)
{
	/* pr_debug("%s Aud_block = %d bEnable = %d\n", __func__, Aud_block, bEnable); */
	if (Aud_block >= Soc_Aud_Digital_Block_NUM_OF_DIGITAL_BLOCK)
		return false;

	/* set for counter */
	if (bEnable == true) {
		if (mAudioMEMIF[Aud_block]->mUserCount == 0)
			mAudioMEMIF[Aud_block]->mState = true;

		mAudioMEMIF[Aud_block]->mUserCount++;
	} else {
		mAudioMEMIF[Aud_block]->mUserCount--;
		if (mAudioMEMIF[Aud_block]->mUserCount == 0)
			mAudioMEMIF[Aud_block]->mState = false;

		if (mAudioMEMIF[Aud_block]->mUserCount < 0) {
			mAudioMEMIF[Aud_block]->mUserCount = 0;
			pr_warn("warning , user count <0\n");
		}
	}
	/*pr_warn("%s bEnable = %d Aud_block = %d mAudioMEMIF[Aud_block]->mUserCount = %d\n",
		__func__, bEnable, Aud_block, mAudioMEMIF[Aud_block]->mUserCount);*/

	if (Aud_block > Soc_Aud_Digital_Block_NUM_OF_MEM_INTERFACE)
		return true;

	if ((bEnable == true) && (mAudioMEMIF[Aud_block]->mUserCount == 1))
		Afe_Set_Reg(AFE_DAC_CON0, bEnable << (Aud_block + 1), 1 << (Aud_block + 1));
	else if ((bEnable == false) && (mAudioMEMIF[Aud_block]->mUserCount == 0))
		Afe_Set_Reg(AFE_DAC_CON0, bEnable << (Aud_block + 1), 1 << (Aud_block + 1));

	return true;
}

bool GetMemoryPathEnable(unsigned int Aud_block)
{
	if (Aud_block < Soc_Aud_Digital_Block_NUM_OF_DIGITAL_BLOCK)
		return mAudioMEMIF[Aud_block]->mState;

	return false;
}

void SetULSrcEnable(bool bEnable)
{
	unsigned long flags;

	/*pr_debug("%s bEnable = %d\n", __func__, bEnable);*/

	spin_lock_irqsave(&afe_control_lock, flags);
	if (bEnable == true) {
		Afe_Set_Reg(AFE_ADDA_UL_SRC_CON0, 0x1, 0x1);
	} else {
		if (mAudioMEMIF[Soc_Aud_Digital_Block_I2S_IN_ADC]->mState == false &&
		    mAudioMEMIF[Soc_Aud_Digital_Block_I2S_IN_ADC_2]->mState == false) {
			Afe_Set_Reg(AFE_ADDA_UL_SRC_CON0, 0x0, 0x1);
		}
	}
	spin_unlock_irqrestore(&afe_control_lock, flags);
}

void SetADDAEnable(bool bEnable)
{
	unsigned long flags;

	/*pr_debug("%s bEnable = %d\n", __func__, bEnable);*/

	spin_lock_irqsave(&afe_control_lock, flags);
	if (bEnable == true) {
		Afe_Set_Reg(AFE_ADDA_UL_DL_CON0, 0x1, 0x1);
	} else {
		if (mAudioMEMIF[Soc_Aud_Digital_Block_I2S_OUT_DAC]->mState == false &&
		    mAudioMEMIF[Soc_Aud_Digital_Block_I2S_IN_ADC]->mState == false &&
		    mAudioMEMIF[Soc_Aud_Digital_Block_I2S_IN_ADC_2]->mState == false) {
			Afe_Set_Reg(AFE_ADDA_UL_DL_CON0, 0x0, 0x1);
		}
	}
	spin_unlock_irqrestore(&afe_control_lock, flags);
}

bool SetI2SDacEnable(bool bEnable)
{
	pr_warn("%s bEnable = %d", __func__, bEnable);
	if (bEnable) {
		/* Enable UL SRC order:
		* UL clock (AUDIO_TOP_CON0) -> AFE (AFE_DAC_CON0) ->
		* ADDA UL DL (AFE_ADDA_UL_DL_CON0) ->
		* ADDA DL SRC (AFE_ADDA_DL_SRC2_CON0)
		*/
		EnableAfe(true);
		SetADDAEnable(true);
		Afe_Set_Reg(AFE_ADDA_DL_SRC2_CON0, bEnable, 0x01);
		Afe_Set_Reg(AFE_I2S_CON1, bEnable, 0x1);
		Afe_Set_Reg(FPGA_CFG1, 0, 0x10);	/* For FPGA Pin the same with DAC */
	} else {
		Afe_Set_Reg(AFE_ADDA_DL_SRC2_CON0, bEnable, 0x01);
		Afe_Set_Reg(AFE_I2S_CON1, bEnable, 0x1);
		SetADDAEnable(false);

		/* should delayed 1/fs(smallest is 8k) = 125us before afe off */
		udelay(125);

		Afe_Set_Reg(FPGA_CFG1, 1 << 4, 0x10);	/* For FPGA Pin the same with DAC */
	}
	return true;
}

bool GetI2SDacEnable(void)
{
	return mAudioMEMIF[Soc_Aud_Digital_Block_I2S_OUT_DAC]->mState;
}


bool checkDllinkMEMIfStatus(void)
{
	int i = 0;

	for (i = Soc_Aud_Digital_Block_MEM_DL1 ; i <= Soc_Aud_Digital_Block_MEM_DL2 ; i++) {
		if (mAudioMEMIF[i]->mState  == true)
			return true;
	}
	return false;
}

bool checkUplinkMEMIfStatus(void)
{
	int i = 0;

	for (i = Soc_Aud_Digital_Block_MEM_VUL; i <= Soc_Aud_Digital_Block_MEM_VUL_DATA2; i++) {
		if (mAudioMEMIF[i]->mState == true)
			return true;
	}
	return false;
}

bool SetHDMIChannels(unsigned int Channels)
{
	pr_warn("+%s(), not supported!!!\n", __func__);
	return true;
}

bool SetHDMIEnable(bool bEnable)
{
	pr_warn("+%s(), not supported!!!\n", __func__);
	return true;
}

bool SetHDMIConnection(unsigned int ConnectionState, unsigned int Input, unsigned int Output)
{
	return true;
}

bool SetConnection(unsigned int ConnectionState, unsigned int Input, unsigned int Output)
{
	return SetConnectionState(ConnectionState, Input, Output);
}


static bool SetIrqEnable(unsigned int Irqmode, bool bEnable)
{
	/*pr_warn("%s(), Irqmode %d, bEnable %d\n", __func__, Irqmode, bEnable);*/
	switch (Irqmode) {
	case Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE:
	case Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE:
		Afe_Set_Reg(AFE_IRQ_MCU_CON, (bEnable << Irqmode), (1 << Irqmode));
		break;
	default:
		pr_err("%s(), error, not supported IRQ %d", __func__, Irqmode);
		break;
	}
	/* pr_warn("-%s(), Irqmode = %d, bEnable = %d\n", __FUNCTION__, Irqmode, bEnable); */
	return true;
}


static bool SetIrqMcuSampleRate(unsigned int Irqmode, unsigned int SampleRate)
{
	unsigned int SRIdx = SampleRateTransform(SampleRate);

	/*pr_debug("%s(), Irqmode %d, SampleRate %d\n",
		__func__, Irqmode, SampleRate);*/
	switch (Irqmode) {
	case Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE:
		Afe_Set_Reg(AFE_IRQ_MCU_CON, SRIdx << 4, 0xf << 4);
		break;
	case Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE:
		Afe_Set_Reg(AFE_IRQ_MCU_CON, SRIdx << 8, 0xf << 8);
		break;
	default:
		return false;
	}
	return true;
}

static bool SetIrqMcuCounter(unsigned int Irqmode, unsigned int Counter)
{
	/*pr_debug("%s(), Irqmode %d, Counter %d\n", __func__, Irqmode, Counter);*/
	switch (Irqmode) {
	case Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE:
		Afe_Set_Reg(AFE_IRQ_MCU_CNT1, Counter, 0xffffffff);
		break;
	case Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE:
		Afe_Set_Reg(AFE_IRQ_MCU_CNT2, Counter, 0xffffffff);
		break;
	case Soc_Aud_IRQ_MCU_MODE_IRQ3_MCU_MODE:
		Afe_Set_Reg(AFE_IRQ_MCU_CNT1, Counter << 20, 0xfff00000);
		break;
	default:
		return false;
	}
	return true;
}

bool SetMemDuplicateWrite(unsigned int InterfaceType, int dupwrite)
{
	switch (InterfaceType) {
	case Soc_Aud_Digital_Block_MEM_DAI:{
			Afe_Set_Reg(AFE_DAC_CON1, dupwrite << 29, 1 << 29);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_MOD_DAI:{
			Afe_Set_Reg(AFE_DAC_CON1, dupwrite << 31, 1 << 31);
			break;
		}
	default:
		return false;
	}
	return true;
}


bool Set2ndI2SInConfig(unsigned int sampleRate, bool bIsSlaveMode)
{
	struct AudioDigtalI2S I2S2ndIn_attribute;

	memset((void *)&I2S2ndIn_attribute, 0, sizeof(I2S2ndIn_attribute));
	I2S2ndIn_attribute.mLR_SWAP = Soc_Aud_LR_SWAP_NO_SWAP;
	I2S2ndIn_attribute.mI2S_SLAVE = bIsSlaveMode;
	I2S2ndIn_attribute.mI2S_SAMPLERATE = sampleRate;
	I2S2ndIn_attribute.mINV_LRCK = Soc_Aud_INV_LRCK_NO_INVERSE;
	I2S2ndIn_attribute.mI2S_FMT = Soc_Aud_I2S_FORMAT_I2S;
	I2S2ndIn_attribute.mI2S_WLEN = Soc_Aud_I2S_WLEN_WLEN_16BITS;
	Set2ndI2SIn(&I2S2ndIn_attribute);
	return true;
}

bool Set2ndI2SIn(struct AudioDigtalI2S *mDigitalI2S)
{
	unsigned int Audio_I2S_Adc = 0;

	memcpy((void *)m2ndI2S, (void *)mDigitalI2S, sizeof(struct AudioDigtalI2S));
	if (!m2ndI2S->mI2S_SLAVE) {	/* Master setting SampleRate only */
		SetSampleRate(Soc_Aud_Digital_Block_MEM_I2S, m2ndI2S->mI2S_SAMPLERATE);
	}
	Audio_I2S_Adc |= (m2ndI2S->mINV_LRCK << 5);
	Audio_I2S_Adc |= (m2ndI2S->mI2S_FMT << 3);
	Audio_I2S_Adc |= (m2ndI2S->mI2S_SLAVE << 2);
	Audio_I2S_Adc |= (m2ndI2S->mI2S_WLEN << 1);
	Audio_I2S_Adc |= (m2ndI2S->mI2S_IN_PAD_SEL << 28);
	Audio_I2S_Adc |= 1 << 31;	/* Default enable phase_shift_fix for better quality */
	pr_warn("Set2ndI2SIn Audio_I2S_Adc= 0x%x", Audio_I2S_Adc);
	Afe_Set_Reg(AFE_I2S_CON, Audio_I2S_Adc, 0xfffffffe);
	if (!m2ndI2S->mI2S_SLAVE)
		Afe_Set_Reg(FPGA_CFG1, 1 << 8, 0x0100);
	else
		Afe_Set_Reg(FPGA_CFG1, 0, 0x0100);

	return true;
}

bool Set2ndI2SInEnable(bool bEnable)
{
	pr_warn("Set2ndI2SInEnable bEnable = %d", bEnable);
	m2ndI2S->mI2S_EN = bEnable;
	Afe_Set_Reg(AFE_I2S_CON, bEnable, 0x1);
	mAudioMEMIF[Soc_Aud_Digital_Block_I2S_IN_2]->mState = bEnable;
	return true;
}

bool SetI2SASRCConfig(bool bIsUseASRC, unsigned int dToSampleRate)
{
	pr_warn("+%s() bIsUseASRC [%d] dToSampleRate [%d]\n", __func__, bIsUseASRC,
	       dToSampleRate);
	if (true == bIsUseASRC) {
		BUG_ON(!(dToSampleRate == 44100 || dToSampleRate == 48000));
		Afe_Set_Reg(AFE_CONN4, 0, 1 << 30);
		SetSampleRate(Soc_Aud_Digital_Block_MEM_I2S, dToSampleRate);	/* To target sample rate */
		Afe_Set_Reg(AFE_ASRC_CON13, 0, 1 << 16);	/* 0:Stereo 1:Mono */
		if (dToSampleRate == 44100) {
			Afe_Set_Reg(AFE_ASRC_CON14, 0xDC8000, AFE_MASK_ALL);
			Afe_Set_Reg(AFE_ASRC_CON15, 0xA00000, AFE_MASK_ALL);
			Afe_Set_Reg(AFE_ASRC_CON17, 0x1FBD, AFE_MASK_ALL);
		} else {
			Afe_Set_Reg(AFE_ASRC_CON14, 0x600000, AFE_MASK_ALL);
			Afe_Set_Reg(AFE_ASRC_CON15, 0x400000, AFE_MASK_ALL);
			Afe_Set_Reg(AFE_ASRC_CON17, 0xCB2, AFE_MASK_ALL);
		}

		Afe_Set_Reg(AFE_ASRC_CON16, 0x00075987, AFE_MASK_ALL);	/* Calibration setting */
		Afe_Set_Reg(AFE_ASRC_CON20, 0x00001b00, AFE_MASK_ALL);	/* Calibration setting */
	} else {
		Afe_Set_Reg(AFE_CONN4, 1 << 30, 1 << 30);
	}
	return true;
}

bool SetI2SASRCEnable(bool bEnable)
{
	if (true == bEnable) {
		Afe_Set_Reg(AFE_ASRC_CON0, ((1 << 6) | (1 << 0)), ((1 << 6) | (1 << 0)));
	} else {
		unsigned int dNeedDisableASM = (Afe_Get_Reg(AFE_ASRC_CON0) & 0x0030) ? 1 : 0;

		Afe_Set_Reg(AFE_ASRC_CON0, 0, (1 << 6 | dNeedDisableASM));
	}
	return true;
}

bool SetMemIfFetchFormatPerSample(unsigned int InterfaceType, unsigned int eFetchFormat)
{
	mAudioMEMIF[InterfaceType]->mFetchFormatPerSample = eFetchFormat;
	/*
	   pr_warn("+%s(), InterfaceType = %d, eFetchFormat = %d, mFetchFormatPerSample = %d\n",
	   __FUNCTION__, InterfaceType, eFetchFormat, mAudioMEMIF[InterfaceType]->mFetchFormatPerSample); */
#if 0				/* not support */
	switch (InterfaceType) {
	case Soc_Aud_Digital_Block_MEM_DL1:{
			Afe_Set_Reg(AFE_MEMIF_PBUF_SIZE,
				    mAudioMEMIF[InterfaceType]->mFetchFormatPerSample << 16,
				    0x00030000);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_DL1_DATA2:{
			Afe_Set_Reg(AFE_MEMIF_PBUF_SIZE,
				    mAudioMEMIF[InterfaceType]->mFetchFormatPerSample << 12,
				    0x00003000);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_DL2:{
			Afe_Set_Reg(AFE_MEMIF_PBUF_SIZE,
				    mAudioMEMIF[InterfaceType]->mFetchFormatPerSample << 18,
				    0x0000c000);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_I2S:{
			/* Afe_Set_Reg(AFE_DAC_CON1, mAudioMEMIF[InterfaceType].mSampleRate << 8 , 0x00000f00); */
			pr_warn("Unsupport MEM_I2S");
			break;
		}
	case Soc_Aud_Digital_Block_MEM_AWB:{
			Afe_Set_Reg(AFE_MEMIF_PBUF_SIZE,
				    mAudioMEMIF[InterfaceType]->mFetchFormatPerSample << 20,
				    0x00300000);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_VUL:{
			Afe_Set_Reg(AFE_MEMIF_PBUF_SIZE,
				    mAudioMEMIF[InterfaceType]->mFetchFormatPerSample << 22,
				    0x00C00000);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_VUL_DATA2:{
			Afe_Set_Reg(AFE_MEMIF_PBUF_SIZE,
				    mAudioMEMIF[InterfaceType]->mFetchFormatPerSample << 14,
				    0x000C0000);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_DAI:{
			Afe_Set_Reg(AFE_MEMIF_PBUF_SIZE,
				    mAudioMEMIF[InterfaceType]->mFetchFormatPerSample << 24,
				    0x03000000);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_MOD_DAI:{
			Afe_Set_Reg(AFE_MEMIF_PBUF_SIZE,
				    mAudioMEMIF[InterfaceType]->mFetchFormatPerSample << 26,
				    0x0C000000);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_HDMI:{
			Afe_Set_Reg(AFE_MEMIF_PBUF_SIZE,
				    mAudioMEMIF[InterfaceType]->mFetchFormatPerSample << 28,
				    0x30000000);
			break;
		}
	default:
		return false;
	}
#endif
	return true;
}

bool SetoutputConnectionFormat(unsigned int ConnectionFormat, unsigned int Output)
{
#if 0				/* not support */
	/* pr_warn("+%s(), Data Format = %d, Output = %d\n", __FUNCTION__, ConnectionFormat, Output); */
	Afe_Set_Reg(AFE_CONN_24BIT, (ConnectionFormat << Output), (1 << Output));
#endif
	return true;
}

bool SetHDMIMCLK(void)
{
	unsigned int mclksamplerate = mHDMIOutput->mSampleRate * 256;
	unsigned int hdmi_APll = GetHDMIApLLSource();
	unsigned int hdmi_mclk_div = 0;

	pr_warn("%s\n", __func__);
	if (hdmi_APll == APLL_SOURCE_24576)
		hdmi_APll = 24576000;
	else
		hdmi_APll = 22579200;

	pr_warn("%s hdmi_mclk_div = %d mclksamplerate = %d\n", __func__, hdmi_mclk_div,
	       mclksamplerate);
	hdmi_mclk_div = (hdmi_APll / mclksamplerate / 2) - 1;
	mHDMIOutput->mHdmiMckDiv = hdmi_mclk_div;
	pr_warn("%s hdmi_mclk_div = %d\n", __func__, hdmi_mclk_div);
	Afe_Set_Reg(FPGA_CFG1, hdmi_mclk_div << 24, 0x3f000000);

	SetCLkMclk(Soc_Aud_I2S3, mHDMIOutput->mSampleRate);
	return true;
}

bool SetHDMIBCLK(void)
{
	mHDMIOutput->mBckSamplerate = mHDMIOutput->mSampleRate * mHDMIOutput->mChannels;
	pr_warn("%s mBckSamplerate = %d mSampleRate = %d mChannels = %d\n", __func__,
	       mHDMIOutput->mBckSamplerate, mHDMIOutput->mSampleRate, mHDMIOutput->mChannels);
	mHDMIOutput->mBckSamplerate *= (mHDMIOutput->mI2S_WLEN + 1) * 16;
	pr_warn("%s mBckSamplerate = %d mApllSamplerate = %d\n", __func__,
	       mHDMIOutput->mBckSamplerate, mHDMIOutput->mApllSamplerate);
	mHDMIOutput->mHdmiBckDiv = (mHDMIOutput->mApllSamplerate /
				    mHDMIOutput->mBckSamplerate / 2) - 1;
	pr_warn("%s mHdmiBckDiv = %d\n", __func__, mHDMIOutput->mHdmiBckDiv);
	Afe_Set_Reg(FPGA_CFG1, (mHDMIOutput->mHdmiBckDiv) << 16, 0x00ff0000);
	return true;
}

unsigned int GetHDMIApLLSource(void)
{
	pr_warn("%s ApllSource = %d\n", __func__, mHDMIOutput->mApllSource);
	return mHDMIOutput->mApllSource;
}

bool SetHDMIApLL(unsigned int ApllSource)
{
	pr_warn("%s ApllSource = %d", __func__, ApllSource);
	if (ApllSource == APLL_SOURCE_24576) {
		Afe_Set_Reg(FPGA_CFG1, 0 << 31, 1 << 31);
		mHDMIOutput->mApllSource = APLL_SOURCE_24576;
		mHDMIOutput->mApllSamplerate = 24576000;
	} else if (ApllSource == APLL_SOURCE_225792) {
		Afe_Set_Reg(FPGA_CFG1, 1 << 31, 1 << 31);
		mHDMIOutput->mApllSource = APLL_SOURCE_225792;
		mHDMIOutput->mApllSamplerate = 22579200;
	}
	return true;
}

bool SetHDMIdatalength(unsigned int length)
{
	pr_warn("%s length = %d\n ", __func__, length);
	mHDMIOutput->mI2S_WLEN = length;
	return true;
}

bool SetHDMIsamplerate(unsigned int samplerate)
{
	unsigned int SampleRateinedx = SampleRateTransform(samplerate);

	mHDMIOutput->mSampleRate = samplerate;
	pr_warn("%s samplerate = %d\n", __func__, samplerate);
	switch (SampleRateinedx) {
	case Soc_Aud_I2S_SAMPLERATE_I2S_8K:
		SetHDMIApLL(APLL_SOURCE_24576);
		break;
	case Soc_Aud_I2S_SAMPLERATE_I2S_11K:
		SetHDMIApLL(APLL_SOURCE_225792);
		break;
	case Soc_Aud_I2S_SAMPLERATE_I2S_12K:
		SetHDMIApLL(APLL_SOURCE_24576);
		break;
	case Soc_Aud_I2S_SAMPLERATE_I2S_16K:
		SetHDMIApLL(APLL_SOURCE_24576);
		break;
	case Soc_Aud_I2S_SAMPLERATE_I2S_22K:
		SetHDMIApLL(APLL_SOURCE_225792);
		break;
	case Soc_Aud_I2S_SAMPLERATE_I2S_24K:
		SetHDMIApLL(APLL_SOURCE_24576);
		break;
	case Soc_Aud_I2S_SAMPLERATE_I2S_32K:
		SetHDMIApLL(APLL_SOURCE_24576);
		break;
	case Soc_Aud_I2S_SAMPLERATE_I2S_44K:
		SetHDMIApLL(APLL_SOURCE_225792);
		break;
	case Soc_Aud_I2S_SAMPLERATE_I2S_48K:
		SetHDMIApLL(APLL_SOURCE_24576);
		break;
	case Soc_Aud_I2S_SAMPLERATE_I2S_88K:
		SetHDMIApLL(APLL_SOURCE_225792);
		break;
	case Soc_Aud_I2S_SAMPLERATE_I2S_96K:
		SetHDMIApLL(APLL_SOURCE_24576);
		break;
	default:
		break;
	}
	return true;
}

bool SetTDMLrckWidth(unsigned int cycles)
{
	pr_debug("%s not support!!!\n", __func__);
	return true;
}

bool SetTDMbckcycle(unsigned int cycles)
{
	pr_debug("%s not support!!!\n", __func__);
	return true;
}

bool SetTDMChannelsSdata(unsigned int channels)
{
	pr_debug("%s not support!!!\n", __func__);
	return true;
}

bool SetTDMDatalength(unsigned int length)
{
	pr_debug("%s not support!!!\n", __func__);
	return true;
}

bool SetTDMI2Smode(unsigned int mode)
{
	pr_debug("%s not support!!!\n", __func__);
	return true;
}

bool SetTDMLrckInverse(bool enable)
{
	pr_debug("%s not support!!!\n", __func__);
	return true;
}

bool SetTDMBckInverse(bool enable)
{
	pr_debug("%s not support!!!\n", __func__);
	return true;
}

bool SetTDMEnable(bool enable)
{
	pr_debug("%s not support!!!\n", __func__);
	return true;
}

/*****************************************************************************
 * FUNCTION
 *  AudDrv_Allocate_DL1_Buffer / AudDrv_Free_DL1_Buffer
 *
 * DESCRIPTION
 *  allocate DL1 Buffer
 *

******************************************************************************/
int AudDrv_Allocate_DL1_Buffer(struct device *pDev, uint32_t Afe_Buf_Length)
{
#ifdef AUDIO_MEMORY_SRAM
	uint32_t u4PhyAddr = 0;
#endif
	struct AFE_BLOCK_T *pblock;

	/* pr_debug("%s Afe_Buf_Length = %d\n ", __func__, Afe_Buf_Length); */

	pblock = &(AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_DL1]->rBlock);
	pblock->u4BufferSize = Afe_Buf_Length;

#ifdef AUDIO_MEMORY_SRAM
	if (Afe_Buf_Length > AFE_INTERNAL_SRAM_SIZE) {
		PRINTK_AUDDRV("Afe_Buf_Length > AUDDRV_DL1_MAX_BUFFER_LENGTH\n");
		return -1;
	}
#endif
	/* allocate memory */
	{
#ifdef AUDIO_MEMORY_SRAM
		/* todo , there should be a sram manager to allocate memory for low power. */
		u4PhyAddr = AFE_INTERNAL_SRAM_PHY_BASE;
		pblock->pucPhysBufAddr = u4PhyAddr;

#ifdef AUDIO_MEM_IOREMAP
		/* PRINTK_AUDDRV("AudDrv_Allocate_DL1_Buffer length AUDIO_MEM_IOREMAP  = %d\n",
			      Afe_Buf_Length); */
		pblock->pucVirtBufAddr = (uint8_t *) Get_Afe_SramBase_Pointer();
#else
		pblock->pucVirtBufAddr = AFE_INTERNAL_SRAM_VIR_BASE;
#endif

#else
		PRINTK_AUDDRV("AudDrv_Allocate_DL1_Buffer use dram");
		pblock->pucVirtBufAddr = dma_alloc_coherent(pDev, pblock->u4BufferSize,
							    &pblock->pucPhysBufAddr, GFP_KERNEL);
#endif
	}
	PRINTK_AUDDRV("AudDrv_Allocate_DL1_Buffer Afe_Buf_Length = %d pucVirtBufAddr = %p\n",
		      Afe_Buf_Length, pblock->pucVirtBufAddr);

	/* check 32 bytes align */
	if ((pblock->pucPhysBufAddr & 0x1f) != 0) {
		PRINTK_AUDDRV("[Auddrv] AudDrv_Allocate_DL1_Buffer is not aligned (0x%x)\n",
			      pblock->pucPhysBufAddr);
	}

	pblock->u4SampleNumMask = 0x001f;	/* 32 byte align */
	pblock->u4WriteIdx = 0;
	pblock->u4DMAReadIdx = 0;
	pblock->u4DataRemained = 0;
	pblock->u4fsyncflag = false;
	pblock->uResetFlag = true;

	/* set sram address top hardware */
	Afe_Set_Reg(AFE_DL1_BASE, pblock->pucPhysBufAddr, 0xffffffff);
	Afe_Set_Reg(AFE_DL1_END, pblock->pucPhysBufAddr + (Afe_Buf_Length - 1), 0xffffffff);
	memset_io(pblock->pucVirtBufAddr, 0, pblock->u4BufferSize);
	return 0;
}

int AudDrv_Allocate_DL2_Buffer(struct device *pDev, uint32_t Afe_Buf_Length)
{
#ifdef AUDIO_MEMORY_SRAM
	uint32_t u4PhyAddr = 0;
#endif
	struct AFE_BLOCK_T *pblock;

	/*pr_debug("%s Afe_Buf_Length = %d\n ", __func__, Afe_Buf_Length);*/
	pblock = &(AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_DL2]->rBlock);
	pblock->u4BufferSize = Afe_Buf_Length;

#ifdef AUDIO_MEMORY_SRAM
	if (Afe_Buf_Length > AFE_INTERNAL_SRAM_SIZE) {
		PRINTK_AUDDRV("%s(), Afe_Buf_Length > AFE_INTERNAL_SRAM_SIZE\n",
			      __func__);
		return -1;
	}
#endif
	/* allocate memory */
	{
#ifdef AUDIO_MEMORY_SRAM
		/* todo , there should be a sram manager to allocate memory for low power. */
		u4PhyAddr = AFE_INTERNAL_SRAM_PHY_BASE;
		pblock->pucPhysBufAddr = u4PhyAddr;
#ifdef AUDIO_MEM_IOREMAP
		PRINTK_AUDDRV("%s(), length AUDIO_MEM_IOREMAP  = %d\n",
			      __func__, Afe_Buf_Length);
		pblock->pucVirtBufAddr = (uint8_t *) Get_Afe_SramBase_Pointer();
#else
		pblock->pucVirtBufAddr = AFE_INTERNAL_SRAM_VIR_BASE;
#endif
#else
		PRINTK_AUDDRV("%s(), use dram", __func__);
		pblock->pucVirtBufAddr =
		    dma_alloc_coherent(pDev, pblock->u4BufferSize,
		    		       &pblock->pucPhysBufAddr,
				       GFP_KERNEL);
#endif
	}
	PRINTK_AUDDRV("%s(), Afe_Buf_Length = %dpucVirtBufAddr = %p\n",
		      __func__, Afe_Buf_Length, pblock->pucVirtBufAddr);

	/* check 32 bytes align */
	if ((pblock->pucPhysBufAddr & 0x1f) != 0) {
		PRINTK_AUDDRV("[Auddrv] %s(), is not aligned (0x%x)\n",
			      __func__, pblock->pucPhysBufAddr);
	}

	pblock->u4SampleNumMask = 0x001f;	/* 32 byte align */
	pblock->u4WriteIdx = 0;
	pblock->u4DMAReadIdx = 0;
	pblock->u4DataRemained = 0;
	pblock->u4fsyncflag = false;
	pblock->uResetFlag = true;

	/* set sram address top hardware */
	Afe_Set_Reg(AFE_DL2_BASE, pblock->pucPhysBufAddr, 0xffffffff);
	Afe_Set_Reg(AFE_DL2_END, pblock->pucPhysBufAddr + (Afe_Buf_Length - 1), 0xffffffff);
	memset_io(pblock->pucVirtBufAddr, 0, pblock->u4BufferSize);

	return 0;
}

int AudDrv_Allocate_mem_Buffer(struct device *pDev,
			       enum Soc_Aud_Digital_Block MemBlock, unsigned int Buffer_length)
{
	switch (MemBlock) {
	case Soc_Aud_Digital_Block_MEM_DL1:
	case Soc_Aud_Digital_Block_MEM_DL2:
	case Soc_Aud_Digital_Block_MEM_DAI:
	case Soc_Aud_Digital_Block_MEM_AWB:
	case Soc_Aud_Digital_Block_MEM_MOD_DAI:
	case Soc_Aud_Digital_Block_MEM_DL1_DATA2:
	case Soc_Aud_Digital_Block_MEM_VUL_DATA2:
	case Soc_Aud_Digital_Block_MEM_HDMI:{
			pr_warn("%s MemBlock =%d Buffer_length = %d\n ", __func__, MemBlock,
			       Buffer_length);
			if (Audio_dma_buf[MemBlock] != NULL) {
				pr_warn
				    ("AudDrv_Allocate_mem_Buffer MemBlock = %d dma_alloc_coherent\n",
				     MemBlock);
				if (Audio_dma_buf[MemBlock]->area == NULL) {
					pr_warn("dma_alloc_coherent\n");
					Audio_dma_buf[MemBlock]->area =
					    dma_alloc_coherent(pDev, Buffer_length,
							       &Audio_dma_buf[MemBlock]->addr,
							       GFP_KERNEL);
					if (Audio_dma_buf[MemBlock]->area)
						Audio_dma_buf[MemBlock]->bytes = Buffer_length;
				}
				pr_warn("area = %p\n", Audio_dma_buf[MemBlock]->area);
			}
		}
		break;
	case Soc_Aud_Digital_Block_MEM_VUL:{
			pr_warn("%s MemBlock =%d Buffer_length = %d\n ", __func__, MemBlock,
			       Buffer_length);
			if (Audio_dma_buf[MemBlock] != NULL) {
				pr_warn
				    ("AudDrv_Allocate_mem_Buffer MemBlock = %d dma_alloc_coherent\n",
				     MemBlock);
				if (Audio_dma_buf[MemBlock]->area == NULL) {
					pr_warn("dma_alloc_coherent\n");
					Audio_dma_buf[MemBlock]->area =
					    dma_alloc_coherent(pDev, Buffer_length,
							       &Audio_dma_buf[MemBlock]->addr,
							       GFP_KERNEL);
					if (Audio_dma_buf[MemBlock]->area)
						Audio_dma_buf[MemBlock]->bytes = Buffer_length;
				}
				pr_warn("area = %p\n", Audio_dma_buf[MemBlock]->area);
			}
			break;
		}
	case Soc_Aud_Digital_Block_MEM_I2S:
		pr_warn("currently not support\n");
		break;
	default:
		pr_warn("%s not support\n", __func__);
		break;
	}
	return true;
}

struct AFE_MEM_CONTROL_T *Get_Mem_ControlT(enum Soc_Aud_Digital_Block MemBlock)
{
	if (MemBlock >= 0 && MemBlock <= Soc_Aud_Digital_Block_MEM_HDMI)
		return AFE_Mem_Control_context[MemBlock];

	pr_warn("%s error\n", __func__);
	return NULL;
}

bool SetMemifSubStream(enum Soc_Aud_Digital_Block MemBlock, struct snd_pcm_substream *substream)
{
	struct substreamList *head;
	struct substreamList *temp = NULL;
	unsigned long flags;

	pr_debug("+%s MemBlock = %d substream = %p\n ", __func__, MemBlock, substream);
	spin_lock_irqsave(&AFE_Mem_Control_context[MemBlock]->substream_lock, flags);
	head = AFE_Mem_Control_context[MemBlock]->substreamL;
	if (head == NULL) {	/* frst item is NULL */
		/* printk("%s head == NULL\n ", __func__); */
		temp = kzalloc(sizeof(struct substreamList), GFP_ATOMIC);
		temp->substream = substream;
		temp->next = NULL;
		AFE_Mem_Control_context[MemBlock]->substreamL = temp;
	} else {		/* find out Null pointer */
		while (head->next != NULL) {
			/* find next */
			head = head->next;
		}
		/* head->next is NULL */
		temp = kzalloc(sizeof(struct substreamList), GFP_ATOMIC);
		temp->substream = substream;
		temp->next = NULL;
		head->next = temp;
	}

	AFE_Mem_Control_context[MemBlock]->MemIfNum++;

	spin_unlock_irqrestore(&AFE_Mem_Control_context[MemBlock]->substream_lock, flags);
	/*pr_warn("-%s MemBlock = %d\n ", __func__, MemBlock);*/
	/* DumpMemifSubStream(); */
	return true;
}

bool ClearMemBlock(enum Soc_Aud_Digital_Block MemBlock)
{
	if (MemBlock >= 0 && MemBlock <= Soc_Aud_Digital_Block_MEM_HDMI) {
		struct AFE_BLOCK_T *pBlock = &AFE_Mem_Control_context[MemBlock]->rBlock;

		memset_io(pBlock->pucVirtBufAddr, 0, pBlock->u4BufferSize);
		pBlock->u4WriteIdx = 0;
		pBlock->u4DMAReadIdx = 0;
		pBlock->u4DataRemained = 0;
		pBlock->u4fsyncflag = false;
		pBlock->uResetFlag = true;
	} else {
		pr_warn("%s error\n", __func__);
		return NULL;
	}
	return true;
}

bool RemoveMemifSubStream(enum Soc_Aud_Digital_Block MemBlock, struct snd_pcm_substream *substream)
{
	struct substreamList *head;
	struct substreamList *temp = NULL;
	unsigned long flags;

	spin_lock_irqsave(&AFE_Mem_Control_context[MemBlock]->substream_lock, flags);

	if (AFE_Mem_Control_context[MemBlock]->MemIfNum == 0)
		pr_warn("%s AFE_Mem_Control_context[%d]->MemIfNum == 0\n ", __func__, MemBlock);
	else
		AFE_Mem_Control_context[MemBlock]->MemIfNum--;

	head = AFE_Mem_Control_context[MemBlock]->substreamL;
	/*pr_warn("+ %s MemBlock = %d substream = %p\n ", __func__, MemBlock, substream);*/
	if (head == NULL) {	/* no object */
		/* do nothing */
	} else {
		/* condition for first item hit */
		if (head->substream == substream) {
			/* printk("%s head->substream = %p\n ", __func__, head->substream); */
			AFE_Mem_Control_context[MemBlock]->substreamL = head->next;

			head->substream = NULL;
			kfree(head);
			head = NULL;
			/* DumpMemifSubStream(); */
		} else {
			temp = head;
			head = head->next;
			while (head) {
				if (head->substream == substream) {
					temp->next = head->next;
					head->substream = NULL;
					kfree(head);
					head = NULL;
					break;
				}
				temp = head;
				head = head->next;
			}
		}
	}
	/* DumpMemifSubStream(); */

	if (AFE_Mem_Control_context[MemBlock]->substreamL == NULL)
		ClearMemBlock(MemBlock);
	else
		pr_warn("%s substreram is not NULL MemBlock = %d\n", __func__, MemBlock);

	spin_unlock_irqrestore(&AFE_Mem_Control_context[MemBlock]->substream_lock, flags);
	/*pr_warn("- %s MemBlock = %d\n ", __func__, MemBlock);*/
	return true;
}

static unsigned long dl1_flags;
void Auddrv_Dl1_Spinlock_lock(void)
{
	spin_lock_irqsave(&auddrv_dl1_lock, dl1_flags);
}

void Auddrv_Dl1_Spinlock_unlock(void)
{
	spin_unlock_irqrestore(&auddrv_dl1_lock, dl1_flags);
}

static unsigned long dl2_flags;
void Auddrv_Dl2_Spinlock_lock(void)
{
	spin_lock_irqsave(&auddrv_dl2_lock, dl2_flags);
}

void Auddrv_Dl2_Spinlock_unlock(void)
{
	spin_unlock_irqrestore(&auddrv_dl2_lock, dl2_flags);
}

static unsigned long ul1_flags;
void Auddrv_UL1_Spinlock_lock(void)
{
	spin_lock_irqsave(&auddrv_ul1_lock, ul1_flags);
}

void Auddrv_UL1_Spinlock_unlock(void)
{
	spin_unlock_irqrestore(&auddrv_ul1_lock, ul1_flags);
}

void Auddrv_HDMI_Interrupt_Handler(void)	/* irq5 ISR handler */
{
}


void Auddrv_AWB_Interrupt_Handler(void)
{
	struct AFE_MEM_CONTROL_T *Mem_Block = AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_AWB];
	uint32_t HW_Cur_ReadIdx = 0;
	uint32_t MaxCopySize = 0;
	int32_t Hw_Get_bytes = 0;
	struct substreamList *temp = NULL;
	struct AFE_BLOCK_T *mBlock = NULL;
	unsigned long flags;
	uint32_t temp_cnt = 0;

	if (Mem_Block == NULL) {
		pr_err("-%s(), Mem_Block == NULL\n ", __func__);
		return;
	}

	spin_lock_irqsave(&Mem_Block->substream_lock, flags);

	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_AWB) == false) {
		/* printk("%s(), GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_AWB) == false, return\n ", __func__); */
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		pr_warn("-%s(), GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_AWB) = %d\n ",
		       __func__, GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_AWB));
		return;
	}

	mBlock = &Mem_Block->rBlock;
#ifdef AUDIO_64BYTE_ALIGN	/* no need to do 64 bytes align */
	HW_Cur_ReadIdx = Align64ByteSize(Afe_Get_Reg(AFE_AWB_CUR));
#else
	HW_Cur_ReadIdx = Afe_Get_Reg(AFE_AWB_CUR);
#endif

	PRINTK_AUD_AWB("+Auddrv_AWB_Interrupt_Handler HW_Cur_ReadIdx = 0x%x\n ", HW_Cur_ReadIdx);

	if (CheckSize(HW_Cur_ReadIdx)) {
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		return;
	}
	if (mBlock->pucVirtBufAddr == NULL) {
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		return;
	}

	MaxCopySize = Get_Mem_MaxCopySize(Soc_Aud_Digital_Block_MEM_AWB);
	PRINTK_AUD_AWB("1  mBlock = %p MaxCopySize = 0x%x u4BufferSize = 0x%x\n",
		       mBlock, MaxCopySize, mBlock->u4BufferSize);
	if (MaxCopySize) {
		if (MaxCopySize > mBlock->u4BufferSize)
			MaxCopySize = mBlock->u4BufferSize;

		mBlock->u4DataRemained -= MaxCopySize;
		mBlock->u4DMAReadIdx += MaxCopySize;
		mBlock->u4DMAReadIdx %= mBlock->u4BufferSize;
		Clear_Mem_CopySize(Soc_Aud_Digital_Block_MEM_AWB);
		PRINTK_AUD_AWB("%s read  ReadIdx:0x%x, WriteIdx:0x%x,BufAddr:0x%x  CopySize =0x%x\n",
			       __func__, mBlock->u4DMAReadIdx, mBlock->u4WriteIdx,
			       mBlock->pucPhysBufAddr, mBlock->u4MaxCopySize);
	}
	/* HW already fill in */
	Hw_Get_bytes = (HW_Cur_ReadIdx - mBlock->pucPhysBufAddr) - mBlock->u4WriteIdx;
	if (Hw_Get_bytes < 0)
		Hw_Get_bytes += mBlock->u4BufferSize;

	PRINTK_AUD_AWB
	    ("%s Get_bytes:0x%x,Cur_ReadIdx:0x%x,ReadIdx:0x%x,WriteIdx:0x%x,BufAddr:0x%x Remained = 0x%x\n",
	     __func__, Hw_Get_bytes, HW_Cur_ReadIdx, mBlock->u4DMAReadIdx, mBlock->u4WriteIdx,
	     mBlock->pucPhysBufAddr, mBlock->u4DataRemained);
	mBlock->u4WriteIdx += Hw_Get_bytes;
	mBlock->u4WriteIdx %= mBlock->u4BufferSize;
	mBlock->u4DataRemained += Hw_Get_bytes;
	/* buffer overflow */
	if (mBlock->u4DataRemained > mBlock->u4BufferSize) {
		pr_warn
		("%s buffer overflow u4DMAReadIdx:%x, u4WriteIdx:%x, u4DataRemained:%x, u4BufferSize:%x\n",
		__func__, mBlock->u4DMAReadIdx, mBlock->u4WriteIdx, mBlock->u4DataRemained,
		mBlock->u4BufferSize);

		mBlock->u4DataRemained %= mBlock->u4BufferSize;
	}
	Mem_Block->interruptTrigger = 1;

	temp = Mem_Block->substreamL;
	while (temp != NULL) {
		if (temp->substream != NULL) {
			temp_cnt = Mem_Block->MemIfNum;

			spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
			snd_pcm_period_elapsed(temp->substream);
			spin_lock_irqsave(&Mem_Block->substream_lock, flags);

			if (temp_cnt != Mem_Block->MemIfNum) {
				pr_warn("%s() temp_cnt = %u, Mem_Block->MemIfNum = %u\n", __func__,
				       temp_cnt, Mem_Block->MemIfNum);
				temp = Mem_Block->substreamL;
			}
		}
		if (temp != NULL) {
			/* find next */
			temp = temp->next;
		}
	}

	PRINTK_AUD_AWB
	    ("-Auddrv_AWB_Interrupt_Handler u4DMAReadIdx:0x%x, u4WriteIdx:0x%x mBlock->u4DataRemained = 0x%x\n",
	     mBlock->u4DMAReadIdx, mBlock->u4WriteIdx, mBlock->u4DataRemained);
	spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
}

void Auddrv_DAI_Interrupt_Handler(void)
{
#if 0				/* not support */

	struct AFE_MEM_CONTROL_T *Mem_Block = AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_DAI];
	uint32_t HW_Cur_ReadIdx = 0;
	int32_t Hw_Get_bytes = 0;
	struct AFE_BLOCK_T *mBlock = NULL;
	unsigned long flags;

	if (Mem_Block == NULL) {
		pr_err("Auddrv_DAI_Interrupt_Handler, no Mem_Block\n");
		return;
	}

	spin_lock_irqsave(&Mem_Block->substream_lock, flags);
	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DAI) == false) {
		/* printk("%s(), GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DAI) == false, return\n ", __func__); */
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		return;
	}

	mBlock = &Mem_Block->rBlock;
	HW_Cur_ReadIdx = Afe_Get_Reg(AFE_DAI_CUR);

	if (CheckSize(HW_Cur_ReadIdx)) {
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		return;
	}
	if (mBlock->pucVirtBufAddr == NULL) {
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		return;
	}
	/* HW already fill in */
	Hw_Get_bytes = (HW_Cur_ReadIdx - mBlock->pucPhysBufAddr) - mBlock->u4WriteIdx;
	if (Hw_Get_bytes < 0) {
		/* recal the size */
		Hw_Get_bytes += mBlock->u4BufferSize;
	}

	PRINTK_AUD_DAI(
	"%s Hw_Get_bytes:0x%x, Cur_ReadIdx:0x%x,ReadIdx:0x%x,WriteIdx:0x%x, PhysAddr:0x%x Block->MemIfNum = %d\n",
	__func__, Hw_Get_bytes, HW_Cur_ReadIdx, mBlock->u4DMAReadIdx, mBlock->u4WriteIdx,
	mBlock->pucPhysBufAddr, Mem_Block->MemIfNum);

	mBlock->u4WriteIdx += Hw_Get_bytes;
	mBlock->u4WriteIdx %= mBlock->u4BufferSize;
	mBlock->u4DataRemained += Hw_Get_bytes;

	/* buffer overflow */
	if (mBlock->u4DataRemained > mBlock->u4BufferSize) {
		PRINTK_AUD_DAI(
			"%s buffer overflow u4DMAReadIdx:%x,WriteIdx:%x, Remained:%x, u4BufferSize:%x\n",
		__func__, mBlock->u4DMAReadIdx, mBlock->u4WriteIdx,
		mBlock->u4DataRemained, mBlock->u4BufferSize);
		/*
		   mBlock->u4DataRemained = mBlock->u4BufferSize / 2;
		   mBlock->u4DMAReadIdx = mBlock->u4WriteIdx - mBlock->u4BufferSize / 2;
		   if (mBlock->u4DMAReadIdx < 0)
		   {
		   mBlock->u4DMAReadIdx += mBlock->u4BufferSize;
		   } */
	}
	Mem_Block->interruptTrigger = 1;

	if (Mem_Block->substreamL != NULL) {
		if (Mem_Block->substreamL->substream != NULL) {
			spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
			snd_pcm_period_elapsed(Mem_Block->substreamL->substream);
			spin_lock_irqsave(&Mem_Block->substream_lock, flags);
		}
	}
	spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
#endif
}

void Auddrv_DL1_Interrupt_Handler(void)	/* irq1 ISR handler */
{
#define MAGIC_NUMBER 0xFFFFFFC0

	struct AFE_MEM_CONTROL_T *Mem_Block = AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_DL1];
	int32_t Afe_consumed_bytes = 0;
	int32_t HW_memory_index = 0;
	int32_t HW_Cur_ReadIdx = 0;
	struct AFE_BLOCK_T *Afe_Block = &(AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_DL1]->rBlock);
	unsigned long flags;

	if (Mem_Block == NULL) {
		pr_err("-%s(), Mem_Block == NULL\n", __func__);
		return;
	}


	spin_lock_irqsave(&Mem_Block->substream_lock, flags);
	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1) == false) {
		/* pr_warn("%s(), GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1) == false, return\n ", __func__); */
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		return;
	}

	HW_Cur_ReadIdx = Afe_Get_Reg(AFE_DL1_CUR);
	if (HW_Cur_ReadIdx == 0) {
		PRINTK_AUDDRV("[Auddrv] HW_Cur_ReadIdx ==0\n");
		HW_Cur_ReadIdx = Afe_Block->pucPhysBufAddr;
	}
	HW_memory_index = (HW_Cur_ReadIdx - Afe_Block->pucPhysBufAddr);

	PRINTK_AUD_DL1
	    ("[Auddrv] HW_Cur_ReadIdx=0x%x HW_memory_index = 0x%x Afe_Block->pucPhysBufAddr = 0x%x\n",
	     HW_Cur_ReadIdx, HW_memory_index, Afe_Block->pucPhysBufAddr);


	/* get hw consume bytes */
	if (HW_memory_index > Afe_Block->u4DMAReadIdx) {
		Afe_consumed_bytes = HW_memory_index - Afe_Block->u4DMAReadIdx;
	} else {
		Afe_consumed_bytes = Afe_Block->u4BufferSize + HW_memory_index -
		    Afe_Block->u4DMAReadIdx;
	}

#ifdef AUDIO_64BYTE_ALIGN	/* no need to do 64 bytes align */
	Afe_consumed_bytes = Afe_consumed_bytes & MAGIC_NUMBER;	/* 64 bytes align */
#endif
	/*
	   if ((Afe_consumed_bytes & 0x1f) != 0)
	   {
	   pr_warn("[Auddrv] DMA address is not aligned 32 bytes\n");
	   } */
	PRINTK_AUD_DL1("+%s ReadIdx:%x WriteIdx:%x,Remained:%x,  consumed_bytes:%x HW_memory_index = %x\n",
	__func__, Afe_Block->u4DMAReadIdx, Afe_Block->u4WriteIdx, Afe_Block->u4DataRemained,
	Afe_consumed_bytes, HW_memory_index);

	if (Afe_Block->u4DataRemained < Afe_consumed_bytes ||
	    Afe_Block->u4DataRemained <= 0 || Afe_Block->u4DataRemained > Afe_Block->u4BufferSize) {
		if (AFE_dL_Abnormal_context.u4UnderflowCnt < DL_ABNORMAL_CONTROL_MAX) {
			AFE_dL_Abnormal_context.pucPhysBufAddr[AFE_dL_Abnormal_context.u4UnderflowCnt] =
									Afe_Block->pucPhysBufAddr;
			AFE_dL_Abnormal_context.u4BufferSize[AFE_dL_Abnormal_context.u4UnderflowCnt] =
									Afe_Block->u4BufferSize;
			AFE_dL_Abnormal_context.u4ConsumedBytes[AFE_dL_Abnormal_context.u4UnderflowCnt] =
									Afe_consumed_bytes;
			AFE_dL_Abnormal_context.u4DataRemained[AFE_dL_Abnormal_context.u4UnderflowCnt] =
									Afe_Block->u4DataRemained;
			AFE_dL_Abnormal_context.u4DMAReadIdx[AFE_dL_Abnormal_context.u4UnderflowCnt] =
									Afe_Block->u4DMAReadIdx;
			AFE_dL_Abnormal_context.u4HwMemoryIndex[AFE_dL_Abnormal_context.u4UnderflowCnt] =
									HW_memory_index;
			AFE_dL_Abnormal_context.u4WriteIdx[AFE_dL_Abnormal_context.u4UnderflowCnt] =
									Afe_Block->u4WriteIdx;
			AFE_dL_Abnormal_context.MemIfNum[AFE_dL_Abnormal_context.u4UnderflowCnt] = MEM_DL1;
		}
		AFE_dL_Abnormal_context.u4UnderflowCnt++;
		if (Afe_Block->u4DataRemained < Afe_consumed_bytes) {
			Afe_Block->u4DMAReadIdx += Afe_consumed_bytes;
			Afe_Block->u4DMAReadIdx %= Afe_Block->u4BufferSize;
			Afe_Block->u4DataRemained = 0;
		}
	} else {

		PRINTK_AUD_DL1("+DL_Handling normal ReadIdx:%x ,DataRemained:%x, WriteIdx:%x\n",
			       Afe_Block->u4DMAReadIdx, Afe_Block->u4DataRemained,
			       Afe_Block->u4WriteIdx);
		Afe_Block->u4DataRemained -= Afe_consumed_bytes;
		Afe_Block->u4DMAReadIdx += Afe_consumed_bytes;
		Afe_Block->u4DMAReadIdx %= Afe_Block->u4BufferSize;
	}
	AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_DL1]->interruptTrigger = 1;
	PRINTK_AUD_DL1("-DL_Handling normal ReadIdx:%x ,DataRemained:%x, WriteIdx:%x\n",
		       Afe_Block->u4DMAReadIdx, Afe_Block->u4DataRemained, Afe_Block->u4WriteIdx);

	if (Mem_Block->substreamL != NULL) {
		if (Mem_Block->substreamL->substream != NULL) {
			spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
			snd_pcm_period_elapsed(Mem_Block->substreamL->substream);
			spin_lock_irqsave(&Mem_Block->substream_lock, flags);
		}
	}
	spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
}

void Auddrv_DL2_Interrupt_Handler(void)	/* irq2 ISR handler */
{
#define MAGIC_NUMBER 0xFFFFFFC0

	struct AFE_MEM_CONTROL_T *Mem_Block = AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_DL2];
	unsigned long flags;

	if (Mem_Block == NULL)
		return;

	Auddrv_Dl2_Spinlock_lock();
	spin_lock_irqsave(&Mem_Block->substream_lock, flags);
	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL2) == false) {
		/* printk("%s(), GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL2) == false, return\n ", __func__); */
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		Auddrv_Dl2_Spinlock_unlock();
		return;
	}

	if (Mem_Block->substreamL != NULL) {
		if (Mem_Block->substreamL->substream != NULL) {
			spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
			snd_pcm_period_elapsed(Mem_Block->substreamL->substream);
			spin_lock_irqsave(&Mem_Block->substream_lock, flags);
		}
	}
	spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);

#ifdef AUDIO_DL2_ISR_COPY_SUPPORT
	mtk_dl2_copy_l();
#endif

	Auddrv_Dl2_Spinlock_unlock();
}


struct snd_dma_buffer *Get_Mem_Buffer(enum Soc_Aud_Digital_Block MemBlock)
{
	pr_debug("%s MemBlock = %d\n", __func__, MemBlock);
	switch (MemBlock) {
	case Soc_Aud_Digital_Block_MEM_DL1:
		return Audio_dma_buf[MemBlock];
	case Soc_Aud_Digital_Block_MEM_DL2:
		return Audio_dma_buf[MemBlock];
	case Soc_Aud_Digital_Block_MEM_VUL:
		return Audio_dma_buf[MemBlock];
	case Soc_Aud_Digital_Block_MEM_DAI:
		return Audio_dma_buf[MemBlock];
	case Soc_Aud_Digital_Block_MEM_AWB:
		return Audio_dma_buf[MemBlock];
	case Soc_Aud_Digital_Block_MEM_MOD_DAI:
		return Audio_dma_buf[MemBlock];
	case Soc_Aud_Digital_Block_MEM_DL1_DATA2:
		return Audio_dma_buf[MemBlock];
	case Soc_Aud_Digital_Block_MEM_VUL_DATA2:
		return Audio_dma_buf[MemBlock];
	case Soc_Aud_Digital_Block_MEM_HDMI:
		return Audio_dma_buf[MemBlock];
	case Soc_Aud_Digital_Block_MEM_I2S:
		pr_warn("currently not support\n");
		break;
	default:
		break;
	}
	return NULL;
}

void Auddrv_UL1_Interrupt_Handler(void)
{
	struct AFE_MEM_CONTROL_T *Mem_Block = AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_VUL];

	uint32_t HW_Cur_ReadIdx = 0;
	int32_t Hw_Get_bytes = 0;
	struct AFE_BLOCK_T *mBlock = NULL;
	unsigned long flags;

	if (Mem_Block == NULL) {
		pr_err("Auddrv_UL1_Interrupt_Handler Mem_Block == NULL\n ");
		return;
	}

	spin_lock_irqsave(&Mem_Block->substream_lock, flags);
	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_VUL) == false) {
		/* pr_warn("%s(), GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_VUL) == false, return\n ", __func__); */
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		return;
	}

	mBlock = &Mem_Block->rBlock;

#ifdef AUDIO_64BYTE_ALIGN	/* no need to do 64 bytes align */
	HW_Cur_ReadIdx = Align64ByteSize(Afe_Get_Reg(AFE_VUL_CUR));
#else
	HW_Cur_ReadIdx = Afe_Get_Reg(AFE_VUL_CUR);
#endif

	PRINTK_AUD_UL1("+Auddrv_UL1_Interrupt_Handler HW_Cur_ReadIdx = 0x%x\n ", HW_Cur_ReadIdx);
	if (CheckSize(HW_Cur_ReadIdx)) {
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		return;
	}
	if (mBlock->pucVirtBufAddr == NULL) {
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		return;
	}
	/* HW already fill in */
	Hw_Get_bytes = (HW_Cur_ReadIdx - mBlock->pucPhysBufAddr) - mBlock->u4WriteIdx;
	if (Hw_Get_bytes < 0) {
		/* recalculate size */
		Hw_Get_bytes += mBlock->u4BufferSize;
	}

	PRINTK_AUD_UL1("%s Get_bytes:%x, Cur_ReadIdx:%x,ReadIdx:%x, WriteIdx:0x%x, BufAddr:%x MemIfNum = %d\n",
	__func__, Hw_Get_bytes, HW_Cur_ReadIdx, mBlock->u4DMAReadIdx, mBlock->u4WriteIdx,
	mBlock->pucPhysBufAddr, Mem_Block->MemIfNum);

	mBlock->u4WriteIdx += Hw_Get_bytes;
	mBlock->u4WriteIdx %= mBlock->u4BufferSize;
	mBlock->u4DataRemained += Hw_Get_bytes;

	/* buffer overflow */
	if (mBlock->u4DataRemained > mBlock->u4BufferSize) {

		pr_warn
		("%s, buffer overflow u4DMAReadIdx:%x, u4WriteIdx:%x, u4DataRemained:%x, u4BufferSize:%x\n",
		__func__, mBlock->u4DMAReadIdx, mBlock->u4WriteIdx, mBlock->u4DataRemained, mBlock->u4BufferSize);
	}
	AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_VUL]->interruptTrigger = 1;

	if (Mem_Block->substreamL != NULL) {
		if (Mem_Block->substreamL->substream != NULL) {
			spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
			snd_pcm_period_elapsed(Mem_Block->substreamL->substream);
			spin_lock_irqsave(&Mem_Block->substream_lock, flags);
		}
	}

	PRINTK_AUD_UL1
	    ("-Auddrv_UL1_Interrupt_Handler u4DMAReadIdx:0x%x, u4WriteIdx:0x%x mBlock->u4DataRemained = 0x%x\n",
	     mBlock->u4DMAReadIdx, mBlock->u4WriteIdx, mBlock->u4DataRemained);
	spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
}

void Clear_Mem_CopySize(enum Soc_Aud_Digital_Block MemBlock)
{
	struct substreamList *head;
	/* unsigned long flags; */
	/* spin_lock_irqsave(&AFE_Mem_Control_context[MemBlock]->substream_lock, flags); */
	head = AFE_Mem_Control_context[MemBlock]->substreamL;
	/* pr_warn("+%s MemBlock = %d\n ", __func__, MemBlock); */
	while (head != NULL) {	/* frst item is NULL */
		head->u4MaxCopySize = 0;
		head = head->next;
	}
	/* spin_unlock_irqrestore(&AFE_Mem_Control_context[MemBlock]->substream_lock, flags); */
	/* pr_warn("-%s MemBlock = %d\n ", __func__, MemBlock); */
}

uint32_t Get_Mem_CopySizeByStream(enum Soc_Aud_Digital_Block MemBlock,
				    struct snd_pcm_substream *substream)
{
	struct substreamList *head;
	unsigned long flags;
	uint32_t MaxCopySize;

	spin_lock_irqsave(&AFE_Mem_Control_context[MemBlock]->substream_lock, flags);
	head = AFE_Mem_Control_context[MemBlock]->substreamL;
	/* pr_warn("+%s MemBlock = %d\n ", __func__, MemBlock); */
	while (head != NULL) {	/* frst item is NULL */
		if (head->substream == substream) {
			MaxCopySize = head->u4MaxCopySize;
			spin_unlock_irqrestore(&AFE_Mem_Control_context[MemBlock]->substream_lock,
					       flags);
			return MaxCopySize;
		}
		head = head->next;
	}
	spin_unlock_irqrestore(&AFE_Mem_Control_context[MemBlock]->substream_lock, flags);
	/* pr_warn("-%s MemBlock = %d\n ", __func__, MemBlock); */
	return 0;
}

uint32_t Get_Mem_MaxCopySize(enum Soc_Aud_Digital_Block MemBlock)
{
	struct substreamList *head;
	/* unsigned long flags; */
	uint32_t MaxCopySize;
	/* spin_lock_irqsave(&AFE_Mem_Control_context[MemBlock]->substream_lock, flags); */
	head = AFE_Mem_Control_context[MemBlock]->substreamL;
	MaxCopySize = 0;
	/* pr_warn("+%s MemBlock = %d\n ", __func__, MemBlock); */
	while (head != NULL) {	/* frst item is NULL */
		if (MaxCopySize < head->u4MaxCopySize)
			MaxCopySize = head->u4MaxCopySize;

		head = head->next;
	}
	/* spin_unlock_irqrestore(&AFE_Mem_Control_context[MemBlock]->substream_lock, flags); */
	/* pr_warn("-%s MemBlock = %d\n ", __func__, MemBlock); */
	return MaxCopySize;
}

void Set_Mem_CopySizeByStream(enum Soc_Aud_Digital_Block MemBlock,
			      struct snd_pcm_substream *substream, unsigned int size)
{
	struct substreamList *head;
	unsigned long flags;

	spin_lock_irqsave(&AFE_Mem_Control_context[MemBlock]->substream_lock, flags);
	head = AFE_Mem_Control_context[MemBlock]->substreamL;
	/* pr_warn("+%s MemBlock = %d\n ", __func__, MemBlock); */
	while (head != NULL) {	/* frst item is NULL */
		if (head->substream == substream) {
			head->u4MaxCopySize += size;
			break;
		}
		head = head->next;
	}
	spin_unlock_irqrestore(&AFE_Mem_Control_context[MemBlock]->substream_lock, flags);
	/* pr_warn("-%s MemBlock = %d\n ", __func__, MemBlock); */
}

void Auddrv_UL2_Interrupt_Handler(void)
{
#if 0				/* not support */
	struct AFE_MEM_CONTROL_T *Mem_Block = AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_VUL_DATA2];
	uint32_t HW_Cur_ReadIdx = 0;
	int32_t Hw_Get_bytes = 0;
	struct AFE_BLOCK_T *mBlock = NULL;
	unsigned long flags;

	PRINTK_AUD_UL2("Auddrv_UL2_Interrupt_Handler\n ");

	if (Mem_Block == NULL) {
		pr_err("Mem_Block == NULL\n");
		return;
	}

	spin_lock_irqsave(&Mem_Block->substream_lock, flags);
	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_VUL_DATA2) == false) {
		/* pr_err("%s(), GetMemoryPathEnable false, return\n ", __func__); */
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		return;
	}

	mBlock = &Mem_Block->rBlock;
	HW_Cur_ReadIdx = Afe_Get_Reg(AFE_VUL_D2_CUR);
	PRINTK_AUD_UL2("Auddrv_UL2_Interrupt_Handler HW_Cur_ReadIdx = 0x%x\n ", HW_Cur_ReadIdx);

	if (CheckSize(HW_Cur_ReadIdx)) {
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		return;
	}
	if (mBlock->pucVirtBufAddr == NULL) {
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		return;
	}
	/* HW already fill in */
	Hw_Get_bytes = (HW_Cur_ReadIdx - mBlock->pucPhysBufAddr) - mBlock->u4WriteIdx;
	if (Hw_Get_bytes < 0)
		Hw_Get_bytes += mBlock->u4BufferSize;

	PRINTK_AUD_UL2("%s Get_bytes:%x, Cur_ReadIdx:%x,RadIdx:%x, WriteIdx:0x%x, BufAddr:%x MemIfNum = %d\n",
	__func_-, Hw_Get_bytes, HW_Cur_ReadIdx, mBlock->u4DMAReadIdx, mBlock->u4WriteIdx,
	mBlock->pucPhysBufAddr, Mem_Block->MemIfNum);

	mBlock->u4WriteIdx += Hw_Get_bytes;
	mBlock->u4WriteIdx %= mBlock->u4BufferSize;
	mBlock->u4DataRemained += Hw_Get_bytes;

	/* buffer overflow */
	if (mBlock->u4DataRemained > mBlock->u4BufferSize) {

		PRINTK_AUD_UL1("%s buffer overflow u4DMAReadIdx:%x,WriteIdx:%x,Remained:%x,Size:%x\n",
		__func__, mBlock->u4DMAReadIdx, mBlock->u4WriteIdx,
		mBlock->u4DataRemained, mBlock->u4BufferSize);
		mBlock->u4DataRemained = mBlock->u4BufferSize / 2;
		mBlock->u4DMAReadIdx = mBlock->u4WriteIdx - mBlock->u4BufferSize / 2;
		if (mBlock->u4DMAReadIdx < 0)
			mBlock->u4DMAReadIdx += mBlock->u4BufferSize;
	}
	AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_VUL_DATA2]->interruptTrigger = 1;

	if (Mem_Block->substreamL != NULL) {
		if (Mem_Block->substreamL->substream != NULL) {
			spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
			snd_pcm_period_elapsed(Mem_Block->substreamL->substream);
			spin_lock_irqsave(&Mem_Block->substream_lock, flags);
		}
	}
	spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
#else
	pr_warn("%s,not support\n ", __func__);
#endif
}


bool BackUp_Audio_Register(void)
{
	AudDrv_Clk_On();
	mAudioRegCache.REG_AUDIO_TOP_CON1 = Afe_Get_Reg(AUDIO_TOP_CON1);
	mAudioRegCache.REG_AUDIO_TOP_CON2 = Afe_Get_Reg(AUDIO_TOP_CON2);
	mAudioRegCache.REG_AUDIO_TOP_CON3 = Afe_Get_Reg(AUDIO_TOP_CON3);
	mAudioRegCache.REG_AFE_DAC_CON0 = Afe_Get_Reg(AFE_DAC_CON0);
	mAudioRegCache.REG_AFE_DAC_CON1 = Afe_Get_Reg(AFE_DAC_CON1);
	mAudioRegCache.REG_AFE_I2S_CON = Afe_Get_Reg(AFE_I2S_CON);
	/* mAudioRegCache.REG_AFE_DAIBT_CON0 = Afe_Get_Reg(AFE_DAIBT_CON0); */
	mAudioRegCache.REG_AFE_CONN0 = Afe_Get_Reg(AFE_CONN0);
	mAudioRegCache.REG_AFE_CONN1 = Afe_Get_Reg(AFE_CONN1);
	mAudioRegCache.REG_AFE_CONN2 = Afe_Get_Reg(AFE_CONN2);
	mAudioRegCache.REG_AFE_CONN3 = Afe_Get_Reg(AFE_CONN3);
	mAudioRegCache.REG_AFE_CONN4 = Afe_Get_Reg(AFE_CONN4);
	mAudioRegCache.REG_AFE_I2S_CON1 = Afe_Get_Reg(AFE_I2S_CON1);
	mAudioRegCache.REG_AFE_I2S_CON2 = Afe_Get_Reg(AFE_I2S_CON2);
	/* mAudioRegCache.REG_AFE_MRGIF_CON = Afe_Get_Reg(AFE_MRGIF_CON); */
	mAudioRegCache.REG_AFE_DL1_BASE = Afe_Get_Reg(AFE_DL1_BASE);
	mAudioRegCache.REG_AFE_DL1_CUR = Afe_Get_Reg(AFE_DL1_CUR);
	mAudioRegCache.REG_AFE_DL1_END = Afe_Get_Reg(AFE_DL1_END);
	/* mAudioRegCache.REG_AFE_DL1_D2_BASE = Afe_Get_Reg(AFE_DL1_D2_BASE); */
	/* mAudioRegCache.REG_AFE_DL1_D2_CUR = Afe_Get_Reg(AFE_DL1_D2_CUR); */
	/* mAudioRegCache.REG_AFE_DL1_D2_END = Afe_Get_Reg(AFE_DL1_D2_END); */
	/* mAudioRegCache.REG_AFE_VUL_D2_BASE = Afe_Get_Reg(AFE_VUL_D2_BASE); */
	/* mAudioRegCache.REG_AFE_VUL_D2_END = Afe_Get_Reg(AFE_VUL_D2_END); */
	/* mAudioRegCache.REG_AFE_VUL_D2_CUR = Afe_Get_Reg(AFE_VUL_D2_CUR); */
	mAudioRegCache.REG_AFE_I2S_CON3 = Afe_Get_Reg(AFE_I2S_CON3);
	mAudioRegCache.REG_AFE_DL2_BASE = Afe_Get_Reg(AFE_DL2_BASE);
	mAudioRegCache.REG_AFE_DL2_CUR = Afe_Get_Reg(AFE_DL2_CUR);
	mAudioRegCache.REG_AFE_DL2_END = Afe_Get_Reg(AFE_DL2_END);
	/* mAudioRegCache.REG_AFE_CONN5 = Afe_Get_Reg(AFE_CONN5); */
	/* mAudioRegCache.REG_AFE_CONN_24BIT = Afe_Get_Reg(AFE_CONN_24BIT); */
	mAudioRegCache.REG_AFE_AWB_BASE = Afe_Get_Reg(AFE_AWB_BASE);
	mAudioRegCache.REG_AFE_AWB_END = Afe_Get_Reg(AFE_AWB_END);
	mAudioRegCache.REG_AFE_AWB_CUR = Afe_Get_Reg(AFE_AWB_CUR);
	mAudioRegCache.REG_AFE_VUL_BASE = Afe_Get_Reg(AFE_VUL_BASE);
	mAudioRegCache.REG_AFE_VUL_END = Afe_Get_Reg(AFE_VUL_END);
	mAudioRegCache.REG_AFE_VUL_CUR = Afe_Get_Reg(AFE_VUL_CUR);
	/* mAudioRegCache.REG_AFE_DAI_BASE = Afe_Get_Reg(AFE_DAI_BASE); */
	/* mAudioRegCache.REG_AFE_DAI_END = Afe_Get_Reg(AFE_DAI_END); */
	/* mAudioRegCache.REG_AFE_DAI_CUR = Afe_Get_Reg(AFE_DAI_CUR); */
	/* mAudioRegCache.REG_AFE_CONN6 = Afe_Get_Reg(AFE_CONN6); */

	/* mAudioRegCache.REG_AFE_MEMIF_MSB = Afe_Get_Reg(AFE_MEMIF_MSB); */

	mAudioRegCache.REG_AFE_ADDA_DL_SRC2_CON0 = Afe_Get_Reg(AFE_ADDA_DL_SRC2_CON0);
	mAudioRegCache.REG_AFE_ADDA_DL_SRC2_CON1 = Afe_Get_Reg(AFE_ADDA_DL_SRC2_CON1);
	mAudioRegCache.REG_AFE_ADDA_UL_SRC_CON0 = Afe_Get_Reg(AFE_ADDA_UL_SRC_CON0);
	mAudioRegCache.REG_AFE_ADDA_UL_SRC_CON1 = Afe_Get_Reg(AFE_ADDA_UL_SRC_CON1);
	mAudioRegCache.REG_AFE_ADDA_TOP_CON0 = Afe_Get_Reg(AFE_ADDA_TOP_CON0);
	mAudioRegCache.REG_AFE_ADDA_UL_DL_CON0 = Afe_Get_Reg(AFE_ADDA_UL_DL_CON0);

	mAudioRegCache.REG_AFE_ADDA_NEWIF_CFG0 = Afe_Get_Reg(AFE_ADDA_NEWIF_CFG0);
	mAudioRegCache.REG_AFE_ADDA_NEWIF_CFG1 = Afe_Get_Reg(AFE_ADDA_NEWIF_CFG1);

	mAudioRegCache.REG_AFE_SIDETONE_CON0 = Afe_Get_Reg(AFE_SIDETONE_CON0);
	mAudioRegCache.REG_AFE_SIDETONE_COEFF = Afe_Get_Reg(AFE_SIDETONE_COEFF);
	mAudioRegCache.REG_AFE_SIDETONE_CON1 = Afe_Get_Reg(AFE_SIDETONE_CON1);
	mAudioRegCache.REG_AFE_SIDETONE_GAIN = Afe_Get_Reg(AFE_SIDETONE_GAIN);
	mAudioRegCache.REG_AFE_SGEN_CON0 = Afe_Get_Reg(AFE_SGEN_CON0);
	mAudioRegCache.REG_AFE_TOP_CON0 = Afe_Get_Reg(AFE_TOP_CON0);
	mAudioRegCache.REG_AFE_ADDA_PREDIS_CON0 = Afe_Get_Reg(AFE_ADDA_PREDIS_CON0);
	mAudioRegCache.REG_AFE_ADDA_PREDIS_CON1 = Afe_Get_Reg(AFE_ADDA_PREDIS_CON1);

	mAudioRegCache.REG_AFE_MOD_DAI_BASE = Afe_Get_Reg(AFE_MOD_DAI_BASE);
	mAudioRegCache.REG_AFE_MOD_DAI_END = Afe_Get_Reg(AFE_MOD_DAI_END);
	mAudioRegCache.REG_AFE_MOD_DAI_CUR = Afe_Get_Reg(AFE_MOD_DAI_CUR);
	mAudioRegCache.REG_AFE_IRQ_MCU_CON = Afe_Get_Reg(AFE_IRQ_MCU_CON);
	mAudioRegCache.REG_AFE_IRQ_MCU_CNT1 = Afe_Get_Reg(AFE_IRQ_MCU_CNT1);
	mAudioRegCache.REG_AFE_IRQ_MCU_CNT2 = Afe_Get_Reg(AFE_IRQ_MCU_CNT2);
	/* mAudioRegCache.REG_AFE_IRQ_MCU_EN = Afe_Get_Reg(AFE_IRQ_MCU_EN); */
	mAudioRegCache.REG_AFE_MEMIF_MAXLEN = Afe_Get_Reg(AFE_MEMIF_MAXLEN);
	mAudioRegCache.REG_AFE_MEMIF_PBUF_SIZE = Afe_Get_Reg(AFE_MEMIF_PBUF_SIZE);
	/* mAudioRegCache.REG_AFE_IRQ_MCU_CNT7 = Afe_Get_Reg(AFE_IRQ_MCU_CNT7); */

	/* mAudioRegCache.REG_AFE_APLL1_TUNER_CFG = Afe_Get_Reg(AFE_APLL1_TUNER_CFG); */
	/* mAudioRegCache.REG_AFE_APLL2_TUNER_CFG = Afe_Get_Reg(AFE_APLL2_TUNER_CFG); */
	mAudioRegCache.REG_AFE_GAIN1_CON0 = Afe_Get_Reg(AFE_GAIN1_CON0);
	mAudioRegCache.REG_AFE_GAIN1_CON1 = Afe_Get_Reg(AFE_GAIN1_CON1);
	mAudioRegCache.REG_AFE_GAIN1_CON2 = Afe_Get_Reg(AFE_GAIN1_CON2);
	mAudioRegCache.REG_AFE_GAIN1_CON3 = Afe_Get_Reg(AFE_GAIN1_CON3);
	mAudioRegCache.REG_AFE_GAIN1_CONN = Afe_Get_Reg(AFE_GAIN1_CONN);
	mAudioRegCache.REG_AFE_GAIN1_CUR = Afe_Get_Reg(AFE_GAIN1_CUR);
	mAudioRegCache.REG_AFE_GAIN2_CON0 = Afe_Get_Reg(AFE_GAIN1_CON0);
	mAudioRegCache.REG_AFE_GAIN2_CON1 = Afe_Get_Reg(AFE_GAIN1_CON1);
	mAudioRegCache.REG_AFE_GAIN2_CON2 = Afe_Get_Reg(AFE_GAIN1_CON2);
	mAudioRegCache.REG_AFE_GAIN2_CON3 = Afe_Get_Reg(AFE_GAIN1_CON3);
	mAudioRegCache.REG_AFE_GAIN2_CONN = Afe_Get_Reg(AFE_GAIN1_CONN);
	mAudioRegCache.REG_AFE_GAIN2_CUR = Afe_Get_Reg(AFE_GAIN2_CUR);
	mAudioRegCache.REG_AFE_GAIN2_CONN2 = Afe_Get_Reg(AFE_GAIN2_CONN2);
	/* mAudioRegCache.REG_AFE_GAIN2_CONN3 = Afe_Get_Reg(AFE_GAIN2_CONN3); */
	/* mAudioRegCache.REG_AFE_GAIN1_CONN2 = Afe_Get_Reg(AFE_GAIN1_CONN2); */
	/* mAudioRegCache.REG_AFE_GAIN1_CONN3 = Afe_Get_Reg(AFE_GAIN1_CONN3); */
	/* mAudioRegCache.REG_AFE_CONN7 = Afe_Get_Reg(AFE_CONN7); */
	/* mAudioRegCache.REG_AFE_CONN8 = Afe_Get_Reg(AFE_CONN8); */
	/* mAudioRegCache.REG_AFE_CONN9 = Afe_Get_Reg(AFE_CONN9); */
	/* mAudioRegCache.REG_AFE_CONN10 = Afe_Get_Reg(AFE_CONN10); */

	mAudioRegCache.REG_FPGA_CFG2 = Afe_Get_Reg(FPGA_CFG2);
	mAudioRegCache.REG_FPGA_CFG3 = Afe_Get_Reg(FPGA_CFG3);
	mAudioRegCache.REG_FPGA_CFG0 = Afe_Get_Reg(FPGA_CFG0);
	mAudioRegCache.REG_FPGA_CFG1 = Afe_Get_Reg(FPGA_CFG1);

	mAudioRegCache.REG_AFE_ASRC_CON0 = Afe_Get_Reg(AFE_ASRC_CON0);
	mAudioRegCache.REG_AFE_ASRC_CON1 = Afe_Get_Reg(AFE_ASRC_CON1);
	mAudioRegCache.REG_AFE_ASRC_CON2 = Afe_Get_Reg(AFE_ASRC_CON2);
	mAudioRegCache.REG_AFE_ASRC_CON3 = Afe_Get_Reg(AFE_ASRC_CON3);
	mAudioRegCache.REG_AFE_ASRC_CON4 = Afe_Get_Reg(AFE_ASRC_CON4);
	mAudioRegCache.REG_AFE_ASRC_CON5 = Afe_Get_Reg(AFE_ASRC_CON5);
	mAudioRegCache.REG_AFE_ASRC_CON6 = Afe_Get_Reg(AFE_ASRC_CON6);
	mAudioRegCache.REG_AFE_ASRC_CON7 = Afe_Get_Reg(AFE_ASRC_CON7);
	mAudioRegCache.REG_AFE_ASRC_CON8 = Afe_Get_Reg(AFE_ASRC_CON8);
	mAudioRegCache.REG_AFE_ASRC_CON9 = Afe_Get_Reg(AFE_ASRC_CON9);
	mAudioRegCache.REG_AFE_ASRC_CON10 = Afe_Get_Reg(AFE_ASRC_CON10);
	mAudioRegCache.REG_AFE_ASRC_CON11 = Afe_Get_Reg(AFE_ASRC_CON11);
	mAudioRegCache.REG_PCM_INTF_CON = Afe_Get_Reg(PCM_INTF_CON);
	mAudioRegCache.REG_PCM_INTF_CON2 = Afe_Get_Reg(PCM_INTF_CON2);
	mAudioRegCache.REG_PCM2_INTF_CON = Afe_Get_Reg(PCM2_INTF_CON);

	mAudioRegCache.REG_AFE_ASRC_CON13 = Afe_Get_Reg(AFE_ASRC_CON13);
	mAudioRegCache.REG_AFE_ASRC_CON14 = Afe_Get_Reg(AFE_ASRC_CON14);
	mAudioRegCache.REG_AFE_ASRC_CON15 = Afe_Get_Reg(AFE_ASRC_CON15);
	mAudioRegCache.REG_AFE_ASRC_CON16 = Afe_Get_Reg(AFE_ASRC_CON16);
	mAudioRegCache.REG_AFE_ASRC_CON17 = Afe_Get_Reg(AFE_ASRC_CON17);
	mAudioRegCache.REG_AFE_ASRC_CON18 = Afe_Get_Reg(AFE_ASRC_CON18);
	mAudioRegCache.REG_AFE_ASRC_CON19 = Afe_Get_Reg(AFE_ASRC_CON19);
	mAudioRegCache.REG_AFE_ASRC_CON20 = Afe_Get_Reg(AFE_ASRC_CON20);
	mAudioRegCache.REG_AFE_ASRC_CON21 = Afe_Get_Reg(AFE_ASRC_CON21);

	AudDrv_Clk_Off();
	return true;
}

bool Restore_Audio_Register(void)
{
	return true;
}

unsigned int Align64ByteSize(unsigned int insize)
{
#define MAGIC_NUMBER 0xFFFFFFC0

	unsigned int align_size;

	align_size = insize & MAGIC_NUMBER;
	return align_size;
}

bool AudDrv_checkDLISRStatus(void)
{
	unsigned long flags1;
	struct AFE_DL_ABNORMAL_CONTROL_T localctl;
	bool dumplog = false;

	spin_lock_irqsave(&afe_dl_abnormal_context_lock, flags1);
	if (AFE_dL_Abnormal_context.IrqDelayCnt || AFE_dL_Abnormal_context.u4UnderflowCnt) {
		memcpy((void *)&localctl, (void *)&AFE_dL_Abnormal_context, sizeof(struct AFE_DL_ABNORMAL_CONTROL_T));
		dumplog = true;
		if (AFE_dL_Abnormal_context.IrqDelayCnt > 0)
			AFE_dL_Abnormal_context.IrqDelayCnt = 0;

		if (AFE_dL_Abnormal_context.u4UnderflowCnt > 0)
			AFE_dL_Abnormal_context.u4UnderflowCnt = 0;
	}
	spin_unlock_irqrestore(&afe_dl_abnormal_context_lock, flags1);

	if (dumplog) {
		int index = 0;

		if (localctl.IrqDelayCnt) {
			for (index = 0; index < localctl.IrqDelayCnt && index < DL_ABNORMAL_CONTROL_MAX; index++) {
				pr_warn("AudWarn isr blocked [%d/%d] %llu - %llu = %llu > %d ms\n",
					index, localctl.IrqDelayCnt,
					localctl.IrqCurrentTimeNs[index],
					localctl.IrqLastTimeNs[index],
					localctl.IrqIntervalNs[index],
					localctl.IrqIntervalLimitMs[index]);
			}
		}
		if (localctl.u4UnderflowCnt) {
			for (index = 0; index < localctl.u4UnderflowCnt && index < DL_ABNORMAL_CONTROL_MAX; index++) {
				pr_warn("AudWarn data underflow [%d/%d] MemType %d, Remain:0x%x, R:0x%x,",
					index,
					localctl.u4UnderflowCnt,
					localctl.MemIfNum[index],
					localctl.u4DataRemained[index],
					localctl.u4DMAReadIdx[index]);
				pr_warn("W:0x%x, BufSize:0x%x, consumebyte:0x%x, hw index:0x%x, addr:0x%x\n",
					localctl.u4WriteIdx[index],
					localctl.u4BufferSize[index],
					localctl.u4ConsumedBytes[index],
					localctl.u4HwMemoryIndex[index],
					localctl.pucPhysBufAddr[index]);
			}
		}
	}
	return dumplog;
}

/* IRQ Manager */
static int enable_aud_irq(const struct irq_user *_irq_user,
			  enum Soc_Aud_IRQ_MCU_MODE _irq,
			  unsigned int _rate,
			  unsigned int _count)
{
	SetIrqMcuSampleRate(_irq, _rate);
	SetIrqMcuCounter(_irq, _count);
	SetIrqEnable(_irq, true);

	irq_managers[_irq].is_on = true;
	irq_managers[_irq].rate = _rate;
	irq_managers[_irq].count = _count;
	irq_managers[_irq].selected_user = _irq_user;

	return 0;
}

static int disable_aud_irq(enum Soc_Aud_IRQ_MCU_MODE _irq)
{
	SetIrqEnable(_irq, false);
	SetIrqMcuCounter(_irq, 0);

	irq_managers[_irq].is_on = false;
	irq_managers[_irq].count = 0;
	irq_managers[_irq].selected_user = NULL;
	return 0;
}

static int update_aud_irq(const struct irq_user *_irq_user,
			  enum Soc_Aud_IRQ_MCU_MODE _irq,
			  unsigned int _count)
{
	SetIrqMcuCounter(_irq, _count);
	irq_managers[_irq].count = _count;
	irq_managers[_irq].selected_user = _irq_user;
	return 0;
}

static void dump_irq_manager(void)
{
	struct irq_user *ptr;
	int i;

	for (i = 0; i < Soc_Aud_IRQ_MCU_MODE_NUM_OF_IRQ_MODE; i++) {
		pr_warn("irq_managers[%d], is_on %d, rate %d, count %d, selected_user %p\n",
			i,
			irq_managers[i].is_on,
			irq_managers[i].rate,
			irq_managers[i].count,
			(void *)irq_managers[i].selected_user);

		list_for_each_entry(ptr, &irq_managers[i].users, list) {
			pr_warn("\tirq_user: user %p, rate %d, count %d\n",
				ptr->user,
				ptr->request_rate,
				ptr->request_count);
		}
	}
}

static unsigned int get_tgt_count(unsigned int _rate,
				  unsigned int _count,
				  unsigned int _tgt_rate)
{
	return ((_tgt_rate / 100) * _count) / (_rate / 100);
}

static bool is_tgt_rate_ok(unsigned int _rate,
			   unsigned int _count,
			   unsigned int _tgt_rate)
{
	unsigned int tgt_rate = _tgt_rate / 100;
	unsigned int request_rate = _rate / 100;
	unsigned int target_cnt = get_tgt_count(_rate, _count, _tgt_rate);
	unsigned int val_1 = _count * tgt_rate;
	unsigned int val_2 = target_cnt * request_rate;
	unsigned int val_3 = (IRQ_TOLERANCE_US * tgt_rate * request_rate)
			     / 100;

	if (target_cnt <= 1)
		return false;

	if (val_1 > val_2) {
		if (val_1 - val_2 >= val_3)
			return false;
	} else {
		if (val_2 - val_1 >= val_3)
			return false;
	}

	return true;
}
/*
static bool is_min_rate_ok(unsigned int _rate, unsigned int _count)
{
	return is_tgt_rate_ok(_rate, _count, IRQ_MIN_RATE);
}
*/
static bool is_period_smaller(enum Soc_Aud_IRQ_MCU_MODE _irq,
			      struct irq_user *_user)
{
	const struct irq_user *selected_user = irq_managers[_irq].selected_user;

	if (selected_user != NULL) {
		if (get_tgt_count(_user->request_rate,
				  _user->request_count,
				  IRQ_MAX_RATE) >=
		    get_tgt_count(selected_user->request_rate,
				  selected_user->request_count,
				  IRQ_MAX_RATE))
			return false;
	}

	return true;
}

static const struct irq_user *get_min_period_user(
	enum Soc_Aud_IRQ_MCU_MODE _irq)
{
	struct irq_user *ptr;
	struct irq_user *min_user = NULL;
	unsigned int min_count = IRQ_MAX_RATE;
	unsigned int cur_count;

	if (list_empty(&irq_managers[_irq].users)) {
		pr_err("error, irq_managers[%d].users is empty\n", _irq);
		dump_irq_manager();
		/*pr_err("error, irq_managers[].users is empty\n");*/
	}

	list_for_each_entry(ptr, &irq_managers[_irq].users, list) {
		cur_count = get_tgt_count(ptr->request_rate,
					  ptr->request_count,
					  IRQ_MAX_RATE);
		if (cur_count < min_count) {
			min_count = cur_count;
			min_user = ptr;
		}
	}

	return min_user;
}

static int check_and_update_irq(const struct irq_user *_irq_user,
				enum Soc_Aud_IRQ_MCU_MODE _irq)
{
	if (NULL == _irq_user)
		return -EINVAL;
	if (!is_tgt_rate_ok(_irq_user->request_rate,
			    _irq_user->request_count,
			    irq_managers[_irq].rate)) {
		/* if you got here, you should reconsider your irq usage */
		pr_err("error, irq not updated, irq %d, irq rate %d, rate %d, count %d\n",
			_irq,
			irq_managers[_irq].rate,
			_irq_user->request_rate,
			_irq_user->request_count);
		dump_irq_manager();

		/* mt6797 disable for MP, enable before enter SQC !!!! */
		/* pr_err("error, irq not updated\n"); */

		return -EINVAL;
	}

	update_aud_irq(_irq_user,
		       _irq,
		       get_tgt_count(_irq_user->request_rate,
				     _irq_user->request_count,
				     irq_managers[_irq].rate));

	return 0;
}

int init_irq_manager(void)
{
	int i;

	memset((void *)&irq_managers, 0, sizeof(irq_managers));
	for (i = 0; i < Soc_Aud_IRQ_MCU_MODE_NUM_OF_IRQ_MODE; i++)
		INIT_LIST_HEAD(&irq_managers[i].users);

	return 0;
}

int irq_add_user(const void *_user,
		 enum Soc_Aud_IRQ_MCU_MODE _irq,
		 unsigned int _rate,
		 unsigned int _count)
{
	unsigned long flags;
	struct irq_user *new_user;
	struct irq_user *ptr;

	spin_lock_irqsave(&afe_control_lock, flags);
	/*pr_debug("%s(), user %p, irq %d, rate %d, count %d\n",
		 __func__, _user, _irq, _rate, _count);*/
	/* check if user already exist */
	list_for_each_entry(ptr, &irq_managers[_irq].users, list) {
		if (ptr->user == _user) {
			pr_err("error, _user %p already exist\n", _user);
			dump_irq_manager();
			/*pr_err("error, _user already exist\n");*/
		}
	}

	/* create instance */
	new_user = kzalloc(sizeof(*new_user), GFP_ATOMIC);
	if (!new_user) {
		spin_unlock_irqrestore(&afe_control_lock, flags);
		return -ENOMEM;
	}

	new_user->user = _user;
	new_user->request_rate = _rate;
	new_user->request_count = _count;
	INIT_LIST_HEAD(&new_user->list);

	/* add user to list */
	list_add(&new_user->list, &irq_managers[_irq].users);

	/* */
	if (irq_managers[_irq].is_on) {
		if (is_period_smaller(_irq, new_user))
			check_and_update_irq(new_user, _irq);
	} else {
		enable_aud_irq(new_user,
			       _irq,
			       _rate,
			       _count);
	}

	spin_unlock_irqrestore(&afe_control_lock, flags);
	return 0;
}

int irq_remove_user(const void *_user,
		    enum Soc_Aud_IRQ_MCU_MODE _irq)
{
	unsigned long flags;
	struct irq_user *ptr;
	struct irq_user *corr_user = NULL;

	spin_lock_irqsave(&afe_control_lock, flags);
	/*pr_debug("%s(), user %p, irq %d\n",
		 __func__, _user, _irq);*/
	/* get _user's irq_user ptr */
	list_for_each_entry(ptr, &irq_managers[_irq].users, list) {
		if (ptr->user == _user) {
			corr_user = ptr;
			break;
		}
	}
	if (corr_user == NULL) {
		pr_err("%s(), error, _user not found\n", __func__);
		dump_irq_manager();
		/*pr_err("error, _user not found\n");*/
		spin_unlock_irqrestore(&afe_control_lock, flags);
		return -EINVAL;
	}
	/* remove from irq_handler[_irq].users */
	list_del(&corr_user->list);

	/* check if is selected user */
	if (corr_user == irq_managers[_irq].selected_user) {
		if (list_empty(&irq_managers[_irq].users))
			disable_aud_irq(_irq);
		else
			check_and_update_irq(get_min_period_user(_irq), _irq);
	}
	/* free */
	kfree(corr_user);

	spin_unlock_irqrestore(&afe_control_lock, flags);
	return 0;
}

int irq_update_user(const void *_user,
		    enum Soc_Aud_IRQ_MCU_MODE _irq,
		    unsigned int _rate,
		    unsigned int _count)
{
	unsigned long flags;
	struct irq_user *ptr;
	struct irq_user *corr_user = NULL;

	spin_lock_irqsave(&afe_control_lock, flags);
	pr_debug("%s(), user %p, irq %d, rate %d, count %d\n",
		 __func__, _user, _irq, _rate, _count);
	/* get _user's irq_user ptr */
	list_for_each_entry(ptr, &irq_managers[_irq].users, list) {
		if (ptr->user == _user) {
			corr_user = ptr;
			break;
		}
	}
	if (corr_user == NULL) {
		pr_err("%s(), error, _user not found\n", __func__);
		dump_irq_manager();
		/*pr_err("error, _user not found\n");*/
		spin_unlock_irqrestore(&afe_control_lock, flags);
		return -EINVAL;
	}

	/* if _rate == 0, just update count */
	if (_rate)
		corr_user->request_rate = _rate;

	corr_user->request_count = _count;

	/* update irq user */
	if (corr_user == irq_managers[_irq].selected_user) {
		/* selected user */
		check_and_update_irq(get_min_period_user(_irq), _irq);
	} else {
		/* not selected user */
		if (is_period_smaller(_irq, corr_user))
			check_and_update_irq(corr_user, _irq);
	}

	spin_unlock_irqrestore(&afe_control_lock, flags);
	return 0;
}
/* IRQ Manager END*/

/* low latency debug */
int get_LowLatencyDebug(void)
{
	return LowLatencyDebug;
}

void set_LowLatencyDebug(unsigned int bFlag)
{
	LowLatencyDebug = bFlag;
	pr_warn("%s LowLatencyDebug = %d\n", __func__, LowLatencyDebug);
}

