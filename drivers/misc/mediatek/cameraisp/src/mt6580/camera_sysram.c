// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
/* #include <asm/io.h> */
#include <linux/proc_fs.h>	/* proc file use */
#include <linux/slab.h>		/* kmalloc/kfree in kernel 3.10 */
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/ioctl.h>
#include <linux/time.h>
/*#include <linux/xlog.h>*/
/*#include "inc/mt_typedefs.h"*/
/*#include <mach/mt_reg_base.h>*/
#include <mach/mt_clkmgr.h>
#include <mt-plat/sync_write.h>
#include "inc/camera_sysram.h"
/*#include <mach/mt_reg_base.h>*/
#include "inc/camera_sysram_imp.h"

#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif


#ifdef CONFIG_OF
#include <linux/of_platform.h>	/* for device tree */
#include <linux/of_irq.h>	/* for device tree */
#include <linux/of_address.h>	/* for device tree */
#endif


/* --------------------------------------------- */
#define ISP_VALID_REG_RANGE  0x10000
#define IMGSYS_BASE_ADDR     0x15000000
static struct SYSRAM_STRUCT Sysram;
static unsigned int Keep_Table;
static bool Flush_before;
/* --------------------------------------------- */
static void SYSRAM_GetTime(unsigned long long *pUS64, unsigned int *pSec, unsigned int *pUSec)
{
	ktime_t Time;
	unsigned long long TimeSec;
	/*  */
	Time = ktime_get();	/* ns */
	TimeSec = Time;
	do_div(TimeSec, 1000);
	/*  */
	*pUS64 = TimeSec;
	*pUSec = do_div(TimeSec, 1000000);
	*pSec = (unsigned long long) TimeSec;
}

/* --------------------------------------------- */
static void SYSRAM_CheckClock(void)
{
/* LOG_MSG("AllocatedTbl(0x%08X),EnableClk(%d)",
 * Sysram.AllocatedTbl,Sysram.EnableClk);
 */
	if (Sysram.AllocatedTbl) {
		if (!(Sysram.EnableClk)) {
			Sysram.EnableClk = true;
/*
 * LOG_MSG("AllocatedTbl(0x%08lX),EnableClk(%d)",
 * Sysram.AllocatedTbl,
 * Sysram.EnableClk);
 */
		}
	} else {
		if (Sysram.EnableClk) {
			Sysram.EnableClk = false;
/*
 * LOG_MSG("AllocatedTbl(0x%08lX),EnableClk(%d)",
 * Sysram.AllocatedTbl,
 * Sysram.EnableClk);
 */
		}
	}
}

/* --------------------------------------------- */
static void SYSRAM_DumpResMgr(void)
{
	unsigned int u4Idx = 0;

	LOG_MSG("TotalUserCount(%d),AllocatedTbl(0x%X)",
		Sysram.TotalUserCount, Sysram.AllocatedTbl);
	/*  */
	for (u4Idx = 0; u4Idx < SYSRAM_USER_AMOUNT; u4Idx++) {
		if (Sysram.AllocatedSize[u4Idx] > 0) {
			struct SYSRAM_USER_STRUCT * const pUserInfo =
			&Sysram.UserInfo[u4Idx];
			LOG_MSG(
			"[id:%u][%s][size:0x%X][pid:%d][tgid:%d][%s][%5u.%06u]",
			u4Idx,
			SysramUserName[u4Idx],
			Sysram.AllocatedSize[u4Idx],
			pUserInfo->pid,
			pUserInfo->tgid,
			pUserInfo->ProcName,
			pUserInfo->TimeS,
			pUserInfo->TimeUS);
		}
	}
	LOG_MSG("End");
}

/* --------------------------------------------- */
static inline bool SYSRAM_IsBadOwner(enum SYSRAM_USER_ENUM const User)
{
	if (User >= SYSRAM_USER_AMOUNT || User < 0)
		return MTRUE;
	else
		return MFALSE;
}

/* --------------------------------------------- */
static void SYSRAM_SetUserTaskInfo(enum SYSRAM_USER_ENUM const User)
{
	if (!SYSRAM_IsBadOwner(User)) {
		struct SYSRAM_USER_STRUCT * const pUserInfo = &Sysram.UserInfo[User];
		/*  */
		pUserInfo->pid = current->pid;
		pUserInfo->tgid = current->tgid;
		memcpy(pUserInfo->ProcName, current->comm,
		sizeof(pUserInfo->ProcName));
		/*  */
		SYSRAM_GetTime(&(pUserInfo->Time64),
		&(pUserInfo->TimeS), &(pUserInfo->TimeUS));
	}
}

/* --------------------------------------------- */
static void SYSRAM_ResetUserTaskInfo(enum SYSRAM_USER_ENUM const User)
{
	if (!SYSRAM_IsBadOwner(User)) {
		struct SYSRAM_USER_STRUCT * const pUserInfo = &Sysram.UserInfo[User];
		memset(pUserInfo, 0, sizeof(*pUserInfo));
	}
}

/* --------------------------------------------- */
static inline void SYSRAM_SpinLock(void)
{
	spin_lock(&Sysram.SpinLock);
}

/* --------------------------------------------- */
static inline void SYSRAM_SpinUnlock(void)
{
	spin_unlock(&Sysram.SpinLock);
}

/* --------------------------------------------- */
static inline bool SYSRAM_UserIsLocked(enum SYSRAM_USER_ENUM const User)
{
	if ((1 << User) & Sysram.AllocatedTbl)
		return MTRUE;
	else
		return MFALSE;
}

/* --------------------------------------------- */
static inline bool SYSRAM_UserIsUnlocked(enum SYSRAM_USER_ENUM const User)
{
	if (SYSRAM_UserIsLocked(User) == 0)
		return MTRUE;
	else
		return MFALSE;
}

/* --------------------------------------------- */
static void SYSRAM_LockUser(enum SYSRAM_USER_ENUM const User, unsigned int const Size)
{
	if (SYSRAM_UserIsLocked(User))
		return;
	/*  */
	Sysram.TotalUserCount++;
	Sysram.AllocatedTbl |= (1 << User);
	Sysram.AllocatedSize[User] = Size;
	SYSRAM_SetUserTaskInfo(User);
	/* Debug Log. */
	if ((1 << User) & SysramLogUserMask) {
		struct SYSRAM_USER_STRUCT * const pUserInfo = &Sysram.UserInfo[User];
		LOG_MSG(
		"[%s][%u bytes]OK,Time(%u.%06u)",
		SysramUserName[User],
		Sysram.AllocatedSize[User],
		pUserInfo->TimeS,
		pUserInfo->TimeUS);
	}
}

