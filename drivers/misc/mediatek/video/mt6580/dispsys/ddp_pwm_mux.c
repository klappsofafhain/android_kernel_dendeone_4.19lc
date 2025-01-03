// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 *
 */
#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <ddp_pwm_mux.h>
#include <ddp_reg.h>
#include <mach/mt_clkmgr.h>

#define PWM_MSG(fmt, arg...) pr_debug("[PWM] " fmt "\n", ##arg)

static unsigned int disp_pwm_get_pwmmux(void)
{
	unsigned int regsrc = 0;

	regsrc = DISP_REG_GET(CLK_MUX_SEL0);

	return regsrc;
}

/*****************************************************************************
 *
 * disp pwm source clock select mux api
 *
*****************************************************************************/
int disp_pwm_set_pwmmux(unsigned int clk_req)
{
	unsigned int regsrc;
	int ret = 0;

	regsrc = disp_pwm_get_pwmmux();
	if (clk_req == 0)
		clkmux_sel(MT_CLKMUX_PWM_MM_MUX_SEL, MT_CG_SYS_26M, "DISP_PWM");
	else if (clk_req == 1)
		clkmux_sel(MT_CLKMUX_PWM_MM_MUX_SEL, MT_CG_UPLL_D12, "DISP_PWM");
	else
		ret = -1;
	PWM_MSG("PWM_MUX %x->%x", regsrc, disp_pwm_get_pwmmux());

	return ret;
}

int disp_pwm_clksource_enable(int clk_req)
{
	return -1;
}

int disp_pwm_clksource_disable(int clk_req)
{
	return -1;
}


