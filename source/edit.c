/*
 * edit.c: This is really a mishmash of function and such that deal with IRCII
 * commands (both normal and keybinding commands) 
 *
 * Written By Michael Sandrof
 *
 * Copyright (c) 1990 Michael Sandrof.
 * Copyright (c) 1991, 1992 Troy Rollo.
 * Copyright (c) 1992-2015 Matthew R. Green.
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
IRCII_RCSID("@(#)$eterna: edit.c,v 1.260 2015/02/22 12:43:18 mrg Exp $");

#include <sys/stat.h>

#include "parse.h"
#include "ircterm.h"
#include "server.h"
#include "edit.h"
#include "irccrypt.h"
#include "vars.h"
#include "ircaux.h"
#include "lastlog.h"
#include "window.h"
#include "screen.h"
#include "whois.h"
#include "hook.h"
#include "input.h"
#include "ignore.h"
#include "keys.h"
#include "names.h"
#include "alias.h"
#include "history.h"
#include "funny.h"
#include "ctcp.h"
#include "dcc.h"
#include "translat.h"
#include "output.h"
#include "exec.h"
#include "notify.h"
#include "numbers.h"
#include "status.h"
#include "if.h"
#include "help.h"
#include "queue.h"
#include "icb.h"
#include "strsep.h"

/* a structure for the timer list */
typedef struct	timerlist_stru
{
	int	ref;
	int	in_on_who;
	time_t	time;
	unsigned microseconds;
	int	server;
	u_char	*command;
	struct	timerlist_stru *next;
} TimerList;

/* a structure for per-screen info */
struct edit_info_stru {
	int	meta_hit[9];		/* if meta[N] is hit in this screen
					   meta_hit[0] is if QUOTE_CHARACTER
					   has been hit. */
	int	digraph_hit;		/* A digraph key has been hit */
	int	inside_menu;		/* what it says. */
	u_char	digraph_first;
};

/* a structure for per-server info */
struct who_info_stru {
	int	who_mask;
	u_char	*who_name;
	u_char	*who_host;
	u_char	*who_server;
	u_char	*who_file;
	u_char	*who_nick;
	u_char	*who_real;
};

/*
 * current_exec_timer - used to make sure we don't remove a timer
 * from within itself.
 */
static	int	current_exec_timer = -1;

static	int	save_which;
static	int	save_do_all;
static	int	away_set;		/* set if there is an away
					 * message anywhere */

static	void	oper_password_received(u_char *, u_char *);
static	void	get_history(int);
static	int	prompt_check_input(int, u_char *, u_char);
static	void	send_action(u_char *, u_char *);
static	void	show_timer(u_char *);
static	int	create_timer_ref(int);
static	void	load_a_file(FILE *, u_char *, int);

static TimerList *PendingTimers = NULL;

/* used with input_move_cursor */
#define RIGHT 1
#define LEFT 0

/* used with /save */
#define	SFLAG_ALIAS	0x0001
#define	SFLAG_BIND	0x0002
#define	SFLAG_ON	0x0004
#define	SFLAG_SET	0x0008
#define	SFLAG_NOTIFY	0x0010
#define	SFLAG_DIGRAPH	0x0020
#define	SFLAG_IGNORE	0x0040

/* The maximum number of recursive LOAD levels allowed */
#define MAX_LOAD_DEPTH 10

struct prompt_stru
{
	u_char	*prompt;
	u_char	*data;
	int	type;
	void	(*func)(u_char *, u_char *);
	Prompt	*next;
};

/* recv_nick: the nickname of the last person to send you a privmsg */
static	u_char	*recv_nick = NULL;

/* sent_nick: the nickname of the last person to whom you sent a privmsg */
static	u_char	*sent_nick = NULL;
static	u_char	*sent_body = NULL;

/* last channel of an INVITE */
static	u_char	*invite_channel;

static	u_char	wait_nick[] = "#WAIT#";

/* Used to keep down the nesting of /LOADs and to determine if we
 * should activate the warning for /ONs if the NOVICE variable is set.
 */
static	int	load_depth = 0;

/* This is used to make /save not save global things. */
static	int	doing_loading_global = 0;

typedef	struct	WaitCmdstru
{
	u_char	*stuff;
	struct	WaitCmdstru	*next;
}	WaitCmd;

static	WaitCmd	*start_wait_list = NULL,
		*end_wait_list = NULL;

	/* ugly hack */
static	int	my_echo_set_message_from;

/* a few advance declarations */
static	void	sendlinecmd(u_char *, u_char *, u_char *);
static	void	do_send_text(u_char *, u_char *, u_char *);
static	void	funny_stuff(u_char *, u_char *, u_char *);
static	void	catter(u_char *, u_char *, u_char *);
static	void	cd(u_char *, u_char *, u_char *);
static	void	e_wall(u_char *, u_char *, u_char *);
static	void	send_squery(u_char *, u_char *, u_char *);
static	void	send_brick(u_char *, u_char *, u_char *);
static	void	send_2comm(u_char *, u_char *, u_char *);
static	void	send_comm(u_char *, u_char *, u_char *);
static	void	send_invite(u_char *, u_char *, u_char *);
static	void	send_motd(u_char *, u_char *, u_char *);
static	void	send_topic(u_char *, u_char *, u_char *);
static	void	send_channel_nargs(u_char *, u_char *, u_char *);
static	void	send_channel_2args(u_char *, u_char *, u_char *);
static	void	send_channel_1arg(u_char *, u_char *, u_char *);
static	void	my_clear(u_char *, u_char *, u_char *);
static	void	quote(u_char *, u_char *, u_char *);
static	void	e_privmsg(u_char *, u_char *, u_char *);
static	void	flush(u_char *, u_char *, u_char *);
static	void	away(u_char *, u_char *, u_char *);
static	void	oper(u_char *, u_char *, u_char *);
static	void	e_channel(u_char *, u_char *, u_char *);
static	void	who(u_char *, u_char *, u_char *);
static	void	whois(u_char *, u_char *, u_char *);
static	void	ison(u_char *, u_char *, u_char *);
static	void	userhost(u_char *, u_char *, u_char *);
static	void	info(u_char *, u_char *, u_char *);
static	void	e_nick(u_char *, u_char *, u_char *);
static	void	commentcmd(u_char *, u_char *, u_char *);
static	void	sleepcmd(u_char *, u_char *, u_char *);
static	void	version(u_char *, u_char *, u_char *);
static	void	ctcp(u_char *, u_char *, u_char *);
static	void	dcc(u_char *, u_char *, u_char *);
static	void	deop(u_char *, u_char *, u_char *);
static	void	my_echo(u_char *, u_char *, u_char *);
static	void	save_settings(u_char *, u_char *, u_char *);
static	void	redirect(u_char *, u_char *, u_char *);
static	void	waitcmd(u_char *, u_char *, u_char *);
static	void	describe(u_char *, u_char *, u_char *);
static	void	me(u_char *, u_char *, u_char *);
static	void	mload(u_char *, u_char *, u_char *);
static	void	mlist(u_char *, u_char *, u_char *);
static	void	evalcmd(u_char *, u_char *, u_char *);
static	void	hook(u_char *, u_char *, u_char *);
static	void	timercmd(u_char *, u_char *, u_char *);
static	void	inputcmd(u_char *, u_char *, u_char *);
static	void	pingcmd(u_char *, u_char *, u_char *);
static	void	xtypecmd(u_char *, u_char *, u_char *);
static	void	beepcmd(u_char *, u_char *, u_char *);
static	void	abortcmd(u_char *, u_char *, u_char *);
static	void	really_save(u_char *, u_char *);
static	void	e_nuser(u_char *, u_char *, u_char *);

#ifdef HAVE_SETENV
static	void	irc_setenv(u_char *, u_char *, u_char *);
#endif
#ifdef HAVE_UNSETENV
static	void	irc_unsetenv(u_char *, u_char *, u_char *);
#endif

/* IrcCommand: structure for each command in the command table */
typedef	struct
{
	char	*name;					/* what the user types */
	char	*server_func;				/* what gets sent to the server
							 * (if anything) */
	void	(*func)(u_char *, u_char *, u_char *); /* function that is the command */
	unsigned	flags;
}	IrcCommand;

static	IrcCommand *find_command(u_char *, int *);

#define NONOVICEABBREV	0x0001
#define NOINTERACTIVE	0x0002
#define NOSIMPLESCRIPT	0x0004
#define NOCOMPLEXSCRIPT	0x0008
#define SERVERREQ	0x0010
#define NOICB		0x0020

/*
 * irc_command: all the available irc commands:  Note that the first entry has
 * a zero length string name and a null server command... this little trick
 * makes "/ blah blah blah" to always be sent to a channel, bypassing queries,
 * etc.  Neato.  This list MUST be sorted.
 */
static	IrcCommand irc_command[] =
{
	{ "",		"",		 do_send_text,		NOSIMPLESCRIPT| NOCOMPLEXSCRIPT },
	/*
	 * I really want to remove #, but it will break a lot of scripts.  - mycroft
	 *
	 * I agree - it should be converted to a special character in parse_line.
	 *                                                            - Troy
	 */
	{ "#",		NULL,		commentcmd, 		0 },
	{ ":",		NULL,		commentcmd, 		0 },
	{ "ABORT",      NULL,           abortcmd,               0 },
	{ "ADMIN",	"ADMIN",	send_comm, 		SERVERREQ|NOICB },
	{ "ALIAS",	"0",		alias,			0 },
	{ "ASSIGN",	"1",		alias,			0 },
	{ "AWAY",	"AWAY",		away,			SERVERREQ },
	{ "BEEP",	0,		beepcmd,		0 },
	{ "BIND",	NULL,		bindcmd,		0 },
	{ "BRICK",	"BRICK",	send_brick,		SERVERREQ },
	{ "BYE",	"QUIT",		e_quit,			NONOVICEABBREV },
	{ "CAT",	NULL,		catter,			0 },
	{ "CD",		NULL,		cd,			0 },
	{ "CHANNEL",	"JOIN",		e_channel,		SERVERREQ },
	{ "CLEAR",	NULL,		my_clear,		0 },
	{ "COMMENT",	NULL,		commentcmd,		0 },
	{ "CONNECT",	"CONNECT",	send_comm,		SERVERREQ|NOICB },
	{ "CTCC",	NULL,		dcc,			SERVERREQ|NOICB },
	{ "CTCP",	NULL,		ctcp,			SERVERREQ|NOICB },
	{ "DATE",	"TIME",		send_comm,		SERVERREQ },
	{ "DCC",	NULL,		dcc,			SERVERREQ|NOICB },
	{ "DEOP",	NULL,		deop,			SERVERREQ|NOICB },
	{ "DESCRIBE",	NULL,		describe,		SERVERREQ },
	{ "DIE",	"DIE",		send_comm,		SERVERREQ|NOICB },
	{ "DIGRAPH",	NULL,		digraph,		0 },
	{ "DISCONNECT",	NULL,		disconnectcmd,		SERVERREQ },
	{ "ECHO",	NULL,		my_echo,		0 },
	{ "ENCRYPT",	NULL,		encrypt_cmd,		0 },
	{ "EVAL",	NULL,		evalcmd,		0 },
	{ "EXEC",	NULL,		execcmd,		0 },
	{ "EXIT",	"QUIT",		e_quit,			NONOVICEABBREV },
	{ "FE",		NULL,		foreach_handler,	0 },
	{ "FEC",	NULL,		fec,			0 },
	{ "FLUSH",	NULL,		flush,			SERVERREQ },
	{ "FOR",	NULL,		foreach_handler,	0 },
	{ "FOREACH",	NULL,		foreach_handler,	0 },
	{ "HASH",	"HASH",		send_comm,		SERVERREQ|NOICB },
	{ "HELP",	NULL,		help,			0 },
	{ "HISTORY",	NULL,		history,		0 },
	{ "HOOK",	NULL,		hook,			0 },
	{ "HOST",	"USERHOST",	userhost,		SERVERREQ|NOICB },
	{ "ICB",	NULL,		icbcmd,			0 },
	{ "IF",		NULL,		ifcmd,			0 },
	{ "IGNORE",	NULL,		ignore,			0 },
	{ "INFO",	"INFO",		info,			SERVERREQ },
	{ "INPUT",	NULL,		inputcmd,		0 },
	{ "INVITE",	"INVITE",	send_invite,		SERVERREQ },
	{ "ISON",	"ISON",		ison,			SERVERREQ|NOICB },
	{ "JOIN",	"JOIN",		e_channel,		SERVERREQ },
	{ "KICK",	"KICK",		send_channel_2args,	SERVERREQ|NOICB },
	{ "KILL",	"KILL",		send_2comm,		SERVERREQ|NOICB },
	{ "LASTLOG",	NULL,		lastlog,		0 },
	{ "LEAVE",	"PART",		send_channel_1arg,	SERVERREQ|NOICB },
	{ "LINKS",	"LINKS",	send_comm,		NONOVICEABBREV|SERVERREQ|NOICB },
	{ "LIST",	"LIST",		funny_stuff,		SERVERREQ },
	{ "LOAD",	"LOAD",		load,			0 },
	{ "LUSERS",	"LUSERS",	send_comm,		SERVERREQ|NOICB },
	{ "ME",		NULL,		me,			SERVERREQ },
	{ "MLIST",	NULL,		mlist,			0 },
	{ "MLOAD",	NULL,		mload,			0 },
	{ "MODE",	"MODE",		send_channel_nargs,	SERVERREQ|NOICB },
	{ "MOTD",	"MOTD",		send_motd,		SERVERREQ },
	{ "MSG",	"PRIVMSG",	e_privmsg,		0 },
	{ "NAMES",	"NAMES",	funny_stuff,		SERVERREQ },
	{ "NICK",	"NICK",		e_nick,			SERVERREQ },
	{ "NOTE",	"NOTE",		send_comm,		SERVERREQ|NOICB },
	{ "NOTICE",	"NOTICE",	e_privmsg,		SERVERREQ },
	{ "NOTIFY",	NULL,		notify,			SERVERREQ },
	{ "NUSER",	NULL,		e_nuser,		SERVERREQ },
	{ "ON",		NULL,		on,			0 },
	{ "OPER",	"OPER",		oper,			SERVERREQ|NOICB },
	{ "PARSEKEY",	NULL,		parsekeycmd,		0 },
	{ "PART",	"PART",		send_channel_1arg,	SERVERREQ|NOICB },
	{ "PING",	NULL, 		pingcmd,		SERVERREQ },
	{ "QUERY",	NULL,		query,			0 },
	{ "QUEUE",      NULL,           queuecmd,               0 },
	{ "QUIT",	"QUIT",		e_quit,			NONOVICEABBREV },
	{ "QUOTE",	NULL,		quote,			SERVERREQ },
	{ "RBIND",	0,		rbindcmd,		0 },
	{ "REDIRECT",	NULL,		redirect,		NOICB },
	{ "REHASH",	"REHASH",	send_comm,		SERVERREQ|NOICB },
	{ "REQUEST",	NULL,		ctcp,			SERVERREQ|NOICB },
	{ "RESTART",	"RESTART",	send_comm,		SERVERREQ|NOICB },
	{ "SAVE",	NULL,		save_settings,		0 },
	{ "SAY",	"",		 do_send_text,		SERVERREQ },
	{ "SEND",	NULL,		do_send_text,		SERVERREQ },
	{ "SENDLINE",	"",		 sendlinecmd,		0 },
	{ "SERVER",	NULL,		servercmd,		0 },
	{ "SERVLIST",	"SERVLIST",	send_comm,		SERVERREQ|NOICB },
	{ "SET",	NULL,		set_variable,		0 },
#ifdef HAVE_SETENV
	{ "SETENV",	NULL,		irc_setenv,		0 },
#endif
	{ "SIGNOFF",	"QUIT",		e_quit,			NONOVICEABBREV },
	{ "SLEEP",	NULL,		sleepcmd,		0 },
	{ "SQUERY",	"SQUERY",	send_squery,		SERVERREQ|NOICB },
	{ "SQUIT",	"SQUIT",	send_2comm,		SERVERREQ|NOICB },
	{ "STATS",	"STATS",	send_comm,		SERVERREQ|NOICB },
	{ "SUMMON",	"SUMMON",	send_comm,		SERVERREQ|NOICB },
	{ "TIME",	"TIME",		send_comm,		SERVERREQ|NOICB },
	{ "TIMER",	"TIMER",	timercmd,		0 },
	{ "TOPIC",	"TOPIC",	send_topic,		SERVERREQ },
	{ "TRACE",	"TRACE",	send_comm,		SERVERREQ|NOICB },
	{ "TYPE",	NULL,		typecmd,		0 },
#ifdef HAVE_UNSETENV
	{ "UNSETENV",	NULL,		irc_unsetenv,		0 },
#endif
	{ "USERHOST",	NULL,		userhost,		SERVERREQ|NOICB },
	{ "USERS",	"USERS",	send_comm,		SERVERREQ|NOICB },
	{ "VERSION",	"VERSION",	version,		0 },
	{ "VOICE",	"VOICE",	e_privmsg,		SERVERREQ },
	{ "WAIT",	NULL,		waitcmd,		SERVERREQ },
	{ "WALL",	"WALL",		e_wall,			SERVERREQ|NOICB },
	{ "WALLOPS",	"WALLOPS",	e_wall,			SERVERREQ|NOICB },
	{ "WHICH",	"WHICH",	load,			0 },
	{ "WHILE",	NULL,		whilecmd,		0 },
	{ "WHO",	"WHO",		who,			SERVERREQ },
	{ "WHOIS",	"WHOIS",	whois,			SERVERREQ },
	{ "WHOWAS",	"WHOWAS",	whois,			SERVERREQ|NOICB },
	{ "WINDOW",	NULL,		windowcmd,		0 },
	{ "XECHO",	"XECHO",	my_echo,		0 },
	{ "XTRA",	"XTRA",		e_privmsg,		SERVERREQ },
	{ "XTYPE",	NULL,		xtypecmd,		0 },
	{ NULL,		NULL,		commentcmd,		0 }
};

