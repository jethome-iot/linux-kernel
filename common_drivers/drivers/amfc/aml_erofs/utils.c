// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2018 HUAWEI, Inc.
 *             https://www.huawei.com/
 */
#include <linux/pagevec.h>
#include <linux/seq_file.h>
#include "internal.h"

struct page *erofs_allocpage(struct page **pagepool, gfp_t gfp)
{
	struct page *page = *pagepool;

	if (page) {
		DBG_BUGON(page_ref_count(page) != 1);
		*pagepool = (struct page *)page_private(page);
	} else {
		page = alloc_page(gfp);
	}
	return page;
}

void erofs_release_pages(struct page **pagepool)
{
	while (*pagepool) {
		struct page *page = *pagepool;

		*pagepool = (struct page *)page_private(page);
		put_page(page);
	}
}

#ifdef CONFIG_AMLOGIC_EROFS_ZIP
/* global shrink count (for all mounted EROFS instances) */
static atomic_long_t erofs_global_shrink_cnt;

static int erofs_workgroup_get(struct erofs_workgroup *grp)
{
	int o;

repeat:
	o = erofs_wait_on_workgroup_freezed(grp);
	if (o <= 0)
		return -1;

	if (atomic_cmpxchg(&grp->refcount, o, o + 1) != o)
		goto repeat;

	/* decrease refcount paired by erofs_workgroup_put */
	if (o == 1)
		atomic_long_dec(&erofs_global_shrink_cnt);
	return 0;
}

struct erofs_workgroup *erofs_find_workgroup(struct super_block *sb,
					     pgoff_t index)
{
	struct erofs_sb_info *sbi = EROFS_SB(sb);
	struct erofs_workgroup *grp;

repeat:
	rcu_read_lock();
	grp = xa_load(&sbi->managed_pslots, index);
	if (grp) {
		if (erofs_workgroup_get(grp)) {
			/* prefer to relax rcu read side */
			rcu_read_unlock();
			goto repeat;
		}

		DBG_BUGON(index != grp->index);
	}
	rcu_read_unlock();
	return grp;
}

struct erofs_workgroup __nocfi *erofs_insert_workgroup(struct super_block *sb,
					       struct erofs_workgroup *grp)
{
	struct erofs_sb_info *const sbi = EROFS_SB(sb);
	struct erofs_workgroup *pre;

	/*
	 * Bump up a reference count before making this visible
	 * to others for the XArray in order to avoid potential
	 * UAF without serialized by xa_lock.
	 */
	atomic_inc(&grp->refcount);

repeat:
	xa_lock(&sbi->managed_pslots);
	pre = f___xa_cmpxchg(&sbi->managed_pslots, grp->index,
			   NULL, grp, GFP_NOFS);
	if (pre) {
		if (xa_is_err(pre)) {
			pre = ERR_PTR(xa_err(pre));
		} else if (erofs_workgroup_get(pre)) {
			/* try to legitimize the current in-tree one */
			xa_unlock(&sbi->managed_pslots);
			cond_resched();
			goto repeat;
		}
		atomic_dec(&grp->refcount);
		grp = pre;
	}
	xa_unlock(&sbi->managed_pslots);
	return grp;
}

static void  __erofs_workgroup_free(struct erofs_workgroup *grp)
{
	atomic_long_dec(&erofs_global_shrink_cnt);
	erofs_workgroup_free_rcu(grp);
}

int erofs_workgroup_put(struct erofs_workgroup *grp)
{
	int count = atomic_dec_return(&grp->refcount);

	if (count == 1)
		atomic_long_inc(&erofs_global_shrink_cnt);
	else if (!count)
		__erofs_workgroup_free(grp);
	return count;
}

static bool erofs_try_to_release_workgroup(struct erofs_sb_info *sbi,
					   struct erofs_workgroup *grp)
{
	/*
	 * If managed cache is on, refcount of workgroups
	 * themselves could be < 0 (freezed). In other words,
	 * there is no guarantee that all refcounts > 0.
	 */
	if (!erofs_workgroup_try_to_freeze(grp, 1))
		return false;

	/*
	 * Note that all cached pages should be unattached
	 * before deleted from the XArray. Otherwise some
	 * cached pages could be still attached to the orphan
	 * old workgroup when the new one is available in the tree.
	 */
	if (erofs_try_to_free_all_cached_pages(sbi, grp)) {
		erofs_workgroup_unfreeze(grp, 1);
		return false;
	}

