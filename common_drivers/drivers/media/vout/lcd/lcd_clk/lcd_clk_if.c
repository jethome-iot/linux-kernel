// SPDX-License-Identifier: GPL-2.0+
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/clk.h>
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#include "../lcd_reg.h"
#include "../lcd_common.h"
#include "lcd_clk_config.h"
#include "lcd_clk_ctrl.h"
#include "lcd_clk_utils.h"

static struct mutex lcd_clk_mutex;

static char *lcd_ss_freq_table_dft[] = {
	"0, 29.5KHz",
	"1, 31.5KHz",
	"2, 50KHz",
	"3, 75KHz",
	"4, 100KHz",
	"5, 150KHz",
	"6, 200KHz",
};

static char *lcd_ss_mode_table_dft[] = {
	"0, center ss",
	"1, up ss",
	"2, down ss",
};

struct lcd_clk_config_s *get_lcd_clk_config(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	int i;

	if (!pdrv->clk_conf) {
		LCDERR("[%d]: %s: clk_config is null\n", pdrv->index, __func__);
		return NULL;
	}
	cconf = (struct lcd_clk_config_s *)pdrv->clk_conf;

	for (i = 0; i < pdrv->clk_conf_num; i++) {
		if (!cconf[i].data) {
			LCDERR("[%d]: %s: clk config data is null\n",
				pdrv->index, __func__);
			return NULL;
		}
	}

	return cconf;
}

/* ****************************************************
 * lcd clk function api
 * ****************************************************
 */
void lcd_clk_frac_generate(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data)
		return;

	/* update bit_rate by interface */
	switch (pdrv->config.basic.lcd_type) {
	case LCD_VBYONE:
		lcd_vbyone_bit_rate_config(pdrv);
		break;
	case LCD_MLVDS:
		lcd_mlvds_bit_rate_config(pdrv);
		break;
	case LCD_P2P:
		lcd_p2p_bit_rate_config(pdrv);
		break;
	case LCD_MIPI:
		lcd_mipi_dsi_bit_rate_config(pdrv);
		break;
	case LCD_EDP:
		lcd_edp_bit_rate_config(pdrv);
	default:
		break;
	}
	if (cconf->data->pll_frac_generate)
		cconf->data->pll_frac_generate(pdrv);
}

static void lcd_bit_rate_match_phy(struct aml_lcd_drv_s *pdrv)
{
	struct phy_config_s *phy_cfg = &pdrv->config.phy_cfg;
	struct phy_attr_s *phy;
	int i = 0;
	unsigned int phy_clk;

	phy_cfg->act_phy = phy_cfg->phys[0];// if not matched, use default
	phy_clk = lcd_do_div(pdrv->config.timing.bit_rate, 1000000);
	for (i = 0; i < phy_cfg->group_num; i++) {
		phy = phy_cfg->phys[i];
		if (phy->phy_clk < phy_clk - 20 || phy->phy_clk > phy_clk + 20)
			continue;

		phy_cfg->act_phy = phy_cfg->phys[i];
		LCDPR("%s act_phy[%d], clk:%d\n", __func__, i, phy_cfg->act_phy->phy_clk);
		return;
	}
	if (phy_cfg->phys[0]->phy_clk)
		LCDPR("no phy_clk matched, use default(phy[0])\n");
}

static void lcd_phy_match_ss(struct aml_lcd_drv_s *pdrv)
{
	struct phy_attr_s *phy;
	struct lcd_timing_s *tim = &pdrv->config.timing;

	phy = pdrv->config.phy_cfg.act_phy;
	if (!phy)
		return;

	if (tim->act_timing.ss_force) {
		tim->ss_freq = tim->act_timing.ss_freq;
		tim->ss_level = tim->act_timing.ss_level;
		tim->ss_mode = tim->act_timing.ss_mode;
	} else {
		tim->ss_freq = phy->ss.freq;
		tim->ss_level = phy->ss.level;
		tim->ss_mode = phy->ss.mode;
	}

	LCDPR("[%d]:match ss_level=%d, ss_freq=%d, ss_mode=%d\n",
		pdrv->index, tim->ss_level, tim->ss_freq, tim->ss_mode);
}

