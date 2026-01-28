// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/of.h>
#include <linux/reset.h>
#include <linux/clk.h>
#include <linux/sched/clock.h>
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vout/DisplayPort/DPTX.h>
#include <linux/amlogic/media/vout/DisplayPort/DPTX_notify.h>
#include "./dptx_common.h"
#include <linux/sched/clock.h>

/* **************************************************
 * vout server api
 * **************************************************
 */
static struct vinfo_s *dptx_get_current_info(void *data)
{
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)data;

	if (!dptx)
		return NULL;
	return &dptx->vinfo;
}

static int dptx_check_same_vmodeattr(char *mode, void *data)
{
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)data;

	DPTXPR(dptx->idx, LOG_V, "%s: %s", __func__, mode);

	return 1;
}

static int dptx_vmode_is_supported(enum vmode_e mode, void *data)
{
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)data;

	DPTXPR(dptx->idx, LOG_V, "%s: %x", __func__, mode);

	if ((mode & VMODE_MODE_BIT_MASK) == VMODE_DisplayPort)
		return true;
	return false;
}

static int dptx_set_current_vmode(enum vmode_e mode, void *data)
{
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)data;

	//unsigned long long local_time[2];
	int ret = 0;

	if (!dptx)
		return -1;

	if ((mode & VMODE_MODE_BIT_MASK) != VMODE_DisplayPort) {
		DPTXPR(dptx->idx, LOG_E, "unsupport mode 0x%x", mode & VMODE_MODE_BIT_MASK);
		return -1;
	}

	// lcd_vrr_dev_register(pdrv);
	if (mode & VMODE_INIT_BIT_MASK) { //first boot: set clktree
		//lcd_clk_gate_switch(pdrv, 1);
		//bypass
		dptx->status |= DPTX_STA_DISP_ON;
	} else if (dptx->status & DPTX_STA_LINK_ON) { //mode change by vout
		if (dptx->next_vmd_idx == 0xff)
			DPTXPR(dptx->idx, LOG_E, "invalid next vmode");
		else if (dptx->next_vmd_idx == dptx->vmode_mgr.vmode_sel_idx)
			DPTXPR(dptx->idx, LOG_E, "next vmode same as current");
		else
			dptx_user_set_vmode(dptx, dptx->next_vmd_idx);
	} else if (!(dptx->status & DPTX_STA_LINK_ON)) {
		//mode change by drm (disable + mode_change)
		if ((dptx->status & DPTX_STA_DRV_READY) == 0)
			dptx_notifier_call_chain(DPTX_EVENT_PREPARE, (void *)dptx);
		dptx_notifier_call_chain(DPTX_PRIORITY_HPD_CHECK, (void *)dptx);
		// aml_lcd_notifier_call_chain(LCD_EVENT_ENABLE, (void *)pdrv);
		// pdrv->status |= LCD_EVENT_ENABLE;
	}

	return ret;
}

static int dptx_vout_disable(enum vmode_e cur_vmod, void *data)
{
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)data;

	if (!dptx)
		return -1;

	//lcd_vrr_dev_unregister(pdrv);

	dptx_notifier_call_chain(DPTX_EVENT_DISABLE, (void *)dptx);

	return 0;
}

static int dptx_vout_set_state(int index, void *data)
{
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)data;

	if (!dptx)
		return -1;

	//mutex_lock(&lcd_vout_mutex);
	//pdrv->vout_state |= (1 << index);
	//pdrv->viu_sel = index;
	//mutex_unlock(&lcd_vout_mutex);

	return 0;
}

static int dptx_vout_clr_state(int index, void *data)
{
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)data;

	if (!dptx)
		return -1;

	//mutex_lock(&lcd_vout_mutex);
	//pdrv->vout_state &= ~(1 << index);
	//if (pdrv->viu_sel == index)
	//	pdrv->viu_sel = LCD_VIU_SEL_NONE;
	//mutex_unlock(&lcd_vout_mutex);

	return 0;
}

static int dptx_vout_get_state(void *data)
{
	//struct dptx_drv_s *dptx = (struct dptx_drv_s *)data;

	//return dptx->vout_state;
	return 0;
}

