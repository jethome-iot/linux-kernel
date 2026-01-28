/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#ifndef _AMFC_REGS_H_
#define _AMFC_REGS_H_

#define CLKCTRL_AMFC_CLK_CTRL			((0x0065  << 2))

#define AMFC_GL_VERSION				((0x0 << 2))
//Bit 31:16        reserved
//Bit 15:8         ro_major_version
//Bit  7:0         ro_minor_version

#define AMFC_GL_MISC				((0x1 << 2))
//Bit 31:1         reserved
//Bit  0           reg_global_irq_en

#define AMFC_GL_CMD0_DESC_BASE_ADDR		((0x2 << 2))
//Bit 31: 0        reg_cmd0_desc_base_addr

#define AMFC_GL_CMD0_CURR_DESC_ADDR		((0x3 << 2))
//Bit 31: 0        ro_cmd0_cur_desc_addr

#define AMFC_GL_CMD0_CONTROL			((0x4 << 2))
//Bit 31           reg_cmd0_sw_rst
//Bit 30: 2        reserved
//Bit 1            reg_cmd0_sw_terminate
//Bit 0            reg_cmd0_sw_start

#define AMFC_GL_CMD0_CONFIG			((0x5 << 2))
//Bit 31: 0        reg_cmd0_config

#define AMFC_GL_CMD0_STATUS			((0x6 << 2))
//Bit 31: 24       ro_cmd0_irq
//Bit 23: 16       reserved
//Bit 15: 8        ro_cmd0_err_code
//Bit 7 : 0        ro_cmd0_status

#define AMFC_GL_CMD0_FEATURE			((0x7 << 2))
//Bit 31: 5        reserved
//Bit 4            ro_cmd0_feat_zlib
//Bit 3            ro_cmd0_feat_deflate
//Bit 2            ro_cmd0_feat_gzip
//Bit 1            ro_cmd0_feat_lz4
//Bit 0            ro_cmd0_feat_zstd

#define AMFC_GL_CMD0_IRQCLR			((0x8 << 2))
//Bit 31: 8        reserved
//Bit 7 : 0        reg_cmd0_irq_clr

//--------------------------

#define AMFC_GL_CMD1_DESC_BASE_ADDR		((0x12 << 2))
//Bit 31: 0        reg_cmd1_desc_base_addr

#define AMFC_GL_CMD1_CURR_DESC_ADDR		((0x13 << 2))
//Bit 31: 0        ro_cmd1_cur_desc_addr

#define AMFC_GL_CMD1_CONTROL			((0x14 << 2))
//Bit 31           reg_cmd1_sw_rst
//Bit 30: 2        reserved
//Bit 1            reg_cmd1_sw_terminate
//Bit 0            reg_cmd1_sw_start

#define AMFC_GL_CMD1_CONFIG			((0x15 << 2))
//Bit 31: 0        reg_cmd1_config

#define AMFC_GL_CMD1_STATUS			((0x16 << 2))
//Bit 31: 24       ro_cmd1_irq
//Bit 23: 16       reserved
//Bit 15: 8        ro_cmd1_err_code
//Bit 7 : 0        ro_cmd1_status

#define AMFC_GL_CMD1_FEATURE			((0x17 << 2))
//Bit 31: 5        reserved
//Bit 4            ro_cmd1_feat_zlib
//Bit 3            ro_cmd1_feat_deflate
//Bit 2            ro_cmd1_feat_gzip
//Bit 1            ro_cmd1_feat_lz4
//Bit 0            ro_cmd1_feat_zstd

#define AMFC_GL_CMD1_IRQCLR			((0x18 << 2))
//Bit 31: 8        reserved
//Bit 7 : 0        reg_cmd1_irq_clr

#define AMFC_CMD0_TIME_MEASURE			((0x001c  << 2))
#define AMFC_CMD1_TIME_MEASURE			((0x001d  << 2))
#define AMFC_DECOMPR_STATUS4			((0x002e  << 2))

