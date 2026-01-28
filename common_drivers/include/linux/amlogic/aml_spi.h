/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_SPI_H_
#define __AML_SPI_H_

enum {
	DMA_TRIG_NORMAL = 0,
	DMA_TRIG_VSYNC,
	DMA_TRIG_LINE_N,
	DMA_TRIG_PWM_VS,
};

#define CONTROLLER_SPICC	1
#define CONTROLLER_SPISG	2

#define CAP_DMA_TRIG_VSYNC	BIT(0)
#define CAP_DMA_TRIG_LINE_N	BIT(1)
#define CAP_DMA_TRIG_PWM_VS	BIT(2)
#define CAP_TRIG_DELAY		BIT(3)

#define DC_MODE_NONE		0
#define DC_MODE_PIN		1
#define DC_MODE_9BIT		2

#define DC_LEVEL_LOW		0
#define DC_LEVEL_HIGH		1

struct spicc_controller_data {
	unsigned int	controller_version;
	unsigned int	controller_capabilities;
	unsigned	use_ctrl_cs:1;
	unsigned	use_dirspi:1;
	unsigned	ccxfer_en:1;
	unsigned	timing_en:1;
	unsigned	ss_leading_gap:16;
	unsigned	ss_trailing_gap:15;
	unsigned	tx_tuning:4;
	unsigned	rx_tuning:4;
	unsigned	dummy_ctl:1;
	unsigned	dc_mode:2;
	unsigned	dc_level:1;
	unsigned	read_turn_around:8;
	unsigned	miso_latency_en:1;
	int		miso_latency; // in ns, signed
	unsigned int	dma_trig_delay;
	void *priv;
	void (*dirspi_start)(struct spi_device *spi);
	void (*dirspi_stop)(struct spi_device *spi);
	int (*dirspi_async)(struct spi_device *spi,
			    dma_addr_t tx_dma,
			    dma_addr_t rx_dma,
			    int len,
			    void (*complete)(void *context),
			    void *context);
	int (*dirspi_sync)(struct spi_device *spi,
			   dma_addr_t tx_dma,
			   dma_addr_t rx_dma,
			   int len);
	int (*dirspi_xfer)(struct spi_device *spi,
			   u8 *tx_buf,
			   u8 *rx_buf,
			   int len);
	int (*dirspi_dma_trig)(struct spi_device *spi,
			       dma_addr_t tx_dma,
			       dma_addr_t rx_dma,
			       int len,
			       u8 src);
	int (*dirspi_dma_trig_start)(struct spi_device *spi);
	int (*dirspi_dma_trig_stop)(struct spi_device *spi);
	int (*dirspi_dma_trig_release)(struct spi_device *spi);
	int (*dirspi_busy_proc)(struct spi_device *spi);
};

static inline bool is_spicc_capable(struct spicc_controller_data *cdata, unsigned int cap_type)
{
	return cdata->controller_capabilities & cap_type;
}

#endif
