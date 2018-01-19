/*
 * t55149gd030j LCD panel driver.
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 * Author: Jingoo Han  <jg1.han@samsung.com>
 *
 * Derived from drivers/video/s6e63m0.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <linux/wait.h>
#include <linux/module.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>
#include <linux/lcd.h>
#include <linux/backlight.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

//#include <linux/t55149gd030j.h>
#include "t55149gd030j.h"

#define MIN_BRIGHTNESS		0
#define MAX_BRIGHTNESS		255
#define DEFAULT_BRIGHTNESS	150

#define POWER_IS_ON(pwr)	((pwr) <= FB_BLANK_NORMAL)

struct t55149gd030j {
	struct device			*dev;
	struct spi_device		*spi;
	unsigned int			power;
	struct lcd_device		*ld;
	struct backlight_device		*bd;
	struct lcd_platform_data	*lcd_pd;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend		early_suspend;
#endif
};


static int t55149gd030j_spi_write_word(struct t55149gd030j *lcd, int cmd, int data)
{
	u8 buf[3];
	struct spi_message msg;

	struct spi_transfer xfer = {
		.len		= 3,
		.tx_buf		= buf,
	};
	buf[0] = cmd & 0xFF;
	buf[1] = (data>>8) & 0xFF;
	buf[2] = data & 0xFF;
 	
	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);
	
	return spi_sync(lcd->spi, &msg);
}

static int t55149gd030j_spi_write(struct t55149gd030j *lcd, int address,
	int command)
{
	int ret = 0;

	ret = t55149gd030j_spi_write_word(lcd, 0x70, address);
	udelay(5);
	if(ret<0)printk("t55149gd030j Driver : Error During SPI access Address %d\n",ret);
	ret = t55149gd030j_spi_write_word(lcd, 0x72, command);
	udelay(5);
	if(ret<0)printk("t55149gd030j Driver : Error During SPI access Command %d\n",ret);
	
	printk("LCD SPI Command addr: 0x%04X , data : 0x%04X\n",address & 0xFFFF,command & 0xFFFF);
	return ret;
}

static int t55149gd030j_panel_send_sequence(struct t55149gd030j *lcd,
	const unsigned short *wbuf)
{
	int ret = 0, i = 0;

	while ((wbuf[i] & DEFMASK) != ENDDEF) {
		if ((wbuf[i] & DEFMASK) != SLEEPMSEC) {
			ret = t55149gd030j_spi_write(lcd, wbuf[i], wbuf[i+1]);
			if (ret)
				break;
		} else
			mdelay(wbuf[i+1]);				
			//udelay(wbuf[i+1]*1000);
		i += 2;
	}

	return ret;
}

static int _t55149gd030j_gamma_ctl(struct t55149gd030j *lcd,
	const unsigned int *gamma)
{
	unsigned int i = 0;
	int ret = 0;

	for (i = 0 ; i < GAMMA_TABLE_COUNT / 3; i++) {
		ret = t55149gd030j_spi_write(lcd, 0x40 + i, gamma[i]);
		ret = t55149gd030j_spi_write(lcd, 0x50 + i, gamma[i+7*1]);
		ret = t55149gd030j_spi_write(lcd, 0x60 + i, gamma[i+7*2]);
		if (ret) {
			dev_err(lcd->dev, "failed to set gamma table.\n");
			goto gamma_err;
		}
	}

gamma_err:
	return ret;
}

static int t55149gd030j_gamma_ctl(struct t55149gd030j *lcd, int brightness)
{
	int ret = 0;
	int gamma = 0;

	if ((brightness >= 0) && (brightness <= 50))
		gamma = 0;
	else if ((brightness > 50) && (brightness <= 100))
		gamma = 1;
	else if ((brightness > 100) && (brightness <= 150))
		gamma = 2;
	else if ((brightness > 150) && (brightness <= 200))
		gamma = 3;
	else if ((brightness > 200) && (brightness <= 255))
		gamma = 4;

	ret = _t55149gd030j_gamma_ctl(lcd, gamma_table.gamma_22_table[gamma]);

	return ret;
}

#ifdef CONFIG_FB_S5P_T55149GD030J
extern int t55149gd030j_reset();

#else
static int t55149gd030j_reset(void)
{
	int err = 0;
	
	
//	err = gpio_request_one(EXYNOS4_GPC1(2), GPIOF_OUT_INIT_HIGH, "GPC1");
//	if (err) {
//		printk(KERN_ERR "failed to request GPC1 for "
//				"lcd reset control\n");
//		return err;
//	}
//	gpio_set_value(EXYNOS4_GPC1(2), 0);
//	mdelay(10);
//
//	gpio_set_value(EXYNOS4_GPC1(2), 1);
//	mdelay(10);

	//gpio_free(EXYNOS4_GPC1(2));
	
	return err;
	
}
#endif

static void t55149gd030j_set_ID(int id_val)
{
	//LCD IM0 signal assigned to GPX2[0]
	//gpio_set_value(EXYNOS4_GPX2(0), id_val & 0x1); //SPI ID
}

static void t55149gd030j_set_rgbmode(void)
{
	//LCD IM1 signal assigned to GPX1[7]
//	gpio_set_value(EXYNOS4_GPX1(7), 1); //RGB mode
//	t55149gd030j_set_ID(1);
}

static void t55149gd030j_set_spimode(void)
{
	//LCD IM1 signal assigned to GPX1[7]
//	gpio_set_value(EXYNOS4_GPX1(7), 0); //SPI mode
//	t55149gd030j_set_ID(0);
}


static void t55149gd030j_disable_RGBLines(void)
{
//	s3c_gpio_cfgall_range(EXYNOS4_GPF0(0), 8, S3C_GPIO_SFN(0), S3C_GPIO_PULL_DOWN);
//	s3c_gpio_cfgall_range(EXYNOS4_GPF1(0), 8, S3C_GPIO_SFN(0), S3C_GPIO_PULL_DOWN);
//	s3c_gpio_cfgall_range(EXYNOS4_GPF2(0), 5, S3C_GPIO_SFN(0), S3C_GPIO_PULL_DOWN);
//	s3c_gpio_cfgall_range(EXYNOS4_GPF2(7), 2, S3C_GPIO_SFN(0), S3C_GPIO_PULL_DOWN);
//	s3c_gpio_cfgall_range(EXYNOS4_GPF3(0), 4, S3C_GPIO_SFN(0), S3C_GPIO_PULL_DOWN);
}



/*
extern void s3cfb_gpio_setup_24bpp(unsigned int start, unsigned int size,
		unsigned int cfg, s5p_gpio_drvstr_t drvstr);


static void t55149gd030j_enable_RGBLines()
{
	s3cfb_gpio_setup_24bpp(EXYNOS4_GPF0(0), 8, S3C_GPIO_SFN(2), S5P_GPIO_DRVSTR_LV4);
	s3cfb_gpio_setup_24bpp(EXYNOS4_GPF1(0), 8, S3C_GPIO_SFN(2), S5P_GPIO_DRVSTR_LV4);
	s3cfb_gpio_setup_24bpp(EXYNOS4_GPF2(0), 8, S3C_GPIO_SFN(2), S5P_GPIO_DRVSTR_LV4);
	s3cfb_gpio_setup_24bpp(EXYNOS4_GPF3(0), 4, S3C_GPIO_SFN(2), S5P_GPIO_DRVSTR_LV4);
}
*/

