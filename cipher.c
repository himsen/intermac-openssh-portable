/* $OpenBSD: cipher.c,v 1.102 2016/08/03 05:41:57 djm Exp $ */
/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 *
 *
 * Copyright (c) 1999 Niels Provos.  All rights reserved.
 * Copyright (c) 1999, 2000 Markus Friedl.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"

#include <sys/types.h>

#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "cipher.h"
#include "misc.h"
#include "sshbuf.h"
#include "ssherr.h"
#include "digest.h"

#include "openbsd-compat/openssl-compat.h"

#ifdef WITH_SSH1
extern const EVP_CIPHER *evp_ssh1_bf(void);
extern const EVP_CIPHER *evp_ssh1_3des(void);
extern int ssh1_3des_iv(EVP_CIPHER_CTX *, int, u_char *, int);
#endif

struct sshcipher_ctx {
	int	plaintext;
	int	encrypt;
	EVP_CIPHER_CTX *evp;
	struct chachapoly_ctx cp_ctx; /* XXX union with evp? */
	struct aesctr_ctx ac_ctx; /* XXX union with evp? */
	struct intermac_ctx *im_ctx; /* IM EXTENSION */
	const struct sshcipher *cipher;
};

struct sshcipher {
	char	*name;
	int	number;		/* for ssh1 only */
	u_int	block_size;
	u_int	key_len;
	u_int	iv_len;		/* defaults to block_size */
	u_int	auth_len;
	u_int	discard_len;
	u_int	flags;
#define CFLAG_CBC		(1<<0)
#define CFLAG_CHACHAPOLY	(1<<1)
#define CFLAG_AESCTR		(1<<2)
#define CFLAG_NONE		(1<<3)
#define CFLAG_INTERMAC	(1<<4) /* IM EXTENSION */
#ifdef WITH_OPENSSL
	const EVP_CIPHER	*(*evptype)(void);
#else
	void	*ignored;
#endif
};

