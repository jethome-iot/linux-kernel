/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/video_sink/video_safa_reg.h
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

#ifndef VIDEO_SAFA_REG_HH
#define VIDEO_SAFA_REG_HH

#define VPP_VSR_TOP_MISC           0x5000
//Bit 31:16     reg_sync_ctrl    // unsigned ,    RW, default = 0
//Bit 15:3         reserved
//Bit 2         reg_use_inp_fmt  // unsigned ,    RW, default = 1
//Bit 1         reg_inp_422      // unsigned ,    RW, default = 0
//Bit 0         reg_vsr_en       // unsigned ,    RW, default = 0
#define VPP_VSR_TOP_GCLK_CTRL     0x5001
//Bit 31:0      reg_gclk_ctrl    // unsigned ,    RW, default = 0
//
#define VPP_VSR_TOP_IN_SIZE       0x5002
//Bit 31:16     reg_in_vsize     // unsigned ,    RW, default = 2160
//Bit 15:0      reg_in_hsize     // unsigned ,    RW, default = 3840
#define VPP_VSR_TOP_OUT_SIZE      0x5004
//Bit 31:16     reg_out_vsize    // unsigned ,    RW, default = 2160
//Bit 15:0      reg_out_hsize    // unsigned ,    RW, default = 3840
#define VPP_VSR_TOP_C42C44_MODE   0x5008
//Bit 31: 8        reserved
//Bit  7            reserved
//Bit  6        reg_422to444_en// unsigned ,    RW, default = 1
//Bit  5: 4     reg_422to444_mode         // unsigned ,    RW, default = 2
//Bit  3            reserved
//Bit  2        reg_444to422_en// unsigned ,    RW, default = 1
//Bit  1: 0     reg_444to422_mode         // unsigned ,    RW, default = 2
#define VPP_VSR_PPS_DUMMY_DATA    0x5009
//Bit 31:24        reserved
//Bit 23: 0     reg_pps_dummy_data        // unsigned ,    RW, default = 0
#define VPP_VSR_DEBUG_MODE        0x500a
//Bit 31: 8        reserved
//Bit  7: 0     reg_vsr_debug_mode        // unsigned ,    RW, default = 0
#define VPP_VSR_DITHER_MODE       0x500b
//Bit 31        reg_vsr_dither_frmcnt_ini // unsigned ,    RW
				//default = 0  write 1 to clear frmcnt to 0, read is always 0
//Bit 30: 4        reserved
//Bit  3            reserved
//Bit  2        reg_vsr_dither_en         // unsigned ,    RW, default = 0
//Bit  1: 0     reg_vsr_dither_mode       // unsigned ,    RW, default = 0

#define VPP_PI_MISC                0x5040
//Bit 31:24     reg_sync_ctrl    // unsigned ,    RW, default = 0
//Bit 23:16     reg_inp_hblank1  // unsigned ,    RW, default = 0
//Bit 15:8      reg_inp_hblank0  // unsigned ,    RW, default = 0
//Bit 7:2          reserved
//Bit 1         reg_hf_pps_bypss_mode       // unsigned ,    RW, default = 1
//Bit 0         reg_inp_hold_en  // unsigned ,    RW, default = 1
#define VPP_PI_GCLK_CTRL           0x5041
//Bit 31:0      reg_gclk_ctrl   // unsigned ,    RW, default = 0
#define VPP_PI_EN_MODE             0x5048
//Bit 31: 9        reserved
//Bit  8        reg_input_422  // unsigned ,    RW, default = 0
				//sharpen input is 422 or not
//Bit  7: 5        reserved
//Bit  4        reg_pi_en      // unsigned ,    RW, default = 0
//Bit  3: 2        reserved
//Bit  1: 0     reg_pi_out_scl_mode       // unsigned ,  RW, default = 0
#define VPP_PI_DICT_NUM            0x5049
//Bit 31: 7        reserved
//Bit  6: 0     reg_pi_dict_num// unsigned ,    RW, default = 32
#define VPP_PI_HF_COEF             0x504a
//Bit 31:21        reserved
//Bit 20:12     reg_pi_hpf_coef_2         // signed ,    RW, default = -3
//Bit 11: 9        reserved
//Bit  8: 0     reg_pi_hpf_coef_1         // signed ,    RW, default = -21
#define VPP_PI_HF_COEF_F           0x504b
//Bit 31: 9        reserved
//Bit  8: 0     reg_pi_hpf_coef_0         // signed ,    RW, default = 96
#define VPP_PI_HPF_NORM_CORING    0x504c
//Bit 31:10        reserved
//Bit  9: 8     reg_pi_hpf_norm_rs        // unsigned ,    RW, default = 2
//Bit  7: 0     reg_pi_hp_coring          // unsigned ,    RW, default = 0
#define VPP_PI_WIN_OFST            0x504d
//Bit 31:12        reserved
//Bit 11: 8     reg_pi_out_win // unsigned ,    RW, default = 4
//Bit  7: 4     reg_pi_out_ofst// unsigned ,    RW, default = 1
//Bit  3: 0     reg_pi_in_win  // unsigned ,    RW, default = 3
#define VPP_PI_HF_SCL_COEF_0      0x504e
//Bit 31:24     reg_pi_pps_coef_tap2_3_0  // unsigned ,    RW, default = 128
//Bit 23:16     reg_pi_pps_coef_tap2_2_0  // unsigned ,    RW, default = 128
//Bit 15: 8     reg_pi_pps_coef_tap2_1_0  // unsigned ,    RW, default = 128
//Bit  7: 0     reg_pi_pps_coef_tap2_0_0  // unsigned ,    RW, default = 128
#define VPP_PI_HF_SCL_COEF_1      0x504f
//Bit 31:24     reg_pi_pps_coef_tap2_7_0  // unsigned ,    RW, default = 0
//Bit 23:16     reg_pi_pps_coef_tap2_6_0  // unsigned ,    RW, default = 0
//Bit 15: 8     reg_pi_pps_coef_tap2_5_0  // unsigned ,    RW, default = 0
//Bit  7: 0     reg_pi_pps_coef_tap2_4_0  // unsigned ,    RW, default = 0
#define VPP_PI_HF_SCL_COEF_2      0x5050
//Bit 31:24     reg_pi_pps_coef_tap2_11_0 // unsigned ,    RW, default = 127
//Bit 23:16     reg_pi_pps_coef_tap2_10_0 // unsigned ,    RW, default = 127
//Bit 15: 8     reg_pi_pps_coef_tap2_9_0  // unsigned ,    RW, default = 127
//Bit  7: 0     reg_pi_pps_coef_tap2_8_0  // unsigned ,    RW, default = 127
#define VPP_PI_HF_SCL_COEF_3      0x5051
//Bit 31:24     reg_pi_pps_coef_tap2_15_0 // unsigned ,    RW, default = 1
//Bit 23:16     reg_pi_pps_coef_tap2_14_0 // unsigned ,    RW, default = 1
//Bit 15: 8     reg_pi_pps_coef_tap2_13_0 // unsigned ,    RW, default = 1
//Bit  7: 0     reg_pi_pps_coef_tap2_12_0 // unsigned ,    RW, default = 1
#define VPP_PI_HF_SCL_COEF_4      0x5052
//Bit 31:24     reg_pi_pps_coef_tap2_19_0 // unsigned ,    RW, default = 126
//Bit 23:16     reg_pi_pps_coef_tap2_18_0 // unsigned ,    RW, default = 126
//Bit 15: 8     reg_pi_pps_coef_tap2_17_0 // unsigned ,    RW, default = 126
//Bit  7: 0     reg_pi_pps_coef_tap2_16_0 // unsigned ,    RW, default = 126
#define VPP_PI_HF_SCL_COEF_5      0x5053
//Bit 31:24     reg_pi_pps_coef_tap2_23_0 // unsigned ,    RW, default = 2
//Bit 23:16     reg_pi_pps_coef_tap2_22_0 // unsigned ,    RW, default = 2
//Bit 15: 8     reg_pi_pps_coef_tap2_21_0 // unsigned ,    RW, default = 2
//Bit  7: 0     reg_pi_pps_coef_tap2_20_0 // unsigned ,    RW, default = 2
#define VPP_PI_HF_SCL_COEF_6      0x5054
//Bit 31:24     reg_pi_pps_coef_tap2_27_0 // unsigned ,    RW, default = 124
//Bit 23:16     reg_pi_pps_coef_tap2_26_0 // unsigned ,    RW, default = 124
//Bit 15: 8     reg_pi_pps_coef_tap2_25_0 // unsigned ,    RW, default = 124
//Bit  7: 0     reg_pi_pps_coef_tap2_24_0 // unsigned ,    RW, default = 124
#define VPP_PI_HF_SCL_COEF_7      0x5055
//Bit 31:24     reg_pi_pps_coef_tap2_31_0 // unsigned ,    RW, default = 4
//Bit 23:16     reg_pi_pps_coef_tap2_30_0 // unsigned ,    RW, default = 4
//Bit 15: 8     reg_pi_pps_coef_tap2_29_0 // unsigned ,    RW, default = 4
//Bit  7: 0     reg_pi_pps_coef_tap2_28_0 // unsigned ,    RW, default = 4
#define VPP_PI_HF_SCL_COEF_F      0x5056
//Bit 31: 8        reserved
//Bit  7: 0     reg_pi_pps_coef_tap2_32_0 // unsigned ,    RW, default = 99
#define VPP_PI_HF_HSC_PART        0x5057
//Bit 31:28        reserved
//Bit 27: 4     reg_pi_hf_hsc_fraction_part // unsigned ,    RW, default = 1
//Bit  3: 0     reg_pi_hf_hsc_integer_part // unsigned ,    RW, default = 0
#define VPP_PI_HF_HSC_INI         0x5058
//Bit 31:29        reserved
//Bit 28:24     reg_pi_hf_hsc_ini_integer // signed ,    RW, default = -1
//Bit 23:17        reserved
//Bit 16: 0     reg_pi_hf_hsc_ini_phase   // unsigned ,    RW, default = 0
#define VPP_PI_HF_VSC_PART        0x5059
//Bit 31:28        reserved
//Bit 27: 4     reg_pi_hf_vsc_fraction_part // unsigned ,    RW, default = 1
//Bit  3: 0     reg_pi_hf_vsc_integer_part // unsigned ,    RW, default = 0
#define VPP_PI_HF_VSC_INI         0x505a
//Bit 31:29        reserved
//Bit 28:24     reg_pi_hf_vsc_ini_integer // signed ,    RW, default = -1
//Bit 23:17        reserved
//Bit 16: 0     reg_pi_hf_vsc_ini_phase   // unsigned ,    RW, default = 0
#define VPP_PI_IN_HSC_PART        0x505b
//Bit 31:28        reserved
//Bit 27: 4     reg_pi_hsc_fraction_part  // unsigned ,    RW, default = 1
//Bit  3: 0     reg_pi_hsc_integer_part   // unsigned ,    RW, default = 0
#define VPP_PI_IN_HSC_INI         0x505c
//Bit 31:21        reserved
//Bit 20:16     reg_pi_hsc_ini_integer    // signed ,    RW, default = -1
//Bit 15: 0     reg_pi_hsc_ini_phase      // unsigned ,    RW, default = 0
#define VPP_PI_IN_VSC_PART        0x505d
//Bit 31:28        reserved
//Bit 27: 4     reg_pi_vsc_fraction_part  // unsigned ,    RW, default = 1
//Bit  3: 0     reg_pi_vsc_integer_part   // unsigned ,    RW, default = 0
#define VPP_PI_IN_VSC_INI         0x505e
//Bit 31:21        reserved
//Bit 20:16     reg_pi_vsc_ini_integer    // signed ,    RW, default = -1
//Bit 15: 0     reg_pi_vsc_ini_phase      // unsigned ,    RW, default = 0
#define VPP_PI_PPS_NOR_RS_BITS    0x505f
//Bit 31: 4        reserved
//Bit  3: 0     reg_pi_pps_nor_rs_bits    // unsigned ,    RW,default = 7,
				//normalize right shift bits of hsc