/* number of entries in irc_command array */
# define	NUMBER_OF_COMMANDS	ARRAY_SIZE(irc_command)

/*
 * find_command: looks for the given name in the command list, returning a
 * pointer to the first match and the number of matches in cnt.  If no
 * matches are found, null is returned (as well as cnt being 0). The command
 * list is sorted, so we do a binary search.  The returned commands always
 * points to the first match in the list.  If the match is exact, it is
 * returned and cnt is set to the number of matches * -1.  Thus is 4 commands
 * matched, but the first was as exact match, cnt is -4.
 */
static	IrcCommand *
find_command(u_char *com, int *cnt)
{
	size_t	len;

	if (com && (len = my_strlen(com)))
	{
		int	min,
			max,
			pos,
			old_pos = -1,
			c;

		min = 1;
		max = NUMBER_OF_COMMANDS - 1;
		while (1)
		{
			pos = (max + min) / 2;
			if (pos == old_pos)
			{
				*cnt = 0;
				return (NULL);
			}
			old_pos = pos;
			c = my_strncmp(com, irc_command[pos].name, len);
			if (c == 0)
				break;
			else if (c > 0)
				min = pos;
			else
				max = pos;
		}
		*cnt = 0;
		(*cnt)++;
		min = pos - 1;
		while (min > 0 &&
		       my_strncmp(com, irc_command[min].name, len) == 0)
		{
			(*cnt)++;
			min--;
		}
		min++;
		max = pos + 1;
		while (max < NUMBER_OF_COMMANDS - 1 &&
		       my_strncmp(com, irc_command[max].name, len) == 0)
		{
			(*cnt)++;
			max++;
		}
		if (*cnt)
		{
			if (my_strlen(irc_command[min].name) == len)
				*cnt *= -1;
			else if (*cnt == 1 && 
					irc_command[min].flags&NONOVICEABBREV &&
					get_int_var(NOVICE_VAR))
			{
				say("As a novice you may not abbreviate the %s command", irc_command[min].name);
				*cnt=0;
				return (NULL);
			}
			return (&(irc_command[min]));
		}
		else
			return (NULL);
	}
	else
	{
		*cnt = -1;
		return (irc_command);
	}
}

static	void
ctcp(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*to,
		*tag;
	int	ctcptype;

	if ((to = next_arg(args, &args)) != NULL)
	{
		if (!my_strcmp(to, "*"))
			if ((to = get_channel_by_refnum(0)) == NULL)
				to = zero();
		if ((tag = next_arg(args, &args)) != NULL)
			upper(tag);
		else
			tag = UP("VERSION");
		if ((ctcptype = in_ctcp()) == -1)
			my_echo(NULL, UP("*** You may not use the CTCP command in an ON CTCP_REPLY!"), empty_string());
		else
		{
			if (args && *args)
				send_ctcp(ctcp_type(ctcptype), to, tag, "%s", args);
			else
				send_ctcp(ctcp_type(ctcptype), to, tag, NULL);
		}
	}
	else
		say("Request from whom?");
}

static	void
hook(u_char *command, u_char *args, u_char *subargs)
{
	if (*args)
		do_hook(HOOK_LIST, "%s", args);
	else
		say("Must supply an argument to HOOK");
}

static	void
dcc(u_char *command, u_char *args, u_char *subargs)
{
	if (*args)
		process_dcc(args);
	else
		dcc_list(NULL);
}

static	void
deop(u_char *command, u_char *args, u_char *subargs)
{
	send_to_server("MODE %s -o", server_get_nickname(get_from_server()));
}

static	void
funny_stuff(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*arg,
		*cmd = NULL,
		*stuff,
		*s;
	int	min = 0,
		max = 0,
		flags = 0;
	size_t	len;
	const	int	from_server = get_from_server();

	if (server_get_version(from_server) == ServerICB)
	{
		icb_put_funny_stuff(command, args, subargs);
		return;
	}
	stuff = empty_string();
	while ((arg = next_arg(args, &args)) != NULL)
	{
		len = my_strlen(arg);
		malloc_strcpy(&cmd, arg);
		upper(cmd);
		if (my_strncmp(cmd, "-MAX", len) == 0)
		{
			if ((arg = next_arg(args, &args)) != NULL)
				max = my_atoi(arg);
		}
		else if (my_strncmp(cmd, "-MIN", len) == 0)
		{
			if ((arg = next_arg(args, &args)) != NULL)
				min = my_atoi(arg);
		}
		else if (my_strncmp(cmd, "-ALL", len) == 0)
		{
			flags &= ~(FUNNY_PUBLIC | FUNNY_PRIVATE);
		}
		else if (my_strncmp(cmd, "-PUBLIC", len) == 0)
		{
			flags |= FUNNY_PUBLIC;
			flags &= ~FUNNY_PRIVATE;
		}
		else if (my_strncmp(cmd, "-PRIVATE", len) == 0)
		{
			flags |= FUNNY_PRIVATE;
			flags &= ~FUNNY_PUBLIC;
		}
		else if (my_strncmp(cmd, "-TOPIC", len) == 0)
			flags |= FUNNY_TOPIC;
		else if (my_strncmp(cmd, "-WIDE", len) == 0)
			flags |= FUNNY_WIDE;
		else if (my_strncmp(cmd, "-USERS", len) == 0)
			flags |= FUNNY_USERS;
		else if (my_strncmp(cmd, "-NAME", len) == 0)
			flags |= FUNNY_NAME;
		else
			stuff = arg;
		new_free(&cmd);
	}
	set_funny_flags(min, max, flags);
	if (my_strcmp(stuff, "*") == 0)
		if (!(stuff = get_channel_by_refnum(0)))
			stuff = empty_string();
	if ((s = my_index(stuff, '*')) &&
	    !is_on_channel(stuff, from_server, server_get_nickname(from_server)) &&
	    !(s > stuff && s[-1] == ':'))
	{
		funny_match(stuff);
		send_to_server("%s %s", command, empty_string());
	}
	else
	{
		funny_match(NULL);
		send_to_server("%s %s", command, stuff);
	}
}

static void
waitcmd(u_char *command, u_char *args, u_char *subargs)
{
	WaitCmd	*new;
	int	wait_index;
	u_char	*flag;
	u_char	*procindex;
	int	cmd = 0;
	size_t	len;
	u_char	buffer[BIG_BUFFER_SIZE];

	while (args && *args == '-')
	{
		flag = next_arg(args, &args);
		len = my_strlen(++flag);
		if (!my_strnicmp(UP("CMD"), flag, len))
		{
			cmd = 1;
			break;
		}
		else
			yell("Unknown argument to WAIT: %s", flag);
	}
	if (!cmd)
	{
		yell("WAIT without -CMD is no longer available.");
		return;
	}
	if ((procindex = next_arg(args, &args)) && *procindex == '%' &&
			(wait_index = get_process_index(&procindex)) != -1)
	{
		if (is_process_running(wait_index))
			add_process_wait(wait_index, args ? args : empty_string());
		else
			say("Not a valid process!");
		return;
	}

	snprintf(CP(buffer), sizeof buffer, "%s %s", procindex, args);
	new = new_malloc(sizeof *new);
	new->stuff = NULL;
	malloc_strcpy(&new->stuff, buffer);
	new->next = NULL;
	if (end_wait_list)
		end_wait_list->next = new;
	end_wait_list = new;
	if (!start_wait_list)
		start_wait_list = new;
	send_to_server("%s", wait_nick);
	return;
}

int
check_wait_command(u_char *nick)
{
	if (start_wait_list && !my_strcmp(nick, wait_nick))
	{
		if (start_wait_list->stuff)
		{
			parse_command(start_wait_list->stuff, 0, empty_string());
			new_free(&start_wait_list->stuff);
		}
		start_wait_list = start_wait_list->next;
		return 1;
	}
	return 0;
}

static	void
redirect(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*to;

	if ((to = next_arg(args, &args)) != NULL)
	{
		const	int	from_server = get_from_server();

		if (!my_strcmp(to, "*"))
			if (!(to = get_channel_by_refnum(0)))
			{
				say("Must be on a channel to redirect to '*'");
				return;
			}
		if (!my_stricmp(to, server_get_nickname(from_server)))
		{
			say("You may not redirect output to yourself");
			return;
		}
		window_redirect(to, from_server);
		server_set_sent(from_server, 0);
		parse_line(NULL, args, NULL, 0, 0, 0);
		if (server_get_sent(from_server))
			send_to_server("%s", get_redirect_token());
		else
			window_redirect(NULL, from_server);
	}
	else
		say("Usage: REDIRECT <nick|channel|%%process> <cmd>");
}

static	void
sleepcmd(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*arg;

	if ((arg = next_arg(args, &args)) != NULL)
		sleep((unsigned)my_atoi(arg));
	else
		say("SLEEP: you must specify the amount of time to sleep (in seconds)");
}

/*
 * my_echo: simply displays the args to the screen, or, if it's XECHO,
 * processes the flags first, then displays the text on
 * the screen
 */
static void
my_echo(u_char *command, u_char *args, u_char *subargs)
{
	unsigned int	display;
	int	lastlog_level = 0;
	int	from_level = 0;
	u_char	*flag_arg;
	int	temp;
	Window *old_to_window;

	save_message_from();
	old_to_window = get_to_window();
	if (command && *command == 'X')
	{
		while (args && *args == '-')
		{
			flag_arg = next_arg(args, &args);
			switch(flag_arg[1])
			{
			case 'L':
			case 'l':
				if (!(flag_arg = next_arg(args, &args)))
					break;
				if ((temp = parse_lastlog_level(flag_arg)) != 0)
				{
					lastlog_level = set_lastlog_msg_level(temp);
					from_level = message_from_level(temp);
					my_echo_set_message_from = 1;
				}
				break;
			case 'W':
			case 'w':
				if (!(flag_arg = next_arg(args, &args)))
					break;
				if (isdigit(*flag_arg))
					set_to_window(get_window_by_refnum((unsigned)my_atoi(flag_arg)));
				else
					set_to_window(get_window_by_name(flag_arg));
				lastlog_level = set_lastlog_msg_level(LOG_CRAP);
				from_level = message_from_level(LOG_CRAP);
				break;
			}
			if (!args)
				args = empty_string();
		}
	}
	display = set_display_on();
	put_it("%s", args);
	set_display(display);
	if (lastlog_level)
		set_lastlog_msg_level(lastlog_level);
	if (from_level)
		message_from_level(from_level);
	restore_message_from();
	set_to_window(old_to_window);
	my_echo_set_message_from = 0;
}

/*
 */
static	void
oper_password_received(u_char *data, u_char *line)
{
	send_to_server("OPER %s %s", data, line);
}

/* oper: the OPER command.  */
static	void
oper(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*password;
	u_char	*nick;

 	server_set_oper_command(1);
	if (!(nick = next_arg(args, &args)))
		nick = my_nickname();
	if (!(password = next_arg(args, &args)))
	{
		add_wait_prompt(UP("Operator Password:"),
			oper_password_received, nick, WAIT_PROMPT_LINE);
		return;
	}
	send_to_server("OPER %s %s", nick, password);
}

