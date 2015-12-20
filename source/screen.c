/*
 * screen.c
 *
 * Written By Matthew Green, based on portions of window.c
 * by Michael Sandrof.
 *
 * Copyright (c) 1990 Michael Sandrof.
 * Copyright (c) 1993-2014 Matthew R. Green.
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
IRCII_RCSID("@(#)$eterna: screen.c,v 1.205 2015/11/20 09:23:54 mrg Exp $");

#ifdef HAVE_SYS_UN_H
# include <sys/un.h>
#endif /* HAVE_SYS_UN_H */

#ifdef HAVE_ICONV_H
# include <iconv.h>
#endif /* HAVE ICONV_H */

#include "screen.h"
#include "menu.h"
#include "window.h"
#include "output.h"
#include "vars.h"
#include "server.h"
#include "list.h"
#include "ircterm.h"
#include "names.h"
#include "ircaux.h"
#include "input.h"
#include "log.h"
#include "hook.h"
#include "dcc.h"
#include "translat.h"
#include "exec.h"
#include "newio.h"
#include "parse.h"
#include "edit.h"
#include "strsep.h"

#ifdef lines
#undef lines
#undef columns
#endif /* lines */

struct	screen_stru
{
	int	screennum;
	Window	*current_window;
	unsigned int	last_window_refnum;	/* reference number of the
						   window that was last
						   the current_window */
	Window	*window_list;			/* List of all visible
						   windows */
	Window	*window_list_end;		/* end of visible window
						   list */
	Window	*cursor_window;			/* Last window to have
						   something written to it */
	int	visible_windows;		/* total number of windows */
	WindowStack *window_stack;		/* the windows here */

	EditInfo *edit_info;			/* our edit info */

	Screen *next;				/* the Screen list */

	int	fdin;				/* These are the file */
	FILE	*fpout;				/* pointers and descriptions */
	int	fdout;				/* input/output */
	int	wserv_fd;			/* control socket for wserv */

	Prompt	*promptlist;

	u_char	*redirect_name;
	u_char	*redirect_token;
	int	redirect_server;

	u_char	*tty_name;
	int	co, li;

	int	old_term_co, old_term_li;

	ScreenInputData *inputdata;

	int	alive;
};

static	int	screen_get_fdout(Screen *);
static	void	screen_set_fdin(Screen *, int);
static	void	screen_set_fpout(Screen *, FILE *);
static	void	screen_set_wserv_fd(Screen *, int);
static	void	screen_set_fdout(Screen *, int);

/* are we currently processing a redirect command? */
static	int	current_in_redirect;

static	Window	*to_window;
static	Screen	*current_screen;
static	Screen	*main_screen;
static	Screen	*last_input_screen;

/* Our list of screens */
static	Screen	*screen_list = NULL;

/*
 * create_new_screen creates a new screen structure. with the help of
 * this structure we maintain ircII windows that cross screen window
 * boundaries.
 */
Screen	*
create_new_screen(void)
{
	Screen	*new = NULL,
		**list;
	static	int	refnumber = 0;

	for (list = &screen_list; *list; list = &((*list)->next))
	{
		if (!screen_get_alive(*list))
		{
			new = *list;
			break;
		}
	}
	if (!new)
	{
		new = new_malloc(sizeof *new);
		new->inputdata = NULL;
		new->screennum = ++refnumber;
		new->next = screen_list;
		screen_list = new;
	}
	new->last_window_refnum = 1;
	new->window_list = NULL;
	new->window_list_end = NULL;
	new->cursor_window = NULL;
	new->current_window = NULL;
	new->visible_windows = 0;
	new->window_stack = NULL;
	new->edit_info = alloc_edit_info();
	screen_set_fdout(new, 1);
	screen_set_fpout(new, stdout);
	screen_set_fdin(new, 0);
	new->alive = 1;
	new->promptlist = NULL;
	new->redirect_name = NULL;
	new->redirect_token = NULL;
	new->tty_name = NULL;
	new->li = main_screen ? main_screen->li : 24;
	new->co = main_screen ? main_screen->co : 79;
	new->old_term_li = -1;
	new->old_term_co = -1;

	input_reset_screen(new);

	new->redirect_server = -1;
	last_input_screen = new;
	return new;
}

void
set_current_screen(Screen *screen)
{
	if (screen_get_alive(screen))
		current_screen = screen;
	else
		current_screen = screen_list;
	term_set_fp(screen_get_fpout(current_screen));
}

