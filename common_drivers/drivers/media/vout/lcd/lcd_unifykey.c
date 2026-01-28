// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/reset.h>
#include <linux/amlogic/media/vout/lcd/lcd_unifykey.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/lcd/lcd_model.h>

#define LCDUKEY(fmt, args...)     pr_info("lcd: ukey: " fmt "", ## args)
#define LCDUKEYERR(fmt, args...)  pr_info("lcd: ukey error: " fmt "", ## args)

#ifdef CONFIG_AMLOGIC_UNIFYKEY
bool lcd_unifykey_init_get(void)
{
	if (is_ukey_in_param_mem()) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDUKEY("ukey in panel param mem, init ok\n");
		return true;
	}

	if (key_unify_get_init_flag())
		return true;
	return false;
}

int lcd_unifykey_len_check(int key_len, int len)
{
	if (key_len < len) {
		LCDUKEYERR("invalid unifykey length %d, need %d\n",
			   key_len, len);
		return -1;
	}
	return 0;
}

int lcd_unifykey_check(char *key_name)
{
	unsigned int key_exist = 0, keypermit;
	int ret;
	unsigned int size;

	if (!key_name) {
		LCDUKEYERR("%s: key_name is null\n", __func__);
		return -1;
	}

	if (is_ukey_in_param_mem())
		return panel_param_mem_get(key_name, &size) ? 0 : -1;

	ret = key_unify_query(get_ukdev(), key_name, &key_exist, &keypermit);
	if (ret < 0) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDUKEYERR("%s: %s query exist error\n", __func__, key_name);
		return -1;
	}
	if (key_exist == 0) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDUKEYERR("%s: %s is not exist\n", __func__, key_name);
		return -1;
	}

	return 0;
}

int lcd_unifykey_get_size(char *key_name, int *len)
{
	int key_len;
	int ret;
	unsigned int size = 0;

	if (is_ukey_in_param_mem()) {
		panel_param_mem_get(key_name, &size);
		if (size) {
			*len = (int)size;
			return 0;
		} else {
			return -1;
		}
	}

	key_len = 0;
	ret = lcd_unifykey_check(key_name);
	if (ret < 0)
		return -1;
	ret = key_unify_size(get_ukdev(), key_name, &key_len);
	if (ret < 0)
		return -1;
	if (key_len == 0) {
		LCDUKEYERR("%s: %s size 0!\n", __func__, key_name);
		return -1;
	}
	*len = key_len;
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDUKEY("%s: %s size: 0x%x\n", __func__, key_name, *len);

	return 0;
}

int lcd_unifykey_get(char *key_name, unsigned char *buf, int len)
{
	struct aml_lcd_unifykey_header_s *key_header;
	unsigned int retry_cnt = 0, key_crc32;
	int key_len = 0;
	int ret;
	unsigned char *mem;
	unsigned int size = 0;

	if (is_ukey_in_param_mem()) {
		mem = panel_param_mem_get(key_name, &size);
		if (!mem || !size || len < size)
			return -1;
		memcpy(buf, mem, size);
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDUKEY("%s %s from panel_param_mem, size:%d\n", __func__, key_name, size);
		return 0;
	}

	ret = lcd_unifykey_check(key_name);
	if (ret < 0)
		return -1;

lcd_unifykey_get_retry:
	ret = key_unify_read(get_ukdev(), key_name, buf, len, &key_len);
	if (ret < 0) {
		LCDUKEYERR("%s: %s error\n", __func__, key_name);
		return -1;
	}
	if (key_len != len) {
		LCDUKEYERR("%s: %s key_len(0x%x) and buf_size(0x%x) mismatch\n",
			__func__, key_name, key_len, len);
		return -1;
	}

	/* check header */
	if (key_len <= LCD_UKEY_HEAD_SIZE) {
		LCDUKEYERR("%s: %s key_len %d error\n", __func__, key_name, key_len);
		return -1;
	}
	key_header = (struct aml_lcd_unifykey_header_s *)buf;
	if (key_len != key_header->data_len) {  /* length check */
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDUKEYERR("%s: %s data_len %d is not match key_len %d\n",
				   __func__, key_name, key_header->data_len, key_len);
		}
		if (retry_cnt++ < LCD_UKEY_RETRY_CNT_MAX) {
			memset(buf, 0, key_len);
			goto lcd_unifykey_get_retry;
		}
		LCDUKEYERR("%s: %s failed\n", __func__, key_name);
		return -1;
	}
	key_crc32 = cal_CRC32(0, &buf[4], (key_len - 4)); /* except crc32 */
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDUKEY("%s: %s crc32: 0x%08x, header_crc32: 0x%08x\n",
			__func__, key_name, key_crc32, key_header->crc32);
	}
	if (key_crc32 != key_header->crc32) {  /* crc32 check */
		LCDUKEYERR("%s: %s crc32 0x%08x is not match header_crc32 0x%08x\n",
			   __func__, key_name, key_crc32, key_header->crc32);
		if (retry_cnt++ < LCD_UKEY_RETRY_CNT_MAX) {
			memset(buf, 0, key_len);
			goto lcd_unifykey_get_retry;
		}
		LCDUKEYERR("%s: %s failed\n", __func__, key_name);
		return -1;
	}

	return 0;
}

