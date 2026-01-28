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
#include <linux/fs.h>
#include <linux/uaccess.h>
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#ifdef CONFIG_AMLOGIC_BACKLIGHT
#include <linux/amlogic/media/vout/lcd/aml_bl.h>
#endif
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/lcd/lcd_tcon_data.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#include <linux/amlogic/media/vout/lcd/lcd_unifykey.h>
#include <linux/amlogic/media/vout/lcd/lcd_model.h>
#include "./lcd_clk/lcd_clk_config.h"
#include "lcd_reg.h"
#include "lcd_common.h"
#include "lcd_debug.h"

int lcd_debug_parse_param(char *buf_orig, char **parm, int max_parm)
{
	char *ps, *token;
	char str[3] = {' ', '\n', '\0'};
	unsigned int n = 0;

	ps = buf_orig;
	while (n < max_parm) {
		token = strsep(&ps, str);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
	return n;
}

void lcd_debug_info_print(char *print_buf)
{
	char *ps, *token;
	char str[3] = {'\n', '\0'};

	ps = print_buf;
	while (1) {
		token = strsep(&ps, str);
		if (!token)
			break;
		if (*token == '\0') {
			pr_info("\n");
			continue;
		}
		pr_info("%s\n", token);
	}
}

int lcd_debug_info_len(int num)
{
	int ret = 0;

	if (num >= (PR_BUF_MAX - 1)) {
		pr_info("%s: string length %d is out of support\n",
			__func__, num);
		return 0;
	}

	ret = PR_BUF_MAX - 1 - num;
	return ret;
}

int str_add_reg_sets(struct aml_lcd_drv_s *pdrv, char *buf, int offset,
		     unsigned char reg_bus, unsigned int reg_offset,
		     struct reg_name_set_s *reg_sets, unsigned char set_cnt)
{
	unsigned char idx, str_pos = 0;
#ifdef CONFIG_AMLOGIC_LCD_TABLET
	unsigned char reg_temp;
#endif
	unsigned int reg_val, len = 0, n;

	for (idx = 0; idx < set_cnt; idx++) {
		if (strlen(reg_sets[idx].name) > str_pos)
			str_pos = strlen(reg_sets[idx].name);
	}
	str_pos++;

	for (idx = 0; idx < set_cnt; idx++) {
		switch (reg_bus) {
		case LCD_REG_DBG_VC_BUS:
			reg_val = lcd_vcbus_read(reg_sets[idx].reg + reg_offset);
			break;
		case LCD_REG_DBG_ANA_BUS:
			reg_val = lcd_ana_read(reg_sets[idx].reg + reg_offset);
			break;
		case LCD_REG_DBG_CLK_BUS:
			reg_val = lcd_clk_read(reg_sets[idx].reg + reg_offset);
			break;
		case LCD_REG_DBG_PERIPHS_BUS:
			reg_val = lcd_periphs_read(pdrv, reg_sets[idx].reg + reg_offset);
			break;
#ifdef CONFIG_AMLOGIC_LCD_TABLET
		case LCD_REG_DBG_MIPIHOST_BUS:
			reg_val = dsi_host_read(pdrv, reg_sets[idx].reg + reg_offset);
			break;
		case LCD_REG_DBG_MIPIPHY_BUS:
			reg_val = dsi_phy_read(pdrv, reg_sets[idx].reg + reg_offset);
			break;
		case LCD_REG_DBG_EDPHOST_BUS:
			reg_val = dptx_reg_read(pdrv, reg_sets[idx].reg + reg_offset);
			break;
		case LCD_REG_DBG_EDPDPCD_BUS:
			if (dptx_aux_read(pdrv, reg_sets[idx].reg + reg_offset, 1, &reg_temp))
				continue;
			reg_val = reg_temp;
			break;
#endif
#ifdef CONFIG_AMLOGIC_LCD_TV
		case LCD_REG_DBG_TCON_BUS:
			reg_val = lcd_tcon_reg_read(pdrv, reg_sets[idx].reg + reg_offset);
			break;
#endif
		case LCD_REG_DBG_COMBOPHY_BUS:
			reg_val = lcd_combo_dphy_read(pdrv, reg_sets[idx].reg + reg_offset);
			break;
		case LCD_REG_DBG_RST_BUS:
			reg_val = lcd_reset_read(pdrv, reg_sets[idx].reg + reg_offset);
			break;
		case LCD_REG_DBG_HHI_BUS:
			reg_val = lcd_hiu_read(reg_sets[idx].reg + reg_offset);
			break;
		default:
			return len;
		}

		n = lcd_debug_info_len(len + offset);
		len += snprintf(buf + len, n, "%-*s [0x%04x] = 0x%08x\n", str_pos,
			reg_sets[idx].name, reg_sets[idx].reg, reg_val);
	}

	return len;
}

static const char *lcd_common_usage_str = {
"Usage:\n"
"  echo 0|1 > enable\n"
"  echo type <adj_type> > frame_rate\n"
"  echo set <frame_rate> > frame_rate\n"
"  echo <num> > test\n"
"  echo level|freq|mode <val> > ss\n"
"  echo w|r|d<type> <reg> <val> > > reg; write:val=reg_value, read|dump:val=cnt\n"
"  echo <level> > print\n"
"  echo <cmd> > dump\n"
"  echo <cmd> ... > debug\n"
"  echo 0|1 > power\n"
"  echo on|off <step_num> <delay> > power_step\n"
};

static const char *lcd_debug_usage_str = {
"Usage:\n"
"  echo clk <freq> > debug\n"
"  echo bit <lcd_bits> > debug\n"
"  echo basic <h_active> <v_active> <h_period> <v_period> <lcd_bits> > debug\n"
"  echo sync <hs_width> <hs_bp> <hs_pol> <vs_width> <vs_bp> <vs_pol> > debug\n"
"  echo info > debug\n"
"  echo reg > debug\n"
"  echo dump > debug\n"
"  echo dith <dither_en> <rounding_en> <dither_md>  > debug\n"
"  echo key > debug\n"
"  echo reset > debug\n"
"  echo power 0|1 > debug\n"
};

static const char *lcd_debug_change_usage_str = {
"Usage:\n"
"  echo clk <freq> > change\n"
"  echo bit <lcd_bits> > change\n"
"  echo basic <h_active> <v_active> <h_period> <v_period> <lcd_bits> > change\n"
"  echo sync <hs_width> <hs_bp> <hs_pol> <vs_width> <vs_bp> <vs_pol> > change\n"
"  echo set > change\n"
};

static ssize_t lcd_debug_common_help(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", lcd_common_usage_str);
}

static ssize_t lcd_debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", lcd_debug_usage_str);
}

static int lcd_cpu_gpio_register_print(struct lcd_config_s *pconf, char *buf, int offset)
{
	int i, n, len = 0;
	struct lcd_cpu_gpio_s *cpu_gpio;

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n, "cpu_gpio register:\n");

	i = 0;
	while (i < LCD_CPU_GPIO_NUM_MAX) {
		cpu_gpio = &pconf->power.cpu_gpio[i];
		if (cpu_gpio->probe_flag == 0) {
			i++;
			continue;
		}

		if (cpu_gpio->register_flag) {
			n = lcd_debug_info_len(len + offset);
			len += snprintf((buf + len), n, "%d: name=%s, gpio=%p\n",
				i, cpu_gpio->name, cpu_gpio->gpio);
		} else {
			n = lcd_debug_info_len(len + offset);
			len += snprintf((buf + len), n, "%d: name=%s, no registered\n",
				i, cpu_gpio->name);
		}
		i++;
	}

	return len;
}

static int lcd_power_step_print(struct lcd_config_s *pconf, int status, char *buf, int offset)
{
	struct lcd_power_step_s *power_step;
	int i = 0, max_step, n, len = 0;

	n = lcd_debug_info_len(len + offset);
	if (status)
		len += snprintf((buf + len), n, "power on step:\n");
	else
		len += snprintf((buf + len), n, "power off step:\n");

	if (status) {
		power_step = pconf->power.power_on_step;
		max_step = pconf->power.power_on_step_max;
	} else {
		power_step = pconf->power.power_off_step;
		max_step = pconf->power.power_off_step_max;
	}
	while (i < max_step) {
		if (power_step[i].type >= LCD_POWER_TYPE_MAX)
			break;

		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n,
			"%d: type=%d, index=%d, value=%d, delay=%d\n",
			i, power_step[i].type, power_step[i].index,
			power_step[i].value, power_step[i].delay);

		i++;
	}

	return len;
}

static int lcd_power_step_info_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int len = 0;

	len += lcd_power_step_print(&pdrv->config, 1, (buf + len), (len + offset));
	len += lcd_power_step_print(&pdrv->config, 0, (buf + len), (len + offset));
	len += lcd_cpu_gpio_register_print(&pdrv->config, (buf + len), (len + offset));

	return len;
}

static int lcd_info_print_lvds(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int n, len = 0;

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"lvds_repack : %u\n"
		"dual_port   : %u\n"
		"pn_swap     : %u\n"
		"port_swap   : %u\n"
		"lane_reverse: %u\n"
		"phy_vswing  : 0x%x\n"
		"phy_preem   : 0x%x\n\n",
		pdrv->config.control.lvds_cfg.lvds_repack,
		pdrv->config.control.lvds_cfg.dual_port,
		pdrv->config.control.lvds_cfg.pn_swap,
		pdrv->config.control.lvds_cfg.port_swap,
		pdrv->config.control.lvds_cfg.lane_reverse,
		pdrv->config.control.lvds_cfg.phy_vswing,
		pdrv->config.control.lvds_cfg.phy_preem);

	return len;
}

static int lcd_info_print_vbyone(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	struct vbyone_config_s *vx1_conf;
	int n, len = 0;

	vx1_conf = &pdrv->config.control.vbyone_cfg;

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"lane_count    : %u\n"
		"region_num    : %u\n"
		"byte_mode     : %u\n"
		"color_fmt     : %u\n"
		"bit_rate      : %lluHz\n"
		"phy_vswing    : 0x%x\n"
		"phy_preem     : 0x%x\n"
		"intr_en       : %u\n"
		"vsync_intr_en : %u\n"
		"hw_filter_time: 0x%x\n"
		"hw_filter_cnt : 0x%x\n"
		"ctrl_flag     : 0x%x\n\n",
		vx1_conf->lane_count,
		vx1_conf->region_num,
		vx1_conf->byte_mode,
		vx1_conf->color_fmt,
		pdrv->config.timing.bit_rate,
		vx1_conf->phy_vswing,
		vx1_conf->phy_preem,
		vx1_conf->intr_en,
		vx1_conf->vsync_intr_en,
		vx1_conf->hw_filter_time,
		vx1_conf->hw_filter_cnt,
		vx1_conf->ctrl_flag);
	if (vx1_conf->ctrl_flag & 0x1) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n,
			"power_on_reset_en    %u\n"
			"power_on_reset_delay %ums\n\n",
			(vx1_conf->ctrl_flag & 0x1),
			vx1_conf->power_on_reset_delay);
	}
	if (vx1_conf->ctrl_flag & 0x2) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n,
			"hpd_data_delay_en    %u\n"
			"hpd_data_delay       %ums\n\n",
			((vx1_conf->ctrl_flag >> 1) & 0x1),
			vx1_conf->hpd_data_delay);
	}
	if (vx1_conf->ctrl_flag & 0x4) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n,
			"cdr_training_hold_en %u\n"
			"cdr_training_hold    %ums\n\n",
			((vx1_conf->ctrl_flag >> 2) & 0x1),
			vx1_conf->cdr_training_hold);
	}

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"pinmux_flag          %d\n\n", pdrv->config.pinmux_flag);

	return len;
}

#ifdef CONFIG_AMLOGIC_LCD_TABLET
static int lcd_info_print_mipi(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int len = 0;

	lcd_dsi_info_print(&pdrv->config);

	return len;
}

static int lcd_info_print_edp(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int n, len = 0;

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"max_lane_count: %d\n"
		"max_link_rate : %d\n"
		"training_mode : %d\n"
		"sync_clk_mode : %d\n"
		"lane_count    : %d\n"
		"link_rate     : %d\n"
		"bit_rate      : %lluHz\n"
		"phy_vswing    : 0x%x\n"
		"phy_preem     : 0x%x\n\n",
		pdrv->config.control.edp_cfg.max_lane_count,
		pdrv->config.control.edp_cfg.max_link_rate,
		pdrv->config.control.edp_cfg.training_mode,
		pdrv->config.control.edp_cfg.sync_clk_mode,
		pdrv->config.control.edp_cfg.lane_count,
		pdrv->config.control.edp_cfg.link_rate,
		pdrv->config.timing.bit_rate,
		pdrv->config.control.edp_cfg.phy_vswing_preset,
		pdrv->config.control.edp_cfg.phy_preem_preset);

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"pinmux_flag   : %d\n\n", pdrv->config.pinmux_flag);

	return len;
}
#endif

#ifdef CONFIG_AMLOGIC_LCD_TV
static int lcd_info_print_mlvds(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int n, len = 0;

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"channel_num : %d\n"
		"channel_sel0: 0x%08x\n"
		"channel_sel1: 0x%08x\n"
		"clk_phase   : 0x%04x\n"
		"pn_swap     : %u\n"
		"bit_swap    : %u\n"
		"phy_vswing  : 0x%x\n"
		"phy_preem   : 0x%x\n"
		"bit_rate    : %lluHz\n\n",
		pdrv->config.control.mlvds_cfg.channel_num,
		pdrv->config.control.mlvds_cfg.channel_sel0,
		pdrv->config.control.mlvds_cfg.channel_sel1,
		pdrv->config.control.mlvds_cfg.clk_phase,
		pdrv->config.control.mlvds_cfg.pn_swap,
		pdrv->config.control.mlvds_cfg.bit_swap,
		pdrv->config.control.mlvds_cfg.phy_vswing,
		pdrv->config.control.mlvds_cfg.phy_preem,
		pdrv->config.timing.bit_rate);

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"pinmux_flag : %d\n", pdrv->config.pinmux_flag);

	return len;
}

static int lcd_info_print_p2p(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int n, len = 0;

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"p2p_type    : 0x%x\n"
		"lane_num    : %d\n"
		"channel_sel0: 0x%08x\n"
		"channel_sel1: 0x%08x\n"
		"pn_swap     : %u\n"
		"bit_swap    : %u\n"
		"bit_rate    : %lluHz\n"
		"phy_vswing  : 0x%x\n"
		"phy_preem   : 0x%x\n\n",
		pdrv->config.control.p2p_cfg.p2p_type,
		pdrv->config.control.p2p_cfg.lane_num,
		pdrv->config.control.p2p_cfg.channel_sel0,
		pdrv->config.control.p2p_cfg.channel_sel1,
		pdrv->config.control.p2p_cfg.pn_swap,
		pdrv->config.control.p2p_cfg.bit_swap,
		pdrv->config.timing.bit_rate,
		pdrv->config.control.p2p_cfg.phy_vswing,
		pdrv->config.control.p2p_cfg.phy_preem);

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"pinmux_flag : %d\n\n", pdrv->config.pinmux_flag);

	return len;
}
#endif

static int lcd_info_basic_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	struct lcd_config_s *pconf;
	unsigned int sync_duration, mute_state = 0;
	int n, len = 0, i, pr_len = 4 * 1024, base_id = 0, tag_id = 0, tag_id2 = 0;
	char *pr_buf = NULL;
	struct lcd_detail_timing_s *dt;
	const char * const tags[] = {"", "(default)", "(base)"};

	pconf = &pdrv->config;
	sync_duration = pconf->timing.act_timing.sync_duration_num * 100;
	sync_duration = sync_duration / pconf->timing.act_timing.sync_duration_den;
	mute_state = lcd_mute_state_get(pdrv);

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"[%d]: driver version: %s\n"
		"config_check_glb: %d, config_check_para: 0x%x, config_check_en: %d\n"
		"panel_type: %s, chip: %d, mode: %s, status: 0x%x\n"
		"viu_sel: %d, isr_cnt: %d, resume_type: %d\n"
		"fr_auto_flag: 0x%x, fr_mode: %d, fr_duration: %d, frame_rate: %d\n"
		"fr_auto_policy(global): %d, fr_auto_cus: 0x%x, custom_pinmux: %d\n"
		"mute_state: %d(real %d), test_flag: 0x%x\n"
		"key_valid: %d, config_load: %d\n\n",
		pdrv->index, LCD_DRV_VERSION,
		pdrv->config_check_glb, pconf->basic.config_check, pdrv->config_check_en,
		pconf->propname, pdrv->data->chip_type,
		lcd_mode_mode_to_str(pdrv->mode), pdrv->status,
		pdrv->viu_sel, pdrv->vsync_cnt, pdrv->resume_type,
		pconf->fr_auto_flag, pdrv->fr_mode, pdrv->fr_duration,
		pconf->timing.act_timing.frame_rate,
		pdrv->fr_auto_policy, pconf->fr_auto_cus, pconf->custom_pinmux,
		pdrv->mute_flag, mute_state, pdrv->test_flag,
		pdrv->key_valid, pdrv->config_load);

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"%s, %s %ubit, %dppc, %ux%u@%d.%02dHz\n",
		pconf->basic.model_name,
		lcd_type_type_to_str(pconf->basic.lcd_type),
		pconf->timing.act_timing.lcd_bits, pconf->timing.ppc,
		pconf->timing.act_timing.h_active, pconf->timing.act_timing.v_active,
		(sync_duration / 100), (sync_duration % 100));

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"bit_rate       %llu\n"
		"enc_clk        %uHz\n"
		"clk_mode       %s(%d)\n"
		"ss_level       %d\n"
		"ss_freq        %d\n"
		"ss_mode        %d\n"
		"pll_flag       %d\n\n",
		pconf->timing.bit_rate,
		pconf->timing.enc_clk,
		(pconf->timing.clk_mode ? "independence" : "dependence"),
		pconf->timing.clk_mode,
		pconf->timing.ss_level, pconf->timing.ss_freq, pconf->timing.ss_mode,
		pconf->timing.pll_flag);

	pr_buf = kzalloc(pr_len, GFP_KERNEL);
	if (!pr_buf)
		return len;

	for (i = 0; i < pdrv->config.timing.num_timings; i++) {
		dt = pdrv->config.timing.timings[i];
		if (!dt)
			continue;
		lcd_detail_timing_print(dt, pr_buf, 0, pr_len);
		n = lcd_debug_info_len(len + offset);
		tag_id = dt == pdrv->config.timing.dft_timing ? 1 : 0;
		tag_id2 = dt == pdrv->config.timing.base_timing ? 2 : 0;
		len += snprintf(buf + len, n, "\ntiming[%d]%s%s:\n%s",
				i, tags[tag_id], tags[tag_id2], pr_buf);
		if (dt == pdrv->config.timing.base_timing)
			base_id = i;
	}

	lcd_config_timing_check(pdrv, &pconf->timing.act_timing);
	dt = &pconf->timing.act_timing;
	lcd_detail_timing_print(dt, pr_buf, 0, pr_len);
	n = lcd_debug_info_len(len + offset);
	len += snprintf(buf + len, n, "\nact_timing: based timing[%d]:\n%s\n", base_id, pr_buf);

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"pre_de_h %d, pre_de_v %d\n"
		"hstart   %d, vstart   %d\n\n",
		pconf->timing.pre_de_h, pconf->timing.pre_de_v,
		pconf->timing.hstart, pconf->timing.vstart);

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"pll_ctrl  0x%08x\n"
		"div_ctrl  0x%08x\n"
		"clk_ctrl  0x%08x\n",
		pconf->timing.pll_ctrl, pconf->timing.div_ctrl,
		pconf->timing.clk_ctrl);

	if (pconf->timing.clk_mode == LCD_CLK_MODE_INDEPENDENCE) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n,
			"pll_ctrl2 0x%08x\n"
			"div_ctrl2 0x%08x\n"
			"clk_ctrl2 0x%08x\n",
			pconf->timing.pll_ctrl2, pconf->timing.div_ctrl2,
			pconf->timing.clk_ctrl2);
	}
	kfree(pr_buf);
	return len;
}

static int lcd_info_adv_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	struct lcd_debug_info_s *lcd_debug_info = (struct lcd_debug_info_s *)pdrv->debug_info;
	char *buf_tmp;
	int n, len = 0, i = 0, base_id = 0;
	struct phy_config_s *phy_cfg = &pdrv->config.phy_cfg;
	struct phy_attr_s *phy;

	if (!lcd_debug_info) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n, "%s: debug_info is null\n", __func__);
		return len;
	}
	if (lcd_debug_info->interface_print)
		len += lcd_debug_info->interface_print(pdrv, (buf + len), (len + offset));

	/* phy_attr */
	switch (pdrv->config.basic.lcd_type) {
	case LCD_LVDS:
	case LCD_VBYONE:
	case LCD_MLVDS:
	case LCD_P2P:
	case LCD_EDP:
		buf_tmp = kzalloc(2048, GFP_KERNEL);
		if (!buf_tmp)
			break;

		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n, "phy config:\n");

		for (i = 0; i < phy_cfg->group_num; i++) {
			phy = phy_cfg->phys[i];
			if (!phy)
				continue;
			n = lcd_debug_info_len(len + offset);
			len += snprintf((buf + len), n, "phy_attr[%d]%s:\n",
					i, phy == phy_cfg->act_phy ? "(active)" : "");

			lcd_phy_attr_print(phy, phy_cfg->lane_num, buf_tmp, 0, 2048);
			n = lcd_debug_info_len(len + offset);
			len += snprintf(buf + len, n, "%s\n", buf_tmp);
			if (phy == pdrv->config.phy_cfg.act_phy)
				base_id = i;
		}

		lcd_phy_param_print(pdrv, buf_tmp, 0);
		n = lcd_debug_info_len(len + offset);
		len += snprintf(buf + len, n, "active phy(group[%d]):\n%s\n", base_id, buf_tmp);
		kfree(buf_tmp);
		break;
	default:
		break;
	}

	return len;
}

static ssize_t lcd_proc_time_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	ssize_t len = 0;

	len = sprintf(buf, "switch times attr:\n"
		"switch_type:    0x%x\n"
		"mute_time:        %llu\n"
		"bl_off_time:      %llu\n"
		"switch_off_time:  %llu\n"
		"tcon_off_time:    %llu\n"
		"signal_off_time:  %llu\n"
		"power_off_time:   %llu\n"
		"signal_on_time:   %llu\n"
		"drv_change_time:  %llu\n"
		"extern_init_time: %llu\n"
		"tcon_reg_time:    %llu\n"
		"tcon_data_time:   %llu\n"
		"tcon_on_time:     %llu\n"
		"power_on_time:    %llu\n"
		"switch_on_time:   %llu\n"
		"bl_on_time:       %llu\n"
		"unmute_time:      %llu\n"
		"switch_full_time: %llu\n\n"
		"lcd_vs_isr_time:  %llu\n"
		"tcon_vs_isr_time: %llu\n\n",
		pdrv->config.timing.switch_type,
		pdrv->proc_time.mute_time,
		pdrv->proc_time.bl_off_time,
		pdrv->proc_time.switch_off_time,
		pdrv->proc_time.tcon_off_time,
		pdrv->proc_time.signal_off_time,
		pdrv->proc_time.power_off_time,
		pdrv->proc_time.signal_on_time,
		pdrv->proc_time.driver_change_time,
		pdrv->proc_time.extern_init_time,
		pdrv->proc_time.tcon_reg_time,
		pdrv->proc_time.tcon_data_time,
		pdrv->proc_time.tcon_on_time,
		pdrv->proc_time.power_on_time,
		pdrv->proc_time.switch_on_time,
		pdrv->proc_time.bl_on_time,
		pdrv->proc_time.unmute_time,
		pdrv->proc_time.switch_full_time,
		pdrv->proc_time.lcd_vs_isr_time,
		pdrv->proc_time.tcon_vs_isr_time);

	return len;
}

static int lcd_info_tcon_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int len = 0;

	if (pdrv->config.basic.lcd_type != LCD_MLVDS &&
	    pdrv->config.basic.lcd_type != LCD_P2P)
		return len;

	len = lcd_tcon_info_print(pdrv, buf, offset);

	return len;
}

static int lcd_reg_print_lvds(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int n, len = 0;
	struct reg_name_set_s lvds_reg_sets[] = {
		{LVDS_PACK_CNTL_ADDR, "LVDS_PACK_CNTL"},
		{LVDS_GEN_CNTL,       "LVDS_GEN_CNTL"}
	};

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n, "\nlvds regs:\n");
	len += str_add_reg_sets(pdrv, buf + len, len + offset, LCD_REG_DBG_VC_BUS, 0,
				lvds_reg_sets, ARRAY_SIZE(lvds_reg_sets));
	return len;
}

