// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/thermal.h>
#include <linux/amlogic/aml_indmc_sensor.h>

struct dmc_cooling_data {
	struct thermal_zone_device *tzd;
	struct thermal_cooling_device *tcd;
	enum dmc_type type;
	void __iomem *reg_dram_type;
	void __iomem *reg_dram_rate;
	void __iomem *reg_sensor;
	void __iomem **reg_dmc_refresh;
	void __iomem **reg_dmc_init;
	u32 sensor_mask; /*temperature related bits*/
	bool sensor_bits_need_reverse;
	int dmcreg_num; /*register numbers of setting refresh rate*/
	int dmcreg_data_num;
	u32 *dmc_data; /*data set to reg_dmc_refresh*/
	int dmc_idx; /*dmc temperatrue level*/
};

static int sensor_temp_num[TYPE_MAX] = {0, 4, 8, 0, 0, 0, 16};
static int coeff_ddr4[4] = {780, 780, 390, 195};
static int coeff_lpddr4[8] = {1560, 1560, 780, 390, 195, 97, 97, 48};
static int coeff_lpddr5[16] = {0};
static int *dmc_refresh_coeff[TYPE_MAX];
static u32 sensor_mask[TYPE_MAX][2] = {{0, 0}, {3, 4}, {0, 2}, {0, 0}, {0, 0}, {0, 0}, {0, 0}};
static u32 init_val[TYPE_MAX][2] = {{0, 0}, {0x400, 0x80000100}, {0x4, 0x80000100}, {0, 0},
	{0, 0}, {0, 0}, {0, 0}};

static int dmc_get_temp(void *p, int *temp)
{
	*temp = 25000;
	return 0;
}

static int aml_bits_reverse(int value, int len)
{
	int ret = 0, i;

	for (i = 0; i < len; i++) {
		if (i < len >> 1)
			ret += (value & (1 << i)) << (len - 1 - 2 * i);
		else if (i == len >> 1 && (len % 2 == 1))
			ret += (value & (1 << i));
		else
			ret += (value & (1 << i)) >> (2 * i + 1 - len);
	}

	return ret;
}

static void amlogic_dmc_hot_monitor(void *sensor_data)
{
	struct dmc_cooling_data *data = sensor_data;
	u32 val_sensor, val_dmc, temp;
	int i, idx, shift, len;

	shift = sensor_mask[data->type][0];
	val_sensor = readl_relaxed(data->reg_sensor);
	idx = (val_sensor & data->sensor_mask) >> shift;
	if (data->sensor_bits_need_reverse) {
		len = sensor_mask[data->type][1] - sensor_mask[data->type][0] + 1;
		idx = aml_bits_reverse(idx, len);
	}
	if (idx == data->dmc_idx)
		return;
	data->dmc_idx = idx;

	for (i = 0; i < data->dmcreg_num; i++) {
		temp = readl_relaxed(data->reg_dmc_refresh[i]);
		val_dmc = *(data->dmc_data + (i * data->dmcreg_data_num) + idx);
		val_dmc = (temp & 0xffff0000) + val_dmc;
		writel_relaxed(val_dmc, data->reg_dmc_refresh[i]);
		pr_info("temp %d, set reg_dmc_refresh%d 0x%x.\n", data->dmc_idx, i, val_dmc);
	}
}

static struct thermal_zone_of_device_ops dmc_sensor_ops = {
	.get_temp = dmc_get_temp,
	.hot = amlogic_dmc_hot_monitor
};

static int dmc_get_max_state(struct thermal_cooling_device *cdev,
			     unsigned long *state)
{
	*state = 0;

	return 0;
}

static int dmc_get_cur_state(struct thermal_cooling_device *cdev,
			     unsigned long *state)
{
	return 0;
}

static int dmc_set_cur_state(struct thermal_cooling_device *cdev,
			     unsigned long state)
{
	return 0;
}

static const struct thermal_cooling_device_ops indmc_cooling_ops = {
	.get_max_state = dmc_get_max_state,
	.get_cur_state = dmc_get_cur_state,
	.set_cur_state = dmc_set_cur_state
};

static void aml_init_global_variables(void)
{
	dmc_refresh_coeff[TYPE_DDR4] = coeff_ddr4;
	dmc_refresh_coeff[TYPE_LPDDR4] = coeff_lpddr4;
	dmc_refresh_coeff[TYPE_LPDDR5] = coeff_lpddr5;
}

static void aml_calc_sensor_mask(struct dmc_cooling_data *data)
{
	enum dmc_type type = data->type;
	int bit_start, bit_end;

	bit_start = sensor_mask[type][0];
	bit_end = sensor_mask[type][1];

	data->sensor_mask = ((1 << (bit_end - bit_start + 1)) - 1) * (1 << bit_start);
}

