// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/syscore_ops.h>
#include <linux/sched_clock.h>
#include <linux/irqreturn.h>
#include <linux/jiffies.h>
#include <linux/clockchips.h>
#include <linux/clocksource.h>
#include <linux/io.h>

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/sched_clock.h>

#include <linux/version.h>
#include <linux/slab.h>

#include <mach/mt_gpt.h>
#if defined(CONFIG_MACH_MT6735M) || defined (CONFIG_MACH_MT6735) || defined(CONFIG_MACH_MT6753)
#include <mach/mt_cpuxgpt.h>
#endif
#include <mt-plat/sync_write.h>

#define GPT_CLKEVT_ID				(GPT1)
#define GPT_CLKSRC_ID       (GPT2)
#define GPT_SYSCNT_ID       (GPT6)

#define AP_XGPT_BASE		xgpt_timers.tmr_regs
#define GPT_IRQEN           (AP_XGPT_BASE + 0x0000)
#define GPT_IRQSTA          (AP_XGPT_BASE + 0x0004)
#define GPT_IRQACK          (AP_XGPT_BASE + 0x0008)
#define GPT1_BASE           (AP_XGPT_BASE + 0x0010)

#define GPT_CON             (0x00)
#define GPT_CLK             (0x04)
#define GPT_CNT             (0x08)
#define GPT_CMP             (0x0C)
#define GPT_CNTH            (0x18)
#define GPT_CMPH            (0x1C)

#define GPT_CON_ENABLE      (0x1 << 0)
#define GPT_CON_CLRCNT      (0x1 << 1)
#define GPT_CON_OPMODE      (0x3 << 4)

#define GPT_OPMODE_MASK     (0x3)
#define GPT_CLKDIV_MASK     (0xf)
#define GPT_CLKSRC_MASK     (0x1)

#define GPT_OPMODE_OFFSET   (4)
#define GPT_CLKSRC_OFFSET   (4)

#define GPT_FEAT_64_BIT     (0x0001)
#define GPT_ISR             (0x0010)
#define GPT_IN_USE          (0x0100)

/************define this for 32/64 compatible**************/
#define GPT_BIT_MASK_L 0x00000000FFFFFFFF
#define GPT_BIT_MASK_H 0xFFFFFFFF00000000
/****************************************************/

struct mt_xgpt_timers {
	int tmr_irq;
	void __iomem *tmr_regs;
};

struct gpt_device {
	unsigned int id;
	unsigned int mode;
	unsigned int clksrc;
	unsigned int clkdiv;
	unsigned int cmp[2];
	void (*func)(unsigned long);
	int flags;
	int features;
	void __iomem *base_addr;
};

static struct mt_xgpt_timers xgpt_timers;
static struct gpt_device gpt_devs[NR_GPTS];

static DEFINE_SPINLOCK(gpt_lock);

/*
 * Return GPT4 count(before init clear) to record
 * kernel start time between LK and kernel
 */

/* 1000000 / 76.92ns = 13000.520 */
#define GPT4_1MS_TICK       ((u32)(13000))
#define GPT4_BASE           (AP_XGPT_BASE + 0x0040)
static unsigned int boot_time_value;

#define mt_gpt_set_reg(val, addr) \
	mt_reg_sync_writel(__raw_readl(addr)|(val), addr)
#define mt_gpt_clr_reg(val, addr) \
	mt_reg_sync_writel(__raw_readl(addr)&~(val), addr)

static unsigned int xgpt_boot_up_time(void)
{
	unsigned int tick;

	tick = __raw_readl(GPT4_BASE + GPT_CNT);
	return ((tick + (GPT4_1MS_TICK - 1)) / GPT4_1MS_TICK);
}
/*********************************************************/

static struct gpt_device *id_to_dev(unsigned int id)
{
	return id < NR_GPTS ? gpt_devs + id : NULL;
}

#define gpt_update_lock(flags)		spin_lock_irqsave(&gpt_lock, flags)
#define gpt_update_unlock(flags)	spin_unlock_irqrestore(&gpt_lock, flags)

static inline void noop(unsigned long data) { }
static void (*handlers[]) (unsigned long) = {
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
};

static struct tasklet_struct task[NR_GPTS];
static void task_sched(unsigned long data)
{
	unsigned int id = (unsigned int)data;

	tasklet_schedule(&task[id]);
}

