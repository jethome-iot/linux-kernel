// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/major.h>
#include <linux/compat.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/reset.h>
#include <linux/of_reserved_mem.h>
#include <linux/cma.h>
#include <linux/dma-map-ops.h>
#include <linux/dma-mapping.h>
#include <linux/sched/clock.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/lcd/lcd_unifykey.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#include <linux/amlogic/media/vout/lcd/lcd_tcon_fw.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include "lcd_common.h"
#include "lcd_reg.h"
#include "lcd_tcon.h"

static unsigned int lcd_tcon_fw_reg_read(struct lcd_tcon_fw_s *fw, unsigned int bit_flag,
				unsigned int reg)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)fw->drvdat;

	if (!pdrv)
		return 0;

	if (bit_flag == 8)
		return lcd_tcon_read_byte(pdrv, reg);
	return lcd_tcon_read(pdrv, reg);
}

static void lcd_tcon_fw_reg_write(struct lcd_tcon_fw_s *fw, unsigned int bit_flag,
				unsigned int reg, unsigned int value)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)fw->drvdat;

	if (!pdrv)
		return;

	if (bit_flag == 8)
		lcd_tcon_write_byte(pdrv, reg, value);
	else
		lcd_tcon_write(pdrv, reg, value);
}

static void lcd_tcon_fw_reg_setb(struct lcd_tcon_fw_s *fw, unsigned int bit_flag,
				unsigned int reg, unsigned int value,
				unsigned int start, unsigned int len)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)fw->drvdat;

	if (!pdrv)
		return;

	if (bit_flag == 8)
		lcd_tcon_setb_byte(pdrv, reg, value, start, len);
	else
		lcd_tcon_setb(pdrv, reg, value, start, len);
}

static unsigned int lcd_tcon_fw_reg_getb(struct lcd_tcon_fw_s *fw, unsigned int bit_flag,
				unsigned int reg, unsigned int start, unsigned int len)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)fw->drvdat;

	if (!pdrv)
		return 0;

	if (bit_flag == 8)
		return lcd_tcon_getb_byte(pdrv, reg, start, len);
	return lcd_tcon_getb(pdrv, reg, start, len);
}

static void lcd_tcon_fw_reg_update_bits(struct lcd_tcon_fw_s *fw, unsigned int bit_flag,
				unsigned int reg, unsigned int mask, unsigned int value)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)fw->drvdat;

	if (!pdrv)
		return;

	if (bit_flag == 8)
		lcd_tcon_update_bits_byte(pdrv, reg, mask, value);
	else
		lcd_tcon_update_bits(pdrv, reg, mask, value);
}

static int lcd_tcon_fw_reg_check_bits(struct lcd_tcon_fw_s *fw, unsigned int bit_flag,
				unsigned int reg, unsigned int mask, unsigned int value)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)fw->drvdat;

	if (!pdrv)
		return 0;

	if (bit_flag == 8)
		return lcd_tcon_check_bits_byte(pdrv, reg, mask, value);
	return lcd_tcon_check_bits(pdrv, reg, mask, value);
}

static unsigned int lcd_tcon_fw_get_encl_line_cnt(struct lcd_tcon_fw_s *fw)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)fw->drvdat;

	if (!pdrv)
		return 0;

	return lcd_get_encl_line_cnt(pdrv);
}

static unsigned int lcd_tcon_fw_get_encl_frm_cnt(struct lcd_tcon_fw_s *fw)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)fw->drvdat;

	if (!pdrv)
		return 0;

	return lcd_get_encl_frm_cnt(pdrv);
}

static struct tcon_fw_config_s lcd_tcon_fw_config = {
	.config_size = sizeof(struct tcon_fw_config_s),
	.chip_type = TCON_CHIP_MAX,
	.if_type = TCON_IF_TYPE_MAX,
	.axi_cnt = 0,
	.axi_rmem = NULL,
};

static struct tcon_fw_base_timing_s lcd_tcon_fw_base_timing;

static struct lcd_tcon_fw_s lcd_tcon_fw = {
	/* init by driver */
	.para_ver = TCON_FW_PARA_VER,
	.para_size = sizeof(struct lcd_tcon_fw_s),
	.valid = 0,
	.flag = 0, //for some state update
	.tcon_state = 0,

