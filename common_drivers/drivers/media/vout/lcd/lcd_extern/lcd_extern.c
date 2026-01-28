// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/compat.h>
#include <linux/of.h>
#include <linux/amlogic/media/vout/lcd/lcd_extern.h>
#include <linux/amlogic/media/vout/lcd/lcd_unifykey.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#include "lcd_extern.h"

#include <linux/amlogic/gki_module.h>

#define EXT_CDEV_NAME "lcd_ext"
struct ext_cdev_s {
	dev_t           devno;
	struct class    *class;
};

static struct ext_cdev_s *ext_cdev;
/* for driver global resource init:
 *  0: none
 *  n: initialized cnt
 */
static unsigned char ext_global_init_flag;
static unsigned int ext_drv_init_state;

int lcd_ext_dev_cnt[LCD_MAX_DRV];
int lcd_ext_index_lut[LCD_MAX_DRV][LCD_EXTERN_DEV_MAX];
static struct lcd_extern_driver_s *ext_driver[LCD_MAX_DRV];

struct lcd_extern_driver_s *lcd_extern_get_driver(int drv_index)
{
	if (drv_index >= LCD_MAX_DRV)
		return NULL;

	return ext_driver[drv_index];
}

struct lcd_extern_dev_s *lcd_extern_get_dev(struct lcd_extern_driver_s *edrv, int dev_index)
{
	int i = 0;

	if (!edrv)
		return NULL;
	if (dev_index >= LCD_EXTERN_INDEX_INVALID)
		return NULL;

	for (i = 0; i < edrv->dev_cnt; i++) {
		if (!edrv->dev[i])
			break;

		if (lcd_debug_print_flag & LCD_DBG_PR_ADV) {
			EXTPR("%s: dev[%d]: name: %s, dev_index:%d, get dev_index:%d\n",
				__func__, i,
				edrv->dev[i]->config.name,
				edrv->dev[i]->dev_index,
				dev_index);
		}
		if (edrv->dev[i]->dev_index == dev_index)
			return edrv->dev[i];
	}

	EXTERR("[%d]: invalid dev_index: %d\n", edrv->index, dev_index);
	return NULL;
}

int lcd_extern_dev_index_add(int drv_index, int dev_index)
{
	int dev_cnt, i;

	if (drv_index >= LCD_MAX_DRV) {
		EXTERR("%s: invalid drv_index: %d\n", __func__, drv_index);
		return -1;
	}
	if (dev_index == 0xff)
		return 0;

	dev_cnt = lcd_ext_dev_cnt[drv_index];
	if (dev_cnt >= LCD_EXTERN_DEV_MAX) {
		EXTERR("[%d]: %s: out off dev_cnt support\n", drv_index, __func__);
		return -1;
	}

	for (i = 0; i < LCD_EXTERN_DEV_MAX; i++) {
		if (lcd_ext_index_lut[drv_index][i] == dev_index) {
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				EXTPR("[%d]: %s: dev_index %d already exist\n",
				      drv_index, __func__, dev_index);
			}
			return 0;
		}
	}

	lcd_ext_index_lut[drv_index][dev_cnt] = dev_index;
	lcd_ext_dev_cnt[drv_index]++;
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		EXTPR("[%d]: %s: dev_index: %d, dev_cnt: %d\n",
		      drv_index, __func__, dev_index,
		      lcd_ext_dev_cnt[drv_index]);
	}
	return 0;
}

int lcd_extern_dev_index_remove(int drv_index, int dev_index)
{
	int find, i;

	if (drv_index >= LCD_MAX_DRV) {
		EXTERR("%s: invalid drv_index: %d\n", __func__, dev_index);
		return -1;
	}
	if (dev_index == 0xff)
		return 0;

	if (lcd_ext_dev_cnt[drv_index] == 0)
		return -1;

	find = 0xff;
	for (i = 0; i < LCD_EXTERN_DEV_MAX; i++) {
		if (lcd_ext_index_lut[drv_index][i] == dev_index)
			find = i;
	}
	if (find == 0xff)
		return 0;

	lcd_ext_index_lut[drv_index][find] = LCD_EXTERN_INDEX_INVALID;
	for (i = (find + 1); i < LCD_EXTERN_DEV_MAX; i++) {
		if (lcd_ext_index_lut[drv_index][i] == LCD_EXTERN_INDEX_INVALID)
			break;
		lcd_ext_index_lut[drv_index][i - 1] = lcd_ext_index_lut[drv_index][i];
		lcd_ext_index_lut[drv_index][i] = LCD_EXTERN_INDEX_INVALID;
	}
	if (lcd_ext_dev_cnt[drv_index])
		lcd_ext_dev_cnt[drv_index]--;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		EXTPR("[%d]: %s: dev_index: %d\n", drv_index, __func__, dev_index);
	return 0;
}

int lcd_extern_init(void)
{
	int i, j;

	for (i = 0; i < LCD_MAX_DRV; i++) {
		for (j = 0; j < LCD_EXTERN_DEV_MAX; j++)
			lcd_ext_index_lut[i][j] = LCD_EXTERN_INDEX_INVALID;
		lcd_ext_dev_cnt[i] = 0;
	}

	return 0;
}

static void lcd_extern_multi_list_add(struct lcd_extern_dev_s *edev,
		unsigned int index, unsigned int type,
		unsigned char data_len, unsigned char *data_buf)
{
	struct lcd_extern_multi_list_s *temp_list;
	struct lcd_extern_multi_list_s *cur_list;

	/* creat list */
	cur_list = kzalloc(sizeof(*cur_list), GFP_KERNEL);
	if (!cur_list)
		return;
	cur_list->index = index;
	cur_list->type = type;
	cur_list->data_len = data_len;
	cur_list->data_buf = kzalloc(data_len, GFP_KERNEL);
	if (!cur_list->data_buf) {
		kfree(cur_list);
		return;
	}
	memcpy(cur_list->data_buf, data_buf, data_len);

	if (!edev->multi_list_header) {
		edev->multi_list_header = cur_list;
	} else {
		temp_list = edev->multi_list_header;
		while (temp_list->next) {
			if (temp_list->index == cur_list->index) {
				EXTERR("%s: dev_%d: index=%d(type=%d) already in list\n",
					__func__, edev->dev_index,
					cur_list->index, cur_list->type);
				kfree(cur_list->data_buf);
				kfree(cur_list);
				return;
			}
			temp_list = temp_list->next;
		}
		temp_list->next = cur_list;
	}

	EXTPR("%s: dev_%d: index=%d, type=%d\n",
	       __func__, edev->dev_index, cur_list->index, cur_list->type);
}

