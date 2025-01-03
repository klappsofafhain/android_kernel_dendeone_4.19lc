// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __MTK_PEP20_INTF_H
#define __MTK_PEP20_INTF_H


#ifdef CONFIG_MTK_PUMP_EXPRESS_PLUS_20_SUPPORT
extern int mtk_pep20_init(void);
extern int mtk_pep20_reset_ta_vchr(void);
extern int mtk_pep20_check_charger(void);
extern int mtk_pep20_start_algorithm(void);
extern int mtk_pep20_set_charging_current(CHR_CURRENT_ENUM *ichg,
	CHR_CURRENT_ENUM *aicr);

extern void mtk_pep20_set_to_check_chr_type(bool check);
extern void mtk_pep20_set_is_enable(bool enable);
extern void mtk_pep20_set_is_cable_out_occur(bool out);

extern bool mtk_pep20_get_to_check_chr_type(void);
extern bool mtk_pep20_get_is_connect(void);
extern bool mtk_pep20_get_is_enable(void);

#else /* NOT CONFIG_MTK_PUMP_EXPRESS_PLUS_20_SUPPORT */
static inline int mtk_pep20_init(void)
{
	return -ENOTSUPP;
}

static inline int mtk_pep20_reset_ta_vchr(void)
{
	return -ENOTSUPP;
}

static inline int mtk_pep20_check_charger(void)
{
	return -ENOTSUPP;
}

static inline int mtk_pep20_start_algorithm(void)
{
	return -ENOTSUPP;
}

static inline int mtk_pep20_set_charging_current(CHR_CURRENT_ENUM *ichg,
	CHR_CURRENT_ENUM *aicr)
{
	return -ENOTSUPP;
}

static inline void mtk_pep20_set_to_check_chr_type(bool check)
{
}

static inline void mtk_pep20_set_is_enable(bool enable)
{
}

static inline void mtk_pep20_set_is_cable_out_occur(bool out)
{
}

static inline bool mtk_pep20_get_to_check_chr_type(void)
{
	return false;
}

static inline bool mtk_pep20_get_is_connect(void)
{
	return false;
}

static inline bool mtk_pep20_get_is_enable(void)
{
	return false;
}

#endif /* CONFIG_MTK_PUMP_EXPRESS_PLUS_20_SUPPORT */

#endif /* __MTK_PEP20_INTF_H */
