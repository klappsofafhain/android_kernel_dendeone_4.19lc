#ifndef BUILD_LK
#include <linux/string.h>
#endif
#include "lcm_drv.h"

#ifdef BUILD_LK
	#include <platform/mt_gpio.h>
	#include <string.h>
#elif defined(BUILD_UBOOT)
	#include <asm/arch/mt_gpio.h>
#else
#endif

// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  										480
#define FRAME_HEIGHT 										854

#define REGFLAG_DELAY             							0xFFF
#define REGFLAG_END_OF_TABLE      							0xEEE   // END OF REGISTERS MARKER

#define LCM_DSI_CMD_MODE	0
#define GC9503_LCM_ID       0x9504

//added for lcm detect ,read adc voltage
#define AUXADC_LCM_VOLTAGE_CHANNEL     12
#define MIN_VOLTAGE (0)     // 0mv
#define MAX_VOLTAGE (500) // 500mv
extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int* rawdata);

//#define GC9504_TEST
// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static struct LCM_UTIL_FUNCS lcm_util = { 0 };

#define SET_RESET_PIN(v)    								(lcm_util.set_reset_pin((v)))

#define UDELAY(n) 											(lcm_util.udelay(n))
#define MDELAY(n) 											(lcm_util.mdelay(n))


// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg											lcm_util.dsi_read_reg()
#define read_reg_v2(cmd, buffer, buffer_size)				lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

 struct LCM_setting_table {
    unsigned cmd;
    unsigned char count;
    unsigned char para_list[64];
};
 
