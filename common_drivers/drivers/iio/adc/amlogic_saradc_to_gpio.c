// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/iio/consumer.h>
#include <linux/gpio/driver.h>

struct amlogic_saradc_to_gpio {
	struct iio_channel *channels;
	int num_channels;
	u32 *thresholds;
	struct gpio_chip chip;
};

static int amlogic_saradc_to_gpio_get_direction(struct gpio_chip *chip,
						unsigned int offset)
{
	return GPIO_LINE_DIRECTION_IN;
}

static int amlogic_saradc_to_gpio_direction_input(struct gpio_chip *chip,
						  unsigned int offset)
{
	/* Do nothing */
	return 0;
}

static int amlogic_saradc_to_gpio_direction_output(struct gpio_chip *chip,
						   unsigned int offset,
						   int value)
{
	/* Do nothing */
	return 0;
}

static int amlogic_saradc_to_gpio_set_config(struct gpio_chip *chip,
					     unsigned int offset,
					     unsigned long config)
{
	/* Do nothing */
	return 0;
}

static int amlogic_saradc_to_gpio_get(struct gpio_chip *chip, unsigned int offset)
{
	struct amlogic_saradc_to_gpio *priv = gpiochip_get_data(chip);
	int result;
	int ret;

	ret = iio_read_channel_processed(&priv->channels[offset], &result);
	if (ret < 0)
		return ret;

	return !(result < priv->thresholds[offset]);
}

static int amlogic_saradc_to_gpio_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct amlogic_saradc_to_gpio *priv;
	const char *const name = dev_name(dev);
	int ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	/* Get all channels */
	priv->channels = devm_iio_channel_get_all(dev);
	if (IS_ERR(priv->channels)) {
		if (PTR_ERR(priv->channels) == -ENODEV)
			return -EPROBE_DEFER;
		return PTR_ERR(priv->channels);
	}

	/* Count the number of channels */
	while (priv->channels[priv->num_channels].indio_dev)
		priv->num_channels++;

	priv->thresholds = devm_kzalloc(dev, sizeof(*priv->thresholds), GFP_KERNEL);
	if (!priv->thresholds)
		return -ENOMEM;

	/* Read all thresholds */
	ret = of_property_read_u32_array(dev->of_node, "amlogic,threshold",
					 priv->thresholds, priv->num_channels);
	if (ret)
		return ret;

	/* Add GPIO chip */
	priv->chip.label = name;
	priv->chip.parent = dev;
	priv->chip.owner = THIS_MODULE;
	priv->chip.base = -1;
	priv->chip.ngpio = priv->num_channels;
	priv->chip.get_direction = amlogic_saradc_to_gpio_get_direction;
	priv->chip.direction_input = amlogic_saradc_to_gpio_direction_input;
	priv->chip.direction_output = amlogic_saradc_to_gpio_direction_output;
	priv->chip.get = amlogic_saradc_to_gpio_get;
	priv->chip.set_config = amlogic_saradc_to_gpio_set_config;
	ret = devm_gpiochip_add_data(dev, &priv->chip, priv);
	if (ret)
		return ret;

	dev_info(dev, "SARADC to GPIO enabled. Total number of channels: %d\n",
		 priv->num_channels);

	return 0;
}

static const struct of_device_id amlogic_saradc_to_gpio_of_match[] = {
	{ .compatible = "amlogic,saradc-to-gpio", },
	{ /* sentinel */ },
};

static struct platform_driver amlogic_saradc_to_gpio_driver = {
	.probe = amlogic_saradc_to_gpio_probe,
	.driver = {
		.name = "amlogic_saradc_to_gpio",
		.of_match_table = amlogic_saradc_to_gpio_of_match,
	},
};

int __init amlogic_saradc_to_gpio_driver_init(void)
{
	return platform_driver_register(&amlogic_saradc_to_gpio_driver);
}

void __exit amlogic_saradc_to_gpio_driver_exit(void)
{
	platform_driver_unregister(&amlogic_saradc_to_gpio_driver);
}

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Huqiang Qin <huqiang.qin@amlogic.com>");
MODULE_DESCRIPTION("Amlogic SAR ADC to GPIO driver");
