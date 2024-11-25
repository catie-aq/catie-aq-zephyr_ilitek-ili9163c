/*
 * Copyright (c) 2024, CATIE
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_DISPLAY_ILI9163C_H_
#define ZEPHYR_DRIVERS_DISPLAY_ILI9163C_H_

#include <zephyr/drivers/mipi_dbi.h>
#include <zephyr/sys/util.h>
#include <zephyr/device.h>

#define ILI9163C_PIXEL_FORMAT_RGB565 0U
#define ILI9163C_PIXEL_FORMAT_RGB888 1U

/* Commands/registers. */
#define ILI9163C_SWRESET    0x01
#define ILI9163C_SLPOUT     0x11
#define ILI9163C_DINVON     0x21
#define ILI9163C_GAMSET     0x26
#define ILI9163C_DISPOFF    0x28
#define ILI9163C_DISPON     0x29
#define ILI9163C_CASET      0x2a
#define ILI9163C_PASET      0x2b
#define ILI9163C_RAMWR      0x2c
#define ILI9163C_RGBSET     0x2d
#define ILI9163C_RAMRD      0x2e
#define ILI9163C_PIXSET     0x3A
#define ILI9163C_RAMRD_CONT 0x3e

/* MADCTL register fields. */
#define ILI9163C_MADCTL_MY  BIT(7U)
#define ILI9163C_MADCTL_MX  BIT(6U)
#define ILI9163C_MADCTL_MV  BIT(5U)
#define ILI9163C_MADCTL_ML  BIT(4U)
#define ILI9163C_MADCTL_BGR BIT(3U)
#define ILI9163C_MADCTL_MH  BIT(2U)

/* PIXSET register fields. */
#define ILI9163C_PIXSET_RGB_18_BIT 0x60
#define ILI9163C_PIXSET_RGB_16_BIT 0x50
#define ILI9163C_PIXSET_MCU_18_BIT 0x06
#define ILI9163C_PIXSET_MCU_16_BIT 0x05

/* Commands/registers. */
#define ILI9163C_GAMSET   0x26
#define ILI9163C_FRMCTR1  0xB1
#define ILI9163C_PGAMCTRL 0xE0
#define ILI9163C_NGAMCTRL 0xE1
#define ILI9163C_PWCTRL1  0xC0
#define ILI9163C_PWCTRL2  0xC1
#define ILI9163C_PWCTRL3  0xC2
#define ILI9163C_PWCTRL4  0xC3
#define ILI9163C_VMCTRL1  0xC5
#define ILI9163C_VMCTRL2  0xC7
#define ILI9163C_GAMADJ   0xF2
#define ILI9163C_MADCTL   0x36

/* Commands/registers length. */
#define ILI9163C_GAMSET_LEN   1U
#define ILI9163C_FRMCTR1_LEN  2U
#define ILI9163C_PGAMCTRL_LEN 15U
#define ILI9163C_NGAMCTRL_LEN 15U
#define ILI9163C_PWCTRL1_LEN  2U
#define ILI9163C_PWCTRL2_LEN  1U
#define ILI9163C_PWCTRL3_LEN  1U
#define ILI9163C_PWCTRL4_LEN  1U
#define ILI9163C_VMCTRL1_LEN  2U
#define ILI9163C_VMCTRL2_LEN  1U
#define ILI9163C_GAMADJ_LEN   1U
#define ILI9163C_MADCTL_LEN   1U

/** Command/data GPIO level for commands. */
#define ILI9163C_CMD  1U
/** Command/data GPIO level for data. */
#define ILI9163C_DATA 0U

/** Sleep out time (ms), ref. 8.2.12 of ILI9163C manual. */
#define ILI9163C_SLEEP_OUT_TIME 120

/** Reset pulse time (ms), ref 15.4 of ILI9163C manual. */
#define ILI9163C_RESET_PULSE_TIME 1

/** Reset wait time (ms), ref 15.4 of ILI9163C manual. */
#define ILI9163C_RESET_WAIT_TIME 5

struct ili9163c_config {
	const struct device *mipi_dev;
	struct mipi_dbi_config dbi_config;
	uint8_t pixel_format;
	uint16_t rotation;
	uint16_t x_resolution;
	uint16_t y_resolution;
	bool inversion;
	const void *regs;
	int (*regs_init_fn)(const struct device *dev);
};

/** ILI9163C registers to be initialized. */
struct ili9163c_regs {
	uint8_t gamset[ILI9163C_GAMSET_LEN];
	uint8_t frmctr1[ILI9163C_FRMCTR1_LEN];
	uint8_t pgamctrl[ILI9163C_PGAMCTRL_LEN];
	uint8_t ngamctrl[ILI9163C_NGAMCTRL_LEN];
	uint8_t pwctrl1[ILI9163C_PWCTRL1_LEN];
	uint8_t pwctrl2[ILI9163C_PWCTRL2_LEN];
	uint8_t pwctrl3[ILI9163C_PWCTRL3_LEN];
	uint8_t pwctrl4[ILI9163C_PWCTRL4_LEN];
	uint8_t vmctrl1[ILI9163C_VMCTRL1_LEN];
	uint8_t vmctrl2[ILI9163C_VMCTRL2_LEN];
	uint8_t gamadj[ILI9163C_GAMADJ_LEN];
	uint8_t madctl[ILI9163C_MADCTL_LEN]; /* To be replaced by rotation and inversion computation
					      */
};

/* Initializer macro for ILI9163C registers. */
#define ILI9163C_REGS_INIT(n)                                                                      \
	static const struct ili9163c_regs ili9163c_regs_##n = {                                    \
		.gamset = DT_INST_PROP(n, gamset),                                                 \
		.frmctr1 = DT_INST_PROP(n, frmctr1),                                               \
		.pgamctrl = DT_INST_PROP(n, pgamctrl),                                             \
		.ngamctrl = DT_INST_PROP(n, ngamctrl),                                             \
		.pwctrl1 = DT_INST_PROP(n, pwctrl1),                                               \
		.pwctrl2 = DT_INST_PROP(n, pwctrl2),                                               \
		.pwctrl3 = {DT_INST_ENUM_IDX(n, pwctrl3)},                                         \
		.pwctrl4 = {DT_INST_ENUM_IDX(n, pwctrl4)},                                         \
		.vmctrl1 = DT_INST_PROP(n, vmctrl1),                                               \
		.vmctrl2 = DT_INST_PROP(n, vmctrl2),                                               \
		.gamadj = DT_INST_PROP(n, gamadj),                                                 \
		.madctl = {0x00},                                                                  \
	}

#endif /* ZEPHYR_DRIVERS_DISPLAY_ILI9163C_H_ */