	/*
	 * It's impossible to fail after the workgroup is freezed,
	 * however in order to avoid some race conditions, add a
	 * DBG_BUGON to observe this in advance.
	 */
	DBG_BUGON(__xa_erase(&sbi->managed_pslots, grp->index) != grp);

	/* last refcount should be connected with its managed pslot.  */
	erofs_workgroup_unfreeze(grp, 0);
	__erofs_workgroup_free(grp);
	return true;
}

static unsigned long erofs_shrink_workstation(struct erofs_sb_info *sbi,
					      unsigned long nr_shrink)
{
	struct erofs_workgroup *grp;
	unsigned int freed = 0;
	unsigned long index;

	xa_lock(&sbi->managed_pslots);
	xa_for_each(&sbi->managed_pslots, index, grp) {
		/* try to shrink each valid workgroup */
		if (!erofs_try_to_release_workgroup(sbi, grp))
			continue;
		xa_unlock(&sbi->managed_pslots);

		++freed;
		if (!--nr_shrink)
			return freed;
		xa_lock(&sbi->managed_pslots);
	}
	xa_unlock(&sbi->managed_pslots);
	return freed;
}

/* protected by 'erofs_sb_list_lock' */
static unsigned int shrinker_run_no;

/* protects the mounted 'erofs_sb_list' */
static DEFINE_SPINLOCK(erofs_sb_list_lock);
static LIST_HEAD(erofs_sb_list);

void erofs_shrinker_register(struct super_block *sb)
{
	struct erofs_sb_info *sbi = EROFS_SB(sb);

	mutex_init(&sbi->umount_mutex);

	spin_lock(&erofs_sb_list_lock);
	list_add(&sbi->list, &erofs_sb_list);
	spin_unlock(&erofs_sb_list_lock);
}

void erofs_shrinker_unregister(struct super_block *sb)
{
	struct erofs_sb_info *const sbi = EROFS_SB(sb);

	mutex_lock(&sbi->umount_mutex);
	/* clean up all remaining workgroups in memory */
	erofs_shrink_workstation(sbi, ~0UL);

	spin_lock(&erofs_sb_list_lock);
	list_del(&sbi->list);
	spin_unlock(&erofs_sb_list_lock);
	mutex_unlock(&sbi->umount_mutex);
}

static unsigned long erofs_shrink_count(struct shrinker *shrink,
					struct shrink_control *sc)
{
	return atomic_long_read(&erofs_global_shrink_cnt);
}

static unsigned long erofs_shrink_scan(struct shrinker *shrink,
				       struct shrink_control *sc)
{
	struct erofs_sb_info *sbi;
	struct list_head *p;

	unsigned long nr = sc->nr_to_scan;
	unsigned int run_no;
	unsigned long freed = 0;

	spin_lock(&erofs_sb_list_lock);
	do {
		run_no = ++shrinker_run_no;
	} while (run_no == 0);

	/* Iterate over all mounted superblocks and try to shrink them */
	p = erofs_sb_list.next;
	while (p != &erofs_sb_list) {
		sbi = list_entry(p, struct erofs_sb_info, list);

		/*
		 * We move the ones we do to the end of the list, so we stop
		 * when we see one we have already done.
		 */
		if (sbi->shrinker_run_no == run_no)
			break;

		if (!mutex_trylock(&sbi->umount_mutex)) {
			p = p->next;
			continue;
		}

		spin_unlock(&erofs_sb_list_lock);
		sbi->shrinker_run_no = run_no;

		freed += erofs_shrink_workstation(sbi, nr - freed);

		spin_lock(&erofs_sb_list_lock);
		/* Get the next list element before we move this one */
		p = p->next;

		/*
		 * Move this one to the end of the list to provide some
		 * fairness.
		 */
		list_move_tail(&sbi->list, &erofs_sb_list);
		mutex_unlock(&sbi->umount_mutex);

		if (freed >= nr)
			break;
	}
	spin_unlock(&erofs_sb_list_lock);
	return freed;
}

static struct shrinker erofs_shrinker_info = {
	.scan_objects = erofs_shrink_scan,
	.count_objects = erofs_shrink_count,
	.seeks = DEFAULT_SEEKS,
};

int __init erofs_init_shrinker(void)
{
	return register_shrinker(&erofs_shrinker_info);
}

void erofs_exit_shrinker(void)
{
	unregister_shrinker(&erofs_shrinker_info);
}
#endif	/* !CONFIG_AMLOGIC_EROFS_ZIP */

