// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
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
#include <linux/of_reserved_mem.h>
#include <linux/cma.h>
#include <linux/dma-map-ops.h>
#include <linux/dma-mapping.h>
#include <linux/reset.h>
#include <linux/clk.h>
#include <linux/amlogic/media/vout/lcd/aml_lcd.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/lcd/lcd_extern.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#include <linux/amlogic/media/vout/lcd/lcd_unifykey.h>
#include <linux/amlogic/media/vout/lcd/aml_bl.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/lcd/lcd_resman.h>
#include <linux/amlogic/media/vout/lcd/lcd_model.h>
#include <linux/page-flags.h>
#include <linux/mm.h>
#include "lcd_common.h"
#include "lcd_reg.h"
#include "lcd_clk/lcd_clk_config.h"
#include "lcd_tcon_swpdf.h"

/* **********************************
 * lcd type
 * **********************************
 */

static struct num_str_s lcd_type_match_table[] = {
	{LCD_RGB,      "rgb"},
	{LCD_LVDS,     "lvds"},
	{LCD_VBYONE,   "vbyone"},
	{LCD_MIPI,     "mipi"},
	{LCD_MLVDS,    "minilvds"},
	{LCD_P2P,      "p2p"},
	{LCD_EDP,      "edp"},
	{LCD_BT656,    "bt656"},
	{LCD_BT1120,   "bt1120"},
	{LCD_TYPE_MAX, "invalid"},
};

int lcd_type_str_to_type(const char *str)
{
	int type = LCD_TYPE_MAX;
	int i;

	for (i = 0; i < ARRAY_SIZE(lcd_type_match_table); i++) {
		if (!strcmp(str, lcd_type_match_table[i].str)) {
			type = lcd_type_match_table[i].num;
			break;
		}
	}
	return type;
}

char *lcd_type_type_to_str(int type)
{
	char *name = lcd_type_match_table[LCD_TYPE_MAX].str;
	int i;

	for (i = 0; i < ARRAY_SIZE(lcd_type_match_table); i++) {
		if (type == lcd_type_match_table[i].num) {
			name = lcd_type_match_table[i].str;
			break;
		}
	}
	return name;
}

static char *lcd_mode_table[] = {
	"tv",
	"tablet",
	"invalid",
};

unsigned char lcd_mode_str_to_mode(const char *str)
{
	unsigned char mode;

	for (mode = 0; mode < ARRAY_SIZE(lcd_mode_table); mode++) {
		if (!strcmp(str, lcd_mode_table[mode]))
			break;
	}
	return mode;
}

char *lcd_mode_mode_to_str(int mode)
{
	return lcd_mode_table[mode];
}

static void lcd_ss_config_fix(struct aml_lcd_drv_s *pdrv)
{
	int i = 0;

	//fix ss in detail timing and phy_attr if not config
	for (i = 0; i < pdrv->config.phy_cfg.group_num; i++) {
		if (pdrv->config.phy_cfg.phys[i]->ss.freq == 255)
			pdrv->config.phy_cfg.phys[i]->ss.freq = pdrv->config.timing.ss_freq;
		if (pdrv->config.phy_cfg.phys[i]->ss.level == 255)
			pdrv->config.phy_cfg.phys[i]->ss.level = pdrv->config.timing.ss_level;
		if (pdrv->config.phy_cfg.phys[i]->ss.mode == 255)
			pdrv->config.phy_cfg.phys[i]->ss.mode = pdrv->config.timing.ss_mode;
	}

	for (i = 0; i < pdrv->config.timing.num_timings; i++) {
		if (pdrv->config.timing.timings[i]->ss_level == 255)
			pdrv->config.timing.timings[i]->ss_level = pdrv->config.timing.ss_level;
		if (pdrv->config.timing.timings[i]->ss_freq == 255)
			pdrv->config.timing.timings[i]->ss_freq = pdrv->config.timing.ss_freq;
		if (pdrv->config.timing.timings[i]->ss_mode == 255)
			pdrv->config.timing.timings[i]->ss_mode = pdrv->config.timing.ss_mode;
	}
}

/* ************************************************** *
 * lcd config
 * **************************************************
 */
static void lcd_config_load_print(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->config;
	struct lcd_detail_timing_s *ptiming = pdrv->config.timing.dft_timing;
	struct phy_attr_s *phy;
	struct phy_config_s *phy_cfg = &pdrv->config.phy_cfg;
	union lcd_ctrl_config_u *pctrl;
	int i = 0, pr_len = 4 * 1024;
	char *pr_buf;

	if (!ptiming)
		return;

	pr_buf = kzalloc(pr_len, GFP_KERNEL);
	if (!pr_buf)
		return;

	if ((lcd_debug_print_flag & LCD_DBG_PR_NORMAL) == 0) {
		i = sprintf(pr_buf + i, "ppc:%d, clk_mode:%d, ",
				pconf->timing.ppc, pconf->timing.clk_mode);
		if (pconf->timing.pre_de_h || pconf->timing.pre_de_v) {
			i += sprintf(pr_buf + i, "pre_de:%d,%d, ",
					pconf->timing.pre_de_h, pconf->timing.pre_de_v);
		}
		sprintf(pr_buf + i, "cfg_chk:0x%x, cus_pinmux:%d, afr_cus:0x%x",
			pconf->basic.config_check, pconf->custom_pinmux, pconf->fr_auto_cus);

		LCDPR("[%d]: load %s config: %s, %s, %dbit, %dx%d, %s\n",
			pdrv->index, get_lcd_config_load(pdrv->config_load),
			pconf->basic.model_name, lcd_type_type_to_str(pconf->basic.lcd_type),
			ptiming->lcd_bits, ptiming->h_active, ptiming->v_active,
			pr_buf);
		kfree(pr_buf);
		return;
	}

	LCDPR("[%d]: load %s config: %s, %s\n",
	      pdrv->index, get_lcd_config_load(pdrv->config_load), pconf->basic.model_name,
	      lcd_type_type_to_str(pconf->basic.lcd_type));
	LCDPR("pll_flag = %d\n", pconf->timing.pll_flag);
	LCDPR("clk_mode = %d\n", pconf->timing.clk_mode);
	LCDPR("custom_pinmux = %d\n", pconf->custom_pinmux);
	LCDPR("fr_auto_cus = 0x%x\n", pconf->fr_auto_cus);

	LCDPR("\nconfig timings:\n");
	for (i = 0; i < pdrv->config.timing.num_timings; i++) {
		ptiming = pdrv->config.timing.timings[i];
		if (!ptiming)
			continue;
		LCDPR("timing[%d]:\n", i);
		lcd_detail_timing_print(ptiming, pr_buf, 0, pr_len);
		lcd_debug_info_print(pr_buf);
	}

	LCDPR("phy config:\n");
	lcd_phy_cfg_print(phy_cfg, pr_buf, 0, pr_len);
	lcd_debug_info_print(pr_buf);

	for (i = 0; i < phy_cfg->group_num; i++) {
		phy = pdrv->config.phy_cfg.phys[i];
		if (!phy)
			continue;
		LCDPR("phy group[%d]:\n", i);
		lcd_phy_attr_print(phy, phy_cfg->lane_num, pr_buf, 0, pr_len);
		lcd_debug_info_print(pr_buf);
	}

	pctrl = &pconf->control;
	if (pconf->basic.lcd_type == LCD_RGB) {
		LCDPR("type = %d\n", pctrl->rgb_cfg.type);
		LCDPR("clk_pol = %d\n", pctrl->rgb_cfg.clk_pol);
		LCDPR("de_valid = %d\n", pctrl->rgb_cfg.de_valid);
		LCDPR("sync_valid = %d\n", pctrl->rgb_cfg.sync_valid);
		LCDPR("rb_swap = %d\n", pctrl->rgb_cfg.rb_swap);
		LCDPR("bit_swap = %d\n", pctrl->rgb_cfg.bit_swap);
	} else if ((pconf->basic.lcd_type == LCD_BT656) ||
		   (pconf->basic.lcd_type == LCD_BT1120)) {
		LCDPR("clk_phase = 0x%x\n", pctrl->bt_cfg.clk_phase);
		LCDPR("field_type = %d\n", pctrl->bt_cfg.field_type);
		LCDPR("mode_422 = %d\n", pctrl->bt_cfg.mode_422);
		LCDPR("yc_swap = %d\n", pctrl->bt_cfg.yc_swap);
		LCDPR("cbcr_swap = %d\n", pctrl->bt_cfg.cbcr_swap);
	} else if (pconf->basic.lcd_type == LCD_LVDS) {
		LCDPR("lvds_repack = %d\n", pctrl->lvds_cfg.lvds_repack);
		LCDPR("pn_swap = %d\n", pctrl->lvds_cfg.pn_swap);
		LCDPR("dual_port = %d\n", pctrl->lvds_cfg.dual_port);
		LCDPR("port_swap = %d\n", pctrl->lvds_cfg.port_swap);
		LCDPR("lane_reverse = %d\n", pctrl->lvds_cfg.lane_reverse);
		LCDPR("phy_vswing = 0x%x\n", pctrl->lvds_cfg.phy_vswing);
		LCDPR("phy_preem = 0x%x\n", pctrl->lvds_cfg.phy_preem);
	} else if (pconf->basic.lcd_type == LCD_VBYONE) {
		LCDPR("lane_count = %d\n", pctrl->vbyone_cfg.lane_count);
		LCDPR("byte_mode = %d\n", pctrl->vbyone_cfg.byte_mode);
		LCDPR("region_num = %d\n", pctrl->vbyone_cfg.region_num);
		LCDPR("color_fmt = %d\n", pctrl->vbyone_cfg.color_fmt);
		LCDPR("phy_vswing = 0x%x\n", pctrl->vbyone_cfg.phy_vswing);
		LCDPR("phy_preem = 0x%x\n", pctrl->vbyone_cfg.phy_preem);
	} else if (pconf->basic.lcd_type == LCD_MLVDS) {
		LCDPR("channel_num = %d\n", pctrl->mlvds_cfg.channel_num);
		LCDPR("channel_sel0 = 0x%x\n", pctrl->mlvds_cfg.channel_sel0);
		LCDPR("channel_sel1 = 0x%x\n", pctrl->mlvds_cfg.channel_sel1);
		LCDPR("clk_phase = 0x%x\n", pctrl->mlvds_cfg.clk_phase);
		LCDPR("phy_vswing = 0x%x\n", pctrl->mlvds_cfg.phy_vswing);
		LCDPR("phy_preem = 0x%x\n", pctrl->mlvds_cfg.phy_preem);
	} else if (pconf->basic.lcd_type == LCD_P2P) {
		LCDPR("p2p_type = 0x%x\n", pctrl->p2p_cfg.p2p_type);
		LCDPR("lane_num = %d\n", pctrl->p2p_cfg.lane_num);
		LCDPR("channel_sel0 = 0x%x\n", pctrl->p2p_cfg.channel_sel0);
		LCDPR("channel_sel1 = 0x%x\n", pctrl->p2p_cfg.channel_sel1);
		LCDPR("phy_vswing = 0x%x\n", pctrl->p2p_cfg.phy_vswing);
		LCDPR("phy_preem = 0x%x\n", pctrl->p2p_cfg.phy_preem);
	} else if (pconf->basic.lcd_type == LCD_MIPI) {
		if (pctrl->mipi_cfg.check_en) {
			LCDPR("check_reg = 0x%02x\n", pctrl->mipi_cfg.check_reg);
			LCDPR("check_cnt = %d\n", pctrl->mipi_cfg.check_cnt);
		}
		LCDPR("lane_num = %d\n", pctrl->mipi_cfg.lane_num);
		LCDPR("bit_rate_max = %d\n", pctrl->mipi_cfg.bit_rate_max);
		LCDPR("operation_mode_init = %d\n", pctrl->mipi_cfg.operation_mode_init);
		LCDPR("operation_mode_disp = %d\n", pctrl->mipi_cfg.operation_mode_display);
		LCDPR("video_mode_type = %d\n", pctrl->mipi_cfg.video_mode_type);
		LCDPR("clk_always_hs = %d\n", pctrl->mipi_cfg.clk_always_hs);
		LCDPR("extern_init = %d\n", pctrl->mipi_cfg.extern_init);
	} else if (pconf->basic.lcd_type == LCD_EDP) {
		LCDPR("max_lane_count = %d\n", pctrl->edp_cfg.max_lane_count);
		LCDPR("max_link_rate  = %d\n", pctrl->edp_cfg.max_link_rate);
		LCDPR("training_mode  = %d\n", pctrl->edp_cfg.training_mode);
		LCDPR("edid_en        = %d\n", pctrl->edp_cfg.edid_en);
		LCDPR("sync_clk_mode  = %d\n", pctrl->edp_cfg.sync_clk_mode);
		LCDPR("lane_count     = %d\n", pctrl->edp_cfg.lane_count);
		LCDPR("link_rate      = %d\n", pctrl->edp_cfg.link_rate);
		LCDPR("phy_vswing = 0x%x\n", pctrl->edp_cfg.phy_vswing_preset);
		LCDPR("phy_preem  = 0x%x\n", pctrl->edp_cfg.phy_preem_preset);
	}

	kfree(pr_buf);
}

int lcd_base_config_load_from_dts(struct aml_lcd_drv_s *pdrv)
{
	const struct device_node *np;
	const char *mode_str, *str;
	unsigned int val, para[8];
	char str_info[128];
	int ret = 0, cnt = 0, i = 0;
	const char *lcd_gpio[LCD_CPU_GPIO_NUM_MAX];


	if (!pdrv->dev->of_node) {
		LCDERR("dev of_node is null\n");
		pdrv->mode = LCD_MODE_MAX;
		return -1;
	}
	np = pdrv->dev->of_node;

	/* lcd driver assign */
	switch (pdrv->debug_ctrl->debug_lcd_mode) {
	case 1:
		LCDPR("[%d]: debug_lcd_mode: 1,tv mode\n", pdrv->index);
		pdrv->mode = LCD_MODE_TV;
		break;
	case 2:
		LCDPR("[%d]: debug_lcd_mode: 2,tablet mode\n", pdrv->index);
		pdrv->mode = LCD_MODE_TABLET;
		break;
	default:
		ret = of_property_read_string(np, "mode", &mode_str);
		if (ret) {
			LCDERR("[%d]: failed to get mode\n", pdrv->index);
			return -1;
		}
		pdrv->mode = lcd_mode_str_to_mode(mode_str);
		break;
	}

	ret = of_property_read_u32(np, "pxp", &val);
	if (ret == 0)
		pdrv->lcd_pxp = (unsigned char)val;

	ret = of_property_read_u32(np, "fr_auto_policy", &val);
	if (ret == 0)
		pdrv->fr_auto_policy = (unsigned char)val;

	ret = of_property_read_u32(np, "vout_regist_on", &val);
	if (ret) {
		pdrv->vout_regist_on_ctrl = 0;
	} else {
		pdrv->vout_regist_on_ctrl = (unsigned char)val;
		LCDPR("set lcd drv regist:%s%s%s\n",
			pdrv->vout_regist_on_ctrl & 0x1 ? " vout"  : "",
			pdrv->vout_regist_on_ctrl & 0x2 ? " vout2" : "",
			pdrv->vout_regist_on_ctrl & 0x4 ? " vout3" : "");
	}

	switch (pdrv->debug_ctrl->debug_para_source) {
	case 1:
		LCDPR("[%d]: debug_para_source: 1,dts\n", pdrv->index);
		pdrv->key_valid = 0;
		break;
	case 2:
		LCDPR("[%d]: debug_para_source: 2,unifykey\n", pdrv->index);
		pdrv->key_valid = 1;
		break;
	default:
		ret = of_property_read_u32(np, "key_valid", &val);
		if (ret) {
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
				LCDPR("failed to get key_valid\n");
			pdrv->key_valid = 0;
		} else {
			pdrv->key_valid = (unsigned char)val;
		}
		break;
	}

	ret = of_property_read_u32(np, "clk_path", &val);
	if (ret) {
		pdrv->clk_path = 0;
	} else {
		pdrv->clk_path = (unsigned char)val;
		LCDPR("[%d]: detect clk_path: %d\n",
		      pdrv->index, pdrv->clk_path);
	}

	ret = of_property_read_u32(np, "auto_test", &val);
	if (ret) {
		pdrv->auto_test = 0;
	} else {
		pdrv->auto_test = (unsigned char)val;
		LCDPR("[%d]: detect auto_test: %d\n",
		      pdrv->index, pdrv->auto_test);
	}

	ret = of_property_read_u32(np, "resume_type", &val);
	if (ret)
		pdrv->resume_type = 1; /* default workqueue */
	else
		pdrv->resume_type = (unsigned char)val;

	ret = of_property_read_u32(np, "config_check_glb", &val);
	if (ret)
		pdrv->config_check_glb = 0;
	else
		pdrv->config_check_glb = (unsigned char)val;

	sprintf(str_info, "resume_type: 0x%x, cfg_chk_glb: %d",
		pdrv->resume_type, pdrv->config_check_glb);
	LCDPR("[%d]: drv_ver: %s(%d-%s), lcd_mode: %s, fr_auto: %d, %s\n",
	      pdrv->index, LCD_DRV_VERSION, pdrv->data->chip_type, pdrv->data->chip_name,
	      mode_str, pdrv->fr_auto_policy, str_info);

	ret = of_property_read_u32_array(np, "display_timing_req_min", &para[0], 5);
	if (ret) {
		pdrv->disp_req.alert_level = 0;
		pdrv->disp_req.hswbp_vid = 0;
		pdrv->disp_req.hfp_vid = 0;
		pdrv->disp_req.vswbp_vid = 0;
		pdrv->disp_req.vfp_vid = 0;
	} else {
		pdrv->disp_req.alert_level = para[0];
		pdrv->disp_req.hswbp_vid = para[1];
		pdrv->disp_req.hfp_vid = para[2];
		pdrv->disp_req.vswbp_vid = para[3];
		pdrv->disp_req.vfp_vid = para[4];
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("[%d]: find display_timing_req_min: alert_level:%d\n"
				"hswbp:%d, hfp:%d, vswbp:%d, vfp:%d\n",
				pdrv->index, pdrv->disp_req.alert_level,
				pdrv->disp_req.hswbp_vid, pdrv->disp_req.hfp_vid,
				pdrv->disp_req.vswbp_vid, pdrv->disp_req.vfp_vid);
		}
	}

	/* only for test */
	ret = of_property_read_string(np, "lcd_propname_sel", &str);
	if (ret == 0) {
		strcpy(pdrv->config.propname, str);
		LCDPR("[%d]: find lcd_propname_sel: %s\n",
			pdrv->index, pdrv->config.propname);
	}

	ret = of_property_read_u32_array(np, "sw_vlock", &para[0], 7);
	if (ret == 0) {
		pdrv->fr_lock_en = para[0];
		pdrv->fr_lock = kzalloc(sizeof(*pdrv->fr_lock), GFP_KERNEL);
		if (pdrv->fr_lock) {
			pdrv->fr_lock->en = para[0];
			pdrv->fr_lock->mode = para[1];
			pdrv->fr_lock->kp = para[2];
			pdrv->fr_lock->ki = para[3];
			pdrv->fr_lock->kd = para[4];
			pdrv->fr_lock->line_limit = para[5];
			pdrv->fr_lock->freq_limit = para[6];
			LCDPR("fr_lock:%d, mode:%d, kp:%d, ki:%d, kd:%d,\n"
				  "line_limit:%d, freq_limit:%d\n",
				pdrv->fr_lock_en, pdrv->fr_lock->mode, pdrv->fr_lock->kp,
				pdrv->fr_lock->ki, pdrv->fr_lock->kd,
				pdrv->fr_lock->line_limit, pdrv->fr_lock->freq_limit);
		}
	}
	cnt = of_property_read_string_array(pdrv->dev->of_node, "lcd_cpu_gpio_names",
					    lcd_gpio, LCD_CPU_GPIO_NUM_MAX);
	for (i = 0; i < cnt; i++)
		strscpy(pdrv->config.power.cpu_gpio[i].name, lcd_gpio[i], LCD_CPU_GPIO_NAME_MAX);

	return 0;
}

