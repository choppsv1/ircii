/*
 * window.c: Handles the Main Window stuff for irc.  This includes proper
 * scrolling, saving of screen memory, refreshing, clearing, etc. 
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
IRCII_RCSID("@(#)$eterna: window.c,v 1.212 2015/09/05 07:37:40 mrg Exp $");

#include "screen.h"
#include "menu.h"
#include "window.h"
#include "vars.h"
#include "server.h"
#include "list.h"
#include "ircterm.h"
#include "names.h"
#include "ircaux.h"
#include "input.h"
#include "status.h"
#include "output.h"
#include "log.h"
#include "hook.h"
#include "dcc.h"
#include "translat.h"
#include "icb.h"
#include "parse.h"
#include "strsep.h"

/*
 * WindowStack: The structure for the POP, PUSH, and STACK functions. A
 * simple linked list with window refnums as the data.
 *
 * The code for this probably should move into screen.c.
 */
struct	window_stack_stru
{
	unsigned int	refnum;
	WindowStack *next;
};

struct	display_stru
{
	u_char	*line;
	int	linetype;
	Display	*next;
};

/*
 * The main Window structure.
 */
struct window_stru
{
	unsigned refnum;		/* the unique reference number,
					   assigned by IRCII */
	u_char	*name;			/* window's logical name */
	int	server;			/* server index */
	int	prev_server;		/* previous server index */

	int	top;			/* The top line of the window, screen
					   coordinates */
	int	bottom;			/* The botton line of the window, screen
					   coordinates */
	int	cursor;			/* The cursor position in the window,
					   window relative coordinates */
	int	line_cnt;		/* counter of number of lines displayed
					   in window */
	int	scroll;			/* true, window scrolls... false window
					   wraps */
	int	display_size;		/* number of lines of window - menu
					   lines */
	int	old_size;		/* if new_size != display_size,
					   resize_display is called */
	int	visible;		/* true, window is drawn... false window
					   is hidden */
	int	update;			/* window needs updating flag */
	unsigned miscflags;		/* Miscellaneous flags. */

	u_char	*prompt;		/* A prompt string, usually set by
					   EXEC'd process */
	u_char	*status_line[2];	/* The status line strings */

	int	double_status;		/* Display the 2nd status line ?*/

	Display	*top_of_display,	/* Pointer to first line of display
					   structure */
		*display_ip;		/* Pointer to insertiong point of
					   display structure */

	/* hold stuff */
	HoldInfo *hold_info;		/* link to our hold info */
	int	hold_mode;		/* true, hold mode is on for window */
	int	hold_on_next_rite;	/* if true, activate a hold on next call
					   to rite() */

	int	scrolled_lines;		/* number of lines scrolled back */
	int	new_scrolled_lines;	/* number of lines since scroll back
					   keys where pressed */

	/* stuff that is a property of this window */
	u_char	*current_channel;	/* Window's current channel */
	u_char	*bound_channel;		/* Channel that belongs in this window */
	u_char	*query_nick;		/* User being QUERY'ied in this window */
	NickList *nicks;		/* List of nicks that will go to window */
	int	window_level;		/* The LEVEL of the window, determines
					   what messages go to it */
	WindowMenu	*menu;		/* The menu (if any) */

	/* lastlog stuff */
	LastlogInfo	*lastlog_info;	/* pointer to lastlog info */

	int	notify_level;		/* the notify level.. */

	/* window log stuff */
	u_char	*logfile;		/* window's logfile name */
	int	log;			/* file logging for window is on */
	FILE	*log_fp;		/* file pointer for the log file */

	Screen	*screen;		/* our currently attached screen */
	int	server_group;		/* server group number */

	Window	*next;			/* pointer to next entry in window list
					   (null is end) */
	Window	*prev;			/* pointer to previous entry in window
					   list (null is end) */
	int	sticky;			/* make channels stick to window when
					   changing servers ? */
};

/*
 * The following should synthesize MAXINT on any machine with an 8 bit
 * word.
 */
#undef MAXINT
#define	MAXINT (-1&~(1<<(sizeof(int)*8-1)))

static	Window	*invisible_list = NULL; /* list of hidden windows */
static	u_char	*who_from = NULL;	/* nick of person whose message
					 * is being displayed */
static	int	who_level = LOG_CRAP;	/* Log level of message being
					   displayed */

/*
 * status_update_flag: if 1, the status is updated as normal.  If 0, then all
 * status updating is suppressed 
 */
static	int	status_update_flag = 1;

#ifdef lines
#undef lines
#endif /* lines */

static	void	remove_from_window_list(Window *);
static	void	hide_window(Window *);
static	void	hide_other_windows(void);
static	void	remove_from_invisible_list(Window *);
static	void	revamp_window_levels(Window *);
static	void	swap_window(Window *, Window *);
static	void	move_window(Window *, int);
static	void	grow_window(Window *, int);
static	Window	*get_next_window(void);
static	Window	*get_previous_window(void);
static	void	delete_other_windows(void);
static	void	bind_channel(u_char *, Window *);
static	void	unbind_channel(u_char *, Window *);
static	void	irc_goto_window(int);
static	void	list_a_window(Window *, int, int);
static	void	list_windows(void);
static	void	show_window(Window *);
static	void	push_window_by_refnum(u_int);
static	void	pop_window(void);
static	void	show_stack(void);
static	u_int	create_refnum(void);
static	int	is_window_name_unique(u_char *);
static	void	add_nicks_by_refnum(u_int, u_char *, int);
static	Window	*get_window(u_char *, u_char **);
static	Window	*get_invisible_window(u_char *, u_char **);
static	int	get_number(u_char *, u_char **);
static	int	get_boolean(u_char *, u_char **, int *);
static	void	display_lastlog_lines(int, int, Window *);
static	u_char	**split_up_line(u_char *);
static	void	redraw_window(Window *, int, int);
static	int	lastlog_lines(Window *);
static	void	scrollback_backwards_lines(int);
static	void	scrollback_forwards_lines(int);

/*
 * window_traverse: This will do as the name implies, traverse every
 * window (visible and invisible) and return a pointer to each window on
 * subsequent calls.  If flag points to a non-zero value, then the traversal
 * in started from the beginning again, and flag is set to point to 0.  This
 * returns all visible windows first, then all invisible windows.  It returns
 * null after all windows have been returned.  It should generally be used as
 * follows: 
 *
 * Win_Trav wt;
 * Window *tmp;
 *
 * wt.init = 1;
 * while ((tmp = window_traverse(&wt)))
 *	{ code here } 
 *
 * this version is recursive.
 */
Window	*
window_traverse(Win_Trav *wt)
{
	int	foo = 1;

	/* First call, return the current window basically */
	if (wt->init)
	{
		wt->init = 0;
		wt->visible = 1;
		if (!screen_first())
			return NULL;
		wt->screen = screen_first();
		wt->which = screen_get_window_list(wt->screen);
		if (wt->which)
			return (wt->which);
		else
			foo = 0;
	}

	/*
	 * foo is used to indicate the the current screen has no windows.
	 * This happens when we create a new screen..  either way, we want
	 * to go on to the next screen, so if foo isn't set, then if which
	 * is already null, return it again (this should never happen, if
	 * window_traverse()'s is called properly), else move on to
	 * the next window
	 */
	if (foo)
	{
		if (!wt->which)
			return NULL;
		else
			wt->which = wt->which->next;
	}

	if (!wt->which)
	{
		while (wt->screen)
		{
			wt->screen = screen_get_next(wt->screen);
			if (wt->screen && screen_get_alive(wt->screen))
				break;
		}
		if (wt->screen)
			wt->which = screen_get_window_list(wt->screen);
	}

	if (wt->which)
		return (wt->which);
	/* 
	 * Got to the end of the visible list..  so we do the invisible list..
	 * Should also mean, that we've got to the end of all the visible
	 * screen..
	 */
	if (wt->visible)
	{
		wt->visible = 0;
		wt->which = invisible_list;
		return (wt->which);
	}
	return (NULL);
}

void
add_window_to_server_group(Window *window, u_char *group)
{
	int	i = find_server_group(group, 1);
	Win_Trav wt;
	Window	*tmp;

	wt.init = 1;
	while ((tmp = window_traverse(&wt)) != NULL)
		if ((tmp->server_group == i) && (tmp->server != window->server))
		{
			say("Group %s already contains a different server", group);
			return;
		}
	window->server_group = i;
	say("Window's server group is now %s", group);
	update_window_status(window, 1);
}

/*
 * set_scroll_lines: called by /SET SCROLL_LINES to check the scroll lines
 * value 
 */
void
set_scroll_lines(int size)
{
	if (size == 0)
	{
		set_var_value(SCROLL_VAR, UP(var_settings(0)));
		if (curr_scr_win)
			curr_scr_win->scroll = 0;
	}
	else if (size > curr_scr_win->display_size)
	{
		say("Maximum lines that may be scrolled is %d", 
		    curr_scr_win->display_size);
		set_int_var(SCROLL_LINES_VAR, (u_int)curr_scr_win->display_size);
	}
}

/*
 * scroll_window: Given a pointer to a window, this determines if that window
 * should be scrolled, or the cursor moved to the top of the screen again, or
 * if it should be left alone. 
 */
void
scroll_window(Window *window)
{
	if (term_basic())
		return;
	if (window->cursor == window->display_size)
	{
		if (window->scroll)
		{
			int	do_scroll,
				i;

			if ((do_scroll = get_int_var(SCROLL_LINES_VAR)) <= 0)
				do_scroll = 1;

			for (i = 0; i < do_scroll; i++)
			{
				new_free(&window->top_of_display->line);
				window->top_of_display =
					window->top_of_display->next;
			}
			if (window->visible)
			{
				if (term_scroll(window->top + window_menu_lines(window),
						window->top + window_menu_lines(window) + window->cursor - 1,
						do_scroll))
				{
					if (number_of_windows() == 1)
					{
						Window	*tmp;

			/*
			 * this method of sim-u-scroll seems to work fairly
			 * well. The penalty is that you can only have one
			 * window, and of course you can't use the scrollback
			 * buffer either. Or menus. Actually, this method
			 * really doesn't work very well at all anymore.
			 */
						tmp = get_cursor_window();
						term_move_cursor(0, get_li() - 2);
						if (term_clear_to_eol())
							term_space_erase(0);
						term_cr();
						term_newline();
						for (i = 0; i < do_scroll; i++)
						{
							term_cr();
							term_newline();
						}
						update_window_status(window, 1);
						update_input(UPDATE_ALL);
						set_cursor_window(tmp);
						Debug(DB_SCROLL, "scroll_window: screen %d cursor_window set to window %d (%d)",
						       screen_get_screennum(get_current_screen()), window->refnum,
						       screen_get_screennum(window->screen));
					}
					else
						redraw_window(window, 1, 0);
				}
				window->cursor -= do_scroll;
				term_move_cursor(0, window->cursor + window->top + window_menu_lines(window));
			}
			else
				window->cursor -= do_scroll;
		}
		else
		{
			window->cursor = 0;
			if (window->visible)
				term_move_cursor(0, window->top + window_menu_lines(window));
		}
	}
	else if (window->visible && get_cursor_window() == window)
	{
		term_cr();
		term_newline();
	}
	if (window->visible && get_cursor_window())
	{
		if (term_clear_to_eol()) /* && !window->hold_mode && !window->hold_on_next_rite) */
		{
			term_space_erase(0);
			term_cr();
		}
		term_flush();
	}
}

/*
 * set_scroll: called by /SET SCROLL to make sure the SCROLL_LINES variable
 * is set correctly 
 */
void
set_scroll(int value)
{
	if (value && (get_int_var(SCROLL_LINES_VAR) == 0))
	{
		put_it("You must set SCROLL_LINES to a positive value first!");
		if (curr_scr_win)
			curr_scr_win->scroll = 0;
	}
	else
	{
		if (curr_scr_win)
		{
			int	old_value = curr_scr_win->scroll;

			curr_scr_win->scroll = value;
			if (old_value != value)
				scroll_window(curr_scr_win);
		}
	}
}

/*
 * this is where the real work of scrolling backwards and forwards
 * through the history is done.
 *
 * what happens is some functions are called from other code to
 * make scroll requests:  forward, back, start, end.  these functions
 * do some minor house keeping, and then call the function
 * display_lastlog_lines() to do the real work.
 *
 * we keep a split up copy of each line attached to the Display
 * structure, as well as the original full line, so we can split
 * it up again when the number of columns changes.  this splitting
 * can occur either when adding the lines to the lastlog itself,
 * or, if the line wasn't added to the lastlog, directly below
 * by calling split_up_line().
 */

/*
 * how many lastlog lines in this window?
 */
static int
lastlog_lines(Window *window)
{
	Display	*Disp;
	int num_lines = 0, i;

	(void)lastlog_line_back(window);

	for (i = window->new_scrolled_lines; i--; num_lines++)
		(void)lastlog_line_back(NULL);

	for (i = 0, Disp = window->top_of_display; i < window->display_size;
			Disp = Disp->next, i++)
		if (Disp->linetype)
			(void)lastlog_line_back(NULL);

	while (lastlog_line_back(NULL))
		num_lines++;

	Debug(DB_LASTLOG, "  num_lines ends up as %d", num_lines);
	return (num_lines);
}

/*
 * in window "window", display the lastlog lines from "start" to "end".
 */
static	void
display_lastlog_lines(int start, int end, Window *window)
{
	Display	*Disp;
	u_char	*Line;
	int	i;

	(void)lastlog_line_back(window);

	for (i = window->new_scrolled_lines; i--;)
		(void)lastlog_line_back(NULL);

	for (i = 0, Disp = window->top_of_display; i < window->display_size;
			Disp = Disp->next, i++)
		if (Disp->linetype)
			(void)lastlog_line_back(NULL);

	for (i = 0; i < start; i++)
		(void)lastlog_line_back(NULL);

	for (; i < end; i++)
	{
		if (!(Line = lastlog_line_back(NULL)))
			break;
		term_move_cursor(0, window->top + window_menu_lines(window) +
			window->scrolled_lines - i - 1);
		rite(window, Line, 0, 0, 1, 0);
	}
}

/*
 * scrollback_{back,forw}wards_lines: scroll the named distance, called by
 * internal functions here.
 */
static	void
scrollback_backwards_lines(int ScrollDist)
{
	Window	*window;
	int totallines;
	
	Debug(DB_SCROLL, "scrollback_backwards_lines(%d)", ScrollDist);
	window = curr_scr_win;
	if (!window->scrolled_lines && !window->scroll)
	{
		term_beep();
		return;
	}
	totallines = lastlog_lines(window);
	Debug(DB_SCROLL, "totallines = %d, scrolled_lines = %d", totallines, window->scrolled_lines);
	if (ScrollDist + window->scrolled_lines > totallines)
	{
		ScrollDist = totallines - window->scrolled_lines;
		Debug(DB_SCROLL, "  adjusting ScrollDist to %d", ScrollDist);
	}
	if (ScrollDist == 0)
	{
		term_beep();
		return;
	}
	window->scrolled_lines += ScrollDist;

	Debug(DB_SCROLL, "going to term_scroll(%d, %d, %d)",
	    window->top + window_menu_lines(window),
	    window->top + window_menu_lines(window) + window->display_size - 1,
	    -ScrollDist);
	term_scroll(window->top + window_menu_lines(window),
	    window->top + window_menu_lines(window) + window->display_size - 1,
	    -ScrollDist);

	Debug(DB_SCROLL, "scrolled_lines=%d, new_scrolled_lines=%d, display_size=%d",
	    window->scrolled_lines,
	    window->new_scrolled_lines,
	    window->display_size);

	Debug(DB_SCROLL, "going to display_lastlog_lines(%d, %d, %s)",
	    window->scrolled_lines - ScrollDist,
	    window->scrolled_lines,
	    window->name);
	display_lastlog_lines(window->scrolled_lines - ScrollDist,
	    window->scrolled_lines,
	    window);
	cursor_not_in_display();
	update_input(UPDATE_JUST_CURSOR);	// XXXMRG why this?
	window->update |= UPDATE_STATUS;
	update_window_status(window, 0);
}

