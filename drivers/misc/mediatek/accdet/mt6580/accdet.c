// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <upmu_common.h>
#include <linux/timer.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include "accdet.h"
#ifdef CONFIG_ACCDET_EINT
#include <linux/gpio.h>
#endif


#define DEBUG_THREAD 1

#ifdef CONFIG_FOUR_KEY_HEADSET
#define AUX_IN2_KEY (12)
static int g_FourKey_ADC_channel = AUX_IN2_KEY;
#endif

#define REGISTER_VALUE(x)   (x - 1)
static int button_press_debounce = 0x400;
int cur_key = 0;
struct head_dts_data accdet_dts_data;
s8 accdet_auxadc_offset;
int accdet_irq;
unsigned int gpiopin, headsetdebounce;
unsigned int accdet_eint_type;
struct headset_mode_settings *cust_headset_settings;
static void send_accdet_status_event(int cable_type, int status);
static struct input_dev *kpd_accdet_dev;
static struct cdev *accdet_cdev;
static struct class *accdet_class;
static struct device *accdet_nor_device;
static dev_t accdet_devno;
static int pre_status;
static int pre_state_swctrl;
static int accdet_status = PLUG_OUT;
static int cable_type;
#ifdef CONFIG_ACCDET_PIN_RECOGNIZATION
/* add for new feature PIN recognition */
static int cable_pin_recognition;
static int show_icon_delay;
#endif
static int eint_accdet_sync_flag;
static int g_accdet_first = 1;
static bool IRQ_CLR_FLAG;
static int call_status;
static int button_status;
static struct wakeup_source *accdet_irq_lock;
static struct wakeup_source *accdet_timer_lock;
static struct work_struct accdet_work;
static struct workqueue_struct *accdet_workqueue;
static DEFINE_MUTEX(accdet_eint_irq_sync_mutex);
static inline void clear_accdet_interrupt(void);
static void send_key_event(int keycode, int flag);

#if defined CONFIG_ACCDET_EINT
static struct work_struct accdet_eint_work;
static struct workqueue_struct *accdet_eint_workqueue;
static inline void accdet_init(void);
#define MICBIAS_DISABLE_TIMER   (6 * HZ)/* 6 seconds */
struct timer_list micbias_timer;
static void disable_micbias(struct timer_list *t);
/* Used to let accdet know if the pin has been fully plugged-in */
#define EINT_PIN_PLUG_IN        (1)
#define EINT_PIN_PLUG_OUT       (0)
int cur_eint_state = EINT_PIN_PLUG_OUT;
struct pinctrl *accdet_pinctrl1;
struct pinctrl_state *pins_eint_int;
static struct work_struct accdet_disable_work;
static struct workqueue_struct *accdet_disable_workqueue;
#endif/* end CONFIG_ACCDET_EINT */


static u32 pmic_pwrap_read(u32 addr);
static void pmic_pwrap_write(u32 addr, unsigned int wdata);
char *accdet_status_string[5] = {
	"Plug_out",
	"Headset_plug_in",
	"Hook_switch",
	"Stand_by"
};
char *accdet_report_string[4] = {
	"No_device",
	"Headset_mic",
	"Headset_no_mic",
};

void accdet_detect(void)
{
	int ret = 0;

	pr_notice("[Accdet]accdet_detect\n");

	accdet_status = PLUG_OUT;
	ret = queue_work(accdet_workqueue, &accdet_work);
	if (!ret)
		pr_notice("[Accdet]accdet_detect:accdet_work return:%d!\n", ret);
}
EXPORT_SYMBOL(accdet_detect);

void accdet_state_reset(void)
{

	pr_notice("[Accdet]accdet_state_reset\n");

	accdet_status = PLUG_OUT;
	cable_type = NO_DEVICE;
}
EXPORT_SYMBOL(accdet_state_reset);

int accdet_get_cable_type(void)
{
	return cable_type;
}

void accdet_auxadc_switch(int enable)
{
	if (enable) {
		pmic_pwrap_write(ACCDET_RSV, ACCDET_1V9_MODE_ON);
	} else {
		pmic_pwrap_write(ACCDET_RSV, ACCDET_1V9_MODE_OFF);
	}
}

static u64 accdet_get_current_time(void)
{
	return sched_clock();
}

static bool accdet_timeout_ns(u64 start_time_ns, u64 timeout_time_ns)
{
	u64 cur_time = 0;
	u64 elapse_time = 0;

	/* get current tick */
	cur_time = accdet_get_current_time();/* ns */
	if (cur_time < start_time_ns) {
		pr_notice("@@@@Timer overflow, start%lld cur timer%lld\n",
			start_time_ns, cur_time);
		start_time_ns = cur_time;
		timeout_time_ns = 400 * 1000;/* 400us */
		pr_notice("@@@@reset timer, start%lld setting%lld\n",
			start_time_ns, timeout_time_ns);
	}
	elapse_time = cur_time - start_time_ns;

	/* check if timeout */
	if (timeout_time_ns <= elapse_time) {
		/* timeout */
		pr_notice("@@@@ACCDET IRQ clear Timeout\n");
		return false;
	}
	return true;
}

/* pmic wrap read and write func */
static u32 pmic_pwrap_read(u32 addr)
{
	u32 val = 0;

	pwrap_read(addr, &val);
	return val;
}

static void pmic_pwrap_write(unsigned int addr, unsigned int wdata)
{
	pwrap_write(addr, wdata);
}


#ifdef CONFIG_ACCDET_PIN_SWAP

static void accdet_FSA8049_enable(void)
{
	mt_set_gpio_mode(GPIO_FSA8049_PIN, GPIO_FSA8049_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_FSA8049_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_FSA8049_PIN, GPIO_OUT_ONE);
}

static void accdet_FSA8049_disable(void)
{
	mt_set_gpio_mode(GPIO_FSA8049_PIN, GPIO_FSA8049_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_FSA8049_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_FSA8049_PIN, GPIO_OUT_ZERO);
}

#endif
static inline void headset_plug_out(void)
{
	send_accdet_status_event(cable_type, 0);
	accdet_status = PLUG_OUT;
	cable_type = NO_DEVICE;
	/*update the cable_type*/
	if (cur_key != 0) {
		send_key_event(cur_key, 0);
		pr_notice("[accdet]headset_plug_out send key = %d release\n",
			cur_key);
		cur_key = 0;
	}
	pr_notice("[accdet]set state in cable_type = NO_DEVICE\n");

}

/* Accdet only need this func */
static inline void enable_accdet(u32 state_swctrl)
{
	/* enable ACCDET unit */
	pr_notice("accdet:enable_accdet!\n");
	/* enable clock */
	pmic_pwrap_write(TOP_CKPDN_CLR, RG_ACCDET_CLK_CLR);
	pmic_pwrap_write(ACCDET_STATE_SWCTRL,
		pmic_pwrap_read(ACCDET_STATE_SWCTRL) | state_swctrl);
	pmic_pwrap_write(ACCDET_CTRL,
		pmic_pwrap_read(ACCDET_CTRL) | ACCDET_ENABLE);

}

