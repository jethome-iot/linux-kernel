// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/amlogic/module_merge.h>
#include "tm2.h"

static int __init tm2_main_init(void)
{
	call_sub_init(tm2_init);
	call_sub_init(tm2_aoclk_init);
	return 0;
}

static void __exit tm2_main_exit(void)
{
	tm2_exit();
	tm2_aoclk_exit();
}

module_init(tm2_main_init);
module_exit(tm2_main_exit);

MODULE_LICENSE("GPL v2");
