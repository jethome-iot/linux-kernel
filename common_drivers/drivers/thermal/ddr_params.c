// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/printk.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/bitfield.h>
#include <linux/of_device.h>
#include <dt-bindings/thermal/ddr-params.h>
#include "ddr_control.h"

static u32 ddr_type_get(struct platform_device *pdev, const char *name,
		const char *type_name, bool *exist)
{
	u32 type = 0;
	struct resource *res;
	void __iomem *vaddr;
	struct device *dev = &pdev->dev;
	u32 mask[2] = {0, 0};
	int ret;

	*exist = false;
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, name);
	if (!IS_ERR_OR_NULL(res)) {
		vaddr = devm_ioremap_resource(dev, res);
		if (IS_ERR_OR_NULL(vaddr))
			return 0;
		ret = of_property_read_u32_array(dev->of_node, type_name, mask, 2);
		if (ret)
			goto type_iounmap;
		if ((mask[0] + mask[1]) > 32 || mask[1] == 0)
			goto type_iounmap;

		*exist = true;
		type = ((readl(vaddr) >> mask[0]) & ((u32)(BIT_ULL(mask[1]) - 1)));
type_iounmap:
		devm_iounmap(dev, vaddr);
		devm_release_mem_region(dev, res->start, resource_size(res));
	}

	return type;
}

static u32 ddr_type_init(struct platform_device *pdev, u32 *dflags)
{
	u32 type, value;
	bool exist = false;

	type = ddr_type_get(pdev, "type-addr", "type-mask", &exist) << DDR_TYPE_SHIFT;
	if (exist)
		*dflags = DDR_TYPE_FLAG;
	type |= (ddr_type_get(pdev, "sip-addr", "sip-mask", &exist) << DDR_SIP_SHIFT);
	if (exist)
		*dflags |= DDR_SIP_FLAG;
	type |= (ddr_type_get(pdev, "process-addr", "process-mask", &exist) << DDR_PROCESS_SHIFT);
	if (exist)
		*dflags |= DDR_PROCESS_FLAG;

	value = ddr_type_get(pdev, "rank0-addr", "rank0-mask", &exist);
	if (exist) {
		*dflags |= DDR_RANK0_FLAG;
		if (value)
			type |= DDR_RANK_16BIT << DDR_RANK0_SHIFT;
		else
			type |= DDR_RANK_32BIT << DDR_RANK0_SHIFT;
	}

	value = ddr_type_get(pdev, "rank1-addr", "rank1-mask", &exist);
	if (exist) {
		*dflags |= DDR_RANK1_FLAG;
		if (value)
			type |= DDR_RANK_16BIT << DDR_RANK1_SHIFT;
		else
			type |= DDR_RANK_32BIT << DDR_RANK1_SHIFT;
	}

	return type;
}

static int ddr_condition_init(struct control_device *cdev, struct device_node *of_node)
{
	struct ddr_control *ddr_control = cdev->ddr_control;
	struct ddr_thermal *thermal;
	struct ddr_dmc *dmc;
	const char *name;
	u32 reg = 0, msk_val[2] = {0, 0};
	int con_cnt, msk_cnt;
	int idx, ret;

	ret = of_property_read_u32(of_node, "condition-type", &ddr_control->cd_type);
	if (ret) {
		dev_err(cdev->dev, "condition-type error\n");
		return -EINVAL;
	}
	switch (ddr_control->cd_type) {
	case DDR_DMC:
		con_cnt = of_property_count_u32_elems(of_node, "dmc-reg");
		msk_cnt = of_property_count_u32_elems(of_node, "dmc-mask") / 2;
		if (con_cnt < 0 || msk_cnt < 0)
			return -EINVAL;
		if (con_cnt != msk_cnt || con_cnt == 0) {
			dev_err(cdev->dev, "condition and  mask count not equal\n");
			return -EINVAL;
		}

		dmc = devm_kzalloc(cdev->dev, con_cnt * sizeof(*dmc), GFP_KERNEL);
		if (!dmc)
			return -ENOMEM;

		for (idx = 0; idx < con_cnt; idx++) {
			ret = of_property_read_u32_index(of_node, "dmc-reg", idx, &reg);
			if (ret)
				return -EINVAL;
			dmc[idx].rd_addr = devm_ioremap(cdev->dev, reg, 4);
			ret = of_property_read_u32_index(of_node, "dmc-mask", idx * 2, &msk_val[0]);
			if (ret)
				return -EINVAL;
			ret = of_property_read_u32_index(of_node, "dmc-mask",
							idx * 2 + 1, &msk_val[1]);
			if (ret)
				return -EINVAL;
			if (msk_val[0] + msk_val[1] > 32 || msk_val[1] == 0)
				return -EINVAL;
			dmc[idx].mask = ((u32)(BIT_ULL(msk_val[1]) - 1)) << msk_val[0];
			dmc[idx].offset = msk_val[0];
		}
		ddr_control->condition = dmc;
		ddr_control->con_cnt = con_cnt;
		break;
	case DDR_THERMAL:
		thermal = devm_kzalloc(cdev->dev, sizeof(*thermal), GFP_KERNEL);
		ddr_control->condition = thermal;
		ret = of_property_read_string(of_node, "zone-name", &name);
		if (ret)
			return ret;
		thermal->tzd = thermal_zone_get_zone_by_name(name);
		if (IS_ERR(thermal->tzd)) {
			dev_err(cdev->dev, "fail to get thermal zone %ld\n",
				PTR_ERR(thermal->tzd));
			return PTR_ERR(thermal->tzd);
		}
		ret = of_property_read_u32(of_node, "hyst-temp", &thermal->hyst_temp);
		if (ret)
			thermal->hyst_temp = 1000;
		break;
	default:
		return -EINVAL;
	};
	return 0;
}

