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

#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
#include <linux/amlogic/media/video_sink/video.h>
#endif

/**********************************************
 * bit [0:1] for debug level
 **********************************************/
//static unsigned int tst_post_vpp;
//module_param_named(tst_post_vpp, tst_post_vpp, uint, 0664);

static void dpvpph_post_frame_reset_g12(bool pvpp_link,
	const struct reg_acc *op_in)
{
	if (!cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		return;
	if (DIM_IS_IC_BF(SC2)) {
		if (pvpp_link) {
			if (op_in->rd(DI_POST_GL_CTRL) & 0x80000000)
				op_in->bwr(DI_POST_GL_CTRL, 0, 31, 1);
			return;
		}
	} else {
		if (pvpp_link) {
			if (op_in->rd(DI_SC2_POST_GL_CTRL) & 0x80000000)
				op_in->bwr(DI_SC2_POST_GL_CTRL, 0, 31, 1);
			return;
		}
	}
}

irqreturn_t dpvpp_post_irq(int irq, void *dev_instance)
{
	struct dim_pvpp_ds_s *ds;
	struct dim_pvpp_hw_s *hw;
	unsigned int id;

	unsigned int data32 = RD(DI_INTR_CTRL);
	unsigned int mask32 = (data32 >> 16) & 0x3ff;
	unsigned int flag = 0;

	hw = &get_datal()->dvs_pvpp.hw;
	id = hw->id_c;
	ds = get_datal()->dvs_pvpp.itf[id].ds;
	if (!ds) {
		PR_ERR("%s:no ds\n", __func__);
		return IRQ_HANDLED;
	}

	data32 &= 0x3fffffff;
	if ((data32 & 4) && !(mask32 & 4)) {
		dim_print("irq post wr ==\n");
		flag = 1;
	} else {
		dim_print("irq DI IRQ 0x%x ==\n", data32);
		flag = 0;
	}
	return IRQ_HANDLED;
}

//static
static bool dpvpp_post_reg_ch(struct dimn_itf_s *itf)
{
	struct dim_pvpp_hw_s *hw;
	struct dim_pvpp_ds_s *ds;
	//unsigned int out_fmt;
	//struct dvfm_s *dvfm_dm;

	if (!itf)
		return false;
	if (!itf->ds)
		return false;
	if (itf->c.src_state & EDVPP_SRC_NEED_MSK_CRT)
		return false;

	hw = &get_datal()->dvs_pvpp.hw;
	ds = itf->ds;
	if (!itf->c.flg_reg_mem) {
		PR_INF("%s:ch[%d]: reg_nub:%d\n", __func__,
			itf->bind_ch, hw->reg_nub + 1);

		if (!ds->en_out_afbce)
			itf->c.src_state &= ~(EDVPP_SRC_NEED_SCT_TAB);
		hw->reg_nub++;
		itf->c.flg_reg_mem = true;
		itf->c.src_state &= (~EDVPP_SRC_NEED_MEM);
		itf->c.src_state &= (~EDVPP_SRC_NEED_REG2);
		get_datal()->pst_vpp_exist = true; /* ?? */
	}
	return true;
}

static void dpvpp_post_irq_hw_reg(void)
{
	struct dim_pvpp_hw_s *hw;

	hw = &get_datal()->dvs_pvpp.hw;
	hw->intr_mode = 3;
}

int dpvpp_reg_postlink_sw(bool vpp_disable_async)
{
	unsigned int ton, ton_vpp, ton_di;
	struct platform_device *pdev;
	struct dim_pvpp_hw_s *hw;

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
		get_datal()->pst_vpp_active = true;/* interface */
		hw->has_notify_vpp = false;
		if (dim_check_exit_process()) {
		//can sw:
			if (dpvpp_set_one_time()) {
				pdev = get_dim_de_devp()->pdev;
				if (!pdev)
					return false;
				dpvpph_post_frame_reset_g12(true, hw->op_n);
			}
			dpvpp_post_irq_hw_reg();
			atomic_set(&hw->link_sts, ton);//on
			get_datal()->pst_vpp_set = true; /* interface */
			atomic_set(&hw->link_in_busy, 0);
			ext_vpp_plink_state_changed_notify();
			PR_INF("%s:set active:<%d, %d> op:%px\n",
			       __func__, ton_vpp, ton_di, hw->op_n);
		}
	} else {
		bool sw_done = false;

		//off:
		/* check if bypass */
		if (!ton_di && !hw->has_notify_vpp) {
			/* non-block */
			ext_vpp_disable_plink_notify(vpp_disable_async);
			ext_vpp_plink_real_sw(false, false, true);
			hw->has_notify_vpp = true;
			sw_done = true;
		}
		if (hw->dis_last_para.dmode != EPVPP_DISPLAY_MODE_BYPASS) {
			PR_INF("wait for bypass\n");
			return 0;
		}
		if (!sw_done)
			ext_vpp_plink_real_sw(false, true, true);
		atomic_set(&hw->link_sts, 0);//on
		get_datal()->pst_vpp_active = false;/* interface */
		atomic_set(&hw->link_in_busy, 0);
		ext_vpp_plink_state_changed_notify();
		PR_INF("%s:set inactive<%d, %d> arb:%x\n", __func__,
				ton_vpp, ton_di, RD(DI_ARB_DBG_STAT_L1C1));
	}
	return 0;
}

static void dpvpp_post_queue_to_display(struct dimn_itf_s *itf,
		struct di_buf_s *di_buf)
{
	if (!di_buf || !itf) {
		PR_ERR("%s:no di_buf 0x%px or no itf 0x%px\n",
			__func__, di_buf, itf);
		return;
	}
	queue_out(itf->bind_ch, di_buf);
	queue_in(itf->bind_ch, di_buf, QUEUE_DISPLAY);
}

static bool update_vframe_meta(struct vframe_s *vfm, u8 from)
{
	struct dimn_dvfm_s *ndvfm = NULL;
	struct di_buf_s *di_buf = NULL;

	if (!vfm)
		return false;

	ndvfm = (struct dimn_dvfm_s *)vfm->private_data;
	if (ndvfm)
		di_buf = (struct di_buf_s *)ndvfm->c.di_buf;
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
	clear_vframe_src_fmt(vfm);
	if (di_buf && di_buf->local_meta &&
	    di_buf->local_meta_used_size)
		update_vframe_src_fmt(vfm,
			di_buf->local_meta,
			di_buf->local_meta_used_size,
			false, NULL, NULL);
#endif
#ifdef DIM_EN_UD_USED
	/* reset ud_param ptr */
	if (di_buf && di_buf->local_ud &&
	    di_buf->local_ud_used_size &&
	    is_ud_param_valid(vfm->vf_ud_param)) {
		vfm->vf_ud_param.ud_param.pbuf_addr = (void *)di_buf->local_ud;
		vfm->vf_ud_param.ud_param.buf_len = di_buf->local_ud_used_size;
	} else {
		vfm->vf_ud_param.ud_param.pbuf_addr = NULL;
		vfm->vf_ud_param.ud_param.buf_len = 0;
	}
#else
	vfm->vf_ud_param.ud_param.pbuf_addr = NULL;
	vfm->vf_ud_param.ud_param.buf_len = 0;
#endif

	if (di_buf)
		dbg_plink2("%s:#%d: vfm:%px di_buf:%px meta:<%px %d> ud:<%px %d - %px %d>\n",
			__func__, from, vfm, di_buf,
			di_buf->local_meta, di_buf->local_meta_used_size,
			di_buf->local_ud, di_buf->local_ud_used_size,
			vfm->vf_ud_param.ud_param.pbuf_addr,
			vfm->vf_ud_param.ud_param.buf_len);
	else
		dbg_plink2("%s:#%d: vfm:%px no di_buf\n",
			__func__, from, vfm);
	return true;
}

static bool post_vtype_fill_d(struct dimn_itf_s *itf,
		struct vframe_s *vfmt,
		struct vframe_s *vfmf,
		struct dvfm_s *by_dvfm)
{
	void *priv;
	unsigned int index;

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
	dbg_plink2("%s: %px vfmt type:%x flag:%x di_flag:%x bitdepth:%x <%x %x %x %px>\n",
		__func__, vfmt, vfmt->type, vfmt->flag, vfmt->di_flag, vfmt->bitdepth,
		by_dvfm->vfs.type, by_dvfm->vfs.flag, by_dvfm->vfs.di_flag,
		by_dvfm->vf_ext);

	vfmt->canvas0Addr = (u32)-1;
	vfmt->canvas1Addr = (u32)-1;
	vfmt->di_instance_id = by_dvfm->sum_reg_cnt;
	if (by_dvfm->is_prvpp_link) {
		dbg_plink2("%s:by type:0x%x\n", __func__, by_dvfm->vfs.type);
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
			memcpy(&vfmt->canvas0_config[0],
			       &by_dvfm->vfs.canvas0_config[0],
			       sizeof(vfmt->canvas0_config));
		}
		vfmt->vf_ext = by_dvfm->vf_ext;
		if (vfmt->vf_ext)
			vfmt->flag |= VFRAME_FLAG_DOUBLE_FRAM; /* for vf_ext */

