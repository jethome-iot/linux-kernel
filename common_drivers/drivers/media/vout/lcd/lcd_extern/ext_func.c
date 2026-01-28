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
#include <linux/amlogic/media/vout/lcd/lcd_extern.h>
#include "lcd_extern.h"

void lcd_extern_gpio_probe(struct lcd_extern_driver_s *edrv, unsigned char index)
{
	struct lcd_ext_gpio_s *ext_gpio;
	const char *str;
	int ret;

	if (index >= LCD_EXTERN_GPIO_NUM_MAX) {
		EXTERR("[%d]: %s: invalid gpio index %d, exit\n",
		       edrv->index, __func__, index);
		return;
	}
	ext_gpio = &edrv->gpio[index];
	if (ext_gpio->probe_flag) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			EXTPR("[%d]: %s: gpio %s[%d] is already probed\n",
			      edrv->index, __func__, ext_gpio->name, index);
		}
		return;
	}

	/* get gpio name */
	ret = of_property_read_string_index(edrv->sub_dev->of_node,
					    "extern_gpio_names", index, &str);
	if (ret) {
		EXTERR("[%d]: %s: failed to get extern_gpio_names: %d\n",
		       edrv->index, __func__, index);
		str = "unknown";
	}
	strscpy(ext_gpio->name, str, LCD_CPU_GPIO_NAME_MAX);

	/* init gpio flag */
	ext_gpio->probe_flag = 1;
	ext_gpio->register_flag = 0;
}

void lcd_extern_gpio_unregister(struct lcd_extern_driver_s *edrv, int index)
{
	struct lcd_ext_gpio_s *ext_gpio;

	if (index >= LCD_EXTERN_GPIO_NUM_MAX) {
		EXTERR("[%d]: %s: invalid gpio index %d, exit\n",
		       edrv->index, __func__, index);
		return;
	}
	ext_gpio = &edrv->gpio[index];
	if (ext_gpio->probe_flag == 0) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			EXTPR("[%d]: %s: gpio [%d] is not probed, exit\n",
			      edrv->index, __func__, index);
		}
		return;
	}
	if (ext_gpio->register_flag == 0) {
		EXTPR("[%d]: %s: gpio %s[%d] is already unregistered\n",
		      edrv->index, __func__, ext_gpio->name, index);
		return;
	}
	if (IS_ERR(ext_gpio->gpio)) {
		EXTERR("[%d]: %s: gpio %s[%d]: %p, err: %d\n",
		       edrv->index, __func__, ext_gpio->name, index,
		       ext_gpio->gpio, IS_ERR(ext_gpio->gpio));
		ext_gpio->gpio = NULL;
		return;
	}

	/* release gpio */
	devm_gpiod_put(edrv->sub_dev, ext_gpio->gpio);
	ext_gpio->register_flag = 0;
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		EXTPR("[%d]: %s: release gpio %s[%d]\n",
		      edrv->index, __func__, ext_gpio->name, index);
	}
}

static int lcd_extern_gpio_register(struct lcd_extern_driver_s *edrv,
				    unsigned char index, int init_value)
{
	struct lcd_ext_gpio_s *ext_gpio;
	int value;

	if (index >= LCD_EXTERN_GPIO_NUM_MAX) {
		EXTERR("[%d]: %s: invalid gpio [%d], exit\n",
		       edrv->index, __func__, index);
		return -1;
	}
	ext_gpio = &edrv->gpio[index];
	if (ext_gpio->probe_flag == 0) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			EXTPR("[%d]: %s: gpio [%d] is not probed, exit\n",
			      edrv->index, __func__, index);
		}
		return -1;
	}
	if (ext_gpio->register_flag) {
		EXTPR("[%d]: %s: gpio %s[%d] is already registered\n",
		      edrv->index, __func__, ext_gpio->name, index);
		return 0;
	}

	switch (init_value) {
	case LCD_GPIO_OUTPUT_LOW:
		value = GPIOD_OUT_LOW;
		break;
	case LCD_GPIO_OUTPUT_HIGH:
		value = GPIOD_OUT_HIGH;
		break;
	case LCD_GPIO_INPUT:
	default:
		value = GPIOD_IN;
		break;
	}

	/* request gpio */
	ext_gpio->gpio = devm_gpiod_get_index(edrv->sub_dev, "extern", index, value);
	if (IS_ERR(ext_gpio->gpio)) {
		EXTERR("[%d]: %s: gpio %s[%d]: %p, err: %d\n",
		       edrv->index, __func__, ext_gpio->name, index, ext_gpio->gpio,
		       IS_ERR(ext_gpio->gpio));
		ext_gpio->gpio = NULL;
		return -1;
	}
	ext_gpio->register_flag = 1;
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		EXTPR("[%d]: %s: gpio %s[%d]: %p, init value: %d\n",
		      edrv->index, __func__, ext_gpio->name, index,
		      ext_gpio->gpio, init_value);
	}

	return 0;
}

