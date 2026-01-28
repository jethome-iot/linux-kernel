// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/amlogic/media/vout/lcd/lcd_extern.h>
#include "lcd_extern.h"

#define LCD_EXTERN_NAME           "ext_default"
#define LCD_EXTERN_TYPE           LCD_EXTERN_MAX

static int lcd_extern_reg_read(struct lcd_extern_driver_s *edrv, struct lcd_extern_dev_s *edev,
			       unsigned char reg_byte_len,
			       unsigned short reg, unsigned char *buf)
{
	struct lcd_extern_i2c_dev_s *i2c_dev;
	unsigned char tmp;
	int ret = 0;

	tmp = reg;
	switch (edev->config.type) {
	case LCD_EXTERN_I2C:
		if (edev->addr_sel >= 4) {
			EXTERR("[%d]: %s: dev_%d invalid addr_sel %d\n",
			       edrv->index, __func__, edev->dev_index, edev->addr_sel);
			return -1;
		}
		i2c_dev = edev->i2c_dev[edev->addr_sel];
		if (!i2c_dev) {
			EXTERR("[%d]: %s: dev_%d i2c[%d] device is null\n",
			       edrv->index, __func__, edev->dev_index, edev->addr_sel);
			return -1;
		}
		lcd_extern_i2c_read(i2c_dev->client, &tmp, 1, &tmp, 1);
		buf[0] = tmp;
		break;
	case LCD_EXTERN_SPI:
		EXTPR("[%d]: %s: not support\n", edrv->index, __func__);
		break;
	default:
		break;
	}

	return ret;
}

static int lcd_extern_reg_write(struct lcd_extern_driver_s *edrv, struct lcd_extern_dev_s *edev,
			       unsigned char *buf, unsigned int len)
{
	struct lcd_extern_i2c_dev_s *i2c_dev;
	int ret = 0;

	switch (edev->config.type) {
	case LCD_EXTERN_I2C:
		if (edev->addr_sel >= 4) {
			EXTERR("[%d]: %s: dev_%d invalid addr_sel %d\n",
			       edrv->index, __func__, edev->dev_index, edev->addr_sel);
			return -1;
		}
		i2c_dev = edev->i2c_dev[edev->addr_sel];
		if (!i2c_dev) {
			EXTERR("[%d]: %s: dev_%d i2c[%d] device is null\n",
			       edrv->index, __func__, edev->dev_index, edev->addr_sel);
			return -1;
		}
		ret = lcd_extern_i2c_write(i2c_dev->client, buf, len);
		break;
	case LCD_EXTERN_SPI:
		ret = lcd_extern_spi_write(edrv, edev, buf, 2);
		break;
	default:
		break;
	}

	return ret;
}

static int lcd_extern_power_ctrl(struct lcd_extern_driver_s *edrv,
				 struct lcd_extern_dev_s *edev, int flag)
{
	int ret = 0;

	if (edev->config.type == LCD_EXTERN_SPI)
		spi_gpio_init(edrv, edev);

	ret = lcd_extern_power_cmd(edrv, edev, flag);

	if (edev->config.type == LCD_EXTERN_SPI)
		spi_gpio_off(edrv, edev);

	EXTPR("[%d]: %s: %s(%d): %d\n",
	      edrv->index, __func__,
	      edev->config.name, edev->config.index, flag);
	return ret;
}

static int lcd_extern_power_on(struct lcd_extern_driver_s *edrv,
			       struct lcd_extern_dev_s *edev)
{
	int ret;

	mutex_lock(&edrv->power_mutex);
	lcd_extern_pinmux_set(edrv, 1);
	ret = lcd_extern_power_ctrl(edrv, edev, 1);
	edev->state = 1;
	mutex_unlock(&edrv->power_mutex);
	return ret;
}

static int lcd_extern_power_off(struct lcd_extern_driver_s *edrv,
				struct lcd_extern_dev_s *edev)
{
	int ret;

	mutex_lock(&edrv->power_mutex);
	edev->state = 0;
	ret = lcd_extern_power_ctrl(edrv, edev, 0);
	lcd_extern_pinmux_set(edrv, 0);
	mutex_unlock(&edrv->power_mutex);
	return ret;
}

static int lcd_extern_driver_update(struct lcd_extern_driver_s *edrv,
				    struct lcd_extern_dev_s *edev)
{
	if (edev->config.table_init_loaded == 0) {
		EXTERR("[%d]: dev_%d(%s): tablet_init is invalid\n",
		       edrv->index, edev->dev_index, edev->config.name);
		return -1;
	}