static int lcd_reg_print_lvds_t7(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	unsigned int reg_offset = pdrv->data->offset_venc_if[pdrv->index];
	int n, len = 0;

	struct reg_name_set_s lvds_reg_sets[] = {
		{LVDS_SER_EN_T7,         "LVDS_SER_EN"},
		{LVDS_PACK_CNTL_ADDR_T7, "LVDS_PACK_CNTL_ADDR"},
		{LVDS_GEN_CNTL_T7,       "LVDS_GEN_CNTL"},
		{P2P_BIT_REV_T7,         "P2P_BIT_REV"}
	};

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n, "\nlvds regs:\n");
	len += str_add_reg_sets(pdrv, buf + len, len + offset, LCD_REG_DBG_VC_BUS, reg_offset,
				lvds_reg_sets, ARRAY_SIZE(lvds_reg_sets));
	return len;
}

static int lcd_reg_print_vbyone(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	unsigned int reg_offset = pdrv->data->offset_venc_if[pdrv->index];
	int n, len = 0;
	struct reg_name_set_s vbo_reg_sets[] = {
		{VBO_STATUS_L,               "VBO_STATUS_L"},
		{VBO_CTRL_L,                 "VBO_CTRL"},
		{VBO_GCLK_MAIN,              "VBO_GCLK_MAIN"},
		{VBO_INFILTER_TICK_PERIOD_H, "VBO_INFILTER_TICK_PERIOD_H"},
		{VBO_INFILTER_TICK_PERIOD_L, "VBO_INFILTER_TICK_PERIOD_L"},
		{VBO_INSGN_CTRL,             "VBO_INSGN_CTRL"},
		{VBO_FSM_HOLDER_L,           "VBO_FSM_HOLDER_L"},
		{VBO_FSM_HOLDER_H,           "VBO_FSM_HOLDER_H"},
		{VBO_INTR_STATE_CTRL,        "VBO_INTR_STATE_CTRL"},
		{VBO_INTR_UNMASK,            "VBO_INTR_UNMASK"},
		{VBO_INTR_STATE,             "VBO_INTR_STATE"},
		{LCD_PORT_SWAP,              "LCD_PORT_SWAP"}
	};

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n, "\nvbyone regs:\n");
	len += str_add_reg_sets(pdrv, buf + len, len + offset, LCD_REG_DBG_VC_BUS, reg_offset,
				vbo_reg_sets, ARRAY_SIZE(vbo_reg_sets));
	return len;
}

static int lcd_reg_print_vbyone_t7(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	unsigned int reg_offset = pdrv->data->offset_venc_if[pdrv->index];
	int n, len = 0;
	struct reg_name_set_s vbo_reg_sets[] = {
		{VBO_STATUS_L_T7,        "VBO_STATUS_L"},
		{VBO_CTRL_L_T7,          "VBO_CTRL"},
		{VBO_GCLK_MAIN_T7,       "VBO_GCLK_MAIN"},
		{VBO_INFILTER_CTRL_H_T7, "VBO_INFILTER_CTRL_H"},
		{VBO_INFILTER_CTRL_T7,   "VBO_INFILTER_CTRL"},
		{VBO_INSGN_CTRL_T7,      "VBO_INSGN_CTRL"},
		{VBO_FSM_HOLDER_L_T7,    "VBO_FSM_HOLDER_L"},
		{VBO_FSM_HOLDER_H_T7,    "VBO_FSM_HOLDER_H"},
		{VBO_INTR_STATE_CTRL_T7, "VBO_INTR_STATE_CTRL"},
		{VBO_INTR_UNMASK_T7,     "VBO_INTR_UNMASK"},
		{VBO_INTR_STATE_T7,      "VBO_INTR_STATE"},
	};

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n, "\nvbyone regs:\n");
	len += str_add_reg_sets(pdrv, buf + len, len + offset, LCD_REG_DBG_VC_BUS, reg_offset,
				vbo_reg_sets, ARRAY_SIZE(vbo_reg_sets));
	return len;
}

static int lcd_reg_print_vbyone_t3x(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	unsigned int reg_offset = pdrv->data->offset_venc_if[pdrv->index];
	int n, len = 0;
	struct reg_name_set_s vbo_reg_sets[] = {
		{VBO_STATUS_L_T3X,        "VBO_STATUS"},
		{VBO_CTRL_T3X,            "VBO_CTRL"},
		{VBO_GCLK_MAIN_T3X,       "VBO_GCLK_MAIN"},
		{VBO_INFILTER_CTRL_T3X,   "VBO_INFILTER_CTRL"},
		{VBO_INSGN_CTRL_T3X,      "VBO_INSGN_CTRL"},
		{VBO_FSM_HOLDER_T3X,      "VBO_FSM_HOLDER"},
		{VBO_INTR_STATE_CTRL_T3X, "VBO_INTR_STATE_CTRL"},
		{VBO_INTR_UNMASK_T3X,     "VBO_INTR_UNMASK"},
		{VBO_INTR_STATE_T3X,      "VBO_INTR_STATE"},
		{VBO_LANES_T3X,           "VBO_LANES"},
		{VBO_VIN_CTRL_T3X,        "VBO_VIN_CTRL"},
		{VBO_ACT_VSIZE_T3X,       "VBO_ACT_VSIZE"},
		{VBO_PXL_CTRL_T3X,        "VBO_PXL_CTRL"},
		{VBO_RGN_HSIZE_T3X,       "VBO_RGN_HSIZE"},
		{VBO_RGN_CTRL_T3X,        "VBO_RGN_CTRL"},
		{VBO_SLICE_CTRL_T3X,      "VBO_SLICE_CTRL"},
	};

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n, "\nvbyone regs:\n");
	len += str_add_reg_sets(pdrv, buf + len, len + offset, LCD_REG_DBG_VC_BUS, reg_offset,
				vbo_reg_sets, ARRAY_SIZE(vbo_reg_sets));
	return len;
}

#ifdef CONFIG_AMLOGIC_LCD_TABLET
static int lcd_reg_print_mipi(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int len = 0;
	struct reg_name_set_s dsi_reg_sets[] = {
		{MIPI_DSI_TOP_CNTL,          "MIPI_DSI_TOP_CNTL"},
		{MIPI_DSI_TOP_CLK_CNTL,      "MIPI_DSI_TOP_CLK_CNTL"},
		{MIPI_DSI_DWC_PWR_UP_OS,     "MIPI_DSI_DWC_PWR_UP_OS"},
		{MIPI_DSI_DWC_PCKHDL_CFG_OS, "MIPI_DSI_DWC_PCKHDL_CFG_OS"},
		{MIPI_DSI_DWC_LPCLK_CTRL_OS, "MIPI_DSI_DWC_LPCLK_CTRL_OS"},
		{MIPI_DSI_DWC_CMD_MODE_CFG_OS, "MIPI_DSI_DWC_CMD_MODE_CFG_OS"},
		{MIPI_DSI_DWC_VID_MODE_CFG_OS, "MIPI_DSI_DWC_VID_MODE_CFG_OS"},
		{MIPI_DSI_DWC_VID_PKT_SIZE_OS, "DWC_VID_PKT_SIZE_OS"},
		{MIPI_DSI_DWC_VID_NUM_CHUNKS_OS, "DWC_VID_NUM_CHUNKS_OS"},
		{MIPI_DSI_DWC_VID_NULL_SIZE_OS, "DWC_VID_NULL_SIZE_OS"},
		{MIPI_DSI_DWC_MODE_CFG_OS,    "MIPI_DSI_DWC_MODE_CFG_OS"},
		{MIPI_DSI_DWC_PHY_STATUS_OS,  "MIPI_DSI_DWC_PHY_STATUS_OS"},
		{MIPI_DSI_DWC_INT_ST0_OS,     "MIPI_DSI_DWC_INT_ST0_OS"},
		{MIPI_DSI_DWC_INT_ST1_OS,     "MIPI_DSI_DWC_INT_ST1_OS"},
		{MIPI_DSI_TOP_STAT,           "MIPI_DSI_TOP_STAT"},
		{MIPI_DSI_TOP_INTR_CNTL_STAT, "MIPI_DSI_TOP_INTR_CNTL_STAT"},
		{MIPI_DSI_TOP_MEM_PD,         "MIPI_DSI_TOP_MEM_PD"},
	};

	len += str_add_reg_sets(pdrv, buf + len, len + offset, LCD_REG_DBG_MIPIHOST_BUS, 0,
			dsi_reg_sets, ARRAY_SIZE(dsi_reg_sets));
	return len;
}

static int lcd_reg_print_edp(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int len = 0;
	struct reg_name_set_s edp_reg_sets[] = {
		{EDP_TX_LINK_BW_SET,               "EDP_TX_LINK_BW_SET"},
		{EDP_TX_LINK_COUNT_SET,            "EDP_TX_LINK_COUNT_SET"},
		{EDP_TX_TRAINING_PATTERN_SET,      "EDP_TX_TRAINING_PATTERN_SET"},
		{EDP_TX_SCRAMBLING_DISABLE,        "EDP_TX_SCRAMBLING_DISABLE"},
		{EDP_TX_TRANSMITTER_OUTPUT_ENABLE, "EDP_TX_TRANSMITTER_OUTPUT_ENABLE"},
		{EDP_TX_MAIN_STREAM_ENABLE,        "EDP_TX_MAIN_STREAM_ENABLE"},
		{EDP_TX_PHY_RESET,                 "EDP_TX_PHY_RESET"},
		{EDP_TX_PHY_STATUS,                "EDP_TX_PHY_STATUS"},
		{EDP_TX_AUX_COMMAND,               "EDP_TX_AUX_COMMAND"},
		{EDP_TX_AUX_ADDRESS,               "EDP_TX_AUX_ADDRESS"},
		{EDP_TX_AUX_REPLY_CODE,            "EDP_TX_AUX_REPLY_CODE"},
		{EDP_TX_AUX_REPLY_COUNT,           "EDP_TX_AUX_REPLY_COUNT"},
		{EDP_TX_AUX_REPLY_DATA_COUNT,      "EDP_TX_AUX_REPLY_DATA_COUNT"},
		{EDP_TX_AUX_TRANSFER_STATUS,       "EDP_TX_AUX_TRANSFER_STATUS"},
	};

	len += str_add_reg_sets(pdrv, buf + len, len + offset, LCD_REG_DBG_EDPHOST_BUS, 0,
			edp_reg_sets, ARRAY_SIZE(edp_reg_sets));
	return len;
}
#endif

#ifdef CONFIG_AMLOGIC_LCD_TV
static int lcd_reg_print_tcon(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int n, len = 0;
	struct reg_name_set_s tcon_reg_sets[] = {
		{TCON_TOP_CTRL,     "TCON_TOP_CTRL"},
		{TCON_RGB_IN_MUX,   "TCON_RGB_IN_MUX"},
		{TCON_OUT_CH_SEL0,  "TCON_OUT_CH_SEL0"},
		{TCON_OUT_CH_SEL1,  "TCON_OUT_CH_SEL1"},
		{TCON_STATUS0,      "TCON_STATUS0"},
		{TCON_PLLLOCK_CNTL, "TCON_PLLLOCK_CNTL"},
		{TCON_RST_CTRL,     "TCON_RST_CTRL"},
		{TCON_AXI_OFST0,    "TCON_AXI_OFST0"},
		{TCON_AXI_OFST1,    "TCON_AXI_OFST1"},
		{TCON_AXI_OFST2,    "TCON_AXI_OFST2"},
		{TCON_CLK_CTRL,     "TCON_CLK_CTRL"},
		{TCON_STATUS1,      "TCON_STATUS1"},
		{TCON_DDRIF_CTRL1,  "TCON_DDRIF_CTRL1"},
		{TCON_DDRIF_CTRL2,  "TCON_DDRIF_CTRL2"},
		{TCON_INTR_MASKN,   "TCON_INTR_MASKN"},
		{TCON_INTR_RO,      "TCON_INTR_RO"},
	};

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n, "\ntcon regs:\n");
	len += str_add_reg_sets(pdrv, buf + len, len + offset, LCD_REG_DBG_TCON_BUS, 0,
			tcon_reg_sets, ARRAY_SIZE(tcon_reg_sets));
	return len;
}
#endif

static int lcd_reg_if_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	struct lcd_debug_info_s *lcd_debug_info;
	int n, len = 0;

	lcd_debug_info = (struct lcd_debug_info_s *)pdrv->debug_info;
	if (!lcd_debug_info || !lcd_debug_info->reg_dump_interface) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n, "%s: debug_info is null\n", __func__);
		return len;
	}

	len += lcd_debug_info->reg_dump_interface(pdrv, (buf + len), (len + offset));

	return len;
}

static int lcd_reg_phy_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int len;

	len = lcd_dphy_reg_print(pdrv, buf, offset);
	len += lcd_phy_analog_reg_print(pdrv, (buf + len), (len + offset));

	return len;
}

static int lcd_reg_pinmux_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	struct lcd_debug_info_s *lcd_debug_info;
	int i, n, len = 0;
	unsigned int *table;

	lcd_debug_info = (struct lcd_debug_info_s *)pdrv->debug_info;
	if (!lcd_debug_info) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n, "%s: debug_info is null\n", __func__);
		return len;
	}
	if (!lcd_debug_info->reg_pinmux_table)
		return len;

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n, "\npinmux regs:\n");
	table = lcd_debug_info->reg_pinmux_table;
	i = 0;
	while (i < LCD_DEBUG_REG_CNT_MAX) {
		if (table[i] == LCD_DEBUG_REG_END)
			break;
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n, "PERIPHS_PIN_MUX [0x%02x] = 0x%08x\n",
			table[i], lcd_periphs_read(pdrv, table[i]));
		i++;
	}

	return len;
}

static int lcd_optical_info_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	struct lcd_config_s *pconf;
	int n, len = 0;

	pconf = &pdrv->config;
	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"\nlcd optical info:\n"
		"  hdr_support   %d\n"
		"  features      %d\n"
		"  primaries_r_x %d\n"
		"  primaries_r_y %d\n"
		"  primaries_g_x %d\n"
		"  primaries_g_y %d\n"
		"  primaries_b_x %d\n"
		"  primaries_b_y %d\n"
		"  white_point_x %d\n"
		"  white_point_y %d\n"
		"  luma_max      %d\n"
		"  luma_min      %d\n"
		"  luma_avg      %d\n\n",
		pconf->optical.hdr_support,
		pconf->optical.features,
		pconf->optical.primaries_r_x,
		pconf->optical.primaries_r_y,
		pconf->optical.primaries_g_x,
		pconf->optical.primaries_g_y,
		pconf->optical.primaries_b_x,
		pconf->optical.primaries_b_y,
		pconf->optical.white_point_x,
		pconf->optical.white_point_y,
		pconf->optical.luma_max,
		pconf->optical.luma_min,
		pconf->optical.luma_avg);
	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"adv_val:\n"
		"  ldim_support  %d\n"
		"  luma_peak     %d\n\n",
		pconf->optical.ldim_support,
		pconf->optical.luma_peak);

	return len;
}

unsigned int lcd_prbs_flag = 0, lcd_prbs_freq = 0, lcd_prbs_performed = 0, lcd_prbs_err = 0;

static ssize_t lcd_debug_prbs_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	if ((lcd_prbs_performed & LCD_PRBS_MODE_LVDS) ||
	    (lcd_prbs_performed & LCD_PRBS_MODE_VX1)) {
		len += sprintf(buf + len,
			"lvds performed: %d, error: %d\n"
			"vx1 performed: %d, error: %d\n",
			(lcd_prbs_performed & LCD_PRBS_MODE_LVDS) ? 1 : 0,
			(lcd_prbs_err & LCD_PRBS_MODE_LVDS) ? 1 : 0,
			(lcd_prbs_performed & LCD_PRBS_MODE_VX1) ? 1 : 0,
			(lcd_prbs_err & LCD_PRBS_MODE_VX1) ? 1 : 0);
	}
	if (lcd_prbs_performed & LCD_PRBS_MODE_FREQ) {
		len += sprintf(buf + len, "freq %dMHz performed: %d, error: %d\n",
		       lcd_prbs_freq,
		       (lcd_prbs_performed & LCD_PRBS_MODE_FREQ) ? 1 : 0,
		       (lcd_prbs_err & LCD_PRBS_MODE_FREQ) ? 1 : 0);
	}
	len += sprintf(buf + len, "lcd_prbs_flag: %d\n", lcd_prbs_flag);

	return len;
}

static ssize_t lcd_debug_prbs_store(struct device *dev, struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct lcd_debug_info_s *lcd_debug_info;
	int ret = 0;
	unsigned int test_ms = 0;
	unsigned int prbs_mode_flag;

	lcd_debug_info = (struct lcd_debug_info_s *)pdrv->debug_info;
	if (!lcd_debug_info) {
		LCDPR("%s: debug_info is null\n", __func__);
		return count;
	}

	switch (buf[0]) {
	case 'v': /* vx1 */
		ret = sscanf(buf, "vx1 %d", &test_ms);
		if (ret) {
			prbs_mode_flag = LCD_PRBS_MODE_VX1;
		} else {
			LCDERR("invalid data\n");
			return -EINVAL;
		}
		break;
	case 'l': /* lvds */
		ret = sscanf(buf, "lvds %d", &test_ms);
		if (ret) {
			prbs_mode_flag = LCD_PRBS_MODE_LVDS;
		} else {
			LCDERR("invalid data\n");
			return -EINVAL;
		}
		break;
	case 'f':
		ret = sscanf(buf, "freq %d %d", &lcd_prbs_freq, &test_ms);
		if (ret == 2) {
			prbs_mode_flag = LCD_PRBS_MODE_FREQ;
		} else {
			LCDERR("invalid data\n");
			return -EINVAL;
		}
		break;
	default:
		prbs_mode_flag = LCD_PRBS_MODE_LVDS | LCD_PRBS_MODE_VX1;
		ret = kstrtouint(buf, 10, &test_ms);
		if (ret) {
			LCDERR("invalid data\n");
			return -EINVAL;
		}
		break;
	}

	if (test_ms) {
		if (lcd_prbs_flag) {
			LCDPR("lcd prbs check is already running\n");
			return count;
		}
		lcd_prbs_flag = 1;
		aml_lcd_prbs_test(pdrv, test_ms, prbs_mode_flag);
	} else {
		if (lcd_prbs_flag == 0) {
			LCDPR("lcd prbs check is already stopped\n");
			return count;
		}
		lcd_prbs_flag = 0;
	}

	return count;
}

static void lcd_debug_config_update(struct aml_lcd_drv_s *pdrv)
{

	pdrv->module_reset(pdrv);

	lcd_vinfo_update(pdrv);
}

static void lcd_debug_clk_change(struct aml_lcd_drv_s *pdrv, unsigned int pclk)
{
	struct lcd_config_s *pconf;
	unsigned int sync_duration;

	lcd_vout_notify_mode_change_pre(pdrv);

	pconf = &pdrv->config;
	sync_duration = pclk / pconf->timing.act_timing.h_period;
	sync_duration = sync_duration * 100 / pconf->timing.act_timing.v_period;


	pconf->timing.act_timing.pixel_clk = pclk;
	pconf->timing.act_timing.frame_rate = sync_duration / 100;
	pconf->timing.act_timing.sync_duration_num = sync_duration;
	pconf->timing.act_timing.sync_duration_den = 100;
	pconf->timing.enc_clk = pconf->timing.act_timing.pixel_clk / pconf->timing.ppc;

	if (pdrv->config.timing.ppc > 1) {
		LCDPR("ppc=%d, pixel_clk=%d, enc_clk=%d\n", pdrv->config.timing.ppc,
			pconf->timing.act_timing.pixel_clk, pconf->timing.enc_clk);
	}

	/* update vinfo */
	pdrv->vinfo.sync_duration_num = sync_duration;
	pdrv->vinfo.sync_duration_den = 100;
	pdrv->vinfo.std_duration = sync_duration / 100;
	pdrv->vinfo.video_clk = pconf->timing.enc_clk;

	lcd_clk_generate_parameter(pdrv);

	if (pdrv->config.basic.lcd_type == LCD_VBYONE)
		lcd_vbyone_interrupt_enable(pdrv, 0);
	lcd_set_clk(pdrv);
	if (pdrv->config.basic.lcd_type == LCD_VBYONE)
		lcd_vbyone_wait_stable(pdrv);

	lcd_vout_notify_mode_change(pdrv);
}