static int t55149gd030j_ldi_init(struct t55149gd030j *lcd)
{
	int ret, i;
	const unsigned short *init_seq[] = {
		SEQ_SETTING,
		//SEQ_STAND_BY_OFF,
	};
	
	//t55149gd030j_reset();
	
	for (i = 0; i < ARRAY_SIZE(init_seq); i++) {
		ret = t55149gd030j_panel_send_sequence(lcd, init_seq[i]);
		if (ret)
			break;
	}

	return ret;
}

static int t55149gd030j_ldi_enable(struct t55149gd030j *lcd)
{
	int ret, i;
	const unsigned short *init_seq[] = {
		//SEQ_STAND_BY_OFF,
		SEQ_DISPLAY_ON,
	};

	for (i = 0; i < ARRAY_SIZE(init_seq); i++) {
		ret = t55149gd030j_panel_send_sequence(lcd, init_seq[i]);
		if (ret)
			break;
	}

	return ret;
}

static int t55149gd030j_ldi_disable(struct t55149gd030j *lcd)
{
	int ret, i;

	const unsigned short *init_seq[] = {
		SEQ_DISPLAY_OFF,
		//SEQ_STAND_BY_ON,
	};

	for (i = 0; i < ARRAY_SIZE(init_seq); i++) {
		ret = t55149gd030j_panel_send_sequence(lcd, init_seq[i]);
		if (ret)
			break;
	}

	return ret;
}

