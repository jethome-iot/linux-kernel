// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/di_multi/dd_rotation.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/seq_file.h>

#include <linux/amlogic/media/vfm/vframe.h>
#include "register.h"
#include "register_nr4.h"
#include "deinterlace.h"
#include "deinterlace_dbg.h"
#include "di_data_l.h"
#include "deinterlace_hw.h"
#include "di_hw_v3.h"
#include "di_dbg.h"
#include "di_pps.h"
#include "di_pre.h"
#include "di_prc.h"
#include "di_task.h"
#include "di_vframe.h"
#include "di_que.h"
#include "di_api.h"
#include "di_sys.h"

//------------------------------------------------------
/* di_interface */
void cfg_ch_set_for_rotation(struct di_ch_s *pch)
{
	unsigned int out_format;

	if (pch->itf.u.dinst.parm.work_mode != WORK_MODE_ROTATION)
		return;
	cfgsch(pch, KEEP_DEC_VF, 0); // for all new_interface
	out_format = pch->itf.u.dinst.parm.output_format &
		DIM_OUT_FORMAT_FIX_MASK;
	/* out: fix afbce nv21 10 bit? */
	cfgsch(pch, POUT_FMT, 3);

	//PR_INF("%s:output_format:0x%x\n", __func__, out_format);
	cfgsch(pch, IOUT_FMT, 1);
	cfgsch(pch, ALLOC_SCT, 0x03);
	cfgsch(pch, DAT, 1);
	cfgsch(pch, 4K, 1);
	cfgsch(pch, HF, 0);
	cfgsch(pch, DCT, 0);
	cfgsch(pch, HDR_EN, 0);
	cfgsch(pch, DW_EN, 0);
	cfgsch(pch, EN_PRE_LINK, 0);
	cfgsch(pch, TB, 0);
	cfgsch(pch, EN_POST_LINK, 0);
	pch->ponly = 1; //
}

static enum DI_ERRORTYPE _in_empty(struct di_ch_s *pch,
					   struct di_buffer *buffer)
{
	unsigned int ch;
	unsigned int index;
	unsigned int free_nub;
	struct buf_que_s *pbufq;
	struct dim_nins_s	*pins;
	bool flg_q;

	ch = pch->ch_id;
	pbufq = &pch->nin_qb;
	free_nub	= qbufp_count(pbufq, QBF_NINS_Q_IDLE);
	if (!free_nub) {
		PR_WARN("%s:no nins idle\n", __func__);
		return DI_ERR_IN_NO_SPACE;
	}

	/* get ins */
	flg_q = qbuf_out(pbufq, QBF_NINS_Q_IDLE, &index);
	if (!flg_q) {
		PR_ERR("%s:qout\n", __func__);
		return DI_ERR_IN_NO_SPACE;
	}
	pins = (struct dim_nins_s *)pbufq->pbuf[index].qbc;
	pins->c.ori = buffer;

	pins->c.cnt = pch->in_cnt;
	pch->in_cnt++;

	flg_q = qbuf_in(pbufq, QBF_NINS_Q_DCT, index);
	dbg_copy("%s:%d\n", __func__, pins->c.cnt);
	sum_g_inc(ch);
	if (!flg_q) {
		PR_ERR("%s:qin check\n", __func__);
		qbuf_in(pbufq, QBF_NINS_Q_IDLE, index);
	}

	if (pch->in_cnt < 4) {
		if (pch->in_cnt == 1)
			dbg_timer(ch, EDBG_TIMER_1_GET);
		else if (pch->in_cnt == 2)
			dbg_timer(ch, EDBG_TIMER_2_GET);
		else if (pch->in_cnt == 3)
			dbg_timer(ch, EDBG_TIMER_3_GET);
	}
	return DI_ERR_NONE;
}

enum DI_ERRORTYPE rt_empty_input(struct di_ch_s *pch,
				   struct di_buffer *buffer)
{
	enum DI_ERRORTYPE ret;

	mutex_lock(&pch->itf.lock_in);
	ret = _in_empty(pch, buffer);
	mutex_unlock(&pch->itf.lock_in);

	if (ret == DI_ERR_NONE)
		task_send_ready(20);
	return ret;
}

