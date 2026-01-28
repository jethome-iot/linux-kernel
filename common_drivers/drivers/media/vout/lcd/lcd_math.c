// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/amlogic/media/vout/lcd/lcd_math.h>

struct s32_slide_filter_s *alloc_slide_filter(int data_cnt)
{
	struct s32_slide_filter_s *ft;

	ft = kzalloc(sizeof(*ft), GFP_KERNEL);
	if (!ft)
		return NULL;

	ft->len = data_cnt;
	ft->data = kcalloc(ft->len, sizeof(int), GFP_KERNEL);
	if (!ft->data) {
		kfree(ft);
		return NULL;
	}

	return ft;
}

void free_slider_filter(struct s32_slide_filter_s *ft)
{
	if (!ft)
		return;
	kfree(ft->data);
	kfree(ft);
}

int init_slide_filter(struct s32_slide_filter_s *ft, int data_cnt)
{
	if (!ft)
		return -1;

	ft->cnt = 0;
	ft->sum = 0;
	ft->avg = 0;
	ft->full = 0;
	ft->rst = 0;
	ft->len = data_cnt;
	ft->data = kcalloc(ft->len, sizeof(int), GFP_KERNEL);
	if (!ft->data) {
		ft->len = 0;
		ft->avg = 0;
		return -1;
	}

	return 0;
}

int slide_filter(struct s32_slide_filter_s *ft, int data)
{
	int cnt, i = 0, old;
	unsigned long long temp;

	if (!ft || !ft->len)
		return 0;

	if (ft->rst) {
		ft->cnt = 0;
		ft->sum = 0;
		ft->avg = 0;
		ft->full = 0;
		ft->rst = 0;
		memset(ft->data, 0, sizeof(int) * ft->len);
		return 0;
	}

	if (ft->cnt >= ft->len) {
		ft->cnt = 0;
		ft->full = 1;
	}

	/* a optimize point in the future, construct sliding window filter */
	old = ft->data[ft->cnt];
	ft->data[ft->cnt] = data;
	if (ft->full)
		cnt = ft->len;
	else
		cnt = ft->cnt + 1;

	ft->sum += ft->data[ft->cnt];
	ft->sum -= old;

	temp = ft->sum > 0 ? ft->sum : -ft->sum;
	/* strongly suggest use base 2 exponential number*/
	if ((cnt & -cnt) == cnt) {
		for (i = 0; !(cnt & 0x1); cnt >>= 1, i++)
			;
		ft->avg = temp >> i;
	} else {
		ft->avg = lcd_do_div(temp, cnt);
	}
	ft->avg = ft->sum > 0 ? ft->avg : -ft->avg;
	ft->cnt++;

	return ft->avg;
}

void reset_slide_filter(struct s32_slide_filter_s *ft)
{
	if (!ft || !ft->data)
		return;
	ft->rst = 1;
	slide_filter(ft, 0);
}

unsigned int cal_crc32(unsigned int crc, const unsigned char *buf, int buf_len)
{
	static unsigned int s_crc32[16] = {
		0, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
		0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
		0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
		0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c,
	};
	unsigned int crcu32 = crc;
	unsigned char b;

	if (buf_len <= 0)
		return 0;
	if (!buf)
		return 0;

	crcu32 = ~crcu32;
	while (buf_len--) {
		b = *buf++;
		crcu32 = (crcu32 >> 4) ^ s_crc32[(crcu32 & 0xf) ^ (b & 0xf)];
		crcu32 = (crcu32 >> 4) ^ s_crc32[(crcu32 & 0xf) ^ (b >> 4)];
	}

	return ~crcu32;
}