#include <linux/proc_fs.h>

__xa_cmpxchg_t			f___xa_cmpxchg;
fs_ftype_to_dtype_t		f_fs_ftype_to_dtype;
filemap_read_t			f_filemap_read;
iomap_dio_rw_t			f_iomap_dio_rw;
iomap_bmap_t			f_iomap_bmap;
iomap_readahead_t		f_iomap_readahead;
iomap_readpage_t		f_iomap_readpage;
generic_file_readonly_mmap_t	f_generic_file_readonly_mmap;
noop_direct_IO_t		f_noop_direct_IO;
iomap_fiemap_t			f_iomap_fiemap;
read_cache_page_gfp_t		f_read_cache_page_gfp;
page_get_link_t			f_page_get_link;
simple_get_link_t		f_simple_get_link;
crc32c_t			f_crc32c;
add_to_page_cache_lru_t		f_add_to_page_cache_lru;
readahead_expand_t		f_readahead_expand;
kthread_create_worker_on_cpu_t	f_kthread_create_worker_on_cpu;
LZ4_decompress_safe_t		f_LZ4_decompress_safe;
LZ4_decompress_safe_partial_t	f_LZ4_decompress_safe_partial;
out_of_line_wait_on_bit_lock_t	f_out_of_line_wait_on_bit_lock;
fs_param_is_enum_t		f_fs_param_is_enum;
unsigned long			posix_acl_from_xattr_t;
struct file_operations		*generic_ro_fops_t;
struct xattr_handler		*posix_acl_default_xattr_handler_t;
struct xattr_handler		*posix_acl_access_xattr_handler_t;

struct ksymbol {
	const char *name;
	void *data;
	unsigned int name_len;
};

#define KSYM_FUN(sym)			\
	{				\
		.name = #sym,		\
		.data = &f_##sym,	\
	}

#ifdef CONFIG_KASAN_GENERIC
/* When KASAN enabled cfi is disabled */
#define KSYM_CFI(sym)			\
	{				\
		.name = #sym ,		\
		.data = &f_##sym,	\
	}
#else
#define KSYM_CFI(sym)			\
	{				\
		.name = #sym ".cfi_jt",	\
		.data = &f_##sym,	\
	}
#endif

#define KSYM_OBJ(sym)			\
	{				\
		.name = #sym,		\
		.data = &sym##_t,	\
	}

#ifdef CONFIG_ARM64
static struct ksymbol module_symbols[] = {
	KSYM_FUN(__xa_cmpxchg),
	KSYM_FUN(fs_ftype_to_dtype),
	KSYM_FUN(filemap_read),
	KSYM_FUN(iomap_dio_rw),
	KSYM_FUN(iomap_bmap),
	KSYM_FUN(iomap_readahead),
	KSYM_FUN(iomap_readpage),
#ifdef CONFIG_CFI_CLANG
	KSYM_CFI(generic_file_readonly_mmap),
	KSYM_CFI(noop_direct_IO),
	KSYM_CFI(simple_get_link),
	KSYM_CFI(page_get_link),
	KSYM_CFI(fs_param_is_enum),
#else
	KSYM_FUN(generic_file_readonly_mmap),
	KSYM_FUN(noop_direct_IO),
	KSYM_FUN(simple_get_link),
	KSYM_FUN(page_get_link),
	KSYM_FUN(fs_param_is_enum),
#endif
	KSYM_FUN(iomap_fiemap),
	KSYM_FUN(read_cache_page_gfp),
	KSYM_FUN(crc32c),
	KSYM_FUN(add_to_page_cache_lru),
	KSYM_FUN(readahead_expand),
	KSYM_FUN(kthread_create_worker_on_cpu),
	KSYM_FUN(LZ4_decompress_safe),
	KSYM_FUN(LZ4_decompress_safe_partial),
	KSYM_FUN(out_of_line_wait_on_bit_lock),
	KSYM_OBJ(generic_ro_fops),
	KSYM_OBJ(posix_acl_default_xattr_handler),
	KSYM_OBJ(posix_acl_access_xattr_handler),
	{	/*
		 * compiler always say symbol undefined if use KSYM_FUN macro
		 * seems compiler have bug.
		 */
		.name = "posix_acl_from_xattr",
		.data = &posix_acl_from_xattr_t,
	},
	{}
};