void lcd_extern_gpio_set(struct lcd_extern_driver_s *edrv, unsigned char index, int value)
{
	struct lcd_ext_gpio_s *ext_gpio;

	if (index >= LCD_EXTERN_GPIO_NUM_MAX) {
		EXTERR("[%d]: %s: invalid gpio [%d], exit\n",
		       edrv->index, __func__, index);
		return;
	}
	ext_gpio = &edrv->gpio[index];
	if (ext_gpio->probe_flag == 0) {
		EXTPR("[%d]: %s: gpio [%d] is not probed, exit\n",
		      edrv->index, __func__, index);
		return;
	}
	if (ext_gpio->register_flag == 0) {
		lcd_extern_gpio_register(edrv, index, value);
		return;
	}

	if (IS_ERR_OR_NULL(ext_gpio->gpio)) {
		EXTERR("[%d]: %s: gpio %s[%d]: %p, err: %ld\n",
		       edrv->index, __func__, ext_gpio->name, index,
		       ext_gpio->gpio, PTR_ERR(ext_gpio->gpio));
		return;
	}

	switch (value) {
	case LCD_GPIO_OUTPUT_LOW:
	case LCD_GPIO_OUTPUT_HIGH:
		gpiod_direction_output(ext_gpio->gpio, value);
		break;
	case LCD_GPIO_INPUT:
	default:
		gpiod_direction_input(ext_gpio->gpio);
		break;
	}
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		EXTPR("[%d]: %s: set gpio %s[%d] value: %d\n",
		      edrv->index, __func__, ext_gpio->name, index, value);
	}
}

unsigned int lcd_extern_gpio_get(struct lcd_extern_driver_s *edrv, unsigned char index)
{
	struct lcd_ext_gpio_s *ext_gpio;

	if (index >= LCD_EXTERN_GPIO_NUM_MAX) {
		EXTERR("[%d]: %s: invalid gpio [%d], exit\n",
		       edrv->index, __func__, index);
		return -1;
	}
	ext_gpio = &edrv->gpio[index];
	if (ext_gpio->probe_flag == 0) {
		EXTPR("[%d]: %s: gpio [%d] is not probed, exit\n",
		      edrv->index, __func__, index);
		return -1;
	}
	if (ext_gpio->register_flag == 0) {
		EXTERR("[%d]: %s: gpio %s[%d] is not registered, exit\n",
		       edrv->index, __func__, ext_gpio->name, index);
		return -1;
	}
	if (IS_ERR_OR_NULL(ext_gpio->gpio)) {
		EXTERR("[%d]: %s: gpio %s[%d]: %p, err: %ld\n",
		       edrv->index, __func__, ext_gpio->name, index,
		       ext_gpio->gpio, PTR_ERR(ext_gpio->gpio));
		return -1;
	}

	return gpiod_get_value(ext_gpio->gpio);
}

#define LCD_EXTERN_PINMUX_MAX    3
static char *lcd_extern_pinmux_str[LCD_EXTERN_PINMUX_MAX] = {
	"extern_on",   /* 0 */
	"extern_off",  /* 1 */
	"none",
};

void lcd_extern_pinmux_set(struct lcd_extern_driver_s *edrv, int status)
{
	int index = 0xff;

	if (edrv->pinmux_valid == 0) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			EXTPR("[%d]: %s: pinmux invalid, bypass\n",
			      edrv->index, __func__);
		return;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		EXTPR("[%d]: %s: %d\n", edrv->index, __func__, status);

	index = (status) ? 0 : 1;
	if (edrv->pinmux_flag == index) {
		EXTPR("[%d]: %s: pinmux %s is already selected\n",
		      edrv->index, __func__, lcd_extern_pinmux_str[index]);
		return;
	}

	/* request pinmux */
	edrv->pin = devm_pinctrl_get_select(edrv->sub_dev, lcd_extern_pinmux_str[index]);
	if (IS_ERR(edrv->pin)) {
		EXTERR("[%d]: %s: set pinmux %s error\n",
		       edrv->index, __func__, lcd_extern_pinmux_str[index]);
	} else {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			EXTPR("[%d]: %s: set pinmux %s ok\n",
			      edrv->index, __func__, lcd_extern_pinmux_str[index]);
		}
	}
	edrv->pinmux_flag = index;
}

void lcd_extern_check_add(struct lcd_extern_driver_s *edrv, struct lcd_extern_dev_s *edev,
		int cmd_step, unsigned char *data_buf, unsigned char data_len)
{
	int i;

	if (!edrv || !edev || !data_buf)
		return;
	if (data_len < 1 || ((data_len - 1) % 2)) {
		edev->check_step = 0;
		EXTERR("[%d]: %s: dev[%d]: step[%d]: data_len %d error\n",
			edrv->index, __func__, edev->dev_index, cmd_step, data_len);
		return;
	}

	edev->check_step = data_buf[0];
	edev->check_execute = 0;
	if (data_len == 1) //special case for whole data check
		edev->check_block_cnt = 1;
	else
		edev->check_block_cnt = (data_len - 1) / 2;
	edev->check_state = 1; //default execute
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		EXTPR("[%d]: %s: dev[%d]: check_step=%d, check_block_cnt=%d, check_state=%d\n",
			edrv->index, __func__, edev->dev_index,
			edev->check_step, edev->check_block_cnt, edev->check_state);
	}

	kfree(edev->check_block);
	edev->check_block = kcalloc(edev->check_block_cnt,
			sizeof(struct lcd_extern_check_block_s), GFP_KERNEL);
	if (!edev->check_block)
		return;

	if (data_len == 1) {//special case for whole data check
		edev->check_block[0].offset = 0;
		edev->check_block[0].len = 0;
	} else {
		for (i = 0; i < edev->check_block_cnt; i++) {
			edev->check_block[i].offset = data_buf[1 + i * 2];
			edev->check_block[i].len = data_buf[1 + i * 2 + 1];
		}
	}
}

