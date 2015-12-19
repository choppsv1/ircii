/*
 * keys.h: header for keys.c 
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

/*
 * @(#)$eterna: keys.h,v 1.38 2014/03/14 20:59:19 mrg Exp $
 */

#ifndef irc__keys_h_
#define irc__keys_h_

/* Move KeyMap and KeyMapNames, and their variables into keys.c */
/* KeyMap: the structure of the irc keymaps */
typedef struct
{
	int	index;
	u_char	changed;
	int	global;
	u_char	*stuff;
}	KeyMap;

typedef	void	(*KeyMap_Func)(u_int, u_char *);

	void	save_bindings(FILE *, int);
	void	bindcmd(u_char *, u_char *, u_char *);
	void	rbindcmd(u_char *, u_char *, u_char *);
	void	parsekeycmd(u_char *, u_char *, u_char *);
	void	typecmd(u_char *, u_char *, u_char *);
	KeyMap_Func keys_get_func_from_index_and_meta(u_char, int);
	u_char *keys_get_str_from_key_and_meta(u_char, int);

enum {
	BACKSPACE = 0,
	BACKWARD_CHARACTER,
	BACKWARD_HISTORY,
	BACKWARD_WORD,
	BEGINNING_OF_LINE,
	CLEAR_SCREEN,
	COMMAND_COMPLETION,
	DELETE_CHARACTER,
	DELETE_NEXT_WORD,
	DELETE_PREVIOUS_WORD,
	END_OF_LINE,
	ENTER_DIGRAPH,
	ENTER_MENU,
	ERASE_LINE,
	ERASE_TO_BEG_OF_LINE,
	ERASE_TO_END_OF_LINE,
	FORWARD_CHARACTER,
	FORWARD_HISTORY,
	FORWARD_WORD,
	META1_CHARACTER,
	META2_CHARACTER,
	META3_CHARACTER,
	META4_CHARACTER,
	META5_CHARACTER,
	META6_CHARACTER,
	META7_CHARACTER,
	META8_CHARACTER,
	NEXT_WINDOW,
	NOTHING,
	PARSE_COMMAND,
	PREVIOUS_WINDOW,
	QUIT_IRC,
	QUOTE_CHARACTER,
	REFRESH_INPUTLINE,
	REFRESH_SCREEN,
	SCROLL_BACKWARD,
	SCROLL_END,
	SCROLL_FORWARD,
	SCROLL_START,
	SELF_INSERT,
	SEND_LINE,
	STOP_IRC,
	SWAP_LAST_WINDOW,
	SWAP_NEXT_WINDOW,
	SWAP_PREVIOUS_WINDOW,
	SWITCH_CHANNELS,
	TOGGLE_INSERT_MODE,
	TOGGLE_STOP_SCREEN,
	TRANSPOSE_CHARACTERS,
	TYPE_TEXT,
	UNSTOP_ALL_WINDOWS,
	YANK_FROM_CUTBUFFER,
	NUMBER_OF_FUNCTIONS
};

#endif /* irc__keys_h_ */