int lcd_unifykey_get_tcon(char *key_name, unsigned char *buf, int len)
{
	struct lcd_tcon_init_block_header_s *init_header;
	unsigned int retry_cnt = 0, key_crc32;
	int key_len = 0;
	int ret;
	unsigned char *mem;
	unsigned int size = 0;

	if (is_ukey_in_param_mem()) {
		mem = panel_param_mem_get(key_name, &size);
		if (!mem || !size || len < size)
			return -1;
		memcpy(buf, mem, size);
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDUKEY("%s %s from panel_param_mem, size:%d\n", __func__, key_name, size);
		return 0;
	}

	ret = lcd_unifykey_check(key_name);
	if (ret < 0)
		return -1;

lcd_unifykey_get_tcon_retry:
	ret = key_unify_read(get_ukdev(), key_name, buf, len, &key_len);
	if (ret < 0) {
		LCDUKEYERR("%s: %s error\n", __func__, key_name);
		return -1;
	}
	if (key_len != len) {
		LCDUKEYERR("%s: %s key_len(0x%x) and buf_size(0x%x) mismatch\n",
			__func__, key_name, key_len, len);
		return -1;
	}

	/* check header */
	if (key_len <= LCD_TCON_DATA_BLOCK_HEADER_SIZE) {
		LCDUKEYERR("%s: %s key_len %d error\n", __func__, key_name, key_len);
		return -1;
	}
	init_header = (struct lcd_tcon_init_block_header_s *)buf;
	if (key_len != init_header->block_size) {  /* length check */
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDUKEYERR("%s: %s block_size %d is not match key_len %d\n",
				   __func__, key_name, init_header->block_size, key_len);
		}
		if (retry_cnt++ < LCD_UKEY_RETRY_CNT_MAX) {
			memset(buf, 0, key_len);
			goto lcd_unifykey_get_tcon_retry;
		}
		LCDUKEYERR("%s: %s failed\n", __func__, key_name);
		return -1;
	}
	key_crc32 = cal_CRC32(0, &buf[4], (key_len - 4)); /* except crc32 */
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDUKEY("%s: %s crc32: 0x%08x, header_crc32: 0x%08x\n",
			__func__, key_name, key_crc32, init_header->crc32);
	}
	if (key_crc32 != init_header->crc32) {  /* crc32 check */
		LCDUKEYERR("%s: %s crc32 0x%08x is not match header_crc32 0x%08x\n",
			   __func__, key_name, key_crc32, init_header->crc32);
		if (retry_cnt++ < LCD_UKEY_RETRY_CNT_MAX) {
			memset(buf, 0, key_len);
			goto lcd_unifykey_get_tcon_retry;
		}
		LCDUKEYERR("%s: %s failed\n", __func__, key_name);
		return -1;
	}

	return 0;
}

