/*
 * irccrypt.h: header for crypt.c 
 *
 * Written By Michael Sandrof
 *
 * Copyright (c) 1990 Michael Sandrof.
 * Copyright (c) 1991, 1992 Troy Rollo.
 * Copyright (c) 1992-2014 Matthew R. Green.
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
 *
 * @(#)$eterna: irccrypt.h,v 1.2 2014/08/14 01:51:25 mrg Exp $
 */

#ifndef irc__crypt_h_
#define irc__crypt_h_

#define	CTCP_SHUTUP	0
#define	CTCP_VERBOSE	1
#define	CTCP_NOREPLY	2

/*
 * crypt interface
 */

typedef struct crypt_key crypt_key;

typedef int (*CryptFunc)(crypt_key *, u_char **, size_t *);

/* Keep this in here so cipher-test.c can use it */
struct crypt_key
{
	u_char		*key;
	u_char		*type;
	CryptFunc	crypt;
	CryptFunc	decrypt;
	void		*cookie;	/* cipher dependant cookie, will be freed */
};

/*
 * function interfaces we have
 */
u_char		*crypt_msg(u_char *, crypt_key *, int);
void		encrypt_cmd(u_char *, u_char *, u_char *);
crypt_key	*is_crypted(u_char *);

/*
 * the ciphers we support
 */
#define CAST_STRING	UP("CAST128ED-CBC")
#define AES_STRING	UP("AES256-CBC")

#define DEFAULT_CRYPTER cast_encrypt_str
#define DEFAULT_DECRYPTER cast_decrypt_str
#define DEFAULT_CRYPTYPE CAST_STRING

/* Make this less than the trasmittable buffer */
#define CRYPT_BUFFER_SIZE (IRCD_BUFFER_SIZE - 50)

#endif /* irc__crypt_h_ */
