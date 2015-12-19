/*
 * ctcp.c: handles the client-to-client protocol(ctcp). 
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
IRCII_RCSID("@(#)$eterna: ctcp.c,v 1.101 2014/03/14 21:19:26 mrg Exp $");

#include <pwd.h>

#include <sys/utsname.h>

#include "ircaux.h"
#include "hook.h"
#include "irccrypt.h"
#include "ctcp.h"
#include "vars.h"
#include "server.h"
#include "status.h"
#include "lastlog.h"
#include "ignore.h"
#include "output.h"
#include "window.h"
#include "dcc.h"
#include "names.h"
#include "parse.h"
#include "whois.h"

/*
 * CtcpEntry: the format for each ctcp function.   note that the function
 * described takes 4 parameters:
 *     - a pointer to the ctcp entry,
 *     - who the message was from,
 *     - who the message was to (nickname, channel, etc),
 *     - and the rest of the ctcp message.
 * it can return null, or it can return a malloced string that will be
 * inserted into the oringal message at the point of the ctcp.  if null is
 * returned, nothing is added to the original message
 */
typedef	struct ctcp_stru CtcpEntry;
struct ctcp_stru
{
	u_char	*name;		/* name of ctcp datatag */
	u_char	*desc;		/* description returned by ctcp clientinfo */
	int	flag;
	u_char	*(*func)(CtcpEntry *, u_char *, u_char *, u_char *);
};

/* CtcpFlood: used to limit flooding */
struct ctcp_flood_stru {
	time_t	ctcp_last_reply_time;
	time_t  ctcp_flood_time;
	int	ctcp_backlog_size;
	int	*ctcp_send_size;
};

static	u_char	CTCP_Reply_Buffer[BIG_BUFFER_SIZE] = "";

static	void	do_new_notice_ctcp(u_char *, u_char *, u_char **, u_char *);

/* forward declarations for the built in CTCP functions */
static	u_char	*do_crypto(CtcpEntry *, u_char *, u_char *, u_char *);
static	u_char	*do_version(CtcpEntry *, u_char *, u_char *, u_char *);
static	u_char	*do_clientinfo(CtcpEntry *, u_char *, u_char *, u_char *);
static	u_char	*do_echo(CtcpEntry *, u_char *, u_char *, u_char *);
static	u_char	*do_userinfo(CtcpEntry *, u_char *, u_char *, u_char *);
static	u_char	*do_finger(CtcpEntry *, u_char *, u_char *, u_char *);
static	u_char	*do_time(CtcpEntry *, u_char *, u_char *, u_char *);
static	u_char	*do_atmosphere(CtcpEntry *, u_char *, u_char *, u_char *);
static	u_char	*do_dcc(CtcpEntry *, u_char *, u_char *, u_char *);
static	u_char	*do_utc(CtcpEntry *, u_char *, u_char *, u_char *);

static CtcpEntry ctcp_cmd[] =
{
	{ UP("VERSION"),	UP("shows client type, version and environment"),
		CTCP_VERBOSE, do_version },
	{ UP("CLIENTINFO"),	UP("gives information about available CTCP commands"),
		CTCP_VERBOSE, do_clientinfo },
	{ UP("USERINFO"),	UP("returns user settable information"),
		CTCP_VERBOSE, do_userinfo },
#define CTCP_ERRMSG	3
	{ UP("ERRMSG"),	UP("returns error messages"),
		CTCP_VERBOSE, do_echo },
	{ UP("FINGER"),	UP("shows real name, login name and idle time of user"),
		CTCP_VERBOSE, do_finger },
	{ UP("TIME"),	UP("tells you the time on the user's host"),
		CTCP_VERBOSE, do_time },
	{ UP("ACTION"),	UP("contains action descriptions for atmosphere"),
		CTCP_SHUTUP, do_atmosphere },
	{ UP("DCC"),	UP("requests a direct_client_connection"),
		CTCP_SHUTUP | CTCP_NOREPLY, do_dcc },
	{ UP("UTC"),	UP("substitutes the local timezone"),
		CTCP_SHUTUP | CTCP_NOREPLY, do_utc },
	{ UP("PING"), 	UP("returns the arguments it receives"),
		CTCP_VERBOSE, do_echo },
	{ UP("ECHO"), 	UP("returns the arguments it receives"),
		CTCP_VERBOSE, do_echo },
	{ CAST_STRING, UP("contains CAST-128 strongly encrypted data, CBC mode"),
		CTCP_SHUTUP | CTCP_NOREPLY, do_crypto },
#if 0
	{ RIJNDAEL_STRING, UP("contains rijndael (AES) strongly encrypted data, CBC mode"),
		CTCP_SHUTUP | CTCP_NOREPLY, do_crypto },
#endif
};
#define	NUMBER_OF_CTCPS ARRAY_SIZE(ctcp_cmd)