int lcd_unifykey_get_no_header(char *key_name, unsigned char *buf, int len)
{
	int key_len = 0;
	int ret;
	unsigned char *mem;
	unsigned int size = 0;

	if (is_ukey_in_param_mem()) {
		mem = panel_param_mem_get(key_name, &size);
		if (!mem || !size || len < size)
			return -1;
		memcpy(buf, mem, size);
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDUKEY("%s %s from panel_param_mem, size:%d\n", __func__, key_name, size);
		return 0;
	}

	ret = lcd_unifykey_check(key_name);
	if (ret < 0)
		return -1;

	ret = key_unify_read(get_ukdev(), key_name, buf, len, &key_len);
	if (ret < 0) {
		LCDUKEYERR("%s: %s error\n", __func__, key_name);
		return -1;
	}
	if (key_len != len) {
		LCDUKEYERR("%s: %s key_len(0x%x) and buf_size(0x%x) mismatch\n",
			__func__, key_name, key_len, len);
		return -1;
	}

	return 0;
}

#else
/* dummy driver */
bool lcd_unifykey_init_get(void)
{
	if (is_ukey_in_param_mem()) {
		LCDUKEY("ukey in panel param mem, init ok\n");
		return true;
	}

	return false;
}

int lcd_unifykey_len_check(int key_len, int len)
{
	if (key_len < len) {
		LCDUKEYERR("invalid unifykey length %d, need %d\n",
			   key_len, len);
		return -1;
	}
	return 0;
}

int lcd_unifykey_check(char *key_name)
{
	unsigned int size;

	if (!key_name) {
		LCDUKEYERR("%s: key_name is null\n", __func__);
		return -1;
	}

	if (is_ukey_in_param_mem())
		return panel_param_mem_get(key_name, &size) ? 0 : -1;

	return -1;
}

int lcd_unifykey_get_size(char *key_name, int *len)
{
	unsigned int size = 0;

	if (is_ukey_in_param_mem()) {
		panel_param_mem_get(key_name, &size);
		printf("%s: %s size:%d\n", __func__, key_name, size);
		if (size) {
			*len = (int)size;
			return 0;
		} else {
			return -1;
		}
	}

	return -1;
}

int lcd_unifykey_get(char *key_name, unsigned char *buf, int len)
{
	unsigned char *mem;
	unsigned int size = 0;

	if (is_ukey_in_param_mem()) {
		mem = panel_param_mem_get(key_name, &size);
		if (!mem || !size || len < size)
			return -1;
		memcpy(buf, mem, size);
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDUKEY("%s %s from panel_param_mem, size:%d\n", __func__, key_name, size);
		return 0;
	}

	return -1;
}

int lcd_unifykey_get_tcon(char *key_name, unsigned char *buf, int len)
{
	unsigned char *mem;
	unsigned int size = 0;

	if (is_ukey_in_param_mem()) {
		mem = panel_param_mem_get(key_name, &size);
		if (!mem || !size || len < size)
			return -1;
		memcpy(buf, mem, size);
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDUKEY("%s %s from panel_param_mem, size:%d\n", __func__, key_name, size);
		return 0;
	}

	return -1;
}

int lcd_unifykey_get_no_header(char *key_name, unsigned char *buf, int len)
{
	unsigned char *mem;
	unsigned int size = 0;

	if (is_ukey_in_param_mem()) {
		mem = panel_param_mem_get(key_name, &size);
		if (!mem || !size || len < size)
			return -1;
		memcpy(buf, mem, size);
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDUKEY("%s %s from panel_param_mem, size:%d\n", __func__, key_name, size);
		return 0;
	}

	return -1;
}

