// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/amlogic/media/vout/DisplayPort/DPTX.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/media/vout/vclk_serve.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include "dptx_common.h"
#include "dptx_reg_op.h"

#define DPTX_REG_MAP_PERIPHS     0
#define DPTX_REG_MAP_EDP         1
#define DPTX_REG_MAP_COMBO_DPHY  2
#define DPTX_REG_MAP_RST         3
#define DPTX_REG_MAP_MAX         4

int dptx_reg_t7[] = {
	DPTX_REG_MAP_COMBO_DPHY,
	DPTX_REG_MAP_EDP,
	DPTX_REG_MAP_RST,
	DPTX_REG_MAP_PERIPHS,
	DPTX_REG_MAP_MAX,
};

#define DPTX_REG_OFFSET(reg)                   (((reg) << 2))

/* for lcd reg access */
spinlock_t dptx_reg_spinlock;

int dptx_ioremap(struct dptx_drv_s *dptx, struct platform_device *pdev)
{
	struct dptx_reg_map_s *reg_map = NULL;
	struct resource *res;
	int *table;
	int i = 0;

	if (!dptx->data) {
		DPTXPR(dptx->idx, LOG_E, "%s: reg_map table is null", __func__);
		return -1;
	}

	switch (dptx->data->chip_type) {
	case DPTX_CHIP_T7:
		table = dptx_reg_t7;
		break;
	default:
		DPTXPR(dptx->idx, LOG_E, "%s: unknown chip", __func__);
		return -1;
	}

	dptx->reg_map = kcalloc(DPTX_REG_MAP_MAX, sizeof(struct dptx_reg_map_s), GFP_KERNEL);
	if (!dptx->reg_map)
		return -1;
	reg_map = dptx->reg_map;

	spin_lock_init(&dptx_reg_spinlock);

	while (i < DPTX_REG_MAP_MAX && table[i] < DPTX_REG_MAP_MAX) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			DPTXPR(dptx->idx, LOG_E, "%s: get resource %d error", __func__, i);
			goto dptx_ioremap_err;
		}
		reg_map[table[i]].base_addr = res->start;
		reg_map[table[i]].size = resource_size(res);
		reg_map[table[i]].p = devm_ioremap(&pdev->dev, res->start, reg_map[table[i]].size);
		if (!reg_map[table[i]].p) {
			reg_map[table[i]].flag = 0;
			DPTXPR(dptx->idx, LOG_E, "%s: reg %d failed: 0x%x 0x%x", __func__, i,
			       reg_map[table[i]].base_addr, reg_map[table[i]].size);
			goto dptx_ioremap_err;
		}
		reg_map[table[i]].flag = 1;
		DPTXPR(dptx->idx, LOG_I, "%s: reg %d: 0x%x -> 0x%px, size: 0x%x", __func__, i,
			reg_map[table[i]].base_addr, reg_map[table[i]].p, reg_map[table[i]].size);

		i++;
	}

	return 0;

dptx_ioremap_err:
	kfree(reg_map);
	dptx->reg_map = NULL;
	return -1;
}

static int dptx_check_ioremap(struct dptx_drv_s *dptx, u32 n)
{
	if (!dptx->reg_map) {
		DPTXPR(dptx->idx, LOG_E, "%s: reg_map is null", __func__);
		return -1;
	}
	if (n >= DPTX_REG_MAP_MAX)
		return -1;
	if (dptx->reg_map[n].flag)
		return 0;

	DPTXPR(dptx->idx, LOG_E, "%s: reg 0x%x map error", __func__, dptx->reg_map[n].base_addr);
	return -1;
}

static inline void __iomem *dptx_check_periphs_reg(struct dptx_drv_s *dptx, u32 reg)
{
	if (dptx_check_ioremap(dptx, DPTX_REG_MAP_PERIPHS))
		return NULL;
	if (DPTX_REG_OFFSET(reg) >= dptx->reg_map[DPTX_REG_MAP_PERIPHS].size) {
		DPTXPR(dptx->idx, LOG_E, "invalid periphs reg offset: 0x%04x", reg);
		return NULL;
	}
	return dptx->reg_map[DPTX_REG_MAP_PERIPHS].p + DPTX_REG_OFFSET(reg);
}