/* see struct proc_dir_entry in fs/proc/internal.h */
struct proc_node {
	atomic_t in_use;
	refcount_t refcnt;
	struct list_head pde_openers;
	spinlock_t pde_unload_lock;
	struct completion *pde_unload_completion;
	const struct inode_operations *proc_iops;
	union {
		const struct proc_ops *proc_ops;
		const struct file_operations *proc_dir_ops;
	};
	const struct dentry_operations *proc_dops;
	union {
		const struct seq_operations *seq_ops;
		int (*single_show)(struct seq_file *, void *);
	};
	proc_write_t write;
	void *data;
	unsigned int state_size;
	unsigned int low_ino;
	nlink_t nlink;
	kuid_t uid;
	kgid_t gid;
	loff_t size;
	struct proc_node *parent;
	struct rb_root subdir;
	struct rb_node subdir_node;
	char *name;
	umode_t mode;
	u8 flags;
	u8 namelen;
	char inline_name[];
} __randomize_layout;

static struct proc_node *find_subnode(struct proc_node *parent, char *name)
{
	struct proc_node *pd = NULL;

	pd = rb_entry_safe(rb_first(&parent->subdir), struct proc_node, subdir_node);
	while (1) {
		if (!pd)
			break;
		if (!strcmp(pd->name, name))
			break;
		pd = rb_entry_safe(rb_next(&pd->subdir_node), struct proc_node, subdir_node);
	}
	return pd;
}

static int namecmp(const char *name1, int len1, const char *name2, int len2)
{
	int cmp;

	cmp = memcmp(name1, name2, min(len1, len2));
	if (cmp == 0)
		cmp = len1 - len2;
	return cmp;
}

static int proc_handle(struct ctl_table *table, int write,
		  void *buffer, size_t *lenp, loff_t *ppos)
{
	return 0;
}

static struct ctl_table *disable_kstr_ptr(int *old)
{
	struct ctl_table tmp[] = {
		{
			.procname = "amfc",
			.proc_handler = proc_handle,
			.mode = 0666,
			.data = NULL,
		},
		{}
	};
	struct ctl_table_header *hdr, *head;
	struct rb_node *node;
	struct ctl_table *entry = NULL;
	struct ctl_dir *dir;

	hdr = register_sysctl("kernel", tmp);
	if (!hdr) {
		pr_err("%s, register failed\n", __func__);
		return NULL;
	}

	dir = hdr->parent;
	node = dir->root.rb_node;
	while (node) {
		struct ctl_node *ctl_node;
		const char *procname;
		int cmp;

		ctl_node = rb_entry(node, struct ctl_node, node);
		head = ctl_node->header;
		entry = &head->ctl_table[ctl_node - head->node];
		procname = entry->procname;

		cmp = namecmp("perf_event_paranoid", 13, procname, strlen(procname));
		if (cmp < 0)
			node = node->rb_left;
		else if (cmp > 0)
			node = node->rb_right;
		else {
			break;
		}
	}
	unregister_sysctl_table(hdr);
	if (entry) {
		*old = *(int *)entry->data;
		pr_debug("get entry:%px, %s, value:%d\n",
			entry, entry->procname, *old);
		*(int *)entry->data = 0;
		return entry;
	}
	return NULL;
}

static int fill_symbol(char *line, struct ksymbol *sym)
{
	int finish = 2;
	unsigned long value;
	int ret, find = 0, len;

	while (sym->name) {
		if (*(unsigned long *)sym->data) {
			sym++;
			continue;
		} else
			finish = 0;

		if (!sym->name_len)
			sym->name_len = strlen(sym->name);
		len = sym->name_len;
		if (!strncmp(line + 19, sym->name, len)) {
			if (line[19 + len + 1]) {	// not terminate
				sym++;
				continue;
			}
			line[16] = '\0';
			ret = kstrtoul(line, 16, &value);
			pr_debug("find sym:%lx %s\n", value, sym->name);
			if (ret)
				return ret;
			*(unsigned long *)sym->data = value;
			find = 1;
			break;
		}
		sym++;
	}
	if (finish == 2)
		return 2;
	return find;
}

int check_sym(struct ksymbol *sym)
{
	int find = 0, nfind = 0;

	while (sym->name) {
		if (*(unsigned long *)sym->data) {
			find++;
		} else {
			pr_err("can't find sym:%s\n", sym->name);
			nfind++;
		}
		sym++;
	}
	return nfind;
}

static struct super_block _s = {};