static int lcd_power_load_from_dts(struct aml_lcd_drv_s *pdrv, struct device_node *child)
{
	struct lcd_power_ctrl_s *power_step = &pdrv->config.power;
	struct lcd_power_step_s *pstep;
	int ret = 0, append_more = 1;
	unsigned int para[5];
	unsigned int val;
	int i, j, temp;
	unsigned int index;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	if (!child) {
		LCDERR("[%d]: error: failed to get %s\n",
		      pdrv->index, pdrv->config.propname);
		return -1;
	}

	pstep = pdrv->config.power.power_on_step;
	ret = of_property_read_u32_array(child, "power_on_step", &para[0], 4);
	if (ret) {
		LCDPR("[%d]: failed to get power_on_step\n", pdrv->index);
		pstep[0].type = 0xff;
		pdrv->config.power.power_on_step_max = 1;
	} else {
		i = 0;
		while (i < LCD_PWR_STEP_MAX) {
			j = 4 * i;
			ret = of_property_read_u32_index(child, "power_on_step", j, &val);
			if (ret) {
				LCDPR("[%d]: failed to get power_on_step %d\n", pdrv->index, i);
				pstep[i].type = 0xff;
				i++;
				break;
			}
			pstep[i].type = (unsigned char)val;
			if (pstep[i].type >= LCD_POWER_TYPE_MAX) {
				i++;
				break;
			}

			j = 4 * i + 1;
			ret = of_property_read_u32_index(child, "power_on_step", j, &val);
			if (ret) {
				LCDPR("[%d]: failed to get power_on_step %d\n", pdrv->index, i);
				pstep[i].type = 0xff;
				i++;
				break;
			}
			pstep[i].index = val;
			j = 4 * i + 2;
			ret = of_property_read_u32_index(child, "power_on_step", j, &val);
			if (ret) {
				LCDPR("[%d]: failed to get power_on_step %d\n", pdrv->index, i);
				pstep[i].type = 0xff;
				i++;
				break;
			}
			pstep[i].value = val;
			j = 4 * i + 3;
			ret = of_property_read_u32_index(child, "power_on_step", j, &val);
			if (ret) {
				LCDPR("[%d]: failed to get power_on_step %d\n", pdrv->index, i);
				pstep[i].type = 0xff;
				i++;
				break;
			}
			pstep[i].delay = val;

			/* gpio/extern probe */
			index = pstep[i].index;
			switch (pstep[i].type) {
			case LCD_POWER_TYPE_GPIO:
			case LCD_POWER_TYPE_WAIT_GPIO:
				if (index < LCD_CPU_GPIO_NUM_MAX)
					lcd_cpu_gpio_probe(pdrv, index);
				break;
#ifdef CONFIG_AMLOGIC_LCD_EXTERN
			case LCD_POWER_TYPE_EXTERN:
				lcd_resource_add(pdrv, LCD_RES_EXTERN, index);
				lcd_extern_dev_index_add(pdrv->index, index);
				break;
#endif
			case LCD_POWER_TYPE_CLK_SS:
				temp = pstep[i].value;
				pdrv->config.timing.ss_freq = temp & 0xf;
				pdrv->config.timing.ss_mode = (temp >> 4) & 0xf;
				if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
					LCDPR("[%d]: clk_ss value=0x%x: ss_freq=%d, ss_mode=%d\n",
					      pdrv->index, temp,
					      pdrv->config.timing.ss_freq,
					      pdrv->config.timing.ss_mode);
				}
				break;
			case LCD_POWER_TYPE_BACKLIGHT:
			case LCD_POWER_TYPE_MUTE:
				append_more = 0;
				break;
			default:
				break;
			}
			i++;
		}
		power_step->power_on_step_max = i;

		if (append_more && i + 2 < LCD_PWR_STEP_MAX) {
			i--;
			pstep[i].type = LCD_POWER_TYPE_BACKLIGHT;
			pstep[i].index = 0;
			pstep[i].value = 1; //bl on
			pstep[i].delay = 0;
			i++;

			pstep[i].type = LCD_POWER_TYPE_MUTE;
			pstep[i].index = 0;
			pstep[i].value = 0;//unmute
			pstep[i].delay = pdrv->unmute_cnt;
			i++;
			pstep[i].type = LCD_POWER_TYPE_MAX;
			i++;
			power_step->power_on_step_max = i;
		}
	}

	pstep = pdrv->config.power.power_off_step;
	ret = of_property_read_u32_array(child, "power_off_step", &para[0], 4);
	if (ret) {
		LCDPR("[%d]: failed to get power_off_step\n", pdrv->index);
		pstep[0].type = 0xff;
		power_step->power_off_step_max = 1;
	} else {
		append_more = 1;
		i = 0;
		while (i < LCD_PWR_STEP_MAX) {
			j = 4 * i;
			ret = of_property_read_u32_index(child, "power_off_step", j, &val);
			if (ret) {
				LCDPR("[%d]: failed to get power_off_step %d\n", pdrv->index, i);
				pstep[i].type = 0xff;
				i++;
				break;
			}
			pstep[i].type = (unsigned char)val;
			if (pstep[i].type >= LCD_POWER_TYPE_MAX) {
				i++;
				break;
			}

			j = 4 * i + 1;
			ret = of_property_read_u32_index(child, "power_off_step", j, &val);
			if (ret) {
				LCDPR("[%d]: failed to get power_off_step %d\n", pdrv->index, i);
				pstep[i].type = 0xff;
				i++;
				break;
			}
			pstep[i].index = val;
			j = 4 * i + 2;
			ret = of_property_read_u32_index(child, "power_off_step", j, &val);
			if (ret) {
				LCDPR("[%d]: failed to get power_off_step %d\n", pdrv->index, i);
				pstep[i].type = 0xff;
				i++;
				break;
			}
			pstep[i].value = val;
			j = 4 * i + 3;
			ret = of_property_read_u32_index(child, "power_off_step", j, &val);
			if (ret) {
				LCDPR("[%d]: failed to get power_off_step %d\n", pdrv->index, i);
				pstep[i].type = 0xff;
				i++;
				break;
			}
			pstep[i].delay = val;

			/* gpio/extern probe */
			index = pstep[i].index;
			switch (pstep[i].type) {
			case LCD_POWER_TYPE_GPIO:
			case LCD_POWER_TYPE_WAIT_GPIO:
				if (index < LCD_CPU_GPIO_NUM_MAX)
					lcd_cpu_gpio_probe(pdrv, index);
				break;
#ifdef CONFIG_AMLOGIC_LCD_EXTERN
			case LCD_POWER_TYPE_EXTERN:
				lcd_resource_add(pdrv, LCD_RES_EXTERN, index);
				lcd_extern_dev_index_add(pdrv->index, index);
				break;
#endif
			case LCD_POWER_TYPE_BACKLIGHT:
			case LCD_POWER_TYPE_MUTE:
				append_more = 0;
				break;
			default:
				break;
			}
			i++;
		}
		power_step->power_off_step_max = i;

		if (append_more && i + 2 < LCD_POWER_TYPE_MAX) {
			i--;
			for (j = i + 2; j >= 2; j--)
				memcpy(&pstep[j], &pstep[j - 2], sizeof(struct lcd_power_step_s));
			power_step->power_off_step_max += 2;
			pstep[0].type  = LCD_POWER_TYPE_MUTE;
			pstep[0].index = 0;
			pstep[0].value = 1; //mute
			pstep[0].delay = pdrv->mute_cnt;

			pstep[1].type = LCD_POWER_TYPE_BACKLIGHT;
			pstep[1].index = 0;
			pstep[1].value = 0;//bl off
			pstep[1].delay = 0;
		}
	}

	return ret;
}

static int lcd_vlock_param_load_from_dts(struct aml_lcd_drv_s *pdrv, struct device_node *child)
{
	unsigned int para[4];
	int ret;

	pdrv->config.vlock_param[0] = LCD_VLOCK_PARAM_BIT_UPDATE;

	ret = of_property_read_u32_array(child, "vlock_attr", &para[0], 4);
	if (ret == 0) {
		LCDPR("[%d]: find vlock_attr\n", pdrv->index);
		pdrv->config.vlock_param[0] |= LCD_VLOCK_PARAM_BIT_VALID;
		pdrv->config.vlock_param[1] = para[0];
		pdrv->config.vlock_param[2] = para[1];
		pdrv->config.vlock_param[3] = para[2];
		pdrv->config.vlock_param[4] = para[3];
	}

	return 0;
}

static int lcd_optical_load_from_dts(struct aml_lcd_drv_s *pdrv, struct device_node *child)
{
	unsigned int para[13];
	int ret;

	ret = of_property_read_u32_array(child, "optical_attr", &para[0], 13);
	if (ret == 0) {
		LCDPR("[%d]: find optical_attr\n", pdrv->index);
		pdrv->config.optical.hdr_support = para[0];
		pdrv->config.optical.features = para[1];
		pdrv->config.optical.primaries_r_x = para[2];
		pdrv->config.optical.primaries_r_y = para[3];
		pdrv->config.optical.primaries_g_x = para[4];
		pdrv->config.optical.primaries_g_y = para[5];
		pdrv->config.optical.primaries_b_x = para[6];
		pdrv->config.optical.primaries_b_y = para[7];
		pdrv->config.optical.white_point_x = para[8];
		pdrv->config.optical.white_point_y = para[9];
		pdrv->config.optical.luma_max = para[10];
		pdrv->config.optical.luma_min = para[11];
		pdrv->config.optical.luma_avg = para[12];
	}
	ret = of_property_read_u32_array(child, "optical_adv_val", &para[0], 13);
	if (ret == 0) {
		LCDPR("[%d]: find optical_adv_val\n", pdrv->index);
		pdrv->config.optical.ldim_support = para[0];
		pdrv->config.optical.luma_peak = para[4];
	}

	lcd_optical_vinfo_update(pdrv);

	return 0;
}

static int lcd_config_load_from_dts(struct aml_lcd_drv_s *pdrv)
{
	struct device_node *child;
	struct lcd_config_s *pconf = &pdrv->config;
	struct lcd_detail_timing_s *ptiming;
	struct lcd_timing_s *tims = &pdrv->config.timing;
	union lcd_ctrl_config_u *pctrl = &pdrv->config.control;
	struct phy_config_s *phy_cfg = &pdrv->config.phy_cfg;
	struct phy_attr_s *phy = NULL;
	unsigned int para[10], val;
	const char *str;
	int i, ret = 0;

	if (!pdrv->dev->of_node) {
		LCDERR("[%d]: dev of_node is null\n", pdrv->index);
		return -1;
	}

	child = of_get_child_by_name(pdrv->dev->of_node, pconf->propname);
	if (!child) {
		LCDERR("[%d]: failed to get %s\n", pdrv->index, pconf->propname);
		return -1;
	}

	ret = of_property_read_string(child, "model_name", &str);
	if (ret) {
		LCDERR("[%d]: failed to get model_name\n", pdrv->index);
		strscpy(pconf->basic.model_name, pconf->propname, MOD_LEN_MAX);
	} else {
		strscpy(pconf->basic.model_name, str, MOD_LEN_MAX);
	}
	/* ensure string ending */
	pconf->basic.model_name[MOD_LEN_MAX - 1] = '\0';

	ret = of_property_read_string(child, "interface", &str);
	if (ret) {
		LCDERR("[%d]: failed to get interface\n", pdrv->index);
		str = "invalid";
	}
	pconf->basic.lcd_type = lcd_type_str_to_type(str);

	ret = of_property_read_u32(child, "config_check", &val);
	if (ret == 0)
		pconf->basic.config_check = val ? 0x3 : 0x2;

	ret = of_property_read_u32_array(child, "basic_setting", &para[0], 7);
	if (ret) {
		LCDERR("[%d]: failed to get basic_setting\n", pdrv->index);
		return -1;
	}

	ptiming = lcd_timing_alloc(pdrv);
	if (!ptiming) {
		LCDERR("[%d] dft_timing alloc fail\n", pdrv->index);
		return -1;
	}
	memset(ptiming, 0, sizeof(*ptiming));
	tims->dft_timing = tims->timings[0];

	ptiming->h_active = para[0];
	ptiming->v_active = para[1];
	ptiming->h_period = para[2];
	ptiming->v_period = para[3];
	ptiming->lcd_bits = para[4] * 3;
	pconf->basic.screen_width = para[5];
	pconf->basic.screen_height = para[6];

	ret = of_property_read_u32_array(child, "range_setting", &para[0], 6);
	if (ret) {
		LCDPR("[%d]: no range_setting\n", pdrv->index);
		ptiming->h_period_min = ptiming->h_period;
		ptiming->h_period_max = ptiming->h_period;
		ptiming->v_period_min = ptiming->v_period;
		ptiming->v_period_max = ptiming->v_period;
		ptiming->pclk_min = 0;
		ptiming->pclk_max = 0;
	} else {
		ptiming->h_period_min = para[0];
		ptiming->h_period_max = para[1];
		ptiming->v_period_min = para[2];
		ptiming->v_period_max = para[3];
		ptiming->pclk_min = para[4];
		ptiming->pclk_max = para[5];
	}

	ret = of_property_read_u32_array(child, "range_frame_rate", &para[0], 6);
	if (ret == 0) {
		ptiming->frame_rate_min = para[0];
		ptiming->frame_rate_max = para[1];
	}

	ret = of_property_read_u32_array(child, "lcd_timing", &para[0], 6);
	if (ret) {
		LCDERR("[%d]: failed to get lcd_timing\n", pdrv->index);
		lcd_timing_free_last(pdrv);
		return -1;
	}
	ptiming->hsync_width = (unsigned short)(para[0]);
	ptiming->hsync_bp = (unsigned short)(para[1]);
	ptiming->hsync_fp = ptiming->h_period - ptiming->h_active -
			ptiming->hsync_width - ptiming->hsync_bp;
	ptiming->hsync_pol = (unsigned short)(para[2]);
	ptiming->vsync_width = (unsigned short)(para[3]);
	ptiming->vsync_bp = (unsigned short)(para[4]);
	ptiming->vsync_fp = ptiming->v_period - ptiming->v_active -
			ptiming->vsync_width - ptiming->vsync_bp;
	ptiming->vsync_pol = (unsigned short)(para[5]);
	ret = of_property_read_u32(child, "ppc_mode", &val);
	if (ret)
		pconf->timing.ppc = 1;
	else
		pconf->timing.ppc = val;

	ret = of_property_read_u32(child, "clk_mode", &val);
	if (ret)
		pconf->timing.clk_mode = LCD_CLK_MODE_DEPENDENCE;
	else
		pconf->timing.clk_mode = val;

	ret = of_property_read_u32_array(child, "pre_de", &para[0], 2);
	if (ret) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDERR("failed to get pre_de\n");
		pconf->timing.pre_de_h = 0;
		pconf->timing.pre_de_v = 0;
	} else {
		pconf->timing.pre_de_h = (unsigned short)(para[0]);
		pconf->timing.pre_de_v = (unsigned short)(para[1]);
	}

	ret = of_property_read_u32_array(child, "clk_attr", &para[0], 4);
	if (ret) {
		LCDERR("[%d]: failed to get clk_attr\n", pdrv->index);
		ptiming->fr_adjust_type = 0;
		pconf->timing.ss_level = 0;
		pconf->timing.ss_freq = 0;
		pconf->timing.ss_mode = 0;
		pconf->timing.pll_flag = 1;
		ptiming->pixel_clk = 60;
	} else {
		ptiming->fr_adjust_type = (unsigned char)(para[0]);
		pconf->timing.ss_level = para[1] & 0xff;
		pconf->timing.ss_freq = (para[1] >> 8) & 0xf;
		pconf->timing.ss_mode = (para[1] >> 12) & 0xf;
		pconf->timing.pll_flag = para[2] & 0xf;
		ptiming->pixel_clk = para[3];
	}
	ptiming->switch_type = LCD_VMODE_SWITCH_NONE;
	ptiming->ss_force = 0;
	ptiming->ss_freq = pconf->timing.ss_freq;
	ptiming->ss_level = pconf->timing.ss_level;
	ptiming->ss_mode = pconf->timing.ss_mode;

	lcd_clk_frame_rate_init(ptiming);
	lcd_default_to_basic_timing_init_config(pdrv);
	lcd_config_timing_check(pdrv, ptiming);

	ret = of_property_read_u32(child, "custom_pinmux", &val);
	if (ret) {
		ret = of_property_read_u32(child, "customer_pinmux", &val);
		if (ret)
			pconf->custom_pinmux = 0;
		else
			pconf->custom_pinmux = val;
	} else {
		pconf->custom_pinmux = val;
	}

	ret = of_property_read_u32(child, "fr_auto_disable", &val);
	if (ret) {
		ret = of_property_read_u32(child, "fr_auto_custom", &val);
		if (ret)
			pconf->fr_auto_cus = 0;
		else
			pconf->fr_auto_cus = val;
	} else {
		if (val)
			pconf->fr_auto_cus = 0xff;
		else
			pconf->fr_auto_cus = 0;
	}

	switch (pconf->basic.lcd_type) {
	case LCD_RGB:
		ret = of_property_read_u32_array(child, "rgb_attr", &para[0], 6);
		if (ret) {
			LCDERR("[%d]: failed to get rgb_attr\n", pdrv->index);
			return -1;
		}
		pctrl->rgb_cfg.type = para[0];
		pctrl->rgb_cfg.clk_pol = para[1];
		pctrl->rgb_cfg.de_valid = para[2];
		pctrl->rgb_cfg.sync_valid = para[3];
		pctrl->rgb_cfg.rb_swap = para[4];
		pctrl->rgb_cfg.bit_swap = para[5];
		break;
	case LCD_BT656:
	case LCD_BT1120:
		ret = of_property_read_u32_array(child, "bt_attr", &para[0], 8);
		if (ret) {
			LCDERR("[%d]: failed to get bt_attr\n", pdrv->index);
			return -1;
		}
		pctrl->bt_cfg.clk_phase = para[0];
		pctrl->bt_cfg.field_type = para[1];
		pctrl->bt_cfg.mode_422 = para[2];
		pctrl->bt_cfg.yc_swap = para[3];
		pctrl->bt_cfg.cbcr_swap = para[4];
		break;
	case LCD_LVDS:
		ret = of_property_read_u32_array(child, "lvds_attr", &para[0], 5);
		if (ret) {
			LCDPR("[%d]: failed to get lvds_attr\n", pdrv->index);
			return -1;
		}
		pctrl->lvds_cfg.lvds_repack = para[0];
		pctrl->lvds_cfg.dual_port = para[1];
		pctrl->lvds_cfg.pn_swap = para[2];
		pctrl->lvds_cfg.port_swap = para[3];
		pctrl->lvds_cfg.lane_reverse = para[4];

		ret = of_property_read_u32_array(child, "phy_attr", &para[0], 2);
		if (ret) {
			LCDPR("[%d]: failed to get phy_attr\n", pdrv->index);
			pctrl->lvds_cfg.phy_vswing = 0x5;
			pctrl->lvds_cfg.phy_preem  = 0x1;
		} else {
			pctrl->lvds_cfg.phy_vswing = para[0];
			pctrl->lvds_cfg.phy_preem = para[1];
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("[%d]: phy vswing_level=0x%x, preem_level=0x%x\n",
				      pdrv->index,
				      pctrl->lvds_cfg.phy_vswing,
				      pctrl->lvds_cfg.phy_preem);
			}
		}

		phy_cfg->vswing_level = pctrl->lvds_cfg.phy_vswing & 0xf;
		phy_cfg->ext_pullup = (pctrl->lvds_cfg.phy_vswing >> 4) & 0x3;
		phy_cfg->preem_level = pctrl->lvds_cfg.phy_preem;
		break;
	case LCD_VBYONE:
		ret = of_property_read_u32_array(child, "vbyone_attr", &para[0], 4);
		if (ret) {
			LCDERR("[%d]: failed to get vbyone_attr\n", pdrv->index);
			return -1;
		}
		pctrl->vbyone_cfg.lane_count = para[0];
		pctrl->vbyone_cfg.region_num = para[1];
		pctrl->vbyone_cfg.byte_mode = para[2];
		pctrl->vbyone_cfg.color_fmt = para[3];

		pctrl->vbyone_cfg.slice = pdrv->config.timing.ppc ? pdrv->config.timing.ppc : 1;

		ret = of_property_read_u32_array(child, "vbyone_intr_enable", &para[0], 2);
		if (ret) {
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
				LCDERR("[%d]: failed to get vbyone_intr_enable\n", pdrv->index);
		} else {
			pctrl->vbyone_cfg.intr_en = para[0];
			pctrl->vbyone_cfg.vsync_intr_en = para[1];
		}
		ret = of_property_read_u32_array(child, "phy_attr", &para[0], 2);
		if (ret) {
			LCDPR("[%d]: failed to get phy_attr\n", pdrv->index);
			pctrl->vbyone_cfg.phy_vswing = 0x5;
			pctrl->vbyone_cfg.phy_preem  = 0x1;
		} else {
			pctrl->vbyone_cfg.phy_vswing = para[0];
			pctrl->vbyone_cfg.phy_preem = para[1];
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("[%d]: phy vswing_level=0x%x, preem_level=0x%x\n",
				      pdrv->index,
				      pctrl->vbyone_cfg.phy_vswing,
				      pctrl->vbyone_cfg.phy_preem);
			}
		}

		phy_cfg->vswing_level = pctrl->vbyone_cfg.phy_vswing & 0xf;
		phy_cfg->ext_pullup = (pctrl->vbyone_cfg.phy_vswing >> 4) & 0x3;
		phy_cfg->preem_level = pctrl->vbyone_cfg.phy_preem;

		ret = of_property_read_u32(child, "vbyone_ctrl_flag", &val);
		if (ret) {
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
				LCDPR("[%d]: failed to get vbyone_ctrl_flag\n", pdrv->index);
		} else {
			pctrl->vbyone_cfg.ctrl_flag = val;
			LCDPR("[%d]: vbyone ctrl_flag=0x%x\n",
			      pdrv->index, pctrl->vbyone_cfg.ctrl_flag);
		}
		if (pctrl->vbyone_cfg.ctrl_flag & 0x7) {
			ret = of_property_read_u32_array(child, "vbyone_ctrl_timing", &para[0], 3);
			if (ret) {
				LCDPR("[%d]: failed to get vbyone_ctrl_timing\n", pdrv->index);
			} else {
				pctrl->vbyone_cfg.power_on_reset_delay = para[0];
				pctrl->vbyone_cfg.hpd_data_delay = para[1];
				pctrl->vbyone_cfg.cdr_training_hold = para[2];
			}
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("[%d]: power_on_reset_delay: %d\n",
				      pdrv->index,
				      pctrl->vbyone_cfg.power_on_reset_delay);
				LCDPR("[%d]: hpd_data_delay: %d\n",
				      pdrv->index,
				      pctrl->vbyone_cfg.hpd_data_delay);
				LCDPR("[%d]: cdr_training_hold: %d\n",
				      pdrv->index,
				      pctrl->vbyone_cfg.cdr_training_hold);
			}
		}
		ret = of_property_read_u32_array(child, "hw_filter", &para[0], 2);
		if (ret) {
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
				LCDPR("[%d]: failed to get hw_filter\n", pdrv->index);
		} else {
			pctrl->vbyone_cfg.hw_filter_time = para[0];
			pctrl->vbyone_cfg.hw_filter_cnt = para[1];
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("[%d]: vbyone hw_filter=0x%x 0x%x\n",
				      pdrv->index,
				      pctrl->vbyone_cfg.hw_filter_time,
				      pctrl->vbyone_cfg.hw_filter_cnt);
			}
		}
		break;
	case LCD_MLVDS:
		ret = of_property_read_u32_array(child, "minilvds_attr", &para[0], 6);
		if (ret) {
			LCDERR("[%d]: failed to get minilvds_attr\n", pdrv->index);
			return -1;
		}
		pctrl->mlvds_cfg.channel_num = para[0];
		pctrl->mlvds_cfg.channel_sel0 = para[1];
		pctrl->mlvds_cfg.channel_sel1 = para[2];
		pctrl->mlvds_cfg.clk_phase = para[3];
		pctrl->mlvds_cfg.pn_swap = para[4];
		pctrl->mlvds_cfg.bit_swap = para[5];

		ret = of_property_read_u32_array(child, "phy_attr", &para[0], 2);
		if (ret) {
			LCDPR("[%d]: failed to get phy_attr\n", pdrv->index);
			pctrl->mlvds_cfg.phy_vswing = 0x5;
			pctrl->mlvds_cfg.phy_preem  = 0x1;
		} else {
			pctrl->mlvds_cfg.phy_vswing = para[0];
			pctrl->mlvds_cfg.phy_preem = para[1];
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("[%d]: phy vswing_level=0x%x, preem_level=0x%x\n",
				      pdrv->index,
				      pctrl->mlvds_cfg.phy_vswing,
				      pctrl->mlvds_cfg.phy_preem);
			}
		}

		phy_cfg->vswing_level = pctrl->mlvds_cfg.phy_vswing & 0xf;
		phy_cfg->ext_pullup = (pctrl->mlvds_cfg.phy_vswing >> 4) & 0x3;
		phy_cfg->preem_level = pctrl->mlvds_cfg.phy_preem;
		break;
	case LCD_P2P:
		ret = of_property_read_u32_array(child, "p2p_attr", &para[0], 6);
		if (ret) {
			LCDERR("[%d]: failed to get p2p_attr\n", pdrv->index);
			return -1;
		}
		pctrl->p2p_cfg.p2p_type = para[0];
		pctrl->p2p_cfg.lane_num = para[1];
		pctrl->p2p_cfg.channel_sel0 = para[2];
		pctrl->p2p_cfg.channel_sel1 = para[3];
		pctrl->p2p_cfg.pn_swap = para[4];
		pctrl->p2p_cfg.bit_swap = para[5];

		ret = of_property_read_u32_array(child, "phy_attr", &para[0], 2);
		if (ret) {
			LCDPR("[%d]: failed to get phy_attr\n", pdrv->index);
			pctrl->p2p_cfg.phy_vswing = 0x5;
			pctrl->p2p_cfg.phy_preem  = 0x1;
		} else {
			pctrl->p2p_cfg.phy_vswing = para[0];
			pctrl->p2p_cfg.phy_preem = para[1];
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("[%d]: phy vswing_level=0x%x, preem_level=0x%x\n",
				      pdrv->index,
				      pctrl->p2p_cfg.phy_vswing,
				      pctrl->p2p_cfg.phy_preem);
			}
		}

		phy_cfg->vswing_level = pctrl->p2p_cfg.phy_vswing & 0xf;
		phy_cfg->ext_pullup = (pctrl->p2p_cfg.phy_vswing >> 4) & 0x3;
		phy_cfg->preem_level = pctrl->p2p_cfg.phy_preem;
		break;
	case LCD_MIPI:
		ret = of_property_read_u32_array(child, "mipi_attr", &para[0], 8);
		if (ret) {
			LCDERR("[%d]: failed to get mipi_attr\n", pdrv->index);
			return -1;
		}
		pctrl->mipi_cfg.lane_num = para[0];
		pctrl->mipi_cfg.bit_rate_max = para[1];
		pctrl->mipi_cfg.operation_mode_init = para[3];
		pctrl->mipi_cfg.operation_mode_display = para[4];
		pctrl->mipi_cfg.video_mode_type = para[5];
		pctrl->mipi_cfg.clk_always_hs = para[6];
		pctrl->mipi_cfg.user_pkt_size = para[7];

