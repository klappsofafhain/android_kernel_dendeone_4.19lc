// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/smp.h>
/* #include <linux/earlysuspend.h> */
#include <linux/clk-provider.h>
#include <linux/io.h>

/* **** */
/* #include <mach/mt_typedefs.h> */
#include <mt-plat/sync_write.h>
/*#include "mt_clkmgr.h"*/
#include "mt6779_clkmgr.h"
/* #include <mach/mt_dcm.h> */
/* #include <mach/mt_idvfs.h> */ /* Fix when idvfs ready */
/*#include "mt_spm.h"*/
/*#include <mach/mt_spm_mtcmos.h>*/
/* #include <mach/mt_spm_sleep.h> */
/* #include <mach/mt_gpufreq.h> */
/* #include <mach/irqs.h> */

/* #include <mach/upmu_common.h> */
/* #include <mach/upmu_sw.h> */
/* #include <mach/upmu_hw.h> */
/*#include "mt_freqhopping_drv.h"*/

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#endif

#ifdef CONFIG_OF
void __iomem *clk_apmixed_base;
void __iomem *clk_mcucfg_base;
#endif


/************************************************
 **********         log debug          **********
 ************************************************/

/************************************************
 **********      register access       **********
 ************************************************/

#define clk_readl(addr) \
	readl(addr)
    /* DRV_Reg32(addr) */

#define clk_writel(addr, val)   \
	mt_reg_sync_writel(val, addr)

#define clk_setl(addr, val) \
	mt_reg_sync_writel(clk_readl(addr) | (val), addr)

#define clk_clrl(addr, val) \
	mt_reg_sync_writel(clk_readl(addr) & ~(val), addr)

/************************************************
 **********      struct definition     **********
 ************************************************/
/************************************************
 **********      global variablies     **********
 ************************************************/
/************************************************
 **********      spin lock protect     **********
 ************************************************/
/************************************************
 **********          pll part          **********
 ************************************************/
/************************************************
 **********         subsys part        **********
 ************************************************/
/*ARMPLL1*/
#define ARMPLL_LL_CON0           (clk_apmixed_base + 0x200)
#define ARMPLL_LL_CON1           (clk_apmixed_base + 0x204)
#define ARMPLL_LL_PWR_CON0           (clk_apmixed_base + 0x20C)
/*ARMPLL2*/
#define ARMPLL_L_CON0           (clk_apmixed_base + 0x210)
#define ARMPLL_L_CON1           (clk_apmixed_base + 0x214)
#define ARMPLL_L_PWR_CON0           (clk_apmixed_base + 0x21C)
/*CCIPLL*/
#define CCIPLL_CON0           (clk_apmixed_base + 0x2A0)
#define CCIPLL_CON1           (clk_apmixed_base + 0x2A4)
#define CCIPLL_PWR_CON0           (clk_apmixed_base + 0x2AC)
/*MMPLL*/
#define MMPLL_CON0           (clk_apmixed_base + 0x280)
#define MMPLL_CON1           (clk_apmixed_base + 0x284)
#define MMPLL_PWR_CON0           (clk_apmixed_base + 0x28C)
/*GPUPLL*/
#define MFGPLL_CON0           (clk_apmixed_base + 0x250)
#define MFGPLL_CON1           (clk_apmixed_base + 0x254)
#define MFGPLL_PWR_CON0           (clk_apmixed_base + 0x25C)

/* MCUCFG Register */
#define MP0_PLL_DIV_CFG           (clk_mcucfg_base + 0xA2A0) /*ARMPLL_LL*/
#define MP1_PLL_DIV_CFG           (clk_mcucfg_base + 0xA2A4) /*ARMPLL_L*/
#define BUS_PLL_DIV_CFG           (clk_mcucfg_base + 0xA2E0) /*CCIPLL*/

/************************************************
 **********         cg_clk part        **********
 ************************************************/

/************************************************
 **********       initialization       **********
 ************************************************/

/************************************************
 **********       function debug       **********
 ************************************************/
