// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (C) 2019  MediaTek Inc.
*
*/
/*******************************************************************************
 *
 * Filename:
 * ---------
 *   AudDrv_Gpio.h
 *
 * Project:
 * --------
 *   MT6735  Audio Driver Gpio control
 *
 * Description:
 * ------------
 *   Audio gpio control
 *
 * Author:
 * -------
 *   George
 *
 *------------------------------------------------------------------------------
 *
 *
 *******************************************************************************/

#ifndef _AUDDRV_GPIO_H_
#define _AUDDRV_GPIO_H_

/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include "AudDrv_Common.h"
#include "AudDrv_Def.h"

/*****************************************************************************
 *                         D A T A   T Y P E S
 *****************************************************************************/


/*****************************************************************************
 *                         M A C R O
 *****************************************************************************/


/*****************************************************************************
 *                 FUNCTION       D E F I N I T I O N
 *****************************************************************************/
#if !defined(CONFIG_MTK_LEGACY)
#include <linux/gpio.h>
#else
#include <mt-plat/mt_gpio.h>
#endif

#if !defined(CONFIG_MTK_LEGACY)
void AudDrv_GPIO_probe(void *dev);
int AudDrv_GPIO_PMIC_Select(int bEnable);
int AudDrv_GPIO_I2S_Select(int bEnable);
int AudDrv_GPIO_EXTAMP_Select(int bEnable);
int AudDrv_GPIO_EXTAMP2_Select(int bEnable);
int AudDrv_GPIO_RCVSPK_Select(int bEnable);

#endif


#endif
