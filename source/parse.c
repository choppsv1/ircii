/*
 * parse.c: handles messages from the server.   Believe it or not.  I
 * certainly wouldn't if I were you. 
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
IRCII_RCSID("@(#)$eterna: parse.c,v 1.114 2014/03/14 21:19:26 mrg Exp $");

#include "server.h"
#include "names.h"
#include "vars.h"
#include "ctcp.h"
#include "hook.h"
#include "edit.h"
#include "ignore.h"
#include "whois.h"
#include "lastlog.h"
#include "ircaux.h"
#include "funny.h"
#include "irccrypt.h"
#include "ircterm.h"
#include "flood.h"
#include "window.h"
#include "screen.h"
#include "output.h"
#include "numbers.h"
#include "parse.h"
#include "notify.h"
#include "notice.h"
#include "alias.h"

#define STRING_CHANNEL '+'
#define MULTI_CHANNEL '#'
#define LOCAL_CHANNEL '&'
#define SAFE_CHANNEL '!'

#define	MAXPARA	15	/* Taken from the ircd */

static	void	BreakArgs(u_char *, u_char **, u_char **);
static	void	p_linreply(u_char **);
static	void	p_ping(u_char **);
static	void	p_topic(u_char *, u_char **);
static	void	p_wall(u_char *, u_char **);
static	void	p_wallops(u_char *, u_char **);
static	void	p_privmsg(u_char *, u_char **);
static	void	p_quit(u_char *, u_char **);
static	void	p_pong(u_char *, u_char **);
static	void	p_error(u_char *, u_char **);
static	void	p_channel(u_char *, u_char **);
static	void	p_invite(u_char *, u_char **);
static	void	p_server_kill(u_char *, u_char **);
static	void	p_nick(u_char *, u_char **);
static	void	p_mode(u_char *, u_char **);
static	void	p_kick(u_char *, u_char **);
static	void	p_part(u_char *, u_char **);

/* User and host information from server 2.7 */
static	u_char	*FromUserHost = NULL;

/* doing a PRIVMSG */
static	int	doing_privmsg_active = 0;

/*
 * joined_nick: the nickname of the last person who joined the current
 * channel 
 */
static	u_char	*joined_nick = NULL;

/* public_nick: nick of the last person to send a message to your channel */
static	u_char	*public_nick = NULL;

/*
 * is_channel: determines if the argument is a channel.  If it's a number,
 * begins with MULTI_CHANNEL and has no '*', or STRING_CHANNEL, then its a
 * channel.
 */
int
is_channel(u_char *to)
{
	int	version;
	const	int	from_server = get_from_server();

	if (to == 0)
		return (0);

	version = server_get_version(from_server);
	if (version == ServerICB)
		return (my_stricmp(to, server_get_icbgroup(from_server)) == 0);
	else
		return ((version < Server2_7 && (isdigit(*to) || (*to == STRING_CHANNEL) || *to == '-'))
		     || (version > Server2_6 && *to == MULTI_CHANNEL)
		     || (version > Server2_7 && *to == LOCAL_CHANNEL)
		     || (version > Server2_8 && *to == STRING_CHANNEL)
		     || (version > Server2_9 && *to == SAFE_CHANNEL));
}


u_char	*
PasteArgs(u_char **Args, int StartPoint)
{
	int	i;

	for (; StartPoint; Args++, StartPoint--)
		if (!*Args)
			return NULL;
	for (i = 0; Args[i] && Args[i+1]; i++)
		Args[i][my_strlen(Args[i])] = ' ';
	Args[1] = NULL;
	return Args[0];
}

/*
 * BreakArgs: breaks up the line from the server, in to where its from,
 * setting FromUserHost if it should be, and returns all the arguements
 * that are there.   Re-written by phone, dec 1992.
 */