static	const	u_char	*ctcp_types[] =
{
	UP("PRIVMSG"),
	UP("NOTICE")
};

static	u_char	ctcp_buffer[BIG_BUFFER_SIZE] = "";

/* This is set to one if we parsed an crypted message */
static	int     ctcp_was_crypted_local = 0;

/*
 * in_ctcp_flag is set to true when IRCII is handling a CTCP request.  This
 * is used by the ctcp() sending function to force NOTICEs to be used in any
 * CTCP REPLY 
 */
static	int	in_ctcp_flag = 0;

/*
 * quote_it: This quotes the given string making it sendable via irc.  A
 * pointer to the length of the data is required and the data need not be
 * null terminated (it can contain nulls).  Returned is a malloced, null
 * terminated string.   
 */
u_char	*
ctcp_quote_it(u_char *str, size_t len)
{
	u_char	lbuf[BIG_BUFFER_SIZE];
	u_char	*ptr;
	size_t	i;

	ptr = lbuf;
	for (i = 0; i < len; i++)
	{
		switch (str[i])
		{
		case CTCP_DELIM_CHAR:
			*(ptr++) = CTCP_QUOTE_CHAR;
			*(ptr++) = 'a';
			break;
		case '\n':
			*(ptr++) = CTCP_QUOTE_CHAR;
			*(ptr++) = 'n';
			break;
		case '\r':
			*(ptr++) = CTCP_QUOTE_CHAR;
			*(ptr++) = 'r';
			break;
		case CTCP_QUOTE_CHAR:
			*(ptr++) = CTCP_QUOTE_CHAR;
			*(ptr++) = CTCP_QUOTE_CHAR;
			break;
		case '\0':
			*(ptr++) = CTCP_QUOTE_CHAR;
			*(ptr++) = '0';
			break;
		case ':':
			*(ptr++) = CTCP_QUOTE_CHAR;
		default:
			*(ptr++) = str[i];
			break;
		}
	}
	*ptr = '\0';
	str = NULL;
	malloc_strcpy(&str, lbuf);
	return (str);
}

/*
 * ctcp_unquote_it: This takes a null terminated string that had previously
 * been quoted using ctcp_quote_it and unquotes it.  Returned is a malloced
 * space pointing to the unquoted string.  The len is modified to contain
 * the size of the data returned. 
 */
u_char	*
ctcp_unquote_it(u_char *str, size_t *len)
{
	u_char	*lbuf;
	u_char	*ptr;
	u_char	c;
	int	i,
		new_size = 0;

	lbuf = new_malloc(sizeof(*lbuf) * *len);
	ptr = lbuf;
	i = 0;
	while (i < *len)
	{
		if ((c = str[i++]) == CTCP_QUOTE_CHAR)
		{
			switch (c = str[i++])
			{
			case CTCP_QUOTE_CHAR:
				*(ptr++) = CTCP_QUOTE_CHAR;
				break;
			case 'a':
				*(ptr++) = CTCP_DELIM_CHAR;
				break;
			case 'n':
				*(ptr++) = '\n';
				break;
			case 'r':
				*(ptr++) = '\r';
				break;
			case '0':
				*(ptr++) = '\0';
				break;
			default:
				*(ptr++) = c;
				break;
			}
		}
		else
			*(ptr++) = c;
		new_size++;
	}
	*len = new_size;
	return (lbuf);
}

