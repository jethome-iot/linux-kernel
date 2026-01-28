// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/sched/clock.h>
#include <linux/mm.h>
#include <linux/seq_file.h>
#include <linux/cma.h>
#include <cma.h>
#include <trace/events/kmem.h>

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/hugetlb.h>
#include <linux/mman.h>
#include <linux/mmzone.h>
#include <linux/proc_fs.h>
#include <linux/percpu.h>
#include <linux/seq_file.h>
#include <linux/swap.h>
#include <linux/vmstat.h>
#include <linux/atomic.h>
#include <linux/vmalloc.h>
#include <linux/sysrq.h>

#include <trace/hooks/mm.h>
#define KERNEL_ATRACE_TAG KERNEL_ATRACE_TAG_MEMORY
#include <trace/events/meson_atrace.h>

static unsigned long long_fault_thresh = 100000000; //100ms
module_param(long_fault_thresh, ulong, 0644);

static unsigned long long_fault_dump;
module_param(long_fault_dump, ulong, 0644);

static unsigned long long_alloc_thresh_ms = 200;
module_param(long_alloc_thresh_ms, ulong, 0644);

#ifdef CONFIG_AMLOGIC_CMA
static unsigned long long_cma_alloc_thresh = 1000000000; //1000ms
module_param(long_cma_alloc_thresh, ulong, 0644);

static unsigned long long_cma_alloc_dump;
module_param(long_cma_alloc_dump, ulong, 0644);
#endif

static int pte_fault_full_name;
module_param(pte_fault_full_name, int, 0644);

#define FILENAME_LEN 256

static const char *parse_name(struct vm_area_struct *vma, char *buf)
{
	struct file *file = vma->vm_file;

	if (!file)
		return "";

	if (pte_fault_full_name) {
		char *p = d_path(&file->f_path, buf, FILENAME_LEN);

		if (!IS_ERR(p)) {
			mangle_path(buf, p, "\n");
			return buf;
		}
	} else {
		int i;
		char *p = file->f_path.dentry->d_iname;

		for (i = 0; i < FILENAME_LEN; i++) {
			if (p[i] >= 33 && p[i] <= 126) {
				buf[i] = p[i];
			} else {
				buf[i] = '\0';
				break;
			}
		}
		return buf;
	}

	return "";
}

static int handle_pte_fault_begin(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	unsigned long long *start_time = (unsigned long long *)ri->data;
	char buf[512];
	char filename[FILENAME_LEN];
#ifdef CONFIG_ARM64
	struct vm_fault *vmf = (struct vm_fault *)regs->regs[0];
#endif
#ifdef CONFIG_ARM
	struct vm_fault *vmf = (struct vm_fault *)regs->ARM_r0;
#endif
	pte_t entry = vmf->orig_pte;

	if (!get_atrace_tag_enabled(KERNEL_ATRACE_TAG))
		return 0;

	*start_time = sched_clock();

	if (!vmf->pte) {
		if (vma_is_anonymous(vmf->vma)) {
			sprintf(buf, "pte_fault_anon:%lx", vmf->address);
			ATRACE_BEGIN_TASK(buf);
		} else  {
			snprintf(buf, sizeof(buf), "pte_fault_file:%s:%lx",
				parse_name(vmf->vma, filename), vmf->address);
			ATRACE_BEGIN_TASK(buf);
		}
		return 0;
	}

	if (!pte_present(vmf->orig_pte)) {
		sprintf(buf, "pte_fault_swap:%lx", vmf->address);
		ATRACE_BEGIN_TASK(buf);
		return 0;
	}

	if (vmf->flags & FAULT_FLAG_WRITE) {
		if (!pte_write(entry)) {
			sprintf(buf, "pte_fault_wp:%lx", vmf->address);
			ATRACE_BEGIN_TASK(buf);
			return 0;
		}
	}

	sprintf(buf, "pte_fault_other:%lx", vmf->address);
	ATRACE_BEGIN_TASK(buf);

	return 0;
}

