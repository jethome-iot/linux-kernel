/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef MEDIA_CAMERA_CAM_PROBE_S6_H
#define MEDIA_CAMERA_CAM_PROBE_S6_H

#include <linux/amlogic/media/camera/aml_cam_info.h>

int cam_enable_clk_s6(struct aml_cam_info_s *cam_dev);
int cam_disable_clk_s6(struct aml_cam_info_s *cam_dev);

#endif
