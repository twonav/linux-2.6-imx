#ifndef __BD7181_BL_H_
#define __BD7181_BL_H_


struct bd7181_bl_brightness {
	unsigned int dft;
	unsigned int n_levels;
	unsigned int *levels;
	unsigned int actual;
};

struct bd7181_bl_data {
	const char *name;
	const char *supply;
	bool init_on;
	struct bd7181_bl_brightness brightness;
};

struct bd7181_bl_regulator_data {
	const char *name;
	const char *supply;

	struct mutex lock;
	bool enabled;
	unsigned int actual_uA;

	struct regulator *regulator;
	struct bd7181_bl_brightness brightness;
};



static int reg_set_state(struct bd7181_bl_regulator_data *, bool );
static int backlight_power_off(struct backlight_device *);
static int backlight_power_on(struct backlight_device *);
static unsigned int brightness2uA( struct bd7181_bl_regulator_data *, int );
static int reg_set_current_uA(struct bd7181_bl_regulator_data *, long );
static int update_brightness_status(struct backlight_device *);
static int get_brightness_status(struct backlight_device *);
static struct bd7181_bl_data *get_pdata_from_dt_node(struct platform_device *);



#endif /* __BD7181_BL_H_ */