static	void
BreakArgs(u_char *Input, u_char **Sender, u_char **OutPut)
{
	u_char	*s = Input,
		*t;
	int	ArgCount = 0;

	/*
	 * Get sender from :sender and user@host if :nick!user@host
	 */
	FromUserHost = NULL;

	if (*Input == ':')
	{
		u_char	*tmp;

		*Input++ = '\0';
		if ((s = (u_char *) my_index(Input, ' ')) != NULL)
			*s++ = '\0';
		*Sender = Input;
		if ((tmp = (u_char *) my_index(*Sender, '!')) != NULL)
		{
			*tmp++ = '\0';
			FromUserHost = tmp;
		}
	}
	else
		*Sender = empty_string();

	if (!s)
		return;

	for (;;)
	{
		while (*s == ' ')
			*s++ = '\0';

		if (!*s)
			break;

		if (*s == ':')
		{
			for (t = s; *t; t++)
				*t = *(t + 1);
			OutPut[ArgCount++] = s;
			break;
		}
		OutPut[ArgCount++] = s;
		if (ArgCount >= MAXPARA)
			break;

		for (; *s != ' ' && *s; s++)
			;
	}
	OutPut[ArgCount] = NULL;
}

/* beep_em: Not hard to figure this one out */
void
beep_em(int beeps)
{
	int	cnt,
		i;

	for (cnt = beeps, i = 0; i < cnt; i++)
		term_beep();
}

/* in response to a TOPIC message from the server */
static	void
p_topic(u_char *from, u_char **ArgList)
{
	int	flag;

	if (!from)
		return;
	flag = double_ignore(from, from_user_host(), IGNORE_CRAP);
	if (flag == IGNORED)
		return;

	save_message_from();
	if (!ArgList[1])
	{
		message_from(NULL, LOG_CRAP);
		if (do_hook(TOPIC_LIST, "%s * %s", from, ArgList[0]))
			say("%s has changed the topic to %s", from, ArgList[0]);
	}
	else
	{
		message_from(ArgList[0], LOG_CRAP);
		if (do_hook(TOPIC_LIST, "%s %s %s", from, ArgList[0], ArgList[1]))
			say("%s has changed the topic on channel %s to %s",
				from, ArgList[0], ArgList[1]);
	}
	restore_message_from();
}

static	void
p_linreply(u_char **ArgList)
{
	PasteArgs(ArgList, 0);
	say("%s", ArgList[0]);
}

static	void
p_wall(u_char *from, u_char **ArgList)
{
	int	flag,
		level;
	u_char	*line;
	u_char	*high;

	if (!from)
		return;
	PasteArgs(ArgList, 0);
	if (!(line = ArgList[0]))
		return;
	flag = double_ignore(from, from_user_host(), IGNORE_WALLS);
	save_message_from();
	message_from(from, LOG_WALL);
	if (flag != IGNORED)
	{
		if (flag == HIGHLIGHTED)
			high = highlight_str();
		else
			high = empty_string();
		if ((flag != DONT_IGNORE) && (ignore_usernames() & IGNORE_WALLS)
				&& !from_user_host())
			add_to_whois_queue(from, whois_ignore_walls, "%s",line);
		else
		{
			level = set_lastlog_msg_level(LOG_WALL);
			if (check_flooding(from,
			    server_get_nickname(parsing_server()),
			    WALL_FLOOD, line) && do_hook(WALL_LIST, "%s %s",
						     from, line))
				put_it("%s#%s#%s %s", high, from, high, line);
			if (do_beep_on_level(LOG_WALL))
				beep_em(1);
			set_lastlog_msg_level(level);
		}
	}
	restore_message_from();
}

static	void
p_wallops(u_char *from, u_char **ArgList)
{
	int	flag, level;
	u_char	*line;

	if (!from)
		return;
	if (!(line = PasteArgs(ArgList, 0)))
		return;
	flag = double_ignore(from, from_user_host(), IGNORE_WALLOPS);
	level = set_lastlog_msg_level(LOG_WALLOP);
	save_message_from();
	message_from(from, LOG_WALLOP);
	if (my_index(from, '.'))
	{
		u_char	*high;

		if (flag != IGNORED)
		{
			if (flag == HIGHLIGHTED)
				high = highlight_str();
			else
				high = empty_string();
			if (do_hook(WALLOP_LIST, "%s S %s", from, line))
				put_it("%s!%s!%s %s", high, from, high, line);
			if (do_beep_on_level(LOG_WALLOP))
				beep_em(1);
		}
	}
	else
	{
		if (get_int_var(USER_WALLOPS_VAR))
		{
			if (flag != DONT_IGNORE &&
			    check_flooding(from,
					   server_get_nickname(parsing_server()),
					   WALLOP_FLOOD, line))
				add_to_whois_queue(from, whois_new_wallops, "%s", line);
		}
		else if (my_strcmp(from, server_get_nickname(get_window_server(0))) != 0)
			put_it("!%s! %s", from, line);
	}
	set_lastlog_msg_level(level);
	restore_message_from();
}

