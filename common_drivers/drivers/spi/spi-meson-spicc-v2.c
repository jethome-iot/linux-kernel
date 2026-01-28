// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/clk-provider.h>
#include <linux/dma-mapping.h>
#include <linux/amlogic/power_domain.h>
#include <linux/pm_runtime.h>
#include <linux/pm_domain.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/reset.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <asm/cacheflush.h>
#include <linux/amlogic/aml_spi.h>
#ifdef CONFIG_SPICC_TEST
#include "spicc_test.h"
#endif
#define MESON_SPICC_HW_IF

/* Register Map */
#define SPICC_REG_CFG_READY		0x00
#define SPICC_REG_CFG_SPI		0x04
#define HW_POS				BIT(6)
#define HW_NEG				BIT(7)
#define SPICC_REG_CFG_START		0x08
#define SPICC_REG_CFG_BUS		0x0C
#define SPICC_REG_PIO_TX_DATA_L		0x10
#define SPICC_REG_PIO_TX_DATA_H		0x14
#define SPICC_REG_PIO_RX_DATA_L		0x18
#define SPICC_REG_PIO_RX_DATA_H		0x1C
#define SPICC_REG_MEM_TX_ADDR_L		0x10
#define SPICC_REG_MEM_TX_ADDR_H		0x14
#define SPICC_REG_MEM_RX_ADDR_L		0x18
#define SPICC_REG_MEM_RX_ADDR_H		0x1C
#define SPICC_REG_DESC_LIST_L		0x20
#define SPICC_REG_DESC_LIST_H		0x24
#define SPICC_DESC_PENDING		BIT(31)
#define SPICC_REG_DESC_CURRENT_L	0x28
#define SPICC_REG_DESC_CURRENT_H	0x2c
#define SPICC_REG_IRQ_STS		0x30
#define SPICC_REG_IRQ_ENABLE		0x34
#define SPICC_RCH_DESC_EOC		BIT(0)
#define SPICC_RCH_DESC_INVALID		BIT(1)
#define SPICC_RCH_DESC_RESP		BIT(2)
#define SPICC_RCH_DATA_RESP		BIT(3)
#define SPICC_WCH_DESC_EOC		BIT(4)
#define SPICC_WCH_DESC_INVALID		BIT(5)
#define SPICC_WCH_DESC_RESP		BIT(6)
#define SPICC_WCH_DATA_RESP		BIT(7)
#define SPICC_DESC_ERR			BIT(8)
#define SPICC_SPI_READY			BIT(9)
#define SPICC_DESC_DONE			BIT(10)
#define SPICC_DESC_CHAIN_DONE		BIT(11)

union spicc_cfg_spi {
	u32			d32;
	struct  {
		u32		bus64_en:1;
		u32		slave_en:1;
		u32		ss:2;
		u32		flash_wp_pin_en:1;
		u32		flash_hold_pin_en:1;
		u32		hw_pos:1; /* start on vsync rising */
		u32		hw_neg:1; /* start on vsync falling */
		u32		rsv:24;
	} b;
};

union spicc_cfg_start {
	u32			d32;
	struct  {
		u32		block_num:20;
#define SPICC_BLOCK_MAX			0x100000

		u32		block_size:3;
		u32		dc_level:1;
#define SPICC_DC_DATA0_CMD1		0
#define SPICC_DC_DATA1_CMD0		1

		u32		op_mode:2;
#define SPICC_OP_MODE_WRITE_CMD		0
#define SPICC_OP_MODE_READ_STS		1
#define SPICC_OP_MODE_WRITE		2
#define SPICC_OP_MODE_READ		3

		u32		rx_data_mode:2;
		u32		tx_data_mode:2;
#define SPICC_DATA_MODE_NONE		0
#define SPICC_DATA_MODE_PIO		1
#define SPICC_DATA_MODE_MEM		2
#define SPICC_DATA_MODE_SG		3

		u32		eoc:1;
		u32		pending:1;
	} b;
};

union spicc_cfg_bus {
	u32			d32;
	struct  {
		u32		clk_div:8;
#define SPICC_CLK_DIV_SHIFT	0
#define SPICC_CLK_DIV_WIDTH	8
#define SPICC_CLK_DIV_MASK	GENMASK(7, 0)
#define SPICC_CLK_DIV_MAX	256
#define SPICC_CLK_DIV_MIN	4 /* recommended by specification */

		/* signed, -8~-1 early, 1~7 later */
		u32		rx_tuning:4;
		u32		tx_tuning:4;

		u32		ss_leading_gap:4;
		u32		lane:2;
#define SPICC_SINGLE_SPI		0
#define SPICC_DUAL_SPI			1
#define SPICC_QUAD_SPI			2

		u32		half_duplex_en:1;
		u32		little_endian_en:1;
		u32		dc_mode:1;
#define SPICC_DC_MODE_PIN		0
#define SPICC_DC_MODE_9BIT		1

		u32		null_ctl:1;
		u32		dummy_ctl:1;
		u32		read_turn_around:2;
#define SPICC_READ_TURN_AROUND_MAX	3

		u32		keep_ss:1;
		u32		cpha:1;
		u32		cpol:1;
	} b;
};

struct spicc_sg_link {
	u32			valid:1;
	u32			eoc:1;
	u32			irq:1;
	u32			act:3;
	u32			ring:1;
	u32			rsv:1;
	u32			len:24;
#define SPICC_SG_LEN_MAX		0x1000000
#define SPICC_SG_TX_LEN_MAX		SPICC_SG_LEN_MAX
#define SPICC_SG_RX_LEN_MAX		SPICC_SG_LEN_MAX

#ifdef CONFIG_ARM64_SPICC
	u64			addr;
	u32			addr_rsv;
#else
	u32			addr;
#endif
};

struct spicc_descriptor {
	union spicc_cfg_start		cfg_start;
	union spicc_cfg_bus		cfg_bus;
	u64				tx_paddr;
	u64				rx_paddr;
};

struct spicc_descriptor_extra {
	struct spicc_sg_link		*tx_ccsg;
	struct spicc_sg_link		*rx_ccsg;
	int				tx_ccsg_len;
	int				rx_ccsg_len;
};

struct spicc_device {
	struct spi_controller		*controller;
	struct platform_device		*pdev;
	void __iomem			*base;
	struct clk			*sys_clk;
	struct clk			*spi_clk;
	struct clk			*sclk;
	struct completion		completion;
	u32				status;
	u32			speed_hz;
	u32				actual_speed_hz;
	u32			bytes_per_word;
	union spicc_cfg_spi		cfg_spi;
	union spicc_cfg_start		cfg_start;
	union spicc_cfg_bus		cfg_bus;
	u8				config_ss_trailing_gap;
#ifdef MESON_SPICC_HW_IF
	void (*dirspi_complete)(void *context);
	void *dirspi_context;
	void __iomem			*trig_reg;
	struct spicc_descriptor		*dirspi_desc;
	dma_addr_t			dirspi_desc_paddr;
	int				dirspi_desc_len;
	int				dirspi_status;
#define DIRSPI_STA_IDLE		0
#define DIRSPI_STA_READY	1
#define DIRSPI_STA_RUNNING	2
#endif
};