static int lcd_extern_multi_list_remove(struct lcd_extern_dev_s *edev)
{
	struct lcd_extern_multi_list_s *cur_list;
	struct lcd_extern_multi_list_s *next_list;

	/* add to exist list */
	cur_list = edev->multi_list_header;
	while (cur_list) {
		next_list = cur_list->next;
		kfree(cur_list->data_buf);
		kfree(cur_list);
		cur_list = next_list;
	}
	edev->multi_list_header = NULL;

	return 0;
}

static void lcd_extern_config_update_dynamic_size(struct lcd_extern_driver_s *edrv,
						  struct lcd_extern_dev_s *edev, int flag)
{
	unsigned char type, next_cmd, size, *table;
	unsigned int max_len = 0, i = 0, j, index;

	if (flag) {
		max_len = edev->config.table_init_on_cnt;
		table = edev->config.table_init_on;
	} else {
		max_len = edev->config.table_init_off_cnt;
		table = edev->config.table_init_off;
	}

	while ((i + 1) < max_len) {
		type = table[i];
		size = table[i + 1];
		if (type == LCD_EXT_CMD_TYPE_END)
			break;
		if (size == 0)
			goto lcd_extern_config_update_dynamic_size_next;
		if ((i + 2 + size) > max_len)
			break;

		if (type == LCD_EXT_CMD_TYPE_GPIO ||
		    type == LCD_EXT_CMD_TYPE_WAIT_GPIO) {
			if (size >= 2) {
				/* gpio probe */
				index = table[i + 2];
				lcd_extern_gpio_probe(edrv, index);
			}
		} else if (type == LCD_EXT_CMD_TYPE_MULTI_CMD ||
			   type == LCD_EXT_CMD_TYPE_MULTI_DFT_CMD) {
			next_cmd = table[i + 3];
			if (next_cmd == LCD_EXT_CMD_TYPE_GPIO ||
			    next_cmd == LCD_EXT_CMD_TYPE_WAIT_GPIO) {
				if (size >= 3) {
					/* gpio probe */
					index = table[i + 4];
					lcd_extern_gpio_probe(edrv, index);
				}
			}
		} else if (type == LCD_EXT_CMD_TYPE_MULTI_LIST_FR) {
			for (j = 0; j < size; j += 3) {
				index = i + 2 + j;
				lcd_extern_multi_list_add(edev, table[index],
					type, 2, &table[index + 1]);
			}
		} else if (type == LCD_EXT_CMD_TYPE_MULTI_LIST_UFR) {
			for (j = 0; j < size; j += 5) {
				index = i + 2 + j;
				lcd_extern_multi_list_add(edev, table[index],
					type, 4, &table[index + 1]);
			}
		}
lcd_extern_config_update_dynamic_size_next:
		i += (size + 2);
	}
}

static void lcd_extern_config_update_fixed_size(struct lcd_extern_driver_s *edrv,
						struct lcd_extern_dev_s *edev, int flag)
{
	int i = 0, max_len = 0;
	unsigned char type, cmd_size, index;
	unsigned char *table;

	cmd_size = edev->config.cmd_size;
	if (cmd_size < 2) {
		EXTERR("[%d]: %s: dev_%d invalid cmd_size %d\n",
		       edrv->index, __func__, edev->dev_index, cmd_size);
		return;
	}

	if (flag) {
		max_len = edev->config.table_init_on_cnt;
		table = edev->config.table_init_on;
	} else {
		max_len = edev->config.table_init_off_cnt;
		table = edev->config.table_init_off;
	}

	while ((i + cmd_size) <= max_len) {
		type = table[i];
		if (type == LCD_EXT_CMD_TYPE_END)
			break;
		if (type == LCD_EXT_CMD_TYPE_GPIO) {
			/* gpio probe */
			index = table[i + 1];
			lcd_extern_gpio_probe(edrv, index);
		}
		i += cmd_size;
	}
}

void lcd_extern_config_update(struct lcd_extern_driver_s *edrv,
			      struct lcd_extern_dev_s *edev)
{
	if (edev->config.cmd_size == 0)
		return;

	if (edev->config.cmd_size == LCD_EXT_CMD_SIZE_DYNAMIC) {
		lcd_extern_config_update_dynamic_size(edrv, edev, 1);
		lcd_extern_config_update_dynamic_size(edrv, edev, 0);
	} else {
		lcd_extern_config_update_fixed_size(edrv, edev, 1);
		lcd_extern_config_update_fixed_size(edrv, edev, 0);
	}
}

struct lcd_extern_dev_s *lcd_extern_dev_malloc(int dev_index)
{
	struct lcd_extern_dev_s *edev;

	edev = kzalloc(sizeof(*edev), GFP_KERNEL);
	if (!edev)
		return NULL;

	edev->dev_index = dev_index;
	edev->config.index = LCD_EXTERN_INDEX_INVALID;
	edev->config.type = LCD_EXTERN_MAX;
	edev->config.i2c_addr = LCD_EXT_I2C_ADDR_INVALID;
	edev->config.i2c_addr2 = LCD_EXT_I2C_ADDR_INVALID;
	edev->config.i2c_addr3 = LCD_EXT_I2C_ADDR_INVALID;
	edev->config.i2c_addr4 = LCD_EXT_I2C_ADDR_INVALID;

	edev->config.spi_gpio_cs = LCD_EXT_GPIO_INVALID;
	edev->config.spi_gpio_clk = LCD_EXT_GPIO_INVALID;
	edev->config.spi_gpio_data = LCD_EXT_GPIO_INVALID;
	edev->config.spi_clk_pol = 1;

	return edev;
}

void lcd_extern_dev_free(struct lcd_extern_dev_s *edev)
{
	if (!edev)
		return;

	lcd_extern_multi_list_remove(edev);

	kfree(edev->config.table_init_on);
	edev->config.table_init_on = NULL;

	kfree(edev->config.table_init_off);
	edev->config.table_init_off = NULL;

	kfree(edev);
}