Screen *
get_current_screen(void)
{
	return current_screen;
}

/*
 * window_redirect: Setting who to non null will cause IRCII to echo all
 * commands and output from that server (including output generated by IRCII)
 * to who.  Setting who to null disables this
 *
 * XXXMRG why does this operate on the whole screen instead of a window?
 */
void
window_redirect(u_char *who, int server)
{
	u_char	buf[BIG_BUFFER_SIZE];

	if (who)
		snprintf(CP(buf), sizeof buf, "%04d%s", server, who);
	else
		snprintf(CP(buf), sizeof buf, "%04d#LAME", server);
	malloc_strcpy(&current_screen->redirect_token, buf);
	malloc_strcpy(&current_screen->redirect_name, who);
	current_screen->redirect_server = server;
}

int
check_screen_redirect(u_char *nick)
{
	Screen	*screen,
		*tmp_screen;

	for (screen = screen_first(); screen; screen = screen->next)
	{
		if (!screen->redirect_token)
			continue;
		if (!my_strcmp(nick, screen->redirect_token))
		{
			tmp_screen = current_screen;
			set_current_screen(screen);
			window_redirect(NULL, get_from_server());
			set_current_screen(tmp_screen);
			return 1;
		}
	}
	return 0;
}

/* Old screens never die. They just fade away. */

#ifdef WINDOW_CREATE
void
kill_screen(Screen *screen)
{
	Window	*window;

	if (main_screen == screen)
	{
		say("You may not kill the main screen");
		return;
	}
	if (screen_get_fdin(screen))
	{
		new_close(screen_get_fdin(screen));
		new_close(screen_get_fdout(screen));
		/* But fdout is attached to fpout, so close that too.. */
		fclose(screen_get_fpout(screen));
	}
	while ((window = screen_get_window_list(screen)))
	{
		screen->window_list = window_get_next(window);
		add_to_invisible_list(window);
	}

	if (last_input_screen == screen)
		last_input_screen = screen_first();
	screen->alive = 0;
}

int
is_main_screen(Screen *screen)
{
	return (screen == main_screen);
}

#else

int
is_main_screen(Screen *screen)
{
	return 1;
}

#endif /* WINDOW_CREATE */


/*
 * cursor_not_in_display: This forces the cursor out of the display by
 * setting the cursor window to null.  This doesn't actually change the
 * physical position of the cursor, but it will force rite() to reset the
 * cursor upon its next call
 */
void
cursor_not_in_display(void)
{
	Debug(DB_CURSOR, "screen %d cursor_window set to NULL",
	      screen_get_screennum(current_screen));
	set_cursor_window(NULL);
}

/*
 * cursor_in_display: this forces the cursor_window to be the
 * current_screen->current_window.
 * It is actually only used in hold.c to trick the system into thinking the
 * cursor is in a window, thus letting the input updating routines move the
 * cursor down to the input line.  Dumb dumb dumb
 */
void
cursor_in_display(void)
{
	Debug(DB_CURSOR, "screen %d cursor_window set to window %d (%d)",
	       screen_get_screennum(current_screen), window_get_refnum(curr_scr_win),
	       screen_get_screennum(window_get_screen(curr_scr_win)));
	set_cursor_window(curr_scr_win);
}

/*
 * is_cursor_in_display: returns true if the cursor is in one of the windows
 * (cursor_window is not null), false otherwise
 */
int
is_cursor_in_display(void)
{
	if  (get_cursor_window())
		return (1);
	else
		return (0);
}

void
decode_colour(u_char **ptr, int *fg, int *bg)
{
	char c;
	/* At input, *ptr points to the first char after ^C */

	*fg = *bg = 16; /* both unset */

	/*
	 * mIRC doesn't accept codes beginning with comma (,),
	 * VIRC accepts. mIRC is our authority in this case.
	 */
	c = **ptr;
	if (c < '0' || c > '9')
	{
		--*ptr;
		return;
	}

	*fg = c - '0';

	c = (*ptr)[1];
	if (c >= '0' && c <= '9')
	{
		++*ptr;
		*fg = *fg * 10 + c - '0';
		c = (*ptr)[1];
	}
	if (c == ',')
	{
		/*
		 * mIRC doesn't eat the comma if it is not followed by
		 * a number, VIRC does. mIRC is again the authority.
		 */
		c = (*ptr)[2];
		if (c >= '0' && c <= '9')
		{
			*bg = c - '0';
			*ptr += 2;
			c = (*ptr)[1];
			if (c >= '0' && c <= '9')
			{
				++*ptr;
				*bg = *bg * 10 + c - '0';
			}
		}
	}
	/* At this point, *ptr points to last char WITHIN the colour code */
}

