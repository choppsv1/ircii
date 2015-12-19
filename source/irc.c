/*
 * ircII: a new irc client.  I like it.  I hope you will too!
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

#define IRCII_VERSION	"20151120"	/* YYYYMMDD */

#include "irc.h"
IRCII_RCSID("@(#)$eterna: irc.c,v 1.386 2015/11/20 09:23:54 mrg Exp $");

#include <sys/stat.h>
#include <pwd.h>

#ifdef DO_USER2
# include <setjmp.h>
#endif /* DO_USER2 */

#include "status.h"
#include "dcc.h"
#include "names.h"
#include "vars.h"
#include "input.h"
#include "alias.h"
#include "output.h"
#include "ircterm.h"
#include "exec.h"
#include "screen.h"
#include "log.h"
#include "server.h"
#include "hook.h"
#include "keys.h"
#include "ircaux.h"
#include "edit.h"
#include "window.h"
#include "history.h"
#include "exec.h"
#include "notify.h"
#include "mail.h"
#include "debug.h"
#include "newio.h"
#include "ctcp.h"
#include "parse.h"
#include "strsep.h"
#include "notice.h"
#include "icb.h"

static	int	qflag;			/* set if we ignore .ircrc */
static	int	bflag;			/* set if we load .ircrc before connecting */
static	int	tflag = 1;		/* don't use termcap ti/te sequences */
static	time_t	current_idle_time;	/* last time the user hit a key */
static	time_t  start_time;		/* epoch time we started */

static	int	cntl_c_hit = 0,
		irc_port_local = IRC_PORT,	/* port of ircd */
		add_servers = 0,		/* -a flag */
		do_refresh_screen,
		no_full_screen = 0,
		cur_signal = -1,		/* head of below */
		occurred_signals[16],		/* got signal which may
						   require /ON action */
		client_default_icb = 0,		/* default to irc server
						   connections */
		fflag = USE_FLOW_CONTROL,	/* true: ^Q/^S used for flow
						   cntl */
		using_server_process = 0;	/* Don't want to start ircio
						   by default... */

static	u_char	*irc_path,			/* paths used by /load */
		*ircrc_file,			/* full path .ircrc file */
		*ircquick_file,			/* full path .ircquick file */
		*progname,			/* set from argv[0] */
		*send_umode,			/* sent umode */
		*args_str,			/* list of command line args */
		hostname[NAME_LEN + 1],		/* name of current host */
		realname[REALNAME_LEN + 1],	/* real name of user */
		username[NAME_LEN + 1],		/* usernameof user */
		*nickname,			/* users nickname */
		*source_host,			/* specify a specific source
						   host for multi-homed
						   machines */
		*irc_lib,			/* path to the ircII library */
		*my_path_dir,			/* path to users home dir */
		*log_file,			/* path to debug log file */
		*default_proxy,			/* default proxy to use */
		irc_version_str[] = IRCII_VERSION;

#ifdef DO_USER2
static	jmp_buf	outta_here;
#endif /* DO_USER2 */

static	void	cntl_c(int);
static	void	sig_user1(int);
static	void	sig_refresh_screen(int);
static	void	sig_on(int);
static	void	real_sig_user1(void);
static	int	do_sig_user1;
#ifdef	DO_USER2
static	void	sig_user2(int) ;
#endif /* DO_USER2 */
static	int	irc_do_a_screen(Screen *, fd_set *);
static	void	irc_io(void);
static	void	quit_response(u_char *, u_char *);
static	void	show_version(void);
static	u_char	*get_arg(u_char *, u_char *, int *);
static	int	parse_arg(u_char *, u_char **, int *, u_char **);
static	u_char	*parse_args(u_char **, int);
static	void	input_pause(void);

#ifdef DEBUG
#define DEBUG_USAGE1 \
"\
   -D <level>\tenable debugging at the specified level\n"
#define DEBUG_USAGE2 \
"\
   -o <file>\twrite debug output to file\n"
#else
#define DEBUG_USAGE1 ""
#define DEBUG_USAGE2 ""
#endif