void
whoreply(u_char *from, u_char **ArgList)
{
	static	u_char	format[40];
	static	int	last_width = -1;
	int	ok;
	u_char	*channel,
		*user,
		*host,
		*server,
		*nick,
		*status,
		*name;
	int	i;

	if (last_width != get_int_var(CHANNEL_NAME_WIDTH_VAR))
	{
		if ((last_width = get_int_var(CHANNEL_NAME_WIDTH_VAR)) != 0)
		    snprintf(CP(format), sizeof format, "%%-%u.%us %%-9s %%-3s %%s@%%s (%%s)",
					(u_char) last_width,
					(u_char) last_width);
		else
		    my_strcpy(format, "%s\t%-9s %-3s %s@%s (%s)");
	}
	i = 0;
	channel = user = host = server = nick = status = name = empty_string();
	if (ArgList[i])
		channel = ArgList[i++];
	if (ArgList[i])
		user = ArgList[i++];
	if (ArgList[i])
		host = ArgList[i++];
	if (ArgList[i])
		server = ArgList[i++];
	if (ArgList[i])
		nick = ArgList[i++];
	if (ArgList[i])
		status = ArgList[i++];
	PasteArgs(ArgList, i);

	if (ArgList[i])
		name = ArgList[i];

	ok = whoreply_check(channel, user, host, server, nick, status, name,
			    ArgList, format);

	if (ok)
	{
		if (do_hook(WHO_LIST, "%s %s %s %s %s %s", channel, nick,
				status, user, host, name))
		{
			if (get_int_var(SHOW_WHO_HOPCOUNT_VAR))
				put_it(CP(format), channel, nick, status, user, host,
					name);
			else
			{
				u_char	*tmp;

				if ((tmp = (u_char *) my_index(name, ' ')) !=
								NULL)
					tmp++;
				else
					tmp = name;
				put_it(CP(format), channel, nick, status, user, host,
					tmp);
			}
		}
	}
}

