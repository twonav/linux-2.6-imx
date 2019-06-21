/*
 * bd7181x-power.c
 * @file ROHM BD71815/BD71817 Charger driver
 *
 * Copyright 2014 Embest Technology Co. Ltd. Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/power_supply.h>
#include <linux/mfd/bd7181x.h>
#include <linux/delay.h>

#include <linux/debugfs.h>
#include <asm/siginfo.h>
#include <linux/rcupdate.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/pid.h>

extern enum twonav_hw_types twonav_hw_type;

#if 0
#define bd7181x_info	dev_info
#else
#define bd7181x_info(...)
#endif

#define JITTER_DEFAULT		3000		/* seconds */
#define JITTER_REPORT_CAP	10000		/* seconds */

#ifdef TWONAV_VELO
	#define BD7181X_BATTERY_CAP_MAH		1650 // mAh
	#define VBAT_CHG1					0x18 // 4.2V
	#define VBAT_CHG2					0x13 // 4.1V
	#define VBAT_CHG3					0x10 // 4.04V
	#define DCIN_ANTICOLAPSE_VOLTAGE 	0x34 // 4.24V - register 0x43 (80mV steps)
	#define ITERM_CURRENT 				0x02 // 0.01C typical value 1650*0.01=16.5mA -> 20mA
	#define OVP_MNT_THR 				0x16 // OVP:4.25V Recharge-threshold: VBAT_CHG1/2/3 - 0.05V
#elif defined TWONAV_HORIZON
	#define BD7181X_BATTERY_CAP_MAH		1500 // mAh
	#define VBAT_CHG1					0x1F // 4.34V
	#define VBAT_CHG2					0x1A // 4.24V
	#define VBAT_CHG3					0x15 // 4.14V
	#define DCIN_ANTICOLAPSE_VOLTAGE 	0x36 // 4.4V
	#define ITERM_CURRENT 				0x02 // 0.01C typical value 1500*0.01=15mA -> 20mA
	#define OVP_MNT_THR 				0x46 // OVP:4.4V Recharge-threshold: VBAT_CHG1/2/3 - 0.05V
#elif defined TWONAV_TRAIL
	#define BD7181X_BATTERY_CAP_MAH		4000 // mAh
	#define VBAT_CHG1					0x18 // 4.2V
	#define VBAT_CHG2					0x13 // 4.1V
	#define VBAT_CHG3					0x10 // 4.04V
	#define DCIN_ANTICOLAPSE_VOLTAGE 	0x34 // 4.24V
	#define ITERM_CURRENT 				0x05 // 0.01C typical value 4000*0.01=40mA -> 50mA
	#define OVP_MNT_THR 				0x16 // OVP:4.25V Recharge-threshold: VBAT_CHG1/2/3 - 0.05V
#elif defined TWONAV_AVENTURA
	#define BD7181X_BATTERY_CAP_MAH		5000 // mAh
	#define VBAT_CHG1					0x18 // 4.2V
	#define VBAT_CHG2					0x13 // 4.1V
	#define VBAT_CHG3					0x10 // 4.04V
	#define DCIN_ANTICOLAPSE_VOLTAGE 	0x34 // 4.24V
	#define ITERM_CURRENT 				0x06 // 0.01C typical value 5000*0.01=50mA -> 100mA
	#define OVP_MNT_THR 				0x16 // OVP:4.25V Recharge-threshold: VBAT_CHG1/2/3 - 0.05V
#else
	#define BD7181X_BATTERY_CAP_MAH		1650
	#define VBAT_CHG1					0x18
	#define VBAT_CHG2					0x13
	#define VBAT_CHG3					0x10
	#define DCIN_ANTICOLAPSE_VOLTAGE 	0x36
	#define ITERM_CURRENT 				0x02
	#define OVP_MNT_THR 				0x16
#endif

#define BD7181X_BATTERY_CAP	mAh_A10s(BD7181X_BATTERY_CAP_MAH)
#define MAX_VOLTAGE		ocv_table[0]
#define MIN_VOLTAGE		3000000 // bellow this value soc -> 0
#define THR_VOLTAGE		3800000 // There is no charging if Vsys is less than 3.8V
#define MAX_CURRENT		1000000	// uA - changing this doesn't change max charger current
#define TRICKLE_CURRENT		25000	// uA
#define PRECHARGE_CURRENT	300000	// uA
#define FAST_CHARGE_CURRENT 10	// 0x0A -> 1A - 100mA steps- TOTALLY RELATED WITH MAX_CURRENT - changing this changes current value
#define FAST_CHARGE_CURRENT_MIN_VALUE 1
#define FAST_CHARGE_CURRENT_MAX_VALUE 10

#define AC_NAME			"bd7181x_ac"
#define BAT_NAME		"bd7181x_bat"
#define BD7181X_BATTERY_FULL	100

// used to set initial coulomb counter CC based on battery voltage OCV
#define BY_BAT_VOLT		0
#define BY_VBATLOAD_REG		1
#define INIT_COULOMB		BY_VBATLOAD_REG

//VBAT Low voltage detection Threshold
#define VBAT_LOW_TH		0x00BF // 0x00BF (191) * 16mV = 3.056V

//#define RS_30mOHM		/* we have 10mOhm */

#ifdef RS_30mOHM
#define A10s_mAh(s)		((s) * 1000 / (360 * 3))
#define mAh_A10s(m)		((m) * (360 * 3) / 1000)
#else
#define A10s_mAh(s)		((s) * 1000 / 360)
#define mAh_A10s(m)		((m) * 360 / 1000)
#endif

#define THR_RELAX_CURRENT	10		/* mA */ // Coulomb counter related
#define THR_RELAX_TIME		(60 * 60)	/* sec. */ // Coulomb counter related

#define BD7181X_DGRD_CYC_CAP	26	/* 1 micro Ah unit How much capacity we lose after each cycle */

#define BD7181X_DGRD_TEMP_M	25	/* 1 degrees C unit */ // CALC FULL CAPACITY
#define BD7181X_DGRD_TEMP_L	5	/* 1 degrees C unit */ // CALC FULL CAPACITY
#define BD7181X_DGRD_TEMP_CAP_H	(0)	/* 1 micro Ah unit */ // CALC FULL CAPACITY
#define BD7181X_DGRD_TEMP_CAP_M	(1187)	/* 1 micro Ah unit */ // CALC FULL CAPACITY
#define BD7181X_DGRD_TEMP_CAP_L	(5141)	/* 1 micro Ah unit */ // CALC FULL CAPACITY

// Bellow values are limmits that exist in ocv_table
#define CANCEL_ADJ_COULOMB_SOC_H_1	700	/* unit 0.1% */ // ADJUST COULOMB COUNTER & SW
#define CANCEL_ADJ_COULOMB_SOC_L_1	550	/* unit 0.1% */ // ADJUST COULOMB COUNTER & SW
#define CANCEL_ADJ_COULOMB_SOC_H_2	350	/* unit 0.1% */ // ADJUST COULOMB COUNTER & SW
#define CANCEL_ADJ_COULOMB_SOC_L_2	0	/* unit 0.1% */ // ADJUST COULOMB COUNTER & SW

#define FORCE_ADJ_COULOMB_TEMP_H	35	/* 1 degrees C unit */
#define FORCE_ADJ_COULOMB_TEMP_L	15	/* 1 degrees C unit */

unsigned int battery_cycle;

// Variables related to low battery signal
static struct dentry *bd7181x_dir;
static struct dentry *related_process_pid_file;
static int related_process_pid = 0;


static ssize_t send_signal(int type)
{
	int ret;
	struct siginfo info;
    struct task_struct *task;

    if (related_process_pid == 0) {
        printk(KERN_DEBUG "BD7181x-power: no pid related to send signal type :%d\n", type);
        return EPERM;
    }

    memset(&info, 0, sizeof(struct siginfo));
    info.si_signo = type;
    info.si_code = SI_USER;

    rcu_read_lock();
    task = pid_task(find_pid_ns(related_process_pid, &init_pid_ns), PIDTYPE_PID);
    if(task == NULL){
        printk(KERN_DEBUG "BD7181x-power: no such pid :%d\n",related_process_pid);
        related_process_pid = 0;
        rcu_read_unlock();
        return -ENODEV;
    }

    rcu_read_unlock();
    ret = send_sig_info(type, &info, task);
    if (ret < 0) {
        printk(KERN_DEBUG "BD7181x-power: error sending signal\n");
    }

    return ret;
}


static ssize_t write_related_pid(struct file *file,
								 const char __user *buf,
								 size_t count,
								 loff_t *ppos)
{
    char mybuf[10];
    if(count > 10)
        return -EINVAL;
    if (copy_from_user(mybuf, buf, count))
        return -EFAULT;
    sscanf(mybuf, "%d", &related_process_pid);

    return count;
}

static const struct file_operations related_pid_fops = {
    .write = write_related_pid,
};


#ifdef TWONAV_VELO
	static int ocv_table[] = {
		4200000,
		4114000,
		4070000,
		4016000,
		3977000,
		3943000,
		3899000,
		3865000,
		3831000,
		3797000,
		3767000,
		3743000,
		3728000,
		3719000,
		3709000,
		3699000,
		3675000,
		3640000,
		3587000,
		3518000,
		3470000,
		3343000,
		3000000
	};	/* unit 1 micro V */
#elif defined TWONAV_HORIZON
	static int ocv_table[] = {
		4350000,
		4270000,
		4202000,
		4148000,
		4099000,
		4050000,
		4011000,
		3948000,
		3914000,
		3875000,
		3845000,
		3816000,
		3792000,
		3772000,
		3753000,
		3743000,
		3728000,
		3714000,
		3694000,
		3665000,
		3645000,
		3582000,
		3000000
	};
#elif defined TWONAV_TRAIL
	static int ocv_table[] = {
		4200000,
		4165665,
		4136689,
		4069218,
		4008334,
		3953394,
		3903754,
		3858772,
		3817803,
		3780204,
		3745332,
		3712543,
		3681193,
		3650640,
		3620240,
		3589350,
		3557325,
		3523523,
		3487300,
		3448012,
		3405017,
		3357671,
		3305330
	};
#elif defined TWONAV_AVENTURA
static int ocv_table[] = {
		4200000,
		4148000,
		4104000,
		4055000,
		4011000,
		3972000,
		3940000,
		3909000,
		3880000,
		3845000,
		3811000,
		3792000,
		3772000,
		3762000,
		3753000,
		3748000,
		3733000,
		3719000,
		3694000,
		3665000,
		3645000,
		3372000,
		2850000
	};	/* unit 1 micro V */
#else
	static int ocv_table[] = {
		4200000,
		4167456,
		4109781,
		4065242,
		4025618,
		3989877,
		3958031,
		3929302,
		3900935,
		3869637,
		3838475,
		3815196,
		3799778,
		3788385,
		3779627,
		3770675,
		3755368,
		3736049,
		3713545,
		3685118,
		3645278,
		3465599,
		2830610
	};	/* unit 1 micro V */

#endif

static int soc_table[] = {
	1000,
	1000,
	950,
	900,
	850,
	800,
	750,
	700,
	650,
	600,
	550,
	500,
	450,
	400,
	350,
	300,
	250,
	200,
	150,
	100,
	50,
	0,
	-50
	/* unit 0.1% */
};

/** @brief power deivce */
struct bd7181x_power {
	struct device *dev;
	struct bd7181x *mfd;			/**< multi-function device parent for access register */
	struct power_supply *ac;		/**< alternating current power */
	struct power_supply *bat;		/**< battery power */
	struct delayed_work bd_work;	/**< delayed work for timed work */

	int	reg_index;			/**< register address saved for sysfs ???? */

	int vbus_status;		/**< last vbus status */
	int charge_status;		/**< last charge status */
	int charge_type;
	int bat_status;			/**< last bat status */

	int	hw_ocv1;			/**< HW open charge voltage 1. Measured Battery Voltage (1st time) at boot */
	int	hw_ocv2;			/**< HW ocv2 . Measured Battery Voltage (2nd time) at boot*/
	int	bat_online;			/**< battery connect - by checking BAT_TEMP */
	int	charger_online;		/**< charger connect - by VBAT_DET */
	int	vcell;				/**< battery voltage */
	int	vsys;				/**< system voltage */
	int	vcell_min;			/**< Latest minimum Battery Voltage (simple average) */
	int	vsys_min;			/**< Latest minimum VSYS voltage (simple average) */
	int	rpt_status;			/**< battery status report */
	int	prev_rpt_status;	/**< previous battery status report */
	int	bat_health;			/**< battery health */
	int	designed_cap;		/**< battery designed capacity */
	int	full_cap;			/**< battery capacity */
	int	curr;				/**< battery current from DS-ADC */
	int	curr_sar;			/**< battery current from VM_IBAT */
	int	temp;				/**< battery tempature */
	u32	coulomb_cnt;		/**< Coulomb Counter */
	int	state_machine;		/**< initial-procedure state machine - POWER_ON / INITIALIZED */

	u32	soc_org;			/**< State Of Charge using designed capacity without by load */
	u32	soc_norm;			/**< State Of Charge using full capacity without by load */
	u32	soc;				/**< State Of Charge using full capacity with by load */
	u32	clamp_soc;			/**< Clamped State Of Charge using full capacity with by load */

	int	relax_time;			/**< Relax Time */

	u32	cycle;				/**< Charging and Discharging cycle number */
	volatile int calib_current;		/**< calibration current */
};


#define CALIB_NORM			0
#define CALIB_START			1
#define CALIB_GO			2

enum {
	STAT_POWER_ON,
	STAT_INITIALIZED,
};

static int bd7181x_calc_soc_org(struct bd7181x_power* pwr);

/** @brief read a register group once
 *  @param mfd bd7181x device
 *  @param reg	 register address of lower register
 *  @return register value
 */
#ifdef __BD7181X_REGMAP_H__
static u16 bd7181x_reg_read16(struct bd7181x* mfd, int reg) {
	u16 v;

	v = (u16)bd7181x_reg_read(mfd, reg) << 8;
	v |= (u16)bd7181x_reg_read(mfd, reg + 1) << 0;
	return v;
}
#else
static u16 bd7181x_reg_read16(struct bd7181x* mfd, int reg) {
	union {
		u16 long_type;
		char chars[2];
	} u;
	int r;

	r = regmap_bulk_read(mfd->regmap, reg, u.chars, sizeof u.chars);
	if (r) {
		return -1;
	}
	return be16_to_cpu(u.long_type);
}
#endif