int lcd_extern_add_dev(struct lcd_extern_driver_s *edrv, struct lcd_extern_dev_s *edev)
{
	struct aml_lcd_drv_s *pdrv;
	int ret = -1;

	if (strcmp(edev->config.name, "ext_default") == 0) {
		if (edev->config.type == LCD_EXTERN_MIPI)
			ret = lcd_extern_mipi_default_probe(edrv, edev);
		else
			ret = lcd_extern_default_probe(edrv, edev);
	} else if (strcmp(edev->config.name, "mipi_default") == 0) {
		ret = lcd_extern_mipi_default_probe(edrv, edev);
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_I2C_CS602
	} else if (strcmp(edev->config.name, "i2c_CS602") == 0) {
		ret = lcd_extern_i2c_CS602_probe(edrv, edev);
#endif
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_I2C_ANX6862_7911
	} else if (strcmp(edev->config.name, "i2c_ANX6862_7911") == 0) {
		ret = lcd_extern_i2c_ANX6862_7911_probe(edrv, edev);
#endif
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_I2C_OLED
	} else if (strcmp(edev->config.name, "i2c_oled") == 0) {
		ret = lcd_extern_i2c_oled_probe(edrv, edev);
#endif
	} else {
		EXTERR("[%d]: %s: invalid dev: %s(%d)\n",
		       edrv->index, __func__,
		       edev->config.name, edev->dev_index);
	}

	if (ret) {
		EXTERR("[%d]: %s: %s(%d) failed\n",
		       edrv->index, __func__,
		       edev->config.name, edev->dev_index);
		return -1;
	}

	pdrv = aml_lcd_get_driver(edrv->index);
	if (pdrv && (pdrv->status & LCD_STATUS_IF_ON)) {
		if (edev->init)
			edev->init(edrv, edev);
		edev->state = 1;
	}

	EXTPR("[%d]: %s: %s(%d) ok\n",
	      edrv->index, __func__,
	      edev->config.name, edev->dev_index);
	return 0;
}

/* *********************************************************
 * debug function
 * *********************************************************
 */
static int lcd_extern_init_dynamic_print(char *buf, struct lcd_extern_config_s *econf, int flag)
{
	int i, j, max_len;
	unsigned char type, size;
	unsigned char *table;
	int len = 0;

	if (flag) {
		len = sprintf(buf, "power on:\n");
		table = econf->table_init_on;
		max_len = econf->table_init_on_cnt;
	} else {
		len = sprintf(buf, "power off:\n");
		table = econf->table_init_off;
		max_len = econf->table_init_off_cnt;
	}
	if (!table) {
		len += sprintf(buf + len, "init_table %d is NULL\n", flag);
		return len;
	}

	i = 0;
	switch (econf->type) {
	case LCD_EXTERN_I2C:
	case LCD_EXTERN_SPI:
	case LCD_EXTERN_SIMPLE:
		while ((i + 1) < max_len) {
			type = table[i];
			size = table[i + 1];
			if (type == LCD_EXT_CMD_TYPE_END) {
				len += sprintf(buf + len, "  0x%02x,%d,\n", type, size);
				break;
			}

			len += sprintf(buf + len, "  0x%02x,%d,", type, size);
			if (size == 0)
				goto init_table_dynamic_print_i2c_spi_next;
			if (i + 2 + size > max_len) {
				len += sprintf(buf + len, "size out of support\n");
				break;
			}

			for (j = 0; j < size; j++)
				len += sprintf(buf + len, "0x%02x,", table[i + 2 + j]);

init_table_dynamic_print_i2c_spi_next:
			len += sprintf(buf + len, "\n");
			i += (size + 2);
		}
		break;
	case LCD_EXTERN_MIPI:
		while ((i + 1) < max_len) {
			type = table[i];
			size = table[i + 1];
			if (type == LCD_EXT_CMD_TYPE_END) {
				if (size == 0xff) {
					len += sprintf(buf + len, "  0x%02x,0x%02x,\n",
						type, size);
					break;
				}
				if (size == 0) {
					len += sprintf(buf + len, "  0x%02x,%d,\n", type, size);
					break;
				}
				size = 0;
			}

			len += sprintf(buf + len, "  0x%02x,%d,", type, size);
			if (size == 0)
				goto init_table_dynamic_print_mipi_next;
			if (i + 2 + size > max_len) {
				len += sprintf(buf + len, "size out of support\n");
				break;
			}

			if (type == LCD_EXT_CMD_TYPE_GPIO ||
			    type == LCD_EXT_CMD_TYPE_DELAY) {
				for (j = 0; j < size; j++)
					len += sprintf(buf + len, "%d,", table[i + 2 + j]);
			} else if ((type & 0xf) == 0x0) {
				len += sprintf(buf + len, "  init_%s wrong data_type: 0x%02x\n",
					       flag ? "on" : "off", type);
				break;
			} else {
				size = table[i + DSI_CMD_SIZE_INDEX];
				len += sprintf(buf + len, "  0x%02x,%d,", type, size);
				for (j = 0; j < size; j++)
					len += sprintf(buf + len, "0x%02x,", table[i + 2 + j]);
			}

init_table_dynamic_print_mipi_next:
			len += sprintf(buf + len, "\n");
			i += (size + 2);
		}
		break;
	default:
		break;
	}

	return len;
}

static int lcd_extern_init_fixed_print(char *buf, struct lcd_extern_config_s *econf, int flag)
{
	int i, j, max_len;
	unsigned char cmd_size;
	unsigned char *table;
	int len = 0;

	cmd_size = econf->cmd_size;
	if (flag) {
		len = sprintf(buf, "power on:\n");
		table = econf->table_init_on;
		max_len = econf->table_init_on_cnt;
	} else {
		len = sprintf(buf, "power off:\n");
		table = econf->table_init_off;
		max_len = econf->table_init_off_cnt;
	}
	if (!table) {
		len += sprintf(buf + len, "init_table %d is NULL\n", flag);
		return len;
	}

	i = 0;
	while ((i + cmd_size) <= max_len) {
		len += sprintf(buf + len, " ");
		for (j = 0; j < cmd_size; j++)
			len += sprintf(buf + len, " 0x%02x", table[i + j]);
		len += sprintf(buf + len, "\n");

		if (table[i] == LCD_EXT_CMD_TYPE_END)
			break;
		i += cmd_size;
	}

	return len;
}

