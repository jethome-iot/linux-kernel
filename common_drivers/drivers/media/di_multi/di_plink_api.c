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

#define DIM_PVPP_NAME	"di_pvpp"

/* MAX_FIFO_SIZE */
#define MAX_FIFO_SIZE_PVPP		(32)
//#define AUTO_NR_DISABLE		(1)
//#define DBG_TIMER		(1)
//#define DBG_UNREG_LOG		(1)
//#define DBG_CHECK_VF		(1)
//#define DBG_QUE_LOCAL		(1)
//#define DBG_FLOW_SETTING		(1)
//#define DBG_QUE_IN_OUT		(1)
//#define DBG_QUE_INTERFACE	(1)
//#define DBG_OUT_SIZE		(1)
DEFINE_SPINLOCK(lock_pvpp);

unsigned int tst_plink_vpp;
module_param_named(tst_plink_vpp, tst_plink_vpp, uint, 0664);

bool dim_is_creat_p_vpp_link(void)
{
	if (tst_plink_vpp & DI_BIT4)
		return true;
	return false;
}

static unsigned char dim_dbg_is_disable_nr(void)
{
	unsigned char ret = 0;

	if (tst_plink_vpp & DI_BIT6)
		ret |= DI_BIT7;

	if (tst_plink_vpp & DI_BIT7)
		ret |= DI_BIT0;

	return ret;
}

static bool dpvpp_is_bypass(void)
{
	if (tst_plink_vpp & DI_BIT8)
		return true;
	return false;
}

bool dpvpp_is_not_active_di(void)
{
	if (tst_plink_vpp & DI_BIT9)
		return true;
	return false;
}

// return : true: update; false: not update
bool dpvpp_set_one_time(void)
{
	if (!(tst_plink_vpp & DI_BIT10))
		return true;
	if (!(tst_plink_vpp & DI_BIT11)) {
		tst_plink_vpp |= DI_BIT11;
		return true; //need set
	}
	return false;
}

static bool dpvpp_dbg_trig_change_all_one(void)
{
	if (tst_plink_vpp & DI_BIT12) {
		tst_plink_vpp &= (~DI_BIT12);
		return true;
	}
	return false;
}

static bool dpvpp_dbg_force_same(void)
{
	if (tst_plink_vpp & DI_BIT13)
		return true;

	return false;
}

static bool dpvpp_dbg_force_update_all(void)
{
	if (tst_plink_vpp & DI_BIT17)
		return true;

	return false;
}

bool dpvpp_dbg_force_bypass_2(void)
{
	if (tst_plink_vpp & DI_BIT19)
		return true;

	if (dimp_get(edi_mp_di_debug_flag) & 0x100000)
		return true;

	return false;
}

bool dpvpp_dbg_en_irq(void)
{
	if (tst_plink_vpp & DI_BIT24)
		return true;
	return false;
}

bool dpvpp_bypass_display(void)
{
	if (tst_plink_vpp & DI_BIT28)
		return true;
	return false;
}

bool dpvpp_dbg_tnr_disable(void)
{
	if (tst_plink_vpp & DI_BIT27)
		return true;
	return false;
}

//when 1: 4k used tnr; when 0: 4k used snr;
//
bool dpvpp_dbg_4k_tsnr(void)
{
	if (tst_plink_vpp & DI_BIT29)
		return true;
	return false;
}

void ext_vpp_disable_plink_notify(bool async)
{
#ifndef VPP_LINK_USED_FUNC
	u64 ustime, udiff;

	dbg_plink1("ext:%s:begin\n", "di_disable_plink_notify");
	ustime = cur_to_usecs();
	di_disable_plink_notify(async);
	udiff = cur_to_usecs() - ustime;
	dbg_plink1("ext:%s:%dus:%d\n", "di_disable_plink_notify",
	       (unsigned int)udiff, async);
#else
	PR_WARN("dbg:%s:no this function\n", __func__);
#endif
}

void ext_vpp_plink_state_changed_notify(void)
{
#ifndef VPP_LINK_USED_FUNC
	dbg_plink1("ext:plink notify:begin\n");
	di_plink_state_changed_notify();
	dbg_plink1("ext:plink notify\n");
#else
	PR_WARN("dbg:%s:no this function\n", __func__);
#endif
}

/* allow vpp sw other*/
void ext_vpp_plink_real_sw(bool sw, bool wait, bool interlace)
{
#ifndef VPP_LINK_USED_FUNC
	di_plink_force_dmc_priority(sw, wait, interlace);
#else
	PR_WARN("dbg:%s:no this function\n", __func__);
#endif
}

static bool instance_id_check(unsigned int instance_id)
{
	struct dimn_itf_s *itf;
	int i;
	struct dim_dvs_pvpp_s *pvpp;
	bool ret = false;

	pvpp = &get_datal()->dvs_pvpp;

	for (i = 0; i < DI_PLINK_CN_NUB; i++) {
		itf = &pvpp->itf[i];
		if (atomic_read(&itf->reg) &&
		    /* check if in unreg bottom half */
		    !atomic_read(&itf->in_unreg_step2) &&
		    itf->sum_reg_cnt == instance_id) {
			ret = true;
			break;
		}
	}

	return ret;
}

struct dimn_itf_s *get_itf_vfm(struct vframe_s *vfm)
{
	//struct dimn_itf_s *itf;
	struct dimn_dvfm_s *dvfm = NULL;
	struct dim_pvpp_hw_s *hw;

	if (!vfm)
		return NULL;

	if (!(vfm->di_flag & (DI_FLAG_DI_PVPPLINK | DI_FLAG_DI_PSTVPPLINK)))
		return NULL;
	if (!vfm->private_data)
		return NULL;
	if (!instance_id_check(vfm->di_instance_id)) {
		hw = &get_datal()->dvs_pvpp.hw;
		PR_WARN("%s:vfm illegal:0x%px:h[%d],%d\n",
			__func__,
			vfm, hw->reg_nub,
			vfm->di_instance_id);
		return NULL;
	}

	dvfm = (struct dimn_dvfm_s *)vfm->private_data;
	if (!dvfm)
		return NULL;
	if (!dvfm->itf)
		return NULL;
	if (dvfm->itf->sum_reg_cnt != vfm->di_instance_id) {
		PR_ERR("%s:id not map:0x%px:%d:%d\n",
			__func__, vfm,
			vfm->di_instance_id,
			dvfm->itf->sum_reg_cnt);
		return NULL;
	}
	return dvfm->itf;
}

/* */
static void dbg_display_status(struct vframe_s *vfm,
			 struct pvpp_dis_para_in_s *in_para,
			 void *out_para,
			 int ret)
{
	struct dimn_itf_s *itf;
	unsigned int dis_status;
	static unsigned int lst_sts;

	if (!dpvpp_dbg_en_irq())
		return;
	if (!vfm)
		return;
	itf = get_itf_vfm(vfm);
	if (!itf) {
		PR_INF("dbg:sts:no itf\n");
		return;
	}
	dis_status = atomic_read(&itf->c.dbg_display_sts);
	if (lst_sts != dis_status) {
		PR_INF("dbg:sts:0x%08x -> 0x%08x\n", lst_sts, dis_status);
		lst_sts = dis_status;
	}
}

void dbg_check_ud(struct dim_pvpp_ds_s *ds, unsigned int dbgid)
{
#ifdef DBG_CHECK_VF
	int i;
	struct vframe_s *vf;
	void *p;
	ud tmp, tmp2;
	struct dimn_itf_s *itf;

	if (!ds || !ds->en_dbg_check_ud)
		return;
	itf = ds->itf;
	for (i = 0; i < DIM_LKIN_NUB; i++) {
		vf = dpvpp_get_vf_base(itf) + i;
		tmp = (ud)vf->private_data;
		p = &ds->lk_in_bf[i];
		tmp2 = (ud)p;
		if (tmp != tmp2) {
			PR_ERR("%s:%d:ud[%d]:0x%lx -> 0x%lx\n",
				__func__,
				dbgid,
				i,
				(unsigned long)tmp2,
				(unsigned long)tmp);
		}
	}
#endif
}

void dbg_check_vf(struct dim_pvpp_ds_s *ds,
	struct vframe_s *vf, unsigned int dbgid)
{
#ifdef DBG_CHECK_VF
	int i;
	ud tmp;
	bool ret = false;
	struct vframe_s *vf_l;
	struct dimn_itf_s *itf;

	if (!ds || !ds->en_dbg_check_vf)
		return;
	itf = ds->itf;
	for (i = 0; i < DIM_LKIN_NUB; i++) {
		tmp = (ud)vf;
		vf_l = dpvpp_get_vf_base(itf) + i;
		if (tmp == (ud)vf_l) {
			ret = true;
			break;
		}
	}
	if (!ret)
		PR_ERR("%s:%d:0x%px\n", __func__, dbgid, vf);
#endif
}

int dpvpp_show_ds(struct seq_file *s, struct dim_pvpp_ds_s *ds)
{
	char *spltb = "---------------------------";

	if (!ds) {
		seq_printf(s, "%s,no ds\n", __func__);
		return 0;
	}
	seq_printf(s, "%s\n", spltb); /*----*/
	seq_printf(s, "%s\n", "ds-sum");
	seq_printf(s, "\tque:%d:%d:%d:%d:%d\n",
		   ds->sum_que_idle,
		   ds->sum_que_in,
		   ds->sum_que_ready,
		   ds->sum_que_recycle,
		   ds->sum_que_kback);
	seq_printf(s, "\tdct:%d:%d:\n",
		   ds->dct_sum_in,
		   ds->dct_sum_o);
	seq_printf(s, "%s\n", spltb); /*----*/
	return 0;
}

/************************************************
 * dump
 ************************************************/
int dpvpp_show_itf(struct seq_file *s, struct dimn_itf_s *itf)
{
	char *spltb = "---------------------------";
	int i;

	if (!itf) {
		seq_printf(s, "%s,no itf\n", __func__);
		return 0;
	}

	seq_printf(s, "%s\n", spltb); /*----*/
	seq_printf(s, "%s\n", "itf");
	seq_printf(s, "0x%x:id[%d]:ch[%d]\n", itf->ch, itf->id, itf->bind_ch);
	seq_printf(s, "%s:%d:%d\n", "reg", atomic_read(&itf->reg), itf->c.reg_di);
	seq_printf(s, "%s:%d:%d\n", "in_unreg", atomic_read(&itf->in_unreg),
		atomic_read(&itf->in_unreg_step2));
	seq_printf(s, "%s:%d\n", "etype:1:vfm", itf->etype);
	seq_printf(s, "%s:%d\n", "link_mode", itf->link_mode);
	seq_printf(s, "%s:%s\n", "name", itf->dvfm.name);
	seq_printf(s, "%s:0x%x:0x%x:0x%x\n", "srcx",
		   itf->src_need,
		   itf->c.src_state,
		   itf->c.src_last);
	seq_printf(s, "%s:%d\n", "bypass_c", itf->bypass_complete);
	seq_printf(s, "%s:%d\n", "bypass_reason", itf->c.bypass_reason);
	seq_printf(s, "%s:%d:%d:%d:%d\n", "pause",
		   itf->c.pause_pst_get,
		   itf->c.pause_pre_get,
		   itf->c.pause_parser,
		   itf->c.flg_block);
	seq_printf(s, "%s:\n", "sum");
	seq_printf(s, "\t%s:%d\n", "pre_get", itf->c.sum_pre_get);
	seq_printf(s, "\t%s:%d\n", "pre_put", itf->c.sum_pre_put);
	seq_printf(s, "\t%s:%d\n", "pst_get", itf->c.sum_pst_get);
	seq_printf(s, "\t%s:%d\n", "pst_put", itf->c.sum_pst_put);
	seq_printf(s, "\t%s:%d\n", "reg_cnt", itf->sum_reg_cnt);

	/* show vf_bf */
	seq_printf(s, "%s\n", spltb); /*----*/
	seq_printf(s, "%s:\n", "bf_vf_addr");
	for (i = 0; i < DIM_LKIN_NUB; i++)
		seq_printf(s, "\t%d:0x%px\n", i, &itf->vf_bf[i]);

	/* show buf_bf */
	seq_printf(s, "%s\n", spltb); /*----*/
	seq_printf(s, "%s:\n", "bf_buf_addr");
	for (i = 0; i < DIM_LKIN_NUB; i++)
		seq_printf(s, "\t%d:0x%px\n", i, &itf->buf_bf[i]);
	seq_printf(s, "%s\n", spltb); /*----*/
	if (itf->ext_vfm)
		seq_printf(s, "%s:\n", "ext_vfm");
	else
		seq_printf(s, "%s:\n", "ext_vfm null");
	if (itf->ext_ins)
		seq_printf(s, "%s:\n", "ext_ins");
	else
		seq_printf(s, "%s:\n", "ext_ins null");

	if (itf->ops_vfm)
		seq_printf(s, "%s:\n", "ops_vfm");
	else
		seq_printf(s, "%s:\n", "ops_vfm null");

	return 0;
}

int dpvpp_show_que(struct seq_file *s, struct dimn_qs_cls_s *que, unsigned char level)
{
	char *spltb = "---------------------------";
	ud rtab[MAX_FIFO_SIZE_PVPP];
	unsigned int rsize;
	int i;

	/*******************
	 * level:
	 *	0: only number
	 *	1: base infor
	 *	2: list
	 *******************/
	seq_printf(s, "%s\n", spltb);
	if (!que) {
		seq_printf(s, "%s,no que\n", __func__);
		return 0;
	}
	seq_printf(s, "%s:%d\n", que->cfg.name, level);
	seq_printf(s, "%s:%d\n", "cnt", que->ops.count(que));

	if (level < 1)
		return 0;
	/* level 1 */
	seq_printf(s, "%s:%d\n", "flg", que->flg);
	seq_printf(s, "%s:0x%x\n", "flg_lock", que->cfg.flg_lock);
	seq_printf(s, "%s:0x%x\n", "ele_nub", que->cfg.ele_nub);
	seq_printf(s, "%s:0x%x\n", "ele_size", que->cfg.ele_size);
	seq_printf(s, "%s:0x%x\n", "ele_mod", que->cfg.ele_mod);
	seq_printf(s, "%s:0x%x\n", "qsize", que->size);

	if (level < 2)
		return 0;
	rsize = que->ops.list(que, &rtab[0]);
	if (!rsize) {
		seq_printf(s, "%s:\n", "no list");
		return 0;
	}
	seq_printf(s, "%s:%d\n", "list:", rsize);
	for (i  = 0; i < rsize; i++)
		seq_printf(s, "\t%d:0x%px\n", i, (void *)rtab[i]);

	return 0;
}

int dpvpp_show_ndvfm(struct seq_file *s, struct dimn_dvfm_s *ndvfm)
{
	char *spltb = "---------------------------";
	char *splta = "--------------";

	seq_printf(s, "%s\n", spltb);
	if (!ndvfm) {
		seq_printf(s, "%s,no ndvfm\n", __func__);
		return 0;
	}

	dbg_hd(s, &ndvfm->header);
	seq_printf(s, "%s:0x%px\n", "addr", ndvfm);
	if (ndvfm->itf)
		seq_printf(s, "%s:[%d]\n", "itf id", ndvfm->itf->id);

	seq_printf(s, "%s\n", splta);
	seq_printf(s, "%s:%d\n", "etype:1:vfm", ndvfm->etype);
	seq_printf(s, "%s:0x%px\n", "pvf", ndvfm->pvf);
	seq_printf(s, "%s:0x%px\n", "ori_in", ndvfm->c.ori_in);
	seq_printf(s, "%s:%d\n", "cnt_in", ndvfm->c.cnt_in);
	seq_printf(s, "%s:%d,%d\n", "src", ndvfm->c.src_w, ndvfm->c.src_h);
	seq_printf(s, "%s:%d:%d:%d:%d:%d\n", "wr_done",
		   atomic_read(&ndvfm->c.wr_done),
		   atomic_read(&ndvfm->c.flg_vpp),
		   atomic_read(&ndvfm->c.flg_get),
		   atomic_read(&ndvfm->c.flg_ref),
		   atomic_read(&ndvfm->c.flg_show));
	seq_printf(s, "%s:%d:0x%px\n", "mem_cnt",
		   ndvfm->c.mem_cnt,
		   ndvfm->c.mem_link);
	seq_printf(s, "in_dvfm:%s\n", splta);
	seq_file_dvfm(s, NULL, &ndvfm->c.in_dvfm);
	seq_printf(s, "out_dvfm:%s\n", splta);
	seq_file_dvfm(s, NULL, &ndvfm->c.out_dvfm);
	seq_printf(s, "%s\n", spltb);
	return 0;
}

