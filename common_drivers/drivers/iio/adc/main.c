// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

//#define DEBUG
#include <linux/module.h>
#include <linux/amlogic/module_merge.h>
#include "main.h"

static int __init amlogic_saradc_main_init(void)
{
	pr_debug("### %s() start\n", __func__);
	call_sub_init(amlogic_saradc_common_driver_init);
	call_sub_init(meson_sar_adc_driver_init);
	call_sub_init(amlogic_saradc_driver_init);
	call_sub_init(amlogic_pdd_driver_init);
	call_sub_init(amlogic_pdd_monitor_driver_init);
	call_sub_init(amlogic_saradc_to_gpio_driver_init);
	pr_debug("### %s() end\n", __func__);
	return 0;
}

static void __exit amlogic_saradc_main_exit(void)
{
	amlogic_saradc_to_gpio_driver_exit();
	amlogic_pdd_monitor_driver_exit();
	amlogic_pdd_driver_exit();
	meson_sar_adc_driver_exit();
	amlogic_saradc_driver_exit();
	amlogic_saradc_common_driver_exit();
}

module_init(amlogic_saradc_main_init);
module_exit(amlogic_saradc_main_exit);

MODULE_LICENSE("GPL v2");