		vfmt->early_process_fun = NULL;
		vfmt->type |= VIDTYPE_PRE_INTERLACE;
		dbg_plink2("%s: vfmt[%d]:%px type:0x%x addr:0x%lx plane_num:%d private:%px\n",
			__func__, index, vfmt, vfmt->type,
			vfmt->canvas0_config[0].phy_addr,
			vfmt->plane_num, vfmt->private_data);

		vfmt->process_fun = NULL;

		if (IS_COMP_MODE(vfmf->type)) { //??
			vfmt->width = vfmt->compWidth;
			vfmt->height = vfmt->compHeight;
		}
		vfmt->di_flag &= ~DI_FLAG_DCT_DS_RATIO_MASK;
		update_vframe_meta(vfmt, 1);
	} else {
		PR_ERR("%s:not postlink vfm %px\n", __func__, by_dvfm);
	}
	return true;
}

static bool post_vtype_fill_local_bypass(struct vframe_s *vfmt,
		struct vframe_s *vfmf)
{
	void *priv;
	unsigned int index;

	if (!vfmt || !vfmf)
		return false;
	/* keep private and index */
	priv = vfmt->private_data;
	index = vfmt->index;
	memcpy(vfmt, vfmf, sizeof(*vfmt));
	vfmt->private_data = priv;
	vfmt->index	= index;

	vfmt->canvas0Addr = (u32)-1;
	vfmt->canvas1Addr = (u32)-1;
	vfmt->di_instance_id = -1;
	vfmt->vf_ext = NULL;
	vfmt->early_process_fun = NULL;
	vfmt->type |= VIDTYPE_PRE_INTERLACE;
	vfmt->process_fun = NULL;
	dbg_plink2("%s: vfmf:%px vfmt[%d]:%px addr:0x%lx plane_num:%d private:%px\n",
		__func__, vfmf, index, vfmt,
		vfmt->canvas0_config[0].phy_addr,
		vfmt->plane_num, vfmt->private_data);
	update_vframe_meta(vfmt, 0);
	return true;
}

/* cnt: for */
static bool dpvpp_post_bypass(struct dimn_itf_s *itf,
				unsigned int cnt)
{
	struct dimn_qs_cls_s *qin, *qready, *qidle;
	struct dimn_dvfm_s *dvfm = NULL;
	struct vframe_s *vf, *vf_ori;
	struct di_buffer *buffer;
	unsigned int cnt_in;
	int i;
	struct dim_pvpp_ds_s *ds;
	struct di_buf_s *di_buf;

	if (!itf || !itf->ds)
		return false;
	ds = itf->ds;

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
		di_buf = (struct di_buf_s *)dvfm->c.di_buf;
		if (!di_buf && !dvfm->c.ori_in) {
			PR_ERR("%s: buffer and ori_in NULL vf %px\n", __func__, vf);
			qidle->ops.put(qidle, vf);
			break;
		}

		dbg_plink2("%s: vf:%px ori_in:%px di_buf:%px di_buf->vframe:%px cnt:%d cnt_in:%d\n",
			__func__, vf, dvfm->c.ori_in,
			di_buf, di_buf ? di_buf->vframe : NULL,
			cnt, cnt_in);
		if (dvfm->c.ori_in) {
			if (itf->etype == EDIM_NIN_TYPE_VFM) {
				vf_ori = (struct vframe_s *)dvfm->c.ori_in;
				vf_ori->di_flag &= ~DI_FLAG_DCT_DS_RATIO_MASK;
				vf_ori->di_flag &= ~DI_FLAG_DI_PVPPLINK;
				vf_ori->di_flag &= ~(DI_FLAG_DI_PSTVPPLINK | DI_FLAG_DI_LOCAL_BUF);
				vf_ori->di_flag |= DI_FLAG_DI_PVPPLINK_BYPASS | DI_FLAG_DI_BYPASS;
				vf_ori->private_data = NULL;
				dvfm->c.di_buf = NULL;
				if (di_buf)
					recycle_vframe_type_post(di_buf, itf->bind_ch);
				qidle->ops.put(qidle, vf);
				qready->ops.put(qready, vf_ori);
			} else {
				/*EDIM_NIN_TYPE_INS*/
				buffer = (struct di_buffer *)dvfm->c.ori_in;
				buffer->flag |= DI_FLAG_BUF_BY_PASS;
				if (buffer->vf) {
					buffer->vf->di_flag &= ~DI_FLAG_DCT_DS_RATIO_MASK;
					buffer->vf->di_flag &= ~DI_FLAG_DI_PVPPLINK;
					buffer->vf->di_flag &=
						~(DI_FLAG_DI_PSTVPPLINK | DI_FLAG_DI_LOCAL_BUF);
					buffer->vf->di_flag |= DI_FLAG_DI_PVPPLINK_BYPASS;
					if (di_buf)
						buffer->caller_mng = di_buf->caller_mng;
				}
				dvfm->c.di_buf = NULL;
				if (di_buf)
					recycle_vframe_type_post(di_buf, itf->bind_ch);
				qidle->ops.put(qidle, vf);
				qready->ops.put(qready, buffer);
			}
		} else {
			/* only bypass by local buffer vf */
			if (itf->etype == EDIM_NIN_TYPE_VFM) {
				post_vtype_fill_local_bypass(vf, &dvfm->c.vf_in_cp);
				vf->di_flag &= ~DI_FLAG_DCT_DS_RATIO_MASK;
				vf->di_flag &= ~(DI_FLAG_DI_PVPPLINK | DI_FLAG_DI_PSTVPPLINK);
				vf->di_flag |=
					(DI_FLAG_DI_PVPPLINK_BYPASS | DI_FLAG_DI_LOCAL_BUF);
				qready->ops.put(qready, vf);
			} else {
				buffer = &itf->buf_bf[vf->index];
				buffer->flag = 0;
				if (buffer->vf) {
					post_vtype_fill_local_bypass(vf, &dvfm->c.vf_in_cp);
					buffer->vf->di_flag &= ~DI_FLAG_DCT_DS_RATIO_MASK;
					buffer->vf->di_flag &=
						~(DI_FLAG_DI_PVPPLINK | DI_FLAG_DI_PSTVPPLINK);
					buffer->vf->di_flag |=
						(DI_FLAG_DI_PVPPLINK_BYPASS | DI_FLAG_DI_LOCAL_BUF);
				}
				qready->ops.put(qready, buffer);
			}
			dpvpp_post_queue_to_display(itf, di_buf);
		}
		if (qready->ops.is_full(qready))
			break;
	}
	return true;
}

static void *dpvpp_get_bypass_post_frame(u8 ch, struct di_buf_s *di_buf)
{
	struct dim_nins_s *nins;
	struct di_ch_s *pch;
	void *in_ori = NULL;
	struct di_buf_s *ibuf;
	struct di_buffer *buffer;

	pch = get_chdata(ch);
	if (pch && (di_buf->is_nbypass || di_buf->is_eos)) {
		//check:
		if (di_buf->type != VFRAME_TYPE_POST) {
			PR_ERR("%s:bypass not post ?\n", __func__);
			return NULL;
		}
		ibuf = di_buf->di_buf_dup_p[0];
		if (!ibuf) {
			PR_ERR("%s:bypass no in ?\n", __func__);
			return NULL;
		}
		nins = (struct dim_nins_s *)ibuf->c.in;
		if (!nins) {
			PR_ERR("%s:no in ori ?\n", __func__);
			return NULL;
		}
		in_ori = nins->c.ori;
		/* recycle */
		nins->c.ori = NULL;
		nins_used_some_to_recycle(pch, nins);

		ibuf->c.in = NULL;
		//di_buf_clear(pch, di_buf);
		//di_que_in(pch->ch_id, QUE_PST_NO_BUF, di_buf);
		di_buf->di_buf_dup_p[0] = NULL;
		queue_in(pch->ch_id, ibuf, QUEUE_RECYCLE);

		/* to ready buffer */
		if (dip_itf_is_ins(pch)) {
			/* need mark bypass flg*/
			buffer = (struct di_buffer *)in_ori;
			buffer->flag |= DI_FLAG_BUF_BY_PASS;
		}
		return in_ori;
	}
	return NULL;
}

static void dpvpp_post_fill_outvf(struct vframe_s *vfm,
		struct di_buf_s *di_buf,
		enum EDPST_OUT_MODE mode)
{
	struct canvas_config_s *cvsp;
	unsigned int cvsh, cvsv, csize;
	unsigned int ch;
	struct di_ch_s *pch;
#ifdef CONFIG_AMLOGIC_MEDIA_THERMAL1
	unsigned int ori_vfm_bitdepth;
#endif

