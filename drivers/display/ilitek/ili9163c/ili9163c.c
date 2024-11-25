/*
 * Copyright (c) 2024, CATIE
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ilitek_ili9163c

#include "ili9163c.h"

#include <zephyr/drivers/display.h>
#include <zephyr/sys/byteorder.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ILI9163C, CONFIG_DISPLAY_LOG_LEVEL);

struct ili9163c_data {
	uint8_t bytes_per_pixel;
	enum display_pixel_format pixel_format;
	enum display_orientation orientation;
};

int ili9163c_transmit(const struct device *dev, uint8_t cmd, const void *tx_data, size_t tx_len)
{
	const struct ili9163c_config *config = dev->config;

	return mipi_dbi_command_write(config->mipi_dev, &config->dbi_config, cmd, tx_data, tx_len);
}

static int ili9163c_exit_sleep(const struct device *dev)
{
	int r;

	r = ili9163c_transmit(dev, ILI9163C_SLPOUT, NULL, 0);
	if (r < 0) {
		return r;
	}

	k_sleep(K_MSEC(ILI9163C_SLEEP_OUT_TIME));

	return 0;
}

static void ili9163c_hw_reset(const struct device *dev)
{
	const struct ili9163c_config *config = dev->config;

	if (mipi_dbi_reset(config->mipi_dev, ILI9163C_RESET_PULSE_TIME) < 0) {
		return;
	};
	k_sleep(K_MSEC(ILI9163C_RESET_WAIT_TIME));
}

static int ili9163c_set_mem_area(const struct device *dev, const uint16_t x, const uint16_t y,
				 const uint16_t w, const uint16_t h)
{
	int r;
	uint16_t spi_data[2];

	spi_data[0] = sys_cpu_to_be16(x);
	spi_data[1] = sys_cpu_to_be16(x + w - 1U);
	r = ili9163c_transmit(dev, ILI9163C_CASET, &spi_data[0], 4U);
	if (r < 0) {
		return r;
	}

	spi_data[0] = sys_cpu_to_be16(y);
	spi_data[1] = sys_cpu_to_be16(y + h - 1U);
	r = ili9163c_transmit(dev, ILI9163C_PASET, &spi_data[0], 4U);
	if (r < 0) {
		return r;
	}

	return 0;
}

static int ili9163c_write(const struct device *dev, const uint16_t x, const uint16_t y,
			  const struct display_buffer_descriptor *desc, const void *buf)
{
	const struct ili9163c_config *config = dev->config;
	struct ili9163c_data *data = dev->data;
	struct display_buffer_descriptor mipi_desc;

	int r;
	const uint8_t *write_data_start = (const uint8_t *)buf;
	uint16_t write_cnt;
	uint16_t nbr_of_writes;
	uint16_t write_h;

	__ASSERT(desc->width <= desc->pitch, "Pitch is smaller than width");
	__ASSERT((desc->pitch * data->bytes_per_pixel * desc->height) <= desc->buf_size,
		 "Input buffer to small");

	LOG_DBG("Writing %dx%d (w,h) @ %dx%d (x,y)", desc->width, desc->height, x, y);
	r = ili9163c_set_mem_area(dev, x, y, desc->width, desc->height);
	if (r < 0) {
		return r;
	}

	if (desc->pitch > desc->width) {
		write_h = 1U;
		nbr_of_writes = desc->height;
		mipi_desc.height = 1;
		mipi_desc.buf_size = desc->pitch * data->bytes_per_pixel;
	} else {
		write_h = desc->height;
		mipi_desc.height = desc->height;
		mipi_desc.buf_size = desc->width * data->bytes_per_pixel * write_h;
		nbr_of_writes = 1U;
	}

	mipi_desc.width = desc->width;
	mipi_desc.pitch = desc->width;

	r = ili9163c_transmit(dev, ILI9163C_RAMWR, NULL, 0);
	if (r < 0) {
		return r;
	}

	for (write_cnt = 0U; write_cnt < nbr_of_writes; ++write_cnt) {
		r = mipi_dbi_write_display(config->mipi_dev, &config->dbi_config, write_data_start,
					   &mipi_desc, data->pixel_format);
		if (r < 0) {
			return r;
		}

		write_data_start += desc->pitch * data->bytes_per_pixel;
	}

	return 0;
}

static int ili9163c_display_blanking_off(const struct device *dev)
{
	LOG_DBG("Turning display blanking off");
	return ili9163c_transmit(dev, ILI9163C_DISPON, NULL, 0);
}

static int ili9163c_display_blanking_on(const struct device *dev)
{
	LOG_DBG("Turning display blanking on");
	return ili9163c_transmit(dev, ILI9163C_DISPOFF, NULL, 0);
}

static int ili9163c_set_pixel_format(const struct device *dev,
				     const enum display_pixel_format pixel_format)
{
	struct ili9163c_data *data = dev->data;

	int r;
	uint8_t tx_data;
	uint8_t bytes_per_pixel;

	if (pixel_format == PIXEL_FORMAT_RGB_565) {
		bytes_per_pixel = 2U;
		tx_data = ILI9163C_PIXSET_RGB_16_BIT | ILI9163C_PIXSET_MCU_16_BIT;
	} else if (pixel_format == PIXEL_FORMAT_RGB_888) {
		bytes_per_pixel = 3U;
		tx_data = ILI9163C_PIXSET_RGB_18_BIT | ILI9163C_PIXSET_MCU_18_BIT;
	} else {
		LOG_ERR("Unsupported pixel format");
		return -ENOTSUP;
	}

	r = ili9163c_transmit(dev, ILI9163C_PIXSET, &tx_data, 1U);
	if (r < 0) {
		return r;
	}

	data->pixel_format = pixel_format;
	data->bytes_per_pixel = bytes_per_pixel;

	return 0;
}

static int ili9163c_set_orientation(const struct device *dev,
				    const enum display_orientation orientation)
{
	const struct ili9163c_config *config = dev->config;
	struct ili9163c_data *data = dev->data;

	int r;
	uint8_t tx_data = ILI9163C_MADCTL_BGR;
	if (config->quirks->cmd_set == CMD_SET_1) {
		if (orientation == DISPLAY_ORIENTATION_NORMAL) {
			tx_data |= ILI9163C_MADCTL_MX;
		} else if (orientation == DISPLAY_ORIENTATION_ROTATED_90) {
			tx_data |= ILI9163C_MADCTL_MV;
		} else if (orientation == DISPLAY_ORIENTATION_ROTATED_180) {
			tx_data |= ILI9163C_MADCTL_MY;
		} else if (orientation == DISPLAY_ORIENTATION_ROTATED_270) {
			tx_data |= ILI9163C_MADCTL_MV | ILI9163C_MADCTL_MX | ILI9163C_MADCTL_MY;
		}
	} else if (config->quirks->cmd_set == CMD_SET_2) {
		if (orientation == DISPLAY_ORIENTATION_NORMAL) {
			/* Do nothing */
		} else if (orientation == DISPLAY_ORIENTATION_ROTATED_90) {
			tx_data |= ILI9163C_MADCTL_MV | ILI9163C_MADCTL_MY;
		} else if (orientation == DISPLAY_ORIENTATION_ROTATED_180) {
			tx_data |= ILI9163C_MADCTL_MY | ILI9163C_MADCTL_MX;
		} else if (orientation == DISPLAY_ORIENTATION_ROTATED_270) {
			tx_data |= ILI9163C_MADCTL_MV | ILI9163C_MADCTL_MX;
		}
	}

	r = ili9163c_transmit(dev, ILI9163C_MADCTL, &tx_data, 1U);
	if (r < 0) {
		return r;
	}

	data->orientation = orientation;

	return 0;
}