/* --------------------------------------------- */
static void SYSRAM_UnlockUser(enum SYSRAM_USER_ENUM const User)
{
	if (SYSRAM_UserIsUnlocked(User))
		return;
	/* Debug Log. */
	if ((1 << User) & SysramLogUserMask) {
		struct SYSRAM_USER_STRUCT * const pUserInfo = &Sysram.UserInfo[User];
		unsigned int Sec, USec;
		unsigned long long Time64 = 0;

		SYSRAM_GetTime(&Time64, &Sec, &USec);
		/*  */
		LOG_MSG("[%s][%u bytes]Time(%u.%06u - %u.%06u)(%u.%06u)",
			SysramUserName[User],
			Sysram.AllocatedSize[User],
			pUserInfo->TimeS,
			pUserInfo->TimeUS,
			Sec,
			USec,
			((unsigned int) (Time64 - pUserInfo->Time64)) / 1000,
			((unsigned int) (Time64 - pUserInfo->Time64)) % 1000);
	}
	/*  */
	if (Sysram.TotalUserCount > 0)
		Sysram.TotalUserCount--;
	Sysram.AllocatedTbl &= (~(1 << User));
	Sysram.AllocatedSize[User] = 0;
	SYSRAM_ResetUserTaskInfo(User);
}

/* --------------------------------------------- */
static void SYSRAM_DumpLayout(void)
{
	unsigned int Index = 0;
	struct SYSRAM_MEM_NODE_STRUCT *pCurrNode = NULL;
	/*  */
	LOG_DMP("[SYSRAM_DumpLayout]\n");
	LOG_DMP("AllocatedTbl = 0x%08X\n", Sysram.AllocatedTbl);
	LOG_DMP("=========================================\n");
	for (Index = 0; Index < SYSRAM_MEM_BANK_AMOUNT; Index++) {
		LOG_DMP(
		"\n [Mem Pool %d] (IndexTbl, UserCount)=(%X, %d)\n",
		Index,
		SysramMemPoolInfo[Index].IndexTbl,
		SysramMemPoolInfo[Index].UserCount);
		LOG_DMP(
		"[Locked Time] [Owner   Offset   Size  Index pCurrent pPrevious pNext]  [pid tgid] [Proc Name / Owner Name]\n");
		pCurrNode = &SysramMemPoolInfo[Index].pMemNode[0];
		while (pCurrNode != NULL) {
			enum SYSRAM_USER_ENUM const User = pCurrNode->User;
			if (SYSRAM_IsBadOwner(User)) {
				LOG_DMP(
				"------------ -------- %2d\t0x%05X 0x%05X  %d    %p %p\t%p\n",
				pCurrNode->User,
				pCurrNode->Offset,
				pCurrNode->Length,
				pCurrNode->Index,
				pCurrNode, pCurrNode->pPrev,
				pCurrNode->pNext);
			} else {
				struct SYSRAM_USER_STRUCT * const pUserInfo
				= &Sysram.UserInfo[User];
				LOG_DMP(
				"%5u.%06u %2d\t0x%05X 0x%05X  %d    %p %p\t%p  %-4d %-4d \"%s\" / \"%s\"\n",
				pUserInfo->TimeS,
				pUserInfo->TimeUS,
				User,
				pCurrNode->Offset,
				pCurrNode->Length,
				pCurrNode->Index,
				pCurrNode,
				pCurrNode->pPrev,
				pCurrNode->pNext,
				pUserInfo->pid,
				pUserInfo->tgid,
				pUserInfo->ProcName,
				SysramUserName[User]);
			}
			pCurrNode = pCurrNode->pNext;
		};
	}
	LOG_DMP("\n");
	SYSRAM_DumpResMgr();
}

/* --------------------------------------------- */
static struct SYSRAM_MEM_NODE_STRUCT *SYSRAM_AllocNode(
struct SYSRAM_MEM_POOL_STRUCT * const pMemPoolInfo)
{
	struct SYSRAM_MEM_NODE_STRUCT *pNode = NULL;
	unsigned int Index = 0;
	/*  */
	for (Index = 0; Index < pMemPoolInfo->UserAmount; Index += 1) {
		if ((pMemPoolInfo->IndexTbl) & (1 << Index)) {
			pMemPoolInfo->IndexTbl &= (~(1 << Index));
			/* A free node is found. */
			pNode = &pMemPoolInfo->pMemNode[Index];
			pNode->User = SYSRAM_USER_NONE;
			pNode->Offset = 0;
			pNode->Length = 0;
			pNode->pPrev = NULL;
			pNode->pNext = NULL;
			pNode->Index = Index;
			break;
		}
	}
	/* Shouldn't happen. */
	if (!pNode)
		LOG_ERR(
		"returns NULL - pMemPoolInfo->IndexTbl(%X)",
		pMemPoolInfo->IndexTbl);
	return pNode;
}

/* --------------------------------------------- */
static void SYSRAM_FreeNode(struct SYSRAM_MEM_POOL_STRUCT * const pMemPoolInfo,
			    struct SYSRAM_MEM_NODE_STRUCT * const pNode)
{
	pMemPoolInfo->IndexTbl |= (1 << pNode->Index);
	pNode->User = SYSRAM_USER_NONE;
	pNode->Offset = 0;
	pNode->Length = 0;
	pNode->pPrev = NULL;
	pNode->pNext = NULL;
	pNode->Index = 0;
}

/* --------------------------------------------- */
static bool SYSRAM_IsLegalSizeToAlloc(
enum SYSRAM_MEM_BANK_ENUM const MemBankNo,
enum SYSRAM_USER_ENUM const User, unsigned int const Size)
{
	unsigned int MaxSize = 0;
	/* (1) Check the memory pool. */
	switch (MemBankNo) {
	case SYSRAM_MEM_BANK_BAD:
	case SYSRAM_MEM_BANK_AMOUNT:
		{
			/* Illegal Memory Pool: return "illegal" */
			/* Shouldn't happen. */
			goto EXIT;
		}
	default:
		{
			break;
		}
	}
	/* (2) */
	/* Here we use the dynamic memory pools. */
	MaxSize = SysramUserSize[User];
	/*  */
EXIT:
	if (MaxSize < Size) {
		LOG_ERR("[User:%s]requested size(0x%X) > max size(0x%X)",
			SysramUserName[User], Size, MaxSize);
		SYSRAM_DumpLayout();
		return MFALSE;
	}
	return MTRUE;
}

/* --------------------------------------------- */
/*
 * Alignment should be 2^N, 4/8/2048 bytes alignment only
 * First fit algorithm
 */