int dpvpp_itf_show(struct seq_file *s, void *what)
{
	struct dimn_itf_s *itf;
	int *chp;
	unsigned int ch;
	struct di_ch_s *pch;

	chp = (int *)s->private;
	ch = *chp;
	pch = get_chdata(ch);
	if (pch->itf.p_vpp_link && pch->itf.p_itf) {
		itf = pch->itf.p_itf;
	} else {
		seq_printf(s, "%s:ch[%d] not pre-vpp link\n", __func__, ch);
		return 0;
	}

	seq_printf(s, "%s:ch[%d]\n", __func__, *chp);

	dpvpp_show_itf(s, itf);
	dpvpp_show_ds(s, itf->ds);
	return 0;
}

static void VS_REG_WR(unsigned int addr, unsigned int val)
{
	VSYNC_WR_TABLE_REG(VIDEO_PARTITION_TABLE, addr, val);
}

/* dim_VSYNC_WR_MPEG_REG_BITS */

static unsigned int VS_REG_WRB(unsigned int addr, unsigned int val,
			       unsigned int start, unsigned int len)
{
	VSYNC_WR_TABLE_REG_BITS(VIDEO_PARTITION_TABLE, addr, val, start, len);

	return 0;
}

static unsigned int VS_REG_RD(unsigned int addr)
{
	return VSYNC_RD_TABLE_REG(VIDEO_PARTITION_TABLE, addr);
}

static unsigned int get_reg_bits(unsigned int val, unsigned int bstart,
				 unsigned int bw)
{
	return((val &
	       (((1L << bw) - 1) << bstart)) >> (bstart));
}

static unsigned int VS_REG_RDB(unsigned int adr, unsigned int start,
			    unsigned int len)
{
	unsigned int val;

	val = VS_REG_RD(adr);

	return get_reg_bits(val, start, len);
}

static const struct reg_acc dpvpp_regset = {
	.wr = VS_REG_WR,
	.rd = VS_REG_RD,
	.bwr = VS_REG_WRB,
	.brd = VS_REG_RDB,
};

struct vframe_s *dpvpp_get_vf_base(struct dimn_itf_s *itf)
{
	return &itf->vf_bf[0];
}

struct vframe_s *in_vf_get(struct dimn_itf_s *itf)
{
#ifdef CONFIG_AMLOGIC_MEDIA_VFM
	struct vframe_s *vf;

	itf->c.sum_pre_get++;
	vf = vf_get(itf->dvfm.name);
	if (vf) {
		vf->di_flag = 0;
		vf->di_flag |= DI_FLAG_DI_GET;
	}

	return vf;
#else
	return NULL;
#endif
}

struct vframe_s *in_vf_peek(struct dimn_itf_s *itf)
{
#ifdef CONFIG_AMLOGIC_MEDIA_VFM
	return vf_peek(itf->dvfm.name);
#else
	return NULL;
#endif
}

void in_vf_put(struct dimn_itf_s *itf, struct vframe_s *vf)
{
#ifdef CONFIG_AMLOGIC_MEDIA_VFM
	itf->c.sum_pre_put++;
	if (vf)
		vf->di_flag &= ~DI_FLAG_DI_GET;
	vf_put(vf, itf->dvfm.name);
#endif
}

void in_buffer_put(struct dimn_itf_s *itf, struct di_buffer *buffer)
{
	if (!itf || itf->etype != EDIM_NIN_TYPE_INS ||
		!buffer) {
		PR_ERR("%s:not ins\n", __func__);
		return;
	}

	if (itf->nitf_parm.ops.empty_input_done) {
		itf->c.sum_pre_put++;
		itf->nitf_parm.ops.empty_input_done(buffer);
//#if defined(DBG_QUE_IN_OUT) || defined(DBG_QUE_INTERFACE)
		dbg_plink2("ins:empty_done:0x%px,0x%px\n", buffer, buffer->vf);
		dbg_link("B:vf %x,canvas0 addr:%lx\n",
					buffer->vf->type,
					buffer->vf->canvas0_config[0].phy_addr);
//#endif
	}
}

//vf_notify_receiver
//vf_light_unreg_provider(prov);

/*--------------------------*/
static struct vframe_s *dimn_vf_peek(void *arg)
{
	struct dimn_itf_s *itf;
	struct vframe_s *vfm = NULL;

	if (!arg) {
		PR_ERR("%s:no arg\n", __func__);
		return NULL;
	} else {
		itf = (struct dimn_itf_s *)arg;
	}
	if (!atomic_read(&itf->reg) || atomic_read(&itf->in_unreg)) {
		PR_ERR("%s:unreg?(%d:%d)\n",
			__func__,
			atomic_read(&itf->reg),
			atomic_read(&itf->in_unreg));
		return NULL;
	}

	if (itf->c.flg_block || itf->c.pause_pst_get)
		return NULL;

	if (itf->bypass_complete) {
		vfm = in_vf_peek(itf);
		if (itf->ops_vfm &&
		    itf->ops_vfm->vf_bypass_check_chg &&
		    itf->ops_vfm->vf_bypass_check_chg(itf, vfm))
			return NULL;
		return vfm;
	}
	if (itf->ops_vfm && itf->ops_vfm->vf_peek)
		vfm = itf->ops_vfm->vf_peek(itf);
	return vfm;
}

/* dpvpp_vf_ops(mode)->get */
static struct vframe_s *dimn_vf_get(void *arg)
{
	struct dimn_itf_s *itf;
	struct vframe_s *vfm = NULL;

	if (!arg) {
		PR_ERR("%s:no arg\n", __func__);
		return NULL;
	} else {
		itf = (struct dimn_itf_s *)arg;
	}
	if (!atomic_read(&itf->reg) || atomic_read(&itf->in_unreg)) {
		PR_ERR("%s:unreg?(%d:%d)\n",
			__func__,
			atomic_read(&itf->reg),
			atomic_read(&itf->in_unreg));
		return NULL;
	}
	if (itf->c.flg_block || itf->c.pause_pst_get)
		return NULL;
	itf->c.sum_pst_get++;

	if (itf->bypass_complete) {
		vfm = in_vf_peek(itf);
		if (itf->ops_vfm &&
		    itf->ops_vfm->vf_bypass_check_chg &&
		    itf->ops_vfm->vf_bypass_check_chg(itf, vfm))
			return NULL;
		vfm = in_vf_get(itf);
		vfm->di_flag |= DI_FLAG_DI_BYPASS;
		return vfm;
	}
	if (itf->ops_vfm && itf->ops_vfm->vf_get)
		vfm = itf->ops_vfm->vf_get(itf);
	return vfm;
}

/* dpvpp_vf_ops(mode)->put */
static void dimn_vf_put(struct vframe_s *vf, void *arg)
{
	struct dimn_itf_s *itf;

	if (!arg) {
		PR_ERR("%s:no arg\n", __func__);
		return;
	} else {
		itf = (struct dimn_itf_s *)arg;
	}
	if (!atomic_read(&itf->reg) || atomic_read(&itf->in_unreg)) {
		PR_ERR("%s:unreg?(%d:%d)\n",
			__func__,
			atomic_read(&itf->reg),
			atomic_read(&itf->in_unreg));
		return;
	}

	itf->c.sum_pst_put++;

	if (vf->di_flag & DI_FLAG_DI_BYPASS) {
		vf->di_flag &= (~DI_FLAG_DI_BYPASS);
		in_vf_put(itf, vf);
		vf_notify_provider(itf->dvfm.name,
				   VFRAME_EVENT_RECEIVER_PUT, NULL);
		return;
	}
	if (itf->ops_vfm && itf->ops_vfm->vf_put)
		itf->ops_vfm->vf_put(vf, itf);
}

static int dimn_event_cb(int type, void *data, void *private_data)
{
	if (type == VFRAME_EVENT_RECEIVER_FORCE_UNREG) {
		PR_INF("%s: _FORCE_UNREG return\n",
			__func__);
		return 0;
	}
	return 0;
}

/* dpvpp_vf_ops(mode)->vf_states */
static int dimn_vf_states(struct vframe_states *states, void *arg)
{
	struct dimn_itf_s	*itf;

	if (!states)
		return -1;
	if (!arg) {
		PR_ERR("%s:no arg\n", __func__);
		return -1;
	} else {
		itf = (struct dimn_itf_s *)arg;
	}
	if (itf->ops_vfm && itf->ops_vfm->et_states)
		itf->ops_vfm->et_states(itf, states);

	return 0;
}

//dpvpp_vf_ops(mode)
static const struct vframe_operations_s dimn_vf_provider = {
	.peek		= dimn_vf_peek,
	.get		= dimn_vf_get,
	.put		= dimn_vf_put,
	.event_cb	= dimn_event_cb,
	.vf_states	= dimn_vf_states,
};

void dvframe_exit(struct dimn_itf_s *itf)
{
	struct dev_vfram_t *pvfm;

	pvfm = &itf->dvfm;
	vf_unreg_provider(&pvfm->di_vf_prov);
	vf_unreg_receiver(&pvfm->di_vf_recv);

	PR_INF("%s finish:%s\n", __func__, pvfm->name);
}

/************************************************
 * que for point
 ************************************************/
#ifdef DBG_QUE_LOCAL
static void qqs_err_add(struct qs_err_log_s *plog, struct qs_err_msg_s *msg)
{
	if (!plog || !msg ||
	    plog->pos >= QS_ERR_LOG_SIZE)
		return;
	plog->msg[plog->pos] = *msg;
	plog->pos++;
}
#endif
static bool ffp_reset(struct dimn_qs_cls_s *p)
{
	kfifo_reset(&p->fifo);
	return true;
}

static bool ffp_put(struct dimn_qs_cls_s *p, void *ubuf)
{
#ifdef DBG_QUE_LOCAL
	struct qs_err_msg_s msg;
#endif
	ud buf_add;
	unsigned int ret;
#ifdef DBG_QUE_IN_OUT
	struct vframe_s *vfm;
	struct dimn_dvfm_s *ndvfm;
	bool is_act = false;
#endif

	buf_add = (ud)ubuf;
	if (p->cfg.flg_lock & DIM_QUE_LOCK_WR) {
		ret = kfifo_in_spinlocked(&p->fifo,
					  &buf_add,
					  p->cfg.ele_size,
					  &p->lock_wr);
	} else {
		ret = kfifo_in(&p->fifo, &buf_add, p->cfg.ele_size);
	}

	if (ret	!= p->cfg.ele_size) {
#ifdef DBG_QUE_LOCAL
		msg.func_id	= QS_FUNC_F_IN;
		msg.err_id	= QS_ERR_FIFO_IN;
		msg.qname	= p->cfg.name;
		msg.index1	= buf_add;
		msg.index2	= 0;
		qqs_err_add(p->plog, &msg);
#endif
		PR_ERR("%s:%s:0x%px\n", __func__, p->cfg.name, ubuf);
		return false;
	}
#ifdef DBG_QUE_IN_OUT
	vfm = (struct vframe_s *)ubuf;
	if (vfm->private_data) {
		ndvfm = (struct dimn_dvfm_s *)vfm->private_data;
		if (ndvfm->header.code == CODE_INS_LBF)
			is_act = true;
	}
	if (is_act)
		dbg_plink3("%s:put:%d ndvfm:0x%px\n",
		       p->cfg.name, ndvfm->header.index, ubuf);
	else
		dbg_plink3("%s:put:0x%px?\n", p->cfg.name, ubuf);
#endif
	return true;
}

static void *ffp_get(struct dimn_qs_cls_s *p)
{
#ifdef DBG_QUE_LOCAL
	struct qs_err_msg_s msg;
#endif
#ifdef DBG_QUE_IN_OUT
		struct vframe_s *vfm;
		struct dimn_dvfm_s *ndvfm;
		bool is_act = false;
#endif
	ud buf_add;

	unsigned int ret;

	if (p->cfg.flg_lock & DIM_QUE_LOCK_RD) {
		ret = kfifo_out_spinlocked(&p->fifo,
					   &buf_add,
					   p->cfg.ele_size,
					   &p->lock_rd);
	} else {
		ret = kfifo_out(&p->fifo, &buf_add, p->cfg.ele_size);
	}

	if (ret != p->cfg.ele_size) {
#ifdef DBG_QUE_LOCAL
		msg.func_id	= QS_FUNC_F_O;
		msg.err_id	= QS_ERR_FIFO_O;
		msg.qname	= p->cfg.name;
		msg.index1	= 0;
		msg.index2	= 0;
		qqs_err_add(p->plog, &msg);
#endif
		PR_ERR("%s:%s\n",
			 __func__, p->cfg.name);
		return NULL;
	}
#ifdef DBG_QUE_IN_OUT
	vfm = (struct vframe_s *)buf_add;
	if (vfm->private_data) {
		ndvfm = (struct dimn_dvfm_s *)vfm->private_data;
		if (ndvfm->header.code == CODE_INS_LBF)
			is_act = true;
	}
	if (is_act)
		dbg_plink3("%s:get:%d ndvfm:0x%px\n",
		       p->cfg.name, ndvfm->header.index, buf_add);
	else
		dbg_plink3("%s:get:?:0x%px\n", p->cfg.name, buf_add);
#endif

	return (void *)buf_add;
}

static unsigned int ffp_in(struct dimn_qs_cls_s *p,
			   void *ubuf, unsigned int cnt)
{
#ifdef DBG_QUE_LOCAL
	struct qs_err_msg_s msg;
	ud buf_add;
#endif
#ifdef DBG_QUE_IN_OUT
	ud *addr;
	ud buf_add;
#endif
	unsigned int ret, rcnt;

	//buf_add = (ud)ubuf;
	if (p->cfg.flg_lock & DIM_QUE_LOCK_WR) {
		ret = kfifo_in_spinlocked(&p->fifo,
					  ubuf,
					  cnt << p->cfg.ele_mod,
					  &p->lock_wr);
	} else {
		ret = kfifo_in(&p->fifo, ubuf, cnt << p->cfg.ele_mod);
	}
	rcnt = ret >> p->cfg.ele_mod;

	if (rcnt != cnt) {
#ifdef DBG_QUE_LOCAL
		msg.func_id	= QS_FUNC_F_IN;
		msg.err_id	= QS_ERR_FIFO_IN;
		msg.qname	= p->cfg.name;
		msg.index1	= buf_add;
		msg.index2	= 0;
		qqs_err_add(p->plog, &msg);
#endif
		PR_ERR("%s:%s:0x%px\n", __func__, p->cfg.name, ubuf);
		return rcnt;
	}
#ifdef DBG_QUE_IN_OUT
	addr = (ud *)ubuf;

	buf_add = *(addr + 0);

	dbg_plink3("%s:in:%d:0x%px,cnt[%d]\n", p->cfg.name, rcnt, buf_add);
#endif
	return rcnt;
}

static unsigned int ffp_out(struct dimn_qs_cls_s *p, void *buf, unsigned int cnt)
{
#ifdef DBG_QUE_LOCAL
	struct qs_err_msg_s msg;
#endif
#ifdef DBG_QUE_IN_OUT
		ud buf_add, *pud;
#endif
	unsigned int ret, rcnt;

	if (p->cfg.flg_lock & DIM_QUE_LOCK_RD) {
		ret = kfifo_out_spinlocked(&p->fifo,
					   buf,
					   cnt << p->cfg.ele_mod,
					   &p->lock_rd);
	} else {
		ret = kfifo_out(&p->fifo, buf, cnt << p->cfg.ele_mod);
	}
	rcnt = ret >> p->cfg.ele_mod;

	if (rcnt != cnt) {
#ifdef DBG_QUE_LOCAL
		msg.func_id	= QS_FUNC_F_O;
		msg.err_id	= QS_ERR_FIFO_O;
		msg.qname	= p->cfg.name;
		msg.index1	= 0;
		msg.index2	= 0;
		qqs_err_add(p->plog, &msg);
#endif
		PR_ERR("%s:%s\n",
			 __func__, p->cfg.name);
		return rcnt;
	}
#ifdef DBG_QUE_IN_OUT
	pud = (ud *)buf;

	buf_add = *(pud + 0);

	dbg_plink3("%s:out:%d:0x%px:cnt[%d]\n", p->cfg.name, rcnt, buf_add, rcnt);
#endif

	return rcnt;
}

static void *ffp_peek(struct dimn_qs_cls_s *p)
{
	ud index;
#ifdef DBG_QUE_LOCAL
	struct qs_err_msg_s msg;
#endif
	if (kfifo_is_empty(&p->fifo))
		return NULL;

	if (kfifo_out_peek(&p->fifo, &index, p->cfg.ele_size) !=
	    p->cfg.ele_size) {
#ifdef DBG_QUE_LOCAL
		msg.func_id	= QS_FUNC_F_PEEK;
		msg.err_id	= QS_ERR_FIFO_PEEK;
		msg.qname	= p->cfg.name;
		msg.index1	= 0;
		msg.index2	= 0;
		qqs_err_add(p->plog, &msg);
#endif
		PR_ERR("%s:\n", __func__);
		return NULL;
	}

	return (void *)index;
}

