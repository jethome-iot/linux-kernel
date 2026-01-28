// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vout/DisplayPort/DPTX.h>
#include "dptx_common.h"
#include <linux/component.h>
#include <drm/amlogic/meson_drm_bind.h>

static int meson_DisplayPort_bind(struct device *dev, struct device *master, void *data);
static void meson_DisplayPort_unbind(struct device *dev, struct device *master, void *data);

struct drm_DisplayPort_wrapper {
	struct meson_DisplayPort_dev drm_dptx_instance;
	struct dptx_drv_s *dptx_drv;
	int drm_type;
	int drm_id;
};

#define to_drm_dptx_wrapper(x)	container_of(x, struct drm_DisplayPort_wrapper, drm_dptx_instance)

static struct drm_DisplayPort_wrapper drm_dptx_wrappers[DPTX_MAX_DRV];

static void dptx_drm_vmode_add(struct dptx_drv_s *dptx, struct drm_display_mode *pmode, u8 vmd_idx)
{
	struct dptx_detail_timing_s *raw_timing;
	struct dptx_vmode_s *vmd_p;
	u64 pclk;

	vmd_p = &dptx->vmode_mgr.vmodes[vmd_idx];
	raw_timing = &dptx->edid_info.dtd_timing[vmd_p->base_dtd_idx];

	pmode->clock = raw_timing->pclk / 1000;

	pmode->hdisplay = raw_timing->h_act;
	pmode->hsync_start = raw_timing->h_act + raw_timing->h_fp;
	pmode->hsync_end = raw_timing->h_act + raw_timing->h_fp + raw_timing->h_pw;
	pmode->htotal = raw_timing->h_period;

	pmode->vdisplay = raw_timing->v_act;
	pmode->vsync_start = raw_timing->v_act + raw_timing->v_fp;
	pmode->vsync_end = raw_timing->v_act + raw_timing->v_fp + raw_timing->v_pw;
	pmode->vtotal = raw_timing->v_period;

	pmode->width_mm = raw_timing->h_size;
	pmode->height_mm = raw_timing->v_size;

	switch (vmd_p->fr_adv) {
	case 0: //fr int
		pclk = (vmd_p->fr100_int + 50) / 100;
		pclk = pclk * raw_timing->h_period * raw_timing->v_period;
		break;
	case 1: //fr frac
		pclk = vmd_p->fr100_int;
		pclk *= 10;
		pclk = pclk * raw_timing->h_period * raw_timing->v_period;
		pclk = pclk / 1001;
		break;
	case 2: //raw fr
	default:
		pclk = raw_timing->pclk;
		break;
	}
	pmode->clock = pclk / 1000;
	//dptx->act_timing.pclk = pclk;
	pclk = dptx_div_around(pclk * 1000, dptx->act_timing.v_period);
	pclk = dptx_div_around(pclk, dptx->act_timing.h_period);

	//dptx->act_timing.cfmt = dptx_cfmt_table[dptx->vmode_mgr.vmode_cfmt_sel].cfmt_id;

	//str_add_vmode ??
	//sprintf(&pmode->name, "%ux%up%uhz")
	__str_add_vmode(dptx, pmode->name, vmd_p, pclk / 1000);

	DPTXPR(dptx->idx, LOG_I, "%s: %s, clock=%dkHz, htotal=%d, vtotal=%d",
		__func__, pmode->name, pmode->clock, pmode->htotal, pmode->vtotal);
}

static int get_dptx_modes(struct meson_DisplayPort_dev *dptx_drm_dev,
		struct drm_display_mode **modes, int *num)
{
	struct drm_DisplayPort_wrapper *wrapper = to_drm_dptx_wrapper(dptx_drm_dev);
	struct dptx_drv_s *dptx = wrapper->dptx_drv;
	u8 mode_idx = 0;

	struct drm_display_mode *nmodes;
	//struct lcd_detail_timing_s *ptiming;
	unsigned int i = 0, mode_cnt = 0, valid_mode_cnt = 0;

	if (!dptx)
		return -EFAULT;
	//mode_cnt = dptx->vmode_mgr.vmode_cnt;