static void ili9163c_get_capabilities(const struct device *dev,
				      struct display_capabilities *capabilities)
{
	struct ili9163c_data *data = dev->data;
	const struct ili9163c_config *config = dev->config;

	memset(capabilities, 0, sizeof(struct display_capabilities));

	capabilities->supported_pixel_formats = PIXEL_FORMAT_RGB_565 | PIXEL_FORMAT_RGB_888;
	capabilities->current_pixel_format = data->pixel_format;

	if (data->orientation == DISPLAY_ORIENTATION_NORMAL ||
	    data->orientation == DISPLAY_ORIENTATION_ROTATED_180) {
		capabilities->x_resolution = config->x_resolution;
		capabilities->y_resolution = config->y_resolution;
	} else {
		capabilities->x_resolution = config->y_resolution;
		capabilities->y_resolution = config->x_resolution;
	}

	capabilities->current_orientation = data->orientation;
}

static int ili9163c_configure(const struct device *dev)
{
	const struct ili9163c_config *config = dev->config;

	int r;
	enum display_pixel_format pixel_format;
	enum display_orientation orientation;

	r = config->regs_init_fn(dev);
	if (r < 0) {
		return r;
	}

	if (config->pixel_format == ILI9163C_PIXEL_FORMAT_RGB565) {
		pixel_format = PIXEL_FORMAT_RGB_565;
	} else {
		pixel_format = PIXEL_FORMAT_RGB_888;
	}

	r = ili9163c_set_pixel_format(dev, pixel_format);
	if (r < 0) {
		return r;
	}

	if (config->rotation == 0U) {
		orientation = DISPLAY_ORIENTATION_NORMAL;
	} else if (config->rotation == 90U) {
		orientation = DISPLAY_ORIENTATION_ROTATED_90;
	} else if (config->rotation == 180U) {
		orientation = DISPLAY_ORIENTATION_ROTATED_180;
	} else {
		orientation = DISPLAY_ORIENTATION_ROTATED_270;
	}

	r = ili9163c_set_orientation(dev, orientation);
	if (r < 0) {
		return r;
	}

	if (config->inversion) {
		r = ili9163c_transmit(dev, ILI9163C_DINVON, NULL, 0U);
		if (r < 0) {
			return r;
		}
	}

	return 0;
}

