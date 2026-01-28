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
#include <linux/arm-smccc.h>
#include <linux/panic_notifier.h>
#include <linux/timer.h>
#include <linux/syscore_ops.h>
#include <linux/hardirq.h>
#include <asm/arch_timer.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include "iotm_hw.h"

#define IOTM_DISABLE				0
#define IOTM_ENABLE				1
#define IOTM_ENABLE_NO_TRACE			2
#define AML_IOTM_SMC_CMD			0x8200007A
#define AML_IOTM_INIT_SMC_ARG			0x1
#define AML_IOTM_RANGE_SMC_ARG			0x2
#define AML_IOTM_JTAG_SMC_ARG			0x3
#define AML_IOTM_JTAG_DISABLE_SMC_ARG		0x0
#define AML_IOTM_JTAG_ENABLE_SMC_ARG		0x1
#define IOTM_IDX_RANGE				16
#define IOTM_FAIL_TRACE_CNT_MAX			5
#define IOTM_DUMP_TRACE_CNT			100
#define ETB_SIZE				(8 * 1024)
#define TRACE_BUF_SIZE				512
#define IOTM_MONITOR_RANGE_MAX			0xFFFFFFFF
#define IOTM_MONITOR_RANGE_MIN			0xE0000000
#define CONTINUOUS_PULSE			20
#define CONTINUOUS_PULSE_TIME			10
#define IOTM_V1					0x1
#define IOTM_V2					0x2

enum iotm_mode {
	AXI_MODE,
	TPIU_MODE,
	ETB_MODE,
};

struct reg_info {
	phys_addr_t start;
	phys_addr_t end;
	struct list_head list;
};

struct node_info {
	char name[32];
	struct list_head reg_list;
	struct list_head list;
};

static LIST_HEAD(nodes_list);

struct iotm iotm = {
	.dump_cnt = IOTM_DUMP_TRACE_CNT,
};

static int iotm_en = IOTM_ENABLE;

const char *sw_record_name[] = {
	[IOTM_SW_SCHED_BEGIN] = "IOTM_SCHED_SWITCH_BEGIN",
	[IOTM_SW_SCHED_END] = "IOTM_SCHED_SWITCH_END",
	[IOTM_SW_IRQ_IN] = "IOTM_ISR_IN",
	[IOTM_SW_IRQ_OUT] = "IOTM_ISR_OUT",
	[IOTM_SW_SMC_IN] = "IOTM_SMC_IN",
	[IOTM_SW_SMC_OUT] = "IOTM_SMC_OUT",
	[IOTM_SW_SMC_NORET_IN] = "IOTM_SMC_NORET_IN",
};

/* Obtain the operating mode of the IOTM */
int iotm_monitor_mode_get(void)
{
	return iotm.monitor_mode;
}
EXPORT_SYMBOL(iotm_monitor_mode_get);

/* Obtain the DDR size and status of the IOTM */
int iotm_en_ddr_size_get(int *iotm_ddr_size)
{
	*iotm_ddr_size = iotm.buf_end - iotm.buf_start + 1;
	return iotm.supported;
}
EXPORT_SYMBOL(iotm_en_ddr_size_get);

static int iotm_en_set(const char *val, const struct kernel_param *kp)
{
	u32 iotm_ctrl_mode_val;
	struct arm_smccc_res res;

	if (!iotm.supported) {
		pr_err("IOTM:not support in this board\n");
		return -EINVAL;
	}

	if (kstrtoint(val, 0, &iotm_en)) {
		pr_err("IOTM:iotm_en error: %s\n", val);
		return -EINVAL;
	}
	iotm_ctrl_mode_val = readl(iotm.cssys_base + IOTM_CTRL_MODE);

	switch (iotm_en) {
	case IOTM_DISABLE:
		iotm_ctrl_mode_val |= IOTM_CTRL_MODE_TRACE_DISABLE;

		pr_info("IOTM:has been disabled\n");
		break;
	case IOTM_ENABLE:
	/* monitor vapb4 and capu bus then start trace data */
		iotm_smccc_smc(AML_IOTM_SMC_CMD, AML_IOTM_JTAG_SMC_ARG,
			AML_IOTM_JTAG_ENABLE_SMC_ARG, 0, 0, res);
		iotm_ctrl_mode_val |= (IOTM_CTRL_MODE_CAPU_ENABLE |
			IOTM_CTRL_MODE_VAPB4_ENABLE | IOTM_CTRL_MODE_TRACE_ENABLE);

		pr_info("IOTM:has been enabled and can record trace.\n");
		break;
	case IOTM_ENABLE_NO_TRACE:
		iotm_smccc_smc(AML_IOTM_SMC_CMD, AML_IOTM_JTAG_SMC_ARG,
			AML_IOTM_JTAG_DISABLE_SMC_ARG, 0, 0, res);
		iotm_ctrl_mode_val |= (IOTM_CTRL_MODE_CAPU_ENABLE |
			IOTM_CTRL_MODE_VAPB4_ENABLE | IOTM_CTRL_MODE_TRACE_ENABLE);

		pr_info("IOTM:has been enabled, but can't record trace, just trigger timeout\n");
		break;
	default:
		pr_err("IOTM:iotm_en error: %s\n", val);
		break;
	}

	writel(iotm_ctrl_mode_val, iotm.cssys_base + IOTM_CTRL_MODE);

	return 0;
}

