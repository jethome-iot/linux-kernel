/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/enhancement/amvecm/reg_helper.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef __REG_HELPER_H
#define __REG_HELPER_H

//#include "arch/vpp_regs.h"
#include <linux/amlogic/media/amvecm/amvecm.h>
#include "arch/ve_regs.h"
#include "arch/cm_regs.h"
#include <linux/amlogic/media/rdma/rdma_mgr.h>

#define CLR_BIT(x) (~(0x01 << (x)))
#define CLR_BITS(x, y) ((~((0x01 << (y)) - 1)) << (x))
#define SET_BIT(x) (0x01 << (x))
#define GET_BIT(x) (0x01 << (x))
#define GET_BITS(x, y) (((0x01 << (y)) - 1) << (x))

#define srsharp0_sharp_hvsize 0x3e00
#define srsharp0_pkosht_vsluma_lut_h 0x3e81
#define srsharp1_sharp_hvsize 0x3f00
#define srsharp1_pkosht_vsluma_lut_h 0x3f81
#define srsharp1_lc_input_mux 0x3fb1
#define srsharp1_lc_map_ram_data 0x3ffe

/* useful inline functions to handle different offset */
static inline bool cpu_after_eq_t7(void)
{
	return cpu_after_eq(MESON_CPU_MAJOR_ID_T7);
}

static inline bool cpu_after_eq_tm2b(void)
{
	return cpu_after_eq(MESON_CPU_MAJOR_ID_TM2);
}

static inline bool cpu_after_eq_tl1(void)
{
	return cpu_after_eq(MESON_CPU_MAJOR_ID_TL1);
}

static inline bool is_sr0_reg(u32 addr)
{
	return (addr >= srsharp0_sharp_hvsize &&
		addr <= srsharp0_pkosht_vsluma_lut_h);
}

static inline bool is_sr1_reg(u32 addr)
{
	return (addr >= srsharp1_sharp_hvsize &&
		addr <= srsharp1_pkosht_vsluma_lut_h);
}

static inline bool is_lc_reg(u32 addr)
{
	return (addr >= srsharp1_lc_input_mux &&
		addr <= srsharp1_lc_map_ram_data);
}

static inline bool is_sr0_dnlpv2_reg(u32 addr)
{
	/*because s5 have no sr0 dnlp
	 *old sr0 reg overlap with slice3 hdr reg
	 */
	if (chip_type_id == chip_s5)
		return 0;

	return (addr >= SRSHARP0_DNLP2_00 &&
		addr <= SRSHARP0_DNLP2_31);
}

static inline bool is_sr1_dnlpv2_reg(u32 addr)
{
	return (addr >= SRSHARP1_DNLP2_00 &&
		addr <= SRSHARP1_DNLP2_31);
}

static inline u32 get_sr0_offset(void)
{
	/*sr0  register shfit*/
	if (cpu_after_eq_t7())
		return 0x1200;
	else if (cpu_after_eq_tm2b())
		return 0x1200;
	else if (cpu_after_eq_tl1())
		return 0x0;
	else if (is_meson_g12a_cpu() ||
		 is_meson_g12b_cpu() ||
		is_meson_sm1_cpu())
		return 0x0;

	return 0xfffff400 /*-0xc00*/;
}

static inline u32 get_sr1_offset(void)
{
	/*sr1 register shfit*/
	if (cpu_after_eq_t7())
		return 0x1300;
	else if (cpu_after_eq_tm2b())
		return 0x1300;
	else if (cpu_after_eq_tl1())
		return 0x0;
	else if (is_meson_g12a_cpu() ||
		 is_meson_g12b_cpu() ||
		is_meson_sm1_cpu())
		return 0x0;

	return 0xfffff380; /*-0xc80*/;
}

static inline u32 get_lc_offset(void)
{
	/* lc register shfit*/
	if (cpu_after_eq_t7())
		return 0x1300;
	else if (cpu_after_eq_tm2b())
		return 0x1300;

	return 0;
}