/* Full scale abort.  Does a "save" into the filename in line, and
        then does a coredump */
static  void   
abortcmd(u_char *command, u_char *args, u_char *subargs)
{
        u_char    *filename = (u_char *) next_arg(args, &args);

	if (!filename)
		filename = UP("irc.aborted");
	save_which = SFLAG_ALIAS | SFLAG_BIND | SFLAG_ON | SFLAG_SET |
			     SFLAG_NOTIFY | SFLAG_DIGRAPH | SFLAG_IGNORE;
        really_save(filename, UP("y"));
        abort();
}
        
/* This generates a file of your ircII setup */
static	void
really_save(u_char *file, u_char *line)
{
	FILE	*fp;

	if (*line != 'y' && *line != 'Y')
		return;
	if ((fp = fopen(CP(file), "w")) != NULL)
	{
		if (save_which & SFLAG_BIND)
			save_bindings(fp, save_do_all);
		if (save_which & SFLAG_ON)
			save_hooks(fp, save_do_all);
		if (save_which & SFLAG_NOTIFY)
			save_notify(fp);
		if (save_which & SFLAG_SET)
			save_variables(fp, save_do_all);
		if (save_which & SFLAG_ALIAS)
			save_aliases(fp, save_do_all);
		if (save_which & SFLAG_DIGRAPH)
			save_digraphs(fp);
		if (save_which & SFLAG_IGNORE)
			save_ignore(fp);
		fclose(fp);
		say("IRCII settings saved to %s", file);
	}
	else
		say("Error opening %s: %s", file, strerror(errno));
}

/* save_settings: saves the current state of IRCII to a file */
static	void
save_settings(u_char *command, u_char *args, u_char *subargs)
{
	u_char	buffer[BIG_BUFFER_SIZE];
	u_char	*arg, *temp;
	int	all = 1, save_force = 0;

	save_which = save_do_all = 0;
	while ((arg = next_arg(args, &args)) != NULL)
	{
		if ('-' == *arg)
		{
			u_char	*cmd = NULL;

			all = 0;
			malloc_strcpy(&cmd, arg+1);
			upper(cmd);
			if (0 == my_strncmp("ALIAS", cmd, 5))
				save_which |= SFLAG_ALIAS;
			else if (0 == my_strncmp("ASSIGN", cmd, 6))
				save_which |= SFLAG_ALIAS;
			else if (0 == my_strncmp("BIND", cmd, 4))
				save_which |= SFLAG_BIND;
			else if (0 == my_strncmp("ON", cmd, 2))
				save_which |= SFLAG_ON;
			else if (0 == my_strncmp("SET", cmd, 3))
				save_which |= SFLAG_SET;
			else if (0 == my_strncmp("NOTIFY", cmd, 6))
				save_which |= SFLAG_NOTIFY;
			else if (0 == my_strncmp("DIGRAPH", cmd, 7))
				save_which |= SFLAG_DIGRAPH;
			else if (0 == my_strncmp("DIGRAPH", cmd, 7))
				save_which |= SFLAG_IGNORE;
			else if (0 == my_strncmp("ALL", cmd, 3))
				save_do_all = 1;
			else if (0 == my_strncmp("FORCE", cmd, 3))
				save_force = 1;
			else
			{
				say("%s: unknown argument", arg);
				new_free(&cmd);
				return;
			}
			new_free(&cmd);
			continue;
		}
#ifdef DAEMON_UID
		if (getuid() == DAEMON_UID)
		{
			say("You may only use the default value");
			return;
		}
#endif /* DAEMON_UID */
		temp = expand_twiddle(arg);
		if (temp)
			set_ircrc_file_path(temp);
		else
		{
			say("Unknown user");
			return;
		}
	}
	if (all || !save_which)
		save_which = SFLAG_ALIAS | SFLAG_BIND | SFLAG_ON | SFLAG_SET |
			     SFLAG_NOTIFY | SFLAG_DIGRAPH | SFLAG_IGNORE;
	if (term_basic() || save_force)
		really_save(ircrc_file_path(), UP("y"));
	else
	{
		snprintf(CP(buffer), sizeof buffer, "Really write %s? ", ircrc_file_path());
		add_wait_prompt(buffer, really_save, ircrc_file_path(),
				WAIT_PROMPT_LINE);
	}
}

/*
 * do_channel : checks whether the channel has already been joined and
 * returns the channel's name if not
 */
u_char *
do_channel(u_char *chan, int force)
{
	u_char	*old;
	int	server = window_get_server(curr_scr_win);
	const	int	from_server = get_from_server();

	if (from_server < 0 || server < 0)
		return NULL;

	if (is_bound(chan, server) && channel_is_current_window(chan, server))
		say("Channel %s is bound", chan);
	else if (is_on_channel(chan, server, server_get_nickname(server)) ||
	    (server_get_version(from_server) == ServerICB &&
	     my_stricmp(server_get_icbgroup(from_server), chan) == 0))
	{
		is_current_channel(chan, server, 1);
		say("You are now talking to channel %s",
		    set_channel_by_refnum(0, chan));
		update_all_windows();
	}
	else
	{
		/* only do this if we're actually joining a new channel */
		if (get_int_var(NOVICE_VAR) &&
		    server_get_version(from_server) != ServerICB)
		{
			if ((old = get_channel_by_refnum(0)) &&
			    my_strcmp(old, zero()))
				send_to_server("PART %s", old);
		}
		add_channel(chan, 0, from_server, CHAN_JOINING, NULL);
		force = 1;
	}
	if (force)
		return chan;
	else
		return NULL;
}

/*
 * e_channel: does the channel command.  I just added displaying your current
 * channel if none is given 
 */
static	void
e_channel(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*chan;
	size_t	len;
	u_char	*chanstr = NULL,
		*ptr;
	int 	force = 0;
	const	int	from_server = get_from_server();

	save_message_from();
	message_from(NULL, LOG_CURRENT);
	if ((chan = next_arg(args, &args)) != NULL)
	{
		len = MAX(2, my_strlen(chan));
		if (my_strnicmp(chan, UP("-force"), len) == 0)
		{
			force = 1;
			if ((chan = next_arg(args, &args)) == NULL)
				goto out;	/* allow /alias join join -force */
			len = MAX(2, my_strlen(chan));
		}
		if (my_strnicmp(chan, UP("-invite"), len) == 0)
		{
			if (invite_channel)
			{
				if ((ptr = do_channel(invite_channel, force)))
				{
					if (server_get_version(from_server) == ServerICB)
						icb_put_group(invite_channel);
					else
						send_to_server("%s %s %s", command, invite_channel, args);
				}
				else
					say("You are already on %s ?", invite_channel);
			}
			else
				say("You have not been invited to a channel!");
		}
		else if (server_get_version(from_server) == ServerICB)
		{
			if (do_channel(chan, force))
				icb_put_group(chan);
		}
		else
		{
			malloc_strcpy(&chanstr, chan);
			chan = chanstr;

			ptr = my_strsep(&chanstr, UP(","));
			if ((ptr = do_channel(ptr, force)) && *ptr)
				send_to_server("%s %s %s", command, ptr, args);

			while (get_int_var(NOVICE_VAR) == 0 && (ptr = my_strsep(&chanstr, UP(","))))
				if ((ptr = do_channel(ptr, force)) && *ptr)
					send_to_server("%s %s %s", command, ptr, args);

			new_free(&chan);
		}
	}
	else
out:
		list_channels();
	restore_message_from();
}

/* comment: does the /COMMENT command, useful in .ircrc */
static	void
commentcmd(u_char *command, u_char *args, u_char *subargs)
{
	/* nothing to do... */
}

/*
 * e_nick: does the /NICK command.  Records the users current nickname and
 * sends the command on to the server 
 */
static	void
e_nick(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*nick;
	const	int	from_server = get_from_server();

	if (!(nick = next_arg(args, &args)))
	{
		say("Your nickname is %s",
			server_get_nickname(get_window_server(0)));
		return;
	}
	if (server_get_version(from_server) == ServerICB)
	{
		icb_put_nick(nick);
		return;
	}
	send_to_server("NICK %s", nick);
	if (server_get_attempting_to_connect(from_server))
		server_set_nickname(get_window_server(0), nick);
}

/* version: does the /VERSION command with some IRCII version stuff */
static	void
version(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*host;
	const	int	from_server = get_from_server();

	if (server_get_version(from_server) != ServerICB && ((host = next_arg(args, &args)) != NULL))
	{
		send_to_server("%s %s", command, host);
	}
	else
	{ 
		say("Client: ircII %s", irc_version());
		if (server_get_version(from_server) == ServerICB)
			icb_put_version(args);
		else
			send_to_server("%s", command);
	}
}

/*
 * info: does the /INFO command.  I just added some credits
 * I updated most of the text -phone, feb 1993.
 */
static	void
info(u_char *command, u_char *args, u_char *subargs)
{
	if (!args || !*args)
	{
		say("This is ircII version %s", irc_version());
		say(" - originally written by Michael Sandrof");
		say(" - versions 2.1 to 2.2pre7 by Troy Rollo");
		say(" - development continued by matthew green");
		say("       e-mail: mrg@eterna.com.au  irc: phone");
		say(" - copyright (c) 1990-2015");
		say(" - do a /help ircii copyright for the full copyright");
		say(" - ircii includes software developed by the university");
		say("   of california, berkeley and its contributors");
		say("%s", "");
		say("ircii contributors");
		say("%s", "");
		say("\tMichael Sandrof       Mark T. Dameu");
		say("\tStellan Klebom        Carl v. Loesch");
		say("\tTroy Rollo            Martin  Friedrich");
		say("\tMichael Weber         Bill Wisner");
		say("\tRiccardo Facchetti    Stephen van den Berg");
		say("\tVolker Paulsen        Kare Pettersson");
		say("\tIan Frechette         Charles Hannum");
		say("\tmatthew green         christopher williams");
		say("\tJonathan Lemon        Brian Koehmstedt");
		say("\tNicolas Pioch         Brian Fehdrau");
		say("\tDarren Reed           Jeff Grills");
		say("\tJeremy Nelson         Philippe Levan");
		say("\tScott Reynolds        Glen McCready");
		say("\tChristopher Kalt      Joel Yliluoma");
		say("\tFlier");
	}
	if (server_get_version(get_from_server()) != ServerICB)
		send_to_server("%s %s", command, args);
}

void
ison_now(WhoisStuff *notused, u_char *nicklist, u_char *notused2)
{
	if (do_hook(current_numeric(), "%s", nicklist))
		put_it("%s Currently online: %s", numeric_banner(), nicklist);
}

static	void
ison(u_char *command, u_char *args, u_char *subargs)
{
	if (!args[strspn(CP(args), " ")])
		args = server_get_nickname(get_from_server());
	add_ison_to_whois(args, ison_now);
}

/*
 * userhost: Does the USERHOST command.  Need to split up the queries,
 * since the server will only reply to 5 at a time.
 */
static	void
userhost(u_char *command, u_char *args, u_char *subargs)
{
	int	n = 0,
		total = 0,
		userhost_cmd = 0;
	u_char	*nick;
	u_char	buffer[BIG_BUFFER_SIZE];

	while ((nick = next_arg(args, &args)) != NULL)
	{
		size_t	len;

		++total;
		len = my_strlen(nick);
		if (!my_strnicmp(nick, UP("-CMD"), len))
		{
			if (total < 2)
			{
				yell("userhost -cmd with no nick!");
				return;
			}
			userhost_cmd = 1;
			break;
		}
		else
		{
			if (n++)
				my_strmcat(buffer, " ", sizeof buffer);
			else
				*buffer = '\0';
			my_strmcat(buffer, nick, sizeof buffer);
		}
	}
	if (n)
	{
		u_char	*the_list = NULL;
		u_char	*s, *t;
		int	i;

		malloc_strcpy(&the_list, buffer);
		s = t = the_list;
		while (n)
		{
			for (i = 5; i && *s; s++)
				if (' ' == *s)
					i--, n--;
			if (' ' == *(s - 1))
				*(s - 1) = '\0';
			else
				n--;
			my_strmcpy(buffer, t, sizeof buffer);
			t = s;

			if (userhost_cmd)
				add_to_whois_queue(buffer, userhost_cmd_returned, "%s", args);
			else
				add_to_whois_queue(buffer, USERHOST_USERHOST, 0);
		}
		new_free(&the_list);
	}
	else if (!total)
		/* Default to yourself.  */
		add_to_whois_queue(server_get_nickname(get_from_server()), USERHOST_USERHOST, 0);
}

/*
 * whois: the WHOIS and WHOWAS commands.  This translates the 
 * to the whois handlers in whois.c 
 */
static	void
whois(u_char *command, u_char *args, u_char *subargs)
{
	const	int	from_server = get_from_server();

	if (server_get_version(from_server) == ServerICB)
	{
		unsigned display;
		u_char	*buf;
		size_t	len;

		display = set_display_off();
		len = 7 + 1 + my_strlen(args) + 1;
		buf = new_malloc(len);
		snprintf(CP(buf), len, "whereis %s", args);
		icb_put_msg2(UP("server"), buf);
		new_free(&buf);
		set_display(display);
		return;
	}

	if (args && *args)
		send_to_server("%s %s", command, args);
	else /* Default to yourself  -lynx */
		send_to_server("%s %s", command, server_get_nickname(from_server));
}

/*
 * who: the /WHO command. Parses the who switches and sets the who_mask and
 * whoo_stuff accordingly.  Who_mask and who_stuff are used in whoreply() in
 * parse.c 
 */