static irqreturn_t gpt_handler(int irq, void *dev_id);
static u64 mt_gpt_read(struct clocksource *cs);
static int mt_gpt_clkevt_next_event(unsigned long cycles,
				   struct clock_event_device *evt);
static int mt_gpt_clkevt_shutdown(struct clock_event_device *clk);
static int mt_gpt_clkevt_oneshot(struct clock_event_device *clk);
static int mt_gpt_clkevt_resume(struct clock_event_device *clk);
static int mt_gpt_set_periodic(struct clock_event_device *clk);

static struct clocksource gpt_clocksource = {
	.name = "mt6580-gpt",
	.rating = 300,
	.read = mt_gpt_read,
	.mask = CLOCKSOURCE_MASK(32),
	.shift = 25,
	.flags = CLOCK_SOURCE_IS_CONTINUOUS,
};

static struct clock_event_device gpt_clockevent = {
	.name = "mt6580-gpt",
	.features = CLOCK_EVT_FEAT_ONESHOT,
	.shift = 32,
	.rating = 300,
	.set_next_event = mt_gpt_clkevt_next_event,
	.set_state_shutdown = mt_gpt_clkevt_shutdown,
	.set_state_periodic = mt_gpt_set_periodic,
	.set_state_oneshot = mt_gpt_clkevt_oneshot,
	.tick_resume = mt_gpt_clkevt_resume,
};

static struct irqaction gpt_irq = {
	.name = "mt-gpt",
	.flags = IRQF_TIMER | IRQF_IRQPOLL | IRQF_TRIGGER_LOW,
	.handler = gpt_handler,
	.dev_id = &gpt_clockevent,
};

void mt_gpt_clkevt_aee_dump(void)
{
}

static inline unsigned int gpt_get_and_ack_irq(void)
{
	unsigned int id;
	unsigned int mask;
	unsigned int status = __raw_readl(GPT_IRQSTA);

	for (id = GPT1; id < NR_GPTS; id++) {
		mask = 0x1 << id;
		if (status & mask) {
			mt_reg_sync_writel(mask, GPT_IRQACK);
			break;
		}
	}
	return id;
}

static irqreturn_t gpt_handler(int irq, void *dev_id)
{
	unsigned int id = gpt_get_and_ack_irq();
	struct gpt_device *dev = id_to_dev(id);

	if (likely(dev)) {
		if (!(dev->flags & GPT_ISR))
			handlers[id] (id);
		else
			handlers[id] ((unsigned long)dev_id);
	} else
		pr_info("GPT id is %d\n", id);

	return IRQ_HANDLED;
}

static void __gpt_enable_irq(struct gpt_device *dev)
{
	mt_gpt_set_reg(0x1 << (dev->id), GPT_IRQEN);
}

static void __gpt_disable_irq(struct gpt_device *dev)
{
	mt_gpt_clr_reg(0x1 << (dev->id), GPT_IRQEN);
}

static void __gpt_ack_irq(struct gpt_device *dev)
{
	mt_reg_sync_writel(0x1 << (dev->id), GPT_IRQACK);
}

static void __gpt_reset(struct gpt_device *dev)
{
	mt_reg_sync_writel(0x0, dev->base_addr + GPT_CON);
	__gpt_disable_irq(dev);
	__gpt_ack_irq(dev);
	mt_reg_sync_writel(0x0, dev->base_addr + GPT_CLK);
	mt_reg_sync_writel(0x2, dev->base_addr + GPT_CON);
	mt_reg_sync_writel(0x0, dev->base_addr + GPT_CMP);
	if (dev->features & GPT_FEAT_64_BIT)
		mt_reg_sync_writel(0, dev->base_addr + GPT_CMPH);
}

static void __gpt_get_cnt(struct gpt_device *dev, unsigned int *ptr)
{
	*ptr = __raw_readl(dev->base_addr + GPT_CNT);
	if (dev->features & GPT_FEAT_64_BIT)
		*(++ptr) = __raw_readl(dev->base_addr + GPT_CNTH);
}

static void __gpt_get_cmp(struct gpt_device *dev, unsigned int *ptr)
{
	*ptr = __raw_readl(dev->base_addr + GPT_CMP);
	if (dev->features & GPT_FEAT_64_BIT)
		*(++ptr) = __raw_readl(dev->base_addr + GPT_CMPH);
}

