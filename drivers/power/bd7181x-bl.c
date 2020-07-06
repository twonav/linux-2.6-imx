/*
 * bd7181_bl.c
 *
 * Copyright 2018 CompeGPS Team S.L.
 *
 * Author(s): Joaquin Jimenez Lorenzo <jjimenez@twonav.com>
			  Theodoros Paschidis <tpaschidis@twonav.com>
 *
 * Based on userspace-consumer.c:
 * 		Copyright 2009 CompuLab, Ltd.
 * 		Author: Mike Rapoport <mike@compulab.co.il>
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 */

#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/backlight.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/delay.h>
#include "bd7181x-bl.h"


static int brighntess_value_before_disable = -1;


static ssize_t reg_show_enabled(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
    struct backlight_device *bl_dev = dev_get_drvdata(dev);
    struct bd7181_bl_regulator_data *reg_data = bl_get_data(bl_dev);

	if (reg_data->enabled) {
		return sprintf(buf, "enabled\n");
	}

	return sprintf(buf, "disabled\n");

}


static int reg_set_state(struct bd7181_bl_regulator_data *reg_data, bool enabled )
{
	int ret=0;
	mutex_lock(&reg_data->lock);
	if (enabled != reg_data->enabled) { // on _probe this is always true
		if (enabled) {
			ret = regulator_enable(reg_data->regulator);
		}
		else {
			ret = regulator_disable(reg_data->regulator);
		}

		if (ret == 0) {
			reg_data->enabled = enabled;
			ret=0;
		}
	}

	mutex_unlock(&reg_data->lock);

	return ret;
}


static int backlight_power_off(struct backlight_device *bl_dev)
{
	struct bd7181_bl_regulator_data *reg_data = bl_get_data(bl_dev);
	int ret = 0;

	dev_info(&bl_dev->dev, "Backlight poweroff requested\n");

	/* The backlight regulator consists in current-controlled DC/DC boost with a large capacitor. If it is powered off
	 * directly, the capacitor remains charged (no discharge circuit) and when powered up next time to a low brightness
	 * state, it will "flash" for around 200 msec, depending on the capacitor. It is necessary to set the lowest brightness
	 * for this time.
	 *  */
	if (reg_data->brightness.actual>1 && bl_dev->props.max_brightness > 0){
		bl_dev->props.brightness = 1;
		ret = update_brightness_status(bl_dev); /* Set to lowest brightness */

		if(ret!=0) {
			dev_err(&bl_dev->dev, "Fail to discharge backlight capacitor\n",ret);
		}

		msleep(200); /* Wait for the capacitor to discharge */
	}

	ret = reg_set_state(reg_data,false);
	if(ret!=0) {
		dev_err(&bl_dev->dev, "Fail to poweroff backlight\n",ret);
	}

	return ret;
}


static int backlight_power_on(struct backlight_device *bl_dev)
{
	struct bd7181_bl_regulator_data *reg_data = bl_get_data(bl_dev);
	int ret=0;

	dev_info(&bl_dev->dev, "Backlight poweron requested\n");

	ret = reg_set_state(reg_data,true);

	if(ret!=0) {
		dev_err(&bl_dev->dev, "Fail to poweron backlight\n",ret);
	}

	return ret;
}


static ssize_t reg_set_enabled(struct device *dev,
							   struct device_attribute *attr,
							   const char *buf,
							   size_t count)
{
	struct backlight_device *bl_dev = dev_get_drvdata(dev);
	struct bd7181_bl_regulator_data *data = bl_get_data(bl_dev);

	long val;
	if (kstrtol(buf, 10, &val) != 0) {
		return count;
	}

	if ((!data->enabled) || ((val==1) && (brighntess_value_before_disable==-1))) {
		return count;
	}

	if (val > 0) {
		bl_dev->props.brightness = brighntess_value_before_disable;
	}
	else {
		brighntess_value_before_disable = bl_dev->props.brightness;
		bl_dev->props.brightness = 1;
	}

	update_brightness_status(bl_dev);

	return count;
}


static unsigned int brightness2uA( struct bd7181_bl_regulator_data *reg_data, int brightness)
{
	unsigned int micro_amps;

	if(reg_data->brightness.levels) {
		micro_amps = reg_data->brightness.levels[brightness];
	}
	else {
		return -EINVAL;
	}

	return micro_amps;
}


static int reg_set_current_uA(struct bd7181_bl_regulator_data *reg_data, long val )
{
	int ret;

	mutex_lock(&reg_data->lock);

	ret = regulator_set_current_limit(reg_data->regulator, val, val);

	mutex_unlock(&reg_data->lock);

	if (ret>=0) {
		reg_data->actual_uA=val;
	}

	return ret;
}


