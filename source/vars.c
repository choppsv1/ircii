/*
 * vars.c: All the dealing of the irc variables are handled here. 
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
IRCII_RCSID("@(#)$eterna: vars.c,v 1.104 2014/07/11 11:36:04 mrg Exp $");

#include "status.h"
#include "window.h"
#include "lastlog.h"
#include "log.h"
#include "irccrypt.h"
#include "history.h"
#include "notify.h"
#include "vars.h"
#include "input.h"
#include "ircaux.h"
#include "whois.h"
#include "ircterm.h"
#include "translat.h"
#include "output.h"
#include "server.h"
#include "dcc.h"
#include "ssl.h"

#define	VF_NODAEMON	0x0001
#define VF_EXPAND_PATH	0x0002

#define VIF_CHANGED	0x01
#define VIF_GLOBAL	0x02

/* the types of IrcVariables */
#define BOOL_TYPE_VAR 0
#define CHAR_TYPE_VAR 1
#define INT_TYPE_VAR 2
#define STR_TYPE_VAR 3

static	void	check_variable_order(void);
static	int	find_variable(u_char *, int *);
static	void	exec_warning(int);
static	void	input_warning(int);
static	void	eight_bit_characters(int);
static	void	v_update_all_status(int);

/* IrcVariable: structure for each variable in the variable table */
typedef struct
{
	char	*name;			/* what the user types */
	int	type;			/* variable types, see below */
	int	integer;		/* int value of variable */
	u_char	*string;		/* string value of variable */
					/* function to call every time variable is set */
	void	(*ifunc)(int);
	void	(*sfunc)(u_char *);
	unsigned short	int_flags;	/* internal flags to the variable */
	unsigned short	flags;		/* flags for this variable */
}	IrcVariable;

/*
 * irc_variable: all the irc variables used.  Note that the integer and
 * boolean defaults are set here, which the string default value are set in
 * the init_variables() procedure.
 *
 * this structure needs to be in alphabetical order, and the
 * check_variable_order() procedure will assert that it is from
 * init_variables()
 */
