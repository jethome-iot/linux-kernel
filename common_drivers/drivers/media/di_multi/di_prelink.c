// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/semaphore.h>
#include <linux/kfifo.h>
#include <linux/spinlock.h>
#include <linux/amlogic/tee.h>

#include "deinterlace.h"
#include "di_data_l.h"
#include "di_data.h"
#include "di_pre.h"
#include "di_prc.h"
#include "di_dbg.h"
#include "di_que.h"

#include "di_vframe.h"
#include "di_task.h"
#include "di_sys.h"
#include "di_api.h"
#include "di_reg_v3.h"
#include "di_hw_v3.h"
#include "di_reg_v2.h"
#include "di_afbc_v3.h"

#include "register.h"
#include "register_nr4.h"

#include <linux/dma-map-ops.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/media/di/di_interface.h>
#define MAX_DI_HOLD_CTRL_CNT 20
#define MAX_SCREEN_RATIO 400
//#define AUTO_NR_DISABLE		(1)
//#define DBG_TIMER		(1)
//#define DBG_FLOW_SETTING		(1)

/*VFRAME_EVENT_PROVIDER_*/
#define VP_EVENT(cc)	(VFRAME_EVENT_PROVIDER_##cc)

static void dpvpph_reg_setting(const struct reg_acc *op_in);
static void dpvpph_size_change(struct dim_pvpp_ds_s *ds,
			       struct dimn_dvfm_s *dvfm,
			       const struct reg_acc *op_in);
static void dpvpph_display_update_all(struct dim_pvpp_ds_s *ds,
			       struct dimn_dvfm_s *dvfm,
			       const struct reg_acc *op_in);
static void dpvpph_display_update_part(struct dim_pvpp_ds_s *ds,
			       struct dimn_dvfm_s *ndvfm,
			       const struct reg_acc *op_in,
			       unsigned int diff);
static void dpvpph_pre_data_mif_ctrl(bool enable, const struct reg_acc *op_in);
static void dpvpph_reverse_mif_ctrl(bool reverse, unsigned int hv_mirror,
					const struct reg_acc *op_in);

static void dpvpph_gl_sw(bool on, bool pvpp_link, const struct reg_acc *op);
static void dpvpph_prelink_sw(const struct reg_acc *op, bool p_link);

static bool dpvpp_parser_nr(struct dimn_itf_s *itf);
static bool vtype_fill_d(struct dimn_itf_s *itf,
		struct vframe_s *vfmt,
		struct vframe_s *vfmf,
		struct dvfm_s *by_dvfm);
static void dpvpph_prelink_polling_check(const struct reg_acc *op, bool p_link);
static void dpvpph_pre_frame_reset_g12(bool pvpp_link,
				       const struct reg_acc *op_in);
static unsigned int dpvpp_dbg_force_dech;
module_param_named(dpvpp_dbg_force_dech, dpvpp_dbg_force_dech, uint, 0664);
static unsigned int dpvpp_dbg_force_decv;
module_param_named(dpvpp_dbg_force_decv, dpvpp_dbg_force_decv, uint, 0664);

/* refor to enum EPVPP_BUF_CFG_T */
const char *const name_buf_cfg[] = {
	"hd_f",
	"hd_f_tvp",
	"uhd_f_afbce",
	"uhd_f_afbce_tvp",
	"uhd_f_afbce_hd",
	"uhd_f_afbce_hd_tvp",
	"hd_nv21",
	"hd_nv21_tvp",
	"uhd_nv21",
	"uhd_nv21_tvp",
	"uhd_f",
	"uhd_f_tvp",
};

struct di_hold_setting {
	u8 level_id;
	u32 clk_val;
	u32 reg_val;
};

struct di_hold_ctrl {
	u32 max_clk;
	u32 last_clk_level;
	u32 available_level_cnt;
	struct di_hold_setting setting[MAX_DI_HOLD_CTRL_CNT];
};

struct di_hold_ctrl reg_set;

static struct di_hold_setting setting[MAX_DI_HOLD_CTRL_CNT] = {
	{.level_id = 0,
	.clk_val = 33,
	.reg_val = 0x80130001,
	},
	{.level_id = 1,
	.clk_val = 55,
	.reg_val = 0x800b0001,
	},
	{.level_id = 2,
	.clk_val = 66,
	.reg_val = 0x80090001,
	},
	{.level_id = 3,
	.clk_val = 111,
	.reg_val = 0x80050001,
	},
	{.level_id = 4,
	.clk_val = 148,
	.reg_val = 0x80070002,
	},
	{.level_id = 5,
	.clk_val = 180,
	.reg_val = 0x80080003,
	},
	{.level_id = 6,
	.clk_val = 200,
	.reg_val = 0x80070003,
	},
	{.level_id = 7,
	.clk_val = 240,
	.reg_val = 0x80070004,
	},
	{.level_id = 8,
	.clk_val = 277,
	.reg_val = 0x80070005,
	},
	{.level_id = 9,
	.clk_val = 302,
	.reg_val = 0x80060005,
	},
	{.level_id = 10,
	.clk_val = 333,
	.reg_val = 0x80050005,
	},
	{.level_id = 11,
	.clk_val = 363,
	.reg_val = 0x80050006,
	},
	{.level_id = 12,
	.clk_val = 388,
	.reg_val = 0x80050007,
	},
	{.level_id = 13,
	.clk_val = 423,
	.reg_val = 0x80040007,
	},
	{.level_id = 14,
	.clk_val = 466,
	.reg_val = 0x80030007,
	},
	{.level_id = 15,
	.clk_val = 518,
	.reg_val = 0x80020007,
	},
	{.level_id = 16,
	.clk_val = 544,
	.reg_val = 0x80020009,
	},
	{.level_id = 17,
	.clk_val = 599,
	.reg_val = 0x80010009,
	},
	{.level_id = 18,
	.clk_val = 610,
	.reg_val = 0x8001000b
	},
	{.level_id = 19,
	.clk_val = 624,
	.reg_val = 0x8001000f
	},
};

//move to di_plink_api.c
/**********************************************
 * bit 2 for vpp
 * bit 3 for di
 * bit 5 vpp-link is post write;
 * bit 7 nr disable irq setting;
 * bit 8 bypass in parser
 * bit [16]: force mem bypass
 * bit [18]: force not write nr
 * bit [19]: force bypass top	//in parser
 * bit [20]: force pause
 * bit [22]: force not crop win;
 * bit [23]: force bypass dct post;
 * bit [24]: dbg irq en
 * bit [25]: force not bypass mem
 * bit [30]: force disable pre_hold;
 **********************************************/
static unsigned int tst_pre_vpp;
module_param_named(tst_pre_vpp, tst_pre_vpp, uint, 0664);

//bit 0 for vpp
bool dim_is_pre_link(void)
{
	if (tst_pre_vpp & DI_BIT2)
		return true;
	return false;
}

static bool dim_is_pre_link_l(void)
{
	if (tst_pre_vpp & DI_BIT3)
		return true;

	return false;
}

static bool dim_is_pst_wr(void)
{
	if (tst_pre_vpp & DI_BIT5)
		return true;
	return false;
}

static bool dpvpp_dbg_force_mem_bypass(void)
{
	if (tst_pre_vpp & DI_BIT16)
		return true;

	return false;
}

static bool dpvpp_dbg_force_disable_ddr_wr(void)
{
	if (tst_pre_vpp & DI_BIT18) {
		tst_pre_vpp |= DI_BIT16;//need bypass mem
		return true;
	}
	return false;
}

static bool dpvpp_dbg_force_no_crop(void)
{
	if (tst_pre_vpp & DI_BIT22)
		return true;
	return false;
}

static bool dpvpp_dbg_force_bypass_dct_post(void)
{
	if (tst_pre_vpp & DI_BIT23)
		return true;
	return false;
}

static bool dpvpp_dbg_force_mem_en(void)
{
	if (tst_pre_vpp & DI_BIT25)
		return true;

	return false;
}

//mem follow
static bool dpvpp_dbg_mem_fl_in(void)
{
	if (tst_pre_vpp & DI_BIT26)
		return true;

	return false;
}

static bool dpvpp_dbg_force_disable_pre_hold(void)
{
	if (tst_pre_vpp & DI_BIT30)
		return true;
	return false;
}

#ifdef DBG_FLOW_SETTING
void dim_print_win(struct di_win_s *win, char *name)
{
	if (!win) {
		PR_INF("%s:%s:no\n", __func__, name);
		return;
	}
	PR_INF("%s:%s:\n", __func__, name);

	PR_INF("%s:[%d:%d:%d:%d:%d:%d]:\n", "win",
		(unsigned int)win->x_size,
		(unsigned int)win->y_size,
		(unsigned int)win->x_end,
		(unsigned int)win->y_end,
		(unsigned int)win->x_st,
		(unsigned int)win->x_st);
}

int print_dim_mifpara(struct dim_mifpara_s *p, char *name)
{
	if (!p) {
		PR_INF("%s:%s:no\n", __func__, name);
		return 0;
	}
	PR_INF("%s:%s:\n", __func__, name);
	PR_INF("%s:0x%x:\n", "cur_inp_type", p->cur_inp_type);
	PR_INF("%s:0x%x:\n", "prog_proc_type", p->prog_proc_type);
//	PR_INF("%s:%d:\n", "linear", p->linear);
	PR_INF("%s:[0x%x:0x%x:0x%x:]:\n", "cvs_id",
		(unsigned int)p->cvs_id[0],
		(unsigned int)p->cvs_id[1],
		(unsigned int)p->cvs_id[2]);
	dim_print_win(&p->win, name);

	return 0;
}

int print_pvpp_dis_para_in(struct pvpp_dis_para_in_s *p, char *name)
{
	if (!p)
		return 0;

	PR_INF("pvpp_dis_para_in:%s", name);
	PR_INF("\t%s:0x%x", "dmode", p->dmode);
	dim_print_win(&p->win, name);
	return 0;
}
#endif /* DBG_FLOW_SETTING */

void dbg_irq_status(unsigned int pos)
{
	if (!dpvpp_dbg_en_irq())
		return;

	PR_INF("dbg:irq:0x%08x\n", pos);
}

static struct vframe_s *in_patch_first_buffer(struct dimn_itf_s *itf)
{
	unsigned int cnt;
	struct buf_que_s *pbufq;
	union q_buf_u q_buf;
	struct dim_nins_s *ins;
	bool ret;
	unsigned int qt_in;
	unsigned int bindex;
	struct di_ch_s *pch;
	struct vframe_s *vf = NULL;

	if (!itf)
		return NULL;

	if (itf->bind_ch >= DI_CHANNEL_MAX) {
		PR_ERR("%s:ch[%d] overflow\n", __func__, itf->bind_ch);
		return NULL;
	}
	pch = get_chdata(itf->bind_ch);

	if (!pch) {
		PR_ERR("%s:no pch:id[%d]\n", __func__, itf->bind_ch);
		return NULL;
	}
	pbufq = &pch->nin_qb;

	qt_in = QBF_NINS_Q_CHECK;

	cnt = nins_cnt(pch, qt_in);

	if (!cnt)
		return NULL;

	ret = qbuf_out(pbufq, qt_in, &bindex);
	if (!ret) {
		PR_ERR("%s:1:%d:can't get out\n", __func__, bindex);
		return NULL;
	}
	if (bindex >= DIM_NINS_NUB) {
		PR_ERR("%s:2:%d\n", __func__, bindex);
		return NULL;
	}
	q_buf = pbufq->pbuf[bindex];
	ins = (struct dim_nins_s *)q_buf.qbc;
	vf = (struct vframe_s *)ins->c.ori;
	if (!vf) {
		ins->c.vfm_cp.decontour_pre = NULL;
		memset(&ins->c, 0, sizeof(ins->c));
		qbuf_in(pbufq, QBF_NINS_Q_IDLE, bindex);
		PR_ERR("%s:3:%d\n", __func__, bindex);
		return NULL;
	}
	vf->di_flag = 0;
	vf->di_flag |= DI_FLAG_DI_GET;
	if (ins->c.vfm_cp.decontour_pre)
		vf->decontour_pre = ins->c.vfm_cp.decontour_pre;
	else
		vf->decontour_pre = NULL;
	ins->c.vfm_cp.decontour_pre = NULL;
	memset(&ins->c, 0, sizeof(ins->c));
	qbuf_in(pbufq, QBF_NINS_Q_IDLE, bindex);
	itf->c.sum_pre_get++;
	dim_print("%s:%px cnt:%d\n", __func__, vf, itf->c.sum_pre_get);
	return vf;
}

/* copy from di_cnt_pst_afbct */
//static
void dpvpp_cnt_pst_afbct(struct dimn_itf_s *itf)
{
	/* cnt the largest size to avoid rebuild */
	struct dim_pvpp_ds_s *ds = NULL;
//	struct dimn_itf_s *itf;

	unsigned int height, width;
	bool is_4k = false;
//	const struct di_mm_cfg_s *pcfg;
	unsigned int afbc_buffer_size = 0, afbc_tab_size = 0;
	unsigned int afbc_info_size = 0, blk_total = 0, tsize;
	bool flg_afbc = false;
	struct div2_mm_s *mm;

	if (!itf || !itf->ds)
		return;
	ds = itf->ds;
	mm = &ds->mm;

	if (dim_afds()			&&
	    dim_afds()->cnt_info_size	&&
	    ds->en_out_afbce)
		flg_afbc = true;

	if (!flg_afbc) {
		mm->cfg.size_pafbct_all	= 0;
		mm->cfg.size_pafbct_one	= 0;
		mm->cfg.nub_pafbct	= 0;
		mm->cfg.dat_pafbct_flg.d32 = 0;
		return;
	}

	/* if ic is not support*/
	is_4k = ds->en_4k;

	width	= mm->cfg.di_w;
	height	= mm->cfg.di_h;

	afbc_info_size = dim_afds()->cnt_info_size(width,
						   height,
						   &blk_total);
	afbc_buffer_size =
		dim_afds()->cnt_buf_size(0x21, blk_total);
	afbc_buffer_size = roundup(afbc_buffer_size, PAGE_SIZE);
	tsize = afbc_buffer_size + afbc_info_size;
	afbc_tab_size =
		dim_afds()->cnt_tab_size(tsize);

	mm->cfg.nub_pafbct	= mm->cfg.num_post;
	mm->cfg.size_pafbct_all = afbc_tab_size * mm->cfg.nub_pafbct;
	mm->cfg.size_pafbct_one = afbc_tab_size;

	mm->cfg.dat_pafbct_flg.d32	= 0;
	//mm->cfg.dat_pafbct_flg.b.afbc	= 1;
	mm->cfg.dat_pafbct_flg.b.typ	= EDIM_BLK_TYP_PAFBCT;
	mm->cfg.dat_pafbct_flg.b.page	= mm->cfg.size_pafbct_all >> PAGE_SHIFT;
	mm->cfg.dat_pafbci_flg.b.tvp	= 0;
	PR_INF("%s:size_pafbct_all:0x%x, 0x%x, nub[%d]:<0x%x,0x%x>\n",
		 __func__,
		 mm->cfg.size_pafbct_all,
		 mm->cfg.size_pafbct_one,
		 mm->cfg.nub_pafbct,
		 afbc_info_size,
		 afbc_buffer_size);
}

/* use dpvpp_set_pst_afbct to replace dpvpp_cnt_pst_afbct*/
static void dpvpp_set_pst_afbct(struct dimn_itf_s *itf)
{
	struct dim_pvpp_ds_s *ds = NULL;

	//unsigned int height;
	bool is_4k = false;
	struct c_cfg_afbc_s *inf_afbc;
	//unsigned int afbc_info_size = 0;
	bool flg_afbc = false;
	struct div2_mm_s *mm;

	if (!itf || !itf->ds)
		return;
	ds = itf->ds;
	mm = &ds->mm;

	if (dim_afds()			&&
	    dim_afds()->cnt_info_size	&&
	    ds->en_out_afbce)
		flg_afbc = true;

	if (!flg_afbc) {
		mm->cfg.size_pafbct_all	= 0;
		mm->cfg.size_pafbct_one	= 0;
		mm->cfg.nub_pafbct	= 0;
		mm->cfg.dat_pafbct_flg.d32 = 0;
		return;
	}

	/* if ic is not support*/
	is_4k = ds->en_4k;

	if (is_4k)
		inf_afbc = m_rd_afbc(EAFBC_T_4K_FULL);
	else
		inf_afbc = m_rd_afbc(EAFBC_T_HD_FULL);
	mm->cfg.nub_pafbct	= mm->cfg.num_post;
	mm->cfg.size_pafbct_all = inf_afbc->size_table * mm->cfg.nub_pafbct;
	mm->cfg.size_pafbct_one = inf_afbc->size_table;

	mm->cfg.dat_pafbct_flg.d32	= 0;
	//mm->cfg.dat_pafbct_flg.b.afbc	= 1;
	mm->cfg.dat_pafbct_flg.b.typ	= EDIM_BLK_TYP_PAFBCT;
	mm->cfg.dat_pafbct_flg.b.page	= mm->cfg.size_pafbct_all >> PAGE_SHIFT;
	mm->cfg.dat_pafbci_flg.b.tvp	= 0;
	PR_INF("%s:size_pafbct_all:0x%x, 0x%x, nub[%d]\n",
		 __func__,
		 mm->cfg.size_pafbct_all,
		 mm->cfg.size_pafbct_one,
		 mm->cfg.nub_pafbct);
}

int dpvpp_set_post_buf(struct dimn_itf_s *itf)
{
	struct dim_pvpp_ds_s *ds = NULL;
	struct c_mm_p_cnt_out_s *inf_buf;
	struct c_cfg_afbc_s *inf_afbc_a;
	struct c_cfg_afbc_s *inf_afbc_b;
	struct div2_mm_s *mm;
	unsigned int nr_width, nr_canvas_width, canvas_align_width = 32;
	unsigned int height, width;
//	unsigned int tmpa, tmpb;
	unsigned int canvas_height;
	unsigned int afbc_info_size = 0, afbc_tab_size = 0;
	unsigned int afbc_buffer_size = 0;
	enum EDPST_MODE mode;
	bool is_4k = false;
	bool is_yuv420_10 = false;
	unsigned int canvas_align_width_hf = 64;

	if (!itf || !itf->ds) {
		PR_INF("%s:no ds ?\n", __func__);
		return false;
	}
	ds = itf->ds;
	mm	= &ds->mm;

	height	= mm->cfg.di_h;
	canvas_height = roundup(height, 32);
	width	= mm->cfg.di_w;
	is_4k	= ds->en_4k;
	mm->cfg.pbuf_hsize = width;
	nr_width = width;
	dbg_mem2("%s w[%d]h[%d]\n", __func__, width, height);
	nr_canvas_width = width;
	/**********************************************/
	/* count buf info */
	/**********************************************/
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		canvas_align_width = 64;

	/***********************/
	mode = ds->out_mode;

	dbg_mem2("%s:mode:%d\n", __func__, mode);

	/***********************/

	/* p */
	//tmp nr_width = roundup(nr_width, canvas_align_width);
	mm->cfg.pst_mode = mode;
	if (mode == EDPST_MODE_NV21_8BIT) {
		inf_buf = m_rd_mm_cnt(EBLK_T_HD_NV21);
		mm->cfg.pst_buf_uv_size = inf_buf->size_total - inf_buf->off_uv;
		mm->cfg.pst_buf_y_size	= inf_buf->off_uv;
		mm->cfg.pst_buf_size	= inf_buf->size_total;
		mm->cfg.size_post	= mm->cfg.pst_buf_size;
		mm->cfg.pst_cvs_w	= inf_buf->cvs_w;
		mm->cfg.pst_cvs_h	= inf_buf->cvs_h;
		mm->cfg.pst_afbci_size	= 0;
		mm->cfg.pst_afbct_size	= 0;
	} else {
		/* 422 */
		inf_buf = m_rd_mm_cnt(EBLK_T_HD_FULL);
		inf_afbc_a = m_rd_afbc(EAFBC_T_4K_FULL);
		inf_afbc_b = m_rd_afbc(EAFBC_T_4K_YUV420_10);
		mm->cfg.pst_buf_size	= inf_buf->size_total;
		mm->cfg.pst_buf_uv_size	= inf_buf->size_total >> 1;
		mm->cfg.pst_buf_y_size	= mm->cfg.pst_buf_uv_size;

		if (dim_afds() && dim_afds()->cnt_info_size &&
		    ds->en_out_afbce) {
			afbc_info_size = inf_afbc_a->size_i;
			if (is_4k && ds->out_afbce_nv2110)
				is_yuv420_10 = true;

			if (is_yuv420_10)
				afbc_buffer_size = inf_afbc_b->size_buffer;
			else
				afbc_buffer_size = inf_afbc_a->size_buffer;

			if (afbc_buffer_size > mm->cfg.pst_buf_size) {
				PR_INF("%s:0x%x, 0x%x\n", "buf large:",
				       mm->cfg.pst_buf_size,
				       afbc_buffer_size);
				mm->cfg.pst_buf_size = afbc_buffer_size;
			}

			if (is_yuv420_10)
				mm->cfg.pst_buf_size = afbc_buffer_size;

			afbc_tab_size =	inf_afbc_a->size_table;
		}
		mm->cfg.pst_afbci_size	= afbc_info_size;
		mm->cfg.pst_afbct_size	= afbc_tab_size;

		if (is_4k && ds->en_out_sctmem) {
			mm->cfg.size_post	= mm->cfg.pst_afbci_size;
		} else {
			mm->cfg.size_post	= mm->cfg.pst_buf_size +
					mm->cfg.pst_afbci_size;
		}

		mm->cfg.pst_cvs_w	= inf_buf->cvs_w;
		mm->cfg.pst_cvs_h	= inf_buf->cvs_h;
	}
	if (ds->en_hf_buf) {
		width = roundup(1920, canvas_align_width_hf);
		mm->cfg.size_buf_hf = width * 1080;
		mm->cfg.size_buf_hf	= PAGE_ALIGN(mm->cfg.size_buf_hf);
		mm->cfg.hf_hsize = width;
		mm->cfg.hf_vsize = 1080;

	} else {
		mm->cfg.size_buf_hf = 0;
	}

	if (ds->en_dw) {
		mm->cfg.dw_size = dim_getdw()->size_info.p.size_total;
		mm->cfg.size_post += mm->cfg.dw_size;
		//done use rsc flg pch->en_dw_mem = true;
	} else {
		mm->cfg.dw_size = 0;
	}

	mm->cfg.size_post	= roundup(mm->cfg.size_post, PAGE_SIZE);
	mm->cfg.size_post_page	= mm->cfg.size_post >> PAGE_SHIFT;

	/* p */
	mm->cfg.pbuf_flg.b.page = mm->cfg.size_post_page;
	mm->cfg.pbuf_flg.b.is_i = 0;
	if (mm->cfg.pst_afbct_size)
		mm->cfg.pbuf_flg.b.afbc = 1;
	else
		mm->cfg.pbuf_flg.b.afbc = 0;
	if (mm->cfg.dw_size)
		mm->cfg.pbuf_flg.b.dw = 1;
	else
		mm->cfg.pbuf_flg.b.dw = 0;

	if (ds->en_out_sctmem)
		mm->cfg.pbuf_flg.b.typ = EDIM_BLK_TYP_PSCT;
	else
		mm->cfg.pbuf_flg.b.typ = EDIM_BLK_TYP_OLDP;

	if (0/*dpvpp_itf_is_ins_exbuf(itf)*/) {
		mm->cfg.pbuf_flg.b.typ |= EDIM_BLK_TYP_POUT;
		mm->cfg.size_buf_hf = 0;
	}

	dbg_mem2("%s:3 pst_cvs_w[%d]\n", __func__, mm->cfg.pst_cvs_w);

#ifdef HIS_CODE
	PR_INF("%s:size:\n", __func__);
	PR_INF("\t%-15s:0x%x\n", "total_size", mm->cfg.size_post);
	PR_INF("\t%-15s:0x%x\n", "total_page", mm->cfg.size_post_page);
	PR_INF("\t%-15s:0x%x\n", "buf_size", mm->cfg.pst_buf_size);
	PR_INF("\t%-15s:0x%x\n", "y_size", mm->cfg.pst_buf_y_size);
	PR_INF("\t%-15s:0x%x\n", "uv_size", mm->cfg.pst_buf_uv_size);
	PR_INF("\t%-15s:0x%x\n", "afbci_size", mm->cfg.pst_afbci_size);
	PR_INF("\t%-15s:0x%x\n", "afbct_size", mm->cfg.pst_afbct_size);
	PR_INF("\t%-15s:0x%x\n", "flg", mm->cfg.pbuf_flg.d32);
	PR_INF("\t%-15s:0x%x\n", "dw_size", mm->cfg.dw_size);
	PR_INF("\t%-15s:0x%x\n", "size_hf", mm->cfg.size_buf_hf);
#endif
	return 0;
}

irqreturn_t dpvpp_pre_irq(int irq, void *dev_instance)
{
	struct dim_pvpp_ds_s *ds;
	//struct di_hpre_s  *pre = get_hw_pre();
	struct dim_pvpp_hw_s *hw;
	unsigned int id;

	unsigned int data32 = RD(DI_INTR_CTRL);
	unsigned int mask32 = (data32 >> 16) & 0x3ff;
	unsigned int flag = 0;
	//unsigned long irq_flg;
	static unsigned int dbg_last_fcnt;

	//dim_print("%s:\n", __func__);
	hw = &get_datal()->dvs_pvpp.hw;
	id = hw->id_c;

	ds = get_datal()->dvs_pvpp.itf[id].ds;
	if (!ds) {
		PR_ERR("%s:no ds\n", __func__);
		return IRQ_HANDLED;
	}

	//dim_ddbg_mod_save(EDI_DBG_MOD_PRE_IRQB, 0, 0);
#ifdef HIS_CODE
	if (!sc2_dbg_is_en_pre_irq()) {
		sc2_dbg_pre_info(data32);
		dim_ddbg_mod_save(EDI_DBG_MOD_PRE_IRQE, 0, 0);
		return;
	}
#endif
	if (dim_is_pre_link_l() || !hw->en_pst_wr_test) {
#ifdef HIS_CODE
		data32 &= 0x3fffffff;
		DIM_DI_WR(DI_INTR_CTRL, data32);
		DIM_DI_WR(DI_INTR_CTRL,
			  (data32 & 0xfffffffb) | (hw->intr_mode << 30));
#endif
		if (ds &&
		    dbg_last_fcnt == ds->pre_done_nub) {
			//dim_print("irq:pre link %d\n", ds->pre_done_nub);
			dbg_irq_status(0x02); /*dbg*/
			return IRQ_HANDLED;
		}
	}
	dbg_last_fcnt = ds->pre_done_nub;
	data32 &= 0x3fffffff;
	if ((data32 & 2) && !(mask32 & 2)) {
		dim_print("irq pre MTNWR ==\n");
		flag = 1;
	} else if ((data32 & 1) && !(mask32 & 1)) {
		dim_print("irq pre NRWR ==\n");
		flag = 1;
	} else {
		dim_print("irq pre DI IRQ 0x%x ==\n", data32);
		flag = 0;
	}

	if (flag && !hw->en_pst_wr_test) {
		//2023-03-30 dpvpph_gl_sw(false, true, &di_pre_regset);
		//dim_arb_sw(false);	//test-04 0622
		//dim_arb_sw(true);	//test-04 0622
		dbg_irq_status(0x03); /*dbg*/
	}

	DIM_DI_WR(DI_INTR_CTRL,
	  (data32 & 0xfffffffb) | (hw->intr_mode << 30));

	if (hw->en_pst_wr_test)
		complete(&hw->pw_done);
	return IRQ_HANDLED;
}

//static
bool dpvpp_mem_reg_ch(struct dimn_itf_s *itf)
{
	struct dim_pvpp_hw_s *hw;
	struct dim_pvpp_ds_s *ds;
	struct div2_mm_s *mm;
	//unsigned int out_fmt;
	//struct dvfm_s *dvfm_dm;

	if (!itf)
		return false;
	if (!itf->ds)
		return false;
	if (itf->c.src_state & EDVPP_SRC_NEED_MSK_CRT)
		return false;

	if (!itf->c.sum_pre_get)
		return false;

	if (!itf->c.m_mode_n)
		return false;
	if (!(itf->c.src_state & EDVPP_SRC_NEED_MEM))
		return false;

	hw = &get_datal()->dvs_pvpp.hw;
	ds = itf->ds;
	mm	= &ds->mm;
	if ((itf->c.m_mode_n == EPVPP_MEM_T_HD_FULL && hw->blk_rd_hd) ||
	    ((itf->c.m_mode_n == EPVPP_MEM_T_UHD_AFBCE ||
	      itf->c.m_mode_n == EPVPP_MEM_T_UHD_AFBCE_HD) &&
	     hw->blk_rd_uhd)) {
		PR_INF("%s:ch[%d]:%d:%d:%d reg_nub:%d\n", __func__, itf->bind_ch,
			itf->c.m_mode_n, hw->blk_rd_hd, hw->blk_rd_uhd, hw->reg_nub + 1);
		ds = itf->ds;

		if (!ds->en_out_afbce)
			itf->c.src_state &= ~(EDVPP_SRC_NEED_SCT_TAB);

		/* cnt post mem size */
		if (dim_afds())
			dim_afds()->pvpp_reg_val(itf);
		dpvpp_set_pst_afbct(itf);
		dpvpp_set_post_buf(itf);
		//dpvpp_pst_sec_alloc(itf, mm->cfg.dat_pafbct_flg.d32);

		hw->reg_nub++;
		itf->c.flg_reg_mem = true;
		itf->c.src_state &= (~EDVPP_SRC_NEED_MEM);

		itf->c.src_state &= (~EDVPP_SRC_NEED_REG2);
		get_datal()->pre_vpp_exist = true; /* ?? */

		dbg_plink1("%s:ok:src_state[0x%x],pst_wr[%d], afbce[%d]\n",
			__func__,
			itf->c.src_state,
			hw->en_pst_wr_test,
			ds->en_out_afbce);
	}

	return true;
}

static struct pvpp_buf_cfg_s *get_buf_cfg(unsigned char index, unsigned int pos)
{
	struct pvpp_buf_cfg_s *buf_cfg;
	struct dim_pvpp_hw_s *hw;

	hw = &get_datal()->dvs_pvpp.hw;
	if (!hw || index >= K_BUF_CFG_T_NUB) {
		PR_WARN("%s:out_fmt[%d]:%d\n", __func__, index, pos);
		return NULL;
	}
	buf_cfg = &hw->buf_cfg[index];
	if (!buf_cfg->support) {
		PR_WARN("%s:not support [%d]:%d\n", __func__, index, pos);
		return NULL;
	}
	return buf_cfg;
}