static int _buf_simple_init(struct di_ch_s *pch)
{
	int i;

	struct vframe_s **pvframe_in;
	struct vframe_s *pvframe_in_dup;
	struct vframe_s *pvframe_local;
	struct vframe_s *pvframe_post;
	struct di_buf_s *pbuf_local;
	struct di_buf_s *pbuf_in;
	struct di_buf_s *pbuf_post;
	struct di_buf_s *di_buf;

	struct div2_mm_s *mm;
	unsigned int cnt;

	unsigned int ch;
	//unsigned int length;

	/**********************************************/
	/* count buf info */
	/**********************************************/
	//if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
	//	canvas_align_width = 64;

	ch = pch->ch_id;
	pvframe_in	= get_vframe_in(ch);
	pvframe_in_dup	= get_vframe_in_dup(ch);
	pvframe_local	= get_vframe_local(ch);
	pvframe_post	= get_vframe_post(ch);
	pbuf_local	= get_buf_local(ch);
	pbuf_in		= get_buf_in(ch);
	pbuf_post	= get_buf_post(ch);
	mm		= dim_mm_get(ch);

	for (i = 0; i < MAX_IN_BUF_NUM; i++)
		pvframe_in[i] = NULL;

	/**********************************************/
	/* que init */
	/**********************************************/
	queue_init(ch, MAX_LOCAL_BUF_NUM * 2);
	di_que_init(ch); /*new que*/

	/**********************************************/
	/* local buf init */
	/**********************************************/
	// no need local buf
	for (i = 0; i < (MAX_LOCAL_BUF_NUM * 2); i++) {
		di_buf = &pbuf_local[i];

		memset(di_buf, 0, sizeof(struct di_buf_s));
	}

	/**********************************************/
	/* input buf init */
	/**********************************************/
	for (i = 0; i < MAX_IN_BUF_NUM; i++) {
		di_buf = &pbuf_in[i];

		if (di_buf) {
			memset(di_buf, 0, sizeof(struct di_buf_s));
			di_buf->type = VFRAME_TYPE_IN;
			di_buf->pre_ref_count = 0;
			di_buf->post_ref_count = 0;
			di_buf->vframe = &pvframe_in_dup[i];
			di_buf->vframe->private_data = di_buf;
			di_buf->index = i;
			di_buf->queue_index = -1;
			di_buf->invert_top_bot_flag = 0;
			di_buf->channel = ch;

			di_que_in(ch, QUE_IN_FREE, di_buf);
		}
	}
	/**********************************************/
	/* post buf init */
	/**********************************************/
	cnt = 0;
	for (i = 0; i < MAX_POST_BUF_NUM; i++) {
		di_buf = &pbuf_post[i];

		if (di_buf) {
			memset(di_buf, 0, sizeof(struct di_buf_s));

			di_buf->type = VFRAME_TYPE_POST;
			di_buf->index = i;
			di_buf->vframe = &pvframe_post[i];
			di_buf->vframe->private_data = di_buf;
			di_buf->queue_index = -1;
			di_buf->invert_top_bot_flag = 0;
			di_buf->channel = ch;
			di_buf->flg_null = 1;

			if (dimp_get(edi_mp_post_wr_en) &&
			    dimp_get(edi_mp_post_wr_support)) {
				di_buf->canvas_width[NR_CANVAS] =
					mm->cfg.pst_cvs_w;
				//di_buf->canvas_rot_w = rot_width;
				di_buf->canvas_height = mm->cfg.pst_cvs_h;
				di_buf->canvas_config_flag = 1;

				dbg_init("[%d]post buf:%d: addr=0x%lx\n", i,
					 di_buf->index, di_buf->nr_adr);
			}
			cnt++;
			//di_que_in(channel, QUE_POST_FREE, di_buf);
			di_que_in(ch, QUE_PST_NO_BUF, di_buf);

		} else {
			PR_ERR("%s:%d:post buf is null\n", __func__, i);
		}
	}

	return 0;
}

