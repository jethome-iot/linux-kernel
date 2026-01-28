// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/cma.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/kthread.h>
#include <linux/syscalls.h>
#include <linux/cpumask.h>
#include <linux/printk.h>
#include <linux/hashtable.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>

#include "codec_mm_priv.h"

static int codec_prealloc_debug = 1;
module_param(codec_prealloc_debug, int, 0644);

#define dprintk(level, fmt, arg...)						\
	do {									\
		if (codec_prealloc_debug >= (level))						\
			pr_info("" fmt, ## arg);\
	} while (0)

#define pr_dbg(fmt, args ...)  dprintk(6, fmt, ## args)
#define pr_error(fmt, args ...) dprintk(1, fmt, ## args)
#define pr_inf(fmt, args ...) dprintk(8, fmt, ## args)

#define MB_ALIGN(size) (ALIGN(size, SZ_1M) >> 20)

static LIST_HEAD(prealloc_job_list);
static spinlock_t job_list_lock;   /* Protect job list */
static spinlock_t ht_list_lock;   /* Protect job list */

struct prealloc_job {
	struct list_head list;
	struct hlist_node hnode;
	struct task_struct *host;
	u32 type;
	u32 size;
	int align_2n;
	int memflags;
	struct completion done;
	struct codec_mm_s *mem;
};

struct prealloc_pcp {
	struct completion start;
	struct task_struct *task;
	int cpu;
	int ret;
};

#define ALLOC_PAGE_NUM_KTHREAD	3
struct prealloc_pcp prealloc_kthread_array[ALLOC_PAGE_NUM_KTHREAD];
DECLARE_HASHTABLE(ht, 7);

void submit_prealloc_job(u32 type, u32 num, u32 size, int align_2n, int memflags)
{
	u32 i, cpu;
	unsigned long flags;
	struct prealloc_pcp *work;
	int cpus = num_online_cpus() - 1;
	struct prealloc_job *job = NULL;

	if (!codec_mm_get_secure_mem_ctrl())
		return;

	if (num == 0 || size == 0)
		return;

	pr_dbg("submit job para type is %d num is %d, size is %d, align_2n is %d flags is %d\n",
		type, num, size, align_2n, memflags);

	if (align_2n < 0 || align_2n < PAGE_SHIFT)
		align_2n = PAGE_SHIFT;

	for (i = 0; i < num; i++) {
		job = kzalloc(sizeof(*job), GFP_KERNEL | __GFP_HIGH);
		job->type = type;
		job->size = size;
		job->align_2n = align_2n;
		job->host = current;
		job->memflags = memflags & (~CODEC_MM_FLAGS_FOR_TRY_PREALLOC);
		INIT_LIST_HEAD(&job->list);
		INIT_HLIST_NODE(&job->hnode);
		init_completion(&job->done);

		spin_lock(&job_list_lock);
		list_add_tail(&job->list, &prealloc_job_list);
		spin_unlock(&job_list_lock);

		spin_lock(&ht_list_lock);
		hash_add(ht, &job->hnode, MB_ALIGN(job->size));
		spin_unlock(&ht_list_lock);

		pr_dbg("submit job size is %d align_2n is %d flags is %d\n",
			job->size, job->align_2n, job->memflags);
	}

	i = 0;
	local_irq_save(flags);
	for (cpu = 0; cpu < ALLOC_PAGE_NUM_KTHREAD; cpu++) {
		work = &prealloc_kthread_array[cpu];
		complete(&work->start);
		i++;
		if (i == cpus) {
			pr_dbg("sched work to %d cpu\n", i);
			break;
		}
	}
	local_irq_restore(flags);
}
EXPORT_SYMBOL(submit_prealloc_job);

void release_prealloc_job(void)
{
	int key;
	struct prealloc_job *job, *tmp;
	struct hlist_node *hnode_tmp;
	struct codec_mm_s *mm = NULL;

	spin_lock(&job_list_lock);
	list_for_each_entry_safe(job, tmp, &prealloc_job_list, list) {
		list_del(&job->list);
		complete(&job->done);
	}
	spin_unlock(&job_list_lock);

	for (key = 0; key < HASH_SIZE(ht); key++) {
		hash_for_each_possible_safe(ht, job, hnode_tmp, hnode, key) {
			hash_del(&job->hnode);
			mm = job->mem;
			if (mm) {
				pr_dbg("prealloc free size is %d\n", mm->buffer_size);
				codec_mm_release(mm, "pre_mem");
			}
			kfree(job);
		}
	}
}
EXPORT_SYMBOL(release_prealloc_job);

static int prealloc_boost_work_func(void *prealloc_data)
{
	struct prealloc_pcp *c_work = (struct prealloc_pcp *)prealloc_data;
	struct prealloc_job *job;
	int ret = -1;
	char mem_name[32] = { 0 };

	for (;;) {
		ret = wait_for_completion_interruptible(&c_work->start);
		if (ret < 0) {
			pr_err("%s wait for task %d is %d\n",
			       __func__, c_work->cpu, ret);
			continue;
		}

		while (1) {
			spin_lock(&job_list_lock);
			if (list_empty(&prealloc_job_list)) {
				spin_unlock(&job_list_lock);
				pr_debug("%s,%d, list empty\n", __func__, __LINE__);
				goto next;
			}
			job = list_first_entry(&prealloc_job_list, struct prealloc_job, list);
			list_del(&job->list);

			if (fatal_signal_pending(job->host)) {
				spin_unlock(&job_list_lock);

				spin_lock(&ht_list_lock);
				if (!hlist_unhashed(&job->hnode)) {
					hash_del(&job->hnode);
					kfree(job);
				} else {
					complete(&job->done);
				}
				spin_unlock(&ht_list_lock);

				c_work->ret = -EINTR;
				goto next;
			}
			spin_unlock(&job_list_lock);
			snprintf(mem_name, sizeof(mem_name),
				"pre_mem_%d", job->type);
			job->mem = codec_mm_alloc(mem_name, job->size,
				job->align_2n, job->memflags);
			complete(&job->done);
		}
next:
		if (kthread_should_stop()) {
			pr_err("%s task exit\n", __func__);
			break;
		}
	}

	return 0;
}

struct codec_mm_s *get_mms_from_hashtable(u32 key, int align_2n)
{
	struct prealloc_job *job = NULL;
	struct prealloc_job *pre_select_job = NULL;
	struct codec_mm_s *mm = NULL;
	struct hlist_node *tmp;

	if (align_2n < 0 || align_2n < PAGE_SHIFT)
		align_2n = PAGE_SHIFT;

	spin_lock(&ht_list_lock);
	hash_for_each_possible_safe(ht, job, tmp, hnode, MB_ALIGN(key)) {
		if (job->size == key && job->align_2n >= align_2n) {
			pre_select_job = job;
			if (job->mem && completion_done(&job->done)) {
				pr_dbg("get job size is %d align_2n is %d\n",
					job->size, job->align_2n);
				hash_del(&job->hnode);
				mm = job->mem;
				kfree(job);
				break;
			}
		}
	}

	if (!mm && pre_select_job)
		hash_del(&pre_select_job->hnode);

	spin_unlock(&ht_list_lock);

	if (!mm && pre_select_job) {
		int ret = wait_for_completion_timeout(&pre_select_job->done,
			msecs_to_jiffies(1000));

		if (ret == 0)
			pr_err("codec_mm wait prealloc buffer timeout\n");
		if (pre_select_job->mem)
			mm = pre_select_job->mem;
		kfree(pre_select_job);
	}

	return mm;
}

int init_prealloc_boost_task(void)
{
	int cpu;
	struct task_struct *task;
	struct prealloc_pcp *work;
	char task_name[20];

	spin_lock_init(&job_list_lock);
	spin_lock_init(&ht_list_lock);
	INIT_LIST_HEAD(&prealloc_job_list);
	hash_init(ht);

	for (cpu = 0; cpu < ALLOC_PAGE_NUM_KTHREAD; cpu++) {
		memset(task_name, 0, sizeof(task_name));
		sprintf(task_name, "prealloc_task%d", cpu);
		work = &prealloc_kthread_array[cpu];
		init_completion(&work->start);
		work->cpu = cpu;
		task = kthread_create(prealloc_boost_work_func, work, task_name);
		if (!IS_ERR(task)) {
			set_user_nice(task, -17);
			work->task = task;
			pr_err("create prealloc task%p, for cpu %d\n", task, cpu);
			wake_up_process(task);
		} else {
			pr_err("create task for cpu %d fail:%p\n", cpu, task);
			return -1;
		}
	}

	return 0;
}
EXPORT_SYMBOL(init_prealloc_boost_task);
