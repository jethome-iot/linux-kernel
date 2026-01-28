/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __EFFECTS_V2_H__
#define __EFFECTS_V2_H__

int check_aed_version(void);
struct audioeffect *get_audioeffects(void);
int str2int(char *str, unsigned int *data, int size);
int card_add_effect_v2_kcontrols(struct snd_soc_card *card);
int get_aed_dst(void);
bool is_aed_reserve_frddr(void);
int mixer_aed_read(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
int mixer_aed_write(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);

#endif
