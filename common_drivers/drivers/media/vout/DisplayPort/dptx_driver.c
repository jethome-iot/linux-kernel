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
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/DisplayPort/DPTX.h>
#include <linux/amlogic/media/vout/DisplayPort/DPTX_notify.h>
#include <linux/amlogic/media/vout/DisplayPort/DPCD_REG.h>
#include "dptx_common.h"
#include <linux/sched/clock.h>

#define DPTX_HPD_TIMEOUT        200
#define DPTX_DELAY_AFTER_HPD    200

extern struct dptx_boot_ctrl_s dptx_uboot_configs[DPTX_MAX_DRV];

void dptx_act_timing_to_vinfo(struct dptx_drv_s *dptx)
{
	dptx->vinfo.name = dptx->act_timing.name;
	dptx->vinfo.frac = 0;
	dptx->vinfo.width               = dptx->act_timing.h_act;
	dptx->vinfo.height              = dptx->act_timing.v_act;
	dptx->vinfo.field_height        = dptx->act_timing.v_act;
	dptx->vinfo.aspect_ratio_num    = dptx->act_timing.h_size;
	dptx->vinfo.aspect_ratio_den    = dptx->act_timing.v_size;
	dptx->vinfo.screen_real_width   = dptx->act_timing.h_size;
	dptx->vinfo.screen_real_height  = dptx->act_timing.v_size;
	dptx->vinfo.sync_duration_num   = dptx->act_timing.sync_duration_num;
	dptx->vinfo.sync_duration_den   = dptx->act_timing.sync_duration_den;
	//dptx->vinfo.brr_duration = dptx->act_timings.frame_rate;
	dptx->vinfo.brr_duration = 0;
	dptx->vinfo.std_duration = dptx->act_timing.fr1000 / 1000;
	dptx->vinfo.vfreq_max    = dptx->act_timing.frame_rate_range[1];
	dptx->vinfo.vfreq_min    = dptx->act_timing.frame_rate_range[0];
	dptx->vinfo.video_clk    = dptx->act_timing.pclk;

	dptx->vinfo.htotal = dptx->act_timing.h_period;
	dptx->vinfo.vtotal = dptx->act_timing.v_period;
	dptx->vinfo.hsw    = dptx->act_timing.h_pw;
	dptx->vinfo.hbp    = dptx->act_timing.h_bp;
	dptx->vinfo.hfp    = dptx->act_timing.h_fp;
	dptx->vinfo.vsw    = dptx->act_timing.v_pw;
	dptx->vinfo.vbp    = dptx->act_timing.v_bp;
	dptx->vinfo.vfp    = dptx->act_timing.v_fp;
	dptx->vinfo.cur_enc_ppc =  1;
	dptx->vinfo.fr_adj_type = VOUT_FR_ADJ_NONE;
	dptx->vinfo.viu_color_fmt = (dptx->act_timing.cfmt <= DPTX_CFMT_RGB_12bit) ?
					COLOR_FMT_RGB444 : COLOR_FMT_YUV444;
	dptx->vinfo.vpp_post_out_color_fmt = (dptx->act_timing.cfmt <= DPTX_CFMT_RGB_12bit) ? 1 : 0;
}

void dptx_HPD_trigger_set(struct dptx_drv_s *dptx, u8 en)
{
	if (dptx->status & DPTX_STA_DRV_READY) {
		dptx_if_set_hpd_interrupt_mask(dptx,
			en ? DPTX_IRQ_HPD_EVENT_MASK | DPTX_IRQ_HPD_IRQ_EVENT : 0);

		if (en)
			dptx->status |= DPTX_STA_HPD_TRI_EN;
		else
			dptx->status &= ~DPTX_STA_HPD_TRI_EN;
	}
}