void lcd_clk_generate_parameter(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data)
		return;

	/* update bit_rate by interface */
	switch (pdrv->config.basic.lcd_type) {
	case LCD_VBYONE:
		lcd_vbyone_bit_rate_config(pdrv);
		break;
	case LCD_MLVDS:
		lcd_mlvds_bit_rate_config(pdrv);
		break;
	case LCD_P2P:
		lcd_p2p_bit_rate_config(pdrv);
		break;
	case LCD_MIPI:
		lcd_mipi_dsi_bit_rate_config(pdrv);
		break;
	case LCD_EDP:
		lcd_edp_bit_rate_config(pdrv);
	default:
		break;
	}

	if (cconf->data->clk_generate_parameter)
		cconf->data->clk_generate_parameter(pdrv);
	lcd_bit_rate_match_phy(pdrv);
	lcd_phy_match_ss(pdrv);
	lcd_clk_ss_param_init(pdrv);

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);
}

int lcd_get_ss_num(struct aml_lcd_drv_s *pdrv,
	unsigned int *level, unsigned int *ppm, unsigned int *freq, unsigned int *mode)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data) {
		LCDERR("[%d] %s: clk_conf is null\n", pdrv->index, __func__);
		return -1;
	}

	if (cconf->data->ss_support == 0) {
		if (level)
			*level = 0;
		if (ppm)
			*ppm = 0;
		if (freq)
			*freq = 0;
		if (mode)
			*mode = 0;
	} else {
		if (level)
			*level = cconf->ss_level;
		if (ppm) {
			if (cconf->ss_level)
				*ppm = cconf->ss_ppm;
			else
				*ppm = 0;
		}
		if (freq)
			*freq = cconf->ss_freq;
		if (mode)
			*mode = cconf->ss_mode;
	}

	return cconf->ss_en;
}

int lcd_get_ss(struct aml_lcd_drv_s *pdrv, char *buf)
{
	struct lcd_clk_config_s *cconf;
	int len = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data) {
		len += sprintf(buf + len, "[%d]: clk config data is null\n", pdrv->index);
		return len;
	}
	if (cconf->data->ss_support == 0) {
		len += sprintf(buf + len, "[%d]: spread spectrum is not support\n", pdrv->index);
		return len;
	}

	if (cconf->ss_level) {
		len += sprintf(buf + len, "ss_level: %d, %dppm, dep_sel=%d, str_m=%d\n",
			cconf->ss_level, cconf->ss_ppm,
			cconf->ss_dep_sel, cconf->ss_str_m);
	} else {
		len += sprintf(buf + len, "ss_level: %d, disabled\n", cconf->ss_level);
	}
	len += sprintf(buf + len, "ss_freq: %s\n",
		lcd_ss_freq_table_dft[cconf->ss_freq]);
	len += sprintf(buf + len, "ss_mode: %s\n",
		lcd_ss_mode_table_dft[cconf->ss_mode]);
	return len;
}

int lcd_set_ss(struct aml_lcd_drv_s *pdrv, unsigned int level, unsigned int freq, unsigned int mode)
{
	struct lcd_clk_config_s *cconf;
	unsigned int ss_flag = 0;
	int ret = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data)
		return -1;
	if (cconf->data->ss_support == 0) {
		LCDERR("[%d]: %s: not support\n", pdrv->index, __func__);
		return -1;
	}

	mutex_lock(&lcd_clk_mutex);

	if (level < 0xff) {
		if (level > cconf->data->ss_level_max) {
			LCDERR("[%d]: %s: ss_level %d is out of support (max %d)\n",
			       pdrv->index, __func__, level, cconf->data->ss_level_max);
			ret = -1;
			goto lcd_set_ss_end;
		}
		cconf->ss_level = level;
		ss_flag |= LCD_SSC_LEVEL;
	}
	if (freq < 0xff) {
		if (freq > cconf->data->ss_freq_max) {
			LCDERR("[%d]: %s: ss_freq %d is out of support (max %d)\n",
			       pdrv->index, __func__, freq, cconf->data->ss_freq_max);
			ret = -1;
			goto lcd_set_ss_end;
		}
		cconf->ss_freq = freq;
		ss_flag |= LCD_SSC_FREQ;
	}
	if (mode < 0xff) {
		if (mode > cconf->data->ss_mode_max) {
			LCDERR("[%d]: %s: ss_mode %d is out of support (max %d)\n",
			       pdrv->index, __func__, mode, cconf->data->ss_mode_max);
			ret = -1;
			goto lcd_set_ss_end;
		}
		cconf->ss_mode = mode;
		ss_flag |= LCD_SSC_MODE;
	}

	if (cconf->data->set_ss && ss_flag)
		cconf->data->set_ss(pdrv, ss_flag);

lcd_set_ss_end:
	mutex_unlock(&lcd_clk_mutex);

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);
	return ret;
}