void buf_cfg_prob(void)
{
	struct dim_pvpp_hw_s *hw;
	struct pvpp_buf_cfg_s *cfg;
//	unsigned char index = 0;
	struct c_mm_p_cnt_out_s *inf_buf;
	int i, j;

	hw = &get_datal()->dvs_pvpp.hw;
	memset(&hw->buf_cfg[0], 0, sizeof(hw->buf_cfg));

	for (i = 0; i < K_BUF_CFG_T_NUB; i++) {
		cfg = &hw->buf_cfg[i];
		cfg->type = i;
		cfg->dbuf_plan_nub = 1;
		if (i == EPVPP_BUF_CFG_T_HD_F	||
		    i == EPVPP_BUF_CFG_T_HD_F_TVP) {
			inf_buf = m_rd_mm_cnt(EBLK_T_HD_FULL);
			cfg->dbuf_hsize = inf_buf->dbuf_hsize;
			for (j = 0; j < DPVPP_BUF_NUB; j++) {
				cfg->dbuf_wr_cfg[j][0].width = inf_buf->cvs_w;
				cfg->dbuf_wr_cfg[j][0].height = inf_buf->cvs_h;
			}
		} else if (i == EPVPP_BUF_CFG_T_UHD_F_AFBCE	||
			   i == EPVPP_BUF_CFG_T_UHD_F_AFBCE_TVP) {
			inf_buf = m_rd_mm_cnt(EBLK_T_UHD_FULL);
			cfg->dbuf_hsize = inf_buf->dbuf_hsize;
			for (j = 0; j < DPVPP_BUF_NUB; j++) {
				cfg->dbuf_wr_cfg[j][0].width = inf_buf->cvs_w;
				cfg->dbuf_wr_cfg[j][0].height = inf_buf->cvs_h;
			}
		} else if (i == EPVPP_BUF_CFG_T_UHD_F_AFBCE_HD	||
			   i == EPVPP_BUF_CFG_T_UHD_F_AFBCE_HD_TVP) {
			inf_buf = m_rd_mm_cnt(EBLK_T_UHD_FULL);
			cfg->dbuf_hsize = inf_buf->dbuf_hsize;
			for (j = 0; j < DPVPP_BUF_NUB; j++) {
				cfg->dbuf_wr_cfg[j][0].width = inf_buf->cvs_w;
				cfg->dbuf_wr_cfg[j][0].height = inf_buf->cvs_h;
			}
		} else if (i == EPVPP_BUF_CFG_T_HD_NV21	||
			   i == EPVPP_BUF_CFG_T_HD_NV21_TVP) {
			inf_buf = m_rd_mm_cnt(EBLK_T_HD_NV21);
			cfg->dbuf_plan_nub = 2;
			cfg->dbuf_hsize = inf_buf->dbuf_hsize;
			for (j = 0; j < DPVPP_BUF_NUB; j++) {
				cfg->dbuf_wr_cfg[j][0].width = inf_buf->cvs_w;
				cfg->dbuf_wr_cfg[j][0].height = inf_buf->cvs_h;
				cfg->dbuf_wr_cfg[j][1].width = inf_buf->cvs_w;
				cfg->dbuf_wr_cfg[j][1].height = inf_buf->cvs_h;
			}
		} else if (i == EPVPP_BUF_CFG_T_UHD_NV21	||
			   i == EPVPP_BUF_CFG_T_UHD_NV21_TVP) {
			inf_buf = m_rd_mm_cnt(EBLK_T_HD_NV21);

			cfg->dbuf_plan_nub = 2;
			cfg->dbuf_hsize = inf_buf->dbuf_hsize;
			for (j = 0; j < DPVPP_BUF_NUB; j++) {
				cfg->dbuf_wr_cfg[j][0].width = inf_buf->cvs_w;
				cfg->dbuf_wr_cfg[j][0].height = inf_buf->cvs_h;
				cfg->dbuf_wr_cfg[j][1].width = inf_buf->cvs_w;
				cfg->dbuf_wr_cfg[j][1].height = inf_buf->cvs_h;
			}
		} else if (i == EPVPP_BUF_CFG_T_UHD_F	||
			   i == EPVPP_BUF_CFG_T_UHD_F_TVP) {
			inf_buf = m_rd_mm_cnt(EBLK_T_UHD_FULL);
			cfg->dbuf_hsize = inf_buf->dbuf_hsize;
			for (j = 0; j < DPVPP_BUF_NUB; j++) {
				cfg->dbuf_wr_cfg[j][0].width = inf_buf->cvs_w;
				cfg->dbuf_wr_cfg[j][0].height = inf_buf->cvs_h;
			}
		}
	}
}

/**************************************
 * mode : ff:  clear all not care idx
 * mode : 0: clear the idx
 * mode : 1: set idx
 * idx: ref to enum EPVPP_BUF_CFG_T
 * note:
 *	for set, use EPVPP_BUF_CFG_T_HD_F meas:
 *	EPVPP_BUF_CFG_T_HD_F or
 *	EPVPP_BUF_CFG_T_HD_F_TVP and
 *	EPVPP_BUF_CFG_T_HD_NV21 or
 *	EPVPP_BUF_CFG_T_HD_NV21_TVP
 **************************************/
//static
void mem_count_buf_cfg(unsigned char mode, unsigned char idx)
{
	struct dim_pvpp_hw_s *hw;
	struct pvpp_buf_cfg_s *cfg;
	unsigned char index = 0;
	struct c_blk_m_s	*blk_m;
	struct blk_s *blk;
	int i, j;
	unsigned int offs;
	struct c_mm_p_cnt_out_s *inf_nv21;
	struct c_cfg_afbc_s *inf_a;
	struct afbce_map_s afbce_map;
	struct di_dev_s *devp = get_dim_de_devp();
	unsigned int b_uhd_size, b_hd_size, t_offs, size_buf, size_table;
	unsigned int b_uhd_total, b_hd_total;
	unsigned long buf_start;

	hw = &get_datal()->dvs_pvpp.hw;
	dim_print("%s:%d:%d\n", __func__, mode, idx);

	if (mode == 0xff) {
		for (i = 0; i < K_BUF_CFG_T_NUB; i++)
			hw->buf_cfg[i].support = false;

		PR_INF("%s:clear all\n", __func__);
		return;
	}
	if (mode > 1 || idx >= EPVPP_BUF_CFG_T_NUB) {
		PR_WARN("%s:overflow:%d:%d\n", __func__, mode, idx);
		return;
	}
	if (mode == 0) {
		hw->buf_cfg[idx].support = false;
		PR_INF("%s:clear %d\n", __func__, idx);
		//dbg_str_buf_cfg(&hw->buf_cfg[idx], get_name_buf_cfg(idx));
		return;
	}

	if (mode == 1 &&
		   (idx == EPVPP_BUF_CFG_T_HD_F ||
		    idx == EPVPP_BUF_CFG_T_HD_F_TVP)) {
		/* for hd */
		inf_nv21 = m_rd_mm_cnt(EBLK_T_HD_NV21);
		if (!hw->src_blk[0] || !hw->src_blk[1] ||
		    !inf_nv21) {
			PR_WARN("%s:no blk?\n", __func__);
			return;
		}

		if (hw->flg_tvp)
			index = EPVPP_BUF_CFG_T_HD_F_TVP;
		else
			index = EPVPP_BUF_CFG_T_HD_F;
		/**/
		cfg = &hw->buf_cfg[index];
		for (i = 0; i < DPVPP_BUF_NUB; i++) {
			blk = hw->src_blk[i];
			if (!blk) {
				PR_ERR("%s:hd:no src blk\n", __func__);
				return;
			}
			blk_m = &blk->c.b.blk_m;
			cfg->dbuf_wr_cfg[i][0].phy_addr = blk_m->mem_start;
		}
		cfg->support = true;
		//dbg_str_buf_cfg(&hw->buf_cfg[index], get_name_buf_cfg(index));
		/* for nv21 */
		if (hw->flg_tvp)
			index = EPVPP_BUF_CFG_T_HD_NV21_TVP;
		else
			index = EPVPP_BUF_CFG_T_HD_NV21;
		/**/
		cfg = &hw->buf_cfg[index];

		offs = 0;
		for (i = 0; i < DPVPP_BUF_NUB; i++) {
			blk = hw->src_blk[i];
			if (!blk) {
				PR_ERR("%s:hd:no src blk\n", __func__);
				return;
			}
			blk_m = &blk->c.b.blk_m;
			cfg->dbuf_wr_cfg[i][0].phy_addr = blk_m->mem_start;
			dbg_plink1("%s:HD addr:[%d]:0x%lx\n", __func__,
				i, cfg->dbuf_wr_cfg[i][0].phy_addr);
			offs = blk_m->mem_start + inf_nv21->off_uv;
			cfg->dbuf_wr_cfg[i][1].phy_addr = offs;
		}
		cfg->support = true;
		//dbg_str_buf_cfg(&hw->buf_cfg[index], get_name_buf_cfg(index));
	} else if (mode == 1 &&
		   (idx == EPVPP_BUF_CFG_T_UHD_F_AFBCE ||
		    idx == EPVPP_BUF_CFG_T_UHD_F_AFBCE_TVP)) {
		/* check blk */
		for (i = 0; i < 2; i++) {
			if (!hw->src_blk_uhd_afbce[i]) {
				PR_ERR("%s:no buffer?%d\n", __func__, i);
				return;
			}
		}
		if (hw->flg_tvp)
			index = EPVPP_BUF_CFG_T_UHD_F_AFBCE_TVP;
		else
			index = EPVPP_BUF_CFG_T_UHD_F_AFBCE;
		cfg = &hw->buf_cfg[index];

		offs = 0;
		inf_a = m_rd_afbc(EAFBC_T_4K_FULL);
		/* afbce table */
		cfg->addr_e[0] = hw->src_blk_e->c.b.blk_m.mem_start;
		cfg->addr_e[1] = cfg->addr_e[0] + inf_a->size_table;

		/* afbce infor */
		cfg->addr_i[0] = hw->src_blk_uhd_afbce[0]->c.b.blk_m.mem_start;
		cfg->addr_i[1] = hw->src_blk_uhd_afbce[1]->c.b.blk_m.mem_start;

		inf_nv21 = m_rd_mm_cnt(EBLK_T_UHD_FULL_AFBCE);

		for (j = 0; j < DPVPP_BUF_NUB; j++) {
			buf_start = cfg->addr_i[j] + inf_a->size_i;
			cfg->afbc_crc[j] = 0;
			afbce_map.bodyadd	= buf_start;
			afbce_map.tabadd	= cfg->addr_e[j];
			afbce_map.size_buf	= inf_a->size_buffer;
			afbce_map.size_tab	= inf_a->size_table;
			cfg->afbc_crc[j] =
				dim_afds()->int_tab(&devp->pdev->dev,
						    &afbce_map);
			/* mif */
			cfg->dbuf_wr_cfg[j][0].phy_addr = buf_start;
			dbg_plink1("%s:UHD addr:[%d]:0x%lx\n", __func__,
				j, cfg->dbuf_wr_cfg[j][0].phy_addr);
		}
		cfg->support = true;
		//dbg_str_buf_cfg(&hw->buf_cfg[index], get_name_buf_cfg(index));
		/* uhd buffer config hd buffer */
		if (hw->flg_tvp)
			index = EPVPP_BUF_CFG_T_HD_F_TVP;
		else
			index = EPVPP_BUF_CFG_T_HD_F;
		/**/
		cfg = &hw->buf_cfg[index];

		offs = 0;
		inf_nv21 = m_rd_mm_cnt(EBLK_T_HD_FULL);
		buf_start = hw->src_blk_uhd_afbce[0]->c.b.blk_m.mem_start;
		for (i = 0; i < DPVPP_BUF_NUB; i++) {
			cfg->dbuf_wr_cfg[i][0].phy_addr = buf_start + offs;
			offs = inf_nv21->size_total;
			dbg_plink1("%s:HD(UHD) addr:[%d]:0x%lx\n", __func__,
				i, cfg->dbuf_wr_cfg[i][0].phy_addr);
		}
		cfg->support = true;
		//dbg_str_buf_cfg(&hw->buf_cfg[index], get_name_buf_cfg(index));
		/**********************************************/
		/* nv 21 to-do */
	} else if (mode == 1 &&
		   (idx == EPVPP_BUF_CFG_T_UHD_F_AFBCE_HD ||
		    idx == EPVPP_BUF_CFG_T_UHD_F_AFBCE_HD_TVP)) {
		/* check blk */
		for (i = 0; i < 10; i++) {
			if (!hw->src_blk[i]) {
				PR_ERR("%s:no buffer?%d\n", __func__, i);
				return;
			}
		}
		if (hw->flg_tvp)
			index = EPVPP_BUF_CFG_T_UHD_F_AFBCE_HD_TVP;
		else
			index = EPVPP_BUF_CFG_T_UHD_F_AFBCE_HD;
		cfg = &hw->buf_cfg[index];
		offs = 0;
		inf_a = m_rd_afbc(EAFBC_T_4K_FULL);
		/* afbce table */
		cfg->addr_e[0] = hw->src_blk_e->c.b.blk_m.mem_start;
		cfg->addr_e[1] = cfg->addr_e[0] + inf_a->size_table;
		dbg_plink1("%s:afbc table:0x%lx, 0x%lx\n", __func__,
			cfg->addr_e[0],
			cfg->addr_e[1]);
		/* afbce infor */
		inf_nv21 = m_rd_mm_cnt(EBLK_T_HD_FULL);
		cfg->addr_i[0] = hw->src_blk[4]->c.b.blk_m.mem_start +
			inf_nv21->size_total - inf_a->size_i;
		cfg->addr_i[1] = hw->src_blk[9]->c.b.blk_m.mem_start +
			inf_nv21->size_total - inf_a->size_i;
		dbg_plink1("%s:afbc infor:0x%lx, 0x%lx\n", __func__,
			cfg->addr_i[0],
			cfg->addr_i[1]);
		//-----------------
		//buffer_4k	buffer_hd	table
		// b_uhd_total	b_hd_total	t_total
		// b_uhd_size	b_hd_size
		//				t_offs
		//
		b_hd_size = (inf_nv21->size_total >> 12) * sizeof(unsigned int);
		b_hd_total = (inf_nv21->size_total >> 12) << 12;
		for (j = 0; j < DPVPP_BUF_NUB; j++) {
			cfg->afbc_crc[j] = 0;
			t_offs = 0;
			b_uhd_size = ((inf_a->size_buffer + 0xfff) >> 12) * sizeof(unsigned int);
			b_uhd_total = ((inf_a->size_buffer + 0xfff) >> 12) << 12;

			for (i = 0; i < 5; i++) {
				blk = hw->src_blk[i + j * 5];
				blk_m = &blk->c.b.blk_m;

				if (b_uhd_size > b_hd_size) {
					size_table = b_hd_size;
					size_buf = b_hd_total;
				} else {
					size_table = inf_a->size_table - t_offs;
					size_buf = b_uhd_total;
				}
				afbce_map.bodyadd	= blk_m->mem_start;
				afbce_map.tabadd	= cfg->addr_e[j] + t_offs;
				afbce_map.size_buf	= size_buf;
				afbce_map.size_tab	= size_table;

				cfg->afbc_crc[j] +=
					dim_afds()->int_tab(&devp->pdev->dev,
							    &afbce_map);
				t_offs += size_table;
				b_uhd_size -= size_table;
				b_uhd_total -= b_hd_total;
			}
		}
		cfg->support = true;
		//dbg_str_buf_cfg(&hw->buf_cfg[index], get_name_buf_cfg(index));
	}
}

/************************************************
 * idx: enum EPVPP_BUF_CFG_T
 * by: enum EPVPP_MEM_T
 ************************************************/
static void mem_count_buf_cfg2(unsigned char mode, unsigned char idx, unsigned char by)
{
	struct dim_pvpp_hw_s *hw;
	struct pvpp_buf_cfg_s *cfg;
	unsigned char index = 0;
	struct c_blk_m_s	*blk_m;
	struct blk_s *blk;
	int i, j;
	unsigned int offs;
	struct c_mm_p_cnt_out_s *inf_nv21, *inf_buf;
	struct c_cfg_afbc_s *inf_a;
	struct afbce_map_s afbce_map;
	struct di_dev_s *devp = get_dim_de_devp();
	unsigned int b_uhd_size, b_hd_size, t_offs, size_buf, size_table;
	unsigned int b_uhd_total, b_hd_total;
	unsigned long buf_start;

	hw = &get_datal()->dvs_pvpp.hw;

	dim_print("%s:overflow:%d:%d,:%d\n", __func__, mode, idx, by);

	if (mode == 0xff) {
		for (i = 0; i < K_BUF_CFG_T_NUB; i++)
			hw->buf_cfg[i].support = false;

		PR_INF("%s:clear all\n", __func__);
		return;
	}
	if (mode > 1 || idx >= EPVPP_BUF_CFG_T_NUB) {
		PR_WARN("%s:overflow:%d:%d\n", __func__, mode, idx);
		return;
	}
	if (mode == 0) {
		hw->buf_cfg[idx].support = false;
		PR_INF("%s:clear %d\n", __func__, idx);
		//dbg_str_buf_cfg(&hw->buf_cfg[idx], get_name_buf_cfg(idx));
		return;
	}

	if ((idx == EPVPP_BUF_CFG_T_HD_F ||
	     idx == EPVPP_BUF_CFG_T_HD_F_TVP) &&
	    (by == EPVPP_MEM_T_HD_FULL ||
	     by == EPVPP_MEM_T_UHD_AFBCE_HD)) {
		if (!hw->src_blk[0] || !hw->src_blk[1]) {
			PR_WARN("%s:no blk?\n", __func__);
			return;
		}

		if (hw->flg_tvp)
			index = EPVPP_BUF_CFG_T_HD_F_TVP;
		else
			index = EPVPP_BUF_CFG_T_HD_F;
		/*check active or not to-do */
		/**/
		cfg = &hw->buf_cfg[index];
		for (i = 0; i < DPVPP_BUF_NUB; i++) {
			blk = hw->src_blk[i];
			if (!blk) {
				PR_ERR("%s:1:no src blk\n", __func__);
				return;
			}
			blk_m = &blk->c.b.blk_m;
			cfg->dbuf_wr_cfg[i][0].phy_addr = blk_m->mem_start;
			dbg_plink1("%s:HD1 addr:[%d]:0x%lx\n", __func__,
				i, cfg->dbuf_wr_cfg[i][0].phy_addr);
		}
		cfg->support = true;
	} else if ((idx == EPVPP_BUF_CFG_T_UHD_F ||
		    idx == EPVPP_BUF_CFG_T_UHD_F_TVP) &&
		   (by == EPVPP_MEM_T_UHD_FULL ||
		   by == EPVPP_MEM_T_UHD_AFBCE)) {
		if (!hw->src_blk_uhd_afbce[0] || !hw->src_blk_uhd_afbce[1]) {
			PR_ERR("%s:2:no blk?\n", __func__);
			return;
		}

		if (hw->flg_tvp)
			index = EPVPP_BUF_CFG_T_UHD_F_TVP;
		else
			index = EPVPP_BUF_CFG_T_UHD_F;
		/*check active or not to-do */
		/**/
		cfg = &hw->buf_cfg[index];
		for (i = 0; i < DPVPP_BUF_NUB; i++) {
			blk = hw->src_blk_uhd_afbce[i];
			if (!blk) {
				PR_ERR("%s:hd:no src blk\n", __func__);
				return;
			}
			blk_m = &blk->c.b.blk_m;
			cfg->dbuf_wr_cfg[i][0].phy_addr = blk_m->mem_start;
			dbg_plink1("%s:UHD1 addr:[%d]:0x%lx\n", __func__,
				i, cfg->dbuf_wr_cfg[i][0].phy_addr);
		}
		cfg->support = true;
	} else if ((idx == EPVPP_BUF_CFG_T_HD_F ||
		    idx == EPVPP_BUF_CFG_T_HD_F_TVP) &&
		    (by == EPVPP_MEM_T_UHD_FULL ||
		     by == EPVPP_MEM_T_UHD_AFBCE)) {
		//dbg_str_buf_cfg(&hw->buf_cfg[index], get_name_buf_cfg(index));
		/* uhd buffer config hd buffer */
		if (hw->flg_tvp)
			index = EPVPP_BUF_CFG_T_HD_F_TVP;
		else
			index = EPVPP_BUF_CFG_T_HD_F;
		/**/
		cfg = &hw->buf_cfg[index];

		offs = 0;
		inf_buf = m_rd_mm_cnt(EBLK_T_HD_FULL);
		buf_start = hw->src_blk_uhd_afbce[0]->c.b.blk_m.mem_start;
		for (i = 0; i < DPVPP_BUF_NUB; i++) {
			cfg->dbuf_wr_cfg[i][0].phy_addr = buf_start + offs;
			offs = inf_buf->size_total;
			dbg_plink1("%s:HD(UHD) addr:[%d]:0x%lx\n", __func__,
				i, cfg->dbuf_wr_cfg[i][0].phy_addr);
		}
		cfg->support = true;
		//dbg_str_buf_cfg(&hw->buf_cfg[index], get_name_buf_cfg(index));
	} else if ((idx == EPVPP_BUF_CFG_T_HD_NV21 ||
		   idx == EPVPP_BUF_CFG_T_HD_NV21_TVP) &&
		   (by == EPVPP_MEM_T_HD_FULL ||
		    by == EPVPP_MEM_T_UHD_AFBCE_HD)) {
		inf_nv21 = m_rd_mm_cnt(EBLK_T_HD_NV21);
		if (!inf_nv21) {
			PR_ERR("no inf:2\n");
			return;
		}
		if (hw->flg_tvp)
			index = EPVPP_BUF_CFG_T_HD_NV21_TVP;
		else
			index = EPVPP_BUF_CFG_T_HD_NV21;
		/*check active or not to-do */
		cfg = &hw->buf_cfg[index];

		offs = 0;
		for (i = 0; i < DPVPP_BUF_NUB; i++) {
			blk = hw->src_blk[i];
			if (!blk) {
				PR_ERR("%s:hd:no src blk\n", __func__);
				return;
			}
			blk_m = &blk->c.b.blk_m;
			cfg->dbuf_wr_cfg[i][0].phy_addr = blk_m->mem_start;
			dbg_plink1("%s:HD2 addr:[%d]:0x%lx\n", __func__,
				i, cfg->dbuf_wr_cfg[i][0].phy_addr);

			offs = blk_m->mem_start + inf_nv21->off_uv;
			cfg->dbuf_wr_cfg[i][1].phy_addr = offs;
		}
		cfg->support = true;
	} else if ((idx == EPVPP_BUF_CFG_T_UHD_F_AFBCE ||
		    idx == EPVPP_BUF_CFG_T_UHD_F_AFBCE_TVP) &&
		   by == EPVPP_MEM_T_UHD_AFBCE) {
		/* check blk */
		for (i = 0; i < 2; i++) {
			if (!hw->src_blk_uhd_afbce[i]) {
				PR_ERR("%s:no buffer?%d\n", __func__, i);
				return;
			}
		}
		if (hw->flg_tvp)
			index = EPVPP_BUF_CFG_T_UHD_F_AFBCE_TVP;
		else
			index = EPVPP_BUF_CFG_T_UHD_F_AFBCE;
		cfg = &hw->buf_cfg[index];

		offs = 0;
		inf_a = m_rd_afbc(EAFBC_T_4K_FULL);
		/* afbce table */
		cfg->addr_e[0] = hw->src_blk_e->c.b.blk_m.mem_start;
		cfg->addr_e[1] = cfg->addr_e[0] + inf_a->size_table;

		/* afbce infor */
		cfg->addr_i[0] = hw->src_blk_uhd_afbce[0]->c.b.blk_m.mem_start;
		cfg->addr_i[1] = hw->src_blk_uhd_afbce[1]->c.b.blk_m.mem_start;

		for (j = 0; j < DPVPP_BUF_NUB; j++) {
			buf_start = cfg->addr_i[j] + inf_a->size_i;
			cfg->afbc_crc[j] = 0;
			afbce_map.bodyadd	= buf_start;
			afbce_map.tabadd	= cfg->addr_e[j];
			afbce_map.size_buf	= inf_a->size_buffer;
			afbce_map.size_tab	= inf_a->size_table;
			cfg->afbc_crc[j] =
				dim_afds()->int_tab(&devp->pdev->dev,
						    &afbce_map);
			/* mif */
			cfg->dbuf_wr_cfg[j][0].phy_addr = buf_start;
			dbg_plink1("%s:UHD2 addr:[%d]:0x%lx\n", __func__,
				j, cfg->dbuf_wr_cfg[j][0].phy_addr);
		}
		cfg->support = true;
	} else if ((idx == EPVPP_BUF_CFG_T_UHD_F_AFBCE_HD ||
		    idx == EPVPP_BUF_CFG_T_UHD_F_AFBCE_HD_TVP) &&
		   by == EPVPP_MEM_T_UHD_AFBCE_HD) {
		/* check blk */
		for (i = 0; i < 10; i++) {
			if (!hw->src_blk[i]) {
				PR_ERR("%s:no buffer?%d\n", __func__, i);
				return;
			}
		}
		if (hw->flg_tvp)
			index = EPVPP_BUF_CFG_T_UHD_F_AFBCE_HD_TVP;
		else
			index = EPVPP_BUF_CFG_T_UHD_F_AFBCE_HD;
		cfg = &hw->buf_cfg[index];
		offs = 0;
		inf_a = m_rd_afbc(EAFBC_T_4K_FULL);
		/* afbce table */
		cfg->addr_e[0] = hw->src_blk_e->c.b.blk_m.mem_start;
		cfg->addr_e[1] = cfg->addr_e[0] + inf_a->size_table;
		dbg_plink1("%s:afbc table:0x%lx, 0x%lx\n", __func__,
			cfg->addr_e[0],
			cfg->addr_e[1]);
		/* afbce infor */
		inf_buf = m_rd_mm_cnt(EBLK_T_HD_FULL);
		cfg->addr_i[0] = hw->src_blk[4]->c.b.blk_m.mem_start +
			inf_buf->size_total - inf_a->size_i;
		cfg->addr_i[1] = hw->src_blk[9]->c.b.blk_m.mem_start +
			inf_buf->size_total - inf_a->size_i;
		dbg_plink1("%s:afbc infor:0x%lx, 0x%lx\n", __func__,
			cfg->addr_i[0],
			cfg->addr_i[1]);
		//-----------------
		//buffer_4k	buffer_hd	table
		// b_uhd_total	b_hd_total	t_total
		// b_uhd_size	b_hd_size
		//				t_offs
		//
		b_hd_size = (inf_buf->size_total >> 12) * sizeof(unsigned int);
		b_hd_total = (inf_buf->size_total >> 12) << 12;
		for (j = 0; j < DPVPP_BUF_NUB; j++) {
			cfg->afbc_crc[j] = 0;
			t_offs = 0;
			b_uhd_size = ((inf_a->size_buffer + 0xfff) >> 12) * sizeof(unsigned int);
			b_uhd_total = ((inf_a->size_buffer + 0xfff) >> 12) << 12;

			for (i = 0; i < 5; i++) {
				blk = hw->src_blk[i + j * 5];
				blk_m = &blk->c.b.blk_m;

				if (b_uhd_size > b_hd_size) {
					size_table = b_hd_size;
					size_buf = b_hd_total;
				} else {
					size_table = inf_a->size_table - t_offs;
					size_buf = b_uhd_total;
				}
				afbce_map.bodyadd	= blk_m->mem_start;
				afbce_map.tabadd	= cfg->addr_e[j] + t_offs;
				afbce_map.size_buf	= size_buf;
				afbce_map.size_tab	= size_table;

				cfg->afbc_crc[j] +=
					dim_afds()->int_tab(&devp->pdev->dev,
							    &afbce_map);
				t_offs += size_table;
				b_uhd_size -= size_table;
				b_uhd_total -= b_hd_total;
			}
		}
		cfg->support = true;

	} else {
		PR_ERR("%s:not support:%d:%d\n", __func__, idx, by);
	}
}

/* call when buffer is rd */
static void mem_hw_reg(void)
{
	struct dim_pvpp_hw_s *hw;

	hw = &get_datal()->dvs_pvpp.hw;
	dbg_plink1("%s\n", __func__);
	init_completion(&hw->pw_done);
	if (dim_is_pst_wr())
		hw->en_pst_wr_test	= true;
	else
		hw->en_pst_wr_test	= false; //true;//
	hw->intr_mode = 3;
}

static unsigned long dpvpp_dct_mem_alloc(unsigned int ch)
{
	struct dim_pvpp_hw_s *hw;
	unsigned long addr = 0;

	hw = &get_datal()->dvs_pvpp.hw;
	if (hw->src_blk_dct) {
		if (hw->dct_ch == 0xff) {
			addr = hw->src_blk_dct->c.b.blk_m.mem_start;
			hw->dct_ch = ch;
		} else {
			PR_WARN("%s:%d: dis_ch:%d; dct_ch:%d; dct_flg:%d; src_blk_dct:%px\n",
				__func__, ch,
				hw->dct_ch, hw->dis_ch,
				hw->dct_flg, hw->src_blk_dct);
		}
	}
	return addr;
}

static void dpvpp_dct_mem_release(unsigned int ch)
{
	struct dim_pvpp_hw_s *hw;

	hw = &get_datal()->dvs_pvpp.hw;
	if (hw->src_blk_dct && hw->dct_ch == ch)
		hw->dct_ch = 0xff;
	else
		PR_WARN("%s:%d: dis_ch:%d; dct_ch:%d; dct_flg:%d; src_blk_dct:%px\n",
			__func__, ch,
			hw->dct_ch, hw->dis_ch,
			hw->dct_flg, hw->src_blk_dct);
}

bool dpvpp_dct_mem_reg(struct di_ch_s *pch)
{
	ulong irq_flag = 0;
	unsigned long addr_dct;

	if (!pch) {
		PR_ERR("%s: no pch %px\n",
			__func__, pch);
		return false;
	}

	spin_lock_irqsave(&lock_pvpp, irq_flag);
	addr_dct = dpvpp_dct_mem_alloc(pch->ch_id);
	if (!addr_dct) {
		PR_ERR("%s: allocate dct mem fail\n", __func__);
		spin_unlock_irqrestore(&lock_pvpp, irq_flag);
		return false;
	}
	if (!dct_pre_plink_reg_mem(pch, addr_dct)) {
		dpvpp_dct_mem_release(pch->ch_id);
		PR_ERR("%s: ch[%d] plink_reg_mem fail\n",
			__func__, pch->ch_id);
		spin_unlock_irqrestore(&lock_pvpp, irq_flag);
		return false;
	}
	spin_unlock_irqrestore(&lock_pvpp, irq_flag);
	return true;
}

bool dpvpp_dct_mem_unreg(struct di_ch_s *pch)
{
	ulong irq_flag = 0;

	if (!pch) {
		PR_ERR("%s: no pch %px\n",
			__func__, pch);
		return false;
	}

	spin_lock_irqsave(&lock_pvpp, irq_flag);
	dpvpp_dct_mem_release(pch->ch_id);
	dct_pre_plink_unreg_mem(pch);
	spin_unlock_irqrestore(&lock_pvpp, irq_flag);
	return true;
}

/************************************************
 * dpvpp_dct_get_flg
 * return:
 *	true: have flag;
 * ch: back channel information;
 * data:
 *	1:reg decontour;
 *	2: unreg decontour;
 *	other: nothing;
 ************************************************/
bool dpvpp_dct_get_flg(unsigned char *ch, unsigned char *data)
{
	struct dim_pvpp_hw_s *hw;
	unsigned char ch_l, data_l;

	/* TODO: check if to add spin_lock_irqsave(&lock_pvpp, irq_flag) */
	hw = &get_datal()->dvs_pvpp.hw;
	if (hw->dct_flg) {
		if (hw->dct_flg == 2)
			ch_l = (unsigned char)hw->dct_ch;
		else
			ch_l = (unsigned char)hw->dis_ch;
		data_l = (unsigned char)hw->dct_flg;
		if (data_l == 1 && ch_l > DI_CHANNEL_NUB) {
			PR_ERR("%s:fix:%d,%d\n", __func__, data_l, ch_l);
			hw->dct_flg = 0;
			return false;
		}
		*ch = ch_l;
		*data = data_l;
		return true;
	}
	return false;
}

void dpvpp_dct_clear_flg(void)
{
	struct dim_pvpp_hw_s *hw;
	ulong irq_flag = 0;

	spin_lock_irqsave(&lock_pvpp, irq_flag);
	hw = &get_datal()->dvs_pvpp.hw;
	dbg_plink1("%s flg:%d dis_ch:%d dct_ch:%d\n",
		__func__, hw->dct_flg,
		hw->dis_ch, hw->dct_ch);
	hw->dct_flg = 0;
	spin_unlock_irqrestore(&lock_pvpp, irq_flag);
}

void dpvpp_dct_unreg(unsigned char ch)
{
	struct dim_pvpp_hw_s *hw;
	ulong irq_flag = 0;
	struct di_ch_s *pch;

	//for unreg
	hw = &get_datal()->dvs_pvpp.hw;
	if (hw->dct_ch != ch)
		return;
	spin_lock_irqsave(&lock_pvpp, irq_flag);
	pch = get_chdata(ch);
	dpvpp_dct_mem_release(ch);
	dct_pre_plink_unreg_mem(pch);
	if (hw->dct_flg) {
		PR_WARN("disable flg ch:%d flg:%d dis_ch:%d dct_ch:%d\n",
			ch, hw->dct_flg, hw->dis_ch, hw->dct_ch);
	} else {
		dbg_plink3("trig dct unreg 3 ch:%d dis_ch:%d dct_ch:%d\n",
			ch, hw->dis_ch, hw->dct_ch);
		hw->dis_ch = 0xff;
	}
	spin_unlock_irqrestore(&lock_pvpp, irq_flag);

	get_datal()->dct_op->unreg(pch);
}