/*
 * do_crypto: performs the ecrypted data trasfer for ctcp.  Returns in a
 * malloc string the decryped message (if a key is set for that user) or the
 * text "[ENCRYPTED MESSAGE]" 
 */
static	u_char	*
do_crypto(CtcpEntry *ctcp, u_char *from, u_char *to, u_char *args)
{
	crypt_key *key;
	u_char	*crypt_who,
		*msg;
	u_char	*ret = NULL;

	if (is_channel(to))
		crypt_who = to;
	else
		crypt_who = from;
	if ((key = is_crypted(crypt_who)) && (msg = crypt_msg(args, key, 0)))
	{
		/* this doesn't work ...
		   ... when it does, set this to 0 ... */
		static	int	the_youth_of_america_on_elle_esse_dee = 1;

		malloc_strcpy(&ret, msg);
		/*
		 * since we are decrypting, run it through do_ctcp() again
		 * to detect embeded CTCP messages, in an encrypted message.
		 * we avoid recusing here more than once.
		 */
		if (the_youth_of_america_on_elle_esse_dee++ == 0)
			ret = do_ctcp(from, to, ret);
		the_youth_of_america_on_elle_esse_dee--;
		ctcp_was_crypted_local = 1;
	}
	else	/*
		 * XXX we should (optionally) save these so that
		 * the user can decrypt them after giving the key.
		 */
		malloc_strcpy(&ret, UP("[ENCRYPTED MESSAGE]"));
	return (ret);
}

/*
 * do_clientinfo: performs the CLIENTINFO CTCP.  If cmd is empty, returns the
 * list of all CTCPs currently recognized by IRCII.  If an arg is supplied,
 * it returns specific information on that CTCP.  If a matching CTCP is not
 * found, an ERRMSG ctcp is returned 
 */
static	u_char	*
do_clientinfo(CtcpEntry *ctcp, u_char *from, u_char *to, u_char *cmd)
{
	int	i;
	u_char	*ucmd = NULL;
	u_char	buffer[BIG_BUFFER_SIZE];

	if (cmd && *cmd)
	{
		malloc_strcpy(&ucmd, cmd);
		upper(ucmd);
		for (i = 0; i < NUMBER_OF_CTCPS; i++)
		{
			if (my_strcmp(ucmd, ctcp_cmd[i].name) == 0)
			{
				send_ctcp_reply(from, ctcp->name, "%s %s",
					ctcp_cmd[i].name, ctcp_cmd[i].desc);
				return NULL;
			}
		}
		send_ctcp_reply(from, ctcp_cmd[CTCP_ERRMSG].name,
				"%s: %s is not a valid function",
				ctcp->name, cmd);
	}
	else
	{
		*buffer = '\0';
		for (i = 0; i < NUMBER_OF_CTCPS; i++)
		{
			my_strmcat(buffer, ctcp_cmd[i].name, sizeof buffer);
			my_strmcat(buffer, " ", sizeof buffer);
		}
		send_ctcp_reply(from, ctcp->name, 
			"%s :Use CLIENTINFO <COMMAND> to get more specific information",
			buffer);
	}
	return NULL;
}

/* do_version: does the CTCP VERSION command */
static	u_char	*
do_version(CtcpEntry *ctcp, u_char *from, u_char *to, u_char *cmd)
{

#if defined(PARANOID)
	send_ctcp_reply(from, ctcp->name, "ircII user");
#else
	u_char	*tmp;
	struct utsname un;
	u_char	*the_unix,
		*the_version;

	if (uname(&un) < 0)
	{
		the_version = empty_string();
		the_unix = UP("unknown");
	}
	else
	{
		the_version = UP(un.release);
		the_unix = UP(un.sysname);
	}
	send_ctcp_reply(from, ctcp->name, "ircII %s %s %s :%s", irc_version(), the_unix, the_version,
		(tmp = get_string_var(CLIENTINFO_VAR)) ? tmp : UP(IRCII_COMMENT));
#endif /* PARANOID */
	return NULL;
}

