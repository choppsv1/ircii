/*
 * window.h: header file for window.c 
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
 *
 * @(#)$eterna: window.h,v 1.80 2014/11/22 21:29:00 mrg Exp $
 */

#ifndef irc__window_h_
#define irc__window_h_

/*
 * Define this if you want to play with the new window feature, 
 * CREATE, that allows you to start new screen or xterm windows
 * connected to the ircII client.
 */
#if defined(HAVE_SYS_UN_H)
#define	WINDOW_CREATE
#endif

/*
 * Define this if you want ircII to scroll after printing a line,
 * like it used to (2.1.5 and back era), not before printing the
 * line.   Its a waste of a line to me, but what ever people want.
 * Thanks to Veggen for telling me what to do for this.
 */
#undef SCROLL_AFTER_DISPLAY

#ifdef SCROLL_AFTER_DISPLAY
#define SCROLL_DISPLAY_OFFSET 1
#else
#define SCROLL_DISPLAY_OFFSET 0
#endif

#include "hold.h"
#include "lastlog.h"
#include "edit.h"
#include "menu.h"
#include "names.h"
#include "lastlog.h"
#include "server.h"

/* used by the update flag to determine what needs updating */
#define REDRAW_DISPLAY_FULL 1
#define REDRAW_DISPLAY_FAST 2
#define UPDATE_STATUS 4
#define REDRAW_STATUS 8

#define	LT_UNLOGGED	0
#define	LT_LOGHEAD	1
#define	LT_LOGTAIL	2

/* var_settings indexes */
#define OFF 0
#define ON 1
#define TOGGLE 2

typedef	struct window_stack_stru WindowStack;
typedef struct display_stru Display;

/* used in window_traverse(); use it instead of traverse_all_windows() */
typedef struct
{
	int	init;
	Window	*which;
	Screen	*screen;
	int	visible;
} Win_Trav;

