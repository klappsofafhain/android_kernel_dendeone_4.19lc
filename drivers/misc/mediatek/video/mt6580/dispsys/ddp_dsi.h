// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 *
 */

#ifndef __DSI_DRV_H__
#define __DSI_DRV_H__

#include <linux/types.h>
#include "lcm_drv.h"
#include "ddp_hal.h"
#include "ddp_info.h"
#include "fbconfig_kdebug.h"
#include "ddp_manager.h"

#ifdef __cplusplus
extern "C" {
#endif


extern const struct LCM_UTIL_FUNCS PM_lcm_utils_dsi0;
extern void DSI_manual_enter_HS(cmdqRecHandle cmdq);
extern void DSI_sw_clk_trail_cmdq(int module_idx, cmdqRecHandle cmdq);
extern void DSI_LFR_UPDATE(enum DISP_MODULE_ENUM module, cmdqRecHandle cmdq);
extern void DSI_Set_LFR(enum DISP_MODULE_ENUM module, cmdqRecHandle cmdq, unsigned int mode,
	 unsigned int type, unsigned int enable, unsigned int skip_num);
extern int DSI_LFR_Status_Check(void);
void DSI_ForceConfig(int forceconfig);
int DSI_set_roi(int x, int y);
int DSI_check_roi(void);

/* --------------------------------------------------------------------------- */

#define DSI_CHECK_RET(expr)			\
	do {					\
		enum DSI_STATUS ret = (expr);        \
		ASSERT(DSI_STATUS_OK == ret);   \
	} while (0)

/* --------------------------------------------------------------------------- */

#define		DSI_DCS_SHORT_PACKET_ID_0			0x05
#define		DSI_DCS_SHORT_PACKET_ID_1			0x15
#define		DSI_DCS_LONG_PACKET_ID				0x39
#define		DSI_DCS_READ_PACKET_ID				0x06

#define		DSI_GERNERIC_SHORT_PACKET_ID_1		0x13
#define		DSI_GERNERIC_SHORT_PACKET_ID_2		0x23
#define		DSI_GERNERIC_LONG_PACKET_ID			0x29
#define		DSI_GERNERIC_READ_LONG_PACKET_ID	0x14


#define		DSI_WMEM_CONTI						(0x3C)
#define		DSI_RMEM_CONTI						(0x3E)

/* ESD recovery method for video mode LCM */
#define		METHOD_NONCONTINUOUS_CLK			(0x1)
#define		METHOD_BUS_TURN_AROUND				(0x2)

/* State of DSI engine */
#define		DSI_VDO_VSA_VS_STATE				(0x008)
#define		DSI_VDO_VSA_HS_STATE				(0x010)
#define		DSI_VDO_VSA_VE_STATE				(0x020)
#define		DSI_VDO_VBP_STATE					(0x040)
#define		DSI_VDO_VACT_STATE					(0x080)
#define		DSI_VDO_VFP_STATE					(0x100)

/* --------------------------------------------------------------------------- */

enum DSI_STATUS{
	DSI_STATUS_OK = 0,

	DSI_STATUS_ERROR,
};


enum DSI_INS_TYPE{
	SHORT_PACKET_RW = 0,
	FB_WRITE = 1,
	LONG_PACKET_W = 2,
	FB_READ = 3,
};


enum DSI_CMDQ_BTA{
	DISABLE_BTA = 0,
	ENABLE_BTA = 1,
};


enum DSI_CMDQ_HS{
	LOW_POWER = 0,
	HIGH_SPEED = 1,
};


enum DSI_CMDQ_CL{
	CL_8BITS = 0,
	CL_16BITS = 1,
};


enum DSI_CMDQ_TE{
	DISABLE_TE = 0,
	ENABLE_TE = 1,
};


enum DSI_CMDQ_RPT{
	DISABLE_RPT = 0,
	ENABLE_RPT = 1,
};


struct DSI_CMDQ_CONFG{
	unsigned type:2;
	unsigned BTA:1;
	unsigned HS:1;
	unsigned CL:1;
	unsigned TE:1;
	unsigned Rsv:1;
	unsigned RPT:1;
};


struct DSI_T0_INS{
	unsigned CONFG:8;
	unsigned Data_ID:8;
	unsigned Data0:8;
	unsigned Data1:8;
};

struct DSI_T1_INS{
	unsigned CONFG:8;
	unsigned Data_ID:8;
	unsigned mem_start0:8;
	unsigned mem_start1:8;
};

struct DSI_T2_INS{
	unsigned CONFG:8;
	unsigned Data_ID:8;
	unsigned WC16:16;
	unsigned int *pdata;
};

struct DSI_T3_INS{
	unsigned CONFG:8;
	unsigned Data_ID:8;
	unsigned mem_start0:8;
	unsigned mem_start1:8;
};

struct DSI_PLL_CONFIG{
	uint8_t TXDIV0;
	uint8_t TXDIV1;
	uint32_t SDM_PCW;
	uint8_t SSC_PH_INIT;
	uint16_t SSC_PRD;
	uint16_t SSC_DELTA1;
	uint16_t SSC_DELTA;
};

enum DSI_INTERFACE_ID{
	DSI_INTERFACE_0 = 0,
	DSI_INTERFACE_1,
	DSI_INTERFACE_DUAL,
	DSI_INTERFACE_NUM,
};


void DSI_ChangeClk(enum DISP_MODULE_ENUM module, uint32_t clk);
int32_t DSI_ssc_enable(uint32_t dsi_idx, uint32_t en);
uint32_t PanelMaster_get_CC(uint32_t dsi_idx);
void PanelMaster_set_CC(uint32_t dsi_index, uint32_t enable);
uint32_t PanelMaster_get_dsi_timing(uint32_t dsi_index, enum MIPI_SETTING_TYPE type);
uint32_t PanelMaster_get_TE_status(uint32_t dsi_idx);
void PanelMaster_DSI_set_timing(uint32_t dsi_index, struct MIPI_TIMING timing);
unsigned int PanelMaster_set_PM_enable(unsigned int value);
uint32_t DSI_dcs_read_lcm_reg_v2(enum DISP_MODULE_ENUM module, cmdqRecHandle cmdq, uint8_t cmd,
			       uint8_t *buffer, uint8_t buffer_size);
void *get_dsi_params_handle(uint32_t dsi_idx);

enum DSI_STATUS DSI_BIST_Pattern_Test(enum DISP_MODULE_ENUM module, cmdqRecHandle cmdq, bool enable,
				 unsigned int color);
extern struct DDP_MODULE_DRIVER ddp_driver_dsi0;
extern struct DDP_MODULE_DRIVER ddp_driver_dsi1;
extern struct DDP_MODULE_DRIVER ddp_driver_dsidual;

#ifdef __cplusplus
}
#endif
#endif				/* __DPI_DRV_H__ */