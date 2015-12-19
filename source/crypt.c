/*
 * crypt.c: handles some encryption of messages stuff. 
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
 */

#include "irc.h"
IRCII_RCSID("@(#)$eterna: crypt.c,v 1.82 2014/08/14 01:51:25 mrg Exp $");

#include "irccrypt.h"
#include "vars.h"
#include "ircaux.h"
#include "list.h"
#include "ctcp.h"
#include "output.h"
#include "newio.h"

#include <sys/wait.h>

static	void	add_to_crypt(u_char *, u_char *, CryptFunc, CryptFunc, u_char *);
static	int	remove_crypt(u_char *);
static	u_char	*do_crypt(u_char *, crypt_key *, int, u_char **);

#ifdef HAVE_DEV_RANDOM
static	int	crypt_dev_random_byte(void);
#define GET_RANDOM_BYTE	crypt_dev_random_byte()
#else
/* gotta use the sucky one */
#define GET_RANDOM_BYTE	(random() & 255)
#endif

#include "cast.c"
#if 0
#include "aes.c"
#endif

/*
 * Crypt: the crypt list structure,  consists of the nickname, and the
 * encryption key 
 */
typedef struct	CryptStru
{
	struct	CryptStru *next;
	u_char	*nick;
	crypt_key	*key;
}	Crypt;

/* crypt_list: the list of nicknames and encryption keys */
static	Crypt	*crypt_list = NULL;

/*
 * add_to_crypt: adds the nickname and key pair to the crypt_list.  If the
 * nickname is already in the list, then the key is changed the the supplied
 * key. 
 */
static	void
add_to_crypt(u_char *nick, u_char *keystr, CryptFunc enc, CryptFunc dec, u_char *type)
{
	Crypt	*new;

	if ((new = (Crypt *) remove_from_list((List **)(void *)&crypt_list, nick)) != NULL)
	{
		new_free(&(new->nick));
		memset(new->key->key, 0, my_strlen(new->key->key));		/* wipe it out */
		new_free(&(new->key->key));
		/* XXX destroy cookie first? */
		new_free(&(new->key->cookie));
		new_free(&(new->key));
		new_free(&new);
	}
	new = new_malloc(sizeof *new);
	new->key = new_malloc(sizeof(*new->key));
	new->nick = NULL;
	new->key->key = NULL;
	new->key->type = type;
	malloc_strcpy(&(new->nick), nick);
	malloc_strcpy(&(new->key->key), keystr);
	new->key->crypt = enc;
	new->key->decrypt = dec;
	new->key->cookie = NULL;
	add_to_list((List **)(void *)&crypt_list, (List *) new);
}

/*
 * remove_crypt: removes the given nickname from the crypt_list, returning 0
 * if successful, and 1 if not (because the nickname wasn't in the list) 
 */
static	int
remove_crypt(u_char *nick)
{
	Crypt	*tmp;

	if ((tmp = (Crypt *) list_lookup((List **)(void *)&crypt_list, nick, 0, REMOVE_FROM_LIST)) != NULL)
	{
		new_free(&tmp->nick);
		memset(tmp->key->key, 0, my_strlen(tmp->key->key));		/* wipe it out */
		new_free(&tmp->key->key);
		new_free(&tmp->key->cookie);
		new_free(&tmp->key);
		new_free(&tmp);
		return (0);
	}
	return (1);
}

/*
 * is_crypted: looks up nick in the crypt_list and returns the encryption key
 * if found in the list.  If not found in the crypt_list, null is returned. 
 */
crypt_key *
is_crypted(u_char *nick)
{
	Crypt	*tmp;

	if (!crypt_list)
		return NULL;
	if ((tmp = (Crypt *) list_lookup((List **)(void *)&crypt_list, nick, 0, 0)) != NULL)
		return (tmp->key);
	return NULL;
}

/*
 * encrypt_cmd: the ENCRYPT command.  Adds the given nickname and key to the
 * encrypt list, or removes it, or list the list, you know. 
 */