/* strlen() with internal format codes stripped */
/* Return value: number of columns this string takes */
int
my_strlen_i(u_char *c)
{
	int result = 0;
	struct mb_data mbdata;
#ifdef HAVE_ICONV_OPEN
	mbdata_init(&mbdata, CP(current_irc_encoding()));
#else
	mbdata_init(&mbdata, NULL);
#endif /* HAVE_ICONV_OPEN */

	while (*c)
	{
		if (*c == REV_TOG || *c == UND_TOG || *c == BOLD_TOG || *c == ALL_OFF)
		{
			/* These don't add visual length */
			++c;
			continue;
		}
		if (*c == COLOUR_TAG)
		{
			/* Colour code didn't add visual length */
			c += 3;
			continue;
		}

		/* Anything else is character data. */
		decode_mb(c, NULL, 0, &mbdata);
		result += mbdata.num_columns;
		c      += mbdata.input_bytes;
	}

	mbdata_done(&mbdata);
	return result;
}

/* strlen() with colour codes stripped */
/* Return value: Number of columns this string takes */
int
my_strlen_c(u_char *c)
{
	int result = 0;
	struct mb_data mbdata;
#ifdef HAVE_ICONV_OPEN
	mbdata_init(&mbdata, CP(current_irc_encoding()));
#else
	mbdata_init(&mbdata, NULL);
#endif /* HAVE_ICONV_OPEN */

	while (*c)
	{
		if (*c == REV_TOG || *c == UND_TOG || *c == BOLD_TOG || *c == ALL_OFF)
		{
			/* These don't add visual length */
			++c;
			continue;
		}
		if (*c == COLOUR_TAG)
		{
			int fg, bg;

			++c;
			decode_colour(&c, &fg, &bg);
			++c;
			/* Colour code didn't add visual length */
			continue;
		}

		/* Anything else is character data. */
		decode_mb(c, NULL, 0, &mbdata);
		result += mbdata.num_columns;
		c      += mbdata.input_bytes;
	}
	mbdata_done(&mbdata);
	return result;
}

/* strlen() with colour codes converted to internal codes */
/* Doesn't do actual conversion */
/* Return value: Number of bytes this string takes when converted */
int
my_strlen_ci(u_char *c)
{
	int result = 0;

	struct mb_data mbdata;
#ifdef HAVE_ICONV_OPEN
	mbdata_init(&mbdata, CP(current_irc_encoding()));
#else
	mbdata_init(&mbdata, NULL);
#endif /* HAVE_ICONV_OPEN */

	while (*c)
	{
		if (*c == REV_TOG || *c == UND_TOG || *c == BOLD_TOG || *c == ALL_OFF)
		{
			/* These are preserved */
			++c;
			++result;
			continue;
		}
		if (*c == COLOUR_TAG)
		{
			int fg, bg;

			++c;
			decode_colour(&c, &fg, &bg);
			++c;
			/* Modifies into three characters */
			result += 3;
			continue;
		}

		/* Anything else is character data. */
		decode_mb(c, NULL, 0, &mbdata);
		result += mbdata.output_bytes;
		c      += mbdata.input_bytes;
	}
	mbdata_done(&mbdata);
	return result;
}

/* strcpy() with colour codes converted to internal codes */
/* Converts the codes */
void
my_strcpy_ci(u_char *dest, size_t destlen, u_char *c)
{
	struct mb_data mbdata;
#ifdef HAVE_ICONV_OPEN
	mbdata_init(&mbdata, CP(current_irc_encoding()));
#else
	mbdata_init(&mbdata, NULL);
#endif /* HAVE_ICONV_OPEN */

	while (*c && destlen > 1)
	{
		if (*c == REV_TOG || *c == UND_TOG || *c == BOLD_TOG || *c == ALL_OFF)
		{
			*dest++ = *c++;
			destlen--;
			continue;
		}
		if (*c == COLOUR_TAG)
		{
			int fg, bg;

			if (destlen <= 3)
				break;
			*dest++ = *c++;
			decode_colour(&c, &fg, &bg);
			*dest++ = fg+1;
			*dest++ = bg+1;
			++c;
			destlen -= 3;
			continue;
		}

		/* Anything else is character data. */
		decode_mb(c, dest, destlen, &mbdata);

		dest += mbdata.output_bytes;
		destlen -= mbdata.output_bytes;
		c += mbdata.input_bytes;
	}
	mbdata_done(&mbdata);
	*dest++ = '\0';
}

