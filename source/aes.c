/*
 * aes.c: interface to AES cipher
 *
 * Written By Matthew Green
 *
 * Copyright (c) 2000-2014 Matthew R. Green.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* XXX THIS DOES NOT WORK XXX */

IRCII_RCSID_NAMED("@(#)$eterna: aes.c,v 1.8 2014/03/14 01:57:15 mrg Exp $", aes_rcsid);

#include "rijndael-api-ref.h"

#define RJBITS	128
#define RJLEN	RJBITS / 4

typedef struct {
	rijndael_keyInstance k0;
	rijndael_keyInstance k1;
	rijndael_cipherInstance c;
} aeskey;

static	int	aes_decrypt_str(crypt_key *, u_char **, size_t *);
static	int	aes_encrypt_str(crypt_key *, u_char **, size_t *);

static	int	aes_setkey(crypt_key *, size_t, u_char *);

static	void	makehex(u_char *, u_char *, size_t, u_int);
#define	MAKEHEX_ALPHA	0x1

static	void
makehex(u_char *s, u_char *d, size_t len, u_int flags)
{

	for (; len > 0; len -= ((flags & MAKEHEX_ALPHA) ? 1 : 2), d++)
		if ((flags & MAKEHEX_ALPHA) == 0)
			snprintf(d++, len, "%02x", *s++);
		else
			snprintf(d, len, "%01x", *s++ & 0xf);
}

static	int
aes_setkey(crypt_key *key, size_t len, u_char *IV)
{
	aeskey	*k;
	u_char keypad[RJLEN + 1];
	u_char keymat[RJLEN + 1];
	u_char IVmat[RJLEN + 1];
	int rv;

	if (key->cookie)
	{
		/*yell("aes_setkey: key-cookie not null; freeing.");*/
		new_free(&key->cookie);
	}
	key->cookie = k = new_malloc(sizeof *k);

	if (len > sizeof(keypad))
		len = sizeof(keypad);
	memcpy(keypad, key->key, len);
	if (len != sizeof(keypad))
		memset(keypad, 0, sizeof(keypad) - len);
	makehex(keypad, keymat, RJLEN, MAKEHEX_ALPHA);
	keymat[sizeof(keymat) - 1] = '\0';

	makehex(IV, IVmat, RJLEN, 0);
	memset(&k->c, 0, sizeof(k->c));
	rv = rijndael_cipherInit(&k->c, MODE_CBC, IVmat);
	if (rv < 0)
		return -1;
	k->c.blockLen = RJBITS;

	rv = rijndael_makeKey(&k->k0, DIR_DECRYPT, RJBITS, keymat);
	if (rv < 0)
		return -2;

	rv = rijndael_makeKey(&k->k1, DIR_ENCRYPT, RJBITS, keymat);
	if (rv < 0)
		return -3;
	return 0;
}

static	int
aes_decrypt_str(crypt_key *key, u_char **str, size_t len)
{
	aeskey	*k;
	u_char *s = *str;
	int adj;
	int rv;

	if (key->cookie == NULL)
	{
		rv = aes_setkey(key, my_strlen(key->key), s);
		if (rv < 0)
			goto bad;
		adj = RJLEN / 2;
	}
	else
		adj = 0;
	k = key->cookie;

	rv = rijndael_blockDecrypt(&k->c, &k->k0, s + adj, *len * 8, s);
	if (rv < 0)
		goto bad;

	return (0);
bad:
	new_free(&key->cookie);
	*s = '\0';
	return (rv);
}

static int
aes_encrypt_str(crypt_key *key, u_char **str, size_t len)
{
	aeskey	*k;
	int rv, i;
	u_char IV[RJLEN + 1];
	u_char *s, *d;

	if (key->cookie == NULL)
	{
		for (i = 0, s = IV; i < RJLEN / 2; i++)
			s[i] = (u_char)GET_RANDOM_BYTE;

		rv = aes_setkey(key, my_strlen(key->key), s);
		if (rv < 0)
			goto bad;

		s = *str;
		*str = new_malloc(*len + (RJLEN / 2) + 1);
		k = key->cookie;
		
		memcpy(*str, k->c.IV, RJLEN / 2);
		d = *str + RJLEN;
	}
	else
	{
		d = s = *str;
		k = key->cookie;
	}

	rv = rijndael_blockEncrypt(&k->c, &k->k1, s, *len * 8, d);
	if (s != d)
		*len += (RJLEN / 2);

/*yell("--- after encrypt, rv = %d, d == `%s' *len = %d *str = `%s'", rv, d, *len, *str);*/
	if (rv < 0)
		goto bad;

	return (0);
bad:
	new_free(&key->cookie);
	**str = '\0';
	return (rv);
}
