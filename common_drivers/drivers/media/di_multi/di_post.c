// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/di_multi/di_post.c
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
#include "deinterlace.h"
#include "deinterlace_dbg.h"

#include "di_data_l.h"
#include "di_data.h"
#include "di_dbg.h"
#include "di_vframe.h"
#include "di_que.h"
#include "di_task.h"

#include "di_prc.h"
#include "di_post.h"

#include "nr_downscale.h"
#include "register.h"

bool dpst_can_exit(unsigned int ch)
{
	//struct di_hpst_s  *pst = get_hw_pst();
	bool ret = false;
	struct di_ch_s *pch;

	pch = get_chdata(ch);

	if (pch->c.st <= POL_M_ST_IDLE) //EDI_PST_ST_IDLE
		ret = true;

	if (!ret)
		PR_INF("%s:ch[%d]:stat[%d] ret[%d]\n",
		       __func__,
		       ch, pch->c.st,
		       ret);
	return ret;
}

//----------------------------------------------
//------------------function----------------
//this is pst normal table
enum EDI_NM_ST {
	EDI_NM_ST_CHECK = K_DO_TABLE_ID_START,
	EDI_NM_ST_SET,
	EDI_NM_ST_WAIT_INT,
	EDI_NM_ST_TIMEOUT,
	EDI_NM_ST_UB,
};

/* check:
 * cfg is bypass or not,
 *	bypass: K_DO_R_JUMP(K_DO_TABLE_ID_STOP);
 * normal: K_DO_R_FINISH
 */
static unsigned int nm_check(void *data)
{

	struct di_hpst_s  *pst = get_hw_pst();
	unsigned int ch;
	struct di_post_stru_s *ppost;
//	bool reflesh = false;

	ch = pst->curr_ch;
	ppost = get_post_stru(ch);

	if (di_que_is_empty(ch, QUE_POST_DOING)) {
	//dbg only if (!nm_trig_is_havebuffer(ch)) {
		ppost->post_peek_underflow++;
		return K_DO_R_JUMP(K_DO_TABLE_ID_STOP);
	}
	return K_DO_R_FINISH;
}

//true: set ok; failed: need go to stop
bool _dpst_step_set(unsigned int ch)
{
	struct di_buf_s *di_buf = NULL;
	struct vframe_s *vf_p = NULL;
	struct di_post_stru_s *ppost;
	struct di_hpst_s  *pst = get_hw_pst();
	struct di_ch_s *pch;

	ppost = get_post_stru(ch);

	di_buf = di_que_peek(ch, QUE_POST_DOING);
	if (dim_check_di_buf(di_buf, 20, ch)) {
		PR_ERR("%s:err1\n", __func__);
		return false;
	}

	vf_p = di_buf->vframe;

	pch = get_chdata(ch);
	dim_print("%s:pr_index=%d\n", __func__, di_buf->process_fun_index);
	if (pch->link_mode == EPVPP_API_MODE_NONE &&
	    di_buf->process_fun_index) { /*not bypass?*/

		ppost->post_wr_cnt++;
		ppost->process_doing = true;

		dim_post_process(di_buf, 0, vf_p->width - 1,
				 0, vf_p->height - 1, vf_p);

		/*begin to count timer*/
		di_tout_contr(EDI_TOUT_CONTR_EN, &pst->tout);

		ppost->post_de_busy = 1;
		ppost->irq_time = cur_to_msecs();
	} else {
		ppost->de_post_process_done = 1;	/*trig done*/
		pst->flg_int_done = 1;
	}
	ppost->cur_post_buf = di_buf;

	return true;
}

static unsigned int nm_set(void *data)
{
	struct di_hpst_s  *pst = get_hw_pst();
	unsigned int ch;

	ch = pst->curr_ch;
	if (_dpst_step_set(ch))
		return K_DO_R_FINISH;

	return K_DO_R_JUMP(K_DO_TABLE_ID_STOP);
}