#define VPP_PI_MM_WIN_INTERP_EN   0x5060
//Bit 31: 9        reserved
//Bit  8        reg_pi_mm_win_x// unsigned ,    RW, default = 0  fix 0
//Bit  7: 5        reserved
//Bit  4        reg_pi_mm_win_y// unsigned ,    RW, default = 0  fix 0
//Bit  3: 1        reserved
//Bit  0        reg_pi_mm_interp_en       // unsigned ,    RW, default = 1
#define SAFA_PPS_SR_422_EN        0x5100
//Bit 31: 5        reserved
//Bit  4        reg_sr_en      // unsigned ,    RW, default = 0  enable of sr
//Bit  3: 1        reserved
//Bit  0        reg_input_422  // unsigned ,    RW, default = 1  safa input is 422 or not
#define SAFA_PPS_VSC_START_PHASE_STEP              0x5101
//Bit 31:28        reserved
//Bit 27:24     reg_vsc_integer_part      // unsigned ,    RW, default = 1,
				//vertical	start	phase	step,
				//(source/dest)*(2^24),integer	part	of	step
//Bit 23: 0     reg_vsc_fraction_part     // unsigned ,    RW, default = 0,
				//vertical	start	phase	step,
				//(source/dest)*(2^24),fraction	part	of	step
#define SAFA_PPS_HSC_START_PHASE_STEP              0x5102
//Bit 31:28        reserved
//Bit 27:24     reg_hsc_integer_part      // unsigned ,    RW, default = 1,
				//integer	part	of	step
//Bit 23: 0     reg_hsc_fraction_part     // unsigned ,    RW, default = 0,
				//fraction	part	of	step
#define SAFA_PPS_SC_MISC           0x5103
//Bit 31:13        reserved
//Bit 12        reg_repeat_last_line_en   // unsigned ,    RW, default = 0
				//repeat	last	line
//Bit 11: 9        reserved
//Bit  8        reg_prehsc_en  // unsigned ,    RW, default = 0, prehsc_en
//Bit  7: 5        reserved
//Bit  4        reg_prevsc_en  // unsigned ,    RW, default = 0, prevsc_en
//Bit  3: 0     reg_prehsc_mode// unsigned ,    RW, default = 0, prehsc_mode,
				//bit	3:2,	prehsc	odd	line	interp	mode,
				//bit	1:0,	prehsc	even	line	interp	mode,
				//each	2bit,	0:00	pix0+pix1/2,	average,	1:00
#define SAFA_PPS_HSC_INI_PAT_CTRL 0x5104
//Bit 31:16        reserved
//Bit 15: 8     reg_prehsc_pattern        // unsigned ,    RW, default = 0,
				//prehsc	pattern,
				//each	patten	1	bit,	from	lsb	->	msb
//Bit  7            reserved
//Bit  6: 4     reg_prehsc_pat_star       // unsigned ,    RW, default = 0,
				//prehsc	pattern	start
//Bit  3            reserved
//Bit  2: 0     reg_prehsc_pat_end        // unsigned ,    RW, default = 0,
				//prehsc	pattern	end
#define SAFA_PPS_PRE_HSCALE_COEF_Y1                0x5105
//Bit 31:26        reserved
//Bit 25:16     reg_prehsc_coef_y_3       // signed ,    RW, default = 0
				//default	=	0x00,
				//coefficient2	pre horizontal	filter
//Bit 15:10        reserved
//Bit  9: 0     reg_prehsc_coef_y_2       // signed ,    RW, default = 0
				//default	=	0x40,
				//coefficient3	pre horizontal	filter
#define SAFA_PPS_PRE_HSCALE_COEF_Y0                0x5106
//Bit 31:26        reserved
//Bit 25:16     reg_prehsc_coef_y_1       // signed ,    RW, default = 0
				//default	=	0x00,
				//coefficient0	pre horizontal	filter
//Bit 15:10        reserved
//Bit  9: 0     reg_prehsc_coef_y_0       // signed ,    RW, default = 256
				//default	=	0x00,
				//coefficient1	pre horizontal	filter
#define SAFA_PPS_PRE_HSCALE_COEF_C1                0x5107
//Bit 31:26        reserved
//Bit 25:16     reg_prehsc_coef_c_3       // signed ,    RW, default = 0
				//default	=	0x00,
				//coefficient2	pre horizontal	filter
//Bit 15:10        reserved
//Bit  9: 0     reg_prehsc_coef_c_2       // signed ,    RW, default = 0
				//default	=	0x40,
				//coefficient3	pre horizontal	filter
#define SAFA_PPS_PRE_HSCALE_COEF_C0                0x5108
//Bit 31:26        reserved
//Bit 25:16     reg_prehsc_coef_c_1       // signed ,    RW, default = 0
				//default	=	0x00,
				//coefficient0	pre horizontal	filter
//Bit 15:10        reserved
//Bit  9: 0     reg_prehsc_coef_c_0       // signed ,    RW, default = 256
				//default	=	0x00,
				//coefficient1	pre horizontal	filter
#define SAFA_PPS_PRE_SCALE        0x5109
//Bit 31:20        reserved
//Bit 19:16     reg_prehsc_flt_num_y      // unsigned ,    RW, default = 2
				//default = 2, prehsc filter tap num
//Bit 15:12     reg_prehsc_flt_num_c      // unsigned ,    RW, default = 2
				//default = 2, prehsc filter tap num
//Bit 11: 8     reg_prevsc_flt_num        // unsigned ,    RW, default = 2
				//default = 2, prevsc filter tap num
//Bit  7: 6        reserved
//Bit  5: 4     reg_prehsc_rate// unsigned ,    RW, default = 1
				//default =   0,pre hscale down rate,
				//0:width,1:width/2,
				//2:width/4,3:width/8
//Bit  3: 2        reserved
//Bit  1: 0     reg_prevsc_rate// unsigned ,    RW, default = 1
				//default =   0,pre vscale down rate,
				//0:height,1:height/2,
				//2:height/4, 3:height/8