static ssize_t lcd_debug_store(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	int i, ret = 0;
	unsigned int temp, val[6];
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct lcd_config_s *pconf;
	struct lcd_detail_timing_s *ptiming;
	char *print_buf;
	unsigned long flags = 0;

	pconf = &pdrv->config;
	ptiming = &pconf->timing.act_timing;
	switch (buf[0]) {
	case 'c':
		if (buf[1] == 'l') { /* clk */
			ret = sscanf(buf, "clk %d", &temp);
			if (ret == 1) {
				if (temp > 500) {
					pr_info("set clk: %dHz\n", temp);
				} else {
					pr_info("set frame_rate: %dHz\n", temp);
					temp = ptiming->h_period * ptiming->v_period * temp;
					pr_info("set clk: %dHz\n", temp);
				}
				lcd_debug_clk_change(pdrv, temp);
			} else {
				LCDERR("invalid data\n");
				return -EINVAL;
			}
		} else if (buf[1] == 'h') { /* check */
			ret = lcd_config_timing_check(pdrv, ptiming);
			if (ret == 0)
				pr_info("lcd config_timing_check: PASS\n");
			pr_info("disp_tmg_min_req:\n"
				"  alert_lvl  %d\n"
				"  hswbp  %d\n"
				"  hfp    %d\n"
				"  vswbp  %d\n"
				"  vfp    %d\n\n",
				pdrv->disp_req.alert_level,
				pdrv->disp_req.hswbp_vid, pdrv->disp_req.hfp_vid,
				pdrv->disp_req.vswbp_vid, pdrv->disp_req.vfp_vid);
			if (pconf->basic.lcd_type == LCD_MLVDS ||
			    pconf->basic.lcd_type == LCD_P2P) {
				lcd_tcon_dbg_check(pdrv, ptiming);
			}
			pr_info("config_check_glb: %d, config_check: 0x%x, config_check_en: %d\n\n",
				pdrv->config_check_glb, pconf->basic.config_check,
				pdrv->config_check_en);
		}
		break;
	case 'b':
		if (buf[1] == 'a') { /* basic */
			ret = sscanf(buf, "basic %d %d %d %d %d",
				     &val[0], &val[1], &val[2], &val[3], &val[4]);
			if (ret == 4) {
				ptiming->h_active = val[0];
				ptiming->v_active = val[1];
				ptiming->h_period = val[2];
				ptiming->v_period = val[3];
				pr_info("set h_active=%d, v_active=%d\n", val[0], val[1]);
				pr_info("set h_period=%d, v_period=%d\n", val[2], val[3]);
				lcd_enc_timing_init_config(pdrv);
				lcd_debug_config_update(pdrv);
			} else if (ret == 5) {
				ptiming->h_active = val[0];
				ptiming->v_active = val[1];
				ptiming->h_period = val[2];
				ptiming->v_period = val[3];
				ptiming->lcd_bits = val[4] * 3;
				pr_info("set h_active=%d, v_active=%d\n", val[0], val[1]);
				pr_info("set h_period=%d, v_period=%d\n", val[2], val[3]);
				pr_info("set lcd_bits=%d\n", val[4]);
				lcd_enc_timing_init_config(pdrv);
				lcd_debug_config_update(pdrv);
			} else {
				LCDERR("invalid data\n");
				return -EINVAL;
			}
		} else if (buf[1] == 'i') { /* bit */
			ret = sscanf(buf, "bit %d", &val[0]);
			if (ret == 1) {
				ptiming->lcd_bits = val[0] * 3;
				pr_info("set lcd_bits=%d\n", val[0]);
				lcd_debug_config_update(pdrv);
			} else {
				LCDERR("invalid data\n");
				return -EINVAL;
			}
		}
		break;
	case 's': /* sync */
		ret = sscanf(buf, "sync %d %d %d %d %d %d",
			     &val[0], &val[1], &val[2], &val[3], &val[4], &val[5]);
		if (ret == 6) {
			ptiming->hsync_width = val[0];
			ptiming->hsync_bp =    val[1];
			ptiming->hsync_pol =   val[2];
			ptiming->vsync_width = val[3];
			ptiming->vsync_bp =    val[4];
			ptiming->vsync_pol =   val[5];
			ptiming->hsync_fp = ptiming->h_period - ptiming->h_active -
					ptiming->hsync_width - ptiming->hsync_bp;
			ptiming->vsync_fp = ptiming->v_period - ptiming->v_active -
					ptiming->vsync_width - ptiming->vsync_bp;
			pr_info("set hsync width=%d, bp=%d, pol=%d\n", val[0], val[1], val[2]);
			pr_info("set vsync width=%d, bp=%d, pol=%d\n", val[3], val[4], val[5]);
			lcd_enc_timing_init_config(pdrv);
			lcd_debug_config_update(pdrv);
		} else {
			LCDERR("invalid data\n");
			return -EINVAL;
		}
		break;
	case 't': /* test */
		ret = sscanf(buf, "test %d", &temp);
		if (ret == 1) {
			spin_lock_irqsave(&pdrv->isr_lock, flags);
			pdrv->test_flag = (unsigned char)temp;
			spin_unlock_irqrestore(&pdrv->isr_lock, flags);
			LCDPR("%s: test %d\n", __func__, temp);
			i = 0;
			while (i++ < 5000) {
				if (pdrv->test_state == temp)
					break;
				usleep_range(20, 30);
			}
		} else {
			LCDERR("invalid data\n");
			return -EINVAL;
		}
		break;
	case 'i': /* info */
		print_buf = kcalloc(PR_BUF_MAX, sizeof(char), GFP_KERNEL);
		if (!print_buf) {
			LCDERR("%s: buf malloc error\n", __func__);
			return -EINVAL;
		}
		lcd_info_basic_print(pdrv, print_buf, 0);
		lcd_debug_info_print(print_buf);
		memset(print_buf, 0, PR_BUF_MAX);
		lcd_info_adv_print(pdrv, print_buf, 0);
		lcd_debug_info_print(print_buf);
		memset(print_buf, 0, PR_BUF_MAX);
		lcd_info_tcon_print(pdrv, print_buf, 0);
		lcd_debug_info_print(print_buf);
		memset(print_buf, 0, PR_BUF_MAX);
		lcd_power_step_info_print(pdrv, print_buf, 0);
		lcd_debug_info_print(print_buf);
		kfree(print_buf);
		break;
	case 'r':
		if (buf[2] == 'g') { /* reg */
			print_buf = kcalloc(PR_BUF_MAX, sizeof(char), GFP_KERNEL);
			if (!print_buf) {
				LCDERR("%s: buf malloc error\n", __func__);
				return -EINVAL;
			}
			lcd_clk_reg_print(pdrv, print_buf, 0);
			lcd_debug_info_print(print_buf);
			memset(print_buf, 0, PR_BUF_MAX);
			lcd_venc_reg_print(pdrv, print_buf, 0);
			lcd_debug_info_print(print_buf);
			memset(print_buf, 0, PR_BUF_MAX);
			lcd_reg_if_print(pdrv, print_buf, 0);
			lcd_debug_info_print(print_buf);
			memset(print_buf, 0, PR_BUF_MAX);
			lcd_reg_phy_print(pdrv, print_buf, 0);
			lcd_debug_info_print(print_buf);
			memset(print_buf, 0, PR_BUF_MAX);
			lcd_reg_pinmux_print(pdrv, print_buf, 0);
			lcd_debug_info_print(print_buf);
			kfree(print_buf);
		} else if (buf[2] == 's') { /* reset */
			pdrv->module_reset(pdrv);
		} else if (buf[2] == 'n') { /* range */
			ret = sscanf(buf, "range %d %d %d %d %d %d",
				     &val[0], &val[1], &val[2], &val[3], &val[4], &val[5]);
			if (ret == 6) {
				ptiming->h_period_min = val[0];
				ptiming->h_period_max = val[1];
				ptiming->h_period_min = val[2];
				ptiming->v_period_max = val[3];
				ptiming->pclk_min  = val[4];
				ptiming->pclk_max  = val[5];
				pr_info("set h_period min=%d, max=%d\n",
					ptiming->h_period_min,
					ptiming->h_period_max);
				pr_info("set v_period min=%d, max=%d\n",
					ptiming->v_period_min,
					ptiming->v_period_max);
				pr_info("set pclk min=%d, max=%d\n",
					ptiming->pclk_min,
					ptiming->pclk_max);
				lcd_enc_timing_init_config(pdrv);
			} else {
				LCDERR("invalid data\n");
				return -EINVAL;
			}
		}
		break;
	case 'd': /* dump */
		print_buf = kcalloc(PR_BUF_MAX, sizeof(char), GFP_KERNEL);
		if (!print_buf) {
			LCDERR("%s: buf malloc error\n", __func__);
			return -EINVAL;
		}
		lcd_info_basic_print(pdrv, print_buf, 0);
		lcd_debug_info_print(print_buf);
		memset(print_buf, 0, PR_BUF_MAX);
		lcd_info_adv_print(pdrv, print_buf, 0);
		lcd_debug_info_print(print_buf);
		memset(print_buf, 0, PR_BUF_MAX);
		lcd_info_tcon_print(pdrv, print_buf, 0);
		lcd_debug_info_print(print_buf);
		memset(print_buf, 0, PR_BUF_MAX);
		lcd_power_step_info_print(pdrv, print_buf, 0);
		lcd_debug_info_print(print_buf);
		memset(print_buf, 0, PR_BUF_MAX);
		lcd_clk_reg_print(pdrv, print_buf, 0);
		lcd_debug_info_print(print_buf);
		memset(print_buf, 0, PR_BUF_MAX);
		lcd_venc_reg_print(pdrv, print_buf, 0);
		lcd_debug_info_print(print_buf);
		memset(print_buf, 0, PR_BUF_MAX);
		lcd_reg_if_print(pdrv, print_buf, 0);
		lcd_debug_info_print(print_buf);
		memset(print_buf, 0, PR_BUF_MAX);
		lcd_reg_phy_print(pdrv, print_buf, 0);
		lcd_debug_info_print(print_buf);
		memset(print_buf, 0, PR_BUF_MAX);
		lcd_reg_pinmux_print(pdrv, print_buf, 0);
		lcd_debug_info_print(print_buf);
		//memset(print_buf, 0, PR_BUF_MAX);
		//lcd_optical_info_print(pdrv, print_buf, 0);
		//lcd_debug_info_print(print_buf);
		i = lcd_clk_config_print(pdrv, print_buf, 0);
		lcd_clk_clkmsr_print(pdrv, (print_buf + i), i);
		lcd_debug_info_print(print_buf);
		kfree(print_buf);
		break;
	case 'p': /*parse*/
		dump_panel_file_parse_mem(pdrv->index);
		break;
	case 'k': /* key */
		LCDPR("key_valid: %d, config_load: %d\n", pdrv->key_valid, pdrv->config_load);
		if (pdrv->key_valid)
			lcd_unifykey_print(pdrv->index);
		break;
	case 'h': /* hdr */
		print_buf = kcalloc(PR_BUF_MAX, sizeof(char), GFP_KERNEL);
		if (!print_buf) {
			LCDERR("%s: buf malloc error\n", __func__);
			return -EINVAL;
		}
		lcd_optical_info_print(pdrv, print_buf, 0);
		lcd_debug_info_print(print_buf);
		kfree(print_buf);
		break;
	case 'v':
		ret = sscanf(buf, "vout %d", &temp);
		if (ret == 1) {
			LCDPR("vout_serve bypass: %d\n", temp);
			lcd_vout_serve_bypass = temp;
		} else {
			LCDERR("invalid data\n");
			return -EINVAL;
		}
		break;
	case 'g':
		ret = sscanf(buf, "gamma %d", &temp);
		if (ret == 1) {
			LCDPR("gamma en: %d\n", temp);
			lcd_gamma_debug_test_en(pdrv, temp);
		} else {
			LCDERR("invalid data\n");
			return -EINVAL;
		}
		break;
	default:
		LCDERR("wrong command\n");
		break;
	}
	return count;
}

static ssize_t lcd_debug_change_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", lcd_debug_change_usage_str);
}

static void lcd_debug_change_clk_change(struct aml_lcd_drv_s *pdrv, unsigned int pclk)
{
	struct lcd_config_s *pconf = &pdrv->config;
	unsigned int sync_duration;
	struct lcd_detail_timing_s *ptiming;

	ptiming = &pconf->timing.act_timing;
	sync_duration = pclk / ptiming->h_period;
	sync_duration = sync_duration * 100 / ptiming->v_period;

	pconf->timing.act_timing.pixel_clk = pclk;
	pconf->timing.act_timing.frame_rate = sync_duration / 100;
	pconf->timing.act_timing.sync_duration_num = sync_duration;
	pconf->timing.act_timing.sync_duration_den = 100;
	pconf->timing.enc_clk = pconf->timing.act_timing.pixel_clk / pconf->timing.ppc;
	if (pdrv->config.timing.ppc > 1) {
		LCDPR("ppc=%d, pixel_clk=%d, enc_clk=%d\n",
		      pdrv->config.timing.ppc, pconf->timing.act_timing.pixel_clk,
		      pconf->timing.enc_clk);
	}

	/* update vinfo */
	pdrv->vinfo.sync_duration_num = sync_duration;
	pdrv->vinfo.sync_duration_den = 100;
	pdrv->vinfo.std_duration = sync_duration / 100;
	pdrv->vinfo.video_clk = pconf->timing.enc_clk;

	lcd_clk_generate_parameter(pdrv);
}

static ssize_t lcd_debug_change_store(struct device *dev, struct device_attribute *attr,
				      const char *buf, size_t count)
{
	int ret = 0;
	unsigned int temp, val[10];
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct lcd_config_s *pconf;
	union lcd_ctrl_config_u *pctrl;
	struct lcd_detail_timing_s *ptiming;

	pconf = &pdrv->config;
	pctrl = &pconf->control;

	switch (buf[0]) {
	case 'c': /* clk */
		ret = sscanf(buf, "clk %d", &temp);
		if (ret == 1) {
			if (temp > 500) {
				pr_info("change clk=%dHz\n", temp);
			} else {
				pr_info("change frame_rate=%dHz\n", temp);
				temp = pconf->timing.act_timing.h_period *
					pconf->timing.act_timing.v_period * temp;
				pr_info("change clk=%dHz\n", temp);
			}
			lcd_debug_change_clk_change(pdrv, temp);
			pconf->change_flag = 1;
		} else {
			LCDERR("invalid data\n");
			return -EINVAL;
		}
		break;
	case 'b':
		if (buf[1] == 'a') { /* basic */
			ret = sscanf(buf, "basic %d %d %d %d %d",
				     &val[0], &val[1], &val[2], &val[3], &val[4]);
			ptiming = &pconf->timing.act_timing;
			if (ret == 4) {
				ptiming->h_active = val[0];
				ptiming->v_active = val[1];
				ptiming->h_period = val[2];
				ptiming->v_period = val[3];
				pr_info("change h_active=%d, v_active=%d\n", val[0], val[1]);
				pr_info("change h_period=%d, v_period=%d\n", val[2], val[3]);
				lcd_enc_timing_init_config(pdrv);
				pconf->change_flag = 1;
			} else if (ret == 5) {
				ptiming->h_active = val[0];
				ptiming->v_active = val[1];
				ptiming->h_period = val[2];
				ptiming->v_period = val[3];
				ptiming->lcd_bits = val[4] * 3;
				pr_info("change h_active=%d, v_active=%d\n", val[0], val[1]);
				pr_info("change h_period=%d, v_period=%d\n", val[2], val[3]);
				pr_info("change lcd_bits=%d\n", val[4]);
				lcd_enc_timing_init_config(pdrv);
				pconf->change_flag = 1;
			} else {
				LCDERR("invalid data\n");
				return -EINVAL;
			}
		} else if (buf[1] == 'i') { /* bit */
			ret = sscanf(buf, "bit %d", &val[0]);
			if (ret == 1) {
				pconf->timing.act_timing.lcd_bits = val[4] * 3;
				pr_info("change lcd_bits=%d\n", val[4]);
				pconf->change_flag = 1;
			} else {
				LCDERR("invalid data\n");
				return -EINVAL;
			}
		}
		break;
	case 's':
		if (buf[1] == 'e') { /* set */
			if (pconf->change_flag) {
				LCDPR("apply config changing\n");
				lcd_debug_config_update(pdrv);
			} else {
				LCDPR("config is no changing\n");
			}
		} else if (buf[1] == 'y') { /* sync */
			ret = sscanf(buf, "sync %d %d %d %d %d %d",
				     &val[0], &val[1], &val[2], &val[3], &val[4], &val[5]);
			if (ret == 6) {
				ptiming = &pconf->timing.act_timing;
				ptiming->hsync_width = val[0];
				ptiming->hsync_bp =    val[1];
				ptiming->hsync_pol =   val[2];
				ptiming->vsync_width = val[3];
				ptiming->vsync_bp =    val[4];
				ptiming->vsync_pol =   val[5];
				ptiming->hsync_fp = ptiming->h_period - ptiming->h_active -
						ptiming->hsync_width - ptiming->hsync_bp;
				ptiming->vsync_fp = ptiming->v_period - ptiming->v_active -
						ptiming->vsync_width - ptiming->vsync_bp;
				pr_info("change hs width=%d, bp=%d, pol=%d\n",
					val[0], val[1], val[2]);
				pr_info("change vs width=%d, bp=%d, pol=%d\n",
					val[3], val[4], val[5]);
				lcd_enc_timing_init_config(pdrv);
				pconf->change_flag = 1;
			} else {
				LCDERR("invalid data\n");
				return -EINVAL;
			}
		}
		break;
	case 'r':
		ret = sscanf(buf, "rgb %d %d %d %d %d %d",
			     &val[0], &val[1], &val[2], &val[3], &val[4], &val[5]);
		ptiming = &pconf->timing.act_timing;
		if (ret == 5) {
			pctrl->rgb_cfg.type = val[0];
			pctrl->rgb_cfg.clk_pol = val[1];
			pctrl->rgb_cfg.de_valid = val[2];
			pctrl->rgb_cfg.sync_valid = val[3];
			pctrl->rgb_cfg.rb_swap = val[4];
			pctrl->rgb_cfg.bit_swap = val[5];
			pr_info("set rgb config:\n"
	"type=%d, clk_pol=%d, de_valid=%d, sync_valid=%d, rb_swap=%d, bit_swap=%d\n",
				val[0], val[1], val[2], val[3], val[4], val[5]);
			lcd_debug_change_clk_change(pdrv, ptiming->pixel_clk);
			pconf->change_flag = 1;
		} else {
			LCDERR("invalid data\n");
			return -EINVAL;
		}
		break;
	case 'l':
		ret = sscanf(buf, "lvds %d %d %d %d %d",
			     &val[0], &val[1], &val[2], &val[3], &val[4]);
		if (ret == 5) {
			pctrl->lvds_cfg.lvds_repack = val[0];
			pctrl->lvds_cfg.dual_port = val[1];
			pctrl->lvds_cfg.pn_swap = val[2];
			pctrl->lvds_cfg.port_swap = val[3];
			pctrl->lvds_cfg.lane_reverse = val[4];
			pr_info("set lvds config:\n"
	"repack=%d, dual_port=%d, pn_swap=%d, port_swap=%d, lane_reverse=%d\n",
				pctrl->lvds_cfg.lvds_repack,
				pctrl->lvds_cfg.dual_port,
				pctrl->lvds_cfg.pn_swap,
				pctrl->lvds_cfg.port_swap,
				pctrl->lvds_cfg.lane_reverse);
			ptiming = &pconf->timing.act_timing;
			lcd_debug_change_clk_change(pdrv, ptiming->pixel_clk);
			pconf->change_flag = 1;
		} else if (ret == 4) {
			pctrl->lvds_cfg.lvds_repack = val[0];
			pctrl->lvds_cfg.dual_port = val[1];
			pctrl->lvds_cfg.pn_swap = val[2];
			pctrl->lvds_cfg.port_swap = val[3];
			pr_info("set lvds config:\n"
				"repack=%d, dual_port=%d, pn_swap=%d, port_swap=%d\n",
				pctrl->lvds_cfg.lvds_repack,
				pctrl->lvds_cfg.dual_port,
				pctrl->lvds_cfg.pn_swap,
				pctrl->lvds_cfg.port_swap);
			ptiming = &pconf->timing.act_timing;
			lcd_debug_change_clk_change(pdrv, ptiming->pixel_clk);
			pconf->change_flag = 1;
		} else {
			LCDERR("invalid data\n");
			return -EINVAL;
		}
		break;
	case 'v':
		ret = sscanf(buf, "vbyone %d %d %d %d",
			     &val[0], &val[1], &val[2], &val[3]);
		if (ret == 4 || ret == 3) {
			pctrl->vbyone_cfg.lane_count = val[0];
			pctrl->vbyone_cfg.region_num = val[1];
			pctrl->vbyone_cfg.byte_mode = val[2];
			pr_info("set vbyone config:\n"
				"lane_count=%d, region_num=%d, byte_mode=%d\n",
				pctrl->vbyone_cfg.lane_count,
				pctrl->vbyone_cfg.region_num,
				pctrl->vbyone_cfg.byte_mode);
			ptiming = &pconf->timing.act_timing;
			lcd_debug_change_clk_change(pdrv, ptiming->pixel_clk);
			pconf->change_flag = 1;
		} else {
			LCDERR("invalid data\n");
			return -EINVAL;
		}
		break;
	case 'm':
		if (buf[1] == 'i') {
			ret = sscanf(buf, "mipi %d %d %d %d %d %d %d %d",
				     &val[0], &val[1], &val[2], &val[3],
				     &val[4], &val[5], &val[6], &val[7]);
			if (ret == 8) {
				pctrl->mipi_cfg.lane_num = (unsigned char)val[0];
				pctrl->mipi_cfg.bit_rate_max = val[1];
				pctrl->mipi_cfg.operation_mode_init = (unsigned char)val[3];
				pctrl->mipi_cfg.operation_mode_display = (unsigned char)val[4];
				pctrl->mipi_cfg.video_mode_type = (unsigned char)val[5];
				pctrl->mipi_cfg.clk_always_hs = (unsigned char)val[6];
				pr_info("change mipi_dsi config:\n"
					"lane_num=%d, bit_rate_max=%dMhz\n"
					"operation_mode_init=%d, operation_mode_display=%d\n"
					"video_mode_type=%d, clk_always_hs=%d\n",
					pctrl->mipi_cfg.lane_num,
					pctrl->mipi_cfg.bit_rate_max,
					pctrl->mipi_cfg.operation_mode_init,
					pctrl->mipi_cfg.operation_mode_display,
					pctrl->mipi_cfg.video_mode_type,
					pctrl->mipi_cfg.clk_always_hs);
				ptiming = &pconf->timing.act_timing;
				lcd_debug_change_clk_change(pdrv, ptiming->pixel_clk);

				pconf->change_flag = 1;
			} else {
				LCDERR("invalid data\n");
				return -EINVAL;
			}
		} else if (buf[1] == 'l') {
			ret = sscanf(buf, "mlvds %d %x %x %x %d %d",
				     &val[0], &val[1], &val[2], &val[3],
				     &val[4], &val[5]);
			if (ret == 6) {
				pctrl->mlvds_cfg.channel_num = val[0];
				pctrl->mlvds_cfg.channel_sel0 = val[1];
				pctrl->mlvds_cfg.channel_sel1 = val[2];
				pctrl->mlvds_cfg.clk_phase = val[3];
				pctrl->mlvds_cfg.pn_swap = val[4];
				pctrl->mlvds_cfg.bit_swap = val[5];
				pr_info("change mlvds config:\n"
					"channel_num=%d,\n"
					"channel_sel0=0x%08x, channel_sel1=0x%08x,\n"
					"clk_phase=0x%04x,\n"
					"pn_swap=%d, bit_swap=%d\n",
					pctrl->mlvds_cfg.channel_num,
					pctrl->mlvds_cfg.channel_sel0,
					pctrl->mlvds_cfg.channel_sel1,
					pctrl->mlvds_cfg.clk_phase,
					pctrl->mlvds_cfg.pn_swap,
					pctrl->mlvds_cfg.bit_swap);
				pdrv->config.phy_cfg.bypass_resample =
							(pctrl->mlvds_cfg.clk_phase >> 12) & 1;
				pdrv->config.phy_cfg.act_phy->clk_phase =
							pctrl->mlvds_cfg.clk_phase & 0xfff;
				lcd_mlvds_phy_ckdi_config(pdrv);
				ptiming = &pconf->timing.act_timing;
				lcd_debug_change_clk_change(pdrv, ptiming->pixel_clk);

				pconf->change_flag = 1;
			} else {
				LCDERR("invalid data\n");
				return -EINVAL;
			}
		}
		break;
	case 'p':
		ret = sscanf(buf, "p2p %x %d %x %x %d %d",
			     &val[0], &val[1], &val[2], &val[3], &val[4], &val[5]);
		if (ret == 6) {
			pctrl->p2p_cfg.p2p_type = val[0];
			pctrl->p2p_cfg.lane_num = val[1];
			pctrl->p2p_cfg.channel_sel0 = val[2];
			pctrl->p2p_cfg.channel_sel1 = val[3];
			pctrl->p2p_cfg.pn_swap = val[4];
			pctrl->p2p_cfg.bit_swap = val[5];
			pr_info("change p2p config:\n"
				"p2p_type=0x%x, lane_num=%d,\n"
				"channel_sel0=0x%08x, channel_sel1=0x%08x,\n"
				"pn_swap=%d, bit_swap=%d\n",
				pctrl->p2p_cfg.p2p_type,
				pctrl->p2p_cfg.lane_num,
				pctrl->p2p_cfg.channel_sel0,
				pctrl->p2p_cfg.channel_sel1,
				pctrl->p2p_cfg.pn_swap,
				pctrl->p2p_cfg.bit_swap);
			ptiming = &pconf->timing.act_timing;
			lcd_debug_change_clk_change(pdrv, ptiming->pixel_clk);

			pconf->change_flag = 1;
		} else {
			LCDERR("invalid data\n");
			return -EINVAL;
		}
		break;
	case 'u': /* update */
		if (pconf->change_flag) {
			LCDPR("apply config changing\n");
			lcd_debug_config_update(pdrv);
		} else {
			LCDPR("config is no changing\n");
		}
		break;
	default:
		LCDERR("wrong command\n");
		break;
	}

	return count;
}

static ssize_t lcd_debug_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);

	return sprintf(buf, "lcd_status: 0x%x\n", pdrv->status);
}

static ssize_t lcd_debug_enable_store(struct device *dev, struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	int ret = 0;
	unsigned int temp = 1;

	ret = kstrtouint(buf, 10, &temp);
	if (ret) {
		LCDERR("invalid data\n");
		return -EINVAL;
	}
	if (temp) {
		mutex_lock(&lcd_power_mutex);
		aml_lcd_notifier_call_chain(LCD_EVENT_ENABLE, (void *)pdrv);
		lcd_if_enable_retry(pdrv);
		pdrv->status |= (LCD_STATUS_PREPARE | LCD_STATUS_POWER);
		mutex_unlock(&lcd_power_mutex);
	} else {
		mutex_lock(&lcd_power_mutex);
		lcd_proc_time_clear(pdrv);
		pdrv->status &= ~(LCD_STATUS_PREPARE | LCD_STATUS_POWER);
		aml_lcd_notifier_call_chain(LCD_EVENT_DISABLE, (void *)pdrv);
		mutex_unlock(&lcd_power_mutex);
	}

	return count;
}

static ssize_t lcd_debug_resume_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);

	return sprintf(buf, "lcd resume type: %d(%s)\n",
		pdrv->resume_type, pdrv->resume_type ? "workqueue" : "directly");
}

static ssize_t lcd_debug_resume_store(struct device *dev, struct device_attribute *attr,
				      const char *buf, size_t count)
{
	int ret = 0;
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	unsigned int temp = 1;

	ret = kstrtouint(buf, 10, &temp);
	if (ret) {
		LCDERR("invalid data\n");
		return -EINVAL;
	}
	pdrv->resume_type = (unsigned char)temp;
	LCDPR("set lcd resume flag: %d\n", pdrv->resume_type);

	return count;
}

static int lcd_debug_power_type;
static ssize_t lcd_debug_power_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	unsigned int state;

	if ((pdrv->status & LCD_STATUS_ON) == 0) {
		state = 0;
	} else {
		if (pdrv->status & LCD_STATUS_IF_ON)
			state = 1;
		else
			state = 0;
	}

	if (lcd_debug_power_type)
		return sprintf(buf, "for_tool:%u\n", state);
	else
		return sprintf(buf, "lcd power state: %u\n", state);
}