/* design for vlock, don't save ss_level to clk_config */
//can't mutex_lock for atomic context
int lcd_ss_enable(int index, unsigned int flag)
{
	struct aml_lcd_drv_s *pdrv;
	struct lcd_clk_config_s *cconf;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", index, __func__);

	pdrv = aml_lcd_get_driver(index);
	if (!pdrv) {
		LCDERR("[%d]: %s: drv is null\n", index, __func__);
		return -1;
	}
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data)
		return -1;

	if (cconf->data->clk_ss_enable)
		cconf->data->clk_ss_enable(pdrv, flag);

	return 0;
}

int lcd_encl_clk_msr(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	int clk_mux;
	int encl_clk = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data)
		return 0;

	clk_mux = cconf->data->enc_clk_msr_id;
	if (clk_mux == -1)
		return 0;
	encl_clk = meson_clk_measure(clk_mux);

	return encl_clk;
}

void lcd_clk_pll_reset(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data)
		return;

	mutex_lock(&lcd_clk_mutex);
	if (cconf->data->pll_reset)
		cconf->data->pll_reset(pdrv);
	mutex_unlock(&lcd_clk_mutex);
	LCDPR("[%d]: %s\n", pdrv->index, __func__);
}

unsigned long long lcd_pll_freq_get(int index)
{
	struct aml_lcd_drv_s *pdrv = aml_lcd_get_driver(index);
	struct lcd_clk_config_s *cconf;
	long long m, f, hz;
	int sign = 1;

	if (!pdrv)
		return 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return 0;

#define PLL_FRAC_CONST_LEN 17
	m = cconf->pll_m;
	f = cconf->pll_frac & ((1 << (PLL_FRAC_CONST_LEN + 1)) - 1);
	sign = (f & (1 << cconf->data->pll_frac_sign_bit)) ? -1 : 1;
	f *= sign;
	hz = 24000000 * ((m << PLL_FRAC_CONST_LEN) + f) >> PLL_FRAC_CONST_LEN;
#undef PLL_FRAC_CONST_LEN

	return (unsigned long long)hz;
}

//can't mutex_lock for atomic context
void lcd_vlock_m_update(int index, unsigned int vlock_m)
{
	struct aml_lcd_drv_s *pdrv;
	struct lcd_clk_config_s *cconf;

	pdrv = aml_lcd_get_driver(index);
	if (!pdrv) {
		LCDERR("[%d]: %s: drv is null\n", index, __func__);
		return;
	}
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data)
		return;

	if (pdrv->config.timing.clk_mode == LCD_CLK_MODE_INDEPENDENCE) {
		LCDERR("%s: clk_mode independence, can't adjust single pll m\n", __func__);
		return;
	}

	vlock_m &= 0xff;
	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("[%d]: %s, vlcok_m: 0x%x\n", index, __func__, vlock_m);

	if (cconf->data->pll_m_set)
		cconf->data->pll_m_set(pdrv, vlock_m);
}

//can't mutex_lock for atomic context
void lcd_vlock_frac_update(int index, unsigned int vlock_frac)
{
	struct aml_lcd_drv_s *pdrv;
	struct lcd_clk_config_s *cconf;

	pdrv = aml_lcd_get_driver(index);
	if (!pdrv) {
		LCDERR("[%d]: %s: drv is null\n", index, __func__);
		return;
	}
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data)
		return;

	if (pdrv->config.timing.clk_mode == LCD_CLK_MODE_INDEPENDENCE) {
		LCDERR("%s: clk_mode independence, can't adjust single pll frac\n", __func__);
		return;
	}

	vlock_frac &= 0x7ffff;
	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("[%d]: %s, vlock_frac: 0x%x\n", index, __func__, vlock_frac);

	if (cconf->data->pll_frac_set)
		cconf->data->pll_frac_set(pdrv, vlock_frac);
}

/* for frame rate change */
void lcd_update_clk_frac(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data)
		return;

	mutex_lock(&lcd_clk_mutex);
	if (cconf->data->pll_frac_set)
		cconf->data->pll_frac_set(pdrv, cconf->pll_frac);
	pdrv->config.timing.clk_change = 0; /* clear clk_change flag */
	mutex_unlock(&lcd_clk_mutex);

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("[%d]: %s: clk_change=0x%x\n",
			pdrv->index, __func__, pdrv->config.timing.clk_change);
	}
}