//0: need wait;
//1: get int, to stop
//2: time out, to timeout
static unsigned int _dpst_step_wait_int(unsigned int ch)
{
	struct di_hpst_s  *pst = get_hw_pst();
	struct di_post_stru_s *ppost;
	unsigned int ret = 0;

	struct di_ch_s *pch;
	struct di_buf_s *di_buf = NULL;

	pch = get_chdata(ch);
	ppost = get_post_stru(ch);

	dim_print("%s:ch[%d],done_flg[%d]\n", __func__,
		  pst->curr_ch, pst->flg_int_done);
	if (pst->flg_int_done) { //done
		/*finish to count timer*/
		di_tout_contr(EDI_TOUT_CONTR_FINISH, &pst->tout);

		if (pch->link_mode == EPVPP_API_MODE_POST) {
			di_buf = ppost->cur_post_buf;
			di_buf->is_lastp = 0;
			if (dpvpp_ops(EPVPP_API_MODE_POST) &&
				dpvpp_is_allowed(EPVPP_API_MODE_POST) &&
				dpvpp_is_insert(EPVPP_API_MODE_POST)) {
				dpvpp_ops(EPVPP_API_MODE_POST)->post((void *)ppost->cur_post_buf);
			} else {
				PR_WARN("ch[%d]:post link but no ops! di_buf=%px\n", ch,
					ppost->cur_post_buf);
				recycle_vframe_type_post(ppost->cur_post_buf, ch);
			}
			ppost->cur_post_buf = NULL;
		} else {
			dim_post_de_done_buf_config(ch);
		}

		pst->flg_int_done = false;
		/*state*/

		ret = 1;
	} else {
		/*check if timeout:*/
		if (di_tout_contr(EDI_TOUT_CONTR_CHECK, &pst->tout)) {
			ppost = get_post_stru(ch);
			PR_WARN("ch[%d]:post timeout[%d]\n", ch,
				ppost->cur_post_buf->seq_post);
			hpst_timeout_read();
			dim_ddbg_mod_save(EDI_DBG_MOD_POST_TIMEOUT, ch, 0);
			/*state*/
			//pst->state = EDI_PST_ST_TIMEOUT;
			//reflesh = true;
			ret = 2;
		} else {
			ret = 0;
		}
	}
	return ret;
}

/* wait int:
 * return
 *	: stay here, wait	: K_DO_R_NOT_FINISH
 *	: got interrupt, to normal finish : K_DO_R_JUMP(K_DO_TABLE_ID_STOP)
 *	: timeout, to timeout control : K_DO_R_FINISH
 */
static unsigned int nm_wait_int(void *data)
{
	unsigned int wait;
	unsigned int ret = K_DO_R_NOT_FINISH;
	struct di_hpst_s  *pst = get_hw_pst();

	wait = _dpst_step_wait_int(pst->curr_ch);
	if (!wait)
		ret = K_DO_R_NOT_FINISH;
	else if (wait == 1)
		ret = K_DO_R_JUMP(K_DO_TABLE_ID_STOP);
	else //timeout:
		ret = K_DO_R_JUMP(EDI_NM_ST_TIMEOUT);

	return ret;
}

void dpst_timeout(unsigned int ch)
{
	hpst_dbg_mem_pd_trig(0);
	post_close_new();

	dimh_pst_trig_resize();
}

void _dpst_step_timeout(unsigned int ch)
{
	struct di_hpst_s  *pst = get_hw_pst();

	dpst_timeout(ch);
	dim_post_de_done_buf_config(ch);

	pst->flg_int_done = false;
	pst->flg_have_set = false;
}

/* timeout control */
static unsigned int nm_timeout(void *data)
{
	struct di_hpst_s  *pst = get_hw_pst();

	_dpst_step_timeout(pst->curr_ch);

	return K_DO_R_JUMP(K_DO_TABLE_ID_STOP);
}

const struct do_table_ops_s nm_sub_pol_tab[] = {
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
	.name = "nm_stop",
	},
	/******************/
	[K_DO_TABLE_ID_START] = {	/*EDI_NM_ST_CHECK*/
	.id = K_DO_TABLE_ID_START,
	.mark = 0,
	.con = NULL,
	.do_op = nm_check,
	.do_stop_op = NULL,
	.name = "nm-check",
	},
	[EDI_NM_ST_SET] = {	/*EDI_NM_ST_CHECK*/
	.id = EDI_NM_ST_SET,
	.mark = 0,
	.con = NULL,
	.do_op = nm_set,
	.do_stop_op = NULL,
	.name = "nm-set",
	},
	[EDI_NM_ST_WAIT_INT] = {
	.id = EDI_NM_ST_WAIT_INT,
	.mark = K_DO_TABLE_IS_WAIT,
	.con = NULL,
	.do_op = nm_wait_int,
	.do_stop_op = NULL,
	.name = "nm_wait_int",
	},
	[EDI_NM_ST_TIMEOUT] = {
	.id = EDI_NM_ST_TIMEOUT,
	.mark = 0,
	.con = NULL,
	.do_op = nm_timeout,
	.do_stop_op = NULL,
	.name = "nm_timeout",
	},
};

/*call in main loop check state */
void mp_check_sw_nm(unsigned int ch)
{
	struct di_ch_s *pch;

	dbg_dt("m_chk_sw[%d]\n", ch);

	pch = get_chdata(ch);
	if (!pch->itf.flg_rotation) {
		//tmp: set do_table:
		pol_pst_set_dtb(&nm_sub_pol_tab[0], ARRAY_SIZE(nm_sub_pol_tab));
	}
}