static const struct kernel_param_ops iotm_en_ops = {
	.set = iotm_en_set,
	.get = param_get_int,
};
module_param_cb(iotm_en, &iotm_en_ops, &iotm_en, 0644);

static int iotm_en_setup(char *buf)
{
	if (!buf)
		return -EINVAL;

	if (kstrtoint(buf, 0, &iotm_en)) {
		pr_err("IOTM:iotm_en error: %s\n", buf);
		return -EINVAL;
	}

	return 1;
}
__setup("iotm_en=", iotm_en_setup);

static int iotm_dump_mode_setup(char *buf)
{
	if (!buf)
		return -EINVAL;

	if (kstrtouint(buf, 0, &iotm.dump_mode) || iotm.dump_mode > 2) {
		pr_err("IOTM:iotm.dump_mode error: %s\n", buf);
		iotm.dump_mode = IOTM_DUMP_WATCHDOG;
		return -EINVAL;
	}

	return 1;
}
__setup("iotm_dump_mode=", iotm_dump_mode_setup);

static int iotm_monitor_mode_setup(char *buf)
{
	if (!buf)
		return -EINVAL;

	if (kstrtouint(buf, 0, &iotm.monitor_mode) || iotm.monitor_mode > 2) {
		pr_err("IOTM:iotm_monitor_mode error: %s\n", buf);
		iotm.monitor_mode = 0;
		return -EINVAL;
	}

	return 1;
}
__setup("iotm_monitor_mode=", iotm_monitor_mode_setup);

static int iotm_dump_cnt_setup(char *buf)
{
	if (!buf)
		return -EINVAL;

	if (kstrtouint(buf, 0, &iotm.dump_cnt)) {
		pr_err("IOTM:iotm.dump_cnt error: %s\n", buf);
		return -EINVAL;
	}

	return 1;
}
__setup("iotm_dump_cnt=", iotm_dump_cnt_setup);

/* interface for the software to record data */
void iotm_sw_record_write(u32 sw_type, u32 val1, u32 val2)
{
	unsigned long flags;

	if (iotm_en != IOTM_ENABLE || !iotm.supported)
		return;

	spin_lock_irqsave(&iotm.record_lock, flags);
	iotm.ops->sw_record_write(sw_type, val1, val2);
	spin_unlock_irqrestore(&iotm.record_lock, flags);
}
EXPORT_SYMBOL(iotm_sw_record_write);

char *find_register_node(u32 addr)
{
	struct node_info *mod;
	struct reg_info *reg;
	char *result = NULL;
	int result_len = 0;

	result = kzalloc(TRACE_BUF_SIZE, GFP_KERNEL);
	if (!result)
		return NULL;

	list_for_each_entry(mod, &nodes_list, list) {
		list_for_each_entry(reg, &mod->reg_list, list) {
			if (addr >= reg->start && addr <= reg->end) {
				u32 mod_name_len = strlen(mod->name);

				if (result_len > 0) {
					result[result_len] = '/';
					result_len++;
				}
				strncpy(result + result_len, mod->name, mod_name_len);
				result_len += mod_name_len;

				break;
			}
		}
	}

	return result;
}

/* Indicates the number of consecutive errors in the trace information */
static int print_correct_trace(void *ptr, char *buf)
{
	static int last_idx;
	int idx = iotm.ops->print_single_trace(ptr, buf);

	/*
	 * The trace data may be incorrect.
	 * Check whether the IDX of the trace is in order[0-15]
	 */
	if (idx != (last_idx + 1) % IOTM_IDX_RANGE)
		iotm.fail_trace_cnt++;

	last_idx = idx;

	if (iotm.fail_trace_cnt >= IOTM_FAIL_TRACE_CNT_MAX) {
		pr_err("IOTM:trace buf is error\n");
		return -1;
	}

	return 0;
}

static void print_ddr_buf(int *cnt, int trace_cnt, void *trace_start, void *trace_end)
{
	char *buf;

	buf = kmalloc(TRACE_BUF_SIZE, GFP_KERNEL);
	if (!buf)
		return;

	while (trace_start < trace_end) {
		if (print_correct_trace(trace_start, buf))
			break;

		if (*cnt > trace_cnt)
			pr_err("%s", buf);

		(*cnt)++;
		trace_start += iotm.bytes_per_trace;
	}
	kfree(buf);
}