void
encrypt_cmd(u_char *command, u_char *args, u_char *subargs)
{
	/* XXX this is getting really, really gross */
	CryptFunc enc = DEFAULT_CRYPTER, dec = DEFAULT_DECRYPTER;
	u_char	*type = UP(DEFAULT_CRYPTYPE);
	u_char	*nick;
	int	showkeys = 0;

restart:
	if ((nick = next_arg(args, &args)) != NULL)
	{
		size_t len = my_strlen(nick);

		if (my_strnicmp(nick, UP("-showkeys"), len) == 0)
		{
			showkeys = 1;
			goto restart;
		}
		else if (my_strnicmp(nick, UP("-cast"), len) == 0)
		{
			enc = cast_encrypt_str;
			dec = cast_decrypt_str;
			type = UP(CAST_STRING);
			goto restart;
		}
#if 0
		else if (my_strnicmp(nick, UP("-aes"), len) == 0)
		{
			enc = aes_encrypt_str;
			dec = aes_decrypt_str;
			type = UP(AES_STRING);
			goto restart;
		}
#endif

		if (args && *args)
		{
			add_to_crypt(nick, args, enc, dec, type);
			say("%s added to the %s crypt", nick, type);
		}
		else
		{
			if (remove_crypt(nick))
				say("No such nickname in the crypt: %s", nick);
			else
				say("%s removed from the crypt", nick);
		}
	}
	else
	{
		if (crypt_list)
		{
			Crypt	*tmp;

			say("The crypt:");
			for (tmp = crypt_list; tmp; tmp = tmp->next)
				if (showkeys)
					put_it("%s with key \"%s\" type %s", tmp->nick, tmp->key->key, tmp->key->type);
				else
					put_it("%s type %s", tmp->nick, tmp->key->type);
		}
		else
			say("The crypt is empty");
	}
}

static	u_char	*
do_crypt(u_char *str, crypt_key *key, int flag, u_char **type)
{
	int	in[2],
		out[2];
	size_t	c;
	ssize_t	sc;
	u_char	lbuf[CRYPT_BUFFER_SIZE];
	u_char	*ptr = NULL,
		*crypt_program,
		*encrypt_program,
		*decrypt_program,
		*crypt_str;

	encrypt_program = get_string_var(ENCRYPT_PROGRAM_VAR);
	decrypt_program = get_string_var(DECRYPT_PROGRAM_VAR);
	if ((flag && encrypt_program) || (!flag && decrypt_program))
	{
#ifdef DAEMON_UID
		if (DAEMON_UID == getuid())
		{
			say("ENCRYPT_PROGRAM not available from daemon mode");
			return NULL;
		}
#endif /* DAEMON_ID */
		in[0] = in[1] = -1;
		out[0] = out[1] = -1;
		if (flag)
		{
			crypt_str = UP("encryption");
			crypt_program = encrypt_program;
		}
		else
		{
			crypt_str = UP("decryption");
			crypt_program = decrypt_program;
		}
		if (access(CP(crypt_program), X_OK))
		{
			say("Unable to execute %s program: %s", crypt_str, crypt_program);
			return (NULL);
		}
		c = my_strlen(str);
		if (!flag)
			ptr = ctcp_unquote_it(str, &c);
		else 
			malloc_strcpy(&ptr, str);
		if (pipe(in) || pipe(out))
		{
			say("Unable to start %s process: %s", crypt_str, strerror(errno));
			if (in[0] != -1)
			{
				new_close(in[0]);
				new_close(in[1]);
			}
			if (out[0] != -1)
			{
				new_close(out[0]);
				new_close(out[1]);
			}
		}
		switch (fork())
		{
		case -1:
			say("Unable to start %s process: %s", crypt_str, strerror(errno));
			return (NULL);
		case 0:
			MY_SIGNAL(SIGINT, (sigfunc *) SIG_IGN, 0);
			dup2(out[1], 1);
			dup2(in[0], 0);
			new_close(out[0]);
			new_close(out[1]);
			new_close(in[0]);
			new_close(in[1]);
			(void)setgid(getgid());
			(void)setuid(getuid());
			if (get_int_var(OLD_ENCRYPT_PROGRAM_VAR))
				execl(CP(crypt_program), CP(crypt_program), key->key, NULL);
			else
				execl(CP(crypt_program), CP(crypt_program), NULL);
			exit(0);
		default:
			new_close(out[1]);
			new_close(in[0]);
			if (get_int_var(OLD_ENCRYPT_PROGRAM_VAR) == 0)
			{
				(void)write(in[1], key->key, my_strlen(key->key));
				(void)write(in[1], "\n", 1);
			}
			(void)write(in[1], ptr, c);
			new_close(in[1]);
			sc = read(out[0], lbuf, sizeof lbuf);
			wait(NULL);
			if (sc > 0)
				lbuf[sc] = '\0';
			new_close(out[0]);
			break;
		}
		new_free(&ptr);
		if (flag)
			ptr = ctcp_quote_it(lbuf, my_strlen(lbuf));
		else
			malloc_strcpy(&ptr, lbuf);
	}
	else
	{
		c = my_strlen(str);
		if (flag)
		{
			if (key->crypt(key, &str, &c) != 0)
			{
				yell("--- do_crypt(): crypto encrypt failed");
				return 0;
			}
			ptr = ctcp_quote_it(str, c);
		}
		else
		{
			ptr = ctcp_unquote_it(str, &c);
			if (key->decrypt(key, &ptr, &c) != 0)
			{
				yell("--- do_crypt(): crypto decrypt failed");
				return 0;
			}
		}
	}
	if (type)
		*type = key->type;
	return (ptr);
}