int dptx_timing_config_restore(struct dptx_drv_s *dptx)
{
	// u8 ret = 0;
	struct dptx_vmode_s *vmd_p;

	dptx_drv_check_HPD(dptx);

	if (dptx->status & DPTX_STA_HPD_HIGH) {
		//DP protocol: EDID -> DPCD, Intel: DPCD -> EDID
		if (dptx_DPCD_capability_to_link_cfg(dptx))
			DPTXPR(dptx->idx, LOG_E, "DPCD capability detect ERROR");

		if (!__dptx_EDID_probe(dptx, 1)) {
			dptx_vmode_manage(dptx);
			dptx->vmode_mgr.vmode_sel_idx = dptx_uboot_configs[dptx->idx].vmode_sel_idx;
			vmd_p = &dptx->vmode_mgr.vmodes[dptx->vmode_mgr.vmode_sel_idx];
			dptx_vmode_apply_to_act_timing(dptx, vmd_p);
		}
		// todo: edid not same
		DPTXPR(dptx->idx, LOG_E, "no");
	}

	dptx_act_timing_to_vinfo(dptx);

	return 0;
}

void dptx_driver_ready(struct dptx_drv_s *dptx)
{
	if (!(dptx->status & (DPTX_STA_PROBE_DONE))) {
		DPTXPR(dptx->idx, LOG_E, "DPTX sta=0x%x, not ready for %s", dptx->status, __func__);
		return;
	}

	DPTXPR(dptx->idx, LOG_I, "driver init(ver %s)", DPTX_DRV_VERSION);
	dptx_phy_enable(dptx);
	//dptx_power_init(dptx, 1);

	dptx_venc_enable(dptx, 0);

	dptx_if_transmitter_init(dptx);

	dptx->PWR_gpio = devm_gpiod_get(dptx->dev, "dptx-gpio-PWR", GPIOD_OUT_HIGH);
	if (IS_ERR_OR_NULL(dptx->PWR_gpio)) {
		DPTXPR(dptx->idx, LOG_E, "regist PWR gpio: %p, err: %d",
		       dptx->PWR_gpio, IS_ERR(dptx->PWR_gpio));
	} else {
		gpiod_direction_output(dptx->PWR_gpio, 1);
	}
	//dptx->CFG1_gpio = devm_gpiod_get(dptx->dev, "dptx-gpio-CFG1", GPIOD_IN);
	//if (IS_ERR_OR_NULL(dptx->CFG1_gpio)) {
	//	DPTXPR(dptx->idx, LOG_E, "regist CFG1 gpio: %p, err: %d",
	//	       dptx->CFG1_gpio, IS_ERR(dptx->CFG1_gpio));
	//}
	//dptx->CFG2_gpio = devm_gpiod_get(dptx->dev, "dptx-gpio-CFG2", GPIOD_IN);
	//if (IS_ERR_OR_NULL(dptx->CFG2_gpio)) {
	//	DPTXPR(dptx->idx, LOG_E, "regist CFG2 gpio: %p, err: %d",
	//	       dptx->CFG2_gpio, IS_ERR(dptx->CFG2_gpio));
	//}

	dptx->pin_c = devm_pinctrl_get_select(dptx->dev, "DPTX-ON");
	if (IS_ERR_OR_NULL(dptx->pin_c)) {
		DPTXPR(dptx->idx, LOG_E, "pinctrl mux: %p, err: %d",
		       dptx->PWR_gpio, IS_ERR(dptx->PWR_gpio));
	}
	//pconf->pin = devm_pinctrl_get_select(pdrv->dev, "DPTX-ON");
	//if (IS_ERR(pconf->pin)) {
	//	LCDERR("[%d]: set vbyone pinmux %s error\n",
	//	       pdrv->index, lcd_vbyone_pinmux_str[index]);
	//} else {
	//	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
	//		LCDPR("[%d]: set vbyone pinmux %s: 0x%px\n",
	//		      pdrv->index, lcd_vbyone_pinmux_str[index],
	//		      pconf->pin);
	//	}
	//}

	//dptx_HPD_pinmux_set(dptx);

	dptx->status |= DPTX_STA_DRV_READY;
}