static inline void disable_accdet(void)
{
	int irq_temp = 0;

/* sync with accdet_irq_handler set clear accdet irq bit to avoid
 * set clear accdet irq bit after disable accdet disable accdet irq
 */
	pmic_pwrap_write(INT_CON_ACCDET_CLR, RG_ACCDET_IRQ_CLR);
	clear_accdet_interrupt();
	udelay(200);
	mutex_lock(&accdet_eint_irq_sync_mutex);
	while (pmic_pwrap_read(ACCDET_IRQ_STS) & IRQ_STATUS_BIT) {
		pr_notice("[Accdet]check_cable_type: Clear interrupt on-going\n");
		msleep(20);
	}
	irq_temp = pmic_pwrap_read(ACCDET_IRQ_STS);
	irq_temp = irq_temp & (~IRQ_CLR_BIT);
	pmic_pwrap_write(ACCDET_IRQ_STS, irq_temp);
	mutex_unlock(&accdet_eint_irq_sync_mutex);
	/*disable ACCDET unit*/
	pr_notice("accdet: disable_accdet\n");
	pre_state_swctrl = pmic_pwrap_read(ACCDET_STATE_SWCTRL);
	pmic_pwrap_write(ACCDET_STATE_SWCTRL, 0);
	pmic_pwrap_write(ACCDET_CTRL, ACCDET_DISABLE);
	/*disable clock*/
	pmic_pwrap_write(TOP_CKPDN_SET, RG_ACCDET_CLK_SET);
}

static void disable_micbias(struct timer_list *t)
{
	int ret = 0;

	ret = queue_work(accdet_disable_workqueue, &accdet_disable_work);
	if (!ret)
		pr_notice("[Accdet]disable_micbias:accdet_work return:%d!\n", ret);
}

static void disable_micbias_callback(struct work_struct *work)
{

	if (cable_type == HEADSET_NO_MIC) {
#ifdef CONFIG_ACCDET_PIN_RECOGNIZATION
		show_icon_delay = 0;
		cable_pin_recognition = 0;
		pr_notice("[Accdet] cable_pin_recognition = %d\n",
			cable_pin_recognition);
		pmic_pwrap_write(ACCDET_PWM_WIDTH, cust_headset_settings->pwm_width);
		pmic_pwrap_write(ACCDET_PWM_THRESH, cust_headset_settings->pwm_thresh);
#endif
		/*setting pwm idle;*/
		pmic_pwrap_write(ACCDET_STATE_SWCTRL,
			pmic_pwrap_read(ACCDET_STATE_SWCTRL) & ~ACCDET_SWCTRL_IDLE_EN);
#ifdef CONFIG_ACCDET_PIN_SWAP
		/*accdet_FSA8049_disable(); disable GPIOxxx for PIN swap */
		/*pr_notice("[Accdet] FSA8049 disable!\n");*/
#endif
		disable_accdet();
		pr_notice("[Accdet] more than 5s MICBIAS : Disabled\n");
	}
#ifdef CONFIG_ACCDET_PIN_RECOGNIZATION
	else if (cable_type == HEADSET_MIC) {
		pmic_pwrap_write(ACCDET_PWM_WIDTH, cust_headset_settings->pwm_width);
		pmic_pwrap_write(ACCDET_PWM_THRESH, cust_headset_settings->pwm_thresh);
		pr_notice("[Accdet]pin recog after 5s recover micbias polling!\n");
	}
#endif
		pr_notice("[Accdet]argus %d %d\n", cable_type, accdet_status);
}

static void accdet_eint_work_callback(struct work_struct *work)
{
	/*KE under fastly plug in and plug out*/
	if (cur_eint_state == EINT_PIN_PLUG_IN) {
		pr_notice("[Accdet]ACC EINT func:plug-in, cur_eint_state = %d\n",
			cur_eint_state);
		mutex_lock(&accdet_eint_irq_sync_mutex);
		eint_accdet_sync_flag = 1;
		mutex_unlock(&accdet_eint_irq_sync_mutex);
		__pm_wakeup_event(accdet_timer_lock, jiffies_to_msecs(7 * HZ));
#ifdef CONFIG_ACCDET_PIN_SWAP
		pmic_pwrap_write(0x0400, pmic_pwrap_read(0x0400)|(1<<14));
		msleep(800);
		accdet_FSA8049_enable();	/*enable GPIOxxx for PIN swap */
		pr_notice("[Accdet] FSA8049 enable!\n");
		msleep(250);	/*PIN swap need ms */
#endif
		accdet_init();	/* do set pwm_idle on in accdet_init*/

#ifdef CONFIG_ACCDET_PIN_RECOGNIZATION
		show_icon_delay = 1;
		/*micbias always on during detected PIN recognition*/
		pmic_pwrap_write(ACCDET_PWM_WIDTH, cust_headset_settings->pwm_width);
		pmic_pwrap_write(ACCDET_PWM_THRESH, cust_headset_settings->pwm_width);
		pr_notice("[Accdet]pin recog start, micbias always on!\n");
#endif
		/*set PWM IDLE  on*/
		pmic_pwrap_write(ACCDET_STATE_SWCTRL,
			(pmic_pwrap_read(ACCDET_STATE_SWCTRL) | ACCDET_SWCTRL_IDLE_EN));
		/*enable ACCDET unit*/
		enable_accdet(ACCDET_SWCTRL_EN);
	} else {
/*EINT_PIN_PLUG_OUT*/
/*Disable ACCDET*/
		pr_notice("[Accdet]EINT func:plug-out,cur_eint_state = %d\n",
			cur_eint_state);
		mutex_lock(&accdet_eint_irq_sync_mutex);
		eint_accdet_sync_flag = 0;
		mutex_unlock(&accdet_eint_irq_sync_mutex);
		del_timer_sync(&micbias_timer);
#ifdef CONFIG_ACCDET_PIN_RECOGNIZATION
		show_icon_delay = 0;
		cable_pin_recognition = 0;
#endif
#ifdef CONFIG_ACCDET_PIN_SWAP
		pmic_pwrap_write(0x0400, pmic_pwrap_read(0x0400)&~(1<<14));
		accdet_FSA8049_disable();	/* disable GPIOxxx for PIN swap */
		pr_notice("[Accdet] FSA8049 disable!\n");
#endif
		accdet_auxadc_switch(0);
		disable_accdet();
		headset_plug_out();
	}
	enable_irq(accdet_irq);
	pr_notice("[Accdet]enable_irq!!\n");
}

static irqreturn_t accdet_eint_func(int irq, void *data)
{
	int ret = 0;

	pr_notice("[Accdet]Enter accdet_eint_func,accdet_eint_type=%d\n",
		accdet_eint_type);
	if (cur_eint_state == EINT_PIN_PLUG_IN) {
/*
 * To trigger EINT when the headset was plugged in
 * We set the polarity back as we initialed.
 */
		if (accdet_eint_type == IRQ_TYPE_LEVEL_HIGH)
			irq_set_irq_type(accdet_irq, IRQ_TYPE_LEVEL_HIGH);
		else
			irq_set_irq_type(accdet_irq, IRQ_TYPE_LEVEL_LOW);
		gpio_set_debounce(gpiopin, headsetdebounce);
		/* update the eint status */
		cur_eint_state = EINT_PIN_PLUG_OUT;
	} else {
/*
 * To trigger EINT when the headset was plugged out
 * We set the opposite polarity to what we initialed.
 */
		if (accdet_eint_type == IRQ_TYPE_LEVEL_HIGH)
			irq_set_irq_type(accdet_irq, IRQ_TYPE_LEVEL_LOW);
		else
			irq_set_irq_type(accdet_irq, IRQ_TYPE_LEVEL_HIGH);

		gpio_set_debounce(gpiopin,
			accdet_dts_data.accdet_plugout_debounce * 1000);
		/* update the eint status */
		cur_eint_state = EINT_PIN_PLUG_IN;

		mod_timer(&micbias_timer, jiffies + MICBIAS_DISABLE_TIMER);
	}
	disable_irq_nosync(accdet_irq);
	pr_notice("[Accdet]accdet_eint_func after cur_eint_state=%d\n",
		cur_eint_state);

	ret = queue_work(accdet_eint_workqueue, &accdet_eint_work);
	return IRQ_HANDLED;
}