static int handle_pte_fault_end(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	unsigned long long delta;
	char buf[256];

	if (!get_atrace_tag_enabled(KERNEL_ATRACE_TAG))
		return 0;

	delta = sched_clock() - *(unsigned long long *)ri->data;
	if (delta > long_fault_thresh) {
		pr_err("%s/%d long_fault:%lluns\n", current->comm, current->pid, delta);

		if (long_fault_dump)
			dump_stack();
	}

	sprintf(buf, "pte_fault_time:%llu", delta);
	ATRACE_END_NAME_TASK(buf);

	return 0;
}

static struct kretprobe krp_handle_pte_fault = {
	.kp.symbol_name		= "handle_pte_fault",
	.entry_handler		= handle_pte_fault_begin,
	.handler		= handle_pte_fault_end,
	.data_size		= sizeof(unsigned long long),
	.maxactive		= 100,
};

static void aml_android_vh_alloc_pages_entry(void *data, gfp_t *gfp, unsigned int order,
		int preferred_nid, nodemask_t *nodemask)
{
	char buf[256];

	if (!get_atrace_tag_enabled(KERNEL_ATRACE_TAG))
		return;

	sprintf(buf, "alloc_pages:order:%u,gfp:%x", order, *gfp);
	ATRACE_BEGIN_TASK(buf);
}

static void aml_mm_page_alloc(void *data, struct page *page, unsigned int order,
		gfp_t gfp_flags, int migratetype)
{
	char buf[256];

	if (!get_atrace_tag_enabled(KERNEL_ATRACE_TAG))
		return;

	sprintf(buf, "alloc_pages_end:order:%u,flags:%x,mtype:%d", order, gfp_flags, migratetype);
	ATRACE_END_NAME_TASK(buf);
}

static void aml_android_vh_alloc_pages_slowpath(void *data, gfp_t gfp_mask, unsigned int order,
		unsigned long start)
{
	unsigned long now = jiffies;

	if (time_after(now, start + msecs_to_jiffies(long_alloc_thresh_ms))) {
		char buf[256];

		sprintf(buf, "long_alloc_error:order:%d start:%lu now=%lu\n", order, start, now);
		ATRACE_BEGIN_TASK(buf);
		ATRACE_END_NAME_TASK(buf);

		pr_err("long_alloc_error:order:%d start:%lu now=%lu\n", order, start, now);
		handle_sysrq('m');
	}
}

#ifdef CONFIG_AMLOGIC_CMA
struct cma_alloc_data {
	unsigned long long start_time;
	unsigned long count;
};

static int cma_alloc_begin(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	struct cma_alloc_data *data = (struct cma_alloc_data *)ri->data;
	char buf[256];
#ifdef CONFIG_ARM64
	struct cma *cma = (struct cma *)regs->regs[0];
	unsigned long count = (unsigned long)regs->regs[1];
#endif
#ifdef CONFIG_ARM
	struct cma *cma = (struct cma *)regs->ARM_r0;
	unsigned long count = (unsigned long)regs->ARM_r1;
#endif

	if (!get_atrace_tag_enabled(KERNEL_ATRACE_TAG))
		return 0;

	data->start_time = sched_clock();
	data->count = count;

	sprintf(buf, "cma_alloc:%s,count:%lu", cma->name, count);
	ATRACE_BEGIN_TASK(buf);

	return 0;
}

static int cma_alloc_end(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	struct cma_alloc_data *data = (struct cma_alloc_data *)ri->data;
	unsigned long long delta;
	char buf[256];

	if (!get_atrace_tag_enabled(KERNEL_ATRACE_TAG))
		return 0;

	delta = sched_clock() - data->start_time;

	if (delta > long_cma_alloc_thresh) {
		pr_err("%s/%d long_cma_alloc:%lluns\n", current->comm, current->pid, delta);

		if (long_cma_alloc_dump)
			dump_stack();
	}

	sprintf(buf, "cma_alloc_time:%llu,count:%lu", delta, data->count);
	ATRACE_END_NAME_TASK(buf);

	return 0;
}

