/* SPDX-License-Identifier: (GPL-2.0-only OR MIT) */
/*
 * Copyright (C) 2023 Amlogic, Inc. All rights reserved
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/fb.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/sysfs.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/reset.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/pm_domain.h>
#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <linux/amlogic/media/vicp/vicp.h>

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif
/* Amlogic Headers */
#include <linux/amlogic/major.h>
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif

#define VICP_AUTO_POWER_OFF_DELAY (200) /*ms*/
#define VICP_DEVICE_NAME "vicp"

struct vicp_device_s {
	char name[20];
	atomic_t open_count;
	int major;
	unsigned int dbg_enable;
	struct platform_device *pdev;
	struct class *cla;
	struct device *dev;
	struct vicp_clock_config_s clock_cfg;
};

int vicp_dev_init(struct vicp_device_s vicp_dev);
int vicp_clock_config(int is_enable);
int vicp_pwr_init(struct device *dev);
void vicp_pwr_remove(struct device *dev);
void vicp_runtime_pwr(int enable);
unsigned long get_buf_phy_addr(u32 buf_fd);
int config_vicp_param(struct vicp_data_info_s *vicp_data_info,
	struct vicp_data_config_s *data_config);

