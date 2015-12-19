/*
 * cast.c: CAST-128 bit encryption
 *
 * Written By Matthew Green.
 *
 * Copyright (c) 1998-2014 Matthew R. Green.
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

IRCII_RCSID_NAMED("@(#)$eterna: cast.c,v 2.47 2014/03/14 08:40:46 mrg Exp $", cast_rcsid);

static	int	cast_encrypt_str(crypt_key *, u_char **, size_t *);
static	int	cast_decrypt_str(crypt_key *, u_char **, size_t *);

/* pull in the sboxes */
#include "cast_sbox.h"

#include "assert.h"

/* our structured cast key: 32 subkeys, and do we do 12 or 16 rounds? */
typedef struct {
	uint32_t rk[32];		/* prepared key */
	int	full16;			/* do 12 our 16 rounds? */
	uint32_t enc_oldr, enc_oldl;	/* state for encryption */
	uint32_t dec_oldr, dec_oldl;	/* state for decryption */
} castkey;

static void cast_setkey(crypt_key *, size_t);
static void cast_encrypt(castkey *, u_char *, u_char *, int);
static void cast_decrypt(castkey *, u_char *, u_char *, int);
static void cast_clearstate_dec(castkey *k);
static void cast_clearstate_enc(castkey *k);

/* get different 8 bit parts of a 32 bit variable */
#define E0(x)	((u_char) (x >> 24))
#define E1(x)	((u_char)((x >> 16) & 255))
#define E2(x)	((u_char)((x >> 8)  & 255))
#define E3(x)	((u_char)((x)       & 255))

/* rotate left */
#define ROT(x, n) ( ((x)<<(n)) | ((x)>>(32-(n))) )

/* CAST-128 needs three rounding functions */
#define R1(l, r, i) do { \
	I = ROT((k)->rk[(i)] + (r), (k)->rk[(i) + 16]); \
	l ^= ((cast_S1[E0(I)] ^ cast_S2[E1(I)]) - cast_S3[E2(I)]) \
	     + cast_S4[E3(I)]; \
} while (0)

#define R2(l, r, i) do { \
	I = ROT((k)->rk[(i)] ^ (r), (k)->rk[(i) + 16]); \
	l ^= ((cast_S1[E0(I)] - cast_S2[E1(I)]) + cast_S3[E2(I)]) \
	     ^ cast_S4[E3(I)]; \
} while (0)

#define R3(l, r, i) do { \
	I = ROT((k)->rk[(i)] - (r), (k)->rk[(i) + 16]); \
	l ^= ((cast_S1[E0(I)] + cast_S2[E1(I)]) ^ cast_S3[E2(I)]) \
	     - cast_S4[E3(I)]; \
} while (0)

/* get 32 bits from the block, from the specified offset */
#define G32(s, o) \
	(((uint32_t)(s)[(o) + 0] << 24) | ((uint32_t)(s)[(o) + 1] << 16) | \
	 ((uint32_t)(s)[(o) + 2] << 8)  |  (uint32_t)(s)[(o) + 3])

/*
 * cast_encrypt:
 *	- converts 8 bytes of data from src to dest using key k.
 *	- note that we only do 12 rounds if we have a long enough
 *	  key (80 or more bits).
 */
static void
cast_encrypt(castkey *k, u_char *src, u_char *dest, int first)
{
	uint32_t I, l, r;

	/*
	 * if this is the first encryption, we only want to
	 * setup internal state
	 */
	if (first)
	{
		k->enc_oldl = G32(src, 0);
		k->enc_oldr = G32(src, 4);
		return;
	}

	/* 
	 * split src into left and right parts, xoring the previous
	 * cipherblock as we go
	 */
	l = G32(src, 0) ^ k->enc_oldl;
	r = G32(src, 4) ^ k->enc_oldr;

	/* do it */
	R1(l, r,  0);
	R2(r, l,  1);
	R3(l, r,  2);
	R1(r, l,  3);
	R2(l, r,  4);
	R3(r, l,  5);
	R1(l, r,  6);
	R2(r, l,  7);
	R3(l, r,  8);
	R1(r, l,  9);
	R2(l, r, 10);
	R3(r, l, 11);
	if (k->full16) {
		R1(l, r, 12);
		R2(r, l, 13);
		R3(l, r, 14);
		R1(r, l, 15);
	}
	
	/* now put the left and right parts back into dest */
	dest[0] = E0(r);
	dest[1] = E1(r);
	dest[2] = E2(r);
	dest[3] = E3(r);
	dest[4] = E0(l);
	dest[5] = E1(l);
	dest[6] = E2(l);
	dest[7] = E3(l);

	/* save the final cipherblock for the next block's encryption */ 
	k->enc_oldl = G32(dest, 0);
	k->enc_oldr = G32(dest, 4);

	/* and clean up our stack */
	I = l = r = 0;
}