/* Ddr buffer can't be confirmed to have been overwritten.
 * So the data has been overwritten defaultly when data is dumped.
 * ddr size needs to be set to a multiple of 48 to confirm corrected.
 */
static void auto_dump_ddr_buf(void)
{
	unsigned int iotm_ddr_size = iotm.buf_end - iotm.buf_start + 1;
	unsigned int iotm_axi_addr_val = readl(iotm.cssys_base + IOTM_AXI_ADDR);
	unsigned int last_seg_size = iotm_axi_addr_val - iotm.buf_start;
	int trace_cnt = iotm_ddr_size / iotm.bytes_per_trace;
	void *vaddr;
	void *trace_start, *trace_end;
	u32 offset = 0;
	int cnt = 0;

	offset = last_seg_size % iotm.bytes_per_trace;
	if (offset)
		last_seg_size += iotm.bytes_per_trace - offset;

	if (iotm_axi_addr_val < iotm.buf_start || iotm_axi_addr_val > iotm.buf_end) {
		pr_err("IOTM:error!IOTM_AXI_ADDR=0x%x, ddr_buf=[0x%x, 0x%x]\n",
				iotm_axi_addr_val, iotm.buf_start, iotm.buf_end);
		return;
	}

	vaddr = phys_to_virt(iotm.buf_start);
	if (!vaddr) {
		pr_err("IOTM: NOMEM\n");
		return;
	}

	trace_cnt = trace_cnt - iotm.dump_cnt;
	if (iotm.ops->is_trace_loop()) {
		trace_start = vaddr + last_seg_size;
		trace_end = vaddr + iotm_ddr_size;

		print_ddr_buf(&cnt, trace_cnt, trace_start, trace_end);
	}

	iotm.fail_trace_cnt = 0;
	trace_start = vaddr;
	trace_end = vaddr + last_seg_size;
	print_ddr_buf(&cnt, trace_cnt, trace_start, trace_end);
}

static void auto_dump_etb_buf(void)
{
	u32 write_ptr = 0;
	u32 depth;
	int i, j;
	int words_per_trace = iotm.bytes_per_trace / sizeof(u32);
	u32 iotm_trace[4];
	u8 *buf;

	buf = kmalloc(TRACE_BUF_SIZE, GFP_KERNEL);
	if (!buf)
		return;

	// stop etb trace
	writel(0x0, iotm.cssys_base + ETB_CTL);

	depth = readl(iotm.cssys_base + ETB_DEPTH);

	/* -4: T6D/T6W chip designs that RWP will take 4 more steps. */
	if (!(readl(iotm.cssys_base + ETB_STATUS) & 0x1)) {
		/* etb buffer is not full, need to read from 0 */
		iotm.etb_buf_words_cnt = readl(iotm.cssys_base + ETB_RWP) - 4;
		writel(0x0, iotm.cssys_base + ETB_RRP);
	} else {
		/*
		 * etb buffer is full. need to read from ETB_RWP.
		 * (depth % words_per_trace): Loop buffer has been overwritten,
		 * and the oldest data needs to be found.
		 * +words_per_trace: the lastest data is [1,0,0] and useless.
		 */
		write_ptr = readl(iotm.cssys_base + ETB_RWP);
		write_ptr += (depth % words_per_trace) + words_per_trace - 4;
		iotm.etb_buf_words_cnt = depth;
		writel(write_ptr, iotm.cssys_base + ETB_RRP);
	}

	/* 8K */
	for (i = 0; i < iotm.etb_buf_words_cnt; i += words_per_trace) {
		for (j = 0; j < words_per_trace; j++) {
			iotm_trace[j] = readl(iotm.cssys_base + ETB_RRD);

			if (!iotm.timeout_irq_handled)
				iotm.etb_buf[i + j] = iotm_trace[j];
		}

		if (print_correct_trace(iotm_trace, buf))
			break;

		if ((iotm.timeout_irq_handled || iotm.dump_mode != IOTM_DUMP_NONE) &&
			(i > iotm.etb_buf_words_cnt - iotm.dump_cnt * words_per_trace))
			pr_err("%s", buf);
	}
	kfree(buf);
}