/** @brief write a register group once
 * @param mfd bd7181x device
 * @param reg register address of lower register
 * @param val value to write
 * @retval 0 success
 * @retval -1 fail
 */
static int bd7181x_reg_write16(struct bd7181x *mfd, int reg, u16 val) {
	union {
		u16 long_type;
		char chars[2];
	} u;
	int r;

	u.long_type = cpu_to_be16(val);
	// printk("write16 0x%.4X 0x%.4X\n", val, u.long_type);
#ifdef __BD7181X_REGMAP_H__
	r = mfd->write(mfd, reg, sizeof u.chars, u.chars);
#else
	r = regmap_bulk_write(mfd->regmap, reg, u.chars, sizeof u.chars);
#endif
	if (r) {
		return -1;
	}
	return 0;	
}

/** @brief read quad register once
 *  @param mfd bd7181x device
 *  @param reg	 register address of lower register
 *  @return register value
 */
static int bd7181x_reg_read32(struct bd7181x *mfd, int reg) {
	union {
		u32 long_type;
		char chars[4];
	} u;
	int r;

#ifdef __BD7181X_REGMAP_H__
	r = mfd->read(mfd, reg, sizeof u.chars, u.chars);
#else
	r = regmap_bulk_read(mfd->regmap, reg, u.chars, sizeof u.chars);
#endif
	if (r) {
		return -1;
	}
	return be32_to_cpu(u.long_type);
}

#if INIT_COULOMB == BY_VBATLOAD_REG
/** @brief get initial battery Open Circuit Voltages at PMIC boot
 * @param pwr power device
 * @return 0
 */
static int bd7181x_get_init_bat_stat(struct bd7181x_power *pwr) {
	struct bd7181x *mfd = pwr->mfd;
	int vcell;

	vcell = bd7181x_reg_read16(mfd, BD7181X_REG_VM_OCV_PRE_U) * 1000;
	bd7181x_info(pwr->dev, "VM_OCV_PRE = %d\n", vcell);
	pwr->hw_ocv1 = vcell;

	vcell = bd7181x_reg_read16(mfd, BD7181X_REG_VM_OCV_PST_U) * 1000;
	bd7181x_info(pwr->dev, "VM_OCV_PST = %d\n", vcell);
	pwr->hw_ocv2 = vcell;

	return 0;
}
#endif

/** @brief get battery average current
 * @param pwr power device
 * @param vcell pointer to return back voltage in unit uV.
 * @param curr  pointer to return back current in unit uA.
 * @return 0
 */
static int bd7181x_get_vbat_curr(struct bd7181x_power *pwr, int *vcell, int *curr) {
	struct bd7181x* mfd = pwr->mfd;
	int tmp_vcell, tmp_curr;

	tmp_vcell = 0;
	tmp_curr = 0;

	tmp_vcell = bd7181x_reg_read16(mfd, BD7181X_REG_VM_SA_VBAT_U); // SA: Simple Average
	tmp_curr = bd7181x_reg_read16(mfd, BD7181X_REG_VM_SA_IBAT_U);
	if (tmp_curr & IBAT_SA_DIR_Discharging) {
		tmp_curr = -(tmp_curr & ~IBAT_SA_DIR_Discharging);
	}

	*vcell = tmp_vcell * 1000;
#ifdef RS_30mOHM
	*curr = tmp_curr * 1000 / 3;
#else
	*curr = tmp_curr * 1000;
#endif
	return 0;
}

/** @brief get battery current from DS-ADC
 * @param pwr power device
 * @return current in unit uA
 */
static int bd7181x_get_current_ds_adc(struct bd7181x_power *pwr) {
	int r;
	
	r = bd7181x_reg_read16(pwr->mfd, BD7181X_REG_CC_CURCD_U);
	if (r < 0) {
		return 0;
	}
	if (r & CURDIR_Discharging) {
		r = -(r & ~CURDIR_Discharging);
	}
#ifdef RS_30mOHM
	return r * 1000 / 3;
#else
	return r * 1000;
#endif
}

/** @brief get system average voltage
 * @param pwr power device
 * @param vcell pointer to return back voltage in unit uV.
 * @return 0
 */
static int bd7181x_get_vsys(struct bd7181x_power *pwr, int *vsys) {
	struct bd7181x* mfd = pwr->mfd;
	int tmp_vsys;

	tmp_vsys = 0;

	tmp_vsys = bd7181x_reg_read16(mfd, BD7181X_REG_VM_SA_VSYS_U);

	*vsys = tmp_vsys * 1000;

	return 0;
}

/** @brief get battery minimum average voltage
 * @param pwr power device
 * @param vcell pointer to return back voltage in unit uV.
 * @return 0
 */
static int bd7181x_get_vbat_min(struct bd7181x_power *pwr, int *vcell) {
	struct bd7181x* mfd = pwr->mfd;
	int tmp_vcell;

	tmp_vcell = 0;

	tmp_vcell = bd7181x_reg_read16(mfd, BD7181X_REG_VM_SA_VBAT_MIN_U);
	bd7181x_set_bits(pwr->mfd, BD7181X_REG_VM_SA_MINMAX_CLR, VBAT_SA_MIN_CLR);

	*vcell = tmp_vcell * 1000;

	return 0;
}

/** @brief get system minimum average voltage
 * @param pwr power device
 * @param vcell pointer to return back voltage in unit uV.
 * @return 0
 */
static int bd7181x_get_vsys_min(struct bd7181x_power *pwr, int *vcell) {
	struct bd7181x* mfd = pwr->mfd;
	int tmp_vcell;

	tmp_vcell = 0;

	tmp_vcell = bd7181x_reg_read16(mfd, BD7181X_REG_VM_SA_VSYS_MIN_U);
	bd7181x_set_bits(pwr->mfd, BD7181X_REG_VM_SA_MINMAX_CLR, VSYS_SA_MIN_CLR);

	*vcell = tmp_vcell * 1000;

	return 0;
}

/** @brief get battery capacity
 * @param ocv open circuit voltage
 * @return capacity in unit 0.1 percent
 */
static int bd7181x_voltage_to_capacity(int ocv) {
	int i = 0;
	int soc;

	if (ocv > ocv_table[0]) {
		soc = soc_table[0];
	} else {
		i = 0;
		while (soc_table[i] != -50) {
			if ((ocv <= ocv_table[i]) && (ocv > ocv_table[i+1])) {
				soc = (soc_table[i] - soc_table[i+1]) * (ocv - ocv_table[i+1]) / (ocv_table[i] - ocv_table[i+1]);
				soc += soc_table[i+1];
				break;
			}
			i++;
		}
		if (soc_table[i] == -50)
			soc = soc_table[i];
	}

	return soc;
}

/** @brief get battery temperature
 * @param pwr power device
 * @return temperature in unit deg.Celsius
 */
static int bd7181x_get_temp(struct bd7181x_power *pwr) {
	struct bd7181x* mfd = pwr->mfd;
	int t;

	t = 200 - (int)bd7181x_reg_read(mfd, BD7181X_REG_VM_BTMP);

	// battery temperature error
	t = (t > 200)? 200: t;
	return t;
}

static int bd7181x_reset_coulomb_count_at_full_charge(struct bd7181x_power* pwr);

/** @brief get battery charge status
 * @param pwr power device
 * @return temperature in unit deg.Celsius
 */
static int bd7181x_charge_status(struct bd7181x_power *pwr)
{
	// called/updated  every 3 secs (JITTER_DEFAULT)
	u8 state;
	int ret = 1;

	state = bd7181x_reg_read(pwr->mfd, BD7181X_REG_CHG_STATE);
	// bd7181x_info(pwr->dev, "CHG_STATE %d\n", state);

	pwr->charge_type = state;
	switch (state) {
	case CHG_STATE_SUSPEND:
		ret = 0;
		pwr->rpt_status = POWER_SUPPLY_STATUS_DISCHARGING;
		pwr->bat_health = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case CHG_STATE_TRICKLE_CHARGE: // TRICKLE
	case CHG_STATE_PRE_CHARGE: // PRE-CHARGING
	case CHG_STATE_FAST_CHARGE: // FAST-CHARGING
	case CHG_STATE_TOP_OFF: // TERMINATION CURRENT REACHED
		pwr->rpt_status = POWER_SUPPLY_STATUS_CHARGING;
		pwr->bat_health = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case CHG_STATE_DONE:
		ret = 0;
		pwr->rpt_status = POWER_SUPPLY_STATUS_FULL;
		pwr->bat_health = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case CHG_STATE_TEMP_ERR1:
	case CHG_STATE_TEMP_ERR2:
	case CHG_STATE_TEMP_ERR3:
	case CHG_STATE_TEMP_ERR4:
	case CHG_STATE_TEMP_ERR5:
	case CHG_STATE_TSD1:
	case CHG_STATE_TSD2:
	case CHG_STATE_TSD3:
	case CHG_STATE_TSD4:
	case CHG_STATE_TSD5:
		ret = 0;
		pwr->rpt_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		pwr->bat_health = POWER_SUPPLY_HEALTH_OVERHEAT;
		break;
	case CHG_STATE_BATT_ASSIST1: // VSYS < VBAT
	case CHG_STATE_BATT_ASSIST2:
	case CHG_STATE_BATT_ASSIST3:
		ret = 0;
		pwr->rpt_status = POWER_SUPPLY_STATUS_DISCHARGING;
		pwr->bat_health = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case CHG_STATE_BATT_ERROR: // BATTERY ERROR
	default:
		ret = 0;
		pwr->rpt_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		pwr->bat_health = POWER_SUPPLY_HEALTH_DEAD;
		break;	
	}

	bd7181x_reset_coulomb_count_at_full_charge(pwr);

	pwr->prev_rpt_status = pwr->rpt_status;

	return ret;
}

#if INIT_COULOMB == BY_BAT_VOLT
static int bd7181x_calib_voltage(struct bd7181x_power* pwr, int* ocv) {
	int r, curr, volt;

	bd7181x_get_vbat_curr(pwr, &volt, &curr);

	r = bd7181x_reg_read(pwr->mfd, BD7181X_REG_CHG_STATE);
	if (r >= 0 && curr > 0) {
		// voltage increment caused by battery inner resistor
		if (r == 3) volt -= 100 * 1000;
		else if (r == 2) volt -= 50 * 1000;
	}
	*ocv = volt;

	return 0;
}
#endif

/** @brief set initial coulomb counter value from battery voltage
 * @param pwr power device
 * @return 0
 */
static int init_coulomb_counter(struct bd7181x_power* pwr) {
	u32 bcap;
	int soc, ocv;

#if INIT_COULOMB == BY_VBATLOAD_REG
	/* Get init OCV by HW */
	bd7181x_get_init_bat_stat(pwr);

	ocv = (pwr->hw_ocv1 >= pwr->hw_ocv2)? pwr->hw_ocv1: pwr->hw_ocv2;
	bd7181x_info(pwr->dev, "ocv %d\n", ocv);
#elif INIT_COULOMB == BY_BAT_VOLT
	bd7181x_calib_voltage(pwr, &ocv);
#endif

	/* Get init soc from ocv/soc table */
	soc = bd7181x_voltage_to_capacity(ocv);
	bd7181x_info(pwr->dev, "soc %d[0.1%%]\n", soc);
	if (soc < 0)
		soc = 0;
	bcap = pwr->designed_cap * soc / 1000;

	bd7181x_reg_write16(pwr->mfd, BD7181X_REG_CC_CCNTD_1, 0);
	bd7181x_reg_write16(pwr->mfd, BD7181X_REG_CC_CCNTD_3, ((bcap + pwr->designed_cap / 200) & 0x1FFFUL));

	pwr->coulomb_cnt = bd7181x_reg_read32(pwr->mfd, BD7181X_REG_CC_CCNTD_3) & 0x1FFFFFFFUL;
	bd7181x_info(pwr->dev, "%s() CC_CCNTD = %d\n", __func__, pwr->coulomb_cnt);

	/* Start canceling offset of the DS ADC. This needs 1 second at least */
	bd7181x_set_bits(pwr->mfd, BD7181X_REG_CC_CTRL, CCCALIB); // 0x71h -> 0x20 FORCE CALIBRATION....

	return 0;
}

/** @brief adjust coulomb counter values at relaxed state
 * @param pwr power device
 * @return 0
 */
static int bd7181x_adjust_coulomb_count(struct bd7181x_power* pwr) {
	u32 relaxed_coulomb_cnt;

	relaxed_coulomb_cnt = bd7181x_reg_read32(pwr->mfd, BD7181X_REG_REX_CCNTD_3) & 0x1FFFFFFFUL;
	if (relaxed_coulomb_cnt != 0) {
		u32 bcap;
		int soc, ocv;
		int diff_coulomb_cnt;

		/* Get OCV at relaxed state by HW */
		ocv = bd7181x_reg_read16(pwr->mfd, BD7181X_REG_REX_SA_VBAT_U) * 1000;
		bd7181x_info(pwr->dev, "ocv %d\n", ocv);

		/* Clear Relaxed Coulomb Counter */
		bd7181x_set_bits(pwr->mfd, BD7181X_REG_REX_CTRL_1, REX_CLR);

		diff_coulomb_cnt = relaxed_coulomb_cnt - (bd7181x_reg_read32(pwr->mfd, BD7181X_REG_CC_CCNTD_3) & 0x1FFFFFFFUL);
		diff_coulomb_cnt = diff_coulomb_cnt >> 16;
		bd7181x_info(pwr->dev, "diff_coulomb_cnt = %d\n", diff_coulomb_cnt);

		/* Get soc at relaxed state from ocv/soc table */
		soc = bd7181x_voltage_to_capacity(ocv);
		bd7181x_info(pwr->dev, "soc %d[0.1%%]\n", soc);
		if (soc < 0)
			soc = 0;

		if ((soc > CANCEL_ADJ_COULOMB_SOC_H_1) || ((soc < CANCEL_ADJ_COULOMB_SOC_L_1) && (soc > CANCEL_ADJ_COULOMB_SOC_H_2)) || (soc < CANCEL_ADJ_COULOMB_SOC_L_2) || 
			((pwr->temp <= FORCE_ADJ_COULOMB_TEMP_H) && (pwr->temp >= FORCE_ADJ_COULOMB_TEMP_L))) {
			bcap = pwr->designed_cap * soc / 1000;

			/* Stop Coulomb Counter */
			bd7181x_clear_bits(pwr->mfd, BD7181X_REG_CC_CTRL, CCNTENB);

			bd7181x_reg_write16(pwr->mfd, BD7181X_REG_CC_CCNTD_1, 0);
			bd7181x_reg_write16(pwr->mfd, BD7181X_REG_CC_CCNTD_3, ((bcap + pwr->designed_cap / 200) & 0x1FFFUL) + diff_coulomb_cnt);

			pwr->coulomb_cnt = bd7181x_reg_read32(pwr->mfd, BD7181X_REG_CC_CCNTD_3) & 0x1FFFFFFFUL;
			bd7181x_info(pwr->dev, "Adjust Coulomb Counter at Relaxed State\n");
			bd7181x_info(pwr->dev, "CC_CCNTD = %d\n", pwr->coulomb_cnt);

			/* Start Coulomb Counter */
			bd7181x_set_bits(pwr->mfd, BD7181X_REG_CC_CTRL, CCNTENB);

			/* If the following commented out code is enabled, the SOC is not clamped at the relax time. */
			/* Reset SOCs */
			/* bd7181x_calc_soc_org(pwr); */
			/* pwr->soc_norm = pwr->soc_org; */
			/* pwr->soc = pwr->soc_norm; */
			/* pwr->clamp_soc = pwr->soc; */
		}
	}

	return 0;
}

