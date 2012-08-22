/*
 * Glue Code for AVX assembler versions of Serpent Cipher
 *
 * Copyright (C) 2012 Johannes Goetzfried
 *     <Johannes.Goetzfried@informatik.stud.uni-erlangen.de>
 *
 * Glue code based on serpent_sse2_glue.c by:
 *  Copyright (C) 2011 Jussi Kivilinna <jussi.kivilinna@mbnet.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 *
 */

#include <linux/module.h>
#include <linux/hardirq.h>
#include <linux/types.h>
#include <linux/crypto.h>
#include <linux/err.h>
#include <crypto/algapi.h>
#include <crypto/serpent.h>
#include <crypto/cryptd.h>
#include <crypto/b128ops.h>
#include <crypto/ctr.h>
#include <crypto/lrw.h>
#include <crypto/xts.h>
#include <asm/xcr.h>
#include <asm/xsave.h>
#include <asm/crypto/serpent-avx.h>
#include <asm/crypto/ablk_helper.h>
#include <asm/crypto/glue_helper.h>

static void serpent_decrypt_cbc_xway(void *ctx, u128 *dst, const u128 *src)
{
	u128 ivs[SERPENT_PARALLEL_BLOCKS - 1];
	unsigned int j;

	for (j = 0; j < SERPENT_PARALLEL_BLOCKS - 1; j++)
		ivs[j] = src[j];

	serpent_dec_blk_xway(ctx, (u8 *)dst, (u8 *)src);

	for (j = 0; j < SERPENT_PARALLEL_BLOCKS - 1; j++)
		u128_xor(dst + (j + 1), dst + (j + 1), ivs + j);
}

static void serpent_crypt_ctr(void *ctx, u128 *dst, const u128 *src, u128 *iv)
{
	be128 ctrblk;

	u128_to_be128(&ctrblk, iv);
	u128_inc(iv);

	__serpent_encrypt(ctx, (u8 *)&ctrblk, (u8 *)&ctrblk);
	u128_xor(dst, src, (u128 *)&ctrblk);
}

static void serpent_crypt_ctr_xway(void *ctx, u128 *dst, const u128 *src,
				   u128 *iv)
{
	be128 ctrblks[SERPENT_PARALLEL_BLOCKS];
	unsigned int i;

	for (i = 0; i < SERPENT_PARALLEL_BLOCKS; i++) {
		if (dst != src)
			dst[i] = src[i];

		u128_to_be128(&ctrblks[i], iv);
		u128_inc(iv);
	}

	serpent_enc_blk_xway_xor(ctx, (u8 *)dst, (u8 *)ctrblks);
}

static const struct common_glue_ctx serpent_enc = {
	.num_funcs = 2,
	.fpu_blocks_limit = SERPENT_PARALLEL_BLOCKS,

	.funcs = { {
		.num_blocks = SERPENT_PARALLEL_BLOCKS,
		.fn_u = { .ecb = GLUE_FUNC_CAST(serpent_enc_blk_xway) }
	}, {
		.num_blocks = 1,
		.fn_u = { .ecb = GLUE_FUNC_CAST(__serpent_encrypt) }
	} }
};

static const struct common_glue_ctx serpent_ctr = {
	.num_funcs = 2,
	.fpu_blocks_limit = SERPENT_PARALLEL_BLOCKS,

	.funcs = { {
		.num_blocks = SERPENT_PARALLEL_BLOCKS,
		.fn_u = { .ctr = GLUE_CTR_FUNC_CAST(serpent_crypt_ctr_xway) }
	}, {
		.num_blocks = 1,
		.fn_u = { .ctr = GLUE_CTR_FUNC_CAST(serpent_crypt_ctr) }
	} }
};

static const struct common_glue_ctx serpent_dec = {
	.num_funcs = 2,
	.fpu_blocks_limit = SERPENT_PARALLEL_BLOCKS,

	.funcs = { {
		.num_blocks = SERPENT_PARALLEL_BLOCKS,
		.fn_u = { .ecb = GLUE_FUNC_CAST(serpent_dec_blk_xway) }
	}, {
		.num_blocks = 1,
		.fn_u = { .ecb = GLUE_FUNC_CAST(__serpent_decrypt) }
	} }
};