static ssize_t lcd_debug_power_store(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	int ret = 0;
	unsigned int temp = 1;

	if (buf[0] == 'o') {
		if (buf[1] == 'n') { //on
			lcd_debug_power_type = 1; // for tool
			temp = 1;
			goto lcd_debug_power_store_next;
		}
		if (buf[1] == 'f') { //off
			lcd_debug_power_type = 1; // for tool
			temp = 0;
			goto lcd_debug_power_store_next;
		}
	} else if (buf[0] == 's') { //state
		lcd_debug_power_type = 1; // for tool
		return count;
	}

	lcd_debug_power_type = 0;
	ret = kstrtouint(buf, 10, &temp);
	if (ret) {
		LCDERR("invalid data\n");
		return -EINVAL;
	}

lcd_debug_power_store_next:
	mutex_lock(&lcd_power_mutex);
	LCDPR("[%d]: %s: %d\n", pdrv->index, __func__, temp);
	if (temp) {
		if (pdrv->status & LCD_STATUS_ENCL_ON) {
			aml_lcd_notifier_call_chain(LCD_EVENT_POWER_ON, (void *)pdrv);
			lcd_if_enable_retry(pdrv);
			pdrv->status |= LCD_STATUS_POWER;
		} else {
			LCDERR("%s: can't power on when driver disable\n", __func__);
		}
	} else {
		pdrv->status &= ~LCD_STATUS_POWER;
		aml_lcd_notifier_call_chain(LCD_EVENT_POWER_OFF, (void *)pdrv);
	}
	mutex_unlock(&lcd_power_mutex);

	return count;
}

static ssize_t lcd_debug_power_step_show(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	char *print_buf;
	int n = 0;

	print_buf = kcalloc(PR_BUF_MAX, sizeof(char), GFP_KERNEL);
	if (!print_buf)
		return sprintf(buf, "%s: buf malloc error\n", __func__);

	lcd_power_step_info_print(pdrv, print_buf, 0);

	n = sprintf(buf, "%s\n", print_buf);
	kfree(print_buf);

	return n;
}

static ssize_t lcd_debug_power_step_store(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t count)
{
	int ret = 0;
	unsigned int i;
	unsigned int tmp[2];
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct lcd_power_ctrl_s *power_step;

	power_step = &pdrv->config.power;
	switch (buf[1]) {
	case 'n': /* on */
		ret = sscanf(buf, "on %d %d %d", &i, &tmp[0], &tmp[1]);
		if (ret == 3) {
			if (i >= power_step->power_on_step_max) {
				pr_info("invalid power_on step: %d, step_max: %d\n",
					i, power_step->power_on_step_max);
				return -EINVAL;
			}
			power_step->power_on_step[i].value = tmp[0];
			power_step->power_on_step[i].delay = tmp[1];
			pr_info("set power_on step %d value %d delay: %dms\n", i, tmp[0], tmp[1]);
		} else if (ret == 2) {
			if (i >= power_step->power_on_step_max) {
				pr_info("invalid power_on step: %d\n", i);
				return -EINVAL;
			}
			power_step->power_on_step[i].delay = tmp[0];
			pr_info("set power_on step %d delay: %dms\n",
				i, tmp[0]);
		} else {
			pr_info("invalid data\n");
			return -EINVAL;
		}
		break;
	case 'f': /* off */
		ret = sscanf(buf, "off %d %d %d\n", &i, &tmp[0], &tmp[1]);
		if (ret == 3) {
			if (i >= power_step->power_off_step_max) {
				pr_info("invalid power_off step: %d\n", i);
				return -EINVAL;
			}
			power_step->power_off_step[i].value = tmp[0];
			power_step->power_off_step[i].delay = tmp[1];
			pr_info("set power_off step %d value %d delay: %dms\n", i, tmp[0], tmp[1]);
		} else if (ret == 2) {
			if (i >= power_step->power_off_step_max) {
				pr_info("invalid power_off step: %d\n", i);
				return -EINVAL;
			}
			power_step->power_off_step[i].delay = tmp[0];
			pr_info("set power_off step %d delay: %dms\n", i, tmp[0]);
		} else {
			pr_info("invalid data\n");
			return -EINVAL;
		}
		break;
	default:
		pr_info("wrong command\n");
		break;
	}

	return count;
}

static ssize_t lcd_debug_frame_rate_show(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	unsigned int sync_duration;
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct lcd_detail_timing_s *ptiming;

	ptiming = &pdrv->config.timing.act_timing;
	sync_duration = ptiming->sync_duration_num * 100;
	sync_duration = sync_duration / ptiming->sync_duration_den;

	return sprintf(buf, "get frame_rate: %u.%02uHz, fr_adjust_type: %d\n",
		(sync_duration / 100), (sync_duration % 100), ptiming->fr_adjust_type);
}

static ssize_t lcd_debug_frame_rate_store(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t count)
{
	int ret = 0;
	unsigned int temp = 0;
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);

	switch (buf[0]) {
	case 't':
		ret = sscanf(buf, "type %d", &temp);
		if (ret == 1) {
			pdrv->config.timing.act_timing.fr_adjust_type = temp;
			pr_info("set fr_adjust_type: %d\n", temp);
		} else {
			pr_info("invalid data\n");
			return -EINVAL;
		}
		break;
	case 's':
		ret = sscanf(buf, "set %d", &temp);
		if (ret == 1) {
			pr_info("set frame rate(*100): %d\n", temp);
			pdrv->fr_adjust(pdrv, temp);
		} else {
			pr_info("invalid data\n");
			return -EINVAL;
		}
		break;
	default:
		pr_info("wrong command\n");
		break;
	}

	return count;
}

static ssize_t lcd_debug_fr_flag_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);

	return sprintf(buf, "fr_auto_flag: 0x%x\n", pdrv->config.fr_auto_flag);
}

static ssize_t lcd_debug_fr_flag_store(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	unsigned int temp = 0;
	int ret = 0;

	ret = kstrtouint(buf, 16, &temp);
	if (ret) {
		pr_info("invalid data\n");
		return -EINVAL;
	}
	pdrv->config.fr_auto_flag = temp;
	pr_info("set fr_auto_flag: 0x%x\n", temp);

	return count;
}

#define SSC_DEBUG_INFO     0
#define SSC_DEBUG_MODE     1
#define SSC_DEBUG_FREQ     2
#define SSC_DEBUG_LEVEL    3
#define SSC_DEBUG_EN       4
#define SSC_DEBUG_UNKNOWN  5
#define SSC_DEBUG_ERR      6
static unsigned int ssc_debug_type;

static ssize_t lcd_debug_ss_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	unsigned int n = 0, level, ppm, freq, mode;
	int ret;

	ret = lcd_get_ss_num(pdrv, &level, &ppm, &freq, &mode);
	if (ret < 0)
		return sprintf(buf, "get ss error\n");

	switch (ssc_debug_type) {
	case SSC_DEBUG_MODE:
		n = sprintf(buf, "for_tool:%u\n", mode);
		break;
	case SSC_DEBUG_FREQ:
		n = sprintf(buf, "for_tool:%u\n", freq);
		break;
	case SSC_DEBUG_LEVEL:
		n = sprintf(buf, "for_tool:%u, %dppm\n", level, ppm);
		break;
	case SSC_DEBUG_EN:
		n = sprintf(buf, "for_tool:%u\n", ret);
		break;
	case SSC_DEBUG_UNKNOWN:
		n = sprintf(buf, "for_tool:unknown cmd\n");
		break;
	case SSC_DEBUG_ERR:
		n = sprintf(buf, "for_tool:error\n");
		break;
	case SSC_DEBUG_INFO:
	default:
		n = lcd_get_ss(pdrv, buf);
		break;
	}

	return n;
}

static ssize_t lcd_debug_ss_store(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	unsigned char set_val = 0xff;
	char *buf_orig;
	char **parm = NULL;
	int ret;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return count;
	parm = kcalloc(4, sizeof(char *), GFP_KERNEL);
	if (!parm) {
		kfree(buf_orig);
		return count;
	}

	lcd_debug_parse_param(buf_orig, parm, 4);

	if (strcmp(parm[0], "info") == 0) {
		ssc_debug_type = SSC_DEBUG_INFO;
		goto lcd_ss_debug_store_end;
	} else if (strcmp(parm[0], "en") == 0) {
		ssc_debug_type = SSC_DEBUG_EN;
		if (!parm[1])
			goto lcd_ss_debug_store_end;
		if (kstrtou8(parm[1], 10, &set_val))
			goto lcd_ss_debug_store_err;
		ret = lcd_ss_enable(pdrv->index, set_val);
		if (ret)
			goto lcd_ss_debug_store_err;
	} else if (strcmp(parm[0], "level") == 0) {
		ssc_debug_type = SSC_DEBUG_LEVEL;
		if (!parm[1])
			goto lcd_ss_debug_store_end;
		if (kstrtou8(parm[1], 10, &set_val))
			goto lcd_ss_debug_store_err;
		ret = lcd_set_ss(pdrv, set_val, 0xff, 0xff);
		if (ret)
			goto lcd_ss_debug_store_err;
		pdrv->config.timing.ss_level = set_val;
	} else if (strcmp(parm[0], "freq") == 0) {
		ssc_debug_type = SSC_DEBUG_FREQ;
		if (!parm[1])
			goto lcd_ss_debug_store_end;
		if (kstrtou8(parm[1], 10, &set_val))
			goto lcd_ss_debug_store_err;
		ret = lcd_set_ss(pdrv, 0xff, set_val, 0xff);
		if (ret)
			goto lcd_ss_debug_store_err;
		pdrv->config.timing.ss_freq = set_val;
	} else if (strcmp(parm[0], "mode") == 0) {
		ssc_debug_type = SSC_DEBUG_MODE;
		if (!parm[1])
			goto lcd_ss_debug_store_end;
		if (kstrtou8(parm[1], 10, &set_val))
			goto lcd_ss_debug_store_err;
		ret = lcd_set_ss(pdrv, 0xff, 0xff, set_val);
		if (ret)
			goto lcd_ss_debug_store_err;
		pdrv->config.timing.ss_mode = set_val;
	} else {
		ssc_debug_type = SSC_DEBUG_UNKNOWN;
		goto lcd_ss_debug_store_err;
	}

lcd_ss_debug_store_end:
	kfree(buf_orig);
	kfree(parm);
	return count;

lcd_ss_debug_store_err:
	LCDERR("%s: 0x%x fail\n", __func__, ssc_debug_type);
	ssc_debug_type = SSC_DEBUG_ERR;
	kfree(buf_orig);
	kfree(parm);
	return count;
}

static ssize_t lcd_debug_clk_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	unsigned int n = 0;

	n += lcd_clk_config_print(pdrv, buf, 0);
	n += lcd_clk_clkmsr_print(pdrv, (buf + n), n);
	n += sprintf(buf + n, "\n");

	return n;
}

static ssize_t lcd_debug_clk_store(struct device *dev, struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	unsigned int set_val = 0xffffffff;
	char *buf_orig;
	char **parm = NULL;
	int ret;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return count;
	parm = kcalloc(4, sizeof(char *), GFP_KERNEL);
	if (!parm) {
		kfree(buf_orig);
		return count;
	}

	lcd_debug_parse_param(buf_orig, parm, 4);

	if (strcmp(parm[0], "path") == 0) {
		if (!parm[1] || kstrtou32(parm[1], 10, &set_val)) {
			LCDERR("clk path set error\n");
			goto lcd_clk_debug_store_end;
		}

		ret = lcd_clk_path_change(pdrv, set_val);
		if (ret == 0)
			pdrv->clk_path = set_val;
		lcd_clk_generate_parameter(pdrv);
		LCDPR("[%u]: CLK change path: %u\n", pdrv->index, set_val);
	}

lcd_clk_debug_store_end:
	kfree(buf_orig);
	kfree(parm);
	return count;
}

static ssize_t lcd_debug_test_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);

	return sprintf(buf, "[%d]: test pattern: %d\n", pdrv->index, pdrv->test_state);
}

static ssize_t lcd_debug_test_store(struct device *dev, struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	unsigned int temp = 0, i = 0;
	int ret = 0;
	unsigned long flags = 0;

	if (buf[0] == 'f') { /* force test pattern */
		ret = sscanf(buf, "force %d", &temp);
		if (ret == 0)
			goto lcd_debug_test_store_next;
		spin_lock_irqsave(&pdrv->isr_lock, flags);
		pdrv->test_flag = (unsigned char)temp;
		pdrv->test_state = (unsigned char)temp;
		lcd_debug_test(pdrv, pdrv->test_state);
		spin_unlock_irqrestore(&pdrv->isr_lock, flags);
		return count;
	}

lcd_debug_test_store_next:
	ret = kstrtouint(buf, 10, &temp);
	if (ret) {
		pr_info("invalid data\n");
		return -EINVAL;
	}
	spin_lock_irqsave(&pdrv->isr_lock, flags);
	pdrv->test_flag = (unsigned char)temp;
	spin_unlock_irqrestore(&pdrv->isr_lock, flags);

	LCDPR("[%d]: %s: %d\n", pdrv->index, __func__, temp);
	while (i++ < 5000) {
		if (pdrv->test_state == temp)
			break;
		lcd_delay_us(20);
	}

	return count;
}

static ssize_t lcd_debug_mute_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	int flag = lcd_mute_state_get(pdrv);

	return sprintf(buf, "get lcd mute state: %d\n", flag);
}

static ssize_t lcd_debug_mute_store(struct device *dev, struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	unsigned int temp = 0;
	int ret = 0;

	ret = kstrtouint(buf, 10, &temp);
	if (ret) {
		pr_info("invalid data\n");
		return -EINVAL;
	}

	temp = temp ? 1 : 0;
	if (temp)
		lcd_screen_black(pdrv);
	else
		lcd_screen_restore(pdrv);

	return count;
}

static void lcd_debug_reg_op(struct aml_lcd_drv_s *pdrv, unsigned int reg,
				unsigned int data, unsigned int bus, unsigned char reg_write)
{
#ifdef CONFIG_AMLOGIC_LCD_TABLET
	unsigned char temp = 0;
	int ret;
#endif
	unsigned int wr_rd_out, cnt;
	char reg_bus_name[10];
	char *reg_log;

	reg_log = kzalloc(64 * sizeof(char), GFP_KERNEL);
	if (!reg_log)
		return;

	switch (bus) {
	case LCD_REG_DBG_VC_BUS:
		sprintf(reg_bus_name, "vcbus");
		if (reg_write)
			lcd_vcbus_write(reg, data);
		wr_rd_out = lcd_vcbus_read(reg);
		break;
	case LCD_REG_DBG_ANA_BUS:
		sprintf(reg_bus_name, "analog");
		if (reg_write)
			lcd_ana_write(reg, data);
		wr_rd_out = lcd_ana_read(reg);
		break;
	case LCD_REG_DBG_CLK_BUS:
		sprintf(reg_bus_name, "clock");
		if (reg_write)
			lcd_clk_write(reg, data);
		wr_rd_out = lcd_clk_read(reg);
		break;
	case LCD_REG_DBG_PERIPHS_BUS:
		sprintf(reg_bus_name, "periphs");
		if (reg_write)
			lcd_periphs_write(pdrv, reg, data);
		wr_rd_out = lcd_periphs_read(pdrv, reg);
		break;
#ifdef CONFIG_AMLOGIC_LCD_TV
	case LCD_REG_DBG_TCON_BUS:
		sprintf(reg_bus_name, "TCON");
		if (reg_write)
			lcd_tcon_reg_write(pdrv, reg, data);
		wr_rd_out = lcd_tcon_reg_read(pdrv, reg);
		break;
#endif
#ifdef CONFIG_AMLOGIC_LCD_TABLET
	case LCD_REG_DBG_MIPIHOST_BUS:
		sprintf(reg_bus_name, "DSI host");
		if (reg_write)
			dsi_host_write(pdrv, reg, data);
		wr_rd_out = dsi_host_read(pdrv, reg);
		break;
	case LCD_REG_DBG_MIPIPHY_BUS:
		sprintf(reg_bus_name, "DSI dphy");
		if (reg_write)
			dsi_phy_write(pdrv, reg, data);
		wr_rd_out = dsi_phy_read(pdrv, reg);
		break;
	case LCD_REG_DBG_EDPHOST_BUS:
		sprintf(reg_bus_name, "eDP host");
		if (reg_write)
			dptx_reg_write(pdrv, reg, data);
		wr_rd_out = dptx_reg_read(pdrv, reg);
		break;
	case LCD_REG_DBG_EDPDPCD_BUS:
		sprintf(reg_bus_name, "eDP DPCD");
		if (reg_write) {
			ret = dptx_aux_write_single(pdrv, reg, data);
			if (ret) {
				LCDERR("[%d]: write DPCD reg 0x%08x error", pdrv->index, reg);
				kfree(reg_log);
				return;
			}
		}
		ret = dptx_aux_read(pdrv, reg, 1, &temp);
		if (ret) {
			LCDERR("[%d]: read DPCD reg 0x%08x error", pdrv->index, reg);
			kfree(reg_log);
			return;
		}
		wr_rd_out = temp;
		break;
#endif
	case LCD_REG_DBG_COMBOPHY_BUS:
		sprintf(reg_bus_name, "COMBODPHY");
		if (reg_write)
			lcd_combo_dphy_write(pdrv, reg, data);
		wr_rd_out = lcd_combo_dphy_read(pdrv, reg);
		break;
	case LCD_REG_DBG_RST_BUS:
		sprintf(reg_bus_name, "RST");
		if (reg_write)
			lcd_reset_write(pdrv, reg, data);
		wr_rd_out = lcd_reset_read(pdrv, reg);
		break;
	case LCD_REG_DBG_HHI_BUS:
		sprintf(reg_bus_name, "HIU");
		if (reg_write)
			lcd_hiu_write(reg, data);
		wr_rd_out = lcd_hiu_read(reg);
		break;
	default:
		LCDERR("[%d]: unknown bus %d\n", pdrv->index, bus);
		kfree(reg_log);
		return;
	}

	cnt = snprintf(reg_log, 64, "%s %s [0x%04x] = 0x%08x",
		reg_write ? "write" : "read", reg_bus_name, reg, reg_write ? data : wr_rd_out);
	if (reg_write)
		snprintf(reg_log + cnt, 64 - cnt, ", readback 0x%08x", wr_rd_out);
	else
		snprintf(reg_log + cnt, 64 - cnt, "\n");

	pr_info("%s", reg_log);
	kfree(reg_log);
}

static void lcd_debug_reg_dump(struct aml_lcd_drv_s *pdrv, unsigned int reg,
			       unsigned int num, unsigned int bus)
{
	int i;
#ifdef CONFIG_AMLOGIC_LCD_TABLET
	unsigned char *buf;
	int ret;
#endif

	switch (bus) {
	case LCD_REG_DBG_VC_BUS:
		pr_info("dump vcbus regs:\n");
		for (i = 0; i < num; i++) {
			pr_info("[0x%04x] = 0x%08x\n",
				(reg + i), lcd_vcbus_read(reg + i));
		}
		break;
	case LCD_REG_DBG_ANA_BUS:
		pr_info("dump ana regs:\n");
		for (i = 0; i < num; i++) {
			pr_info("[0x%04x] = 0x%08x\n",
				(reg + i), lcd_ana_read(reg + i));
		}
		break;
	case LCD_REG_DBG_CLK_BUS:
		pr_info("dump clk regs:\n");
		for (i = 0; i < num; i++) {
			pr_info("[0x%04x] = 0x%08x\n",
				(reg + i), lcd_clk_read(reg + i));
		}
		break;
	case LCD_REG_DBG_PERIPHS_BUS:
		pr_info("dump periphs-bus regs:\n");
		for (i = 0; i < num; i++) {
			pr_info("[0x%04x] = 0x%08x\n",
				(reg + i), lcd_periphs_read(pdrv, reg + i));
		}
		break;
#ifdef CONFIG_AMLOGIC_LCD_TV
	case LCD_REG_DBG_TCON_BUS:
		pr_info("dump tcon regs:\n");
		for (i = 0; i < num; i++) {
			pr_info("[0x%04x] = 0x%08x\n",
				(reg + i), lcd_tcon_reg_read(pdrv, reg + i));
		}
		break;
#endif
#ifdef CONFIG_AMLOGIC_LCD_TABLET
	case LCD_REG_DBG_MIPIHOST_BUS:
		pr_info("dump mipi_dsi_host regs:\n");
		for (i = 0; i < num; i++) {
			pr_info("[0x%04x] = 0x%08x\n",
				(reg + i), dsi_host_read(pdrv, reg + i));
		}
		break;
	case LCD_REG_DBG_MIPIPHY_BUS:
		pr_info("dump mipi_dsi_phy regs:\n");
		for (i = 0; i < num; i++) {
			pr_info("[0x%04x] = 0x%08x\n",
				(reg + i), dsi_phy_read(pdrv, reg + i));
		}
		break;
	case LCD_REG_DBG_EDPHOST_BUS:
		pr_info("dump edp regs:\n");
		for (i = 0; i < num; i++) {
			pr_info("[0x%04x] = 0x%08x\n",
				(reg + i), dptx_reg_read(pdrv, reg + i));
		}
		break;
	case LCD_REG_DBG_EDPDPCD_BUS:
		pr_info("dump edp dpcd regs:\n");
		buf = kcalloc(num, sizeof(unsigned char), GFP_KERNEL);
		if (!buf)
			break;
		ret = dptx_aux_read(pdrv, reg, num, buf);
		if (ret) {
			kfree(buf);
			break;
		}
		for (i = 0; i < num; i++)
			pr_info("[0x%04x] = 0x%02x\n", (reg + i), buf[i]);
		kfree(buf);
		break;
#endif
	case LCD_REG_DBG_COMBOPHY_BUS:
		pr_info("dump combo dphy regs:\n");
		for (i = 0; i < num; i++) {
			pr_info("[0x%04x] = 0x%08x\n",
				(reg + i), lcd_combo_dphy_read(pdrv, reg + i));
		}
		break;
	case LCD_REG_DBG_RST_BUS:
		pr_info("dump rst regs:\n");
		for (i = 0; i < num; i++) {
			pr_info("[0x%04x] = 0x%08x\n",
				(reg + i), lcd_reset_read(pdrv, reg + i));
		}
		break;
	case LCD_REG_DBG_HHI_BUS:
		pr_info("dump hiu regs:\n");
		for (i = 0; i < num; i++) {
			pr_info("[0x%04x] = 0x%08x\n",
				(reg + i), lcd_hiu_read(reg + i));
		}
		break;
	default:
		break;
	}
}