static	void
scrollback_forwards_lines(int ScrollDist)
{
	Window	*window;

	Debug(DB_SCROLL, "scrollback_forward_lines(%d)", ScrollDist);
	window = curr_scr_win;
	if (!window->scrolled_lines)
	{
		term_beep();
		return;
	}
	if (ScrollDist > window->scrolled_lines)
		ScrollDist = window->scrolled_lines;

	Debug(DB_SCROLL, "scrolled_lines = %d", window->scrolled_lines);
	window->scrolled_lines -= ScrollDist;
	Debug(DB_SCROLL, "going to term_scroll(%d, %d, %d)",
	    window->top + window_menu_lines(window),
	    window->top + window_menu_lines(window) + window->display_size - 1,
	    ScrollDist);
	term_scroll(window->top + window_menu_lines(window),
	    window->top + window_menu_lines(window) + window->display_size - 1,
	    ScrollDist);

	Debug(DB_SCROLL, "scrolled_lines=%d, new_scrolled_lines=%d, display_size=%d",
	    window->scrolled_lines,
	    window->new_scrolled_lines,
	    window->display_size);

	if (window->scrolled_lines < window->display_size)
		redraw_window(window,
		    ScrollDist + window->scrolled_lines - window->display_size, 1);

	Debug(DB_SCROLL, "going to display_lastlog_lines(%d, %d, %s)",
	    window->scrolled_lines - window->display_size,
	    window->scrolled_lines - window->display_size + ScrollDist,
	    window->name);
	display_lastlog_lines(window->scrolled_lines - window->display_size,
	    window->scrolled_lines - window->display_size + ScrollDist,
	    window);
	cursor_not_in_display();
	update_input(UPDATE_JUST_CURSOR);	// XXXMRG why this?

	if (!window->scrolled_lines)
	{
		window->new_scrolled_lines = 0;
		if (window->hold_mode)
			window_hold_mode(window, ON, 1);
		else
			window_hold_mode(window, OFF, 0);
	}
	window->update |= UPDATE_STATUS;
	update_window_status(window, 0);
}

/*
 * scrollback_{forw,back}wards: scrolls the window up or down half the screen.
 * these are called when the respective key are pressed.
 */
void
scrollback_forwards(u_int key, u_char *ptr)
{
	scrollback_forwards_lines(curr_scr_win->display_size/2);
}

void
scrollback_backwards(u_int key, u_char *ptr)
{
	scrollback_backwards_lines(curr_scr_win->display_size/2);
}

/*
 * scrollback_end: exits scrollback mode and moves the display back
 * to the end of the scrollback buffer.
 */
void
scrollback_end(u_int key, u_char *ptr)
{
	Window	*window;

	window = curr_scr_win;
	window->new_scrolled_lines = 0;

	if (!window->scrolled_lines)
	{
		term_beep();
		return;
	}
	if (window->scrolled_lines < window->display_size)
		scrollback_forwards_lines(window->scrolled_lines);
	else
	{
		window->scrolled_lines = window->new_scrolled_lines = 0;
		redraw_window(window, 1, 1);
		cursor_not_in_display();
		update_input(UPDATE_JUST_CURSOR);	// XXXMRG why this?
		if (window->hold_mode)
			window_hold_mode(window, ON, 1);
		else
			window_hold_mode(window, OFF, 0);
		window->update |= UPDATE_STATUS;
		update_window_status(window, 0);
	}
}

/*
 * scrollback_start: moves the current screen back to the start of
 * the scrollback buffer.
 */
void
scrollback_start(u_int key, u_char *ptr)
{
	Window	*window;
	int	num_lines;

	window = curr_scr_win;
	if (!window_get_lastlog_size(window))
	{
		term_beep();
		return;
	}

	Debug(DB_SCROLL, "scrollback_start: new_scrolled_lines=%d display_size=%d",
	       window->new_scrolled_lines, window->display_size);
	num_lines = lastlog_lines(window);

	if (num_lines < window->display_size)
		scrollback_backwards_lines(num_lines);
	else
	{
		window->scrolled_lines = num_lines;
		Debug(DB_SCROLL, "going to display_lastlog_lines(%d, %d, %s)",
		      num_lines - window->display_size, window->scrolled_lines, window->name);
		display_lastlog_lines(num_lines - window->display_size,
		    window->scrolled_lines, window);
		cursor_not_in_display();
		update_input(UPDATE_JUST_CURSOR);	// XXXMRG why this?
		window->new_scrolled_lines = 0;
		if (window->hold_mode)
			window_hold_mode(window, ON, 1);
		else
			window_hold_mode(window, OFF, 0);
		window->update |= UPDATE_STATUS;
		update_window_status(window, 0);
	}
}

/*
 * update_all_windows: This goes through each visible window and draws the
 * necessary portions according the the update field of the window. 
 */
void
update_all_windows(void)
{
	Window	*tmp;
	int	fast_window,
		full_window,
		r_status,
		u_status;

	for (tmp = screen_get_window_list(get_current_screen()); tmp; tmp = window_get_next(tmp))
	{
		if (tmp->display_size != tmp->old_size)
			resize_display(tmp);
		if (tmp->update)
		{
			fast_window = tmp->update & REDRAW_DISPLAY_FAST;
			full_window = tmp->update & REDRAW_DISPLAY_FULL;
			r_status = tmp->update & REDRAW_STATUS;
			u_status = tmp->update & UPDATE_STATUS;
			if (full_window)
				redraw_window(tmp, 1, 0);
			else if (fast_window)
				redraw_window(tmp, 0, 0);
			if (r_status)
				update_window_status(tmp, 1);
			else if (u_status)
				update_window_status(tmp, 0);
		}
		tmp->update = 0;
	}
	for (tmp = invisible_list; tmp; tmp = window_get_next(tmp))
	{
		if (tmp->display_size != tmp->old_size)
			resize_display(tmp);
		tmp->update = 0;
	}
	update_input(UPDATE_JUST_CURSOR);
	term_flush();
}

/*
 * reset_line_cnt: called by /SET HOLD_MODE to reset the line counter so we
 * always get a held screen after the proper number of lines 
 */
void
reset_line_cnt(int value)
{
	curr_scr_win->hold_mode = value;
	curr_scr_win->hold_on_next_rite = 0;
	curr_scr_win->line_cnt = 0;
}

#define	MAXIMUM_SPLITS	80
static	u_char	**
split_up_line(u_char *str)
{
	static	u_char	*output[MAXIMUM_SPLITS] =
	{ 
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
	};
	u_char	lbuf[BIG_BUFFER_SIZE];
	u_char	*ptr;
	u_char	*cont_ptr,
		*cont = NULL,
		*temp = NULL,
		*mystr = NULL,
		c;
	int	pos = 0,
		col = 0,
		nd_cnt = 0,
		word_break = 0,
		start = 0,
		i,
		indent = 0,
		beep_cnt = 0,
		beep_max,
		tab_cnt = 0,
		tab_max,
		line = 0;
	int	bold_state = 0,
		invert_state = 0,
		underline_state = 0,
		fg_colour_state = 16,
		bg_colour_state = 16;
		
	size_t	len;

	struct mb_data mbdata;
#ifdef HAVE_ICONV_OPEN
	mbdata_init(&mbdata, CP(current_irc_encoding()));
#else
	mbdata_init(&mbdata, NULL);
#endif /* HAVE_ICONV_OPEN */
	
	memset(lbuf, 0, sizeof(lbuf));
	
	for (i = 0; i < MAXIMUM_SPLITS; i++)
		new_free(&output[i]);
	if (!str || !*str)
	{
		/* special case to make blank lines show up */
		malloc_strcpy(&mystr, UP(" "));
		str = mystr;
	}

	beep_max = get_int_var(BEEP_VAR) ? get_int_var(BEEP_MAX_VAR) : -1;
	tab_max = get_int_var(TAB_VAR) ? get_int_var(TAB_MAX_VAR) : -1;
	for (ptr = (u_char *) str; *ptr && (pos < sizeof(lbuf) - 8); ptr++)
	{
		switch (*ptr)
		{
		case '\007':	/* bell */
			if (beep_max == -1)
			{
				lbuf[pos++] = REV_TOG;
				lbuf[pos++] = (*ptr & 127) | 64;
				lbuf[pos++] = REV_TOG;
				nd_cnt += 2;
				col++;
			}
			else if (!beep_max || (++beep_cnt <= beep_max))
			{
				lbuf[pos++] = *ptr;
				nd_cnt++;
				col++;
			}
			break;
		case '\011':	/* tab */
			if (tab_max && (++tab_cnt > tab_max))
			{
				lbuf[pos++] = REV_TOG;
				lbuf[pos++] = (*ptr & 127) | 64;
				lbuf[pos++] = REV_TOG;
				nd_cnt += 2;
				col++;
			}
			else
			{
				if (indent == 0)
					indent = -1;
				len = 8 - (col % 8);
				word_break = pos;
				for (i = 0; i < len; i++)
					lbuf[pos++] = ' ';
				col += len;
			}
			break;
		case UND_TOG:
		case ALL_OFF:
		case REV_TOG:
		case BOLD_TOG:
			switch(*ptr)
			{
			case UND_TOG:
				underline_state = !underline_state;
				break;
			case REV_TOG:
				invert_state = !invert_state;
				break;
			case BOLD_TOG:
				bold_state = !bold_state;
				break;
			case ALL_OFF:
				underline_state = invert_state = bold_state = 0;
				fg_colour_state = bg_colour_state = 16;
				break;
			}
			lbuf[pos++] = *ptr;
			nd_cnt++;
			break;
		case COLOUR_TAG:
			{
				int fg, bg;

				lbuf[pos++] = *ptr++;
				decode_colour(&ptr, &fg, &bg);

				lbuf[pos++] = (u_char)(fg_colour_state = fg) + 1;
				lbuf[pos++] = (u_char)(bg_colour_state = bg) + 1;
				nd_cnt += 3;
				break;
			}
		case ' ':	/* word break */
			if (indent == 0)
				indent = -1;
			word_break = pos;
			lbuf[pos++] = *ptr;
			col++;
			break;
		default:	/* Anything else, make it displayable */
			if (indent == -1)
				indent = pos - nd_cnt;
   		   	
			decode_mb(ptr, lbuf + pos, sizeof(lbuf) - pos, &mbdata);
			
			/* If the sequence takes multiple columns,
			 * be wrappable now when we can
			 */
			if (mbdata.num_columns > 1)
				word_break = pos;

			pos += mbdata.output_bytes;
			nd_cnt += mbdata.output_bytes - mbdata.num_columns;
			ptr += mbdata.input_bytes - 1;
			col += mbdata.num_columns;
			break;
		}
		if (pos >= sizeof(lbuf) - 1)
			*ptr = '\0';

		if (col > get_co())
		{
			char c1;
			int oldpos;

			/* one big long line, no word breaks */
			if (word_break == 0)
				word_break = pos - (col - get_co());
			c = lbuf[word_break];
			c1 = lbuf[word_break+1];
			lbuf[word_break] = FULL_OFF;
			lbuf[word_break+1] = '\0';
			if (cont)
			{
				malloc_strcpy(&temp, cont);
				malloc_strcat(&temp, &lbuf[start]);
			}
			else
				malloc_strcpy(&temp, &lbuf[start]);
			malloc_strcpy(&output[line++], temp);
			if (line == MAXIMUM_SPLITS)
				break;
			lbuf[word_break] = c;
			lbuf[word_break+1] = c1;
			start = word_break;
			word_break = 0;
			while (lbuf[start] == ' ')
				start++;
			if (start > pos)
				start = pos;

			if (!(cont_ptr = get_string_var(CONTINUED_LINE_VAR)))
				cont_ptr = empty_string();

			if (get_int_var(INDENT_VAR) && (indent < get_co() / 3))
			{
				if (!cont) /* Build it only the first time */
				{
					/* Visual length */
					int vlen = my_strlen_c(cont_ptr);
					/* Converted length */
					int clen = my_strlen_ci(cont_ptr);
					int padlen = indent - vlen;

					if (padlen < 0)
						padlen = 0;
					
					cont = new_malloc(clen + padlen + 1);
					my_strcpy_ci(cont, clen + padlen + 1,
						     cont_ptr);
					
					for (; padlen > 0; --padlen)
					{
						cont[clen++] = ' ';
						/* Add space */
					}
					cont[clen] = '\0';
				}
			}
			else
			{
				/* Converted length */
				int clen = my_strlen_ci(cont_ptr);
				
				/* No indent, just prefix only */				
				cont = new_malloc(clen + 1);
				my_strcpy_ci(cont, clen + 1, cont_ptr);
			}
			
			/* cont contains internal codes, so use my_strlen_i */
			col = my_strlen_i(cont) + (pos - start);
			
			/* rebuild previous state */
			oldpos = pos;
			if (underline_state)
			{
				lbuf[pos++] = UND_TOG;
				nd_cnt++;
			}
			if (invert_state)
			{
				lbuf[pos++] = REV_TOG;
				nd_cnt++;
			}
			if (bold_state)
			{
				lbuf[pos++] = BOLD_TOG;
				nd_cnt++;
			}
			if (fg_colour_state != 16 || bg_colour_state != 16)
			{
				lbuf[pos++] = COLOUR_TAG;
				lbuf[pos++] = fg_colour_state + 1;
				lbuf[pos++] = bg_colour_state + 1;
				nd_cnt++;
			}
			if (pos != oldpos)
				while(start < oldpos)
					lbuf[pos++] = lbuf[start++];
		}
	}
	mbdata_done(&mbdata);
	
	if (line < MAXIMUM_SPLITS) {
		lbuf[pos++] = FULL_OFF;
		lbuf[pos] = '\0';
		if (lbuf[start])
		{
			if (cont)
			{
				malloc_strcpy(&temp, cont);
				malloc_strcat(&temp, &lbuf[start]);
			}
			else
				malloc_strcpy(&temp, &lbuf[start]);
			malloc_strcpy(&output[line++], temp);
		}
	}

	if (mystr)
		new_free(&mystr);
	new_free(&cont);
	new_free(&temp);
	return output;
}

/*
 * split_up_line_alloc(): like split_up_line() except it doesn't
 * return the static arrays, but copies them.  eventually when we
 * eliminate split_up_line() usage, we can make this entirely
 * dynamic.
 */
u_char	**
split_up_line_alloc(u_char *str)
{
	u_char **lines, **new_lines, **tmp;
	size_t total_lines = 0, count;

	lines = split_up_line(str);

	for (tmp = lines; *tmp; tmp++) {
		total_lines++;
	}
	
	new_lines = new_malloc(sizeof(*new_lines) * (total_lines + 1));

	/* Copy the pointers; clear the original from lines[] */
	for (count = 0; count < total_lines; count++) {
		new_lines[count] = lines[count];
		lines[count] = NULL;
	}
	new_lines[count] = NULL;

	return new_lines;
}

/*
 * add_to_window: adds the given string to the display.  No kidding. This
 * routine handles the whole ball of wax.  It keeps track of what's on the
 * screen, does the scrolling, everything... well, not quite everything...
 * The CONTINUED_LINE idea thanks to Jan L. Peterson (jlp@hamblin.byu.edu)  
 *
 * At least it used to. Now most of this is done by split_up_line, and this
 * function just dumps it onto the screen. This is because the scrollback
 * functions need to be able to figure out how to split things up too.
 */
