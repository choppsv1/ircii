/*
 * numbers.c: handles all those strange numeric response dished out by that
 * wacky, nutty program we call ircd 
 *
 * Written by Michael Sandrof
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
IRCII_RCSID("@(#)$eterna: numbers.c,v 1.95 2014/08/14 01:51:25 mrg Exp $");

#include "input.h"
#include "edit.h"
#include "ircaux.h"
#include "vars.h"
#include "lastlog.h"
#include "hook.h"
#include "server.h"
#include "whois.h"
#include "numbers.h"
#include "window.h"
#include "screen.h"
#include "output.h"
#include "names.h"
#include "whois.h"
#include "funny.h"
#include "parse.h"
#include "notice.h"

static	void	reset_nickname(u_char *);
static	void	password_sendline(u_char *, u_char *);
static	void	get_password(void);
static	void	nickname_sendline(u_char *, u_char *);
static	void	channel_topic(u_char *, u_char **);
static	void	not_valid_channel(u_char *, u_char **);
static	void	cannot_join_channel(u_char *, u_char **);
static	void	version(u_char *, u_char **);
static	void	invite(u_char *, u_char **);

static	int	already_doing_reset_nickname = 0;
static	int	current_numeric_local;	/* this is negative of the
					 * current numeric! */

/*
 * numeric_banner: This returns in a static string of either "xxx" where
 * xxx is the current numeric, or "***" if SHOW_NUMBERS is OFF 
 */
u_char	*
numeric_banner(void)
{
	static	u_char	thing[4];

	if (get_int_var(SHOW_NUMERICS_VAR))
		snprintf(CP(thing), sizeof thing, "%3.3u", -current_numeric());
	else
		my_strcpy(thing, "***");
	return (thing);
}


/*
 * display_msg: handles the displaying of messages from the variety of
 * possible formats that the irc server spits out.  you'd think someone would
 * simplify this 
 */
void
display_msg(u_char *from, u_char **ArgList)
{
	u_char	*ptr;
	u_char	*rest;

	rest = PasteArgs(ArgList, 0);
	if (from && (my_strnicmp(server_get_itsname(parsing_server()), from,
			my_strlen(server_get_itsname(parsing_server()))) == 0))
		from = NULL;
	if ((ptr = (u_char *) my_index(rest, ':')) != NULL)
	{
		*(ptr++) = (u_char) 0;
		if (my_strlen(rest))
		{
			if (from)
				put_it("%s %s: %s (from %s)", numeric_banner(),
					rest, ptr, from);
			else
				put_it("%s %s: %s", numeric_banner(), rest,
					ptr);
		}
		else
		{
			if (from)
				put_it("%s %s (from %s)", numeric_banner(),
					ptr, from);
			else
				put_it("%s %s", numeric_banner(), ptr);
		}
	}
	else
	{
		if (from)
			put_it("%s %s (from %s)", numeric_banner(), rest, from);
		else
			put_it("%s %s", numeric_banner(), rest);
	}
}

/*
 * password_sendline: called by send_line() in get_password() to handle
 * hitting of the return key, etc 
 */
static	void
password_sendline(u_char *data, u_char *line)
{
	int	new_server;

	new_server = my_atoi(data);
	server_set_password(new_server, line);
	connect_to_server(server_get_name(new_server),
		server_get_port(new_server), server_get_nickname(new_server), -1);
}

/*
 * get_password: when a host responds that the user needs to supply a
 * password, it gets handled here!  the user is prompted for a password and
 * then reconnection is attempted with that password.  but, the reality of
 * the situation is that no one really uses user passwords.  ah well 
 */
static	void
get_password(void)
{
	u_char	server_num[8];

	say("password required for connection to server %s",
		server_get_name(parsing_server()));
	close_server(parsing_server(), empty_string());
	if (!term_basic())
	{
		snprintf(CP(server_num), sizeof server_num, "%d", parsing_server());
		add_wait_prompt(UP("Server Password:"), password_sendline,
			server_num, WAIT_PROMPT_LINE);
	}
}

