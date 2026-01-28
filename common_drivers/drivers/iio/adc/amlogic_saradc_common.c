// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

static const struct of_device_id amlogic_saradc_common_of_match[] = {
	{ .compatible = "amlogic,saradc-common", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, amlogic_saradc_common_of_match);

static const struct regmap_config amlogic_saradc_common_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
};

static int amlogic_saradc_common_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct regmap *regmap;
	void __iomem *base;

	base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(base))
		return PTR_ERR(base);

	regmap = devm_regmap_init_mmio(dev, base,
				       &amlogic_saradc_common_regmap_config);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	/* The following call can make the child nodes correctly match */
	return devm_of_platform_populate(dev);
}

static struct platform_driver amlogic_saradc_common_driver = {
	.probe		= amlogic_saradc_common_probe,
	.driver		= {
		.name	= "amlogic-saradc-common",
		.of_match_table = amlogic_saradc_common_of_match,
	},
};

int __init amlogic_saradc_common_driver_init(void)
{
	return platform_driver_register(&amlogic_saradc_common_driver);
}

void __exit amlogic_saradc_common_driver_exit(void)
{
	platform_driver_unregister(&amlogic_saradc_common_driver);
}

MODULE_AUTHOR("Huqiang Qin <huqiang.qin@amlogic.com>");
MODULE_DESCRIPTION("Amlogic SAR ADC common driver");
MODULE_LICENSE("GPL v2");
