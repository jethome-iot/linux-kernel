// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sched/clock.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include "lcd_reg.h"
#include "lcd_common.h"
#include "lcd_clk/lcd_clk_config.h"

static inline int is_fr_change(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_detail_timing_s *pt = &pdrv->config.timing.act_timing;
	struct aml_fr_lock_s *fr_lock = pdrv->fr_lock;
	struct lcd_clk_config_s *cconf = get_lcd_clk_config(pdrv);

	if (!pt || !fr_lock || !cconf)
		return 0;
	if (fr_lock->base_dura_num != pt->sync_duration_num ||
	   fr_lock->base_dura_den != pt->sync_duration_den ||
	   fr_lock->pll_base_m != cconf->pll_m ||
	   fr_lock->pll_base_frac != cconf->pll_frac)
		return 1;
	else
		return 0;
}

static inline void fr_lock_update_freq(struct aml_lcd_drv_s *pdrv)
{
	struct aml_fr_lock_s *fr_lock = pdrv->fr_lock;
	unsigned long long pll_freq;
	struct lcd_clk_config_s *cconf = get_lcd_clk_config(pdrv);
	unsigned int temp_m, temp_frac;

	if (!fr_lock || !cconf)
		return;

	pll_freq = fr_lock->pll_adj_hz;
	temp_frac = lcd_do_div((u64)do_div(pll_freq, 24000000) << 17, 24000000);
	temp_m = pll_freq;
	if (temp_m > cconf->pll_m) {
		temp_m = cconf->pll_m;
		temp_frac = cconf->data->pll_frac_range + temp_frac;
	} else if (temp_m  < cconf->pll_m) {
		temp_m = cconf->pll_m;
		temp_frac = (cconf->data->pll_frac_range - temp_frac) |
			(1 << cconf->data->pll_frac_sign_bit);
	}

	fr_lock->pll_adj_m = temp_m;
	fr_lock->pll_adj_frac = temp_frac;
	lcd_vlock_m_update(pdrv->index, fr_lock->pll_adj_m);
	lcd_vlock_frac_update(pdrv->index, fr_lock->pll_adj_frac);
}

void fr_lock_recovery_freq(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf = get_lcd_clk_config(pdrv);
	struct aml_fr_lock_s *fr_lock = pdrv->fr_lock;

	if (!fr_lock || !cconf)
		return;

	lcd_vlock_m_update(pdrv->index, cconf->pll_m);
	lcd_vlock_frac_update(pdrv->index, cconf->pll_frac);
}

static inline int hw_framelock_working(int enc_mux)
{
	int sta;

	sta = get_framelock_sta();
	if (sta == 2 || (sta == 1 && vlock_is_working(enc_mux)))
		return 1;
	else
		return 0;
}

#define FRL_CNT_NULTI_BITS 10 //(1024 multi)
#define FRL_CALI_FRAMES_BITS 8 //(128 frmes to calibration measure count)
#define FRL_CALI_FRAMES BIT(FRL_CALI_FRAMES_BITS)

void lcd_fr_lock_init(struct aml_lcd_drv_s *pdrv)
{
	struct aml_fr_lock_s *fr_lock = pdrv->fr_lock;
	int ret = 0;

	if (!pdrv->fr_lock_en || !fr_lock)
		return;

	fr_lock->en = 0;
	fr_lock->rst = 1;
	fr_lock->dbg = 0;
	ret = init_slide_filter(&fr_lock->ft_cnt, 64);
	ret |= init_slide_filter(&fr_lock->ft_time, 64);
	if (ret) {
		kfree(fr_lock->ft_cnt.data);
		kfree(fr_lock->ft_time.data);
		kfree(pdrv->fr_lock);
		pdrv->fr_lock = NULL;
	}
}

