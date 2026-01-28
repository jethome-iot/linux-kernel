// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
#include <linux/amlogic/media/rdma/rdma_mgr.h>
#include <common/rdma/rdma.h>
#endif
#include "meson_drv.h"
#include "meson_vpu.h"
#include "meson_vpu_util.h"
#include "meson_vpu_pipeline.h"

u32 s5_reg_table_cached[VPU_CACHED_REG_TABLE_SIZE] = {
	/*mif*/
	VIU_OSD1_CTRL_STAT_S5,
	VIU_OSD2_CTRL_STAT_S5,
	VIU_OSD3_CTRL_STAT_S5,
	VIU_OSD4_CTRL_STAT_S5,
	/*afbc*/
	OSD_PATH_MISC_CTRL,
	/*osdblend*/
	VIU_OSD_BLEND_CTRL_S5,
};

u32 s7_reg_table_cached[VPU_CACHED_REG_TABLE_SIZE] = {
	/* mif */
	VIU_OSD1_CTRL_STAT,
	VIU_OSD2_CTRL_STAT,
	VIU_OSD3_CTRL_STAT,
	VIU_OSD4_CTRL_STAT,
	/*afbc*/
	OSD_PATH_MISC_CTRL,
	/* osdblend */
	VIU_OSD_BLEND_CTRL,
};

u32 reg_table_cached[VPU_CACHED_REG_TABLE_SIZE];
struct meson_drm_rdma_table rdma_tbl[MESON_MAX_CRTC];

void update_cached_reg_table(u32 *table)
{
	if (!table)
		return;

	memcpy(reg_table_cached, table, sizeof(reg_table_cached));
}

u32 meson_drm_read_rdma_table_reg(int index, int handle, u32 adr)
{
	u32 read_val;
	int i;
	bool found = false;

	for (i = 0; i < rdma_tbl[index].rdma_item_count; i++)
		if (rdma_tbl[index].rdma_item[i << 1] == adr) {
			read_val = rdma_tbl[index].rdma_item[(i << 1) + 1];
			found = true;
			break;
		}

	if (!found)
		read_val = aml_read_vcbus(adr);

	return read_val;
}

int meson_drm_write_rdma_table_reg(int index, int handle, u32 adr, u32 val)
{
	u32 rdma_item_count = rdma_tbl[index].rdma_item_count;
	int i;

	if (rdma_tbl[index].rdma_item_count < RDMA_TABLE_ITEM_TOTAL) {
		rdma_tbl[index].rdma_item[rdma_item_count << 1] = adr;
		rdma_tbl[index].rdma_item[(rdma_item_count << 1) + 1] = val;
		rdma_tbl[index].rdma_item_count++;
		MESON_DRM_REG("handle%d adr:0x%04x val:0x%x rdma_item_count=%d\n",
			handle, adr, val, rdma_tbl[index].rdma_item_count);
	} else {
		DRM_ERROR("vpp%d(handle:%d adr:%d val:%d) overflow, rdma_item_count=%d\n",
			index, handle, adr, val, rdma_tbl[index].rdma_item_count);

		for (i = 0; i < rdma_tbl[index].rdma_item_count; i++)
			aml_write_vcbus(rdma_tbl[index].rdma_item[rdma_item_count << 1],
				rdma_tbl[index].rdma_item[(rdma_item_count << 1) + 1]);
		rdma_tbl[index].rdma_item_count = 0;
		rdma_item_count = rdma_tbl[index].rdma_item_count;
		//ins->rdma_write_count = 0;
		rdma_tbl[index].rdma_item[rdma_item_count << 1] = adr;
		rdma_tbl[index].rdma_item[(rdma_item_count << 1) + 1] = val;
		rdma_tbl[index].rdma_item_count++;
	}

	return 0;
}

int meson_drm_write_rdma_table_reg_bits(int index, int handle,
	u32 adr, u32 val, u32 start, u32 len)
{
	u32 write_val, temp;
	int i, read_from = 0;
	bool found = false, match_wr = false;
	u32 read_val = aml_read_vcbus(adr);

	for (i = rdma_tbl[index].rdma_item_count - 1; i >= 0 ; i--)
		if (rdma_tbl[index].rdma_item[i << 1] == adr) {
			read_val = rdma_tbl[index].rdma_item[(i << 1) + 1];
			found = true;
			read_from = 1;
			break;
		}

	if (!found) {
		for (i = 0; i < VPU_CACHED_REG_TABLE_SIZE ; i++) {
			if (!reg_table_cached[i])
				break;

			if (reg_table_cached[i] == adr) {
				temp = rdma_read_reg_write_table(handle, adr, &match_wr);
				if (match_wr) {
					read_from = 2;
					read_val = temp;
					break;
				}
			}
		}
	}

	write_val = (read_val & ~(((1L << (len)) - 1) << (start))) |
			((unsigned int)(val) << (start));
	MESON_DRM_REG("handle %d, adr:0x%04x rd=0x%08x wr=0x%08x from%d\n",
		handle, adr, read_val, write_val, read_from);

	if (found) {
		rdma_tbl[index].rdma_item[(i << 1) + 1] = write_val;
		return 0;
	}

	return meson_drm_write_rdma_table_reg(index, handle, adr, write_val);
}

int meson_drm_write_allregs2rdma(int index)
{
	if (!rdma_tbl[index].flag)
		return 0;

	if (rdma_update_reg_buf(rdma_tbl[index].handle, rdma_tbl[index].rdma_item,
		rdma_tbl[index].rdma_item_count)) {
		DRM_ERROR("update rdma reg buf failed!");
		return -1;
	}

	MESON_DRM_REG("vpp%d total registers:%d\n", index, rdma_tbl[index].rdma_item_count);
	memset(rdma_tbl[index].rdma_item, 0, MESON_VPU_RDMA_TABLE_SIZE);
	rdma_tbl[index].rdma_item_count = 0;

	return 0;
}

void meson_drm_rdma_init(struct meson_drm *private, int idx, int handle)
{
	rdma_tbl[idx].rdma_item = vmalloc(MESON_VPU_RDMA_TABLE_SIZE);
	if (!rdma_tbl[idx].rdma_item) {
		DRM_ERROR("crtc%d rdma alloc table failed!\n", idx);
		return;
	}

	rdma_tbl[idx].handle = handle;
	if (private->creat_rdma_table)
		rdma_tbl[idx].flag = true;

	update_cached_reg_table(private->vpu_data->cached_regs);
}

