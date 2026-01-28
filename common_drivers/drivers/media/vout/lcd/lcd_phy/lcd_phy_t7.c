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
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include "../lcd_reg.h"
#include "lcd_phy_config.h"

static struct lcd_phy_ctrl_s *phy_ctrl_p;

static int lcd_phy_reg_dump(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int len = 0;
	struct reg_name_set_s reg_table[] = {
		{ANACTRL_DIF_PHY_CNTL1,  "PHY_CNTL1"},
		{ANACTRL_DIF_PHY_CNTL2,  "PHY_CNTL2"},
		{ANACTRL_DIF_PHY_CNTL3,  "PHY_CNTL3"},
		{ANACTRL_DIF_PHY_CNTL4,  "PHY_CNTL4"},
		{ANACTRL_DIF_PHY_CNTL5,  "PHY_CNTL5"},
		{ANACTRL_DIF_PHY_CNTL6,  "PHY_CNTL6"},
		{ANACTRL_DIF_PHY_CNTL7,  "PHY_CNTL7"},
		{ANACTRL_DIF_PHY_CNTL8,  "PHY_CNTL8"},
		{ANACTRL_DIF_PHY_CNTL9,  "PHY_CNTL9"},
		{ANACTRL_DIF_PHY_CNTL10, "PHY_CNTL10"},
		{ANACTRL_DIF_PHY_CNTL11, "PHY_CNTL11"},
		{ANACTRL_DIF_PHY_CNTL12, "PHY_CNTL12"},
		{ANACTRL_DIF_PHY_CNTL13, "PHY_CNTL13"},
		{ANACTRL_DIF_PHY_CNTL14, "PHY_CNTL14"},
		{ANACTRL_DIF_PHY_CNTL15, "PHY_CNTL15"},
		{ANACTRL_DIF_PHY_CNTL16, "PHY_CNTL16"},
		{ANACTRL_DIF_PHY_CNTL17, "PHY_CNTL17"},
		{ANACTRL_DIF_PHY_CNTL18, "PHY_CNTL18"},
		{ANACTRL_DIF_PHY_CNTL19, "PHY_CNTL19"},
		{ANACTRL_DIF_PHY_CNTL20, "PHY_CNTL20"},
		{ANACTRL_DIF_PHY_CNTL21, "PHY_CNTL21"}
	};

	len += str_add_reg_sets(pdrv, buf, offset, LCD_REG_DBG_ANA_BUS, 0,
				reg_table, ARRAY_SIZE(reg_table));
	return len;
}

static void lcd_phy_cntl_set_lane_t7(unsigned int flag, unsigned int base_val,
		unsigned int preem_val)
{
	unsigned int i;
	unsigned int t7_lane_cntl_reg[] = {
		ANACTRL_DIF_PHY_CNTL1,  ANACTRL_DIF_PHY_CNTL3,
		ANACTRL_DIF_PHY_CNTL4,  ANACTRL_DIF_PHY_CNTL5,
		ANACTRL_DIF_PHY_CNTL6,  ANACTRL_DIF_PHY_CNTL7,
		ANACTRL_DIF_PHY_CNTL8,  ANACTRL_DIF_PHY_CNTL9,
		ANACTRL_DIF_PHY_CNTL10, ANACTRL_DIF_PHY_CNTL12,
		ANACTRL_DIF_PHY_CNTL13, ANACTRL_DIF_PHY_CNTL14,
		ANACTRL_DIF_PHY_CNTL15, ANACTRL_DIF_PHY_CNTL16,
		ANACTRL_DIF_PHY_CNTL17, ANACTRL_DIF_PHY_CNTL18,
	};

	for (i = 0; i < ARRAY_SIZE(t7_lane_cntl_reg); i++) {
		if (flag & (1 << i))
			lcd_ana_write(t7_lane_cntl_reg[i], base_val | preem_val << 28);
	}
}

static void lcd_phy_cntl_set_lane_aux_t7(unsigned int flag, unsigned int aux_reg)
{
	if (flag & 0x01)
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL2, aux_reg);
	if (flag & 0x0100)
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL11, aux_reg);
}

static void lcd_lvds_phy_set(struct aml_lcd_drv_s *pdrv, int status)
{
	struct phy_config_s *phy_cfg = &pdrv->config.phy_cfg;

	if (!phy_ctrl_p)
		return;

	if (status) {
		lcd_phy_cntl_set_lane_t7(phy_cfg->lane_valid & 0x0101, 0x06430028,
					 phy_cfg->preem_level);
		lcd_phy_cntl_set_lane_t7(phy_cfg->lane_valid & 0xfefe, 0x06530028,
					 phy_cfg->preem_level);
		lcd_phy_cntl_set_lane_aux_t7(phy_cfg->lane_valid, 0x0100ffff);

		lcd_ana_write(ANACTRL_DIF_PHY_CNTL19, 0x00406240 | phy_cfg->vswing_level);
	} else {
		lcd_phy_cntl_set_lane_t7(phy_cfg->lane_valid, 0, 0);
		lcd_phy_cntl_set_lane_aux_t7(phy_cfg->lane_valid, 0);
		if (phy_ctrl_p->lane_lock_total == 0)
			lcd_ana_write(ANACTRL_DIF_PHY_CNTL19, 0);
	}
}

