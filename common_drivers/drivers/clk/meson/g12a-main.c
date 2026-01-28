// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/amlogic/module_merge.h>
#include "g12a.h"
#include "g12a-aoclk.h"

static int __init g12a_main_init(void)
{
	call_sub_init(g12a_init);
	call_sub_init(g12a_aoclk_init);
	return 0;
}

static void __exit g12a_main_exit(void)
{
	g12a_exit();
	g12a_aoclk_exit();
}

module_init(g12a_main_init);
module_exit(g12a_main_exit);

MODULE_LICENSE("GPL v2");