static bool ffp_is_full(struct dimn_qs_cls_s *p)
{
	return kfifo_is_full(&p->fifo);
}

static unsigned int ffp_count(struct dimn_qs_cls_s *p)
{
	unsigned int length;

	length = kfifo_len(&p->fifo);
	length = length >> p->cfg.ele_mod;
	return length;
}

static bool ffp_empty(struct dimn_qs_cls_s *p)
{
	//return kfifo_is_empty(&p->fifo);
	if (!ffp_count(p))
		return true;
	return false;
}

static unsigned int ffp_avail(struct dimn_qs_cls_s *p)
{
	unsigned int length;

	length = kfifo_avail(&p->fifo);
	length = length >> p->cfg.ele_mod;
	return length;
}

static unsigned int ffp_list(struct dimn_qs_cls_s *p, ud *rtab)
{
	unsigned int cnt = 0;

	cnt = kfifo_out_peek(&p->fifo, rtab, p->size);
	cnt = cnt >> p->cfg.ele_mod;
	return cnt;
}

static const struct dimn_qsp_ops_s dimn_pque_ops = {
	.put	= ffp_put,
	.get	= ffp_get,
	.in	= ffp_in,
	.out	= ffp_out,
	.peek	= ffp_peek,
	.is_empty = ffp_empty,
	.is_full	= ffp_is_full,
	.count	= ffp_count,
	.avail	= ffp_avail,
	.reset	= ffp_reset,
	.list	= ffp_list,
};

/* copy from qfp_int */
void dimn_que_int(struct dimn_qs_cls_s	*pq,
		  struct dimn_qs_cfg_s *cfg) /*  MAX_FIFO_SIZE_PVPP */
{
	int ret;

	if (!pq)
		return;

	/*creat que*/
	memset(pq, 0, sizeof(*pq));
	memcpy(&pq->cfg, cfg, sizeof(pq->cfg));

	pq->flg	= true;

	ret = kfifo_alloc(&pq->fifo,
			  cfg->ele_nub << cfg->ele_mod,
			  GFP_KERNEL);
	if (ret < 0) {
		PR_ERR("%s:alloc kfifo err:%s\n", __func__, cfg->name);
		pq->flg = false;
		return;//no resource
	}

	pq->size = kfifo_size(&pq->fifo);

	memcpy(&pq->ops, &dimn_pque_ops, sizeof(pq->ops));

	/*reset*/
	pq->ops.reset(pq);
	/*lock ?*/
	if (cfg->flg_lock & DIM_QUE_LOCK_RD)
		spin_lock_init(&pq->lock_rd);

	if (cfg->flg_lock & DIM_QUE_LOCK_WR)
		spin_lock_init(&pq->lock_wr);

	dbg_reg("%s:%s:end\n", __func__, cfg->name);
}

bool dimn_que_release(struct dimn_qs_cls_s	*pq)
{
	if (!pq)
		return true;
	if (pq->flg)
		kfifo_free(&pq->fifo);

	memset(pq, 0, sizeof(*pq));

	return true;
}

/************************************************
 * hw timer for wake up
 ************************************************/
#define DIM_HW_TIMER_MS		(12)
static void task_send_ready_now(unsigned int reason)
{
	struct dim_dvs_pvpp_s *pvpp;

	pvpp = &get_datal()->dvs_pvpp;

	atomic_set(&pvpp->hr_timer_have, 0);
	atomic_set(&pvpp->wk_need, 0);
	pvpp->ktimer_lst_wk = ktime_get();
	atomic_inc(&pvpp->sum_wk_real_cnt);
	task_send_ready(reason);
}

static enum hrtimer_restart dpvpp_wk_hrtimer_func(struct hrtimer *timer)
{
	task_send_ready_now(35);

	return HRTIMER_NORESTART;
}

void task_send_wk_timer(unsigned int reason)
{
	struct dim_dvs_pvpp_s *pvpp;
	ktime_t ktimer_now;
	s64 kdiff;
	unsigned int mstimer;

	pvpp = &get_datal()->dvs_pvpp;
	atomic_inc(&pvpp->sum_wk_rq);

	if (reason & DIM_WKUP_TAG_CRITICAL) {
		hrtimer_cancel(&pvpp->hrtimer_wk);
		task_send_ready_now(reason);
		return;
	}

	atomic_inc(&pvpp->wk_need);
	if (atomic_read(&pvpp->hr_timer_have))
		return;

	ktimer_now = ktime_get();
	kdiff = ktime_ms_delta(ktimer_now, pvpp->ktimer_lst_wk);
	if (kdiff > (DIM_HW_TIMER_MS - 3)) {
		hrtimer_cancel(&pvpp->hrtimer_wk);
		task_send_ready_now(reason);
		return;
	}

	/* no timer */
	if (kdiff <= 0)
		mstimer = DIM_HW_TIMER_MS;
	else
		mstimer = DIM_HW_TIMER_MS - (unsigned int)kdiff;
	atomic_set(&pvpp->hr_timer_have, 1);
	hrtimer_start(&pvpp->hrtimer_wk,
		      ms_to_ktime(mstimer), HRTIMER_MODE_REL);
}

/************************************************
 * in put
 ************************************************/

void dpvpp_dbg_unreg_log_print(void)
{
#ifdef DBG_UNREG_LOG
	struct dimn_itf_s *itf;
	struct dim_dvs_pvpp_s *pvpp;
	char *spltb = "---------------------------";

	pvpp = &get_datal()->dvs_pvpp;
	itf = &pvpp->itf;
	/* no input */
	if (!itf->c.sum_pre_get)
		return;

	PR_INF("%s\n", spltb); /*-----*/
	PR_INF("%s:\n", "timer");

	PR_INF("%-15s:%d\n", "sum_wk_rq", atomic_read(&pvpp->sum_wk_rq));
	PR_INF("%-15s:%d\n", "sum_wk_real",
	       atomic_read(&pvpp->sum_wk_real_cnt));

	PR_INF("%s\n", spltb); /*-----*/
	PR_INF("%s:%d\n", "etype:1:vfm", itf->etype);
	PR_INF("%s:0x%x:0x%x:0x%x\n", "srcx",
		   itf->src_need,
		   itf->c.src_state,
		   itf->c.src_last);
	PR_INF("%s:%d\n", "bypass_c", itf->bypass_complete);
	PR_INF("%s:%d:%d:%d:%d\n", "pause",
		   itf->c.pause_pst_get,
		   itf->c.pause_pre_get,
		   itf->c.pause_parser,
		   itf->c.flg_block);
	PR_INF("%s:\n", "sum");
	PR_INF("\t%s:%d\n", "pre_get", itf->c.sum_pre_get);
	PR_INF("\t%s:%d\n", "pre_put", itf->c.sum_pre_put);
	PR_INF("\t%s:%d\n", "pst_get", itf->c.sum_pst_get);
	PR_INF("\t%s:%d\n", "pst_put", itf->c.sum_pst_put);
	PR_INF("\t%s:%d\n", "reg_cnt", itf->sum_reg_cnt);
	PR_INF("%s\n", spltb); /*-----*/
#endif
}

static bool dpvpp_unreg_val(struct dimn_itf_s *itf)
{
	struct dim_pvpp_ds_s *ds = NULL;
	struct dim_pvpp_hw_s *hw;
	bool flg_disable = false;
	ulong irq_flag = 0;
	int i;
	struct di_ch_s *pch;
	enum EPVPP_API_MODE link_mode;

	dbg_plink2("%s:enter\n", __func__);
	if (!itf) {
		PR_ERR("%s:no itf\n", __func__);
		return false;
	}

	spin_lock_irqsave(&lock_pvpp, irq_flag);
	hw = &get_datal()->dvs_pvpp.hw;
	itf->flg_freeze = true;
	link_mode = itf->link_mode;
	if (hw && hw->id_c == itf->id &&
	    hw->reg_cnt == itf->sum_reg_cnt)
		flg_disable = true;
	spin_unlock_irqrestore(&lock_pvpp, irq_flag);

	if (hw)
		dbg_plink1("%s:<%d %d %d> <%d %d> <%d> <%px %px>\n",
			__func__,
			hw->reg_cnt, hw->reg_nub,
			itf->sum_reg_cnt,
			hw->id_c, itf->id, link_mode,
			hw->src_blk[0], hw->src_blk_uhd_afbce[0]);

	if (hw && !flg_disable && hw->reg_nub <= 1) {
		if (itf->link_mode == EPVPP_API_MODE_PRE &&
		    (hw->src_blk[0] || hw->src_blk_uhd_afbce[0])) {
			flg_disable = true;
			PR_INF("%s:lst ch? pre:<%d %d> <%d %d> <%px %px>\n", __func__,
				hw->reg_cnt, itf->sum_reg_cnt,
				hw->id_c, itf->id,
				hw->src_blk[0], hw->src_blk_uhd_afbce[0]);
		} else if (itf->link_mode == EPVPP_API_MODE_POST) {
			flg_disable = true;
			PR_INF("%s:lst ch? post:<%d %d> <%d %d>\n", __func__,
				hw->reg_cnt, itf->sum_reg_cnt,
				hw->id_c, itf->id);
		} else {
			PR_WARN("%s:lst ch? no link_mode\n", __func__);
		}
	}
	if (flg_disable && atomic_read(&hw->link_on_bydi)) {
		dpvpp_link_sw_by_di(false);
		ext_vpp_plink_real_sw(false, true, false);
		if (itf->link_mode == EPVPP_API_MODE_PRE)
			dpvpp_reg_prelink_sw(false);
		else if (itf->link_mode == EPVPP_API_MODE_POST)
			dpvpp_reg_postlink_sw(false);
		else
			PR_WARN("%s:no link_mode\n", __func__);
	}
	dbg_plink2("%s:step 1\n", __func__);
	/* TODO: check if dpvpp_unreg_val and dpvpp_destroy_internal function racing */
	spin_lock_irqsave(&lock_pvpp, irq_flag);
	atomic_set(&itf->in_unreg_step2, atomic_read(&itf->in_unreg));
	spin_unlock_irqrestore(&lock_pvpp, irq_flag);
	dpvpp_dbg_unreg_log_print();

	pch = get_chdata(itf->bind_ch);
	dim_polic_unreg(pch);

	//not need set this flg 2022-01-13 set_hw_reg_flg(true);
	//use flg replace dpvpph_unreg_setting();
	itf->en_first_buf = false;
	/* mem */
	if (!itf->ds) {
		PR_WARN("%s:no ds\n", __func__);
		return false;
	}
	ds = itf->ds;
	//dpvpp_mem_release(ds);
	if (itf->c.flg_reg_mem) {
		hw->reg_nub--;
		itf->c.flg_reg_mem = false;
	}
	//dpvpp_sec_release(&hw->dat_p_afbct);
	/**/
	/* release que */
	dimn_que_release(&ds->lk_que_idle);
	dimn_que_release(&ds->lk_que_in);
	dimn_que_release(&ds->lk_que_ready);
	dimn_que_release(&ds->lk_que_kback);
	dimn_que_release(&ds->lk_que_recycle);
	vfree(ds);
	itf->ds = NULL;
	spin_lock_irqsave(&lock_pvpp, irq_flag);
	itf->bind_ch = -1;
	itf->sum_reg_cnt = 1; //
	itf->flg_freeze = false;
	itf->c.m_mode_n = EPVPP_MEM_T_NONE;
	itf->link_mode = EPVPP_API_MODE_NONE;
	memset(&itf->vf_bf[0], 0,
		sizeof(itf->vf_bf));
	memset(&itf->buf_bf[0], 0,
		sizeof(itf->buf_bf));
	spin_unlock_irqrestore(&lock_pvpp, irq_flag);
	for (i = 0; i < DIM_LKIN_NUB; i++) {
		if (itf->buf_bf[i].vf) {
			PR_INF("check:buf:%d:0x%px,0x%px\n", i,
				&itf->buf_bf[i],
				&itf->buf_bf[i].vf);
		}
	}
	for (i = 0; i < DIM_LKIN_NUB; i++) {
		if (itf->vf_bf[i].private_data) {
			PR_INF("check:vf:%d:0x%px,0x%px\n", i,
				&itf->vf_bf[i],
				itf->vf_bf[i].private_data);
		}
	}
	if (link_mode == EPVPP_API_MODE_PRE) {
		get_datal()->dvs_pvpp.en_polling &= ~DI_BIT0;
		get_datal()->pre_vpp_exist = false;
		dbg_plink2("%s:prelink step 2\n", __func__);
		dpvpp_mem_mng_get(2);
	} else if (link_mode == EPVPP_API_MODE_POST) {
		get_datal()->dvs_pvpp.en_polling &= ~DI_BIT1;
		get_datal()->pst_vpp_exist = false;
	}
	atomic_set(&itf->in_unreg_step2, 0);
	dbg_plink2("%s:check:end\n", __func__);
	return true;
}

//static
unsigned char power_of_2(unsigned int a)
{
	unsigned int position = 0, i = 0;

	/* check power error */
	if (!a || (a & (a - 1)) != 0) {
		PR_ERR("not power:0x%x\n", a);
		return 0;
	}

	for (i = a; i != 0; i >>= 1)
		position++;
	position--;

	return position;
}

/* dvpp_m_ops -> reg | dpvpp_ops(mode)->reg */
static bool dpvpp_reg(struct dimn_itf_s *itf,
	enum EPVPP_API_MODE link_mode)
{
	struct dim_pvpp_ds_s *ds = NULL;
	int i;
	struct dimn_qs_cls_s *qidle;
	struct dimn_dvfm_s *ndvfm;
	struct vframe_s *vf;
	struct dim_dvs_pvpp_s *pvpp;
	struct dimn_qs_cfg_s qcfg;
	struct di_buffer *di_buffer;
	unsigned int id;
	struct dim_pvpp_hw_s *hw;
	struct div2_mm_s *mm;
	struct di_ch_s *pch;

	pvpp = &get_datal()->dvs_pvpp;

	if (!itf)
		return false;

	//kfifo_test_print();
	/* clear htimer */
	dbg_plink2("%s:id[%d]\n", __func__, itf->id);
	pvpp->ktimer_lst_wk = ktime_get();
	atomic_set(&pvpp->sum_wk_real_cnt, 0);
	atomic_set(&pvpp->sum_wk_rq, 0);
	dbg_mem("dim_dvpp_set_s:size:0x%zx, etype[%d]\n",
		sizeof(struct dim_dvpp_set_s),
		itf->etype);
	id = itf->id;
	/* clear vf/di_buff*/
	memset(&itf->vf_bf[0], 0,
		sizeof(itf->vf_bf));
	memset(&itf->buf_bf[0], 0,
		sizeof(itf->buf_bf));
	/* clear c */
	memset(&itf->c, 0, sizeof(itf->c));
	itf->c.reg_di = true;
	itf->c.src_state = itf->src_need;
	itf->c.last_ds_ratio = DI_FLAG_DCT_DS_RATIO_MAX;
	/* dbg */
	di_g_plink_dbg()->display_sts = -10;
	di_g_plink_dbg()->flg_check_di_act = -10;
	di_g_plink_dbg()->flg_check_vf = -10;

	//PR_INF("%s:src_state set[0x%x]\n", __func__, itf->c.src_state);
	/* alloc */
	if (itf->ds) {
		PR_ERR("%s:have ds?\n", __func__);
		/* @ary 11-08 need check release only or unreg? */
		dpvpp_unreg_val(itf);
	}
	if (!itf->ds) {
		/* alloc*/
		ds = vmalloc(sizeof(*ds));
		if (!ds) {
			PR_ERR("%s:can't alloc\n", __func__);
			return false;
		}
		itf->c.src_state &= ~EDVPP_SRC_NEED_VAL;
		//PR_INF("%s:src_state[0x%x]\n", __func__, itf->c.src_state);
		itf->ds = ds;
	}
	ds = itf->ds;
	if (!ds) {
		PR_WARN("%s:no ds\n", __func__);
		return false;
	}
	memset(ds, 0, sizeof(*ds));
	ds->id = id;	/* @ary 11-08 add */
	ds->itf = itf;	/* @ary 11-08 add */
	/* que int cfg */
	qcfg.ele_size	= tst_quep_ele;
	qcfg.ele_nub	= MAX_FIFO_SIZE_PVPP;
	qcfg.ele_mod	= power_of_2(qcfg.ele_size);
	qcfg.flg_lock	= 0;
	qcfg.name	= "lk_idle";
	dimn_que_int(&ds->lk_que_idle, &qcfg);
	qcfg.name	= "lk_recycle";
	dimn_que_int(&ds->lk_que_recycle, &qcfg);
	qcfg.name = "lk_in";
	dimn_que_int(&ds->lk_que_in,  &qcfg);
	qcfg.name = "lk_ready";
	qcfg.flg_lock = DIM_QUE_LOCK_RD;
	dimn_que_int(&ds->lk_que_ready, &qcfg);
	qcfg.name = "lk_back";
	qcfg.flg_lock = DIM_QUE_LOCK_WR;
	dimn_que_int(&ds->lk_que_kback, &qcfg);
	/* check define */
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
	dbg_plink1("define check:%s:yes\n", "VSYNC_WR_TABLE_REG");
#else
	PR_ERR("define check:%s:no\n", "VSYNC_WR_TABLE_REG");
#endif
	/* check que if ready */
	if (!ds->lk_que_idle.flg	||
	    !ds->lk_que_in.flg	||
	    !ds->lk_que_ready.flg	||
	    !ds->lk_que_kback.flg)
		goto ERR_DPVPP_REG;

