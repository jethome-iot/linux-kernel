/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HOST_REPORT_H__
#define __HOST_REPORT_H__

#include "host.h"
#include <linux/input.h>

void host_dsp_vad_report(struct host_module *host);
void host_dsp_vad_input_device_init(struct host_module *host);

#endif /*_HOST_REPORT_H*/