void dptx_drv_check_HPD(struct dptx_drv_s *dptx)
{
	u16 i = 0;
	u8 data;

	if (!(dptx->status & DPTX_STA_DRV_READY)) {
		DPTXPR(dptx->idx, LOG_E, "DPTX sta=0x%x, not ready for %s", dptx->status, __func__);
		return;
	}

	if (dptx->hpd_ignore) {
		DPTXPR(dptx->idx, LOG_I, "ignore HPD");
		dptx_delay_ms(dptx->hpd_ignore);
		dptx->status |= DPTX_STA_HPD_HIGH;
		return;
	}

	while (i < DPTX_HPD_TIMEOUT) {
		data = dptx_if_get_hpd_level(dptx);
		if (data) {
			dptx->status |= DPTX_STA_HPD_HIGH;
			DPTXPR(dptx->idx, LOG_I, "HPD after %ums", i);
			return;
		}
		dptx_delay_ms(1);
		i++;
	}

	DPTXPR(dptx->idx, LOG_I, "DPTX HPD is LOW");
	dptx->status &= ~DPTX_STA_HPD_HIGH;
}

void dptx_drv_start(struct dptx_drv_s *dptx)
{
	struct dptx_vmode_s *dp_vmode = NULL;

	if (!(dptx->status & (DPTX_STA_PROBE_DONE | DPTX_STA_DRV_READY | DPTX_STA_HPD_HIGH))) {
		DPTXPR(dptx->idx, LOG_E, "DPTX sta=0x%x, not ready for %s", dptx->status, __func__);
		return;
	}

	dptx_venc_enable(dptx, 0);

	dptx_if_transmitter_init(dptx);
	__dptx_set_phy_config(dptx, 1); //?

	dptx_delay_ms(DPTX_DELAY_AFTER_HPD);

	//Power up link
	if (dptx_if_aux_write_single(dptx, DPCD_SET_POWER, 0x1))
		DPTXPR(dptx->idx, LOG_E, "DPCD SET POWER ERROR");
	dptx_delay_ms(20);

	//DP protocol: EDID -> DPCD, Intel: DPCD -> EDID
	if (dptx_DPCD_capability_to_link_cfg(dptx)) {
		DPTXPR(dptx->idx, LOG_E, "DPCD capability detect ERROR");
		dp_vmode = &DPTX_SafeMode_640x480_vmode;
		dptx_vmode_apply_to_act_timing(dptx, dp_vmode);
		dptx_act_timing_apply(dptx);
		//dptx_venc_enable(dptx, 1);
		return;
	}

	dptx->link_cfg.link_rate  = dptx->link_cfg.max_link_rate;
	dptx->link_cfg.lane_count = dptx->link_cfg.max_lane_count;

	__dptx_set_lane_config(dptx);
	__dptx_set_phy_config(dptx, 1); //?

	__dptx_link_training(dptx);

	if (1) {
		if (!__dptx_EDID_probe(dptx, 0)) {
			dptx_vmode_manage(dptx);

			//if (dptx->drm_hpd_cb.callback)
			//	dptx->drm_hpd_cb.callback(dptx->drm_hpd_cb.data);
			dp_vmode = dptx_get_optimum_vmode(dptx);
		}
		if (!dp_vmode) {
			dp_vmode = &DPTX_SafeMode_640x480_vmode;
			DPTXPR(dptx->idx, LOG_E, "no vmode to satisfy BW, to safemode 480P");
		}
		dptx_vmode_apply_to_act_timing(dptx, dp_vmode);
		dptx_act_timing_apply(dptx);
	}

	dptx_set_content_protection(dptx);

	dptx_if_set_MSA(dptx);

	dptx_venc_enable(dptx, 1);

	dptx_if_transmitter_output(dptx, 1);

	dptx->status |= DPTX_STA_LINK_ON;
	DPTXPR(dptx->idx, LOG_I, "%s enable main stream video finished", __func__);
}