int ili9163c_regs_init(const struct device *dev)
{
	const struct ili9163c_config *config = dev->config;
	const struct ili9163c_regs *regs = config->regs;

	int r;

	LOG_HEXDUMP_DBG(regs->gamset, ILI9163C_GAMSET_LEN, "GAMSET");
	r = ili9163c_transmit(dev, ILI9163C_GAMSET, regs->gamset, ILI9163C_GAMSET_LEN);
	if (r < 0) {
		return r;
	}

	LOG_HEXDUMP_DBG(regs->gamadj, ILI9163C_GAMADJ_LEN, "GAMADJ");
	r = ili9163c_transmit(dev, ILI9163C_GAMADJ, regs->gamadj, ILI9163C_GAMADJ_LEN);
	if (r < 0) {
		return r;
	}

	LOG_HEXDUMP_DBG(regs->pgamctrl, ILI9163C_PGAMCTRL_LEN, "PGAMCTRL");
	r = ili9163c_transmit(dev, ILI9163C_PGAMCTRL, regs->pgamctrl, ILI9163C_PGAMCTRL_LEN);
	if (r < 0) {
		return r;
	}

	LOG_HEXDUMP_DBG(regs->ngamctrl, ILI9163C_NGAMCTRL_LEN, "NGAMCTRL");
	r = ili9163c_transmit(dev, ILI9163C_NGAMCTRL, regs->ngamctrl, ILI9163C_NGAMCTRL_LEN);
	if (r < 0) {
		return r;
	}

	LOG_HEXDUMP_DBG(regs->frmctr1, ILI9163C_FRMCTR1_LEN, "FRMCTR1");
	r = ili9163c_transmit(dev, ILI9163C_FRMCTR1, regs->frmctr1, ILI9163C_FRMCTR1_LEN);
	if (r < 0) {
		return r;
	}

	LOG_HEXDUMP_DBG(regs->pwctrl1, ILI9163C_PWCTRL1_LEN, "PWCTRL1");
	r = ili9163c_transmit(dev, ILI9163C_PWCTRL1, regs->pwctrl1, ILI9163C_PWCTRL1_LEN);
	if (r < 0) {
		return r;
	}

	LOG_HEXDUMP_DBG(regs->pwctrl2, ILI9163C_PWCTRL2_LEN, "PWCTRL2");
	r = ili9163c_transmit(dev, ILI9163C_PWCTRL2, regs->pwctrl2, ILI9163C_PWCTRL2_LEN);
	if (r < 0) {
		return r;
	}

	LOG_HEXDUMP_DBG(regs->pwctrl3, ILI9163C_PWCTRL3_LEN, "PWCTRL3");
	r = ili9163c_transmit(dev, ILI9163C_PWCTRL3, regs->pwctrl3, ILI9163C_PWCTRL3_LEN);
	if (r < 0) {
		return r;
	}

	LOG_HEXDUMP_DBG(regs->pwctrl4, ILI9163C_PWCTRL4_LEN, "PWCTRL4");
	r = ili9163c_transmit(dev, ILI9163C_PWCTRL4, regs->pwctrl4, ILI9163C_PWCTRL4_LEN);
	if (r < 0) {
		return r;
	}

	LOG_HEXDUMP_DBG(regs->vmctrl1, ILI9163C_VMCTRL1_LEN, "VMCTRL1");
	r = ili9163c_transmit(dev, ILI9163C_VMCTRL1, regs->vmctrl1, ILI9163C_VMCTRL1_LEN);
	if (r < 0) {
		return r;
	}

	LOG_HEXDUMP_DBG(regs->vmctrl2, ILI9163C_VMCTRL2_LEN, "VMCTRL2");
	r = ili9163c_transmit(dev, ILI9163C_VMCTRL2, regs->vmctrl2, ILI9163C_VMCTRL2_LEN);
	if (r < 0) {
		return r;
	}

	LOG_HEXDUMP_DBG(regs->madctl, ILI9163C_MADCTL_LEN, "MADCTL");
	r = ili9163c_transmit(dev, ILI9163C_MADCTL, regs->madctl, ILI9163C_MADCTL_LEN);
	if (r < 0) {
		return r;
	}

	return 0;
}

