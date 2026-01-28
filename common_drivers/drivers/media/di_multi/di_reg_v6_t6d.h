/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DI_REG_V6_T6D_H__
#define __DI_REG_V6_T6D_H__

/* for t6d */
#define DI_T6D_SUB_RDARB_MODE                          0x3e00
#define DI_T6D_SUB_RDARB_REQEN_SLV                     0x3e01
#define DI_T6D_SUB_RDARB_WEIGH0_SLV                    0x3e02
#define DI_T6D_SUB_RDARB_WEIGH1_SLV                    0x3e03
#define DI_T6D_SUB_RDARB_UGT                           0x3e04
#define DI_T6D_SUB_RDARB_LIMT0                         0x3e05
#define DI_T6D_SUB_WRARB_MODE                          0x3e06
#define DI_T6D_SUB_WRARB_REQEN_SLV                     0x3e07	//DI_SUB_WRARB_REQEN_SLV
#define DI_T6D_SUB_WRARB_WEIGH0_SLV                    0x3e08
#define DI_T6D_SUB_WRARB_WEIGH1_SLV                    0x3e09
#define DI_T6D_SUB_WRARB_UGT                           0x3e0a
#define DI_T6D_SUB_RDWR_ARB_STATUS                     0x3e0b
#define DI_T6D_SUB_ARB_DBG_CTRL                        0x3e0c
#define DI_T6D_SUB_ARB_DBG_STAT                        0x3e0d

#define T6D_CONTRD_CTRL1                               0x3e0e
#define T6D_CONTRD_CTRL2                               0x3e0f
#define T6D_CONTRD_SCOPE_X                             0x3e10
#define T6D_CONTRD_SCOPE_Y                             0x3e11
#define T6D_CONTRD_RO_STAT                             0x3e12
#define T6D_CONT2RD_CTRL1                              0x3e13
#define T6D_CONT2RD_CTRL2                              0x3e14
#define T6D_CONT2RD_SCOPE_X                            0x3e15
#define T6D_CONT2RD_SCOPE_Y                            0x3e16
#define T6D_CONT2RD_RO_STAT                            0x3e17
#define T6D_MTNRD_CTRL1                                0x3e18
#define T6D_MTNRD_CTRL2                                0x3e19
#define T6D_MTNRD_SCOPE_X                              0x3e1a
#define T6D_MTNRD_SCOPE_Y                              0x3e1b
#define T6D_MTNRD_RO_STAT                              0x3e1c
#define T6D_MCVECRD_CTRL1                              0x3e1d
#define T6D_MCVECRD_CTRL2                              0x3e1e
#define T6D_MCVECRD_SCOPE_X                            0x3e1f
#define T6D_MCVECRD_SCOPE_Y                            0x3e20
#define T6D_MCVECRD_RO_STAT                            0x3e21
#define T6D_MCINFRD_CTRL1                              0x3e22
#define T6D_MCINFRD_CTRL2                              0x3e23
#define T6D_MCINFRD_SCOPE_X                            0x3e24
#define T6D_MCINFRD_SCOPE_Y                            0x3e25
#define T6D_MCINFRD_RO_STAT                            0x3e26	//note
#define T6D_CONTRD_BADDR                               0x3e4a	//note
#define T6D_CONT2RD_BADDR                              0x3e4b
#define T6D_MTNRD_BADDR                                0x3e4c
#define T6D_MCVECRD_BADDR                              0x3e4d
#define T6D_MCINFRD_BADDR                              0x3e4e