static ssize_t lcd_debug_reg_store(struct device *dev, struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	unsigned int bus = LCD_REG_DBG_MAX_BUS;
	unsigned int reg32 = 0, data32 = 0, cnt = 1, i;
	int ret = 0;

	switch (buf[0]) {
	case 'w':
		if (buf[1] == 'v') { /* vcbus */
			ret = sscanf(buf, "wv %x %x", &reg32, &data32);
			bus = LCD_REG_DBG_VC_BUS;
		} else if (buf[1] == 'a') { /* ana */
			ret = sscanf(buf, "wa %x %x", &reg32, &data32);
			bus = LCD_REG_DBG_ANA_BUS;
		} else if (buf[1] == 'h') { /* hiu */
			ret = sscanf(buf, "wh %x %x", &reg32, &data32);
			bus = LCD_REG_DBG_HHI_BUS;
		} else if (buf[1] == 'c') { /* clk */
			ret = sscanf(buf, "wc %x %x", &reg32, &data32);
			bus = LCD_REG_DBG_CLK_BUS;
		} else if (buf[1] == 'p') { /* periphs */
			ret = sscanf(buf, "wp %x %x", &reg32, &data32);
			bus = LCD_REG_DBG_PERIPHS_BUS;
#ifdef CONFIG_AMLOGIC_LCD_TV
		} else if (buf[1] == 't') { /* tcon */
			ret = sscanf(buf, "wt %x %x", &reg32, &data32);
			bus = LCD_REG_DBG_TCON_BUS;
#endif
#ifdef CONFIG_AMLOGIC_LCD_TABLET
		} else if (buf[1] == 'm') {
			if (buf[2] == 'h') { /* mipi host */
				ret = sscanf(buf, "wmh %x %x", &reg32, &data32);
				bus = LCD_REG_DBG_MIPIHOST_BUS;
			} else if (buf[2] == 'p') { /* mipi phy */
				ret = sscanf(buf, "wmp %x %x", &reg32, &data32);
				bus = LCD_REG_DBG_MIPIPHY_BUS;
			}
		} else if (buf[1] == 'e') {
			if (buf[2] == 'h') { /* edp host */
				ret = sscanf(buf, "weh %x %x", &reg32, &data32);
				bus = LCD_REG_DBG_EDPHOST_BUS;
			} else if (buf[2] == 'd') { /* edp dpcd */
				ret = sscanf(buf, "wed %x %x", &reg32, &data32);
				bus = LCD_REG_DBG_EDPDPCD_BUS;
			}
#endif
		} else if (buf[1] == 'd') { /* combo dphy */
			ret = sscanf(buf, "wd %x %x", &reg32, &data32);
			bus = LCD_REG_DBG_COMBOPHY_BUS;
		} else if (buf[1] == 'r') { /* rst */
			ret = sscanf(buf, "wr %x %x", &reg32, &data32);
			bus = LCD_REG_DBG_RST_BUS;
		}
		if (ret == 2) {
			lcd_debug_reg_op(pdrv, reg32, data32, bus, 1);
		} else {
			pr_info("invalid data\n");
			return -EINVAL;
		}
		break;
	case 'r':
		if (buf[1] == 'v') { /* vcbus */
			ret = sscanf(buf, "rv %x %d", &reg32, &cnt);
			bus = LCD_REG_DBG_VC_BUS;
		} else if (buf[1] == 'a') { /* ana */
			ret = sscanf(buf, "ra %x %d", &reg32, &cnt);
			bus = LCD_REG_DBG_ANA_BUS;
		} else if (buf[1] == 'h') { /* hiu */
			ret = sscanf(buf, "rh %x", &reg32);
			bus = LCD_REG_DBG_HHI_BUS;
		} else if (buf[1] == 'c') { /* clk */
			ret = sscanf(buf, "rc %x %d", &reg32, &cnt);
			bus = LCD_REG_DBG_CLK_BUS;
		} else if (buf[1] == 'p') { /* periphs */
			ret = sscanf(buf, "rp %x %d", &reg32, &cnt);
			bus = LCD_REG_DBG_PERIPHS_BUS;
#ifdef CONFIG_AMLOGIC_LCD_TV
		} else if (buf[1] == 't') { /* tcon */
			ret = sscanf(buf, "rt %x, %d", &reg32, &cnt);
			bus = LCD_REG_DBG_TCON_BUS;
#endif
#ifdef CONFIG_AMLOGIC_LCD_TABLET
		} else if (buf[1] == 'm') {
			if (buf[2] == 'h') { /* mipi host */
				ret = sscanf(buf, "rmh %x %d", &reg32, &cnt);
				bus = LCD_REG_DBG_MIPIHOST_BUS;
			} else if (buf[2] == 'p') { /* mipi phy */
				ret = sscanf(buf, "rmp %x %d", &reg32, &cnt);
				bus = LCD_REG_DBG_MIPIPHY_BUS;
			}
		} else if (buf[1] == 'e') {
			if (buf[2] == 'h') { /* edp host */
				ret = sscanf(buf, "reh %x, %d", &reg32, &cnt);
				bus = LCD_REG_DBG_EDPHOST_BUS;
			} else if (buf[2] == 'd') { /* edp dpcd */
				ret = sscanf(buf, "red %x %d", &reg32, &cnt);
				bus = LCD_REG_DBG_EDPDPCD_BUS;
			}
#endif
		} else if (buf[1] == 'd') { /* combo dphy */
			ret = sscanf(buf, "rd %x %d", &reg32, &cnt);
			bus = LCD_REG_DBG_COMBOPHY_BUS;
		} else if (buf[1] == 'r') { /* rst */
			ret = sscanf(buf, "rr %x %d", &reg32, &cnt);
			bus = LCD_REG_DBG_RST_BUS;
		}

		if (ret == 1) {
			lcd_debug_reg_op(pdrv, reg32, 0, bus, 0);
		} else if (ret == 2) {
			for (i = 0; i < cnt; i++)
				lcd_debug_reg_op(pdrv, reg32 + i, 0, bus, 0);
		} else {
			pr_info("invalid data\n");
			return -EINVAL;
		}
		break;
	case 'd':
		if (buf[1] == 'v') { /* vcbus */
			ret = sscanf(buf, "dv %x %d", &reg32, &data32);
			bus = LCD_REG_DBG_VC_BUS;
		} else if (buf[1] == 'a') { /* ana */
			ret = sscanf(buf, "da %x %d", &reg32, &data32);
			bus = LCD_REG_DBG_ANA_BUS;
		} else if (buf[1] == 'h') { /* hiu */
			ret = sscanf(buf, "dh %x %d", &reg32, &data32);
			bus = LCD_REG_DBG_HHI_BUS;
		} else if (buf[1] == 'c') { /* clk */
			ret = sscanf(buf, "dc %x %d", &reg32, &data32);
			bus = LCD_REG_DBG_CLK_BUS;
		} else if (buf[1] == 'p') { /* periphs */
			ret = sscanf(buf, "dp %x %d", &reg32, &data32);
			bus = LCD_REG_DBG_PERIPHS_BUS;
#ifdef CONFIG_AMLOGIC_LCD_TV
		} else if (buf[1] == 't') { /* tcon */
			ret = sscanf(buf, "dt %x %d", &reg32, &data32);
			bus = LCD_REG_DBG_TCON_BUS;
#endif
#ifdef CONFIG_AMLOGIC_LCD_TABLET
		} else if (buf[1] == 'm') {
			if (buf[2] == 'h') { /* mipi host */
				ret = sscanf(buf, "dmh %x %d", &reg32, &data32);
				bus = LCD_REG_DBG_MIPIHOST_BUS;
			} else if (buf[2] == 'p') { /* mipi phy */
				ret = sscanf(buf, "dmp %x %d", &reg32, &data32);
				bus = LCD_REG_DBG_MIPIPHY_BUS;
			}
		} else if (buf[1] == 'e') {
			if (buf[2] == 'h') { /* edp host */
				ret = sscanf(buf, "deh %x %d", &reg32, &data32);
				bus = LCD_REG_DBG_EDPHOST_BUS;
			} else if (buf[2] == 'd') { /* edp dpcd */
				ret = sscanf(buf, "ded %x %d", &reg32, &data32);
				bus = LCD_REG_DBG_EDPDPCD_BUS;
			}
#endif
		} else if (buf[1] == 'd') { /* combo dphy */
			ret = sscanf(buf, "dd %x %x", &reg32, &data32);
			bus = LCD_REG_DBG_COMBOPHY_BUS;
		} else if (buf[1] == 'r') { /* rst */
			ret = sscanf(buf, "dr %x %x", &reg32, &data32);
			bus = LCD_REG_DBG_RST_BUS;
		}
		if (ret == 2) {
			lcd_debug_reg_dump(pdrv, reg32, data32, bus);
		} else {
			pr_info("invalid data\n");
			return -EINVAL;
		}
		break;
	default:
		pr_info("wrong command\n");
		break;
	}

	return count;
}

static ssize_t lcd_debug_vlock_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	ssize_t len = 0;

	len = sprintf(buf, "custom vlock attr:\n"
			   "  vlock_valid:       %d\n"
			   "  vlock_en:          %d\n"
			   "  vlock_work_mode:   %d\n"
			   "  vlock_pll_m_limit: %d\n"
			   "  vlock_line_limit:  %d\n",
			   pdrv->config.vlock_param[0],
			   pdrv->config.vlock_param[1],
			   pdrv->config.vlock_param[2],
			   pdrv->config.vlock_param[3],
			   pdrv->config.vlock_param[4]);

	return len;
}

#define LCD_DEBUG_DUMP_INFO_BASIC     0
#define LCD_DEBUG_DUMP_INFO_CUS_CTRL  2
#define LCD_DEBUG_DUMP_INFO_TCON      3
#define LCD_DEBUG_DUMP_INFO_POWER     4
#define LCD_DEBUG_DUMP_REG            5
#define LCD_DEBUG_DUMP_OPTICAL        10
#define LCD_DEBUG_DUMP_CLK_PARA       11
static int lcd_debug_dump_state;
static ssize_t lcd_debug_dump_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	char *print_buf;
	int len = 0;

	print_buf = kcalloc(PR_BUF_MAX, sizeof(char), GFP_KERNEL);
	if (!print_buf)
		return sprintf(buf, "%s: buf malloc error\n", __func__);

	switch (lcd_debug_dump_state) {
	case LCD_DEBUG_DUMP_INFO_BASIC:
		len = lcd_info_basic_print(pdrv, print_buf, 0);
		lcd_info_adv_print(pdrv, print_buf + len, len);
		break;
	case LCD_DEBUG_DUMP_INFO_TCON:
		lcd_info_tcon_print(pdrv, print_buf, 0);
		break;
	case LCD_DEBUG_DUMP_INFO_POWER:
		lcd_power_step_info_print(pdrv, print_buf, 0);
		break;
	case LCD_DEBUG_DUMP_REG:
		len = lcd_clk_reg_print(pdrv, print_buf, 0);
		len += lcd_venc_reg_print(pdrv, print_buf + len, len);
		len += lcd_reg_if_print(pdrv, print_buf + len, len);
		len += lcd_reg_phy_print(pdrv, print_buf + len, len);
		lcd_reg_pinmux_print(pdrv, print_buf + len, len);
		break;
	case LCD_DEBUG_DUMP_OPTICAL:
		lcd_optical_info_print(pdrv, print_buf, 0);
		break;
	case LCD_DEBUG_DUMP_CLK_PARA:
		len = lcd_clk_config_print(pdrv, print_buf, 0);
		lcd_clk_clkmsr_print(pdrv, print_buf + len, len);
		break;
	default:
		sprintf(print_buf, "%s: invalid command\n", __func__);
		break;
	}
	len = sprintf(buf, "%s\n", print_buf);
	kfree(print_buf);

	return len;
}

static ssize_t lcd_debug_dump_store(struct device *dev, struct device_attribute *attr,
				    const char *buf, size_t count)
{
#define __MAX_PARAM 47
	char *buf_orig;
	char *parm[__MAX_PARAM] = {NULL};

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig) {
		LCDERR("%s: buf malloc error\n", __func__);
		return count;
	}
	lcd_debug_parse_param(buf_orig, (char **)&parm, __MAX_PARAM);

	if (strcmp(parm[0], "info") == 0) {
		lcd_debug_dump_state = LCD_DEBUG_DUMP_INFO_BASIC;
	} else if (strcmp(parm[0], "basic") == 0) {
		lcd_debug_dump_state = LCD_DEBUG_DUMP_INFO_BASIC;
	} else if (strcmp(parm[0], "tcon") == 0) {
		lcd_debug_dump_state = LCD_DEBUG_DUMP_INFO_TCON;
	} else if (strcmp(parm[0], "power") == 0) {
		lcd_debug_dump_state = LCD_DEBUG_DUMP_INFO_POWER;
	} else if (strcmp(parm[0], "reg") == 0) {
		lcd_debug_dump_state = LCD_DEBUG_DUMP_REG;
	} else if (strcmp(parm[0], "opt") == 0) {
		lcd_debug_dump_state = LCD_DEBUG_DUMP_OPTICAL;
	} else if (strcmp(parm[0], "hdr") == 0) {
		lcd_debug_dump_state = LCD_DEBUG_DUMP_OPTICAL;
	} else if (strcmp(parm[0], "clk") == 0) {
		lcd_debug_dump_state = LCD_DEBUG_DUMP_CLK_PARA;
	} else {
		LCDERR("invalid command\n");
		kfree(buf_orig);
		return -EINVAL;
	}

	kfree(buf_orig);
	return count;
#undef __MAX_PARAM
}

static ssize_t lcd_debug_print_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "get debug print flag: 0x%x\n", lcd_debug_print_flag);
}

static ssize_t lcd_debug_print_store(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t count)
{
	int ret = 0;
	unsigned int temp = 0;

	ret = kstrtouint(buf, 16, &temp);
	if (ret) {
		pr_info("invalid data\n");
		return -EINVAL;
	}
	lcd_debug_print_flag = temp;
	LCDPR("set debug print flag: 0x%x\n", lcd_debug_print_flag);

	return count;
}

static ssize_t lcd_debug_unmute_cnt_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);

	return sprintf(buf, "unmute_cnt_test: %d, unmute_cnt_added: %d, unmute_cnt: %d\n",
		pdrv->unmute_cnt_test, pdrv->unmute_cnt_added, pdrv->unmute_cnt);
}

static ssize_t lcd_debug_unmute_cnt_store(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	unsigned int temp = 0;
	int ret = 0;

	switch (buf[0]) {
	case 'a':
		ret = sscanf(buf, "add %d", &temp);
		if (ret != 1)
			goto lcd_debug_unmute_cnt_store_err;
		/*unmute_cnt_added take effect only once, will auto clean in next unmute*/
		pdrv->unmute_cnt_added = (unsigned short)temp;
		LCDPR("set unmute_cnt_added: %d\n", pdrv->unmute_cnt_added);
		break;
	default:
		ret = kstrtouint(buf, 10, &temp);
		if (ret)
			goto lcd_debug_unmute_cnt_store_err;
		pdrv->unmute_cnt_test = (unsigned short)temp;
		LCDPR("set unmute_cnt_test: 0x%x\n", pdrv->unmute_cnt_test);
		break;
	}

	return count;

lcd_debug_unmute_cnt_store_err:
	pr_info("invalid data\n");
	return -EINVAL;
}

static ssize_t lcd_debug_mute_cnt_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);

	return sprintf(buf, "mute_cnt_test: %d, mute_cnt: %d\n",
		pdrv->mute_cnt_test, pdrv->mute_cnt);
}

static ssize_t lcd_debug_mute_cnt_store(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	unsigned int temp = 0;
	int ret = 0;

	ret = kstrtouint(buf, 10, &temp);
	if (ret) {
		pr_info("invalid data\n");
		return -EINVAL;
	}
	pdrv->mute_cnt_test = (unsigned short)temp;
	LCDPR("set mute_cnt_test: %d\n", pdrv->mute_cnt_test);

	return count;
}

static ssize_t lcd_debug_cus_ctrl_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	unsigned int temp = 0;
	int ret = 0;

	switch (buf[0]) {
	case 's':
		ret = sscanf(buf, "switch %d", &temp);
		if (ret != 1)
			goto lcd_debug_cus_ctrl_store_err;
		pdrv->config.timing.switch_type_dbg = (unsigned char)temp;
		LCDPR("set timing_switch_flag_dbg: %d\n",
			pdrv->config.timing.switch_type_dbg);
		break;
	default:
		goto lcd_debug_cus_ctrl_store_err;
	}

	return count;

lcd_debug_cus_ctrl_store_err:
	pr_info("invalid data\n");
	return -EINVAL;
}

static ssize_t lcd_debug_vinfo_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	ssize_t len = 0;
	/* DO NOT MODIFY, hwc parse this node to get vinfo */
	len = sprintf(buf, "lcd vinfo:\n"
			   "  lcd_mode:           %s\n"
			   "  name:               %s\n"
			   "  mode:               %d\n"
			   "  frac:               %d\n"
			   "  width:              %d\n"
			   "  height:             %d\n"
			   "  field_height:       %d\n"
			   "  aspect_ratio_num:   %d\n"
			   "  aspect_ratio_den:   %d\n"
			   "  sync_duration_num:  %d\n"
			   "  sync_duration_den:  %d\n"
			   "  std_duration:       %d\n"
			   "  screen_real_width:  %d\n"
			   "  screen_real_height: %d\n"
			   "  htotal:             %d\n"
			   "  vtotal:             %d\n"
			   "  fr_adj_type:        %d\n"
			   "  video_clk:          %d\n"
			   "  viu_color_fmt:      %d\n"
			   "  viu_mux:            0x%x\n\n",
			   lcd_mode_mode_to_str(pdrv->mode),
			   pdrv->vinfo.name,
			   pdrv->vinfo.mode,
			   pdrv->vinfo.frac,
			   pdrv->vinfo.width,
			   pdrv->vinfo.height,
			   pdrv->vinfo.field_height,
			   pdrv->vinfo.aspect_ratio_num,
			   pdrv->vinfo.aspect_ratio_den,
			   pdrv->vinfo.sync_duration_num,
			   pdrv->vinfo.sync_duration_den,
			   pdrv->vinfo.std_duration,
			   pdrv->vinfo.screen_real_width,
			   pdrv->vinfo.screen_real_height,
			   pdrv->vinfo.htotal,
			   pdrv->vinfo.vtotal,
			   pdrv->vinfo.fr_adj_type,
			   pdrv->vinfo.video_clk,
			   pdrv->vinfo.viu_color_fmt,
			   pdrv->vinfo.viu_mux);
	return len;
}

static ssize_t lcd_debug_sw_vlock_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
#define __MAX_PARAM 8
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	char *buf_orig;
	char *parm[__MAX_PARAM] = {NULL};
	unsigned int en, mode;
	int ret;
	struct aml_fr_lock_s *fr_lock = NULL;
	int kp, ki, kd;

	if (!buf || !pdrv)
		return count;

	fr_lock = pdrv->fr_lock;
	if (!fr_lock)
		return count;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig) {
		LCDERR("%s: buf malloc error\n", __func__);
		return count;
	}
	lcd_debug_parse_param(buf_orig, (char **)&parm, __MAX_PARAM);

	if (strcmp(parm[0], "mode") == 0) {
		if (!parm[1]) {
			pr_err("invalid data\n");
			goto lcd_debug_sw_vlock_store_next;
		}

		ret = kstrtouint(parm[1], 10, &mode);
		if (ret) {
			pr_err("invalid data\n");
			goto lcd_debug_sw_vlock_store_next;
		}
		fr_lock->mode = mode;
		fr_lock->base_dura_num = 0;
		fr_lock->base_dura_den = 0;
		fr_lock->rst = 1;
		LCDPR("sw_vlock init mode:%d\n", mode);
	} else if (strcmp(parm[0], "en") == 0) {
		if (!parm[1]) {
			pr_err("invalid data\n");
			goto lcd_debug_sw_vlock_store_next;
		}
		ret = kstrtouint(parm[1], 10, &en);
		if (ret) {
			pr_err("invalid data\n");
			goto lcd_debug_sw_vlock_store_next;
		}
		if (en && !fr_lock->en) {
			fr_lock->base_dura_num = 0;
			fr_lock->base_dura_den = 0;
			fr_lock->rst = 1;
			fr_lock->en = 1;
			LCDPR("sw_vlock enable\n");
		} else if (!en && fr_lock->en) {
			fr_lock->rst = 1;
			fr_lock->en = 0;
			fr_lock_recovery_freq(pdrv);
			LCDPR("sw_vlock disable\n");
		}
	} else if (strcmp(parm[0], "rst") == 0) {
		fr_lock->rst = 1;
		fr_lock->base_dura_num = 0;
		fr_lock->base_dura_den = 0;
		LCDPR("sw_vlock reset\n");
	} else if (strcmp(parm[0], "show") == 0) {
		if (!parm[1]) {
			pr_err("invalid data\n");
			goto lcd_debug_sw_vlock_store_next;
		}

		ret = kstrtouint(parm[1], 10, &mode);
		if (ret) {
			pr_err("invalid data\n");
			goto lcd_debug_sw_vlock_store_next;
		}
		fr_lock->show = mode;
		LCDPR("sw_vlock show %d\n", mode);
	} else if (strcmp(parm[0], "pid") == 0) {
		if (!parm[1] || !parm[2] || !parm[3]) {
			pr_err("invalid data\n");
			goto lcd_debug_sw_vlock_store_next;
		}

		ret = kstrtoint(parm[1], 10, &kp);
		ret |= kstrtoint(parm[2], 10, &ki);
		ret |= kstrtoint(parm[3], 10, &kd);
		if (ret) {
			pr_err("invalid data\n");
			goto lcd_debug_sw_vlock_store_next;
		}
		fr_lock->kp = kp;
		fr_lock->ki = ki;
		fr_lock->kd = kd;
		LCDPR("sw_vlock pid %d %d, %d\n", kp, ki, kd);
	} else if (strcmp(parm[0], "dbg") == 0) {
		if (!parm[1]) {
			pr_err("invalid data\n");
			goto lcd_debug_sw_vlock_store_next;
		}

		ret = kstrtouint(parm[1], 10, &mode);
		if (ret) {
			pr_err("invalid data\n");
			goto lcd_debug_sw_vlock_store_next;
		}
		fr_lock->dbg = mode;
		LCDPR("sw_vlock dbg %d\n", mode);
	}
lcd_debug_sw_vlock_store_next:
	kfree(buf_orig);

	return count;
}

static ssize_t lcd_debug_sw_vlock_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct aml_fr_lock_s *fr_lock = NULL;
	ssize_t len = 0;

	if (!pdrv)
		return sprintf(buf, "null\n");

	fr_lock = pdrv->fr_lock;
	if (!fr_lock) {
		len = sprintf(buf, "not support sw_vlock\n");
		return len;
	}
	len = sprintf(buf, "en:%d, rst=%d, mode=%d, show:%d, ss_sta=%d, ss_level:%d, hw_vlock:%d\n"
		"base_fr=%d\n"
		"sync_num:%d\n"
		"sync_den:%d\n"
		"base_vtotal:%d\n"
		"pll_base_hz:%llu\n"
		"pll_adj_hz:%llu\n"
		"pll_base_m:%u\n"
		"pll_base_frac:%u\n"
		"pll_adj_m:%u\n"
		"pll_adj_frac:%s%u\n"
		"exp_vs_cnt:%d\n"
		"exp_vs_ns:%d\n"
		"line_limit:%d\n"
		"freq_limit:%d\n"
		"kp:%d, ki:%d, kd:%d\n",
		fr_lock->en, fr_lock->rst, fr_lock->mode, fr_lock->show,
		fr_lock->ss_sta, fr_lock->ss_level, fr_lock->hw_vlock_sta,
		fr_lock->base_fr,
		fr_lock->base_dura_num,
		fr_lock->base_dura_den,
		fr_lock->base_vtotal,
		fr_lock->pll_base_hz,
		fr_lock->pll_adj_hz,
		fr_lock->pll_base_m,
		fr_lock->pll_base_frac,
		fr_lock->pll_adj_m,
		(fr_lock->pll_adj_frac & (1 << 18)) ? "-" : "+", fr_lock->pll_adj_frac & 0x3ffff,
		fr_lock->exp_vs_cnt,
		fr_lock->exp_vs_ns,
		fr_lock->line_limit,
		fr_lock->freq_limit,
		fr_lock->kp, fr_lock->ki, fr_lock->kd
		);

	return len;
}

//make sure 30min diff is less than half frame
#define LCD_VS_MSR_ERR_MAX    280   //unit:0.000001
#define LCD_VS_MSR_DUMP       1
#define LCD_VS_MSR_AVG        2
#define LCD_VS_MSR_SNAPSHOT   3
static unsigned int lcd_vs_msr_sel;

static ssize_t lcd_debug_vs_msr_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	ssize_t len = 0, n = PR_BUF_MAX;
	unsigned long long sum = 0, target;
	unsigned int avg, err, i;

	if (!pdrv->vs_msr || !pdrv->vs_msr_rt)
		return sprintf(buf, "vs_msr buffer is NULL\n");

	target = pdrv->config.timing.act_timing.sync_duration_num;
	target = lcd_do_div(target * 1000000, pdrv->config.timing.act_timing.sync_duration_den);
	switch (lcd_vs_msr_sel) {
	case LCD_VS_MSR_DUMP:
		for (i = 0; i < pdrv->vs_msr_cnt; i++) {
			if (target >= pdrv->vs_msr[i])
				err = target - pdrv->vs_msr[i];
			else
				err = pdrv->vs_msr[i] - target;
			if (len >= PR_BUF_MAX)
				return len;
			n = PR_BUF_MAX - len;
			len += snprintf(buf + len, n, "[%d]: %d, %d%s\n",
				i, pdrv->vs_msr[i],
				err, (err > pdrv->vs_msr_err_th) ? "(X)" : "");
		}
		break;
	case LCD_VS_MSR_AVG:
		for (i = 0; i < pdrv->vs_msr_cnt; i++)
			sum += pdrv->vs_msr[i];
		avg = lcd_do_div(sum, pdrv->vs_msr_cnt);
		if (target >= avg)
			err = target - avg;
		else
			err = avg - target;
		len = sprintf(buf, "vs_msr avg: %d, err: %d%s\n",
			avg, err, (err > pdrv->vs_msr_err_th) ? "(X)" : "");
		break;
	case LCD_VS_MSR_SNAPSHOT:
		for (i = 0; i < pdrv->vs_msr_i; i++) {
			if (target >= pdrv->vs_msr_rt[i])
				err = target - pdrv->vs_msr_rt[i];
			else
				err = pdrv->vs_msr_rt[i] - target;
			if (len >= PR_BUF_MAX)
				return len;
			n = PR_BUF_MAX - len;
			len += snprintf(buf + len, n, "[%d]: %d, %d%s\n",
				i, pdrv->vs_msr_rt[i],
				err, (err > pdrv->vs_msr_err_th) ? "(X)" : "");
		}
		break;
	default:
		break;
	}

	if (len >= PR_BUF_MAX)
		return len;
	n = PR_BUF_MAX - len;
	len += snprintf(buf + len, n,
		"vs_msr max:%d, min:%d\n"
		"vs_msr en:%d, err_th:%d, i:%d, cnt:%d, cnt_max:%d\n",
		pdrv->vs_msr_max, pdrv->vs_msr_min,
		pdrv->vs_msr_en, pdrv->vs_msr_err_th,
		pdrv->vs_msr_i, pdrv->vs_msr_cnt, pdrv->vs_msr_cnt_max);

	return len;
}