static inline u32 get_sr0_dnlp2_offset(void)
{
	/* SHARP0_DNLP_00 shfit*/
	if (cpu_after_eq_t7())
		return 0x1200;
	else if (cpu_after_eq_tm2b())
		return 0x1200;

	return 0;
}

static inline u32 get_sr1_dnlp2_offset(void)
{
	/* SHARP1_DNLP_00 shfit*/
	if (cpu_after_eq_t7())
		return 0x1300;
	else if (cpu_after_eq_tm2b())
		return 0x1300;

	return 0;
}

static u32 offset_addr(u32 addr)
{
	unsigned int lc_offset = 0;

	if (chip_type_id == chip_s5)
		return addr;

	if (chip_type_id == chip_txhd2)
		lc_offset = 0x200;

	if (is_sr0_reg(addr))
		return addr + get_sr0_offset();
	else if (is_sr1_reg(addr))
		return addr + get_sr1_offset();
	else if (is_sr0_dnlpv2_reg(addr))
		return addr + get_sr0_dnlp2_offset();
	else if (is_sr1_dnlpv2_reg(addr))
		return addr + get_sr1_dnlp2_offset();
	else if (is_lc_reg(addr))
		return addr + get_lc_offset() - lc_offset;

	return addr;
}

static inline void WRITE_VPP_REG(u32 reg,
				 const u32 value)
{
	if (!reg)
		return;

	aml_write_vcbus_s(offset_addr(reg), value);
}

static inline void WRITE_VPP_REG_S5(u32 reg,
				 const u32 value)
{
	aml_write_vcbus(reg, value);
}

static inline u32 READ_VPP_REG(u32 reg)
{
	if (!reg)
		return 0;

	return aml_read_vcbus_s(offset_addr(reg));
}

static inline u32 READ_VPP_REG_S5(u32 reg)
{
	return aml_read_vcbus(reg);
}

static inline void WRITE_VPP_REG_EX(u32 reg,
				    const u32 value,
				    bool add_offset)
{
	if (add_offset)
		reg = offset_addr(reg);

	if (!reg)
		return;

	aml_write_vcbus_s(reg, value);
}

static inline u32 READ_VPP_REG_EX(u32 reg,
				  bool add_offset)
{
	if (add_offset)
		reg = offset_addr(reg);

	if (!reg)
		return 0;

	return aml_read_vcbus_s(reg);
}

static inline void WRITE_VPP_REG_BITS(u32 reg,
				      const u32 value,
		const u32 start,
		const u32 len)
{
	if (!reg)
		return;

	aml_vcbus_update_bits_s(offset_addr(reg), value, start, len);
}

static inline void WRITE_VPP_REG_BITS_S5(u32 reg,
				      const u32 value,
		const u32 start,
		const u32 len)
{
	aml_write_vcbus(reg, ((aml_read_vcbus(reg) &
			     ~(((1L << (len)) - 1) << (start))) |
			    (((value) & ((1L << (len)) - 1)) << (start))));
}

static inline u32 READ_VPP_REG_BITS(u32 reg,
				    const u32 start,
				    const u32 len)
{
	u32 val;
	u32 reg1 = offset_addr(reg);

	if (!reg)
		return 0;

	val = ((aml_read_vcbus_s(reg1) >> (start)) & ((1L << (len)) - 1));

	return val;
}

static inline void WRITE_VPP_REG_SEL(u32 reg,
				const u32 value,
				const u32 vpp_sel)
{
	aml_write_vcbus_s(offset_addr(reg), value);
}

static inline u32 READ_VCBUS_REG_SEL(u32 reg, const u32 vpp_sel)
{
	return aml_read_vcbus_s(offset_addr(reg));
}

static inline void WRITE_VCBUS_REG_BITS_SEL(u32 reg,
		const u32 value,
		const u32 start,
		const u32 len,
		const u32 vpp_sel)
{
	aml_vcbus_update_bits_s(offset_addr(reg), value, start, len);
}

#ifndef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
#define VSYNC_WR_MPEG_REG(adr, val) WRITE_VPP_REG(adr, val)
#define VSYNC_RD_MPEG_REG(adr) READ_VPP_REG(adr)
#define VSYNC_WR_MPEG_REG_BITS(adr, val, start, len) \
	WRITE_VPP_REG_BITS(adr, val, start, len)