static const struct sshcipher ciphers[] = {
#ifdef WITH_SSH1
	{ "des",	SSH_CIPHER_DES, 8, 8, 0, 0, 0, 1, EVP_des_cbc },
	{ "3des",	SSH_CIPHER_3DES, 8, 16, 0, 0, 0, 1, evp_ssh1_3des },
# ifndef OPENSSL_NO_BF
	{ "blowfish",	SSH_CIPHER_BLOWFISH, 8, 32, 0, 0, 0, 1, evp_ssh1_bf },
# endif /* OPENSSL_NO_BF */
#endif /* WITH_SSH1 */
#ifdef WITH_OPENSSL
	{ "none",	SSH_CIPHER_NONE, 8, 0, 0, 0, 0, 0, EVP_enc_null },
	{ "3des-cbc",	SSH_CIPHER_SSH2, 8, 24, 0, 0, 0, 1, EVP_des_ede3_cbc },
# ifndef OPENSSL_NO_BF
	{ "blowfish-cbc",
			SSH_CIPHER_SSH2, 8, 16, 0, 0, 0, 1, EVP_bf_cbc },
# endif /* OPENSSL_NO_BF */
# ifndef OPENSSL_NO_CAST
	{ "cast128-cbc",
			SSH_CIPHER_SSH2, 8, 16, 0, 0, 0, 1, EVP_cast5_cbc },
# endif /* OPENSSL_NO_CAST */
# ifndef OPENSSL_NO_RC4
	{ "arcfour",	SSH_CIPHER_SSH2, 8, 16, 0, 0, 0, 0, EVP_rc4 },
	{ "arcfour128",	SSH_CIPHER_SSH2, 8, 16, 0, 0, 1536, 0, EVP_rc4 },
	{ "arcfour256",	SSH_CIPHER_SSH2, 8, 32, 0, 0, 1536, 0, EVP_rc4 },
# endif /* OPENSSL_NO_RC4 */
	{ "aes128-cbc",	SSH_CIPHER_SSH2, 16, 16, 0, 0, 0, 1, EVP_aes_128_cbc },
	{ "aes192-cbc",	SSH_CIPHER_SSH2, 16, 24, 0, 0, 0, 1, EVP_aes_192_cbc },
	{ "aes256-cbc",	SSH_CIPHER_SSH2, 16, 32, 0, 0, 0, 1, EVP_aes_256_cbc },
	{ "rijndael-cbc@lysator.liu.se",
			SSH_CIPHER_SSH2, 16, 32, 0, 0, 0, 1, EVP_aes_256_cbc },
	{ "aes128-ctr",	SSH_CIPHER_SSH2, 16, 16, 0, 0, 0, 0, EVP_aes_128_ctr },
	{ "aes192-ctr",	SSH_CIPHER_SSH2, 16, 24, 0, 0, 0, 0, EVP_aes_192_ctr },
	{ "aes256-ctr",	SSH_CIPHER_SSH2, 16, 32, 0, 0, 0, 0, EVP_aes_256_ctr },
# ifdef OPENSSL_HAVE_EVPGCM
	{ "aes128-gcm@openssh.com",
			SSH_CIPHER_SSH2, 16, 16, 12, 16, 0, 0, EVP_aes_128_gcm },
	{ "aes256-gcm@openssh.com",
			SSH_CIPHER_SSH2, 16, 32, 12, 16, 0, 0, EVP_aes_256_gcm },
	{"im-aes128-gcm-127",
			SSH_CIPHER_SSH2, 127, 16, 0, 1, 0, CFLAG_INTERMAC, NULL}, /* IM EXTENSION block_size used as chunk length */
	{"im-chacha-poly-127",
			SSH_CIPHER_SSH2, 127, 16, 0, 1, 0, CFLAG_INTERMAC, NULL}, /* IM EXTENSION block_size used as chunk length */
	{"im-aes128-gcm-128",
			SSH_CIPHER_SSH2, 128, 16, 0, 1, 0, CFLAG_INTERMAC, NULL}, /* IM EXTENSION */
	{"im-chacha-poly-128",
			SSH_CIPHER_SSH2, 128, 16, 0, 1, 0, CFLAG_INTERMAC, NULL}, /* IM EXTENSION */
	{"im-aes128-gcm-255",
			SSH_CIPHER_SSH2, 255, 16, 0, 1, 0, CFLAG_INTERMAC, NULL}, /* IM EXTENSION */
	{"im-chacha-poly-255",
			SSH_CIPHER_SSH2, 255, 16, 0, 1, 0, CFLAG_INTERMAC, NULL}, /* IM EXTENSION */
	{"im-aes128-gcm-256",
			SSH_CIPHER_SSH2, 256, 16, 0, 1, 0, CFLAG_INTERMAC, NULL}, /* IM EXTENSION */
	{"im-chacha-poly-256",
			SSH_CIPHER_SSH2, 256, 16, 0, 1, 0, CFLAG_INTERMAC, NULL}, /* IM EXTENSION */
	{"im-chacha-poly-511",
			SSH_CIPHER_SSH2, 511, 16, 0, 1, 0, CFLAG_INTERMAC, NULL}, /* IM EXTENSION */
	{"im-aes128-gcm-511",
			SSH_CIPHER_SSH2, 511, 16, 0, 1, 0, CFLAG_INTERMAC, NULL}, /* IM EXTENSION */
	{"im-chacha-poly-512",
			SSH_CIPHER_SSH2, 512, 16, 0, 1, 0, CFLAG_INTERMAC, NULL}, /* IM EXTENSION */
	{"im-aes128-gcm-512",
			SSH_CIPHER_SSH2, 512, 16, 0, 1, 0, CFLAG_INTERMAC, NULL}, /* IM EXTENSION */
	{"im-chacha-poly-1023",
			SSH_CIPHER_SSH2, 1023, 16, 0, 1, 0, CFLAG_INTERMAC, NULL}, /* IM EXTENSION */
	{"im-aes128-gcm-1023",
			SSH_CIPHER_SSH2, 1023, 16, 0, 1, 0, CFLAG_INTERMAC, NULL}, /* IM EXTENSION */
	{"im-chacha-poly-1024",
			SSH_CIPHER_SSH2, 1024, 16, 0, 1, 0, CFLAG_INTERMAC, NULL}, /* IM EXTENSION */
	{"im-aes128-gcm-1024",
			SSH_CIPHER_SSH2, 1024, 16, 0, 1, 0, CFLAG_INTERMAC, NULL}, /* IM EXTENSION */
	{"im-chacha-poly-2047",
			SSH_CIPHER_SSH2, 2047, 16, 0, 1, 0, CFLAG_INTERMAC, NULL}, /* IM EXTENSION */
	{"im-aes128-gcm-2047",
			SSH_CIPHER_SSH2, 2047, 16, 0, 1, 0, CFLAG_INTERMAC, NULL}, /* IM EXTENSION */
	{"im-chacha-poly-2048",
			SSH_CIPHER_SSH2, 2048, 16, 0, 1, 0, CFLAG_INTERMAC, NULL}, /* IM EXTENSION */
	{"im-aes128-gcm-2048",
			SSH_CIPHER_SSH2, 2048, 16, 0, 1, 0, CFLAG_INTERMAC, NULL}, /* IM EXTENSION */
	{"im-chacha-poly-4095",
			SSH_CIPHER_SSH2, 4095, 16, 0, 1, 0, CFLAG_INTERMAC, NULL}, /* IM EXTENSION */
	{"im-aes128-gcm-4095",
			SSH_CIPHER_SSH2, 4095, 16, 0, 1, 0, CFLAG_INTERMAC, NULL}, /* IM EXTENSION */
	{"im-chacha-poly-4096",
			SSH_CIPHER_SSH2, 4096, 16, 0, 1, 0, CFLAG_INTERMAC, NULL}, /* IM EXTENSION */
	{"im-aes128-gcm-4096",
			SSH_CIPHER_SSH2, 4096, 16, 0, 1, 0, CFLAG_INTERMAC, NULL}, /* IM EXTENSION */
	{"im-chacha-poly-8191",
			SSH_CIPHER_SSH2, 8191, 16, 0, 1, 0, CFLAG_INTERMAC, NULL}, /* IM EXTENSION */
	{"im-aes128-gcm-8191",
			SSH_CIPHER_SSH2, 8191, 16, 0, 1, 0, CFLAG_INTERMAC, NULL}, /* IM EXTENSION */
	{"im-chacha-poly-8192",
			SSH_CIPHER_SSH2, 8192, 16, 0, 1, 0, CFLAG_INTERMAC, NULL}, /* IM EXTENSION */
	{"im-aes128-gcm-8192",
			SSH_CIPHER_SSH2, 8192, 16, 0, 1, 0, CFLAG_INTERMAC, NULL}, /* IM EXTENSION */
# endif /* OPENSSL_HAVE_EVPGCM */
#else /* WITH_OPENSSL */
	{ "aes128-ctr",	SSH_CIPHER_SSH2, 16, 16, 0, 0, 0, CFLAG_AESCTR, NULL },
	{ "aes192-ctr",	SSH_CIPHER_SSH2, 16, 24, 0, 0, 0, CFLAG_AESCTR, NULL },
	{ "aes256-ctr",	SSH_CIPHER_SSH2, 16, 32, 0, 0, 0, CFLAG_AESCTR, NULL },
	{ "none",	SSH_CIPHER_NONE, 8, 0, 0, 0, 0, CFLAG_NONE, NULL },
#endif /* WITH_OPENSSL */
	{ "chacha20-poly1305@openssh.com",
			SSH_CIPHER_SSH2, 8, 64, 0, 16, 0, CFLAG_CHACHAPOLY, NULL },

	{ NULL,		SSH_CIPHER_INVALID, 0, 0, 0, 0, 0, 0, NULL }
};