void
add_to_window(Window *window, u_char *str)
{
	int flag;

	flag = do_hook(WINDOW_LIST, "%u %s", window_get_refnum(window), str);

	if (flag)
	{
		size_t	len = my_strlen(str);
		u_char	*my_str = new_malloc(len + 2);
		u_char	**lines, **curlinep, **freelines;
		int	logged;

		add_to_window_log(window, str);
		display_highlight(OFF);
		display_bold(OFF);
		display_colours(get_int_var(FOREGROUND_COLOUR_VAR),
		    get_int_var(BACKGROUND_COLOUR_VAR),
		    OFF, OFF, OFF);
		memmove(my_str, str, len);
		my_str[len] = ALL_OFF;
		my_str[len + 1] = '\0';
		lines = add_to_lastlog(window, my_str);
		if (!lines)
			freelines = lines = split_up_line(my_str);
		logged = islogged(window);
		/*
		 * For each of the lines created by split_up_line(),
		 * display the line.
		 * rite() will assume each input line fits on 1 line.
		 */
		for (curlinep = lines; *curlinep; curlinep++)
		{
			rite(window, *curlinep, 0, 0, 0, logged);
			if (logged == LT_LOGHEAD)
				logged = LT_LOGTAIL;
		}
		new_free(&my_str);
		if (0)
			new_free(&freelines);
		term_flush();
	}
}

/*
 * set_continued_line: checks the value of CONTINUED_LINE for validity,
 * altering it if its no good 
 */
void
set_continued_line(u_char *value)
{
	if (value && ((int) my_strlen(value) > (get_co() / 2)))
		value[get_co() / 2] = '\0';
}

static	void
remove_from_invisible_list(Window *window)
{
	window->visible = 1;
	window->screen = get_current_screen();
	window->miscflags &= ~WINDOW_NOTIFIED;
	if (window->prev)
		window->prev->next = window->next;
	else
		invisible_list = window->next;
	if (window->next)
		window->next->prev = window->prev;
}

void
add_to_invisible_list(Window *window)
{
	if ((window->next = invisible_list) != NULL)
		invisible_list->prev = window;
	invisible_list = window;
	window->prev = NULL;
	window->visible = 0;
	window->screen = NULL;
}

/*
 * swap_window: This swaps the given window with the current window.  The
 * window passed must be invisible.  Swapping retains the positions of both
 * windows in their respective window lists, and retains the dimensions of
 * the windows as well, but update them depending on the number of status
 * lines displayed
 */
static	void
swap_window(Window *v_window, Window *window)
{
	Window tmp, *prev, *next;
	int	top, bottom, size;

	if (window->visible || !v_window->visible)
	{
		say("You can only SWAP a hidden window with a visible window.");
		return;
	}

	channel_swap_win_ptr(v_window, window);

	prev = v_window->prev;
	next = v_window->next;

	set_last_window_refnum(v_window->refnum);
	remove_from_invisible_list(window);

	tmp = *v_window;
	*v_window = *window;
	v_window->top = tmp.top;
	v_window->bottom = tmp.bottom + tmp.double_status - 
		v_window->double_status;
	v_window->display_size = tmp.display_size + menu_lines(tmp.menu) +
		tmp.double_status -
		menu_lines(v_window->menu) - v_window->double_status -
		SCROLL_DISPLAY_OFFSET;
	v_window->prev = prev;
	v_window->next = next;

	/* I don't understand the use of the following, I'll ignore it
	 * If double status screws window sizes, I should look here
	 * again - krys
	 */
	top = window->top;
	bottom = window->bottom;
	size = window->display_size;
	*window = tmp;
	window->top = top;
	window->bottom = bottom - tmp.double_status;
	window->display_size = size - SCROLL_DISPLAY_OFFSET;

	add_to_invisible_list(window);

	v_window->update |= REDRAW_DISPLAY_FULL | REDRAW_STATUS;
	window->update |= REDRAW_DISPLAY_FULL | REDRAW_STATUS;

	do_hook(WINDOW_SWAP_LIST, "%d %d", v_window->refnum, window->refnum);
}

/*
 * move_window: This moves a window offset positions in the window list. This
 * means, of course, that the window will move on the screen as well 
 */
static	void
move_window(Window *window, int offset)
{
	Window	*tmp, *last;
	int	win_pos, pos, num;

	if (offset == 0)
		return;
	last = NULL;
	for (win_pos = 0, tmp = screen_get_window_list(get_current_screen()); tmp;
	    tmp = tmp->next, win_pos++)
	{
		if (window == tmp)
			break;
		last = tmp;
	}
	if (tmp == NULL)
		return;
	if (last == NULL)
		screen_set_window_list(get_current_screen(), tmp->next);
	else
		last->next = tmp->next;
	if (tmp->next)
		tmp->next->prev = last;
	else
		screen_set_window_list_end(get_current_screen(), last);
	num = screen_get_visible_windows(window->screen);
	win_pos = (offset + win_pos) % num;
	if (win_pos < 0)
		win_pos = num + win_pos;
	last = NULL;
	for (pos = 0, tmp = screen_get_window_list(get_current_screen());
	    pos != win_pos; tmp = tmp->next, pos++)
		last = tmp;
	if (last == NULL)
		screen_set_window_list(get_current_screen(), window);
	else
		last->next = window;
	if (tmp)
		tmp->prev = window;
	else
		screen_set_window_list_end(get_current_screen(), window);
	window->prev = last;
	window->next = tmp;
	recalculate_window_positions();
}

/*
 * grow_window: This will increase or descrease the size of the given window
 * by offset lines (positive offset increases, negative decreases).
 * Obviously, with a fixed terminal size, this means that some other window
 * is going to have to change size as well.  Normally, this is the next
 * window in the window list (the window below the one being changed) unless
 * the window is the last in the window list, then the previous window is
 * changed as well 
 */
static	void
grow_window(Window *window, int offset)
{
	Window	*other,
	    *tmp;
	int	after,
	window_size,
	other_size;

	if (window == NULL)
		window = curr_scr_win;
	if (!window->visible)
	{
		say("You cannot change the size of hidden windows!");
		return;
	}
	if (window->next)
	{
		other = window->next;
		after = 1;
	}
	else
	{
		other = NULL;
		for (tmp = screen_get_window_list(get_current_screen()); tmp; tmp = tmp->next)
		{
			if (tmp == window)
				break;
			other = tmp;
		}
		if (other == NULL)
		{
			say("Can't change the size of this window!");
			return;
		}
		after = 0;
	}
	window_size = window->display_size + offset;
	other_size = other->display_size - offset;
	if ((window_size < 4) ||
	    (other_size < 4))
	{
		say("Not enough room to resize this window!");
		return;
	}
	if (after)
	{
		window->bottom += offset;
		other->top += offset;
	}
	else
	{
		window->top -= offset;
		other->bottom -= offset;
	}
	window->display_size = window_size - SCROLL_DISPLAY_OFFSET;
	other->display_size = other_size - SCROLL_DISPLAY_OFFSET;
	window->update |= REDRAW_DISPLAY_FULL | REDRAW_STATUS;
	other->update |= REDRAW_DISPLAY_FULL | REDRAW_STATUS;
	term_flush();
}

/*
 * the message_from stack structure.
 */
struct mfstack
{
	u_char	*who_from;	/* saved from */
	int	who_level;	/* saved level */
	struct mfstack *next;	/* next in the list */
} mfstack_head = { NULL, 0, NULL };

/*
 * save_message_from: this is used to save (for later restoration) the
 * who_from variable.  This comes in handy very often when a routine might
 * call another routine that might change who_from.   Note that if you
 * call this routine, you *must* call restore_message_from().
 */
void
save_message_from(void)
{
	struct mfstack *mfs;

	mfs = new_malloc(sizeof *mfs);

	mfs->who_from = NULL;
	malloc_strcpy(&mfs->who_from, who_from);
	mfs->who_level = who_level;
	mfs->next = mfstack_head.next;

	mfstack_head.next = mfs;
}

/* restore_message_from: restores a previously saved who_from variable */
void
restore_message_from(void)
{
	struct mfstack *mfs = mfstack_head.next;

	if (mfs == NULL)
	{
		/*yell("--- restore_message_from: NULL next pointer, fudging..");*/
		malloc_strcpy(&who_from, NULL);
		who_level = LOG_CRAP;
	}
	else
	{
		malloc_strcpy(&who_from, mfs->who_from);
		who_level = mfs->who_level;
		mfstack_head.next = mfs->next;
		new_free(&mfs->who_from);
		new_free(&mfs);
	}
}

/*
 * message_from: With this you can the who_from variable and the who_level
 * variable, used by the display routines to decide which window messages
 * should go to.  
 */
void
message_from(u_char *who, int level)
{
	malloc_strcpy(&who_from, who);
	who_level = level;
}

/*
 * message_from_level: Like set_lastlog_msg_level, except for message_from.
 * this is needed by XECHO, because we could want to output things in more
 * than one level.
 */
int
message_from_level(int level)
{
	int	temp;

	temp = who_level;
	who_level = level;
	return temp;
}

/*
 * get_window_by_refnum: Given a reference number to a window, this returns a
 * pointer to that window if a window exists with that refnum, null is
 * returned otherwise.  The "safe" way to reference a window is throught the
 * refnum, since a window might be delete behind your back and and Window
 * pointers might become invalid.  
 */
Window	*
get_window_by_refnum(u_int refnum)
{
	Window	*tmp;

	if (refnum)
	{
		Win_Trav wt;

		wt.init = 1;
		while ((tmp = window_traverse(&wt)) != NULL)
		{
			if (tmp->refnum == refnum)
				return (tmp);
		}
	}
	else
		return (curr_scr_win);
	return (NULL);
}

/*
 * clear_window_by_refnum: just like clear_window(), but it uses a refnum. If
 * the refnum is invalid, the current window is cleared. 
 */
void
clear_window_by_refnum(u_int refnum)
{
	Window	*tmp;

	if ((tmp = get_window_by_refnum(refnum)) == NULL)
		tmp = curr_scr_win;
	clear_window(tmp);
}

/*
 * revamp_window_levels: Given a level setting for the current window, this
 * makes sure that that level setting is unused by any other window. Thus
 * only one window in the system can be set to a given level.  This only
 * revamps levels for windows with servers matching the given window 
 * it also makes sure that only one window has the level `DCC', as this is
 * not dependant on a server.
 */
static	void
revamp_window_levels(Window *window)
{
	Window	*tmp;
	Win_Trav wt;
	int	got_dcc;

	got_dcc = (LOG_DCC & window->window_level) ? 1 : 0;
	wt.init = 1;
	while ((tmp = window_traverse(&wt)) != NULL)
	{
		if (tmp == window)
			continue;
		if (LOG_DCC & tmp->window_level)
		{
			if (0 != got_dcc)
				tmp->window_level &= ~LOG_DCC;
			got_dcc = 1;
		}
		if (window->server == tmp->server)
			tmp->window_level ^= (tmp->window_level & window->window_level);
	}
}

/*
 * set_level_by_refnum: This sets the window level given a refnum.  It
 * revamps the windows levels as well using revamp_window_levels() 
 */
void
set_level_by_refnum(u_int refnum, int level)
{
	Window	*tmp;

	if ((tmp = get_window_by_refnum(refnum)) == NULL)
		tmp = curr_scr_win;
	tmp->window_level = level;
	revamp_window_levels(tmp);
}

/*
 * set_prompt_by_refnum: changes the prompt for the given window.  A window
 * prompt will be used as the target in place of the query user or current
 * channel if it is set 
 */
void
set_prompt_by_refnum(u_int refnum, u_char *prompt)
{
	Window	*tmp;

	if ((tmp = get_window_by_refnum(refnum)) == NULL)
		tmp = curr_scr_win;
	malloc_strcpy(&tmp->prompt, prompt);
}

/*
 * message_to: This allows you to specify a window (by refnum) as a
 * destination for messages.  Used by EXEC routines quite nicely 
 */
void
message_to(u_int refnum)
{
	if (refnum)
		set_to_window(get_window_by_refnum(refnum));
	else
		set_to_window(NULL);
}

/*
 * get_next_window: This returns a pointer to the next *visible* window in
 * the window list.  It automatically wraps at the end of the list back to
 * the beginning of the list 
 */
static	Window	*
get_next_window(void)
{
	if (curr_scr_win && curr_scr_win->next)
		return window_get_next(curr_scr_win);
	else
		return screen_get_window_list(get_current_screen());
}

/*
 * get_previous_window: this returns the previous *visible* window in the
 * window list.  This automatically wraps to the last window in the window
 * list 
 */
static	Window	*
get_previous_window(void)
{
	if (curr_scr_win && curr_scr_win->prev)
		return window_get_prev(curr_scr_win);
	else
		return screen_get_window_list_end(get_current_screen());
}

/*
 * set_current_window: This sets the "current" window to window.  It also
 * keeps track of the last_get_current_screen()->current_window by setting it to the
 * previous current window.  This assures you that the new current window is
 * visible.
 * If not, a new current window is chosen from the window list 
 */
void
set_current_window(Window *window)
{
	Window	*tmp;
	unsigned int	refnum;

	refnum = get_last_window_refnum();
	if (curr_scr_win)
	{
		curr_scr_win->update |= UPDATE_STATUS;
		set_last_window_refnum(curr_scr_win->refnum);
	}
	if ((window == NULL) || (!window->visible))

	{
		if ((tmp = get_window_by_refnum(refnum)) && (tmp->visible))
			set_curr_scr_win(tmp);
		else
			set_curr_scr_win(get_next_window());
	}
	else
		set_curr_scr_win(window);
	curr_scr_win->update |= UPDATE_STATUS;
}

/*
 * swap_last_window:  This swaps the current window with the last window
 * that was hidden.
 */

void
swap_last_window(u_int key, u_char *ptr)
{
	if (invisible_list == NULL)
	{
		/* say("There are no hidden windows"); */
		/* Not sure if we need to warn   - phone. */
		return;
	}
	swap_window(curr_scr_win, invisible_list);
	update_all_windows();
	cursor_to_input();
}

/*
 * next_window: This switches the current window to the next visible window 
 */
void
next_window(u_int key, u_char *ptr)
{
	if (number_of_windows() == 1)
		return;
	set_current_window(get_next_window());
	update_all_windows();
}

/*
 * swap_next_window:  This swaps the current window with the next hidden
 * window.
 */

void
swap_next_window(u_int key, u_char *ptr)
{
	Win_Trav wt;
	Window	*tmp;
	u_int	next = MAXINT;
	int	smallest;

	if (invisible_list == NULL)
	{
		say("There are no hidden windows");
		return;
	}
	smallest = curr_scr_win->refnum;
	wt.init = 1;
	while ((tmp = window_traverse(&wt)) != NULL)
	{
		if (!tmp->visible)
		{
			if (tmp->refnum < smallest)
				smallest = tmp->refnum;
			if ((tmp->refnum > curr_scr_win->refnum)
			    && (next > tmp->refnum))
				next = tmp->refnum;
		}
	}
	if (next != MAXINT)
		tmp = get_window_by_refnum(next);
	else
		tmp = get_window_by_refnum((u_int)smallest);
	if (tmp)
	{
		swap_window(curr_scr_win, tmp);
		update_all_windows();
		update_all_status();
		cursor_to_input();
	}
}

/*
 * previous_window: This switches the current window to the previous visible
 * window 
 */
void
previous_window(u_int key, u_char *ptr)
{
	if (number_of_windows() == 1)
		return;
	set_current_window(get_previous_window());
	update_all_windows();
}