static inline void __iomem *dptx_check_IP_reg(struct dptx_drv_s *dptx, u32 reg)
{
	if (dptx_check_ioremap(dptx, DPTX_REG_MAP_EDP))
		return NULL;
	if (reg >= dptx->reg_map[DPTX_REG_MAP_EDP].size) {
		DPTXPR(dptx->idx, LOG_E, "invalid dptx reg offset: 0x%04x", reg);
		return NULL;
	}
	return dptx->reg_map[DPTX_REG_MAP_EDP].p + reg;
}

static inline void __iomem *dptx_check_combo_dphy_reg(struct dptx_drv_s *dptx, u32 reg)
{
	if (dptx_check_ioremap(dptx, DPTX_REG_MAP_COMBO_DPHY))
		return NULL;
	if (DPTX_REG_OFFSET(reg) >= dptx->reg_map[DPTX_REG_MAP_COMBO_DPHY].size) {
		DPTXPR(dptx->idx, LOG_E, "invalid combo dphy reg offset: 0x%04x", reg);
		return NULL;
	}
	return dptx->reg_map[DPTX_REG_MAP_COMBO_DPHY].p + DPTX_REG_OFFSET(reg);
}

static inline void __iomem *dptx_check_reset_reg(struct dptx_drv_s *dptx, u32 reg)
{
	if (dptx_check_ioremap(dptx, DPTX_REG_MAP_RST))
		return NULL;
	if (DPTX_REG_OFFSET(reg) >= dptx->reg_map[DPTX_REG_MAP_RST].size) {
		DPTXPR(dptx->idx, LOG_E, "invalid reset reg offset: 0x%04x\n", reg);
		return NULL;
	}
	return dptx->reg_map[DPTX_REG_MAP_RST].p + DPTX_REG_OFFSET(reg);
}

/******************************************************/
u32 dptx_vcbus_read(u32 reg)
{
#ifdef CONFIG_AMLOGIC_VPU
	u32 temp;
	unsigned long flags = 0;

	spin_lock_irqsave(&dptx_reg_spinlock, flags);
	temp = vpu_vcbus_read(reg);
	spin_unlock_irqrestore(&dptx_reg_spinlock, flags);
	return temp;
#endif
};

void dptx_vcbus_write(u32 reg, u32 value)
{
#ifdef CONFIG_AMLOGIC_VPU
	unsigned long flags = 0;

	spin_lock_irqsave(&dptx_reg_spinlock, flags);
	vpu_vcbus_write(reg, value);
	spin_unlock_irqrestore(&dptx_reg_spinlock, flags);
#endif
};

void dptx_vcbus_setb(u32 reg, u32 value, u32 start, u32 len)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&dptx_reg_spinlock, flags);
	vpu_vcbus_write(reg, ((vpu_vcbus_read(reg) &
		(~(((1L << len) - 1) << start))) |
		((value & ((1L << len) - 1)) << start)));
	spin_unlock_irqrestore(&dptx_reg_spinlock, flags);
}

u32 dptx_vcbus_getb(u32 reg, u32 start, u32 len)
{
	u32 temp;
	unsigned long flags = 0;

	spin_lock_irqsave(&dptx_reg_spinlock, flags);
	temp = (vpu_vcbus_read(reg) >> start) & ((1L << len) - 1);
	spin_unlock_irqrestore(&dptx_reg_spinlock, flags);

	return temp;
}

u32 dptx_clk_read(u32 reg)
{
	u32 temp;
	unsigned long flags = 0;

	spin_lock_irqsave(&dptx_reg_spinlock, flags);
	temp =  vclk_clk_reg_read(reg);
	spin_unlock_irqrestore(&dptx_reg_spinlock, flags);

	return temp;
};

void dptx_clk_write(u32 reg, u32 val)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&dptx_reg_spinlock, flags);
	vclk_clk_reg_write(reg, val);
	spin_unlock_irqrestore(&dptx_reg_spinlock, flags);
};

