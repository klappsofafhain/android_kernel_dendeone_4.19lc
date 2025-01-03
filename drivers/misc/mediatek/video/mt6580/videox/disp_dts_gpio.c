// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 *
 */

#include "disp_dts_gpio.h" //4.19
#include <linux/bug.h>

#include <linux/kernel.h> /* printk */
#include <linux/pinctrl/consumer.h>

static struct pinctrl *this_pctrl; /* static pinctrl instance */

/* DTS state mapping name */
static const char *this_state_name[DTS_GPIO_STATE_MAX] = {
	"mode_te_gpio",                 /* DTS_GPIO_STATE_TE_MODE_GPIO */
	"mode_te_te",                   /* DTS_GPIO_STATE_TE_MODE_TE */
	"pwm_test_pin_mux_gpio66",      /* DTS_GPIO_STATE_PWM_TEST_PINMUX_55 */
	"lcm_rst_out0_gpio",
	"lcm_rst_out1_gpio",
	"lcm1_rst_out0_gpio",
	"lcm1_rst_out1_gpio",
	"lcd_bias_enn_gpio",
	"lcd_bias_enp_gpio",
	"lcd_bias_enp0_gpio",
	"lcd_bias_enp1_gpio",
	"lcd_bias_enn0_gpio",
	"lcd_bias_enn1_gpio"
};

/* pinctrl implementation */
static long _set_state(const char *name)
{
	long ret = 0;
	struct pinctrl_state *pState = 0;

	BUG_ON(!this_pctrl);

	pState = pinctrl_lookup_state(this_pctrl, name);
	if (IS_ERR(pState)) {
		pr_err("set state '%s' failed\n", name);
		ret = PTR_ERR(pState);
		goto exit;
	}

	/* select state! */
	pinctrl_select_state(this_pctrl, pState);

exit:
	return ret; /* Good! */
}

long disp_dts_gpio_init(struct platform_device *pdev)
{
	long ret = 0;
	struct pinctrl *pctrl;

	/* retrieve */
	pctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pctrl)) {
		dev_err(&pdev->dev, "Cannot find disp pinctrl!");
		ret = PTR_ERR(pctrl);
		goto exit;
	}

	this_pctrl = pctrl;

exit:
	return ret;
}

long disp_dts_gpio_select_state(enum DTS_GPIO_STATE s)
{
	BUG_ON(!((unsigned int)(s) < (unsigned int)(DTS_GPIO_STATE_MAX)));
	return _set_state(this_state_name[s]);
}

