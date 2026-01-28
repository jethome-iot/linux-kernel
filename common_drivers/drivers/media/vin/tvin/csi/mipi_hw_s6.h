/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

// only for export functions implemented in csi_s6.c

#ifndef _MIPI_HW_S6_H

#include "csi.h"

void powerup_csi_analog_s6(struct csi_adapt *adap_dev);
void powerdown_csi_analog_s6(struct csi_adapt *adap_dev);

#endif