static int _buf_init(struct di_ch_s *pch)
{
	struct mtsk_cmd_s blk_cmd;

	struct div2_mm_s *mm;
	unsigned int ch;
	unsigned int length_keep;
	unsigned int post_nub;

	ch = pch->ch_id;

	mm = dim_mm_get(ch);
	memset(&mm->cfg, 0, sizeof(mm->cfg));

	if (!dip_is_support_4k(ch)) {
		mm->cfg.di_w = 1920;
		mm->cfg.di_h = 1088;

	} else {
		mm->cfg.di_w = 3840;
		mm->cfg.di_h = 2160;
	}
	post_nub = cfggch(pch, POST_NUB);
	if (post_nub && post_nub <= POST_BUF_NUM)
		mm->cfg.num_post = post_nub;
	pch->mode = EDPST_MODE_422_10BIT_PACK;//dim_cnt_mode(pch);

	di_cnt_pst_afbct(pch);
	di_cnt_post_buf(pch);
	_buf_simple_init(pch);

	length_keep = ndis_cnt(pch, QBF_NDIS_Q_KEEP);
	//di_que_list_count(ch, QUE_POST_KEEP);
	mm->cfg.num_rebuild_alloc = 0;

	if (cfgeq(MEM_FLAG, EDI_MEM_M_CMA)	||
		cfgeq(MEM_FLAG, EDI_MEM_M_CODEC_A)	||
		cfgeq(MEM_FLAG, EDI_MEM_M_CODEC_B)) {
		/*trig cma alloc*/
		dbg_timer(ch, EDBG_TIMER_ALLOC);

		//pch->sumx.vfm_bypass = false;
		blk_cmd.cmd = ECMD_BLK_ALLOC;
		blk_cmd.hf_need = 0;
		if (mm->cfg.num_post  && mm->cfg.pbuf_flg.b.page) {
			//blk_cmd.cmd = ECMD_BLK_ALLOC;
			blk_cmd.nub = mm->cfg.num_post - length_keep;
			blk_cmd.flg.d32 = mm->cfg.pbuf_flg.d32;
			//mtask_send_cmd(ch, &blk_cmd);
			mtsk_alloc_block2(ch, &blk_cmd);
		} else {
			PR_INF("%s:nub=%d,page_size=0x%x\n", __func__,
				mm->cfg.num_post, mm->cfg.pbuf_flg.b.page);
		}

		/* post */
		if (length_keep > 8)
			PR_ERR("%s:keep nub:%d\n", __func__, length_keep);

		mm->sts.flg_alloced = true;
		mm->cfg.num_rebuild_keep = 0;
	} else {
		mm->sts.flg_alloced = true;
		mm->cfg.num_rebuild_keep = 0;
	}
	return 0;
}

enum RT_BYPASS_ID {
	RT_BYPASS_ALL_BYPASS = 1,
	RT_BYPASS_CH,
	RT_BYPASS_NOT_AFBCD,
	RT_BYPASS_I,
	RT_BYPASS_TYP,
	RT_BYPASS_SRC_PPMGR,
	RT_BYPASS_SIZE_UP_4K,
	RT_BYPASS_SIZE_DOWN,
	RT_BYPASS_EOS,
};

static unsigned int _bypasse_checkvf(struct vframe_s *vf)
{
	unsigned int x, y;

	if (!vf)
		return 0;
	if (!IS_COMP_MODE(vf->type))
		return RT_BYPASS_NOT_AFBCD;
	if (VFMT_IS_I(vf->type))
		return RT_BYPASS_I;
	if (vf->type & DIM_BYPASS_VF_TYPE)
		return RT_BYPASS_TYP;
	if (vf->type & VIDTYPE_V4L_EOS)
		return RT_BYPASS_EOS;
	if (vf->source_type == VFRAME_SOURCE_TYPE_PPMGR)
		return RT_BYPASS_SRC_PPMGR;

	dim_vf_x_y(vf, &x, &y);
	if (x > 3840 || y > 2160)
		return RT_BYPASS_SIZE_UP_4K;

	if (x < 128 || y < 16)
		return RT_BYPASS_SIZE_DOWN;

	return 0;
}

static u32 _bypass_checkother(struct di_ch_s *pch)
{
	if (dimp_get(edi_mp_di_debug_flag) & 0x100000)
		return RT_BYPASS_ALL_BYPASS;
	/* EDI_CFGX_BYPASS_ALL */
	di_cfgx_set(pch->ch_id,
		    EDI_CFGX_BYPASS_ALL,
		    pch->itf.bypass_ext, DI_BIT4);
	if (di_cfgx_get(pch->ch_id, EDI_CFGX_BYPASS_ALL))
		return RT_BYPASS_CH;

	return 0;
}

static u32 _is_bypass(struct di_ch_s *pch, struct vframe_s *vf)
{
	u32 ret = 0;

	ret = _bypass_checkother(pch);
	if (!ret && vf)
		ret = _bypasse_checkvf(vf);
	//debug only
	if (pch->c.last_bypass != ret) {
		PR_INF("%s:ch[%d]:%d->%d\n", __func__,
		       pch->ch_id, pch->c.last_bypass, ret);
		pch->c.last_bypass = ret;
	}
	return ret;
}

static bool _bypass_2_ready_bynins(struct di_ch_s *pch,
				       struct dim_nins_s *nins)
{
	void *in_ori;
	struct di_buffer *buffer = NULL;

	if (!nins) {
		PR_ERR("%s:no in ori ?\n", __func__);
		return true;
	}
	in_ori = nins->c.ori;

	/* recycle */
	nins->c.ori = NULL;
	nins_used_some_to_recycle(pch, nins);

	/* to ready buffer */
	if (dip_itf_is_ins(pch)) {
		/* need mark bypass flg*/
		buffer = (struct di_buffer *)in_ori;
		buffer->flag |= DI_FLAG_BUF_BY_PASS;
	}

	if (dip_itf_is_ins_exbuf(pch))
		PR_ERR("%s:use out buffer ?\n", __func__);
	else
		ndrd_qin(pch, in_ori);

