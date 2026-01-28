/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _AML_SHA3_DMA_H_
#define _AML_SHA3_DMA_H_

#define SHAKE_128_DIGEST_SIZE	(128 / 8)
#define SHAKE_128_BLOCK_SIZE	(200 - 2 * SHAKE_128_DIGEST_SIZE)

#define SHAKE_256_DIGEST_SIZE	(256 / 8)
#define SHAKE_256_BLOCK_SIZE	(200 - 2 * SHAKE_256_DIGEST_SIZE)

#define ENC_SHA_ONLY_SHA3  (0)
#define ENC_SHA_ONLY_SHAKE (1)

#define OP_MODE_SHA3_224 (0)
#define OP_MODE_SHA3_256 (1)
#define OP_MODE_SHA3_384 (2)
#define OP_MODE_SHA3_512 (3)
#define OP_MODE_SHAKE_128_ABSORB  (0)
#define OP_MODE_SHAKE_256_ABSORB  (1)
#define OP_MODE_SHAKE_128_SQUEEZE (2)
#define OP_MODE_SHAKE_256_SQUEEZE (3)

int aml_sha3_process(struct ahash_request *req);
void aml_sha3_finish_req(struct ahash_request *req, int err);

#endif  // _AML_SHA_DMA_H_
