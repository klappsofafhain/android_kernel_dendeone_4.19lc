// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 *
 */

#define LOG_TAG "RDMA"

#include <linux/delay.h>
#include "m4u.h"
/*#if defined(COMMON_DISP_LOG)*/
#include "disp_debug.h"
#include "disp_log.h"
/*#else
#include "disp_drv_log.h"
#include "ddp_log.h"
#endif*/
#include "ddp_reg.h"
#include "ddp_matrix_para.h"
#include "ddp_dump.h"
#include "lcm_drv.h"
#include "ddp_rdma.h"
#include "ddp_rdma_ex.h"


unsigned int rdma_index(enum DISP_MODULE_ENUM module)
{
	int idx = 0;

	switch (module) {
	case DISP_MODULE_RDMA0:
		idx = 0;
		break;
	case DISP_MODULE_RDMA1:
		idx = 1;
		break;
	case DISP_MODULE_RDMA2:
		idx = 2;
		break;
	default:
		DISPERR("invalid rdma module=%d\n", module);	/* invalid module */
		ASSERT(0);
	}
	ASSERT((idx >= 0) && (idx < RDMA_INSTANCES));
	return idx;
}

void rdma_set_target_line(enum DISP_MODULE_ENUM module, unsigned int line, void *handle)
{
	unsigned int idx = rdma_index(module);

	DISP_REG_SET(handle, idx * DISP_RDMA_INDEX_OFFSET + DISP_REG_RDMA_TARGET_LINE, line);
}

int rdma_init(enum DISP_MODULE_ENUM module, void *handle)
{
	return rdma_clock_on(module, handle);
}

int rdma_deinit(enum DISP_MODULE_ENUM module, void *handle)
{
	return rdma_clock_off(module, handle);
}

void rdma_get_address(enum DISP_MODULE_ENUM module, unsigned long *addr)
{
	unsigned int idx = rdma_index(module);

	*addr = DISP_REG_GET(DISP_REG_RDMA_MEM_START_ADDR + DISP_RDMA_INDEX_OFFSET * idx);
}