	return true;
}

static void _vfm_check(struct di_ch_s *pch, struct vframe_s *vfm_in)
{
	struct di_vinfo_s *vinf_l;
	struct di_vinfo_s *vinf_c;

	vinf_l = &pch->c.vinf_lst;
	vinf_c = &pch->c.vinf_crr;

	memcpy(vinf_l, vinf_c, sizeof(*vinf_l));
	dim_vfm_2_vinfo(vinf_c, vfm_in);
	dim_is_vinfo_chg(vinf_c, vinf_l, true);
}

/* in -> check */
void rt_parser_infor(struct di_ch_s *pch)
{
	unsigned int index;
	unsigned int in_nub;
	struct buf_que_s *pbufq;
	struct dim_nins_s	*pins;
	bool flg_q;
	int i;
	struct vframe_s *vfm = NULL;
	struct di_buffer *buffer = NULL;
	u32 bypass_ret;

	if (!pch || !pch->itf.flg_rotation)
		return;
	pbufq = &pch->nin_qb;
	in_nub	= qbufp_count(pbufq, QBF_NINS_Q_DCT);
	if (!in_nub) {
		//PR_WARN("%s:no nins in\n", __func__);
		return;
	}
	for (i = 0; i < in_nub; i++) {
		/* get ins */
		flg_q = qbuf_out(pbufq, QBF_NINS_Q_DCT, &index);
		if (!flg_q) {
			PR_ERR("%s:qout\n", __func__);
			return;
		}
		pins = (struct dim_nins_s *)pbufq->pbuf[index].qbc;
		buffer = (struct di_buffer *)pins->c.ori;
		if (!buffer->vf) {
			pins->c.wmode.is_bypass = 1;
			dbg_copy("set1:b:%d\n", pins->c.cnt);
			PR_INF("%s:no vfm\n", __func__);
		} else {
			vfm = buffer->vf;
			_vfm_check(pch, vfm);
			bypass_ret = _is_bypass(pch, vfm);
			if (bypass_ret) {
				pins->c.wmode.is_bypass = 1;
				dbg_copy("set2:b:%d\n", pins->c.cnt);
			} else {
				dbg_copy("set3:no bypass:%d\n", pins->c.cnt);
				memcpy(&pins->c.vfm_cp, vfm, sizeof(pins->c.vfm_cp));
			}
		}

		flg_q = qbuf_in(pbufq, QBF_NINS_Q_CHECK, index);
		if (!flg_q) {
			PR_ERR("%s:qin check:%d:%d\n", __func__, in_nub, i);
			return;
		}
	}
}

static bool _pre_sw2bypass(struct di_ch_s *pch, unsigned int bypassid)
{
	struct di_pre_stru_s *ppre;
	unsigned int ch;
	bool ret = false;

	ch = pch->ch_id;
	ppre = get_pre_stru(ch);

	dim_bypass_set(pch, 1, bypassid);
	if (di_bypass_state_get(ch) == 0) {
		//cnt_rebuild = 0; /*from no bypass to bypass*/
		ppre->is_bypass_all = true;
		bset(&pch->self_trig_mask, 29);
		/*********************************/
		di_bypass_state_set(ch, true);/*bypass_state:1;*/
		pch->sumx.need_local = false;
		PR_INF("%s:to  bypass\n", __func__);
		ret = true;
	}
	return ret;
}

/*************************************************
 * return
 *	< 0: do not need do next;
 *	> 0: need di process;
 *	-1: no input;
 *	-2: have done;
 *	1 : not in bypass mode;
 *	2 : have input but not bypass.
 **************************************************/
static int _pre_bypass(struct di_ch_s *pch)
{
	struct dim_nins_s *nins = NULL;
	unsigned int ch, nub, bypass_nub;
	int i, flg = -1, err = 0;

	ch = pch->ch_id;

	if (di_bypass_state_get(ch) == 0) {
		nins = nins_peek_pre(pch);
		if (!nins) {
			//dbg_copy("%s:no:input\n", __func__);
			return -1; // no input
		}
		if (nins->c.wmode.is_bypass) {
			//switch state:
			dbg_copy("%s:in:bypass\n", __func__);
			_pre_sw2bypass(pch, nins->c.wmode.bypass_reason);
		} else {
			dbg_copy("%s:not bypass:%d\n", __func__, nins->c.cnt);
		}
	}

	if (di_bypass_state_get(ch) == 0)
		return 1;

	nub = nins_cnt(pch, QBF_NINS_Q_CHECK);

	if (nub <= 0)
		return -1;
	bypass_nub = 0;
	for (i = 0; i < nub; i++) {
		nins = nins_peek_pre(pch);
		if (!nins) {
			err = 2;
			break;
		}
		if (!nins->c.wmode.is_bypass) {
			flg = 2; //have input but not bypass.
			break;
		}
		//bypass:
		nins = nins_get(pch);
		if (!nins) {
			err = 1;
			PR_ERR("%s:peek but can't get\n", __func__);
			break;
		}

		if (pch->rsc_bypass.b.ponly_fst_cnt) {
			pch->rsc_bypass.b.ponly_fst_cnt--;
			//PR_INF("%s:%d\n", __func__, pch->rsc_bypass.b.ponly_fst_cnt);
		}
		_bypass_2_ready_bynins(pch, nins);
		bypass_nub++;
	}
	if (err) {
		PR_INF("%s:err:%d;%d:%d\n", __func__, err, nub, i);
		return -1;//no input;
	}
	if (nub)
		dbg_copy("%s:%d,%d\n", __func__, nub, bypass_nub);
	return flg;
}