void dptx_clk_setb(u32 reg, u32 val, u32 start, u32 len)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&dptx_reg_spinlock, flags);
	vclk_clk_reg_write(reg, ((vclk_clk_reg_read(reg) &
			   ~(((1L << (len)) - 1) << (start))) |
			   (((val) & ((1L << (len)) - 1)) << (start))));
	spin_unlock_irqrestore(&dptx_reg_spinlock, flags);
}

u32 dptx_clk_getb(u32 reg, u32 start, u32 len)
{
	u32 temp;
	unsigned long flags = 0;

	spin_lock_irqsave(&dptx_reg_spinlock, flags);
	temp =  (vclk_clk_reg_read(reg) >> (start)) & ((1L << (len)) - 1);
	spin_unlock_irqrestore(&dptx_reg_spinlock, flags);

	return temp;
}

u32 dptx_ana_read(u32 reg)
{
	u32 temp;
	unsigned long flags = 0;

	spin_lock_irqsave(&dptx_reg_spinlock, flags);
	temp =  vclk_ana_reg_read(reg);
	spin_unlock_irqrestore(&dptx_reg_spinlock, flags);

	return temp;
}

void dptx_ana_write(u32 reg, u32 val)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&dptx_reg_spinlock, flags);
	vclk_ana_reg_write(reg, val);
	spin_unlock_irqrestore(&dptx_reg_spinlock, flags);
}

void dptx_ana_setb(u32 reg, u32 val, u32 start, u32 len)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&dptx_reg_spinlock, flags);
	vclk_ana_reg_write(reg, ((vclk_ana_reg_read(reg) &
			(~(((1L << len) - 1) << start))) | ((val & ((1L << len) - 1)) << start)));
	spin_unlock_irqrestore(&dptx_reg_spinlock, flags);
}

u32 dptx_ana_getb(u32 reg, u32 start, u32 len)
{
	u32 temp;
	unsigned long flags = 0;

	spin_lock_irqsave(&dptx_reg_spinlock, flags);
	temp = (vclk_ana_reg_read(reg) >> (start)) & ((1L << (len)) - 1);
	spin_unlock_irqrestore(&dptx_reg_spinlock, flags);

	return temp;
}

u32 dptx_periphs_read(struct dptx_drv_s *dptx, u32 reg)
{
	void __iomem *p;
	u32 temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&dptx_reg_spinlock, flags);
	p = dptx_check_periphs_reg(dptx, reg);
	if (p)
		temp = readl(p);
	spin_unlock_irqrestore(&dptx_reg_spinlock, flags);

	return temp;
};

void dptx_periphs_write(struct dptx_drv_s *dptx, u32 reg, u32 val)
{
	void __iomem *p;
	unsigned long flags = 0;

	spin_lock_irqsave(&dptx_reg_spinlock, flags);
	p = dptx_check_periphs_reg(dptx, reg);
	if (p)
		writel(val, p);
	spin_unlock_irqrestore(&dptx_reg_spinlock, flags);
};

u32 __dptx_reg_read(struct dptx_drv_s *dptx, u32 reg)
{
	void __iomem *p;
	u32 temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&dptx_reg_spinlock, flags);
	p = dptx_check_IP_reg(dptx, reg);
	if (p)
		temp = readl(p);
	spin_unlock_irqrestore(&dptx_reg_spinlock, flags);

	return temp;
};

void __dptx_reg_write(struct dptx_drv_s *dptx, u32 reg, u32 val)
{
	void __iomem *p;
	unsigned long flags = 0;

	spin_lock_irqsave(&dptx_reg_spinlock, flags);
	p = dptx_check_IP_reg(dptx, reg);
	if (p)
		writel(val, p);
	spin_unlock_irqrestore(&dptx_reg_spinlock, flags);
};

void __dptx_reg_setb(struct dptx_drv_s *dptx, u32 reg, u32 value, u32 start, u32 len)
{
	void __iomem *p;
	u32 temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&dptx_reg_spinlock, flags);
	p = dptx_check_IP_reg(dptx, reg);
	if (p) {
		temp = readl(p);
		temp = (temp & (~(((1L << len) - 1) << start))) |
			((value & ((1L << len) - 1)) << start);
		writel(temp, p);
	}
	spin_unlock_irqrestore(&dptx_reg_spinlock, flags);
}