static unsigned int SYSRAM_AllocUserPhy(enum SYSRAM_USER_ENUM const User,
				   unsigned int const Size,
				   unsigned int const Alignment,
				   enum SYSRAM_MEM_BANK_ENUM const MemBankNo)
{
	struct SYSRAM_MEM_NODE_STRUCT *pSplitNode = NULL;
	struct SYSRAM_MEM_NODE_STRUCT *pCurrNode = NULL;
	unsigned int AlingnedAddr = 0;
	unsigned int ActualSize = 0;
	/*  */
	struct SYSRAM_MEM_POOL_STRUCT * const pMemPoolInfo
	= SYSRAM_GetMemPoolInfo(MemBankNo);
	if (!pMemPoolInfo)
		return 0;
	/*  */
	pCurrNode = &pMemPoolInfo->pMemNode[0];
	for (; pCurrNode && pCurrNode->Offset <
	pMemPoolInfo->Size; pCurrNode =
	pCurrNode->pNext) {
		if (pCurrNode->User == SYSRAM_USER_NONE) {
			/* Free space */
			AlingnedAddr = (pCurrNode->Offset +
			Alignment - 1) & (~(Alignment - 1));
			ActualSize = Size + AlingnedAddr - pCurrNode->Offset;
			if (ActualSize <= pCurrNode->Length) {
				/* Hit!! Split into 2 */
				/* pSplitNode pointers to the
				 * next available (free) node.
				 */
				pSplitNode = SYSRAM_AllocNode(pMemPoolInfo);
				if (NULL == pSplitNode) {
					LOG_DMP("pSplitNode is NULL\n");
					return 0;
				}
				pSplitNode->Offset =
				pCurrNode->Offset + ActualSize;
				pSplitNode->Length =
				pCurrNode->Length - ActualSize;
				pSplitNode->pPrev = pCurrNode;
				pSplitNode->pNext = pCurrNode->pNext;
				/*  */
				pCurrNode->User = User;
				pCurrNode->Length = ActualSize;
				pCurrNode->pNext = pSplitNode;
				/*  */
				if (pSplitNode->pNext != NULL)
					pSplitNode->pNext->pPrev = pSplitNode;
				/*  */
				pMemPoolInfo->UserCount++;
				break;
			}
			/* Not hit */
			ActualSize = 0;
		}
	};
	/*  */
	return ActualSize ? (AlingnedAddr + pMemPoolInfo->Addr) : 0;
}

/* --------------------------------------------- */
static bool SYSRAM_FreeUserPhy(
enum SYSRAM_USER_ENUM const User,
enum SYSRAM_MEM_BANK_ENUM const MemBankNo)
{
	bool Ret = MFALSE;
	struct SYSRAM_MEM_NODE_STRUCT *pPrevOrNextNode = NULL;
	struct SYSRAM_MEM_NODE_STRUCT *pCurrNode = NULL;
	struct SYSRAM_MEM_NODE_STRUCT *pTempNode = NULL;

	struct SYSRAM_MEM_POOL_STRUCT * const pMemPoolInfo
	= SYSRAM_GetMemPoolInfo(MemBankNo);
	/*  */
	if (!pMemPoolInfo) {
		LOG_ERR(
		"pMemPoolInfo==NULL,User(%d),MemBankNo(%d)",
		User, MemBankNo);
		return MFALSE;
	}
	/*  */
	pCurrNode = &pMemPoolInfo->pMemNode[0];
	for (; pCurrNode; pCurrNode = pCurrNode->pNext) {
		if (User == pCurrNode->User) {
			Ret = MTRUE;	/* user is found. */
			if (pMemPoolInfo->UserCount > 0)
				pMemPoolInfo->UserCount--;

			pCurrNode->User = SYSRAM_USER_NONE;
			if (pCurrNode->pPrev != NULL) {
				pPrevOrNextNode = pCurrNode->pPrev;
				/*  */
				if (pPrevOrNextNode->User == SYSRAM_USER_NONE) {
					/* Merge previous: prev(o) + curr(x) */
					pTempNode = pCurrNode;
					pCurrNode = pPrevOrNextNode;
					pCurrNode->Length += pTempNode->Length;
					pCurrNode->pNext = pTempNode->pNext;
					if (pTempNode->pNext != NULL)
						pTempNode->pNext->
						pPrev = pCurrNode;
					SYSRAM_FreeNode(
					pMemPoolInfo, pTempNode);
				}
			}

			if (pCurrNode->pNext != NULL) {
				pPrevOrNextNode = pCurrNode->pNext;
				/*  */
				if (pPrevOrNextNode->User == SYSRAM_USER_NONE) {
					/* Merge next: curr(o) + next(x) */
					pTempNode = pPrevOrNextNode;
					pCurrNode->Length += pTempNode->Length;
					pCurrNode->pNext = pTempNode->pNext;
					if (pCurrNode->pNext != NULL)
						pCurrNode->pNext->
						pPrev = pCurrNode;
					SYSRAM_FreeNode(
					pMemPoolInfo, pTempNode);
				}
			}
			break;
		}
	}
	/*  */
	return Ret;
}

/* --------------------------------------------- */
static unsigned int SYSRAM_AllocUser(
enum SYSRAM_USER_ENUM const User, unsigned int Size,
unsigned int const Alignment)
{
	unsigned int Addr = 0;

	enum SYSRAM_MEM_BANK_ENUM const MemBankNo = SYSRAM_GetMemBankNo(User);
	/*  */
	if (SYSRAM_IsBadOwner(User)) {
		LOG_ERR("User(%d) out of range(%d)", User, SYSRAM_USER_AMOUNT);
		return 0;
	}
	/*  */
	if (!SYSRAM_IsLegalSizeToAlloc(MemBankNo, User, Size))
		return 0;
	/*  */
	switch (MemBankNo) {
	case SYSRAM_MEM_BANK_BAD:
	case SYSRAM_MEM_BANK_AMOUNT:
		{
			/* Do nothing. */
			break;
		}
	default:
		{
			Addr = SYSRAM_AllocUserPhy(User,
			Size, Alignment, MemBankNo);
			break;
		}
	}
	/*  */
	if (Addr > 0)
		SYSRAM_LockUser(User, Size);
	/*  */
	return Addr;
}

/* --------------------------------------------- */
static void SYSRAM_FreeUser(enum SYSRAM_USER_ENUM const User)
{
	enum SYSRAM_MEM_BANK_ENUM const MemBankNo = SYSRAM_GetMemBankNo(User);
	/*  */
	switch (MemBankNo) {
	case SYSRAM_MEM_BANK_BAD:
	case SYSRAM_MEM_BANK_AMOUNT:
		{
			/* Do nothing. */
			break;
		}
	default:
		{
			if (SYSRAM_FreeUserPhy(User, MemBankNo)) {
				SYSRAM_UnlockUser(User);
			} else {
				LOG_ERR("Cannot free User(%d)", User);
				SYSRAM_DumpLayout();
			}
			break;
		}
	}
}

/* --------------------------------------------- */
static unsigned int SYSRAM_MsToJiffies(unsigned int TimeMs)
{
	return (TimeMs * HZ + 512) >> 10;
}

/* --------------------------------------------- */
static unsigned int SYSRAM_TryAllocUser(enum SYSRAM_USER_ENUM const User,
				   unsigned int const Size, unsigned int const Alignment)
{
	unsigned int Addr = 0;
	/*  */
	SYSRAM_SpinLock();
	/*  */
	if (SYSRAM_UserIsLocked(User)) {
		SYSRAM_SpinUnlock();
		LOG_ERR(
		"[User:%s]has been already allocated!",
		SysramUserName[User]);
		return 0;
	}
	/*  */
	Addr = SYSRAM_AllocUser(User, Size, Alignment);
	if (Addr != 0)
		SYSRAM_CheckClock();
	SYSRAM_SpinUnlock();
	/*  */
	return Addr;
}