int
in_redirect(void)
{
	return current_in_redirect;
}

/*
 * add_to_screen: This adds the given null terminated buffer to the screen.
 * That is, it determines which window the information should go to, which
 * lastlog the information should be added to, which log the information
 * should be sent to, etc.
 *
 * All logging to files is performed here as well.
 */
void
add_to_screen(u_char *incoming)
{
	Win_Trav wt;
	Window	*tmp;
	u_char	buffer[BIG_BUFFER_SIZE];
	u_char	*who_from = current_who_from();
	const	int	from_server = get_from_server();

	/* eek! */
	if (!current_screen)
	{
		puts(CP(incoming));
		fflush(stdout);
		add_to_log(NULL, incoming);
		return;
	}
	/* Handles output redirection first */
	if (!current_in_redirect && current_screen->redirect_name &&
	    from_server == current_screen->redirect_server)
	{
		int	i;

		/*
		 * More general stuff by Bisqwit, allow
		 * multiple target redirection (with different targets)
		 */
		current_in_redirect = 1;
		i = set_in_on_who(0); /* To allow redirecting /who, /whois and /join */
		redirect_msg(current_screen->redirect_name, incoming);
		set_in_on_who(i);
		current_in_redirect = 0;
	}
	if (term_basic())
	{
		/* FIXME: Do iconv for "incoming" in dumb mode too */
		(void*)add_to_lastlog(curr_scr_win, incoming);
		if (do_hook(WINDOW_LIST, "%u %s",
		    window_get_refnum(curr_scr_win), incoming))
		{
			add_to_log(NULL, incoming);
			puts(CP(incoming));
		}
		term_flush();
		return;
	}
	if (current_who_level() == LOG_CURRENT &&
	    window_get_server(curr_scr_win) == from_server)
	{
		add_to_window(curr_scr_win, incoming);
		return;
	}
	if (to_window)
	{
		add_to_window(to_window, incoming);
		return;
	}
	/*
	 * XXX - hack alert
	 * If /echo or /xecho set the message_from, use it here before we
	 * look at who_from.  If not, look at it afterwards.
	 * XXX we could probably use to_window to fix this less hackily?
	 */
	if (current_who_level() && who_level_before_who_from())
	{
		wt.init = 1;
		while ((tmp = window_traverse(&wt)) != NULL)
		{
			if ((from_server == window_get_server(tmp) ||
			     from_server == -1) &&
			    (current_who_level() & window_get_window_level(tmp)))
			{
				add_to_window(tmp, incoming);
				return;
			}
		}
	}
	if (who_from)
	{
		if (is_channel(who_from))
		{
			Window	*window;

			if ((window = channel_window(who_from, from_server)) != NULL)
			{
				add_to_window(window, incoming);
				return;
			}
			if (current_who_level() == LOG_DCC)
			{
				my_strcpy(buffer, "=");
				my_strmcat(buffer, who_from, sizeof buffer);
				if ((window = channel_window(buffer, from_server)) != NULL)
				{
					add_to_window(window, incoming);
					return;
				}
			}
		}
		else
		{
			wt.init = 1;
			while ((tmp = window_traverse(&wt)) != NULL)
			{
				u_char *nick = window_get_query_nick(tmp);

				if (nick &&
				    (((current_who_level() == LOG_MSG ||
				       current_who_level() == LOG_NOTICE) &&
				      my_stricmp(who_from, nick) == 0 &&
				      from_server == window_get_server(tmp)) ||
				     (current_who_level() == LOG_DCC &&
				      (*nick == '=' || *nick == '@') &&
				      my_stricmp(who_from, nick + 1) == 0)))
				{
					add_to_window(tmp, incoming);
					return;
				}
			}
			wt.init = 1;
			while ((tmp = window_traverse(&wt)) != NULL)
			{
				if (from_server == window_get_server(tmp))
				{
					if (window_has_who_from(tmp))
					{
						add_to_window(tmp, incoming);
						return;
					}
				}
			}
		}
	}
	if (current_who_level() && !who_level_before_who_from())
	{
		wt.init = 1;
		while ((tmp = window_traverse(&wt)) != NULL)
		{
			if ((from_server == window_get_server(tmp) ||
			     from_server == -1) &&
			    (current_who_level() & window_get_window_level(tmp)))
			{
				add_to_window(tmp, incoming);
				return;
			}
		}
	}
	if (from_server == window_get_server(curr_scr_win))
		tmp = curr_scr_win;
	else
	{
		wt.init = 1;
		while ((tmp = window_traverse(&wt)) != NULL)
		{
			if (window_get_server(tmp) == from_server)
				break;
		}
		if (!tmp)
			tmp = curr_scr_win;
	}
	add_to_window(tmp, incoming);
}

