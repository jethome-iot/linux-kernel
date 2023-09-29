// SPDX-License-Identifier: GPL-2.0
/*
 * SMP support for Allwinner SoCs
 *
 * Copyright (C) 2013 Maxime Ripard
 *
 * Maxime Ripard <maxime.ripard@free-electrons.com>
 *
 * Based on code
 *  Copyright (C) 2012-2013 Allwinner Ltd.
 *
 */

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/memory.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/smp.h>

#define CPUCFG_CPU_PWR_CLAMP_STATUS_REG(cpu)	((cpu) * 0x40 + 0x64)
#define CPUCFG_CPU_RST_CTRL_REG(cpu)		(((cpu) + 1) * 0x40)
#define CPUCFG_CPU_CTRL_REG(cpu)		(((cpu) + 1) * 0x40 + 0x04)
#define CPUCFG_CPU_STATUS_REG(cpu)		(((cpu) + 1) * 0x40 + 0x08)
#define CPUCFG_GEN_CTRL_REG			0x184
#define CPUCFG_PRIVATE0_REG			0x1a4
#define CPUCFG_PRIVATE1_REG			0x1a8
#define CPUCFG_DBG_CTL0_REG			0x1e0
#define CPUCFG_DBG_CTL1_REG			0x1e4

#define PRCM_CPU_PWROFF_REG			0x100
#define PRCM_CPU_PWR_CLAMP_REG(cpu)		(((cpu) * 4) + 0x140)

#define SUN8I_R528_C0_CPUX_CFG			(volatile void __iomem *)0x09010000
#define SUN8I_R528_C0_RST_CTRL			0x00
#define SUN8I_R528_C0_CTRL_REG0			(0x0010)

static void __iomem *cpucfg_membase;
static void __iomem *prcm_membase;

static DEFINE_SPINLOCK(cpu_lock);

static void __init sun6i_smp_prepare_cpus(unsigned int max_cpus)
{
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "allwinner,sun6i-a31-prcm");
	if (!node) {
		pr_err("Missing A31 PRCM node in the device tree\n");
		return;
	}

	prcm_membase = of_iomap(node, 0);
	of_node_put(node);
	if (!prcm_membase) {
		pr_err("Couldn't map A31 PRCM registers\n");
		return;
	}

	node = of_find_compatible_node(NULL, NULL,
				       "allwinner,sun6i-a31-cpuconfig");
	if (!node) {
		pr_err("Missing A31 CPU config node in the device tree\n");
		return;
	}

	cpucfg_membase = of_iomap(node, 0);
	of_node_put(node);
	if (!cpucfg_membase)
		pr_err("Couldn't map A31 CPU config registers\n");

}

static int sun6i_smp_boot_secondary(unsigned int cpu,
				    struct task_struct *idle)
{
	u32 reg;
	int i;

	if (!(prcm_membase && cpucfg_membase))
		return -EFAULT;

	spin_lock(&cpu_lock);

	/* Set CPU boot address */
	writel(__pa_symbol(secondary_startup),
	       cpucfg_membase + CPUCFG_PRIVATE0_REG);

	/* Assert the CPU core in reset */
	writel(0, cpucfg_membase + CPUCFG_CPU_RST_CTRL_REG(cpu));

	/* Assert the L1 cache in reset */
	reg = readl(cpucfg_membase + CPUCFG_GEN_CTRL_REG);
	writel(reg & ~BIT(cpu), cpucfg_membase + CPUCFG_GEN_CTRL_REG);

	/* Disable external debug access */
	reg = readl(cpucfg_membase + CPUCFG_DBG_CTL1_REG);
	writel(reg & ~BIT(cpu), cpucfg_membase + CPUCFG_DBG_CTL1_REG);

	/* Power up the CPU */
	for (i = 0; i <= 8; i++)
		writel(0xff >> i, prcm_membase + PRCM_CPU_PWR_CLAMP_REG(cpu));
	mdelay(10);

	/* Clear CPU power-off gating */
	reg = readl(prcm_membase + PRCM_CPU_PWROFF_REG);
	writel(reg & ~BIT(cpu), prcm_membase + PRCM_CPU_PWROFF_REG);
	mdelay(1);

	/* Deassert the CPU core reset */
	writel(3, cpucfg_membase + CPUCFG_CPU_RST_CTRL_REG(cpu));

	/* Enable back the external debug accesses */
	reg = readl(cpucfg_membase + CPUCFG_DBG_CTL1_REG);
	writel(reg | BIT(cpu), cpucfg_membase + CPUCFG_DBG_CTL1_REG);

	spin_unlock(&cpu_lock);

	return 0;
}

static const struct smp_operations sun6i_smp_ops __initconst = {
	.smp_prepare_cpus	= sun6i_smp_prepare_cpus,
	.smp_boot_secondary	= sun6i_smp_boot_secondary,
};
CPU_METHOD_OF_DECLARE(sun6i_a31_smp, "allwinner,sun6i-a31", &sun6i_smp_ops);

static void __init sun8i_smp_prepare_cpus(unsigned int max_cpus)
{
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "allwinner,sun8i-a23-prcm");
	if (!node) {
		pr_err("Missing A23 PRCM node in the device tree\n");
		return;
	}

	prcm_membase = of_iomap(node, 0);
	of_node_put(node);
	if (!prcm_membase) {
		pr_err("Couldn't map A23 PRCM registers\n");
		return;
	}

	node = of_find_compatible_node(NULL, NULL,
				       "allwinner,sun8i-a23-cpuconfig");
	if (!node) {
		pr_err("Missing A23 CPU config node in the device tree\n");
		return;
	}

	cpucfg_membase = of_iomap(node, 0);
	of_node_put(node);
	if (!cpucfg_membase)
		pr_err("Couldn't map A23 CPU config registers\n");

}

