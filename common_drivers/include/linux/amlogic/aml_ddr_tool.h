/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_DDR_TOOL_H__
#define __AML_DDR_TOOL_H__

/* ddr_tool: dev_access */
struct dmc_dev_access_data {
	unsigned long addr;
	unsigned long size;
};

#if IS_ENABLED(CONFIG_AMLOGIC_DMC_DEV_ACCESS)
int register_dmc_dev_access_notifier(char *dev_name, struct notifier_block *nb);
int unregister_dmc_dev_access_notifier(char *dev_name, struct notifier_block *nb);
#else
static inline int register_dmc_dev_access_notifier(char *dev_name, struct notifier_block *nb)
{
	return 0;
}

static inline int unregister_dmc_dev_access_notifier(char *dev_name, struct notifier_block *nb)
{
	return 0;
}
#endif

/* ddr_tool: outstanding */
#if IS_ENABLED(CONFIG_AMLOGIC_DDR_BANDWIDTH)
int get_bus_num(void);
int get_bus_ots_value(int bus);
int set_bus_ots_by_value(int bus, int value);
int set_bus_ots_by_level(int bus, unsigned int level);
int get_ots_level(void);
int set_all_ots_by_level(unsigned int level);
#else
static inline int get_bus_num(void)
{
	return -1;
}

static inline int get_bus_ots_value(int bus)
{
	return -1;
}

static inline int set_bus_ots_by_value(int bus, int value)
{
	return -1;
}

static inline int set_bus_ots_by_level(int bus, unsigned int level)
{
	return -1;
}

static inline int get_ots_level(void)
{
	return -1;
}

static inline int set_all_ots_by_level(unsigned int level)
{
	return -1;
}
#endif

#if IS_ENABLED(CONFIG_AMLOGIC_DDR_BANDWIDTH)
void aml_get_all_channel_grant(u64 *channel_grant);
#else
static inline void aml_get_all_channel_grant(u64 *channel_grant)
{
}
#endif

#endif