/* do_time: does the CTCP TIME command --- done by Veggen */
static	u_char	*
do_time(CtcpEntry *ctcp, u_char *from, u_char *to, u_char *cmd)
{
	time_t	tm = time(NULL);
	u_char	*s, *t = (u_char *) ctime(&tm);

	if (NULL != (s = my_index(t, '\n')))
		*s = '\0';
	send_ctcp_reply(from, ctcp->name, "%s", t);
	return NULL;
}

/* do_userinfo: does the CTCP USERINFO command */
static	u_char	*
do_userinfo(CtcpEntry *ctcp, u_char *from, u_char *to, u_char *cmd)
{
	send_ctcp_reply(from, ctcp->name, "%s", get_string_var(USER_INFO_VAR));
	return NULL;
}

/*
 * do_echo: does the CTCP ECHO, CTCP ERRMSG and CTCP PING commands. Does
 * not send an error for ERRMSG and if the CTCP was sent to a channel.
 */
static	u_char	*
do_echo(CtcpEntry *ctcp, u_char *from, u_char *to, u_char *cmd)
{
	if (!is_channel(to) || my_strncmp(cmd, "ERRMSG", 6))
		send_ctcp_reply(from, ctcp->name, "%s", cmd);
	return NULL;
}

static	u_char	*
do_finger(CtcpEntry *ctcp, u_char *from, u_char *to, u_char *cmd)
{
#if defined(PARANOID)
	send_ctcp_reply(from, ctcp->name, "ircII user");
#else
	struct	passwd	*pwd;
	time_t	diff;
	unsigned	uid;
	u_char	c;

	/*
	 * sojge complained that ircII says 'idle 1 seconds'
	 * well, now he won't ever get the chance to see that message again
	 *   *grin*  ;-)    -lynx
	 *
	 * Made this better by saying 'idle 1 second'  -phone
	 */

	diff = time(0) - idle_time();
	c = (diff == 1) ? ' ' : 's';
	uid = getuid();
# ifdef DAEMON_UID
	if (uid != DAEMON_UID)
	{
# endif /* DAEMON_UID */	
		if ((pwd = getpwuid(uid)) != NULL)
		{
			u_char	*tmp;

# ifndef GECOS_DELIMITER
#  define GECOS_DELIMITER ','
# endif /* GECOS_DELIMITER */
			if ((tmp = my_index(pwd->pw_gecos, GECOS_DELIMITER)) != NULL)
				*tmp = '\0';
			send_ctcp_reply(from, ctcp->name,
				"%s (%s@%s) Idle %d second%c", pwd->pw_gecos,
				pwd->pw_name, my_hostname(), (int)diff, c);
		}
# ifdef DAEMON_UID
	}
	else
		send_ctcp_reply(from, ctcp->name,
			"IRCII Telnet User (%s) Idle %d second%c",
			my_realname(), (int)diff, c);
# endif /* DAEMON_UID */	
#endif /* PARANOID */
	return NULL;
}

/*
 * do_atmosphere: does the CTCP ACTION command --- done by lynX
 * Changed this to make the default look less offensive to people
 * who don't like it and added a /on ACTION. This is more in keeping
 * with the design philosophy behind IRCII
 */
static	u_char	*
do_atmosphere(CtcpEntry *ctcp, u_char *from, u_char *to, u_char *cmd)
{
	if (cmd && *cmd)
	{
		int old;

		save_message_from();
		old = set_lastlog_msg_level(LOG_ACTION);
		if (is_channel(to))
		{
			message_from(to, LOG_ACTION);
			if (do_hook(ACTION_LIST, "%s %s %s", from, to, cmd))
			{
				if (is_current_channel(to, parsing_server(), 0))
					put_it("* %s %s", from, cmd);
				else
					put_it("* %s:%s %s", from, to, cmd);
			}
		}
		else
		{
			if ('=' == *from)
			{
				set_lastlog_msg_level(LOG_DCC);
				message_from(from+1, LOG_DCC);
			}
			else
				message_from(from, LOG_ACTION);
			if (do_hook(ACTION_LIST, "%s %s %s", from, to, cmd))
				put_it("*> %s %s", from, cmd);
		}
		set_lastlog_msg_level(old);
		restore_message_from();
	}
	return NULL;
}

/*
 * do_dcc: Records data on an incoming DCC offer. Makes sure it's a
 *	user->user CTCP, as channel DCCs don't make any sense whatsoever
 */
