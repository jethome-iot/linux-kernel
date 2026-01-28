/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _AMBLT_IOC_H_
#define _AMBLT_IOC_H_

#include <linux/types.h>

/* **********************************
 * IOCTL define
 * **********************************
 */
struct amblt_data_s {
	unsigned int sum_r;
	unsigned int sum_g;
	unsigned int sum_b;
	unsigned short avg_r;
	unsigned short avg_g;
	unsigned short avg_b;
	unsigned char  err;
};

#define AMBLT_IOC_TYPE               'B'
#define AMBLT_IOC_GET_EN_CTRL			0x1
#define AMBLT_IOC_SET_EN_CTRL			0x2
#define AMBLT_IOC_GET_ZONE_H_V			0x3
#define AMBLT_IOC_SET_ZONE_H_V			0x4
#define AMBLT_IOC_GET_DATA				0x5

#define AMBLT_IOC_CMD_GET_EN_CTRL   \
	_IOR(AMBLT_IOC_TYPE, AMBLT_IOC_GET_EN_CTRL, unsigned int)
#define AMBLT_IOC_CMD_SET_EN_CTRL   \
	_IOW(AMBLT_IOC_TYPE, AMBLT_IOC_SET_EN_CTRL, unsigned int)
#define AMBLT_IOC_CMD_GET_ZONE_H_V   \
	_IOR(AMBLT_IOC_TYPE, AMBLT_IOC_GET_ZONE_H_V, unsigned int)
#define AMBLT_IOC_CMD_SET_ZONE_H_V   \
	_IOW(AMBLT_IOC_TYPE, AMBLT_IOC_SET_ZONE_H_V, unsigned int)
#define AMBLT_IOC_CMD_GET_DATA   \
	_IOR(AMBLT_IOC_TYPE, AMBLT_IOC_GET_DATA, struct amblt_data_s)

#endif /*_AMBLT_IOC_H_*/