static	void
nickname_sendline(u_char *data, u_char *nick)
{
	int	new_server, server;

	new_server = my_atoi(data);
	server = parsing_server();
	set_from_server(new_server);
	if (nick && *nick)
	{
		send_to_server("NICK %s", nick);
		if (new_server == get_primary_server())
			set_nickname(nick);
		server_set_nickname(new_server, nick);
	}
	set_from_server(server);
	already_doing_reset_nickname = 0;
	update_all_status();
}

/*
 * reset_nickname: when the server reports that the selected nickname is not
 * a good one, it gets reset here. 
 */
static	void
reset_nickname(u_char *from)
{
	u_char	server_num[10];
	const	int	from_server = get_from_server();

	if (already_doing_reset_nickname ||
	    is_server_connected(from_server) ||
	    !server_get_attempting_to_connect(from_server))
		return;

	say("You have specified an illegal nickname");
	if (!term_basic() && !get_int_var(NO_ASK_NICKNAME_VAR))
	{
		already_doing_reset_nickname = 1;
		say("Please enter your nickname");
		snprintf(CP(server_num), sizeof server_num, "%d", parsing_server());
		add_wait_prompt(UP("Nickname: "), nickname_sendline, server_num,
			WAIT_PROMPT_LINE);
	}
	update_all_status();
}

static	void
channel_topic(u_char *from, u_char **ArgList)
{
	u_char	*topic, *channel;

	save_message_from();
	if (ArgList[1] && is_channel(ArgList[0]))
	{
		topic = ArgList[1];
		channel = ArgList[0];
		message_from(channel, LOG_CRAP);
		put_it("%s Topic for %s: %s", numeric_banner(), channel,
			topic);
	}
	else
	{
		message_from(NULL, LOG_CURRENT);
		PasteArgs(ArgList, 0);
		put_it("%s Topic: %s", numeric_banner(), ArgList[0]);
	}
	restore_message_from();
}

static	void
not_valid_channel(u_char *from, u_char **ArgList)
{
	u_char	*channel;
	u_char	*s;

	if (!(channel = ArgList[0]) || !ArgList[1])
		return;
	PasteArgs(ArgList, 1);
	s = server_get_name(parsing_server());
	if (0 == my_strnicmp(s, from, my_strlen(s)))
	{
		remove_channel(channel, parsing_server());
		put_it("%s %s %s", numeric_banner(), channel, ArgList[1]);
	}
}

static	void
cannot_join_channel(u_char *from, u_char **ArgList)
{
	u_char	*chan;
	u_char	buffer[BIG_BUFFER_SIZE];

	if (ArgList[0])
		chan = ArgList[0];
	else
		return;

	if (!is_on_channel(chan, parsing_server(),
			server_get_nickname(parsing_server())))
		remove_channel(chan, parsing_server());
	else
		return;

	PasteArgs(ArgList, 0);
	if (do_hook(current_numeric(), "%s %s", from, *ArgList)) {
		my_strmcpy(buffer, ArgList[0], sizeof buffer);
		switch(-current_numeric())
		{
		case 471:		/* #define ERR_CHANNELISFULL    471 */
			my_strmcat(buffer, " (Channel is full)", sizeof buffer);
			break;
		case 473:		/* #define ERR_INVITEONLYCHAN   473 */
			my_strmcat(buffer, " (Invite only channel)", sizeof buffer);
			break;
		case 474:		/* #define ERR_BANNEDFROMCHAN   474 */
			my_strmcat(buffer, " (Banned from channel)", sizeof buffer);
			break;
		case 475: 		/* #define ERR_BADCHANNELKEY    475 */
			my_strmcat(buffer, " (Bad channel key)", sizeof buffer);
			break;
		case 476:		/* #define ERR_BADCHANMASK      476 */
			my_strmcat(buffer, " (Bad channel mask)", sizeof buffer);
			break;
		}
		put_it("%s %s", numeric_banner(), buffer);
	}
}