static	void
p_privmsg(u_char *from, u_char **Args)
{
	int	level,
		flag,
		list_type,
		flood_type,
		log_type;
	u_char	ignore_type;
	u_char	*ptr,
		*to;
	u_char	*high;
	int	no_flood;

	if (!from)
		return;
	PasteArgs(Args, 1);
	to = Args[0];
	ptr = Args[1];
	if (!to || !ptr)
		return;
	save_message_from();
	if (is_channel(to))
	{
		malloc_strcpy(&public_nick, from);
		if (!is_on_channel(to, parsing_server(), from))
		{
			log_type = LOG_PUBLIC;
			ignore_type = IGNORE_PUBLIC;
			list_type = PUBLIC_MSG_LIST;
			flood_type = PUBLIC_FLOOD;
		}
		else
		{
			log_type = LOG_PUBLIC;
			ignore_type = IGNORE_PUBLIC;
			if (is_current_channel(to, parsing_server(), 0))
				list_type = PUBLIC_LIST;
			else
				list_type = PUBLIC_OTHER_LIST;
			flood_type = PUBLIC_FLOOD;
		}
		message_from(to, log_type);
	}
	else
	{
		flood_type = MSG_FLOOD;
		if (my_stricmp(to, server_get_nickname(parsing_server())))
		{
			log_type = LOG_WALL;
			ignore_type = IGNORE_WALLS;
			list_type = MSG_GROUP_LIST;
		}
		else
		{
			log_type = LOG_MSG;
			ignore_type = IGNORE_MSGS;
			list_type = MSG_LIST;
		}
		message_from(from, log_type);
	}
	flag = double_ignore(from, from_user_host(), ignore_type);
	switch (flag)
	{
	case IGNORED:
		if ((list_type == MSG_LIST) && get_int_var(SEND_IGNORE_MSG_VAR))
			send_to_server("NOTICE %s :%s is ignoring you", from,
					server_get_nickname(parsing_server()));
		goto out;
	case HIGHLIGHTED:
		high = highlight_str();
		break;
	default:
		high = empty_string();
		break;
	}
	level = set_lastlog_msg_level(log_type);
	ptr = do_ctcp(from, to, ptr);
	if (!ptr || !*ptr)
		goto out;
	if ((flag != DONT_IGNORE) && (ignore_usernames() & ignore_type) && !from_user_host())
		add_to_whois_queue(from, whois_ignore_msgs, "%s", ptr);
	else
	{
		no_flood = check_flooding(from, to, flood_type, ptr);
		if (ctcp_was_crypted() == 0 || do_hook(ENCRYPTED_PRIVMSG_LIST,"%s %s %s",from, to, ptr))
		{
		switch (list_type)
		{
		case PUBLIC_MSG_LIST:
			if (no_flood && do_hook(list_type, "%s %s %s", from, to, ptr))
			    put_it("%s(%s/%s)%s %s", high, from, to, high, ptr);
			break;
		case MSG_GROUP_LIST:
			if (no_flood && do_hook(list_type, "%s %s %s", from, to, ptr))
			    put_it("%s-%s:%s-%s %s", high, from, to, high, ptr);
			break;
		case MSG_LIST:
			if (!no_flood)
				break;
			set_recv_nick(from);
			if (is_away_set())
				beep_em(get_int_var(BEEP_WHEN_AWAY_VAR));
			if (do_hook(list_type, "%s %s", from, ptr))
			{
			    if (is_away_set())
			    {
				time_t t;
				u_char *msg = NULL;
				size_t len = my_strlen(ptr) + 20;

				t = time(NULL);
				msg = new_malloc(len);
				snprintf(CP(msg), len, "%s <%.16s>", ptr, ctime(&t));
				put_it("%s*%s*%s %s", high, from, high, msg);
				new_free(&msg);
			    }
			    else
				put_it("%s*%s*%s %s", high, from, high, ptr);
			}
			break;
		case PUBLIC_LIST:
			if (get_int_var(MAKE_NOTICE_MSG_VAR))
				doing_privmsg_active = 1;
			if (no_flood && do_hook(list_type, "%s %s %s", from, 
			    to, ptr))
				put_it("%s<%s>%s %s", high, from, high, ptr);
			doing_privmsg_active = 0;
			break;
		case PUBLIC_OTHER_LIST:
			if (get_int_var(MAKE_NOTICE_MSG_VAR))
				doing_privmsg_active = 1;
			if (no_flood && do_hook(list_type, "%s %s %s", from,
			    to, ptr))
				put_it("%s<%s:%s>%s %s", high, from, to, high,
					ptr);
			doing_privmsg_active = 0;
			break;
		}
		if (do_beep_on_level(log_type))
			beep_em(1);
		}
	}
	set_lastlog_msg_level(level);
out:
	restore_message_from();
}

static	void
p_quit(u_char *from, u_char **ArgList)
{
	int	one_prints = 0;
	u_char	*chan;
	u_char	*Reason;
	int	flag;

	if (!from)
		return;
	flag = double_ignore(from, from_user_host(), IGNORE_CRAP);
	save_message_from();
	if (flag != IGNORED)
	{
		PasteArgs(ArgList, 0);
		Reason = ArgList[0] ? ArgList[0] : (u_char *) "?";
		for (chan = walk_channels(from, 1, parsing_server()); chan; chan = walk_channels(from, 0, -1))
		{
			message_from(chan, LOG_CRAP);
			if (do_hook(CHANNEL_SIGNOFF_LIST, "%s %s %s", chan, from, Reason))
				one_prints = 1;
		}
		if (one_prints)
		{
			message_from(what_channel(from, parsing_server()), LOG_CRAP);
			if (do_hook(SIGNOFF_LIST, "%s %s", from, Reason))
				say("Signoff: %s (%s)", from, Reason);
		}
	}
	message_from(NULL, LOG_CURRENT);
	remove_from_channel(NULL, from, parsing_server());
	notify_mark(from, 0, 0);
	restore_message_from();
}

static	void
p_pong(u_char *from, u_char **ArgList)
{
	int	flag;

	if (!from)
		return;
	flag = double_ignore(from, from_user_host(), IGNORE_CRAP);
	if (flag == IGNORED)
		return;

	if (ArgList[0])
		say("%s: PONG received from %s", ArgList[0], from);
}

static	void
p_error(u_char *from, u_char **ArgList)
{
	PasteArgs(ArgList, 0);
	if (!ArgList[0])
		return;
	say("%s", ArgList[0]);
}