static	u_char	*
do_dcc(CtcpEntry *ctcp, u_char *from, u_char *to, u_char *args)
{
	u_char	*type;
	u_char	*description;
	u_char	*inetaddr;
	u_char	*port;
	u_char	*size;

	if (my_stricmp(to, server_get_nickname(parsing_server())))
		return NULL;
	if (!(type = next_arg(args, &args)) ||
			!(description = next_arg(args, &args)) ||
			!(inetaddr = next_arg(args, &args)) ||
			!(port = next_arg(args, &args)))
		return NULL;
	size = next_arg(args, &args);
	register_dcc_offer(from, type, description, inetaddr, port, size);
	return NULL;
}

/*
 * do_utc: converts `UTC <number>' into a date string, for
 # inclusion into the normal chat stream.
 */
static u_char	*
do_utc(CtcpEntry *ctcp, u_char *from, u_char *to, u_char *args)
{
	time_t	tm;
	u_char	*date = NULL;

	if (!args || !*args)
		return NULL;
	tm = my_atol(args);
	malloc_strcpy(&date, UP(ctime(&tm)));
	date[my_strlen(date)-1] = '\0';
	return date;
}

/*
 * do_ctcp: handles the client to client protocol embedded in PRIVMSGs.  Any
 * such messages are removed from the original str, so after do_ctcp()
 * returns, str will be changed
 */