static void clk_dump(void)
{
	pr_notice("[ARMPLL_LL_CON0]=0x%08x\n", clk_readl(ARMPLL_LL_CON0));
	pr_notice("[ARMPLL_LL_CON1]=0x%08x\n", clk_readl(ARMPLL_LL_CON1));
	pr_notice("[MP0_PLL_DIV_CFG]=0x%08x\n", clk_readl(MP0_PLL_DIV_CFG));
	pr_notice("[CCF] ARMPLL(LL): %d\n", mt_get_abist_freq(22));

	pr_notice("[ARMPLL_L_CON0]=0x%08x\n", clk_readl(ARMPLL_L_CON0));
	pr_notice("[ARMPLL_L_CON1]=0x%08x\n", clk_readl(ARMPLL_L_CON1));
	pr_notice("[MP1_PLL_DIV_CFG]=0x%08x\n", clk_readl(MP1_PLL_DIV_CFG));
	pr_notice("[CCF] ARMPLL(L): %d\n", mt_get_abist_freq(20));

	pr_notice("[CCIPLL_CON0]=0x%08x\n", clk_readl(CCIPLL_CON0));
	pr_notice("[CCIPLL_CON1]=0x%08x\n", clk_readl(CCIPLL_CON1));
	pr_notice("[BUS_PLL_DIV_CFG]=0x%08x\n", clk_readl(BUS_PLL_DIV_CFG));
	pr_notice("[CCF] CCIPLL: %d\n", mt_get_abist_freq(49));
}

static char last_cmd[32] = "null";
static int fmeter_read(struct seq_file *m, void *v)
{
	char cmd[sizeof(last_cmd)];
	unsigned int meter, idx = 0;

	strncpy(cmd, last_cmd, sizeof(cmd));
	cmd[sizeof(cmd) - 1] = '\0';

	if (sscanf(cmd, "%d %d", &meter, &idx) == 2) {
		if (meter == 0)
			seq_printf(m, "ckgen(%d): %dKHZ\r\n",
				idx, mt_get_ckgen_freq(idx));
		else
			seq_printf(m, "abist(%d): %dKHZ\r\n",
				idx, mt_get_abist_freq(idx));
	}
	return 0;
}

static int armpll1_fsel_read(struct seq_file *m, void *v)
{
	return 0;
}

static int armpll2_fsel_read(struct seq_file *m, void *v)
{
	return 0;
}

static int ccipll_fsel_read(struct seq_file *m, void *v)
{
	return 0;
}

static int mmpll_fsel_read(struct seq_file *m, void *v)
{
	return 0;
}

static int gpupll_fsel_read(struct seq_file *m, void *v)
{
	return 0;
}

static ssize_t fmeter_write(struct file *file, const char __user *buffer,
				   size_t count, loff_t *data)
{
	int len = 0;

	len = (count < (sizeof(last_cmd) - 1)) ? count : (sizeof(last_cmd) - 1);
	if (copy_from_user(last_cmd, buffer, len))
		return 0;
	last_cmd[len] = '\0';

	return count;
}

static int pll_div_value_map(int index)
{
	int div = 0x08;

	switch (index) {
	case 1:
		div = 0x08;
	break;

	case 2:
		div = 0x0A;
	break;
	case 4:
		div = 0x0B;
	break;
	case 6:
		div = 0x1D;
	break;
	default:
		div = 0x08;
	break;
	}
	return div;
}
#define SDM_PLL_N_INFO_MASK 0x003FFFFF /*N_INFO[21:0]*/
#define ARMPLL_POSDIV_MASK  0x07000000 /*POSDIV[26:24]*/
#define SDM_PLL_N_INFO_CHG  0x80000000
#define ARMPLL_DIV_MASK	    0xFFE1FFFF
#define ARMPLL_DIV_SHIFT    17
static ssize_t armpll1_fsel_write(struct file *file, const char __user *buffer,
				   size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;
	unsigned int ctrl_value = 0;
	int div;
	unsigned int value = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (sscanf(desc, "%x %x", &div, &value) == 2) {
		clk_dump();
		ctrl_value = clk_readl(ARMPLL_LL_CON1);
		ctrl_value &= ~(SDM_PLL_N_INFO_MASK | ARMPLL_POSDIV_MASK);
		ctrl_value |= value & (SDM_PLL_N_INFO_MASK |
			ARMPLL_POSDIV_MASK);
		ctrl_value |= SDM_PLL_N_INFO_CHG;
		clk_writel(ARMPLL_LL_CON1, ctrl_value);
		udelay(20);
		ctrl_value = clk_readl(MP0_PLL_DIV_CFG);
		ctrl_value &= ARMPLL_DIV_MASK;
		ctrl_value |= (pll_div_value_map(div) << ARMPLL_DIV_SHIFT);
		clk_writel(MP0_PLL_DIV_CFG, ctrl_value);
	}
	clk_dump();
	return count;
}

