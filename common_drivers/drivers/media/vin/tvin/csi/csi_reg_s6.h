/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __CSI_REG_S6_H
#define __CSI_REG_S6_H

// note: this file only defines regs which are not compatible with sm1

/*mipi aphy registers*/
// aphy hw addr. 4k aligned for page mapping;

#define HHI_CSI_PHY_S6            0xfe008000

// aphy registers offset.
#define ANACTRL_MIPICSI_CTRL0_S6  0x240
#define ANACTRL_MIPICSI_CTRL1_S6  0x244
#define ANACTRL_MIPICSI_CTRL2_S6  0x248
#define ANACTRL_MIPICSI_CTRL3_S6  0x24c

#define ANACTRL_CSIPLL_CTRL3_S6   0x26c

/*mipi-csi2 phy registers*/
#define MIPI_PHY_ESC_CMD_S6            (0x19 << 2)
#define MIPI_PHY_INT_CTRL_S6           (0x1a << 2)
#define MIPI_PHY_INT_STS_S6            (0x1b << 2)
#define MIPI_PHY_ANA_STS_S6            (0x1c << 2)
#define MIPI_PHY_DDR_STS_S6            (0x1d << 2)
#define MIPI_PHY_TWD_ESC_S6            (0x1e << 2)
#define MIPI_PHY_DESKEW_CTRL_S6        (0x1f << 2)
#define MIPI_PHY_DESKEW_PHS_SEL_S6     (0x20 << 2)
#define MIPI_PHY_DESKEW_L0_ST_CHECK_S6 (0x21 << 2)
#define MIPI_PHY_DESKEW_L1_ST_CHECK_S6 (0x22 << 2)
#define MIPI_PHY_DESKEW_L2_ST_CHECK_S6 (0x23 << 2)
#define MIPI_PHY_DESKEW_L3_ST_CHECK_S6 (0x24 << 2)
#define MIPI_PHY_DESKEW_L0_CDLL_IDX_S6 (0x25 << 2)
#define MIPI_PHY_DESKEW_L1_CDLL_IDX_S6 (0x26 << 2)
#define MIPI_PHY_DESKEW_L2_CDLL_IDX_S6 (0x27 << 2)
#define MIPI_PHY_DESKEW_L3_CDLL_IDX_S6 (0x28 << 2)
#define MIPI_PHY_DESKEW_L0_DDLL_IDX_S6 (0x29 << 2)
#define MIPI_PHY_DESKEW_L1_DDLL_IDX_S6 (0x2a << 2)
#define MIPI_PHY_DESKEW_L2_DDLL_IDX_S6 (0x2b << 2)
#define MIPI_PHY_DESKEW_L3_DDLL_IDX_S6 (0x2c << 2)

#define MIPI_PHY_MUX_CTRL0_S6          (0xa1 << 2)
#define MIPI_PHY_MUX_CTRL1_S6          (0xa2 << 2)
#define MIPI_PHY_DESKEW_LEN_LANE01_S6  (0xa3 << 2)
#define MIPI_PHY_DESKEW_LEN_LANE23_S6  (0xa4 << 2)
#define MIPI_PHY_TWD_SOT_TIMEOUT_S6    (0xa5 << 2)
#define MIPI_PHY_BIST_CTRL_S6          (0xa6 << 2)
#define MIPI_PHY_STAT_BIST0_S6         (0xa7 << 2)
#define MIPI_PHY_STAT_BIST1_S6         (0xa8 << 2)
#define MIPI_PHY_STAT_BIST2_S6         (0xa9 << 2)
#define MIPI_PHY_STAT_BIST3_S6         (0xaa << 2)
#define MIPI_PHY_DESKEW_CTRL1_S6       (0xab << 2)
#define MIPI_PHY_DESKEW_CTRL2_S6       (0xac << 2)
#define MIPI_PHY_DESKEW_CTRL3_S6       (0xad << 2)
#define MIPI_PHY_SQRST_CTRL_S6         (0xae << 2)

/*adapt frontend registers*/
#define CSI2_STAT_GEN_SHORT_08_S6    (0x28 << 2)   // 0xa0
#define CSI2_STAT_GEN_SHORT_09_S6    (0x29 << 2)   // 0xa4
#define CSI2_STAT_GEN_SHORT_0A_S6    (0x2a << 2)   // 0xa8
#define CSI2_STAT_GEN_SHORT_0B_S6    (0x2b << 2)   // 0xac
#define CSI2_STAT_GEN_SHORT_0C_S6    (0x2c << 2)   // 0xb0
#define CSI2_STAT_GEN_SHORT_0D_S6    (0x2d << 2)   // 0xb4
#define CSI2_STAT_GEN_SHORT_0E_S6    (0x2e << 2)   // 0xb8
#define CSI2_STAT_GEN_SHORT_0F_S6    (0x2f << 2)   // 0xbc
#define CSI2_STAT_TYPE_RCVD_L_S6     (0x30 << 2)   // 0xc0
#define CSI2_STAT_TYPE_RCVD_H_S6     (0x31 << 2)   // 0xc4

#endif