static	void
who(u_char *command, u_char *args, u_char *subargs)
{
	WhoInfo	*wi = server_get_who_info();
	u_char	*arg,
		*channel = NULL;
	int	no_args = 1;
	size_t	len;

	if (server_get_version(get_from_server()) == ServerICB)
	{
		icb_put_who(args);
		return;
	}

	wi->who_mask = 0;
	new_free(&wi->who_name);
	new_free(&wi->who_host);
	new_free(&wi->who_server);
	new_free(&wi->who_file);
	new_free(&wi->who_nick);
	new_free(&wi->who_real);

	while ((arg = next_arg(args, &args)) != NULL)
	{
		no_args = 0;
		if ((*arg == '-') && (!isdigit(*(arg + 1))))
		{
			u_char	*cmd = NULL;

			arg++;
			if ((len = my_strlen(arg)) == 0)
			{
				say("Unknown or missing flag");
				return;
			}
			malloc_strcpy(&cmd, arg);
			lower(cmd);
			if (my_strncmp(cmd, "operators", len) == 0)
				wi->who_mask |= WHO_OPS;
			else if (my_strncmp(cmd, "lusers", len) == 0)
				wi->who_mask |= WHO_LUSERS;
			else if (my_strncmp(cmd, "chops", len) == 0)
				wi->who_mask |= WHO_CHOPS;
			else if (my_strncmp(cmd, "hosts", len) == 0)
			{
				if ((arg = next_arg(args, &args)) != NULL)
				{
					wi->who_mask |= WHO_HOST;
					malloc_strcpy(&wi->who_host, arg);
					channel = wi->who_host;
				}
				else
				{
					say("WHO -HOSTS: missing arguement");
					new_free(&cmd);
					return;
				}
			}
			else if (my_strncmp(cmd, "here", len) ==0)
				wi->who_mask |= WHO_HERE;
			else if (my_strncmp(cmd, "away", len) ==0)
				wi->who_mask |= WHO_AWAY;
			else if (my_strncmp(cmd, "servers", len) == 0)
			{
				if ((arg = next_arg(args, &args)) != NULL)
				{
					wi->who_mask |= WHO_SERVER;
					malloc_strcpy(&wi->who_server, arg);
					channel = wi->who_server;
				}
				else
				{
					say("WHO -SERVERS: missing arguement");
					new_free(&cmd);
					return;
				}
			}
			else if (my_strncmp(cmd, "name", len) == 0)
			{
				if ((arg = next_arg(args, &args)) != NULL)
				{
					wi->who_mask |= WHO_NAME;
					malloc_strcpy(&wi->who_name, arg);
					channel = wi->who_name;
				}
				else
				{
					say("WHO -NAME: missing arguement");
					new_free(&cmd);
					return;
				}
			}
			else if (my_strncmp(cmd, "realname", len) == 0)
			{
				if ((arg = next_arg(args, &args)) != NULL)
				{
					wi->who_mask |= WHO_REAL;
					malloc_strcpy(&wi->who_real, arg);
					channel = wi->who_real;
				}
				else
				{
					say("WHO -REALNAME: missing arguement");
					new_free(&cmd);
					return;
				}
			}
			else if (my_strncmp(cmd, "nick", len) == 0)
			{
				if ((arg = next_arg(args, &args)) != NULL)
				{
					wi->who_mask |= WHO_NICK;
					malloc_strcpy(&wi->who_nick, arg);
					channel = wi->who_nick;
				}
				else
				{
					say("WHO -NICK: missing arguement");
					new_free(&cmd);
					return;
				}
				/* WHO -FILE by Martin 'Efchen' Friedrich */
			}
			else if (my_strncmp(cmd, "file", len) == 0)
			{
				wi->who_mask |= WHO_FILE;
				if ((arg = next_arg(args, &args)) != NULL)
				{
					malloc_strcpy(&wi->who_file, arg);
				}
				else
				{
					say("WHO -FILE: missing arguement");
					new_free(&cmd);
					return;
				}
			}
			else
			{
				say("Unknown or missing flag");
				new_free(&cmd);
				return;
			}
			new_free(&cmd);
		}
		else if (my_strcmp(arg, "*") == 0)
		{
			channel = get_channel_by_refnum(0);
			if (!channel || *channel == '0')

			{
				say("I wouldn't do that if I were you");
				return;
			}
		}
		else
			channel = arg;
	}
	if (no_args)
		say("No argument specified");
	else
	{
		u_char *o = NULL;

		if (wi->who_mask & WHO_OPS)
			o = UP(" o");
		if (!channel && wi->who_mask & WHO_OPS)
			channel = UP("*");
		send_to_server("%s %s%s", command,
				channel ? channel : empty_string(),
				o ? o : empty_string());
	}
}

int
whoreply_check(u_char *channel, u_char *user, u_char *host, u_char *server,
	       u_char *nick, u_char *status, u_char *name, u_char **ArgList,
	       u_char *format)
{
	WhoInfo	*wi = server_get_who_info();
	int ok = 1;

	if (*status == 'S')	/* this only true for the header WHOREPLY */
	{
		FILE *fp = NULL;

		channel = UP("Channel");
		if ((wi->who_mask & WHO_FILE) == 0 || (fp = fopen(CP(wi->who_file), "r")))
		{
			if (do_hook(WHO_LIST, "%s %s %s %s %s %s", channel,
					nick, status, user, host, ArgList[6]))
				put_it(CP(format), channel, nick, status, user,
					host, ArgList[6]);
			if (fp)
				fclose(fp);
			/* whoreply() does nothing here */
			return 0;
		}
	}

	if (wi->who_mask)
	{
		if (wi->who_mask & WHO_HERE)
			ok = ok && (*status == 'H');
		if (wi->who_mask & WHO_AWAY)
			ok = ok && (*status == 'G');
		if (wi->who_mask & WHO_OPS)
			ok = ok && (*(status + 1) == '*');
		if (wi->who_mask & WHO_LUSERS)
			ok = ok && (*(status + 1) != '*');
		if (wi->who_mask & WHO_CHOPS)
			ok = ok && ((*(status + 1) == '@') ||
			(*(status + 2) == '@'));
		if (wi->who_mask & WHO_NAME)
			ok = ok && wild_match(wi->who_name, user);
		if (wi->who_mask & WHO_NICK)
			ok = ok && wild_match(wi->who_nick, nick);
		if (wi->who_mask & WHO_HOST)
			ok = ok && wild_match(wi->who_host, host);
		if (wi->who_mask & WHO_REAL)
			ok = ok && wild_match(wi->who_real, name);
		if (wi->who_mask & WHO_SERVER)
			ok = ok && wild_match(wi->who_server, server);
		if (wi->who_mask & WHO_FILE)
		{
			FILE	*fip;

			ok = 0;
			if ((fip = fopen(CP(wi->who_file), "r")) != NULL)
			{
				u_char	buf_data[BUFSIZ];

				while (fgets ((char *)buf_data, BUFSIZ, fip) !=
								NULL)
				{
					buf_data[my_strlen(buf_data)-1] = '\0';
					ok = ok || wild_match(buf_data, nick);
				}
				fclose (fip);
			} else
				yell("Cannot open: %s", wi->who_file);
		}
	}

	return ok;
}

/*
 * query: the /QUERY command.  Works much like the /MSG, I'll let you figure
 * it out.
 */
void
query(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*nick,
		*rest;

	save_message_from();
	message_from(NULL, LOG_CURRENT);
	if ((nick = next_arg(args, &rest)) != NULL)
	{
		if (my_strcmp(nick, ".") == 0)
		{
			if (!(nick = sent_nick))
			{
				say("You have not messaged anyone yet");
				goto out;
			}
		}
		else if (my_strcmp(nick, ",") == 0)
		{
			if (!(nick = recv_nick))
			{
				say("You have not recieved a message from \
						anyone yet");
				goto out;
			}
		}
		else if (my_strcmp(nick, "*") == 0)
			if (!(nick = get_channel_by_refnum(0)))
			{
				say("You are not on a channel");
				goto out;
			}

		if (*nick == '%')
		{
			if (is_process(nick) == 0)
			{
				say("Invalid processes specification");
				goto out;
			}
		}
		say("Starting conversation with %s", nick);
		set_query_nick(nick);
	}
	else
	{
		if (query_nick())
		{
			say("Ending conversation with %s", query_nick());
			set_query_nick(NULL);
		}
		else
			say("You aren't querying anyone!");
	}
	update_input(UPDATE_ALL);
out:
	restore_message_from();
}

/*
 * away: the /AWAY command.  Keeps track of the away message locally, and
 * sends the command on to the server.
 */
static	void
away(u_char *command, u_char *args, u_char *subargs)
{
	size_t	len;
	u_char	*arg = NULL;
	int	flag = AWAY_ONE;
	int	i;

	if (*args)
	{
		if (*args == '-')
		{
			u_char	*cmd = NULL;

			args = next_arg(args, &arg);
			len = my_strlen(args);
			if (len == 0)
			{
				say("%s: No argument given with -", command);
				return;
			}
			malloc_strcpy(&cmd, args);
			upper(cmd);
			if (0 == my_strncmp(cmd, "-ALL", len))
			{
				flag = AWAY_ALL;
				args = arg;
			}
			else if (0 == my_strncmp(cmd, "-ONE", len))
			{
				flag = AWAY_ONE;
				args = arg;
			}
			else
			{
				say("%s: %s unknown flag", command, args);
				new_free(&cmd);
				return;
			}
			new_free(&cmd);
		}
	}
	if (flag == AWAY_ALL)
	{
		if (*args)
		{
			away_set = 1;
			MarkAllAway(command, args);
		}
		else
		{
			away_set = 0;
			for (i = 0; i < number_of_servers(); i++)
				server_set_away(i, NULL);
		}
	}
	else
	{
		send_to_server("%s :%s", command, args);
		if (*args)
		{
			away_set = 1;
			server_set_away(window_get_server(curr_scr_win), args);
		}
		else
		{
			server_set_away(window_get_server(curr_scr_win), NULL);
			away_set = 0;
			for (i = 0; i < number_of_servers(); i++)
				if (is_server_connected(i) &&
				    server_get_away(i))
				{
					away_set = 1;
					break;
				}
		}
	}
	update_all_status();
}

/* e_quit: The /QUIT, /EXIT, etc command */
void
e_quit(u_char *command, u_char *args, u_char *subargs)
{
	int	max;
	int	i;
	u_char	*Reason;

	Reason = ((args && *args) ? args : (u_char *) "Leaving");
	max = number_of_servers();
	for (i = 0; i < max; i++)
		if (is_server_connected(i))
		{
			set_from_server(i);
			if (server_get_version(i) != ServerICB)
				send_to_server("QUIT :%s", Reason);
		}
	irc_exit();
}

/* flush: flushes all pending stuff coming from the server */
static	void
flush(u_char *command, u_char *args, u_char *subargs)
{
	if (get_int_var(HOLD_MODE_VAR))
	{
		while (window_held_lines(curr_scr_win))
			window_remove_from_hold_list(curr_scr_win);
		window_hold_mode(NULL, OFF, 1);
	}
	say("Standby, Flushing server output...");
	flush_server();
	say("Done");
}

/* e_wall: used for WALL and WALLOPS */
static	void
e_wall(u_char *command, u_char *args, u_char *subargs)
{
	const	int	from_server = get_from_server();

	save_message_from();
	if (my_strcmp(command, "WALL") == 0)
	{	/* I hate this */
		message_from(NULL, LOG_WALL);
		if (!server_get_operator(from_server))
			put_it("## %s", args);
	}
	else
	{
		message_from(NULL, LOG_WALLOP);
		if (!server_get_flag(from_server, USER_MODE_W))
			put_it("!! %s", args);
	}
	if (!in_on_who())
		send_to_server("%s :%s", command, args);
	restore_message_from();
}

void
redirect_msg(u_char *dest, u_char *msg)
{
	u_char	buffer[BIG_BUFFER_SIZE];

	my_strmcpy(buffer, dest, sizeof buffer);
	my_strmcat(buffer, " ", sizeof buffer);
	my_strmcat(buffer, msg, sizeof buffer);
	e_privmsg(UP("PRIVMSG"), buffer, NULL);
}

/*
 * e_privmsg: The MSG command, displaying a message on the screen indicating
 * the message was sent.  Also, this works for the NOTICE command. 
 */
static	void
e_privmsg(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*nick;
	int old_from_server = -2;

	if ((nick = next_arg(args, &args)) != NULL)
	{
		if (my_stricmp(nick, UP("-server")) == 0)
		{
			u_char *server;
			int sgroup, target_server = -1;

			if ((server = next_arg(args, &args)) == NULL)
			{
bad_server:
				say("You must supply a valid server name or "
				    "group with -server");
				goto out;
			}
			if ((nick = next_arg(args, &args)) == NULL)
				goto missing_nick;
			sgroup = find_server_group(server, 0);
			if (sgroup == 0) {
				target_server = my_atoi(server);
				if (target_server < 0 || target_server >= number_of_servers())
				{
					u_char	*port = NULL,
						*nick2 = NULL,
						*foo = NULL;
					int foo2, port_num = 0;
					server_ssl_level level = SSL_OFF;

					parse_server_info(&server, &port, &foo,
							  &nick2, &foo, &foo,
							  &foo2, &level, &foo,
							  &foo2);
					if (port && *port)
						port_num = my_atoi(port);
					target_server = find_in_server_list(server,
								port_num, nick2);
				}
			} else
				target_server = active_server_group(sgroup);

			if (target_server == -1)
				goto bad_server;

			old_from_server = set_from_server(target_server);
		}

		if (my_strcmp(nick, ".") == 0)
		{
			if (!(nick = sent_nick))
			{
				say("You haven't sent a message to anyone yet");
				goto out;
			}
		}

		else if (my_strcmp(nick, ",") == 0)
		{
			if (!(nick = recv_nick))
			{
				say("You have not received a message from anyone yet");
				goto out;
			}
		}
		else if (!my_strcmp(nick, "*"))
			if (!(nick = get_channel_by_refnum(0)))
				nick = zero();
		send_text(nick, args, command);
	}
	else
missing_nick:
		say("You must specify a nickname or channel!");
out:
	if (old_from_server != -2)
		set_from_server(old_from_server);
}

/*
 * quote: handles the QUOTE command.  args are a direct server command which
 * is simply send directly to the server 
 */
static	void
quote(u_char *command, u_char *args, u_char *subargs)
{
	if (!in_on_who())
		send_to_server("%s", args);
}

/* clear: the CLEAR command.  Figure it out */
static	void
my_clear(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*arg;
	int	all = 0,
		unhold = 0;

	while ((arg = next_arg(args, &args)) != NULL)
	{
		upper(arg);
		/* -ALL and ALL here becuase the help files used to be wrong */
		if (!my_strncmp(arg, "ALL", my_strlen(arg))
				|| !my_strncmp(arg, "-ALL", my_strlen(arg)))
			all = 1;
		else if (!my_strncmp(arg, "-UNHOLD", my_strlen(arg)))
			unhold = 1;
		else
			say("Unknown flag: %s", arg);
	}
	if (all)
		clear_all_windows(unhold);
	else
	{
		if (unhold)
			window_hold_mode(NULL, OFF, 1);
		clear_window_by_refnum(0);
	}
	update_input(UPDATE_JUST_CURSOR);
}