/********************************************
 * s4dw_pre_cfg_bypass
 *	part of pre config;
 * when return > 0 date to ready, and state to bypass
 * when return 0, state to normal.
 * ary: need check ,called in pre_config
 ********************************************/
static unsigned char _pre_cfg_bypass(struct di_ch_s *pch, struct dim_nins_s *nins)
{
	unsigned int ch, bypassr;

	ch = pch->ch_id;

	bypassr = _bypass_checkother(pch);
	if (!bypassr) {
		if (nins->c.wmode.is_bypass || nins->c.wmode.need_bypass) {
			bypassr = nins->c.wmode.bypass_reason;
		} else {
			//not bypass:
			return 0;
		}
	}

	_pre_sw2bypass(pch, bypassr);

	if (pch->rsc_bypass.b.ponly_fst_cnt) {
		pch->rsc_bypass.b.ponly_fst_cnt--;
		//PR_INF("%s:%d\n", __func__, pch->rsc_bypass.b.ponly_fst_cnt);
	}
	_bypass_2_ready_bynins(pch, nins);

	return (unsigned char)bypassr;
}

static unsigned int dim_dbg_o_vtype;
module_param_named(dim_dbg_o_vtype, dim_dbg_o_vtype, uint, 0664);
static unsigned int dim_dbg_o_b;
module_param_named(dim_dbg_o_b, dim_dbg_o_b, uint, 0664);

/* for afbcd out put, need check */
static void _dimpst_fill_outvf(struct di_ch_s *pch,
					struct vframe_s *vfm_t,
					struct vframe_s *vfm_f,
				    struct di_buf_s *di_buf,
				    enum EDPST_OUT_MODE mode)
{
	unsigned int tmp;

	if (IS_COMP_MODE(vfm_f->type)) {
		vfm_f->width = vfm_f->compWidth;
		vfm_f->height = vfm_f->compHeight;
	}
	//check if 4k:
	if (vfm_f->width > 1920 || vfm_f->height > 1088)
		di_buf->is_4k = 1;
	else
		di_buf->is_4k = 0;

	if (rotation_test_420_10())
		di_buf->afbce_out_yuv420_10 = 1;
	else
		di_buf->afbce_out_yuv420_10 = 0;

	memcpy(vfm_t, vfm_f, sizeof(*vfm_t));
	di_buf->rot = pch->itf.rotation_mode & 0x3;

	vfm_t->private_data = di_buf;
	//
	vfm_t->early_process_fun = dim_do_post_wr_fun;
	vfm_t->process_fun = NULL;
	vfm_t->mem_handle = NULL;
	vfm_t->type |= VIDTYPE_DI_PW;

	vfm_t->flag &=
		~(VFRAME_FLAG_DI_PW_VFM |
		  VFRAME_FLAG_DI_PW_N_LOCAL |
		  VFRAME_FLAG_DI_PW_N_EXT);
	if (dip_itf_is_vfm(pch))
		vfm_t->flag |= VFRAME_FLAG_DI_PW_VFM;
	else if (dip_itf_is_ins_lbuf(pch))
		vfm_t->flag |= VFRAME_FLAG_DI_PW_N_LOCAL;
	else
		vfm_t->flag |= VFRAME_FLAG_DI_PW_N_EXT;

	if (!dip_itf_is_o_linear(pch))
		vfm_t->flag &= (~VFRAME_FLAG_VIDEO_LINEAR);

	if (di_buf->rot == 2 || di_buf->rot == 3) {
		tmp = vfm_t->compHeight;
		vfm_t->compHeight = vfm_t->compWidth;
		vfm_t->compWidth =  tmp;
	}

