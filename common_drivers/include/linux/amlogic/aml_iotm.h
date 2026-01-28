/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_IOTM_H
#define __AML_IOTM_H

enum iotm_sw_type {
	IOTM_SW_SCHED_BEGIN,
	IOTM_SW_SCHED_END,
	IOTM_SW_IRQ_IN,
	IOTM_SW_IRQ_OUT,
	IOTM_SW_SMC_IN,
	IOTM_SW_SMC_OUT,
	IOTM_SW_SMC_NORET_IN,
};

#if IS_ENABLED(CONFIG_AMLOGIC_DEBUG_IOTM)
int iotm_init(void);
int iotm_en_ddr_size_get(int *iotm_ddr_size);
int iotm_monitor_mode_get(void);
void iotm_sw_record_write(u32 sw_type, u32 val1, u32 val2);

static inline void iotm_sched_record_write(char *comm)
{
	u32 comm_1 = 0, comm_2 = 0;

	((char *)&comm_1)[0] = comm[0];
	((char *)&comm_1)[1] = comm[1];
	((char *)&comm_2)[0] = comm[2];
	((char *)&comm_2)[1] = comm[3];
	((char *)&comm_2)[2] = comm[4];
	((char *)&comm_2)[3] = comm[5];
	iotm_sw_record_write(IOTM_SW_SCHED_BEGIN, comm_1, comm_2);

	((char *)&comm_1)[0] = comm[6];
	((char *)&comm_1)[1] = comm[7];
	((char *)&comm_2)[0] = comm[8];
	((char *)&comm_2)[1] = comm[9];
	((char *)&comm_2)[2] = comm[10];
	((char *)&comm_2)[3] = comm[11];
	iotm_sw_record_write(IOTM_SW_SCHED_END, comm_1, comm_2);
}
#else
static inline int iotm_init(void)
{
	return 0;
}

static inline int iotm_en_ddr_size_get(int *iotm_ddr_size)
{
	return 0;
}

static inline int iotm_monitor_mode_get(void)
{
	return 0;
}

static inline void iotm_sw_record_write(u32 sw_type, u32 val1, u32 val2)
{
}

static inline void iotm_sched_record_write(char *comm)
{
}
#endif

#endif  /* __AML_IOTM_H */