#ifdef CONFIG_AMLOGIC_LCD_TABLET
		lcd_mipi_dsi_init_table_detect(pdrv, child);
#endif
#ifdef CONFIG_AMLOGIC_LCD_EXTERN
		ret = of_property_read_u32_array(child, "extern_init", &para[0], 1);
		if (ret) {
			LCDPR("[%d]: failed to get extern_init\n", pdrv->index);
		} else {
			pctrl->mipi_cfg.extern_init = para[0];
			lcd_resource_add(pdrv, LCD_RES_EXTERN, para[0]);
			lcd_extern_dev_index_add(pdrv->index, para[0]);
		}
#endif

		phy_cfg->vswing_level = 0;
		phy_cfg->preem_level = 0;
		break;
	case LCD_EDP:
		ret = of_property_read_u32_array(child, "edp_attr", &para[0], 9);
		if (ret) {
			LCDERR("[%d]: failed to get edp_attr\n", pdrv->index);
			return -1;
		}
		pctrl->edp_cfg.max_lane_count = (unsigned char)para[0];
		pctrl->edp_cfg.max_link_rate = (unsigned char)(para[1] < 0x6 ? 0 : para[1]);
		pctrl->edp_cfg.training_mode = (unsigned char)para[2];
		pctrl->edp_cfg.edid_en = (unsigned char)para[3];

		ret = of_property_read_u32_array(child, "phy_attr", &para[0], 2);
		if (ret) {
			LCDPR("[%d]: failed to get phy_attr\n", pdrv->index);
			pctrl->edp_cfg.phy_vswing_preset = 0x5;
			pctrl->edp_cfg.phy_preem_preset  = 0x1;
		} else {
			pctrl->edp_cfg.phy_vswing_preset = para[0];
			pctrl->edp_cfg.phy_preem_preset = para[1];
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("[%d]: phy vswing_level=0x%x, preem_level=0x%x\n",
				      pdrv->index,
				      pctrl->edp_cfg.phy_vswing_preset,
				      pctrl->edp_cfg.phy_preem_preset);
			}
		}

		phy_cfg->vswing_level = pctrl->edp_cfg.phy_vswing_preset & 0xf;
		phy_cfg->preem_level = pctrl->edp_cfg.phy_preem_preset;
		break;
	default:
		LCDERR("[%d]: invalid lcd type\n", pdrv->index);
		break;
	}

	phy = lcd_phy_alloc(pdrv);// first parse, it is only be phy_cfg->phys[0];
	if (!phy) {
		LCDERR("%s phy alloc fail\n", __func__);
		return -1;
	}
	memset(phy, 0, sizeof(*phy));
	phy_cfg->act_phy = phy_cfg->phys[0];
	lcd_phy_param_preset(pdrv);
	lcd_lane_map_preset(pdrv);
	phy->ss.freq = 255;
	phy->ss.level = 255;
	phy->ss.mode = 255;

	ret = of_property_read_u32_array(child, "phy_adv_attr", &para[0], 5);
	if (ret)
		goto config_phy_adv_attr_done;

	phy_cfg->flag = para[0];
	phy->vswing   = para[1];
	phy->vcm      = para[2];
	phy->ref_bias = para[3];
	phy->odt      = para[4];
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("%s: ctrl_flag=0x%x vsw=0x%08x vcm=0x%x, ref_bias=0x%x, odt=0x%x\n",
			__func__, phy_cfg->flag, phy->vswing, phy->vcm,
			phy->ref_bias, phy->odt);
	}
	ret = of_property_read_variable_u32_array(child, "phy_lane_ctrl", &para[0], 1, 32);
	if (ret <= 0 || ((phy_cfg->flag & (0x3 << 12)) == 0))
		goto config_phy_adv_attr_done;

	for (i = 0; i < phy_cfg->lane_num; i++) {
		if (i >= ret)
			break;

		if (phy_cfg->flag & (1 << 12))
			phy->lane[i].preem = para[i] & 0xffff;

		if (phy_cfg->flag & (1 << 13))
			phy->lane[i].amp   = para[i] >> 16;

		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("%s: lane[%d]: preem=0x%x amp=0x%x\n",
				__func__, i, phy->lane[i].preem, phy->lane[i].amp);
	}
config_phy_adv_attr_done:

	lcd_vlock_param_load_from_dts(pdrv, child);
	ret = lcd_power_load_from_dts(pdrv, child);

	lcd_optical_load_from_dts(pdrv, child);

	lcd_cus_ctrl_load_from_dts(pdrv, child);

	//fix ss in detail timing and phy_attr if not config
	lcd_ss_config_fix(pdrv);

	ret = of_property_read_u32(child, "backlight_index", &para[0]);
	if (ret) {
		LCDPR("[%d]: failed to get backlight_index\n", pdrv->index);
		pconf->backlight_index = 0xff;
	} else {
		pconf->backlight_index = para[0];
#ifdef CONFIG_AMLOGIC_BACKLIGHT
		lcd_resource_add(pdrv, LCD_RES_BACKLIGHT, pconf->backlight_index);
		aml_bl_index_add(pdrv->index, pconf->backlight_index);
#endif
	}

	return ret;
}

static int lcd_power_load_from_unifykey(struct aml_lcd_drv_s *pdrv,
					unsigned char *buf, int key_len, int len)
{
	struct lcd_power_ctrl_s *power_step = &pdrv->config.power;
	struct lcd_power_step_s *pstep;
	int i, j, temp;
	unsigned char *p;
	unsigned int index;
	int ret, append_more = 1;

	/* power: (5byte * n) */
	p = buf + len;
	i = 0;
	pstep = pdrv->config.power.power_on_step;
	while (i < LCD_PWR_STEP_MAX) {
		len += 5;
		ret = lcd_unifykey_len_check(key_len, len);
		if (ret < 0) {
			pstep[i].type = 0xff;
			pstep[i].index = 0;
			pstep[i].value = 0;
			pstep[i].delay = 0;
			LCDERR("[%d]: unifykey power_on length is incorrect\n", pdrv->index);
			return -1;
		}
		pstep[i].type = *(p + LCD_UKEY_PWR_TYPE + 5 * i);
		pstep[i].index = *(p + LCD_UKEY_PWR_INDEX + 5 * i);
		pstep[i].value = *(p + LCD_UKEY_PWR_VAL + 5 * i);
		pstep[i].delay =
			(*(p + LCD_UKEY_PWR_DELAY + 5 * i) |
			((*(p + LCD_UKEY_PWR_DELAY + 5 * i + 1)) << 8));

		if (pstep[i].type >= LCD_POWER_TYPE_MAX) {
			i++;
			break;
		}

		/* gpio/extern probe */
		index = pstep[i].index;
		switch (pstep[i].type) {
		case LCD_POWER_TYPE_GPIO:
		case LCD_POWER_TYPE_WAIT_GPIO:
			if (index < LCD_CPU_GPIO_NUM_MAX)
				lcd_cpu_gpio_probe(pdrv, index);
			break;
#ifdef CONFIG_AMLOGIC_LCD_EXTERN
		case LCD_POWER_TYPE_EXTERN:
			lcd_resource_add(pdrv, LCD_RES_EXTERN, index);
			lcd_extern_dev_index_add(pdrv->index, index);
			break;
#endif
		case LCD_POWER_TYPE_CLK_SS:
			temp = pstep[i].value;
			pdrv->config.timing.ss_freq = temp & 0xf;
			pdrv->config.timing.ss_mode = (temp >> 4) & 0xf;
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("[%d]: clk_ss value=0x%x: ss_freq=%d, ss_mode=%d\n",
					      pdrv->index, temp,
					      pdrv->config.timing.ss_freq,
					      pdrv->config.timing.ss_mode);
			}
			break;
		case LCD_POWER_TYPE_BACKLIGHT:
		case LCD_POWER_TYPE_MUTE:
			append_more = 0;
			break;
		default:
			break;
		}
		i++;
	}
	power_step->power_on_step_max = i;

	p += 5 * i;
	if (append_more && i + 2 < LCD_PWR_STEP_MAX) {
		i--;
		pstep[i].type = LCD_POWER_TYPE_BACKLIGHT;
		pstep[i].index = 0;
		pstep[i].value = 1; //bl on
		pstep[i].delay = 0;
		i++;
		pstep[i].type = LCD_POWER_TYPE_MUTE;
		pstep[i].index = 0;
		pstep[i].value = 0;//unmute
		pstep[i].delay = pdrv->unmute_cnt;
		i++;
		pstep[i].type = LCD_POWER_TYPE_MAX;
		i++;
		power_step->power_on_step_max = i;
	}

	append_more = 1;
	i = 0;
	pstep = pdrv->config.power.power_off_step;
	while (i < LCD_PWR_STEP_MAX) {
		len += 5;
		ret = lcd_unifykey_len_check(key_len, len);
		if (ret < 0) {
			pstep[i].type = 0xff;
			pstep[i].index = 0;
			pstep[i].value = 0;
			pstep[i].delay = 0;
			LCDERR("[%d]: unifykey power_off length is incorrect\n", pdrv->index);
			return -1;
		}
		pstep[i].type = *(p + LCD_UKEY_PWR_TYPE + 5 * i);
		pstep[i].index = *(p + LCD_UKEY_PWR_INDEX + 5 * i);
		pstep[i].value = *(p + LCD_UKEY_PWR_VAL + 5 * i);
		pstep[i].delay = (*(p + LCD_UKEY_PWR_DELAY + 5 * i) |
				((*(p + LCD_UKEY_PWR_DELAY + 5 * i + 1)) << 8));
		if (pstep[i].type >= LCD_POWER_TYPE_MAX) {
			i++;
			break;
		}

		/* gpio/extern probe */
		index = pstep[i].index;
		switch (pstep[i].type) {
		case LCD_POWER_TYPE_GPIO:
		case LCD_POWER_TYPE_WAIT_GPIO:
			if (index < LCD_CPU_GPIO_NUM_MAX)
				lcd_cpu_gpio_probe(pdrv, index);
			break;
#ifdef CONFIG_AMLOGIC_LCD_EXTERN
		case LCD_POWER_TYPE_EXTERN:
			lcd_resource_add(pdrv, LCD_RES_EXTERN, index);
			lcd_extern_dev_index_add(pdrv->index, index);
			break;
#endif
		case LCD_POWER_TYPE_BACKLIGHT:
		case LCD_POWER_TYPE_MUTE:
			append_more = 0;
			break;
		default:
			break;
		}
		i++;
	}
	power_step->power_off_step_max = i;

	if (append_more && i + 2 < LCD_POWER_TYPE_MAX) {
		i--;
		for (j = i + 2; j >= 2; j--)
			memcpy(&pstep[j], &pstep[j - 2], sizeof(struct lcd_power_step_s));
		power_step->power_off_step_max += 2;
		pstep[0].type  = LCD_POWER_TYPE_MUTE;
		pstep[0].index = 0;
		pstep[0].value = 1; //mute
		pstep[0].delay = pdrv->mute_cnt;

		pstep[1].type = LCD_POWER_TYPE_BACKLIGHT;
		pstep[1].index = 0;
		pstep[1].value = 0;//bl off
		pstep[1].delay = 0;
	}

	return 0;
}

