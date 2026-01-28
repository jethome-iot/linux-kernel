// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#if IS_ENABLED(CONFIG_ARM64_MTE)
#include <linux/irqflags.h>
#include <linux/moduleparam.h>
#include <linux/module.h>
#include <linux/cpuset.h>
#include <linux/sched/isolation.h>
#include <linux/amlogic/gki_module.h>
#include <linux/sched.h>
#include <asm/cpufeature.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/errno.h>
#include <linux/of_address.h>

static void __iomem *debug_mte_reg;

/* SYSCTRL_DEBUG_REG4[0]=0b1: stop writing MTE clock */
#define MTE_CLK_STOP BIT(0)

int aml_debug_mte_init(void)
{
	struct device_node *node;
	unsigned int val;

	if (!system_supports_mte()) {
		node = of_find_node_by_path("/mte_debug");

		if (!node) {
			pr_info("no mte_debug node\n");
			return -EINVAL;
		}

		debug_mte_reg = of_iomap(node, 0);
		if (!debug_mte_reg) {
			pr_info("mte_debug map fail\n");
			return -ENOMEM;
		}

		val = readl_relaxed(debug_mte_reg);
		val |= MTE_CLK_STOP;
		writel_relaxed(val, debug_mte_reg);
		pr_info("mte disabled\n");
	} else {
		pr_info("mte enabled\n");
	}
	return 0;
}

void aml_debug_mte_exit(void)
{
	if (debug_mte_reg)
		iounmap(debug_mte_reg);
}

#endif