typedef	struct
{
	int	top;
	int	bottom;
	int	position;
} ShrinkInfo;

	void	scroll_window(Window *);
	void	reset_line_cnt(int);
	void	set_continued_line(u_char *);
	void	set_underline_video(int);
	void	window_get_connected(Window *, u_char *, int,
				     u_char *, u_char *, int,
				     u_char *, int,
				     server_ssl_level);
	int	unhold_windows(void);
	Window	*traverse_all_windows(int *);
	Window	*window_traverse(Win_Trav *);
	void	add_to_invisible_list(Window *);
	void	delete_window(Window *);
	Window	*add_to_window_list(Window *);
	void	add_to_window(Window *, u_char *);
	void	scrollback_forwards(u_int, u_char *);
	void	scrollback_backwards(u_int, u_char *);
	void	scrollback_end(u_int, u_char *);
	void	scrollback_start(u_int, u_char *);
	void	set_scroll(int);
	void	set_scroll_lines(int);
	void	update_all_status(void);
	void	set_query_nick(u_char *);
	u_char	*query_nick(void);
	void	update_window_status(Window *, int);
	void	windowcmd(u_char *, u_char *, u_char *);
	void	next_window(u_int, u_char *);
	void	swap_last_window(u_int, u_char *);
	void	swap_next_window(u_int, u_char *);
	void	previous_window(u_int, u_char *);
	void	swap_previous_window(u_int, u_char *);
	void	back_window(u_int, u_char *);
	void	window_kill_swap(void);
	void	redraw_resized(Window *, ShrinkInfo, int);
	void	recalculate_windows(void);
	void	balance_windows(void);
	void	clear_window(Window *);
	void	clear_all_windows(int);
	void	recalculate_window_positions(void);
	void	redraw_all_windows(void);
	ShrinkInfo resize_display(Window *);
	int	is_current_channel(u_char *, int, int);
	void	redraw_all_status(void);
	void	message_to(u_int);
	void	message_from(u_char *, int);
	void	unstop_all_windows(u_int, u_char *);
	void	set_prompt_by_refnum(u_int, u_char *);
	int	number_of_windows(void);
	void	clear_window_by_refnum(u_int);
	u_int	current_refnum(void);
	Window	*get_window_by_refnum(u_int);
	u_char	*get_target_by_refnum(u_int);
	u_char	*get_prompt_by_refnum(u_int);
	u_char	*set_channel_by_refnum(u_int, u_char *);
	u_char	*get_channel_by_refnum(u_int);
	void	window_set_server(int, int, int);
	Window	*new_window(void);
	Window	*get_window_by_name(u_char *);
	void	free_display(Window *);
	void	erase_display(Window *);
	u_char	**split_up_line_alloc(u_char *);
	void	window_add_display_line(Window *, u_char *, int);
	int	get_window_server(u_int);
	int	message_from_level(int);
	void	restore_message_from(void);
	void	save_message_from(void);
	void	window_check_servers(void);
	void	set_current_window(Window *);
	void	set_level_by_refnum(u_int, int);
	Window	*is_bound(u_char *, int);
	void	add_window_to_server_group(Window *, u_char *);
	void	delete_window_from_server_group(Window *, u_char *);
	void	window_restore_server(int);
	void	window_copy_prev_server(int);
	int	window_get_server(Window *);
	int	window_get_server_group(Window *);
	void	window_server_delete(int);
	unsigned int window_get_refnum(Window *);
	int	window_get_sticky(Window *);
	void	window_set_sticky(Window *, int);
	Window	*window_get_next(Window *);
	Window	*window_get_prev(Window *);
	Screen	*window_get_screen(Window *);
	int	window_is_current(Window *);
	u_char	*window_get_name(Window *);
	int	window_get_prev_server(Window *);
	int	window_get_notify_level(Window *);
	void	window_set_notify_level(Window *, int);
	u_char	*window_get_query_nick(Window *);
	void	add_to_window_log(Window *, u_char *);
	LastlogInfo *window_get_lastlog_info(Window *);
	int	window_get_lastlog_size(Window *);
	WindowMenu *window_get_menu(Window *);
	int	window_menu_lines(Window *);
	int	window_get_window_level(Window *);
	u_char	*window_get_current_channel(Window *);
	void	window_set_current_channel(Window *, u_char *);
	unsigned window_get_miscflags(Window *);
	void	window_set_miscflags(Window *, unsigned, unsigned);
	int	window_get_all_scrolled_lines(Window *);
	int	window_get_scrolled_lines(Window *);
	int	window_held_lines(Window *);
	void	window_remove_from_hold_list(Window *);
	void    window_hold_mode(Window *, int, int);
	int	window_hold_output(Window *);
	int	window_held(Window *);
	void	window_add_new_scrolled_line(Window *);
	int	window_get_hold_mode(Window *);
	void	window_set_hold_mode(Window *, int);
	int	window_get_hold_on_next_rite(Window *);
	void	window_set_hold_on_next_rite(Window *, int);
	int	window_get_double_status(Window *);
	void	window_set_status_line(Window *, int, u_char *);
	void	window_set_update(Window *, unsigned, unsigned);
	int	window_get_visible(Window *);
	int	window_get_scroll(Window *);
	int	window_get_display_size(Window *);
	void	window_set_display_size_normal(Window *);
	int	window_get_line_cnt(Window *);
	void	window_set_line_cnt(Window *, int);
	int	window_get_cursor(Window *);
	void	window_set_cursor(Window *, int);
	int	window_get_top(Window *);
	int	window_get_bottom(Window *);
	NickList **window_get_nicks(Window *);
	HoldInfo *window_get_hold_info(Window *);
	int	window_has_who_from(Window *);
	int	current_who_level(void);
	u_char	*current_who_from(void);

#define WINDOW_NOTIFY	((unsigned) 0x0001)
#define WINDOW_NOTIFIED	((unsigned) 0x0002)

/* for window_set_server() -Sol */
#define	WIN_ALL		0x01
#define	WIN_TRANSFER	0x02
#define	WIN_FORCE	0x04

#endif /* irc__window_h_ */
