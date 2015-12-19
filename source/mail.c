/*
 * mail.c: Ok, so I gave in.  I added mail checking.  So sue me. 
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
IRCII_RCSID("@(#)$eterna: mail.c,v 1.57 2014/03/14 01:57:16 mrg Exp $");

#include <sys/stat.h>

#include "newio.h"
#include "mail.h"
#include "lastlog.h"
#include "hook.h"
#include "vars.h"
#include "ircaux.h"
#include "output.h"
#include "window.h"

#if defined(UNIX_MAIL)
static	u_char	*mail_path = NULL;

static	void	init_mail(void);
#endif

/* init_mail: this initialized the path to the users mailbox */
static	void
init_mail(void)
{
#if defined(UNIX_MAIL)
	u_char	*tmp_mail_path;

	if (mail_path)
		return; /* why do it 2000 times?  -lynx */

	if ((tmp_mail_path = my_getenv("MAIL")) != NULL)
		malloc_strcpy(&mail_path, tmp_mail_path);
	else
	{
		malloc_strcpy(&mail_path, UP(UNIX_MAIL));
		malloc_strcat(&mail_path, UP("/"));
		malloc_strcat(&mail_path, my_username());
	}
#endif /* UNIX_MAIL */
}

/*
 * check_mail_status: returns 0 if mail status has not changed, 1 if mail
 * status has changed 
 */
int
check_mail_status(void)
{
#if defined(UNIX_MAIL)
	struct	stat	stat_buf;
	static	time_t	old_stat = 0L;

#ifdef DAEMON_UID
	if (getuid() == DAEMON_UID)
		return 0;
#endif /* DAEMON_UID */

	if (!get_int_var(MAIL_VAR))
	{
		old_stat = 0L;
		return (0);
	}
	init_mail();
	if (stat(CP(mail_path), &stat_buf) == -1)
		return (0);
	if (stat_buf.st_ctime > old_stat)
	{
		old_stat = stat_buf.st_ctime;
		return (1);
	}
#endif /* defined(UNIX_MAIL) */
	return (0);
}

/*
 * check_mail: This here thing counts up the number of pieces of mail and
 * returns it as static string.  If there are no mail messages, null is
 * returned. 
 */
u_char	*
check_mail(void)
{
#if !defined(UNIX_MAIL)
	return	NULL;
#else
	static	unsigned int	cnt = 0;
	static	time_t	old_stat = 0L;
	static	u_char	ret_str[8];
	struct	stat	stat_buf;
	unsigned int	new_cnt = 0;
	u_char	tmp[8];
	static	int	VirginProgram = 1;  /* It's its first time */
	int	lastlog_level;
	u_char	buffer[BIG_BUFFER_SIZE];
#ifdef UNIX_MAIL
	int	des;
	int	blanks = 1;
	int	reset_blanks = 0;
#endif /* UNIX_MAIL */

#ifdef DAEMON_UID
	if (getuid()==DAEMON_UID)
		return (NULL);
#endif /* DAEMON_UID */

	init_mail();
#ifdef UNIX_MAIL
	if (stat(CP(mail_path), &stat_buf) == -1)
		return (NULL);
	lastlog_level = set_lastlog_msg_level(LOG_CRAP);
	save_message_from();
	message_from(NULL, LOG_CURRENT);
	if (stat_buf.st_ctime > old_stat)
	{
		old_stat = stat_buf.st_ctime;
		if ((des = open(CP(mail_path), O_RDONLY, 0)) >= 0)
		{
			new_cnt = 0;
			while (dgets(buffer, sizeof buffer, des) > 0)
			{
				if (buffer[0] == '\n') {
					blanks++;
					continue;
				}
				else
					reset_blanks = 1;
				if (!my_strncmp(MAIL_DELIMITER, buffer, sizeof(MAIL_DELIMITER) - 1) && blanks)
				{
					new_cnt++;
					if (new_cnt > cnt && !VirginProgram && get_int_var(MAIL_VAR) == 2)
					{
						while (dgets(buffer, sizeof buffer, des) > 0 && *buffer != '\0' && *buffer != '\n')
						{
							buffer[my_strlen(buffer)-1] = '\0';
							if (!my_strncmp(buffer, "From:", 5) || !my_strncmp(buffer, "Subject:", 8))
								say("%s", buffer);
						}
					}
				}
				if (reset_blanks)
					reset_blanks = blanks = 0;
			}
			VirginProgram=0;
			new_close(des);
		}
#endif /* UNIX_MAIL */
		/* yeeeeack */
		if (new_cnt > cnt)
		{
			snprintf(CP(tmp), sizeof tmp, "%d", new_cnt - cnt);
			snprintf(CP(buffer), sizeof buffer, "%d", new_cnt);
			if (do_hook(MAIL_LIST, "%s %s", tmp, buffer) && get_int_var(MAIL_VAR) == 1)
				say("You have new email.");
		}
		cnt = new_cnt;
	}
	set_lastlog_msg_level(lastlog_level);
	restore_message_from();
	if (cnt && (cnt < 65536))
	{
		snprintf(CP(ret_str), sizeof ret_str, "%d", cnt);
		return (ret_str);
	}
	else
		return (NULL);
#endif /* !defined(UNIX_MAIL) */
}