	for (i = 0; i < DPTX_DRV_VMODE_MAX; i++) {
		if (dptx->vmode_mgr.vmodes[i].flag & VMODE_FLAG_VALID) {
			mode_cnt++;
			if (dptx->vmode_mgr.vmodes[i].cfmt_support)
				valid_mode_cnt++;
		}
	}
	if (i == DPTX_DRV_VMODE_MAX)
		return 0;

	DPTXPR(dptx->idx, LOG_I, "%s: %u/%u", __func__, valid_mode_cnt, mode_cnt);

	nmodes = kcalloc(valid_mode_cnt, sizeof(struct drm_display_mode), GFP_KERNEL);
	if (!nmodes) {
		*num = 0;
		return -ENOMEM;
	}

	if (dptx->vmode_mgr.vmodes[i].cfmt_support) {
		dptx_drm_vmode_add(dptx, &nmodes[mode_idx], i);
		mode_idx++;
	}

	*num = valid_mode_cnt;
	*modes = nmodes;

	return 0;
}

static int meson_DisplayPort_bind(struct device *dev, struct device *master, void *data)
{
	struct meson_drm_bound_data *bound_data = data;
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)dev_get_drvdata(dev);
	//int connector_type = 0;

	/*init drm instance*/
	drm_dptx_wrappers[dptx->idx].dptx_drv = dptx;
	drm_dptx_wrappers[dptx->idx].drm_dptx_instance.base.ver = MESON_DRM_CONNECTOR_V10;
	drm_dptx_wrappers[dptx->idx].drm_dptx_instance.get_modes = get_dptx_modes;

	if (dptx->idx == 1)
		drm_dptx_wrappers[dptx->idx].drm_type = DRM_MODE_CONNECTOR_MESON_DP_B;
	else
		drm_dptx_wrappers[dptx->idx].drm_type = DRM_MODE_CONNECTOR_MESON_DP_A;
	// DRM_MODE_CONNECTOR_MESON_EDP_A
	// DRM_MODE_CONNECTOR_MESON_EDP_B

	/*bind instance to drm*/
	if (bound_data->connector_component_bind) {
		drm_dptx_wrappers[dptx->idx].drm_dptx_instance.base.crtc_sel = dptx->crtc_sel;
		drm_dptx_wrappers[dptx->idx].drm_id =
			bound_data->connector_component_bind(bound_data->drm,
				drm_dptx_wrappers[dptx->idx].drm_type,
				&drm_dptx_wrappers[dptx->idx].drm_dptx_instance.base);
		DPTXPR(dptx->idx, LOG_I, "%s: connector_type: 0x%x, drm_id: %d", __func__,
			drm_dptx_wrappers[dptx->idx].drm_type,
			drm_dptx_wrappers[dptx->idx].drm_id);
	} else {
		DPTXPR(dptx->idx, LOG_E, "no bind func from drm");
	}

	return 0;
}

static void meson_DisplayPort_unbind(struct device *dev, struct device *master, void *data)
{
	struct meson_drm_bound_data *bound_data = data;
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)dev_get_drvdata(dev);

	if (bound_data->connector_component_unbind) {
		bound_data->connector_component_unbind(bound_data->drm,
			drm_dptx_wrappers[dptx->idx].drm_type,
			drm_dptx_wrappers[dptx->idx].drm_id);
		DPTXPR(dptx->idx, LOG_I, "%s: connector_type: 0x%x, drm_id: %d", __func__,
			drm_dptx_wrappers[dptx->idx].drm_type,
			drm_dptx_wrappers[dptx->idx].drm_id);
	} else {
		DPTXPR(dptx->idx, LOG_E, "no unbind func from drm");
	}

	drm_dptx_wrappers[dptx->idx].drm_id = 0;
}

static const struct component_ops meson_DisplayPort_bind_ops = {
	.bind	= meson_DisplayPort_bind,
	.unbind	= meson_DisplayPort_unbind,
};

int dptx_drm_add(struct device *dev)
{
	/*bind to drm*/
	component_add(dev, &meson_DisplayPort_bind_ops);

	return 0;
}

void dptx_drm_remove(struct device *dev)
{
	component_del(dev, &meson_DisplayPort_bind_ops);
}