static	void
p_channel(u_char *from, u_char **ArgList)
{
	int	join;
	u_char	*channel;
	int	flag;
	u_char	*s, *ov = NULL;
	int	chan_oper = 0, chan_voice = 0;

	if (!from)
		return;
	flag = double_ignore(from, from_user_host(), IGNORE_CRAP);
	if (my_strcmp(ArgList[0], zero()))
	{
		join = 1;
		channel = ArgList[0];
		/*
		 * this \007 should be \a but a lot of compilers are
		 * broken.  *sigh*  -mrg
		 */
		if ((ov = s = my_index(channel, '\007')))
		{
			*s = '\0';
			ov++;
			while (*++s)
			{
				if (*s == 'o')
					chan_oper = 1;
				if (*s == 'v')
					chan_voice = 1;

			}
		}
		malloc_strcpy(&joined_nick, from);
	}
	else
	{
		channel = zero();
		join = 0;
	}
	if (!my_stricmp(from, server_get_nickname(parsing_server())))
	{
		if (join)
		{
			add_channel(channel, 0, parsing_server(), CHAN_JOINED, NULL);
			send_to_server("MODE %s", channel);
		}
		else
			remove_channel(channel, parsing_server());
	}
	else
	{
		save_message_from();
		message_from(channel, LOG_CRAP);
		if (join)
			add_to_channel(channel, from, parsing_server(), chan_oper, chan_voice);
		else
			remove_from_channel(channel, from, parsing_server());
		restore_message_from();
	}
	if (join)
	{
		if (!get_channel_oper(channel, parsing_server()))
			set_in_on_who(1);
		save_message_from();
		message_from(channel, LOG_CRAP);
		if (flag != IGNORED && do_hook(JOIN_LIST, "%s %s %s", from,
						channel, ov ? ov : (u_char *) ""))
		{
			if (from_user_host())
				if (ov && *ov)
					say("%s (%s) has joined channel %s +%s", from,
				    from_user_host(), channel, ov);
				else
					say("%s (%s) has joined channel %s", from,
				    from_user_host(), channel);
			else
				if (ov && *ov)
					say("%s has joined channel %s +%s", from, 
					channel, ov);
				else
					say("%s has joined channel %s", from, channel);
		}
		restore_message_from();
		set_in_on_who(0);
	}
}

static	void
p_invite(u_char *from, u_char **ArgList)
{
	u_char	*high;
	int	flag;

	if (!from)
		return;
	flag = double_ignore(from, from_user_host(), IGNORE_INVITES);
	switch (flag)
	{
	case IGNORED:
		if (get_int_var(SEND_IGNORE_MSG_VAR))
			send_to_server("NOTICE %s :%s is ignoring you",
				from, server_get_nickname(parsing_server()));
		return;
	case HIGHLIGHTED:
		high = highlight_str();
		break;
	default:
		high = empty_string();
		break;
	}
	if (ArgList[0] && ArgList[1])
	{
		if ((flag != DONT_IGNORE) && (ignore_usernames() & IGNORE_INVITES)
		    && !from_user_host())
			add_to_whois_queue(from, whois_ignore_invites,
					"%s", ArgList[1]);
		else
		{
			save_message_from();
			message_from(from, LOG_CRAP);
			if (do_hook(INVITE_LIST, "%s %s", from, ArgList[1]))
				say("%s%s%s invites you to channel %s", high,
						from, high, ArgList[1]);
			restore_message_from();
			set_invite_channel(ArgList[1]);
			set_recv_nick(from);
		}
	}
}

static	void
p_server_kill(u_char *from, u_char **ArgList)
{
	/*
	 * this is so bogus checking for a server name having a '.'
	 * in it - phone, april 1993.
	 */
	if (my_index(from, '.'))
		say("You have been rejected by server %s", from);
	else
		say("You have been killed by operator %s %s", from,
			ArgList[1] ? ArgList[1] : (u_char *) "(No Reason Given)");
	close_server(parsing_server(), empty_string());
	window_check_servers();
	if (!connected_to_server())
		say("Use /SERVER to reconnect to a server");
}

static	void
p_ping(u_char **ArgList)
{
	PasteArgs(ArgList, 0);
	send_to_server("PONG :%s", ArgList[0]);
}

