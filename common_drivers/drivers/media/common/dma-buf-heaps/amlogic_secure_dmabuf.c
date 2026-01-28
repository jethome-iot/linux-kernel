// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/dma-heap.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/scatterlist.h>
#include <linux/sched/signal.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/codec_mm/dmabuf_manage.h>
#include <linux/amlogic/media/dmabuf_heaps/amlogic_dmabuf_heap.h>

static struct dma_heap *secure_heap;

static int dma_buf_debug = 1;
module_param(dma_buf_debug, int, 0644);

#define dprintk(level, fmt, arg...) \
	do { \
		if (dma_buf_debug >= (level)) \
			pr_info("DMA-BUF:%s: " fmt,  __func__, ## arg);	\
	} while (0)

#define pr_dbg(fmt, args ...)		dprintk(6, fmt, ## args)
#define pr_error(fmt, args ...)		dprintk(1, fmt, ## args)
#define pr_inf(fmt, args ...)		dprintk(8, fmt, ## args)
#define pr_enter()					pr_inf("enter")


#define AML_SECURE_DMABUF_HEAP_SETTING_PHYADDR		3

struct secure_block_info {
	__u32 version;
	__u32 size;
	__u32 handle;
	__u32 unused;
	__u32 state;
	__u32 id_high;
	__u32 id_low;
	__u32 paddr;
	__u32 psize;
	__u64 handle_extend;
	__u64 paddr_extend;
};

struct secure_heap_buffer {
	struct mutex lock;
	struct sg_table sg_table;
	struct secure_block_info *block;
	struct page *block_page;
};

static int secure_heap_attach(struct dma_buf *dmabuf,
				struct dma_buf_attachment *attachment)
{
	pr_enter();
	attachment->priv = dmabuf->priv;
	return 0;
}

static void secure_heap_detach(struct dma_buf *dmabuf,
				struct dma_buf_attachment *attachment)
{
	pr_enter();
	attachment->priv = NULL;
}

static struct sg_table *secure_heap_map_dma_buf
					(struct dma_buf_attachment *attachment,
					enum dma_data_direction direction)
{
	struct secure_heap_buffer *buffer = attachment->priv;

	return &buffer->sg_table;
}

static void secure_heap_unmap_dma_buf(struct dma_buf_attachment *attachment,
					struct sg_table *table,
					enum dma_data_direction direction)
{
	pr_enter();
	return;
}

static int secure_heap_dma_buf_begin_cpu_access
						(struct dma_buf *dmabuf,
					enum dma_data_direction direction)
{
	pr_enter();
	return 0;
}

static int secure_heap_dma_buf_end_cpu_access(struct dma_buf *dmabuf,
						enum dma_data_direction direction)
{
	pr_enter();
	return 0;
}

static int secure_heap_mmap(struct dma_buf *dmabuf,
						struct vm_area_struct *vma)
{
	struct secure_heap_buffer *buffer = dmabuf->priv;
	struct secure_block_info *block = NULL;
	int ret = -1;

	pr_enter();
	if (!buffer)
		goto out;

	mutex_lock(&buffer->lock);
	if (!buffer->block)
		goto out_unlock;

	block = buffer->block;
	if (block->version >= SECURE_HEAP_USER_TA_VERSION) {
		switch (block->state) {
		case AML_SECURE_DMABUF_HEAP_SETTING_PHYADDR:
			if (block->version > SECURE_HEAP_USER_TA_VERSION) {
				sg_set_page(buffer->sg_table.sgl,
					pfn_to_page(PFN_DOWN(block->paddr_extend)),
					PAGE_ALIGN(block->psize), 0);
				buffer->sg_table.sgl->dma_address = block->paddr_extend;
#ifdef CONFIG_NEED_SG_DMA_LENGTH
				buffer->sg_table.sgl->dma_length = PAGE_ALIGN(block->size);
#else
				buffer->sg_table.sgl->length = PAGE_ALIGN(block->size);
#endif
				block->paddr_extend = 0;
				block->psize = 0;
				block->state = 0;
			} else {
				sg_set_page(buffer->sg_table.sgl,
					pfn_to_page(PFN_DOWN(block->paddr)),
					PAGE_ALIGN(block->psize), 0);
				buffer->sg_table.sgl->dma_address = block->paddr;
#ifdef CONFIG_NEED_SG_DMA_LENGTH
				buffer->sg_table.sgl->dma_length = PAGE_ALIGN(block->size);
#else
				buffer->sg_table.sgl->length = PAGE_ALIGN(block->size);
#endif
				block->paddr = 0;
				block->psize = 0;
				block->state = 0;
			}
		default:
			break;
		}
	}

	ret = remap_pfn_range(vma, vma->vm_start, page_to_pfn(buffer->block_page),
			PAGE_SIZE, vma->vm_page_prot);
out_unlock:
	mutex_unlock(&buffer->lock);
out:
	return ret;
}

static int secure_heap_vmap(struct dma_buf *dmabuf, struct dma_buf_map *vaddr)
{
	pr_enter();
	return 0;
}

static void secure_heap_vunmap(struct dma_buf *dmabuf, struct dma_buf_map *vaddr)
{
	pr_enter();
}

static void secure_heap_dma_buf_release(struct dma_buf *dmabuf)
{
	struct secure_heap_buffer *buffer = dmabuf->priv;
	struct secure_block_info *block = NULL;
	u64 handle = 0;

	pr_enter();
	if (!buffer)
		return;

	pr_dbg("<%s %d> thread (%s, %d) %d\n", __func__, __LINE__,
		current->comm, current->pid, current->tgid);
	mutex_lock(&buffer->lock);
	block = buffer->block;
	if (block) {
		if (block->version > SECURE_HEAP_USER_TA_VERSION)
			handle = block->handle_extend;
		else
			handle = block->handle;

		if (dmabuf_manage_secure_block_free(block->id_high, block->id_low,
			handle, block->size, block->version))
			pr_error("Secure vdec block free error please fix it");
	}
	sg_free_table(&buffer->sg_table);
	if (buffer->block_page)
		free_reserved_page(buffer->block_page);
	else if (buffer->block)
		free_pages((unsigned long)buffer->block, 0);

	mutex_unlock(&buffer->lock);
	kfree(buffer);
}

static const struct dma_buf_ops secure_heap_buf_ops = {
	.attach = secure_heap_attach,
	.detach = secure_heap_detach,
	.map_dma_buf = secure_heap_map_dma_buf,
	.unmap_dma_buf = secure_heap_unmap_dma_buf,
	.begin_cpu_access = secure_heap_dma_buf_begin_cpu_access,
	.end_cpu_access = secure_heap_dma_buf_end_cpu_access,
	.mmap = secure_heap_mmap,
	.vmap = secure_heap_vmap,
	.vunmap = secure_heap_vunmap,
	.release = secure_heap_dma_buf_release,
};

static struct dma_buf *secure_heap_do_allocate(struct dma_heap *heap,
						unsigned long len,
						unsigned long fd_flags,
						unsigned long heap_flags,
						bool uncached)
{
	struct secure_heap_buffer *buffer = NULL;
	struct secure_block_info *block = NULL;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	struct dma_buf *dmabuf = NULL;
	struct sg_table *table = NULL;
	int ret = -ENOMEM;

	pr_dbg("<%s %d> thread (%s, %d %d) len %d\n", __func__, __LINE__,
		current->comm, current->pid, current->tgid, (u32)len);

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return ERR_PTR(-ENOMEM);

	mutex_init(&buffer->lock);

	block = (struct secure_block_info *)get_zeroed_page(GFP_KERNEL);
	if (!block)
		goto free_buffer;

	buffer->block_page = pfn_to_page(PFN_DOWN(virt_to_phys(block)));
	if (buffer->block_page)
		mark_page_reserved(buffer->block_page);

	table = &buffer->sg_table;
	if (sg_alloc_table(table, 1, GFP_KERNEL))
		goto free_priv_buffer;

	block->version = dmabuf_manage_get_secure_heap_version();
	block->id_high = current->tgid;
	block->id_low = current->pid;

	if (dmabuf_manage_secure_pool_create(block->id_high, block->id_low, len,
		block->version))
		goto free_tables;

	if (block->version > SECURE_HEAP_USER_TA_VERSION) {
		block->handle_extend = dmabuf_manage_secure_block_alloc(block->id_high,
			block->id_low, len, block->version);
		if (!block->handle_extend)
			goto free_tables;
	} else {
		block->handle = (u32)dmabuf_manage_secure_block_alloc(block->id_high,
			block->id_low, len, block->version);
		if (!block->handle)
			goto free_tables;
	}

	block->size = len;
	/* create the dmabuf */
	exp_info.exp_name = dma_heap_get_name(heap);
	exp_info.ops = &secure_heap_buf_ops;
	exp_info.size = len;
	exp_info.flags = fd_flags;
	exp_info.priv = buffer;

	dmabuf = dma_buf_export(&exp_info);
	if (IS_ERR(dmabuf)) {
		ret = PTR_ERR(dmabuf);
		goto free_tables;
	}

	buffer->block = block;
	return dmabuf;

free_tables:
	sg_free_table(table);
free_priv_buffer:
	free_page((unsigned long)block);
free_buffer:
	kfree(buffer);
	return ERR_PTR(ret);
}

static struct dma_buf *secure_uncached_heap_allocate
						(struct dma_heap *heap,
						unsigned long len,
						unsigned long fd_flags,
						unsigned long heap_flags)
{
	pr_enter();
	return secure_heap_do_allocate(heap, len, fd_flags, heap_flags, true);
}

static struct dma_buf *secure_uncached_heap_not_initialized
						(struct dma_heap *heap,
						unsigned long len,
						unsigned long fd_flags,
						unsigned long heap_flags)
{
	pr_enter();
	return ERR_PTR(-EBUSY);
}

static struct dma_heap_ops secure_heap_ops = {
	.allocate = secure_uncached_heap_not_initialized,
};

int __init amlogic_system_secure_dma_buf_init(void)
{
	struct dma_heap_export_info exp_info;

	pr_enter();
	exp_info.name = SYSTEM_SECURE_UNCACHE_HEAP_NAME;
	exp_info.ops = &secure_heap_ops;
	exp_info.priv = NULL;

	secure_heap = dma_heap_add(&exp_info);
	if (IS_ERR(secure_heap))
		return PTR_ERR(secure_heap);

	secure_heap_ops.allocate = secure_uncached_heap_allocate;
	return 0;
}

MODULE_LICENSE("GPL v2");