#define _VSYNC_WR_MPEG_REG(adr, val) WRITE_VPP_REG(adr, val)
#define _VSYNC_RD_MPEG_REG(adr) READ_VPP_REG(adr)
#define _VSYNC_WR_MPEG_REG_BITS(adr, val, start, len) \
		WRITE_VPP_REG_BITS(adr, val, start, len)

#define VSYNC_WR_MPEG_REG_VPP1(adr, val) WRITE_VCBUS_REG(adr, val)
#define VSYNC_RD_MPEG_REG_VPP1(adr) READ_VCBUS_REG(adr)
#define VSYNC_WR_MPEG_REG_BITS_VPP1(adr, val, start, len) \
	WRITE_VCBUS_REG_BITS(adr, val, start, len)

#define VSYNC_WR_MPEG_REG_VPP2(adr, val) WRITE_VCBUS_REG(adr, val)
#define VSYNC_RD_MPEG_REG_VPP2(adr) READ_VCBUS_REG(adr)
#define VSYNC_WR_MPEG_REG_BITS_VPP2(adr, val, start, len) \
	WRITE_VCBUS_REG_BITS(adr, val, start, len)

#define PRE_VSYNC_WR_MPEG_REG(adr, val) WRITE_VCBUS_REG(adr, val)
#define PRE_VSYNC_RD_MPEG_REG(adr) READ_VCBUS_REG(adr)
#define PRE_VSYNC_WR_MPEG_REG_BITS(adr, val, start, len) \
	WRITE_VCBUS_REG_BITS(adr, val, start, len)

#define VSYNC_WR_MPEG_REG_VPP_SEL(adr, val, vpp_sel) \
	WRITE_VPP_REG_SEL(adr, val, vpp_sel)
#define VSYNC_RD_MPEG_REG_VPP_SEL(adr, vpp_sel) \
	READ_VCBUS_REG_SEL(adr, vpp_sel)
#define VSYNC_WR_MPEG_REG_BITS_VPP_SEL(adr, val, start, len, vpp_sel) \
	WRITE_VCBUS_REG_BITS_SEL(adr, val, start, len, vpp_sel)

#else
int VSYNC_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len);
u32 VSYNC_RD_MPEG_REG(u32 adr);
int VSYNC_WR_MPEG_REG(u32 adr, u32 val);

int _VSYNC_WR_MPEG_REG(u32 adr, u32 val);
int _VSYNC_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len);
u32 _VSYNC_RD_MPEG_REG(u32 adr);

int VSYNC_WR_MPEG_REG_BITS_VPP1(u32 adr, u32 val, u32 start, u32 len);
u32 VSYNC_RD_MPEG_REG_VPP1(u32 adr);
int VSYNC_WR_MPEG_REG_VPP1(u32 adr, u32 val);

int VSYNC_WR_MPEG_REG_BITS_VPP2(u32 adr, u32 val, u32 start, u32 len);
u32 VSYNC_RD_MPEG_REG_VPP2(u32 adr);
int VSYNC_WR_MPEG_REG_VPP2(u32 adr, u32 val);

int PRE_VSYNC_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len);
u32 PRE_VSYNC_RD_MPEG_REG(u32 adr);
int PRE_VSYNC_WR_MPEG_REG(u32 adr, u32 val);

int VSYNC_WR_MPEG_REG_BITS_VPP_SEL(u32 adr, u32 val, u32 start, u32 len, int vpp_sel);
u32 VSYNC_RD_MPEG_REG_VPP_SEL(u32 adr, int vpp_sel);
int VSYNC_WR_MPEG_REG_VPP_SEL(u32 adr, u32 val, int vpp_sel);

#endif

