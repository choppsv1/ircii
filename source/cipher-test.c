/*
 * Copyright (c) 2004-2014 Matthew R. Green
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "irc.h"
IRCII_RCSID("@(#)$eterna: cipher-test.c,v 1.12 2014/03/14 21:19:26 mrg Exp $");

#include "irccrypt.h"
#include "vars.h"
#include "ircaux.h"
#include "list.h"
#include "ctcp.h"
#include "output.h"
#include "newio.h"

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif /* HAVE_SYS_WAIT_H */

#ifdef HAVE_DEV_RANDOM
static	int	crypt_dev_random_byte(void);
#define GET_RANDOM_BYTE	crypt_dev_random_byte()
#else
/* gotta use the sucky one */
#define GET_RANDOM_BYTE	(random() & 255)
#endif

#include "aes.c"
#include "cast.c"

static	char	*progname;

static	void	usage(void);
static	void
usage(void)
{
	fprintf(stderr, "Usage: %s [aes|cast]\n", progname);
	exit(1);
}
	u_char crypt_me[] = "\
this is the thing to be encrypted.  oink!  we will now pass this string\
to the crypt function and then the decrypt function and make sure it\
comes out the same way!\
";

int
main(int argc, char *argv[])
{
	struct crypt_key k;
	u_char *s;
	int len, rv, r;

	progname = argv[0];

	if (argc != 2)
		usage();

	s = crypt_me;

	if (strcmp(argv[1], "cast") == 0)
	{
		k.key = "this is the cast encryption key that we are going to use for the test";
		k.type = CAST_STRING;
		k.crypt = cast_encrypt_str;
		k.decrypt = cast_decrypt_str;
		k.cookie = 0;
	}
	else if (strcmp(argv[1], "aes") == 0)
	{
		k.key = "this is the aes encryption key that we are going to use for the test";
		k.type = AES_STRING;
		k.crypt = aes_encrypt_str;
		k.decrypt = aes_decrypt_str;
		k.cookie = 0;
	}
	else
		usage();

	len = strlen(s);
	r = rv = (*k.crypt)(&k, &s, &len);
	if (rv < 0)
		printf("crypt rv = %d\n", rv);
	else
		printf("crypt worked\n");

	rv = strcmp(s, crypt_me);
	r |= rv == 0;
	printf("comparision %s\n", rv == 0 ? "succeeded" : "failed");

	rv = (*k.decrypt)(&k, &s, &len);
	r |= rv;
	if (rv < 0)
		printf("decrypt rv = %d\n", rv);
	else
		printf("decrypt worked\n");

	rv = strcmp(s, crypt_me);
	r |= rv;
	printf("comparision %s\n", rv == 0 ? "succeeded" : "failed");

	exit(r);
}

/* XXX stolen routines */

static int
crypt_dev_random_byte(void)
{
	static	int	devrndfd = -1;
	u_char	c;

	if (devrndfd == -1)
	{
		devrndfd = open(DEV_RANDOM_PATH, O_RDONLY);

		if (devrndfd == -1)
		{
			printf("--- HELP!!!!rypt_dev_random_byte: can not open %s: %s\n",
			    DEV_RANDOM_PATH, strerror(errno));
			printf("--- using random()\n");
			devrndfd = -2;
		}
	}
	if (devrndfd == -2)
		goto do_random_instead;

	if (read(devrndfd, &c, 1) != 1)
	{
		/* if we were just interrupted, don't bail on /dev/random */
		if (errno == EINTR)
		{
			printf("--- crypt_dev_random_byte: timeout, using random()\n");
			goto do_random_instead;
		}
		printf("--- HELP!  crypt_dev_random_byte: read of one byte on %s failed: %s\n",
		    DEV_RANDOM_PATH, strerror(errno));
		printf("--- using random()\n");
		devrndfd = -2;
		goto do_random_instead;
	}
	return ((int)c);

do_random_instead:
	return (random() & 255);
}

void	*
new_malloc(size_t size)
{
	void	*ptr;

	if ((ptr = malloc(size)) == NULL)
	{
		printf("-1 0\n");
		exit(1);
	}
	return (ptr);
}

/*
 * new_free:  Why do this?  Why not?  Saves me a bit of trouble here and
 * there 
 */
void
new_free(void *ptr)
{
	void	**iptr = ptr;

	if (*iptr)
	{
		free(*iptr);
		*iptr = 0;
	}
}