static void dump_register_info(void *buf)
{
	int i = 0;
	int pos = 0;
	u32 *reg_base = NULL;

	if (!iotm.timeout_irq_handled && iotm.supported)
		//manual dump
		reg_base = iotm.saved_trace;
	else
		//auto dump
		reg_base = iotm.cssys_base + ADDR_RANGE0_BEGIN;

//monitor_range
	pos += sprintf(buf + pos, "IOTM:include monitor range:[%x, %x]\n",
			reg_base[(ADDR_RANGE7_BEGIN - ADDR_RANGE0_BEGIN) >> 2],
			reg_base[(ADDR_RANGE7_END - ADDR_RANGE0_BEGIN) >> 2]);
	pos += sprintf(buf + pos, "IOTM:exclude monitor range:");

	for (i = 0; i < MAX_EXCLUDE_RANGE * 2; i += 2)
		if (reg_base[i] != 0)
			pos += sprintf(buf + pos, "[%x, %x] ",
						reg_base[i], reg_base[i + 1]);

	pos += sprintf(buf + pos, "\n");

//ddr range
	if (iotm.monitor_mode == AXI_MODE)
		iotm.ops->ddr_range_get(reg_base, buf, &pos);
	else if (iotm.monitor_mode == ETB_MODE)
		pos += sprintf(buf + pos, "IOTM:ETB\n");

//print cnt
	pos += sprintf(buf + pos, "IOTM:trace_print cnt = %d\n", iotm.dump_cnt);

//others
	pos += sprintf(buf + pos, "IOTM:MONITOR_STATUS:0x%x IOTM_CTRL_MODE = 0x%x\n",
				reg_base[(MONITOR_STATUS - ADDR_RANGE0_BEGIN) >> 2],
				reg_base[(IOTM_CTRL_MODE - ADDR_RANGE0_BEGIN) >> 2]);
}

static void dump_trace(void)
{
	char *buf;

	buf = kmalloc(TRACE_BUF_SIZE, GFP_KERNEL);
	if (!buf)
		return;

	if (iotm.timeout_irq_handled || iotm.dump_mode != IOTM_DUMP_NONE) {
		pr_err("IOTM:dump trace start==========\n");

		dump_register_info(buf);
		pr_err("%s", buf);
	}
	kfree(buf);

	iotm.fail_trace_cnt = 0;
	if (iotm.monitor_mode == AXI_MODE) {
		if (iotm.timeout_irq_handled || iotm.dump_mode != IOTM_DUMP_NONE)
			auto_dump_ddr_buf();
	} else if (iotm.monitor_mode == ETB_MODE) {
		auto_dump_etb_buf();
	}

	if (iotm.timeout_irq_handled || iotm.dump_mode != IOTM_DUMP_NONE)
		pr_err("IOTM:dump trace end==========\n");
}

static void print_timeout_data(void)
{
	unsigned int val;

	val = readl(iotm.cssys_base + IOTM_IRQ_CTRL);

	/* capu timeout, time is setted in bl31. */
	if (val & IOTM_IRQ_CTRL_CAPU_TIMEOUT) {
		pr_err("IOTM:capu timeout(50ms) addr:%x data:%x monitor_status:%x\n",
			readl(iotm.cssys_base + CAPU_MONITOR_ADDR),
			readl(iotm.cssys_base + CAPU_MONITOR_DATA),
			readl(iotm.cssys_base + MONITOR_STATUS));
	}

	/* vapb4 timeout, time is setted in bl31. */
	if (val & IOTM_IRQ_CTRL_VAPB4_TIMEOUT) {
		pr_err("IOTM:vapb timeout(50ms) addr:%x data:%x monitor_status:%x\n",
			readl(iotm.cssys_base + VAPB4_MONITOR_ADDR),
			readl(iotm.cssys_base + VAPB4_MONITOR_DATA),
			readl(iotm.cssys_base + MONITOR_STATUS));
	}
}

static int iotm_exception_handler(struct notifier_block *nb,
			unsigned long action, void *data)
{
	unsigned int val;

	val = readl(iotm.cssys_base + IOTM_IRQ_CTRL);

	/* panic has nothing with IOTM */
	if (iotm.timeout_irq_handled == 0 && !(val & (IOTM_IRQ_CTRL_CAPU_TIMEOUT |
		IOTM_IRQ_CTRL_VAPB4_TIMEOUT)))
		return 0;

	print_timeout_data();

	dump_trace();

	return 0;
}

static struct notifier_block iotm_panic_notifier = {
	.notifier_call  = iotm_exception_handler,
};