	itf->c.src_state &= ~EDVPP_SRC_NEED_KFIFO;
	//PR_INF("%s:src_state[0x%x]\n", __func__, itf->c.src_state);

	/* data */
	for (i = 0; i < DIM_LKIN_NUB; i++) {
		ndvfm = &ds->lk_in_bf[i];
		ndvfm->header.ch = DIM_PVPP_NUB;
		ndvfm->header.code = CODE_INS_LBF;
		ndvfm->etype = itf->etype;
		ndvfm->header.index = i;
		ndvfm->itf = itf;
		vf = dpvpp_get_vf_base(itf) + i;
		vf->private_data = ndvfm;
		vf->index = i;
		ndvfm->pvf = vf;
	}
	/*itf*/
	if (itf->etype == EDIM_NIN_TYPE_INS) {
		dbg_plink1("%s:0x%px\n", "caller",
			   itf->nitf_parm.caller_data);
		for (i = 0; i < DIM_LKIN_NUB; i++) {
			vf = dpvpp_get_vf_base(itf) + i;
			di_buffer = &itf->buf_bf[i];
			di_buffer->mng.ch = DIM_PVPP_NUB;
			di_buffer->mng.index = i;
			di_buffer->mng.code = CODE_INS_LBF;
			di_buffer->caller_data =
				itf->nitf_parm.caller_data;
			di_buffer->vf = vf;
		}
	}

	for (i = 0; i < DIM_LKIN_NUB; i++) {
		qidle = &ds->lk_que_idle;
		qidle->ops.put(qidle, dpvpp_get_vf_base(itf) + i);
	}
	di_attach_opr_nr(&get_datal()->ops_nr_op);
	itf->bypass_complete = false;
	itf->en_first_buf = true;

	/* cfg top copy */
	di_cfg_cp_ch(&ds->cfg_cp[0]);
	/* cfg temp */
	ds->cma_mode = 4;

	if (link_mode == EPVPP_API_MODE_POST) {
		itf->c.src_state &= ~EDVPP_SRC_NEED_MEM;
		ds->out_mode = EDPST_MODE_422_10BIT_PACK;
		ds->out_afbce_nv2110	= false;
		ds->en_out_sctmem	= false;
		ds->en_hf_buf		= false;
		ds->en_dw		= false;
		ds->en_dbg_off_nr = dim_dbg_is_disable_nr();
		hw = &get_datal()->dvs_pvpp.hw;
		ds->op = hw->op;

		mm	= &ds->mm;
		mm->cfg.di_h		= 1088;
		mm->cfg.di_w		= 1920;
		mm->cfg.num_local	= 0;
		mm->cfg.num_post	= DPVPP_BUF_NUB;
		itf->link_mode = link_mode;
		dbg_plink1("%s:post:id[%d]\n", __func__, itf->id);
		return true;
	}

	if (cfgg(4K))
		ds->en_4k = true;
	else
		ds->en_4k = false;
	ds->out_mode = EDPST_MODE_422_10BIT_PACK;
	pch = get_chdata(itf->bind_ch);
	if (link_mode == EPVPP_API_MODE_PRE && cfgg(EN_PRE_LINK) == 3 &&
		pch && !IS_VDIN_SRC(pch->src_type))
		ds->en_4k_snr = true;
	else
		ds->en_4k_snr = false;

	ds->en_4k_tnr = false; //fix false

	dbg_plink1("snr:%s:[%d][%d]:src[%d]\n", __func__,
			ds->en_4k_snr,
			ds->en_4k_tnr,
			pch->src_type);
/* */
	if (cfgg(POUT_FMT) == 0) {
		ds->o_hd.b.mode = EDPST_MODE_422_10BIT_PACK;
		ds->o_hd.b.out_buf = EPVPP_BUF_CFG_T_HD_F;
		ds->o_hd.b.afbce_fmt = 0;
		ds->o_hd.b.is_sct_mem = 0;
		ds->o_hd.b.is_afbce = 0;
		if (ds->en_4k) {
			ds->o_uhd.d32 = ds->o_hd.d32;
			ds->o_uhd.b.out_buf = EPVPP_BUF_CFG_T_UHD_F;
		}
		ds->en_out_afbce = false;
		ds->m_mode_n_hd = EPVPP_MEM_T_HD_FULL;
		ds->m_mode_n_uhd = EPVPP_MEM_T_UHD_AFBCE;
		dbg_plink1("format 0\n");
#ifdef NOT_SUPPORT_SETTING
	} else if (cfgg(POUT_FMT) == 1 || cfgg(POUT_FMT) == 2) {
		ds->o_hd.b.mode = EDPST_MODE_NV21_8BIT;
		ds->o_hd.b.out_buf = EPVPP_BUF_CFG_T_HD_NV21;
		ds->o_hd.b.afbce_fmt = 0;
		ds->o_hd.b.is_sct_mem = 0;
		ds->o_hd.b.is_afbce = 0;
		if (ds->en_4k) {
			ds->o_uhd.d32 = ds->o_hd.d32;
			ds->o_uhd.b.out_buf = EPVPP_BUF_CFG_T_UHD_NV21;
		}
		ds->en_out_afbce = false;
		ds->m_mode_n_hd = EPVPP_MEM_T_HD_FULL;
		ds->m_mode_n_uhd = EPVPP_MEM_T_UHD_AFBCE;
		PR_WARN("not support\n");
#endif
	} else if (cfgg(POUT_FMT) == 3) {
		ds->o_hd.b.mode = EDPST_MODE_422_10BIT_PACK;
		ds->o_hd.b.out_buf = EPVPP_BUF_CFG_T_UHD_F_AFBCE;
		ds->o_hd.b.afbce_fmt = EAFBC_T_HD_FULL;
		ds->o_hd.b.is_afbce = 1;
		ds->o_hd.b.is_sct_mem = 0;
		if (ds->en_4k) {
			ds->o_uhd.d32 = ds->o_hd.d32;
			ds->o_uhd.b.out_buf = EPVPP_BUF_CFG_T_UHD_F_AFBCE;
		}

		ds->en_out_afbce	= true;
		ds->m_mode_n_hd = EPVPP_MEM_T_UHD_AFBCE;
		ds->m_mode_n_uhd = EPVPP_MEM_T_UHD_AFBCE;
		dbg_plink1("format 3:afbce\n");
	} else {
		//fix: hd used mif, uhd use afbce
		ds->o_hd.b.mode = EDPST_MODE_422_10BIT_PACK;
		ds->o_hd.b.out_buf = EPVPP_BUF_CFG_T_HD_F;
		ds->o_hd.b.afbce_fmt = 0;
		ds->o_hd.b.is_sct_mem = 0;
		ds->o_hd.b.is_afbce = 0;
		if (ds->en_4k) {
			ds->o_uhd.d32 = ds->o_hd.d32;
			ds->o_uhd.b.out_buf = EPVPP_BUF_CFG_T_UHD_F_AFBCE;
			ds->o_uhd.b.is_afbce = 1;
			ds->o_uhd.b.afbce_fmt = EAFBC_T_4K_FULL;
			ds->en_out_afbce = true;
		}
		ds->m_mode_n_hd = EPVPP_MEM_T_HD_FULL;
		ds->m_mode_n_uhd = EPVPP_MEM_T_UHD_AFBCE;
		dbg_plink1("fix uhd is afbce\n");
	}

	ds->out_afbce_nv2110	= false;
	ds->en_out_sctmem	= false;
	ds->en_hf_buf		= false;
	ds->en_dw		= false;
	ds->en_dbg_off_nr = dim_dbg_is_disable_nr();

	hw = &get_datal()->dvs_pvpp.hw;

	ds->op = hw->op;
	//PR_INF("%s:ds->op:0x%px,0x%px\n", __func__, ds->op, ds->op->bwr);

	mm	= &ds->mm;
	if (ds->en_4k) {
		mm->cfg.di_h		= 2160;
		mm->cfg.di_w		= 3840;
	} else {
		mm->cfg.di_h		= 1088;
		mm->cfg.di_w		= 1920;
	}
	mm->cfg.num_local	= 0;
	mm->cfg.num_post	= DPVPP_BUF_NUB;
	dct_pre_plink_reg(NULL);
	dcntr_reg(1);
	itf->link_mode = link_mode;
	dbg_plink1("%s:pre:id[%d]\n", __func__, itf->id);
	return true;

ERR_DPVPP_REG:
	itf->c.src_last = itf->c.src_state;
	if (ds) {
		dimn_que_release(&ds->lk_que_idle);
		dimn_que_release(&ds->lk_que_in);
		dimn_que_release(&ds->lk_que_ready);
		dimn_que_release(&ds->lk_que_kback);
		dimn_que_release(&ds->lk_que_recycle);
		vfree(itf->ds);
		itf->ds = NULL;
		itf->bypass_complete = true;
		itf->c.src_state = itf->src_need;
		PR_ERR("%s:src_state[0x%x]\n", __func__, itf->c.src_state);
	}

	return false;
}

/* from dip_itf_is_ins_exbuf */
//static //not for pre-vpp link, no ext buffer mode
bool dpvpp_itf_is_ins_exbuf(struct dimn_itf_s *itf)
{
	bool ret = false;

	if (itf->etype == EDIM_NIN_TYPE_INS &&
	    itf->tmode == EDIM_TMODE_2_PW_OUT)
		ret = true;
	return ret;
}

int dpvpp_link_sw_api(bool sw)
{
	struct dim_pvpp_hw_s *hw;
	int cur_sts = 0;

	hw = &get_datal()->dvs_pvpp.hw;

	cur_sts = atomic_read(&hw->link_on_byvpp);
	if (sw)
		atomic_set(&hw->link_on_byvpp, 1);
	else
		atomic_set(&hw->link_on_byvpp, 0);

	if (!sw && cur_sts && atomic_read(&hw->link_sts))
		atomic_set(&hw->link_in_busy, 1);

	dim_print("%s:%d\n", __func__, sw);
	return true;
}

int dpvpp_link_sw_by_di(bool sw)
{
	struct dim_pvpp_hw_s *hw;

	hw = &get_datal()->dvs_pvpp.hw;
	if (sw) {
		if (!atomic_read(&hw->link_on_bydi)) {
			atomic_set(&hw->link_on_bydi, 1);
			ext_vpp_plink_state_changed_notify();
			dim_print("%s:%d\n", __func__, sw);
		}
	} else {
		if (atomic_read(&hw->link_on_bydi)) {
			atomic_set(&hw->link_on_bydi, 0);
			dim_print("%s:%d\n", __func__, sw);
		}
	}

	return true;
}

bool dpvpp_que_sum(struct dimn_itf_s *itf)
{
	struct dimn_qs_cls_s *q;
	struct dim_pvpp_ds_s *ds;

	if (!itf->ds)
		return false;
	ds = itf->ds;
	q = &ds->lk_que_idle;
	ds->sum_que_idle = q->ops.count(q);
	q = &ds->lk_que_in;
	ds->sum_que_in = q->ops.count(q);
	q = &ds->lk_que_ready;
	ds->sum_que_ready = q->ops.count(q);
	q = &ds->lk_que_recycle;
	ds->sum_que_recycle = q->ops.count(q);
	q = &ds->lk_que_kback;
	ds->sum_que_kback = q->ops.count(q);
	return true;
}

static const struct dimn_pvpp_ops_s dvpp_pre_m_ops = {
	//.reg = dpvpp_reg,
	//.unreg = dpvpp_unreg_val,
	.post = dpvpp_pre_process,
	//.display = dpvpp_display,
};

static const struct dimn_pvpp_ops_s dvpp_post_m_ops = {
	//.reg = dpvpp_reg,
	//.unreg = dpvpp_unreg_val,
	.post = dpvpp_post_process,
	//.display = dpvpp_display,
};

/* get by other module not mean get by vpp, so need vpp mark */
struct dimn_dvfm_s *dpvpp_check_dvfm_act(struct dimn_itf_s *itf,
		struct vframe_s *vf)
{
	struct dimn_dvfm_s *dvfm = NULL;

	if (!itf || !itf->ds ||
	    !vf || !vf->private_data || vf->di_flag & DI_FLAG_DI_BYPASS)
		return NULL;

	dvfm = (struct dimn_dvfm_s *)vf->private_data;
	if (!dvfm)
		return NULL;

	if (itf->sum_reg_cnt != vf->di_instance_id) {
		PR_ERR("not active:no get:0x%px\n", vf);
		return NULL;
	}
	return dvfm;
}

static int dpvpp_check_dvfm_addr(struct dim_pvpp_ds_s *ds, struct vframe_s *vf)
{
	ud ud_dvfm, ud_check;

	/* check addr */
	if (!ds || !vf || !vf->private_data)
		return -1;
	if (vf->index >= DIM_LKIN_NUB) {
		PR_ERR("vf:no act:overflow:0x%px\n", vf);
		return -2;
	}
	ud_dvfm = (ud)vf->private_data;
	ud_check = (ud)(&ds->lk_in_bf[vf->index]);
	if (ud_dvfm != ud_check)
		return -3;
	return 1;
}

/* enum EDIM_DVPP_DIFF */
unsigned int check_diff(struct dimn_itf_s *itf,
		struct dimn_dvfm_s *dvfm_c,
		struct pvpp_dis_para_in_s *in_para)
{
	struct pvpp_dis_para_in_s *pa_l, *pa_c;
	struct di_win_s *win_l, *win_c;
	struct dsub_vf_s *dvfml, *dvfmc;
	unsigned int state = 0; //EDIM_DVPP_DIFF
	struct dim_pvpp_hw_s *hw;
	struct dim_pvpp_ds_s *ds;

	if (!itf || !itf->ds) {
		PR_ERR("%s:1\n", __func__);
		return EDIM_DVPP_DIFF_NONE;
	}
	hw = &get_datal()->dvs_pvpp.hw;
	ds = itf->ds;

	if (!ds || !dvfm_c || !in_para || !itf)
		return EDIM_DVPP_DIFF_NONE;

	dvfmc = &dvfm_c->c.in_dvfm.vfs;
	/* copy current para */
	pa_c = &hw->dis_c_para;
	hw->id_c = itf->id;

	memcpy(pa_c, in_para, sizeof(*pa_c));
	win_c = &pa_c->win;
	win_c->x_end = win_c->x_st + win_c->x_size - 1;
	win_c->y_end = win_c->y_st + win_c->y_size - 1;
	win_c->orig_w = dvfmc->width;
	win_c->orig_h = dvfmc->height;
	win_c->x_check_sum = ((win_c->x_size & 0xffff) << 16) |
		((win_c->x_st + win_c->x_end) & 0xffff);
	win_c->y_check_sum = ((win_c->y_size & 0xffff) << 16) |
		((win_c->y_st + win_c->y_end) & 0xffff);

	if (dpvpp_dbg_force_same())
		return EDIM_DVPP_DIFF_SAME_FRAME;
	if (!hw->dis_last_dvf) {
		atomic_add(DI_BIT10, &itf->c.dbg_display_sts);	/* dbg only */
		return EDIM_DVPP_DIFF_ALL;
	}
	if (dpvpp_dbg_trig_change_all_one()) {
		atomic_add(DI_BIT11, &itf->c.dbg_display_sts);	/* dbg only */
		return EDIM_DVPP_DIFF_ALL;
	}

	if (hw->id_c != hw->id_l) {
		PR_INF("diff:change id:%d:%d\n", hw->id_l, hw->id_c);
		return EDIM_DVPP_DIFF_ALL;
	}
	if (hw->en_4k_snr != dvfm_c->c.in_dvfm.en_4k_snr) {
		dbg_plink1("snr:diff:%d->%d\n", hw->en_4k_snr,
			dvfm_c->c.in_dvfm.en_4k_snr);
		return EDIM_DVPP_DIFF_ALL;
	}