	ch = di_buf->channel;
	pch = get_chdata(ch);

	/* canvas */
	vfm->canvas0Addr = (u32)-1;
	if (mode == EDPST_OUT_MODE_DEF) {
		vfm->plane_num = 1;
		cvsp = &vfm->canvas0_config[0];
		cvsp->phy_addr = di_buf->nr_adr;
		cvsp->block_mode = 0;
		cvsp->endian = 0;
		cvsp->width = di_buf->canvas_width[NR_CANVAS];
		cvsp->height = di_buf->canvas_height;
	} else {
		vfm->plane_num = 2;
		/* count canvs size */
		di_cnt_cvs_nv21(0, &cvsh, &cvsv, di_buf->channel);
		/* 0 */
		cvsp = &vfm->canvas0_config[0];
		cvsp->phy_addr = di_buf->nr_adr;
		cvsp->width = cvsh;
		cvsp->height = cvsv;
		cvsp->block_mode = 0;
		cvsp->endian = 0;

		csize = roundup((cvsh * cvsv), PAGE_SIZE);
		/* 1 */
		cvsp = &vfm->canvas0_config[1];
		cvsp->phy_addr = di_buf->nr_adr + csize;
		cvsp->width = cvsh;
		cvsp->height = cvsv;
		cvsp->block_mode = 0;
		cvsp->endian = 0;
	}

	dbg_plink2("%s:addr0[0x%lx], 1[0x%lx],w[%d,%d]\n",
		__func__,
		(unsigned long)vfm->canvas0_config[0].phy_addr,
		(unsigned long)vfm->canvas0_config[1].phy_addr,
		vfm->canvas0_config[0].width,
		vfm->canvas0_config[0].height);

	/* type */
	if (mode == EDPST_OUT_MODE_NV21 ||
	    mode == EDPST_OUT_MODE_NV12) {
		/*clear*/
		vfm->type &= ~(VIDTYPE_VIU_NV12	|
			VIDTYPE_VIU_444	|
			VIDTYPE_VIU_NV21	|
			VIDTYPE_VIU_422	|
			VIDTYPE_VIU_SINGLE_PLANE	|
			VIDTYPE_COMPRESS	|
			VIDTYPE_PRE_INTERLACE);
		vfm->type |= VIDTYPE_VIU_FIELD;
		if (mode == EDPST_OUT_MODE_NV21)
			vfm->type |= VIDTYPE_VIU_NV21;
		else
			vfm->type |= VIDTYPE_VIU_NV12;

		/* bit */
		vfm->bitdepth &= ~(BITDEPTH_MASK);
		vfm->bitdepth &= ~(FULL_PACK_422_MODE);
		vfm->bitdepth |= (BITDEPTH_Y8	|
			BITDEPTH_U8	|
			BITDEPTH_V8);
	}
#ifdef CONFIG_AMLOGIC_MEDIA_THERMAL1
	ori_vfm_bitdepth = vfm->bitdepth;
	if (di_buf->bit_8_flag == 1) {
		dbg_plink2("bitdepth: 0x%x\n", vfm->bitdepth);
		vfm->bitdepth &= ~(BITDEPTH_MASK);
		vfm->bitdepth |= (FULL_PACK_422_MODE);
		vfm->bitdepth |= (BITDEPTH_Y8	| BITDEPTH_U8 | BITDEPTH_V8);
	} else if (!di_buf->bit_8_flag) {
		vfm->bitdepth = ori_vfm_bitdepth;
	}
	dbg_plink2("%s bitdepth: 0x%x\n", __func__, vfm->bitdepth);
#endif
	if (DIM_IS_IC(T5DB) || DIM_IS_IC(T5) || DIM_IS_IC(T6D)) {
		if (di_reverse)
			vfm->flag |= (VFRAME_FLAG_MIRROR_H |
			VFRAME_FLAG_MIRROR_V);
		dbg_plink2("%s di_reverse: 0x%x\n", __func__, di_reverse);
	}
}

static void dpvpp_post_feed_buffer(struct dimn_itf_s *itf,
		struct di_buf_s *di_buf)
{
	struct dim_pvpp_ds_s *ds = NULL;
	struct di_buffer *buffer_ori;
	struct dimn_qs_cls_s *qidle, *qin;
	unsigned int free_nub;
	struct vframe_s *vf, *mvf;
	struct dimn_dvfm_s *ndvfm;
	struct di_buffer *buffer_l;
	unsigned int err_cnt = 0;
	bool flg_q;
	bool is_eos;
	bool is_bypass = false;

	if (!di_buf) {
		PR_ERR("%s:no di_buf\n", __func__);
		return;
	}

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

	qidle	= &ds->lk_que_idle;
	qin	= &ds->lk_que_in;
	free_nub = qidle->ops.count(qidle);
	if (!free_nub || qin->ops.is_full(qin)) {
		PR_ERR("%s:queue full. free_nub:%d\n", __func__, free_nub);
		return;
	}
	is_eos = di_buf->is_eos;
	mvf = (struct vframe_s *)qidle->ops.get(qidle);
	if (!mvf || !mvf->private_data) {
		PR_ERR("%s:qout:%d:0x%px\n",
			__func__,
			qidle->ops.count(qidle),
			mvf);
		err_cnt++;
		return;
	}

	dbg_check_vf(ds, mvf, 1);
	/* clear */
	buffer_l = &itf->buf_bf[mvf->index];
	buffer_l->flag		= 0;
	buffer_l->crcout	= 0;
	buffer_l->nrcrcout	= 0;

	ndvfm = (struct dimn_dvfm_s *)mvf->private_data;
	memset(&ndvfm->c, 0, sizeof(ndvfm->c));
	ndvfm->c.cnt_in = itf->c.sum_pre_get;
	ndvfm->c.di_buf = di_buf;
	ndvfm->c.di_buf->caller_mng = di_buf->caller_mng;
	ndvfm->c.ori_in = dpvpp_get_bypass_post_frame(itf->bind_ch, di_buf);
	if (ndvfm->c.ori_in) {
		is_bypass = true;
		if (itf->etype == EDIM_NIN_TYPE_VFM) {
			vf = (struct vframe_s *)ndvfm->c.ori_in;
		} else {
			buffer_ori = (struct di_buffer *)ndvfm->c.ori_in;
			vf = buffer_ori->vf;
		}
	} else {
		vf = di_buf->vframe;
	}

	dbg_plink2("%s:nvfm:0x%px di_buf:%px mvf:%px vf:%px ori_in:%px\n",
		__func__, ndvfm, di_buf, mvf, vf, ndvfm->c.ori_in);
	dbg_plink2("%s:di_buf->di_buf:<%px %px> dup_p:<%px %px> nb:%d eos:%d vf:%px adr:%lx\n",
		__func__, di_buf->di_buf[0], di_buf->di_buf[1],
		di_buf->di_buf_dup_p[0], di_buf->di_buf_dup_p[1],
		di_buf->is_nbypass, di_buf->is_eos,
		di_buf->vframe, di_buf->vframe->canvas0_config[0].phy_addr);

	itf->c.sum_pre_get++;
	if (!vf && !is_eos) {
		PR_ERR("%s:no vf:0x%px\n", __func__, di_buf);
		qidle->ops.put(qidle, mvf);
		return;
	}

	if (is_eos) {
		buffer_l->flag |= DI_FLAG_BUF_BY_PASS;
		ndvfm->c.in_dvfm.vfs.type |= VIDTYPE_V4L_EOS;//23-02-15
	} else {
		//ndvfm->c.ori_vf = vf;
		if (!is_bypass && di_buf->di_buf_dup_p[1]) {
			dpvpp_post_fill_outvf(vf,
				di_buf->di_buf_dup_p[1], EDPST_OUT_MODE_DEF);
			ds->bypass_cnt = 0;
		} else {
			if (ds->bypass_cnt == 0)
				dbg_plink1("%s:bypass:%d di_buf_dup_p[1]:%px\n", __func__,
					is_bypass, di_buf->di_buf_dup_p[1]);
			ds->bypass_cnt++;
		}
		memcpy(&ndvfm->c.vf_in_cp, vf, sizeof(ndvfm->c.vf_in_cp));
		dim_dvf_cp(&ndvfm->c.in_dvfm, vf, 0);
		if (IS_COMP_MODE(ndvfm->c.in_dvfm.vfs.type)) {
			ndvfm->c.in_dvfm.vfs.width = vf->compWidth;
			ndvfm->c.in_dvfm.vfs.height = vf->compHeight;
		}
		ndvfm->c.src_w = ndvfm->c.in_dvfm.vfs.width;
		ndvfm->c.src_h = ndvfm->c.in_dvfm.vfs.height;
		//buffer_l->phy_addr	= vf->canvas0_config[0].phy_addr;//tmp
		dim_print("%s:0x%lx\n", __func__,
			  (unsigned long)vf->canvas0_config[0].phy_addr);
	}
	flg_q = qin->ops.put(qin, mvf);

