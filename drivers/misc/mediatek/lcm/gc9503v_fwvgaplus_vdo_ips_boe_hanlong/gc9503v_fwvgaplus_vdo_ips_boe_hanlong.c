#ifndef BUILD_LK
#include <linux/string.h>
#endif
#include "lcm_drv.h"

#ifdef BUILD_LK
    #include <platform/mt_gpio.h>
#else
    //#include <mt-plat/mt_gpio.h>
	//#include "../../../../misc/mediatek/auxadc/mt_auxadc.h"
#endif

// ---------------------------------------------------------------------------
//  Local Constantsq
// ---------------------------------------------------------------------------

#define AUXADC_LCM_VOLTAGE_CHANNEL     1
#define FRAME_WIDTH  (480)
#define FRAME_HEIGHT (960)

#define REGFLAG_DELAY             							0XFFE
#define REGFLAG_END_OF_TABLE      							0xFFF   // END OF REGISTERS MARKER
#define   LCM_DSI_CMD_MODE							(0)
// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static struct LCM_UTIL_FUNCS lcm_util = {0};
//static int g_which_lcm = -1;
extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int* rawdata);
//static unsigned int lcm_compare_id(void);
#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))

// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	        lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)											lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)   				lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)    



struct LCM_setting_table 
{
    unsigned cmd;
    unsigned char count;
    unsigned char para_list[128];
};

						
static struct LCM_setting_table lcm_initialization_setting[] = {

{0xF0, 5,{0x55, 0xAA, 0x52, 0x08, 0x00}},

{0xF6, 2,{0x5A, 0x87}},

{0xC1, 1,{0x3F}},

{0xC2, 1,{0x0E}},

{0xC6, 1,{0xF8}},

{0xC9, 1,{0x10}},

{0xCD, 1,{0x25}},

{0xF8, 1,{0x8A}},

{0xAC, 1,{0x45}},

{0xA7, 1,{0x47}},

{0xA0, 1,{0x88}},

{0xFA, 4,{0x02, 0x02, 0x02, 0x04}},

{0xA3, 1,{0x2E}},

{0xFD, 3,{0x28, 0x3C, 0x00}},

{0x71, 1,{0x48}},

{0x72, 1,{0x48}},

{0x73, 2,{0x00, 0x44}},

{0x97, 1,{0xEE}},

{0x83, 1,{0x93}},


{0x9A, 1,{0x4A}},//9e

{0x9B, 1,{0x22}},//15

{0x82, 2,{0x00, 0x00}},//19

{0x80, 1,{0x1B}},//15

{0xB1, 1,{0x10}},

{0x7A, 2,{0x13, 0x1A}},

{0x7B, 2,{0x13, 0x1A}},

{0x86,4,{0x99,0xa3,0xa3,0x51}},///0x61

{0x6D, 32,{0x00, 0x1d, 0x0A, 0x10, 0x08, 0x1F, 0x1e, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1f, 0x01, 0x0F, 0x09, 0x1d, 0x00}},

{0x64, 16,{0x18, 0x04, 0x03, 0xC7, 0x03, 0x03, 0x18, 0x03, 0x03, 0xC6, 0x03, 0x03,0x0B, 0x7A,0x0B, 0x7A}},

{0x67, 16,{0x18, 0x02, 0x03, 0xC5, 0x03, 0x03, 0x18, 0x01, 0x03, 0xC4, 0x03, 0x03,0x0B, 0x7A,0x0B, 0x7A}},

{0x60, 8,{0x18, 0x06, 0x0B, 0x7A, 0x18, 0x02,0x0B, 0x7A}},

{0x63, 8,{0x18, 0x02, 0x0B, 0x7A, 0x18, 0x05, 0x0B, 0x7A}},

{0x69, 7,{0x14, 0x22, 0x14, 0x22, 0x44, 0x22, 0x08}},

{0xD1, 52,{0x00, 0x00, 0x00, 0x10, 0x00, 0x26, 0x00, 0x3B, 0x00, 0x4B, 0x00, 0x69, 0x00, 0x85, 0x00, 0xB4, 0x00, 0xDE, 0x01, 0x22, 0x01, 0x5A, 0x01, 0xB4, 0x01, 0xFC, 0x01, 0xFE, 0x02, 0x40, 0x02, 0x8E, 0x02, 0xc8, 0x03, 0x1d, 0x03, 0x5c, 0x03, 0xA9, 0x03, 0xC8, 0x03, 0xD8, 0x03, 0xE6, 0x03, 0xEF, 0x03, 0xFE, 0x03, 0xFF}},

{0xD2, 52,{0x00, 0x00, 0x00, 0x10, 0x00, 0x26, 0x00, 0x3B, 0x00, 0x4B, 0x00, 0x69, 0x00, 0x85, 0x00, 0xB4, 0x00, 0xDE, 0x01, 0x22, 0x01, 0x5A, 0x01, 0xB4, 0x01, 0xFC, 0x01, 0xFE, 0x02, 0x40, 0x02, 0x8E, 0x02, 0xc8, 0x03, 0x1d, 0x03, 0x5c, 0x03, 0xA9, 0x03, 0xC8, 0x03, 0xD8, 0x03, 0xE6, 0x03, 0xEF, 0x03, 0xFE, 0x03, 0xFF}},

{0xD3, 52,{0x00, 0x00, 0x00, 0x10, 0x00, 0x26, 0x00, 0x3B, 0x00, 0x4B, 0x00, 0x69, 0x00, 0x85, 0x00, 0xB4, 0x00, 0xDE, 0x01, 0x22, 0x01, 0x5A, 0x01, 0xB4, 0x01, 0xFC, 0x01, 0xFE, 0x02, 0x40, 0x02, 0x8E, 0x02, 0xc8, 0x03, 0x1d, 0x03, 0x5c, 0x03, 0xA9, 0x03, 0xC8, 0x03, 0xD8, 0x03, 0xE6, 0x03, 0xEF, 0x03, 0xFE, 0x03, 0xFF}},

{0xD4, 52,{0x00, 0x00, 0x00, 0x10, 0x00, 0x26, 0x00, 0x3B, 0x00, 0x4B, 0x00, 0x69, 0x00, 0x85, 0x00, 0xB4, 0x00, 0xDE, 0x01, 0x22, 0x01, 0x5A, 0x01, 0xB4, 0x01, 0xFC, 0x01, 0xFE, 0x02, 0x40, 0x02, 0x8E, 0x02, 0xc8, 0x03, 0x1d, 0x03, 0x5c, 0x03, 0xA9, 0x03, 0xC8, 0x03, 0xD8, 0x03, 0xE6, 0x03, 0xEF, 0x03, 0xFE, 0x03, 0xFF}},

{0xD5, 52,{0x00, 0x00, 0x00, 0x10, 0x00, 0x26, 0x00, 0x3B, 0x00, 0x4B, 0x00, 0x69, 0x00, 0x85, 0x00, 0xB4, 0x00, 0xDE, 0x01, 0x22, 0x01, 0x5A, 0x01, 0xB4, 0x01, 0xFC, 0x01, 0xFE, 0x02, 0x40, 0x02, 0x8E, 0x02, 0xc8, 0x03, 0x1d, 0x03, 0x5c, 0x03, 0xA9, 0x03, 0xC8, 0x03, 0xD8, 0x03, 0xE6, 0x03, 0xEF, 0x03, 0xFE, 0x03, 0xFF}},

{0xD6, 52,{0x00, 0x00, 0x00, 0x10, 0x00, 0x26, 0x00, 0x3B, 0x00, 0x4B, 0x00, 0x69, 0x00, 0x85, 0x00, 0xB4, 0x00, 0xDE, 0x01, 0x22, 0x01, 0x5A, 0x01, 0xB4, 0x01, 0xFC, 0x01, 0xFE, 0x02, 0x40, 0x02, 0x8E, 0x02, 0xc8, 0x03, 0x1d, 0x03, 0x5c, 0x03, 0xA9, 0x03, 0xC8, 0x03, 0xD8, 0x03, 0xE6, 0x03, 0xEF, 0x03, 0xFE, 0x03, 0xFF}},


{0x11, 0,{0x00}},
{REGFLAG_DELAY, 120, {}},//Delay 100ms

{0x29, 0,{0x00}},
{REGFLAG_DELAY, 20, {}},//Delay 100ms
	
{REGFLAG_END_OF_TABLE, 0x00, {}}

};