static void dct_pst(const struct reg_acc *op, struct dimn_dvfm_s *ndvfm)
{
	struct vframe_s *vf;
	struct dim_pvpp_hw_s *hw;
	bool ret;
	struct di_ch_s *pch;
	struct dcntr_mem_s	*pdcn;
	bool dct_bypass_sts = false;

	hw = &get_datal()->dvs_pvpp.hw;
	if (hw->dis_ch == 0xff)
		return;
	if (!op || !ndvfm || !ndvfm->c.dct_pre)
		return;
	if (!ndvfm->itf) {
		PR_ERR("%s:no itf ndvfm:%px\n", __func__, ndvfm);
		return;
	}

	pch = get_chdata(ndvfm->itf->bind_ch);
	if (!pch) {
		PR_ERR("%s:no pch ndvfm->itf:%px bind_ch:%d\n",
			__func__, ndvfm->itf,
			ndvfm->itf->bind_ch);
		return;
	}
	if (!get_datal()->dct_op->mem_check(pch, ndvfm->c.dct_pre)) {
		dbg_plink1("%s:ch[%d]:dct_pre not match: %px %px\n",
			__func__, ndvfm->itf->bind_ch,
			ndvfm->c.dct_pre, pch->dct_pre);
		return;
	}
	vf = &ndvfm->c.vf_in_cp;
	vf->decontour_pre = ndvfm->c.dct_pre;
	pdcn = (struct dcntr_mem_s *)vf->decontour_pre;
	/* set pre-link flag to notify the dct pst process*/
	if (pdcn)
		pdcn->plink = true;
	dcntr_check(vf);
	ret = dcntr_set(op);
	if (ret)
		hw->dct_sum_d++;
	if (pdcn && !dpvpp_dbg_force_no_crop()) {
		if (pdcn->x_start != hw->dis_c_para.win.x_st ||
		    pdcn->x_size != hw->dis_c_para.win.x_size ||
		    pdcn->y_start != hw->dis_c_para.win.y_st ||
		    pdcn->y_size != hw->dis_c_para.win.y_size) {
			dcntr_hw_bypass(op);
			dct_bypass_sts = true;
			dim_print("%s: dct_bypass: (%d %d %d %d) -- (%d %d %d %d)\n",
				__func__,
				pdcn->x_start, pdcn->y_start,
				pdcn->x_size, pdcn->y_size,
				hw->dis_c_para.win.x_st,
				hw->dis_c_para.win.y_st,
				hw->dis_c_para.win.x_size,
				hw->dis_c_para.win.y_size);
		}
	}
	if (dpvpp_dbg_force_bypass_dct_post()) {
		dcntr_hw_bypass(op);
		dct_bypass_sts = true;
	}
	if (hw->last_dct_bypass != dct_bypass_sts && !dct_bypass_sts)
		hw->cur_dct_delay = 0;

	if (hw->cur_dct_delay < hw->dct_delay_cnt) {
		dcntr_hw_bypass(op);
		dim_print("dct enable delay:%d %d\n", hw->cur_dct_delay, hw->dct_delay_cnt);
		hw->cur_dct_delay++;
	}

	if (dct_bypass_sts != hw->last_dct_bypass)
		dim_print("%s: dct_bypass:%d->%d, force:%d\n",
			__func__, hw->last_dct_bypass, dct_bypass_sts,
			dpvpp_dbg_force_bypass_dct_post());
	hw->last_dct_bypass = dct_bypass_sts;
	dcntr_pq_tune(&hw->pq_rpt, op);
	vf->decontour_pre = NULL;
}

static void dpvpp_mem_dct(void)
{
	struct dim_pvpp_hw_s *hw;
	unsigned int dct_idx;
	struct blk_s *blk;
	struct dd_s *d_dd;

	if (!get_datal())
		return;
	hw = &get_datal()->dvs_pvpp.hw;
	d_dd = get_dd();
	if (hw->m_mode_tgt != hw->m_mode ||
	    hw->flg_tvp_tgt != hw->flg_tvp ||
	    hw->m_mode == EPVPP_MEM_T_NONE)
		return;

	if (hw->en_dct && !hw->src_blk_dct) {
		if (hw->flg_tvp)
			dct_idx = EBLK_T_LINK_DCT_TVP;
		else
			dct_idx = EBLK_T_LINK_DCT;
		blk = blk_o_get_locked(d_dd, dct_idx);
		if (blk) {
			hw->src_blk_dct = blk;
			dbg_plink1("dct mem get\n");
		}
	}
}

void dpvpp_secure_pre_en(bool is_tvp)
{
	if (is_tvp) {
		if (DIM_IS_IC_EF(SC2)) {
			DIM_DI_WR(DI_PRE_SEC_IN, 0x3F);//secure
		} else {
		#if IS_ENABLED(CONFIG_AMLOGIC_TEE)
			tee_config_device_state(16, 1);
		#endif
		}
		if (DIM_IS_IC(S5) || DIM_IS_IC(T3X)) {
		#if IS_ENABLED(CONFIG_AMLOGIC_TEE)
			tee_write_reg_bits
				(((DI_VIUB_SECURE_REG << 2) + 0xff800000),
				 1, 8, 1);// HF secure Polarity
		#endif
		}
		if (IS_IC_SUPPORT(DECONTOUR))
			DIM_DI_WR_REG_BITS(DI_VPU_SECURE_REG, 0x1, 8, 1);
		dbg_plink1("%s:tvp pre SECURE\n", __func__);
	} else {
		if (DIM_IS_IC_EF(SC2)) {
			DIM_DI_WR(DI_PRE_SEC_IN, 0x0);
		} else {
		#if IS_ENABLED(CONFIG_AMLOGIC_TEE)
			tee_config_device_state(16, 0);
		#endif
		}
		if (DIM_IS_IC(S5) || DIM_IS_IC(T3X)) {
		#if IS_ENABLED(CONFIG_AMLOGIC_TEE)
			tee_write_reg_bits
				(((DI_VIUB_SECURE_REG << 2) + 0xff800000),
				 0, 8, 1);// HF secure Polarity
		#endif
		}
		if (IS_IC_SUPPORT(DECONTOUR))
			DIM_DI_WR_REG_BITS(DI_VPU_SECURE_REG, 0x0, 8, 1);
		dbg_plink1("%s:tvp pre NOSECURE:\n", __func__);
	}
}

//static
void dpvpp_mng_set(void)
{
	struct dim_pvpp_hw_s *hw;
	int i, cnt = 0;
	struct dd_s *d_dd;
	struct blk_s *blk;
	bool ret = false;
	bool is_tvp;
	unsigned int index;

	if (!get_datal())
		return;
	hw = &get_datal()->dvs_pvpp.hw;
	d_dd = get_dd();

	if (hw->m_mode_tgt == hw->m_mode &&
	    hw->flg_tvp_tgt == hw->flg_tvp) {
		dim_print("%s:<%d,%d>, <%d,%d>\n", __func__,
			hw->m_mode_tgt, hw->flg_tvp_tgt,
			hw->m_mode, hw->flg_tvp);
		return;
	}

	dbg_plink1("%s:<%d,%d>, <%d,%d>\n", __func__,
		hw->m_mode_tgt, hw->flg_tvp_tgt,
		hw->m_mode, hw->flg_tvp);

	dpvpp_secure_pre_en(hw->flg_tvp_tgt);
	switch (hw->m_mode_tgt) {
	case EPVPP_MEM_T_HD_FULL:
		if (hw->flg_tvp_tgt) {
			is_tvp = true;
			index = EBLK_T_HD_FULL_TVP;
		} else {
			is_tvp = false;
			index = EBLK_T_HD_FULL;
		}
		hw->flg_tvp = hw->flg_tvp_tgt;
		cnt = 0;
		for (i = 0; i < 2; i++) {
			if (hw->src_blk[i]) {
				cnt++;
				continue;
			}
			blk = blk_o_get_locked(d_dd, index);
			if (blk) {
				hw->src_blk[i] = blk;
				cnt++;
				dbg_plink2("get blk %d\n", cnt); //tmp
			}
		}
		if (cnt >= 2) {
			ret = true;
			/* buffer is ready*/
			mem_hw_reg();
			//mem_count_buf_cfg(1, EPVPP_BUF_CFG_T_HD_F);
			mem_count_buf_cfg2(1, EPVPP_BUF_CFG_T_HD_F,
					   EPVPP_MEM_T_HD_FULL);
			mem_count_buf_cfg2(1, EPVPP_BUF_CFG_T_HD_NV21,
					   EPVPP_MEM_T_HD_FULL);
			hw->blk_rd_hd = true;
		}
		break;
	case EPVPP_MEM_T_UHD_AFBCE:
		if (hw->flg_tvp_tgt) {
			is_tvp = true;
			index = EBLK_T_UHD_FULL_AFBCE_TVP;
		} else {
			is_tvp = false;
			index = EBLK_T_UHD_FULL_AFBCE;
		}
		hw->flg_tvp = hw->flg_tvp_tgt;
		cnt = 0;
		for (i = 0; i < 2; i++) {
			if (hw->src_blk_uhd_afbce[i]) {
				cnt++;
				continue;
			}
			blk = blk_o_get_locked(d_dd, index);
			if (blk) {
				hw->src_blk_uhd_afbce[i] = blk;
				cnt++;
				dbg_plink2("uhd get blk %d\n", cnt);
			}
		}
		if (!hw->src_blk_e) {
			blk = blk_o_get_locked(d_dd, EBLK_T_S_LINK_AFBC_T_FULL);
			if (blk) {
				cnt++;
				hw->src_blk_e = blk;
				dbg_plink2("uhd get table %d\n", cnt);
			}
		} else {
			cnt++;
		}
		if (cnt >= 3) {
			ret = true;

			mem_hw_reg();
			//mem_count_buf_cfg(1, EPVPP_BUF_CFG_T_HD_F);
			//mem_count_buf_cfg(1, EPVPP_BUF_CFG_T_UHD_F_AFBCE);
			mem_count_buf_cfg2(1, EPVPP_BUF_CFG_T_HD_F,
					   EPVPP_MEM_T_UHD_AFBCE);
			mem_count_buf_cfg2(1, EPVPP_BUF_CFG_T_UHD_F,
					   EPVPP_MEM_T_UHD_AFBCE);
			mem_count_buf_cfg2(1, EPVPP_BUF_CFG_T_UHD_F_AFBCE,
					   EPVPP_MEM_T_UHD_AFBCE);
			hw->blk_rd_uhd = true;
			hw->blk_rd_hd = true;
		}
		break;
	case EPVPP_MEM_T_UHD_AFBCE_HD:
		if (hw->flg_tvp_tgt) {
			is_tvp = true;
			index = EBLK_T_HD_FULL_TVP;
		} else {
			is_tvp = false;
			index = EBLK_T_HD_FULL;
		}
		hw->flg_tvp = hw->flg_tvp_tgt;
		cnt = 0;
		for (i = 0; i < 10; i++) {
			if (hw->src_blk[i]) {
				cnt++;
				continue;
			}
			blk = blk_o_get_locked(d_dd, index);
			if (blk) {
				hw->src_blk[i] = blk;
				cnt++;
			}
		}
		if (!hw->src_blk_e) {
			blk = blk_o_get_locked(d_dd, EBLK_T_S_LINK_AFBC_T_FULL);
			if (blk) {
				cnt++;
				hw->src_blk_e = blk;
			}
		} else {
			cnt++;
		}
		if (cnt >= 11) {
			ret = true;
			mem_hw_reg();
			//mem_count_buf_cfg(1, EPVPP_BUF_CFG_T_HD_F);
			mem_count_buf_cfg(1, EPVPP_BUF_CFG_T_UHD_F_AFBCE_HD);
			hw->blk_rd_uhd = true;
		}
		break;
	case EPVPP_MEM_T_NONE:
	default:
		hw->m_mode = hw->m_mode_tgt;
		break;
	}
	if (ret) {
		dbg_plink1("%s:set:%d cnt:%d\n", __func__, hw->m_mode_tgt, cnt);
		hw->m_mode = hw->m_mode_tgt;
		hw->flg_tvp	= hw->flg_tvp_tgt;
	}
}

//static
void dpvpp_mem_mng_get(unsigned int id)
{
	unsigned int ch;
	struct dim_pvpp_hw_s *hw;
	struct dimn_itf_s *itf;
	int i;
	struct dd_s *d_dd;
	unsigned int blk_idx, dct_idx;
	unsigned int cnt[K_MEM_T_NUB], cnt_tvp[K_MEM_T_NUB];
	unsigned char tgt_mode;
	bool	tgt_tvp, back_tvp;
	ulong irq_flag = 0;
	bool flg_release_err = false;
	unsigned int back_tgt;

	if (!get_datal())
		return;
	hw = &get_datal()->dvs_pvpp.hw;
	d_dd = get_dd();

	/* TODO: remove the spinlock for blk_used_hd */
	spin_lock_irqsave(&lock_pvpp, irq_flag);
	if (hw->blk_used_hd || hw->blk_used_uhd) { /*in using, don't change */
		spin_unlock_irqrestore(&lock_pvpp, irq_flag);
		dim_print("%s:used<%d,%d>\n", __func__,
			hw->blk_used_hd, hw->blk_rd_uhd);
		return;
	}
	back_tgt = hw->m_mode_tgt;
	back_tvp = hw->flg_tvp_tgt;
	memset(&cnt[0], 0, sizeof(cnt));
	memset(&cnt_tvp[0], 0, sizeof(cnt_tvp));
	dbg_plink1("%s:s1:<%d,%d>, <%d,%d>\n", __func__,
		hw->m_mode_tgt, hw->flg_tvp_tgt,
		hw->m_mode, hw->flg_tvp);

	for (ch = 0; ch < DI_PLINK_CN_NUB; ch++) {
		//count mode:
		itf = &get_datal()->dvs_pvpp.itf[ch];
		if (!itf) {
			PR_ERR("%s:%d:overflow\n", __func__, ch);
			break;
		}
		if (!atomic_read(&itf->reg) || atomic_read(&itf->in_unreg))
			continue;
		if (itf->c.m_mode_n >= K_MEM_T_NUB) {
			PR_ERR("%s:%d:overflow:%d\n", __func__,
				ch, itf->c.m_mode_n);
			continue;
		}
		cnt[itf->c.m_mode_n]++;
		if (itf->c.is_tvp)
			cnt_tvp[itf->c.m_mode_n]++;
	}
	//PR_INF("%s:cnt:%d:%d:%d\n", __func__, cnt[1], cnt[2], cnt[3]);
	/*tmp use large buffer config */
	tgt_mode = EPVPP_MEM_T_NONE;
	tgt_tvp = false;
	for (i = K_MEM_T_NUB - 1; i > 0; i--) {
		if (cnt[i] > 0) {
			tgt_mode = i;
			if (cnt_tvp[i])
				tgt_tvp = true;
			else
				tgt_tvp = false;
			//PR_INF("%s:tgt_mode=%d:%d\n", __func__, tgt_mode, tgt_tvp);
			break;
		}
	}
	/* same */
	if (hw->m_mode_tgt == tgt_mode &&
	    hw->flg_tvp_tgt == tgt_tvp) {
		spin_unlock_irqrestore(&lock_pvpp, irq_flag);
		return;
	}
#ifdef _HIS_CODE_	//add snr
	if (get_datal()->pre_vpp_active) {
		/* check if cur_m_mode is none */
		if (hw->m_mode_tgt != EPVPP_MEM_T_NONE) {
			PR_WARN("%s: trigger release when vpp_active: <%d %d>-><%d %d> <%d %d>\n",
				__func__,
				hw->m_mode_tgt, hw->flg_tvp_tgt,
				tgt_mode, tgt_tvp,
				hw->blk_rd_uhd, hw->blk_rd_hd);
		} else {
			dbg_plink1("%s: vpp_active:<%d %d>-><%d %d> <%d %d>\n",
				__func__,
				hw->m_mode_tgt, hw->flg_tvp_tgt,
				tgt_mode, tgt_tvp,
				hw->blk_rd_uhd, hw->blk_rd_hd);
		}
	}
#endif
	dbg_plink1("%s:s2:<%d,%d>, <%d,%d>\n", __func__,
		hw->m_mode_tgt, hw->flg_tvp_tgt,
		hw->m_mode, hw->flg_tvp);

	/* release first */
	if (hw->m_mode_tgt) {
		if (hw->blk_used_hd || hw->blk_used_uhd) {
			flg_release_err = true;
		} else {
			flg_release_err = false;
			hw->blk_rd_uhd = false;
			hw->blk_rd_hd = false;
		}
		spin_unlock_irqrestore(&lock_pvpp, irq_flag);
		if (flg_release_err) {
			PR_ERR("release the buffer in used\n");
			return;
		}
		/* check dct */
		if (tgt_tvp != hw->flg_tvp_tgt &&
		    hw->src_blk_dct) {
			if (hw->flg_tvp_tgt)
				dct_idx = EBLK_T_LINK_DCT_TVP;
			else
				dct_idx = EBLK_T_LINK_DCT;
			blk_o_put_locked(d_dd, dct_idx, hw->src_blk_dct);
			hw->src_blk_dct = NULL;
		}
		/* src_blk_uhd_afbce */
		if (hw->flg_tvp_tgt)
			blk_idx = EBLK_T_UHD_FULL_AFBCE_TVP;
		else
			blk_idx = EBLK_T_UHD_FULL_AFBCE;
		for (i = 0; i < DPVPP_BLK_NUB_MAX; i++) {
			if (hw->src_blk_uhd_afbce[i]) {
				blk_o_put_locked(d_dd, blk_idx, hw->src_blk_uhd_afbce[i]);
				hw->src_blk_uhd_afbce[i] = NULL;
			}
		}
		/* src_blk */
		if (hw->flg_tvp_tgt)
			blk_idx = EBLK_T_HD_FULL_TVP;
		else
			blk_idx = EBLK_T_HD_FULL;
		for (i = 0; i < DPVPP_BLK_NUB_MAX; i++) {
			if (hw->src_blk[i]) {
				blk_o_put_locked(d_dd, blk_idx, hw->src_blk[i]);
				hw->src_blk[i] = NULL;
			}
		}
	} else {
		spin_unlock_irqrestore(&lock_pvpp, irq_flag);
	}
	/* alloc */
	if (tgt_tvp)
		dct_idx = EBLK_T_LINK_DCT_TVP;
	else
		dct_idx = EBLK_T_LINK_DCT;
	if (tgt_mode == EPVPP_MEM_T_UHD_AFBCE_HD) {
		if (tgt_tvp)
			blk_idx = EBLK_T_HD_FULL_TVP;

		else
			blk_idx = EBLK_T_HD_FULL;

		memset(&hw->blkt_n[0], 0, sizeof(hw->blkt_n));

		hw->blkt_n[blk_idx] = 10;
		hw->blkt_n[EBLK_T_S_LINK_AFBC_T_FULL] = 1;
		if (hw->en_dct)
			hw->blkt_n[dct_idx] = 1;
		m_a_blkt_cp(0, &hw->blkt_n[0]);
		hw->m_mode_tgt = EPVPP_MEM_T_UHD_AFBCE_HD;
		hw->flg_tvp_tgt = tgt_tvp;
	} else if (tgt_mode == EPVPP_MEM_T_UHD_AFBCE) {
		if (tgt_tvp)
			blk_idx = EBLK_T_UHD_FULL_AFBCE_TVP;
		else
			blk_idx = EBLK_T_UHD_FULL_AFBCE;

		memset(&hw->blkt_n[0], 0, sizeof(hw->blkt_n));

		hw->blkt_n[blk_idx] = 2;
		hw->blkt_n[EBLK_T_S_LINK_AFBC_T_FULL] = 1;
		if (hw->en_dct)
			hw->blkt_n[dct_idx] = 1;
		m_a_blkt_cp(0, &hw->blkt_n[0]);
		hw->m_mode_tgt = tgt_mode;
		hw->flg_tvp_tgt = tgt_tvp;
	} else if (tgt_mode == EPVPP_MEM_T_UHD_FULL) {
		if (tgt_tvp)
			blk_idx = EBLK_T_UHD_FULL_TVP;
		else
			blk_idx = EBLK_T_UHD_FULL;

		/* check afbce table */
		if (hw->src_blk_e) {
			blk_o_put_locked(d_dd, EBLK_T_S_LINK_AFBC_T_FULL, hw->src_blk_e);
			hw->src_blk_e = NULL;
		}
		memset(&hw->blkt_n[0], 0, sizeof(hw->blkt_n));

		hw->blkt_n[blk_idx] = 2;
		if (hw->en_dct)
			hw->blkt_n[dct_idx] = 1;
		m_a_blkt_cp(0, &hw->blkt_n[0]);
		hw->m_mode_tgt = tgt_mode;
		hw->flg_tvp_tgt = tgt_tvp;
	} else if (tgt_mode == EPVPP_MEM_T_HD_FULL) {
		if (tgt_tvp)
			blk_idx = EBLK_T_HD_FULL_TVP;
		else
			blk_idx = EBLK_T_HD_FULL;

		/* check afbce table */
		if (hw->src_blk_e) {
			blk_o_put_locked(d_dd, EBLK_T_S_LINK_AFBC_T_FULL, hw->src_blk_e);
			hw->src_blk_e = NULL;
		}
		memset(&hw->blkt_n[0], 0, sizeof(hw->blkt_n));
		hw->blkt_n[blk_idx] = 2;
		if (hw->en_dct)
			hw->blkt_n[dct_idx] = 1;
		m_a_blkt_cp(0, &hw->blkt_n[0]);
		hw->m_mode_tgt = EPVPP_MEM_T_HD_FULL;
		hw->flg_tvp_tgt = tgt_tvp;
	} else if (tgt_mode == EPVPP_MEM_T_NONE) {
		/* check afbce table */
		if (hw->src_blk_e) {
			blk_o_put_locked(d_dd, EBLK_T_S_LINK_AFBC_T_FULL, hw->src_blk_e);
			hw->src_blk_e = NULL;
		}
		/**/
		if (hw->src_blk_dct) {
			if (hw->flg_tvp_tgt)
				dct_idx = EBLK_T_LINK_DCT_TVP;
			else
				dct_idx = EBLK_T_LINK_DCT;
			blk_o_put_locked(d_dd, dct_idx, hw->src_blk_dct);
			hw->src_blk_dct = NULL;
		}
		mem_count_buf_cfg(0xff, 0);
		memset(&hw->blkt_n[0], 0, sizeof(hw->blkt_n));
		m_a_blkt_cp(0, &hw->blkt_n[0]);
		hw->m_mode_tgt = EPVPP_MEM_T_NONE;
		hw->m_mode	= hw->m_mode_tgt;
		hw->flg_tvp	= false;
		hw->flg_tvp_tgt	= false;
	} else {
		PR_WARN("%s:tgt_mode[%d]\n", __func__, tgt_mode);
	}

	if (back_tgt != hw->m_mode_tgt || back_tvp != hw->flg_tvp_tgt)
		dbg_plink1("%s:<%d %d>-><%d %d>:%d\n", __func__,
			back_tgt, back_tvp,
			hw->m_mode_tgt, hw->flg_tvp_tgt,
			id);
}

int dpvpp_reg_prelink_sw(bool vpp_disable_async)
{
	unsigned int ton, ton_vpp, ton_di;
	struct platform_device *pdev;
	struct dim_pvpp_hw_s *hw;
//	ulong irq_flag = 0;

//	if (itf && itf->c.src_state & EDVPP_SRC_NEED_REG2)
//		return 0;

	hw = &get_datal()->dvs_pvpp.hw;
	//check:
	ton_vpp = atomic_read(&hw->link_on_byvpp);
	ton_di = atomic_read(&hw->link_on_bydi);
	if (ton_vpp == ton_di)
		ton = ton_vpp;
	else
		ton = 0;

	if (ton == atomic_read(&hw->link_sts)) {
		if (atomic_read(&hw->link_in_busy))
			PR_ERR("%s: error state: check:<ton_vpp %d, ton_di %d> hw->link_sts:%d\n",
				__func__, ton_vpp, ton_di, atomic_read(&hw->link_sts));
		return 0;
	}

	if (ton) {
		//bypass other ch:
		get_datal()->pre_vpp_active = true;/* interface */
		hw->has_notify_vpp = false;
		if (dim_check_exit_process()) {
		//can sw:
			di_unreg_setting(true);
			if (dpvpp_set_one_time()) {
				pdev = get_dim_de_devp()->pdev;
				if (!pdev)
					return false;
				/* hw set */
				dpvpph_reg_setting(hw->op_n);
				/* reset to avoid flicker on T5D VB afbcd */
				dpvpph_pre_frame_reset_g12(true, hw->op_n);
			}

			atomic_set(&hw->link_sts, ton);//on
			get_datal()->pre_vpp_set = true; /* interface */
			atomic_set(&hw->link_in_busy, 0);
			ext_vpp_plink_state_changed_notify();
			PR_INF("%s:set active:<%d, %d> op:%px\n",
			       __func__, ton_vpp, ton_di, hw->op_n);
		}
	} else {
		bool sw_done = false;

		//off:
		/*check if bypass*/
		if (!ton_di && !hw->has_notify_vpp) {
			/* non-block */
			ext_vpp_disable_plink_notify(vpp_disable_async);
			ext_vpp_plink_real_sw(false, false, false);
			hw->has_notify_vpp = true;
			sw_done = true;
		}
		if (hw->dis_last_para.dmode != EPVPP_DISPLAY_MODE_BYPASS) {
			PR_INF("wait for bypass\n");
			return 0;
		}
		if (!sw_done && !ton_di)
			ext_vpp_plink_real_sw(false, true, false);
		atomic_set(&hw->link_sts, 0);//on
		get_datal()->pre_vpp_active = false;/* interface */
		atomic_set(&hw->link_in_busy, 0);
#ifdef _HIS_CODE_	//move 2024-08-19
		spin_lock_irqsave(&lock_pvpp, irq_flag);
		hw->blk_used_hd = false;
		hw->blk_used_uhd = false;
		spin_unlock_irqrestore(&lock_pvpp, irq_flag);
#endif
		ext_vpp_plink_state_changed_notify();
		PR_INF("%s:set inactive<%d, %d> arb:%x\n", __func__,
				ton_vpp, ton_di, RD(DI_ARB_DBG_STAT_L1C1));
	}
	return 0;
}

//static
bool ext_is_fps_l_30(void)
{
	int output_fps, output_pts_inc_scale = 0, output_pts_inc_scale_base = 0;
	bool ret = false;
	static bool last_st;

	if (dpvpp_dbg_tnr_disable())
		return false;

	get_output_pcrscr_info(&output_pts_inc_scale, &output_pts_inc_scale_base);
	if (!output_pts_inc_scale_base) {
		PR_WARN("get output pcrscr info failed.\n");
		output_fps = 0;
	} else {
		output_fps = 90000 * 16 * (u64)output_pts_inc_scale;
		output_fps = div64_u64(output_fps, output_pts_inc_scale_base);
	}
	dbg_plink2("scale: %d, base: %d, output_fps is %d.\n",
		output_pts_inc_scale, output_pts_inc_scale_base, output_fps);
	if (output_fps >= 48000) { //rate is < 30Hz
		dbg_plink2("output fps < 30\n");
		ret = true;
	}

	if (last_st != ret) {
		dbg_plink1("snr:ext tnr :%d -> %d\n", last_st, ret);
		last_st = ret;
	}
	return ret;
}

//old:
static void count_4k_old(struct dimn_itf_s *itf, struct dvfm_s *vf)
{
	unsigned char mode_back; //debug only
	struct dim_pvpp_ds_s *ds;

	if (!itf || !vf || VFMT_IS_EOS(vf->vfs.type) || !itf->ds)
		return;

	if (vf->is_4k)
		itf->c.sum_4k++;

	/*to-do : for more format */
	mode_back = itf->c.m_mode_n;//debug only
	if (itf->c.m_mode_n != EPVPP_MEM_T_UHD_AFBCE) {
		ds = itf->ds;
		if (itf->c.sum_4k && itf->ds->en_4k)
			itf->c.m_mode_n = ds->m_mode_n_uhd;
		else
			itf->c.m_mode_n = ds->m_mode_n_hd;
	}

	if (dimp_get(edi_mp_di_debug_flag) & 0x100000) {
		itf->c.m_mode_n = EPVPP_MEM_T_NONE;
	}

	if (mode_back != itf->c.m_mode_n) {
		dbg_plink1("ch[%d]:mode:%d->%d\n",
			itf->bind_ch, mode_back, itf->c.m_mode_n);
		//dpvpp_mem_mng_get(1);//
	}
}

/*****************************************************
 *	count 4k input for enable_4k and enable 4k_snr
 *	new
 *****************************************************/
void count_4k_snr(struct dimn_itf_s *itf, struct dvfm_s *vf)
{
	struct dim_pvpp_ds_s *ds;
	struct dim_pvpp_hw_s *hw;
	unsigned long delay;
	bool trig_alloc = false;
	bool trig_release = false;
	unsigned char mode = EPVPP_MEM_T_NONE;
	bool cur_tnr = false;
	bool old_tnr;
	unsigned char mode_back; //debug only
	unsigned char tmp_snr_st = 0;

	if (!itf || !vf || VFMT_IS_EOS(vf->vfs.type) || !itf->ds)
		return;

	ds = itf->ds;
	hw = &get_datal()->dvs_pvpp.hw;

	//ext_is_fps_l_30(); //test only
	if (dimp_get(edi_mp_di_debug_flag) & 0x100000 &&
	    ds->snr_st == 0) { //only can used for reg
		//itf->c.m_mode_n = EPVPP_MEM_T_NONE;
		if (itf->c.m_mode_n != EPVPP_MEM_T_NONE) {
			dbg_plink1("ch[%d]:mode:%d->none\n",
				itf->bind_ch, itf->c.m_mode_n);
			itf->c.m_mode_n = EPVPP_MEM_T_NONE;
			//dpvpp_mem_mng_get(1);//
		}
		return;
	}

	if (vf->is_4k)
		itf->c.sum_4k++;

	if (ext_is_fps_l_30() || dpvpp_dbg_4k_tsnr())
		cur_tnr = true;

	if (vf->is_4k) { //note: follow before tnr status
		if (ds->en_4k_tnr)
			vf->en_4k_snr = false;
		else
			vf->en_4k_snr = true;
	} else {
		vf->en_4k_snr = false;
	}

	//init:
	if (ds->snr_st == 0) { //init
		if (itf->c.sum_4k && cur_tnr) {
			mode = ds->m_mode_n_uhd;
			ds->snr_st = 0x2;
		} else {
			mode = ds->m_mode_n_hd;
			ds->snr_st = 0x1;
		}
		itf->c.m_mode_n = mode;

		dbg_plink1("snr:[%d,%d,%d,0x%x,%d]\n", cur_tnr, ds->en_4k_tnr,
			vf->en_4k_snr, ds->snr_st, itf->c.m_mode_n);
		return;
	}

	tmp_snr_st = ds->snr_st;
	old_tnr = ds->en_4k_tnr;
	if (ds->snr_st == 1 && hw->blk_rd_hd)
		ds->snr_st = 0x11;
	else if (ds->snr_st == 2 && hw->blk_rd_uhd)
		ds->snr_st = 0x12;
	else if (ds->snr_st == 4 && hw->blk_rd_hd)
		ds->snr_st = 0x11;
	else if (ds->snr_st == 5 && !hw->blk_rd_hd && !hw->blk_rd_uhd)
		ds->snr_st = 0x10;
	if (tmp_snr_st != ds->snr_st) {
		//change:
		dbg_plink1("snr:st 0x%x->0x%x\n", tmp_snr_st, ds->snr_st);
	}

	if (ds->snr_st < 0x0f)
		return;

	mode_back = itf->c.m_mode_n;

	if (cur_tnr != ds->en_4k_tnr &&
	    vf->is_4k) { //change for input tnr and snr_st
		//dbg_plink1("snr:tnr chg %d-%d\n", ds->en_4k_tnr, cur_tnr);
		if (ds->en_4k_tnr) {
			// from tnr to snr
			ds->en_4k_tnr = false;
			vf->en_4k_snr = true;
			//wait for release buffer
		} else {
			//from snr to tnr?
			if (hw->blk_rd_uhd &&
			    ds->snr_st == 0x12) {
				ds->en_4k_tnr = true;
			} else {
				if (ds->snr_st == 0x10 ||
				    ds->snr_st == 0x11) {
					//trig alloc buffer, and need wait
					dbg_plink1("snr:trig alloc 4k buffer:\n");
					trig_alloc = true;
				} else {
					dbg_plink1("snr:wait:0x%x\n", ds->snr_st);
				}
			}
		}
	} else if (!vf->is_4k) {
		ds->en_4k_tnr = cur_tnr;
	}

	//release count timer:
	if (vf->en_4k_snr && ds->snr_st == 0x12) {
		if (!ds->en_snr_tm) {
			//begin to time:
			ds->jiff_mem = jiffies;
			ds->en_snr_tm = true;
		} else if (hw->blk_used_hd	||
			   hw->blk_used_uhd) { //reset timer
			ds->jiff_mem = jiffies;
		} else {
			delay = ds->jiff_mem + 3 * HZ;
			if (time_after(jiffies, delay)) {
				dbg_plink1("snr:trig release 4k buffer\n");
				trig_release = true;
			}
		}
	} else {
		ds->en_snr_tm = false;
	}

	if (trig_alloc) {
		ds->snr_st = 2;
		itf->c.m_mode_n = ds->m_mode_n_uhd;
	} else if (trig_release) {
		ds->snr_st = 4;
		hw->blk_rd_uhd = false;
		hw->blk_rd_hd  = false;
		itf->c.m_mode_n = ds->m_mode_n_hd;
	}
	if (cur_tnr != ds->en_4k_tnr ||
	    old_tnr != ds->en_4k_tnr ||
	    tmp_snr_st != ds->snr_st ||
	    mode_back != itf->c.m_mode_n)
		dbg_plink1("snr:[%d,%d,%d,0x%x,%d]\n", cur_tnr, ds->en_4k_tnr,
				vf->en_4k_snr, ds->snr_st, itf->c.m_mode_n);
}