static	void
p_nick(u_char *from, u_char **ArgList)
{
	int	one_prints = 0,
		its_me = 0;
	u_char	*chan;
	u_char	*line;
	int	flag;

	if (!from)
		return;
	flag = double_ignore(from, from_user_host(), IGNORE_CRAP);
	line = ArgList[0];
	if (my_stricmp(from, server_get_nickname(parsing_server())) == 0){
		if (parsing_server() == get_primary_server())
			set_nickname(line);
		server_set_nickname(parsing_server(), line);
		its_me = 1;
	}
	save_message_from();
	if (flag != IGNORED)
	{
		for (chan = walk_channels(from, 1, parsing_server()); chan;
				chan = walk_channels(from, 0, -1))
		{
			message_from(chan, LOG_CRAP);
			if (do_hook(CHANNEL_NICK_LIST, "%s %s %s", chan, from, line))
				one_prints = 1;
		}
		if (one_prints)
		{
			if (its_me)
				message_from(NULL, LOG_CRAP);
			else
				message_from(what_channel(from, parsing_server()), LOG_CRAP);
			if (do_hook(NICKNAME_LIST, "%s %s", from, line))
				say("%s is now known as %s", from, line);
		}
	}
	rename_nick(from, line, parsing_server());
	if (my_stricmp(from, line))
	{
		message_from(NULL, LOG_CURRENT);
		notify_mark(from, 0, 0);
		notify_mark(line, 1, 0);
	}
	restore_message_from();
}

static	void
p_mode(u_char *from, u_char **ArgList)
{
	u_char	*channel;
	u_char	*line;
	int	flag;

	if (!from)
		return;
	flag = double_ignore(from, from_user_host(), IGNORE_CRAP);
	PasteArgs(ArgList, 1);
	channel = ArgList[0];
	line = ArgList[1];
	save_message_from();
	message_from(channel, LOG_CRAP);
	if (channel && line)
	{
		if (is_channel(channel))
		{
			if (flag != IGNORED && do_hook(MODE_LIST, "%s %s %s",
					from, channel, line))
				say("Mode change \"%s\" on channel %s by %s",
						line, channel, from);
			update_channel_mode(channel, parsing_server(), line);
		}
		else
		{
			if (flag != IGNORED && do_hook(MODE_LIST, "%s %s %s",
					from, channel, line))
				say("Mode change \"%s\" for user %s by %s",
						line, channel, from);
			update_user_mode(line);
		}
		update_all_status();
	}
	restore_message_from();
}

static	void
p_kick(u_char *from, u_char **ArgList)
{
	u_char	*channel,
		*who,
		*comment;

	if (!from)
		return;
	channel = ArgList[0];
	who = ArgList[1];
	comment = ArgList[2];

	if (channel && who)
	{
		save_message_from();
		message_from(channel, LOG_CRAP);
		if (my_stricmp(who, server_get_nickname(parsing_server())) == 0)
		{
			if (comment && *comment)
			{
				if (do_hook(KICK_LIST, "%s %s %s %s", who,
						from, channel, comment))
					say("You have been kicked off channel %s by %s (%s)",
						channel, from, comment);
			}
			else
			{
				if (do_hook(KICK_LIST, "%s %s %s", who, from,
						channel))
					say("You have been kicked off channel %s by %s",
						channel, from);
			}
			remove_channel(channel, parsing_server());
			update_all_status();
		}
		else
		{
			if (comment && *comment)
			{
				if (do_hook(KICK_LIST, "%s %s %s %s", who,
						from, channel, comment))
					say("%s has been kicked off channel %s by %s (%s)",
						who, channel, from, comment);
			}
			else
			{
				if (do_hook(KICK_LIST, "%s %s %s", who, from,
						channel))
					say("%s has been kicked off channel %s by %s",
						who, channel, from);
			}
			remove_from_channel(channel, who, parsing_server());
		}
		restore_message_from();
	}
}

static	void
p_part(u_char *from, u_char **ArgList)
{
	u_char	*channel;
	u_char	*comment;
	int	flag;

	if (!from)
		return;
	flag = double_ignore(from, from_user_host(), IGNORE_CRAP);
	channel = ArgList[0];
	if (!is_on_channel(channel, parsing_server(), from))
		return;
	comment = ArgList[1];
	if (!comment)
		comment = empty_string();
	set_in_on_who(1);
	if (flag != IGNORED)
	{
		save_message_from();
		message_from(channel, LOG_CRAP);
		if (do_hook(LEAVE_LIST, "%s %s %s", from, channel, comment))
		{
			if (comment && *comment != '\0')
				say("%s has left channel %s (%s)", from, channel, comment);
			else
				say("%s has left channel %s", from, channel);
		}
		restore_message_from();
	}
	if (my_stricmp(from, server_get_nickname(parsing_server())) == 0)
		remove_channel(channel, parsing_server());
	else
		remove_from_channel(channel, from, parsing_server());
	set_in_on_who(0);
}


