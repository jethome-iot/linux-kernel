/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __TM2_MBOX_H__
#define __TM2_MBOX_H__

#include "amlogic,mbox.h"

// MBOX DRIVER ID
#define TM2_REE2AO0      0
#define TM2_REE2AO1      1
#define TM2_REE2AO2      2
#define TM2_REE2AO3      3
#define TM2_REE2AO4      4
#define TM2_REE2AO5      5
#define TM2_REE2AO6      6
#define TM2_REE2DSPA0    (TM2_REE2AO6 + 1)
#define TM2_REE2DSPA1    (TM2_REE2AO6 + 2)
#define TM2_REE2DSPA2    (TM2_REE2AO6 + 3)
#define TM2_DSPA2REE0    (TM2_REE2DSPA2 + 1)
#define TM2_REE2DSPB0    (TM2_DSPA2REE0 + 1)
#define TM2_REE2DSPB1    (TM2_DSPA2REE0 + 2)
#define TM2_REE2DSPB2    (TM2_DSPA2REE0 + 3)
#define TM2_DSPB2REE0    (TM2_REE2DSPB2 + 1)

#define TM2_REE2AO_DEV        TM2_REE2AO0
#define TM2_REE2AO_VRTC       TM2_REE2AO1
#define TM2_REE2AO_HIFI4DSPA  TM2_REE2AO2
#define TM2_REE2AO_HIFI4DSPB  TM2_REE2AO3
#define TM2_REE2AO_AOCEC      TM2_REE2AO4
#define TM2_REE2AO_LED        TM2_REE2AO5
#define TM2_REE2AO_ETH        TM2_REE2AO6

#define TM2_REE2DSPA_DEV      TM2_REE2DSPA0
#define TM2_REE2DSPA_DSP      TM2_REE2DSPA1
#define TM2_REE2DSPA_AUDIO    TM2_REE2DSPA2
#define TM2_DSPA2REE_DEV      TM2_DSPA2REE0
#define TM2_REE2DSPB_DEV      TM2_REE2DSPB0
#define TM2_REE2DSPB_DSP      TM2_REE2DSPB1
#define TM2_REE2DSPB_AUDIO    TM2_REE2DSPB2
#define TM2_DSPB2REE_DEV      TM2_DSPB2REE0

// MBOX CHANNEL ID
#define TM2_MBOX_REE2AO_LOW    0
#define TM2_MBOX_REE2AO_HIGH   1
#define TM2_MBOX_PL0_NUMS      2

#define TM2_MBOX_REE2DSPA      0
#define TM2_MBOX_DSPA2REE      1
#define TM2_MBOX_REE2DSPB      2
#define TM2_MBOX_DSPB2REE      3
#define TM2_MBOX_PL1_NUMS      4

#endif /* __TM2_MBOX_H__ */