static inline int accdet_setup_eint(struct platform_device *accdet_device)
{
	int ret;
	u32 ints[2] = { 0, 0 };
	u32 ints1[2] = { 0, 0 };
	struct device_node *node = NULL;
	struct pinctrl_state *pins_default;

	/* configure to GPIO function, external interrupt */
	pr_info("[Accdet]accdet_setup_eint\n");
	accdet_pinctrl1 = devm_pinctrl_get(&accdet_device->dev);
	if (IS_ERR(accdet_pinctrl1)) {
		ret = PTR_ERR(accdet_pinctrl1);
		dev_err(&accdet_device->dev, "Cannot find accdet accdet_pinctrl1!\n");
		return ret;
	}

	pins_default = pinctrl_lookup_state(accdet_pinctrl1, "default");
	if (IS_ERR(pins_default)) {
		ret = PTR_ERR(pins_default);
		dev_err(&accdet_device->dev, "Cannot find accdet pinctrl default!\n");
	}

	pins_eint_int = pinctrl_lookup_state(accdet_pinctrl1, "state_eint_as_int");
	if (IS_ERR(pins_eint_int)) {
		ret = PTR_ERR(pins_eint_int);
		dev_err(&accdet_device->dev,
			"Cannot find accdet pinctrl state_eint_int!\n");
		return ret;
	}
	pinctrl_select_state(accdet_pinctrl1, pins_eint_int);

	node = of_find_matching_node(node, accdet_of_match);
	if (node) {
		of_property_read_u32_array(node, "debounce", ints, ARRAY_SIZE(ints));
		of_property_read_u32_array(node, "interrupts",
			ints1, ARRAY_SIZE(ints1));
		gpiopin = ints[0];
		headsetdebounce = ints[1];
		accdet_eint_type = ints1[1];
		gpio_set_debounce(gpiopin, headsetdebounce);

		accdet_irq = irq_of_parse_and_map(node, 0);
		ret = request_irq(accdet_irq, accdet_eint_func,
			IRQF_TRIGGER_NONE, "accdet-eint", NULL);
		if (ret != 0) {
			pr_notice("[Accdet]EINT IRQ LINE NOT AVAILABLE\n");
		} else {
			pr_notice("[Accdet]accdet set EINT finished,accdet_irq=%d,headset deb=%d\n",
				     accdet_irq, headsetdebounce);
		}
	} else {
		pr_notice("[Accdet]%s can't find compatible node\n", __func__);
	}
	return 0;
}

#define KEY_SAMPLE_PERIOD        (60)/* ms */
#define MULTIKEY_ADC_CHANNEL	 (8)

static DEFINE_MUTEX(accdet_multikey_mutex);
#define NO_KEY			 (0x0)
#define UP_KEY			 (0x01)
#define MD_KEY			(0x02)
#define DW_KEY			 (0x04)
#define AS_KEY			 (0x08)

#ifndef CONFIG_FOUR_KEY_HEADSET
static int key_check(int b)
{
	/* 0.24V ~ */
	/* pr_notice("[accdet] come in key_check!!\n"); */
	if ((b < accdet_dts_data.three_key.down_key) &&
		(b >= accdet_dts_data.three_key.up_key))
		return DW_KEY;
	else if ((b < accdet_dts_data.three_key.up_key) &&
		(b >= accdet_dts_data.three_key.mid_key))
		return UP_KEY;
	else if ((b < accdet_dts_data.three_key.mid_key) && (b >= 0))
		return MD_KEY;
	pr_notice("[accdet] leave key_check!!\n");
	return NO_KEY;
}
#else
static int key_check(int b)
{
	/* 0.24V ~ */
	/* pr_notice("[accdet] come in key_check!!\n"); */
	if ((b < accdet_dts_data.four_key.down_key_four) &&
		(b >= accdet_dts_data.four_key.up_key_four))
		return DW_KEY;
	else if ((b < accdet_dts_data.four_key.up_key_four) &&
		(b >= accdet_dts_data.four_key.voice_key_four))
		return UP_KEY;
	else if ((b < accdet_dts_data.four_key.voice_key_four) &&
		(b >= accdet_dts_data.four_key.mid_key_four))
		return AS_KEY;
	else if ((b < accdet_dts_data.four_key.mid_key_four) && (b >= 0))
		return MD_KEY;
	pr_notice("[accdet] leave key_check!!\n");
	return NO_KEY;
}

#endif

static void send_accdet_status_event(int cable_type, int status)
{
	switch (cable_type) {
	case HEADSET_NO_MIC:
		input_report_switch(kpd_accdet_dev, SW_HEADPHONE_INSERT, status);
		input_report_switch(kpd_accdet_dev, SW_JACK_PHYSICAL_INSERT, status);
		input_sync(kpd_accdet_dev);
		pr_notice("[accdet]Headphone(3-pole)%s\n",
			status?"PlugIn":"PlugOut");
		break;
	case HEADSET_MIC:
		input_report_switch(kpd_accdet_dev, SW_MICROPHONE_INSERT, status);
		input_report_switch(kpd_accdet_dev, SW_HEADPHONE_INSERT, status);
		input_report_switch(kpd_accdet_dev, SW_JACK_PHYSICAL_INSERT, status);
		input_sync(kpd_accdet_dev);
		pr_notice("[accdet]MICROPHONE(4-pole) %s\n",
			status?"PlugIn":"PlugOut");
		break;
	default:
		pr_notice("[accdet][send_accdet_status_Inputevent]Invalid cableType\n");
	}
}

static void send_key_event(int keycode, int flag)
{
	switch (keycode) {
	case DW_KEY:
		input_report_key(kpd_accdet_dev, KEY_VOLUMEDOWN, flag);
		input_sync(kpd_accdet_dev);
		pr_notice("[accdet]KEY_VOLUMEDOWN %d\n", flag);
		break;
	case UP_KEY:
		input_report_key(kpd_accdet_dev, KEY_VOLUMEUP, flag);
		input_sync(kpd_accdet_dev);
		pr_notice("[accdet]KEY_VOLUMEUP %d\n", flag);
		break;
	case MD_KEY:
		input_report_key(kpd_accdet_dev, KEY_PLAYPAUSE, flag);
		input_sync(kpd_accdet_dev);
		pr_notice("[accdet]KEY_PLAYPAUSE %d\n", flag);
		break;
	case AS_KEY:
		input_report_key(kpd_accdet_dev, KEY_VOICECOMMAND, flag);
		input_sync(kpd_accdet_dev);
		pr_notice("[accdet]KEY_VOICECOMMAND %d\n", flag);
		break;
	}
}

