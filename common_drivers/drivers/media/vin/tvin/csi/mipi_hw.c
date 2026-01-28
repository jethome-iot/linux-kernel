// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/etherdevice.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/amlogic/media/mipi/am_mipi_csi2.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/platform_device.h>
#include <linux/amlogic/power_domain.h>
#include <linux/io.h>
//#include <dt-bindings/power/amlogic,pd.h>

#include "csi.h"
#include "csi_reg.h"

#include "mipi_hw_s6.h"

static struct csi_adapt g_csi;

static inline u32 WRITE_CBUS_REG(void __iomem *base, u32 addr,
		unsigned long val)
{
	void __iomem *io_addr = base + addr;

	__raw_writel(val, io_addr);

	return 0;
}

inline void phy_update_wr_reg_bits(unsigned int reg,
				unsigned int mask, unsigned int val)
{
	unsigned int tmp, orig;
	void __iomem *base = g_csi.csi_phy;

	if (base) {
		orig = __raw_readl(base + reg);
		tmp = orig & ~mask;
		tmp |= val & mask;
		__raw_writel(tmp, base + reg);
	}
}

inline void WRITE_CSI_PHY_REG_BITS(unsigned int adr, unsigned int val,
		unsigned int start, unsigned int len)
{
	phy_update_wr_reg_bits(adr,
		((1 << len) - 1) << start, val << start);
}

inline void WRITE_CSI_PHY_REG(int addr, u32 val)
{
	void __iomem *reg_addr = g_csi.csi_phy + addr;

	__raw_writel(val, reg_addr);
}

inline u32 READ_CSI_PHY_REG(int addr)
{
	u32 val;
	void __iomem *reg_addr = g_csi.csi_phy + addr;

	val = __raw_readl(reg_addr);
	return val;
}

inline void WRITE_CSI_HST_REG(int addr, u32 val)
{
	void __iomem *reg_addr = g_csi.csi_host + addr;

	__raw_writel(val, reg_addr);
}

inline u32 READ_CSI_HST_REG(int addr)
{
	u32 val;
	void __iomem *reg_addr = g_csi.csi_host + addr;

	val = __raw_readl(reg_addr);
	return val;
}

inline void WRITE_CSI_ADPT_REG(int addr, u32 val)
{
	void __iomem *reg_addr = g_csi.csi_adapt + addr;

	__raw_writel(val, reg_addr);
}

inline u32 READ_CSI_ADPT_REG(int addr)
{
	u32 val;
	void __iomem *reg_addr = g_csi.csi_adapt + addr;

	val = __raw_readl(reg_addr);
	return val;
}

inline u32 READ_CSI_ADPT_REG_BIT(int addr,
		const u32 _start, const u32 _len)
{
	void __iomem *reg_addr = g_csi.csi_adapt + addr;

	return	((__raw_readl(reg_addr) >> (_start)) & ((1L << (_len)) - 1));
}

void mipi_phy_reg_wr_and_check(s32 addr, s32 data)
{
#ifdef MIPI_WR_AND_CHECK
	u32 tmp;
#endif
	WRITE_CSI_PHY_REG(addr, data);
#ifdef MIPI_WR_AND_CHECK
	tmp = READ_CSI_PHY_REG(addr);
	if (tmp != data) {
		pr_info("%s, addr = %x, data = %x, read write unmatch\n",
			__func__, addr, tmp);
	}
#endif
}

void init_am_mipi_csi2_clock(void)
{
	g_csi.csi_clk = devm_clk_get(&g_csi.p_dev->dev,
		"cts_csi_phy_clk_composite");
	if (IS_ERR(g_csi.csi_clk)) {
		pr_err("%s: cannot get clk_gate_csi !!!\n",
			__func__);
		g_csi.csi_clk = NULL;
		return;
	}

	g_csi.adapt_clk = devm_clk_get(&g_csi.p_dev->dev,
		"cts_csi_adapt_clk_composite");
	if (IS_ERR(g_csi.adapt_clk)) {
		pr_err("%s: cannot get clk_gate_adapt !!!\n",
			__func__);
		g_csi.adapt_clk = NULL;
		return;
	}
}