static int lcd_vlock_param_load_from_unifykey(struct aml_lcd_drv_s *pdrv, unsigned char *buf)
{
	unsigned char *p;

	p = buf;

	pdrv->config.vlock_param[0] = LCD_VLOCK_PARAM_BIT_UPDATE;
	pdrv->config.vlock_param[1] = *(p + LCD_UKEY_VLOCK_VAL_0);
	pdrv->config.vlock_param[2] = *(p + LCD_UKEY_VLOCK_VAL_1);
	pdrv->config.vlock_param[3] = *(p + LCD_UKEY_VLOCK_VAL_2);
	pdrv->config.vlock_param[4] = *(p + LCD_UKEY_VLOCK_VAL_3);
	if (pdrv->config.vlock_param[1] ||
	    pdrv->config.vlock_param[2] ||
	    pdrv->config.vlock_param[3] ||
	    pdrv->config.vlock_param[4]) {
		LCDPR("[%d]: find vlock_attr\n", pdrv->index);
		pdrv->config.vlock_param[0] |= LCD_VLOCK_PARAM_BIT_VALID;
	}

	return 0;
}

static int lcd_optical_load_from_unifykey(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_optical_info_s *opt_info = &pdrv->config.optical;
	char key_str[15];
	unsigned char *para, *p;
	int key_len, len;
	int ret;

	memset(key_str, 0, 15);
	if (pdrv->index == 0)
		sprintf(key_str, "lcd_optical");
	else
		sprintf(key_str, "lcd%d_optical", pdrv->index);

	ret = lcd_unifykey_get_size(key_str, &key_len);
	if (ret < 0)
		return -1;
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s: find ukey: %s\n", pdrv->index, __func__, key_str);

	para = kzalloc(key_len, GFP_KERNEL);
	if (!para)
		return -1;

	ret = lcd_unifykey_get(key_str, para, key_len);
	if (ret < 0) {
		kfree(para);
		return -1;
	}

	/* check parameters */
	len = LCD_UKEY_OPTICAL_SIZE;
	ret = lcd_unifykey_len_check(key_len, len);
	if (ret < 0) {
		LCDERR("[%d]: %s: unifykey parameters length is incorrect\n",
		       pdrv->index, key_str);
		kfree(para);
		return -1;
	}

	/* attr (52Byte) */
	p = para;

	opt_info->hdr_support = (*(p + LCD_UKEY_OPT_HDR_SUPPORT) |
		((*(p + LCD_UKEY_OPT_HDR_SUPPORT + 1)) << 8) |
		((*(p + LCD_UKEY_OPT_HDR_SUPPORT + 2)) << 16) |
		((*(p + LCD_UKEY_OPT_HDR_SUPPORT + 3)) << 24));
	opt_info->features = (*(p + LCD_UKEY_OPT_FEATURES) |
		((*(p + LCD_UKEY_OPT_FEATURES + 1)) << 8) |
		((*(p + LCD_UKEY_OPT_FEATURES + 2)) << 16) |
		((*(p + LCD_UKEY_OPT_FEATURES + 3)) << 24));
	opt_info->primaries_r_x = (*(p + LCD_UKEY_OPT_PRI_R_X) |
		((*(p + LCD_UKEY_OPT_PRI_R_X + 1)) << 8) |
		((*(p + LCD_UKEY_OPT_PRI_R_X + 2)) << 16) |
		((*(p + LCD_UKEY_OPT_PRI_R_X + 3)) << 24));
	opt_info->primaries_r_y = (*(p + LCD_UKEY_OPT_PRI_R_Y) |
		((*(p + LCD_UKEY_OPT_PRI_R_Y + 1)) << 8) |
		((*(p + LCD_UKEY_OPT_PRI_R_Y + 2)) << 16) |
		((*(p + LCD_UKEY_OPT_PRI_R_Y + 3)) << 24));
	opt_info->primaries_g_x = (*(p + LCD_UKEY_OPT_PRI_G_X) |
		((*(p + LCD_UKEY_OPT_PRI_G_X + 1)) << 8) |
		((*(p + LCD_UKEY_OPT_PRI_G_X + 2)) << 16) |
		((*(p + LCD_UKEY_OPT_PRI_G_X + 3)) << 24));
	opt_info->primaries_g_y = (*(p + LCD_UKEY_OPT_PRI_G_Y) |
		((*(p + LCD_UKEY_OPT_PRI_G_Y + 1)) << 8) |
		((*(p + LCD_UKEY_OPT_PRI_G_Y + 2)) << 16) |
		((*(p + LCD_UKEY_OPT_PRI_G_Y + 3)) << 24));
	opt_info->primaries_b_x = (*(p + LCD_UKEY_OPT_PRI_B_X) |
		((*(p + LCD_UKEY_OPT_PRI_B_X + 1)) << 8) |
		((*(p + LCD_UKEY_OPT_PRI_B_X + 2)) << 16) |
		((*(p + LCD_UKEY_OPT_PRI_B_X + 3)) << 24));
	opt_info->primaries_b_y = (*(p + LCD_UKEY_OPT_PRI_B_Y) |
		((*(p + LCD_UKEY_OPT_PRI_B_Y + 1)) << 8) |
		((*(p + LCD_UKEY_OPT_PRI_B_Y + 2)) << 16) |
		((*(p + LCD_UKEY_OPT_PRI_B_Y + 3)) << 24));
	opt_info->white_point_x = (*(p + LCD_UKEY_OPT_WHITE_X) |
		((*(p + LCD_UKEY_OPT_WHITE_X + 1)) << 8) |
		((*(p + LCD_UKEY_OPT_WHITE_X + 2)) << 16) |
		((*(p + LCD_UKEY_OPT_WHITE_X + 3)) << 24));
	opt_info->white_point_y = (*(p + LCD_UKEY_OPT_WHITE_Y) |
		((*(p + LCD_UKEY_OPT_WHITE_Y + 1)) << 8) |
		((*(p + LCD_UKEY_OPT_WHITE_Y + 2)) << 16) |
		((*(p + LCD_UKEY_OPT_WHITE_Y + 3)) << 24));
	opt_info->luma_max = (*(p + LCD_UKEY_OPT_LUMA_MAX) |
		((*(p + LCD_UKEY_OPT_LUMA_MAX + 1)) << 8) |
		((*(p + LCD_UKEY_OPT_LUMA_MAX + 2)) << 16) |
		((*(p + LCD_UKEY_OPT_LUMA_MAX + 3)) << 24));
	opt_info->luma_min = (*(p + LCD_UKEY_OPT_LUMA_MIN) |
		((*(p + LCD_UKEY_OPT_LUMA_MIN + 1)) << 8) |
		((*(p + LCD_UKEY_OPT_LUMA_MIN + 2)) << 16) |
		((*(p + LCD_UKEY_OPT_LUMA_MIN + 3)) << 24));
	opt_info->luma_avg = (*(p + LCD_UKEY_OPT_LUMA_AVG) |
		((*(p + LCD_UKEY_OPT_LUMA_AVG + 1)) << 8) |
		((*(p + LCD_UKEY_OPT_LUMA_AVG + 2)) << 16) |
		((*(p + LCD_UKEY_OPT_LUMA_AVG + 3)) << 24));

	opt_info->ldim_support = *(p + LCD_UKEY_OPT_ADV_FLAG0);
	opt_info->luma_peak = *(unsigned int *)(p + LCD_UKEY_OPT_ADV_VAL1);

	kfree(para);

	lcd_optical_vinfo_update(pdrv);

	return 0;
}

static int lcd_config_load_from_unifykey_v2(struct aml_lcd_drv_s *pdrv,
					    unsigned char *p,
					    unsigned int key_len,
					    unsigned int offset)
{
	struct aml_lcd_unifykey_header_s *lcd_header;
	struct phy_config_s *phy_cfg = &pdrv->config.phy_cfg;
	struct phy_attr_s *phy = NULL;
	unsigned int len, size;
	unsigned char version;
	int i, ret;

	if (!phy_cfg->phys[0])
		return -1;

	lcd_header = (struct aml_lcd_unifykey_header_s *)p;
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		lcd_unifykey_header_print(p);

	/* step 2: check lcd parameters */
	len = offset + lcd_header->block_cur_size;
	ret = lcd_unifykey_len_check(key_len, len);
	if (ret < 0) {
		LCDERR("unifykey parameters length is incorrect\n");
		return -1;
	}

	/*phy 356byte*/
	phy_cfg->flag = (*(p + LCD_UKEY_PHY_ATTR_FLAG) |
		((*(p + LCD_UKEY_PHY_ATTR_FLAG + 1)) << 8) |
		((*(p + LCD_UKEY_PHY_ATTR_FLAG + 2)) << 16) |
		((*(p + LCD_UKEY_PHY_ATTR_FLAG + 3)) << 24));
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("%s: ctrl_flag=0x%x\n", __func__, phy_cfg->flag);

	phy = phy_cfg->phys[0];
	if (phy_cfg->flag & PHY_BIT_VSWING) {
		phy->vswing = (*(p + LCD_UKEY_PHY_ATTR_0) |
				*(p + LCD_UKEY_PHY_ATTR_0 + 1) << 8);
	}
	if (phy_cfg->flag & PHY_BIT_VCM) {
		phy->vcm = (*(p + LCD_UKEY_PHY_ATTR_1) |
				*(p + LCD_UKEY_PHY_ATTR_1 + 1) << 8);
	}
	if (phy_cfg->flag & PHY_BIT_REF_BIAS) {
		phy->ref_bias = (*(p + LCD_UKEY_PHY_ATTR_2) |
				*(p + LCD_UKEY_PHY_ATTR_2 + 1) << 8);
	}
	if (phy_cfg->flag & PHY_BIT_ODT) {
		phy->odt = (*(p + LCD_UKEY_PHY_ATTR_3) |
				*(p + LCD_UKEY_PHY_ATTR_3 + 1) << 8);
	}
	if (phy_cfg->flag & PHY_BIT_CV_MODE) {
		phy->cv_mode = (*(p + LCD_UKEY_PHY_ATTR_4) |
				*(p + LCD_UKEY_PHY_ATTR_4 + 1) << 8);
	}
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("%s: vswing=0x%x, vcm=0x%x, ref_bias=0x%x, odt=0x%x, cv_mode=%d\n",
		      __func__, phy->vswing, phy->vcm, phy->ref_bias,
		      phy->cv_mode, phy->odt);
	}

	if (phy_cfg->flag & PHY_BIT_LANE_PREEM) {
		for (i = 0; i < phy_cfg->lane_num; i++) {
			phy->lane[i].preem =
				*(p + LCD_UKEY_PHY_LANE_CTRL + 4 * i) |
				(*(p + LCD_UKEY_PHY_LANE_CTRL + 4 * i + 1) << 8);
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("%s: lane[%d]: preem=0x%x\n",
				      __func__, i,
				      phy->lane[i].preem);
			}
		}
	}

	if (phy_cfg->flag & PHY_BIT_LANE_AMP) {
		for (i = 0; i < phy_cfg->lane_num; i++) {
			phy->lane[i].amp =
				*(p + LCD_UKEY_PHY_LANE_CTRL + 4 * i + 2) |
				(*(p + LCD_UKEY_PHY_LANE_CTRL + 4 * i + 3) << 8);
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("%s: lane[%d]: amp=0x%x\n",
				      __func__, i,
				      phy->lane[i].amp);
			}
		}
	}

	if (phy_cfg->flag & PHY_BIT_LANE_SEL) {
		for (i = 0; i < phy_cfg->lane_num; i++) {
			phy_cfg->ch_ctrl[i].sel = *(p + LCD_UKEY_PHY_LANE_SEL + i);
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("%s: lane[%d]: sel=0x%x\n",
					__func__, i, phy_cfg->ch_ctrl[i].sel);
			}
		}
	}

	size = LCD_UKEY_CUS_CTRL_ATTR_FLAG;
	version = lcd_header->version;
	lcd_cus_ctrl_load_from_unifykey(pdrv, (p + size), (key_len - size), version);

	return 0;
}

static int lcd_config_load_from_unifykey_v3(struct aml_lcd_drv_s *pdrv,
					    unsigned char *p,
					    unsigned int key_len,
					    unsigned int offset)
{
	struct aml_lcd_unifykey_header_s *lcd_header;
	unsigned int len, size;
	unsigned char version;
	int ret;

	lcd_header = (struct aml_lcd_unifykey_header_s *)p;
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		lcd_unifykey_header_print(p);

	/* step 2: check lcd parameters */
	len = offset + lcd_header->block_cur_size;
	ret = lcd_unifykey_len_check(key_len, len);
	if (ret < 0) {
		LCDERR("unifykey parameters length is incorrect\n");
		return -1;
	}

	size = LCD_UKEY_CUS_CTRL_ATTR_FLAG_V3;
	version = lcd_header->version;
	lcd_cus_ctrl_load_from_unifykey(pdrv, (p + size), (key_len - size), version);

	return 0;
}

