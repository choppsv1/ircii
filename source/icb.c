/*
 * icb.c - handles icb connections.
 *
 * written by matthew green
 *
 * copyright (C) 1995-2014.  Matthew R. Green.
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
IRCII_RCSID("@(#)$eterna: icb.c,v 2.96 2014/08/06 19:52:00 mrg Exp $");

/*
 * ICB protocol is weird.  everything is separated by ASCII 001.  there
 * are fortunately few protocol message types (unlike IRC).
 *
 * each message is formatted like:
 *	CTdata
 *
 * where C is one byte interpreted as a message length, including this
 * byte.  this makes the max length size 255.  the T is the packet type,
 * making 256 types of messages available (not many are used).  and data
 * is interpreted on a per-T basis.  normally, within T, there are
 * different fields separtaed by ASCII 001.  most data types have a 
 * specific number of fields (perhaps variable).  see each function below
 * for a description of how these work.  
 */

/*
*** error: [ERROR] 104: Invalid packet type "P" (PRIVMSG blah :?DCC CHAT chat 3406786305 49527?)
 */

#include "ircaux.h"
#include "names.h"
#include "server.h"
#include "output.h"
#include "vars.h"
#include "ircterm.h"
#include "names.h"
#include "parse.h"
#include "screen.h"
#include "server.h"
#include "icb.h"
#include "hook.h"
#include "ignore.h"
#include "flood.h"
#include "whois.h"
#include "notice.h"

/*
 * private data for ICB.
 */
struct icb_server_status {
	/*
	 * some grossness to prime the $chanusers() list.  if this variable
	 * is non-zero we are currently expecting Members: or more output.
	 */
	int	icb_got_group_waiting_for_members;
};
static	void	icb_close_callback(void **);

/* sender functions */
static	void	icb_put_login(u_char *, u_char *, u_char *, u_char *, u_char *);

/* hooks out of "icbcmd" */
static	void	icb_put_msg(u_char *);
static	void	icb_put_beep(u_char *);
static	void	icb_put_boot(u_char *);
static	void	icb_put_cancel(u_char *);
static	void	icb_put_echo(u_char *);
static	void	icb_put_pass(u_char *);
static	void	icb_put_status(u_char *);
static	void	icb_put_pong(u_char *);
static	void	icb_put_command(u_char *);

/* receiver functions */
static	void	icb_got_login(u_char *);
static	void	icb_got_public(u_char *);
static	void	icb_got_msg(u_char *);
static	void	icb_got_status(u_char *);
static	void	icb_got_error(u_char *);
static	void	icb_got_important(u_char *);
static	void	icb_got_exit(u_char *);
static	void	icb_got_cmdout(u_char *);
static	void	icb_got_proto(u_char *);
static	void	icb_got_beep(u_char *);
static	void	icb_got_ping(u_char *);
static	void	icb_got_something(int, u_char *);

/* misc. helper functions */
static	int	icb_split_msg(u_char *, u_char **, int);
static	void	icb_set_fromuserhost(u_char *);
static	void	icb_got_who(u_char *);
static	u_char	*icb_who_idle(u_char *);
static	u_char	*icb_who_signon(u_char *);

/* icb state variables */
static	u_char	*icb_initial_status = UP("iml");

static	int	icb_port_local = ICB_PORT;	/* port of icbd */

static	void
icb_close_callback(void **ptrptr)
{
	new_free(ptrptr);
}

/*
 * these are the functions that interpret incoming ICB packets.
 */

/*
 * login packets fromt the server should be a single field "a"
 */
static void
icb_got_login(u_char *line)
{
	u_char	*server = server_get_name(parsing_server());
	struct icb_server_status *status;

	status = new_malloc(sizeof *status);
	status->icb_got_group_waiting_for_members = 0;

	server_set_version_string(parsing_server(), UP("ICB"));
	server_set_itsname(parsing_server(), server);
	server_set_server_private(parsing_server(), status, icb_close_callback);
	maybe_load_ircrc();
	update_all_status();
	do_hook(CONNECT_LIST, "%s %d", server, server_get_port(parsing_server()));
}

/*
 * public messages have two fields
 *	sender
 *	text
 */
