// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "mixer_hw.h"

int mixer_format_set(int channel, int sample_bit, int fddr_type)
{
	audiobus_update_bits(EE_AUDIO_MIXER_CTRL0, 0x7 << 20, fddr_type << 20);
	audiobus_update_bits(EE_AUDIO_MIXER_CTRL0, 0xf << 16, channel << 16);

	audiobus_update_bits(EE_AUDIO_MIXER_CTRL2, 0x1f << 24, 0x1 << 24);

	audiobus_update_bits(EE_AUDIO_MIXER_CTRL2, 0x1f << 11, (sample_bit - 1) << 11);
	audiobus_update_bits(EE_AUDIO_MIXER_CTRL2, 0x7 << 8, fddr_type << 8);

	if (channel == 2)
		audiobus_update_bits(EE_AUDIO_MIXER_CTRL1, 0xff << 0, 0x1 << 0);
	else if (channel == 8)
		audiobus_update_bits(EE_AUDIO_MIXER_CTRL1, 0xff << 0, 0xf << 0);
	audiobus_update_bits(EE_AUDIO_MIXER_CTRL1, 0x1 << 8, 0x1 << 8);
	return 0;
}
EXPORT_SYMBOL_GPL(mixer_format_set);

int mixer_fifo_reset(void)
{
	audiobus_update_bits(EE_AUDIO_SW_RESET1, 0x1 << 19, 0x1 << 19);
	audiobus_update_bits(EE_AUDIO_SW_RESET1, 0x1 << 19, 0x1 << 0);

	audiobus_update_bits(EE_AUDIO_MIXER_CTRL0, 0x1 << 29, 0x1 << 29);
	audiobus_update_bits(EE_AUDIO_MIXER_CTRL0, 0x1 << 29, 0x0 << 29);

	audiobus_update_bits(EE_AUDIO_MIXER_CTRL1, 0x1 << 28, 0x0 << 28);
	audiobus_update_bits(EE_AUDIO_MIXER_CTRL1, 0x1 << 29, 0x0 << 29);

	audiobus_update_bits(EE_AUDIO_MIXER_CTRL1, 0x1 << 28, 0x1 << 28);
	audiobus_update_bits(EE_AUDIO_MIXER_CTRL1, 0x1 << 29, 0x1 << 29);

	audiobus_update_bits(EE_AUDIO_MIXER_CTRL2, 0x1 << 31, 0x1 << 31);
	audiobus_update_bits(EE_AUDIO_MIXER_CTRL2, 0x1 << 31, 0x0 << 31);

	audiobus_update_bits(EE_AUDIO_MIXER_CTRL2, 0x1 << 30, 0x0 << 30);
	audiobus_update_bits(EE_AUDIO_MIXER_CTRL2, 0x1 << 29, 0x0 << 29);

	audiobus_update_bits(EE_AUDIO_MIXER_CTRL2, 0x1 << 30, 0x1 << 30);
	audiobus_update_bits(EE_AUDIO_MIXER_CTRL2, 0x1 << 29, 0x1 << 29);
	return 0;
}
EXPORT_SYMBOL_GPL(mixer_fifo_reset);

int mixer_source_set(int fifo_id)
{
	audiobus_update_bits(EE_AUDIO_MIXER_CTRL0, 0x7 << 0, fifo_id << 0);
	return 0;
}
EXPORT_SYMBOL_GPL(mixer_source_set);

static inline unsigned int
	calc_frddr_address(unsigned int reg, unsigned int base)
{
	return base + reg - EE_AUDIO_FRDDR_A_CTRL0;
}

int mixer_eq_enable(struct frddr *fr, int enable)
{
	unsigned int reg_base = fr->reg_base;
	unsigned int reg;

	reg = calc_frddr_address(EE_AUDIO_FRDDR_A_CTRL2, reg_base);
	aml_audiobus_update_bits(fr->actrl,
				reg, 0x1 << 3, 0 << 3);
	audiobus_update_bits(EE_AUDIO_MIXER_CTRL1, 0x1 << 27, 0x1 << 27);
	audiobus_update_bits(EE_AUDIO_MIXER_CTRL1, 0x7 << 24, 0x6 << 24);
	audiobus_update_bits(EE_AUDIO_MIXER_CTRL0, 0x1 << 3, 0 << 3);
	eqdrc_update_bits(AED_TOP_CTL0, 0x1 << 14, 1 << 14);
	audiobus_update_bits(EE_AUDIO_MIXER_CTRL4, 0x1 << 3, enable << 3);
	return 0;
}
EXPORT_SYMBOL_GPL(mixer_eq_enable);