static ssize_t lcd_debug_vs_msr_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
#define __MAX_PARAM 8
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	char *buf_orig;
	char *parm[__MAX_PARAM] = {NULL};
	unsigned int temp, msr_en = 0;
	int ret;

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig) {
		LCDERR("%s: buf malloc error\n", __func__);
		return count;
	}
	lcd_debug_parse_param(buf_orig, (char **)&parm, __MAX_PARAM);

	if (strcmp(parm[0], "init") == 0) {
		if (!parm[1]) {
			pr_err("invalid data\n");
			goto lcd_debug_vs_msr_store_next;
		}
		ret = kstrtouint(parm[1], 10, &temp);
		if (ret) {
			pr_err("invalid data\n");
			goto lcd_debug_vs_msr_store_next;
		}
		if (temp) {
			pdrv->vs_msr_cnt_max = temp;
			pdrv->vs_msr = kcalloc(pdrv->vs_msr_cnt_max,
					sizeof(unsigned int), GFP_KERNEL);
			if (!pdrv->vs_msr)
				goto lcd_debug_vs_msr_store_next;
			pdrv->vs_msr_rt = kcalloc(300, sizeof(unsigned int), GFP_KERNEL);
			if (!pdrv->vs_msr_rt)
				goto lcd_debug_vs_msr_store_next;
			pr_info("vs_msr buffer init ok, size: %d\n", pdrv->vs_msr_cnt_max);
		} else {
			if (pdrv->vs_msr_en) {
				pr_err("vs_msr is still enabled, can't release buffer\n");
				goto lcd_debug_vs_msr_store_next;
			}
			pdrv->vs_msr_i = 0;
			pdrv->vs_msr_cnt = 0;
			pdrv->vs_msr_cnt_max = 0;

			kfree(pdrv->vs_msr);
			pdrv->vs_msr = NULL;
			kfree(pdrv->vs_msr_rt);
			pdrv->vs_msr_rt = NULL;
		}
	} else if (strcmp(parm[0], "en") == 0) {
		if (parm[1]) {
			ret = kstrtouint(parm[1], 10, &temp);
			if (ret) {
				pr_err("invalid data\n");
				goto lcd_debug_vs_msr_store_next;
			}
			if (temp) {
				if (!pdrv->vs_msr || !pdrv->vs_msr_rt) {
					pr_err("vs_msr buffer is NULL, can't enable\n");
					goto lcd_debug_vs_msr_store_next;
				}
				pdrv->vs_msr_cnt = 0;
				pdrv->vs_msr_i = 0;
				pdrv->vs_msr_max = 0;
				pdrv->vs_msr_min = 0xffffffff;
				msr_en = 1; //waiting for behind parameters
			} else {
				pdrv->vs_msr_en = 0;
			}
		}
		if (parm[2]) {
			ret = kstrtouint(parm[2], 10, &temp);
			if (ret) {
				pr_err("invalid data\n");
				goto lcd_debug_vs_msr_store_next;
			}
			pdrv->vs_msr_err_th = temp;
		} else {
			if (pdrv->vs_msr_err_th == 0)
				pdrv->vs_msr_err_th = LCD_VS_MSR_ERR_MAX;
		}
		if (msr_en)
			pdrv->vs_msr_en = 1;
		pr_info("vs_msr en:%d, err_th:%d\n"
			"i:%d, cnt:%d, cnt_max:%d\n",
			pdrv->vs_msr_en, pdrv->vs_msr_err_th,
			pdrv->vs_msr_i, pdrv->vs_msr_cnt, pdrv->vs_msr_cnt_max);
	} else if (strcmp(parm[0], "dump") == 0) {
		lcd_vs_msr_sel = LCD_VS_MSR_DUMP;
	} else if (strcmp(parm[0], "avg") == 0) {
		lcd_vs_msr_sel = LCD_VS_MSR_AVG;
	} else if (strcmp(parm[0], "snapshot") == 0) {
		lcd_vs_msr_sel = LCD_VS_MSR_SNAPSHOT;
	} else {
		LCDERR("invalid command\n");
		kfree(buf_orig);
		return -EINVAL;
	}

lcd_debug_vs_msr_store_next:
	kfree(buf_orig);
	return count;
#undef __MAX_PARAM
}

/****** LCD interface debug file operation func ******/
static const char *lcd_lvds_debug_usage_str = {
"Usage:\n"
"  echo <repack> <dual_port> <pn_swap> <port_swap> <lane_reverse> > lvds\n"
"  echo <vswing> <preem> > phy\n"
};

static const char *lcd_vbyone_debug_usage_str = {
"Usage:\n"
"  echo <lane_count> <region_num> <byte_mode> > vbyone\n"
"  echo <vswing> <preem> > phy\n"
"  echo intr <state> <en> > vbyone\n"
"  echo vintr <en> > vbyone\n"
"  echo ctrl <ctrl_flag> <power_on_reset_delay> <hpd_data_delay> <cdr_training_hold> > vbyone\n"
};

static ssize_t lcd_lvds_debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct lvds_config_s *lvds_conf;

	lvds_conf = &pdrv->config.control.lvds_cfg;

	len += sprintf(buf + len, "lvds config: repack=%d, dual_port=%d,",
		lvds_conf->lvds_repack, lvds_conf->dual_port);
	len += sprintf(buf + len, "pn_swap=%d, port_swap=%d, lane_reverse=%d\n\n",
		lvds_conf->pn_swap, lvds_conf->port_swap, lvds_conf->lane_reverse);
	len += sprintf(buf + len, "%s\n", lcd_lvds_debug_usage_str);

	return len;
}

static ssize_t lcd_lvds_debug_store(struct device *dev, struct device_attribute *attr,
				    const char *buf, size_t count)
{
	int ret = 0;
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct lvds_config_s *lvds_conf;

	lvds_conf = &pdrv->config.control.lvds_cfg;
	ret = sscanf(buf, "%d %d %d %d %d",
		     &lvds_conf->lvds_repack, &lvds_conf->dual_port,
		     &lvds_conf->pn_swap, &lvds_conf->port_swap,
		     &lvds_conf->lane_reverse);
	if (ret == 5 || ret == 4) {
		pr_info("set lvds config:\n"
			"repack=%d, dual_port=%d, pn_swap=%d, port_swap=%d, lane_reverse=%d\n",
			lvds_conf->lvds_repack, lvds_conf->dual_port,
			lvds_conf->pn_swap, lvds_conf->port_swap,
			lvds_conf->lane_reverse);
		lcd_debug_config_update(pdrv);
	} else {
		pr_info("invalid data\n");
		return -EINVAL;
	}

	return count;
}

static ssize_t lcd_vx1_debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct vbyone_config_s *vx1_conf;

	vx1_conf = &pdrv->config.control.vbyone_cfg;

	len += sprintf(buf + len, "vbyone config: lane_count=%d,", vx1_conf->lane_count);
	len += sprintf(buf + len, "region_num=%d, byte_mode=%d\n\n",
		vx1_conf->region_num, vx1_conf->byte_mode);
	len += sprintf(buf + len, "%s\n", lcd_vbyone_debug_usage_str);

	return len;
}

static ssize_t lcd_vx1_debug_store(struct device *dev, struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int ret = 0;
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct vbyone_config_s *vx1_conf;
	int val[5];
	unsigned int offset;

	offset = pdrv->data->offset_venc_if[pdrv->index];

	vx1_conf = &pdrv->config.control.vbyone_cfg;
	if (buf[0] == 'i') { /* intr */
		ret = sscanf(buf, "intr %d %d", &val[0], &val[1]);
		if (ret == 1) {
			pr_info("set vbyone interrupt enable: %d\n", val[0]);
			vx1_conf->intr_state = val[0];
			lcd_vbyone_interrupt_enable(pdrv, vx1_conf->intr_state);
		} else if (ret == 2) {
			pr_info("set vbyone interrupt enable: %d %d\n",
				val[0], val[1]);
			vx1_conf->intr_state = val[0];
			vx1_conf->intr_en = val[1];
			lcd_vbyone_interrupt_enable(pdrv, vx1_conf->intr_state);
		} else {
			pr_info("vx1_intr_enable: %d %d\n",
				vx1_conf->intr_state, vx1_conf->intr_en);
			return -EINVAL;
		}
	} else if (buf[0] == 'v') { /* vintr */
		ret = sscanf(buf, "vintr %d", &val[0]);
		if (ret == 1) {
			pr_info("set vbyone vsync interrupt enable: %d\n", val[0]);
			vx1_conf->vsync_intr_en = val[0];
			lcd_vbyone_interrupt_enable(pdrv, vx1_conf->intr_state);
		} else {
			pr_info("vx1_vsync_intr_enable: %d\n", vx1_conf->vsync_intr_en);
			return -EINVAL;
		}
	} else if (buf[0] == 'c') {
		if (buf[1] == 't') { /* ctrl */
			ret = sscanf(buf, "ctrl %x %d %d %d", &val[0], &val[1], &val[2], &val[3]);
			if (ret == 4) {
				pr_info("set vbyone ctrl_flag: 0x%x\n", val[0]);
				pr_info("power_on_reset_delay: %dms\n", val[1]);
				pr_info("hpd_data_delay: %dms\n", val[2]);
				pr_info("cdr_training_hold: %dms\n", val[3]);
				vx1_conf->ctrl_flag = val[0];
				vx1_conf->power_on_reset_delay = val[1];
				vx1_conf->hpd_data_delay = val[2];
				vx1_conf->cdr_training_hold = val[3];
				lcd_debug_config_update(pdrv);
			} else {
				pr_info("vbyone ctrl_flag: 0x%x\n", vx1_conf->ctrl_flag);
				pr_info("power_on_reset_delay: %dms\n",
					vx1_conf->power_on_reset_delay);
				pr_info("hpd_data_delay: %dms\n", vx1_conf->hpd_data_delay);
				pr_info("cdr_training_hold: %dms\n", vx1_conf->cdr_training_hold);
				return -EINVAL;
			}
		} else if (buf[1] == 'd') { /* cdr */
			lcd_vbyone_debug_cdr(pdrv);
		}
	} else if (buf[0] == 'f') { /* filter */
		ret = sscanf(buf, "filter %x %x", &val[0], &val[1]);
		if (ret == 2) {
			pr_info("set vbyone hw_filter_time: 0x%x, hw_filter_cnt: 0x%x\n",
				val[0], val[1]);
			vx1_conf->hw_filter_time = val[0];
			vx1_conf->hw_filter_cnt = val[1];
			lcd_debug_config_update(pdrv);
		} else {
			pr_info("vbyone hw_filter_time: 0x%x, hw_filter_cnt: 0x%x\n",
				vx1_conf->hw_filter_time, vx1_conf->hw_filter_cnt);
			return -EINVAL;
		}
	} else if (buf[0] == 'r') { /* rst */
		lcd_vbyone_debug_reset(pdrv);
	} else if (buf[0] == 'l') { /* lock */
		lcd_vbyone_debug_lock(pdrv);
	} else {
		ret = sscanf(buf, "%d %d %d", &vx1_conf->lane_count,
			     &vx1_conf->region_num, &vx1_conf->byte_mode);
		if (ret == 3) {
			pr_info("set vbyone config:\n"
				"lane_count=%d, region_num=%d, byte_mode=%d\n",
				vx1_conf->lane_count, vx1_conf->region_num,
				vx1_conf->byte_mode);
			lcd_debug_config_update(pdrv);
		} else {
			pr_info("invalid data\n");
			return -EINVAL;
		}
	}

	return count;
}

static ssize_t lcd_vx1_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	unsigned int offset;

	offset = pdrv->data->offset_venc_if[pdrv->index];
	return sprintf(buf, "vbyone status: lockn = %d hpdn = %d\n",
		       ((lcd_vcbus_read(VBO_STATUS_L + offset) >> 7) & 0x1),
		       ((lcd_vcbus_read(VBO_STATUS_L + offset) >> 6) & 0x1));
}

#ifdef CONFIG_AMLOGIC_LCD_TV
static const char *lcd_mlvds_debug_usage_str = {
"Usage:\n"
"  echo <channel_num> <channel_sel0> <channel_sel1> <clk_phase> <pn_swap> <bit_swap> > minilvds\n"
};

static const char *lcd_p2p_debug_usage_str = {
"Usage:\n"
"  echo <p2p_type> <lane_num> <channel_sel0> <channel_sel1> <pn_swap> <bit_swap> > p2p\n"
};

#define MLVDS_DEBUG_NORMAL     0
#define MLVDS_DEBUG_PHASE      1
#define MLVDS_DEBUG_HELP       2
#define MLVDS_DEBUG_UNKNOWN    3
#define MLVDS_DEBUG_ERR        4
static int lcd_mlvds_debug_type;

static ssize_t lcd_mlvds_debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct mlvds_config_s *mlvds_conf;
	int len = 0;

	mlvds_conf = &pdrv->config.control.mlvds_cfg;
	switch (lcd_mlvds_debug_type) {
	case MLVDS_DEBUG_PHASE:
		len = sprintf(buf, "for_tool:0x%x\n", mlvds_conf->clk_phase);
		break;
	case MLVDS_DEBUG_UNKNOWN:
		len = sprintf(buf, "for_tool:unknown cmd\n");
		break;
	case MLVDS_DEBUG_ERR:
		len = sprintf(buf, "for_tool:error\n");
		break;
	case MLVDS_DEBUG_HELP:
		len = sprintf(buf, "%s\n", lcd_mlvds_debug_usage_str);
		break;
	case MLVDS_DEBUG_NORMAL:
	default:
		len += sprintf(buf + len, "mlvds config: channel_num=%d, ",
			mlvds_conf->channel_num);
		len += sprintf(buf + len, "channel_sel0=0x%08x, channel_sel1=0x%08x, ",
			mlvds_conf->channel_sel0, mlvds_conf->channel_sel1);
		len += sprintf(buf + len, "clk_phase=0x%04x, pn_swap=%d, bit_swap=%d\n\n",
			mlvds_conf->clk_phase, mlvds_conf->pn_swap, mlvds_conf->bit_swap);
		break;
	}

	return len;
}

static ssize_t lcd_mlvds_debug_store(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct mlvds_config_s *mlvds_conf;
	char *buf_orig;
	char **parm = NULL;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return count;
	parm = kcalloc(8, sizeof(char *), GFP_KERNEL);
	if (!parm) {
		kfree(buf_orig);
		return count;
	}

	lcd_debug_parse_param(buf_orig, parm, 8);

	mlvds_conf = &pdrv->config.control.mlvds_cfg;
	if (strcmp(parm[0], "phase") == 0) {
		lcd_mlvds_debug_type = MLVDS_DEBUG_PHASE;
		if (!parm[1])
			goto lcd_mlvds_debug_store_end;
		if (kstrtouint(parm[1], 16, &mlvds_conf->clk_phase))
			goto lcd_mlvds_debug_store_err;
		pr_info("set mlvds clk_phase: 0x%x\n", mlvds_conf->clk_phase);
		pdrv->config.phy_cfg.bypass_resample = (mlvds_conf->clk_phase >> 12) & 1;
		pdrv->config.phy_cfg.act_phy->clk_phase = mlvds_conf->clk_phase & 0xfff;
		lcd_mlvds_clk_phase_set(pdrv);
	} else if (strcmp(parm[0], "help") == 0) {
		lcd_mlvds_debug_type = MLVDS_DEBUG_HELP;
		pr_info("%s\n", lcd_mlvds_debug_usage_str);
	} else if (strcmp(parm[0], "info") == 0) {
		lcd_mlvds_debug_type = MLVDS_DEBUG_NORMAL;
	} else {
		if (!parm[5]) {
			lcd_mlvds_debug_type = MLVDS_DEBUG_UNKNOWN;
			goto lcd_mlvds_debug_store_end;
		}
		lcd_mlvds_debug_type = MLVDS_DEBUG_NORMAL;
		if (kstrtouint(parm[0], 10, &mlvds_conf->channel_num))
			goto lcd_mlvds_debug_store_err;
		if (kstrtouint(parm[1], 16, &mlvds_conf->channel_sel0))
			goto lcd_mlvds_debug_store_err;
		if (kstrtouint(parm[2], 16, &mlvds_conf->channel_sel1))
			goto lcd_mlvds_debug_store_err;
		if (kstrtouint(parm[3], 16, &mlvds_conf->clk_phase))
			goto lcd_mlvds_debug_store_err;
		if (kstrtouint(parm[4], 10, &mlvds_conf->pn_swap))
			goto lcd_mlvds_debug_store_err;
		if (kstrtouint(parm[5], 10, &mlvds_conf->bit_swap))
			goto lcd_mlvds_debug_store_err;
		pr_info("set mlvds config:\n"
			"channel_num=%d,\n"
			"channel_sel0=0x%08x, channel_sel1=0x%08x,\n"
			"clk_phase=0x%04x,\n"
			"pn_swap=%d, bit_swap=%d\n",
			mlvds_conf->channel_num,
			mlvds_conf->channel_sel0, mlvds_conf->channel_sel1,
			mlvds_conf->clk_phase,
			mlvds_conf->pn_swap, mlvds_conf->bit_swap);
		pdrv->config.phy_cfg.bypass_resample = (mlvds_conf->clk_phase >> 12) & 1;
		pdrv->config.phy_cfg.act_phy->clk_phase = mlvds_conf->clk_phase & 0xfff;
		lcd_mlvds_phy_ckdi_config(pdrv);
		lcd_debug_config_update(pdrv);
	}

lcd_mlvds_debug_store_end:
	kfree(buf_orig);
	kfree(parm);
	return count;

lcd_mlvds_debug_store_err:
	LCDERR("%s: 0x%x parameter fail\n", __func__, lcd_mlvds_debug_type);
	lcd_mlvds_debug_type = MLVDS_DEBUG_ERR;
	kfree(buf_orig);
	kfree(parm);
	return count;
}

static ssize_t lcd_p2p_debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct p2p_config_s *p2p_conf;

	p2p_conf = &pdrv->config.control.p2p_cfg;

	len += sprintf(buf + len, "p2p config: p2p_type=0x%x, lane_num=%d, ",
		p2p_conf->p2p_type, p2p_conf->lane_num);
	len += sprintf(buf + len, "channel_sel0=0x%08x, channel_sel1=0x%08x, ",
		p2p_conf->channel_sel0, p2p_conf->channel_sel1);
	len += sprintf(buf + len, "pn_swap=%d, bit_swap=%d\n\n",
		p2p_conf->pn_swap, p2p_conf->bit_swap);
	len += sprintf(buf + len, "%s\n", lcd_p2p_debug_usage_str);

	return len;
}

static ssize_t lcd_p2p_debug_store(struct device *dev, struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int ret = 0;
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct p2p_config_s *p2p_conf;

	p2p_conf = &pdrv->config.control.p2p_cfg;
	ret = sscanf(buf, "%x %d %x %x %d %d",
		     &p2p_conf->p2p_type, &p2p_conf->lane_num,
		     &p2p_conf->channel_sel0, &p2p_conf->channel_sel1,
		     &p2p_conf->pn_swap, &p2p_conf->bit_swap);
	if (ret == 6) {
		pr_info("set p2p config:\n"
			"p2p_type=0x%x, lane_num=%d,\n"
			"channel_sel0=0x%08x, channel_sel1=0x%08x,\n"
			"pn_swap=%d, bit_swap=%d\n",
			p2p_conf->p2p_type, p2p_conf->lane_num,
			p2p_conf->channel_sel0, p2p_conf->channel_sel1,
			p2p_conf->pn_swap, p2p_conf->bit_swap);
		lcd_debug_config_update(pdrv);
	} else {
		pr_info("invalid data\n");
		return -EINVAL;
	}

	return count;
}
#endif

#ifdef CONFIG_AMLOGIC_LCD_TABLET
static const char *lcd_rgb_debug_usage_str = {
"Usage:\n"
"  echo <type> <clk_pol> <de_valid> <sync_valid> <rb_swpa> <bit_swap> > rgb\n"
};

static const char *lcd_mipi_debug_usage_str = {
"Usage:\n"
"  echo <lane_num> <bit_rate> 0 <mode_init> <mode_disp> <vid_mode_type> <clk_always_hs> 0 > mipi\n"
};

static const char *lcd_mipi_cmd_debug_usage_str = {
"Usage:\n"
"  echo <data_type> <N> <data0> <data1> <data2> ...... <dataN-1> > mpcmd\n"
};

static const char *lcd_edp_debug_usage_str = {
"Usage:\n"
"  echo <lane_cnt_max> <link_rate_max> <training_mode> > edp\n"
};

static ssize_t lcd_rgb_debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct rgb_config_s *rgb_conf;

	rgb_conf = &pdrv->config.control.rgb_cfg;

	len += sprintf(buf + len,
		"rgb config: type=%d, clk_pol=%d, de_valid=%d, sync_valid=%d,",
		rgb_conf->type, rgb_conf->clk_pol,
		rgb_conf->de_valid, rgb_conf->sync_valid);
	len += sprintf(buf + len, "rb_swap=%d, bit_swap=%d\n\n",
		rgb_conf->rb_swap, rgb_conf->bit_swap);
	len += sprintf(buf + len, "%s\n", lcd_rgb_debug_usage_str);

	return len;
}

static ssize_t lcd_rgb_debug_store(struct device *dev, struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int ret = 0;
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct rgb_config_s *rgb_conf;
	unsigned int temp[6];

	rgb_conf = &pdrv->config.control.rgb_cfg;
	ret = sscanf(buf, "%d %d %d %d %d %d",
		     &temp[0], &temp[1], &temp[2], &temp[3], &temp[4], &temp[5]);
	if (ret == 5) {
		pr_info("set rgb config:\n"
		"type=%d, clk_pol=%d, de_valid=%d, sync_valid=%d, rb_swap=%d, bit_swap=%d\n",
			temp[0], temp[1], temp[2], temp[3], temp[4], temp[5]);
		rgb_conf->type = temp[0];
		rgb_conf->clk_pol = temp[1];
		rgb_conf->de_valid = temp[2];
		rgb_conf->sync_valid = temp[3];
		rgb_conf->rb_swap = temp[4];
		rgb_conf->bit_swap = temp[5];
		lcd_debug_config_update(pdrv);
	} else {
		pr_info("invalid data\n");
		return -EINVAL;
	}

	return count;
}

static ssize_t lcd_bt_debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct bt_config_s *bt_conf;
	int len = 0;

	bt_conf = &pdrv->config.control.bt_cfg;

	len += sprintf(buf + len, "bt config: clk_phase=%d, field_type=%d, mode_422=%d,",
		bt_conf->clk_phase, bt_conf->field_type, bt_conf->mode_422);
	len += sprintf(buf + len, "yc_swap=%d, cbcr_swap=%d\n\n",
		bt_conf->yc_swap, bt_conf->cbcr_swap);

	return len;
}

static ssize_t lcd_bt_debug_store(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct bt_config_s *bt_conf;
	unsigned int temp[5];
	int ret = 0;

	bt_conf = &pdrv->config.control.bt_cfg;
	ret = sscanf(buf, "%d %d %d %d %d", &temp[0], &temp[1], &temp[2], &temp[3], &temp[4]);
	if (ret == 5) {
		pr_info("set bt config:\n"
			"clk_phase=%d, field_type=%d, mode_422=%d\n"
			"yc_swap=%d, cbcr_swap=%d\n",
			temp[0], temp[1], temp[2], temp[3], temp[4]);
		bt_conf->clk_phase = temp[0];
		bt_conf->field_type = temp[1];
		bt_conf->mode_422 = temp[2];
		bt_conf->yc_swap = temp[3];
		bt_conf->cbcr_swap = temp[4];
		lcd_debug_config_update(pdrv);
	} else {
		pr_info("invalid data\n");
		return -EINVAL;
	}

	return count;
}