static void
icb_got_public(u_char *line)
{
	u_char	*ap[ICB_GET_PUBLIC_MAXFIELD];
	int	ac, level;
	u_char	*high;

	ac = icb_split_msg(line, ap, ICB_GET_PUBLIC_MAXFIELD);
	if (ac != ICB_GET_PUBLIC_MAXFIELD)
		return;

	save_message_from();
	level = set_lastlog_msg_level(LOG_PUBLIC);
	message_from(server_get_icbgroup(parsing_server()), LOG_PUBLIC);

	switch (double_ignore(ap[0], from_user_host(), IGNORE_PUBLIC))
	{
	case IGNORED:
		goto out;
	case HIGHLIGHTED:
		high = highlight_str();
		break;
	default:
		high = empty_string();
		break;
	}

	if (check_flooding(ap[0], server_get_nickname(parsing_server()), PUBLIC_FLOOD, ap[1]))
	{
		if (do_hook(PUBLIC_LIST, "%s %s %s", ap[0],
		    server_get_icbgroup(parsing_server()), ap[1]))
			put_it("%s<%s>%s %s", high, ap[0], high, ap[1]);
		if (do_beep_on_level(LOG_PUBLIC))
			beep_em(1);
	}
out:
	set_lastlog_msg_level(level);
	restore_message_from();
}

static void
icb_got_msg(u_char *line)
{
	u_char	*ap[ICB_GET_MSG_MAXFIELD];
	int	ac, level;
	u_char	*high;

	ac = icb_split_msg(line, ap, ICB_GET_MSG_MAXFIELD);
	if (ac != ICB_GET_MSG_MAXFIELD)
		return;
	set_recv_nick(ap[0]);
	if (is_away_set())
		beep_em(get_int_var(BEEP_WHEN_AWAY_VAR));
	save_message_from();
	message_from(ap[0], LOG_MSG);
	level = set_lastlog_msg_level(LOG_MSG);

	switch (double_ignore(ap[0], from_user_host(), IGNORE_MSGS))
	{
	case IGNORED:
		goto out;
	case HIGHLIGHTED:
		high = highlight_str();
		break;
	default:
		high = empty_string();
		break;
	}

	if (check_flooding(ap[0], server_get_nickname(parsing_server()), MSG_FLOOD, ap[1]))
	{
		if (do_hook(MSG_LIST, "%s %s", ap[0], ap[1]))
		{
			if (is_away_set())
			{
				time_t t;
				u_char *msg = NULL;
				size_t len = my_strlen(ap[1]) + 20;

				t = time(NULL);
				msg = new_malloc(len);
				snprintf(CP(msg), len, "%s <%.16s>", ap[1], ctime(&t));
				put_it("%s*%s*%s %s", high, ap[0], high, msg);
				new_free(&msg);
			}
			else
				put_it("%s*%s*%s %s", high, ap[0], high, ap[1]);
		}
		if (do_beep_on_level(LOG_MSG))
			beep_em(1);
	}
out:
	set_lastlog_msg_level(level);
	restore_message_from();
}

static const u_char status_match[] = "You are now in group ";
static const u_char signoff_match[] = "Your group moderator signed off.";
static const u_char change_match[] = "Group is now named ";
static const u_char change_match2[] = "renamed group to ";
static const u_char rsvp_match[] = "You are invited to group ";
static const u_char topic_match[] = "changed the topic to \"";
static const u_char nick_match[] = "changed nickname to ";