u32 __dptx_reg_getb(struct dptx_drv_s *dptx, u32 reg, u32 start, u32 len)
{
	void __iomem *p;
	u32 temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&dptx_reg_spinlock, flags);
	p = dptx_check_IP_reg(dptx, reg);
	if (p) {
		temp = readl(p);
		temp = (temp >> start) & ((1L << len) - 1);
	}
	spin_unlock_irqrestore(&dptx_reg_spinlock, flags);

	return temp;
}

u32 dptx_combo_dphy_read(struct dptx_drv_s *dptx, u32 reg)
{
	void __iomem *p;
	u32 temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&dptx_reg_spinlock, flags);
	p = dptx_check_combo_dphy_reg(dptx, reg);
	if (p)
		temp = readl(p);
	spin_unlock_irqrestore(&dptx_reg_spinlock, flags);

	return temp;
};

void dptx_combo_dphy_write(struct dptx_drv_s *dptx, u32 reg, u32 val)
{
	void __iomem *p;
	unsigned long flags = 0;

	spin_lock_irqsave(&dptx_reg_spinlock, flags);
	p = dptx_check_combo_dphy_reg(dptx, reg);
	if (p)
		writel(val, p);
	spin_unlock_irqrestore(&dptx_reg_spinlock, flags);
};

void dptx_combo_dphy_setb(struct dptx_drv_s *dptx, u32 reg, u32 value, u32 start, u32 len)
{
	void __iomem *p;
	u32 temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&dptx_reg_spinlock, flags);
	p = dptx_check_combo_dphy_reg(dptx, reg);
	if (p) {
		temp = readl(p);
		temp = (temp & (~(((1L << len) - 1) << start))) |
			((value & ((1L << len) - 1)) << start);
		writel(temp, p);
	}
	spin_unlock_irqrestore(&dptx_reg_spinlock, flags);
}

u32 dptx_combo_dphy_getb(struct dptx_drv_s *dptx, u32 reg, u32 start, u32 len)
{
	void __iomem *p;
	u32 temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&dptx_reg_spinlock, flags);
	p = dptx_check_combo_dphy_reg(dptx, reg);
	if (p) {
		temp = readl(p);
		temp = (temp >> start) & ((1L << len) - 1);
	}
	spin_unlock_irqrestore(&dptx_reg_spinlock, flags);

	return temp;
}

u32 dptx_reset_read(struct dptx_drv_s *dptx, u32 reg)
{
	void __iomem *p;
	u32 temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&dptx_reg_spinlock, flags);
	p = dptx_check_reset_reg(dptx, reg);
	if (p)
		temp = readl(p);
	spin_unlock_irqrestore(&dptx_reg_spinlock, flags);

	return temp;
};

void dptx_reset_write(struct dptx_drv_s *dptx, u32 reg, u32 val)
{
	void __iomem *p;
	unsigned long flags = 0;

	spin_lock_irqsave(&dptx_reg_spinlock, flags);
	p = dptx_check_reset_reg(dptx, reg);
	if (p)
		writel(val, p);
	spin_unlock_irqrestore(&dptx_reg_spinlock, flags);
};

void dptx_reset_setb(struct dptx_drv_s *dptx, u32 reg, u32 value, u32 start, u32 len)
{
	void __iomem *p;
	u32 temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&dptx_reg_spinlock, flags);
	p = dptx_check_reset_reg(dptx, reg);
	if (p) {
		temp = readl(p);
		temp = (temp & (~(((1L << len) - 1) << start))) |
			((value & ((1L << len) - 1)) << start);
		writel(temp, p);
	}
	spin_unlock_irqrestore(&dptx_reg_spinlock, flags);
}

u32 dptx_reset_getb(struct dptx_drv_s *dptx, u32 reg, u32 start, u32 len)
{
	void __iomem *p;
	u32 temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&dptx_reg_spinlock, flags);
	p = dptx_check_reset_reg(dptx, reg);
	if (p) {
		temp = readl(p);
		temp = (temp >> start) & ((1L << len) - 1);
	}
	spin_unlock_irqrestore(&dptx_reg_spinlock, flags);

	return temp;
}
