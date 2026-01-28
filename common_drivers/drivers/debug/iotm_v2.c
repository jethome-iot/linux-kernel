// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/irqreturn.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_device.h>
#include <linux/sched/clock.h>
#include <linux/sysrq.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/amlogic/gki_module.h>
#include <trace/hooks/traps.h>
#include <linux/amlogic/aml_iotm.h>
#include <linux/panic_notifier.h>
#include <linux/timer.h>
#include <linux/syscore_ops.h>
#include <linux/hardirq.h>
#include <asm/arch_timer.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include "iotm_hw.h"

static void sw_record_write_v2(u32 sw_type, u32 val1, u32 val2)
{
	struct iotm_record_v2 record_v2;

	record_v2.sw_stream2.sw_type = sw_type;
	record_v2.sw_stream2.cpu = smp_processor_id();
	record_v2.sw_stream2.reserved = val1;
	record_v2.stream3 = val2;

	writel(record_v2.stream3,
		iotm.cssys_base + SW_DATA_STREAM0);
	writel(record_v2.stream2 | SW_DATA_STREAM1_WRITE,
	iotm.cssys_base + SW_DATA_STREAM1);
}

static u64 ts_to_kernel_time_v2(u64 ts)
{
	u64 kernel_time;
	u64 time_offset;
	u32 high, low;
	void *base_addr = NULL;

	if (!iotm.timeout_irq_handled && iotm.supported)
		/* It is neither automatically printed after timeout nor after reboot. */
		base_addr = iotm.saved_trace - ADDR_RANGE0_BEGIN;
	else
		base_addr = iotm.cssys_base;

	high = readl(base_addr + ADDR_RANGE6_BEGIN);
	low = readl(base_addr + ADDR_RANGE6_END);
	time_offset = PACK_U32_TO_U64(high, low);

	kernel_time = ts - time_offset;
	return kernel_time;
}

static int print_single_trace_v2(void *ptr, char *buf)
{
	u8 sched_comm[7] = {0};
	int pos = 0;
	struct iotm_record_v2 *record = ptr;
	u64 ts = ((u64)record->stream0 + record->stream1.ts) * NSEC_PER_IOTM_TS;
	u64 kernel_time = ts_to_kernel_time_v2(ts);
	u64 rem_nsec = do_div(kernel_time, 1000000000);
	u32 sched_stream2;

	do_div(rem_nsec, 1000);
	pos += sprintf(buf + pos, "[%05llu.%06llu] <%02d> ",
			kernel_time, rem_nsec, record->stream1.idx);

	if (record->stream1.mid == 1 && record->sw_stream2.mid == 0) {
		/* The source of the data is software */
		switch (record->sw_stream2.sw_type) {
		case IOTM_SW_IRQ_IN:
		case IOTM_SW_IRQ_OUT:
			pos += sprintf(buf + pos, "<%s %d> ",
					sw_record_name[record->sw_stream2.sw_type],
					record->stream3);
			break;
		case IOTM_SW_SMC_IN:
		case IOTM_SW_SMC_OUT:
		case IOTM_SW_SMC_NORET_IN:
			pos += sprintf(buf + pos, "<%s 0x%x 0x%x> ",
					sw_record_name[record->sw_stream2.sw_type],
					record->stream3,
					record->sw_stream2.reserved);
			break;
		case IOTM_SW_SCHED_BEGIN:
		case IOTM_SW_SCHED_END:
			sched_stream2 = record->sw_stream2.reserved;
			sched_comm[0] = ((char *)&sched_stream2)[0];
			sched_comm[1] = ((char *)&sched_stream2)[1];
			sched_comm[2] = ((char *)&record->stream3)[0];
			sched_comm[3] = ((char *)&record->stream3)[1];
			sched_comm[4] = ((char *)&record->stream3)[2];
			sched_comm[5] = ((char *)&record->stream3)[3];
			sched_comm[6] = '\0';

			pos += sprintf(buf + pos, "<%s next:%s> ",
				sw_record_name[record->sw_stream2.sw_type], sched_comm);
			break;
		default:
			break;
		}
		pos += sprintf(buf + pos, "<cpu%d>", record->sw_stream2.cpu);
	} else {
		char *node_name =
			find_register_node(record->io_stream2.addr + ADDR_OFFSET);

		/* The source of the data is hardware */
		pos += sprintf(buf + pos, "<%s %s %08x-%08x> %s<%s>",
				record->io_stream2.rw ? "IOTM-W" : "IOTM-R",
				record->io_stream2.fail ? "fail" : "",
				record->io_stream2.addr + ADDR_OFFSET,
				record->stream3,
				record->io_stream2.mid == AOCPU_TRACE ? "AOCPU" : "",
				node_name);

		kfree(node_name);
	}
	pos += sprintf(buf + pos, "\n");
	return record->stream1.idx;
}

static bool is_trace_loop_v2(void)
{
	if (readl(iotm.cssys_base + IOTM_AXI_SIZE) ==
		readl(iotm.cssys_base + MONITOR_BUF_SIZE_LOW))
		return true;

	return false;
}

static inline void etb_coresight_clk_v2(void)
{
	writel(FUNNEL_CTRL_REG_VAL_V2, iotm.cssys_base + FUNNEL_CTRL_REG);
}

/*
 * Different versions have different status detections for watchdogs
 * if watchdog reset:
 * v1: wdt irq pull up
 * v2: status change
 */
static void detect_watchdog_status_v2(void)
{
	u32 monitor_status = readl(iotm.cssys_base + MONITOR_STATUS);

	if (monitor_status & MONITOR_STATUS_WDT_RESET)
		iotm.dump_mode = IOTM_DUMP_WATCHDOG;
}

static void boot_time_record_v2(u64 pct, u64 ns_time)
{
	u64 tmp_pct = pct * 1000;
	u64 pos;

	do_div(tmp_pct, 24);
	pos = tmp_pct - ns_time;

	writel((u32)(pos >> 32), iotm.cssys_base + ADDR_RANGE6_BEGIN);
	writel((u32)pos, iotm.cssys_base + ADDR_RANGE6_END);
	pr_err("iotm:TS1 = %llu, ns_time = %llu, pos = %llu\n",
			pct, ns_time, pos);
}

static void ddr_range_get_v2(u32 *reg_base, void *buf, int *pos)
{
	*pos += sprintf(buf + *pos, "IOTM:AXI:ddr_range:[%x, %x]\n",
		reg_base[(MONITOR_BUF_BASEADDR_LOW - ADDR_RANGE0_BEGIN) >> 2],
		reg_base[(MONITOR_BUF_BASEADDR_LOW - ADDR_RANGE0_BEGIN) >> 2] +
		reg_base[(MONITOR_BUF_SIZE_LOW - ADDR_RANGE0_BEGIN) >> 2]);
}

static void ddr_range_set_v2(void)
{
	writel(iotm.buf_end - iotm.buf_start + 1,
		iotm.cssys_base + MONITOR_BUF_SIZE_LOW);
}

struct iotm_ops iotm_v2_ops = {
	.ddr_range_set = ddr_range_set_v2,
	.etb_coresight_clk = etb_coresight_clk_v2,
	.boot_time_record = boot_time_record_v2,
	.detect_watchdog_status = detect_watchdog_status_v2,
	.ddr_range_get = ddr_range_get_v2,
	.is_trace_loop = is_trace_loop_v2,
	.print_single_trace = print_single_trace_v2,
	.sw_record_write = sw_record_write_v2,
};