static const struct common_glue_ctx serpent_dec_cbc = {
	.num_funcs = 2,
	.fpu_blocks_limit = SERPENT_PARALLEL_BLOCKS,

	.funcs = { {
		.num_blocks = SERPENT_PARALLEL_BLOCKS,
		.fn_u = { .cbc = GLUE_CBC_FUNC_CAST(serpent_decrypt_cbc_xway) }
	}, {
		.num_blocks = 1,
		.fn_u = { .cbc = GLUE_CBC_FUNC_CAST(__serpent_decrypt) }
	} }
};

static int ecb_encrypt(struct blkcipher_desc *desc, struct scatterlist *dst,
		       struct scatterlist *src, unsigned int nbytes)
{
	return glue_ecb_crypt_128bit(&serpent_enc, desc, dst, src, nbytes);
}

static int ecb_decrypt(struct blkcipher_desc *desc, struct scatterlist *dst,
		       struct scatterlist *src, unsigned int nbytes)
{
	return glue_ecb_crypt_128bit(&serpent_dec, desc, dst, src, nbytes);
}

static int cbc_encrypt(struct blkcipher_desc *desc, struct scatterlist *dst,
		       struct scatterlist *src, unsigned int nbytes)
{
	return glue_cbc_encrypt_128bit(GLUE_FUNC_CAST(__serpent_encrypt), desc,
				     dst, src, nbytes);
}

static int cbc_decrypt(struct blkcipher_desc *desc, struct scatterlist *dst,
		       struct scatterlist *src, unsigned int nbytes)
{
	return glue_cbc_decrypt_128bit(&serpent_dec_cbc, desc, dst, src,
				       nbytes);
}

static int ctr_crypt(struct blkcipher_desc *desc, struct scatterlist *dst,
		     struct scatterlist *src, unsigned int nbytes)
{
	return glue_ctr_crypt_128bit(&serpent_ctr, desc, dst, src, nbytes);
}

static inline bool serpent_fpu_begin(bool fpu_enabled, unsigned int nbytes)
{
	return glue_fpu_begin(SERPENT_BLOCK_SIZE, SERPENT_PARALLEL_BLOCKS,
			      NULL, fpu_enabled, nbytes);
}

static inline void serpent_fpu_end(bool fpu_enabled)
{
	glue_fpu_end(fpu_enabled);
}

struct crypt_priv {
	struct serpent_ctx *ctx;
	bool fpu_enabled;
};

static void encrypt_callback(void *priv, u8 *srcdst, unsigned int nbytes)
{
	const unsigned int bsize = SERPENT_BLOCK_SIZE;
	struct crypt_priv *ctx = priv;
	int i;

	ctx->fpu_enabled = serpent_fpu_begin(ctx->fpu_enabled, nbytes);

	if (nbytes == bsize * SERPENT_PARALLEL_BLOCKS) {
		serpent_enc_blk_xway(ctx->ctx, srcdst, srcdst);
		return;
	}

	for (i = 0; i < nbytes / bsize; i++, srcdst += bsize)
		__serpent_encrypt(ctx->ctx, srcdst, srcdst);
}

static void decrypt_callback(void *priv, u8 *srcdst, unsigned int nbytes)
{
	const unsigned int bsize = SERPENT_BLOCK_SIZE;
	struct crypt_priv *ctx = priv;
	int i;

	ctx->fpu_enabled = serpent_fpu_begin(ctx->fpu_enabled, nbytes);

	if (nbytes == bsize * SERPENT_PARALLEL_BLOCKS) {
		serpent_dec_blk_xway(ctx->ctx, srcdst, srcdst);
		return;
	}

	for (i = 0; i < nbytes / bsize; i++, srcdst += bsize)
		__serpent_decrypt(ctx->ctx, srcdst, srcdst);
}

struct serpent_lrw_ctx {
	struct lrw_table_ctx lrw_table;
	struct serpent_ctx serpent_ctx;
};

static int lrw_serpent_setkey(struct crypto_tfm *tfm, const u8 *key,
			      unsigned int keylen)
{
	struct serpent_lrw_ctx *ctx = crypto_tfm_ctx(tfm);
	int err;

	err = __serpent_setkey(&ctx->serpent_ctx, key, keylen -
							SERPENT_BLOCK_SIZE);
	if (err)
		return err;

	return lrw_init_table(&ctx->lrw_table, key + keylen -
						SERPENT_BLOCK_SIZE);
}

