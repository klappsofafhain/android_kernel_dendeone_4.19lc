// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include "inc/camera_isp_FrmB.h"

#include <linux/types.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/time.h>
/*  */
/*#include <mach/hardware.h>*/
/*#include <mach/mt_reg_base.h>*/

#include <m4u.h>

#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif
/*  */
/* for systrace */
#define CONFIG_K_FOR_SYSTRACE     0	/* default:0 */
#if CONFIG_K_FOR_SYSTRACE
#include <linux/kallsyms.h>
#include <linux/ftrace_event.h>
static unsigned long __read_mostly tracing_mark_write_addr;
#define _kernel_trace_begin(name) {\
	tracing_mark_write_addr = kallsyms_lookup_name("tracing_mark_write");\
	event_trace_printk(tracing_mark_write_addr,\
	"B|%d|%s\n", current->tgid, name);\
	}
#define _kernel_trace_end() {\
	event_trace_printk(tracing_mark_write_addr,  "E\n");\
	}
#endif
/* / */

/*  */
#define CAMSV_DBG
#ifdef CAMSV_DBG
#define CAM_TAG "CAM:"
#define CAMSV_TAG "SV1:"
#define CAMSV2_TAG "SV2:"
#else
#define CAMSV_TAG ""
#define CAMSV2_TAG ""
#define CAM_TAG ""
#endif
/*  */
#ifndef MTRUE
#define MTRUE               1
#endif
#ifndef MFALSE
#define MFALSE              0
#endif



/* --------------------------------------------- */
#define MyTag "ISP"
#define LOG_DBG(format, args...)    pr_debug(MyTag format, ##args)
#define LOG_INF(format, args...)    pr_info(MyTag format,  ##args)
#define LOG_NOTICE(format, args...) pr_notice(MyTag format,  ##args)
#define LOG_WRN(format, args...)    pr_warn(MyTag format,  ##args)
#define LOG_ERR(format, args...)    pr_err(MyTag format,  ##args)
#define LOG_AST(format, args...)    pr_alert(MyTag format, ##args)


/*************************************************
 *
 *************************************************/
#define ISP_WR32(addr, data)    iowrite32(data, (void *)addr)
#define ISP_RD32(addr)          ioread32((void *)addr)
#define ISP_SET_BIT(reg, bit)   ((*(unsigned int *)(reg)) \
|= (unsigned int)(1 << (bit)))
#define ISP_CLR_BIT(reg, bit)   ((*(unsigned int *)(reg)) \
&= ~((unsigned int)(1 << (bit))))


/*************************************************
 *
 *************************************************/
#define ISP_DBG_INT                 (0x00000001)
#define ISP_DBG_HOLD_REG            (0x00000002)
#define ISP_DBG_READ_REG            (0x00000004)
#define ISP_DBG_WRITE_REG           (0x00000008)
#define ISP_DBG_CLK                 (0x00000010)
#define ISP_DBG_TASKLET             (0x00000020)
#define ISP_DBG_SCHEDULE_WORK       (0x00000040)
#define ISP_DBG_BUF_WRITE           (0x00000080)
#define ISP_DBG_BUF_CTRL            (0x00000100)
#define ISP_DBG_REF_CNT_CTRL        (0x00000200)
#define ISP_DBG_INT_2               (0x00000400)
#define ISP_DBG_INT_3               (0x00000800)

#ifdef CONFIG_OF
enum ISP_CAM_IRQ_ENUM {
	ISP_SENINF_IRQ_IDX = 0,
	ISP_CAM0_IRQ_IDX,
	ISP_CAM_IRQ_IDX_NUM
};

enum ISP_CAM_BASEADDR_ENUM {
	ISP_BASE_ADDR = 0,
	ISP_IMGSYS_CONFIG_BASE_ADDR,
	ISP_MIPI_ANA_BASE_ADDR,
	ISP_CAM_BASEADDR_NUM
};

static unsigned long gISPSYS_Irq_FrmB[ISP_CAM_IRQ_IDX_NUM];
static unsigned long gISPSYS_Reg_FrmB[ISP_CAM_BASEADDR_NUM];
#define ISP_ADDR         (gISPSYS_Reg_FrmB[ISP_BASE_ADDR])
#define ISP_IMGSYS_BASE  (gISPSYS_Reg_FrmB[ISP_IMGSYS_CONFIG_BASE_ADDR])
#define ISP_ADDR_CAMINF  (gISPSYS_Reg_FrmB[ISP_IMGSYS_CONFIG_BASE_ADDR])
#define CAMINF_BASE      (gISPSYS_Reg_FrmB[ISP_IMGSYS_CONFIG_BASE_ADDR])
#define ISP_IMGSYS_BASE_PHY	        0x15000000
#else
#define CAMINF_BASE                     0xf5000000
#define ISP_ADDR                        (CAMINF_BASE + 0x4000)
#define ISP_ADDR_CAMINF                 CAMINF_BASE
#endif

/*
 * #define ISP_ADDR (CAMINF_BASE + 0x4000)
 * #define ISP_ADDR_CAMINF CAMINF_BASE
 */
#define ISP_REG_ADDR_EN1                (ISP_ADDR + 0x4)
#define ISP_REG_ADDR_INT_STATUS         (ISP_ADDR + 0x24)
#define ISP_REG_ADDR_DMA_INT            (ISP_ADDR + 0x28)
#define ISP_REG_ADDR_INTB_STATUS        (ISP_ADDR + 0x30)
#define ISP_REG_ADDR_DMAB_INT           (ISP_ADDR + 0x34)
#define ISP_REG_ADDR_INTC_STATUS        (ISP_ADDR + 0x3C)
#define ISP_REG_ADDR_DMAC_INT           (ISP_ADDR + 0x40)
#define ISP_REG_ADDR_INT_STATUSX        (ISP_ADDR + 0x44)
#define ISP_REG_ADDR_DMA_INTX           (ISP_ADDR + 0x48)
#define ISP_REG_ADDR_SW_CTL             (ISP_ADDR + 0x5C)
#define ISP_REG_ADDR_CQ0C_BASE_ARRR     (ISP_ADDR + 0xBC)
#define ISP_REG_ADDR_CQ0_CUR_BASE_ARRR  (ISP_ADDR + 0xE8)
#define ISP_REG_ADDR_IMGO_FBC           (ISP_ADDR + 0xF4)
#define ISP_REG_ADDR_IMG2O_FBC          (ISP_ADDR + 0xF8)
#define ISP_REG_ADDR_FBC_INT            (ISP_ADDR + 0xFC)
#define ISP_REG_ADDR_IMGO_BASE_ADDR     (ISP_ADDR + 0x300)
#define ISP_REG_ADDR_IMG2O_BASE_ADDR    (ISP_ADDR + 0x320)
#define ISP_REG_ADDR_TG_VF_CON          (ISP_ADDR + 0x414)
#define ISP_REG_ADDR_CTL_DBG_SET        (ISP_ADDR + 0x160)
#define ISP_REG_ADDR_CTL_DBG_PORT       (ISP_ADDR + 0x164)
#define ISP_REG_ADDR_CTL_EN2            (ISP_ADDR + 0x008)
#define ISP_REG_ADDR_CTL_CROP_X         (ISP_ADDR + 0x110)

#define ISP_REG_ADDR_CTL_DBG_SET_CQ_STS                (0x6000)

#define ISP_REG_ADDR_CTL_DBG_SET_IMGI_STS              (0x9003)
#define ISP_REG_ADDR_CTL_DBG_SET_IMGI_SYNC             (0x9004)
#define ISP_REG_ADDR_CTL_DBG_SET_IMGI_NO_SYNC          (0x9005)

#define ISP_REG_ADDR_CTL_DBG_SET_CFA_STS               (0x9006)
#define ISP_REG_ADDR_CTL_DBG_SET_CFA_SYNC              (0x9007)
#define ISP_REG_ADDR_CTL_DBG_SET_CFA_NO_SYNC           (0x9008)

#define ISP_REG_ADDR_CTL_DBG_SET_YUV_STS               (0x9009)
#define ISP_REG_ADDR_CTL_DBG_SET_YUV_SYNC              (0x900a)
#define ISP_REG_ADDR_CTL_DBG_SET_YUV_NO_SYNC           (0x900b)

#define ISP_REG_ADDR_CTL_DBG_SET_OUT_STS               (0x900c)
#define ISP_REG_ADDR_CTL_DBG_SET_OUT_SYNC              (0x900d)
#define ISP_REG_ADDR_CTL_DBG_SET_OUT_NO_SYNC           (0x900e)

#define ISP_REG_ADDR_CTL_EN2_UV_CRSA_EN_BIT            (1<<23)
#define ISP_REG_ADDR_CTL_CROP_X_MDP_CROP_EN_BIT        (1<<15)

/* #define ISP_REG_ADDR_TG2_VF_CON         (ISP_ADDR + 0x4B4) */


#define ISP_TPIPE_ADDR                  (0x15004000)

/* // */
#define ISP_REG_ADDR_PIX_ID             (ISP_ADDR + 0x001C)
#define ISP_REG_ADDR_TG_MAGIC_0         (CAMINF_BASE + 0x43DC)

/* ////////////////////////////////////////////////////////////// */

/* remp to fix build err (need to be removed) 95 */
/* #define ISP_REG_ADDR_IMGO_FBC           (ISP_ADDR + 0xF0) */
/* #define ISP_REG_ADDR_RRZO_FBC           (ISP_ADDR + 0xF4) */
/* #define ISP_REG_ADDR_IMGO_D_FBC         (ISP_ADDR + 0xF8) */
/* #define ISP_REG_ADDR_RRZO_D_FBC         (ISP_ADDR + 0xFC) */
/* #define ISP_REG_ADDR_TG_VF_CON          (ISP_ADDR + 0x414) */
#define ISP_REG_ADDR_TG_INTER_ST        (ISP_ADDR + 0x44C)
/* #define ISP_REG_ADDR_TG2_VF_CON         (ISP_ADDR + 0x2414) */
/* #define ISP_REG_ADDR_TG2_INTER_ST       (ISP_ADDR + 0x244C) */
/* #define ISP_REG_ADDR_IMGO_BASE_ADDR     (ISP_ADDR + 0x3300) */
/* #define ISP_REG_ADDR_RRZO_BASE_ADDR     (ISP_ADDR + 0x3320) */
/* #define ISP_REG_ADDR_IMGO_D_BASE_ADDR   (ISP_ADDR + 0x34D4) */
/* #define ISP_REG_ADDR_RRZO_D_BASE_ADDR   (ISP_ADDR + 0x34F4) */
/* #define ISP_REG_ADDR_CAMSV_FMT_SEL      (ISP_ADDR + 0x5004) */
/* #define ISP_REG_ADDR_CAMSV_INT          (ISP_ADDR + 0x500C) */
/* #define ISP_REG_ADDR_CAMSV_SW_CTL       (ISP_ADDR + 0x5010) */
/* #define ISP_REG_ADDR_CAMSV_TG_INTER_ST  (ISP_ADDR + 0x544C) */
/* #define ISP_REG_ADDR_CAMSV2_FMT_SEL     (ISP_ADDR + 0x5804) */
/* #define ISP_REG_ADDR_CAMSV2_INT         (ISP_ADDR + 0x580C) */
/* #define ISP_REG_ADDR_CAMSV2_SW_CTL      (ISP_ADDR + 0x5810) */
/* #define ISP_REG_ADDR_CAMSV_TG2_INTER_ST (ISP_ADDR + 0x5C4C) */
/* #define ISP_REG_ADDR_CAMSV_IMGO_FBC     (ISP_ADDR + 0x501C) */
/* #define ISP_REG_ADDR_CAMSV2_IMGO_FBC    (ISP_ADDR + 0x581C) */
/* #define ISP_REG_ADDR_IMGO_SV_BASE_ADDR  (ISP_ADDR + 0x5208) */
/* #define ISP_REG_ADDR_IMGO_SV_XSIZE      (ISP_ADDR + 0x5210) */
/* #define ISP_REG_ADDR_IMGO_SV_YSIZE      (ISP_ADDR + 0x5214) */
/* #define ISP_REG_ADDR_IMGO_SV_STRIDE     (ISP_ADDR + 0x5218) */
/* #define ISP_REG_ADDR_IMGO_SV_D_BASE_ADDR    (ISP_ADDR + 0x5228) */
/* #define ISP_REG_ADDR_IMGO_SV_D_XSIZE    (ISP_ADDR + 0x5230) */
/* #define ISP_REG_ADDR_IMGO_SV_D_YSIZE    (ISP_ADDR + 0x5234) */
/* #define ISP_REG_ADDR_IMGO_SV_D_STRIDE   (ISP_ADDR + 0x5238) */
#define TG_REG_ADDR_GRAB_W              (ISP_ADDR + 0x418)
/* #define TG2_REG_ADDR_GRAB_W             (ISP_ADDR + 0x2418) */
#define TG_REG_ADDR_GRAB_H              (ISP_ADDR + 0x41C)
#define ISP_REG_ADDR_FMT_SEL_P1         (ISP_ADDR + 0x0010)
/* #define ISP_INNER_REG_ADDR_FMT_SEL_P1_D     (ISP_ADDR + 0x002C) */
/* #define ISP_INNER_REG_ADDR_FMT_SEL_P1       (ISP_ADDR_CAMINF + 0xD028) */
/* #define ISP_INNER_REG_ADDR_FMT_SEL_P1_D     (ISP_ADDR_CAMINF + 0xD02C) */
#define ISP_REG_ADDR_IMGO_CROP        (ISP_ADDR_CAMINF + 0x431C)
#define ISP_REG_ADDR_IMGO_XSIZE       (ISP_ADDR_CAMINF + 0x4308)
#define ISP_REG_ADDR_IMGO_YSIZE       (ISP_ADDR_CAMINF + 0x430C)
#define ISP_REG_ADDR_IMGO_STRIDE      (ISP_ADDR_CAMINF + 0x4310)
#define ISP_REG_ADDR_IMG2O_CROP        (ISP_ADDR_CAMINF + 0x433C)
#define ISP_REG_ADDR_IMG2O_XSIZE       (ISP_ADDR_CAMINF + 0x4328)
#define ISP_REG_ADDR_IMG2O_YSIZE       (ISP_ADDR_CAMINF + 0x432C)
#define ISP_REG_ADDR_IMG2O_STRIDE      (ISP_ADDR_CAMINF + 0x4330)
/* #define ISP_INNER_REG_ADDR_IMGO_D_XSIZE     (ISP_ADDR_CAMINF + 0xF4DC) */
/* #define ISP_INNER_REG_ADDR_IMGO_D_YSIZE     (ISP_ADDR_CAMINF + 0xF4E0) */
/* #define ISP_INNER_REG_ADDR_IMGO_D_STRIDE    (ISP_ADDR_CAMINF + 0xF4E4) */
/* #define ISP_INNER_REG_ADDR_RRZO_D_XSIZE     (ISP_ADDR_CAMINF + 0xF4FC) */
/* #define ISP_INNER_REG_ADDR_RRZO_D_YSIZE     (ISP_ADDR_CAMINF + 0xF500) */
/* #define ISP_INNER_REG_ADDR_RRZO_D_STRIDE    (ISP_ADDR_CAMINF + 0xF504) */
/* #define ISP_INNER_REG_ADDR_RRZ_HORI_INT_OFST
 * (ISP_ADDR_CAMINF + 0xD7B4) //FLORIA disable
 */
/* #define ISP_INNER_REG_ADDR_RRZ_VERT_INT_OFST
 * (ISP_ADDR_CAMINF + 0xD7BC) //FLORIA disable
 */
/* #define ISP_INNER_REG_ADDR_RRZ_IN_IMG
 * (ISP_ADDR_CAMINF + 0xD7A4) //FLORIA disable
 */
/* #define ISP_INNER_REG_ADDR_RRZ_OUT_IMG
 * (ISP_ADDR_CAMINF + 0xD7A8) //FLORIA disable
 */
/* #define ISP_INNER_REG_ADDR_RRZ_D_HORI_INT_OFST (ISP_ADDR_CAMINF + 0xE7B4) */
/* #define ISP_INNER_REG_ADDR_RRZ_D_VERT_INT_OFST (ISP_ADDR_CAMINF + 0xE7BC) */
/* #define ISP_INNER_REG_ADDR_RRZ_D_IN_IMG        (ISP_ADDR_CAMINF + 0xE7A4) */
/* #define ISP_INNER_REG_ADDR_RRZ_D_OUT_IMG       (ISP_ADDR_CAMINF + 0xE7A8) */
#define ISP_REG_ADDR_HRZ_OUT_IMG        (ISP_ADDR_CAMINF + 0x4584)

/* #define ISP_REG_ADDR_TG_MAGIC_0         (CAMINF_BASE + 0x75DC) //0088 */
/* #define ISP_REG_ADDR_TG2_MAGIC_0        (CAMINF_BASE + 0x75E4) //0090 */

/* //////////////////////////////////////////// */

/*************************************************
 * struct & enum
 *************************************************/

/* //////////////////////////////////////////// */

static signed int gEismetaRIdx;
static signed int gEismetaWIdx;
static signed int gEismetaInSOF;

#define EISMETA_RINGSIZE 4
/* record remain node count(success/fail)
 * excludes head when enque/deque control
 */
static signed int EDBufQueRemainNodeCnt;

static wait_queue_head_t WaitQueueHead_EDBuf_WaitDeque;
static wait_queue_head_t WaitQueueHead_EDBuf_WaitFrame;
static spinlock_t SpinLockEDBufQueList;
#define _MAX_SUPPORT_P2_FRAME_NUM_ 512
#define _MAX_SUPPORT_P2_BURSTQ_NUM_ 4
static signed int P2_Support_BurstQNum = 1;
#define _MAX_SUPPORT_P2_PACKAGE_NUM_ \
(_MAX_SUPPORT_P2_FRAME_NUM_/_MAX_SUPPORT_P2_BURSTQ_NUM_)
#define P2_EDBUF_MLIST_TAG 1
#define P2_EDBUF_RLIST_TAG 2
struct ISP_EDBUF_STRUCT {
	unsigned int processID;	/* caller process ID */
	unsigned int callerID;	/* caller thread ID */
	/* p2 duplicate CQ index(for recognize
	 * belong to which package)
	 */
	signed int p2dupCQIdx;

	enum ISP_ED_BUF_STATE_ENUM bufSts;	/* buffer status */
};
static signed int P2_EDBUF_RList_FirstBufIdx;
static signed int P2_EDBUF_RList_CurBufIdx;
static signed int P2_EDBUF_RList_LastBufIdx;
static struct ISP_EDBUF_STRUCT P2_EDBUF_RingList[
_MAX_SUPPORT_P2_FRAME_NUM_];

struct ISP_EDBUF_MGR_STRUCT {
	unsigned int processID;	/* caller process ID */
	unsigned int callerID;	/* caller thread ID */
	/* p2 duplicate CQ index(for recognize belong
	 * to which package)
	 */
	signed int p2dupCQIdx;
	/* number of dequed buffer no matter deque
	 * success or fail
	 */
	signed int dequedNum;
};
static signed int P2_EDBUF_MList_FirstBufIdx;
/* static volatile signed int P2_EDBUF_MList_CurBufIdx=0; */
static signed int P2_EDBUF_MList_LastBufIdx;
static struct ISP_EDBUF_MGR_STRUCT P2_EDBUF_MgrList[
_MAX_SUPPORT_P2_PACKAGE_NUM_];

// static unsigned int g_regScen = 0xa5a5a5a5;
static spinlock_t SpinLockRegScen;

/* m4u_callback_ret_t ISP_M4U_TranslationFault_
 * callback(int port, unsigned int mva, void* data);
 */


/*************************************************
 *
 *************************************************/
/* internal data */
/* pointer to the kmalloc'd area, rounded up to a page boundary */
static int *pTbl_RTBuf;
/* original pointer for kmalloc'd area as returned by kmalloc */
static void *pBuf_kmalloc;
/*  */
/*static volatile ISP_RT_BUF_STRUCT_FRMB *pstRTBuf_FrmB = NULL;*/
static struct ISP_RT_BUF_STRUCT_FRMB *pstRTBuf_FrmB;

/* static ISP_DEQUE_BUF_INFO_STRUCT g_deque_buf = {0,{}};
 * Marked to remove build warning.
 */

unsigned long g_Flash_SpinLock;


// static unsigned int G_u4EnableClockCount;


/*************************************************
 *
 *************************************************/
#define ISP_BUF_SIZE            (4096)
#define ISP_BUF_SIZE_WRITE      1024
#define ISP_BUF_WRITE_AMOUNT    6

enum ISP_BUF_STATUS_ENUM_FRMB {
	ISP_BUF_STATUS_EMPTY_FRMB,
	ISP_BUF_STATUS_HOLD_FRMB,
	ISP_BUF_STATUS_READY_FRMB
};

struct ISP_BUF_STRUCT_FRMB {
	enum ISP_BUF_STATUS_ENUM_FRMB Status;
	unsigned int Size;
	unsigned char *pData;
};

struct ISP_BUF_INFO_STRUCT_FRMB {
	struct ISP_BUF_STRUCT_FRMB Read;
	struct ISP_BUF_STRUCT_FRMB Write[ISP_BUF_WRITE_AMOUNT];
};


/*************************************************
 *
 *************************************************/
/*
 * typedef struct
 * {
 *     atomic_t            HoldEnable;
 *     atomic_t            WriteEnable;
 *     ISP_HOLD_TIME_ENUM_FRMB  Time;
 * }ISP_HOLD_INFO_STRUCT_FRMB;
 *
 * typedef struct
 * {
 *    unsigned int     Vd;
 *    unsigned int     Expdone;
 *    unsigned int     WorkQueueVd;
 *    unsigned int     WorkQueueExpdone;
 *    unsigned int     TaskletVd;
 *    unsigned int     TaskletExpdone;
 * }ISP_TIME_LOG_STRUCT_FRMB;
 */




enum _eChannel {
	_PASS1 = 0,
	_PASS1_D = 1,
	_CAMSV = 2,
	_CAMSV_D = 3,
	_PASS2 = 4,
	_ChannelMax = 5,
};

#if 1
#define DMA_TRANS(dma, Out) \
do { \
	if (dma == _imgo_) {\
		Out = _PASS1;\
	} \
} while (0)
#else
#define DMA_TRANS(dma, Out) \
do { \
	if (dma == _imgo_ || dma == _rrzo_) {\
		Out = _PASS1;\
	} \
	else if (dma == _imgo_d_ || dma == _rrzo_d_) { \
		Out = _PASS1_D;\
	} \
	else if (dma == _camsv_imgo_) {\
		Out = _CAMSV;\
	} \
	else if (dma == _camsv2_imgo_) {\
		Out = _CAMSV_D;\
	} \
} while (0)
#endif

enum _eLOG_TYPE {
	/* currently, only used at ipl_buf_ctrl.
	 * to protect critical section
	 */
	_LOG_DBG = 0,
	_LOG_INF = 1,
	_LOG_ERR = 2,
	_LOG_MAX = 3,
};

enum _eLOG_OP {
	_LOG_INIT = 0,
	_LOG_RST = 1,
	_LOG_ADD = 2,
	_LOG_PRT = 3,
	_LOG_GETCNT = 4,
	_LOG_OP_MAX = 5
};

#define NORMAL_STR_LEN (512)
#define ERR_PAGE 2
#define DBG_PAGE 2
#define INF_PAGE 4
/* #define SV_LOG_STR_LEN NORMAL_STR_LEN */

#define LOG_PPNUM 2
static unsigned int m_CurrentPPB;
struct SV_LOG_STR {
	unsigned int _cnt[LOG_PPNUM][_LOG_MAX];
	/* char   _str[_LOG_MAX][SV_LOG_STR_LEN]; */
	char *_str[LOG_PPNUM][_LOG_MAX];
} *PSV_LOG_STR;

static void *pLog_kmalloc;
static struct SV_LOG_STR gSvLog[_IRQ_MAX];
/* static SV_LOG_STR gSvLog_IRQ = {0}; */
/* static SV_LOG_STR gSvLog_CAMSV_IRQ= {0}; */
/* static SV_LOG_STR gSvLog_CAMSV_D_IRQ= {0}; */
static bool g_bDmaERR_p1 = MFALSE;
static bool g_bDmaERR_p1_d = MFALSE;
// static bool g_bDmaERR_p2 = MFALSE;
static bool g_bDmaERR_deepDump = MFALSE;
static unsigned int g_ISPIntErr[_IRQ_MAX] = { 0 };

#define nDMA_ERR_P1     (6)
#define nDMA_ERR_P1_D   (0)
#define nDMA_ERR    (nDMA_ERR_P1 + nDMA_ERR_P1_D)
static unsigned int g_DmaErr_p1[nDMA_ERR] = { 0 };

/*
 * for irq used,keep log until IRQ_LOG_PRINTER
 * being involked, limited:
 * each log must shorter than 512 bytes
 * total log length in each irq/logtype
 * can't over 1024 bytes
 */
