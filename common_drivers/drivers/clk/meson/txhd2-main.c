// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/amlogic/module_merge.h>
#include "txhd2.h"

static int __init txhd2_main_init(void)
{
	call_sub_init(txhd2_init);
	call_sub_init(txhd2_aoclk_init);
	return 0;
}

static void __exit txhd2_main_exit(void)
{
	txhd2_exit();
	txhd2_aoclk_exit();
}

module_init(txhd2_main_init);
module_exit(txhd2_main_exit);

MODULE_LICENSE("GPL v2");