static	char	switch_help[] =
"Usage: irc [switches] [nickname] [server list] \n\
  The [nickname] can be at most 9 characters long on some server\n\
  The [server list] is a whitespace separated list of server names\n\
  The [switches] may be any or all of the following:\n\
   -a\t\tadds default servers and command line servers to server list\n\
   -b\t\tload .ircrc before connecting to a server\n\
   -c <channel>\tjoins <channel> on startup\n\
   -d\t\truns IRCII in non full screen terminal mode\n"
   DEBUG_USAGE1 "\
   -f\t\tyour terminal uses flow control (^S/^Q), so IRCII shouldn't\n\
   -F\t\tyour terminal doesn't use flow control (default)\n\
   -h <host>\tsource host, for multihomed machines\n\
   -H <host>\toriginating address for dcc requests (to work behind NATs etc)\n\
   -icb\t\tuse ICB connections by default\n\
   -irc\t\tuse IRC connections by default\n\
   -I <file>\tloads <file> in place of your .ircquick\n\
   -l <file>\tloads <file> in place of your .ircrc\n"
   DEBUG_USAGE2 "\
   -p <port>\tdefault IRC server connection port (usually 6667)\n\
   -P <port>\tdefault ICB server connection port (usually 7326)\n\
   -q\t\tdoes not load .ircrc nor .ircquick\n\
   -r\t\treverse terminal colours (only if colours are activated)\n\
   -R <host>[:<port>]\tdefault to using HTTP proxy for connections\n\
   -s\t\tdon't use separate server processes (ircio)\n\
   -S\t\tuse separate server processes (ircio)\n\
   -t\t\tdo not use termcap ti and te sequences at startup\n\
   -T\t\tuse termcap ti and te sequences at startup (default)\n\
Usage: icb [same switches]  (default to -icb)\n";

/* irc_exit: cleans up and leaves */
void
irc_exit(void)
{
	do_hook(EXIT_LIST, "Exiting");
	close_server(-1, empty_string());
	logger(0);
	set_history_file(NULL);
	clean_up_processes();
	if (!term_basic())
	{
		cursor_to_input();	/* Needed so that ircII doesn't gobble
					 * the last line of the kill. */
		term_cr();
		if (term_clear_to_eol())
			term_space_erase(0);
		term_reset();
	}
	exit(0);
}

/*
 * quit_response: Used by irc_io when called from irc_quit to see if we got
 * the right response to our question.  If the response was affirmative, the
 * user gets booted from irc.  Otherwise, life goes on. 
 */
static	void
quit_response(u_char *dummy, u_char *ptr)
{
	size_t	len;

	if ((len = my_strlen(ptr)) != 0)
	{
		if (!my_strnicmp(ptr, UP("yes"), len))
		{
			send_to_server("QUIT");
			irc_exit();
		}
	}
}

/* irc_quit: prompts the user if they wish to exit, then does the right thing */
void
irc_quit(u_int key, u_char *ptr)
{
	static	int in_it = 0;

	if (in_it)
		return;
	in_it = 1;
	add_wait_prompt(UP("Do you really want to quit? "), quit_response,
		empty_string(), WAIT_PROMPT_LINE);
	in_it = 0;
}

void
on_signal_occurred(int signo)
{
	if (cur_signal > ARRAY_SIZE(occurred_signals))
		return;
	occurred_signals[++cur_signal] = signo;
}

/*
 * cntl_c: emergency exit.... if somehow everything else freezes up, hitting
 * ^C five times should kill the program. 
 */
static	void
cntl_c(int signo)
{
	on_signal_occurred(signo);

	if (cntl_c_hit++ >= 4)
		irc_exit();
}

static void
sig_on(int signo)
{
	on_signal_occurred(signo);
}

static void
sig_user1(int signo)
{
	do_sig_user1++;
	on_signal_occurred(signo);
}

static void
real_sig_user1(void)
{
	say("Got SIGUSR1, closing DCC connections and EXECed processes");
	close_all_dcc();
	close_all_exec();
}

#ifdef DO_USER2
static void
sig_user2(int signo)
{
	on_signal_occurred(signo);
	say("Got SIGUSR2, jumping to normal loop");	/* unsafe */
	longjmp(outta_here);
}
#endif 

static void
sig_refresh_screen(int signo)
{
	on_signal_occurred(signo);
	do_refresh_screen++;
}

/* shows the version of irc */
static	void
show_version(void)
{
	printf("ircII version %s\n\r", irc_version());
	exit (0);
}

static	void
open_log_file(u_char *file)
{
	FILE	*fp;

	if (!file)
	{
		printf("irc: need filename for -o\n");
		exit(-1);
	}
	fflush(stderr);
	fp = freopen(CP(file), "w", stderr);
	if (!fp)
	{
		printf("irc: can not open %s: %s\n",
		       file,
		       errno ? "" : strerror(errno));
		exit(-1);
	}
}