static void
icb_got_status(u_char *line)
{
	u_char	*ap[ICB_GET_STATUS_MAXFIELD], *group, *space, save;
	int	ac, do_say = 1, level;

/* use these a few times */
#define	KILL_SPACE(what)						\
	do { 								\
		if ((space = my_index(what, ' ')) != NULL) {		\
			save = *space;					\
			*space = 0;					\
		}							\
	} while (0)
#define RESTORE_SPACE							\
	do {								\
		if (space)						\
			*space = save;					\
	} while (0)

	save = 0;
	space = NULL;
	save_message_from();
	level = set_lastlog_msg_level(LOG_CRAP);
	ac = icb_split_msg(line, ap, ICB_GET_STATUS_MAXFIELD);
	if (ac != ICB_GET_STATUS_MAXFIELD)
		goto out;
	if (my_stricmp(ap[0], UP("status")) == 0)
	{
		if (my_strnicmp(ap[1], status_match, sizeof(status_match) - 1) == 0)
		{
			group = ap[1] + sizeof(status_match) - 1;
			KILL_SPACE(group);
			clear_channel_list(parsing_server());
			add_channel(group, 0, parsing_server(), CHAN_JOINED, 0);
			server_set_icbgroup(parsing_server(), group);
			icb_set_fromuserhost(empty_string());
			message_from(group, LOG_CRAP);
			if (do_hook(JOIN_LIST, "%s %s %s", server_get_nickname(parsing_server()), group, empty_string()) == 0)
				do_say = 0;
			if (get_int_var(SHOW_CHANNEL_NAMES_VAR))
				icb_put_funny_stuff(UP("NAMES"), group, NULL);
			RESTORE_SPACE;
		}
		/* leave do_say set */
	}
	else
	if (my_stricmp(ap[0], UP("sign-on")) == 0)
	{
		KILL_SPACE(ap[1]);
		icb_set_fromuserhost(space+1);
		message_from(server_get_icbgroup(parsing_server()), LOG_CRAP);
		if (double_ignore(ap[1], from_user_host(), IGNORE_CRAP) != IGNORED &&
		    do_hook(JOIN_LIST, "%s %s %s", ap[1], server_get_icbgroup(parsing_server()), empty_string()) == 0)
			do_say = 0;
		add_to_channel(server_get_icbgroup(parsing_server()), ap[1], parsing_server(), 0, 0);
		RESTORE_SPACE;
	}
	else
	if (my_stricmp(ap[0], UP("sign-off")) == 0)
	{
		if (my_strnicmp(ap[1], signoff_match, sizeof(signoff_match) - 1) != 0)
		{
			u_char *s = server_get_icbgroup(parsing_server());

			KILL_SPACE(ap[1]);
			icb_set_fromuserhost(space+1);
			message_from(s, LOG_CRAP);
			if (double_ignore(ap[1], from_user_host(), IGNORE_CRAP) != IGNORED &&
			    (do_hook(CHANNEL_SIGNOFF_LIST, "%s %s %s", s, ap[1], s) == 0 ||
			     do_hook(SIGNOFF_LIST, "%s %s", ap[1], s) == 0))
				do_say = 0;
			remove_from_channel(server_get_icbgroup(parsing_server()), ap[1], parsing_server());
			RESTORE_SPACE;
		}
		/* leave do_say set */
	}
	else
	if (my_stricmp(ap[0], UP("arrive")) == 0)
	{
		KILL_SPACE(ap[1]);
		icb_set_fromuserhost(space+1);
		message_from(server_get_icbgroup(parsing_server()), LOG_CRAP);
		if (double_ignore(ap[1], from_user_host(), IGNORE_CRAP) != IGNORED &&
		    do_hook(JOIN_LIST, "%s %s %s", ap[1], server_get_icbgroup(parsing_server()), empty_string()) == 0)
			do_say = 0;
		add_to_channel(server_get_icbgroup(parsing_server()), ap[1], parsing_server(), 0, 0);
		RESTORE_SPACE;
	}
	else
	if (my_stricmp(ap[0], UP("depart")) == 0)
	{
		KILL_SPACE(ap[1]);
		icb_set_fromuserhost(space+1);
		message_from(server_get_icbgroup(parsing_server()), LOG_CRAP);
		if (double_ignore(ap[1], from_user_host(), IGNORE_CRAP) != IGNORED &&
		    do_hook(LEAVE_LIST, "%s %s", ap[1], server_get_icbgroup(parsing_server())) == 0)
			do_say = 0;
		remove_from_channel(server_get_icbgroup(parsing_server()), ap[1], parsing_server());
		RESTORE_SPACE;
	}
	else
	if (my_stricmp(ap[0], UP("change")) == 0)
	{
		int	match2;
		int	len;

		group = 0;
		KILL_SPACE(ap[1]);
		match2 = my_strnicmp(space+1, change_match2, sizeof(change_match2) - 1);
		RESTORE_SPACE;
		if (match2 == 0)
			group = space+1 + sizeof(change_match2) - 1;
		else
		if (my_strnicmp(ap[1], change_match, sizeof(change_match) - 1) == 0)
			group = ap[1] + sizeof(change_match) - 1;

		if (group)
		{
			len = my_strlen(group);
			if (group[len - 1] == '.')
				group[len - 1] = 0;	/* kill the period */

			message_from(group, LOG_CRAP);
			rename_channel(server_get_icbgroup(parsing_server()), group, parsing_server());
			server_set_icbgroup(parsing_server(), group);
			icb_set_fromuserhost(empty_string());
		}
		/* leave do_say set for all cases */
	}
	else
	if (my_stricmp(ap[0], UP("RSVP")) == 0)
	{
		if (my_strnicmp(ap[1], rsvp_match, sizeof(rsvp_match) - 1) == 0)
		{
			group = ap[1] + sizeof(rsvp_match) - 1;
			KILL_SPACE(group);
			set_invite_channel(group);
			RESTORE_SPACE;
		}
		/* leave do_say set */
	}
	else
	if (my_stricmp(ap[0], UP("topic")) == 0)
	{
		KILL_SPACE(ap[1]);
		if (my_strnicmp(space + 1, topic_match, sizeof(topic_match) - 1) == 0)
		{
			u_char	*topic, *lbuf = NULL;

			group = server_get_icbgroup(parsing_server());
			message_from(group, LOG_CRAP);
			topic = space + 1 + sizeof(topic_match) - 1;
			/* trim the final " */
			malloc_strcpy(&lbuf, topic);
			lbuf[my_strlen(lbuf) - 1] = '\0';
			if (do_hook(TOPIC_LIST, "%s %s %s", ap[1], group, lbuf) == 0)
				do_say = 0;
			new_free(&lbuf);
		}
		RESTORE_SPACE;
		/* leave do_say set */
	}
	else
	if (my_stricmp(ap[0], UP("name")) == 0)
	{
		KILL_SPACE(ap[1]);
		if (my_strnicmp(space + 1, nick_match, sizeof(nick_match) - 1) == 0)
		{
			u_char	*new_nick;

			group = server_get_icbgroup(parsing_server());
			message_from(group, LOG_CRAP);
			new_nick = space + 1 + sizeof(nick_match) - 1;
			if (do_hook(CHANNEL_NICK_LIST, "%s %s %s", group, ap[1], new_nick) &&
			    do_hook(NICKNAME_LIST, "%s %s", ap[1], new_nick) == 0)
				do_say = 0;
			rename_nick(ap[1], new_nick, parsing_server());
		}
		RESTORE_SPACE;
		/* leave do_say set */
	}
	/* make default info messages go to the current channel */
	else
		message_from(server_get_icbgroup(parsing_server()), LOG_CRAP);

#if 0
/* these need to be match for /ignore to work */
*** info Sign-on: nick (user@domain.com) entered group
*** info Sign-off: nick (user@domain.com) has signed off.
#endif

	if (do_say && do_hook(ICB_STATUS_LIST, "%s %s", ap[0], ap[1]))
		say("info %s: %s", ap[0], ap[1]);
out:	
	set_lastlog_msg_level(level);
	restore_message_from();
}