static	IrcVariable irc_variable[] =
{
	{ "ALWAYS_SPLIT_BIGGEST",	BOOL_TYPE_VAR,	DEFAULT_ALWAYS_SPLIT_BIGGEST,		NULL, 0, NULL,				0, 0 },
	{ "AUTO_UNMARK_AWAY",		BOOL_TYPE_VAR,	DEFAULT_AUTO_UNMARK_AWAY,		NULL, 0, NULL,				0, 0 },
	{ "AUTO_WHOWAS",		BOOL_TYPE_VAR,	DEFAULT_AUTO_WHOWAS,			NULL, 0, NULL,				0, 0 },
	{ "BACKGROUND_COLOUR",		INT_TYPE_VAR,	DEFAULT_BACKGROUND_COLOUR,		NULL, 0, NULL,				0, 0 },
	{ "BEEP",			BOOL_TYPE_VAR,	DEFAULT_BEEP,				NULL, 0, NULL,				0, 0 },
	{ "BEEP_MAX",			INT_TYPE_VAR,	DEFAULT_BEEP_MAX,			NULL, 0, NULL,				0, 0 },
	{ "BEEP_ON_MSG",		STR_TYPE_VAR,	0,					NULL, 0, set_beep_on_msg,		0, 0 },
	{ "BEEP_WHEN_AWAY",		INT_TYPE_VAR,	DEFAULT_BEEP_WHEN_AWAY,			NULL, 0, NULL,				0, 0 },
	{ "BIND_LOCAL_DCCHOST",		BOOL_TYPE_VAR,	DEFAULT_BIND_LOCAL_DCCHOST,		NULL, 0, NULL,				0, 0 },
	{ "BOLD_VIDEO",			BOOL_TYPE_VAR,	DEFAULT_BOLD_VIDEO,			NULL, 0, NULL,				0, 0 },
	{ "CHANNEL_NAME_WIDTH",		INT_TYPE_VAR,	DEFAULT_CHANNEL_NAME_WIDTH,		NULL, v_update_all_status, 0,		0, 0 },
	{ "CLIENT_INFORMATION",		STR_TYPE_VAR,	0,					NULL, 0, NULL,				0, 0 },
	{ "CLOCK",			BOOL_TYPE_VAR,	DEFAULT_CLOCK,				NULL, v_update_all_status, 0,		0, 0 },
	{ "CLOCK_24HOUR",		BOOL_TYPE_VAR,	DEFAULT_CLOCK_24HOUR,			NULL, reset_clock, 0,			0, 0 },
	{ "CLOCK_ALARM",		STR_TYPE_VAR,	0,					NULL, 0, set_alarm,			0, 0 },
	{ "CMDCHARS",			STR_TYPE_VAR,	0,					NULL, 0, NULL,				0, 0 },
	{ "COLOUR",			BOOL_TYPE_VAR,	DEFAULT_COLOUR,				NULL, 0, NULL,				0, 0 },
	{ "COMMAND_MODE",		BOOL_TYPE_VAR,	DEFAULT_COMMAND_MODE,			NULL, 0, NULL,				0, 0 },
	{ "CONTINUED_LINE",		STR_TYPE_VAR,	0,					NULL, 0, set_continued_line,		0, 0 },
	{ "CTCP_REPLY_BACKLOG_SECONDS",	INT_TYPE_VAR,	DEFAULT_CTCP_REPLY_BACKLOG_SECONDS,	NULL, ctcp_reply_backlog_change, 0,	0, 0 },
	{ "CTCP_REPLY_FLOOD_SIZE",	INT_TYPE_VAR,	DEFAULT_CTCP_REPLY_FLOOD_SIZE_VAR,	NULL, 0, NULL,				0, 0 },
	{ "CTCP_REPLY_IGNORE_SECONDS",	INT_TYPE_VAR,	DEFAULT_CTCP_REPLY_IGNORE_SECONDS,	NULL, 0, NULL,				0, 0 },
	{ "DCCHOST",			STR_TYPE_VAR,	0,					NULL, 0, set_dcchost,			0, 0 },
	{ "DCCPORT",			INT_TYPE_VAR,	DEFAULT_DCCPORT,			NULL, 0, NULL,				0, 0 },
	{ "DCC_BLOCK_SIZE",		INT_TYPE_VAR,	DEFAULT_DCC_BLOCK_SIZE,			NULL, 0, NULL,				0, 0 },
	{ "DEBUG",			INT_TYPE_VAR,	0,					NULL, 0, NULL,				0, 0 },
	{ "DECRYPT_PROGRAM",		STR_TYPE_VAR,	0,					NULL, 0, NULL,				0, VF_NODAEMON },
	{ "DISPLAY",			BOOL_TYPE_VAR,	DEFAULT_DISPLAY,			NULL, 0, NULL,				0, 0 },
	{ "DISPLAY_ENCODING",		STR_TYPE_VAR,	0,					NULL, 0, set_display_encoding,			0, 0 },
	{ "EIGHT_BIT_CHARACTERS",	BOOL_TYPE_VAR,	DEFAULT_EIGHT_BIT_CHARACTERS,		NULL, eight_bit_characters, 0,		0, 0 },
	{ "ENCRYPT_PROGRAM",		STR_TYPE_VAR,	0,					NULL, 0, NULL,				0, VF_NODAEMON },
	{ "EXEC_PROTECTION",		BOOL_TYPE_VAR,	DEFAULT_EXEC_PROTECTION,		NULL, exec_warning, 0,			0, VF_NODAEMON },
	{ "FLOOD_AFTER",		INT_TYPE_VAR,	DEFAULT_FLOOD_AFTER,			NULL, 0, NULL,				0, 0 },
	{ "FLOOD_RATE",			INT_TYPE_VAR,	DEFAULT_FLOOD_RATE,			NULL, 0, NULL,				0, 0 },
	{ "FLOOD_USERS",		INT_TYPE_VAR,	DEFAULT_FLOOD_USERS,			NULL, 0, NULL,				0, 0 },
	{ "FLOOD_WARNING",		BOOL_TYPE_VAR,	DEFAULT_FLOOD_WARNING,			NULL, 0, NULL,				0, 0 },
	{ "FOREGROUND_COLOUR",		INT_TYPE_VAR,	DEFAULT_FOREGROUND_COLOUR,		NULL, 0, NULL,				0, 0 },
	{ "FULL_STATUS_LINE",		BOOL_TYPE_VAR,	DEFAULT_FULL_STATUS_LINE,		NULL, v_update_all_status, 0,		0, 0 },
	{ "HELP_PAGER",			BOOL_TYPE_VAR,	DEFAULT_HELP_PAGER,			NULL, 0, NULL,				0, 0 },
	{ "HELP_PATH",			STR_TYPE_VAR,	0,					NULL, 0, NULL,				0, VF_EXPAND_PATH|VF_NODAEMON },
	{ "HELP_PROMPT",		BOOL_TYPE_VAR,	DEFAULT_HELP_PROMPT,			NULL, 0, NULL,				0, 0 },
	{ "HELP_WINDOW",		BOOL_TYPE_VAR,	DEFAULT_HELP_WINDOW,			NULL, 0, NULL,				0, 0 },
	{ "HIDE_CHANNEL_KEYS",		BOOL_TYPE_VAR,	DEFAULT_HIDE_CHANNEL_KEYS,		NULL, v_update_all_status, 0,		0, 0 },
	{ "HIDE_PRIVATE_CHANNELS",	BOOL_TYPE_VAR,	DEFAULT_HIDE_PRIVATE_CHANNELS,		NULL, v_update_all_status, 0,		0, 0 },
	{ "HIGHLIGHT_CHAR",		STR_TYPE_VAR,	0,					NULL, 0, set_highlight_char,		0, 0 },
	{ "HISTORY",			INT_TYPE_VAR,	DEFAULT_HISTORY,			NULL, set_history_size, 0,		0, VF_NODAEMON },
	{ "HISTORY_FILE",		STR_TYPE_VAR,	0,					NULL, 0, set_history_file,		0, 0 },
	{ "HOLD_MODE",			BOOL_TYPE_VAR,	DEFAULT_HOLD_MODE,			NULL, reset_line_cnt, 0,		0, 0 },
	{ "HOLD_MODE_MAX",		INT_TYPE_VAR,	DEFAULT_HOLD_MODE_MAX,			NULL, 0, NULL,				0, 0 },
	{ "INDENT",			BOOL_TYPE_VAR,	DEFAULT_INDENT,				NULL, 0, NULL,				0, 0 },
	{ "INPUT_ALIASES",		BOOL_TYPE_VAR,	DEFAULT_INPUT_ALIASES,			NULL, 0, NULL,				0, 0 },
	{ "INPUT_ENCODING",		STR_TYPE_VAR,	0,					NULL, 0, set_input_encoding,			0, 0 },
	{ "INPUT_PROMPT",		STR_TYPE_VAR,	0,					NULL, 0, set_input_prompt,		0, 0 },
	{ "INPUT_PROTECTION",		BOOL_TYPE_VAR,	DEFAULT_INPUT_PROTECTION,		NULL, input_warning, 0,			0, 0 },
	{ "INSERT_MODE",		BOOL_TYPE_VAR,	DEFAULT_INSERT_MODE,			NULL, v_update_all_status, 0,		0, 0 },
	{ "INVERSE_VIDEO",		BOOL_TYPE_VAR,	DEFAULT_INVERSE_VIDEO,			NULL, 0, NULL,				0, 0 },
	{ "IRCHOST",			STR_TYPE_VAR,	0,					NULL, 0, set_irchost,			0, 0 },
	{ "IRC_ENCODING",		STR_TYPE_VAR,	0,					NULL, 0, set_irc_encoding,			0, 0 },
	{ "LASTLOG",			INT_TYPE_VAR,	DEFAULT_LASTLOG,			NULL, set_lastlog_size, 0,		0, 0 },
	{ "LASTLOG_LEVEL",		STR_TYPE_VAR,	0,					NULL, 0, set_lastlog_level,		0, 0 },
	{ "LOAD_PATH",			STR_TYPE_VAR,	0,					NULL, 0, NULL,				0, VF_NODAEMON },
	{ "LOG",			BOOL_TYPE_VAR,	DEFAULT_LOG,				NULL, logger, 0,			0, 0 },
	{ "LOGFILE",			STR_TYPE_VAR,	0,					NULL, 0, set_log_file,			0, VF_NODAEMON },
	{ "MAIL",			INT_TYPE_VAR,	DEFAULT_MAIL,				NULL, v_update_all_status, 0,		0, VF_NODAEMON },
	{ "MAKE_NOTICE_MSG",		BOOL_TYPE_VAR,	DEFAULT_MAKE_NOTICE_MSG,		NULL, 0, NULL,				0, 0},
	{ "MAX_RECURSIONS",		INT_TYPE_VAR,	DEFAULT_MAX_RECURSIONS,			NULL, 0, NULL,				0, 0 },
	{ "MENU",			STR_TYPE_VAR,	0,					NULL, 0, set_menu,			0, 0 },
	{ "MINIMUM_SERVERS",		INT_TYPE_VAR,	DEFAULT_MINIMUM_SERVERS,		NULL, 0, NULL,				0, VF_NODAEMON },
	{ "MINIMUM_USERS",		INT_TYPE_VAR,	DEFAULT_MINIMUM_USERS,			NULL, 0, NULL,				0, VF_NODAEMON },
	{ "NOTIFY_HANDLER",		STR_TYPE_VAR, 	0,					NULL, 0, set_notify_handler,		0, 0 },
	{ "NOTIFY_LEVEL",		STR_TYPE_VAR,	0,					NULL, 0, set_notify_level,		0, 0 },
	{ "NOTIFY_ON_TERMINATION",	BOOL_TYPE_VAR,	DEFAULT_NOTIFY_ON_TERMINATION,		NULL, 0, NULL,				0, VF_NODAEMON },
	{ "NOVICE",			BOOL_TYPE_VAR,	DEFAULT_NOVICE,				NULL, 0, NULL,				0, 0 },
	{ "NO_ASK_NICKNAME",		BOOL_TYPE_VAR,	DEFAULT_NO_ASK_NICKNAME,		NULL, 0, NULL,				0, 0 },
	{ "NO_CTCP_FLOOD",		BOOL_TYPE_VAR,	DEFAULT_NO_CTCP_FLOOD,			NULL, 0, NULL,				0, 0 },
	{ "OLD_ENCRYPT_PROGRAM",	BOOL_TYPE_VAR,	0,					NULL, 0, NULL,				0, VF_NODAEMON },
	{ "REALNAME",			STR_TYPE_VAR,	0,					NULL, 0, set_realname,			0, VF_NODAEMON },
	{ "SAME_WINDOW_ONLY",		BOOL_TYPE_VAR,	DEFAULT_SAME_WINDOW_ONLY,		NULL, 0, NULL,				0, 0 },
	{ "SCREEN_OPTIONS", 		STR_TYPE_VAR,	0,					NULL, 0, NULL,				0, VF_NODAEMON },
	{ "SCROLL",			BOOL_TYPE_VAR,	DEFAULT_SCROLL,				NULL, set_scroll, 0,			0, 0 },
	{ "SCROLL_LINES",		INT_TYPE_VAR,	DEFAULT_SCROLL_LINES,			NULL, set_scroll_lines, 0,		0, 0 },
	{ "SEND_IGNORE_MSG",		BOOL_TYPE_VAR,	DEFAULT_SEND_IGNORE_MSG,		NULL, 0, NULL,				0, 0 },
	{ "SHELL",			STR_TYPE_VAR,	0,					NULL, 0, NULL,				0, VF_NODAEMON },
	{ "SHELL_FLAGS",		STR_TYPE_VAR,	0,					NULL, 0, NULL,				0, VF_NODAEMON },
	{ "SHELL_LIMIT",		INT_TYPE_VAR,	DEFAULT_SHELL_LIMIT,			NULL, 0, NULL,				0, VF_NODAEMON },
	{ "SHOW_AWAY_ONCE",		BOOL_TYPE_VAR,	DEFAULT_SHOW_AWAY_ONCE,			NULL, 0, NULL,				0, 0 },
	{ "SHOW_CHANNEL_NAMES",		BOOL_TYPE_VAR,	DEFAULT_SHOW_CHANNEL_NAMES,		NULL, 0, NULL,				0, 0 },
	{ "SHOW_END_OF_MSGS",		BOOL_TYPE_VAR,	DEFAULT_SHOW_END_OF_MSGS,		NULL, 0, NULL,				0, 0 },
	{ "SHOW_NUMERICS",		BOOL_TYPE_VAR,	DEFAULT_SHOW_NUMERICS,			NULL, 0, NULL,				0, 0 },
	{ "SHOW_STARS",			BOOL_TYPE_VAR,	1,					NULL, 0, NULL,				0, 0 },
	{ "SHOW_STATUS_ALL",		BOOL_TYPE_VAR,	DEFAULT_SHOW_STATUS_ALL,		NULL, v_update_all_status, 0,		0, 0 },
	{ "SHOW_WHO_HOPCOUNT", 		BOOL_TYPE_VAR,	DEFAULT_SHOW_WHO_HOPCOUNT,		NULL, 0, NULL,				0, 0 },
	{ "SSL_CA_CHAIN_FILE", 		STR_TYPE_VAR,	0,					NULL, 0, ssl_setup_certs,		0, VF_NODAEMON },
	{ "SSL_CA_FILE", 		STR_TYPE_VAR,	0,					NULL, 0, ssl_setup_certs,		0, VF_NODAEMON },
	{ "SSL_CA_PATH", 		STR_TYPE_VAR,	0,					NULL, 0, ssl_setup_certs,		0, VF_NODAEMON },
	{ "SSL_CA_PRIVATE_KEY_FILE", 	STR_TYPE_VAR,	0,					NULL, 0, ssl_setup_certs,		0, VF_NODAEMON },
	{ "STAR_PREFIX",		STR_TYPE_VAR,	0,					NULL, 0, NULL,				0, 0 },
	{ "STATUS_AWAY",		STR_TYPE_VAR,	0,					NULL, 0, build_status,			0, 0 },
	{ "STATUS_CHANNEL",		STR_TYPE_VAR,	0,					NULL, 0, build_status,			0, 0 },
	{ "STATUS_CHANOP",		STR_TYPE_VAR,	0,					NULL, 0, build_status,			0, 0 },
	{ "STATUS_CLOCK",		STR_TYPE_VAR,	0,					NULL, 0, build_status,			0, 0 },
	{ "STATUS_FORMAT",		STR_TYPE_VAR,	0,					NULL, 0, build_status,			0, 0 },
	{ "STATUS_FORMAT1",		STR_TYPE_VAR,	0,					NULL, 0, build_status,			0, 0 },
	{ "STATUS_FORMAT2",		STR_TYPE_VAR,	0,					NULL, 0, build_status,			0, 0 },
	{ "STATUS_GROUP",		STR_TYPE_VAR,	0,					NULL, 0, build_status,			0, 0 },
	{ "STATUS_HOLD",		STR_TYPE_VAR,	0,					NULL, 0, build_status,			0, 0 },
	{ "STATUS_HOLD_LINES",		STR_TYPE_VAR,	0,					NULL, 0, build_status,			0, 0 },
	{ "STATUS_INSERT",		STR_TYPE_VAR,	0,					NULL, 0, build_status,			0, 0 },
	{ "STATUS_MAIL",		STR_TYPE_VAR,	0,					NULL, 0, build_status,			0, VF_NODAEMON },
	{ "STATUS_MODE",		STR_TYPE_VAR,	0,					NULL, 0, build_status,			0, 0 },
	{ "STATUS_NOTIFY",		STR_TYPE_VAR,	0,					NULL, 0, build_status,			0, 0 },
	{ "STATUS_OPER",		STR_TYPE_VAR,	0,					NULL, 0, build_status,			0, 0 },
	{ "STATUS_OVERWRITE",		STR_TYPE_VAR,	0,					NULL, 0, build_status,			0, 0 },
	{ "STATUS_QUERY",		STR_TYPE_VAR,	0,					NULL, 0, build_status,			0, 0 },
	{ "STATUS_SCROLLED",		STR_TYPE_VAR,	0,					NULL, 0, build_status,			0, 0 },
	{ "STATUS_SCROLLED_LINES",	STR_TYPE_VAR,	0,					NULL, 0, build_status,			0, 0 },
	{ "STATUS_SERVER",		STR_TYPE_VAR,	0,					NULL, 0, build_status,			0, 0 },
	{ "STATUS_UMODE",		STR_TYPE_VAR,	0,					NULL, 0, build_status,			0, 0 },
	{ "STATUS_USER",		STR_TYPE_VAR,	0,					NULL, 0, build_status,			0, 0 },
	{ "STATUS_USER1",		STR_TYPE_VAR,	0,					NULL, 0, build_status,			0, 0 },
	{ "STATUS_USER2",		STR_TYPE_VAR,	0,					NULL, 0, build_status,			0, 0 },
	{ "STATUS_USER3",		STR_TYPE_VAR,	0,					NULL, 0, build_status,			0, 0 },
	{ "STATUS_VOICE",		STR_TYPE_VAR,	0,					NULL, 0, build_status,			0, 0 },
	{ "STATUS_WINDOW",		STR_TYPE_VAR,	0,					NULL, 0, build_status,			0, 0 },
	{ "SUPPRESS_SERVER_MOTD",	BOOL_TYPE_VAR,	DEFAULT_SUPPRESS_SERVER_MOTD,		NULL, 0, NULL,				0, VF_NODAEMON },
	{ "SWITCH_TO_QUIET_CHANNELS",	BOOL_TYPE_VAR,	DEFAULT_SWITCH_TO_QUIET_CHANNELS,	NULL, 0, NULL,				0, VF_NODAEMON },
	{ "TAB",			BOOL_TYPE_VAR,	DEFAULT_TAB,				NULL, 0, NULL,				0, 0 },
	{ "TAB_MAX",			INT_TYPE_VAR,	DEFAULT_TAB_MAX,			NULL, 0, NULL,				0, 0 },
	{ "UNDERLINE_VIDEO",		BOOL_TYPE_VAR,	DEFAULT_UNDERLINE_VIDEO,		NULL, 0, NULL,				0, 0 },
	{ "USER_INFORMATION", 		STR_TYPE_VAR,	0,					NULL, 0, NULL,				0, 0 },
	{ "USER_WALLOPS",		BOOL_TYPE_VAR,	DEFAULT_USER_WALLOPS,			NULL, 0, NULL,				0, 0 },
	{ "VERBOSE_CTCP",		BOOL_TYPE_VAR,	DEFAULT_VERBOSE_CTCP,			NULL, 0, NULL,				0, 0 },
	{ "WARN_OF_IGNORES",		BOOL_TYPE_VAR,	DEFAULT_WARN_OF_IGNORES,		NULL, 0, NULL,				0, 0 },
	{ "WSERV_PATH",			STR_TYPE_VAR,	0,					NULL, 0, NULL,				0, 0 },
	{ "XTERM_GEOMOPTSTR", 		STR_TYPE_VAR,	0,					NULL, 0, NULL,				0, VF_NODAEMON },
	{ "XTERM_OPTIONS", 		STR_TYPE_VAR,	0,					NULL, 0, NULL,				0, VF_NODAEMON },
	{ "XTERM_PATH", 		STR_TYPE_VAR,	0,					NULL, 0, NULL,				0, VF_NODAEMON },
	{ NULL,				0,		0,					NULL, 0, NULL,				0, 0 }
};