	if (!flg_q) {
		PR_ERR("%s:qin\n", __func__);
		err_cnt++;
		//in_vf_put(itf, vf);
		/* how to return di_buffer ?*/
		qidle->ops.put(qidle, mvf);
		return;
	}
	task_send_wk_timer(EDIM_WKUP_REASON_IN_HAVE);
}

static bool dpvpp_fill_post_frame(struct dimn_itf_s *itf)
{
	struct dimn_qs_cls_s *qin;
	struct vframe_s *vfm;
	struct dimn_dvfm_s *ndvfm = NULL;
	struct dvfm_s *in_dvfm, *out_dvfm;
	unsigned int bypass;
	struct dim_cvspara_s *cvsp;
	unsigned int chg;
	struct dim_pvpp_ds_s *ds;
	struct dim_pvpp_hw_s *hw;
	struct dvfm_s *dvfm_dm;
	unsigned int out_fmt;
	struct pvpp_dis_para_in_s *dis_para_demo;
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
	in_dvfm = &ndvfm->c.in_dvfm;

	bypass = dpvpp_is_bypass_dvfm_postlink(in_dvfm);
	if (!ndvfm->c.di_buf)
		bypass = EPVPP_BYPASS_REASON_NO_LOCAL_BUF;
	ndvfm->c.bypass_reason = bypass;
	if (bypass) {
		if (!itf->c.bypass_reason)
			PR_INF("to bypass:%d:%d\n", bypass, ndvfm->c.cnt_in);
		itf->c.bypass_reason = bypass;
		dpvpp_post_bypass(itf, 1);
		return true;
	}

	if (itf->c.bypass_reason)
		dbg_plink1("leave bypass:%d\n", ndvfm->c.cnt_in);
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
		PR_INF("postlink:source change:ch[%d]:id[%d]:0x%x/%d/%d/%d=>0x%x/%d/%d/%d:%d\n",
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
			       ndvfm->c.cnt_in);
		/* reset last_ds_ratio when source size change */
		itf->c.last_ds_ratio = 0;
		ds->set_cfg_cur.b.en_linear_cp = hw->en_linear;
		ds->is_inp_4k = false;
		ds->is_out_4k = false;
		//ds->o_c.d32 = ds->o_hd.d32;

		dvfm_dm	= &ds->out_dvfm_demo;
		out_fmt = 0;
		dbg_plink1("%s:id[%d]:out_fmt[%d]\n", __func__, itf->id, out_fmt);

		memcpy(dvfm_dm,
			   in_dvfm, sizeof(*in_dvfm));
		if (chg != 2)
			dim_dvf_type_p_change(dvfm_dm, out_fmt);
		dvfm_dm->flg_afbce_set = false;
		if (itf->etype == EDIM_NIN_TYPE_VFM)
			dvfm_dm->is_itf_vfm = 1;
		dvfm_dm->is_prvpp_link	= 1;

		ds->set_cfg_cur.b.en_wr_afbce	= false;
		ds->set_cfg_cur.b.en_wr_mif	= false; /*tmp*/
		ds->afbc_sgn_cfg.b.mem		= false;
		ds->afbc_sgn_cfg.b.enc_nr	= false;
		ds->set_cfg_cur.b.en_mem_mif = false;
		ds->set_cfg_cur.b.en_mem_afbcd = false;
		if (ds->set_cfg_cur.b.en_wr_mif && !hw->en_linear) {
			ds->set_cfg_cur.b.en_wr_cvs	= true;
			ds->set_cfg_cur.b.en_mem_cvs	= true;
		} else {
			ds->set_cfg_cur.b.en_wr_cvs	= false;
			ds->set_cfg_cur.b.en_mem_cvs	= false;
		}

		/* cfg out dvfm demo */
		ds->out_dvfm_demo.vfs.height = in_dvfm->vfs.height;
		ds->out_dvfm_demo.vfs.width	= in_dvfm->vfs.width;
		if (itf->etype == EDIM_NIN_TYPE_INS)
			ds->out_dvfm_demo.is_itf_ins_local = true;
		else
			ds->out_dvfm_demo.is_itf_ins_local = false;

		/* dis_para_demo */
		dis_para_demo = &ds->dis_para_demo;
		memset(dis_para_demo, 0, sizeof(*dis_para_demo));
		dis_para_demo->dmode = EPVPP_DISPLAY_MODE_DI;
		dis_para_demo->win.x_size = in_dvfm->vfs.width;
		dis_para_demo->win.y_size = in_dvfm->vfs.height;
		dis_para_demo->win.x_end = dis_para_demo->win.x_size - 1;
		dis_para_demo->win.y_end = dis_para_demo->win.y_size - 1;

		/* afbc */
		ndvfm->c.afbc_sgn_cfg.d8 = ds->afbc_sgn_cfg.d8;
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

		ds->out_dvfm_demo.vfs.di_flag &=
			~(DI_FLAG_DI_PVPPLINK | DI_FLAG_DI_PSTVPPLINK |
			  DI_FLAG_DI_LOCAL_BUF | DI_FLAG_DI_PVPPLINK_BYPASS);
		ds->out_dvfm_demo.vfs.di_flag |= (DI_FLAG_DI_PSTVPPLINK | DI_FLAG_DI_LOCAL_BUF);
		if (cfgg(LINEAR))
			ds->out_dvfm_demo.is_linear = true;
		else
			ds->out_dvfm_demo.is_linear = false;
		//set last information:
		dpvpp_set_type_smp(&ds->in_last, in_dvfm);
		ds->cnt_parser = 0;
	} else {
		memcpy(&ds->out_dvfm_demo.vfs,
	       &in_dvfm->vfs, sizeof(struct dsub_vf_s));
		if (ds->out_dvfm_demo.is_in_linear)
			ds->out_dvfm_demo.vfs.flag |= VFRAME_FLAG_VIDEO_LINEAR;
		else
			ds->out_dvfm_demo.vfs.flag &= (~VFRAME_FLAG_VIDEO_LINEAR);

		ds->out_dvfm_demo.vfs.di_flag &=
			~(DI_FLAG_DI_PVPPLINK | DI_FLAG_DI_PSTVPPLINK |
			  DI_FLAG_DI_LOCAL_BUF | DI_FLAG_DI_PVPPLINK_BYPASS);
		ds->out_dvfm_demo.vfs.di_flag |= (DI_FLAG_DI_PSTVPPLINK | DI_FLAG_DI_LOCAL_BUF);
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
	out_dvfm->caller_mng = ndvfm->c.di_buf->caller_mng;

	if (ndvfm->c.set_cfg.b.en_in_cvs) {
		/* config cvs for input */
		cvsp = &ndvfm->c.cvspara_in;
		cvsp->plane_nub = in_dvfm->vfs.plane_num;
		cvsp->cvs_cfg = &in_dvfm->vfs.canvas0_config[0];
	}
	ndvfm->c.sum_reg_cnt = itf->sum_reg_cnt;
	ndvfm->c.cnt_parser = ds->cnt_parser;
	ds->cnt_parser++;

	/* get */
	vfm = (struct vframe_s *)qin->ops.get(qin);
	post_vtype_fill_d(itf, vfm, &ndvfm->c.vf_in_cp, &ndvfm->c.out_dvfm);
	dim_print("%s:link\n", __func__);
	didbg_vframe_out_save(itf->bind_ch, vfm, 6);
	if (itf->etype == EDIM_NIN_TYPE_INS) {
		buffer = &itf->buf_bf[vfm->index];
		buffer->caller_mng = out_dvfm->caller_mng;
	}
	dpvpp_put_ready_vf(itf, ds, vfm);
	return true;
}

static bool dpvpp_post_parser_frame(struct dimn_itf_s *itf,
		struct di_buf_s *di_buf)
{
	if (!itf || !itf->c.reg_di ||
	    !itf->ds || itf->c.pause_parser) {
		PR_ERR("%s: should not be here 1\n", __func__);
		return false;
	}
	if (itf->c.src_state & EDVPP_SRC_NEED_MSK_CRT) {
		PR_ERR("%s: should not be here 2\n", __func__);
		return false;
	}

	dpvpp_post_feed_buffer(itf, di_buf);

	if ((itf->c.src_state & EDVPP_SRC_NEED_MSK_BYPASS) ||
	   dpvpp_dbg_force_bypass_2()) {
		if (itf->c.src_state & EDVPP_SRC_NEED_MSK_BYPASS)
			itf->c.bypass_reason = EPVPP_NO_MEM;
		dpvpp_post_bypass(itf, 0);
		dpvpp_recycle_back(itf);
		dim_print("%s:SRC_NEED_MSK_BYPASS:0x%x\n", __func__, itf->c.src_state);
		return true;
	}

