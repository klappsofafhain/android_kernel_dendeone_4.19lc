/* Copyright Statement:
*
* This software/firmware and related documentation ("MediaTek Software") are
* protected under relevant copyright laws. The information contained herein
* is confidential and proprietary to MediaTek Inc. and/or its licensors.
* Without the prior written permission of MediaTek inc. and/or its licensors,
* any reproduction, modification, use or disclosure of MediaTek Software,
* and information contained herein, in whole or in part, shall be strictly prohibited.
*/
/* MediaTek Inc. (C) 2015. All rights reserved.
*
* BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
* THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
* RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
* AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
* NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
* SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
* SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
* THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
* THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
* CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
* SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
* STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
* CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
* AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
* OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
* MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
*
* The following software/firmware and/or related documentation ("MediaTek Software")
* have been modified by MediaTek Inc. All revisions are subject to any receiver\'s
* applicable license agreements with MediaTek Inc.
*/
#ifndef BUILD_LK
#include <linux/string.h>
#endif

#include "lcm_drv.h"




// ---------------------------------------------------------------------------
// Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH (480)
#define FRAME_HEIGHT (854)

#define REGFLAG_DELAY 0XFD
#define REGFLAG_END_OF_TABLE 0xFE // END OF REGISTERS MARKER

#define LCM_DSI_CMD_MODE 0   
#define LCM_ID_OTM8019A                       (0x8019)


#define LCD_MODUL_ID (0x02)

#if 0
#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

static unsigned int lcm_esd_test = FALSE; ///only for ESD test

#endif

// ---------------------------------------------------------------------------
// Local Variables
// ---------------------------------------------------------------------------

static struct LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v) (lcm_util.set_reset_pin((v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))


// ---------------------------------------------------------------------------
// Local Functions
// ---------------------------------------------------------------------------

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd) lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size) 

static struct LCM_setting_table {
unsigned char cmd;
unsigned char count;
unsigned char para_list[64];
}lcm_initialization_setting[],lcm_deep_sleep_mode_in_setting[];

