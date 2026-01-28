/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _INC_LCD_H
#define _INC_LCD_H

#include <linux/types.h>

/* **********************************
 * HDR info define
 * **********************************
 */
struct lcd_optical_info_s {
	unsigned int hdr_support;
	unsigned int features;
	unsigned int primaries_r_x;
	unsigned int primaries_r_y;
	unsigned int primaries_g_x;
	unsigned int primaries_g_y;
	unsigned int primaries_b_x;
	unsigned int primaries_b_y;
	unsigned int white_point_x;
	unsigned int white_point_y;
	unsigned int luma_max;
	unsigned int luma_min;
	unsigned int luma_avg;

	unsigned char ldim_support;  /* adv_flag_0 */
	unsigned char adv_flag_1;
	unsigned char adv_flag_2;
	unsigned char adv_flag_3;
	unsigned int luma_peak;
};

/**************************************
 * Ioctl Phy Info
 * ************************************
 */
#define CH_LANE_MAX 32

struct ioctl_phy_lane_s {
	unsigned int preem;
	unsigned int amp;
};

struct ioctl_phy_config_s {
	unsigned int flag;
	unsigned int vswing;
	unsigned int vcm;
	unsigned int odt;
	unsigned int ref_bias;
	unsigned int cv_mode;
	unsigned int weakly_pull_down;
	unsigned int lane_num;
	unsigned int ext_pullup;
	unsigned int vswing_level;
	unsigned int preem_level;
	int ioctl_mode;
	struct ioctl_phy_lane_s ioctl_lane[CH_LANE_MAX];
};

/* **********************************
 * IOCTL define
 * **********************************
 */
struct aml_lcd_tcon_bin_s {
	unsigned int index;
	unsigned int size;
	union {
		void *ptr;
		long long ptr_length;
	};
};

struct aml_lcd_ss_ctl_s {
	unsigned int level;
	unsigned int freq;
	unsigned int mode;
};

struct aml_lcd_dccd_config_s {
	unsigned int size;
	union {
		void *ptr;
		unsigned long long unuse;
	} data;
};

#define LCD_IOC_TYPE               'C'
#define LCD_IOC_NR_GET_HDR_INFO    0x0
#define LCD_IOC_NR_SET_HDR_INFO    0x1
#define LCD_IOC_GET_TCON_BIN_MAX_CNT_INFO 0x2
#define LCD_IOC_SET_TCON_DATA_INDEX_INFO  0x3
#define LCD_IOC_GET_TCON_BIN_PATH_INFO    0x4
#define LCD_IOC_SET_TCON_BIN_DATA_INFO    0x5

#define LCD_IOC_POWER_CTRL        0x6
#define LCD_IOC_MUTE_CTRL         0x7
#define LCD_IOC_GET_FRAME_RATE    0x9
#define LCD_IOC_SET_PHY_PARAM     0xa
#define LCD_IOC_GET_PHY_PARAM     0xb
#define LCD_IOC_SET_SS            0xc
#define LCD_IOC_GET_SS            0xd

/*
 * tcon use 0x20~0x2f
 */
#define TCON_IOC_SET_DCCD         0x20
#define TCON_IOC_SET_QUICK_REG    0x21
#define TCON_IOC_SET_CORE_BASE    0x22
#define TCON_IOC_GET_DCCD_FLG     0x23
#define TCON_IOC_GET_CALC_BUF     0x24
#define TCON_IOC_GET_CALC_STATUS  0x25
#define TCON_IOC_GET_CALC_CNT     0x26
#define TCON_IOC_SET_DCCD_DONE    0x27

#define LCD_IOC_CMD_GET_HDR_INFO   \
	_IOR(LCD_IOC_TYPE, LCD_IOC_NR_GET_HDR_INFO, struct lcd_optical_info_s)
#define LCD_IOC_CMD_SET_HDR_INFO   \
	_IOW(LCD_IOC_TYPE, LCD_IOC_NR_SET_HDR_INFO, struct lcd_optical_info_s)
#define LCD_IOC_CMD_GET_TCON_BIN_MAX_CNT_INFO   \
	_IOR(LCD_IOC_TYPE, LCD_IOC_GET_TCON_BIN_MAX_CNT_INFO, unsigned int)
