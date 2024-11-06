/*
 * Copyright (c) 2024, CATIE
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <stdio.h>
#include <string.h>

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app);

#ifdef CONFIG_ARCH_POSIX
#include "posix_board_if.h"
#endif

// Display variables
enum corner {
    TOP_LEFT,
    TOP_RIGHT,
    BOTTOM_RIGHT,
    BOTTOM_LEFT
};
const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
struct display_capabilities capabilities;

// PWM variables
static bool blinking = false;
static int period = 1000000;
static int ratio = 500000; 
static const struct pwm_dt_spec pwm_dev = PWM_DT_SPEC_GET(DT_NODELABEL(backlight_lcd));

int setup_pwm(void)
{
    if (!pwm_is_ready_dt(&pwm_dev)) {
        LOG_ERR("Error: PWM device %s is not ready", pwm_dev.dev->name);
        return -ENODEV;
    }
    int err;

    err = pwm_set_dt(&pwm_dev, period, ratio);

    if (err < 0) {
        LOG_ERR("ERROR! [%d]", err);
        return err;
    }

    return 0;
}

void update_pwm(void)
{
    int err = pwm_set_dt(&pwm_dev, period, ratio);
    if (err < 0) {
        LOG_ERR("ERROR! [%d]", err);
    } else {
        LOG_INF("Set pulse to [%d/1000000]", ratio);
    }
}

typedef void (*fill_buffer)(enum corner corner, uint8_t grey, uint8_t *buf, size_t buf_size);

#ifdef CONFIG_ARCH_POSIX
static void posix_exit_main(int exit_code)
{
#if CONFIG_TEST
    if (exit_code == 0) {
        LOG_INF("PROJECT EXECUTION SUCCESSFUL");
    } else {
        LOG_INF("PROJECT EXECUTION FAILED");
    }
#endif
    posix_exit(exit_code);
}
#endif

static void fill_buffer_argb8888(enum corner corner, uint8_t grey, uint8_t *buf, size_t buf_size)
{
    uint32_t color = 0;

    switch (corner) {
    case TOP_LEFT:
        color = 0x00F18700u; 
        break;
    case TOP_RIGHT:
        color = 0x0000b0ebu; 
        break;
    case BOTTOM_RIGHT:
        color = 0x00a2c857u; 
        break;
    case BOTTOM_LEFT:
        color = 0x00585757u; 
        break;
    }

    for (size_t idx = 0; idx < buf_size; idx += 4) {
        *((uint32_t *)(buf + idx)) = color;
    }
}

static void fill_buffer_rgb888(enum corner corner, uint8_t grey, uint8_t *buf, size_t buf_size)
{
    uint32_t color = 0;

    switch (corner) {
    case TOP_LEFT:
        color = 0x00F18700u; 
        break;
    case TOP_RIGHT:
        color = 0x0000b0ebu; 
        break;
    case BOTTOM_RIGHT:
        color = 0x00a2c857u; 
        break;
    case BOTTOM_LEFT:
        color = 0x00585757u; 
        break;
    }

    for (size_t idx = 0; idx < buf_size; idx += 3) {
        *(buf + idx + 0) = color >> 16;
        *(buf + idx + 1) = color >> 8;
        *(buf + idx + 2) = color >> 0;
    }
}

static uint16_t get_rgb565_color(enum corner corner, uint8_t grey)
{
    uint32_t color = 0;

    switch (corner) {
    case TOP_LEFT:
        color = 0xF18700u; 
        break;
    case TOP_RIGHT:
        color = 0x00b0ebu; 
        break;
    case BOTTOM_RIGHT:
        color = 0xa2c857u; 
        break;
    case BOTTOM_LEFT:
        color = 0x585757u; 
        break;
    }
    return color;
}

static void fill_buffer_rgb565(enum corner corner, uint8_t grey, uint8_t *buf, size_t buf_size)
{
    uint16_t color = get_rgb565_color(corner, grey);

    for (size_t idx = 0; idx < buf_size; idx += 2) {
        *(buf + idx + 0) = (color >> 8) & 0xFFu;
        *(buf + idx + 1) = (color >> 0) & 0xFFu;
    }
}

static void fill_buffer_bgr565(enum corner corner, uint8_t grey, uint8_t *buf, size_t buf_size)
{
    uint16_t color = get_rgb565_color(corner, grey);

    for (size_t idx = 0; idx < buf_size; idx += 2) {
        *(uint16_t *)(buf + idx) = color;
    }
}

static void fill_buffer_mono(enum corner corner, uint8_t grey, uint8_t black, uint8_t white, uint8_t *buf, size_t buf_size)
{
    uint16_t color;

    switch (corner) {
    case BOTTOM_LEFT:
        color = (grey & 0x01u) ? white : black;
        break;
    default:
        color = black;
        break;
    }

    memset(buf, color, buf_size);
}

static inline void fill_buffer_mono01(enum corner corner, uint8_t grey, uint8_t *buf, size_t buf_size)
{
    fill_buffer_mono(corner, grey, 0x00u, 0xFFu, buf, buf_size);
}

static inline void fill_buffer_mono10(enum corner corner, uint8_t grey, uint8_t *buf, size_t buf_size)
{
    fill_buffer_mono(corner, grey, 0xFFu, 0x00u, buf, buf_size);
}

static int initialize_display(const struct device **display_dev, struct display_capabilities *capabilities)
{
    if (!device_is_ready(*display_dev)) {
        LOG_ERR("Device not ready, aborting test");
        return -ENODEV;
    }

    display_get_capabilities(*display_dev, capabilities);
    display_blanking_off(*display_dev);

    return 0;
}

static int allocate_buffer(uint8_t **buf, size_t buf_size)
{
    *buf = k_malloc(buf_size);
    if (*buf == NULL) {
        LOG_ERR("Could not allocate memory. Aborting sample.");
        return -ENOMEM;
    }

    return 0;
}

static void fill_display(const struct device *display_dev, struct display_capabilities *capabilities, uint8_t *buf, size_t buf_size, fill_buffer fill_buffer_fnc)
{
    struct display_buffer_descriptor buf_desc;
    size_t rect_w, rect_h, h_step, scale;
    uint8_t bg_color;
    size_t x, y, grey_count;
    int32_t grey_scale_sleep;

    if (capabilities->screen_info & SCREEN_INFO_MONO_VTILED) {
        rect_w = 16;
        rect_h = 8;
    } else {
        rect_w = 2;
        rect_h = 1;
    }

    if ((capabilities->x_resolution < 3 * rect_w) ||
        (capabilities->y_resolution < 3 * rect_h) ||
        (capabilities->x_resolution < 8 * rect_h)) {
        rect_w = capabilities->x_resolution * 40 / 100;
        rect_h = capabilities->y_resolution * 40 / 100;
        h_step = capabilities->y_resolution * 20 / 100;
        scale = 1;
    } else {
        h_step = rect_h;
        scale = (capabilities->x_resolution / 8) / rect_h;
    }

    rect_w *= scale;
    rect_h *= scale;

    if (capabilities->screen_info & SCREEN_INFO_EPD) {
        grey_scale_sleep = 10000;
    } else {
        grey_scale_sleep = 100;
    }

    buf_size = rect_w * rect_h;

    if (buf_size < (capabilities->x_resolution * h_step)) {
        buf_size = capabilities->x_resolution * h_step;
    }

    switch (capabilities->current_pixel_format) {
    case PIXEL_FORMAT_ARGB_8888:
        bg_color = 0xFFu;
        fill_buffer_fnc = fill_buffer_argb8888;
        buf_size *= 4;
        break;
    case PIXEL_FORMAT_RGB_888:
        bg_color = 0xFFu;
        fill_buffer_fnc = fill_buffer_rgb888;
        buf_size *= 3;
        break;
    case PIXEL_FORMAT_RGB_565:
        bg_color = 0xFFu;
        fill_buffer_fnc = fill_buffer_rgb565;
        buf_size *= 2;
        break;
    case PIXEL_FORMAT_BGR_565:
        bg_color = 0xFFu;
        fill_buffer_fnc = fill_buffer_bgr565;
        buf_size *= 2;
        break;
    case PIXEL_FORMAT_MONO01:
        bg_color = 0xFFu;
        fill_buffer_fnc = fill_buffer_mono01;
        buf_size = DIV_ROUND_UP(DIV_ROUND_UP(buf_size, NUM_BITS(uint8_t)), sizeof(uint8_t));
        break;
    case PIXEL_FORMAT_MONO10:
        bg_color = 0x00u;
        fill_buffer_fnc = fill_buffer_mono10;
        buf_size = DIV_ROUND_UP(DIV_ROUND_UP(buf_size, NUM_BITS(uint8_t)), sizeof(uint8_t));
        break;
    default:
        LOG_ERR("Unsupported pixel format. Aborting sample.");
#ifdef CONFIG_ARCH_POSIX
        posix_exit_main(1);
#else
        return;
#endif
    }

    (void)memset(buf, bg_color, buf_size);

    buf_desc.buf_size = buf_size;
    buf_desc.pitch = capabilities->x_resolution;
    buf_desc.width = capabilities->x_resolution;
    buf_desc.height = h_step;

    for (int idx = 0; idx < capabilities->y_resolution; idx += h_step) {
        if ((capabilities->y_resolution - idx) < h_step) {
            buf_desc.height = (capabilities->y_resolution - idx);
        }
        display_write(display_dev, 0, idx, &buf_desc, buf);
    }

    buf_desc.pitch = rect_w;
    buf_desc.width = rect_w;
    buf_desc.height = rect_h;

    fill_buffer_fnc(TOP_LEFT, 0, buf, buf_size);
    x = 0;
    y = 0;
    display_write(display_dev, x, y, &buf_desc, buf);

    fill_buffer_fnc(TOP_RIGHT, 0, buf, buf_size);
    x = capabilities->x_resolution - rect_w;
    y = 0;
    display_write(display_dev, x, y, &buf_desc, buf);

    fill_buffer_fnc(BOTTOM_RIGHT, 0, buf, buf_size);
    x = capabilities->x_resolution - rect_w;
    y = capabilities->y_resolution - rect_h;
    display_write(display_dev, x, y, &buf_desc, buf);

    display_blanking_off(display_dev);

    grey_count = 0;
    x = 0;
    y = capabilities->y_resolution - rect_h;

    while (1) {
        fill_buffer_fnc(BOTTOM_LEFT, grey_count, buf, buf_size);
        display_write(display_dev, x, y, &buf_desc, buf);
        ++grey_count;
        k_msleep(grey_scale_sleep);
#if CONFIG_TEST
        if (grey_count >= 1024) {
            break;
        }
#endif
    }
}

int main(void)
{
    uint8_t *buf;
    size_t buf_size = 0;
    fill_buffer fill_buffer_fnc = NULL;

	if (!pwm_is_ready_dt(&pwm_dev)) {
		LOG_ERR("Error: PWM device %s is not ready", pwm_dev.dev->name);
		return -ENODEV;
	}

    if (initialize_display(&display_dev, &capabilities) < 0) {
        return 0;
    }

    LOG_INF("Display sample for %s", display_dev->name);
    display_get_capabilities(display_dev, &capabilities);

    if (setup_pwm() < 0) {
        return 0;
    }

    if (allocate_buffer(&buf, buf_size) < 0) {
        return 0;
    }

    fill_display(display_dev, &capabilities, buf, buf_size, fill_buffer_fnc);

    while (1) {
        if (blinking) {
            update_pwm();
        }

        k_sleep(K_MSEC(10));
    }

#ifdef CONFIG_ARCH_POSIX
    posix_exit_main(0);
#endif
    return 0;
}