static	void
version(u_char *from, u_char **ArgList)
{
	if (ArgList[2])
	{
		PasteArgs(ArgList, 2);
		put_it("%s Server %s: %s %s", numeric_banner(), ArgList[1],
			ArgList[0], ArgList[2]);
	}
	else
	{
		PasteArgs(ArgList, 1);
		put_it("%s Server %s: %s", numeric_banner(), ArgList[1],
			ArgList[0]);
	}
}


static	void
invite(u_char *from, u_char **ArgList)
{
	u_char	*who,
		*channel;

	if ((who = ArgList[0]) && (channel = ArgList[1]))
	{
		save_message_from();
		message_from(channel, LOG_CRAP);
		if (do_hook(current_numeric(), "%s %s %s", from, who, channel))
			put_it("%s Inviting %s to channel %s",
					numeric_banner(), who, channel);
		restore_message_from();
	}
}


/*
 * numbered_command: does (hopefully) the right thing with the numbered
 * responses from the server.  I wasn't real careful to be sure I got them
 * all, but the default case should handle any I missed (sorry) 
 */
void
numbered_command(u_char *from, int comm, u_char **ArgList)
{
	u_char	*user;
	u_char	none_of_these = 0;
	u_char	blah[BIG_BUFFER_SIZE];
	int	flag,
		lastlog_level;
	const	int	from_server = parsing_server();
#if 0
	int	user_cnt,
		inv_cnt,
		server_cnt;
#endif /* 0 */

	if (!from || !*from)
		return;
	if (!*ArgList[0])
		user = NULL;
	else
		user = ArgList[0];
	if (!ArgList[1])
		return;
	lastlog_level = set_lastlog_msg_level(LOG_CRAP);
	save_message_from();
	message_from(NULL, LOG_CRAP);
	ArgList++;
	current_numeric_local = -comm;	/* must be negative of numeric! */
	switch (comm)
	{
	case 001:	/* #define RPL_WELCOME          001 */
		PasteArgs(ArgList, 0);
		if (my_strcmp(user, server_get_nickname(from_server)) != 0)
		{
			yell("=== Setting this servers nickname to \"%s\" from \"%s\"", user, server_get_nickname(from_server));
			server_set_nickname(from_server, user);
		}
		if (do_hook(current_numeric(), "%s %s", from, *ArgList)) 
			display_msg(from, ArgList);
		clean_whois_queue();
		break;
	case 002:	/* #define RPL_YOURHOST         002 */
		PasteArgs(ArgList, 0);
		snprintf(CP(blah), sizeof blah, "*** %s", ArgList[0]);
		got_initial_version(blah);
		if (do_hook(current_numeric(), "%s %s", from, *ArgList))
			display_msg(from, ArgList);
		break;

/* should do something with this some day, 2.8 had channel/user mode switches */
	case 004:	/* #define RPL_MYINFO           004 */
		PasteArgs(ArgList, 0);
		if (do_hook(current_numeric(), "%s %s", from, *ArgList))
			display_msg(from, ArgList);
		break;

/*
 * this part of ircii has been broken for most of ircd 2.7, so someday I'll
 * make it work for ircd 2.8 ...  phone..
 */
#if 0
	case 251:		/* #define RPL_LUSERCLIENT      251 */
		display_msg(from, ArgList);
		if (is_server_connected(from_server))
			break;
		if (from_server == get_primary_server() && ((sscanf(ArgList[1],
		    "There are %d users and %d invisible on %d servers",
		    &user_cnt, &inv_cnt, &server_cnt) == 3)||(sscanf(ArgList[1],
		    "There are %d users and %d invisible on %d servers",
		    &user_cnt, &inv_cnt, &server_cnt) == 3)))
		{
			user_cnt =+ inv_cnt;
			if ((server_cnt < get_int_var(MINIMUM_SERVERS_VAR)) ||
			    (user_cnt < get_int_var(MINIMUM_USERS_VAR)))
			{
				say("Trying better populated server...");
				get_connected(from_server + 1);
			}
		}
		break;
#endif /* 0 */
	case 301:		/* #define RPL_AWAY             301 */
		user_is_away(from, ArgList);
		break;

	case 302:		/* #define RPL_USERHOST         302 */
		userhost_returned(from, ArgList);
		break;

	case 303:		/* #define RPL_ISON             303 */
		ison_returned(from, ArgList);
		break;

	case 311:		/* #define RPL_WHOISUSER        311 */
		whois_name(from, ArgList);
		break;

	case 312:		/* #define RPL_WHOISSERVER      312 */
		whois_server(from, ArgList);
		break;

	case 313:		/* #define RPL_WHOISOPERATOR    313 */
		whois_oper(from, ArgList);
		break;

	case 314:		/* #define RPL_WHOWASUSER       314 */
		whowas_name(from, ArgList);
		break;

	case 316:		/* #define RPL_WHOISCHANOP      316 */
		whois_chop(from, ArgList);
		break;

	case 317:		/* #define RPL_WHOISIDLE        317 */
		whois_lastcom(from, ArgList);
		break;

	case 318:		/* #define RPL_ENDOFWHOIS       318 */
		end_of_whois(from, ArgList);
		break;

	case 319:		/* #define RPL_WHOISCHANNELS    319 */
		whois_channels(from, ArgList);
		break;

	case 321:		/* #define RPL_LISTSTART        321 */
		ArgList[0] = UP("Channel\0Users\0Topic");
		ArgList[1] = ArgList[0] + 8;
		ArgList[2] = ArgList[1] + 6;
		ArgList[3] = NULL;
		funny_list(from, ArgList);
		break;

	case 322:		/* #define RPL_LIST             322 */
		funny_list(from, ArgList);
		break;

	case 324:		/* #define RPL_CHANNELMODEIS    324 */
		funny_mode(from, ArgList);
		break;

	case 341:		/* #define RPL_INVITING         341 */
		invite(from, ArgList);
		break;

	case 352:		/* #define RPL_WHOREPLY         352 */
		whoreply(NULL, ArgList);
		break;

	case 353:		/* #define RPL_NAMREPLY         353 */
		funny_namreply(from, ArgList);
		break;

	case 366:		/* #define RPL_ENDOFNAMES       366 */
		{
			u_char	*tmp = NULL,
				*chan;

			PasteArgs(ArgList, 0);
			malloc_strcpy(&tmp, ArgList[0]);
			chan = next_arg(tmp, 0);
			flag = do_hook(current_numeric(), "%s %s", from, ArgList[0]);
			
			if (flag &&
			    channel_mode_lookup(chan, CHAN_NAMES | CHAN_MODE, 0) &&
			    get_int_var(SHOW_END_OF_MSGS_VAR) && flag)
				display_msg(from, ArgList);

			new_free(&tmp);
		}
		break;

	case 381: 		/* #define RPL_YOUREOPER        381 */
		PasteArgs(ArgList, 0);
		if (do_hook(current_numeric(), "%s %s", from, *ArgList))
			display_msg(from, ArgList);
		server_set_operator(parsing_server(), 1);
		update_all_status();	/* fix the status line */
		break;

	case 401:		/* #define ERR_NOSUCHNICK       401 */
		no_such_nickname(from, ArgList);
		break;

	case 405:		/* #define ERR_TOOMANYCHANNELS  405 */
		remove_channel(ArgList[0], parsing_server());
		break;
		
	case 421:		/* #define ERR_UNKNOWNCOMMAND   421 */
		if (check_screen_redirect(ArgList[0]))
			break;
		if (check_wait_command(ArgList[0]))
			break;
		PasteArgs(ArgList, 0);
		flag = do_hook(current_numeric(), "%s %s", from, *ArgList);
		if (!my_strncmp("ISON", *ArgList, 4) || !my_strncmp("USERHOST",
		    *ArgList, 8))
		{
			server_set_2_6_2(parsing_server(), 0);
			convert_to_whois();
		}
		else if (flag)
			display_msg(from, ArgList);
		break;

	case 432:		/* #define ERR_ERRONEUSNICKNAME 432 */
	case 433:		/* #define ERR_NICKNAMEINUSE    433 */ 
		PasteArgs(ArgList, 0);
		if (do_hook(current_numeric(), "%s %s", from, *ArgList))
			display_msg(from, ArgList);
		reset_nickname(from);
		break;

	case 437:		/* #define ERR_UNAVAILRESOURCE  437 */
		PasteArgs(ArgList, 0);
		if (do_hook(current_numeric(), "%s %s", from, *ArgList))
			display_msg(from, ArgList);
		if (!is_channel(*ArgList))
			reset_nickname(from);
		break;

	case 463:		/* #define ERR_NOPERMFORHOST    463 */
		display_msg(from, ArgList);
		close_server(parsing_server(), empty_string());
		window_check_servers();
		if (!connected_to_server())
			get_connected(parsing_server() + 1);
		break;

	case 464:		/* #define ERR_PASSWDMISMATCH   464 */
		PasteArgs(ArgList, 0);
		flag = do_hook(current_numeric(), "%s %s", from, ArgList[0]);
		if (server_get_oper_command())
		{
			if (flag)
				display_msg(from, ArgList);
		}
		else
			get_password();
		break;

	case 465:		/* #define ERR_YOUREBANNEDCREEP 465 */
		{
			int	klined_server = parsing_server();

			PasteArgs(ArgList, 0);
			if (do_hook(current_numeric(), "%s %s", from, ArgList[0]))
				display_msg(from, ArgList);
			close_server(parsing_server(), empty_string());
			window_check_servers();
			if (number_of_servers() > 1)
				remove_from_server_list(klined_server);
			if (!connected_to_server())
				say("You are not connected to a server. Use /SERVER to connect.");
			break;
		}

	case 471:		/* #define ERR_CHANNELISFULL    471 */
	case 473:		/* #define ERR_INVITEONLYCHAN   473 */
	case 474:		/* #define ERR_BANNEDFROMCHAN   474 */
	case 475: 		/* #define ERR_BADCHANNELKEY    475 */
	case 476:		/* #define ERR_BADCHANMASK      476 */
		cannot_join_channel(from, ArgList);
		break;

	case 484:		/* #define ERR_RESTRICTED       484 */
		if (do_hook(current_numeric(), "%s %s", from, *ArgList))
			display_msg(from, ArgList);
		server_set_flag(parsing_server(), USER_MODE_R, 1);
		break;

		/*
		 * The following accumulates the remaining arguments
		 * in ArgSpace for hook detection.  We can't use
		 * PasteArgs here because we still need the arguments
		 * separated for use elsewhere.
		 */
	default:
		{
			u_char	*ArgSpace = NULL;
			int	i,
				do_message_from = 0;
			size_t	len;

			for (i = len = 0; ArgList[i]; len += my_strlen(ArgList[i++]))
				;
			len += (i - 1);
			ArgSpace = new_malloc(len + 1);
			ArgSpace[0] = '\0';
			/* this is cheating */
			if (ArgList[0] && is_channel(ArgList[0]))
				do_message_from = 1;
			for (i = 0; ArgList[i]; i++)
			{
				if (i)
					my_strcat(ArgSpace, " ");
				my_strcat(ArgSpace, ArgList[i]);
			}
			if (do_message_from)
				message_from(ArgList[0], LOG_CRAP);
			i = do_hook(current_numeric(), "%s %s", from, ArgSpace);
			new_free(&ArgSpace);
			if (do_message_from)
				restore_message_from();
			if (i == 0)
				goto done;
			none_of_these = 1;
		}
	}
	/* the following do not hurt the ircII if intercepted by a hook */
	if (none_of_these)
	{
		switch (comm)
		{
		case 221: 		/* #define RPL_UMODEIS          221 */
			put_it("%s Your user mode is \"%s\"", numeric_banner(),
				ArgList[0]);
			break;

		case 242:		/* #define RPL_STATSUPTIME      242 */
			PasteArgs(ArgList, 0);
			if (from && !my_strnicmp(server_get_itsname(parsing_server()),
			    from, my_strlen(server_get_itsname(parsing_server()))))
				from = NULL;
			if (from)
				put_it("%s %s from (%s)", numeric_banner(),
					*ArgList, from);
			else
				put_it("%s %s", numeric_banner(), *ArgList);
			break;

		case 332:		/* #define RPL_TOPIC            332 */
			channel_topic(from, ArgList);
			break;

		case 351:		/* #define RPL_VERSION          351 */
			version(from, ArgList);
			break;

		case 364:		/* #define RPL_LINKS            364 */
			if (ArgList[2])
			{
				PasteArgs(ArgList, 2);
				put_it("%s %-20s %-20s %s", numeric_banner(),
					ArgList[0], ArgList[1], ArgList[2]);
			}
			else
			{
				PasteArgs(ArgList, 1);
				put_it("%s %-20s %s", numeric_banner(),
					ArgList[0], ArgList[1]);
			}
			break;

		case 372:		/* #define RPL_MOTD             372 */
			if (!get_int_var(SUPPRESS_SERVER_MOTD_VAR) ||
			    !server_get_motd(parsing_server()))
			{
				PasteArgs(ArgList, 0);
				put_it("%s %s", numeric_banner(), ArgList[0]);
			}
			break;

		case 375:		/* #define RPL_MOTDSTART        375 */
			if (!get_int_var(SUPPRESS_SERVER_MOTD_VAR) ||
			    !server_get_motd(parsing_server()))
			{
				PasteArgs(ArgList, 0);
				put_it("%s %s", numeric_banner(), ArgList[0]);
			}
			break;

		case 376:		/* #define RPL_ENDOFMOTD        376 */
			if (server_get_attempting_to_connect(parsing_server()))
				got_initial_version(UP("*** Your host is broken and not running any version"));
			if (get_int_var(SHOW_END_OF_MSGS_VAR) &&
			    (!get_int_var(SUPPRESS_SERVER_MOTD_VAR) ||
			    !server_get_motd(parsing_server())))
			{
				PasteArgs(ArgList, 0);
				put_it("%s %s", numeric_banner(), ArgList[0]);
			}
			server_set_motd(parsing_server(), 0);
			break;

		case 384:		/* #define RPL_MYPORTIS         384 */
			PasteArgs(ArgList, 0);
			put_it("%s %s %s", numeric_banner(), ArgList[0], user);
			break;

		case 385:		/* #define RPL_NOTOPERANYMORE   385 */
			server_set_operator(parsing_server(), 0);
			display_msg(from, ArgList);
			update_all_status();
			break;

		case 403:		/* #define ERR_NOSUCHCHANNEL    403 */
			not_valid_channel(from, ArgList);
			break;

		case 451:		/* #define ERR_NOTREGISTERED    451 */
	/*
	 * Sometimes the server doesn't catch the USER line, so
	 * here we send a simplified version again  -lynx 
	 */
			send_to_server("USER %s %s . :%s", my_username(),
				irc_umode(), my_realname());
			send_to_server("NICK %s",
				server_get_nickname(parsing_server()));
			break;

		case 462:		/* #define ERR_ALREADYREGISTRED 462 */
			display_msg(from, ArgList);
			break;

#define RPL_CLOSEEND         363
#define RPL_SERVLISTEND      235
		case 315:		/* #define RPL_ENDOFWHO         315 */
		case 323:               /* #define RPL_LISTEND          323 */
			funny_print_widelist();

		case 219:		/* #define RPL_ENDOFSTATS       219 */
		case 232:		/* #define RPL_ENDOFSERVICES    232 */
		case 365:		/* #define RPL_ENDOFLINKS       365 */
		case 368:		/* #define RPL_ENDOFBANLIST     368 */
		case 369:		/* #define RPL_ENDOFWHOWAS      369 */
		case 374:		/* #define RPL_ENDOFINFO        374 */
#if 0	/* this case needs special handing - see above */
		case 376:		/* #define RPL_ENDOFMOTD        376 */
#endif /* 0 */
		case 394:		/* #define RPL_ENDOFUSERS       394 */
			if (!get_int_var(SHOW_END_OF_MSGS_VAR))
				break;
		default:
			display_msg(from, ArgList);
		}
	}
	set_lastlog_msg_level(lastlog_level);
done:
	restore_message_from();
}

int
current_numeric(void)
{
	return current_numeric_local;
}