static void multi_key_detection(int current_status)
{
	int m_key = 0;
	int cali_voltage = 0;
#ifdef CONFIG_FOUR_KEY_HEADSET
	int data[4], adc_raw = 0, ap_key_vol = 0;
#endif

	if (current_status == 0) {
		cali_voltage = PMIC_IMM_GetOneChannelValue(MULTIKEY_ADC_CHANNEL, 1, 1);
		if (cali_voltage < 0) {
			pr_notice("[Accdet]read auxadc cali_vol:%d adjust to 0\n",
				cali_voltage);
			cali_voltage = 0;
		}
		pr_notice("[Accdet]adc cali_voltage = %d mV\n", cali_voltage);
#ifdef CONFIG_FOUR_KEY_HEADSET
		if (cali_voltage < 100) {
			IMM_GetOneChannelValue(g_FourKey_ADC_channel, data, &adc_raw);
			ap_key_vol = (data[0] * 1000 + data[2]) * 189 / 169;
/* skip the case that user press the key very very quickly and repeatedly */
			if (ap_key_vol > 130 || ap_key_vol - cali_voltage > 35 ||
				cali_voltage - ap_key_vol > 35)
				cali_voltage = 1500;
			else if (ap_key_vol < cali_voltage)
				cali_voltage = ap_key_vol;
		}
#endif
		m_key = cur_key = key_check(cali_voltage);
	}
	mdelay(30);
	if (((pmic_pwrap_read(ACCDET_IRQ_STS) & IRQ_STATUS_BIT) != IRQ_STATUS_BIT) || eint_accdet_sync_flag) {
		send_key_event(cur_key, !current_status);
	} else {
		pr_notice("[Accdet]plug out side effect key press, do not report key = %d\n",
			cur_key);
		cur_key = NO_KEY;
	}
	if (current_status)
		cur_key = NO_KEY;
}

static void accdet_workqueue_func(void)
{
	int ret;

	ret = queue_work(accdet_workqueue, &accdet_work);
	if (!ret)
		pr_notice("[Accdet]accdet_work return:%d!\n", ret);
}

int accdet_irq_handler(void)
{
	u64 cur_time = 0;

	cur_time = accdet_get_current_time();

	if ((pmic_pwrap_read(ACCDET_IRQ_STS) & IRQ_STATUS_BIT)) {
			clear_accdet_interrupt();
		if (accdet_status == MIC_BIAS) {
			accdet_auxadc_switch(1);
			pmic_pwrap_write(ACCDET_PWM_WIDTH, REGISTER_VALUE(cust_headset_settings->pwm_width));
			pmic_pwrap_write(ACCDET_PWM_THRESH, REGISTER_VALUE(cust_headset_settings->pwm_width));
		}
		accdet_workqueue_func();
		while (((pmic_pwrap_read(ACCDET_IRQ_STS) & IRQ_STATUS_BIT)
			&& (accdet_timeout_ns(cur_time, ACCDET_TIME_OUT))))
			;
	} else {
		pr_notice("[Accdet]No AB int: ACCDET_IRQ_STS = 0x%x\n",
			pmic_pwrap_read(ACCDET_IRQ_STS));
	}

	return 1;
}

/*clear ACCDET IRQ in accdet register*/
static inline void clear_accdet_interrupt(void)
{
	/*it is safe by using polling to adjust when to clear IRQ_CLR_BIT*/
	pmic_pwrap_write(ACCDET_IRQ_STS, ((pmic_pwrap_read(ACCDET_IRQ_STS)) & 0x8000) | (IRQ_CLR_BIT));
	pr_notice("[Accdet]clear_accdet_interrupt: ACCDET_IRQ_STS = 0x%x\n",
		pmic_pwrap_read(ACCDET_IRQ_STS));
}