static int ddr_calc_dynamic_params(struct control_device *cdev, struct device_node *of_node)
{
	struct device *dev = cdev->dev;
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct ddr_control *ddr_control = cdev->ddr_control;
	struct ddr_cstep *cstep = ddr_control->ddr_cstep;
	void __iomem *vaddr;
	struct ddr_caddr *caddr;
	u32 msk_val[2] = {0, 0}, freq = 2002;
	int idx, idx0;
	bool is_constant;
	u32 freq_div[2] = {0, 0};
	int ret;

	is_constant = of_property_read_bool(of_node, "refresh-constant-value");
	if (is_constant)
		return 0;

	vaddr = devm_platform_ioremap_resource_byname(pdev, "freq-addr");
	if (!IS_ERR_OR_NULL(vaddr)) {
		ret = of_property_read_u32_array(dev->of_node, "freq-mask", msk_val, 2);
		if (ret)
			return ret;
		if ((msk_val[0] + msk_val[1]) > 32 || msk_val[1] == 0)
			return -EINVAL;
		freq = (readl(vaddr) >> msk_val[0]) & (((u32)BIT_ULL(msk_val[1])) - 1);
	}
	ret = of_property_read_u32_array(of_node, "freq-divisor", freq_div, 2);
	if (ret) {
		freq_div[0] = 1;
		freq_div[1] = 2;
	} else if (freq_div[1] == 0) {
		dev_err(cdev->dev, "freq_div[1] is error, the denominator can't be zero\n");
		return -EINVAL;
	}
	for (idx = 0; idx < cstep->caddr_cnt; idx++) {
		caddr = &cstep->caddr_tab[idx];
		for (idx0 = 0; idx0 < caddr->tab_cnt; idx0++) {
			caddr->table[idx0] = caddr->table[idx0] *
					     ((freq * freq_div[0] / freq_div[1]) - 1) / 1000;
			dev_dbg(cdev->dev, "val idx %d value %d\n", idx0, caddr->table[idx0]);
		}
	}
	return 0;
}

static int ddr_params_dmc_init(struct control_device *cdev, struct device_node *of_node)
{
	struct ddr_control *ddr_control = cdev->ddr_control;
	struct init_tab *init_tab;
	u32 msk_val[2] = {0, 0};
	int ret, idx;

	ret = of_property_count_u32_elems(of_node, "dmc-init");
	if (ret > 1) {
		ddr_control->init_cnt = ret / 2;
		init_tab = devm_kzalloc(cdev->dev, (ret / 2) * sizeof(*init_tab), GFP_KERNEL);
		for (idx = 0; idx < ddr_control->init_cnt; idx++) {
			ret = of_property_read_u32_index(of_node, "dmc-init", idx * 2, &msk_val[0]);
			if (ret)
				return -EINVAL;
			ret = of_property_read_u32_index(of_node, "dmc-init",
							idx * 2 + 1, &msk_val[1]);
			if (ret)
				return -EINVAL;
			init_tab[idx].vaddr = devm_ioremap(cdev->dev, msk_val[0], 4);
			init_tab[idx].val = msk_val[1];
			writel(init_tab[idx].val, init_tab[idx].vaddr);
		}
		ddr_control->init_tab = init_tab;
	}
	return 0;
}