/*
 * cast_decrypt:
 *	- unconverts 8 bytes of data from src to dest using key k
 *	- note that we only do 12 rounds if we have a long enough
 *	  key (80 or more bits).
 */
static void
cast_decrypt(castkey *k, u_char *src, u_char *dest, int first)
{
	uint32_t new_oldr, new_oldl;
	uint32_t I, r, l;

	/*
	 * if this is the first decryption, we only want to
	 * setup internal state
	 */
	if (first)
	{
		k->dec_oldl = G32(src, 0);
		k->dec_oldr = G32(src, 4);
		return;
	}
	new_oldl = G32(src, 0);
	new_oldr = G32(src, 4);

	/* split src into left and right parts */
	r = G32(src, 0);
	l = G32(src, 4);

	/* do it */
	if (k->full16) {
		R1(r, l, 15);
		R3(l, r, 14);
		R2(r, l, 13);
		R1(l, r, 12);
	}
	R3(r, l, 11);
	R2(l, r, 10);
	R1(r, l,  9);
	R3(l, r,  8);
	R2(r, l,  7);
	R1(l, r,  6);
	R3(r, l,  5);
	R2(l, r,  4);
	R1(r, l,  3);
	R3(l, r,  2);
	R2(r, l,  1);
	R1(l, r,  0);

	/* now put the left and right parts back into dest */
	dest[0] = E0(l) ^ E0(k->dec_oldl);
	dest[1] = E1(l) ^ E1(k->dec_oldl);
	dest[2] = E2(l) ^ E2(k->dec_oldl);
	dest[3] = E3(l) ^ E3(k->dec_oldl);
	dest[4] = E0(r) ^ E0(k->dec_oldr);
	dest[5] = E1(r) ^ E1(k->dec_oldr);
 	dest[6] = E2(r) ^ E2(k->dec_oldr);
	dest[7] = E3(r) ^ E3(k->dec_oldr);

	/* save the final cipherblock for the next block's encryption */ 
	k->dec_oldr = new_oldr;
	k->dec_oldl = new_oldl;

	/* and clean up our stack */
	I = l = r = 0;
	new_oldr = new_oldl = 0;
}

/*
 * Clear the encryption/decryption state.
 */
static void
cast_clearstate_enc(castkey *k)
{
	k->enc_oldr = k->enc_oldl = 0;
}

static void
cast_clearstate_dec(castkey *k)
{
	k->dec_oldr = k->dec_oldl = 0;
}

/*
 * cast_setkey:
 *	- fill in key from the raw bytes in key for length len.
 */
