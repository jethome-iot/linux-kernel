/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _AMLOGIC_DisplayPort_TX_NOTIFY_H
#define _AMLOGIC_DisplayPort_TX_NOTIFY_H

#include <linux/notifier.h>

#define DPTX_PRIORITY_HPD_IGNORE       31
#define DPTX_PRIORITY_HPD_CHECK        30

#define DPTX_PRIORITY_BACKLIGHT_OFF    8
#define DPTX_PRIORITY_SCREEN_BLACK     7
#define DPTX_PRIORITY_LINK_OFF         6
#define DPTX_PRIORITY_TX_CLOSE         5

#define DPTX_PRIORITY_TX_READY         4
#define DPTX_PRIORITY_LINK_ON          3
#define DPTX_PRIORITY_SCREEN_RESTORE   2
#define DPTX_PRIORITY_BACKLIGHT_ON     1

#define DPTX_PRIORITY_HPD_RESTORE      0

/* original event */
#define DPTX_EVENT_BACKLIGHT_OFF       BIT(0)
#define DPTX_EVENT_SCREEN_BLACK        BIT(1)
#define DPTX_EVENT_LINK_OFF            BIT(2)
#define DPTX_EVENT_TX_CLOSE            BIT(3)

#define DPTX_EVENT_TX_READY            BIT(4)
#define DPTX_EVENT_HPD_CHECK           BIT(5)
#define DPTX_EVENT_LINK_ON             BIT(6)
#define DPTX_EVENT_SCREEN_RESTORE      BIT(7)

#define DPTX_EVENT_BACKLIGHT_ON        BIT(8)
#define DPTX_EVENT_HPD_IGNORE          BIT(9)
#define DPTX_EVENT_HPD_RESTORE         BIT(10)

//! when (early suspend)
#define DPTX_EVENT_DISP_OFF          \
	(DPTX_EVENT_HPD_IGNORE |     \
	 DPTX_EVENT_BACKLIGHT_OFF | DPTX_EVENT_SCREEN_BLACK | DPTX_EVENT_LINK_OFF)
//! when (late resume)
// #define DPTX_EVENT_DISP_ON        (DPTX_EVENT_HPD_CHECK)

//! when (plug out)
#define DPTX_EVENT_PLUG_OUT       (DPTX_EVENT_DISP_OFF | DPTX_EVENT_HPD_RESTORE)
//! when (plug in)
#define DPTX_EVENT_PLUG_IN           \
	(DPTX_EVENT_HPD_IGNORE |     \
	 DPTX_EVENT_LINK_ON | DPTX_EVENT_SCREEN_RESTORE | DPTX_EVENT_BACKLIGHT_ON | \
	 DPTX_EVENT_HPD_RESTORE)

//! when (suspend)
#define DPTX_EVENT_UNPREPARE      (DPTX_EVENT_TX_CLOSE)
//! when (resume)
#define DPTX_EVENT_PREPARE        (DPTX_EVENT_TX_READY)

#define DPTX_EVENT_ENABLE         (DPTX_EVENT_PREPARE  | DPTX_EVENT_DISP_ON)
#define DPTX_EVENT_DISABLE        (DPTX_EVENT_DISP_OFF | DPTX_EVENT_UNPREPARE)

int dptx_notifier_register(struct notifier_block *nb);
int dptx_notifier_unregister(struct notifier_block *nb);
int dptx_notifier_call_chain(unsigned long event, void *v);

#endif /* _INC_LCD_NOTIFY_H_ */