	dpvpp_recycle_back(itf);
	dpvpp_fill_post_frame(itf);
	if (di_buf && !itf->c.bypass_reason)
		dpvpp_post_queue_to_display(itf, di_buf);
	return true;
}

bool dpvpp_post_process(void *para)
{
	struct dimn_itf_s *itf;
	int ch;
	unsigned int cnt_active = 0;
	unsigned int freeze = 0;
	struct di_buf_s *di_buf = (struct di_buf_s *)para;
	bool put_done = false;

	for (ch = 0; ch < DI_PLINK_CN_NUB; ch++) {
		itf = &get_datal()->dvs_pvpp.itf[ch];
		if (itf->link_mode == EPVPP_API_MODE_PRE)
			continue;

		dpvpp_post_reg_ch(itf);
		if (di_buf &&
		    di_buf->channel == itf->bind_ch) {
			put_done = dpvpp_post_parser_frame(itf, di_buf);
		}
		if (!itf->ds ||
		    !atomic_read(&itf->reg) ||
		    atomic_read(&itf->in_unreg))
			continue;
		if (!di_buf)
			dpvpp_recycle_back(itf);
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

	if (di_buf && !put_done)
		PR_ERR("%s: di_buf is not used %px\n", __func__, di_buf);
	if (dpvpp_is_not_active_di())
		dpvpp_link_sw_by_di(false);
	else if (!freeze && cnt_active)
		dpvpp_link_sw_by_di(true);
	else if (!cnt_active)
		dpvpp_link_sw_by_di(false);
	dpvpp_reg_postlink_sw(true);
	return put_done;
}

int dpvpp_post_unreg_bypass(void)
{
	struct dim_pvpp_hw_s *hw;

	hw = &get_datal()->dvs_pvpp.hw;
	if (hw->dis_last_para.dmode != EPVPP_DISPLAY_MODE_BYPASS) {
		dbg_plink1("%s:trig dis_ch:%d->255\n", __func__, hw->dis_ch);
		hw->dis_ch = 0xff;
		hw->last_dct_bypass = true;
		//post_close_new();
		dimh_disable_post_deinterlace_2(true);
		if (hw->op)
			hw->op->wr(DI_POST_CTRL, 0);
	}
	hw->dis_last_para.dmode = EPVPP_DISPLAY_MODE_BYPASS;
	hw->dis_last_dvf = NULL;
	atomic_set(&hw->link_instance_id, DIM_PLINK_INSTANCE_ID_INVALID);
	dbg_plink1("%s:end\n", "unreg_bypass");
	return EPVPP_DISPLAY_MODE_BYPASS;
}

static int dpvpp_post_process_update(struct dimn_itf_s *itf, struct di_buf_s *di_buf,
		struct pvpp_dis_para_in_s *para,
		bool flush_all, bool new_frame,
		const struct reg_acc *op)
{
	struct di_buf_s *di_pldn_buf = NULL;
	unsigned int di_width, di_height, di_start_x, di_end_x;
	unsigned int di_start_y, di_end_y, hold_line;
	unsigned int post_blend_en = 0, post_blend_mode = 0;
	unsigned int blend_mtn_en = 0, ei_en = 0, post_field_num = 0;
	unsigned char mc_pre_flag = 0;
	bool invert_mv = false;
	struct di_hpst_s *pst = get_hw_pst();
	unsigned char channel;
	struct di_pre_stru_s *ppre = NULL;
	struct di_post_stru_s *ppost = NULL;
	struct di_cvs_s *cvss;
	union hw_sc2_ctr_pst_s *sc2_post_cfg, *sc2_post_cfg_set;
	struct di_ch_s *pch;
	bool cur_overturn;
	unsigned int cur_pst_size;
	unsigned int x_size_ori;

	dimp_inc(edi_mp_post_cnt);
	dim_mp_update_post();

	if (IS_ERR_OR_NULL(itf) ||
		IS_ERR_OR_NULL(op) ||
		IS_ERR_OR_NULL(para) ||
	    IS_ERR_OR_NULL(di_buf) ||
	    IS_ERR_OR_NULL(di_buf->vframe) ||
	    IS_ERR_OR_NULL(di_buf->di_buf_dup_p[0]))
		return 0;

	channel = itf->bind_ch;
	ppre = get_pre_stru(channel);
	ppost = get_post_stru(channel);
	pch = get_chdata(channel);
	cvss = &get_datal()->cvs;
	hold_line = dimp_get(edi_mp_post_hold_line);
	di_pldn_buf = di_buf->di_buf_dup_p[pldn_dly];
	cur_overturn = dim_get_overturn();

	if (flush_all)
		ppost->update_post_reg_flag = 1;

	if (new_frame)
		ppost->toggle_flag = true;
	if (ppost->toggle_flag && di_buf->di_buf_dup_p[1])
		top_bot_config(di_buf->di_buf_dup_p[1]);
	ppost->toggle_flag = false;

	ppost->cur_disp_index = di_buf->index;
	if (!get_init_flag(channel)) {
		PR_ERR("%s 2:\n", __func__);
		return 0;
	}

	if (ppost->frame_cnt == 1 && di_get_kpi_frame_num() > 0)
		pr_dbg("[di_kpi] di:ch[%d]:queue 1st frame to post ready.\n",
		       channel);

	dim_secure_sw_post(channel);
	dim_ddbg_mod_save(EDI_DBG_MOD_POST_SETB, channel, ppost->frame_cnt);
	/* make sure the height is even number */
	di_start_x = para->win.x_st;
	di_width = para->win.x_size;
	di_end_x = di_width + di_start_x - 1;
	di_start_y = para->win.y_st;
	di_height = para->win.y_size;
	di_end_y = di_height + di_start_y - 1;
	if (di_height & 1)
		di_height++;

	if (is_mask(SC2_POST_TRIG))
		pst->last_pst_size  = 0;

	if (di_buf->trig_post_update) {
		pst->last_pst_size = 0;
		ppost->seq = 0;
	}

	di_buf->seq = ppost->seq;
	cur_pst_size = (di_width - 1) | ((di_height - 1) << 16);
	if (flush_all || pst->last_pst_size != cur_pst_size ||
	    ppost->buf_type != di_buf->di_buf_dup_p[0]->type ||
	    ppost->di_buf0_mif.luma_x_start0 != di_start_x ||
	    (ppost->di_buf0_mif.luma_y_start0 != di_start_y / 2)) {
		dim_ddbg_mod_save(EDI_DBG_MOD_POST_RESIZE, channel, ppost->frame_cnt);/*dbg*/
		ppost->buf_type = di_buf->di_buf_dup_p[0]->type;

		dimh_initial_di_post_2(di_width, di_height, hold_line, 0);

		if (!di_buf->di_buf_dup_p[0]->vframe) {
			PR_ERR("%s 3:\n", __func__);
			return 0;
		}

		if (DIM_IS_IC_EF(SC2)) {
			sc2_post_cfg = &ppost->pst_top_cfg;
			sc2_post_cfg_set = &pst->pst_top_cfg;
			sc2_post_cfg->b.afbc_en = 1;
			sc2_post_cfg->b.mif_en = 1;
			sc2_post_cfg->b.is_4k = 0;
			sc2_post_cfg->b.post_frm_sel = 0;
			sc2_post_cfg->b.afbc_if0 = 0;
			sc2_post_cfg->b.afbc_if1 = 0;
			sc2_post_cfg->b.afbc_if2 = 0;
			sc2_post_cfg->b.afbc_wr = 0;
			sc2_post_cfg->b.is_if0_4k = 0;
			sc2_post_cfg->b.is_if1_4k = 0;
			sc2_post_cfg->b.is_if2_4k = 0;
			if (sc2_post_cfg_set->d32 != sc2_post_cfg->d32) {
				sc2_post_cfg_set->d32 = sc2_post_cfg->d32;
				dim_sc2_contr_pst(sc2_post_cfg_set, NULL);
			}

			if (cfgg(LINEAR)) {
				ppost->di_buf0_mif.linear = 1;
				ppost->di_buf0_mif.buf_crop_en = 1;
				ppost->di_buf0_mif.buf_hsize =
					di_buf->di_buf_dup_p[0]->buf_hsize;
				ppost->di_mtnprd_mif.linear = 1;
				ppost->di_mcvecrd_mif.linear = 1;
			}
		} else {
			op->bwr(DI_AFBCE_CTRL, 0, 3, 1);
		}
		pst->last_pst_size = cur_pst_size;

		/******************************************/
		/* bit mode config */
		if (di_buf->vframe->bitdepth & BITDEPTH_Y10) {
			u8 bit_mode;
			u32 bitdepth = di_buf->vframe->bitdepth;

			if (di_buf->vframe->type & VIDTYPE_VIU_444)
				bit_mode = (bitdepth & FULL_PACK_422_MODE) ? 3 : 2;
			else
				bit_mode = (bitdepth & FULL_PACK_422_MODE) ? 3 : 1;
			ppost->di_buf0_mif.bit_mode = bit_mode;
		} else {
			ppost->di_buf0_mif.bit_mode = 0;
		}
		if (di_buf->vframe->type & VIDTYPE_VIU_444) {
			if (DIM_IS_IC_EF(SC2))
				ppost->di_buf0_mif.video_mode = 2;
			else
				ppost->di_buf0_mif.video_mode = 1;
		} else if (di_buf->vframe->type & VIDTYPE_VIU_422) {
			if (DIM_IS_IC_EF(SC2)) {
				if (opl1()->info.main_version >= 3)
					ppost->di_buf0_mif.video_mode = 1;
			} else {
				ppost->di_buf0_mif.video_mode = 0;
			}
		} else {
			ppost->di_buf0_mif.video_mode = 0;
		}
		if (ppost->buf_type == VFRAME_TYPE_IN &&
		    !(di_buf->di_buf_dup_p[0]->vframe->type &
		      VIDTYPE_VIU_FIELD)) {
			if ((di_buf->vframe->type & VIDTYPE_VIU_NV21) ||
			    (di_buf->vframe->type & VIDTYPE_VIU_NV12))
				ppost->di_buf0_mif.set_separate_en = 1;
			else
				ppost->di_buf0_mif.set_separate_en = 0;
			ppost->di_buf0_mif.luma_y_start0 = di_start_y;
			ppost->di_buf0_mif.luma_y_end0 = di_end_y;
		} else { /* from vdin or local vframe process by di pre */
			ppost->di_buf0_mif.set_separate_en = 0;
			ppost->di_buf0_mif.luma_y_start0 = di_start_y >> 1;
			ppost->di_buf0_mif.luma_y_end0 = di_end_y >> 1;
		}
		ppost->di_buf0_mif.luma_x_start0 = di_start_x;
		ppost->di_buf0_mif.luma_x_end0 = di_end_x;
		ppost->di_buf0_mif.reg_swap	= 1;
		ppost->di_buf0_mif.l_endian	= 0;
		ppost->di_buf0_mif.cbcr_swap	= 0;
#ifdef CONFIG_AMLOGIC_MEDIA_THERMAL1
		if (DIM_IS_IC_TXHD2)
			ppost->di_buf0_mif.bit8_flag = di_buf->bit_8_flag;
#endif
		memcpy(&ppost->di_buf1_mif, &ppost->di_buf0_mif, sizeof(struct DI_MIF_S));
		/* recovery the attr */
		ppost->di_buf1_mif.mif_index = DI_MIF0_ID_IF1;
		memcpy(&ppost->di_buf2_mif, &ppost->di_buf0_mif, sizeof(struct DI_MIF_S));
		/* recovery the attr */
		ppost->di_buf2_mif.mif_index = DI_MIF0_ID_IF2;

		ppost->di_mtnprd_mif.start_x = di_start_x;
		ppost->di_mtnprd_mif.end_x = di_end_x;
		ppost->di_mtnprd_mif.start_y = di_start_y >> 1;
		ppost->di_mtnprd_mif.end_y = di_end_y >> 1;
		ppost->di_mtnprd_mif.per_bits	= 4;
		if (dimp_get(edi_mp_mcpre_en)) {
			unsigned int mv_offset;

			ppost->di_mcvecrd_mif.start_x = di_start_x / 5;
			mv_offset = (di_start_x % 5) ? (5 - di_start_x % 5) : 0;
			ppost->di_mcvecrd_mif.vecrd_offset =
				cur_overturn ? (di_end_x + 1) % 5 : mv_offset;
			ppost->di_mcvecrd_mif.start_y =
				(di_start_y >> 1);
			ppost->di_mcvecrd_mif.size_x =
				(di_end_x + 1 + 4) / 5 - 1 - di_start_x / 5;
			ppost->di_mcvecrd_mif.end_y =
				(di_end_y >> 1);
			ppost->di_mcvecrd_mif.per_bits = 16;
		}
		ppost->update_post_reg_flag = 1;
		/* if height decrease, mtn will not enough */
		if (di_buf->pd_config.global_mode != PULL_DOWN_BUF1)
			di_buf->pd_config.global_mode = PULL_DOWN_EI;
	}

	x_size_ori = di_buf->di_buf_dup_p[1]->vframe->width;
	ppost->di_mtnprd_mif.buf_hsize = x_size_ori;
	ppost->di_mcvecrd_mif.buf_hsize = (x_size_ori + 4) / 5;
	ppost->canvas_id = ppost->next_canvas_id;
	dimp_set(edi_mp_post_blend, di_buf->pd_config.global_mode);
	dim_print("%s:ch[%d]\n", __func__, channel);
	switch (dimp_get(edi_mp_post_blend)) {
	case PULL_DOWN_BLEND_0:
	case PULL_DOWN_NORMAL:
		config_canvas_idx(di_buf->di_buf_dup_p[1],
			cvss->post_idx[ppost->canvas_id][0], -1);
		config_canvas_idx(di_buf->di_buf_dup_p[2], -1,
			cvss->post_idx[ppost->canvas_id][2]);
		config_canvas_idx(di_buf->di_buf_dup_p[0],
			cvss->post_idx[ppost->canvas_id][1], -1);
		config_canvas_idx(di_buf->di_buf_dup_p[2],
			cvss->post_idx[ppost->canvas_id][3], -1);
		if (dimp_get(edi_mp_mcpre_en))
			config_mcvec_canvas_idx(di_buf->di_buf_dup_p[2],
				cvss->post_idx[ppost->canvas_id][4]);
		break;
	case PULL_DOWN_BLEND_2:
	case PULL_DOWN_NORMAL_2:
		config_canvas_idx(di_buf->di_buf_dup_p[0],
			cvss->post_idx[ppost->canvas_id][3], -1);
		config_canvas_idx(di_buf->di_buf_dup_p[1],
			cvss->post_idx[ppost->canvas_id][0], -1);
		config_canvas_idx(di_buf->di_buf_dup_p[2], -1,
			cvss->post_idx[ppost->canvas_id][2]);
		config_canvas_idx(di_buf->di_buf_dup_p[2],
			cvss->post_idx[ppost->canvas_id][1], -1);
		if (dimp_get(edi_mp_mcpre_en))
			config_mcvec_canvas_idx(di_buf->di_buf_dup_p[2],
				cvss->post_idx[ppost->canvas_id][4]);
		break;
	case PULL_DOWN_MTN:
		config_canvas_idx(di_buf->di_buf_dup_p[1],
			cvss->post_idx[ppost->canvas_id][0], -1);
		config_canvas_idx(di_buf->di_buf_dup_p[2], -1,
			cvss->post_idx[ppost->canvas_id][2]);
		config_canvas_idx(di_buf->di_buf_dup_p[0],
			cvss->post_idx[ppost->canvas_id][1], -1);
		break;
	case PULL_DOWN_BUF1:/* wave with buf1 */
		config_canvas_idx(di_buf->di_buf_dup_p[1],
			cvss->post_idx[ppost->canvas_id][0], -1);
		config_canvas_idx(di_buf->di_buf_dup_p[1], -1,
			cvss->post_idx[ppost->canvas_id][2]);
		config_canvas_idx(di_buf->di_buf_dup_p[0],
			cvss->post_idx[ppost->canvas_id][1], -1);
		break;
	case PULL_DOWN_EI:
		if (di_buf->di_buf_dup_p[1])
			config_canvas_idx(di_buf->di_buf_dup_p[1],
				cvss->post_idx[ppost->canvas_id][0], -1);
		break;
	default:
		break;
	}

	ppost->next_canvas_id = ppost->canvas_id ? 0 : 1;

	if (!di_buf->di_buf_dup_p[1]) {
		PR_ERR("%s 4:\n", __func__);
		return 0;
	}
	if (!di_buf->di_buf_dup_p[1]->vframe ||
	    !di_buf->di_buf_dup_p[0]->vframe) {
		PR_ERR("%s 5:\n", __func__);
		return 0;
	}

	switch (dimp_get(edi_mp_post_blend)) {
	case PULL_DOWN_BLEND_0:
	case PULL_DOWN_NORMAL:
		post_field_num =
			(di_buf->di_buf_dup_p[1]->vframe->type &
			 VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_TOP ? 0 : 1;
		ppost->di_buf0_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[1]->nr_canvas_idx;
		ppost->di_buf1_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[0]->nr_canvas_idx;
		ppost->di_buf2_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[2]->nr_canvas_idx;
		//t7
		ppost->di_buf0_mif.addr0 =
			di_buf->di_buf_dup_p[1]->nr_adr;
		ppost->di_buf1_mif.addr0 =
			di_buf->di_buf_dup_p[0]->nr_adr;
		ppost->di_buf2_mif.addr0 =
			di_buf->di_buf_dup_p[2]->nr_adr;

		ppost->di_mtnprd_mif.canvas_num =
			di_buf->di_buf_dup_p[2]->mtn_canvas_idx;
		ppost->di_mtnprd_mif.addr =
			di_buf->di_buf_dup_p[2]->mtn_adr;
		mc_pre_flag = cur_overturn ? 0 : 1;
		if (di_buf->pd_config.global_mode == PULL_DOWN_NORMAL) {
			post_blend_mode = 3;
			/*if pulldown, mcdi_mcpreflag is 1,*/
			/*it means use previous field for MC*/
			/*else not pulldown,mcdi_mcpreflag is 2*/
			/*it means use forward & previous field for MC*/
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXHD))
				mc_pre_flag = 2;
		} else {
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXHD))
				mc_pre_flag = 1;
			post_blend_mode = 1;
		}

		if (dimp_get(edi_mp_mcpre_en)) {
			ppost->di_mcvecrd_mif.canvas_num =
				di_buf->di_buf_dup_p[2]->mcvec_canvas_idx;
			ppost->di_mcvecrd_mif.addr =
				di_buf->di_buf_dup_p[2]->mcvec_adr;
		}
		blend_mtn_en = 1;
		ei_en = 1;
		dimp_set(edi_mp_post_ei, 1);
		post_blend_en = 1;
		break;
	case PULL_DOWN_BLEND_2:
	case PULL_DOWN_NORMAL_2:
		post_field_num =
			(di_buf->di_buf_dup_p[1]->vframe->type &
			 VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_TOP ? 0 : 1;
		ppost->di_buf0_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[1]->nr_canvas_idx;
		ppost->di_buf1_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[2]->nr_canvas_idx;
		ppost->di_buf2_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[0]->nr_canvas_idx;

		ppost->di_buf0_mif.addr0 =
			di_buf->di_buf_dup_p[1]->nr_adr;
		ppost->di_buf1_mif.addr0 =
			di_buf->di_buf_dup_p[2]->nr_adr;
		ppost->di_buf2_mif.addr0 =
			di_buf->di_buf_dup_p[0]->nr_adr;

		ppost->di_mtnprd_mif.canvas_num =
			di_buf->di_buf_dup_p[2]->mtn_canvas_idx;
		ppost->di_mtnprd_mif.addr =
			di_buf->di_buf_dup_p[2]->mtn_adr;

		if (dimp_get(edi_mp_mcpre_en)) {
			ppost->di_mcvecrd_mif.canvas_num =
				di_buf->di_buf_dup_p[2]->mcvec_canvas_idx;
			ppost->di_mcvecrd_mif.addr =
				di_buf->di_buf_dup_p[2]->mcvec_adr;
			mc_pre_flag = is_meson_txl_cpu() ? 0 :
				(cur_overturn ? 1 : 0);
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX)) {
				invert_mv = true;
			} else if (!cur_overturn) {
				ppost->di_buf2_mif.canvas0_addr0 =
				di_buf->di_buf_dup_p[2]->nr_canvas_idx;
			}
		}
		if (di_buf->pd_config.global_mode == PULL_DOWN_NORMAL_2) {
			post_blend_mode = 3;
			/*if pulldown, mcdi_mcpreflag is 1,*/
			/*it means use previous field for MC*/
			/*else not pulldown,mcdi_mcpreflag is 2*/
			/*it means use forward & previous field for MC*/
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXHD))
				mc_pre_flag = 2;
		} else {
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXHD))
				mc_pre_flag = 1;
			post_blend_mode = 1;
		}
		blend_mtn_en = 1;
		ei_en = 1;
		dimp_set(edi_mp_post_ei, 1);
		post_blend_en = 1;
		break;
	case PULL_DOWN_MTN:
		post_field_num =
			(di_buf->di_buf_dup_p[1]->vframe->type &
			 VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_TOP ? 0 : 1;
		ppost->di_buf0_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[1]->nr_canvas_idx;
		ppost->di_buf1_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[0]->nr_canvas_idx;
		//t7
		ppost->di_buf0_mif.addr0 =
			di_buf->di_buf_dup_p[1]->nr_adr;
		ppost->di_buf1_mif.addr0 =
			di_buf->di_buf_dup_p[0]->nr_adr;
		ppost->di_mtnprd_mif.canvas_num =
			di_buf->di_buf_dup_p[2]->mtn_canvas_idx;
		ppost->di_mtnprd_mif.addr =
			di_buf->di_buf_dup_p[2]->mtn_adr;
		post_blend_mode = 0;
		blend_mtn_en = 1;
		ei_en = 1;
		dimp_set(edi_mp_post_ei, 1);
		post_blend_en = 1;
		break;
	case PULL_DOWN_BUF1:
		post_field_num =
			(di_buf->di_buf_dup_p[1]->vframe->type &
			 VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_TOP ? 0 : 1;
		ppost->di_buf0_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[1]->nr_canvas_idx;
		//t7:
		ppost->di_buf0_mif.addr0 =
			di_buf->di_buf_dup_p[1]->nr_adr;
		ppost->di_mtnprd_mif.canvas_num =
			di_buf->di_buf_dup_p[1]->mtn_canvas_idx;
		ppost->di_mtnprd_mif.addr =
			di_buf->di_buf_dup_p[1]->mtn_adr;
		ppost->di_buf1_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[0]->nr_canvas_idx;
		ppost->di_buf1_mif.addr0 =
			di_buf->di_buf_dup_p[0]->nr_adr;
		post_blend_mode = 1;
		blend_mtn_en = 0;
		ei_en = 0;
		dimp_set(edi_mp_post_ei, 0);
		post_blend_en = 0;
		break;
	case PULL_DOWN_EI:
		if (di_buf->di_buf_dup_p[1]) {
			ppost->di_buf0_mif.canvas0_addr0 =
				di_buf->di_buf_dup_p[1]->nr_canvas_idx;
			ppost->di_buf0_mif.addr0 =
				di_buf->di_buf_dup_p[1]->nr_adr;
			post_field_num =
				(di_buf->di_buf_dup_p[1]->vframe->type &
				 VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_TOP ? 0 : 1;
		} else {
			post_field_num =
				(di_buf->di_buf_dup_p[0]->vframe->type &
				 VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_TOP ? 0 : 1;
			ppost->di_buf0_mif.src_field_mode =
				post_field_num;
		}
		post_blend_mode = 2;
		blend_mtn_en = 0;
		ei_en = 1;
		dimp_set(edi_mp_post_ei, 1);
		post_blend_en = 0;
		break;
	default:
		break;
	}
	ppost->di_diwr_mif.is_dw = 0;

	/**************************************************/
	/* if post size < MIN_POST_WIDTH, force ei */
	if (di_width < MIN_BLEND_WIDTH &&
	    (di_buf->pd_config.global_mode == PULL_DOWN_BLEND_0 ||
	     di_buf->pd_config.global_mode == PULL_DOWN_BLEND_2 ||
	     di_buf->pd_config.global_mode == PULL_DOWN_NORMAL)) {
		post_blend_mode = 1;
		blend_mtn_en = 0;
		ei_en = 0;
		dimp_set(edi_mp_post_ei, 0);
		post_blend_en = 0;
	}

	if (dimp_get(edi_mp_mcpre_en))
		ppost->di_mcvecrd_mif.blend_en = post_blend_en;

	invert_mv = cur_overturn ? (!invert_mv) : invert_mv;

	if (ppost->update_post_reg_flag) {
		dimh_enable_di_post_2
			(&ppost->di_buf0_mif,
			 &ppost->di_buf1_mif,
			 &ppost->di_buf2_mif,
			 &ppost->di_diwr_mif,
			 &ppost->di_mtnprd_mif,
			 ei_en,            /* ei en*/
			 post_blend_en,    /* blend en */
			 blend_mtn_en,     /* blend mtn en */
			 post_blend_mode,  /* blend mode. */
			 1,	/* di_vpp_en. */
			 0,	/* di_ddr_en. */
			 post_field_num,/* 1bottom gen top */
			 hold_line,
			 dimp_get(edi_mp_post_urgent),
			 (invert_mv ? 1 : 0),
			 0);
		if (dimp_get(edi_mp_mcpre_en)) {
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
				dimh_en_mc_di_post_g12(&ppost->di_mcvecrd_mif,
					dimp_get(edi_mp_post_urgent),
					cur_overturn, (invert_mv ? 1 : 0));
		} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXLX)) {
			DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL, 0, 0, 2);
		}
		dpvpph_post_frame_reset_g12(true, op);
	} else {
		dimh_post_switch_buffer
			(&ppost->di_buf0_mif,
			 &ppost->di_buf1_mif,
			 &ppost->di_buf2_mif,
			 &ppost->di_diwr_mif,
			 &ppost->di_mtnprd_mif,
			 &ppost->di_mcvecrd_mif,
			 ei_en, /* ei enable */
			 post_blend_en, /* blend enable */
			 blend_mtn_en,  /* blend mtn enable */
			 post_blend_mode,  /* blend mode. */
			 1,  /* di_vpp_en. */
			 0,	/* di_ddr_en. */
			 post_field_num,/*1bottom gen top */
			 hold_line,
			 dimp_get(edi_mp_post_urgent),
			 (invert_mv ? 1 : 0),
			 dimp_get(edi_mp_pulldown_enable),
			 dimp_get(edi_mp_mcpre_en),
			 0);
	}
	if (DIM_IS_IC(T5DB) ||
	    DIM_IS_IC_EF(SC2)) {
#ifdef HIS_CODE
		if (di_cfg_top_get(EDI_CFG_REF_2)	&&
		    mc_pre_flag				&&
		    dimp_get(edi_mp_post_wr_en)) { /*OTT-3210*/
			dbg_once("mc_old=%d\n", mc_pre_flag);
			mc_pre_flag = 1;
		}
#endif
		dim_post_read_reverse_irq(cur_overturn, mc_pre_flag,
			post_blend_en ? dimp_get(edi_mp_mcpre_en) : false);
		/* disable mc for first 2 fieldes mv unreliable */
		if (di_buf->seq < 2)
			DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL, 0, 0, 2);
	}
	if (dimp_get(edi_mp_mcpre_en) && di_buf->di_buf_dup_p[2])
		set_post_mcinfo(&di_buf->di_buf_dup_p[2]->curr_field_mcinfo);

	/* set pull down region (f(t-1) */
	if (di_pldn_buf && dimp_get(edi_mp_pulldown_enable) &&
	    !ppre->cur_prog_flag) {
		unsigned short offset = (di_start_y >> 1);

		if (cur_overturn)
			offset = ((di_buf->vframe->height - di_end_y) >> 1);
		else
			offset = 0;
		/*pulldown_vof_win_vshift*/
		get_ops_pd()->vof_win_vshift(&di_pldn_buf->pd_config, offset);
		dimh_pulldown_vof_win_config(&di_pldn_buf->pd_config);
	}
	if (DIM_IS_IC_EF(SC2))
		opl1()->pst_mif_sw(true, DI_MIF0_SEL_PST_ALL);
	else
		post_mif_sw(true);

	/*ary add for post crash*/
	if (DIM_IS_IC_EF(SC2))
		opl1()->pst_set_flow(0, EDI_POST_FLOW_STEP2_START, op);
	else
		di_post_set_flow(0, EDI_POST_FLOW_STEP2_START);
	if (ppost->update_post_reg_flag > 0)
		ppost->update_post_reg_flag--;

	di_buf->seq_post = ppost->seq;
	pst->flg_have_set = true;
	/*dbg*/
	dim_ddbg_mod_save(EDI_DBG_MOD_POST_SETE, channel, ppost->frame_cnt);
	if (new_frame) {
		ppost->frame_cnt++;
		ppost->seq++;
		pch->sum_pst++;
	}
	return 0;
}