void lcd_extern_check_handler(struct lcd_extern_driver_s *edrv, struct lcd_extern_dev_s *edev,
		struct i2c_client *i2client, unsigned char cmd_type,
		unsigned char *raw_table, unsigned char data_len)
{
	unsigned char *read_buf = NULL, *chk_buf = NULL, *chk_table = NULL, *raw_buf = NULL;
	unsigned char read_buf_offset, read_start;
	unsigned int read_len, cmp_len, pr_len;
	char *pr_buf;
	int i, check_fail = 0, ret = 0;

	if (!edrv || !edev || !i2client || !raw_table || data_len == 0)
		return;

	if (edev->check_step == 0 || !edev->check_block)
		return;

	switch (cmd_type & 0xf0) {
	case LCD_EXT_CMD_TYPE_CMD:
	case LCD_EXT_CMD_TYPE_CMD_BIN:
		read_start = raw_table[0]; //reg_addr
		read_buf_offset = 0;
		data_len--;
		raw_table++;
		read_len = data_len;
		break;
	case LCD_EXT_CMD_TYPE_CMD_BIN_DATA:
		read_start = 0; //reg_addr nonexistent
		read_buf_offset = 0;
		read_len = data_len;
		break;
	case LCD_EXT_CMD_TYPE_CMD_BIN2:
		read_start = 0; //read start from addr 0
		read_buf_offset = raw_table[0];
		data_len--;
		raw_table++;
		read_len = data_len + read_buf_offset;
		break;
	default:
		return;
	}

	edev->check_execute = 1;
	pr_buf = kcalloc(1024, sizeof(char), GFP_KERNEL);
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		if (pr_buf) {
			pr_len = sprintf(pr_buf, "%s: read_start: 0x%x, ", __func__, read_start);
			pr_len += sprintf(pr_buf + pr_len,
				"data_len: %d, read_buf_offset: %d, read_len: %d\n",
				data_len, read_buf_offset, read_len);
			EXTPR("%s\n", pr_buf);
			pr_len = sprintf(pr_buf, "%s: raw_table data:", __func__);
			for (i = 0; i < data_len; i++)
				pr_len += sprintf(pr_buf + pr_len, " %02x", raw_table[i]);
			EXTPR("%s\n", pr_buf);
		}
	}

	read_buf = kcalloc(data_len, sizeof(unsigned char), GFP_KERNEL);
	if (!read_buf) {
		kfree(pr_buf);
		return;
	}

	read_buf[0] = read_start;
	ret = lcd_extern_i2c_read(i2client, read_buf, 1, read_buf, read_len);
	if (ret < 0)
		goto lcd_extern_check_handler_end;

	chk_table = read_buf + read_buf_offset;
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		if (pr_buf) {
			pr_len = sprintf(pr_buf, "%s: chk_table data:", __func__);
			for (i = 0; i < data_len; i++)
				pr_len += sprintf(pr_buf + pr_len, " %02x", chk_table[i]);
			EXTPR("%s\n", pr_buf);
		}
	}

	for (i = 0; i < edev->check_block_cnt; i++) {
		cmp_len = edev->check_block[i].len;
		if (edev->check_block[i].len == 0) {
			if (edev->check_block[i].offset) {
				EXTERR("[%d]: %s: dev[%d]: check_block[%d] len is 0!\n",
					edrv->index, __func__, edev->dev_index, i);
				continue;
			}
			cmp_len = data_len;//offset=0,len=0, special case for whole data check
		}
		if (edev->check_block[i].offset + cmp_len > data_len) {
			EXTERR("[%d]: %s: dev[%d]: check_block[%d] offset+len %d oversize!\n",
				edrv->index, __func__, edev->dev_index, i,
				edev->check_block[i].offset + cmp_len);
			continue;
		}
		chk_buf = chk_table + edev->check_block[i].offset;
		raw_buf = raw_table + edev->check_block[i].offset;
		ret = memcmp(chk_buf, raw_buf, cmp_len);
		if (ret) {
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				EXTPR("[%d]: %s: dev[%d]: offset:0x%x, len:%d not match!\n",
					edrv->index, __func__, edev->dev_index,
					edev->check_block[i].offset, cmp_len);
			}
			check_fail++;
		}
	}

	if (check_fail)
		edev->check_state = 1;
	else
		edev->check_state = 0;
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		EXTPR("[%d]: %s: dev[%d]: check_state: %d\n",
			edrv->index, __func__, edev->dev_index, edev->check_state);
	}

lcd_extern_check_handler_end:
	kfree(pr_buf);
	kfree(read_buf);
}

int lcd_extern_cmd_multi_id(struct lcd_extern_driver_s *edrv, struct lcd_extern_dev_s *edev,
			unsigned char multi_id)
{
	struct aml_lcd_drv_s *pdrv;
	struct lcd_extern_multi_list_s *temp_list;
	unsigned short frame_rate, frame_rate_max, frame_rate_min;
	unsigned char *data_buf;

	pdrv = aml_lcd_get_driver(edrv->index);
	if (!pdrv)
		return -1;

	frame_rate = pdrv->config.timing.act_timing.frame_rate;

	temp_list = edev->multi_list_header;
	while (temp_list) {
		if (multi_id != temp_list->index) {
			temp_list = temp_list->next;
			continue;
		}

		data_buf = temp_list->data_buf;
		switch (temp_list->type) {
		case LCD_EXT_CMD_TYPE_MULTI_LIST_FR:
			frame_rate_min = data_buf[0];
			frame_rate_max = data_buf[1];
			if (frame_rate < frame_rate_min || frame_rate > frame_rate_max)
				return -1;
			goto lcd_extern_cmd_multi_id_find_fr;
		case LCD_EXT_CMD_TYPE_MULTI_LIST_UFR:
			frame_rate_min = *(unsigned short *)&data_buf[0];
			frame_rate_max = *(unsigned short *)&data_buf[2];
			if (frame_rate < frame_rate_min || frame_rate > frame_rate_max)
				return -1;
			goto lcd_extern_cmd_multi_id_find_fr;
		default:
			break;
		}

		temp_list = temp_list->next;
	}
	return -1;

lcd_extern_cmd_multi_id_find_fr:
	EXTPR("[%d]: %s: dev[%d]: multi_id=%d, type=0x%x, framerate=%d\n",
		edrv->index, __func__, edev->dev_index,
		temp_list->index, temp_list->type, frame_rate);
	return 0;
}