	if (edev->config.type == LCD_EXTERN_SPI)
		edev->config.spi_delay_us = 1000 / edev->config.spi_clk_freq;

	edev->reg_read  = lcd_extern_reg_read;
	edev->reg_write = lcd_extern_reg_write;
	edev->power_on  = lcd_extern_power_on;
	edev->power_off = lcd_extern_power_off;

	return 0;
}

int lcd_extern_default_probe(struct lcd_extern_driver_s *edrv, struct lcd_extern_dev_s *edev)
{
	int ret = 0;

	if (!edrv || !edev) {
		EXTERR("%s: dev is null\n", __func__);
		return -1;
	}

	if (edev->config.cmd_size < 2) {
		EXTERR("[%d]: %s: %s(%d): cmd_size %d is invalid\n",
			edrv->index, __func__,
			edev->config.name,
			edev->dev_index,
			edev->config.cmd_size);
		return -1;
	}

	switch (edev->config.type) {
	case LCD_EXTERN_I2C:
		if (edev->config.i2c_addr < LCD_EXT_I2C_ADDR_INVALID) {
			edev->i2c_dev[0] = lcd_extern_get_i2c_device(edev->config.i2c_addr);
			if (!edev->i2c_dev[0]) {
				EXTERR("[%d]: %s: dev_%d invalid i2c0 device\n",
				       edrv->index, __func__, edev->dev_index);
				return -1;
			}
			EXTPR("[%d]: %s: dev_%d get i2c0 device: %s, addr 0x%02x OK\n",
			      edrv->index, __func__, edev->dev_index,
			      edev->i2c_dev[0]->name, edev->i2c_dev[0]->client->addr);
		}
		if (edev->config.i2c_addr2 < LCD_EXT_I2C_ADDR_INVALID) {
			edev->i2c_dev[1] = lcd_extern_get_i2c_device(edev->config.i2c_addr2);
			if (!edev->i2c_dev[1]) {
				EXTERR("[%d]: %s: dev_%d invalid i2c1 device\n",
				       edrv->index, __func__, edev->dev_index);
				edev->i2c_dev[0] = NULL;
				return -1;
			}
			EXTPR("[%d]: %s: dev_%d get i2c1 device: %s, addr 0x%02x OK\n",
			      edrv->index, __func__, edev->dev_index,
			      edev->i2c_dev[1]->name, edev->i2c_dev[1]->client->addr);
		}
		if (edev->config.i2c_addr3 < LCD_EXT_I2C_ADDR_INVALID) {
			edev->i2c_dev[2] = lcd_extern_get_i2c_device(edev->config.i2c_addr3);
			if (!edev->i2c_dev[2]) {
				EXTERR("[%d]: %s: dev_%d invalid i2c2 device\n",
				       edrv->index, __func__, edev->dev_index);
				edev->i2c_dev[0] = NULL;
				edev->i2c_dev[1] = NULL;
				return -1;
			}
			EXTPR("[%d]: %s: dev_%d get i2c2 device: %s, addr 0x%02x OK\n",
			      edrv->index, __func__, edev->dev_index,
			      edev->i2c_dev[2]->name, edev->i2c_dev[2]->client->addr);
		}
		if (edev->config.i2c_addr4 < LCD_EXT_I2C_ADDR_INVALID) {
			edev->i2c_dev[3] = lcd_extern_get_i2c_device(edev->config.i2c_addr4);
			if (!edev->i2c_dev[3]) {
				EXTERR("[%d]: %s: dev_%d invalid i2c3 device\n",
				       edrv->index, __func__, edev->dev_index);
				edev->i2c_dev[0] = NULL;
				edev->i2c_dev[1] = NULL;
				edev->i2c_dev[2] = NULL;
				return -1;
			}
			EXTPR("[%d]: %s: dev_%d get i2c3 device: %s, addr 0x%02x OK\n",
			      edrv->index, __func__, edev->dev_index,
			      edev->i2c_dev[3]->name, edev->i2c_dev[3]->client->addr);
		}
		break;
	case LCD_EXTERN_SPI:
		break;
	default:
		break;
	}

	ret = lcd_extern_driver_update(edrv, edev);
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		EXTPR("[%d]: %s: dev_%d %d\n",
		      edrv->index, __func__, edev->dev_index, ret);
	}
	return ret;
}