static int ddr_params_caddr_init(struct control_device *cdev, struct device_node *of_node)
{
	struct ddr_control *ddr_control = cdev->ddr_control;
	struct device *dev = cdev->dev;
	struct ddr_cstep *cstep;
	struct ddr_caddr *caddr;
	u32 caddr_cnt, cmsk_cnt, cval_cnt, cstep_cnt, cval = 0;
	u32 tab_idx, addr_idx, reg = 0, msk_val[2] = {0, 0};
	int idx, ret;

	caddr_cnt = of_property_count_u32_elems(of_node, "caddr");
	cmsk_cnt = of_property_count_u32_elems(of_node, "caddr-mask") / 2;
	if (caddr_cnt != cmsk_cnt || caddr_cnt == 0) {
		dev_err(cdev->dev, "caddr and mask count not equal\n");
		return -EINVAL;
	}
	ret = of_property_count_u32_elems(of_node, "caddr-val");
	if (ret < 0) {
		dev_err(dev, "%s ret = %d\n", __func__, ret);
		return -EINVAL;
	}
	cval_cnt = ret / caddr_cnt;
	if (cval_cnt == 0) {
		dev_err(dev, "%s cval_cnt = 0\n", __func__);
		return -EINVAL;
	}

	caddr = devm_kzalloc(cdev->dev, caddr_cnt * sizeof(*caddr), GFP_KERNEL);
	if (!caddr)
		return -ENOMEM;

	for (idx = 0; idx < caddr_cnt; idx++) {
		ret = of_property_read_u32_index(of_node, "caddr", idx, &reg);
		if (ret)
			return -EINVAL;
		caddr[idx].wr_addr = devm_ioremap(cdev->dev, reg, 4);
		ret = of_property_read_u32_index(of_node, "caddr-mask", idx * 2, &msk_val[0]);
		if (ret)
			return -EINVAL;
		ret = of_property_read_u32_index(of_node, "caddr-mask", idx * 2 + 1, &msk_val[1]);
		if (ret)
			return -EINVAL;
		if (msk_val[0] + msk_val[1] > 32 || msk_val[1] == 0)
			return -EINVAL;
		caddr[idx].wr_mask = ((u32)(BIT_ULL(msk_val[1]) - 1)) << msk_val[0];
		caddr[idx].wr_offset = msk_val[0];
		caddr[idx].table = devm_kzalloc(cdev->dev, cval_cnt * sizeof(u32), GFP_KERNEL);
		if (!caddr[idx].table)
			return -ENOMEM;
		caddr[idx].tab_cnt = cval_cnt;
	}

	for (idx = 0; idx < cval_cnt * caddr_cnt; idx++) {
		ret = of_property_read_u32_index(of_node, "caddr-val", idx, &cval);
		if (ret)
			return -EINVAL;
		addr_idx = idx % caddr_cnt;
		tab_idx = idx / caddr_cnt;
		caddr[addr_idx].table[tab_idx] = cval;
	}

	ret = of_property_count_u32_elems(of_node, "cstep-val");
	if (ret < 0)
		cstep_cnt = cval_cnt;
	else
		cstep_cnt = ret;

	if (cstep_cnt != cval_cnt) {
		dev_err(cdev->dev, "cstep_cnt and cval_cnt not equal\n");
		return -EINVAL;
	}

	cstep = devm_kzalloc(cdev->dev, cstep_cnt * sizeof(*cstep), GFP_KERNEL);
	for (idx = 0; idx < cstep_cnt; idx++) {
		ret = of_property_read_u32_index(of_node, "cstep-val", idx, &cval);
		if (ret)
			cstep[idx].temp = idx;
		else
			cstep[idx].temp = cval;
		cstep[idx].caddr_tab = caddr;
		cstep[idx].caddr_cnt = caddr_cnt;
	}
	ddr_control->ddr_cstep = cstep;
	ddr_control->cstep_cnt = cstep_cnt;

	return 0;
}

static int ddr_params_data_init(struct control_device *cdev, struct device_node *of_node)
{
	struct ddr_control *ddr_control;
	int ret;

	ddr_control = devm_kzalloc(cdev->dev, sizeof(*ddr_control), GFP_KERNEL);
	if (!ddr_control)
		return -ENOMEM;
	cdev->ddr_control = ddr_control;

	ret = ddr_condition_init(cdev, of_node);
	if (ret)
		return ret;
	ret = ddr_params_caddr_init(cdev, of_node);
	if (ret)
		return ret;

	ddr_calc_dynamic_params(cdev, of_node);
	ddr_params_dmc_init(cdev, of_node);
	cdev->last_idx = UINT_MAX;

	return 0;
}

