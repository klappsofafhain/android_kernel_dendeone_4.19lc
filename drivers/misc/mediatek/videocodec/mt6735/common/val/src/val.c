// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (C) 2019  MediaTek Inc.
*
*/

/*=============================================================================
 *                              Include Files
 *===========================================================================*/
#include "val_types_private.h"
#include "val_api_private.h"
/* #include "mfv_reg.h" */
/* #include "mfv_config.h" */
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/semaphore.h>

#define MAX_HEAP_SIZE   (0x1000000)
#ifdef _6573_FPGA
#define GMC             0xC8000000
#else				/* _6573_REAL_CHIP E1 address */
#define GMC             0x40000000
#endif

/*=============================================================================
 *                              Function Body
 *===========================================================================*/

enum VAL_RESULT_T eVideoMemAlloc(struct VAL_MEMORY_T *a_prParam, VAL_UINT32_T a_u4ParamSize)
{

	return VAL_RESULT_NO_ERROR;
}

enum VAL_RESULT_T eVideoMemFree(struct VAL_MEMORY_T *a_prParam, VAL_UINT32_T a_u4ParamSize)
{
	/* MODULE_MFV_LOGD("!!Free Mem Size:%d!!\n",a_prParam->u4MemSize); */

	return VAL_RESULT_NO_ERROR;

}

/* mimic internal memory buffer, 128K bytes. */
enum VAL_RESULT_T eVideoIntMemUsed(struct VAL_INTMEM_T *a_prParam, VAL_UINT32_T a_u4ParamSize)
{

	return VAL_RESULT_NO_ERROR;
}

/* mimic internal memory buffer, 128K bytes. */
enum VAL_RESULT_T eVideoIntMemAlloc(struct VAL_INTMEM_T *a_prParam, VAL_UINT32_T a_u4ParamSize)
{

	return VAL_RESULT_NO_ERROR;
}

/* mimic internal memory buffer, 128K bytes. */
enum VAL_RESULT_T eVideoIntMemFree(struct VAL_INTMEM_T *a_prParam, VAL_UINT32_T a_u4ParamSize)
{

	return VAL_RESULT_NO_ERROR;
}



enum VAL_RESULT_T eVideoCreateEvent(struct VAL_EVENT_T *a_prParam, VAL_UINT32_T a_u4ParamSize)
{
	wait_queue_head_t *pWaitQueue;
	VAL_UINT8_T *pFlag;

	pWaitQueue = kmalloc(sizeof(wait_queue_head_t), GFP_ATOMIC);
	pFlag = kmalloc(sizeof(VAL_UINT8_T), GFP_ATOMIC);
	if (pWaitQueue != VAL_NULL) {
		init_waitqueue_head(pWaitQueue);
		a_prParam->pvWaitQueue = (VAL_VOID_T *) pWaitQueue;
	} else {
		pr_info("[VCODEC][ERROR] Event wait Queue failed to create\n");
	}
	if (pFlag != VAL_NULL) {
		a_prParam->pvReserved = (VAL_VOID_T *) pFlag;
		*((VAL_UINT8_T *) a_prParam->pvReserved) = VAL_FALSE;
	} else {
		pr_info("[VCODEC][ERROR] Event flag failed to create\n");
	}

	return VAL_RESULT_NO_ERROR;
}

enum VAL_RESULT_T eVideoCloseEvent(struct VAL_EVENT_T *a_prParam, VAL_UINT32_T a_u4ParamSize)
{
	wait_queue_head_t *pWaitQueue;
	VAL_UINT8_T *pFlag;

	pWaitQueue = (wait_queue_head_t *) a_prParam->pvWaitQueue;
	pFlag = (VAL_UINT8_T *) a_prParam->pvReserved;

	kfree(pWaitQueue);
	kfree(pFlag);

	a_prParam->pvWaitQueue = VAL_NULL;
	a_prParam->pvReserved = VAL_NULL;
	return VAL_RESULT_NO_ERROR;
}

enum VAL_RESULT_T eVideoWaitEvent(struct VAL_EVENT_T *a_prParam, VAL_UINT32_T a_u4ParamSize)
{
	wait_queue_head_t *pWaitQueue;
	long timeout_jiff, i4Ret;
	enum VAL_RESULT_T status;

	pWaitQueue = (wait_queue_head_t *) a_prParam->pvWaitQueue;
	timeout_jiff = (a_prParam->u4TimeoutMs) * HZ / 1000;
	/* pr_info("[MFV]eVideoWaitEvent,a_prParam->u4TimeoutMs=%d, timeout = %ld\n",
	   a_prParam->u4TimeoutMs,timeout_jiff); */
	i4Ret = wait_event_interruptible_timeout(*pWaitQueue,
						 *((VAL_UINT8_T *) a_prParam->
						   pvReserved) /*g_mflexvideo_interrupt_handler */ ,
						 timeout_jiff);
	if (0 == i4Ret) {
		status = VAL_RESULT_INVALID_ISR;	/* timeout */
	} else if (-ERESTARTSYS == i4Ret) {
		pr_info("[VCODEC] eVideoWaitEvent wake up by ERESTARTSYS");
		status = VAL_RESULT_RESTARTSYS;
	} else if (i4Ret > 0) {
		status = VAL_RESULT_NO_ERROR;
	} else {
		pr_info("[VCODEC] eVideoWaitEvent wake up by %ld", i4Ret);
		status = VAL_RESULT_NO_ERROR;
	}
	*((VAL_UINT8_T *) a_prParam->pvReserved) = VAL_FALSE;
	return status;
}