static int dptx_vout_get_disp_cap(char *buf, void *data)
{
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)data;
	u8 idx;
	u32 fr100, str_n = 0;
	struct dptx_vmode_s *vmode_p;

	for (idx = 0; idx < DPTX_DRV_VMODE_MAX; idx++) {
		vmode_p = &dptx->vmode_mgr.vmodes[idx];
		if (!(vmode_p->flag & VMODE_FLAG_VALID))
			continue;
		if (!(vmode_p->cfmt_support))
			continue;
		if (vmode_p->fr_adv == 1) { //5994 case
			fr100 = vmode_p->fr100_int * 1000;
			fr100 = fr100 / 1001;
		} else {
			fr100 = vmode_p->fr100_int;
		}

		str_n += __str_add_vmode(dptx, buf + str_n, vmode_p, fr100 / 100);
		str_n += sprintf(buf + str_n, "\n");
	}
	return str_n;
}

static enum vmode_e dptx_validate_vmode(char *mode, unsigned int frac, void *data)
{
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)data;

	if (!dptx || !mode || frac)
		return VMODE_MAX;

	DPTXPR(dptx->idx, LOG_I, "%s: %s (curr_mode=%s)", __func__, mode, dptx->vinfo.name);

	if (dptx_vmode_str_check(dptx, mode))
		return VMODE_MAX;

	return VMODE_DisplayPort;
}

static int dptx_set_vframe_rate_hint(int duration, void *data)
{
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)data;

	DPTXPR(dptx->idx, LOG_I, "%s: %d", __func__, duration);

	return 0;
}

static int dptx_get_vframe_rate_hint(void *data)
{
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)data;

	if (!dptx)
		return 0;

	return 0;
}

static void dptx_vout_debug_test(unsigned int num, void *data)
{
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)data;

	if (!dptx)
		return;

	dptx_debug_test(dptx, num);
}

static void dptx_vout_set_bl_brightness(unsigned int brightness, void *data)
{
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)data;

	if (!dptx)
		return;
}

static unsigned int dptx_vout_get_bl_brightness(void *data)
{
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)data;

	if (!dptx)
		return 0;

	if ((dptx->status & DPTX_STA_DISP_ON) == 0)
		return 1;

	return 1;
}

static int dptx_early_suspend(void *data)
{
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)data;

	if (!dptx)
		return -1;

	if ((dptx->status & DPTX_STA_PROBE_DONE) == 0)
		return 0;

	dptx_notifier_call_chain(DPTX_EVENT_DISP_OFF, (void *)dptx);
	DPTXPR(dptx->idx, LOG_I, "%s finished (0x%x)", __func__, dptx->status);

	return 0;
}

static int dptx_late_resume(void *data)
{
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)data;

	if (!dptx)
		return -1;

	if ((dptx->status & DPTX_STA_PROBE_DONE) == 0)
		return 0;

	if ((dptx->status & DPTX_STA_DRV_READY) == 0)
		dptx_notifier_call_chain(DPTX_EVENT_PREPARE, (void *)dptx);

	dptx_notifier_call_chain(DPTX_EVENT_HPD_CHECK, dptx);
	if (dptx->status & DPTX_STA_HPD_HIGH)
		dptx_notifier_call_chain(DPTX_EVENT_PLUG_IN, dptx);

	DPTXPR(dptx->idx, LOG_I, "%s finished (0x%x)", __func__, dptx->status);

	return 0;
}