static int lcd_extern_multi_list_print(char *buf, struct lcd_extern_dev_s *edev)
{
	struct lcd_extern_multi_list_s *temp_list;
	unsigned char *data_buf;
	int len = 0, i;

	if (!edev->multi_list_header) {
		len = sprintf(buf, "multi_list: NULL\n");
		return len;
	}

	temp_list = edev->multi_list_header;
	while (temp_list) {
		len += sprintf(buf + len, "multi_list[%d]:\n", temp_list->index);
		len += sprintf(buf + len, "  type: 0x%x\n", temp_list->type);
		len += sprintf(buf + len, "  data:");
		data_buf = temp_list->data_buf;
		if (temp_list->type == LCD_EXT_CMD_TYPE_MULTI_LIST_UFR) {
			for (i = 0; i < temp_list->data_len; i += 2) {
				len += sprintf(buf + len, " %d",
					data_buf[i] | (data_buf[i + 1] << 8));
			}
		} else {
			for (i = 0; i < temp_list->data_len; i++)
				len += sprintf(buf + len, " %d", data_buf[i]);
		}
		len += sprintf(buf + len, "\n");
		temp_list = temp_list->next;
	}

	return len;
}

static ssize_t lcd_extern_info_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct lcd_extern_driver_s *edrv = dev_get_drvdata(dev);
	struct lcd_extern_dev_s *edev;
	ssize_t len = 0;
	int i = 0;

	len = sprintf(buf, "lcd extern driver[%d] info:\n", edrv->index);
	for (i = 0; i < edrv->dev_cnt; i++) {
		edev = edrv->dev[i];
		if (!edev)
			continue;

		len += sprintf(buf + len, "dev[%d]: %s\n",
			       edev->dev_index, edev->config.name);
		len += sprintf(buf + len, "status:             %d\n", edev->config.status);
		switch (edev->config.type) {
		case LCD_EXTERN_I2C:
			len += sprintf(buf + len,
				"type:               i2c(%d)\n"
				"i2c_addr:           0x%02x\n"
				"i2c_addr2:          0x%02x\n"
				"i2c_addr3:          0x%02x\n"
				"i2c_addr4:          0x%02x\n"
				"i2c_bus:            %d\n"
				"table_loaded:       %d\n"
				"cmd_size:           %d\n"
				"table_init_on_cnt:  %d\n"
				"table_init_off_cnt: %d\n",
				edev->config.type,
				edev->config.i2c_addr, edev->config.i2c_addr2,
				edev->config.i2c_addr3, edev->config.i2c_addr4,
				edrv->i2c_bus,
				edev->config.table_init_loaded, edev->config.cmd_size,
				edev->config.table_init_on_cnt,
				edev->config.table_init_off_cnt);
			if (edev->config.cmd_size == 0)
				break;
			if (edev->config.cmd_size == LCD_EXT_CMD_SIZE_DYNAMIC) {
				len += lcd_extern_init_dynamic_print(buf + len, &edev->config, 1);
				len += lcd_extern_init_dynamic_print(buf + len, &edev->config, 0);
			} else {
				len += lcd_extern_init_fixed_print(buf + len, &edev->config, 1);
				len += lcd_extern_init_fixed_print(buf + len, &edev->config, 0);
			}
			len += lcd_extern_multi_list_print(buf + len, edev);
			break;
		case LCD_EXTERN_SPI:
			len += sprintf(buf + len,
				"type:               spi(%d)\n"
				"spi_gpio_cs:        %d\n"
				"spi_gpio_clk:       %d\n"
				"spi_gpio_data:      %d\n"
				"spi_clk_freq:       %dKHz\n"
				"spi_delay_us:       %d\n"
				"spi_clk_pol:        %d\n"
				"table_loaded:       %d\n"
				"cmd_size:           %d\n"
				"table_init_on_cnt:  %d\n"
				"table_init_off_cnt: %d\n",
				edev->config.type,
				edev->config.spi_gpio_cs, edev->config.spi_gpio_clk,
				edev->config.spi_gpio_data, edev->config.spi_clk_freq,
				edev->config.spi_delay_us, edev->config.spi_clk_pol,
				edev->config.table_init_loaded, edev->config.cmd_size,
				edev->config.table_init_on_cnt,
				edev->config.table_init_off_cnt);
			if (edev->config.cmd_size == 0)
				break;
			if (edev->config.cmd_size == LCD_EXT_CMD_SIZE_DYNAMIC) {
				len += lcd_extern_init_dynamic_print(buf + len, &edev->config, 1);
				len += lcd_extern_init_dynamic_print(buf + len, &edev->config, 0);
			} else {
				len += lcd_extern_init_fixed_print(buf + len, &edev->config, 1);
				len += lcd_extern_init_fixed_print(buf + len, &edev->config, 0);
			}
			break;
		case LCD_EXTERN_MIPI:
			len += sprintf(buf + len,
				"type:            mipi(%d)\n"
				"table_loaded:    %d\n"
				"cmd_size:        %d\n"
				"table_init_on_cnt:  %d\n"
				"table_init_off_cnt: %d\n",
				edev->config.type,
				edev->config.table_init_loaded,
				edev->config.cmd_size,
				edev->config.table_init_on_cnt,
				edev->config.table_init_off_cnt);
			if (edev->config.cmd_size != LCD_EXT_CMD_SIZE_DYNAMIC)
				break;
			len += lcd_extern_init_dynamic_print(buf + len, &edev->config, 1);
			len += lcd_extern_init_dynamic_print(buf + len, &edev->config, 0);
			break;
		case LCD_EXTERN_SIMPLE:
			len += sprintf(buf + len,
				"type:               simple(%d)\n"
				"table_loaded:       %d\n"
				"cmd_size:           %d\n"
				"table_init_on_cnt:  %d\n"
				"table_init_off_cnt: %d\n",
				edev->config.type,
				edev->config.table_init_loaded, edev->config.cmd_size,
				edev->config.table_init_on_cnt,
				edev->config.table_init_off_cnt);
			if (edev->config.cmd_size == 0)
				break;
			if (edev->config.cmd_size == LCD_EXT_CMD_SIZE_DYNAMIC) {
				len += lcd_extern_init_dynamic_print(buf + len, &edev->config, 1);
				len += lcd_extern_init_dynamic_print(buf + len, &edev->config, 0);
			} else {
				len += lcd_extern_init_fixed_print(buf + len, &edev->config, 1);
				len += lcd_extern_init_fixed_print(buf + len, &edev->config, 0);
			}
			len += lcd_extern_multi_list_print(buf + len, edev);
			break;
		default:
			len += sprintf(buf + len, "invalid extern_type\n");
			break;
		}

		if (edrv->pinmux_valid) {
			len += sprintf(buf + len,
				"pinmux_flag:     %d\n"
				"pinmux_pointer:  0x%p\n",
				edrv->pinmux_flag, edrv->pin);
		}
	}

	return len;
}