/* for timing init */
void lcd_set_clk(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	int cnt = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data)
		return;

	if (pdrv->lcd_pxp) {
		if (cconf->data->vclk_crt_set)
			cconf->data->vclk_crt_set(pdrv);
		return;
	}

	mutex_lock(&lcd_clk_mutex);
lcd_set_clk_retry:
	if (cconf->data->clk_set)
		cconf->data->clk_set(pdrv);
	if (cconf->data->vclk_crt_set)
		cconf->data->vclk_crt_set(pdrv);
	usleep_range(10000, 10001);

	while (lcd_clk_msr_check(pdrv)) {
		if (cnt++ >= 5) {
			LCDERR("[%d]: %s timeout\n", pdrv->index, __func__);
			break;
		}
		goto lcd_set_clk_retry;
	}

	if (cconf->data->clktree_set)
		cconf->data->clktree_set(pdrv);
	pdrv->config.timing.clk_change = 0; /* clear clk_change flag */
	mutex_unlock(&lcd_clk_mutex);

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("[%d]: %s: clk_change=0x%x\n",
			pdrv->index, __func__, pdrv->config.timing.clk_change);
	}
}

void lcd_disable_clk(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data)
		return;

	mutex_lock(&lcd_clk_mutex);
	if (cconf->data->clk_disable)
		cconf->data->clk_disable(pdrv);
	mutex_unlock(&lcd_clk_mutex);

	LCDPR("[%d]: %s\n", pdrv->index, __func__);
}

void lcd_clk_change(struct aml_lcd_drv_s *pdrv)
{
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("[%d]: %s: clk_change:0x%x\n",
			pdrv->index, __func__, pdrv->config.timing.clk_change);
	}

	if ((pdrv->config.timing.clk_change & LCD_CLK_PLL_CHANGE) ||
	    (pdrv->config.timing.clk_change & LCD_CLK_PLL_RESET)) {
#ifdef CONFIG_AMLOGIC_VPU
		if (vpu_support_overclk() && pdrv->vmode_switch) {
			LCDPR("[%d]: %s: vpu overclk flow\n", pdrv->index, __func__);
			lcd_venc_enable(pdrv, 0);
			msleep(30);
			vpu_dev_clk_request(pdrv->lcd_vpu_dev, pdrv->config.timing.enc_clk);
			msleep(30);
			lcd_venc_enable(pdrv, 1);
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
				LCDPR("[%d]: %s: vpu overclk flow done\n", pdrv->index, __func__);
		}
#endif
		lcd_set_clk(pdrv);
	} else if (pdrv->config.timing.clk_change & LCD_CLK_FRAC_UPDATE) {
		lcd_update_clk_frac(pdrv);
	}
}

int lcd_clk_set_dummy(struct aml_lcd_drv_s *pdrv, int status)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data || !cconf->data->clk_set_dummy)
		return -1;

	if (status) {
#ifdef CONFIG_AMLOGIC_VPU
		if (vpu_support_overclk()) {
			LCDPR("[%d]: %s: vpu overclk flow\n", pdrv->index, __func__);
			lcd_venc_enable(pdrv, 0);
			msleep(30);
			vpu_dev_clk_request(pdrv->lcd_vpu_dev, 25000000);
			msleep(30);
			lcd_venc_enable(pdrv, 1);
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
				LCDPR("[%d]: %s: vpu overclk flow done\n", pdrv->index, __func__);
		}
#endif
		mutex_lock(&lcd_clk_mutex);
		cconf->data->clk_set_dummy(pdrv);
		mutex_unlock(&lcd_clk_mutex);
	} else {
#ifdef CONFIG_AMLOGIC_VPU
		if (vpu_support_overclk()) {
			LCDPR("[%d]: %s: vpu overclk flow\n", pdrv->index, __func__);
			lcd_venc_enable(pdrv, 0);
			msleep(30);
			vpu_dev_clk_request(pdrv->lcd_vpu_dev, pdrv->config.timing.enc_clk);
			msleep(30);
			lcd_venc_enable(pdrv, 1);
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
				LCDPR("[%d]: %s: vpu overclk flow done\n", pdrv->index, __func__);
		}
#endif
		lcd_set_clk(pdrv);
	}
	LCDPR("[%d]: %s status: %d\n", pdrv->index, __func__, status);
	return 0;
}

