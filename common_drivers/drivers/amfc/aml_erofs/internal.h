/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2017-2018 HUAWEI, Inc.
 *             https://www.huawei.com/
 * Copyright (C) 2021, Alibaba Cloud
 */
#ifndef __EROFS_INTERNAL_H
#define __EROFS_INTERNAL_H

#include <linux/fs.h>
#include <linux/dcache.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/bio.h>
#include <linux/buffer_head.h>
#include <linux/magic.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/iomap.h>
#include <linux/fs_context.h>
#include <linux/fs_parser.h>
#include "erofs_fs.h"

/* redefine pr_fmt "erofs: " */
#undef pr_fmt
#define pr_fmt(fmt) "erofs: " fmt

__printf(3, 4) void _erofs_err(struct super_block *sb,
			       const char *function, const char *fmt, ...);
#define erofs_err(sb, fmt, ...)	\
	_erofs_err(sb, __func__, fmt "\n", ##__VA_ARGS__)
__printf(3, 4) void _erofs_info(struct super_block *sb,
			       const char *function, const char *fmt, ...);
#define erofs_info(sb, fmt, ...) \
	_erofs_info(sb, __func__, fmt "\n", ##__VA_ARGS__)
#ifdef CONFIG_AMLOGIC_EROFS_DEBUG
#define erofs_dbg(x, ...)       pr_debug(x "\n", ##__VA_ARGS__)
#define DBG_BUGON               BUG_ON
#else
#define erofs_dbg(x, ...)       ((void)0)
#define DBG_BUGON(x)            ((void)(x))
#endif	/* !CONFIG_AMLOGIC_EROFS_DEBUG */

/* EROFS_SUPER_MAGIC_V1 to represent the whole file system */
#define EROFS_SUPER_MAGIC   EROFS_SUPER_MAGIC_V1

typedef u64 erofs_nid_t;
typedef u64 erofs_off_t;
/* data type for filesystem-wide blocks number */
typedef u32 erofs_blk_t;

struct erofs_device_info {
	char *path;
	struct block_device *bdev;
	struct dax_device *dax_dev;

	u32 blocks;
	u32 mapped_blkaddr;
};

struct erofs_mount_opts {
#ifdef CONFIG_AMLOGIC_EROFS_ZIP
	/* current strategy of how to use managed cache */
	unsigned char cache_strategy;
	/* strategy of sync decompression (false - auto, true - force on) */
	bool readahead_sync_decompress;

	/* threshold for decompression synchronously */
	unsigned int max_sync_decompress_pages;
#endif
	unsigned int mount_opt;
};

struct erofs_dev_context {
	struct idr tree;
	struct rw_semaphore rwsem;

	unsigned int extra_devices;
};

struct erofs_fs_context {
	struct erofs_mount_opts opt;
	struct erofs_dev_context *devs;
};

/* all filesystem-wide lz4 configurations */
struct erofs_sb_lz4_info {
	/* # of pages needed for EROFS lz4 rolling decompression */
	u16 max_distance_pages;
	/* maximum possible blocks for pclusters in the filesystem */
	u16 max_pclusterblks;
};

struct erofs_sb_info {
	struct erofs_mount_opts opt;	/* options */
#ifdef CONFIG_AMLOGIC_EROFS_ZIP
	/* list for all registered superblocks, mainly for shrinker */
	struct list_head list;
	struct mutex umount_mutex;

	/* managed XArray arranged in physical block number */
	struct xarray managed_pslots;

	unsigned int shrinker_run_no;
	u16 available_compr_algs;

	/* pseudo inode to manage cached pages */
	struct inode *managed_cache;

	struct erofs_sb_lz4_info lz4;
	/* decompress crypto for other algorithms except lz4/lzma */
	struct crypto_comp *crypto;
#endif	/* CONFIG_AMLOGIC_EROFS_ZIP */
	struct erofs_dev_context *devs;
	struct dax_device *dax_dev;
	u64 total_blocks;
	u32 primarydevice_blocks;

	u32 meta_blkaddr;
#ifdef CONFIG_AMLOGIC_EROFS_XATTR
	u32 xattr_blkaddr;
#endif
	u16 device_id_mask;	/* valid bits of device id to be used */

	/* inode slot unit size in bit shift */
	unsigned char islotbits;

	u32 sb_size;			/* total superblock size */
	u32 build_time_nsec;
	u64 build_time;

	/* what we really care is nid, rather than ino.. */
	erofs_nid_t root_nid;
	/* used for statfs, f_files - f_favail */
	u64 inos;

	u8 uuid[16];                    /* 128-bit uuid for volume */
	u8 volume_name[16];             /* volume name */
	u32 feature_compat;
	u32 feature_incompat;

	/* sysfs support */
	struct kobject s_kobj;		/* /sys/fs/erofs/<devname> */
	struct completion s_kobj_unregister;
};

#define EROFS_SB(sb) ((struct erofs_sb_info *)(sb)->s_fs_info)
#define EROFS_I_SB(inode) ((struct erofs_sb_info *)(inode)->i_sb->s_fs_info)

/* Mount flags set via mount options or defaults */
#define EROFS_MOUNT_XATTR_USER		0x00000010
#define EROFS_MOUNT_POSIX_ACL		0x00000020
#define EROFS_MOUNT_DAX_ALWAYS		0x00000040
#define EROFS_MOUNT_DAX_NEVER		0x00000080

#define clear_opt(opt, option)	((opt)->mount_opt &= ~EROFS_MOUNT_##option)
#define set_opt(opt, option)	((opt)->mount_opt |= EROFS_MOUNT_##option)
#define test_opt(opt, option)	((opt)->mount_opt & EROFS_MOUNT_##option)

enum {
	EROFS_ZIP_CACHE_DISABLED,
	EROFS_ZIP_CACHE_READAHEAD,
	EROFS_ZIP_CACHE_READAROUND
};

#ifdef CONFIG_AMLOGIC_EROFS_ZIP
#define EROFS_LOCKED_MAGIC     (INT_MIN | 0xE0F510CCL)

/* basic unit of the workstation of a super_block */
struct erofs_workgroup {
	/* the workgroup index in the workstation */
	pgoff_t index;

	/* overall workgroup reference count */
	atomic_t refcount;
};

static inline bool erofs_workgroup_try_to_freeze(struct erofs_workgroup *grp,
						 int val)
{
	preempt_disable();
	if (val != atomic_cmpxchg(&grp->refcount, val, EROFS_LOCKED_MAGIC)) {
		preempt_enable();
		return false;
	}
	return true;
}

static inline void erofs_workgroup_unfreeze(struct erofs_workgroup *grp,
					    int orig_val)
{
	/*
	 * other observers should notice all modifications
	 * in the freezing period.
	 */
	smp_mb();
	atomic_set(&grp->refcount, orig_val);
	preempt_enable();
}

static inline int erofs_wait_on_workgroup_freezed(struct erofs_workgroup *grp)
{
	return atomic_cond_read_relaxed(&grp->refcount,
					VAL != EROFS_LOCKED_MAGIC);
}
#endif	/* !CONFIG_AMLOGIC_EROFS_ZIP */

/* we strictly follow PAGE_SIZE and no buffer head yet */
#define LOG_BLOCK_SIZE		PAGE_SHIFT

#undef LOG_SECTORS_PER_BLOCK
#define LOG_SECTORS_PER_BLOCK	(PAGE_SHIFT - 9)

#undef SECTORS_PER_BLOCK
#define SECTORS_PER_BLOCK	(1 << SECTORS_PER_BLOCK)

#define EROFS_BLKSIZ		(1 << LOG_BLOCK_SIZE)

#if (EROFS_BLKSIZ % 4096 || !EROFS_BLKSIZ)
#error erofs cannot be used in this platform
#endif

#define ROOT_NID(sb)		((sb)->root_nid)

#define erofs_blknr(addr)       ((addr) / EROFS_BLKSIZ)
#define erofs_blkoff(addr)      ((addr) % EROFS_BLKSIZ)
#define blknr_to_addr(nr)       ((erofs_off_t)(nr) * EROFS_BLKSIZ)

static inline erofs_off_t iloc(struct erofs_sb_info *sbi, erofs_nid_t nid)
{
	return blknr_to_addr(sbi->meta_blkaddr) + (nid << sbi->islotbits);
}

#define EROFS_FEATURE_FUNCS(name, compat, feature) \
static inline bool erofs_sb_has_##name(struct erofs_sb_info *sbi) \
{ \
	return sbi->feature_##compat & EROFS_FEATURE_##feature; \
}

EROFS_FEATURE_FUNCS(lz4_0padding, incompat, INCOMPAT_LZ4_0PADDING)
EROFS_FEATURE_FUNCS(compr_cfgs, incompat, INCOMPAT_COMPR_CFGS)
EROFS_FEATURE_FUNCS(big_pcluster, incompat, INCOMPAT_BIG_PCLUSTER)
EROFS_FEATURE_FUNCS(chunked_file, incompat, INCOMPAT_CHUNKED_FILE)
EROFS_FEATURE_FUNCS(device_table, incompat, INCOMPAT_DEVICE_TABLE)
EROFS_FEATURE_FUNCS(compr_head2, incompat, INCOMPAT_COMPR_HEAD2)
EROFS_FEATURE_FUNCS(sb_chksum, compat, COMPAT_SB_CHKSUM)

/* atomic flag definitions */
#define EROFS_I_EA_INITED_BIT	0
#define EROFS_I_Z_INITED_BIT	1

/* bitlock definitions (arranged in reverse order) */
#define EROFS_I_BL_XATTR_BIT	(BITS_PER_LONG - 1)
#define EROFS_I_BL_Z_BIT	(BITS_PER_LONG - 2)

struct erofs_inode {
	erofs_nid_t nid;

	/* atomic flags (including bitlocks) */
	unsigned long flags;

	unsigned char datalayout;
	unsigned char inode_isize;
	unsigned int xattr_isize;

	unsigned int xattr_shared_count;
	unsigned int *xattr_shared_xattrs;

	union {
		erofs_blk_t raw_blkaddr;
		struct {
			unsigned short	chunkformat;
			unsigned char	chunkbits;
		};
#ifdef CONFIG_AMLOGIC_EROFS_ZIP
		struct {
			unsigned short z_advise;
			unsigned char  z_algorithmtype[2];
			unsigned char  z_logical_clusterbits;
		};
#endif	/* CONFIG_AMLOGIC_EROFS_ZIP */
	};
	/* the corresponding vfs inode */
	struct inode vfs_inode;
};

#define EROFS_I(ptr)	\
	container_of(ptr, struct erofs_inode, vfs_inode)

static inline unsigned long erofs_inode_datablocks(struct inode *inode)
{
	/* since i_size cannot be changed */
	return DIV_ROUND_UP(inode->i_size, EROFS_BLKSIZ);
}

static inline unsigned int erofs_bitrange(unsigned int value, unsigned int bit,
					  unsigned int bits)
{

	return (value >> bit) & ((1 << bits) - 1);
}


static inline unsigned int erofs_inode_version(unsigned int value)
{
	return erofs_bitrange(value, EROFS_I_VERSION_BIT,
			      EROFS_I_VERSION_BITS);
}

static inline unsigned int erofs_inode_datalayout(unsigned int value)
{
	return erofs_bitrange(value, EROFS_I_DATALAYOUT_BIT,
			      EROFS_I_DATALAYOUT_BITS);
}

/*
 * Different from grab_cache_page_nowait(), reclaiming is never triggered
 * when allocating new pages.
 */
static inline
struct page *erofs_grab_cache_page_nowait(struct address_space *mapping,
					  pgoff_t index)
{
	return pagecache_get_page(mapping, index,
			FGP_LOCK|FGP_CREAT|FGP_NOFS|FGP_NOWAIT,
			readahead_gfp_mask(mapping) & ~__GFP_RECLAIM);
}

extern const struct super_operations erofs_sops;

extern struct address_space_operations erofs_raw_access_aops;
extern const struct address_space_operations z_erofs_aops;

/*
 * Logical to physical block mapping
 *
 * Different with other file systems, it is used for 2 access modes:
 *
 * 1) RAW access mode:
 *
 * Users pass a valid (m_lblk, m_lofs -- usually 0) pair,
 * and get the valid m_pblk, m_pofs and the longest m_len(in bytes).
 *
 * Note that m_lblk in the RAW access mode refers to the number of
 * the compressed ondisk block rather than the uncompressed
 * in-memory block for the compressed file.
 *
 * m_pofs equals to m_lofs except for the inline data page.
 *
 * 2) Normal access mode:
 *
 * If the inode is not compressed, it has no difference with
 * the RAW access mode. However, if the inode is compressed,
 * users should pass a valid (m_lblk, m_lofs) pair, and get
 * the needed m_pblk, m_pofs, m_len to get the compressed data
 * and the updated m_lblk, m_lofs which indicates the start
 * of the corresponding uncompressed data in the file.
 */
enum {
	BH_Encoded = BH_PrivateStart,
	BH_FullMapped,
};

/* Has a disk mapping */
#define EROFS_MAP_MAPPED	(1 << BH_Mapped)
/* Located in metadata (could be copied from bd_inode) */
#define EROFS_MAP_META		(1 << BH_Meta)
/* The extent is encoded */
#define EROFS_MAP_ENCODED	(1 << BH_Encoded)
/* The length of extent is full */
#define EROFS_MAP_FULL_MAPPED	(1 << BH_FullMapped)

struct erofs_map_blocks {
	erofs_off_t m_pa, m_la;
	u64 m_plen, m_llen;

	unsigned short m_deviceid;
	char m_algorithmformat;
	unsigned int m_flags;

	struct page *mpage;
};

/* Flags used by erofs_map_blocks_flatmode() */
#define EROFS_GET_BLOCKS_RAW    0x0001
/*
 * Used to get the exact decompressed length, e.g. fiemap (consider lookback
 * approach instead if possible since it's more metadata lightweight.)
 */
#define EROFS_GET_BLOCKS_FIEMAP	0x0002
/* Used to map the whole extent if non-negligible data is requested for LZMA */
#define EROFS_GET_BLOCKS_READMORE	0x0004

enum {
	Z_EROFS_COMPRESSION_SHIFTED = Z_EROFS_COMPRESSION_MAX,
	Z_EROFS_COMPRESSION_RUNTIME_MAX
};

/* zmap.c */
extern const struct iomap_ops z_erofs_iomap_report_ops;

#ifdef CONFIG_AMLOGIC_EROFS_ZIP
int z_erofs_fill_inode(struct inode *inode);
int z_erofs_map_blocks_iter(struct inode *inode,
			    struct erofs_map_blocks *map,
			    int flags);
#else
static inline int z_erofs_fill_inode(struct inode *inode) { return -EOPNOTSUPP; }
static inline int z_erofs_map_blocks_iter(struct inode *inode,
					  struct erofs_map_blocks *map,
					  int flags)
{
	return -EOPNOTSUPP;
}
#endif	/* !CONFIG_AMLOGIC_EROFS_ZIP */

struct erofs_map_dev {
	struct block_device *m_bdev;
	struct dax_device *m_daxdev;

	erofs_off_t m_pa;
	unsigned int m_deviceid;
};

/* data.c */
extern struct file_operations erofs_file_fops;
struct page *erofs_get_meta_page(struct super_block *sb, erofs_blk_t blkaddr);
int erofs_map_dev(struct super_block *sb, struct erofs_map_dev *dev);
int erofs_fiemap(struct inode *inode, struct fiemap_extent_info *fieinfo,
		 u64 start, u64 len);

/* inode.c */
static inline unsigned long erofs_inode_hash(erofs_nid_t nid)
{
#if BITS_PER_LONG == 32
	return (nid >> 32) ^ (nid & 0xffffffff);
#else
	return nid;
#endif
}

extern struct inode_operations erofs_generic_iops;
extern struct inode_operations erofs_symlink_iops;
extern struct inode_operations erofs_fast_symlink_iops;

struct inode *erofs_iget(struct super_block *sb, erofs_nid_t nid, bool dir);
int erofs_getattr(struct user_namespace *mnt_userns, const struct path *path,
		  struct kstat *stat, u32 request_mask,
		  unsigned int query_flags);

/* namei.c */
extern const struct inode_operations erofs_dir_iops;

int erofs_namei(struct inode *dir, struct qstr *name,
		erofs_nid_t *nid, unsigned int *d_type);

/* dir.c */
extern const struct file_operations erofs_dir_fops;

static inline void *erofs_vm_map_ram(struct page **pages, unsigned int count)
{
	int retried = 0;

	while (1) {
		void *p = vm_map_ram(pages, count, -1);

		/* retry two more times (totally 3 times) */
		if (p || ++retried >= 3)
			return p;
		vm_unmap_aliases();
	}
	return NULL;
}

/* pcpubuf.c */
void *erofs_get_pcpubuf(unsigned int requiredpages);
void erofs_put_pcpubuf(void *ptr);
int erofs_pcpubuf_growsize(unsigned int nrpages);
void erofs_pcpubuf_init(void);
void erofs_pcpubuf_exit(void);

/* sysfs.c */
int erofs_register_sysfs(struct super_block *sb);
void erofs_unregister_sysfs(struct super_block *sb);
int __init erofs_init_sysfs(void);
void erofs_exit_sysfs(void);

/* utils.c / zdata.c */
struct page *erofs_allocpage(struct page **pagepool, gfp_t gfp);
static inline void erofs_pagepool_add(struct page **pagepool,
		struct page *page)
{
	set_page_private(page, (unsigned long)*pagepool);
	*pagepool = page;
}
void erofs_release_pages(struct page **pagepool);

#ifdef CONFIG_AMLOGIC_EROFS_ZIP
int erofs_workgroup_put(struct erofs_workgroup *grp);
struct erofs_workgroup *erofs_find_workgroup(struct super_block *sb,
					     pgoff_t index);
struct erofs_workgroup *erofs_insert_workgroup(struct super_block *sb,
					       struct erofs_workgroup *grp);
void erofs_workgroup_free_rcu(struct erofs_workgroup *grp);
void erofs_shrinker_register(struct super_block *sb);
void erofs_shrinker_unregister(struct super_block *sb);
int __init erofs_init_shrinker(void);
void erofs_exit_shrinker(void);
int __init z_erofs_init_zip_subsystem(void);
void z_erofs_exit_zip_subsystem(void);
int erofs_try_to_free_all_cached_pages(struct erofs_sb_info *sbi,
				       struct erofs_workgroup *egrp);
int erofs_try_to_free_cached_page(struct page *page);
int z_erofs_load_lz4_config(struct super_block *sb,
			    struct erofs_super_block *dsb,
			    struct z_erofs_lz4_cfgs *lz4, int len);
int z_erofs_load_crypto_config(struct super_block *sb,
			     struct erofs_super_block *dsb,
			     struct z_erofs_crypto_cfgs *crypto, int size);
#else
static inline void erofs_shrinker_register(struct super_block *sb) {}
static inline void erofs_shrinker_unregister(struct super_block *sb) {}
static inline int erofs_init_shrinker(void) { return 0; }
static inline void erofs_exit_shrinker(void) {}
static inline int z_erofs_init_zip_subsystem(void) { return 0; }
static inline void z_erofs_exit_zip_subsystem(void) {}
static inline int z_erofs_load_lz4_config(struct super_block *sb,
				  struct erofs_super_block *dsb,
				  struct z_erofs_lz4_cfgs *lz4, int len)
{
	if (lz4 || dsb->u1.lz4_max_distance) {
		erofs_err(sb, "lz4 algorithm isn't enabled");
		return -EINVAL;
	}
	return 0;
}
static inline int z_erofs_load_crypto_config(struct super_block *sb,
					     struct erofs_super_block *dsb,
					     struct z_erofs_crypto_cfgs *crypto,
					     int size);
{
	if (crypto) {
		erofs_err(sb, "crypto algorithm isn't enabled");
		return -EINVAL;
	}
	return 0;
}
#endif	/* !CONFIG_AMLOGIC_EROFS_ZIP */

#ifdef CONFIG_AMLOGIC_EROFS_ZIP_LZMA
int z_erofs_lzma_init(void);
void z_erofs_lzma_exit(void);
int z_erofs_load_lzma_config(struct super_block *sb,
			     struct erofs_super_block *dsb,
			     struct z_erofs_lzma_cfgs *lzma, int size);
#else
static inline int z_erofs_lzma_init(void) { return 0; }
static inline int z_erofs_lzma_exit(void) { return 0; }
static inline int z_erofs_load_lzma_config(struct super_block *sb,
			     struct erofs_super_block *dsb,
			     struct z_erofs_lzma_cfgs *lzma, int size) {
	if (lzma) {
		erofs_err(sb, "lzma algorithm isn't enabled");
		return -EINVAL;
	}
	return 0;
}
#endif	/* !CONFIG_AMLOGIC_EROFS_ZIP */

#define EFSCORRUPTED    EUCLEAN         /* Filesystem is corrupted */

// GKI symbol handle
#ifdef CONFIG_FS_DAX
int erofs_file_mmap(struct file *file, struct vm_area_struct *vma);
#endif
typedef void *(*__xa_cmpxchg_t)(struct xarray *xa, unsigned long index, void *old, void *entry, gfp_t gfp);
typedef unsigned char (*fs_ftype_to_dtype_t)(unsigned int filetype);
typedef ssize_t (*filemap_read_t)(struct kiocb *iocb, struct iov_iter *iter,ssize_t already_read);
typedef ssize_t (*iomap_dio_rw_t)(struct kiocb *iocb, struct iov_iter *iter,
				  const struct iomap_ops *ops, const struct iomap_dio_ops *dops,
				  unsigned int dio_flags, size_t done_before);
typedef sector_t (*iomap_bmap_t)(struct address_space *mapping, sector_t bno, struct iomap_ops *ops);
typedef void (*iomap_readahead_t)(struct readahead_control *rac, const struct iomap_ops *ops);
typedef int (*iomap_readpage_t)(struct page *page, const struct iomap_ops *ops);
typedef int (*generic_file_readonly_mmap_t)(struct file *file, struct vm_area_struct *vma);
typedef ssize_t (*noop_direct_IO_t)(struct kiocb *iocb, struct iov_iter *iter);
typedef int (*iomap_fiemap_t)(struct inode *inode, struct fiemap_extent_info *fi,
			      u64 start, u64 len, const struct iomap_ops *ops);
typedef struct page *(*read_cache_page_gfp_t)(struct address_space *mapping,
					      pgoff_t index, gfp_t gfp);
typedef u32 (*crc32c_t)(u32 crc, const void *address, unsigned int length);
typedef int (*add_to_page_cache_lru_t)(struct page *page, struct address_space *mapping,
				       pgoff_t offset, gfp_t gfp_mask);
typedef void (*readahead_expand_t)(struct readahead_control *ractl, loff_t new_start, size_t new_len);
typedef struct kthread_worker *(*kthread_create_worker_on_cpu_t)(int cpu, unsigned int flags,
				const char namefmt[], ...);
typedef int (*LZ4_decompress_safe_t)(const char *source, char *dest, int compressedSize,
				     int maxDecompressedSize);
typedef int (*LZ4_decompress_safe_partial_t)(const char *src, char *dst, int compressedSize,
					     int targetOutputSize, int dstCapacity);
typedef int (*out_of_line_wait_on_bit_lock_t)(void *word, int bit, wait_bit_action_f *action, unsigned mode);
typedef struct posix_acl *(*posix_acl_from_xattr_tt)(struct user_namespace *user_ns,
						   const void *value, size_t size);
typedef const char *(*page_get_link_t)(struct dentry *dentry, struct inode *inode,
				      struct delayed_call *callback);
typedef const char *(*simple_get_link_t)(struct dentry *dentry, struct inode *inode, struct delayed_call *done);
typedef int (*fs_param_is_enum_t)(struct p_log *log, const struct fs_parameter_spec *p,
				  struct fs_parameter *param, struct fs_parse_result *result);

extern struct file_operations *generic_ro_fops_t;
extern struct xattr_handler *posix_acl_default_xattr_handler_t;
extern struct xattr_handler *posix_acl_access_xattr_handler_t;

extern __xa_cmpxchg_t			f___xa_cmpxchg;
extern fs_ftype_to_dtype_t		f_fs_ftype_to_dtype;
extern filemap_read_t			f_filemap_read;
extern iomap_dio_rw_t			f_iomap_dio_rw;
extern iomap_bmap_t			f_iomap_bmap;
extern iomap_readahead_t		f_iomap_readahead;
extern iomap_readpage_t			f_iomap_readpage;
extern generic_file_readonly_mmap_t	f_generic_file_readonly_mmap;
extern noop_direct_IO_t			f_noop_direct_IO;
extern iomap_fiemap_t			f_iomap_fiemap;
extern read_cache_page_gfp_t		f_read_cache_page_gfp;
extern page_get_link_t			f_page_get_link;
extern simple_get_link_t		f_simple_get_link;
extern crc32c_t				f_crc32c;
extern add_to_page_cache_lru_t		f_add_to_page_cache_lru;
extern readahead_expand_t		f_readahead_expand;
extern kthread_create_worker_on_cpu_t	f_kthread_create_worker_on_cpu;
extern LZ4_decompress_safe_t		f_LZ4_decompress_safe;
extern LZ4_decompress_safe_partial_t	f_LZ4_decompress_safe_partial;
extern out_of_line_wait_on_bit_lock_t	f_out_of_line_wait_on_bit_lock;
extern fs_param_is_enum_t		f_fs_param_is_enum;
//extern posix_acl_from_xattr_tt		posix_acl_from_xattr_t;
extern unsigned long			posix_acl_from_xattr_t;



static inline int __nocfi
wait_on_bit_lock_t(unsigned long *word, int bit, unsigned mode)
{
        might_sleep();
        if (!test_and_set_bit(bit, word))
                return 0;
        return f_out_of_line_wait_on_bit_lock(word, bit, bit_wait, mode);
}

int symbol_fix(void);
#endif	/* __EROFS_INTERNAL_H */