static ssize_t lcd_mipi_debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	lcd_dsi_info_print(&pdrv->config);

	len = sprintf(buf, "%s\n", lcd_mipi_debug_usage_str);

	return len;
}

static ssize_t lcd_mipi_debug_store(struct device *dev, struct device_attribute *attr,
				    const char *buf, size_t count)
{
	int ret = 0;
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct dsi_config_s *dsi_conf;
	int val[8];

	dsi_conf = &pdrv->config.control.mipi_cfg;
	ret = sscanf(buf, "%d %d %d %d %d %d %d %d",
		     &val[0], &val[1], &val[2], &val[3],
		     &val[4], &val[5], &val[6], &val[7]);
	if (ret >= 2) {
		dsi_conf->lane_num = (unsigned char)val[0];
		dsi_conf->bit_rate_max = val[1];
		dsi_conf->operation_mode_init = (unsigned char)val[3];
		dsi_conf->operation_mode_display = (unsigned char)val[4];
		dsi_conf->video_mode_type = (unsigned char)val[5];
		dsi_conf->clk_always_hs = (unsigned char)val[6];
		pr_info("set mipi_dsi config:\n"
			"  lane_num=%d,\n"
			"  bit_rate_max=%dMhz,\n"
			"  operation_mode_init=%d,\n"
			"  operation_mode_display=%d\n"
			"  video_mode_type=%d\n"
			"  clk_always_hs=%d\n\n",
			dsi_conf->lane_num,
			dsi_conf->bit_rate_max,
			dsi_conf->operation_mode_init,
			dsi_conf->operation_mode_display,
			dsi_conf->video_mode_type,
			dsi_conf->clk_always_hs);
		lcd_debug_config_update(pdrv);
	} else {
		pr_info("invalid data\n");
		return -EINVAL;
	}

	return count;
}

static ssize_t lcd_mipi_cmd_debug_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", lcd_mipi_cmd_debug_usage_str);
}

static ssize_t lcd_mipi_cmd_debug_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	unsigned char wr_c[24];
	int ret = 0;

	ret = sscanf(buf,
		"%hhx %hhu %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx"
		"%hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx",
		&wr_c[0],  &wr_c[1],  &wr_c[2],  &wr_c[3],  &wr_c[4],  &wr_c[5],
		&wr_c[6],  &wr_c[7],  &wr_c[8],  &wr_c[9],  &wr_c[10], &wr_c[11],
		&wr_c[12], &wr_c[13], &wr_c[14], &wr_c[15], &wr_c[16], &wr_c[17],
		&wr_c[18], &wr_c[19], &wr_c[20], &wr_c[21], &wr_c[22], &wr_c[23]);
	if (ret < 2) {
		pr_info("invalid data\n");
		return count;
	}

	lcd_dsi_write_cmd(pdrv, &wr_c[0]);

	return count;
}

u8 wr_c[24] = {0};

static ssize_t lcd_mipi_read_debug_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	u8 rd_c[4] = {0, 0, 0, 0};
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	int temp = 0, offset;

	if (!wr_c[0])
		return sprintf(buf, "lcd[%d]: mipi dsi read command buffer empty\n", pdrv->index);

	temp = lcd_dsi_read(pdrv, &wr_c[0], &rd_c[0], 4);

	if (temp <= 0 || temp > 4)
		return sprintf(buf, "lcd[%d]: mipi dsi read failed\n", pdrv->index);

	offset = sprintf(buf, "lcd[%d] mipi dsi read %d: ", pdrv->index, temp);
		while (temp) {
			offset += sprintf(buf + offset, "0x%02x ", rd_c[temp - 1]);
			temp--;
		}
	offset += sprintf(buf + offset - 1, "\n");

	return offset - 1;
}

static ssize_t lcd_mipi_read_debug_store(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t count)
{
	int ret;

	memset(wr_c, 0, sizeof(u8) * 24);

	ret = sscanf(buf, "%hhx %hhu %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx"
		"%hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx",
		&wr_c[0],  &wr_c[1],  &wr_c[2],  &wr_c[3],  &wr_c[4],  &wr_c[5],
		&wr_c[6],  &wr_c[7],  &wr_c[8],  &wr_c[9],  &wr_c[10], &wr_c[11],
		&wr_c[12], &wr_c[13], &wr_c[14], &wr_c[15], &wr_c[16], &wr_c[17],
		&wr_c[18], &wr_c[19], &wr_c[20], &wr_c[21], &wr_c[22], &wr_c[23]);
	if (ret < 2)
		pr_info("invalid data\n");

	return count;
}

static ssize_t lcd_mipi_state_debug_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	unsigned int state_save, offset;

	offset = pdrv->data->offset_venc_if[pdrv->index];
	state_save = lcd_vcbus_getb(L_VCOM_VS_ADDR + offset, 12, 1);

	return sprintf(buf, "state: %d, check_en: %d, state_save: %d\n",
		pdrv->config.control.mipi_cfg.check_state,
		pdrv->config.control.mipi_cfg.check_en,
		state_save);
}

static ssize_t lcd_mipi_mode_debug_store(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	unsigned int temp;
	unsigned char mode;
	int ret = 0;

	if ((pdrv->status & LCD_STATUS_IF_ON) == 0) {
		LCDERR("panel is disabled\n");
		return count;
	}

	ret = kstrtouint(buf, 10, &temp);
	if (ret) {
		pr_info("invalid data\n");
		return -EINVAL;
	}
	mode = (unsigned char)temp;
	lcd_dsi_set_operation_mode(pdrv, mode);

	return count;
}

static ssize_t lcd_mipi_dphy_debug_store(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	unsigned int temp;

	if ((pdrv->status & LCD_STATUS_IF_ON) == 0) {
		LCDERR("lcd is disabled\n");
		return count;
	}

	temp = (buf[0] == 'H') ? 0x10 : 0x00; //HS or LP
	if (buf[3] == 'H') //HIGH
		temp |= 0x0;
	if (buf[3] == 'L') //LOW
		temp |= 0x1;
	if (buf[3] == 'P' && buf[7] == '7') //PRBS7
		temp |= 0x2;
	if (buf[3] == 'P' && buf[7] == '1' && buf[8] == '3') //PRBS13
		temp |= 0x3;
	if (buf[3] == 'P' && buf[7] == '1' && buf[8] == '5') //PRBS15
		temp |= 0x4;

	lcd_dsi_dphy_test(pdrv, temp);

	return count;
}

static ssize_t lcd_edp_debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct edp_config_s *edp_conf;
	int len = 0;

	edp_conf = &pdrv->config.control.edp_cfg;

	len += sprintf(buf + len, "edp config: max_lane_count=%d, ",
		edp_conf->max_lane_count);
	len += sprintf(buf + len, "max_link_rate=%dMhz, training_mode=%d, ",
		edp_conf->max_link_rate, edp_conf->training_mode);
	len += sprintf(buf + len, "%s\n", lcd_edp_debug_usage_str);

	return len;
}

static ssize_t lcd_edp_debug_store(struct device *dev, struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct edp_config_s *edp_conf;
	unsigned int val[3];
	unsigned char cmd[20];
	int ret = 0;

	edp_conf = &pdrv->config.control.edp_cfg;
	ret = sscanf(buf, "%d %d %d", &val[0], &val[1], &val[2]);
	if (ret == 3) {
		edp_conf->max_lane_count = (unsigned char)val[0];
		edp_conf->max_link_rate = (unsigned char)val[1];
		edp_conf->training_mode = (unsigned char)val[2];
		pr_info("set edp config:\n"
			"lane_num=%d, bit_rate_max=%dMhz, factor_numerator=%d\n\n",
			edp_conf->max_lane_count,
			edp_conf->max_link_rate,
			edp_conf->training_mode);
		lcd_debug_config_update(pdrv);
		return count;
	}
	if (count > 19) {
		LCDERR("eDP debug cmd error\n");
		return count;
	}

	ret = sscanf(buf, "%s %d", cmd, &val[0]);
	if (ret == 2)
		edp_debug_test(pdrv, cmd, val[0]);
	else
		edp_debug_test(pdrv, cmd, -1);
	return count;
}

static ssize_t lcd_edp_dpcd_debug_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	dptx_DPCD_dump(pdrv);
	return count;
}

static ssize_t lcd_edp_edid_debug_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	unsigned char len;
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	dptx_EDID_dump(pdrv);
	len = sprintf(buf, "edid_en: %d\n", pdrv->config.control.edp_cfg.edid_en);
	return len;
}
#endif

/****** LCD interface debug file operation func Done ******/
//[24:31]: vsw_sort/lane_idx;
//[8:23]: reserved;
//[0:7]: debug_cmd
#define PHY_DEBUG_INFO          0
#define PHY_DEBUG_VSWING        1
#define PHY_DEBUG_VCM           2
#define PHY_DEBUG_ODT           3
#define PHY_DEBUG_REF_BIAS      4
#define PHY_DEBUG_CV_MODE       5
#define PHY_DEBUG_LANE_OP       6
#define PHY_DEBUG_LANE_EN       7
#define PHY_DEBUG_LANE_PREEM    8
#define PHY_DEBUG_LANE_AMP      9
#define PHY_DEBUG_LANE_PHASE    10
#define PHY_DEBUG_LANE_SEL      11
#define PHY_DEBUG_STATE         12
#define PHY_DEBUG_LCD_IF        13
#define PHY_DEBUG_PHY_CLK       14
#define PHY_DEBUG_UNKNOWN       15
#define PHY_DEBUG_ERR           16
static unsigned int phy_debug_type;

static ssize_t lcd_phy_debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct phy_attr_s local_phy;
	struct phy_config_s local_phy_cfg;
	ssize_t len = 0;
	unsigned int phy_clk;
	unsigned short i = (((phy_debug_type >> 24) & 0xff) >= CH_LANE_MAX) ?
				0 : ((phy_debug_type >> 24) & 0xff);
	int ret;

	ret = lcd_phy_param_get(pdrv, &local_phy_cfg, &local_phy);
	if (ret)
		return sprintf(buf, "for_tool:error, phy_param_get error\n");

	switch (phy_debug_type & 0xff) {
	case PHY_DEBUG_VSWING:
		len = sprintf(buf, "for_tool:0x%x\n", local_phy.vswing);
		break;
	case PHY_DEBUG_VCM:
		len = sprintf(buf, "for_tool:0x%x\n", local_phy.vcm);
		break;
	case PHY_DEBUG_ODT:
		len = sprintf(buf, "for_tool:0x%x\n", local_phy.odt);
		break;
	case PHY_DEBUG_REF_BIAS:
		len = sprintf(buf, "for_tool:%u\n", local_phy.ref_bias);
		break;
	case PHY_DEBUG_CV_MODE:
		len = sprintf(buf, "for_tool:%u\n", local_phy.cv_mode);
		break;
	case PHY_DEBUG_LANE_PREEM:
		len = sprintf(buf, "for_tool:0x%x\n", local_phy.lane[i].preem);
		break;
	case PHY_DEBUG_LANE_AMP:
		len = sprintf(buf, "for_tool:0x%x\n", local_phy.lane[i].amp);
		break;
	case PHY_DEBUG_LANE_SEL:
		len = sprintf(buf, "for_tool:0x%x\n", local_phy_cfg.ch_ctrl[i].sel);
		break;
	case PHY_DEBUG_LANE_EN:
		len = sprintf(buf, "for_tool:%u\n", local_phy_cfg.ch_ctrl[i].en);
		break;
	case PHY_DEBUG_LANE_OP:
		len = sprintf(buf, "for_tool:sel=0x%x, amp=0x%x, preem=0x%x, en=%d\n",
			local_phy_cfg.ch_ctrl[i].sel, local_phy.lane[i].amp,
			local_phy.lane[i].preem, local_phy_cfg.ch_ctrl[i].en);
		break;
	case PHY_DEBUG_LANE_PHASE:
		if (local_phy_cfg.ch_ctrl[i].phase_sel == 0xff)
			len = sprintf(buf, "for_tool:x\n");
		else
			len = sprintf(buf, "for_tool:%x\n", local_phy_cfg.ch_ctrl[i].phase_sel);
		break;
	case PHY_DEBUG_STATE:
		len = sprintf(buf, "for_tool:%u\n", local_phy_cfg.state);
		break;
	case PHY_DEBUG_LCD_IF:
		len = sprintf(buf, "for_tool:%s\n",
			lcd_type_type_to_str(pdrv->config.basic.lcd_type));
		break;
	case PHY_DEBUG_PHY_CLK:
		phy_clk = lcd_do_div(pdrv->config.timing.bit_rate, 1000000);
		len = sprintf(buf, "for_tool:%u\n", phy_clk);
		break;
	case PHY_DEBUG_UNKNOWN:
		len = sprintf(buf, "for_tool:unknown cmd\n");
		break;
	case PHY_DEBUG_ERR:
		len = sprintf(buf, "for_tool:error\n");
		break;
	case PHY_DEBUG_INFO:
	default:
		len = lcd_phy_param_print(pdrv, buf, 0);
		break;
	}
	return len;
}

static ssize_t lcd_phy_debug_store(struct device *dev, struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	union lcd_ctrl_config_u *pctrl = &pdrv->config.control;
	struct phy_config_s *phy_cfg = &pdrv->config.phy_cfg;
	struct phy_attr_s *phy = pdrv->config.phy_cfg.act_phy;
	unsigned int para[4], set_val = 0xffffffff;
	unsigned char op_lane = 0, op_port;
	int i = 0;
	char *buf_orig;
	char **parm = NULL;

	if (!phy)
		return count;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return count;
	parm = kcalloc(16, sizeof(char *), GFP_KERNEL);
	if (!parm) {
		kfree(buf_orig);
		return count;
	}

	lcd_debug_parse_param(buf_orig, parm, 16);

	if (strcmp(parm[0], "lane") == 0) {
		phy_debug_type = PHY_DEBUG_LANE_OP;
		if (!parm[1])
			goto lcd_phy_debug_store_err;
		if (kstrtou8(parm[1], 10, &op_lane))
			goto lcd_phy_debug_store_err;
		if (op_lane >= CH_LANE_MAX && op_lane != 0xff)
			goto lcd_phy_debug_store_err;

		if (!parm[2]) {
			phy_debug_type = ((op_lane & 0xff) << 24) | PHY_DEBUG_LANE_OP;
			goto lcd_phy_debug_store_end;
		}

		if (strcmp(parm[2], "en") == 0) {
			phy_debug_type = ((op_lane & 0xff) << 24) | PHY_DEBUG_LANE_EN;
			if (!parm[3])
				goto lcd_phy_debug_store_end;
			if (kstrtou32(parm[3], 10, &set_val))
				goto lcd_phy_debug_store_err;
			if (op_lane == 0xff)
				goto lcd_phy_debug_store_err;
			phy_cfg->ch_ctrl[op_lane].en = set_val ? 1 : 0;
			pr_info("LCD PHY set: Lane[%u]: %s\n", op_lane,
				phy_cfg->ch_ctrl[op_lane].en ? "on" : "off");
		} else if (strcmp(parm[2], "amp") == 0) {
			phy_debug_type = ((op_lane & 0xff) << 24) | PHY_DEBUG_LANE_AMP;
			if (!parm[3])
				goto lcd_phy_debug_store_end;
			if (kstrtou32(parm[3], 16, &set_val))
				goto lcd_phy_debug_store_err;
			if (op_lane == 0xff) { //write all lane
				for (i = 0; i < phy_cfg->lane_num; i++)
					phy->lane[i].amp = set_val;
			} else {
				phy->lane[op_lane].amp = set_val;
			}
			phy_cfg->flag |= PHY_BIT_LANE_AMP;
			pr_info("LCD PHY set: Lane[%u]: Amp=0x%02x\n", op_lane, set_val);
		} else if (strcmp(parm[2], "preem") == 0) {
			phy_debug_type = ((op_lane & 0xff) << 24) | PHY_DEBUG_LANE_PREEM;
			if (!parm[3])
				goto lcd_phy_debug_store_end;
			if (kstrtou32(parm[3], 16, &set_val))
				goto lcd_phy_debug_store_err;
			if (op_lane == 0xff) { //write all lane
				for (i = 0; i < phy_cfg->lane_num; i++)
					phy->lane[i].preem = set_val;
			} else {
				phy->lane[op_lane].preem = set_val;
			}
			phy_cfg->flag |= PHY_BIT_LANE_PREEM;
			pr_info("LCD PHY set: Lane[%u]: PreEm=0x%02x\n", op_lane, set_val);
		} else if (strcmp(parm[2], "phase") == 0) {
			phy_debug_type = ((op_lane & 0xff) << 24) | PHY_DEBUG_LANE_PHASE;
			if (!parm[3])
				goto lcd_phy_debug_store_end;
			if (kstrtou32(parm[3], 16, &set_val))
				goto lcd_phy_debug_store_err;
			if (!lcd_phy_support_lane_phase(pdrv)) {
				LCDERR("not support lane phase select\n");
				goto lcd_phy_debug_store_err;
			}
			if (set_val != PHY_PHASE_0 &&
					set_val != PHY_PHASE_A &&
					set_val != PHY_PHASE_B) {
				LCDERR("Invalid phase sel=%d\n", set_val);
				goto lcd_phy_debug_store_err;
			}
			if (op_lane == 0xff) { //write all lane
				for (i = 0; i < phy_cfg->lane_num; i++)
					phy_cfg->ch_ctrl[i].phase_sel = set_val;
			} else {
				phy_cfg->ch_ctrl[op_lane].phase_sel = set_val;
			}
			pr_info("LCD PHY set: Lane[%u]: Phase=0x%02x\n", op_lane, set_val);
		} else if (strcmp(parm[2], "sel") == 0) {
			phy_debug_type = ((op_lane & 0xff) << 24) | PHY_DEBUG_LANE_SEL;
			if (!parm[3])
				goto lcd_phy_debug_store_end;
			if (kstrtou32(parm[3], 16, &set_val))
				goto lcd_phy_debug_store_err;
			if (op_lane == 0xff)
				goto lcd_phy_debug_store_err;
			phy_cfg->ch_ctrl[op_lane].sel = set_val;
			phy_cfg->flag |= PHY_BIT_LANE_SEL;
			pr_info("LCD PHY set: Lane[%u]: sel=0x%x\n", op_lane, set_val);
			lcd_lane_map_update(pdrv);
			lcd_lane_map_set(pdrv);
			//mlvds need set phy reg for ckdi update
			if (pdrv->config.basic.lcd_type != LCD_MLVDS)
				goto lcd_phy_debug_store_end;
		} else {
			if (!parm[3]) {
				phy_debug_type = PHY_DEBUG_UNKNOWN;
				goto lcd_phy_debug_store_end;
			}
			phy_debug_type = ((op_lane & 0xff) << 24) | PHY_DEBUG_LANE_OP;
			if (kstrtou32(parm[2], 16, &para[1]))
				goto lcd_phy_debug_store_err;
			if (kstrtou32(parm[3], 16, &para[2]))
				goto lcd_phy_debug_store_err;
			if (op_lane == 0xff) {
				for (i = 0; i < phy_cfg->lane_num; i++) {
					phy->lane[i].amp = para[1];
					phy->lane[i].preem = para[2];
				}
			} else {
				phy->lane[op_lane].amp = para[1];
				phy->lane[op_lane].preem = para[2];
			}
			phy_cfg->flag |= (PHY_BIT_LANE_PREEM | PHY_BIT_LANE_AMP);
			pr_info("LCD PHY set: Lane[%u]: Amp=0x%02x, PreEm=0x%02x\n",
				op_lane, para[1], para[2]);
		}
	} else if (strcmp(parm[0], "vcm") == 0) {
		phy_debug_type = PHY_DEBUG_VCM;
		if (!parm[1])
			goto lcd_phy_debug_store_end;
		if (kstrtou32(parm[1], 16, &set_val))
			goto lcd_phy_debug_store_err;
		phy->vcm = set_val;
		phy_cfg->flag |= PHY_BIT_VCM;
		pr_info("LCD PHY set: VCM=0x%02x\n", set_val);
	} else if (strcmp(parm[0], "vswing") == 0) {
		phy_debug_type = PHY_DEBUG_VSWING;
		if (!parm[1])
			goto lcd_phy_debug_store_end;
		if (parm[2]) {
			if (kstrtou8(parm[1], 10, &op_port))
				goto lcd_phy_debug_store_err;
			if (kstrtou32(parm[2], 16, &set_val))
				goto lcd_phy_debug_store_err;
			phy_debug_type |= ((op_port & 0xff) << 24);
			pr_info("LCD PHY set: Vswing[%d]=0x%02x\n", op_port, set_val);
		} else {
			if (kstrtou32(parm[1], 16, &set_val))
				goto lcd_phy_debug_store_err;
			pr_info("LCD PHY set: Vswing=0x%02x\n", set_val);
		}
		phy->vswing = set_val;
		phy_cfg->flag |= PHY_BIT_VSWING;
	} else if (strcmp(parm[0], "odt") == 0) {
		phy_debug_type = PHY_DEBUG_ODT;
		if (!parm[1])
			goto lcd_phy_debug_store_end;
		if (kstrtou32(parm[1], 16, &set_val))
			goto lcd_phy_debug_store_err;
		phy->odt = set_val;
		phy_cfg->flag |= PHY_BIT_ODT;
		pr_info("LCD PHY set: odt=0x%03x\n", set_val);
	} else if (strcmp(parm[0], "ref_bias") == 0) {
		phy_debug_type = PHY_DEBUG_REF_BIAS;
		if (!parm[1])
			goto lcd_phy_debug_store_end;
		if (kstrtou32(parm[1], 16, &set_val))
			goto lcd_phy_debug_store_err;
		phy->ref_bias = set_val ? 1 : 0;
		phy_cfg->flag |= PHY_BIT_REF_BIAS;
		pr_info("LCD PHY set: ref_bias=%d\n", set_val);
	} else if (strcmp(parm[0], "mode") == 0) {
		phy_debug_type = PHY_DEBUG_CV_MODE;
		if (!parm[1])
			goto lcd_phy_debug_store_end;
		if (kstrtou32(parm[1], 16, &set_val))
			goto lcd_phy_debug_store_err;
		phy->cv_mode = set_val ? 1 : 0;
		phy_cfg->flag |= PHY_BIT_CV_MODE;
		pr_info("LCD PHY set: cv_mode=%d\n", set_val);
	} else if (strcmp(parm[0], "on") == 0) {
		phy_debug_type = PHY_DEBUG_STATE;
		lcd_phy_set(pdrv, LCD_PHY_ON);
		pr_info("LCD PHY: on\n");
		goto lcd_phy_debug_store_end;
	} else if (strcmp(parm[0], "off") == 0) {
		phy_debug_type = PHY_DEBUG_STATE;
		pr_info("LCD PHY: off\n");
		lcd_phy_set(pdrv, LCD_PHY_OFF);
		goto lcd_phy_debug_store_end;
	} else if (strcmp(parm[0], "state") == 0) {
		phy_debug_type = PHY_DEBUG_STATE;
		goto lcd_phy_debug_store_end;
	} else if (strcmp(parm[0], "info") == 0) {
		phy_debug_type = PHY_DEBUG_INFO;
		goto lcd_phy_debug_store_end;
	} else if (strcmp(parm[0], "lcd_if") == 0) {
		phy_debug_type = PHY_DEBUG_LCD_IF;
		goto lcd_phy_debug_store_end;
	} else if (strcmp(parm[0], "phy_clk") == 0) {
		phy_debug_type = PHY_DEBUG_PHY_CLK;
		goto lcd_phy_debug_store_end;
	} else {
		if (!parm[1]) {
			phy_debug_type = PHY_DEBUG_UNKNOWN;
			goto lcd_phy_debug_store_end;
		}
		phy_debug_type = PHY_DEBUG_INFO;
		if (kstrtou32(parm[0], 16, &para[0]))
			goto lcd_phy_debug_store_err;
		if (kstrtou32(parm[1], 16, &para[1]))
			goto lcd_phy_debug_store_err;
		switch (pdrv->config.basic.lcd_type) {
		case LCD_LVDS:
			pctrl->lvds_cfg.phy_vswing = para[0];
			pctrl->lvds_cfg.phy_preem = para[1];
			break;
		case LCD_VBYONE:
			pctrl->vbyone_cfg.phy_vswing = para[0];
			pctrl->vbyone_cfg.phy_preem = para[1];
			break;
		case LCD_MLVDS:
			pctrl->mlvds_cfg.phy_vswing = para[0];
			pctrl->mlvds_cfg.phy_preem = para[1];
			break;
		case LCD_P2P:
			pctrl->p2p_cfg.phy_vswing = para[0];
			pctrl->p2p_cfg.phy_preem = para[1];
			break;
		case LCD_EDP:
			pctrl->edp_cfg.phy_vswing_preset = para[0];
			pctrl->edp_cfg.phy_preem_preset = para[1];
			break;
		default:
			break;
		}
		phy_cfg->vswing_level = para[0] & 0xf;
		phy_cfg->ext_pullup = (para[0] >> 4) & 0x3;
		phy_cfg->preem_level = para[1];
		phy->vswing = lcd_phy_vswing_level_to_value(pdrv, phy_cfg->vswing_level);
		para[2] = lcd_phy_preem_level_to_value(pdrv, phy_cfg->preem_level);
		for (i = 0; i < phy_cfg->lane_num; i++)
			phy->lane[i].preem = para[2];
		pr_info("LCD PHY set: global Vswing_level=0x%02x, PreEm_level=0x%02x\n",
			para[0], para[1]);
	}

	if (pdrv->status & LCD_STATUS_IF_ON)
		lcd_phy_set(pdrv, LCD_PHY_ON);

lcd_phy_debug_store_end:
	kfree(parm);
	kfree(buf_orig);
	return count;

lcd_phy_debug_store_err:
	LCDERR("%s: 0x%x fail\n", __func__, phy_debug_type);
	phy_debug_type = PHY_DEBUG_ERR;
	kfree(parm);
	kfree(buf_orig);
	return count;
}

