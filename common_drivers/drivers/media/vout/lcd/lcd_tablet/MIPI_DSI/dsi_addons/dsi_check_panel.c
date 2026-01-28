// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include "../../../lcd_reg.h"
#include "../../../lcd_common.h"
#include "../dsi_common.h"
#include "../dsi_ctrl/dsi_ctrl.h"

int mipi_dsi_check_state(struct aml_lcd_drv_s *pdrv, u8 reg, u8 cnt)
{
	int ret = 0, i;
	u8 *rd_data;
	u8 payload[3] = {DT_GEN_RD_1, 1, 0x04};
	struct dsi_config_s *dconf;
	u32 offset;

	dconf = &pdrv->config.control.mipi_cfg;
	if (dconf->check_en == 0)
		return 0;
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	rd_data = kcalloc(cnt, sizeof(u8), GFP_KERNEL);
	if (!rd_data) {
		LCDERR("[%d]: %s: rd_data kcalloc error\n", pdrv->index, __func__);
		return 0;
	}

	offset = pdrv->data->offset_venc_if[pdrv->index];

	payload[2] = reg;
	ret = lcd_dsi_read(pdrv, payload, rd_data, cnt);
	if (ret < 0)
		goto mipi_dsi_check_state_err;
	if (ret != cnt) {
		LCDERR("[%d]: %s: read back cnt is wrong\n", pdrv->index, __func__);
		goto mipi_dsi_check_state_err;
	}

	pr_info("read reg 0x%02x:", reg);
	for (i = 0; i < ret; i++)
		pr_info(" 0x%02x", rd_data[i]);
	pr_info("\n");

	dconf->check_state = 1;
	lcd_vcbus_setb(L_VCOM_VS_ADDR + offset, 1, 12, 1);
	pdrv->config.retry_enable_flag = 0;
	LCDPR("[%d]: %s: %d\n", pdrv->index, __func__, dconf->check_state);
	kfree(rd_data);
	return 0;

mipi_dsi_check_state_err:
	dconf->check_state = 0;
	lcd_vcbus_setb(L_VCOM_VS_ADDR + offset, 0, 12, 1);
	LCDPR("[%d]: %s: %d\n", pdrv->index, __func__, dconf->check_state);
	pdrv->config.retry_enable_flag = 1;
	kfree(rd_data);
	return -1;
}