static struct kretprobe krp_cma_alloc = {
	.kp.symbol_name		= "cma_alloc",
	.entry_handler		= cma_alloc_begin,
	.handler		= cma_alloc_end,
	.data_size		= sizeof(struct cma_alloc_data),
	.maxactive		= 100,
};
#else
/* called in aml_cma.ko */
void cma_alloc_trace_start(char *name, unsigned long count)
{
	char buf[256];

	if (!get_atrace_tag_enabled(KERNEL_ATRACE_TAG))
		return;

	sprintf(buf, "cma_alloc:%s,count:%lu", name, count);
	ATRACE_BEGIN_TASK(buf);
}
EXPORT_SYMBOL(cma_alloc_trace_start);

void cma_alloc_trace_end(char *name, unsigned long count, struct page *page)
{
	char buf[256];

	if (!get_atrace_tag_enabled(KERNEL_ATRACE_TAG))
		return;

	sprintf(buf, "cma_alloc:%s,count:%lu,pfn:%lx",
		name, count, page ? page_to_pfn(page) : -1UL);
	ATRACE_END_NAME_TASK(buf);
}
EXPORT_SYMBOL(cma_alloc_trace_end);
#endif

static struct delayed_work trace_meminfo_work;
static int trace_meminfo_period = 8;
module_param(trace_meminfo_period, int, 0644);