#define SAFA_PPS_PRE_VSCALE_COEF  0x510a
//Bit 31:26        reserved
//Bit 25:16     reg_prevsc_coef_1         // signed ,    RW, default = 0
				//default	=	0x00,
				//coefficient2	pre vertical	filter
//Bit 15:10        reserved
//Bit  9: 0     reg_prevsc_coef_0         // signed ,    RW, default = 256
				//default	=	0x40,
				//coefficient3	pre vertical	filter
#define SAFA_PPS_VSC_INIT         0x510b
//Bit 31:21        reserved
//Bit 20:16     reg_vsc_ini_integer       // signed ,    RW, default = -1  = 0
//Bit 15: 0     reg_vsc_ini_phase         // unsigned ,    RW, default = 0  = 0
#define SAFA_PPS_HSC_INIT         0x510c
//Bit 31:21        reserved
//Bit 20:16     reg_hsc_ini_integer       // signed ,    RW, default = -1  = 0
//Bit 15: 0     reg_hsc_ini_phase         // unsigned ,    RW, default = 0  = 0
#define SAFA_PPS_INTERP_EN_MODE   0x510d
//Bit 31:26        reserved
//Bit 25        reg_dir_interp_en         // unsigned ,    RW, default = 1
				//the enable signal of directional interpolation
//Bit 24        reg_dir_interp_chroma_en  // unsigned ,    RW, default = 1
				//the enable signal of chroma directional interpolation
//Bit 23:21        reserved
//Bit 20        reg_beta_hf_gain_en       // unsigned ,    RW, default = 1
				//enable of the hf-gain of beta
//Bit 19:17        reserved
//Bit 16        reg_out_alpha_adp_en      // unsigned ,    RW, default = 1
				//the enable of adp out alpha
//Bit 15:14        reserved
//Bit 13:12     reg_out_beta_mode         // unsigned ,    RW, default = 1
				//the out beta mode, 0: org, 1: avg with neighbor,
				//2:min with neighbor, 3: force mode
//Bit 11:10        reserved
//Bit  9: 8     reg_out_adp_tap_alp_mode  // unsigned ,    RW, default = 3
				//adp tap alpha mode, 0: org, 1: avg with neighbor,
				//2:max with neighbor, 3: force mode
//Bit  7: 5        reserved
//Bit  4        reg_adp_tap_chroma_en     // unsigned ,    RW, default = 0
//Bit  3: 1        reserved
//Bit  0        reg_interp_nearest_en     // unsigned ,    RW, default = 0
				//the enable of interpolation nearest
#define SAFA_PPS_YUV_SHARPEN_EN   0x5116
//Bit 31: 5        reserved
//Bit  4        reg_yuv_sharpen_en        // unsigned ,    RW, default = 0
				//0: disable, 1: enable, when >1920 in disable this feature.
//Bit  3: 1        reserved
//Bit  0        reg_yuv_sharpen_win_mode  // unsigned ,    RW, default = 0
				//0: 3x3, 1: 5x5
#define SAFA_PPS_DIR_MIN_IDX_VALID  0x511f
//Bit 31:13        reserved
//Bit 12           reg_delta_chk_min_idx_valid // unsigned ,    RW, default = 0
//Bit 11: 9        reserved
//Bit  8           reg_dir_hist_chk_min_idx_valid // unsigned ,    RW, default = 1
//Bit  7: 5        reserved
//Bit  4           reg_dir_dif_chk_min_idx_valid // unsigned ,    RW, default = 1
//Bit  3: 1        reserved
//Bit  0           reg_beta_chk_min_idx_valid // unsigned ,    RW, default = 0
#define SAFA_PPS_DIR_EN_MODE      0x511e
//Bit 31:25        reserved
//Bit 24        reg_dir_x_ds_en// unsigned ,    RW, default = 1
//Bit 23:21        reserved
//Bit 20        reg_dir_y_ds_en// unsigned ,    RW, default = 0
//Bit 19:17        reserved
//Bit 16        reg_dir_xerr_mode         // unsigned ,    RW, default = 1
//Bit 15:13        reserved
//Bit 12        reg_dir_sad_flt_en        // unsigned ,    RW, default = 1
//Bit 11: 9        reserved
//Bit  8        reg_dir_sad_flt_mode      // unsigned ,    RW, default = 0
				//0: 3-tap, 1: 5-tap
//Bit  7: 4        reserved
//Bit  3: 2        reserved
//Bit  1: 0     reg_dir_blend_range_en    // unsigned ,    RW, default = 1

#define SAFA_PPS_DEJAGGY_CTRL        0x518a
//Bit 31        dejaggy enable
#define S6_SAFA_PPS_BOT_VSC_INIT                 0x5195
//Bit 31:21        reserved
//Bit 20:16        reg_vsc_bot_ini_integer   // signed ,      RW, default = -1
//Bit 15: 0        reg_vsc_bot_ini_phase     // unsigned ,    RW, default = 0

#define SAFA_PPS_HW_CTRL           0x5190
//Bit 31:29        reserved
//Bit 28        reg_field      // unsigned ,    RW, default = 0
//Bit 27:26     reg_frm2fld_en // unsigned ,    RW, default = 0  ,
				//0bit: 0:p 1:i 1bit: 0:field 1:reg_field
//Bit 25:21     reg_vsc_bot_ini_integer   //   signed ,    RW, default = -1
//Bit 20        reg_postvsc_rpt_en        // unsigned ,    RW, default = 1
//Bit 19        reg_hvalp_rptbuf_en       // unsigned ,    RW, default = 1
//Bit 18:11        reserved
//Bit 10: 9     reg_pi_out_vofs// unsigned ,    RW, default = 0
//Bit  8        reg_safa_pps_top_en       // unsigned ,    RW, default = 0  ,
				//reg_safa_pps_top_en
//Bit  7: 6     reg_444to422_mode         // unsigned ,    RW, default = 0
//Bit  5        reg_padding_mode          // unsigned ,    RW, default = 0
//Bit  4        reg_analy_en   // unsigned ,    RW, default = 1  ,reg_postsc_en
//Bit  3        reg_rdout_mode // unsigned ,    RW, default = 0  ,post_vscaler_rd_mode
//Bit  2        reg_postsc_en  // unsigned ,    RW, default = 1  ,reg_postsc_en
//Bit  1        reg_size_mux   // unsigned ,    RW, default = 0  ,hsize sel
//Bit  0        reg_prevsc_outside_en     // unsigned ,    RW, default = 1,
				//video1 scale out enable
#define SAFA_PPS_CNTL_SCALE_COEF_IDX_LUMA          0x5197
//Bit 31:15        reserved
//Bit 14        reg_index_inc_luma        // unsigned ,    RW, default = 0 ,
				//index increment, if bit9 == 1  then
				//(0: index increase 1, 1: index increase 2) else
				//(index increase 2)
//Bit 13        reg_rd_cbus_coef_en_luma  // unsigned ,    RW, default = 0 ,
				//1: read coef through cbus enable,
				//just for debug purpose in case
				//when we wanna check the coef in ram in correct or not
//Bit 12:10        reserved
//Bit  9: 7     reg_type_index_luma       // unsigned ,    RW, default = 0 ,
				//type of index, 00: vertical coef,
				//01: vertical chroma coef: 10: horizontal coef, 11: resevered
//Bit  6: 0     reg_coef_index_luma       // unsigned ,    RW, default = 0 ,
				//coef	index
#define SAFA_PPS_CNTL_SCALE_COEF_LUMA              0x5198
//Bit 31:24     reg_coef0_luma // signed ,      RW, default = 0  ,
				//coefficients for vertical filter and horizontal	filter
//Bit 23:16     reg_coef1_luma // signed ,      RW, default = 0  ,
				//coefficients for vertical filter and horizontal	filter
//Bit 15: 8     reg_coef2_luma // signed ,      RW, default = 0  ,
				//coefficients for vertical filter and horizontal	filter
//Bit  7: 0     reg_coef3_luma // signed ,      RW, default = 0  ,
				//coefficients for vertical filter and horizontal	filter
#define SAFA_PPS_CNTL_SCALE_COEF_IDX_CHRO          0x5199
//Bit 31:15        reserved
//Bit 14        reg_index_inc_chro        // unsigned ,    RW, default = 0 ,
				//index increment,
				//if bit9 == 1
				//then (0: index increase 1, 1: index increase 2)
				//else (index increase 2)
//Bit 13        reg_rd_cbus_coef_en_chro  // unsigned ,    RW, default = 0 ,
				//1: read coef through cbus enable,
				//just for debug purpose in case
				//when we wanna check the coef in ram in correct or not
//Bit 12:10        reserved
//Bit  9: 7     reg_type_index_chro       // unsigned ,    RW, default = 0,
				//type of index, 00: vertical coef,
				//01: vertical chroma coef: 10: horizontal coef, 11: resevered
