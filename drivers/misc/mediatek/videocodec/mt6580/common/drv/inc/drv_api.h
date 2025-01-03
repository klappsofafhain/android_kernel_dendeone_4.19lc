/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef __MT6589_DRVBASE_H__
#define __MT6589_DRVBASE_H__

#include "val_types_private.h"

/* ============================================================= */
/* For driver */

struct VAL_VCODEC_HW_LOCK_T {
	void *pvHandle;	/* HW vcodec handle */
	struct VAL_TIME_T rLockedTime;
	enum VAL_DRIVER_TYPE_T eDriverType;
	unsigned int u4ThreadID;
	/* for hybrid can find instance in WAITISR by thread id */
	/* due to handle in user space may already freed! */
	unsigned int u4VCodecThreadNum;
	unsigned int u4VCodecThreadID[VCODEC_THREAD_MAX_NUM];
};

struct VAL_NON_CACHE_MEMORY_LIST_T {
	unsigned long ulKVA;	/* Kernel virtual address */
	unsigned long ulKPA;	/* Kernel physical address */
	unsigned long pvHandle;	/*  */
	unsigned int u4VCodecThreadNum;	/* Hybrid vcodec thread num */
	/* hybrid vcodec thread ids */
	unsigned int u4VCodecThreadID[VCODEC_THREAD_MAX_NUM];
	unsigned long ulSize;
};

/* ============================================================== */
/* For Hybrid HW */
#define VCODEC_INST_NUM 16
#define VCODEC_INST_NUM_x_10 (VCODEC_INST_NUM * 10)

/* spinlock : OalHWContextLock */
extern struct VAL_VCODEC_OAL_HW_CONTEXT_T hw_ctx[VCODEC_INST_NUM];
/* mutex : NonCacheMemoryListLock */
extern struct VAL_NON_CACHE_MEMORY_LIST_T ncache_mem_list[VCODEC_INST_NUM_x_10];

/* For both hybrid and pure HW */
/* extern struct VAL_VCODEC_HW_LOCK_T DecHWLock; //mutex : VdecHWLock */
/* extern struct VAL_VCODEC_HW_LOCK_T EncHWLock; //mutex : VencHWLock */
extern struct VAL_VCODEC_HW_LOCK_T VcodecHWLock;	/* mutex : HWLock */

extern unsigned int gu4LockDecHWCount;	/* spinlock : LockDecHWCountLock */
extern unsigned int gu4LockEncHWCount;	/* spinlock : LockEncHWCountLock */
extern unsigned int gu4DecISRCount;	/* spinlock : DecISRCountLock */
extern unsigned int gu4EncISRCount;	/* spinlock : EncISRCountLock */

extern int gu4VDecIRQCount;	/* no lock, just for debug */
extern int gu4VEncIRQCount;	/* no lock, just for debug */

int search_slot_byTID(unsigned long ulpa, unsigned int curr_tid);
int search_slot_byHdl(unsigned long ulpa, unsigned long handle);
struct VAL_VCODEC_OAL_HW_CONTEXT_T *set_slot(unsigned long ulpa,
						unsigned int tid);
struct VAL_VCODEC_OAL_HW_CONTEXT_T
	*set_slot_TID(struct VAL_VCODEC_THREAD_ID_T a_prVcodecThreadID,
			unsigned int *a_prIndex);
struct VAL_VCODEC_OAL_HW_CONTEXT_T *free_slot(unsigned long ulpa);
void add_ncmem(unsigned long a_ulKVA,
			unsigned long a_ulKPA,
			unsigned long a_ulSize,
			unsigned int a_u4VCodecThreadNum,
			unsigned int *a_pu4VCodecThreadID);
void free_ncmem(unsigned long a_ulKVA, unsigned long a_ulKPA);
void ffree_ncmem(unsigned int a_u4Tid);
unsigned long search_ncmem_byKPA(unsigned long a_u4KPA);

#include "m4u.h"
enum m4u_callback_ret vcodec_m4u_fault_callback(int port,
				unsigned int mva, void *data);

#endif