static void
icb_set_fromuserhost(u_char *what)
{
	static	u_char	*icb_fromuserhost = NULL;
	u_char	*righty;
	
	if (!what || !*what)
		what = empty_string();
	else if (*what == '(')
		what++;
	malloc_strcpy(&icb_fromuserhost, what);	/* ( for below */
	if ((righty = my_index(icb_fromuserhost, ')')))
		*righty = 0;
	set_from_user_host(icb_fromuserhost);
}

static void
icb_got_error(u_char *line)
{
	int	level;

	save_message_from();
	level = set_lastlog_msg_level(LOG_CRAP);
	message_from(NULL, LOG_CRAP);
	if (do_hook(ICB_ERROR_LIST, "%s", line))
		say("error: %s", line);
	set_lastlog_msg_level(level);
	restore_message_from();
}

static void
icb_got_important(u_char *line)
{
	u_char	*ap[ICB_GET_IMPORTANT_MAXFIELD];
	int	ac, level;

	ac = icb_split_msg(line, ap, ICB_GET_IMPORTANT_MAXFIELD);
	if (ac != ICB_GET_IMPORTANT_MAXFIELD)
		return;
	save_message_from();
	level = set_lastlog_msg_level(LOG_CRAP);
	message_from(NULL, LOG_CRAP);
	if (do_hook(SERVER_NOTICE_LIST, "%s *** %s", ap[0], ap[1]))
		say("Important Message %s: %s", ap[0], ap[1]);
	set_lastlog_msg_level(level);
	restore_message_from();
}

static void
icb_got_exit(u_char *line)
{
	int	level;

	level = set_lastlog_msg_level(LOG_CRAP);
	say("ICB server disconnecting us");
	close_server(parsing_server(), empty_string());
	set_lastlog_msg_level(level);
}

/*
 * command output: we split this up a little ...
 */

static	u_char	*
icb_who_idle(u_char *line)
{
	time_t	idle = (time_t)my_atoi(line);
	static	u_char	lbuf[16];

	if (idle > 99 * 60)
		snprintf(CP(lbuf), sizeof lbuf, "%5dm", (int)idle / 60);
	else
	if (idle < 60)
		snprintf(CP(lbuf), sizeof lbuf, "%d sec", (int)idle);
	else
		snprintf(CP(lbuf), sizeof lbuf, "%d:%02ds", (int)idle / 60, (int)idle % 60);
	/* only has 6 chars */
	lbuf[6] = 0;
	return (lbuf);
}

static	u_char	*
icb_who_signon(u_char *line)
{
	time_t	their_time = (time_t)my_atoi(line);
	u_char	*s = UP(asctime(localtime(&their_time)));

	s[16] = '\0';
	/* Tue Mar  2 05:10:10 1999J */
	/*     <---------->          */
	return (s + 4);
}