int lcd_mlvds_clk_phase_set(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	int ret = -1;

	if ((pdrv->status & LCD_STATUS_IF_ON) == 0)
		return -1;
	if (pdrv->config.basic.lcd_type != LCD_MLVDS)
		return -1;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data)
		return -1;

	mutex_lock(&lcd_clk_mutex);
	if (cconf->data->mlvds_clk_phase_set)
		ret = cconf->data->mlvds_clk_phase_set(pdrv);
	mutex_unlock(&lcd_clk_mutex);

	LCDPR("[%d]: %s\n", pdrv->index, __func__);
	return ret;
}

void lcd_clk_gate_switch(struct aml_lcd_drv_s *pdrv, int status)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data)
		return;

	if (cconf->clktree.clk_gate_state == status) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("[%d]: gate is already %s\n", pdrv->index, status ? "on" : "off");
		return;
	}

	if (status) {
#ifdef CONFIG_AMLOGIC_VPU
		vpu_dev_clk_gate_on(pdrv->lcd_vpu_dev);
#endif
		lcd_clktree_gate_switch(pdrv, 1);
	} else {
		lcd_clktree_gate_switch(pdrv, 0);
#ifdef CONFIG_AMLOGIC_VPU
		vpu_dev_clk_gate_off(pdrv->lcd_vpu_dev);
#endif
	}
	cconf->clktree.clk_gate_state = status;
}

int lcd_clk_clkmsr_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	struct lcd_clk_config_s *cconf;
	int clk;
	int n, len = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n, "[%d]: %s: clk config is null\n",
				pdrv->index, __func__);
		return len;
	}

	if (cconf->data->enc_clk_msr_id == -1)
		goto lcd_clk_clkmsr_print_step_1;
	clk = meson_clk_measure(cconf->data->enc_clk_msr_id);
	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n, "encl_clk:    %u\n", clk);

lcd_clk_clkmsr_print_step_1:
	if (cconf->data->fifo_clk_msr_id == -1)
		goto lcd_clk_clkmsr_print_step_2;
	clk = meson_clk_measure(cconf->data->fifo_clk_msr_id);
	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n, "fifo_clk:    %u\n", clk);

lcd_clk_clkmsr_print_step_2:
	switch (pdrv->config.basic.lcd_type) {
	case LCD_MLVDS:
	case LCD_P2P:
		if (cconf->data->tcon_clk_msr_id == -1)
			break;
		clk = meson_clk_measure(cconf->data->tcon_clk_msr_id);
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n, "tcon_clk:    %u\n", clk);
	default:
		break;
	}

	return len;
}

int lcd_clk_config_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	struct lcd_clk_config_s *cconf;
	int n, len = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n, "[%d]: %s: clk config is null\n",
				pdrv->index, __func__);
		return len;
	}

	if (cconf->data->clk_config_print)
		len = cconf->data->clk_config_print(pdrv, buf, offset);

	return len;
}

int lcd_clk_reg_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	struct lcd_clk_config_s *cconf;
	int n, len = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n, "[%d]: %s: clk config is null\n",
				pdrv->index, __func__);
		return len;
	}

	if (cconf->data->clk_reg_print)
		len = cconf->data->clk_reg_print(pdrv, buf, offset);

	return len;
}

void aml_lcd_prbs_test(struct aml_lcd_drv_s *pdrv, unsigned int ms, unsigned int mode_flag)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data)
		return;

	if (cconf->data->prbs_test)
		cconf->data->prbs_test(pdrv, ms, mode_flag);
}

int lcd_clk_path_change(struct aml_lcd_drv_s *pdrv, int sel)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data)
		return -1;

	if (cconf->clk_path_change)
		cconf->clk_path_change(pdrv, sel);

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2) {
		if (cconf->data->clk_config_init_print)
			cconf->data->clk_config_init_print(pdrv);
	}

	return 0;
}

void lcd_clk_ss_param_init(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf = get_lcd_clk_config(pdrv);
	unsigned int ss_level, ss_freq, ss_mode;

	if (!cconf || !cconf->data)
		return;

	ss_level = pdrv->config.timing.ss_level;
	cconf->ss_level = (ss_level > cconf->data->ss_level_max) ?
				cconf->data->ss_level_max : ss_level;
	if (cconf->ss_level == 0)
		cconf->ss_en = 0;
	else
		cconf->ss_en = 1;

	ss_freq = pdrv->config.timing.ss_freq;
	cconf->ss_freq = (ss_freq > cconf->data->ss_freq_max) ?
				cconf->data->ss_freq_max : ss_freq;

	ss_mode = pdrv->config.timing.ss_mode;
	cconf->ss_mode = (ss_mode > cconf->data->ss_mode_max) ? 0 : ss_mode;

	if (cconf->data->clk_ss_init)
		cconf->data->clk_ss_init(cconf);

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("[%d]: %s: ss_level=%d, ss_freq=%d, ss_mode=%d\n",
		      pdrv->index, __func__,
		      cconf->ss_level, cconf->ss_freq, cconf->ss_mode);
	}
}