	/* init by fw */
	.fw_ready = 0,
	.fw_ver = "not installed",
	.dbg_level = 0,

	/* driver resource */
	.drvdat = NULL,
	.dev = NULL,
	.config = &lcd_tcon_fw_config,
	.base_timing = &lcd_tcon_fw_base_timing,

	/* fw resource */
	.buf_table_in = NULL,
	.buf_table_out = NULL,

	/* API by driver */
	.reg_read = lcd_tcon_fw_reg_read,
	.reg_write = lcd_tcon_fw_reg_write,
	.reg_setb = lcd_tcon_fw_reg_setb,
	.reg_getb = lcd_tcon_fw_reg_getb,
	.reg_update_bits = lcd_tcon_fw_reg_update_bits,
	.reg_check_bits = lcd_tcon_fw_reg_check_bits,

	.get_encl_line_cnt = lcd_tcon_fw_get_encl_line_cnt,
	.get_encl_frm_cnt = lcd_tcon_fw_get_encl_frm_cnt,

	/* API by fw */
	.vsync_isr = NULL,
	.tcon_alg = NULL,
	.debug_show = NULL,
	.debug_store = NULL,
};

struct lcd_tcon_fw_s *aml_lcd_tcon_get_fw(void)
{
	return &lcd_tcon_fw;
}
EXPORT_SYMBOL(aml_lcd_tcon_get_fw);

/* buf_table:
 * ....top_header
 * ........crc32: only include top_header+index
 * ........data_size: only include top_header+index
 * ....index(buf0~n addr pointer)
 * ....buf[0](header+data)
 * ........crc32: only buf[0] header+data
 * ........data_size: only buf[0] header+data
 * ............
 * ....buf[n]
 */
int lcd_tcon_fw_buf_table_generate(struct lcd_tcon_fw_s *tcon_fw)
{
	return 0;
}

void lcd_tcon_fw_base_timing_update(struct aml_lcd_drv_s *pdrv)
{
	if (!pdrv || !lcd_tcon_fw.base_timing)
		return;

	lcd_tcon_fw.base_timing->hsize = pdrv->config.timing.act_timing.h_active;
	lcd_tcon_fw.base_timing->vsize = pdrv->config.timing.act_timing.v_active;
	lcd_tcon_fw.base_timing->htotal = pdrv->config.timing.act_timing.h_period;
	lcd_tcon_fw.base_timing->vtotal = pdrv->config.timing.act_timing.v_period;
	lcd_tcon_fw.base_timing->frame_rate = pdrv->config.timing.act_timing.frame_rate;
	lcd_tcon_fw.base_timing->pclk = pdrv->config.timing.act_timing.pixel_clk;
	lcd_tcon_fw.base_timing->bit_rate = pdrv->config.timing.bit_rate;

	lcd_tcon_fw.base_timing->de_hstart = pdrv->config.timing.hstart;
	lcd_tcon_fw.base_timing->de_hend = pdrv->config.timing.hend;
	lcd_tcon_fw.base_timing->de_vstart = pdrv->config.timing.vstart;
	lcd_tcon_fw.base_timing->de_vend = pdrv->config.timing.vend;
	lcd_tcon_fw.base_timing->pre_de_h = 8;
	lcd_tcon_fw.base_timing->pre_de_v = 8;

	lcd_tcon_fw.base_timing->hsw = pdrv->config.timing.act_timing.hsync_width;
	lcd_tcon_fw.base_timing->hbp = pdrv->config.timing.act_timing.hsync_bp;
	lcd_tcon_fw.base_timing->hfp = pdrv->config.timing.act_timing.hsync_fp;

	lcd_tcon_fw.base_timing->vsw = pdrv->config.timing.act_timing.vsync_width;
	lcd_tcon_fw.base_timing->vbp = pdrv->config.timing.act_timing.vsync_bp;
	lcd_tcon_fw.base_timing->vfp = pdrv->config.timing.act_timing.vsync_fp;

	lcd_tcon_fw.base_timing->update_flag = 1;
}

