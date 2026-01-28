/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef _LCD_EXTERN_H_
#define _LCD_EXTERN_H_
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/lcd/lcd_extern.h>
#include "../lcd_common.h"

#define EXTPR(fmt, args...)     pr_info("lcd extern: " fmt "", ## args)
#define EXTERR(fmt, args...)    pr_info("lcd extern: error: " fmt "", ## args)

#define LCD_EXTERN_DRIVER	"lcd_extern"
extern int lcd_ext_dev_cnt[LCD_MAX_DRV];
extern int lcd_ext_index_lut[LCD_MAX_DRV][LCD_EXTERN_DEV_MAX];
/* common API */
#ifdef CONFIG_USE_OF
struct device_node *aml_lcd_extern_get_dts_child(struct lcd_extern_driver_s *edrv, int index);
#endif

/* common API */
struct lcd_extern_i2c_dev_s *lcd_extern_get_i2c_device(unsigned char addr);
int lcd_extern_i2c_write(struct i2c_client *i2client,
			 unsigned char *buff, unsigned int len);
int lcd_extern_i2c_read(struct i2c_client *i2client,
			unsigned char *wbuf, unsigned int wlen,
			unsigned char *rbuf, unsigned int rlen);
void spi_gpio_init(struct lcd_extern_driver_s *edrv, struct lcd_extern_dev_s *edev);
void spi_gpio_off(struct lcd_extern_driver_s *edrv, struct lcd_extern_dev_s *edev);
int lcd_extern_spi_write(struct lcd_extern_driver_s *edrv, struct lcd_extern_dev_s *edev,
			unsigned char *buf, int len);

void lcd_extern_config_update(struct lcd_extern_driver_s *edrv,
			      struct lcd_extern_dev_s *edev);
int lcd_extern_add_dev(struct lcd_extern_driver_s *edrv, struct lcd_extern_dev_s *edev);
struct lcd_extern_dev_s *lcd_extern_dev_malloc(int dev_index);
void lcd_extern_dev_free(struct lcd_extern_dev_s *edev);
int lcd_ext_check_config_load(struct lcd_extern_driver_s *drv);
int lcd_extern_config_load(struct lcd_extern_driver_s *edrv);
void lcd_extern_gpio_probe(struct lcd_extern_driver_s *edrv, unsigned char index);
void lcd_extern_gpio_unregister(struct lcd_extern_driver_s *edrv, int index);
void lcd_extern_gpio_set(struct lcd_extern_driver_s *edrv, unsigned char index, int value);
unsigned int lcd_extern_gpio_get(struct lcd_extern_driver_s *edrv, unsigned char index);
void lcd_extern_pinmux_set(struct lcd_extern_driver_s *edrv, int status);

void lcd_extern_check_add(struct lcd_extern_driver_s *edrv, struct lcd_extern_dev_s *edev,
		int cmd_step, unsigned char *data_buf, unsigned char data_len);
void lcd_extern_check_handler(struct lcd_extern_driver_s *edrv, struct lcd_extern_dev_s *edev,
		struct i2c_client *i2client, unsigned char cmd_type,
		unsigned char *raw_table, unsigned char data_len);
int lcd_extern_cmd_multi_id(struct lcd_extern_driver_s *edrv, struct lcd_extern_dev_s *edev,
			unsigned char multi_id);
int lcd_extern_cmd_gpio(struct lcd_extern_driver_s *edrv, struct lcd_extern_dev_s *edev,
		int cmd_step, unsigned char *data_buf, unsigned char data_len);
int lcd_extern_cmd_wait_gpio(struct lcd_extern_driver_s *edrv, struct lcd_extern_dev_s *edev,
		int cmd_step, unsigned char *data_buf, unsigned char data_len);
int lcd_extern_cmd_delay(struct lcd_extern_driver_s *edrv, struct lcd_extern_dev_s *edev,
		int cmd_step, unsigned char *data_buf, unsigned char data_len);
int lcd_extern_cmd_i2c(struct lcd_extern_driver_s *edrv, struct lcd_extern_dev_s *edev,
			int cmd_step, unsigned char cmd_type,
			unsigned char *data_buf, unsigned char data_len);
int lcd_extern_power_cmd(struct lcd_extern_driver_s *edrv,
			struct lcd_extern_dev_s *edev, int flag);

/* specific API */
int lcd_extern_default_probe(struct lcd_extern_driver_s *edrv, struct lcd_extern_dev_s *edev);
int lcd_extern_mipi_default_probe(struct lcd_extern_driver_s *edrv, struct lcd_extern_dev_s *edev);

#ifdef CONFIG_AMLOGIC_LCD_EXTERN_I2C_ANX6862_7911
int lcd_extern_i2c_ANX6862_7911_probe(struct lcd_extern_driver_s *edrv,
				      struct lcd_extern_dev_s *edev);
#endif
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_I2C_CS602
int lcd_extern_i2c_CS602_probe(struct lcd_extern_driver_s *edrv,
			       struct lcd_extern_dev_s *edev);
#endif
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_I2C_OLED
int lcd_extern_i2c_oled_probe(struct lcd_extern_driver_s *edrv,
			      struct lcd_extern_dev_s *edev);
#endif

#endif