void enable_am_mipi_csi2_clk(void)
{
	int rtn = 0;
	u32 csi_rate = 200000000;
	u32 adapt_rate = 666666667;

	if (!IS_ERR_OR_NULL(g_csi.csi_clk)) {
		if (clk_set_rate(g_csi.csi_clk, csi_rate) < 0)
			pr_err("%s :Failed to set csi rate\n", __func__);
		if (!__clk_is_enabled(g_csi.csi_clk)) {
			rtn = clk_prepare_enable(g_csi.csi_clk);
			if (rtn) {
				pr_err("Error to enable csi_clk\n");
				return;
			}
		}
	}
	if (!IS_ERR_OR_NULL(g_csi.adapt_clk)) {
		if (clk_set_rate(g_csi.adapt_clk, adapt_rate))
			pr_err("%s: Failed to set adapt rate\n", __func__);
		if (!__clk_is_enabled(g_csi.adapt_clk)) {
			rtn = clk_prepare_enable(g_csi.adapt_clk);
			if (rtn) {
				pr_err("Error to enable adapt_clk\n");
				return;
			}
		}
	}
}
void disable_am_mipi_csi2_clk(void)
{
	if (!IS_ERR_OR_NULL(g_csi.csi_clk)) {
		if (__clk_is_enabled(g_csi.csi_clk))
			clk_disable_unprepare(g_csi.csi_clk);
	}
	if (!IS_ERR_OR_NULL(g_csi.adapt_clk)) {
		if (__clk_is_enabled(g_csi.adapt_clk))
			clk_disable_unprepare(g_csi.adapt_clk);
	}
}

void deinit_am_mipi_csi2_clock(void)
{
	if (g_csi.csi_clk) {
		devm_clk_put(&g_csi.p_dev->dev, g_csi.csi_clk);
		g_csi.csi_clk = NULL;
	}

	if (g_csi.adapt_clk) {
		devm_clk_put(&g_csi.p_dev->dev, g_csi.adapt_clk);
		g_csi.adapt_clk = NULL;
	}
}

static void init_am_mipi_csi2_host(struct amcsi_dev_s *csi_dev, struct csi_adapt *adap_dev)
{
	struct csi_parm_s *info = &csi_dev->csi_parm;
	pr_info("info->lanes = %d\n", info->lanes);
	WRITE_CSI_HST_REG(CSI2_HOST_CSI2_RESETN, 1);
	WRITE_CSI_HST_REG(CSI2_HOST_N_LANES, (info->lanes - 1) & 3);

	if (get_csi_chip_type(csi_dev) == CSI_ON_S6) {
		WRITE_CSI_HST_REG(CSI2_HOST_MASK1, 0xf00);
	} else {
		// default to sm1;
		WRITE_CSI_HST_REG(CSI2_HOST_MASK1, 0x0);
	}
	WRITE_CSI_HST_REG(CSI2_HOST_MASK2, 0x0);

	udelay(1);
}

static int init_am_mipi_csi2_adapter(struct amcsi_dev_s *csi_dev, struct csi_adapt *adap_dev)
{
	unsigned int data32;

	WRITE_CSI_ADPT_REG(CSI2_CLK_RESET, 0);

	data32  = 0;
	data32 |= CSI2_CFG_422TO444_MODE        << 21;
	data32 |= 0x1f                          << 16;
	data32 |= CSI2_CFG_COLOR_EXPAND         << 15;
	data32 |= CSI2_CFG_BUFFER_PIC_SIZE      << 11;
	data32 |= CSI2_CFG_USE_NULL_PACKET      << 10;
	data32 |= CSI2_CFG_INV_FIELD            <<  9;
	data32 |= CSI2_CFG_INTERLACE_EN         <<  8;
	data32 |= CSI2_CFG_ALL_TO_MEM           <<  4;
	data32 |= 0xf;
	WRITE_CSI_ADPT_REG(CSI2_GEN_CTRL0,  data32);

	data32  = 0;
	data32 |= (csi_dev->para.h_active - 1) << 16;
	data32 |= 0                             <<  0;
	WRITE_CSI_ADPT_REG(CSI2_X_START_END_ISP, data32);

	data32  = 0;
	data32 |= (csi_dev->para.v_active - 1) << 16;
	data32 |= 0                             <<  0;
	WRITE_CSI_ADPT_REG(CSI2_Y_START_END_ISP, data32);

	WRITE_CSI_ADPT_REG(CSI2_INTERRUPT_CTRL_STAT, (1 << 1));

	WRITE_CSI_ADPT_REG(CSI2_ERR_STAT0, 0xffff);

	/*Enable clock*/
	data32  = 0;
	data32 |= 0 <<  2;
	data32 |= 1 <<  1;
	data32 |= 0 <<  0;
	WRITE_CSI_ADPT_REG(CSI2_CLK_RESET, data32);
	return 0;
}