static void __gpt_set_mode(struct gpt_device *dev, unsigned int mode)
{
	unsigned int ctl = __raw_readl(dev->base_addr + GPT_CON);

	mode <<= GPT_OPMODE_OFFSET;

	ctl &= ~GPT_CON_OPMODE;
	ctl |= mode;

	mt_reg_sync_writel(ctl, dev->base_addr + GPT_CON);

	dev->mode = mode >> GPT_OPMODE_OFFSET;
}

static void __gpt_set_clk(struct gpt_device *dev,
	unsigned int clksrc, unsigned int clkdiv)
{
	unsigned int clk = (clksrc << GPT_CLKSRC_OFFSET) | clkdiv;

	mt_reg_sync_writel(clk, dev->base_addr + GPT_CLK);

	dev->clksrc = clksrc;
	dev->clkdiv = clkdiv;
}

static void __gpt_set_cmp(struct gpt_device *dev, unsigned int cmpl,
		unsigned int cmph)
{
	mt_reg_sync_writel(cmpl, dev->base_addr + GPT_CMP);
	dev->cmp[0] = cmpl;

	if (dev->features & GPT_FEAT_64_BIT) {
		mt_reg_sync_writel(cmph, dev->base_addr + GPT_CMPH);
		dev->cmp[1] = cmpl;
	}
}

static void __gpt_clrcnt(struct gpt_device *dev)
{
	mt_gpt_set_reg(GPT_CON_CLRCNT, dev->base_addr + GPT_CON);
	while (__raw_readl(dev->base_addr + GPT_CNT))
		cpu_relax();
}

static void __gpt_start(struct gpt_device *dev)
{
	mt_gpt_set_reg(GPT_CON_ENABLE, dev->base_addr + GPT_CON);
}

static void __gpt_wait_clrcnt(void)
{
	/*
	 * if gpt is running in 32K domain, it needs 3T (~90 us) for clearing
	 * old counter.
	 */
	#define WAIT_CLR_CNT_TIME_NS 100000

	uint64_t start_time = 0, end_time = 0;

	start_time = sched_clock();
	end_time = start_time;

	while ((end_time - start_time) < WAIT_CLR_CNT_TIME_NS)
		end_time = sched_clock();
}

static void __gpt_wait_clrcnt_then_start(struct gpt_device *dev)
{
	__gpt_wait_clrcnt();
	__gpt_start(dev);
}

static void __gpt_stop(struct gpt_device *dev)
{
	mt_gpt_clr_reg(GPT_CON_ENABLE, dev->base_addr + GPT_CON);
}
static void __gpt_set_flags(struct gpt_device *dev, unsigned int flags)
{
	dev->flags |= flags;
}

static void __gpt_set_handler(struct gpt_device *dev,
	void (*func)(unsigned long))
{
	if (func) {
		if (dev->flags & GPT_ISR)
			handlers[dev->id] = func;
		else {
			tasklet_init(&task[dev->id], func, 0);
			handlers[dev->id] = task_sched;
		}
	}
	dev->func = func;
}

static void gpt_devs_init(void)
{
	int i;

	for (i = 0; i < NR_GPTS; i++) {
		gpt_devs[i].id = i;
		gpt_devs[i].base_addr = GPT1_BASE + 0x10 * i;
		pr_info("[mtk_gpt] gpt%d, base=0x%lx\n",
			i + 1, (unsigned long)gpt_devs[i].base_addr);
	}

	gpt_devs[GPT6].features |= GPT_FEAT_64_BIT;
}

static void setup_gpt_dev_locked(struct gpt_device *dev, unsigned int mode,
				 unsigned int clksrc, unsigned int clkdiv, unsigned int cmp,
				 void (*func)(unsigned long), unsigned int flags)
{
	__gpt_set_flags(dev, flags | GPT_IN_USE);

	__gpt_set_mode(dev, mode & GPT_OPMODE_MASK);
	__gpt_set_clk(dev, clksrc & GPT_CLKSRC_MASK, clkdiv & GPT_CLKDIV_MASK);

	if (func)
		__gpt_set_handler(dev, func);

	if (dev->mode != GPT_FREE_RUN) {
		__gpt_set_cmp(dev, cmp, 0);
		if (!(dev->flags & GPT_NOIRQEN))
			__gpt_enable_irq(dev);
	}

	if (!(dev->flags & GPT_NOAUTOEN))
		__gpt_start(dev);
}

