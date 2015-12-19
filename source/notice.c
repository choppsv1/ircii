/*
 * notice.c: special stuff for parsing NOTICEs
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
IRCII_RCSID("@(#)$eterna: notice.c,v 1.75 2014/03/14 01:57:16 mrg Exp $");

#include "whois.h"
#include "ctcp.h"
#include "window.h"
#include "lastlog.h"
#include "flood.h"
#include "vars.h"
#include "ircaux.h"
#include "hook.h"
#include "ignore.h"
#include "server.h"
#include "funny.h"
#include "output.h"
#include "names.h"
#include "parse.h"
#include "notify.h"
#include "notice.h"

static	void	parse_note(u_char *, u_char *);
static	void	parse_server_notice(u_char *, u_char *, u_char *);

/*
 * parse_note: handles the parsing of irc note messages which are sent as
 * NOTICES.  The notice() function determines which notices are note messages
 * and send that info to parse_note() 
 */
static	void
parse_note(u_char *server, u_char *line)
{
	u_char	*date,
		*nick,
		*flags,
		*high,
		*name,
		*message;
	int	ign1,
		ign2,
		level;
	time_t	the_time;

	flags = next_arg(line, &date);	/* what to do with these flags */
	nick = next_arg(date, &date);
	name = next_arg(date, &date);
	if ((message = my_index(date, '*')) != NULL)
		*message = (u_char) 0;
	if (((ign1 = is_ignored(nick, IGNORE_NOTES)) == IGNORED) ||
			((ign2 = is_ignored(name, IGNORE_NOTES)) == IGNORED))
		return;
	if ((ign1 == HIGHLIGHTED) || (ign2 == HIGHLIGHTED))
		high = highlight_str();
	else
		high = empty_string();
	the_time = my_atol(date);
	date = UP(ctime(&the_time));
	date[24] = (u_char) 0;
	level = set_lastlog_msg_level(LOG_NOTES);
	if (do_hook(NOTE_LIST, "%s %s %s %s %s %s", nick, name, flags, date,
		server, message + 2))
	{
		put_it("Note from %s (%s) %s", nick, name, flags);
		put_it("It was queued %s from server %s", date, server);
		put_it("%s[%s]%s %s", high, nick, high, message + 2);
	}
	if (do_beep_on_level(LOG_NOTES))
		beep_em(1);
	set_lastlog_msg_level(level);
}

static	void
parse_server_notice(u_char *from, u_char *to, u_char *line)
{
	u_char	server[81],
		version[21];
	int	user_cnt,
		server_cnt,
		lastlog_level;
	int	flag;

	if (!from || !*from)
		from = server_get_itsname(parsing_server());
		if (!from)
			from = server_get_name(parsing_server());
	if (get_int_var(SUPPRESS_SERVER_MOTD_VAR) &&
		server_get_motd(parsing_server()))
	{
		if (my_strncmp("*** Message-of-today", line, 20) == 0)
		{
			server_set_motd(parsing_server(), 0);
			return;
		}
		if (my_strncmp("MOTD ", line, 5) == 0)
		{
			server_set_motd(parsing_server(), 1);
			return;
		}
		if (my_strcmp("* End of /MOTD command.", line) == 0)
		{
			server_set_motd(parsing_server(), 0);
			return;
		}
	}
	save_message_from();
	if (!my_strncmp(line, "*** Notice --", 13))
	{
		message_from(NULL, LOG_OPNOTE);
		lastlog_level = set_lastlog_msg_level(LOG_OPNOTE);
	}
	else
	{
		message_from(NULL, LOG_SNOTE);
		lastlog_level = set_lastlog_msg_level(LOG_SNOTE);
	}
	if (to)
	{
		if (do_hook(SERVER_NOTICE_LIST, "%s %s %s", from, to, line))
			put_it("%s %s", to, line);
	}
	else
	{
		if (server_get_version(parsing_server()) >= Server2_7 && 
		    *line != '*'  && *line != '#' && my_strncmp(line, "MOTD ", 4))
			flag = 1;
		else
			flag = 0;
		if (do_hook(SERVER_NOTICE_LIST, flag ? "%s *** %s"
						     : "%s %s", from, line))
			put_it(flag ? "*** %s" : "%s", line);
	}
	if ((parsing_server() == get_primary_server()) &&
			((sscanf(CP(line), "*** There are %d users on %d servers",
			&user_cnt, &server_cnt) == 2) ||
			(sscanf(CP(line), "There are %d users on %d servers",
			&user_cnt, &server_cnt) == 2)))
	{
		if ((server_cnt < get_int_var(MINIMUM_SERVERS_VAR)) ||
				(user_cnt < get_int_var(MINIMUM_USERS_VAR)))
		{
			say("Trying better populated server...");
			get_connected(parsing_server() + 1);
		}
	}
	else if ((sscanf(CP(line), "*** Your host is %80s running version %20s",
			server, version) == 2))
	{
		if (server_get_version(parsing_server()) < Server2_8)
			got_initial_version(line);
	}
	if (lastlog_level)
	{
		set_lastlog_msg_level(lastlog_level);
		restore_message_from();
	}
}