static void
icb_got_who(u_char *line)
{
	u_char	*ap[ICB_GET_WHOOUT_MAXFIELD], *arg0;
	int	ac, level;

	/* just get the command */
	ac = icb_split_msg(line, ap, ICB_GET_WHOOUT_MAXFIELD);
	if (ac != ICB_GET_WHOOUT_MAXFIELD)
	{
		yell("--- icb_got_who: split ac(%d) not 8", ac);
		return;
	}
	/* ap[3] is always "0" */
	/* these are: mod?, nick, idle, signon, user, [@]host, status? */
	/* keep format in sync with below */
	save_message_from();
	level = set_lastlog_msg_level(LOG_CRAP);
	message_from(server_get_icbgroup(parsing_server()), LOG_CRAP);
	arg0 = UP(ap[0][0] == 'm' ? "*" : " ");
	if (do_hook(ICB_WHO_LIST, "%s%s %s %s %s %s %s", arg0,
				ap[1], ap[2], ap[4],
				ap[5], ap[6], ap[7]))
		say("%s%-13s %6s %-12s %s@%s %s", arg0, ap[1],
				icb_who_idle(ap[2]),
				icb_who_signon(ap[4]),
				ap[5], ap[6], ap[7]);
	set_lastlog_msg_level(level);
	restore_message_from();
}

static const u_char group_match[] = "Group: ";
static const u_char members_match[] = "    Members: ";

static void icb_cmdout_check_members(u_char *);
static void
icb_cmdout_check_members(u_char *line)
{
	u_char	*group, *nick_list = NULL;
	struct icb_server_status *status;

	status = server_get_server_private(parsing_server());

	if (status->icb_got_group_waiting_for_members)
		status->icb_got_group_waiting_for_members++;
	else if (my_strnicmp(line, UP(members_match),
	    sizeof(members_match) - 1) == 0)
	{
		status->icb_got_group_waiting_for_members = 2;
		line += sizeof(members_match) - 1;
	}
	else
		return;

	/* parse list of nicks, calling add_to_channel() */
	group = server_get_icbgroup(parsing_server());
	malloc_strcpy(&nick_list, line);
	line = nick_list;
	while (line && *line)
	{
		u_char	*next;

		next = my_index(line, ',');
		if (next)
		{
			*next++ = '\0';
			if (*next == ' ')
				next++;
		}
		add_to_channel(group, line, parsing_server(), 0, 0);
		line = next;
	}
	new_free(&nick_list);
}

static void
icb_got_cmdout(u_char *line)
{
	u_char	*ap[2];
	int	ac, level;

	/* just get the command */
	ac = icb_split_msg(line, ap, -2);
 
	save_message_from();
	level = set_lastlog_msg_level(LOG_CRAP);
	message_from(server_get_icbgroup(parsing_server()), LOG_CRAP);
	if (ac == 2)
	{
		if (my_stricmp(ap[0], UP("co")) == 0)
		{
			if (my_strnicmp(ap[1], UP(group_match),
			    sizeof(group_match) - 1) == 0) {
				u_char *s = ap[1] + sizeof(group_match) - 1;

				/* skip "*name*<spaces>" */
				while (!isspace(*s))
					s++;
				while (isspace(*s))
					s++;
				if (*s == '(' && *(s+4) == ')') {
					u_char mode[4];

					mode[0] = s[1];
					mode[1] = s[2];
					mode[2] = s[3];
					mode[3] = 0;
					server_set_icbmode(parsing_server(), mode);
				}
			}
			else 
				icb_cmdout_check_members(ap[1]);

			if (do_hook(ICB_CMDOUT_LIST, "%s", ap[1]))
				say("%s", ap[1]);
		}
		else
		/* who list */
		if (my_stricmp(ap[0], UP("wl")) == 0)
			icb_got_who(ap[1]);
		else
			put_it("[unknown command output %s] %s", ap[0], ap[1]);
	}
	else
	/* head of who list */
	if (my_stricmp(ap[0], UP("wh")) == 0 &&
	    do_hook(ICB_WHO_LIST, "%s %s %s %s %s %s", 
			"Nickname", "Idle", "Sign-On", "Account",
			empty_string(), empty_string()))
		/* keep format in sync with above. */
		say(" %-13s %6s %-12s %s", "Nickname", "Idle",
			"Sign-On", "Account");
	set_lastlog_msg_level(level);
	restore_message_from();
}

/*
 * we send our login to the server now... it has told us we are
 * actually there, time to login.
 */
static void
icb_got_proto(u_char *line)
{
	int	server = parsing_server();
	u_char	*chan = server_get_icbgroup(server);
	u_char	*mode = server_get_icbmode(server);

	if (!chan)
		chan = empty_string();
	if (!mode && !*mode)
		mode = icb_initial_status;
	say("You are wasting time.");
	server_is_connected(server, 1);
	icb_put_login(my_username(),
		server_get_nickname(server),
		chan,
		server_get_password(server),
		mode);
	do_hook(CONNECT_LIST, "%s %d",
	    server_get_name(server), server_get_port(server));
}