/*
 * init_variables: initializes the string variables that can't really be
 * initialized properly above 
 */
void
init_variables(void)
{
	check_variable_order();

	set_string_var(CMDCHARS_VAR, UP(DEFAULT_CMDCHARS));
	set_string_var(LOGFILE_VAR, UP(DEFAULT_LOGFILE));
	set_string_var(SHELL_VAR, UP(DEFAULT_SHELL));
	set_string_var(SHELL_FLAGS_VAR, UP(DEFAULT_SHELL_FLAGS));
	set_string_var(DECRYPT_PROGRAM_VAR, UP(DEFAULT_DECRYPT_PROGRAM));
	set_string_var(DISPLAY_ENCODING_VAR, UP(DEFAULT_DISPLAY_ENCODING));
	set_string_var(ENCRYPT_PROGRAM_VAR, UP(DEFAULT_ENCRYPT_PROGRAM));
	set_string_var(CONTINUED_LINE_VAR, UP(DEFAULT_CONTINUED_LINE));
	set_string_var(INPUT_ENCODING_VAR, UP(DEFAULT_INPUT_ENCODING));
	set_string_var(INPUT_PROMPT_VAR, UP(DEFAULT_INPUT_PROMPT));
	set_string_var(IRC_ENCODING_VAR, UP(DEFAULT_IRC_ENCODING));
	set_string_var(HIGHLIGHT_CHAR_VAR, UP(DEFAULT_HIGHLIGHT_CHAR));
	set_string_var(HISTORY_FILE_VAR, UP(DEFAULT_HISTORY_FILE));
	set_string_var(LASTLOG_LEVEL_VAR, UP(DEFAULT_LASTLOG_LEVEL));
	set_string_var(NOTIFY_HANDLER_VAR, UP(DEFAULT_NOTIFY_HANDLER));
	set_string_var(NOTIFY_LEVEL_VAR, UP(DEFAULT_NOTIFY_LEVEL));
	set_string_var(REALNAME_VAR, my_realname());
	set_string_var(SSL_CA_CHAIN_FILE_VAR, UP(DEFAULT_SSL_CA_CHAIN_FILE));
	set_string_var(SSL_CA_FILE_VAR, UP(DEFAULT_SSL_CA_FILE));
	set_string_var(SSL_CA_PATH_VAR, UP(DEFAULT_SSL_CA_PATH));
	set_string_var(SSL_CA_PRIVATE_KEY_FILE_VAR, UP(DEFAULT_SSL_CA_PRIVATE_KEY_FILE));
	set_string_var(STAR_PREFIX_VAR, UP(DEFAULT_STAR_PREFIX));
	set_string_var(STATUS_FORMAT_VAR, UP(DEFAULT_STATUS_FORMAT));
	set_string_var(STATUS_FORMAT1_VAR, UP(DEFAULT_STATUS_FORMAT1));
	set_string_var(STATUS_FORMAT2_VAR, UP(DEFAULT_STATUS_FORMAT2));
	set_string_var(STATUS_AWAY_VAR, UP(DEFAULT_STATUS_AWAY));
	set_string_var(STATUS_CHANNEL_VAR, UP(DEFAULT_STATUS_CHANNEL));
	set_string_var(STATUS_CHANOP_VAR, UP(DEFAULT_STATUS_CHANOP));
	set_string_var(STATUS_CLOCK_VAR, UP(DEFAULT_STATUS_CLOCK));
	set_string_var(STATUS_GROUP_VAR, UP(DEFAULT_STATUS_GROUP));
	set_string_var(STATUS_HOLD_VAR, UP(DEFAULT_STATUS_HOLD));
	set_string_var(STATUS_HOLD_LINES_VAR, UP(DEFAULT_STATUS_HOLD_LINES));
	set_string_var(STATUS_INSERT_VAR, UP(DEFAULT_STATUS_INSERT));
	set_string_var(STATUS_MAIL_VAR, UP(DEFAULT_STATUS_MAIL));
	set_string_var(STATUS_MODE_VAR, UP(DEFAULT_STATUS_MODE));
	set_string_var(STATUS_OPER_VAR, UP(DEFAULT_STATUS_OPER));
	set_string_var(STATUS_OVERWRITE_VAR, UP(DEFAULT_STATUS_OVERWRITE));
	set_string_var(STATUS_QUERY_VAR, UP(DEFAULT_STATUS_QUERY));
	set_string_var(STATUS_SCROLLED_VAR, UP(DEFAULT_STATUS_SCROLLED));
	set_string_var(STATUS_SCROLLED_LINES_VAR, UP(DEFAULT_STATUS_SCROLLED_LINES));
	set_string_var(STATUS_SERVER_VAR, UP(DEFAULT_STATUS_SERVER));
	set_string_var(STATUS_UMODE_VAR, UP(DEFAULT_STATUS_UMODE));
	set_string_var(STATUS_USER_VAR, UP(DEFAULT_STATUS_USER));
	set_string_var(STATUS_USER1_VAR, UP(DEFAULT_STATUS_USER1));
	set_string_var(STATUS_USER2_VAR, UP(DEFAULT_STATUS_USER2));
	set_string_var(STATUS_USER3_VAR, UP(DEFAULT_STATUS_USER3));
	set_string_var(STATUS_WINDOW_VAR, UP(DEFAULT_STATUS_WINDOW));
	set_string_var(USER_INFO_VAR, UP(DEFAULT_USERINFO));
#ifdef WSERV_PATH
	set_string_var(WSERV_PATH_VAR, UP(WSERV_PATH));
#else
	set_string_var(WSERV_PATH_VAR, UP(DEFAULT_WSERV_PATH));
#endif
	set_string_var(XTERM_GEOMOPTSTR_VAR, UP(DEFAULT_XTERM_GEOMOPTSTR));
	set_string_var(XTERM_OPTIONS_VAR, UP(DEFAULT_XTERM_OPTIONS));
	set_string_var(XTERM_PATH_VAR, UP(DEFAULT_XTERM_PATH));
	set_alarm(DEFAULT_CLOCK_ALARM);
	set_beep_on_msg(UP(DEFAULT_BEEP_ON_MSG));
	set_string_var(STATUS_NOTIFY_VAR, UP(DEFAULT_STATUS_NOTIFY));
	set_string_var(CLIENTINFO_VAR, UP(IRCII_COMMENT));
	set_string_var(IRCHOST_VAR, get_irchost());
	set_string_var(DCCHOST_VAR, get_dcchost());
#ifdef HAVE_ICONV_OPEN
	set_irc_encoding(UP(irc_variable[IRC_ENCODING_VAR].string));
	set_input_encoding(UP(irc_variable[INPUT_ENCODING_VAR].string));
	set_display_encoding(UP(irc_variable[DISPLAY_ENCODING_VAR].string));
#endif /* HAVE_ICONV_OPEN */
	set_string_var(HELP_PATH_VAR, UP(DEFAULT_HELP_PATH));
	set_lastlog_size(irc_variable[LASTLOG_VAR].integer);
	set_history_size(irc_variable[HISTORY_VAR].integer);
	set_history_file(UP(irc_variable[HISTORY_FILE_VAR].string));
	set_highlight_char(UP(irc_variable[HIGHLIGHT_CHAR_VAR].string));
	set_lastlog_level(UP(irc_variable[LASTLOG_LEVEL_VAR].string));
	set_notify_level(UP(irc_variable[NOTIFY_LEVEL_VAR].string));
	if (get_int_var(LOG_VAR))
		set_int_var(LOG_VAR, 1);
}