static int lrw_encrypt(struct blkcipher_desc *desc, struct scatterlist *dst,
		       struct scatterlist *src, unsigned int nbytes)
{
	struct serpent_lrw_ctx *ctx = crypto_blkcipher_ctx(desc->tfm);
	be128 buf[SERPENT_PARALLEL_BLOCKS];
	struct crypt_priv crypt_ctx = {
		.ctx = &ctx->serpent_ctx,
		.fpu_enabled = false,
	};
	struct lrw_crypt_req req = {
		.tbuf = buf,
		.tbuflen = sizeof(buf),

		.table_ctx = &ctx->lrw_table,
		.crypt_ctx = &crypt_ctx,
		.crypt_fn = encrypt_callback,
	};
	int ret;

	desc->flags &= ~CRYPTO_TFM_REQ_MAY_SLEEP;
	ret = lrw_crypt(desc, dst, src, nbytes, &req);
	serpent_fpu_end(crypt_ctx.fpu_enabled);

	return ret;
}

static int lrw_decrypt(struct blkcipher_desc *desc, struct scatterlist *dst,
		       struct scatterlist *src, unsigned int nbytes)
{
	struct serpent_lrw_ctx *ctx = crypto_blkcipher_ctx(desc->tfm);
	be128 buf[SERPENT_PARALLEL_BLOCKS];
	struct crypt_priv crypt_ctx = {
		.ctx = &ctx->serpent_ctx,
		.fpu_enabled = false,
	};
	struct lrw_crypt_req req = {
		.tbuf = buf,
		.tbuflen = sizeof(buf),

		.table_ctx = &ctx->lrw_table,
		.crypt_ctx = &crypt_ctx,
		.crypt_fn = decrypt_callback,
	};
	int ret;

	desc->flags &= ~CRYPTO_TFM_REQ_MAY_SLEEP;
	ret = lrw_crypt(desc, dst, src, nbytes, &req);
	serpent_fpu_end(crypt_ctx.fpu_enabled);

	return ret;
}

static void lrw_exit_tfm(struct crypto_tfm *tfm)
{
	struct serpent_lrw_ctx *ctx = crypto_tfm_ctx(tfm);

	lrw_free_table(&ctx->lrw_table);
}

struct serpent_xts_ctx {
	struct serpent_ctx tweak_ctx;
	struct serpent_ctx crypt_ctx;
};

static int xts_serpent_setkey(struct crypto_tfm *tfm, const u8 *key,
			      unsigned int keylen)
{
	struct serpent_xts_ctx *ctx = crypto_tfm_ctx(tfm);
	u32 *flags = &tfm->crt_flags;
	int err;

	/* key consists of keys of equal size concatenated, therefore
	 * the length must be even
	 */
	if (keylen % 2) {
		*flags |= CRYPTO_TFM_RES_BAD_KEY_LEN;
		return -EINVAL;
	}

	/* first half of xts-key is for crypt */
	err = __serpent_setkey(&ctx->crypt_ctx, key, keylen / 2);
	if (err)
		return err;

	/* second half of xts-key is for tweak */
	return __serpent_setkey(&ctx->tweak_ctx, key + keylen / 2, keylen / 2);
}

static int xts_encrypt(struct blkcipher_desc *desc, struct scatterlist *dst,
		       struct scatterlist *src, unsigned int nbytes)
{
	struct serpent_xts_ctx *ctx = crypto_blkcipher_ctx(desc->tfm);
	be128 buf[SERPENT_PARALLEL_BLOCKS];
	struct crypt_priv crypt_ctx = {
		.ctx = &ctx->crypt_ctx,
		.fpu_enabled = false,
	};
	struct xts_crypt_req req = {
		.tbuf = buf,
		.tbuflen = sizeof(buf),

		.tweak_ctx = &ctx->tweak_ctx,
		.tweak_fn = XTS_TWEAK_CAST(__serpent_encrypt),
		.crypt_ctx = &crypt_ctx,
		.crypt_fn = encrypt_callback,
	};
	int ret;