static int update_brightness_status(struct backlight_device *bl_dev)
{
	struct bd7181_bl_regulator_data *reg_data = bl_get_data(bl_dev);
	int brightness = bl_dev->props.brightness;
	int ret = 0;
	int micro_amps;

	dev_info(&bl_dev->dev, "Backlight update requested to: %d\n",brightness);

	if (reg_data->brightness.actual != brightness) {

		micro_amps = brightness2uA(reg_data, brightness);

		if(micro_amps > 0) {
			ret = reg_set_current_uA(reg_data, micro_amps);
			if (ret<0) {
				return ret;
			}
			ret = backlight_power_on(bl_dev); /*No need to check if already ON, it is checked/set internally*/
		}
		else if (micro_amps==0) {
			ret = backlight_power_off(bl_dev);/*No need to check if already OFF, it is checked/set internally*/
		}
		else {
			ret = -EINVAL;
		}

		if (ret>=0) {
			reg_data->brightness.actual = brightness;
		}
	}

	return ret;
}


static int get_brightness_status(struct backlight_device *bl_dev)
{
	struct bd7181_bl_regulator_data *reg_data = bl_get_data(bl_dev);
	int brightness = reg_data->brightness.actual;

	dev_info(&bl_dev->dev,"brightness status requested\n");

	if(brightness >= 0) {
		return brightness;
	}
	else {
		return -EINVAL;
	}
}


static const struct backlight_ops reg_backlight_ops = {
	.update_status	= update_brightness_status,
	.get_brightness = get_brightness_status,
};


static DEVICE_ATTR(enable, 0644, reg_show_enabled, reg_set_enabled);


static struct attribute *attributes[] = {
	&dev_attr_enable.attr,
	NULL,
};


static const struct attribute_group attr_group = {
	.attrs	= attributes,
};


static struct bd7181_bl_data *get_pdata_from_dt_node(struct platform_device *pdev)
{
	struct bd7181_bl_data *pdata;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *brightness_np;
	struct property *prop;
	int length;
	int ret;

	if (!np) {
		return -ENODEV;
	}

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		return ERR_PTR(-ENOMEM);
	}

	pdata->name = of_get_property(np, "regulator-name", NULL);
	if (!pdata->name) {
		dev_err(&pdev->dev, "regulator-name property not found\n");
		return -EINVAL;
	}

	pdata->init_on = of_property_read_bool(np, "regulator-boot-on");/* If not found, leave as 0 (OFF) */

	pdata->supply = of_get_property(np, "regulator-supply", NULL);
	if (!pdata->supply) {
		dev_err(&pdev->dev, "regulator-supply property not found\n");
		return -EINVAL;
	}

	/*Get brightness node*/
	brightness_np = of_find_node_by_name(np, "brightness");
	if (!brightness_np) {
		dev_err(&pdev->dev, "brightness node not found\n");
		return -EINVAL;
	}

	/*Get the number of brightness levels*/
	prop = of_find_property(brightness_np, "brightness-levels", &length);
	if (!prop) {
		return -EINVAL;
	}

	pdata->brightness.n_levels = length / sizeof(u32);

	if (pdata->brightness.n_levels > 1) { /* At least two levels required */
		size_t size = sizeof(*pdata->brightness.levels) * pdata->brightness.n_levels;

		pdata->brightness.levels = devm_kzalloc(&pdev->dev, size, GFP_KERNEL);
		if (!pdata->brightness.levels) {
			return -ENOMEM;
		}

		ret = of_property_read_u32_array(brightness_np, "brightness-levels",
										 pdata->brightness.levels,
										 pdata->brightness.n_levels);
		if (ret < 0) {
			dev_err(&pdev->dev, "brightness-levels property not found\n");
			return ret;
		}

		ret = of_property_read_u32(brightness_np, "default-brightness-level",
					   	   	   	   &pdata->brightness.dft);
		if (ret < 0) {
			dev_err(&pdev->dev, "default-brightness-level property not found\n");
			return ret;
		}
	}
	else {
		dev_err(&pdev->dev, "too few values for brightness-levels property");
		return -EINVAL;
	}

	return pdata;
}