/* --------------------------------------------- */
static unsigned int SYSRAM_IOC_Alloc(enum SYSRAM_USER_ENUM const User,
unsigned int const Size, unsigned int Alignment,
unsigned int const TimeoutMS)
{
	unsigned int Addr = 0;
	signed int TimeOut = 0;
	/*  */
	if (SYSRAM_IsBadOwner(User)) {
		LOG_ERR("User(%d) out of range(%d)", User, SYSRAM_USER_AMOUNT);
		return 0;
	}
	/*  */
	if (Size == 0) {
		LOG_ERR("[User:%s]allocates 0 size!", SysramUserName[User]);
		return 0;
	}
	/*  */
	Addr = SYSRAM_TryAllocUser(User, Size, Alignment);
	if (Addr != 0	/* success */
	    || 0 == TimeoutMS	/* failure without a timeout specified */
	    ) {
		goto EXIT;
	}
	/*  */
	TimeOut = wait_event_interruptible_timeout(
	Sysram.WaitQueueHead, 0 != (Addr =
	SYSRAM_TryAllocUser(User, Size, Alignment)),
	SYSRAM_MsToJiffies(TimeoutMS));
	/*  */
	if (0 == TimeOut && 0 == Addr)
		LOG_ERR("[User:%s]allocate timeout", SysramUserName[User]);
	/*  */
EXIT:
	if (Addr == 0) {	/* Failure */
		LOG_ERR(
		"[User:%s]fails to allocate.Size(%u),Alignment(%u),TimeoutMS(%u)",
		SysramUserName[User],
		Size, Alignment, TimeoutMS);
		SYSRAM_DumpLayout();
	} else {		/* Success */
		if ((1 << User) & SysramLogUserMask)
			LOG_MSG(
			"[User:%s]%u bytes OK",
			SysramUserName[User], Size);
	}
	/*  */
	return Addr;
}

/* --------------------------------------------- */
static void SYSRAM_IOC_Free(enum SYSRAM_USER_ENUM User)
{
	if (SYSRAM_IsBadOwner(User)) {
		LOG_ERR("User(%d) out of range(%d)", User, SYSRAM_USER_AMOUNT);
		return;
	}
	/*  */
	SYSRAM_SpinLock();
	SYSRAM_FreeUser(User);
	wake_up_interruptible(&Sysram.WaitQueueHead);
	SYSRAM_CheckClock();
	SYSRAM_SpinUnlock();
	/*  */
	if ((1 << User) & SysramLogUserMask)
		LOG_MSG("[User:%s]Done", SysramUserName[User]);
}

/* --------------------------------------------- */
static int SYSRAM_Open(struct inode *pInode, struct file *pFile)
{
	int Ret = 0;
	unsigned int Sec = 0, USec = 0;
	unsigned long long Time64 = 0;
	struct SYSRAM_PROC_STRUCT *pProc;
	/*  */
	SYSRAM_GetTime(&Time64, &Sec, &USec);
	/*  */
	LOG_MSG("Cur:Name(%s),pid(%d),tgid(%d),Time(%d.%06d)",
		current->comm, current->pid, current->tgid, Sec, USec);
	/*  */
	SYSRAM_SpinLock();
	/*  */
	pFile->private_data = kmalloc(sizeof(struct SYSRAM_PROC_STRUCT), GFP_ATOMIC);
	if (pFile->private_data == NULL) {
		Ret = -ENOMEM;
	} else {
		pProc = (struct SYSRAM_PROC_STRUCT *) (pFile->private_data);
		pProc->Pid = 0;
		pProc->Tgid = 0;
		strncpy(pProc->ProcName, SYSRAM_PROC_NAME,
			strlen(SYSRAM_PROC_NAME));
		pProc->ProcName[strlen(SYSRAM_PROC_NAME)] = '\0';
		pProc->Table = 0;
		pProc->Time64 = Time64;
		pProc->TimeS = Sec;
		pProc->TimeUS = USec;
	}
	/*  */
	SYSRAM_SpinUnlock();
	/*  */
	if (Ret == (-ENOMEM)) {
		LOG_ERR("No enough memory");
/*
 * LOG_ERR("Cur:Name(%s),pid(%d),tgid(%d),Time(%ld.%06ld)",
 * current->comm,
 * current->pid,
 * current->tgid,
 * Sec,
 * USec);
 */
	}
	/*  */
	return Ret;
}

/* --------------------------------------------- */
static int SYSRAM_Release(struct inode *pInode, struct file *pFile)
{
	unsigned int Index = 0;
	unsigned int Sec = 0, USec = 0;
	unsigned long long Time64 = 0;
	struct SYSRAM_PROC_STRUCT *pProc;
	/*  */
	SYSRAM_GetTime(&Time64, &Sec, &USec);
	/*  */
	LOG_MSG("Cur:Name(%s),pid(%d),tgid(%d),Time(%d.%06d)",
		current->comm, current->pid, current->tgid, Sec, USec);
	/*  */
	if (pFile->private_data != NULL) {
		pProc = (struct SYSRAM_PROC_STRUCT *) (pFile->private_data);
		/*  */
		if (pProc->Pid != 0 || pProc->Tgid != 0 || pProc->Table != 0) {
			/*  */
			LOG_WRN(
			"Proc:Name(%s),pid(%d),tgid(%d),Table(0x%08X),Time(%d.%06d)",
			pProc->ProcName,
			pProc->Pid, pProc->Tgid,
			pProc->Table, pProc->TimeS,
			pProc->TimeUS);
			/*  */
			if (pProc->Table) {
				LOG_WRN("Force to release");
/*
 * LOG_WRN("Proc:Name(%s),pid(%d),tgid(%d),Table(0x%08lX),Time(%ld.%06ld)",
 * pProc->ProcName,
 * pProc->Pid,
 * pProc->Tgid,
 * pProc->Table,
 * pProc->TimeS,
 * pProc->TimeUS);
 */
				SYSRAM_DumpLayout();
				/*  */
				for (Index = 0; Index <
				SYSRAM_USER_AMOUNT; Index++) {
					if (pProc->Table & (1 << Index))
						SYSRAM_IOC_Free((
						enum SYSRAM_USER_ENUM) Index);
				}
			}
		}
		/*  */
		kfree(pFile->private_data);
		pFile->private_data = NULL;
	} else {
		LOG_WRN("private_data is NULL");
/*
 * LOG_WRN("Cur:Name(%s),pid(%d),
 * tgid(%d),Time(%ld.%06ld)",
 * current->comm,
 * current->pid,
 * current->tgid,
 * Sec,
 * USec);
 */
	}
	/*  */
	return 0;
}