int lcd_extern_cmd_gpio(struct lcd_extern_driver_s *edrv, struct lcd_extern_dev_s *edev,
		int cmd_step, unsigned char *data_buf, unsigned char data_len)
{
	int delay_len, delay_ms = 0;
	int i;

	if (!edrv || !edev || !data_buf)
		return -1;
	if (data_len < 2) {
		EXTERR("[%d]: dev[%d] step[%d]: invalid data_len %d for GPIO\n",
			edrv->index, edev->dev_index, cmd_step, data_len);
		return -1;
	}

	if (edev->check_step) {
		if (edev->check_state == 0)
			return 0;
	}

	lcd_extern_gpio_set(edrv, data_buf[0], data_buf[1]);
	delay_len = data_len - 2;
	if (delay_len) {
		for (i = 0; i < delay_len; i++)
			delay_ms |= (data_buf[i + 2] << (i * 8));
		if (delay_ms)
			lcd_delay_ms(delay_ms);
	}

	return 0;
}

int lcd_extern_cmd_wait_gpio(struct lcd_extern_driver_s *edrv, struct lcd_extern_dev_s *edev,
		int cmd_step, unsigned char *data_buf, unsigned char data_len)
{
	int delay_len, delay_ms = 0, done = 0;
	int i, ret;

	if (!edrv || !edev || !data_buf)
		return -1;
	if (data_len < 3) {
		EXTERR("[%d]: dev[%d] step[%d]: invalid data_len %d for wait_gpio\n",
			edrv->index, edev->dev_index, cmd_step, data_len);
		return -1;
	}

	if (edev->check_step) {
		if (edev->check_state == 0)
			return 0;
	}

	delay_len = data_len - 2;
	for (i = 0; i < delay_len; i++)
		delay_ms |= (data_buf[i + 2] << (i * 8));
	for (i = 0; i < delay_ms; i++) {
		ret = lcd_extern_gpio_get(edrv, data_buf[0]);
		if (ret == data_buf[1]) {
			done = 1;
			break;
		}
		lcd_delay_ms(1);
	}
	if (done == 0) {
		EXTPR("[%d]: step[%d]: wait for gpio[%d] %s to %d timeout: %dms\n",
			edrv->index, cmd_step, data_buf[0], edrv->gpio[data_buf[0]].name,
			data_buf[1], delay_ms);
	}

	return 0;
}

int lcd_extern_cmd_delay(struct lcd_extern_driver_s *edrv, struct lcd_extern_dev_s *edev,
		int cmd_step, unsigned char *data_buf, unsigned char data_len)
{
	int delay_ms = 0, i;

	if (!edrv || !edev || !data_buf)
		return -1;

	if (edev->check_step) {
		if (edev->check_state == 0)
			return 0;
	}

	for (i = 0; i < data_len; i++)
		delay_ms |= (data_buf[i] << (i * 8));
	if (delay_ms > 0)
		lcd_delay_ms(delay_ms);

	return 0;
}

int lcd_extern_cmd_i2c(struct lcd_extern_driver_s *edrv, struct lcd_extern_dev_s *edev,
			int cmd_step, unsigned char cmd_type,
			unsigned char *data_buf, unsigned char data_len)
{
	int i2c_idx;
	struct i2c_client *i2client = NULL;

	if (!edrv || !edev || !data_buf)
		return -1;
	i2c_idx = cmd_type & 0xf;
	if (i2c_idx >= 4 || !edev->i2c_dev[i2c_idx] || !edev->i2c_dev[i2c_idx]->client) {
		EXTERR("[%d]: dev[%d] invalid i2c[%d] device\n",
			edrv->index, edev->dev_index, i2c_idx);
		return -1;
	}

	i2client = edev->i2c_dev[i2c_idx]->client;
	if (edev->check_step) {
		if (edev->check_execute == 0) {
			lcd_extern_check_handler(edrv, edev, i2client,
					cmd_type, data_buf, data_len);
		}
		if (edev->check_state == 0)
			return 0;
	}
	return lcd_extern_i2c_write(i2client, data_buf, data_len);
}

static int lcd_extern_power_cmd_dynamic_size_i2c(struct lcd_extern_driver_s *edrv,
		struct lcd_extern_dev_s *edev, unsigned char *table, int max_len)
{
	int i = 0, step = 0;
	unsigned char type, i2c_idx, next_cmd, size;
	int ret = 0;

	if (!edrv || !edev || !table)
		return -1;

	while ((i + 1) < max_len) {
		type = table[i];
		size = table[i + 1];
		if (type == LCD_EXT_CMD_TYPE_END)
			break;
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			EXTPR("[%d]: %s: dev[%d] step[%d]: type=0x%02x, size=%d, chk[%d]_st=%d\n",
				edrv->index, __func__, edev->dev_index,
				step, type, size, edev->check_step, edev->check_state);
		}
		if (size == 0)
			goto power_cmd_dynamic_i2c_next;
		if ((i + 2 + size) > max_len)
			break;

