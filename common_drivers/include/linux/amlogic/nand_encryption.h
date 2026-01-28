/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_CRYPTO_H_
#define __AML_CRYPTO_H_

#define CONFIG_NAND_ENCRYPTION	1
#define CRYPT_BLOCK_SIZE	16
#define MAX_ENCRYPT_PARTITIONS	4

#define IV_FACTOR_BASE_ADDR	(0x800)

struct encrypt_partition {
	u64 size;
	u64 offset;
	int shift;
	int map_len;
	u16 *map;
	struct crypto_skcipher *nand_cipher;
	struct crypto_skcipher *iv_cipher;
	unsigned char key[CRYPT_BLOCK_SIZE];
	unsigned char nand_key_hash[CRYPT_BLOCK_SIZE];
	unsigned char have_nand_key;
	unsigned char have_nand_key_hash;
};

void nand_decrypt_page(struct encrypt_partition *encrypt_region,
		       unsigned int pagesize, unsigned char *buf, int page);
bool nand_encrypt_page(struct encrypt_partition *encrypt_region,
		       unsigned int pagesize,
		       const unsigned char *buf, int page);
struct encrypt_partition *is_need_encrypted(unsigned long addr);
int set_region_encrypted(struct mtd_info *mtd,
			 struct mtd_partition *part,
			 bool need_map);
int disable_region_encrypted(struct mtd_partition *part);
#endif