static int mt_gpt_clkevt_next_event(unsigned long cycles,
				   struct clock_event_device *evt)
{
	struct gpt_device *dev = id_to_dev(GPT_CLKEVT_ID);

	/*
	 * disable irq first because we do not expect interrupt is triggered
	 * by old compare value.
	 */
	__gpt_disable_irq(dev);

	/*
	 * Configure gpt1 to use 13MHz clock during re-configuration.
	 *
	 * Reason: Clock synchronization issue may happen if gpt is in 32KHz
	 *         domain during re-configuration. For example: Updating cmp
	 *         value may need to wait a period of time (e.g., 3.5T) to let
	 *         gpt hw finish jobs: gpt hw will clear counter value
	 *         automatically while setting new cmp value.
	 *
	 *         GPT_EN shall be enabled after gpt hw finishes above job,
	 *         otherwise gpt may work abnormally, e.g., wrong cmp value
	 *         is latched or counter value is not reset.
	 *
	 *         If gpt is running under 13MHz, above waiting is not required
	 *         since gpt hw guarantees that GPT_EN is applied after above
	 *         job is done.
	 */
	__gpt_set_clk(dev, GPT_CLK_SRC_SYS & GPT_CLKSRC_MASK,
		GPT_CLK_DIV_1 & GPT_CLKDIV_MASK);

	__gpt_stop(dev);

	if (cycles < 3) {
		pr_info("[mt_gpt] invalid cycles < 3\n");
		cycles = 3;
	}

	/*
	 * Do cmp first because updating cmp will trigger most
	 * complicated behavior in gpt hw. See above description.
	 */
	__gpt_set_cmp(dev, cycles, 0);

	/* ack irq */
	__gpt_ack_irq(dev);

	/* ensure irq is enabled before next running */
	__gpt_enable_irq(dev);

	/*
	 * Configure gpt1 to use 32KHz clock before enabling.
	 *
	 * Reason: 13MHz clock source may be disabled during some
	 *         low-power scenarios, e.g., SODI. We shall use a
	 *         always-on clock after enabling, e.g., 32KHz.
	 */
	__gpt_set_clk(dev, GPT_CLK_SRC_RTC & GPT_CLKSRC_MASK,
		GPT_CLK_DIV_1 & GPT_CLKDIV_MASK);

	__gpt_start(dev);

#if defined(CONFIG_MTK_TIMER_AEE_DUMP)
	gpt_clkevt_last_setting_next_event_time = sched_clock();
#endif
	return 0;
}

static int mt_gpt_clkevt_shutdown(struct clock_event_device *clk)
{
	struct gpt_device *dev = id_to_dev(GPT_CLKEVT_ID);

	__gpt_stop(dev);
	__gpt_disable_irq(dev);
	__gpt_ack_irq(dev);

	return 0;
}

static int mt_gpt_clkevt_resume(struct clock_event_device *clk)
{
	return mt_gpt_clkevt_shutdown(clk);
}

static int mt_gpt_clkevt_oneshot(struct clock_event_device *clk)
{
	struct gpt_device *dev = id_to_dev(GPT_CLKEVT_ID);

	__gpt_stop(dev);
	__gpt_set_mode(dev, GPT_ONE_SHOT);
	/* __gpt_enable_irq(dev); */
	/* __gpt_clrcnt_and_start(dev); */

	return 0;
}

static int mt_gpt_set_periodic(struct clock_event_device *clk)
{
	struct gpt_device *dev = id_to_dev(GPT_CLKEVT_ID);

	__gpt_stop(dev);
	__gpt_set_mode(dev, GPT_REPEAT);
	__gpt_enable_irq(dev);
	__gpt_wait_clrcnt_then_start(dev);

	return 0;
}

static u64 mt_gpt_read(struct clocksource *cs)
{
	u64 cycles;
	unsigned int cnt[2] = { 0, 0 };
	struct gpt_device *dev = id_to_dev(GPT_CLKSRC_ID);

	__gpt_get_cnt(dev, cnt);

	cycles = ((u64) (cnt[1])) << 32 | (u64) (cnt[0]);

	return cycles;
}

