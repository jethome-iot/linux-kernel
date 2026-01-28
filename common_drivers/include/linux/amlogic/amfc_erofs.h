/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#ifndef __AMFC_EROFS_H__
#define __AMFC_EROFS_H__

struct z_erofs_decompress_req;

#ifdef CONFIG_AMLOGIC_EROFS
int z_erofs_zstd_init(void);
void z_erofs_zstd_exit(void);
int z_erofs_load_zstd_config(struct super_block *sb,
			     struct erofs_super_block *dsb,
			     struct z_erofs_zstd_cfgs *zstd, int size);
int z_erofs_zstd_decompress(struct z_erofs_decompress_req *rq,
			    struct page **pagepool);
#else
static inline int z_erofs_zstd_init(void) { return 0; }
static inline int z_erofs_zstd_exit(void) { return 0; }
static inline int z_erofs_load_zstd_config(struct super_block *sb,
			     struct erofs_super_block *dsb,
			     struct z_erofs_zstd_cfgs *zstd, int size) {
	if (zstd) {
		erofs_err(sb, "zstd algorithm isn't enabled");
		return -EINVAL;
	}
	return 0;
}

static inline int z_erofs_zstd_decompress(struct z_erofs_decompress_req *rq,
					  struct page **pagepool)
{
	return -EINVAL;
}
#endif	/* !CONFIG_AMLOGIC_EROFS*/

#endif /* __AMFC_EROFS_H__ */