/* get_arg: used by parse_args() to get an argument after a switch */
static	u_char	*
get_arg(u_char *arg, u_char *next, int *ac)
{
	(*ac)++;
	if (*arg)
		return (arg);
	else
	{
		if (next)
			return (next);
		fprintf(stderr, "irc: missing parameter\n");
		exit(1);
		return (0); /* cleans up a warning */
	}
}

/*
 * parse_arg: parse a single command line argument.  returns 1 when
 * a "--" sequence is detected.
 */
static	int
parse_arg(u_char *arg, u_char **argv, int *acp, u_char **channelp)
{
	while (*arg)
	{
		switch (*(arg++))
		{
		case 'a':
			add_servers = 1;
			break;
		case 'b':
			if (qflag)
			{
				fprintf(stderr, "Can not use -b with -q\n");
				exit(1);
			}
			bflag = 1;
			break;
		case 'c':
			malloc_strcpy(channelp, get_arg(arg, argv[*acp], acp));
			break;
		case 'd':
			no_full_screen = 1;
			break;
#ifdef DEBUG
		case 'D':
			setdlevel(get_arg(arg, argv[*acp], acp));
			break;
#endif /* DEBUG */
		case 'e':
			{
				u_char *proto = get_arg(arg, argv[*acp], acp);
				server_default_encryption(proto,
							  get_arg(arg,
							      argv[*acp], acp));
			}
			break;
		case 'f':
			fflag = 1;
			break;
		case 'F':
			fflag = 0;
			break;
		case 'h':
			malloc_strcpy(&source_host,
				      get_arg(arg, argv[*acp], acp));
			break;
		case 'H':
			set_dcchost(get_arg(arg, argv[*acp], acp));
			break;
		case 'i':
			if (arg[0] == 'r' && arg[1] == 'c')
				client_default_icb = 0;
			else
			if (arg[0] == 'c' && arg[1] == 'b')
				client_default_icb = 1;
			else
			{
				printf("irc: invalid -i option; use -irc "
				       "or -icb");
				exit(-1);
			}
			arg += 2;
			break;
		case 'I':
			malloc_strcpy(&ircquick_file,get_arg(arg, argv[*acp], acp));
			break;
		case 'l':
			malloc_strcpy(&ircrc_file, get_arg(arg, argv[*acp], acp));
			break;
#ifdef DEBUG
		case 'o':
			log_file = get_arg(arg, argv[*acp], acp);
			break;
#endif /* DEBUG */
		case 'p':
			irc_port_local = my_atoi(get_arg(arg, argv[*acp], acp));
			break;
		case 'P':
			set_icb_port(my_atoi(get_arg(arg, argv[*acp], acp)));
			break;
		case 'q':
			if (bflag)
			{
				fprintf(stderr, "Can not use -q with -b\n");
				exit(1);
			}
			qflag = 1;
			break;
		case 'r':
			{
				int oldfg = get_int_var(FOREGROUND_COLOUR_VAR);
				set_int_var(FOREGROUND_COLOUR_VAR,
					    get_int_var(BACKGROUND_COLOUR_VAR));
				set_int_var(BACKGROUND_COLOUR_VAR, oldfg);
			}
			break;
		case 'R':
			default_proxy = get_arg(arg, argv[*acp], acp);
			break;
		case 's':
			using_server_process = 0;
			break;
		case 'S':
			using_server_process = 1;
			break;
		case 't':
			tflag = 1;
			break;
		case 'T':
			tflag = 0;
			break;
		case 'v':
			show_version();
			break;
		case '-':
			return 1;
		default:
			fputs(switch_help, stderr);
			exit(1);
		}
	}

	return 0;
}


/*
 * parse_args: parse command line arguments for irc, and sets all initial
 * flags, etc. 
 */