int dpvpp_post_display(struct vframe_s *vfm,
			 struct pvpp_dis_para_in_s *in_para,
			 void *out_para)
{
	struct dim_pvpp_ds_s *ds;
	struct dimn_itf_s *itf;
	struct dimn_dvfm_s *ndvfm = NULL;
	unsigned int diff;/*enum EDIM_DVPP_DIFF*/
	struct pvpp_dis_para_in_s *para;
	bool dbg_tm = false;
	unsigned long delay;
	struct dim_pvpp_hw_s *hw;
	u32 bypass_reason = 0;
	bool new_frame = false;

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
	if (!ds || !vfm || !in_para) {
		PR_ERR("%s:di not reg\n", __func__);
		if (!ds)
			PR_ERR("no ds\n");
		if (!vfm)
			PR_ERR("no vfm");
		if (!in_para)
			PR_ERR("no in_para");
		return EPVPP_ERROR_DI_NOT_REG;
	}

	atomic_add(DI_BIT0, &itf->c.dbg_display_sts);	/* dbg only */
	if (!atomic_read(&hw->link_sts)) {
		bypass_reason = 1;
		goto DISPLAY_BYPASS;
	}

	if (itf->c.src_state &
	    (EDVPP_SRC_NEED_MSK_CRT | EDVPP_SRC_NEED_MSK_BYPASS)) {
		bypass_reason = 2;
		bypass_reason |= (itf->c.src_state << 16);
		goto DISPLAY_BYPASS;
	}

	atomic_add(DI_BIT1, &itf->c.dbg_display_sts);	/* dbg only */
	/* check if bypass */
	ndvfm = dpvpp_check_dvfm_act(itf, vfm);
	if (!ndvfm || !ndvfm->c.di_buf) {
		bypass_reason = 3;
		goto DISPLAY_BYPASS;
	}
	atomic_add(DI_BIT2, &itf->c.dbg_display_sts);	/* dbg only */
	dbg_check_ud(ds, 2); /* dbg */

	para = in_para;
	if (para->dmode == EPVPP_DISPLAY_MODE_BYPASS) {
		bypass_reason = 4;
		goto DISPLAY_BYPASS;
	}
	if (dpvpp_bypass_display()) { //dbg only
		bypass_reason = 5;
		goto DISPLAY_BYPASS;
	}

	diff = check_diff(itf, ndvfm, para);
	ndvfm->c.cnt_display = ds->cnt_display;
	if (hw->dis_last_dvf != ndvfm)
		new_frame = true;
	if ((diff & 0xfff0) &&
	    (diff & 0xfff0)  != (ds->dbg_last_diff & 0xfff0)) {
		dbg_mem("diff:0x%x->0x%x\n", ds->dbg_last_diff, diff);
		atomic_add(DI_BIT3, &itf->c.dbg_display_sts);	/* dbg only */
		ds->dbg_last_diff = diff;
	}
	dbg_plink3("diff:0x%x\n", diff);
	ndvfm->c.sts_diff = diff;//dbg only;
	if (diff & EDIM_DVPP_DIFF_ALL) {
		dim_print("%s:vfm:0x%px, in_para:0x%px, o:0x%px\n",
		__func__, vfm, in_para, out_para);
		hw->reg_cnt = itf->sum_reg_cnt;
		PR_INF("%s:ch[%d] connected\n", __func__, itf->bind_ch);
		ds->cnt_display = 0;
		ndvfm->c.cnt_display = ds->cnt_display;
		dpvpp_post_process_update(itf,
			ndvfm->c.di_buf, &hw->dis_c_para,
			true, new_frame, hw->op);
		//set last:?
		memcpy(&hw->dis_last_para,
		       &hw->dis_c_para, sizeof(hw->dis_last_para));
		hw->id_l = hw->id_c;
		dim_print("display:set end:%d\n", ds->cnt_display);
		hw->dis_last_dvf = ndvfm;
		ds->cnt_display++;
		atomic_add(DI_BIT4, &itf->c.dbg_display_sts);	/* dbg only */
	} else {
		ndvfm->c.cnt_display = ds->cnt_display;
		if (diff & EDIM_DVPP_DIFF_SAME_FRAME) {
			/* nothing ? */
			/* dbg only */
			atomic_add(DI_BIT5, &itf->c.dbg_display_sts);
		} else if (diff & EDIM_DVPP_DIFF_ONLY_ADDR) {
			dpvpp_post_process_update(itf,
				ndvfm->c.di_buf, &hw->dis_c_para,
				false, new_frame, hw->op);
			dim_print("display:up addr end:%d\n", ds->cnt_display);
			//set last:
			ds->cnt_display++;
			hw->dis_last_dvf = ndvfm;
			/* dbg only */
			atomic_add(DI_BIT6, &itf->c.dbg_display_sts);
		} else {
			PR_INF("?other?\n");
			/* dbg only */
			atomic_add(DI_BIT7, &itf->c.dbg_display_sts);
		}
	}
	ds->field_count_for_cont++;
	atomic_set(&hw->link_instance_id, itf->sum_reg_cnt);
	return EPVPP_DISPLAY_MODE_DI;

DISPLAY_BYPASS:
	if (hw->dis_last_para.dmode != EPVPP_DISPLAY_MODE_BYPASS) {
		hw->dis_ch = 0xff;
		hw->last_dct_bypass = true;
		//post_close_new();
		dimh_disable_post_deinterlace_2(true);
		if (hw->op)
			hw->op->wr(DI_POST_CTRL, 0);
		atomic_add(DI_BIT8, &itf->c.dbg_display_sts);	/* dbg only */
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