int lcd_tcon_fw_add_core_table(struct lcd_tcon_fw_s *fw, unsigned char *core_table)
{
	struct lcd_tcon_config_s *tcon_cfg = get_lcd_tcon_config();
	struct tcon_fw_core_reg_s *fw_core = NULL;
	struct lcd_tcon_init_block_header_s *init_header = NULL;
	struct lcd_tcon_init_block_ext_header_s *ext_header = NULL;
	char str[64] = "";
	int ret = -1;

	if (!fw || !fw->config || !core_table || !tcon_cfg)
		goto __add_core_table_exit;

	fw_core = kzalloc(sizeof(*fw_core), GFP_KERNEL);
	if (!fw_core)
		goto __add_core_table_exit;

	init_header = (struct lcd_tcon_init_block_header_s *)core_table;
	if (init_header->ext_header_size) {
		ext_header = (struct lcd_tcon_init_block_ext_header_s *)
			(core_table + init_header->header_size);
	}

	fw_core->full_table = kzalloc(init_header->block_size, GFP_KERNEL);
	if (!fw_core->full_table)
		goto __add_core_table_exit;

	memmove(fw_core->full_table, core_table, init_header->block_size);
	fw_core->core_reg_size = tcon_cfg->reg_table_len;
	fw_core->init_header = (struct lcd_tcon_init_block_header_s *)fw_core->full_table;
	if (init_header->ext_header_size) {
		fw_core->ext_header = (struct lcd_tcon_init_block_ext_header_s *)
			(fw_core->full_table + init_header->header_size);
	}
	fw_core->core_reg = fw_core->full_table + init_header->header_size
		+ init_header->ext_header_size;

	list_add_tail(&fw_core->list, &fw->config->core_reg_list);
	ret = 0;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		memset(str, 0, sizeof(str));
		memcpy(str, init_header->version, LCD_TCON_INIT_BIN_VERSION_SIZE);
		LCDPR("Add core reg info:\n");
		LCDPR("  name       = %s\n", init_header->name);
		LCDPR("  version    = %s\n", str);
		LCDPR("  block_size = %#x (%d)\n", init_header->block_size,
			init_header->block_size);
		LCDPR("  resolution = %dx%d\n", init_header->h_active, init_header->v_active);
		if (ext_header) {
			LCDPR("  fr range   = %d~%d\n",
				ext_header->framerate_min, ext_header->framerate_max);
		}
		LCDPR("  crc        = %#x\n", init_header->crc32);
	}

__add_core_table_exit:
	if (ret) {
		if (fw_core)
			kfree(fw_core->full_table);
		kfree(fw_core);
	}

	return ret;
}

void lcd_tcon_fw_remove_core_table(struct lcd_tcon_fw_s *fw)
{
	struct tcon_fw_core_reg_s *fw_core, *core_n;

	if (!fw || !fw->config)
		return;

	list_for_each_entry_safe(fw_core, core_n, &fw->config->core_reg_list, list) {
		kfree(fw_core->full_table);
		list_del(&fw_core->list);
		kfree(fw_core);
	}
}

int lcd_tcon_fw_core_table_cnt(struct lcd_tcon_fw_s *fw)
{
	struct list_head *cur_list = NULL;
	int cnt = 0;

	if (!fw || !fw->config)
		return 0;

	list_for_each(cur_list, &fw->config->core_reg_list)
		cnt++;

	return cnt;
}

static int lcd_tcon_fw_alloc_core_reg(struct lcd_tcon_fw_s *fw,
		struct tcon_fw_core_reg_s *fw_core_reg)
{
	struct tcon_mem_map_table_s *mm_table = get_lcd_tcon_mm_table();
	struct lcd_tcon_init_block_header_s *core_header = NULL;
	int ret = -1;

	if (!fw || !fw_core_reg || !mm_table)
		goto __alloc_mem_by_reg_exit;
	core_header = fw_core_reg->init_header;

	if (!mm_table->core_reg_header && core_header) {
		mm_table->core_reg_header = kzalloc(core_header->header_size,
			GFP_KERNEL);
		if (!mm_table->core_reg_header) {
			LCDERR("%s: mm_table alloc core header fail\n", __func__);
			goto __alloc_mem_by_reg_exit;
		}
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("%s: mm_table alloc core header\n", __func__);
	}