static bool ddr_domain_match(u32 type, u32 ptype, u32 pflags, u32 cflag)
{
	u32 shift, mask;

	if ((pflags & cflag) == 0)
		return true;

	switch (cflag) {
	case DDR_PROCESS_FLAG:
		shift = DDR_PROCESS_SHIFT;
		mask = 0xff;
		break;
	case DDR_TYPE_FLAG:
		shift = DDR_TYPE_SHIFT;
		mask = 0xf;
		break;
	case DDR_SIP_FLAG:
		shift = DDR_SIP_SHIFT;
		mask = 0xff;
		break;
	case DDR_RANK0_FLAG:
		shift = DDR_RANK0_SHIFT;
		mask = 0x1;
		break;
	case DDR_RANK1_FLAG:
		shift = DDR_RANK1_SHIFT;
		mask = 0x1;
		break;
	default:
		return false;
	}
	if (((type >> shift) & mask) == ((ptype >> shift) & mask))
		return true;

	return false;
}

static bool ddr_particle_compare(u32 type, u32 ptype, u32 pflags)
{
	bool process_match, type_match, sip_match;
	bool bank0_match, bank1_match;

	process_match = ddr_domain_match(type, ptype, pflags, DDR_PROCESS_FLAG);
	type_match = ddr_domain_match(type, ptype, pflags, DDR_TYPE_FLAG);
	sip_match = ddr_domain_match(type, ptype, pflags, DDR_SIP_FLAG);
	bank0_match = ddr_domain_match(type, ptype, pflags, DDR_RANK0_FLAG);
	bank1_match = ddr_domain_match(type, ptype, pflags, DDR_RANK1_FLAG);

	return process_match && type_match && sip_match &&
		bank0_match && bank1_match;
}

static struct device_node *ddr_params_node_search(struct device *dev, u32 type, u32 dflags)
{
	struct device_node *child, *node = NULL;
	u32 ptype = 0, pflags = 0;
	int ret;
	bool match;

	for_each_available_child_of_node(dev->of_node, child) {
		ret = of_property_read_u32(child, "particle", &ptype);
		if (ret < 0)
			ptype = 0;
		ret = of_property_read_u32(child, "particle-flag", &pflags);
		if (ret < 0)
			pflags = 0;

		if ((dflags & pflags) != pflags)
			return NULL;

		match = ddr_particle_compare(type, ptype, pflags);
		if (match)
			node = child;

		if (match && pflags != 0)
			break;
	}
	return node;
}

static int ddr_params_init(struct control_device *cdev)
{
	struct device *dev = cdev->dev;
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct device_node *of_node = NULL;
	u32 type, dflags = 0;
	int ret;

	type = ddr_type_init(pdev, &dflags);
	of_node = ddr_params_node_search(dev, type, dflags);
	if (!of_node)
		return -ENODEV;

	ret = ddr_params_data_init(cdev, of_node);
	return ret;
}

static int ddr_control_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct control_device *cdev;
	int ret;

	cdev = devm_kzalloc(dev, sizeof(*cdev), GFP_KERNEL);
	if (!cdev)
		return -ENOMEM;
	cdev->dev = dev;
	dev_set_drvdata(dev, cdev);

	ret = ddr_params_init(cdev);
	if (ret)
		return ret;

	ret = of_property_read_u32(dev->of_node, "poll-time-ms", &cdev->poll_time_ms);
	if (ret)
		cdev->poll_time_ms = DDR_DEFAULT_POLLING_MS;
	ddr_control_device(cdev);
	INIT_DELAYED_WORK(&cdev->poll_dwork, control_poll_work);
	schedule_delayed_work(&cdev->poll_dwork,
			msecs_to_jiffies(cdev->poll_time_ms));

	dev_dbg(dev, "probe done\n");
	return 0;
}

static int ddr_control_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id control_of_match[] = {
	{
		.compatible = "amlogic, ddr-control",
	},
	{},
};

struct platform_driver ddr_control_platdrv = {
	.driver = {
		.name		= "ddr-control",
		.owner		= THIS_MODULE,
		.of_match_table = control_of_match,
	},
	.probe	= ddr_control_probe,
	.remove	= ddr_control_remove,
};