//Bit  6: 0     reg_coef_index_chro       // unsigned ,    RW, default = 0 ,
				//coef	index
#define SAFA_PPS_CNTL_SCALE_COEF_CHRO              0x519a
//Bit 31:24     reg_coef0_chro // signed ,      RW, default = 0  ,
				//coefficients for vertical filter and horizontal	filter
//Bit 23:16     reg_coef1_chro // signed ,      RW, default = 0  ,
				//coefficients for vertical filter and horizontal	filter
//Bit 15: 8     reg_coef2_chro // signed ,      RW, default = 0  ,
				//coefficients for vertical filter and horizontal	filter
//Bit  7: 0     reg_coef3_chro // signed ,      RW, default = 0  ,
				//coefficients for vertical filter and horizontal	filter
#define SAFA_PPS_SR_ALP_INFO      0x515b
//Bit 31:26        reserved
//Bit 25:24     reg_sr_delta_alp_mode     // unsigned ,    RW, default = 1
//Bit 23:22        reserved
//Bit 21:16     reg_sr_delta_value        // unsigned ,    RW, default = 0
//Bit 15:10        reserved
//Bit  9: 8     reg_sr_gamma_alp_mode     // unsigned ,    RW, default = 1
//Bit  7: 6        reserved
//Bit  5: 0     reg_sr_gamma_value        // unsigned ,    RW, default = 0
#define SAFA_PPS_PI_INFO           0x515c
//Bit 31:22        reserved
//Bit 21:20     reg_pi_gamma_mode         // unsigned ,    RW, default = 1
//Bit 19:18        reserved
//Bit 17:12     reg_pi_gamma_value        // unsigned ,    RW, default = 0
//Bit 11:10        reserved
//Bit  9: 8     reg_pi_max_sad_mode       // unsigned ,    RW, default = 1
//Bit  7: 0     reg_pi_max_sad_value      // unsigned ,    RW, default = 0

#define T6D_SAFA_PPS_HSC_REGION12_STARTP               0x510d
//Bit 31:29        reserved
//Bit 28:16        reg_hsc_region1_startp    // unsigned ,    RW, default = 0  default	=	0,region1	startp
//Bit 15:13        reserved
//Bit 12: 0        reg_hsc_region2_startp    // unsigned ,    RW, default = 0  default	=	0,region2	startp
#define T6D_SAFA_PPS_HSC_REGION34_STARTP               0x510e
//Bit 31:29        reserved
//Bit 28:16        reg_hsc_region3_startp    // unsigned ,    RW, default = 13'h780  default	=	0x780,region3	startp
//Bit 15:13        reserved
//Bit 12: 0        reg_hsc_region4_startp    // unsigned ,    RW, default = 13'h780  default	=	0x780,region4	startp
#define T6D_SAFA_PPS_HSC_REGION4_ENDP                  0x510f
//Bit 31:13        reserved
//Bit 12: 0        reg_hsc_region4_endp      // unsigned ,    RW, default = 1919  default	=	13'd1919,region4	startp
#define T6D_SAFA_PPS_HSC_REGION0_PHASE_SLOPE           0x5110
//Bit 31:25        reserved
//Bit 24: 0        reg_hsc_region0_phase_slope // unsigned ,    RW, default = 0  default	=	0,region0	phase	slope
#define T6D_SAFA_PPS_HSC_REGION1_PHASE_SLOPE           0x5111
//Bit 31:25        reserved
//Bit 24: 0        reg_hsc_region1_phase_slope // unsigned ,    RW, default = 0  default	=	0,region1	phase	slope
#define T6D_SAFA_PPS_HSC_REGION3_PHASE_SLOPE           0x5112
//Bit 31:25        reserved
//Bit 24: 0        reg_hsc_region3_phase_slope // unsigned ,    RW, default = 0  default	=	0,region3	phase	slope
#define T6D_SAFA_PPS_HSC_REGION4_PHASE_SLOPE           0x5113
//Bit 31:25        reserved
//Bit 24: 0        reg_hsc_region4_phase_slope // unsigned ,    RW, default = 0  default	=	0,region4	phase	slope
#define T6D_SAFA_PPS_INTERP_EN_MODE                    0x5114
//Bit 31:27        reserved
//Bit 26           reg_hsc_nonlinear_4region_en // unsigned ,    RW, default = 0
//Bit 25           reg_dir_interp_en         // unsigned ,    RW, default = 1  the enable signal of directional interpolation
//Bit 24           reg_dir_interp_chroma_en  // unsigned ,    RW, default = 1  the enable signal of chroma directional interpolation
//Bit 23:21        reserved
//Bit 20           reg_beta_hf_gain_en       // unsigned ,    RW, default = 1  enable of the hf-gain of beta
//Bit 19:17        reserved
//Bit 16           reg_out_alpha_adp_en      // unsigned ,    RW, default = 1  the enable of adp out alpha
//Bit 15:14        reserved
//Bit 13:12        reg_out_beta_mode         // unsigned ,    RW, default = 1  the out beta mode, 0: org, 1: avg with neighbor, 2:min with neighbor, 3: force mode
//Bit 11:10        reserved
//Bit  9: 8        reg_out_adp_tap_alp_mode  // unsigned ,    RW, default = 2  adp tap alpha mode, 0: org, 1: avg with neighbor, 2:max with neighbor, 3: force mode
//Bit  7: 5        reserved
//Bit  4           reg_adp_tap_chroma_en     // unsigned ,    RW, default = 0
//Bit  3: 2        reserved
//Bit  1           reg_interp_nearest_ver_en // unsigned ,    RW, default = 0  the enable of interpolation nearest
//Bit  0           reg_interp_nearest_hor_en // unsigned ,    RW, default = 0  the enable of interpolation nearest
#define T6D_SAFA_PPS_YUV_SHARPEN_EN                    0x511d
//Bit 31: 5        reserved
//Bit  4           reg_yuv_sharpen_en        // unsigned ,    RW, default = 0  0: disable, 1: enable, when >1920 in disable this feature.
//Bit  3: 1        reserved
//Bit  0           reg_yuv_sharpen_win_mode  // unsigned ,    RW, default = 0  0: 3x3, 1: 5x5
#define T6D_SAFA_PPS_DIR_EN_MODE                       0x5125
//Bit 31:25        reserved
//Bit 24           reg_dir_x_ds_en           // unsigned ,    RW, default = 1
//Bit 23:21        reserved
//Bit 20           reg_dir_y_ds_en           // unsigned ,    RW, default = 0
//Bit 19:17        reserved
//Bit 16           reg_dir_xerr_mode         // unsigned ,    RW, default = 1
//Bit 15:13        reserved
//Bit 12           reg_dir_sad_flt_en        // unsigned ,    RW, default = 1
//Bit 11: 9        reserved
//Bit  8           reg_dir_sad_flt_mode      // unsigned ,    RW, default = 0  0: 3-tap, 1: 5-tap
//Bit  7: 4        reserved
//Bit  3: 2        reserved
//Bit  1: 0        reg_dir_blend_range_en    // unsigned ,    RW, default = 1
#define T6D_SAFA_PPS_HW_CTRL                           0x5196
//Bit 31:29        reserved
//Bit 28           reg_field                 // unsigned ,    RW, default = 0
//Bit 27:26        reg_frm2fld_en            // unsigned ,    RW, default = 0  ,0bit: 0:p 1:i 1bit: 0:field 1:reg_field
//Bit 25:21        reserved
//Bit 20           reg_postvsc_rpt_en        // unsigned ,    RW, default = 1
//Bit 19           reg_posthsc_nonlinear_en  // unsigned ,    RW, default = 0
//Bit 18:11        reserved
//Bit 10: 9        reg_pi_out_vofs           // unsigned ,    RW, default = 0
//Bit  8           reg_safa_pps_top_en       // unsigned ,    RW, default = 0  ,reg_safa_pps_top_en
//Bit  7: 6        reg_444to422_mode         // unsigned ,    RW, default = 0
//Bit  5           reg_padding_mode          // unsigned ,    RW, default = 0
//Bit  4           reg_analy_en              // unsigned ,    RW, default = 0  ,reg_postsc_en
//Bit  3           reg_rdout_mode            // unsigned ,    RW, default = 0  ,post_vscaler_rd_mode
//Bit  2           reg_postsc_en             // unsigned ,    RW, default = 0  ,reg_postsc_en
//Bit  1           reg_size_mux              // unsigned ,    RW, default = 0  ,hsize sel
//Bit  0           reg_prevsc_outside_en     // unsigned ,    RW, default = 1  ,video1 scale out enable
#define T6D_SAFA_PPS_DEJAGGY_CTRL                      0x5191
//Bit 31           reg_dejaggy_en            // unsigned ,    RW, default = 0
//Bit 30           reg_dejaggy_dps_en_0      // unsigned ,    RW, default = 0
//Bit 29           reg_dejaggy_dps_en_1      // unsigned ,    RW, default = 0
//Bit 28           reg_dejaggy_sameside_mode // unsigned ,    RW, default = 0
//Bit 27:25        reserved
//Bit 24           reg_dejaggy_sameside_prtct // unsigned ,    RW, default = 0
//Bit 23:16        reserved
//Bit 15:12        reg_dejaggy_procluma_alpha_1 // unsigned ,    RW, default = 15
//Bit 11: 8        reg_dejaggy_procluma_alpha_0 // unsigned ,    RW, default = 15
//Bit  7: 4        reg_dejaggy_procchrm_alpha_1 // unsigned ,    RW, default = 15
//Bit  3: 0        reg_dejaggy_procchrm_alpha_0 // unsigned ,    RW, default = 15
#define T6D_SAFA_PPS_CNTL_SCALE_COEF_IDX_LUMA          0x5192
//Bit 31:15        reserved
//Bit 14           reg_index_inc_luma        // unsigned ,    RW, default = 0  ,index increment, if bit9 == 1  then (0: index increase 1, 1: index increase 2) else (index increase 2)
//Bit 13           reg_rd_cbus_coef_en_luma  // unsigned ,    RW, default = 0  ,1: read coef through cbus enable, just for debug purpose in case when we wanna check the coef in ram in correct or not
//Bit 12:10        reserved
//Bit  9: 7        reg_type_index_luma       // unsigned ,    RW, default = 0  ,type of index, 00: vertical coef, 01: vertical chroma coef: 10: horizontal coef, 11: resevered
//Bit  6: 0        reg_coef_index_luma       // unsigned ,    RW, default = 0  ,coef	index
#define T6D_SAFA_PPS_CNTL_SCALE_COEF_LUMA              0x5193
//Bit 31:24        reg_coef0_luma            // signed ,      RW, default = 0  ,	coefficients for vertical filter and horizontal	filter
//Bit 23:16        reg_coef1_luma            // signed ,      RW, default = 0  ,	coefficients for vertical filter and horizontal	filter
//Bit 15: 8        reg_coef2_luma            // signed ,      RW, default = 0  ,	coefficients for vertical filter and horizontal	filter
//Bit  7: 0        reg_coef3_luma            // signed ,      RW, default = 0  ,	coefficients for vertical filter and horizontal	filter
#define T6D_SAFA_PPS_CNTL_SCALE_COEF_IDX_CHRO          0x5194
//Bit 31:15        reserved
//Bit 14           reg_index_inc_chro        // unsigned ,    RW, default = 0  ,index increment, if bit9 == 1  then (0: index increase 1, 1: index increase 2) else (index increase 2)
//Bit 13           reg_rd_cbus_coef_en_chro  // unsigned ,    RW, default = 0  ,1: read coef through cbus enable, just for debug purpose in case when we wanna check the coef in ram in correct or not
//Bit 12:10        reserved
//Bit  9: 7        reg_type_index_chro       // unsigned ,    RW, default = 0  ,type of index, 00: vertical coef, 01: vertical chroma coef: 10: horizontal coef, 11: resevered
//Bit  6: 0        reg_coef_index_chro       // unsigned ,    RW, default = 0  ,coef	index
#define T6D_SAFA_PPS_CNTL_SCALE_COEF_CHRO              0x5195
//Bit 31:24        reg_coef0_chro            // signed ,      RW, default = 0  ,	coefficients for vertical filter and horizontal	filter
//Bit 23:16        reg_coef1_chro            // signed ,      RW, default = 0  ,	coefficients for vertical filter and horizontal	filter
//Bit 15: 8        reg_coef2_chro            // signed ,      RW, default = 0  ,	coefficients for vertical filter and horizontal	filter
//Bit  7: 0        reg_coef3_chro            // signed ,      RW, default = 0  ,	coefficients for vertical filter and horizontal	filter
#define T6D_SAFA_PPS_SR_ALP_INFO                       0x5162
//Bit 31:26        reserved
//Bit 25:24        reg_sr_delta_alp_mode     // unsigned ,    RW, default = 1
//Bit 23:22        reserved
//Bit 21:16        reg_sr_delta_value        // unsigned ,    RW, default = 0
//Bit 15:10        reserved
//Bit  9: 8        reg_sr_gamma_alp_mode     // unsigned ,    RW, default = 1
//Bit  7: 6        reserved
//Bit  5: 0        reg_sr_gamma_value        // unsigned ,    RW, default = 0
#define T6D_SAFA_PPS_PI_INFO                           0x5163
//Bit 31:22        reserved
//Bit 21:20        reg_pi_gamma_mode         // unsigned ,    RW, default = 1
//Bit 19:18        reserved
//Bit 17:12        reg_pi_gamma_value        // unsigned ,    RW, default = 0
//Bit 11:10        reserved
//Bit  9: 8        reg_pi_max_sad_mode       // unsigned ,    RW, default = 1
//Bit  7: 0        reg_pi_max_sad_value      // unsigned ,    RW, default = 0
#define T6D_SAFA_PPS_BOT_VSC_INIT                      0x5197
//Bit 31:21        reserved
//Bit 20:16        reg_vsc_bot_ini_integer   // signed ,      RW, default = -1
//Bit 15: 0        reg_vsc_bot_ini_phase     // unsigned ,    RW, default = 0