static	u_char	*
parse_args(u_char **argv, int argc)
{
	u_char	*arg,
		*ptr;
	int	ac;
	u_char	*channel = NULL;
	struct	passwd	*entry;
	int	minus_minus = 0;

	if (my_strncmp(argv[0], "icb", 3) == 0 || strstr(CP(argv[0]), "/icb") != 0)
		client_default_icb = 1;
	*realname = '\0';
	ac = 1;
	malloc_strcpy(&args_str, argv[0]);
	malloc_strcat(&args_str, UP(" "));
	while (ac < argc && (arg = argv[ac++]) != NULL)
	{
		malloc_strcat(&args_str, arg);
		malloc_strcat(&args_str, UP(" "));
		if (*arg == '-')
		{
			minus_minus = parse_arg(arg+1, argv, &ac, &channel);
			if (minus_minus)
				break;
		}
		else
		{
			if (nickname && *nickname)
				build_server_list(arg);
			else
				malloc_strcpy(&nickname, arg);
		}
	}
	if (NULL != (ptr = my_getenv("IRCLIB")))
	{
		malloc_strcpy(&irc_lib, ptr);
		malloc_strcat(&irc_lib, UP("/"));
	}
	else
		malloc_strcpy(&irc_lib, UP(IRCLIB));

	/* -h overrides environment variable */
	if (NULL == source_host && NULL != (ptr = my_getenv("IRCHOST")))
		malloc_strcpy(&source_host, ptr);

	if (NULL == ircrc_file && NULL != (ptr = my_getenv("IRCRC")))
		malloc_strcpy(&ircrc_file, ptr);

	if (NULL == ircquick_file && NULL != (ptr = my_getenv("IRCQUICK")))
		malloc_strcpy(&ircquick_file, ptr);

	if ((nickname == 0 || *nickname == '\0') && NULL != (ptr = my_getenv("IRCNICK")))
		malloc_strcpy(&nickname, ptr);

	if ((ptr = my_getenv("USER")) != NULL)
		my_strmcpy(username, ptr, NAME_LEN);

	if (NULL != (ptr = my_getenv("IRCUMODE")))
		malloc_strcpy(&send_umode, ptr);

	if (NULL != (ptr = my_getenv("IRCNAME")))
		my_strmcpy(realname, ptr, REALNAME_LEN);

	if (NULL != (ptr = my_getenv("IRCPATH")))
		malloc_strcpy(&irc_path, ptr);
	else
	{
#ifdef IRCPATH
		malloc_strcpy(&irc_path, UP(IRCPATH));
#else
		malloc_strcpy(&irc_path, UP(".:~/.irc:"));
		malloc_strcat(&irc_path, irc_lib);
		malloc_strcat(&irc_path, UP("script"));
#endif /* IRCPATH */
	}

	/* now, open the log file, which redirects stderr */
	if (log_file)
		open_log_file(log_file);

	if (default_proxy)
		server_set_default_proxy(default_proxy);

	set_string_var(LOAD_PATH_VAR, irc_path);
	new_free(&irc_path);
	if (NULL != (ptr = my_getenv("IRCSERVER")))
		build_server_list(ptr);
	if (0 == number_of_servers() || add_servers)
	{
		if (read_server_file() || 0 == number_of_servers())
		{
			u_char *s = NULL;

			malloc_strcpy(&s, UP(DEFAULT_SERVER));
			build_server_list(s);
			new_free(&s);
		}
	}

	if (NULL != (entry = getpwuid(getuid())))
	{
		if ((*realname == '\0') && entry->pw_gecos && *(entry->pw_gecos))
		{
#ifdef GECOS_DELIMITER
			if ((ptr = my_index(entry->pw_gecos, GECOS_DELIMITER)) 
					!= NULL)
				*ptr = '\0';
#endif /* GECOS_DELIMITER */
			if ((ptr = my_index(entry->pw_gecos, '&')) == NULL)
				my_strmcpy(realname, entry->pw_gecos, REALNAME_LEN);
			else {
				size_t len = ptr - (u_char *) entry->pw_gecos;

				if (len < REALNAME_LEN && *(entry->pw_name)) {
					u_char *q = realname + len;

					my_strmcpy(realname, entry->pw_gecos, len);
					my_strmcat(realname, entry->pw_name, REALNAME_LEN);
					my_strmcat(realname, ptr + 1, REALNAME_LEN);
					if (islower(*q) && (q == realname || isspace(*(q - 1))))
						*q = toupper(*q);
				} else
					my_strmcpy(realname, entry->pw_gecos, REALNAME_LEN);
			}
		}
		if (entry->pw_name && *(entry->pw_name) && '\0' == *username)
			my_strmcpy(username, entry->pw_name, NAME_LEN);

		if (entry->pw_dir && *(entry->pw_dir))
			malloc_strcpy(&my_path_dir, UP(entry->pw_dir));
	}
	if (NULL != (ptr = my_getenv("HOME")))
		malloc_strcpy(&my_path_dir, ptr);
	else if (*my_path_dir == '\0')
		malloc_strcpy(&my_path_dir, UP("/"));

	if ('\0' == *realname)
		my_strmcpy(realname, "*Unknown*", REALNAME_LEN);

	if ('\0' == *username)
		my_strmcpy(username, "Unknown", NAME_LEN);

	if (NULL != (ptr = my_getenv("IRCUSER")))
		my_strmcpy(username, ptr, REALNAME_LEN);

	if (nickname == 0 || *nickname == '\0')
		malloc_strcpy(&nickname, username);
	if (NULL == ircrc_file)
	{
		ircrc_file = new_malloc(my_strlen(my_path_dir) +
			my_strlen(IRCRC_NAME) + 1);
		my_strcpy(ircrc_file, my_path_dir);
		my_strcat(ircrc_file, IRCRC_NAME);
	}
	if (NULL == ircquick_file)
	{
		ircquick_file = new_malloc(my_strlen(my_path_dir) +
			my_strlen(IRCQUICK_NAME) + 1);
		my_strcpy(ircquick_file, my_path_dir);
		my_strcat(ircquick_file, IRCQUICK_NAME);
	}

	return (channel);
}

