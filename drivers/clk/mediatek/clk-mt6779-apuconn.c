// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Wendell Lin <wendell.lin@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/platform_device.h>
#include <dt-bindings/clock/mt6779-clk.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#define MT_CLKMGR_MODULE_INIT	0
#define CCF_SUBSYS_DEBUG		1

static const struct mtk_gate_regs apuconn_cg_regs = {
	.set_ofs = 0x0004,
	.clr_ofs = 0x0008,
	.sta_ofs = 0x0000,
};

#define GATE_APU_CONN_DUMMY(_id, _name, _parent, _shift) {	\
	.id = _id,				\
	.name = _name,				\
	.parent_name = _parent,			\
	.regs = &apuconn_cg_regs,		\
	.shift = _shift,			\
	.ops = &mtk_clk_gate_ops_setclr_dummy,	\
}


static const struct mtk_gate apuconn_clks[] = {
	GATE_APU_CONN_DUMMY(CLK_APU_CONN_APU, "apu_conn_apu", "dsp1_sel", 0),
	GATE_APU_CONN_DUMMY(CLK_APU_CONN_AHB, "apu_conn_ahb", "dsp_sel", 1),
	GATE_APU_CONN_DUMMY(CLK_APU_CONN_AXI, "apu_conn_axi", "dsp_sel", 2),
	GATE_APU_CONN_DUMMY(CLK_APU_CONN_ISP, "apu_conn_isp", "dsp_sel", 3),
	GATE_APU_CONN_DUMMY(CLK_APU_CONN_CAM_ADL, "apu_conn_cam_adl",
		"dsp_sel", 4),
	GATE_APU_CONN_DUMMY(CLK_APU_CONN_IMG_ADL, "apu_conn_img_adl",
		"dsp_sel", 5),
	GATE_APU_CONN_DUMMY(CLK_APU_CONN_EMI_26M, "apu_conn_emi_26m",
		"dsp_sel", 6),
	GATE_APU_CONN_DUMMY(CLK_APU_CONN_VPU_UDI, "apu_conn_vpu_udi",
		"dsp_sel", 7),
};

static const struct of_device_id of_match_clk_mt6779_apuconn[] = {
	{ .compatible = "mediatek,mt6779-apu_conn", },
	{}
};

static int clk_mt6779_apuconn_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	struct device_node *node = pdev->dev.of_node;
	int ret;

	clk_data = mtk_alloc_clk_data(CLK_APU_CONN_NR_CLK);

#if CCF_SUBSYS_DEBUG
	pr_info("%s(): clk data number: %d\n", __func__, clk_data->clk_num);
#endif

	mtk_clk_register_gates(node, apuconn_clks, ARRAY_SIZE(apuconn_clks),
			       clk_data);

	ret = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (ret)
		pr_notice("%s(): could not register clock provider: %d\n",
					__func__, ret);

	return ret;
}

static struct platform_driver clk_mt6779_apuconn_drv = {
	.probe = clk_mt6779_apuconn_probe,
	.driver = {
		.name = "clk-mt6779-apu_conn",
		.of_match_table = of_match_clk_mt6779_apuconn,
	},
};

#if MT_CLKMGR_MODULE_INIT

builtin_platform_driver(clk_mt6779_apuconn_drv);

#else

static int __init clk_mt6779_apuconn_platform_init(void)
{
	return platform_driver_register(&clk_mt6779_apuconn_drv);
}

arch_initcall_sync(clk_mt6779_apuconn_platform_init);

#endif  /* MT_CLKMGR_MODULE_INIT */