static struct LCM_setting_table lcm_initialization_setting[] = {

{0x00,1,{0x00}},
{0xFF,3,{0x80,0x19,0x01}},
{0x00,1,{0x80}},
{0xFF,2,{0x80,0x19}},
{0x00,1,{0xB4}},
{0xC0,1,{0x00}},
{0x00,1,{0x81}},
{0xC4,1,{0x83}},
{0x00,1,{0x82}},
{0xC5,1,{0xB0}},
{0x00,1,{0x90}},
{0xC5,2,{0x4E,0xC6}},
{0x00,1,{0xB1}},
{0xC5,1,{0xA9}},
{0x00,1,{0x92}},
{0xC5,1,{0x06}},
{0x00,1,{0x93}},
{0xC5,1,{0x06}},
{0x00,1,{0x00}},
{0xD8,2,{0x6f,0x6f}},
{0x00,1,{0x00}},
{0xD9,1,{0x4C}},
{0x00,1,{0x90}},
{0xB3,1,{0x02}},
{0x00,1,{0x92}},
{0xB3,1,{0x45}},
{0x00,1,{0x80}},
{0xC0,9,{0x00,0x5F,0x00,0x10,0x10,0x00,0x5F,0x10,0x10}},
{0x00,1,{0x90}},
{0xC0,6,{0x00,0x15,0x00,0x00,0x00,0x03}},
{0x00,1,{0xA8}},
{0xC0,1,{0x00}},
{0x00,1,{0x80}},
{0xCE,12,{0x87,0x03,0x00,0x86,0x03,0x00,0x85,0x03,0x00,0x84,0x03,0x00}},
{0x00,1,{0x90}},
{0xCE,14,{0x33,0x52,0x00,0x33,0x53,0x00,0x33,0x54,0x00,0x33,0x55,0x00,0x00,0x00}},
{0x00,1,{0xA0}},
{0xCE,14,{0x38,0x05,0x03,0x56,0x00,0x00,0x00,0x38,0x04,0x03,0x57,0x00,0x00,0x00}},
{0x00,1,{0xB0}},
{0xCE,14,{0x38,0x03,0x03,0x58,0x00,0x00,0x00,0x38,0x02,0x03,0x59,0x00,0x00,0x00}},
{0x00,1,{0xC0}},
{0xCE,14,{0x38,0x01,0x03,0x5A,0x00,0x00,0x00,0x38,0x00,0x03,0x5C,0x00,0x00,0x00}},
{0x00,1,{0xD0}},
{0xCE,14,{0x30,0x00,0x03,0x5C,0x00,0x00,0x00,0x30,0x01,0x03,0x5D,0x00,0x00,0x00}},
{0x00,1,{0xC3}},
{0xCB,8,{0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01}},
{0x00,1,{0xDA}},
{0xCB,5,{0x01,0x01,0x01,0x01,0x01}},
{0x00,1,{0xE0}},
{0xCB,3,{0x01,0x01,0x01}},
{0x00,1,{0x83}},
{0xCC,7,{0x03,0x01,0x09,0x0B,0x0D,0x0F,0x05}},
{0x00,1,{0x90}},
{0xCC,1,{0x07}},
{0x00,1,{0xA0}},
{0xCC,8,{0x08,0x06,0x10,0x0e,0x0c,0x0a,0x02,0x04}},
{0x00,1,{0xB3}},
{0xCC,7,{0x06,0x08,0x0A,0x10,0x0E,0x0C,0x04}},
{0x00,1,{0xC0}},
{0xCC,1,{0x02}},
{0x00,1,{0xD0}},
{0xCC,8,{0x01,0x03,0x0b,0x0d,0x0f,0x09,0x07,0x05}},
{0x00,1,{0xC7}},
{0xCF,1,{0x01}},
{0x00,1,{0x00}},                                                                               
{0xE1,20,{0x00,0x08,0x0f,0x1c,0x35,0x4c,0x59,0x8d,0x7c,0x92,0x75,0x63,0x7b,0x66,0x6b,0x62,0x5b,0x4b,0x41,0x00}},
{0x00,1,{0x00}},
{0xE2,20,{0x00,0x08,0x0f,0x1c,0x35,0x4c,0x59,0x8d,0x7b,0x92,0x75,0x64,0x7b,0x65,0x6a,0x62,0x5b,0x4b,0x41,0x00}},
{0x00,1,{0x80}},
{0xC4,1,{0x30}},	
{0x00,1,{0x98}},
{0xC0,1,{0x00}},	
{0xC0,1,{0x06}},	
{0x00,1,{0xb0}},
{0xC1,3,{0x20,0x00,0x00}},
{0x00,1,{0xe1}},
{0xC0,2,{0x40,0x18}},
{0x00,1,{0x80}},
{0xC1,2,{0x03,0x33}},
{0x00,1,{0xA0}},
{0xC1,1,{0xe8}},
{0x00,1,{0x90}},
{0xb6,1,{0xb4}},
{0x00,1,{0x00}},
{0xFF,3,{0xFF,0xFF,0xFF}},

{0x11,0,{}},
{REGFLAG_DELAY,120, {}},
{0x29,0,{}},
{REGFLAG_DELAY,30, {}},

};


#if 0
static struct LCM_setting_table lcm_sleep_out_setting[] = {
    // Sleep Out
    {0x11, 1, {0x00}},
    {REGFLAG_DELAY, 120, {}},

    // Display ON
    {0x29, 1, {0x00}},
    {REGFLAG_DELAY, 20, {}},
    {REGFLAG_END_OF_TABLE, 0x00, {}}
};
#endif

static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
     // Display off sequence
     {0x28, 1, {0x00}},
     {REGFLAG_DELAY, 20, {}},
     // Sleep Mode Ondiv1_real*div2_real
     {0x10, 1, {0x00}},
     {REGFLAG_DELAY, 120, {}},
     {REGFLAG_END_OF_TABLE, 0x00, {}}
};

#if 0
static struct LCM_setting_table lcm_backlight_level_setting[] = {
    {0x51, 1, {0xFF}},
    {0x53, 1, {0x24}},//close dimming
    {0x55, 1, {0x00}},//close cabc
    {REGFLAG_END_OF_TABLE, 0x00, {}}
};
#endif