/** @brief reset coulomb counter values at full charged state
 * @param pwr power device
 * @return 0
 */
static int bd7181x_reset_coulomb_count_at_full_charge(struct bd7181x_power* pwr)
{
	u32 full_charged_coulomb_cnt;

	full_charged_coulomb_cnt = bd7181x_reg_read32(pwr->mfd, BD7181X_REG_FULL_CCNTD_3) & 0x1FFFFFFFUL;
	if (full_charged_coulomb_cnt != 0) {
		int diff_coulomb_cnt;

		/* Clear Full Charged Coulomb Counter */
		bd7181x_set_bits(pwr->mfd, BD7181X_REG_FULL_CTRL, FULL_CLR);

		diff_coulomb_cnt = full_charged_coulomb_cnt - (bd7181x_reg_read32(pwr->mfd, BD7181X_REG_CC_CCNTD_3) & 0x1FFFFFFFUL);
		diff_coulomb_cnt = diff_coulomb_cnt >> 16;
		if (diff_coulomb_cnt > 0) {
			diff_coulomb_cnt = 0;
		}
		bd7181x_info(pwr->dev, "diff_coulomb_cnt = %d\n", diff_coulomb_cnt);

		/* Stop Coulomb Counter */
		bd7181x_clear_bits(pwr->mfd, BD7181X_REG_CC_CTRL, CCNTENB);

		bd7181x_reg_write16(pwr->mfd, BD7181X_REG_CC_CCNTD_1, 0);
		bd7181x_reg_write16(pwr->mfd, BD7181X_REG_CC_CCNTD_3, ((pwr->designed_cap + pwr->designed_cap / 200) & 0x1FFFUL) + diff_coulomb_cnt);

		pwr->coulomb_cnt = bd7181x_reg_read32(pwr->mfd, BD7181X_REG_CC_CCNTD_3) & 0x1FFFFFFFUL;
		bd7181x_info(pwr->dev, "Reset Coulomb Counter at POWER_SUPPLY_STATUS_FULL\n");
		bd7181x_info(pwr->dev, "CC_CCNTD = %d\n", pwr->coulomb_cnt);

		/* Start Coulomb Counter */
		bd7181x_set_bits(pwr->mfd, BD7181X_REG_CC_CTRL, CCNTENB);
	}

	return 0;
}

/** @brief get battery parameters, such as voltages, currents, temperatures.
 * @param pwr power device
 * @return 0
 */
static int bd7181x_get_battery_parameters(struct bd7181x_power* pwr)
{
	/* Read detailed vcell and current */
	bd7181x_get_vbat_curr(pwr, &pwr->vcell, &pwr->curr_sar);
	bd7181x_info(pwr->dev, "VM_VBAT = %d\n", pwr->vcell); // battery voltage
	bd7181x_info(pwr->dev, "VM_IBAT = %d\n", pwr->curr_sar); // current now

	pwr->curr = bd7181x_get_current_ds_adc(pwr);
	bd7181x_info(pwr->dev, "CC_CURCD = %d\n", pwr->curr); // battery current value from DS_ADC

	/* Read detailed vsys */
	bd7181x_get_vsys(pwr, &pwr->vsys);
	bd7181x_info(pwr->dev, "VM_VSYS = %d\n", pwr->vsys);

	/* Read detailed vbat_min */
	bd7181x_get_vbat_min(pwr, &pwr->vcell_min);
	bd7181x_info(pwr->dev, "VM_VBAT_MIN = %d\n", pwr->vcell_min);

	/* Read detailed vsys_min */
	bd7181x_get_vsys_min(pwr, &pwr->vsys_min);
	bd7181x_info(pwr->dev, "VM_VSYS_MIN = %d\n", pwr->vsys_min);

	/* Get tempature */
	pwr->temp = bd7181x_get_temp(pwr);
	// bd7181x_info(pwr->dev, "Temperature %d degrees C\n", pwr->temp);

	return 0;
}

/** @brief adjust coulomb counter values at relaxed state by SW
 * @param pwr power device
 * @return 0
 */
static int bd7181x_adjust_coulomb_count_sw(struct bd7181x_power* pwr)
{
	int tmp_curr_mA;

	tmp_curr_mA = pwr->curr / 1000;
	if ((tmp_curr_mA * tmp_curr_mA) <= (THR_RELAX_CURRENT * THR_RELAX_CURRENT)) { /* No load */
		pwr->relax_time += (JITTER_DEFAULT / 1000);
	}
	else {
		pwr->relax_time = 0;
	}
	
	if (pwr->relax_time >= THR_RELAX_TIME) { /* Battery is relaxed. */
		u32 bcap;
		int soc, ocv;

		pwr->relax_time = 0;

		/* Get OCV */
		ocv = pwr->vcell;

		/* Get soc at relaxed state from ocv/soc table */
		soc = bd7181x_voltage_to_capacity(ocv);
		bd7181x_info(pwr->dev, "soc %d[0.1%%]\n", soc);
		if (soc < 0)
			soc = 0;

		if ((soc > CANCEL_ADJ_COULOMB_SOC_H_1) || ((soc < CANCEL_ADJ_COULOMB_SOC_L_1) && (soc > CANCEL_ADJ_COULOMB_SOC_H_2)) || (soc < CANCEL_ADJ_COULOMB_SOC_L_2) || 
			((pwr->temp <= FORCE_ADJ_COULOMB_TEMP_H) && (pwr->temp >= FORCE_ADJ_COULOMB_TEMP_L))) {
			bcap = pwr->designed_cap * soc / 1000;

			/* Stop Coulomb Counter */
			bd7181x_clear_bits(pwr->mfd, BD7181X_REG_CC_CTRL, CCNTENB);

			bd7181x_reg_write16(pwr->mfd, BD7181X_REG_CC_CCNTD_1, 0);
			bd7181x_reg_write16(pwr->mfd, BD7181X_REG_CC_CCNTD_3, ((bcap + pwr->designed_cap / 200) & 0x1FFFUL));

			pwr->coulomb_cnt = bd7181x_reg_read32(pwr->mfd, BD7181X_REG_CC_CCNTD_3) & 0x1FFFFFFFUL;
			bd7181x_info(pwr->dev, "Adjust Coulomb Counter by SW at Relaxed State\n");
			bd7181x_info(pwr->dev, "CC_CCNTD = %d\n", pwr->coulomb_cnt);

			/* Start Coulomb Counter */
			bd7181x_set_bits(pwr->mfd, BD7181X_REG_CC_CTRL, CCNTENB);

			/* If the following commented out code is enabled, the SOC is not clamped at the relax time. */
			/* Reset SOCs */
			/* bd7181x_calc_soc_org(pwr); */
			/* pwr->soc_norm = pwr->soc_org; */
			/* pwr->soc = pwr->soc_norm; */
			/* pwr->clamp_soc = pwr->soc; */
		}

	}

	return 0;
}

/** @brief get coulomb counter values
 * @param pwr power device
 * @return 0
 */
static int bd7181x_coulomb_count(struct bd7181x_power* pwr) {
	if (pwr->state_machine == STAT_POWER_ON) {
		pwr->state_machine = STAT_INITIALIZED;
		/* Start Coulomb Counter */
		bd7181x_set_bits(pwr->mfd, BD7181X_REG_CC_CTRL, CCNTENB);
	} else if (pwr->state_machine == STAT_INITIALIZED) {
		pwr->coulomb_cnt = bd7181x_reg_read32(pwr->mfd, BD7181X_REG_CC_CCNTD_3) & 0x1FFFFFFFUL;
		// bd7181x_info(pwr->dev, "CC_CCNTD = %d\n", pwr->coulomb_cnt);
	}
	return 0;
}

/** @brief calc cycle
 * @param pwr power device
 * @return 0
 */
static int bd7181x_update_cycle(struct bd7181x_power* pwr) { // TODO: review cycle concept, it should be a full cycle no small
	int charged_coulomb_cnt;

	charged_coulomb_cnt = bd7181x_reg_read16(pwr->mfd, BD7181X_REG_CCNTD_CHG_3);
	if (charged_coulomb_cnt >= pwr->designed_cap) {
		pwr->cycle++;
		bd7181x_info(pwr->dev, "Update cycle = %d\n", pwr->cycle);
		battery_cycle = pwr->cycle;
		charged_coulomb_cnt -= pwr->designed_cap;
		/* Stop Coulomb Counter */
		bd7181x_clear_bits(pwr->mfd, BD7181X_REG_CC_CTRL, CCNTENB);

		bd7181x_reg_write16(pwr->mfd, BD7181X_REG_CCNTD_CHG_3, charged_coulomb_cnt);

		/* Start Coulomb Counter */
		bd7181x_set_bits(pwr->mfd, BD7181X_REG_CC_CTRL, CCNTENB);
	}
	return 0;
}

/** @brief calc full capacity value by Cycle and Temperature
 * @param pwr power device
 * @return 0
 */
static int bd7181x_calc_full_cap(struct bd7181x_power* pwr) {
	u32 designed_cap_uAh;
	u32 full_cap_uAh;

	/* Calculate full capacity by cycle */
	designed_cap_uAh = A10s_mAh(pwr->designed_cap) * 1000;
	bd7181x_info(pwr->dev, "Cycle = %d\n", pwr->cycle);
	full_cap_uAh = designed_cap_uAh - BD7181X_DGRD_CYC_CAP * pwr->cycle;
	pwr->full_cap = mAh_A10s(full_cap_uAh / 1000);
	bd7181x_info(pwr->dev, "Calculate full capacity by cycle\n");
	bd7181x_info(pwr->dev, "%s() pwr->full_cap = %d\n", __func__, pwr->full_cap);

	/* Calculate full capacity by temperature */
	bd7181x_info(pwr->dev, "Temperature = %d\n", pwr->temp);
	if (pwr->temp >= BD7181X_DGRD_TEMP_M) {
		full_cap_uAh += (pwr->temp - BD7181X_DGRD_TEMP_M) * BD7181X_DGRD_TEMP_CAP_H;
		pwr->full_cap = mAh_A10s(full_cap_uAh / 1000);
	}
	else if (pwr->temp >= BD7181X_DGRD_TEMP_L) {
		full_cap_uAh += (pwr->temp - BD7181X_DGRD_TEMP_M) * BD7181X_DGRD_TEMP_CAP_M;
		pwr->full_cap = mAh_A10s(full_cap_uAh / 1000);
	}
	else {
		full_cap_uAh += (BD7181X_DGRD_TEMP_L - BD7181X_DGRD_TEMP_M) * BD7181X_DGRD_TEMP_CAP_M;
		full_cap_uAh += (pwr->temp - BD7181X_DGRD_TEMP_L) * BD7181X_DGRD_TEMP_CAP_L;
		pwr->full_cap = mAh_A10s(full_cap_uAh / 1000);
	}
	bd7181x_info(pwr->dev, "Calculate full capacity by cycle and temperature\n");
	bd7181x_info(pwr->dev, "%s() pwr->full_cap = %d\n", __func__, pwr->full_cap);

	return 0;
}

/** @brief calculate SOC values by designed capacity
 * @param pwr power device
 * @return 0
 */
static int bd7181x_calc_soc_org(struct bd7181x_power* pwr) {
	pwr->soc_org = (pwr->coulomb_cnt >> 16) * 100 /  pwr->designed_cap;
	if (pwr->soc_org > 100) {
		pwr->soc_org = 100;
		/* Stop Coulomb Counter */
		bd7181x_clear_bits(pwr->mfd, BD7181X_REG_CC_CTRL, CCNTENB);

		bd7181x_reg_write16(pwr->mfd, BD7181X_REG_CC_CCNTD_1, 0);
		bd7181x_reg_write16(pwr->mfd, BD7181X_REG_CC_CCNTD_3, ((pwr->designed_cap + pwr->designed_cap / 200) & 0x1FFFUL));

		pwr->coulomb_cnt = bd7181x_reg_read32(pwr->mfd, BD7181X_REG_CC_CCNTD_3) & 0x1FFFFFFFUL;
		bd7181x_info(pwr->dev, "Limit Coulomb Counter\n");
		bd7181x_info(pwr->dev, "CC_CCNTD = %d\n", pwr->coulomb_cnt);

		/* Start Coulomb Counter */
		bd7181x_set_bits(pwr->mfd, BD7181X_REG_CC_CTRL, CCNTENB);
	}

	bd7181x_info(pwr->dev, "%s() pwr->soc_org = %d\n", __func__, pwr->soc_org);

	return 0;
}

/** @brief calculate SOC values by full capacity
 * @param pwr power device
 * @return 0
 */
static int bd7181x_calc_soc_norm(struct bd7181x_power* pwr) {
	int lost_cap;
	int mod_coulomb_cnt;

	lost_cap = pwr->designed_cap - pwr->full_cap;
	bd7181x_info(pwr->dev, "%s() lost_cap = %d\n", __func__, lost_cap);
	mod_coulomb_cnt = (pwr->coulomb_cnt >> 16) - lost_cap;
	if ((mod_coulomb_cnt > 0) && (pwr->full_cap > 0)) {
		pwr->soc_norm = mod_coulomb_cnt * 100 /  pwr->full_cap;
	}
	else {
		pwr->soc_norm = 0;
	}
	if (pwr->soc_norm > 100) {
		pwr->soc_norm = 100;
	}

	bd7181x_info(pwr->dev, "%s() pwr->soc_norm = %d\n", __func__, pwr->soc_norm);

	return 0;
}

/** @brief get OCV value by SOC
 * @param pwr power device
 * @return 0
 */
