// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

//#define DEBUG
#include <linux/module.h>
#include <linux/amlogic/module_merge.h>
#include "main.h"

#if IS_ENABLED(CONFIG_AMLOGIC_DEBUG)
#include <linux/amlogic/gki_module.h>
#endif

static int __init aml_cpufreq_init(void)
{
	pr_debug("### %s() start\n", __func__);
	call_sub_init(aml_cpufreq_v1_init);
	call_sub_init(aml_cpufreq_v2_init);
	pr_debug("### %s() end\n", __func__);

	return 0;
}

static void __exit aml_cpufreq_exit(void)
{
	aml_cpufreq_v2_exit();
	aml_cpufreq_v1_exit();
}

module_init(aml_cpufreq_init);
module_exit(aml_cpufreq_exit);
MODULE_LICENSE("GPL v2");