/*
 * swap_previous_window:  This swaps the current window with the next 
 * hidden window.
 */

void
swap_previous_window(u_int key, u_char *ptr)
{
	Win_Trav wt;
	Window	*tmp;
	int	previous = 0;
	int	largest;

	if (invisible_list == NULL)
	{
		say("There are no hidden windows");
		return;
	}
	largest = curr_scr_win->refnum;
	wt.init = 1;
	while ((tmp = window_traverse(&wt)) != NULL)
	{
		if (!tmp->visible)
		{
			if (tmp->refnum > largest)
				largest = tmp->refnum;
			if ((tmp->refnum < curr_scr_win->refnum)
			    && (previous < tmp->refnum))
				previous = tmp->refnum;
		}
	}
	if (previous)
		tmp = get_window_by_refnum((u_int)previous);
	else
		tmp = get_window_by_refnum((u_int)largest);
	if (tmp)
	{
		swap_window(curr_scr_win,tmp);
		update_all_windows();
		update_all_status();
		cursor_to_input();
	}
}

/*
 * back_window:  goes to the last window that was current.  Swapping the
 * current window if the last window was hidden.
 */

void
back_window(u_int key, u_char *ptr)
{
	Window	*tmp;

	tmp = get_window_by_refnum(get_last_window_refnum());
	if (tmp)
	{
		if (tmp->visible)
			set_current_window(tmp);
		else
		{
			swap_window(curr_scr_win, tmp);
			update_all_windows();
			update_all_status();
			cursor_to_input();
		}
	}
}

/*
 * The common way to set display_size(), also used externally.
 */
void
window_set_display_size_normal(Window *window)
{
	window->display_size = window->bottom - window->top -
			menu_lines(window->menu) - SCROLL_DISPLAY_OFFSET;
}

/*
 * add_to_window_list: This inserts the given window into the visible window
 * list (and thus adds it to the displayed windows on the screen).  The
 * window is added by splitting the current window.  If the current window is
 * too small, the next largest window is used.  The added window is returned
 * as the function value or null is returned if the window couldn't be added 
 */
Window	*
add_to_window_list(Window *new)
{
	Window	*biggest = NULL,
		*tmp;

	incr_visible_windows();
	if (curr_scr_win == NULL)
	{
		screen_set_window_list(get_current_screen(), new);
		screen_set_window_list_end(get_current_screen(), new);
		if (term_basic())
		{
			/* what the hell */
			new->display_size = 24 - SCROLL_DISPLAY_OFFSET;
			set_current_window(new);
			return (new);
		}
		recalculate_windows();
	}
	else
	{
		/* split current window, or find a better window to split */
		if ((curr_scr_win->display_size < 4) ||
				get_int_var(ALWAYS_SPLIT_BIGGEST_VAR))
		{
			int	size = 0;

			for (tmp = screen_get_window_list(get_current_screen()); tmp;
					tmp = tmp->next)
			{
				if (tmp->display_size > size)
				{
					size = tmp->display_size;
					biggest = tmp;
				}
			}
			if ((biggest == NULL) || (size < 4))
			{
				say("Not enough room for another window!");
				/* Probably a source of memory leaks */
				new_free(&new);
				decr_visible_windows();
				return (NULL);
			}
		}
		else
			biggest = curr_scr_win;
		if ((new->prev = biggest->prev) != NULL)
			new->prev->next = new;
		else
			screen_set_window_list(get_current_screen(), new);
		new->next = biggest;
		biggest->prev = new;
		new->top = biggest->top;
		new->bottom = (biggest->top + biggest->bottom) / 2 -
			new->double_status;
		biggest->top = new->bottom + new->double_status + 1;
		new->display_size = new->bottom - new->top -
			SCROLL_DISPLAY_OFFSET;
		biggest->display_size = biggest->bottom - biggest->top -
			menu_lines(biggest->menu) -
			SCROLL_DISPLAY_OFFSET;
		new->update |= REDRAW_DISPLAY_FULL | REDRAW_STATUS;
		biggest->update |= REDRAW_DISPLAY_FULL | REDRAW_STATUS;
	}
	return (new);
}

/*
 * remove_from_window_list: this removes the given window from the list of
 * visible windows.  It closes up the hole created by the windows absense in
 * a nice way 
 */
static void
remove_from_window_list(Window *window)
{
	Window	*other;

	/* find adjacent visible window to close up the screen */
	for (other = window->next; other; other = other->next)
	{
		if (other->visible)
		{
			other->top = window->top;
			break;
		}
	}
	if (other == NULL)
	{
		for (other = window->prev; other; other = other->prev)
		{
			if (other->visible)
			{
				other->bottom = window->bottom + window->double_status - other->double_status;
				break;
			}
		}
	}
	/* remove window from window list */
	if (window->prev)
		window->prev->next = window->next;
	else
		screen_set_window_list(get_current_screen(), window->next);
	if (window->next)
		window->next->prev = window->prev;
	else
		screen_set_window_list_end(get_current_screen(), window->prev);
	if (window->visible)
	{
		decr_visible_windows();
		other->display_size = other->bottom - other->top -
			menu_lines(other->menu) - SCROLL_DISPLAY_OFFSET;
		if (window == curr_scr_win)
			set_current_window(NULL);
		if (window->refnum == get_last_window_refnum())
			set_last_window_refnum(curr_scr_win->refnum);
	}
}

/*
 * window_check_servers: this checks the validity of the open servers vs the
 * current window list.  Every open server must have at least one window
 * associated with it.  If a window is associated with a server that's no
 * longer open, that window's server is set to the primary server.  If an
 * open server has no assicatiate windows, that server is closed.  If the
 * primary server is no more, a new primary server is picked from the open
 * servers 
 */
void
window_check_servers(void)
{
	Window	*tmp;
	Win_Trav wt;
	int	cnt, max, i, not_connected;
	int	alive_servers = 0;
	int	prime = -1;

	max = number_of_servers();
	for (i = 0; i < max; i++)
	{
		not_connected = !is_server_open(i);
		cnt = 0;
		wt.init = 1;
		while ((tmp = window_traverse(&wt)) != NULL)
		{
			if (tmp->server == i)
			{
				if (not_connected)
				{
					tmp->server = get_primary_server();
					if (tmp->current_channel)
						new_free(&tmp->current_channel);
				}
				else
				{
					prime = tmp->server;
					cnt++;
				}
			}
		}
		if (cnt == 0)
		{
#ifdef NON_BLOCKING_CONNECTS
			if (!server_get_flag(i, CLOSE_PENDING))
#endif /* NON_BLOCKING_CONNECTS */
				close_server(i, empty_string());
		}
		else
			alive_servers++;
	}
	if (!is_server_open(get_primary_server()))
	{
		wt.init = 1;
		while ((tmp = window_traverse(&wt)) != NULL)
			if (tmp->server == get_primary_server())
			{
				tmp->server = prime;
			}
		set_primary_server(prime);
	}
	set_connected_to_server(alive_servers);
	update_all_status();
	cursor_to_input();
}

/*
 * restore_previous_server: Attempts to restore all windows that were
 * associated with `server', that currently contain nothing, to said
 * server.
 */
void
window_restore_server(int server)
{
	Window	*tmp;
	int	max = number_of_servers(),
		i;

	for (i = 0; i < max; i++)
	{
		Win_Trav wt;

		wt.init = 1;
		while ((tmp = window_traverse(&wt)) != NULL)
		{
			if (tmp->server == get_primary_server() &&
			    tmp->prev_server == server)
			{
				tmp->server = tmp->prev_server;
				realloc_channels(tmp);
			}
		}
	}
}

/*
 * delete_window: This deletes the given window.  It frees all data and
 * structures associated with the window, and it adjusts the other windows so
 * they will display correctly on the screen. 
 */
void
delete_window(Window *window)
{
	u_char	*tmp = NULL;
	u_char	buffer[BIG_BUFFER_SIZE];

	if (window == NULL)
		return;
	if (window->visible && number_of_windows() == 1)
	{
		if (invisible_list)
		{
			swap_window(window, invisible_list);
			window = invisible_list;
		}
		else
		{
			say("You can't kill the last window!");
			return;
		}
	}
	if (window->name)
		my_strmcpy(buffer, window->name, sizeof buffer);
	else
		snprintf(CP(buffer), sizeof buffer, "%u", window->refnum);
	malloc_strcpy(&tmp, buffer);
	realloc_channels(window);
	new_free(&window->status_line[0]);
	new_free(&window->status_line[1]);
	new_free(&window->query_nick);
	new_free(&window->current_channel);
	new_free(&window->logfile);
	new_free(&window->name);
	new_free(&window->menu);
	free_display(window);
	free_hold_info(&window->hold_info);
	free_lastlog(window);
	free_nicks(window);
	if (window->visible)
		remove_from_window_list(window);
	else
		remove_from_invisible_list(window);
	new_free(&window);
	window_check_servers();
	do_hook(WINDOW_KILL_LIST, "%s", tmp);
	new_free(&tmp);
}

/* delete_other_windows: zaps all visible windows except the current one */
static	void
delete_other_windows(void)
{
	Window	*tmp,
		*cur,
		*next;

	cur = curr_scr_win;
	tmp = screen_get_window_list(get_current_screen());
	while (tmp)
	{
		next = tmp->next;
		if (tmp != cur)
		{
			delete_window(tmp);
			update_all_windows();
		}
		tmp = next;
	}
}

/*
 * window_kill_swap:  Swaps with the last window that was hidden, then
 * kills the window that was swapped.  Give the effect of replacing the
 * current window with the last one, and removing it at the same time.
 */

void
window_kill_swap(void)
{
	if (invisible_list != NULL)
	{
		swap_last_window(0, NULL);
		delete_window(get_window_by_refnum(get_last_window_refnum()));
	}
	else
		say("There are no hidden windows!");
}

/*
 * unhold_windows: This is used by the main io loop to display held
 * information at an appropriate time.  Each time this is called, each
 * windows hold list is checked.  If there is info in the hold list and the
 * window is not held, the first line is displayed and removed from the hold
 * list.  Zero is returned if no infomation is displayed 
 */
int
unhold_windows(void)
{
	Window	*tmp;
	Win_Trav wt;
	u_char	*stuff;
	int	hold_flag = 0;
	int	logged;

	wt.init = 1;
	while ((tmp = window_traverse(&wt)) != NULL)
	{
		if (!window_hold_output(tmp) && (stuff = hold_queue(tmp->hold_info)))
		{
			logged = hold_queue_logged(tmp->hold_info);
			if (rite(tmp, stuff, 1, 0, 0, logged) == 0)
			{
				remove_from_hold_list(tmp->hold_info);
				hold_flag = 1;
			}
		}
	}
	return (hold_flag);
}

/*
 * update_window_status: This updates the status line for the given window.
 * If the refresh flag is true, the entire status line is redrawn.  If not,
 * only some of the changed portions are redrawn 
 */
void
update_window_status(Window *window, int refreshit)
{
	if (term_basic() || (!window->visible) || !status_update_flag || never_connected())
		return;
	if (window == NULL)
		window = curr_scr_win;
	if (refreshit)
	{
		new_free(&window->status_line[0]);
		new_free(&window->status_line[1]);
	}
	make_status(window);
}

/*
 * redraw_all_status: This redraws all of the status lines for all of the
 * windows. 
 */
void
redraw_all_status(void)
{
	Window	*tmp;

	if (term_basic())
		return;
	for (tmp = screen_get_window_list(get_current_screen()); tmp; tmp = tmp->next)
	{
		new_free(&tmp->status_line[0]);
		new_free(&tmp->status_line[1]);
		make_status(tmp);
	}
	update_input(UPDATE_JUST_CURSOR);	// XXXMRG why this?
	term_flush();
}

/*
 * update_all_status: This updates all of the status lines for all of the
 * windows.  By updating, it only draws from changed portions of the status
 * line to the right edge of the screen 
 */
void
update_all_status(void)
{
	Window	*window;
	Screen	*screen;

	if (term_basic() || !status_update_flag || never_connected())
		return;
	for (screen = screen_first(); screen; screen = screen_get_next(screen))
	{
		if (!screen_get_alive(screen))
			continue;
		for (window = screen_get_window_list(screen);window; window = window->next)
			if (window->visible)
				make_status(window);
	}
	update_input(UPDATE_JUST_CURSOR);	// XXXMRG why this?
	term_flush();
}

/*
 * status_update: sets the status_update_flag to whatever flag is.  This also
 * calls update_all_status(), which will update the status line if the flag
 * was true, otherwise it's just ignored 
 */
void
status_update(int flag)
{
	status_update_flag = flag;
	update_all_status();
	cursor_to_input();
}

void
redraw_resized(Window *window, ShrinkInfo Info, int AnchorTop)
{
	if (AnchorTop)
		return;
	if (Info.bottom < 0)
		term_scroll(window->top + window_menu_lines(window) + Info.bottom,
			window->top + window_menu_lines(window) +
			window->display_size - 1,
			Info.bottom);
	else if (Info.bottom)
		term_scroll(window->top + window_menu_lines(window),
			window->top + window_menu_lines(window) +
			window->display_size -1, Info.bottom);
}

/*
 * resize_display: After determining that the screen has changed sizes, this
 * resizes all the internal stuff.  If the screen grew, this will add extra
 * empty display entries to the end of the display list.  If the screen
 * shrank, this will remove entries from the end of the display list.  By
 * doing this, we try to maintain as much of the display as possible. 
 *
 * This has now been improved so that it returns enough information for
 * redraw_resized to redisplay the contents of the window without having
 * to redraw too much.
 */
ShrinkInfo
resize_display(Window *window)
{
	int	cnt,
		i;
	Display *tmp, *pre_ip;
	int	Wrapped = 0;
	ShrinkInfo Result;

	Result.top = Result.bottom = 0;
	Result.position = window->cursor;
	if (term_basic())
	{
		return Result;
	}
	if (!window->top_of_display)
	{
		window->top_of_display = new_malloc(sizeof(Display));
		window->top_of_display->line = NULL;
		window->top_of_display->linetype = LT_UNLOGGED;
		window->top_of_display->next = window->top_of_display;
		window->display_ip = window->top_of_display;
		window->old_size = 1;
	}
	/* cnt = size - window->display_size; */
	cnt = window->display_size - window->old_size;
	if (cnt > 0)
	{
		Display *new = NULL;

		/*
		 * screen got bigger: got to last display entry and link
		 * in new entries 
		 */
		for (tmp = window->top_of_display, i = 0;
		    i < (window->old_size - 1);
		    i++, tmp = tmp->next);
		for (i = 0; i < cnt; i++)
		{
			new = new_malloc(sizeof *new);
			new->line = NULL;
			new->linetype = LT_UNLOGGED;
			new->next = tmp->next;
			tmp->next = new;
		}
		if (window->display_ip == window->top_of_display &&
		    window->top_of_display->line)
			window->display_ip = new;
		Result.top = 0;
		Result.bottom = cnt;
		Result.position = 0;
	}
	else if (cnt < 0)

	{
		Display *ptr;

		/*
		 * screen shrank: find last display entry we want to keep,
		 * and remove all after that point 
		 */
		cnt = -cnt;
		for (pre_ip = window->top_of_display;
		    pre_ip->next != window->display_ip;
		    pre_ip = pre_ip->next);
		for (tmp = pre_ip->next, i = 0; i < cnt; i++, tmp = ptr)
		{
			ptr = tmp->next;
			if (tmp == window->top_of_display)

			{
				if (tmp->line)
					Wrapped = 1;
				window->top_of_display = ptr;
			}
			if (Wrapped)
				Result.top--;
			else
				Result.bottom--;
			new_free(&(tmp->line));
			new_free(&tmp);
		}
		window->display_ip = pre_ip->next = tmp;
		window->cursor += Result.top;
		if (!window->scroll)
		{
			if (window->cursor == window->display_size)
				window->cursor = 0;
			new_free(&window->display_ip->line);
		}
	}
	window->update |= REDRAW_DISPLAY_FULL | REDRAW_STATUS;
	window->old_size = window->display_size;
	return Result;
}

