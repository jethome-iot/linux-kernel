// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/mtd/spinand.h>
#include <linux/amlogic/aml_spi_mem.h>

#define SPINAND_MFR_FORESEE			0xCD

#define FORESEE_F35SQB002G_STATUS_ECC_MASK				GENMASK(6, 4)
#define FORESEE_F35SQB002G_STATUS_ECC_NO_BITFLIPS		(0 << 4)
#define FORESEE_F35SQB002G_STATUS_ECC_1_3_BITFLIPS		(1 << 4)
#define FORESEE_F35SQB002G_STATUS_ECC_4_BITFLIPS		(2 << 4)
#define FORESEE_F35SQB002G_STATUS_ECC_5_BITFLIPS		(3 << 4)
#define FORESEE_F35SQB002G_STATUS_ECC_6_BITFLIPS		(4 << 4)
#define FORESEE_F35SQB002G_STATUS_ECC_7_BITFLIPS		(5 << 4)
#define FORESEE_F35SQB002G_STATUS_ECC_8_BITFLIPS		(6 << 4)
#define FORESEE_F35SQB002G_STATUS_ECC_UNCOR_ERROR		(7 << 4)

static SPINAND_OP_VARIANTS(read_cache_variants,
		//SPINAND_PAGE_READ_FROM_CACHE_QUADIO_OP(0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_X4_OP(0, 1, NULL, 0),
		//SPINAND_PAGE_READ_FROM_CACHE_DUALIO_OP(0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_X2_OP(0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_OP(true, 0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_OP(false, 0, 1, NULL, 0));

static SPINAND_OP_VARIANTS(write_cache_variants,
		SPINAND_PROG_LOAD_X4(true, 0, NULL, 0),
		SPINAND_PROG_LOAD(true, 0, NULL, 0));

static SPINAND_OP_VARIANTS(update_cache_variants,
		SPINAND_PROG_LOAD_X4(false, 0, NULL, 0),
		SPINAND_PROG_LOAD(false, 0, NULL, 0));

static int f35sqa001g_ooblayout_ecc(struct mtd_info *mtd, int section,
								struct mtd_oob_region *region)
{
		/* Unable to know the layout of ECC */
		return -ERANGE;
}

static int f35sqa001g_ooblayout_free(struct mtd_info *mtd, int section,
								struct mtd_oob_region *region)
{
		if (section > 0)
			return -ERANGE;

		/* Reserve 2 bytes for the BBM. */
		region->offset = 2;
		region->length = mtd->oobsize - 2;

		return 0;
}

static const struct mtd_ooblayout_ops f35sqa001g_ooblayout = {
	.ecc = f35sqa001g_ooblayout_ecc,
	.free = f35sqa001g_ooblayout_free,
};

static int f35sqa001g_ecc_get_status(struct spinand_device *spinand,
									u8 status)
{
		switch (status & STATUS_ECC_MASK) {
		case STATUS_ECC_NO_BITFLIPS:
				return 0;

		case STATUS_ECC_HAS_BITFLIPS:
		/*
		 * We have no way to know exactly how many bitflips have been
		 * fixed, so let's return the maximum possible value so that
		 * wear-leveling layers move the data immediately.
		 */
				return 1;

		case STATUS_ECC_UNCOR_ERROR:
				return -EBADMSG;

		default:
				break;
		}

		return -EINVAL;
}

static int f35sqb002g_ooblayout_ecc(struct mtd_info *mtd, int section,
				   struct mtd_oob_region *region)
{
	/* Unable to know the layout of ECC */
	return -ERANGE;
}

static int f35sqb002g_ooblayout_free(struct mtd_info *mtd, int section,
				   struct mtd_oob_region *region)
{
	if (section > 3)
		return -ERANGE;

	region->offset = (16 * section) + 4;
	region->length = 12;

	return 0;
}

static const struct mtd_ooblayout_ops f35sqb002g_ooblayout = {
	.ecc = f35sqb002g_ooblayout_ecc,
	.free = f35sqb002g_ooblayout_free,
};

static int f35sqb004g_ooblayout_ecc(struct mtd_info *mtd, int section,
				   struct mtd_oob_region *region)
{
	/* Unable to know the layout of ECC */
	return -ERANGE;
}

static int f35sqb004g_ooblayout_free(struct mtd_info *mtd, int section,
				   struct mtd_oob_region *region)
{
	if (section > 7)
		return -ERANGE;

	region->offset = (16 * section) + 4;
	region->length = 12;

	return 0;
}

static const struct mtd_ooblayout_ops f35sqb004g_ooblayout = {
	.ecc = f35sqb004g_ooblayout_ecc,
	.free = f35sqb004g_ooblayout_free,
};

static int f35sqb002g_ecc_get_status(struct spinand_device *spinand,
				   u8 status)
{
	switch (status & FORESEE_F35SQB002G_STATUS_ECC_MASK) {
	case FORESEE_F35SQB002G_STATUS_ECC_NO_BITFLIPS:
		return 0;
	case FORESEE_F35SQB002G_STATUS_ECC_1_3_BITFLIPS:
		/*
		 * We have no way to know exactly how many bitflips have been
		 * fixed, so let's return the maximum possible value so that
		 * wear-leveling layers move the data immediately.
		 */
		return 3;
	case FORESEE_F35SQB002G_STATUS_ECC_4_BITFLIPS:
		return 4;
	case FORESEE_F35SQB002G_STATUS_ECC_5_BITFLIPS:
		return 5;
	case FORESEE_F35SQB002G_STATUS_ECC_6_BITFLIPS:
		return 6;
	case FORESEE_F35SQB002G_STATUS_ECC_7_BITFLIPS:
		return 7;
	case FORESEE_F35SQB002G_STATUS_ECC_8_BITFLIPS:
		return 8;
	case FORESEE_F35SQB002G_STATUS_ECC_UNCOR_ERROR:
		return -EBADMSG;

	default:
		break;
	}

	return -EINVAL;
}

static const struct spinand_info foresee_spinand_table[] = {
	SPINAND_INFO("F35SQA001G",
		     SPINAND_ID(SPINAND_READID_METHOD_OPCODE_DUMMY, 0x71, 0X71),
		     NAND_MEMORG(1, 2048, 64, 64, 1024, 20, 1, 1, 1),
		     NAND_ECCREQ(1, 512),
		     SPINAND_INFO_OP_VARIANTS(&read_cache_variants,
					      &write_cache_variants,
					      &update_cache_variants),
		     SPINAND_HAS_QE_BIT,
		     SPINAND_ECCINFO(&f35sqa001g_ooblayout,
				     f35sqa001g_ecc_get_status)),

	SPINAND_INFO("F35SQB002G 3.3v",
		     SPINAND_ID(SPINAND_READID_METHOD_OPCODE_DUMMY, 0x52, 0x52),
		     NAND_MEMORG(1, 2048, 64, 64, 2048, 40, 1, 1, 1),
		     NAND_ECCREQ(8, 512),
		     SPINAND_INFO_OP_VARIANTS(&read_cache_variants,
					      &write_cache_variants,
					      &update_cache_variants),
		     SPINAND_HAS_QE_BIT,
		     SPINAND_ECCINFO(&f35sqb002g_ooblayout,
				     f35sqb002g_ecc_get_status)),
	SPINAND_INFO("F35SQB004G 3.3v",
		     SPINAND_ID(SPINAND_READID_METHOD_OPCODE_DUMMY, 0x53, 0x53),
		     NAND_MEMORG(1, 4096, 128, 64, 2048, 40, 1, 1, 1),
		     NAND_ECCREQ(8, 512),
		     SPINAND_INFO_OP_VARIANTS(&read_cache_variants,
					      &write_cache_variants,
					      &update_cache_variants),
		     SPINAND_HAS_QE_BIT,
		     SPINAND_ECCINFO(&f35sqb004g_ooblayout,
				     f35sqb002g_ecc_get_status)),
};

static const struct spinand_manufacturer_ops foresee_spinand_manuf_ops = {
};

const struct spinand_manufacturer foresee_spinand_manufacturer = {
	.id = SPINAND_MFR_FORESEE,
	.name = "Foresee",
	.chips = foresee_spinand_table,
	.nchips = ARRAY_SIZE(foresee_spinand_table),
	.ops = &foresee_spinand_manuf_ops,
};
