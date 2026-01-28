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
#include <linux/delay.h>
#include <linux/syscalls.h>
#include <linux/cpumask.h>
#include <linux/printk.h>

#include "codec_mm_priv.h"

static struct list_head work_list;
static spinlock_t work_list_lock;		/* protect job list */

#define MAX_JOB_NUMBER		12
#define TASK_ARRAY_NUMBER	342

struct work_alloc_pages {
	struct list_head list;
	struct page **page[TASK_ARRAY_NUMBER];
	struct task_struct *host;
	gfp_t gfp;
	int array_len;
};

struct alloc_page_pcp {
	struct completion start;
	struct completion end;
	struct task_struct *task;
	int cpu;
	int ret;
};

#define ALLOC_PAGE_NUM_KTHREAD	3
static DEFINE_MUTEX(alloc_page_mutex);
struct work_alloc_pages alloc_pages_job[MAX_JOB_NUMBER] = {};
struct alloc_page_pcp alloc_kthread_array[ALLOC_PAGE_NUM_KTHREAD];

unsigned long aml_alloc_pages_array(gfp_t gfp, unsigned long nr_pages, struct page **page_array)
{
	int cpu, cpus, i = 0, j = 0, k = 0, ret = 0, enomem = 0, einv = 0;
	atomic_t ok;
	unsigned long flags;
	struct alloc_page_pcp *work;
	unsigned int max_count = MAX_JOB_NUMBER * TASK_ARRAY_NUMBER;

	mutex_lock(&alloc_page_mutex);

	cpus = num_online_cpus() - 1;

	if (nr_pages > max_count) {
		pr_err("nr pages too large: %ld > max(%d)\n", nr_pages, max_count);
		return -ENOMEM;
	}

	spin_lock(&work_list_lock);
	if (!list_empty(&work_list))
		INIT_LIST_HEAD(&work_list);
	for (k = 0; k < nr_pages; i++) {
		INIT_LIST_HEAD(&alloc_pages_job[i].list);
		for (j = 0; j < TASK_ARRAY_NUMBER && k < nr_pages; j++)
			alloc_pages_job[i].page[j] = &page_array[k++];
		alloc_pages_job[i].array_len = j;
		alloc_pages_job[i].host = current;
		alloc_pages_job[i].gfp = gfp;
		list_add(&alloc_pages_job[i].list, &work_list);
	}
	spin_unlock(&work_list_lock);
	i = 0;

	atomic_set(&ok, 0);
	local_irq_save(flags);
	for (cpu = 0; cpu < ALLOC_PAGE_NUM_KTHREAD; cpu++) {
		work = &alloc_kthread_array[cpu];
		complete(&work->start);
		i++;
		if (i == cpus) {
			pr_debug("sched work to %d cpu\n", i);
			break;
		}
	}
	local_irq_restore(flags);

	for (cpu = 0; cpu < i; cpu++) {
		work = &alloc_kthread_array[cpu];
		wait_for_completion(&work->end);
		if (work->ret) {
			if (work->ret == -EINTR)
				einv++;
			else
				enomem++;
		}
	}

	if (einv)
		ret = -EINVAL;
	else if (enomem)
		ret = -ENOMEM;
	else
		ret = 0;

	if (ret < 0)
		pr_err("%s, failed, ret:%d, ok:%d\n",
		       __func__, ret, atomic_read(&ok));

	mutex_unlock(&alloc_page_mutex);

	return ret;
}
EXPORT_SYMBOL(aml_alloc_pages_array);

static int alloc_page_boost_work_func(void *alloc_page_data)
{
	struct alloc_page_pcp *c_work;
	struct work_alloc_pages *job;
	int ret = -1, i = 0;
	struct page *page;
	gfp_t gfp;

	c_work  = (struct alloc_page_pcp *)alloc_page_data;
	for (;;) {
		ret = wait_for_completion_interruptible(&c_work->start);
		if (ret < 0) {
			pr_err("%s wait for task %d is %d\n",
			       __func__, c_work->cpu, ret);
			continue;
		}
again:
		spin_lock(&work_list_lock);
		if (list_empty(&work_list)) {
			/* NO job todo ? */
			spin_unlock(&work_list_lock);
			pr_debug("%s,%d, list empty\n", __func__, __LINE__);
			goto next;
		}
		job = list_first_entry(&work_list, struct work_alloc_pages, list);
		list_del(&job->list);
		spin_unlock(&work_list_lock);
		gfp = job->gfp;

		if (fatal_signal_pending(job->host)) {
			c_work->ret = -EINTR;
			goto next;
		}
		for (i = 0; i < job->array_len; i++) {
			page = alloc_pages(gfp, 0);
			if (page) {
				if (!job->page[i])
					pr_err("job i: %d, array_len: %d\n", i, job->array_len);
				*job->page[i] = page;
			} else {
				c_work->ret = -ENOMEM;
				pr_err("alloc page failed\n");
				break;
			}
		}
		if (i >= job->array_len)
			goto again;
next:
		complete(&c_work->end);
		if (kthread_should_stop()) {
			pr_err("%s task exit\n", __func__);
			break;
		}
	}
	return 0;
}

int init_alloc_page_boost_task(void)
{
	int cpu;
	struct task_struct *task;
	struct alloc_page_pcp *work;
	char task_name[20] = {};

	spin_lock_init(&work_list_lock);
	INIT_LIST_HEAD(&work_list);

	for (cpu = 0; cpu < ALLOC_PAGE_NUM_KTHREAD; cpu++) {
		memset(task_name, 0, sizeof(task_name));
		sprintf(task_name, "alloc_page_task%d", cpu);
		work = &alloc_kthread_array[cpu];
		init_completion(&work->start);
		init_completion(&work->end);
		work->cpu = cpu;
		task = kthread_create(alloc_page_boost_work_func, work, task_name);
		if (!IS_ERR(task)) {
			set_user_nice(task, -17);
			work->task = task;
			pr_debug("create alloc page task%p, for cpu %d\n", task, cpu);
			wake_up_process(task);
		} else {
			pr_err("create task for cpu %d fail:%p\n", cpu, task);
			return -1;
		}
	}

	return 0;
}

void aml_free_pages_array(unsigned long nr_pages, struct page **page_array)
{
	unsigned int count = 0;
	struct page *page;
	unsigned int i = 0;

	while (nr_pages) {
		page = page_array[i];
		if (!page) {
			nr_pages--;
			i++;
			continue;
		}
		if (!put_page_testzero(page)) {
			pr_info("%s, page %d are still in use\n", __func__, i);
			count++;
			break;
		}
		__free_page(page);
		nr_pages--;
		i++;
	}
	WARN(count != 0, "%d pages are still in use!\n", count);
}
EXPORT_SYMBOL(aml_free_pages_array);