u_char	*
do_ctcp(u_char *from, u_char *to, u_char *str)
{
	int	i = 0,
		ctcp_flag = 1;
	u_char	*end,
		*cmd,
		*args,
		*ptr;
	u_char	*arg_copy = NULL;
	int	flag;
	int	messages = 0;
	time_t	curtime = time(NULL);

	flag = double_ignore(from, from_user_host(), IGNORE_CTCPS);

	if (!in_ctcp_flag)
		in_ctcp_flag = 1;
	*ctcp_buffer = '\0';
	while ((cmd = my_index(str, CTCP_DELIM_CHAR)) != NULL)
	{
		if (messages > 3)
			break;
		*(cmd++) = '\0';
		my_strmcat(ctcp_buffer, str, sizeof ctcp_buffer);
		if ((end = my_index(cmd, CTCP_DELIM_CHAR)) != NULL)
		{
			messages++;
			if (flag == IGNORED)
				continue;
			*(end++) = '\0';
			if ((args = my_index(cmd, ' ')) != NULL)
				*(args++) = '\0';
			else
				args = empty_string();
#if 0
			/* Skip leading : for arguments */
			if (*args == ':')
				++args;
#endif
			malloc_strcpy(&arg_copy, args);
			for (i = 0; i < NUMBER_OF_CTCPS; i++)
			{
				if (my_strcmp(cmd, ctcp_cmd[i].name) == 0)
				{
					/* protect against global (oper) messages */
					if (*to != '$' && !(*to == '#' && !lookup_channel(to, parsing_server(), CHAN_NOUNLINK)))
					{
						ptr = ctcp_cmd[i].func(&ctcp_cmd[i], from, to, arg_copy);
						if (ptr)
						{
							my_strmcat(ctcp_buffer, ptr, sizeof ctcp_buffer);
							new_free(&ptr);
						}
					}
					ctcp_flag = ctcp_cmd[i].flag;
					cmd = ctcp_cmd[i].name;
					break;
				}
			}
			new_free(&arg_copy);
			if (in_ctcp_flag == 1 &&
			    do_hook(CTCP_LIST, "%s %s %s %s", from, to, cmd,
				    args) &&
			    get_int_var(VERBOSE_CTCP_VAR))
			{
				int     lastlog_level;

				save_message_from();
				lastlog_level = set_lastlog_msg_level(LOG_CTCP);
				message_from(NULL, LOG_CTCP);
				if (i == NUMBER_OF_CTCPS)
				{
					if (do_beep_on_level(LOG_CTCP))
						beep_em(1);
					say("Unknown CTCP %s from %s to %s: %s%s",
						cmd, from, to, *args ?
						(u_char *) ": " : empty_string(), args);
				}
				else if (ctcp_flag & CTCP_VERBOSE)
				{
					if (do_beep_on_level(LOG_CTCP))
						beep_em(1);
					if (my_stricmp(to,
					    server_get_nickname(parsing_server())))
						say("CTCP %s from %s to %s: %s",
							cmd, from, to, args);
					else
						say("CTCP %s from %s%s%s", cmd,
							from, *args ? (u_char *)
							": " : empty_string(), args);
				}
				set_lastlog_msg_level(lastlog_level);
				restore_message_from();
			}
			str = end;
		}
		else
		{
			my_strmcat(ctcp_buffer, CTCP_DELIM_STR, sizeof ctcp_buffer);
			str = cmd;
		}
	}
	if (in_ctcp_flag == 1)
		in_ctcp_flag = 0;
	if (*CTCP_Reply_Buffer)
	{
#ifdef PARANOID
		/*
		 * paranoid users don't want to send ctcp replies to
		 * requests send to channels, probably...
		 */
		if (is_channel(to))
			goto clear_ctcp_reply_buffer;
#endif
		/*
		 * Newave ctcp flood protection : each time you are requested to send
		 * more than CTCP_REPLY_FLOOD_SIZE bytes in CTCP_REPLY_BACKLOG_SECONDS
		 * no ctcp replies will be done for CTCP_REPLY_IGNORE_SECONDS.
		 * Current default is 256 bytes/ 5s/ 10s
		 * This is a sliding window, i.e. you can't get caught sending too much
		 * because of a 5s boundary, and the checking is still active even if
		 * you don't reply anymore.
		 */

		if (*from == '=')
			send_ctcp(ctcp_type(CTCP_NOTICE), from, NULL, "%s", CTCP_Reply_Buffer);
		else
		{
			CtcpFlood *flood_info = server_get_ctcp_flood(parsing_server());
			int	no_reply,
				no_flood = get_int_var(NO_CTCP_FLOOD_VAR),
				delta = flood_info->ctcp_last_reply_time ?
					curtime - flood_info->ctcp_last_reply_time : 0,
				size = 0,
				was_ignoring = flood_info->ctcp_flood_time != 0,
				crbs = get_int_var(CTCP_REPLY_BACKLOG_SECONDS_VAR),
				crfs = get_int_var(CTCP_REPLY_FLOOD_SIZE_VAR),
				cris = get_int_var(CTCP_REPLY_IGNORE_SECONDS_VAR);

			flood_info->ctcp_last_reply_time = curtime;

			if (delta)
			{
				for (i = crbs - 1; i >= delta; i--)
					flood_info->ctcp_send_size[i] = flood_info->ctcp_send_size[i - delta];
				for (i = 0; i < delta && i < crbs; i++)
					flood_info->ctcp_send_size[i] = 0;
			}

			flood_info->ctcp_send_size[0] += my_strlen(CTCP_Reply_Buffer);

			for (i = 0; i < crbs; i++)
				size += flood_info->ctcp_send_size[i];
			if (size >= crfs)
				flood_info->ctcp_flood_time = curtime;

			no_reply = flood_info->ctcp_flood_time &&
				   (curtime <= flood_info->ctcp_flood_time + cris);

			if (no_flood && get_int_var(VERBOSE_CTCP_VAR))
			{
				save_message_from();
				message_from(NULL, LOG_CTCP);
				if (no_reply && was_ignoring == 0)
					say("CTCP flood detected - suspending replies");
				else if (no_reply == 0 && was_ignoring)
					say("CTCP reply suspending time elapsed - replying normally");
				restore_message_from();
			}
			if (no_flood == 0 || no_reply == 0)
			{
				flood_info->ctcp_flood_time = 0;
				send_ctcp(ctcp_type(CTCP_NOTICE), from, NULL, "%s", CTCP_Reply_Buffer);
			}
		}
#ifdef PARANOID
clear_ctcp_reply_buffer:
#endif
		*CTCP_Reply_Buffer = '\0';
	}
	if (*str)
		my_strmcat(ctcp_buffer, str, sizeof ctcp_buffer);
	return (ctcp_buffer);
}