static int t55149gd030j_power_on(struct t55149gd030j *lcd)
{
	int ret = 0;
	struct lcd_platform_data *pd = NULL;
	struct backlight_device *bd = NULL;

	pd = lcd->lcd_pd;
	if (!pd) {
		dev_err(lcd->dev, "platform data is NULL.\n");
		return -EFAULT;
	}

	bd = lcd->bd;
	if (!bd) {
		dev_err(lcd->dev, "backlight device is NULL.\n");
		return -EFAULT;
	}

	if (!pd->power_on) {
		dev_err(lcd->dev, "power_on is NULL.\n");
		return -EFAULT;
	} else {
		pd->power_on(lcd->ld, 1);
		mdelay(pd->power_on_delay);
	}

	//t55149gd030j_disable_RGBLines();
	t55149gd030j_set_spimode();


	if (!pd->reset) {
		dev_err(lcd->dev, "reset is NULL.\n");
		return -EFAULT;
	} else {
		pd->reset(lcd->ld);
		mdelay(pd->reset_delay);
	}
	
	
	ret = t55149gd030j_ldi_init(lcd);
	if (ret) {
		dev_err(lcd->dev, "failed to initialize ldi.\n");
		return ret;
	}

	//ret = t55149gd030j_ldi_enable(lcd);
	//if (ret) {
	//	dev_err(lcd->dev, "failed to enable ldi.\n");
	//	return ret;
	//}

	/* set brightness to current value after power on or resume. */
	//ret = t55149gd030j_gamma_ctl(lcd, bd->props.brightness);
	//if (ret) {
	//	dev_err(lcd->dev, "lcd gamma setting failed.\n");
	//	return ret;
	//}
	
	/*	
	t55149gd030j_spi_write(lcd, 0x000F, 0x0000);
	t55149gd030j_spi_write(lcd, 0x000C, 0x0110);
	t55149gd030j_spi_write(lcd, 0x0212, 0x0000);
	t55149gd030j_spi_write(lcd, 0x0213, 0x018F);	
	t55149gd030j_spi_write(lcd, 0x0202, 0x0000);
	*/		
	/*
	ret=0;
	line_pos=0;
	while(1){
		switch(ret){
		case 0:
			t55149gd030j_spi_write(lcd, 0x0202, 0x0000);	
			printk("ret : %d\n",line_pos);
			msleep(10000);
		break;
		case 1:
			printk("ret : %d\n",ret);
			msleep(10000);
		break;
		case 2:
			printk("ret : %d\n",ret);
			msleep(10000);
		break;
		case 3:
			printk("ret : %d\n",ret);
			msleep(10000);
		break;
		default:
			ret=0;
		break;		
		}
		//ret=ret+1;
		//ret=ret&0x3;
		line_pos=line_pos+1;
	}	
	*/

	//t55149gd030j_set_rgbmode();
		
	//t55149gd030j_enable_RGBLines();

	return 0;
}

static int t55149gd030j_power_off(struct t55149gd030j *lcd)
{
	int ret = 0;
	struct lcd_platform_data *pd = NULL;

	pd = lcd->lcd_pd;
	if (!pd) {
		dev_err(lcd->dev, "platform data is NULL\n");
		return -EFAULT;
	}

	ret = t55149gd030j_ldi_disable(lcd);
	if (ret) {
		dev_err(lcd->dev, "lcd setting failed.\n");
		return -EIO;
	}

	mdelay(pd->power_off_delay);

	if (!pd->power_on) {
		dev_err(lcd->dev, "power_on is NULL.\n");
		return -EFAULT;
	} else
		pd->power_on(lcd->ld, 0);

	return 0;
}

static int t55149gd030j_power(struct t55149gd030j *lcd, int power)
{
	int ret = 0;

	if (POWER_IS_ON(power) && !POWER_IS_ON(lcd->power))
		ret = t55149gd030j_power_on(lcd);
	else if (!POWER_IS_ON(power) && POWER_IS_ON(lcd->power))
		ret = t55149gd030j_power_off(lcd);

	if (!ret)
		lcd->power = power;

	return ret;
}