static inline void lcd_fr_lock_auto_switch(struct aml_lcd_drv_s *pdrv)
{
	struct aml_fr_lock_s *fr_lock = pdrv->fr_lock;
	unsigned int ss_ppm;
	int sta = 0, sta1 = 0;
	int ret;

	fr_lock->hw_vlock_sta = hw_framelock_working(pdrv->index);
	if (fr_lock->hw_vlock_sta && !fr_lock->hw_vlock_sta_last) {
		sta = -1;//off
		if (fr_lock->ss_switching) {
			lcd_ss_enable(pdrv->index, fr_lock->ss_sta_back);
			fr_lock->ss_switching = 0;
		}
	} else if (!fr_lock->hw_vlock_sta && fr_lock->hw_vlock_sta_last) {
		sta = 1; //on
	}
	fr_lock->hw_vlock_sta_last = fr_lock->hw_vlock_sta;

	ret = lcd_get_ss_num(pdrv, &fr_lock->ss_level, &ss_ppm,
			&fr_lock->ss_freq, &fr_lock->ss_mode);
	fr_lock->ss_sta = (ret < 0) ? 0 : ret;
	if (!fr_lock->ss_switching) {
		if (fr_lock->ss_sta && !fr_lock->ss_sta_last)
			sta1 = 1;//on
		else if (!fr_lock->ss_sta && fr_lock->ss_sta_last)
			sta1 = -1;//off
	}
	fr_lock->ss_sta_last = fr_lock->ss_sta;

	if (!fr_lock->en &&
	   ((sta == 1 && fr_lock->ss_sta) || (sta1 == 1 && !fr_lock->hw_vlock_sta))) {
		//switch on
		fr_lock->en = 1;
		fr_lock->step = 0;
		if (fr_lock->show)
			LCDPR("fr_lock auto switch on\n");
	} else if (fr_lock->en && (sta == -1 || sta1 == -1)) {
		//switch off
		fr_lock->en = 0;
		lcd_venc_vrr_recovery(pdrv);
		fr_lock_recovery_freq(pdrv);
		if (fr_lock->show)
			LCDPR("fr_lock auto switch off\n");
	}
}