static void powerup_csi_analog_sm1(struct csi_adapt *adap_dev)
{
	void __iomem *base_addr;

	base_addr = ioremap(HHI_CSI_PHY, 0x400);
	if (!base_addr) {
		pr_err("%s: Failed to ioremap addr\n", __func__);
		return;
	}

	WRITE_CBUS_REG(base_addr, HHI_CSI_PHY_CNTL0, 0x0b440585);
	WRITE_CBUS_REG(base_addr, HHI_CSI_PHY_CNTL1, 0x803f0000);
	WRITE_CBUS_REG(base_addr, HHI_CSI_PHY_CNTL2, 0xf002);

	iounmap(base_addr);

	//power_domain_switch(PM_CSI, PWR_ON);
}

static void powerdown_csi_analog_sm1(struct csi_adapt *adap_dev)
{
	//power_domain_switch(PM_CSI, PWR_OFF);
}

static void init_am_mipi_phy(struct amcsi_dev_s *csi_dev, struct csi_adapt *adap_dev)
{
	struct csi_parm_s *info = &csi_dev->csi_parm;
	u32 settle = 0;
	u32 ui_val = 0;
	u32 cycle_time = 5;

	ui_val = 1000 / info->settle;
	if ((1000 % info->settle) != 0)
		ui_val += 1;
	settle = (85 + 145 + (16 * ui_val)) / 2;
	settle = settle / cycle_time;
	pr_info("settle = 0x%04x\n", settle);

	if (get_csi_chip_type(csi_dev) == CSI_ON_S6) {
		settle = settle - 4;
		pr_info("adjust settle = 0x%04x\n", settle);
		mipi_phy_reg_wr_and_check(MIPI_PHY_CLK_LANE_CTRL, 0x140d8);
		mipi_phy_reg_wr_and_check(MIPI_PHY_TINIT, 0x0d);
	} else {
		// default to sm1
		mipi_phy_reg_wr_and_check(MIPI_PHY_CLK_LANE_CTRL, 0x3d8);
		mipi_phy_reg_wr_and_check(MIPI_PHY_TINIT, 0x4e20);
	}
	mipi_phy_reg_wr_and_check(MIPI_PHY_TCLK_MISS, 0x9);
	mipi_phy_reg_wr_and_check(MIPI_PHY_TCLK_SETTLE, 0x1f);
	mipi_phy_reg_wr_and_check(MIPI_PHY_THS_SKIP, 0xa);
	mipi_phy_reg_wr_and_check(MIPI_PHY_THS_SETTLE, settle);
	if (get_csi_chip_type(csi_dev) == CSI_ON_S6)
		mipi_phy_reg_wr_and_check(MIPI_PHY_THS_EXIT, 0x8);
	else
		mipi_phy_reg_wr_and_check(MIPI_PHY_THS_EXIT, 0x1f);
	mipi_phy_reg_wr_and_check(MIPI_PHY_TMBIAS, 0x100);
	mipi_phy_reg_wr_and_check(MIPI_PHY_TULPS_C, 0x1000);
	mipi_phy_reg_wr_and_check(MIPI_PHY_TULPS_S, 0x100);
	mipi_phy_reg_wr_and_check(MIPI_PHY_TLP_EN_W, 0x0c);
	mipi_phy_reg_wr_and_check(MIPI_PHY_TLPOK, 0x100);
	mipi_phy_reg_wr_and_check(MIPI_PHY_TWD_INIT, 0x400000);
	mipi_phy_reg_wr_and_check(MIPI_PHY_TWD_HS, 0x400000);
	mipi_phy_reg_wr_and_check(MIPI_PHY_DATA_LANE_CTRL, 0x0);
	mipi_phy_reg_wr_and_check(MIPI_PHY_DATA_LANE_CTRL1,
		0x3 | (0x1f << 2) | (0x3 << 7));
	if (get_csi_chip_type(csi_dev) == CSI_ON_S6) {
		if (info->lanes == 4) {
			// bit [17:16] clk lane select ob00 from clk-0; 0b01 from clk-1;
			// bit [15:0] SFEN 0 1 2 3 from: data lans 0 1 2 3;
			mipi_phy_reg_wr_and_check(MIPI_PHY_MUX_CTRL0_S6, 0x00000123);
			// bit [17] clk-1 lane ctrl signal from: 0 DPHY SCNN; 1 input 0;
			// bit [16] clk-1 lane ctrl signal from: 0 DPHY SCNN; 1 input 0;
			// bit [15:0] data lane 0 1 2 3 ctrl signal from: SFEN 0 1 2 3;
			mipi_phy_reg_wr_and_check(MIPI_PHY_MUX_CTRL1_S6, 0x00020123);
		} else if (info->lanes == 2) {
			// using clk a and data lane 0 & 1
			mipi_phy_reg_wr_and_check(MIPI_PHY_MUX_CTRL0_S6, 0x000001ff);
			mipi_phy_reg_wr_and_check(MIPI_PHY_MUX_CTRL1_S6, 0x000201ff);
			// using clk a and data lane 2 & 3
			//mipi_phy_reg_wr_and_check(MIPI_PHY_MUX_CTRL0_S6, 0x000023ff);
			//mipi_phy_reg_wr_and_check(MIPI_PHY_MUX_CTRL1_S6, 0x0002ff01);
		}
		// deskew_mode
		//[31] 1,       enable deskew
		//[26] 0,       use identical deskew sync data
		//[25] 1,       adjust auto adjust deskew phase, 0: manual 1: auto;
		//[24] 1,       deskew pattern format, 1: 0x55, 0: 0xAA.
		//[23:12] 0,    deskew window start
		//[11:0] ‘d128  deskew window end; 256 is 0x100; 512 is 0x200

		if (g_csi.deskew_mode == 0) {
			// disable initial deskew
			WRITE_CSI_PHY_REG_BITS(MIPI_PHY_DESKEW_CTRL_S6, 0, 31, 1);
			mipi_phy_reg_wr_and_check(MIPI_PHY_DESKEW_CTRL1_S6, 0x8);
		} else {
			// enable initial deskew
			mipi_phy_reg_wr_and_check(MIPI_PHY_DESKEW_CTRL_S6, 0x82006026);
			mipi_phy_reg_wr_and_check(MIPI_PHY_DESKEW_CTRL1_S6, 0x007101E8);
		}
		// disable period deskew.
		mipi_phy_reg_wr_and_check(MIPI_PHY_DESKEW_CTRL2_S6, 0x0);
		mipi_phy_reg_wr_and_check(MIPI_PHY_DESKEW_CTRL3_S6, 0x0);

		// analog squlech_mode
		if (g_csi.squlech_mode == 0) // disable
			WRITE_CSI_PHY_REG_BITS(MIPI_PHY_DESKEW_CTRL1_S6, 0, 3, 1);
		else
			WRITE_CSI_PHY_REG_BITS(MIPI_PHY_DESKEW_CTRL1_S6, 1, 3, 1);

		mipi_phy_reg_wr_and_check(MIPI_PHY_SQRST_CTRL_S6,
			(0x1 << 0) + (0x2 << 5) + (0x1f << 10) + (0x8 << 15) +
			(0x10 << 20) + (0x11 << 25));
	} else {
		// default to sm1
		mipi_phy_reg_wr_and_check(MIPI_PHY_MUX_CTRL0, 0x00000123);
		mipi_phy_reg_wr_and_check(MIPI_PHY_MUX_CTRL1, 0x00000123);
	}
	mipi_phy_reg_wr_and_check(MIPI_PHY_CTRL, 0);
}