static struct LCM_setting_table lcm_initialization_setting[] = {
{0xF0,5,{0x55,0xAA,0x52,0x08,0x00}},
{0xF6,2,{0x5A,0x87}},
{0xC1,1,{0x3F}},
{0xC2,1,{0x0E}},
{0xC6,1,{0xF8}},
{0xC9,1,{0x10}},
{0xCD,1,{0x25}},
{0xF8,1,{0x8A}},
{0xAC,1,{0x65}},
{0x73,2,{0x00,0x44}},
{0x71,1,{0x48}},
{0x72,1,{0x48}},
{0x83,1,{0x93}},
{0x97,1,{0xEE}},
{0xA7,1,{0x47}},
{0xA0,1,{0xDD}},
{0x87, 3,{0x04, 0x03, 0x66}},
{0xFA,4,{0x8F,0x00,0x00,0x04}},
{0xA3,1,{0x22}},
{0xFD,3,{0x28,0x3C,0x00}},
{0x9A,1,{0xc5}},
{0x9B,1,{0x45}},
{0x82,2,{0x47,0x47}},
{0xB1,1,{0x10}},
/*
{0x9A,1,{0xc6}},
{0x9B,1,{0x46}},
{0x82,2,{0x44,0x44}},
{0xB1,1,{0x04}},
*/
{0x7A,2,{0x12,0x1A}},
{0x7B,2,{0x12,0x1A}},
{0x6D,32,{0x00,0x1F,0x19,0x1A,0x0C,0x0A,0x10,0x0E,0x02,0x08,0x1E,0x1E,0x1E,0x1E,0x1E,0x1E,0x1E,0x1E,0x1E,0x1E,0x1E,0x1E,0x07,0x01,0x0D,0x0F,0x09,0x0B,0x1A,0x19,0x1F,0x00}},
{0x60,8,{0x28,0x0C,0x7A,0x7A,0x28,0x0B,0x7A,0x7A}},
{0x63,8,{0x28,0x0A,0x7A,0x7A,0x28,0x09,0x7A,0x7A}},
{0x64,16,{0x28,0x08,0x03,0x5E,0x03,0x03,0x28,0x07,0x03,0x5F,0x03,0x03,0x7A,0x7A,0x7A,0x7A}},
{0x65,16,{0x28,0x06,0x03,0x60,0x03,0x03,0x28,0x05,0x03,0x61,0x03,0x03,0x7A,0x7A,0x7A,0x7A}},
{0x66,16,{0x28,0x04,0x03,0x62,0x03,0x03,0x28,0x03,0x03,0x63,0x03,0x03,0x7A,0x7A,0x7A,0x7A}},
{0x67,16,{0x28,0x02,0x03,0x64,0x03,0x03,0x28,0x01,0x03,0x65,0x03,0x03,0x7A,0x7A,0x7A,0x7A}},
{0x68,13,{0x00,0x08,0x1B,0x08,0x1A,0x68,0x68,0x08,0x1B,0x08,0x1A,0x68,0x68}},
{0x69,7,{0x14,0x22,0x14,0x22,0x44,0x22,0x08}},
{0x6B,1,{0x07}},
{0xD1,52,{0x00,0x0A,0x00,0x0C,0x00,0x0F,0x00,0x12,0x00,0x16,0x00,0x35,0x00,0x6B,0x01,0x40,0x01,0xA0,0x02,0x08,0x02,0x44,0x02,0x96,0x02,0xD1,0x02,0xD3,0x03,0x06,0x03,0x37,0x03,0x53,0x03,0x72,0x03,0x86,0x03,0x9C,0x03,0xA5,0x03,0xAC,0x03,0xB2,0x03,0xBB,0x03,0xCA,0x03,0xFF}},
{0xD2,52,{0x00,0x0A,0x00,0x0C,0x00,0x0F,0x00,0x12,0x00,0x16,0x00,0x35,0x00,0x6B,0x01,0x40,0x01,0xA0,0x02,0x08,0x02,0x44,0x02,0x96,0x02,0xD1,0x02,0xD3,0x03,0x06,0x03,0x37,0x03,0x53,0x03,0x72,0x03,0x86,0x03,0x9C,0x03,0xA5,0x03,0xAC,0x03,0xB2,0x03,0xBB,0x03,0xCA,0x03,0xFF}},
{0xD3,52,{0x00,0x0A,0x00,0x0C,0x00,0x0F,0x00,0x12,0x00,0x16,0x00,0x35,0x00,0x6B,0x01,0x40,0x01,0xA0,0x02,0x08,0x02,0x44,0x02,0x96,0x02,0xD1,0x02,0xD3,0x03,0x06,0x03,0x37,0x03,0x53,0x03,0x72,0x03,0x86,0x03,0x9C,0x03,0xA5,0x03,0xAC,0x03,0xB2,0x03,0xBB,0x03,0xCA,0x03,0xFF}},
{0xD4,52,{0x00,0x0A,0x00,0x0C,0x00,0x0F,0x00,0x12,0x00,0x16,0x00,0x35,0x00,0x6B,0x01,0x40,0x01,0xA0,0x02,0x08,0x02,0x44,0x02,0x96,0x02,0xD1,0x02,0xD3,0x03,0x06,0x03,0x37,0x03,0x53,0x03,0x72,0x03,0x86,0x03,0x9C,0x03,0xA5,0x03,0xAC,0x03,0xB2,0x03,0xBB,0x03,0xCA,0x03,0xFF}},
{0xD5,52,{0x00,0x0A,0x00,0x0C,0x00,0x0F,0x00,0x12,0x00,0x16,0x00,0x35,0x00,0x6B,0x01,0x40,0x01,0xA0,0x02,0x08,0x02,0x44,0x02,0x96,0x02,0xD1,0x02,0xD3,0x03,0x06,0x03,0x37,0x03,0x53,0x03,0x72,0x03,0x86,0x03,0x9C,0x03,0xA5,0x03,0xAC,0x03,0xB2,0x03,0xBB,0x03,0xCA,0x03,0xFF}},
{0xD6,52,{0x00,0x0A,0x00,0x0C,0x00,0x0F,0x00,0x12,0x00,0x16,0x00,0x35,0x00,0x6B,0x01,0x40,0x01,0xA0,0x02,0x08,0x02,0x44,0x02,0x96,0x02,0xD1,0x02,0xD3,0x03,0x06,0x03,0x37,0x03,0x53,0x03,0x72,0x03,0x86,0x03,0x9C,0x03,0xA5,0x03,0xAC,0x03,0xB2,0x03,0xBB,0x03,0xCA,0x03,0xFF}},
{0x11,0,{0x00}},
{REGFLAG_DELAY, 120, {}},
{0x29,0,{0x00}},
{REGFLAG_DELAY, 20, {}},			
};