int bd7181x_get_ocv(struct bd7181x_power* pwr, int dsoc) {
	int i = 0;
	int ocv = 0;

	if (dsoc > soc_table[0]) {
		ocv = MAX_VOLTAGE;
	}
	else if (dsoc == 0) {
			ocv = ocv_table[21];
	}
	else {
		i = 0;
		while (i < 22) {
			if ((dsoc <= soc_table[i]) && (dsoc > soc_table[i+1])) {
				ocv = (ocv_table[i] - ocv_table[i+1]) * (dsoc - soc_table[i+1]) / (soc_table[i] - soc_table[i+1]) + ocv_table[i+1];
				break;
			}
			i++;
		}
		if (i == 22)
			ocv = ocv_table[22];
	}
	bd7181x_info(pwr->dev, "%s() ocv = %d\n", __func__, ocv);
	return ocv;
}

/** @brief calculate SOC value by full_capacity and load
 * @param pwr power device
 * @return OCV
 */
static int bd7181x_calc_soc(struct bd7181x_power* pwr) {
	int ocv_table_load[23]; // HARDCODED VALUE 23 - 23 samples in vector soc

	pwr->soc = pwr->soc_norm;

	switch (pwr->rpt_status) { /* Adjust for 0% between THR_VOLTAGE and MIN_VOLTAGE */
	case POWER_SUPPLY_STATUS_DISCHARGING:
	case POWER_SUPPLY_STATUS_NOT_CHARGING:
		if (pwr->vsys_min <= THR_VOLTAGE) { // WHY calculate SOC only whev latest minimun Vsys <= Vthr ????????????????? STILL I DONT GET IT 
			int i;
			int ocv;
			int lost_cap;
			int mod_coulomb_cnt;
			int dsoc;

			lost_cap = pwr->designed_cap - pwr->full_cap;
			mod_coulomb_cnt = (pwr->coulomb_cnt >> 16) - lost_cap;
			dsoc = mod_coulomb_cnt * 1000 /  pwr->full_cap;
			bd7181x_info(pwr->dev, "%s() dsoc = %d\n", __func__, dsoc);
			ocv = bd7181x_get_ocv(pwr, dsoc);
			for (i = 1; i < 23; i++) {
				ocv_table_load[i] = ocv_table[i] - (ocv - pwr->vsys_min);
				if (ocv_table_load[i] <= MIN_VOLTAGE) {
					bd7181x_info(pwr->dev, "%s() ocv_table_load[%d] = %d\n", __func__, i, ocv_table_load[i]);
					break;
				}
			}
			if (i < 23) {
				int j;
				int dv = (ocv_table_load[i-1] - ocv_table_load[i]) / 5;
				int lost_cap2;
				int mod_coulomb_cnt2, mod_full_cap;
				for (j = 1; j < 5; j++){
					if ((ocv_table_load[i] + dv * j) > MIN_VOLTAGE) {
						break;
					}
				}
				lost_cap2 = ((21 - i) * 5 + (j - 1)) * pwr->full_cap / 100;
				bd7181x_info(pwr->dev, "%s() lost_cap2 = %d\n", __func__, lost_cap2);
				mod_coulomb_cnt2 = mod_coulomb_cnt - lost_cap2;
				mod_full_cap = pwr->full_cap - lost_cap2;
				if ((mod_coulomb_cnt2 > 0) && (mod_full_cap > 0)) {
					pwr->soc = mod_coulomb_cnt2 * 100 / mod_full_cap;
				}
				else {
					pwr->soc = 0;
				}
				bd7181x_info(pwr->dev, "%s() pwr->soc(by load) = %d\n", __func__, pwr->soc);
			}
		}
		break;
	default:
		break;
	}

	switch (pwr->rpt_status) {/* Adjust for 0% and 100% */
	case POWER_SUPPLY_STATUS_DISCHARGING:
	case POWER_SUPPLY_STATUS_NOT_CHARGING:
		if (pwr->vsys_min <= MIN_VOLTAGE) {
			pwr->soc = 0;
		}
		else {
			if (pwr->soc == 0) {
				pwr->soc = 1;
			}
		}
		break;
	case POWER_SUPPLY_STATUS_CHARGING:
		if (pwr->soc == 100) {
			pwr->soc = 99;
		}
		break;
	default:
		break;
	}
	bd7181x_info(pwr->dev, "%s() pwr->soc = %d\n", __func__, pwr->soc);
	return 0;
}

/** @brief calculate Clamped SOC value by full_capacity and load
 * @param pwr power device
 * @return OCV
 */
static int bd7181x_calc_soc_clamp(struct bd7181x_power* pwr) {
	switch (pwr->rpt_status) {/* Adjust for 0% and 100% */
	case POWER_SUPPLY_STATUS_DISCHARGING:
	case POWER_SUPPLY_STATUS_NOT_CHARGING:
		if (pwr->soc <= pwr->clamp_soc) {
			pwr->clamp_soc = pwr->soc;
		}
		break;
	default:
		pwr->clamp_soc = pwr->soc;
		break;
	}
	bd7181x_info(pwr->dev, "%s() pwr->clamp_soc = %d\n", __func__, pwr->clamp_soc);
	return 0;
}

/** @brief get battery and DC online status
 * @param pwr power device
 * @return 0
 */
static int bd7181x_get_online(struct bd7181x_power* pwr) {
	int r;

#if 0
#define TS_THRESHOLD_VOLT	0xD9
	r = bd7181x_reg_read(pwr->mfd, BD7181X_REG_VM_VTH);
	pwr->bat_online = (r > TS_THRESHOLD_VOLT);
#endif
#if 0
	r = bd7181x_reg_read(pwr->mfd, BD7181X_REG_BAT_STAT);
	if (r >= 0 && (r & BAT_DET_DONE)) {
		pwr->bat_online = (r & BAT_DET) != 0;
	}
#endif
#if 1
#define BAT_OPEN	0x7
	r = bd7181x_reg_read(pwr->mfd, BD7181X_REG_BAT_TEMP);
	if (r >= 0) {
		pwr->bat_online = (r != BAT_OPEN);
		bd7181x_info(pwr->dev, "%s() pwr->bat_online = %d\n", __func__, pwr->bat_online);
	}
#endif	
	r = bd7181x_reg_read(pwr->mfd, BD7181X_REG_DCIN_STAT);
	if (r >= 0) {
		pwr->charger_online = (r & VBUS_DET) != 0;
		bd7181x_info(pwr->dev, "%s() pwr->charger_online = %d\n", __func__, pwr->charger_online);
	}

	return 0;
}

/*
static int bd7181x_charger_reset(struct bd7181x_power *pwr) {
	int r;
	struct bd7181x *mfd = pwr->mfd;
	r = bd7181x_reg_write(mfd, BD7181X_REG_SYS_INIT, 0x02);
	if (r != 0) {
		bd7181x_info(pwr->dev, "%s() could not reset charger\n", __fund__);
	}
	return 0;
}
*/

/** Init registers on every startup
 */
static void bd7181x_init_registers(struct bd7181x *mfd)
{
	// Fast Charging Voltage for the temperature ranges
	bd7181x_reg_write(mfd, BD7181X_REG_CHG_VBAT_1, VBAT_CHG1); // ROOM
	bd7181x_reg_write(mfd, BD7181X_REG_CHG_VBAT_2, VBAT_CHG2); // HOT1
	bd7181x_reg_write(mfd, BD7181X_REG_CHG_VBAT_3, VBAT_CHG3); // HOT2 & COLD1

	/* DCIN Anti-collapse entry voltage threshold 0.08V to 20.48V range, 80 mV steps.
	 * When DCINOK = L, Anti-collapse detection is invalid.
	 * When DCIN < DCIN_CLPS is detected, the charger decreases the input current restriction value.
	 * DCIN_CLPS voltage must be set higher than VBAT_CHG1, VBAT_CHG2, and VBAT_CHG3.
	 * If DCIN_CLPS set lower than these value, can't detect removing DCIN.
	 */
	bd7181x_reg_write(mfd, BD7181X_REG_DCIN_CLPS, DCIN_ANTICOLAPSE_VOLTAGE); // 4.24V

	// Configure Trickle and Pre-charging current
	bd7181x_reg_write(mfd, BD7181X_REG_CHG_IPRE, 0xAC); // Trickle: 25mA Pre-charge:300mA

	// Battery Charging Current for Fast Charge 100 mA to 2000 mA range, 100 mA steps.
	bd7181x_reg_write(mfd, BD7181X_REG_CHG_IFST, FAST_CHARGE_CURRENT); // 0x4C 1A (0x0A) with Ext MOSFET and Rsns=10mOhm

	// Charging Termination Current for Fast Charge 10 mA to 200 mA range.
	bd7181x_reg_write(mfd, BD7181X_REG_CHG_IFST_TERM, ITERM_CURRENT);

	// Battery over-voltage detection threshold. 4.25V
	// Battery voltage maintenance/recharge threshold : VBAT_CHG1/2/3 - 0.1V ****************** IMPORTANT ******************
	bd7181x_reg_write(mfd, BD7181X_REG_BAT_SET_2, OVP_MNT_THR);

	// Charging Termination Battery voltage threshold for Fast Charge.
	// VBAT_DONE = VBAT_CHG1/2/3 - 0.016V
	bd7181x_reg_write(mfd, BD7181X_REG_BAT_SET_3, 0x62);

	bd7181x_reg_write(mfd, BD7181X_REG_CHG_VPRE, 0x97); // precharge voltage thresholds VPRE_LO: 2.8V, VPRE_HI: 3.0V
}

/** @brief init bd7181x sub module charger
 * @param pwr power device
 * @return 0
 */
static int bd7181x_init_hardware(struct bd7181x_power *pwr)
{
	struct bd7181x *mfd = pwr->mfd;
	int r;

	bd7181x_init_registers(mfd);
	/* XSTB
	   Oscillator Stop Flag
	   0: RTC clock has been stopped.
	   1: RTC clock is normallyOscillator operating normally.
	   The XSTB bit is used to check the status of the Real Time Clock (RTC). This bit accepts R/W for "1" and "0".
	   If "1" is written to this bit, the XSTB bit will change value to "0" when the RTC is stopped.
	*/
#define XSTB		0x02
	r = bd7181x_reg_read(mfd, BD7181X_REG_CONF); // 0x37

#if 0
	for (i = 0; i < 300; i++) {
		r = bd7181x_reg_read(pwr->mfd, BD7181X_REG_BAT_STAT);
		if (r >= 0 && (r & BAT_DET_DONE)) {
			break;
		}
		msleep(5);
	}
#endif
	if ((r & XSTB) == 0x00) {
		printk(KERN_INFO "bd7181x-power: new battery inserted, initializing PMIC\n");

		//if (r & BAT_DET) {
			/* Init HW, when the battery is inserted. */

		bd7181x_reg_write(mfd, BD7181X_REG_CONF, r | XSTB); // enable RTC

		// VSYS_REG_Register VSYS regulation voltage setting. 4.2V to 5.25V range, 50mV step.
		bd7181x_reg_write(mfd, BD7181X_REG_VSYS_REG, 0x0B); // 4.75V (ask Joaquin)

		// VSYS voltage rising detection threshold. 0.0V to 8.128V range, 64mV steps
		bd7181x_reg_write(mfd, BD7181X_REG_VSYS_MAX, 0x33); // 3.264

		// VSYS voltage falling detection threshold. 0.0V to 8.128V range, 64mV steps.
		bd7181x_reg_write(mfd, BD7181X_REG_VSYS_MIN, 0x30); // 3.072

#define TEST_SEQ_00		0x00
#define TEST_SEQ_01		0x76
#define TEST_SEQ_02		0x66
#define TEST_SEQ_03		0x56

		/* Stop Coulomb Counter */
		bd7181x_clear_bits(mfd, BD7181X_REG_CC_CTRL, CCNTENB); // 0x71 -> 0x40 Disable (stop counting)

		/* Set Coulomb Counter Reset bit*/
		bd7181x_set_bits(mfd, BD7181X_REG_CC_CTRL, CCNTRST); // 0x71 -> 0x80 Reset CC_CCNTD_3-0

		/* Clear Coulomb Counter Reset bit*/
		bd7181x_clear_bits(mfd, BD7181X_REG_CC_CTRL, CCNTRST); // Release reset

		/* Set default Battery Capacity */
		pwr->designed_cap = BD7181X_BATTERY_CAP;
		pwr->full_cap = BD7181X_BATTERY_CAP;

		/* Set initial Coulomb Counter by HW OCV
		 * This estimation is and should only be performed once, when a new battery is connected.
		 * PRE and POST vbat registers are fixed to the same values until the battery is removed
		 * so they cannot be used to estimate the capacity on a later stage i.e. after charge/re-charge
		 * because the result would be based on values that do not reflect current battery state
		 *  */
		init_coulomb_counter(pwr);

		/* IMPORTANT: IN ORDER TO ENABLE EXT_MOSFET WE HAVE TO DISABLE THE CHARGER FIRST */
		bd7181x_reg_write(mfd, BD7181X_REG_CHG_SET1, WDT_AUTO_CHG_DISABLE);

		// 0xD8 -> 11011000
		// bit 7 VF_TREG_EN 1 thermal shutdown enabled
		// bit 6 EXTMOS_EN 1 Select External MOSFET. Change this register after CHG_EN is set to '0'
		// bit 5 REBATDET 0  Trigger for re-trial of Battery detection
		// bit 4 BATDET_EN 1 Enable Battery detection
		// bit 3 INHIBIT_1(note2) 1 For ROHM factory only
		// bit 1-0 Transition Timer Setting from the Suspend State to the Trickle state.
		bd7181x_reg_write(mfd, BD7181X_REG_CHG_SET2, 0xD8);

		/* VBAT Low voltage detection Setting */ // 0x58
		// Battery Voltage Alarm Threshold. Setting Range is from 0.000V to 8.176V, 16mV steps
		// Note : Alarms are reported as interrupts (INTB) INT_STAT_12 register but also have to be enabled
		bd7181x_reg_write16(mfd, BD7181X_REG_ALM_VBAT_TH_U, VBAT_LOW_TH); // 3.056V

		/* Mask Relax decision by PMU STATE */ // 0xE6
		// ?????????????????????????? value 0x04 Mask a condition according to Power State for Relax State detection.
		bd7181x_set_bits(pwr->mfd, BD7181X_REG_REX_CTRL_1, REX_PMU_STATE_MASK); // Enable Relax State detection // What is relax state and what is it used for

		/* Set Battery Capacity Monitor threshold1 as 90% */
		bd7181x_reg_write16(mfd, BD7181X_REG_CC_BATCAP1_TH_U, (BD7181X_BATTERY_CAP * 9 / 10));  // Interrupt CC_MON1_DET (INTB)
		bd7181x_info(pwr->dev, "BD7181X_REG_CC_BATCAP1_TH = %d\n", (BD7181X_BATTERY_CAP * 9 / 10));

		/* Enable LED ON when charging */ // 0x0E
		bd7181x_set_bits(pwr->mfd, BD7181X_REG_LED_CTRL, CHGDONE_LED_EN);

		// Battery over-current threshold. The value is set in 64 mA units (RSENS=10mohm).
		// Note: there are 3 thresholds available
		bd7181x_reg_write(pwr->mfd, BD7181X_REG_VM_OCUR_THR_1, 0xAB); // 1100mA

		// Battery over-temperature threshold. The value is set in 1-degree units, -55 to 200 degree range.
		bd7181x_reg_write(pwr->mfd, BD7181X_REG_VM_BTMP_OV_THR, 0x8C); // 95ºC ???

		// Battery low-temperature threshold. The value is set in 1-degree units, -55 to 200 degree range.
		bd7181x_reg_write(pwr->mfd, BD7181X_REG_VM_BTMP_LO_THR, 0x32); // -5ºC

		/* WDT_FST auto set */
		bd7181x_reg_write(mfd, BD7181X_REG_CHG_SET1, WDT_AUTO);

		pwr->state_machine = STAT_POWER_ON;
	}
	else {
		pwr->designed_cap = BD7181X_BATTERY_CAP;
		pwr->full_cap = BD7181X_BATTERY_CAP;	// bd7181x_reg_read16(pwr->mfd, BD7181X_REG_CC_BATCAP_U);
		pwr->state_machine = STAT_INITIALIZED;	// STAT_INITIALIZED
	}

	pwr->temp = bd7181x_get_temp(pwr);
	bd7181x_info(pwr->dev, "Temperature = %d\n", pwr->temp);
	bd7181x_adjust_coulomb_count(pwr);
	bd7181x_reset_coulomb_count_at_full_charge(pwr);
	pwr->coulomb_cnt = bd7181x_reg_read32(mfd, BD7181X_REG_CC_CCNTD_3) & 0x1FFFFFFFUL;
	bd7181x_calc_soc_org(pwr);
	pwr->soc_norm = pwr->soc_org;
	pwr->soc = pwr->soc_norm;
	pwr->clamp_soc = pwr->soc;

	bd7181x_info(pwr->dev, "%s() CC_CCNTD = %d\n", __func__, pwr->coulomb_cnt);
	bd7181x_info(pwr->dev, "%s() pwr->soc = %d\n", __func__, pwr->soc);
	bd7181x_info(pwr->dev, "%s() pwr->clamp_soc = %d\n", __func__, pwr->clamp_soc);

	pwr->cycle = battery_cycle;
	pwr->curr = 0;
	pwr->curr_sar = 0;
	pwr->relax_time = 0;

	return 0;
}