static void trace_meminfo(struct work_struct *work)
{
	struct sysinfo i;
	long cached;
	long available;
	unsigned long pages[NR_LRU_LISTS];
	unsigned long sreclaimable, sunreclaim;
	int lru;

	if (!get_atrace_tag_enabled(KERNEL_ATRACE_TAG))
		return;

	si_meminfo(&i);
	si_swapinfo(&i);

	cached = global_node_page_state(NR_FILE_PAGES) - total_swapcache_pages() - i.bufferram;
	if (cached < 0)
		cached = 0;

	for (lru = LRU_BASE; lru < NR_LRU_LISTS; lru++)
		pages[lru] = global_node_page_state(NR_LRU_BASE + lru);

	available = si_mem_available();
	sreclaimable = global_node_page_state_pages(NR_SLAB_RECLAIMABLE_B);
	sunreclaim = global_node_page_state_pages(NR_SLAB_UNRECLAIMABLE_B);

	ATRACE_COUNTER("MemFree:	", i.freeram * 4);
	ATRACE_COUNTER("CmaFree:	", global_zone_page_state(NR_FREE_CMA_PAGES) * 4);
	ATRACE_COUNTER("RealFree:       ",
			(i.freeram - global_zone_page_state(NR_FREE_CMA_PAGES)) * 4);
	ATRACE_COUNTER("Buffers:	", i.bufferram * 4);
	ATRACE_COUNTER("Cached:	 ", cached * 4);
	ATRACE_COUNTER("Anon:	   ", pages[LRU_ACTIVE_ANON] + pages[LRU_INACTIVE_ANON]);
	ATRACE_COUNTER("Active(anon):   ", pages[LRU_ACTIVE_ANON] * 4);
	ATRACE_COUNTER("Inactive(anon): ", pages[LRU_INACTIVE_ANON] * 4);
	ATRACE_COUNTER("File:	   ", pages[LRU_ACTIVE_FILE] + pages[LRU_INACTIVE_FILE]);
	ATRACE_COUNTER("Active(file):   ", pages[LRU_ACTIVE_FILE] * 4);
	ATRACE_COUNTER("Inactive(file): ", pages[LRU_INACTIVE_FILE] * 4);
	ATRACE_COUNTER("Unevictable:    ", pages[LRU_UNEVICTABLE] * 4);
	ATRACE_COUNTER("Mlocked:	", global_zone_page_state(NR_MLOCK) * 4);

#ifdef CONFIG_HIGHMEM
	ATRACE_COUNTER("HighTotal:      ", i.totalhigh * 4);
	ATRACE_COUNTER("HighFree:       ", i.freehigh * 4);
	ATRACE_COUNTER("LowTotal:       ", (i.totalram - i.totalhigh) * 4);
	ATRACE_COUNTER("LowFree:	", (i.freeram - i.freehigh) * 4);
#endif

	ATRACE_COUNTER("SwapTotal:      ", i.totalswap * 4);
	ATRACE_COUNTER("SwapFree:       ", i.freeswap * 4);
	ATRACE_COUNTER("Dirty:	  ", global_node_page_state(NR_FILE_DIRTY) * 4);
	ATRACE_COUNTER("Writeback:      ", global_node_page_state(NR_WRITEBACK) * 4);
	ATRACE_COUNTER("AnonPages:      ", global_node_page_state(NR_ANON_MAPPED) * 4);
	ATRACE_COUNTER("Mapped:	 ", global_node_page_state(NR_FILE_MAPPED) * 4);
	ATRACE_COUNTER("Shmem:	  ", i.sharedram * 4);
	ATRACE_COUNTER("KReclaimable:   ",
			(sreclaimable + global_node_page_state(NR_KERNEL_MISC_RECLAIMABLE)) * 4);
	ATRACE_COUNTER("Slab:	   ", (sreclaimable + sunreclaim) * 4);
	ATRACE_COUNTER("SReclaimable:   ", sreclaimable * 4);
	ATRACE_COUNTER("SUnreclaim:     ", sunreclaim * 4);
	ATRACE_COUNTER("KernelStack:    ", global_node_page_state(NR_KERNEL_STACK_KB) * 4);
#ifdef CONFIG_SHADOW_CALL_STACK
	ATRACE_COUNTER("ShadowCallStack:", global_node_page_state(NR_KERNEL_SCS_KB) * 4);
#endif
	ATRACE_COUNTER("PageTables:     ", global_node_page_state(NR_PAGETABLE) * 4);
	ATRACE_COUNTER("VmallocUsed:    ", vmalloc_nr_pages() * 4);
	ATRACE_COUNTER("Percpu:	 ", pcpu_nr_pages() * 4);

	schedule_delayed_work(&trace_meminfo_work, msecs_to_jiffies(trace_meminfo_period));
}

int atrace_mem_init(void)
{
	int ret;
	static int initialized;

	if (initialized)
		return 0;

	initialized = 1;

	ret = register_kretprobe(&krp_handle_pte_fault);
	if (ret < 0)
		pr_err("register_kretprobe:krp_handle_pte_fault failed, %d\n", ret);

	//__alloc_pages() begin
	ret = register_trace_android_vh_alloc_pages_entry(aml_android_vh_alloc_pages_entry, NULL);
	if (ret < 0)
		pr_err("register_trace_android_vh_alloc_pages_entry failed, %d\n", ret);

	//__alloc_pages() end
	ret = register_trace_mm_page_alloc(aml_mm_page_alloc, NULL);
	if (ret < 0)
		pr_err("register_trace_mm_page_alloc failed, %d\n", ret);

	ret = register_trace_android_vh_alloc_pages_slowpath(aml_android_vh_alloc_pages_slowpath,
			NULL);
	if (ret < 0)
		pr_err("register_trace_android_vh_alloc_pages_slowpath failed, %d\n", ret);

#ifdef CONFIG_AMLOGIC_CMA
	ret = register_kretprobe(&krp_cma_alloc);
	if (ret < 0)
		pr_err("register_kretprobe:krp_cma_alloc failed, %d\n", ret);
#endif

	INIT_DELAYED_WORK(&trace_meminfo_work, trace_meminfo);
	schedule_delayed_work(&trace_meminfo_work, msecs_to_jiffies(trace_meminfo_period));

	return 0;
}
