// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 *
 */

#ifndef _DDP_OVL_H_
#define _DDP_OVL_H_

#include "ddp_hal.h"
#include "DpDataType.h"

#define OVL_MAX_WIDTH  (4095)
#define OVL_MAX_HEIGHT (4095)

#define OVL_LAYER_NUM  (4)

/* start overlay module */
int ovl_start(enum DISP_MODULE_ENUM module, void *handle);

/* stop overlay module */
int ovl_stop(enum DISP_MODULE_ENUM module, void *handle);

/* reset overlay module */
int ovl_reset(enum DISP_MODULE_ENUM module, void *handle);

/* set region of interest */
int ovl_roi(enum DISP_MODULE_ENUM module, unsigned int bgW, unsigned int bgH, unsigned int bgColor, void *handle);

/* switch layer on/off */
int ovl_layer_switch(enum DISP_MODULE_ENUM module, unsigned layer, unsigned int en, void *handle);
/* get ovl input address */
void ovl_get_address(enum DISP_MODULE_ENUM module, unsigned long *add);

int ovl_3d_config(enum DISP_MODULE_ENUM module,
		  unsigned int layer_id,
		  unsigned int en_3d, unsigned int landscape, unsigned int r_first, void *handle);

void ovl_dump_analysis(enum DISP_MODULE_ENUM module);
void ovl_dump_reg(enum DISP_MODULE_ENUM module);

void ovl_get_info(int idx, void *data);
unsigned int ddp_ovl_get_cur_addr(bool rdma_mode, int layerid);
#endif