/**@brief timed work function called by system
 *  read battery capacity,
 *  sense change of charge status, etc.
 * @param work work struct
 * @return  void
 */

static void bd_work_callback(struct work_struct *work)
{
	struct bd7181x_power *pwr;
	struct delayed_work *delayed_work;
	int status, changed = 0;
	static int cap_counter = 0;

	delayed_work = container_of(work, struct delayed_work, work);
	pwr = container_of(delayed_work, struct bd7181x_power, bd_work);

	status = bd7181x_reg_read(pwr->mfd, BD7181X_REG_DCIN_STAT);
	if (status != pwr->vbus_status) {
		pwr->vbus_status = status;
		changed = 1;
	}

	status = bd7181x_reg_read(pwr->mfd, BD7181X_REG_BAT_STAT);
	status &= ~BAT_DET_DONE;
	if (status != pwr->bat_status) {
		pwr->bat_status = status;
		changed = 1;
	}

	status = bd7181x_reg_read(pwr->mfd, BD7181X_REG_CHG_STATE);
	if (status != pwr->charge_status) {
		pwr->charge_status = status;
		//changed = 1;
	}

	bd7181x_get_battery_parameters(pwr);
	bd7181x_adjust_coulomb_count(pwr);
	bd7181x_reset_coulomb_count_at_full_charge(pwr);
	bd7181x_adjust_coulomb_count_sw(pwr);
	bd7181x_coulomb_count(pwr);
	bd7181x_update_cycle(pwr);
	bd7181x_calc_full_cap(pwr);
	bd7181x_calc_soc_org(pwr);
	bd7181x_calc_soc_norm(pwr);
	bd7181x_calc_soc(pwr);
	bd7181x_calc_soc_clamp(pwr);
	bd7181x_get_online(pwr);
	bd7181x_charge_status(pwr);

	if (changed || cap_counter++ > JITTER_REPORT_CAP / JITTER_DEFAULT) {
		power_supply_changed(pwr->ac);
		power_supply_changed(pwr->bat);
		cap_counter = 0;
	}

	if (pwr->calib_current == CALIB_NORM) {
		schedule_delayed_work(&pwr->bd_work, msecs_to_jiffies(JITTER_DEFAULT));
	} else if (pwr->calib_current == CALIB_START) {
		pwr->calib_current = CALIB_GO;
	}
}

/**@brief bd7181x power interrupt
 * @param irq system irq
 * @param pwrsys bd7181x power device of system
 * @retval IRQ_HANDLED success
 * @retval IRQ_NONE error
 */
static irqreturn_t bd7181x_power_interrupt(int irq, void *pwrsys)
{
	int reg, r;

	struct device *dev = pwrsys;
	struct bd7181x *mfd = dev_get_drvdata(dev->parent);

	//bd7181x_info(pwr->dev, "%s , irq: %d\n", __func__, irq);
	
	reg = bd7181x_reg_read(mfd, BD7181X_REG_INT_STAT_03);
	if (reg < 0)
		return IRQ_NONE;

	r = bd7181x_reg_write(mfd, BD7181X_REG_INT_STAT_03, reg); // write the same value (1) to clear the interrupt
	if (r)
		return IRQ_NONE;

	if (reg & DCIN_MON_WDOGB) {
		printk("\n~~~ WDOGB detection\n");
	}
	if (reg & DCIN_MON_DET) {
		printk("\n~~~ DCIN General Alarm Detection : DCIN(61h+62h) ≦ DCIN_TH(59h)\n");
	}
	if (reg & DCIN_MON_RES) {
		printk("\n~~~ DCIN General Alarm Resume : DCIN(61h+62h) > DCIN_TH(59h)\n");
	}

	return IRQ_HANDLED;
}

/**@brief bd7181x vbat low voltage detection interrupt
 * @param irq system irq
 * @param pwrsys bd7181x power device of system
 * @retval IRQ_HANDLED success
 * @retval IRQ_NONE error
 * added by John Zhang at 2015-07-22
 */
static irqreturn_t bd7181x_vbat_interrupt(int irq, void *pwrsys)
{
	int reg, r;
	struct device *dev = pwrsys;
	struct bd7181x *mfd = dev_get_drvdata(dev->parent);

	bd7181x_info(pwr->dev, "%s , irq: %d\n", __func__, irq);
	
	reg = bd7181x_reg_read(mfd, BD7181X_REG_INT_STAT_08);
	if (reg < 0)
		return IRQ_NONE;

	r = bd7181x_reg_write(mfd, BD7181X_REG_INT_STAT_08, reg);
	if (r)
		return IRQ_NONE;

	if (reg & VBAT_MON_DET) {
		printk(KERN_DEBUG "\n~~~ VBAT General Alarm Detection : VBAT(5Dh+5Eh) ≦ VBAT_TH(57h+58h)... \n");
		send_signal(SIGTERM);
	}
	else if (reg & VBAT_MON_RES) {
		printk("\n~~~ VBAT General Alarm Resume : VBAT(5Dh+5Eh) > VBAT_TH(57h+58h) ... \n");
	}

	return IRQ_HANDLED;
}

static irqreturn_t bd7181x_int_dcin_ov_interrupt(int irq, void *pwrsys)
{
	int reg, r;
	struct device *dev = pwrsys;
	struct bd7181x *mfd = dev_get_drvdata(dev->parent);

	//bd7181x_info(pwr->dev, "%s , irq: %d\n", __func__, irq);

	reg = bd7181x_reg_read(mfd, BD7181X_REG_INT_STAT_02);
	if (reg < 0)
		return IRQ_NONE;

	r = bd7181x_reg_write(mfd, BD7181X_REG_INT_STAT_02, reg);
	if (r)
		return IRQ_NONE;

	if (reg & DCIN_OV_DET) {
		printk("\n~~~ DCIN >= 6.5V(typ) OVERVOLTAGE Detected ... \n");
	}
	else if (reg & DCIN_OV_RES) {
		printk("\n~~~ DCIN OVERVOLTAGE Resume DCIN <= 6.5-150mV(typ) ... \n");
	}
	else if (reg & DCIN_CLPS_IN) {
		printk("\n~~~ DCIN ANTI-COLAPSE detection DCIN(61h-62h) < DCIN_CLPS(43h) ... \n");
		if ((reg & DCIN_CLPS_OUT) == 0) {
			send_signal(SIGUSR2);
		}
	}
	else if (reg & DCIN_CLPS_OUT) {
		printk("\n~~~ DCIN COLAPSE RESUME  DCIN(61h-62h) >= DCIN_CLPS(43h)... \n");
	}
	else if (reg & DCIN_RMV) {
		printk("\n~~~ DCIN Removal ... \n");
		//
	}

	return IRQ_HANDLED;
}

/**@brief bd7181x int_stat_11 detection interrupt
 * @param irq system irq
 * @param pwrsys bd7181x power device of system
 * @retval IRQ_HANDLED success
 * @retval IRQ_NONE error
 * added 2015-12-26
 */
static irqreturn_t bd7181x_int_11_interrupt(int irq, void *pwrsys)
{
	int reg, r;
	struct device *dev = pwrsys;
	struct bd7181x *mfd = dev_get_drvdata(dev->parent);
	
	//bd7181x_info(pwr->dev, "%s , irq: %d\n", __func__, irq);
	
	reg = bd7181x_reg_read(mfd, BD7181X_REG_INT_STAT_11);
	if (reg < 0)
		return IRQ_NONE;

	r = bd7181x_reg_write(mfd, BD7181X_REG_INT_STAT_11, reg);
	if (r) {
		return IRQ_NONE;
	}

	if (reg & INT_STAT_11_VF_DET) {
		printk("\n~~~ Die temp.(VF) General Alarm Detection : VF(64h) ≦ VF_TH(53h) ... \n");
	}
	else if (reg & INT_STAT_11_VF_RES) {
		printk("\n~~~ Die temp.(VF) General Alarm Resume : VF(64h) > VF_TH(53h) ... \n");
	}
	else if (reg & INT_STAT_11_VF125_DET) {
		printk("\n~~~ Die temp(VF) Over 125 degC Detection : VF(64h) ≦ 125 degC(typ) ... \n");
	}
	else if (reg & INT_STAT_11_VF125_RES) {
		printk("\n~~~ Die temp(VF) Over 125 degC Resume : VF(64h) > 125 degC(typ ... \n");
	}
	else if (reg & INT_STAT_11_OVTMP_DET) {
		printk("\n~~~ Battery Over-Temperature Detection : BTMP(5Fh) < OVBTMPTHR(86h) with duration timer OVBTMPDUR(87h) ... \n");
	}
	else if (reg & INT_STAT_11_OVTMP_RES) {
		printk("\n~~~ Battery Over-Temperature Resume : BTMP(5Fh) ≧ OVBTMPTHR(86h) with duration timer OVBTMPDUR(87h) ... \n");
	}
	else if (reg & INT_STAT_11_LOTMP_DET) {
		printk("\n~~~ Battery Low-Temperature Detection : BTMP(5Fh) > LOBTMPTHR(88h) with duration timer LOBTMPDUR(89h) ... \n");
	}
	else if (reg & INT_STAT_11_LOTMP_RES) {
		printk("\n~~~ Battery Low-Temperature Resume : BTMP(5Fh) ≦ LOBTMPTHR(88h) with duration timer LOBTMPDUR(89h) ... \n");
	}

	return IRQ_HANDLED;
}

/** @brief get property of power supply ac
 *  @param psy power supply deivce
 *  @param psp property to get
 *  @param val property value to return
 *  @retval 0  success
 *  @retval negative fail
 */
static int bd7181x_charger_get_property(struct power_supply *psy,
					enum power_supply_property psp, union power_supply_propval *val)
{
	struct bd7181x_power *pwr = dev_get_drvdata(psy->dev.parent);
	u32 vot;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = pwr->charger_online;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		vot = bd7181x_reg_read16(pwr->mfd, BD7181X_REG_VM_DCIN_U);
		val->intval = 5000 * vot;		// 5 milli volt steps
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/** @brief get property of power supply bat
 *  @param psy power supply deivce
 *  @param psp property to get
 *  @param val property value to return
 *  @retval 0  success
 *  @retval negative fail
 */

static int bd7181x_battery_get_property(struct power_supply *psy,
					enum power_supply_property psp, union power_supply_propval *val)
{
	struct bd7181x_power *pwr = dev_get_drvdata(psy->dev.parent);
	// u32 cap, vot, r;
	// u8 ret;