u_char	*
do_notice_ctcp(u_char *from, u_char *to, u_char *str)
{
	u_char	*cmd;

	in_ctcp_flag = -1;
	*ctcp_buffer = '\0';
	/*
	 * The following used to say "While". It now says "if" because people
	 * Started using CTCP ERRMSG replies to CTCP bomb. The effect of this
	 * is that IRCII users can only send one CTCP/message if they expect a
	 * reply. This shouldn't be a problem as that is the way IRCII operates
	 *
	 * Changed this behavouir to follow NO_CTCP_FLOOD
	 */

	if (get_int_var(NO_CTCP_FLOOD_VAR))
	{
		if ((cmd = my_index(str, CTCP_DELIM_CHAR)) != NULL)
			do_new_notice_ctcp(from, to, &str, cmd);
	}
	else
		while ((cmd = my_index(str, CTCP_DELIM_CHAR)) != NULL)
			do_new_notice_ctcp(from, to, &str, cmd);
	in_ctcp_flag = 0;
	my_strmcat(ctcp_buffer, str, sizeof ctcp_buffer);
	return (ctcp_buffer);
}

static	void
do_new_notice_ctcp(u_char *from, u_char *to, u_char **str, u_char *cmd)
{
	u_char	*end,
		*args,
		*ptr,
		*arg_copy = NULL;
	int	flags,
		i,
		lastlog_level;

	flags = 0;
	*(cmd++) = '\0';
	my_strmcat(ctcp_buffer, *str, sizeof ctcp_buffer);
	if ((end = my_index(cmd, CTCP_DELIM_CHAR)) != NULL)
	{
		*(end++) = '\0';
		if ((args = my_index(cmd, ' ')) != NULL)
			*(args++) = '\0';
		malloc_strcpy(&arg_copy, args);
		for (i = 0; i < NUMBER_OF_CTCPS; i++)
		{
			if ((my_strcmp(cmd, ctcp_cmd[i].name) == 0) && ctcp_cmd[i].flag & CTCP_NOREPLY)
			{
				if ((ptr = ctcp_cmd[i].func(&(ctcp_cmd[i]), from, to, arg_copy)) != NULL)
				{
					my_strmcat(ctcp_buffer, ptr, sizeof ctcp_buffer);
					new_free(&ptr);
					flags = ctcp_cmd[i].flag;
				}
				break;
			}
		}
		new_free(&arg_copy);
		if (!args)
			args = empty_string();
		if (do_hook(CTCP_REPLY_LIST, "%s %s %s %s", from, to, cmd,
				args) && !(flags & CTCP_NOREPLY))
		{
			if (!my_strcmp(cmd, "PING"))
			{
				u_char	buf[20];
				time_t	timediff,
					currenttime;

				currenttime = time(NULL);
				if (args && *args)
					timediff = currenttime -
						(time_t) my_atol(args);
				else
					timediff = (time_t) 0;
				snprintf(CP(buf), sizeof buf, "%ld second%s",
				   (long) timediff, (timediff == 1) ? "" : "s");
				args = buf;
			}
			save_message_from();
			lastlog_level = set_lastlog_msg_level(LOG_CTCP);
			message_from(NULL, LOG_CTCP);
			say("CTCP %s reply from %s: %s", cmd, from,
				args);
			set_lastlog_msg_level(lastlog_level);
			restore_message_from();
		}
		*str = end;
	}
	else
	{
		my_strmcat(ctcp_buffer, CTCP_DELIM_STR, sizeof ctcp_buffer);
		*str = cmd;
	}
}

/* in_ctcp: simply returns the value of the ctcp flag */
int
in_ctcp(void)
{
	return (in_ctcp_flag);
}

/* These moved here because they belong here - phone */

/*
 * send_ctcp: A simply way to send CTCP queries.   if the datatag
 * is NULL, we must have already formatted the ctcp reply (it has the
 * ctcp delimiters), so don't add them again, etc.
 */