/*
 * send_comm: the generic command function.  Uses the full command name found
 * in 'command', combines it with the 'args', and sends it to the server 
 */
static	void
send_comm(u_char *command, u_char *args, u_char *subargs)
{
	if (args && *args)
		send_to_server("%s %s", command, args);
	else
		send_to_server("%s", command);
}

static	void
send_invite(u_char *command, u_char *args, u_char *subargs)
{
	if (server_get_version(get_from_server()) == ServerICB)
		icb_put_invite(args);
	else
		send_comm(command, args, subargs);
}

static	void
send_motd(u_char *command, u_char *args, u_char *subargs)
{
	if (server_get_version(get_from_server()) == ServerICB)
		icb_put_motd(args);
	else
		send_comm(command, args, subargs);
}

static	void
send_topic(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*arg;
	u_char	*arg2;

	if (server_get_version(get_from_server()) == ServerICB)
	{
		icb_put_topic(args);
		return;
	}
	else
	if (!(arg = next_arg(args, &args)) || (my_strcmp(arg, "*") == 0))
		arg = get_channel_by_refnum(0);

	if (!arg)
	{
		say("You aren't on a channel in this window");
		return;
	}
	if (is_channel(arg))
	{
		if ((arg2 = next_arg(args, &args)) != NULL)
			send_to_server("%s %s :%s %s", command, arg,
					arg2, args);
		else
			send_to_server("%s %s", command, arg);
	}
	else
	if (get_channel_by_refnum(0))
		send_to_server("%s %s :%s", command, get_channel_by_refnum(0), subargs);
	else
		say("You aren't on a channel in this window");
}

static void
send_squery(u_char *command, u_char *args, u_char *subargs)
{
	put_it("*** Sent to service %s: %s", command, args);
	send_2comm(command, args, subargs);
}

static void
send_brick(u_char *command, u_char *args, u_char *subargs)
{

	if (args && *args)
	{
		u_char	*channel;

		channel = next_arg(args, &args);
		send_to_server("%s %s :%s", command, channel, args);
	}
	else
		send_to_server("%s", command);
}

/*
 * send_2comm: Sends a command to the server with one arg and
 * one comment. Used for KILL and SQUIT.
 */
static	void
send_2comm(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*comment;

	args = next_arg(args, &comment);
	
	send_to_server("%s %s %c%s",
		       command,
		       args && *args ? args : (u_char *) "",
		       comment && *comment ? ':' : ' ',
		       comment && *comment ? comment : (u_char *) "");
}

/*
 * send_channel_nargs: Sends a command to the server with one channel,
 * and 0-n args. Used for MODE.
 */

static	void
send_channel_nargs(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*arg1 = 0,
	        *s = get_channel_by_refnum(0);

	args = next_arg(args, &arg1);
	if (!args || !my_strcmp(args, "*"))
	{
		if (s)
			args = s;
		else
		{
			say("You aren't on a channel in this window");
			return;
		}
	}

	send_to_server("%s %s %s",
		       command,
		       args,
		       arg1 && *arg1 ? arg1 : (u_char *) "");
}

/*
 * send_channel_2args: Sends a command to the server with one channel,
 * one arg and one comment. Used for KICK
 */

static	void
send_channel_2args(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*arg1 = 0,
		*comment = 0,
	        *s = get_channel_by_refnum(0);

	args = next_arg(args, &arg1);
	if (!args || !my_strcmp(args, "*"))
	{
		if (s)
			args = s;
		else
		{
			say("You aren't on a channel in this window");
			return;
		}
	}

	if (arg1 && *arg1)
		arg1 = next_arg(arg1, &comment);
	
	send_to_server("%s %s %s %c%s",
		       command,
		       args,
		       arg1 && *arg1 ? arg1 : (u_char *) "",
		       comment && *comment ? ':' : ' ',
		       comment && *comment ? comment : (u_char *) "");
}

/*
 * send_channel_1arg: Sends a command to the server with one channel
 * and one comment. Used for PART (LEAVE)
 */
static	void
send_channel_1arg(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*comment,
		*s = get_channel_by_refnum(0);

	args = next_arg(args, &comment);

	if (!args || !my_strcmp(args, "*"))
	{
		if (s)
			args = s;
		else
		{
			say("You aren't on a channel in this window");
			return;
		}
	}

	send_to_server("%s %s %c%s",
		       command,
		       args,
		       comment && *comment ? ':' : ' ',
		       comment && *comment ? comment : (u_char *) "");
}

/*
 * send_text: Sends the line of text to whomever the user is currently
 * talking.  If they are quering someone, it goes to that user, otherwise
 * it's the current channel.  Some tricky parameter business going on. If
 * nick is null (as if you had typed "/ testing" on the command line) the
 * line is sent to your channel, and the command parameter is never used. If
 * nick is not null, and command is null, the line is sent as a PRIVMSG.  If
 * nick is not null and command is not null, the message is sent using
 * command.  Currently, only NOTICEs and PRIVMSGS work. 
 * fixed to not be anal about "/msg foo,bar foobar" -phone
 */
void
send_text(u_char *org_nick, u_char *line, u_char *command)
{
	crypt_key	*key;
	u_char 	*ptr,
		*free_nick,
		*nick = NULL;
	int	lastlog_level,
		list_type;
	int	check_away = 0;
	u_char	the_thing;
	u_char	*query_command = NULL;
	u_char	nick_list[IRCD_BUFFER_SIZE];
	int	do_final_send = 0;
	const	int	from_server = get_from_server();
	int	is_icb = server_get_version(from_server) == ServerICB;
	
	*nick_list = '\0';
	malloc_strcpy(&nick, org_nick);
	free_nick = ptr = nick;
	save_message_from();
	while ((nick = ptr) != NULL)
	{
		if (is_icb && is_current_channel(nick, window_get_server(curr_scr_win), 0))
			ptr = NULL;
		else if ((ptr = my_index(nick, ',')) != NULL)
			*(ptr++) = (u_char) 0;
		if (!*nick)
			continue;
		if (is_process(nick))
		{
			int	i;

			if ((i = get_process_index(&nick)) == -1)
				say("Invalid process specification");
			else
				text_to_process(i, line, 1);
			continue;
		}
		if (!*line)
			continue; /* lynx */
		if (in_on_who() && *nick != '=') /* allow dcc messages anyway */
		{
			say("You may not send messages from ON WHO, ON WHOIS, or ON JOIN");
			continue;
		}
		if (doing_privmsg())
			command	= UP("NOTICE");
		if (is_current_channel(nick, window_get_server(curr_scr_win), 0))
		{
			/* nothing */
		}
		else if (*nick == '\\') /* quoteme */
			nick++;
		else if (my_strcmp(nick, "\"") == 0) /* quote */
		{
			send_to_server("%s", line);
			continue;
		}
		else if (*nick == '=') /* DCC chat */
		{
			set_from_server(-1);
			dcc_chat_transmit(nick + 1, line);
			set_from_server(from_server);
			continue;
		}
		else if (*nick == '/') /* Command */
		{
			malloc_strcpy(&query_command, nick);
			malloc_strcat(&query_command, UP(" "));
			malloc_strcat(&query_command, line);
			parse_command(query_command, 0, empty_string());
			new_free(&query_command);
			continue;
		}
		if (current_dcc_hook() == NOTICE_LIST)
		{
			say("You cannot send a message from a NOTICE");
			new_free(&free_nick);
			goto out;
		}
		if (is_icb)
		{
			if (my_stricmp(nick, server_get_icbgroup(from_server)) != 0 ||
			    (command && my_strcmp(command, "PRIVMSG") == 0)) {
				icb_put_msg2(nick, line);
				malloc_strcpy(&sent_nick, nick);
			} else
				icb_put_public(line);
		}
		else
		{	/* special */
		if (current_dcc_hook() == MSG_LIST)
			command = UP("NOTICE");
		if (is_channel(nick))
		{
			int	current;

			current = is_current_channel(nick, window_get_server(curr_scr_win), 0);
			if (!command || my_strcmp(command, "NOTICE"))
			{
				check_away = 1;
				command = UP("PRIVMSG");
				lastlog_level = set_lastlog_msg_level(LOG_PUBLIC);
				message_from(nick, LOG_PUBLIC);
				list_type = SEND_PUBLIC_LIST;
				the_thing = '>';
			}
			else
			{
				lastlog_level = set_lastlog_msg_level(LOG_NOTICE);
				message_from(nick, LOG_NOTICE);
				list_type = SEND_NOTICE_LIST;
				the_thing = '-';
			}
			if (do_hook(list_type, "%s %s", nick, line))
			{
				if (current)
					put_it("%c %s", the_thing, line);
				else
					put_it("%c%s> %s", the_thing, nick,
						line);
			}
			set_lastlog_msg_level(lastlog_level);
			if ((key = is_crypted(nick)) != 0)
			{
				u_char	*crypt_line;

				if ((crypt_line = crypt_msg(line, key, 1)))
					send_to_server("%s %s :%s", command, nick, crypt_line);
				continue;
			}
			if (!in_on_who())
			{
				if (*nick_list)
				{
					my_strmcat(nick_list, ",", sizeof nick_list);
					my_strmcat(nick_list, nick, sizeof nick_list);
				}
				else
					my_strmcpy(nick_list, nick, sizeof nick_list);
			}
			do_final_send = 1;
		}
		else
		{
			if (!command || my_strcmp(command, "NOTICE"))
			{
				check_away = 1;
				lastlog_level = set_lastlog_msg_level(LOG_MSG);
				command = UP("PRIVMSG");
				message_from(nick, LOG_MSG);
				list_type = SEND_MSG_LIST;
				the_thing = '*';
			}
			else
			{
				lastlog_level = set_lastlog_msg_level(LOG_NOTICE);
				message_from(nick, LOG_NOTICE);
				list_type = SEND_NOTICE_LIST;
				the_thing = '-';
			}
			if (get_display() && do_hook(list_type, "%s %s", nick, line))
				put_it("-> %c%s%c %s", the_thing, nick, the_thing, line);
			if ((key = is_crypted(nick)) != NULL)
			{
				u_char	*crypt_line;

				if ((crypt_line = crypt_msg(line, key, 1)))
					send_to_server("%s %s :%s", command ? command : (u_char *) "PRIVMSG", nick, crypt_line);
				continue;
			}
			set_lastlog_msg_level(lastlog_level);

			if (!in_on_who())
			{
				if (*nick_list)
				{
					my_strmcat(nick_list, ",", sizeof nick_list);
					my_strmcat(nick_list, nick, sizeof nick_list);
				}
				else
					my_strmcpy(nick_list, nick, sizeof nick_list);
			}

			if (get_int_var(WARN_OF_IGNORES_VAR) && (is_ignored(nick, IGNORE_MSGS) == IGNORED))
				say("Warning: You are ignoring private messages from %s", nick);

			malloc_strcpy(&sent_nick, nick);
			do_final_send = 1;
		}
		}	/* special */
	}
	if (check_away && server_get_away(window_get_server(curr_scr_win)) &&
	    get_int_var(AUTO_UNMARK_AWAY_VAR) &&
	    server_get_version(from_server) != ServerICB)
		away(UP("AWAY"), empty_string(), empty_string());

	malloc_strcpy(&sent_body, line);
	if (do_final_send)
		send_to_server("%s %s :%s", command ? command : (u_char *) "PRIVMSG", nick_list, line);
	new_free(&free_nick);
out:
	restore_message_from();
}

static void
do_send_text(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*tmp;

	if (command)
		tmp = get_channel_by_refnum(0);
	else
		tmp = get_target_by_refnum(0);
	send_text(tmp, args, NULL);
}

/*
 * command_completion: builds lists of commands and aliases that match the
 * given command and displays them for the user's lookseeing 
 */
void
command_completion(u_int key, u_char *ptr)
{
	int	do_aliases;
	int	cmd_cnt,
		alias_cnt,
		i,
		c,
		len;
	u_char	**aliases = NULL;
	u_char	*line = NULL,
		*com,
		*cmdchars,
		*rest,
		firstcmdchar = '/';
	u_char	buffer[BIG_BUFFER_SIZE];
	IrcCommand	*command;

	malloc_strcpy(&line, get_input());
	if ((com = next_arg(line, &rest)) != NULL)
	{
		if (!(cmdchars = get_string_var(CMDCHARS_VAR)))
			cmdchars = UP(DEFAULT_CMDCHARS);
		if (my_index(cmdchars, *com))
		{
			firstcmdchar = *cmdchars;
			com++;
			if (*com && my_index(cmdchars, *com))
			{
				do_aliases = 0;
				alias_cnt = 0;
				com++;
			}
			else
				do_aliases = 1;
			upper(com);
			if (do_aliases)
				aliases = match_alias(com, &alias_cnt,
					COMMAND_ALIAS);
			if ((command = find_command(com, &cmd_cnt)) != NULL)
			{
				if (cmd_cnt < 0)
					cmd_cnt *= -1;
				/* special case for the empty string */

				if (*(command[0].name) == (u_char) 0)
				{
					command++;
					cmd_cnt = NUMBER_OF_COMMANDS;
				}
			}
			if ((alias_cnt == 1) && (cmd_cnt == 0))
			{
				snprintf(CP(buffer), sizeof buffer, "%c%s %s", firstcmdchar,
					aliases[0], rest);
				set_input(buffer);
				new_free(&(aliases[0]));
				new_free(&aliases);
				update_input(UPDATE_ALL);
			}
			else if (((cmd_cnt == 1) && (alias_cnt == 0)) ||
			    ((cmd_cnt == 1) && (alias_cnt == 1) &&
			    (my_strcmp(aliases[0], command[0].name) == 0)))
			{
				snprintf(CP(buffer), sizeof buffer, "%c%s%s %s", firstcmdchar,
					do_aliases ? (u_char *) "" : &firstcmdchar,
					command[0].name, rest);
				set_input(buffer);
				update_input(UPDATE_ALL);
			}
			else
			{
				*buffer = (u_char) 0;
				if (command)
				{
					say("Commands:");
					my_strmcpy(buffer, "\t", sizeof buffer);
					c = 0;
					for (i = 0; i < cmd_cnt; i++)
					{
						my_strmcat(buffer, command[i].name,
							sizeof buffer);
						for (len =
						    my_strlen(command[i].name);
						    len < 15; len++)
							my_strmcat(buffer, " ",
							    sizeof buffer);
						if (++c == 4)
						{
							say("%s", buffer);
							my_strmcpy(buffer, "\t",
							    sizeof buffer);
							c = 0;
						}
					}
					if (c)
						say("%s", buffer);
				}
				if (aliases)
				{
					say("Aliases:");
					my_strmcpy(buffer, "\t", sizeof buffer);
					c = 0;
					for (i = 0; i < alias_cnt; i++)
					{
						my_strmcat(buffer, aliases[i],
							sizeof buffer);
						for (len = my_strlen(aliases[i]);
								len < 15; len++)
							my_strmcat(buffer, " ",
							    sizeof buffer);
						if (++c == 4)
						{
							say("%s", buffer);
							my_strmcpy(buffer, "\t",
							    sizeof buffer);
							c = 0;
						}
						new_free(&(aliases[i]));
					}
					if ((int) my_strlen(buffer) > 1)
						say("%s", buffer);
					new_free(&aliases);
				}
				if (!*buffer)
					term_beep();
			}
		}
		else
			term_beep();
	}
	else
		term_beep();
	new_free(&line);
}