#define IRQ_LOG_KEEPER_T(sec, usec) {\
	ktime_t time;           \
	time = ktime_get();     \
	sec = time;        \
	do_div(sec, 1000);    \
	usec = do_div(sec, 1000000);\
}
#if 1
#define IRQ_LOG_KEEPER(irq, ppb, logT, fmt, ...) \
do {\
	char *ptr; \
	char *pDes;\
	unsigned int *ptr2 = &gSvLog[irq]._cnt[ppb][logT];\
	unsigned int str_leng;\
	if (logT == _LOG_ERR) {\
		str_leng = NORMAL_STR_LEN*ERR_PAGE; \
	} else if (logT == _LOG_DBG) {\
		str_leng = NORMAL_STR_LEN*DBG_PAGE; \
	} else if (logT == _LOG_INF) {\
		str_leng = NORMAL_STR_LEN*INF_PAGE;\
	} else {\
		str_leng = 0;\
	} \
	ptr = pDes = (char *)&(gSvLog[irq]._str[ppb] \
	[logT][gSvLog[irq]._cnt[ppb][logT]]);    \
	if (sprintf((char *)(pDes), fmt, ##__VA_ARGS__) < 0) {\
		LOG_ERR("Error: sprintf fail");\
	}\
	if ('\0' != gSvLog[irq]._str[ppb][logT][str_leng - 1]) {\
		LOG_ERR("log str over flow(%d)", irq);\
	} \
	while (*ptr++ != '\0') {        \
		(*ptr2)++;\
	}     \
} while (0)
#else
#define IRQ_LOG_KEEPER(irq, ppb, logT, fmt, ...) \
pr_debug("KEEPER[%s] " fmt, __func__, ##__VA_ARGS__)
#endif

#if 1
#define IRQ_LOG_PRINTER(irq, ppb_in, logT_in) \
do {\
	struct SV_LOG_STR *pSrc = &gSvLog[irq];\
	char *ptr;\
	unsigned int i;\
	unsigned int ppb = 0;\
	unsigned int logT = 0;\
	if (ppb_in > 1) {\
		ppb = 1;\
	} else{\
		ppb = ppb_in;\
	} \
	if (logT_in > _LOG_ERR) {\
		logT = _LOG_ERR;\
	} else{\
		logT = logT_in;\
	} \
        if ((ppb < 0) || (ppb >= LOG_PPNUM) || (logT < 0) || (logT >= _LOG_MAX)) {\
		LOG_ERR("Error: _str Invalid with index ppb(%d) ,logT(%d)",ppb,logT);\
		break;\
	}\
	ptr = pSrc->_str[ppb][logT];\
	if (pSrc->_cnt[ppb][logT] != 0) {\
		if (logT == _LOG_DBG) {\
			for (i = 0; i < DBG_PAGE; i++) {\
				if (ptr[NORMAL_STR_LEN*(i+1) - 1] != '\0') {\
					ptr[NORMAL_STR_LEN*(i+1) - 1] = '\0';\
					LOG_DBG("%s", &ptr[NORMAL_STR_LEN*i]);\
				} else{\
					LOG_DBG("%s", &ptr[NORMAL_STR_LEN*i]);\
					break;\
				} \
			} \
		} \
		else if (logT == _LOG_INF) {\
			for (i = 0; i < INF_PAGE; i++) {\
				if (ptr[NORMAL_STR_LEN*(i+1) - 1] != '\0') {\
					ptr[NORMAL_STR_LEN*(i+1) - 1] = '\0';\
					LOG_INF("%s", &ptr[NORMAL_STR_LEN*i]);\
				} else{\
					LOG_INF("%s", &ptr[NORMAL_STR_LEN*i]);\
					break;\
				} \
			} \
		} \
		else if (logT == _LOG_ERR) {\
			for (i = 0; i < ERR_PAGE; i++) {\
				if (ptr[NORMAL_STR_LEN*(i+1) - 1] != '\0') {\
					ptr[NORMAL_STR_LEN*(i+1) - 1] = '\0';\
					LOG_ERR("%s", &ptr[NORMAL_STR_LEN*i]);\
				} else{\
					LOG_ERR("%s", &ptr[NORMAL_STR_LEN*i]);\
					break;\
				} \
			} \
		} \
		else {\
			LOG_ERR("N.S.%d", logT);\
		} \
		ptr[0] = '\0';\
		pSrc->_cnt[ppb][logT] = 0;\
	} \
} while (0)


#else
#define IRQ_LOG_PRINTER(irq, ppb, logT)
#endif

/**********************************************************************/
#define my_get_pow_idx(value)      \
({                                                          \
	int i = 0, cnt = 0;                                  \
	for (i = 0; i < 32; i++) {                            \
		if ((value>>i) & (0x00000001)) {    \
			break; }                                            \
		else {                                           \
			cnt++; }                                      \
	}                                                    \
	cnt;                                                \
})
/* ////////////////////// Interrupt ///////////////////////////// */

/* maximum number for supporting user to do interrupt operation */
/* index 0 is for all the user that do not do register irq first */
#define IRQ_USER_NUM_MAX 32
static spinlock_t SpinLock_UserKey;

static signed int FirstUnusedIrqUserKey = 1;
#define USERKEY_STR_LEN 128

struct UserKeyInfo {
	/* name for the user that register a userKey */
	char userName[USERKEY_STR_LEN];
	int userKey;		/* the user key for that user */
};
/* array for recording the user name for
 * a specific user key
 */
static struct UserKeyInfo IrqUserKey_UserInfo[
IRQ_USER_NUM_MAX];
struct ISP_IRQ_INFO_STRUCT_FRMB {
/* interrupt status for each user in irqType/irqBit */
	unsigned int Status[IRQ_USER_NUM_MAX][ISP_IRQ_TYPE_AMOUNT];
	unsigned int Mask[ISP_IRQ_TYPE_AMOUNT];
	unsigned int ErrMask[ISP_IRQ_TYPE_AMOUNT];

	/* flag for indicating that user do mark for
	 * a interrupt or not
	 */
	unsigned int MarkedFlag[IRQ_USER_NUM_MAX][
	ISP_IRQ_TYPE_AMOUNT];

	/* time for marking a specific interrupt */
	unsigned int MarkedTime_sec[IRQ_USER_NUM_MAX][
	ISP_IRQ_TYPE_AMOUNT][32];

	/* time for marking a specific interrupt */
	unsigned int MarkedTime_usec[IRQ_USER_NUM_MAX][
	ISP_IRQ_TYPE_AMOUNT][32];

	/* number of a specific signal that passed by */
	signed int PassedBySigCnt[IRQ_USER_NUM_MAX][
	ISP_IRQ_TYPE_AMOUNT][32];


	/* latest occurring time for each interrupt */
	unsigned int LastestSigTime_sec[ISP_IRQ_TYPE_AMOUNT][32];

	/* latest occurring time for each interrupt */
	unsigned int LastestSigTime_usec[ISP_IRQ_TYPE_AMOUNT][32];

	/* eis meta only for p1 and p1_d */
	struct ISP_EIS_META_STRUCT Eismeta[
	ISP_IRQ_TYPE_INTB][EISMETA_RINGSIZE];

};

struct ISP_INFO_STRUCT_FRMB {
/* currently, IRQ and IRQ_D share the same ISR ,
 * so share the same key,IRQ.
 */
	spinlock_t SpinLockIrq[_IRQ_MAX];
	spinlock_t SpinLockRTBC;
	wait_queue_head_t WaitQueueHead;
	unsigned int DebugMask;
	struct ISP_IRQ_INFO_STRUCT_FRMB IrqInfo;
	struct ISP_BUF_INFO_STRUCT_FRMB BufInfo;

/*
 * spinlock_t SpinLockIspRef;
 * spinlock_t SpinLockIsp;
 * spinlock_t SpinLockHold;
 * spinlock_t SpinLockClock;
 * volatile wait_queue_head_t WaitQHeadList[SUPPORT_MAX_IRQ];
 * struct work_struct ScheduleWorkVD;
 * struct work_struct ScheduleWorkEXPDONE;
 * signed int IrqNum;
 * unsigned int UserCount;
 * ISP_HOLD_INFO_STRUCT_FRMB HoldInfo;
 * ISP_TIME_LOG_STRUCT_FRMB TimeLog;
 * ISP_CALLBACK_STRUCT_FRMB Callback[ISP_CALLBACK_AMOUNT];
 */
};

static struct ISP_INFO_STRUCT_FRMB IspInfo_FrmB;
unsigned int t_SOF;	/* (ns) */

/**********************************************************************/


unsigned int PrvAddr[_ChannelMax] = { 0 };

/*************************************************
 *
 *************************************************/
#ifdef T_STAMP_2_0
#define SlowMotion  100
struct T_STAMP {
/* 1st frame start time, accurency in us,unit in ns */
	unsigned long long T_ns;
	unsigned long interval_us;	/* unit in us */
	unsigned long compensation_us;
	unsigned int fps;
	unsigned int fcnt;
};

static struct T_STAMP m_T_STAMP = { 0 };
#endif

/* ////////////////////////////////////////////// */
/* keep another one due to static functions could not be externed */
/*************************************************
 *
 *************************************************/
static inline unsigned int ISP_MsToJiffies_FrmB(unsigned int Ms)
{
	return (Ms * HZ + 512) >> 10;
}

/*************************************************
 *
 *************************************************/
static inline unsigned int ISP_UsToJiffies_FrmB(unsigned int Us)
{
	return ((Us / 1000) * HZ + 512) >> 10;
}

/*************************************************
 *
 *************************************************/
static inline unsigned int ISP_JiffiesToMs_FrmB(unsigned int Jiffies)
{
	return (Jiffies * 1000) / HZ;
}

/*************************************************
 *
 *************************************************/
static inline unsigned int ISP_GetIRQState_FrmB(enum eISPIrq
eIrq, unsigned int type, unsigned int userKey, unsigned int stus)
{
	unsigned int ret;
	unsigned long flags;/*unsigned int flags;*/
	/*  */
	spin_lock_irqsave(&(IspInfo_FrmB.SpinLockIrq[eIrq]), flags);
	ret = (IspInfo_FrmB.IrqInfo.Status[userKey][type] & stus);
	spin_unlock_irqrestore(&(IspInfo_FrmB.SpinLockIrq[eIrq]), flags);
	/*  */
	return ret;
}

/*************************************************
 *
 *************************************************/
static inline unsigned int ISP_GetEDBufQueWaitDequeState(signed int idx)
{
	unsigned int ret = MFALSE;
	/*  */
	spin_lock(&(SpinLockEDBufQueList));
	if (P2_EDBUF_RingList[idx].bufSts == ISP_ED_BUF_STATE_RUNNING)
		ret = MTRUE;
	spin_unlock(&(SpinLockEDBufQueList));
	/*  */
	return ret;
}

static inline unsigned int ISP_GetEDBufQueWaitFrameState(signed int idx)
{
	unsigned int ret = MFALSE;
	/*  */
	spin_lock(&(SpinLockEDBufQueList));
	if (P2_EDBUF_MgrList[idx].dequedNum == P2_Support_BurstQNum)
		ret = MTRUE;
	spin_unlock(&(SpinLockEDBufQueList));
	/*  */
	return ret;
}

extern signed int ISP_DumpReg(void);

/*************************************************
 *
 *************************************************/
static unsigned int ISP_DumpDmaDeepDbg(void)
{
#define _BASE (ISP_ADDR + 0X200)
	if (g_bDmaERR_p1) {
		g_DmaErr_p1[0] = (unsigned int) ISP_RD32(_BASE + 0x1A8);
		g_DmaErr_p1[1] = (unsigned int) ISP_RD32(_BASE + 0x1B0);
		g_DmaErr_p1[2] = (unsigned int) ISP_RD32(_BASE + 0x1C4);
		g_DmaErr_p1[3] = (unsigned int) ISP_RD32(_BASE + 0x1C8);
		g_DmaErr_p1[4] = (unsigned int) ISP_RD32(_BASE + 0x1D0);
		g_DmaErr_p1[5] = (unsigned int) ISP_RD32(_BASE + 0x1D4);

		LOG_ERR(
		"IMGI:0x%x,LSCI=0x%x,imgo=0x%x,img2o:0x%x,esfko:0x%x,aao:0x%x",
		g_DmaErr_p1[0], g_DmaErr_p1[1],
		g_DmaErr_p1[2], g_DmaErr_p1[3],
		g_DmaErr_p1[4], g_DmaErr_p1[5]);
		g_bDmaERR_p1 = MFALSE;
	}
	if (g_bDmaERR_p1_d)
		g_bDmaERR_p1_d = MFALSE;
#if 0
	if (g_bDmaERR_p2) {
		LOG_ERR(
		"vipi:0x%x,VIPI:0x%x,VIP2I=0x%x,VIP3I=0x%x,MFBO=0x%x,IMG3BO=0x%x,IMG3CO:0x%x,IMG2O:0x%x,IMG3O:0x%x,FEO:0x%x",
		ISP_RD32(ISP_ADDR + 0x3574), ISP_RD32(ISP_ADDR + 0x3580),
		ISP_RD32(ISP_ADDR + 0x3584), ISP_RD32(ISP_ADDR + 0x3588),
		ISP_RD32(ISP_ADDR + 0x35a4), ISP_RD32(ISP_ADDR + 0x35a8),
		ISP_RD32(ISP_ADDR + 0x35ac), ISP_RD32(ISP_ADDR + 0x35b0),
		ISP_RD32(ISP_ADDR + 0x35b4), ISP_RD32(ISP_ADDR + 0x35b8));
		g_bDmaERR_p2 = MFALSE;
	}

	if (g_bDmaERR_deepDump) {
		ISP_WR32((ISP_ADDR + 0x160), 0x0);
		ISP_WR32((ISP_ADDR + 0x35f4), 0x1E);
		LOG_ERR("imgi_debug_0 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x11E);
		LOG_ERR("imgi_debug_1 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x21E);
		LOG_ERR("imgi_debug_2 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x31E);
		LOG_ERR("imgi_debug_3 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		/* vipi */
		ISP_WR32((ISP_ADDR + 0x35f4), 0x41E);
		LOG_ERR("vipi_debug_0 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x51E);
		LOG_ERR("vipi_debug_1 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x61E);
		LOG_ERR("vipi_debug_2 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x71E);
		LOG_ERR("vipi_debug_3 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		/* imgo */
		ISP_WR32((ISP_ADDR + 0x35f4), 0x81E);
		LOG_ERR("imgo_debug_0 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x91E);
		LOG_ERR("imgo_debug_1 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0xa1E);
		LOG_ERR("imgo_debug_2 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0xb1E);
		LOG_ERR("imgo_debug_3 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		/* imgo_d */
		ISP_WR32((ISP_ADDR + 0x35f4), 0xc1E);
		LOG_ERR("imgo_d_debug_0 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0xd1E);
		LOG_ERR("imgo_d_debug_1 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0xe1E);
		LOG_ERR("imgo_d_debug_2 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0xf1E);
		LOG_ERR("imgo_d_debug_3 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		/* rrzo */
		ISP_WR32((ISP_ADDR + 0x35f4), 0x101E);
		LOG_ERR("rrzo_debug_0 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x111E);
		LOG_ERR("rrzo_debug_1 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x121E);
		LOG_ERR("rrzo_debug_2 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x131E);
		LOG_ERR("rrzo_debug_3 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		/* rrzo_d */
		ISP_WR32((ISP_ADDR + 0x35f4), 0x151E);
		LOG_ERR("rrzo_d_debug_0 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x161E);
		LOG_ERR("rrzo_d_debug_1 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x171E);
		LOG_ERR("rrzo_d_debug_2 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x181E);
		LOG_ERR("rrzo_d_debug_3 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		/* img3o */
		ISP_WR32((ISP_ADDR + 0x35f4), 0x181E);
		LOG_ERR("img3o_debug_0 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x191E);
		LOG_ERR("img3o_debug_1 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x1a1E);
		LOG_ERR("img3o_debug_2 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x1b1E);
		LOG_ERR("img3o_debug_3 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		/* img2o */
		ISP_WR32((ISP_ADDR + 0x35f4), 0x1c1E);
		LOG_ERR("img3o_debug_0 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x1d1E);
		LOG_ERR("img3o_debug_1 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x1e1E);
		LOG_ERR("img3o_debug_2 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x1f1E);
		LOG_ERR("img3o_debug_3 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		g_bDmaERR_deepDump = MFALSE;
	}
#endif

	return 0;
}

#define RegDump(start, end) {\
	unsigned int i;\
	for (i = start; i <= end; i += 0x10) {\
		LOG_INF(\
		"[0x%08X %08X],[0x%08X %08X],[0x%08X %08X],[0x%08X %08X]",\
		(unsigned int)(ISP_TPIPE_ADDR + i),\
		(unsigned int)ISP_RD32(ISP_ADDR + i),\
		(unsigned int)(ISP_TPIPE_ADDR + i+0x4),\
		(unsigned int)ISP_RD32(ISP_ADDR + i+0x4),\
		(unsigned int)(ISP_TPIPE_ADDR + i+0x8),\
		(unsigned int)ISP_RD32(ISP_ADDR + i+0x8),\
		(unsigned int)(ISP_TPIPE_ADDR + i+0xc),\
		(unsigned int)ISP_RD32(ISP_ADDR + i+0xc));\
	} \
}

bool ISP_chkModuleSetting(void)
{
	/*check the setting; */
	unsigned int cam_ctrl_en_p1;  /*4004 */
	unsigned int cam_ctrl_mux; /*4074 */
	unsigned int cam_ctrl_sel; /*4018 */
	unsigned int cam_tg1_vf_con;  /*4414 */

	unsigned int grab_width;
	unsigned int grab_height;

	unsigned int cam_tg1_sen_grab_pxl; /*4418 */
	unsigned int cam_tg1_sen_grab_lin; /*441C */

	unsigned int sgg_sel;
	unsigned int eis_sel;
	bool flk_en;
	unsigned int i;

	cam_ctrl_en_p1 = ISP_RD32(ISP_ADDR + 0x4);
	flk_en = (cam_ctrl_en_p1 >> 17) & 0x01;
	cam_ctrl_mux = ISP_RD32(ISP_ADDR + 0x74);
	sgg_sel = (cam_ctrl_mux >> 6) & 0x03;
	cam_ctrl_sel = ISP_RD32(ISP_ADDR + 0x18);
	eis_sel = (cam_ctrl_sel >> 15) & 0x01;

	cam_tg1_sen_grab_pxl = ISP_RD32(ISP_ADDR + 0x418);
	cam_tg1_sen_grab_lin = ISP_RD32(ISP_ADDR + 0x41C);

	cam_tg1_vf_con = ISP_RD32(ISP_ADDR + 0x414);

	if (cam_tg1_vf_con & 0x01) {
		/*Check FLK setting */
		unsigned int cam_flk_con;  /*4770 */
		unsigned int cam_flk_ofst; /*4774 */
		unsigned int cam_flk_size; /*4778 */
		unsigned int cam_flk_num;  /*477C */
		//TBD can't find
		//unsigned int cam_esfko_xsize; /*7370 */

		unsigned int FLK_OFST_X;
		unsigned int FLK_OFST_Y;
		unsigned int FLK_SIZE_X;
		unsigned int FLK_SIZE_Y;
		unsigned int FLK_NUM_X;
		unsigned int FLK_NUM_Y;

		//unsigned int esfko_xsize;

		//unsigned int cam_aao_xsize;   /*7390 */ //TBD can't find
		//unsigned int cam_aao_ysize;   /*7394 */ //TBD can't find
		unsigned int cam_awb_win_num; /*45BC */
		unsigned int cam_ae_hst_ctl;  /*4650 */
		//unsigned int cam_ae_stat_en;  /*4698 */ //TBD can't find

		unsigned int AWB_W_HNUM;
		unsigned int AWB_W_VNUM;
		unsigned int histogramen_num;

		unsigned int cam_awb_win_org; /*45B0 */
		unsigned int cam_awb_win_siz; /*45B4 */
		unsigned int cam_awb_win_pit; /*45B8 */

		unsigned int AAO_InWidth;
		unsigned int AAO_InHeight;
		unsigned int AWB_W_HPIT;
		unsigned int AWB_W_VPIT;
		unsigned int AWB_W_HSIZ;
		unsigned int AWB_W_VSIZ;
		unsigned int AWB_W_HORG;
		unsigned int AWB_W_VORG;

		unsigned int tmp, rst;
		unsigned int h_size;
		unsigned int v_size;
		// unsigned int afo_d_xsize, afo_d_ysize;
		unsigned int TG_W;
		unsigned int TG_H;
		unsigned int AF_EN;
		unsigned int SGG_EN,EIS_EN;
		unsigned int cam_tg_sen_mode, dbl_data_bus;
		unsigned int tg_w_pxl_e, tg_w_pxl_s;
		unsigned int tg_h_lin_e, tg_h_lin_s;
		unsigned int cam_af_size, af_xsize, af_ysize;

		unsigned int scenario;

		unsigned int cam_ctl_scenario;  /*4010 */

		unsigned int EIS_RP_VOFST;
		unsigned int EIS_RP_HOFST;
		unsigned int EIS_WIN_VSIZE;
		unsigned int EIS_WIN_HSIZE;

		unsigned int EIS_OP_HORI;
		unsigned int EIS_OP_VERT;

		unsigned int EIS_SUBG_EN;
		unsigned int EIS_NUM_HRP;
		unsigned int EIS_NUM_VRP;
		unsigned int EIS_NUM_HWIN;
		unsigned int EIS_NUM_VWIN;

		unsigned int EIS_IMG_WIDTH;
		unsigned int EIS_IMG_HEIGHT;

		bool bError;

		unsigned int CAM_EIS_PREP_ME_CTRL1; /*4DC0 */
		unsigned int CAM_EIS_MB_OFFSET;     /*4DD0 */
		unsigned int CAM_EIS_MB_INTERVAL;   /*4DD4 */
		unsigned int CAM_EIS_IMAGE_CTRL;    /*4DE0 */

		LOG_INF("ISP chk TG1");

		grab_width = ((cam_tg1_sen_grab_pxl >> 16) & 0x7fff) -
			     (cam_tg1_sen_grab_pxl & 0x7fff);
		grab_height = ((cam_tg1_sen_grab_lin >> 16) & 0x1fff) -
			      (cam_tg1_sen_grab_lin & 0x1fff);

		//cam_esfko_xsize = ISP_RD32(ISP_ADDR + 0x3370);
		//esfko_xsize = cam_esfko_xsize & 0xffff;

		cam_flk_con = ISP_RD32(ISP_ADDR + 0x770);
		cam_flk_ofst = ISP_RD32(ISP_ADDR + 0x774);
		cam_flk_size = ISP_RD32(ISP_ADDR + 0x778);
		cam_flk_num = ISP_RD32(ISP_ADDR + 0x77C);
		FLK_OFST_X = cam_flk_ofst & 0xFFF;
		FLK_OFST_Y = (cam_flk_ofst >> 16) & 0xFFF;
		FLK_SIZE_X = cam_flk_size & 0xFFF;
		FLK_SIZE_Y = (cam_flk_size >> 16) & 0xFFF;
		FLK_NUM_X = cam_flk_num & 0x7;
		FLK_NUM_Y = (cam_flk_num >> 4) & 0x7;
		//if ((flk_en == 1) && (sgg_3en == 0)) //TBD, need block diagram
		//	pr_info("HwRWCtrl:: Flicker Error: SGG3_EN should be 1 when FLK_EN = 1");
		/*1. The window size must be multiples of 2 */
		if ((FLK_SIZE_X % 2 != 0) || (FLK_SIZE_Y % 2 != 0) ||
		    (FLK_SIZE_X == 0) || (FLK_SIZE_Y == 0)) {
			/* Error */
			pr_info("HwRWCtrl:: Flicker Error: The window size must be multiples of 2. horizontally and vertically!!");
			pr_info("HwRWCtrl:: Flicker Error: CAM_FLK_SIZE.FLK_SIZE_X(%d) and CAM_FLK_SIZE.FLK_SIZE_Y(%d) value can't be 0!!",
				FLK_SIZE_X, FLK_SIZE_Y);
		}

		/*Check AF setting */

		// under twin case, sgg_sel won't be 0 , so , don't need to take
		// into consideration at twin case
		tmp = 0;
		rst = MTRUE;
		cam_tg_sen_mode = ISP_RD32(ISP_ADDR + 0x410);
		TG_W = ISP_RD32(ISP_ADDR + 0x418);
		TG_H = ISP_RD32(ISP_ADDR + 0x41C);
		cam_af_size = ISP_RD32(ISP_ADDR + 0x6CC);

		AF_EN = (cam_ctrl_en_p1 >> 16) & 0x1;
		SGG_EN = (cam_ctrl_en_p1 >> 15) & 0x1;
		EIS_EN = (cam_ctrl_en_p1 >> 21) & 0x1;

		//
		tg_w_pxl_e = (TG_W >> 16) & 0x7fff;
		tg_w_pxl_s = TG_W & 0x7fff;
		tg_h_lin_e = (TG_H >> 16) & 0x7fff;
		tg_h_lin_s = TG_H & 0x7fff;
		if (tg_w_pxl_e - tg_w_pxl_s < 32) {
			LOG_INF("tg width < 32, can't enable AF:0x%x\n",
				(tg_w_pxl_e - tg_w_pxl_s));
			rst = MFALSE;
		}

		// AFO and AF relaterd module enable check
		if (SGG_EN == 0) {
			pr_info("AF is enabled, MUST enable SGG1:0x%x\n",
				SGG_EN);
			rst = MFALSE;
		}

		//
		dbl_data_bus = (cam_tg_sen_mode >> 1) & 0x1;
		// AF image wd
		switch (sgg_sel) {
		case 0:
			h_size = tg_w_pxl_e - tg_w_pxl_s;
			v_size = tg_h_lin_e - tg_h_lin_s;
			break;
		case 1:
			h_size = tg_w_pxl_e - tg_w_pxl_s + 1;
			v_size = tg_h_lin_e - tg_h_lin_s;
			break;
		case 2:
			h_size = tg_w_pxl_e - tg_w_pxl_s + 1;
			v_size = tg_h_lin_e - tg_h_lin_s;
			break;
		default:
			LOG_INF("unsupported sgg_sel:0x%x\n", sgg_sel);
			return MFALSE;
		}
		af_xsize = cam_af_size & 0x3ff;
		af_ysize = (cam_af_size >> 16) & 0x3ff;
		if (af_xsize < 8 || af_xsize > 510 || af_ysize < 4 || af_ysize > 511) {
			LOG_INF("af_xsize/af_ysize out of range:0x%x/0x%x\n",
				af_xsize, af_ysize);
			rst = MFALSE;
		}
/*Check AE setting */
		{

			cam_awb_win_num = ISP_RD32(ISP_ADDR + 0x5BC);
			cam_ae_hst_ctl = ISP_RD32(ISP_ADDR + 0x650);

			AWB_W_HNUM = cam_awb_win_num & 0xff;
			AWB_W_VNUM = (cam_awb_win_num >> 16) & 0xff;
			histogramen_num = 0;
			for (i = 0; i < 4; i++) {
				if ((cam_ae_hst_ctl >> i) & 0x1)
					histogramen_num += 1;

			}
		}

		/*Check AWB setting */

		cam_awb_win_num = ISP_RD32(ISP_ADDR + 0x5BC);
		cam_awb_win_siz = ISP_RD32(ISP_ADDR + 0x5B4);
		cam_awb_win_pit = ISP_RD32(ISP_ADDR + 0x5B8);
		cam_awb_win_org = ISP_RD32(ISP_ADDR + 0x5B0);

		AAO_InWidth = grab_width;
		AAO_InHeight = grab_height;

		AWB_W_HNUM = (cam_awb_win_num & 0xff);
		AWB_W_VNUM = ((cam_awb_win_num >> 16) & 0xff);

		AWB_W_HSIZ = (cam_awb_win_siz & 0x1fff);
		AWB_W_VSIZ = ((cam_awb_win_siz >> 16) & 0x1fff);

		AWB_W_HPIT = (cam_awb_win_pit & 0x1fff);
		AWB_W_VPIT = ((cam_awb_win_pit >> 16) & 0x1fff);

		AWB_W_HORG = (cam_awb_win_org & 0x1fff);
		AWB_W_VORG = ((cam_awb_win_org >> 16) & 0x1fff);
		if (AAO_InWidth < (AWB_W_HNUM * AWB_W_HPIT + AWB_W_HORG)) {
			/*Error */
			pr_info("Error HwRWCtrl:: grab_width(%d), grab_height(%d)!!",
				grab_width,
				grab_height);
			pr_info("Error HwRWCtrl:: input frame width(%d) >= AWB_W_HNUM(%d)	* AWB_W_HPIT(%d) + AWB_W_HORG(%d) !!",
				AAO_InWidth, AWB_W_HNUM, AWB_W_HPIT,
				AWB_W_HORG);
		}
		if (AAO_InHeight < (AWB_W_VNUM * AWB_W_VPIT + AWB_W_VORG)) {
			/*Error */
			pr_info("Error HwRWCtrl:: grab_width(%d), grab_height(%d)!!",
				grab_width,
				grab_height);
			pr_info("Error HwRWCtrl:: input frame height(%d) >= AWB_W_VNUM(%d) * AWB_W_VPIT(%d) + AWB_W_VORG(%d) !!",
				AAO_InHeight, AWB_W_VNUM, AWB_W_VPIT,
				AWB_W_VORG);
		}
		if (AWB_W_HPIT < AWB_W_HSIZ || AWB_W_VPIT < AWB_W_VSIZ) {
			/*Error */
			pr_info("Error HwRWCtrl:: AWB_W_HPIT(%d) >= AWB_W_HSIZ(%d), AWB_W_VPIT(%d) >= AWB_W_VSIZ(%d) !!",
				AWB_W_HPIT, AWB_W_HSIZ, AWB_W_VPIT, AWB_W_VSIZ);
		}


		bError = MFALSE;

		cam_ctl_scenario = ISP_RD32(ISP_ADDR + 0x10);
		scenario = (cam_ctl_scenario >> 4) & 0x7;
		if (EIS_EN) {
		CAM_EIS_PREP_ME_CTRL1 = ISP_RD32(ISP_ADDR + 0xDC0);
		CAM_EIS_MB_OFFSET = ISP_RD32(ISP_ADDR + 0xDD0);
		CAM_EIS_MB_INTERVAL = ISP_RD32(ISP_ADDR + 0xDD4);
		CAM_EIS_IMAGE_CTRL = ISP_RD32(ISP_ADDR + 0xDE0);

		EIS_SUBG_EN = (CAM_EIS_PREP_ME_CTRL1 >> 6) & 0x1;
		EIS_RP_VOFST = (CAM_EIS_MB_OFFSET)&0xfff;
		EIS_RP_HOFST = (CAM_EIS_MB_OFFSET >> 16) & 0xfff;
		EIS_WIN_VSIZE = (CAM_EIS_MB_INTERVAL)&0xfff;
		EIS_WIN_HSIZE = (CAM_EIS_MB_INTERVAL >> 16) & 0xfff;

		EIS_OP_HORI = CAM_EIS_PREP_ME_CTRL1 & 0x7;
		EIS_OP_VERT = (CAM_EIS_PREP_ME_CTRL1 >> 3) & 0x7;

		EIS_NUM_HRP = (CAM_EIS_PREP_ME_CTRL1 >> 8) & 0x1f;
		EIS_NUM_VRP = (CAM_EIS_PREP_ME_CTRL1 >> 21) & 0xf;
		EIS_NUM_HWIN = (CAM_EIS_PREP_ME_CTRL1 >> 25) & 0x7;
		EIS_NUM_VWIN = (CAM_EIS_PREP_ME_CTRL1 >> 28) & 0xf;

		EIS_IMG_WIDTH = (CAM_EIS_IMAGE_CTRL >> 16) & 0x1fff;
		EIS_IMG_HEIGHT = CAM_EIS_IMAGE_CTRL & 0x1fff;


		/*1. The max horizontal window size is 4 */
		/*2. The max vertical window size is 8 */
		/*3. EIS_MF_OFFSET.EIS_RP_VOFST/EIS_RP_HOFST  > 16 */
		/*6. EIS_PREP_ME_CTRL1.EIS_NUM_HRP <= 16 */
		/*7. EIS_PREP_ME_CTRL1.EIS_NUM_VRP <= 8 */
		if ((EIS_NUM_VWIN > 8) || (EIS_NUM_HWIN > 4) ||
		    (EIS_NUM_VRP > 8) || (EIS_NUM_HRP > 16) ||
		    (EIS_RP_VOFST < 16) || (EIS_RP_HOFST <= 16)) {
			/*Error */
			pr_info("EIS Error, 1. The max horizontal window size is 4, EIS_NUM_HWIN(%d)!!",
				EIS_NUM_HWIN);
			pr_info("EIS Error, 2. The max vertical window size is 8, EIS_NUM_VWIN(%d)!!",
				EIS_NUM_VWIN);
			pr_info("EIS Error, 3. EIS_MF_OFFSET.EIS_RP_VOFST or EIS_RP_HOFST  > 16!!, EIS_RP_VOFST(%d), EIS_RP_HOFST(%d)",
				EIS_RP_VOFST, EIS_RP_HOFST);
			pr_info("EIS Error, 6. EIS_PREP_ME_CTRL1.EIS_NUM_HRP <= 16, EIS_NUM_HRP(%d)!!",
				EIS_NUM_HRP);
			pr_info("EIS Error, 7. EIS_PREP_ME_CTRL1.EIS_NUM_VRP <= 8, EIS_NUM_VRP(%d)!!",
				EIS_NUM_VRP);
		}
		/* It's special changing HW constraint limitation for EIS
		 *8. EIS_MB_INTERVAL.EIS_WIN_HSIZE >=
		 * (EIS_PREP_ME_CTRL1.EIS_NUM_HRP+1)*16+1
		 *9. EIS_MB_INTERVAL.EIS_WIN_VSIZE >=
		 * (EIS_PREP_ME_CTRL1.EIS_NUM_VRP+1)*16+1
		 */
		if ((EIS_WIN_HSIZE < (((EIS_NUM_HRP + 1) << 4) + 1)) ||
		    (EIS_WIN_VSIZE < (((EIS_NUM_VRP + 1) << 4) + 1))) {
			/*Error */
			pr_info("EIS Error, 8. EIS_MB_INTERVAL.EIS_WIN_HSIZE >= (EIS_PREP_ME_CTRL1.EIS_NUM_HRP+1)*16+1!!, EIS_WIN_HSIZE:%d, EIS_NUM_HRP:%d",
				EIS_WIN_HSIZE, EIS_NUM_HRP);
			pr_info("EIS Error, 9. EIS_MB_INTERVAL.EIS_WIN_VSIZE >=	(EIS_PREP_ME_CTRL1.EIS_NUM_VRP+1)*16+1!!, EIS_WIN_VSIZE:%d, EIS_NUM_VRP:%d",
				EIS_WIN_VSIZE, EIS_NUM_VRP);
		}
/*10. (EIS_MB_OFFSET.EIS_RP_HOFST +
 *  ((EIS_MB_INTERVAL.EIS_WIN_HSIZE-1)*
 *	EIS_PREP_ME_CTRL1.EIS_NUM_HWIN)+EIS_PREP_ME_CTRL1.EIS_NUM_HRP*16)*
 *	EIS_PREP_ME_CTRL1.EIS_OP_HORI < EIS_IMAGE_CTRL.WIDTH
 *10.( EIS_MB_OFFSET.EIS_RP_VOFST +
 *  ((EIS_MB_INTERVAL.EIS_WIN_VSIZE-1)*
 *	EIS_PREP_ME_CTRL1.EIS_NUM_VWIN)+EIS_PREP_ME_CTRL1.EIS_NUM_VRP*16)*
 *	EIS_PREP_ME_CTRL1.EIS_OP_VERT < EIS_IMAGE_CTRL.HEIGHT
 */
		if ((((EIS_RP_HOFST +
		       ((EIS_WIN_HSIZE - 1) * (EIS_NUM_HWIN - 1)) +
		       (EIS_NUM_HRP << 4)) *
		      EIS_OP_HORI) >= EIS_IMG_WIDTH) ||
		    (((EIS_RP_VOFST +
		       ((EIS_WIN_VSIZE - 1) * (EIS_NUM_VWIN - 1)) +
		       (EIS_NUM_VRP << 4)) *
		      EIS_OP_VERT) >= EIS_IMG_HEIGHT)) {
			/*Error */
			pr_info("EIS Error, 10. (EIS_MB_OFFSET.EIS_RP_HOFST(%d) + ((EIS_MB_INTERVAL.EIS_WIN_HSIZE(%d)-1)*(EIS_PREP_ME_CTRL1.EIS_NUM_HWIN(%d)-1))+EIS_PREP_ME_CTRL1.EIS_NUM_HRP(%d)*16)*EIS_PREP_ME_CTRL1.EIS_OP_HORI(%d) < EIS_IMAGE_CTRL.WIDTH(%d)!!",
				EIS_RP_HOFST, EIS_WIN_HSIZE, EIS_NUM_HWIN,
				EIS_NUM_HRP, EIS_OP_HORI, EIS_IMG_WIDTH);
			pr_info("EIS Error, 10. (EIS_MB_OFFSET.EIS_RP_VOFST(%d) + ((EIS_MB_INTERVAL.EIS_WIN_VSIZE(%d)-1)*(EIS_PREP_ME_CTRL1.EIS_NUM_VWIN(%d)-1))+EIS_PREP_ME_CTRL1.EIS_NUM_VRP(%d)*16)*EIS_PREP_ME_CTRL1.EIS_OP_VERT(%d) < EIS_IMG_HEIGHT.WIDTH(%d)!!",
				EIS_RP_VOFST, EIS_WIN_VSIZE, EIS_NUM_VWIN,
				EIS_NUM_VRP, EIS_OP_VERT, EIS_IMG_HEIGHT);
		}

		/*11. EISO_XISZE = 255 (after 82, EISO_XISZE = 407 in 89)
		 *4. EIS_IMAGE_CTRL.WIDTH = EIS input image width but if
		 *   (two_pix mode) EIS_IMAGE_CTRL.WIDTH = input image width/2
		 *5. EIS_IMAGE_CTRL.HEIGHT = EIS input image height
		 */
#if 0
		switch (eis_sel) {
		case 0:
			if ((scenario == 1) && (sgg_sel == 0x0)) {
				if ((grab_width != EIS_IMG_WIDTH) ||
				    (grab_height != EIS_IMG_HEIGHT)) {
					bError = MTRUE;
				}
				/*Error */
			} else {
				/*Error in non-yuv sensor */
				LOG_INF("EIS Error, Non-Yuv Sensor!!");
                bError = MTRUE;
			}
			break;
		case 1:
			if ((TG_W !=
			     EIS_IMG_WIDTH) ||
			    (grab_height != EIS_IMG_HEIGHT)) {
				bError = MTRUE;
			}
			/*Error */
			break;
		default:
			/*Error */
			break;
		}
		if (bError == MTRUE) {
			pr_info("EIS Error, 4. EIS_IMAGE_CTRL.WIDTH = EIS input image width but if (two_pix mode) EIS_IMAGE_CTRL.WIDTH = input image width/2!!\n");
			pr_info("EIS Error, 5. EIS_IMAGE_CTRL.HEIGHT != EIS input image height!!\n");
			pr_info("eis_sel:%d, scenario:%d, sgg_sel:%d",
				eis_sel, scenario, sgg_sel);
			pr_info("EIS_IMG_WIDTH:%d, EIS_IMG_HEIGHT:%d, grab_width:%d, grab_height:%d, TG_W:%d",
				EIS_IMG_WIDTH, EIS_IMG_HEIGHT, grab_width,
				grab_height, TG_W);
		}
#endif
		}
	}

	return MTRUE;
}

/*************************************************
 *
 *************************************************/
static signed int ISP_DumpReg_FrmB(void)
{
	ISP_chkModuleSetting();
	return ISP_DumpReg();
}

/*************************************************
 *
 *************************************************/
/* js_test */
/*  */
#ifndef _rtbc_use_cq0c_
static unsigned int bEnqBuf;
static unsigned int bDeqBuf;
static signed int rtbc_enq_dma = _rt_dma_max_;
static signed int rtbc_deq_dma = _rt_dma_max_;
#endif

static unsigned int prv_tstamp_s[_rt_dma_max_] = { 0 };
static unsigned int prv_tstamp_us[_rt_dma_max_] = { 0 };

static unsigned int sof_count[_ChannelMax] = { 0, 0, 0, 0 };
static unsigned int start_time[_ChannelMax] = { 0, 0, 0, 0 };
static unsigned int avg_frame_time[_ChannelMax] = { 0, 0, 0, 0 };

/* record lost p1_done or not, 1 for lost p1_done.
 * 0 for normal , 2 for last working buffer. 0xF for hw error at previous frame
 */
static int sof_pass1done[2] = { 0, 0 };
// this flag is to record int_err happened or not.
static bool bClrErrRecord = MFALSE;


// static unsigned int gSof_camsvdone[2] = { 0, 0 };
static bool g1stSof[4] = { MTRUE, MTRUE };

#ifdef _rtbc_buf_que_2_0_
struct FW_RCNT_CTRL {
	unsigned int INC[_IRQ_MAX][ISP_RT_BUF_SIZE];	/* rcnt_in */
	unsigned int DMA_IDX[_rt_dma_max_];	/* enque cnt */
	unsigned int rdIdx[_IRQ_MAX];	/* enque read cnt */
	unsigned int curIdx[_IRQ_MAX];	/* record avail rcnt pair */
	unsigned int bLoadBaseAddr[_IRQ_MAX];
};
static struct FW_RCNT_CTRL mFwRcnt = { {{0} }, {0}, {0}, {0}, {0} };
static unsigned char dma_en_recorder[_rt_dma_max_][ISP_RT_BUF_SIZE] = { {0} };
#endif
/*  */
static signed int ISP_RTBC_ENQUE_FRMB(signed int dma,
struct ISP_RT_BUF_INFO_STRUCT_FRMB *prt_buf_info)
{
	signed int Ret = 0;
	signed int rt_dma = dma;
	unsigned int buffer_exist = 0;
	unsigned int i = 0;
	unsigned int index = 0;

	if ((rt_dma < 0) || (rt_dma >= _rt_dma_max_)) {
		LOG_DBG("Error: rt_dma(%d) Index is Invalid",rt_dma);
		Ret = -EFAULT;
		return Ret;
	}
	/* check max */
	if (pstRTBuf_FrmB->ring_buf[rt_dma].total_count == ISP_RT_BUF_SIZE) {
		LOG_ERR(
		"[rtbc][ENQUE]:real time buffer number FULL:rt_dma(%d)",
		rt_dma);
		Ret = -EFAULT;
		/* break; */
	}
	/*  */
	/* spin_lock_irqsave(&(IspInfo_FrmB.SpinLockRTBC),g_Flash_SpinLock); */
	/* check if buffer exist */
	for (i = 0; i < ISP_RT_BUF_SIZE; i++) {
		if (pstRTBuf_FrmB->ring_buf[rt_dma].data[i].
			base_pAddr == prt_buf_info->base_pAddr) {
			buffer_exist = 1;
			break;
		}
		/*  */
		if (pstRTBuf_FrmB->ring_buf[rt_dma].data[i].base_pAddr == 0)
			break;
	}
	/*  */
	if (buffer_exist) {
		/*  */
		if (pstRTBuf_FrmB->ring_buf[rt_dma].data[i].
			bFilled != ISP_RTBC_BUF_EMPTY) {
			pstRTBuf_FrmB->ring_buf[rt_dma].data[i].
			bFilled = ISP_RTBC_BUF_EMPTY;
			pstRTBuf_FrmB->ring_buf[rt_dma].empty_count++;
			index = i;
		}
		/*  */
		if (IspInfo_FrmB.DebugMask & ISP_DBG_BUF_CTRL) {
			LOG_DBG(
			"[rtbc][ENQUE]::buffer_exist(%d)/i(%d)/PA(0x%x)/bFilled(%d)/empty(%d)",
			buffer_exist, i, prt_buf_info->base_pAddr,
			pstRTBuf_FrmB->ring_buf[rt_dma].data[i].bFilled,
			pstRTBuf_FrmB->ring_buf[rt_dma].empty_count);
		}

	} else {
		/* overwrite oldest element if buffer is full */
		if (pstRTBuf_FrmB->ring_buf[rt_dma].
			total_count == ISP_RT_BUF_SIZE) {
			LOG_ERR("[ENQUE]:[rtbc]:buffer full(%d)",
				pstRTBuf_FrmB->ring_buf[rt_dma].total_count);
		} else {
			/* first time add */
			index = pstRTBuf_FrmB->ring_buf[rt_dma].
			total_count % ISP_RT_BUF_SIZE;
			/*  */
			pstRTBuf_FrmB->ring_buf[rt_dma].data[index].
			memID = prt_buf_info->memID;
			pstRTBuf_FrmB->ring_buf[rt_dma].data[index].
			size = prt_buf_info->size;
			pstRTBuf_FrmB->ring_buf[rt_dma].data[index].
			base_vAddr =
			    prt_buf_info->base_vAddr;
			pstRTBuf_FrmB->ring_buf[rt_dma].data[index].
			base_pAddr =
			    prt_buf_info->base_pAddr;
			pstRTBuf_FrmB->ring_buf[rt_dma].data[index].
			bFilled = ISP_RTBC_BUF_EMPTY;
			pstRTBuf_FrmB->ring_buf[rt_dma].data[index].
			bufIdx = prt_buf_info->bufIdx;
			/*  */
			pstRTBuf_FrmB->ring_buf[rt_dma].total_count++;
			pstRTBuf_FrmB->ring_buf[rt_dma].empty_count++;
			/*  */
			if (IspInfo_FrmB.DebugMask & ISP_DBG_BUF_CTRL) {
				LOG_DBG(
				"[rtbc][ENQUE]:dma(%d),index(%d),bufIdx(0x%x),PA(0x%x)/empty(%d)/total(%d)",
				rt_dma, index, prt_buf_info->bufIdx,
				pstRTBuf_FrmB->ring_buf[rt_dma].
				data[index].base_pAddr,
				pstRTBuf_FrmB->ring_buf[rt_dma].empty_count,
				pstRTBuf_FrmB->ring_buf[rt_dma].total_count);
			}
		}
	}
	/*  */

	/* count ==1 means DMA stalled already or NOT start yet */
	if (pstRTBuf_FrmB->ring_buf[rt_dma].empty_count == 1) {
		if (_imgo_ == rt_dma) {
			/* set base_addr at beginning before VF_EN */
			ISP_WR32(ISP_REG_ADDR_IMGO_BASE_ADDR,
				 pstRTBuf_FrmB->ring_buf[rt_dma].
				 data[index].base_pAddr);
		} else if (_img2o_ == rt_dma) {
			/* set base_addr at beginning before VF_EN */
			ISP_WR32(ISP_REG_ADDR_IMG2O_BASE_ADDR,
				 pstRTBuf_FrmB->ring_buf[rt_dma].
				 data[index].base_pAddr);
		}

		if (IspInfo_FrmB.DebugMask & ISP_DBG_BUF_CTRL) {
			LOG_DBG(
			"[rtbc][ENQUE]:dma(%d),base_pAddr(0x%x)/imgo(0x%x)/img2o(0x%x)empty_count(%d) ",
			rt_dma,
			pstRTBuf_FrmB->ring_buf[rt_dma].data[index].base_pAddr,
			ISP_RD32(ISP_REG_ADDR_IMGO_BASE_ADDR),
			ISP_RD32(ISP_REG_ADDR_IMG2O_BASE_ADDR),
			pstRTBuf_FrmB->ring_buf[rt_dma].empty_count);
		}
#if defined(_rtbc_use_cq0c_)
		/* Do nothing */
#else
		unsigned int reg_val = 0;

		/* disable FBC control to go on download */
		if (_imgo_ == rt_dma) {
			reg_val = ISP_RD32(ISP_REG_ADDR_IMGO_FBC);
			reg_val &= ~0x4000;
			ISP_WR32(ISP_REG_ADDR_IMGO_FBC, reg_val);
		} else {
			reg_val = ISP_RD32(ISP_REG_ADDR_IMGO_D_FBC);
			reg_val &= ~0x4000;
			ISP_WR32(ISP_REG_ADDR_IMGO_D_FBC, reg_val);
		}
		if (IspInfo_FrmB.DebugMask & ISP_DBG_BUF_CTRL) {
			LOG_DBG(
			"[rtbc][ENQUE]:dma(%d),disable fbc:IMGO(0x%x),IMG2O(0x%x)",
			rt_dma,
			ISP_RD32(ISP_REG_ADDR_IMGO_FBC),
			ISP_RD32(ISP_REG_ADDR_IMGO_D_FBC));
		}
#endif
		pstRTBuf_FrmB->ring_buf[rt_dma].pre_empty_count =
		    pstRTBuf_FrmB->ring_buf[rt_dma].empty_count;

	}
/*  */
/* spin_unlock_irqrestore(&(IspInfo_FrmB.
 * SpinLockRTBC),g_Flash_SpinLock);
 */
/*  */
	if (IspInfo_FrmB.DebugMask & ISP_DBG_BUF_CTRL) {
		LOG_DBG(
		"[rtbc][ENQUE]:dma:(%d),start(%d),index(%d),empty_count(%d),base_pAddr(0x%x)",
		rt_dma, pstRTBuf_FrmB->ring_buf[rt_dma].start, index,
		pstRTBuf_FrmB->ring_buf[rt_dma].empty_count,
		pstRTBuf_FrmB->ring_buf[rt_dma].data[index].base_pAddr);
	}
	/*  */
	return Ret;
}

static void ISP_FBC_DUMP(unsigned int dma_id, unsigned int VF_1,
unsigned int VF_2, unsigned int VF_3, unsigned int VF_4)
{
	unsigned int z;
	char str[128];
	char str2[_rt_dma_max_];
	unsigned int dma;
	signed int nleft = 0, nsize = 128, len = 0;

	LOG_INF("================================\n");
	LOG_INF("pass1 timeout log(timeout port:%d)", dma_id);
	LOG_INF("================================\n");
	str[0] = '\0';
	nleft = nsize - 1; /* minus1 for null terminate */
	LOG_INF("current activated dmaport");
	for (z = 0; z < _rt_dma_max_; z++) {
		len = sprintf(str2, "%d_", pstRTBuf_FrmB->ring_buf[z].active);
		strncat(str, str2, ((nleft < len) ? nleft : len));
		nleft -= ((nleft < len) ? nleft : len);
	}
	LOG_INF("%s", str);
	LOG_INF("================================\n");
	if (VF_1) {
		LOG_INF("imgo:");
		dma = _imgo_;
		{
			str[0] = '\0';
			nleft = nsize - 1;
			LOG_INF("current fillled buffer(buf cnt): %d\n",
				pstRTBuf_FrmB->ring_buf[dma].total_count);
			for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
				len = sprintf(str2, "%d_",
				pstRTBuf_FrmB->ring_buf[dma].data[z].bFilled);
				strncat(str, str2, ((nleft < len) ?
				nleft : len));
				nleft -= ((nleft < len) ? nleft : len);
			}
			LOG_INF("%s", str);
			LOG_INF("================================\n");
			LOG_INF("cur_start_idx:%d",
			pstRTBuf_FrmB->ring_buf[dma].start);
			LOG_INF("cur_read_idx=%d",
			pstRTBuf_FrmB->ring_buf[dma].read_idx);
			LOG_INF("cur_empty_cnt:%d",
			pstRTBuf_FrmB->ring_buf[dma].empty_count);
			LOG_INF("================================\n");
			LOG_INF("RCNT_RECORD:cur dma_en_recorder\n");
			str[0] = '\0';
			nleft = nsize - 1;
			for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
				len = sprintf(str2, "%d_",
				dma_en_recorder[dma][z]);
				strncat(str, str2, ((nleft < len) ?
				nleft : len));
				nleft -= ((nleft < len) ? nleft : len);
			}
			LOG_INF("%s", str);
			LOG_INF("================================\n");
			LOG_INF("RCNT_RECORD:inc record\n");
			str[0] = '\0';
			nleft = nsize - 1;
			for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
				len = sprintf(str2, "%d_",
				mFwRcnt.INC[_IRQ][z]);
				strncat(str, str2, ((nleft < len) ?
				nleft : len));
				nleft -= ((nleft < len) ? nleft : len);
			}
			LOG_INF("%s", str);
			LOG_INF("RCNT_RECORD: dma idx = %d\n",
			mFwRcnt.DMA_IDX[dma]);
			LOG_INF("RCNT_RECORD: read idx = %d\n",
			mFwRcnt.rdIdx[_IRQ]);
			LOG_INF("RCNT_RECORD: cur idx = %d\n",
			mFwRcnt.curIdx[_IRQ]);
			LOG_INF("RCNT_RECORD: bLoad = %d\n",
			mFwRcnt.bLoadBaseAddr[_IRQ]);
			LOG_INF("================================\n");
		}
		LOG_INF("img2o:");
		dma = _img2o_;
		{
			str[0] = '\0';
			nleft = nsize - 1;
			LOG_INF("current fillled buffer(buf cnt): %d\n",
				pstRTBuf_FrmB->ring_buf[dma].total_count);
			for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
				len = sprintf(str2, "%d_",
				pstRTBuf_FrmB->ring_buf[dma].data[z].bFilled);
				strncat(str, str2, ((nleft < len) ?
				nleft : len));
				nleft -= ((nleft < len) ? nleft : len);
			}
			LOG_INF("%s", str);
			LOG_INF("================================\n");
			LOG_INF("cur_start_idx:%d",	pstRTBuf_FrmB->ring_buf[dma].start);
			LOG_INF("cur_read_idx=%d", pstRTBuf_FrmB->ring_buf[dma].read_idx);
			LOG_INF("cur_empty_cnt:%d", pstRTBuf_FrmB->ring_buf[dma].empty_count);
			LOG_INF("================================\n");
			LOG_INF("RCNT_RECORD:cur dma_en_recorder\n");
			str[0] = '\0';
			nleft = nsize - 1;
			for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
				len = sprintf(str2, "%d_",
				dma_en_recorder[dma][z]);
				strncat(str, str2, ((nleft < len) ?
				nleft : len));
				nleft -= ((nleft < len) ? nleft : len);
			}
			LOG_INF("%s", str);
			LOG_INF("================================\n");
			LOG_INF("RCNT_RECORD:inc record\n");
			str[0] = '\0';
			nleft = nsize - 1;
			for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
				len = sprintf(str2, "%d_",
				mFwRcnt.INC[_IRQ][z]);
				strncat(str, str2, ((nleft < len) ?
				nleft : len));
				nleft -= ((nleft < len) ? nleft : len);
			}
			LOG_INF("%s", str);
			LOG_INF("RCNT_RECORD: dma idx = %d\n",
			mFwRcnt.DMA_IDX[dma]);
			LOG_INF("RCNT_RECORD: read idx = %d\n",
			mFwRcnt.rdIdx[_IRQ]);
			LOG_INF("RCNT_RECORD: cur idx = %d\n",
			mFwRcnt.curIdx[_IRQ]);
			LOG_INF("RCNT_RECORD: bLoad = %d\n",
			mFwRcnt.bLoadBaseAddr[_IRQ]);
			LOG_INF("================================\n");
		}
	}
}

static signed int ISP_RTBC_DEQUE_FRMB(signed int dma,
struct ISP_DEQUE_BUF_INFO_STRUCT_FRMB *pdeque_buf)
{
	signed int Ret = 0;
	signed int rt_dma = dma;
	unsigned int i = 0;
	unsigned int index = 0;

	if ((rt_dma < 0) || (rt_dma >= _rt_dma_max_)) {
		LOG_DBG("Error: rt_dma(%d) Index is Invalid", rt_dma);
		Ret = -EFAULT;;
		return Ret;
	}
	/* spin_lock_irqsave(&(IspInfo_FrmB.SpinLockRTBC),g_Flash_SpinLock); */
	/*  */
	if (IspInfo_FrmB.DebugMask & ISP_DBG_BUF_CTRL) {
		LOG_DBG("[rtbc][DEQUE]+");
		LOG_DBG("[rtbc][DEQUE] dma(%d),total(%d)", dma,
			pstRTBuf_FrmB->ring_buf[rt_dma].total_count);
	}
	/*  */
	pdeque_buf->count = 0;
	/* in SOF, "start" is next buffer index */
	for (i = 0; i < pstRTBuf_FrmB->ring_buf[rt_dma].total_count; i++) {
		/*  */
		index =
		    (pstRTBuf_FrmB->ring_buf[rt_dma].start +
		     i) % pstRTBuf_FrmB->ring_buf[rt_dma].total_count;
		/*  */
		if (pstRTBuf_FrmB->ring_buf[rt_dma].data[
			index].bFilled == ISP_RTBC_BUF_FILLED) {
			pstRTBuf_FrmB->ring_buf[rt_dma].data[
			index].bFilled = ISP_RTBC_BUF_LOCKED;
			pdeque_buf->count = P1_DEQUE_CNT;
			break;
		}
	}
	/*  */
	if (pdeque_buf->count == 0) {
		/* queue buffer status */
		LOG_DBG(
		"[rtbc][DEQUE]:dma(%d),start(%d),total(%d),empty(%d), pdeque_buf->count(%d) ",
		rt_dma, pstRTBuf_FrmB->ring_buf[rt_dma].start,
		pstRTBuf_FrmB->ring_buf[rt_dma].total_count,
		pstRTBuf_FrmB->ring_buf[rt_dma].empty_count,
		pdeque_buf->count);
		/*  */
		for (i = 0; i <= pstRTBuf_FrmB->ring_buf[
		rt_dma].total_count - 1; i++) {
			LOG_DBG(
			"[rtbc][DEQUE]Buf List:%d/%d/0x%x/0x%llx/0x%x/%d/  ",
			i,
			pstRTBuf_FrmB->ring_buf[rt_dma].data[i].memID,
			pstRTBuf_FrmB->ring_buf[rt_dma].data[i].size,
			pstRTBuf_FrmB->ring_buf[rt_dma].
			data[i].base_vAddr,
			pstRTBuf_FrmB->ring_buf[rt_dma].
			data[i].base_pAddr,
			pstRTBuf_FrmB->ring_buf[rt_dma].
			data[i].bFilled);
		}
	}
	/*  */
	if (pdeque_buf->count) {
		/* Fill buffer head */
		/* "start" is current working index */
		if (IspInfo_FrmB.DebugMask & ISP_DBG_BUF_CTRL) {
			LOG_DBG(
			"[rtbc][DEQUE]:rt_dma(%d)/index(%d)/empty(%d)/total(%d)",
			rt_dma,
			index,
			pstRTBuf_FrmB->ring_buf[rt_dma].empty_count,
			pstRTBuf_FrmB->ring_buf[rt_dma].total_count);
		}
		/*  */
		for (i = 0; i < pdeque_buf->count; i++) {
			pdeque_buf->data[i].memID =
			pstRTBuf_FrmB->ring_buf[rt_dma].
			data[index + i].memID;
			pdeque_buf->data[i].size =
			pstRTBuf_FrmB->ring_buf[rt_dma].
			data[index + i].size;
			pdeque_buf->data[i].base_vAddr =
			pstRTBuf_FrmB->ring_buf[rt_dma].
			data[index + i].base_vAddr;
			pdeque_buf->data[i].base_pAddr =
			pstRTBuf_FrmB->ring_buf[rt_dma].
			data[index + i].base_pAddr;
			pdeque_buf->data[i].timeStampS =
			pstRTBuf_FrmB->ring_buf[rt_dma].
			data[index + i].timeStampS;
			pdeque_buf->data[i].timeStampUs =
			pstRTBuf_FrmB->ring_buf[rt_dma].
			data[index + i].timeStampUs;
			/*  */
			if (IspInfo_FrmB.DebugMask & ISP_DBG_BUF_CTRL) {
				LOG_DBG(
				"[rtbc][DEQUE]:index(%d)/PA(0x%x)/memID(%d)/size(0x%x)/VA(0x%llx)",
				index + i,
				pdeque_buf->data[i].base_pAddr,
				pdeque_buf->data[i].memID,
				pdeque_buf->data[i].size,
				pdeque_buf->data[i].base_vAddr);
			}

		}
		/*  */
		if (IspInfo_FrmB.DebugMask &
			ISP_DBG_BUF_CTRL) {
			LOG_DBG("[rtbc][DEQUE]-");
		}
/*  */
/* spin_unlock_irqrestore(&(IspInfo_FrmB.
 * SpinLockRTBC),g_Flash_SpinLock);
 */
/*  */
	} else {
/*  */
/* spin_unlock_irqrestore(&(IspInfo_FrmB.
 * SpinLockRTBC),g_Flash_SpinLock);
 */
		LOG_ERR("[rtbc][DEQUE]:no filled buffer");
		Ret = -EFAULT;
	}

	return Ret;
}

#ifdef _MAGIC_NUM_ERR_HANDLING_
#define _INVALID_FRM_CNT_ 0xFFFF
#define _MAX_FRM_CNT_ 0xFF

#define _UNCERTAIN_MAGIC_NUM_FLAG_ 0x40000000
#define _DUMMY_MAGIC_              0x20000000
static unsigned int m_LastMNum[_rt_dma_max_] = { 0 };	/* imgo/img2o */

#endif

#ifdef _89SERIAL_
static unsigned int m_P1_RCNT_INC_CNT;
#endif

/* static long ISP_Buf_CTRL_FUNC_FRMB(unsigned int Param) */
static long ISP_Buf_CTRL_FUNC_FRMB(unsigned long Param)
{
	signed int Ret = 0;
	signed int rt_dma;
	unsigned int reg_val = 0;
	/* unsigned int reg_val2 = 0; */
	/* unsigned int camsv_reg_cal[2] = {0,0}; */
	unsigned int i = 0;
	unsigned int iBuf = 0;
	unsigned int size = 0;
	unsigned int bWaitBufRdy = 0;
	struct ISP_BUFFER_CTRL_STRUCT_FRMB rt_buf_ctrl;
	bool _bFlag = MTRUE;
	/* unsigned int buffer_exist = 0; */
	union CQ_RTBC_FBC p1_fbc[_rt_dma_max_];
	/* unsigned int p1_fbc_reg[_rt_dma_max_]; */
	unsigned long p1_fbc_reg[_rt_dma_max_];
	/* unsigned int p1_dma_addr_reg[_rt_dma_max_]; */
	unsigned long p1_dma_addr_reg[_rt_dma_max_] = {0};
	unsigned long flags;/*unsigned int flags;*/
	struct ISP_RT_BUF_INFO_STRUCT_FRMB rt_buf_info;
	struct ISP_DEQUE_BUF_INFO_STRUCT_FRMB deque_buf;
	enum eISPIrq irqT = _IRQ_MAX;
	enum eISPIrq irqT_Lock = _IRQ_MAX;
	bool CurVF_En = MFALSE;
	bool bBufFilled = MFALSE;

	memset(&rt_buf_info, 0, sizeof(struct ISP_RT_BUF_INFO_STRUCT_FRMB));
	memset(&deque_buf, 0, sizeof(struct ISP_DEQUE_BUF_INFO_STRUCT_FRMB));
	/*  */
	if (pstRTBuf_FrmB == NULL) {
		LOG_ERR("[rtbc]NULL pstRTBuf_FrmB");
		return -EFAULT;
	}
	/*  */
	if (copy_from_user(&rt_buf_ctrl, (void __user *)
		Param, sizeof(struct ISP_BUFFER_CTRL_STRUCT_FRMB))
	    == 0) {
		rt_dma = rt_buf_ctrl.buf_id;
		/*  */
		/* if(IspInfo_FrmB.DebugMask & ISP_DBG_BUF_CTRL) { */
		/* LOG_DBG("[rtbc]ctrl(0x%x)/buf_id(0x%x)/
		 * data_ptr(0x%x)/ex_data_ptr(0x%x)\n",
		 */
		/* rt_buf_ctrl.ctrl,  */
		/* rt_buf_ctrl.buf_id,  */
		/* rt_buf_ctrl.data_ptr,  */
		/* rt_buf_ctrl.ex_data_ptr); */
		/* } */
		/*  */
		if (_imgo_ == rt_dma || _img2o_ == rt_dma) {

#if defined(_rtbc_use_cq0c_)
			/* do nothing */
#else				/* for camsv */
/* if ( ( _camsv_imgo_  == rt_dma) ||
 * (_camsv2_imgo_ == rt_dma) )
 */
/* _bFlag = MTRUE; */
/* else */
			_bFlag = MFALSE;
#endif
			/*  */
			if (_bFlag == MTRUE) {
				if ((rt_buf_ctrl.ctrl ==
				ISP_RT_BUF_CTRL_ENQUE_FRMB) ||
				 (rt_buf_ctrl.ctrl ==
				 ISP_RT_BUF_CTRL_DEQUE_FRMB) ||
				 (rt_buf_ctrl.ctrl ==
				 ISP_RT_BUF_CTRL_IS_RDY_FRMB)
				 || (rt_buf_ctrl.ctrl ==
				 ISP_RT_BUF_CTRL_ENQUE_IMD_FRMB)) {
					/*  */
					p1_fbc[_imgo_].Reg_val =
					ISP_RD32(ISP_REG_ADDR_IMGO_FBC);
					p1_fbc[_img2o_].Reg_val =
					ISP_RD32(ISP_REG_ADDR_IMG2O_FBC);

					p1_fbc_reg[_imgo_] =
					ISP_REG_ADDR_IMGO_FBC;
					p1_fbc_reg[_img2o_] =
					ISP_REG_ADDR_IMG2O_FBC;

					p1_dma_addr_reg[_imgo_] =
					ISP_REG_ADDR_IMGO_BASE_ADDR;
					p1_dma_addr_reg[_img2o_] =
					ISP_REG_ADDR_IMG2O_BASE_ADDR;

#if 0
					if (IspInfo_FrmB.DebugMask &
						ISP_DBG_BUF_CTRL) {
						LOG_DBG(
						"[rtbc]:ctrl(%d),o(0x%x),2o(0x%x)",
						rt_buf_ctrl.ctrl,
						p1_fbc[_imgo_].Reg_val,
						p1_fbc[_img2o_].Reg_val);
					}
#endif
				}
			}
		} else {
#ifdef _rtbc_buf_que_2_0_
			if (rt_buf_ctrl.ctrl != ISP_RT_BUF_CTRL_DMA_EN_FRMB)
#endif
			{
				LOG_ERR("[rtbc]invalid dma channel(%d)",
				rt_dma);
				return -EFAULT;
			}
		}
		/* ISP_BUFFER_CTRL_STRUCT_FRMB rt_buf_ctrl */
		/* ISP_RT_BUF_CTRL_ENUM_FRMB ctrl          */
	switch (rt_buf_ctrl.ctrl) {
	case ISP_RT_BUF_CTRL_EXCHANGE_ENQUE_FRMB:
		break;
	case ISP_RT_BUF_CTRL_ENQUE_FRMB:
	case ISP_RT_BUF_CTRL_ENQUE_IMD_FRMB:
		/*  */
		deque_buf.sof_cnt = 0;
	if (copy_from_user(&rt_buf_info,
	(void __user *)rt_buf_ctrl.data_ptr,
	sizeof(struct ISP_RT_BUF_INFO_STRUCT_FRMB))
	== 0) {
		reg_val = ISP_RD32(ISP_REG_ADDR_TG_VF_CON);
/* reg_val2 = ISP_RD32(ISP_REG_ADDR_TG2_VF_CON); */
	LOG_INF("[rtbc][ENQUE]:dma:(%d),bufIdx(%d),base_pAddr(0x%x),base_vAddr(0x%llx)",
			rt_dma, rt_buf_info.bufIdx,
			rt_buf_info.base_pAddr,
			rt_buf_info.base_vAddr
			);
/* VF start already */
/* bool CurVF_En = MFALSE; */
		if ((_imgo_ == rt_dma) ||
			(_img2o_ == rt_dma)) {
			if (reg_val & 0x1)
				CurVF_En = MTRUE;
			else
				CurVF_En = MFALSE;
		}

		if (CurVF_En) {
			if (_bFlag == MTRUE) {
				unsigned int ch_imgo = 0,
				ch_img2o = 0;
						/*  */
				switch (rt_dma) {
				case _imgo_:
				case _img2o_:
					irqT = _IRQ;
					ch_imgo = _imgo_;
					ch_img2o = _img2o_;
					irqT_Lock = _IRQ;
				break;

				default:
					irqT_Lock = _IRQ;
					irqT = _IRQ;
					LOG_ERR("[rtbc]N.S.(%d)\n", rt_dma);
				break;
				}
#if 0
					static unsigned int RTBC_DBG_test;

					if (RTBC_DBG_test++ > 3) {
						RTBC_DBG_test -= 3;
						ISP_FBC_DUMP(
						rt_dma, 1, 0, 0, 0);
					}
#endif
/* copy_from/to_user might sleep when page-fault,
 * can't call in atomic contex
 */
/* spin_lock_irqsave(&(IspInfo_FrmB.
 * SpinLockIrq[irqT_Lock]), flags);
 */
			if (rt_buf_ctrl.ex_data_ptr != 0) {
/* borrow deque_buf.data memory , in order to
 * shirnk memory required,avoid compile err
 */
				if (copy_from_user
				(&deque_buf.data[0],
				(void __user *)
				rt_buf_ctrl.ex_data_ptr,
				sizeof(struct ISP_RT_BUF_INFO_STRUCT_FRMB))
				== 0) {
					spin_lock_irqsave(&
					(IspInfo_FrmB.
					SpinLockIrq[irqT_Lock]),
					flags);
					i = 0;
					if (deque_buf.data[0].
					bufIdx != 0xFFFF) {
/* replace the specific buffer with the same bufIdx */
/* LOG_ERR("[rtbc][replace2]Search By Idx"); */
					for (i = 0;
					i < ISP_RT_BUF_SIZE;
					i++) {
					if (pstRTBuf_FrmB->
					ring_buf[rt_dma].
					data[i].
					bufIdx ==
					deque_buf.
					data[0].
					bufIdx) {
						break;
					}
					}
					} else {
/* LOG_ERR("[rtbc][replace2]Search By Addr+"); */
					for (i = 0;
					i < ISP_RT_BUF_SIZE;
					i++) {
					if (pstRTBuf_FrmB->
					ring_buf[rt_dma].
					data[i].
					base_pAddr ==
					rt_buf_info.
					base_pAddr) {
/* LOG_ERR("[rtbc][replace2]Search By Addr i[%d]", i); */
						break;
					}
					}
					}

					if (i == ISP_RT_BUF_SIZE) {
/* error: can't search the buffer... */
						LOG_ERR(
						"[rtbc][replace2]error Can't get the idx-(0x%x)/Addr(0x%x) buf\n",
						deque_buf.
						data[0].bufIdx,
						rt_buf_info.
						base_pAddr);
						spin_unlock_irqrestore(&
						(IspInfo_FrmB.
						SpinLockIrq
						[irqT_Lock]),
						flags);
						IRQ_LOG_PRINTER(
						irqT, 0,
						_LOG_DBG);

					for (i = 0;
					i < ISP_RT_BUF_SIZE;
					i += 4) {
						LOG_ERR(
						"[rtbc][replace2]error idx-(0x%x/0x%x/0x%x/0x%x)\n",
						pstRTBuf_FrmB->
						ring_buf
						[rt_dma].
						data[i + 0].
						bufIdx,
						pstRTBuf_FrmB->
						ring_buf
						[rt_dma].
						data[i + 1].
						bufIdx,
						pstRTBuf_FrmB->
						ring_buf
						[rt_dma].
						data[i + 2].
						bufIdx,
						pstRTBuf_FrmB->
						ring_buf
						[rt_dma].
						data[i + 3].
						bufIdx);
					}
							return -EFAULT;
					}
								{
									/*  */
			{
/* LOG_DBG("[rtbc][ENQUE] dma(%d),old(%d) PA(0x%x) VA(0x%llx)",
 * rt_dma,i,pstRTBuf_FrmB->ring_buf[rt_dma].data[i].
 * base_pAddr,pstRTBuf_FrmB->ring_buf[rt_dma].
 * data[i].base_vAddr);
 */

				IRQ_LOG_KEEPER(
				irqT, 0,
				_LOG_DBG,
				"[rtbc][replace2]dma(%d),idx(%d) PA(0x%x_0x%x)\n",
				rt_dma,
				i,
				pstRTBuf_FrmB->
				ring_buf
				[rt_dma].
				data[i].
				base_pAddr,
				deque_buf.
				data[0].
				base_pAddr);
/* spin_lock_irqsave(&(IspInfo_FrmB.SpinLockIrq[irqT]), flags); */
				pstRTBuf_FrmB->
				ring_buf
				[rt_dma].
				data[i].memID =
				deque_buf.
				data[0].memID;
				pstRTBuf_FrmB->
				ring_buf
				[rt_dma].
				data[i].size =
				deque_buf.
				data[0].size;
				pstRTBuf_FrmB->
				ring_buf
				[rt_dma].
				data[i].
				base_pAddr =
				deque_buf.
				data[0].
				base_pAddr;
				pstRTBuf_FrmB->
				ring_buf
				[rt_dma].
				data[i].
				base_vAddr =
				deque_buf.
				data[0].
				base_vAddr;
				pstRTBuf_FrmB->
				ring_buf
				[rt_dma].
				data[i].
				bFilled =
				ISP_RTBC_BUF_EMPTY;
				pstRTBuf_FrmB->
				ring_buf
				[rt_dma].
				data[i].image.
				frm_cnt =
				_INVALID_FRM_CNT_;

#ifdef _rtbc_buf_que_2_0_
				if (pstRTBuf_FrmB->
				ring_buf
				[rt_dma].
				empty_count <
				pstRTBuf_FrmB->
				ring_buf
				[rt_dma].
				total_count)
				pstRTBuf_FrmB->
				ring_buf
				[rt_dma].
				empty_count++;
				else {
				spin_unlock_irqrestore
				(&(IspInfo_FrmB.
				SpinLockIrq
				[irqT_Lock]),
				flags);
				IRQ_LOG_PRINTER
				(irqT, 0,
				_LOG_DBG);
				LOG_ERR(
				"[rtbc]dma(%d),PA(0x%x),over enque",
				rt_dma,
				pstRTBuf_FrmB->
				ring_buf
				[rt_dma].
				data[i].
				base_pAddr);
				return
					-EFAULT;
				}
/* LOG_INF("RTBC_DBG7 e_dma_%d:%d %d %d\n",rt_dma,
 * pstRTBuf_FrmB->ring_buf[rt_dma].data[0].bFilled,
 * pstRTBuf_FrmB->ring_buf[rt_dma].data[1].bFilled,
 * pstRTBuf_FrmB->ring_buf[rt_dma].data[2].bFilled);
 */
#else
				pstRTBuf_FrmB->
				ring_buf
				[rt_dma].
				empty_count++;
#endif
/* spin_unlock_irqrestore(&(IspInfo_FrmB.SpinLockIrq[irqT]), flags); */
		}
		}
		} else {
			LOG_ERR(
			"cpy from user fail\n");
/*spin_unlock_irqrestore(&(IspInfo_FrmB.
 * SpinLockIrq[irqT_Lock]), flags);
 */
			return -EFAULT;
		}
		} else {
/* this case for camsv & pass1 fw rtbc */
			spin_lock_irqsave(&
			(IspInfo_FrmB.
			SpinLockIrq[irqT_Lock]),
			flags);
				for (i = 0; i <
				ISP_RT_BUF_SIZE; i++) {
								/*  */
					if (pstRTBuf_FrmB->ring_buf[rt_dma].
					data[i].base_pAddr ==
					rt_buf_info.base_pAddr) {
/* LOG_DBG("[rtbc]dma(%d),old(%d) PA(0x%x) VA(0x%x)",
 * rt_dma,i,pstRTBuf_FrmB->ring_buf[rt_dma].data[i].
 * base_pAddr,pstRTBuf_FrmB->ring_buf[rt_dma].
 * data[i].base_vAddr);
 */
/* spin_lock_irqsave(&(IspInfo_FrmB.
 * SpinLockIrq[irqT]), flags);
 */
						pstRTBuf_FrmB->
						ring_buf[rt_dma].
						data[i].bFilled =
						ISP_RTBC_BUF_EMPTY;
						pstRTBuf_FrmB->
						ring_buf[rt_dma].
						data[i].image.frm_cnt =
						_INVALID_FRM_CNT_;
#ifdef _rtbc_buf_que_2_0_
					if (pstRTBuf_FrmB->
					ring_buf[rt_dma].
					empty_count <
					pstRTBuf_FrmB->
					ring_buf[rt_dma].
					total_count)
					pstRTBuf_FrmB->
					ring_buf
					[rt_dma].
					empty_count++;
					else {
					spin_unlock_irqrestore
					(&(IspInfo_FrmB.
					SpinLockIrq
					[irqT_Lock]),
					flags);
					IRQ_LOG_PRINTER
					(irqT, 0,
					_LOG_DBG);
					LOG_ERR(
					"[rtbc]error:dma(%d),PA(0x%x),over enque",
					rt_dma,
					pstRTBuf_FrmB->
					ring_buf
					[rt_dma].
					data[i].
					base_pAddr);
						return -EFAULT;
						}

/* double check */
					if (1) {
					if (rt_buf_info.
					bufIdx !=
					pstRTBuf_FrmB->
					ring_buf
					[rt_dma].
					data[i].
					bufIdx) {
						LOG_ERR(
						"[rtbc][replace2]error: BufIdx MisMatch. 0x%x/0x%x",
						rt_buf_info.
						bufIdx,
						pstRTBuf_FrmB->
						ring_buf
						[rt_dma].
						data[i].
						bufIdx);
					}
					}
#else
					pstRTBuf_FrmB->
					ring_buf[rt_dma].
					empty_count++;
#endif
/* spin_unlock_irqrestore(&(IspInfo_FrmB.
 * SpinLockIrq[irqT]), flags);
 */
/* LOG_DBG("[rtbc]dma(%d),new(%d) PA(0x%x) VA(0x%x)",
 * rt_dma,i,pstRTBuf_FrmB->ring_buf[rt_dma].data[i].
 * base_pAddr,pstRTBuf_FrmB->ring_buf[rt_dma].
 * data[i].base_vAddr);
 */
							break;
						}
				}
				}
/* set RCN_INC = 1; */
/* RCNT++ */
/* FBC_CNT-- */


/* RCNT_INC++ */
						{
#ifdef _rtbc_buf_que_2_0_
			unsigned int z;

			if (rt_buf_ctrl.ctrl ==
			ISP_RT_BUF_CTRL_ENQUE_FRMB) {
				if ((irqT == _IRQ)
				|| (irqT == _IRQ_D)) {
					if ((MTRUE ==
						pstRTBuf_FrmB->
						ring_buf[ch_imgo].
						active) &&
						(MTRUE ==
						pstRTBuf_FrmB->
						ring_buf[ch_img2o].
						active)) {
					if (0 !=
					rt_buf_ctrl.
					ex_data_ptr) {
					if ((p1_fbc
					[rt_dma].
					Bits.
					FB_NUM
					==
					p1_fbc
					[rt_dma].
					Bits.
					FBC_CNT)
					||
					((p1_fbc
					[rt_dma].
					Bits.
					FB_NUM
					- 1) ==
					p1_fbc
					[rt_dma].
					Bits.
					FBC_CNT)) {
					mFwRcnt.
					bLoadBaseAddr
					[irqT]
					= MTRUE;
					}
					}
						dma_en_recorder
						[rt_dma]
						[mFwRcnt.
						DMA_IDX
						[rt_dma]] =
						MTRUE;
						mFwRcnt.
						DMA_IDX[rt_dma]
						=
						(++mFwRcnt.
						DMA_IDX[rt_dma]
						>=
						ISP_RT_BUF_SIZE)
						? (mFwRcnt.
						DMA_IDX
						[rt_dma] -
						ISP_RT_BUF_SIZE)
						: (mFwRcnt.
						DMA_IDX
						[rt_dma]);
/* LOG_INF("RTBC_DBG1:%d %d %d\n",rt_dma,dma_en_recorder
 * [rt_dma][mFwRcnt.DMA_IDX[rt_dma]],
 * mFwRcnt.DMA_IDX[rt_dma]);
 */
					for (z = 0;
					z <
					ISP_RT_BUF_SIZE;
					z++) {
					if (dma_en_recorder
						[ch_imgo]
						[mFwRcnt.rdIdx[irqT]]
						&& dma_en_recorder
						[ch_img2o]
						[mFwRcnt.rdIdx
						[irqT]]) {
					mFwRcnt.
					INC
					[irqT]
					[mFwRcnt.
					curIdx
					[irqT]++]
					= 1;
					dma_en_recorder
					[ch_imgo]
					[mFwRcnt.
					rdIdx
					[irqT]] =
					dma_en_recorder
					[ch_img2o]
					[mFwRcnt.
					rdIdx
					[irqT]]
					= MFALSE;
					mFwRcnt.
					rdIdx
					[irqT] =
					(++mFwRcnt.
					rdIdx
					[irqT] >=
					ISP_RT_BUF_SIZE)
					? (mFwRcnt.
					rdIdx
					[irqT] -
					ISP_RT_BUF_SIZE)
					: (mFwRcnt.
					rdIdx
					[irqT]);
/* LOG_INF("RTBC_DBG2:%d %d\n",mFwRcnt.rdIdx[irqT],
 * mFwRcnt.curIdx[irqT]);
 */
					} else {
						break;
					}
						}
					} else {
/* rcnt_sync only work when multi-dma ch enabled.
 * but in order to support multi-enque, these mech.
 * also to be worked under 1 dma ch enabled
 */
					if (MTRUE ==
					pstRTBuf_FrmB->
					ring_buf
					[rt_dma].
					active) {
					if (0 !=
					rt_buf_ctrl.
					ex_data_ptr) {
					if ((p1_fbc[rt_dma].
					Bits.FB_NUM ==
					p1_fbc[rt_dma].
					Bits.FBC_CNT)
					|| ((p1_fbc[
					rt_dma].Bits.
					FB_NUM - 1) ==
					p1_fbc[rt_dma].
					Bits.FBC_CNT)) {
						mFwRcnt.
						bLoadBaseAddr
						[irqT] =
						MTRUE;
					}
					}
					dma_en_recorder
					[rt_dma]
					[mFwRcnt.
					DMA_IDX
					[rt_dma]]
					= MTRUE;
					mFwRcnt.
					DMA_IDX
					[rt_dma] =
					(++mFwRcnt.
					DMA_IDX
					[rt_dma] >=
					ISP_RT_BUF_SIZE)
					? (mFwRcnt.
					DMA_IDX
					[rt_dma] -
					ISP_RT_BUF_SIZE)
					: (mFwRcnt.
					DMA_IDX
					[rt_dma]);

					for (z = 0;
					z <
					ISP_RT_BUF_SIZE;
					z++) {
					if (dma_en_recorder[
					rt_dma][
					mFwRcnt.rdIdx[
					irqT]]) {
					mFwRcnt.
					INC
					[irqT]
					[mFwRcnt.
					curIdx
					[irqT]++]
					= 1;
					dma_en_recorder
					[rt_dma]
					[mFwRcnt.
					rdIdx
					[irqT]]
					= MFALSE;
					mFwRcnt.
					rdIdx
					[irqT] =
					(++mFwRcnt.
					rdIdx
					[irqT] >=
					ISP_RT_BUF_SIZE)
					? (mFwRcnt.
					rdIdx
					[irqT] -
					ISP_RT_BUF_SIZE)
					: (mFwRcnt.
					rdIdx
					[irqT]);
					} else {
						break;
					}
					}
					} else {
					spin_unlock_irqrestore
					(&(IspInfo_FrmB.
					SpinLockIrq
					[irqT_Lock]),
					flags);
					LOG_ERR(
					"[rtbc]error:dma(%d) r not being activated(%d)",
					rt_dma,
					pstRTBuf_FrmB->
					ring_buf
					[rt_dma].
					active);
					return
						-EFAULT;
					}
					}
				} else {	/* camsv case */
				if (MTRUE ==
				pstRTBuf_FrmB->
				ring_buf[rt_dma].
				active) {
					if (0 !=
					rt_buf_ctrl.
					ex_data_ptr) {
					if ((p1_fbc
					[rt_dma].
					Bits.
					FB_NUM
					== p1_fbc
					[rt_dma].
					Bits.
					FBC_CNT)
					|| ((p1_fbc
					[rt_dma].
					Bits.
					FB_NUM -
					1) ==
					p1_fbc
					[rt_dma].
					Bits.
					FBC_CNT)) {
					mFwRcnt.
					bLoadBaseAddr
					[irqT] =
					MTRUE;
					}
					}
					dma_en_recorder
					[rt_dma]
					[mFwRcnt.
					DMA_IDX
					[rt_dma]] =
					MTRUE;
					mFwRcnt.
					DMA_IDX[rt_dma]
					= (++mFwRcnt.
					DMA_IDX[rt_dma]
					>= ISP_RT_BUF_SIZE)
					? (mFwRcnt.
					DMA_IDX
					[rt_dma] -
					ISP_RT_BUF_SIZE)
					: (mFwRcnt.
					DMA_IDX
					[rt_dma]);

					for (z = 0;
					z <
					ISP_RT_BUF_SIZE;
					z++) {
					if (dma_en_recorder
					[rt_dma]
					[mFwRcnt.rdIdx
					[irqT]]) {
					mFwRcnt.
					INC
					[irqT]
					[mFwRcnt.
					curIdx
					[irqT]++]
					= 1;
					dma_en_recorder
					[rt_dma]
					[mFwRcnt.
					rdIdx
					[irqT]]
					 = MFALSE;
					mFwRcnt.
					rdIdx
					[irqT] =
					(++mFwRcnt.
					rdIdx
					[irqT] >=
					ISP_RT_BUF_SIZE)
					? (mFwRcnt.
					rdIdx
					[irqT] -
					ISP_RT_BUF_SIZE)
					: (mFwRcnt.
					rdIdx
					[irqT]);
					} else {
						break;
					}
					}
				} else {
					spin_unlock_irqrestore
					(&(IspInfo_FrmB.
					SpinLockIrq
					[irqT_Lock]),
					flags);
					LOG_ERR(
					"[rtbc]error:dma(%d) r not being activated(%d)",
					rt_dma,
					pstRTBuf_FrmB->
					ring_buf
					[rt_dma].
					active);
					return -EFAULT;
				}
				}
			} else {
/* immediate enque mode */
				unsigned int _openedDma = 1;
				bool _bypass = MFALSE;

				if ((MTRUE ==
				pstRTBuf_FrmB->
				ring_buf[ch_imgo].active)
				&& (MTRUE ==
				pstRTBuf_FrmB->
				ring_buf[ch_img2o].
				active)) {
/* record wheather all enabled dma r alredy enqued, */
/* rcnt_inc will only be pulled to high once all
 * enabled dma r enqued.
 */
/* inorder to reduce the probability of crossing vsync. */
/* this global par. r no use under immediate mode,
 * borrow this to shirk memory
 */
				dma_en_recorder[rt_dma][0] = MTRUE;
				_openedDma = 2;
				if ((dma_en_recorder
				[ch_imgo][0] == MTRUE)
				&& (dma_en_recorder
				[ch_img2o][0] ==
				MTRUE)) {
				dma_en_recorder
				[ch_imgo][0] =
				dma_en_recorder
				[ch_img2o][0] =
				MFALSE;
				} else
					_bypass = MTRUE;
				}
				if (_bypass == MFALSE) {
					if ((p1_fbc[rt_dma].Bits.
					FB_NUM ==
					p1_fbc[rt_dma].Bits.
					FBC_CNT) ||
					((p1_fbc[rt_dma].Bits.
					FB_NUM - 1) ==
					p1_fbc[rt_dma].Bits.
					FBC_CNT)) {
/* write to phy register */
/* LOG_INF("[rtbc_%d][ENQUE] write2Phy directly(%d,%d)",
 * rt_dma,p1_fbc[rt_dma].Bits.FB_NUM,p1_fbc[rt_dma].
 * Bits.FBC_CNT);
 */
					IRQ_LOG_KEEPER(irqT,
					0, _LOG_DBG,
					"[rtbc_%d][ENQUE] write2Phy directly(%d,%d) ",
					rt_dma,
					p1_fbc
					[rt_dma].
					Bits.
					FB_NUM,
					p1_fbc
					[rt_dma].
					Bits.
					FBC_CNT);
					if (pstRTBuf_FrmB->
					ring_buf[ch_imgo].
					active == MTRUE)
					ISP_WR32(
					p1_dma_addr_reg[ch_imgo],
					pstRTBuf_FrmB->
					ring_buf[ch_imgo].data[i].
					base_pAddr);
					if (pstRTBuf_FrmB->
					ring_buf[ch_img2o].
					active == MTRUE)
					ISP_WR32(
					p1_dma_addr_reg[ch_img2o],
					pstRTBuf_FrmB->
					ring_buf[ch_img2o].data[i].
					base_pAddr);
					}
					if (_openedDma == 1) {
					p1_fbc[rt_dma].Bits.
					RCNT_INC = 1;
					ISP_WR32(p1_fbc_reg
					[rt_dma],
					p1_fbc
					[rt_dma].
					Reg_val);
					IRQ_LOG_KEEPER(irqT,
					0, _LOG_DBG,
					"  RCNT_INC(dma:0x%x)\n",
					rt_dma);
					} else {
					p1_fbc[ch_imgo].
					Bits.RCNT_INC = 1;
					ISP_WR32(p1_fbc_reg
					[ch_imgo],
					p1_fbc
					[ch_imgo].
					Reg_val);
					p1_fbc[ch_img2o].
					Bits.RCNT_INC = 1;
					ISP_WR32(p1_fbc_reg
					[ch_img2o],
					p1_fbc
					[ch_img2o].
					Reg_val);
					IRQ_LOG_KEEPER(irqT,
					0, _LOG_DBG,
					"  RCNT_INC(dma:0x%x)\n",
					ch_imgo);
					IRQ_LOG_KEEPER(irqT,
					0, _LOG_DBG,
					"  RCNT_INC(dma:0x%x)\n",
					ch_img2o);
					}
#ifdef _89SERIAL_
				++m_P1_RCNT_INC_CNT;
#endif
				}
			}
#else
/* if ( FB_NUM==FBC_CNT ||  (FB_NUM-1)==FBC_CNT ) */
					if ((p1_fbc[rt_dma].
					Bits.FB_NUM == p1_fbc[
					rt_dma].Bits.FBC_CNT) ||
					((p1_fbc[rt_dma].
					Bits.FB_NUM - 1) ==
					p1_fbc[rt_dma].Bits.
					FBC_CNT)) {
/* write to phy register */
/* LOG_INF("[rtbc_%d][ENQUE] write2Phy directly(%d,%d)"
 * ,rt_dma,p1_fbc[rt_dma].Bits.FB_NUM,
 * p1_fbc[rt_dma].Bits.FBC_CNT);
 */
						IRQ_LOG_KEEPER(
						irqT, 0, _LOG_DBG,
						"[rtbc_%d][ENQUE] write2Phy directly(%d,%d)\n",
						rt_dma,
						p1_fbc[rt_dma].
						Bits.FB_NUM,
						p1_fbc[rt_dma].
						Bits.FBC_CNT);
						ISP_WR32(
						p1_dma_addr_reg[
						rt_dma],
						pstRTBuf_FrmB->
						ring_buf[rt_dma].
						data[i].
						base_pAddr);
					}
/* patch camsv hw bug
 * if( (_camsv_imgo_ == rt_dma) ||
 * (_camsv2_imgo_ == rt_dma) ){
 * p1_fbc[rt_dma].Bits.RCNT_INC = 1;
 * ISP_WR32(p1_fbc_reg[rt_dma],p1_fbc[rt_dma].Reg_val);
 * p1_fbc[rt_dma].Bits.RCNT_INC = 0;
 * ISP_WR32(p1_fbc_reg[rt_dma],p1_fbc[rt_dma].Reg_val);
 * } else{
 */
							p1_fbc[rt_dma].
							Bits.RCNT_INC = 1;
							ISP_WR32(p1_fbc_reg[
							rt_dma],
							p1_fbc[rt_dma].
							Reg_val);
							/* } */
#endif
						}
						spin_unlock_irqrestore(&
						(IspInfo_FrmB.
						SpinLockIrq[irqT_Lock]),
						flags);
						IRQ_LOG_PRINTER(
						irqT, 0, _LOG_DBG);
						/*  */
					if (IspInfo_FrmB.DebugMask
					& ISP_DBG_BUF_CTRL) {
/* LOG_DBG("[rtbc][ENQUE]:dma(%d),PA(0x%x),O(0x%x),
 * ZO(0x%x),O_D(0x%x),ZO_D(0x%x),camsv(0x%x/0x%x)
 * fps(%d/%d/%d/%d)us",
 */
						LOG_DBG(
						"[rtbc][ENQUE]:dma(%d),PA(0x%x),O(0x%x),2O(0x%x),fps(%d/%d/%d/%d)us,IMD_%d\n",
						rt_dma,
						rt_buf_info.base_pAddr,
						ISP_RD32(
						ISP_REG_ADDR_IMGO_BASE_ADDR),
						ISP_RD32(
						ISP_REG_ADDR_IMG2O_BASE_ADDR),
						avg_frame_time[_PASS1],
						avg_frame_time[_PASS1_D],
						avg_frame_time[_CAMSV],
						avg_frame_time[_CAMSV_D],
						rt_buf_ctrl.ctrl);
						}

					}
				} else {
					ISP_RTBC_ENQUE_FRMB(rt_dma,
					&rt_buf_info);
				}

			} else {
				LOG_ERR("[rtbc][ENQUE]:copy_from_user fail");
				return -EFAULT;
			}
			break;

		case ISP_RT_BUF_CTRL_DEQUE_FRMB:
			switch (rt_dma) {
			case _imgo_:
			case _img2o_:
				irqT_Lock = _IRQ;
				irqT = _IRQ;
				break;
			default:
				LOG_ERR("unsupported DMAO:0x%x\n", rt_dma);
				return -EFAULT;
			}
			/*  */
			reg_val = ISP_RD32(ISP_REG_ADDR_TG_VF_CON);
			/* VF start already */
			spin_lock_irqsave(&(IspInfo_FrmB.
			SpinLockIrq[irqT_Lock]), flags);
			if (reg_val & 0x01) {
				if (_bFlag == MTRUE) {
					unsigned int _magic;

					deque_buf.count = P1_DEQUE_CNT;
#ifdef _rtbc_buf_que_2_0_
/* p1_fbc[rt_dma].Bits.WCNT - 1;    //WCNT = [1,2,..] */
					iBuf = pstRTBuf_FrmB->
					ring_buf[rt_dma].read_idx;
					pstRTBuf_FrmB->ring_buf[
					rt_dma].read_idx =
					(pstRTBuf_FrmB->ring_buf[
					rt_dma].read_idx + 1) %
					pstRTBuf_FrmB->ring_buf[
					rt_dma].total_count;
					if (deque_buf.count != P1_DEQUE_CNT) {
						LOG_ERR(
						"support only deque 1 buf at 1 time\n");
						deque_buf.count = P1_DEQUE_CNT;
					}
#else
/* RCNT = [1,2,3,...] */
					iBuf = p1_fbc[rt_dma].Bits.RCNT - 1;
#endif
					for (i = 0; i < deque_buf.count; i++) {
						unsigned int out;

						deque_buf.data[i].memID =
						pstRTBuf_FrmB->ring_buf[
						rt_dma].data[iBuf + i].memID;
						deque_buf.data[i].size =
						pstRTBuf_FrmB->ring_buf[
						rt_dma].data[iBuf + i].size;
						deque_buf.data[i].base_vAddr =
						pstRTBuf_FrmB->ring_buf[
						rt_dma].data[iBuf + i].
						base_vAddr;
						deque_buf.data[i].base_pAddr =
						pstRTBuf_FrmB->ring_buf[
						rt_dma].data[iBuf + i].
						base_pAddr;
						deque_buf.data[i].timeStampS =
						pstRTBuf_FrmB->ring_buf[
						rt_dma].data[iBuf + i].
						timeStampS;
						deque_buf.data[i].timeStampUs =
						pstRTBuf_FrmB->ring_buf[
						rt_dma].data[iBuf + i].
						timeStampUs;
						deque_buf.data[i].image.w =
						pstRTBuf_FrmB->ring_buf[
						rt_dma].data[iBuf + i].image.w;
						deque_buf.data[i].image.h =
						pstRTBuf_FrmB->ring_buf[
						rt_dma].data[iBuf + i].image.h;
						deque_buf.data[i].image.xsize =
						pstRTBuf_FrmB->ring_buf[
						rt_dma].data[iBuf + i].image.
						xsize;
						deque_buf.data[i].image.stride =
						pstRTBuf_FrmB->ring_buf[
						rt_dma].data[iBuf + i].image.
						stride;
						deque_buf.data[i].image.fmt =
						pstRTBuf_FrmB->ring_buf[
						rt_dma].data[iBuf + i].image.
						fmt;
						deque_buf.data[i].image.pxl_id =
						pstRTBuf_FrmB->ring_buf[
						rt_dma].data[iBuf + i].image.
						pxl_id;
						deque_buf.data[i].image.wbn =
						pstRTBuf_FrmB->ring_buf[
						rt_dma].data[iBuf + i].image.
						wbn;
						deque_buf.data[i].image.ob =
						pstRTBuf_FrmB->ring_buf[
						rt_dma].data[iBuf + i].image.
						ob;
						deque_buf.data[i].image.lsc =
						pstRTBuf_FrmB->ring_buf[
						rt_dma].data[iBuf + i].image.
						lsc;
						deque_buf.data[i].image.rpg =
						pstRTBuf_FrmB->ring_buf[
						rt_dma].data[iBuf + i].image.
						rpg;
						deque_buf.data[i].image.
						m_num_0 =
						pstRTBuf_FrmB->ring_buf[
						rt_dma].data[iBuf + i].image.
						m_num_0;
						deque_buf.data[i].
						image.frm_cnt =
						pstRTBuf_FrmB->ring_buf[
						rt_dma].data[iBuf + i].image.
						frm_cnt;
						deque_buf.data[i].HrzInfo.srcX =
						pstRTBuf_FrmB->ring_buf[
						rt_dma].data[iBuf + i].HrzInfo.
						srcX;
						deque_buf.data[i].HrzInfo.srcY =
						pstRTBuf_FrmB->ring_buf[
						rt_dma].data[iBuf + i].HrzInfo.
						srcY;
						deque_buf.data[i].HrzInfo.srcW =
						pstRTBuf_FrmB->ring_buf[
						rt_dma].data[iBuf + i].HrzInfo.
						srcW;
						deque_buf.data[i].HrzInfo.srcH =
						pstRTBuf_FrmB->ring_buf[
						rt_dma].data[iBuf + i].HrzInfo.
						srcH;
						deque_buf.data[i].HrzInfo.dstW =
						pstRTBuf_FrmB->ring_buf[
						rt_dma].data[iBuf + i].HrzInfo.
						dstW;
						deque_buf.data[i].HrzInfo.dstH =
						pstRTBuf_FrmB->ring_buf[
						rt_dma].data[iBuf + i].HrzInfo.
						dstH;
						deque_buf.data[i].dmaoCrop.x =
						pstRTBuf_FrmB->ring_buf[
						rt_dma].data[iBuf + i].
						dmaoCrop.x;
						deque_buf.data[i].dmaoCrop.y =
						pstRTBuf_FrmB->ring_buf[
						rt_dma].data[iBuf + i].
						dmaoCrop.y;
						deque_buf.data[i].dmaoCrop.w =
						pstRTBuf_FrmB->ring_buf[
						rt_dma].data[iBuf + i].
						dmaoCrop.w;
						deque_buf.data[i].dmaoCrop.h =
						pstRTBuf_FrmB->ring_buf[
						rt_dma].data[iBuf + i].
						dmaoCrop.h;
						deque_buf.data[i].bufIdx =
						pstRTBuf_FrmB->ring_buf[
						rt_dma].data[iBuf + i].
						bufIdx;
#ifdef _MAGIC_NUM_ERR_HANDLING_

/*LOG_ERR("[rtbc][deque][m_num]:d(%d),fc(0x%x),
 * lfc0x%x,m0(0x%x),lm#(0x%x)\n",
 * rt_dma,
 * deque_buf.data[i].image.frm_cnt,
 * m_LastFrmCnt[rt_dma]
 * ,deque_buf.data[i].image.m_num_0,
 * m_LastMNum[rt_dma]);
 */
						_magic = deque_buf.
						data[i].image.m_num_0;

					if (_DUMMY_MAGIC_ &
						deque_buf.data[i].
						image.m_num_0)
						_magic = (deque_buf.
						data[i].image.
						m_num_0 &
						(~_DUMMY_MAGIC_));


					if ((_INVALID_FRM_CNT_
					== deque_buf.data[i].
					image.frm_cnt) ||
					(m_LastMNum[rt_dma]
					> _magic)) {
							/*  */
					if ((_DUMMY_MAGIC_ &
					deque_buf.data[i].
					image.m_num_0) == 0)
					deque_buf.data[i].
					image.m_num_0 |=
					_UNCERTAIN_MAGIC_NUM_FLAG_;
							/*  */
							IRQ_LOG_KEEPER(irqT,
							0, _LOG_DBG,
							"m# uncertain:dma(%d),m0(0x%x),fcnt(0x%x),Lm#(0x%x)",
							rt_dma,
							deque_buf.data[i].image.
							m_num_0,
							deque_buf.data[i].image.
							frm_cnt,
							m_LastMNum[rt_dma]);
#ifdef T_STAMP_2_0
/* patch here is because of that uncertain should
 * happen only in missing SOF. And because of FBC,
 * image still can be deque. That's why  timestamp
 * still need to be increased here.
 */
					if (m_T_STAMP.fps >
					SlowMotion) {
						m_T_STAMP.T_ns +=
						((unsigned long long)
						m_T_STAMP.
						interval_us * 1000);
					if (++m_T_STAMP.fcnt
					== m_T_STAMP.fps) {
						m_T_STAMP.fcnt = 0;
						m_T_STAMP.T_ns +=
						((unsigned long long)
						m_T_STAMP.
						compensation_us
						* 1000);
							}
						}
#endif
					/**/	} else {
							m_LastMNum[rt_dma]
							= _magic;
						}

#endif

						DMA_TRANS(rt_dma, out);
						bBufFilled = MTRUE;
					if (pstRTBuf_FrmB->ring_buf[
					rt_dma].data[iBuf + i].
					bFilled !=
					ISP_RTBC_BUF_FILLED) {
						bBufFilled = MFALSE;
/*
 * Unknown extra unbalanced deque will break FBC.
 * To avoid this, we still return the uncertain result, but keep bFilled as
 * old value and prevent further 'start idx mismatch' error.
 * The FBC will recover after the error slot been enque again.
 */
					        if ((_DUMMY_MAGIC_ &
					                deque_buf.data[i].
					                image.m_num_0) == 0)
					                deque_buf.data[i].
					                image.m_num_0 |=
					                _UNCERTAIN_MAGIC_NUM_FLAG_;

					}else {
						pstRTBuf_FrmB->ring_buf[
						rt_dma].data[iBuf + i].
						bFilled =
						ISP_RTBC_BUF_LOCKED;
					}
						deque_buf.sof_cnt =
						sof_count[out];
						deque_buf.img_cnt =
						pstRTBuf_FrmB->ring_buf[
						rt_dma].img_cnt;
						spin_unlock_irqrestore(&
						(IspInfo_FrmB.
						SpinLockIrq[irqT_Lock]),
						flags);
						IRQ_LOG_PRINTER(
						irqT, 0, _LOG_DBG);
						/*  */
					if (bBufFilled == MFALSE) {
						LOG_ERR(
						"deque buf:d(%d)[%d] is not filled.",
						rt_dma, iBuf + i);
					}
/* LOG_INF("RTBC_DBG7 d_dma_%d:%d %d %d\n",rt_dma,
 * pstRTBuf_FrmB->ring_buf[rt_dma].data[0].bFilled,
 * pstRTBuf_FrmB->ring_buf[rt_dma].data[1].bFilled,
 * pstRTBuf_FrmB->ring_buf[rt_dma].data[2].bFilled);
 */
					if (IspInfo_FrmB.DebugMask &
						ISP_DBG_BUF_CTRL) {
						LOG_INF(
						"[rtbc][DEQUE](%d):d(%d)/id(0x%x)/bs(0x%x)/va(0x%llx)/pa(0x%x)/t(%d.%d)/img(%d,%d,%d,%d,%d,%d,%d,%d)/m(0x%x)/fc(%d)/hrz(%d,%d,%d,%d,%d,%d),dmao(%d,%d,%d,%d),lm#(0x%x)",
						iBuf + i, rt_dma,
						deque_buf.data[i].memID,
						deque_buf.data[i].size,
						deque_buf.data[i].base_vAddr,
						deque_buf.data[i].base_pAddr,
						deque_buf.data[i].timeStampS,
						deque_buf.data[i].timeStampUs,
						deque_buf.data[i].image.w,
						deque_buf.data[i].image.h,
						deque_buf.data[i].image.stride,
						deque_buf.data[i].image.fmt,
						deque_buf.data[i].image.wbn,
						deque_buf.data[i].image.ob,
						deque_buf.data[i].image.lsc,
						deque_buf.data[i].image.rpg,
						deque_buf.data[i].image.m_num_0,
						deque_buf.data[i].image.frm_cnt,
						deque_buf.data[i].HrzInfo.srcX,
						deque_buf.data[i].HrzInfo.srcY,
						deque_buf.data[i].HrzInfo.srcW,
						deque_buf.data[i].HrzInfo.srcH,
						deque_buf.data[i].HrzInfo.dstW,
						deque_buf.data[i].HrzInfo.dstH,
						deque_buf.data[i].dmaoCrop.x,
						deque_buf.data[i].dmaoCrop.y,
						deque_buf.data[i].dmaoCrop.w,
						deque_buf.data[i].dmaoCrop.h,
						m_LastMNum[rt_dma]);


							/*  */
#if 0
					LOG_DBG(
					"[rtbc][DEQUE]:D(%d),TStamp\"%d.%06d\",o(0x%08x),2o(0x%08x),i(%d),VA(0x%x),PA(0x%x),O(0x%x),2O(0x%x)",
					rt_dma,
					deque_buf.data[i].timeStampS,
					deque_buf.data[i].timeStampUs,
					p1_fbc[_imgo_], p1_fbc[_img2o_],
					iBuf + i,
					deque_buf.data[i].base_vAddr,
					deque_buf.data[i].base_pAddr,
					ISP_RD32(ISP_REG_ADDR_IMGO_BASE_ADDR),
					ISP_RD32(ISP_REG_ADDR_IMG2O_BASE_ADDR));
#endif
						}
						/*  */
/* tstamp = deque_buf.data[i].timeStampS*1000000+
 * deque_buf.data[i].timeStampUs;
 */
/* if ( (0 != prv_tstamp) &&
 * (prv_tstamp >= tstamp) ) {
 */
					if (prv_tstamp_s[rt_dma] != 0) {
					if ((prv_tstamp_s[rt_dma] >
					deque_buf.data[i].timeStampS)
					|| ((prv_tstamp_s[rt_dma] ==
					deque_buf.data[i].timeStampS)
					&& (prv_tstamp_us[rt_dma] >=
					deque_buf.data[i].
					timeStampUs))) {
					LOG_ERR(
					"[rtbc]TS rollback,D(%d),prv\"%d.%06d\",cur\"%d.%06d\"",
					rt_dma,
					(int)(prv_tstamp_s[rt_dma]),
					(int)(prv_tstamp_us[rt_dma]),
					(int)(deque_buf.data[i].
					timeStampS),
					(int)(deque_buf.data[i].
					timeStampUs));
					//ISP_DumpReg_FrmB();
						}
					}
					prv_tstamp_s[rt_dma] =
					deque_buf.
					data[i].timeStampS;
					prv_tstamp_us[rt_dma] =
					deque_buf.data[i].timeStampUs;
					}

#if 0
					LOG_DBG(
					"+LARB in DEQUE,BWL(0x%08X)/(0x%08X)/(0x%08X)/(0x%08X),220(0x%08X)/(0x%08X),0x14(0x%08X)/(0x%08X)/(0x%08X)/(0x%08X)/(0x%08X)",
					ISP_RD32(0xF0202000 + 0x204),
					ISP_RD32(0xF0202000 + 0x20c),
					ISP_RD32(0xF0202000 + 0x210),
					ISP_RD32(0xF0202000 + 0x214),
					ISP_RD32(0xF0202000 + 0x220),
					ISP_RD32(0xF0202000 + 0x230),
					ISP_RD32(SMI_LARB0 + 0x10),
					ISP_RD32(SMI_LARB1 + 0x10),
					ISP_RD32(SMI_LARB2 + 0x10),
					ISP_RD32(SMI_LARB3 + 0x10),
					ISP_RD32(SMI_LARB4 + 0x10));
#endif
				}
			} else {
				ISP_RTBC_DEQUE_FRMB(rt_dma, &deque_buf);
			}

			if (deque_buf.count) {
				/*  */
/* if(copy_to_user((void*)rt_buf_ctrl.data_ptr,&deque_buf,
 * sizeof(ISP_DEQUE_BUF_INFO_STRUCT_FRMB)) != 0)
 */
				if (copy_to_user((void __user *)
					rt_buf_ctrl.pExtend, &deque_buf,
				  sizeof(struct ISP_DEQUE_BUF_INFO_STRUCT_FRMB))
				  != 0) {
					LOG_ERR(
					"[rtbc][DEQUE]:copy_to_user failed");
					Ret = -EFAULT;
				}

			} else {
				/*  */
				/* spin_unlock_irqrestore(&(IspInfo_FrmB.
				 * SpinLockRTBC),g_Flash_SpinLock);
				 */
				LOG_ERR("[rtbc][DEQUE]:no filled buffer");
				Ret = -EFAULT;
			}

			break;
		case ISP_RT_BUF_CTRL_CUR_STATUS_FRMB:
			reg_val = ISP_RD32(ISP_REG_ADDR_TG_VF_CON) & 0x1;
			ISP_FBC_DUMP(rt_buf_ctrl.buf_id, reg_val, 0, 0, 0);
			break;
		case ISP_RT_BUF_CTRL_IS_RDY_FRMB:
			/*  */
			/* spin_lock_irqsave(&(IspInfo_FrmB.
			 * SpinLockRTBC),g_Flash_SpinLock);
			 */
			/*  */
			bWaitBufRdy = 1;
#ifdef _rtbc_buf_que_2_0_
			switch (rt_dma) {
			case _imgo_:
			case _img2o_:
				irqT_Lock = _IRQ;
				irqT = _IRQ;
				break;

			default:
				LOG_ERR("[rtbc]N.S.(%d)\n", rt_dma);
				irqT_Lock = _IRQ;
				irqT = _IRQ;
				break;
			}

			spin_lock_irqsave(&(IspInfo_FrmB.
			SpinLockIrq[irqT_Lock]), flags);

			if (ISP_RTBC_BUF_FILLED ==
			pstRTBuf_FrmB->ring_buf[rt_dma].
			data[pstRTBuf_FrmB->ring_buf[rt_dma].
			read_idx].bFilled) {
				bWaitBufRdy = 0;
			}
			if (IspInfo_FrmB.DebugMask & ISP_DBG_BUF_CTRL) {
				unsigned int z;

				IRQ_LOG_KEEPER(irqT, 0, _LOG_DBG,
				"cur dma:%d,read idx = %d,total cnt = %d,bWaitBufRdy= %d  ,",
				rt_dma, pstRTBuf_FrmB->
				ring_buf[rt_dma].read_idx,
				pstRTBuf_FrmB->ring_buf[rt_dma].
				total_count, bWaitBufRdy);
				for (z = 0; z < pstRTBuf_FrmB->ring_buf[
				rt_dma].total_count; z++) {
					IRQ_LOG_KEEPER(irqT, 0, _LOG_DBG, "%d_",
					pstRTBuf_FrmB->ring_buf[rt_dma].data[z].
					bFilled);
				}
				IRQ_LOG_KEEPER(irqT, 0, _LOG_DBG, "\n");
			}
			spin_unlock_irqrestore(&(IspInfo_FrmB.
			SpinLockIrq[irqT_Lock]), flags);
			IRQ_LOG_PRINTER(irqT, 0, _LOG_DBG);
#else
#if defined(_rtbc_use_cq0c_)
			bWaitBufRdy = p1_fbc[rt_dma].Bits.FBC_CNT ? 0 : 1;
#else
			bWaitBufRdy = MTRUE;
#endif
#endif

/*  */
/* spin_unlock_irqrestore(&(
 * IspInfo_FrmB.SpinLockRTBC),g_Flash_SpinLock);
 */
/*  */
/* if(copy_to_user((void*)rt_buf_ctrl.data_ptr,
 * &bWaitBufRdy, sizeof(unsigned int)) != 0)
 */
			if (copy_to_user
			    ((void __user *)rt_buf_ctrl.pExtend, &bWaitBufRdy,
			     sizeof(unsigned int)) != 0) {
				LOG_ERR("[rtbc][IS_RDY]:copy_to_user failed");
				Ret = -EFAULT;
			}
			/*  */
			/* spin_unlock_irqrestore(&
			 * (IspInfo_FrmB.SpinLockRTBC), flags);
			 */
			/*  */
			break;
		case ISP_RT_BUF_CTRL_GET_SIZE_FRMB:
			/*  */
			size = pstRTBuf_FrmB->ring_buf[rt_dma].total_count;
/*  */
/* if(IspInfo_FrmB.DebugMask & ISP_DBG_BUF_CTRL) { */
/* LOG_DBG("[rtbc][GET_SIZE]:rt_dma(%d)/
 * size(%d)",rt_dma,size);
 */
/* } */
/* if(copy_to_user((void*)rt_buf_ctrl.data_ptr,
 * &size, sizeof(unsigned int)) != 0)
 */
			if (copy_to_user((void __user *)rt_buf_ctrl.
				pExtend, &size, sizeof(unsigned int)) != 0) {
				LOG_ERR("[rtbc][GET_SIZE]:copy_to_user failed");
				Ret = -EFAULT;
			}
			break;
		case ISP_RT_BUF_CTRL_CLEAR_FRMB:
			/*  */
			if (IspInfo_FrmB.DebugMask & ISP_DBG_BUF_CTRL)
				LOG_INF("[rtbc][CLEAR]:rt_dma(%d)", rt_dma);
			/*  */
			switch (rt_dma) {
			case _imgo_:
			case _img2o_:
				memset((void *)&g_DmaErr_p1[0], 0,
				sizeof(unsigned int) * nDMA_ERR_P1);
				memset((void *)IspInfo_FrmB.IrqInfo.
				LastestSigTime_usec[ISP_IRQ_TYPE_INT],
				       0, sizeof(unsigned int) * 32);
				memset((void *)IspInfo_FrmB.IrqInfo.
				LastestSigTime_sec[ISP_IRQ_TYPE_INT], 0,
				       sizeof(unsigned int) * 32);
				memset((void *)IspInfo_FrmB.IrqInfo.
				Eismeta[ISP_IRQ_TYPE_INT], 0,
				sizeof(struct ISP_EIS_META_STRUCT) *
				EISMETA_RINGSIZE);
				gEismetaRIdx = 0;
				gEismetaWIdx = 0;
				gEismetaInSOF = 0;
				break;

			default:
				LOG_ERR("[rtbc][CLEAR]N.S.(%d)\n", rt_dma);
				return -EFAULT;
			}
/* remove, cause clear will be involked only when
 * current module r totally stopped
 */
/* spin_lock_irqsave(&(IspInfo_FrmB.SpinLockIrq[
 * irqT_Lock]), flags);
 */

#if 0
			pstRTBuf_FrmB->ring_buf[rt_dma].total_count = 0;
			pstRTBuf_FrmB->ring_buf[rt_dma].start = 0;
			pstRTBuf_FrmB->ring_buf[rt_dma].empty_count = 0;
			pstRTBuf_FrmB->ring_buf[rt_dma].active = 0;

			for (i = 0; i < ISP_RT_BUF_SIZE; i++) {
				if (pstRTBuf_FrmB->ring_buf[rt_dma].
					data[i].base_pAddr ==
				    rt_buf_info.base_pAddr) {
					buffer_exist = 1;
					break;
				}
				/*  */
				if (pstRTBuf_FrmB->ring_buf[rt_dma].
					data[i].base_pAddr == 0)
					break;
			}
#else
/* if ((_imgo_ == rt_dma)||(_rrzo_ == rt_dma)||
 * (_imgo_d_ == rt_dma)||(_rrzo_d_ == rt_dma))
 */
			/* active */
			pstRTBuf_FrmB->ring_buf[rt_dma].active = MFALSE;
			memset((char *)&pstRTBuf_FrmB->ring_buf[rt_dma], 0x00,
			       sizeof(struct ISP_RT_RING_BUF_INFO_STRUCT_FRMB));
			/* init. frmcnt before vf_en */
			for (i = 0; i < ISP_RT_BUF_SIZE; i++) {
				pstRTBuf_FrmB->ring_buf[rt_dma].
				data[i].image.frm_cnt =
				    _INVALID_FRM_CNT_;
			}

			memset((char *)&prv_tstamp_s[rt_dma],
			0x0, sizeof(unsigned int));
			memset((char *)&prv_tstamp_us[rt_dma],
			0x0, sizeof(unsigned int));
#ifdef _rtbc_buf_que_2_0_
			memset((void *)dma_en_recorder[rt_dma], 0,
			       sizeof(unsigned char) * ISP_RT_BUF_SIZE);
			mFwRcnt.DMA_IDX[rt_dma] = 0;
#endif

			{
				unsigned int ii = 0;
				unsigned int out[4] = { _IRQ_MAX, _IRQ_MAX,
					_IRQ_MAX, _IRQ_MAX };

				if ((pstRTBuf_FrmB->ring_buf[
					_imgo_].active == MFALSE)
				    && (pstRTBuf_FrmB->ring_buf[
				    _img2o_].active == MFALSE))
					out[0] = _IRQ;

				for (ii = 0; ii < 4; ii++) {
					if (out[ii] != _IRQ_MAX) {
						sof_count[out[ii]] = 0;
						start_time[out[ii]] = 0;
						avg_frame_time[out[ii]] = 0;
						g1stSof[out[ii]] = MTRUE;
						PrvAddr[out[ii]] = 0;
						g_ISPIntErr[out[ii]] = 0;
#ifdef _rtbc_buf_que_2_0_
						mFwRcnt.bLoadBaseAddr[
						out[ii]] = 0;
						mFwRcnt.curIdx[
						out[ii]] = 0;
						memset((void *)mFwRcnt.
						INC[out[ii]], 0,
						sizeof(unsigned int) *
						ISP_RT_BUF_SIZE);
						mFwRcnt.rdIdx[
						out[ii]] = 0;
#endif
#ifdef T_STAMP_2_0
					if (out[ii] == _IRQ) {
						memset((char *)&
						m_T_STAMP, 0x0,
						sizeof(struct T_STAMP));
						}
#endif

#ifdef _89SERIAL_
						m_P1_RCNT_INC_CNT = 0;
#endif
					}
				}
				for (ii = 0; ii < _rt_dma_max_; ii++) {
					if (pstRTBuf_FrmB->
						ring_buf[ii].active)
						break;
				}

				if (ii == _rt_dma_max_) {
					pstRTBuf_FrmB->dropCnt = 0;
					pstRTBuf_FrmB->state = 0;
				}
			}

#ifdef _MAGIC_NUM_ERR_HANDLING_
			m_LastMNum[rt_dma] = 0;
#endif

#endif
/* spin_unlock_irqrestore(&(IspInfo_FrmB.
 * SpinLockIrq[irqT_Lock]), flags);
 */

			break;
#ifdef _rtbc_buf_que_2_0_
		case ISP_RT_BUF_CTRL_DMA_EN_FRMB:
			{
				unsigned char array[_rt_dma_max_];
/* if(copy_from_user(array, (void*)rt_buf_ctrl.data_ptr,
 * sizeof(UINT8)*_rt_dma_max_) == 0) {
 */
				if (copy_from_user
				    (array, (void __user *)rt_buf_ctrl.pExtend,
				     sizeof(unsigned char) *
				     _rt_dma_max_) == 0) {
					unsigned int z;

					for (z = 0; z < _rt_dma_max_; z++) {
						pstRTBuf_FrmB->ring_buf[z].
						active = array[z];
					if (IspInfo_FrmB.DebugMask &
						ISP_DBG_BUF_CTRL) {
						LOG_INF(
						"[rtbc][DMA_EN]:dma_%d:%d",
						z, array[z]);
					}
					}
				} else {
					LOG_ERR(
					"[rtbc][DMA_EN]:copy_from_user failed");
					Ret = -EFAULT;
				}
			}
			break;
#endif
/* Add this to remove build warning. */
		case ISP_RT_BUF_CTRL_MAX_FRMB:
			/* Do nothing. */
			break;

		}
		/*  */
	} else {
		LOG_ERR("[rtbc]copy_from_user failed");
		Ret = -EFAULT;
	}

	return Ret;
}



#if 0
/*
 * img2o/imgo have hw cq, if lost p1 done,
 * need to add start index inorder to match HW CQ
 * camsv have no hw cq, it will refer to WCNT at SOF.
 * WCNT have no change when no p1_done, so start
 * index no need to change.
 */
static signed int ISP_LostP1Done_ErrHandle(unsigned int dma)
{
	switch (dma) {
	case _imgo_:
	case _img2o_:
		pstRTBuf_FrmB->ring_buf[dma].start++;
		pstRTBuf_FrmB->ring_buf[dma].start =
		pstRTBuf_FrmB->ring_buf[dma].start %
		pstRTBuf_FrmB->ring_buf[dma].total_count;
		break;
	default:
		break;
	}
}
#endif
static signed int ISP_SOF_Buf_Get_FrmB(enum eISPIrq irqT,
unsigned long long sec, unsigned long usec,
bool bDrop, union CQ_RTBC_FBC *pFbc, unsigned int *pCurr_pa)
{
#if defined(_rtbc_use_cq0c_)

	union CQ_RTBC_FBC imgo_fbc;
	union CQ_RTBC_FBC img2o_fbc;
/* (imgo_fbc.Bits.WCNT+imgo_fbc.Bits.FB_NUM-1)%
 * imgo_fbc.Bits.FB_NUM; //[0,1,2,...]
 */
	unsigned int imgo_idx = 0;
/* (img2o_fbc.Bits.WCNT+img2o_fbc.Bits.FB_NUM-1)%
 * img2o_fbc.Bits.FB_NUM; //[0,1,2,...]
 */
	unsigned int img2o_idx = 0;
	unsigned int curr_pa = 0;
	unsigned int ch_imgo = 0;
	unsigned int ch_img2o = 0;
	unsigned int i = 0;
	unsigned int _dma_cur_fw_idx = 0;
	unsigned int _dma_cur_hw_idx = 0;
	unsigned int _working_dma = 0;
	unsigned int out = 0;

	if (irqT == _IRQ) {
		imgo_fbc.Reg_val = pFbc[0].Reg_val;
		img2o_fbc.Reg_val = pFbc[1].Reg_val;
		ch_imgo = _imgo_;
		ch_img2o = _img2o_;
		if (pstRTBuf_FrmB->ring_buf[ch_imgo].active)
			curr_pa = pCurr_pa[0];
		else
			curr_pa = pCurr_pa[1];
		i = _PASS1;
	}

	if (g1stSof[irqT] == MTRUE) {	/* 1st frame of streaming */
#ifdef _89SERIAL_
		pstRTBuf_FrmB->ring_buf[ch_imgo].start =
		    ((imgo_fbc.Bits.FBC_CNT + m_P1_RCNT_INC_CNT) % 3 + 1) - 1;
		pstRTBuf_FrmB->ring_buf[ch_img2o].start =
		    ((img2o_fbc.Bits.FBC_CNT + m_P1_RCNT_INC_CNT) % 3 + 1) - 1;
#else
		pstRTBuf_FrmB->ring_buf[ch_imgo].start
		= imgo_fbc.Bits.WCNT - 1;
		pstRTBuf_FrmB->ring_buf[ch_img2o].start
		= img2o_fbc.Bits.WCNT - 1;
#endif
		g1stSof[irqT] = MFALSE;
	}
	/*  */
#if 0
/* this can't be trusted , because rcnt_in
 * is pull high at sof
 */
	/* No drop */
	if (imgo_fbc.Bits.FB_NUM != imgo_fbc.Bits.FBC_CNT)
		pstRTBuf_FrmB->dropCnt = 0;
	/* dropped */
	else
		pstRTBuf_FrmB->dropCnt = 1;
#else
	pstRTBuf_FrmB->dropCnt = bDrop;
#endif
	/*  */
	/* if(IspInfo_FrmB.DebugMask & ISP_DBG_INT_2) { */
	/* IRQ_LOG_KEEPER(irqT,m_CurrentPPB,_LOG_INF,
	 * "[rtbc]dropCnt(%d)\n",pstRTBuf_FrmB->dropCnt);
	 */
	/* } */
	/* No drop */
	if (pstRTBuf_FrmB->dropCnt == 0) {

		/* verify write buffer */

		/* if(PrvAddr[i] == curr_pa) */
		/* { */
		/* IRQ_LOG_KEEPER(irqT,m_CurrentPPB,_LOG_INF
		 * "PrvAddr:Last(0x%x) == Cur(0x%x)\n",
		 * PrvAddr[i],curr_pa);
		 */
		/* ISP_DumpReg(); */
		/* } */
		PrvAddr[i] = curr_pa;
#ifdef _rtbc_buf_que_2_0_
		imgo_idx = pstRTBuf_FrmB->ring_buf[ch_imgo].start;
		img2o_idx = pstRTBuf_FrmB->ring_buf[ch_img2o].start;
		/* dynamic dma port ctrl */
		if (pstRTBuf_FrmB->ring_buf[ch_imgo].active) {
			_dma_cur_fw_idx = imgo_idx;
			_dma_cur_hw_idx = imgo_fbc.Bits.WCNT - 1;
			_working_dma = ch_imgo;
		} else if (pstRTBuf_FrmB->ring_buf[ch_img2o].active) {
			_dma_cur_fw_idx = img2o_idx;
			_dma_cur_hw_idx = img2o_fbc.Bits.WCNT - 1;
			_working_dma = ch_img2o;
		}
		if (_dma_cur_fw_idx != _dma_cur_hw_idx) {
			IRQ_LOG_KEEPER(irqT, m_CurrentPPB,
			_LOG_INF,
			"dma sof after done %d_%d\n",
			_dma_cur_fw_idx, _dma_cur_hw_idx);
		}
#else
		/* last update buffer index */
		img2o_fbc = img2o_fbc.Bits.WCNT - 1;	/* [0,1,2,...] */
		/* curr_img2o = img2o_fbc.Bits.WCNT - 1; //[0,1,2,...] */
		imgo_idx = img2o_idx;
#endif
/* verify write buffer,once pass1_done lost,
 * WCNT is untrustful.
 */
		if (pstRTBuf_FrmB->ring_buf[_working_dma].
			total_count > ISP_RT_CQ0C_BUF_SIZE) {
			IRQ_LOG_KEEPER(irqT, m_CurrentPPB,
			_LOG_ERR, "buf cnt(%d)\n",
			pstRTBuf_FrmB->ring_buf[
			_working_dma].total_count);
			pstRTBuf_FrmB->ring_buf[_working_dma].
			total_count = ISP_RT_CQ0C_BUF_SIZE;
		}
		/*  */
		if (curr_pa !=
		    pstRTBuf_FrmB->ring_buf[_working_dma].
		    data[_dma_cur_fw_idx].base_pAddr) {
			/*  */
/* LOG_INF("RTBC_DBG6:0x%x_0x%x\n",curr_pa,
 * pstRTBuf_FrmB->ring_buf[ch_img2o].
 * data[img2o_idx].base_pAddr);
 */
			for (i = 0; i < pstRTBuf_FrmB->ring_buf[
			_working_dma].total_count; i++) {
				/*  */
				if (curr_pa ==
				    pstRTBuf_FrmB->ring_buf[_working_dma].
				    data[i].base_pAddr) {
					/*  */
					if (IspInfo_FrmB.DebugMask
						& ISP_DBG_INT_2) {
						IRQ_LOG_KEEPER(irqT,
						m_CurrentPPB, _LOG_INF,
						"[rtbc]curr:old/new(%d/%d)\n",
						 _dma_cur_fw_idx, i);
					}
/*  */
/* mark */
/* indx can't be chged if enque by immediate mode,
 * write baseaddress timing issue.
 */
/* even if not in immediate mode, this case also
 * should not be happened
 */
/* imgo_idx  = i; */
/* rrzo_idx = i; */
/* ignor this log if enque in immediate mode */
					IRQ_LOG_KEEPER(irqT, m_CurrentPPB,
					_LOG_INF,
					"img header err: PA(%x):0x%x_0x%x, idx:0x%x_0x%x\n",
					_working_dma, curr_pa,
					pstRTBuf_FrmB->ring_buf[_working_dma].
					data[_dma_cur_fw_idx].base_pAddr,
					_dma_cur_fw_idx, i);
					break;
				}
			}
		}
/* LOG_INF("RTBC_DBG3:%d_%d\n",imgo_idx,img2o_idx); */
/* LOG_INF("RTBC_DBG7 imgo:%d %d %d\n",pstRTBuf_FrmB->
 * ring_buf[_imgo_].data[0].bFilled,pstRTBuf_FrmB->
 * ring_buf[_imgo_].data[1].bFilled,pstRTBuf_FrmB->
 * ring_buf[_imgo_].data[2].bFilled);
 */
/* LOG_INF("RTBC_DBG7 img2o:%d %d %d\n",pstRTBuf_FrmB->
 * ring_buf[_img2o_].data[0].bFilled,pstRTBuf_FrmB->
 * ring_buf[_img2o_].data[1].bFilled,pstRTBuf_FrmB->
 * ring_buf[_img2o_].data[2].bFilled);
 */
		/*  */
		pstRTBuf_FrmB->ring_buf[ch_imgo].
		data[imgo_idx].timeStampS = sec;
		pstRTBuf_FrmB->ring_buf[ch_imgo].
		data[imgo_idx].timeStampUs = usec;
		pstRTBuf_FrmB->ring_buf[ch_img2o].
		data[img2o_idx].timeStampS = sec;
		pstRTBuf_FrmB->ring_buf[ch_img2o].
		data[img2o_idx].timeStampUs = usec;
		if (IspInfo_FrmB.DebugMask & ISP_DBG_INT_3) {
			static unsigned int m_sec = 0, m_usec;
			unsigned int _tmp =
			    pstRTBuf_FrmB->ring_buf[ch_imgo].
			    data[imgo_idx].timeStampS * 1000000 +
			    pstRTBuf_FrmB->ring_buf[ch_imgo].
			    data[imgo_idx].timeStampUs;
			if (g1stSof[irqT]) {
				m_sec = 0;
				m_usec = 0;
			} else
				IRQ_LOG_KEEPER(irqT, m_CurrentPPB,
				_LOG_INF, " timestamp:%d\n",
				(_tmp - (1000000 * m_sec + m_usec)));
			m_sec = pstRTBuf_FrmB->ring_buf[ch_imgo].
			data[imgo_idx].timeStampS;
			m_usec = pstRTBuf_FrmB->ring_buf[ch_imgo].
			data[imgo_idx].timeStampUs;
		}
		if (irqT == _IRQ) {
			unsigned int _tmp = ISP_RD32(TG_REG_ADDR_GRAB_W);

			pstRTBuf_FrmB->ring_buf[ch_imgo].data[
			imgo_idx].image.xsize =
			ISP_RD32(ISP_REG_ADDR_IMGO_XSIZE) & 0x3FFF;
			pstRTBuf_FrmB->ring_buf[ch_imgo].
			data[imgo_idx].image.w =
			((_tmp >> 16) & 0x7FFF) - (_tmp & 0x7FFF);
			_tmp = ISP_RD32(TG_REG_ADDR_GRAB_H);
			pstRTBuf_FrmB->ring_buf[ch_imgo].
			data[imgo_idx].image.h =
			((_tmp >> 16) & 0x1FFF) - (_tmp & 0x1FFF);
			pstRTBuf_FrmB->ring_buf[ch_imgo].
			data[imgo_idx].image.stride =
			ISP_RD32(ISP_REG_ADDR_IMGO_STRIDE) & 0x3FFF;
			pstRTBuf_FrmB->ring_buf[ch_imgo].
			data[imgo_idx].image.fmt =
			(ISP_RD32(ISP_REG_ADDR_FMT_SEL_P1) & 0xF000) >> 12;
			pstRTBuf_FrmB->ring_buf[ch_imgo].
			data[imgo_idx].image.pxl_id =
			(ISP_RD32(ISP_REG_ADDR_PIX_ID) & 0x03);
			pstRTBuf_FrmB->ring_buf[ch_imgo].
			data[imgo_idx].image.m_num_0 =
			ISP_RD32(ISP_REG_ADDR_TG_MAGIC_0);
			pstRTBuf_FrmB->ring_buf[ch_imgo].
			data[imgo_idx].image.frm_cnt =
			(ISP_RD32(ISP_REG_ADDR_TG_INTER_ST) & 0x00FF0000) >> 16;
			pstRTBuf_FrmB->ring_buf[ch_imgo].
			data[imgo_idx].dmaoCrop.x =
			ISP_RD32(ISP_REG_ADDR_IMGO_CROP) & 0x3fff;
			pstRTBuf_FrmB->ring_buf[ch_imgo].
			data[imgo_idx].dmaoCrop.y =
			(ISP_RD32(ISP_REG_ADDR_IMGO_CROP) >> 16) & 0x1fff;
			pstRTBuf_FrmB->ring_buf[ch_imgo].
			data[imgo_idx].dmaoCrop.w =
			(ISP_RD32(ISP_REG_ADDR_IMGO_XSIZE) & 0x3FFF) + 1;
			pstRTBuf_FrmB->ring_buf[ch_imgo].
			data[imgo_idx].dmaoCrop.h =
			(ISP_RD32(ISP_REG_ADDR_IMGO_YSIZE) & 0x1FFF) + 1;
/* pstRTBuf_FrmB->ring_buf[_imgo_].data[imgo_idx].image.wbn; */
/* pstRTBuf_FrmB->ring_buf[_imgo_].data[imgo_idx].image.ob; */
/* pstRTBuf_FrmB->ring_buf[_imgo_].data[imgo_idx].image.lsc; */
/* pstRTBuf_FrmB->ring_buf[_imgo_].data[imgo_idx].image.rpg; */
/*  */
			pstRTBuf_FrmB->ring_buf[ch_img2o].
			data[img2o_idx].image.xsize =
			ISP_RD32(ISP_REG_ADDR_IMG2O_XSIZE) & 0x3FFF;
			pstRTBuf_FrmB->ring_buf[ch_img2o].
			data[img2o_idx].image.w =
			pstRTBuf_FrmB->ring_buf[ch_imgo].
			data[imgo_idx].image.w;
			pstRTBuf_FrmB->ring_buf[ch_img2o].
			data[img2o_idx].image.h =
			(ISP_RD32(ISP_REG_ADDR_IMG2O_YSIZE) & 0x1FFF) + 1;
			pstRTBuf_FrmB->ring_buf[ch_img2o].
			data[img2o_idx].image.stride =
			ISP_RD32(ISP_REG_ADDR_IMG2O_STRIDE) & 0x3FFF;
			pstRTBuf_FrmB->ring_buf[ch_img2o].
			data[img2o_idx].image.fmt =
			(ISP_RD32(ISP_REG_ADDR_FMT_SEL_P1) & 0xF000) >> 12;
			pstRTBuf_FrmB->ring_buf[ch_img2o].
			data[img2o_idx].image.pxl_id =
			pstRTBuf_FrmB->ring_buf[ch_imgo].
			data[imgo_idx].image.pxl_id;
			pstRTBuf_FrmB->ring_buf[ch_img2o].
			data[img2o_idx].image.m_num_0 =
			pstRTBuf_FrmB->ring_buf[ch_imgo].
			data[imgo_idx].image.m_num_0;
			pstRTBuf_FrmB->ring_buf[ch_img2o].
			data[img2o_idx].image.frm_cnt =
			pstRTBuf_FrmB->ring_buf[ch_imgo].
			data[imgo_idx].image.frm_cnt;
			pstRTBuf_FrmB->ring_buf[ch_img2o].
			data[img2o_idx].HrzInfo.srcX = 0;
			pstRTBuf_FrmB->ring_buf[ch_img2o].
			data[img2o_idx].HrzInfo.srcY = 0;
			_tmp = ISP_RD32(TG_REG_ADDR_GRAB_W);
			pstRTBuf_FrmB->ring_buf[ch_img2o].
			data[img2o_idx].HrzInfo.srcW =
			((_tmp >> 16) & 0x7FFF) - (_tmp & 0x7FFF);
			_tmp = ISP_RD32(TG_REG_ADDR_GRAB_H);
			pstRTBuf_FrmB->ring_buf[ch_img2o].
			data[img2o_idx].HrzInfo.srcH =
			((_tmp >> 16) & 0x1FFF) - (_tmp & 0x1FFF);

			_tmp = ISP_RD32(ISP_REG_ADDR_HRZ_OUT_IMG);
			pstRTBuf_FrmB->ring_buf[ch_img2o].
			data[img2o_idx].HrzInfo.dstW =
			_tmp & 0x1FFF;
			pstRTBuf_FrmB->ring_buf[ch_img2o].
			data[img2o_idx].HrzInfo.dstH =
			pstRTBuf_FrmB->ring_buf[ch_img2o].
			data[img2o_idx].HrzInfo.srcH;
			pstRTBuf_FrmB->ring_buf[ch_img2o].
			data[img2o_idx].dmaoCrop.x =
			ISP_RD32(ISP_REG_ADDR_IMG2O_CROP) & 0x3fff;
			pstRTBuf_FrmB->ring_buf[ch_img2o].
			data[img2o_idx].dmaoCrop.y =
			(ISP_RD32(ISP_REG_ADDR_IMG2O_CROP) >> 16) & 0x1fff;
			pstRTBuf_FrmB->ring_buf[ch_img2o].
			data[img2o_idx].dmaoCrop.w =
			(ISP_RD32(ISP_REG_ADDR_IMG2O_XSIZE) & 0x3FFF) + 1;
			pstRTBuf_FrmB->ring_buf[ch_img2o].
			data[img2o_idx].dmaoCrop.h =
			(ISP_RD32(ISP_REG_ADDR_IMG2O_YSIZE) & 0x1FFF) + 1;

			/*  */

#if 0
#ifdef _MAGIC_NUM_ERR_HANDLING_
			LOG_ERR(
			"[rtbc][sof0][m_num]:fc(0x%x),m0(0x%x)",
			pstRTBuf_FrmB->ring_buf[ch_imgo].
			data[imgo_idx].image.frm_cnt,
			pstRTBuf_FrmB->ring_buf[ch_imgo].
			data[imgo_idx].image.m_num_0);
#endif

			if (IspInfo_FrmB.DebugMask & ISP_DBG_INT_2) {
				IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF,
				"[rtbc]TStamp(%d.%06d),curr(%d),pa(0x%x/0x%x),cq0c(0x%x)\n",
				pstRTBuf_FrmB->ring_buf[ch_imgo].
				data[imgo_idx].timeStampS,
				pstRTBuf_FrmB->ring_buf[ch_imgo].
				data[imgo_idx].timeStampUs,
				imgo_idx,
				pstRTBuf_FrmB->ring_buf[ch_imgo].
				data[imgo_idx].base_pAddr,
				pstRTBuf_FrmB->ring_buf[ch_img2o].
				data[img2o_idx].base_pAddr,
				ISP_RD32(ISP_ADDR + 0xB4));
			}
#endif
		} else {
			LOG_ERR("WE DO NOT SUPPORT THIS IRQ\n");
		}
		/*  */
	}
	/* frame time profile */
	DMA_TRANS(ch_imgo, out);
	if (start_time[out] == 0) {
		start_time[out] = sec * 1000000 + usec;	/* us */
	} else {		/* calc once per senond */
		if (avg_frame_time[out]) {
			avg_frame_time[out] += (sec * 1000000 +
			usec) - avg_frame_time[out];
			avg_frame_time[out] = avg_frame_time[out] >> 1;
		} else {
			avg_frame_time[out] = (sec * 1000000 +
			usec) - start_time[out];
		}
	}

	sof_count[out]++;
	if (sof_count[out] > 255) {	/* for match vsync cnt */
		sof_count[out] -= 256;
	}
	pstRTBuf_FrmB->state = ISP_RTBC_STATE_SOF;
#else
#ifdef _rtbc_buf_que_2_0_
#error "isp kernel define condition is conflicted"
#endif
#endif

	return 0;
}				/*  */

static signed int ISP_DONE_Buf_Time_FrmB(enum eISPIrq irqT,
unsigned long long sec, unsigned long usec,
union CQ_RTBC_FBC *pFbc)
{
	int i, k;
	unsigned int i_dma;
	unsigned int curr;
	/* unsigned int reg_fbc; */
	/* unsigned int reg_val = 0; */
	unsigned int ch_imgo;
	unsigned int ch_img2o = 0;
	union CQ_RTBC_FBC imgo_fbc;
	union CQ_RTBC_FBC img2o_fbc;
	union CQ_RTBC_FBC _dma_cur_fbc;
	unsigned int _working_dma = 0;
#ifdef _rtbc_buf_que_2_0_
	/* for isr cb timing shift err hanlde */
	unsigned int shiftT = 0;
	unsigned int out;
#endif
	if (irqT == _IRQ) {
		ch_imgo = _imgo_;
		ch_img2o = _img2o_;
		imgo_fbc.Reg_val = pFbc[0].Reg_val;
		img2o_fbc.Reg_val = pFbc[1].Reg_val;
	}

#ifdef _rtbc_buf_que_2_0_

	/* dynamic dma port ctrl */
	if (pstRTBuf_FrmB->ring_buf[ch_imgo].active) {
		_dma_cur_fbc = imgo_fbc;
		_working_dma = ch_imgo;
	} else if (pstRTBuf_FrmB->ring_buf[ch_img2o].active) {
		_dma_cur_fbc = img2o_fbc;
		_working_dma = ch_img2o;
	} else {
		LOG_ERR(
		"non-supported dma port (%d/%d)\n",
		pstRTBuf_FrmB->ring_buf[ch_imgo].active,
		pstRTBuf_FrmB->ring_buf[ch_img2o].active);
		return 0;
	}
#ifdef _89SERIAL_
	if (_dma_cur_fbc.Bits.WCNT > 0) {
		unsigned int _sw_wcnt = ((_dma_cur_fbc.Bits.FBC_CNT
		+ m_P1_RCNT_INC_CNT) % 3 + 1);

		if (_sw_wcnt > (pstRTBuf_FrmB->ring_buf[
			_working_dma].start + 2)) {
			shiftT = _sw_wcnt - pstRTBuf_FrmB->ring_buf[
			_working_dma].start - 2;
			if (IspInfo_FrmB.DebugMask & ISP_DBG_INT_2) {
				IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF,
				"[rtbc] WCNT(%d)>start+2(%d),shiftT(%d)\n",
				_sw_wcnt, pstRTBuf_FrmB->ring_buf[
				_working_dma].start + 2,
				shiftT);
			}
			if (shiftT > 0)
				IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF,
				"[rtbc%d]:alert(%d,%d)\n", irqT,
				pstRTBuf_FrmB->ring_buf[_working_dma].start,
				_sw_wcnt);
		} else if (_sw_wcnt < (pstRTBuf_FrmB->
			ring_buf[_working_dma].start + 2)) {
			shiftT =
			    _sw_wcnt + _dma_cur_fbc.Bits.FB_NUM -
			    (pstRTBuf_FrmB->ring_buf[_working_dma].start + 2);
			if (IspInfo_FrmB.DebugMask & ISP_DBG_INT_2) {
				IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF,
				"[rtbc] WCNT(%d)<start+2(%d),shiftT(%d)\n",
				_sw_wcnt, pstRTBuf_FrmB->ring_buf[
				_working_dma].start + 2,
				shiftT);
			}
			if (shiftT >= _dma_cur_fbc.Bits.FB_NUM) {
				LOG_ERR(
				"err shiftT = (%d,%d ,%d)\n",
				_sw_wcnt,
				_dma_cur_fbc.Bits.FB_NUM,
				pstRTBuf_FrmB->ring_buf[
				_working_dma].start);
				shiftT =
				    (_dma_cur_fbc.Bits.
				     FB_NUM ? (_dma_cur_fbc.Bits.FB_NUM -
					       1) : (_dma_cur_fbc.Bits.FB_NUM));
			}
		} else {
		}
/* _dma_cur_fbc.Bits.WCNT == (pstRTBuf_FrmB->
 * ring_buf[_working_dma].start + 2)
 */
	}
#else
	/* isr cb timing shift err handle */
	if (_dma_cur_fbc.Bits.WCNT > 0) {
		if (_dma_cur_fbc.Bits.WCNT > (pstRTBuf_FrmB->
			ring_buf[_working_dma].start + 2)) {
			shiftT = _dma_cur_fbc.Bits.WCNT -
			pstRTBuf_FrmB->ring_buf[_working_dma].start -
			    2;
			if (IspInfo_FrmB.DebugMask & ISP_DBG_INT_2) {
				IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF,
				"[rtbc] WCNT(%d)>start+2(%d),shiftT(%d)\n",
				_dma_cur_fbc.Bits.WCNT, pstRTBuf_FrmB->
				ring_buf[_working_dma].start + 2,
				shiftT);
			}
			if (shiftT > 0)
				IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF,
				"[rtbc%d]:alert(%d,%d)\n", irqT,
				pstRTBuf_FrmB->ring_buf[_working_dma].start,
				_dma_cur_fbc.Bits.WCNT);
		} else if (_dma_cur_fbc.Bits.WCNT <
			   (pstRTBuf_FrmB->ring_buf[_working_dma].start + 2)) {
			shiftT =
			    _dma_cur_fbc.Bits.WCNT + _dma_cur_fbc.Bits.FB_NUM -
			    (pstRTBuf_FrmB->ring_buf[_working_dma].start + 2);
			if (IspInfo_FrmB.DebugMask & ISP_DBG_INT_2) {
				IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF,
				"[rtbc] WCNT(%d)<start+2(%d),shiftT(%d)\n",
				_dma_cur_fbc.Bits.WCNT,
				pstRTBuf_FrmB->ring_buf[
				_working_dma].start + 2,
				shiftT);
			}
			if (shiftT >= _dma_cur_fbc.Bits.FB_NUM) {
				LOG_ERR(
				"err shiftT = (%d,%d ,%d)\n",
				_dma_cur_fbc.Bits.WCNT,
				_dma_cur_fbc.Bits.FB_NUM,
				pstRTBuf_FrmB->ring_buf[
				_working_dma].start);
				shiftT =
				    (_dma_cur_fbc.Bits.
				     FB_NUM ? (_dma_cur_fbc.Bits.FB_NUM -
					       1) : (_dma_cur_fbc.Bits.FB_NUM));
			}
		} else {
		}
/* _dma_cur_fbc.Bits.WCNT == (pstRTBuf_FrmB->
 * ring_buf[_working_dma].start + 2)
 */
	}
#endif
#endif


#ifdef _rtbc_buf_que_2_0_
	for (k = 0; k < shiftT + 1; k++)	/* patch missed isr_done */
#endif
	{
		for (i = 0; i <= 1; i++) {
			/*  */
			if (i == 0) {
				i_dma = ch_imgo;
				/* reg_fbc = ch_imgo_fbc; */
			} else {
				i_dma = ch_img2o;
				/* reg_fbc = ch_img2o_fbc; */
			}
			/*  */
			if (pstRTBuf_FrmB->ring_buf[i_dma].empty_count == 0) {
				/*  */
				if (IspInfo_FrmB.DebugMask & ISP_DBG_INT_2) {
					IRQ_LOG_KEEPER(irqT,
					m_CurrentPPB, _LOG_INF,
					"[rtbc][DONE]:dma(%d)buf num empty,start(%d)\n",
					i_dma, pstRTBuf_FrmB->
					ring_buf[i_dma].start);
				}
				/*  */
				continue;
			}
#if 0
/* once if buffer put into queue
 * between SOF and ISP_DONE.
 */
			if (pstRTBuf_FrmB->ring_buf[i_dma].active == MFALSE) {
				/*  */
				if (IspInfo_FrmB.DebugMask & ISP_DBG_INT_2)
					LOG_DBG(
					"[rtbc][DONE] ERROR: missing SOF ");
				/*  */
				continue;
			}
#endif
			curr = pstRTBuf_FrmB->ring_buf[i_dma].start;
			/* unsigned int loopCount = 0; */
			while (1) {
				if (IspInfo_FrmB.DebugMask & ISP_DBG_INT_2) {
					IRQ_LOG_KEEPER(irqT,
					m_CurrentPPB, _LOG_INF,
					"i_dma(%d),curr(%d),bFilled(%d)\n",
					i_dma, curr,
					pstRTBuf_FrmB->ring_buf[
					i_dma].data[curr].bFilled);
				}
				/*  */
				if (pstRTBuf_FrmB->ring_buf[
					i_dma].data[curr].bFilled ==
				    ISP_RTBC_BUF_EMPTY) {
					if (IspInfo_FrmB.DebugMask &
						ISP_DBG_INT_2)
						IRQ_LOG_KEEPER(irqT,
						m_CurrentPPB, _LOG_INF,
						"[rtbc][DONE]:dma_%d,fill buffer,cur_%d\n",
						i_dma, curr);
					pstRTBuf_FrmB->ring_buf[i_dma].
					data[curr].bFilled =
					    ISP_RTBC_BUF_FILLED;
					/* start + 1 */
					pstRTBuf_FrmB->ring_buf[i_dma].start =
					(curr + 1) % pstRTBuf_FrmB->
					ring_buf[i_dma].total_count;
					pstRTBuf_FrmB->ring_buf[
					i_dma].empty_count--;
					/*  */
					if (g1stSof[irqT] == MTRUE) {
						LOG_ERR(
						"Done&&Sof receive at the same time in 1st f(%d)\n",
						i_dma);
					}
					break;
				}
/* (IspInfo_FrmB.DebugMask & ISP_DBG_INT_2) { */
					if (1) {
						LOG_ERR(
						"dma_%d,cur_%d,bFilled_%d != EMPTY(%d %d %d)\n",
						i_dma, curr,
						pstRTBuf_FrmB->ring_buf[i_dma].
						data[curr].bFilled,
						pstRTBuf_FrmB->ring_buf[i_dma].
						data[0].bFilled,
						pstRTBuf_FrmB->ring_buf[i_dma].
						data[1].bFilled,
						pstRTBuf_FrmB->ring_buf[i_dma].
						data[2].bFilled);

					}
					/* start + 1 */
/* pstRTBuf_FrmB->ring_buf[i_dma].start = (curr+1)%
 * pstRTBuf_FrmB->ring_buf[i_dma].total_count;
 */
					break;
#if 0
				loopCount++;
				if (loopCount > pstRTBuf_FrmB->
					ring_buf[i_dma].total_count) {
					LOG_ERR(
					"Can't find empty dma(%d) buf in total_count(%d)",
					i_dma, pstRTBuf_FrmB->
					ring_buf[i_dma].total_count);
					break;
				}
#endif
			}
#if 0
			/* enable fbc to stall DMA */
			if (pstRTBuf_FrmB->ring_buf[i_dma].empty_count == 0) {
				if (_imgo_ == i_dma) {
					reg_val = ISP_RD32(
					ISP_REG_ADDR_IMGO_FBC);
					reg_val |= 0x4000;
/* ISP_WR32(ISP_REG_ADDR_IMGO_FBC,reg_val); */
				} else {
					reg_val = ISP_RD32(
					ISP_REG_ADDR_IMGO_D_FBC);
					reg_val |= 0x4000;
/* ISP_WR32(ISP_REG_ADDR_IMGO_D_FBC,reg_val); */
				}
				/*  */
				if (IspInfo_FrmB.DebugMask & ISP_DBG_INT_2) {
					IRQ_LOG_KEEPER(irqT,
					m_CurrentPPB, _LOG_INF,
					"[rtbc][DONE]:dma(%d),en fbc(0x%x) stalled DMA out",
					i_dma, ISP_RD32(reg_fbc));
				}
			}
#endif
			if (IspInfo_FrmB.DebugMask & ISP_DBG_INT_2) {
				IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF,
				"[rtbc][DONE]:dma(%d),start(%d),empty(%d)\n",
				i_dma,
				pstRTBuf_FrmB->ring_buf[i_dma].start,
				pstRTBuf_FrmB->ring_buf[i_dma].empty_count);
			}
#if 0				/* time stamp move to sof */
			/*  */
			pstRTBuf_FrmB->ring_buf[i_dma].
			data[curr].timeStampS = sec;
			pstRTBuf_FrmB->ring_buf[i_dma].
			data[curr].timeStampUs = usec;
			/*  */
			if (IspInfo_FrmB.DebugMask & ISP_DBG_INT_2) {
				LOG_DBG(
				"[rtbc][DONE]:dma(%d),curr(%d),sec(%lld),usec(%ld) ",
				i_dma, curr, sec, usec);
			}
#endif
			/*  */
			DMA_TRANS(i_dma, out);
			pstRTBuf_FrmB->ring_buf[i_dma].img_cnt = sof_count[out];
		}
	}

	if (pstRTBuf_FrmB->ring_buf[ch_imgo].active
		&& pstRTBuf_FrmB->ring_buf[ch_img2o].active) {
		if (pstRTBuf_FrmB->ring_buf[ch_imgo].start !=
		    pstRTBuf_FrmB->ring_buf[ch_img2o].start) {
			LOG_ERR(
			"start idx mismatch %d_%d(%d %d %d,%d %d %d)",
			pstRTBuf_FrmB->ring_buf[ch_imgo].start,
			pstRTBuf_FrmB->ring_buf[ch_img2o].start,
			pstRTBuf_FrmB->ring_buf[ch_imgo].
			data[0].bFilled,
			pstRTBuf_FrmB->ring_buf[ch_imgo].
			data[1].bFilled,
			pstRTBuf_FrmB->ring_buf[ch_imgo].
			data[2].bFilled,
			pstRTBuf_FrmB->ring_buf[ch_img2o].
			data[0].bFilled,
			pstRTBuf_FrmB->ring_buf[ch_img2o].
			data[1].bFilled,
			pstRTBuf_FrmB->ring_buf[ch_img2o].
			data[2].bFilled);
		}
	}
/* LOG_INF("RTBC_DBG7 imgo(buf cnt): %d %d %d\n",
 * pstRTBuf_FrmB->ring_buf[_imgo_].data[0].bFilled,
 * pstRTBuf_FrmB->ring_buf[_imgo_].data[1].bFilled,
 * pstRTBuf_FrmB->ring_buf[_imgo_].data[2].bFilled);
 */
/* LOG_INF("RTBC_DBG7 img2o(buf cnt): %d %d %d\n",
 * pstRTBuf_FrmB->ring_buf[_img2o_].data[0].bFilled,
 * pstRTBuf_FrmB->ring_buf[_img2o_].data[1].bFilled,
 * pstRTBuf_FrmB->ring_buf[_img2o_].data[2].bFilled);
 */
#if 0
	if (IspInfo_FrmB.DebugMask & ISP_DBG_INT_2)
		IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF, "-:[rtbc]");
#endif
	/*  */
	pstRTBuf_FrmB->state = ISP_RTBC_STATE_DONE;
	/* spin_unlock_irqrestore(&
	 * (IspInfo_FrmB.SpinLockRTBC),g_Flash_SpinLock);
	 */

	return 0;
}

/*************************************************
 *
 *************************************************/
static signed int ISP_ED_BufQue_Update_GPtr(int listTag)
{
	signed int ret = 0;
	signed int tmpIdx = 0;
	signed int cnt = 0;
	bool stop = false;
	int i = 0;
	enum ISP_ED_BUF_STATE_ENUM gPtrSts = ISP_ED_BUF_STATE_NONE;

	switch (listTag) {
	case P2_EDBUF_RLIST_TAG:
		/* [1] check global pointer current sts */
		gPtrSts = P2_EDBUF_RingList[P2_EDBUF_RList_CurBufIdx].bufSts;

/* //////////////////////////////////////////// */
/* Assume we have the buffer list in the following situation */
/* ++++++         ++++++         ++++++ */
/* +  vss +         +  prv +         +  prv + */
/* ++++++         ++++++         ++++++ */
/* not deque         erased           enqued */
/* done */
/*  */
/* if the vss deque is done, we should update the
 * CurBufIdx to the next "enqued" buffer node
 * instead of just moving to the next buffer node
 */
/* //////////////////////////////////////////// */
/* [2]calculate traverse count needed */
		if (P2_EDBUF_RList_FirstBufIdx <= P2_EDBUF_RList_LastBufIdx) {
			cnt = P2_EDBUF_RList_LastBufIdx
			- P2_EDBUF_RList_FirstBufIdx;
		} else {
			cnt = _MAX_SUPPORT_P2_FRAME_NUM_
			-P2_EDBUF_RList_FirstBufIdx;
			cnt += P2_EDBUF_RList_LastBufIdx;
		}

		/* [3] update */
		tmpIdx = P2_EDBUF_RList_CurBufIdx;
		switch (gPtrSts) {
		case ISP_ED_BUF_STATE_ENQUE:
			P2_EDBUF_RingList[P2_EDBUF_RList_CurBufIdx].bufSts =
			    ISP_ED_BUF_STATE_RUNNING;
			break;
		case ISP_ED_BUF_STATE_WAIT_DEQUE_FAIL:
		case ISP_ED_BUF_STATE_DEQUE_SUCCESS:
		case ISP_ED_BUF_STATE_DEQUE_FAIL:
			do {	/* to find the newest cur index */
				tmpIdx = (tmpIdx + 1) %
				_MAX_SUPPORT_P2_FRAME_NUM_;
				switch (P2_EDBUF_RingList[tmpIdx].bufSts) {
				case ISP_ED_BUF_STATE_ENQUE:
				case ISP_ED_BUF_STATE_RUNNING:
					P2_EDBUF_RingList[tmpIdx].bufSts =
					ISP_ED_BUF_STATE_RUNNING;
					P2_EDBUF_RList_CurBufIdx = tmpIdx;
					stop = true;
					break;
				case ISP_ED_BUF_STATE_WAIT_DEQUE_FAIL:
				case ISP_ED_BUF_STATE_DEQUE_SUCCESS:
				case ISP_ED_BUF_STATE_DEQUE_FAIL:
				case ISP_ED_BUF_STATE_NONE:
				default:
					break;
				}
				i++;
			} while ((i < cnt) && (!stop));
/* //////////////////////////////////////////// */
/* Assume we have the buffer list in the following situation */
/* ++++++         ++++++         ++++++ */
/* +  vss +         +  prv +         +  prv + */
/* ++++++         ++++++         ++++++ */
/* not deque         erased           erased */
/* done */
/*  */
/* all the buffer node are deque done in the current
 * moment, should update current index to the last node
 */
/* if the vss deque is done, we should update the
 * CurBufIdx to the last buffer node
 */
/* ///////////////////////////////////////////// */
			if ((!stop) &&
				(i == (cnt))) {
				P2_EDBUF_RList_CurBufIdx =
				P2_EDBUF_RList_LastBufIdx;
			}
			break;
		case ISP_ED_BUF_STATE_NONE:
		case ISP_ED_BUF_STATE_RUNNING:
		default:
			break;
		}
		break;
	case P2_EDBUF_MLIST_TAG:
	default:
		LOG_ERR("Wrong List tag(%d)\n", listTag);
		break;
	}
	return ret;
}

/*************************************************
 *
 *************************************************/
#if 0				/* disable it to avoid build warning */
static signed int ISP_ED_BufQue_Set_FailNode(
ISP_ED_BUF_STATE_ENUM failType, signed int idx)
{
	signed int ret = 0;

	spin_lock(&(SpinLockEDBufQueList));
	/* [1]set fail type */
	P2_EDBUF_RingList[idx].bufSts = failType;

	/* [2]update global pointer */
	ISP_ED_BufQue_Update_GPtr(P2_EDBUF_RLIST_TAG);
	spin_unlock(&(SpinLockEDBufQueList));
	return ret;
}
#endif

/*************************************************
 *
 *************************************************/
static signed int ISP_ED_BufQue_Erase(signed int idx, int listTag)
{
	signed int ret = -1;
	bool stop = false;
	int i = 0;
	signed int cnt = 0;
	int tmpIdx = 0;

	switch (listTag) {
	case P2_EDBUF_MLIST_TAG:
		tmpIdx = P2_EDBUF_MList_FirstBufIdx;
		/* [1] clear buffer status */
		P2_EDBUF_MgrList[idx].processID = 0x0;
		P2_EDBUF_MgrList[idx].callerID = 0x0;
		P2_EDBUF_MgrList[idx].p2dupCQIdx = -1;
		P2_EDBUF_MgrList[idx].dequedNum = 0;
		/* [2] update first index */
		if (P2_EDBUF_MgrList[tmpIdx].p2dupCQIdx == -1) {
/* traverse count needed, cuz user may erase the element
 * but not the one at first idx(pip or vss scenario)
 */
			if (P2_EDBUF_MList_FirstBufIdx
				<= P2_EDBUF_MList_LastBufIdx) {
				cnt = P2_EDBUF_MList_LastBufIdx
				- P2_EDBUF_MList_FirstBufIdx;
			} else {
				cnt = _MAX_SUPPORT_P2_PACKAGE_NUM_
				-P2_EDBUF_MList_FirstBufIdx;
				cnt += P2_EDBUF_MList_LastBufIdx;
			}
			do {	/* to find the newest first lindex */
				tmpIdx = (tmpIdx + 1) %
				_MAX_SUPPORT_P2_PACKAGE_NUM_;
				switch (P2_EDBUF_MgrList[tmpIdx].p2dupCQIdx) {
				case (-1):
					break;
				default:
					stop = true;
					P2_EDBUF_MList_FirstBufIdx = tmpIdx;
					break;
				}
				i++;
			} while ((i < cnt) && (!stop));
/* current last erased element in list is
 * the one firstBufindex point at
 */
/* and all the buffer node are deque done in the
 * current moment, should update first index to
 * the last node
 */
			if ((!stop) && (i == cnt))
				P2_EDBUF_MList_FirstBufIdx =
				P2_EDBUF_MList_LastBufIdx;
		}
		break;
	case P2_EDBUF_RLIST_TAG:
		tmpIdx = P2_EDBUF_RList_FirstBufIdx;
		/* [1] clear buffer status */
		P2_EDBUF_RingList[idx].processID = 0x0;
		P2_EDBUF_RingList[idx].callerID = 0x0;
		P2_EDBUF_RingList[idx].p2dupCQIdx = -1;
		P2_EDBUF_RingList[idx].bufSts = ISP_ED_BUF_STATE_NONE;
		EDBufQueRemainNodeCnt--;
		/* [2]update first index */
		if (P2_EDBUF_RingList[tmpIdx].bufSts == ISP_ED_BUF_STATE_NONE) {
/* traverse count needed, cuz user may erase
 * the element but not the one at first idx
 */
			if (P2_EDBUF_RList_FirstBufIdx
				<= P2_EDBUF_RList_LastBufIdx) {
				cnt = P2_EDBUF_RList_LastBufIdx
				- P2_EDBUF_RList_FirstBufIdx;
			} else {
				cnt = _MAX_SUPPORT_P2_FRAME_NUM_
				-P2_EDBUF_RList_FirstBufIdx;
				cnt += P2_EDBUF_RList_LastBufIdx;
			}
			/* to find the newest first lindex */
			do {
				tmpIdx = (tmpIdx + 1) %
				_MAX_SUPPORT_P2_FRAME_NUM_;
				switch (P2_EDBUF_RingList[tmpIdx].bufSts) {
				case ISP_ED_BUF_STATE_ENQUE:
				case ISP_ED_BUF_STATE_RUNNING:
				case ISP_ED_BUF_STATE_WAIT_DEQUE_FAIL:
				case ISP_ED_BUF_STATE_DEQUE_SUCCESS:
				case ISP_ED_BUF_STATE_DEQUE_FAIL:
					stop = true;
					P2_EDBUF_RList_FirstBufIdx = tmpIdx;
					break;
				case ISP_ED_BUF_STATE_NONE:
				default:
					break;
				}
				i++;
			} while ((i < cnt) && (!stop));
/* current last erased element in list is
 * the one firstBufindex point at
 */
/* and all the buffer node are deque done
 * in the current moment, should update first
 * index to the last node
 */
			if ((!stop) && (i == (cnt)))
				P2_EDBUF_RList_FirstBufIdx =
				P2_EDBUF_RList_LastBufIdx;
		}
		break;
	default:
		break;
	}
	return ret;
}

/*************************************************
 * get first matched buffer
 *************************************************/
static signed int ISP_ED_BufQue_Get_FirstMatBuf(
struct ISP_ED_BUFQUE_STRUCT_FRMB param, int ListTag, int type)
{
	signed int idx = -1;
	signed int i = 0;

	switch (ListTag) {
	case P2_EDBUF_MLIST_TAG:
/* for user wait frame, do not care p2 dupCq index,
 * first enqued p2 dupCQ first out
 */
		if (type == 0) {
			if (P2_EDBUF_MList_FirstBufIdx <=
				P2_EDBUF_MList_LastBufIdx) {
				for (i = P2_EDBUF_MList_FirstBufIdx;
				i <= P2_EDBUF_MList_LastBufIdx;
				     i++) {
					if ((P2_EDBUF_MgrList[i].processID
						== param.processID) &&
						(P2_EDBUF_MgrList[i].callerID ==
						param.callerID)) {
						idx = i;
						break;
					}
				}
			} else {
				for (i = P2_EDBUF_MList_FirstBufIdx;
				     i < _MAX_SUPPORT_P2_PACKAGE_NUM_; i++) {
					if ((P2_EDBUF_MgrList[i].processID
						== param.processID) &&
						(P2_EDBUF_MgrList[i].callerID
					  == param.callerID)) {
						idx = i;
						break;
					}
				}
				/*get in the first for loop */
				if (idx != -1) {
				} else {
					for (i = 0; i <=
					P2_EDBUF_MList_LastBufIdx;
					i++) {
					if ((P2_EDBUF_MgrList[i].
						processID ==
						param.processID) &&
						(P2_EDBUF_MgrList[i].callerID
						== param.callerID)) {
						idx = i;
						break;
						}
					}
				}
			}
		} else {	/* for buffer node deque done notify */
			if (P2_EDBUF_MList_FirstBufIdx
				<= P2_EDBUF_MList_LastBufIdx) {
				for (i = P2_EDBUF_MList_FirstBufIdx;
				i <= P2_EDBUF_MList_LastBufIdx;
				     i++) {
					if ((P2_EDBUF_MgrList[i].processID ==
						param.processID) &&
						(P2_EDBUF_MgrList[i].callerID ==
						param.callerID) &&
						(P2_EDBUF_MgrList[i].
						p2dupCQIdx ==
						param.p2dupCQIdx) &&
						(P2_EDBUF_MgrList[i].dequedNum <
						P2_Support_BurstQNum)) {
/* avoid race that dupCQ_1 of buffer2 enqued while
 * dupCQ_1 of buffer1 have beend deque done but
 * not been erased yet
 */
						idx = i;
						break;
					}
				}
			} else {
				for (i = P2_EDBUF_MList_FirstBufIdx;
				     i < _MAX_SUPPORT_P2_PACKAGE_NUM_; i++) {
					if ((P2_EDBUF_MgrList[i].processID
						== param.processID)
					    && (P2_EDBUF_MgrList[i].callerID
					    == param.callerID)
					    && (P2_EDBUF_MgrList[i].p2dupCQIdx
					    == param.p2dupCQIdx)
					    && (P2_EDBUF_MgrList[i].dequedNum <
						P2_Support_BurstQNum)) {
						idx = i;
						break;
					}
				}
				/*get in the first for loop */
				if (idx != -1) {
				} else {
					for (i = 0; i <=
					P2_EDBUF_MList_LastBufIdx; i++) {
					if ((P2_EDBUF_MgrList[i].processID
						== param.processID) &&
						(P2_EDBUF_MgrList[i].
						callerID ==
						param.callerID) &&
						(P2_EDBUF_MgrList[i].
						p2dupCQIdx
						== param.p2dupCQIdx) &&
						(P2_EDBUF_MgrList[i].
						dequedNum <
						P2_Support_BurstQNum)) {
						idx = i;
						break;
						}
					}
				}
			}
		}
		break;
	case P2_EDBUF_RLIST_TAG:
		if (P2_EDBUF_RList_FirstBufIdx <= P2_EDBUF_RList_LastBufIdx) {
			for (i = P2_EDBUF_RList_FirstBufIdx;
			i <= P2_EDBUF_RList_LastBufIdx; i++) {
				if ((P2_EDBUF_RingList[i].processID
					== param.processID)
				    && (P2_EDBUF_RingList[i].callerID
				    == param.callerID)) {
					idx = i;
					break;
				}
			}
		} else {
			for (i = P2_EDBUF_RList_FirstBufIdx;
			i < _MAX_SUPPORT_P2_FRAME_NUM_; i++) {
				if ((P2_EDBUF_RingList[i].processID
					== param.processID) &&
					(P2_EDBUF_RingList[i].callerID
					== param.callerID)) {
					idx = i;
					break;
				}
			}
			if (idx != -1) {	/*get in the first for loop */
			} else {
				for (i = 0; i <=
				P2_EDBUF_RList_LastBufIdx; i++) {
					if ((P2_EDBUF_RingList[i].processID
						== param.processID) &&
						(P2_EDBUF_RingList[i].callerID
						== param.callerID)) {
						idx = i;
						break;
					}
				}
			}
		}
		break;
	default:
		break;
	}
	if (idx == -1) {
		LOG_ERR(
		"Could not find match buffer tag(%d) pid/cid/p2dupCQidx(%d/0x%x/%d)",
		ListTag, param.processID,
		param.callerID, param.p2dupCQIdx);
	}
	return idx;
}

/*************************************************
 *
 *************************************************/
static signed int ISP_ED_BufQue_CTRL_FUNC_FRMB(struct ISP_ED_BUFQUE_STRUCT_FRMB param)
{
	signed int ret = 0;
	int i = 0;
	int idx = -1, idx2 = -1;
	signed int restTime = 0;

	switch (param.ctrl) {
/* signal that a specific buffer is enqueued */
	case ISP_ED_BUFQUE_CTRL_ENQUE_FRAME:
		/* [1] check the ring buffer list is full or not */
		spin_lock(&(SpinLockEDBufQueList));
		if (((P2_EDBUF_MList_LastBufIdx + 1) %
			_MAX_SUPPORT_P2_PACKAGE_NUM_) ==
		  P2_EDBUF_MList_FirstBufIdx &&
		  (P2_EDBUF_MList_LastBufIdx != -1)) {
			LOG_ERR(
			"F/L(%d,%d),(%d,%d), RF/C/L(%d,%d,%d),(%d,%d,%d)",
			P2_EDBUF_MList_FirstBufIdx,
			P2_EDBUF_MList_LastBufIdx,
			P2_EDBUF_MgrList[P2_EDBUF_MList_FirstBufIdx].
			p2dupCQIdx,
			P2_EDBUF_MgrList[P2_EDBUF_MList_LastBufIdx].
			p2dupCQIdx,
			P2_EDBUF_RList_FirstBufIdx,
			P2_EDBUF_RList_CurBufIdx,
			P2_EDBUF_RList_LastBufIdx,
			P2_EDBUF_RingList[P2_EDBUF_RList_FirstBufIdx].
			bufSts,
			P2_EDBUF_RingList[P2_EDBUF_RList_CurBufIdx].
			bufSts,
			P2_EDBUF_RingList[P2_EDBUF_RList_LastBufIdx].
			bufSts);
			spin_unlock(&(SpinLockEDBufQueList));
			LOG_ERR("p2 ring buffer list is full, enque Fail.");
			ret = -EFAULT;
			return ret;
		}
			IRQ_LOG_KEEPER(_CAMSV_D_IRQ, 0, _LOG_DBG,
			"pD(%d_0x%x) MF/L(%d,%d),(%d,%d), RF/C/L(%d,%d,%d),(%d,%d,%d),dCq(%d)/Bq(%d)\n",
			param.processID, param.callerID,
			P2_EDBUF_MList_FirstBufIdx,
			P2_EDBUF_MList_LastBufIdx,
			P2_EDBUF_MgrList[P2_EDBUF_MList_FirstBufIdx].
			p2dupCQIdx,
			P2_EDBUF_MgrList[P2_EDBUF_MList_LastBufIdx].
			p2dupCQIdx,
			P2_EDBUF_RList_FirstBufIdx,
			P2_EDBUF_RList_CurBufIdx,
			P2_EDBUF_RList_LastBufIdx,
			P2_EDBUF_RingList[P2_EDBUF_RList_FirstBufIdx].
			bufSts,
			P2_EDBUF_RingList[P2_EDBUF_RList_CurBufIdx].
			bufSts,
			P2_EDBUF_RingList[P2_EDBUF_RList_LastBufIdx].
			bufSts,
			param.p2dupCQIdx, param.p2burstQIdx);
/* [2] add new element to the last of the list */
			if (P2_EDBUF_RList_FirstBufIdx ==
				P2_EDBUF_RList_LastBufIdx &&
				P2_EDBUF_RingList[P2_EDBUF_RList_FirstBufIdx].
				bufSts == ISP_ED_BUF_STATE_NONE) {
/* all buffer node is empty */
				P2_EDBUF_RList_LastBufIdx =
				    (P2_EDBUF_RList_LastBufIdx + 1) %
				    _MAX_SUPPORT_P2_FRAME_NUM_;
				P2_EDBUF_RList_FirstBufIdx =
				P2_EDBUF_RList_LastBufIdx;
				P2_EDBUF_RList_CurBufIdx =
				P2_EDBUF_RList_LastBufIdx;
			} else if (P2_EDBUF_RList_CurBufIdx ==
				P2_EDBUF_RList_LastBufIdx &&
				P2_EDBUF_RingList[P2_EDBUF_RList_CurBufIdx].
				bufSts == ISP_ED_BUF_STATE_NONE) {
/* first node is not empty, but current/last is empty */
				P2_EDBUF_RList_LastBufIdx =
				    (P2_EDBUF_RList_LastBufIdx + 1) %
				    _MAX_SUPPORT_P2_FRAME_NUM_;
				P2_EDBUF_RList_CurBufIdx =
				P2_EDBUF_RList_LastBufIdx;
			} else {
				P2_EDBUF_RList_LastBufIdx =
				    (P2_EDBUF_RList_LastBufIdx + 1) %
				    _MAX_SUPPORT_P2_FRAME_NUM_;
			}
			P2_EDBUF_RingList[P2_EDBUF_RList_LastBufIdx].
			processID = param.processID;
			P2_EDBUF_RingList[P2_EDBUF_RList_LastBufIdx].
			callerID = param.callerID;
			P2_EDBUF_RingList[P2_EDBUF_RList_LastBufIdx].
			p2dupCQIdx = param.p2dupCQIdx;
			P2_EDBUF_RingList[P2_EDBUF_RList_LastBufIdx].
			bufSts = ISP_ED_BUF_STATE_ENQUE;
			EDBufQueRemainNodeCnt++;

			/* [3] add new buffer package in manager list */
			if (param.p2burstQIdx == 0) {
				/* all managed buffer node is empty */
				if (P2_EDBUF_MList_FirstBufIdx ==
					P2_EDBUF_MList_LastBufIdx &&
					P2_EDBUF_MgrList[
					P2_EDBUF_MList_FirstBufIdx].
					p2dupCQIdx == -1) {
					P2_EDBUF_MList_LastBufIdx =
					    (P2_EDBUF_MList_LastBufIdx +
					     1) % _MAX_SUPPORT_P2_PACKAGE_NUM_;
					P2_EDBUF_MList_FirstBufIdx =
					P2_EDBUF_MList_LastBufIdx;
				} else {
					P2_EDBUF_MList_LastBufIdx =
					    (P2_EDBUF_MList_LastBufIdx +
					     1) % _MAX_SUPPORT_P2_PACKAGE_NUM_;
				}
				P2_EDBUF_MgrList[P2_EDBUF_MList_LastBufIdx].
				processID = param.processID;
				P2_EDBUF_MgrList[P2_EDBUF_MList_LastBufIdx].
				callerID = param.callerID;
				P2_EDBUF_MgrList[P2_EDBUF_MList_LastBufIdx].
				p2dupCQIdx = param.p2dupCQIdx;
				P2_EDBUF_MgrList[P2_EDBUF_MList_LastBufIdx].
				dequedNum = 0;
			}
		/* [4]update global index */
		ISP_ED_BufQue_Update_GPtr(P2_EDBUF_RLIST_TAG);
		spin_unlock(&(SpinLockEDBufQueList));
		IRQ_LOG_PRINTER(_CAMSV_D_IRQ, 0, _LOG_DBG);
		/* [5] wake up thread that wait for deque */
		wake_up_interruptible((wait_queue_head_t *)(
		&WaitQueueHead_EDBuf_WaitDeque));
		break;
/* a dequeue thread is waiting to do dequeue */
	case ISP_ED_BUFQUE_CTRL_WAIT_DEQUE:
/* [1]traverse for finding the buffer which
 * had not beed dequeued of the process
 */
		spin_lock(&(SpinLockEDBufQueList));
		if (P2_EDBUF_RList_FirstBufIdx <= P2_EDBUF_RList_LastBufIdx) {
			for (i = P2_EDBUF_RList_FirstBufIdx;
			i <= P2_EDBUF_RList_LastBufIdx; i++) {
				if ((P2_EDBUF_RingList[i].processID
					== param.processID) &&
					((P2_EDBUF_RingList[i].bufSts ==
					ISP_ED_BUF_STATE_ENQUE) ||
					(P2_EDBUF_RingList[i].bufSts ==
					ISP_ED_BUF_STATE_RUNNING))) {
					idx = i;
					break;
				}
			}
		} else {
			for (i = P2_EDBUF_RList_FirstBufIdx;
			i < _MAX_SUPPORT_P2_FRAME_NUM_; i++) {
				if ((P2_EDBUF_RingList[i].processID
					== param.processID) &&
					((P2_EDBUF_RingList[i].bufSts ==
					ISP_ED_BUF_STATE_ENQUE) ||
					(P2_EDBUF_RingList[i].bufSts ==
					ISP_ED_BUF_STATE_RUNNING))) {
					idx = i;
					break;
				}
			}
			if (idx != -1) {	/*get in the first for loop */
			} else {
				for (i = 0; i <=
				P2_EDBUF_RList_LastBufIdx; i++) {
					if ((P2_EDBUF_RingList[i].processID
					== param.processID) &&
					((P2_EDBUF_RingList[i].bufSts
					== ISP_ED_BUF_STATE_ENQUE) ||
					(P2_EDBUF_RingList[i].bufSts ==
					ISP_ED_BUF_STATE_RUNNING))) {
						idx = i;
						break;
					}
				}
			}
		}
		spin_unlock(&(SpinLockEDBufQueList));
		if (idx == -1) {
			LOG_ERR(
			"Do not find match buffer (pid/cid %d/0x%x) to deque!",
			param.processID, param.callerID);
			ret = -EFAULT;
			return ret;
		}
			restTime = wait_event_interruptible_timeout(*
			((wait_queue_head_t *)(
			&WaitQueueHead_EDBuf_WaitDeque)),
			ISP_GetEDBufQueWaitDequeState(idx),
			ISP_UsToJiffies_FrmB(5000000));	/* 5s */
			if (restTime == 0) {
				LOG_ERR(
				"Wait Deque fail, idx(%d) pID(%d),cID(0x%x)",
				idx,
				param.processID, param.callerID);
				ret = -EFAULT;
			} else {
/* LOG_INF("wakeup and goto deque,rTime(%d),
 * pID(%d)",restTime,param.processID);
 */
			}
		break;
/* signal that a buffer is dequeued(success) */
	case ISP_ED_BUFQUE_CTRL_DEQUE_SUCCESS:
/* signal that a buffer is dequeued(fail) */
	case ISP_ED_BUFQUE_CTRL_DEQUE_FAIL:
		if (IspInfo_FrmB.DebugMask & ISP_DBG_BUF_CTRL) {
			LOG_DBG("dq cm(%d),pID(%d),cID(0x%x)\n",
			param.ctrl, param.processID,
			param.callerID);
		}
		spin_lock(&(SpinLockEDBufQueList));
/* [1]update buffer status for the current buffer */
/* ////////////////////////////////////////////// */
/* Assume we have the buffer list in the following situation */
/* ++++++    ++++++ */
/* +  vss +    +  prv + */
/* ++++++    ++++++ */
/*  */
/* if the vss deque is not done(not blocking deque),
 * dequeThread in userspace would change to deque prv
 * buffer(block deque) immediately to decrease ioctrl
 * count.
 */
/* -> vss buffer would be deque at next turn,
 * so curBuf is still at vss buffer node
 */
/* -> we should use param to find the current buffer
 * index in Rlikst to update the buffer status cuz deque
 * success/fail may not be the first buffer in Rlist
 */
/* //////////////////////////////////////////// */
		idx2 = ISP_ED_BufQue_Get_FirstMatBuf(
		param, P2_EDBUF_RLIST_TAG, 1);
		if ((idx2 < 0) || (idx2 >= _MAX_SUPPORT_P2_FRAME_NUM_)) {
			LOG_DBG("Error:idx2(%d) is Invalid Index", idx2);
			ret = -EFAULT;
			return ret;
		}
		if (param.ctrl == ISP_ED_BUFQUE_CTRL_DEQUE_SUCCESS)
			P2_EDBUF_RingList[idx2].bufSts =
			ISP_ED_BUF_STATE_DEQUE_SUCCESS;
		else {
			P2_EDBUF_RingList[idx2].bufSts =
			ISP_ED_BUF_STATE_DEQUE_FAIL;
		}
		/* [2]update dequeued num in managed buffer list */
		idx = ISP_ED_BufQue_Get_FirstMatBuf(
		param, P2_EDBUF_MLIST_TAG, 1);
		if (idx == -1) {
			spin_unlock(&(SpinLockEDBufQueList));
			LOG_ERR("ERRRRRRRRRRR findmatch index fail");
			ret = -EFAULT;
			return ret;
		}
		P2_EDBUF_MgrList[idx].dequedNum++;
		/* [3]update global pointer */
		ISP_ED_BufQue_Update_GPtr(P2_EDBUF_RLIST_TAG);
		/* [4]erase node in ring buffer list */
		if (idx2 == -1) {
			spin_unlock(&(SpinLockEDBufQueList));
			LOG_ERR("ERRRRRRRRRRR findmatch index fail");
			ret = -EFAULT;
			return ret;
		}
		ISP_ED_BufQue_Erase(idx2, P2_EDBUF_RLIST_TAG);
		spin_unlock(&(SpinLockEDBufQueList));
/* [5]wake up thread user that wait for a specific
 * buffer and the thread that wait for deque
 */
		wake_up_interruptible((wait_queue_head_t *)(
		&WaitQueueHead_EDBuf_WaitFrame));
		wake_up_interruptible((wait_queue_head_t *)(
		&WaitQueueHead_EDBuf_WaitDeque));
		break;
	case ISP_ED_BUFQUE_CTRL_WAIT_FRAME:	/* wait for a specific buffer */
		spin_lock(&(SpinLockEDBufQueList));
		/* [1]find first match buffer */
		idx = ISP_ED_BufQue_Get_FirstMatBuf(param,
		P2_EDBUF_MLIST_TAG, 0);
		if (idx == -1) {
			spin_unlock(&(SpinLockEDBufQueList));
			LOG_ERR(
			"could not find match buffer pID/cID (%d/0x%x)",
			param.processID, param.callerID);
			ret = -EFAULT;
			return ret;
		}
		/* [2]check the buffer is dequeued or not */
		if (P2_EDBUF_MgrList[idx].dequedNum == P2_Support_BurstQNum) {
/* erase the buffer no matter user
 * wait successfully or not
 */
			ISP_ED_BufQue_Erase(idx, P2_EDBUF_MLIST_TAG);
			spin_unlock(&(SpinLockEDBufQueList));
			ret = 0;
			LOG_DBG(
			"Frame is alreay dequeued, return user, pd(%d/0x%x),idx(%d)",
			param.processID, param.callerID, idx);
			return ret;
		}
			spin_unlock(&(SpinLockEDBufQueList));
			if (IspInfo_FrmB.DebugMask & ISP_DBG_BUF_CTRL) {
				LOG_DBG(
				"=pd(%d/0x%x_%d)wait(%d us)=\n",
				param.processID, param.callerID,
				idx, param.timeoutUs);
			}
/* [3]if not, goto wait event and
 * wait for a signal to check
 */
			restTime = wait_event_interruptible_timeout(*
			((wait_queue_head_t *)(
			&WaitQueueHead_EDBuf_WaitFrame)),
			ISP_GetEDBufQueWaitFrameState(idx),
			ISP_UsToJiffies_FrmB(param.timeoutUs));
			if (restTime == 0) {
				LOG_ERR(
				"Dequeue Buffer fail, rT(%d),idx(%d) pID(%d),cID(0x%x),p2SupportBNum(%d)\n",
				restTime, idx, param.processID,
				param.callerID, P2_Support_BurstQNum);
				ret = -EFAULT;
				break;
			}
				spin_lock(&(SpinLockEDBufQueList));
				/* erase the buffer if wait successfully */
				ISP_ED_BufQue_Erase(idx, P2_EDBUF_MLIST_TAG);
				spin_unlock(&(SpinLockEDBufQueList));
		break;
/* wake all slept users to check buffer
 * is dequeued or not
 */
	case ISP_ED_BUFQUE_CTRL_WAKE_WAITFRAME:
		wake_up_interruptible((wait_queue_head_t *)(
		&WaitQueueHead_EDBuf_WaitFrame));
		break;
		/* free all recored dequeued buffer */
	case ISP_ED_BUFQUE_CTRL_CLAER_ALL:
		spin_lock(&(SpinLockEDBufQueList));
		for (i = 0; i < _MAX_SUPPORT_P2_FRAME_NUM_; i++) {
			P2_EDBUF_RingList[i].processID = 0x0;
			P2_EDBUF_RingList[i].callerID = 0x0;
			P2_EDBUF_RingList[i].bufSts = ISP_ED_BUF_STATE_NONE;
		}
		P2_EDBUF_RList_FirstBufIdx = 0;
		P2_EDBUF_RList_CurBufIdx = 0;
		P2_EDBUF_RList_LastBufIdx = -1;
		/*  */
		for (i = 0; i < _MAX_SUPPORT_P2_PACKAGE_NUM_; i++) {
			P2_EDBUF_MgrList[i].processID = 0x0;
			P2_EDBUF_MgrList[i].callerID = 0x0;
			P2_EDBUF_MgrList[i].p2dupCQIdx = -1;
			P2_EDBUF_MgrList[i].dequedNum = 0;
		}
		P2_EDBUF_MList_FirstBufIdx = 0;
		P2_EDBUF_MList_LastBufIdx = -1;
		spin_unlock(&(SpinLockEDBufQueList));
		break;
	default:
		LOG_ERR("do not support this ctrl cmd(%d)", param.ctrl);
		break;
	}
	return ret;
}

/*************************************************
 *
 *************************************************/
static signed int ISP_REGISTER_IRQ_USERKEY(char *userName)
{
/* -1 means there is no any un-locked user key */
	int key = -1;
	int i = 0;
	int length = 0;
/* local veriable for saving Username from user space */
	char m_UserName[USERKEY_STR_LEN];
	bool bCopyFromUser = MTRUE;

	if (userName == NULL) {
		LOG_ERR(" [regUser] userName is NULL\n");
	} else {
		/* get UserName from user space */
		length = strnlen_user(userName, USERKEY_STR_LEN);
		if (length == 0) {
			LOG_ERR(" [regUser] userName address is not valid\n");
			return key;
		}

		if (length > USERKEY_STR_LEN)
			length = USERKEY_STR_LEN;

		if (copy_from_user(m_UserName, (void *)(userName),
			length * sizeof(char)) != 0)
			bCopyFromUser = MFALSE;

		if (bCopyFromUser == MTRUE) {
			spin_lock((spinlock_t *)(&SpinLock_UserKey));
			/* check String length, add end */
			/* string length too long */
			if (length == USERKEY_STR_LEN) {
				m_UserName[length - 1] = '\0';
				if (IspInfo_FrmB.DebugMask & ISP_DBG_INT)
					LOG_INF(
					" [regUser] userName(%s) is too long (>%d)\n",
					m_UserName, USERKEY_STR_LEN);
			}

			if (IspInfo_FrmB.DebugMask & ISP_DBG_INT)
				LOG_INF(
				" [regUser] UserName (%s)\n",
				m_UserName);

			/* 1. check the current users is full or not */
			if (FirstUnusedIrqUserKey >=
				IRQ_USER_NUM_MAX ||
				FirstUnusedIrqUserKey < 0) {
				key = -1;
			} else {
/* 2. check the user had registered or not */
/* index 0 is for all the users that
 * do not register irq first
 */
				for (i = 1; i < FirstUnusedIrqUserKey; i++) {
					if (strcmp((char *)IrqUserKey_UserInfo[
						i].userName, m_UserName) == 0) {
						key = IrqUserKey_UserInfo[i].
						userKey;
						break;
					}
				}

/* 3.return new userkey for user if the user
 * had not registered before
 */
				if (key > 0) {
				} else {
					memset((void *)IrqUserKey_UserInfo[i].
					userName, 0, USERKEY_STR_LEN);
					strncpy((char *)IrqUserKey_UserInfo[i].
					userName, m_UserName,
					USERKEY_STR_LEN-1);
					IrqUserKey_UserInfo[i].userKey =
					FirstUnusedIrqUserKey;
					key = FirstUnusedIrqUserKey;
					FirstUnusedIrqUserKey++;
				}
			}
			spin_unlock((spinlock_t *)(&SpinLock_UserKey));
		} else {
			LOG_ERR(" [regUser] copy_from_user failed (%d)\n", i);
		}
	}
	return key;
}

/*************************************************
 *
 *************************************************/
static signed int ISP_MARK_IRQ(struct ISP_WAIT_IRQ_STRUCT_FRMB irqinfo)
{
	enum eISPIrq eIrq = _IRQ;
	unsigned long flags;/*unsigned int flags;*/
	int idx;
	unsigned long long time_sec;
	unsigned long time_usec;

	switch (irqinfo.UserInfo.Type) {
	default:
		eIrq = _IRQ;
		break;
	}

	if ((irqinfo.UserInfo.UserKey >= IRQ_USER_NUM_MAX)
		|| (irqinfo.UserInfo.UserKey < 1)) {
		LOG_DBG("invalid userKey(%d), max(%d)",
		irqinfo.UserInfo.UserKey,
		IRQ_USER_NUM_MAX);
		return -EINVAL;
	}
	if ((irqinfo.UserInfo.Type >= ISP_IRQ_TYPE_AMOUNT_FRMB)
		|| (irqinfo.UserInfo.Type < 0)) {
		LOG_DBG("invalid type(%d), max(%d)", irqinfo.UserInfo.Type,
			ISP_IRQ_TYPE_AMOUNT_FRMB);
		return -EINVAL;
	}

	/* 1. enable marked flag */
	spin_lock_irqsave(&(IspInfo_FrmB.SpinLockIrq[eIrq]), flags);
	IspInfo_FrmB.IrqInfo.MarkedFlag[irqinfo.UserInfo.
	UserKey][irqinfo.UserInfo.Type] |=
	irqinfo.UserInfo.Status;
	spin_unlock_irqrestore(&(IspInfo_FrmB.SpinLockIrq[eIrq]), flags);

	/* 2. record mark time */
	idx = my_get_pow_idx(irqinfo.UserInfo.Status);

	time_sec = cpu_clock(0);	/* ns */
	do_div(time_sec, 1000);	/* usec */
	time_usec = do_div(time_sec, 1000000);	/* sec and usec */
	spin_lock_irqsave(&(IspInfo_FrmB.SpinLockIrq[eIrq]), flags);
	if ((irqinfo.UserInfo.UserKey < 0) || (irqinfo.UserInfo.UserKey >= IRQ_USER_NUM_MAX) ||
		(irqinfo.UserInfo.Type < 0) || (irqinfo.UserInfo.Type >= ISP_IRQ_TYPE_AMOUNT) ||
		(idx < 0) || (idx >= 32)) {
		LOG_DBG("Error: Invalid Index");
		return -EINVAL;
	}
	IspInfo_FrmB.IrqInfo.MarkedTime_usec[irqinfo.
	UserInfo.UserKey][irqinfo.UserInfo.Type][idx] =
	(unsigned int)(time_usec);
	IspInfo_FrmB.IrqInfo.MarkedTime_sec[irqinfo.
	UserInfo.UserKey][irqinfo.UserInfo.Type][idx] =
	(unsigned int)(time_sec);
	spin_unlock_irqrestore(&(IspInfo_FrmB.SpinLockIrq[eIrq]), flags);

	/* 3. clear passed by signal count */
	spin_lock_irqsave(&(IspInfo_FrmB.SpinLockIrq[eIrq]), flags);
	IspInfo_FrmB.IrqInfo.PassedBySigCnt[irqinfo.
	UserInfo.UserKey][irqinfo.UserInfo.Type][idx] =
	    0;
	spin_unlock_irqrestore(&(IspInfo_FrmB.SpinLockIrq[eIrq]), flags);

	LOG_DBG(
	"MARK key/type/sts (%d/%d/0x%x), t(%d us)",
	irqinfo.UserInfo.UserKey,
	irqinfo.UserInfo.Type,
	irqinfo.UserInfo.Status, (int)(time_usec));

	return 0;
}


/*************************************************
 *
 *************************************************/
static signed int ISP_GET_MARKtoQEURY_TIME(struct ISP_WAIT_IRQ_STRUCT_FRMB *irqinfo)
{
	signed int Ret = 0;
	unsigned long flags;/*unsigned int flags;*/
	enum eISPIrq eIrq = _IRQ;
	unsigned long long time_ready2return_sec;
	unsigned long time_ready2return_usec;
	unsigned int idx;

	time_ready2return_sec = cpu_clock(0);	/* ns */
	do_div(time_ready2return_sec, 1000);	/* usec */
	time_ready2return_usec = do_div(
	time_ready2return_sec, 1000000);	/* sec and usec */
	idx = my_get_pow_idx(irqinfo->UserInfo.Status);

	switch (irqinfo->UserInfo.Type) {
/* case ISP_IRQ_TYPE_INT_CAMSV:
 * eIrq = _CAMSV_IRQ;      break;
 */
/* case ISP_IRQ_TYPE_INT_CAMSV2:
 * eIrq = _CAMSV_D_IRQ;    break;
 */
/* case ISP_IRQ_TYPE_INT_P1_ST_D: */
/* case ISP_IRQ_TYPE_INT_P1_ST2_D: */
	default:
		eIrq = _IRQ;
		break;
	}

	if ((irqinfo->UserInfo.UserKey < 0) || (irqinfo->UserInfo.UserKey >= IRQ_USER_NUM_MAX) ||
		(irqinfo->UserInfo.Type < 0) || (irqinfo->UserInfo.Type >= ISP_IRQ_TYPE_AMOUNT) ||
		(idx <0) || (idx >= 32)) {
		LOG_DBG("Error: Invalid Index, \
		irqinfo->UserInfo.UserKey(%d), \
		irqinfo->UserInfo.Type(%d), \
		idx(%d)",
		irqinfo->UserInfo.UserKey,
		irqinfo->UserInfo.Type,
		idx);
		Ret = -EINVAL;
		return Ret;
		}
	if ((irqinfo->UserInfo.UserKey >= IRQ_USER_NUM_MAX)
		|| (irqinfo->UserInfo.UserKey < 1)) {
		LOG_DBG("invalid userKey(%d), max(%d)",
		irqinfo->UserInfo.UserKey,
		IRQ_USER_NUM_MAX);
		Ret = -EINVAL;
		return Ret;
	}
	if ((irqinfo->UserInfo.Type >= ISP_IRQ_TYPE_AMOUNT_FRMB)
		|| (irqinfo->UserInfo.Type < 0)) {
		LOG_DBG("invalid type(%d), max(%d)", irqinfo->UserInfo.Type,
			ISP_IRQ_TYPE_AMOUNT_FRMB);
		Ret = -EINVAL;
		return Ret;
	}

	spin_lock_irqsave(&(IspInfo_FrmB.SpinLockIrq[eIrq]), flags);
	if (irqinfo->UserInfo.Status & IspInfo_FrmB.IrqInfo.
	    MarkedFlag[irqinfo->UserInfo.UserKey][irqinfo->UserInfo.Type]) {
		if ((irqinfo->UserInfo.Type < 0) || (irqinfo->UserInfo.Type >= ISP_IRQ_TYPE_AMOUNT) ||
			(idx < 0) || (idx >= 32)) {
			LOG_DBG("Error: Invalid Index ,irqinfo->UserInfo.Type(%d), idx(%d)",
			irqinfo->UserInfo.Type,idx);
			Ret = -EINVAL;
			return Ret;
		}
		/*  */
		irqinfo->TimeInfo.passedbySigcnt =
		IspInfo_FrmB.IrqInfo.PassedBySigCnt[irqinfo->
		UserInfo.UserKey][irqinfo->UserInfo.Type][idx];
		/*  */
		irqinfo->TimeInfo.tMark2WaitSig_usec =
		(time_ready2return_usec - IspInfo_FrmB.
		IrqInfo.MarkedTime_usec[irqinfo->UserInfo.
		UserKey][irqinfo->UserInfo.Type][idx]);
		irqinfo->TimeInfo.tMark2WaitSig_sec =
		(time_ready2return_sec - IspInfo_FrmB.
		IrqInfo.MarkedTime_sec[irqinfo->UserInfo.
		UserKey][irqinfo->UserInfo.Type][idx]);
		if ((int)(irqinfo->TimeInfo.tMark2WaitSig_usec) < 0) {
			irqinfo->TimeInfo.tMark2WaitSig_sec =
			    irqinfo->TimeInfo.tMark2WaitSig_sec - 1;
			if ((int)(irqinfo->TimeInfo.tMark2WaitSig_sec) < 0)
				irqinfo->TimeInfo.tMark2WaitSig_sec = 0;
			irqinfo->TimeInfo.tMark2WaitSig_usec =
			    1 * 1000000 + irqinfo->TimeInfo.tMark2WaitSig_usec;
		}
		/*  */
		if (irqinfo->TimeInfo.passedbySigcnt > 0) {
			irqinfo->TimeInfo.tLastSig2GetSig_usec =
			(time_ready2return_usec -
			IspInfo_FrmB.IrqInfo.LastestSigTime_usec[
			irqinfo->UserInfo.Type][idx]);
			irqinfo->TimeInfo.tLastSig2GetSig_sec =
			(time_ready2return_sec -
			IspInfo_FrmB.IrqInfo.LastestSigTime_sec[
			irqinfo->UserInfo.Type][idx]);
			if ((int)(irqinfo->TimeInfo.tLastSig2GetSig_usec) < 0) {
				irqinfo->TimeInfo.tLastSig2GetSig_sec =
				    irqinfo->TimeInfo.tLastSig2GetSig_sec - 1;
				if ((int)(irqinfo->TimeInfo.
					tLastSig2GetSig_sec) < 0)
					irqinfo->TimeInfo.
					tLastSig2GetSig_sec = 0;
				irqinfo->TimeInfo.tLastSig2GetSig_usec =
				1 * 1000000 + irqinfo->
				TimeInfo.tLastSig2GetSig_usec;
			}
		} else {
			irqinfo->TimeInfo.tLastSig2GetSig_usec = 0;
			irqinfo->TimeInfo.tLastSig2GetSig_sec = 0;
		}
	} else {
		LOG_WRN("plz mark irq first, userKey/Type/Status (%d/%d/0x%x)",
			irqinfo->UserInfo.UserKey, irqinfo->UserInfo.Type,
			irqinfo->UserInfo.Status);
		Ret = -EFAULT;
	}
	spin_unlock_irqrestore(&(IspInfo_FrmB.SpinLockIrq[eIrq]), flags);

	return Ret;
}

/*************************************************
 *
 *************************************************/
static signed int ISP_FLUSH_IRQ(struct ISP_WAIT_IRQ_STRUCT_FRMB irqinfo)
{
	enum eISPIrq eIrq = _IRQ;
	unsigned long flags;/*unsigned int flags;*/

	switch (irqinfo.UserInfo.Type) {
	default:
		eIrq = _IRQ;
		break;
	}

	if (irqinfo.UserInfo.UserKey != 0) {   /* isp driver */
		LOG_DBG(
		"invalid userKey(%d), max(%d)",
		irqinfo.UserInfo.UserKey,
		IRQ_USER_NUM_MAX);
		return -EINVAL;
	}
	if ((irqinfo.UserInfo.Type >= ISP_IRQ_TYPE_AMOUNT_FRMB)
		|| (irqinfo.UserInfo.Type < 0)) {
		LOG_DBG("invalid type(%d), max(%d)", irqinfo.UserInfo.Type,
			ISP_IRQ_TYPE_AMOUNT_FRMB);
		return -EINVAL;
	}

	/* 1. enable signal */
	spin_lock_irqsave(&(IspInfo_FrmB.SpinLockIrq[eIrq]), flags);
	IspInfo_FrmB.IrqInfo.Status[irqinfo.UserInfo.
	UserKey][irqinfo.UserInfo.Type] |=
	    irqinfo.UserInfo.Status;
	spin_unlock_irqrestore(&(IspInfo_FrmB.SpinLockIrq[eIrq]), flags);

	/* 2. force to wake up the user that
	 * are waiting for that signal
	 */
	wake_up_interruptible((wait_queue_head_t *)(
	&IspInfo_FrmB.WaitQueueHead));

	return 0;
}


/*************************************************
 *
 *************************************************/
static signed int ISP_WaitIrq_FrmB(struct ISP_WAIT_IRQ_STRUCT_FRMB *WaitIrq)
{
	signed int Ret = 0, Timeout = WaitIrq->Timeout;
	/*unsigned int i;*/
	unsigned long flags;/*unsigned int flags;*/
	enum eISPIrq eIrq = _IRQ;
	/*int cnt = 0;*/
	int idx = my_get_pow_idx(WaitIrq->UserInfo.Status);
	bool freeze_passbysigcnt = false;
	unsigned long long time_getrequest_sec;
	unsigned long time_getrequest_usec;
	unsigned int t_Wake = 0;		/* ns */
	unsigned long long time_ready2return_sec;
	unsigned long time_ready2return_usec;

	time_getrequest_sec = cpu_clock(0);	/* ns */
	do_div(time_getrequest_sec, 1000);	/* usec */
	/* sec and usec */
	time_getrequest_usec = do_div(
	time_getrequest_sec, 1000000);

	/*struct timeval time_wake;*/
	/*struct timeval time_x;*/
	/*unsigned int t_Wake = 0;*/		/* ns */
	/*  */
	if (IspInfo_FrmB.DebugMask & ISP_DBG_INT) {
		if (WaitIrq->UserInfo.
		    Status & (ISP_IRQ_INT_STATUS_SOF1_INT_ST |
		    ISP_IRQ_INT_STATUS_PASS1_TG1_DON_ST
/* |ISP_IRQ_P1_STATUS_D_SOF1_INT_ST|
 * ISP_IRQ_P1_STATUS_D_PASS1_DON_ST
 */
			      )) {
			LOG_DBG(
			"+WaitIrq Clear(%d), Type(%d), Status(0x%08X), Timeout(%d),user(%d)",
			WaitIrq->Clear, WaitIrq->UserInfo.Type,
			WaitIrq->UserInfo.Status,
			WaitIrq->Timeout, WaitIrq->UserInfo.UserKey);
		}
	}

	switch (WaitIrq->UserInfo.Type) {
/* case ISP_IRQ_TYPE_INT_CAMSV:
 * eIrq = _CAMSV_IRQ;      break;
 */
/* case ISP_IRQ_TYPE_INT_CAMSV2:
 * eIrq = _CAMSV_D_IRQ;    break;
 */
/* case ISP_IRQ_TYPE_INT_P1_ST_D: */
/* case ISP_IRQ_TYPE_INT_P1_ST2_D: */
	default:
		eIrq = _IRQ;
		break;
	}

	if ((WaitIrq->UserInfo.UserKey >= IRQ_USER_NUM_MAX)
		|| (WaitIrq->UserInfo.UserKey < 0)) {
		LOG_DBG("invalid userKey(%d), max(%d)",
		WaitIrq->UserInfo.UserKey,
			IRQ_USER_NUM_MAX);
		Ret = -EINVAL;
		return Ret;
	}

	if ((WaitIrq->UserInfo.Type >= ISP_IRQ_TYPE_AMOUNT_FRMB)
		|| (WaitIrq->UserInfo.Type < 0)) {
		LOG_DBG("invalid type(%d), max(%d)", WaitIrq->UserInfo.Type,
			ISP_IRQ_TYPE_AMOUNT_FRMB);
		Ret = -EINVAL;
		return Ret;
	}

	/* 1. wait type update */
	if (WaitIrq->Clear == ISP_IRQ_CLEAR_STATUS_FRMB) {
		spin_lock_irqsave(&(IspInfo_FrmB.SpinLockIrq[eIrq]), flags);
/* LOG_DBG("WARNING: Clear(%d), Type(%d):
 * IrqStatus(0x%08X) has been cleared",WaitIrq->Clear,
 * WaitIrq->Type,IspInfo_FrmB.IrqInfo.Status[
 * WaitIrq->Type]);
 */
		IspInfo_FrmB.IrqInfo.Status[WaitIrq->UserInfo.
		UserKey][WaitIrq->UserInfo.Type] &=
		(~WaitIrq->UserInfo.Status);
		spin_unlock_irqrestore(&(
		IspInfo_FrmB.SpinLockIrq[eIrq]), flags);
		return Ret;
	}
		if (WaitIrq->UserInfo.Status & IspInfo_FrmB.IrqInfo.
		MarkedFlag[WaitIrq->UserInfo.UserKey][
		WaitIrq->UserInfo.Type]) {
/* force to be non_clear wait if marked before,
 * and check the request wait timing
 */
/* if the entry time of wait request after mark is
 * before signal occurring, we freese the
 * counting for passby signal
 */

/*  */
/* v : kernel receive mark request */
/* o : kernel receive wait request */
/* \A1\F4: return to user */
/*  */
/* case: freeze is true, and passby signal count = 0 */
/*  */
/* |                                              | */
/* |                                  (wait)    | */
/* |       v-------------o++++++ |\A1\F4 */
/* |                                              | */
/* Sig                                            Sig */
/*  */
/* case: freeze is false, and passby signal count = 1 */
/*  */
/* |                                              | */
/* |                                              | */
/* |       v---------------------- |-o  \A1\F4(return) */
/* |                                              | */
/* Sig                                            Sig */
/*  */

			freeze_passbysigcnt =
			!(ISP_GetIRQState_FrmB
			(eIrq, WaitIrq->UserInfo.Type,
			WaitIrq->UserInfo.UserKey,
			WaitIrq->UserInfo.Status));
		} else {
			if (WaitIrq->Clear == ISP_IRQ_CLEAR_WAIT_FRMB) {
				spin_lock_irqsave(&(IspInfo_FrmB.
				SpinLockIrq[eIrq]), flags);
				if (IspInfo_FrmB.IrqInfo.
				Status[WaitIrq->UserInfo.UserKey][
				WaitIrq->UserInfo.Type]
				& WaitIrq->UserInfo.
				    Status) {
/* LOG_DBG("WARNING: Clear(%d), Type(%d):
 * IrqStatus(0x%08X) has been cleared",WaitIrq->Clear,
 * WaitIrq->Type,IspInfo_FrmB.IrqInfo.Status[
 * WaitIrq->Type] & WaitIrq->Status);
 */
					IspInfo_FrmB.IrqInfo.Status[
					WaitIrq->UserInfo.UserKey][
					WaitIrq->UserInfo.Type] &=
					(~WaitIrq->UserInfo.Status);
				}
				spin_unlock_irqrestore(&(IspInfo_FrmB.
				SpinLockIrq[eIrq]), flags);
			} else if (WaitIrq->Clear == ISP_IRQ_CLEAR_ALL_FRMB) {
				spin_lock_irqsave(&
				(IspInfo_FrmB.SpinLockIrq[eIrq]), flags);
				/* LOG_DBG("WARNING: Clear(%d), Type(%d):
				 * IrqStatus(0x%08X) has been cleared",
				 WaitIrq->Clear,WaitIrq->Type,
				 IspInfo_FrmB.IrqInfo.Status[WaitIrq->Type]);
				 */
				IspInfo_FrmB.IrqInfo.Status[WaitIrq->
				UserInfo.UserKey][WaitIrq->
				UserInfo.Type] = 0;
				spin_unlock_irqrestore(&
				(IspInfo_FrmB.SpinLockIrq[eIrq]), flags);
			}
		}

	/* 2. start to wait signal */
	Timeout = wait_event_interruptible_timeout(
	*((wait_queue_head_t *)(
	&IspInfo_FrmB.WaitQueueHead)),
	ISP_GetIRQState_FrmB(eIrq,
	WaitIrq->UserInfo.Type,
	WaitIrq->UserInfo.UserKey,
	WaitIrq->UserInfo.Status),
	ISP_MsToJiffies_FrmB(WaitIrq->Timeout));

	if ((WaitIrq->UserInfo.UserKey == 0) &&
		(IspInfo_FrmB.DebugMask & ISP_DBG_INT)
	    && (WaitIrq->UserInfo.Status & ISP_IRQ_INT_STATUS_SOF1_INT_ST)) {
		unsigned long long time_wake_sec;
		unsigned long time_wake_usec;
#if CONFIG_K_FOR_SYSTRACE
		char strName[128];

		sprintf(strName, "TAG_K_WAKEUP (%d)", sof_count[_PASS1]);
		_kernel_trace_begin(strName);
#endif

		LOG_DBG("WaitIrq wakeup (%d)\n", WaitIrq->UserInfo.UserKey);


		time_wake_sec = cpu_clock(0);	/* ns */
		do_div(time_wake_sec, 1000);	/* usec */
		/* sec and usec */
		time_wake_usec = do_div(time_wake_sec, 1000000);
		t_Wake = time_wake_sec * 1000000 + time_wake_usec;
		if (t_SOF && (t_Wake - t_SOF >= 2000)) {	/* >2ms */
			LOG_DBG("_T: SOF-Wake (%d)\n", (t_Wake - t_SOF));
		}
	}
	/* check if user is interrupted by system signal */
	if ((Timeout != 0) && (!ISP_GetIRQState_FrmB(eIrq,
		WaitIrq->UserInfo.Type, WaitIrq->UserInfo.UserKey,
		WaitIrq->UserInfo.Status))) {
		LOG_DBG(
		"interrupted by system signal,return value(%d),irq Type/User/Sts(0x%x/%d/0x%x)",
		Timeout, WaitIrq->UserInfo.Type,
		WaitIrq->UserInfo.UserKey,
		WaitIrq->UserInfo.Status);
		Ret = -ERESTARTSYS;	/* actually it should be -ERESTARTSYS */
		goto EXIT;
	}

	if (Timeout == 0) {	/* timeout */
		spin_lock_irqsave(&(IspInfo_FrmB.SpinLockIrq[eIrq]), flags);
		LOG_ERR(
		"WaitIrq Timeout User(%d),Clear(%d), Type(%d), IrqStatus(0x%08X), WaitStatus(0x%08X), Timeout(%d)",
		WaitIrq->UserInfo.UserKey, WaitIrq->Clear,
		WaitIrq->UserInfo.Type,
		IspInfo_FrmB.IrqInfo.Status[WaitIrq->
		UserInfo.UserKey][WaitIrq->UserInfo.Type],
		WaitIrq->UserInfo.Status, WaitIrq->Timeout);
		spin_unlock_irqrestore(&(IspInfo_FrmB.
		SpinLockIrq[eIrq]), flags);

		LOG_ERR("bDumpReg(%d)", WaitIrq->bDumpReg);
		if (WaitIrq->bDumpReg)
			ISP_DumpReg_FrmB();
		Ret = -EFAULT;
		goto EXIT;
	}

	/* 3. get interrupt and update time related
	 * information that would be return to user
	 */
	/*unsigned long long time_ready2return_sec;*/
	/*unsigned long time_ready2return_usec;*/

	time_ready2return_sec = cpu_clock(0);	/* ns */
	do_div(time_ready2return_sec, 1000);	/* usec */
	time_ready2return_usec = do_div(
	time_ready2return_sec, 1000000);	/* sec and usec */
	spin_lock_irqsave(&(IspInfo_FrmB.SpinLockIrq[eIrq]), flags);
	/* signal time stamp for eis */
	if ((WaitIrq->UserInfo.Type < 0) || (WaitIrq->UserInfo.Type >= ISP_IRQ_TYPE_AMOUNT) ||
		(idx < 0) || (idx >= 32)) {
		LOG_ERR("Error: Invalid Index");
		Ret = -EINVAL;
		return Ret;
	}
	WaitIrq->TimeInfo.tLastSig_sec =
	IspInfo_FrmB.IrqInfo.LastestSigTime_sec[
	WaitIrq->UserInfo.Type][idx];
	WaitIrq->TimeInfo.tLastSig_usec =
	IspInfo_FrmB.IrqInfo.LastestSigTime_usec[
	WaitIrq->UserInfo.Type][idx];
	if (WaitIrq->UserInfo.Type < ISP_IRQ_TYPE_INTB_FRMB/*ISP_IRQ_TYPE_INTB*/
	    && WaitIrq->SpecUser == ISP_IRQ_WAITIRQ_SPEUSER_EIS) {
		if (gEismetaWIdx == 0) {
			if (gEismetaInSOF == 0)
				gEismetaRIdx = (EISMETA_RINGSIZE - 1);
			else
				gEismetaRIdx = (EISMETA_RINGSIZE - 2);
		} else if (gEismetaWIdx == 1) {
			if (gEismetaInSOF == 0)
				gEismetaRIdx = 0;
			else
				gEismetaRIdx = (EISMETA_RINGSIZE - 1);
		} else
			gEismetaRIdx = (gEismetaWIdx - gEismetaInSOF - 1);

		if ((gEismetaRIdx < 0) || (gEismetaRIdx >= EISMETA_RINGSIZE)) {
			/* BUG_ON(1); */
			gEismetaRIdx = 0;
			/* TBD WARNING */
		}
		/* eis meta */
		WaitIrq->EisMeta.tLastSOF2P1done_sec =
		IspInfo_FrmB.IrqInfo.Eismeta[WaitIrq->
		UserInfo.Type][gEismetaRIdx].
		tLastSOF2P1done_sec;
		WaitIrq->EisMeta.tLastSOF2P1done_usec =
		IspInfo_FrmB.IrqInfo.Eismeta[WaitIrq->
		UserInfo.Type][gEismetaRIdx].
		tLastSOF2P1done_usec;
	}
	/* time period for 3A */
	if (WaitIrq->UserInfo.Status & IspInfo_FrmB.IrqInfo.
	    MarkedFlag[WaitIrq->UserInfo.UserKey][WaitIrq->UserInfo.Type]) {
		if ((WaitIrq->UserInfo.UserKey < 0) || (WaitIrq->UserInfo.UserKey >= IRQ_USER_NUM_MAX) ||
			(WaitIrq->UserInfo.Type < 0) || (WaitIrq->UserInfo.Type >= ISP_IRQ_TYPE_AMOUNT) ||
			(idx < 0) || (idx >= 32)) {
			LOG_DBG("Error: Invalid Index");
			Ret = -EINVAL;
			return Ret;
		}
		WaitIrq->TimeInfo.tMark2WaitSig_usec =
		(time_getrequest_usec - IspInfo_FrmB.IrqInfo.
		MarkedTime_usec[WaitIrq->UserInfo.UserKey][
		WaitIrq->UserInfo.Type][idx]);
		WaitIrq->TimeInfo.tMark2WaitSig_sec =
		(time_getrequest_sec - IspInfo_FrmB.IrqInfo.
		MarkedTime_sec[WaitIrq->UserInfo.UserKey][
		WaitIrq->UserInfo.Type][idx]);
		if ((int)(WaitIrq->TimeInfo.tMark2WaitSig_usec) < 0) {
			WaitIrq->TimeInfo.tMark2WaitSig_sec =
			    WaitIrq->TimeInfo.tMark2WaitSig_sec - 1;
			if ((int)(WaitIrq->TimeInfo.tMark2WaitSig_sec) < 0)
				WaitIrq->TimeInfo.tMark2WaitSig_sec = 0;
			WaitIrq->TimeInfo.tMark2WaitSig_usec =
			    1 * 1000000 + WaitIrq->TimeInfo.tMark2WaitSig_usec;
		}
		/*  */
		if ((WaitIrq->UserInfo.Type < 0) || (WaitIrq->UserInfo.Type >= ISP_IRQ_TYPE_AMOUNT) ||
			(idx < 0) || (idx >= 32)) {
			LOG_DBG("Error: Invalid Index");
			Ret = -EINVAL;
			return Ret;
		}
		WaitIrq->TimeInfo.tLastSig2GetSig_usec =
		    (time_ready2return_usec -
		     IspInfo_FrmB.IrqInfo.LastestSigTime_usec[
		     WaitIrq->UserInfo.Type][idx]);
		WaitIrq->TimeInfo.tLastSig2GetSig_sec =
		    (time_ready2return_sec -
		     IspInfo_FrmB.IrqInfo.LastestSigTime_sec[
		     WaitIrq->UserInfo.Type][idx]);
		if ((int)(WaitIrq->TimeInfo.tLastSig2GetSig_usec) < 0) {
			WaitIrq->TimeInfo.tLastSig2GetSig_sec =
			    WaitIrq->TimeInfo.tLastSig2GetSig_sec - 1;
			if ((int)(WaitIrq->TimeInfo.tLastSig2GetSig_sec) < 0)
				WaitIrq->TimeInfo.tLastSig2GetSig_sec = 0;
			WaitIrq->TimeInfo.tLastSig2GetSig_usec =
			1 * 1000000 + WaitIrq->
			TimeInfo.tLastSig2GetSig_usec;
		}
		/*  */
		if ((WaitIrq->UserInfo.UserKey < 0) || (WaitIrq->UserInfo.UserKey >= IRQ_USER_NUM_MAX) ||
			(WaitIrq->UserInfo.Type < 0) || (WaitIrq->UserInfo.Type >= ISP_IRQ_TYPE_AMOUNT) ||
			(idx < 0) || (idx >= 32)) {
			LOG_DBG("Error: Invalid Index");
			Ret = -EINVAL;
			return Ret;
		}
		if (freeze_passbysigcnt) {
			WaitIrq->TimeInfo.passedbySigcnt =
			IspInfo_FrmB.IrqInfo.PassedBySigCnt[
			WaitIrq->UserInfo.UserKey][WaitIrq->
			UserInfo.Type][idx] - 1;
		} else {
			WaitIrq->TimeInfo.passedbySigcnt =
			IspInfo_FrmB.IrqInfo.PassedBySigCnt[WaitIrq->
			UserInfo.UserKey][WaitIrq->
			UserInfo.Type][idx];
		}
	}
	LOG_DBG(
	" [WAITIRQv3]user(%d)  sigNum(%d/%d), d0 sec/usec (%d/%d), d1 sec/usec (%d/%d)\n",
	WaitIrq->UserInfo.UserKey,
	IspInfo_FrmB.IrqInfo.PassedBySigCnt[WaitIrq->
	UserInfo.UserKey][WaitIrq->UserInfo.Type][idx],
	WaitIrq->TimeInfo.passedbySigcnt,
	WaitIrq->TimeInfo.tMark2WaitSig_sec,
	WaitIrq->TimeInfo.tMark2WaitSig_usec,
	WaitIrq->TimeInfo.tLastSig2GetSig_sec,
	WaitIrq->TimeInfo.tLastSig2GetSig_usec);
	/*  */
#if 0
	if (IspInfo_FrmB.DebugMask & ISP_DBG_INT) {
		for (i = 0; i < ISP_IRQ_TYPE_AMOUNT_FRMB; i++)
			LOG_DBG("Type(%d), IrqStatus(0x%08X)",
			i, IspInfo_FrmB.IrqInfo.Status[i]);
	}
#endif
	/*  */
	/* clear the status if someone get the irq */
	IspInfo_FrmB.IrqInfo.Status[WaitIrq->UserInfo.UserKey][
	WaitIrq->UserInfo.Type] &= (~WaitIrq->UserInfo.Status);
	spin_unlock_irqrestore(&(IspInfo_FrmB.SpinLockIrq[eIrq]), flags);
	/*  */

EXIT:
	/* 4. clear mark flag / reset marked time /
	 * reset time related infor and passedby signal count
	 */
	spin_lock_irqsave(&(IspInfo_FrmB.SpinLockIrq[eIrq]), flags);
	if (WaitIrq->UserInfo.Status & IspInfo_FrmB.IrqInfo.
	MarkedFlag[WaitIrq->UserInfo.UserKey][WaitIrq->UserInfo.Type]) {
	IspInfo_FrmB.IrqInfo.MarkedFlag[WaitIrq->UserInfo.UserKey][
	WaitIrq->UserInfo.Type] &=
		    (~WaitIrq->UserInfo.Status);
		if ((WaitIrq->UserInfo.UserKey < 0) || (WaitIrq->UserInfo.UserKey >= IRQ_USER_NUM_MAX) ||
			(WaitIrq->UserInfo.Type < 0) || (WaitIrq->UserInfo.Type >= ISP_IRQ_TYPE_AMOUNT) ||
			(idx < 0) || (idx >= 32)) {
			LOG_DBG("Error: Invalid Index");
			Ret = -EINVAL;
			return Ret;
		}
		IspInfo_FrmB.IrqInfo.MarkedTime_usec[
		WaitIrq->UserInfo.UserKey][WaitIrq->
		UserInfo.Type][idx] = 0;
		IspInfo_FrmB.IrqInfo.MarkedTime_sec[
		WaitIrq->UserInfo.UserKey][WaitIrq->
		UserInfo.Type][idx] = 0;
		IspInfo_FrmB.IrqInfo.PassedBySigCnt[
		WaitIrq->UserInfo.UserKey][WaitIrq->
		UserInfo.Type][idx] = 0;
	}
	spin_unlock_irqrestore(&(IspInfo_FrmB.SpinLockIrq[eIrq]), flags);

	if ((WaitIrq->UserInfo.UserKey == 0) &&
		(IspInfo_FrmB.DebugMask & ISP_DBG_INT)
	    && (WaitIrq->UserInfo.Status & ISP_IRQ_INT_STATUS_SOF1_INT_ST)) {
		unsigned long long time_x_sec;
		unsigned long time_x_usec;
		unsigned int _tmp;
#if CONFIG_K_FOR_SYSTRACE
		_kernel_trace_end();
#endif
		LOG_DBG("WaitIrq X (%d)\n", WaitIrq->UserInfo.UserKey);

		time_x_sec = cpu_clock(0);	/* ns */
		do_div(time_x_sec, 1000);	/* usec */
		/* sec and usec */
		time_x_usec = do_div(time_x_sec, 1000000);
		_tmp = time_x_sec * 1000000 + time_x_usec;

		if (_tmp - t_Wake >= 1000) {	/* >1ms */
			LOG_DBG("_T: Wake-X (%d)\n", (_tmp - t_Wake));
		}
	}

	return Ret;
}

/* #define _debug_dma_err_ */
#if defined(_debug_dma_err_)
#define bit(x) (0x1<<(x))

unsigned int DMA_ERR[3 * 12] = {
	bit(1), 0xF50043A8, 0x00000011,	/* IMGI */
	bit(2), 0xF50043AC, 0x00000021,	/* IMGCI */
	bit(4), 0xF50043B0, 0x00000031,	/* LSCI */
	bit(5), 0xF50043B4, 0x00000051,	/* FLKI */
	bit(6), 0xF50043B8, 0x00000061,	/* LCEI */
	bit(7), 0xF50043BC, 0x00000071,	/* VIPI */
	bit(8), 0xF50043C0, 0x00000081,	/* VIP2I */
	bit(9), 0xF50043C4, 0x00000194,	/* IMGO */
	bit(10), 0xF50043C8, 0x000001a4,	/* IMG2O */
	bit(11), 0xF50043CC, 0x000001b4,	/* LCSO */
	bit(12), 0xF50043D0, 0x000001c4,	/* ESFKO */
	bit(13), 0xF50043D4, 0x000001d4,	/* AAO */
};

static signed int DMAErrHandler(void)
{
	unsigned int err_ctrl = ISP_RD32(0xF50043A4);

	LOG_DBG("err_ctrl(0x%08x)", err_ctrl);

	unsigned int i = 0;

	unsigned int *pErr = DMA_ERR;

	for (i = 0; i < 12; i++) {
		unsigned int addr = 0;
#if 1
		if (err_ctrl & (*pErr)) {
			ISP_WR32(0xF5004160, pErr[2]);
			addr = pErr[1];

			LOG_DBG("(0x%08x, 0x%08x), dbg(0x%08x, 0x%08x)",
			addr, ISP_RD32(addr), ISP_RD32(0xF5004160),
			ISP_RD32(0xF5004164));
		}
#else
		addr = pErr[1];
		unsigned int status = ISP_RD32(addr);

		if (status & 0x0000FFFF) {
			ISP_WR32(0xF5004160, pErr[2]);
			addr = pErr[1];

			LOG_DBG(
			"(0x%08x, 0x%08x), dbg(0x%08x, 0x%08x)",
			addr, status, ISP_RD32(0xF5004160),
			ISP_RD32(0xF5004164));
		}
#endif
		pErr = pErr + 3;
	}

}
#endif


/*************************************************
 *
 *************************************************/
static void ISP_Irq_FrmB(unsigned int *IrqStatus)
{
	/* printk("+ ===== ISP_Irq =====\n"); */

/* LOG_DBG("- E."); */
	/*unsigned int i;*/
	/* unsigned int result=0x0; */
	/*unsigned int j = 0;*/
	/* signed int  idx=0; */
/* unsigned int IrqStatus[ISP_IRQ_TYPE_AMOUNT]={0}; */
	/* unsigned int IrqStatus_fbc_int; */
	union CQ_RTBC_FBC p1_fbc[2];
	unsigned int curr_pa[2];	/* debug only at sof */
	unsigned int cur_v_cnt = 0;
	/*unsigned int d_cur_v_cnt = 0;*/


#if 1
#define STATUSX_WARNING (ISP_IRQ_INT_STATUS_IMGO_ERR_ST \
|ISP_IRQ_INT_STATUS_AAO_ERR_ST|ISP_IRQ_INT_STATUS_IMG2O_ERR_ST| \
ISP_IRQ_INT_STATUS_ESFKO_ERR_ST| \
ISP_IRQ_INT_STATUS_FLK_ERR_ST|ISP_IRQ_INT_STATUS_LSC_ERR_ST)

	if (IrqStatus[ISP_IRQ_TYPE_INTX] & IspInfo_FrmB.
		IrqInfo.ErrMask[ISP_IRQ_TYPE_INTX]) {
			g_bDmaERR_p1 = MTRUE;
			g_bDmaERR_deepDump = MFALSE;
			ISP_DumpDmaDeepDbg();

		/* mark, can ignor fifo may overrun if dma_err isn't pulled. */
		if (IrqStatus[ISP_IRQ_TYPE_INTX] & STATUSX_WARNING) {
			bClrErrRecord = MTRUE;
			LOG_INF("warning: fifo may overrun");
		}

		if (IrqStatus[ISP_IRQ_TYPE_INTX] & IspInfo_FrmB.
			IrqInfo.ErrMask[ISP_IRQ_TYPE_INTX]) {
			LOG_ERR("ISP INT ERR_P1 0x%x\n",
			IrqStatus[ISP_IRQ_TYPE_INTX]);
			ISP_chkModuleSetting();
			g_ISPIntErr[_IRQ] |= IrqStatus[ISP_IRQ_TYPE_INTX];
		}
	}

	if ((IrqStatus[ISP_IRQ_TYPE_INT] & ISP_IRQ_INT_STATUS_PASS1_TG1_DON_ST)
	    || (IrqStatus[ISP_IRQ_TYPE_INT] & ISP_IRQ_INT_STATUS_SOF1_INT_ST))
		cur_v_cnt = ((ISP_RD32(ISP_REG_ADDR_TG_INTER_ST)
		& 0x00FF0000) >> 16);

	if ((IrqStatus[ISP_IRQ_TYPE_INT] & ISP_IRQ_INT_STATUS_PASS1_TG1_DON_ST)
	    && (IrqStatus[ISP_IRQ_TYPE_INT] & ISP_IRQ_INT_STATUS_SOF1_INT_ST)) {
		if (cur_v_cnt != sof_count[_PASS1])
			LOG_ERR("isp sof_don block, %d_%d\n",
			cur_v_cnt, sof_count[_PASS1]);
	}
#endif

	if (IrqStatus[ISP_IRQ_TYPE_INT] & ISP_IRQ_INT_STATUS_VS1_ST) {
		if (IspInfo_FrmB.DebugMask & ISP_DBG_INT)
			t_SOF = 0;
	}
	/* service pass1_done first once if
	 * SOF/PASS1_DONE are coming together.
	 */
	/* get time stamp */
	/* push hw filled buffer to sw list */
	/* LOG_INF("RTBC_DBG %x_%x\n",IrqStatus
	 * [ISP_IRQ_TYPE_INT_P1_ST],
	 * IrqStatus[ISP_IRQ_TYPE_INT_STATUSX]);
	 */
	spin_lock(&(IspInfo_FrmB.SpinLockIrq[_IRQ]));
	/* move into spinlock for protecting
	 * "m_P1_RCNT_INC_CNT" & fbc_cnt racing issue.
	 */
	p1_fbc[0].Reg_val = ISP_RD32(ISP_REG_ADDR_IMGO_FBC);
	p1_fbc[1].Reg_val = ISP_RD32(ISP_REG_ADDR_IMG2O_FBC);
	curr_pa[0] = ISP_RD32(ISP_REG_ADDR_IMGO_BASE_ADDR);
	curr_pa[1] = ISP_RD32(ISP_REG_ADDR_IMG2O_BASE_ADDR);

	if (IrqStatus[ISP_IRQ_TYPE_INT] & ISP_IRQ_INT_STATUS_PASS1_TG1_DON_ST) {
#ifdef _rtbc_buf_que_2_0_
		unsigned long long sec;
		unsigned long usec;

		sec = cpu_clock(0);	/* ns */
		do_div(sec, 1000);	/* usec */
		usec = do_div(sec, 1000000);	/* sec and usec */
		/* update pass1 done time stamp for eis user
		 * (need match with the time stamp in image header)
		 */
		IspInfo_FrmB.IrqInfo.LastestSigTime_usec[ISP_IRQ_TYPE_INT][10] =
		    (unsigned int)(usec);
		IspInfo_FrmB.IrqInfo.LastestSigTime_sec[
		ISP_IRQ_TYPE_INT][10] = (unsigned int)(sec);
		gEismetaInSOF = 0;
		/* time stamp move to sof */
		ISP_DONE_Buf_Time_FrmB(_IRQ, 0, 0, p1_fbc);

		if (IspInfo_FrmB.DebugMask & ISP_DBG_INT) {
			IRQ_LOG_KEEPER(_IRQ, m_CurrentPPB, _LOG_INF,
			"P1_DON_%d(0x%x,0x%x, D_%d(%d/%d)_Filled(%d_%d_%d),D_%d(%d/%d)_Filled(%d_%d_%d) )\n",
			(sof_count[_PASS1]) ? (sof_count[_PASS1] -
			1) : (sof_count[_PASS1]),
			(unsigned int)p1_fbc[0].Reg_val,
			(unsigned int)p1_fbc[1].Reg_val,
			_imgo_,
			pstRTBuf_FrmB->ring_buf[_imgo_].start,
			pstRTBuf_FrmB->ring_buf[_imgo_].read_idx,
			pstRTBuf_FrmB->ring_buf[_imgo_].data[0].bFilled,
			pstRTBuf_FrmB->ring_buf[_imgo_].data[1].bFilled,
			pstRTBuf_FrmB->ring_buf[_imgo_].data[2].bFilled,
			_img2o_,
			pstRTBuf_FrmB->ring_buf[_img2o_].start,
			pstRTBuf_FrmB->ring_buf[_img2o_].read_idx,
			pstRTBuf_FrmB->ring_buf[_img2o_].data[0].bFilled,
			pstRTBuf_FrmB->ring_buf[_img2o_].data[1].bFilled,
			pstRTBuf_FrmB->ring_buf[_img2o_].data[2].bFilled);
		}
#else
#if defined(_rtbc_use_cq0c_)
		if (IspInfo_FrmB.DebugMask & ISP_DBG_INT) {
			IRQ_LOG_KEEPER(_IRQ, m_CurrentPPB, _LOG_INF,
			"P1_DON_%d(0x%x,0x%x, D_%d(%d/%d)_Filled(%d_%d_%d),D_%d(%d/%d)_Filled(%d_%d_%d) )\n",
			(sof_count[_PASS1]) ? (sof_count[_PASS1] -
			1) : (sof_count[_PASS1]),
			(unsigned int)p1_fbc[0].Reg_val,
			(unsigned int)p1_fbc[1].Reg_val,
			_imgo_,
			pstRTBuf_FrmB->ring_buf[_imgo_].start,
			pstRTBuf_FrmB->ring_buf[_imgo_].read_idx,
			pstRTBuf_FrmB->ring_buf[_imgo_].data[0].bFilled,
			pstRTBuf_FrmB->ring_buf[_imgo_].data[1].bFilled,
			pstRTBuf_FrmB->ring_buf[_imgo_].data[2].bFilled,
			_img2o_,
			pstRTBuf_FrmB->ring_buf[_img2o_].start,
			pstRTBuf_FrmB->ring_buf[_img2o_].read_idx,
			pstRTBuf_FrmB->ring_buf[_img2o_].data[0].bFilled,
			pstRTBuf_FrmB->ring_buf[_img2o_].data[1].bFilled,
			pstRTBuf_FrmB->ring_buf[_img2o_].data[2].bFilled);
		}
#else
		/* LOG_DBG("[k_js_test]Pass1_done(0x%x)",
		 * IrqStatus[ISP_IRQ_TYPE_INT]);
		 */
		unsigned long long sec;
		unsigned long usec;

		sec = cpu_clock(0);	/* ns */
		do_div(sec, 1000);	/* usec */
		usec = do_div(sec, 1000000);	/* sec and usec */


		ISP_DONE_Buf_Time_FrmB(_IRQ, sec, usec, p1_fbc);
		/*Check Timesamp reverse */
		/* what's this? */
		/*  */
#endif
#endif
	}
	/* switch pass1 WDMA buffer */
	/* fill time stamp for cq0c */
	if (IrqStatus[ISP_IRQ_TYPE_INT] & ISP_IRQ_INT_STATUS_SOF1_INT_ST) {

		unsigned int _dmaport = 0;
#if CONFIG_K_FOR_SYSTRACE
		char strName[128];

		sprintf(strName, "TAG_K_SOF (%d)", (sof_count[_PASS1] + 1));
		_kernel_trace_begin(strName);
#endif

		if (IspInfo_FrmB.DebugMask & ISP_DBG_INT) {
			/*struct timeval time_sof;*/
			unsigned long long time_sof_sec;
			unsigned long time_sof_usec;

			time_sof_sec = cpu_clock(0);	/* ns */
			do_div(time_sof_sec, 1000);	/* usec */
			time_sof_usec = do_div(time_sof_sec,
			1000000);	/* sec and usec */
			t_SOF = time_sof_sec * 1000000 + time_sof_usec;
		}


		if (pstRTBuf_FrmB->ring_buf[_imgo_].active)
			_dmaport = 0;
		else if (pstRTBuf_FrmB->ring_buf[_img2o_].active)
			_dmaport = 1;
		else
			LOG_ERR("no main dma port opened at SOF\n");
		/* chk this frame have EOF or not, dynimic dma port chk */
		if (p1_fbc[_dmaport].Bits.FB_NUM ==
			p1_fbc[_dmaport].Bits.FBC_CNT) {
			sof_pass1done[0] = 1;
#ifdef _rtbc_buf_que_2_0_
			/* ISP_LostP1Done_ErrHandle(_imgo_); */
			/* ISP_LostP1Done_ErrHandle(_img2o_); */
			/* IRQ_LOG_KEEPER(_IRQ,m_CurrentPPB,_LOG_INF,
			 * "lost p1Done ErrHandle\n");
			 */
#endif
			IRQ_LOG_KEEPER(_IRQ, m_CurrentPPB, _LOG_INF,
			"Lost p1 done_%d (0x%x): ",
			sof_count[_PASS1], cur_v_cnt);
		} else {
			if (bClrErrRecord) {
				/* error case */
				sof_pass1done[0] = 0xF;
				bClrErrRecord = MFALSE;
				LOG_ERR("int_err happened before this SOF\n");
			}
			else{/* no error */
				sof_pass1done[0] = 0;
				if (p1_fbc[_dmaport].Bits.FB_NUM ==
					(p1_fbc[_dmaport].Bits.FBC_CNT + 1))
					sof_pass1done[0] = 2;
			}
		}
#ifdef _rtbc_buf_que_2_0_
		{
			unsigned int z;

			if (mFwRcnt.bLoadBaseAddr[_IRQ] == 1) {
				if (pstRTBuf_FrmB->ring_buf[_imgo_].active) {
					IRQ_LOG_KEEPER(_IRQ, m_CurrentPPB,
					_LOG_INF, " p1_%d:wr2Phy_0x%x: ",
					_imgo_, pstRTBuf_FrmB->
					ring_buf[_imgo_].data[
					pstRTBuf_FrmB->ring_buf[_imgo_].
					start].base_pAddr);
					ISP_WR32(ISP_REG_ADDR_IMGO_BASE_ADDR,
					pstRTBuf_FrmB->ring_buf[_imgo_].
					data[pstRTBuf_FrmB->ring_buf[
					_imgo_].start].base_pAddr);
				}
				if (pstRTBuf_FrmB->ring_buf[_img2o_].active) {
					IRQ_LOG_KEEPER(_IRQ, m_CurrentPPB,
					_LOG_INF, " p1_%d:wr2Phy_0x%x: ",
					_img2o_, pstRTBuf_FrmB->
					ring_buf[_img2o_].data[
					pstRTBuf_FrmB->ring_buf[_img2o_].start].
					base_pAddr);
					ISP_WR32(ISP_REG_ADDR_IMG2O_BASE_ADDR,
					pstRTBuf_FrmB->ring_buf[_img2o_].
					data[pstRTBuf_FrmB->ring_buf[
					_img2o_].start].base_pAddr);
				}
				mFwRcnt.bLoadBaseAddr[_IRQ] = 0;
			}
			/* equal case is for clear curidx */
			for (z = 0; z <= mFwRcnt.curIdx[_IRQ]; z++) {
/* LOG_INF("curidx:%d\n",mFwRcnt.curIdx[_IRQ]); */
				if (mFwRcnt.INC[_IRQ][z] == 1) {
					mFwRcnt.INC[_IRQ][z] = 0;
					p1_fbc[0].Bits.RCNT_INC = 1;
					ISP_WR32(ISP_REG_ADDR_IMGO_FBC,
					p1_fbc[0].Reg_val);
					p1_fbc[1].Bits.RCNT_INC = 1;
					ISP_WR32(ISP_REG_ADDR_IMG2O_FBC,
					p1_fbc[1].Reg_val);
					if (IspInfo_FrmB.DebugMask &
						ISP_DBG_INT)
						IRQ_LOG_KEEPER(_IRQ,
						m_CurrentPPB,
						_LOG_INF, " p1:RCNT_INC: ");
#ifdef _89SERIAL_
					++m_P1_RCNT_INC_CNT;
#endif
				} else {
/* LOG_INF("RTBC_DBG:%d %d %d %d %d %d %d %d %d %d",
 * mFwRcnt.INC[_IRQ][0],mFwRcnt.INC[_IRQ][1],
 * mFwRcnt.INC[_IRQ][2],mFwRcnt.INC[_IRQ][3],
 * mFwRcnt.INC[_IRQ][4],\
 */
/* mFwRcnt.INC[_IRQ][5],mFwRcnt.INC[_IRQ][6],
 * mFwRcnt.INC[_IRQ][7],mFwRcnt.INC[_IRQ][8],
 * mFwRcnt.INC[_IRQ][9]);
 */
					mFwRcnt.curIdx[_IRQ] = 0;
					break;
				}
			}
		}
#endif
		if (IspInfo_FrmB.DebugMask & ISP_DBG_INT) {
			/* can chk fbc status compare to p1_fbc.
			 * (the difference is the timing of reading)
			 */
			union CQ_RTBC_FBC _fbc_chk[2];
			/* in order to log newest fbc condition */
			_fbc_chk[0].Reg_val = ISP_RD32(ISP_REG_ADDR_IMGO_FBC);
			_fbc_chk[1].Reg_val = ISP_RD32(ISP_REG_ADDR_IMG2O_FBC);
			IRQ_LOG_KEEPER(_IRQ, m_CurrentPPB, _LOG_INF,
			"P1_SOF_%d_%d(0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x, D_%d(%d/%d)_Filled(%d_%d_%d),D_%d(%d/%d)_Filled(%d_%d_%d) )\n",
			sof_count[_PASS1], cur_v_cnt,
			(unsigned int)_fbc_chk[0].Reg_val,
			(unsigned int)_fbc_chk[1].Reg_val,
			ISP_RD32(ISP_REG_ADDR_IMGO_BASE_ADDR),
			ISP_RD32(ISP_REG_ADDR_IMG2O_BASE_ADDR),
			ISP_RD32(ISP_REG_ADDR_IMGO_YSIZE),
			ISP_RD32(ISP_REG_ADDR_IMG2O_YSIZE),
			ISP_RD32(ISP_REG_ADDR_TG_MAGIC_0),
			ISP_RD32(ISP_REG_ADDR_CQ0_CUR_BASE_ARRR),
			_imgo_,
			pstRTBuf_FrmB->ring_buf[_imgo_].start,
			pstRTBuf_FrmB->ring_buf[_imgo_].read_idx,
			pstRTBuf_FrmB->ring_buf[_imgo_].data[0].bFilled,
			pstRTBuf_FrmB->ring_buf[_imgo_].data[1].bFilled,
			pstRTBuf_FrmB->ring_buf[_imgo_].data[2].bFilled,
			_img2o_,
			pstRTBuf_FrmB->ring_buf[_img2o_].start,
			pstRTBuf_FrmB->ring_buf[_img2o_].read_idx,
			pstRTBuf_FrmB->ring_buf[_img2o_].data[0].bFilled,
			pstRTBuf_FrmB->ring_buf[_img2o_].data[1].bFilled,
			pstRTBuf_FrmB->ring_buf[_img2o_].data[2].bFilled);
		}
		{
			unsigned long long sec;
			unsigned long usec;
			ktime_t time;

			time = ktime_get();	/* ns */
			sec = time;/*.tv64;*/
#ifdef T_STAMP_2_0
			if (g1stSof[_IRQ] == MTRUE)
				m_T_STAMP.T_ns = sec;
			if (m_T_STAMP.fps > SlowMotion) {
				m_T_STAMP.fcnt++;
				if (g1stSof[_IRQ] == MFALSE) {
					m_T_STAMP.T_ns +=
					((unsigned long long)m_T_STAMP.
					interval_us * 1000);
					if (m_T_STAMP.fcnt == m_T_STAMP.fps) {
						m_T_STAMP.fcnt = 0;
						m_T_STAMP.T_ns +=
						((unsigned long long)m_T_STAMP.
						compensation_us * 1000);
					}
				}
				sec = m_T_STAMP.T_ns;
			}
#endif
			do_div(sec, 1000);	/* usec */
			usec = do_div(sec, 1000000);	/* sec and usec */

/* update SOF time stamp for eis user
 * (need match with the time stamp in image header)
 */
			if ((gEismetaWIdx < 0) || (gEismetaWIdx >= EISMETA_RINGSIZE)) {
				LOG_DBG("Error: Invalid Index ,gEismetaWIdx(%d)",gEismetaWIdx);
				return;
			}
			IspInfo_FrmB.IrqInfo.LastestSigTime_usec[
			ISP_IRQ_TYPE_INT][12] =
			    (unsigned int)(usec);
			IspInfo_FrmB.IrqInfo.LastestSigTime_sec[
			ISP_IRQ_TYPE_INT][12] =
			    (unsigned int)(sec);
			IspInfo_FrmB.IrqInfo.Eismeta[
			ISP_IRQ_TYPE_INT][gEismetaWIdx].
			    tLastSOF2P1done_sec = (unsigned int)(sec);
			IspInfo_FrmB.IrqInfo.Eismeta[
			ISP_IRQ_TYPE_INT][gEismetaWIdx].
			    tLastSOF2P1done_usec = (unsigned int)(usec);
			gEismetaInSOF = 1;
			gEismetaWIdx = ((gEismetaWIdx + 1) % EISMETA_RINGSIZE);

			if (sof_pass1done[0] == 1)
				ISP_SOF_Buf_Get_FrmB(_IRQ, sec,
				usec, MTRUE, p1_fbc, curr_pa);
			else
				ISP_SOF_Buf_Get_FrmB(_IRQ, sec,
				usec, MFALSE, p1_fbc, curr_pa);
		}
#if CONFIG_K_FOR_SYSTRACE
		_kernel_trace_end();
#endif
	}
	spin_unlock(&(IspInfo_FrmB.SpinLockIrq[_IRQ]));

	/* dump log during spin lock */
#ifdef ISR_LOG_ON
	IRQ_LOG_PRINTER(_IRQ, m_CurrentPPB, _LOG_INF);
	/* IRQ_LOG_PRINTER(_IRQ,m_CurrentPPB,_LOG_ERR); */

	IRQ_LOG_PRINTER(_IRQ_D, m_CurrentPPB, _LOG_INF);
	/* IRQ_LOG_PRINTER(_IRQ_D,m_CurrentPPB,_LOG_ERR); */
#endif
}

/*************************************************
 *
 *************************************************/
static void ISP_BufWrite_Free_FrmB(void)
{
	unsigned int i;
	/*  */
	if (IspInfo_FrmB.DebugMask & ISP_DBG_BUF_WRITE)
		LOG_DBG("- E.");
	/*  */
	for (i = 0; i < ISP_BUF_WRITE_AMOUNT; i++) {
		IspInfo_FrmB.BufInfo.Write[i].Status =
		ISP_BUF_STATUS_EMPTY_FRMB;
		IspInfo_FrmB.BufInfo.Write[i].Size = 0;
		if (IspInfo_FrmB.BufInfo.Write[i].pData != NULL) {
			kfree(IspInfo_FrmB.BufInfo.Write[i].pData);
			IspInfo_FrmB.BufInfo.Write[i].pData = NULL;
		}
	}
}

/*************************************************
 *
 *************************************************/
static bool ISP_BufWrite_Alloc_FrmB(void)
{
	unsigned int i;
	/*  */
	if (IspInfo_FrmB.DebugMask & ISP_DBG_BUF_WRITE)
		LOG_DBG("- E.");
	/*  */
	for (i = 0; i < ISP_BUF_WRITE_AMOUNT; i++) {
		IspInfo_FrmB.BufInfo.Write[i].Status =
		ISP_BUF_STATUS_EMPTY_FRMB;
		IspInfo_FrmB.BufInfo.Write[i].Size = 0;
		IspInfo_FrmB.BufInfo.Write[i].pData =
		    kmalloc(ISP_BUF_SIZE_WRITE, GFP_ATOMIC);
		if (IspInfo_FrmB.BufInfo.Write[i].pData == NULL) {
			LOG_DBG("ERROR: i = %d, pData is NULL", i);
			ISP_BufWrite_Free_FrmB();
			return false;
		}
	}
	return true;
}


/*************************************************
 *
 *************************************************/
static signed int ISP_open_FrmB(void)
{
	signed int Ret = 0;
	unsigned int i;
	int q = 0, p = 0;
	/*  */
	for (i = 0; i < IRQ_USER_NUM_MAX; i++) {
		FirstUnusedIrqUserKey = 1;
		memset((void *)IrqUserKey_UserInfo[i].userName,
		'\0', USERKEY_STR_LEN);
		IrqUserKey_UserInfo[i].userKey = -1;
	}
	/*  */
	EDBufQueRemainNodeCnt = 0;
	P2_Support_BurstQNum = 1;
	/*  */
	for (i = 0; i < _MAX_SUPPORT_P2_FRAME_NUM_; i++) {
		P2_EDBUF_RingList[i].processID = 0x0;
		P2_EDBUF_RingList[i].callerID = 0x0;
		P2_EDBUF_RingList[i].p2dupCQIdx = -1;
		P2_EDBUF_RingList[i].bufSts = ISP_ED_BUF_STATE_NONE;
	}
	P2_EDBUF_RList_FirstBufIdx = 0;
	P2_EDBUF_RList_CurBufIdx = 0;
	P2_EDBUF_RList_LastBufIdx = -1;
	/*  */
	for (i = 0; i < _MAX_SUPPORT_P2_PACKAGE_NUM_; i++) {
		P2_EDBUF_MgrList[i].processID = 0x0;
		P2_EDBUF_MgrList[i].callerID = 0x0;
		P2_EDBUF_MgrList[i].p2dupCQIdx = -1;
		P2_EDBUF_MgrList[i].dequedNum = 0;
	}
	P2_EDBUF_MList_FirstBufIdx = 0;
	P2_EDBUF_MList_LastBufIdx = -1;
	/*  */
	IspInfo_FrmB.BufInfo.Read.pData =
	kmalloc(ISP_BUF_SIZE, GFP_ATOMIC);
	IspInfo_FrmB.BufInfo.Read.Size = ISP_BUF_SIZE;
	IspInfo_FrmB.BufInfo.Read.Status = ISP_BUF_STATUS_EMPTY_FRMB;
	if (IspInfo_FrmB.BufInfo.Read.pData == NULL) {
		LOG_DBG("ERROR: BufRead kmalloc failed");
		Ret = -ENOMEM;
		goto EXIT;
	}
	/*  */
	if (!ISP_BufWrite_Alloc_FrmB()) {
		LOG_DBG("ERROR: BufWrite kmalloc failed");
		Ret = -ENOMEM;
		goto EXIT;
	}
	/*  */
	for (q = 0; q < IRQ_USER_NUM_MAX; q++) {
		for (i = 0; i < ISP_IRQ_TYPE_AMOUNT_FRMB; i++) {
			IspInfo_FrmB.IrqInfo.Status[q][i] = 0;
			IspInfo_FrmB.IrqInfo.MarkedFlag[q][i] = 0;
			for (p = 0; p < 32; p++) {
				IspInfo_FrmB.IrqInfo.
				MarkedTime_usec[q][i][p] = 0;
				IspInfo_FrmB.IrqInfo.
				PassedBySigCnt[q][i][p] = 0;
				IspInfo_FrmB.IrqInfo.
				LastestSigTime_usec[i][p] = 0;
			}
			if (i < ISP_IRQ_TYPE_INTB) {
				for (p = 0; p < EISMETA_RINGSIZE; p++) {
					IspInfo_FrmB.IrqInfo.Eismeta[i][p].
					tLastSOF2P1done_sec = 0;
					IspInfo_FrmB.IrqInfo.Eismeta[i][p].
					tLastSOF2P1done_usec = 0;
				}
			}
		}
	}
	gEismetaRIdx = 0;
	gEismetaWIdx = 0;
	gEismetaInSOF = 0;

#ifdef KERNEL_LOG
	IspInfo_FrmB.DebugMask = (ISP_DBG_INT |
	ISP_DBG_BUF_CTRL | ISP_DBG_INT_2);
#endif
	/*  */
EXIT:
	if (Ret < 0) {
		if (IspInfo_FrmB.BufInfo.Read.pData != NULL) {
			kfree(IspInfo_FrmB.BufInfo.Read.pData);
			IspInfo_FrmB.BufInfo.Read.pData = NULL;
		}
		/*  */
		ISP_BufWrite_Free_FrmB();
	}
	return Ret;
}

/*************************************************
 *
 *************************************************/
static signed int ISP_release_FrmB(void)
{

	/*unsigned int Reg;*/
	/* reason of close vf is to make sure camera can
	 * serve regular after previous abnormal exit
	 */
	/* Reg = ISP_RD32(ISP_REG_ADDR_TG_VF_CON); */
	/* Reg &= 0xfffffffE;//close Vfinder */
	/* ISP_WR32(ISP_REG_ADDR_TG_VF_CON,Reg); */

	/* Reg = ISP_RD32(ISP_REG_ADDR_TG2_VF_CON); */
	/* Reg &= 0xfffffffE;//close Vfinder */
	/* ISP_WR32(ISP_REG_ADDR_TG2_VF_CON,Reg); */
	unsigned int i = 0;
	/* reset */
	/*  */
	for (i = 0; i < IRQ_USER_NUM_MAX; i++) {
		FirstUnusedIrqUserKey = 1;
		memset((void *)IrqUserKey_UserInfo[i].userName,
		'\0', USERKEY_STR_LEN);
		IrqUserKey_UserInfo[i].userKey = -1;
	}

	if (IspInfo_FrmB.BufInfo.Read.pData != NULL) {
		kfree(IspInfo_FrmB.BufInfo.Read.pData);
		IspInfo_FrmB.BufInfo.Read.pData = NULL;
		IspInfo_FrmB.BufInfo.Read.Size = 0;
		IspInfo_FrmB.BufInfo.Read.Status = ISP_BUF_STATUS_EMPTY_FRMB;
	}
	/*  */
	ISP_BufWrite_Free_FrmB();
	/*  */
/*EXIT:*/
	return 0;
}

/*************************************************
 *
 *************************************************/

static signed int __init ISP_Init_FrmB(void)
{
	signed int Ret = 0, j;
	void *tmp;

	int i;
	/*  */
	LOG_DBG("- E.");
	/*  */
	/* for kernel coding rule: ERROR:INITIALISED_STATIC:
	 * do not initialise statics to 0 or NULL
	 */
	pstRTBuf_FrmB = NULL;
	/*  */
	/* allocate a memory area with kmalloc.
	 * Will be rounded up to a page boundary
	 */
	/* RT_BUF_TBL_NPAGES*4096(1page) = 64k Bytes */

	if (sizeof(struct ISP_RT_BUF_STRUCT_FRMB) >
		((RT_BUF_TBL_NPAGES) * PAGE_SIZE)) {
		i = 0;
		while (i < sizeof(struct ISP_RT_BUF_STRUCT_FRMB))
			i += PAGE_SIZE;

		pBuf_kmalloc = kmalloc(i + 2 * PAGE_SIZE, GFP_KERNEL);
		if ((pBuf_kmalloc) == NULL) {
			LOG_ERR("mem not enough\n");
			return -ENOMEM;
		}
		memset(pBuf_kmalloc, 0x00, i * PAGE_SIZE);
	} else {
		pBuf_kmalloc =
		     kmalloc((RT_BUF_TBL_NPAGES + 2) * PAGE_SIZE, GFP_KERNEL);
		if ((pBuf_kmalloc) == NULL) {
			LOG_ERR("mem not enough\n");
			return -ENOMEM;
		}
		memset(pBuf_kmalloc, 0x00, RT_BUF_TBL_NPAGES * PAGE_SIZE);
	}
	/* round it up to the page bondary */
	pTbl_RTBuf = (int *)((((unsigned long)pBuf_kmalloc)
	+ PAGE_SIZE - 1) & PAGE_MASK);
	pstRTBuf_FrmB = (struct ISP_RT_BUF_STRUCT_FRMB *) pTbl_RTBuf;
	pstRTBuf_FrmB->state = ISP_RTBC_STATE_INIT;

	/* isr log */
	if (PAGE_SIZE < ((_IRQ_MAX * NORMAL_STR_LEN *
		((DBG_PAGE + INF_PAGE + ERR_PAGE) + 1)) *
		LOG_PPNUM)) {
		i = 0;
		while (i < ((_IRQ_MAX * NORMAL_STR_LEN *
		((DBG_PAGE + INF_PAGE + ERR_PAGE) + 1)) *
			LOG_PPNUM)) {
			i += PAGE_SIZE;
		}
	} else {
		i = PAGE_SIZE;
	}
	pLog_kmalloc = kmalloc(i, GFP_KERNEL);
	if ((pLog_kmalloc) == NULL) {
		LOG_ERR("mem not enough\n");
		return -ENOMEM;
	}
	memset(pLog_kmalloc, 0x00, i);
	tmp = pLog_kmalloc;
	for (i = 0; i < LOG_PPNUM; i++) {
		for (j = 0; j < _IRQ_MAX; j++) {
			gSvLog[j]._str[i][_LOG_DBG] = (char *)tmp;
			/* tmp = (void*) ((unsigned int)tmp +
			 * (NORMAL_STR_LEN*DBG_PAGE));
			 */
			tmp = (void *)((char *)tmp +
			(NORMAL_STR_LEN * DBG_PAGE));
			gSvLog[j]._str[i][_LOG_INF] = (char *)tmp;
			/* tmp = (void*) ((unsigned int)tmp +
			 * (NORMAL_STR_LEN*INF_PAGE));
			 */
			tmp = (void *)((char *)tmp +
			(NORMAL_STR_LEN * INF_PAGE));
			gSvLog[j]._str[i][_LOG_ERR] = (char *)tmp;
			/* tmp = (void*) ((unsigned int)tmp +
			 * (NORMAL_STR_LEN*ERR_PAGE));
			 */
			tmp = (void *)((char *)tmp +
			(NORMAL_STR_LEN * ERR_PAGE));
		}
/* tmp = (void*) ((unsigned int)tmp + NORMAL_STR_LEN);
 * log buffer ,in case of overflow
 */
/* log buffer ,in case of overflow */
		tmp = (void *)((char *)tmp + NORMAL_STR_LEN);
	}
	/* mark the pages reserved , FOR MMAP */
	for (i = 0; i < RT_BUF_TBL_NPAGES * PAGE_SIZE; i += PAGE_SIZE)
		SetPageReserved(virt_to_page(((unsigned long)pTbl_RTBuf) + i));

#ifdef _MAGIC_NUM_ERR_HANDLING_
	LOG_DBG("init m_LastMNum");
	for (i = 0; i < _rt_dma_max_; i++)
		m_LastMNum[i] = 0;
#endif

	LOG_DBG("- X. Ret: %d.", Ret);
	return Ret;
}

/*************************************************
 *
 *************************************************/
static void __exit ISP_Exit_FRMB(void)
{
	int i;

	LOG_DBG("- E.");
	/*  */

	/* unreserve the pages */
	for (i = 0; i < RT_BUF_TBL_NPAGES * PAGE_SIZE; i += PAGE_SIZE)
		SetPageReserved(virt_to_page(((unsigned long)pTbl_RTBuf) + i));
	/* free the memory areas */
	kfree(pBuf_kmalloc);
	kfree(pLog_kmalloc);
	/*  */
	LOG_DBG("- X.");
}

/*---------------------------------------------------------------------------*/
