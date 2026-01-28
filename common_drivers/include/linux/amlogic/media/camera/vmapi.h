/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */

/*
 * include/linux/amlogic/media/camera/vmapi.h
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

#ifndef VM_API_INCLUDE_
#define VM_API_INCLUDE_

struct vm_output_para {
	int width;
	int height;
	int bytesperline;
	int v4l2_format;
	int canvas_id;
	int v4l2_memory;
	int zoom;     /* set -1 as invalid */
	int mirror;   /* set -1 as invalid */
	int angle;
	uintptr_t addr;/*unsigned*/
	unsigned int ext_canvas;
};

struct videobuf_buffer;
struct vb2_buffer;

#endif /* VM_API_INCLUDE_ */