static u64 notrace mt_read_sched_clock(void)
{
	return mt_gpt_read(NULL);
}

static void clkevt_handler(unsigned long data)
{
	struct clock_event_device *evt = (struct clock_event_device *)data;

	evt->event_handler(evt);
}

static inline void setup_clksrc(u32 freq)
{
	struct clocksource *cs = &gpt_clocksource;
	struct gpt_device *dev = id_to_dev(GPT_CLKSRC_ID);

	pr_info("setup_clksrc1: dev->base_addr=0x%lx GPT2_CON=0x%x\n",
		(unsigned long)dev->base_addr, __raw_readl(dev->base_addr));

	/* add GPT_NOIRQEN flag to avoid irq asserted because
	 * clksrc is not used
	 */
	setup_gpt_dev_locked(dev, GPT_FREE_RUN, GPT_CLK_SRC_SYS, GPT_CLK_DIV_1,
		0, NULL, GPT_NOIRQEN);

	sched_clock_register(mt_read_sched_clock, 32, (unsigned long)freq);

	/* clocksource_register(cs); */
	clocksource_register_hz(cs, freq);

	pr_info("setup_clksrc2: dev->base_addr=0x%lx GPT2_CON=0x%x\n",
		(unsigned long)dev->base_addr, __raw_readl(dev->base_addr));
}

static inline void setup_clkevt(u32 freq, int irq)
{
	unsigned int cmp[2];
	struct clock_event_device *evt = &gpt_clockevent;
	struct gpt_device *dev = id_to_dev(GPT_CLKEVT_ID);

	/* ensure to provide irq number for tick_broadcast_set_affinity() */
	evt->irq = irq;

	evt->mult = div_sc(freq, NSEC_PER_SEC, evt->shift);
	evt->max_delta_ns = clockevent_delta2ns(0xffffffff, evt);
	evt->min_delta_ns = clockevent_delta2ns(3, evt);
	evt->cpumask = cpumask_of(0);

	setup_gpt_dev_locked(dev, GPT_REPEAT, GPT_CLK_SRC_SYS, GPT_CLK_DIV_1,
			     freq / HZ, clkevt_handler, GPT_ISR);

	__gpt_get_cmp(dev, cmp);

	pr_info("[mtk_gpt] gpt%d: cmp=%d, hz=%d, freq=%d\n",
		GPT_CLKEVT_ID + 1, cmp[0], HZ, freq);

	clockevents_register_device(evt);
}

static inline void setup_syscnt(void)
{
	struct gpt_device *dev = id_to_dev(GPT_SYSCNT_ID);

	setup_gpt_dev_locked(dev, GPT_FREE_RUN, GPT_CLK_SRC_SYS, GPT_CLK_DIV_1, 0, NULL, 0);

#if defined(CONFIG_MACH_MT6735M) || defined(CONFIG_MACH_MT6735) || defined (CONFIG_MACH_MT6753)
	/* map cpuxgpt address */
	mt_cpuxgpt_map_base();

	/* set cpuxgpt free run,cpuxgpt always free run & oneshot no need to set */
	/* set cpuxgpt 13Mhz clock */
	set_cpuxgpt_clk(CLK_DIV2);
	/* enable cpuxgpt */
	enable_cpuxgpt();
#endif

	pr_alert("fwq sysc count\n");
}

static int __init mt_gpt_init(struct device_node *node)
{
	int i;
	u32 freq = 0;
	unsigned long save_flags;

	gpt_update_lock(save_flags);

	/* freq=SYS_CLK_RATE */
	if (of_property_read_u32(node, "clock-frequency", &freq))
		pr_err("clock-frequency not set in the .dts file");

#ifdef CONFIG_MTK_FPGA
	freq = (freq / 13 * 6);	/* 13M would be 6M on FPGA */
#endif

	/* Setup IRQ numbers */
	xgpt_timers.tmr_irq = irq_of_parse_and_map(node, 0);

	/* Setup IO addresses */
	xgpt_timers.tmr_regs = of_iomap(node, 0);

	boot_time_value = xgpt_boot_up_time();	/*record the time when init GPT */

	pr_alert("mt_gpt_init: tmr_regs=0x%x, tmr_irq=%d, freq=%d\n", (u32) xgpt_timers.tmr_regs,
		 xgpt_timers.tmr_irq, freq);

	gpt_devs_init();

	for (i = 0; i < NR_GPTS; i++) {
#ifdef CONFIG_TRUSTY
		if (i == GPT_SYSCNT_ID)
			continue;
#endif
		__gpt_reset(&gpt_devs[i]);
	}

	setup_clksrc(freq);
	setup_irq(xgpt_timers.tmr_irq, &gpt_irq);
	setup_clkevt(freq, xgpt_timers.tmr_irq);

	/* use cpuxgpt as syscnt */
	setup_syscnt();

	gpt_update_unlock(save_flags);
return 0;
}