static const char *lcd_extern_debug_usage_str = {
"Usage:\n"
"    echo test <on/off> > debug ; test power on/off for extern device\n"
"        <on/off>: 1 for power on, 0 for power off\n"
"    echo r <addr_sel> <reg> > debug ; read reg for extern device\n"
"    echo d <addr_sel> <reg> <cnt> > debug ; dump regs for extern device\n"
"    echo w <addr_sel> <reg> <value> > debug ; write reg for extern device\n"
};

static ssize_t lcd_extern_debug_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", lcd_extern_debug_usage_str);
}

static ssize_t lcd_extern_debug_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct lcd_extern_driver_s *edrv = dev_get_drvdata(dev);
	struct lcd_extern_dev_s *edev;
	unsigned int ret, j;
	unsigned int val[3];
	unsigned short reg;
	unsigned char value, reg_buf[2];
	unsigned int index = LCD_EXTERN_INDEX_INVALID;

	switch (buf[0]) {
	case 't':
		ret = sscanf(buf, "test %d %d", &index, &val[0]);
		if (ret == 2) {
			edev = lcd_extern_get_dev(edrv, index);
			if (!edev)
				return -EINVAL;

			if (val[0]) {
				if (edev->power_on)
					edev->power_on(edrv, edev);
			} else {
				if (edev->power_off)
					edev->power_off(edrv, edev);
			}
		} else {
			pr_info("invalid data\n");
			return -EINVAL;
		}
		break;
	case 'r':
		ret = sscanf(buf, "r %d %d %x", &index, &val[0], &val[1]);
		if (ret == 3) {
			edev = lcd_extern_get_dev(edrv, index);
			if (!edev)
				return -EINVAL;
			edev->addr_sel = (unsigned char)val[0];
			reg = (unsigned short)val[1];
			if (edev->reg_read) {
				edev->reg_read(edrv, edev, 1, reg, &value);
				pr_info("reg read: 0x%02x = 0x%02x\n", reg, value);
			}
		} else {
			pr_info("invalid data\n");
			return -EINVAL;
		}
		break;
	case 'd':
		ret = sscanf(buf, "d %d %d %x %d", &index, &val[0], &val[1], &val[2]);
		if (ret == 4) {
			edev = lcd_extern_get_dev(edrv, index);
			if (!edev)
				return -EINVAL;
			edev->addr_sel = (unsigned char)val[0];
			reg = (unsigned short)val[1];
			if (edev->reg_read) {
				pr_info("reg dump:\n");
				for (j = 0; j < val[2]; j++) {
					edev->reg_read(edrv, edev, 1, reg + j, &value);
					pr_info("  0x%02x = 0x%02x\n", reg, value);
				}
			}
		} else {
			pr_info("invalid data\n");
			return -EINVAL;
		}
		break;
	case 'w':
		ret = sscanf(buf, "w %d %d %x %x", &index, &val[0],
			     &val[1], &val[2]);
		if (ret == 4) {
			edev = lcd_extern_get_dev(edrv, index);
			if (!edev)
				return -EINVAL;
			edev->addr_sel = (unsigned char)val[0];
			reg = (unsigned short)val[1];
			value = (unsigned char)val[2];
			if (edev->reg_write) {
				reg_buf[0] = (unsigned char)val[1];
				reg_buf[1] = (unsigned char)val[2];
				edev->reg_write(edrv, edev, reg_buf, 2);
				if (edev->reg_read) {
					edev->reg_read(edrv, edev, 1, reg, &value);
					pr_info("reg write 0x%02x = 0x%02x, readback: 0x%02x\n",
						reg, val[2], value);
				} else {
					pr_info("reg write 0x%02x = 0x%02x\n", reg, value);
				}
			}
		} else {
			pr_info("invalid data\n");
			return -EINVAL;
		}
		break;
	default:
		pr_info("invalid data\n");
		break;
	}

	return count;
}

static struct device_attribute lcd_extern_debug_attrs[] = {
	__ATTR(info, 0444, lcd_extern_info_show, NULL),
	__ATTR(debug, 0644, lcd_extern_debug_show, lcd_extern_debug_store),
};

static int lcd_extern_debug_file_creat(struct lcd_extern_driver_s *edrv)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(lcd_extern_debug_attrs); i++) {
		if (device_create_file(edrv->sub_dev, &lcd_extern_debug_attrs[i])) {
			EXTERR("[%d]: create debug attribute %s fail\n",
			       edrv->index, lcd_extern_debug_attrs[i].attr.name);
		}
	}

	return 0;
}

static int lcd_extern_debug_file_remove(struct lcd_extern_driver_s *edrv)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(lcd_extern_debug_attrs); i++)
		device_remove_file(edrv->sub_dev, &lcd_extern_debug_attrs[i]);

	return 0;
}