static struct LCM_setting_table lcm_sleep_in_setting[] = {

  {0x6C, 1,{0x60}},
	{REGFLAG_DELAY, 20, {}},
	{0xB1, 1,{0x80}},
	{0xFA, 4,{0x7F, 0x00, 0x00, 0x00}},
	{REGFLAG_DELAY, 20, {}},
	{0x6c,1,{0x50}}, 
	{REGFLAG_DELAY, 10, {}},

	{0x28, 0,{0x00}},	
	{REGFLAG_DELAY, 50, {}},  
	{0x10, 0,{0x00}},
	{REGFLAG_DELAY, 20, {}},
	
         //�µ����
	{0xF0,5,{0x55,0xaa,0x52,0x08,0x00}},
	{0xc2,1,{0xce}},
	{0xc3,1,{0xcd}},
	{0xc6,1,{0xfc}},
	{0xc5,1,{0x03}},
	{0xcd,1,{0x64}},
	{0xc4,1,{0xff}},///REG85 EN
	
	{0xc9,1,{0xcd}},
	{0xF6,2,{0x5a,0x87}},
	{0xFd,3,{0xaa,0xaa, 0x0a}},
	{0xFe,2,{0x6a,0x0a}},
	{0x78,2,{0x2a,0xaa}},
	{0x92,2,{0x17,0x08}},
	{0x77,2,{0xaa,0x2a}},
	{0x76,2,{0xaa,0xaa}},