	desc->flags &= ~CRYPTO_TFM_REQ_MAY_SLEEP;
	ret = xts_crypt(desc, dst, src, nbytes, &req);
	serpent_fpu_end(crypt_ctx.fpu_enabled);

	return ret;
}

static int xts_decrypt(struct blkcipher_desc *desc, struct scatterlist *dst,
		       struct scatterlist *src, unsigned int nbytes)
{
	struct serpent_xts_ctx *ctx = crypto_blkcipher_ctx(desc->tfm);
	be128 buf[SERPENT_PARALLEL_BLOCKS];
	struct crypt_priv crypt_ctx = {
		.ctx = &ctx->crypt_ctx,
		.fpu_enabled = false,
	};
	struct xts_crypt_req req = {
		.tbuf = buf,
		.tbuflen = sizeof(buf),

		.tweak_ctx = &ctx->tweak_ctx,
		.tweak_fn = XTS_TWEAK_CAST(__serpent_encrypt),
		.crypt_ctx = &crypt_ctx,
		.crypt_fn = decrypt_callback,
	};
	int ret;

	desc->flags &= ~CRYPTO_TFM_REQ_MAY_SLEEP;
	ret = xts_crypt(desc, dst, src, nbytes, &req);
	serpent_fpu_end(crypt_ctx.fpu_enabled);

	return ret;
}