static void mp_stop_done_nm(unsigned int ch)
{
	PR_INF("%s:ch[%d]\n", __func__, ch);
}

static bool mp_has_job_nm(unsigned int ch)
{
	if (di_que_is_empty(ch, QUE_POST_DOING))
		return false;

	return true;
}

void dpost_init(unsigned int ch)
{/*reg:*/
	struct di_hpst_s  *pst = get_hw_pst();
	struct di_ch_s *pch;

	pch = get_chdata(ch);
	if (!pch->itf.flg_rotation) {
		memset(&pch->c, 0, sizeof(pch->c));
		pch->c.pst_pol_en = true;
		pch->c.pre_dis = false;
		pch->c.op_mp_check_sw = mp_check_sw_nm;
		pch->c.op_mp_has_job = mp_has_job_nm;
		pch->c.op_mp_stop_done = mp_stop_done_nm;
	}
	//no used pst->state = EDI_PST_ST_IDLE;

	/*timer out*/
	di_tout_int(&pst->tout, 40);	/*ms*/
	dim_p_pst_start(ch);
}

void dpost_unreg(unsigned int ch)
{
	struct di_ch_s *pch;

	pch = get_chdata(ch);

	memset(&pch->c, 0, sizeof(pch->c));
}

//=================================================

//---------------------------------------
// new main polling
//---------------------------------------

/************************************************/
//main polling:
/* function: idle find next ch, if there is some ch have job, to check
 *return:
 *	false: still in idle
 *	true: go next
 */
static bool m_pst_is_active(unsigned int ch)
{
	struct di_ch_s *pch;

	pch = get_chdata(ch);

	if (get_reg_flag(ch)			&&
	    !get_flag_trig_unreg(ch)	&&
	    !is_bypss2_complete(ch)		&&
	    pch->c.pst_pol_en) {
		return true;
	}

	return false;
}

static enum POL_M_RJ m_pst_check(unsigned int ch) /*switch do_table */
{
//	struct di_hpst_s  *pst = get_hw_pst();
	struct di_ch_s *pch;
	bool job = true;

	dbg_dt("m_chk[%d]\n", ch);

	pch = get_chdata(ch);
	if (pch->itf.flg_rotation && di_is_pause(ch))
		return POL_M_RJ_2_ID;

	di_pause_step_done(ch);

	if (pch->c.op_mp_has_job)
		job = pch->c.op_mp_has_job(ch);

	if (job)
		return POL_M_RJ_2_NX;

	return POL_M_RJ_2_ID;
}

const char * const mp_name4[] = {
	"EXIT",
	"IDLE",	/*switch to next channel?*/
	"CHECK",
	"DO_TABLE",
};

static const char *_mp_name_get(enum POL_M_ST state)
{
	if (state > POL_M_ST_DO_TABLE)
		return "nothing";

	return mp_name4[state];
}

void dim_p_pst_prob(void)
{
	struct s_pol_l_s *po;

	po = get_poll_pst();
	if (!po) {
		PR_ERR("%s:no po\n", __func__);
		return;
	}

	po->cmd_asy_stop_ch_bits = 0; /* one bit for one ch*/
	po->m_is_reg = m_pst_is_active;
	po->m_check = m_pst_check;
	spin_lock_init(&po->lock_c_stop);
}

/**************************************
 * dim_p_pst_asy_stop:
 *	has spinlock
 **************************************/
void dim_p_pst_asy_stop(unsigned int ch, bool sw)
{
	struct s_pol_l_s *po;

	po = get_poll_pst();

	if (!po) {
		PR_ERR("%s:no po\n", __func__);
		return;
	}
	PR_INF("asy_stop_cmd: ch[%d] %d\n", ch, sw);

	spin_lock(&po->lock_c_stop);//-------------
	if (sw)
		bset(&po->cmd_asy_stop_ch_bits, ch);
	else
		bclr(&po->cmd_asy_stop_ch_bits, ch);
	spin_unlock(&po->lock_c_stop);//-----------
}

static unsigned int _asy_stop_get(void)
{
	struct s_pol_l_s *po;
	unsigned int ret;

	po = get_poll_pst();
	if (!po) {
		PR_ERR("%s:no po\n", __func__);
		return false;
	}

	spin_lock(&po->lock_c_stop);//-------------
	ret = po->cmd_asy_stop_ch_bits;
	spin_unlock(&po->lock_c_stop);//-----------

	return ret;
}

void dim_p_pst_start(unsigned int ch)
{
	struct di_ch_s *pch;

	pch = get_chdata(ch);
	if (!pch)
		return;

	if (pch->c.st == POL_M_ST_EXIT) {
		pch->c.st = POL_M_ST_IDLE;
		PR_INF("%s:%d\n", "poll main start", ch);
	}
}