/*
 * crypt_msg: Executes the encryption program on the given string with the
 * given key.  If flag is true, the string is encrypted and the returned
 * string is ready to be sent over irc.  If flag is false, the string is
 * decrypted and the returned string should be readable.
 */
u_char	*
crypt_msg(u_char *str, crypt_key *key, int flag)
{
	static	u_char	lbuf[CRYPT_BUFFER_SIZE];
	u_char	*ptr,
		*rest,
		*type;
	int	on = 1;

	if (flag)
	{
		*lbuf = '\0';
		while ((rest = my_index(str, '\005')) != NULL)
		{
			*(rest++) = '\0';
			if (on && *str && (ptr = do_crypt(str, key, flag, &type)))
			{
				snprintf(CP(lbuf), sizeof lbuf, "%c%.30s ", CTCP_DELIM_CHAR, type);
				my_strmcat(lbuf, ptr, sizeof lbuf);
				my_strmcat(lbuf, CTCP_DELIM_STR, sizeof lbuf);
				new_free(&ptr);
			}
			else
				my_strmcat(lbuf, str, sizeof lbuf);
			on = !on;
			str = rest;
		}
		if (on && (ptr = do_crypt(str, key, flag, &type)))
		{
			snprintf(CP(lbuf), sizeof lbuf, "%c%.30s ", CTCP_DELIM_CHAR, type);
			my_strmcat(lbuf, ptr, sizeof lbuf);
			my_strmcat(lbuf, CTCP_DELIM_STR, sizeof lbuf);
			new_free(&ptr);
		}
		else
			my_strmcat(lbuf, str, sizeof lbuf);
	}
	else
	{
		if ((ptr = do_crypt(str, key, flag, &type)) != NULL)
		{
			my_strmcpy(lbuf, ptr, sizeof lbuf);
			new_free(&ptr);
		}
		else
			my_strmcat(lbuf, str, sizeof lbuf);
	}
	return (lbuf);
}

#ifdef HAVE_DEV_RANDOM
static void alarmer(int);

static void
alarmer(int dummy)
{

}

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
			yell("--- HELP!!!! crypt_dev_random_byte: can not open %s: %s",
			    DEV_RANDOM_PATH, strerror(errno));
			yell("--- using random()");
			devrndfd = -2;
		}
	}
	if (devrndfd == -2)
		goto do_random_instead;

	(void)MY_SIGNAL(SIGALRM, (sigfunc *) alarmer, 0);
	alarm(2);
	if (read(devrndfd, &c, 1) != 1)
	{
		alarm(0);
		(void)MY_SIGNAL(SIGALRM, (sigfunc *) SIG_DFL, 0);
		/* if we were just interrupted, don't bail on /dev/random */
		if (errno == EINTR)
		{
			yell("--- crypt_dev_random_byte: timeout, using random()");
			goto do_random_instead;
		}
		yell("--- HELP!  crypt_dev_random_byte: read of one byte on %s failed: %s",
		    DEV_RANDOM_PATH, strerror(errno));
		yell("--- using random()");
		devrndfd = -2;
		goto do_random_instead;
	}
	alarm(0);
	(void)MY_SIGNAL(SIGALRM, (sigfunc *) SIG_DFL, 0);
	return ((int)c);

do_random_instead:
	return (random() & 255);
}
#endif