	/*clear*/
	vfm_t->type &= ~(VIDTYPE_VIU_NV12 |
			   VIDTYPE_VIU_444	|
			   VIDTYPE_VIU_NV21 |
			   VIDTYPE_VIU_422	|
			   VIDTYPE_VIU_SINGLE_PLANE |
			   VIDTYPE_COMPRESS |
			   VIDTYPE_PRE_INTERLACE | 0x0f);

	/* bit clear */
	vfm_t->bitdepth &= ~(BITDEPTH_MASK);
	vfm_t->bitdepth &= ~(FULL_PACK_422_MODE);

	/* type */
	vfm_t->type |= (VIDTYPE_COMPRESS | VIDTYPE_SCATTER);
	vfm_t->type |= VIDTYPE_DI_PW;
	vfm_t->compHeadAddr = di_buf->afbc_adr;
	vfm_t->compBodyAddr = di_buf->nr_adr;

	if (di_buf->afbce_out_yuv420_10) { //note
		vfm_t->bitdepth |= (BITDEPTH_Y10	|
						    BITDEPTH_U10	| BITDEPTH_V10);
		//vfm_t->type &= ~VFMT_COLOR_MSK;
		vfm_t->type |= VIDTYPE_VIU_FIELD;
		vfm_t->type |= VIDTYPE_VIU_SINGLE_PLANE;
	} else if (mode == EDPST_OUT_MODE_NV21 ||
		mode == EDPST_OUT_MODE_NV12) {
		vfm_t->type |= VIDTYPE_VIU_FIELD;

		if (mode == EDPST_OUT_MODE_NV21)
			vfm_t->type |= VIDTYPE_VIU_NV21;
		else
			vfm_t->type |= VIDTYPE_VIU_NV12;

		vfm_t->bitdepth |= (BITDEPTH_Y8	|
				  BITDEPTH_U8	|
				  BITDEPTH_V8);
	} else { // EDPST_OUT_MODE_DEF
		vfm_t->type |= VIDTYPE_VIU_422 | VIDTYPE_VIU_FIELD;
		vfm_t->type |= VIDTYPE_VIU_SINGLE_PLANE;
		vfm_t->bitdepth |= (BITDEPTH_Y10	|
				  BITDEPTH_U10	|
				  BITDEPTH_V10	|
				  FULL_PACK_422_MODE);
	}

	if (cfgg(AFBCE_LOSS_EN) == 1 && DIM_IS_IC(T3X)) {
		vfm_t->vf_lossycomp_param.quant_diff_root_leave = 2;
		vfm_t->vf_lossycomp_param.lossy_mode = 1;
		vfm_t->vf_lossycomp_param.ofset_burst4_en = 0;
	}
	vfm_t->canvas0Addr = -1;
	vfm_t->canvas1Addr = -1;

	if (cfgg(AFBCE_LOSS_EN) == 1 && DIM_IS_IC(T3X)) {
		vfm_t->vf_lossycomp_param.quant_diff_root_leave = 2;
		vfm_t->vf_lossycomp_param.lossy_mode = 1;
		vfm_t->vf_lossycomp_param.ofset_burst4_en = 0;
	}

	dim_print("%s:vfm typ=0x%x, flg=0x%x, bit=0x%x:addr:0x%lx, 0x%lx\n",
		__func__,
		vfm_t->type, vfm_t->flag, vfm_t->bitdepth,
		vfm_t->compHeadAddr,
		vfm_t->compBodyAddr);
	if (dim_dbg_o_vtype)
		vfm_t->type = dim_dbg_o_vtype;

	if (dim_dbg_o_b)
		vfm_t->bitdepth = dim_dbg_o_b;
}

static unsigned char _pre_buf_config(struct di_ch_s *pch)
{
	unsigned int ch;
	struct di_buf_s *di_buf = NULL;
	struct vframe_s *vframe;

	struct dim_nins_s *nins = NULL;
	struct di_post_stru_s *ppst;

	if (!pch) {
		PR_ERR("%s:no pch\n", __func__);
		return 100;
	}
	ch =  pch->ch_id;
	ppst = get_post_stru(ch);
	//ppre = get_pre_stru(ch);

	if (di_que_is_empty(ch, QUE_POST_FREE))
		return 5;

	/* check post buffer */
	di_buf = di_que_peek(ch, QUE_POST_FREE);
//	mm = dim_mm_get(ch);

	/* input get */
	nins = nins_peek_pre(pch);
	if (!nins)
		return 10;
	nins = nins_get(pch);
	if (!nins) {
		PR_ERR("%s:peek but can't get\n", __func__);
		return 14;
	}
	/*vfm check*/
	//_vfm_check(pch, nins);
	/* bypass control */
	if (_pre_cfg_bypass(pch, nins))
		return 101;
	di_buf = di_que_out_to_di_buf(ch, QUE_POST_FREE);
	if (dim_check_di_buf(di_buf, 17, ch)) {
		//di_unlock_irqfiq_restore(irq_flag2);
		return 22;
	}
	/**************************************************/
	vframe = &nins->c.vfm_cp;
	/**/
	_dimpst_fill_outvf(pch, di_buf->vframe, vframe, di_buf, EDPST_OUT_MODE_DEF);
	pch->c.nins = nins;

	dim_afds()->set_rotation(vframe, di_buf->vframe);
	opl1()->pst_set_flow((dimp_get(edi_mp_post_wr_en) &&
				      dimp_get(edi_mp_post_wr_support)),
				     EDI_POST_FLOW_STEP2_START, NULL);
	ppst->cur_post_buf = di_buf;
	return 0;
}

