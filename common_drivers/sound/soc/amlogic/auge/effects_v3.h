/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __EFFECTS_V3_H__
#define __EFFECTS_V3_H__

int add_effect_v3_kcontrols(struct snd_soc_card *card);
void aed_v3_set_filter_data(void);
void aed_v3_set_drc_data(void);

#endif