static int aml_setup_dmc_registers(struct device_node *np, struct dmc_cooling_data *data)
{
	int ret = -1, i;
	u32 reg_dram_type, reg_dram_rate, reg_sensor, *reg_dmc_refresh, reg_dmc_init[2];

	if (of_property_read_u32(np, "reg_dram_type", &reg_dram_type))
		goto out;
	else
		data->reg_dram_type = ioremap(reg_dram_type, 4);

	if (of_property_read_u32(np, "reg_dram_rate", &reg_dram_rate))
		goto out;
	else
		data->reg_dram_rate = ioremap(reg_dram_rate, 4);

	if (of_property_read_u32(np, "reg_sensor", &reg_sensor))
		goto out;
	else
		data->reg_sensor = ioremap(reg_sensor, 4);

	if (!data->reg_dram_type || !data->reg_dram_rate || !data->reg_sensor)
		goto out;
	data->dmcreg_num = of_property_count_u32_elems(np, "reg_dmc_refresh");
	if (data->dmcreg_num < 1)
		goto out;
	reg_dmc_refresh = kcalloc(data->dmcreg_num, sizeof(u32), GFP_KERNEL);
	if (!reg_dmc_refresh)
		goto out;
	if (of_property_read_u32_array(np, "reg_dmc_refresh", reg_dmc_refresh, data->dmcreg_num))
		goto free;
	data->reg_dmc_refresh = kmalloc_array(data->dmcreg_num, sizeof(void *), GFP_KERNEL);
	if (!data->reg_dmc_refresh)
		goto free;
	for (i = 0; i < data->dmcreg_num; i++) {
		data->reg_dmc_refresh[i] = ioremap(reg_dmc_refresh[i], 4);
		if (!data->reg_dmc_refresh[i])
			goto free1;
	}

	if (of_property_read_u32_array(np, "reg_dmc_init", reg_dmc_init, 2))
		goto free1;
	data->reg_dmc_init = kmalloc_array(2, sizeof(void *), GFP_KERNEL);
	if (!data->reg_dmc_init)
		goto free1;
	for (i = 0; i < 2; i++) {
		data->reg_dmc_init[i] = ioremap(reg_dmc_init[i], 4);
		if (!data->reg_dmc_init[i])
			goto free2;
	}
	ret = 0;
	goto free;
free2:
	kfree(data->reg_dmc_init);
free1:
	kfree(data->reg_dmc_refresh);
free:
	kfree(reg_dmc_refresh);
out:
	return ret;
}

static int aml_build_dmc_data(struct device_node *np, struct dmc_cooling_data *data)
{
	int ret = -1, i, j;
	u32 type, rate, temp;

	type = (readl_relaxed(data->reg_dram_type) & 0x0000ff00) >> 8;
	rate = readl_relaxed(data->reg_dram_rate) & 0x0000ffff;
	data->type = type;
	if (type >= TYPE_MAX)
		goto out;
	aml_calc_sensor_mask(data);
	data->dmcreg_data_num = sensor_temp_num[type];
	if (data->type == TYPE_DDR4)
		data->sensor_bits_need_reverse =
			of_property_read_bool(np, "sensor_bits_need_reverse");
	else
		data->sensor_bits_need_reverse = false;

	data->dmc_idx = -1;
	data->dmc_data = kzalloc(data->dmcreg_data_num * data->dmcreg_num * sizeof(u32),
			GFP_KERNEL);
	if (!data->dmc_data)
		goto out;

	for (i = 0; i < data->dmcreg_data_num; i++) {
		for (j = 0; j < data->dmcreg_num; j++) {
			temp = ((rate >> 1) - 1) * dmc_refresh_coeff[type][i] / 100;
			data->dmc_data[i +  j * data->dmcreg_data_num] = temp >> j;
		}
	}
	writel_relaxed(init_val[type][0], data->reg_dmc_init[0]);
	writel_relaxed(init_val[type][1], data->reg_dmc_init[1]);
	ret = 0;
out:
	return ret;
}

static int aml_indmc_sensor_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct dmc_cooling_data *data;
	int ret = -1, sensor_id;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		goto out;

	aml_init_global_variables();

	if (aml_setup_dmc_registers(np, data)) {
		pr_info("setup dmc related registers failed.\n");
		goto out;
	}
	if (aml_build_dmc_data(np, data)) {
		pr_info("build dmc data failed.\n");
		goto free1;
	}

	of_property_read_u32(np, "tsensor_id", &sensor_id);
	data->tzd = devm_thermal_zone_of_sensor_register(&pdev->dev, sensor_id, data,
							 &dmc_sensor_ops);
	if (IS_ERR(data->tzd)) {
		dev_err(&pdev->dev, "failed to register dmc internal sensor.\n");
		goto free2;
	}

	data->tcd = thermal_of_cooling_device_register(np, "indmc_cooling", data,
						      &indmc_cooling_ops);
	if (!data->tcd) {
		dev_err(&pdev->dev, "failed to register dmc internal cooling device.\n");
		goto free2;
	}
	ret = 0;
	goto out;

free2:
	kfree(data->dmc_data);
free1:
	kfree(data->reg_dmc_init);
	kfree(data->reg_dmc_refresh);
out:
	return ret;
}

static int aml_indmc_sensor_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id aml_indmc_sensor_of_match[] = {
	{ .compatible = "amlogic, indmc-tsensor" },
	{},
};

struct platform_driver aml_indmc_sensor_platdrv = {
	.driver = {
		.name		= "aml-indmc-sensor",
		.owner		= THIS_MODULE,
		.of_match_table = aml_indmc_sensor_of_match,
	},
	.probe	= aml_indmc_sensor_probe,
	.remove	= aml_indmc_sensor_remove
};