/*
 * check_variable_order: make sure irc_variable[] is properly ordered.
 */
static	void
check_variable_order(void)
{
	IrcVariable *curr, *prev = NULL;
	size_t	len;

	for (curr = irc_variable; curr->name; prev = curr++)
		if (prev && (len = my_strlen(prev->name)) && my_strncmp(prev->name, curr->name, len) > 0)
		{
			yell("irc_variables[] order is wrong at elements \"%s\" and \"%s\"!", prev->name, curr->name);
			abort();
		}
			
}

/*
 * find_variable: looks up variable name in the variable table and returns
 * the index into the variable array of the match.  If there is no match, cnt
 * is set to 0 and -1 is returned.  If more than one match the string, cnt is
 * set to that number, and it returns the first match.  Index will contain
 * the index into the array of the first found entry 
 */
static	int
find_variable(u_char *org_name, int *cnt)
{
	IrcVariable *v,
		    *first;
	size_t	len;
	int	var_index;
	u_char	*name = NULL;

	malloc_strcpy(&name, org_name);
	upper(name);
	len = my_strlen(name);
	var_index = 0;
	for (first = irc_variable; first->name; first++, var_index++)

	{
		if (my_strncmp(name, first->name, len) == 0)
		{
			*cnt = 1;
			break;
		}
	}
	if (first->name)

	{
		if (my_strlen(first->name) != len)
		{
			v = first;
			for (v++; v->name; v++, (*cnt)++)

			{
				if (my_strncmp(name, v->name, len) != 0)
					break;
			}
		}
		new_free(&name);
		return (var_index);
	}
	else
	{
		*cnt = 0;
		new_free(&name);
		return (-1);
	}
}

