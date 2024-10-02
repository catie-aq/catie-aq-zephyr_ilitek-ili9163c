/*
 * Copyright (c) 2024, CATIE
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ilitek_ili9163c

#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

#include "display_ili9163c.h"

LOG_MODULE_REGISTER(ILI9163C, CONFIG_SENSOR_LOG_LEVEL);

struct ili9163c_config {
};

struct ili9163c_data {
};

static int ili9163c_attr_set(const struct device *dev, enum sensor_channel chan,
			   enum sensor_attribute attr, const struct sensor_value *val)
{
	return 0;
}

static int ili9163c_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	struct ili9163c_data *data = dev->data;
	const struct ili9163c_config *config = dev->config;

	return 0;
}

static int ili9163c_channel_get(const struct device *dev, enum sensor_channel chan,
			      struct sensor_value *val)
{
	struct ili9163c_data *data = dev->data;

	// TODO: Update val with the sensor value
	val->val1 = 0;
	val->val2 = 0;

	return 0;
}

static int ili9163c_init(const struct device *dev)
{
	const struct ili9163c_config *config = dev->config;
	struct ili9163c_data *data = dev->data;

	return 0;
}

static const struct sensor_driver_api ili9163c_driver_api = {
	.attr_set = ili9163c_attr_set,
	.sample_fetch = ili9163c_sample_fetch,
	.channel_get = ili9163c_channel_get,
};

#define ILI9163C_INIT(n)                                                                             \
	static struct ili9163c_config ili9163c_config_##n = {                                             \
	};                                                                                         \
	static struct ili9163c_data ili9163c_data_##n;                                                 \
	DEVICE_DT_INST_DEFINE(n, ili9163c_init, NULL, &ili9163c_data_##n, &ili9163c_config_##n,          \
			      POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY, &ili9163c_driver_api);

DT_INST_FOREACH_STATUS_OKAY(ILI9163C_INIT)