/*
 * input_pause: prompt the user for a single keystroke before continuing.
 * Only call this before calling irc_io().
 */
static void
input_pause(void)
{
	const u_char *msg = UP("********  Press any key to continue  ********");
	char dummy;

	if (term_basic())
	{
		puts(CP(msg));
		fflush(stdout);
	}
	else
	{
		int cols;

		copy_window_size(NULL, &cols);
		term_move_cursor(0, cols);
		output_line(msg, 0);
		cursor_not_in_display();
	}
	(void)read(0, &dummy, 1);
}

/*
 * generally processes one screen's input.  this used to be part of irc_io()
 * but has been split out because that function was too large and too nested.
 * it returns 0 if IO processing should continue and 1 if it should stop.
 */
static	int
irc_do_a_screen(Screen *screen, fd_set *rd) 
{
	if (!screen_get_alive(screen))
		return 0;
	set_current_screen(screen);
	if (!is_main_screen(screen) &&
	    FD_ISSET(screen_get_wserv_fd(screen), rd))
		screen_wserv_message(screen);
	if (FD_ISSET(screen_get_fdin(screen), rd))
	{
		/* buffer much bigger than IRCD_BUFFER_SIZE */
		u_char	lbuf[BIG_BUFFER_SIZE + 1];

		/*
		 * This section of code handles all in put from the terminal(s)
		 * connected to ircII.  Perhaps the idle time *shouldn't* be
		 * reset unless its not a screen-fd that was closed..
		 */
		current_idle_time = time(0);
		if (term_basic())
		{
			int     old_timeout;

			old_timeout = dgets_timeout(1);
			switch (dgets(lbuf, INPUT_BUFFER_SIZE, screen_get_fdin(screen)))
			{
			case 0:
			case -1:
				say("IRCII exiting on EOF from stdin");
				irc_exit();
				break;
			default:
				(void) dgets_timeout(old_timeout);
				*(lbuf + my_strlen(lbuf) - 1) = '\0';
				if (get_int_var(INPUT_ALIASES_VAR))	
					parse_line(NULL, lbuf,
					    empty_string(), 1, 0, 0);
				else
					parse_line(NULL, lbuf,
					    NULL, 1, 0, 0);
				break;
			}
		}
		else
		{
			int server;
			u_char	loc_buffer[BIG_BUFFER_SIZE + 1];
			int	n, i;

			server = set_from_server(get_window_server(0));
			set_last_input_screen(screen);
			if ((n = read(screen_get_fdin(screen), loc_buffer,
					BIG_BUFFER_SIZE)) != 0)
				for (i = 0; i < n; i++)
					edit_char((u_int)loc_buffer[i]);
				/*
				 * if the current screen isn't the main  screen,
				 * then the socket to the current screen must have
				 * closed, so we call kill_screen() to handle 
				 * this - phone, jan 1993.
				 * but not when we arent running windows - Fizzy, may 1993
				 * if it is the main screen we got an EOF on, we exit..
				 * closed tty -> chew cpu -> bad .. -phone, july 1993.
				 */
#ifdef WINDOW_CREATE
			else
			{
				if (!is_main_screen(screen))
					kill_screen(screen);
				else
					irc_exit();
			}
#endif /* WINDOW_CREATE */
			cntl_c_hit = 0;
			set_from_server(server);
		}
	}
	return 0;
}

/*
 * irc_io: the main irc input/output loop.   Handles all io from keyboard,
 * server, exec'd processes, etc.  If a prompt is specified, it is displayed
 * in the input line and cannot be backspaced over, etc. The func is a
 * function which will take the place of the SEND_LINE function (which is
 * what happens when you hit return at the end of a line). This function must
 * decide if it's ok to exit based on anything you really want.
 */