#define VPP_SR_EN 0x5204
//Bit 31: 1        reserved
//Bit 0         reg_sr_en       // unsigned ,    RW, default = 0
#define VPP_VSR_DEBUG_MODE                         0x500a
//Bit 31: 8        reserved
//Bit  7: 0        reg_vsr_debug_mode        // unsigned ,    RW, default = 0
#define VPP_PI_DEBUG_DEMO_WND_EN                   0x508a
//Bit 31: 8        reserved
//Bit  7: 5        reserved
//Bit  4           reg_pi_debug_demo_en      // unsigned ,    RW, default = 0
//Bit  3: 1        reserved
//Bit  0           reg_pi_debug_demo_inverse // unsigned ,    RW, default = 0
#define VPP_PI_DEBUG_DEMO_WND_COEF_1               0x508b
//Bit 31:28        reserved
//Bit 27:16        reg_debug_demo_wnd_3      // unsigned ,    RW,
					//default = 1620  control debug window row size
//Bit 15:12        reserved
//Bit 11: 0        reg_debug_demo_wnd_2      // unsigned ,    RW,
					//default = 2880  control debug window col size
#define VPP_PI_DEBUG_DEMO_WND_COEF_0               0x508c
//Bit 31:28        reserved
//Bit 27:16        reg_debug_demo_wnd_1      // unsigned ,    RW,
					//default = 540  control debug window row size
//Bit 15:12        reserved
//Bit 11: 0        reg_debug_demo_wnd_0      // unsigned ,    RW,
					//default = 960  control debug window col size
#define SAFA_PPS_DEBUG_DEMO_EN                     0x510e
//Bit 31: 9        reserved
//Bit  8           reg_debug_demo_dir_interp_en // unsigned ,    RW,
					//default = 0  the demo dir lpf enable
//Bit  7: 5        reserved
//Bit  4           reg_debug_demo_adp_tap_en // unsigned ,    RW, default = 0
//Bit  3: 1        reserved
//Bit  0           reg_debug_demo_inverse    // unsigned ,    RW, default = 0
#define SAFA_PPS_DEBUG_DEMO_WND_COEF_1             0x5114
//Bit 31:28        reserved
//Bit 27:16        reg_debug_demo_wnd_3      // unsigned ,    RW,
					//default = 1620  control debug window row size
//Bit 15:12        reserved
//Bit 11: 0        reg_debug_demo_wnd_2      // unsigned ,    RW,
					//default = 2880  control debug window col size
#define SAFA_PPS_DEBUG_DEMO_WND_COEF_0             0x5115
//Bit 31:28        reserved
//Bit 27:16        reg_debug_demo_wnd_1      // unsigned ,    RW,
					//default = 540  control debug window row size
//Bit 15:12        reserved
//Bit 11: 0        reg_debug_demo_wnd_0      // unsigned ,    RW,
					//default = 960  control debug window col size
#define VPP_SR_DEBUG_DEMO_WND_EN                   0x52ed
//Bit 31: 8        reserved
//Bit  7: 5        reserved
//Bit  4           reg_sr_debug_demo_en      // unsigned ,    RW, default = 0
//Bit  3: 1        reserved
//Bit  0           reg_sr_debug_demo_inverse // unsigned ,    RW, default = 0
#define VPP_SR_DEBUG_DEMO_WND_COEF_1               0x52ee
//Bit 31:28        reserved
//Bit 27:16        reg_debug_demo_wnd_3      // unsigned ,    RW,
					//default = 1620  control debug window row size
//Bit 15:12        reserved
//Bit 11: 0        reg_debug_demo_wnd_2      // unsigned ,    RW,
					//default = 2880  control debug window col size
#define VPP_SR_DEBUG_DEMO_WND_COEF_0               0x52ef
//Bit 31:28        reserved
//Bit 27:16        reg_debug_demo_wnd_1      // unsigned ,    RW,
					//default = 540  control debug window row size
