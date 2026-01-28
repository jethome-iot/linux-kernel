/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_INDMC_SENSOR_H__
#define __AML_INDMC_SENSOR_H__

enum dmc_type {
	TYPE_DDR3,
	TYPE_DDR4,
	TYPE_LPDDR4,
	TYPE_LPDDR3,
	TYPE_LPDDR2,
	TYPE_LPDDR4X,
	TYPE_LPDDR5,
	TYPE_MAX
};

extern struct platform_driver aml_indmc_sensor_platdrv;

#endif