/*
 * do_boolean: just a handy thing.  Returns 1 if the str is not ON, OFF, or
 * TOGGLE 
 */
int
do_boolean(u_char *str, int *value)
{
	upper(str);
	if (my_strcmp(str, var_settings(ON)) == 0)
		*value = 1;
	else if (my_strcmp(str, var_settings(OFF)) == 0)
		*value = 0;
	else if (my_strcmp(str, var_settings(TOGGLE)) == 0)
	{
		if (*value)
			*value = 0;
		else
			*value = 1;
	}
	else
		return (1);
	return (0);
}

/*
 * set_var_value: Given the variable structure and the string representation
 * of the value, this sets the value in the most verbose and error checking
 * of manors.  It displays the results of the set and executes the function
 * defined in the var structure 
 */
void
set_var_value(int var_index, u_char *value)
{
	u_char	*rest;
	IrcVariable *var;
	int	old;


	var = &(irc_variable[var_index]);
#ifdef DAEMON_UID
	if (getuid() == DAEMON_UID && var->flags&VF_NODAEMON && value && *value)
	{
		say("You are not permitted to set that variable");
		return;
	}
#endif /* DAEMON_UID */
	switch (var->type)
	{
	case BOOL_TYPE_VAR:
		if (value && *value && (value = next_arg(value, &rest)))
		{
			old = var->integer;
			if (do_boolean(value, &(var->integer)))

			{
				say("Value must be either ON, OFF, or TOGGLE");
				break;
			}
			if (!(var->int_flags & VIF_CHANGED))
			{
				if (old != var->integer)
					var->int_flags |= VIF_CHANGED;
			}
			if (loading_global())
				var->int_flags |= VIF_GLOBAL;
			if (var->ifunc)
				(*var->ifunc)(var->integer);
			say("Value of %s set to %s", var->name,
				var->integer ? var_settings(ON)
					     : var_settings(OFF));
		}
		else
			say("Current value of %s is %s", var->name,
				(var->integer) ?
				var_settings(ON) : var_settings(OFF));
		break;
	case CHAR_TYPE_VAR:
		if (value && *value && (value = next_arg(value, &rest)))
		{
			if ((int) my_strlen(value) > 1)
				say("Value of %s must be a single character",
					var->name);
			else
			{
				if (!(var->int_flags & VIF_CHANGED))
				{
					if (var->integer != *value)
						var->int_flags |= VIF_CHANGED;
				}
				if (loading_global())
					var->int_flags |= VIF_GLOBAL;
				var->integer = *value;
				if (var->ifunc)
					(*var->ifunc)(var->integer);
				say("Value of %s set to '%c'", var->name,
					var->integer);
			}
		}
		else
			say("Current value of %s is '%c'", var->name,
				var->integer);
		break;
	case INT_TYPE_VAR:
		if (value && *value && (value = next_arg(value, &rest)))
		{
			int	val;

			if (!is_number(value))
			{
				say("Value of %s must be numeric!", var->name);
				break;
			}
			if ((val = my_atoi(value)) < 0)
			{
				say("Value of %s must be greater than 0",
					var->name);
				break;
			}
			if (!(var->int_flags & VIF_CHANGED))
			{
				if (var->integer != val)
					var->int_flags |= VIF_CHANGED;
			}
			if (loading_global())
				var->int_flags |= VIF_GLOBAL;
			var->integer = val;
			if (var->ifunc)
				(*var->ifunc)(var->integer);
			say("Value of %s set to %d", var->name, var->integer);
		}
		else
			say("Current value of %s is %d", var->name,
				var->integer);
		break;
	case STR_TYPE_VAR:
		if (value)
		{
			if (*value)
			{
				u_char	*temp = NULL;

				if (var->flags & VF_EXPAND_PATH)
				{
					temp = expand_twiddle(value);
					if (temp)
						value = temp;
					else
						say("SET: no such user");
				}
				if ((!var->int_flags & VIF_CHANGED))
				{
					if ((var->string && ! value) ||
					    (! var->string && value) ||
					    my_stricmp(var->string, value))
						var->int_flags |= VIF_CHANGED;
				}
				if (loading_global())
					var->int_flags |= VIF_GLOBAL;
				malloc_strcpy(&var->string, value);
				if (temp)
					new_free(&temp);
			}
			else
			{
				if (var->string)
					say("Current value of %s is %s",
						var->name, var->string);
				else
					say("No value for %s has been set",
						var->name);
				return;
			}
		}
		else
			new_free(&var->string);
		if (var->sfunc)
			(*var->sfunc)(var->string);
		say("Value of %s set to %s", var->name, var->string ?
			var->string : UP("<EMPTY>"));
		break;
	}
}

