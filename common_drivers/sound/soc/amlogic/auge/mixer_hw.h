/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <linux/amlogic/pm.h>
#include <linux/amlogic/clk_measure.h>
#include <linux/amlogic/cpu_version.h>

#include "ddr_mngr.h"
#include "audio_io.h"
#include "../common/iomapres.h"
#include "regs.h"
#include "iomap.h"

int mixer_format_set(int channel, int sample_bit, int fddr_type);
int mixer_fifo_reset(void);
int mixer_source_set(int fifo_id);
int mic_mixer_source(int fifo_id);
int mic_mixer_fifo_reset(void);
int mic_mixer_format_set(int sample_bit, int fddr_type);

int mixer_eq_enable(struct frddr *fr, int enable);
int mixer_demux_sel(enum frddr_dest dst);
int mixer_coef_set(unsigned int vol);
int mixer_en(int enable);
int mixer_clip_top_en(int  enable);