#define spicc_info(fmt, args...) \
	pr_info("[info]%s: " fmt, __func__, ## args)

#define spicc_err(fmt, args...) \
	pr_info("[error]%s: " fmt, __func__, ## args)

#define spicc_warn(fmt, args...) \
	pr_info("[warn]%s: " fmt, __func__, ## args)

//#define SPICC_DEBUG_EN
#ifdef SPICC_DEBUG_EN
#define spicc_dbg(fmt, args...) \
	pr_info("[debug]%s: " fmt, __func__, ## args)
#else
#define spicc_dbg(fmt, args...)
#endif

#define spicc_writel(_spicc, _val, _offset) \
	 writel(_val, (_spicc)->base + (_offset))
#define spicc_readl(_spicc, _offset) \
	readl((_spicc)->base + (_offset))

#ifdef MESON_SPICC_HW_IF
static void dirspi_start(struct spi_device *spi);
static void dirspi_stop(struct spi_device *spi);
static int dirspi_async(struct spi_device *spi,
			dma_addr_t tx_dma,
			dma_addr_t rx_dma,
			int len,
			void (*complete)(void *context),
			void *context);
static int dirspi_sync(struct spi_device *spi,
			dma_addr_t tx_dma,
			dma_addr_t rx_dma,
			int len);
static int dirspi_dma_trig(struct spi_device *spi,
			   dma_addr_t tx_dma,
			   dma_addr_t rx_dma,
			   int len,
			   u8 src);
static int dirspi_dma_trig_start(struct spi_device *spi);
static int dirspi_dma_trig_stop(struct spi_device *spi);
static int dirspi_dma_trig_release(struct spi_device *spi);
#endif

static inline int spicc_sem_down_read(struct spicc_device *spicc)
{
	int ret;

	ret = spicc_readl(spicc, SPICC_REG_CFG_READY);
	if (ret)
		spicc_writel(spicc, 0, SPICC_REG_CFG_READY);

	return ret;
}

static inline void spicc_sem_up_write(struct spicc_device *spicc)
{
	spicc_writel(spicc, 1, SPICC_REG_CFG_READY);
}

static int spicc_set_speed(struct spicc_device *spicc, uint speed_hz)
{
	u32 div;

	if (!speed_hz || speed_hz == spicc->speed_hz)
		return 0;

	spicc->speed_hz = speed_hz;
	clk_set_rate(spicc->sclk, speed_hz);
	/* Store the div for the descriptor mode */
	div = FIELD_GET(SPICC_CLK_DIV_MASK,
			spicc_readl(spicc, SPICC_REG_CFG_BUS));
	spicc->cfg_bus.b.clk_div = div;
	spicc->actual_speed_hz = clk_get_rate(spicc->sclk);
	spicc_dbg("desired speed %luHz, actual speed %luHz, div=%d\n",
		  speed_hz, spicc->actual_speed_hz, div);

	return 0;
}

static inline unsigned long spicc_xfer_time_max(struct spicc_device *spicc,
						int len)
{
	unsigned long ms;

	if (!spicc->actual_speed_hz)
		return 200;

	ms = (8 * len) / (spicc->actual_speed_hz / 1000);
	ms += ms + 20; /* some tolerance */

	return ms;
}

static int meson_spicc_config(struct spicc_device *spicc,
			      struct spi_device *spi)
{
	struct  spicc_controller_data *cdata;

	if (!spi->bits_per_word || spi->bits_per_word % 8) {
		spicc_err("invalid wordlen %d\n", spi->bits_per_word);
		return -EINVAL;
	}

	spicc->bytes_per_word = spi->bits_per_word >> 3;
	spicc->cfg_start.b.block_size = spicc->bytes_per_word & 0x7;
	spicc->cfg_spi.b.ss = spi->chip_select;

	spicc->cfg_bus.b.cpol = !!(spi->mode & SPI_CPOL);
	spicc->cfg_bus.b.cpha = !!(spi->mode & SPI_CPHA);
	spicc->cfg_bus.b.little_endian_en = !!(spi->mode & SPI_LSB_FIRST);
	spicc->cfg_bus.b.half_duplex_en = !!(spi->mode & SPI_3WIRE);

	cdata = (struct spicc_controller_data *)spi->controller_data;
	if (cdata && cdata->timing_en) {
		/* SCLK * N */
		spicc->cfg_bus.b.ss_leading_gap = cdata->ss_leading_gap;
		/* 2.75us + SCLK * 9 * N */
		spicc->config_ss_trailing_gap = cdata->ss_trailing_gap;
		/* 4bit signed, SCLK * N */
		spicc->cfg_bus.b.tx_tuning = cdata->tx_tuning;
		spicc->cfg_bus.b.rx_tuning = cdata->rx_tuning;
		spicc->cfg_bus.b.dummy_ctl = cdata->dummy_ctl;
	} else if (spi_controller_is_slave(spicc->controller)) {
		spicc->cfg_bus.b.ss_leading_gap = 0;
		spicc->config_ss_trailing_gap = 0;
		spicc->cfg_bus.b.tx_tuning = 15; /* -1 SCLK */
		spicc->cfg_bus.b.rx_tuning = 0;
		spicc->cfg_bus.b.dummy_ctl = 0;
	} else {
		spicc->cfg_bus.b.ss_leading_gap = 5;
		spicc->config_ss_trailing_gap = 1;
		spicc->cfg_bus.b.tx_tuning = 0;
		spicc->cfg_bus.b.rx_tuning = 0;
		spicc->cfg_bus.b.dummy_ctl = 0;
	}

	return 0;
}

static bool meson_spicc_can_dma(struct spi_controller *ctlr,
				struct spi_device *spi,
				struct spi_transfer *xfer)
{
	return true;
}

static void spicc_sg_xlate(struct sg_table *sgt, struct spicc_sg_link *ccsg)
{
	struct scatterlist *sg;
	int i;

	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		ccsg->valid = 1;
		/* EOC specially for the last sg */
		ccsg->eoc = sg_is_last(sg);
		ccsg->ring = 0;
		ccsg->len = sg_dma_len(sg);
#ifdef CONFIG_ARM64_SPICC
		ccsg->addr = sg_dma_address(sg);
#else
		ccsg->addr = (u32)sg_dma_address(sg);
#endif
		ccsg++;
	}
}

static int nbits_to_lane[] = {
	SPICC_SINGLE_SPI,
	SPICC_SINGLE_SPI,
	SPICC_DUAL_SPI,
	-EINVAL,
	SPICC_QUAD_SPI
};

static int spicc_config_desc_one_transfer(struct spicc_device *spicc,
			struct spi_transfer *xfer,
			struct spicc_descriptor *desc,
			struct spicc_descriptor_extra *exdesc,
			struct spicc_controller_data *cdata)
{
	int block_size, blocks;
	struct device *dev = &spicc->pdev->dev;
	struct spicc_sg_link *ccsg;
	int ccsg_len;
	dma_addr_t paddr;
	int ret;

	memset(desc, 0, sizeof(*desc));
	if (exdesc)
		memset(exdesc, 0, sizeof(*exdesc));
	spicc_set_speed(spicc, xfer->speed_hz);
	desc->cfg_start.d32 = spicc->cfg_start.d32;
	desc->cfg_bus.d32 = spicc->cfg_bus.d32;

	block_size = xfer->bits_per_word >> 3;
	blocks = xfer->len / block_size;

	desc->cfg_start.b.tx_data_mode = SPICC_DATA_MODE_NONE;
	desc->cfg_start.b.rx_data_mode = SPICC_DATA_MODE_NONE;
	desc->cfg_start.b.eoc = 0;
	desc->cfg_bus.b.keep_ss = !xfer->cs_change;
	desc->cfg_bus.b.null_ctl = 0;
	if (cdata && cdata->dc_mode) {
		desc->cfg_start.b.dc_level = cdata->dc_level;
		desc->cfg_bus.b.read_turn_around = min_t(unsigned int,
					cdata->read_turn_around,
					SPICC_READ_TURN_AROUND_MAX);
		desc->cfg_bus.b.dc_mode = cdata->dc_mode - 1;
	}

	if (xfer->tx_buf || xfer->tx_dma) {
		desc->cfg_bus.b.lane = nbits_to_lane[xfer->tx_nbits];
		desc->cfg_start.b.op_mode = (cdata && cdata->dc_mode) ?
			SPICC_OP_MODE_WRITE_CMD : SPICC_OP_MODE_WRITE;
	}
	if (xfer->rx_buf || xfer->rx_dma) {
		desc->cfg_bus.b.lane = nbits_to_lane[xfer->rx_nbits];
		desc->cfg_start.b.op_mode = (cdata && cdata->dc_mode) ?
			SPICC_OP_MODE_READ_STS : SPICC_OP_MODE_READ;
	}

	if (desc->cfg_start.b.op_mode == SPICC_OP_MODE_READ_STS) {
		desc->cfg_start.b.block_size = blocks;
		desc->cfg_start.b.block_num = 1;
	} else {
		desc->cfg_start.b.block_size = block_size & 0x7;
		blocks = min_t(int, blocks, SPICC_BLOCK_MAX);
		desc->cfg_start.b.block_num = blocks;
	}

	if (xfer->tx_sg.nents && xfer->tx_sg.sgl) {
		ccsg_len = xfer->tx_sg.nents * sizeof(struct spicc_sg_link);
		ccsg = kzalloc(ccsg_len, GFP_KERNEL | GFP_DMA);
		if (!ccsg) {
			dev_err(dev, "alloc tx_ccsg failed\n");
			return -ENOMEM;
		}

		spicc_sg_xlate(&xfer->tx_sg, ccsg);
		paddr = dma_map_single(dev, (void *)ccsg,
				       ccsg_len, DMA_TO_DEVICE);
		ret = dma_mapping_error(dev, paddr);
		if (ret) {
			kfree(ccsg);
			dev_err(dev, "tx ccsg map failed\n");
			return ret;
		}

		desc->tx_paddr = paddr;
		desc->cfg_start.b.tx_data_mode = SPICC_DATA_MODE_SG;
		exdesc->tx_ccsg = ccsg;
		exdesc->tx_ccsg_len = ccsg_len;
	} else if (xfer->tx_buf || xfer->tx_dma) {
		paddr = xfer->tx_dma;
		if (!paddr) {
			paddr = dma_map_single(dev, (void *)xfer->tx_buf,
					       xfer->len, DMA_TO_DEVICE);
			ret = dma_mapping_error(dev, paddr);
			if (ret) {
				dev_err(dev, "tx buf map failed\n");
				return ret;
			}
		}
		desc->tx_paddr = paddr;
		desc->cfg_start.b.tx_data_mode = SPICC_DATA_MODE_MEM;
	}

	if (xfer->rx_sg.nents && xfer->rx_sg.sgl) {
		ccsg_len = xfer->rx_sg.nents * sizeof(struct spicc_sg_link);
		ccsg = kzalloc(ccsg_len, GFP_KERNEL | GFP_DMA);
		if (!ccsg) {
			dev_err(dev, "alloc rx_ccsg failed\n");
			return -ENOMEM;
		}

		spicc_sg_xlate(&xfer->rx_sg, ccsg);
		paddr = dma_map_single(dev, (void *)ccsg,
				       ccsg_len, DMA_TO_DEVICE);
		ret = dma_mapping_error(dev, paddr);
		if (ret) {
			kfree(ccsg);
			dev_err(dev, "rx ccsg map failed\n");
			return ret;
		}

		desc->rx_paddr = paddr;
		desc->cfg_start.b.rx_data_mode = SPICC_DATA_MODE_SG;
		exdesc->rx_ccsg = ccsg;
		exdesc->rx_ccsg_len = ccsg_len;
	} else if (xfer->rx_buf || xfer->rx_dma) {
		paddr = xfer->rx_dma;
		if (!paddr) {
			paddr = dma_map_single(dev, xfer->rx_buf,
					       xfer->len, DMA_FROM_DEVICE);
			ret = dma_mapping_error(dev, paddr);
			if (ret) {
				dev_err(dev, "rx buf map failed\n");
				return ret;
			}
		}
		desc->rx_paddr = paddr;
		desc->cfg_start.b.rx_data_mode = SPICC_DATA_MODE_MEM;
	}

	return 0;
}

static void spicc_deconfig_desc_one_transfer(struct spicc_device *spicc,
			struct spi_transfer *xfer,
			struct spicc_descriptor *desc,
			struct spicc_descriptor_extra *exdesc)
{
	struct device *dev = &spicc->pdev->dev;

	if (desc->tx_paddr) {
		if (desc->cfg_start.b.tx_data_mode == SPICC_DATA_MODE_SG) {
			dma_unmap_single(dev, (dma_addr_t)desc->tx_paddr,
					 exdesc->tx_ccsg_len, DMA_TO_DEVICE);
			kfree(exdesc->tx_ccsg);
		} else if (!xfer->tx_dma) {
			dma_unmap_single(dev, (dma_addr_t)desc->tx_paddr,
					 xfer->len, DMA_TO_DEVICE);
		}
	}

	if (desc->rx_paddr) {
		if (desc->cfg_start.b.rx_data_mode == SPICC_DATA_MODE_SG) {
			dma_unmap_single(dev, (dma_addr_t)desc->rx_paddr,
					 exdesc->rx_ccsg_len, DMA_TO_DEVICE);
			kfree(exdesc->rx_ccsg);
		} else if (!xfer->rx_dma) {
			dma_unmap_single(dev, (dma_addr_t)desc->rx_paddr,
					 xfer->len, DMA_FROM_DEVICE);
		}
	}
}

static void spicc_configure_last_desc(struct spicc_device *spicc,
				struct spicc_descriptor *desc)
{
	/* configure the additional descriptor null */
	if (spicc->config_ss_trailing_gap) {
		desc++;
		desc->cfg_start.d32 = spicc->cfg_start.d32;
		desc->cfg_bus.d32 = spicc->cfg_bus.d32;
		desc->cfg_start.b.op_mode = SPICC_OP_MODE_WRITE;
		desc->cfg_start.b.block_size = 1;
		desc->cfg_start.b.block_num = spicc->config_ss_trailing_gap;
		desc->cfg_bus.b.null_ctl = 1;
	}

	desc->cfg_bus.b.keep_ss = 0;
	desc->cfg_start.b.eoc = 1;
}

static void spicc_desc_pending(struct spicc_device *spicc,
			       dma_addr_t desc_paddr,
			       bool trig,
			       bool irq_en)
{
	u32 desc_l, desc_h, cfg_spi;

#ifdef	CONFIG_ARCH_DMA_ADDR_T_64BIT
	desc_l = (u64)desc_paddr & 0xffffffff;
	desc_h = (u64)desc_paddr >> 32;
#else
	desc_l = desc_paddr & 0xffffffff;
	desc_h = 0;
#endif

	cfg_spi = spicc->cfg_spi.d32;
	if (trig)
		cfg_spi |= HW_POS;
	else
		desc_h |= SPICC_DESC_PENDING;

	spicc_writel(spicc, irq_en ? SPICC_DESC_CHAIN_DONE : 0,
		     SPICC_REG_IRQ_ENABLE);
	spicc_writel(spicc, cfg_spi, SPICC_REG_CFG_SPI);
	spicc_writel(spicc, desc_l, SPICC_REG_DESC_LIST_L);
	spicc_writel(spicc, desc_h, SPICC_REG_DESC_LIST_H);
}

static irqreturn_t meson_spicc_irq(int irq, void *data)
{
	struct spicc_device *spicc = (void *)data;
	u32 sts;

	spicc->status = 0;
	sts = spicc_readl(spicc, SPICC_REG_IRQ_STS);
	spicc_writel(spicc, sts, SPICC_REG_IRQ_STS);
	if (sts & (SPICC_RCH_DESC_INVALID |
		   SPICC_RCH_DESC_RESP |
		   SPICC_RCH_DATA_RESP |
		   SPICC_WCH_DESC_INVALID |
		   SPICC_WCH_DESC_RESP |
		   SPICC_WCH_DATA_RESP |
		   SPICC_DESC_ERR)) {
		spicc->status = sts;
	}

#ifdef MESON_SPICC_HW_IF
	if (spicc->dirspi_complete) {
		spicc_sem_up_write(spicc);
		spicc->dirspi_complete(spicc->dirspi_context);
		spicc->dirspi_complete = NULL;
		return IRQ_HANDLED;
	}
#endif

	complete(&spicc->completion);

	return IRQ_HANDLED;
}

/*
 * spi_transfer_one_message - Default implementation of transfer_one_message()
 *
 * This is a standard implementation of transfer_one_message() for
 * drivers which implement a transfer_one() operation.  It provides
 * standard handling of delays and chip select management.
 */
static int meson_spicc_transfer_one_message(struct spi_controller *ctlr,
					    struct spi_message *msg)
{
	struct spicc_device *spicc = spi_controller_get_devdata(ctlr);
	struct device *dev = &spicc->pdev->dev;
	struct spicc_controller_data *cdata = msg->spi->controller_data;
	unsigned long ms = spicc_xfer_time_max(spicc, msg->frame_length);
	struct spi_transfer *xfer;
	struct spicc_descriptor *descs, *desc;
	struct spicc_descriptor_extra *exdescs, *exdesc;
	dma_addr_t descs_paddr;
	int desc_num = 0, descs_len;
	int ret = -EIO;

	if (!spicc_sem_down_read(spicc)) {
		spi_finalize_current_message(ctlr);
		dev_err(dev, "controller busy\n");
		return -EBUSY;
	}

	/*calculate the desc num for all xfer */
	list_for_each_entry(xfer, &msg->transfers, transfer_list)
		desc_num++;
	/* additional descriptor to achieve a ss trailing gap */
	if (spicc->config_ss_trailing_gap)
		desc_num++;

	/* alloc descriptor/extra-descriptor table */
	descs = kcalloc(desc_num, sizeof(*desc) + sizeof(*exdesc),
			GFP_KERNEL | GFP_DMA);
	if (!descs) {
		spi_finalize_current_message(ctlr);
		spicc_sem_up_write(spicc);
		return -ENOMEM;
	}
	descs_len = sizeof(*desc) * desc_num;
	exdescs = (struct spicc_descriptor_extra *)(descs + desc_num);

	/* config descriptor for each xfer */
	desc = descs;
	exdesc = exdescs;
	list_for_each_entry(xfer, &msg->transfers, transfer_list) {
		ret = spicc_config_desc_one_transfer(spicc, xfer,
				desc++, exdesc++, cdata);
		if (ret) {
			dev_err(dev, "config descriptor failed\n");
			goto end;
		}
	}
	spicc_configure_last_desc(spicc, --desc);

	descs_paddr = dma_map_single(dev, (void *)descs,
				     descs_len, DMA_TO_DEVICE);
	ret = dma_mapping_error(dev, descs_paddr);
	if (ret) {
		dev_err(dev, "desc table map failed\n");
		goto end;
	}

	reinit_completion(&spicc->completion);
	spicc_desc_pending(spicc, descs_paddr, false, true);
	if (wait_for_completion_timeout(&spicc->completion,
			spi_controller_is_slave(spicc->controller) ?
			MAX_SCHEDULE_TIMEOUT : msecs_to_jiffies(ms)))
		ret = spicc->status ? -EIO : 0;
	else
		ret = -ETIMEDOUT;

	dma_unmap_single(dev, descs_paddr, descs_len, DMA_TO_DEVICE);
end:
	desc = descs;
	exdesc = exdescs;
	list_for_each_entry(xfer, &msg->transfers, transfer_list)
		spicc_deconfig_desc_one_transfer(spicc, xfer, desc++, exdesc++);
	kfree(descs);

	if (!ret)
		msg->actual_length = msg->frame_length;
	msg->status = ret;
	spi_finalize_current_message(ctlr);
	spicc_sem_up_write(spicc);

	return ret;
}

#ifdef CONFIG_ARCH_MESON_ODROID_COMMON
static void hk_spi_set_cs(struct spi_device *spi, bool enable, bool force)
{
	bool activate = enable;

	/*
	 * Avoid calling into the driver (or doing delays) if the chip select
	 * isn't actually changing from the last time this was called.
	 */
	if (!force && (spi->controller->last_cs_enable == enable) &&
	    (spi->controller->last_cs_mode_high == (spi->mode & SPI_CS_HIGH)))
		return;

	spi->controller->last_cs_enable = enable;
	spi->controller->last_cs_mode_high = spi->mode & SPI_CS_HIGH;

	if ((spi->cs_gpiod || gpio_is_valid(spi->cs_gpio) ||
	    !spi->controller->set_cs_timing) && !activate) {
		spi_delay_exec(&spi->cs_hold, NULL);
	}

	if (spi->mode & SPI_CS_HIGH)
		enable = !enable;

	if (spi->cs_gpiod || gpio_is_valid(spi->cs_gpio)) {
		if (!(spi->mode & SPI_NO_CS)) {
			if (spi->cs_gpiod) {
				/* Polarity handled by GPIO library */
				gpiod_set_value_cansleep(spi->cs_gpiod, activate);
			} else {
				/*
				 * invert the enable line, as active low is
				 * default for SPI.
				 */
				gpio_set_value_cansleep(spi->cs_gpio, !enable);
			}
		}
		/* Some SPI masters need both GPIO CS & slave_select */
		if ((spi->controller->flags & SPI_MASTER_GPIO_SS) &&
		    spi->controller->set_cs)
			spi->controller->set_cs(spi, !enable);
	} else if (spi->controller->set_cs) {
		spi->controller->set_cs(spi, !enable);
	}

	if (spi->cs_gpiod || gpio_is_valid(spi->cs_gpio) ||
	    !spi->controller->set_cs_timing) {
		if (activate)
			spi_delay_exec(&spi->cs_setup, NULL);
		else
			spi_delay_exec(&spi->cs_inactive, NULL);
	}
}

static int hk_sw_transfer(struct spi_controller *ctlr, struct spi_message *msg, struct list_head *ptr, int desc_num)
{
	struct spicc_device *spicc = spi_controller_get_devdata(ctlr);
	struct device *dev = &spicc->pdev->dev;
	struct spicc_controller_data *cdata = msg->spi->controller_data;
	unsigned long ms = spicc_xfer_time_max(spicc, msg->frame_length);
	struct spi_transfer *xfer;
	struct spicc_descriptor *descs, *desc;
	struct spicc_descriptor_extra *exdescs, *exdesc;
	dma_addr_t descs_paddr;
	int descs_len;
	int ret = -EIO;
	int i;
	int original_desc_num = desc_num;

	if (spicc->config_ss_trailing_gap)
		desc_num++;

	/* alloc descriptor/extra-descriptor table */
	descs = kcalloc(desc_num, sizeof(*desc) + sizeof(*exdesc),
			GFP_KERNEL | GFP_DMA);
	if (!descs) {
		return -ENOMEM;
	}
	descs_len = sizeof(*desc) * desc_num;
	exdescs = (struct spicc_descriptor_extra *)(descs + desc_num);

	/* config descriptor for each xfer */
	desc = descs;
	exdesc = exdescs;

	xfer = list_first_entry(ptr, typeof(*xfer), transfer_list);
	for (i = 0; i < original_desc_num; ++i) {
		ret = spicc_config_desc_one_transfer(spicc, xfer,
		desc++, exdesc++, cdata);
		if (ret) {
			dev_err(dev, "config descriptor failed\n");
			goto end;
		}
		xfer = list_next_entry(xfer, transfer_list);
	}
	spicc_configure_last_desc(spicc, --desc);

	hk_spi_set_cs(msg->spi, true, false);

	descs_paddr = dma_map_single(dev, (void *)descs,
				     descs_len, DMA_TO_DEVICE);
	ret = dma_mapping_error(dev, descs_paddr);
	if (ret) {
		dev_err(dev, "desc table map failed\n");
		goto end;
	}

	reinit_completion(&spicc->completion);
	spicc_desc_pending(spicc, descs_paddr, false, true);
	if (wait_for_completion_timeout(&spicc->completion,
			spi_controller_is_slave(spicc->controller) ?
			MAX_SCHEDULE_TIMEOUT : msecs_to_jiffies(ms)))
		ret = spicc->status ? -EIO : 0;
	else
		ret = -ETIMEDOUT;

	dma_unmap_single(dev, descs_paddr, descs_len, DMA_TO_DEVICE);
	end:
	hk_spi_set_cs(msg->spi, false, false);
		desc = descs;
	exdesc = exdescs;

	xfer = list_entry(ptr, typeof(*xfer), transfer_list);
	for (i = 0; i < desc_num; ++i) {
		spicc_deconfig_desc_one_transfer(spicc, xfer, desc++, exdesc++);
		xfer = list_next_entry(xfer, transfer_list);
	}
	kfree(descs);

	return ret;
}

/*
 * spi_transfer_one_message - Default implementation of transfer_one_message()
 *
 * This is a standard implementation of transfer_one_message() for
 * drivers which implement a transfer_one() operation.  It provides
 * standard handling of delays and chip select management.
 */
static int hk_sw_meson_spicc_transfer_one_message(struct spi_controller *ctlr,
					    struct spi_message *msg)
{
	struct spicc_device *spicc = spi_controller_get_devdata(ctlr);
	struct device *dev = &spicc->pdev->dev;
	struct spi_transfer *xfer;
	int desc_num = 0;
	int ret = -EIO;
	struct list_head *ptr;

	if (!spicc_sem_down_read(spicc)) {
		spi_finalize_current_message(ctlr);
		dev_err(dev, "controller busy\n");
		return -EBUSY;
	}

	ptr = &msg->transfers;

	/*calculate the desc number for end of transfers or cs_change set 1*/
	list_for_each_entry(xfer, &msg->transfers, transfer_list)
	{
		++desc_num;

		// Changes the state of the cs pin if it is the last element in the list or cs_change is set.
		if (list_is_last(&xfer->transfer_list, &msg->transfers) || xfer->cs_change) {
			ret = hk_sw_transfer(ctlr, msg, ptr, desc_num);
			if (ret != 0) break;

			ptr = &(xfer)->transfer_list;
			desc_num = 0;
		}
	}

	spi_finalize_current_message(ctlr);
	spicc_sem_up_write(spicc);

	if (!ret)
		msg->actual_length = msg->frame_length;
	msg->status = ret;

	return ret;
}

#endif

static int meson_spicc_prepare_message(struct spi_controller *ctlr,
				       struct spi_message *message)
{
	struct spicc_device *spicc = spi_controller_get_devdata(ctlr);

	return meson_spicc_config(spicc, message->spi);
}

static int meson_spicc_unprepare_transfer(struct spi_controller *ctlr)
{
	return 0;
}

static int meson_spicc_setup(struct spi_device *spi)
{
#ifdef MESON_SPICC_HW_IF
	struct spicc_device *spicc;
	struct  spicc_controller_data *cdata;

	spicc = spi_controller_get_devdata(spi->controller);
	cdata = (struct spicc_controller_data *)spi->controller_data;
	if (cdata) {
		cdata->controller_version = CONTROLLER_SPISG;
		cdata->controller_capabilities = CAP_DMA_TRIG_VSYNC |
			CAP_DMA_TRIG_PWM_VS | CAP_TRIG_DELAY;
		cdata->dirspi_start = dirspi_start;
		cdata->dirspi_stop = dirspi_stop;
		cdata->dirspi_async = dirspi_async;
		cdata->dirspi_sync = dirspi_sync;
		cdata->dirspi_dma_trig = dirspi_dma_trig;
		cdata->dirspi_dma_trig_start = dirspi_dma_trig_start;
		cdata->dirspi_dma_trig_stop = dirspi_dma_trig_stop;
		cdata->dirspi_dma_trig_release = dirspi_dma_trig_release;
		if (cdata->use_dirspi)
			spicc_set_speed(spicc, spi->max_speed_hz);
	}
#endif

	return 0;
}

static void meson_spicc_cleanup(struct spi_device *spi)
{
	spi->controller_state = NULL;
}

static int meson_spicc_slave_abort(struct spi_controller *ctlr)
{
	struct spicc_device *spicc = spi_controller_get_devdata(ctlr);

	spicc->status = 0;
	spicc_writel(spicc, 0, SPICC_REG_DESC_LIST_H);
	complete(&spicc->completion);

	return 0;
}

#ifdef MESON_SPICC_HW_IF
static int spicc_wait_complete(struct spicc_device *spicc, u32 flags,
				unsigned long timeout)
{
	u32 sts = 0;

	timeout += jiffies;
	while (time_before(jiffies, timeout)) {
		sts = spicc_readl(spicc, SPICC_REG_IRQ_STS);
		if (sts & (SPICC_RCH_DESC_INVALID |
			   SPICC_RCH_DESC_RESP |
			   SPICC_RCH_DATA_RESP |
			   SPICC_WCH_DESC_INVALID |
			   SPICC_WCH_DESC_RESP |
			   SPICC_WCH_DATA_RESP |
			   SPICC_DESC_ERR)) {
			spicc_err("controller error sts=0x%x\n", sts);
			return -EIO;
		}

		if (sts & flags) {
			spicc_writel(spicc, sts, SPICC_REG_IRQ_STS);
			return 0;
		}

		cpu_relax();
	}

	spicc_err("timedout, sts=0x%x\n", sts);
	return -ETIMEDOUT;
}

static void dirspi_start(struct spi_device *spi)
{
	if (spi->controller->auto_runtime_pm) {
		if (pm_runtime_get_sync(spi->controller->dev.parent) < 0) {
			dev_err(spi->controller->dev.parent,
				"%s: pm_runtime_get_sync fails\n", __func__);
		}
	}
}

static void dirspi_stop(struct spi_device *spi)
{
}

/*
 * Return
 * <0: error code if the transfer is failed
 * 0: if the transfer is finished.(callback executed)
 * >0: if the transfer is still in progress
 */
static int dirspi_async(struct spi_device *spi,
			dma_addr_t tx_dma,
			dma_addr_t rx_dma,
			int len,
			void (*complete)(void *context),
			void *context)
{
	struct spicc_device *spicc;
	struct device *dev;
	struct spi_transfer xfer;
	int ret;

	spicc = spi_controller_get_devdata(spi->controller);
	dev = &spicc->pdev->dev;
	if (!spicc->dirspi_desc) {
		dev_err(dev, "no descriptor for dirspi\n");
		return -ENOMEM;
	}

	if (!spicc_sem_down_read(spicc)) {
		dev_err(dev, "controller busy\n");
		return -EBUSY;
	}

	ret = meson_spicc_config(spicc, spi);
	if (ret) {
		spicc_sem_up_write(spicc);
		return ret;
	}

	memset(&xfer, 0, sizeof(xfer));
	xfer.tx_dma = tx_dma;
	xfer.rx_dma = rx_dma;
	xfer.len = len;
	xfer.bits_per_word = spi->bits_per_word;
	ret = spicc_config_desc_one_transfer(spicc, &xfer,
			spicc->dirspi_desc, NULL, NULL);
	if (ret) {
		spicc_sem_up_write(spicc);
		return ret;
	}
	spicc_configure_last_desc(spicc, spicc->dirspi_desc);
	spicc_desc_pending(spicc, spicc->dirspi_desc_paddr, false,
			   complete ? true : false);

	spicc->dirspi_complete = complete;
	spicc->dirspi_context = context;
	if (complete)
		return 1;

	ret = spicc_wait_complete(spicc, SPICC_DESC_CHAIN_DONE,
			msecs_to_jiffies(spicc_xfer_time_max(spicc, len)));
	spicc_sem_up_write(spicc);

	return ret;
}

static int dirspi_sync(struct spi_device *spi,
			dma_addr_t tx_dma,
			dma_addr_t rx_dma,
			int len)
{
	return dirspi_async(spi, tx_dma, rx_dma, len, NULL, NULL);
}

/* SYSCTRL_SPI_TRIG */
#define TRIG_REG_IN_SEL_MASK		GENMASK(4, 0)
#define IN_SEL_VSYNC		13
#define IN_SEL_LINE_N		16
#define IN_SEL_PWM_VS		10
#define TRIG_REG_OUT_SEL_MASK		GENMASK(6, 5)
#define TRIG_REG_DELAY_CNT_MASK		GENMASK(30, 7)
#define TRIG_DELAY_MIN			2
#define TRIG_DELAY_MAX			0xFFFFF
#define TRIG_REG_MUX_EN			BIT(31)

static int spicc_dma_trig_start(struct spicc_device *spicc)
{
	struct device *dev = &spicc->pdev->dev;

	if (spicc->dirspi_status == DIRSPI_STA_RUNNING) {
		dev_warn(dev, "start trig in running state\n");
		return 0;
	}

	if (spicc->dirspi_status == DIRSPI_STA_IDLE) {
		dev_err(dev, "start trig in idle state\n");
		return -EIO;
	}

	if (!spicc_sem_down_read(spicc)) {
		dev_err(dev, "controller busy, start trig failed\n");
		return -EBUSY;
	}

	if (!IS_ERR_OR_NULL(spicc->trig_reg))
		writel(readl(spicc->trig_reg) | TRIG_REG_MUX_EN,
		       spicc->trig_reg);

	spicc->dirspi_status = DIRSPI_STA_RUNNING;
	dev_dbg(dev, "start trig in ready state success\n");

	return 0;
}

static int spicc_dma_trig_stop(struct spicc_device *spicc)
{
	struct device *dev = &spicc->pdev->dev;
	unsigned long timeout;
	union spicc_cfg_start cfg_start;
	u32 desc_h;

	if (spicc->dirspi_status == DIRSPI_STA_IDLE) {
		dev_warn(dev, "stop trig in idle state\n");
		return 0;
	}

	if (spicc->dirspi_status == DIRSPI_STA_READY) {
		dev_warn(dev, "stop trig in ready state\n");
		return 0;
	}

	if (!IS_ERR_OR_NULL(spicc->trig_reg))
		writel(readl(spicc->trig_reg) & ~TRIG_REG_MUX_EN,
		       spicc->trig_reg);

	timeout = msecs_to_jiffies(100) + jiffies;
	while (time_before(jiffies, timeout)) {
		cfg_start.d32 = spicc_readl(spicc, SPICC_REG_CFG_START);
		desc_h = spicc_readl(spicc, SPICC_REG_DESC_LIST_H);
		if (!cfg_start.b.pending && !(desc_h & SPICC_DESC_PENDING))
			break;
	}

	if (time_after(jiffies, timeout))
		dev_warn(dev, "dma busy\n");

	spicc_sem_up_write(spicc);
	spicc->dirspi_status = DIRSPI_STA_READY;
	dev_dbg(dev, "stop trig in running state success\n");

	return 0;
}

static int spicc_dma_trig_release(struct spicc_device *spicc)
{
	struct device *dev = &spicc->pdev->dev;

	if (spicc->dirspi_status == DIRSPI_STA_IDLE) {
		dev_warn(dev, "release trig in idle state\n");
		return 0;
	}

	if (spicc->dirspi_status == DIRSPI_STA_RUNNING)
		spicc_dma_trig_stop(spicc);

	spicc->dirspi_status = DIRSPI_STA_IDLE;
	dev_dbg(dev, "release trig success\n");

	return 0;
}

/*
 * @tx_dma: DMA address of tx buf
 * @rx_dma: DMA address of rx buf
 * @src: trigger source, DMA_TRIG_VSYNC or DMA_TRIG_LINE_N
 */
static int dirspi_dma_trig(struct spi_device *spi,
			   dma_addr_t tx_dma,
			   dma_addr_t rx_dma,
			   int len,
			   u8 src)
{
	struct spicc_device *spicc;
	struct device *dev;
	struct spicc_controller_data *cdata;
	struct spi_transfer xfer;
	u32 in_sel, delay = TRIG_DELAY_MIN;
	int ret;

	cdata = (struct spicc_controller_data *)spi->controller_data;
	spicc = spi_controller_get_devdata(spi->controller);
	dev = &spicc->pdev->dev;
	if (!spicc->dirspi_desc) {
		dev_err(dev, "no descriptor for dirspi\n");
		return -ENOMEM;
	}

	spicc_dma_trig_release(spicc);

	if (!spicc_sem_down_read(spicc)) {
		dev_err(dev, "controller busy\n");
		return -EBUSY;
	}

	ret = meson_spicc_config(spicc, spi);
	if (ret) {
		spicc_sem_up_write(spicc);
		return ret;
	}

	memset(&xfer, 0, sizeof(xfer));
	xfer.tx_dma = tx_dma;
	xfer.rx_dma = rx_dma;
	xfer.len = len;
	xfer.bits_per_word = spi->bits_per_word;
	ret = spicc_config_desc_one_transfer(spicc, &xfer,
			spicc->dirspi_desc, NULL, NULL);
	if (ret) {
		spicc_sem_up_write(spicc);
		return ret;
	}
	spicc_configure_last_desc(spicc, spicc->dirspi_desc);
	spicc_desc_pending(spicc, spicc->dirspi_desc_paddr, true, false);

	if (!IS_ERR_OR_NULL(spicc->trig_reg)) {
		if (src == DMA_TRIG_VSYNC)
			in_sel = IN_SEL_VSYNC;
		else if (src == DMA_TRIG_LINE_N)
			in_sel = IN_SEL_LINE_N;
		else
			in_sel = IN_SEL_PWM_VS;

		if (cdata) {
			if (cdata->dma_trig_delay > TRIG_DELAY_MAX)
				delay = TRIG_DELAY_MAX;
			else if (cdata->dma_trig_delay > TRIG_DELAY_MIN)
				delay = cdata->dma_trig_delay;
		}

		writel(FIELD_PREP(TRIG_REG_IN_SEL_MASK, in_sel)
		       | FIELD_PREP(TRIG_REG_DELAY_CNT_MASK, delay),
		       spicc->trig_reg);
	}

	spicc_sem_up_write(spicc);
	spicc->dirspi_status = DIRSPI_STA_READY;
	dev_dbg(dev, "init trig success\n");

	return 0;
}

static int dirspi_dma_trig_start(struct spi_device *spi)
{
	struct spicc_device *spicc;

	spicc = spi_controller_get_devdata(spi->controller);
	return spicc_dma_trig_start(spicc);
}

static int dirspi_dma_trig_stop(struct spi_device *spi)
{
	struct spicc_device *spicc;

	spicc = spi_controller_get_devdata(spi->controller);
	return spicc_dma_trig_stop(spicc);
}

static int dirspi_dma_trig_release(struct spi_device *spi)
{
	struct spicc_device *spicc;

	spicc = spi_controller_get_devdata(spi->controller);
	return spicc_dma_trig_release(spicc);
}
#endif	/* end of MESON_SPICC_HW_IF */

#ifdef CONFIG_SPICC_TEST
static DEVICE_ATTR_WO(test);
static DEVICE_ATTR_RW(testdev);
#endif

#define DIV_NUM (SPICC_CLK_DIV_MAX - SPICC_CLK_DIV_MIN + 1)
static struct clk_div_table linear_div_table[DIV_NUM + 1] = {
	[0] = {0, 0 /* init flag */},
	[DIV_NUM] = {0, 0 /* sentinel */}
};

static struct clk *meson_spicc_divider_clk_get(struct spicc_device *spicc)
{
	struct device *dev = &spicc->pdev->dev;
	struct clk_init_data init;
	struct clk_divider *div;
	const char *parent_names[1];
	char name[32];
	u32 val;
	int i;

	if (!linear_div_table[0].div)
		for (i = 0; i < DIV_NUM; i++) {
			linear_div_table[i].val = i + SPICC_CLK_DIV_MIN - 1;
			linear_div_table[i].div = i + SPICC_CLK_DIV_MIN;
		}

	div = devm_kzalloc(dev, sizeof(*div), GFP_KERNEL);
	if (!div)
		return ERR_PTR(-ENOMEM);

	div->flags = CLK_DIVIDER_ROUND_CLOSEST;
	div->reg = spicc->base + SPICC_REG_CFG_BUS;
	div->shift = SPICC_CLK_DIV_SHIFT;
	div->width = SPICC_CLK_DIV_WIDTH;
	div->table = linear_div_table;

	/* Register value should not be outside of the table */
	val = spicc_readl(spicc, SPICC_REG_CFG_BUS);
	val &= ~SPICC_CLK_DIV_MASK;
	val |= FIELD_PREP(SPICC_CLK_DIV_MASK, SPICC_CLK_DIV_MIN - 1);
	spicc_writel(spicc, val, SPICC_REG_CFG_BUS);

	/* Register clk-divider */
	parent_names[0] = __clk_get_name(spicc->spi_clk);
	snprintf(name, sizeof(name), "%s_div", dev_name(dev));
	init.name = name;
	init.ops = &clk_divider_ops;
	init.flags = CLK_SET_RATE_PARENT;
	if (of_property_read_bool(dev->of_node, "assigned-clock-rates"))
		init.flags = 0;
	init.parent_names = parent_names;
	init.num_parents = 1;
	div->hw.init = &init;

	return devm_clk_register(dev, &div->hw);
}

static int meson_spicc_probe(struct platform_device *pdev)
{
	struct spi_controller *ctlr;
	struct spicc_device *spicc;
	int ret, irq;
#ifdef CONFIG_ARCH_MESON_ODROID_COMMON
	bool use_hw_cs;

	use_hw_cs = of_property_read_bool(pdev->dev.of_node, "use-hw-cs");
#endif

	ctlr = __spi_alloc_controller(&pdev->dev, sizeof(*spicc),
			of_property_read_bool(pdev->dev.of_node, "slave"));
	if (!ctlr) {
		dev_err(&pdev->dev, "controller allocation failed\n");
		return -ENOMEM;
	}
	spicc = spi_controller_get_devdata(ctlr);
	spicc->controller = ctlr;

	spicc->pdev = pdev;
	platform_set_drvdata(pdev, spicc);

	spicc->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR_OR_NULL(spicc->base)) {
		dev_err(&pdev->dev, "io resource mapping failed\n");
		ret = PTR_ERR(spicc->base);
		goto out_controller;
	}

	spicc->trig_reg = devm_platform_ioremap_resource(pdev, 1);
	if (IS_ERR_OR_NULL(spicc->trig_reg))
		dev_warn(&pdev->dev, "no trig resource\n");

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		ret = irq;
		goto out_controller;
	}

	ret = devm_request_irq(&pdev->dev, irq, meson_spicc_irq,
			       IRQF_TRIGGER_RISING, NULL, spicc);
	if (ret) {
		dev_err(&pdev->dev, "irq request failed\n");
		goto out_controller;
	}

	spicc->sys_clk = devm_clk_get(&pdev->dev, "sys");
	if (IS_ERR_OR_NULL(spicc->sys_clk))
		dev_warn(&pdev->dev, "no sys clock\n");
	else
		clk_prepare_enable(spicc->sys_clk);

	spicc->spi_clk = devm_clk_get(&pdev->dev, "spi");
	if (IS_ERR_OR_NULL(spicc->spi_clk)) {
		dev_err(&pdev->dev, "spi clock request failed\n");
		ret = PTR_ERR(spicc->spi_clk);
		goto out_controller;
	}
	ret = clk_prepare_enable(spicc->spi_clk);
	if (ret) {
		dev_err(&pdev->dev, "spi clock enable failed\n");
		goto out_controller;
	}

	spicc->sclk = meson_spicc_divider_clk_get(spicc);
	if (IS_ERR_OR_NULL(spicc->sclk)) {
		dev_err(&pdev->dev, "register divider clk failed\n");
		ret = PTR_ERR(spicc->sclk);
		goto out_controller;
	}

	spicc->cfg_spi.d32 = 0;
	spicc->cfg_start.d32 = 0;
	spicc->cfg_bus.d32 = 0;

	spicc->cfg_spi.b.flash_wp_pin_en = 1;
	spicc->cfg_spi.b.flash_hold_pin_en = 1;
	if (spi_controller_is_slave(ctlr))
		spicc->cfg_spi.b.slave_en = true;
	/* default pending */
	spicc->cfg_start.b.pending = 1;

	device_reset_optional(&pdev->dev);
	ctlr->num_chipselect = 4;
	ctlr->dev.of_node = pdev->dev.of_node;
	ctlr->mode_bits = SPI_CPHA | SPI_CPOL | SPI_LSB_FIRST |
			  SPI_3WIRE | SPI_TX_QUAD | SPI_RX_QUAD;
	ctlr->max_speed_hz = 1000 * 1000 * 100;
	ctlr->min_speed_hz = 1000 * 10;
	ctlr->setup = meson_spicc_setup;
	ctlr->cleanup = meson_spicc_cleanup;
	ctlr->prepare_message = meson_spicc_prepare_message;
	ctlr->unprepare_transfer_hardware = meson_spicc_unprepare_transfer;
#ifdef CONFIG_ARCH_MESON_ODROID_COMMON
	ctlr->transfer_one_message = use_hw_cs ? meson_spicc_transfer_one_message : hk_sw_meson_spicc_transfer_one_message;
	ctlr->use_gpio_descriptors = !use_hw_cs;
#else
	ctlr->transfer_one_message = meson_spicc_transfer_one_message;
#endif
	ctlr->slave_abort = meson_spicc_slave_abort;
	ctlr->can_dma = meson_spicc_can_dma;
	ctlr->max_dma_len = SPICC_BLOCK_MAX;
	dma_set_max_seg_size(&pdev->dev, SPICC_BLOCK_MAX);
	ret = devm_spi_register_master(&pdev->dev, ctlr);
	if (ret) {
		dev_err(&pdev->dev, "spi controller registration failed\n");
		goto out_clk;
	}

	init_completion(&spicc->completion);

#ifdef MESON_SPICC_HW_IF
	/* additional descriptor to achieve a ss trailing gap */
	spicc->dirspi_desc_len = sizeof(struct spicc_descriptor);
	if (spicc->config_ss_trailing_gap)
		spicc->dirspi_desc_len += sizeof(struct spicc_descriptor);
	spicc->dirspi_desc = dma_alloc_coherent(&pdev->dev,
					spicc->dirspi_desc_len,
					&spicc->dirspi_desc_paddr,
					GFP_KERNEL | GFP_DMA);
#endif

#ifdef CONFIG_SPICC_TEST
	device_create_file(&ctlr->dev, &dev_attr_test);
	device_create_file(&ctlr->dev, &dev_attr_testdev);
#endif

	return 0;

out_clk:
	if (spicc->sys_clk)
		clk_disable_unprepare(spicc->sys_clk);
	clk_disable_unprepare(spicc->spi_clk);
out_controller:
	spi_controller_put(ctlr);

	return ret;
}

static int meson_spicc_remove(struct platform_device *pdev)
{
	struct spicc_device *spicc = platform_get_drvdata(pdev);

	if (spicc->sys_clk)
		clk_disable_unprepare(spicc->sys_clk);
	clk_disable_unprepare(spicc->spi_clk);

#ifdef MESON_SPICC_HW_IF
	if (spicc->dirspi_desc)
		dma_free_coherent(&pdev->dev,
				  spicc->dirspi_desc_len,
				  spicc->dirspi_desc,
				  spicc->dirspi_desc_paddr);
#endif

#ifdef CONFIG_SPICC_TEST
	device_remove_file(&pdev->dev, &dev_attr_test);
	device_remove_file(&pdev->dev, &dev_attr_testdev);
#endif
	return 0;
}

static int meson_spicc_off(struct spicc_device *spicc)
{
	pinctrl_pm_select_sleep_state(&spicc->pdev->dev);
	clk_disable_unprepare(spicc->spi_clk);
	if (spicc->sys_clk)
		clk_disable_unprepare(spicc->sys_clk);

	return 0;
}

static int meson_spicc_on(struct spicc_device *spicc)
{
	if (spicc->sys_clk)
		clk_prepare_enable(spicc->sys_clk);
	clk_prepare_enable(spicc->spi_clk);
	pinctrl_pm_select_default_state(&spicc->pdev->dev);

	return 0;
}

#ifdef CONFIG_HIBERNATION
static int meson_spicc_freeze(struct device *dev)
{
	struct spicc_device *spicc = dev_get_drvdata(dev);

	return meson_spicc_off(spicc);
}

static int meson_spicc_thaw(struct device *dev)
{
	struct spicc_device *spicc = dev_get_drvdata(dev);

	return meson_spicc_on(spicc);
}

static int meson_spicc_restore(struct device *dev)
{
	struct spicc_device *spicc = dev_get_drvdata(dev);

	return meson_spicc_on(spicc);
}
#endif

static void meson_spicc_shutdown(struct platform_device *pdev)
{
	struct spicc_device *spicc = platform_get_drvdata(pdev);

	meson_spicc_off(spicc);
}

static int meson_spicc_suspend(struct device *dev)
{
	struct spicc_device *spicc = dev_get_drvdata(dev);
	int ret;

	ret = spi_controller_suspend(spicc->controller);
	if (!ret)
		ret = meson_spicc_off(spicc);

	return ret;
}

static int meson_spicc_resume(struct device *dev)
{
	struct spicc_device *spicc = dev_get_drvdata(dev);
	int ret;

	ret = meson_spicc_on(spicc);
	if (!ret)
		ret = spi_controller_resume(spicc->controller);

	return ret;
}

static const struct dev_pm_ops meson_spicc_pm_ops = {
	.suspend	= meson_spicc_suspend,
	.resume		= meson_spicc_resume,
#ifdef CONFIG_HIBERNATION
	.freeze		= meson_spicc_freeze,
	.thaw		= meson_spicc_thaw,
	.restore	= meson_spicc_restore,
#endif
};

static const struct of_device_id meson_spicc_v2_of_match[] = {
	{
		.compatible = "amlogic,meson-a4-spicc-v2",
		.data = 0,
	},

	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, meson_spicc_v2_of_match);

struct platform_driver meson_spicc_v2_driver = {
	.probe   = meson_spicc_probe,
	.remove  = meson_spicc_remove,
	.shutdown = meson_spicc_shutdown,
	.driver  = {
		.name = "meson-spicc-v2",
		.of_match_table = of_match_ptr(meson_spicc_v2_of_match),
		.pm = &meson_spicc_pm_ops,
	},
};

#ifndef MODULE
module_platform_driver(meson_spicc_v2_driver);
#endif

MODULE_DESCRIPTION("Meson SPI Communication Controller(v2) driver");
MODULE_AUTHOR("Sunny.luo <sunny.luo@amlogic.com>");
MODULE_LICENSE("GPL");