static int lcd_config_load_from_unifykey(struct aml_lcd_drv_s *pdrv, char *key_str)
{
	unsigned char *para;
	int key_len, len;
	unsigned char *p, val;
	const char *str;
	struct aml_lcd_unifykey_header_s *lcd_header;
	struct lcd_config_s *pconf = &pdrv->config;
	struct lcd_detail_timing_s *ptiming = NULL;
	struct phy_config_s *phy_cfg = &pdrv->config.phy_cfg;
	struct phy_attr_s *phy = NULL;
	union lcd_ctrl_config_u *pctrl = &pdrv->config.control;
	unsigned int temp, lcd_bits = 0;
	int ret;

	ret = lcd_unifykey_get_size(key_str, &key_len);
	if (ret)
		return -1;

	para = kzalloc(key_len, GFP_KERNEL);
	if (!para)
		return -1;

	ret = lcd_unifykey_get(key_str, para, key_len);
	if (ret < 0) {
		kfree(para);
		return -1;
	}

	/* step 1: check header size */
	lcd_header = (struct aml_lcd_unifykey_header_s *)para;
	len = LCD_UKEY_DATA_LEN_V1; /*10+36+18+31+20*/
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		lcd_unifykey_header_print(para);

	/* step 2: check lcd parameters */
	ret = lcd_unifykey_len_check(key_len, len);
	if (ret < 0) {
		LCDERR("[%d]: unifykey parameters length is incorrect\n", pdrv->index);
		goto load_from_unifykey_exit;
	}

	/* panel_type update */
	sprintf(pconf->propname, "%s", "unifykey");

	/* basic: 36byte */
	p = para;
	str = (const char *)(p + LCD_UKEY_HEAD_SIZE);
	strscpy(pconf->basic.model_name, str, MOD_LEN_MAX);
	/* ensure string ending */
	pconf->basic.model_name[MOD_LEN_MAX - 1] = '\0';
	temp = *(p + LCD_UKEY_INTERFACE);
	pconf->basic.lcd_type = temp & 0x3f;
	pconf->basic.config_check = (temp >> 6) & 0x3;
	temp = *(p + LCD_UKEY_LCD_BITS_CFMT);
	lcd_bits = (temp & 0x3f) * 3;
	pconf->basic.screen_width = (*(p + LCD_UKEY_SCREEN_WIDTH) |
		((*(p + LCD_UKEY_SCREEN_WIDTH + 1)) << 8));
	pconf->basic.screen_height = (*(p + LCD_UKEY_SCREEN_HEIGHT) |
		((*(p + LCD_UKEY_SCREEN_HEIGHT + 1)) << 8));

	/* timing: 18byte */
	ptiming = lcd_timing_alloc(pdrv);
	if (!ptiming) {
		ret = -1;
		LCDERR("%s timing alloc fail\n", __func__);
		goto load_from_unifykey_exit;
	}
	memset(ptiming, 0, sizeof(*ptiming));
	ptiming->h_active = (*(p + LCD_UKEY_H_ACTIVE) |
		((*(p + LCD_UKEY_H_ACTIVE + 1)) << 8));
	ptiming->v_active = (*(p + LCD_UKEY_V_ACTIVE)) |
		((*(p + LCD_UKEY_V_ACTIVE + 1)) << 8);
	ptiming->h_period = (*(p + LCD_UKEY_H_PERIOD)) |
		((*(p + LCD_UKEY_H_PERIOD + 1)) << 8);
	ptiming->v_period = (*(p + LCD_UKEY_V_PERIOD)) |
		((*(p + LCD_UKEY_V_PERIOD + 1)) << 8);
	temp = *(unsigned short *)(p + LCD_UKEY_HS_WIDTH_POL);
	ptiming->hsync_width = temp & 0xfff;
	ptiming->hsync_pol = (temp >> 12) & 0xf;
	ptiming->hsync_bp = (*(p + LCD_UKEY_HS_BP) |
		((*(p + LCD_UKEY_HS_BP + 1)) << 8));
	ptiming->hsync_fp = ptiming->h_period - ptiming->h_active -
			ptiming->hsync_width - ptiming->hsync_bp;
	temp = *(unsigned short *)(p + LCD_UKEY_VS_WIDTH_POL);
	ptiming->vsync_width = temp & 0xfff;
	ptiming->vsync_pol = (temp >> 12) & 0xf;
	ptiming->vsync_bp = (*(p + LCD_UKEY_VS_BP) |
		((*(p + LCD_UKEY_VS_BP + 1)) << 8));
	ptiming->vsync_fp = ptiming->v_period - ptiming->v_active -
			ptiming->vsync_width - ptiming->vsync_bp;
	pconf->timing.pre_de_h = *(p + LCD_UKEY_PRE_DE_H);
	pconf->timing.pre_de_v = *(p + LCD_UKEY_PRE_DE_V);

	/* customer: 31byte */
	ptiming->fr_adjust_type = *(p + LCD_UKEY_FR_ADJ_TYPE);
	pconf->timing.ss_level = *(p + LCD_UKEY_SS_LEVEL);
	val = *(p + LCD_UKEY_CUST_VAL0);
	pconf->timing.clk_mode = (val >> 4) & 0xf;
	pconf->timing.pll_flag = val & 0xf;
	ptiming->pixel_clk = (*(p + LCD_UKEY_PCLK) |
		((*(p + LCD_UKEY_PCLK + 1)) << 8) |
		((*(p + LCD_UKEY_PCLK + 2)) << 16) |
		((*(p + LCD_UKEY_PCLK + 3)) << 24));
	ptiming->h_period_min = (*(p + LCD_UKEY_H_PERIOD_MIN) |
		((*(p + LCD_UKEY_H_PERIOD_MIN + 1)) << 8));
	ptiming->h_period_max = (*(p + LCD_UKEY_H_PERIOD_MAX) |
		((*(p + LCD_UKEY_H_PERIOD_MAX + 1)) << 8));
	ptiming->v_period_min = (*(p + LCD_UKEY_V_PERIOD_MIN) |
		((*(p + LCD_UKEY_V_PERIOD_MIN + 1)) << 8));
	ptiming->v_period_max = (*(p + LCD_UKEY_V_PERIOD_MAX) |
		((*(p + LCD_UKEY_V_PERIOD_MAX + 1)) << 8));
	ptiming->pclk_min = (*(p + LCD_UKEY_PCLK_MIN) |
		((*(p + LCD_UKEY_PCLK_MIN + 1)) << 8) |
		((*(p + LCD_UKEY_PCLK_MIN + 2)) << 16) |
		((*(p + LCD_UKEY_PCLK_MIN + 3)) << 24));
	ptiming->pclk_max = (*(p + LCD_UKEY_PCLK_MAX) |
		((*(p + LCD_UKEY_PCLK_MAX + 1)) << 8) |
		((*(p + LCD_UKEY_PCLK_MAX + 2)) << 16) |
		((*(p + LCD_UKEY_PCLK_MAX + 3)) << 24));
	ptiming->frame_rate_min = *(p + LCD_UKEY_FRAME_RATE_MIN);
	ptiming->frame_rate_max = *(p + LCD_UKEY_FRAME_RATE_MAX);
	ptiming->lcd_bits = lcd_bits;

	val = *(p + LCD_UKEY_CUST_VAL1);
	pconf->timing.ppc = (val >> 4) & 0xf;
	pconf->custom_pinmux = val & 0xf;

	pconf->fr_auto_cus = *(p + LCD_UKEY_FR_AUTO_CUS);
	ptiming->switch_type = LCD_VMODE_SWITCH_NONE;
	ptiming->ss_force = 0;
	ptiming->ss_freq = 255;
	ptiming->ss_level = pconf->timing.ss_level;
	ptiming->ss_mode = 255;

	pconf->timing.dft_timing = pconf->timing.timings[0];
	lcd_clk_frame_rate_init(ptiming);
	lcd_config_timing_check(pdrv, ptiming);
	lcd_default_to_basic_timing_init_config(pdrv);

	/* interface: 20byte */
	switch (pconf->basic.lcd_type) {
	case LCD_LVDS:
		pctrl->lvds_cfg.lvds_repack = *(p + LCD_UKEY_IF_ATTR_0) |
			((*(p + LCD_UKEY_IF_ATTR_0 + 1)) << 8);
		pctrl->lvds_cfg.dual_port = *(p + LCD_UKEY_IF_ATTR_1) |
			((*(p + LCD_UKEY_IF_ATTR_1 + 1)) << 8);
		pctrl->lvds_cfg.pn_swap = *(p + LCD_UKEY_IF_ATTR_2) |
			((*(p + LCD_UKEY_IF_ATTR_2 + 1)) << 8);
		pctrl->lvds_cfg.port_swap = *(p + LCD_UKEY_IF_ATTR_3) |
			((*(p + LCD_UKEY_IF_ATTR_3 + 1)) << 8);
		pctrl->lvds_cfg.phy_vswing = *(p + LCD_UKEY_IF_ATTR_4) |
			((*(p + LCD_UKEY_IF_ATTR_4 + 1)) << 8);
		pctrl->lvds_cfg.phy_preem = *(p + LCD_UKEY_IF_ATTR_5) |
			((*(p + LCD_UKEY_IF_ATTR_5 + 1)) << 8);
		pctrl->lvds_cfg.lane_reverse = *(p + LCD_UKEY_IF_ATTR_8) |
			((*(p + LCD_UKEY_IF_ATTR_8 + 1)) << 8);

		phy_cfg->vswing_level = pctrl->lvds_cfg.phy_vswing & 0xf;
		phy_cfg->ext_pullup = (pctrl->lvds_cfg.phy_vswing >> 4) & 0x3;
		phy_cfg->preem_level = pctrl->lvds_cfg.phy_preem;
		break;
	case LCD_VBYONE:
		pctrl->vbyone_cfg.lane_count = *(p + LCD_UKEY_IF_ATTR_0) |
			((*(p + LCD_UKEY_IF_ATTR_0 + 1)) << 8);
		pctrl->vbyone_cfg.region_num = *(p + LCD_UKEY_IF_ATTR_1) |
			((*(p + LCD_UKEY_IF_ATTR_1 + 1)) << 8);
		pctrl->vbyone_cfg.byte_mode = *(p + LCD_UKEY_IF_ATTR_2) |
			((*(p + LCD_UKEY_IF_ATTR_2 + 1)) << 8);
		pctrl->vbyone_cfg.color_fmt = *(p + LCD_UKEY_IF_ATTR_3) |
			((*(p + LCD_UKEY_IF_ATTR_3 + 1)) << 8);
		pctrl->vbyone_cfg.phy_vswing = *(p + LCD_UKEY_IF_ATTR_4) |
			((*(p + LCD_UKEY_IF_ATTR_4 + 1)) << 8);
		pctrl->vbyone_cfg.phy_preem = *(p + LCD_UKEY_IF_ATTR_5) |
			((*(p + LCD_UKEY_IF_ATTR_5 + 1)) << 8);
		pctrl->vbyone_cfg.hw_filter_time =
			*(p + LCD_UKEY_IF_ATTR_8) |
			((*(p + LCD_UKEY_IF_ATTR_8 + 1)) << 8);
		pctrl->vbyone_cfg.hw_filter_cnt =
			*(p + LCD_UKEY_IF_ATTR_9) |
			((*(p + LCD_UKEY_IF_ATTR_9 + 1)) << 8);
		pctrl->vbyone_cfg.ctrl_flag = 0;
		pctrl->vbyone_cfg.power_on_reset_delay = VX1_PWR_ON_RESET_DLY_DFT;
		pctrl->vbyone_cfg.hpd_data_delay = VX1_HPD_DATA_DELAY_DFT;
		pctrl->vbyone_cfg.cdr_training_hold = VX1_CDR_TRAINING_HOLD_DFT;
		pctrl->vbyone_cfg.slice = pdrv->config.timing.ppc ? pdrv->config.timing.ppc : 1;

		phy_cfg->vswing_level = pctrl->vbyone_cfg.phy_vswing & 0xf;
		phy_cfg->ext_pullup = (pctrl->vbyone_cfg.phy_vswing >> 4) & 0x3;
		phy_cfg->preem_level = pctrl->vbyone_cfg.phy_preem;
		break;
	case LCD_MLVDS:
		pctrl->mlvds_cfg.channel_num = *(p + LCD_UKEY_IF_ATTR_0) |
			((*(p + LCD_UKEY_IF_ATTR_0 + 1)) << 8);
		pctrl->mlvds_cfg.channel_sel0 = *(p + LCD_UKEY_IF_ATTR_1) |
			((*(p + LCD_UKEY_IF_ATTR_1 + 1)) << 8) |
			(*(p + LCD_UKEY_IF_ATTR_2) << 16) |
			((*(p + LCD_UKEY_IF_ATTR_2 + 1)) << 24);
		pctrl->mlvds_cfg.channel_sel1 = *(p + LCD_UKEY_IF_ATTR_3) |
			((*(p + LCD_UKEY_IF_ATTR_3 + 1)) << 8) |
			(*(p + LCD_UKEY_IF_ATTR_4) << 16) |
			((*(p + LCD_UKEY_IF_ATTR_4 + 1)) << 24);
		pctrl->mlvds_cfg.clk_phase = *(p + LCD_UKEY_IF_ATTR_5) |
			((*(p + LCD_UKEY_IF_ATTR_5 + 1)) << 8);
		pctrl->mlvds_cfg.pn_swap = *(p + LCD_UKEY_IF_ATTR_6) |
			((*(p + LCD_UKEY_IF_ATTR_6 + 1)) << 8);
		pctrl->mlvds_cfg.bit_swap = *(p + LCD_UKEY_IF_ATTR_7) |
			((*(p + LCD_UKEY_IF_ATTR_7 + 1)) << 8);
		pctrl->mlvds_cfg.phy_vswing = *(p + LCD_UKEY_IF_ATTR_8) |
			((*(p + LCD_UKEY_IF_ATTR_8 + 1)) << 8);
		pctrl->mlvds_cfg.phy_preem = *(p + LCD_UKEY_IF_ATTR_9) |
			((*(p + LCD_UKEY_IF_ATTR_9 + 1)) << 8);

		phy_cfg->vswing_level = pctrl->mlvds_cfg.phy_vswing & 0xf;
		phy_cfg->ext_pullup = (pctrl->mlvds_cfg.phy_vswing >> 4) & 0x3;
		phy_cfg->preem_level = pctrl->mlvds_cfg.phy_preem;
		break;
	case LCD_P2P:
		pctrl->p2p_cfg.p2p_type = *(p + LCD_UKEY_IF_ATTR_0) |
			((*(p + LCD_UKEY_IF_ATTR_0 + 1)) << 8);
		pctrl->p2p_cfg.lane_num = *(p + LCD_UKEY_IF_ATTR_1) |
			((*(p + LCD_UKEY_IF_ATTR_1 + 1)) << 8);
		pctrl->p2p_cfg.channel_sel0 = *(p + LCD_UKEY_IF_ATTR_2) |
			((*(p + LCD_UKEY_IF_ATTR_2 + 1)) << 8) |
			(*(p + LCD_UKEY_IF_ATTR_3) << 16) |
			((*(p + LCD_UKEY_IF_ATTR_3 + 1)) << 24);
		pctrl->p2p_cfg.channel_sel1 = *(p + LCD_UKEY_IF_ATTR_4) |
			((*(p + LCD_UKEY_IF_ATTR_4 + 1)) << 8) |
			(*(p + LCD_UKEY_IF_ATTR_5) << 16) |
			((*(p + LCD_UKEY_IF_ATTR_5 + 1)) << 24);
		pctrl->p2p_cfg.pn_swap = *(p + LCD_UKEY_IF_ATTR_6) |
			((*(p + LCD_UKEY_IF_ATTR_6 + 1)) << 8);
		pctrl->p2p_cfg.bit_swap = *(p + LCD_UKEY_IF_ATTR_7) |
			((*(p + LCD_UKEY_IF_ATTR_7 + 1)) << 8);
		pctrl->p2p_cfg.phy_vswing = *(p + LCD_UKEY_IF_ATTR_8) |
			((*(p + LCD_UKEY_IF_ATTR_8 + 1)) << 8);
		pctrl->p2p_cfg.phy_preem = *(p + LCD_UKEY_IF_ATTR_9) |
			((*(p + LCD_UKEY_IF_ATTR_9 + 1)) << 8);

		phy_cfg->vswing_level = pctrl->p2p_cfg.phy_vswing & 0xf;
		phy_cfg->ext_pullup = (pctrl->p2p_cfg.phy_vswing >> 4) & 0x3;
		phy_cfg->preem_level = pctrl->p2p_cfg.phy_preem;
		break;
	default:
		LCDERR("[%d]: unsupport lcd_type: %d\n",
		       pdrv->index, pconf->basic.lcd_type);
		break;
	}
	phy = lcd_phy_alloc(pdrv);// first parse, it is only be phy_cfg->phys[0];
	if (!phy) {
		ret = -1;
		LCDERR("%s phy alloc fail\n", __func__);
		goto load_from_unifykey_exit;
	}
	memset(phy, 0, sizeof(*phy));
	phy_cfg->act_phy = phy_cfg->phys[0];
	lcd_phy_param_preset(pdrv);
	lcd_lane_map_preset(pdrv);
	phy->ss.freq = 255;
	phy->ss.level = 255;
	phy->ss.mode = 255;

	lcd_vlock_param_load_from_unifykey(pdrv, para);

	/* step 3: check power sequence */
	ret = lcd_power_load_from_unifykey(pdrv, para, key_len, len);
	if (ret < 0) {
		LCDERR("%s power parse fail\n", __func__);
		goto load_from_unifykey_exit;
	}

	p = para + lcd_header->block_cur_size;
	switch (lcd_header->version) {
	case 2:
		lcd_config_load_from_unifykey_v2(pdrv, p, key_len, lcd_header->block_cur_size);
		break;
	case 3:
		lcd_config_load_from_unifykey_v3(pdrv, p, key_len, lcd_header->block_cur_size);
		break;
	default:
		break;
	}

	//fix ss in detail timing and phy_attr if not config
	lcd_ss_config_fix(pdrv);

	lcd_optical_load_from_unifykey(pdrv);

#ifdef CONFIG_AMLOGIC_BACKLIGHT
	lcd_resource_add(pdrv, LCD_RES_BACKLIGHT, 0);
	aml_bl_index_add(pdrv->index, 0);
#endif

load_from_unifykey_exit:
	kfree(para);

	return ret;
}

/* json parse =================================================================================== */

static struct num_str_s p2p_type_name[] = {
	{P2P_CEDS, "CEDS"},
	{P2P_CMPI, "CMPI"},
	{P2P_ISP,  "ISP"},
	{P2P_EPI,  "EPI"},
	{P2P_CHPI, "CHPI"},
	{P2P_CSPI, "CSPI"},
	{P2P_USIT, "USIT"},
	{P2P_MAX,  "Invalid"}
};

static struct num_str_s vmode_switch_name[] = {
	{LCD_VMODE_SWITCH_NONE,  "NONE"},
	{LCD_VMODE_SWITCH_FULL,  "FULL"},
	{LCD_VMODE_SWITCH_LIMIT, "LIMIT"},
	{LCD_VMODE_SWITCH_MIN,   "MIN"},
};

struct color_fmt_info_s color_fmt_info[] = {
	{CFMT_RGB565,         16, "RGB565"},
	{CFMT_RGB_6bit,       18, "RGB_6bit"},
	{CFMT_RGB_8bit,       24, "RGB_8bit"},
	{CFMT_RGB_10bit,      30, "RGB_10bit"},
	{CFMT_RGB_12bit,      36, "RGB_12bit"},
	{CFMT_YCbCr422_8bit,  16, "YCbCr422_8bit"},
	{CFMT_YCbCr422_10bit, 20, "YCbCr422_10bit"},
	{CFMT_YCbCr422_12bit, 24, "YCbCr422_12bit"},
	{CFMT_YCbCr444_8bit,  24, "YCbCr444_8bit"},
	{CFMT_YCbCr444_10bit, 30, "YCbCr444_10bit"},
	{CFMT_YCbCr444_12bit, 36, "YCbCr444_12bit"},
	{CFMT_YCbCr420_8bit,  12, "YCbCr420_8bit"},
	{CFMT_YCbCr420_10bit, 15, "YCbCr420_10bit"},
	{CFMT_YCbCr420_12bit, 18, "YCbCr420_12bit"},
};

static int panel_str2fmt(const char *str, unsigned char *cfmt, unsigned char *bits)
{
	unsigned int i = 0;

	if (!str)
		return -1;
	for (i = 0; i < ARRAY_SIZE(color_fmt_info); i++) {
		if (strcmp(str, color_fmt_info[i].name) == 0) {
			*cfmt = color_fmt_info[i].cfmt;
			*bits = color_fmt_info[i].bits;
			return 0;
		}
	}

	return -1;
}

static int lcd_panel_parse_basic(struct json_parse_s *jsp, struct aml_lcd_drv_s *pdrv)
{
	struct json_s *json, *child;
	const char *str = NULL;
	struct lcd_basic_s *cfg = &pdrv->config.basic;

	json = json_path_to_node(jsp, jsp->root, "/basic");
	if (!json) {
		LCDERR("find /basic\n");
		return -1;
	}

	str = json_get_obj_str(jsp, json, "model_name", "invalid");
	sprintf(cfg->model_name, "%s", str ? str : "invalid");

	str = json_get_obj_str(jsp, json, "interface", "invalid");
	cfg->lcd_type = lcd_type_str_to_type(str);

	cfg->config_check = json_get_obj_u32(jsp, json, "config_check", 1);
	pdrv->config.custom_pinmux = json_get_obj_u32(jsp, json, "custom_pinmux", 0);

	child = json_get_object_child(jsp, json, "screen_size");
	cfg->screen_width = json_get_arr_u32(jsp, child, 0, 16);
	cfg->screen_height = json_get_arr_u32(jsp, child, 1, 9);

	return 0;
}

static int lcd_panel_parse_timing(struct json_parse_s *jsp, struct aml_lcd_drv_s *pdrv)
{
	struct json_s *parent, *child, *child2;
	const char *str = NULL;
	char strtmp[64];
	int cnt = 1, i = 0, bits = 8;
	struct lcd_detail_timing_s *dt;
	struct lcd_timing_s *tims = &pdrv->config.timing;

	parent = json_path_to_node(jsp, jsp->root, "/timing");
	if (!parent) {
		LCDERR("find /timing\n");
		return -1;
	}
	tims->ppc      = json_get_obj_u32(jsp, parent, "ppc_mode", 1);
	tims->clk_mode = json_get_obj_u32(jsp, parent, "clk_mode", LCD_CLK_MODE_DEPENDENCE);
	tims->pll_flag = json_get_obj_u32(jsp, parent, "pll_flag", 1);

	parent         = json_get_object_child(jsp, parent, "pre_de");
	tims->pre_de_h = json_get_arr_u32(jsp, parent, 0, 0);
	tims->pre_de_v = json_get_arr_u32(jsp, parent, 1, 0);

	parent = json_path_to_node(jsp, jsp->root, "/timing/timing");
	cnt = json_get_array_size(jsp, parent);
	if (cnt <= 0) {
		LCDERR("/timing/timing error\n");
		return -1;
	}

	for (i = 0; i < cnt; i++) {
		if (tims->num_timings >= LCD_MAX_NUM_TIMINGS)
			break;
		child = json_get_array_child(jsp, parent, i);
		if (!child) {
			LCDPR("fail find  timing[%d]\n", i);
			break;
		}
		dt = lcd_timing_alloc(pdrv);
		if (!dt)
			break;

		if (dt != tims->timings[0])
			memcpy(dt, tims->timings[0], sizeof(*dt));
		else
			memset(dt, 0, sizeof(*dt));

		dt->fr_adjust_type = json_get_obj_u32(jsp, child, "fr_adj_type",
						      dt->fr_adjust_type);
		dt->lcd_bits = 24;
		dt->cfmt = CFMT_RGB_8bit;
		bits = json_get_obj_u32(jsp, child, "lcd_bits", 8);
		str = json_get_obj_str(jsp, child, "color_fmt", NULL);
		if (str) {
			if (strcmp(str, "RGB565"))
				snprintf(strtmp, 63, "%s_%dbit", str, bits);
			else
				snprintf(strtmp, 63, "%s", str);

			panel_str2fmt(strtmp, &dt->cfmt, &dt->lcd_bits);
		}
		str = json_get_obj_str(jsp, child, "mode_switch_type", NULL);
		dt->switch_type = strnum_get_num(str, vmode_switch_name,
						 ARRAY_SIZE(vmode_switch_name),
						 LCD_VMODE_SWITCH_NONE);

		child2 = json_get_object_child(jsp, child, "timing");
		if (!child2 && dt == tims->timings[0]) {
			LCDPR("fail find  timing[0]->timing\n");
			lcd_timing_free_last(pdrv);
			return -1;
		}
		if (!child2) {
			LCDPR("fail find  timing[%d]->timing\n", i);
			continue;
		}
		dt->h_period    = json_get_arr_u32(jsp, child2, 0, dt->h_period);
		dt->h_active    = json_get_arr_u32(jsp, child2, 1, dt->h_active);
		dt->hsync_width = json_get_arr_u32(jsp, child2, 2, dt->hsync_width);
		dt->hsync_bp    = json_get_arr_u32(jsp, child2, 3, dt->hsync_bp);
		dt->hsync_pol   = json_get_arr_u32(jsp, child2, 4, dt->hsync_pol);
		dt->v_period    = json_get_arr_u32(jsp, child2, 5, dt->v_period);
		dt->v_active    = json_get_arr_u32(jsp, child2, 6, dt->v_active);
		dt->vsync_width = json_get_arr_u32(jsp, child2, 7, dt->vsync_width);
		dt->vsync_bp    = json_get_arr_u32(jsp, child2, 8, dt->vsync_bp);
		dt->vsync_pol   = json_get_arr_u32(jsp, child2, 9, dt->vsync_pol);
		dt->hsync_fp = dt->h_period - dt->h_active - dt->hsync_width - dt->hsync_bp;
		dt->vsync_fp = dt->v_period - dt->v_active - dt->vsync_width - dt->vsync_bp;

		child2 = json_get_object_child(jsp, child, "period_range");
		if (child2) {
			dt->h_period_min = json_get_arr_u32(jsp, child2, 0, dt->h_period_min);
			dt->h_period_max = json_get_arr_u32(jsp, child2, 1, dt->h_period_max);
			dt->v_period_min = json_get_arr_u32(jsp, child2, 2, dt->v_period_min);
			dt->v_period_max = json_get_arr_u32(jsp, child2, 3, dt->v_period_max);
		}

		child2 = json_get_object_child(jsp, child, "pclk_range");
		if (child2) {
			dt->pclk_min  = json_get_arr_u32(jsp, child2, 0, dt->pclk_min);
			dt->pclk_max  = json_get_arr_u32(jsp, child2, 1, dt->pclk_max);
			dt->pixel_clk = json_get_arr_u32(jsp, child2, 2, dt->pixel_clk);
		}

		child2 = json_get_object_child(jsp, child, "fr_range");
		if (child2) {
			dt->frame_rate_min = json_get_arr_u32(jsp, child2, 0, dt->frame_rate_min);
			dt->frame_rate_max = json_get_arr_u32(jsp, child2, 1, dt->frame_rate_max);
		}

		child2 = json_get_object_child(jsp, child, "ssc");
		if (child2) {
			dt->ss_level = json_get_obj_u32(jsp, child2, "level", 0);
			dt->ss_freq  = json_get_obj_u32(jsp, child2, "freq", 0);
			dt->ss_mode  = json_get_obj_u32(jsp, child2, "mode", 0);
			dt->ss_force = json_get_obj_u32(jsp, child2, "force", 0);
		}

		lcd_clk_frame_rate_init(dt);
		lcd_config_timing_check(pdrv, dt);
	}
	tims->dft_timing = tims->timings[0];
	lcd_default_to_basic_timing_init_config(pdrv);

	return 0;
}