/* ************************************************************* */
static int ext_bin_data_update(int type, unsigned char *bin_buf, unsigned int bin_size,
			unsigned char *buf, unsigned int offset, unsigned int data_len)
{
	if (!buf) {
		EXTERR("%s, buf is null\n", __func__);
		return -1;
	}
	buf[0] = 0; /* init invalid data */

	switch (type) {
	case LCD_EXT_CMD_TYPE_CMD_BIN_DATA: /* all data replace, reg_addr nonexistent */
		buf[0] = bin_size;
		memcpy(&buf[1], bin_buf, bin_size);
		EXTPR("%s, bin_data, size=%d\n", __func__, buf[0]);
		break;
	case LCD_EXT_CMD_TYPE_CMD_BIN: /* data with reg_addr auto fill 0x0 */
		buf[0] = (bin_size + 1); /* data size include reg_addr */
		buf[1] = 0x00;            /* reg_addr */
		memcpy(&buf[2], bin_buf, bin_size);
		EXTPR("%s, bin, size=%d, reg=0x%x\n", __func__, buf[0], buf[1]);
		break;
	case LCD_EXT_CMD_TYPE_CMD_BIN2: /* data with reg_addr, only replace i2c_data */
		if (bin_size < (offset + data_len - 1)) {
			EXTERR("%s, offset size %d out of bin_size(%d)!\n",
			      __func__, (offset + data_len - 1), bin_size);
			return -1;
		}

		buf[0] = data_len;
		buf[1] = offset;
		memcpy(&buf[2], &bin_buf[offset], data_len - 1);
		EXTPR("%s, bin2, size=%d, reg=0x%x\n", __func__, buf[0], buf[1]);
		break;
	default:
		break;
	}

	return 0;
}

static int ext_cmd_bin_buf_load(unsigned int i2c_index, unsigned int target_multi_id,
			unsigned char type,
			unsigned char *bin_buf, unsigned int bin_size,
			unsigned char *raw_buf, unsigned int raw_size,
			unsigned char *tmp_buf, unsigned int tmp_size)
{
	unsigned char next_type, multi_flag;
	unsigned int index, multi_id = 0;
	unsigned int offset = 0, data_len = 0, pr_len;
	char *pr_buf;
	int k, n, ret = -1;

	switch (type) {
	case LCD_EXT_CMD_TYPE_MULTI_CMD:
	case LCD_EXT_CMD_TYPE_MULTI_DFT_CMD:
		multi_flag = 1;
		multi_id = raw_buf[0];
		next_type = raw_buf[1];
		if (multi_id != target_multi_id)
			return -1;

		data_len = raw_size - 2; //id,next_cmd
		offset = raw_buf[2];
		n = 2;
		break;
	case LCD_EXT_CMD_TYPE_CMD_MULTI:
	case LCD_EXT_CMD_TYPE_CMD2_MULTI:
	case LCD_EXT_CMD_TYPE_CMD3_MULTI:
	case LCD_EXT_CMD_TYPE_CMD4_MULTI:
		multi_flag = 1;
		multi_id = raw_buf[0];
		next_type = ((raw_buf[1] << 4) | (type & 0xf));
		if (multi_id != target_multi_id)
			return -1;

		data_len = raw_size - 2; //id,next_cmd
		offset = raw_buf[2];
		n = 2;
		break;
	default:
		multi_flag = 0;
		next_type = type;
		data_len = raw_size;
		offset = raw_buf[0];
		n = 0;
	}

	switch (next_type & 0xf0) {
	case LCD_EXT_CMD_TYPE_CMD_BIN2:
	case LCD_EXT_CMD_TYPE_CMD_BIN:
	case LCD_EXT_CMD_TYPE_CMD_BIN_DATA:
		break;
	default:
		return -1;
	}

	index = next_type & 0xf;
	if (index != i2c_index)
		return -1;
	EXTPR("%s: next_type=0x%x, i2c_index=%d, multi_id=%d, multi_flag=%d\n",
		__func__, next_type, index, multi_id, multi_flag);
	if (data_len > tmp_size) {
		EXTERR("%s, data_len %d over size!\n", __func__, data_len);
		return -1;
	}
	memset(tmp_buf, 0, tmp_size);
	pr_buf = kzalloc(4096, GFP_KERNEL);
	if (!pr_buf)
		return -1;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		EXTPR("%s, raw_size=%d, data_len=%d, raw_buf(data_offset=%d):\n",
			__func__, raw_size, data_len, n);
		pr_len = 0;
		for (k = 0; k < raw_size; k++)
			pr_len += sprintf(pr_buf + pr_len, " 0x%02x,", raw_buf[k]);
		pr_info("%s\n", pr_buf);
		EXTPR("%s, bin_size=%d, bin_data:\n", __func__, bin_size);
		pr_len = 0;
		for (k = 0; k < bin_size; k++)
			pr_len += sprintf(pr_buf + pr_len, " 0x%02x,", bin_buf[k]);
		pr_info("%s\n", pr_buf);
	}

	//save multi_flag for cmd data different replace method
	tmp_buf[0] = multi_flag;
	ret = ext_bin_data_update(next_type, bin_buf, bin_size, &tmp_buf[1], offset, data_len);
	if (ret || tmp_buf[1] == 0) { /* bin data size invalid */
		kfree(pr_buf);
		return -1;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		EXTPR("%s, multi_flag=%d, temp_size=%d, temp_data:\n",
			__func__, tmp_buf[0], tmp_buf[1]);
		pr_len = 0;
		for (k = 0; k < tmp_buf[1]; k++)
			pr_len += sprintf(pr_buf + pr_len, " 0x%02x,", tmp_buf[k + 2]);
		pr_info("%s\n", pr_buf);
	}

	kfree(pr_buf);
	return 0;
}

