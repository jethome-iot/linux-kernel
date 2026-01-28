/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef MEDIA_CAMERA_CAM_PROBE_H
#define MEDIA_CAMERA_CAM_PROBE_H

#include <linux/amlogic/media/camera/aml_cam_info.h>

enum cam_type_e {
	CAM_ON_SM1_S905D3,
	CAM_ON_S6_S905D5,
	CAM_UNKNOWN_BOARD
};

static inline enum cam_type_e get_cam_type(struct aml_cam_info_s *cam_dev)
{
	if (cam_dev)
		return cam_dev->cam_type;
	else
		return CAM_ON_SM1_S905D3;
}

#endif