		switch (type) {
		case LCD_EXT_CMD_TYPE_NONE:
			/* do nothing */
			break;
		case LCD_EXT_CMD_TYPE_GPIO:
			lcd_extern_cmd_gpio(edrv, edev, step, &table[i + 2], size);
			break;
		case LCD_EXT_CMD_TYPE_WAIT_GPIO:
			lcd_extern_cmd_wait_gpio(edrv, edev, step, &table[i + 2], size);
			break;
		case LCD_EXT_CMD_TYPE_DELAY:
			lcd_extern_cmd_delay(edrv, edev, step, &table[i + 2], size);
			break;
		case LCD_EXT_CMD_TYPE_CHECK:
			lcd_extern_check_add(edrv, edev, step, &table[i + 2], size);
			goto power_cmd_dynamic_i2c_check_next;
		case LCD_EXT_CMD_TYPE_CMD:
		case LCD_EXT_CMD_TYPE_CMD_BIN:
		case LCD_EXT_CMD_TYPE_CMD_BIN_DATA:
		case LCD_EXT_CMD_TYPE_CMD_BIN2:
		case LCD_EXT_CMD_TYPE_CMD2:
		case LCD_EXT_CMD_TYPE_CMD2_BIN:
		case LCD_EXT_CMD_TYPE_CMD2_BIN_DATA:
		case LCD_EXT_CMD_TYPE_CMD2_BIN2:
		case LCD_EXT_CMD_TYPE_CMD3:
		case LCD_EXT_CMD_TYPE_CMD3_BIN:
		case LCD_EXT_CMD_TYPE_CMD3_BIN_DATA:
		case LCD_EXT_CMD_TYPE_CMD3_BIN2:
		case LCD_EXT_CMD_TYPE_CMD4:
		case LCD_EXT_CMD_TYPE_CMD4_BIN:
		case LCD_EXT_CMD_TYPE_CMD4_BIN_DATA:
		case LCD_EXT_CMD_TYPE_CMD4_BIN2:
			ret = lcd_extern_cmd_i2c(edrv, edev, step, type, &table[i + 2], size);
			if (ret)
				goto power_cmd_dynamic_i2c_err;
			break;
		case LCD_EXT_CMD_TYPE_MULTI_LIST_FR:
		case LCD_EXT_CMD_TYPE_MULTI_LIST_UFR:
			/* do nothing here */
			break;
		case LCD_EXT_CMD_TYPE_MULTI_DFT_CMD:
		case LCD_EXT_CMD_TYPE_MULTI_CMD:
			if (size < 2) {
				EXTERR("[%d]: dev[%d] step[%d]: invalid size %d for multi_cmd\n",
					edrv->index, edev->dev_index, step, size);
				goto power_cmd_dynamic_i2c_next;
			}

			ret = lcd_extern_cmd_multi_id(edrv, edev, table[i + 2]);
			if (ret)
				goto power_cmd_dynamic_i2c_next;
			if (type == LCD_EXT_CMD_TYPE_MULTI_DFT_CMD && edev->state == 0) {
				EXTPR("[%d]: dev[%d]: bypass init for multi dft cmd\n",
					edrv->index, edev->dev_index);
				goto power_cmd_dynamic_i2c_next;
			}
			next_cmd = table[i + 3];
			if ((next_cmd & 0xf0) == LCD_EXT_CMD_TYPE_CMD ||
			    (next_cmd & 0xf0) == LCD_EXT_CMD_TYPE_CMD_BIN ||
			    (next_cmd & 0xf0) == LCD_EXT_CMD_TYPE_CMD_BIN2 ||
			    (next_cmd & 0xf0) == LCD_EXT_CMD_TYPE_CMD_BIN_DATA) {
				ret = lcd_extern_cmd_i2c(edrv, edev, step,
					next_cmd, &table[i + 4], (size - 2));
				if (ret)
					goto power_cmd_dynamic_i2c_err;
			} else if ((next_cmd & 0xf0) == LCD_EXT_CMD_TYPE_CMD_DELAY) {
				ret = lcd_extern_cmd_i2c(edrv, edev, step,
					next_cmd, &table[i + 4], (size - 3));
				if (ret)
					goto power_cmd_dynamic_i2c_err;
				if (table[i + size + 1])
					lcd_delay_ms(table[i + size + 1]);
			} else if (next_cmd == LCD_EXT_CMD_TYPE_GPIO) {
				lcd_extern_cmd_gpio(edrv, edev, step, &table[i + 4], (size - 2));
			} else if (next_cmd == LCD_EXT_CMD_TYPE_WAIT_GPIO) {
				lcd_extern_cmd_wait_gpio(edrv, edev, step,
							 &table[i + 4], (size - 2));
			} else if (next_cmd == LCD_EXT_CMD_TYPE_DELAY) {
				lcd_extern_cmd_delay(edrv, edev, step, &table[i + 4], (size - 2));
			}
			break;
		case LCD_EXT_CMD_TYPE_CMD_MULTI:
		case LCD_EXT_CMD_TYPE_CMD2_MULTI:
		case LCD_EXT_CMD_TYPE_CMD3_MULTI:
		case LCD_EXT_CMD_TYPE_CMD4_MULTI:
			ret = lcd_extern_cmd_multi_id(edrv, edev, table[i + 2]);
			if (ret)
				goto power_cmd_dynamic_i2c_next;
			i2c_idx = type & 0xf;
			next_cmd = ((table[i + 3] << 4) | i2c_idx);
			ret = lcd_extern_cmd_i2c(edrv, edev, step,
					next_cmd, &table[i + 4], (size - 2));
			if (ret)
				goto power_cmd_dynamic_i2c_err;
			break;
		case LCD_EXT_CMD_TYPE_CMD_DELAY:
		case LCD_EXT_CMD_TYPE_CMD2_DELAY:
		case LCD_EXT_CMD_TYPE_CMD3_DELAY:
		case LCD_EXT_CMD_TYPE_CMD4_DELAY:
			ret = lcd_extern_cmd_i2c(edrv, edev, step, type, &table[i + 2], (size - 1));
			if (ret)
				goto power_cmd_dynamic_i2c_err;
			if (table[i + size + 1])
				lcd_delay_ms(table[i + size + 1]);
			break;
		default:
			EXTERR("[%d]: %s: %s(%d): type 0x%02x invalid\n",
				edrv->index, __func__,
				edev->config.name, edev->config.index, type);
			break;
		}
power_cmd_dynamic_i2c_next:
		if (edev->check_step)
			edev->check_step--;
power_cmd_dynamic_i2c_check_next:
		i += (size + 2);
		step++;
	}

	return 0;