/* --------------------------------------------- */
static int SYSRAM_Flush(struct file *pFile, fl_owner_t Id)
{
	unsigned int Index = 0;
	unsigned int Sec = 0, USec = 0;
	unsigned long long Time64 = 0;
	struct SYSRAM_PROC_STRUCT *pProc;
	/*  */
	SYSRAM_GetTime(&Time64, &Sec, &USec);
	/*  */
	LOG_MSG("Cur:Name(%s),pid(%d),tgid(%d),Time(%d.%06d)",
		current->comm, current->pid, current->tgid, Sec, USec);
	/*  */
	if (pFile->private_data != NULL) {
		pProc = (struct SYSRAM_PROC_STRUCT *) pFile->private_data;
		/*  */
		if (pProc->Pid != 0 || pProc->Tgid != 0 || pProc->Table != 0) {
			/*  */
			LOG_WRN(
			"Proc:Name(%s),pid(%d),tgid(%d),Table(0x%08X),Time(%d.%06d)",
			pProc->ProcName,
			pProc->Pid, pProc->Tgid, pProc->Table,
			pProc->TimeS, pProc->TimeUS);
			/*  */
			if (pProc->Tgid == 0 && pProc->Table != 0) {
				LOG_ERR("No Tgid info");
/*
 * LOG_ERR("Cur:Name(%s),pid(%d),
 * tgid(%d),Time(%ld.%06ld)",
 * current->comm,
 * current->pid,
 * current->tgid,
 * Sec,
 * USec);
 * LOG_ERR("Proc:Name(%s),pid(%d),
 * tgid(%d),Table(0x%08lX),Time(%ld.%06ld)",
 * pProc->ProcName,
 * pProc->Pid,
 * pProc->Tgid,
 * pProc->Table,
 * pProc->TimeS,
 * pProc->TimeUS);
 */
			} else
			if ((pProc->Tgid == current->tgid) ||
				((pProc->Tgid != current->tgid)
				 && (strcmp(current->comm, "binder") == 0))) {
				if (pProc->Table) {
					LOG_WRN("Force to release");
/*
 * LOG_WRN("Cur:Name(%s),pid(%d),
 * tgid(%d),Time(%ld.%06ld)",
 * current->comm,
 * current->pid,
 * current->tgid,
 * Sec,
 * USec);
 */
					SYSRAM_DumpLayout();
					Keep_Table = pProc->Table;
					Flush_before = true;
					/*  */
					for (Index = 0; Index <
					SYSRAM_USER_AMOUNT; Index++) {
					if (pProc->Table &
						(1 << Index))
						SYSRAM_IOC_Free((
						enum SYSRAM_USER_ENUM)
						Index);
					}
					/*  */
					pProc->Table = 0;
					LOG_WRN(
					"Force to release - (0x%X/0x%X)",
					(unsigned int)(Keep_Table),
					(unsigned int)(pProc->Table));
				}
			}

		}
	} else {
		LOG_WRN("private_data is NULL");
/*
 * LOG_WRN("Cur:Name(%s),pid(%d),tgid(%d),Time(%ld.%06ld)",
 * current->comm,
 * current->pid,
 * current->tgid,
 * Sec,
 * USec);
 */
	}
	/*  */
	return 0;
}

/* --------------------------------------------- */
static int SYSRAM_mmap(struct file *pFile, struct vm_area_struct *pVma)
{
	/* LOG_MSG(""); */
	long length = 0;
	unsigned int pfn = 0x0;

	pVma->vm_page_prot = pgprot_noncached(pVma->vm_page_prot);
	length = (long)(pVma->vm_end - pVma->vm_start);
/* page from number, physical address of kernel memory */
	pfn = pVma->vm_pgoff << PAGE_SHIFT;
	LOG_WRN(
	"pVma->vm_pgoff(0x%lx),phy(0x%lx),pVmapVma->vm_start(0x%lx),pVma->vm_end(0x%lx),length(0x%lx)",
	 pVma->vm_pgoff, pVma->vm_pgoff << PAGE_SHIFT,
	 pVma->vm_start, pVma->vm_end, length);
	if ((length > ISP_VALID_REG_RANGE) || (pfn < IMGSYS_BASE_ADDR)
	    || (pfn > (IMGSYS_BASE_ADDR + ISP_VALID_REG_RANGE))) {
		LOG_ERR(
		"mmap range error : vm_start(0x%lx),vm_end(0x%lx),length(0x%lx),pfn(0x%x)!",
		pVma->vm_start, pVma->vm_end, length, pfn);
		return -EAGAIN;
	}
	if (remap_pfn_range(pVma,
	pVma->vm_start,
	pVma->vm_pgoff, pVma->vm_end - pVma->
	vm_start, pVma->vm_page_prot)) {
		LOG_ERR("fail");
		return -EAGAIN;
	}
	return 0;
}

