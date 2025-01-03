/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2015 MediaTek Inc.
 */
#ifndef __FIQ_SMP_CALL_H
#define __FIQ_SMP_CALL_H

typedef void (*fiq_smp_call_func_t) (void *info, void *regs, void *svc_sp);

int fiq_smp_call_function(fiq_smp_call_func_t func, void *info, int wait);

#endif				/* !__FIQ_SMP_CALL_H */