//Bit 15:12        reserved
//Bit 11: 0        reg_debug_demo_wnd_0      // unsigned ,    RW,
					//default = 960  control debug window col size
#define SAFA_PPS_EDGE_AVGSTD_LUT2D_0_0             0x5142
//Bit 31:30        reserved
//Bit 29:24        reg_edge_avgstd_lut2d_0_6 // unsigned ,    RW, default = 0
//Bit 23:22        reserved
//Bit 21:16        reg_edge_avgstd_lut2d_0_5 // unsigned ,    RW, default = 0
//Bit 15:14        reserved
//Bit 13: 8        reg_edge_avgstd_lut2d_0_4 // unsigned ,    RW, default = 0
//Bit  7: 6        reserved
//Bit  5: 0        reg_edge_avgstd_lut2d_0_3 // unsigned ,    RW, default = 0
#define SAFA_PPS_EDGE_AVGSTD_LUT2D_F_0_0           0x5143
//Bit 31:22        reserved
//Bit 21:16        reg_edge_avgstd_lut2d_0_2 // unsigned ,    RW, default = 0
//Bit 15:14        reserved
//Bit 13: 8        reg_edge_avgstd_lut2d_0_1 // unsigned ,    RW, default = 0
//Bit  7: 6        reserved
//Bit  5: 0        reg_edge_avgstd_lut2d_0_0 // unsigned ,    RW, default = 0
#define SAFA_PPS_EDGE_AVGSTD_LUT2D_1_0             0x5144
//Bit 31:30        reserved
//Bit 29:24        reg_edge_avgstd_lut2d_1_6 // unsigned ,    RW, default = 0
//Bit 23:22        reserved
//Bit 21:16        reg_edge_avgstd_lut2d_1_5 // unsigned ,    RW, default = 0
//Bit 15:14        reserved
//Bit 13: 8        reg_edge_avgstd_lut2d_1_4 // unsigned ,    RW, default = 0
//Bit  7: 6        reserved
//Bit  5: 0        reg_edge_avgstd_lut2d_1_3 // unsigned ,    RW, default = 8
#define SAFA_PPS_EDGE_AVGSTD_LUT2D_F_1_0           0x5145
//Bit 31:22        reserved
//Bit 21:16        reg_edge_avgstd_lut2d_1_2 // unsigned ,    RW, default = 0
//Bit 15:14        reserved
//Bit 13: 8        reg_edge_avgstd_lut2d_1_1 // unsigned ,    RW, default = 0
//Bit  7: 6        reserved
//Bit  5: 0        reg_edge_avgstd_lut2d_1_0 // unsigned ,    RW, default = 0
#define SAFA_PPS_EDGE_AVGSTD_LUT2D_2_0             0x5146
//Bit 31:30        reserved
//Bit 29:24        reg_edge_avgstd_lut2d_2_6 // unsigned ,    RW, default = 0
//Bit 23:22        reserved
//Bit 21:16        reg_edge_avgstd_lut2d_2_5 // unsigned ,    RW, default = 0
//Bit 15:14        reserved
//Bit 13: 8        reg_edge_avgstd_lut2d_2_4 // unsigned ,    RW, default = 8
//Bit  7: 6        reserved
//Bit  5: 0        reg_edge_avgstd_lut2d_2_3 // unsigned ,    RW, default = 16
#define SAFA_PPS_EDGE_AVGSTD_LUT2D_F_2_0           0x5147
//Bit 21:16        reg_edge_avgstd_lut2d_2_2 // unsigned ,    RW, default = 0
//Bit 15:14        reserved
//Bit 13: 8        reg_edge_avgstd_lut2d_2_1 // unsigned ,    RW, default = 0
//Bit  7: 6        reserved
//Bit  5: 0        reg_edge_avgstd_lut2d_2_0 // unsigned ,    RW, default = 8
#define SAFA_PPS_EDGE_AVGSTD_LUT2D_3_0             0x5148
//Bit 31:30        reserved
//Bit 29:24        reg_edge_avgstd_lut2d_3_6 // unsigned ,    RW, default = 0
//Bit 23:22        reserved
//Bit 21:16        reg_edge_avgstd_lut2d_3_5 // unsigned ,    RW, default = 16
//Bit 15:14        reserved
//Bit 13: 8        reg_edge_avgstd_lut2d_3_4 // unsigned ,    RW, default = 32
//Bit  7: 6        reserved
//Bit  5: 0        reg_edge_avgstd_lut2d_3_3 // unsigned ,    RW, default = 48
#define SAFA_PPS_EDGE_AVGSTD_LUT2D_F_3_0           0x5149
//Bit 31:22        reserved
//Bit 21:16        reg_edge_avgstd_lut2d_3_2 // unsigned ,    RW, default = 0
//Bit 15:14        reserved
//Bit 13: 8        reg_edge_avgstd_lut2d_3_1 // unsigned ,    RW, default = 16
//Bit  7: 6        reserved
//Bit  5: 0        reg_edge_avgstd_lut2d_3_0 // unsigned ,    RW, default = 32
#define SAFA_PPS_EDGE_AVGSTD_LUT2D_4_0             0x514a
//Bit 31:30        reserved
//Bit 29:24        reg_edge_avgstd_lut2d_4_6 // unsigned ,    RW, default = 0
//Bit 23:22        reserved
//Bit 21:16        reg_edge_avgstd_lut2d_4_5 // unsigned ,    RW, default = 32
//Bit 15:14        reserved
//Bit 13: 8        reg_edge_avgstd_lut2d_4_4 // unsigned ,    RW, default = 48
//Bit  7: 6        reserved
//Bit  5: 0        reg_edge_avgstd_lut2d_4_3 // unsigned ,    RW, default = 56
#define SAFA_PPS_EDGE_AVGSTD_LUT2D_F_4_0           0x514b
//Bit 31:22        reserved
//Bit 21:16        reg_edge_avgstd_lut2d_4_2 // unsigned ,    RW, default = 0
//Bit 15:14        reserved
//Bit 13: 8        reg_edge_avgstd_lut2d_4_1 // unsigned ,    RW, default = 32
//Bit  7: 6        reserved
//Bit  5: 0        reg_edge_avgstd_lut2d_4_0 // unsigned ,    RW, default = 48