static irqreturn_t iotm_irq_handler(int irq, void *data)
{
	unsigned int val;
	int iotm_en_saved = iotm_en;

	iotm_en = IOTM_DISABLE;

	val = readl(iotm.cssys_base + IOTM_IRQ_CTRL);

	if (val & (IOTM_IRQ_CTRL_CAPU_TIMEOUT | IOTM_IRQ_CTRL_VAPB4_TIMEOUT)) {
		pr_err("IOTM:timeout,IOTM_IRQ_CTRL = 0x%x\n", val);
		print_timeout_data();
		iotm.timeout_irq_handled = 1;
		mb(); /* make sure we only clear irq after iotm.timeout_irq_handled is set */
		writel(val | IOTM_IRQ_CTRL_VAPB4_TIMEOUT_CLEAR | IOTM_IRQ_CTRL_CAPU_TIMEOUT_CLEAR,
				iotm.cssys_base + IOTM_IRQ_CTRL);
	}

	if (val & (IOTM_IRQ_CTRL_PACK_IRQ | IOTM_IRQ_CTRL_VAPB4_FULL | IOTM_IRQ_CTRL_CAPU_FULL)) {
		pr_err("IOTM:full irq! IOTM_IRQ_CTRL = 0x%x\n", val);
		writel(val | IOTM_IRQ_CTRL_PACK_IRQ_CLEAR | IOTM_IRQ_CTRL_VAPB4_FULL_CLEAR |
				IOTM_IRQ_CTRL_CAPU_FULL_CLEAR, iotm.cssys_base + IOTM_IRQ_CTRL);

		/* restart trace data */
		val = readl(iotm.cssys_base + IOTM_CTRL_MODE);
		val |= (IOTM_CTRL_MODE_CAPU_ENABLE | IOTM_CTRL_MODE_VAPB4_ENABLE |
			IOTM_CTRL_MODE_TRACE_ENABLE);
		writel(val, iotm.cssys_base + IOTM_CTRL_MODE);

		iotm_en = iotm_en_saved;
	}

	return IRQ_HANDLED;
}

static int iotm_dt_init(struct platform_device *pdev)
{
	struct device_node *node;
	struct reserved_mem *rmem;
	u32 iotm_ddr_size;
	int ret = 0;
	struct resource *res;
	struct device_node *dn = pdev->dev.of_node;
	int index = 1;
	const __be32 *exclude_reg;
	int len, i;

	/* get the version of iotm */
	if (of_property_read_u32(dn, "version", &iotm.version)) {
		pr_err("IOTM:Failed to read version property\n");
		return -EINVAL;
	}

	switch (iotm.version) {
	case IOTM_V1:
		iotm.ops = &iotm_v1_ops;
		iotm.bytes_per_trace = sizeof(struct iotm_record_v1);
		break;
	case IOTM_V2:
		iotm.ops = &iotm_v2_ops;
		iotm.bytes_per_trace = sizeof(struct iotm_record_v2);
		break;
	default:
		break;
	}

	/* get registers of a53_debug */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	iotm.cssys_base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (IS_ERR(iotm.cssys_base)) {
		pr_err("IOTM:fail to ioremap iotm register\n");
		return PTR_ERR(iotm.cssys_base);
	}

	/* get registers of exclude_monitor_range */
	exclude_reg = of_get_property(dn, "exclude-reg", &len);
	if (!exclude_reg || len % (sizeof(u32) * 2))
		dev_err(&pdev->dev, "Invalid or missing exclude-reg property\n");

	for (i = 0; i < len / (sizeof(u32) * 2); i++, index++) {
		iotm.range[MAX_EXCLUDE_RANGE - index].start = be32_to_cpup(exclude_reg++);
		iotm.range[MAX_EXCLUDE_RANGE - index].end = be32_to_cpup(exclude_reg++);
	}

	/* get ddr_range buffer */
	if (iotm.monitor_mode == AXI_MODE) {
		node = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
		if (!node) {
			pr_err("IOTM:Failed to find memory-region node\n");
			return -ENODEV;
		}

		rmem = of_reserved_mem_lookup(node);
		if (!rmem) {
			dev_warn(&pdev->dev, "no such reserved mem of node name %s\n",
					pdev->dev.of_node->name);
			return -ENODEV;
		}

		ret = of_property_read_u32(node, "iotm-size", &iotm_ddr_size);
		if (ret) {
			pr_err("IOTM:Failed to read iotm-size property\n");
			return ret;
		}

		iotm.buf_start = (rmem->base + rmem->size - iotm_ddr_size + 1) & PAGE_MASK;
		iotm.buf_end = iotm.buf_start + iotm_ddr_size - 1;
	}

	return 0;
}

static void etb_coresight_init(void)
{
	writel(ETB_LOCK_ACCESS_ALL, iotm.cssys_base + FUNNUL_LOCKACCESS);
	iotm.ops->etb_coresight_clk();
	writel(ETB_LOCK_ACCESS_ALL, iotm.cssys_base + REPLICAPTER_LOCKACCESS);
	writel(ETB_LOCK_ACCESS_ALL, iotm.cssys_base + TPIU_LOCKACCESS);
	writel(ETB_LOCK_ACCESS_ALL, iotm.cssys_base + ETB_LOCKACCESS);
	writel(TPIU_ITCTRL_DISABLE, iotm.cssys_base + TPIU_ITCTRL);
	writel(TPIU_ITATBCTR2_ATREADY, iotm.cssys_base + TPIU_ITATBCTR2);
	writel(ETB_FFCR_DISABLE_FORMATTING, iotm.cssys_base + ETB_FFCR);
	writel(ETB_CTL_ENABLE_CAPTURE, iotm.cssys_base + ETB_CTL);
}