         {0x84,1,{0x00}},
	{0x78,2,{0x2b,0xba}},
	{0x89,1,{0x73}},
	{0x88,1,{0x3A}},
	{0x85,1,{0xB0}},
	{0x76,2,{0xeb,0xaa}},
	{0x94,1,{0x80}},
	{0x87,3,{0x04,0x07,0x30}},					
	{0x93,1,{0x27}},
	{0xaf,1,{0x02}},

	
	
    {REGFLAG_END_OF_TABLE, 0x00, {}}

};


//static struct LCM_setting_table lcm_backlight_level_setting[] = {
	//{0x51, 1, {0xFF}},
	//{REGFLAG_END_OF_TABLE, 0x00, {}}
//};


static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
	unsigned int i;

    for(i = 0; i < count; i++)
    {
        unsigned cmd;
        cmd = table[i].cmd;
		
        switch (cmd)
        {	
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

static void init_lcm_registers(void)
{

    push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);

}

// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------

static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS * util)
{
    memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}
#if 0
static int adc_read_vol(void)
{
	int adc[1];
	int data[4] ={0,0,0,0};
	int sum = 0;
	int adc_vol=0;
	int num = 0;
	int ret =0;

	for(num=0;num<10;num++)
	{
		ret = IMM_GetOneChannelValue(AUXADC_LCM_VOLTAGE_CHANNEL, data, adc);
		sum+=(data[0]*100+data[1]);
	}
	adc_vol = sum/10;
#if defined(BUILD_LK)
	printf("lfz st7701s test adc_vol is %d\n",adc_vol);
#else
	printk("lfz st7701s test adc_vol is %d\n",adc_vol);
#endif

/*
	if(adc_vol < 30)
	{       	
		printk("lfz st7701s_fwvga_dsi_vdo_jh_hsd_4502 \n");
		return 0;
	}
	else if((adc_vol >= 30) && (adc_vol < 50))
	{
		printk("lfz st7701s_fwvga_dsi_vdo_zgd_hsd_h4504 \n");
		return 1;
	}
	else if((adc_vol >= 50) && (adc_vol < 100))
	{
		printk("lfz st7701s_fwvga_dsi_vdo_gt_cmi_h4504 \n");
		return 2;
	}
	else if((adc_vol >= 100) && (adc_vol < 130))
	{
		printk("lfz st7701s_fwvga_dsi_vdo_hy_cpt_h4504 \n");
		return 3;
	}
*/	
	if(adc_vol < 60)
	{       	
		printk("lfz st7701s_fwvga_dsi_vdo_jh_hsd_4502 \n");//38
		return 0;
	}
	else
	{
		printk("lfz st7701s_fwvga_dsi_vdo_xinqi_hsd_4502 \n");//115
		return 1;
	}
}
#endif
static void lcm_get_params(struct LCM_PARAMS * params)
{
	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type  = LCM_TYPE_DSI;

	params->width = FRAME_WIDTH;
	params->height= FRAME_HEIGHT;
	params->physical_width = 62;
    params->physical_height = 124;

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
    params->dsi.data_format.format	  = LCM_DSI_FORMAT_RGB888;
    
    params->dsi.packet_size=256;
    // Video mode setting		
    params->dsi.intermediat_buffer_num = 2;
    
    params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;
   // params->dsi.cont_clock = 1;  //added by cheguosheng  // Continuous mode   or  not Continuous mode

	params->dsi.vertical_sync_active	= 2;// 3    2  4
	params->dsi.vertical_backporch		= 15;// 20   11  6
	params->dsi.vertical_frontporch		= 8; // 8 1  12 11
	params->dsi.vertical_active_line	= FRAME_HEIGHT; 

	params->dsi.horizontal_sync_active	= 10;// 10 50  20
	params->dsi.horizontal_backporch	= 30;//30  150
	params->dsi.horizontal_frontporch	= 30;//30 150
  
    params->dsi.horizontal_active_pixel				= FRAME_WIDTH;
    params->dsi.PLL_CLOCK				= 200;
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 1;
	params->dsi.lcm_esd_check_table[0].cmd          = 0x0A;
	params->dsi.lcm_esd_check_table[0].count        = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;	
}