/*--*/

/* CAPTURE */
char *_get_cipher(struct sshcipher_ctx *cc) {
	return cc->cipher->name;
}

/* IM EXTENSION */
int cipher_is_intermac(struct sshcipher_ctx *cc) {

	return (cc->cipher->flags & CFLAG_INTERMAC) != 0;
}

/* IM EXTENSION */
struct intermac_ctx * cipher_get_intermac_context(struct sshcipher_ctx *cc) {

	return cc->im_ctx;
}

/* IM EXTENSION */
int cipher_im_block_size(struct sshcipher_ctx *cc) {

	u_char *name = cipher_im_get_name(cc->cipher->name);

	if (name == NULL) {
		return SSH_ERR_INTERNAL_ERROR;
	}

	u_int name_length = strlen(name);

	if (strlen("im-aes128-gcm") == name_length &&
		memcmp("im-aes128-gcm", name, name_length)) {
		return 16;
	}
	else if (strlen("im-chacha-poly") == name_length &&
				memcmp("im-chacha-poly", name, name_length)) {
		return 64;
	}

	return SSH_ERR_INTERNAL_ERROR;
}

/* 
 * IM EXTENSION
 * Is there an easier way to do this?
 */
char* cipher_im_get_name(char* cipher_name) {

	u_int cp_len = strlen("im-chacha-poly");
	u_int gcm_len = strlen("im-aes128-gcm");
	u_int cipher_name_length = strlen(cipher_name);

	if (cipher_name_length >= cp_len) {
		if (memcmp("im-chacha-poly", cipher_name, cp_len) == 0) {
			return "im-chacha-poly";
		}
	}
	
	if (cipher_name_length >= gcm_len) {
		if (memcmp("im-aes128-gcm", cipher_name, gcm_len) == 0){
			return "im-aes128-gcm";
		}
	}

	return NULL;
}

