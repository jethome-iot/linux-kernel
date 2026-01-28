/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _MESON_OSD_GFCD_H_
#define _MESON_OSD_GFCD_H_

#define VPP_OSD_HDR_DIV_ALPHA                      0x1d7f

#define GFCD_FRM_BGN                               0x5950
#define GFCD_FRM_END                               0x5951
#define GFCD_CLK_CTRL                              0x5952
#define GFCD_SW_RESET                              0x5953
#define GFCD_DBG_CTRL                              0x5954
#define GFCD_TOP_CTRL                              0x5955
#define GFCD_AFRC_CTRL                             0x5956
#define GFCD_AFRC_PARAM                            0x5957
#define GFCD_AFBC_CTRL                             0x5958
#define GFCD_FRM_SIZE                              0x5959
#define GFCD_RO_AM_G0                              0x595a
#define GFCD_RO_AM_G1                              0x595b
#define GFCD_RO_SOLID_G0                           0x595c
#define GFCD_RO_SOLID_G1                           0x595d
#define GFCD_RO_SOLID_G2                           0x595e
#define GFCD_RO_COMB_G0                            0x595f
#define GFCD_RO_CUT_G0                             0x5960
#define GFCD_RO_CUT_G1                             0x5961

#define GFCD_GLB_CLK_CTRL                          0x59f0
#define GFCD_GLB_SW_RST                            0x59f1
#define GFCD_MIF_CLK_CTRL                          0x5900
#define GFCD_MIF_SW_RST                            0x5901
#define GFCD_MIF_MODE_SEL                          0x5902
#define GFCD_MIF_HEAD_CTRL0                        0x5903
#define GFCD_MIF_HEAD_BADDR                        0x5904
#define GFCD_MIF_HEAD_URGENT                       0x5905
#define GFCD_MIF_HEAD_DIMENSION                    0x5906
#define GFCD_MIF_BODY_CTRL                         0x5907
#define GFCD_MIF_BODY_BADDR                        0x5908
#define GFCD_MIF_BODY_URGENT                       0x5909
#define GFCD_MIF_BODY_DIMENSION                    0x590a
#define GFCD_RO_HEAD_G0                            0x590b
#define GFCD_RO_HEAD_G1                            0x590c
#define GFCD_RO_HEAD_G2                            0x590d
#define GFCD_RO_HEAD_G3                            0x590e
#define GFCD_RO_BODY_G0                            0x590f
#define GFCD_RO_BODY_G1                            0x5910
#define GFCD_RO_BODY_G2                            0x5911
#define GFCD_RO_MIF_G0                             0x5912
#define GFCD_RO_MIF_G1                             0x5913

//offset 0x70
//#define GFCD_MIF_MODE_SEL_S1                       0x591e
//#define GFCD_MIF_HEAD_BADDR_S1                     0x5920
//#define GFCD_MIF_BODY_BADDR_S1                     0x5924
//#define GFCD_FRM_BGN_S1                            0x596c
//#define GFCD_FRM_END_S1                            0x596d
//#define GFCD_TOP_CTRL_S1                           0x5971
//#define GFCD_AFRC_CTRL_S1                          0x5973
//#define GFCD_AFBC_CTRL_S1                          0x5974
//#define GFCD_FRM_SIZE_S1                           0x5975
//#define GFCD_MIF_HEAD_CTRL0_S1                     0x591f
//#define GFCD_MIF_BODY_CTRL_S1                      0x5923

#define GFCD_MIF_MODE_SEL_S1                       0x5972
#define GFCD_MIF_HEAD_BADDR_S1                     0x5974
#define GFCD_MIF_BODY_BADDR_S1                     0x5978
#define GFCD_FRM_BGN_S1                            0x59c0
#define GFCD_FRM_END_S1                            0x59c1
#define GFCD_TOP_CTRL_S1                           0x59c5
#define GFCD_AFRC_CTRL_S1                          0x59c6
#define GFCD_AFBC_CTRL_S1                          0x59c8
#define GFCD_FRM_SIZE_S1                           0x59c9
#define GFCD_MIF_HEAD_CTRL0_S1                     0x5973
#define GFCD_MIF_BODY_CTRL_S1                      0x5977

struct gfcd_reg_s {
	u32 gfcd_mif_mode_sel;
	u32 gfcd_mif_head_baddr;
	u32 gfcd_mif_body_baddr;
	u32 gfcd_frm_bgn;
	u32 gfcd_frm_end;
	u32 gfcd_top_ctrl;
	u32 gfcd_afrc_ctrl;
	u32 gfcd_afbc_ctrl;
	u32 gfcd_frm_size;
	u32 gfcd_mif_head_ctrl0;
	u32 gfcd_mif_body_ctrl;
};

extern const struct meson_drm_format_info *
	__meson_drm_gfcd_format_info(u32 format);

#endif