/*
 * helper for recalculate_windows(): set the sizes for one window.
 */
static void
window_set_one_size(Window *window, int size, int top)
{
	window->update |= REDRAW_DISPLAY_FULL | REDRAW_STATUS;
	window->display_size = size - window_menu_lines(window)
		- window->double_status - SCROLL_DISPLAY_OFFSET;
	if (window->display_size <= 0)
		window->display_size = 1;
	window->top = top;
	window->bottom = top + size - window->double_status;

	Debug(DB_WINDOW, "size %d top %d set refnum %d display_size %d bottom %d", size, top, window->refnum, window->display_size, window->bottom);
	return;
}

/*
 * recalculate_windows: this is called when the terminal size changes (as
 * when an xterm window size is changed).  It recalculates the sized and
 * positions of all the windows.
 *
 * XXX currently still buggy.
 */
void
recalculate_windows(void)
{
	Window	*window, *last_window;
	int	total_size = 0, total_menu_size = 0;
	int	win_count = 0;
	int	lines, old_lines;
	int	top, extra = 0;
	int	got_extra = 0;
	double	change;

	if (term_basic() || number_of_windows() == 0)
		return;

	/*
	 * Calculate the totals we have right now.
	 */
	for (window = screen_get_window_list(get_current_screen());
	     window; window = window_get_next(window))
	{
		total_size += window->display_size;
		total_menu_size += window_menu_lines(window);
		win_count++;
	}
	Debug(DB_WINDOW, "old total size %d menu lines %d wincount %d",
	      total_size, total_menu_size, win_count);

	old_lines = screen_get_old_li() - 2;
	lines = get_li() - 2;

	if (win_count == 1)
	{
		window = screen_get_window_list(get_current_screen());
		window_set_one_size(window, lines, 0);
		return;
	}

	change = lines;
	change /= old_lines;

	Debug(DB_WINDOW, "change %lf (old %d / new %d)", change, old_lines, lines);

restart:
	for (top = 0, window = screen_get_window_list(get_current_screen());
	     window; last_window = window, window = window_get_next(window))
	{
		int new_size;

		/* don't adjust via change if we're in extra mode. */
		if (got_extra)
		{
			if (extra)
			{
				extra--;
				/* Already adjusted */
				new_size = window->display_size + 1;
			}
		}
		else
		{
			double adjsize; /* could use fixed-point */

			adjsize = window->display_size;
			adjsize *= change;
			new_size = (int)adjsize;
		}

		Debug(DB_WINDOW, "refnum %d new display size %d (old %d)",
		      window->refnum, new_size, window->display_size);

		/*
		 * calculate the extra lines when we're at the last window.
		 */
		if (--win_count == 0 && (top + new_size != lines))
		{
			extra = lines - (top + new_size);
			Debug(DB_WINDOW, "top[%d] + new_size[%d] != lines[%d], "
					 "saving extra = %d",
			      top, new_size, lines, extra);
		}

		window_set_one_size(window, new_size, top);
		top += new_size + 1;
	}

	if (!extra)
		return;

	/* if extra is negative, we've really gone wrong .. */
	if (extra < 0)
		abort();

	/*
	 * if we had extra, we distribute it out to the windows from the
	 * bottom up, adjusting them as we go.
	 */
	for (window = last_window; extra; --extra, window = window_get_prev(window))
	{
		if (window == NULL)
			window = last_window;
		window->display_size++;
		window->top += extra - 1;
		window->bottom += extra;
	}

	/*
	 * while we still have extra, let's restart and g
	 */
	if (extra)
	{
		got_extra = 1;
		goto restart;
	}
}

/*
 * balance_windows: back end for /window balance.  window sizes are
 * spread as evenly as possible.
 */
void
balance_windows(void)
{
	int	base_size,
		size,
		top,
		extra;
	Window	*tmp;
	
	if (term_basic() || number_of_windows() == 0)
		return;

	base_size = ((get_li() - 1) / number_of_windows()) - 1;
	extra = (get_li() - 1) - ((base_size + 1) * number_of_windows());
	top = 0;
	for (tmp = screen_get_window_list(get_current_screen()); tmp; tmp = window_get_next(tmp))
	{
		tmp->update |= REDRAW_DISPLAY_FULL | REDRAW_STATUS;
		if (extra)
		{
			extra--;
			size = base_size + 1;
		}
		else
			size = base_size;
		tmp->display_size = size - window_menu_lines(tmp)
			- tmp->double_status - SCROLL_DISPLAY_OFFSET;
		if (tmp->display_size <= 0)
			tmp->display_size = 1;
		tmp->top = top;
		tmp->bottom = top + size - tmp->double_status;
		top += size + 1;
	}
}

/*
 * clear_window: This clears the display list for the given window, or
 * current window if null is given.  
 */
void
clear_window(Window *window)
{
	int	i,
		cnt;

	if (term_basic())
		return;
	if (window == NULL)
		window = curr_scr_win;
	erase_display(window);
	term_move_cursor(0, window->top + window_menu_lines(window));
	cnt = window->bottom - window->top - window_menu_lines(window);
	for (i = 0; i < cnt; i++)
	{
		if (term_clear_to_eol())
			term_space_erase(0);
		term_newline();
	}
	term_flush();
}

/* clear_all_windows: This clears all *visible* windows */
void
clear_all_windows(int unhold)
{
	Window	*tmp;

	for (tmp = screen_get_window_list(get_current_screen()); tmp; tmp = window_get_next(tmp))
	{
		if (unhold)
			window_hold_mode(tmp, OFF, 1);
		clear_window(tmp);
	}
}

/*
 * redraw_window: This redraws the display list for the given window. Some
 * special considerations are made if you are just redrawing one window as
 * opposed to using this routine to redraw the whole screen one window at a
 * time 
 *
 * A negative value in just_one indicates not all of the window needs to
 * be redrawn.
 */
static void
redraw_window(Window *window, int just_one, int backscroll)
{
	Display *tmp;
	int	i;
	int	StartPoint;
	int	yScr;

	if (term_basic() || !window->visible)
		return;
	window = window ? window : curr_scr_win;
	if (just_one < 0)
	{
		/* This part of the window is scrolling into view */
		StartPoint = -just_one;
		just_one = 0;
	}
	else
	{
		StartPoint = 0;
		if (window->scrolled_lines)
			display_lastlog_lines(window->scrolled_lines - window->display_size,
			    window->scrolled_lines, window);
	}
	if (menu_active(window_get_menu(window)))
		ShowMenuByWindow(window, just_one ? SMF_ERASE : 0);
	if (window->scrolled_lines + StartPoint < window->display_size)
		yScr = window->scrolled_lines + StartPoint;
	else
		yScr = 0;
	term_move_cursor(0, window->top+window_menu_lines(window)+yScr);
	/*
	 * if (term_clear_to_eol())
	 *	{ term_space_erase(0); term_cr(); } 
	 */
	for (tmp = window->top_of_display, i = 0; i < window->display_size-window->scrolled_lines; i++, tmp = tmp->next)
	{
		if (i < StartPoint)
			continue;
		if (tmp->line)
			rite(window, tmp->line, 1, 1, backscroll, 0);
		else
		{
			if (just_one)
			{
				if (term_clear_to_eol())
					term_space_erase(0);
			}
			term_newline();
		}
	}
	term_flush();
}

/*
 * recalculate_window_positions: This runs through the window list and
 * re-adjusts the top and bottom fields of the windows according to their
 * current positions in the window list.  This doesn't change any sizes of
 * the windows 
 */
void
recalculate_window_positions(void)
{
	Window	*tmp;
	int	top;

	top = 0;
	for (tmp = screen_get_window_list(get_current_screen()); tmp; tmp = window_get_next(tmp))
	{
		tmp->update |= REDRAW_DISPLAY_FULL | REDRAW_STATUS;
		tmp->top = top;
		tmp->bottom = top + tmp->display_size + window_menu_lines(tmp);
		top += tmp->display_size + window_menu_lines(tmp) + 1 +
			tmp->double_status;
	}
}

/*
 * redraw_all_windows: This basically clears and redraws the entire display
 * portion of the screen.  All windows and status lines are draws.  This does
 * nothing for the input line of the screen.  Only visible windows are drawn 
 */
void
redraw_all_windows(void)
{
	Window	*tmp;

	if (term_basic())
		return;
	for (tmp = screen_get_window_list(get_current_screen()); tmp; tmp = window_get_next(tmp))
		tmp->update |= REDRAW_STATUS | REDRAW_DISPLAY_FAST;
}

/*
 * is_current_channel: Returns true is channel is a current channel for any
 * window.  If the delete_window is not 0, then unset channel as the current
 * channel and attempt to replace it by a non-current channel or the 
 * current_channel of window specified by value of delete
 */
int
is_current_channel(u_char *channel, int server, int delete)
{
	Window	*tmp,
		*found_window = NULL;
	Win_Trav wt;
	int	found = 0;

	wt.init = 1;
	while ((tmp = window_traverse(&wt)) != NULL)
	{
		u_char	*c = tmp->current_channel;

		if (c && !my_stricmp(channel, c) && tmp->server == server)
		{
			found_window = tmp;
			found = 1;
			if (delete)
			{
				new_free(&tmp->current_channel);
				tmp->update |= UPDATE_STATUS;
			}
		}
	}
	if (found && delete)
		found = channel_is_on_window(channel, server, found_window, delete);

	return found;
}

Window *
is_bound(u_char *channel, int server)
{
	Win_Trav wt;
	Window *tmp;

	wt.init = 1;
	while ((tmp = window_traverse(&wt)))
	{
		if (tmp->server == server && tmp->bound_channel &&
		    !my_stricmp(channel, tmp->bound_channel))
			return tmp;
	}

	return NULL;
}

static void
bind_channel(u_char *channel, Window *window)
{
	Win_Trav wt;
	Window *tmp;

	/* check it isn't bound on this server elsewhere */
	wt.init = 1;
	while ((tmp = window_traverse(&wt)))
	{
		if (tmp->server != window->server && tmp == window)
			continue;
		if (!my_stricmp(tmp->bound_channel, channel) && tmp != window)
		{
			say("Channel %s is already bound to window %d", channel, tmp->refnum);
			return;
		}
	}
	if (is_on_channel(channel, window->server, server_get_nickname(window->server)))
	{
		is_current_channel(channel, window->server, (int)window->refnum);
		set_channel_by_refnum(0, channel);
	}
	/* XXX fix this */
#if 0
	else
	{
		int	server, sg = -1, fsg = -2;	/* different */
		u_char	*group;

		server = set_from_server(window->server);

		group = server_get_server_group(server);
		if (group)
			sg = find_server_group(group, 0);

		if (sg == 0 || fsg == 0)
			yell("--- huh. coudn't find server groups");

		if (sg == fsg)
		{
			switch (server_get_version(window->server)) {
			case ServerICB:
				icb_put_group(channel);
				break;
			default:
				send_to_server("JOIN %s", channel);
			}
			add_channel(channel, 0, get_from_server(), CHAN_JOINING, NULL);
		}
		set_from_server(server);
	}
#endif
	malloc_strcpy(&window->bound_channel, channel);
	say("Channel %s bound to window %d", channel, window->refnum);
}

static void
unbind_channel(u_char *channel, Window *window)
{

	window = is_bound(channel, window->server);
	if (!window)
		return;
	new_free(&window->bound_channel);
}

/*
 * get_window_server: returns the server index for the window with the given
 * refnum 
 */
int
get_window_server(unsigned refnum)
{
	Window	*tmp;

	if ((tmp = get_window_by_refnum(refnum)) == NULL)
		tmp = curr_scr_win;
	return (tmp->server);
}

/*
 * window_set_server:  This sets the server of the given window to server. 
 * If refnum is -1 then we are setting the primary server and all windows
 * that are set to the current primary server are changed to server.  The misc
 * flag is ignored in this case.  If refnum is not -1, then that window is
 * set to the given server.  If WIN_ALL is set in misc, then all windows
 * with the same server as refnum are set to the new server as well.
 * If the window is in a group, all the group is set to the new server.
 * WIN_TRANSFER will move the channels to the new server, WIN_FORCE will
 * force a 'sticky' behaviour of the window. -Sol
 */
void
window_set_server(int refnum, int server, int misc)
{
	int	old_serv = -2; // -2 for unset
	Window	*window = 0, *ptr, *new_win = NULL;
	int	moved = 0;
	Win_Trav wt;

	if (refnum == -1)
	{
		old_serv = get_primary_server();
		set_primary_server(server);
		misc |= WIN_ALL;
	}
	else
	{
		window = get_window_by_refnum((u_int)refnum);
		if (window)
		{
			old_serv = window->server;
		}
	}

	if (server == old_serv)
		return;

	/* Moving all windows associated with old_serv -Sol */
	if (misc & WIN_ALL)
	{
		if ((misc & WIN_TRANSFER) && (old_serv >= 0))
		{
			moved = channels_move_server_simple(old_serv, server);

#ifdef NON_BLOCKING_CONNECTS
			if (server_get_flag(old_serv, CLOSE_PENDING))
				server_set_flag(old_serv, CLEAR_PENDING, 1);
			else
#endif /* NON_BLOCKING_CONNECTS */
				clear_channel_list(old_serv);
		}
		wt.init = 1;
		while ((ptr = window_traverse(&wt)) != NULL)
			if (ptr->server == old_serv)
			{
				ptr->server = server;
				/*
				 * XXX we could save this to old_current_channel and use
				 * that after other checks to decide where a channel should
				 * go, maybe??
				 */
				if (ptr->current_channel)
					new_free(&ptr->current_channel);
			}
		window_check_servers();
		return;
	}

	/*
	 * We are setting only some windows of the old server : let's look
	 * for a window of that server that is not being moved.
	 * refnum == -1 has been dealt with above so window is defined. -Sol
	 */

	wt.init = 1;
	while ((ptr = window_traverse(&wt)) != NULL)
		if (ptr != window &&
		    (!ptr->server_group || (ptr->server_group != window->server_group)) &&
		    ptr->server == old_serv)
		{
			/* Possible relocation -Sol */
			new_win = ptr;

			/* Immediately retain window if no group -Sol */
			if (!ptr->server_group)
				break;
		}

	/* No relocation : we're closing last windows for old_serv -Sol */
	if (!new_win)
	{
		window_set_server(refnum, server, misc | WIN_ALL);
		return;
	}

	/*
	 * Now that we know that server still has at least one window open,
	 * move what we're supposed to -Sol
	 */

	if ((misc & WIN_TRANSFER) && (old_serv >= 0) && server_get_version(old_serv) != ServerICB)
		channels_move_server_complex(window, new_win, old_serv, server, misc, moved);

	wt.init = 1;
	while ((ptr = window_traverse(&wt)) != NULL)
		if ((ptr == window) || (ptr->server_group && (ptr->server_group == window->server_group)))
		{
			ptr->server = server;
			if (ptr->current_channel)
				new_free(&ptr->current_channel);
		}
		window_check_servers();
}

/*
 * set_channel_by_refnum: This sets the current channel for the current
 * window. It returns the current channel as its value.  If channel is null,
 * the * current channel is not changed, but simply reported by the function
 * result.  This treats as a special case setting the current channel to
 * channel "0".  This frees the current_channel for the
 * get_current_screen()->current_window, * setting it to null 
 */