//---------------------------------------------
//Gate clock control
#define AMFC_CMD_GATE_CLK_CTRL			((0x20 << 2))
//Bit 31: 0        reg_cmd_gclk_ctrl

#define AMFC_ENC_GATE_CTRL_CTRL_0		((0x21 << 2))
//Bit 31: 0        reg_enc_gclk_ctrl_0

#define AMFC_ENC_GATE_CTRL_CTRL_1		((0x22 << 2))
//Bit 31: 0        reg_enc_gclk_ctrl_1

#define AMFC_DEC_GATE_CTRL_CTRL_0		((0x23 << 2))
//Bit 31: 0        reg_dec_gclk_ctrl_0

#define AMFC_DEC_GATE_CTRL_CTRL_1		((0x24 << 2))
//Bit 31: 0        reg_dec_gclk_ctrl_1

//CODEC
#define AMFC_CODEC_CTRL				((0x28 << 2))
//Bit 31:8         reserved
//Bit  7:6         reg_cmd1_dst_end_mode
//Bit  5:4         reg_cmd0_dst_end_mode
//Bit  3           reserved
//Bit  2           reg_cmds_split_mode
//Bit  1           reg_decmpr_enable
//Bit  0           reg_cmpr_enable

#define AMFC_COMPR_STATUS			((0x29 << 2))
//Bit 31:0         ro_cmpr_status

#define AMFC_DECOMPR_STATUS			((0x2a << 2))
//Bit 31:0         ro_decmpr_status

//----------------------------------
#define AMFC_ZSTD_MODE_MISC			((0x30 << 2))
//Bit 31:2         reserved
//Bit  1           reg_enc_lz77_disable
//Bit  0           reg_fse_default_allowed

#define AMFC_ZSTD_HASH_TBL_INIT			((0x31 << 2))
//Bit 31:6         reserved
//Bit 5:4          reg_hash_tbl_init_mode
//Bit  3:1         reserved

//------------------------------------------
//MIF register
#define AMFC_WR_MIF_CTRL			((0x40 << 2))
//Bit  31:24   reserved
//Bit  23:16   reg_wrmif_canvas_id
//Bit  15:11   reserved
//Bit  10:8    reg_wrmif_burst_len
//Bit  7:6     reserved
//Bit  5       reg_wrmif_swap_64bit
//Bit  4       reg_wrmif_little_endian
//Bit  3:1     reserved
//Bit  0       reg_wrmif_enable

#define AMFC_WR_MIF_STATUS			((0x41 << 2))
//Bit  31:0    ro_wrmif_status

#define AMFC_RD_MIF_CTRL			((0x42 << 2))
//Bit  31:24   reserved
//Bit  23:16   reg_rdmif_canvas_id
//Bit  15:11   reserved
//Bit  10:8    reg_rdmif_burst_len
//Bit  7:6     reserved
//Bit  5       reg_rdmif_swap_64bit
//Bit  4       reg_rdmif_little_endian
//Bit  3:1     reserved
//Bit  0       reg_rdmif_enable

#define AMFC_RD_MIF_STATUS			((0x43 << 2))
//Bit  31:0    ro_rdmif_status

#define AMFC_MIF_QOS_UGT			((0x44 << 2))
//Bit  31:16   reserved
//Bit  15      reg_decmpr_arugt
//Bit  14      reg_decmpr_arqos
//Bit  13      reg_decmpr_awugt
//Bit  12      reg_decmpr_awqos
//Bit  11      reg_cmpr_arugt
//Bit  10      reg_cmpr_arqos
//Bit   9      reg_cmpr_awugt
//Bit   8      reg_cmpr_awqos
//Bit   7      reg_cmd1_arugt
//Bit   6      reg_cmd1_arqos
//Bit   5      reg_cmd1_awugt
//Bit   4      reg_cmd1_awqos
//Bit   3      reg_cmd0_arugt
//Bit   2      reg_cmd0_arqos
//Bit   1      reg_cmd0_awugt
//Bit   0      reg_cmd0_awqos

#endif
