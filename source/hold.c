/*
 * hold.c: handles buffering of display output.
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
IRCII_RCSID("@(#)$eterna: hold.c,v 1.30 2014/02/21 11:04:36 mrg Exp $");

#include "ircaux.h"
#include "window.h"
#include "screen.h"
#include "vars.h"
#include "input.h"

/* Hold: your general doubly-linked list type structure */
struct hold_stru
{
	u_char	*str;
	Hold	*next;
	Hold	*prev;
	int	logged;
};

/* HoldInfo: stuff specific to this module. */
struct hold_info_stru
{
	int	held;		/* true, the window is currently being held */
	int	last_held;	/* Previous value of hold flag.
				   Used for various updating needs */
	Hold	*hold_head,	/* Pointer to first entry in hold list */
		*hold_tail;	/* Pointer to last entry in hold list */
	int	held_lines;	/* number of lines being held */
};

/* reset_hold: Make hold_mode behave like VM CHAT, hold only
 * when there is no user interaction, this should be called
 * whenever the user does something in a window.  -lynx
 */
void
reset_hold(Window *win)
{
	if (!win)
		win = curr_scr_win;
	if (!window_get_scrolled_lines(win))
		window_set_line_cnt(win, 0);
}

/* add_to_hold_list: adds str to the hold list queue */
void
add_to_hold_list(Window *window, HoldInfo *info, u_char *str, int logged)
{
	Hold	*new;
	unsigned int	max;

	new = new_malloc(sizeof *new);
	new->str = NULL;
	malloc_strcpy(&(new->str), str);
	new->logged = logged;
	info->held_lines++;
	if ((max = get_int_var(HOLD_MODE_MAX_VAR)) != 0)
	{
		if (info->held_lines > max)
			hold_mode(window, info, OFF, 1);
	}
	new->next = info->hold_head;
	new->prev = NULL;
	if (info->hold_tail == NULL)
		info->hold_tail = new;
	if (info->hold_head)
		info->hold_head->prev = new;
	info->hold_head = new;
	update_all_status();
}

/* remove_from_hold_list: pops the next element off the hold list queue. */
void
remove_from_hold_list(HoldInfo *info)
{
	Hold	*crap;

	if (info->hold_tail)
	{
		info->held_lines--;
		new_free(&info->hold_tail->str);
		crap = info->hold_tail;
		info->hold_tail = info->hold_tail->prev;
		if (info->hold_tail == NULL)
			info->hold_head = info->hold_tail;
		else
			info->hold_tail->next = NULL;
		new_free(&crap);
		update_all_status();
	}
}

/*
 * hold_mode: sets the "hold mode".  Really.  If the update flag is true,
 * this will also update the status line, if needed, to display the hold mode
 * state.  If update is false, only the internal flag is set.  
 */
void
hold_mode(Window *window, HoldInfo *info, int flag, int update)
{
	if (flag != ON && window_get_scrolled_lines(window))
		return;
	if (flag == TOGGLE)
	{
		if (info->held == OFF)
			info->held = ON;
		else
			info->held = OFF;
	}
	else
		info->held = flag;
	if (update)
	{
		if (info->held != info->last_held)
		{
			info->last_held = info->held;
					/* This shouldn't be done
					 * this way */
			update_window_status(window, 0);
			window_set_update(window, 0, UPDATE_STATUS);
			cursor_in_display();
			update_input(NO_UPDATE);
		}
	}
	else
		info->last_held = -1;
}

/*
 * hold_output: returns the state of the window->held, which is set in the
 * hold_mode() routine. 
 */
int
hold_output(Window *window, HoldInfo *info)
{
	return info->held == ON || window_get_scrolled_lines(window) != 0;
}

/*
 * hold_queue: returns the string value of the next element on the hold
 * quere.  This does not change the hold queue 
 */
u_char	*
hold_queue(HoldInfo *info)
{
	if (info->hold_tail)
		return info->hold_tail->str;
	else
		return NULL;
}

int
hold_queue_logged(HoldInfo *info)
{
	if (info->hold_tail)
		return info->hold_tail->logged;
	else
		return 0;
}

/* toggle_stop_screen: the BIND function TOGGLE_STOP_SCREEN */
void
toggle_stop_screen(u_int key, u_char *ptr)
{
	window_hold_mode(NULL, TOGGLE, 1);
	update_all_windows();
}

HoldInfo *
alloc_hold_info(void)
{
	HoldInfo *new;

	new = new_malloc(sizeof *new);
	new->held = OFF;
	new->last_held = OFF;
	new->hold_head = new->hold_tail = NULL;
	new->held_lines = 0;

	return new;
}

/*
 * free_hold: This frees all the data and structures associated with the hold
 * list for the given window 
 */
void
free_hold_info(HoldInfo **hold_info)
{
	Hold *tmp, *next;

	for (tmp = (*hold_info)->hold_head; tmp; tmp = next)
	{
		next = tmp->next;
		new_free(&tmp->str);
		new_free(&tmp);
	}
	new_free(hold_info);
}

int
held_lines(HoldInfo *info)
{
	return info->held_lines;
}

int
hold_is_held(HoldInfo *info)
{
	return info->held;
}