static int lcd_panel_parse_phy(struct json_parse_s *jsp, struct aml_lcd_drv_s *pdrv)
{
	struct json_s *parent, *child, *child2;
	const char *str = NULL;
	int cnt = 1, cnt2, i = 0, k;
	struct phy_config_s *phy_cfg;
	struct phy_attr_s *phy;
	struct ss_config_s *ss;

	parent = json_get_object_child(jsp, jsp->root, "phy");
	if (!parent) {
		LCDERR("find /phy\n");
		return -1;
	}

	phy_cfg = &pdrv->config.phy_cfg;
	phy = lcd_phy_alloc(pdrv);
	if (!phy) { //phy_cfg->phys[0] default phy
		LCDERR("%s dft phy alloc failed\n", __func__);
		return -1;
	}
	memset(phy, 0, sizeof(*phy));
	phy_cfg->act_phy = phy_cfg->phys[0];
	lcd_phy_param_preset(pdrv);
	lcd_lane_map_preset(pdrv);

	phy_cfg->lane_num = json_get_obj_u32(jsp, parent, "lane_num", phy_cfg->lane_num);
	child = json_get_object_child(jsp, parent, "ch_sel");
	if (child) {
		cnt = json_get_array_size(jsp, child);
		cnt = lcd_s32_constraint(cnt, 0, phy_cfg->lane_num);
		for (i = 0; i < cnt; i++)
			phy_cfg->ch_ctrl[i].sel = json_get_arr_u32(jsp, child, i, i);
	}
	phy_cfg->bypass_resample = json_get_obj_u32(jsp, child, "bypass_resample", 1);
	child = json_get_object_child(jsp, parent, "pn_swap");
	if (child) {
		cnt = json_get_array_size(jsp, child);
		cnt = lcd_s32_constraint(cnt, 0, phy_cfg->lane_num);
		for (i = 0; i < cnt; i++)
			phy_cfg->ch_ctrl[i].pn_swap = json_get_arr_u32(jsp, child, i, 0);
	}

	child = json_get_object_child(jsp, parent, "phase_sel");
	if (child) {
		cnt = json_get_array_size(jsp, child);
		cnt = lcd_s32_constraint(cnt, 0, phy_cfg->lane_num);
		for (i = 0; i < cnt; i++)
			phy_cfg->ch_ctrl[i].phase_sel = json_get_arr_u32(jsp, child, i, 0xff);
	}

	parent = json_get_object_child(jsp, parent, "attr");
	cnt = json_get_array_size(jsp, parent);
	if (cnt <= 0) {
		LCDPR("not find phy attr, use dft\n");
		return 0;
	}

	for (i = 0; i < cnt; i++) {
		child = json_get_array_child(jsp, parent, i);
		if (!child) {
			LCDPR("fail to find attr[%d]\n", i);
			return 0;
		}
		if (i != 0) {
			phy = lcd_phy_alloc(pdrv);
			if (!phy) {
				LCDPR("%s phy[%d] alloc fail, ignore it\n", __func__, i);
				return 0;
			}
			memcpy(phy, phy_cfg->phys[0], sizeof(*phy));
		}

		str = json_get_obj_str(jsp, child, "mode", NULL);
		phy->cv_mode   = (str && (strcmp(str, "voltage") == 0)) ? PHY_VMODE : PHY_CMODE;
		phy->phy_clk   = json_get_obj_u32(jsp, child, "phy_clk", 0);
		phy->vcm       = json_get_obj_u32(jsp, child, "vcm", phy->vcm);
		phy->odt       = json_get_obj_u32(jsp, child, "odt", phy->odt);
		phy->ref_bias  = json_get_obj_u32(jsp, child, "bias", phy->ref_bias);
		phy->vswing    = json_get_obj_u32(jsp, child, "vswing", phy->vswing);
		phy->clk_phase = json_get_obj_u32(jsp, child, "clk_phase", phy->clk_phase);

		child2 = json_get_object_child(jsp, child, "ssc");
		if (child2) {
			ss = &phy->ss;
			ss->level = json_get_obj_u32(jsp, child2, "level", ss->level);
			ss->freq  = json_get_obj_u32(jsp, child2, "freq", ss->freq);
			ss->mode  = json_get_obj_u32(jsp, child2, "mode", ss->mode);
		}

		child2 = json_get_object_child(jsp, child, "ch_preem");
		if (child2) {
			cnt2 = json_get_array_size(jsp, child2);
			cnt2 = lcd_s32_constraint(cnt2, 0, phy_cfg->lane_num);
			for (k = 0; k < cnt2; k++)
				phy->lane[k].preem = json_get_arr_u32(jsp, child2, k,
								     phy->lane[k].preem);
		}

		child2 = json_get_object_child(jsp, child, "ch_amp");
		if (child2) {
			cnt2 = json_get_array_size(jsp, child2);
			cnt2 = lcd_s32_constraint(cnt2, 0, phy_cfg->lane_num);
			for (k = 0; k < cnt2; k++)
				phy->lane[k].amp = json_get_arr_u32(jsp, child2, k,
								     phy->lane[k].amp);
		}
	}

	return 0;
}

static int lcd_panel_parse_interface(struct json_parse_s *jsp, struct aml_lcd_drv_s *pdrv)
{
	struct json_s *parent;
	struct lvds_config_s   *lvds;
	struct vbyone_config_s *vx1;
	struct dsi_config_s    *mipi;
	struct mlvds_config_s  *mlvds;
	struct p2p_config_s    *p2p;
	union lcd_ctrl_config_u *cfg;
	int type, lcd_bits = pdrv->config.timing.base_timing->lcd_bits;
	const char *str;
	unsigned int *nums = NULL, nums_size = 0;
	int cnt = 0, cnt_max, i = 0;

	parent = json_get_object_child(jsp, jsp->root, "interface");
	if (!parent) {
		LCDERR("find /interface\n");
		return -1;
	}

	cfg = &pdrv->config.control;
	type = pdrv->config.basic.lcd_type;
	switch (type) {
	case LCD_LVDS:
		lvds = &cfg->lvds_cfg;
		str = json_get_obj_str(jsp, parent, "lvds_fmt", NULL);
		lvds->lvds_repack  = (str && strcmp(str, "VESA") == 0) ? 1 : 0;
		if (lvds->lvds_repack)
			lvds->lvds_repack = (lcd_bits == 30) ? 2 : (lcd_bits == 18) ? 0 : 1;
		lvds->dual_port    = json_get_obj_u32(jsp, parent, "dual_port", 1);
		lvds->pn_swap      = json_get_obj_u32(jsp, parent, "pn_swap", 0);
		break;
	case LCD_VBYONE:
		vx1 = &cfg->vbyone_cfg;
		vx1->lane_count  = json_get_obj_u32(jsp, parent, "lane_num", 8);
		vx1->region_num  = json_get_obj_u32(jsp, parent, "region", 2);
		vx1->color_fmt   = 4;
		vx1->byte_mode   = (lcd_bits + 7) >> 3;
		vx1->vsync_intr_en  = json_get_obj_u32(jsp, parent, "vsync_isr", 3);
		vx1->intr_en        = json_get_obj_u32(jsp, parent, "vx1_isr", 1);
		vx1->hw_filter_time = json_get_obj_u32(jsp, parent, "filter_time", 0);
		vx1->hw_filter_cnt  = json_get_obj_u32(jsp, parent, "filter_cnt", 0);
		break;
	case LCD_P2P:
		p2p = &cfg->p2p_cfg;
		p2p->lane_num = json_get_obj_u32(jsp, parent, "lane_num", 0);
		str = json_get_obj_str(jsp, parent, "protocol", "Invalid");
		p2p->p2p_type = strnum_get_num(str, p2p_type_name, ARRAY_SIZE(p2p_type_name),
					       P2P_MAX);
		break;
	case LCD_MLVDS:
		mlvds = &cfg->mlvds_cfg;
		mlvds->channel_num  = json_get_obj_u32(jsp, parent, "lane_num", 0);
		break;
	case LCD_MIPI:
		mipi = &cfg->mipi_cfg;
		mipi->lane_num = json_get_obj_u32(jsp, parent, "data_lane", 0);
		mipi->bit_rate_max = json_get_obj_u32(jsp, parent, "bit_rate_max", 0);
		mipi->operation_mode_init =
				json_get_obj_u32(jsp, parent, "operation_mode_init", 0);
		mipi->operation_mode_display =
				json_get_obj_u32(jsp, parent, "operation_mode_display", 0);
		mipi->video_mode_type = json_get_obj_u32(jsp, parent, "video_mode", 0);
		mipi->clk_always_hs = json_get_obj_u32(jsp, parent, "clk_always_HS", 0);
		mipi->check_en = 0;
		mipi->check_reg = 0xff;
		mipi->check_cnt = 0;
		kfree(mipi->dsi_init_on);
		kfree(mipi->dsi_init_off);
		mipi->dsi_init_on = NULL;
		mipi->dsi_init_off = NULL;

		str = json_get_obj_str(jsp, parent, "init_on", NULL);
		if (!str) {
			LCDERR("not find mipi init_on\n");
			return -1;
		}
		cnt_max = lcd_get_str_array_cnt(str);
		if (cnt_max <= 0) {
			LCDERR("mipi init_on error\n");
			return -1;
		}
		nums_size = cnt_max * sizeof(unsigned int);
		nums = kzalloc(nums_size, GFP_KERNEL);
		if (!nums)
			return -1;

		cnt = lcd_trans_str_array(str, nums, cnt_max);
		if (cnt <= 0) {
			LCDERR("mipi init_on error\n");
			kfree(nums);
			return -1;
		}
		mipi->dsi_init_on = kcalloc(cnt, sizeof(unsigned char), GFP_KERNEL);
		if (!mipi->dsi_init_on) {
			LCDERR("no memory to save init_on data\n");
			kfree(nums);
			return -1;
		}
		for (i = 0; i < cnt; i++)
			mipi->dsi_init_on[i] = nums[i];

		kfree(nums);
		nums = NULL;

		str = json_get_obj_str(jsp, parent, "init_off", NULL);
		if (!str) {
			LCDERR("not find mipi init_off\n");
			kfree(mipi->dsi_init_on);
			mipi->dsi_init_on = NULL;
			return -1;
		}
		cnt_max = lcd_get_str_array_cnt(str);
		if (cnt_max <= 0) {
			LCDERR("mipi init_on error\n");
			return -1;
		}
		nums_size = cnt_max * sizeof(unsigned int);
		nums = kzalloc(nums_size, GFP_KERNEL);
		if (!nums)
			return -1;

		cnt = lcd_trans_str_array(str, nums, cnt_max);
		if (cnt <= 0) {
			LCDERR("mipi init_off error\n");
			kfree(nums);
			return -1;
		}
		mipi->dsi_init_off = kcalloc(cnt, sizeof(unsigned char), GFP_KERNEL);
		if (!mipi->dsi_init_off) {
			LCDERR("no memory to save init_off data\n");
			kfree(nums);
			return -1;
		}
		for (i = 0; i < cnt; i++)
			mipi->dsi_init_off[i] = nums[i];

		kfree(nums);
		nums = NULL;

		break;
	default:
		LCDERR("can't match valid interface\n");
		return -1;
	}

	return 0;
}

struct num_str_s power_type[] = {
	{LCD_POWER_TYPE_GPIO,               "gpio"},
	{LCD_POWER_TYPE_PMU,                "pmu"},
	{LCD_POWER_TYPE_SIGNAL,             "interface"},
	{LCD_POWER_TYPE_EXTERN,             "extern"},
	{LCD_POWER_TYPE_WAIT_GPIO,          "wait_gpio"},
	{LCD_POWER_TYPE_TCON_SPI_DATA_LOAD, "tcon_spi"},
	{LCD_POWER_TYPE_BACKLIGHT,          "backlight"},
	{LCD_POWER_TYPE_MUTE,               "mute"}
};

static int lcd_gpio_name_to_index(struct aml_lcd_drv_s *pdrv, const char *name)
{
	int i = 0;

	if (!name)
		return LCD_CPU_GPIO_NUM_MAX;

	for (i = 0; i < LCD_CPU_GPIO_NUM_MAX; i++)
		if (!strcmp(pdrv->config.power.cpu_gpio[i].name, name))
			return i;

	return LCD_CPU_GPIO_NUM_MAX;
}

static int lcd_panel_parse_power(struct json_parse_s *jsp, struct aml_lcd_drv_s *pdrv)
{
	struct json_s *parent, *child;
	int cnt = 1, i = 0;
	struct lcd_power_ctrl_s *cfg = &pdrv->config.power;
	struct lcd_power_step_s *step;
	const char *str;

	parent = json_path_to_node(jsp, jsp->root, "/power_sequence/on");
	cnt = json_get_array_size(jsp, parent);
	if (cnt <= 0) {
		LCDERR("invalid /power_sequence/on\n");
		return -1;
	}

	cnt = lcd_s32_constraint(cnt, 0, LCD_PWR_STEP_MAX - 1);
	step = cfg->power_on_step;
	for (i = 0; i < cnt; i++) {
		child = json_get_array_child(jsp, parent, i);
		if (!child)
			return -1;

		step[i].delay = json_get_arr_u32(jsp, child, 3, 0);
		step[i].value = json_get_arr_u32(jsp, child, 2, 0);
		str          = json_get_arr_str(jsp, child, 0, NULL);
		step[i].type = strnum_get_num(str, power_type, ARRAY_SIZE(power_type),
					      LCD_POWER_TYPE_MAX);
		if (step[i].type >= LCD_POWER_TYPE_MAX) {
			i++;
			break;
		}

		switch (step[i].type) {
		case LCD_POWER_TYPE_GPIO:
		case LCD_POWER_TYPE_WAIT_GPIO:
			str = json_get_arr_str(jsp, child, 1, NULL);
			step[i].index = lcd_gpio_name_to_index(pdrv, str);
			if (step[i].index < LCD_CPU_GPIO_NUM_MAX)
				lcd_cpu_gpio_probe(pdrv, step[i].index);
			break;
#ifdef CONFIG_AMLOGIC_LCD_EXTERN
		case LCD_POWER_TYPE_EXTERN:
			step[i].index = 0xff;
			str = json_get_arr_str(jsp, child, 1, NULL);
			if (str && !strncmp(str, "lcd_ext_dev", 11))
				if (kstrtou32(str + 11, 10, &step[i].index))
					step[i].index = 0xff;

			if (step[i].index < 255) {
				lcd_resource_add(pdrv, LCD_RES_EXTERN, step[i].index);
				lcd_extern_dev_index_add(pdrv->index, step[i].index);
			}
			break;
#endif
		case LCD_POWER_TYPE_MUTE:
			pdrv->unmute_cnt = step[i].value;
			break;
		default:
			break;
		}
	}
	if (step[i - 1].type != 0xff) {
		step[i].type = 0xff;
		i++;
	}
	cfg->power_on_step_max = i;
	if (lcd_debug_print_flag) {
		LCDPR("init on:\n");
		step = cfg->power_on_step;
		for (i = 0; i < cfg->power_on_step_max; i++) {
			LCDPR("step[%d]: type=%d, index=%d, value=%d, delay=%d\n",
				i, step[i].type, step[i].index, step[i].value, step[i].delay);
		}
	}

	parent = json_path_to_node(jsp, jsp->root, "/power_sequence/off");
	cnt = json_get_array_size(jsp, parent);
	if (cnt <= 0) {
		LCDERR("/power_sequence/off\n");
		return -1;
	}

	cnt = lcd_s32_constraint(cnt, 0, LCD_PWR_STEP_MAX - 1);
	step = cfg->power_off_step;
	for (i = 0; i < cnt; i++) {
		child = json_get_array_child(jsp, parent, i);
		if (!child)
			return -1;
		step[i].delay = json_get_arr_u32(jsp, child, 3, 0);
		step[i].value = json_get_arr_u32(jsp, child, 2, 0);
		str	     = json_get_arr_str(jsp, child, 0, NULL);
		step[i].type = strnum_get_num(str, power_type, ARRAY_SIZE(power_type),
					      LCD_POWER_TYPE_MAX);
		if (step[i].type >= LCD_POWER_TYPE_MAX) {
			i++;
			break;
		}

		switch (step[i].type) {
		case LCD_POWER_TYPE_GPIO:
		case LCD_POWER_TYPE_WAIT_GPIO:
			str = json_get_arr_str(jsp, child, 1, NULL);
			step[i].index = lcd_gpio_name_to_index(pdrv, str);
			if (step[i].index < LCD_CPU_GPIO_NUM_MAX)
				lcd_cpu_gpio_probe(pdrv, step[i].index);
			break;
#ifdef CONFIG_AMLOGIC_LCD_EXTERN
		case LCD_POWER_TYPE_EXTERN:
			step[i].index = 0xff;
			str = json_get_arr_str(jsp, child, 1, NULL);
			if (str && !strncmp(str, "lcd_ext_dev", 11))
				if (kstrtou32(str + 11, 10, &step[i].index))
					step[i].index = 0xff;

			if (step[i].index < 255) {
				lcd_resource_add(pdrv, LCD_RES_EXTERN, step[i].index);
				lcd_extern_dev_index_add(pdrv->index, step[i].index);
			}
			break;
#endif
		case LCD_POWER_TYPE_MUTE:
			pdrv->mute_cnt = step[i].value;
			break;
		default:
			break;
		}
	}
	if (step[i - 1].type != 0xff) {
		step[i].type = 0xff;
		i++;
	}
	cfg->power_off_step_max = i;
	if (lcd_debug_print_flag) {
		LCDPR("init off:\n");
		step = cfg->power_off_step;
		for (i = 0; i < cnt; i++) {
			LCDPR("step[%d]: type=%d, index=%d, value=%d, delay=%d\n",
				i, step[i].type, step[i].index, step[i].value, step[i].delay);
		}
	}

	return 0;
}

static int lcd_panel_parse_vlock(struct json_parse_s *jsp, struct aml_lcd_drv_s *pdrv)
{
	struct json_s *parent;
	unsigned int *param = pdrv->config.vlock_param;

	param[0] = LCD_VLOCK_PARAM_BIT_UPDATE;
	parent = json_get_object_child(jsp, jsp->root, "vlock");
	if (!parent)
		return -1;

	param[0] |= LCD_VLOCK_PARAM_BIT_VALID;
	param[1] = json_get_obj_u32(jsp, parent, "en", 0);
	param[2] = json_get_obj_u32(jsp, parent, "mode", 0);
	param[3] = json_get_obj_u32(jsp, parent, "pll_m_limit", 0);
	param[4] = json_get_obj_u32(jsp, parent, "line_limit", 0);

	if (lcd_debug_print_flag)
		LCDPR("vlock: en:%d, mode:%d, pll_m_limit:%d, line_limit:%d\n",
			param[1], param[2], param[3], param[4]);

	return 0;
}

