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
#include "./dsi_ctrl.h"

struct dsi_ctrl_s dsi_ctrl_v2 = {};

struct dsi_ctrl_s *dsi_bind_v2(struct aml_lcd_drv_s *pdrv)
{
	return &dsi_ctrl_v2;
}
