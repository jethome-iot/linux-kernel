/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __S6_MBOX_H__
#define __S6_MBOX_H__

#include "amlogic,mbox.h"

// MBOX DRIVER ID
#define S6_DSPA2REE0     0
#define S6_DSPA2REE1     1
#define S6_REE2DSPA0     2
#define S6_REE2DSPA1     3
#define S6_REE2DSPA2     4
#define S6_AO2REE        5
#define S6_REE2AO0       (S6_AO2REE + 1)
#define S6_REE2AO1       (S6_AO2REE + 2)
#define S6_REE2AO2       (S6_AO2REE + 3)
#define S6_REE2AO3       (S6_AO2REE + 4)
#define S6_REE2AO4       (S6_AO2REE + 5)
#define S6_REE2AO5       (S6_AO2REE + 6)
#define S6_REE2AO6       (S6_AO2REE + 7)

// DEVICE TREE ID
#define S6_REE2DSPA_DEV  S6_REE2DSPA0
#define S6_REE2DSPA_HOST S6_REE2DSPA1
#define S6_DSPA2REE_DEV  S6_DSPA2REE0
#define S6_DSPA2REE_HOST S6_DSPA2REE1
#define S6_REE2AO_DEV    S6_REE2AO0
#define S6_REE2AO_VRTC   S6_REE2AO1
#define S6_REE2AO_KEYPAD S6_REE2AO2
#define S6_REE2AO_AOCEC  S6_REE2AO3
#define S6_REE2AO_ETH    S6_REE2AO4
#define S6_REE2AO_LED    S6_REE2AO5
#define S6_REE2AO_IR     S6_REE2AO6

// MBOX CHANNEL ID
#define S6_MBOX_DSPA2REE  0
#define S6_MBOX_REE2DSPA  1
#define S6_MBOX_AO2REE    2
#define S6_MBOX_REE2AO    3
#define S6_MBOX_NUMS      4

// AOCPU STATUS MBOX CHANNEL ID
#define S6_MBOX_AO2TEE    4

#endif /* __S6_MBOX_H__ */