static void get_boot_time(struct timer_list *t)
{
	u64 ns_time, pct;
	unsigned long flags;

	local_irq_save(flags);
	pct = __arch_counter_get_cntpct();
	ns_time = sched_clock();
	local_irq_restore(flags);

	iotm.ops->boot_time_record(pct, ns_time);
}

static int coresight_init(void)
{
	unsigned int val, i;
	struct arm_smccc_res res;
	unsigned long flags;

	/* stop iotm. */
	val = readl(iotm.cssys_base + IOTM_CTRL_MODE);
	val |= IOTM_CTRL_MODE_TRACE_DISABLE;
	writel(val, iotm.cssys_base + IOTM_CTRL_MODE);

	/* init iotm and reset coresight */
	iotm_smccc_smc(AML_IOTM_SMC_CMD, AML_IOTM_INIT_SMC_ARG, 0, 0, 0, res);

	/* Whether the iotm records trace data */
	if (iotm_en == IOTM_ENABLE_NO_TRACE) {
		iotm_smccc_smc(AML_IOTM_SMC_CMD, AML_IOTM_JTAG_SMC_ARG,
			AML_IOTM_JTAG_DISABLE_SMC_ARG, 0, 0, res);
		pr_info("IOTM:can't record trace, just trigger timeout\n");
	}

	/* do exclude registers */
	for (i = 0; i < MAX_EXCLUDE_RANGE; i++) {
		if (iotm.range[i].start != 0)
			iotm_smccc_smc(AML_IOTM_SMC_CMD, AML_IOTM_RANGE_SMC_ARG, i,
				iotm.range[i].start, iotm.range[i].end, res);
	}

	if (iotm.monitor_mode == AXI_MODE) {
		/* Allocate DDR reserved space to MONITOR_BUF_SIZE_LOW. */
		writel(iotm.buf_start,
			iotm.cssys_base + MONITOR_BUF_BASEADDR_LOW);

		iotm.ops->ddr_range_set();
	} else if (iotm.monitor_mode == ETB_MODE) {
		/* init coresight and set iotm_mode*/
		etb_coresight_init();

		val = readl(iotm.cssys_base + IOTM_CTRL_MODE);
		val |= ETB_MODE;
		writel(val, iotm.cssys_base + IOTM_CTRL_MODE);
	}

	/* enable iotm */
	val = readl(iotm.cssys_base + IOTM_CTRL_MODE);

	/* ts1_accurate_sel=4: TS1 = (soc timestamp)/ (2^ts1_accurate_sel) */
	val &= ~IOTM_CTRL_MODE_ACCURATE;
	val |= IOTM_CTRL_MODE_ACCURATE_16;
	/* crtl_iotm_ts=4: TS1:[31:5],TS0: [4:1] */
	val &= ~IOTM_CTRL_MODE_TS;
	val |= IOTM_CTRL_MODE_TS_27;

	if (iotm.monitor_mode == AXI_MODE) {
		/* confirm IOTM_AXI_ADDR can point to the correct ddr address */
		for (i = 0; i < CONTINUOUS_PULSE; i++) {
			val |= IOTM_CTRL_MODE_TRACE_ENABLE;
			writel(val, iotm.cssys_base + IOTM_CTRL_MODE);
			udelay(CONTINUOUS_PULSE_TIME);

			val &= ~IOTM_CTRL_MODE_TRACE_ENABLE;
			writel(val, iotm.cssys_base + IOTM_CTRL_MODE);
		}
	}

	local_irq_save(flags);
	/* monitor vapb4 and capu bus then start trace data */
	val |= (IOTM_CTRL_MODE_CAPU_ENABLE | IOTM_CTRL_MODE_VAPB4_ENABLE |
			IOTM_CTRL_MODE_TRACE_ENABLE);
	writel(val, iotm.cssys_base + IOTM_CTRL_MODE);

	/* When disable record trace, the axi_addr can't be changed. */
	if (iotm.monitor_mode == AXI_MODE && iotm_en != IOTM_ENABLE_NO_TRACE) {
		/* Check whether 0x0 is written in the trace */
		val = readl(iotm.cssys_base + IOTM_AXI_ADDR);
		val = readl(iotm.cssys_base + IOTM_AXI_ADDR);
		if (val < iotm.buf_start || val >= iotm.buf_end) {
			pr_err("IOTM:AXI_ADDR = 0x%x is wrong,close iotm\n", val);

			val = readl(iotm.cssys_base + IOTM_CTRL_MODE);
			val |= IOTM_CTRL_MODE_TRACE_DISABLE;
			writel(val, iotm.cssys_base + IOTM_CTRL_MODE);

			local_irq_restore(flags);
			return -1;
		}
	}
	local_irq_restore(flags);
	return 0;
}