static inline void check_cable_type(void)
{
	int current_status = 0;
	int irq_temp = 0;	/*for clear IRQ_bit*/
	int wait_clear_irq_times = 0;
#ifdef CONFIG_ACCDET_PIN_RECOGNIZATION
	int pin_adc_value = 0;
#define PIN_ADC_CHANNEL 5
#endif

	/*A=bit1; B=bit0*/
	current_status = ((pmic_pwrap_read(ACCDET_STATE_RG) & 0xc0) >> 6);

	button_status = 0;
	pre_status = accdet_status;

	IRQ_CLR_FLAG = false;
	switch (accdet_status) {
	case PLUG_OUT:
#ifdef CONFIG_ACCDET_PIN_RECOGNIZATION
		pmic_pwrap_write(ACCDET_DEBOUNCE1, cust_headset_settings->debounce1);
#endif
		if (current_status == 0) {
#ifdef CONFIG_ACCDET_PIN_RECOGNIZATION
			/*micbias always on during detected PIN recognition*/
			pmic_pwrap_write(ACCDET_PWM_WIDTH, cust_headset_settings->pwm_width);
			pmic_pwrap_write(ACCDET_PWM_THRESH, cust_headset_settings->pwm_width);
			pr_notice("[Accdet]PIN recognition micbias always on!\n");
			pr_notice("[Accdet]before adc read, pin_adc_value = %d mv!\n",
				pin_adc_value);
			msleep(500);
			current_status = ((pmic_pwrap_read(ACCDET_STATE_RG) & 0xc0) >> 6);
			if (current_status == 0 && show_icon_delay != 0) {
				/* switch on when need to use auxadc read voltage */
				accdet_auxadc_switch(1);
				pin_adc_value = PMIC_IMM_GetOneChannelValue(8, 10, 1);
				pr_notice("[Accdet]pin_adc_value = %d mv!\n", pin_adc_value);
				accdet_auxadc_switch(0);
				/* 100mv ilegal headset */
				if (200 > pin_adc_value && pin_adc_value > 100) {
				/* mt_set_gpio_out(GPIO_CAMERA_2_CMRST_PIN, GPIO_OUT_ONE); */
					mutex_lock(&accdet_eint_irq_sync_mutex);
					if (1 == eint_accdet_sync_flag) {
						cable_type = HEADSET_NO_MIC;
						accdet_status = HOOK_SWITCH;
						cable_pin_recognition = 1;
						pr_notice("[Accdet]cable_pin_recognition = %d\n",
							     cable_pin_recognition);
					} else {
						pr_notice("[Accdet]Headset has plugged out\n");
					}
					mutex_unlock(&accdet_eint_irq_sync_mutex);
				} else {
					mutex_lock(&accdet_eint_irq_sync_mutex);
					if (1 == eint_accdet_sync_flag) {
						cable_type = HEADSET_NO_MIC;
						accdet_status = HOOK_SWITCH;
					} else {
						pr_notice("[Accdet]Headset has plugged out\n");
					}
					mutex_unlock(&accdet_eint_irq_sync_mutex);
				}
			}
#else
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (1 == eint_accdet_sync_flag) {
				cable_type = HEADSET_NO_MIC;
				accdet_status = HOOK_SWITCH;
			} else {
				pr_notice("[Accdet]Headset has plugged out\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);
#endif
		} else if (current_status == 1) {
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (1 == eint_accdet_sync_flag) {
				accdet_status = MIC_BIAS;
				cable_type = HEADSET_MIC;
				/* AB=11 debounce=30ms */
				pmic_pwrap_write(ACCDET_DEBOUNCE3,
					cust_headset_settings->debounce3 * 30);
			} else {
				pr_notice("[Accdet]Headset has plugged out\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);
			pmic_pwrap_write(ACCDET_DEBOUNCE0, button_press_debounce);
			/*recover polling set AB 00-01*/
#ifdef CONFIG_ACCDET_PIN_RECOGNIZATION
			pmic_pwrap_write(ACCDET_PWM_WIDTH,
				REGISTER_VALUE(cust_headset_settings->pwm_width));
			pmic_pwrap_write(ACCDET_PWM_THRESH,
				REGISTER_VALUE(cust_headset_settings->pwm_thresh));
#endif
		} else if (current_status == 3) {
			pr_notice("[Accdet]PLUG_OUT state not change!\n");
#ifdef CONFIG_ACCDET_EINT
			pr_notice("[Accdet]do not send plug out event in plug out\n");
#else
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (1 == eint_accdet_sync_flag) {
				accdet_status = PLUG_OUT;
				cable_type = NO_DEVICE;
			} else {
				pr_notice("[Accdet]Headset has plugged out\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);
#endif
		} else {
			pr_notice("[Accdet]PLUG_OUT can't change to this state!\n");
		}
		break;

	case MIC_BIAS:
		/* solution: resume hook switch debounce time */
		pmic_pwrap_write(ACCDET_DEBOUNCE0, cust_headset_settings->debounce0);

		if (current_status == 0) {
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (1 == eint_accdet_sync_flag) {
				while ((pmic_pwrap_read(ACCDET_IRQ_STS) & IRQ_STATUS_BIT)
					&& (wait_clear_irq_times < 3)) {
					pr_notice("[Accdet]check_cable_type: MIC BIAS clear IRQ on-going1\n");
					wait_clear_irq_times++;
					msleep(20);
				}
				irq_temp = pmic_pwrap_read(ACCDET_IRQ_STS);
				irq_temp = irq_temp & (~IRQ_CLR_BIT);
				pmic_pwrap_write(ACCDET_IRQ_STS, irq_temp);
				IRQ_CLR_FLAG = true;
				accdet_status = HOOK_SWITCH;
			} else {
				pr_notice("[Accdet]Headset has plugged out\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);
			button_status = 1;
			if (button_status) {
				mutex_lock(&accdet_eint_irq_sync_mutex);
				if (1 == eint_accdet_sync_flag)
					multi_key_detection(current_status);
				else
					pr_notice("[Accdet]multi_key_detection: Headset has plugged out\n");
				mutex_unlock(&accdet_eint_irq_sync_mutex);
				accdet_auxadc_switch(0);
				/* recover  pwm frequency and duty */
				pmic_pwrap_write(ACCDET_PWM_WIDTH,
					REGISTER_VALUE(cust_headset_settings->pwm_width));
				pmic_pwrap_write(ACCDET_PWM_THRESH,
					REGISTER_VALUE(cust_headset_settings->pwm_thresh));
			}
		} else if (current_status == 1) {
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (1 == eint_accdet_sync_flag) {
				accdet_status = MIC_BIAS;
				cable_type = HEADSET_MIC;
				pr_notice("[Accdet]MIC_BIAS state not change!\n");
			} else {
				pr_notice("[Accdet]Headset has plugged out\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);
		} else if (current_status == 3) {
			pr_notice("[Accdet]do not send plug ou in micbiast\n");
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (1 == eint_accdet_sync_flag)
				accdet_status = PLUG_OUT;
			else
				pr_notice("[Accdet]Headset has plugged out\n");
			mutex_unlock(&accdet_eint_irq_sync_mutex);
		} else {
			pr_notice("[Accdet]MIC_BIAS can't change to this state!\n");
		}
		break;

	case HOOK_SWITCH:
		if (current_status == 0) {
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag == 1) {
				/* for avoid 01->00 framework of Headset */
				/* will report press key info for Audio */
				/* cable_type = HEADSET_NO_MIC; */
				/* accdet_status = HOOK_SWITCH; */
				pr_notice("[Accdet]HOOK_SWITCH state not change!\n");
			} else {
				pr_notice("[Accdet]Headset has plugged out\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);
		} else if (current_status == 1) {
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag == 1) {
				multi_key_detection(current_status);
				accdet_status = MIC_BIAS;
				cable_type = HEADSET_MIC;
			} else {
				pr_notice("[Accdet]Headset has plugged out\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);
			/* accdet_auxadc_switch(0); */
#ifdef CONFIG_ACCDET_PIN_RECOGNIZATION
			cable_pin_recognition = 0;
			pr_notice("[Accdet]cable_pin_recognition = %d\n",
				cable_pin_recognition);
			pmic_pwrap_write(ACCDET_PWM_WIDTH,
				REGISTER_VALUE(cust_headset_settings->pwm_width));
			pmic_pwrap_write(ACCDET_PWM_THRESH,
				REGISTER_VALUE(cust_headset_settings->pwm_thresh));
#endif
			/* solution: reduce hook switch debounce time to 0x400 */
			pmic_pwrap_write(ACCDET_DEBOUNCE0, button_press_debounce);
		} else if (current_status == 3) {

#ifdef CONFIG_ACCDET_PIN_RECOGNIZATION
			cable_pin_recognition = 0;
			pr_notice("[Accdet]cable_pin_recognition = %d\n",
				cable_pin_recognition);
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag == 1)
				accdet_status = PLUG_OUT;
			else
				pr_notice("[Accdet]Headset has plugged out\n");
			mutex_unlock(&accdet_eint_irq_sync_mutex);
#endif
#if defined CONFIG_ACCDET_EINT
			pr_notice("[Accdet]don't send plug out event in hook switch\n");
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag == 1)
				accdet_status = PLUG_OUT;
			else
				pr_notice("[Accdet] Headset has plugged out\n");
			mutex_unlock(&accdet_eint_irq_sync_mutex);
#endif
		} else {
			pr_notice("[Accdet]HOOK_SWITCH can't change to this state!\n");
		}
		break;
	case STAND_BY:
		if (current_status == 3) {
#if defined CONFIG_ACCDET_EINT
			pr_notice("[Accdet]accdet don't send plug out event in stand by\n");
#endif
		} else {
			pr_notice("[Accdet]STAND_BY can't change to this state\n");
		}
		break;

	default:
		pr_notice("[Accdet]check_cable_type:accdet current status error\n");
		break;

	}

	if (!IRQ_CLR_FLAG) {
		mutex_lock(&accdet_eint_irq_sync_mutex);
		if (eint_accdet_sync_flag == 1) {
			while ((pmic_pwrap_read(ACCDET_IRQ_STS) & IRQ_STATUS_BIT) &&
				(wait_clear_irq_times < 3)) {
				pr_notice("[Accdet]check_cable_type:Clear interrupt on-going2\n");
				wait_clear_irq_times++;
				msleep(20);
			}
		}
		irq_temp = pmic_pwrap_read(ACCDET_IRQ_STS);
		irq_temp = irq_temp & (~IRQ_CLR_BIT);
		pmic_pwrap_write(ACCDET_IRQ_STS, irq_temp);
		mutex_unlock(&accdet_eint_irq_sync_mutex);
		IRQ_CLR_FLAG = true;
		pr_notice("[Accdet]check_cable_type:Clear interrupt:Done[0x%x]!\n",
			pmic_pwrap_read(ACCDET_IRQ_STS));

	} else {
		IRQ_CLR_FLAG = false;
	}
}

static void accdet_work_callback(struct work_struct *work)
{
	__pm_stay_awake(accdet_irq_lock);
	check_cable_type();

#ifdef CONFIG_ACCDET_PIN_SWAP
#ifdef CONFIG_ACCDET_PIN_RECOGNIZATION
	if (cable_pin_recognition == 1) {
		cable_pin_recognition = 0;
		accdet_FSA8049_disable();
		cable_type = HEADSET_NO_MIC;
		accdet_status = PLUG_OUT;
	}
#endif
#endif
	mutex_lock(&accdet_eint_irq_sync_mutex);
	if (eint_accdet_sync_flag == 1)
		send_accdet_status_event(cable_type, 1);
	else
		pr_notice("[Accdet]Headset has plugged out,don't set accdet state\n");
	mutex_unlock(&accdet_eint_irq_sync_mutex);
	pr_notice("[Accdet] set state in cable_type status\n");
	__pm_relax(accdet_irq_lock);
}

void accdet_get_dts_data(void)
{
	struct device_node *node = NULL;
	int debounce[7];
	#ifdef CONFIG_FOUR_KEY_HEADSET
	int four_key[5];
	#else
	int three_key[4];
	#endif
	int ret = 0;

	pr_info("[ACCDET]Start accdet_get_dts_data");
	node = of_find_matching_node(node, accdet_of_match);
	if (node) {
		ret = of_property_read_u32_array(node, "headset-mode-setting",
			debounce, ARRAY_SIZE(debounce));
		if (ret)
			pr_notice("[Accdet]%s get field error\n", __func__);
		of_property_read_u32(node, "accdet-mic-vol",
			&accdet_dts_data.mic_mode_vol);
		of_property_read_u32(node, "accdet-plugout-debounce",
			&accdet_dts_data.accdet_plugout_debounce);
		of_property_read_u32(node, "accdet-mic-mode",
			&accdet_dts_data.accdet_mic_mode);
		#ifdef CONFIG_FOUR_KEY_HEADSET
		ret = of_property_read_u32_array(node,
				"headset-four-key-threshold",
				four_key, ARRAY_SIZE(four_key));
		if (ret)
			pr_notice("[Accdet]%s get field error\n", __func__);
		memcpy(&accdet_dts_data.four_key, four_key+1,
			sizeof(struct four_key_threshold));
		pr_info("[Accdet]mid-Key=%d,voice=%d,up_key=%d,down_key=%d\n",
		     accdet_dts_data.four_key.mid_key_four,
		     accdet_dts_data.four_key.voice_key_four,
		     accdet_dts_data.four_key.up_key_four,
		     accdet_dts_data.four_key.down_key_four);
		#else
		ret = of_property_read_u32_array(node,
				"headset-three-key-threshold",
				three_key, ARRAY_SIZE(three_key));
		if (ret)
			pr_notice("[Accdet]%s get field error\n", __func__);
		memcpy(&accdet_dts_data.three_key, three_key+1,
			sizeof(struct three_key_threshold));
		pr_info("[Accdet]mid-Key = %d, up_key = %d, down_key = %d\n",
		     accdet_dts_data.three_key.mid_key,
		     accdet_dts_data.three_key.up_key,
		     accdet_dts_data.three_key.down_key);
		#endif

		memcpy(&accdet_dts_data.headset_debounce, debounce, sizeof(debounce));
		cust_headset_settings = &accdet_dts_data.headset_debounce;
		pr_info("[Accdet]pwm_width=%x,pwm_thresh=%x\n",
		     cust_headset_settings->pwm_width,
		     cust_headset_settings->pwm_thresh);
		pr_info("[Accdet]deb0=%x,deb1=%x,mic_mode=%d\n",
		     cust_headset_settings->debounce0,
		     cust_headset_settings->debounce1,
		     accdet_dts_data.accdet_mic_mode);
	} else {
		pr_notice("[Accdet]%s can't find compatible dts node\n", __func__);
	}
}

static inline void accdet_init(void)
{
	pr_notice("[Accdet]accdet hardware init\n");
	/* clock */
	pmic_pwrap_write(TOP_CKPDN_CLR, RG_ACCDET_CLK_CLR);
	/* reset the accdet unit */
	pmic_pwrap_write(TOP_RST_ACCDET_SET, ACCDET_RESET_SET);
	pmic_pwrap_write(TOP_RST_ACCDET_CLR, ACCDET_RESET_CLR);
	/* init  pwm frequency and duty */
	pmic_pwrap_write(ACCDET_PWM_WIDTH,
		REGISTER_VALUE(cust_headset_settings->pwm_width));
	pmic_pwrap_write(ACCDET_PWM_THRESH,
		REGISTER_VALUE(cust_headset_settings->pwm_thresh));
	pmic_pwrap_write(ACCDET_STATE_SWCTRL, 0x07);

	/* rise and fall delay of PWM */
	pmic_pwrap_write(ACCDET_EN_DELAY_NUM,
			 (cust_headset_settings->fall_delay << 15 |
			 cust_headset_settings->rise_delay));
	/* init the debounce time */
#ifdef CONFIG_ACCDET_PIN_RECOGNIZATION
	pmic_pwrap_write(ACCDET_DEBOUNCE0, cust_headset_settings->debounce0);
	pmic_pwrap_write(ACCDET_DEBOUNCE1, 0xFFFF);	/*2.0s*/
	pmic_pwrap_write(ACCDET_DEBOUNCE3, cust_headset_settings->debounce3);
#else
	pmic_pwrap_write(ACCDET_DEBOUNCE0, cust_headset_settings->debounce0);
	pmic_pwrap_write(ACCDET_DEBOUNCE1, cust_headset_settings->debounce1);
	pmic_pwrap_write(ACCDET_DEBOUNCE3, cust_headset_settings->debounce3);
#endif
	/* enable INT */
	pmic_pwrap_write(ACCDET_IRQ_STS,
		pmic_pwrap_read(ACCDET_IRQ_STS) & (~IRQ_CLR_BIT));
	pmic_pwrap_write(INT_CON_ACCDET_SET, RG_ACCDET_IRQ_SET);

#if defined CONFIG_ACCDET_EINT
	/* disable ACCDET unit */
	pre_state_swctrl = pmic_pwrap_read(ACCDET_STATE_SWCTRL);
	pmic_pwrap_write(ACCDET_CTRL, ACCDET_DISABLE);
	pmic_pwrap_write(ACCDET_STATE_SWCTRL, 0x0);
	pmic_pwrap_write(TOP_CKPDN_SET, RG_ACCDET_CLK_SET);
#else
	/* enable ACCDET unit */
	pmic_pwrap_write(ACCDET_CTRL, ACCDET_ENABLE);
#endif
}

/* -sysfs- */
#if DEBUG_THREAD
static int dump_register(void)
{
	int i = 0;

	for (i = ACCDET_RSV; i <= ACCDET_RSV_CON1; i += 2)
		pr_notice(" ACCDET_BASE + %x=%x\n", i,
			pmic_pwrap_read(ACCDET_BASE + i));

	pr_notice(" TOP_RST_ACCDET(0x%x)=%x\n",
		TOP_RST_ACCDET, pmic_pwrap_read(TOP_RST_ACCDET));
	pr_notice(" INT_CON_ACCDET(0x%x)=%x\n",
		INT_CON_ACCDET, pmic_pwrap_read(INT_CON_ACCDET));
	pr_notice(" TOP_CKPDN(0x%x)=%x\n",
		TOP_CKPDN, pmic_pwrap_read(TOP_CKPDN));
	return 0;
}

static ssize_t call_state_store(struct device_driver *ddri,
	const char *buf, size_t count)
{
	int ret;

	ret = kstrtoint(buf, 10, &call_status);
	if (ret != 0) {
		pr_notice("accdet: Invalid values\n");
		return -EINVAL;
	}

	switch (call_status) {
	case CALL_IDLE:
		pr_notice("[Accdet]accdet call: Idle state!\n");
		break;

	case CALL_RINGING:

		pr_notice("[Accdet]accdet call: ringing state!\n");
		break;

	case CALL_ACTIVE:
		pr_notice("[Accdet]accdet call: active or hold state!\n");
		break;

	default:
		pr_notice("[Accdet]accdet call : Invalid values\n");
		break;
	}
	return count;
}

static ssize_t pin_recognition_show(struct device_driver *ddri,
	char *buf)
{
#ifdef CONFIG_ACCDET_PIN_RECOGNIZATION
	pr_notice("ACCDET show_pin_recognition_state = %d\n",
		cable_pin_recognition);
	return sprintf(buf, "%u\n", cable_pin_recognition);
#else
	return sprintf(buf, "%u\n", 0);
#endif
}

static int g_start_debug_thread;
static struct task_struct *thread;
static int g_dump_register;
static int dbug_thread(void *unused)
{
	while (g_start_debug_thread) {
		if (g_dump_register) {
			dump_register();
			/*dump_pmic_register();*/
		}

		msleep(500);

	}
	return 0;
}

static ssize_t start_debug_store(struct device_driver *ddri,
	const char *buf, size_t count)
{

	int start_flag;
	int error;
	int ret;

	ret = kstrtoint(buf, 10, &start_flag);
	if (ret != 0) {
		pr_notice("accdet: Invalid values\n");
		return -EINVAL;
	}

	pr_notice("[Accdet] start flag =%d\n", start_flag);

	g_start_debug_thread = start_flag;

	if (start_flag == 1) {
		thread = kthread_run(dbug_thread, 0, "ACCDET");
		if (IS_ERR(thread)) {
			error = PTR_ERR(thread);
			pr_notice(" failed to create kernel thread: %d\n", error);
		}
	}

	return count;
}

static ssize_t set_headset_mode_store(struct device_driver *ddri,
	const char *buf, size_t count)
{

	int value;
	int ret;

	ret = kstrtoint(buf, 10, &value);
	if (ret != 0) {
		pr_notice("accdet: Invalid values\n");
		return -EINVAL;
	}

	pr_notice("[Accdet]store_accdet_set_headset_mode value=%d\n", value);

	return count;
}

static ssize_t dump_register_store(struct device_driver *ddri,
	const char *buf, size_t count)
{
	int value;
	int ret;

	ret = kstrtoint(buf, 10, &value);
	if (ret != 0) {
		pr_notice("accdet: Invalid values\n");
		return -EINVAL;
	}

	g_dump_register = value;

	pr_notice("[Accdet]store_accdet_dump_register value=%d\n", value);

	return count;
}

static ssize_t state_show(struct device_driver *ddri, char *buf)
{
	char temp_type = (char)cable_type;
	int ret = 0;

	if (buf == NULL) {
		pr_notice("[%s] *buf is NULL Pointer\n",  __func__);
		return -EINVAL;
	}

	ret = snprintf(buf, 3, "%d\n", temp_type);
	if (ret < 0)
		pr_notice("[Accdet]snprintf failed\n");

	return strlen(buf);
}

static DRIVER_ATTR_WO(start_debug);
static DRIVER_ATTR_WO(set_headset_mode);
static DRIVER_ATTR_WO(dump_register);
static DRIVER_ATTR_WO(call_state);
static DRIVER_ATTR_RO(state);
static DRIVER_ATTR_RO(pin_recognition);

static struct driver_attribute *accdet_attr_list[] = {
	&driver_attr_start_debug,
	&driver_attr_set_headset_mode,
	&driver_attr_dump_register,
	&driver_attr_call_state,
	&driver_attr_state,
	&driver_attr_pin_recognition,
};

static int accdet_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(accdet_attr_list) / sizeof(accdet_attr_list[0]));

	if (driver == NULL)
		return -EINVAL;
	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, accdet_attr_list[idx]);
		if (err) {
			pr_notice("driver_create_file (%s) = %d\n",
				accdet_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

#endif

int mt_accdet_probe(struct platform_device *dev)
{
	int ret = 0;

#if DEBUG_THREAD
	struct platform_driver accdet_driver_hal = accdet_driver_func();
#endif

	pr_info("[Accdet]accdet_probe begin!\n");

	/* Create normal device for auido use */
	ret = alloc_chrdev_region(&accdet_devno, 0, 1, ACCDET_DEVNAME);
	if (ret)
		pr_notice("[Accdet]alloc_chrdev_region failed\n");

	accdet_cdev = cdev_alloc();
	accdet_cdev->owner = THIS_MODULE;
	accdet_cdev->ops = accdet_get_fops();
	ret = cdev_add(accdet_cdev, accdet_devno, 1);
	if (ret)
		pr_notice("[Accdet]cdev_add failed\n");

	accdet_class = class_create(THIS_MODULE, ACCDET_DEVNAME);

	/* if we want auto creat device node, we must call this */
	accdet_nor_device = device_create(accdet_class, NULL,
		accdet_devno, NULL, ACCDET_DEVNAME);

	/* Create input device */
	kpd_accdet_dev = input_allocate_device();
	if (!kpd_accdet_dev) {
		pr_notice("[Accdet]kpd_accdet_dev: fail!\n");
		return -ENOMEM;
	}
	/* INIT the timer to disable micbias */
	timer_setup(&micbias_timer, disable_micbias, 0);
	micbias_timer.expires = jiffies + MICBIAS_DISABLE_TIMER;

	/* define multi-key keycode */
	__set_bit(EV_KEY, kpd_accdet_dev->evbit);
	__set_bit(KEY_PLAYPAUSE, kpd_accdet_dev->keybit);
	__set_bit(KEY_VOLUMEDOWN, kpd_accdet_dev->keybit);
	__set_bit(KEY_VOLUMEUP, kpd_accdet_dev->keybit);
	__set_bit(KEY_VOICECOMMAND, kpd_accdet_dev->keybit);

	__set_bit(EV_SW, kpd_accdet_dev->evbit);
	__set_bit(SW_HEADPHONE_INSERT, kpd_accdet_dev->swbit);
	__set_bit(SW_MICROPHONE_INSERT, kpd_accdet_dev->swbit);
	__set_bit(SW_JACK_PHYSICAL_INSERT, kpd_accdet_dev->swbit);
	__set_bit(SW_LINEOUT_INSERT, kpd_accdet_dev->swbit);


	kpd_accdet_dev->id.bustype = BUS_HOST;
	kpd_accdet_dev->name = "ACCDET";
	if (input_register_device(kpd_accdet_dev))
		pr_notice("[Accdet]kpd_accdet_dev register : fail!\n");
	/* Create workqueue */
	accdet_workqueue = create_singlethread_workqueue("accdet");
	INIT_WORK(&accdet_work, accdet_work_callback);

	/* wake lock */
	accdet_irq_lock = wakeup_source_register(NULL, "accdet_irq_lock");
	if (!accdet_irq_lock)
		return -ENOMEM;
	accdet_timer_lock = wakeup_source_register(NULL, "accdet_timer_lock");
	if (!accdet_timer_lock)
		return -ENOMEM;

#if DEBUG_THREAD
	ret = accdet_create_attr(&accdet_driver_hal.driver);
	if (ret != 0)
		pr_notice("create attribute err = %d\n", ret);
#endif
	pr_info("[Accdet]accdet_probe : ACCDET_INIT\n");
	if (g_accdet_first == 1) {
		eint_accdet_sync_flag = 1;
		/* Accdet Hardware Init */
		pmic_pwrap_write(ACCDET_RSV, ACCDET_1V9_MODE_OFF);
		accdet_get_dts_data();
		accdet_init();

		/* schedule a work for the first detection */
		queue_work(accdet_workqueue, &accdet_work);
#ifdef CONFIG_ACCDET_EINT
		accdet_disable_workqueue = \
			create_singlethread_workqueue("accdet_disable");
		INIT_WORK(&accdet_disable_work, disable_micbias_callback);
		accdet_eint_workqueue = \
			create_singlethread_workqueue("accdet_eint");
		INIT_WORK(&accdet_eint_work, accdet_eint_work_callback);
		accdet_setup_eint(dev);
#endif
		g_accdet_first = 0;
	}
	pr_info("[Accdet]accdet_probe done!\n");
	return 0;
}

void mt_accdet_remove(void)
{
	pr_notice("[Accdet]accdet_remove begin!\n");

	/* cancel_delayed_work(&accdet_work); */
#if defined CONFIG_ACCDET_EINT
	destroy_workqueue(accdet_eint_workqueue);
#endif
	destroy_workqueue(accdet_workqueue);
	device_del(accdet_nor_device);
	class_destroy(accdet_class);
	cdev_del(accdet_cdev);
	unregister_chrdev_region(accdet_devno, 1);
	input_unregister_device(kpd_accdet_dev);
	pr_notice("[Accdet]accdet_remove Done!\n");
}

/* only one suspend mode */
void mt_accdet_suspend(void)
{

#if defined CONFIG_ACCDET_EINT
	pr_notice("[Accdet]in suspend: ACCDET_IRQ_STS = 0x%x\n",
		pmic_pwrap_read(ACCDET_IRQ_STS));
#else
	pr_notice("[Accdet]suspend:ACCDET_CTRL=[0x%x],STATE=[0x%x]->[0x%x]\n",
	       pmic_pwrap_read(ACCDET_CTRL), pre_state_swctrl,
	       pmic_pwrap_read(ACCDET_STATE_SWCTRL));
#endif
}

/* wake up */
void mt_accdet_resume(void)
{
#if defined CONFIG_ACCDET_EINT
	pr_notice("[Accdet]in resume1:ACCDET_IRQ_STS = 0x%x\n",
		pmic_pwrap_read(ACCDET_IRQ_STS));
#else
	pr_notice("[Accdet]resume: ACCDET_CTRL=[0x%x],STATE_SWCTRL=[0x%x]\n",
	       pmic_pwrap_read(ACCDET_CTRL),
	       pmic_pwrap_read(ACCDET_STATE_SWCTRL));

#endif

}

/* add for IPO-H need update headset state when resume */
#ifdef CONFIG_ACCDET_PIN_RECOGNIZATION
struct timer_list accdet_disable_ipoh_timer;
static void mt_accdet_pm_disable(unsigned long a)
{
	if (cable_type == NO_DEVICE && eint_accdet_sync_flag == 0) {
		/* disable accdet */
		pre_state_swctrl = pmic_pwrap_read(ACCDET_STATE_SWCTRL);
		pmic_pwrap_write(ACCDET_STATE_SWCTRL, 0);
#ifdef CONFIG_ACCDET_EINT
		pmic_pwrap_write(ACCDET_CTRL, ACCDET_DISABLE);
		/* disable clock */
		pmic_pwrap_write(TOP_CKPDN_SET, RG_ACCDET_CLK_SET);
#endif
		pr_notice("[Accdet]daccdet_pm_disable: disable!\n");
	} else {
		pr_notice("[Accdet]daccdet_pm_disable: enable!\n");
	}
}
#endif
void mt_accdet_pm_restore_noirq(void)
{
	int current_status_restore = 0;

	pr_notice("[Accdet]pm_restore_noirq start!\n");
	/* enable ACCDET unit */
	pr_notice("accdet: enable_accdet\n");
	/* enable clock */
	pmic_pwrap_write(TOP_CKPDN_CLR, RG_ACCDET_CLK_CLR);
	enable_accdet(ACCDET_SWCTRL_EN);
	pmic_pwrap_write(ACCDET_STATE_SWCTRL,
		(pmic_pwrap_read(ACCDET_STATE_SWCTRL) | ACCDET_SWCTRL_IDLE_EN));

	eint_accdet_sync_flag = 1;
	current_status_restore = ((pmic_pwrap_read(ACCDET_STATE_RG) & 0xc0) >> 6);

	switch (current_status_restore) {
	case 0:/* AB=0 */
		cable_type = HEADSET_NO_MIC;
		accdet_status = HOOK_SWITCH;
		send_accdet_status_event(cable_type, 1);
		break;
	case 1:/* AB=1 */
		cable_type = HEADSET_MIC;
		accdet_status = MIC_BIAS;
		send_accdet_status_event(cable_type, 1);
		break;
	case 3:/* AB=3 */
		send_accdet_status_event(cable_type, 0);
		cable_type = NO_DEVICE;
		accdet_status = PLUG_OUT;
		break;
	default:
		pr_notice("[Accdet]pm_restore_noirq:current status error\n");
		break;
	}
	if (cable_type == NO_DEVICE) {
#ifdef CONFIG_ACCDET_PIN_RECOGNIZATION
		init_timer(&accdet_disable_ipoh_timer);
		accdet_disable_ipoh_timer.expires = jiffies + 3 * HZ;
		accdet_disable_ipoh_timer.function = &mt_accdet_pm_disable;
		accdet_disable_ipoh_timer.data = ((unsigned long)0);
		add_timer(&accdet_disable_ipoh_timer);
		pr_notice("[Accdet]enable! pm timer\n");

#else
		/* disable accdet */
		pre_state_swctrl = pmic_pwrap_read(ACCDET_STATE_SWCTRL);
#ifdef CONFIG_ACCDET_EINT
		pmic_pwrap_write(ACCDET_STATE_SWCTRL, 0);
		pmic_pwrap_write(ACCDET_CTRL, ACCDET_DISABLE);
		/* disable clock */
		pmic_pwrap_write(TOP_CKPDN_SET, RG_ACCDET_CLK_SET);
#endif
#endif
	}
}
/* IPO_H end */

long mt_accdet_unlocked_ioctl(unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case ACCDET_INIT:
		break;
	case SET_CALL_STATE:
		call_status = (int)arg;
		pr_notice("[Accdet]accdet_ioctl:CALL_STATE=%d\n",
			call_status);
		break;
	case GET_BUTTON_STATUS:
		return button_status;
	default:
		pr_notice("[Accdet]accdet_ioctl:default\n");
		break;
	}
	return 0;
}