static int ext_reload_cmd_bin(struct lcd_extern_driver_s *edrv, struct lcd_extern_dev_s *edev,
			unsigned int i2c_index, unsigned int multi_id,
			unsigned char *bin_buf, unsigned int bin_size)
{
	unsigned int i = 0, j = 0, pre_cnt;
	unsigned char type, raw_size, data_size, data_len, multi_flag, step = 0;
	unsigned char *tmp_buf, *raw_table, *new_table;
	int ret;

	if (edev->config.cmd_size != LCD_EXT_CMD_SIZE_DYNAMIC)
		return -1;
	if (bin_size > LCD_EXTERN_INIT_ON_MAX) {
		EXTERR("[%d]: %s: invalid bin_size %d\n", edrv->index, __func__, bin_size);
		return -1;
	}

	mutex_lock(&edrv->power_mutex);
	new_table = kzalloc(LCD_EXTERN_INIT_ON_MAX, GFP_KERNEL);
	if (!new_table) {
		mutex_unlock(&edrv->power_mutex);
		return -1;
	}
	tmp_buf = kzalloc(LCD_EXTERN_INIT_ON_MAX, GFP_KERNEL);
	if (!tmp_buf) {
		kfree(new_table);
		mutex_unlock(&edrv->power_mutex);
		return -1;
	}

	pre_cnt = edev->config.table_init_on_cnt;
	raw_table = edev->config.table_init_on;

	while (i < pre_cnt) {
		type = raw_table[i];
		raw_size = raw_table[i + 1];
		new_table[j] = type;
		if (type == LCD_EXT_CMD_TYPE_END) {
			new_table[j + 1] = 0;
			i += 2;
			j += 2;
			break;
		}

		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			EXTPR("%s: step %d\n", __func__, step);
		ret = ext_cmd_bin_buf_load(i2c_index, multi_id, type,
			bin_buf, bin_size, &raw_table[i + 2], raw_size,
			tmp_buf, LCD_EXTERN_INIT_ON_MAX);
		if (ret == 0) {
			//buf[0]:multi_flag
			//buf[1]:data_len
			//buf[2..]:data
			multi_flag = tmp_buf[0];
			data_len = tmp_buf[1];
			if (multi_flag) {
				data_size = data_len + 2;
				new_table[j + 1] = data_size;
				new_table[j + 2] = raw_table[i + 2];
				new_table[j + 3] = raw_table[i + 3];
				memcpy(&new_table[j + 4], &tmp_buf[2], data_len);
			} else {
				data_size = data_len;
				new_table[j + 1] = data_size;
				memcpy(&new_table[j + 2], &tmp_buf[2], data_size);
			}
		} else {
			/* original ini data */
			data_size = raw_size;
			new_table[j + 1] = data_size;
			memcpy(&new_table[j + 2], &raw_table[i + 2], data_size);
		}

		j += data_size + 2;
		i += raw_size + 2; /* raw data */
		step++;
	}
	edev->config.table_init_on_cnt = j;

	kfree(edev->config.table_init_on);
	edev->config.table_init_on = kzalloc(edev->config.table_init_on_cnt, GFP_KERNEL);
	if (!edev->config.table_init_on) {
		kfree(new_table);
		kfree(tmp_buf);
		mutex_unlock(&edrv->power_mutex);
		return -1;
	}
	memcpy(edev->config.table_init_on, new_table, edev->config.table_init_on_cnt);

	kfree(new_table);
	kfree(tmp_buf);
	mutex_unlock(&edrv->power_mutex);

	return 0;
}

static int ext_io_open(struct inode *inode, struct file *file)
{
	struct lcd_extern_driver_s *edrv;

	edrv = container_of(inode->i_cdev, struct lcd_extern_driver_s, cdev);
	file->private_data = edrv;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		EXTPR("%s\n", __func__);

	return 0;
}

static int ext_io_release(struct inode *inode, struct file *file)
{
	if (!file->private_data)
		return 0;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		EXTPR("%s\n", __func__);
	file->private_data = NULL;
	return 0;
}

