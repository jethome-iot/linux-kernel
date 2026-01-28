/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */

/*
 * ad82088d.h  --  ad82088d ALSA SoC Audio driver
 *
 * Copyright 1998 Elite Semiconductor Memory Technology
 *
 * Author: ESMT Audio/Power Product BU Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __AD82088D_H__
#define __AD82088D_H__

#define AD82088D_REGISTER_COUNT	167
#define AD82088D_RAM_TABLE_COUNT	128

/* Register Address Map */
#define AD82088D_STATE_CTRL1_REG	0x00
#define AD82088D_STATE_CTRL2_REG	0x01
#define AD82088D_STATE_CTRL3_REG	0x02
#define AD82088D_VOLUME_CTRL_REG	0x03
#define AD82088D_STATE_CTRL5_REG	0x1A

#define AD82088D_CLK_DET_CTRL	0x17

#define CFADDR	0x1d
#define A1CF1	0x1e
#define A1CF2	0x1f
#define A1CF3	0x20
#define A1CF4	0x21
#define CFUD	0x32

#define AD82088D_DEVICE_ID_REG	0x37

#define AD82088D_COMPEN_FILTER_CTRL_REG	0x66

#define AD82088D_FAULT_REG		0x69
#define AD82088D_MAX_REG		0xA6

/* AD82088D_STATE_CTRL2_REG */
#define AD82088D_SSZ_DS			BIT(5)

/* AD82088D_STATE_CTRL1_REG */
#define AD82088D_SAIF_I2S		(0x0 << 5)
#define AD82088D_SAIF_LEFTJ		(0x1 << 5)
#define AD82088D_SAIF_FORMAT_MASK	GENMASK(7, 5)

/* AD82088D_STATE_CTRL3_REG */
#define AD82088D_MUTE			BIT(6)

/* AD82088D_STATE_CTRL5_REG */
#define AD82088D_SW_RESET			BIT(5)

/* AD82088D_CLK_DET_CTRL */
#define AD82088D_ASR_DET			BIT(7)

/* AD82088D_DEVICE_ID_REG */
#define AD82088D_DEVICE_ID 0x50

/* AD82088D_COMPEN_FILTER_CTRL_REG */
#define AD82088D_CHIP_SYNC_EN			BIT(6)

/* AD82088D_FAULT_REG */
#define AD82088D_OCE			BIT(7)
#define AD82088D_OTE			BIT(6)
#define AD82088D_UVE			BIT(5)
#define AD82088D_BSUVE			BIT(4)
#define AD82088D_BSOVE			BIT(3)
#define AD82088D_CLKE			BIT(2)
#define AD82088D_OVPE			BIT(1)
#define AD82088D_D_CLKE			BIT(0)
#endif /* __AD82088D_H__ */