/* table1:hdr; table2:sr dnlp lc;table3:other modules*/
static int index_rdma_part_ins(u32 reg)
{
	int table_index = 0;

	if ((reg >= 0x3800 && reg <= 0x384c) ||
		(reg >= 0x3850 && reg <= 0x389c) ||
		(reg >= 0x5930 && reg <= 0x5971) ||
		(reg >= 0x6000 && reg <= 0x603f) ||
		(reg >= 0x6200 && reg <= 0x624c) ||
		(reg >= 0x6280 && reg <= 0x62cc) ||
		(reg >= 0x38f0 && reg <= 0x38fd) ||
		(reg >= 0x38a0 && reg <= 0x38df) ||
		(reg >= 0x5b00 && reg <= 0x5b3f) ||
		(reg >= 0x5b50 && reg <= 0x5b8f) ||
		(reg >= 0x3290 && reg <= 0x329d) ||
		(reg >= 0x39a0 && reg <= 0x39ad) ||
		(reg >= 0x32b0 && reg <= 0x32bd) ||
		(reg >= 0x5990 && reg <= 0x599d) ||
		(reg >= 0x59d0 && reg <= 0x59dd) ||
		(reg >= 0x3d60 && reg <= 0x3d6d) ||
		(reg >= 0x0259 && reg <= 0x0266) ||
		(reg >= 0x3920 && reg <= 0x392d))
		table_index = 1;

	if ((reg >= 0x5000 && reg <= 0x5081) ||
		(reg >= 0x5200 && reg <= 0x5281) ||
		(reg >= 0x5090 && reg <= 0x50af) ||
		(reg >= 0x5290 && reg <= 0x52af) ||
		(reg >= 0x52b1 && reg <= 0x52fe) ||
		(reg >= 0x3e00 && reg <= 0x3e81) ||
		(reg >= 0x3f00 && reg <= 0x3f81) ||
		(reg >= 0x3e90 && reg <= 0x3eaf) ||
		(reg >= 0x3f90 && reg <= 0x3faf) ||
		(reg >= 0x3fb1 && reg <= 0x3ffe) ||
		(reg >= 0x50b1 && reg <= 0x50fe) ||
		(reg >= 0x7500 && reg <= 0x75b0) ||
		reg == 0x508a || reg == 0x508b ||
		reg == 0x5134 || reg == 0x5334 ||
		(reg == 0x52c1 || reg == 0x77c1 ||
		reg == 0x52e2 || reg == 0x77e2 ||
		reg == 0x52e9 || reg == 0x77e9 ||
		reg == 0x52c0 || reg == 0x77c0 ||
		reg == 0x5a40 || reg == 0x5a80 ||
		reg == 0x5a41 || reg == 0x5a81 ||
		reg == 0x5a56 || reg == 0x5a96 ||
		reg == 0x5a57 || reg == 0x5a97 ||
		reg == 0x5ae9 || reg == 0x5aea ||
		reg == 0x5ad9 || reg == 0x5ada ||
		reg == 0x5ad7 || reg == 0x5ad8 ||
		reg == 0x52fc || reg == 0x77fc ||
		reg == 0x52fd || reg == 0x77fd ||
		reg == 0x52fe || reg == 0x77fe) ||
		(reg >= 0x7700 && reg <= 0x77b0))
		table_index = 2;

	if (reg == 0x2880 || reg == 0x2980 ||
		reg == 0x2a80 || reg == 0x32a0 ||
		reg == 0x31a0 || reg == 0x2ba0 ||
		reg == 0x19a0 || reg == 0x3280 ||
		reg == 0x2e80 || reg == 0x1d40 ||
		(reg >= 0x1d6a && reg <= 0x1d6e) ||
		(reg >= 0x39d4 && reg <= 0x39d6) ||
		(reg >= 0x05b4 && reg <= 0x05b6) ||
		(reg >= 0x14e9 && reg <= 0x14eb) ||
		(reg >= 0x1400 && reg <= 0x1402) ||
		(reg >= 0x14b4 && reg <= 0x14b6) ||
		reg == 0x2e63 || reg == 0x2863 ||
		reg == 0x2763 || reg == 0x2663 ||
		reg == 0x1da1 || reg == 0x1d70 ||
		reg == 0x1d71 || reg == 0x20f ||
		(reg >= 0x200 && reg <= 0x20a) ||
		reg == 0x39d0 || reg == 0x39d1 ||
		reg == 0x39d2 || reg == 0x39d3)
		table_index = 3;

	return table_index;
}