power_cmd_dynamic_i2c_err:
	return -1;
}

static int lcd_extern_power_cmd_dynamic_size_spi(struct lcd_extern_driver_s *edrv,
		struct lcd_extern_dev_s *edev, unsigned char *table, int max_len)
{
	int i = 0, step = 0;
	unsigned char type, size;
	int ret = 0;

	if (!edrv || !edev || !table)
		return -1;

	while ((i + 1) < max_len) {
		type = table[i];
		size = table[i + 1];
		if (type == LCD_EXT_CMD_TYPE_END)
			break;
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			EXTPR("[%d]: %s: dev[%d] step %d: type=0x%02x, size=%d\n",
				edrv->index, __func__,
				edev->dev_index, step, type, size);
		}
		if (size == 0)
			goto power_cmd_dynamic_spi_next;
		if ((i + 2 + size) > max_len)
			break;

		switch (type) {
		case LCD_EXT_CMD_TYPE_NONE:
			/* do nothingy */
			break;
		case LCD_EXT_CMD_TYPE_GPIO:
			lcd_extern_cmd_gpio(edrv, edev, step, &table[i + 2], size);
			break;
		case LCD_EXT_CMD_TYPE_WAIT_GPIO:
			lcd_extern_cmd_wait_gpio(edrv, edev, step, &table[i + 2], size);
			break;
		case LCD_EXT_CMD_TYPE_DELAY:
			lcd_extern_cmd_delay(edrv, edev, step, &table[i + 2], size);
			break;
		case LCD_EXT_CMD_TYPE_CMD:
			ret = lcd_extern_spi_write(edrv, edev, &table[i + 2], size);
			if (ret)
				goto power_cmd_dynamic_spi_err;
			break;
		case LCD_EXT_CMD_TYPE_CMD_DELAY:
			ret = lcd_extern_spi_write(edrv, edev, &table[i + 2], (size - 1));
			if (ret)
				goto power_cmd_dynamic_spi_err;
			if (table[i + size + 1] > 0)
				lcd_delay_ms(table[i + size + 1]);
			break;
		default:
			EXTERR("[%d]: %s: %s(%d): type 0x%02x invalid\n",
				edrv->index, __func__,
				edev->config.name, edev->config.index, type);
			break;
		}
power_cmd_dynamic_spi_next:
		i += (size + 2);
		step++;
	}

	return 0;

power_cmd_dynamic_spi_err:
	return -1;
}

static int lcd_extern_power_cmd_dynamic_size_simple(struct lcd_extern_driver_s *edrv,
		struct lcd_extern_dev_s *edev, unsigned char *table, int max_len)
{
	int i = 0, step = 0;
	unsigned char type, next_cmd, size;
	int ret = 0;

	if (!edrv || !edev || !table)
		return -1;

	while ((i + 1) < max_len) {
		type = table[i];
		size = table[i + 1];
		if (type == LCD_EXT_CMD_TYPE_END)
			break;
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			EXTPR("[%d]: %s: dev[%d] step[%d]: type=0x%02x, size=%d\n",
				edrv->index, __func__, edev->dev_index,
				step, type, size);
		}
		if (size == 0)
			goto power_cmd_dynamic_simple_next;
		if ((i + 2 + size) > max_len)
			break;