	pa_l = &hw->dis_last_para;
	win_l = &pa_l->win;
	if (pa_c->dmode != pa_l->dmode		||
	    win_c->x_size != win_l->x_size	||
	    win_c->y_size != win_l->y_size	||
	    win_c->x_st != win_l->x_st	||
	    win_c->y_st != win_l->y_st) {
		atomic_add(DI_BIT12, &itf->c.dbg_display_sts);	/* dbg only */
		PR_INF("diff:size:%d:<%d,%d,%d,%d,%d>,<%d,%d,%d,%d,%d>\n",
			dvfm_c->c.cnt_in,
			pa_l->dmode,
			win_l->x_size,
			win_l->y_size,
			win_l->x_st,
			win_l->y_st,
			pa_c->dmode,
			win_c->x_size,
			win_c->y_size,
			win_c->x_st,
			win_c->y_st);
		return EDIM_DVPP_DIFF_ALL;
	}
//tmp a
	if ((ud)hw->dis_last_dvf == (ud)dvfm_c && !dpvpp_dbg_force_update_all())
		return EDIM_DVPP_DIFF_SAME_FRAME;

	dvfml = &hw->dis_last_dvf->c.in_dvfm.vfs;
	if (dvfml->bitdepth != dvfmc->bitdepth	||
	    dvfml->type != dvfmc->type		||
	    dvfml->width != dvfmc->width	||
	    dvfml->height != dvfmc->height) {
		atomic_add(DI_BIT13, &itf->c.dbg_display_sts);	/* dbg only */
		PR_INF("diff:size2:%d<0x%x,0x%x,%d,%d,%d>,<0x%x,0x%x,%d,%d,%d>\n",
			hw->dis_last_dvf->c.cnt_in,
			dvfml->bitdepth,
			dvfml->type,
			dvfml->width,
			dvfml->height,
			hw->dis_last_dvf->c.cnt_in,
			dvfmc->bitdepth,
			dvfmc->type,
			dvfmc->width,
			dvfmc->height,
			dvfm_c->c.cnt_in);
		return EDIM_DVPP_DIFF_ALL;
	}

	if (dpvpp_dbg_force_update_all()) { /* debug only */
		atomic_add(DI_BIT14, &itf->c.dbg_display_sts);	/* dbg only */
		return EDIM_DVPP_DIFF_ALL;
	}
	if (hw->dis_last_dvf->c.set_cfg.d32 != dvfm_c->c.set_cfg.d32) {
		atomic_add(DI_BIT15, &itf->c.dbg_display_sts);	/* dbg only */
		state = EDIM_DVPP_DIFF_MEM;
	}
	return state | EDIM_DVPP_DIFF_ONLY_ADDR;
}

static int dpvpp_check_vf_api_l(struct vframe_s *vfm)
{
	struct dim_pvpp_ds_s *ds;
	struct dimn_itf_s *itf;
	int check_add;
	int ret;

	itf = get_itf_vfm(vfm);

	if (!itf || !itf->ds)
		return EPVPP_ERROR_DI_NOT_REG;
	ds = itf->ds;

	check_add = dpvpp_check_dvfm_addr(ds, vfm);

	if (check_add < 0) {
		dbg_mem("%s:add:[%d]\n", __func__, check_add);
		return EPVPP_ERROR_VFM_NOT_ACT;
	}

	if (!dpvpp_check_dvfm_act(itf, vfm))
		return EPVPP_ERROR_VFM_NOT_ACT;

	if (vfm->di_flag &  DI_FLAG_DI_PSTVPPLINK)
		ret = EPVPP_DISPLAY_MODE_DI;
	else
		ret = EPVPP_DISPLAY_MODE_NR;
	return ret;
}

static int dpvpp_check_vf_api(struct vframe_s *vfm)
{
	ulong irq_flag = 0;
	int ret;

	spin_lock_irqsave(&lock_pvpp, irq_flag);
	ret = dpvpp_check_vf_api_l(vfm);
	spin_unlock_irqrestore(&lock_pvpp, irq_flag);
	return ret;
}

static int dpvpp_check_pvpp_link_by_di_api(enum EPVPP_API_MODE mode)
{
	struct dim_pvpp_hw_s *hw;
	int ret;

	hw = &get_datal()->dvs_pvpp.hw;
	if (mode == EPVPP_API_MODE_POST)
		ret = EPVPP_DISPLAY_MODE_DI;
	else
		ret = EPVPP_DISPLAY_MODE_NR;

	if (!atomic_read(&hw->link_on_bydi))
		return EPVPP_ERROR_DI_OFF;
	if (atomic_read(&hw->link_in_busy))
		return EPVPP_ERROR_DI_IN_BUSY;

	return ret;
}

static int dpvpp_pre_unreg_bypass_api(void)
{
	int ret;
	ulong irq_flag = 0;

	spin_lock_irqsave(&lock_pvpp, irq_flag);
	ret = dpvpp_pre_unreg_bypass();
	spin_unlock_irqrestore(&lock_pvpp, irq_flag);
	return ret;
}

static int dpvpp_post_unreg_bypass_api(void)
{
	int ret;
	ulong irq_flag = 0;

	spin_lock_irqsave(&lock_pvpp, irq_flag);
	ret = dpvpp_post_unreg_bypass();
	spin_unlock_irqrestore(&lock_pvpp, irq_flag);
	return ret;
}

static int dpvpp_pre_display_api(struct vframe_s *vfm,
			 struct pvpp_dis_para_in_s *in_para,
			 void *out_para)
{
	int ret;
	ulong irq_flag = 0;
	struct vframe_s *in_vfm = vfm;

	spin_lock_irqsave(&lock_pvpp, irq_flag);
	ret = dpvpp_pre_display(in_vfm, in_para, out_para);
	dbg_display_status(in_vfm, in_para, out_para, ret);
	spin_unlock_irqrestore(&lock_pvpp, irq_flag);
	return ret;
}

static int dpvpp_post_display_api(struct vframe_s *vfm,
			 struct pvpp_dis_para_in_s *in_para,
			 void *out_para)
{
	int ret;
	ulong irq_flag = 0;
	struct vframe_s *in_vfm = vfm;

	spin_lock_irqsave(&lock_pvpp, irq_flag);
	ret = dpvpp_post_display(in_vfm, in_para, out_para);
	dbg_display_status(in_vfm, in_para, out_para, ret);
	spin_unlock_irqrestore(&lock_pvpp, irq_flag);
	return ret;
}

static const struct dimn_pvpp_ops_api_s dvpp_pre_api_ops = {
	.display	= dpvpp_pre_display_api,
	.display_unreg_bypass = dpvpp_pre_unreg_bypass_api,
	.vpp_sw		=  dpvpp_link_sw_api,
	.check_vf	= dpvpp_check_vf_api,
	.check_di_act	= dpvpp_check_pvpp_link_by_di_api,
	.get_di_in_win = dpvpp_get_plink_input_win,
};

static const struct dimn_pvpp_ops_api_s dvpp_post_api_ops = {
	.display	= dpvpp_post_display_api,
	.display_unreg_bypass = dpvpp_post_unreg_bypass_api,
	.vpp_sw		=  dpvpp_link_sw_api,
	.check_vf	= dpvpp_check_vf_api,
	.check_di_act	= dpvpp_check_pvpp_link_by_di_api,
	//.get_di_in_win = dpvpp_get_plink_input_win,
};

static void dpvpp_vf_put(struct vframe_s *vf, struct dimn_itf_s *itf)
{
	struct dimn_qs_cls_s *qback;
	struct dimn_dvfm_s *dvfm = NULL;
	struct dim_pvpp_ds_s *ds = NULL;

	if (!itf || !itf->ds || !vf) {
		PR_ERR("%s:\n", __func__);
		return;
	}
	ds = itf->ds;
	dvfm = (struct dimn_dvfm_s *)vf->private_data;
	if (!dvfm) {
		PR_ERR("%s private_data null:\n", __func__);
		return;
	}
	if (dvfm->header.code != CODE_INS_LBF) {
		PR_ERR("%s:%px,code err 0x%x\n",
		       __func__, vf, dvfm->header.code);
		return;
	}

	if (!atomic_dec_and_test(&dvfm->c.flg_get)) {
		PR_ERR("%s:%px,flg_vpp %d\n", __func__, dvfm,
			atomic_read(&dvfm->c.flg_get));
		return;
	}
	qback = &ds->lk_que_kback;
	qback->ops.put(qback, vf);
	task_send_wk_timer(EDIM_WKUP_REASON_BACK_BUF);
}

static struct vframe_s *dpvpp_vf_get(struct dimn_itf_s *itf)
{
	struct dimn_qs_cls_s *qready;
	struct dimn_dvfm_s *dvfm = NULL;
	struct dim_pvpp_ds_s *ds = NULL;
	struct vframe_s *vf = NULL;
#ifdef DBG_OUT_SIZE
	static unsigned int o_x, o_y;
#endif
	if (!itf || !itf->ds) {
		PR_ERR("%s:\n", __func__);
		return NULL;
	}
	ds = itf->ds;
	qready = &ds->lk_que_ready;
	if (!qready->ops.count(qready) || itf->c.pause_pst_get)
		return NULL;
	vf = qready->ops.get(qready);
	if (!vf) {
		PR_ERR("%s:get err?\n", __func__);
		return NULL;
	}
	if (vf->di_flag & DI_FLAG_DI_BYPASS)
		return vf;

	dvfm = (struct dimn_dvfm_s *)vf->private_data;
	if (!dvfm) {
		PR_ERR("%s:no dvfm?\n", __func__);
		return NULL;
	}
	atomic_inc(&dvfm->c.flg_get);
#ifdef DBG_OUT_SIZE
	if (vf->width != o_x ||
	    vf->height != o_y) {
		PR_INF("osize:<%d,%d> <%d,%d>\n",
			o_x, o_y, vf->width, vf->height);
		o_x = vf->width;
		o_y = vf->height;
	}
#endif /* DBG_OUT_SIZE */
	return vf;
}

static struct vframe_s *dpvpp_vf_peek(struct dimn_itf_s *itf)
{
	struct dimn_qs_cls_s *qready;
	struct dim_pvpp_ds_s *ds = NULL;
	struct vframe_s *vf = NULL;

	if (!itf || !itf->ds) {
		PR_ERR("%s:\n", __func__);
		return NULL;
	}
	ds = itf->ds;
	qready = &ds->lk_que_ready;
	if (itf->c.pause_pst_get)
		return NULL;
	vf = (struct vframe_s *)qready->ops.peek(qready);

	return vf;
}

static int dpvpp_et_ready(struct dimn_itf_s *itf)
{
	task_send_wk_timer(EDIM_WKUP_REASON_IN_HAVE);

	return 0;
}

//itf->ops_vfm->reg_api
static const struct dimn_vfml_ops_s dvpp_ops_vfm = {
	.vf_put		= dpvpp_vf_put,
	.vf_get		= dpvpp_vf_get,
	.vf_peek	= dpvpp_vf_peek,
	.et_ready	= dpvpp_et_ready,
	//.vf_m_in	= vf_m_in,
};

bool dpvpp_recycle_in(struct dimn_itf_s *itf, ud buf)
{
	struct vframe_s *vf;
	struct dimn_dvfm_s *ndvfm;
	unsigned int err = 0;
	struct dim_pvpp_ds_s *ds;
	struct dim_pvpp_hw_s *hw;
	struct di_ch_s *pch;
	bool recycle_done = false;

	if (!itf || !itf->ds) {
		PR_ERR("%s:1\n", __func__);
		return false;
	}
	ds = itf->ds;

	vf = (struct vframe_s *)buf;
	dbg_check_vf(ds, vf, 3);
	ndvfm = (struct dimn_dvfm_s *)vf->private_data;
	if (!ndvfm) {
		err = 1;
		goto RECYCLE_IN_F;
	}
	hw = &get_datal()->dvs_pvpp.hw;
	if (hw->dct_ch != 0xff && ndvfm->c.dct_pre) {
		pch = get_chdata(itf->bind_ch);
		if (hw->dct_ch == itf->bind_ch &&
		    pch && pch->dct_pre) {
			if (get_datal()->dct_op->mem_check(pch, ndvfm->c.dct_pre))
				get_datal()->dct_op->mem_put_free(ndvfm->c.dct_pre);
			else
				dbg_plink1("%s:dct_pre not match %px %px\n",
					__func__, ndvfm->c.dct_pre, pch->dct_pre);
		} else {
			PR_WARN("%s:ch or NULL:%d %d:(%d %d) %px %px %px %px(%px) %s\n",
				__func__,
				itf->id, itf->bind_ch,
				hw->dct_ch, hw->dis_ch,
				ndvfm->c.dct_pre, ndvfm, vf,
				pch, pch ? pch->dct_pre : NULL,
				current->comm);
		}
		ndvfm->c.dct_pre = NULL;
		ds->dct_sum_o++;
	}

	if (itf->link_mode == EPVPP_API_MODE_POST &&
	    ndvfm->c.di_buf) {
		recycle_vframe_type_post(ndvfm->c.di_buf, itf->bind_ch);
		recycle_done = true;
	}
	ndvfm->c.di_buf = NULL;
	if (ndvfm->c.ori_in) {
		if (itf->etype == EDIM_NIN_TYPE_VFM)
			in_vf_put(itf, ndvfm->c.ori_in);
		else
			in_buffer_put(itf, ndvfm->c.ori_in);
		dim_print("%s:vf put\n", __func__);
		ndvfm->c.ori_in = NULL;
		recycle_done = true;
	}
RECYCLE_IN_F:
	if (!recycle_done)
		PR_ERR("%s:err[%d]\n", __func__, err);
	return recycle_done;
}

void dpvpp_recycle_back(struct dimn_itf_s *itf)
{
	struct dimn_qs_cls_s *qback, *qidle;
	unsigned int cnt, cnt_out, cnt_in;
	ud buf[DIM_LKIN_NUB];
	int i;
	struct dim_pvpp_ds_s *ds;

	if (!itf || !itf->ds) {
		PR_ERR("%s:NULL handle:%px ch[%d]\n", __func__,
			itf, itf ? itf->bind_ch : -1);
		return;
	}
	ds = itf->ds;
	qback = &ds->lk_que_kback;
	if (qback->ops.is_empty(qback)) {
		dbg_plink3("%s:lk_que_kback empty\n", __func__);
		return;
	}
	qidle	= &ds->lk_que_idle;
	cnt = qback->ops.count(qback);
	cnt_out = qback->ops.out(qback, &buf[0], cnt);
	dbg_check_ud(ds, 3); /* dbg */
	for (i = 0; i < cnt_out; i++)
		dpvpp_recycle_in(itf, buf[i]);

	cnt_in = qidle->ops.in(qidle, &buf[0], cnt_out);

	if (cnt != cnt_out || cnt != cnt_in)
		PR_ERR("%s:err:cnt[%d:%d:%d]\n", __func__, cnt, cnt_out, cnt_in);
	dbg_plink3("%s: END cnt[%d:%d:%d]\n", __func__, cnt, cnt_out, cnt_in);
}

