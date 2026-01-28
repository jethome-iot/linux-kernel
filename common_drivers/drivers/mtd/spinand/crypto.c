// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <crypto/skcipher.h>
#include <crypto/hash.h>
#include <linux/scatterlist.h>
#include <linux/mtd/rawnand.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/amlogic/nand_encryption.h>

int encrypt_part_num;
struct encrypt_partition encrypt_part[MAX_ENCRYPT_PARTITIONS] = {0};

/* nand crypto mutex */
DEFINE_MUTEX(nand_crypto_mutex);
static unsigned char iv[CRYPT_BLOCK_SIZE];

static int get_encrypted_part_from_dts(const char *part_name)
{
	struct property *prop;
	struct device_node *keys;
	int ret;

	keys = of_find_node_by_name(NULL, "keys");
	if (!keys) {
		pr_err("can't find keys node\n");
		return -1;
	}

	prop = of_find_property(keys, part_name, &ret);
	if (!prop)
		return -1;

	return 0;
}

static int get_key_from_dts(unsigned char *key, const char *part_name)
{
	struct property *prop;
	struct device_node *keys;
	unsigned int tmp[4];
	int ret;

	keys = of_find_node_by_name(NULL, "keys");
	if (!keys) {
		pr_err("can't find keys node\n");
		return -1;
	}
	ret = of_property_read_u32_array(keys, part_name,
					 tmp, 4);
	if (ret) {
		pr_info("read %s key failed\n", part_name);
		return ret;
	}

	tmp[0] = cpu_to_be32(tmp[0]);
	tmp[1] = cpu_to_be32(tmp[1]);
	tmp[2] = cpu_to_be32(tmp[2]);
	tmp[3] = cpu_to_be32(tmp[3]);
	memcpy(key, tmp, CRYPT_BLOCK_SIZE);

	prop = of_find_property(keys, part_name, &ret);
	if (!prop) {
		pr_err("%s key not found, unable to remove\n", part_name);
	} else {
		if (of_remove_property(keys, prop))
			pr_warn("failed to remove %s key\n", part_name);
	}

	return 0;
}

static unsigned char *get_nand_key(struct encrypt_partition *encrypt_region,
				   const char *part_name)
{
	struct encrypt_partition *encrypt_region_start = encrypt_part;
	int i, j;

	if (encrypt_region->have_nand_key)
		return encrypt_region->key;

	if (!get_key_from_dts(encrypt_region->key, part_name))
		encrypt_region->have_nand_key = 1;
	else
		return NULL;

	for (i = 0; i < MAX_ENCRYPT_PARTITIONS; i++, encrypt_region_start++) {
		if (!encrypt_region_start->offset ||
		    !encrypt_region_start->size)
			continue;

		if (!encrypt_region->have_nand_key)
			continue;

		for (j = 0; j < CRYPT_BLOCK_SIZE; j++)
			if (encrypt_region_start->key[j] != encrypt_region->key[j])
				break;

		WARN_ON(j == CRYPT_BLOCK_SIZE);
	}

	return encrypt_region->key;
}

static unsigned char *get_nand_key_hash(struct encrypt_partition *encrypt_region)
{
	struct shash_desc *desc = 0;
	int descsize;
	struct crypto_shash *hash;

	if (encrypt_region->have_nand_key_hash)
		return encrypt_region->nand_key_hash;

	hash = crypto_alloc_shash("md5", 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(hash))
		return NULL;

	if (crypto_shash_digestsize(hash) > CRYPT_BLOCK_SIZE)
		return NULL;

	descsize = sizeof(struct shash_desc) + crypto_shash_descsize(hash);
	desc = kzalloc(descsize, GFP_KERNEL);
	if (!desc)
		return NULL;

	desc->tfm = hash;
	crypto_shash_init(desc);
	crypto_shash_digest(desc, encrypt_region->key,
			    CRYPT_BLOCK_SIZE, encrypt_region->nand_key_hash);
	crypto_free_shash(hash);
	kfree(desc);
	encrypt_region->have_nand_key_hash = 1;

	return encrypt_region->nand_key_hash;
}

enum crypttype {
	ENCRYPT,
	DECRYPT
};