/*
 * set_variable: The SET command sets one of the irc variables.  The args
 * should consist of "variable-name setting", where variable name can be
 * partial, but non-ambbiguous, and setting depends on the variable being set 
 */
void
set_variable(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*var;
	int	cnt,
		var_index,
		lastlog_level;

	if ((var = next_arg(args, &args)) != NULL)
	{
		if (*var == '-')
		{
			var++;
			args = NULL;
		}
		var_index = find_variable(var, &cnt);
		switch (cnt)
		{
		case 0:
			say("No such variable \"%s\"", var);
			return;
		case 1:
			set_var_value(var_index, args);
			return;
		default:
			say("%s is ambiguous", var);
			for (cnt += var_index; var_index < cnt; var_index++)
				set_var_value(var_index, empty_string());
			return;
		}
	}
	lastlog_level = message_from_level(LOG_CRAP);
	for (var_index = 0; var_index < NUMBER_OF_VARIABLES; var_index++)
		set_var_value(var_index, empty_string());
	(void) message_from_level(lastlog_level);
}

/*
 * get_string_var: returns the value of the string variable given as an index
 * into the variable table.  Does no checking of variable types, etc 
 */
u_char	*
get_string_var(int var)
{
	return irc_variable[var].string;
}

/*
 * get_int_var: returns the value of the integer string given as an index
 * into the variable table.  Does no checking of variable types, etc 
 */
