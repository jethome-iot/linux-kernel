/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _CODEC_MM_SYS_H
#define _CODEC_MM_SYS_H

int init_alloc_page_boost_task(void);

unsigned long aml_alloc_pages_array(gfp_t gfp, unsigned long nr_pages, struct page **page_array);

void aml_free_pages_array(unsigned long nr_pages, struct page **page_array);

#endif //_CODEC_MM_SYS_H