static void _done_config(unsigned int ch, bool flg_timeout)
{
	struct di_ch_s *pch;
	struct di_post_stru_s *ppst = get_post_stru(ch);

	pch = get_chdata(ch);
	pch->itf.op_fill_ready(pch, ppst->cur_post_buf);
	ppst->cur_post_buf = NULL;
	ppst->process_doing = false;
	nins_used_some_to_recycle(pch, pch->c.nins); //need check
	pch->c.nins = NULL;
}

//s4dw pre setting table :
/* use do_table: */
/* bypass or setting */
static unsigned int rt_check(void *data)
{
	struct di_hpst_s *pst = get_hw_pst();
	unsigned int ret = K_DO_R_JUMP(K_DO_TABLE_ID_STOP);
	struct di_ch_s *pch;
	int ret_bypass;

	pch = get_chdata(pst->curr_ch);

	ret_bypass = _pre_bypass(pch);
	dbg_copy("%s:ch[%d]:%d\n", "rt check", pst->curr_ch, ret_bypass);
	if (ret_bypass < 0) {
		ret = K_DO_R_JUMP(K_DO_TABLE_ID_STOP);
	} else {
		ret_bypass = _pre_buf_config(pch);
		dbg_copy("%s:2:%d\n", "rt check", ret_bypass);
		if (!ret_bypass) {
			pst->flg_have_set = true;
			di_tout_contr(EDI_TOUT_CONTR_EN, &pst->tout);
			ret = K_DO_R_FINISH;

		} else {
			ret = K_DO_R_JUMP(K_DO_TABLE_ID_STOP);
		}
	}

	return ret;
}

/*************************************
 * return:
 *	false: don't wait. finish seting and go to start;
 *	true: wait
 *************************************/
static bool _wait_int_process(void)
{
	bool ret = true; //wait
	//-----------
	struct di_hpst_s  *pst = get_hw_pst();

	if (pst->flg_int_done) {
		/*finish to count timer*/
		di_tout_contr(EDI_TOUT_CONTR_FINISH, &pst->tout);

		_done_config(pst->curr_ch, false);
		pst->flg_have_set = false;

		pst->flg_int_done = 0;

		ret = false;

	} else {
		/*check if timeout:*/
		if (di_tout_contr(EDI_TOUT_CONTR_CHECK, &pst->tout)) {
			//timeout:
			PR_WARN("rt: timeout\n");
			di_tout_contr(EDI_TOUT_CONTR_FINISH, &pst->tout);
			_done_config(pst->curr_ch, false);
			pst->flg_have_set = false;
			pst->flg_int_done = 0;
			ret = false;
		} else {	//wait
			ret = true;
		}
	}
	return  ret;
}

/* wait int or done or reset ? */
static unsigned int rt_wait_int(void *data)
{
	bool wait;
	unsigned int ret = K_DO_R_NOT_FINISH;

	wait = _wait_int_process();
	if (wait)
		ret = K_DO_R_NOT_FINISH;
	else
		ret = K_DO_R_JUMP(K_DO_TABLE_ID_STOP);

	return ret;
}

//-----------------------------------------------------
//sub polling:
//for rotation:
enum EDI_RT_ST {
	EDI_RT_ST_CHECK = K_DO_TABLE_ID_START,
	EDI_RT_ST_WAIT_INT,
	EDI_RT_ST_UB,
};

static const struct do_table_ops_s rt_sub_pol_tab[] = {
	/*fix*/
	[K_DO_TABLE_ID_PAUSE] = {
	.id = K_DO_TABLE_ID_PAUSE,
	.mark = 0,
	.con = NULL,
	.do_op = NULL,
	.do_stop_op = NULL,
	.name = "pause",
	},
	[K_DO_TABLE_ID_STOP] = {
	.id = K_DO_TABLE_ID_STOP,
	.mark = 0,
	.con = NULL,
	.do_op = NULL,
	.do_stop_op = NULL,
	.name = "rt_stop",
	},
	/******************/
	[K_DO_TABLE_ID_START] = {	/*EDI_RT_ST_CHECK*/
	.id = K_DO_TABLE_ID_START,
	.mark = 0,
	.con = NULL,
	.do_op = rt_check,
	.do_stop_op = NULL,
	.name = "rt-check",
	},

	[EDI_RT_ST_WAIT_INT] = {
	.id = EDI_RT_ST_WAIT_INT,
	.mark = K_DO_TABLE_IS_WAIT,
	.con = NULL,
	.do_op = rt_wait_int,
	.do_stop_op = NULL,
	.name = "rt_wait_int",
	},

};