	if (!mm_table->core_reg_ext_header && core_header &&
			core_header->ext_header_size) {
		mm_table->core_reg_ext_header = kzalloc(core_header->ext_header_size,
			GFP_KERNEL);
		if (!mm_table->core_reg_ext_header) {
			LCDERR("%s: mm_table alloc core ext header fail\n", __func__);
			goto __alloc_mem_by_reg_exit;
		}
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("%s: mm_table alloc core ext header\n", __func__);
	}

	if (!mm_table->core_reg_table) {
		mm_table->core_reg_table = kzalloc(mm_table->core_reg_table_size,
			GFP_KERNEL);
		if (!mm_table->core_reg_table) {
			LCDERR("%s: mm_table alloc core reg table fail\n", __func__);
			goto __alloc_mem_by_reg_exit;
		}
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("%s: mm_table alloc core table\n", __func__);
	}
	ret = 0;

__alloc_mem_by_reg_exit:
	return ret;
}

void lcd_tcon_fw_update_core(struct lcd_tcon_fw_s *fw)
{
	struct aml_lcd_drv_s *pdrv = NULL;
	struct tcon_mem_map_table_s *mm_table = get_lcd_tcon_mm_table();
	struct tcon_fw_core_reg_s *core_reg = NULL;
	struct tcon_fw_core_reg_s *match_reg = NULL;
	struct lcd_detail_timing_s *dft_timing = NULL;
	struct lcd_tcon_init_block_header_s *init_header;
	struct lcd_tcon_init_block_ext_header_s *ext_header;

	if (!fw || !mm_table)
		return;

	pdrv = (struct aml_lcd_drv_s *)fw->drvdat;
	dft_timing = pdrv ? pdrv->config.timing.dft_timing : NULL;
	if (!pdrv || !dft_timing)
		return;

	list_for_each_entry(core_reg, &fw->config->core_reg_list, list) {
		init_header = core_reg->init_header;
		ext_header = core_reg->ext_header;

		//find match binary and add to core reg
		if (dft_timing->h_active == core_reg->init_header->h_active &&
				dft_timing->v_active == core_reg->init_header->v_active) {
			if (ext_header) {
				if (dft_timing->frame_rate < ext_header->framerate_min ||
				    dft_timing->frame_rate > ext_header->framerate_max)
					continue;
			}
			match_reg = core_reg;
			LCDPR("%s: match %dx%d@%dhz\n", __func__, init_header->h_active,
				init_header->v_active, dft_timing->frame_rate);
			break;
		}
	}

	//step 3. update core reg
	if (match_reg) {
		lcd_tcon_fw_alloc_core_reg(fw, match_reg);
		if (mm_table->core_reg_header) {
			memcpy(mm_table->core_reg_header, match_reg->init_header,
				mm_table->core_reg_header->header_size);
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
				LCDPR("%s: core reg header updated\n", __func__);
		}
		if (mm_table->core_reg_ext_header &&
				match_reg->init_header) {
			memcpy(mm_table->core_reg_ext_header, match_reg->ext_header,
				match_reg->init_header->ext_header_size);
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
				LCDPR("%s: core reg ext header updated\n", __func__);
		}
		if (mm_table->core_reg_table) {
			memcpy(mm_table->core_reg_table, match_reg->core_reg,
				mm_table->core_reg_table_size);
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
				LCDPR("%s: core reg table updated\n", __func__);
		}
	}
}