/* vsync for vpp_top0 */
static inline void VSYNC_WRITE_VPP_REG(u32 reg,
				       const u32 value)
{
	int index;
	u32 reg_offset;

	reg_offset = offset_addr(reg);

	/*table*/
	index = index_rdma_part_ins(reg_offset);

	if (index)
		VSYNC_WR_TABLE_REG(index, reg_offset, value);
	else
		VSYNC_WR_MPEG_REG(reg_offset, value);
}

static inline u32 VSYNC_READ_VPP_REG(u32 reg)
{
	int index;
	u32 reg_offset;

	reg_offset = offset_addr(reg);

	index = index_rdma_part_ins(reg_offset);

	if (index)
		return VSYNC_RD_TABLE_REG(index, reg_offset);
	else
		return VSYNC_RD_MPEG_REG(reg_offset);
}

static inline void VSYNC_WRITE_VPP_REG_EX(u32 reg,
					  const u32 value,
					  bool add_offset)
{
	int index;

	if (add_offset)
		reg = offset_addr(reg);

	index = index_rdma_part_ins(reg);

	if (index)
		VSYNC_WR_TABLE_REG(index, reg, value);
	else
		VSYNC_WR_MPEG_REG(reg, value);
}

static inline void VSYNC_WRITE_VPP_REG_BITS_EX(u32 reg,
		const u32 value,
		const u32 start,
		const u32 len,
		bool add_offset)
{
	int index;

	if (add_offset)
		reg = offset_addr(reg);
	index = index_rdma_part_ins(reg);

	if (index)
		VSYNC_WR_TABLE_REG_BITS(index, reg, value, start, len);
	else
		VSYNC_WR_MPEG_REG_BITS(reg, value, start, len);
}

static inline u32 VSYNC_READ_VPP_REG_EX(u32 reg,
					bool add_offset)
{
	int index;

	if (add_offset)
		reg = offset_addr(reg);

	index = index_rdma_part_ins(reg);
	if (index)
		return VSYNC_RD_TABLE_REG(index, reg);
	else
		return VSYNC_RD_MPEG_REG(reg);
}

static inline void VSYNC_WRITE_VPP_REG_BITS(u32 reg,
					    const u32 value,
		const u32 start,
		const u32 len)
{
	int index;

	reg = offset_addr(reg);
	index = index_rdma_part_ins(reg);

	if (index)
		VSYNC_WR_TABLE_REG_BITS(index, reg, value, start, len);
	else
		VSYNC_WR_MPEG_REG_BITS(reg, value, start, len);
}

/* vsync for vpp_top1 */
static inline void VSYNC_WRITE_VPP_REG_VPP1(u32 reg,
				       const u32 value)
{
	VSYNC_WR_MPEG_REG_VPP1(offset_addr(reg), value);
}

static inline u32 VSYNC_READ_VPP_REG_VPP1(u32 reg)
{
	return VSYNC_RD_MPEG_REG_VPP1(offset_addr(reg));
}

static inline void VSYNC_WRITE_VPP_REG_EX_VPP1(u32 reg,
					  const u32 value,
					  bool add_offset)
{
	if (add_offset)
		reg = offset_addr(reg);
	VSYNC_WR_MPEG_REG_VPP1(reg, value);
}

static inline u32 VSYNC_READ_VPP_REG_EX_VPP1(u32 reg,
					bool add_offset)
{
	if (add_offset)
		reg = offset_addr(reg);
	return VSYNC_RD_MPEG_REG_VPP1(reg);
}

static inline void VSYNC_WRITE_VPP_REG_BITS_VPP1(u32 reg,
					    const u32 value,
		const u32 start,
		const u32 len)
{
	VSYNC_WR_MPEG_REG_BITS_VPP1(offset_addr(reg), value, start, len);
}

/* vsync for vpp_top2 */
static inline void VSYNC_WRITE_VPP_REG_VPP2(u32 reg,
				       const u32 value)
{
	VSYNC_WR_MPEG_REG_VPP2(offset_addr(reg), value);
}

