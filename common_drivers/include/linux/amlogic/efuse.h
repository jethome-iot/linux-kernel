/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __EFUSE_AMLOGIC_H
#define __EFUSE_AMLOGIC_H

#define EFUSE_KEY_NAME_LEN	32

#define EFUSE_READ_CALI_ITEM		0x8200003E
#define EFUSE_CALI_SUBITEM_WHOBURN       0x100
#define EFUSE_CALI_SUBITEM_SENSOR0       0x101
#define EFUSE_CALI_SUBITEM_SARADC        0x102
#define EFUSE_CALI_SUBITEM_USBPHY        0x103
#define EFUSE_CALI_SUBITEM_MIPICSI       0x104
#define EFUSE_CALI_SUBITEM_HDMIRX        0x105
#define EFUSE_CALI_SUBITEM_ETHERNET      0x106
#define EFUSE_CALI_SUBITEM_CVBS          0x107
#define EFUSE_CALI_SUBITEM_EARCRX        0x108
#define EFUSE_CALI_SUBITEM_EARCTX        0x109
#define EFUSE_CALI_SUBITEM_USBCCLOGIC    0x10A
#define EFUSE_CALI_SUBITEM_BC            0x10B
#define EFUSE_CALI_SUBITEM_HDMITX        0x10C
#define EFUSE_CALI_SUBITEM_DEMODADC      0x10D
#define EFUSE_CALI_SUBITEM_HDMIRX21      0x10E
#define EFUSE_CALI_SUBITEM_SENSOR1       0x10F
#define EFUSE_CALI_SUBITEM_ODIO33        0x110
#define EFUSE_CALI_SUBITEM_SARADC_VREF   0x111
#define EFUSE_CALI_SUBITEM_SARADC_MIN    0x112
#define EFUSE_CALI_SUBITEM_SARADC_MAX    0x113
#define EFUSE_CALI_SUBITEM_VREF14        0x114
#define EFUSE_CALI_SUBITEM_ETH_TXAMP     0x115
#define EFUSE_CALI_SUBITEM_ETH_RESCTL    0x116
#define EFUSE_CALI_SUBITEM_U3PCIE_TX     0x117
#define EFUSE_CALI_SUBITEM_U3PCIE_RX     0x118
#define EFUSE_CALI_SUBITEM_DSI           0x119
#define EFUSE_CALI_SUBITEM_CSI           0x11A
#define EFUSE_CALI_SUBITEM_MISC_PZQ      0x11B
#define EFUSE_CALI_SUBITEM_P2P_VINLP     0x11C
#define EFUSE_CALI_SUBITEM_P2P_COMMON    0x11D
#define EFUSE_CALI_SUBITEM_MAX           0x0fff

#define EFUSE_LOCK_SUBITEM_BASE          0x1000
#define EFUSE_LOCK_SUBITEM_DGPK1_KEY     0x1000
#define EFUSE_LOCK_SUBITEM_DGPK2_KEY     0x1001
#define EFUSE_LOCK_SUBITEM_AUDIO_V_ID    0x1002
#define EFUSE_LOCK_SUBITEM_MAX           0X1FFF

struct efusekey_info {
	char keyname[EFUSE_KEY_NAME_LEN];
	unsigned int offset;
	unsigned int size;
};

int efuse_getinfo(char *item, struct efusekey_info *info);
ssize_t efuse_user_attr_show(char *name, char *buf);
ssize_t efuse_user_attr_store(char *name, const char *buf, size_t count);
ssize_t efuse_user_attr_read(char *name, char *buf);

int efuse_amlogic_cali_item_read(unsigned int item);
int efuse_amlogic_check_lockable_item(unsigned int item);

#endif