//bind 3 vout op
void dptx_vout_server_init(struct dptx_drv_s *dptx)
{
	char *curr_vout_connector;//, *curr_vout_mode;
	char dptx_connector[8];
	u8 i;

	snprintf(dptx_connector, 7, "DP-%c", 'A' + dptx->idx);

#ifdef CONFIG_AMLOGIC_VOUT_SERVE
	curr_vout_connector = get_uboot_connector0_type();
	// curr_vout_mode = get_vout_mode_uboot();
	if (!strcmp(dptx_connector, curr_vout_connector)) {
		dptx->viu_sel = 1;
		dptx->crtc_sel = BIT(0);
		//strncpy(dptx->outputmode, curr_vout_mode, 32);
	}
#endif
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	curr_vout_connector = get_uboot_connector1_type();
	// curr_vout_mode = get_vout2_mode_uboot();
	if (!strcmp(dptx_connector, curr_vout_connector)) {
		dptx->viu_sel = 2;
		dptx->crtc_sel = BIT(1);
		//strncpy(dptx->outputmode, curr_vout_mode, 32);
	}
#endif
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
	curr_vout_connector = get_uboot_connector2_type();
	// curr_vout_mode = get_vout3_mode_uboot();
	if (!strcmp(dptx_connector, curr_vout_connector)) {
		dptx->viu_sel = 3;
		dptx->crtc_sel = BIT(2);
		//strncpy(dptx->outputmode, curr_vout_mode, 32);
	}
#endif
	for (i = 0; i < DPTX_MAX_VOUT; i++) {
		dptx->vout_server[i].op.get_vinfo = dptx_get_current_info;
		dptx->vout_server[i].op.set_vmode = dptx_set_current_vmode;
		dptx->vout_server[i].op.validate_vmode = dptx_validate_vmode;
		dptx->vout_server[i].op.check_same_vmodeattr = dptx_check_same_vmodeattr;
		dptx->vout_server[i].op.vmode_is_supported = dptx_vmode_is_supported;
		dptx->vout_server[i].op.disable = dptx_vout_disable;
		dptx->vout_server[i].op.set_state = dptx_vout_set_state;
		dptx->vout_server[i].op.clr_state = dptx_vout_clr_state;
		dptx->vout_server[i].op.get_state = dptx_vout_get_state;
		dptx->vout_server[i].op.get_disp_cap = dptx_vout_get_disp_cap;
		dptx->vout_server[i].op.set_vframe_rate_hint = dptx_set_vframe_rate_hint;
		dptx->vout_server[i].op.get_vframe_rate_hint = dptx_get_vframe_rate_hint;
		dptx->vout_server[i].op.set_bist = dptx_vout_debug_test;
		dptx->vout_server[i].op.set_bl_brightness = dptx_vout_set_bl_brightness;
		dptx->vout_server[i].op.get_bl_brightness = dptx_vout_get_bl_brightness;
		dptx->vout_server[i].op.vout_suspend = dptx_early_suspend;
		dptx->vout_server[i].op.vout_resume = dptx_late_resume;
		dptx->vout_server[i].data = (void *)dptx;
		dptx->vout_server[i].name = kzalloc(32, GFP_KERNEL);
		if (dptx->vout_server[i].name)
			sprintf(dptx->vout_server[i].name, "dptx%u_vout%u_server", dptx->idx, i);
	}

	if (dptx->viu_sel == 1) {
#ifdef CONFIG_AMLOGIC_VOUT_SERVE
		DPTXPR(dptx->idx, LOG_I, "regist[0]: %s", dptx->vout_server[0].name);
		vout_register_server(&dptx->vout_server[0]);
#endif
	} else if (dptx->viu_sel == 2) {
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
		DPTXPR(dptx->idx, LOG_I, "regist[1]: %s", dptx->vout_server[1].name);
		vout2_register_server(&dptx->vout_server[1]);
#endif
	} else if (dptx->viu_sel == 3) {
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
		DPTXPR(dptx->idx, LOG_I, "regist[2]: %s", dptx->vout_server[2].name);
		vout3_register_server(&dptx->vout_server[2]);
#endif
	}
}

void dptx_vout_server_deinit(struct dptx_drv_s *dptx)
{
#ifdef CONFIG_AMLOGIC_VOUT_SERVE
	if (dptx->viu_sel == 1)
		vout_unregister_server(&dptx->vout_server[0]);
#endif
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	if (dptx->viu_sel == 2)
		vout2_unregister_server(&dptx->vout_server[1]);
#endif
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
	if (dptx->viu_sel == 3)
		vout3_unregister_server(&dptx->vout_server[2]);
#endif
}
