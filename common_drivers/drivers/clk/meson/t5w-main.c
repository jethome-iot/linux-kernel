// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/amlogic/module_merge.h>
#include "t5w.h"

static int __init t5w_main_init(void)
{
	call_sub_init(t5w_init);
	call_sub_init(t5w_aoclk_init);
	return 0;
}

static void __exit t5w_main_exit(void)
{
	t5w_exit();
	t5w_aoclk_exit();
}

module_init(t5w_main_init);
module_exit(t5w_main_exit);

MODULE_LICENSE("GPL v2");