static void release_gpt_dev_locked(struct gpt_device *dev)
{
	__gpt_reset(dev);

	handlers[dev->id] = noop;
	dev->func = NULL;

	dev->flags = 0;
}

/* gpt is counting or not */
static int __gpt_get_status(struct gpt_device *dev)
{
	return !!(__raw_readl(dev->base_addr + GPT_CON) & GPT_CON_ENABLE);
}

/**********************	export area *********************/
int request_gpt(unsigned int id, unsigned int mode, unsigned int clksrc,
		unsigned int clkdiv, unsigned int cmp,
		void (*func)(unsigned long), unsigned int flags)
{
	unsigned long save_flags;
	struct gpt_device *dev = id_to_dev(id);

	if (!dev)
		return -EINVAL;

	if (dev->flags & GPT_IN_USE) {
		pr_info("%s: GPT%d is in use!\n", __func__, (id + 1));
		return -EBUSY;
	}

	gpt_update_lock(save_flags);
	setup_gpt_dev_locked(dev, mode, clksrc, clkdiv, cmp, func, flags);
	gpt_update_unlock(save_flags);

	return 0;
}
EXPORT_SYMBOL(request_gpt);

int free_gpt(unsigned int id)
{
	unsigned long save_flags;

	struct gpt_device *dev = id_to_dev(id);

	if (!dev)
		return -EINVAL;

	if (!(dev->flags & GPT_IN_USE))
		return 0;

	gpt_update_lock(save_flags);
	release_gpt_dev_locked(dev);
	gpt_update_unlock(save_flags);
	return 0;
}
EXPORT_SYMBOL(free_gpt);

int start_gpt(unsigned int id)
{
	unsigned long save_flags;
	struct gpt_device *dev = id_to_dev(id);

	if (!dev)
		return -EINVAL;

	if (!(dev->flags & GPT_IN_USE)) {
		pr_info("%s: GPT%d is not in use!\n", __func__, id);
		return -EBUSY;
	}

	gpt_update_lock(save_flags);
	__gpt_clrcnt(dev);
	__gpt_start(dev);
	gpt_update_unlock(save_flags);

	return 0;
}
EXPORT_SYMBOL(start_gpt);

int stop_gpt(unsigned int id)
{
	unsigned long save_flags;
	struct gpt_device *dev = id_to_dev(id);

	if (!dev)
		return -EINVAL;

	if (!(dev->flags & GPT_IN_USE)) {
		pr_info("%s: GPT%d is not in use!\n", __func__, id);
		return -EBUSY;
	}

	gpt_update_lock(save_flags);
	__gpt_stop(dev);
	gpt_update_unlock(save_flags);

	return 0;
}
EXPORT_SYMBOL(stop_gpt);

int restart_gpt(unsigned int id)
{
	unsigned long save_flags;
	struct gpt_device *dev = id_to_dev(id);

	if (!dev)
		return -EINVAL;

	if (!(dev->flags & GPT_IN_USE)) {
		pr_info("%s: GPT%d is not in use!\n", __func__, id);
		return -EBUSY;
	}

	gpt_update_lock(save_flags);
	__gpt_start(dev);
	gpt_update_unlock(save_flags);

	return 0;
}
EXPORT_SYMBOL(restart_gpt);

int gpt_is_counting(unsigned int id)
{
	unsigned long save_flags;
	int is_counting;
	struct gpt_device *dev = id_to_dev(id);

	if (!dev)
		return -EINVAL;

	if (!(dev->flags & GPT_IN_USE)) {
		pr_info("%s: GPT%d is not in use!\n", __func__, id);
		return -EBUSY;
	}

	gpt_update_lock(save_flags);
	is_counting = __gpt_get_status(dev);
	gpt_update_unlock(save_flags);

	return is_counting;
}
EXPORT_SYMBOL(gpt_is_counting);