	switch (psp) {
	/*
	case POWER_SUPPLY_PROP_STATUS:
		r = bd7181x_reg_read(pwr->mfd, BD7181X_REG_CHG_STATE);
		// printk("CHG_STATE = 0x%.2X\n", r);
		switch(r) {
		case CHG_STATE_SUSPEND:
			val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
			break;
		case CHG_STATE_TRICKLE_CHARGE:
		case CHG_STATE_PRE_CHARGE:
		case CHG_STATE_FAST_CHARGE:
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
			break;
		case CHG_STATE_TOP_OFF:
		case CHG_STATE_DONE:
			val->intval = POWER_SUPPLY_STATUS_FULL;
			break;
		default:
			val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
		}
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		ret = bd7181x_reg_read(pwr->mfd, BD7181X_REG_BAT_STAT);
		if (ret & DBAT_DET)
			val->intval = POWER_SUPPLY_HEALTH_DEAD;
		else if (ret & VBAT_OV)
			val->intval = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		else
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		cap = bd7181x_reg_read16(pwr->mfd, BD7181X_REG_CC_BATCAP_U);
		// printk("CC_BATCAP = 0x%.4X\n", cap);
		val->intval = cap * 100 / 0x1FFF;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		vot = bd7181x_reg_read16(pwr->mfd, BD7181X_REG_VM_VBAT_U) * 1000;
		val->intval = vot;
		break;
	*/
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = pwr->rpt_status;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = pwr->bat_health;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		if (pwr->rpt_status == POWER_SUPPLY_STATUS_CHARGING) {
			u8 colapse = bd7181x_reg_read(pwr->mfd, BD7181X_REG_IGNORE_0); // 0x41 50mV steps starting from 100mA
			if (colapse < 18) {
				val->intval = POWER_SUPPLY_CHARGE_TYPE_COLAPSED;
			}
			else {
				switch(pwr->charge_type) {
					case CHG_STATE_TRICKLE_CHARGE:
						val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
						break;
					case CHG_STATE_PRE_CHARGE:
						val->intval = POWER_SUPPLY_CHARGE_TYPE_PRECHARGE;
						break;
					case CHG_STATE_FAST_CHARGE:
						val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
						break;
					case CHG_STATE_TOP_OFF:
						val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
						break;
					default:
						val->intval = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
						break;
				}
				break;
			}
		}
		else {
			val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
		}
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = pwr->bat_online;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = pwr->vcell;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = pwr->clamp_soc;
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		{
		u32 t;

		t = pwr->coulomb_cnt >> 16;
		t = A10s_mAh(t);
		if (t > A10s_mAh(pwr->designed_cap)) t = A10s_mAh(pwr->designed_cap);
		val->intval = t * 1000;		/* uA to report */
		}
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = pwr->bat_online;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = BD7181X_BATTERY_FULL * A10s_mAh(pwr->designed_cap) * 10;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		val->intval = BD7181X_BATTERY_FULL * A10s_mAh(pwr->full_cap) * 10;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = pwr->curr_sar;
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		val->intval = pwr->curr;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = pwr->temp * 10; /* 0.1 degrees C unit */
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = MAX_VOLTAGE;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN:
		val->intval = MIN_VOLTAGE;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		{
			u8 colapse_cur = bd7181x_reg_read(pwr->mfd, BD7181X_REG_IGNORE_0);
			if (colapse_cur < 18)
				val->intval = 100000 + colapse_cur * 50000;
			else{
				switch(pwr->charge_type) {
					case CHG_STATE_TRICKLE_CHARGE:
						val->intval = TRICKLE_CURRENT;
						break;
					case CHG_STATE_PRE_CHARGE:
						val->intval = PRECHARGE_CURRENT;
						break;
					case CHG_STATE_FAST_CHARGE:
						val->intval = MAX_CURRENT;
						break;
					case CHG_STATE_TOP_OFF:
						val->intval = ITERM_CURRENT;
						break;
					default:
						val->intval = MAX_CURRENT;
						break;
				}
			}
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/** @brief ac properties */
static enum power_supply_property bd7181x_charger_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
};

/** @brief bat properies */
static enum power_supply_property bd7181x_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MIN,
};

static int first_offset(struct bd7181x_power *pwr)
{
	unsigned char ra2, ra3, ra6, ra7;
	unsigned char ra2_temp;
	struct bd7181x *mfd = pwr->mfd;

	bd7181x_reg_write(mfd, BD7181X_REG_TEST_MODE, TEST_SEQ_01);
	bd7181x_reg_write(mfd, BD7181X_REG_TEST_MODE, TEST_SEQ_02);
	bd7181x_reg_write(mfd, BD7181X_REG_TEST_MODE, TEST_SEQ_03);


	ra2 = bd7181x_reg_read(mfd, 0xA2);	// I want to know initial A2 & A3.
	ra3 = bd7181x_reg_read(mfd, 0xA3);	// I want to know initial A2 & A3.
	ra6 = bd7181x_reg_read(mfd, 0xA6);
	ra7 = bd7181x_reg_read(mfd, 0xA7);

	bd7181x_reg_write(mfd, 0xA2, 0x00);
	bd7181x_reg_write(mfd, 0xA3, 0x00);

	bd7181x_info(pwr->dev, "TEST[A2] = 0x%.2X\n", ra2);
	bd7181x_info(pwr->dev, "TEST[A3] = 0x%.2X\n", ra3);
	bd7181x_info(pwr->dev, "TEST[A6] = 0x%.2X\n", ra6);
	bd7181x_info(pwr->dev, "TEST[A7] = 0x%.2X\n", ra7);

	//-------------- First Step -------------------
	bd7181x_info(pwr->dev, "Frist Step begginning \n");

	// delay some time , Make a state of IBAT=0mA
	// mdelay(1000 * 10);

	ra2_temp = ra2;

	if (ra7 != 0) {
		//if 0<0xA7<20 decrease the Test register 0xA2[7:3] until 0xA7 becomes 0x00.
		if ((ra7 > 0) && (ra7 < 20)) {
			do {
				ra2 = bd7181x_reg_read(mfd, 0xA2);
				ra2_temp = ra2 >> 3;
				ra2_temp -= 1;
				ra2_temp <<= 3;
				bd7181x_reg_write(mfd, 0xA2, ra2_temp);
				bd7181x_info(pwr->dev, "TEST[A2] = 0x%.2X\n", ra2_temp);

				ra7 = bd7181x_reg_read(mfd, 0xA7);
				bd7181x_info(pwr->dev, "TEST[A7] = 0x%.2X\n", ra7);
				mdelay(1000);	// 1sec?
			} while (ra7);

			bd7181x_info(pwr->dev, "A7 becomes 0 . \n");

		}		// end if((ra7 > 0)&&(ra7 < 20)) 
		else if ((ra7 > 0xDF) && (ra7 < 0xFF))
			//if DF<0xA7<FF increase the Test register 0xA2[7:3] until 0xA7 becomes 0x00.
		{
			do {
				ra2 = bd7181x_reg_read(mfd, 0xA2);
				ra2_temp = ra2 >> 3;
				ra2_temp += 1;
				ra2_temp <<= 3;

				bd7181x_reg_write(mfd, 0xA2, ra2_temp);
				bd7181x_info(pwr->dev, "TEST[A2] = 0x%.2X\n", ra2_temp);

				ra7 = bd7181x_reg_read(mfd, 0xA7);
				bd7181x_info(pwr->dev, "TEST[A7] = 0x%.2X\n", ra7);
				mdelay(1000);	// 1sec?                           
			} while (ra7);

			bd7181x_info(pwr->dev, "A7 becomes 0 . \n");
		}
	}

	// please use "ra2_temp" at step2.
	return ra2_temp;
}

static int second_step(struct bd7181x_power *pwr, u8 ra2_temp)
{
	u16 ra6, ra7;
	u8 aft_ra2, aft_ra3;
	u8 r79, r7a;
	unsigned int LNRDSA_FUSE;
	long ADC_SIGN;
	long DSADGAIN1_INI;
	struct bd7181x *mfd = pwr->mfd;

	//-------------- Second Step -------------------
	bd7181x_info(pwr->dev, "Second Step begginning \n");

	// need to change boad setting ( input 1A tio 10mohm)
	// delay some time , Make a state of IBAT=1000mA
	// mdelay(1000 * 10);

// rough adjust
	bd7181x_info(pwr->dev, "ra2_temp = 0x%.2X\n", ra2_temp);

	ra6 = bd7181x_reg_read(mfd, 0xA6);
	ra7 = bd7181x_reg_read(mfd, 0xA7);
	ra6 <<= 8;
	ra6 |= ra7;		// [0xA6 0xA7]
	bd7181x_info(pwr->dev, "TEST[A6,A7] = 0x%.4X\n", ra6);

	bd7181x_reg_write(mfd, 0xA2, ra2_temp);	// this value from step1
	bd7181x_reg_write(mfd, 0xA3, 0x00);

	bd7181x_reg_write(mfd, BD7181X_REG_TEST_MODE, TEST_SEQ_00);

	r79 = bd7181x_reg_read(mfd, 0x79);
	r7a = bd7181x_reg_read(mfd, 0x7A);

	ADC_SIGN = r79 >> 7;
	ADC_SIGN = 1 - (2 * ADC_SIGN);
	DSADGAIN1_INI = r79 << 8;
	DSADGAIN1_INI = DSADGAIN1_INI + r7a;
	DSADGAIN1_INI = DSADGAIN1_INI & 0x7FFF;
	DSADGAIN1_INI = DSADGAIN1_INI * ADC_SIGN; //  unit 0.001

	// unit 0.000001
	DSADGAIN1_INI *= 1000;
	{
	if (DSADGAIN1_INI > 1000001) {
		DSADGAIN1_INI = 2048000000UL - (DSADGAIN1_INI - 1000000) * 8187;
	} else if (DSADGAIN1_INI < 999999) {
		DSADGAIN1_INI = -(DSADGAIN1_INI - 1000000) * 8187;
	} else {
		DSADGAIN1_INI = 0;
	}
	}

	LNRDSA_FUSE = (int) DSADGAIN1_INI / 1000000;

	bd7181x_info(pwr->dev, "LNRDSA_FUSE = 0x%.8X\n", LNRDSA_FUSE);

	aft_ra2 = (LNRDSA_FUSE >> 8) & 255;
	aft_ra3 = (LNRDSA_FUSE) & 255;

	aft_ra2 = aft_ra2 + ra2_temp;

	bd7181x_reg_write(mfd, BD7181X_REG_TEST_MODE, TEST_SEQ_01);
	bd7181x_reg_write(mfd, BD7181X_REG_TEST_MODE, TEST_SEQ_02);
	bd7181x_reg_write(mfd, BD7181X_REG_TEST_MODE, TEST_SEQ_03);

	bd7181x_reg_write(mfd, 0xA2, aft_ra2);
	bd7181x_reg_write(mfd, 0xA3, aft_ra3);

	return 0;
}

static int third_step(struct bd7181x_power *pwr, unsigned thr) {
	u16 ra2_a3, ra6, ra7;
	u8 ra2, ra3;
	u8 aft_ra2, aft_ra3;
	struct bd7181x *mfd = pwr->mfd;

// fine adjust
	ra2 = bd7181x_reg_read(mfd, 0xA2);	//
	ra3 = bd7181x_reg_read(mfd, 0xA3);	//

	ra6 = bd7181x_reg_read(mfd, 0xA6);
	ra7 = bd7181x_reg_read(mfd, 0xA7);
	ra6 <<= 8;
	ra6 |= ra7;		// [0xA6 0xA7]
	bd7181x_info(pwr->dev, "TEST[A6,A7] = 0x%.4X\n", ra6);


	if (ra6 > thr) {
		do {
			ra2_a3 = bd7181x_reg_read(mfd, 0xA2);
			ra2_a3 <<= 8;
			ra3 = bd7181x_reg_read(mfd, 0xA3);
			ra2_a3 |= ra3;
			//ra2_a3 >>= 3; // ? 0xA3[7:3] , or 0xA3[7:0]

			ra2_a3 -= 1;
			//ra2_a3 <<= 3;
			ra3 = ra2_a3;
			bd7181x_reg_write(mfd, 0xA3, ra3);

			ra2_a3 >>= 8;
			ra2 = ra2_a3;
			bd7181x_reg_write(mfd, 0xA2, ra2);

			bd7181x_info(pwr->dev, "TEST[A2] = 0x%.2X , TEST[A3] = 0x%.2X \n", ra2, ra3);

			mdelay(1000);	// 1sec?

			ra6 = bd7181x_reg_read(mfd, 0xA6);
			ra7 = bd7181x_reg_read(mfd, 0xA7);
			ra6 <<= 8;
			ra6 |= ra7;	// [0xA6 0xA7]
			bd7181x_info(pwr->dev, "TEST[A6,A7] = 0x%.4X\n", ra6);
		} while (ra6 > thr);
	} else if (ra6 < thr) {
		do {
			ra2_a3 = bd7181x_reg_read(mfd, 0xA2);
			ra2_a3 <<= 8;
			ra3 = bd7181x_reg_read(mfd, 0xA3);
			ra2_a3 |= ra3;
			//ra2_a3 >>= 3; // ? 0xA3[7:3] , or 0xA3[7:0]

			ra2_a3 += 1;
			//ra2_a3 <<= 3;
			ra3 = ra2_a3;
			bd7181x_reg_write(mfd, 0xA3, ra3);

			ra2_a3 >>= 8;
			ra2 = ra2_a3;
			bd7181x_reg_write(mfd, 0xA2, ra2);

			bd7181x_info(pwr->dev, "TEST[A2] = 0x%.2X , TEST[A3] = 0x%.2X \n", ra2, ra3);

			mdelay(1000);	// 1sec?

			ra6 = bd7181x_reg_read(mfd, 0xA6);
			ra7 = bd7181x_reg_read(mfd, 0xA7);
			ra6 <<= 8;
			ra6 |= ra7;	// [0xA6 0xA7]
			bd7181x_info(pwr->dev, "TEST[A6,A7] = 0x%.4X\n", ra6);

		} while (ra6 < thr);
	}

	bd7181x_info(pwr->dev, "[0xA6 0xA7] becomes [0x%.4X] . \n", thr);
	bd7181x_info(pwr->dev, " Calibation finished ... \n\n");

	aft_ra2 = bd7181x_reg_read(mfd, 0xA2);	// 
	aft_ra3 = bd7181x_reg_read(mfd, 0xA3);	// I want to know initial A2 & A3.

	bd7181x_info(pwr->dev, "TEST[A2,A3] = 0x%.2X%.2X\n", aft_ra2, aft_ra3);

	// bd7181x_reg_write(mfd, BD7181X_REG_TEST_MODE, TEST_SEQ_00);

	return 0;
}

static ssize_t bd7181x_sysfs_set_calibrate(struct device *dev,
					   	   	   	   	   	   struct device_attribute *attr,
										   const char *buf,
										   size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct bd7181x_power *pwr = container_of(psy, struct bd7181x_power, bat);
	ssize_t ret = 0;
	unsigned int val, mA;
	static u8 rA2;

	ret = sscanf(buf, "%d %d", &val, &mA);
	if (ret < 1) {
		bd7181x_info(pwr->dev, "error: write a integer string");
		return count;
	}

	if (val == 1) {
		pwr->calib_current = CALIB_START;
		while (pwr->calib_current != CALIB_GO) {
			msleep(500);
		}
		rA2 = first_offset(pwr);
	}
	if (val == 2) {
		second_step(pwr, rA2);
	}
	if (val == 3) {
		if (ret <= 1) {
			bd7181x_info(pwr->dev, "error: Fine adjust need a mA argument!");
		} else {
		unsigned int ra6_thr;

		ra6_thr = mA * 0xFFFF / 20000;
		bd7181x_info(pwr->dev, "Fine adjust at %d mA, ra6 threshold %d(0x%X)\n", mA, ra6_thr, ra6_thr);
		third_step(pwr, ra6_thr);
		}
	}
	if (val == 4) {
		bd7181x_reg_write(pwr->mfd, BD7181X_REG_TEST_MODE, TEST_SEQ_00);
		pwr->calib_current = CALIB_NORM;
		schedule_delayed_work(&pwr->bd_work, msecs_to_jiffies(0));
	}

	return count;
}

static ssize_t bd7181x_sysfs_show_calibrate(struct device *dev,
											struct device_attribute *attr,
											char *buf)
{
	// struct power_supply *psy = dev_get_drvdata(dev);
	// struct bd7181x_power *pwr = container_of(psy, struct bd7181x_power, bat);
	ssize_t ret = 0;

	ret = 0;
	ret += sprintf(buf + ret, "write string value\n"
		"\t1      0 mA for step one\n"
		"\t2      1000 mA for rough adjust\n"
		"\t3 <mA> for fine adjust\n"
		"\t4      exit current calibration\n");
	return ret;
}

static DEVICE_ATTR(calibrate, S_IWUSR | S_IRUGO,
		bd7181x_sysfs_show_calibrate, bd7181x_sysfs_set_calibrate);

static struct attribute *bd7181x_sysfs_attributes[] = {
	/*
	 * TODO: some (appropriate) of these attrs should be switched to
	 * use pwr supply class props.
	 */
	&dev_attr_calibrate.attr,
	NULL,
};

static const struct attribute_group bd7181x_sysfs_attr_group = {
	.attrs = bd7181x_sysfs_attributes,
};

/** @brief powers supplied by bd7181x_ac */
static char *bd7181x_ac_supplied_to[] = {
	BAT_NAME,
};

static const struct power_supply_desc bd7181x_bat_desc = {
	.name		= BAT_NAME,
	.type		= POWER_SUPPLY_TYPE_BATTERY,
	.properties	= bd7181x_battery_props,
	.num_properties	= ARRAY_SIZE(bd7181x_battery_props),
	.get_property	= bd7181x_battery_get_property,
};

static const struct power_supply_desc bd7181x_ac_desc = {
	.name		= AC_NAME,
	.type		= POWER_SUPPLY_TYPE_MAINS,
	.properties	= bd7181x_charger_props,
	.num_properties	= ARRAY_SIZE(bd7181x_charger_props),
	.get_property	= bd7181x_charger_get_property,
};

static irqreturn_t bd7181x_int_buck_interrupt(int irq, void *pwrsys)
{
	struct device *dev = pwrsys;
	struct bd7181x *mfd = dev_get_drvdata(dev->parent);
	int reg, r;

	reg = bd7181x_reg_read(mfd, BD7181X_REG_INT_STAT_01);
	if (reg < 0)
		return IRQ_NONE;

	r = bd7181x_reg_write(mfd, BD7181X_REG_INT_STAT_01, reg); // write the same value to clear the interrupt
	if (r)
		return IRQ_NONE;

	if (reg & LED_SCP) {
		printk("\n~~~ Enable LED SCP detection ... \n");
	}
	else if (reg & LED_OCP) {
		printk("\n~~~ Enable LED OCP detection ... \n");
	}
	else if (reg & LED_OVP) {
		printk("\n~~~ Enable LED OVP detection ... \n");
	}
	else if (reg & BUCK5FAULT) {
		printk("\n~~~ Enable BUCK5 output current limit detection interrupt ... \n");
	}
	else if (reg & BUCK4FAULT) {
		printk("\n~~~ Enable BUCK4 output current limit detection interrupt ... \n");
	}
	else if (reg & BUCK3FAULT) {
		printk("\n~~~ Enable BUCK3 output current limit detection interrupt ... \n");
	}
	else if (reg & BUCK2FAULT) {
		printk("\n~~~ Enable BUCK2 output current limit detection interrupt ... \n");
	}
	else if (reg & BUCK1FAULT) {
		printk("\n~~~ Enable BUCK1 output current limit detection interrupt ... \n");
	}

	return IRQ_HANDLED;
}

static irqreturn_t bd7181x_int_vsys_interrupt(int irq, void *pwrsys)
{
	struct device *dev = pwrsys;
	struct bd7181x *mfd = dev_get_drvdata(dev->parent);
	int reg, r;

	reg = bd7181x_reg_read(mfd, BD7181X_REG_INT_STAT_04);
	if (reg < 0)
		return IRQ_NONE;

	r = bd7181x_reg_write(mfd, BD7181X_REG_INT_STAT_04, reg); // write the same value to clear the interrupt
	if (r)
		return IRQ_NONE;

	if (reg & VSYS_MON_DET) {
		printk("\n~~~ VSYS General Alarm Detection : VSYS(63h) ≦ VSYS_TH(5Ah) ... \n");
	}
	else if (reg & VSYS_MON_RES) {
		printk("\n~~~ VSYS General Alarm Resume : VSYS(63h) > VSYS_TH(5Ah) ... \n");
	}
	else if (reg & VSYS_LO_DET) {
		printk("\n~~~ VSYS Low Voltage Detection : VSYS(63h) ≦ VSYS_MIN(46h) ... \n");
	}
	else if (reg & VSYS_LO_RES) {
		printk("\n~~~ VSYS Low Voltage Resume : VSYS(63h) ≧ VSYS_MAX(45h) ... \n");
	}
	else if (reg & VSYS_UV_DET) {
		printk("\n~~~ VSYS Under-Voltage Detection : VSYS ≦ 2.9V(typ) ... \n");
	}
	else if (reg & VSYS_UV_RES) {
		printk("\n~~~ VSYS Under-Voltage Resume : VSYS ≧ 3.2V(typ) ... \n");
	}

	return IRQ_HANDLED;
}

static irqreturn_t bd7181x_int_chg_interrupt(int irq, void *pwrsys)
{
	struct device *dev = pwrsys;
	struct bd7181x *mfd = dev_get_drvdata(dev->parent);
	int reg, r;

	reg = bd7181x_reg_read(mfd, BD7181X_REG_INT_STAT_05);
	if (reg < 0)
		return IRQ_NONE;

	r = bd7181x_reg_write(mfd, BD7181X_REG_INT_STAT_05, reg); // write the same value to clear the interrupt
	if (r)
		return IRQ_NONE;

	if (reg & CHG_TRNS) {
		printk("\n~~~ CHG_TRNS Battery Charger State Transition : CHG_STATE(39h) ... \n");
	}
	else if (reg & TMP_TRNS) {
		printk("\n~~~ TMP_TRNS Ranged Battery Temperature Transition : BAT_TEMP(40h) ... \n");
	}
	else if (reg & BAT_MNT_IN) {
		printk("\n~~~ BAT_MNT_IN Battery Maintenance(Re-Charging) Condition Detection : VBAT(5Dh+5Eh) ≦ VBAT_MNT(55h) ... \n");
	}
	else if (reg & BAT_MNT_OUT) {
		printk("\n~~~ BAT_MNT_OUT Battery Maintenance(Re-Charging) Condition Resume : VBAT(5Dh+5Eh) < VBAT_MNT(55h) ... \n");
	}
	else if (reg & CHG_WDT_EXP) {
		printk("\n~~~ CHG_WDT_EXP Charging Watch Dog Timer Expiration for abnormal long charging : CHG_WDT_PRE(49h), CHG_WDT_FST(4Ah) ... \n");
	}
	else if (reg & EXTEMP_TOUT) {
		printk("\n~~~ EXTEMP_TOUT Charging Watch Dog Timer Expiration for abnormal temperature protection .... \n");
	}
	else if (reg & INHIBIT_0) {
		printk("\n~~~ INHIBIT For ROHM factory only ... \n");
	}

	return IRQ_HANDLED;
}


static irqreturn_t bd7181x_int_bat_interrupt(int irq, void *pwrsys)
{
	struct device *dev = pwrsys;
	struct bd7181x *mfd = dev_get_drvdata(dev->parent);
	int reg, r;

	reg = bd7181x_reg_read(mfd, BD7181X_REG_INT_STAT_06);
	if (reg < 0)
		return IRQ_NONE;

	r = bd7181x_reg_write(mfd, BD7181X_REG_INT_STAT_06, reg); // write the same value to clear the interrupt
	if (r)
		return IRQ_NONE;

	if (reg & TH_DET) {
		printk("\n~~~ TH_DET External Thermistor Detection ... \n");
	}
	else if (reg & TH_RMV) {
		printk("\n~~~ TH_RMV External Thermister Removal ... \n");
	}
	else if (reg & BAT_DET) {
		printk("\n~~~ BAT_DET Battery Detection : BAT_SET(3Bh) [5]BAT_DET, [4]BAT_DET_DONE and CHG_SET2(48h) [4]BATDET_E... \n");
	}
	else if (reg & BAT_RMV) {
		printk("\n~~~ BAT_RMV Battery Removal : BAT_SET(3Bh) [5]BAT_DET, [4]BAT_DET_DONE and CHG_SET2(48h) [4]BATDET_E... \n");
	}
	else if (reg & TMP_OUT_DET) {
		printk("\n~~~ TMP_OUT_DET Out of Battery Charging Temperature Range Detection : BAT_TEMP(40h) is HOT3 or COLD2... \n");
	}
	else if (reg & TMP_OUT_RES) {
		printk("\n~~~ TMP_OUT_RES Out of Battery Charging Temperature Range Resume : BAT_TEMP(40h) is except HOT3 and COLD2... \n");
	}

	return IRQ_HANDLED;
}

static irqreturn_t bd7181x_int_vbat_mon1_interrupt(int irq, void *pwrsys)
{
	struct device *dev = pwrsys;
	struct bd7181x *mfd = dev_get_drvdata(dev->parent);
	int reg, r;

	reg = bd7181x_reg_read(mfd, BD7181X_REG_INT_STAT_07);
	if (reg < 0)
		return IRQ_NONE;

	r = bd7181x_reg_write(mfd, BD7181X_REG_INT_STAT_07, reg); // write the same value to clear the interrupt
	if (r)
		return IRQ_NONE;

	if (reg & VBAT_OV_DET) {
		printk("\n~~~ VBAT Over-Voltage Detection : VBAT(5Dh+5Eh) ≧ VBAT_OVP(55h) ... \n");
	}
	else if (reg & VBAT_OV_RES) {
		printk("\n~~~ VBAT Over-Voltage Resume : VBAT(5Dh+5Eh) ≦ VBAT_OVP(55h)-150mV ... \n");
	}
	else if (reg & VBAT_LO_DET) {
		printk("\n~~~ VBAT Low-Voltage Detection : VBAT(5Dh+5Eh) ≦ VBAT_LO(54h) ... \n");
	}
	else if (reg & VBAT_LO_RES) {
		printk("\n~~~ VBAT Low-Voltage Resume : VBAT(5Dh+5Eh) ≧ VBAT_HI(54h) ... \n");
	}
	else if (reg & VBAT_SHT_DET) {
		printk("\n~~~ VBAT Short-Circuit Detection : VBAT(5Dh+5Eh) ≦ 1.5V(typ) ... \n");
	}
	else if (reg & VBAT_SHT_RES) {
		printk("\n~~~ VBAT Short-Circuit Resume : VBAT(5Dh+5Eh) > 1.6V(typ) ... \n");
	}
	else if (reg & VBAT_DBAT_DET) {
		printk("\n~~~ VBAT Dead-Battery Detection : VBAT(5Dh+5Eh) ≦ VBAT_LO(54h) with duration timer TIM_DBP(56h) ... \n");
	}

	return IRQ_HANDLED;
}


static irqreturn_t bd7181x_int_vbat_mon3_interrupt(int irq, void *pwrsys)
{
	struct device *dev = pwrsys;
	struct bd7181x *mfd = dev_get_drvdata(dev->parent);
	int reg, r;

	reg = bd7181x_reg_read(mfd, BD7181X_REG_INT_STAT_09);
	if (reg < 0)
		return IRQ_NONE;

	r = bd7181x_reg_write(mfd, BD7181X_REG_INT_STAT_09, reg); // write the same value to clear the interrupt
	if (r)
		return IRQ_NONE;

	if (reg & CC_MON3_DET) {
		printk("\n~~~ Battery Capacity Alarm 3 : CCNTD(79h+7Ah+7Bh+7Ch) ≦ CC_BATCAP3_TH(76h+77h) (lower than equal) ... \n");
	}
	else if (reg & CC_MON2_DET) {
		printk("\n~~~ Battery Capacity Alarm 2 : CCNTD(79h+7Ah+7Bh+7Ch) ≦ CC_BATCAP2_TH(74h+75h) (lower than equal) ... \n");
	}
	else if (reg & CC_MON1_DET) {
		printk("\n~~~ Battery Capacity Alarm 1 : CCNTD(79h+7Ah+7Bh+7Ch) ≧ CC_BATCAP1_TH(72h+73h) (greater than equal) ... \n");
	}

	return IRQ_HANDLED;
}


static irqreturn_t bd7181x_int_vbat_mon4_interrupt(int irq, void *pwrsys)
{
	struct device *dev = pwrsys;
	struct bd7181x *mfd = dev_get_drvdata(dev->parent);
	int reg, r;

	reg = bd7181x_reg_read(mfd, BD7181X_REG_INT_STAT_10);
	if (reg < 0)
		return IRQ_NONE;

	r = bd7181x_reg_write(mfd, BD7181X_REG_INT_STAT_10, reg); // write the same value to clear the interrupt
	if (r)
		return IRQ_NONE;

	if (reg & OCUR3_DET) {
		printk("\n~~~ Battery Over-Current 3 Detection : CURCD(7Dh+7Eh) ≧ OCURTHR3(83h) with duration timer OCURDUR3(84h) ... \n");
	}
	else if (reg & OCUR3_RES) {
		printk("\n~~~ Battery Over-Current 3 Resume : CURCD(7Dh+7Eh) < OCURTHR3(83h) with duration timer OCURDUR3(84h) ... \n");
	}
	else if (reg & OCUR2_DET) {
		printk("\n~~~ Battery Over-Current 2 Detection : CURCD(7Dh+7Eh) ≧ OCURTHR2(81h) with duration timer OCURDUR2(82h) ... \n");
	}
	else if (reg & OCUR2_RES) {
		printk("\n~~~ Battery Over-Current 2 Resume : CURCD(7Dh+7Eh) < OCURTHR2(81h) with duration timer OCURDUR2(82h) ... \n");
	}
	else if (reg & OCUR1_DET) {
		printk("\n~~~ Battery Over-Current 1 Detection : CURCD(7Dh+7Eh) ≧ OCURTHR1(7Fh) with duration timer OCURDUR1(80h) ... \n");
	}
	else if (reg & OCUR1_RES) {
		printk("\n~~~ Battery Over-Current 1 Resume : CURCD(7Dh+7Eh) < OCURTHR1(7Fh) with duration timer OCURDUR1(80h) ... \n");
	}

	return IRQ_HANDLED;
}

static int bd7181x_add_irq(struct platform_device *pdev,
						   const char* irq_name,
						   irq_handler_t thread_fn)
{
	int irq;
	int ret;

	irq  = platform_get_irq_byname(pdev, irq_name);
	if (irq <= 0) {
		dev_warn(&pdev->dev, "platform irq error # %d\n", irq);
		return -ENXIO;
	}
	ret = devm_request_threaded_irq(&pdev->dev, irq, NULL,
			thread_fn, IRQF_TRIGGER_LOW | IRQF_EARLY_RESUME,
			dev_name(&pdev->dev), &pdev->dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "IRQ %d is not free.\n", irq);
	}