static void count_4k(struct dimn_itf_s *itf, struct dvfm_s *vf)
{
	struct dim_pvpp_ds_s *ds;

	if (!itf || !vf || VFMT_IS_EOS(vf->vfs.type) || !itf->ds)
		return;
	ds = itf->ds;
	if (!ds->en_4k_snr || !ds->en_4k)
		return count_4k_old(itf, vf);
	count_4k_snr(itf, vf);
}

static unsigned int vtype_fill_bypass(struct vframe_s *vf)
{
	unsigned int ds_ratio = 0;
	unsigned int hdctds_ratio = 0;
	unsigned int src_fmt = 2;
	unsigned int skip = 0;
	bool need_ds = false;
	bool memuse_org = true;

	if ((vf->type & VIDTYPE_VIU_422) && !(vf->type & 0x10000000)) {
		src_fmt = 0;
		need_ds = true;
		/*422 is one plane, post not support, need pre out nv21*/
	} else if ((vf->type & VIDTYPE_VIU_NV21) ||
		   (vf->type & 0x10000000)) {
		/*hdmi in dw is nv21 VIDTYPE_DW_NV21*/
		src_fmt = 2;
	}

	if (vf->type & VIDTYPE_INTERLACE) {
		if (src_fmt == 2) {
			skip = 1;
		} else if (src_fmt == 0) {
			need_ds = true;
		/*hdmiin output, In the first half of the line*/
			if (vf->width > 960 ||
			    (vf->height >> 1) > 540)
				hdctds_ratio = 1;
		}
	} else {
		if (vf->width > 1920 || vf->height > 1080) {
			hdctds_ratio = 1;
			skip = 1;
		} else if (vf->width > 960 || vf->height > 540) {
			if (src_fmt == 0) {
				/*hdmi in always use ds*/
				hdctds_ratio = 1;
			} else {
				/*decoder use mif skip for save ddr*/
				hdctds_ratio = 0;
				skip = 1;
				dbg_dctp("1080p use mif skip\n");
			}
		}
	}

	if (hdctds_ratio || skip || need_ds)
		memuse_org = false;

	if (!memuse_org) {
		ds_ratio = hdctds_ratio;
		if (skip)
			ds_ratio = ds_ratio + 1;

		if (need_ds && (vf->type & VIDTYPE_COMPRESS))
			ds_ratio = (vf->compWidth / vf->width) >> 1;
	}

	if (memuse_org) {
		if (vf->type & VIDTYPE_COMPRESS) {
			if (vf->width == vf->compWidth)
				ds_ratio = 0;
			else if (vf->width >= (vf->compWidth >> 1))
				ds_ratio = 1;
			else if (vf->width >= (vf->compWidth >> 2))
				ds_ratio = 2;
			else
				ds_ratio = 3;
		}
	}
	dbg_poll("ds_ratio=%d, skip=%d,memuse_org=%d,need_ds=%d,src_fmt=%d\n", ds_ratio,
		skip, memuse_org, need_ds, src_fmt);
	return ds_ratio;
}

/* cnt: for */
static bool dpvpp_parser_bypass(struct dimn_itf_s *itf,
				unsigned int cnt)
{
	struct dimn_qs_cls_s *qin, *qready, *qidle;
	struct dimn_dvfm_s *dvfm = NULL;
	struct vframe_s *vf, *vf_ori;
	struct di_buffer *buffer_ori;
	unsigned int cnt_in;
	int i;
	unsigned int ds_ratio = 0;
	struct dim_pvpp_ds_s *ds;
	struct dev_vfram_t *pvfm;

	if (!itf || !itf->ds)
		return false;
	ds = itf->ds;
	pvfm = &itf->dvfm;

	qin	= &ds->lk_que_in;
	qready	= &ds->lk_que_ready;
	qidle	= &ds->lk_que_idle;

	cnt_in = qin->ops.count(qin);
	if (!cnt_in || qready->ops.is_full(qready))
		return false;
	if (cnt > 0)
		cnt_in = cnt;
	for (i = 0; i < cnt_in; i++) {
		vf = qin->ops.get(qin);
		if (!vf || !vf->private_data) {
			PR_ERR("%s:qin get null?\n", __func__);
			break;
		}

		dvfm = (struct dimn_dvfm_s *)vf->private_data;
		if (itf->etype == EDIM_NIN_TYPE_VFM) {
			vf_ori = (struct vframe_s *)dvfm->c.ori_in;
			ds_ratio = vtype_fill_bypass(vf_ori);
			vf_ori->di_flag |= DI_FLAG_DI_PVPPLINK_BYPASS | DI_FLAG_DI_BYPASS;
			ds_ratio = ds_ratio << DI_FLAG_DCT_DS_RATIO_BIT;
			ds_ratio &= DI_FLAG_DCT_DS_RATIO_MASK;
			vf_ori->di_flag &= ~DI_FLAG_DCT_DS_RATIO_MASK;
			vf_ori->di_flag |= ds_ratio;
			vf_ori->private_data = NULL;

			qidle->ops.put(qidle, vf);
			qready->ops.put(qready, vf_ori);
			vf_notify_receiver(pvfm->name,
				VP_EVENT(VFRAME_READY), NULL);
			if (qready->ops.is_full(qready))
				break;
		} else {
			/*EDIM_NIN_TYPE_INS*/
			buffer_ori = (struct di_buffer *)dvfm->c.ori_in;
			buffer_ori->flag |= DI_FLAG_BUF_BY_PASS;
			if (buffer_ori->vf) {
				ds_ratio = vtype_fill_bypass(buffer_ori->vf);
				buffer_ori->vf->di_flag |= DI_FLAG_DI_PVPPLINK_BYPASS;
				ds_ratio = ds_ratio << DI_FLAG_DCT_DS_RATIO_BIT;
				ds_ratio &= DI_FLAG_DCT_DS_RATIO_MASK;
				buffer_ori->vf->di_flag &= ~DI_FLAG_DCT_DS_RATIO_MASK;
				buffer_ori->vf->di_flag |= ds_ratio;
			}
			qidle->ops.put(qidle, vf);
			qready->ops.put(qready, buffer_ori);
			if (qready->ops.is_full(qready))
				break;
		}
	}
	return true;
}

static bool vf_m_in(struct dimn_itf_s *itf)
{
	struct dimn_qs_cls_s *qidle, *qin, *qr;
	struct dimn_dvfm_s *ndvfm;
	struct dim_pvpp_ds_s *ds = NULL;
	unsigned int in_nub, free_nub;
	int i;
	struct vframe_s *vf, *mvf;
	bool flg_q;
	unsigned int err_cnt = 0;
#ifdef DBG_TIMER
	u64 ustime;

	//ustime = cur_to_usecs();
	ustime	= cur_to_msecs();
#endif

	if (!itf || !itf->ds) {
		PR_ERR("%s:\n", __func__);
		return false;
	}
	ds = itf->ds;
	if (itf->c.pause_pre_get) /* stop input get */
		return false;
	qidle	= &ds->lk_que_idle;
	qin	= &ds->lk_que_in;
	qr	= &ds->lk_que_recycle;
	in_nub		= qin->ops.count(qin);
	free_nub	= qidle->ops.count(qidle);

	if (in_nub >= DIM_K_VFM_IN_LIMIT	||
	    (free_nub < (DIM_K_VFM_IN_LIMIT - in_nub))) {
		return false;
	}

	for (i = 0; i < (DIM_K_VFM_IN_LIMIT - in_nub); i++) {
#ifdef pre_dbg_is_run
		if (!pre_dbg_is_run(itf->bind_ch)) //p_pause
			break;
#endif
		vf = in_patch_first_buffer(itf);
		if (!vf)
			break;
		mvf = (struct vframe_s *)qidle->ops.get(qidle);

		if (!mvf || !mvf->private_data) {
			PR_ERR("%s:qout:%d:0x%px\n",
				__func__,
				qidle->ops.count(qidle),
				mvf);
			err_cnt++;
			in_vf_put(itf, vf);
			break;
		}

		/* dbg only */
		dbg_check_vf(ds, mvf, 1);
		if (dimp_get(edi_mp_force_width))
			vf->width = dimp_get(edi_mp_force_width);

		if (dimp_get(edi_mp_force_height))
			vf->height = dimp_get(edi_mp_force_height);

		/* dbg end **************************/
		ndvfm = (struct dimn_dvfm_s *)mvf->private_data;
		dim_print("%s:nvfm:0x%px\n", __func__, ndvfm);
		memset(&ndvfm->c, 0, sizeof(ndvfm->c));
		ndvfm->c.ori_in = vf;
		ndvfm->c.ori_vf = vf;
		ndvfm->c.cnt_in = itf->c.sum_pre_get;
		if (vf->decontour_pre) {
			ndvfm->c.dct_pre = vf->decontour_pre;
			vf->decontour_pre = NULL;
			ds->dct_sum_in++;
		}
		memcpy(&ndvfm->c.vf_in_cp, vf, sizeof(ndvfm->c.vf_in_cp));
		dim_dvf_cp(&ndvfm->c.in_dvfm, &ndvfm->c.vf_in_cp, 0);

		if (IS_COMP_MODE(ndvfm->c.in_dvfm.vfs.type)) {
			ndvfm->c.in_dvfm.vfs.width = vf->compWidth;
			ndvfm->c.in_dvfm.vfs.height = vf->compHeight;
		}
		ndvfm->c.src_w = ndvfm->c.in_dvfm.vfs.width;
		ndvfm->c.src_h = ndvfm->c.in_dvfm.vfs.height;
		count_4k(itf, &ndvfm->c.in_dvfm);
		flg_q = qin->ops.put(qin, mvf);

		if (!flg_q) {
			PR_ERR("%s:qin\n", __func__);
			err_cnt++;
			in_vf_put(itf, vf);
			qidle->ops.put(qidle, mvf);
			break;
		}
#ifdef DBG_TIMER
		if (itf->c.sum_pre_get < 4)
			dbg_mem2("get[%d]:%lu\n",
				 itf->c.sum_pre_get, (unsigned long)ustime);
#endif
	}

	if (err_cnt)
		return false;
	return true;
}

static enum DI_ERRORTYPE dpvpp_empty_input_buffer(struct dimn_itf_s *itf,
					   struct di_buffer *buffer)
{
	struct dim_pvpp_ds_s *ds = NULL;
//	struct dimn_itf_s *itf;
	struct dimn_qs_cls_s *qidle, *qin;
	unsigned int free_nub;
	struct vframe_s *vf, *mvf;
	struct dimn_dvfm_s *ndvfm;
	struct di_buffer *buffer_l;
	unsigned int err_cnt = 0;
	bool flg_q;

	if (!itf || !itf->ds) {
		PR_ERR("%s:no itf:0x%px\n", __func__, itf);
		return DI_ERR_INDEX_NOT_ACTIVE;
	}

	ds	= itf->ds;
	if (!atomic_read(&itf->reg) ||
	    atomic_read(&itf->in_unreg) || !ds) {
		PR_WARN("%s:not reg(%d:%d); ds:%px\n",
			__func__,
			atomic_read(&itf->reg),
			atomic_read(&itf->in_unreg),
			ds);
		return DI_ERR_INDEX_NOT_ACTIVE;
	}

	qidle	= &ds->lk_que_idle;
	qin	= &ds->lk_que_in;

	free_nub	= qidle->ops.count(qidle);

	if (!free_nub || qin->ops.is_full(qin)) {
		PR_ERR("%s:queue full. free_nub:%d\n", __func__, free_nub);
		return DI_ERR_IN_NO_SPACE;
	}

	mvf = (struct vframe_s *)qidle->ops.get(qidle);

	if (!mvf || !mvf->private_data) {
		PR_ERR("%s:qout:%d:0x%px\n",
			__func__,
			qidle->ops.count(qidle),
			mvf);
		err_cnt++;
		/* to do bypass ?*/
		return DI_ERR_IN_NO_SPACE; //?
	}

	dbg_check_vf(ds, mvf, 1);
	ndvfm = (struct dimn_dvfm_s *)mvf->private_data;
	dim_print("%s:nvfm:0x%px\n", __func__, ndvfm);
	memset(&ndvfm->c, 0, sizeof(ndvfm->c));
	ndvfm->c.ori_in = buffer;
	ndvfm->c.cnt_in = itf->c.sum_pre_get;
	if (buffer->vf && buffer->vf->decontour_pre) {
		ndvfm->c.dct_pre = buffer->vf->decontour_pre;
		buffer->vf->decontour_pre = NULL;
		ds->dct_sum_in++;
	}
	/* clear */
	buffer_l = &itf->buf_bf[mvf->index];
	buffer_l->flag		= 0;
	buffer_l->crcout	= 0;
	buffer_l->nrcrcout	= 0;
//#if defined(DBG_QUE_IN_OUT) || defined(DBG_QUE_INTERFACE)
	dbg_plink2("ins:in:0x%px:%d:0x%x\n",
		   buffer, ndvfm->c.cnt_in, buffer->flag);
	dbg_plink2("ins:buffer[0x%px:0x%x]:vf[0x%px:0x%x]\n",
				buffer, buffer->flag,
				buffer->vf, buffer->vf->type);

//#endif

	itf->c.sum_pre_get++;
	if (!buffer->vf && !(buffer->flag & DI_FLAG_EOS)) {
		PR_ERR("%s:no vf:0x%px\n", __func__, buffer);
		qidle->ops.put(qidle, mvf);
		itf->nitf_parm.ops.empty_input_done(buffer);
		return DI_ERR_UNSUPPORT;
	}
	if (buffer->flag & DI_FLAG_EOS) {
		buffer->flag |= DI_FLAG_BUF_BY_PASS;
		ndvfm->c.in_dvfm.vfs.type |= VIDTYPE_V4L_EOS;//23-02-15
	} else {
		vf = buffer->vf;
		ndvfm->c.ori_vf = vf;
		memcpy(&ndvfm->c.vf_in_cp, vf, sizeof(ndvfm->c.vf_in_cp));
		ndvfm->c.in_dvfm.caller_mng = buffer->caller_mng;
		dim_dvf_cp(&ndvfm->c.in_dvfm, vf, 0);
		count_4k(itf, &ndvfm->c.in_dvfm);
		if (IS_COMP_MODE(ndvfm->c.in_dvfm.vfs.type)) {
			ndvfm->c.in_dvfm.vfs.width = vf->compWidth;
			ndvfm->c.in_dvfm.vfs.height = vf->compHeight;
		}
		ndvfm->c.src_w = ndvfm->c.in_dvfm.vfs.width;
		ndvfm->c.src_h = ndvfm->c.in_dvfm.vfs.height;
		//buffer_l->phy_addr	= vf->canvas0_config[0].phy_addr;//tmp
		dim_print("%s:0x%lx\n", __func__,
			  (unsigned long)vf->canvas0_config[0].phy_addr);
		dbg_link("empty pre %x,%lx,frame_index=%d,queued=%d,dropped=%d,dummy=%d,file:%px\n",
			vf->type,
			vf->canvas0_config[0].phy_addr,
			vf->frame_index,
			buffer->caller_mng.queued,
			buffer->caller_mng.dropped,
			buffer->caller_mng.dummy,
			buffer->caller_mng.src_file);

	}
	flg_q = qin->ops.put(qin, mvf);

	if (!flg_q) {
		PR_ERR("%s:qin\n", __func__);
		err_cnt++;
		//in_vf_put(itf, vf);
		/* how to return di_buffer ?*/
		qidle->ops.put(qidle, mvf);
		return DI_ERR_IN_NO_SPACE; //?
	}

	task_send_wk_timer(EDIM_WKUP_REASON_IN_HAVE);

	return DI_ERR_NONE;
}

static void dpvpp_patch_first_buffer(struct dimn_itf_s *itf)
{
	int i;
	unsigned int cnt1, cnt2;
	struct dim_pvpp_ds_s *ds = NULL;
	struct dimn_qs_cls_s *qidle;
	struct buf_que_s *pbufq;
	union q_buf_u q_buf;
	struct dim_nins_s *ins;
	bool ret;
	unsigned int qt_in;
	unsigned int bindex;
	struct di_buffer *buffer;
	struct di_ch_s *pch;
//	struct dcntr_mem_s	*pdcn; //tmp

	if (!itf || !itf->ds) {
		PR_ERR("%s:no itf:0x%px\n", __func__, itf);
		return;
	}

	ds	= itf->ds;
	if (!atomic_read(&itf->reg) ||
	    atomic_read(&itf->in_unreg) || !ds) {
		PR_WARN("%s:not reg(%d:%d); ds:%px\n",
			__func__,
			atomic_read(&itf->reg),
			atomic_read(&itf->in_unreg),
			ds);
		return;
	}

	if (itf->bind_ch >= DI_CHANNEL_MAX) {
		PR_ERR("%s:ch[%d] overflow\n", __func__, itf->bind_ch);
		return;
	}

	pch = get_chdata(itf->bind_ch);
	if (!pch) {
		PR_ERR("%s:no pch:id[%d]\n", __func__, itf->bind_ch);
		return;
	}

	pbufq = &pch->nin_qb;
	qt_in = QBF_NINS_Q_CHECK;
	cnt1 = nins_cnt(pch, qt_in);
	if (!cnt1) {
		dbg_plink3("%s:no input buffer\n", __func__);
		return;
	}

	qidle = &ds->lk_que_idle;
	cnt2 = qidle->ops.count(qidle);
	if (!cnt2) {
		dbg_plink3("%s:no free nub. input buffer:%d\n", __func__, cnt1);
		return;
	}

	if (cnt1 > cnt2)
		cnt1 = cnt2;
	for (i = 0; i < cnt1; i++) {
		ret = qbuf_out(pbufq, qt_in, &bindex);
		if (!ret) {
			PR_ERR("%s:1:%d:can't get out\n", __func__, i);
			continue;
		}
		if (bindex >= DIM_NINS_NUB) {
			PR_ERR("%s:2:%d:%d\n", __func__, i, bindex);
			continue;
		}
		q_buf = pbufq->pbuf[bindex];
		ins = (struct dim_nins_s *)q_buf.qbc;

		buffer = (struct di_buffer *)ins->c.ori;
		if (!buffer) {
			PR_ERR("%s:3:%d,%d\n", __func__, i, bindex);
			continue;
		}
		if (buffer->vf) {
			buffer->vf->decontour_pre = ins->c.vfm_cp.decontour_pre;
			buffer->caller_mng = ins->c.caller_mng;
		} else {
			PR_WARN("%s:no vf:0x%px,0x%x\n",
				__func__, buffer,
				buffer->flag);
		}
		ins->c.vfm_cp.decontour_pre = NULL;
		memset(&ins->c, 0, sizeof(ins->c));
		qbuf_in(pbufq, QBF_NINS_Q_IDLE, bindex);

		dpvpp_empty_input_buffer(itf, buffer);
	}
	//PR_INF("%s:%d\n", __func__, cnt1);
}

static bool dpvpp_parser(struct dimn_itf_s *itf)
{
	/* reg 2 */
	if (!itf ||
	    !itf->c.reg_di || !itf->ds || itf->c.pause_parser)
		return false;
	if (itf->c.src_state & EDVPP_SRC_NEED_MSK_CRT)
		return false;

	/* input polling */
	if (itf->etype == EDIM_NIN_TYPE_VFM)
		vf_m_in(itf);
	else
		dpvpp_patch_first_buffer(itf);

	if (!pre_dbg_is_run()) //p pause
		return true;

	if ((itf->c.src_state & EDVPP_SRC_NEED_MSK_BYPASS) ||
	   dpvpp_dbg_force_bypass_2()) {
		if (itf->c.src_state & EDVPP_SRC_NEED_MSK_BYPASS)
			itf->c.bypass_reason = EPVPP_NO_MEM;
		dpvpp_parser_bypass(itf, 0);
		dpvpp_recycle_back(itf);
		dim_print("%s:SRC_NEED_MSK_BYPASS:0x%x\n", __func__, itf->c.src_state);
		return true;
	}

	dpvpp_recycle_back(itf);

	/* */
	dpvpp_parser_nr(itf);

	return true;
}

bool dpvpp_pre_process(void *para)
{
	struct dimn_itf_s *itf;
	int ch;
	unsigned int cnt_active = 0;
	unsigned int freeze = 0;

	for (ch = 0; ch < DI_PLINK_CN_NUB; ch++) {
		itf = &get_datal()->dvs_pvpp.itf[ch];
		if (itf->link_mode == EPVPP_API_MODE_POST)
			continue;
		dpvpp_mem_reg_ch(itf);

		dpvpp_parser(itf);

		if (!itf->ds ||
		    !atomic_read(&itf->reg) ||
		    atomic_read(&itf->in_unreg))
			continue;
		cnt_active++;
		if (itf->flg_freeze)
			freeze++;
		dpvpp_ins_fill_out(itf);

		if (!(itf->c.src_state &
		      (EDVPP_SRC_NEED_MSK_CRT | EDVPP_SRC_NEED_MSK_BYPASS))) {
			if (dpvpp_que_sum(itf)) {
				if (itf->ds && itf->ds->sum_que_in)
					task_send_wk_timer(EDIM_WKUP_REASON_IN_L);
			}
		}
	}

	if (dpvpp_is_not_active_di() || dim_polic_is_prvpp_bypass())
		dpvpp_link_sw_by_di(false);
	else if (!freeze && cnt_active)
		dpvpp_link_sw_by_di(true);
	else if (!cnt_active)
		dpvpp_link_sw_by_di(false);
	dpvpp_reg_prelink_sw(true);
	dpvpp_mem_mng_get(2);
	dpvpp_mng_set();

	dpvpp_mem_dct();
	return true;
}

int dpvpp_get_plink_input_win(struct di_ch_s *pch,
		unsigned int src_w, unsigned int src_h, struct di_win_s *out)
{
	struct di_win_s *win_c;
	struct dim_pvpp_hw_s *hw;
	struct di_win_s tmp;
	u32 val;
	//ulong irq_flag = 0;
	int ret = 0;

	if (!pch) {
		PR_ERR("%s:1\n", __func__);
		return -1;
	}

	if (!pch->itf.p_vpp_link) {
		dbg_plink3("%s: ch[%d] is not prelink mode\n",
			__func__, pch->ch_id);
		return -10;
	}
	//spin_lock_irqsave(&lock_pvpp, irq_flag);
	hw = &get_datal()->dvs_pvpp.hw;
	if (!out ||
	    hw->dis_ch != pch->ch_id ||
	    !atomic_read(&hw->link_sts)) {
		dbg_plink2("%s:2 %d %d %px, sts:%d\n",
			__func__, hw->dis_ch, pch->ch_id,
			out, atomic_read(&hw->link_sts) ? 1 : 0);
		//spin_unlock_irqrestore(&lock_pvpp, irq_flag);
		return -1;
	}

	if (dpvpp_dbg_force_no_crop())
		return -2;

	/* copy current win para */
	win_c = &hw->dis_c_para.win;
	memcpy(&tmp, win_c, sizeof(*win_c));
	/* x size check */
	val = (tmp.x_check_sum >> 16) & 0xffff;
	if (val != tmp.x_size)
		ret++;
	val = tmp.x_check_sum & 0xffff;
	if (val != ((tmp.x_st + tmp.x_end) & 0xffff))
		ret++;

	/* y size check */
	val = (tmp.y_check_sum >> 16) & 0xffff;
	if (val != tmp.y_size)
		ret++;
	val = tmp.y_check_sum & 0xffff;
	if (val != ((tmp.y_st + tmp.y_end) & 0xffff))
		ret++;

	if (ret)
		PR_WARN("%s: win info incompleted (%d %d; %d %d) x:%x %x %x %x; y:%x %x %x %x\n",
			__func__, src_w, src_h, tmp.orig_w, tmp.orig_h,
			tmp.x_st, tmp.x_end, tmp.x_size, tmp.x_check_sum,
			tmp.y_st, tmp.y_end, tmp.y_size, tmp.y_check_sum);
	/* just skip warning if only source size change */
	if (src_w != tmp.orig_w)
		ret++;
	if (src_h != tmp.orig_h)
		ret++;

	if (!ret)
		memcpy(out, &tmp, sizeof(*out));
	//spin_unlock_irqrestore(&lock_pvpp, irq_flag);
	return ret;
}

static void dpvpp_set_default_para(struct dim_pvpp_ds_s *ds,
				   struct dimn_dvfm_s *ndvfm)
{
	struct pvpp_dis_para_in_s  *dis_para_demo;
	struct dvfm_s *in_dvfm;

	dis_para_demo = &ds->dis_para_demo;
	in_dvfm		= &ndvfm->c.in_dvfm;
	memset(dis_para_demo, 0, sizeof(*dis_para_demo));
	dis_para_demo->dmode = EPVPP_DISPLAY_MODE_NR;
	dis_para_demo->win.x_size = in_dvfm->vfs.width;
	dis_para_demo->win.y_size = in_dvfm->vfs.height;
	dis_para_demo->win.x_end = dis_para_demo->win.x_size - 1;
	dis_para_demo->win.y_end = dis_para_demo->win.y_size - 1;
#ifdef DBG_FLOW_SETTING
	print_pvpp_dis_para_in(dis_para_demo, "dis_para_demo");
#endif
}

static void dpvpp_cnt_para(struct pvpp_dis_para_in_s  *dis_para_demo,
			struct vframe_s *vfm,
			unsigned int x_off,
			unsigned int y_off)
{
	unsigned int x, y;

	if (!dis_para_demo || !vfm) {
		PR_ERR("%s:null\n", __func__);
		return;
	}
	dim_vf_x_y(vfm, &x, &y);
	memset(dis_para_demo, 0, sizeof(*dis_para_demo));
	dis_para_demo->dmode = EPVPP_DISPLAY_MODE_NR;
	dis_para_demo->win.x_st	= x_off;
	dis_para_demo->win.y_st	= y_off;
	dis_para_demo->win.x_size = x - (x_off << 1);
	dis_para_demo->win.y_size = y - (y_off << 1);
	dis_para_demo->win.x_end = dis_para_demo->win.x_st +
				dis_para_demo->win.x_size - 1;
	dis_para_demo->win.y_end = dis_para_demo->win.y_st +
				dis_para_demo->win.y_size - 1;
#ifdef DBG_FLOW_SETTING
	print_pvpp_dis_para_in(dis_para_demo, "dis_para_demo");
#endif
}

int dpvpp_pre_unreg_bypass(void)
{
	struct dim_pvpp_hw_s *hw;

	hw = &get_datal()->dvs_pvpp.hw;

	if (hw->dis_last_para.dmode != EPVPP_DISPLAY_MODE_BYPASS) {
		dbg_plink1("%s:trig dis_ch:%d->255\n", __func__, hw->dis_ch);
		hw->dis_ch = 0xff;
		hw->last_dct_bypass = true;
		/* from pre-vpp link to bypass */
		//be sure vpp can display
		dim_afds()->pvpp_sw_setting_op(false, hw->op);
		//dpvpph_pre_data_mif_ctrl(false, hw->op);
		dpvpph_reverse_mif_ctrl(0, 0, hw->op);
		opl1()->pre_mif_sw(false, hw->op, true);
		dpvpph_prelink_sw(hw->op, false);
	}
	if (hw->dis_ch != hw->dct_ch && !hw->dct_flg) {
		hw->dct_flg = 2;
		dbg_plink1("dis:trig2:dct unreg dis_ch:%d dct_ch:%d\n",
			hw->dis_ch, hw->dct_ch);
	}
	hw->dis_last_para.dmode = EPVPP_DISPLAY_MODE_BYPASS;
	hw->dis_last_dvf	= NULL;
	if (hw->en_4k_snr) {
		dbg_plink1("snr:3:to bypass\n");
		hw->en_4k_snr = 0;
	}
	/* move to dpvpp_reg_prelink_sw */
	//hw->blk_used_hd = false;
	//hw->blk_used_uhd = false;
	atomic_set(&hw->link_instance_id, DIM_PLINK_INSTANCE_ID_INVALID);
	dbg_plink1("%s:end\n", "unreg_bypass");
	return EPVPP_DISPLAY_MODE_BYPASS;
}

/*copy from dim_pulldown_info_clear_g12a*/
void dpvpph_pulldown_info_clear_g12a(const struct reg_acc *op_in)
{
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		op_in->bwr(DI_PRE_CTRL, 1, 30, 1);
	dim_print("%s:\n", __func__);
}