int mixer_demux_sel(enum frddr_dest dst)
{
	unsigned int offset, reg, index = 0;

	index = dst;
	if (dst <= TDMOUT_C || dst == TDMOUT_D) {
		if (dst == TDMOUT_D)
			index = 3;
		if (index == 3) {
			reg = EE_AUDIO_TDMOUT_D_CTRL1;
		} else {
			offset = EE_AUDIO_TDMOUT_B_CTRL1
						- EE_AUDIO_TDMOUT_A_CTRL1;
			reg = EE_AUDIO_TDMOUT_A_CTRL1 + offset * index;
		}
		audiobus_update_bits(reg, 0x1 << 1, 0x1 << 1);
	} else {
		if (dst == SPDIFOUT_A)
			index = 0;
		else if (dst == SPDIFOUT_B)
			index = 1;
		offset = EE_AUDIO_SPDIFOUT_B_CTRL1 - EE_AUDIO_SPDIFOUT_CTRL1;
		reg = EE_AUDIO_SPDIFOUT_CTRL1 + offset * index;
		audiobus_update_bits(reg, 0x1 << 1, 0x1 << 1);
	}
	return 0;
}
EXPORT_SYMBOL_GPL(mixer_demux_sel);

int mic_mixer_source(int fifo_id)
{
	audiobus_update_bits(EE_AUDIO_MIXER_CTRL0, 0x7 << 4, fifo_id << 4);
	return 0;
}
EXPORT_SYMBOL_GPL(mic_mixer_source);

int mic_mixer_fifo_reset(void)
{
	audiobus_update_bits(EE_AUDIO_MIXER_CTRL3, 0x1 << 31, 0x1 << 31);
	audiobus_update_bits(EE_AUDIO_MIXER_CTRL3, 0x1 << 31, 0x0 << 31);
	audiobus_update_bits(EE_AUDIO_MIXER_CTRL3, 0x1 << 30, 0x0 << 30);
	audiobus_update_bits(EE_AUDIO_MIXER_CTRL3, 0x1 << 29, 0x0 << 29);
	audiobus_update_bits(EE_AUDIO_MIXER_CTRL3, 0x1 << 30, 0x1 << 30);
	audiobus_update_bits(EE_AUDIO_MIXER_CTRL3, 0x1 << 29, 0x1 << 29);
	return 0;
}
EXPORT_SYMBOL_GPL(mic_mixer_fifo_reset);

int mic_mixer_format_set(int sample_bit, int fddr_type)
{
	audiobus_update_bits(EE_AUDIO_MIXER_CTRL3, 0x1f << 24, 0x1 << 24);
	audiobus_update_bits(EE_AUDIO_MIXER_CTRL3, 0x1f << 11, (sample_bit - 1) << 11);
	audiobus_update_bits(EE_AUDIO_MIXER_CTRL3, 0x7 << 8, fddr_type << 8);
	return 0;
}
EXPORT_SYMBOL_GPL(mic_mixer_format_set);

int mixer_coef_set(unsigned int vol)
{
	audiobus_write(EE_AUDIO_MIXER_COEF0, vol);
	audiobus_write(EE_AUDIO_MIXER_COEF1, vol);
	audiobus_write(EE_AUDIO_MIXER_COEF2, vol);
	audiobus_write(EE_AUDIO_MIXER_COEF3, vol);
	return 0;
}
EXPORT_SYMBOL_GPL(mixer_coef_set);

int mixer_en(int enable)
{
	audiobus_update_bits(EE_AUDIO_MIXER_CTRL0, 0x1 << 31, enable << 31);
	return 0;
}
EXPORT_SYMBOL_GPL(mixer_en);

int mixer_clip_top_en(int enable)
{
	audiobus_update_bits(EE_AUDIO_MIXER_CTRL0, 0x1 << 30, enable << 30);
	return 0;
}
EXPORT_SYMBOL_GPL(mixer_clip_top_en);