void
irc_io(void)
{
	fd_set	rd,
		wd;
	struct	timeval cursor_timeout,
		clock_timeout,
		right_away,
		timer,
		*timeptr;
	int	hold_over;
	Screen	*screen,
		*old_current_screen;
	int	irc_io_loop = 1;
	int	i;

	/* time before cursor jumps from display area to input line */
	cursor_timeout.tv_usec = 0L;
	cursor_timeout.tv_sec = 1L;

	/* time delay for updating of internal clock */
	clock_timeout.tv_usec = 0L;
	clock_timeout.tv_sec = 30L;

	right_away.tv_usec = 0L;
	right_away.tv_sec = 0L;

	if (!term_basic())
		set_input_prompt(get_string_var(INPUT_PROMPT_VAR));

#ifdef DO_USER2
	if (setjmp(outta_here))
		yell("*** Got SIGUSR2, Aborting");
#endif /* DO_USER2 */

	timeptr = &clock_timeout;
	do
	{
		set_ctcp_was_crypted(0);
		FD_ZERO(&rd);
		FD_ZERO(&wd);
		set_process_bits(&rd);
		server_set_bits(&rd, &wd);
		for (screen = screen_first(); screen; screen = screen_get_next(screen))
			if (screen_get_alive(screen))
			{
				FD_SET(screen_get_fdin(screen), &rd);
				if (!is_main_screen(screen))
					FD_SET(screen_get_wserv_fd(screen), &rd);
			}
		set_dcc_bits(&rd, &wd);
		term_check_refresh();
		timer_timeout(&timer);
		if (timer.tv_sec <= timeptr->tv_sec)
			timeptr = &timer;
		if ((hold_over = unhold_windows()) != 0)
			timeptr = &right_away;
		Debug(DB_IRCIO, "irc_io: selecting with %ld:%ld timeout", timeptr->tv_sec,
			(long)timeptr->tv_usec);
		switch (new_select(&rd, &wd, timeptr))
		{
		case 0:
		case -1:
	/*
	 * yay for the QNX socket manager... drift here, drift there, oops,
	 * i fell down a hole..
	 */
#ifdef __QNX__
			if (errno == EBADF || errno == ESRCH)
				irc_io_loop = 0;
#endif /* __QNX__ */
			if (cntl_c_hit)
			{
				edit_char((u_int)'\003');
				cntl_c_hit = 0;
			}
			check_status_alarmed();
			if (do_refresh_screen)
			{
				refresh_screen(0, NULL);
				do_refresh_screen = 0;
			}
			if (do_sig_user1)
			{
				real_sig_user1();
				do_sig_user1 = 0;
			}
			for (i = 0; i <= cur_signal; i++)
				if (occurred_signals[i] != 0)
					do_hook(OS_SIGNAL_LIST, "%s",
					    my_sigstr(occurred_signals[i]));
			cur_signal = -1;
			if (!hold_over)
				cursor_to_input();
			break;
		default:
			term_check_refresh();
			old_current_screen = get_current_screen();
			set_current_screen(get_last_input_screen());
			dcc_check(&rd, &wd);
			do_server(&rd, &wd);
			set_current_screen(old_current_screen);
			for (screen = screen_first(); screen;
			     screen = screen_get_next(screen))
				if (irc_do_a_screen(screen, &rd))
					irc_io_loop = 0;
			set_current_screen(old_current_screen);
			if (irc_io_loop)
				do_processes(&rd);
			break;
		}
		if (!irc_io_loop)
			break;
		execute_timer();
		check_process_limits();
		while (check_wait_status(-1) >= 0)
			;
		if (get_primary_server() == -1 && !never_connected())
			do_hook(DISCONNECT_LIST, "%s", nickname);
		timeptr = &clock_timeout;

		old_current_screen = get_current_screen();
		for (screen = screen_first(); screen; screen = screen_get_next(screen))
		{
			set_current_screen(screen);
			if (screen_get_alive(screen) && is_cursor_in_display())
				timeptr = &cursor_timeout;
		}
		set_current_screen(old_current_screen);

		if (update_clock(0, 0, UPDATE_TIME))
		{
			Debug(DB_IRCIO, "update_clock(0,0,0) returned true; updating clock");
			if (get_int_var(CLOCK_VAR) || check_mail_status())
			{
				status_update(1);
				cursor_to_input();
			}
			if (get_primary_server() != -1)
				do_notify();
		}
	}
	while (irc_io_loop);

	update_input(UPDATE_ALL);
	return;
}