u_char	*
set_channel_by_refnum(unsigned refnum, u_char *channel)
{
	Window	*tmp;
	Window *tmp2;
	Win_Trav wt;

	if ((tmp = get_window_by_refnum(refnum)) == NULL)
		tmp = curr_scr_win;
	if (channel && my_strcmp(channel, zero()) == 0)
		channel = NULL;

	wt.init = 1;
	while ((tmp2 = window_traverse(&wt)))
		if (tmp2->server == tmp->server && my_stricmp(tmp2->current_channel, channel) == 0)
			new_free(&tmp2->current_channel);

	malloc_strcpy(&tmp->current_channel, channel);
	tmp->update |= UPDATE_STATUS;
	set_channel_window(tmp, channel, tmp->server);
	return (channel);
}

/* get_channel_by_refnum: returns the current channel for window refnum */
u_char	*
get_channel_by_refnum(u_int refnum)
{
	Window	*tmp;

	if ((tmp = get_window_by_refnum(refnum)) == NULL)
		tmp = curr_scr_win;
	return (tmp->current_channel);
}

/* current_refnum: returns the reference number for the current window */
unsigned int
current_refnum(void)
{
	return (curr_scr_win->refnum);
}

/* query_nick: Returns the query nick for the current channel */
u_char	*
query_nick(void)
{
	return (curr_scr_win->query_nick);
}

/* get_prompt_by_refnum: returns the prompt for the given window refnum */
u_char	*
get_prompt_by_refnum(u_int refnum)
{
	Window	*tmp;

	if ((tmp = get_window_by_refnum(refnum)) == NULL)
		tmp = curr_scr_win;
	if (tmp->prompt)
		return (tmp->prompt);
	else
		return (empty_string());
}

/*
 * get_target_by_refnum: returns the target for the window with the given
 * refnum (or for the current window).  The target is either the query nick
 * or current channel for the window 
 */
u_char	*
get_target_by_refnum(u_int refnum)
{
	Window	*tmp;

	if ((tmp = get_window_by_refnum(refnum)) == NULL)
		tmp = curr_scr_win;
	if (tmp->query_nick)
		return tmp->query_nick;
	else if (tmp->current_channel)
		return tmp->current_channel;
	else
		return NULL;
}

/* set_query_nick: sets the query nick for the current channel to nick */
void
set_query_nick(u_char *nick)
{
	if (curr_scr_win->query_nick)
	{
		new_free(&curr_scr_win->query_nick);
		curr_scr_win->update |= UPDATE_STATUS;
	}

	if (nick)
	{
		malloc_strcpy(&curr_scr_win->query_nick, nick);
		curr_scr_win->update |= UPDATE_STATUS;
	}
	update_window_status(curr_scr_win, 0);
}

/*
 * irc_goto_window: This will switch the current window to the window numbered
 * "which", where which is 0 through the number of visible windows on the
 * screen.  The which has nothing to do with the windows refnum. 
 */
static	void
irc_goto_window(int which)
{
	Window	*tmp;
	int	i;


	if (which == 0)
		return;
	if ((which < 0) || (which > number_of_windows()))
	{
		say("GOTO: Illegal value");
		return;
	}
	tmp = screen_get_window_list(get_current_screen());
	for (i = 1; tmp && (i != which); tmp = tmp->next, i++)
		;
	set_current_window(tmp);
}

/*
 * hide_window: sets the given window to invisible and recalculates remaing
 * windows to fill the entire screen 
 */
static void
hide_window(Window *window)
{
	if (number_of_windows() == 1)
	{
		say("You can't hide the last window.");
		return;
	}
	if (window->visible)
	{
		remove_from_window_list(window);
		add_to_invisible_list(window);
		window->display_size = get_li() - 2 - SCROLL_DISPLAY_OFFSET;
		set_current_window(NULL);
	}
}

/* hide_other_windows: makes all visible windows but the current one hidden */
static void
hide_other_windows(void)
{
	Window	*tmp,
		*cur,
		*next;

	cur = curr_scr_win;
	tmp = screen_get_window_list(get_current_screen());
	while (tmp)
	{
		next = tmp->next;
		if (tmp != cur)
			hide_window(tmp);
		tmp = next;
	}
}

#define WIN_FORM "%%-4s %%-%u.%us %%-%u.%us  %%-%u.%us %%-9.9s %%-10.10s %%s%%s"
#define WIN_FORM_HOOK "%d %s %s %s %s %s %s %s %s"

static	void
list_a_window(Window *window, int len, int clen)
{
	u_char	tmp[10];

	snprintf(CP(tmp), sizeof tmp, "%-4u", window->refnum);
	if (do_hook(WINDOW_LIST_LIST, WIN_FORM_HOOK,
		    len,
		    tmp,
		    window->server ?
			server_get_nickname(window->server) : (u_char *) "<None>",
		    window->name ?
			window->name : (u_char *) "<None>",
		    window->current_channel ?
			window->current_channel : (u_char *) "<None>",
		    window->query_nick ?
			window->query_nick : (u_char *) "<None>",
		    window->server != -1 ?
			server_get_itsname(window->server) : (u_char *) "<None>",
		    bits_to_lastlog_level(window->window_level),
		    (window->visible) ?
			"" : " Hidden"))
	{
		const	unsigned old_nick_len = 9;
		u_char	buffer[BIG_BUFFER_SIZE];

		snprintf(CP(buffer), sizeof buffer, WIN_FORM, old_nick_len,
		    old_nick_len, len, len, clen, clen);
		say(CP(buffer),
		    tmp,
		    window->server ?
			server_get_nickname(window->server) : (u_char *) "<None>",
		    window->name ?
			window->name : (u_char *) "<None>",
		    window->current_channel ?
			window->current_channel : (u_char *) "<None>",
		    window->query_nick ?
			window->query_nick : (u_char *) "<None>",
		    window->server != -1 ?
			server_get_itsname(window->server) : (u_char *) "<None>",
		    bits_to_lastlog_level(window->window_level),
		    (window->visible) ?
			"" : " Hidden");
	}
}

/*
 * list_windows: This Gives a terse list of all the windows, visible or not,
 * by displaying their refnums, current channel, and current nick 
 */
static	void
list_windows(void)
{
	Window	*tmp;
	Win_Trav wt;
	int	len = 4;
	int	clen = get_int_var(CHANNEL_NAME_WIDTH_VAR);
	int	check_clen = clen == 0;

	wt.init = 1;
	while ((tmp = window_traverse(&wt)) != NULL)
	{
		if (tmp->name && ((int) my_strlen(tmp->name) > len))
			len = my_strlen(tmp->name);
		if (check_clen == 0)
			continue;
		if (tmp->current_channel &&
		    ((int) my_strlen(tmp->current_channel) > clen))
			clen = my_strlen(tmp->current_channel);
	}
	if (do_hook(WINDOW_LIST_LIST, WIN_FORM_HOOK, len,
		    "Ref", "Nick", "Name", "Channel", "Query",
		    "Server", "Level", empty_string()))
	{
		const	unsigned old_nick_len = 9;
		u_char	buffer[BIG_BUFFER_SIZE];

		snprintf(CP(buffer), sizeof buffer, WIN_FORM, old_nick_len,
		    old_nick_len, len, len, clen, clen);
		say(CP(buffer),
		    "Ref", "Nick", "Name", "Channel", "Query",
		    "Server", "Level", empty_string());
	}
	wt.init = 1;
	while ((tmp = window_traverse(&wt)) != NULL)
		list_a_window(tmp, len, clen);
}

/* show_window: This makes the given window visible.  */
static	void
show_window(Window *window)
{
	if (window->visible)
	{
		set_current_window(window);
		return;
	}
	remove_from_invisible_list(window);
	if (add_to_window_list(window))
		set_current_window(window);
	else
		add_to_invisible_list(window);
}

/* push_window_by_refnum:  This pushes the given refnum onto the window stack */
static	void
push_window_by_refnum(u_int refnum)
{
	WindowStack *new;

	new = new_malloc(sizeof *new);
	new->refnum = refnum;
	new->next = get_window_stack();
	set_window_stack(new);
}

/*
 * pop_window: this pops the top entry off the window stack and sets the
 * current window to that window.  If the top of the window stack is no
 * longer a valid window, then that entry is discarded and the next entry
 * down is tried (as so on).  If the stack is empty, the current window is
 * left unchanged 
 */
static	void
pop_window(void)
{
	int	refnum;
	WindowStack *tmp;
	Window	*win;

	while (1)
	{
		WindowStack *window_stack = get_window_stack();

		if (window_stack)
		{
			refnum = window_stack->refnum;
			tmp = window_stack->next;
			new_free(&window_stack);
			set_window_stack(tmp);
			if ((win = get_window_by_refnum((u_int)refnum)) != NULL)
			{
				if (!win->visible)
					show_window(win);
				else
					set_current_window(win);
				break;
			}
		}
		else
		{
			say("The window stack is empty!");
			break;
		}
	}
}

/*
 * show_stack: displays the current window stack.  This also purges out of
 * the stack any window refnums that are no longer valid 
 */
static	void
show_stack(void)
{
	WindowStack *last = NULL,
	    *tmp, *crap;
	Window	*win;
	Win_Trav wt;
	int	len = 4;
	int	clen = get_int_var(CHANNEL_NAME_WIDTH_VAR);
	int	check_clen = clen == 0;

	wt.init = 1;
	while ((win = window_traverse(&wt)) != NULL)
	{
		if (win->name && ((int) my_strlen(win->name) > len))
			len = my_strlen(win->name);
		if (check_clen == 0)
			continue;
		if (win->current_channel && ((int) my_strlen(win->current_channel) > clen))
			clen = my_strlen(win->current_channel);
	}
	say("Window stack:");
	tmp = get_window_stack();
	while (tmp)
	{
		if ((win = get_window_by_refnum(tmp->refnum)) != NULL)
		{
			list_a_window(win, len, clen);
			tmp = tmp->next;
		}
		else
		{
			crap = tmp->next;
			new_free(&tmp);
			if (last)
				last->next = crap;
			else
				set_window_stack(crap);
			tmp = crap;
		}
	}
}

/*
 * create_refnum: this generates a reference number for a new window that is
 * not currently is use by another window.  A refnum of 0 is reserved (and
 * never returned by this routine).  Using a refnum of 0 in the message_to()
 * routine means no particular window (stuff goes to CRAP) 
 */
static	u_int
create_refnum(void)
{
	unsigned int	new_refnum = 1;
	Window	*tmp;
	int	done = 0;

	while (!done)
	{
		Win_Trav wt;

		done = 1;
		if (new_refnum == 0)
			new_refnum++;

		wt.init = 1;
		while ((tmp = window_traverse(&wt)) != NULL)
		{
			if (window_get_refnum(tmp) == new_refnum)
			{
				done = 0;
				new_refnum++;
				break;
			}
		}
	}
	return (new_refnum);
}

/*
 * new_window: This creates a new window on the screen.  It does so by either
 * splitting the current window, or if it can't do that, it splits the
 * largest window.  The new window is added to the window list and made the
 * current window 
 *
 * move this into window.c.
 */
Window	*
new_window(void)
{
	Window	*new;
	static	int	no_screens = 1;

	if (no_screens)
	{
		set_current_screen(create_new_screen());
		set_main_screen(get_current_screen());
		no_screens = 0;
	}
	if (term_basic() && (number_of_windows() == 1))
		return NULL;
	new = new_malloc(sizeof *new);
	new->refnum = create_refnum();
	if (curr_scr_win)
		new->server = curr_scr_win->server;
	else
		new->server = get_primary_server();
	new->prev_server = -1;
	new->line_cnt = 0;
	if (number_of_windows() == 0)
		new->window_level = LOG_DEFAULT;
	else
		new->window_level = LOG_NONE;
	new->hold_mode = get_int_var(HOLD_MODE_VAR);
	new->hold_info = alloc_hold_info();
	new->scroll = get_int_var(SCROLL_VAR);
	new->lastlog_info = lastlog_new_window();
	new->nicks = 0;
	new->name = 0;
	new->prompt = 0;
	new->current_channel = 0;
	new->bound_channel = 0;
	new->query_nick = 0;
	new->hold_on_next_rite = 0;
	new->status_line[0] = NULL;
	new->status_line[1] = NULL;
	new->double_status = 0;
	new->top_of_display = 0;
	new->display_ip = 0;
	new->display_size = 1;
	new->old_size = 1;
	new->scrolled_lines = 0;
	new->new_scrolled_lines = 0;
	new->next = 0;
	new->prev = 0;
	new->cursor = 0;
	new->visible = 1;
	new->screen = get_current_screen();
	new->logfile = 0;
	new->log = 0;
	new->log_fp = 0;
	new->miscflags = 0;
	new->update = 0;
	new->menu = menu_init();
	new->notify_level = real_notify_level();
	new->server_group = 0;
	new->sticky = 1;
	resize_display(new);
	if (add_to_window_list(new))
		set_current_window(new);
	term_flush();
	return (new);
}

/*
 * is_window_name_unique: checks the given name vs the names of all the
 * windows and returns true if the given name is unique, false otherwise 
 */
static	int
is_window_name_unique(u_char *name)
{
	Window	*tmp;
	Win_Trav wt;

	if (name)
	{
		wt.init = 1;
		while ((tmp = window_traverse(&wt)) != NULL)
		{
			if (tmp->name && (my_stricmp(tmp->name, name) == 0))
				return (0);
		}
	}
	return (1);
}

/*
 * free_display: This frees all memory for the display list for a given
 * window.  It resets all of the structures related to the display list
 * appropriately as well 
 */
void
free_display(Window *window)
{
	Display *tmp, *next;
	int	i;

	if (window == NULL)
		window = curr_scr_win;
	for (tmp = window->top_of_display, i = 0; i < window->display_size - window->double_status; i++, tmp = next)
	{
		next = tmp->next;
		new_free(&(tmp->line));
		new_free(&tmp);
	}
	window->top_of_display = NULL;
	window->display_ip = NULL;
	window->display_size = 0;
}

/*
 * erase_display: This effectively causes all members of the display list for
 * a window to be set to empty strings, thus "clearing" a window.  It sets
 * the cursor to the top of the window, and the display insertion point to
 * the top of the display. Note, this doesn't actually refresh the screen,
 * just cleans out the display list 
 */
void
erase_display(Window *window)
{
	int	i;
	Display *tmp;

	if (term_basic())
		return;
	if (window == NULL)
		window = curr_scr_win;
	for (tmp = window->top_of_display, i = 0; i < window->display_size;
			i++, tmp = tmp->next)
		new_free(&(tmp->line));
	window->cursor = 0;
	window->line_cnt = 0;
	window->hold_on_next_rite = 0;
	window->display_ip = window->top_of_display;
}

void
window_add_display_line(Window *window, u_char *str, int logged)
{
	if (window->scroll)
		scroll_window(window);
	malloc_strcpy(&window->display_ip->line, str);
	window->display_ip->linetype = logged;
	window->display_ip = window->display_ip->next;
	if (!window->scroll)
		new_free(&window->display_ip->line);
}

/*
 * add_nicks_by_refnum:  This adds the given str to the nicklist of the
 * window refnum.  Only unique names are added to the list.  If add is zero
 * or the name is preceeded by a ^ it is removed from the list.  The str
 * may be a comma separated list of nicks, channels, etc .
 */
