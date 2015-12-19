/*
 * screen.h: header for screen.c
 *
 * written by matthew green.
 *
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
 *
 * see the copyright file, or type help ircii copyright
 *
 * @(#)$eterna: screen.h,v 1.48 2015/05/14 01:47:38 mrg Exp $
 */

#ifndef irc__screen_h_
#define irc__screen_h_

#include "window.h"
#include "input.h"
#include "edit.h"

#define WAIT_PROMPT_LINE	0x01
#define WAIT_PROMPT_KEY		0x02

/* Stuff for the screen/xterm junk */

#define ST_NOTHING	-1
#define ST_SCREEN	0
#define ST_XTERM	1

	Screen	*create_new_screen(void);
	int	output_line(const u_char *, int);
	Window	*create_additional_screen(int);
	void	update_all_windows(void);
	void	add_wait_prompt(u_char *, void (*)(u_char *, u_char *), u_char *, int);
	void	cursor_not_in_display(void);
	void	cursor_in_display(void);
	int	is_cursor_in_display(void);
	void	decode_colour(u_char **, int *, int *);
	void	set_current_screen(Screen *);
	void	window_redirect(u_char *, int);
	void	close_all_screen(void);
	int	check_screen_redirect(u_char *);
	void	kill_screen(Screen *);
	int	is_main_screen(Screen *);
	void	add_to_screen(u_char *);
	void	screen_wserv_message(Screen *screen);
	int	my_strlen_i(u_char *);
	int	my_strlen_c(u_char *);
	int	my_strlen_ci(u_char *);
	void	my_strcpy_ci(u_char *, size_t, u_char *);
	int	screen_get_screennum(Screen *);
	Window	*screen_get_current_window(Screen *);
	void	screen_set_current_window(Screen *, Window *);
	ScreenInputData *screen_get_inputdata(Screen *);
	void	screen_set_inputdata(Screen *, ScreenInputData *);
	int	screen_get_alive(Screen *);
	u_char	*get_redirect_token(void);
	int	screen_get_wserv_fd(Screen *);
	Window	*screen_get_window_list(Screen *);
	void	screen_set_window_list(Screen *, Window *);
	Window	*screen_get_window_list_end(Screen *);
	void	screen_set_window_list_end(Screen *, Window *);
	int	screen_get_visible_windows(Screen *);
	void	incr_visible_windows(void);
	void	decr_visible_windows(void);
	int	screen_get_fdin(Screen *);
	FILE	*screen_get_fpout(Screen *);
	unsigned get_last_window_refnum(void);
	void	set_last_window_refnum(unsigned);
	Prompt	*get_current_promptlist(void);
	void	set_current_promptlist(Prompt *);
	Screen	*screen_get_next(Screen *);
	WindowStack *get_window_stack(void);
	void	set_window_stack(WindowStack *);
	Window *get_cursor_window(void);
	void	set_cursor_window(Window *);
	EditInfo *get_edit_info(void);
	int	screen_set_size(int, int);
	int	screen_get_old_co(void);
	int	screen_get_old_li(void);
	int	get_co(void);
	int	get_li(void);
	Screen	*screen_first(void);
	void	set_main_screen(Screen *);
	Screen	*get_last_input_screen(void);
	void	set_last_input_screen(Screen *);
	Window	*get_to_window(void);
	void	set_to_window(Window *);
	Screen	*get_current_screen(void);
	int	in_redirect(void);

/* These is here because they happens in so many places */
#define curr_scr_win			screen_get_current_window(get_current_screen())
#define set_curr_scr_win(window)	screen_set_current_window(get_current_screen(), window)

#endif /* irc__screen_h_ */