static inline u32 VSYNC_READ_VPP_REG_VPP2(u32 reg)
{
	return VSYNC_RD_MPEG_REG_VPP2(offset_addr(reg));
}

static inline void VSYNC_WRITE_VPP_REG_EX_VPP2(u32 reg,
					  const u32 value,
					  bool add_offset)
{
	if (add_offset)
		reg = offset_addr(reg);
	VSYNC_WR_MPEG_REG_VPP2(reg, value);
}

static inline u32 VSYNC_READ_VPP_REG_EX_VPP2(u32 reg,
					bool add_offset)
{
	if (add_offset)
		reg = offset_addr(reg);
	return VSYNC_RD_MPEG_REG_VPP2(reg);
}

static inline void VSYNC_WRITE_VPP_REG_BITS_VPP2(u32 reg,
					    const u32 value,
		const u32 start,
		const u32 len)
{
	VSYNC_WR_MPEG_REG_BITS_VPP2(offset_addr(reg), value, start, len);
}

static inline void VSYNC_WR_MPEG_REG_BITS_S5(u32 reg,
		      const u32 value,
		      const u32 start,
		      const u32 len)
{
	int index;

	index = index_rdma_part_ins(reg);
	if (index)
		VSYNC_WR_TABLE_REG(index, reg, ((aml_read_vcbus(reg) &
			     ~(((1L << (len)) - 1) << (start))) |
			    (((value) & ((1L << (len)) - 1)) << (start))));
	else
		VSYNC_WR_MPEG_REG(reg, ((aml_read_vcbus(reg) &
			     ~(((1L << (len)) - 1) << (start))) |
			    (((value) & ((1L << (len)) - 1)) << (start))));
}

/* vsync for vpp_top_sel */
static inline void VSYNC_WRITE_VPP_REG_VPP_SEL(u32 reg,
				       const u32 value, int vpp_sel)
{
	int index;
	u32 reg1;

	reg1 = offset_addr(reg);
	index = index_rdma_part_ins(reg1);

	if (vpp_sel == 0xff) {
		aml_write_vcbus_s(reg1, value);
	} else if (vpp_sel == 0xfe) {
		aml_write_vcbus(reg, value);
	} else if (vpp_sel == 3) {
		if (index)
			PRE_VSYNC_WR_TABLE_REG(index, reg1, value);
		else
			PRE_VSYNC_WR_MPEG_REG(reg1, value);
	} else if (vpp_sel == 2) {
		VSYNC_WR_MPEG_REG_VPP2(reg1, value);
	} else if (vpp_sel == 1) {
		VSYNC_WR_MPEG_REG_VPP1(reg1, value);
	} else {
		if (index)
			VSYNC_WR_TABLE_REG(index, reg1, value);
		else
			VSYNC_WR_MPEG_REG(reg1, value);
	}
}

static inline u32 VSYNC_READ_VPP_REG_VPP_SEL(u32 reg, int vpp_sel)
{
	int index;
	u32 reg1;
	u32 ret = 0;

	reg1 = offset_addr(reg);
	index = index_rdma_part_ins(reg1);

	if (vpp_sel == 0xff) {
		ret = aml_read_vcbus_s(reg1);
	} else if (vpp_sel == 0xfe) {
		ret = aml_read_vcbus(reg);
	} else if (vpp_sel == 3) {
		if (index)
			ret = PRE_VSYNC_RD_TABLE_REG(index, reg1);
		else
			ret = PRE_VSYNC_RD_MPEG_REG(reg1);
	} else if (vpp_sel == 2) {
		ret = VSYNC_RD_MPEG_REG_VPP2(reg1);
	} else if (vpp_sel == 1) {
		ret = VSYNC_RD_MPEG_REG_VPP1(reg1);
	} else {
		if (index)
			ret = VSYNC_RD_TABLE_REG(index, reg1);
		else
			ret = VSYNC_RD_MPEG_REG(reg1);
	}

	return ret;
}