#define VPP_PI_MAXSAD_GAMMA_LUT2D_0_0_0            0x5061
//Bit 31:30        reserved
//Bit 29:24        reg_maxsad_gamma_lut2d_0_8 // unsigned ,    RW, default = 48
//Bit 23:22        reserved
//Bit 21:16        reg_maxsad_gamma_lut2d_0_7 // unsigned ,    RW, default = 48
//Bit 15:14        reserved
//Bit 13: 8        reg_maxsad_gamma_lut2d_0_6 // unsigned ,    RW, default = 48
//Bit  7: 6        reserved
//Bit  5: 0        reg_maxsad_gamma_lut2d_0_5 // unsigned ,    RW, default = 56
#define VPP_PI_MAXSAD_GAMMA_LUT2D_1_0_0            0x5062
//Bit 31:30        reserved
//Bit 29:24        reg_maxsad_gamma_lut2d_0_4 // unsigned ,    RW, default = 48
//Bit 23:22        reserved
//Bit 21:16        reg_maxsad_gamma_lut2d_0_3 // unsigned ,    RW, default = 48
//Bit 15:14        reserved
//Bit 13: 8        reg_maxsad_gamma_lut2d_0_2 // unsigned ,    RW, default = 48
//Bit  7: 6        reserved
//Bit  5: 0        reg_maxsad_gamma_lut2d_0_1 // unsigned ,    RW, default = 56
#define VPP_PI_MAXSAD_GAMMA_LUT2D_2_0_0            0x5063
//Bit 31: 6        reserved
//Bit  5: 0        reg_maxsad_gamma_lut2d_0_0 // unsigned ,    RW, default = 48
#define VPP_PI_MAXSAD_GAMMA_LUT2D_0_1_0            0x5064
//Bit 29:24        reg_maxsad_gamma_lut2d_1_8 // unsigned ,    RW, default = 48
//Bit 23:22        reserved
//Bit 21:16        reg_maxsad_gamma_lut2d_1_7 // unsigned ,    RW, default = 48
//Bit 15:14        reserved
//Bit 13: 8        reg_maxsad_gamma_lut2d_1_6 // unsigned ,    RW, default = 48
//Bit  7: 6        reserved
//Bit  5: 0        reg_maxsad_gamma_lut2d_1_5 // unsigned ,    RW, default = 56
#define VPP_PI_MAXSAD_GAMMA_LUT2D_1_1_0            0x5065
//Bit 31:30        reserved
//Bit 29:24        reg_maxsad_gamma_lut2d_1_4 // unsigned ,    RW, default = 48
//Bit 23:22        reserved
//Bit 21:16        reg_maxsad_gamma_lut2d_1_3 // unsigned ,    RW, default = 48
//Bit 15:14        reserved
//Bit 13: 8        reg_maxsad_gamma_lut2d_1_2 // unsigned ,    RW, default = 48
//Bit  7: 6        reserved
//Bit  5: 0        reg_maxsad_gamma_lut2d_1_1 // unsigned ,    RW, default = 56
#define VPP_PI_MAXSAD_GAMMA_LUT2D_2_1_0            0x5066
//Bit 31: 6        reserved
//Bit  5: 0        reg_maxsad_gamma_lut2d_1_0 // unsigned ,    RW, default = 48
#define VPP_PI_MAXSAD_GAMMA_LUT2D_0_2_0            0x5067
//Bit 31:30        reserved
//Bit 29:24        reg_maxsad_gamma_lut2d_2_8 // unsigned ,    RW, default = 56
//Bit 23:22        reserved
//Bit 21:16        reg_maxsad_gamma_lut2d_2_7 // unsigned ,    RW, default = 48
//Bit 15:14        reserved
//Bit 13: 8        reg_maxsad_gamma_lut2d_2_6 // unsigned ,    RW, default = 48
//Bit  7: 6        reserved
//Bit  5: 0        reg_maxsad_gamma_lut2d_2_5 // unsigned ,    RW, default = 56
#define VPP_PI_MAXSAD_GAMMA_LUT2D_1_2_0            0x5068
//Bit 31:30        reserved
//Bit 29:24        reg_maxsad_gamma_lut2d_2_4 // unsigned ,    RW, default = 56
//Bit 23:22        reserved
//Bit 21:16        reg_maxsad_gamma_lut2d_2_3 // unsigned ,    RW, default = 48
//Bit 15:14        reserved
//Bit 13: 8        reg_maxsad_gamma_lut2d_2_2 // unsigned ,    RW, default = 48
//Bit  7: 6        reserved
//Bit  5: 0        reg_maxsad_gamma_lut2d_2_1 // unsigned ,    RW, default = 56
#define VPP_PI_MAXSAD_GAMMA_LUT2D_2_2_0            0x5069
//Bit 31: 6        reserved
//Bit  5: 0        reg_maxsad_gamma_lut2d_2_0 // unsigned ,    RW, default = 56
#define VPP_PI_MAXSAD_GAMMA_LUT2D_0_3_0            0x506a
//Bit 31:30        reserved
//Bit 29:24        reg_maxsad_gamma_lut2d_3_8 // unsigned ,    RW, default = 56
//Bit 23:22        reserved
//Bit 21:16        reg_maxsad_gamma_lut2d_3_7 // unsigned ,    RW, default = 56
//Bit 15:14        reserved
//Bit 13: 8        reg_maxsad_gamma_lut2d_3_6 // unsigned ,    RW, default = 48
//Bit  7: 6        reserved
//Bit  5: 0        reg_maxsad_gamma_lut2d_3_5 // unsigned ,    RW, default = 56
#define VPP_PI_MAXSAD_GAMMA_LUT2D_1_3_0            0x506b
//Bit 31:30        reserved
//Bit 29:24        reg_maxsad_gamma_lut2d_3_4 // unsigned ,    RW, default = 56
//Bit 23:22        reserved
//Bit 21:16        reg_maxsad_gamma_lut2d_3_3 // unsigned ,    RW, default = 56
//Bit 15:14        reserved
//Bit 13: 8        reg_maxsad_gamma_lut2d_3_2 // unsigned ,    RW, default = 48
//Bit  7: 6        reserved
//Bit  5: 0        reg_maxsad_gamma_lut2d_3_1 // unsigned ,    RW, default = 56
#define VPP_PI_MAXSAD_GAMMA_LUT2D_2_3_0            0x506c
//Bit 31: 6        reserved
//Bit  5: 0        reg_maxsad_gamma_lut2d_3_0 // unsigned ,    RW, default = 56
#define VPP_PI_MAXSAD_GAMMA_LUT2D_0_4_0            0x506d
//Bit 31:30        reserved
//Bit 29:24        reg_maxsad_gamma_lut2d_4_8 // unsigned ,    RW, default = 56
//Bit 23:22        reserved
//Bit 21:16        reg_maxsad_gamma_lut2d_4_7 // unsigned ,    RW, default = 56
//Bit 15:14        reserved
//Bit 13: 8        reg_maxsad_gamma_lut2d_4_6 // unsigned ,    RW, default = 56
//Bit  7: 6        reserved
//Bit  5: 0        reg_maxsad_gamma_lut2d_4_5 // unsigned ,    RW, default = 48
#define VPP_PI_MAXSAD_GAMMA_LUT2D_1_4_0            0x506e
//Bit 31:30        reserved
//Bit 29:24        reg_maxsad_gamma_lut2d_4_4 // unsigned ,    RW, default = 56
//Bit 23:22        reserved
//Bit 21:16        reg_maxsad_gamma_lut2d_4_3 // unsigned ,    RW, default = 56
//Bit 15:14        reserved
//Bit 13: 8        reg_maxsad_gamma_lut2d_4_2 // unsigned ,    RW, default = 56
//Bit  7: 6        reserved
//Bit  5: 0        reg_maxsad_gamma_lut2d_4_1 // unsigned ,    RW, default = 48
#define VPP_PI_MAXSAD_GAMMA_LUT2D_2_4_0            0x506f
//Bit 31: 6        reserved
//Bit  5: 0        reg_maxsad_gamma_lut2d_4_0 // unsigned ,    RW, default = 56
#define VPP_PI_MAXSAD_GAMMA_LUT2D_0_5_0            0x5070
//Bit 31:30        reserved
//Bit 29:24        reg_maxsad_gamma_lut2d_5_8 // unsigned ,    RW, default = 56
//Bit 23:22        reserved
//Bit 21:16        reg_maxsad_gamma_lut2d_5_7 // unsigned ,    RW, default = 56
//Bit 15:14        reserved
//Bit 13: 8        reg_maxsad_gamma_lut2d_5_6 // unsigned ,    RW, default = 56
//Bit  7: 6        reserved
//Bit  5: 0        reg_maxsad_gamma_lut2d_5_5 // unsigned ,    RW, default = 48
#define VPP_PI_MAXSAD_GAMMA_LUT2D_1_5_0            0x5071
//Bit 31:30        reserved
//Bit 29:24        reg_maxsad_gamma_lut2d_5_4 // unsigned ,    RW, default = 56
//Bit 23:22        reserved
//Bit 21:16        reg_maxsad_gamma_lut2d_5_3 // unsigned ,    RW, default = 56
//Bit 15:14        reserved
//Bit 13: 8        reg_maxsad_gamma_lut2d_5_2 // unsigned ,    RW, default = 56
//Bit  7: 6        reserved
//Bit  5: 0        reg_maxsad_gamma_lut2d_5_1 // unsigned ,    RW, default = 48
#define VPP_PI_MAXSAD_GAMMA_LUT2D_2_5_0            0x5072
//Bit 31: 6        reserved
//Bit  5: 0        reg_maxsad_gamma_lut2d_5_0 // unsigned ,    RW, default = 56
#define VPP_PI_MAXSAD_GAMMA_LUT2D_0_6_0            0x5073
//Bit 31:30        reserved
//Bit 29:24        reg_maxsad_gamma_lut2d_6_8 // unsigned ,    RW, default = 56
//Bit 23:22        reserved
//Bit 21:16        reg_maxsad_gamma_lut2d_6_7 // unsigned ,    RW, default = 56
//Bit 15:14        reserved
//Bit 13: 8        reg_maxsad_gamma_lut2d_6_6 // unsigned ,    RW, default = 56
//Bit  7: 6        reserved
//Bit  5: 0        reg_maxsad_gamma_lut2d_6_5 // unsigned ,    RW, default = 56
#define VPP_PI_MAXSAD_GAMMA_LUT2D_1_6_0            0x5074
//Bit 31:30        reserved
//Bit 29:24        reg_maxsad_gamma_lut2d_6_4 // unsigned ,    RW, default = 56
//Bit 23:22        reserved
//Bit 21:16        reg_maxsad_gamma_lut2d_6_3 // unsigned ,    RW, default = 56
//Bit 15:14        reserved
//Bit 13: 8        reg_maxsad_gamma_lut2d_6_2 // unsigned ,    RW, default = 56
//Bit  7: 6        reserved
//Bit  5: 0        reg_maxsad_gamma_lut2d_6_1 // unsigned ,    RW, default = 56
#define VPP_PI_MAXSAD_GAMMA_LUT2D_2_6_0            0x5075
//Bit 31: 6        reserved
//Bit  5: 0        reg_maxsad_gamma_lut2d_6_0 // unsigned ,    RW, default = 56
#define VPP_PI_MAXSAD_GAMMA_LUT2D_0_7_0            0x5076
//Bit 31:30        reserved
//Bit 29:24        reg_maxsad_gamma_lut2d_7_8 // unsigned ,    RW, default = 63
//Bit 23:22        reserved
//Bit 21:16        reg_maxsad_gamma_lut2d_7_7 // unsigned ,    RW, default = 56
//Bit 15:14        reserved
//Bit 13: 8        reg_maxsad_gamma_lut2d_7_6 // unsigned ,    RW, default = 56
//Bit  7: 6        reserved
//Bit  5: 0        reg_maxsad_gamma_lut2d_7_5 // unsigned ,    RW, default = 56
#define VPP_PI_MAXSAD_GAMMA_LUT2D_1_7_0            0x5077
//Bit 31:30        reserved
//Bit 29:24        reg_maxsad_gamma_lut2d_7_4 // unsigned ,    RW, default = 63
//Bit 23:22        reserved
//Bit 21:16        reg_maxsad_gamma_lut2d_7_3 // unsigned ,    RW, default = 56
//Bit 15:14        reserved
//Bit 13: 8        reg_maxsad_gamma_lut2d_7_2 // unsigned ,    RW, default = 56
//Bit  7: 6        reserved
//Bit  5: 0        reg_maxsad_gamma_lut2d_7_1 // unsigned ,    RW, default = 56
#define VPP_PI_MAXSAD_GAMMA_LUT2D_2_7_0            0x5078
//Bit 31: 6        reserved
//Bit  5: 0        reg_maxsad_gamma_lut2d_7_0 // unsigned ,    RW, default = 63
#define VPP_PI_MAXSAD_GAMMA_LUT2D_0_8_0            0x5079
//Bit 31:30        reserved
//Bit 29:24        reg_maxsad_gamma_lut2d_8_8 // unsigned ,    RW, default = 63
//Bit 23:22        reserved
//Bit 21:16        reg_maxsad_gamma_lut2d_8_7 // unsigned ,    RW, default = 63
//Bit 15:14        reserved
//Bit 13: 8        reg_maxsad_gamma_lut2d_8_6 // unsigned ,    RW, default = 63
//Bit  7: 6        reserved
//Bit  5: 0        reg_maxsad_gamma_lut2d_8_5 // unsigned ,    RW, default = 63
#define VPP_PI_MAXSAD_GAMMA_LUT2D_1_8_0            0x507a
//Bit 31:30        reserved
//Bit 29:24        reg_maxsad_gamma_lut2d_8_4 // unsigned ,    RW, default = 63
//Bit 23:22        reserved
//Bit 21:16        reg_maxsad_gamma_lut2d_8_3 // unsigned ,    RW, default = 63
//Bit 15:14        reserved
//Bit 13: 8        reg_maxsad_gamma_lut2d_8_2 // unsigned ,    RW, default = 63
//Bit  7: 6        reserved
//Bit  5: 0        reg_maxsad_gamma_lut2d_8_1 // unsigned ,    RW, default = 63
#define VPP_PI_MAXSAD_GAMMA_LUT2D_2_8_0            0x507b
//Bit 31: 6        reserved
//Bit  5: 0        reg_maxsad_gamma_lut2d_8_0 // unsigned ,    RW, default = 63

