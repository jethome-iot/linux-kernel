/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _CODEC_MM_PREALLOC_PRIV_H
#define _CODEC_MM_PREALLOC_PRIV_H

struct codec_mm_s *get_mms_from_hashtable(u32 key, int align_2n);
void release_prealloc_job(void);
int init_prealloc_boost_task(void);
#endif //_CODEC_MM_PREALLOC_PRIV_H

