/*  SPDX-License-Identifier: GPL-2.0 */  
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MT_FREQHOPPING_H__
#define __MT_FREQHOPPING_H__

/* #include <linux/xlog.h> */

#define PLATFORM_DEP_DEBUG_PROC_READ
#undef  DISABLE_FREQ_HOPPING	/* Disable all FHCTL function for K2 Bringup */

#define FHTAG "[FH]"
#define VERBOSE_DEBUG 0
#define DEBUG_MSG_ENABLE 0

#if VERBOSE_DEBUG
#define FH_MSG(fmt, args...) \
			printk(FHTAG""fmt" <- %s(): L<%d>  PID<%s><%d>\n", \
			##args , __func__, __LINE__, current->comm, current->pid)

#define FH_MSG_DEBUG   FH_MSG
#else
#if 1				/* log level is 6 xlog */
#define FH_MSG(fmt, args...) pr_debug(fmt, ##args)
#else				/* log level is 4 (printk) */
#define FH_MSG(fmt, args...) printk(FHTAG""fmt"\n", ##args)
#endif

#define FH_MSG_DEBUG(fmt, args...) \
	do { \
		if (DEBUG_MSG_ENABLE) \
			printk(FHTAG""fmt"\n", ##args); \
	} while (0)
#endif



enum FH_FH_STATUS {
	FH_FH_DISABLE = 0,
	FH_FH_ENABLE_SSC,
	FH_FH_ENABLE_DFH,
	FH_FH_ENABLE_DVFS,
};

enum FH_PLL_STATUS {
	FH_PLL_DISABLE = 0,
	FH_PLL_ENABLE = 1
};

/* TODO: FREQ_MODIFIED should not be here */
/* FH_PLL_STATUS_FREQ_MODIFIED = 3 */

enum FH_CMD {
	FH_CMD_ENABLE = 1,
	FH_CMD_DISABLE,
	FH_CMD_ENABLE_USR_DEFINED,
	FH_CMD_DISABLE_USR_DEFINED,
	FH_CMD_INTERNAL_MAX_CMD
};

/* mt6580 */
enum FH_PLL_ID {
	FH_MIN_PLLID = 0,
	FH_ARMCA7_PLLID = FH_MIN_PLLID,
	FH_MAIN_PLLID = 1,
	FH_WH_PLLID = 2,
	FH_MEM_PLLID = 3,
	FH_MAX_PLLID = FH_MEM_PLLID,
	FH_PLL_NUM
};

/* keep track the status of each PLL */
/* TODO: do we need another "uint mode" for Dynamic FH */
struct fh_pll_t {
	unsigned int fh_status;
	unsigned int pll_status;
	unsigned int setting_id;
	unsigned int curr_freq;
	unsigned int user_defined;
};


/* Record the owner of enable freq hopping <==TBD */
struct freqhopping_pll {
	union {
		int mt_pll[FH_PLL_NUM];
		struct {
			int mt_arm_fhpll;
			int mt_main_fhpll;
			int mt_mem_fhpll;
			int mt_msdc_fhpll;
			int mt_mm_fhpll;
			int mt_venc_fhpll;
		};
	};
};

struct freqhopping_ssc {
	unsigned int freq;
	unsigned int dt;
	unsigned int df;
	unsigned int upbnd;
	unsigned int lowbnd;
	unsigned int dds;
};

struct freqhopping_ioctl {
	unsigned int pll_id;
	struct freqhopping_ssc ssc_setting;	/* used only when user-define */
	int result;
};

int freqhopping_config(unsigned int pll_id, unsigned long vco_freq, unsigned int enable);
//in mt_freqhopping_init(void);
void mt_freqhopping_pll_init(void);
int mt_h2l_mempll(void);
int mt_l2h_mempll(void);
int mt_h2l_dvfs_mempll(void);
int mt_l2h_dvfs_mempll(void);
int mt_is_support_DFS_mode(void);
void mt_fh_popod_save(void);
void mt_fh_popod_restore(void);
int mt_fh_dram_overclock(int clk);
int mt_fh_get_dramc(void);
int mt_freqhopping_devctl(unsigned int cmd, void *args);


/* for PLL 695.5 MHz */
#define MMPLL_TARGET1_VCO	2782000
#define MMPLL_TARGET1_DDS	0x001AC000
/* for PLL 598 MHz */
#define MMPLL_TARGET2_VCO	2392000
#define MMPLL_TARGET2_DDS	0x00170000
/* for PLL 494 MHz */
#define MMPLL_TARGET3_VCO	1976000
#define MMPLL_TARGET3_DDS	0x00130000
/* for PLL 396.5 MHz */
#define MMPLL_TARGET4_VCO	1586000
#define MMPLL_TARGET4_DDS	0x000F4000
/* for PLL 299 MHz */
#define MMPLL_TARGET5_VCO	1196000
#define MMPLL_TARGET5_DDS	0x000B8000
/* for 253.5 PLL Hz */
#define MMPLL_TARGET6_VCO	1014000
#define MMPLL_TARGET6_DDS	0x0009C000
int mt_dfs_mmpll(unsigned int target_freq);
/*
 pll = {FH_ARMCA7_PLLID, FH_ARMCA15_PLLID}
*/
int mt_dfs_armpll(unsigned int pll, unsigned int dds);
struct mt_fh_hal_driver *mt_get_fh_hal_drv(void);

#define FH_BUG_ON(x) \
do {    \
		if ((x)) \
			pr_err("BUGON %s:%d %s:%d\n", __func__, __LINE__, current->comm, current->pid); \
} while (0)

#endif				/* !__MT_FREQHOPPING_H__ */