#define LCD_IOC_CMD_SET_TCON_DATA_INDEX_INFO   \
	_IOW(LCD_IOC_TYPE, LCD_IOC_SET_TCON_DATA_INDEX_INFO, unsigned int)
#define LCD_IOC_CMD_GET_TCON_BIN_PATH_INFO   \
	_IOR(LCD_IOC_TYPE, LCD_IOC_GET_TCON_BIN_MAX_CNT_INFO, long long)
#define LCD_IOC_CMD_SET_TCON_BIN_DATA_INFO   \
	_IOW(LCD_IOC_TYPE, LCD_IOC_SET_TCON_BIN_DATA_INFO, struct aml_lcd_tcon_bin_s)

#define LCD_IOC_CMD_POWER_CTRL   \
	_IOW(LCD_IOC_TYPE, LCD_IOC_POWER_CTRL, unsigned int)
#define LCD_IOC_CMD_MUTE_CTRL   \
	_IOW(LCD_IOC_TYPE, LCD_IOC_MUTE_CTRL, unsigned int)
#define LCD_IOC_CMD_SET_PHY_PARAM   \
	_IOW(LCD_IOC_TYPE, LCD_IOC_SET_PHY_PARAM, struct ioctl_phy_config_s)
#define LCD_IOC_CMD_GET_PHY_PARAM   \
	_IOR(LCD_IOC_TYPE, LCD_IOC_GET_PHY_PARAM, struct ioctl_phy_config_s)
#define LCD_IOC_CMD_SET_SS   \
	_IOW(LCD_IOC_TYPE, LCD_IOC_SET_SS, struct aml_lcd_ss_ctl_s)
#define LCD_IOC_CMD_GET_SS   \
	_IOR(LCD_IOC_TYPE, LCD_IOC_GET_SS, struct aml_lcd_ss_ctl_s)

#define TCON_IOC_CMD_SET_DCCD \
	_IOW(LCD_IOC_TYPE, TCON_IOC_SET_DCCD, struct aml_lcd_dccd_config_s)
#define TCON_IOC_CMD_SET_QUICK_REG \
	_IOW(LCD_IOC_TYPE, TCON_IOC_SET_QUICK_REG, unsigned int)
#define TCON_IOC_CMD_SET_CORE_BASE \
	_IOW(LCD_IOC_TYPE, TCON_IOC_SET_CORE_BASE, struct aml_lcd_dccd_config_s)
#define TCON_IOC_CMD_GET_DCCD_FLG \
	_IOR(LCD_IOC_TYPE, TCON_IOC_GET_DCCD_FLG, unsigned int)
#define TCON_IOC_CMD_GET_CALC_BUF \
	_IOR(LCD_IOC_TYPE, TCON_IOC_GET_CALC_BUF, struct aml_lcd_dccd_config_s)
#define TCON_IOC_CMD_GET_CALC_STATUS \
	_IOR(LCD_IOC_TYPE, TCON_IOC_GET_CALC_STATUS, unsigned int)
#define TCON_IOC_CMD_GET_CALC_CNT \
	_IOR(LCD_IOC_TYPE, TCON_IOC_GET_CALC_CNT, unsigned int)
#define TCON_IOC_CMD_SET_DCCD_DONE \
	_IOW(LCD_IOC_TYPE, TCON_IOC_SET_DCCD_DONE, unsigned int)

/* **********************************
 * lcd_extern IOCTL define
 * **********************************
 */
struct aml_lcd_extern_bin_s {
	unsigned int dev_index;
	unsigned int i2c_index;
	unsigned int multi_id;
	unsigned int bin_size;
	union {
		void *ptr;
		long long ptr_length;
	};
};

#define LCD_EXT_IOC_TYPE               'E'
#define LCD_EXT_IOC_NR_SET_BIN         0x01

#define LCD_EXT_IOC_CMD_SET_HDR_INFO   \
	_IOW(LCD_EXT_IOC_TYPE, LCD_EXT_IOC_NR_SET_BIN, struct aml_lcd_extern_bin_s)

#endif
