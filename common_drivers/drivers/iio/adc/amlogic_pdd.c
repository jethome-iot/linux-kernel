// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/irqchip.h>
#include <linux/irqdomain.h>
#include <linux/irqchip/chained_irq.h>

#include <dt-bindings/interrupt-controller/arm-gic.h>

#define FLT_TICK_BIT_WIDTH		2
#define FLT_CNT_BIT_WIDTH		3
#define FLT_TICK_MASK(OFFSET)		((BIT(FLT_TICK_BIT_WIDTH) - 1) << (OFFSET))
#define FLT_CNT_MASK(OFFSET)		((BIT(FLT_CNT_BIT_WIDTH) - 1) << (OFFSET))

#define DET_TRIM_BIT_WIDTH		6
#define DET_CTRL_BIT_WIDTH		2
#define DET_TRIM_MASK(OFFSET)		((BIT(DET_TRIM_BIT_WIDTH) - 1) << (OFFSET))
#define DET_CTRL_MASK(OFFSET)		((BIT(DET_CTRL_BIT_WIDTH) - 1) << (OFFSET))

enum digital_map {
	IRQ_OUT,
	IRQ_CLR,
	IRQ_EN,
	FLT_TICK,
	FLT_CNT,
	DIGITAL_MAP_MAX,
};

enum analog_map {
	DET_TRIM,
	DET_EN,
	DET_CTRL,
	ANALOG_MAP_MAX,
};

struct reg_info {
	u32 byte_offs;
	u32 bit_offs;
};

enum amlogic_pdd_state {
	STATE_IDLE,
	STATE_ALLOC,
	STATE_ACTIVATE,
	STATE_MAX,
};

struct amlogic_pdd_priv {
	struct regmap *regmap;
	struct clk *clk_core;
	struct reg_info digital[DIGITAL_MAP_MAX];
	struct reg_info analog[ANALOG_MAP_MAX];
	u32 *voltage;
	int num_voltage;
	u32 hw_irq;
	int ctrl_value;
	enum amlogic_pdd_state state;
	/* Used to protect the state variable */
	spinlock_t lock;
};

static const struct regmap_config amlogic_pdd_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
};

