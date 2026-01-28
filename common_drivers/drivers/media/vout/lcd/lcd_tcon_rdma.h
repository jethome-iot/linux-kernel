/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_LCD_TCON_RDMA_H__
#define __AML_LCD_TCON_RDMA_H__
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>

#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
int VSYNC_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len);
int VSYNC_WR_MPEG_REG(u32 adr, u32 val);
#endif

int lcd_tcon_rdma_init(struct aml_lcd_drv_s *pdrv);
int lcd_tcon_rdma_wr(struct aml_lcd_drv_s *pdrv, u32 reg, const u32 val);
int lcd_tcon_rdma_wr_bits(struct aml_lcd_drv_s *pdrv, u32 reg, const u32 val,
		const u32 start, const u32 len);
int lcd_tcon_rdma_vs_handler(struct aml_lcd_drv_s *pdrv);
void lcd_tcon_rdma_remove(struct aml_lcd_drv_s *pdrv);

#endif  //__AML_LCD_TCON_RDMA_H__