/*
 * parse_line: This is the main parsing routine.  It should be called in
 * almost all circumstances over parse_command().
 *
 * parse_line breaks up the line into commands separated by unescaped
 * semicolons if we are in non interactive mode. Otherwise it tries to leave
 * the line untouched.
 *
 * Currently, a carriage return or newline breaks the line into multiple
 * commands too. This is expected to stop at some point when parse_command
 * will check for such things and escape them using the ^P convention.
 * We'll also try to check before we get to this stage and escape them before
 * they become a problem.
 *
 * Other than these two conventions the line is left basically untouched.
 */
void
parse_line(u_char *name, u_char *org_line, u_char *args, int hist_flag, int append_flag, int eat_space)
{
	u_char	*line = NULL,
		*free_line, *stuff, *start, *lbuf, *s, *t;
	int	args_flag;

	malloc_strcpy(&line, org_line);
	free_line = line;
	args_flag = 0;
	if (!*line)
		do_send_text(NULL, empty_string(), empty_string());
	else if (args)
		do
		{
			stuff = expand_alias(name, line, args, &args_flag,
					&line);
			start = stuff;
			if (eat_space)
				for (; isspace(*start); start++)
					;       

			if (!line && append_flag && !args_flag && args && *args)
			{
				lbuf = new_malloc(my_strlen(stuff) + 1 + my_strlen(args) + 1);
				my_strcpy(lbuf, start);
				my_strcat(lbuf, " ");
				my_strcat(lbuf, args);
				new_free(&stuff);
				start = stuff = lbuf;
			}
			parse_command(start, hist_flag, args);
			new_free(&stuff);
		}
		while (line);
	else
	{
		start = line;
		if (eat_space)
			for (; isspace(*start); start++)
				;
		if (load_depth)
			parse_command(start, hist_flag, args);
		else
			while ((s = line))
			{
				if ((t = sindex(line, UP("\r\n"))) != NULL)
				{
					*t++ = '\0';
					if (eat_space)
						for (; isspace(*t); t++)
							;
					line = t;
				}
				else
					line = NULL;
				parse_command(s, hist_flag, args);
			}
	}
	new_free(&free_line);
	return;
}

/*
 * parse_command: parses a line of input from the user.  If the first
 * character of the line is equal to irc_variable[CMDCHAR_VAR].value, the
 * line is used as an irc command and parsed appropriately.  If the first
 * character is anything else, the line is sent to the current channel or to
 * the current query user.  If hist_flag is true, commands will be added to
 * the command history as appropriate.  Otherwise, parsed commands will not
 * be added. 
 *
 * Parse_command() only parses a single command.  In general, you will want
 * to use parse_line() to execute things.Parse command recognized no quoted
 * characters or anything (beyond those specific for a given command being
 * executed). 
 */
void
parse_command(u_char *line, int hist_flag, u_char *sub_args)
{
	static	unsigned int	 level = 0;
	u_char	*cmdchars,
		*com,
		*this_cmd = NULL;
	int	args_flag,
		add_to_hist,
		cmdchar_used;

	if (!line || !*line)
		return;
	if (get_int_var(DEBUG_VAR) & DEBUG_COMMANDS)
		yell("Executing [%d] %s", level, line);
	level++;
	if (!(cmdchars = get_string_var(CMDCHARS_VAR)))
		cmdchars = UP(DEFAULT_CMDCHARS);
	malloc_strcpy(&this_cmd, line);
	add_to_hist = 1;
	if (my_index(cmdchars, *line))
	{
		cmdchar_used = 1;
		com = line + 1;
	}
	else
	{
		cmdchar_used = 0;
		com = line;
	}
	/*
	 * always consider input a command unless we are in interactive mode
	 * and command_mode is off.   -lynx
	 */
	if (hist_flag && !cmdchar_used && !get_int_var(COMMAND_MODE_VAR))
	{
		do_send_text(NULL, line, empty_string());
		if (hist_flag && add_to_hist)
		{
			add_to_history(this_cmd);
			set_input(empty_string());
		}
		/* Special handling for ' and : */
	}
	else if (*com == '\'' && get_int_var(COMMAND_MODE_VAR))
	{
		do_send_text(NULL, line+1, empty_string());
		if (hist_flag && add_to_hist)
		{
			add_to_history(this_cmd);
			set_input(empty_string());
		}
	}
	else if (*com == '@')
	{
		/* This kludge fixes a memory leak */
		u_char	*tmp;

		tmp = parse_inline(line + 1, sub_args, &args_flag);
		if (tmp)
			new_free(&tmp);
		if (hist_flag && add_to_hist)
		{
			add_to_history(this_cmd);
			set_input(empty_string());
		}
	}
	else
	{
		unsigned display,
			 old_display_var;
		u_char	*rest,
			*nalias = NULL,
			*alias_name;
		int	cmd_cnt,
			alias_cnt;
		IrcCommand	*command; /* = NULL */

		display = get_display();
		old_display_var = (unsigned) get_int_var(DISPLAY_VAR);
		if ((rest = my_index(com, ' ')) != NULL)
			*(rest++) = (u_char) 0;
		else
			rest = empty_string();
		upper(com);

		/* first, check aliases */
		if (*com && my_index(cmdchars, *com))
		{
			alias_cnt = 0;
			com++;
			if (*com == '^')
			{
				com++;
				set_display_off();
			}
		}
		else
		{
			if (*com == '^')
			{
				com++;
				set_display_off();
			}
			nalias = get_alias(COMMAND_ALIAS, com, &alias_cnt,
				&alias_name);
		}
		if (nalias && (alias_cnt == 0))
		{
			if (hist_flag && add_to_hist)
			{
				add_to_history(this_cmd);
				set_input(empty_string());
			}
			execute_alias(alias_name, nalias, rest);
			new_free(&alias_name);
		}
		else
		{
			/* History */
			if (*com == '!')
			{
				if ((com = do_history(com + 1, rest)) != NULL)
				{
					if (level == 1)
					{
						set_input(com);
						update_input(UPDATE_ALL);
					}
					else
						parse_command(com, 0, sub_args);
					new_free(&com);
				}
				else
					set_input(empty_string());
			}
			else
			{
				if (hist_flag && add_to_hist)
				{
					add_to_history(this_cmd);
					set_input(empty_string());
				}
				command = find_command(com, &cmd_cnt);
				if ((command && cmd_cnt < 0) || (0 == alias_cnt && 1 == cmd_cnt))
				{
					if ((command->flags & SERVERREQ) && connected_to_server() == 0)
						say("%s: You are not connected to a server. Use /SERVER to connect.", com);
					else if ((command->flags & NOICB) && server_get_version(get_from_server()) == ServerICB)
						say("%s: Not available for ICB.", command->name);
					else if (command->func)
						command->func(UP(command->server_func), rest, sub_args);
					else
						say("%s: command disabled", command->name);
				}
				else if (nalias && 1 == alias_cnt && cmd_cnt == 1 && !my_strcmp(alias_name, command[0].name))
					execute_alias(alias_name, nalias, rest);
				else if ((alias_cnt + cmd_cnt) > 1)
					say("Ambiguous command: %s", com);
				else if (nalias && 1 == alias_cnt)
					execute_alias(alias_name, nalias, rest);
				else if (!my_stricmp(com, my_nickname()))
						/* nick = /me  -lynx */
					me(NULL, rest, empty_string());
				else
					say("Unknown command: %s", com);
			}
			if (nalias)
				new_free(&alias_name);
		}
		if (old_display_var != get_int_var(DISPLAY_VAR))
			set_display(get_int_var(DISPLAY_VAR));
		else
			set_display(display);
	}
	new_free(&this_cmd);
	level--;
}

/*
 * load_a_file: helper for load() to load one already-open file.
 */
static void
load_a_file(FILE *fp, u_char *args, int flag)
{
	u_char	*current_row = NULL;
	int	paste_level = 0;
	int	display;
	int	no_semicolon = 1;

	display = set_display_off();
	if (!flag)
		args = NULL;

	for (;;)
	{
		u_char	lbuf[BIG_BUFFER_SIZE];

		/* XXX need better /load policy, but this will do for now */
		if (fgets(CP(lbuf), sizeof(lbuf) / 2, fp))
		{
			size_t	len;
			u_char	*ptr;
			u_char	*start;

			for (start = lbuf; isspace(*start); start++)
				;
			if (!*start || *start == '#')
				continue;

			len = my_strlen(start);
			/*
			 * this line from stargazer to allow \'s in scripts for
			 * continued lines <spz@specklec.mpifr-bonn.mpg.de>
			 */
			while (start[len-1] == '\n' && start[len-2] == '\\' &&
			    len < sizeof(lbuf) / 2 && fgets(CP(&(start[len-2])),
			    (int)(sizeof(lbuf) / 2 - len), fp))
				len = my_strlen(start);

			if (start[len - 1] == '\n')
			    start[--len] = '\0';

			while (start && *start)
			{
				u_char	*optr = start;

				while ((ptr = sindex(optr, UP("{};"))) &&
						ptr != optr &&
						ptr[-1] == '\\')
					optr = ptr+1;

				if (no_semicolon)
					no_semicolon = 0;
				else if ((!ptr || ptr != start) && current_row)
				{
					if (!paste_level)
					{
						Debug(DB_LOAD,
						    "calling parse_line(NULL, "
						    "'%s', '%s', ...)",
						    current_row, args);
						parse_line(NULL, current_row,
							args, 0, 0, 0);
						new_free(&current_row);
					}
					else
						malloc_strcat(&current_row, UP(";"));
				}

				if (ptr)
				{
					u_char	c = *ptr;

					*ptr = '\0';
					malloc_strcat(&current_row, start);
					*ptr = c;

					switch (c)
					{
					case '{' :
						paste_level++;
						if (ptr == start)
							malloc_strcat(&current_row, UP(" {"));
						else
							malloc_strcat(&current_row, UP("{"));
						no_semicolon = 1;
						break;

					case '}' :
						if (!paste_level)
							yell("Unexpected }");
						else
						{
							--paste_level;
							malloc_strcat(&current_row, UP("}"));
							no_semicolon = ptr[1] ? 1 : 0;
						}
						break;

					case ';' :
						malloc_strcat(&current_row, UP(";"));
						no_semicolon = 1;
						break;
					}

					start = ptr + 1;
				}
				else
				{
					malloc_strcat(&current_row, start);
					start = NULL;
				}
			}
		}
		else
			break;
	}
	if (current_row)
	{
		if (paste_level)
			yell("Unexpected EOF");
		else
			parse_line(NULL,
				current_row, 
				args, 0, 0, 0);
		new_free(&current_row);
	}
	set_display(display);
	fclose(fp);
}

/*
 * load: the /LOAD command.  Reads the named file, parsing each line as
 * though it were typed in (passes each line to parse_command). 
 */
void
load(u_char *command, u_char *args, u_char *subargs)
{
	FILE	*fp;
	u_char	*filename,
		*expanded = NULL;
	int	flag = 0;
	struct	stat	stat_buf;
	u_char	*ircpath;
#ifdef ZCAT
	u_char	*expand_z = NULL;
	int	exists;
	int	pos;
#endif /* ZCAT */

	ircpath = get_string_var(LOAD_PATH_VAR);
	if (!ircpath)
	{
		say("LOAD_PATH has not been set");
		return;
	}

	if (load_depth == MAX_LOAD_DEPTH)
	{
		say("No more than %d levels of LOADs allowed", MAX_LOAD_DEPTH);
		return;
	}
	load_depth++;
	status_update(0);
#ifdef DAEMON_UID
	if (getuid() == DAEMON_UID)
	{
		say("You may only load your SAVED file");
		filename = ircrc_file_path();
	}
	else
#endif /* DAEMON_UID */
		while ((filename = next_arg(args, &args)) != NULL)
		{
			if (my_strnicmp(filename, UP("-args"), my_strlen(filename)) == 0)
				flag = 1;
			else
				break;
		}
	if (filename)
	{
		if ((expanded = expand_twiddle(filename)) != NULL)
		{
#ifdef ZCAT
			/* Handle both <expanded> and <expanded>.Z */
			pos = my_strlen(expanded) - my_strlen(ZSUFFIX);
			if (pos < 0 || my_strcmp(expanded + pos, ZSUFFIX))
			{
				malloc_strcpy(&expand_z, expanded);
				malloc_strcat(&expand_z, UP(ZSUFFIX));
			}
#endif /* ZCAT */
			if (*expanded != '/')
			{
				filename = path_search(expanded, ircpath);
#ifdef ZCAT
				if (!filename && expand_z)
					filename = path_search(expand_z, ircpath);
#endif /* ZCAT */
				if (!filename)
				{
					say("%s: File not found", expanded);
					status_update(1);
					load_depth--;
#ifdef ZCAT
					new_free(&expand_z);
#endif /* ZCAT */
					new_free(&expanded);
					return;
				}
				else
					malloc_strcpy(&expanded, filename);
			}
#ifdef ZCAT
			if ((exists = stat(CP(expanded), &stat_buf)) == -1)
			{
				if (!(exists = stat(CP(expand_z), &stat_buf)))
				{
					if (expanded)
						new_free(&expanded);
					expanded = expand_z;
				}
				else
					new_free(&expand_z);
			}
			if (exists == 0)
#else
				if (!stat(expanded, &stat_buf))
#endif /* ZCAT */
				{
					if (stat_buf.st_mode & S_IFDIR)
					{
						say("%s is a directory", expanded);
						status_update(1);
						load_depth--;
#ifdef ZCAT
						new_free(&expand_z);
#endif /* ZCAT */
						new_free(&expanded);
						return;
					}
					if (stat_buf.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH))
					{
						yell("*** %s is executable and may not be loaded", expanded);
						status_update(1);
						load_depth--;
#ifdef ZCAT
						new_free(&expand_z);
#endif /* ZCAT */
						new_free(&expanded);
						return;
					}
				}
			if (command && *command == 'W')
			{
				say("%s", expanded);
				status_update(1);
				load_depth--;
				new_free(&expanded);
#ifdef ZCAT
				new_free(&expand_z);
#endif /* ZCAT */
				return;
			}
#ifdef ZCAT
			/* Open if uncompressed, zcat if compressed */
			pos = my_strlen(expanded) - my_strlen(ZSUFFIX);
			if (pos >= 0 && !my_strcmp(expanded + pos, ZSUFFIX))
				fp = zcat(expanded);
			else
				fp = fopen(CP(expanded), "r");
			if (fp != NULL)
#else
				if (fp = fopen(CP(expanded), "r"))
#endif /* ZCAT */
					load_a_file(fp, args, flag);
				else
					say("Couldn't open %s: %s", expanded,
						strerror(errno));
			new_free(&expanded);
#ifdef ZCAT
			new_free(&expand_z);
#endif /* ZCAT */
		}
		else
			say("Unknown user");
	}
	else
		say("No filename specified");
	status_update(1);
	load_depth--;
}