void dptx_drv_disp_on(struct dptx_drv_s *dptx)
{
	if (!(dptx->status &
	      (DPTX_STA_PROBE_DONE | DPTX_STA_DRV_READY | DPTX_STA_HPD_HIGH | DPTX_STA_LINK_ON))) {
		DPTXPR(dptx->idx, LOG_E, "DPTX sta=0x%x, not ready for %s", dptx->status, __func__);
		return;
	}
	//if (dptx->idx == 0)
	//	run_command("gpio set GPIOY_1", 0);
	//else
	//	run_command("gpio set GPIOY_8", 0);

	dptx_mute_set(dptx, 0);

	dptx->status |= DPTX_STA_DISP_ON;
}

void dptx_drv_disp_off(struct dptx_drv_s *dptx)
{
	if (!(dptx->status & (DPTX_STA_PROBE_DONE | DPTX_STA_DRV_READY))) {
		DPTXPR(dptx->idx, LOG_E, "DPTX sta=0x%x, not ready for %s", dptx->status, __func__);
		return;
	}

	dptx_mute_set(dptx, 1);

	dptx->status &= ~DPTX_STA_DISP_ON;
}

void dptx_driver_close(struct dptx_drv_s *dptx)
{
	unsigned char auxdata;
	int ret;
	struct dptx_vmode_s *dp_vmode = NULL;

	if (!(dptx->status & (DPTX_STA_PROBE_DONE | DPTX_STA_DRV_READY))) {
		DPTXPR(dptx->idx, LOG_E, "DPTX sta=0x%x, not ready for %s", dptx->status, __func__);
		return;
	}
	//dptx_clear_timing(dptx);
	//Power down link
	auxdata = 0x2;
	ret = dptx_if_aux_write(dptx, DPCD_SET_POWER, 1, &auxdata);
	if (ret)
		DPTXPR(dptx->idx, LOG_V, "sink power down link failed");

	DPTXPR(dptx->idx, LOG_I, "disable main stream video");
	dptx_if_transmitter_output(dptx, 0);

	dp_vmode = &DPTX_SafeMode_640x480_vmode;
	dptx_vmode_apply_to_act_timing(dptx, dp_vmode);
	dptx_act_timing_apply(dptx);

	memset(&dptx->link_cfg, 0, sizeof(struct dptx_link_cfg_s));
	dptx->status &= ~DPTX_STA_LINK_ON;
	DPTXPR(dptx->idx, LOG_I, "%s finished", __func__);
}

void dptx_notify_check_HPD(struct dptx_drv_s *dptx)
{
	if (!(dptx->status & DPTX_STA_DRV_READY))
		return;

	dptx_drv_check_HPD(dptx);
}

void dptx_notify_set_backlight(struct dptx_drv_s *dptx, u8 en)
{
	if (!(dptx->status & DPTX_STA_DRV_READY))
		return;
}

void dptx_notify_set_screen_mute(struct dptx_drv_s *dptx, u8 en)
{
	if (!(dptx->status & DPTX_STA_DRV_READY))
		return;

	if (en)
		dptx_drv_disp_off(dptx);
	else
		dptx_drv_disp_on(dptx);
}

void dptx_notify_set_HPD_trigger(struct dptx_drv_s *dptx, u8 en)
{
	dptx_HPD_trigger_set(dptx, en);
}

void dptx_notify_set_link(struct dptx_drv_s *dptx, u8 en)
{
	if (!(dptx->status & DPTX_STA_DRV_READY))
		return;

	if (!en) {
		dptx_driver_close(dptx);
		return;
	}

	if (!(dptx->status & DPTX_STA_HPD_HIGH))
		dptx_drv_check_HPD(dptx);

	if (dptx->status & DPTX_STA_HPD_HIGH)
		dptx_drv_start(dptx);
}

void dptx_notify_set_driver_ready(struct dptx_drv_s *dptx, u8 en)
{
	if (en)
		dptx_driver_ready(dptx);
	else
		dptx_driver_close(dptx);
}

void dptx_notify_set_venc(struct dptx_drv_s *dptx, u8 en)
{
}