static	void
add_nicks_by_refnum(u_int refnum, u_char *str, int add)
{
	Window	*tmp;
	u_char	*ptr;

	if ((tmp = get_window_by_refnum(refnum)) != NULL)
	{
		while (str)
		{
			if ((ptr = my_index(str, ',')) != NULL)
				*(ptr++) = '\0';
			if (add == 0 || *str == '^')
			{
				if (add == 0 && *str == '^')
					str++;
				if (nicks_remove_from_window(tmp, str))
					say("%s removed from window name list", str);
				else
					say("%s is not on the list for this window!", str);
			}
			else
			{
				if (nicks_add_to_window(tmp, str))
					say("%s add to window name list", str);
				else
					say("%s already on window name list", str);
			}
			str = ptr;
		}
	}
	else
		say("No such window!");
}

/* below is stuff used for parsing of WINDOW command */

/*
 * get_window_by_name: returns a pointer to a window with a matching logical
 * name or null if no window matches 
 */
Window	*
get_window_by_name(u_char *name)
{
	Window	*tmp;
	Win_Trav wt;

	wt.init = 1;
	while ((tmp = window_traverse(&wt)) != NULL)
	{
		if (tmp->name && (my_stricmp(tmp->name, name) == 0))
			return (tmp);
	}
	return (NULL);
}

/*
 * get_window: this parses out any window (visible or not) and returns a
 * pointer to it 
 */
static	Window	*
get_window(u_char *name, u_char **args)
{
	u_char	*arg;
	Window	*tmp;

	if ((arg = next_arg(*args, args)) != NULL)
	{
		if (is_number(arg))
		{
			if ((tmp = get_window_by_refnum((u_int)my_atoi(arg))) != NULL)
				return (tmp);
		}
		if ((tmp = get_window_by_name(arg)) != NULL)
			return (tmp);
		say("%s: No such window: %s", name, arg);
	}
	else
		say("%s: Please specify a window refnum or name", name);
	return (NULL);
}

/*
 * get_invisible_window: parses out an invisible window by reference number.
 * Returns the pointer to the window, or null.  The args can also be "LAST"
 * indicating the top of the invisible window list (and thus the last window
 * made invisible) 
 */
static	Window	*
get_invisible_window(u_char *name, u_char **args)
{
	u_char	*arg;
	Window	*tmp;

	if ((arg = next_arg(*args, args)) != NULL)
	{
		if (my_strnicmp(arg, UP("LAST"), my_strlen(arg)) == 0)
		{
			if (invisible_list == NULL)
				say("%s: There are no hidden windows", name);
			return (invisible_list);
		}
		if ((tmp = get_window(name, &arg)) != NULL)
		{
			if (!tmp->visible)
				return (tmp);
			else
			{
				if (tmp->name)
					say("%s: Window %s is not hidden!",
						name, tmp->name);
				else
					say("%s: Window %d is not hidden!",
						name, tmp->refnum);
			}
		}
	}
	else
		say("%s: Please specify a window refnum or LAST", name);
	return (NULL);
}

/* get_number: parses out an integer number and returns it */
static	int
get_number(u_char *name, u_char **args)
{
	u_char	*arg;

	if ((arg = next_arg(*args, args)) != NULL)
		return (my_atoi(arg));
	else
		say("%s: You must specify the number of lines", name);
	return (0);
}

/*
 * get_boolean: parses either ON, OFF, or TOGGLE and sets the var
 * accordingly.  Returns 0 if all went well, -1 if a bogus or missing value
 * was specified 
 */
static	int
get_boolean(u_char *name, u_char **args, int *var)
{
	u_char	*arg;

	if (((arg = next_arg(*args, args)) == NULL) ||
	    do_boolean(arg, var))
	{
		say("Value for %s must be ON, OFF, or TOGGLE", name);
		return (-1);
	}
	else
	{
		say("Window %s is %s", name, var_settings(*var));
		return (0);
	}
}

void
windowcmd(u_char *command, u_char *args, u_char *subargs)
{
	size_t	len;
	u_char	*arg,
		*cmd = NULL,
		*proxy_name = NULL,
		*proxy_port_str;
	u_char	buffer[BIG_BUFFER_SIZE];
	int	no_args = 1,
		proxy_port = 0;
	Window	*window,
		*tmp;

	update_all_windows();
	save_message_from();
	message_from(NULL, LOG_CURRENT);
	window = curr_scr_win;
	while ((arg = next_arg(args, &args)) != NULL)
	{
		no_args = 0;
		len = my_strlen(arg);
		malloc_strcpy(&cmd, arg);
		upper(cmd);
		if (my_strncmp("NEW", cmd, len) == 0)
		{
			if ((tmp = new_window()) != NULL)
				window = tmp;
		}
#ifdef WINDOW_CREATE
		else if (my_strncmp("CREATE", cmd, len) == 0)
		{
			int type;

			if (*args == '-' &&
			    (arg = next_arg(args, &args)) != NULL)
			{
				if (my_stricmp(arg, UP("-SCREEN")) == 0)
					type = ST_SCREEN;
				else if (my_stricmp(arg, UP("-XTERM")) == 0)
					type = ST_XTERM;
				else {
					say("Unknown /WINDOW CREATE option");
					continue;
				}
			}
			else
				type = ST_NOTHING;
			if ((tmp = create_additional_screen(type)) != NULL)
				window = tmp;
			else
				say("Cannot create new screen!");
		}
		else if (!my_strncmp("DELETE", cmd, len))
			kill_screen(get_current_screen());
#endif /* WINODW_CREATE */
		else if (my_strncmp("REFNUM", cmd, len) == 0)
		{
			if ((tmp = get_window(UP("REFNUM"), &args)) != NULL)
			{
				if (tmp->screen && tmp->screen !=window->screen)
					say("Window in another screen!");
				else if (tmp->visible)
				{ 
					set_current_window(tmp);
					window = tmp;
				}
				else
					say("Window not visible!");
			}
			else
			{
				say("No such window!");
				new_free(&cmd);
				goto out;
			}
		}
		else if (my_strncmp("KILL", cmd, len) == 0)
			delete_window(window);
		else if (my_strncmp("SHRINK", cmd, len) == 0)
			grow_window(window, -get_number(UP("SHRINK"), &args));
		else if (my_strncmp("GROW", cmd, len) == 0)
			grow_window(window, get_number(UP("SHRINK"), &args));
		else if (my_strncmp("SCROLL", cmd, len) == 0)
			get_boolean(UP("SCROLL"), &args, &(window->scroll));
		else if (my_strncmp("STICKY", cmd, len) == 0)
			get_boolean(UP("STICKY"), &args, &(window->sticky));
		else if (my_strncmp("LOG", cmd, len) == 0)
		{
			if (get_boolean(UP("LOG"), &args, &(window->log)))
				break;
			else
			{
				u_char	*logfile;
				int	add_ext = 1;

				if ((logfile = window->logfile) != NULL)
					add_ext = 0;
				else if (!(logfile = get_string_var(LOGFILE_VAR)))
					logfile = empty_string();
				if (!add_ext)
					snprintf(CP(buffer), sizeof buffer, "%s", logfile);
				else if (window->current_channel)
					snprintf(CP(buffer), sizeof buffer, "%s.%s", logfile, window->current_channel);
				else if (window->query_nick)
					snprintf(CP(buffer), sizeof buffer, "%s.%s", logfile, window->query_nick);
				else
					snprintf(CP(buffer), sizeof buffer, "%s.Window_%d", logfile, window->refnum);
				do_log(window->log, buffer, &window->log_fp);
				if (window->log_fp == NULL)
					window->log = 0;
			}
		}
		else if (my_strncmp("HOLD_MODE", cmd, len) == 0)
			get_boolean(UP("HOLD_MODE"), &args, &window->hold_mode);
		else if (my_strncmp("LASTLOG_LEVEL", cmd, len) == 0)
		{
			if ((arg = next_arg(args, &args)) != NULL)
			{
				int lastlog_level = parse_lastlog_level(arg);
				lastlog_set_level(window->lastlog_info,
						  lastlog_level);
				say("Lastlog level is %s",
				    bits_to_lastlog_level(lastlog_level));
			}
			else
				say("Level required");
		}
		else if (my_strncmp("LEVEL", cmd, len) == 0)
		{
			if ((arg = next_arg(args, &args)) != NULL)
			{
				window->window_level = parse_lastlog_level(arg);
				say("Window level is %s",
				   bits_to_lastlog_level(window->window_level));
				revamp_window_levels(window);
			}
			else
				say("LEVEL: Level required");
		}
		else if (my_strncmp("BALANCE", cmd, len) == 0)
			balance_windows();
		else if (my_strncmp("NAME", cmd, len) == 0)
		{
			if ((arg = next_arg(args, &args)) != NULL)
			{
				if (is_window_name_unique(arg))
				{
					malloc_strcpy(&window->name, arg);
					window->update |= UPDATE_STATUS;
				}
				else
					say("%s is not unique!", arg);
			}
			else
				say("You must specify a name for the window!");
		}
		else if (my_strncmp("PROMPT", cmd, len) == 0)
		{
			if ((arg = next_arg(args, &args)) != NULL)
			{
				malloc_strcpy(&window->prompt, arg);
				window->update |= UPDATE_STATUS;
			}
			else
			    say("You must specify a prompt for the window!");
		}
		else if (my_strncmp("GOTO", cmd, len) == 0)
		{
			irc_goto_window(get_number(UP("GOTO"), &args));
			window = curr_scr_win;
		}
		else if (my_strncmp("LAST", cmd, len) == 0)
			set_current_window(NULL);
		else if (my_strncmp("MOVE", cmd, len) == 0)
		{
			move_window(window, get_number(UP("MOVE"), &args));
			window = curr_scr_win;
		}
		else if (my_strncmp("SWAP", cmd, len) == 0)
		{
			if ((tmp = get_invisible_window(UP("SWAP"), &args)) != NULL)
				swap_window(window, tmp);
		}
		else if (my_strncmp("HIDE", cmd, len) == 0)
			hide_window(window);
		else if (my_strncmp("PUSH", cmd, len) == 0)
			push_window_by_refnum(window->refnum);
		else if (my_strncmp("POP", cmd, len) == 0)
			pop_window();
		else if (my_strncmp("ADD", cmd, len) == 0)
		{
			if ((arg = next_arg(args, &args)) != NULL)
				add_nicks_by_refnum(window->refnum, arg, 1);
			else
				say("ADD: Do something!  Geez!");
		}
		else if (my_strncmp("REMOVE", cmd, len) == 0)
		{
			if ((arg = next_arg(args, &args)) != NULL)
				add_nicks_by_refnum(window->refnum, arg, 0);
			else
				say("REMOVE: Do something!  Geez!");
		}
		else if (my_strncmp("STACK", cmd, len) == 0)
			show_stack();
		else if (my_strncmp("LIST", cmd, len) == 0)
			list_windows();
		else if (my_strncmp("SERVER", cmd, len) == 0)
		{
			if ((arg = next_arg(args, &args)) != NULL)
			{
				u_char	*group = 0;
				int	type = Server2_8;
				server_ssl_level level = SSL_OFF;

				while (*arg == '-')
				{
					if (my_stricmp(UP("-ICB"), arg) == 0)
						type = ServerICB;
					else if (my_stricmp(UP("-IRC"), arg) == 0)
						type = Server2_8;
					else if (my_stricmp(UP("-SSL"), arg) == 0)
						level = SSL_VERIFY;
					else if (my_stricmp(UP("-SSLNOCHECK"), arg) == 0)
						level = SSL_ON;
					else if (my_stricmp(UP("-NOSSL"), arg) == 0)
						level = SSL_OFF;
					else if (my_stricmp(UP("-GROUP"), arg) == 0)
					{
						if ((group = next_arg(args, &args)) == NULL)
						{
							say("SERVER -GROUP needs <group> and <server>");
							return;
						}
					}
					else if (my_stricmp(UP("-PROXY"), arg) == 0)
					{
						if ((proxy_name = next_arg(args, &args)) == NULL ||
						    (proxy_port_str = next_arg(args, &args)) == NULL)
						{
							say("SERVER -PROXY needs <proxy> and <host>");
							return;
						}
						proxy_port = my_atoi(proxy_port_str);
					}
					else
						say("SERVER: %s: unknown flag", arg);
					if ((arg = next_arg(args, &args)) == NULL)
						say("SERVER: You must specify a server");
				}
				window_get_connected(window, arg, -1, args,
					proxy_name, proxy_port, group, type, level);
			}
			else
				say("SERVER: You must specify a server");
		}
		else if (my_strncmp("SHOW", cmd, len) == 0)
		{
			if ((tmp = get_window(UP("SHOW"), &args)) != NULL)
			{
				show_window(tmp);
				window = curr_scr_win;
			}
		}
		else if (my_strncmp("HIDE_OTHERS", cmd, len) == 0)
			hide_other_windows();
		else if (my_strncmp("KILL_OTHERS", cmd, len) == 0)
			delete_other_windows();
		else if (my_strncmp("NOTIFY", cmd, len) == 0)
		{
			window->miscflags ^= WINDOW_NOTIFY;
			say("Notification when hidden set to %s",
			    (window->miscflags & WINDOW_NOTIFY)? "ON" : "OFF");
		}
		else if (my_strncmp("WHERE", cmd, len) == 0)
			window_list_channels(curr_scr_win);
		else if (my_strncmp("QUERY", cmd, len) == 0)
		{
			u_char *a = 0;

			a = next_arg(args, &args);
			query(cmd, a, 0);
		}
		else if (my_strncmp("CHANNEL", cmd, len) == 0)
		{
			if ((arg = next_arg(args, &args)) != NULL)
			{
				u_char *key, *t;

				t = arg;
				arg = my_strsep(&t, UP(","));
				if ((key = my_strsep(&t, UP(", "))) == 0)
					key = 0;

				if (is_bound(arg, window->server))
				{
					say("Channel %s is bound", arg);
				}
				else
				{
					if (is_on_channel(arg, window->server,
						server_get_nickname(window->server)))
					{
						is_current_channel(arg, window->server,
							(int)window->refnum);
						say("You are now talking to channel %s", arg);
						set_channel_by_refnum(0, arg);
					}
					else if (*arg == '0' && !*(arg + 1))
						set_channel_by_refnum(0, NULL);
					else
					{
						int	server;

						server = set_from_server(window->server);
						switch (server_get_version(window->server)) {
						case ServerICB:
							icb_put_group(arg);
							break;
						default:
							send_to_server("JOIN %s%s%s", arg,
							    key ? (u_char *) " " : empty_string(),
							    key ? key : empty_string());
						}
						add_channel(arg, key, window->server, CHAN_JOINING, NULL);
						set_from_server(server);
					}
				}
			}
			else
				set_channel_by_refnum(0, zero());
		}
		else if (my_strncmp("PREVIOUS", cmd, len) == 0)
		{
			swap_previous_window(0, NULL);
		}
		else if (my_strncmp("NEXT", cmd, len) == 0)
		{
			swap_next_window(0, NULL);
		}
		else if (my_strncmp("BACK", cmd, len) == 0)
		{
			back_window(0, NULL);
		}
		else if (my_strncmp("KILLSWAP", cmd, len) == 0)
		{
			window_kill_swap();
		}
		else if (my_strncmp("LOGFILE", cmd, len) == 0)
		{
			if ((arg = next_arg(args, &args)) != NULL)
			{
				malloc_strcpy(&window->logfile, arg);
				say("Window LOGFILE set to %s", arg);
			}
			else
				say("No LOGFILE given");
		}
		else if (my_strncmp("NOTIFY_LEVEL", cmd, len) == 0)
		{
			if ((arg = next_arg(args, &args)) != NULL)
			{
				window->notify_level = parse_lastlog_level(arg);
				say("Window notify level is %s",
				   bits_to_lastlog_level(window->notify_level));
			}
			else
				say("Level missing");
		}
		else if (my_strncmp("NUMBER", cmd, len) == 0)
		{
			if ((arg = next_arg(args, &args)) != NULL)
			{
				int	i;
				Window	*wtmp;

				i = my_atoi(arg);
				if (i > 0)
				{
					/* check if window number exists */
						
					wtmp = get_window_by_refnum((u_int)i);
					if (!wtmp)
						window->refnum = i;
					else
					{
						wtmp->refnum = window->refnum;
						window->refnum = i;
					}
					update_all_status();
				}
				else
					say("Window number must be greater "
					    "than 1");
			}
			else
				say("Window number missing");
		}
		else if (my_strncmp("BIND", cmd, len) == 0)
		{
			if ((arg = next_arg(args, &args)) != NULL)
			{
				if (!is_channel(arg))
					say("BIND: %s is not a valid "
					    "channel name", arg);
				else
				{
					bind_channel(arg, window);
				}
			}
			else
				if (window->bound_channel)
					say("Channel %s is bound to window %d",
					    window->bound_channel,
					    window->refnum);
		}
		else if (my_strncmp("UNBIND", cmd, len) == 0)
		{
			if ((arg = next_arg(args, &args)) != NULL)
			{
				if (is_bound(arg, window->server))
				{
					say("Channel %s is no longer bound",
					    arg);
					unbind_channel(arg, window);
				}
				else
					say("Channel %s is not bound", arg);
			}
			else
				say("UNBIND: You must specify a channel name");
		}
		else if (my_strncmp("ADDGROUP", cmd, len) == 0)
		{
			if ((arg = next_arg(args, &args)) != NULL)
				add_window_to_server_group(window, arg);
			else
				say("WINDOW ADDGROUP requires a group name");
		}
		else if (my_strncmp("DELGROUP", cmd, len) == 0)
		{
			window->server_group = 0;
			say("Window no longer has a server group");
			update_window_status(window, 1);
		}
		else if (my_strncmp("DOUBLE", cmd, len) == 0)
		{
			int current = window->double_status;

			if (get_boolean(UP("DOUBLE"), &args,
					&window->double_status) == 0)
			{
				window->display_size += current -
					window->double_status;
				recalculate_window_positions();
				update_all_windows();
				build_status((u_char *) NULL);
			}
		}
		else if (my_strncmp("NOSTATUS", cmd, len) == 0)
		{
			int current = window->double_status;

			window->double_status = -1;
			window->display_size += current - window->double_status;
			recalculate_window_positions();
			update_all_windows();
			build_status((u_char *) NULL);
		}
		else
			say("Unknown WINDOW command: %s", arg);
		new_free(&cmd);
	}
	if (no_args)
	{
		if (window->name)
			say("Window %s (%u)", window->name, window->refnum);
		else
			say("Window %u", window->refnum);
		if (window->server == -1)
			say("\tServer: <None>");
		else
			say("\tServer: %s", server_get_name(window->server));
		say("\tCurrent channel: %s",
		    window->current_channel ?
			window->current_channel : UP("<None>"));
		say("\tQuery User: %s",
		    window->query_nick ? window->query_nick : UP("<None>"));
		say("\tPrompt: %s",
		    window->prompt ?  window->prompt : (u_char *) "<None>");
		say("\tSecond status line is %s",
		    var_settings(window->double_status));
		say("\tScrolling is %s", var_settings(window->scroll));
		say("\tLogging is %s", var_settings(window->log));
		if (window->logfile)
			say("\tLogfile is %s", window->logfile);
		else
			say("\tNo logfile given");
		say("\tNotification is %s",
		    var_settings(window->miscflags & WINDOW_NOTIFY));
		say("\tHold mode is %s", var_settings(window->hold_mode));
		say("\tSticky behaviour is %s", var_settings(window->sticky));
		say("\tWindow level is %s",
		    bits_to_lastlog_level(window->window_level));
		say("\tLastlog level is %s",
		    bits_to_lastlog_level(lastlog_get_level(window->lastlog_info)));
		say("\tNotify level is %s",
		    bits_to_lastlog_level(window->notify_level));
		if (window->server_group)
			say("\tServer Group is (%d) %s", window->server_group,
			    find_server_group_name(window->server_group));
		if (window->bound_channel)
			say("\tBound Channel is %s", window->bound_channel);
		display_nicks_info(window);
	}
out:
	restore_message_from();
	update_all_windows();
	cursor_to_input();
}