	return ret;
}

static int bd7181x_enable_irq(struct platform_device *pdev,
					  u8 reg,
					  const char* reg_name,
					  unsigned int val)
{
	int ret;
	int reg_val;

	struct bd7181x *bd7181x = dev_get_drvdata(pdev->dev.parent);
	/* Enable IRQ_BUCK_01 */
	ret = bd7181x_reg_write(bd7181x, reg, val);
	if (ret < 0) {
		dev_warn(&pdev->dev, "Write %s failed\n",reg_name);
	}
	reg_val = bd7181x_reg_read(bd7181x, reg);
	if (reg < 0) {
		dev_warn(&pdev->dev, "Read %s failed\n", reg_name);
	}
	dev_info(&pdev->dev, "%s=0x%x\n", reg_name, reg_val);

	return ret;
}

static int clear_all_interrupts(struct platform_device *pdev) {
	int r;
	struct bd7181x *mfd = dev_get_drvdata(pdev->dev.parent);
	r = bd7181x_reg_write(mfd, BD7181X_REG_INT_STAT_01, 0xFF);
	if (r < 0)
		return -ENXIO;
	r = bd7181x_reg_write(mfd, BD7181X_REG_INT_STAT_02, 0xFF);
	if (r < 0)
		return -ENXIO;
	r = bd7181x_reg_write(mfd, BD7181X_REG_INT_STAT_03, 0xFF);
	if (r < 0)
		return -ENXIO;
	r = bd7181x_reg_write(mfd, BD7181X_REG_INT_STAT_04, 0xFF);
	if (r < 0)
		return -ENXIO;
	r = bd7181x_reg_write(mfd, BD7181X_REG_INT_STAT_05, 0xFF);
	if (r < 0)
		return -ENXIO;
	r = bd7181x_reg_write(mfd, BD7181X_REG_INT_STAT_06, 0xFF);
	if (r < 0)
		return -ENXIO;
	r = bd7181x_reg_write(mfd, BD7181X_REG_INT_STAT_07, 0xFF);
	if (r < 0)
		return -ENXIO;
	r = bd7181x_reg_write(mfd, BD7181X_REG_INT_STAT_08, 0xFF);
	if (r < 0)
		return -ENXIO;
	r = bd7181x_reg_write(mfd, BD7181X_REG_INT_STAT_09, 0xFF);
	if (r < 0)
		return -ENXIO;
	r = bd7181x_reg_write(mfd, BD7181X_REG_INT_STAT_10, 0xFF);
	if (r < 0)
		return -ENXIO;
	r = bd7181x_reg_write(mfd, BD7181X_REG_INT_STAT_11, 0xFF);
	if (r < 0)
		return -ENXIO;
	r = bd7181x_reg_write(mfd, BD7181X_REG_INT_STAT_12, 0xFF);
	if (r < 0)
		return -ENXIO;

	return 0;
}

static int enable_interrupts(struct platform_device *pdev) {
	int ret;

	ret = bd7181x_add_irq(pdev, "IRQ_BUCK_01", bd7181x_int_buck_interrupt);
	if (ret == -ENXIO)
		return -ENXIO;

	ret = bd7181x_add_irq(pdev, "IRQ_DCIN_02", bd7181x_int_dcin_ov_interrupt);
	if (ret == -ENXIO)
		return -ENXIO;

	ret = bd7181x_add_irq(pdev, "IRQ_DCIN_03", bd7181x_power_interrupt);
	if (ret == -ENXIO)
		return -ENXIO;

	ret = bd7181x_add_irq(pdev, "IRQ_VSYS_04", bd7181x_int_vsys_interrupt);
	if (ret == -ENXIO)
		return -ENXIO;

	ret = bd7181x_add_irq(pdev, "IRQ_CHARGE_05", bd7181x_int_chg_interrupt);
	if (ret == -ENXIO)
		return -ENXIO;

	ret = bd7181x_add_irq(pdev, "IRQ_BAT_06", bd7181x_int_bat_interrupt);
	if (ret == -ENXIO)
		return -ENXIO;

	ret = bd7181x_add_irq(pdev, "IRQ_BAT_MON_07", bd7181x_int_vbat_mon1_interrupt);
	if (ret == -ENXIO)
		return -ENXIO;

	ret = bd7181x_add_irq(pdev, "IRQ_BAT_MON_08", bd7181x_vbat_interrupt);
	if (ret == -ENXIO)
		return -ENXIO;

	ret = bd7181x_add_irq(pdev, "IRQ_BAT_MON_09", bd7181x_int_vbat_mon3_interrupt);
	if (ret == -ENXIO)
		return -ENXIO;

	ret = bd7181x_add_irq(pdev, "IRQ_BAT_MON_10", bd7181x_int_vbat_mon4_interrupt);
	if (ret == -ENXIO)
		return -ENXIO;

	ret = bd7181x_add_irq(pdev, "IRQ_TEMPERATURE_11", bd7181x_int_11_interrupt);
	if (ret == -ENXIO)
		return -ENXIO;

	//bd7181x_enable_irq(pdev, BD7181X_REG_INT_EN_01, "BD7181X_REG_INT_EN_01", BD7181X_INT_EN_01_BUCKAST_MASK);
	bd7181x_enable_irq(pdev, BD7181X_REG_INT_EN_02, "BD7181X_REG_INT_EN_02", BD7181X_INT_EN_02_DCINAST_MASK);
	//bd7181x_enable_irq(pdev, BD7181X_REG_INT_EN_03, "BD7181X_REG_INT_EN_03", BD7181X_INT_EN_03_DCINAST_MASK);
	//bd7181x_enable_irq(pdev, BD7181X_REG_INT_EN_04, "BD7181X_REG_INT_EN_04", BD7181X_INT_EN_04_VSYSAST_MASK);
	//bd7181x_enable_irq(pdev, BD7181X_REG_INT_EN_05, "BD7181X_REG_INT_EN_05", BD7181X_INT_EN_05_CHGAST_MASK); // Disable CHG_TRNS
	//bd7181x_enable_irq(pdev, BD7181X_REG_INT_EN_06, "BD7181X_REG_INT_EN_06", BD7181X_INT_EN_06_BATAST_MASK);
	//bd7181x_enable_irq(pdev, BD7181X_REG_INT_EN_07, "BD7181X_REG_INT_EN_07", BD7181X_INT_EN_07_BMONAST_MASK);
	bd7181x_enable_irq(pdev, BD7181X_REG_INT_EN_08, "BD7181X_REG_INT_EN_08", BD7181X_INT_EN_08_BMONAST_MASK);
	//bd7181x_enable_irq(pdev, BD7181X_REG_INT_EN_09, "BD7181X_REG_INT_EN_09", BD7181X_INT_EN_09_BMONAST_MASK);
	//bd7181x_enable_irq(pdev, BD7181X_REG_INT_EN_10, "BD7181X_REG_INT_EN_10", BD7181X_INT_EN_10_BMONAST_MASK);
	//bd7181x_enable_irq(pdev, BD7181X_REG_INT_EN_11, "BD7181X_REG_INT_EN_11", BD7181X_INT_EN_11_TMPAST_MASK);

	return ret;
}

/** @brief probe pwr device 
 * @param pdev platform deivce of bd7181x_power
 * @retval 0 success
 * @retval negative fail
 */
static int __init bd7181x_power_probe(struct platform_device *pdev)
{
	struct bd7181x *bd7181x = dev_get_drvdata(pdev->dev.parent);
	struct bd7181x_power *pwr;
	int ret;
	struct power_supply_config charger_cfg = {};

	pwr = kzalloc(sizeof(*pwr), GFP_KERNEL);
	if (pwr == NULL)
		return -ENOMEM;

	pwr->dev = &pdev->dev;
	pwr->mfd = bd7181x;

	platform_set_drvdata(pdev, pwr);

	if (battery_cycle <= 0) {
		battery_cycle = 0;
	}
	dev_err(pwr->dev, "battery_cycle = %d\n", battery_cycle);

	/* If the product often power up/down and the power down time is long, the Coulomb Counter may have a drift. */
	/* If so, it may be better accuracy to enable Coulomb Counter using following commented out code */
	/* for counting Coulomb when the product is power up(including sleep). */
	/* The condition  */
	/* (1) Product often power up and down, the power down time is long and there is no power consumed in power down time. */
	/* (2) Kernel must call this routin at power up time. */
	/* (3) Kernel must call this routin at charging time. */
	/* (4) Must use this code with "Stop Coulomb Counter" code in bd7181x_power_remove() function */
	/* Start Coulomb Counter */
	/* bd7181x_set_bits(pwr->mfd, BD7181x_REG_CC_CTRL, CCNTENB); */

	bd7181x_init_hardware(pwr);

	//pwr->bat.desc = &bd7181x_desc;
	//ret = power_supply_register(&pdev->dev, &pwr->bat);
	pwr->bat = power_supply_register(&pdev->dev, &bd7181x_bat_desc, NULL);
	if (pwr->bat == NULL) {
		dev_err(&pdev->dev, "failed to register bat\n");
		goto fail_register_bat;
	}

	charger_cfg.supplied_to = bd7181x_ac_supplied_to;
	charger_cfg.num_supplicants = ARRAY_SIZE(bd7181x_ac_supplied_to);

	pwr->ac = power_supply_register(&pdev->dev, &bd7181x_ac_desc, &charger_cfg);
	if (pwr->ac == NULL) {
		dev_err(&pdev->dev, "failed to register ac\n");
		goto fail_register_ac;
	}

	ret = enable_interrupts(pdev);
	if (ret == -ENXIO)
		return -ENXIO;

	/* Interrupts are cleared on startup acording to documentation. If special
	 * handling is needed it should be done here. VBAT_LOW interrupt will be
	 *  retriggered so clearing it on start-up does not create any issue.
	*/

	ret = clear_all_interrupts(pdev);
	if (ret == -ENXIO)
		return -ENXIO;

	ret = sysfs_create_group(&pwr->bat->dev.kobj, &bd7181x_sysfs_attr_group);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to register sysfs interface\n");
	}

