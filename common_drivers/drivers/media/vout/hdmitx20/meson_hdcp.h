/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_HDCP_H__
#define __MESON_HDCP_H__
#include <drm/amlogic/meson_connector_dev.h>

/* ioctl numbers */
enum {
	TEE_HDCP_START,
	TEE_HDCP_END,
	HDCP_DAEMON_LOAD_END,
	HDCP_DAEMON_REPORT,
	HDCP_EXE_VER_SET,
	HDCP_TX_VER_REPORT,
	HDCP_DOWNSTR_VER_REPORT,
	HDCP_EXE_VER_REPORT
};

#define TEE_HDCP_IOC_START    _IOW('P', TEE_HDCP_START, int)
#define TEE_HDCP_IOC_END    _IOW('P', TEE_HDCP_END, int)
#define HDCP_DAEMON_IOC_LOAD_END    _IOW('P', HDCP_DAEMON_LOAD_END, int)
#define HDCP_DAEMON_IOC_REPORT    _IOR('P', HDCP_DAEMON_REPORT, int)
#define HDCP_EXE_VER_IOC_SET    _IOW('P', HDCP_EXE_VER_SET, int)
#define HDCP_TX_VER_IOC_REPORT    _IOR('P', HDCP_TX_VER_REPORT, int)
#define HDCP_DOWNSTR_VER_IOC_REPORT    _IOR('P', HDCP_DOWNSTR_VER_REPORT, int)
#define HDCP_EXE_VER_IOC_REPORT    _IOR('P', HDCP_EXE_VER_REPORT, int)

enum {
	HDCP_TX22_DISCONNECT = 0,
	HDCP_TX22_START,
	HDCP_TX22_STOP
};

enum {
	HDCP22_DAEMON_LOADING = 0,
	HDCP22_DAEMON_DONE,
	HDCP22_DAEMON_TIMEOUT
};

#define HDCP_AUTH_TIMEOUT (40) /*40*200ms = 8s*/
#define HDCP22_LOAD_TIMEOUT (160)
#define TIMER_CHECK	(1 * HZ / 2)
#define TIMER_CHK_CNT 60

void meson_hdcp_init(void);
void meson_hdcp_exit(void);

void meson_hdcp_enable(int hdcp_type);
void meson_hdcp_disable(void);
void meson_hdcp_disconnect(void);

void meson_hdcp_reg_result_notify(struct connector_hdcp_cb *cb);

bool hdcp_tx22_daemon_ready(void);

unsigned int meson_hdcp_get_rx_cap(void);
unsigned int meson_hdcp_get_tx_cap(void);
bool is_hdcp22_stop_state(void);
unsigned char drm_hdmitx_get_hdcp_topo_info(void);
#endif
