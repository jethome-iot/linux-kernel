// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>

#include <linux/amlogic/media/camera/aml_cam_info.h>

#define USING_CLK_API 1

#ifndef USING_CLK_API
static inline u32 WRITE_CBUS_REG(void __iomem *base, u32 reg_offset, u32 val)
{
	__raw_writel(val, base + reg_offset);
	return 0;
}

static int mclk_enable(struct device *dev, uint32_t rate)
{
	void __iomem *base_addr;

	base_addr = ioremap(0xfe008000, 0x400);
	if (!base_addr) {
		pr_err("%s: Failed to ioremap addr\n", __func__);
		return -1;
	}

	WRITE_CBUS_REG(base_addr, 0x260, 0x000010c6);
	WRITE_CBUS_REG(base_addr, 0x264, 0x00627040);
	WRITE_CBUS_REG(base_addr, 0x268, 0x60000201);
	WRITE_CBUS_REG(base_addr, 0x26c, 0x03180240);
	WRITE_CBUS_REG(base_addr, 0x260, 0x100010c6);
	WRITE_CBUS_REG(base_addr, 0x260, 0x300010c6);
	WRITE_CBUS_REG(base_addr, 0x264, 0x20627040);
	WRITE_CBUS_REG(base_addr, 0x26c, 0x03180e40);
	WRITE_CBUS_REG(base_addr, 0x26c, 0x07180c40);

	iounmap(base_addr);
	dev_dbg(dev, "ioremap clk api - fixed 24M mclk\n");

	return 0;
}
#else
static int mclk_enable(struct device *dev, uint32_t rate)
{
	struct clk *mclk0 = NULL;
	int clk_val;
	int ret;

	mclk0 = devm_clk_get(dev, "mclk0");
	if (IS_ERR_OR_NULL(mclk0)) {
		pr_err("cannot get %s\n", "mclk0");
		mclk0 = NULL;
		return -1;
	}

	ret = clk_set_rate(mclk0, rate);
	if (ret < 0)
		dev_err(dev, "clk_set_rate failed\n");
	usleep_range(30, 40);

	ret = clk_prepare_enable(mclk0);
	if (ret < 0)
		dev_err(dev, " clk_prepare_enable failed\n");

	clk_val = clk_get_rate(mclk0);
	devm_clk_put(dev, mclk0);

	dev_dbg(dev, "std clk api - mclk is %d MHZ\n", clk_val / 1000000);

	return 0;
}
#endif

static int mclk_disable(struct device *dev)
{
#ifdef USING_CLK_API
	struct clk *mclk0 = NULL;

	mclk0 = devm_clk_get(dev, "mclk0");
	if (IS_ERR_OR_NULL(mclk0)) {
		pr_err("cannot get %s clk\n", "mclk0");
		mclk0 = NULL;
		return -1;
	}

	if (__clk_is_enabled(mclk0))
		clk_disable_unprepare(mclk0);

	devm_clk_put(dev, mclk0);

	dev_dbg(dev, "disable mclk\n");
#endif
	return 0;
}

int cam_enable_clk_s6(struct aml_cam_info_s *cam_dev)
{
	struct platform_device *pdev = cam_dev->cam_plat_dev;

	return mclk_enable(&pdev->dev, cam_dev->mclk_freq);
}

int cam_disable_clk_s6(struct aml_cam_info_s *cam_dev)
{
	struct platform_device *pdev = cam_dev->cam_plat_dev;

	return mclk_disable(&pdev->dev);
}