int
get_int_var(int var)
{
	return (irc_variable[var].integer);
}

/*
 * set_string_var: sets the string variable given as an index into the
 * variable table to the given string.  If string is null, the current value
 * of the string variable is freed and set to null 
 */
void
set_string_var(int var, u_char *string)
{
	if (string)
		malloc_strcpy(&irc_variable[var].string, string);
	else
		new_free(&irc_variable[var].string);
}

/*
 * set_int_var: sets the integer value of the variable given as an index into
 * the variable table to the given value 
 */
void
set_int_var(int var, unsigned value)
{
	if (var == NOVICE_VAR && !get_load_depth() && !value)
	{
say("WARNING: Setting NOVICE to OFF enables commands in your client which");
say("         could be used by others on IRC to control your IRC session");
say("         or compromise security on your machine. If somebody has");
say("         asked you to do this, and you do not know EXACTLY why, or if");
say("         you are not ABSOLUTELY sure what you are doing, you should");
say("         immediately /SET NOVICE ON and find out more information.");
	}
	irc_variable[var].integer = value;
}

/*
 * save_variables: this writes all of the IRCII variables to the given FILE
 * pointer in such a way that they can be loaded in using LOAD or the -l switch 
 */
void
save_variables(FILE *fp, int do_all)
{
	IrcVariable *var;

	for (var = irc_variable; var->name; var++)
	{
		if (!(var->int_flags & VIF_CHANGED))
			continue;
		if (do_all || !(var->int_flags & VIF_GLOBAL))
		{
			if (my_strcmp(var->name, "DISPLAY") == 0 || my_strcmp(var->name, "CLIENT_INFORMATION") == 0)
				continue;
			fprintf(fp, "SET ");
			switch (var->type)
			{
			case BOOL_TYPE_VAR:
				fprintf(fp, "%s %s\n", var->name, var->integer ?
					var_settings(ON) : var_settings(OFF));
				break;
			case CHAR_TYPE_VAR:
				fprintf(fp, "%s %c\n", var->name, var->integer);
				break;
			case INT_TYPE_VAR:
				fprintf(fp, "%s %u\n", var->name, var->integer);
				break;
			case STR_TYPE_VAR:
				if (var->string)
					fprintf(fp, "%s %s\n", var->name,
						var->string);
				else
					fprintf(fp, "-%s\n", var->name);
				break;
			}
		}
	}
}