#if 0
static struct LCM_setting_table lcm_sleep_out_setting[] = {
    // Sleep Out
	{0x11, 0, {0x00}},
    {REGFLAG_DELAY, 120, {}},

    // Display ON
	{0x29, 0, {0x00}},
    {REGFLAG_DELAY, 100, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};
#endif

static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
     
     {0x6C, 1,{0x60}},
	{REGFLAG_DELAY, 20, {}},
	{0xB1, 1,{0x00}},
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
//				MDELAY(10);
       	}
    }
	
}


// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------

static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}


static void lcm_get_params(struct LCM_PARAMS *params)
{
	memset(params, 0, sizeof(struct LCM_PARAMS));
	
    params->type   = LCM_TYPE_DSI;
    
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


static unsigned int lcm_compare_id(void);

static void lcm_init(void)
{
    SET_RESET_PIN(1);
	MDELAY(10);//Must > 5ms
    SET_RESET_PIN(0);
    MDELAY(100);//Must > 5ms
    SET_RESET_PIN(1);
    MDELAY(120);//Must > 50ms
	#if defined(BUILD_LK)
		printf("gc9503_fwvga_dsi_vdo_hsd_yihua lcm_init \n");
	#else
		printk("gc9503_fwvga_dsi_vdo_hsd_yihua lcm_init \n");
	#endif

    push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);

}


static void lcm_suspend(void)
{

	push_table(lcm_deep_sleep_mode_in_setting, sizeof(lcm_deep_sleep_mode_in_setting) / sizeof(struct LCM_setting_table), 1);
	SET_RESET_PIN(1);
     MDELAY(10);//Must > 5ms
 
}


static void lcm_resume(void)
{
    #ifdef BUILD_LK
		printf("%s, LK gc9503_fwvga_dsi_vdo_hsd_yihua debug \n", __func__);
    #else
		printk("%s, kernel gc9503_fwvga_dsi_vdo_hsd_yihua debug \n", __func__);
    #endif
        
	//lcm_compare_id();
	//lcm_initialization_setting[23].para_list[0]+=2;
	//lcm_initialization_setting[21].para_list[0]+=2;
	//lcm_initialization_setting[22].para_list[0]+=2;
	//lcm_initialization_setting[22].para_list[1]+=2;
lcm_init();
		//push_table(lcm_sleep_out_setting, sizeof(lcm_sleep_out_setting) / sizeof(struct LCM_setting_table), 1);
}

//added for lcm detect ,read adc voltage
static unsigned int lcm_compare_id_auxadc(void)
{
    int data[4] = {0,0,0,0};
    int res = 0;
    int rawdata = 0;
    int lcm_vol = 0;
    
    res = IMM_GetOneChannelValue(AUXADC_LCM_VOLTAGE_CHANNEL,data,&rawdata);
    if(res < 0)
    {
        #ifdef BUILD_LK
        printf("%s,gc9503_fwvga_dsi_vdo_hsd_yihua: get adc data error\n",__func__);
        #else
        printk("%s,gc9503_fwvga_dsi_vdo_hsd_yihua: get adc data error\n",__func__);
        #endif
        
        return 0;
    }
    
    lcm_vol = data[0]*1000+data[1]*10;
    
    #ifdef BUILD_LK
    printf("%s,gc9503_fwvga_dsi_vdo_hsd_yihua:lcm_vol=%d,MIN_VOLTAGE=%d,,MAX_VOLTAGE=%d\n",__func__,lcm_vol,MIN_VOLTAGE,MAX_VOLTAGE);
    #else
    printk("%s,gc9503_fwvga_dsi_vdo_hsd_yihua:lcm_vol=%d,MIN_VOLTAGE=%d,MAX_VOLTAGE=%d\n",__func__,lcm_vol,MIN_VOLTAGE,MAX_VOLTAGE);
    #endif
    
    if (lcm_vol >= MIN_VOLTAGE && lcm_vol <= MAX_VOLTAGE)
    {
        #ifdef BUILD_LK
        printf("%s,gc9503_fwvga_dsi_vdo_hsd_yihua return 1.\n",__func__);
        #else
        printk("%s,gc9503_fwvga_dsi_vdo_hsd_yihua return 1.\n",__func__);
        #endif
        return 1;
    }
    #ifdef BUILD_LK
    printf("%s,gc9503_fwvga_dsi_vdo_hsd_yihua return 0.\n",__func__);
    #else
    printk("%s,gc9503_fwvga_dsi_vdo_hsd_yihua return 0.\n",__func__);
    #endif
    return 0;

}