#define VPP_PI_HF_SCL_COEF_0                       0x504e
//Bit 31:24        reg_pi_pps_coef_tap2_3_0  // unsigned ,    RW, default = 128
//Bit 23:16        reg_pi_pps_coef_tap2_2_0  // unsigned ,    RW, default = 128
//Bit 15: 8        reg_pi_pps_coef_tap2_1_0  // unsigned ,    RW, default = 128
//Bit  7: 0        reg_pi_pps_coef_tap2_0_0  // unsigned ,    RW, default = 128
#define VPP_PI_HF_SCL_COEF_1                       0x504f
//Bit 31:24        reg_pi_pps_coef_tap2_7_0  // unsigned ,    RW, default = 0
//Bit 23:16        reg_pi_pps_coef_tap2_6_0  // unsigned ,    RW, default = 0
//Bit 15: 8        reg_pi_pps_coef_tap2_5_0  // unsigned ,    RW, default = 0
//Bit  7: 0        reg_pi_pps_coef_tap2_4_0  // unsigned ,    RW, default = 0
#define VPP_PI_HF_SCL_COEF_2                       0x5050
//Bit 31:24        reg_pi_pps_coef_tap2_11_0 // unsigned ,    RW, default = 127
//Bit 23:16        reg_pi_pps_coef_tap2_10_0 // unsigned ,    RW, default = 127
//Bit 15: 8        reg_pi_pps_coef_tap2_9_0  // unsigned ,    RW, default = 127
//Bit  7: 0        reg_pi_pps_coef_tap2_8_0  // unsigned ,    RW, default = 127
#define VPP_PI_HF_SCL_COEF_3                       0x5051
//Bit 31:24        reg_pi_pps_coef_tap2_15_0 // unsigned ,    RW, default = 1
//Bit 23:16        reg_pi_pps_coef_tap2_14_0 // unsigned ,    RW, default = 1
//Bit 15: 8        reg_pi_pps_coef_tap2_13_0 // unsigned ,    RW, default = 1
//Bit  7: 0        reg_pi_pps_coef_tap2_12_0 // unsigned ,    RW, default = 1
#define VPP_PI_HF_SCL_COEF_4                       0x5052
//Bit 31:24        reg_pi_pps_coef_tap2_19_0 // unsigned ,    RW, default = 126
//Bit 23:16        reg_pi_pps_coef_tap2_18_0 // unsigned ,    RW, default = 126
//Bit 15: 8        reg_pi_pps_coef_tap2_17_0 // unsigned ,    RW, default = 126
//Bit  7: 0        reg_pi_pps_coef_tap2_16_0 // unsigned ,    RW, default = 126
#define VPP_PI_HF_SCL_COEF_5                       0x5053
//Bit 31:24        reg_pi_pps_coef_tap2_23_0 // unsigned ,    RW, default = 2
//Bit 23:16        reg_pi_pps_coef_tap2_22_0 // unsigned ,    RW, default = 2
//Bit 15: 8        reg_pi_pps_coef_tap2_21_0 // unsigned ,    RW, default = 2
//Bit  7: 0        reg_pi_pps_coef_tap2_20_0 // unsigned ,    RW, default = 2
#define VPP_PI_HF_SCL_COEF_6                       0x5054
//Bit 31:24        reg_pi_pps_coef_tap2_27_0 // unsigned ,    RW, default = 124
//Bit 23:16        reg_pi_pps_coef_tap2_26_0 // unsigned ,    RW, default = 124
//Bit 15: 8        reg_pi_pps_coef_tap2_25_0 // unsigned ,    RW, default = 124
//Bit  7: 0        reg_pi_pps_coef_tap2_24_0 // unsigned ,    RW, default = 124
#define VPP_PI_HF_SCL_COEF_7                       0x5055
//Bit 31:24        reg_pi_pps_coef_tap2_31_0 // unsigned ,    RW, default = 4
//Bit 23:16        reg_pi_pps_coef_tap2_30_0 // unsigned ,    RW, default = 4
//Bit 15: 8        reg_pi_pps_coef_tap2_29_0 // unsigned ,    RW, default = 4
//Bit  7: 0        reg_pi_pps_coef_tap2_28_0 // unsigned ,    RW, default = 4
#define VPP_PI_HF_SCL_COEF_F                       0x5056
//Bit 31: 8        reserved
//Bit  7: 0        reg_pi_pps_coef_tap2_32_0 // unsigned ,    RW, default = 99
#define VPP_P2I_H_V_SIZE                           0x1da7
//Bit 31:29  reserved
//Bit 28:16  p2i_line_length  // unsigned  , default = 780  ve_line_length
//Bit 15:13  reserved
//Bit 12:0   p2i_pic_height   // unsigned  , default = 438  ve_pic_height
#define T6D_SAFA_PPS_SAD_FLAT_THD                      0x5140
//Bit 31:24        reserved
//Bit 23:16        reg_sad_flat_th2          // unsigned ,    RW, default = 10
//Bit 15: 8        reg_sad_flat_th1          // unsigned ,    RW, default = 10
//Bit  7: 0        reg_sad_flat_th0          // unsigned ,    RW, default = 3
#define T6D_SAFA_PPS_DIR_MIN_IDX_VALID                 0x5126
//Bit 31:13        reserved
//Bit 12           reg_delta_chk_min_idx_valid // unsigned ,    RW, default = 0
//Bit 11: 9        reserved
//Bit  8           reg_dir_hist_chk_min_idx_valid // unsigned ,    RW, default = 1
//Bit  7: 5        reserved
//Bit  4           reg_dir_dif_chk_min_idx_valid // unsigned ,    RW, default = 1
//Bit  3: 1        reserved
//Bit  0           reg_beta_chk_min_idx_valid // unsigned ,    RW, default = 0

#define S6_SAFA_PPS_CNTL_SCALE_COEF_IDX_LUMA          0x5190
#define S6_SAFA_PPS_CNTL_SCALE_COEF_LUMA              0x5191
#define S6_SAFA_PPS_CNTL_SCALE_COEF_IDX_CHRO          0x5192
#define S6_SAFA_PPS_CNTL_SCALE_COEF_CHRO              0x5193
#define S6_SAFA_PPS_HW_CTRL                           0x5194
#endif
