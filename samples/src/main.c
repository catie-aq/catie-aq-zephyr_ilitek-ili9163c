/*
 * Copyright (c) 2024, CATIE
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <lvgl.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app);

static uint32_t count;

// PWM related variables
static int direction = 1;
static int period = 1000000;
static int increment = 10000;
static int ratio = 0;
static const struct pwm_dt_spec pwm_dev = PWM_DT_SPEC_GET(DT_NODELABEL(backlight_lcd));

int setup_pwm(void)
{
	if (!pwm_is_ready_dt(&pwm_dev)) {
		LOG_ERR("Error: PWM device %s is not ready", pwm_dev.dev->name);
		return -ENODEV;
	}

	int err = pwm_set_dt(&pwm_dev, period, ratio);
	if (err < 0) {
		LOG_ERR("ERROR! [%d]", err);
		return err;
	} else {
		LOG_INF("Set pulse to [%d/1000000]", ratio);
	}

	return 0;
}

void update_pwm(void)
{
	if (ratio >= period) {
		direction = -1;
	} else if (ratio <= 0) {
		direction = 1;
	}

	ratio += direction * increment;
	int err = pwm_set_dt(&pwm_dev, period, ratio);
	if (err < 0) {
		LOG_ERR("ERROR! [%d]", err);
	} else {
		LOG_INF("Set pulse to [%d/1000000]", ratio);
	}
}

int main(void)
{
	char count_str[11] = {0};
	const struct device *display_dev;
	lv_obj_t *hello_world_label;
	lv_obj_t *count_label;

	display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	if (!device_is_ready(display_dev)) {
		LOG_ERR("Device not ready, aborting test");
		return 0;
	}

		hello_world_label = lv_label_create(lv_scr_act());

	lv_label_set_text(hello_world_label, "6TRON BY CATIE!");
	lv_obj_align(hello_world_label, LV_ALIGN_CENTER, 0, 0);

	count_label = lv_label_create(lv_scr_act());
	lv_obj_align(count_label, LV_ALIGN_BOTTOM_MID, 0, 0);

	lv_task_handler();
	display_blanking_off(display_dev);

	// Initialize PWM
	if (setup_pwm() < 0) {
		return 0;
	}

	while (1) {
		if ((count % 100) == 0U) {
			sprintf(count_str, "%d", count / 100U);
			lv_label_set_text(count_label, count_str);
		}
		lv_task_handler();
		++count;
		k_sleep(K_MSEC(10));
		update_pwm();
	}
}