int dpvpp_pre_display(struct vframe_s *vfm,
			 struct pvpp_dis_para_in_s *in_para,
			 void *out_para)
{
	struct dim_pvpp_ds_s *ds;
	struct dimn_itf_s *itf;
	struct dimn_dvfm_s *ndvfm = NULL;
	unsigned int	diff;/*enum EDIM_DVPP_DIFF*/
	struct vframe_s *vf_in/*, *vf_out*/;
	const struct reg_acc *op;
	struct pvpp_dis_para_in_s *para;
	bool dbg_tm = false;
	unsigned long delay;
	struct dim_pvpp_hw_s *hw;
	struct pvpp_buf_cfg_s *buf_cfg;
	u32 bypass_reason = 0;

	hw = &get_datal()->dvs_pvpp.hw;

	itf = get_itf_vfm(vfm);
	if (!itf || !itf->ds) {
		PR_ERR("%s:no data:0x%px:%d itf:%px\n",
			__func__, vfm, vfm->di_instance_id, itf);
		return EPVPP_ERROR_DI_NOT_REG;
	}
	ds = itf->ds;

	/* debug print */
	delay = itf->c.jiff_display + 1 * HZ;
	if (time_after(jiffies, delay)) {
		dbg_tm = true;
		itf->c.jiff_display = jiffies;
	}
	atomic_set(&itf->c.dbg_display_sts, 0);
	/*******/
	if (!ds || !vfm) {
		PR_ERR("%s:di not reg\n", __func__);
		if (!ds)
			PR_ERR("no ds\n");
		if (!vfm)
			PR_ERR("no vfm");
		return EPVPP_ERROR_DI_NOT_REG;
		//goto DISPLAY_BYPASS;
	}
	atomic_add(DI_BIT0, &itf->c.dbg_display_sts);	/* dbg only */
	if (!atomic_read(&hw->link_sts)) {
		bypass_reason = 1;
		goto DISPLAY_BYPASS;
	}
	/* remove by Brian 11-13*/
#ifdef HIS_CODE
	if (itf->flg_freeze) {	/* @ary 11-08 add */
		bypass_reason = 2;
		goto DISPLAY_BYPASS;
	}
#endif
	op = hw->op;
	if (itf->c.src_state &
	    (EDVPP_SRC_NEED_MSK_CRT | EDVPP_SRC_NEED_MSK_BYPASS)) {
		bypass_reason = 2;
		bypass_reason |= (itf->c.src_state << 16);
		goto DISPLAY_BYPASS;
	}

	if (!hw->blk_rd_uhd && !hw->blk_rd_hd && !ds->en_4k_snr) {
		bypass_reason = 3;
		PR_WARN("%s:src_state ready but blk invalid\n", __func__);
		goto DISPLAY_BYPASS;
	}

	atomic_add(DI_BIT1, &itf->c.dbg_display_sts);	/* dbg only */
	/* check if bypass */
	ndvfm = dpvpp_check_dvfm_act(itf, vfm);
	if (!ndvfm) {
		bypass_reason = 4;
		goto DISPLAY_BYPASS;
	}
	atomic_add(DI_BIT2, &itf->c.dbg_display_sts);	/* dbg only */
	dbg_check_ud(ds, 2); /* dbg */

	para = in_para;
	if (!in_para) {
		dpvpp_set_default_para(ds, ndvfm);
		para = &ds->dis_para_demo;
	}
	if (para->dmode == EPVPP_DISPLAY_MODE_BYPASS) {
		bypass_reason = 5;
		goto DISPLAY_BYPASS;
	}
	if (dpvpp_bypass_display()) { //dbg only
		bypass_reason = 6;
		goto DISPLAY_BYPASS;
	}
	/*check buffer config */
	buf_cfg = get_buf_cfg(ndvfm->c.out_fmt, 1);
	if (!buf_cfg) {
		bypass_reason = 7;
		goto DISPLAY_BYPASS;
	}
	if (ndvfm->c.is_out_4k && !hw->blk_rd_uhd &&
		/*!ds->en_4k_snr*/ !ndvfm->c.in_dvfm.en_4k_snr) {
		bypass_reason = 8;
		PR_WARN("%s:output buffer is mismatch, too small.\n", __func__);
		goto DISPLAY_BYPASS;
	} else if (!ndvfm->c.is_out_4k && !hw->blk_rd_hd) {
		bypass_reason = 9;
		PR_WARN("%s:no buffer.\n", __func__);
		goto DISPLAY_BYPASS;
	}
#ifdef _HIS_CODE_	//move to dpvpph_size_change
	if (ndvfm->c.is_out_4k)
		hw->blk_used_uhd = true;
	else
		hw->blk_used_hd = true;
#endif
	diff = check_diff(itf, ndvfm, para);

	vf_in = &ndvfm->c.vf_in_cp;

	ndvfm->c.cnt_display = ds->cnt_display;
#ifdef HIS_CODE
	if (ndvfm->c.cnt_display < 5)
		PR_INF("%d:diff:0x%x\n", ndvfm->c.cnt_display, diff);
#endif
	if ((diff & 0xfff0) &&
	    (diff & 0xfff0)  != (ds->dbg_last_diff & 0xfff0)) {
		dbg_mem("diff:0x%x->0x%x\n", ds->dbg_last_diff, diff);
		atomic_add(DI_BIT3, &itf->c.dbg_display_sts);	/* dbg only */
		ds->dbg_last_diff = diff;
	}
	dbg_plink3("diff:0x%x\n", diff);
	ndvfm->c.sts_diff = diff;//dbg only;
#ifdef HIS_CODE
	/* di will not influence FRC work flow*/
	set_holdreg_by_in_out(vfm, para, op);
 #endif
	if (diff & EDIM_DVPP_DIFF_ALL) {
		dim_print("%s:vfm:0x%px, in_para:0x%px, o:0x%px\n",
		__func__, vfm, in_para, out_para);
		hw->reg_cnt = itf->sum_reg_cnt;
		ds->pre_done_nub++;

		dbg_plink1("%s:ch[%d] connected\n", __func__, itf->bind_ch);
		dpvpph_size_change(ds, ndvfm, op);
		if (nr_op() &&
		    nr_op()->cue_int &&
		    !(ds->en_dbg_off_nr & DI_BIT7))
			nr_op()->cue_int(vf_in, op);//temp need check reg write
#ifdef HIS_CODE
		if (hw->dis_c_para.win.x_size != ds->out_dvfm_demo.width ||
		    hw->dis_c_para.win.y_size != ds->out_dvfm_demo.height) {
			ds->out_dvfm_demo.width = hw->dis_c_para.win.x_size;
			ds->out_dvfm_demo.height = hw->dis_c_para.win.y_size;
		}
#endif
		//note: out_win is not same as dis_c_para.win.
		ds->out_win.x_size = hw->dis_c_para.win.x_size;
		ds->out_win.y_size = hw->dis_c_para.win.y_size;
		ds->out_win.x_end	= ds->out_win.x_size - 1;
		ds->out_win.y_end	= ds->out_win.y_size - 1;
		ds->out_win.x_st	= 0;
		ds->out_win.y_st	= 0;
		ds->cnt_display = 0;
		//mif in:
		ds->mifpara_in.cur_inp_type = 0; //p is 0, i is 1;
		ds->mifpara_in.prog_proc_type = 0x10;// p is 0x10, i is 0; tmp;
		//ds->mifpara_in.linear = 0; //tmp;
		memcpy(&ds->mifpara_in.win,
		       &hw->dis_c_para.win, sizeof(ds->mifpara_in.win));
		//mif out:
		memcpy(&ds->mifpara_out,
		       &ds->mifpara_in, sizeof(ds->mifpara_out));
		memcpy(&ds->mifpara_out.win,
		       &ds->out_win, sizeof(ds->mifpara_out.win));
#ifdef DBG_FLOW_SETTING
		print_dim_mifpara(&ds->mifpara_out, "display:out_set");
#endif
		//mif mem
		memcpy(&ds->mifpara_mem,
		       &ds->mifpara_out, sizeof(ds->mifpara_mem));

		ds->mif_in.mif_index = DI_MIF0_ID_INP;
		ds->mif_mem.mif_index  = DI_MIF0_ID_MEM;

		/**/
		if (DIM_IS_IC_EF(SC2)) {
			dim_sc2_contr_pre(&ds->pre_top_cfg, ds->op);
			if (ndvfm->c.is_out_4k)
				ds->pre_top_cfg.b.mode_4k = 1;
			else
				ds->pre_top_cfg.b.mode_4k = 0;
			dim_sc2_4k_set(ds->pre_top_cfg.b.mode_4k, ds->op);
		}
		ndvfm->c.cnt_display = ds->cnt_display;
		//cvs issue dpvpph_display_update_sub_last_cvs(itf, ds, ndvfm, op);

		//dbg_di_prelink_reg_check_v3_op(op);
		dpvpph_display_update_all(ds, ndvfm, ds->op);

		if (nr_op() && !ds->en_dbg_off_nr)
			nr_op()->nr_process_in_irq(op);

		//set last:?
		memcpy(&hw->dis_last_para,
		       &hw->dis_c_para, sizeof(hw->dis_last_para));
		hw->id_l = hw->id_c;
		if (ndvfm->c.in_dvfm.en_4k_snr != hw->en_4k_snr)
			dbg_plink1("snr:disp:chg:%d->%d\n", hw->en_4k_snr,
				ndvfm->c.in_dvfm.en_4k_snr);
		hw->en_4k_snr = ndvfm->c.in_dvfm.en_4k_snr;
		dim_print("display:set end:%d\n", ds->cnt_display);
		hw->dis_last_dvf	=  ndvfm;
		/* dct */
		if (hw->en_dct &&
		    hw->dis_ch != itf->bind_ch &&
		    !hw->dct_flg) {
			dbg_plink3("dis:ch<%d:%d>\n", hw->dis_ch, itf->bind_ch);
			hw->dis_ch = itf->bind_ch;
		}
		ds->cnt_display++;
		atomic_add(DI_BIT4, &itf->c.dbg_display_sts);	/* dbg only */
	} else {
		ndvfm->c.cnt_display = ds->cnt_display;
		//cvs issue dpvpph_display_update_sub_last_cvs(itf,
		//					       ds, ndvfm, op);

		if (diff & EDIM_DVPP_DIFF_SAME_FRAME) {
			/* nothing ? */
			//PR_INF("?same?\n");
			dpvpph_prelink_polling_check(ds->op, true);
			/* dbg only */
			atomic_add(DI_BIT5, &itf->c.dbg_display_sts);
		} else if (diff & EDIM_DVPP_DIFF_ONLY_ADDR) {
			ds->pre_done_nub++;

			//dbg_di_prelink_reg_check_v3_op(op);
			dpvpph_display_update_part(ds, ndvfm, ds->op, diff);

			if (nr_op() && !ds->en_dbg_off_nr)
				nr_op()->nr_process_in_irq(op);
#ifndef AUTO_NR_DISABLE
			dpvpph_pulldown_info_clear_g12a(op);
#endif
			dim_print("display:up addr end:%d\n", ds->cnt_display);
			//set last:
			ds->cnt_display++;
			hw->dis_last_dvf	=  ndvfm;
			/* dbg only */
			atomic_add(DI_BIT6, &itf->c.dbg_display_sts);
		} else {
			PR_INF("?other?\n");
			/* dbg only */
			atomic_add(DI_BIT7, &itf->c.dbg_display_sts);
		}
	}
	ds->field_count_for_cont++;
	if (cfgg(DCT) &&
	    hw->dis_ch != 0xff		&&
	    hw->dis_ch != hw->dct_ch	&&
	    !hw->dct_flg) {
		if (hw->dct_ch != 0xff)
			hw->dct_flg = 2;
		else
			hw->dct_flg = 1;
		dbg_plink1("dis:trig:dct reg:%d->%d\n", hw->dct_ch, hw->dis_ch);
	}
	atomic_set(&hw->link_instance_id, itf->sum_reg_cnt);
	return EPVPP_DISPLAY_MODE_NR;

DISPLAY_BYPASS:
	if (hw->dis_last_para.dmode != EPVPP_DISPLAY_MODE_BYPASS) {
		hw->dis_ch = 0xff;
		hw->last_dct_bypass = true;
		hw->blk_used_uhd = false;
		if (hw->en_4k_snr) {
			dbg_plink1("snr: to bypass\n");
			hw->en_4k_snr = false;
		}
		/* from pre-vpp link to bypass */
		//be sure vpp can display
		dim_afds()->pvpp_sw_setting_op(false, hw->op); //disable afbcd
		//dpvpph_pre_data_mif_ctrl(false, hw->op);
		opl1()->pre_mif_sw(false, hw->op, true);
		dpvpph_reverse_mif_ctrl(0, 0, hw->op);
		dpvpph_prelink_sw(hw->op, false);
		atomic_add(DI_BIT8, &itf->c.dbg_display_sts);	/* dbg only */
		dbg_plink1("%s:dis:bypass:dct unreg:%d->%d\n", __func__, hw->dct_ch, hw->dis_ch);
		/* move to dpvpp_reg_prelink_sw */
		//hw->blk_used_hd = false;
		//hw->blk_used_uhd = false;
	}
	if (hw->dis_ch != hw->dct_ch && !hw->dct_flg) {
		hw->dct_flg = 2;
		dbg_plink1("dis:trig1:dct unreg:%d->%d\n", hw->dct_ch, hw->dis_ch);
	}
	hw->dis_last_para.dmode = EPVPP_DISPLAY_MODE_BYPASS;
	hw->dis_last_dvf = NULL;
	if (ds)
		ds->cnt_display = 0;
	atomic_set(&hw->link_instance_id, DIM_PLINK_INSTANCE_ID_INVALID);
	atomic_add(DI_BIT9, &itf->c.dbg_display_sts);	/* dbg only */
	if (dbg_tm)
		dbg_dbg("%s:bypass\n", __func__);
	dbg_plink1("%s:ch[%d] to bypass:vfm:%px ndvfm:%px itf:%px reason:%x\n",
		__func__, itf->bind_ch, vfm, ndvfm, itf, bypass_reason);
	return EPVPP_DISPLAY_MODE_BYPASS;
}

/* use this for coverity */
#define FIX_MC_PRE_EN	(0) //for mc_pre_en
#define FIX_MADI_EN	(0)	//madi_en
/*copy from di_reg_setting */
static void dpvpph_reg_setting(const struct reg_acc *op_in)
{
	struct di_dev_s *de_devp = get_dim_de_devp();

	dbg_plink1("%s:\n", __func__);

	diext_clk_b_sw(true);

	if (1/*pst_wr_en*/)
		dim_set_power_control(1);

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX)) {
		/*if (!use_2_interlace_buff) {*/
		if (1) {
			if (DIM_IS_IC_EF(SC2))
				dim_top_gate_control_sc2(true, true);
			else
				dim_top_gate_control(true, true);
			/*dim_post_gate_control(true);*/
			/* freerun for reg configuration */
			/*dimh_enable_di_post_mif(GATE_AUTO);*/
			hpst_power_ctr(true);
		} else {
			dim_top_gate_control(true, false);
		}
		de_devp->flags |= DI_VPU_CLKB_SET;
		/*set clkb to max */
		if (is_meson_g12a_cpu()	||
		    is_meson_g12b_cpu()	||
		    is_meson_tl1_cpu()	||
		    is_meson_tm2_cpu()	||
		    DIM_IS_IC(T5)	||
		    DIM_IS_IC(T5DB)	||
		    DIM_IS_IC(T5D)	||
		    is_meson_sm1_cpu()	||
		    DIM_IS_IC_EF(SC2)) {
			#ifdef CLK_TREE_SUPPORT
			clk_set_rate(de_devp->vpu_clkb,
				     de_devp->clkb_max_rate);
			#endif
		}

		dimh_enable_di_pre_mif(false, FIX_MC_PRE_EN);
		if (DIM_IS_IC_EF(SC2))
			dim_pre_gate_control_sc2(true,
						 FIX_MC_PRE_EN);
		else
			dim_pre_gate_control(true, FIX_MC_PRE_EN);
		dim_pre_gate_control(true, FIX_MC_PRE_EN);

		/*2019-01-22 by VLSI feng.wang*/
		//
		if (DIM_IS_IC_EF(SC2)) {
			if (opl1()->wr_rst_protect)
				opl1()->wr_rst_protect(true);
		} else {
			dim_rst_protect(true);
		}

		dim_pre_nr_wr_done_sel(true);
		if (nr_op() && nr_op()->nr_gate_control)
			nr_op()->nr_gate_control(true, op_in);
	} else {
		/* if mcdi enable DI_CLKG_CTRL should be 0xfef60000 */
		DIM_DI_WR(DI_CLKG_CTRL, 0xfef60001);
		/* nr/blend0/ei0/mtn0 clock gate */
	}
	/*--------------------------*/
	dim_init_setting_once();
	/*--------------------------*/
	/*di_post_reset();*/ /*add by feijun 2018-11-19 */
	if (DIM_IS_IC_EF(SC2)) {
		opl1()->pst_mif_sw(false, DI_MIF0_SEL_PST_ALL);
		opl1()->pst_dbg_contr(op_in);
	} else {
		post_mif_sw(false);
		post_dbg_contr();
	}

	/*--------------------------*/
	/*--------------------------*/
	dimh_calc_lmv_init();
	/*--------------------------*/
	/*test*/
	dimh_int_ctr(0, 0, 0, 0, 0, 0);
	//dim_ddbg_mod_save(EDI_DBG_MOD_REGE, channel, 0);
	sc2_dbg_set(DI_BIT0 | DI_BIT1);
	/*
	 * enable&disable contwr txt
	 */
	if (DIM_IS_IC_EF(SC2)) {
		op_in->bwr(DI_MTN_1_CTRL1, FIX_MADI_EN ? 5 : 0, 29, 3);
	} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12B)) {
		op_in->bwr(DI_MTN_CTRL, FIX_MADI_EN, 29, 1);
		op_in->bwr(DI_MTN_CTRL, FIX_MADI_EN, 31, 1);
	} else {
		op_in->bwr(DI_MTN_1_CTRL1, FIX_MADI_EN ? 5 : 0, 29, 3);
	}
	if (DIM_IS_IC(T5DB))
		dim_afds()->reg_sw_op(true, op_in);
}

/* copy from di_pre_size_change */
static void dpvpph_size_change(struct dim_pvpp_ds_s *ds,
			       struct dimn_dvfm_s *dvfm,
			       const struct reg_acc *op_in)
{
//	unsigned int blkhsize = 0;
	union hw_sc2_ctr_pre_s *sc2_pre_cfg;
//	bool pulldown_en = false; //dimp_get(edi_mp_pulldown_enable)
//	bool mc_en = false;//dimp_get(edi_mp_mcpre_en)
	const struct reg_acc *op;
	unsigned int width, height, vf_type; //ori input para
	struct nr_cfg_s cfg_data;
	struct nr_cfg_s *cfg = &cfg_data;
	struct dim_pvpp_hw_s *hw;
	unsigned int val;

	hw = &get_datal()->dvs_pvpp.hw;

	width	= hw->dis_c_para.win.x_size;
	height	= hw->dis_c_para.win.y_size;
	vf_type = 0; //for p
	op = op_in;
	cfg->width = width;
	cfg->height = height;
	cfg->linkflag = 1;

	if (!op)
		op = &di_pre_regset;

	/* need check if write register. */
	if (nr_op() && !(ds->en_dbg_off_nr & DI_BIT7))
		nr_op()->nr_all_config(vf_type, op, cfg);

	/* after nr_all_config and change setting */

	if (dvfm->c.set_cfg.b.en_4k_snr) {
		hw->blk_used_uhd = false;
		hw->blk_used_hd = false;
		if (!hw->back_reg_nr) {
			hw->back_reg_nr = RD(NR4_TOP_CTRL);
			//clear bit 18 VLSI: ml
			val = hw->back_reg_nr;
			val &= 0xfffbffff;
			DIM_DI_WR(NR4_TOP_CTRL, val);
			wr_reg_bits(NR2_MATNR_YBETA_SCL, 0xffff, 16, 16);
			wr_reg_bits(NR2_MATNR_CBETA_SCL, 0xffff, 16, 16);
			dbg_plink1("snr:reg:0x%x -> 0x%08x\n", hw->back_reg_nr, val);
		} else {
			//not save:
			val = RD(NR4_TOP_CTRL);
			//clear bit 18 VLSI: ml
			val &= 0xfffbffff;
			DIM_DI_WR(NR4_TOP_CTRL, val);
			dbg_plink1("snr:en 2: 0x%08x\n", val);
		}
	} else {
		if (dvfm->c.is_out_4k) {
			hw->blk_used_uhd = true;
			hw->blk_used_hd = false;
		} else {
			hw->blk_used_hd = true;
			hw->blk_used_uhd = false;
		}

		if (hw->back_reg_nr) {
			DIM_DI_WR(NR4_TOP_CTRL, hw->back_reg_nr);
			wr_reg_bits(NR2_MATNR_YBETA_SCL, 0x0, 16, 16);
			wr_reg_bits(NR2_MATNR_CBETA_SCL, 0x0, 16, 16);
			dbg_plink1("snr:reg:tnr:0x%08x\n", hw->back_reg_nr);
		}
		hw->back_reg_nr = 0;
	}

	op->wr(DI_PRE_SIZE, (width - 1) | ((height - 1) << 16));
	PR_INF("%s:DI_PRE_SIZE:%d,%d: arb:%x\n", __func__,
		width, height, RD(DI_ARB_DBG_STAT_L1C1));

	/*dimh_int_ctr(0, 0, 0, 0, 0, 0);*/
	if (1/*hw->en_pst_wr_test*/)
		dimh_int_ctr(1, /*ppre->madi_enable*/0,
		     dimp_get(edi_mp_det3d_en) ? 1 : 0,
		     /*de_devp->nrds_enable*/0,
		     /*dimp_get(edi_mp_post_wr_en)*/1,
		     /*ppre->mcdi_enable*/0);

	if (DIM_IS_IC_EF(SC2)) {
		//for crash test if (!ppre->cur_prog_flag)
		dim_pulldown_info_clear_g12a(hw->op);
		dim_sc2_afbce_rst(0, op);
		dbg_mem2("pre reset-----\n");

		sc2_pre_cfg = &ds->pre_top_cfg;//&get_hw_pre()->pre_top_cfg;
		dim_print("%s:cfg[%px]:inp[%d]\n", __func__,
			  sc2_pre_cfg, sc2_pre_cfg->b.afbc_inp);

		sc2_pre_cfg->b.is_4k	= 0; /*temp*/
		sc2_pre_cfg->b.nr_ch0_en	= 1;
		if (!hw->en_pst_wr_test || dim_is_pre_link_l())
			sc2_pre_cfg->b.pre_frm_sel = 2;
		else
			sc2_pre_cfg->b.pre_frm_sel = 0;
	}
}

#define FIX_CHAN2_DISABLE	(1) //chan2_disable
/* copy from dimh_enable_di_pre_aml */
static void dpvpph_enable_di_pre_aml(const struct reg_acc *op_in,
			    unsigned int mode_param,
			    unsigned char pre_vdin_link,
			    /* fix 0 ? and bypass mem ?*/
			    void *pre)
{
	bool mem_bypass = false;
//	bool chan2_disable = false; use FIX_CHAN2_DISABLE
	unsigned int sc2_tfbf = 0; /* DI_PRE_CTRL bit [12:11] */
	unsigned int pd_check = 0; /* DI_PRE_CTRL bit [3:2] */
	unsigned int pre_link_en = 0;
//	unsigned int madi_en = 0;	//use FIX_MADI_EN
	unsigned int pre_field_num = 1;
//	unsigned int tmpk;
	u32 val;
	bool mem_bit8 = 1;
	bool cp_mode = false;
	unsigned int sample_value;
	unsigned int pw_en; /* fix 1 for p vpp link ?*/

	pw_en = mode_param & DI_BIT0;
	sample_value = mode_param & DI_BIT1;

	if (!pw_en || dim_is_pre_link_l())
		pre_link_en = 1;

	mem_bypass = (pre_vdin_link & 0x30) ? true : false;

	if (pre_vdin_link & DI_BIT6)
		cp_mode = true;

	if (mem_bypass) {
		if (DIM_IS_IC_EF(SC2))
			mem_bit8 = 0;
	}
	pre_vdin_link &= 0xf;

	/*bit 12:11*/
	if (DIM_IS_IC_EF(SC2)) {
		sc2_tfbf = 2;
	} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		sc2_tfbf = 1;
		if (pre_link_en)
			sc2_tfbf |= DI_BIT1;
	} else {
		sc2_tfbf = 0;
	}

	pd_check = 1;
#ifdef AUTO_NR_DISABLE
	pd_check = FIX_MADI_EN;
#endif
	/*
	 * the bit define is not same with before ,
	 * from sc2 DI_PRE_CTRL 0x1700,
	 * bit5/6/8/9/10/11/12
	 * bit21/22 chan2 t/b reverse,check with vlsi feijun
	 */

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		//chan2_disable = true;
		//opl1()->pre_gl_thd();
		if (DIM_IS_IC_EF(SC2))
			op_in->bwr(DI_SC2_PRE_GL_THD,
				dimp_get(edi_mp_prelink_hold_line), 16, 6);
		else	/*from feijun's for pre-vpp link */
			op_in->bwr(DI_PRE_GL_THD, 8, 16, 6);
		if (dimp_get(edi_mp_pre_ctrl)) {
			op_in->bwr(DI_PRE_CTRL,
				   dimp_get(edi_mp_pre_ctrl), 0, 29);
			dim_print("%s:a\n", __func__);
		} else {
			if (DIM_IS_IC_EF(SC2)) {
				op_in->wr(DI_PRE_CTRL,
					    1 | /* nr wr en */
					    (FIX_MADI_EN << 1) | /* mtn en */
					    (pd_check << 2) |
					    /*check3:2pulldown*/
					    (pd_check << 3) |
					    /*check2:2pulldown*/
					    (1 << 4)	 |
					    (FIX_MADI_EN << 5) |
					    /*hist check enable*/
					    (1 << 6)	| /* MTN after NR. */
					    (mem_bit8 << 8) | /* mem en. */
						/* chan 2 enable */
					    ((FIX_CHAN2_DISABLE ? 0 : 1) << 9) |
					    (sc2_tfbf << 11)	|
					    /* nrds mif enable */
					    (pre_vdin_link << 13)	   |
					    /* pre go line link */
					    (pre_vdin_link << 14)	   |
					    (1 << 21)	| /*chan2 t/b reverse*/
					    /* under prelink mode, need enable upsample filter */
					    (sample_value << 23)  |
					    /* mode_422c444 [24:23] */
					    (0 << 25)  |
					    /* contrd en */
					    /* under prelink mode, need enable upsample filter */
					    (sample_value << 26)  |
					    /* mode_444c422 [27:26] */
					    pre_field_num << 29);
				val = op_in->rd(DI_TOP_PRE_CTRL);
				//use cp_mode SWPL-127836
				val &= ~(0xff << 12);
				if (cp_mode)
					val |= (0x81 << 12);
				op_in->wr(DI_TOP_PRE_CTRL, val);

				dim_print("%s:b:%d, cp[0x%x]\n", __func__, pre_field_num, val);
			} else {
				op_in->wr(DI_PRE_CTRL,
					  1		 | /* nr wr en */
					  (FIX_MADI_EN << 1) | /* mtn en */
					  (pd_check << 2)	| /* check3:2pulldown*/
					  (pd_check << 3)	| /* check2:2pulldown*/
					  (1 << 4)	 |
					  (FIX_MADI_EN << 5) | /*hist check enable*/
				/* hist check  use chan2. */
					  (FIX_MADI_EN << 6) |
				/*hist check use data before noise reduction.*/
					  ((FIX_CHAN2_DISABLE ? 0 : 1) << 8) |
				/* chan 2 enable for 2:2 pull down check.*/
				/* line buffer 2 enable */
					  ((FIX_CHAN2_DISABLE ? 0 : 1) << 9) |
					  (0 << 10)	| /* pre drop first. */
					  //(1 << 11)	| /* nrds mif enable */
					  (sc2_tfbf << 11) | /* nrds mif enable */
				   /* (pre_link_en << 12)  | *//* pre viu link */
					  (pre_vdin_link << 13) |
					/* pre go line link */
					  (pre_vdin_link << 14)	|
					  (1 << 21)	| /*invertNRfield num*/
					  (1 << 22)	| /* MTN after NR. */
					  /* under prelink mode, need enable upsample filter */
					  (sample_value << 23)  |
					  /* mode_422c444 [24:23] */
					  (0 << 25)	| /* contrd en */
					  /* under prelink mode, need enable upsample filter */
					  (sample_value << 26)  |
					  /* mode_444c422 [27:26] */
					  ((mem_bypass ? 1 : 0) << 28)   |
					  pre_field_num << 29);
				dim_print("%s:c:%d\n", __func__, pre_field_num);
			}
		}
	} else {
		op_in->wr(DI_PRE_CTRL,
			    1			/* nr enable */
			    | (FIX_MADI_EN << 1)	/* mtn_en */
			    | (FIX_MADI_EN << 2)	/* check 3:2 pulldown */
			    | (FIX_MADI_EN << 3)	/* check 2:2 pulldown */
			    | (1 << 4)
			    | (FIX_MADI_EN << 5)	/* hist check enable */
			    | (1 << 6)		/* hist check  use chan2. */
			    | (0 << 7)
			    /* hist check use data before noise reduction. */
			    | (FIX_MADI_EN << 8)
			    /* chan 2 enable for 2:2 pull down check.*/
			    | (FIX_MADI_EN << 9)	/* line buffer 2 enable */
			    | (0 << 10)		/* pre drop first. */
			    | (sc2_tfbf << 11)		/* di pre repeat */
			    | (0 << 12)		/* pre viu link */
			    | (pre_vdin_link << 13)
			    | (pre_vdin_link << 14)	/* pre go line link */
			    | (dimp_get(edi_mp_prelink_hold_line) << 16)
			    /* pre hold line number */
			    | (1 << 22)		/* MTN after NR. */
			    | (FIX_MADI_EN << 25)	/* contrd en */
			    | (pre_field_num << 29)	/* pre field number.*/
			    );
	}
}

/* ary move to di_hw_v2.c*/
/*
 * enable/disable inp&chan2&mem&nrwr mif
 * copy from di_pre_data_mif_ctrl
 */
static void dpvpph_pre_data_mif_ctrl(bool enable, const struct reg_acc *op_in)
{
	if (enable) {
		/*****************************************/
		if (dim_afds() && dim_afds()->is_used())
			dim_afds()->inp_sw_op(true, op_in);
		if (dim_afds() && !dim_afds()->is_used_chan2())
			op_in->bwr(DI_CHAN2_GEN_REG, 1, 0, 1);

		if (dim_afds() && !dim_afds()->is_used_mem())
			op_in->bwr(DI_MEM_GEN_REG, 1, 0, 1);

		if (dim_afds() && !dim_afds()->is_used_inp())
			op_in->bwr(DI_INP_GEN_REG, 1, 0, 1);

		/* nrwr no clk gate en=0 */
		/*DIM_RDMA_WR_BITS(DI_NRWR_CTRL, 0, 24, 1);*/
	} else {
		/* nrwr no clk gate en=1 */
		/*DIM_RDMA_WR_BITS(DI_NRWR_CTRL, 1, 24, 1);*/
		/* nr wr req en =0 */
		op_in->bwr(DI_PRE_CTRL, 0, 0, 1);
		/* disable input mif*/
		op_in->bwr(DI_CHAN2_GEN_REG, 0, 0, 1);
		op_in->bwr(DI_MEM_GEN_REG, 0, 0, 1);
		op_in->bwr(DI_INP_GEN_REG, 0, 0, 1);

		/* disable AFBC input */
		//if (afbc_is_used()) {
		#ifdef HIS_CODE //ary temp
		if (dim_afds() && dim_afds()->is_used()) {
			//afbc_input_sw(false);
			if (dim_afds())
				dim_afds()->inp_sw_op(false, op_in);
		}
		#endif
	}
}

/*
 * reverse_mif control mirror mode
 * copy from dim_post_read_reverse_irq
 */
static void dpvpph_reverse_mif_ctrl(bool reverse, unsigned int hv_mirror,
		const struct reg_acc *op_in)
{
	const unsigned int *reg;
	unsigned int reg_addr;
	unsigned int mif_reg_addr;
	const unsigned int *mif_reg;

	reg = afbc_get_inp_base();
	mif_reg = mif_reg_get_v3();
	if (!mif_reg || !reg)
		return;

	reg_addr = reg[EAFBC_MODE];
	mif_reg_addr = mif_reg[MIF_GEN_REG2];
	if (DIM_IS_IC(T5DB) || DIM_IS_IC(T5))
		mif_reg_addr = DI_INP_GEN_REG2;

	if (reverse) {
		op_in->bwr(mif_reg_addr, 3, 2, 2);
		op_in->bwr(reg_addr, 3, 26, 2);//AFBC_MODE
	} else if (hv_mirror == 1) {
		op_in->bwr(mif_reg_addr,  1, 2, 1);
		op_in->bwr(mif_reg_addr,  0, 3, 1);
		op_in->bwr(reg_addr, 1, 26, 1);//AFBC_MODE
		op_in->bwr(reg_addr, 0, 27, 1);//AFBC_MODE
	} else if (hv_mirror == 2) {
		op_in->bwr(mif_reg_addr,  0, 2, 1);
		op_in->bwr(mif_reg_addr,  1, 3, 1);
		op_in->bwr(reg_addr, 0, 26, 1);//AFBC_MODE
		op_in->bwr(reg_addr, 1, 27, 1);//AFBC_MODE
	} else {
		op_in->bwr(mif_reg_addr,  0, 2, 2);
		op_in->bwr(reg_addr, 0, 26, 2);//AFBC_MODE
	}
}