/*
 * if we beep, we beep.  tell the user about the annoy packet
 * anyway.
 */
static void
icb_got_beep(u_char *line)
{
	int	level;

	if (get_int_var(BEEP_VAR))
		term_beep();
	save_message_from();
	level = set_lastlog_msg_level(LOG_CRAP);
	message_from(server_get_icbgroup(parsing_server()), LOG_CRAP);
	say("%s wants to annoy you.", line);
	set_lastlog_msg_level(level);
	restore_message_from();
}

/*
 * if we get a ping, send a pong
 */
static void
icb_got_ping(u_char *line)
{
	icb_put_pong(line);
}

/*
 * eek.
 */
static void
icb_got_something(int type, u_char *line)
{
	say("unknown: packet type %d, ignored", type);
}

/*
 * below are functions to send messages to the icb server.
 */

/*
 * for hooks perhaps we should have a "icb <command> <args>"
 * hook, that does 'everything'.  dunno, there are enough
 * irc-like ones to afford just adding a few icb specifics..
 */

/*
 * login packets have from 5 to 7 fields:
 *	username
 *	nickname
 *	default group
 *	login command
 *	password
 *	default group status (optional)
 *	protocol level (optional, deprecated).
 *
 * the login command must be either "login" or "w", to either login
 * to ICB or see who is on.
 */
static void
icb_put_login(u_char *user, u_char *nick, u_char *group, u_char *pass, u_char *status)
{
	u_char *mode, *prefix;

	mode = server_get_icbmode(parsing_server());
	if (group[0] != '.' && group[1] != '.' &&
	    mode && mode[1] == 'i') {
		/* save previous status */
		prefix = UP("..");
	} else
		prefix = UP("");

	send_to_server("%c%s%c%s%c%s%s%c%s%c%s%c%s", ICB_LOGIN,
		user, ICB_SEP,
		nick, ICB_SEP,
		prefix, group, ICB_SEP,
		"login", ICB_SEP,
		pass ? pass : (u_char *) "", ICB_SEP,
		status);
}

/*
 * public packets have 1 field:
 *	text
 */
void
icb_put_public(u_char *line)
{
	int	level;
	size_t	len, remain;
	const	int	from_server = get_from_server();

	if (get_display())
	{
		save_message_from();
		level = set_lastlog_msg_level(LOG_PUBLIC);
		message_from(server_get_icbgroup(from_server), LOG_PUBLIC);
		if (do_hook(SEND_PUBLIC_LIST, "%s %s", server_get_icbgroup(from_server), line))
			put_it("> %s", line);
		set_lastlog_msg_level(level);
		restore_message_from();
	}

	/*
	 * deal with ICB 255 length limits.  we have 255 bytes 
	 * maximum to deal with.  as public messages are send
	 * out with our nickname, we must take away that much,
	 * one for the space after our nick, one for the public
	 * message token, one for the trailing nul, one for the
	 * ^A, and one more for good measure, totalling 5.
	 */
	remain = 250 - my_strlen(server_get_nickname(from_server));
	while (*line) {
		u_char	b[256], *s;
		size_t copylen;

		len = my_strlen(line);
		copylen = remain;
		if (len > remain)
		{
			int i;

			/*
			 * try to find a space in the previous
			 * 10 characters, and split there instead.
			 */
			for (i = 1; i < 11 && i < len; i++)
				if (isspace(line[remain - i]))
				{
					copylen -= i;
					break;
				}
			my_strncpy(b, line, copylen);
			b[copylen] = 0;
			s = b;
		}
		else
			s = line;
		send_to_server("%c%s", ICB_PUBLIC, s);
		line += len > copylen ? copylen : len;
	}
}

/*
 * msg packets have 2 fields:
 *	to		- recipient of message
 *	text
 */
static void
icb_put_msg(u_char *line)
{
	u_char	*to;

	if ((to = next_arg(line, &line)) == NULL)
		line = empty_string();
	icb_put_msg2(to, line);
}