void 
parse_notice(u_char *from, u_char **Args)
{
	int	level,
		type;
	u_char	*to;
	int	no_flooding;
	int	flag;
	u_char	*high,
		not_from_server = 1;
	u_char	*line;

	PasteArgs(Args, 1);
	to = Args[0];
	line = Args[1];
	if (!to || !line)
		return;
	if (*to)
	{
		save_message_from();
		if (is_channel(to))
		{
			message_from(to, LOG_NOTICE);
			type = PUBLIC_NOTICE_LIST;
		}
		else
		{
			message_from(from, LOG_NOTICE);
			type = NOTICE_LIST;
		}
		if (from && *from && my_strcmp(server_get_itsname(parsing_server()), from))
		{
			flag = double_ignore(from, from_user_host(), IGNORE_NOTICES);
			if (flag != IGNORED)
			{
				if (flag == HIGHLIGHTED)
					high = highlight_str();
				else
					high = empty_string();
				if (my_index(from, '.'))
				{
		/*
		 * only dots in servernames, right ?
		 *
		 * But a server name doesn't nessicarily have to have
		 * a 'dot' in it..  - phone, jan 1993.
		 */
					not_from_server = 0;
					if (my_strncmp(line, "*/", 2) == 0)
					{
						parse_note(from, line + 1);
						goto out;
					}
				}
				if (not_from_server && (flag != DONT_IGNORE) && !from_user_host() &&
							(ignore_usernames() & IGNORE_NOTICES))
					add_to_whois_queue(from, whois_ignore_notices, "%s %s", to, line);
				else
				{
					line = do_notice_ctcp(from, to, line);
					if (*line == '\0')
					{
						goto out;
					}
					level = set_lastlog_msg_level(LOG_NOTICE);
					no_flooding = check_flooding(from, to, NOTICE_FLOOD, line);

					if (no_flooding &&
					   (ctcp_was_crypted() == 0 || do_hook(ENCRYPTED_NOTICE_LIST, "%s %s %s", from, to, line)))
					{
						if (do_hook(type, "%s %s %s", from, to, line))
						{
							if (type == NOTICE_LIST)
								put_it("%s-%s-%s %s", high, from, high, line);
							else
								put_it("%s-%s:%s-%s %s", high, from, to, high, line);
						}
						if (do_beep_on_level(LOG_NOTICE))
							beep_em(1);
						set_lastlog_msg_level(level);
						if (not_from_server)
							notify_mark(from, 1, 0);
					}
				}
			}
		}
		else 
			parse_server_notice(from, type == PUBLIC_NOTICE_LIST ? to : 0, line);
	}
	else
		put_it("%s", line + 1);
out:
	restore_message_from();
}

/*
 * load the initial .ircrc
 */
void
load_ircrc(void)
{
	static	int done = 0;

	if (done++)
		return;
	if (access(CP(ircrc_file_path()), R_OK) == 0)
	{
		u_char lbuf[BIG_BUFFER_SIZE];

		my_strmcpy(lbuf, ircrc_file_path(), sizeof lbuf);
		my_strmcat(lbuf, " ", sizeof lbuf);
		my_strmcat(lbuf, irc_extra_args(), sizeof lbuf);
		load(empty_string(), lbuf, empty_string());
	}
	else if (get_int_var(NOVICE_VAR))
		say("If you have not already done so, please read the new user information with /HELP NEWUSER");
}

/*
 * load the initial .ircquick
 */
void
load_ircquick(void)
{
	static	int done = 0;

	if (done)
		return;
	done = 1;
	if (access(CP(ircquick_file_path()), R_OK) == 0)
	{
		u_char lbuf[BIG_BUFFER_SIZE];

		my_strmcpy(lbuf, ircquick_file_path(), sizeof lbuf);
		my_strmcat(lbuf, " ", sizeof lbuf);
		my_strmcat(lbuf, irc_extra_args(), sizeof lbuf);
		load(empty_string(), lbuf, empty_string());
	}
}

/*
 * got_initial_version: this is called when ircii get the NOTICE in
 * all version of ircd before 2.8, or the 002 numeric for 2.8 and
 * beyond.  I guess its handled rather badly at the moment....
 * added by phone, late 1992.
 */
void
got_initial_version(u_char *line)
{
	u_char	server[256],
		version[256];
	u_char	*s, c;

	if ((sscanf(CP(line), "*** Your host is %255s running version %255s",
			server, version)) != 2) {
		yell("This server has a non-standard connection message!");
		my_strcpy(version, "2.9");
		my_strcpy(server, server_get_name(parsing_server()));
	}
	else if ((c = server[my_strlen(server) - 1]) == ',' || c == '.')
		server[my_strlen(server) - 1] = '\0';

	server_set_attempting_to_connect(parsing_server(), 0);
	server_set_motd(parsing_server(), 1);
	server_is_connected(parsing_server(), 1);
	if ((s = my_index(server, '[')) != NULL)
		*s = '\0';	/*
				 * Handles the case where the server name is
				 * different to the host name.
				 */
	if (!my_strncmp(version, "2.6", 3))
		server_set_version(parsing_server(), Server2_6);
	else if (!my_strncmp(version, "2.7", 3))
		server_set_version(parsing_server(), Server2_7);
	else if (!my_strncmp(version, "2.8", 3))
		server_set_version(parsing_server(), Server2_8);
	else if (!my_strncmp(version, "2.9", 3))
		server_set_version(parsing_server(), Server2_9);
	else if (!my_strncmp(version, "2.10", 4))
		server_set_version(parsing_server(), Server2_10);
	else
		server_set_version(parsing_server(), Server2_11);
	server_set_version_string(parsing_server(), version);
	server_set_itsname(parsing_server(), server);
	reconnect_all_channels(parsing_server());
	reinstate_user_modes();
	maybe_load_ircrc();
	if (server_get_away(parsing_server()))
		send_to_server("AWAY :%s",
			server_get_away(parsing_server()));
	update_all_status();
	do_hook(CONNECT_LIST, "%s %d", server_get_name(parsing_server()),
		server_get_port(parsing_server()));
}

void
maybe_load_ircrc(void)
{

	if (never_connected())
	{
		unset_never_connected();
		if (!use_background_mode())
			load_global();
		/* read the .ircrc file */
		if (!ignore_ircrc())
			load_ircrc();
	}
}
