/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __CSI_INPUT_H
#define __CSI_INPUT_H

#include <linux/cdev.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h>
#include <linux/amlogic/media/mipi/am_mipi_csi2.h>
#include "../tvin_frontend.h"
#include "../tvin_global.h"

#define PRINT_DEBUG_INFO 1

#ifdef PRINT_DEBUG_INFO
#define DPRINT(...)     pr_info(__VA_ARGS__)
#else
#define DPRINT(...)
#endif

enum csi_chip_type_e {
	CSI_ON_SM1,
	CSI_ON_S6,
	CSI_UNKNOWN_CHIP
};

struct csi_chip_info_s {
	enum csi_chip_type_e csi_chip_type;
};

#define CSI2_CFG_USE_NULL_PACKET    0
#define CSI2_CFG_BUFFER_PIC_SIZE    1
#define CSI2_CFG_422TO444_MODE      1
#define CSI2_CFG_INV_FIELD          0
#define CSI2_CFG_INTERLACE_EN       0
#define CSI2_CFG_COLOR_EXPAND       0
#define CSI2_CFG_ALL_TO_MEM         0

enum amcsi_status_e {
	TVIN_AMCSI_STOP,
	TVIN_AMCSI_RUNNING,
	TVIN_AMCSI_START,
};

enum camera_fe_status {
	CAMERA_FE_OPEN,
	CAMERA_FE_CLOSE,
};

struct amcsi_dev_s {
	int                     index;
	dev_t                   devt;
	struct cdev             cdev;
	struct device           *dev;
	struct platform_device  *pdev;
	unsigned int            overflow_cnt;
	enum amcsi_status_e     dec_status;
	struct vdin_parm_s      para;
	struct csi_parm_s       csi_parm;
	unsigned char           reset;
	unsigned int            reset_count;
	unsigned int            irq_num;
	struct tvin_frontend_s  frontend;
	unsigned int            period;
	unsigned int            min_frmrate;
	struct timer_list       t;
	void __iomem            *csi_adapt_addr;
	enum camera_fe_status   fe_status;
	struct csi_chip_info_s  *csi_chip_info;
};

struct csi_adapt {
	struct platform_device  *p_dev;

	struct resource         csi_phy_reg;
	struct resource         csi_host_reg;
	struct resource         csi_adapt_reg;

	void __iomem            *csi_phy;
	void __iomem            *csi_host;
	void __iomem            *csi_adapt;

	struct clk              *csi_clk;
	struct clk              *adapt_clk;

	int                     squlech_mode;
	int                     deskew_mode;

};

void am_mipi_csi2_para_init(struct platform_device *pdev);

void WRITE_CSI_ADPT_REG(int addr, u32 val);
u32 READ_CSI_PHY_REG(int addr);
u32 READ_CSI_HST_REG(int addr);
u32 READ_CSI_ADPT_REG(int addr);
u32 READ_CSI_ADPT_REG_BIT(int addr,
	const u32 _start, const u32 _len);

// phy clk & adapt clk;
void init_am_mipi_csi2_clock(void);
void deinit_am_mipi_csi2_clock(void);
void enable_am_mipi_csi2_clk(void);
void disable_am_mipi_csi2_clk(void);

void am_mipi_csi2_init(struct amcsi_dev_s *csi_dev);
void cal_csi_para(struct amcsi_dev_s *csi_dev);
void am_mipi_csi2_uninit(struct amcsi_dev_s *csi_dev);

static inline enum csi_chip_type_e get_csi_chip_type(struct amcsi_dev_s *csi_dev)
{
	if (csi_dev->csi_chip_info)
		return csi_dev->csi_chip_info->csi_chip_type;
	else
		return CSI_ON_SM1;
}

#endif