static int bd7181x_backlight_probe(struct platform_device *pdev)
{
	struct bd7181_bl_data *pdata;
	struct bd7181_bl_regulator_data *drvdata;
	struct backlight_properties bl_prop;
	struct backlight_device *bl_dev;
	int ret;

	dev_info(&pdev->dev, "Probe called\n");

	pdata = dev_get_platdata(&pdev->dev);
	if (!pdata && pdev->dev.of_node) {
		pdata = get_pdata_from_dt_node(pdev);
		if (IS_ERR(pdata)) {
			return PTR_ERR(pdata);
		}
	}
	if (!pdata) {
		return -EINVAL;
	}

	drvdata = devm_kzalloc(&pdev->dev, sizeof(struct bd7181_bl_regulator_data), GFP_KERNEL);
	if (drvdata == NULL) {
		return -ENOMEM;
	}

	drvdata->name = pdata->name;
	drvdata->supply = pdata->supply;

	mutex_init(&drvdata->lock);

	drvdata->regulator = devm_regulator_get(&pdev->dev, drvdata->supply);
	if (IS_ERR(drvdata->regulator)) {
		ret = PTR_ERR(drvdata->regulator);
		dev_err(&pdev->dev, "Failed to obtain supply '%s': %d\n", drvdata->supply, ret);
		return ret;
	}

	pdata->brightness.actual = 0;
	drvdata->brightness = pdata->brightness;
	

	ret = sysfs_create_group(&pdev->dev.kobj, &attr_group);
	if (ret != 0) {
		printk(KERN_WARNING, "bd7181x_backlight: failed to create backlight attributes\n");
	}

	/*Set backlight interface*/
	memset(&bl_prop, 0, sizeof(struct backlight_properties));/* Reset memory */
	bl_prop.type = BACKLIGHT_RAW;

	if (pdata->brightness.n_levels > 1) {
		bl_prop.max_brightness = pdata->brightness.n_levels-1;
	}
	else {
		dev_err(&pdev->dev, "Too few brightness levels\n");
		ret = -EINVAL;
		goto err;
	}

	bl_dev = backlight_device_register(dev_name(&pdev->dev), &pdev->dev, drvdata, &reg_backlight_ops, &bl_prop);
	//bl_dev = devm_backlight_device_register(&pdev->dev, dev_name(&pdev->dev), &pdev->dev, drvdata, &reg_backlight_ops, &bl_prop);

	msleep(10);

	printk("DEBUG-mark1\n");

	if (pdata->init_on) {
		struct bd7181_bl_regulator_data *reg_data = bl_get_data(bl_dev);
		reg_data->enabled = false; // Make sure it is called (so that it turns on)

		bl_dev->props.brightness = drvdata->brightness.dft; // Set initial status to default brightness
		ret = update_brightness_status(bl_dev);
		if (ret) {
			dev_err(&pdev->dev, "Failed to set initial state to ON: %d\n", ret);
			goto err;
		}

	}
	else {
		struct bd7181_bl_regulator_data *reg_data = bl_get_data(bl_dev);
		reg_data->enabled = true;  // Make sure it is called
		bl_dev->props.brightness = drvdata->brightness.dft;
		ret = backlight_power_on(bl_dev);
		if (ret) {
			dev_err(&pdev->dev, "Failed to set initial state to OFF: %d\n", ret);
			goto err;
		}
	}

	platform_set_drvdata(pdev, bl_dev);
	dev_info(&pdev->dev, "Probed successfully\n");

	printk("DEBUG-mark2\n");
	return 0;

err:
	sysfs_remove_group(&pdev->dev.kobj, &attr_group);

	return ret;

}


static int bd7181x_backlight_remove(struct platform_device *pdev)
{
	struct backlight_device *bl_dev = platform_get_drvdata(pdev);
	dev_info(&pdev->dev, "Removing backlight\n");

	sysfs_remove_group(&pdev->dev.kobj, &attr_group);

	backlight_power_off(bl_dev);

	backlight_device_unregister(bl_dev);

	return 0;
}


static const struct of_device_id bd7181x_backlight_of_match[] = {
	{ .compatible = "bd7181x-backlight", },
	{/* sentinel */ }
};


MODULE_DEVICE_TABLE(of, bd7181x_backlight_of_match);


static struct platform_driver bd7181x_backlight_driver = {
	.probe		= bd7181x_backlight_probe,
	.remove		= bd7181x_backlight_remove,
	.driver		= {
		.name		= "bd7181x-backlight",
		.of_match_table = bd7181x_backlight_of_match,
	},
};


module_platform_driver(bd7181x_backlight_driver);

MODULE_AUTHOR("Joaquin Jimenez Lorenzo <jjimenez@twonav.com>");
MODULE_AUTHOR("Theodoros Paschidis <tpaschidis@twonav.com>");
MODULE_DESCRIPTION("Backlight for bd7181 PMIC");
MODULE_LICENSE("GPL");