static int ili9163c_init(const struct device *dev)
{
	const struct ili9163c_config *config = dev->config;

	int r;

	if (!device_is_ready(config->mipi_dev)) {
		LOG_ERR("MIPI DBI device is not ready");
		return -ENODEV;
	}

	ili9163c_hw_reset(dev);

	r = ili9163c_transmit(dev, ILI9163C_SWRESET, NULL, 0);
	if (r < 0) {
		LOG_ERR("Error transmit command Software Reset (%d)", r);
		return r;
	}

#ifdef CONFIG_ILI9163C_READ
	ili9163c_transmit(dev, ILI9163C_RGBSET, ili9163c_rgb_lut, sizeof(ili9163c_rgb_lut));
#endif

	k_sleep(K_MSEC(ILI9163C_RESET_WAIT_TIME));

	ili9163c_display_blanking_on(dev);

	r = ili9163c_configure(dev);
	if (r < 0) {
		LOG_ERR("Could not configure display (%d)", r);
		return r;
	}

	r = ili9163c_exit_sleep(dev);
	if (r < 0) {
		LOG_ERR("Could not exit sleep mode (%d)", r);
		return r;
	}

	return 0;
}

static const struct display_driver_api ili9163c_api = {
	.blanking_on = ili9163c_display_blanking_on,
	.blanking_off = ili9163c_display_blanking_off,
	.write = ili9163c_write,
#ifdef CONFIG_ILI9163C_READ
	.read = ili9163c_read,
#endif
	.get_capabilities = ili9163c_get_capabilities,
	.set_pixel_format = ili9163c_set_pixel_format,
	.set_orientation = ili9163c_set_orientation,
};

static const struct ili9163c_quirks ili9163c_quirks = {
	.cmd_set = CMD_SET_1,
};

#define INST_DT_ILI9163C(n) DT_INST(n, ilitek_ili9163c)

#define ILI9163C_INIT(n)                                                                           \
	ILI9163C_REGS_INIT(n);                                                                     \
                                                                                                   \
	static const struct ili9163c_config ili9163c_config_##n = {                                \
		.quirks = &ili9163c_quirks,                                                        \
		.mipi_dev = DEVICE_DT_GET(DT_PARENT(DT_INST(n, DT_DRV_COMPAT))),                   \
		.dbi_config =                                                                      \
			{                                                                          \
				.mode = MIPI_DBI_MODE_SPI_4WIRE,                                   \
				.config = MIPI_DBI_SPI_CONFIG_DT_INST(                             \
					n, SPI_OP_MODE_MASTER | SPI_WORD_SET(8), 0),               \
			},                                                                         \
		.pixel_format = DT_INST_PROP(n, pixel_format),                                     \
		.rotation = DT_INST_PROP(n, rotation),                                             \
		.x_resolution = DT_INST_PROP(n, width),                                            \
		.y_resolution = DT_INST_PROP(n, height),                                           \
		.inversion = DT_INST_PROP(n, display_inversion),                                   \
		.regs = &ili9163c_regs_##n,                                                        \
		.regs_init_fn = ili9163c_regs_init,                                                \
	};                                                                                         \
                                                                                                   \
	static struct ili9163c_data ili9163c_data_##n;                                             \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(n, ili9163c_init, NULL, &ili9163c_data_##n, &ili9163c_config_##n,    \
			      POST_KERNEL, CONFIG_DISPLAY_INIT_PRIORITY, &ili9163c_api);

DT_INST_FOREACH_STATUS_OKAY(ILI9163C_INIT);