void
send_ctcp(const u_char *type, u_char *to, u_char *datatag, char *format, ...)
{
	va_list vl;
	u_char putbuf[BIG_BUFFER_SIZE], sendbuf[BIG_BUFFER_SIZE];
	u_char *sendp;

	if (in_on_who())
		return;	/* Silently drop it on the floor */
	if (format)
	{
		va_start(vl, format);
		vsnprintf(CP(putbuf), sizeof putbuf, format, vl);
		va_end(vl);
		
		if (datatag)
		{
			snprintf(CP(sendbuf), sizeof sendbuf, "%c%s %s%c",
			    CTCP_DELIM_CHAR, datatag, putbuf, CTCP_DELIM_CHAR);
			sendp = sendbuf;
		}
		else
			sendp = putbuf;
	}
	else
	{
		snprintf(CP(sendbuf), sizeof sendbuf, "%c%s%c",
		    CTCP_DELIM_CHAR, datatag, CTCP_DELIM_CHAR);
		sendp = sendbuf;
	}

	/*
	 * ugh, special case dcc because we don't want to go through
	 * send_text in its current state.  XXX - fix send_text to
	 * deal with ctcp's as well.
	 */
	if (*to == '=')
		dcc_message_transmit(to + 1, sendp, DCC_CHAT, 0);
	else
		send_to_server("%s %s :%s", type, to, sendp);
}


/*
 * send_ctcp_notice: A simply way to send CTCP replies.   I put this here
 * rather than in ctcp.c to keep my compiler quiet 
 */
void
send_ctcp_reply(u_char *to, u_char *datatag, char *format, ...)
{
	va_list vl;
	u_char	putbuf[BIG_BUFFER_SIZE];

	if (in_on_who())
		return;	/* Silently drop it on the floor */
	if (to && (*to == '='))
		return;	/* don't allow dcc replies */
	my_strmcat(CTCP_Reply_Buffer, "\001", sizeof CTCP_Reply_Buffer);
	my_strmcat(CTCP_Reply_Buffer, datatag, sizeof CTCP_Reply_Buffer);
	my_strmcat(CTCP_Reply_Buffer, " ", sizeof CTCP_Reply_Buffer);
	if (format)
	{
		va_start(vl, format);
		vsnprintf(CP(putbuf), sizeof putbuf, format, vl);
		va_end(vl);
		my_strmcat(CTCP_Reply_Buffer, putbuf, BIG_BUFFER_SIZE);
	}
	else
		my_strmcat(CTCP_Reply_Buffer, putbuf, sizeof CTCP_Reply_Buffer);
	my_strmcat(CTCP_Reply_Buffer, "\001", sizeof CTCP_Reply_Buffer);
}

/* allocate a new CtcpFlood */
CtcpFlood *
ctcp_new_flood(void)
{
	CtcpFlood *new = new_malloc(sizeof *new);
	int i;

	new->ctcp_last_reply_time = 0;
	new->ctcp_flood_time = 0;
	new->ctcp_backlog_size = get_int_var(CTCP_REPLY_BACKLOG_SECONDS_VAR);
	new->ctcp_send_size = new_malloc(new->ctcp_backlog_size * sizeof(int));

	for (i = 0; i < new->ctcp_backlog_size; i++)
		new->ctcp_send_size[i] = 0;

	return new;
}

/* CtcpFlood sizing changed; update the contentes */
void
ctcp_refresh_flood(CtcpFlood *flood, int size)
{
	int	j, delta;

	delta = size - flood->ctcp_backlog_size;
	if (delta)
	{
		flood->ctcp_send_size =
			new_realloc(flood->ctcp_send_size, size * sizeof(int));
		for (j = flood->ctcp_backlog_size; j < size; j++)
			flood->ctcp_send_size[j] = 0;
		flood->ctcp_backlog_size = size;
	}
}

/* free a CtcpFlood */
void
ctcp_clear_flood(CtcpFlood **flood)
{
	new_free(&(*flood)->ctcp_send_size);
	new_free(flood);
}

const u_char	*
ctcp_type(int type)
{
	if (type < 0 || type >= ARRAY_SIZE(ctcp_types))
		return UP("CTCPUNKNOWN");
	return ctcp_types[type];
}

void
set_ctcp_was_crypted(int val)
{
	ctcp_was_crypted_local = val;
}

int
ctcp_was_crypted(void)
{
	return ctcp_was_crypted_local;
}