void
irc2_parse_server(u_char *line)
{
	u_char	*from,
		*comm,
		*end,
		*copy = NULL;
	int	numeric;
	u_char	**ArgList;
	u_char	*TrueArgs[MAXPARA + 1];

	if (NULL == line)
		return;

	end = my_strlen(line) + line;
	if (*--end == '\n')
		*end-- = '\0';
	if (*end == '\r')
		*end-- = '\0';

	if (*line == ':')
	{
		if (!do_hook(RAW_IRC_LIST, "%s", line + 1))
			return;
	}
	else if (!do_hook(RAW_IRC_LIST, "%s %s", "*", line))
		return;

	malloc_strcpy(&copy, line);
	ArgList = TrueArgs;
	BreakArgs(copy, &from, ArgList);

	if (!(comm = (*ArgList++)))
		return;		/* Empty line from server - ByeBye */

	/*
	 * only allow numbers 1 .. 999.
	 */
	if ((numeric = my_atoi(comm)) > 0 && numeric < 1000)
		numbered_command(from, numeric, ArgList);
	else if (my_strcmp(comm, "NAMREPLY") == 0)
		funny_namreply(from, ArgList);
	else if (my_strcmp(comm, "WHOREPLY") == 0)
		whoreply(from, ArgList);
	else if (my_strcmp(comm, "NOTICE") == 0)
		parse_notice(from, ArgList);
	/* everything else is handled locally */
	else if (my_strcmp(comm, "PRIVMSG") == 0)
		p_privmsg(from, ArgList);
	else if (my_strcmp(comm, "JOIN") == 0)
		p_channel(from, ArgList);
	else if (my_strcmp(comm, "PART") == 0)
		p_part(from, ArgList);
	else if (my_strcmp(comm, "CHANNEL") == 0)
		p_channel(from, ArgList);
	else if (my_strcmp(comm, "QUIT") == 0)
		p_quit(from, ArgList);
	else if (my_strcmp(comm, "WALL") == 0)
		p_wall(from, ArgList);
	else if (my_strcmp(comm, "WALLOPS") == 0)
		p_wallops(from, ArgList);
	else if (my_strcmp(comm, "LINREPLY") == 0)
		p_linreply(ArgList);
	else if (my_strcmp(comm, "PING") == 0)
		p_ping(ArgList);
	else if (my_strcmp(comm, "TOPIC") == 0)
		p_topic(from, ArgList);
	else if (my_strcmp(comm, "PONG") == 0)
		p_pong(from, ArgList);
	else if (my_strcmp(comm, "INVITE") == 0)
		p_invite(from, ArgList);
	else if (my_strcmp(comm, "NICK") == 0)
		p_nick(from, ArgList);
	else if (my_strcmp(comm, "KILL") == 0)
		p_server_kill(from, ArgList);
	else if (my_strcmp(comm, "MODE") == 0)
		p_mode(from, ArgList);
	else if (my_strcmp(comm, "KICK") == 0)
		p_kick(from, ArgList);
	else if (my_strcmp(comm, "ERROR") == 0)
		p_error(from, ArgList);
	else if (my_strcmp(comm, "ERROR:") == 0) /* Server bug makes this a must */
		p_error(from, ArgList);
	else
	{
		PasteArgs(ArgList, 0);
		if (from)
			say("Odd server stuff: \"%s %s\" (%s)", comm,
				ArgList[0], from);
		else
			say("Odd server stuff: \"%s %s\"", comm, ArgList[0]);
	}
	new_free(&copy);
}

int
doing_privmsg(void)
{
	return doing_privmsg_active;
}

u_char *
from_user_host(void)
{
	return FromUserHost;
}

void
set_from_user_host(u_char *fuh)
{
	FromUserHost = fuh;
}

u_char *
get_joined_nick(void)
{
	return joined_nick;
}

u_char *
get_public_nick(void)
{
	return public_nick;
}
