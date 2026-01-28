// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include "isdbt_func.h"
#include "demod_func.h"

static unsigned int bit32_reverse(unsigned int x)
{
	x = (x & 0x55555555) << 1 | ((x >> 1) & 0x55555555);
	x = (x & 0x33333333) << 2 | ((x >> 2) & 0x33333333);
	x = (x & 0x0F0F0F0F) << 4 | ((x >> 4) & 0x0F0F0F0F);
	x = (x << 24) | ((x & 0xFF00) << 8) | ((x >> 8) & 0xFF00) | (x >> 24);

	return x;
}

static unsigned int bitn_reverse(unsigned int x, unsigned char bits)
{
	unsigned int i = 0, v = 0;

	for (i = 0; i < bits; i++) {
		v = 1 & x;
		v <<= 1;
		x >>= 1;
	}

	return v;
}

void isdbt_get_tmcc_info(struct isdbt_tmcc_info *tmcc_info)
{
	unsigned int reg_0x64 = 0, reg_0x65 = 0, reg_0x66 = 0, reg_0x67 = 0;
	unsigned int B20_25 = 0, B26_57 = 0, B58_89 = 0, B90_121 = 0;
	struct isdbt_tmcc_info info;

	reg_0x64 = dvbt_isdbt_rd_reg_new(0x64);
	reg_0x65 = dvbt_isdbt_rd_reg_new(0x65);
	reg_0x66 = dvbt_isdbt_rd_reg_new(0x66);
	reg_0x67 = dvbt_isdbt_rd_reg_new(0x67);

	PR_ISDBT("reg 0x64 0x%x, 0x65 0x%x, 0x66 0x%x, 0x67 0x%x\n",
			reg_0x64, reg_0x65, reg_0x66, reg_0x67);

	B90_121 = bit32_reverse(reg_0x64);
	B58_89 = bit32_reverse(reg_0x65);
	B26_57 = bit32_reverse(reg_0x66);
	B20_25 = bitn_reverse(reg_0x67, 6) & 0x3f;

	PR_ISDBT("after bit_reverse B20_25 0x%x, B26_57 0x%x, B58_89 0x%x, B90_121 0x%x\n",
			B20_25, B26_57, B58_89, B90_121);

	info.system_id = B20_25 & 0x3;
	info.tp_switch = (B20_25 >> 2) & 0xf;
	info.ews_flag = B26_57 & 0x1;

	info.current_info.is_partial = (B26_57 >> 1) & 0x1;
	info.current_info.layer_a.modulation = (B26_57 >> 2) & 0x7;
	info.current_info.layer_a.coding_rate = (B26_57 >> 5) & 0x7;
	info.current_info.layer_a.il_length = (B26_57 >> 8) & 0x7;
	info.current_info.layer_a.segments_num = (B26_57 >> 11) & 0xf;

	info.current_info.layer_b.modulation = (B26_57 >> 15) & 0x7;
	info.current_info.layer_b.coding_rate = (B26_57 >> 18) & 0x7;
	info.current_info.layer_b.il_length = (B26_57 >> 21) & 0x7;
	info.current_info.layer_b.segments_num = (B26_57 >> 24) & 0xf;

	info.current_info.layer_c.modulation = (B26_57 >> 28) & 0x7;
	info.current_info.layer_c.coding_rate = ((B26_57 >> 31) & 0x1) | ((B58_89 & 0x2) << 2);
	info.current_info.layer_c.il_length = (B58_89 >> 2) & 0x7;
	info.current_info.layer_c.segments_num = (B58_89 >> 5) & 0xf;

	info.next_info.is_partial = (B58_89 >> 9) & 0x1;
	info.next_info.layer_a.modulation = (B58_89 >> 10) & 0x7;
	info.next_info.layer_a.coding_rate = (B58_89 >> 13) & 0x7;
	info.next_info.layer_a.il_length = (B58_89 >> 16) & 0x7;
	info.next_info.layer_a.segments_num = (B58_89 >> 19) & 0xf;

	info.next_info.layer_b.modulation = (B58_89 >> 23) & 0x7;
	info.next_info.layer_b.coding_rate = (B58_89 >> 26) & 0x7;
	info.next_info.layer_b.il_length = (B58_89 >> 29) & 0x7;
	info.next_info.layer_b.segments_num = B90_121 & 0xf;

	info.next_info.layer_c.modulation = (B90_121 >> 4) & 0x7;
	info.next_info.layer_c.coding_rate = (B90_121 >> 7) & 0x7;
	info.next_info.layer_c.il_length = (B90_121 >> 10) & 0x7;
	info.next_info.layer_c.segments_num = (B90_121 >> 13) & 0xf;

	info.phase_shift = (B90_121 >> 17) & 0x7;
	info.reserved = (B90_121 >> 20) & 0xfff;

	PR_ISDBT("system_id %d, tp_switch %d, ews_flag  %d, phase_shift %d, reserved %d\n",
		info.system_id, info.tp_switch, info.ews_flag, info.phase_shift, info.reserved);

	PR_ISDBT("Current_info:is_partial=%d\n", info.current_info.is_partial);
	PR_ISDBT("[layer A]:modulation=%d, coding_rate=%d, il_length=%d, segments_num=%d\n",
		info.current_info.layer_a.modulation,
		info.current_info.layer_a.coding_rate,
		info.current_info.layer_a.il_length,
		info.current_info.layer_a.segments_num);
	PR_ISDBT("[layer B]:modulation=%d, coding_rate=%d, il_length=%d, segments_num=%d\n",
		info.current_info.layer_b.modulation,
		info.current_info.layer_b.coding_rate,
		info.current_info.layer_b.il_length,
		info.current_info.layer_b.segments_num);
	PR_ISDBT("[layer C]:modulation=%d, coding_rate=%d, il_length=%d, segments_num=%d\n",
		info.current_info.layer_c.modulation,
		info.current_info.layer_c.coding_rate,
		info.current_info.layer_c.il_length,
		info.current_info.layer_c.segments_num);

	PR_ISDBT("Next_info:is_partial=%d\n", info.next_info.is_partial);
	PR_ISDBT("[layer A]:modulation=%d, coding_rate=%d, il_length=%d, segments_num=%d\n",
		info.next_info.layer_a.modulation,
		info.next_info.layer_a.coding_rate,
		info.next_info.layer_a.il_length,
		info.next_info.layer_a.segments_num);
	PR_ISDBT("[layer B]:modulation=%d, coding_rate=%d, il_length=%d, segments_num=%d\n",
		info.next_info.layer_b.modulation,
		info.next_info.layer_b.coding_rate,
		info.next_info.layer_b.il_length,
		info.next_info.layer_b.segments_num);
	PR_ISDBT("[layer C]:modulation=%d, coding_rate=%d, il_length=%d, segments_num=%d\n",
		info.next_info.layer_c.modulation,
		info.next_info.layer_c.coding_rate,
		info.next_info.layer_c.il_length,
		info.next_info.layer_c.segments_num);

	if (tmcc_info)
		memcpy(tmcc_info, &info, sizeof(struct isdbt_tmcc_info));
}
