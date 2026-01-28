// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
// drivers/amlogic/media/enhancement/amvecm/am_lut3d.c

/* #include <mach/am_regs.h> */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#include <linux/uaccess.h>
#include "arch/vpp_regs.h"
#include "arch/cm_regs.h"
#include "arch/ve_regs.h"
#include <linux/amlogic/media/amvecm/color_tune.h>
#include "blue_stretch/blue_str.h"
#include "amve.h"

unsigned int base_linear;/*1:base linear, 0:base customer*/
module_param(base_linear, int, 0664);
MODULE_PARM_DESC(base_linear, "base_linear");

extern int (*plut)[3];
extern unsigned int (*plut_out)[3];

void bs_ct_tbl(void)
{
	int i, j;
	int lut3d_single_sz;

	if (chip_type_id == chip_t6d)
		lut3d_single_sz = LUT3D_SINGLE_SIZE9;
	else
		lut3d_single_sz = LUT3D_SINGLE_SIZE;

	if (!plut)
		return;

	for (i = 0; i < lut3d_single_sz; i++) {
		for (j = 0; j < 3; j++) {
			if (bs_3dlut_en && base_linear && plut3d)
				plut[i][j] = plut3d[i * 3 + j];
			else if (plut3d_base)
				plut[i][j] = plut3d_base[i * 3 + j];
		}
	}
}

/* color tune and blue stretch set */
void lut3d_set_api(int vpp_index)
{
	struct ct_func_s *ct_f = get_ct_func();

	if (base_linear)
		bls_set();

	if (!ct_f->cl_par->en || !ct_f->ct) {
		pr_info("%s: ct_en = %d, ct = %p\n", __func__, ct_f->cl_par->en, ct_f->ct);
		bs_ct_tbl();
		lut3d_update((unsigned int (*)[3]) plut, vpp_index);
	} else {
		pr_info("%s: ct_en = %d, ct = %p\n", __func__, ct_f->cl_par->en, ct_f->ct);
		bs_ct_tbl();
		ct_process();
		lut3d_update(plut_out, vpp_index);
	}
}

#endif