static ssize_t armpll2_fsel_write(struct file *file, const char __user *buffer,
				  size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;
	unsigned int ctrl_value = 0;
	int div;
	unsigned int value;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (sscanf(desc, "%x %x", &div, &value) == 2) {
		clk_dump();
		ctrl_value = clk_readl(ARMPLL_L_CON1);
		ctrl_value &= ~(SDM_PLL_N_INFO_MASK | ARMPLL_POSDIV_MASK);
		ctrl_value |= value & (SDM_PLL_N_INFO_MASK |
			ARMPLL_POSDIV_MASK);
		ctrl_value |= SDM_PLL_N_INFO_CHG;
		clk_writel(ARMPLL_L_CON1, ctrl_value);
		udelay(20);
		ctrl_value = clk_readl(MP1_PLL_DIV_CFG);
		ctrl_value &= ARMPLL_DIV_MASK;
		ctrl_value |= (pll_div_value_map(div) << ARMPLL_DIV_SHIFT);
		clk_writel(MP1_PLL_DIV_CFG, ctrl_value);
	}
	clk_dump();
	return count;
}

static ssize_t ccipll_fsel_write(struct file *file, const char __user *buffer,
				    size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;
	unsigned int ctrl_value = 0;
	int div;
	unsigned int value;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (sscanf(desc, "%x %x", &div, &value) == 2) {
		clk_dump();
		ctrl_value = clk_readl(CCIPLL_CON1);
		ctrl_value &= ~(SDM_PLL_N_INFO_MASK | ARMPLL_POSDIV_MASK);
		ctrl_value |= value & (SDM_PLL_N_INFO_MASK |
			ARMPLL_POSDIV_MASK);
		ctrl_value |= SDM_PLL_N_INFO_CHG;
		clk_writel(CCIPLL_CON1, ctrl_value);
		udelay(20);
		ctrl_value = clk_readl(BUS_PLL_DIV_CFG);
		ctrl_value &= ARMPLL_DIV_MASK;
		ctrl_value |= (pll_div_value_map(div) << ARMPLL_DIV_SHIFT);
		clk_writel(BUS_PLL_DIV_CFG, ctrl_value);
	}
	clk_dump();
	return count;

}

static ssize_t mmpll_fsel_write(struct file *file, const char __user *buffer,
				    size_t count, loff_t *data)
{
		char desc[32];
		int len = 0;
		unsigned int con0_value, con1_value;

		len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
			if (copy_from_user(desc, buffer, len))
				return 0;
		desc[len] = '\0';
		if (sscanf(desc, "%x %x", &con1_value, &con0_value) == 2) {
			clk_writel(MMPLL_CON1, con1_value);
			clk_writel(MMPLL_CON0, con0_value);
			udelay(20);
		}
		return count;
}

static ssize_t gpupll_fsel_write(struct file *file, const char __user *buffer,
				    size_t count, loff_t *data)
{
		char desc[32];
		int len = 0;
		unsigned int con0_value, con1_value;

		len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
			if (copy_from_user(desc, buffer, len))
				return 0;
		desc[len] = '\0';
		if (sscanf(desc, "%x %x", &con1_value, &con0_value) == 2) {
			clk_writel(MFGPLL_CON1, con1_value);
			clk_writel(MFGPLL_CON0, con0_value);
			udelay(20);
		}
		return count;
}

static int mm_clk_speed_dump_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", mt_get_abist_freq(27));
	return 0;
}

static int gpupll_speed_dump_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", mt_get_abist_freq(25));
	return 0;
}

static int univpll_speed_dump_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", mt_get_abist_freq(24));
	return 0;
}

static int proc_fmeter_open(struct inode *inode, struct file *file)
{
	return single_open(file, fmeter_read, NULL);
}

static const struct file_operations fmeter_proc_fops = {
	.owner = THIS_MODULE,
	.open = proc_fmeter_open,
	.read = seq_read,
	.write = fmeter_write,
	.release = single_release,
};
/************ L ********************/
static int proc_armpll1_fsel_open(struct inode *inode, struct file *file)
{
	return single_open(file, armpll1_fsel_read, NULL);
}

static const struct file_operations armpll1_fsel_proc_fops = {
	.owner = THIS_MODULE,
	.open = proc_armpll1_fsel_open,
	.read = seq_read,
	.write = armpll1_fsel_write,
	.release = single_release,
};
/************ LL ********************/
static int proc_armpll2_fsel_open(struct inode *inode, struct file *file)
{
	return single_open(file, armpll2_fsel_read, NULL);
}

static const struct file_operations armpll2_fsel_proc_fops = {
	.owner = THIS_MODULE,
	.open = proc_armpll2_fsel_open,
	.read = seq_read,
	.write = armpll2_fsel_write,
	.release = single_release,
};
/************ CCI ********************/
static int proc_ccipll_fsel_open(struct inode *inode, struct file *file)
{
	return single_open(file, ccipll_fsel_read, NULL);
}