static int sun8i_smp_boot_secondary(unsigned int cpu,
				    struct task_struct *idle)
{
	u32 reg;

	if (!(prcm_membase && cpucfg_membase))
		return -EFAULT;

	spin_lock(&cpu_lock);

	/* Set CPU boot address */
	writel(__pa_symbol(secondary_startup),
	       cpucfg_membase + CPUCFG_PRIVATE0_REG);

	/* Assert the CPU core in reset */
	writel(0, cpucfg_membase + CPUCFG_CPU_RST_CTRL_REG(cpu));

	/* Assert the L1 cache in reset */
	reg = readl(cpucfg_membase + CPUCFG_GEN_CTRL_REG);
	writel(reg & ~BIT(cpu), cpucfg_membase + CPUCFG_GEN_CTRL_REG);

	/* Clear CPU power-off gating */
	reg = readl(prcm_membase + PRCM_CPU_PWROFF_REG);
	writel(reg & ~BIT(cpu), prcm_membase + PRCM_CPU_PWROFF_REG);
	mdelay(1);

	/* Deassert the CPU core reset */
	writel(3, cpucfg_membase + CPUCFG_CPU_RST_CTRL_REG(cpu));

	spin_unlock(&cpu_lock);

	return 0;
}

static const struct smp_operations sun8i_smp_ops __initconst = {
	.smp_prepare_cpus	= sun8i_smp_prepare_cpus,
	.smp_boot_secondary	= sun8i_smp_boot_secondary,
};
CPU_METHOD_OF_DECLARE(sun8i_a23_smp, "allwinner,sun8i-a23", &sun8i_smp_ops);

static int sun8i_t113_smp_boot_secondary(unsigned int cpu,
				    struct task_struct *idle)
{
    	u32 reg;
    	//void __iomem *cpucfg_membase = ioremap(0x09010000, 0x10);
    	void __iomem *cpuexec_membase[] = {ioremap(0x070005C4, 0x10),ioremap(0x070005C8, 0x10)};

	if (cpu != 1)
	    return 0;
	

	spin_lock(&cpu_lock);

	/* Set CPU boot address */
	writel(__pa_symbol(secondary_startup),
	       cpuexec_membase[cpu]);



	/* Set CPU boot address */
	//writel(__pa_symbol(secondary_startup),	cpuexec_membase[cpu]);

	//          3322 2222 2222 1111 1111 1100 0000 0000
	//          1098 7654 3210 9876 5432 1098 7654 3210
	/* default  0001 0011 1111 1111 0000 0001 0000 0001 */
	//          |||| |||| |||| |||| |||| |||| |||| ||||
	//	    |||| |||| |||| |||| |||| |||| |||| |||| |||+-- 0: cpu0 reset, 1: cpu0 release reset
	//	    |||| |||| |||| |||| |||| |||| |||| |||| ||+--- 0: cpu1 reset, 1: cpu1 release reset
	//	    |||| |||| |||| |||| |||| |||| |||| |||| |+---- not used
	//	    |||| |||| |||| |||| |||| |||| |||| |||| +----- not used
	//	    |||| |||| |||| |||| |||| |||| |||| ++++------- not used
	//          |||| |||| |||| |||| |||| |||+----------------- 0: Assert, 1: De-assert Cluster L2 Cache Reset
	//          |||| |||| |||| |||| ++++ +++------------------ not used
	//          |||| |||| |||| ++++--------------------------- DBG_RST 0: Assert, 1: De-assert Cluster Debug Reset
	//	    |||| |||| ++++-------------------------------- ETM_RST Cluster ETM Reset Assert
	//	    |||| |||+------------------------------------- SOC_DBG_RST Cluster SoC Debug Reset
	//	    |||| ||+-------------------------------------- MBIST_RST, CPUBIST Reset, is for test
	//	    |||| ++--------------------------------------- not used
	//	    ++++------------------------------------------ not used
	/* Assert reset on target CPU */
	reg = readl(SUN8I_R528_C0_CPUX_CFG + SUN8I_R528_C0_RST_CTRL);
	writel(reg & ~BIT(cpu), SUN8I_R528_C0_CPUX_CFG + SUN8I_R528_C0_RST_CTRL);

	/* Invalidate L1 cache */
	reg = readl(SUN8I_R528_C0_CPUX_CFG + SUN8I_R528_C0_CTRL_REG0);
	writel(reg & ~BIT(cpu), SUN8I_R528_C0_CPUX_CFG + SUN8I_R528_C0_CTRL_REG0);

	/* De-Assert reset on target CPU */
	//reg = readl(SUN8I_R528_C0_CPUX_CFG + SUN8I_R528_C0_RST_CTRL);
	//writel(reg | BIT(cpu), SUN8I_R528_C0_CPUX_CFG + SUN8I_R528_C0_RST_CTRL);

	//writel(reg | BIT(cpu), cpucfg_membase);
	printk("!!!!+++++++++++++++++++++ cpuexec_membase[cpu] = %x cpu = %u\n", (unsigned int)cpuexec_membase[cpu], cpu);

	spin_unlock(&cpu_lock);

	return 0;
}

static const struct smp_operations sun8i_t113_smp_ops __initconst = {
	.smp_boot_secondary	= sun8i_t113_smp_boot_secondary,
};
CPU_METHOD_OF_DECLARE(sun8i_t113_smp, "allwinner,sun8iw20p1", &sun8i_t113_smp_ops);