int
number_of_windows(void)
{
	return screen_get_visible_windows(get_current_screen());
}

void
unstop_all_windows(u_int key, u_char *ptr)
{
	Window	*tmp;

	for (tmp = screen_get_window_list(get_current_screen()); tmp;
	     tmp = tmp->next)
		window_hold_mode(tmp, OFF, 1);
}

/* this will make underline toggle between 2 and -1 and never let it get to 0 */
void
set_underline_video(int value)
{
	if (value == OFF)
		set_underline(-1);
	else
		set_underline(1);
}

/*
 * if "arg" is NULL, nargs is a server to connect to.
 */
void
window_get_connected(Window *window, u_char *arg, int narg, u_char *args,
		     u_char *proxy_name, int proxy_port,
		     u_char *group, int type, server_ssl_level level)
{
	int	i,
		port_num,
		new_server_flags = WIN_TRANSFER;
	u_char	*port,
		*password = NULL,
		*nick = NULL,
		*extra = NULL,
		*icbmode = NULL;

	if (arg)
	{
		if (*arg == '=')
		{
			new_server_flags |= WIN_ALL;
			arg++;
		}
		else if (*arg == '~')
		{
			new_server_flags |= WIN_FORCE;
			arg++;
		}
		/*
		 * work in progress.. window->prev_server needs to be set for
		 * all windows that used to be associated with a server as it
		 * switches [successfully] to a new server.
		 * this'll be fun since that can happen in server.c and
		 * window.c and non-blocking-connects will throw yet another
		 * wrench into things since we only want it to happen on
		 * a successful connect. - gkm
		 */
		else if (*arg == '.')
		{
			if (*(++arg))
			{
				say("syntax error - nothing may be specified "
				    "after the '.'");
				return;
			}
			if (window->prev_server != -1)
			{
				if (group)
					add_server_to_server_group(
						window->prev_server, group);

				window_restore_server(window->prev_server);
				if (!proxy_name)
					proxy_name = server_get_proxy_name(
						window->prev_server, 0);
				if (proxy_port == 0)
					proxy_port = server_get_proxy_port(
						window->prev_server, 0);
				window_get_connected(window, NULL,
					window->server, NULL,
					proxy_name, proxy_port,
					group, type, level);
			}
			else
				say("No server previously in use in this window");
			return;
                }
		parse_server_info(&arg, &port, &password, &nick,
		    group ? 0 : &group, &extra, &type, &level,
		    &proxy_name, &proxy_port);
		if (port)
		{
			port_num = my_atoi(port);
			if (!port_num)
				port_num = -1;
		}
		else
			port_num = -1;
		if (extra)
		{
			if (type == ServerICB)
			{
				if ((icbmode = my_index(extra, ':')) && icbmode[1])
					*icbmode++ = 0;
				else
					icbmode = NULL;
			}
			/* XXX should handle extra as :#chan:#chan2:etc for IRC */
		}
		/* relies on parse_server_info putting a null in */
		/* This comes first for "/serv +1" -Sol */
		if ((i = parse_server_index(arg)) != -1)
		{
			if (port_num == -1) /* Could be "/serv +1:6664" -Sol */
				port_num = server_get_port(i);
			if (nick == NULL)
				nick = server_get_nickname(i);
		}
		else if ((i = find_in_server_list(arg, port_num, nick)) != -1)
			port_num = server_get_port(i);
	}
	else
	{
		i = narg;
		port_num = server_get_port(i);
		arg = server_get_name(i);
		if (!proxy_name)
			proxy_name = server_get_proxy_name(i, 0);
		if (proxy_port == 0)
			proxy_port = server_get_proxy_port(i, 0);
	}

	if (!(new_server_flags & WIN_ALL))
	{	/* Test if last window -Sol */
		Win_Trav wt;
		Window	*ptr, *new_win = NULL;

		wt.init = 1;
		while ((ptr = window_traverse(&wt)) != NULL)
			if ((ptr != window) &&
			    (!ptr->server_group ||
			      (ptr->server_group != window->server_group)) &&
			    (ptr->server == window->server))
			{
				new_win = ptr;
				break;
			}
		if (!new_win)
			new_server_flags |= WIN_ALL;
	}

	if (-1 == i)
	{
		if (!nick)
			nick = my_nickname();
		if (port_num == -1)
			port_num = CHOOSE_PORT(type);

		add_to_server_list(arg, port_num, proxy_name, proxy_port,
				   password, nick, -1, type,
				   ssl_level_to_sa_flags(level));
	
		if (group && *group)
			server_set_server_group(get_from_server(),
						find_server_group(group, 1));

		if (extra && type == ServerICB)
		{
			u_char	*newmode;

			if ((newmode = my_index(extra, ':')))
			{
				*newmode++ = 0;
				server_set_icbmode(get_from_server(), newmode);
			}
			server_set_icbgroup(get_from_server(), extra);
		}
	}
	else
	{
		if (nick && *nick)
			server_set_nickname(i, nick);
		if (password && *password)
			server_set_password(i, password);
		if (extra && *extra)
			server_set_icbgroup(i, extra);
		if (icbmode && *icbmode)
			server_set_icbmode(i, icbmode);
		if (group && *group)
			server_set_server_group(i, find_server_group(group, 1));
		/* proxy_name & proxy_port? */

		if (((i = find_in_server_list(server_get_name(i), port_num,
		    nick)) != -1) && is_server_connected(i))
			new_server_flags &= ~WIN_TRANSFER;

		arg = server_get_name(i);
		port_num = server_get_port(i);
	}

	if (!connect_to_server(arg, port_num, nick,
	    (new_server_flags & WIN_ALL) ? window->server : -1))
	{
		window_set_server((int)window->refnum, get_from_server(),
				  new_server_flags);
		update_all_status();
	}
	window_check_servers();
}

void
window_copy_prev_server(int server)
{
	Win_Trav wt;
	Window *tmp;

	wt.init = 1;
	while ((tmp = window_traverse(&wt)))
		if (tmp->server == server)
			tmp->prev_server = server;
}

int
window_get_server(Window *window)
{
	return window->server;
}

int
window_get_server_group(Window *window)
{
	return window->server_group;
}

void
window_server_delete(int deleted_server)
{
	Window	*tmp;
	Win_Trav wt;

	wt.init = 1;
	while ((tmp = window_traverse(&wt)) != NULL)
		if (tmp->server > deleted_server && tmp->server > 0)
			tmp->server--;
}

unsigned int
window_get_refnum(Window *window)
{
	return window->refnum;
}

int
window_get_sticky(Window *window)
{
	return window->sticky;
}

void
window_set_sticky(Window *window, int sticky)
{
	window->sticky = sticky;
}

Window *
window_get_next(Window *window)
{
	return window->next;
}

Window *
window_get_prev(Window *window)
{
	return window->prev;
}

Screen *
window_get_screen(Window *window)
{
	return window->screen;
}

int
window_is_current(Window *window)
{
	return screen_get_current_window(window->screen) == window ? 1 : 0;
}

u_char *
window_get_name(Window *window)
{
	return window->name;
}

int
window_get_prev_server(Window *window)
{
	return window->prev_server;
}

int
window_get_notify_level(Window *window)
{
	return window->notify_level;
}

void
window_set_notify_level(Window *window, int level)
{
	window->notify_level = level;
}

u_char *
window_get_query_nick(Window *window)
{
	return window->query_nick;
}

void
add_to_window_log(Window *window, u_char *str)
{
	add_to_log(window->log_fp, str);
}

LastlogInfo *
window_get_lastlog_info(Window *window)
{
	return window->lastlog_info;
}

int
window_get_lastlog_size(Window *window)
{
	return lastlog_get_size(window->lastlog_info);
}


WindowMenu *
window_get_menu(Window *window)
{
	return window->menu;
}

int
window_menu_lines(Window *window)
{
	return menu_lines(window->menu);
}

int
window_get_window_level(Window *window)
{
	return window->window_level;
}

u_char *
window_get_current_channel(Window *window)
{
	return window->current_channel;
}

void
window_set_current_channel(Window *window, u_char *channel)
{
	if (channel)
		malloc_strcpy(&window->current_channel, channel);
	else
		new_free(&window->current_channel);
}

unsigned
window_get_miscflags(Window *window)
{
	return window->miscflags;
}

void
window_set_miscflags(Window *window, unsigned setflags, unsigned unsetflags)
{
	window->miscflags &= ~unsetflags;
	window->miscflags |= setflags;
}

int
window_get_all_scrolled_lines(Window *window)
{
	return window->scrolled_lines + window->new_scrolled_lines;
}

int
window_get_scrolled_lines(Window *window)
{
	return window->scrolled_lines;
}

int
window_held_lines(Window *window)
{
	return held_lines(window->hold_info);
}

void
window_remove_from_hold_list(Window *window)
{
	remove_from_hold_list(window->hold_info);
}

void
window_hold_mode(Window *window, int flag, int update)
{
	if (window == NULL)
		window = curr_scr_win;
	hold_mode(window, window->hold_info, flag, update);
}

int   
window_hold_output(Window *window)
{
	if (!window)
		window = curr_scr_win;
	return hold_output(window, window->hold_info);
}

int
window_held(Window *window)
{
	return hold_is_held(window->hold_info);
}

void
window_add_new_scrolled_line(Window *window)
{
	window->new_scrolled_lines++;
}

int
window_get_hold_mode(Window *window)
{
	return window->hold_mode;
}

void
window_set_hold_mode(Window *window, int val)
{
	window->hold_mode = val;
}

int
window_get_hold_on_next_rite(Window *window)
{
	return window->hold_on_next_rite;
}

void
window_set_hold_on_next_rite(Window *window, int val)
{
	window->hold_on_next_rite = val;
}

int
window_get_double_status(Window *window)
{
	return window->double_status;
}

void
window_set_status_line(Window *window, int line, u_char *str)
{
	if (line < ARRAY_SIZE(window->status_line) || line < 0)
	{
		if (str)
			malloc_strcpy(&window->status_line[line], str);
		else
			new_free(&window->status_line[line]);
	}
	else
		yell("-- window_set_status_line() called with line = %d", line);
}

void
window_set_update(Window *window, unsigned setflags, unsigned unsetflags)
{
	window->update &= ~unsetflags;
	window->update |= setflags;
}

int
window_get_visible(Window *window)
{
	return window->visible;
}

int
window_get_scroll(Window *window)
{
	return window->scroll;
}

int
window_get_display_size(Window *window)
{
	return window->display_size;
}

int
window_get_line_cnt(Window *window)
{
	return window->line_cnt;
}

void
window_set_line_cnt(Window *window, int line_cnt)
{
	window->line_cnt = line_cnt;
}

int
window_get_cursor(Window *window)
{
	return window->cursor;
}

void
window_set_cursor(Window *window, int cursor)
{
	window->cursor = cursor;
}

int
window_get_top(Window *window)
{
	return window->top;
}

int
window_get_bottom(Window *window)
{
	return window->bottom;
}

NickList **
window_get_nicks(Window *window)
{
	return &window->nicks;
}

HoldInfo *
window_get_hold_info(Window *window)
{
	return window->hold_info;
}

int
window_has_who_from(Window *window)
{
	return nicks_has_who_from(&window->nicks);
}

int
current_who_level(void)
{
	return who_level;
}

u_char *
current_who_from(void)
{
	return who_from;
}
