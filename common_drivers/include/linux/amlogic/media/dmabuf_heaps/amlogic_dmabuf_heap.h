/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef AMLOGIC_DMABUF_HEAP_H
#define AMLOGIC_DMABUF_HEAP_H

#include <linux/miscdevice.h>
#include <linux/dma-heap.h>

#define DMABUF_FLAG_EXTEND_CACHED                  BIT(29)
#define DMABUF_FLAG_EXTEND_MESON_HEAP              BIT(30)
#define DMABUF_FLAG_EXTEND_PROTECTED               BIT(31)

#define CODECMM_HEAP_NAME			"heap-codecmm"
#define CODECMM_SECURE_HEAP_NAME		"heap-secure-codecmm"
#define CODECMM_CACHED_HEAP_NAME		"heap-cached-codecmm"
#define SYSTEM_SECURE_UNCACHE_HEAP_NAME		"system-secure-uncached"

/* open dmaheap /sys/class node */
#define DMAHEAP_CLASS 0

struct dma_heap_device {
	struct miscdevice dev;
	struct dentry *debug_root;
	u64 total_heap_gfx_size;
	u64 total_heap_fb_size;
	u64 total_heap_secure_size;
	u64 heap_gfx_num_of_buffers;
	u64 heap_gfx_num_of_alloc_bytes;
	u64 heap_gfx_num_of_free_bytes;
	u64 heap_fb_num_of_buffers;
	u64 heap_fb_num_of_alloc_bytes;
	u64 heap_fb_num_of_free_bytes;
	u64 heap_secure_num_of_buffers;
	u64 heap_secure_num_of_alloc_bytes;
	u64 heap_secure_num_of_free_bytes;
};

struct meson_cma_heap_info {
	char heap_name[32];
	u64 num_of_buffers;
	u64 num_of_alloc_bytes;
	u64 num_of_free_bytes;
};

union meson_cma_heap_ioctl_arg {
	struct meson_cma_heap_info info_data;
};

struct codec_mm_heap_buffer {
	struct dma_heap *heap;
	struct list_head attachments;
	//lock for buffer access
	struct mutex lock;
	unsigned long len;
	struct sg_table sg_table;
	int vmap_cnt;
	void *vaddr;
	//struct deferred_freelist_item deferred_free;
	bool uncached;
	unsigned long heap_flags;
	void *priv;
};

#define MESON_CMA_HEAP_IOC_MAGIC 'M'
#define MESON_CMA_HEAP_IOC_GET_INFO _IOWR(MESON_CMA_HEAP_IOC_MAGIC, 0, \
				struct meson_cma_heap_info)

bool dmabuf_is_codec_mm_heap_buf(struct dma_buf *dmabuf);

#endif /**/