/* --------------------------------------------- */
static long SYSRAM_Ioctl(struct file *pFile,
unsigned int Cmd, unsigned long Param)
{
	signed int Ret = 0;
	unsigned int Sec = 0, USec = 0;
	unsigned long long Time64 = 0;
	struct SYSRAM_PROC_STRUCT *pProc = (struct SYSRAM_PROC_STRUCT *) pFile->private_data;
    struct SYSRAM_ALLOC_STRUCT Alloc;
	enum SYSRAM_USER_ENUM User;
	/*  */
	SYSRAM_GetTime(&Time64, &Sec, &USec);
/*
 * LOG_MSG("Cur:Name(%s),pid(%d),tgid(%d),Time(%ld.%06ld)",
 * current->comm,
 * current->pid,
 * current->tgid,
 * Sec,
 * USec);
 */
	if (pFile->private_data == NULL) {
		LOG_WRN("private_data is NULL.");
		Ret = -EFAULT;
		goto EXIT;
	}
	/*  */
	switch (Cmd) {
	case SYSRAM_ALLOC:
	{
		if (copy_from_user(&Alloc, (void *)Param,
			sizeof(struct SYSRAM_ALLOC_STRUCT)) == 0) {
			if (SYSRAM_IsBadOwner(Alloc.User)) {
				LOG_ERR(
				"User(%d) out of range(%d)",
				Alloc.User,
				SYSRAM_USER_AMOUNT);
				Ret = -EFAULT;
				goto EXIT;
			}
			/*  */
			Alloc.Addr = SYSRAM_IOC_Alloc
			(Alloc.User, Alloc.Size,
			Alloc.Alignment,
			Alloc.TimeoutMS);
			if (Alloc.Addr != 0) {
				SYSRAM_SpinLock();
				pProc->Table |= (1 << Alloc.User);
				if (pProc->Tgid == 0) {
					pProc->Pid = current->pid;
					pProc->Tgid = current->tgid;
					strncpy(pProc->ProcName,
					current->comm,
					strlen(current->comm));
					pProc->ProcName[strlen(
					current->comm)] = '\0';
					SYSRAM_SpinUnlock();
				} else {
					SYSRAM_SpinUnlock();
					if (pProc->Tgid !=
						current->tgid) {
						LOG_ERR(
						"Tgid is inconsistent");
						Ret = -EFAULT;
					}
				}
				} else {
					LOG_MSG(
					"[christ test] SYSRAM_ALLOC E.6 ");
					Ret = -EFAULT;
				}
				/*  */
				if (copy_to_user((void *)Param,
					&Alloc, sizeof(
					struct SYSRAM_ALLOC_STRUCT))) {
					LOG_MSG(
					"[christ test] SYSRAM_ALLOC E.7 ");
					LOG_ERR("copy to user failed");
					Ret = -EFAULT;
				}
			} else {
				LOG_ERR("copy_from_user fail");
				Ret = -EFAULT;
			}
			break;
		}
		/*  */
	case SYSRAM_FREE:
		{
		if (copy_from_user(&User, (void *)Param,
			sizeof(enum SYSRAM_USER_ENUM)) == 0) {
			if (SYSRAM_IsBadOwner(User)) {
				LOG_ERR("User(%d) out of range(%d)", User,
					SYSRAM_USER_AMOUNT);
				Ret = -EFAULT;
				goto EXIT;
			}
			/*  */
			SYSRAM_SpinLock();
			if ((pProc->Table) & (1 << User)) {
				SYSRAM_SpinUnlock();
				SYSRAM_IOC_Free(User);
				SYSRAM_SpinLock();
				/*  */
				pProc->Table &= (~(1 << User));
				if (pProc->Table == 0) {
					pProc->Pid = 0;
					pProc->Tgid = 0;
					strncpy(pProc->ProcName,
					SYSRAM_PROC_NAME,
					strlen(SYSRAM_PROC_NAME));
					pProc->ProcName[
					strlen(SYSRAM_PROC_NAME)] = '\0';
				}
				SYSRAM_SpinUnlock();
			} else {
				SYSRAM_SpinUnlock();
				LOG_WRN(
				"Freeing unallocated buffer user(%d/0x%x/0x%X)",
				User, (unsigned int)(pProc->Table),
				(unsigned int)(Keep_Table));
				SYSRAM_SpinLock();
				if (Flush_before || ((Keep_Table)
					& (1 << User))) {
					LOG_WRN(
					"WRN, flush before(%d), MASK(0x%X/0x%X)",
					Flush_before,
					(unsigned int)(pProc->Table),
					(unsigned int)(Keep_Table));
					Keep_Table &= ~(1 << User);
					LOG_WRN(
					"WRN_2, flush before(%d), MASK(0x%X/0x%X)",
					Flush_before,
					(unsigned int)(pProc->Table),
					(unsigned int)(Keep_Table));
					if ((int)Keep_Table == 0)
						Flush_before = false;
					Ret = 0;
				} else {
					Ret = -EFAULT;
				}
				SYSRAM_SpinUnlock();
			}
		} else {
			LOG_ERR("copy_from_user fail");
			Ret = -EFAULT;
		}
		break;
		}
	case SYSRAM_DUMP:
		{
			SYSRAM_DumpLayout();
			break;
		}
	default:
		{
			LOG_WRN("No such command");
			Ret = -EINVAL;
			break;
		}
	}
	/*  */
EXIT:
	if (Ret != 0) {
		LOG_ERR("Fail");
		LOG_ERR("Cur:Name(%s),pid(%d),tgid(%d),Time(%d.%06d)",
			current->comm, current->pid, current->tgid, Sec, USec);
		if (pFile->private_data != NULL) {
			LOG_ERR(
			"Proc:Name(%s),pid(%d),tgid(%d),Table(0x%08X),Time(%d.%06d)",
			pProc->ProcName, pProc->Pid,
			pProc->Tgid, pProc->Table,
			Sec, USec);
		}
	}
	/*  */
	return Ret;
}

/* --------------------------------------------- */

#ifdef CONFIG_COMPAT

static int compat_get_sysram_alloc_data(
struct compat_SYSRAM_ALLOC_STRUCT __user *data32,
struct SYSRAM_ALLOC_STRUCT __user *data)
{
	compat_uint_t tmp;
	int err;

	err = get_user(tmp, &data32->Alignment);
	err |= put_user(tmp, &data->Alignment);
	err |= get_user(tmp, &data32->Size);
	err |= put_user(tmp, &data->Size);
	err |= get_user(tmp, &data32->User);
	err |= put_user(tmp, &data->User);
	err |= get_user(tmp, &data32->Addr);
	err |= put_user(tmp, &data->Addr);
	err |= get_user(tmp, &data32->TimeoutMS);
	err |= put_user(tmp, &data->TimeoutMS);
	return err;
}


static int compat_put_sysram_alloc_data(
struct compat_SYSRAM_ALLOC_STRUCT __user *data32,
struct SYSRAM_ALLOC_STRUCT __user *data)
{
	compat_uint_t tmp;
	int err;

	err = get_user(tmp, &data->Alignment);
	err |= put_user(tmp, &data32->Alignment);
	err |= get_user(tmp, &data->Size);
	err |= put_user(tmp, &data32->Size);
	err |= get_user(tmp, &data->User);
	err |= put_user(tmp, &data32->User);
	err |= get_user(tmp, &data->Addr);
	err |= put_user(tmp, &data32->Addr);
	err |= get_user(tmp, &data->TimeoutMS);
	err |= put_user(tmp, &data32->TimeoutMS);
	return err;
}

static int compat_get_sysram_userenum_data(
enum compat_SYSRAM_USER_ENUM __user *data32,
enum SYSRAM_USER_ENUM __user *data)
{
	compat_uint_t tmp;
	int err;

	err = get_user(tmp, &data32);
	err |= put_user(tmp, &data);
	return err;
}


static int compat_put_sysram_userenum_data(
enum compat_SYSRAM_USER_ENUM __user *data32,
enum SYSRAM_USER_ENUM __user *data)
{
	compat_uint_t tmp;
	int err;

	err = get_user(tmp, &data);
	err |= put_user(tmp, &data32);
	return err;
}


static long SYSRAM_ioctl_compat(struct file *filp,
unsigned int cmd, unsigned long arg)
{
	long ret;

	LOG_MSG(
	"SYSRAM_ioctl_compat E, cmd(0x%x), SYSRAM_ALLOC/SYSRAM_FREE/SYSRAM_DUMP/compat_SYSRAM_ALLOC/compat_SYSRAM_FREE(0x%x/0x%x/0x%x/0x%x/0x%x)",
	cmd,
	(unsigned int)SYSRAM_ALLOC,
	(unsigned int)SYSRAM_FREE,
	(unsigned int)SYSRAM_DUMP,
	(unsigned int)COMPAT_SYSRAM_ALLOC,
	(unsigned int)COMPAT_SYSRAM_FREE);

	if (!filp->f_op || !filp->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case COMPAT_SYSRAM_ALLOC:	/*  */
		{
			struct compat_SYSRAM_ALLOC_STRUCT __user *data32;
			struct SYSRAM_ALLOC_STRUCT __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;
			err = compat_get_sysram_alloc_data(data32, data);
			if (err) {
				LOG_MSG("compat_get_sysram_alloc_data error!!!\n");
				return err;
			}
			ret = filp->f_op->unlocked_ioctl(
			filp, SYSRAM_ALLOC, (unsigned long)data);
			err = compat_put_sysram_alloc_data(data32, data);
			if (err) {
				LOG_MSG("compat_put_sysram_alloc_data error!!!\n");
				return err;
			}
			return ret;
		}
	case COMPAT_SYSRAM_FREE:	/*  */
	case SYSRAM_DUMP:	/* no arg */
		return filp->f_op->unlocked_ioctl(filp, cmd, arg);
	default:
		return -ENOIOCTLCMD;
	}
	LOG_MSG("SYSRAM_ioctl_compat X");
}
#endif