static int nand_crypt(enum crypttype crypt_type,
		      struct crypto_skcipher *pcipher,
		      const u8 *data, const u8 *dst, unsigned int data_size,
		      const u8 *iv)
{
	struct skcipher_request *req = NULL;
	struct scatterlist sg, sg_dst, *d = &sg;
	DECLARE_CRYPTO_WAIT(wait);
	int result;

	/* Allocate a request object */
	req = skcipher_request_alloc(pcipher, GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	sg_init_one(&sg, data, data_size);
	if (data != dst) {
		sg_init_one(&sg_dst, dst, data_size);
		d = &sg_dst;
	}

	skcipher_request_set_callback(req, CRYPTO_TFM_REQ_MAY_BACKLOG |
				      CRYPTO_TFM_REQ_MAY_SLEEP,
				      crypto_req_done, &wait);
	skcipher_request_set_crypt(req, &sg,
				   d, data_size, (void *)iv);

	if (crypt_type == ENCRYPT)
		result = crypto_wait_req(crypto_skcipher_encrypt(req), &wait);
	else
		result = crypto_wait_req(crypto_skcipher_decrypt(req), &wait);
	if (result)
		pr_err("Error encrypting data: %d\n", result);

	skcipher_request_free(req);

	return result;
}

int get_iv_fator(struct encrypt_partition *encrypt_region,
		 int page, int pagesize)
{
	int i, lpn, temp;

	if (!encrypt_region->map)
		return page;

	temp = page & (~((1 << encrypt_region->shift) - 1));
	if ((u16)temp > encrypt_region->map[encrypt_region->map_len - 1])
		return -1;

	for (i = 0; i < encrypt_region->map_len; i++)
		if ((u16)temp < encrypt_region->map[i])
			break;

	lpn = (i - 1) << encrypt_region->shift;
	lpn += (page & ((1 << encrypt_region->shift) - 1));

	return IV_FACTOR_BASE_ADDR + lpn * (pagesize >> 9);
}

void nand_decrypt_page(struct encrypt_partition *encrypt_region,
		       unsigned int pagesize, unsigned char *buf, int page)
{
	int s, i = pagesize >> 9, lbn;

	might_sleep();

	lbn = get_iv_fator(encrypt_region, page, pagesize);
	WARN_ON(lbn < 0);
	while (i--) {
		u8 iv_iv[16] = {0xff};

		/* First set iv = page number. */
		iv[0] = lbn & 0xFF;
		iv[1] = (lbn >> 8) & 0xFF;
		iv[2] = (lbn >> 16) & 0xFF;
		iv[3] = (lbn >> 24) & 0xFF;
		memset(&iv[4], 0, CRYPT_BLOCK_SIZE - 4);
		nand_crypt(ENCRYPT, encrypt_region->iv_cipher,
			   iv, iv, CRYPT_BLOCK_SIZE, iv_iv);
		s = nand_crypt(DECRYPT, encrypt_region->nand_cipher,
			       buf, buf, 512, iv);
		/* TODO: understand failure modes of nand_crypt */
		WARN_ON(s < 0);
		lbn += 1;
		buf += 512;
	}
}
EXPORT_SYMBOL_GPL(nand_decrypt_page);

bool nand_encrypt_page(struct encrypt_partition *encrypt_region,
		       unsigned int pagesize,
		       const unsigned char *buf, int page)
{
	unsigned char *new_buf, *new_temp_buf, *tmp_buf = (unsigned char *)buf;
	int s, lbn, i = pagesize >> 9;

	might_sleep();

	new_buf = kmalloc(pagesize, GFP_KERNEL);
	if (WARN_ON_ONCE(!new_buf))
		return NULL;

	lbn = get_iv_fator(encrypt_region, page, pagesize);
	WARN_ON(lbn < 0);

	new_temp_buf = new_buf;
	while (i--) {
		u8 iv_iv[16] = {0xff};

		/* First set iv = page number. */
		iv[0] = lbn & 0xFF;
		iv[1] = (lbn >> 8) & 0xFF;
		iv[2] = (lbn >> 16) & 0xFF;
		iv[3] = (lbn >> 24) & 0xFF;
		memset(&iv[4], 0, CRYPT_BLOCK_SIZE - 4);
		nand_crypt(ENCRYPT, encrypt_region->iv_cipher,
			   iv, iv, CRYPT_BLOCK_SIZE, iv_iv);
		s = nand_crypt(ENCRYPT, encrypt_region->nand_cipher,
			       tmp_buf, new_temp_buf, 512, iv);
		/*
		 * TODO: understand failure modes of nand_crypt.
		 * This probably ought to be BUG_ON
		 */
		if (WARN_ON_ONCE(s < 0))
			return false;

		lbn += 1;
		tmp_buf += 512;
		new_temp_buf += 512;
	}

	memcpy((void *)buf, new_buf, pagesize);
	kfree(new_buf);

	return true;
}
EXPORT_SYMBOL_GPL(nand_encrypt_page);

int get_cipher(struct crypto_skcipher **p_cipher, const unsigned char *key)
{
	int result;

	if (!key)
		return -1;

	*p_cipher = crypto_alloc_skcipher("cbc(aes)", 0, 0);
	if (IS_ERR(*p_cipher)) {
		result = PTR_ERR(*p_cipher);
		*p_cipher = NULL;
		return result;
	}

	return crypto_skcipher_setkey(*p_cipher,
				      key,
				      CRYPT_BLOCK_SIZE);
}

struct encrypt_partition *is_need_encrypted(unsigned long addr)
{
	struct encrypt_partition *encrypt_region = encrypt_part;
	int i;

	/*
	 * each partition can be in either encrypted state or
	 * unencrypted state, but can't be in both encrypted
	 * and unencrypted state, so simply loop for looking
	 * whether the page is in the encrypted region or not.
	 */
	for (i = 0; i < MAX_ENCRYPT_PARTITIONS; i++, encrypt_region++) {
		if (!encrypt_region->offset || !encrypt_region->size)
			continue;

		if (addr >= encrypt_region->offset &&
		    addr < encrypt_region->offset + encrypt_region->size)
			return encrypt_region;
	}

	return NULL;
}
EXPORT_SYMBOL_GPL(is_need_encrypted);

int set_region_encrypted(struct mtd_info *mtd,
			 struct mtd_partition *part,
			 bool need_map)
{
	struct encrypt_partition *encrypt_region = encrypt_part;
	int i, ret;

	if (encrypt_part_num >= MAX_ENCRYPT_PARTITIONS)
		return -1;

	ret = get_encrypted_part_from_dts(part->name);
	if (ret)
		return -1;

	for (i = 0; i < MAX_ENCRYPT_PARTITIONS; i++, encrypt_region++)
		if (encrypt_region->offset == part->offset &&
		    encrypt_region->size == part->size)
			return 0;

	encrypt_region = encrypt_part;
	for (i = 0; i < MAX_ENCRYPT_PARTITIONS; i++, encrypt_region++) {
		int result, map_size;

		if (encrypt_region->offset || encrypt_region->size)
			continue;

		result = get_cipher(&encrypt_region->nand_cipher,
				    get_nand_key(encrypt_region, part->name));
		if (result < 0)
			return -1;

		result = get_cipher(&encrypt_region->iv_cipher,
				    get_nand_key_hash(encrypt_region));
		if (result < 0)
			return -1;

		if (need_map) {
			loff_t offset = part->offset;
			loff_t end = part->offset + part->size;
			int j = 0;

			map_size = part->size >> mtd->erasesize_shift;
			encrypt_region->map = kmalloc(map_size << 1, GFP_KERNEL);
			while (offset < end) {
				if (!mtd->_block_isbad(mtd, offset))
					encrypt_region->map[j++] =
						offset >> mtd->writesize_shift;
				offset += mtd->erasesize;
			}
			encrypt_region->map_len = j;
			encrypt_region->shift = mtd->erasesize_shift -
						mtd->writesize_shift;
		}
		encrypt_region->offset =  part->offset;
		encrypt_region->size =  part->size;
		pr_debug("encrypted region: %llx %llx\n", part->offset, part->size);
		encrypt_part_num++;
		return 0;
	}

	return -1;
}
EXPORT_SYMBOL_GPL(set_region_encrypted);

int disable_region_encrypted(struct mtd_partition *part)
{
	struct encrypt_partition *encrypt_region = encrypt_part;
	int i;

	if (encrypt_part_num == 0)
		return 0;

	for (i = 0; i < MAX_ENCRYPT_PARTITIONS; i++, encrypt_region++) {
		if (encrypt_region->offset == part->offset &&
		    encrypt_region->size == part->size) {
			encrypt_region->offset = 0;
			encrypt_region->size = 0;
			encrypt_region->nand_cipher = NULL;
			encrypt_region->iv_cipher = NULL;
			kfree(encrypt_region->map);
			pr_debug("unencrypted region<-: %llx %llx\n",
				part->offset, part->size);
			encrypt_part_num--;
			return 0;
		}
	}

	return -1;
}
EXPORT_SYMBOL_GPL(disable_region_encrypted);
