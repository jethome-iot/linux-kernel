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

static int __init pwm_main_init(void)
{
	pr_debug("### %s() start\n", __func__);
	call_sub_init(pwm_meson_init);
	call_sub_init(pwm_meson_tee_init);
	pr_debug("### %s() end\n", __func__);

	return 0;
}

static void __exit pwm_main_exit(void)
{
	pwm_meson_exit();
	pwm_meson_tee_exit();
}

module_init(pwm_main_init);
module_exit(pwm_main_exit);
MODULE_LICENSE("GPL v2");