/* --------------------------------------------- */
#if 0
#ifdef CONFIG_OF
struct cam_isp_device {
	void __iomem *regs[ISP_CAM_BASEADDR_NUM];
	struct device *dev;
	int irq[ISP_CAM_IRQ_IDX_NUM];
};

static struct cam_isp_device *cam_isp_devs;
static int nr_camisp_devs;
#endif
#endif


static const struct file_operations SysramFileOper = {
	.owner = THIS_MODULE,
	.open = SYSRAM_Open,
	.release = SYSRAM_Release,
	.flush = SYSRAM_Flush,
	.unlocked_ioctl = SYSRAM_Ioctl,
	.mmap = SYSRAM_mmap,
#ifdef CONFIG_COMPAT
	.compat_ioctl = SYSRAM_ioctl_compat,
#endif
};

/* --------------------------------------------- */
static inline int SYSRAM_RegCharDrv(void)
{
	LOG_MSG("E");
	if (alloc_chrdev_region(&Sysram.DevNo, 0, 1, SYSRAM_DEV_NAME)) {
		LOG_ERR("allocate device no failed");
		return -EAGAIN;
	}
	/* allocate driver */
	Sysram.pCharDrv = cdev_alloc();
	if (Sysram.pCharDrv == NULL) {
		unregister_chrdev_region(Sysram.DevNo, 1);
		LOG_ERR("allocate mem for kobject failed");
		return -ENOMEM;
	}
	/* Attatch file operation. */
	cdev_init(Sysram.pCharDrv, &SysramFileOper);
	Sysram.pCharDrv->owner = THIS_MODULE;
	/* Add to system */
	if (cdev_add(Sysram.pCharDrv, Sysram.DevNo, 1)) {
		LOG_ERR("Attatch file operation failed");
		unregister_chrdev_region(Sysram.DevNo, 1);
		return -EAGAIN;
	}
	LOG_MSG("X");
	return 0;
}

/* --------------------------------------------- */
static inline void SYSRAM_UnregCharDrv(void)
{
	LOG_MSG("E");
	/* Release char driver */
	cdev_del(Sysram.pCharDrv);
	unregister_chrdev_region(Sysram.DevNo, 1);
	LOG_MSG("X");
}

/* --------------------------------------------- */
static int SYSRAM_Probe(struct platform_device *pDev)
{
	signed int Ret = 0;
	unsigned int Index = 0;
	struct device *sysram_device = NULL;
	/*  */
	LOG_MSG("SYSRAM_Probe E");
	/* register char driver */
	/* allocate major no */
	if (SYSRAM_RegCharDrv()) {
		LOG_ERR("register char failed");
		return -EAGAIN;
	}

	Sysram.pClass = class_create(THIS_MODULE, "SysramDrv");
	if (IS_ERR(Sysram.pClass)) {
		Ret = PTR_ERR(Sysram.pClass);
		LOG_ERR("Unable to create class, err(%d)", Ret);
		return Ret;
	}
	sysram_device = device_create(Sysram.pClass, NULL,
	Sysram.DevNo, NULL, SYSRAM_DEV_NAME);
	/* Initialize variables */
	spin_lock_init(&Sysram.SpinLock);
	Sysram.TotalUserCount = 0;
	Sysram.AllocatedTbl = 0;
	memset(Sysram.AllocatedSize, 0, sizeof(Sysram.AllocatedSize));
	memset(Sysram.UserInfo, 0, sizeof(Sysram.UserInfo));
	init_waitqueue_head(&Sysram.WaitQueueHead);
	Sysram.EnableClk = MFALSE;
	Keep_Table = 0x0;
	Flush_before = false;
	/*  */
	for (Index = 0; Index < SYSRAM_MEM_BANK_AMOUNT; Index++) {
		SysramMemPoolInfo[Index].pMemNode[0].User = SYSRAM_USER_NONE;
		SysramMemPoolInfo[Index].pMemNode[0].Offset = 0;
		SysramMemPoolInfo[Index].pMemNode[0].Length =
		SysramMemPoolInfo[Index].Size;
		SysramMemPoolInfo[Index].pMemNode[0].Index = 0;
		SysramMemPoolInfo[Index].pMemNode[0].pNext = NULL;
		SysramMemPoolInfo[Index].pMemNode[0].pPrev = NULL;
		SysramMemPoolInfo[Index].IndexTbl = (~0x1);
		SysramMemPoolInfo[Index].UserCount = 0;
	}
	/*  */
	for (Index = 0; Index < SYSRAM_USER_AMOUNT; Index++)
		Sysram.AllocatedSize[Index] = 0;
	Sysram.DebugFlag = SYSRAM_DEBUG_DEFAULT;
	/*  */
	LOG_MSG("SYSRAM_Probe X");
	return Ret;
}

/* --------------------------------------------- */
static int SYSRAM_Remove(struct platform_device *pDev)
{
	LOG_MSG("E");
	/* unregister char driver. */
	SYSRAM_UnregCharDrv();
	/*  */
	device_destroy(Sysram.pClass, Sysram.DevNo);
	class_destroy(Sysram.pClass);
	/*  */
	LOG_MSG("X");
	/*  */
	return 0;
}

/* --------------------------------------------- */
static int SYSRAM_Suspend(struct platform_device *pDev, pm_message_t Mesg)
{
	LOG_MSG("");
	return 0;
}

/* --------------------------------------------- */
static int SYSRAM_Resume(struct platform_device *pDev)
{
	LOG_MSG("");
	return 0;
}

/* --------------------------------------------- */
/* force to use define in dts file to hook device */
#ifdef CONFIG_OF
static const struct of_device_id isp_of_ids[] = {
	{.compatible = "mediatek,ISP_SYSR",},
	{}
};
#endif



static struct platform_driver SysramPlatformDriver = {
	.probe = SYSRAM_Probe,
	.remove = SYSRAM_Remove,
	.suspend = SYSRAM_Suspend,
	.resume = SYSRAM_Resume,
	.driver = {
		   .name = SYSRAM_DEV_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = isp_of_ids,
#endif
		}
};

/* --------------------------------------------- */
#define FSOP_USE_OLD_TYLE 0
/*
 * ssize_t (*read) (struct file *,
 * char __user *, size_t, loff_t *)
 */
#if FSOP_USE_OLD_TYLE
static int SYSRAM_DumpLayoutToProc(
	char *pPage,
	char **ppStart,
	off_t Off,
	int Count,
	int *pEof,
	void *pData)
#else
static int SYSRAM_DumpLayoutToProc(
	struct file *pFile,
	char *pStart,
	size_t Off,
	loff_t *Count)