void lcd_clk_config_parameter_init(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf = get_lcd_clk_config(pdrv);

	if (!cconf || !cconf->data)
		return;

	if (cconf->data->clk_parameter_init)
		cconf->data->clk_parameter_init(pdrv);

	//lcd_clk_ss_param_init(pdrv); in lcd_config_init()->lcd_clk_generate_parameter()
}

static int lcd_clk_config_chip_init(struct aml_lcd_drv_s *pdrv, struct lcd_clk_config_s *cconf)
{
	unsigned int i;

	for (i = 0; i < pdrv->clk_conf_num; i++) {
		cconf[i].pll_id = pdrv->index + i;
		cconf[i].fin = FIN_FREQ;
	}

	switch (pdrv->data->chip_type) {
#ifndef CONFIG_AMLOGIC_C3_REMOVE
	case LCD_CHIP_AXG:
		lcd_clk_config_chip_init_axg(pdrv, cconf);
		break;
	case LCD_CHIP_G12A:
	case LCD_CHIP_SM1:
		lcd_clk_config_chip_init_g12a(pdrv, cconf);
		break;
	case LCD_CHIP_G12B:
		lcd_clk_config_chip_init_g12b(pdrv, cconf);
		break;
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	case LCD_CHIP_TL1:
		lcd_clk_config_chip_init_tl1(pdrv, cconf);
		break;
#endif
	case LCD_CHIP_TM2:
		lcd_clk_config_chip_init_tm2(pdrv, cconf);
		break;
	case LCD_CHIP_T5:
		lcd_clk_config_chip_init_t5(pdrv, cconf);
		break;
	case LCD_CHIP_T5D:
		lcd_clk_config_chip_init_t5d(pdrv, cconf);
		break;
	case LCD_CHIP_T7:
		lcd_clk_config_chip_init_t7(pdrv, cconf);
		break;
	case LCD_CHIP_T5M: //the same as t3, but only support 1 driver
	case LCD_CHIP_T3: /* only one pll */
		lcd_clk_config_chip_init_t3(pdrv, cconf);
		break;
	case LCD_CHIP_T5W:
		lcd_clk_config_chip_init_t5w(pdrv, cconf);
		break;
#endif
	case LCD_CHIP_C3:
		lcd_clk_config_chip_init_c3(pdrv, cconf);
		break;
#ifndef CONFIG_AMLOGIC_C3_REMOVE
	case LCD_CHIP_T3X:
		lcd_clk_config_chip_init_t3x(pdrv, cconf);
		break;
	case LCD_CHIP_TXHD2:
		lcd_clk_config_chip_init_txhd2(pdrv, cconf);
		break;
	case LCD_CHIP_S6:
		lcd_clk_config_chip_init_s6(pdrv, cconf);
		break;
	case LCD_CHIP_T6D:
		lcd_clk_config_chip_init_t6d(pdrv, cconf);
		break;
#endif
	default:
		LCDPR("[%d]: %s: invalid chip type\n", pdrv->index, __func__);
		return -1;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_CLK) {
		if (cconf->data->clk_config_init_print)
			cconf->data->clk_config_init_print(pdrv);
	}

	return 0;
}

void lcd_clk_config_probe(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	int ret;

	if (pdrv->config.timing.clk_mode == LCD_CLK_MODE_INDEPENDENCE)
		pdrv->clk_conf_num = 2;
	else
		pdrv->clk_conf_num = 1;

	cconf = kcalloc(pdrv->clk_conf_num, sizeof(struct lcd_clk_config_s), GFP_KERNEL);
	if (!cconf)
		return;
	pdrv->clk_conf = (void *)cconf;

	ret = lcd_clk_config_chip_init(pdrv, cconf);
	if (ret)
		return;

	lcd_clktree_bind(pdrv, 1);
}

void lcd_clk_config_remove(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	if (!pdrv->clk_conf)
		return;

	cconf = (struct lcd_clk_config_s *)pdrv->clk_conf;
	lcd_clktree_bind(pdrv, 0);

	kfree(cconf);
	pdrv->clk_conf = NULL;
}

void lcd_clk_init(void)
{
	mutex_init(&lcd_clk_mutex);
}