u_char	*
make_string_var(u_char *var_name)
{
	int	cnt,
		var_index;
	u_char	lbuf[BIG_BUFFER_SIZE],
		*ret = NULL,
		*cmd = NULL;

	malloc_strcpy(&cmd, var_name);
	upper(cmd);
	if (((var_index = find_variable(cmd, &cnt)) == -1) ||
	    (cnt > 1) ||
	    my_strcmp(cmd, irc_variable[var_index].name))
		goto out;
	switch (irc_variable[var_index].type)
	{
	case STR_TYPE_VAR:
		malloc_strcpy(&ret, irc_variable[var_index].string);
		break;
	case INT_TYPE_VAR:
		snprintf(CP(lbuf), sizeof lbuf, "%u", irc_variable[var_index].integer);
		malloc_strcpy(&ret, lbuf);
		break;
	case BOOL_TYPE_VAR:
		malloc_strcpy(&ret, UP(var_settings(irc_variable[var_index].integer)));
		break;
	case CHAR_TYPE_VAR:
		snprintf(CP(lbuf), sizeof lbuf, "%c", irc_variable[var_index].integer);
		malloc_strcpy(&ret, lbuf);
		break;
	}
out:
	new_free(&cmd);
	return (ret);

}

/* exec_warning: a warning message displayed whenever EXEC_PROTECTION is turned off.  */
static	void
exec_warning(int value)
{
	if (value == OFF)
	{
		say("Warning!  You have turned EXEC_PROTECTION off");
		say("Please read the /HELP SET EXEC_PROTECTION documentation");
	}
}

static	void
input_warning(int value)
{
	if (value == OFF)
	{
		say("Warning!  You have turned INPUT_PROTECTION off");
		say("Please read the /HELP ON INPUT, and /HELP SET INPUT_PROTECTION documentation");
	}
}

/* returns the size of the character set */
int
charset_size(void)
{
	return get_int_var(EIGHT_BIT_CHARACTERS_VAR) ? 256 : 128;
}

static	void
eight_bit_characters(int value)
{
	if (value == ON && !term_eight_bit())
		say("Warning!  Your terminal says it does not support eight bit characters");
	set_term_eight_bit(value);
}

static	void
v_update_all_status(int dummy)
{
	update_all_status();
}

char *
var_settings(int idx)
{
	if (idx == OFF)
		return "OFF";
	else if (idx == ON)
		return "ON";
	else if (idx == TOGGLE)
		return "TOGGLE";
	return "<internal error>";
};
