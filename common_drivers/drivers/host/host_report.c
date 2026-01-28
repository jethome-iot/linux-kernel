// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "host_report.h"

void host_dsp_vad_report(struct host_module *host)
{
	input_event(host->host_dsp->input_device, EV_KEY, KEY_POWER, 1);
	input_sync(host->host_dsp->input_device);
	input_event(host->host_dsp->input_device, EV_KEY, KEY_POWER, 0);
	input_sync(host->host_dsp->input_device);
}

void host_dsp_vad_input_device_init(struct host_module *host)
{
	host->host_dsp->input_device->name = "vad_keypad";
	host->host_dsp->input_device->phys = "vad_keypad/input3";
	host->host_dsp->input_device->dev.parent = host->dev;
	host->host_dsp->input_device->id.bustype = BUS_ISA;
	host->host_dsp->input_device->id.vendor  = 0x0001;
	host->host_dsp->input_device->id.product = 0x0001;
	host->host_dsp->input_device->id.version = 0x0100;
	host->host_dsp->input_device->evbit[0] = BIT_MASK(EV_KEY);
	host->host_dsp->input_device->keybit[BIT_WORD(BTN_0)] = BIT_MASK(BTN_0);
	set_bit(KEY_POWER, host->host_dsp->input_device->keybit);
}

