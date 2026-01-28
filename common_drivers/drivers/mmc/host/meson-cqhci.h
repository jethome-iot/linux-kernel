/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef LINUX_AMLMMC_CQHCI_H
#define LINUX_AMLMMC_CQHCI_H

#include <linux/compiler.h>
#include <linux/bitops.h>
#include <linux/spinlock_types.h>
#include <linux/types.h>
#include <linux/completion.h>
#include <linux/wait.h>
#include <linux/irqreturn.h>

#include "../drivers/mmc/host/cqhci.h"
/* The resp returned by cmd19 and cmd52/3 can't use the same mask */
#define SDIO_RESP_ERR_MASK      0x0

#define SD_EMMC_CQE_REG         0x100
/* Crypto General Enable: Enable/Disable bit for Crypto Engine */
#define   CRYPTO_ENGINE         BIT(1)
/* CRYPTO REGISTERS */
#define CQHCI_CRNQP		(0x70)
/* Crypto Enable for Non-Queue Data Transfers (CE) */
#define   CRYPTO_NON_QUEUE      BIT(8)
/* Crypto Configuration Index for Non-Queue Data Transfers (CCI) */
#define   CRYPTO_CFG_INDEX      GENMASK(7, 0)

#define CQHCI_CRNQDUN           (0x74)
#define CQHCI_CRNQIS            (0x78)
#define CQHCI_CRNQIE            (0x7C)
/* crypto rev1; CFGPTR = 2 */
#define CQHCI_CRCAP             (0x100)
#define   CQHCI_CFGPTR          (2)

#define CQHCI_CRCAP0            (0x104)
#define   CQHCI_RESERVED_KS     (0)
#define   CFG_128_BIT	        (1)
#define   CFG_192_BIT           (2)
#define   CFG_256_BIT           (3)
#define   CFG_512_BIT           (4)
#define   CRYPTO_KS(x)          ((x) << 16)
#define CQHCI_CRCAP1            (0x108)
#define CQHCI_CRCAP2            (0x10C)
#define CQHCI_CRCFG(x)          (CQHCI_CFGPTR * 0x100 + ((x) * 0x80))
#define CQHCI_DUSIZE_MASK       (0xFF)


bool aml_cqe_irq(struct meson_host *host, u32 intmask, int *cmd_error,
	int *data_error);
u32 aml_cqhci_irq(struct meson_host *host);
void aml_cqhci_writel(struct cqhci_host *host, u32 val, int reg);
u32 aml_cqhci_readl(struct cqhci_host *host, int reg);
void aml_cqe_enable(struct mmc_host *mmc);
void aml_cqe_disable(struct mmc_host *mmc, bool recovery);
int amlogic_add_host(struct meson_host *host);
void meson_crypto_init(struct cqhci_host *cq_host);
int meson_crypto_prepare_req(struct mmc_host *mmc, struct mmc_request *mrq);
int meson_crypto_post_req(struct mmc_host *mmc, struct mmc_request *mrq);

#endif