static int t55149gd030j_get_power(struct lcd_device *ld)
{
	struct t55149gd030j *lcd = lcd_get_data(ld);

	return lcd->power;
}

static int t55149gd030j_set_power(struct lcd_device *ld, int power)
{
	struct t55149gd030j *lcd = lcd_get_data(ld);

	if (power != FB_BLANK_UNBLANK && power != FB_BLANK_POWERDOWN &&
		power != FB_BLANK_NORMAL) {
		dev_err(lcd->dev, "power value should be 0, 1 or 4.\n");
		return -EINVAL;
	}

	return t55149gd030j_power(lcd, power);
}

static int t55149gd030j_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static int t55149gd030j_set_brightness(struct backlight_device *bd)
{
	int ret = 0;
	int brightness = bd->props.brightness;
	struct t55149gd030j *lcd = dev_get_drvdata(&bd->dev);

	if (brightness < MIN_BRIGHTNESS ||
		brightness > bd->props.max_brightness) {
		dev_err(&bd->dev, "lcd brightness should be %d to %d.\n",
			MIN_BRIGHTNESS, MAX_BRIGHTNESS);
		return -EINVAL;
	}

	ret = t55149gd030j_gamma_ctl(lcd, bd->props.brightness);
	if (ret) {
		dev_err(&bd->dev, "lcd brightness setting failed.\n");
		return -EIO;
	}

	return ret;
}

static struct lcd_ops t55149gd030j_lcd_ops = {
	.get_power = t55149gd030j_get_power,
	.set_power = t55149gd030j_set_power,
};

static const struct backlight_ops t55149gd030j_backlight_ops = {
	.get_brightness = t55149gd030j_get_brightness,
	.update_status = t55149gd030j_set_brightness,
};

#ifdef	CONFIG_HAS_EARLYSUSPEND
unsigned int before_power;

static void t55149gd030j_early_suspend(struct early_suspend *handler)
{
	struct t55149gd030j *lcd = NULL;

	lcd = container_of(handler, struct t55149gd030j, early_suspend);

	before_power = lcd->power;

	t55149gd030j_power(lcd, FB_BLANK_POWERDOWN);
}

static void t55149gd030j_late_resume(struct early_suspend *handler)
{
	struct t55149gd030j *lcd = NULL;

	lcd = container_of(handler, struct t55149gd030j, early_suspend);

	if (before_power == FB_BLANK_UNBLANK)
		lcd->power = FB_BLANK_POWERDOWN;

	t55149gd030j_power(lcd, before_power);
}
#endif

static int __refdata t55149gd030j_probe(struct spi_device *spi)
{
	int ret = 0;
	struct t55149gd030j *lcd = NULL;
	struct lcd_device *ld = NULL;
	struct backlight_device *bd = NULL;

	lcd = kzalloc(sizeof(struct t55149gd030j), GFP_KERNEL);
	if (!lcd)
		return -ENOMEM;

	/* t55149gd030j lcd panel uses 3-wire 16bits SPI Mode. */
	spi->bits_per_word = 8;

	ret = spi_setup(spi);
	if (ret < 0) {
		dev_err(&spi->dev, "spi setup failed.\n");
		goto out_free_lcd;
	}

	lcd->spi = spi;
	lcd->dev = &spi->dev;

	lcd->lcd_pd = (struct lcd_platform_data *)spi->dev.platform_data;
	if (!lcd->lcd_pd) {
		dev_err(&spi->dev, "platform data is NULL\n");
		goto out_free_lcd;
	}

	ld = lcd_device_register("t55149gd030j-lcd-ops", &spi->dev, lcd,
		&t55149gd030j_lcd_ops);
	if (IS_ERR(ld)) {
		ret = PTR_ERR(ld);
		goto out_free_lcd;
	}

	lcd->ld = ld;

	bd = backlight_device_register("t55149gd030j-bl", &spi->dev, lcd,
		&t55149gd030j_backlight_ops, NULL);
	if (IS_ERR(bd)) {
		ret =  PTR_ERR(bd);
		goto out_lcd_unregister;
	}

	bd->props.max_brightness = MAX_BRIGHTNESS;
	bd->props.brightness = DEFAULT_BRIGHTNESS;
	bd->props.type = BACKLIGHT_RAW;
	lcd->bd = bd;

	if (!lcd->lcd_pd->lcd_enabled) {
		/*
		 * if lcd panel was off from bootloader then
		 * current lcd status is powerdown and then
		 * it enables lcd panel.
		 */
		lcd->power = FB_BLANK_POWERDOWN;

		t55149gd030j_power(lcd, FB_BLANK_UNBLANK);
	} else
		lcd->power = FB_BLANK_UNBLANK;

	dev_set_drvdata(&spi->dev, lcd);

#ifdef CONFIG_HAS_EARLYSUSPEND
	lcd->early_suspend.suspend = t55149gd030j_early_suspend;
	lcd->early_suspend.resume = t55149gd030j_late_resume;
	lcd->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB - 1;
	register_early_suspend(&lcd->early_suspend);
#endif
	dev_info(&spi->dev, "t55149gd030j panel driver has been probed.\n");

	return 0;

out_lcd_unregister:
	lcd_device_unregister(ld);
out_free_lcd:
	kfree(lcd);
	return ret;
}