static unsigned int lcm_compare_id(void)
{ 
    int array[4];
    char buffer[4];
    int id=0;
    
    unsigned int id_auxadc = 0;
    
    SET_RESET_PIN(1);  //NOTE:should reset LCM firstly
    MDELAY(5);
    SET_RESET_PIN(0);
    MDELAY(25);
    SET_RESET_PIN(1);
    MDELAY(120);

    ////// for id start
    array[0] = 0x00033700;
    dsi_set_cmdq(array, 1, 1);
    read_reg_v2(0x04, buffer, 3);
    
    id = (buffer[1] << 8) | buffer[2];

    #if defined(BUILD_LK)
    printf("%s,lk gc9503_fwvga_dsi_vdo_hsd_yihua id=0x%x,buffer[0]=%x,buffer[1]=%x,buffer[2]=%x\n",__func__,id, buffer[0], buffer[1], buffer[2]);
    #else
    printk("%s,kernel gc9503_fwvga_dsi_vdo_hsd_yihua id=0x%x,buffer[0]=%x,buffer[1]=%x,buffer[2]=%x\n",__func__,id, buffer[0], buffer[1], buffer[2]);
    #endif
    ////// for id end

    ////// for id_auxadc start
    id_auxadc = lcm_compare_id_auxadc();
    #ifdef BUILD_LK
    printf("%s,lk gc9503_fwvga_dsi_vdo_hsd_yihua lcd_auxadc_id=%d\n",__func__,id_auxadc);
    #else
    printk("%s,kernel gc9503_fwvga_dsi_vdo_hsd_yihua lcd_auxadc_id=%d\n",__func__,id_auxadc);
    #endif
    ////// for id_auxadc end
    
    if((0x9503 == id || 0x9504 == id) && (id_auxadc == 1))
    {
        #ifdef BUILD_LK
        printf("%s,lk gc9503_fwvga_dsi_vdo_hsd_yihua,read lcd id success,return 1\n", __func__);
        #else
        printk("%s,kernel gc9503_fwvga_dsi_vdo_hsd_yihua,read lcd id success,return 1\n", __func__);
        #endif
        
        return 1;
    }
    
    #ifdef BUILD_LK
    printf("%s,lk gc9503_fwvga_dsi_vdo_hsd_yihua id,read lcd id fail,return 0\n", __func__);
    #else
    printk("%s,kernel gc9503_fwvga_dsi_vdo_hsd_yihua,read lcd id fail,return 0\n", __func__);
    #endif
    
    return 0;
}


struct LCM_DRIVER gc9503_fwvga_dsi_vdo_hsd_yihua_lcm_drv =
{
    .name			= "gc9503_fwvga_dsi_vdo_hsd_yihua",
    .set_util_funcs = lcm_set_util_funcs,
    .get_params     = lcm_get_params,
    .init           = lcm_init,
    .suspend        = lcm_suspend,
    .resume         = lcm_resume,
    .compare_id     = lcm_compare_id,
};