void lcd_fr_lock(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_detail_timing_s *pt;
	struct lcd_clk_config_s *cconf = get_lcd_clk_config(pdrv);
	struct aml_fr_lock_s *fr_lock = pdrv->fr_lock;
	unsigned int offset = 0;
	long long temp, msr_time_ns = 0;
	int msr_cnt = 0;
	int temp_cnt = 0, err = 0;

	if (!fr_lock || !cconf)
		return;

	msr_time_ns = (s64)sched_clock();
	msr_cnt = (int)vout_frame_cnt_measure(pdrv->index);

	offset = pdrv->data->offset_venc_data[pdrv->index];
	pt = &pdrv->config.timing.act_timing;

	lcd_fr_lock_auto_switch(pdrv);

	if (!fr_lock->en && !fr_lock->dbg)
		return;

	if (is_fr_change(pdrv) || fr_lock->step == 0) {
		fr_lock->base_dura_num = pt->sync_duration_num;
		fr_lock->base_dura_den = pt->sync_duration_den;
		fr_lock->pll_base_m = cconf->pll_m;
		fr_lock->pll_base_frac = cconf->pll_frac;

		if (!fr_lock->base_dura_den) {
			if (fr_lock->show)
				LCDPR("error base_dura_den\n");
			goto fr_lock_exit;
		}
		fr_lock->base_fr = fr_lock->base_dura_num * 100 / fr_lock->base_dura_den;
		fr_lock->base_vtotal = pt->v_period;
		fr_lock->stable_cnt = 10;
		fr_lock_recovery_freq(pdrv);
		lcd_venc_adj_vtotal(pdrv, pt->v_period);

		fr_lock->ss_sta_back = fr_lock->ss_sta;
		lcd_ss_enable(pdrv->index, 0);
		fr_lock->ss_switching = 1;
		fr_lock->step = 1;
		if (fr_lock->show)
			LCDPR("ss close: level:%u, freq:%u, mode:%u\n",
				fr_lock->ss_level, fr_lock->ss_freq, fr_lock->ss_mode);
	}

	if (fr_lock->step == 1)// wait vmode change ok and clock stable after close ssc
		if (fr_lock->stable_cnt-- <= 0)
			fr_lock->step = 2;

	if (fr_lock->step == 2) {
		fr_lock->pll_base_hz = lcd_pll_freq_get(pdrv->index);
		fr_lock->calibration_cnt = FRL_CALI_FRAMES;
		fr_lock->msr_sum_cnt_m = 0;
		fr_lock->step = 3;
	}

	if (fr_lock->step == 3) {
		fr_lock->calibration_cnt--;
		fr_lock->msr_sum_cnt_m += msr_cnt;

		if (fr_lock->calibration_cnt == 0) {
			fr_lock->msr_sum_cnt_m <<= FRL_CNT_NULTI_BITS;
			fr_lock->exp_vs_cnt_m = fr_lock->msr_sum_cnt_m >> FRL_CALI_FRAMES_BITS;
			fr_lock->exp_vs_cnt = fr_lock->exp_vs_cnt_m >> FRL_CNT_NULTI_BITS;

			lcd_ss_enable(pdrv->index, fr_lock->ss_sta_back);
			fr_lock->step = 4;
			fr_lock->rst = 1;
			if (fr_lock->show)
				LCDPR("ss recovery: en:%d, level:%u, freq:%u, mode:%u\n",
					fr_lock->ss_sta_back, fr_lock->ss_level,
					fr_lock->ss_freq, fr_lock->ss_mode);
		}
	}

	if (fr_lock->step == 4) {
		fr_lock->step = 5;
		fr_lock->ss_switching = 0;
	}

	if (fr_lock->step < 5)
		goto fr_lock_exit;

/*===============================================================================================*/
	if (fr_lock->rst) {
		if (fr_lock->pll_base_hz == 0 || fr_lock->base_dura_num == 0 || pt->v_period == 0) {
			fr_lock->step = 0;
			if (fr_lock->show)
				LCDPR("fatal divider 0\n");
			goto fr_lock_exit;
		}

		fr_lock_recovery_freq(pdrv);
		lcd_venc_adj_vtotal(pdrv, fr_lock->base_vtotal);

		fr_lock->rst = 0;
		fr_lock->start_time = msr_time_ns;
		fr_lock->msr_cur_ns = fr_lock->start_time;
		fr_lock->msr_last_ns = fr_lock->msr_cur_ns;
		fr_lock->msr_sum_cnt_m = 0;
		fr_lock->msr_sum_ns = 0;
		fr_lock->frame_cnt = 0;

		fr_lock->msr_sum_cnt_m = 0;
		fr_lock->msr_clk = vout_measure_freq();
		fr_lock->err_sum_cnt_max = fr_lock->exp_vs_cnt;

		fr_lock->exp_ns_temp = 1000000000 * (s64)fr_lock->base_dura_den;
		fr_lock->exp_vs_ns =
				(int)lcd_s64_div(fr_lock->exp_ns_temp, (int)fr_lock->base_dura_num);
		fr_lock->err_sum_ns_max = fr_lock->exp_vs_ns;
		fr_lock->exp_sum_ns = 0;
		fr_lock->exp_sum_cnt_m = 0;
		fr_lock->exp_line_us =
				(int)lcd_s64_div(fr_lock->exp_ns_temp, fr_lock->base_dura_num);
		fr_lock->exp_line_us = fr_lock->exp_line_us / fr_lock->base_vtotal / 1000;

		reset_slide_filter(&fr_lock->ft_cnt);
		reset_slide_filter(&fr_lock->ft_time);
		fr_lock->exp_line_cnt = fr_lock->exp_vs_cnt / pt->v_period; //
		fr_lock->exp_line_freq = lcd_do_div(fr_lock->pll_base_hz, fr_lock->base_vtotal);

		fr_lock->err_sum = 0;
		if (fr_lock->show)
			LCDPR("exp_vs_ns:%d, exp_vs_cnt:%d, exp_line_us:%d, exp_line_cnt:%d",
			fr_lock->exp_vs_ns, fr_lock->exp_vs_cnt, fr_lock->exp_line_us,
			fr_lock->exp_line_cnt);

		fr_lock->step = 6;
		goto fr_lock_exit;
	}

	//if (fr_lock->step == 7) {
	//	fr_lock->start_time = (s64)sched_clock();
	//	fr_lock->msr_cur_ns = fr_lock->start_time;
	//	fr_lock->msr_last_ns = fr_lock->msr_cur_ns;
	//	fr_lock->msr_sum_cnt_m = 0;
	//	fr_lock->step = 8;
	//	goto fr_lock_exit;
	//}
/*===measure=====================================================================================*/
	temp_cnt = fr_lock->frame_cnt + 1;
	if (fr_lock->mode == 0 || fr_lock->mode == 1 || fr_lock->dbg == 2) {
		fr_lock->msr_last_ns = fr_lock->msr_cur_ns;
		fr_lock->msr_cur_ns = msr_time_ns;
		fr_lock->msr_vs_ns = (int)(fr_lock->msr_cur_ns - fr_lock->msr_last_ns);

		if (fr_lock->msr_vs_ns > fr_lock->exp_vs_ns * 3 >> 1) {
			if (fr_lock->show)
				LCDPR("msr_vs_ns:%d exp_vs_ns:%d, drop frame\n",
					fr_lock->msr_vs_ns, fr_lock->exp_vs_ns);
			goto fr_lock_exit; //too much time, maybe lost sync, we drop this frame
		}

		temp = fr_lock->exp_ns_temp * temp_cnt;
		fr_lock->msr_sum_ns += fr_lock->msr_vs_ns;
		fr_lock->exp_sum_ns = lcd_s64_div(temp, fr_lock->base_dura_num);
		fr_lock->err_sum_ns = (int)(fr_lock->msr_sum_ns - fr_lock->exp_sum_ns);
		fr_lock->err_sum_us = fr_lock->err_sum_ns / 1000;
		fr_lock->err_sum_us_ft =
				slide_filter(&fr_lock->ft_time, fr_lock->err_sum_ns) / 1000;

		if (fr_lock->err_sum_ns > fr_lock->err_sum_ns_max ||
		    fr_lock->err_sum_ns < -fr_lock->err_sum_ns_max) {
			fr_lock->rst = 1;
			if (fr_lock->show)
				LCDPR("err_sum_ns:%d, out of range:%d, rst\n",
					fr_lock->err_sum_ns, fr_lock->err_sum_ns_max);
			goto fr_lock_exit;
		}
	}

	if (fr_lock->mode == 2 || fr_lock->mode == 3 || fr_lock->dbg == 2) {
		fr_lock->msr_vs_cnt = msr_cnt;
		//never be a negative and won't exceed 31 bits, left shift is ok
		fr_lock->msr_sum_cnt_m += (fr_lock->msr_vs_cnt << FRL_CNT_NULTI_BITS);
		fr_lock->exp_sum_cnt_m = temp_cnt * fr_lock->exp_vs_cnt_m; // multi
		fr_lock->err_sum_cnt = (int)(fr_lock->msr_sum_cnt_m - fr_lock->exp_sum_cnt_m);
		// maybe negative number shift, but its ok if only right shift
		fr_lock->err_sum_cnt_ft = slide_filter(&fr_lock->ft_cnt, fr_lock->err_sum_cnt);
		fr_lock->err_sum_cnt_ft >>= FRL_CNT_NULTI_BITS;
		fr_lock->err_sum_cnt >>= FRL_CNT_NULTI_BITS;
		if (fr_lock->show == 3)
			LCDPR("msr_cnt:%d, msr_sum_cnt:%lld, exp_sum_cnt:%lld, err_sum_cnt:%d",
			fr_lock->msr_vs_cnt, fr_lock->msr_sum_cnt_m >> FRL_CNT_NULTI_BITS,
			fr_lock->exp_sum_cnt_m >> FRL_CNT_NULTI_BITS, fr_lock->err_sum_cnt);

		if (fr_lock->err_sum_cnt > fr_lock->err_sum_cnt_max ||
		    fr_lock->err_sum_cnt < -fr_lock->err_sum_cnt_max) {
			fr_lock->rst = 1;
			if (fr_lock->show)
				LCDPR("err_sum_cnt:%d, out of range:%d, rst\n",
					fr_lock->err_sum_cnt, fr_lock->err_sum_cnt_max);
			goto fr_lock_exit;
		}
	}
	fr_lock->frame_cnt++;
/*===control=====================================================================================*/
	switch (fr_lock->mode) {
	case 0: // error time to adjust freq
		err = fr_lock->err_sum_us_ft; //ns to us
		fr_lock->err_sum += err;
		fr_lock->out = fr_lock->kp * err +
			       fr_lock->ki * fr_lock->err_sum / 100 + //do not use ki
			       fr_lock->kd * (err - fr_lock->err_last);
		fr_lock->out = lcd_s32_constraint(fr_lock->out,
				-fr_lock->freq_limit, fr_lock->freq_limit);
		fr_lock->pll_adj_hz = fr_lock->pll_base_hz + fr_lock->out;
		fr_lock->err_last = err; //ns to us

		if (fr_lock->en)
			fr_lock_update_freq(pdrv);

		if (fr_lock->show == 2)
			LCDPR("%d, e_us-ft:%05d %05d F:%lld adj:%06d e_cnt-ft:%06d %06d\n",
				fr_lock->frame_cnt, fr_lock->err_sum_us, fr_lock->err_sum_us_ft,
				fr_lock->pll_adj_hz, fr_lock->out, fr_lock->err_sum_cnt,
				fr_lock->err_sum_cnt_ft);
		break;
	case 1: //error time to adjust vtotal
		if (fr_lock->exp_line_us == 0) {
			fr_lock->step = 0;
			LCDPR("fatal error exp_line_us 0\n");
			goto fr_lock_exit;
		}

		err = fr_lock->err_sum_us; //ns to us
		fr_lock->out = fr_lock->kp * -err / fr_lock->exp_line_us / 100;
		fr_lock->out = lcd_s32_constraint(fr_lock->out,
				-fr_lock->line_limit, fr_lock->line_limit);
		fr_lock->adj_vtotal = fr_lock->base_vtotal + fr_lock->out;

		if (fr_lock->en)
			lcd_venc_adj_vtotal(pdrv, fr_lock->adj_vtotal);

		if (fr_lock->show == 2)
			LCDPR("%d, err_us-ft:%05d %05d vt:%d adj:%02d err_cnt-ft:%06d %06d\n",
				fr_lock->frame_cnt, fr_lock->err_sum_us, fr_lock->err_sum_us_ft,
				fr_lock->adj_vtotal, fr_lock->out, fr_lock->err_sum_cnt,
				fr_lock->err_sum_cnt_ft);
		break;
	case 2: //vs count error  to adjust freq
		if (fr_lock->exp_line_cnt == 0) {
			fr_lock->step = 0;
			LCDPR("fatal error exp_line_cnt 0\n");
			goto fr_lock_exit;
		}
		err = fr_lock->err_sum_cnt;
		fr_lock->out = fr_lock->kp * err * fr_lock->exp_line_freq /
			       fr_lock->exp_line_cnt / 100;
		fr_lock->out = lcd_s32_constraint(fr_lock->out,
				-fr_lock->freq_limit, fr_lock->freq_limit);
		fr_lock->pll_adj_hz = fr_lock->pll_base_hz + fr_lock->out;

		if (fr_lock->en)
			fr_lock_update_freq(pdrv);

		if (fr_lock->show == 2)
			LCDPR("%d, e_cnt-ft:%07d %07d freq:%lld adj:%06d e_us-ft:%06d %06d\n",
				fr_lock->frame_cnt, fr_lock->err_sum_cnt, fr_lock->err_sum_cnt_ft,
				fr_lock->pll_adj_hz, fr_lock->out, fr_lock->err_sum_us,
				fr_lock->err_sum_us_ft);
		break;
	case 3: //vs count error to adjust vtotal
		if (fr_lock->exp_line_cnt == 0) {
			fr_lock->step = 0;
			LCDPR("fatal error exp_line_cnt 0\n");
			goto fr_lock_exit;
		}

		err = fr_lock->err_sum_cnt;
		fr_lock->out = fr_lock->kp * -err / fr_lock->exp_line_cnt / 100;
		fr_lock->out = lcd_s32_constraint(fr_lock->out,
				-fr_lock->line_limit, fr_lock->line_limit);
		fr_lock->adj_vtotal = fr_lock->base_vtotal + fr_lock->out;

		if (fr_lock->en)
			lcd_venc_adj_vtotal(pdrv, fr_lock->adj_vtotal);

		if (fr_lock->show == 2)
			LCDPR("%d, err_cnt-ft:%07d %07d vt:%d adj:%02d err_us-ft:%06d %06d\n",
				fr_lock->frame_cnt, fr_lock->err_sum_cnt, fr_lock->err_sum_cnt_ft,
				fr_lock->adj_vtotal, fr_lock->out, fr_lock->err_sum_us,
				fr_lock->err_sum_us_ft);
		break;
	default:
		break;
	}

fr_lock_exit:
	return;
}
