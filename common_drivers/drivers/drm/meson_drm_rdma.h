/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_DRM_RDMA_H__
#define __MESON_DRM_RDMA_H__

#define VPU_CACHED_REG_TABLE_SIZE     32

struct meson_drm_rdma_table {
	u32 *rdma_item;
	bool flag;
	u32 rdma_item_count;
	u32 handle;
};

void update_cached_reg_table(u32 *table);
u32 meson_drm_read_rdma_table_reg(int index, int handle, u32 adr);
int meson_drm_write_rdma_table_reg(int index, int handle, u32 adr, u32 val);
int meson_drm_write_rdma_table_reg_bits(int index, int handle,
	u32 adr, u32 val, u32 start, u32 len);
int meson_drm_write_allregs2rdma(int index);
void meson_drm_rdma_init(struct meson_drm *private, int idx, int handle);

#endif