static void lcm_suspend(void)
{
//	lcm_compare_id();
//	printk("lcm_suspend g_which_lcm=%d\n", g_which_lcm);
	push_table(lcm_sleep_in_setting, sizeof(lcm_sleep_in_setting) / sizeof(struct LCM_setting_table), 1);

	SET_RESET_PIN(1);	
	MDELAY(20);	
//	SET_RESET_PIN(0);
}



#if (LCM_DSI_CMD_MODE)
static void lcm_update(unsigned int x, unsigned int y,
		       unsigned int width, unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);
	unsigned char y0_MSB = ((y0 >> 8) & 0xFF);
	unsigned char y0_LSB = (y0 & 0xFF);
	unsigned char y1_MSB = ((y1 >> 8) & 0xFF);
	unsigned char y1_LSB = (y1 & 0xFF);

	unsigned int data_array[16];

	data_array[0] = 0x00053902;
	data_array[1] =
	    (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] =
	    (y1_MSB << 24) | (y0_LSB << 16) | (y0_MSB << 8) | 0x2b;
	data_array[2] = (y1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);

}
#endif

extern void DSI_clk_HS_mode(char enter);
static unsigned int lcm_compare_id(void)
{
	unsigned int id=0,id1=0,id2=0;
	unsigned char buffer[5];
	unsigned int array[3]; 
	//int id_adc = 0;
	u8 hw_id0 = 0;
	
	SET_RESET_PIN(1);  //NOTE:should reset LCM firstly
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(120);	

#ifdef BUILD_LK
	DSI_clk_HS_mode(0);
#endif
//	array[0]=0x00063902;
//	array[1]=0x000177FF;
//	array[2]=0x00001000;
//	dsi_set_cmdq(array, 3, 1);
//	MDELAY(10); 

	array[0] = 0x00053700;// set return byte number
//	array[0] = 0x00013700;
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0xA1,  buffer, 5);
//	read_reg_v2(0xDA,  &buffer, 1);	
//	id = buffer[0];
	id = buffer[0]<<8 |buffer[1];
	id1 = buffer[2]<<8 |buffer[3];
	id2 = buffer[4];

	//id_adc = adc_read_vol();
	//mt_set_gpio_mode( GPIO_DISP_ID0_PIN, GPIO_MODE_00);
	//mt_set_gpio_dir( GPIO_DISP_ID0_PIN, GPIO_DIR_IN);
	//hw_id0 = mt_get_gpio_in( GPIO_DISP_ID0_PIN);
	//printf("H811 st7701s_fwvga_chaoyue read hw id pin value hw_id0 = %d\n",hw_id0);
#if defined(BUILD_LK)
	printf("lfz lk -- chaoyue id_adc==1 0x%x , 0x%x , 0x%x \n",id,id1,id2);
#else
	printk("lfz kernel -- st7701s_fwvga_dsi_vdo_xinqi_hsd_4502 id_adc==1 0x%x , 0x%x , 0x%x \n",buffer[0],buffer[1],id);
#endif

	// return 1;
	return ((id == 0x8802) && (hw_id0==0))?1:0;
}

static void lcm_init(void)
{
	SET_RESET_PIN(1);
	MDELAY(20);
	SET_RESET_PIN(0);
	MDELAY(100);
	SET_RESET_PIN(1);
	MDELAY(120);
	
	init_lcm_registers();
}
static void lcm_resume(void)
{
	//printk("lcm_resume g_which_lcm=%d\n", g_which_lcm);
//	lcm_initialization_setting[14].para_list[0]+=2; 
//	lcm_initialization_setting[15].para_list[0]+=2; 
//	lcm_initialization_setting[16].para_list[0]+=2; 
//	lcm_initialization_setting[16].para_list[1]+=2;	
	lcm_init();
	
//	push_table(lcm_sleep_out_setting, sizeof(lcm_sleep_out_setting) / sizeof(struct LCM_setting_table), 1);
}

//  Get LCM Driver Hooks
// ---------------------------------------------------------------------------
struct LCM_DRIVER gc9503v_fwvgaplus_vdo_ips_boe_hanlong_lcm_drv = {
	.name			= "gc9503v_fwvgaplus_vdo_ips_boe_hanlong",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params		= lcm_get_params,
	.init			= lcm_init,
	.suspend		= lcm_suspend,
	.resume			= lcm_resume,
	.compare_id		= lcm_compare_id,
#if (LCM_DSI_CMD_MODE)
	.set_backlight	= lcm_setbacklight,
    .update         = lcm_update,
#endif
};