int gpt_set_cmp(unsigned int id, unsigned int val)
{
	unsigned long save_flags;
	struct gpt_device *dev = id_to_dev(id);

	if (!dev)
		return -EINVAL;

	if (dev->mode == GPT_FREE_RUN)
		return -EINVAL;

	gpt_update_lock(save_flags);
	__gpt_set_cmp(dev, val, 0);
	gpt_update_unlock(save_flags);

	return 0;
}
EXPORT_SYMBOL(gpt_set_cmp);

int gpt_get_cmp(unsigned int id, unsigned int *ptr)
{
	unsigned long save_flags;
	struct gpt_device *dev = id_to_dev(id);

	if (!dev || !ptr)
		return -EINVAL;

	gpt_update_lock(save_flags);
	__gpt_get_cmp(dev, ptr);
	gpt_update_unlock(save_flags);

	return 0;
}
EXPORT_SYMBOL(gpt_get_cmp);

int gpt_get_cnt(unsigned int id, unsigned int *ptr)
{
	unsigned long save_flags;
	struct gpt_device *dev = id_to_dev(id);

	if (!dev || !ptr)
		return -EINVAL;

	if (!(dev->features & GPT_FEAT_64_BIT)) {
		__gpt_get_cnt(dev, ptr);
	} else {
		gpt_update_lock(save_flags);
		__gpt_get_cnt(dev, ptr);
		gpt_update_unlock(save_flags);
	}

	return 0;
}
EXPORT_SYMBOL(gpt_get_cnt);


int gpt_check_irq(unsigned int id)
{
	unsigned int mask = 0x1 << id;
	unsigned int status = __raw_readl(GPT_IRQSTA);

	return (status & mask) ? 1 : 0;
}
EXPORT_SYMBOL(gpt_check_irq);

int gpt_check_and_ack_irq(unsigned int id)
{
	unsigned int mask = 0x1 << id;
	unsigned int status = __raw_readl(GPT_IRQSTA);

	if (status & mask) {
		mt_reg_sync_writel(mask, GPT_IRQACK);
		return 1;
	} else {
		return 0;
	}
}
EXPORT_SYMBOL(gpt_check_and_ack_irq);

unsigned int gpt_boot_time(void)
{
	return boot_time_value;
}
EXPORT_SYMBOL(gpt_boot_time);

static ssize_t gpt_stat_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	char *p = NULL;
	char *page = NULL;
	int len = 0;
	int i = 0;
	int in_use;
	int is_counting;
	int err = 0;

	page = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!page) {
		kfree(page);
		return -ENOMEM;
	}
	p = page;
	p += sprintf(p, "\n(HW Timer) GPT Status :\n");
	p += sprintf(p, "=========================================\n");

	for (i = 0; i < NR_GPTS; i++) {
		in_use = gpt_devs[i].flags & GPT_IN_USE;
		is_counting = gpt_is_counting(i);
		p += sprintf(p, "[GPT%d]in_use:%s, is_counting:%s\n", i + 1,
			     in_use ? "Y" : "N", is_counting ? "Y" : "N");
	}
	len = p - page;

	if (*ppos >= len) {
		kfree(page);
		return 0;
	}

	err = copy_to_user(buf, (char *)page, len);
	*ppos += len;
	if (err) {
		kfree(page);
		return err;
	}
	kfree(page);
	return len;
}

static const struct file_operations xgpt_cmd_proc_fops = {
	.read = gpt_stat_read,
};

static int __init gpt_mod_init(void)
{
	struct proc_dir_entry *xgpt_dir = NULL;

	xgpt_dir = proc_mkdir("mt_xgpt", NULL);
	proc_create("xgpt_stat", S_IRUGO, xgpt_dir, &xgpt_cmd_proc_fops);

	pr_alert("GPT: iniit\n");

	return 0;
}


module_init(gpt_mod_init);
CLOCKSOURCE_OF_DECLARE(mtk_apxgpt, "mediatek,APXGPT", mt_gpt_init);

MODULE_DESCRIPTION("MT6580 GPT Driver v0.1");
MODULE_LICENSE("GPL");
