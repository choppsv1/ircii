/*
 * input.h: header for input.c 
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
 * @(#)$eterna: input.h,v 1.27 2014/03/14 20:59:19 mrg Exp $
 */

#ifndef irc__input_h_
#define irc__input_h_

typedef struct ScreenInputData ScreenInputData;

	void	set_input(u_char *);
	void	set_input_raw(u_char *);
	void	set_input_prompt(u_char *);
	u_char	*get_input_prompt(void);
	u_char	*get_input(void);
	u_char	*get_input_raw(void);
	void	update_input(int);
	void	init_input(void);
	void    input_reset_screen(Screen *);
	void	input_move_cursor(int);
	void	change_input_prompt(int);
	void	cursor_to_input(void);
	void	input_add_character(u_int, u_char *);
	void	input_backward_word(u_int, u_char *);
	void	input_forward_word(u_int, u_char *);
	void	input_delete_previous_word(u_int, u_char *);
	void	input_delete_next_word(u_int, u_char *);
	void	input_clear_to_bol(u_int, u_char *);
	void	input_clear_line(u_int, u_char *);
	void	input_end_of_line(u_int, u_char *);
	void	input_clear_to_eol(u_int, u_char *);
	void	input_beginning_of_line(u_int, u_char *);
	void	refresh_inputline(u_int, u_char *);
	void	input_delete_character(u_int, u_char *);
	void	input_backspace(u_int, u_char *);
	void	input_transpose_characters(u_int, u_char *);
	void	input_yank_cut_buffer(u_int, u_char *);
	u_char	*function_curpos(u_char *);

/* used by update_input */
#define NO_UPDATE 0
#define UPDATE_ALL 1
#define UPDATE_FROM_CURSOR 2
#define UPDATE_JUST_CURSOR 3

#endif /* irc__input_h_ */