static void
cast_setkey(crypt_key *key, size_t len)
{
	castkey *k;
	uint32_t t[4], x[4], z[4];
	int i;

	memset(&t, 0, sizeof t);
	memset(&z, 0, sizeof z);

	if (key->cookie)
	{
		/*yell("cast_setkey: key-cookie not null; freeing.");*/
		new_free(&key->cookie);
	}
	key->cookie = k = new_malloc(sizeof *k);

	cast_clearstate_enc(k);
	cast_clearstate_dec(k);

	/* convert the key so we can use it ... */
	for (i = 0; i < 4; i++) {
		x[i] = 0;
		if ((i * 4 + 0) < len)
			x[i] = (uint32_t)key->key[i * 4 + 0] << 24;
		if ((i * 4 + 1) < len)
			x[i] |= (uint32_t)key->key[i * 4 + 1] << 16;
		if ((i * 4 + 2) < len)
			x[i] |= (uint32_t)key->key[i * 4 + 2] << 8;
		if ((i * 4 + 3) < len)
			x[i] |= (uint32_t)key->key[i * 4 + 3];
	}

	/* if the key length is not sufficient, only do 12 rounds */
	k->full16 = (len > 10 ? 1 : 0);
	
	/*
	 * generate our 32 subkeys (4 at a time, as we can).  used an
	 * idea from steve reid on how to collapse this code a little
	 * more than the fully expanded version .. (pity i found that
	 * later)
	 */
	for (i = 0; i < 32; i += 4) {
		switch (i & 4) {
		case 0:
			t[0] = z[0] = x[0] ^ cast_S5[E1(x[3])] ^ cast_S6[E3(x[3])] ^ cast_S7[E0(x[3])] ^ cast_S8[E2(x[3])] ^ cast_S7[E0(x[2])];
			t[1] = z[1] = x[2] ^ cast_S5[E0(z[0])] ^ cast_S6[E2(z[0])] ^ cast_S7[E1(z[0])] ^ cast_S8[E3(z[0])] ^ cast_S8[E2(x[2])];
			t[2] = z[2] = x[3] ^ cast_S5[E3(z[1])] ^ cast_S6[E2(z[1])] ^ cast_S7[E1(z[1])] ^ cast_S8[E0(z[1])] ^ cast_S5[E1(x[2])];
			t[3] = z[3] = x[1] ^ cast_S5[E2(z[2])] ^ cast_S6[E1(z[2])] ^ cast_S7[E3(z[2])] ^ cast_S8[E0(z[2])] ^ cast_S6[E3(x[2])];
			break;
		case 4:
			t[0] = x[0] = z[2] ^ cast_S5[E1(z[1])] ^ cast_S6[E3(z[1])] ^ cast_S7[E0(z[1])] ^ cast_S8[E2(z[1])] ^ cast_S7[E0(z[0])];
			t[1] = x[1] = z[0] ^ cast_S5[E0(x[0])] ^ cast_S6[E2(x[0])] ^ cast_S7[E1(x[0])] ^ cast_S8[E3(x[0])] ^ cast_S8[E2(z[0])];
			t[2] = x[2] = z[1] ^ cast_S5[E3(x[1])] ^ cast_S6[E2(x[1])] ^ cast_S7[E1(x[1])] ^ cast_S8[E0(x[1])] ^ cast_S5[E1(z[0])];
			t[3] = x[3] = z[3] ^ cast_S5[E2(x[2])] ^ cast_S6[E1(x[2])] ^ cast_S7[E3(x[2])] ^ cast_S8[E0(x[2])] ^ cast_S6[E3(z[0])];
			break;
		}
		switch (i & 12) {
		case 0:
		case 12:
			k->rk[i + 0] = cast_S5[E0(t[2])] ^ cast_S6[E1(t[2])] ^ cast_S7[E3(t[1])] ^ cast_S8[E2(t[1])];
			k->rk[i + 1] = cast_S5[E2(t[2])] ^ cast_S6[E3(t[2])] ^ cast_S7[E1(t[1])] ^ cast_S8[E0(t[1])];
			k->rk[i + 2] = cast_S5[E0(t[3])] ^ cast_S6[E1(t[3])] ^ cast_S7[E3(t[0])] ^ cast_S8[E2(t[0])];
			k->rk[i + 3] = cast_S5[E2(t[3])] ^ cast_S6[E3(t[3])] ^ cast_S7[E1(t[0])] ^ cast_S8[E0(t[0])];
			break;
		case 4:
		case 8:
			k->rk[i + 0] = cast_S5[E3(t[0])] ^ cast_S6[E2(t[0])] ^ cast_S7[E0(t[3])] ^ cast_S8[E1(t[3])];
			k->rk[i + 1] = cast_S5[E1(t[0])] ^ cast_S6[E0(t[0])] ^ cast_S7[E2(t[3])] ^ cast_S8[E3(t[3])];
			k->rk[i + 2] = cast_S5[E3(t[1])] ^ cast_S6[E2(t[1])] ^ cast_S7[E0(t[2])] ^ cast_S8[E1(t[2])];
			k->rk[i + 3] = cast_S5[E1(t[1])] ^ cast_S6[E0(t[1])] ^ cast_S7[E2(t[2])] ^ cast_S8[E3(t[2])];
			break;
		}
		switch (i & 12) {
		case 0:
			k->rk[i + 0] ^= cast_S5[E2(z[0])];
			k->rk[i + 1] ^= cast_S6[E2(z[1])];
			k->rk[i + 2] ^= cast_S7[E1(z[2])];
			k->rk[i + 3] ^= cast_S8[E0(z[3])];
			break;
		case 4:
			k->rk[i + 0] ^= cast_S5[E0(x[2])];
			k->rk[i + 1] ^= cast_S6[E1(x[3])];
			k->rk[i + 2] ^= cast_S7[E3(x[0])];
			k->rk[i + 3] ^= cast_S8[E3(x[1])];
			break;
		case 8:
			k->rk[i + 0] ^= cast_S5[E1(z[2])];
			k->rk[i + 1] ^= cast_S6[E0(z[3])];
			k->rk[i + 2] ^= cast_S7[E2(z[0])];
			k->rk[i + 3] ^= cast_S8[E2(z[1])];
			break;
		case 12:
			k->rk[i + 0] ^= cast_S5[E3(x[0])];
			k->rk[i + 1] ^= cast_S6[E3(x[1])];
			k->rk[i + 2] ^= cast_S7[E0(x[2])];
			k->rk[i + 3] ^= cast_S8[E1(x[3])];
			break;
		}
		if (i >= 16) {
			k->rk[i + 0] &= 31;
			k->rk[i + 1] &= 31;
			k->rk[i + 2] &= 31;
			k->rk[i + 3] &= 31;
		}
	}

	/* and clean up our stack */
	for (i = 0; i < 4; i++)
		t[i] = x[i] = z[i] = 0;
}