		switch (type) {
		case LCD_EXT_CMD_TYPE_NONE:
			/* do nothing */
			break;
		case LCD_EXT_CMD_TYPE_GPIO:
			lcd_extern_cmd_gpio(edrv, edev, step, &table[i + 2], size);
			break;
		case LCD_EXT_CMD_TYPE_WAIT_GPIO:
			lcd_extern_cmd_wait_gpio(edrv, edev, step, &table[i + 2], size);
			break;
		case LCD_EXT_CMD_TYPE_DELAY:
			lcd_extern_cmd_delay(edrv, edev, step, &table[i + 2], size);
			break;
		case LCD_EXT_CMD_TYPE_MULTI_LIST_FR:
		case LCD_EXT_CMD_TYPE_MULTI_LIST_UFR:
			/* do nothing here */
			break;
		case LCD_EXT_CMD_TYPE_MULTI_DFT_CMD:
		case LCD_EXT_CMD_TYPE_MULTI_CMD:
			if (size < 2) {
				EXTERR("[%d]: dev[%d] step[%d]: invalid size %d for multi_cmd\n",
					edrv->index, edev->dev_index, step, size);
				goto power_cmd_dynamic_simple_next;
			}

			ret = lcd_extern_cmd_multi_id(edrv, edev, table[i + 2]);
			if (ret)
				goto power_cmd_dynamic_simple_next;
			if (type == LCD_EXT_CMD_TYPE_MULTI_DFT_CMD && edev->state == 0) {
				EXTPR("[%d]: dev[%d]: bypass init for multi dft cmd\n",
					edrv->index, edev->dev_index);
				goto power_cmd_dynamic_simple_next;
			}
			next_cmd = table[i + 3];
			if (next_cmd == LCD_EXT_CMD_TYPE_GPIO) {
				lcd_extern_cmd_gpio(edrv, edev, step, &table[i + 4], (size - 2));
			} else if (next_cmd == LCD_EXT_CMD_TYPE_WAIT_GPIO) {
				lcd_extern_cmd_wait_gpio(edrv, edev, step,
							 &table[i + 4], (size - 2));
			} else if (next_cmd == LCD_EXT_CMD_TYPE_DELAY) {
				lcd_extern_cmd_delay(edrv, edev, step, &table[i + 4], (size - 2));
			}
			break;
		default:
			EXTERR("[%d]: %s: %s(%d): type 0x%02x invalid\n",
				edrv->index, __func__,
				edev->config.name, edev->config.index, type);
			break;
		}
power_cmd_dynamic_simple_next:
		i += (size + 2);
		step++;
	}

	return 0;
}

static int lcd_extern_power_cmd_fixed_size_i2c(struct lcd_extern_driver_s *edrv,
		struct lcd_extern_dev_s *edev, unsigned char *table, int max_len)
{
	int i = 0, step = 0;
	unsigned char type, cmd_size;
	int ret = 0;

	if (!edrv || !edev || !table)
		return -1;

	cmd_size = edev->config.cmd_size;
	if (cmd_size < 2) {
		EXTERR("[%d]: %s: dev[%d] invalid cmd_size %d\n",
		       edrv->index, __func__, edev->dev_index, cmd_size);
		return -1;
	}

	while ((i + cmd_size) <= max_len) {
		type = table[i];
		if (type == LCD_EXT_CMD_TYPE_END)
			break;
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			EXTPR("[%d]: %s: dev[%d] step[%d]: type=0x%02x, cmd_size=%d\n",
				edrv->index, __func__,
				edev->dev_index, step, type, cmd_size);
		}
		switch (type) {
		case LCD_EXT_CMD_TYPE_NONE:
			/* do nothing */
			break;
		case LCD_EXT_CMD_TYPE_GPIO:
			lcd_extern_cmd_gpio(edrv, edev, step, &table[i + 1], cmd_size - 1);
			break;
		case LCD_EXT_CMD_TYPE_WAIT_GPIO:
			lcd_extern_cmd_wait_gpio(edrv, edev, step, &table[i + 1], (cmd_size - 1));
			break;
		case LCD_EXT_CMD_TYPE_DELAY:
			lcd_extern_cmd_delay(edrv, edev, step, &table[i + 1], cmd_size - 1);
			break;
		case LCD_EXT_CMD_TYPE_CMD:
		case LCD_EXT_CMD_TYPE_CMD2:
		case LCD_EXT_CMD_TYPE_CMD3:
		case LCD_EXT_CMD_TYPE_CMD4:
			ret = lcd_extern_cmd_i2c(edrv, edev, step,
				type, &table[i + 1], (cmd_size - 1));
			if (ret)
				goto power_cmd_fixed_i2c_err;
			break;
		case LCD_EXT_CMD_TYPE_CMD_DELAY:
		case LCD_EXT_CMD_TYPE_CMD2_DELAY:
		case LCD_EXT_CMD_TYPE_CMD3_DELAY:
		case LCD_EXT_CMD_TYPE_CMD4_DELAY:
			ret = lcd_extern_cmd_i2c(edrv, edev, step,
				type, &table[i + 1], (cmd_size - 2));
			if (ret)
				goto power_cmd_fixed_i2c_err;
			if (table[i + cmd_size - 1] > 0)
				lcd_delay_ms(table[i + cmd_size - 1]);
			break;
		default:
			EXTERR("[%d]: %s: %s(%d): type 0x%02x invalid\n",
				edrv->index, __func__,
				edev->config.name, edev->config.index, type);
			break;
		}
		i += cmd_size;
		step++;
	}

	return 0;

power_cmd_fixed_i2c_err:
	return -1;
}

static int lcd_extern_power_cmd_fixed_size_spi(struct lcd_extern_driver_s *edrv,
		struct lcd_extern_dev_s *edev, unsigned char *table, int max_len)
{
	int i = 0, step = 0;
	unsigned char type, cmd_size;
	int ret = 0;

	if (!edrv || !edev || !table)
		return -1;

	cmd_size = edev->config.cmd_size;
	if (cmd_size < 2) {
		EXTERR("[%d]: %s: dev[%d]: invalid cmd_size %d\n",
		       edrv->index, __func__, edev->dev_index, cmd_size);
		return -1;
	}