void
icb_put_msg2(u_char *to, u_char *line)
{
	int	level;
	size_t	nlen, mlen, len, remain;

	if (get_display())
	{
		save_message_from();
		level = set_lastlog_msg_level(LOG_MSG);
		message_from(to, LOG_MSG);
		if (do_hook(SEND_MSG_LIST, "%s %s", to, line))
			put_it("-> *%s* %s", to, line);
		set_lastlog_msg_level(level);
		restore_message_from();
	}

	/*
	 * deal with ICB 255 length limits.  we have 255 bytes maximum to
	 * deal with.  as private messages are send out with our nickname,
	 * but are sent in with the target's nickname, we must take away
	 * the larger length of these, plus one for the space after our
	 * nick, one for the command tag, one for the private message
	 * token, one for the separator, one for the trailing nul, one
	 * for the ^A, and one more for good measure, totalling 7.
	 */
	nlen = my_strlen(to);
	mlen = my_strlen(server_get_nickname(get_from_server()));
	if (nlen > mlen)
		remain = 248 - nlen;
	else
		remain = 248 - mlen;
	while (*line)
	{
		u_char	b[256], *s;

		len = my_strlen(line);
		if (len > remain)
		{
			my_strncpy(b, line, remain);
			b[remain] = 0;
			s = b;
		}
		else
			s = line;
		send_to_server("%cm%c%s %s", ICB_COMMAND, ICB_SEP, to, s);
		line += len > remain ? remain : len;
	}
}

static	void
icb_put_beep(u_char *line)
{
	send_to_server("%cbeep%c%s", ICB_COMMAND, ICB_SEP, line);
}

static	void
icb_put_pong(u_char *line)
{
	send_to_server("%c", ICB_PONG);
}

static	void
icb_put_boot(u_char *line)
{
	send_to_server("%cboot%c%s", ICB_COMMAND, ICB_SEP, line);
}

static	void
icb_put_cancel(u_char *line)
{
	send_to_server("%ccancel%c%s", ICB_COMMAND, ICB_SEP, line);
}

static	void
icb_put_echo(u_char *line)
{
	send_to_server("%cechoback%c%s", ICB_COMMAND, ICB_SEP, line);
}

void
icb_put_group(u_char *line)
{
	send_to_server("%cg%c%s", ICB_COMMAND, ICB_SEP, line);
}

void
icb_put_invite(u_char *line)
{
	send_to_server("%cinvite%c%s", ICB_COMMAND, ICB_SEP, line);
}

void
icb_put_motd(u_char *line)
{
	send_to_server("%cmotd%c", ICB_COMMAND, ICB_SEP);
}

void
icb_put_nick(u_char *line)
{
	send_to_server("%cname%c%s", ICB_COMMAND, ICB_SEP, line);
	server_set_nickname(get_window_server(0), line);
}

static	void
icb_put_pass(u_char *line)
{
	send_to_server("%cpass%c%s", ICB_COMMAND, ICB_SEP, line);
}

static	void
icb_put_status(u_char *line)
{
	send_to_server("%cstatus%c%s", ICB_COMMAND, ICB_SEP, line);
}

void
icb_put_topic(u_char *line)
{
	send_to_server("%ctopic%c%s", ICB_COMMAND, ICB_SEP, line);
}

void
icb_put_version(u_char *line)
{
	send_to_server("%cv%c", ICB_COMMAND, ICB_SEP);
}

/*
 * /names & /list support:
 *
 * a bare /names is ICB /who -s
 * a bare /list is ICB /who -g
 * any arguments are just passed, though a "*" (ircII "this channel") is
 * converted into a "." (icb "this channel").
 */
void
icb_put_funny_stuff(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*arg, *arg1;

	if (my_strcmp(command, "NAMES") == 0)
	{
		arg1 = UP("-s");
	}
	else
	if (my_strcmp(command, "LIST") == 0)
	{
		arg1 = UP("-g");
	}
	else
	{
		yell("--- icb_put_funny_stuff: not NAMES or LIST.");
		return;
	}
	if (args && *args)
	{
		u_char	lbuf[255];

		if ((arg = my_index(args, '*')) != NULL && (arg[1] == '\0' || isspace(arg[1])))
			arg[0] = '.';
		/* XXX */
		if (my_strlen(args) > 251)
			args[251] = 0;
		snprintf(CP(lbuf), sizeof lbuf, "%s %s", arg1, args);
		icb_put_who(lbuf);
	}
	else
		icb_put_who(arg1);
		
}

void
icb_put_who(u_char *line)
{
	send_to_server("%cw%c%s", ICB_COMMAND, ICB_SEP, line);
}

static	void
icb_put_command(u_char *line)
{
	yell("--- icb_put_command not implemented yet");
}

void
icb_put_action(u_char *target, u_char *text)
{
	u_char	*s;
	unsigned display;

	s = new_malloc(2 + my_strlen(text) + 2 + 1);
	strcpy(CP(s), "*");
	strcat(CP(s), CP(text));
	strcat(CP(s), "*");
	display = set_display_off();
	if (is_current_channel(target, get_from_server(), 0))
		icb_put_public(s);
	else
		icb_put_msg2(target, s);
	set_display(display);
	new_free(&s);
}