	pwr->reg_index = -1;

	INIT_DELAYED_WORK(&pwr->bd_work, bd_work_callback);

	/* Schedule timer to check current status */
	pwr->calib_current = CALIB_NORM;
	schedule_delayed_work(&pwr->bd_work, msecs_to_jiffies(0));

	// Userspace interface to register pid for signal
	bd7181x_dir = debugfs_create_dir("bd7181x", NULL);
	related_process_pid_file = debugfs_create_file("vbat_low_related_pid",
			                                       0200,
												   bd7181x_dir,
												   NULL,
												   &related_pid_fops);

	return 0;

//error_exit:
	power_supply_unregister(pwr->ac);
fail_register_ac:
	power_supply_unregister(pwr->bat);
fail_register_bat:
	platform_set_drvdata(pdev, NULL);
	kfree(pwr);

	return ret;
}

/** @brief remove pwr device
 * @param pdev platform deivce of bd7181x_power
 * @return 0
 */

static int __exit bd7181x_power_remove(struct platform_device *pdev)
{
	struct bd7181x_power *pwr = platform_get_drvdata(pdev);

	cancel_delayed_work_sync(&pwr->bd_work);
	cancel_delayed_work(&pwr->bd_work);

	/* If the product often power up/down and the power down time is long, the Coulomb Counter may have a drift. */
	/* If so, it may be better accuracy to disable Coulomb Counter using following commented out code */
	/* for stopping counting Coulomb when the product is power down(without sleep). */
	/* The condition  */
	/* (1) Product often power up and down, the power down time is long and there is no power consumed in power down time. */
	/* (2) Kernel must call this routin at power down time. */
	/* (3) Must use this code with "Start Coulomb Counter" code in bd7181x_power_probe() function */
	/* Stop Coulomb Counter */
	/* bd7181x_clear_bits(pwr->mfd, BD7181x_REG_CC_CTRL, CCNTENB); */

	sysfs_remove_group(&pwr->bat->dev.kobj, &bd7181x_sysfs_attr_group);

	power_supply_unregister(pwr->bat);
	power_supply_unregister(pwr->ac);
	platform_set_drvdata(pdev, NULL);
	kfree(pwr);

	debugfs_remove_recursive(bd7181x_dir);

	return 0;
}

static struct platform_driver bd7181x_power_driver = {
	.probe = bd7181x_power_probe,
	.remove = __exit_p(bd7181x_power_remove),
	.driver = {
		.name = "bd7181x-power",
		.owner = THIS_MODULE,
	},
};

/** @brief module initialize function */
static int __init bd7181x_power_init(void)
{
	return platform_driver_probe(&bd7181x_power_driver, bd7181x_power_probe);
}

module_init(bd7181x_power_init);

/** @brief module deinitialize function */
static void __exit bd7181x_power_exit(void)
{
	platform_driver_unregister(&bd7181x_power_driver);
}

module_exit(bd7181x_power_exit);

module_param(battery_cycle, uint, S_IWUSR | S_IRUGO);

MODULE_PARM_DESC(battery_parameters, "battery_cycle:battery charge/discharge cycles");

MODULE_AUTHOR("Tony Luo <luofc@embest-tech.com>");
MODULE_AUTHOR("Peter Yang <yanglsh@embest-tech.com>");
MODULE_DESCRIPTION("BD71815/BD71817 Battery Charger Power driver");
MODULE_LICENSE("GPL");