static void reset_am_mipi_csi2_host(struct amcsi_dev_s *csi_dev, struct csi_adapt *adap_dev)
{
	WRITE_CSI_HST_REG(CSI2_HOST_CSI2_RESETN, 0);
}

static void reset_am_mipi_csi2_adapter(struct amcsi_dev_s *csi_dev, struct csi_adapt *adap_dev)
{
	WRITE_CSI_ADPT_REG(CSI2_CLK_RESET, 1);
}

static void reset_am_mipi_phy(struct amcsi_dev_s *csi_dev, struct csi_adapt *adap_dev)
{
	WRITE_CSI_PHY_REG_BITS(MIPI_PHY_CTRL, 0x1, 31, 1);
}

void am_mipi_csi2_para_init(struct platform_device *pdev)
{
	struct resource rs = {};
	int i = 0;
	int rtn = 0;

	memset(&g_csi, 0, sizeof(struct csi_adapt));
	g_csi.p_dev = pdev;

	rtn = of_property_read_u32(pdev->dev.of_node, "squlech_mode", &g_csi.squlech_mode);
	if (rtn != 0)
		pr_info("%s: squlech_mode not found in dts\n", __func__);

	rtn = of_property_read_u32(pdev->dev.of_node, "deskew_mode", &g_csi.deskew_mode);
	if (rtn != 0) {
		pr_info("%s: deskew_mode not found in dts\n", __func__);
	}

	for (i = 0; i < 3; i++) {
		rtn = of_address_to_resource(pdev->dev.of_node, i, &rs);
		if (rtn != 0) {
			pr_err("%s:Error idx %d get mipi csi reg resource\n",
				__func__, i);
		} else {
			pr_info("%s: rs idx %d info: name: %s\n", __func__,
				i, rs.name);

			if (strcmp(rs.name, "csi_phy") == 0) {
				g_csi.csi_phy_reg = rs;
				g_csi.csi_phy =
				ioremap(g_csi.csi_phy_reg.start,
				resource_size(&g_csi.csi_phy_reg));
			} else if (strcmp(rs.name, "csi_host") == 0) {
				g_csi.csi_host_reg = rs;
				g_csi.csi_host =
				ioremap(g_csi.csi_host_reg.start,
				resource_size(&g_csi.csi_host_reg));
			} else if (strcmp(rs.name, "csi_adapt") == 0) {
				g_csi.csi_adapt_reg = rs;
				g_csi.csi_adapt =
				ioremap(g_csi.csi_adapt_reg.start,
				resource_size(&g_csi.csi_adapt_reg));
			} else {
				pr_err("%s:Error match address\n", __func__);
			}
		}
	}
}