int
main(int, char *[], char *[]);

int
main(int argc, char *argv[], char *envp[])
{
	u_char	*channel;

	progname = UP(argv[0]);
	srandom(time(NULL) ^ getpid());	/* something */

	start_time = time(NULL);
#ifdef	SOCKS
	SOCKSinit(argv[0]);
#endif /* SOCKS */
	channel = parse_args((u_char **) argv, argc);
	if (gethostname(CP(hostname), NAME_LEN))
	{
		fprintf(stderr, "irc: couldn't figure out the name of your machine!\n");
		exit(1);
	}
	term_set_fp(stdout);
	if (term_basic())
		new_window();
	else
	{
		init_screen();

#if defined(SIGCONT)
		(void) MY_SIGNAL(SIGCONT, (sigfunc *) term_cont, 0);
#endif

#if defined(SIGWINCH)
		(void) MY_SIGNAL(SIGWINCH, (sigfunc *) sig_refresh_screen, 0);
#endif

		(void) MY_SIGNAL(SIGSEGV, (sigfunc *) SIG_DFL, 0);
		(void) MY_SIGNAL(SIGBUS, (sigfunc *) SIG_DFL, 0);
		(void) MY_SIGNAL(SIGHUP, (sigfunc *) irc_exit, 0);
		(void) MY_SIGNAL(SIGTERM, (sigfunc *) irc_exit, 0);
		(void) MY_SIGNAL(SIGPIPE, (sigfunc *) sig_on, 0);
		(void) MY_SIGNAL(SIGINT, (sigfunc *) cntl_c, 0);
#ifdef SIGSTOP
		(void) MY_SIGNAL(SIGSTOP, (sigfunc *) sig_on, 0);
#endif /* SIGSTOP */

		(void) MY_SIGNAL(SIGUSR1, (sigfunc *) sig_user1, 0);
#ifdef DO_USER2
		(void) MY_SIGNAL(SIGUSR2, (sigfunc *) sig_user2, 0);
#else
		(void) MY_SIGNAL(SIGUSR2, (sigfunc *) sig_on, 0);
#endif /* DO_USER2 */

#ifdef SIGPWR
		(void) MY_SIGNAL(SIGPWR, (sigfunc *) sig_on, 0);
#endif
#ifdef SIGCHLD
		(void) MY_SIGNAL(SIGCHLD, (sigfunc *) sig_on, 0);
#endif
#ifdef SIGURG
		(void) MY_SIGNAL(SIGURG, (sigfunc *) sig_on, 0);
#endif
#ifdef SIGTRAP
		(void) MY_SIGNAL(SIGTRAP, (sigfunc *) sig_on, 0);
#endif
#ifdef SIGTTIN
		(void) MY_SIGNAL(SIGTTIN, (sigfunc *) sig_on, 0);
#endif
#ifdef SIGTTOU
		(void) MY_SIGNAL(SIGTTOU, (sigfunc *) sig_on, 0);
#endif
#ifdef SIGXCPU
		(void) MY_SIGNAL(SIGXCPU, (sigfunc *) sig_on, 0);
#endif
#ifdef SIGXFSZ
		(void) MY_SIGNAL(SIGXFSZ, (sigfunc *) sig_on, 0);
#endif
#ifdef SIGPOLL
		(void) MY_SIGNAL(SIGPOLL, (sigfunc *) sig_on, 0);
#endif
#ifdef SIGINFO
		(void) MY_SIGNAL(SIGINFO, (sigfunc *) sig_on, 0);
#endif
#ifdef SIGWAITING
		(void) MY_SIGNAL(SIGWAITING, (sigfunc *) sig_on, 0);
#endif
#ifdef SIGLWP
		(void) MY_SIGNAL(SIGLWP, (sigfunc *) sig_on, 0);
#endif
#ifdef SIGSYS
		(void) MY_SIGNAL(SIGSYS, (sigfunc *) sig_on, 0);
#endif
#ifdef SIGIO
		(void) MY_SIGNAL(SIGIO, (sigfunc *) sig_on, 0);
#endif
		/* More signals could probably be added, perhaps some
		   should be removed */
	}

	init_variables();

	if (!term_basic())
	{
		build_status(NULL);
		update_input(UPDATE_ALL);
	}

#ifdef MOTD_FILE
	{
		struct	stat	motd_stat,
				my_stat;
		u_char	*motd = NULL;
		int	des;

		malloc_strcpy(&motd, irc_lib);
		malloc_strcat(&motd, UP(MOTD_FILE));
		if (stat(CP(motd), &motd_stat) == 0)
		{
			u_char	*s = NULL;

			malloc_strcpy(&s, my_path_dir);
			malloc_strcat(&s, UP("/.ircmotd"));
			if (stat(CP(s), &my_stat))
			{
				my_stat.st_atime = 0L;
				my_stat.st_mtime = 0L;
			}
			unlink(CP(s));
			if ((des = open(CP(s), O_CREAT, S_IREAD | S_IWRITE))
					!= -1)
				new_close(des);
			new_free(&s);
			if (motd_stat.st_mtime > my_stat.st_mtime)
			{
				put_file(motd);
				/*
				 * Thanks to Mark Dame <mdame@uceng.ec.edu>
				 * for this one
				 */
				input_pause();
				clear_window_by_refnum(0);
			}
		}
		new_free(&motd);
	}
#endif /* MOTD_FILE */

	if (bflag)
	{
		load_global();
		load_ircrc();
	}

	get_connected(0);
	if (channel)
	{
		if (server_get_version(get_primary_server()) == ServerICB)
			server_set_icbgroup(get_primary_server(), channel);
		else
		{
			u_char    *ptr;

			ptr = my_strsep(&channel, UP(","));
			if (is_channel(ptr))
				add_channel(ptr, 0, 0, CHAN_LIMBO, NULL);
			while ((ptr = my_strsep(&channel, UP(","))) != NULL)
				if (is_channel(ptr))
					add_channel(ptr, 0, 0, CHAN_LIMBO, NULL);
			new_free(&channel);
		}
	}
	current_idle_time = time(0);
	set_input(empty_string());
	irc_io();
	irc_exit();
	return 0;
}

