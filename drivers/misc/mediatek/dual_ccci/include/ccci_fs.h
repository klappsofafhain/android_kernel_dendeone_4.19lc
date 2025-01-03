/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *   ccci_fs.h
 *
 * Project:
 * --------
 *   ALPS
 *
 * Description:
 * ------------
 *   MT65XX CCCI FS Proxy Driver
 *
 ****************************************************************************/

#ifndef __CCCI_FS_H__
#define __CCCI_FS_H__
/*
 * define IOCTL commands
 */
#define CCCI_FS_IOC_MAGIC 'K'
#define CCCI_FS_IOCTL_GET_INDEX _IO(CCCI_FS_IOC_MAGIC, 1)
#define CCCI_FS_IOCTL_SEND      _IOR(CCCI_FS_IOC_MAGIC, 2, unsigned int)

#define  CCCI_FS_MAX_BUF_SIZE   (16384)
#define  CCCI_FS_MAX_BUFFERS    (5)
struct fs_stream_msg_t {
	unsigned length;
	unsigned index;
};

struct fs_stream_buffer_t {
	unsigned fs_ops;
	unsigned char buffer[CCCI_FS_MAX_BUF_SIZE];
};

#define CCCI_FS_SMEM_SIZE (sizeof(struct fs_stream_buffer_t) * CCCI_FS_MAX_BUFFERS)

#endif				/*  __CCCI_FS_H__ */