void
close_all_screen(void)
{
	Screen *screen;

	for (screen = screen_first(); screen && screen != current_screen;
			screen = screen->next)
		if (screen_get_alive(screen) && screen_get_fdin(screen) != 0)
			new_close(screen_get_fdin(screen));
}

#ifdef WINDOW_CREATE
Window	*
create_additional_screen(int wanted_type)
{
	Window	*win;
	Screen	*oldscreen;
	u_char	*displayvar,
		*termvar;
	int	screen_type = ST_NOTHING;
	struct	sockaddr_un sock, error_sock,
			*sockaddr = &sock,
			*error_sockaddr = &error_sock,
			NewSock;
	socklen_t	NsZ;
	int	s, es;
	int	fd;
	FILE	*fp;
	int	wserv_fd;
	fd_set	fd_read;
	struct	timeval	time_out;
	pid_t	child;
	int	old_timeout;
#define IRCXTERM_MAX 10
	u_char	*ircxterm[IRCXTERM_MAX];
	u_char	*ircxterm_env;
	u_char	*xterm = NULL;
	u_char	*def_xterm = get_string_var(XTERM_PATH_VAR);
	u_char	*p, *q;
	u_char	buffer[BIG_BUFFER_SIZE];
	pid_t	pid = getpid();
	int	ircxterm_num;
	int	i;
	static int cycle = 0;
	int	mycycle = cycle++;

#ifdef DAEMON_UID
	if (DAEMON_UID == getuid())
	{
		say("you are not permitted to use WINDOW CREATE");
		return NULL;
	}
#endif /* DAEMON_UID */

	if (!def_xterm)
		def_xterm = UP("xterm");

	ircxterm_num = 0;
	p = ircxterm_env = my_getenv("IRCXTERM");
	if (p)
	{
		q = ircxterm_env + my_strlen(ircxterm_env);
		while (p < q && ircxterm_num < IRCXTERM_MAX)
		{
			while (':' == *p)
				p++;
			if ('\0' == *p)
				break;
			ircxterm[ircxterm_num++] = p;
			while (':' != *p && '\0' != *p)
				p++;
			if (':' == *p)
			{
				*p = '\0';
				p++;
			}
		}
	}
	else
	{
		ircxterm[ircxterm_num] = def_xterm;
		ircxterm_num++;
	}

	/*
	 * Environment variable STY has to be set for screen to work..  so it is
	 * the best way to check screen..  regardless of what TERM is, the
	 * execpl() for screen won't just open a new window if STY isn't set,
	 * it will open a new screen process, and run the wserv in its first
	 * window, not what we want...  -phone
	 */
	if ((wanted_type == ST_NOTHING || wanted_type == ST_SCREEN) &&
	    0 != my_getenv("STY"))
		screen_type = ST_SCREEN;
	else if ((wanted_type == ST_NOTHING || wanted_type == ST_XTERM) &&
	    NULL != (displayvar = my_getenv("DISPLAY")) &&
	    NULL != (termvar = my_getenv("TERM")))
	{
		screen_type = ST_XTERM;
		if (0 == my_strncmp(termvar, "sun", 3))
		{
			xterm = def_xterm;
		}
		else
		{
			for (; *termvar; termvar++)
			{
				for (i = 0; i < ircxterm_num; i++)
				{
					if (!my_strncmp(termvar, ircxterm[i], my_strlen(ircxterm[i])))
					{
						xterm = ircxterm[i];
						termvar = empty_string();
						break;
					}
				}
			}
		}
		if (!xterm)
			xterm = def_xterm;
	}

	if (screen_type == ST_NOTHING)
	{
		say("I don't know how to create new windows for this terminal");
		return NULL;
	}
	say("Opening new %s...",
		screen_type == ST_XTERM ?  "window" :
		screen_type == ST_SCREEN ? "screen" :
					   "wound" );
	snprintf(sock.sun_path, sizeof sock.sun_path, "/tmp/irc_%08d_%x", (int) pid, mycycle);
	sock.sun_family = AF_UNIX;
	if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
	{
		say("Can't create UNIX socket, punting /WINDOW CREATE");
		return NULL;
	}
	if (bind(s, (struct sockaddr *) &sock, (int)(2 + my_strlen(sock.sun_path))) < 0)
	{
		say("Can't bind UNIX socket, punting /WINDOW CREATE");
		return NULL;
	}
	if (listen(s, 1) < 0)
	{
		say("Can't bind UNIX socket, punting /WINDOW CREATE");
		return NULL;
	}
	snprintf(error_sock.sun_path, sizeof error_sock.sun_path, "/tmp/irc_error_%08d_%x", (int) pid, mycycle);
	error_sock.sun_family = AF_UNIX;
	if ((es = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
	{
		say("Can't create UNIX socket, punting /WINDOW CREATE");
		return NULL;
	}
	if (bind(es, (struct sockaddr *) &error_sock, (int)(2 + my_strlen(error_sock.sun_path))) < 0)
	{
		say("Can't bind UNIX socket, punting /WINDOW CREATE");
		return NULL;
	}
	if (listen(es, 1) < 0)
	{
		say("Can't bind UNIX socket, punting /WINDOW CREATE");
		return NULL;
	}

	oldscreen = current_screen;
	set_current_screen(create_new_screen());
	if (0 == (child = fork()))
	{
		(void)setuid(getuid());
		(void)setgid(getgid());
	/*
	 * Unlike most other cases, it is important here to close down
	 * *ALL* unneeded file descriptors. Failure to do so can cause
	 * Things like server and DCC connections to fail to close on
	 * request. This isn't a problem with "screen", but is with X.
	 */
		new_close(s);
		new_close(es);
		close_all_screen();
		close_all_dcc();
		close_all_exec();
		close_all_server();
		i = 0;
		if (screen_type == ST_SCREEN)
		{
#define MAX_ARGS	64
			char	*args[MAX_ARGS];
			u_char	*ss,
				*t,
				*opts = NULL;

			Debug(DB_WINCREATE, "going to execvp screen wserv...");
			args[i++] = "screen";
			if ((ss = get_string_var(SCREEN_OPTIONS_VAR)) != NULL)
			{
				malloc_strcpy(&opts, ss);
				while ((t = next_arg(opts, &opts)) != NULL)
				{
					Debug(DB_WINCREATE, "added option `%s'", t);
					args[i++] = CP(t);

					/* add 6 more below */
					if (i + 6 > MAX_ARGS) {
						say("Too many args in SCREEN_OPTIONS_VAR");
						break;
					}
				}
			}
			args[i++] = CP(get_string_var(WSERV_PATH_VAR));
#if 0
			args[i++] = "-t";
			args[i++] = CP(program_name());
#endif
			args[i++] = sockaddr->sun_path;
			args[i++] = error_sockaddr->sun_path;
			Debug(DB_WINCREATE, "added: %s %s %s", args[i-3], args[i-2], args[i-1]);
			args[i++] = NULL;
			execvp("screen", args);
		}
		else if (screen_type == ST_XTERM)
		{
			int	lines,
				columns;
			char	*args[64];
			u_char	geom[20],
				*ss,
				*t,
				*opts = NULL;

			Debug(DB_WINCREATE, "going to execvp xterm wserv...");
			copy_window_size(&lines, &columns);
			snprintf(CP(geom), sizeof geom, "%dx%d", columns, lines);
			args[i++] = CP(xterm);
			Debug(DB_WINCREATE, "xterm name is %s", args[0]);
			if ((ss = get_string_var(XTERM_GEOMOPTSTR_VAR)) != NULL)
				args[i++] = CP(ss);
			else
				args[i++] = "-geom";
			args[i++] = CP(geom);
			Debug(DB_WINCREATE, "added geom: %s %s", args[i-2], args[i-1]);
			if ((ss = get_string_var(XTERM_OPTIONS_VAR)) != NULL)
			{
				malloc_strcpy(&opts, ss);
				Debug(DB_WINCREATE, "got xterm options: %s", opts);
				while ((t = next_arg(opts, &opts)) != NULL)
				{
					Debug(DB_WINCREATE, "added option `%s'", t);
					args[i++] = CP(t);
				}
			}
			args[i++] = "-e";
			args[i++] = CP(get_string_var(WSERV_PATH_VAR));
			args[i++] = sockaddr->sun_path;
			args[i++] = error_sockaddr->sun_path;
			Debug(DB_WINCREATE, "added: %s %s %s %s", args[i-4], args[i-3], args[i-2], args[i-1]);
			args[i] = NULL;
			execvp(CP(xterm), args);
		}
		perror("execve");
		unlink(sockaddr->sun_path);
		unlink(error_sockaddr->sun_path);
		exit(errno ? errno : -1);
	}
	NsZ = sizeof(NewSock);
	FD_ZERO(&fd_read);
	FD_SET(s, &fd_read);
	FD_SET(es, &fd_read);
	time_out.tv_sec = (time_t) 5;
	time_out.tv_usec = 0;
	sleep(1);

	Debug(DB_WINCREATE, "-- forked wserv!");
	/* using say(), yell() can be bad in this next section of code. */

	switch (select(NFDBITS , &fd_read, NULL, NULL, &time_out))
	{
	case -1:
	case 0:
		errno = get_child_exit(child);
		new_close(s);
		new_close(es);
		kill_screen(current_screen);
		kill(child, SIGKILL);
		last_input_screen = oldscreen;
		set_current_screen(oldscreen);
		yell("child %s with %d", (errno < 1) ? "signaled" : "exited",
					 (errno < 1) ? -errno : errno);
		win = NULL;
		break;
	default:
		fd = accept(s, (struct sockaddr *) &NewSock, &NsZ);
		if (fd < 0)
		{
			win = NULL;
			break;
		}
		wserv_fd = accept(es, (struct sockaddr *) &NewSock, &NsZ);
		if (wserv_fd < 0)
		{
			close(fd);
			win = NULL;
			break;
		}

		fp = fdopen(fd, "r+");
		if (!fp) {
			close(wserv_fd);
			close(fd);
			win = NULL;
			break;
		}

		screen_set_fdin(current_screen, fd);
		screen_set_fdout(current_screen, fd);
		screen_set_fpout(current_screen, fp);
		screen_set_wserv_fd(current_screen, wserv_fd);
		Debug(DB_WINCREATE, "-- got connected!");

		term_set_fp(fp);
		new_close(s);
		new_close(es);
		old_timeout = dgets_timeout(5);
		/*
		 * dgets returns 0 on EOF and -1 on timeout.  both of these are
		 * error conditions in this case, so we bail out here.
		 */
		if (dgets(buffer, sizeof buffer, screen_get_fdin(current_screen)) < 1)
		{
			new_close(screen_get_fdin(current_screen));
			kill_screen(current_screen);
			kill(child, SIGKILL);
			last_input_screen = oldscreen;
			set_current_screen(oldscreen);
			(void) dgets_timeout(old_timeout);
			win = NULL;
			break;
		}
		else
			malloc_strcpy(&current_screen->tty_name, buffer);
		win = new_window();
		(void) refresh_screen(0, NULL);
		set_current_screen(oldscreen);
		(void) dgets_timeout(old_timeout);
	}
	unlink(sockaddr->sun_path);
	unlink(error_sockaddr->sun_path);
	return win;
}
#endif /* WINDOW_CREATE */

void
screen_wserv_message(Screen *screen)
{
	u_char buf[128], *rs, *cs, *comma;
	int old_timeout, li, co;

	old_timeout = dgets_timeout(0);
	if (dgets(buf, 128, current_screen->wserv_fd) < 1)
	{
		/* this should be impossible. */
		if (!is_main_screen(screen))
			kill_screen(screen);
	}
	(void) dgets_timeout(old_timeout);
	Debug(DB_WINCREATE, "got buf: %s", buf);
	/* should have "rows,cols\n" */
	rs = buf;
	comma = my_index(buf, ',');
	if (comma == NULL)
	{
		yell("--- got wserv message: %s, no comma?", buf);
		return;
	}
	*comma = 0;
	cs = comma+1;
	comma = my_index(buf, '\n');
	if (comma != NULL)
		*comma = 0;

	li = atoi(CP(rs));
	co = atoi(CP(cs));

	Debug(DB_WINCREATE, "got li %d[%d] co %d[%d]", li, screen->li, co, screen->co);

	if (screen->li != li || screen->co != co)
	{
		Screen *old_screen;

		screen->li = li;
		screen->co = co;
		old_screen = current_screen;
		current_screen = screen;
		refresh_screen(0, 0);
		current_screen = old_screen;
	}
}

int
screen_get_screennum(Screen *screen)
{
	return screen->screennum;
}

Window *
screen_get_current_window(Screen *screen)
{
	return screen->current_window;
}

void
screen_set_current_window(Screen *screen, Window *window)
{
	screen->current_window = window;
}

ScreenInputData *
screen_get_inputdata(Screen *screen)
{
	return screen->inputdata;
}

void
screen_set_inputdata(Screen *screen, ScreenInputData *inputdata)
{
	screen->inputdata = inputdata;
}

int
screen_get_alive(Screen *screen)
{
	return screen->alive;
}

u_char *
get_redirect_token(void)
{
	return current_screen->redirect_token;
}

int
screen_get_wserv_fd(Screen *screen)
{
	return screen->wserv_fd;
}

Window *
screen_get_window_list(Screen *screen)
{
	return screen->window_list;
}

void
screen_set_window_list(Screen *screen, Window *window)
{
	screen->window_list = window;
}

Window *
screen_get_window_list_end(Screen *screen)
{
	return screen->window_list_end;
}

void
screen_set_window_list_end(Screen *screen, Window *window)
{
	screen->window_list_end = window;
}

int
screen_get_visible_windows(Screen *screen)
{
	return screen->visible_windows;
}

void
incr_visible_windows(void)
{
	current_screen->visible_windows++;
}

void
decr_visible_windows(void)
{
	current_screen->visible_windows--;
}

int
screen_get_fdin(Screen *screen)
{
	return screen->fdin;
}

static int
screen_get_fdout(Screen *screen)
{
	return screen->fdout;
}

static void
screen_set_fdout(Screen *screen, int fdout)
{
	screen->fdout = fdout;
}

static void
screen_set_fdin(Screen *screen, int fdin)
{
	screen->fdin = fdin;
}

FILE *
screen_get_fpout(Screen *screen)
{
	return screen->fpout;
}

static void
screen_set_fpout(Screen *screen, FILE *fpout)
{
	screen->fpout = fpout;
}

static void
screen_set_wserv_fd(Screen *screen, int wserv_fd)
{
	screen->wserv_fd = wserv_fd;
}

unsigned
get_last_window_refnum(void)
{
	return current_screen->last_window_refnum;
}

void
set_last_window_refnum(unsigned refnum)
{
	current_screen->last_window_refnum = refnum;
}

Prompt *
get_current_promptlist(void)
{
	return current_screen->promptlist;
}

void
set_current_promptlist(Prompt *promptlist)
{
	current_screen->promptlist = promptlist;
}

Screen *
screen_get_next(Screen *screen)
{
	return screen->next;
}

WindowStack *
get_window_stack(void)
{
	return current_screen->window_stack;
}

void
set_window_stack(WindowStack *window_stack)
{
	current_screen->window_stack = window_stack;
}

Window *
get_cursor_window(void)
{
	return current_screen->cursor_window;
}

void
set_cursor_window(Window *cursor_window)
{
	current_screen->cursor_window = cursor_window;
}

EditInfo *
get_edit_info(void)
{
	return current_screen->edit_info;
}

int
screen_set_size(int new_li, int new_co)
{
	int rv = 0;

	if (current_screen->old_term_li != new_li ||
	    current_screen->old_term_co != new_co)
		rv = 1;

	current_screen->old_term_li = current_screen->li;
	current_screen->old_term_co = current_screen->co;
	current_screen->li = new_li;
	current_screen->co = new_co;

	Debug(DB_WINDOW, "new lines/colums = %d/%d (old = %d/%d)",
		new_li, new_co,
		current_screen->old_term_li,
		current_screen->old_term_co);
	return rv;
}

int
screen_get_old_co(void)
{
	return current_screen->old_term_co;
}

int
screen_get_old_li(void)
{
	return current_screen->old_term_li;
}

int
get_co(void)
{
	return current_screen->co;
}

int
get_li(void)
{
	return current_screen->li;
}

Screen *
screen_first(void)
{
	return screen_list;
}

void
set_main_screen(Screen *screen)
{
	main_screen = screen;
}

Screen *
get_last_input_screen(void)
{
	return last_input_screen;
}

void
set_last_input_screen(Screen *screen)
{
	last_input_screen = screen;
}

Window *
get_to_window(void)
{
	return to_window;
}

void
set_to_window(Window *window)
{
	to_window = window;
}