#define T6D_CONTWMIF_CTRL1                             0x3e27
#define T6D_CONTWMIF_CTRL2                             0x3e28
#define T6D_CONTWMIF_CTRL3                             0x3e29
#define T6D_CONTWMIF_CTRL4                             0x3e2a
#define T6D_CONTWMIF_SCOPE_X                           0x3e2b
#define T6D_CONTWMIF_SCOPE_Y                           0x3e2c
#define T6D_CONTWMIF_RO_STAT                           0x3e2d
#define T6D_MTNWMIF_CTRL1                              0x3e2e
#define T6D_MTNWMIF_CTRL2                              0x3e2f
#define T6D_MTNWMIF_CTRL3                              0x3e30
#define T6D_MTNWMIF_CTRL4                              0x3e31
#define T6D_MTNWMIF_SCOPE_X                            0x3e32
#define T6D_MTNWMIF_SCOPE_Y                            0x3e33
#define T6D_MTNWMIF_RO_STAT                            0x3e34
#define T6D_MCVECWMIF_CTRL1                            0x3e35
#define T6D_MCVECWMIF_CTRL2                            0x3e36
#define T6D_MCVECWMIF_CTRL3                            0x3e37
#define T6D_MCVECWMIF_CTRL4                            0x3e38
#define T6D_MCVECWMIF_SCOPE_X                          0x3e39
#define T6D_MCVECWMIF_SCOPE_Y                          0x3e3a
#define T6D_MCVECWMIF_RO_STAT                          0x3e3b
#define T6D_MCINFWMIF_CTRL1                            0x3e3c
#define T6D_MCINFWMIF_CTRL2                            0x3e3d
#define T6D_MCINFWMIF_CTRL3                            0x3e3e
#define T6D_MCINFWMIF_CTRL4                            0x3e3f
#define T6D_MCINFWMIF_SCOPE_X                          0x3e40
#define T6D_MCINFWMIF_SCOPE_Y                          0x3e41
#define T6D_MCINFWMIF_RO_STAT                          0x3e42
#define T6D_NRDSWMIF_CTRL1                             0x3e43
#define T6D_NRDSWMIF_CTRL2                             0x3e44
#define T6D_NRDSWMIF_CTRL3                             0x3e45
#define T6D_NRDSWMIF_CTRL4                             0x3e46
#define T6D_NRDSWMIF_SCOPE_X                           0x3e47
#define T6D_NRDSWMIF_SCOPE_Y                           0x3e48
#define T6D_NRDSWMIF_RO_STAT                           0x3e49

#define NR_WMIF_CTRL0                              0x3e4f
#define NR_WMIF_CTRL1                              0x3e50
#define NR_WMIF_LUMA_CTRL0                         0x3e51
#define NR_WMIF_LUMA_BADDR                         0x3e52
#define NR_WMIF_LUMA_X                             0x3e53
#define NR_WMIF_LUMA_Y                             0x3e54
#define NR_WMIF_LUMA_CTRL1                         0x3e55
#define NR_WMIF_CHRM_CTRL0                         0x3e56
#define NR_WMIF_CHRM_BADDR                         0x3e57
#define NR_WMIF_CHRM_X                             0x3e58
#define NR_WMIF_CHRM_Y                             0x3e59
#define NR_WMIF_CHRM_CTRL1                         0x3e5a
#define NR_WRMIF_RGBA_CTRL                         0x3e5b

#define DI_WMIF_CTRL0                              0x3e5c
#define DI_WMIF_CTRL1                              0x3e5d
#define DI_WMIF_LUMA_CTRL0                         0x3e5e
#define DI_WMIF_LUMA_BADDR                         0x3e5f
#define DI_WMIF_LUMA_X                             0x3e60
#define DI_WMIF_LUMA_Y                             0x3e61
#define DI_WMIF_LUMA_CTRL1                         0x3e62
#define DI_WMIF_CHRM_CTRL0                         0x3e63
#define DI_WMIF_CHRM_BADDR                         0x3e64
#define DI_WMIF_CHRM_X                             0x3e65
#define DI_WMIF_CHRM_Y                             0x3e66
#define DI_WMIF_CHRM_CTRL1                         0x3e67
#define DI_WRMIF_RGBA_CTRL                         0x3e68

#define T6D_VD1_AFBCDM_VDTOP_CTRL0                 0x4838
#define T6D_VD2_AFBCDM_VDTOP_CTRL0                 0x48b8

/* for t6d end */
#endif	/*__DI_REG_V6_T6D_H__*/