static int iotm_syscore_suspend(void)
{
	unsigned int iotm_ctrl_mode_val;

	if (!iotm.supported)
		return 0;

	iotm.en_saved = iotm_en;
	iotm_en = IOTM_DISABLE;

	iotm_ctrl_mode_val = readl(iotm.cssys_base + IOTM_CTRL_MODE);
	iotm_ctrl_mode_val |= IOTM_CTRL_MODE_TRACE_DISABLE;
	writel(iotm_ctrl_mode_val, iotm.cssys_base + IOTM_CTRL_MODE);

	return 0;
}

static void iotm_syscore_resume(void)
{
	int ret;

	if (!iotm.supported)
		return;

	ret = coresight_init();
	if (!ret)
		iotm_en = iotm.en_saved;
}

static void iotm_syscore_shutdown(void)
{
	iotm_syscore_suspend();
}

static struct syscore_ops iotm_syscore_ops = {
	.suspend = iotm_syscore_suspend,
	.resume = iotm_syscore_resume,
	.shutdown = iotm_syscore_shutdown,
};

static void add_reg_info(const char *reg_name, phys_addr_t start, phys_addr_t end)
{
	struct node_info *mod;
	struct reg_info *reg;
	bool found = false;

	list_for_each_entry(mod, &nodes_list, list) {
		if (strcmp(mod->name, reg_name) == 0) {
			found = true;
			break;
		}
	}

	if (!found) {
		mod = kmalloc(sizeof(*mod), GFP_KERNEL);
		if (!mod)
			return;

		strscpy(mod->name, reg_name, sizeof(mod->name));
		INIT_LIST_HEAD(&mod->reg_list);
		list_add_tail(&mod->list, &nodes_list);
	}

	reg = kmalloc(sizeof(*reg), GFP_KERNEL);
	if (!reg)
		return;

	reg->start = start;
	reg->end = end;
	list_add_tail(&reg->list, &mod->reg_list);
}

/* Gets the register range of all nodes in the DTS */
static void node_reg_info_init(void)
{
	struct device_node *dn = of_find_all_nodes(NULL);
	struct resource res;
	int index;
	char *reg_name;

	while (dn) {
		index = 0;
		while (!of_address_to_resource(dn, index++, &res)) {
			if (res.start >= IOTM_MONITOR_RANGE_MIN &&
				res.end <= IOTM_MONITOR_RANGE_MAX) {
				char *buffer = kstrdup(res.name, GFP_KERNEL);
				char *origin_buffer = buffer;

				reg_name = strsep(&buffer, "@");
				if (reg_name)
					add_reg_info(reg_name, res.start, res.end);

				kfree(origin_buffer);
			}
		}
		dn = of_find_all_nodes(dn);
	}
}

static int trace_show(struct seq_file *m, void *v)
{
	void *ptr, *ptr_end = NULL;
	u8 buf[TRACE_BUF_SIZE];

	seq_puts(m, "IOTM:dump trace start==========\n");

	dump_register_info(buf);
	seq_printf(m, "%s", buf);

	ptr = iotm.saved_trace + NUM_OF_IOTM_REGISTERS * sizeof(int);
	if (iotm.monitor_mode == AXI_MODE)
		ptr_end = ptr + iotm.buf_end - iotm.buf_start + 1;
	else if (iotm.monitor_mode == ETB_MODE)
		ptr_end = ptr + iotm.etb_buf_words_cnt * sizeof(int);

	iotm.fail_trace_cnt = 0;
	while (ptr < ptr_end) {
		if (print_correct_trace(ptr, buf))
			break;
		seq_printf(m, "%s", buf);
		ptr += iotm.bytes_per_trace;
	}

	seq_puts(m, "IOTM:dump trace end==========\n");

	return 0;
}