void dpvpp_prob(void)
{
	struct dim_dvs_pvpp_s *pvpp;
	struct dimn_itf_s *itf;
	struct dim_pvpp_hw_s *hw;
	int i;
	int j; //debug only
	bool prelink_support = false;
	bool postlink_support = false;

	hw = &get_datal()->dvs_pvpp.hw;

	pvpp = &get_datal()->dvs_pvpp;

	memset(pvpp, 0, sizeof(*pvpp));

	if (IS_IC_SUPPORT(PRE_VPP_LINK) && cfgg(EN_PRE_LINK))
		prelink_support = true;

	if (IS_IC_SUPPORT(POST_VPP_LINK) && cfgg(EN_POST_LINK))
		postlink_support = true;

	if (!postlink_support && !prelink_support)
		return;

	/* common init */
	pvpp->vf_ops = &dimn_vf_provider;
	for (i = 0; i < DI_PLINK_CN_NUB; i++) {
		itf = &pvpp->itf[i];
		itf->dvfm.name	= DIM_PVPP_NAME;
		itf->ch		= DIM_PVPP_NUB;
		itf->id		= i;
		itf->bypass_complete = true; //tmp
		itf->bind_ch = -1;
		itf->ops_vfm = &dvpp_ops_vfm;
		/* src need */
		itf->src_need = EDVPP_SRC_NEED_VAL	|
				EDVPP_SRC_NEED_KFIFO	|
				EDVPP_SRC_NEED_REG2	|
				//EDVPP_SRC_NEED_SCT_TAB	|
				//EDVPP_SRC_NEED_SCT_TOP	|
				//EDVPP_SRC_NEED_TEST	| /* dbg only */
				EDVPP_SRC_NEED_MEM	|
				EDVPP_SRC_NEED_LINK_ON;
		spin_lock_init(&itf->lock_first_buf);
	}
	//debug only:
	for (i = 0; i < DI_PLINK_CN_NUB; i++) {
		itf = &pvpp->itf[i];
		dbg_plink2("id[%d]:vfm:\n", i);
		for (j = 0; j < DIM_LKIN_NUB; j++)
			dbg_plink2("\t:%d:0x%px\n", j, &itf->vf_bf[j]);
		dbg_plink2("id[%d]:buffer:\n", i);
		for (j = 0; j < DIM_LKIN_NUB; j++)
			dbg_plink2("\t:%d:0x%px\n", j, &itf->buf_bf[j]);
	}
	/* timer */
	hrtimer_init(&pvpp->hrtimer_wk, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	pvpp->hrtimer_wk.function = dpvpp_wk_hrtimer_func;

	hw->op = &dpvpp_regset; //&di_pre_regset; //
	hw->op_n = &di_pre_regset;
	if (cfgg(LINEAR))
		hw->en_linear = true;
	else
		hw->en_linear = false;
	/*cvs*/
	if (!hw->en_linear) {
		hw->cvs[0][0]	= cvs_nub_get(EDI_CVSI_PVPP_A_IN1, "in1_y");
		hw->cvs[0][1]	= cvs_nub_get(EDI_CVSI_PVPP_A_IN1_C, "in1_c");

		hw->cvs[0][2]	= cvs_nub_get(EDI_CVSI_PVPP_A_NR, "nr_y");
		hw->cvs[0][3]	= cvs_nub_get(EDI_CVSI_PVPP_A_NR_C, "nr_c");

		hw->cvs[0][4]	= cvs_nub_get(EDI_CVSI_PVPP_A_MEM, "mem_y");
		hw->cvs[0][5]	= cvs_nub_get(EDI_CVSI_PVPP_A_MEM_C, "mem_c");

		hw->cvs[1][0]	= cvs_nub_get(EDI_CVSI_PVPP_B_IN1, "in1_y");
		hw->cvs[1][1]	= cvs_nub_get(EDI_CVSI_PVPP_B_IN1_C, "in1_c");

		hw->cvs[1][2]	= cvs_nub_get(EDI_CVSI_PVPP_B_NR, "nr_y");
		hw->cvs[1][3]	= cvs_nub_get(EDI_CVSI_PVPP_B_NR_C, "nr_c");

		hw->cvs[1][4]	= cvs_nub_get(EDI_CVSI_PVPP_B_MEM, "mem_y");
		hw->cvs[1][5]	= cvs_nub_get(EDI_CVSI_PVPP_B_MEM_C, "mem_c");
	}
	if (IS_IC_SUPPORT(DECONTOUR) && cfgg(DCT))
		hw->en_dct = true;
	else
		hw->en_dct = false;
	hw->dct_ch = 0xff;
	hw->dis_ch = 0xff;
	hw->dct_delay_cnt = 0;
	hw->cur_dct_delay = 0;
	hw->last_dct_bypass = true;
	atomic_set(&hw->link_instance_id, DIM_PLINK_INSTANCE_ID_INVALID);

	/* prelink function init */
	if (prelink_support) {
		dd_probe();
		pvpp->allowed |= DI_BIT0;
		pvpp->insert |= DI_BIT0;
		pvpp->pre_ops = &dvpp_pre_m_ops;
		pvpp->pre_ops_api = &dvpp_pre_api_ops;
		buf_cfg_prob();
	}

	/* postlink function init */
	if (postlink_support) {
		pvpp->allowed |= DI_BIT1;
		pvpp->insert |= DI_BIT1;
		pvpp->pst_ops = &dvpp_post_m_ops;
		pvpp->pst_ops_api = &dvpp_post_api_ops;
	}

	dbg_tst("%s:size[0x%zx]k\n",
		__func__, (sizeof(*pvpp) >> 10));
}

const struct dimn_pvpp_ops_s *dpvpp_ops(enum EPVPP_API_MODE mode)
{
	if (mode >= EPVPP_MEM_MODE_MAX)
		return NULL;

	if ((!IS_IC_SUPPORT(PRE_VPP_LINK) || !cfgg(EN_PRE_LINK)) &&
	    mode == EPVPP_API_MODE_PRE)
		return NULL;

	if ((!IS_IC_SUPPORT(POST_VPP_LINK) || !cfgg(EN_POST_LINK)) &&
	    mode == EPVPP_API_MODE_POST)
		return NULL;

	if (mode == EPVPP_API_MODE_POST)
		return get_datal()->dvs_pvpp.pst_ops;
	else if (mode == EPVPP_API_MODE_PRE)
		return get_datal()->dvs_pvpp.pre_ops;

	return NULL;
}

//dimn_vf_provider
const struct vframe_operations_s *dpvpp_vf_ops(enum EPVPP_API_MODE mode)
{
	if (mode >= EPVPP_MEM_MODE_MAX)
		return NULL;

	if (mode == EPVPP_API_MODE_POST || mode == EPVPP_API_MODE_PRE)
		return get_datal()->dvs_pvpp.vf_ops;

	return NULL;
}

const struct dimn_pvpp_ops_api_s *dpvpp_ops_api(enum EPVPP_API_MODE mode)
{
	if (mode >= EPVPP_MEM_MODE_MAX)
		return NULL;

	if ((!IS_IC_SUPPORT(PRE_VPP_LINK) || !cfgg(EN_PRE_LINK)) &&
	    mode == EPVPP_API_MODE_PRE)
		return NULL;

	if ((!IS_IC_SUPPORT(POST_VPP_LINK) || !cfgg(EN_POST_LINK)) &&
	    mode == EPVPP_API_MODE_POST)
		return NULL;

	if (!get_dim_de_devp())
		return NULL;

	if (mode == EPVPP_API_MODE_POST)
		return get_datal()->dvs_pvpp.pst_ops_api;
	else if (mode == EPVPP_API_MODE_PRE)
		return get_datal()->dvs_pvpp.pre_ops_api;

	return NULL;
}

void mif_cfg_v2(struct DI_MIF_S *di_mif,
	struct dvfm_s *pvfm,
	struct dim_mifpara_s *ppara,
	enum EPVPP_API_MODE mode)
{
	enum DI_MIF0_ID mif_index = 0;//di_mif->mif_index;
	struct di_win_s *win;
	//struct dvfm_s *pvfm;

	if (!di_mif || !pvfm || !ppara)
		return;
	dim_print("%s:%d:\n",
		__func__,
		di_mif->mif_index);
	mif_index = di_mif->mif_index;
	win = &ppara->win;
	if (pvfm->is_dec)
		di_mif->in_dec = 1;
	else
		di_mif->in_dec = 0;

	di_mif->canvas0_addr0 =
		ppara->cvs_id[0] & 0xff;
	di_mif->canvas0_addr1 =	ppara->cvs_id[1];
	di_mif->canvas0_addr2 = ppara->cvs_id[2];
	if (pvfm->is_linear) {
		di_mif->addr0 = pvfm->vfs.canvas0_config[0].phy_addr;
		di_mif->cvs0_w = pvfm->vfs.canvas0_config[0].width;
		di_mif->cvs1_w = 0;
		di_mif->cvs2_w = 0;
		if (pvfm->vfs.plane_num >= 2) {
			di_mif->addr1 = pvfm->vfs.canvas0_config[1].phy_addr;
			di_mif->cvs1_w = pvfm->vfs.canvas0_config[1].width;
		}
		if (pvfm->vfs.plane_num >= 3) {
			di_mif->addr2 = pvfm->vfs.canvas0_config[2].phy_addr;
			di_mif->cvs2_w = pvfm->vfs.canvas0_config[2].width;
		}
		di_mif->linear = 1;
		di_mif->buf_crop_en = 1;
		di_mif->linear_fromcvs = 1;//new for pre-post link
		if (pvfm->vfs.plane_num == 2) {
			di_mif->buf_hsize = pvfm->vfs.canvas0_config[0].width;
			dbg_ic("\t:buf_h[%d]\n", di_mif->buf_hsize);
		} else {
			//di_mif->buf_crop_en = 0;
			dbg_ic("\t:not nv21?\n");
			di_mif->buf_hsize = pvfm->buf_hsize;
		}

		if (pvfm->vfs.canvas0_config[0].block_mode)
			di_mif->block_mode = 1;
		else
			di_mif->block_mode = 0;
	} else {
		di_mif->linear = 0;
	}

	di_mif->burst_size_y	= 3;
	di_mif->burst_size_cb	= 1;
	di_mif->burst_size_cr	= 1;
	if (mode == EPVPP_API_MODE_PRE) {
		di_mif->hold_line = dimp_get(edi_mp_prelink_hold_line);
		di_mif->urgent	= dimp_get(edi_mp_pre_urgent);
	} else {
		di_mif->hold_line = dimp_get(edi_mp_post_hold_line);
		di_mif->urgent	= dimp_get(edi_mp_post_urgent);
	}
	di_mif->canvas_w = pvfm->vfs.canvas0_config[0].width;

	if (pvfm->vfs.type & VIDTYPE_VIU_NV12)
		di_mif->cbcr_swap = 1;
	else
		di_mif->cbcr_swap = 0;
	//if (ppara->linear) {
	if (pvfm->is_in_linear) {
		di_mif->reg_swap = 0;
		di_mif->l_endian = 1;

	} else {
		di_mif->reg_swap = 1;
		di_mif->l_endian = 0;
		//di_mif->cbcr_swap = 0;
	}
	dim_print("%s:%d:endian[%d]\n",
		  __func__, di_mif->mif_index, di_mif->l_endian);
	if (pvfm->vfs.bitdepth & BITDEPTH_Y10) {
		if (pvfm->vfs.type & VIDTYPE_VIU_444)
			di_mif->bit_mode =
			(pvfm->vfs.bitdepth & FULL_PACK_422_MODE) ?
			3 : 2;
		else if (pvfm->vfs.type & VIDTYPE_VIU_422)
			di_mif->bit_mode =
			(pvfm->vfs.bitdepth & FULL_PACK_422_MODE) ?
			3 : 1;
	} else {
		di_mif->bit_mode = 0;
	}
	if (pvfm->vfs.type & VIDTYPE_VIU_422) {
		/* from vdin or local vframe */
		/* 1. from vdin input mif */
		/* 2. from local mif */
		//if ((!IS_PROG(pvfm->type))	||
		//    ppara->prog_proc_type) {
		di_mif->video_mode = 1;
		di_mif->set_separate_en = 0;
		di_mif->src_field_mode = 0;
		di_mif->output_field_num = 0;
		di_mif->luma_x_start0	= win->x_st;
		di_mif->luma_x_end0	= win->x_end;
		di_mif->luma_y_start0	= win->y_st;
		if (IS_PROG(pvfm->vfs.type)) //
			di_mif->luma_y_end0 = win->y_end;
		else
			di_mif->luma_y_end0 = win->y_size / 2 - 1;

		di_mif->chroma_x_start0 = 0;
		di_mif->chroma_x_end0 = 0;
		di_mif->chroma_y_start0 = 0;
		di_mif->chroma_y_end0 = 0;

		//}
		di_mif->reg_swap = 1;
		di_mif->l_endian = 0;
		di_mif->cbcr_swap = 0;
	} else {
		if (pvfm->vfs.type & VIDTYPE_VIU_444)
			di_mif->video_mode = 2;
		else
			di_mif->video_mode = 0;
		if (pvfm->vfs.type &
		    (VIDTYPE_VIU_NV21 | VIDTYPE_VIU_NV12))
			di_mif->set_separate_en = 2;
		else
			di_mif->set_separate_en = 1;
		dim_print("%s:%d:vtype[0x%x]\n",
			  __func__, mif_index, pvfm->vfs.type);
		if (IS_PROG(pvfm->vfs.type) && ppara->prog_proc_type) {
			di_mif->src_field_mode = 0;
			di_mif->output_field_num = 0; /* top */
			di_mif->luma_x_start0	= win->x_st;
			di_mif->luma_x_end0 = win->x_end;
			di_mif->luma_y_start0 = win->y_st;
			di_mif->luma_y_end0 = win->y_end;
			di_mif->chroma_x_start0 = win->x_st / 2;
			di_mif->chroma_x_end0 = di_mif->chroma_x_start0 +
					win->x_size / 2 - 1;
			di_mif->chroma_y_start0 = win->y_st / 2;
			di_mif->chroma_y_end0 = di_mif->chroma_y_start0 +
				(win->y_size + 1) / 2 - 1;
		} else if ((ppara->cur_inp_type & VIDTYPE_INTERLACE) &&
				(ppara->cur_inp_type & VIDTYPE_VIU_FIELD)) {
			di_mif->src_prog = 0;
			di_mif->src_field_mode = 0;
			di_mif->output_field_num = 0; /* top */
			di_mif->luma_x_start0 = win->x_st;
			di_mif->luma_x_end0 = win->x_end;
			di_mif->luma_y_start0 = win->y_st;
			di_mif->luma_y_end0 = win->y_size / 2 - 1;
			di_mif->chroma_x_start0 = win->x_st / 2;
			di_mif->chroma_x_end0 = di_mif->chroma_x_start0 +
					win->x_size / 2 - 1;
			di_mif->chroma_y_start0 = win->y_st / 4;
			di_mif->chroma_y_end0 = di_mif->chroma_y_start0 +
				win->y_size / 4 - 1;
		} else {
			/*move to mp	di_mif->src_prog = force_prog?1:0;*/
			if (ppara->cur_inp_type  & VIDTYPE_INTERLACE)
				di_mif->src_prog = 0;
			else
				di_mif->src_prog =
				dimp_get(edi_mp_force_prog) ? 1 : 0;
			di_mif->src_field_mode = 1;
			if ((pvfm->vfs.type & VIDTYPE_TYPEMASK) ==
			    VIDTYPE_INTERLACE_TOP) {
				di_mif->output_field_num = 0; /* top */
				di_mif->luma_x_start0 = win->x_st;
				di_mif->luma_x_end0 = win->x_end;

				di_mif->luma_y_start0 = win->y_st;
				di_mif->luma_y_end0 = win->y_end; //??
				di_mif->chroma_x_start0 = win->x_st / 2;
				di_mif->chroma_x_end0 = di_mif->chroma_x_start0 +
					win->x_size / 2 - 1;
				di_mif->chroma_y_start0 = win->y_st / 2;
				di_mif->chroma_y_end0 = di_mif->chroma_y_start0 +
					(win->y_size + 1) / 2 - 1;
			} else {
				di_mif->output_field_num = 1;
				/* bottom */
				di_mif->luma_x_start0 = win->x_st;
				di_mif->luma_x_end0 = win->x_end;
				di_mif->luma_y_start0 = win->y_st + 1;
				di_mif->luma_y_end0 = win->y_end;
				di_mif->chroma_x_start0 = win->x_st;
				di_mif->chroma_x_end0 = di_mif->chroma_x_start0 +
					win->x_size / 2 - 1;
				di_mif->chroma_y_start0 =
					(di_mif->src_prog ? win->y_st : win->y_st + 1);
				di_mif->chroma_y_end0 = di_mif->chroma_y_start0 +
					(win->y_size + 1) / 2 - 1;
			}
		}
	}
	dim_print("\t%s:end\n", __func__);
}

/************************************************
 * from config_di_mif_v3
 *	para[0:2]: cvs id;
 *	para[3]:
 *2021-09-26: copy from mif_cfg in prepost_link.c
 *	change:delete di_buf;
 *	add win
 ************************************************/
void mif_cfg_v2_update_addr(struct DI_MIF_S *di_mif,
		struct dvfm_s *pvfm, struct dim_mifpara_s *ppara)
{
	if (!di_mif || !pvfm || !ppara)
		return;

	di_mif->canvas0_addr0 =
		ppara->cvs_id[0] & 0xff;
	di_mif->canvas0_addr1 =	ppara->cvs_id[1];
	di_mif->canvas0_addr2 = ppara->cvs_id[2];
	if (pvfm->is_linear) {
		di_mif->addr0 = pvfm->vfs.canvas0_config[0].phy_addr;
		di_mif->cvs0_w = pvfm->vfs.canvas0_config[0].width;

		if (pvfm->vfs.plane_num >= 2) {
			di_mif->addr1 = pvfm->vfs.canvas0_config[1].phy_addr;
			di_mif->cvs1_w = pvfm->vfs.canvas0_config[1].width;
		}
		if (pvfm->vfs.plane_num >= 3) {
			di_mif->addr2 = pvfm->vfs.canvas0_config[2].phy_addr;
			di_mif->cvs2_w = pvfm->vfs.canvas0_config[2].width;
		}
	}
}

unsigned int dpvpp_is_bypass_dvfm_prelink(struct dvfm_s *dvfm, bool en_4k)
{
	struct dim_pvpp_hw_s *hw;

	hw = &get_datal()->dvs_pvpp.hw;
	if (VFMT_IS_I(dvfm->vfs.type))
		return EPVPP_BYPASS_REASON_I;
	if (VFMT_IS_EOS(dvfm->vfs.type))
		return EPVPP_BYPASS_REASON_EOS;
	if (dvfm->vfs.type & DIM_BYPASS_VF_TYPE)
		return EPVPP_BYPASS_REASON_TYPE;
	if (en_4k) {
		if (dvfm->vfs.width > 3840 ||
		    dvfm->vfs.height > 2160)
			return EPVPP_BYPASS_REASON_SIZE_L;
		if (dvfm->is_4k && !hw->blk_rd_uhd && !dvfm->en_4k_snr)
			return EPVPP_BYPASS_REASON_BUF_UHD;
		else if (!dvfm->is_4k && !hw->blk_rd_hd)
			return EPVPP_BYPASS_REASON_BUF_HD;
	} else {
		if (dvfm->vfs.width > 1920 ||
		    dvfm->vfs.height > 1088)
			return EPVPP_BYPASS_REASON_SIZE_L;
		if (!hw->blk_rd_hd)
			return EPVPP_BYPASS_REASON_BUF_HD;
	}
	if (dvfm->vfs.flag & VFRAME_FLAG_HIGH_BANDWIDTH)
		return EPVPP_BYPASS_REASON_HIGH_BANDWIDTH;
	if (IS_COMP_MODE(dvfm->vfs.type)) {
		if (dim_afds() && !dim_afds()->is_supported_plink())
			return EPVPP_BYPASS_REASON_NO_AFBC;
	}
	if (dvfm->vfs.width < 128 || dvfm->vfs.height < 16)
		return EPVPP_BYPASS_REASON_SIZE_S;
	if (dpvpp_is_bypass())
		return EPVPP_BYPASS_REASON_DBG;
	if (dvfm->vfs.sei_magic_code == SEI_MAGIC_CODE &&
		(dvfm->vfs.fmt == VFRAME_SIGNAL_FMT_DOVI ||
		dvfm->vfs.fmt == VFRAME_SIGNAL_FMT_DOVI_LL))
		return EPVPP_BYPASS_REASON_DV_PATH;
	if (dvfm->vfs.flag & VFRAME_FLAG_GAME_MODE)
		return EPVPP_BYPASS_REASON_GAME_MODE;
	if (dvfm->vfs.duration < 1600 && dvfm->vfs.duration != 0)
		return EPVPP_BYPASS_REASON_120HZ;
	if (dvfm->vfs.fgs_valid)
		return EPVPP_BYPASS_REASON_FG;
	return 0;
}

unsigned int dpvpp_is_bypass_dvfm_postlink(struct dvfm_s *dvfm)
{
	struct dim_pvpp_hw_s *hw;

	hw = &get_datal()->dvs_pvpp.hw;

	if (VFMT_IS_EOS(dvfm->vfs.type))
		return EPVPP_BYPASS_REASON_EOS;
	if (dvfm->vfs.type & DIM_BYPASS_VF_TYPE)
		return EPVPP_BYPASS_REASON_TYPE;
	if (!IS_PRE_LOCAL(dvfm->vfs.type))
		return EPVPP_BYPASS_REASON_NO_LOCAL_BUF;
	if (dvfm->vfs.width > 1920 ||
	    dvfm->vfs.height > 1088)
		return EPVPP_BYPASS_REASON_SIZE_L;

	if (dvfm->vfs.flag & VFRAME_FLAG_HIGH_BANDWIDTH)
		return EPVPP_BYPASS_REASON_HIGH_BANDWIDTH;

	if (IS_COMP_MODE(dvfm->vfs.type)) {
		if (dim_afds() && !dim_afds()->is_supported_plink())
			return EPVPP_BYPASS_REASON_NO_AFBC;
	}
	if (dvfm->vfs.width < 128 || dvfm->vfs.height < 16)
		return EPVPP_BYPASS_REASON_SIZE_S;
	if (dpvpp_is_bypass())
		return EPVPP_BYPASS_REASON_DBG;
	if (dvfm->vfs.sei_magic_code == SEI_MAGIC_CODE &&
		(dvfm->vfs.fmt == VFRAME_SIGNAL_FMT_DOVI ||
		dvfm->vfs.fmt == VFRAME_SIGNAL_FMT_DOVI_LL))
		return EPVPP_BYPASS_REASON_DV_PATH;
	if (dvfm->vfs.flag & VFRAME_FLAG_GAME_MODE)
		return EPVPP_BYPASS_REASON_GAME_MODE;
	if (dvfm->vfs.fgs_valid)
		return EPVPP_BYPASS_REASON_FG;
	return 0;
}

void dpvpp_set_type_smp(struct dim_type_smp_s *smp,
		struct dvfm_s *dvfm)
{
	if (!smp || !dvfm)
		return;
	smp->vf_type = dvfm->vfs.type & VFMT_MASK_ALL;
	smp->x_size	= dvfm->vfs.width;
	smp->y_size	= dvfm->vfs.height;
	smp->is_4k_snr = dvfm->en_4k_snr;
	//smp->other?
}

unsigned int dpvpp_is_change_dvfm(struct dim_type_smp_s *in_last,
		struct dvfm_s *dvfm)
{
	if (in_last->vf_type != (dvfm->vfs.type & VFMT_MASK_ALL))
		return 1;
	if (in_last->x_size != dvfm->vfs.width ||
	    in_last->y_size != dvfm->vfs.height)
		return 2;
	if (in_last->is_4k_snr != dvfm->en_4k_snr)
		return 3;
	return 0;
}

void dpvpp_put_ready_vf(struct dimn_itf_s *itf,
				struct dim_pvpp_ds_s *ds,
				struct vframe_s *vfm)
{
	struct dimn_qs_cls_s *qready;
	struct di_buffer *buffer;

	qready	= &ds->lk_que_ready;
	if (itf->etype == EDIM_NIN_TYPE_VFM) {
		qready->ops.put(qready, vfm);
	} else {
		buffer = &itf->buf_bf[vfm->index];
		buffer->flag = 0;
		dbg_plink3("%s:[0x%px:0x%x]:vf[0x%px],%px index:%d\n",
			__func__, buffer, buffer->flag, buffer->vf,
			vfm, vfm->index);
		dbg_link("%s:type %x,%lx,frame_index=%d,queued=%d,dropped=%d,dummy=%d,file:%px\n",
			__func__,
			buffer->vf->type,
			buffer->vf->canvas0_config[0].phy_addr,
			buffer->vf->frame_index,
			buffer->caller_mng.queued,
			buffer->caller_mng.dropped,
			buffer->caller_mng.dummy,
			buffer->caller_mng.src_file);
		qready->ops.put(qready, buffer);
	}
}

/* @ary_note: buffer alloc by di			*/
/* @ary_note: use this api to put back display buffer	*/
/* @ary_note: same as vfm put */
//static
enum DI_ERRORTYPE dpvpp_fill_output_buffer(struct dimn_itf_s *itf,
					   struct di_buffer *buffer)
{
	struct dimn_qs_cls_s *qback;
	struct dimn_dvfm_s *dvfm = NULL;
	struct dim_pvpp_ds_s *ds = NULL;
	struct vframe_s *vf;

	if (!itf || !itf->ds) {
		PR_ERR("%s:index overflow\n", __func__);
		return DI_ERR_INDEX_OVERFLOW;
	}

	if (!buffer || !buffer->vf) {
		PR_ERR("%s:\n", __func__);
		return DI_ERR_INDEX_OVERFLOW;
	}

	ds = itf->ds;

	itf->c.sum_pst_put++;
	vf = buffer->vf;
	dvfm = (struct dimn_dvfm_s *)vf->private_data;
	if (dvfm->header.code != CODE_INS_LBF) {
		PR_ERR("%s:%px,code err 0x%x\n",
		       __func__, vf, dvfm->header.code);
		return DI_ERR_UNSUPPORT;
	}

	if (!atomic_dec_and_test(&dvfm->c.flg_get)) {
		PR_ERR("%s:%px,flg_vpp %d\n", __func__, dvfm,
			atomic_read(&dvfm->c.flg_get));
		return DI_ERR_UNSUPPORT;
	}
	qback = &ds->lk_que_kback;
	dbg_plink2("fill_output_buffer:in:0x%px:0x%px,0x%x\n",
		   buffer, vf, buffer->flag);
	qback->ops.put(qback, vf);
	task_send_wk_timer(EDIM_WKUP_REASON_BACK_BUF);

	return DI_ERR_NONE;
}

// use buffer to get itf
static enum DI_ERRORTYPE dpvpp_fill_output_buffer2_l(struct di_buffer *buffer)
{
	struct dimn_itf_s *itf;
	struct dim_pvpp_hw_s *hw;

	hw = &get_datal()->dvs_pvpp.hw;

	if (!buffer || !buffer->vf) {
		PR_ERR("%s:\n", __func__);
		return DI_ERR_INDEX_OVERFLOW;
	}
	if (!hw->reg_nub) {
		PR_WARN("%s:1:0x%px\n", __func__, buffer);
		if (buffer->vf)
			PR_WARN("di:1-1:0x%px\n", buffer->vf);
	}
	itf = get_itf_vfm(buffer->vf);
	if (!hw->reg_nub)
		PR_WARN("%s:2:0x%px\n", __func__, itf);
	if (!itf)
		return DI_ERR_INDEX_NOT_ACTIVE;
	return dpvpp_fill_output_buffer(itf, buffer);
}

enum DI_ERRORTYPE dpvpp_fill_output_buffer2(struct di_buffer *buffer)
{
	enum DI_ERRORTYPE ret;
	ulong irq_flag = 0;

	spin_lock_irqsave(&lock_pvpp, irq_flag);
	ret = dpvpp_fill_output_buffer2_l(buffer);
	spin_unlock_irqrestore(&lock_pvpp, irq_flag);

	return ret;
}

void dpvpp_ins_fill_out(struct dimn_itf_s *itf)
{
	struct dimn_qs_cls_s *qready;
//	struct dimn_dvfm_s *dvfm = NULL;
//	struct vframe_s *vf;
	struct di_buffer *buffer;
	unsigned int cnt_rd, cnt_out;
	ud buf[DIM_LKIN_NUB];
	int i;
	struct dimn_dvfm_s *ndvfm;
	struct dim_pvpp_ds_s *ds;

	if (!itf || !itf->ds || itf->etype != EDIM_NIN_TYPE_INS)
		return;
	ds = itf->ds;
	qready = &ds->lk_que_ready;
	cnt_rd = qready->ops.count(qready);
	if (!cnt_rd)
		return;
	cnt_out = qready->ops.out(qready, &buf[0], cnt_rd);
	dim_print("%s:%d:%d\n", __func__, cnt_out, cnt_rd);
	for (i = 0; i < cnt_out; i++) {
		buffer = (struct di_buffer *)buf[i];
		dim_print("%s:di_buffer:0x%px\n", __func__, buffer);
		if (!buffer->vf) { //ary 0615 need check vf
			PR_WARN("%s:\t buffer-vf null\n", __func__);
			return;
		}
		dim_print("%s:\t vf:0x%px\n", __func__, buffer->vf);
		dim_print("%s:\t caller:0x%px\n", __func__, buffer->caller_data);
		if (buffer->flag & DI_FLAG_BUF_BY_PASS) {
			itf->nitf_parm.ops.fill_output_done(buffer);
			itf->c.sum_pst_get++;
			itf->nitf_parm.ops.empty_input_done(buffer);
			itf->c.sum_pre_put++;
//#if defined(DBG_QUE_IN_OUT) || defined(DBG_QUE_INTERFACE)
			dbg_plink2("ins:out_done:bypass:0x%px:0x%x\n",
				   buffer, buffer->flag);
			dbg_plink2("ins:empty_done:bypass:0x%px:0x%x\n",
				   buffer->vf, buffer->vf->type);
//#endif
		} else {
			/*tmp*/
			ndvfm = (struct dimn_dvfm_s *)buffer->vf->private_data;
			atomic_inc(&ndvfm->c.flg_get);
			if (itf->link_mode == EPVPP_API_MODE_POST)
				buffer->flag |= DI_FLAG_I;
			else
				buffer->flag |= DI_FLAG_P;
			itf->nitf_parm.ops.fill_output_done(buffer);
			itf->c.sum_pst_get++;
//#if defined(DBG_QUE_IN_OUT) || defined(DBG_QUE_INTERFACE)
			dbg_plink2("ins:out_done:0x%px,ch[0x%x]:%d:%d:0x%x\n",
				   buffer, buffer->mng.ch, buffer->mng.index,
				   ndvfm->c.cnt_in, buffer->flag);
			dbg_plink2("\t:vf:0x%x,0x%x,0x%x\n",
				   buffer->vf->type, buffer->vf->flag,
				   buffer->vf->di_flag);
		dbg_link("out_done: %x,%lx,frame_index=%d,queued=%d,dropped=%d,dummy=%d,file:%px\n",
			buffer->vf->type,
			buffer->vf->canvas0_config[0].phy_addr,
			buffer->vf->frame_index,
			buffer->caller_mng.queued,
			buffer->caller_mng.dropped,
			buffer->caller_mng.dummy,
			buffer->caller_mng.src_file);

//#endif
		}
	}
}

//---------------------------------------------------------
static unsigned int dpvpp_create_instance_id(unsigned int ch)
{
	unsigned int tmp_cnt;

	tmp_cnt = atomic_inc_return(&get_datal()->cnt_reg_plink);
	if (tmp_cnt < DIM_PLINK_INSTANCE_ID_MIN ||
	    tmp_cnt > DIM_PLINK_INSTANCE_ID_MAX) {
		atomic_set(&get_datal()->cnt_reg_plink,
			DIM_PLINK_INSTANCE_ID_MIN);
		tmp_cnt = atomic_read(&get_datal()->cnt_reg_plink);
	}
	tmp_cnt = tmp_cnt * DI_PLINK_CN_NUB + ch;
	return tmp_cnt;
}

static int dpvpp_reg_idle_ch(void)
{
	struct dimn_itf_s *itf;
	unsigned int id;
	int ret = -1;

	for (id = 0; id < DI_PLINK_CN_NUB; id++) {
		itf = &get_datal()->dvs_pvpp.itf[id];
		if (atomic_read(&itf->reg))
			continue;

		if (atomic_read(&itf->reg)) {
			PR_WARN("%s:check\n", __func__);
			continue;
		}

		atomic_set(&itf->reg, 1);
		ret = id;

		break;
	}
	dbg_plink1("%s:id[%d]\n", __func__, ret);

	return ret;
}

int dpvpp_create_internal(struct di_ch_s *pch,
	enum EPVPP_API_MODE link_mode)
{
	struct dimn_itf_s *itf;
	int ret;
	struct di_init_parm *parm;

	/* try */
	ret = dpvpp_reg_idle_ch();
	if (ret < 0 || ret >= DI_PLINK_CN_NUB)
		return DI_ERR_REG_NO_IDLE_CH;
	itf = &get_datal()->dvs_pvpp.itf[ret];

	if (itf->ops_vfm &&
	    itf->ops_vfm->reg_pre_check &&
	    !itf->ops_vfm->reg_pre_check(itf)) {
		//to-do: resetting
		return DI_ERR_REG_NO_IDLE_CH;
	}

	/* reg count */
	itf->sum_reg_cnt = dpvpp_create_instance_id(pch->ch_id);

	dbg_mem("reg_cnt:[%d]\n", itf->sum_reg_cnt);

	itf->etype = pch->itf.etype; /* EDIM_NIN_TYPE_INS */
	itf->bind_ch = pch->ch_id;
	if (itf->etype == EDIM_NIN_TYPE_INS) {
		/*parm*/
		parm = &pch->itf.u.dinst.parm;
		memcpy(&itf->nitf_parm, parm, sizeof(itf->nitf_parm));
	} else {
		/* vframe path */
		itf->dvfm.name = pch->itf.dvfm.name;
	}

	if (!dpvpp_reg(itf, link_mode))
		PR_ERR("%s:can't reg?\n", __func__);

	itf->link_mode = link_mode;

	pch->itf.p_itf = itf;
	/* for tvp */
	check_tvp_state(pch);
	if (pch->is_tvp == 2)
		itf->c.is_tvp = 1;
	else
		itf->c.is_tvp = 0;
	if (itf->c.is_tvp) {
		itf->ds->o_hd.b.out_buf++;
		itf->ds->o_uhd.b.out_buf++;
	}
	dbg_plink1("%s:ch[%d]:id[%d] end tvp[%d]:%d:<%d,%d>\n",
		__func__, pch->ch_id, itf->id,
		itf->c.is_tvp, itf->sum_reg_cnt,
		itf->ds->o_hd.b.out_buf,
		itf->ds->o_uhd.b.out_buf);

	return DIM_PVPP_NUB;
}

int dpvpp_destroy_internal(struct dimn_itf_s *itf)
{
	struct di_ch_s *pch;
	int cur_ch;

	if (!itf) {
		PR_ERR("%s:index overflow\n", __func__);
		return DI_ERR_INDEX_OVERFLOW;
	}

	dbg_plink1("%s:enter:itf:%px\n", __func__, itf);

	if (!atomic_read(&itf->reg) || atomic_read(&itf->in_unreg)) {
		PR_WARN("%s:duplicate unreg(%d:%d)\n",
			__func__,
			atomic_read(&itf->reg),
			atomic_read(&itf->in_unreg));
		return DI_ERR_INDEX_NOT_ACTIVE;
	}
	cur_ch = itf->bind_ch;
	if (itf->link_mode == EPVPP_API_MODE_PRE)
		dpvpp_dct_unreg(itf->bind_ch);
	if (itf->ops_vfm && itf->ops_vfm->unreg_trig)
		itf->ops_vfm->unreg_trig(itf);

	atomic_set(&itf->in_unreg, 1);
	dpvpp_unreg_val(itf);
	if (cur_ch >= 0 && cur_ch < DI_PLINK_CN_NUB) {
		pch = get_chdata(cur_ch);
		if (pch)
			pch->link_mode = EPVPP_API_MODE_NONE;
	} else {
		PR_ERR("%s:bind_ch:%d error\n", __func__, cur_ch);
	}
	atomic_set(&itf->in_unreg, 0);
	atomic_set(&itf->reg, 0);

	dbg_plink2("%s:id[%d]\n", __func__, itf->id);
	return 0;
}

bool dpvpp_is_allowed(enum EPVPP_API_MODE mode)
{
	bool ret = false;

	if (mode == EPVPP_API_MODE_PRE)
		ret = get_datal()->dvs_pvpp.allowed & DI_BIT0;
	else if (mode == EPVPP_API_MODE_POST)
		ret = get_datal()->dvs_pvpp.allowed & DI_BIT1;
	return ret;
}

bool dpvpp_is_insert(enum EPVPP_API_MODE mode)
{
	bool ret = false;

	if (mode == EPVPP_API_MODE_PRE)
		ret = get_datal()->dvs_pvpp.insert & DI_BIT0;
	else if (mode == EPVPP_API_MODE_POST)
		ret = get_datal()->dvs_pvpp.insert & DI_BIT1;
	return ret;
}

bool dpvpp_is_en_polling(enum EPVPP_API_MODE mode)
{
	bool ret = false;

	if (mode == EPVPP_API_MODE_PRE)
		ret = get_datal()->dvs_pvpp.en_polling & DI_BIT0;
	else if (mode == EPVPP_API_MODE_POST)
		ret = get_datal()->dvs_pvpp.en_polling & DI_BIT1;
	return ret;
}

static unsigned int dpvpp_bypass_check_prelink(struct vframe_s *vfm, bool first)
{
	unsigned int reason = 0;
	bool is_4k = false;
	unsigned int x, y;

	dim_vf_x_y(vfm, &x, &y);
	if (x > 1920 || y > 1088)
		is_4k = true;
	dbg_dbg("vfm_type:0x%x", vfm->type);
	if (VFMT_IS_I(vfm->type))
		reason = EPVPP_BYPASS_REASON_I;
	if (IS_COMP_MODE(vfm->type)) {
		if (dim_afds() && !dim_afds()->is_supported_plink())
			reason = EPVPP_BYPASS_REASON_NO_AFBC;
	}
	if (dpvpp_is_bypass())
		reason = EPVPP_BYPASS_REASON_DBG;
	if (vfm->src_fmt.sei_magic_code == SEI_MAGIC_CODE &&
		(vfm->src_fmt.fmt == VFRAME_SIGNAL_FMT_DOVI ||
		vfm->src_fmt.fmt == VFRAME_SIGNAL_FMT_DOVI_LL))
		reason = EPVPP_BYPASS_REASON_DV_PATH;

	/* First EOS, vframe information maybe invalid. Just ignore it to create pre-link*/
	if (VFMT_IS_EOS(vfm->type) && first && !reason)
		return 0;

	if (!first) {
		/* Skip the first check to keep prelink pipeline will be activated */
		if (cfgg(4K)) {
			if (vfm->width > 3840 ||
			    vfm->height > 2160)
				reason = EPVPP_BYPASS_REASON_SIZE_L;
		} else {
			if (VFMT_IS_P(vfm->type) &&
			    (vfm->width > 1920 || vfm->height > (1080 + 8)))
				reason = EPVPP_BYPASS_REASON_SIZE_L;
		}
		if (vfm->width < 128 || vfm->height < 16)
			reason = EPVPP_BYPASS_REASON_SIZE_S;
		if (vfm->duration < 1600 && vfm->duration != 0)
			reason = EPVPP_BYPASS_REASON_120HZ;
		if (vfm->flag & VFRAME_FLAG_GAME_MODE)
			reason = EPVPP_BYPASS_REASON_GAME_MODE;
		if (vfm->fgs_valid)
			reason = EPVPP_BYPASS_REASON_FG;
		if (vfm->type & DIM_BYPASS_VF_TYPE)
			reason = EPVPP_BYPASS_REASON_TYPE;
		if (vfm->flag & VFRAME_FLAG_HIGH_BANDWIDTH)
			reason = EPVPP_BYPASS_REASON_HIGH_BANDWIDTH;
	}
	/* If vframe is EOS, information below maybe invalid */
	if (VFMT_IS_EOS(vfm->type))
		reason = EPVPP_BYPASS_REASON_EOS;
	return reason;
}

static unsigned int dpvpp_bypass_check_postlink(struct vframe_s *vfm, bool first)
{
	unsigned int reason = 0;

	dbg_dbg("vfm_type:0x%x", vfm->type);

	if (VFMT_IS_P(vfm->type) && !VFMT_IS_EOS(vfm->type))
		reason = EPVPP_BYPASS_REASON_P;

	if (IS_COMP_MODE(vfm->type)) {
		if (dim_afds() && !dim_afds()->is_supported_plink())
			reason = EPVPP_BYPASS_REASON_NO_AFBC;
	}
	if (dpvpp_is_bypass())
		reason = EPVPP_BYPASS_REASON_DBG;
	if (vfm->src_fmt.sei_magic_code == SEI_MAGIC_CODE &&
		(vfm->src_fmt.fmt == VFRAME_SIGNAL_FMT_DOVI ||
		vfm->src_fmt.fmt == VFRAME_SIGNAL_FMT_DOVI_LL))
		reason = EPVPP_BYPASS_REASON_DV_PATH;

	/* First EOS, vframe information maybe invalid. Just ignore it to create post-link*/
	if (VFMT_IS_EOS(vfm->type) && first && !reason)
		return 0;

	if (!first) {
		/* Skip the first check to keep prelink pipeline will be activated */
		if (cfgg(4K)) {
			if (vfm->width > 3840 ||
			    vfm->height > 2160)
				reason = EPVPP_BYPASS_REASON_SIZE_L;
		} else {
			if (vfm->width > 1920 || vfm->height > (1080 + 8))
				reason = EPVPP_BYPASS_REASON_SIZE_L;
		}
		if (vfm->width < 128 || vfm->height < 16)
			reason = EPVPP_BYPASS_REASON_SIZE_S;
		if (vfm->duration < 1600 && vfm->duration != 0)
			reason = EPVPP_BYPASS_REASON_120HZ;
		if (vfm->flag & VFRAME_FLAG_GAME_MODE)
			reason = EPVPP_BYPASS_REASON_GAME_MODE;
		if (vfm->fgs_valid)
			reason = EPVPP_BYPASS_REASON_FG;
		if (vfm->type & DIM_BYPASS_VF_TYPE)
			reason = EPVPP_BYPASS_REASON_TYPE;
		if (vfm->flag & VFRAME_FLAG_HIGH_BANDWIDTH)
			reason = EPVPP_BYPASS_REASON_HIGH_BANDWIDTH;
	}
	if (VFMT_IS_EOS(vfm->type))
		reason = EPVPP_BYPASS_REASON_EOS;
	return reason;
}

bool dpvpp_try_reg(struct di_ch_s *pch, struct vframe_s *vfm,
		enum EPVPP_API_MODE link_mode)
{
	bool ponly_enable = false;
	unsigned int reason = 0;

	if (!dpvpp_ops(link_mode)	||
	    !dpvpp_is_allowed(link_mode)	||
	    !dpvpp_is_insert(link_mode))
		return false;

	if (link_mode == EPVPP_API_MODE_PRE)
		reason = dpvpp_bypass_check_prelink(vfm, true);
	else if (link_mode == EPVPP_API_MODE_POST)
		reason = dpvpp_bypass_check_postlink(vfm, true);
	else
		reason = EPVPP_BYPASS_REASON_INVALID;
	dim_bypass_set(pch, 0, reason);
	if (reason != 0)
		return false;

	pch->src_type = vfm->source_type; //for 4K SNR

	//check  ponly:
	if (link_mode == EPVPP_API_MODE_PRE) {
		if ((vfm->flag & VFRAME_FLAG_DI_P_ONLY)/* || bget(&dim_cfg, 1)*/)
			ponly_enable = true;

		if (!ponly_enable &&
		    cfgg(PONLY_MODE) == 1 &&
		    (vfm->type & VIDTYPE_TYPEMASK) == VIDTYPE_PROGRESSIVE &&
		    !(vfm->type & VIDTYPE_FORCE_SIGN_IP_JOINT)) {
			ponly_enable = true;
		}

		/* fg case will be checked in dpvpp_bypass_check and dpvpp_is_bypass */
		//if (get_datal()->fg_bypass_en && vfm->fgs_valid)
		//	ponly_enable = false;

		if (!ponly_enable) {
			dbg_plink1("%s:not ponly:%d:0x%x, fgs_valid:[0x%x]\n",
				__func__,
				cfgg(PONLY_MODE), vfm->type, vfm->fgs_valid);
			return false;
		}
	}

	if (dpvpp_create_internal(pch, link_mode) < 0) {
		dbg_plink1("%s:failed\n", __func__);
		return false;
	}
	pch->link_mode = link_mode;
	return true;
}

/*di_display_pvpp_link*/
int dim_pvpp_link_display(struct vframe_s *vfm,
			  struct pvpp_dis_para_in_s *in_para, void *out_para)
{
	int ret = 0;
	struct pvpp_dis_para_in_s *in;
	static int last_x, last_v;
	enum EPVPP_API_MODE mode;

	if (!in_para) {
		PR_ERR("%s: in_para is null\n", __func__);
		return 0;
	}
	mode = in_para->link_mode;
	di_g_plink_dbg()->display_cnt++;
	if (in_para->unreg_bypass) {
		if (dpvpp_ops_api(mode) && dpvpp_ops_api(mode)->display_unreg_bypass)
			ret = dpvpp_ops_api(mode)->display_unreg_bypass();
		else
			PR_ERR("%s:no unreg_bypass?\n", __func__);
		return ret;
	}
	if (!vfm)
		return 0;
	dbg_plink3("%s:0x%px, 0x%px:\n", __func__, vfm, in_para);
	/* dbg only */

	if (mode == EPVPP_API_MODE_PRE)
		dimp_set(edi_mp_prelink_hold_line, in_para->follow_hold_line);
	else if (mode == EPVPP_API_MODE_POST)
		dimp_set(edi_mp_post_hold_line, in_para->follow_hold_line);
	if (last_x != in_para->win.x_end ||
	    last_v != in_para->win.y_end) {
		dbg_plink1("itf:disp:w<%d,%d><%d,%d>m%d\n",
			last_x,
			last_v,
			in_para->win.x_end,
			in_para->win.y_end,
			in_para->dmode);
		last_x = in_para->win.x_end;
		last_v = in_para->win.y_end;
	}

	if (dpvpp_ops_api(mode) && dpvpp_ops_api(mode)->display) {
		ret = dpvpp_ops_api(mode)->display(vfm, in_para, out_para);
		in = in_para;
		dbg_plink3("%s:ret:0x%x:\n", "api:display", ret);
		if (in)
			dbg_plink3("\tm[%d]:\n", in->dmode);

		if (di_g_plink_dbg()->display_sts != ret) {
			dbg_plink1("%s:0x%x->0x%x\n", "api:display",
			       di_g_plink_dbg()->display_sts, ret);
			di_g_plink_dbg()->display_sts = ret;
		}
	}

	return ret;
}

int dpvpp_check_vf(struct vframe_s *vfm)
{
	int ret = 0;
#if defined(DBG_QUE_IN_OUT) || defined(DBG_QUE_INTERFACE)
	static ud vfm_last; //dbg only
	ud vfm_cur;
#endif
	enum EPVPP_API_MODE mode;

	if (!vfm) {
		PR_WARN("%s:no vf\n", __func__);
		return -1;
	}
	if (vfm->di_flag &  DI_FLAG_DI_PSTVPPLINK)
		mode = EPVPP_API_MODE_POST;
	else
		mode = EPVPP_API_MODE_PRE;
	if (dpvpp_ops_api(mode) && dpvpp_ops_api(mode)->check_vf)
		ret = dpvpp_ops_api(mode)->check_vf(vfm);

	if (di_g_plink_dbg()->flg_check_vf != ret) { /*dbg*/
		dbg_plink1("%s:0x%x->0x%x\n", "api:check vf",
		       di_g_plink_dbg()->flg_check_vf,
		       ret);
		di_g_plink_dbg()->flg_check_vf = ret;
	}

#if defined(DBG_QUE_IN_OUT) || defined(DBG_QUE_INTERFACE)
	vfm_cur = (ud)vfm;
	if (vfm_cur != vfm_last) {
		dbg_plink3("%s:0x%px:0x%x\n", "api:check",
			   vfm,
			   ret);
		dbg_plink3("\tfunc:0x%px\n", vfm->process_fun);
		vfm_last = vfm_cur;
	}
#endif
	return ret;
}

int dpvpp_check_di_act(bool interlace)
{
	int ret = EPVPP_ERROR_DI_NOT_REG;
	enum EPVPP_API_MODE mode;

	if (interlace)
		mode = EPVPP_API_MODE_POST;
	else
		mode = EPVPP_API_MODE_PRE;
	if (dpvpp_ops_api(mode) && dpvpp_ops_api(mode)->check_di_act)
		ret = dpvpp_ops_api(mode)->check_di_act(mode);
	if (di_g_plink_dbg()->flg_check_di_act != ret) {
		dbg_plink1("%s:%d->%d\n", "api:check act",
		       di_g_plink_dbg()->flg_check_di_act,
		       ret);
		di_g_plink_dbg()->flg_check_di_act = ret;
	}

	return ret;
}

int dpvpp_sw(bool on, bool interlace)
{
	int ret = 0;
	enum EPVPP_API_MODE mode;

	if (interlace)
		mode = EPVPP_API_MODE_POST;
	else
		mode = EPVPP_API_MODE_PRE;
	if (get_datal()->cur_link_mode != EPVPP_API_MODE_NONE &&
	    mode != get_datal()->cur_link_mode) {
		PR_WARN("api mode mismatch: cur:%d, new:%d, on:%d\n",
			get_datal()->cur_link_mode, mode, on);
		return -1;
	}
	if (dpvpp_ops_api(mode) && dpvpp_ops_api(mode)->vpp_sw)
		ret = dpvpp_ops_api(mode)->vpp_sw(on);
	if (di_g_plink_dbg()->flg_sw != on) {
		dbg_plink1("%s:%d->%d:%d\n", "api:sw",
		       di_g_plink_dbg()->flg_sw,
		       on, ret);
		di_g_plink_dbg()->flg_sw = on;
	}

	return ret;
}

unsigned int dpvpp_get_ins_id(void)
{
	unsigned int ret = 0;
	struct dim_pvpp_hw_s *hw;

	if (!get_dim_de_devp() || !get_dim_de_devp()->data_l)
		return 0;

	hw = &get_datal()->dvs_pvpp.hw;
	ret = (unsigned int)atomic_read(&hw->link_instance_id);
	return ret;
}