	while ((i + cmd_size) <= max_len) {
		type = table[i];
		if (type == LCD_EXT_CMD_TYPE_END)
			break;
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			EXTPR("[%d]: %s: dev[%d] step[%d]: type=0x%02x, cmd_size=%d\n",
				edrv->index, __func__,
				edev->dev_index, step, type, cmd_size);
		}
		switch (type) {
		case LCD_EXT_CMD_TYPE_NONE:
			/* do nothing */
			break;
		case LCD_EXT_CMD_TYPE_GPIO:
			lcd_extern_cmd_gpio(edrv, edev, step, &table[i + 1], cmd_size - 1);
			break;
		case LCD_EXT_CMD_TYPE_WAIT_GPIO:
			lcd_extern_cmd_wait_gpio(edrv, edev, step, &table[i + 1], (cmd_size - 1));
			break;
		case LCD_EXT_CMD_TYPE_DELAY:
			lcd_extern_cmd_delay(edrv, edev, step, &table[i + 1], cmd_size - 1);
			break;
		case LCD_EXT_CMD_TYPE_CMD:
			ret = lcd_extern_spi_write(edrv, edev, &table[i + 1], (cmd_size - 1));
			if (ret)
				goto power_cmd_fixed_spi_err;
			break;
		case LCD_EXT_CMD_TYPE_CMD_DELAY:
			ret = lcd_extern_spi_write(edrv, edev, &table[i + 1], (cmd_size - 2));
			if (ret)
				goto power_cmd_fixed_spi_err;
			if (table[i + cmd_size - 1] > 0)
				lcd_delay_ms(table[i + cmd_size - 1]);
			break;
		default:
			EXTERR("[%d]: %s: %s(%d): type 0x%02x invalid\n",
				edrv->index, __func__,
				edev->config.name, edev->config.index, type);
			break;
		}
		i += cmd_size;
		step++;
	}

	return 0;

power_cmd_fixed_spi_err:
	return -1;
}

static int lcd_extern_power_cmd_fixed_size_simple(struct lcd_extern_driver_s *edrv,
		struct lcd_extern_dev_s *edev, unsigned char *table, int max_len)
{
	int i = 0, step = 0;
	unsigned char type, cmd_size;

	if (!edrv || !edev || !table)
		return -1;

	cmd_size = edev->config.cmd_size;
	if (cmd_size < 2) {
		EXTERR("[%d]: %s: dev[%d] invalid cmd_size %d\n",
		       edrv->index, __func__, edev->dev_index, cmd_size);
		return -1;
	}

	while ((i + cmd_size) <= max_len) {
		type = table[i];
		if (type == LCD_EXT_CMD_TYPE_END)
			break;
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			EXTPR("[%d]: %s: dev[%d] step[%d]: type=0x%02x, cmd_size=%d\n",
				edrv->index, __func__,
				edev->dev_index, step, type, cmd_size);
		}
		switch (type) {
		case LCD_EXT_CMD_TYPE_NONE:
			/* do nothing */
			break;
		case LCD_EXT_CMD_TYPE_GPIO:
			lcd_extern_cmd_gpio(edrv, edev, step, &table[i + 1], cmd_size - 1);
			break;
		case LCD_EXT_CMD_TYPE_WAIT_GPIO:
			lcd_extern_cmd_wait_gpio(edrv, edev, step, &table[i + 1], (cmd_size - 1));
			break;
		case LCD_EXT_CMD_TYPE_DELAY:
			lcd_extern_cmd_delay(edrv, edev, step, &table[i + 1], cmd_size - 1);
			break;
		default:
			EXTERR("[%d]: %s: %s(%d): type 0x%02x invalid\n",
				edrv->index, __func__,
				edev->config.name, edev->config.index, type);
			break;
		}
		i += cmd_size;
		step++;
	}

	return 0;
}

int lcd_extern_power_cmd(struct lcd_extern_driver_s *edrv, struct lcd_extern_dev_s *edev, int flag)
{
	unsigned char *table;
	int max_len = 0, ret = -1;

	if (!edrv || !edev)
		return -1;

	if (edev->config.cmd_size < 2) {
		EXTERR("[%d]: %s: dev[%d]: invalid cmd_size %d\n",
		       edrv->index, __func__, edev->dev_index, edev->config.cmd_size);
		return -1;
	}

	if (flag) {
		table = edev->config.table_init_on;
		max_len = edev->config.table_init_on_cnt;
	} else {
		table = edev->config.table_init_off;
		max_len = edev->config.table_init_off_cnt;
	}

	if (!table) {
		EXTERR("[%d]: %s: dev[%d] init_%s is NULL\n",
		       edrv->index, __func__, edev->dev_index, flag ? "on" : "off");
		return -1;
	}

	edev->check_state = 0;
	edev->check_step = 0;
	edev->check_execute = 0;
	switch (edev->config.type) {
	case LCD_EXTERN_I2C:
		if (edev->config.cmd_size == LCD_EXT_CMD_SIZE_DYNAMIC)
			ret = lcd_extern_power_cmd_dynamic_size_i2c(edrv, edev, table, max_len);
		else
			ret = lcd_extern_power_cmd_fixed_size_i2c(edrv, edev, table, max_len);
		break;
	case LCD_EXTERN_SPI:
		if (edev->config.cmd_size == LCD_EXT_CMD_SIZE_DYNAMIC)
			ret = lcd_extern_power_cmd_dynamic_size_spi(edrv, edev, table, max_len);
		else
			ret = lcd_extern_power_cmd_fixed_size_spi(edrv, edev, table, max_len);
		break;
	case LCD_EXTERN_SIMPLE:
		if (edev->config.cmd_size == LCD_EXT_CMD_SIZE_DYNAMIC)
			ret = lcd_extern_power_cmd_dynamic_size_simple(edrv, edev, table, max_len);
		else
			ret = lcd_extern_power_cmd_fixed_size_simple(edrv, edev, table, max_len);
		break;
		break;
	default:
		EXTERR("[%d]: %s: %s(%d): extern_type %d is not support\n",
		       edrv->index, __func__,
		       edev->config.name, edev->config.index, edev->config.type);
		break;
	}

	return ret;
}