/*call in main loop check state */
static void mp_check_sw_rotaion(unsigned int ch)
{
	struct di_ch_s *pch;

	dbg_dt("m_chk_sw[%d]\n", ch);

	pch = get_chdata(ch);
	if (pch->itf.flg_rotation) {
		//tmp: set do_table:
		pol_pst_set_dtb(&rt_sub_pol_tab[0], ARRAY_SIZE(rt_sub_pol_tab));
	}
}

static void mp_stop_done_rotaion(unsigned int ch)
{
	PR_INF("%s:ch[%d]\n", __func__, ch);
}

static bool mp_has_job_rotation(unsigned int ch)
{
	return true;
}

/************************************************/
bool dim_is_pre_disable(unsigned int ch)
{
	struct di_ch_s *pch;

	pch = get_chdata(ch);

	return pch->c.pre_dis;
}

void rt_unreg(struct di_ch_s *pch)
{
	if (!pch->itf.flg_rotation)
		return;

	memset(&pch->c, 0, sizeof(pch->c));
	PR_INF("%s\n", __func__);
}

void rt_reg_variable(struct di_ch_s *pch, struct vframe_s *vframe)
{
	unsigned int ch;
	struct di_pre_stru_s *ppre;
	struct di_post_stru_s *ppost;
	struct di_dev_s *de_devp = get_dim_de_devp();
	struct div2_mm_s *mm;

	if (!pch)
		return;
	ch = pch->ch_id;

	PR_INF("%s:=%d\n", __func__, ch);
	//tmp dim_slt_init();
	memset(&pch->c, 0, sizeof(pch->c));

	pch->c.pst_pol_en = true;
	pch->c.pre_dis = true;
	pch->c.op_mp_check_sw = mp_check_sw_rotaion;
	pch->c.op_mp_has_job = mp_has_job_rotation;
	pch->c.op_mp_stop_done = mp_stop_done_rotaion;
	dim_p_pst_start(ch);
	ppre = get_pre_stru(ch);

	dim_print("%s:0x%p\n", __func__, vframe);
	mm = dim_mm_get(ch);
	/**/
	if (vframe) {
		/* ext dip_init_value_reg(channel, vframe);*/
		/* struct di_post_stru_s */
		ppost = get_post_stru(ch);
		ppost = get_post_stru(ch);
		memset(ppost, 0, sizeof(struct di_post_stru_s));
		ppost->next_canvas_id = 1; //??
		/*pre: struct di_pre_stru_s */
		memset(ppre, 0, sizeof(struct di_pre_stru_s));

		dim_bypass_st_clear(pch);
		if (pch->itf.op_cfg_ch_set)
			pch->itf.op_cfg_ch_set(pch);

		pch->en_hf	= false;
		pch->en_hf_buf	= false;

		pch->en_dw_mem = false;
		pch->en_dw = false;

		/*******************************************/
		dim_ddbg_mod_save(EDI_DBG_MOD_RVB, ch, 0);

		ppre->bypass_flag = false;

		de_devp->h_sc_down_en = 0;
		if (dimp_get(edi_mp_di_printk_flag) & 2)
			dimp_set(edi_mp_di_printk_flag, 1);

		/*need before set buffer*/
		if (dim_afds())
			dim_afds()->reg_val(pch);

		_buf_init(pch);

		//pre_sec_alloc(pch, mm->cfg.dat_idat_flg.d32);
		if (mm->cfg.pbuf_flg.b.typ == EDIM_BLK_TYP_PSCT) {
			if (mm->cfg.num_post)
				sct_sw_on(pch,
					mm->cfg.num_post,
					mm->cfg.pbuf_flg.b.tvp,
					mm->cfg.pst_buf_size);
			else
				PR_WARN("post is 0, reg no alloc\n");
		}
		pst_sec_alloc(pch, mm->cfg.dat_pafbct_flg.d32);//temp
		ppre->mtn_status =	NULL;

		di_sum_reg_init(ch); //check

		set_init_flag(ch, true);/*init_flag = 1;*/

		dim_ddbg_mod_save(EDI_DBG_MOD_RVE, ch, 0);
	}
}