static const struct of_device_id amlogic_pdd_of_match[] = {
	{ .compatible = "amlogic,pdd", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, amlogic_pdd_of_match);

static void amlogic_pdd_set_ctrl(struct device *dev, u32 ctrl)
{
	struct amlogic_pdd_priv *priv = dev_get_drvdata(dev);
	u32 byte_offs = priv->analog[DET_CTRL].byte_offs;
	u32 bit_offs = priv->analog[DET_CTRL].bit_offs;
	u32 mask = DET_CTRL_MASK(bit_offs);
	u32 regval = ctrl << bit_offs;

	regmap_update_bits(priv->regmap, byte_offs, mask, regval);

	dev_dbg(dev, "Set ctrl value: %d\n", ctrl);
}

static void __maybe_unused amlogic_pdd_set_trim(struct device *dev, u32 trim)
{
	struct amlogic_pdd_priv *priv = dev_get_drvdata(dev);
	u32 byte_offs = priv->analog[DET_TRIM].byte_offs;
	u32 bit_offs = priv->analog[DET_TRIM].bit_offs;
	u32 mask = DET_TRIM_MASK(bit_offs);
	u32 regval = trim << bit_offs;

	regmap_update_bits(priv->regmap, byte_offs, mask, regval);

	dev_dbg(dev, "Set trim value: %d\n", trim);
}

static bool amlogic_pdd_irq_valid(struct device *dev)
{
	struct amlogic_pdd_priv *priv = dev_get_drvdata(dev);
	u32 byte_offs = priv->digital[IRQ_OUT].byte_offs;
	u32 bit_offs = priv->digital[IRQ_OUT].bit_offs;
	u32 regval;

	regmap_read(priv->regmap, byte_offs, &regval);
	if (regval & BIT(bit_offs))
		return true;

	return false;
}

static void amlogic_pdd_irq_clr(struct device *dev)
{
	struct amlogic_pdd_priv *priv = dev_get_drvdata(dev);
	u32 byte_offs = priv->digital[IRQ_CLR].byte_offs;
	u32 bit_offs = priv->digital[IRQ_CLR].bit_offs;

	regmap_update_bits(priv->regmap, byte_offs, BIT(bit_offs), BIT(bit_offs));
}

static void amlogic_pdd_hw_enable(struct device *dev)
{
	struct amlogic_pdd_priv *priv = dev_get_drvdata(dev);
	u32 byte_offs;
	u32 bit_offs;
	u32 mask;
	u32 regval;

	/*
	 * filter_tick_sel:
	 *   2'b00: 1us/tick
	 *   2'b01: 10us/tick
	 *   2'b10: 100us/tick
	 *   2'b11: 1ms/tick
	 */
	byte_offs = priv->digital[FLT_TICK].byte_offs;
	bit_offs = priv->digital[FLT_TICK].bit_offs;
	mask = FLT_TICK_MASK(bit_offs);
	regval = 1 << bit_offs; /* Select 10us/tick */
	regmap_update_bits(priv->regmap, byte_offs, mask, regval);

	/*
	 * filter_cnt_sel:
	 *   3'b000: 0 tick (no filter)
	 *   3'b001: 1x3 tick
	 *   3'b010: 2x3 tick
	 *   ...
	 */
	byte_offs = priv->digital[FLT_CNT].byte_offs;
	bit_offs = priv->digital[FLT_CNT].bit_offs;
	mask = FLT_CNT_MASK(bit_offs);
	regval = 0x7 << bit_offs; /* Select 7x3 tick */
	regmap_update_bits(priv->regmap, byte_offs, mask, regval);

	/* Enable detection */
	byte_offs = priv->analog[DET_EN].byte_offs;
	bit_offs = priv->analog[DET_EN].bit_offs;
	regmap_update_bits(priv->regmap, byte_offs, BIT(bit_offs), BIT(bit_offs));

	/* Clear PDD IRQ */
	amlogic_pdd_irq_clr(dev);

	/* Enable PDD IRQ */
	byte_offs = priv->digital[IRQ_EN].byte_offs;
	bit_offs = priv->digital[IRQ_EN].bit_offs;
	regmap_update_bits(priv->regmap, byte_offs, BIT(bit_offs), BIT(bit_offs));

	dev_dbg(dev, "Power down detection enabled\n");
}

static void amlogic_pdd_hw_disable(struct device *dev)
{
	struct amlogic_pdd_priv *priv = dev_get_drvdata(dev);
	u32 byte_offs;
	u32 bit_offs;

	/* Disable PDD IRQ */
	byte_offs = priv->digital[IRQ_EN].byte_offs;
	bit_offs = priv->digital[IRQ_EN].bit_offs;
	regmap_update_bits(priv->regmap, byte_offs, BIT(bit_offs), 0);

	/* Disable detection */
	byte_offs = priv->analog[DET_EN].byte_offs;
	bit_offs = priv->analog[DET_EN].bit_offs;
	regmap_update_bits(priv->regmap, byte_offs, BIT(bit_offs), 0);

	dev_dbg(dev, "Power down detection disabled\n");
}

static int amlogic_pdd_dt_parse(struct device *dev)
{
	struct amlogic_pdd_priv *priv = dev_get_drvdata(dev);
	struct device_node *np = dev->of_node;
	u32 digital_maps[DIGITAL_MAP_MAX << 1];
	u32 analog_maps[ANALOG_MAP_MAX << 1];
	int index;
	int ret;

	/* Necessary DT checks */
	ret = of_property_count_u32_elems(np, "amlogic,digital-maps");
	if (ret != (DIGITAL_MAP_MAX << 1))
		return dev_err_probe(dev, -EINVAL, "Digital maps is invalid\n");
	ret = of_property_count_u32_elems(np, "amlogic,analog-maps");
	if (ret != (ANALOG_MAP_MAX << 1))
		return dev_err_probe(dev, -EINVAL, "Analog maps is invalid\n");
	ret = of_property_count_u32_elems(np, "amlogic,voltage-maps");
	if (ret <= 0)
		return dev_err_probe(dev, -EINVAL, "Voltage maps is invalid\n");
	priv->num_voltage = ret;
	dev_dbg(dev, "Valid voltage total: %d\n", priv->num_voltage);

	/* Parse digital maps */
	ret = of_property_read_u32_array(np, "amlogic,digital-maps",
					 digital_maps, DIGITAL_MAP_MAX << 1);
	if (ret)
		return ret;
	dev_dbg(dev, "Digital maps:\n");
	for (index = 0; index < DIGITAL_MAP_MAX; index++) {
		priv->digital[index].byte_offs = digital_maps[index << 1];
		priv->digital[index].bit_offs = digital_maps[(index << 1) + 1];
		dev_dbg(dev, "    [%d]: <%08x %08x>\n", index,
			priv->digital[index].byte_offs,
			priv->digital[index].bit_offs);
	}

	/* Parse analog maps */
	ret = of_property_read_u32_array(np, "amlogic,analog-maps",
					 analog_maps, ANALOG_MAP_MAX << 1);
	if (ret)
		return ret;
	dev_dbg(dev, "Analog maps:\n");
	for (index = 0; index < ANALOG_MAP_MAX; index++) {
		priv->analog[index].byte_offs = analog_maps[index << 1];
		priv->analog[index].bit_offs = analog_maps[(index << 1) + 1];
		dev_dbg(dev, "    [%d]: <%08x %08x>\n", index,
			priv->analog[index].byte_offs,
			priv->analog[index].bit_offs);
	}

	priv->voltage = devm_kzalloc(dev, sizeof(*priv->voltage) *
				     priv->num_voltage, GFP_KERNEL);
	if (!priv->voltage)
		return dev_err_probe(dev, -ENOMEM, "Failed allocating memory to voltage\n");

	/* Parse voltage maps */
	ret = of_property_read_u32_array(np, "amlogic,voltage-maps",
					 priv->voltage, priv->num_voltage);
	if (ret)
		return ret;

	/* Parse hardware IRQ number */
	ret = of_property_read_u32(np, "amlogic,interrupt", &priv->hw_irq);
	if (ret)
		return ret;
	dev_dbg(dev, "Hardware IRQ number: %d\n", priv->hw_irq);

	return 0;
}

static void amlogic_pdd_irq_enable(struct irq_data *data)
{
	struct device *dev = irq_data_get_irq_chip_data(data);
	struct amlogic_pdd_priv *priv = dev_get_drvdata(dev);

	amlogic_pdd_set_ctrl(dev, priv->ctrl_value);
	amlogic_pdd_hw_enable(dev);
	irq_chip_enable_parent(data);
}

static void amlogic_pdd_irq_disable(struct irq_data *data)
{
	struct device *dev = irq_data_get_irq_chip_data(data);

	amlogic_pdd_hw_disable(dev);
	irq_chip_disable_parent(data);
}

static void amlogic_pdd_irq_ack(struct irq_data *data)
{
	struct device *dev = irq_data_get_irq_chip_data(data);

	if (!amlogic_pdd_irq_valid(dev))
		dev_warn(dev, "IRQ invalid\n");

	irq_chip_ack_parent(data);
}

static void amlogic_pdd_irq_eoi(struct irq_data *data)
{
	struct device *dev = irq_data_get_irq_chip_data(data);

	amlogic_pdd_irq_clr(dev);
	irq_chip_eoi_parent(data);
}

static struct irq_chip amlogic_pdd_irqchip = {
	.name			= "AML-PDD",
	.irq_enable		= amlogic_pdd_irq_enable,
	.irq_disable		= amlogic_pdd_irq_disable,
	.irq_ack		= amlogic_pdd_irq_ack,
	.irq_eoi		= amlogic_pdd_irq_eoi,
	.irq_mask		= irq_chip_mask_parent,
	.irq_unmask		= irq_chip_unmask_parent,
	.irq_retrigger		= irq_chip_retrigger_hierarchy,
	.flags			= IRQCHIP_SKIP_SET_WAKE,
	.irq_set_affinity	= IS_ENABLED(CONFIG_SMP) ?
				  irq_chip_set_affinity_parent : NULL,
};

static int amlogic_pdd_irq_get_ctrl_level(struct device *dev, u32 voltage)
{
	struct amlogic_pdd_priv *priv = dev_get_drvdata(dev);
	int index;

	for (index = 0; index < priv->num_voltage; index++) {
		if (voltage <= priv->voltage[index])
			return index;
	}

	return -EINVAL;
}

static int amlogic_pdd_state_step(struct amlogic_pdd_priv *priv,
				  enum amlogic_pdd_state next_state)
{
	unsigned long flags;
	enum amlogic_pdd_state state;

	spin_lock_irqsave(&priv->lock, flags);
	state = (priv->state + 1) % STATE_MAX;
	if (next_state != state) {
		spin_unlock_irqrestore(&priv->lock, flags);
		return -EBUSY;
	}
	priv->state = state;
	spin_unlock_irqrestore(&priv->lock, flags);

	return 0;
}

static int amlogic_pdd_domain_alloc(struct irq_domain *domain, unsigned int virq,
				    unsigned int nr_irqs, void *data)
{
	struct irq_fwspec *fwspec = data;
	struct irq_fwspec p_fwspec;
	struct device *dev = domain->host_data;
	struct amlogic_pdd_priv *priv = dev_get_drvdata(dev);
	int ret;

	ret = amlogic_pdd_state_step(priv, STATE_ALLOC);
	if (ret) {
		dev_err(dev, "Failed: power down detection is allocated\n");
		return ret;
	}

	ret = amlogic_pdd_irq_get_ctrl_level(dev, fwspec->param[0]);
	if (ret < 0) {
		dev_err(dev, "Invalid voltage value: %d\n", fwspec->param[0]);
		return ret;
	}
	priv->ctrl_value = ret;

	irq_domain_set_hwirq_and_chip(domain, virq, 0, &amlogic_pdd_irqchip, dev);

	p_fwspec.fwnode = domain->parent->fwnode;
	p_fwspec.param_count = 3;
	p_fwspec.param[0] = GIC_SPI;
	p_fwspec.param[1] = priv->hw_irq;
	p_fwspec.param[2] = IRQ_TYPE_LEVEL_HIGH;

	ret = irq_domain_alloc_irqs_parent(domain, virq, 1, &p_fwspec);
	if (ret)
		return ret;

	return 0;
}

static int amlogic_pdd_domain_activate(struct irq_domain *domain,
				       struct irq_data *data, bool reserve)
{
	struct device *dev = irq_data_get_irq_chip_data(data);
	struct amlogic_pdd_priv *priv = dev_get_drvdata(dev);
	int ret;

	ret = amlogic_pdd_state_step(priv, STATE_ACTIVATE);
	if (ret)
		return ret;

	dev_dbg(dev, "IRQ activated\n");

	return 0;
}

static void amlogic_pdd_domain_deactivate(struct irq_domain *domain,
					  struct irq_data *data)
{
	struct device *dev = irq_data_get_irq_chip_data(data);
	struct amlogic_pdd_priv *priv = dev_get_drvdata(dev);

	amlogic_pdd_state_step(priv, STATE_IDLE);

	dev_dbg(dev, "IRQ deactivated\n");
}

static const struct irq_domain_ops amlogic_pdd_domain_ops = {
	.alloc	= amlogic_pdd_domain_alloc,
	.free	= irq_domain_free_irqs_common,
	.activate = amlogic_pdd_domain_activate,
	.deactivate = amlogic_pdd_domain_deactivate,
	.xlate = irq_domain_xlate_onecell,
};

static void amlogic_pdd_remove_irq(void *data)
{
	struct irq_domain *domain = data;
	struct device *dev = domain->host_data;

	irq_domain_remove(domain);
	dev_dbg(dev, "IRQ domain removed\n");
}

static int amlogic_pdd_add_irq(struct device *dev)
{
	int ret;
	struct irq_domain *parent_domain;
	struct irq_domain *domain;
	struct device_node *np = dev->of_node;

	parent_domain = irq_find_host(of_irq_find_parent(np));
	if (!parent_domain)
		return dev_err_probe(dev, -EINVAL, "GIC interrupt-parent not found\n");

	domain = irq_domain_add_hierarchy(parent_domain, 0, 1, np,
					  &amlogic_pdd_domain_ops, dev);
	if (!domain)
		return dev_err_probe(dev, -ENOMEM, "Could not register IRQ domain\n");

	ret = devm_add_action_or_reset(dev, amlogic_pdd_remove_irq, domain);
	if (ret) {
		amlogic_pdd_remove_irq(domain);
		return ret;
	}

	dev_dbg(dev, "IRQ domain added\n");

	return 0;
}

static int amlogic_pdd_probe(struct platform_device *pdev)
{
	struct amlogic_pdd_priv *priv;
	struct device *dev = &pdev->dev;
	void __iomem *base;
	struct resource *res;
	int ret;

	/* Allocate memory */
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return dev_err_probe(dev, -ENOMEM, "Failed allocating memory\n");
	dev_set_drvdata(dev, priv);

	priv->clk_core = devm_clk_get(dev, "core");
	if (IS_ERR(priv->clk_core))
		return dev_err_probe(dev, PTR_ERR(priv->clk_core), "Failed to get core clk\n");

	/* Parse DT */
	ret = amlogic_pdd_dt_parse(dev);
	if (ret)
		return ret;

	/* Get regmap */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res) {
		base = devm_ioremap_resource(dev, res);
		if (IS_ERR(base))
			return PTR_ERR(base);

		priv->regmap = devm_regmap_init_mmio(dev, base,
						     &amlogic_pdd_regmap_config);
		if (IS_ERR(priv->regmap))
			return PTR_ERR(priv->regmap);
	} else {
		/* From parent */
		priv->regmap = dev_get_regmap(dev->parent, NULL);
		if (!priv->regmap)
			return dev_err_probe(dev, -EINVAL, "Couldn't get parent's regmap\n");
	}

	priv->state = STATE_IDLE;
	spin_lock_init(&priv->lock);

	clk_prepare_enable(priv->clk_core);

	/* Add an interrupt controller */
	ret = amlogic_pdd_add_irq(dev);
	if (ret) {
		clk_disable_unprepare(priv->clk_core);
		return ret;
	}

	return 0;
}

static int amlogic_pdd_remove(struct platform_device *pdev)
{
	struct amlogic_pdd_priv *priv = dev_get_drvdata(&pdev->dev);

	clk_disable_unprepare(priv->clk_core);

	return 0;
}

static struct platform_driver amlogic_pdd_driver = {
	.probe		= amlogic_pdd_probe,
	.remove		= amlogic_pdd_remove,
	.driver		= {
		.name	= "amlogic-pdd",
		.of_match_table = amlogic_pdd_of_match,
	},
};

int __init amlogic_pdd_driver_init(void)
{
	return platform_driver_register(&amlogic_pdd_driver);
}

void __exit amlogic_pdd_driver_exit(void)
{
	platform_driver_unregister(&amlogic_pdd_driver);
}

MODULE_AUTHOR("Huqiang Qin <huqiang.qin@amlogic.com>");
MODULE_DESCRIPTION("Amlogic power down detection driver");
MODULE_LICENSE("GPL v2");