static int t55149gd030j_remove(struct spi_device *spi)
{
	struct t55149gd030j *lcd = dev_get_drvdata(&spi->dev);

	t55149gd030j_power(lcd, FB_BLANK_POWERDOWN);
	lcd_device_unregister(lcd->ld);
	kfree(lcd);

	return 0;
}

#if defined(CONFIG_PM)
#ifndef CONFIG_HAS_EARLYSUSPEND
unsigned int before_power;

static int t55149gd030j_suspend(struct spi_device *spi, pm_message_t mesg)
{
	int ret = 0;
	struct t55149gd030j *lcd = dev_get_drvdata(&spi->dev);

	dev_dbg(&spi->dev, "lcd->power = %d\n", lcd->power);

	before_power = lcd->power;

	/*
	 * when lcd panel is suspend, lcd panel becomes off
	 * regardless of status.
	 */
	ret = t55149gd030j_power(lcd, FB_BLANK_POWERDOWN);

	return ret;
}

static int t55149gd030j_resume(struct spi_device *spi)
{
	int ret = 0;
	struct t55149gd030j *lcd = dev_get_drvdata(&spi->dev);

	/*
	 * after suspended, if lcd panel status is FB_BLANK_UNBLANK
	 * (at that time, before_power is FB_BLANK_UNBLANK) then
	 * it changes that status to FB_BLANK_POWERDOWN to get lcd on.
	 */
	if (before_power == FB_BLANK_UNBLANK)
		lcd->power = FB_BLANK_POWERDOWN;

	dev_dbg(&spi->dev, "before_power = %d\n", before_power);

	ret = t55149gd030j_power(lcd, before_power);

	return ret;
}
#endif
#else
#define t55149gd030j_suspend	NULL
#define t55149gd030j_resume	NULL
#endif

static void t55149gd030j_shutdown(struct spi_device *spi)
{
	struct t55149gd030j *lcd = dev_get_drvdata(&spi->dev);

	t55149gd030j_power(lcd, FB_BLANK_POWERDOWN);
}

static struct spi_driver t55149gd030j_driver = {
	.driver = {
		.name	= "t55149gd030j-spi",
		.bus	= &spi_bus_type,
		.owner	= THIS_MODULE,
	},
	.probe		= t55149gd030j_probe,
	.remove		= t55149gd030j_remove,
	.shutdown	= t55149gd030j_shutdown,
//#ifndef CONFIG_HAS_EARLYSUSPEND
//	.suspend	= t55149gd030j_suspend,
//	.resume		= t55149gd030j_resume,
//#endif
};

/*
static int t55149gd030j_bl_init(void)
{
	return spi_register_driver(&t55149gd030j_driver);
}

static void t55149gd030j_exit(void)
{
	spi_unregister_driver(&t55149gd030j_driver);
}

module_init(t55149gd030j_bl_init);
module_exit(t55149gd030j_exit);
*/
module_spi_driver(t55149gd030j_driver);

MODULE_AUTHOR("Jingoo Han <jg1.han@samsung.com>");
MODULE_DESCRIPTION("t55149gd030j LCD Driver");
MODULE_LICENSE("GPL");