void pol_pst_set_dtb(const struct do_table_ops_s *ptable,
		   unsigned int size_tab)
{
	struct s_pol_l_s *po;

	po = get_poll_pst();
	if (!po)
		return;

	do_table_init(&po->s_do, ptable, size_tab);
}

static void _m_stop_pro(void)
{
	unsigned int stop, ch;
	struct s_pol_l_s *op;
	struct di_ch_s *pch;

	op = get_poll_pst();

	if (!op) {
		PR_ERR("%s:nothing\n", __func__);
		return;
	}
	stop = _asy_stop_get();
	if (!stop)
		return;
	for (ch = 0; ch < DI_CHANNEL_MAX; ch++) {
		if (bget(&stop, ch)) {
			pch = get_chdata(ch);
			if (pch->c.st != POL_M_ST_DO_TABLE) {
				pch->c.st = POL_M_ST_EXIT;
				dim_p_pst_asy_stop(ch, false);
				PR_INF("%s:ch[%d]\n", "poll stop", ch);
				//stop ch
				if (pch->c.op_mp_stop_done)
					pch->c.op_mp_stop_done(ch);
			}
		}
	}
}

static unsigned int _m_pst_prc(unsigned int ch)
{
	struct s_pol_l_s *op;
	enum POL_M_RJ mr;
//	unsigned int i;
	enum POL_M_ST ch_st;
	struct di_ch_s *pch;
	struct di_hpst_s *pst;

	unsigned int do_n = 2;

	op = get_poll_pst();

	pch = get_chdata(ch);

	if (!op || !pch || !pch->c.st) {
		//PR_INF("%s:nothing\n", __func__);
		return 0;
	}
	ch_st = pch->c.st;
	pst = get_hw_pst();
	if (ch_st > POL_M_ST_EXIT)
		dim_recycle_post_back(ch); //tmp
	dbg_dt("%s:ch[%d]:%d:state:%s\n", "m-polling", ch, ch_st, _mp_name_get(ch_st));

	switch (ch_st) {
	case POL_M_ST_IDLE:	//judge if reg, if pst is enable

		mr = POL_M_RJ_ST;

		if (op->m_is_reg(ch))
			mr = POL_M_RJ_2_NX;

		if (mr == POL_M_RJ_2_NX) {
			do_n = 1; /* need loop */
			ch_st++;
			if (op->m_idle2check)
				op->m_idle2check(ch);
			if (op->m_2_check)
				op->m_2_check(ch);
		} else {
			do_n = 0;	/* no loop */
		}

		break;
	case POL_M_ST_CHECK:

		if (op->m_bypass_proc)
			op->m_bypass_proc(ch);

		if (pst->hw_flg_busy_post) {
			ch_st = POL_M_ST_IDLE;
			do_n = 0;	/* no loop */
			break;
		}

		if (op->m_check)
			mr = op->m_check(ch);
		else
			mr = POL_M_RJ_2_NX;

		if (mr ==  POL_M_RJ_2_NX) {
			if (pch->c.op_mp_check_sw)
				pch->c.op_mp_check_sw(ch);
			ch_st++;
			//to do table
			do_table_cmd(&op->s_do, EDO_TABLE_CMD_START);
			if (op->m_2_do)
				op->m_2_do(ch);
			do_n = 1; /* need loop */
			//set busy
			pst->hw_flg_busy_post = true;
			pst->curr_ch = ch;
			pst->pres = get_pre_stru(pst->curr_ch);
			pst->psts = get_post_stru(pst->curr_ch);
		} else if (mr == POL_M_RJ_2_ID) {
			ch_st = POL_M_ST_IDLE;
			do_n = 0;

			if (op->m_2_idle)
				op->m_2_idle();
		}
		break;
	case POL_M_ST_DO_TABLE:
		do_table_working(&op->s_do);
		if (do_table_is_crr(&op->s_do, K_DO_TABLE_ID_STOP)) {
			pst->hw_flg_busy_post = false;
			ch_st = POL_M_ST_IDLE;
			if (op->m_2_idle)
				op->m_2_idle();
			do_n = 0;
		} else {
			if (do_table_is_wait(&op->s_do))
				do_n = 0;
			else
				do_n = 1;
		}
		break;
	default:
		break;
	}

	pch->c.st = ch_st;
	return do_n;
}

void dpst_process(void)
{
	bool re_do = true;
	unsigned int do_nt;
	int i;

	_m_stop_pro();
	for (i = 0; i < DI_CHANNEL_MAX; i++) {
		re_do = true;

		while (re_do) {
			do_nt = _m_pst_prc(i);
			if (do_nt == 1)
				re_do = true;
			else if (!do_nt)
				re_do = false;
			else
				re_do = false;
		}
	}
	_m_stop_pro();
}