/* copy from hpre_gl_sw and hpre_gl_sw_v3 */
static void dpvpph_gl_sw(bool on, bool pvpp_link, const struct reg_acc *op)
{
	if (!cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		return;
	if (DIM_IS_IC_BF(SC2)) {
		if (pvpp_link) {
			if (op->rd(DI_PRE_GL_CTRL) & 0x80000000)
				op->bwr(DI_PRE_GL_CTRL, 0, 31, 1);
			return;
		}
		if (on)
			op->wr(DI_PRE_GL_CTRL,
			       0x80200000 | dimp_get(edi_mp_line_num_pre_frst));
		else
			op->wr(DI_PRE_GL_CTRL, 0xc0000000);
	} else {
		if (pvpp_link) {
			if (op->rd(DI_SC2_PRE_GL_CTRL) & 0x80000000)
				op->bwr(DI_SC2_PRE_GL_CTRL, 0, 31, 1);
			return;
		}
		if (on)
			op->wr(DI_SC2_PRE_GL_CTRL,
			       0x80200000 | dimp_get(edi_mp_line_num_pre_frst));
		else
			op->wr(DI_SC2_PRE_GL_CTRL, 0xc0000000);
	}
}

static void dpvpph_gl_disable(const struct reg_acc *op) //test-05
{
	if (!cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		return;
	if (DIM_IS_IC_BF(SC2)) {
		op->wr(DI_PRE_GL_CTRL, 0xc0000000);
		op->wr(DI_POST_GL_CTRL, 0xc0000000);	//test-06
	} else {
		op->wr(DI_SC2_PRE_GL_CTRL, 0xc0000000);
		op->wr(DI_SC2_POST_GL_CTRL, 0xc0000000);
		return;
	}
}

/* copy from dim_pre_frame_reset_g12 */
static void dpvpph_pre_frame_reset_g12(bool pvpp_link,
				       const struct reg_acc *op_in)
{
	if (!cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		return;
	if (DIM_IS_IC_BF(SC2)) {
		if (pvpp_link) {
			if (op_in->rd(DI_PRE_GL_CTRL) & 0x80000000)
				op_in->bwr(DI_PRE_GL_CTRL, 0, 31, 1);
			return;
		}
	} else {
		if (pvpp_link) {
			if (op_in->rd(DI_SC2_PRE_GL_CTRL) & 0x80000000)
				op_in->bwr(DI_SC2_PRE_GL_CTRL, 0, 31, 1);
			return;
		}
	}
}

/*copy from dim_pre_frame_reset */
void dpvpph_pre_frame_reset(const struct reg_acc *op_in)
{
	op_in->bwr(DI_PRE_CTRL, 3, 30, 2);
}

static void dpvpph_pre_to_link(void)
{
	const struct reg_acc *op;

	op = get_datal()->dvs_pvpp.hw.op_n;
	if (DIM_IS_IC_BF(SC2) && DIM_IS_IC_EF(G12A)) {
		op->bwr(DI_PRE_CTRL, 1, 12, 1);
		dbg_plink1("set:DI_PRE_CTRL bit 12\n");
	} else if (DIM_IS_IC_EF(SC2)) {
		op->bwr(DI_TOP_PRE_CTRL, 2, 30, 2);// pre viu link, use viu vsync as frame reset
		op->bwr(DI_TOP_CTRL, 1, 0, 1);// 1:pre link vpp  0:post link vpp
		dbg_plink1("set:DI_TOP_CTRL bit 0\n");
	}
}

//for t5d vb ?
static void dpvpph_nr_ddr_switch(bool sw, const struct reg_acc *op)
{
	op->bwr(DI_AFBCE_CTRL, sw, 4, 1);
}

static bool dpvpp_parser_nr(struct dimn_itf_s *itf)
{
	struct dimn_qs_cls_s *qin;
	struct vframe_s *vfm;
	struct dimn_dvfm_s *ndvfm = NULL;
	struct dvfm_s *in_dvfm, *out_dvfm;
	unsigned int bypass;
	struct dim_cvspara_s *cvsp;
	unsigned int chg;
	int timeout = 0; //for post write only
	struct dim_pvpp_ds_s *ds;
	struct dim_pvpp_hw_s *hw;
	struct dvfm_s *dvfm_dm;
	unsigned int out_fmt;
	struct pvpp_buf_cfg_s *buf_cfg;
	struct di_buffer *buffer;

	hw = &get_datal()->dvs_pvpp.hw;

	if (!itf || !itf->ds)
		return false;
	ds = itf->ds;
	qin	= &ds->lk_que_in;
	vfm	= (struct vframe_s *)qin->ops.peek(qin);
	if (!vfm)
		return false;
	ndvfm	= (struct dimn_dvfm_s *)vfm->private_data;
	if (!ndvfm) //no input
		return false;
	in_dvfm		= &ndvfm->c.in_dvfm;
	//out_dvfm	= &ndvfm->c.out_dvfm;

	bypass = dpvpp_is_bypass_dvfm_prelink(in_dvfm, ds->en_4k);
	if (hw->flg_tvp != itf->c.is_tvp) {
		dbg_plink2("bypass reason:%d to %d (tvp:c:%d t:%d), cnt:%d\n",
			bypass, EPVPP_BYPASS_REASON_TVP,
			hw->flg_tvp, itf->c.is_tvp, ndvfm->c.cnt_in);
		bypass = EPVPP_BYPASS_REASON_TVP;
	}
	ndvfm->c.bypass_reason = bypass;
	if (bypass) {
		if (!itf->c.bypass_reason)
			dbg_plink2("to bypass:%d:%d\n", bypass, ndvfm->c.cnt_in);
		itf->c.bypass_reason = bypass;
		dpvpp_parser_bypass(itf, 1);
		return true;
	}
	//dbg only:
	dpvpp_cnt_para(&ds->dis_para_demo2, &ndvfm->c.vf_in_cp,
			dpvpp_dbg_force_dech,
			dpvpp_dbg_force_decv);
	if (itf->c.bypass_reason)
		dbg_plink2("leave bypass:%d\n", ndvfm->c.cnt_in);
	itf->c.bypass_reason = 0;
	dim_print("%s:nub:%d\n", __func__, ndvfm->c.cnt_in);
	chg = dpvpp_is_change_dvfm(&ds->in_last, in_dvfm);
	dim_print("%s:chg:[%d]\n", __func__, chg);
	dbg_check_ud(ds, 1); /* dbg */
	if (chg) {
		/* debug only */
		if (cfgg(PAUSE_SRC_CHG)) {
			pre_run_flag = DI_RUN_FLAG_PAUSE;
			PR_INF("dbg change:ppause\n");
		}
		/* change */
		PR_INF("source change:ch[%d]:id[%d]:0x%x/%d/%d/%d=>0x%x/%d/%d/%d:%d:%d\n",
				itf->bind_ch,
			       itf->id,
			       ds->in_last.vf_type,
			       ds->in_last.x_size,
			       ds->in_last.y_size,
			       ds->in_last.cur_source_type,
			       in_dvfm->vfs.type,
			       in_dvfm->vfs.width,
			       in_dvfm->vfs.height,
			       in_dvfm->vfs.source_type,
			       ndvfm->c.cnt_in,
			       in_dvfm->en_4k_snr);
		/* reset last_ds_ratio when source size change */
		itf->c.last_ds_ratio = DI_FLAG_DCT_DS_RATIO_MAX;
		ds->set_cfg_cur.b.en_linear_cp = hw->en_linear;
		ds->is_inp_4k = false;
		ds->is_out_4k = false;
		if (!VFMT_IS_EOS(in_dvfm->vfs.type) && in_dvfm->is_4k) {
			ds->is_inp_4k = true;
			ds->is_out_4k = true;
		}
		if (ds->is_inp_4k) {
			ds->o_c.d32 = ds->o_uhd.d32;
			dbg_plink1("is:uhd");
		} else {
			ds->o_c.d32 = ds->o_hd.d32;
		}

		if (in_dvfm->is_4k && in_dvfm->en_4k_snr)
			ds->o_c.b.out_buf = ds->o_hd.b.out_buf;

		dvfm_dm	= &ds->out_dvfm_demo;
		//memset(out_dvfm, 0, sizeof(*out_dvfm));
		if (ds->o_c.b.mode == EDPST_MODE_NV21_8BIT) { //tmp
			if (ds->en_out_nv12)
				out_fmt = 2;
			else
				out_fmt = 1;
		} else/* if (ds->out_mode == EDPST_MODE_422_10BIT_PACK)*/ {
			out_fmt = 0;
		}
		dbg_plink1("%s:id[%d]:out_fmt[%d]\n", __func__, itf->id, out_fmt);
		dim_dvf_type_p_change(dvfm_dm, out_fmt);

		buf_cfg = get_buf_cfg(ds->o_c.b.out_buf, 5);
		if (!buf_cfg) {
			PR_ERR("no buf_cfg?\n");
			return false;
		}

		if (ds->o_c.b.is_afbce) {
			dvfm_dm->flg_afbce_set = true;
			dvfm_dm->vfs.type |= VIDTYPE_COMPRESS; //tmp
			dvfm_dm->vfs.type |= VIDTYPE_SCATTER;
		} else {
			dvfm_dm->flg_afbce_set = false;
		}

		dvfm_dm->vfs.plane_num = buf_cfg->dbuf_plan_nub;
		dvfm_dm->buf_hsize = buf_cfg->dbuf_hsize;
		memcpy(&dvfm_dm->vfs.canvas0_config[0],
		       &buf_cfg->dbuf_wr_cfg[0][0],
		       sizeof(dvfm_dm->vfs.canvas0_config[0]) << 1);
		if (itf->etype == EDIM_NIN_TYPE_VFM)
			dvfm_dm->is_itf_vfm = 1;
		if (hw->en_pst_wr_test)
			dvfm_dm->is_p_pw	= 1;
		else
			dvfm_dm->is_prvpp_link	= 1;

		if (ds->is_out_4k && in_dvfm->en_4k_snr) {
			ds->set_cfg_cur.b.en_4k_snr = 1;
		} else if (ds->is_out_4k && ds->en_4k_snr && !hw->blk_rd_uhd) {
			PR_WARN("snr:4k tnr no buffer? force snr\n");
			ds->set_cfg_cur.b.en_4k_snr = 1;
		} else {
			ds->set_cfg_cur.b.en_4k_snr = 0;
		}
		if (ds->set_cfg_cur.b.en_4k_snr || dpvpp_dbg_mem_fl_in())
			ds->set_cfg_cur.b.en_mem_cp_in = 1;
		else
			ds->set_cfg_cur.b.en_mem_cp_in = 0;
		/* dvfm_demo end ********************************/
		/* do not care no-support */
		/* cfg seting en */

		if (IS_COMP_MODE(in_dvfm->vfs.type)) {
			ds->set_cfg_cur.b.en_in_afbcd	= true;
			ds->set_cfg_cur.b.en_in_mif	= false;
			//ds->set_cfg_cur.b.en_in_cvs	= false;
			ds->afbc_sgn_cfg.b.inp		= true;
//			ds->afbc_en_cfg.b.inp		= true;
		} else {
			ds->set_cfg_cur.b.en_in_afbcd	= false;
			ds->set_cfg_cur.b.en_in_mif	= true;
			ds->set_cfg_cur.b.en_in_cvs	= true;
			ds->afbc_sgn_cfg.b.inp		= false;
//			ds->afbc_en_cfg.b.inp		= false;
		}
		if (ds->set_cfg_cur.b.en_in_mif && !hw->en_linear)
			ds->set_cfg_cur.b.en_in_cvs	= true;
		if (ds->o_c.b.is_afbce) {//tmp
			ds->set_cfg_cur.b.en_wr_afbce	= true;
			ds->afbc_sgn_cfg.b.mem		= true;
			ds->afbc_sgn_cfg.b.enc_nr	= true;
			ds->set_cfg_cur.b.en_wr_mif	= false; /*tmp*/
		} else {
			ds->set_cfg_cur.b.en_wr_afbce	= false;
			ds->set_cfg_cur.b.en_wr_mif	= true; /*tmp*/
			ds->afbc_sgn_cfg.b.mem		= false;
			ds->afbc_sgn_cfg.b.enc_nr	= false;
		}
		ds->set_cfg_cur.b.en_mem_mif = ds->set_cfg_cur.b.en_wr_mif;
		ds->set_cfg_cur.b.en_mem_afbcd = ds->set_cfg_cur.b.en_wr_afbce;
		if (ds->set_cfg_cur.b.en_wr_mif && !hw->en_linear) {
			ds->set_cfg_cur.b.en_wr_cvs	= true;
			ds->set_cfg_cur.b.en_mem_cvs	= true;
		} else {
			ds->set_cfg_cur.b.en_wr_cvs	= false;
			ds->set_cfg_cur.b.en_mem_cvs	= false;
		}
		if (ds->set_cfg_cur.b.en_mem_cp_in) {
			ds->set_cfg_cur.b.en_mem_mif = ds->set_cfg_cur.b.en_in_mif;
			ds->set_cfg_cur.b.en_mem_afbcd = ds->set_cfg_cur.b.en_in_afbcd;
			ds->set_cfg_cur.b.en_mem_cvs = ds->set_cfg_cur.b.en_in_cvs;
			ds->afbc_sgn_cfg.b.mem = ds->afbc_sgn_cfg.b.inp;
			dim_print("cp:mem\n");
		}
		/* cfg out dvfm demo */
		ds->out_dvfm_demo.vfs.height	= in_dvfm->vfs.height;
		ds->out_dvfm_demo.vfs.width	= in_dvfm->vfs.width;
		if (itf->etype == EDIM_NIN_TYPE_INS)
			ds->out_dvfm_demo.is_itf_ins_local = true;
		else
			ds->out_dvfm_demo.is_itf_ins_local = false;
		/* dis_para_demo */
		if (1/*hw->en_pst_wr_test*/)
			dpvpp_set_default_para(ds, ndvfm);

		/* afbc */
		ndvfm->c.afbc_sgn_cfg.d8 = ds->afbc_sgn_cfg.d8;

		ds->pre_top_cfg.b.is_inp_4k = ds->is_inp_4k;
		ds->pre_top_cfg.b.is_mem_4k = ds->is_out_4k;

		if (itf->nitf_parm.output_format & DI_OUTPUT_LINEAR)
			ds->out_dvfm_demo.is_in_linear = 1;
		else
			ds->out_dvfm_demo.is_in_linear = 0;

		dim_print("%s:set_cfg:cur:0x%x\n",
			__func__, ds->set_cfg_cur.d32);
		if (ds->out_dvfm_demo.is_in_linear)
			ds->out_dvfm_demo.vfs.flag |= VFRAME_FLAG_VIDEO_LINEAR;
		else
			ds->out_dvfm_demo.vfs.flag &= (~VFRAME_FLAG_VIDEO_LINEAR);

		ds->out_dvfm_demo.vfs.di_flag &= ~(DI_FLAG_DI_PVPPLINK | DI_FLAG_DI_PSTVPPLINK);
		ds->out_dvfm_demo.vfs.di_flag |= DI_FLAG_DI_PVPPLINK;
		if (cfgg(LINEAR))
			ds->out_dvfm_demo.is_linear = true;
		else
			ds->out_dvfm_demo.is_linear = false;
		//set last information:
		dpvpp_set_type_smp(&ds->in_last, in_dvfm);
		ds->cnt_parser = 0;
	}

	ndvfm->c.set_cfg.d32 = ds->set_cfg_cur.d32;
	ndvfm->c.is_inp_4k = ds->is_inp_4k;
	ndvfm->c.is_out_4k = ds->is_out_4k;
	ndvfm->c.out_fmt = ds->o_c.b.out_buf;

	ndvfm->c.afbc_sgn_cfg.d8 = ds->afbc_sgn_cfg.d8;
	dim_print("%s:set_cfg:0x%x;afbc[0x%x]\n", __func__,
		ndvfm->c.set_cfg.d32,
		ndvfm->c.afbc_sgn_cfg.d8);
	memcpy(&ndvfm->c.out_dvfm,
	       &ds->out_dvfm_demo, sizeof(ndvfm->c.out_dvfm));
	#ifdef DBG_FLOW_SETTING
	print_dvfm(&ndvfm->c.out_dvfm, "parser_nr:out_dvfm");
	#endif

	/* cvs set */
	memcpy(&ndvfm->c.in_dvfm_crop, in_dvfm, sizeof(ndvfm->c.in_dvfm_crop));
	in_dvfm = &ndvfm->c.in_dvfm_crop;
	out_dvfm = &ndvfm->c.out_dvfm;
	in_dvfm->src_h = ndvfm->c.src_h;
	in_dvfm->src_w = ndvfm->c.src_w;
	out_dvfm->src_h = ndvfm->c.src_h;
	out_dvfm->src_w = ndvfm->c.src_w;
	out_dvfm->vf_ext = ndvfm->c.ori_vf;
	out_dvfm->sum_reg_cnt = itf->sum_reg_cnt;
	out_dvfm->caller_mng = ndvfm->c.in_dvfm.caller_mng;
	if (ndvfm->c.set_cfg.b.en_in_cvs) {
		/* config cvs for input */
		cvsp = &ndvfm->c.cvspara_in;
		cvsp->plane_nub	= in_dvfm->vfs.plane_num;
		cvsp->cvs_cfg	= &in_dvfm->vfs.canvas0_config[0];
		//cvs id need cfg when set
		//cvs_link(&cvspara, "in_cvs");
	}
	if (ndvfm->c.set_cfg.b.en_wr_cvs) {
		/* config cvs for input */
		cvsp = &ndvfm->c.cvspara_wr;
		cvsp->plane_nub	= out_dvfm->vfs.plane_num;
		cvsp->cvs_cfg	= &out_dvfm->vfs.canvas0_config[0];
		//cvs id need cfg when set
		//cvs_link(&cvspara, "in_cvs");
	}

	if (ndvfm->c.set_cfg.b.en_mem_cvs) {
		/* config cvs for input */
		cvsp = &ndvfm->c.cvspara_mem;
		cvsp->plane_nub	= out_dvfm->vfs.plane_num;
		cvsp->cvs_cfg	= &out_dvfm->vfs.canvas0_config[0];
		//cvs id need cfg when set
		//cvs_link(&cvspara, "in_cvs");
	}
	di_load_pq_table();//tmp
	ndvfm->c.sum_reg_cnt = itf->sum_reg_cnt;
	ndvfm->c.cnt_parser = ds->cnt_parser;
	ds->cnt_parser++;
	//ds->en_in_mif = true;

	/* get */
	vfm = (struct vframe_s *)qin->ops.get(qin);

	if (!hw->en_pst_wr_test) {
		vtype_fill_d(itf, vfm, &ndvfm->c.vf_in_cp, &ndvfm->c.out_dvfm);
		dim_print("%s:link\n", __func__);
		didbg_vframe_out_save(itf->bind_ch, vfm, 6);
		if (itf->etype == EDIM_NIN_TYPE_INS) {
			buffer = &itf->buf_bf[vfm->index];
			buffer->caller_mng = out_dvfm->caller_mng;
		}

		dpvpp_put_ready_vf(itf, ds, vfm);

		return true;
	}
	if (hw->en_pst_wr_test) {
		dim_print("%s:pst write\n", __func__);
		init_completion(&hw->pw_done);
		dpvpp_pre_display(vfm, &ds->dis_para_demo, NULL);
		//wait:
		timeout = wait_for_completion_timeout(&hw->pw_done,
			msecs_to_jiffies(60));
		if (!timeout) {
			PR_WARN("%s:timeout\n", "dpvpp pw test");
			dpvpph_gl_sw(false, false, ds->op);
		}

		/**************************/
		vtype_fill_d(itf, vfm, &ndvfm->c.vf_in_cp, &ndvfm->c.out_dvfm);
		dbg_check_vf(ds, vfm, 2);

		dpvpp_put_ready_vf(itf, ds, vfm);

		return true;
	}
	return true;
}

static void dpvpph_prelink_sw(const struct reg_acc *op, bool p_link)
{
	unsigned int val;
	u32 REG_VPU_WRARB_REQEN_SLV_L1C1;
	u32 REG_VPU_RDARB_REQEN_SLV_L1C1;
	u32 REG_VPU_ARB_DBG_STAT_L1C1;
	u32 WRARB_onval;
	u32 WRARB_offval;
	struct dim_pvpp_hw_s *hw;

	if (DIM_IS_IC_BF(TM2B)) {
		dim_print("%s:check return;\n", __func__);
		return;
	}

	REG_VPU_WRARB_REQEN_SLV_L1C1 = DI_WRARB_REQEN_SLV_L1C1;
	REG_VPU_RDARB_REQEN_SLV_L1C1 = DI_RDARB_REQEN_SLV_L1C1;
	REG_VPU_ARB_DBG_STAT_L1C1 = DI_ARB_DBG_STAT_L1C1;
	if (p_link)
		WRARB_onval = 0x3f;
	else
		WRARB_offval = 0x3e;

	if (p_link) {
		/* set on*/
		ext_vpp_plink_real_sw(true, false, false);
		val = op->rd(VD1_AFBCD0_MISC_CTRL);
		if (DIM_IS_IC_EF(T6D)) {
			op->bwr(DI_TOP_CTRL, 1, 0, 1);// 1:pre link vpp  0:post link vpp
			op->bwr(VD1_AFBCD0_MISC_CTRL, 1, 8, 1);    //bit 8 link
		} else if (DIM_IS_IC_EF(SC2)) {
			/* ? */
			op->bwr(DI_TOP_CTRL, 1, 0, 1);// 1:pre link vpp  0:post link vpp
			op->bwr(VD1_AFBCD0_MISC_CTRL, 1, 8, 6);            // data path
			op->bwr(VD1_AFBCD0_MISC_CTRL, 0, 16, 1);    // line buffer
			if (DIM_IS_ICS_T5M) {
				op->wr(DI_PRE_HOLD, 0x0);
				op->wr(DI_AFBCE0_HOLD_CTRL, 0x0);
				op->wr(DI_AFBCE1_HOLD_CTRL, 0x0);
			}
		} else {
			op->bwr(DI_AFBCE_CTRL, 1, 3, 1);
			//op->wr(VD1_AFBCD0_MISC_CTRL, 0x100100);
			if (DIM_IS_IC_TXHD2) {
				op->bwr(VD1_AFBCD0_MISC_CTRL, 3, 8, 2);
			} else {
				op->bwr(VD1_AFBCD0_MISC_CTRL, 1, 8, 1);
				op->bwr(VD1_AFBCD0_MISC_CTRL, 1, 20, 1);
			}
		}
		WR(REG_VPU_WRARB_REQEN_SLV_L1C1, WRARB_onval);
		WR(REG_VPU_RDARB_REQEN_SLV_L1C1, 0xffff);
		PR_INF("c_sw:ak:from[0x%x] %px\n", val, op);
	} else {
		/* set off */
		dpvpph_gl_disable(op);	//test-05
		/* reset prehold reg*/
		if (DIM_IS_ICS_T5M) {
			op->wr(DI_PRE_HOLD, 0x0);
			op->wr(DI_AFBCE0_HOLD_CTRL, 0x0);
			op->wr(DI_AFBCE1_HOLD_CTRL, 0x0);
		}
		if (DIM_IS_IC(T6D)) {
			dbg_plink1("c_sw:t6d\n");
			op->bwr(VD1_AFBCD0_MISC_CTRL, 0, 9, 1);
			op->bwr(VD1_AFBCD0_MISC_CTRL, 0, 8, 1);
		} else if (DIM_IS_IC_EF(SC2)) {
			dbg_plink1("c_sw:sc2\n");
			op->bwr(VD1_AFBCD0_MISC_CTRL, 0, 8, 6);
		} else {
			op->bwr(DI_AFBCE_CTRL, 0, 3, 1);
			op->bwr(VD1_AFBCD0_MISC_CTRL, 0, 8, 1);
			op->bwr(VD1_AFBCD0_MISC_CTRL, 0, 20, 1);
		}

		//for en_4k_snr
		hw = &get_datal()->dvs_pvpp.hw;
		if (hw->back_reg_nr) {
			DIM_DI_WR(NR4_TOP_CTRL, hw->back_reg_nr);
			wr_reg_bits(NR2_MATNR_YBETA_SCL, 0x0, 16, 16);
			wr_reg_bits(NR2_MATNR_CBETA_SCL, 0x0, 16, 16);
			dbg_plink1("snr:reg:tnr 2:0x%08x\n", hw->back_reg_nr);
		}
		hw->back_reg_nr = 0;
		hw->blk_used_hd = false;
		hw->blk_used_uhd = false;

		//prelink_status = false;
		op->wr(REG_VPU_WRARB_REQEN_SLV_L1C1, WRARB_offval);
		op->wr(REG_VPU_RDARB_REQEN_SLV_L1C1, 0xf079);
		PR_INF("c_sw:bk %px arb:%x\n", op, RD(DI_ARB_DBG_STAT_L1C1));
	}
}

static void dpvpph_prelink_polling_check(const struct reg_acc *op, bool p_link)
{
	unsigned int val;
	bool flg_set = false;

	if (DIM_IS_IC_BF(TM2B)) {
		dim_print("%s:check return;\n", __func__);
		return;
	}

	if (!p_link)
		return;

	val = op->rd(VD1_AFBCD0_MISC_CTRL);
	if (DIM_IS_IC_EF(SC2)) {
		/* check bit 8 / 16*/
		if ((val & 0x10100) != 0x00100)
			flg_set = true;
	} else if (DIM_IS_IC_TXHD2) {
		/* check bit 8 / 20 */
		if ((val & 0x100100) != 0x000100)
			flg_set = true;
	} else {
		/* check bit 8 / 20 */
		if ((val & 0x100100) != 0x100100)
			flg_set = true;
	}
	if (flg_set) {
		PR_ERR("%s:check:ccccck 0x%x\n", __func__, val);
		dpvpph_prelink_sw(op, p_link);
		dim_print("check:ccccck\n");
	}
}

static void dpvpph_display_update_all(struct dim_pvpp_ds_s *ds,
			       struct dimn_dvfm_s *ndvfm,
			       const struct reg_acc *op_in)
{
	struct dim_cvspara_s *cvsp;
	struct dvfm_s *in_dvfm, *out_dvfm, *nr_dvfm;
	unsigned char *cvs, pos;
	struct di_win_s *winc;
	bool ref_en = true;
	struct dim_mifpara_s *mifp;
	struct dimn_dvfm_s *ndvfm_last;
	int i;
	unsigned int bypass_mem = 0;
	struct dim_pvpp_hw_s *hw;
	struct pvpp_buf_cfg_s *buf_cfg;
	unsigned short t5d_cnt;
	unsigned int cp_mode = 0;
	unsigned int sample_mode = 2;

	hw = &get_datal()->dvs_pvpp.hw;
	/* ndvfm_last */
	ndvfm_last = hw->dis_last_dvf;
	buf_cfg = get_buf_cfg(ndvfm->c.out_fmt, 2);
	if (!buf_cfg) {
		PR_WARN("buf_cfg\n");
		return;
	}
	dim_print("out_fmt:%d:\n", ndvfm->c.out_fmt);
	if (/*hw->en_pst_wr_test*/0) {
		pos = 0;
		hw->cvs_last = 0;
	} else {
		/* pos: 0 ~ 1 */
		pos = (1 - hw->cvs_last) & 1;
		hw->cvs_last = pos;
	}
	cvs = &hw->cvs[pos][0];
	in_dvfm = &ndvfm->c.in_dvfm_crop;
	winc	= &hw->dis_c_para.win;
	if (winc->x_size != in_dvfm->vfs.width ||
	    winc->y_size != in_dvfm->vfs.height) {
		in_dvfm->vfs.width = winc->x_size;
		in_dvfm->vfs.height = winc->y_size;
	}

	if (DIM_IS_IC(T6D) && in_dvfm->vfs.width <= 1280)
		sample_mode = 0;

	ds->mif_wr.tst_not_setcontr = 0;
	if (dim_is_pre_link_l() || !hw->en_pst_wr_test) { //tmp
		if (ds->cnt_display > 1)
			ds->mif_wr.tst_not_setcontr = 1;
		else
			ds->mif_wr.tst_not_setcontr = 0;
	}

	dim_print("%s:set_cfg:0x%x:tst_not_setcontr[%d]\n",
		__func__, ndvfm->c.set_cfg.d32,
		ds->mif_wr.tst_not_setcontr);
	/* cfg cvs */
	if (ndvfm->c.set_cfg.b.en_in_cvs) {
		/* config cvs for input */
		if (in_dvfm->vfs.canvas0Addr == (u32)-1) {
			cvsp = &ndvfm->c.cvspara_in;
			cvsp->plane_nub	= in_dvfm->vfs.plane_num;
			cvsp->cvs_cfg	= &in_dvfm->vfs.canvas0_config[0];
			cvsp->cvs_id	= cvs + 0;
			//cvs issue if (hw->en_pst_wr_test)
				cvs_link(cvsp, "in_cvs");
			mifp = &ds->mifpara_in;
			mifp->cvs_id[0] = (unsigned int)(*cvsp->cvs_id);
			mifp->cvs_id[1] = (unsigned int)(*(cvsp->cvs_id + 1));
		} else {
			cvsp = &ndvfm->c.cvspara_in;
			if (IS_NV21_12(in_dvfm->vfs.type))
				cvsp->plane_nub = 2;
			else if (in_dvfm->vfs.type & VIDTYPE_VIU_422)
				cvsp->plane_nub = 1;
			mifp = &ds->mifpara_in;
			mifp->cvs_id[0] = in_dvfm->vfs.canvas0Addr & 0xff;
			if (cvsp->plane_nub == 2)
				mifp->cvs_id[1] =
				(in_dvfm->vfs.canvas0Addr >> 16) & 0xff;
		}
	}

	if (ndvfm->c.set_cfg.b.en_wr_cvs || ndvfm->c.set_cfg.b.en_wr_mif) {
		//write address:
		for (i = 0; i < ndvfm->c.out_dvfm.vfs.plane_num; i++)
			ndvfm->c.out_dvfm.vfs.canvas0_config[0].phy_addr =
				buf_cfg->dbuf_wr_cfg[pos][i].phy_addr;
	} else if (ndvfm->c.set_cfg.b.en_wr_afbce) {
		ndvfm->c.out_dvfm.vfs.canvas0_config[0].phy_addr =
				buf_cfg->dbuf_wr_cfg[pos][0].phy_addr;
		ndvfm->c.out_dvfm.afbct_adr = buf_cfg->addr_e[pos];
		ndvfm->c.out_dvfm.vfs.compHeadAddr = buf_cfg->addr_i[pos];
	}

	if (ndvfm->c.set_cfg.b.en_wr_cvs) {
		/* cvs for out */
		cvsp = &ndvfm->c.cvspara_wr;
		out_dvfm = &ndvfm->c.out_dvfm;
		cvsp->plane_nub = out_dvfm->vfs.plane_num;
		cvsp->cvs_cfg		= &out_dvfm->vfs.canvas0_config[0];
		cvsp->cvs_id		= cvs + 2;
		//cvs issue if (hw->en_pst_wr_test)
			cvs_link(cvsp, "wr");

		mifp = &ds->mifpara_out;
		mifp->cvs_id[0] = (unsigned int)(*cvsp->cvs_id);
		mifp->cvs_id[1] = (unsigned int)(*(cvsp->cvs_id + 1));
	}

	/* copy out_dvfm to nr_wr_dvfm, then adjust size by crop */
	memcpy(&ndvfm->c.nr_wr_dvfm,
		&ndvfm->c.out_dvfm,
		sizeof(ndvfm->c.out_dvfm));
	nr_dvfm = &ndvfm->c.nr_wr_dvfm;
	if (winc->x_size != nr_dvfm->vfs.width ||
	    winc->y_size != nr_dvfm->vfs.height) {
		nr_dvfm->vfs.width = winc->x_size;
		nr_dvfm->vfs.height = winc->y_size;
		nr_dvfm->src_w = nr_dvfm->vfs.width;
		nr_dvfm->src_h = nr_dvfm->vfs.height;
	}

	/* check mem */
	if (ndvfm->c.set_cfg.b.en_mem_cp_in) {
		memcpy(&ndvfm->c.mem_dvfm, in_dvfm,	sizeof(ndvfm->c.mem_dvfm));
	} else if (!ndvfm->c.cnt_display || !ndvfm_last) {//tmp
		ref_en = false;
		//ndvfm->c.set_cfg.b.en_mem_cvs = false;
		ndvfm->c.set_cfg.b.en_mem_mif = false;
		ndvfm->c.set_cfg.b.en_mem_afbcd = false;
		dbg_plink1("display:change mem:%d\n", ndvfm->c.cnt_in);
	} else {
		/* have mem */
		memcpy(&ndvfm->c.mem_dvfm,
			&ndvfm_last->c.nr_wr_dvfm,
			sizeof(ndvfm->c.mem_dvfm));
	}
	dim_print("display:set_cfg:0x%x\n", ndvfm->c.set_cfg.d32);
	if (ndvfm->c.set_cfg.b.en_mem_cvs) {
		/* cvs for mem */
		cvsp = &ndvfm->c.cvspara_mem;
		if (ndvfm_last) {
			memcpy(&ndvfm->c.cvspara_mem,
			       &ndvfm_last->c.cvspara_wr,
			       sizeof(ndvfm->c.cvspara_mem)); //???
		} else {
			memcpy(&ndvfm->c.cvspara_mem,
				       &ndvfm->c.cvspara_wr,
				       sizeof(ndvfm->c.cvspara_mem)); //???
		}
		cvsp->cvs_id		= cvs + 4;
		//cvs issue if (hw->en_pst_wr_test)
			cvs_link(cvsp, "mem");

		mifp = &ds->mifpara_mem;
		mifp->cvs_id[0] = (unsigned int)(*cvsp->cvs_id);
		mifp->cvs_id[1] = (unsigned int)(*(cvsp->cvs_id + 1));
	}
	if (hw->en_pst_wr_test) {
		dpvpph_prelink_sw(op_in, false);
	} else {
		if (hw->dis_last_para.dmode != EPVPP_DISPLAY_MODE_NR) {
			dpvpph_pre_to_link();
			afbcd_enable_only_t5dvb(op_in, true);
			dpvpph_prelink_sw(op_in, true);
		} else {
			if (DIM_IS_IC(T6D)) {
				if (in_dvfm->vfs.type & VIDTYPE_COMPRESS) {
					dim_print("vf type:0x%x\n", in_dvfm->vfs.type);
					afbcd_enable_only_t5dvb(op_in, true);
				} else {
					dim_print("no-afbc :0x%x\n", in_dvfm->vfs.type);
				}
			}
		}
	}
	/* mif in*/
	if (ndvfm->c.set_cfg.b.en_in_mif) {
		mif_cfg_v2(&ds->mif_in,
			&ndvfm->c.in_dvfm_crop, &ds->mifpara_in, EPVPP_API_MODE_PRE);
		if (!opl1()) {
			PR_ERR("not opl1?0x%px:\n", opl1());
			return;
		}
		if (!opl1()->pre_mif_set) {
			PR_ERR("not opl1z_pre_mif\n");
			return;
		}
		if (!op_in) {
			PR_ERR("no op_in?0x%px\n", op_in);
			return;
		}
		opl1()->pre_mif_set(&ds->mif_in, DI_MIF0_ID_INP, op_in);

		if (ndvfm->c.set_cfg.b.en_mem_cp_in)
			opl1()->pre_mif_set(&ds->mif_in, DI_MIF0_ID_MEM, op_in);
	}
	/* mif wr */
	if (ndvfm->c.set_cfg.b.en_wr_mif) {
		mif_cfg_v2(&ds->mif_wr,
			&ndvfm->c.nr_wr_dvfm, &ds->mifpara_out, EPVPP_API_MODE_PRE);
#ifdef DBG_FLOW_SETTING
		print_dvfm(&ndvfm->c.nr_wr_dvfm, "set:nr_wr_dvfm");
		dim_dump_mif_state(&ds->mif_wr, "set:mif_wr");
		print_dim_mifpara(&ds->mifpara_out, "display:all:out_use");
#endif

		opl1()->set_wrmif_pp(&ds->mif_wr, op_in, EDI_MIFSM_NR);
	}

	if (ndvfm->c.set_cfg.b.en_mem_cp_in) {
		dim_print("cp\n");
	} else if (ndvfm->c.set_cfg.b.en_mem_mif) {
		mif_cfg_v2(&ds->mif_mem,
			&ndvfm_last->c.nr_wr_dvfm, &ds->mifpara_mem, EPVPP_API_MODE_PRE);
#ifdef DBG_FLOW_SETTING
		print_dvfm(&ndvfm_last->c.nr_wr_dvfm, "set:mem_dvfm");
		dim_dump_mif_state(&ds->mif_mem, "set:mif_mem");
		print_dim_mifpara(&ds->mifpara_mem, "display:all:mem_use");
#endif
		opl1()->pre_mif_set(&ds->mif_mem, DI_MIF0_ID_MEM, op_in);
	} else {
		//use current output
		mif_cfg_v2(&ds->mif_mem,
			&ndvfm->c.nr_wr_dvfm, &ds->mifpara_mem, EPVPP_API_MODE_PRE);
		opl1()->pre_mif_set(&ds->mif_mem, DI_MIF0_ID_MEM, op_in);
	}
	/* afbc */
	if (dim_afds() && dim_afds()->rest_val)
		dim_afds()->rest_val();
	if (dim_afds() && dim_afds()->pvpp_pre_check_dvfm) /* same as pre_check */
		dim_afds()->pvpp_pre_check_dvfm(ds, ndvfm);
	/* use ndvfm's sgn_set??*/

	if (ndvfm_last &&
	    ndvfm_last->c.set_cfg.b.en_in_afbcd &&
	    !ndvfm->c.set_cfg.b.en_in_afbcd) {
		dbg_mem("close afbcd:%d\n", ndvfm->c.cnt_in);
		dim_afds()->pvpp_sw_setting_op(false, op_in);
	}

	if (ndvfm->c.set_cfg.b.en_mem_afbcd ||
	    ndvfm->c.set_cfg.b.en_in_afbcd ||
	    ndvfm->c.set_cfg.b.en_wr_afbce) {
		dim_print("%s:afbc set:0x%x:0x%x\n", __func__,
			ndvfm->c.set_cfg.d32,
			ndvfm->c.afbc_sgn_cfg.b.inp);
		dim_afds()->pvpp_en_pre_set(ds, ndvfm, op_in);
	}

	/* nr write */
	if (ndvfm->c.set_cfg.b.en_4k_snr || dpvpp_dbg_force_disable_ddr_wr())
		ds->pre_top_cfg.b.nr_ch0_en = 0;
	else
		ds->pre_top_cfg.b.nr_ch0_en = 1;
	if (DIM_IS_IC_EF(SC2))
		dim_sc2_contr_pre(&ds->pre_top_cfg, op_in);
	else
		dpvpph_nr_ddr_switch(ds->pre_top_cfg.b.nr_ch0_en, op_in);

	atomic_set(&ndvfm->c.wr_set, 1);
	/*********************************/
	if (DIM_IS_IC(T5DB)) {
		t5d_cnt = cfgg(T5DB_P_NOTNR_THD);
		if (t5d_cnt < 5)
			t5d_cnt = 5;
		if (ds->field_count_for_cont < t5d_cnt)
			bypass_mem = 1;
		else
			bypass_mem = 0;
	}
	if (!ndvfm->c.set_cfg.b.en_mem_mif && !ndvfm->c.set_cfg.b.en_mem_afbcd)
		bypass_mem = 1;

	if (bypass_mem)	//note: before en_4k_snr
		cp_mode = 1;
	if (ndvfm->c.set_cfg.b.en_4k_snr || dpvpp_dbg_force_mem_bypass())
		bypass_mem = 1;

	if (bypass_mem != ds->dbg_last_mem_bypass) {
		dbg_plink1("mem bypass:%d %d->%d\n", ndvfm->c.cnt_in,
				   ds->dbg_last_mem_bypass, bypass_mem);
		ds->dbg_last_mem_bypass = bypass_mem ? true : false;
	}
	if (cp_mode)
		bypass_mem |= DI_BIT2;
	dpvpph_enable_di_pre_aml(hw->op,
				 hw->en_pst_wr_test | sample_mode, bypass_mem << 4, NULL);
	dct_pst(hw->op, ndvfm);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		/* enable mc pre mif*/
		//dimh_enable_di_pre_mif(true, dimp_get(edi_mp_mcpre_en));
		//is dimh_enable_di_pre_mif
#ifdef HIS_CODE
		if (DIM_IS_IC_EF(SC2))
			opl1()->pre_mif_sw(true, op_in);
		else
			dpvpph_pre_data_mif_ctrl(true, op_in);
#else
		opl1()->pre_mif_sw(true, op_in, true);
#endif
		dpvpph_reverse_mif_ctrl(hw->dis_c_para.plink_reverse,
							hw->dis_c_para.plink_hv_mirror, op_in);
		dpvpph_pre_frame_reset_g12(!hw->en_pst_wr_test, op_in);
	} else { /* not test */
		dpvpph_pre_frame_reset(op_in);
		/* enable mc pre mif*/
		//dimh_enable_di_pre_mif(true, dimp_get(edi_mp_mcpre_en));
		dpvpph_pre_data_mif_ctrl(true, op_in);
	}
	dbg_mem("update all\n");
}

/* @ary 11-08 not used */
//static
void dpvpph_display_update_sub_last_cvs(struct dimn_itf_s *itf,
				struct dim_pvpp_ds_s *ds,
			       struct dimn_dvfm_s *ndvfm,
			       const struct reg_acc *op_in)
{
	struct dim_cvspara_s *cvsp;
	struct dimn_dvfm_s *ndvfm_last;
	static unsigned int last_nub;
	struct dim_pvpp_hw_s *hw;

	hw = &get_datal()->dvs_pvpp.hw;

	/* ndvfm_last */ /* same*/
	ndvfm_last = hw->dis_last_dvf;
	//dbg_di_prelink_reg_check_v3_op(op_in, !hw->en_pst_wr_test);

	if (!hw->en_pst_wr_test	&&
	    ndvfm_last &&
	    !ndvfm_last->c.set_cfg.b.en_linear_cp &&
	    atomic_read(&ndvfm_last->c.wr_set)	&&
	    !atomic_read(&ndvfm_last->c.wr_cvs_set)) {
		dim_print("%s:%d\n", __func__, ndvfm->c.cnt_display);
		cvsp = &ndvfm_last->c.cvspara_in;
		cvs_link(cvsp, "in_cvs");
		atomic_add(0x100, &itf->c.dbg_display_sts);	/* dbg only */
		if (ndvfm_last->c.set_cfg.b.en_wr_cvs) {
			cvsp = &ndvfm_last->c.cvspara_wr;
			cvs_link(cvsp, "cvs_wr");
			atomic_add(0x200, &itf->c.dbg_display_sts);/* dbg only */
		}
		if (ndvfm_last->c.set_cfg.b.en_mem_cvs) {
			cvsp = &ndvfm_last->c.cvspara_mem;
			cvs_link(cvsp, "cvs_mem");
			atomic_add(0x400, &itf->c.dbg_display_sts);/* dbg only */
		}
		/* check done, note, this is last two */
		last_nub = ds->pre_done_nub;
		/* cvs flg */
		atomic_inc(&ndvfm_last->c.wr_cvs_set);
	} else {
		if (last_nub == ds->pre_done_nub)
			return;
		last_nub = ds->pre_done_nub;
		if (!ndvfm_last) {
			dim_print("cvs_update: no last\n");
		} else {
			dim_print("linear:%d;wr_set[%d],cvs_set[%d]\n",
				ndvfm_last->c.set_cfg.b.en_linear_cp,
				atomic_read(&ndvfm_last->c.wr_set),
				atomic_read(&ndvfm_last->c.wr_cvs_set));
		}
	}
}

static void dpvpph_display_update_part(struct dim_pvpp_ds_s *ds,
			       struct dimn_dvfm_s *ndvfm,
			       const struct reg_acc *op_in,
			       unsigned int diff)
{
	struct dim_cvspara_s *cvsp;
	struct dvfm_s *in_dvfm, *out_dvfm, *nr_dvfm;
	unsigned char *cvs, pos;
	struct di_win_s *winc;
	bool ref_en = true;
	struct dim_mifpara_s *mifp;
	struct pvpp_buf_cfg_s *buf_cfg;
	struct dimn_dvfm_s *ndvfm_last;
	int i;
	unsigned int bypass_mem = 0;
	struct dim_pvpp_hw_s *hw;
	unsigned short t5d_cnt;
	unsigned int cp_mode = 0;
	unsigned int sample_mode = 2;

	hw = &get_datal()->dvs_pvpp.hw;

	/* ndvfm_last */ /* same*/
	ndvfm_last = hw->dis_last_dvf;
	buf_cfg = get_buf_cfg(ndvfm->c.out_fmt, 3);
	if (!buf_cfg) {
		PR_WARN("buf_cfg\n");
		return;
	}

	/* pos: 0 ~ 1 */
	pos = (1 - hw->cvs_last) & 1;
	hw->cvs_last = pos;
	/* address update */

	cvs = &hw->cvs[pos][0];
	in_dvfm = &ndvfm->c.in_dvfm_crop;
	winc	= &hw->dis_c_para.win;
	if (winc->x_size != in_dvfm->vfs.width ||
	    winc->y_size != in_dvfm->vfs.height) {
		in_dvfm->vfs.width = winc->x_size;
		in_dvfm->vfs.height = winc->y_size;
	}
	if (DIM_IS_IC(T6D) && in_dvfm->vfs.width <= 1280)
		sample_mode = 0;

	ds->mif_wr.tst_not_setcontr = 0;
	if (dim_is_pre_link_l() || !hw->en_pst_wr_test) { //tmp
		if (ds->cnt_display > 1)
			ds->mif_wr.tst_not_setcontr = 1;
		else
			ds->mif_wr.tst_not_setcontr = 0;
	}

	dim_print("%s:set_cfg:0x%x:tst_not_setcontr[%d]\n",
		__func__, ndvfm->c.set_cfg.d32,
		ds->mif_wr.tst_not_setcontr);
	/* cfg cvs */
	if (ndvfm->c.set_cfg.b.en_in_cvs) {
		if (in_dvfm->vfs.canvas0Addr == (u32)-1) {
			/* config cvs for input */
			cvsp = &ndvfm->c.cvspara_in;
			cvsp->plane_nub	= in_dvfm->vfs.plane_num;
			cvsp->cvs_cfg	= &in_dvfm->vfs.canvas0_config[0];
			cvsp->cvs_id	= cvs + 0;
			//cvs issue if (hw->en_pst_wr_test)
				cvs_link(cvsp, "in_cvs");
			mifp = &ds->mifpara_in;
			mifp->cvs_id[0] = (unsigned int)(*cvsp->cvs_id);
			mifp->cvs_id[1] = (unsigned int)(*(cvsp->cvs_id + 1));
		} else {
			cvsp = &ndvfm->c.cvspara_in;
			if (IS_NV21_12(in_dvfm->vfs.type))
				cvsp->plane_nub = 2;
			else if (in_dvfm->vfs.type & VIDTYPE_VIU_422)
				cvsp->plane_nub = 1;
			mifp = &ds->mifpara_in;
			mifp->cvs_id[0] = in_dvfm->vfs.canvas0Addr & 0xff;
			if (cvsp->plane_nub == 2)
				mifp->cvs_id[1] =
				(in_dvfm->vfs.canvas0Addr >> 16) & 0xff;
		}
	}

	if (ndvfm->c.set_cfg.b.en_wr_cvs || ndvfm->c.set_cfg.b.en_wr_mif) {
		//write address:
		for (i = 0; i < ndvfm->c.out_dvfm.vfs.plane_num; i++)
			ndvfm->c.out_dvfm.vfs.canvas0_config[0].phy_addr =
			buf_cfg->dbuf_wr_cfg[pos][i].phy_addr;
	} else if (ndvfm->c.set_cfg.b.en_wr_afbce) {
		ndvfm->c.out_dvfm.vfs.canvas0_config[0].phy_addr =
				buf_cfg->dbuf_wr_cfg[pos][0].phy_addr;
		ndvfm->c.out_dvfm.afbct_adr = buf_cfg->addr_e[pos];
		ndvfm->c.out_dvfm.vfs.compHeadAddr = buf_cfg->addr_i[pos];
	}

	if (ndvfm->c.set_cfg.b.en_wr_cvs) {
		/* cvs for out */
		cvsp = &ndvfm->c.cvspara_wr;
		out_dvfm = &ndvfm->c.out_dvfm;
		cvsp->plane_nub = out_dvfm->vfs.plane_num;
		cvsp->cvs_cfg		= &out_dvfm->vfs.canvas0_config[0];
		cvsp->cvs_id		= cvs + 2;
		//cvs issue if (hw->en_pst_wr_test)
			cvs_link(cvsp, "wr");
		mifp = &ds->mifpara_out;
		mifp->cvs_id[0] = (unsigned int)(*cvsp->cvs_id);
		mifp->cvs_id[1] = (unsigned int)(*(cvsp->cvs_id + 1));
	}

	/* copy out_dvfm to nr_wr_dvfm, then adjust size by crop */
	memcpy(&ndvfm->c.nr_wr_dvfm,
		&ndvfm->c.out_dvfm,
		sizeof(ndvfm->c.out_dvfm));
	nr_dvfm = &ndvfm->c.nr_wr_dvfm;
	if (winc->x_size != nr_dvfm->vfs.width ||
	    winc->y_size != nr_dvfm->vfs.height) {
		nr_dvfm->vfs.width = winc->x_size;
		nr_dvfm->vfs.height = winc->y_size;
		nr_dvfm->src_w = nr_dvfm->vfs.width;
		nr_dvfm->src_h = nr_dvfm->vfs.height;
	}

	/* check mem */
	if (ndvfm->c.set_cfg.b.en_mem_cp_in) {
		memcpy(&ndvfm->c.mem_dvfm, in_dvfm,	sizeof(ndvfm->c.mem_dvfm));
	} else if (!ndvfm->c.cnt_display || !ndvfm_last) {//tmp
		ref_en = false;
		ndvfm->c.set_cfg.b.en_mem_cvs = false;
		ndvfm->c.set_cfg.b.en_mem_mif = false;
		ndvfm->c.set_cfg.b.en_mem_afbcd = false;
		dbg_plink1("display:%d:up mem:0\n", ndvfm->c.cnt_in);
	} else {
#ifdef HIS_CODE
		if (ndvfm->c.mem_dvfm.vfs.type != ndvfm_last->c.nr_wr_dvfm.vfs.type) {
			dbg_plink1("mem:%d:type:0x%x->0x%x\n",
				ndvfm->c.cnt_in,
				ndvfm->c.mem_dvfm.vfs.type,
				ndvfm_last->c.nr_wr_dvfm.vfs.type);
		}
#endif
		/* have mem */
		memcpy(&ndvfm->c.mem_dvfm,
			&ndvfm_last->c.nr_wr_dvfm,
			sizeof(ndvfm->c.mem_dvfm));
	}

	dim_print("display:set_cfg:0x%x", ndvfm->c.set_cfg.d32);
	if (ndvfm->c.set_cfg.b.en_mem_cp_in) {
	} else if (ndvfm->c.set_cfg.b.en_mem_cvs) {
		/* cvs for out */
		cvsp = &ndvfm->c.cvspara_mem;
		//update no need
		if (ndvfm_last)
			memcpy(&ndvfm->c.cvspara_mem,
			       &ndvfm_last->c.cvspara_wr,
			       sizeof(ndvfm->c.cvspara_mem));

		cvsp->cvs_id		= cvs + 4;
		//cvs issue if (hw->en_pst_wr_test)
			cvs_link(cvsp, "mem");

		mifp = &ds->mifpara_mem;
		mifp->cvs_id[0] = (unsigned int)(*cvsp->cvs_id);
		mifp->cvs_id[1] = (unsigned int)(*(cvsp->cvs_id + 1));
	}
	if (!hw->en_pst_wr_test)
		dpvpph_prelink_polling_check(op_in, true);
	/* mif in*/
	if (ndvfm->c.set_cfg.b.en_in_mif) {
		mif_cfg_v2_update_addr(&ds->mif_in,
				       &ndvfm->c.in_dvfm_crop, &ds->mifpara_in);
		opl1()->mif_rd_update_addr(&ds->mif_in, DI_MIF0_ID_INP, op_in);
		if (ndvfm->c.set_cfg.b.en_mem_cp_in)
			opl1()->mif_rd_update_addr(&ds->mif_in, DI_MIF0_ID_MEM, op_in);
	}
	/* mif wr */
	if (ndvfm->c.set_cfg.b.en_wr_mif) {
		mif_cfg_v2_update_addr(&ds->mif_wr,
			&ndvfm->c.nr_wr_dvfm, &ds->mifpara_out);

		opl1()->wrmif_update_addr(&ds->mif_wr, op_in, EDI_MIFSM_NR);
#ifdef DBG_FLOW_SETTING
		print_dvfm(&ndvfm->c.nr_wr_dvfm, "set:nr_wr_dvfm");
		dim_dump_mif_state(&ds->mif_wr, "set:mif_wr");
		print_dim_mifpara(&ds->mifpara_out, "display:all:out_use");
#endif
	}
	if (ndvfm->c.set_cfg.b.en_mem_cp_in) {
		dim_print("part mem cp\n");
	} else if (ndvfm->c.set_cfg.b.en_mem_mif) {
		if (diff & EDIM_DVPP_DIFF_MEM) {
			mif_cfg_v2(&ds->mif_mem,
				&ndvfm_last->c.nr_wr_dvfm, &ds->mifpara_mem, EPVPP_API_MODE_PRE);
			opl1()->pre_mif_set(&ds->mif_mem, DI_MIF0_ID_MEM, op_in);
		} else {
			mif_cfg_v2_update_addr(&ds->mif_mem,
					       &ndvfm_last->c.nr_wr_dvfm,
					       &ds->mifpara_mem);
			opl1()->mif_rd_update_addr(&ds->mif_mem,
						   DI_MIF0_ID_MEM, op_in);

#ifdef DBG_FLOW_SETTING
			print_dvfm(&ndvfm_last->c.nr_wr_dvfm, "set:mem_dvfm");
			dim_dump_mif_state(&ds->mif_mem, "set:mif_mem");
			print_dim_mifpara(&ds->mifpara_mem, "display:all:mem_use");
#endif
		}
	}
	if (dim_afds() && dim_afds()->pvpp_pre_check_dvfm) /* same as pre_check */
		dim_afds()->pvpp_pre_check_dvfm(ds, ndvfm);

	if (ndvfm->c.set_cfg.b.en_mem_afbcd ||
	    ndvfm->c.set_cfg.b.en_in_afbcd ||
	    ndvfm->c.set_cfg.b.en_wr_afbce) {
		dim_print("%s:afbc set:0x%x:0x%x\n", __func__,
			ndvfm->c.set_cfg.d32,
			ndvfm->c.afbc_sgn_cfg.b.inp);
		dim_afds()->pvpp_en_pre_set(ds, ndvfm, op_in);
	}
	/* nr write switch */
	if (atomic_read(&ndvfm->c.wr_set)) {
		ds->pre_top_cfg.b.nr_ch0_en = 0;
	} else {
		ds->pre_top_cfg.b.nr_ch0_en = 1;
		atomic_set(&ndvfm->c.wr_set, 1);
	}
	if (ndvfm->c.set_cfg.b.en_4k_snr || dpvpp_dbg_force_disable_ddr_wr())
		ds->pre_top_cfg.b.nr_ch0_en = 0;

	if (DIM_IS_IC_EF(SC2)) {
		dim_sc2_contr_pre(&ds->pre_top_cfg, op_in);
	} else { //?
		dpvpph_nr_ddr_switch(ds->pre_top_cfg.b.nr_ch0_en, op_in);
	}
	/************************************/
	if (DIM_IS_IC(T5DB)) {
		t5d_cnt = cfgg(T5DB_P_NOTNR_THD);
		if (t5d_cnt < 5)
			t5d_cnt = 5;
		if (ds->field_count_for_cont < t5d_cnt)
			bypass_mem = 1;
		else
			bypass_mem = 0;
	}
	if (!ndvfm->c.set_cfg.b.en_mem_mif && !ndvfm->c.set_cfg.b.en_mem_afbcd)
		bypass_mem = 1;
	if (bypass_mem)
		cp_mode = 1;
	if (ndvfm->c.set_cfg.b.en_4k_snr || dpvpp_dbg_force_mem_bypass())
		bypass_mem = 1;
	if (dpvpp_dbg_force_mem_en())
		bypass_mem = 0;
	if (bypass_mem != ds->dbg_last_mem_bypass) {
		dbg_plink1("mem bypass:%d %d->%d\n", ndvfm->c.cnt_in,
				   ds->dbg_last_mem_bypass, bypass_mem);
		ds->dbg_last_mem_bypass = bypass_mem ? true : false;
	}

	if (cp_mode)
		bypass_mem |= DI_BIT2;
	dpvpph_enable_di_pre_aml(hw->op,
				 hw->en_pst_wr_test | sample_mode, bypass_mem << 4, NULL);
	dct_pst(hw->op, ndvfm);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		/* enable mc pre mif*/
		//dimh_enable_di_pre_mif(true, dimp_get(edi_mp_mcpre_en));
		//is dimh_enable_di_pre_mif
#ifdef HIS_CODE
		if (DIM_IS_IC_EF(SC2))
			opl1()->pre_mif_sw(true, op_in);
		else
			dpvpph_pre_data_mif_ctrl(true, op_in);
#else
		opl1()->pre_mif_sw(true, op_in, true);
#endif
		dpvpph_reverse_mif_ctrl(hw->dis_c_para.plink_reverse,
							hw->dis_c_para.plink_hv_mirror, op_in);
		dpvpph_pre_frame_reset_g12(!hw->en_pst_wr_test, op_in);
	} else {
		dpvpph_pre_frame_reset(op_in);
		/* enable mc pre mif*/
		//dimh_enable_di_pre_mif(true, dimp_get(edi_mp_mcpre_en));
		dpvpph_pre_data_mif_ctrl(true, op_in);
	}
}

static bool vtype_fill_d(struct dimn_itf_s *itf,
		struct vframe_s *vfmt,
		struct vframe_s *vfmf,
		struct dvfm_s *by_dvfm)
{
	int i;
	void *priv;
	unsigned int index;
	struct di_ch_s *pch;
	struct dcntr_mem_s *dcntr_mem = NULL;

	if (!vfmt || !vfmf || !by_dvfm || !itf)
		return false;
	/* keep private and index */
	priv = vfmt->private_data;
	index = vfmt->index;
	memcpy(vfmt, vfmf, sizeof(*vfmt));
	vfmt->private_data = priv;
	vfmt->index	= index;
	//type clear
	vfmt->type &= (~VFMT_DIPVPP_CHG_MASK);
	vfmt->type |= (by_dvfm->vfs.type & VFMT_DIPVPP_CHG_MASK);
	vfmt->bitdepth = by_dvfm->vfs.bitdepth;
	//flg clear
	vfmt->flag &= (~VFMT_FLG_CHG_MASK);
	vfmt->flag |= (by_dvfm->vfs.flag & VFMT_FLG_CHG_MASK);
	vfmt->di_flag = by_dvfm->vfs.di_flag;

	vfmt->canvas0Addr = (u32)-1;
	vfmt->canvas1Addr = (u32)-1;
	vfmt->di_instance_id = by_dvfm->sum_reg_cnt;
	if (by_dvfm->is_prvpp_link || dim_is_pre_link_l()) {
		dim_print("%s:by type:0x%x\n", __func__, by_dvfm->vfs.type);
		if (IS_COMP_MODE(by_dvfm->vfs.type)) { //
			/* afbce */
			vfmt->compBodyAddr	= by_dvfm->vfs.compBodyAddr;
			vfmt->compHeadAddr	= by_dvfm->vfs.compHeadAddr;
			vfmt->compWidth		= by_dvfm->vfs.width;
			vfmt->compHeight	= by_dvfm->vfs.height;
		} else  {
			vfmt->compBodyAddr = 0;
			vfmt->compHeadAddr = 0;
			vfmt->plane_num	= by_dvfm->vfs.plane_num;
			memset(&vfmt->canvas0_config[0],
			       0,
			       sizeof(vfmt->canvas0_config));
		}
		vfmt->vf_ext = by_dvfm->vf_ext;
		if (vfmt->vf_ext)
			vfmt->flag |= VFRAME_FLAG_DOUBLE_FRAM; /* for vf_ext */

		vfmt->early_process_fun = NULL;
		vfmt->type |= VIDTYPE_PRE_INTERLACE;
		dim_print("%s:pre-vpp link:0x%x:0x%px:%d\n", __func__,
			vfmt->type,
			vfmt,
			vfmt->index);

		vfmt->process_fun = NULL;

		if (IS_COMP_MODE(vfmf->type)) { //??
			vfmt->width = vfmt->compWidth;
			vfmt->height = vfmt->compHeight;
		}
		if (dpvpp_dbg_force_dech ||
		    dpvpp_dbg_force_decv) {
			vfmt->width		= by_dvfm->vfs.width; //tmp
			vfmt->height		= by_dvfm->vfs.height;
			dbg_plink1("dbg:force deh\n");
		}
		if (vfmt->di_flag & DI_FLAG_DI_PVPPLINK) {
			struct dimn_dvfm_s *dvfm;
			unsigned int ds_ratio = 0;

			dvfm = (struct dimn_dvfm_s *)vfmt->private_data;
			if (dvfm) {
				dcntr_mem = (struct dcntr_mem_s *)dvfm->c.dct_pre;
				dbg_plink2("%s: private_data:0x%px\n",
						__func__, vfmt->private_data);
			}
			pch = get_chdata(itf->bind_ch);
			if (dcntr_mem && pch && pch->dct_pre &&
				get_datal()->dct_op->mem_check(pch, dcntr_mem)) {
				ds_ratio = dcntr_mem->ds_ratio;
				if (itf->c.last_ds_ratio != ds_ratio)
					dbg_plink1("%s: ds_ratio:%d->%d\n",
						__func__, itf->c.last_ds_ratio, ds_ratio);
				itf->c.last_ds_ratio = ds_ratio;
			} else {
				/* keep the last_ds_ratio if meet dct pre timeout */
				dbg_plink2("%s:dct_pre not match %px\n",
					__func__, pch->dct_pre);
				ds_ratio = itf->c.last_ds_ratio;
			}
			ds_ratio = ds_ratio << DI_FLAG_DCT_DS_RATIO_BIT;
			ds_ratio &= DI_FLAG_DCT_DS_RATIO_MASK;
			vfmt->di_flag &= ~DI_FLAG_DCT_DS_RATIO_MASK;
			vfmt->di_flag |= ds_ratio;
		} else {
			vfmt->di_flag &= ~DI_FLAG_DCT_DS_RATIO_MASK;
		}
	} else {
		if (IS_COMP_MODE(by_dvfm->vfs.type)) {
			/* afbce */
			vfmt->compBodyAddr	= by_dvfm->vfs.compBodyAddr;
			vfmt->compHeadAddr	= by_dvfm->vfs.compHeadAddr;
		} else {
			vfmt->plane_num	= by_dvfm->vfs.plane_num;
			for (i = 0; i < vfmt->plane_num; i++)
				memcpy(&vfmt->canvas0_config[i],
				       &by_dvfm->vfs.canvas0_config[i],
				       sizeof(vfmt->canvas0_config[i]));
			vfmt->width		= by_dvfm->vfs.width; //tmp
			vfmt->height		= by_dvfm->vfs.height;
		}
		vfmt->type |= VIDTYPE_DI_PW;

		if (by_dvfm->is_itf_vfm)
			vfmt->flag |= VFRAME_FLAG_DI_PW_VFM;
		else if (by_dvfm->is_itf_ins_local)
			vfmt->flag |= VFRAME_FLAG_DI_PW_N_LOCAL;
		else
			vfmt->flag |= VFRAME_FLAG_DI_PW_N_EXT;
	}

	return true;
}

int set_holdreg_by_in_out(struct vframe_s *vfm, struct pvpp_dis_para_in_s *in_para,
			const struct reg_acc *op_in)
{
	const struct reg_acc *op;
	long display_time_us_per_frame;
	long v_percentage;
	long vfm_in_pixels;
	int out_frequency;
	int ret = 0;
	int ratio = 10;
	int screen_ratio = 100;

	static int cur_level;
	int clk_need;
	int i;
	struct di_hold_ctrl *ctrl = &reg_set;

	op = op_in;

	if (!DIM_IS_ICS_T5M)
		return ret;

	if (in_para->win.y_size > 1088 || in_para->win.x_size > 1920 ||
		vfm->width > 1920 || vfm->height > 1088 || vfm->compWidth > 1920 ||
		vfm->compHeight > 1088) {
		if (ctrl->last_clk_level != -1) {
			op->wr(DI_PRE_HOLD, 0x0);
			ctrl->last_clk_level = -1;
			dbg_plink1("4k no need set hold reg\n");
		}
		return ret;
	}

	if (in_para->vinfo.y_d_size == 0 || in_para->vinfo.x_d_size == 0 ||
		in_para->vinfo.vtotal == 0 || in_para->vinfo.htotal == 0) {
		return ret;
	}

	screen_ratio = in_para->vinfo.height * in_para->vinfo.width * 100 /
			in_para->vinfo.y_d_size / in_para->vinfo.x_d_size;
	if (screen_ratio < MAX_SCREEN_RATIO) {
		op->wr(DI_PRE_HOLD, 0x0);
		ctrl->last_clk_level = -1;
		dbg_plink1("screen_ratio oversize no need set hold reg\n");
		return ret;
	}

	if (dpvpp_dbg_force_disable_pre_hold())
		return ret;

	memcpy(ctrl->setting, setting, sizeof(setting));
	/*calculate current display need clk*/
	out_frequency = in_para->vinfo.frequency;
	v_percentage = in_para->vinfo.y_d_size * 100 / in_para->vinfo.vtotal;
	display_time_us_per_frame  = (1 * 1000000 / out_frequency) * v_percentage / 100;
	vfm_in_pixels = in_para->win.y_size * in_para->win.x_size;
	clk_need = vfm_in_pixels / display_time_us_per_frame;

	if (in_para->win.x_size > 1280 && in_para->win.y_size > 720)
		ratio = 10;
	if (in_para->win.x_size == 1280 && in_para->win.y_size == 720)
		ratio = 32;
	if (in_para->win.x_size == 640 && in_para->win.y_size == 480)
		ratio = 10;
	if (in_para->win.x_size > 640 && in_para->win.x_size < 1280)
		ratio = 10;
	clk_need = clk_need * ratio / 10;
	for (i = 0 ; i < MAX_DI_HOLD_CTRL_CNT; i++) {
		if (ctrl->setting[i].clk_val > clk_need) {
			cur_level = ctrl->setting[i].level_id;
			if (cur_level != ctrl->last_clk_level || cur_level == 0 ||
				ctrl->last_clk_level == 0) {
				dbg_plink1("<cur-%d switch last-%d>\n",
					cur_level, ctrl->last_clk_level);
				ctrl->last_clk_level = cur_level;
					op->wr(DI_PRE_HOLD, ctrl->setting[i].reg_val);
			}
			dbg_plink1("clk <need:%d set:%d > level :%d\n",
			clk_need, ctrl->setting[i].clk_val, ctrl->setting[i].level_id);
			break;
		}
	}

	dbg_plink1("%s total<%d,%d> vinfo <%d %d> %d\n vfm <%d,%d> x_d/y_d<%d %d> %ld\n"
		"vfm_pixels %ld v_percentage*100 %ld clk_val:reg_val <%d %x> ratio:%d\n",
		__func__,
		in_para->vinfo.htotal,
		in_para->vinfo.vtotal,
		in_para->vinfo.height,
		in_para->vinfo.width,
		in_para->vinfo.frequency,
		in_para->win.x_size,
		in_para->win.y_size,
		in_para->vinfo.x_d_size,
		in_para->vinfo.y_d_size,
		display_time_us_per_frame,
		vfm_in_pixels,
		v_percentage,
		ctrl->setting[cur_level].clk_val,
		ctrl->setting[cur_level].reg_val,
		ratio
		);

	return ret;
}

struct canvas_config_s *dpvpp_get_mem_cvs(unsigned int index)
{
	struct dim_pvpp_hw_s *hw;
	struct canvas_config_s *pcvs;

	hw = &get_datal()->dvs_pvpp.hw;

	pcvs = &hw->buf_cfg[EPVPP_BUF_CFG_T_HD_F].dbuf_wr_cfg[index][0];

	return pcvs;
}

/************************************/
/* also see enum EBLK_TYPE */
const char * const name_blk_t[] = {
	"hd_full",	/* 1080p yuv 422 10 bit */
	"hd_full_tvp",
	"uhd_f_afbce",
	"uhd_f_afbce_tvp",
	"uhd_full",
	"uhd_full_tvp",
	"hd_nv21",
	"hd_nv21_tvp",
	"l_dct",
	"l_dct_tvp",
	"s_afbc_table_4k"
};

static const char *get_name_blk_t(unsigned int idx)
{
	const char *p = "overflow";

	if (idx < ARRAY_SIZE(name_blk_t))
		p = name_blk_t[idx];
	return p;
}

struct c_mm_p_cnt_out_s *m_rd_mm_cnt(unsigned int id)
{
	struct dd_s *dd;

	dd = get_dd();
	if (!dd || id >= K_BLK_T_NUB)
		return NULL;

	return &dd->blk_cfg[id];
}

struct c_cfg_afbc_s *m_rd_afbc(unsigned int id)
{
	struct dd_s *dd;

	dd = get_dd();
	if (!dd || id >= K_AFBC_T_NUB)
		return NULL;

	return &dd->afbc_i[id];
}

struct c_cfg_dct_s *m_rd_dct(void)
{
	struct dd_s *dd;

	dd = get_dd();

	return &dd->dct_i;
}

bool dim_mm_alloc_api2(struct mem_a_s *in_para, struct dim_mm_s *o)
{
	bool ret = false;
//	int cma_mode;
	struct di_dev_s *de_devp = get_dim_de_devp();
	u64 timer_st, timer_end, diff; //debug only
	struct c_cfg_blki_s *blk_i;

#ifdef CONFIG_CMA
	if (!in_para || !in_para->inf || !o) {
		PR_ERR("%s:no input\n", __func__);
		return false;
	}
	dbg_plink2("%s: enter\n", __func__);
	blk_i = in_para->inf;
	timer_st = cur_to_msecs();//dbg

	if (blk_i->mem_from == MEM_FROM_CODEC) {//
		ret = mm_codec_alloc(DEVICE_NAME,
				     blk_i->page_size,
				     4,
				     o,
				     blk_i->tvp);
	} else if (blk_i->mem_from == MEM_FROM_CMA_DI) { //
		ret = mm_cma_alloc(&de_devp->pdev->dev, blk_i->page_size, o);
	} else if (blk_i->mem_from == MEM_FROM_CMA_C) { //
		ret = mm_cma_alloc(NULL, blk_i->page_size, o);
	} else {
		PR_ERR("%s:not finish function:%d:\n",
			__func__, blk_i->mem_from);
	}

	timer_end = cur_to_msecs();//dbg

	diff = timer_end - timer_st;
	if (!ret) {
		PR_ERR("%s:<%d,%d,0x%x>:%s,%s\n",
			__func__, blk_i->mem_from, blk_i->tvp, blk_i->page_size,
			in_para->owner, in_para->note);
	}
	dbg_plink1("%s:%ums:<%d,%d>:<0x%lx,0x%x>:%s,%s\n",
		__func__, (unsigned int)diff,
		blk_i->mem_from, blk_i->tvp,
		o->addr, blk_i->mem_size,
		in_para->owner, in_para->note);
#endif
	return ret;
}

bool dim_mm_release_api2(struct mem_r_s *in_para)
{
	bool ret = false;
	struct di_dev_s *de_devp = get_dim_de_devp();
	struct c_blk_m_s *blk;

#ifdef CONFIG_CMA
	if (!in_para || !in_para->blk) {
		PR_ERR("%s:no input\n", __func__);
		return false;
	}
	dbg_plink2("%s: enter\n", __func__);
	blk = in_para->blk;
	if (!blk->inf) {
		PR_ERR("%s: blk no inf\n", __func__);
		return false;
	}
	if (blk->inf->mem_from == MEM_FROM_CODEC) {
		codec_mm_free_for_dma(DEVICE_NAME, blk->mem_start);
		ret = true;
	} else if (blk->inf->mem_from == MEM_FROM_CMA_DI) {
		ret = dma_release_from_contiguous(&de_devp->pdev->dev,
						  blk->pages,
						  blk->inf->page_size);
	} else if (blk->inf->mem_from == MEM_FROM_CMA_C) {
		ret = dma_release_from_contiguous(NULL,
						  blk->pages,
						  blk->inf->page_size);
	}
	dbg_plink1("%s:r:<%d,%d>,<0x%lx 0x%x>:%s\n",
		__func__,
		blk->inf->mem_from,
		blk->inf->tvp,
		blk->mem_start,
		blk->inf->mem_size,
		in_para->note);
#endif
	return ret;
}

int m_a_blkt_cp(unsigned int ch, unsigned char *blkt_n)
{
	struct c_cfg_blkt_s *pblkt;
	struct dd_s *d_dd;

	if (!get_datal() || !get_datal()->dd)
		return -1;
	d_dd = (struct dd_s *)get_datal()->dd;
	if (ch >= __CH_MAX) {
		PR_ERR("%s:overflow:%d\n", __func__, ch);
		return -2;
	}
	pblkt = &d_dd->blkt_d[ch];

	if (!pblkt) {
		PR_ERR("%s: no input?\n", __func__);
		return -1;
	}

	mutex_lock(&pblkt->lock_cfg);
	memcpy(&pblkt->blkt_n[0], blkt_n, sizeof(pblkt->blkt_n));
	pblkt->flg_update = true;
	mutex_unlock(&pblkt->lock_cfg);
	mtask_wake_m();
	return 0;
}

static int count_mm_afbc(struct c_mm_p_cnt_in_s *i_cfg,
			struct c_cfg_afbc_s *o_cfg)
{
	unsigned int afbc_info_size = 0, afbc_tab_size = 0;
	unsigned int afbc_buffer_size = 0, blk_total = 0;
	unsigned int width, height;
	bool is_yuv420_10 = false;

	if (!dim_afds() || !dim_afds()->cnt_info_size ||
	    !i_cfg || !o_cfg) {
		PR_WARN("%s:no api\n", __func__);
		return 0;
	}

	width	= i_cfg->w;
	height	= i_cfg->h;

	afbc_info_size =
		dim_afds()->cnt_info_size(width,
					  height,
					  &blk_total);

	if (i_cfg->mode == EDPST_MODE_420_10BIT)
		is_yuv420_10 = true;

	if (is_yuv420_10)
		afbc_buffer_size =
		dim_afds()->cnt_buf_size(0x20, blk_total);
	else
		afbc_buffer_size =
			dim_afds()->cnt_buf_size(0x21, blk_total);
	afbc_buffer_size = roundup(afbc_buffer_size, PAGE_SIZE);

	afbc_tab_size =
		dim_afds()->cnt_tab_size(afbc_buffer_size);

	o_cfg->size_i	= afbc_info_size;
	o_cfg->size_table = afbc_tab_size;
	o_cfg->size_buffer = afbc_buffer_size;
	return 0;
}

/*copy from cnt_mm_info_simple_p */
static int count_mm_p(struct c_mm_p_cnt_in_s *i_cfg,
		struct c_mm_p_cnt_out_s *o_cfg)
{
	unsigned int tmpa, tmpb;
	unsigned int height;
	unsigned int width;
	unsigned int canvas_height;

	unsigned int nr_width;
	unsigned int canvas_align_width = 32;

	if (!i_cfg || !o_cfg) {
		PR_ERR("%s:no input\n", __func__);
		return -1;
	}
	height	= i_cfg->h;
	canvas_height = roundup(height, 32);
	width	= i_cfg->w;
	nr_width = width;
	o_cfg->mode = i_cfg->mode;
	/**********************************************/
	/* count buf info */
	/**********************************************/
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		canvas_align_width = 64;

	if (i_cfg->mode == EDPST_MODE_422_10BIT_PACK)
		nr_width = (width * 5) / 4;
	else if (i_cfg->mode == EDPST_MODE_422_10BIT)
		nr_width = (width * 3) / 2;
	else
		nr_width = width;

	/* p */
	//nr_width = roundup(nr_width, canvas_align_width);
	o_cfg->dbuf_hsize = roundup(width, canvas_align_width);

	if (i_cfg->mode == EDPST_MODE_NV21_8BIT) {
		nr_width = roundup(nr_width, canvas_align_width);
		tmpa = (nr_width * canvas_height) >> 1;/*uv*/
		tmpb = roundup(tmpa, PAGE_SIZE);
		tmpa = roundup(nr_width * canvas_height, PAGE_SIZE);

		o_cfg->off_uv		= tmpa;
		o_cfg->size_total	= tmpa + tmpb;
		o_cfg->size_page = o_cfg->size_total >> PAGE_SHIFT;
		o_cfg->cvs_w	= nr_width;
		o_cfg->cvs_h	= canvas_height;
	} else {
		/* 422 */
		tmpa = roundup(nr_width * canvas_height * 2, PAGE_SIZE);
		o_cfg->size_total = tmpa;
		o_cfg->size_page = o_cfg->size_total >> PAGE_SHIFT;
		o_cfg->cvs_w	= nr_width << 1;
		o_cfg->cvs_h	= canvas_height;
		o_cfg->off_uv	= o_cfg->size_total;
	}

	return 0;
}

void m_polling(void)
{
	struct dd_s *d_dd;
	struct di_data_l_s *pdata;
	int i, j;
	struct c_cfg_blkt_s *pblkt;
	unsigned int flash_cnt = 0, buf_cnt;
	int alloc_cnt = 0;
	struct blk_s *blk;
	struct c_cfg_blki_s *blk_i;
	struct dim_mm_s oret;
	bool aret, flg_a = false;
	struct mem_r_s r_para;
	struct mem_a_s a_para;

	if (!IS_IC_SUPPORT(PRE_VPP_LINK) || !cfgg(EN_PRE_LINK))
		return;
	pdata = get_datal();
	if (!pdata)
		return;
	d_dd = (struct dd_s *)pdata->dd;

	/* flash from d */
	for (i = 0; i < __CH_MAX; i++) {
		pblkt = &d_dd->blkt_d[i];
		if (pblkt && pblkt->flg_update) {
			mutex_lock(&pblkt->lock_cfg);
			memcpy(&d_dd->blkt_m[i][0], &pblkt->blkt_n[0],
				sizeof(d_dd->blkt_m[i]));
			pblkt->flg_update = 0;
			mutex_unlock(&pblkt->lock_cfg);
			flash_cnt++;
		} else {
			continue;
		}
	}

	/* count */
	if (flash_cnt) {
		for (j = 0; j < K_BLK_T_NUB; j++) {
			buf_cnt = 0;
			for (i = 0; i < __CH_MAX; i++)
				buf_cnt += d_dd->blkt_m[i][j];
			d_dd->blkt_tgt[j] = buf_cnt;
		}
		dbg_mem2("%s:flash_cnt:%d\n", __func__, flash_cnt);
	}

	/* polling release */
	for (j = 0; j < K_BLK_T_NUB; j++) {
		if (d_dd->blkt_tgt[j] < d_dd->blkt_crr[j]) {
			buf_cnt = d_dd->blkt_crr[j] - d_dd->blkt_tgt[j];
			for (i = 0; i < buf_cnt; i++) {
				//blk = qblk_get(&d_dd->f_blk_t[j]);
				blk = blk_o_get_locked(d_dd, j);
				if (!blk) {
					PR_WARN("%s:release:[%d]:<crr:%d tgt:%d>:break\n",
						__func__, j, i, buf_cnt);
					break;
				}
				memset(&r_para, 0, sizeof(r_para));
				r_para.blk = &blk->c.b.blk_m;//note: tmp;
				r_para.note = get_name_blk_t(j);
				dim_mm_release_api2(&r_para);
				memset(&blk->c, 0, sizeof(blk->c));
				blk_idle_put(d_dd, blk);
				d_dd->blkt_crr[j]--;
			}
		} else if (d_dd->blkt_tgt[j] > d_dd->blkt_crr[j]) {
			alloc_cnt += (d_dd->blkt_tgt[j] - d_dd->blkt_crr[j]);
		}
	}
	if (alloc_cnt <= 0)
		return;
	/* polling alloc */
	for (j = 0; j < K_BLK_T_NUB; j++) {
		if (d_dd->blkt_tgt[j] > d_dd->blkt_crr[j]) {
			blk = blk_idle_get(d_dd);
			if (!blk) {
				PR_ERR("%s:alloc:no idle\n", __func__);
				break;
			}
			//PR_INF("idle:0x%x\n", blk);
			blk_i = &d_dd->blki[j];
			memset(&oret, 0, sizeof(oret));
			memset(&a_para, 0, sizeof(a_para));
			a_para.inf = blk_i;
			a_para.owner = get_name_blk_t(j);
			a_para.note	= "tmp";
			aret = dim_mm_alloc_api2(&a_para, &oret);
			if (aret) {
				blk->c.blk_typ = j;
				blk->c.b.blk_m.inf = blk_i;
				blk->c.b.blk_m.flg_alloc = true;
				blk->c.b.blk_m.mem_start = oret.addr;
				blk->c.b.blk_m.pages	= oret.ppage;
				blk->c.st_id = EBLK_ST_ALLOC;
				d_dd->blkt_crr[j]++;
				alloc_cnt--;
				//PR_INF("alloc:%s:\n", a_para.owner);
				blk_o_put_locked(d_dd, j, blk);
				flg_a = true;
			} else {
				PR_WARN("alloc failed\n");
				blk_idle_put(d_dd, blk);
			}
			break;
		}
	}

	if (alloc_cnt > 0) {
		//PR_INF("%s:trig:%d:\n", __func__, alloc_cnt);
		mtask_wake_m();
	}
	if (flg_a)
		task_send_ready(31);
}

static unsigned int cnt_dct_buf_size(struct c_cfg_dct_s *dct_inf)
{
	int grd_size = SZ_512K;		/*(81*8*16 byte)*45 = 455K*/
	int yds_size = SZ_512K + SZ_32K;    /*960 * 576 = 540K*/
	int cds_size = yds_size / 2;	    /*960 * 576 / 2= 270K*/
	int total_size = grd_size + yds_size + cds_size;

	if (!dct_inf) {
		PR_ERR("%s:\n", __func__);
		return 0;
	}
	dct_inf->grd_size	= grd_size;
	dct_inf->yds_size	= yds_size;
	dct_inf->cds_size	= cds_size;
	dct_inf->size_one	= total_size;

	return total_size;
}

static void dim_reg_buf_i(void)
{
	struct di_data_l_s *pdata;
	struct dd_s *d_dd;
//	struct c_cfg_blkt_s *pblkt;
	struct c_cfg_blki_s *blk_i;
	struct c_mm_p_cnt_in_s *i_cfg, i_cfg_d;
	struct c_mm_p_cnt_out_s *o_cfg, o_cfg_d;
	struct c_cfg_afbc_s *afbc_cfg, afbc_cfg_d;
	int i;
	unsigned int size_tt;
	bool flg_special;

	pdata = get_datal();
	if (!pdata || !pdata->dd)
		return;
	d_dd = (struct dd_s *)pdata->dd;
	i_cfg = &i_cfg_d;
	o_cfg = &o_cfg_d;
	afbc_cfg = &afbc_cfg_d;

	/* count afbc infor */
	memset(i_cfg, 0, sizeof(*i_cfg));

	for (i = 0; i < K_AFBC_T_NUB; i++) {
		memset(afbc_cfg, 0, sizeof(*afbc_cfg));

		if (i == EAFBC_T_HD_FULL) {
			i_cfg->w = 1920;
			i_cfg->h = 1088;
			i_cfg->mode = EDPST_MODE_422_10BIT_PACK;
		} else if (i == EAFBC_T_4K_FULL) {
			i_cfg->w = 3840;
			i_cfg->h = 2160;
			i_cfg->mode = EDPST_MODE_422_10BIT_PACK;
		} else if (i ==  EAFBC_T_4K_YUV420_10) {
			i_cfg->w = 3840;
			i_cfg->h = 2160;
			i_cfg->mode = EDPST_MODE_420_10BIT;
		} else { /* to-do other format*/
			PR_WARN("%s:type %d is replay by %d\n",
				__func__, i, EAFBC_T_4K_FULL);
			i_cfg->mode = EDPST_MODE_422_10BIT_PACK;
		}
		count_mm_afbc(i_cfg, afbc_cfg);
		memcpy(&d_dd->afbc_i[i], afbc_cfg, sizeof(d_dd->afbc_i[i]));
	}
	/* count dct */
	d_dd->dct_i.nub = cfgg(POST_NUB) + 1;//DIM_P_LINK_DCT_NUB;
	d_dd->dct_i.size_one = cnt_dct_buf_size(&d_dd->dct_i);
	d_dd->dct_i.size_total = d_dd->dct_i.size_one * d_dd->dct_i.nub;
	/* count buffer base */
	for (i = 0; i < K_BLK_T_NUB; i++) {
		blk_i = &d_dd->blki[i];
		memset(i_cfg, 0, sizeof(*i_cfg));
		memset(o_cfg, 0, sizeof(*o_cfg));
		flg_special = false;
		if (i == EBLK_T_HD_FULL) {
			i_cfg->w = 1920;
			i_cfg->h = 1088;
			i_cfg->mode = EDPST_MODE_422_10BIT_PACK;

			count_mm_p(i_cfg, o_cfg);
			memcpy(&d_dd->blk_cfg[i], o_cfg, sizeof(d_dd->blk_cfg[i]));
			blk_i->blk_t = i;
			blk_i->mem_size = o_cfg->size_total;
			blk_i->page_size = o_cfg->size_page;
			blk_i->tvp	= false;
			blk_i->mem_from	= MEM_FROM_CODEC;
			blk_i->cnt_cfg = &d_dd->blk_cfg[i];
		} else if (i == EBLK_T_UHD_FULL) {
			i_cfg->w = 3840;
			i_cfg->h = 2160;
			i_cfg->mode = EDPST_MODE_422_10BIT_PACK;

			count_mm_p(i_cfg, o_cfg);
			memcpy(&d_dd->blk_cfg[i], o_cfg, sizeof(d_dd->blk_cfg[i]));
			blk_i->blk_t = i;
			blk_i->mem_size = o_cfg->size_total;
			blk_i->page_size = o_cfg->size_page;
			blk_i->tvp	= false;
			blk_i->mem_from	= MEM_FROM_CODEC;
			blk_i->cnt_cfg = &d_dd->blk_cfg[i];
		} else if (i == EBLK_T_HD_NV21) {
			i_cfg->w = 1920;
			i_cfg->h = 1088;
			i_cfg->mode = EDPST_MODE_NV21_8BIT;

			count_mm_p(i_cfg, o_cfg);
			memcpy(&d_dd->blk_cfg[i], o_cfg, sizeof(d_dd->blk_cfg[i]));
			blk_i->blk_t = i;
			blk_i->mem_size = o_cfg->size_total;
			blk_i->page_size = o_cfg->size_page;
			blk_i->tvp	= false;
			blk_i->mem_from	= MEM_FROM_CODEC;
			blk_i->cnt_cfg = &d_dd->blk_cfg[i];
		} else if (i == EBLK_T_LINK_DCT) {
			d_dd->blk_cfg[i].size_total = d_dd->dct_i.size_total;
			d_dd->blk_cfg[i].size_page =
				d_dd->blk_cfg[i].size_total >> PAGE_SHIFT;

			blk_i->blk_t = i;
			blk_i->mem_size = d_dd->blk_cfg[i].size_total;
			blk_i->page_size = d_dd->blk_cfg[i].size_page;
			blk_i->tvp	= false;
			blk_i->mem_from	= MEM_FROM_CODEC;
			blk_i->cnt_cfg = &d_dd->blk_cfg[i];
		} else if (i == EBLK_T_S_LINK_AFBC_T_FULL) {
			/**/
			size_tt = d_dd->afbc_i[EAFBC_T_4K_FULL].size_table;
			size_tt = size_tt * DPVPP_BUF_NUB;
			size_tt = roundup(size_tt, PAGE_SIZE);
			blk_i->blk_t = i;
			blk_i->mem_from = MEM_FROM_CODEC;
			blk_i->tvp = false;
			blk_i->mem_size = size_tt;
			blk_i->page_size = size_tt >> PAGE_SHIFT;
			memset(&d_dd->blk_cfg[i], 0, sizeof(d_dd->blk_cfg[i]));
			d_dd->blk_cfg[i].mode = EDPST_MODE_420_10BIT;
			d_dd->blk_cfg[i].size_total = blk_i->mem_size;
			d_dd->blk_cfg[i].size_page = blk_i->page_size;
			//note: here: buffer is larger than uhd_full,
			//not set ->cnt_cfg
		} else {
			flg_special = true;
		}
		if (!flg_special)  //dbg only
			dbg_plink2("%s:%d:cnt\n", __func__, i);
	}
	/* count buffer special */
	for (i = 0; i < K_BLK_T_NUB; i++) {
		blk_i = &d_dd->blki[i];
		if (i == EBLK_T_HD_FULL_TVP) {
			memcpy(blk_i, &d_dd->blki[EBLK_T_HD_FULL], sizeof(*blk_i));
			blk_i->blk_t = EBLK_T_HD_FULL_TVP;
			blk_i->tvp = true;
			memcpy(&d_dd->blk_cfg[i], &d_dd->blk_cfg[EBLK_T_HD_FULL],
				sizeof(d_dd->blk_cfg[i]));
		} else if (i == EBLK_T_UHD_FULL_TVP) {
			memcpy(blk_i, &d_dd->blki[EBLK_T_UHD_FULL], sizeof(*blk_i));
			blk_i->blk_t = EBLK_T_UHD_FULL_TVP;
			blk_i->tvp = true;
			memcpy(&d_dd->blk_cfg[i], &d_dd->blk_cfg[EBLK_T_UHD_FULL],
				sizeof(d_dd->blk_cfg[i]));
		} else if (i == EBLK_T_HD_NV21_TVP) {
			memcpy(blk_i, &d_dd->blki[EBLK_T_HD_NV21], sizeof(*blk_i));
			blk_i->blk_t = EBLK_T_HD_NV21_TVP;
			blk_i->tvp = true;
			memcpy(&d_dd->blk_cfg[i], &d_dd->blk_cfg[EBLK_T_HD_NV21],
				sizeof(d_dd->blk_cfg[i]));
		} else if (i == EBLK_T_UHD_FULL_AFBCE) {
			/* blk_cfg */
			memcpy(&d_dd->blk_cfg[i], &d_dd->blk_cfg[EBLK_T_UHD_FULL],
				sizeof(d_dd->blk_cfg[i]));

			size_tt = d_dd->afbc_i[EAFBC_T_4K_FULL].size_i +
				d_dd->afbc_i[EAFBC_T_4K_FULL].size_buffer;
			blk_i->blk_t = i;
			blk_i->mem_size = size_tt;
			blk_i->page_size = blk_i->mem_size >> PAGE_SHIFT;

			d_dd->blk_cfg[i].size_total = blk_i->mem_size;
			d_dd->blk_cfg[i].size_page = blk_i->page_size;

			blk_i->tvp	= false;
			blk_i->mem_from	= MEM_FROM_CODEC;
			blk_i->cnt_cfg = &d_dd->blk_cfg[i];
			//dbg_str_mm_cnt(o_cfg, "a");
		} else if (i == EBLK_T_UHD_FULL_AFBCE_TVP) {
			memcpy(blk_i, &d_dd->blki[EBLK_T_UHD_FULL_AFBCE], sizeof(*blk_i));
			blk_i->blk_t = i;
			blk_i->tvp = true;
			memcpy(&d_dd->blk_cfg[i], &d_dd->blk_cfg[EBLK_T_UHD_FULL_AFBCE],
				sizeof(d_dd->blk_cfg[i]));
		} else if (i == EBLK_T_LINK_DCT_TVP) {
			memcpy(blk_i, &d_dd->blki[EBLK_T_LINK_DCT], sizeof(*blk_i));
			blk_i->blk_t = i;
			blk_i->tvp = true;
			memcpy(&d_dd->blk_cfg[i], &d_dd->blk_cfg[EBLK_T_LINK_DCT],
				sizeof(d_dd->blk_cfg[i]));
		}
	}
}

bool dd_exit(void)
{
	struct dd_s *d_dd;
	struct di_data_l_s *pdata;
	int i;

	pdata = get_datal();
	if (!pdata)
		return false;
	d_dd = (struct dd_s *)pdata->dd;

	UCF_RELEASE(&d_dd->f_dis_free);
	UCF_RELEASE(&d_dd->f_blk_idle);
	for (i = 0; i < K_BLK_T_NUB; i++)
		UCF_RELEASE(&d_dd->f_blk_t[i]);

	kfree(d_dd);
	pdata->dd = NULL;
	return true;
}

bool dd_probe(void)
{
	struct dd_s *d_dd;
	struct di_data_l_s *pdata;
	unsigned int err_cnt = 0;
	int i;
	struct c_hd_s *hd;
	struct blk_s *blk;

	pdata = get_datal();
	if (!pdata)
		return false;

	d_dd = kzalloc(sizeof(*d_dd), GFP_KERNEL);
	if (!d_dd) {
		PR_ERR("%s fail to allocate memory.\n", __func__);
		goto fail_kmalloc_dd;
	}
	pdata->dd = d_dd;
	/* mutex */
	for (i = 0; i < __CH_MAX; i++)
		mutex_init(&d_dd->blkt_d[i].lock_cfg);

	/* que */
	err_cnt = 0;
	i = 0;
	err_cnt += UCF_INIT(&d_dd->f_dis_free,	__SIZE_DIS_FREE, i++, "dis_free");
	err_cnt += UCF_INIT(&d_dd->f_blk_idle,	__SIZE_BLK_IDLE, i++, "blk_idle");
	err_cnt += UCF_INIT(&d_dd->f_blk_t[EBLK_T_HD_FULL],
		__SIZE_BLK_PLINK_HD, i++, get_name_blk_t(EBLK_T_HD_FULL));
	err_cnt += UCF_INIT(&d_dd->f_blk_t[EBLK_T_HD_FULL_TVP],
		__SIZE_BLK_PLINK_HD, i++, get_name_blk_t(EBLK_T_HD_FULL_TVP));
	err_cnt += UCF_INIT(&d_dd->f_blk_t[EBLK_T_UHD_FULL_AFBCE],
		__SIZE_BLK_PLINK_UHD, i++, get_name_blk_t(EBLK_T_UHD_FULL_AFBCE));
	err_cnt += UCF_INIT(&d_dd->f_blk_t[EBLK_T_UHD_FULL_AFBCE_TVP],
		__SIZE_BLK_PLINK_UHD, i++,
		get_name_blk_t(EBLK_T_UHD_FULL_AFBCE_TVP));
	err_cnt += UCF_INIT(&d_dd->f_blk_t[EBLK_T_UHD_FULL],
		__SIZE_BLK_PLINK_UHD, i++, get_name_blk_t(EBLK_T_UHD_FULL));
	err_cnt += UCF_INIT(&d_dd->f_blk_t[EBLK_T_UHD_FULL_TVP],
		__SIZE_BLK_PLINK_UHD, i++, get_name_blk_t(EBLK_T_UHD_FULL_TVP));
	err_cnt += UCF_INIT(&d_dd->f_blk_t[EBLK_T_HD_NV21],
		__SIZE_BLK_PLINK_UHD, i++, get_name_blk_t(EBLK_T_HD_NV21));
	err_cnt += UCF_INIT(&d_dd->f_blk_t[EBLK_T_HD_NV21_TVP],
		__SIZE_BLK_PLINK_UHD, i++, get_name_blk_t(EBLK_T_HD_NV21_TVP));
	err_cnt += UCF_INIT(&d_dd->f_blk_t[EBLK_T_LINK_DCT],
		__SIZE_BLK_PLINK_UHD, i++, get_name_blk_t(EBLK_T_LINK_DCT));
	err_cnt += UCF_INIT(&d_dd->f_blk_t[EBLK_T_LINK_DCT_TVP],
		__SIZE_BLK_PLINK_UHD, i++, get_name_blk_t(EBLK_T_LINK_DCT_TVP));
	err_cnt += UCF_INIT(&d_dd->f_blk_t[EBLK_T_S_LINK_AFBC_T_FULL],
				__SIZE_BLK_PLINK_UHD, i++,
				get_name_blk_t(EBLK_T_S_LINK_AFBC_T_FULL));
	if (err_cnt)
		goto fail_dd_fifo;

	/* blk init */
	for (i = 0; i < __NUB_BLK_MAX; i++) {
		hd	= &d_dd->hd_blk[i];
		blk	= &d_dd->d_blk[i];

		hd->code = CODE_DD_BLK;
		hd->hd_tp = HD_T_BLK;
		hd->idx	= i;
		hd->p	= blk;
		blk->hd	= hd;
		atomic_set(&blk->get, 0);
		blk_idle_put(d_dd, blk);
	}

	dim_reg_buf_i();
	dbg_plink1("%s:ok\n", __func__);
	return true;
fail_dd_fifo:
	UCF_RELEASE(&d_dd->f_dis_free);
	UCF_RELEASE(&d_dd->f_blk_idle);
	for (i = 0; i < K_BLK_T_NUB; i++)
		UCF_RELEASE(&d_dd->f_blk_t[i]);

	kfree(d_dd);
	pdata->dd = NULL;
fail_kmalloc_dd:
	return false;
}
