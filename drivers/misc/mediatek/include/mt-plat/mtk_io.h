/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2015 MediaTek Inc.
 */
#ifndef __MT_IO_H__
#define __MT_IO_H__

/* only for arm64 */
#ifdef CONFIG_ARM64
#define IOMEM(a)	((void __force __iomem *)((a)))
#endif

#endif  /* !__MT_IO_H__ */