/* Returns a comma-separated list of supported ciphers. */
char *
cipher_alg_list(char sep, int auth_only)
{
	char *tmp, *ret = NULL;
	size_t nlen, rlen = 0;
	const struct sshcipher *c;

	for (c = ciphers; c->name != NULL; c++) {
		if (c->number != SSH_CIPHER_SSH2)
			continue;
		if (auth_only && c->auth_len == 0)
			continue;
		if (ret != NULL)
			ret[rlen++] = sep;
		nlen = strlen(c->name);
		if ((tmp = realloc(ret, rlen + nlen + 2)) == NULL) {
			free(ret);
			return NULL;
		}
		ret = tmp;
		memcpy(ret + rlen, c->name, nlen + 1);
		rlen += nlen;
	}
	return ret;
}

u_int
cipher_blocksize(const struct sshcipher *c)
{
	return (c->block_size);
}

u_int
cipher_keylen(const struct sshcipher *c)
{
	return (c->key_len);
}

u_int
cipher_seclen(const struct sshcipher *c)
{
	if (strcmp("3des-cbc", c->name) == 0)
		return 14;
	return cipher_keylen(c);
}

u_int       
cipher_authlen(const struct sshcipher *c)
{
	return (c->auth_len);
}

u_int
cipher_ivlen(const struct sshcipher *c)
{
	/*
	 * Default is cipher block size, except for chacha20+poly1305 that
	 * needs no IV. XXX make iv_len == -1 default?
	 */
	return (c->iv_len != 0 || (c->flags & CFLAG_CHACHAPOLY) != 0 || (c->flags & CFLAG_INTERMAC) != 0) ?
	    c->iv_len : c->block_size; /* IM EXTENSION */
}

u_int
cipher_get_number(const struct sshcipher *c)
{
	return (c->number);
}

u_int
cipher_is_cbc(const struct sshcipher *c)
{
	return (c->flags & CFLAG_CBC) != 0;
}

u_int
cipher_ctx_is_plaintext(struct sshcipher_ctx *cc)
{
	return cc->plaintext;
}

u_int
cipher_ctx_get_number(struct sshcipher_ctx *cc)
{
	return cc->cipher->number;
}

u_int
cipher_mask_ssh1(int client)
{
	u_int mask = 0;
	mask |= 1 << SSH_CIPHER_3DES;		/* Mandatory */
	mask |= 1 << SSH_CIPHER_BLOWFISH;
	if (client) {
		mask |= 1 << SSH_CIPHER_DES;
	}
	return mask;
}

const struct sshcipher *
cipher_by_name(const char *name)
{
	const struct sshcipher *c;
	for (c = ciphers; c->name != NULL; c++)
		if (strcmp(c->name, name) == 0)
			return c;
	return NULL;
}

const struct sshcipher *
cipher_by_number(int id)
{
	const struct sshcipher *c;
	for (c = ciphers; c->name != NULL; c++)
		if (c->number == id)
			return c;
	return NULL;
}

#define	CIPHER_SEP	","
int
ciphers_valid(const char *names)
{
	const struct sshcipher *c;
	char *cipher_list, *cp;
	char *p;

	if (names == NULL || strcmp(names, "") == 0)
		return 0;
	if ((cipher_list = cp = strdup(names)) == NULL)
		return 0;
	for ((p = strsep(&cp, CIPHER_SEP)); p && *p != '\0';
	    (p = strsep(&cp, CIPHER_SEP))) {
		c = cipher_by_name(p);
		if (c == NULL || c->number != SSH_CIPHER_SSH2) {
			free(cipher_list);
			return 0;
		}
	}
	free(cipher_list);
	return 1;
}

/*
 * Parses the name of the cipher.  Returns the number of the corresponding
 * cipher, or -1 on error.
 */