enum VAL_RESULT_T eVideoSetEvent(struct VAL_EVENT_T *a_prParam, VAL_UINT32_T a_u4ParamSize)
{
	wait_queue_head_t *pWaitQueue;
	/* pr_info("[MFV]eVideoSetEvent\n"); */
	pWaitQueue = (wait_queue_head_t *) a_prParam->pvWaitQueue;
	if (a_prParam->pvReserved != VAL_NULL) {
		/* Add one line comment for avoid kernel coding style, WARNING:BRACES: */
		*((VAL_UINT8_T *) a_prParam->pvReserved) = VAL_TRUE;
	} else {
		pr_info("[VCODEC][ERROR]Event flag should not be null\n");
	}
	if (pWaitQueue != VAL_NULL) {
		/* Add one line comment for avoid kernel coding style, WARNING:BRACES: */
		wake_up_interruptible(pWaitQueue);
	} else {
		pr_info("[VCODEC][ERROR]Wait Queue should not be null\n");
	}
	return VAL_RESULT_NO_ERROR;
}

enum VAL_RESULT_T eVideoCreateMutex(struct VAL_MUTEX_T *a_prParam, VAL_UINT32_T a_u4ParamSize)
{
	struct semaphore *pLock;

	pLock = kmalloc(sizeof(struct semaphore), GFP_ATOMIC);
	if (pLock != VAL_NULL) {
		/* Add one line comment for avoid kernel coding style, WARNING:BRACES: */
		a_prParam->pvMutex = (VAL_VOID_T *) pLock;
	} else {
		pr_debug("[VCODEC][ERROR]Enable to create mutex!\n");
                return VAL_RESULT_INVALID_MEMORY;
	}
	/* init_MUTEX(pLock); */
	sema_init(pLock, 1);
	return VAL_RESULT_NO_ERROR;
}

enum VAL_RESULT_T eVideoCloseMutex(struct VAL_MUTEX_T *a_prParam, VAL_UINT32_T a_u4ParamSize)
{
	if (a_prParam->pvMutex != VAL_NULL) {
		/* Add one line comment for avoid kernel coding style, WARNING:BRACES: */
		kfree(a_prParam->pvMutex);
	}
	a_prParam->pvMutex = VAL_NULL;
	return VAL_RESULT_NO_ERROR;
}


enum VAL_RESULT_T eVideoWaitMutex(struct VAL_MUTEX_T *a_prParam, VAL_UINT32_T a_u4ParamSize)
{
	struct semaphore *pLock;

	pLock = (struct semaphore *)a_prParam->pvMutex;
	down(pLock);
	return VAL_RESULT_NO_ERROR;
}

enum VAL_RESULT_T eVideoReleaseMutex(struct VAL_MUTEX_T *a_prParam, VAL_UINT32_T a_u4ParamSize)
{
	struct semaphore *pLock;

	pLock = (struct semaphore *)a_prParam->pvMutex;
	up(pLock);
	return VAL_RESULT_NO_ERROR;
}

enum VAL_RESULT_T eVideoMemSet(struct VAL_MEMORY_T *a_prParam,
			  VAL_UINT32_T a_u4ParamSize, int a_u4Value, VAL_UINT32_T a_u4Size)
{
	return VAL_RESULT_NO_ERROR;
}

enum VAL_RESULT_T eVideoMemCpy(struct VAL_MEMORY_T *a_prParamDst,
			  VAL_UINT32_T a_u4ParamDstSize,
			  struct VAL_MEMORY_T *a_prParamSrc,
			  VAL_UINT32_T a_u4ParamSrcSize, VAL_UINT32_T a_u4Size)
{
	return VAL_RESULT_NO_ERROR;
}

enum VAL_RESULT_T eVideoMemCmp(struct VAL_MEMORY_T *a_prParamSrc1,
			  VAL_UINT32_T a_u4ParamSrc1Size,
			  struct VAL_MEMORY_T *a_prParamSrc2,
			  VAL_UINT32_T a_u4ParamSrc2Size, VAL_UINT32_T a_u4Size)
{
	return VAL_RESULT_NO_ERROR;
}

enum VAL_RESULT_T eVideoGetTimeOfDay(struct VAL_TIME_T *a_prParam, VAL_UINT32_T a_u4ParamSize)
{
	struct timeval t1;

	do_gettimeofday(&t1);
	a_prParam->u4Sec = t1.tv_sec;
	a_prParam->u4uSec = t1.tv_usec;
	return VAL_RESULT_NO_ERROR;
}