#endif
{
#if FSOP_USE_OLD_TYLE
	char *p = pPage;
	unsigned int len = 0;
	unsigned int Index = 0;
	struct SYSRAM_MEM_NODE_STRUCT *pCurrNode = NULL;
	/*  */
	p += sprintf(p, "\n[SYSRAM_DumpLayoutToProc]\n");
	p += sprintf(p, "AllocatedTbl = 0x%08lX\n", Sysram.AllocatedTbl);
	p += sprintf(p, "=========================================\n");
	for (Index = 0; Index < SYSRAM_MEM_BANK_AMOUNT; Index++) {
		p += sprintf(p,
		"\n [Mem Pool %ld] (IndexTbl, UserCount)=(%lX, %ld)\n",
		Index,
		SysramMemPoolInfo[Index].IndexTbl,
		SysramMemPoolInfo[Index].UserCount);
		p += sprintf(p,
		"[Locked Time] [Owner   Offset   Size  Index pCurrent pPrevious pNext]  [pid tgid] [Proc Name / Owner Name]\n");
		pCurrNode = &SysramMemPoolInfo[Index].pMemNode[0];
		while (pCurrNode != NULL) {
			enum SYSRAM_USER_ENUM const User = pCurrNode->User;
			if (SYSRAM_IsBadOwner(User)) {
				p += sprintf(p,
					     "------------ -------- %2d\t0x%05lX 0x%05lX  %ld    %p %p\t%p\n",
/* L0.MT6580.SMT.DEV orig: (int)(pCurrNode->User), */
					     pCurrNode->User,
					     pCurrNode->Offset,
					     pCurrNode->Length,
					     pCurrNode->Index,
					     pCurrNode,
					     pCurrNode->pPrev,
					     pCurrNode->pNext);
			} else {
				struct SYSRAM_USER_STRUCT * const pUserInfo
				= &Sysram.UserInfo[User];
				p += sprintf(p,
				"%5lu.%06lu %2d\t0x%05lX 0x%05lX  %ld    %p %p\t%p  %-4d %-4d \"%s\" / \"%s\"\n",
				pUserInfo->TimeS,
				pUserInfo->TimeUS,
				User,
				pCurrNode->Offset,
				pCurrNode->Length,
				pCurrNode->Index,
				pCurrNode,
				pCurrNode->pPrev,
				pCurrNode->pNext,
				pUserInfo->pid,
				pUserInfo->tgid,
				pUserInfo->ProcName,
				SysramUserName[User]);
			}
			pCurrNode = pCurrNode->pNext;
		};
	}
	/*  */
	*ppStart = pPage + Off;
	len = p - pPage;
	if (len > Off)
		len -= Off;
	else
		len = 0;
	/*  */
	return len < Count ? len : Count;
#else
	return 0;
#endif
}

/* --------------------------------------------- */
/*
 * ssize_t (*read) (struct file *,
 * char __user *, size_t, loff_t *)
 */
#if FSOP_USE_OLD_TYLE
static int SYSRAM_ReadFlag(
	char *pPage,
	char **ppStart,
	off_t Off,
	int Count,
	int *pEof,
	void *pData)
#else
static int SYSRAM_ReadFlag(
	struct file *pFile,
	char *pStart,
	size_t Off,
	loff_t *Count)
#endif
{
#if FSOP_USE_OLD_TYLE
	char *p = pPage;
	unsigned int len = 0;
	/*  */
	p += sprintf(p, "\r\n[SYSRAM_ReadFlag]\r\n");
	p += sprintf(p, "=========================================\r\n");
	p += sprintf(p, "Sysram.DebugFlag = 0x%08lX\r\n", Sysram.DebugFlag);

	*ppStart = pPage + Off;

	len = p - pPage;
	if (len > Off)
		len -= Off;
	else
		len = 0;
	/*  */
	return len < Count ? len : Count;
#else
	return 0;
#endif
}

/* --------------------------------------------- */
/*
 * ssize_t (*write) (struct file *,
 * const char __user *, size_t, loff_t *)
 */
#if FSOP_USE_OLD_TYLE
static int SYSRAM_WriteFlag(
	struct file *pFile,
	const char *pBuffer,
	unsigned long Count,
	void *pData)
#else
static ssize_t SYSRAM_WriteFlag(
	struct file *pFile,
	const char *pBuffer,
	size_t Count,
	loff_t *pData)
#endif
{
#if FSOP_USE_OLD_TYLE
	char acBuf[32];
	unsigned int u4CopySize = 0;
	unsigned int u4SysramDbgFlag = 0;
	/*  */
	u4CopySize = (Count < (sizeof(acBuf) - 1)) ?
	Count : (sizeof(acBuf) - 1);
	if (copy_from_user(acBuf, pBuffer, u4CopySize))
		return 0;
	acBuf[u4CopySize] = '\0';
	if (sscanf(acBuf, "%lx", &u4SysramDbgFlag == 3))
		Sysram.DebugFlag = u4SysramDbgFlag;
	return Count;
#else
	return 0;
#endif
}

/*************************************************
 *
 *************************************************/
static const struct file_operations fsysram_proc_fops = {
	.read = SYSRAM_DumpLayoutToProc,
	.write = NULL,
};

static const struct file_operations fsysram_flag_proc_fops = {
	.read = SYSRAM_ReadFlag,
	.write = SYSRAM_WriteFlag,
};

/* --------------------------------------------- */
static int __init SYSRAM_Init(void)
{
#if 0
	struct proc_dir_entry *pEntry;
#endif
	/*  */
	LOG_MSG("E");
	/*  */
	if (platform_driver_register(&SysramPlatformDriver)) {
		LOG_ERR("failed to register sysram driver");
		return -ENODEV;
	}
	/*  */
	/* linux-3.10 procfs API changed */
#if 1
	proc_create("sysram", 0444, NULL, &fsysram_proc_fops);
	proc_create("sysram_flag", 0444, NULL, &fsysram_flag_proc_fops);
#else
	pEntry = create_proc_entry("sysram", 0, NULL);
	if (pEntry) {
		pEntry->read_proc = SYSRAM_DumpLayoutToProc;
		pEntry->write_proc = NULL;
	} else {
		LOG_ERR("add /proc/sysram entry fail.");
	}
	/*  */
	pEntry = create_proc_entry("sysram_flag", 0, NULL);
	if (pEntry) {
		pEntry->read_proc = SYSRAM_ReadFlag;
		pEntry->write_proc = SYSRAM_WriteFlag;
	} else {
		LOG_ERR("add /proc/sysram_flag entry fail");
	}
#endif
	LOG_MSG("X");
	/*  */
	return 0;
}

/* --------------------------------------------- */
static void __exit SYSRAM_Exit(void)
{
	LOG_MSG("E");
	platform_driver_unregister(&SysramPlatformDriver);
	LOG_MSG("X");
}

/* --------------------------------------------- */
module_init(SYSRAM_Init);
module_exit(SYSRAM_Exit);
MODULE_DESCRIPTION("Camera sysram driver");
MODULE_AUTHOR("Marx <marx.chiu@mediatek.com>");
MODULE_LICENSE("GPL");
/* --------------------------------------------- */