static struct inode _i = {
	.i_sb = &_s,
};

static struct address_space _a = {
	.host = &_i,
};

static struct file f = {
	.f_mapping = &_a,
	.f_inode = &_i,
};

static int fill_module_symbols(struct proc_node *node, struct ksymbol *sym)
{
	struct proc_ops *ops;
	struct seq_file *s;
	char line_buf[256] = {};
	int ret = 0;
	loff_t off = 0;
	void *p;

	ops = (struct proc_ops *)node->proc_ops;
	ret = ops->proc_open(NULL, &f);
	if (ret) {
		pr_info("open fail\n");
		return -1;
	}
	s = f.private_data;
	p = s->op->start(s, &off);
	s->buf = line_buf;
	while (1) {
		s->count = 0;
		s->size  = sizeof(line_buf);
		//memset(line_buf, 0, sizeof(line_buf));
		ret = s->op->show(s, NULL);
		if (ret < 0) {
			pr_info("read fail:%d\n", ret);
			break;
		}
		s->count = 0;
		pr_debug("line:%s", line_buf);
		ret = fill_symbol(line_buf, sym);
		if (ret == 2)
			break;
		p = s->op->next(s, p, &off);
		if (!p || IS_ERR(p))
			break;
	}
	s->buf = NULL;
	ops->proc_release(NULL, &f);
	if (check_sym(sym))
		return -ENOSYS;
	return 0;
}

int symbol_fix(void)
{
	struct proc_dir_entry *entry;
	struct proc_node *parent, *tmp;
	const struct proc_ops temp = {};
	struct ctl_table *kptr;
	int ret = 0, old = -1;

	entry = proc_create("amfc", 0444, NULL, &temp);
	if (!entry) {
		pr_err("%s, create proc failed\n", __func__);
		return -1;
	}
	parent = ((struct proc_node *)entry)->parent;
	if (!parent) {
		pr_err("%s, NULL pareret\n", __func__);
		return -1;
	}

	kptr = disable_kstr_ptr(&old);
	if (!kptr) {
		pr_err("get kptr failed\n");
		return -1;
	}
	tmp = find_subnode(parent, "kallsyms");
	if (!tmp) {
		pr_err("get kallsyms failed\n");
		return -1;
	}

	ret = fill_module_symbols(tmp, module_symbols);

	*(int *)kptr->data = old;

	proc_remove(entry);

	return ret;
}
#else
/* for arm32 */
#include <linux/crc32c.h>
#include <linux/kthread.h>
#include <linux/lz4.h>
#include <linux/posix_acl_xattr.h>
int symbol_fix(void)
{
	f___xa_cmpxchg                    = __xa_cmpxchg;
	f_fs_ftype_to_dtype               = fs_ftype_to_dtype;
	f_filemap_read                    = filemap_read;
	f_iomap_dio_rw                    = iomap_dio_rw;
	f_iomap_bmap                      = (iomap_bmap_t)iomap_bmap;
	f_iomap_readahead                 = iomap_readahead;
	f_iomap_readpage                  = iomap_readpage;
	f_generic_file_readonly_mmap      = generic_file_readonly_mmap;
	f_noop_direct_IO                  = noop_direct_IO;
	f_iomap_fiemap                    = iomap_fiemap;
	f_read_cache_page_gfp             = read_cache_page_gfp;
	f_page_get_link                   = page_get_link;
	f_simple_get_link                 = simple_get_link;
	f_crc32c                          = crc32c;
	f_add_to_page_cache_lru           = add_to_page_cache_lru;
	f_readahead_expand                = readahead_expand;
	f_kthread_create_worker_on_cpu    = kthread_create_worker_on_cpu;
	f_LZ4_decompress_safe             = LZ4_decompress_safe;
	f_LZ4_decompress_safe_partial     = LZ4_decompress_safe_partial;
	f_out_of_line_wait_on_bit_lock    = out_of_line_wait_on_bit_lock;
	f_fs_param_is_enum                = fs_param_is_enum;
	posix_acl_from_xattr_t            = (unsigned long)posix_acl_from_xattr;
	generic_ro_fops_t                 = (struct file_operations *)&generic_ro_fops;
	posix_acl_default_xattr_handler_t = (struct xattr_handler *)&posix_acl_default_xattr_handler;
	posix_acl_access_xattr_handler_t  = (struct xattr_handler *)&posix_acl_access_xattr_handler;

	return 0;
}
#endif