static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
    unsigned int i;

    for(i = 0; i < count; i++) {

        unsigned cmd;
        cmd = table[i].cmd;

        switch (cmd) {
        case REGFLAG_DELAY :
            MDELAY(table[i].count);
            break;
        case REGFLAG_END_OF_TABLE :
            break;
        default:
            dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
        }
    }
}


// ---------------------------------------------------------------------------
// LCM Driver Implementations
// ---------------------------------------------------------------------------

static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS * util)
{
    memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}


static void lcm_get_params(struct LCM_PARAMS * params)
{
	memset(params, 0, sizeof(struct LCM_PARAMS));
	
		params->type   = LCM_TYPE_DSI;
		params->physical_width =55;
		params->physical_height = 111;

		params->width  = FRAME_WIDTH;
		params->height = FRAME_HEIGHT;

		// enable tearing-free
		params->dbi.te_mode 				= LCM_DBI_TE_MODE_VSYNC_ONLY;
		params->dbi.te_edge_polarity		= LCM_POLARITY_RISING;

#if (LCM_DSI_CMD_MODE)
		params->dsi.mode   = CMD_MODE;
#else
		params->dsi.mode   = SYNC_PULSE_VDO_MODE;
#endif
	
		// DSI
		/* Command mode setting */
		params->dsi.LANE_NUM				= LCM_TWO_LANE;
		//The following defined the fomat for data coming from LCD engine.
		params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
		params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
		params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
		params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

		// Highly depends on LCD driver capability.
		// Not support in MT6573
		params->dsi.packet_size=256;

		// Video mode setting		
		params->dsi.intermediat_buffer_num = 2;

		params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;

		params->dsi.vertical_sync_active				= 6;
		params->dsi.vertical_backporch					= 18;//8
		params->dsi.vertical_frontporch					= 80;//8
		params->dsi.vertical_active_line				= FRAME_HEIGHT; 

		params->dsi.horizontal_sync_active				= 10;//6
		params->dsi.horizontal_backporch				= 40;//37
		params->dsi.horizontal_frontporch				= 40;//37
		params->dsi.horizontal_active_pixel				= FRAME_WIDTH;
/*
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 1;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;		
*/
		params->dsi.ssc_disable = 1;
		params->dsi.PLL_CLOCK=200;

}





static void lcm_init(void)
{
    SET_RESET_PIN(1);
    MDELAY(5);
    SET_RESET_PIN(0);
    MDELAY(20);
    SET_RESET_PIN(1);
    MDELAY(120);

    push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
}


static void lcm_suspend(void)
{
	push_table(lcm_deep_sleep_mode_in_setting, sizeof(lcm_deep_sleep_mode_in_setting) / sizeof(struct LCM_setting_table), 1);
}

static void lcm_resume(void)
{
    lcm_init();
}

static unsigned int lcm_compare_id(void)
{
	unsigned int id=0;
    unsigned char buffer[5]={0};
    unsigned int array[2]={0};  

    SET_RESET_PIN(1);
	MDELAY(5);
    SET_RESET_PIN(0);
    MDELAY(25);
    SET_RESET_PIN(1);
    MDELAY(50);

    array[0]=0x00043902;
    array[1]=0x011980ff;
    dsi_set_cmdq(array, 2, 1); 
    
    array[0]=0x80001500;
    dsi_set_cmdq(array, 1, 1); 
    
    array[0]=0x00033902;
    array[1]=0x001980ff;
    dsi_set_cmdq(array, 2, 1);  
  
    array[0] = 0x00053700;
    dsi_set_cmdq(array, 1, 1);
    read_reg_v2(0xA1, buffer, 5); 

    id = buffer[2]<<8 |buffer[3];
    return (id==LCM_ID_OTM8019A)?1:0;
		
}

struct LCM_DRIVER otm8019a_fwvga_dsi_vdo_hanlong_lcm_drv = 
{
    .name = "otm8019a_fwvga_dsi_vdo_hanlong",
    .set_util_funcs = lcm_set_util_funcs,
    .compare_id = lcm_compare_id,
    .get_params = lcm_get_params,
    .init = lcm_init,
    .suspend = lcm_suspend,
    .resume = lcm_resume,
};