/*
 * icb_split_msg(): split up `msg' into (upto) `ac' parts separate parts,
 * delimited by ICB_SEP, returning each part if to `ap', which points to
 * an array of u_char *'s `ac' long.  if ac is positive, and if there is
 * not enough room, this function returns -1, otherwise the number of
 * parts is returned.  if ac is negative, it is an indication that we
 * might have more fields, but we only want to split abs(ac);
 */
static int
icb_split_msg(u_char *msg, u_char **ap, int ac)
{
	int	myac = 0, shortok = 0;
	u_char	*s;

	if (ac == 0)
		return (-1);
	else if (ac < 0)
	{
		shortok = 1;
		ac = -ac;
	}
	ap[myac++] = msg;
	while ((s = my_index(msg, ICB_SEP)))
	{
		if (myac >= ac)
		{
			if (shortok)
				return (myac);
			else
				return (-1);
		}
		*s = '\0';
		msg = s + 1;
		ap[myac++] = msg;
	}
	return (myac);
}

/*
 * external hooks
 */

/*
 * this function handles connecting to a server, and is called from
 * the guts of the server code.  we could send a login packet here
 * but we defer that until we get a protocol packet.
 */
void
icb_login_to_server(int server)
{

}

/*
 * handle the icb protocol.
 */
void
icb_parse_server(u_char *line)
{
	int	len = (int)(u_char)*line++;
	int	type = (int)*line++;
	struct icb_server_status *status;

	status = server_get_server_private(parsing_server());

	(void)len;
	icb_set_fromuserhost(NULL);
	switch (type)
	{
	case ICB_LOGIN:
		icb_got_login(line);
		break;
	case ICB_PUBLIC:
		icb_got_public(line);
		break;
	case ICB_PERSONAL:
		icb_got_msg(line);
		break;
	case ICB_STATUS:
		icb_got_status(line);
		break;
	case ICB_ERROR:
		icb_got_error(line);
		break;
	case ICB_IMPORTANT:
		icb_got_important(line);
		break;
	case ICB_EXIT:
		icb_got_exit(line);
		break;
	case ICB_CMDOUT:
		icb_got_cmdout(line);
		break;
	case ICB_PROTO:
		icb_got_proto(line);
		break;
	case ICB_BEEP:
		icb_got_beep(line);
		break;
	case ICB_PING:
		icb_got_ping(line);
		break;
	default:
		icb_got_something(type, line);
		break;
	}
	if (status && status->icb_got_group_waiting_for_members)
		status->icb_got_group_waiting_for_members--;
}

/*
 * hook for user to send anything so the icb server; we only
 * provide one of these entry points for the user.  let the
 * rest be in scripts, or via some other IRC-like command
 * eg, send_text() will need to call into public/msg.
 *
 * format of this:
 *	type arg1 arg2 arg3 arg4 ...
 */
void
icbcmd(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*type;
	size_t	len;
	
	if ((type = next_arg(args, &args)) != NULL)
	{
		len = my_strlen(type);
		upper(type);

		if (my_strncmp(type, "PUBLIC", len) == 0)
			icb_put_public(args);
		else
		if (my_strncmp(type, "MSG", len) == 0)
			icb_put_msg(args);
		else
		if (my_strncmp(type, "BEEP", len) == 0)
			icb_put_beep(args);
		else
		if (my_strncmp(type, "BOOT", len) == 0)
			icb_put_boot(args);
		else
		if (my_strncmp(type, "CANCEL", len) == 0)
			icb_put_cancel(args);
		else
		if (my_strncmp(type, "ECHO", len) == 0)
			icb_put_echo(args);
		else
		if (my_strncmp(type, "GROUP", len) == 0)
			icb_put_group(args);
		else
		if (my_strncmp(type, "INVITE", len) == 0)
			icb_put_invite(args);
		else
		if (my_strncmp(type, "MOTD", len) == 0)
			icb_put_motd(args);
		else
		if (my_strncmp(type, "NICK", len) == 0)
			icb_put_nick(args);
		else
		if (my_strncmp(type, "PASS", len) == 0)
			icb_put_pass(args);
		else
		if (my_strncmp(type, "STATUS", len) == 0)
			icb_put_status(args);
		else
		if (my_strncmp(type, "TOPIC", len) == 0)
			icb_put_topic(args);
		else
		if (my_strncmp(type, "VERSION", len) == 0)
			icb_put_version(args);
		else
		if (my_strncmp(type, "WHO", len) == 0)
			icb_put_who(args);
		else
		if (my_strncmp(type, "COMMAND", len) == 0)
			icb_put_command(args);
		else
			say("No such ICB command %s", type);
	}
}

int
icb_port(void)
{
	return icb_port_local;
}

void
set_icb_port(int port)
{
	icb_port_local = port;
}
