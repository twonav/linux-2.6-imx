/*
 * LED Cycle Trigger
 *
 * Copyright (C) 2013 Gaël Portay <g.portay@overkiz.com>
 *
 * Based on Richard Purdie's ledtrig-timer.c.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/leds.h>
#include <linux/hrtimer.h>
#include <linux/string.h>

#include "../leds.h"

#define timer_to_cycle(timer) \
	container_of(timer, struct cycle_trig_data, timer)

#define DEFAULT_INVERVAL 10000000
#define DEFAULT_COUNT 0
#define DELIMITER 0x0a

struct cycle_trig_data {
	struct led_classdev *cdev;
	spinlock_t lock;
	struct hrtimer timer;
	ktime_t interval;
	unsigned int plot_index;
	size_t plot_count;
	u8 *plot;
	unsigned int cycle_count;
	unsigned int cycle_repeat;
};

static int cycle_start(struct cycle_trig_data *data)
{
	unsigned long flags;

	if (hrtimer_active(&data->timer))
		return -EINVAL;

	spin_lock_irqsave(&data->lock, flags);
	data->plot_index = 0;
	data->cycle_count = 0;
	hrtimer_start(&data->timer, ktime_get(), HRTIMER_MODE_ABS);
	spin_unlock_irqrestore(&data->lock, flags);

	return 1;
}

static int cycle_stop(struct cycle_trig_data *data)
{
	unsigned long flags;

	if (!hrtimer_active(&data->timer))
		return -EINVAL;

	spin_lock_irqsave(&data->lock, flags);
	data->plot_index = 0;
	data->cycle_count = 0;
	hrtimer_cancel(&data->timer);
	spin_unlock_irqrestore(&data->lock, flags);

	return 1;
}

static int cycle_reset(struct cycle_trig_data *data)
{
	data->plot_index = 0;
	data->cycle_count = 0;

	return 1;
}

static int cycle_pause(struct cycle_trig_data *data)
{
	unsigned long flags;

	if (!hrtimer_active(&data->timer))
		return -EINVAL;

	spin_lock_irqsave(&data->lock, flags);
	hrtimer_cancel(&data->timer);
	spin_unlock_irqrestore(&data->lock, flags);

	return 1;
}

static int cycle_resume(struct cycle_trig_data *data)
{
	unsigned long flags;

	if (hrtimer_active(&data->timer))
		return -EINVAL;

	spin_lock_irqsave(&data->lock, flags);
	hrtimer_start(&data->timer, ktime_get(), HRTIMER_MODE_ABS);
	spin_unlock_irqrestore(&data->lock, flags);

	return 1;
}

static enum hrtimer_restart led_cycle_function(struct hrtimer *timer)
{
	struct cycle_trig_data *data = timer_to_cycle(timer);
	struct led_classdev *cdev = data->cdev;
	enum hrtimer_restart ret = HRTIMER_RESTART;
	unsigned long flags;

	spin_lock_irqsave(&data->lock, flags);
	if (data->plot) {
		led_set_brightness(cdev, data->plot[data->plot_index]);
		data->plot_index++;
		if (data->plot_index >= data->plot_count) {
			data->plot_index = 0;
			data->cycle_count++;
			if (data->cycle_repeat &&
			    data->cycle_count >= data->cycle_repeat)
				ret = HRTIMER_NORESTART;
		}
	}
	spin_unlock_irqrestore(&data->lock, flags);

	if (ret == HRTIMER_RESTART)
		hrtimer_add_expires(timer, data->interval);

	return ret;
}

static ssize_t cycle_status_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct cycle_trig_data *data = led_cdev->trigger_data;

	return snprintf(buf, PAGE_SIZE, "%sactive\n",
			hrtimer_active(&data->timer) ? "" : "in");
}

static DEVICE_ATTR(status, 0444, cycle_status_show, NULL);

static ssize_t cycle_control_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct cycle_trig_data *data = led_cdev->trigger_data;

	if (strncmp(buf, "start", sizeof("start") - 1) == 0)
		cycle_start(data);
	else if (strncmp(buf, "stop", sizeof("stop") - 1) == 0)
		cycle_stop(data);
	else if (strncmp(buf, "reset", sizeof("reset") - 1) == 0)
		cycle_reset(data);
	else if (strncmp(buf, "pause", sizeof("stop") - 1) == 0)
		cycle_pause(data);
	else if (strncmp(buf, "resume", sizeof("resume") - 1) == 0)
		cycle_resume(data);
	else
		return -EINVAL;

	return size;
}

static DEVICE_ATTR(control, 0200, NULL, cycle_control_store);

static ssize_t cycle_repeat_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct cycle_trig_data *data = led_cdev->trigger_data;

	return snprintf(buf, PAGE_SIZE, "%u\n", data->cycle_repeat);
}

static ssize_t cycle_repeat_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct cycle_trig_data *data = led_cdev->trigger_data;
	unsigned long count;
	int err;

	err = kstrtoul(buf, 0, &count);
	if (err)
		return -EINVAL;

	data->cycle_repeat = count;

	return size;
}

static DEVICE_ATTR(repeat, 0644, cycle_repeat_show, cycle_repeat_store);

static ssize_t cycle_count_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct cycle_trig_data *data = led_cdev->trigger_data;

	return snprintf(buf, PAGE_SIZE, "%d\n", data->cycle_count);
}

static DEVICE_ATTR(count, 0444, cycle_count_show, NULL);

static ssize_t cycle_interval_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct cycle_trig_data *data = led_cdev->trigger_data;
	unsigned long interval = div_s64(ktime_to_ns(data->interval), 1000000);

	return snprintf(buf, PAGE_SIZE, "%lu\n", interval);
}

static ssize_t cycle_interval_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct cycle_trig_data *data = led_cdev->trigger_data;
	unsigned long interval;
	int err;

	err = kstrtoul(buf, 0, &interval);
	if (err)
		return -EINVAL;

	data->interval = ktime_set(interval / 1000,
				   (interval % 1000) * 1000000);

	return size;
}

static DEVICE_ATTR(interval, 0644, cycle_interval_show,
		cycle_interval_store);

static ssize_t cycle_rawplot_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct cycle_trig_data *data = led_cdev->trigger_data;
	unsigned long flags;

	spin_lock_irqsave(&data->lock, flags);
	if (data->plot)
		memcpy(buf, data->plot, data->plot_count);
	spin_unlock_irqrestore(&data->lock, flags);

	return data->plot_count;
}

static ssize_t cycle_rawplot_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct cycle_trig_data *data = led_cdev->trigger_data;
	unsigned long flags;
	u8 *plot;
	u8 *oldplot = NULL;

	plot = kzalloc(size, GFP_KERNEL);
	if (plot) {
		hrtimer_cancel(&data->timer);

		memcpy(plot, buf, size);

		spin_lock_irqsave(&data->lock, flags);
		if (data->plot)
			oldplot = data->plot;
		data->plot = plot;
		data->plot_index = 0;
		data->plot_count = size;
		spin_unlock_irqrestore(&data->lock, flags);

		kfree(oldplot);

		hrtimer_start(&data->timer, ktime_get(), HRTIMER_MODE_ABS);
	} else
		return -ENOMEM;

	return data->plot_count;
}

static DEVICE_ATTR(rawplot, 0644, cycle_rawplot_show,
		cycle_rawplot_store);

static ssize_t cycle_plot_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct cycle_trig_data *data = led_cdev->trigger_data;
	unsigned long flags;
	int i;
	size_t size = 0;

	spin_lock_irqsave(&data->lock, flags);
	if (data->plot)
		for (i = 0; i < data->plot_count; i++)
			size += snprintf(&buf[size], PAGE_SIZE - size, "%u%c",
					 data->plot[i], DELIMITER);
	spin_unlock_irqrestore(&data->lock, flags);

	return size;
}

static ssize_t cycle_plot_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct cycle_trig_data *data = led_cdev->trigger_data;
	unsigned long flags;
	ssize_t count;
	const char *del;

	count = 0;
	del = buf;
	for (;;) {
		del = strchr(del, DELIMITER);
		if (!del)
			break;
		del++;
		count++;
	}

	if (count) {
		int i = 0;
		char str[4];
		const char *ptr;
		u8 *oldplot = NULL;
		u8 *plot = kzalloc(size, GFP_KERNEL);

		if (!plot)
			return -ENOMEM;

		ptr = buf;
		del = buf;
		for (;;) {
			int err;
			unsigned long val = 0;

			del = strchr(ptr, DELIMITER);
			if (!del)
				break;

			err = (del - ptr);
			if (err >= sizeof(str)) {
				kfree(plot);
				return -EINVAL;
			}

			strncpy(str, ptr, err);
			str[err] = 0;
			err = kstrtoul(str, 0, &val);
			if (err || (val > LED_FULL)) {
				kfree(plot);
				return (err < 0) ? err : -EINVAL;
			}

			plot[i] = val;
			ptr = del + 1;
			i++;
		}

		hrtimer_cancel(&data->timer);

		spin_lock_irqsave(&data->lock, flags);
		if (data->plot)
			oldplot = data->plot;
		data->plot = plot;
		data->plot_index = 0;
		data->plot_count = count;
		spin_unlock_irqrestore(&data->lock, flags);

		kfree(oldplot);

		hrtimer_start(&data->timer, ktime_get(), HRTIMER_MODE_ABS);
	}

	return size;
}

static DEVICE_ATTR(plot, 0644, cycle_plot_show,
		cycle_plot_store);

static void cycle_trig_activate(struct led_classdev *led_cdev)
{
	struct cycle_trig_data *data;

	data = kzalloc(sizeof(struct cycle_trig_data), GFP_KERNEL);
	if (!data)
		return;

	led_cdev->trigger_data = data;

	spin_lock_init(&data->lock);

	data->cdev = led_cdev;
	data->interval = ktime_set(0, DEFAULT_INVERVAL);
	data->cycle_count = 0;
	data->cycle_repeat = DEFAULT_COUNT;

	data->plot_index = 0;
	data->plot_count = (LED_FULL * 2);
	data->plot = kzalloc(data->plot_count, GFP_KERNEL);
	if (data->plot) {
		int i;
		int val = LED_FULL;
		int step = 1;
		for (i = 0; i < data->plot_count; i++) {
			data->plot[i] = val;
			if (val == LED_FULL)
				step = -1;
			else if (val == LED_OFF)
				step = 1;
			val += step;
		}

		hrtimer_init(&data->timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
		data->timer.function = led_cycle_function;
	}

	device_create_file(led_cdev->dev, &dev_attr_status);
	device_create_file(led_cdev->dev, &dev_attr_control);
	device_create_file(led_cdev->dev, &dev_attr_repeat);
	device_create_file(led_cdev->dev, &dev_attr_count);
	device_create_file(led_cdev->dev, &dev_attr_interval);
	device_create_file(led_cdev->dev, &dev_attr_rawplot);
	device_create_file(led_cdev->dev, &dev_attr_plot);
}

static void cycle_trig_deactivate(struct led_classdev *led_cdev)
{
	struct cycle_trig_data *data = led_cdev->trigger_data;
	unsigned long flags;
	u8 *plot;

	device_remove_file(led_cdev->dev, &dev_attr_status);
	device_remove_file(led_cdev->dev, &dev_attr_control);
	device_remove_file(led_cdev->dev, &dev_attr_repeat);
	device_remove_file(led_cdev->dev, &dev_attr_count);
	device_remove_file(led_cdev->dev, &dev_attr_interval);
	device_remove_file(led_cdev->dev, &dev_attr_rawplot);
	device_remove_file(led_cdev->dev, &dev_attr_plot);

	if (data) {
		hrtimer_cancel(&data->timer);

		spin_lock_irqsave(&data->lock, flags);
		plot = data->plot;
		if (data->plot) {
			data->plot = NULL;
			data->plot_index = 0;
			data->plot_count = 0;
		}
		spin_unlock_irqrestore(&data->lock, flags);

		kfree(plot);

		kfree(data);
	}
}

static struct led_trigger cycle_led_trigger = {
	.name = "cycle",
	.activate = cycle_trig_activate,
	.deactivate = cycle_trig_deactivate,
};

static int __init cycle_trig_init(void)
{
	return led_trigger_register(&cycle_led_trigger);
}

static void __exit cycle_trig_exit(void)
{
	led_trigger_unregister(&cycle_led_trigger);
}

module_init(cycle_trig_init);
module_exit(cycle_trig_exit);

MODULE_AUTHOR("Gaël Portay <g.portay@overkiz.com>");
MODULE_DESCRIPTION("Cycle LED trigger");
MODULE_LICENSE("GPL");