int
cipher_number(const char *name)
{
	const struct sshcipher *c;
	if (name == NULL)
		return -1;
	for (c = ciphers; c->name != NULL; c++)
		if (strcasecmp(c->name, name) == 0)
			return c->number;
	return -1;
}

char *
cipher_name(int id)
{
	const struct sshcipher *c = cipher_by_number(id);
	return (c==NULL) ? "<unknown>" : c->name;
}

const char *
cipher_warning_message(const struct sshcipher_ctx *cc)
{
	if (cc == NULL || cc->cipher == NULL)
		return NULL;
	if (cc->cipher->number == SSH_CIPHER_DES)
		return "use of DES is strongly discouraged due to "
		    "cryptographic weaknesses";
	return NULL;
}

int
cipher_init(struct sshcipher_ctx **ccp, const struct sshcipher *cipher,
    const u_char *key, u_int keylen, const u_char *iv, u_int ivlen,
    int do_encrypt)
{
	struct sshcipher_ctx *cc = NULL;
	int ret = SSH_ERR_INTERNAL_ERROR;
	struct intermac_ctx * im_ctx = NULL;
	char* cipher_name = NULL;
#ifdef WITH_OPENSSL
	const EVP_CIPHER *type;
	int klen;
	u_char *junk, *discard;
#endif
	*ccp = NULL;
	if ((cc = calloc(sizeof(*cc), 1)) == NULL)
		return SSH_ERR_ALLOC_FAIL;

	if (cipher->number == SSH_CIPHER_DES) {
		if (keylen > 8)
			keylen = 8;
	}

	cc->plaintext = (cipher->number == SSH_CIPHER_NONE);
	cc->encrypt = do_encrypt;

	if (keylen < cipher->key_len ||
	    (iv != NULL && ivlen < cipher_ivlen(cipher))) {
		ret = SSH_ERR_INVALID_ARGUMENT;
		goto out;
	}

	cc->cipher = cipher;

	if ((cc->cipher->flags & CFLAG_INTERMAC) != 0) { /* IM EXTENSION */

		cipher_name = cipher_im_get_name(cipher->name);

		if (cipher_name == NULL) {
			ret = SSH_ERR_INTERNAL_ERROR;
			goto out;
		}

		/* cipher->block_size stores the chunk length for the chosen InterMAC cipher suite */
		if (im_initialise(&im_ctx, key, cipher->block_size, cipher_name, do_encrypt) != 0) {
			ret = SSH_ERR_INTERNAL_ERROR;
			goto out;
		}

		cc->im_ctx = im_ctx;

		ret = 0;
		goto out;
	}	
	if ((cc->cipher->flags & CFLAG_CHACHAPOLY) != 0) {
		ret = chachapoly_init(&cc->cp_ctx, key, keylen);
		goto out;
	}
#ifndef WITH_OPENSSL
	if ((cc->cipher->flags & CFLAG_AESCTR) != 0) {
		aesctr_keysetup(&cc->ac_ctx, key, 8 * keylen, 8 * ivlen);
		aesctr_ivsetup(&cc->ac_ctx, iv);
		ret = 0;
		goto out;
	}
	if ((cc->cipher->flags & CFLAG_NONE) != 0) {
		ret = 0;
		goto out;
	}
	ret = SSH_ERR_INVALID_ARGUMENT;
	goto out;
#else /* WITH_OPENSSL */
	type = (*cipher->evptype)();
	if ((cc->evp = EVP_CIPHER_CTX_new()) == NULL) {
		ret = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if (EVP_CipherInit(cc->evp, type, NULL, (u_char *)iv,
	    (do_encrypt == CIPHER_ENCRYPT)) == 0) {
		ret = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}
	if (cipher_authlen(cipher) &&
	    !EVP_CIPHER_CTX_ctrl(cc->evp, EVP_CTRL_GCM_SET_IV_FIXED,
	    -1, (u_char *)iv)) {
		ret = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}
	klen = EVP_CIPHER_CTX_key_length(cc->evp);
	if (klen > 0 && keylen != (u_int)klen) {
		if (EVP_CIPHER_CTX_set_key_length(cc->evp, keylen) == 0) {
			ret = SSH_ERR_LIBCRYPTO_ERROR;
			goto out;
		}
	}
	if (EVP_CipherInit(cc->evp, NULL, (u_char *)key, NULL, -1) == 0) {
		ret = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}

	if (cipher->discard_len > 0) {
		if ((junk = malloc(cipher->discard_len)) == NULL ||
		    (discard = malloc(cipher->discard_len)) == NULL) {
			free(junk);
			ret = SSH_ERR_ALLOC_FAIL;
			goto out;
		}
		ret = EVP_Cipher(cc->evp, discard, junk, cipher->discard_len);
		explicit_bzero(discard, cipher->discard_len);
		free(junk);
		free(discard);
		if (ret != 1) {
			ret = SSH_ERR_LIBCRYPTO_ERROR;
			goto out;
		}
	}
	ret = 0;
#endif /* WITH_OPENSSL */
 out:
	if (ret == 0) {
		/* success */
		*ccp = cc;
	} else {
		if (cc != NULL) {
#ifdef WITH_OPENSSL
			if (cc->evp != NULL)
				EVP_CIPHER_CTX_free(cc->evp);
#endif /* WITH_OPENSSL */
			if (im_ctx != NULL) /* IM EXTENSION */
				im_cleanup(im_ctx);
			explicit_bzero(cc, sizeof(*cc));
			free(cc);
		}
	}
	return ret;
}

/*
 * cipher_crypt() operates as following:
 * Copy 'aadlen' bytes (without en/decryption) from 'src' to 'dest'.
 * Theses bytes are treated as additional authenticated data for
 * authenticated encryption modes.
 * En/Decrypt 'len' bytes at offset 'aadlen' from 'src' to 'dest'.
 * Use 'authlen' bytes at offset 'len'+'aadlen' as the authentication tag.
 * This tag is written on encryption and verified on decryption.
 * Both 'aadlen' and 'authlen' can be set to 0.
 */
int
cipher_crypt(struct sshcipher_ctx *cc, u_int seqnr, u_char *dest,
   const u_char *src, u_int len, u_int aadlen, u_int authlen)
{
	if ((cc->cipher->flags & CFLAG_CHACHAPOLY) != 0) {
		return chachapoly_crypt(&cc->cp_ctx, seqnr, dest, src,
		    len, aadlen, authlen, cc->encrypt);
	}
#ifndef WITH_OPENSSL
	if ((cc->cipher->flags & CFLAG_AESCTR) != 0) {
		if (aadlen) 
			memcpy(dest, src, aadlen);
		aesctr_encrypt_bytes(&cc->ac_ctx, src + aadlen,
		    dest + aadlen, len);
		return 0;
	}
	if ((cc->cipher->flags & CFLAG_NONE) != 0) {
		memcpy(dest, src, aadlen + len);
		return 0;
	}
	return SSH_ERR_INVALID_ARGUMENT;
#else
	if (authlen) {
		u_char lastiv[1];

		if (authlen != cipher_authlen(cc->cipher))
			return SSH_ERR_INVALID_ARGUMENT;
		/* increment IV */
		if (!EVP_CIPHER_CTX_ctrl(cc->evp, EVP_CTRL_GCM_IV_GEN,
		    1, lastiv))
			return SSH_ERR_LIBCRYPTO_ERROR;
		/* set tag on decyption */
		if (!cc->encrypt &&
		    !EVP_CIPHER_CTX_ctrl(cc->evp, EVP_CTRL_GCM_SET_TAG,
		    authlen, (u_char *)src + aadlen + len))
			return SSH_ERR_LIBCRYPTO_ERROR;
	}
	if (aadlen) {
		if (authlen &&
		    EVP_Cipher(cc->evp, NULL, (u_char *)src, aadlen) < 0)
			return SSH_ERR_LIBCRYPTO_ERROR;
		memcpy(dest, src, aadlen);
	}
	if (len % cc->cipher->block_size)
		return SSH_ERR_INVALID_ARGUMENT;
	if (EVP_Cipher(cc->evp, dest + aadlen, (u_char *)src + aadlen,
	    len) < 0)
		return SSH_ERR_LIBCRYPTO_ERROR;
	if (authlen) {
		/* compute tag (on encrypt) or verify tag (on decrypt) */
		if (EVP_Cipher(cc->evp, NULL, NULL, 0) < 0)
			return cc->encrypt ?
			    SSH_ERR_LIBCRYPTO_ERROR : SSH_ERR_MAC_INVALID;
		if (cc->encrypt &&
		    !EVP_CIPHER_CTX_ctrl(cc->evp, EVP_CTRL_GCM_GET_TAG,
		    authlen, dest + aadlen + len))
			return SSH_ERR_LIBCRYPTO_ERROR;
	}
	return 0;
#endif
}

/* Extract the packet length, including any decryption necessary beforehand */
int
cipher_get_length(struct sshcipher_ctx *cc, u_int *plenp, u_int seqnr,
    const u_char *cp, u_int len)
{
	if ((cc->cipher->flags & CFLAG_CHACHAPOLY) != 0)
		return chachapoly_get_length(&cc->cp_ctx, plenp, seqnr,
		    cp, len);
	if (len < 4)
		return SSH_ERR_MESSAGE_INCOMPLETE;
	*plenp = get_u32(cp);
	return 0;
}

void
cipher_free(struct sshcipher_ctx *cc)
{
	if (cc == NULL)
		return;
	if ((cc->cipher->flags & CFLAG_CHACHAPOLY) != 0)
		explicit_bzero(&cc->cp_ctx, sizeof(cc->cp_ctx));
	else if ((cc->cipher->flags & CFLAG_AESCTR) != 0)
		explicit_bzero(&cc->ac_ctx, sizeof(cc->ac_ctx));
#ifdef WITH_OPENSSL
	if ((cc->cipher->flags & CFLAG_INTERMAC) != 0) /* INTERMAC EXTENSION */
		im_cleanup(cc->im_ctx);
	if (cc->evp != NULL) {
		EVP_CIPHER_CTX_free(cc->evp);
		cc->evp = NULL;
	}
#endif
	explicit_bzero(cc, sizeof(*cc));
	free(cc);
}

/*
 * Selects the cipher, and keys if by computing the MD5 checksum of the
 * passphrase and using the resulting 16 bytes as the key.
 */
int
cipher_set_key_string(struct sshcipher_ctx **ccp,
    const struct sshcipher *cipher, const char *passphrase, int do_encrypt)
{
	u_char digest[16];
	int r = SSH_ERR_INTERNAL_ERROR;

	if ((r = ssh_digest_memory(SSH_DIGEST_MD5,
	    passphrase, strlen(passphrase),
	    digest, sizeof(digest))) != 0)
		goto out;

	r = cipher_init(ccp, cipher, digest, 16, NULL, 0, do_encrypt);
 out:
	explicit_bzero(digest, sizeof(digest));
	return r;
}

/*
 * Exports an IV from the sshcipher_ctx required to export the key
 * state back from the unprivileged child to the privileged parent
 * process.
 */
int
cipher_get_keyiv_len(const struct sshcipher_ctx *cc)
{
	const struct sshcipher *c = cc->cipher;
	int ivlen = 0;

	if (c->number == SSH_CIPHER_3DES)
		ivlen = 24;
	else if ((cc->cipher->flags & CFLAG_CHACHAPOLY) != 0)
		ivlen = 0;
	else if ((cc->cipher->flags & CFLAG_AESCTR) != 0)
		ivlen = sizeof(cc->ac_ctx.ctr);
#ifdef WITH_OPENSSL
	else if ((cc->cipher->flags & CFLAG_INTERMAC) != 0) /* IM EXTENSION */
		ivlen = 0;
	else
		ivlen = EVP_CIPHER_CTX_iv_length(cc->evp);
#endif /* WITH_OPENSSL */
	return (ivlen);
}

int
cipher_get_keyiv(struct sshcipher_ctx *cc, u_char *iv, u_int len)
{
	const struct sshcipher *c = cc->cipher;
#ifdef WITH_OPENSSL
	if ((cc->cipher->flags & CFLAG_INTERMAC) != 0) { /* IM EXTENSION */
		return 0;
	}

 	int evplen;
#endif

	if ((cc->cipher->flags & CFLAG_CHACHAPOLY) != 0) {
		if (len != 0)
			return SSH_ERR_INVALID_ARGUMENT;
		return 0;
	}
	if ((cc->cipher->flags & CFLAG_AESCTR) != 0) {
		if (len != sizeof(cc->ac_ctx.ctr))
			return SSH_ERR_INVALID_ARGUMENT;
		memcpy(iv, cc->ac_ctx.ctr, len);
		return 0;
	}
	if ((cc->cipher->flags & CFLAG_NONE) != 0)
		return 0;

	switch (c->number) {
#ifdef WITH_OPENSSL
	case SSH_CIPHER_SSH2:
	case SSH_CIPHER_DES:
	case SSH_CIPHER_BLOWFISH:
		evplen = EVP_CIPHER_CTX_iv_length(cc->evp);
		if (evplen == 0)
			return 0;
		else if (evplen < 0)
			return SSH_ERR_LIBCRYPTO_ERROR;
		if ((u_int)evplen != len)
			return SSH_ERR_INVALID_ARGUMENT;
#ifndef OPENSSL_HAVE_EVPCTR
		if (c->evptype == evp_aes_128_ctr)
			ssh_aes_ctr_iv(cc->evp, 0, iv, len);
		else
#endif
		if (cipher_authlen(c)) {
			if (!EVP_CIPHER_CTX_ctrl(cc->evp, EVP_CTRL_GCM_IV_GEN,
			   len, iv))
			       return SSH_ERR_LIBCRYPTO_ERROR;
		} else
			memcpy(iv, cc->evp->iv, len);
		break;
#endif
#ifdef WITH_SSH1
	case SSH_CIPHER_3DES:
		return ssh1_3des_iv(cc->evp, 0, iv, 24);
#endif
	default:
		return SSH_ERR_INVALID_ARGUMENT;
	}
	return 0;
}

int
cipher_set_keyiv(struct sshcipher_ctx *cc, const u_char *iv)
{
	const struct sshcipher *c = cc->cipher;
#ifdef WITH_OPENSSL
 	int evplen = 0;
#endif

	if ((cc->cipher->flags & CFLAG_CHACHAPOLY) != 0)
		return 0;
	if ((cc->cipher->flags & CFLAG_INTERMAC) != 0)
		return 0;
	if ((cc->cipher->flags & CFLAG_NONE) != 0)
		return 0;

	switch (c->number) {
#ifdef WITH_OPENSSL
	case SSH_CIPHER_SSH2:
	case SSH_CIPHER_DES:
	case SSH_CIPHER_BLOWFISH:
		evplen = EVP_CIPHER_CTX_iv_length(cc->evp);
		if (evplen <= 0)
			return SSH_ERR_LIBCRYPTO_ERROR;
#ifndef OPENSSL_HAVE_EVPCTR
		/* XXX iv arg is const, but ssh_aes_ctr_iv isn't */
		if (c->evptype == evp_aes_128_ctr)
			ssh_aes_ctr_iv(cc->evp, 1, (u_char *)iv, evplen);
		else
#endif
		if (cipher_authlen(c)) {
			/* XXX iv arg is const, but EVP_CIPHER_CTX_ctrl isn't */
			if (!EVP_CIPHER_CTX_ctrl(cc->evp,
			    EVP_CTRL_GCM_SET_IV_FIXED, -1, (void *)iv))
				return SSH_ERR_LIBCRYPTO_ERROR;
		} else
			memcpy(cc->evp->iv, iv, evplen);
		break;
#endif
#ifdef WITH_SSH1
	case SSH_CIPHER_3DES:
		return ssh1_3des_iv(cc->evp, 1, (u_char *)iv, 24);
#endif
	default:
		return SSH_ERR_INVALID_ARGUMENT;
	}
	return 0;
}

#ifdef WITH_OPENSSL
#define EVP_X_STATE(evp)	(evp)->cipher_data
#define EVP_X_STATE_LEN(evp)	(evp)->cipher->ctx_size
#endif

int
cipher_get_keycontext(const struct sshcipher_ctx *cc, u_char *dat)
{
#if defined(WITH_OPENSSL) && !defined(OPENSSL_NO_RC4)
	const struct sshcipher *c = cc->cipher;
	int plen = 0;

	if (c->evptype == EVP_rc4) {
		plen = EVP_X_STATE_LEN(cc->evp);
		if (dat == NULL)
			return (plen);
		memcpy(dat, EVP_X_STATE(cc->evp), plen);
	}
	return (plen);
#else
	return 0;
#endif
}

void
cipher_set_keycontext(struct sshcipher_ctx *cc, const u_char *dat)
{
#if defined(WITH_OPENSSL) && !defined(OPENSSL_NO_RC4)
	const struct sshcipher *c = cc->cipher;
	int plen;

	if (c->evptype == EVP_rc4) {
		plen = EVP_X_STATE_LEN(cc->evp);
		memcpy(EVP_X_STATE(cc->evp), dat, plen);
	}
#endif
}