static long ext_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *argp;
	int mcd_nr = -1;
	struct lcd_extern_driver_s *edrv = (struct lcd_extern_driver_s *)file->private_data;
	struct lcd_extern_dev_s *edev = NULL;
	struct aml_lcd_extern_bin_s ext_bin;
	unsigned char *bin_buf;
	int ret = 0;

	if (!edrv)
		return -EFAULT;

	mcd_nr = _IOC_NR(cmd);
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		EXTPR("[%d]: %s: cmd_dir = 0x%x, cmd_nr = 0x%x\n",
			edrv->index, __func__, _IOC_DIR(cmd), mcd_nr);
	}

	argp = (void __user *)arg;
	switch (mcd_nr) {
	case LCD_EXT_IOC_NR_SET_BIN:
		memset(&ext_bin, 0, sizeof(struct aml_lcd_extern_bin_s));
		if (copy_from_user(&ext_bin, argp, sizeof(struct aml_lcd_extern_bin_s))) {
			ret = -EFAULT;
			break;
		}
		if (ext_bin.bin_size == 0 || ext_bin.bin_size > LCD_EXTERN_INIT_ON_MAX) {
			EXTERR("%s: invalid data size %d\n", __func__, ext_bin.bin_size);
			ret = -EFAULT;
			break;
		}
		edev = lcd_extern_get_dev(edrv, ext_bin.dev_index);
		if (!edev) {
			ret = -EFAULT;
			break;
		}

		bin_buf = kzalloc(ext_bin.bin_size, GFP_KERNEL);
		if (!bin_buf) {
			ret = -EFAULT;
			break;
		}

		argp = (void __user *)ext_bin.ptr;
		if (copy_from_user(bin_buf, argp, ext_bin.bin_size)) {
			kfree(bin_buf);
			ret = -EFAULT;
			break;
		}

		ret = ext_reload_cmd_bin(edrv, edev, ext_bin.i2c_index, ext_bin.multi_id,
				bin_buf, ext_bin.bin_size);
		if (ret)
			ret = -EFAULT;
		kfree(bin_buf);
		break;
	default:
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long ext_compat_ioctl(struct file *file, unsigned int cmd,
			     unsigned long arg)
{
	unsigned long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = ext_ioctl(file, cmd, arg);
	return ret;
}
#endif

static const struct file_operations ext_fops = {
	.owner          = THIS_MODULE,
	.open           = ext_io_open,
	.release        = ext_io_release,
	.unlocked_ioctl = ext_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = ext_compat_ioctl,
#endif
};

static int ext_cdev_add(struct lcd_extern_driver_s *edrv, struct device *parent)
{
	dev_t devno;
	int ret = 0;

	if (!edrv) {
		EXTERR("%s: edrv is null\n", __func__);
		return -1;
	}
	if (!ext_cdev) {
		ret = 1;
		goto ext_cdev_add_failed;
	}

	devno = MKDEV(MAJOR(ext_cdev->devno), edrv->index);

	cdev_init(&edrv->cdev, &ext_fops);
	edrv->cdev.owner = THIS_MODULE;
	ret = cdev_add(&edrv->cdev, devno, 1);
	if (ret) {
		ret = 2;
		goto ext_cdev_add_failed;
	}

	edrv->sub_dev = device_create(ext_cdev->class, parent,
				      devno, NULL, "ext%d", edrv->index);
	if (IS_ERR_OR_NULL(edrv->sub_dev)) {
		ret = 3;
		goto ext_cdev_add_failed1;
	}

	dev_set_drvdata(edrv->sub_dev, edrv);
	edrv->sub_dev->of_node = parent->of_node;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		EXTPR("[%d]: %s OK\n", edrv->index, __func__);
	return 0;

ext_cdev_add_failed1:
	cdev_del(&edrv->cdev);
ext_cdev_add_failed:
	EXTERR("[%d]: %s: failed: %d\n", edrv->index, __func__, ret);
	return -1;
}

static void ext_cdev_remove(struct lcd_extern_driver_s *edrv)
{
	dev_t devno;

	if (!ext_cdev || !edrv)
		return;

	devno = MKDEV(MAJOR(ext_cdev->devno), edrv->index);
	device_destroy(ext_cdev->class, devno);
	cdev_del(&edrv->cdev);
}

static int ext_global_init_once(void)
{
	int ret;

	if (ext_global_init_flag) {
		ext_global_init_flag++;
		return 0;
	}
	ext_global_init_flag++;

	ext_cdev = kzalloc(sizeof(*ext_cdev), GFP_KERNEL);
	if (!ext_cdev)
		return -1;

	ret = alloc_chrdev_region(&ext_cdev->devno, 0,
				  LCD_MAX_DRV, EXT_CDEV_NAME);
	if (ret) {
		ret = 1;
		goto ext_global_init_once_err;
	}

	ext_cdev->class = class_create(THIS_MODULE, "lcd_ext");
	if (IS_ERR_OR_NULL(ext_cdev->class)) {
		ret = 2;
		goto ext_global_init_once_err_1;
	}

	return 0;

ext_global_init_once_err_1:
	unregister_chrdev_region(ext_cdev->devno, LCD_MAX_DRV);
ext_global_init_once_err:
	kfree(ext_cdev);
	ext_cdev = NULL;
	EXTERR("%s: failed: %d\n", __func__, ret);
	return -1;
}

static void ext_global_remove_once(void)
{
	if (ext_global_init_flag > 1) {
		ext_global_init_flag--;
		return;
	}
	ext_global_init_flag--;

	if (!ext_cdev)
		return;

	class_destroy(ext_cdev->class);
	unregister_chrdev_region(ext_cdev->devno, LCD_MAX_DRV);
	kfree(ext_cdev);
	ext_cdev = NULL;
}

/* **************************************** */
static int aml_lcd_extern_probe(struct platform_device *pdev)
{
	struct lcd_extern_driver_s *edrv;
	int index = 0;
	int ret;

	ext_global_init_once();

	if (!pdev->dev.of_node)
		return -1;
	ret = of_property_read_u32(pdev->dev.of_node, "index", &index);
	if (ret) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			EXTPR("%s: no index exist, default to 0\n", __func__);
		index = 0;
	}
	if (index >= LCD_MAX_DRV) {
		EXTERR("%s: invalid index %d\n", __func__, index);
		return -1;
	}
	if (ext_drv_init_state & (1 << index)) {
		EXTERR("%s: index %d driver already registered\n",
		       __func__, index);
		return -1;
	}
	ext_drv_init_state |= (1 << index);

	edrv = kzalloc(sizeof(*edrv), GFP_KERNEL);
	if (!edrv)
		return -ENOMEM;
	edrv->index = index;
	ext_driver[index] = edrv;
	mutex_init(&edrv->power_mutex);

	/* set drvdata */
	platform_set_drvdata(pdev, edrv);
	ret = ext_cdev_add(edrv, &pdev->dev);
	if (ret)
		goto lcd_extern_probe_exit;
	edrv->pdev = pdev;
	edrv->dev_cnt = 0;

	ret = lcd_extern_config_load(edrv);
	if (ret)
		goto lcd_extern_probe_exit;
	if (edrv->dev_cnt == 0) //no device exit
		goto lcd_extern_probe_exit;

	lcd_extern_debug_file_creat(edrv);

	EXTPR("[%d]: probe OK, init_state:0x%x\n", index, ext_drv_init_state);
	return 0;

lcd_extern_probe_exit:
	/* free drvdata */
	platform_set_drvdata(pdev, NULL);
	/* free drv */
	kfree(edrv);
	ext_driver[index] = NULL;
	ext_drv_init_state &= ~(1 << index);
	if (ret)
		EXTPR("[%d]: %s: failed\n", index, __func__);
	return ret;
}

static int aml_lcd_extern_remove(struct platform_device *pdev)
{
	struct lcd_extern_driver_s *edrv = platform_get_drvdata(pdev);
	int index, i;

	if (!edrv)
		return 0;

	index = edrv->index;

	cancel_delayed_work(&edrv->dev_probe_dly_work);
	lcd_extern_debug_file_remove(edrv);
	ext_cdev_remove(edrv);

	platform_set_drvdata(pdev, NULL);
	for (i = 0; i < edrv->dev_cnt; i++)
		lcd_extern_dev_free(edrv->dev[i]);
	kfree(edrv);
	ext_driver[index] = NULL;

	ext_drv_init_state &= ~(1 << index);
	ext_global_remove_once();

	EXTPR("[%d]: %s, init_state:0x%x\n",
		index, __func__, ext_drv_init_state);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id aml_lcd_extern_dt_match[] = {
	{
		.compatible = "amlogic, lcd_extern",
	},
	{},
};
#endif

static struct platform_driver aml_lcd_extern_driver = {
	.probe  = aml_lcd_extern_probe,
	.remove = aml_lcd_extern_remove,
	.driver = {
		.name  = "lcd_extern",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = aml_lcd_extern_dt_match,
#endif
	},
};

int __init aml_lcd_extern_init(void)
{
	int ret;

	ret = platform_driver_register(&aml_lcd_extern_driver);
	if (ret) {
		EXTERR("driver register failed\n");
		return -ENODEV;
	}
	return ret;
}

void __exit aml_lcd_extern_exit(void)
{
	platform_driver_unregister(&aml_lcd_extern_driver);
}

//MODULE_AUTHOR("AMLOGIC");
//MODULE_DESCRIPTION("LCD extern driver");
//MODULE_LICENSE("GPL");