static int lcd_panel_parse_optical(struct json_parse_s *jsp, struct aml_lcd_drv_s *pdrv)
{
	struct json_s *parent, *child;
	struct lcd_optical_info_s *opt = &pdrv->config.optical;

	parent = json_get_object_child(jsp, jsp->root, "optical");
	if (!parent)
		return -1;

	child = json_get_object_child(jsp, parent, "primaries");

	opt->features = json_get_obj_u32(jsp, parent, "feature", 0);
	opt->hdr_support = json_get_obj_u32(jsp, parent, "hdr_support", 0);
	opt->ldim_support = json_get_obj_u32(jsp, parent, "ldim_support", 0);
	opt->white_point_x = json_get_obj_u32(jsp, parent, "white_point_x", 1024);
	opt->white_point_y = json_get_obj_u32(jsp, parent, "white_point_y", 1024);
	opt->luma_min = json_get_obj_u32(jsp, parent, "luma_min", 16);
	opt->luma_max = json_get_obj_u32(jsp, parent, "luma_max", 235);
	opt->luma_avg = json_get_obj_u32(jsp, parent, "luma_avg", 128);
	opt->luma_peak = json_get_obj_u32(jsp, parent, "luma_peak", 235);

	if (child) {
		opt->primaries_b_x = json_get_arr_u32(jsp, child, 0, 0);
		opt->primaries_b_y = json_get_arr_u32(jsp, child, 1, 0);
		opt->primaries_g_x = json_get_arr_u32(jsp, child, 2, 0);
		opt->primaries_g_y = json_get_arr_u32(jsp, child, 3, 0);
		opt->primaries_r_x = json_get_arr_u32(jsp, child, 4, 0);
		opt->primaries_r_y = json_get_arr_u32(jsp, child, 5, 0);
	}
	lcd_optical_vinfo_update(pdrv);

	return 0;
}

static int lcd_panel_parse_swpdf(struct json_parse_s *jsp, struct aml_lcd_drv_s *pdrv)
{
	s32 pat_cnt, blk_cnt = 0, act_cnt = 0, i = 0, k = 0;
	u32 x, y, w, h, reg, mask, val, bus, mat[4], th[2];
	struct swpdf_s *pdf = get_swpdf();
	struct swpdf_pat_s *pat = NULL;
	struct json_s *parent, *thres_node, *pat_node;
	struct json_s *blk_parent, *blk_node, *act_parent, *act_node;

	parent = json_get_object_child(jsp, jsp->root, "sw_pdf");
	if (!parent || !json_get_obj_u32(jsp, parent, "en", 0)) {
		LCDPR("no sw_pdf find\n");
		return 0;
	}

	parent = json_get_object_child(jsp, parent, "pattern");
	pat_cnt = json_get_array_size(jsp, parent);
	if (!parent || pat_cnt <= 0) {
		LCDPR("error pattern\n");
		return 0;
	}

	lcd_swpdf_init(pdrv);
	for (i = 0; i < pat_cnt; i++) {
		pat_node = json_get_array_child(jsp, parent, 0);
		if (!pat_node)
			continue;

		thres_node = json_get_object_child(jsp, parent, "threshold");
		if (thres_node)
			continue;

		blk_parent = json_get_object_child(jsp, parent, "block");
		blk_cnt = json_get_array_size(jsp, blk_parent);
		if (!blk_parent || blk_cnt <= 0)
			continue;

		act_parent = json_get_object_child(jsp, parent, "act");
		act_cnt = json_get_array_size(jsp, act_parent);
		if (!act_parent || act_cnt <= 0)
			continue;

		th[0] = json_get_arr_u32(jsp, thres_node, 0, 4096);
		th[1] = json_get_arr_u32(jsp, thres_node, 0, 0);
		pat = swpdf_pat_create_add(pdf, th[0], th[1]);
		if (!pat) {
			LCDPR("swpdf pat_%d fail: blk_cnt:%d, act_cnt:%d thres: %d-%d",
				i, blk_cnt, act_cnt, th[0], th[1]);
			continue;
		}
		for (k = 0; k < blk_cnt; k++) {
			blk_node = json_get_array_child(jsp, blk_parent, k);
			if (!blk_node)
				continue;

			x = json_get_arr_u32(jsp, blk_node, 0, 0);
			y = json_get_arr_u32(jsp, blk_node, 1, 0);
			w = json_get_arr_u32(jsp, blk_node, 2, 0);
			h = json_get_arr_u32(jsp, blk_node, 3, 0);
			mat[0] = json_get_arr_u32(jsp, blk_node, 4, 0);
			mat[1] = json_get_arr_u32(jsp, blk_node, 5, 0);
			mat[2] = json_get_arr_u32(jsp, blk_node, 6, 0);
			mat[3] = json_get_arr_u32(jsp, blk_node, 7, 0);

			if (!swpdf_block_create_add(pat, x, y, w, h, mat))
				LCDPR("blk:[%d, %d, %d, %d] mat:[%08x, %08x, %08x, %08x]\n",
					x, y, w, h, mat[0], mat[1], mat[2], mat[3]);
		}

		for (k = 0; k < act_cnt; k++) {
			act_node = json_get_array_child(jsp, act_parent, k);
			if (!act_node)
				continue;

			reg = json_get_arr_u32(jsp, act_node, 0, 0);
			mask = json_get_arr_u32(jsp, act_node, 1, 0);
			val = json_get_arr_u32(jsp, act_node, 2, 0);
			bus = json_get_arr_u32(jsp, act_node, 3, 0);

			if (!swpdf_act_create_add(pat, reg, mask, val, bus))
				LCDPR("add act: reg=0x%x, mask=0x%x, val=0x%x\n",
					reg, mask, val);
		}
		if (pdrv->status & LCD_STATUS_IF_ON)
			swpdf_act_pat(pat, SWPDF_ACT_RD_DFT);
		pdrv->config.customer_sw_pdf |= 1 << i;
	}

	return 0;
}

static int lcd_panel_parse_swpol(struct json_parse_s *jsp, struct aml_lcd_drv_s *pdrv)
{
	return 0;
}

int lcd_config_load_from_json(struct aml_lcd_drv_s *pdrv, unsigned char *panel_file)
{
	int ret = -1;
	struct json_parse_s *jsp;

	jsp = get_panel_jsp(pdrv->index);
	if (!json_parse_ok(jsp)) {
		LCDERR("panel%d json not ready\n", pdrv->index);
		return -1;
	}

	/*parse basic*/
	ret = lcd_panel_parse_basic(jsp, pdrv);
	if (ret < 0)
		goto parse_end;

	/*parse timing*/
	ret = lcd_panel_parse_timing(jsp, pdrv);
	if (ret < 0)
		goto parse_end;

	/*parse interface*/
	ret = lcd_panel_parse_interface(jsp, pdrv);
	if (ret < 0)
		goto parse_end;

	/*parse phy*/
	ret = lcd_panel_parse_phy(jsp, pdrv);
	if (ret < 0)
		goto parse_end;

	/*parse power sequence*/
	ret = lcd_panel_parse_power(jsp, pdrv);
	if (ret < 0)
		goto parse_end;

	/*parse vlock*/
	lcd_panel_parse_vlock(jsp, pdrv);

	if (pdrv->config.basic.lcd_type == LCD_MLVDS ||
	    pdrv->config.basic.lcd_type == LCD_P2P) {
		/*parse sw_pdf*/
		lcd_panel_parse_swpdf(jsp, pdrv);

		/*parse sw_pol,   todo*/
		lcd_panel_parse_swpol(jsp, pdrv);
	}

	/*parse hdr*/
	lcd_panel_parse_optical(jsp, pdrv);

	//lcd_panel_parse_data(jsp, pdrv);

#ifdef CONFIG_AMLOGIC_BACKLIGHT
	lcd_resource_add(pdrv, LCD_RES_BACKLIGHT, 0);
	aml_bl_index_add(pdrv->index, 0);
#endif

parse_end:
	return ret;
}

static int lcd_power_load_from_ini(struct aml_lcd_drv_s *pdrv, void *inip, void *psec)
{
	struct lcd_power_step_s *pstep;
	int on_cnt = 0, off_cnt = 0, tmp_cnt, tmp_buf_size, trans_cnt;
	int power_step = 0, append_more = 1;
	unsigned int *tmp_buf;
	int i, j, temp;

	on_cnt = lcd_ini_get_array_cnt(inip, psec, "power_on_step");
	off_cnt = lcd_ini_get_array_cnt(inip, psec, "power_off_step");
	tmp_cnt = (on_cnt >= off_cnt ? on_cnt : off_cnt);
	if (tmp_cnt <= 0) {
		LCDERR("[%d]: %s: get power step failed\n", pdrv->index, __func__);
		return -1;
	}

	tmp_buf_size = tmp_cnt * sizeof(unsigned int);
	tmp_buf = kzalloc(tmp_buf_size, GFP_KERNEL);
	if (!tmp_buf)
		return -1;

	if (on_cnt > 0) {
		pstep = pdrv->config.power.power_on_step;
		trans_cnt = lcd_ini_get_array(inip, psec, "power_on_step", tmp_buf, on_cnt);
		power_step = trans_cnt / 4;
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("[%d]: power_on step: %d\n", pdrv->index, power_step);
		for (i = 0; i < power_step; i++) {
			j = i * 4;
			pstep[i].type = tmp_buf[j + 0];
			pstep[i].index = tmp_buf[j + 1];
			pstep[i].value = tmp_buf[j + 2];
			pstep[i].delay = tmp_buf[j + 3];
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("step[%d]: type=%d, index=%d, value=%d, delay=%d\n",
					i, pstep[i].type, pstep[i].index,
					pstep[i].value, pstep[i].delay);
			}

			if (pstep[i].type >= LCD_POWER_TYPE_MAX) {
				i++;
				break;
			}

			switch (pstep[i].type) {
			case LCD_POWER_TYPE_GPIO:
			case LCD_POWER_TYPE_WAIT_GPIO:
				if (pstep[i].index < LCD_CPU_GPIO_NUM_MAX)
					lcd_cpu_gpio_probe(pdrv, pstep[i].index);
				break;
			case LCD_POWER_TYPE_EXTERN:
#ifdef CONFIG_AMLOGIC_LCD_EXTERN
				lcd_resource_add(pdrv, LCD_RES_EXTERN, pstep[i].index);
				lcd_extern_dev_index_add(pdrv->index, pstep[i].index);
#endif
				break;
			case LCD_POWER_TYPE_CLK_SS:
				temp = pstep[i].value;
				pdrv->config.timing.ss_freq = temp & 0xf;
				pdrv->config.timing.ss_mode = (temp >> 4) & 0xf;
				if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
					LCDPR("[%d]: clk_ss value=0x%x: ss_freq=%d, ss_mode=%d\n",
						pdrv->index, temp,
						pdrv->config.timing.ss_freq,
						pdrv->config.timing.ss_mode);
				}
				break;
			case LCD_POWER_TYPE_BACKLIGHT:
			case LCD_POWER_TYPE_MUTE:
				append_more = 0;
				break;
			default:
				break;
			}
		}
		pdrv->config.power.power_on_step_max = i;

		if (append_more && i + 2 < LCD_PWR_STEP_MAX) {
			i--;
			pstep[i].type = LCD_POWER_TYPE_BACKLIGHT;
			pstep[i].index = 0;
			pstep[i].value = 1; //bl on
			pstep[i].delay = 0;
			i++;

			pstep[i].type = LCD_POWER_TYPE_MUTE;
			pstep[i].index = 0;
			pstep[i].value = 0;//unmute
			pstep[i].delay = pdrv->unmute_cnt;
			i++;
			pstep[i].type = LCD_POWER_TYPE_MAX; //add new ending
			i++;
			pdrv->config.power.power_on_step_max = i;
		}
	}

	if (off_cnt > 0) {
		append_more = 1;
		pstep = pdrv->config.power.power_off_step;
		trans_cnt = lcd_ini_get_array(inip, psec, "power_off_step", tmp_buf, off_cnt);
		power_step = trans_cnt / 4;
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("[%d]: power_off step: %d\n", pdrv->index, power_step);
		for (i = 0; i < power_step; i++) {
			j = i * 4;
			pstep[i].type = tmp_buf[j + 0];
			pstep[i].index = tmp_buf[j + 1];
			pstep[i].value = tmp_buf[j + 2];
			pstep[i].delay = tmp_buf[j + 3];

			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("step[%d]: type=%d, index=%d, value=%d, delay=%d\n",
					i, pstep[i].type, pstep[i].index,
					pstep[i].value, pstep[i].delay);
			}
			if (pstep[i].type >= LCD_POWER_TYPE_MAX) {
				i++;
				break;
			}

			switch (pstep[i].type) {
			case LCD_POWER_TYPE_GPIO:
			case LCD_POWER_TYPE_WAIT_GPIO:
				if (pstep[i].index < LCD_CPU_GPIO_NUM_MAX)
					lcd_cpu_gpio_probe(pdrv, pstep[i].index);
				break;
			case LCD_POWER_TYPE_EXTERN:
#ifdef CONFIG_AMLOGIC_LCD_EXTERN
				lcd_resource_add(pdrv, LCD_RES_EXTERN, pstep[i].index);
				lcd_extern_dev_index_add(pdrv->index, pstep[i].index);
#endif
				break;
			case LCD_POWER_TYPE_BACKLIGHT:
			case LCD_POWER_TYPE_MUTE:
				append_more = 0;
				break;
			default:
				break;
			}
		}
		pdrv->config.power.power_off_step_max = i;

		if (append_more && i + 2 < LCD_POWER_TYPE_MAX) {
			i--;
			for (j = i + 2; j >= 2; j--)
				memcpy(&pstep[j], &pstep[j - 2], sizeof(struct lcd_power_step_s));
			pdrv->config.power.power_off_step_max += 2;

			pstep[0].type  = LCD_POWER_TYPE_MUTE;
			pstep[0].index = 0;
			pstep[0].value = 1; //mute
			pstep[0].delay = pdrv->mute_cnt;

			pstep[1].type = LCD_POWER_TYPE_BACKLIGHT;
			pstep[1].index = 0;
			pstep[1].value = 0;//bl off
			pstep[1].delay = 0;
		}
	}

	kfree(tmp_buf);

	return 0;
}

static int lcd_vlock_param_load_from_ini(struct aml_lcd_drv_s *pdrv, void *inip, void *psec)
{
	pdrv->config.vlock_param[0] = LCD_VLOCK_PARAM_BIT_UPDATE;
	pdrv->config.vlock_param[1] = lcd_ini_get_val(inip, psec, "vlock_val_0", 0);
	pdrv->config.vlock_param[2] = lcd_ini_get_val(inip, psec, "vlock_val_1", 0);
	pdrv->config.vlock_param[3] = lcd_ini_get_val(inip, psec, "vlock_val_2", 0);
	pdrv->config.vlock_param[4] = lcd_ini_get_val(inip, psec, "vlock_val_3", 0);
	if (pdrv->config.vlock_param[1] ||
	    pdrv->config.vlock_param[2] ||
	    pdrv->config.vlock_param[3] ||
	    pdrv->config.vlock_param[4]) {
		LCDPR("[%d]: find vlock_attr\n", pdrv->index);
		pdrv->config.vlock_param[0] |= LCD_VLOCK_PARAM_BIT_VALID;
	}

	return 0;
}

static int lcd_optical_load_from_ini(struct aml_lcd_drv_s *pdrv, void *inip)
{
	struct lcd_optical_info_s *opt_info = &pdrv->config.optical;
	void *psec;

	psec = lcd_ini_get_section(inip, "lcd_optical_Attr");
	if (!psec)
		return 0;

	opt_info->hdr_support = lcd_ini_get_val(inip, psec, "hdr_support", 0);
	opt_info->features = lcd_ini_get_val(inip, psec, "features", 0);
	opt_info->primaries_r_x = lcd_ini_get_val(inip, psec, "primaries_r_x", 0);
	opt_info->primaries_r_y = lcd_ini_get_val(inip, psec, "primaries_r_y", 0);
	opt_info->primaries_g_x = lcd_ini_get_val(inip, psec, "primaries_g_x", 0);
	opt_info->primaries_g_y = lcd_ini_get_val(inip, psec, "primaries_g_y", 0);
	opt_info->primaries_b_x = lcd_ini_get_val(inip, psec, "primaries_b_x", 0);
	opt_info->primaries_b_y = lcd_ini_get_val(inip, psec, "primaries_b_y", 0);
	opt_info->white_point_x = lcd_ini_get_val(inip, psec, "white_point_x", 0);
	opt_info->white_point_y = lcd_ini_get_val(inip, psec, "white_point_y", 0);
	opt_info->luma_max = lcd_ini_get_val(inip, psec, "luma_max", 0);
	opt_info->luma_min = lcd_ini_get_val(inip, psec, "luma_min", 0);
	opt_info->luma_avg = lcd_ini_get_val(inip, psec, "luma_avg", 0);

	opt_info->ldim_support = lcd_ini_get_val(inip, psec, "ldim_support", 0);
	opt_info->luma_peak = lcd_ini_get_val(inip, psec, "luma_peak", 0);

	lcd_optical_vinfo_update(pdrv);

	return 0;
}

static int lcd_config_load_from_ini_v2(struct aml_lcd_drv_s *pdrv, void *inip, void *psec,
				       unsigned char version)
{
	struct phy_config_s *phy_cfg = &pdrv->config.phy_cfg;
	struct phy_attr_s *phy;
	unsigned int *tmp_buf;
	int tmp_cnt, tmp_buf_size, lane_cnt = 0;
	char pr_buf[48];
	int i, pr_len = 0, ret;

	/*phy*/
	phy = phy_cfg->phys[0];
	if (!phy)
		return -1;

	phy_cfg->flag = lcd_ini_get_val(inip, psec, "phy_attr_flag", 0);
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("%s: ctrl_flag=0x%x\n", __func__, phy_cfg->flag);

	if (phy_cfg->flag & PHY_BIT_VSWING)
		phy->vswing = lcd_ini_get_val(inip, psec, "phy_attr_0", phy->vswing);
	if (phy_cfg->flag & PHY_BIT_VCM)
		phy->vcm = lcd_ini_get_val(inip, psec, "phy_attr_1", phy->vcm);
	if (phy_cfg->flag & PHY_BIT_REF_BIAS)
		phy->ref_bias = lcd_ini_get_val(inip, psec, "phy_attr_2", phy->ref_bias);
	if (phy_cfg->flag & PHY_BIT_ODT)
		phy->odt = lcd_ini_get_val(inip, psec, "phy_attr_3", phy->odt);
	if (phy_cfg->flag & PHY_BIT_CV_MODE)
		phy->cv_mode = lcd_ini_get_val(inip, psec, "phy_attr_4", phy->cv_mode);
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("[%d]:%s: vswing=0x%x, vcm=0x%x, ref_bias=0x%x, odt=0x%x, cv_mode=%d\n",
		      pdrv->index, __func__, phy->vswing, phy->vcm, phy->ref_bias,
		      phy->odt, phy->cv_mode);
	}

	tmp_cnt = lcd_ini_get_array_cnt(inip, psec, "phy_lane_ctrl");
	if (tmp_cnt > 0) {
		tmp_buf_size = tmp_cnt * sizeof(unsigned int);
		tmp_buf = kzalloc(tmp_buf_size, GFP_KERNEL);
		if (!tmp_buf)
			return -1;
		lane_cnt = lcd_ini_get_array(inip, psec, "phy_lane_ctrl", tmp_buf, tmp_cnt);
		for (i = 0; i < lane_cnt; i++) {
			pr_len = 0;
			if (phy_cfg->flag & PHY_BIT_LANE_PREEM) {
				phy->lane[i].preem = tmp_buf[i] & 0xffff;
				pr_len += sprintf(pr_buf + pr_len, " preem=0x%x",
						phy->lane[i].preem);
			}
			if (phy_cfg->flag & PHY_BIT_LANE_AMP) {
				phy->lane[i].amp = (tmp_buf[i] >> 16) & 0xffff;
				pr_len += sprintf(pr_buf + pr_len, " amp=0x%x",
						phy->lane[i].amp);
			}
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				if (pr_len)
					LCDPR("%s: lane[%d]:%s\n", __func__, i, pr_buf);
			}
		}
		kfree(tmp_buf);
	}

	ret = lcd_cus_ctrl_load_from_ini(pdrv, inip, psec, version);

	return ret;
}