static inline void VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(u32 reg,
					    const u32 value,
		const u32 start,
		const u32 len, int vpp_sel)
{
	int index;
	u32 reg1;

	reg1 = offset_addr(reg);
	index = index_rdma_part_ins(reg1);

	if (vpp_sel == 0xff) {
		aml_vcbus_update_bits_s(reg1, value, start, len);
	} else if (vpp_sel == 0xfe) {
		VSYNC_WR_MPEG_REG_BITS_S5(reg, value, start, len);
	} else if (vpp_sel == 3) {
		if (index)
			PRE_VSYNC_WR_TABLE_REG_BITS(index, reg1, value, start, len);
		else
			PRE_VSYNC_WR_MPEG_REG_BITS(reg1, value, start, len);
	} else if (vpp_sel == 2) {
		VSYNC_WR_MPEG_REG_BITS_VPP2(reg1, value, start, len);
	} else if (vpp_sel == 1) {
		VSYNC_WR_MPEG_REG_BITS_VPP1(reg1, value, start, len);
	} else {
		if (index)
			VSYNC_WR_TABLE_REG_BITS(index, reg1, value, start, len);
		else
			VSYNC_WR_MPEG_REG_BITS(reg1, value, start, len);
	}
}

static inline void VSYNC_WRITE_VPP_REG_EX_VPP_SEL(u32 reg,
					  const u32 value,
					  bool add_offset, int vpp_sel)
{
	int index;

	if (add_offset)
		reg = offset_addr(reg);

	index = index_rdma_part_ins(reg);

	if (vpp_sel == 3) {
		if (index)
			PRE_VSYNC_WR_TABLE_REG(index, reg, value);
		else
			PRE_VSYNC_WR_MPEG_REG(reg, value);
	} else if (vpp_sel == 2) {
		VSYNC_WR_MPEG_REG_VPP2(reg, value);
	} else if (vpp_sel == 1) {
		VSYNC_WR_MPEG_REG_VPP1(reg, value);
	} else {
		if (index)
			VSYNC_WR_TABLE_REG(index, reg, value);
		else
			VSYNC_WR_MPEG_REG(reg, value);
	}
}

static inline u32 VSYNC_READ_VPP_REG_EX_VPP_SEL(u32 reg,
					bool add_offset, int vpp_sel)
{
	int index;

	if (add_offset)
		reg = offset_addr(reg);

	index = index_rdma_part_ins(reg);

	if (vpp_sel == 3) {
		if (index)
			return PRE_VSYNC_RD_TABLE_REG(index, reg);
		else
			return PRE_VSYNC_RD_MPEG_REG(reg);
	} else if (vpp_sel == 2) {
		return VSYNC_RD_MPEG_REG_VPP2(reg);
	} else if (vpp_sel == 1) {
		return VSYNC_RD_MPEG_REG_VPP1(reg);
	} else {
		if (index)
			return VSYNC_RD_TABLE_REG(index, reg);
		else
			return VSYNC_RD_MPEG_REG(reg);
	}
}

static inline void VSYNC_WRITE_VPP_REG_BITS_EX_VPP_SEL(u32 reg,
		const u32 value,
		const u32 start,
		const u32 len,
		bool add_offset,
		int vpp_sel)
{
	int index;

	if (add_offset)
		reg = offset_addr(reg);

	index = index_rdma_part_ins(reg);
	if (vpp_sel == 3) {
		if (index)
			PRE_VSYNC_WR_TABLE_REG_BITS(index, reg, value, start, len);
		else
			PRE_VSYNC_WR_MPEG_REG_BITS(reg, value, start, len);
	} else if (vpp_sel == 2) {
		VSYNC_WR_MPEG_REG_BITS_VPP2(reg, value, start, len);
	} else if (vpp_sel == 1) {
		VSYNC_WR_MPEG_REG_BITS_VPP1(reg, value, start, len);
	} else {
		if (index)
			VSYNC_WR_TABLE_REG_BITS(index, reg, value, start, len);
		else
			VSYNC_WR_MPEG_REG_BITS(reg, value, start, len);
	}
}

#endif