#endif
void lcd_unifykey_header_print(unsigned char *buf)
{
	struct aml_lcd_unifykey_header_s *header;

	if (!buf)
		return;
	header = (struct aml_lcd_unifykey_header_s *)buf;
	LCDUKEY("unifykey v%d header:\n", header->version);
	LCDUKEY("version           = 0x%04x\n", header->version);
	LCDUKEY("crc32             = 0x%08x\n", header->crc32);
	LCDUKEY("data_len          = %d\n", header->data_len);
	LCDUKEY("block_next_flag   = %d\n", header->block_next_flag);
	LCDUKEY("block_cur_size    = %d\n", header->block_cur_size);
}

static void lcd_unifykey_data_dump(char *pr_buf, char *key_name, unsigned int key_len,
		unsigned char *key_buf)
{
	int i, j, pr_len;

	if (!pr_buf || !key_name) {
		LCDUKEYERR("%s: pr_buf or key_name is NULL\n", __func__);
		return;
	}

	LCDUKEY("print: %s: %d\n", key_name, key_len);
	for (i = 0; i < key_len; i += 16) {
		pr_len = sprintf(pr_buf, "0x%04x:", i);
		for (j = 0; j < 16; j++) {
			if ((i + j) >= key_len)
				break;
			pr_len += sprintf(pr_buf + pr_len, " %02x", key_buf[i + j]);
		}
		pr_info("%s\n", pr_buf);
	}
}

void lcd_unifykey_print(int index)
{
	unsigned char *buf, *pr_buf;
	char key_name[32];
	unsigned int key_len;
	int ret;

	pr_buf = kcalloc(128, sizeof(unsigned char), GFP_KERNEL);
	if (!pr_buf) {
		LCDUKEY("%s: buf malloc error\n", __func__);
		return;
	}

	if (index > 0)
		sprintf(key_name, "lcd%d", index);
	else
		sprintf(key_name, "lcd");
	ret = lcd_unifykey_get_size(key_name, &key_len);
	if (ret)
		goto lcd_ukey_print_next1;
	buf = kcalloc(key_len, sizeof(unsigned char), GFP_KERNEL);
	if (!buf) {
		LCDUKEY("%s: buf malloc error\n", __func__);
		kfree(pr_buf);
		return;
	}
	ret = lcd_unifykey_get(key_name, buf, key_len);
	if (ret < 0) {
		kfree(buf);
		goto lcd_ukey_print_next1;
	}
	lcd_unifykey_data_dump(pr_buf, key_name, key_len, buf);
	kfree(buf);

lcd_ukey_print_next1:
	if (index > 0)
		sprintf(key_name, "lcd%d_extern", index);
	else
		sprintf(key_name, "lcd_extern");
	ret = lcd_unifykey_get_size(key_name, &key_len);
	if (ret)
		goto lcd_ukey_print_next2;
	buf = kcalloc(key_len, sizeof(unsigned char), GFP_KERNEL);
	if (!buf) {
		LCDUKEY("%s: buf malloc error\n", __func__);
		kfree(pr_buf);
		return;
	}
	ret = lcd_unifykey_get(key_name, buf, key_len);
	if (ret < 0) {
		kfree(buf);
		goto lcd_ukey_print_next2;
	}
	lcd_unifykey_data_dump(pr_buf, key_name, key_len, buf);
	kfree(buf);

lcd_ukey_print_next2:
	if (index > 0)
		sprintf(key_name, "backlight%d", index);
	else
		sprintf(key_name, "backlight");
	ret = lcd_unifykey_get_size(key_name, &key_len);
	if (ret)
		goto lcd_ukey_print_next3;
	buf = kcalloc(key_len, sizeof(unsigned char), GFP_KERNEL);
	if (!buf) {
		LCDUKEY("%s: buf malloc error\n", __func__);
		kfree(pr_buf);
		return;
	}
	ret = lcd_unifykey_get(key_name, buf, key_len);
	if (ret < 0) {
		kfree(buf);
		goto lcd_ukey_print_next3;
	}
	lcd_unifykey_data_dump(pr_buf, key_name, key_len, buf);
	kfree(buf);

lcd_ukey_print_next3:
	kfree(pr_buf);
}