void am_mipi_csi2_init(struct amcsi_dev_s *csi_dev)
{
	if (get_csi_chip_type(csi_dev) == CSI_ON_SM1)
		powerup_csi_analog_sm1(&g_csi);
	else if (get_csi_chip_type(csi_dev) == CSI_ON_S6)
		powerup_csi_analog_s6(&g_csi);
	else
		pr_err("unknown chip type %d", csi_dev->csi_chip_info->csi_chip_type);

	init_am_mipi_phy(csi_dev, &g_csi);
	init_am_mipi_csi2_host(csi_dev, &g_csi);
	init_am_mipi_csi2_adapter(csi_dev, &g_csi);
}

void am_mipi_csi2_uninit(struct amcsi_dev_s *csi_dev)
{
	reset_am_mipi_phy(csi_dev, &g_csi);
	reset_am_mipi_csi2_host(csi_dev, &g_csi);
	reset_am_mipi_csi2_adapter(csi_dev, &g_csi);
	if (get_csi_chip_type(csi_dev) == CSI_ON_SM1)
		powerdown_csi_analog_sm1(&g_csi);
	else if (get_csi_chip_type(csi_dev) == CSI_ON_S6)
		powerdown_csi_analog_s6(&g_csi);
	else
		pr_err("unknown chip type %d", csi_dev->csi_chip_info->csi_chip_type);
}

void cal_csi_para(struct amcsi_dev_s *csi_dev)
{
}