/* 
 * set_irchost: This sets the source host for subsequent connections.
 */
void
set_irchost(u_char *irchost)
{
	if (!irchost || !*irchost)
		new_free(&source_host);
	else
		malloc_strcpy(&source_host, irchost);
}

u_char *
get_irchost(void)
{
	return source_host;
}

int
irc_port(void)
{
	return irc_port_local;
}

int
using_ircio(void)
{
	return using_server_process;
}

u_char *
program_name(void)
{
	return progname;
}

u_char *
irc_version(void)
{
	return irc_version_str;
}

time_t
idle_time(void)
{
	return current_idle_time;
}

int
client_default_is_icb(void)
{
	return client_default_icb;
}

int
use_flow_control(void)
{
	return fflag;
}

int
use_termcap_enterexit(void)
{
	return tflag;
}

int
use_background_mode(void)
{
	return bflag;
}

int
ignore_ircrc(void)
{
	return qflag;
}

u_char *
my_path(void)
{
	return my_path_dir;
}

u_char *
my_hostname(void)
{
	return hostname;
}

u_char *
my_realname(void)
{
	return realname;
}

u_char *
my_username(void)
{
	return username;
}

void
set_realname(u_char *value)
{
	if (value)
		strmcpy(realname, value, REALNAME_LEN);
	else
		*realname = '\0';
}

void
set_username(u_char *value)
{
	if (value)
		strmcpy(username, value, REALNAME_LEN);
	else
		*username = '\0';
}

u_char *
my_irc_lib(void)
{
	return irc_lib;
}

u_char *
irc_umode(void)
{
	return (send_umode && *send_umode) ? send_umode : hostname;
}

u_char *
irc_extra_args(void)
{
	return args_str;
}

int
term_basic(void)
{
	return no_full_screen;
}

u_char *
my_nickname(void)
{
	return nickname;
}

void
set_nickname(u_char *value)
{
	if (value)
		malloc_strcpy(&nickname, value);
	else
		new_free(&nickname);
}

u_char *
ircquick_file_path(void)
{
	return ircquick_file;
}

u_char *
ircrc_file_path(void)
{
	return ircrc_file;
}

void
set_ircrc_file_path(u_char *path)
{
	if (path)
		malloc_strcpy(&ircrc_file, path);
	else
		new_free(&ircrc_file);
}

u_char *
zero(void)
{
	static u_char z[] = { '0', '\0' };
	return z;
}

u_char *
one(void)
{
	static u_char o[] = { '1', '\0' };
	return o;
}

u_char *
empty_string(void)
{
	static u_char e[] = { '\0' };
	return e;
}