static const struct file_operations ccipll_fsel_proc_fops = {
	.owner = THIS_MODULE,
	.open = proc_ccipll_fsel_open,
	.read = seq_read,
	.write = ccipll_fsel_write,
	.release = single_release,
};
/************ MM ********************/
static int proc_mmpll_fsel_open(struct inode *inode, struct file *file)
{
	return single_open(file, mmpll_fsel_read, NULL);
}

static const struct file_operations mmpll_fsel_proc_fops = {
	.owner = THIS_MODULE,
	.open = proc_mmpll_fsel_open,
	.read = seq_read,
	.write = mmpll_fsel_write,
	.release = single_release,
};
/************ GPU ********************/
static int proc_gpupll_fsel_open(struct inode *inode, struct file *file)
{
	return single_open(file, gpupll_fsel_read, NULL);
}

static const struct file_operations gpupll_fsel_proc_fops = {
	.owner = THIS_MODULE,
	.open = proc_gpupll_fsel_open,
	.read = seq_read,
	.write = gpupll_fsel_write,
	.release = single_release,
};
/************ mm_clk ********************/
static int proc_mm_clk_open(struct inode *inode, struct file *file)
{
	return single_open(file, mm_clk_speed_dump_read, NULL);
}

static const struct file_operations mm_fops = {
	.owner = THIS_MODULE,
	.open = proc_mm_clk_open,
	.read = seq_read,
};
/************ gpupll ********************/
static int proc_gpupll_open(struct inode *inode, struct file *file)
{
	return single_open(file, gpupll_speed_dump_read, NULL);
}

static const struct file_operations gpu_fops = {
	.owner = THIS_MODULE,
	.open = proc_gpupll_open,
	.read = seq_read,
};
/************ univpll ********************/
static int proc_univpll_open(struct inode *inode, struct file *file)
{
	return single_open(file, univpll_speed_dump_read, NULL);
}

static const struct file_operations univ_fops = {
	.owner = THIS_MODULE,
	.open = proc_univpll_open,
	.read = seq_read,
};

void mt_clkmgr_debug_init(void)
{
/*use proc_create*/
	struct proc_dir_entry *entry;
	struct proc_dir_entry *clkmgr_dir;

	clkmgr_dir = proc_mkdir("clkmgr", NULL);
	if (!clkmgr_dir) {
		pr_notice("[%s]: fail to mkdir /proc/clkmgr\n", __func__);
		return;
	}
	entry =
	    proc_create("fmeter", 0664, clkmgr_dir,
			&fmeter_proc_fops);
	entry =
	    proc_create("armpll1_fsel", 0664, clkmgr_dir,
			&armpll1_fsel_proc_fops);
	entry =
	    proc_create("armpll2_fsel", 0664, clkmgr_dir,
			&armpll2_fsel_proc_fops);
	entry =
	    proc_create("ccipll_fsel", 0664, clkmgr_dir,
			&ccipll_fsel_proc_fops);
	entry =
	    proc_create("mmpll_fsel", 0664, clkmgr_dir,
			&mmpll_fsel_proc_fops);
	entry =
	    proc_create("gpupll_fsel", 0664, clkmgr_dir,
			&gpupll_fsel_proc_fops);
	entry =
	    proc_create("mm_speed_dump", 0444, clkmgr_dir,
			&mm_fops);
	entry =
	    proc_create("gpu_speed_dump", 0444, clkmgr_dir,
			&gpu_fops);
	entry =
	    proc_create("univpll_speed_dump", 0444, clkmgr_dir,
			&univ_fops);
}

/*move to other place*/
int univpll_is_used(void)
{
	/*
	 * 0: univpll is not used, sspm can disable
	 * 1: univpll is used, sspm cannot disable
	 */
	struct clk *c = __clk_lookup("univpll");

	return __clk_get_enable_count(c);
}

#ifdef CONFIG_OF
void iomap(void)
{
	struct device_node *node;

/*apmixed*/
	node = of_find_compatible_node(NULL, NULL, "mediatek,mt6779-apmixed");
	if (!node)
		pr_notice("[CLK_APMIXED] find node failed\n");
	clk_apmixed_base = of_iomap(node, 0);
	if (!clk_apmixed_base)
		pr_notice("[CLK_APMIXED] base failed\n");

/*mcucfg*/
	node = of_find_compatible_node(NULL, NULL, "mediatek,mcucfg");
	if (!node)
		pr_notice("[CLK_MCUCFG] find node failed\n");
	clk_mcucfg_base = of_iomap(node, 0);
	if (!clk_mcucfg_base)
		pr_notice("[CLK_MCUCFG] base failed\n");

}
#endif


static int mt_clkmgr_debug_module_init(void)
{
	iomap();
	mt_clkmgr_debug_init();
	return 0;
}

module_init(mt_clkmgr_debug_module_init);