static void lcd_vbyone_phy_set(struct aml_lcd_drv_s *pdrv, int status)
{
	struct phy_config_s *phy_cfg = &pdrv->config.phy_cfg;

	if (!phy_ctrl_p)
		return;

	if (status) {
		lcd_phy_cntl_set_lane_t7(phy_cfg->lane_valid & 0x0101, 0x06430028,
					 phy_cfg->preem_level);
		lcd_phy_cntl_set_lane_t7(phy_cfg->lane_valid & 0xfefe, 0x06530028,
					 phy_cfg->preem_level);
		lcd_phy_cntl_set_lane_aux_t7(phy_cfg->lane_valid, 0x0100ffff);

		lcd_ana_write(ANACTRL_DIF_PHY_CNTL19, 0x00401640 | phy_cfg->vswing_level);
	} else {
		lcd_phy_cntl_set_lane_t7(phy_cfg->lane_valid, 0, 0);
		lcd_phy_cntl_set_lane_aux_t7(phy_cfg->lane_valid, 0);
		if (phy_ctrl_p->lane_lock_total == 0)
			lcd_ana_write(ANACTRL_DIF_PHY_CNTL19, 0);
	}
}

static void lcd_mipi_phy_set(struct aml_lcd_drv_s *pdrv, int status)
{
	struct phy_config_s *phy_cfg = &pdrv->config.phy_cfg;

	if (!phy_ctrl_p)
		return;

	if (status) {
		lcd_phy_cntl_set_lane_t7(phy_cfg->lane_valid & 0x1b1b, 0x022a0028, 0); //data lane
		lcd_phy_cntl_set_lane_t7(phy_cfg->lane_valid & 0x0404, 0x822a0028, 0); //clk lane
		lcd_phy_cntl_set_lane_aux_t7(phy_cfg->lane_valid, 0x0000ffcf);

		lcd_ana_write(ANACTRL_DIF_PHY_CNTL19, 0x1e406253);
		// if (pdrv->index)
		if (phy_cfg->lane_valid & 0xff00)
			lcd_ana_write(ANACTRL_DIF_PHY_CNTL21, 0xffff);
		else
			lcd_ana_write(ANACTRL_DIF_PHY_CNTL20, (0xffff << 16));
	} else {
		lcd_phy_cntl_set_lane_t7(phy_cfg->lane_valid & 0x1f1f, 0, 0);
		lcd_phy_cntl_set_lane_aux_t7(phy_cfg->lane_valid, 0);

		if (phy_ctrl_p->lane_lock_total == 0)
			lcd_ana_write(ANACTRL_DIF_PHY_CNTL19, 0);
		if (pdrv->index)
			lcd_ana_write(ANACTRL_DIF_PHY_CNTL21, 0);
		else
			lcd_ana_write(ANACTRL_DIF_PHY_CNTL20, 0);
	}
}

static void lcd_edp_phy_set(struct aml_lcd_drv_s *pdrv, int status)
{
	struct phy_config_s *phy_cfg = &pdrv->config.phy_cfg;

	if (!phy_ctrl_p)
		return;

	if (status) {
		lcd_phy_cntl_set_lane_t7(phy_cfg->lane_valid, 0x06530028, phy_cfg->preem_level);
		lcd_phy_cntl_set_lane_t7(phy_cfg->lane_valid & 0x0101, 0x46770038, 0); //DP aux lane
		lcd_phy_cntl_set_lane_aux_t7(phy_cfg->lane_valid, 0x0000ffff);

		lcd_ana_write(ANACTRL_DIF_PHY_CNTL19, 0x00406240 | phy_cfg->vswing_level);
	} else {
		lcd_phy_cntl_set_lane_t7(phy_cfg->lane_valid, 0, 0);
		lcd_phy_cntl_set_lane_aux_t7(phy_cfg->lane_valid, 0);
		if (phy_ctrl_p->lane_lock_total == 0)
			lcd_ana_write(ANACTRL_DIF_PHY_CNTL19, 0);
	}
}

static unsigned int lcd_phy_preem_level_to_value_t7(struct aml_lcd_drv_s *pdrv, unsigned int level)
{
	unsigned int preem_value = 0;

	preem_value = (level >= 0xf) ? 0xf : level;

	return preem_value;
}

static struct lcd_phy_ctrl_s lcd_phy_ctrl_t7 = {
	.lane_num = 16,
	.lane_lock_total = 0,

	.phy_vswing_level_to_val = lcd_phy_vswing_level_to_value_dft,
	.phy_preem_level_to_val = lcd_phy_preem_level_to_value_t7,
	.phy_amp_dft_val = NULL,
	.phy_glb_param_dft_val = NULL,
	.phy_lane_phase_sel_def = NULL,
	.phy_param_get = NULL,
	.phy_reg_dump = lcd_phy_reg_dump,

	.phy_set_lvds = lcd_lvds_phy_set,
	.phy_set_vx1 = lcd_vbyone_phy_set,
	.phy_set_mlvds = NULL,
	.phy_set_p2p = NULL,
	.phy_set_mipi = lcd_mipi_phy_set,
	.phy_set_edp = lcd_edp_phy_set,
};

struct lcd_phy_ctrl_s *lcd_phy_config_init_t7(struct lcd_data_s *pdata)
{
	char *curr_vout_connector;
	phy_ctrl_p = &lcd_phy_ctrl_t7;

#ifdef CONFIG_AMLOGIC_VOUT_SERVE
	curr_vout_connector = get_uboot_connector0_type();
	if (!strncmp("DP-", curr_vout_connector, 3))
		phy_ctrl_p->lane_lock_total |= 1 << 17;
#endif
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	curr_vout_connector = get_uboot_connector1_type();
	if (!strncmp("DP-", curr_vout_connector, 3))
		phy_ctrl_p->lane_lock_total |= 1 << 17;
#endif
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
	curr_vout_connector = get_uboot_connector2_type();
	if (!strncmp("DP-", curr_vout_connector, 3))
		phy_ctrl_p->lane_lock_total |= 1 << 17;
#endif
	return phy_ctrl_p;
}