void lcd_tcon_fw_prepare(struct aml_lcd_drv_s *pdrv, struct lcd_tcon_config_s *tcon_conf)
{
	struct tcon_rmem_s *tcon_rmem = get_lcd_tcon_rmem();
	struct tcon_mem_map_table_s *mm_table = get_lcd_tcon_mm_table();
	int i;

	if (!lcd_tcon_fw.config)
		return;
	if (!pdrv || !pdrv->data || !tcon_conf || !tcon_rmem || !mm_table)
		return;

	switch (pdrv->data->chip_type) {
	case LCD_CHIP_TL1:
		lcd_tcon_fw.config->chip_type = TCON_CHIP_TL1;
		break;
	case LCD_CHIP_TM2:
	case LCD_CHIP_TM2B:
		lcd_tcon_fw.config->chip_type = TCON_CHIP_TM2;
		break;
	case LCD_CHIP_T5:
		lcd_tcon_fw.config->chip_type = TCON_CHIP_T5;
		break;
	case LCD_CHIP_T5D:
		lcd_tcon_fw.config->chip_type = TCON_CHIP_T5D;
		break;
	case LCD_CHIP_T3:
		lcd_tcon_fw.config->chip_type = TCON_CHIP_T3;
		break;
	case LCD_CHIP_T5W:
		lcd_tcon_fw.config->chip_type = TCON_CHIP_T5W;
		break;
	case LCD_CHIP_T5M:
		lcd_tcon_fw.config->chip_type = TCON_CHIP_T5M;
		break;
	case LCD_CHIP_T3X:
		lcd_tcon_fw.config->chip_type = TCON_CHIP_T3X;
		break;
	case LCD_CHIP_TXHD2:
		lcd_tcon_fw.config->chip_type = TCON_CHIP_TXHD2;
		break;
	case LCD_CHIP_T6D:
		lcd_tcon_fw.config->chip_type = TCON_CHIP_T6D;
		break;
	default:
		break;
	}
	switch (pdrv->config.basic.lcd_type) {
	case LCD_MLVDS:
		lcd_tcon_fw.config->if_type = TCON_IF_MLVDS;
		break;
	case LCD_P2P:
		lcd_tcon_fw.config->if_type = TCON_IF_P2P;
		break;
	default:
		break;
	}

	lcd_tcon_fw.config->axi_cnt = tcon_conf->axi_bank;
	if (!tcon_rmem->axi_rmem)
		return;
	lcd_tcon_fw.config->axi_rmem = kcalloc(lcd_tcon_fw.config->axi_cnt,
		sizeof(struct tcon_fw_axi_rmem_s), GFP_KERNEL);
	for (i = 0; i < lcd_tcon_fw.config->axi_cnt; i++) {
		lcd_tcon_fw.config->axi_rmem[i].mem_paddr = tcon_rmem->axi_rmem[i].mem_paddr;
		lcd_tcon_fw.config->axi_rmem[i].mem_size = tcon_rmem->axi_rmem[i].mem_size;
	}

	INIT_LIST_HEAD(&lcd_tcon_fw.config->core_reg_list);

	if (pdrv->status & LCD_STATUS_IF_ON)
		lcd_tcon_fw.tcon_state |= TCON_FW_STATE_TCON_EN;
	if (mm_table->lut_valid_flag & LCD_TCON_DATA_VALID_VAC)
		lcd_tcon_fw.tcon_state |= TCON_FW_STATE_VAC_VALID;
	if (mm_table->lut_valid_flag & LCD_TCON_DATA_VALID_DEMURA)
		lcd_tcon_fw.tcon_state |= TCON_FW_STATE_DEMURA_VALID;
	if (mm_table->lut_valid_flag & LCD_TCON_DATA_VALID_ACC)
		lcd_tcon_fw.tcon_state |= TCON_FW_STATE_ACC_VALID;
	if (mm_table->lut_valid_flag & LCD_TCON_DATA_VALID_DITHER)
		lcd_tcon_fw.tcon_state |= TCON_FW_STATE_DITHER_VALID;
	if (mm_table->lut_valid_flag & LCD_TCON_DATA_VALID_OD)
		lcd_tcon_fw.tcon_state |= TCON_FW_STATE_OD_VALID;
	if (mm_table->lut_valid_flag & LCD_TCON_DATA_VALID_LOD)
		lcd_tcon_fw.tcon_state |= TCON_FW_STATE_LOD_VALID;

	lcd_tcon_fw_base_timing_update(pdrv);
	init_completion(&lcd_tcon_fw.alg_comp);

	lcd_tcon_fw.drvdat = (void *)pdrv;
	lcd_tcon_fw.dev = pdrv->dev;
	lcd_tcon_fw.flag = 0;
	if (pdrv->boot_ctrl->dccd_flag)
		lcd_tcon_fw.flag |= TCON_FW_FLAG_DCCD_RUN;

	lcd_tcon_fw.valid = 1;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("tcon fw prepare done\n");
}
