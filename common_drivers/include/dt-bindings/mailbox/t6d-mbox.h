/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __T6D_MBOX_H__
#define __T6D_MBOX_H__

#include "amlogic,mbox.h"

// MBOX DRIVER ID
#define T6D_AO2REE        0
#define T6D_REE2AO0       (T6D_AO2REE + 1)
#define T6D_REE2AO1       (T6D_AO2REE + 2)
#define T6D_REE2AO2       (T6D_AO2REE + 3)
#define T6D_REE2AO3       (T6D_AO2REE + 4)
#define T6D_REE2AO4       (T6D_AO2REE + 5)
#define T6D_REE2AO5       (T6D_AO2REE + 6)
#define T6D_REE2AO6       (T6D_AO2REE + 7)
#define T6D_REE2AO7       (T6D_AO2REE + 8)

#define T6D_REE2AO_DEV    T6D_REE2AO0
#define T6D_REE2AO_VRTC   T6D_REE2AO1
#define T6D_REE2AO_KEYPAD T6D_REE2AO2
#define T6D_REE2AO_AOCEC  T6D_REE2AO3
#define T6D_REE2AO_LED    T6D_REE2AO4
#define T6D_REE2AO_ETH    T6D_REE2AO5
#define T6D_REE2AO_IR     T6D_REE2AO6

// MBOX CHANNEL ID
#define T6D_MBOX_AO2REE    2
#define T6D_MBOX_REE2AO    3
#define T6D_MBOX_NUMS      2

// AOCPU STATUS MBOX CHANNEL ID
#define T6D_MBOX_AO2TEE    4

#endif /* __T6D_MBOX_H__ */