static void iotm_proc_init(void)
{
	struct proc_dir_entry *aml_iotm_trace;
	unsigned int ddr_size = iotm.buf_end - iotm.buf_start + 1;
	u32 saved_trace_size = 0;

	if (iotm.monitor_mode == AXI_MODE)
		saved_trace_size = ddr_size + NUM_OF_IOTM_REGISTERS * sizeof(int);
	else if (iotm.monitor_mode == ETB_MODE)
		saved_trace_size = (iotm.etb_buf_words_cnt + NUM_OF_IOTM_REGISTERS) * sizeof(int);

	iotm.saved_trace = kmalloc(saved_trace_size, GFP_KERNEL);
	if (!iotm.saved_trace)
		return;

	aml_iotm_trace = proc_create_single_data("aml_iotm_trace",
					0400, NULL, trace_show, NULL);
	if (!aml_iotm_trace) {
		pr_err("IOTM:fail to create /proc/aml_iotm_trace\n");
		kfree(iotm.saved_trace);
		return;
	}

	memcpy_fromio(iotm.saved_trace, iotm.cssys_base + ADDR_RANGE0_BEGIN,
					NUM_OF_IOTM_REGISTERS * sizeof(int));
	if (iotm.monitor_mode == AXI_MODE) {
		u32 iotm_axi_addr_val = readl(iotm.cssys_base + IOTM_AXI_ADDR);
		u32 iotm_ddr_size = iotm.buf_end - iotm.buf_start + 1;
		u32 offset = 0;
		u32 last_seg_size = iotm_axi_addr_val - iotm.buf_start;

		offset = last_seg_size % iotm.bytes_per_trace;
		if (offset)
			last_seg_size += iotm.bytes_per_trace - offset;

		if (iotm_axi_addr_val < iotm.buf_start || iotm_axi_addr_val > iotm.buf_end) {
			pr_err("IOTM:error!IOTM_AXI_ADDR=0x%x, ddr_buf=[0x%x, 0x%x]\n",
					iotm_axi_addr_val, iotm.buf_start, iotm.buf_end);
			kfree(iotm.saved_trace);
			return;
		}

		memcpy_fromio(iotm.saved_trace + NUM_OF_IOTM_REGISTERS * sizeof(int),
			phys_to_virt(iotm.buf_start + last_seg_size),
						iotm_ddr_size - last_seg_size);
		memcpy_fromio(iotm.saved_trace + NUM_OF_IOTM_REGISTERS * sizeof(int) +
			iotm_ddr_size - last_seg_size, phys_to_virt(iotm.buf_start),
			iotm_axi_addr_val - iotm.buf_start);
	} else if (iotm.monitor_mode == ETB_MODE) {
		memcpy_fromio(iotm.saved_trace + NUM_OF_IOTM_REGISTERS * sizeof(int),
				(void *)iotm.etb_buf, iotm.etb_buf_words_cnt * sizeof(int));
	}
}

static void watchdog_dump_trace(void)
{
	unsigned int val;

	if (iotm.dump_mode != IOTM_DUMP_NONE)
		iotm.ops->detect_watchdog_status();

	// stop iotm, munual flush
	val = readl(iotm.cssys_base + IOTM_CTRL_MODE);
	writel(val | IOTM_CTRL_MODE_TRACE_DISABLE, iotm.cssys_base + IOTM_CTRL_MODE);

	/* watchdog reset need clear MONITOR_RANGE_INCLUDE_RST_MASK */
	val = readl(iotm.cssys_base + MONITOR_RANGE_INCLUDE);
	val &= ~MONITOR_RANGE_INCLUDE_RST_MASK;
	writel(val, iotm.cssys_base + MONITOR_RANGE_INCLUDE);

	/* init coresight to dump trace */
	if (iotm.monitor_mode == ETB_MODE) {
		etb_coresight_init();
		iotm.etb_buf = kmalloc(ETB_SIZE, GFP_KERNEL);
		if (!iotm.etb_buf)
			return;
	}

	dump_trace();
	iotm_proc_init();

	if (iotm.monitor_mode == ETB_MODE)
		kfree(iotm.etb_buf);
}

static int iotm_probe(struct platform_device *pdev)
{
	int ret;
	int irq;

	ret = iotm_dt_init(pdev);
	if (ret)
		return ret;

	node_reg_info_init();

	watchdog_dump_trace();

	spin_lock_init(&iotm.record_lock);

	ret = coresight_init();
	if (ret)
		return ret;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		pr_err("IOTM:fail to get iotm irq\n");
		return irq;
	}

	ret = devm_request_irq(&pdev->dev, irq, iotm_irq_handler, 0, "iotm", NULL);
	if (ret) {
		pr_err("IOTM:fail to request irq: %d\n", ret);
		return ret;
	}

	/* create timer to record kernel_time and iotm timestamp */
	timer_setup(&iotm.ts_to_kernel_timer, get_boot_time, 0);
	mod_timer(&iotm.ts_to_kernel_timer, jiffies + msecs_to_jiffies(1));

	/* print iotm trace */
	atomic_notifier_chain_register(&panic_notifier_list, &iotm_panic_notifier);

	register_syscore_ops(&iotm_syscore_ops);
	iotm.supported = 1;

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id iotm_match[] = {
	{
		.compatible = "amlogic, iotm",
	},
	{}
};
#endif

static struct platform_driver iotm_driver = {
	.driver = {
		.name  = "iotm",
		.owner = THIS_MODULE,
	#ifdef CONFIG_OF
		.of_match_table = iotm_match,
	#endif
	},
	.probe = iotm_probe,
};

int iotm_init(void)
{
	int ret = 0;

	if (iotm_en != IOTM_DISABLE)
		ret = platform_driver_register(&iotm_driver);

	return ret;
}
