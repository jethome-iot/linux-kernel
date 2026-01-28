/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __CSI_REG_SM1_H
#define __CSI_REG_SM1_H

// note: regs without chip suffix, default to SM1 regs;
// if not compatible with sm1, please define special regs in csi_reg_xx.h

#include "csi_reg_s6.h"

/*mipi aphy registers*/

// aphy hw addr. 4k aligned for page mapping;
#define HHI_CSI_PHY         0xff63c000
// aphy registers offset.
#define HHI_CSI_PHY_CNTL0  (0x0d3 << 2)
#define HHI_CSI_PHY_CNTL1  (0x0d4 << 2)
#define HHI_CSI_PHY_CNTL2  (0x0d5 << 2)

/*mipi-csi2 phy registers*/
#define MIPI_PHY_CTRL             (0x0 << 2)
#define MIPI_PHY_CLK_LANE_CTRL    (0x1 << 2)
#define MIPI_PHY_DATA_LANE_CTRL   (0x2 << 2)
#define MIPI_PHY_DATA_LANE_CTRL1  (0x3 << 2)
#define MIPI_PHY_TCLK_MISS        (0x4 << 2)
#define MIPI_PHY_TCLK_SETTLE      (0x5 << 2)
#define MIPI_PHY_THS_EXIT         (0x6 << 2)
#define MIPI_PHY_THS_SKIP         (0x7 << 2)
#define MIPI_PHY_THS_SETTLE       (0x8 << 2)
#define MIPI_PHY_TINIT            (0x9 << 2)
#define MIPI_PHY_TULPS_C          (0xa << 2)
#define MIPI_PHY_TULPS_S          (0xb << 2)
#define MIPI_PHY_TMBIAS           (0xc << 2)
#define MIPI_PHY_TLP_EN_W         (0xd << 2)
#define MIPI_PHY_TLPOK            (0xe << 2)
#define MIPI_PHY_TWD_INIT         (0xf << 2)
#define MIPI_PHY_TWD_HS           (0x10 << 2)
#define MIPI_PHY_AN_CTRL0         (0x11 << 2)
#define MIPI_PHY_AN_CTRL1         (0x12 << 2)
#define MIPI_PHY_AN_CTRL2         (0x13 << 2)
#define MIPI_PHY_CLK_LANE_STS     (0x14 << 2)
#define MIPI_PHY_DATA_LANE0_STS   (0x15 << 2)
#define MIPI_PHY_DATA_LANE1_STS   (0x16 << 2)
#define MIPI_PHY_DATA_LANE2_STS   (0x17 << 2)
#define MIPI_PHY_DATA_LANE3_STS   (0x18 << 2)
#define MIPI_PHY_INT_STS          (0x19 << 2)

#define MIPI_PHY_MUX_CTRL0        (0x61 << 2)
#define MIPI_PHY_MUX_CTRL1        (0x62 << 2)

/*mipi-csi2 host registers*/
#define CSI2_HOST_VERSION           0x00
#define CSI2_HOST_N_LANES           0x04
#define CSI2_HOST_PHY_SHUTDOWNZ     0x08
#define CSI2_HOST_DPHY_RSTZ         0x0c
#define CSI2_HOST_CSI2_RESETN       0x10
#define CSI2_HOST_PHY_STATE         0x14
#define CSI2_HOST_DATA_IDS_1        0x18
#define CSI2_HOST_DATA_IDS_2        0x1c
#define CSI2_HOST_ERR1              0x20
#define CSI2_HOST_ERR2              0x24
#define CSI2_HOST_MASK1             0x28
#define CSI2_HOST_MASK2             0x2c
#define CSI2_HOST_PHY_TST_CTRL0     0x30
#define CSI2_HOST_PHY_TST_CTRL1     0x34

/*adapt frontend registers*/
#define CSI2_CLK_RESET           (0x00 << 2)
#define CSI2_GEN_CTRL0           (0x01 << 2)
#define CSI2_GEN_CTRL1           (0x02 << 2)
#define CSI2_X_START_END_ISP     (0x03 << 2)
#define CSI2_Y_START_END_ISP     (0x04 << 2)
#define CSI2_INTERRUPT_CTRL_STAT (0x14 << 2)
#define CSI2_GEN_STAT0           (0x20 << 2)
#define CSI2_ERR_STAT0           (0x21 << 2)
#define CSI2_PIC_SIZE_STAT       (0x22 << 2)

#endif
