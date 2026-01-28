/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef VPP_AFD_
#define VPP_AFD_

enum E_AFD_RESULT {
	AFD_RESULT_ERROR = -1,
	AFD_RESULT_OK = 0,
	AFD_RESULT_ENABLE_CHANGED = 1,
	AFD_RESULT_CROP_CHANGED = 1 << 1,
	AFD_RESULT_DISP_CHANGED = 1 << 2,
	AFD_RESULT_MAX = 0xff,
};

enum E_AFD_CROP_T {
	AFD_CROP_INVALID = 0,
	AFD_CROP_SYSFS = 1,
	AFD_CROP_DB = 2,
	AFD_CROP_MAX = 3
};

enum E_AFD_SOURCE_TYPE_T {
	AFD_SOURCE_TYPE_OTHERS = 0,
	AFD_SOURCE_TYPE_TUNER,
	AFD_SOURCE_TYPE_CVBS,
	AFD_SOURCE_TYPE_COMP,
	AFD_SOURCE_TYPE_HDMI,
	AFD_SOURCE_TYPE_PPMGR,
	AFD_SOURCE_TYPE_OSD,
	AFD_SOURCE_TYPE_HWC,
};

struct ar_fraction_s {
	int numerator;
	int denominator;
};

struct crop_rect_s {
	unsigned int top;
	unsigned int left;
	unsigned int bottom;
	unsigned int right;
};

struct pos_rect_s {
	unsigned int x_start;
	unsigned int y_start;
	unsigned int x_end;
	unsigned int y_end;
};

struct afd_in_param {
	void *ud_param;
	u32 ud_param_size;
	unsigned int video_w;
	unsigned int video_h;
	unsigned int screen_w;
	unsigned int screen_h;
	enum E_AFD_SOURCE_TYPE_T source_type;
	enum E_AFD_CROP_T user_crop_type;
	u32 afd_info;
	struct crop_rect_s src_crop;
	struct crop_rect_s user_crop;
	u32 select_mode;
	struct pos_rect_s disp_info;
	struct ar_fraction_s video_ar;
	struct ar_fraction_s screen_ar;
};

struct afd_out_param {
	bool afd_enable;
	struct crop_rect_s crop_info;
	struct pos_rect_s dst_pos;
};

typedef void *(*afd_init)(void);
typedef int (*afd_info_get)(void *handle, struct afd_in_param *in, struct afd_out_param *out);
typedef void (*afd_uninit)(void *handle);

struct VPP_AFD_Func_Ptr {
	afd_init init;
	afd_info_get info_get;
	afd_uninit uninit;
};

int register_afd_function(struct VPP_AFD_Func_Ptr *func, const char *ver);
int unregister_afd_function(struct VPP_AFD_Func_Ptr *func);
int afd_process(u8 id, struct afd_in_param *in, struct afd_out_param *out);
bool is_afd_available(u8 id);
#endif