/***** LCD debug file operation ******/
static struct device_attribute lcd_debug_attrs[] = {
	__ATTR(help,        0444, lcd_debug_common_help, NULL),
	__ATTR(debug,       0644, lcd_debug_show, lcd_debug_store),
	__ATTR(change,      0644, lcd_debug_change_show, lcd_debug_change_store),
	__ATTR(enable,      0644, lcd_debug_enable_show, lcd_debug_enable_store),
	__ATTR(resume_type, 0644, lcd_debug_resume_show, lcd_debug_resume_store),
	__ATTR(power_on,    0644, lcd_debug_power_show, lcd_debug_power_store),
	__ATTR(power_step,  0644, lcd_debug_power_step_show, lcd_debug_power_step_store),
	__ATTR(frame_rate,  0644, lcd_debug_frame_rate_show, lcd_debug_frame_rate_store),
	__ATTR(fr_flag,     0644, lcd_debug_fr_flag_show, lcd_debug_fr_flag_store),
	__ATTR(ss,          0644, lcd_debug_ss_show, lcd_debug_ss_store),
	__ATTR(clk,         0644, lcd_debug_clk_show, lcd_debug_clk_store),
	__ATTR(test,        0644, lcd_debug_test_show, lcd_debug_test_store),
	__ATTR(mute,        0644, lcd_debug_mute_show, lcd_debug_mute_store),
	__ATTR(mute_count,  0644, lcd_debug_mute_cnt_show, lcd_debug_mute_cnt_store),
	__ATTR(unmute_count,  0644, lcd_debug_unmute_cnt_show, lcd_debug_unmute_cnt_store),
	__ATTR(prbs,        0644, lcd_debug_prbs_show, lcd_debug_prbs_store),
	__ATTR(reg,         0200, NULL, lcd_debug_reg_store),
	__ATTR(vlock,       0444, lcd_debug_vlock_show, NULL),
	__ATTR(time,        0444, lcd_proc_time_show, NULL),
	__ATTR(dump,        0644, lcd_debug_dump_show, lcd_debug_dump_store),
	__ATTR(print,       0644, lcd_debug_print_show, lcd_debug_print_store),
	__ATTR(cus_ctrl,    0200, NULL, lcd_debug_cus_ctrl_store),
	__ATTR(vinfo,       0444, lcd_debug_vinfo_show, NULL),
	__ATTR(sw_vlock,    0644, lcd_debug_sw_vlock_show, lcd_debug_sw_vlock_store),
	__ATTR(vs_msr,      0644, lcd_debug_vs_msr_show, lcd_debug_vs_msr_store)
};

static struct device_attribute lcd_debug_attrs_lvds[] = {
	__ATTR(lvds,   0644, lcd_lvds_debug_show, lcd_lvds_debug_store),
	__ATTR(phy,    0644, lcd_phy_debug_show, lcd_phy_debug_store),
	__ATTR(null,   0644, NULL, NULL)
};

static struct device_attribute lcd_debug_attrs_vbyone[] = {
	__ATTR(vbyone, 0644, lcd_vx1_debug_show, lcd_vx1_debug_store),
	__ATTR(phy,    0644, lcd_phy_debug_show, lcd_phy_debug_store),
	__ATTR(status, 0444, lcd_vx1_status_show, NULL),
	__ATTR(null,   0644, NULL, NULL)
};

#ifdef CONFIG_AMLOGIC_LCD_TABLET
static struct device_attribute lcd_debug_attrs_rgb[] = {
	__ATTR(rgb,    0644, lcd_rgb_debug_show, lcd_rgb_debug_store),
	__ATTR(null,   0644, NULL, NULL)
};

static struct device_attribute lcd_debug_attrs_bt[] = {
	__ATTR(bt,     0644, lcd_bt_debug_show, lcd_bt_debug_store),
	__ATTR(null,   0644, NULL, NULL)
};

static struct device_attribute lcd_debug_attrs_mipi[] = {
	__ATTR(mipi,    0644, lcd_mipi_debug_show,       lcd_mipi_debug_store),
	__ATTR(mpcmd,   0644, lcd_mipi_cmd_debug_show,   lcd_mipi_cmd_debug_store),
	__ATTR(mpread,  0644, lcd_mipi_read_debug_show,  lcd_mipi_read_debug_store),
	__ATTR(mpstate, 0444, lcd_mipi_state_debug_show, NULL),
	__ATTR(mpmode,  0200, NULL,                      lcd_mipi_mode_debug_store),
	__ATTR(mpdphy,  0200, NULL,                      lcd_mipi_dphy_debug_store),
	__ATTR(null,    0644, NULL,                      NULL)
};

static struct device_attribute lcd_debug_attrs_edp[] = {
	__ATTR(edp,   0644, lcd_edp_debug_show, lcd_edp_debug_store),
	__ATTR(dpcd,  0200, NULL, lcd_edp_dpcd_debug_store),
	__ATTR(phy,   0644, lcd_phy_debug_show, lcd_phy_debug_store),
	__ATTR(edid,  0444, lcd_edp_edid_debug_show, NULL),
	__ATTR(null,  0644, NULL, NULL)
};
#endif

#ifdef CONFIG_AMLOGIC_LCD_TV
static struct device_attribute lcd_debug_attrs_mlvds[] = {
	__ATTR(mlvds,  0644, lcd_mlvds_debug_show, lcd_mlvds_debug_store),
	__ATTR(phy,    0644, lcd_phy_debug_show, lcd_phy_debug_store),
	__ATTR(tcon,   0644, lcd_tcon_debug_show, lcd_tcon_debug_store),
	__ATTR(tcon_status,   0444, lcd_tcon_status_show, NULL),
	__ATTR(tcon_reg,   0644, lcd_tcon_reg_debug_show, lcd_tcon_reg_debug_store),
	__ATTR(tcon_fw,   0644, lcd_tcon_fw_dbg_show, lcd_tcon_fw_dbg_store),
	__ATTR(tcon_pdf,  0644, lcd_tcon_pdf_dbg_show, lcd_tcon_pdf_dbg_store),
	__ATTR(tcon_rdma, 0644, lcd_tcon_rdma_dbg_show, lcd_tcon_rdma_dbg_store),
	__ATTR(tcon_info, 0444, lcd_tcon_info_dbg_show, NULL),
	__ATTR(tcon_cmpr, 0444, lcd_tcon_cmpr_dbg_show, NULL),
	__ATTR(null,   0644, NULL, NULL)
};

static struct device_attribute lcd_debug_attrs_p2p[] = {
	__ATTR(p2p,    0644, lcd_p2p_debug_show, lcd_p2p_debug_store),
	__ATTR(phy,    0644, lcd_phy_debug_show, lcd_phy_debug_store),
	__ATTR(tcon,   0644, lcd_tcon_debug_show, lcd_tcon_debug_store),
	__ATTR(tcon_status,   0444, lcd_tcon_status_show, NULL),
	__ATTR(tcon_reg,   0644, lcd_tcon_reg_debug_show, lcd_tcon_reg_debug_store),
	__ATTR(tcon_fw,   0644, lcd_tcon_fw_dbg_show, lcd_tcon_fw_dbg_store),
	__ATTR(tcon_pdf,  0644, lcd_tcon_pdf_dbg_show, lcd_tcon_pdf_dbg_store),
	__ATTR(tcon_rdma, 0644, lcd_tcon_rdma_dbg_show, lcd_tcon_rdma_dbg_store),
	__ATTR(tcon_info, 0444, lcd_tcon_info_dbg_show, NULL),
	__ATTR(tcon_cmpr, 0444, lcd_tcon_cmpr_dbg_show, NULL),
	__ATTR(null,   0644, NULL, NULL)
};
#endif

static int lcd_debug_file_op(struct aml_lcd_drv_s *pdrv, unsigned char en)
{
	struct device_attribute *lcd_attr;
	int i;

	for (i = 0; i < ARRAY_SIZE(lcd_debug_attrs); i++) {
		if (en)	{
			if (device_create_file(pdrv->dev, &lcd_debug_attrs[i])) {
				LCDERR("create lcd debug attribute %s fail\n",
				lcd_debug_attrs[i].attr.name);
			}
		} else {
			device_remove_file(pdrv->dev, &lcd_debug_attrs[i]);
		}
	}

	switch (pdrv->config.basic.lcd_type) {
	case LCD_LVDS:
		lcd_attr = lcd_debug_attrs_lvds;
		break;
	case LCD_VBYONE:
		lcd_attr = lcd_debug_attrs_vbyone;
		break;
#ifdef CONFIG_AMLOGIC_LCD_TABLET
	case LCD_RGB:
		lcd_attr = lcd_debug_attrs_rgb;
		break;
	case LCD_BT656:
	case LCD_BT1120:
		lcd_attr = lcd_debug_attrs_bt;
		break;
	case LCD_MIPI:
		lcd_attr = lcd_debug_attrs_mipi;
		break;
	case LCD_EDP:
		lcd_attr = lcd_debug_attrs_edp;
		break;
#endif
#ifdef CONFIG_AMLOGIC_LCD_TV
	case LCD_MLVDS:
		lcd_attr = lcd_debug_attrs_mlvds;
		break;
	case LCD_P2P:
		lcd_attr = lcd_debug_attrs_p2p;
		break;
#endif
	default:
		return 0;
	}

	while (lcd_attr) {
		if (strcmp(lcd_attr->attr.name, "null") == 0)
			break;

		if (en) {
			if (device_create_file(pdrv->dev, lcd_attr)) {
				LCDERR("create interface debug attribute %s fail\n",
					lcd_attr->attr.name);
			}
		} else {
			device_remove_file(pdrv->dev, lcd_attr);
		}
		lcd_attr++;
	}

	return 0;
}

/***** LCD debug file operation done ******/
static struct lcd_debug_info_s lcd_debug_info_axg = {
	.reg_pinmux_table = NULL,

	.reg_dump_lvds   = NULL,
	.reg_dump_vbyone = NULL,
#ifdef CONFIG_AMLOGIC_LCD_TABLET
	.reg_dump_mipi   = lcd_reg_print_mipi,
	.reg_dump_edp    = NULL,
#endif
#ifdef CONFIG_AMLOGIC_LCD_TV
	.reg_dump_mlvds  = NULL,
	.reg_dump_p2p    = NULL,
#endif
};

static struct lcd_debug_info_s lcd_debug_info_g12a = {
	.reg_pinmux_table = NULL,

	.reg_dump_lvds   = NULL,
	.reg_dump_vbyone = NULL,
#ifdef CONFIG_AMLOGIC_LCD_TABLET
	.reg_dump_mipi   = lcd_reg_print_mipi,
	.reg_dump_edp    = NULL,
#endif
#ifdef CONFIG_AMLOGIC_LCD_TV
	.reg_dump_mlvds  = NULL,
	.reg_dump_p2p    = NULL,
#endif
};

static struct lcd_debug_info_s lcd_debug_info_tl1 = {
	.reg_pinmux_table = lcd_reg_dump_pinmux_tl1,

	.reg_dump_lvds   = lcd_reg_print_lvds,
	.reg_dump_vbyone = lcd_reg_print_vbyone,
#ifdef CONFIG_AMLOGIC_LCD_TABLET
	.reg_dump_mipi   = NULL,
	.reg_dump_edp    = NULL,
#endif
#ifdef CONFIG_AMLOGIC_LCD_TV
	.reg_dump_mlvds  = lcd_reg_print_tcon,
	.reg_dump_p2p    = lcd_reg_print_tcon,
#endif
};

static struct lcd_debug_info_s lcd_debug_info_t5 = {
	.reg_pinmux_table = lcd_reg_dump_pinmux_t5,

	.reg_dump_lvds   = lcd_reg_print_lvds,
	.reg_dump_vbyone = lcd_reg_print_vbyone,
#ifdef CONFIG_AMLOGIC_LCD_TABLET
	.reg_dump_mipi   = NULL,
	.reg_dump_edp    = NULL,
#endif
#ifdef CONFIG_AMLOGIC_LCD_TV
	.reg_dump_mlvds  = lcd_reg_print_tcon,
	.reg_dump_p2p    = lcd_reg_print_tcon,
#endif
};

static struct lcd_debug_info_s lcd_debug_info_t7_0 = {
	.reg_pinmux_table = lcd_reg_dump_pinmux_t7,

	.reg_dump_lvds   = lcd_reg_print_lvds_t7,
	.reg_dump_vbyone = lcd_reg_print_vbyone_t7,
#ifdef CONFIG_AMLOGIC_LCD_TABLET
	.reg_dump_mipi   = lcd_reg_print_mipi,
	.reg_dump_edp    = lcd_reg_print_edp,
#endif
#ifdef CONFIG_AMLOGIC_LCD_TV
	.reg_dump_mlvds  = NULL,
	.reg_dump_p2p    = NULL,
#endif
};

static struct lcd_debug_info_s lcd_debug_info_t7_1 = {
	.reg_pinmux_table = lcd_reg_dump_pinmux_t7,

	.reg_dump_lvds   = lcd_reg_print_lvds_t7,
	.reg_dump_vbyone = lcd_reg_print_vbyone_t7,
#ifdef CONFIG_AMLOGIC_LCD_TABLET
	.reg_dump_mipi   = lcd_reg_print_mipi,
	.reg_dump_edp    = lcd_reg_print_edp,
#endif
#ifdef CONFIG_AMLOGIC_LCD_TV
	.reg_dump_mlvds  = NULL,
	.reg_dump_p2p    = NULL,
#endif
};

static struct lcd_debug_info_s lcd_debug_info_t7_2 = {
	.reg_pinmux_table = lcd_reg_dump_pinmux_t7,

	.reg_dump_lvds   = lcd_reg_print_lvds_t7,
	.reg_dump_vbyone = NULL,
#ifdef CONFIG_AMLOGIC_LCD_TABLET
	.reg_dump_mipi   = NULL,
	.reg_dump_edp    = NULL,
#endif
#ifdef CONFIG_AMLOGIC_LCD_TV
	.reg_dump_mlvds  = NULL,
	.reg_dump_p2p    = NULL,
#endif
};

static struct lcd_debug_info_s lcd_debug_info_t3_0 = {
	.reg_pinmux_table = lcd_reg_dump_pinmux_t3,

	.reg_dump_lvds   = lcd_reg_print_lvds_t7,
	.reg_dump_vbyone = lcd_reg_print_vbyone_t7,
#ifdef CONFIG_AMLOGIC_LCD_TABLET
	.reg_dump_mipi   = NULL,
	.reg_dump_edp    = NULL,
#endif
#ifdef CONFIG_AMLOGIC_LCD_TV
	.reg_dump_mlvds  = lcd_reg_print_tcon,
	.reg_dump_p2p    = lcd_reg_print_tcon,
#endif
};

static struct lcd_debug_info_s lcd_debug_info_t3_1 = {
	.reg_pinmux_table = lcd_reg_dump_pinmux_t3,

	.reg_dump_lvds   = NULL,
	.reg_dump_vbyone = lcd_reg_print_vbyone_t7,
#ifdef CONFIG_AMLOGIC_LCD_TABLET
	.reg_dump_mipi   = NULL,
	.reg_dump_edp    = NULL,
#endif
#ifdef CONFIG_AMLOGIC_LCD_TV
	.reg_dump_mlvds  = NULL,
	.reg_dump_p2p    = NULL,
#endif
};

static struct lcd_debug_info_s lcd_debug_info_t5w = {
	.reg_pinmux_table = lcd_reg_dump_pinmux_tl1,

	.reg_dump_lvds   = lcd_reg_print_lvds_t7,
	.reg_dump_vbyone = lcd_reg_print_vbyone_t7,
#ifdef CONFIG_AMLOGIC_LCD_TABLET
	.reg_dump_mipi   = NULL,
	.reg_dump_edp    = NULL,
#endif
#ifdef CONFIG_AMLOGIC_LCD_TV
	.reg_dump_mlvds  = lcd_reg_print_tcon,
	.reg_dump_p2p    = lcd_reg_print_tcon,
#endif
};

static struct lcd_debug_info_s lcd_debug_info_c3 = {
	.reg_pinmux_table = lcd_reg_dump_pinmux_c3,

	.reg_dump_lvds   = NULL,
	.reg_dump_vbyone = NULL,
#ifdef CONFIG_AMLOGIC_LCD_TABLET
	.reg_dump_mipi   = lcd_reg_print_mipi,
	.reg_dump_edp    = NULL,
#endif
#ifdef CONFIG_AMLOGIC_LCD_TV
	.reg_dump_mlvds  = NULL,
	.reg_dump_p2p    = NULL,
#endif
};

static struct lcd_debug_info_s lcd_debug_info_t3x_0 = {
	.reg_pinmux_table = lcd_reg_dump_pinmux_t3,

	.reg_dump_lvds   = lcd_reg_print_lvds_t7,
	.reg_dump_vbyone = lcd_reg_print_vbyone_t3x,
#ifdef CONFIG_AMLOGIC_LCD_TABLET
	.reg_dump_mipi   = NULL,
	.reg_dump_edp    = NULL,
#endif
#ifdef CONFIG_AMLOGIC_LCD_TV
	.reg_dump_mlvds  = NULL,
	.reg_dump_p2p    = lcd_reg_print_tcon,
#endif
};

static struct lcd_debug_info_s lcd_debug_info_t3x_1 = {
	.reg_pinmux_table = lcd_reg_dump_pinmux_t3,

	.reg_dump_lvds   = NULL,
	.reg_dump_vbyone = lcd_reg_print_vbyone_t3x,
#ifdef CONFIG_AMLOGIC_LCD_TABLET
	.reg_dump_mipi   = NULL,
	.reg_dump_edp    = NULL,
#endif
#ifdef CONFIG_AMLOGIC_LCD_TV
	.reg_dump_mlvds  = NULL,
	.reg_dump_p2p    = NULL,
#endif
};

static struct lcd_debug_info_s lcd_debug_info_txhd2 = {
	.reg_pinmux_table = lcd_reg_dump_pinmux_t5,

	.reg_dump_lvds   = lcd_reg_print_lvds,
	.reg_dump_vbyone = NULL,
#ifdef CONFIG_AMLOGIC_LCD_TABLET
	.reg_dump_mipi   = lcd_reg_print_mipi,
	.reg_dump_edp    = NULL,
#endif
#ifdef CONFIG_AMLOGIC_LCD_TV
	.reg_dump_mlvds  = lcd_reg_print_tcon,
	.reg_dump_p2p    = NULL,
#endif
};

static struct lcd_debug_info_s lcd_debug_info_s6 = {
	.reg_pinmux_table = NULL,

	.reg_dump_lvds   = NULL,
	.reg_dump_vbyone = NULL,
#ifdef CONFIG_AMLOGIC_LCD_TABLET
	.reg_dump_mipi   = lcd_reg_print_mipi,
	.reg_dump_edp    = NULL,
#endif
#ifdef CONFIG_AMLOGIC_LCD_TV
	.reg_dump_mlvds  = NULL,
	.reg_dump_p2p    = NULL,
#endif
};

int lcd_debug_probe(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_debug_info_s *lcd_debug_info;

	switch (pdrv->data->chip_type) {
	case LCD_CHIP_TL1:
	case LCD_CHIP_TM2:
		lcd_debug_info = &lcd_debug_info_tl1;
		break;
	case LCD_CHIP_T5:
	case LCD_CHIP_T5D:
		lcd_debug_info = &lcd_debug_info_t5;
		break;
	case LCD_CHIP_T5W:
		lcd_debug_info = &lcd_debug_info_t5w;
		break;
	case LCD_CHIP_T7:
		if (pdrv->index == 2)
			lcd_debug_info = &lcd_debug_info_t7_2;
		else if (pdrv->index == 1)
			lcd_debug_info = &lcd_debug_info_t7_1;
		else
			lcd_debug_info = &lcd_debug_info_t7_0;
		break;
	case LCD_CHIP_T3:
	case LCD_CHIP_T5M:
	case LCD_CHIP_T6D:
		if (pdrv->index == 1)
			lcd_debug_info = &lcd_debug_info_t3_1;
		else
			lcd_debug_info = &lcd_debug_info_t3_0;
		break;
	case LCD_CHIP_T3X:
		if (pdrv->index == 1)
			lcd_debug_info = &lcd_debug_info_t3x_1;
		else
			lcd_debug_info = &lcd_debug_info_t3x_0;
		break;
	case LCD_CHIP_AXG:
		lcd_debug_info = &lcd_debug_info_axg;
		break;
	case LCD_CHIP_G12A:
	case LCD_CHIP_G12B:
	case LCD_CHIP_SM1:
		lcd_debug_info = &lcd_debug_info_g12a;
		break;
	case LCD_CHIP_C3:
		lcd_debug_info = &lcd_debug_info_c3;
		break;
	case LCD_CHIP_TXHD2:
		lcd_debug_info = &lcd_debug_info_txhd2;
		break;
	case LCD_CHIP_S6:
		lcd_debug_info = &lcd_debug_info_s6;
		break;
	default:
		lcd_debug_info = NULL;
		return -1;
	}

	switch (pdrv->config.basic.lcd_type) {
	case LCD_LVDS:
		lcd_debug_info->interface_print = lcd_info_print_lvds;
		lcd_debug_info->reg_dump_interface = lcd_debug_info->reg_dump_lvds;
		break;
	case LCD_VBYONE:
		lcd_debug_info->interface_print = lcd_info_print_vbyone;
		lcd_debug_info->reg_dump_interface = lcd_debug_info->reg_dump_vbyone;
		break;
#ifdef CONFIG_AMLOGIC_LCD_TABLET
	case LCD_MIPI:
		lcd_debug_info->interface_print = lcd_info_print_mipi;
		lcd_debug_info->reg_dump_interface = lcd_debug_info->reg_dump_mipi;
		break;
	case LCD_EDP:
		lcd_debug_info->interface_print = lcd_info_print_edp;
		lcd_debug_info->reg_dump_interface = lcd_debug_info->reg_dump_edp;
		break;
#endif
#ifdef CONFIG_AMLOGIC_LCD_TV
	case LCD_MLVDS:
		lcd_debug_info->interface_print = lcd_info_print_mlvds;
		lcd_debug_info->reg_dump_interface = lcd_debug_info->reg_dump_mlvds;
		break;
	case LCD_P2P:
		lcd_debug_info->interface_print = lcd_info_print_p2p;
		lcd_debug_info->reg_dump_interface = lcd_debug_info->reg_dump_p2p;
		break;
#endif
	default:
		lcd_debug_info->interface_print = NULL;
		lcd_debug_info->reg_dump_interface = NULL;
		break;
	}

	pdrv->debug_info = (void *)lcd_debug_info;

	return lcd_debug_file_op(pdrv, 1);
}

int lcd_debug_remove(struct aml_lcd_drv_s *pdrv)
{
	return lcd_debug_file_op(pdrv, 0);
}