/*
 * get_history: gets the next history entry, either the PREV entry or the
 * NEXT entry, and sets it to the current input string 
 */
static void	
get_history(int which)
{
	u_char	*ptr;

	if ((ptr = get_from_history(which)) != NULL)
	{
		Debug(DB_HISTORY, "get_history: get_from_history(%d) gave ``%s''", which, ptr);
		set_input_raw(ptr);
		update_input(UPDATE_ALL);
	}
}

/* BIND function: */
void
forward_character(u_int key, u_char *ptr)
{
	input_move_cursor(RIGHT);
}

void
backward_character(u_int key, u_char *ptr)
{
	input_move_cursor(LEFT);
}

void
backward_history(u_int key, u_char *ptr)
{
	get_history(PREV);
}

void
forward_history(u_int key, u_char *ptr)
{
	get_history(NEXT);
}

void
toggle_insert_mode(u_int key, u_char *ptr)
{
	set_var_value(INSERT_MODE_VAR, UP("TOGGLE"));
}

int
prompt_active_count(void)
{
	int count = 0;
	Prompt *promptlist = get_current_promptlist();

	for (; promptlist; promptlist = promptlist->next)
		count++;

	return count;
}

/*
 * what's the current screen's prompt.
 */
u_char	*
prompt_current_prompt(void)
{
	u_char	*prompt;
	Prompt *promptlist = get_current_promptlist();

	if (promptlist)
		prompt = promptlist->prompt;
	else
		prompt = NULL;

	return prompt;
}

/* 
 * add_wait_prompt:  Given a prompt string, a function to call when
 * the prompt is entered.. some other data to pass to the function,
 * and the type of prompt..  either for a line, or a key, we add 
 * this to the prompt_list for the current screen..  and set the
 * input prompt accordingly.
 */
void
add_wait_prompt(u_char *prompt,
		void (*func)(u_char *, u_char *),
		u_char *data,
		int type)
{
	Prompt *new;
	Prompt *promptlist = get_current_promptlist();

	new = new_malloc(sizeof *new);
	new->prompt = NULL;
	malloc_strcpy(&new->prompt, prompt);
	new->data = NULL;
	malloc_strcpy(&new->data, data);
	new->type = type;
	new->func = func;
	new->next = NULL;
	if (promptlist)
	{
		for (; promptlist; promptlist = promptlist->next)
			if (!promptlist->next) {
				promptlist->next = new;
				break;
			}
		// assert(promptlist)
	}
	else
	{
		set_current_promptlist(new);
		change_input_prompt(1);
	}
}

/*
 * Handle the current input for prompt, returns 1 if done so, 0 otherwise.
 */
static int
prompt_check_input(int type, u_char *line, u_char key)
{
	Prompt *promptlist = get_current_promptlist();

	if (promptlist && promptlist->type == type)
	{
		Prompt	*old;

		old = promptlist;
		if (type == WAIT_PROMPT_KEY)
		{
			u_char	buf[2];

			buf[0] = key;
			buf[1] = '\0';
			(*old->func)(old->data, buf);
		}
		else
			(*old->func)(old->data, line);
		set_input(empty_string());
		set_current_promptlist(old->next);
		new_free(&old->data);
		new_free(&old->prompt);
		new_free(&old);
		change_input_prompt(-1);
		return 1;
	}
	return 0;

}

void
send_line(u_int key, u_char *ptr)
{
	u_char	*line;
	int	server;

	server = set_from_server(get_window_server(0));
	reset_hold(NULL);
	window_hold_mode(NULL, OFF, 1);
	line = get_input();
	if (!prompt_check_input(WAIT_PROMPT_LINE, line, 0))
	{
		u_char	*tmp = NULL;

		malloc_strcpy(&tmp, line);

		if (do_hook(INPUT_LIST, "%s", tmp))
		{
			if (get_int_var(INPUT_ALIASES_VAR))
				parse_line(NULL, tmp, empty_string(),
					1, 0, 0);
			else
				parse_line(NULL, tmp, NULL,
					1, 0, 0);
		}
		update_input(UPDATE_ALL);
		new_free(&tmp);
	}
	set_from_server(server);
}

/* The SENDLINE command.. */
static	void
sendlinecmd(u_char *command, u_char *args, u_char *subargs)
{
	unsigned display;

	display = set_display_on();
	if (get_int_var(INPUT_ALIASES_VAR))
		parse_line(NULL, args, empty_string(), 1, 0, 0);
	else
		parse_line(NULL, args, NULL, 1, 0, 0);
	update_input(UPDATE_ALL);
	set_display(display);
}

void
quote_char(u_int key, u_char *ptr)
{
	get_edit_info()->meta_hit[0] = 1;
}

void
meta1_char(u_int key, u_char *ptr)
{
	get_edit_info()->meta_hit[1] = 1;
}

void
meta2_char(u_int key, u_char *ptr)
{
	get_edit_info()->meta_hit[2] = 1;
}

void
meta3_char(u_int key, u_char *ptr)
{
	get_edit_info()->meta_hit[3] = 1;
}

void
meta4_char(u_int key, u_char *ptr)
{
	get_edit_info()->meta_hit[4] = 1 - get_edit_info()->meta_hit[4];
}

void
meta5_char(u_int key, u_char *ptr)
{
	get_edit_info()->meta_hit[5] = 1;
}

void
meta6_char(u_int key, u_char *ptr)
{
	get_edit_info()->meta_hit[6] = 1;
}

void
meta7_char(u_int key, u_char *ptr)
{
	get_edit_info()->meta_hit[7] = 1;
}

void
meta8_char(u_int key, u_char *ptr)
{
	get_edit_info()->meta_hit[8] = 1;
}

/* type_text: the BIND function TYPE_TEXT */
void
type_text(u_int key, u_char *ptr)
{
	for (; *ptr; ptr++)
		input_add_character((u_int)*ptr, NULL);
}

/*
 * irc_clear_screen: the CLEAR_SCREEN function for BIND.  Clears the screen and
 * starts it if it is held 
 */
void
irc_clear_screen(u_int key, u_char *ptr)
{
	window_hold_mode(NULL, OFF, 1);
	my_clear(NULL, empty_string(), empty_string());
}

/* parse_text: the bindable function that executes its string */
void
parse_text(u_int key, u_char *ptr)
{
	parse_line(NULL, ptr, empty_string(), 0, 0, 0);
}

/*
 * edit_char: handles each character for an input stream.  Not too difficult
 * to work out.
 */
void
edit_char(u_int ikey)
{
	KeyMap_Func func;
	u_char	*str;
	u_char	extended_key, key = (u_char)ikey;
	EditInfo *edit_info;
	int	meta_key_index = -1;

	if (prompt_check_input(WAIT_PROMPT_KEY, NULL, key))
		return;

	if (!get_int_var(EIGHT_BIT_CHARACTERS_VAR))
		key &= 0x7F;			/* strip to 7-bitness */

	extended_key = key;

	edit_info = get_edit_info();

	if (edit_info->meta_hit[1])
		meta_key_index = 1;
	else if (edit_info->meta_hit[2])
		meta_key_index = 2;
	else if (edit_info->meta_hit[3])
		meta_key_index = 3;
	else if (edit_info->meta_hit[4])
		meta_key_index = 4;
	else if (edit_info->meta_hit[5])
		meta_key_index = 5;
	else if (edit_info->meta_hit[6])
		meta_key_index = 6;
	else if (edit_info->meta_hit[7])
		meta_key_index = 7;
	else if (edit_info->meta_hit[8])
		meta_key_index = 8;
	else
		meta_key_index = 0;

	func = keys_get_func_from_index_and_meta(key, meta_key_index);
	str = keys_get_str_from_key_and_meta(key, meta_key_index);
	if (meta_key_index > 0 && meta_key_index != 4)
		edit_info->meta_hit[meta_key_index] = 0;

	if (!edit_info->meta_hit[1] && !edit_info->meta_hit[2] &&
	    !edit_info->meta_hit[3] && !edit_info->meta_hit[5] &&
	    !edit_info->meta_hit[6] && !edit_info->meta_hit[7] &&
	    !edit_info->meta_hit[8])
	{
		if (edit_info->inside_menu == 1)
			menu_key((u_int)key);
		else if (edit_info->digraph_hit)
		{
			if (extended_key == 0x08 || extended_key == 0x7f)
				edit_info->digraph_hit = 0;
			else if (edit_info->digraph_hit == 1)
			{
				edit_info->digraph_first = extended_key;
				edit_info->digraph_hit = 2;
			}
			else if (edit_info->digraph_hit == 2)
			{
				if ((extended_key =
				    get_digraph((u_int)extended_key)) != '\0')
					input_add_character((u_int)extended_key,
					    NULL);
				else
					term_beep();
			}
		}
		else if (edit_info->meta_hit[0])
		{
			edit_info->meta_hit[0] = 0;
			input_add_character((u_int)extended_key, NULL);
		}
		else if (func)
			func(extended_key, str ? str : empty_string());
	}
	else
		term_beep();
}

static	void
catter(u_char *command, u_char *args, u_char *subargs)
{
	u_char *target = next_arg(args, &args);

	if (target && args && *args)
	{
		u_char *text = args;
		FILE *fp = fopen(CP(target), "r+");

		if (!fp)
		{
			fp = fopen(CP(target), "w");
			if (!fp)
			{
				say("CAT: error: '%s': %s", target, strerror(errno));
				return;
		}	}
		
		fseek(fp, 0, SEEK_END);
		fprintf(fp, "%s\n", text),
		fclose(fp);
	}
	else
		say("Usage: /CAT <destfile> <line>");
}


static	void
cd(u_char *command, u_char *args, u_char *subargs)
{
	u_char	lbuf[BIG_BUFFER_SIZE];
	u_char	*arg,
		*expand;

#ifdef DAEMON_UID
	if (getuid() == DAEMON_UID)
	{
		say("You are not permitted to use this command");
		return;
	}
#endif /* DAEMON_UID */
	if ((arg = next_arg(args, &args)) != NULL)
	{
		if ((expand = expand_twiddle(arg)) != NULL)
		{
			if (chdir(CP(expand)))
				say("CD: %s", strerror(errno));
			new_free(&expand);
		}
		else
			say("CD: No such user");
	}
	if (getcwd(CP(lbuf), sizeof(lbuf)) == NULL)
	{
		lbuf[0] = '.';
		lbuf[1] = '\0';
	}
	say("Current directory: %s", lbuf);
}

static	void
send_action(u_char *target, u_char *text)
{
	if (server_get_version(get_from_server()) == ServerICB)
		icb_put_action(target, text);
	else
		send_ctcp(ctcp_type(CTCP_PRIVMSG), target, UP("ACTION"), "%s", text);
}

static	void
describe(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*target;

	target = next_arg(args, &args);
	if (target && args && *args)
	{
		int	old;

		save_message_from();
		old = set_lastlog_msg_level(LOG_ACTION);
		message_from_level(LOG_ACTION);
		send_action(target, args);
		if (do_hook(SEND_ACTION_LIST, "%s %s", target, args))
			put_it("* -> %s: %s %s", target,
				server_get_nickname(get_from_server()), args);
		set_lastlog_msg_level(old);
		restore_message_from();
	}
	else
		say("Usage: /DESCRIBE <target> <action description>");
}

/*
 * New 'me' command - now automatically appends period.
 * Necessary for new 'action' script.   -lynx'92
 * Hardly, removed this as it was generally considered offensive
 */
static	void
me(u_char *command, u_char *args, u_char *subargs)
{
	if (args && *args)
	{
		u_char	*target;

		if ((target = get_target_by_refnum(0)) != NULL)
		{
			int	old;

			/* handle "/ foo" */
			if (!my_strncmp(target, get_string_var(CMDCHARS_VAR), 1) &&
			    (!(target = get_channel_by_refnum(0))))
			{
				say("No target, neither channel nor query");
				return;
			}

			old = set_lastlog_msg_level(LOG_ACTION);
			save_message_from();
			message_from(target, LOG_ACTION);
			send_action(target, args);
			if (do_hook(SEND_ACTION_LIST, "%s %s", target, args))
				put_it("* %s %s",
				    server_get_nickname(get_from_server()), args);
			set_lastlog_msg_level(old);
			restore_message_from();
		}
		else
			say("No target, neither channel nor query");
	}
	else
		say("Usage: /ME <action description>");
}

static	void
mload(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*file;

	while ((file = next_arg(args, &args)) != NULL)
		load_menu(file);
}

static	void
mlist(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*menu;

	while ((menu = new_next_arg(args, &args)) != NULL)
		(void) ShowMenu(menu);
}

static	void
evalcmd(u_char *command, u_char *args, u_char *subargs)
{
	parse_line(NULL, args, subargs ? subargs : empty_string(), 0, 0, 0);
}

/*
 * execute_timer:  checks to see if any currently pending timers have
 * gone off, and if so, execute them, delete them, etc, setting the
 * current_exec_timer, so that we can't remove the timer while its
 * still executing.
 */