static struct crypto_alg serpent_algs[10] = { {
	.cra_name		= "__ecb-serpent-avx",
	.cra_driver_name	= "__driver-ecb-serpent-avx",
	.cra_priority		= 0,
	.cra_flags		= CRYPTO_ALG_TYPE_BLKCIPHER,
	.cra_blocksize		= SERPENT_BLOCK_SIZE,
	.cra_ctxsize		= sizeof(struct serpent_ctx),
	.cra_alignmask		= 0,
	.cra_type		= &crypto_blkcipher_type,
	.cra_module		= THIS_MODULE,
	.cra_list		= LIST_HEAD_INIT(serpent_algs[0].cra_list),
	.cra_u = {
		.blkcipher = {
			.min_keysize	= SERPENT_MIN_KEY_SIZE,
			.max_keysize	= SERPENT_MAX_KEY_SIZE,
			.setkey		= serpent_setkey,
			.encrypt	= ecb_encrypt,
			.decrypt	= ecb_decrypt,
		},
	},
}, {
	.cra_name		= "__cbc-serpent-avx",
	.cra_driver_name	= "__driver-cbc-serpent-avx",
	.cra_priority		= 0,
	.cra_flags		= CRYPTO_ALG_TYPE_BLKCIPHER,
	.cra_blocksize		= SERPENT_BLOCK_SIZE,
	.cra_ctxsize		= sizeof(struct serpent_ctx),
	.cra_alignmask		= 0,
	.cra_type		= &crypto_blkcipher_type,
	.cra_module		= THIS_MODULE,
	.cra_list		= LIST_HEAD_INIT(serpent_algs[1].cra_list),
	.cra_u = {
		.blkcipher = {
			.min_keysize	= SERPENT_MIN_KEY_SIZE,
			.max_keysize	= SERPENT_MAX_KEY_SIZE,
			.setkey		= serpent_setkey,
			.encrypt	= cbc_encrypt,
			.decrypt	= cbc_decrypt,
		},
	},
}, {
	.cra_name		= "__ctr-serpent-avx",
	.cra_driver_name	= "__driver-ctr-serpent-avx",
	.cra_priority		= 0,
	.cra_flags		= CRYPTO_ALG_TYPE_BLKCIPHER,
	.cra_blocksize		= 1,
	.cra_ctxsize		= sizeof(struct serpent_ctx),
	.cra_alignmask		= 0,
	.cra_type		= &crypto_blkcipher_type,
	.cra_module		= THIS_MODULE,
	.cra_list		= LIST_HEAD_INIT(serpent_algs[2].cra_list),
	.cra_u = {
		.blkcipher = {
			.min_keysize	= SERPENT_MIN_KEY_SIZE,
			.max_keysize	= SERPENT_MAX_KEY_SIZE,
			.ivsize		= SERPENT_BLOCK_SIZE,
			.setkey		= serpent_setkey,
			.encrypt	= ctr_crypt,
			.decrypt	= ctr_crypt,
		},
	},
}, {
	.cra_name		= "__lrw-serpent-avx",
	.cra_driver_name	= "__driver-lrw-serpent-avx",
	.cra_priority		= 0,
	.cra_flags		= CRYPTO_ALG_TYPE_BLKCIPHER,
	.cra_blocksize		= SERPENT_BLOCK_SIZE,
	.cra_ctxsize		= sizeof(struct serpent_lrw_ctx),
	.cra_alignmask		= 0,
	.cra_type		= &crypto_blkcipher_type,
	.cra_module		= THIS_MODULE,
	.cra_list		= LIST_HEAD_INIT(serpent_algs[3].cra_list),
	.cra_exit		= lrw_exit_tfm,
	.cra_u = {
		.blkcipher = {
			.min_keysize	= SERPENT_MIN_KEY_SIZE +
					  SERPENT_BLOCK_SIZE,
			.max_keysize	= SERPENT_MAX_KEY_SIZE +
					  SERPENT_BLOCK_SIZE,
			.ivsize		= SERPENT_BLOCK_SIZE,
			.setkey		= lrw_serpent_setkey,
			.encrypt	= lrw_encrypt,
			.decrypt	= lrw_decrypt,
		},
	},
}, {
	.cra_name		= "__xts-serpent-avx",
	.cra_driver_name	= "__driver-xts-serpent-avx",
	.cra_priority		= 0,
	.cra_flags		= CRYPTO_ALG_TYPE_BLKCIPHER,
	.cra_blocksize		= SERPENT_BLOCK_SIZE,
	.cra_ctxsize		= sizeof(struct serpent_xts_ctx),
	.cra_alignmask		= 0,
	.cra_type		= &crypto_blkcipher_type,
	.cra_module		= THIS_MODULE,
	.cra_list		= LIST_HEAD_INIT(serpent_algs[4].cra_list),
	.cra_u = {
		.blkcipher = {
			.min_keysize	= SERPENT_MIN_KEY_SIZE * 2,
			.max_keysize	= SERPENT_MAX_KEY_SIZE * 2,
			.ivsize		= SERPENT_BLOCK_SIZE,
			.setkey		= xts_serpent_setkey,
			.encrypt	= xts_encrypt,
			.decrypt	= xts_decrypt,
		},
	},
}, {
	.cra_name		= "ecb(serpent)",
	.cra_driver_name	= "ecb-serpent-avx",
	.cra_priority		= 500,
	.cra_flags		= CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
	.cra_blocksize		= SERPENT_BLOCK_SIZE,
	.cra_ctxsize		= sizeof(struct async_helper_ctx),
	.cra_alignmask		= 0,
	.cra_type		= &crypto_ablkcipher_type,
	.cra_module		= THIS_MODULE,
	.cra_list		= LIST_HEAD_INIT(serpent_algs[5].cra_list),
	.cra_init		= ablk_init,
	.cra_exit		= ablk_exit,
	.cra_u = {
		.ablkcipher = {
			.min_keysize	= SERPENT_MIN_KEY_SIZE,
			.max_keysize	= SERPENT_MAX_KEY_SIZE,
			.setkey		= ablk_set_key,
			.encrypt	= ablk_encrypt,
			.decrypt	= ablk_decrypt,
		},
	},
}, {
	.cra_name		= "cbc(serpent)",
	.cra_driver_name	= "cbc-serpent-avx",
	.cra_priority		= 500,
	.cra_flags		= CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
	.cra_blocksize		= SERPENT_BLOCK_SIZE,
	.cra_ctxsize		= sizeof(struct async_helper_ctx),
	.cra_alignmask		= 0,
	.cra_type		= &crypto_ablkcipher_type,
	.cra_module		= THIS_MODULE,
	.cra_list		= LIST_HEAD_INIT(serpent_algs[6].cra_list),
	.cra_init		= ablk_init,
	.cra_exit		= ablk_exit,
	.cra_u = {
		.ablkcipher = {
			.min_keysize	= SERPENT_MIN_KEY_SIZE,
			.max_keysize	= SERPENT_MAX_KEY_SIZE,
			.ivsize		= SERPENT_BLOCK_SIZE,
			.setkey		= ablk_set_key,
			.encrypt	= __ablk_encrypt,
			.decrypt	= ablk_decrypt,
		},
	},
}, {
	.cra_name		= "ctr(serpent)",
	.cra_driver_name	= "ctr-serpent-avx",
	.cra_priority		= 500,
	.cra_flags		= CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
	.cra_blocksize		= 1,
	.cra_ctxsize		= sizeof(struct async_helper_ctx),
	.cra_alignmask		= 0,
	.cra_type		= &crypto_ablkcipher_type,
	.cra_module		= THIS_MODULE,
	.cra_list		= LIST_HEAD_INIT(serpent_algs[7].cra_list),
	.cra_init		= ablk_init,
	.cra_exit		= ablk_exit,
	.cra_u = {
		.ablkcipher = {
			.min_keysize	= SERPENT_MIN_KEY_SIZE,
			.max_keysize	= SERPENT_MAX_KEY_SIZE,
			.ivsize		= SERPENT_BLOCK_SIZE,
			.setkey		= ablk_set_key,
			.encrypt	= ablk_encrypt,
			.decrypt	= ablk_encrypt,
			.geniv		= "chainiv",
		},
	},
}, {
	.cra_name		= "lrw(serpent)",
	.cra_driver_name	= "lrw-serpent-avx",
	.cra_priority		= 500,
	.cra_flags		= CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
	.cra_blocksize		= SERPENT_BLOCK_SIZE,
	.cra_ctxsize		= sizeof(struct async_helper_ctx),
	.cra_alignmask		= 0,
	.cra_type		= &crypto_ablkcipher_type,
	.cra_module		= THIS_MODULE,
	.cra_list		= LIST_HEAD_INIT(serpent_algs[8].cra_list),
	.cra_init		= ablk_init,
	.cra_exit		= ablk_exit,
	.cra_u = {
		.ablkcipher = {
			.min_keysize	= SERPENT_MIN_KEY_SIZE +
					  SERPENT_BLOCK_SIZE,
			.max_keysize	= SERPENT_MAX_KEY_SIZE +
					  SERPENT_BLOCK_SIZE,
			.ivsize		= SERPENT_BLOCK_SIZE,
			.setkey		= ablk_set_key,
			.encrypt	= ablk_encrypt,
			.decrypt	= ablk_decrypt,
		},
	},
}, {
	.cra_name		= "xts(serpent)",
	.cra_driver_name	= "xts-serpent-avx",
	.cra_priority		= 500,
	.cra_flags		= CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
	.cra_blocksize		= SERPENT_BLOCK_SIZE,
	.cra_ctxsize		= sizeof(struct async_helper_ctx),
	.cra_alignmask		= 0,
	.cra_type		= &crypto_ablkcipher_type,
	.cra_module		= THIS_MODULE,
	.cra_list		= LIST_HEAD_INIT(serpent_algs[9].cra_list),
	.cra_init		= ablk_init,
	.cra_exit		= ablk_exit,
	.cra_u = {
		.ablkcipher = {
			.min_keysize	= SERPENT_MIN_KEY_SIZE * 2,
			.max_keysize	= SERPENT_MAX_KEY_SIZE * 2,
			.ivsize		= SERPENT_BLOCK_SIZE,
			.setkey		= ablk_set_key,
			.encrypt	= ablk_encrypt,
			.decrypt	= ablk_decrypt,
		},
	},
} };

static int __init serpent_init(void)
{
	u64 xcr0;

	if (!cpu_has_avx || !cpu_has_osxsave) {
		printk(KERN_INFO "AVX instructions are not detected.\n");
		return -ENODEV;
	}

	xcr0 = xgetbv(XCR_XFEATURE_ENABLED_MASK);
	if ((xcr0 & (XSTATE_SSE | XSTATE_YMM)) != (XSTATE_SSE | XSTATE_YMM)) {
		printk(KERN_INFO "AVX detected but unusable.\n");
		return -ENODEV;
	}

	return crypto_register_algs(serpent_algs, ARRAY_SIZE(serpent_algs));
}

static void __exit serpent_exit(void)
{
	crypto_unregister_algs(serpent_algs, ARRAY_SIZE(serpent_algs));
}

module_init(serpent_init);
module_exit(serpent_exit);

MODULE_DESCRIPTION("Serpent Cipher Algorithm, AVX optimized");
MODULE_LICENSE("GPL");
MODULE_ALIAS("serpent");