/*
 * we implement cyclic block chaining mode here, where each previous
 * encryption block (and a random initial vector sent with each message,
 * for the first block) is exclusived-ORed with the plaintext before
 * being encryptioned.  this avoids many problems.
 */

/*
 * and here are the functions we pass to the crypt module.
 *
 * XXX: we copy non-64-bit-with-trailing-nul sized data into a new
 * string, and fill the end with garbage, expecting clients to throw
 * away data after the nul.
 */
static	int
cast_encrypt_str(crypt_key *key, u_char **str, size_t *len)
{
	u_char	*s, *newstr;
	int	i;
	size_t	nlen;

	/*
	 * pad the string to 64bit block boundary.  we use the same
	 * trick of DES does, and put the number of pad bytes (not
	 * inclusive) there are.  eg, a 47 byte string will become
	 * a 48 byte string with a '0' in the final byte, where as
	 * a 48 byte string will become a 56 byte string, with a '7'
	 * in the final byte, with garbage from 49 -> 55.
	 *
	 * note we allocate 8 bytes for the IV and generate it here.
	 */
	nlen = (*len + 8 + 8) & ~7UL;
	newstr = new_malloc(nlen + 1);
	my_strcpy(newstr + 8, *str);
	/* XXX this can read upto 15 1 byte chunks.. lame */
	for (i = 0; i < 8; i++)
		newstr[i] = (u_char)GET_RANDOM_BYTE;
	for (i = *len + 8; i < nlen - 1; i++)
		newstr[i] = (u_char)GET_RANDOM_BYTE;
	newstr[nlen - 1] = nlen - *len - 1 - 8;
	newstr[nlen] = '\0';

	/*
	 * fill in str for our parent.  note that we don't free the
	 * old str as it is the property of our caller (and in the
	 * only caller, it is an automatic variable).
	 */
	*str = newstr;

	if (key->cookie == NULL)
		cast_setkey(key, my_strlen(key->key));

	/* encrypt each 64bit chunk */
	for (i = nlen, s = (u_char *)*str; i > 0; s += 8, i -= 8)
		cast_encrypt(key->cookie, s, s, i == nlen);

	cast_clearstate_enc(key->cookie);

	/* set this so that our caller knows it has changed size */
	*len = nlen;
	(*str)[nlen] = '\0';

	return (0);
}

static	int
cast_decrypt_str(crypt_key *key, u_char **str, size_t *len)
{
	u_char	*s;
	size_t	i;

	/*
	 * cast messages must be 8-byte aligned, so force this by
	 * ignoring anything beyond a full block.
	 */
	*len &= ~7UL;

	if (key->cookie == NULL)
		cast_setkey(key, my_strlen(key->key));

	for (i = *len, s = (u_char *)*str; i > 0; s += 8, i -= 8)
		cast_decrypt(key->cookie, s, s, i == *len);
	assert(i == 0);

	cast_clearstate_dec(key->cookie);

	/* find the final byte */
	i = (*str)[*len - 1];
	if (i > 7)
		i = 7;
	*len = *len - 1 - 8 - i;

	/* now remove the trash IV from the top */
	for (i = 0; i < *len; i++)
		(*str)[i] = (*str)[i+8];
	/* fill in our nul byte from the final byte of the data */
	(*str)[i] = 0;

	return (0);
}