void
execute_timer(void)
{
	struct timeval current;
	TimerList *next;
	
	gettimeofday(&current, NULL);

	while (PendingTimers &&
	          (PendingTimers->time < current.tv_sec
	        || (PendingTimers->time == current.tv_sec
	        &&  PendingTimers->microseconds <= current.tv_usec)))
	{
		int	old_in_on_who, old_server;

		old_in_on_who = set_in_on_who(PendingTimers->in_on_who);
		current_exec_timer = PendingTimers->ref;
		save_message_from();
		message_from(NULL, LOG_CRAP);
		old_server = set_from_server(PendingTimers->server);
		parse_command(PendingTimers->command, 0, empty_string());
		set_from_server(old_server);
		restore_message_from();
		current_exec_timer = -1;
		new_free(&PendingTimers->command);
		next = PendingTimers->next;
		new_free(&PendingTimers);
		PendingTimers = next;
		set_in_on_who(old_in_on_who);
	}
}

/*
 * timercmd: the bit that handles the TIMER command.  If there are no
 * arguements, then just list the currently pending timers, if we are
 * give a -DELETE flag, attempt to delete the timer from the list.  Else
 * consider it to be a timer to add, and add it.
 */
static	void
timercmd(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*waittime, *flag;
	struct	timeval timertime;
	long	waitsec, waitusec;
	TimerList **slot,
		  *ntimer;
	int	want = -1,
		refnum;

	if (*args == '-')
	{
		size_t	len;

		flag = next_arg(args, &args);
		len = my_strlen(flag);
		upper(flag);

		/* first check for the -DELETE flag */

		if (!my_strncmp(flag, "-DELETE", len))
		{
			u_char	*ptr;
			int	ref;
			TimerList	*tmp,
					*prev;

			if (current_exec_timer != -1)
			{
				say("You may not remove a TIMER from itself");
				return;
			}
			if (!(ptr = next_arg(args, &args)))
			{
				say("%s: Need a timer reference number for -DELETE", command);
				return;
			}
			ref = my_atoi(ptr);
			for (prev = tmp = PendingTimers; tmp; prev = tmp,
					tmp = tmp->next)
			{
				if (tmp->ref == ref)
				{
					if (tmp == prev)
						PendingTimers =
							PendingTimers->next;
					else
						prev->next = tmp->next;
					new_free(&tmp->command);
					new_free(&tmp);
					return;
				}
			}
			say("%s: Can't delete %d, no such refnum",
				command, ref);
			return;
		}
		else if (!my_strncmp(flag, "-REFNUM", len))
		{
			u_char	*ptr;

			ptr = next_arg(args, &args);
			want = my_atoi(ptr);
			if (want < 0)
			{
				say("%s: Illegal refnum %d", command, want);
				return;
			}
		}
		else
		{
			say("%s: %s no such flag", command, flag);
			return;
		}
	}

	/* else check to see if we have no args -> list */

	if (!(waittime = next_arg(args, &args)))
	{
		show_timer(command);
		return;
	}

	/* must be something to add */

	if ((refnum = create_timer_ref(want)) == -1)
	{
		say("%s: Refnum %d already exists", command, want);
		return;
	}
	
	waitusec = waitsec = 0;
	while (isdigit(*waittime))
		waitsec = waitsec*10 + ((*waittime++) - '0');
	if (*waittime == '.')
	{
		long decimalmul = 100000;
		for(; isdigit(*++waittime); decimalmul /= 10)
			waitusec += (*waittime - '0') * decimalmul;
	}
	
	gettimeofday(&timertime, NULL);	
	timertime.tv_sec += waitsec;
	timertime.tv_usec+= waitusec;
	timertime.tv_sec += (timertime.tv_usec/1000000);
	timertime.tv_usec %= 1000000;
	
	ntimer = new_malloc(sizeof *ntimer);
	ntimer->in_on_who = in_on_who();
	ntimer->time = timertime.tv_sec;
	ntimer->microseconds = timertime.tv_usec;
	ntimer->server = get_from_server();
	ntimer->ref = refnum;
	ntimer->command = NULL;
	malloc_strcpy(&ntimer->command, args);

	/* we've created it, now put it in order */

	for (slot = &PendingTimers; *slot; slot = &(*slot)->next)
	{
		if ((*slot)->time > ntimer->time ||
		    ((*slot)->time == ntimer->time &&
		     (*slot)->microseconds > ntimer->microseconds))
			break;
	}
	ntimer->next = *slot;
	*slot = ntimer;
}

/*
 * show_timer:  Display a list of all the TIMER commands that are
 * pending to be executed.
 */
static	void
show_timer(u_char *command)
{
	u_char  lbuf[BIG_BUFFER_SIZE];
	TimerList *tmp;
	struct timeval current, time_left;

	if (!PendingTimers)
	{
		say("%s: No commands pending to be executed", command);
		return;
	}

	gettimeofday(&current, NULL);
	say("Timer Seconds      Command");
	for (tmp = PendingTimers; tmp; tmp = tmp->next)
	{
		time_left.tv_sec = tmp->time;
		time_left.tv_usec = tmp->microseconds;
		time_left.tv_sec -= current.tv_sec;

		if (time_left.tv_usec >= current.tv_usec)
			time_left.tv_usec -= current.tv_usec;
		else
		{
			time_left.tv_usec = time_left.tv_usec -
			    current.tv_usec + 1000000;
			time_left.tv_sec--;
		}

		snprintf(CP(lbuf), sizeof(lbuf), "%ld.%06d",
		    (long)time_left.tv_sec, (int)time_left.tv_usec);
		say("%-5d %-12s %s", tmp->ref, lbuf, tmp->command);
	}
}

/*
 * create_timer_ref:  returns the lowest unused reference number for
 * a timer
 */
static	int
create_timer_ref(int want)
{
	TimerList	*tmp;
	int	ref = 0;
	int	done = 0;

	if (want == -1)
		while (!done)
		{
			done = 1;
			for (tmp = PendingTimers; tmp; tmp = tmp->next)
				if (ref == tmp->ref)
				{
					ref++;
					done = 0;
					break;
				}
		}
	else
	{
		ref = want;
		for (tmp = PendingTimers; tmp; tmp = tmp->next)
			if (ref == tmp->ref)
			{
				ref = -1;
				break;
			}
	}

	return (ref);
}

/*
 * timer_timeout:  Called from irc_io to help create the timeout
 * part of the call to select.
 */
void
timer_timeout(struct timeval *tv)
{
	struct timeval current;

	tv->tv_usec =0;
	tv->tv_sec  =0;
	
	if (!PendingTimers)
	{
		tv->tv_sec = 70; /* Just larger than the maximum of 60 */
		return;
	}
	gettimeofday(&current, NULL);
	
	if (PendingTimers->time < current.tv_sec ||
	    (PendingTimers->time == current.tv_sec &&
	    PendingTimers->microseconds < current.tv_usec))
	{
		/* No time to lose, the event is now or was */
		return;
	}
	
	tv->tv_sec = PendingTimers->time - current.tv_sec;
	if (PendingTimers->microseconds >= current.tv_usec)
		tv->tv_usec = PendingTimers->microseconds - current.tv_usec;
	else
	{
		tv->tv_usec = current.tv_usec - PendingTimers->microseconds;
		tv->tv_sec -= 1;
	}
}

/*
 * inputcmd:  the INPUT command.   Takes a couple of arguements...
 * the first surrounded in double quotes, and the rest makes up
 * a normal ircII command.  The command is evalutated, with $*
 * being the line that you input.  Used add_wait_prompt() to prompt
 * the user...  -phone, jan 1993.
 */

static	void
inputcmd(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*prompt;

	if (!args || !*args)
		return;
	
	if (*args++ != '"')
	{
		say("Need \" to begin prompt for INPUT");
		return;
	}

	prompt = args;
	if ((args = my_index(prompt, '"')) != NULL)
		*args++ = '\0';
	else
	{
		say("Missing \" in INPUT");
		return;
	}

	for (; *args == ' '; args++)
		;

	add_wait_prompt(prompt, eval_inputlist, args, WAIT_PROMPT_LINE);
}

/*
 * eval_inputlist:  Cute little wrapper that calls parse_line() when we
 * get an input prompt ..
 */

void
eval_inputlist(u_char *args, u_char *line)
{
	parse_line(NULL, args, line ? line : empty_string(), 0, 0, 0);
}

/* pingcmd: ctcp ping, duh - phone, jan 1993. */
static	void
pingcmd(u_char *command, u_char *args, u_char *subargs)
{
	u_char	buffer[BIG_BUFFER_SIZE];

	snprintf(CP(buffer), sizeof buffer, "%s PING %ld", args, (long)time(NULL));
	ctcp(command, buffer, empty_string());
}

static	void
xtypecmd(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*arg;
	size_t	len;

	if (args && *args == '-')
	{
		args++;
		if ((arg = next_arg(args, &args)) != NULL)
		{
			len = my_strlen(arg);
			if (!my_strnicmp(arg, UP("LITERAL"), len))
			{
				for (; *args; args++)
					input_add_character((u_int)*args, NULL);
			}
			else
				say ("Unknown flag -%s to XTYPE", arg);
			return;
		}
		input_add_character('-', NULL);
	}
	else
		typecmd(command, args, empty_string());
	return;
}

static	void
beepcmd(u_char *command, u_char *args, u_char *subargs)
{
	term_beep();
}

#ifdef HAVE_SETENV
static  void   
irc_setenv(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*arg;

	if (args && *args && (arg = next_arg(args, &args)) != NULL)
	{
/* something broken in here */
#undef SPECIAL_TZ_HACK
#ifdef SPECIAL_TZ_HACK
		/*
		 * special hack for $TZ to try to stop memory leaks..
		 *
		 * special thanks (heh) to simonb for this idea...
		 */
		int is_tz;

		is_tz = arg[0] == 'T' && arg[1] == 'Z' && arg[2] == '\0';
		if (is_tz)
		{
			u_char *otz = UP(getenv("TZ"));
			static size_t tzlen = -1;
			size_t ntzlen = my_strlen(args);

			if (otz)
			{
				size_t otzlen = my_strlen(otz);

				if (tzlen == -1)
					tzlen = otzlen;
				if (otzlen > tzlen)
					say("--- weird, irc_setenv, otzlen > tzlen");

				if (ntzlen > tzlen)
					tzlen = -1;
				else
				{
					my_strncpy(otz, args, otzlen - 1);
					otz[otzlen - 1] = 0;
				}
			}
			if (tzlen == -1)
			{
				setenv(CP(arg), CP(args), 1);
				tzlen = ntzlen;
			}
		}
		else
#endif /* SPECIAL_TZ_HACK */
			setenv(CP(arg), CP(args), 1);
#ifdef HAVE_TZSET
# ifdef SPECIAL_TZ_HACK
		if (is_tz)
# endif /* SPECIAL_TZ_HACK */
			tzset();
#endif /* HAVE_TZSET */
	}
}
#endif

#ifdef HAVE_UNSETENV
static  void   
irc_unsetenv(u_char *command, u_char *args, u_char *subargs)
{
	if (args && *args)
	{
		unsetenv(CP(args));
#ifdef HAVE_TZSET
		if (args[0] == 'T' && args[1] == 'Z' && args[0] == '\0')
			tzset();
#endif /* HAVE_TZSET */
	}
}
#endif
  
/*
 * e_nuser: the /NUSER command.  Added by Hendrix, ircII 20110228
 * implementation by zDm
 *   Parameters as follows:
 *     /NUSER <new_username> [new_IRCNAME]
 *       <new_username> is a new username to use and is required
 *       [new_IRCNAME] is a new IRCNAME string to use and is optional
 *   This will disconnect you from your server and reconnect using
 *     the new information given.  You will rejoin all channel you
 *     are currently on and keep your current nickname.
*/
static	void
e_nuser(u_char *command, u_char *args, u_char *subargs)
{
	u_char    *newuname;
	const	int	from_server = get_from_server();

	newuname = next_arg(args, &args);
	if (newuname)
	{
		set_username(newuname);
		set_realname(args);
		say("Reconnecting to server...");
		close_server(from_server, empty_string());
		if (connect_to_server(server_get_name(from_server),
				      server_get_port(from_server),
				      server_get_nickname(from_server),
				      get_primary_server()) != -1)
		{
			change_server_channels(get_primary_server(), from_server);
			window_set_server(-1, from_server, 1);
		}
		else
			say("Unable to reconnect. Use /SERVER to conenct.");
	}
	else
		say("You must specify a username and, optionally, an IRCNAME");
} 

EditInfo *
alloc_edit_info(void)
{
	EditInfo *new;
	int i;

	new = new_malloc(sizeof *new);

	for (i = 0; i < ARRAY_SIZE(new->meta_hit); i++)
		new->meta_hit[i] = 0;
	new->digraph_hit = 0;
	new->inside_menu = 0;
	new->digraph_first = '\0';

	return new;
}

WhoInfo *
alloc_who_info(void)
{
	WhoInfo *new;

	new = new_malloc(sizeof *new);

	new->who_mask = 0;
	new->who_name = NULL;
	new->who_host = NULL;
	new->who_server = NULL;
	new->who_file = NULL;
	new->who_nick = NULL;
	new->who_real = NULL;

	return new;
}

int
get_digraph_hit(void)
{
	return get_edit_info()->digraph_hit;
}

void
set_digraph_hit(int hit)
{
	get_edit_info()->digraph_hit = hit;
}

u_char
get_digraph_first(void)
{
	return get_edit_info()->digraph_first;
}

void
set_digraph_first(u_char first)
{
	get_edit_info()->digraph_first = first;
}

int
get_inside_menu(void)
{
	return get_edit_info()->inside_menu;
}

void
set_inside_menu(int inside_menu)
{
	get_edit_info()->inside_menu = inside_menu;
}

u_char *
get_sent_nick(void)
{
	return sent_nick;
}

u_char *
get_sent_body(void)
{
	return sent_body;
}

u_char *
get_recv_nick(void)
{
	return recv_nick;
}

void
set_recv_nick(u_char *nick)
{
	malloc_strcpy(&recv_nick, nick);
}

int
who_level_before_who_from(void)
{
	return my_echo_set_message_from;
}

int
get_load_depth(void)
{
	return load_depth;
}

void
load_global(void)
{
	doing_loading_global = 1;
	load(empty_string(), UP("global"), empty_string());
	doing_loading_global = 0;
}

int
loading_global(void)
{
	return doing_loading_global;
}

int
is_away_set(void)
{
	return away_set;
}

u_char *
get_invite_channel(void)
{
	return invite_channel;
}

void
set_invite_channel(u_char *nick)
{
	malloc_strcpy(&invite_channel, nick);
}