static int lcd_config_load_from_ini_v3(struct aml_lcd_drv_s *pdrv, void *inip, void *psec,
				       unsigned char version)
{
	int ret;

	ret = lcd_cus_ctrl_load_from_ini(pdrv, inip, psec, version);

	return ret;
}

static int lcd_ini_str_to_type(const char *str)
{
	const char *p = str;
	int type = LCD_TYPE_MAX;
	int i;

	if (strncasecmp("LCD_", str, 3) == 0)
		p = str + 4;

	for (i = 0; i < ARRAY_SIZE(lcd_type_match_table); i++) {
		if (!strcasecmp(p, lcd_type_match_table[i].str)) {
			type = lcd_type_match_table[i].num;
			break;
		}
	}
	return type;
}

static int lcd_config_load_from_ini(struct aml_lcd_drv_s *pdrv, unsigned char *panel_file)
{
	struct lcd_config_s *pconf = &pdrv->config;
	struct lcd_detail_timing_s *ptiming;
	union lcd_ctrl_config_u *pctrl = &pdrv->config.control;
	struct phy_config_s *phy_cfg = &pdrv->config.phy_cfg;
	struct phy_attr_s *phy = NULL;
	void *inip, *psec;
	unsigned char version;
	const char *str;
	unsigned int lcd_bits, temp;
	int ret;

	inip = get_lcd_ini_parse_mem(pdrv->index);
	if (!inip) {
		LCDERR("[%d]: %s: parse_mem not ready\n", pdrv->index, __func__);
		return -1;
	}

	psec = lcd_ini_get_section(inip, "lcd_Attr");
	if (!psec) {
		LCDERR("[%d]: %s: not find lcd_Attr\n", pdrv->index, __func__);
		return -1;
	}
	version = lcd_ini_get_val(inip, psec, "version", 0);

	/*basic*/
	str = lcd_ini_get_str(inip, psec, "model_name", "null");
	strscpy(pconf->basic.model_name, str, MOD_LEN_MAX);

	str = lcd_ini_get_str(inip, psec, "interface", "null");
	pconf->basic.lcd_type = lcd_ini_str_to_type(str);

	temp = lcd_ini_get_val(inip, psec, "config_check", 0);
	pconf->basic.config_check = temp ? 0x3 : 0x2;
	lcd_bits = lcd_ini_get_val(inip, psec, "lcd_bits", 10);
	pconf->basic.screen_width = lcd_ini_get_val(inip, psec, "screen_width", 16);
	pconf->basic.screen_height = lcd_ini_get_val(inip, psec, "screen_height", 9);

	ptiming = lcd_timing_alloc(pdrv);
	if (!ptiming) {
		LCDERR("[%d]: %s: malloc timing error\n", pdrv->index, __func__);
		return -1;
	}
	memset(ptiming, 0, sizeof(*ptiming));

	/* timing: */
	ptiming->h_active = lcd_ini_get_val(inip, psec, "h_active", 0);
	ptiming->v_active = lcd_ini_get_val(inip, psec, "v_active", 0);
	ptiming->h_period = lcd_ini_get_val(inip, psec, "h_period", 0);
	ptiming->v_period = lcd_ini_get_val(inip, psec, "v_period", 0);
	ptiming->hsync_width = lcd_ini_get_val(inip, psec, "hsync_width", 0);
	ptiming->hsync_bp = lcd_ini_get_val(inip, psec, "hsync_bp", 0);
	ptiming->hsync_pol = lcd_ini_get_val(inip, psec, "hsync_pol", 0);
	ptiming->hsync_fp = ptiming->h_period - ptiming->h_active -
			ptiming->hsync_width - ptiming->hsync_bp;
	ptiming->vsync_width = lcd_ini_get_val(inip, psec, "vsync_width", 0);
	ptiming->vsync_bp = lcd_ini_get_val(inip, psec, "vsync_bp", 0);
	ptiming->vsync_pol = lcd_ini_get_val(inip, psec, "vsync_pol", 0);
	ptiming->vsync_fp = ptiming->v_period - ptiming->v_active -
			ptiming->vsync_width - ptiming->vsync_bp;
	pconf->timing.pre_de_h = lcd_ini_get_val(inip, psec, "pre_de_h", 0);
	pconf->timing.pre_de_v = lcd_ini_get_val(inip, psec, "pre_de_v", 0);

	/* customer: 31byte */
	ptiming->fr_adjust_type = lcd_ini_get_val(inip, psec, "fr_adjust_type", 0);
	pconf->timing.ss_level = lcd_ini_get_val(inip, psec, "ss_level", 0);
	pconf->timing.clk_mode = lcd_ini_get_val(inip, psec, "clk_mode", 0);
	pconf->timing.pll_flag = lcd_ini_get_val(inip, psec, "clk_auto_gen", 1);
	ptiming->pixel_clk = lcd_ini_get_val(inip, psec, "pixel_clk", 0);
	ptiming->h_period_min = lcd_ini_get_val(inip, psec, "h_period_min", 0);
	ptiming->h_period_max = lcd_ini_get_val(inip, psec, "h_period_max", 0);
	ptiming->v_period_min = lcd_ini_get_val(inip, psec, "v_period_min", 0);
	ptiming->v_period_max = lcd_ini_get_val(inip, psec, "v_period_max", 0);
	ptiming->pclk_min = lcd_ini_get_val(inip, psec, "pixel_clk_min", 0);
	ptiming->pclk_max = lcd_ini_get_val(inip, psec, "pixel_clk_max", 0);
	ptiming->frame_rate_min = lcd_ini_get_val(inip, psec, "frame_rate_min", 0);
	ptiming->frame_rate_max = lcd_ini_get_val(inip, psec, "frame_rate_max", 0);

	pconf->timing.ppc = lcd_ini_get_val(inip, psec, "ppc_mode", 1);
	pconf->custom_pinmux = lcd_ini_get_val(inip, psec, "custom_pinmux", 0);

	pconf->fr_auto_cus = lcd_ini_get_val(inip, psec, "fr_auto_custom", 0);
	ptiming->switch_type = LCD_VMODE_SWITCH_NONE;
	ptiming->lcd_bits = lcd_bits * 3;
	ptiming->ss_force = 0;
	ptiming->ss_freq = 255;
	ptiming->ss_level = pconf->timing.ss_level;
	ptiming->ss_mode = 255;

	pdrv->config.timing.dft_timing = pdrv->config.timing.timings[0];
	lcd_clk_frame_rate_init(ptiming);
	lcd_config_timing_check(pdrv, ptiming);
	lcd_default_to_basic_timing_init_config(pdrv);

	/* interface: 20byte */
	switch (pconf->basic.lcd_type) {
	case LCD_LVDS:
		str = lcd_ini_get_str(inip, psec, "lvds_fmt", NULL);
		if (!str) {
			pctrl->lvds_cfg.lvds_repack = lcd_ini_get_val(inip, psec, "if_attr_0", 0);
		} else {
			if (strcmp(str, "VESA") == 0) {
				if (ptiming->lcd_bits == 18)
					pctrl->lvds_cfg.lvds_repack = 0;
				else if (ptiming->lcd_bits == 30)
					pctrl->lvds_cfg.lvds_repack = 2;
				else
					pctrl->lvds_cfg.lvds_repack = 1;
			} else { //JEIDA
				pctrl->lvds_cfg.lvds_repack = 0;
			}
		}
		pctrl->lvds_cfg.dual_port = lcd_ini_get_val(inip, psec, "if_attr_1", 0);
		pctrl->lvds_cfg.pn_swap = lcd_ini_get_val(inip, psec, "if_attr_2", 0);
		pctrl->lvds_cfg.port_swap = lcd_ini_get_val(inip, psec, "if_attr_3", 0);
		pctrl->lvds_cfg.phy_vswing = lcd_ini_get_val(inip, psec, "if_attr_4", 0);
		pctrl->lvds_cfg.phy_preem = lcd_ini_get_val(inip, psec, "if_attr_5", 0);
		pctrl->lvds_cfg.lane_reverse = lcd_ini_get_val(inip, psec, "if_attr_8", 0);

		phy_cfg->vswing_level = pctrl->lvds_cfg.phy_vswing & 0xf;
		phy_cfg->ext_pullup = (pctrl->lvds_cfg.phy_vswing >> 4) & 0x3;
		phy_cfg->preem_level = pctrl->lvds_cfg.phy_preem;
		break;
	case LCD_VBYONE:
		pctrl->vbyone_cfg.lane_count = lcd_ini_get_val(inip, psec, "if_attr_0", 0);
		pctrl->vbyone_cfg.region_num = lcd_ini_get_val(inip, psec, "if_attr_1", 0);
		pctrl->vbyone_cfg.byte_mode  = lcd_ini_get_val(inip, psec, "if_attr_2", 0);
		pctrl->vbyone_cfg.color_fmt  = lcd_ini_get_val(inip, psec, "if_attr_3", 0);
		pctrl->vbyone_cfg.phy_vswing = lcd_ini_get_val(inip, psec, "if_attr_4", 0);
		pctrl->vbyone_cfg.phy_preem = lcd_ini_get_val(inip, psec, "if_attr_5", 0);
		pctrl->vbyone_cfg.hw_filter_time = lcd_ini_get_val(inip, psec, "if_attr_8", 0);
		pctrl->vbyone_cfg.hw_filter_cnt = lcd_ini_get_val(inip, psec, "if_attr_9", 0);
		pctrl->vbyone_cfg.ctrl_flag = 0;
		pctrl->vbyone_cfg.power_on_reset_delay = VX1_PWR_ON_RESET_DLY_DFT;
		pctrl->vbyone_cfg.hpd_data_delay = VX1_HPD_DATA_DELAY_DFT;
		pctrl->vbyone_cfg.cdr_training_hold = VX1_CDR_TRAINING_HOLD_DFT;
		pctrl->vbyone_cfg.slice = pdrv->config.timing.ppc ? pdrv->config.timing.ppc : 1;

		phy_cfg->vswing_level = pctrl->vbyone_cfg.phy_vswing & 0xf;
		phy_cfg->ext_pullup = (pctrl->vbyone_cfg.phy_vswing >> 4) & 0x3;
		phy_cfg->preem_level = pctrl->vbyone_cfg.phy_preem;
		break;
	case LCD_MLVDS:
		pctrl->mlvds_cfg.channel_num = lcd_ini_get_val(inip, psec, "if_attr_0", 0);
		pctrl->mlvds_cfg.channel_sel0 =
			(lcd_ini_get_val(inip, psec, "if_attr_1", 0) |
			 (lcd_ini_get_val(inip, psec, "if_attr_2", 0) << 16));
		pctrl->mlvds_cfg.channel_sel1 =
			(lcd_ini_get_val(inip, psec, "if_attr_3", 0) |
			 (lcd_ini_get_val(inip, psec, "if_attr_4", 0) << 16));
		pctrl->mlvds_cfg.clk_phase = lcd_ini_get_val(inip, psec, "if_attr_5", 0);
		pctrl->mlvds_cfg.pn_swap = lcd_ini_get_val(inip, psec, "if_attr_6", 0);
		pctrl->mlvds_cfg.bit_swap = lcd_ini_get_val(inip, psec, "if_attr_7", 0);
		pctrl->mlvds_cfg.phy_vswing = lcd_ini_get_val(inip, psec, "if_attr_8", 0);
		pctrl->mlvds_cfg.phy_preem = lcd_ini_get_val(inip, psec, "if_attr_9", 0);

		phy_cfg->vswing_level = pctrl->mlvds_cfg.phy_vswing & 0xf;
		phy_cfg->ext_pullup = (pctrl->mlvds_cfg.phy_vswing >> 4) & 0x3;
		phy_cfg->preem_level = pctrl->mlvds_cfg.phy_preem;
		break;
	case LCD_P2P:
		pctrl->p2p_cfg.p2p_type = lcd_ini_get_val(inip, psec, "if_attr_0", 0);
		pctrl->p2p_cfg.lane_num = lcd_ini_get_val(inip, psec, "if_attr_1", 0);
		pctrl->p2p_cfg.channel_sel0 =
			(lcd_ini_get_val(inip, psec, "if_attr_2", 0) |
			 (lcd_ini_get_val(inip, psec, "if_attr_3", 0) << 16));
		pctrl->p2p_cfg.channel_sel1 =
			(lcd_ini_get_val(inip, psec, "if_attr_4", 0) |
			 (lcd_ini_get_val(inip, psec, "if_attr_5", 0) << 16));
		pctrl->p2p_cfg.pn_swap = lcd_ini_get_val(inip, psec, "if_attr_6", 0);
		pctrl->p2p_cfg.bit_swap = lcd_ini_get_val(inip, psec, "if_attr_7", 0);
		pctrl->p2p_cfg.phy_vswing = lcd_ini_get_val(inip, psec, "if_attr_8", 0);
		pctrl->p2p_cfg.phy_preem = lcd_ini_get_val(inip, psec, "if_attr_9", 0);

		phy_cfg->vswing_level = pctrl->p2p_cfg.phy_vswing & 0xf;
		phy_cfg->ext_pullup = (pctrl->p2p_cfg.phy_vswing >> 4) & 0x3;
		phy_cfg->preem_level = pctrl->p2p_cfg.phy_preem;
		break;
	default:
		LCDERR("[%d]: unsupport lcd_type: %d\n",
		       pdrv->index, pconf->basic.lcd_type);
		break;
	}

	phy = lcd_phy_alloc(pdrv);
	if (!phy)
		return -1;
	memset(phy, 0, sizeof(*phy));
	phy_cfg->act_phy = phy_cfg->phys[0];
	lcd_phy_param_preset(pdrv);
	lcd_lane_map_preset(pdrv);
	phy->ss.freq = ptiming->ss_freq;
	phy->ss.level = ptiming->ss_level;
	phy->ss.mode = ptiming->ss_mode;

	/* step 3: check power sequence */
	ret = lcd_power_load_from_ini(pdrv, inip, psec);
	if (ret < 0)
		return -1;

	switch (version) {
	case 2:
		lcd_config_load_from_ini_v2(pdrv, inip, psec, version);
		break;
	case 3:
		lcd_config_load_from_ini_v3(pdrv, inip, psec, version);
		break;
	default:
		break;
	}

	//fix ss in detail timing and phy_attr if not config
	lcd_ss_config_fix(pdrv);

	lcd_vlock_param_load_from_ini(pdrv, inip, psec);

	lcd_optical_load_from_ini(pdrv, inip);

#ifdef CONFIG_AMLOGIC_BACKLIGHT
	lcd_resource_add(pdrv, LCD_RES_BACKLIGHT, 0);
	aml_bl_index_add(pdrv->index, 0);
#endif

	return 0;
}

unsigned char lcd_panel_config_load_detect(int index, int key_valid, const char *func_name)
{
	unsigned char load = LCD_CONFIG_NONE;
	unsigned char file_type = PANEL_FILE_INVILD;

	file_type = get_lcd_panel_file_type(index);
	load = lcd_get_dbg_source();
	if (load != LCD_CONFIG_NONE) {
		switch (load) {
		case LCD_CONFIG_DTS:
			break;
		case LCD_CONFIG_UKEY:
			if (!key_valid)
				load = LCD_CONFIG_ERR;
			break;
		case LCD_CONFIG_FILE:
			if (file_type != PANEL_FILE_JSON && file_type != PANEL_FILE_INI)
				load = LCD_CONFIG_ERR;
			break;
		default:
			load = LCD_CONFIG_NONE;
		}
		goto lcd_panel_config_load_detect_done;
	}

	if (file_type == PANEL_FILE_INI || file_type == PANEL_FILE_JSON) {
		load = LCD_CONFIG_FILE;
	} else {
		if (key_valid)
			load = LCD_CONFIG_UKEY;
		else
			load = LCD_CONFIG_DTS;
	}

lcd_panel_config_load_detect_done:
	if (load == LCD_CONFIG_ERR)
		LCDERR("[%d]: %s: ERROR, key_valid:%d\n", index, func_name, key_valid);
	else if (load == LCD_CONFIG_NONE)
		LCDPR("[%d]: %s: NONE, key_valid:%d\n", index, func_name, key_valid);
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s: %d, key_valid:%d\n", index, func_name, load, key_valid);

	return load;
}

int lcd_check_config_load(struct aml_lcd_drv_s *drv)
{
	drv->config_load = lcd_panel_config_load_detect(drv->index, drv->key_valid, __func__);
	if (drv->config_load == LCD_CONFIG_NONE || drv->config_load == LCD_CONFIG_ERR)
		return -1;

	return 0;
}

static int lcd_config_load_init(struct aml_lcd_drv_s *pdrv)
{
	if (pdrv->config.basic.config_check & 0x2)
		pdrv->config_check_en = pdrv->config.basic.config_check & 0x1;
	else
		pdrv->config_check_en = pdrv->config_check_glb;

	switch (pdrv->config.fr_auto_cus) {
	case 0: //follow global fr_auto_policy
		pdrv->config.fr_auto_flag = pdrv->fr_auto_policy;
		break;
	case 0xff: //disable fr_auto
		pdrv->config.fr_auto_flag = 0xff;
		break;
	default: //custom fr_auto
		pdrv->config.fr_auto_flag = pdrv->config.fr_auto_cus;
		break;
	}

	if (pdrv->index)
		pdrv->config.timing.ppc = 1;

	if (pdrv->status & LCD_STATUS_ENCL_ON)
		lcd_clk_gate_switch(pdrv, 1);

	return 0;
}

int lcd_get_config(struct aml_lcd_drv_s *pdrv)
{
	char key_str[10];
	int ret = -1;
	unsigned char file_type = PANEL_FILE_INVILD;

	memset(key_str, 0, 10);
	if (pdrv->index == 0)
		sprintf(key_str, "lcd");
	else
		sprintf(key_str, "lcd%d", pdrv->index);

	switch (pdrv->config_load) {
	case LCD_CONFIG_DTS:
		ret = lcd_config_load_from_dts(pdrv);
		break;
	case LCD_CONFIG_UKEY:
		ret = lcd_config_load_from_unifykey(pdrv, key_str);
		break;
	case LCD_CONFIG_FILE:
		file_type = get_lcd_panel_file_type(pdrv->index);
		if (file_type == PANEL_FILE_JSON)
			ret = lcd_config_load_from_json(pdrv, NULL);
		else if (file_type == PANEL_FILE_INI)
			ret = lcd_config_load_from_ini(pdrv, NULL);
		break;
	default:
		ret = -1;
		break;
	}
	if (ret)
		return -1;

	lcd_lane_map_update(pdrv);

	lcd_config_load_init(pdrv);
	lcd_config_load_print(pdrv);

	lcd_tcon_probe(pdrv);

	return 0;
}
