/*
 * keys.c: Does command line parsing, etc 
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
IRCII_RCSID("@(#)$eterna: keys.c,v 1.64 2014/03/14 20:59:19 mrg Exp $");

#include "output.h"
#include "keys.h"
#include "names.h"
#include "ircaux.h"
#include "window.h"
#include "edit.h"
#include "vars.h"
#include "translat.h"
#include "ircterm.h"
#include "input.h"
#include "screen.h"
#include "menu.h"
#include "ircterm.h"

/* KeyMapNames: the structure of the keymap to realname array */
typedef struct
{
	char	*name;
	KeyMap_Func func;
}	KeyMapNames;

static	int	lookup_function(u_char *, int *);
static	u_char	* display_key(u_int);
static	void	show_binding(u_int, int);
static	int	parse_key(u_char *);
static	void	write_binding(u_int, u_int, FILE *, int);
static	void	bind_it(u_char *, u_char *, u_int, int);

/* The string values for these *MUST* be in ALL CAPITALS */
static KeyMapNames key_names[] =
{
	{ "BACKSPACE",			input_backspace },
	{ "BACKWARD_CHARACTER",		backward_character },
	{ "BACKWARD_HISTORY",		backward_history },
	{ "BACKWARD_WORD",		input_backward_word },
	{ "BEGINNING_OF_LINE",		input_beginning_of_line },
	{ "CLEAR_SCREEN",		irc_clear_screen },
	{ "COMMAND_COMPLETION",		command_completion },
	{ "DELETE_CHARACTER",		input_delete_character },
	{ "DELETE_NEXT_WORD",		input_delete_next_word },
	{ "DELETE_PREVIOUS_WORD",	input_delete_previous_word },
	{ "END_OF_LINE",		input_end_of_line },
	{ "ENTER_DIGRAPH",		enter_digraph },
	{ "ENTER_MENU",			enter_menu },
	{ "ERASE_LINE",			input_clear_line },
	{ "ERASE_TO_BEG_OF_LINE",	input_clear_to_bol },
	{ "ERASE_TO_END_OF_LINE",	input_clear_to_eol },
	{ "FORWARD_CHARACTER",		forward_character },
	{ "FORWARD_HISTORY",		forward_history },
	{ "FORWARD_WORD",		input_forward_word },
	{ "META1_CHARACTER",		meta1_char },
	{ "META2_CHARACTER",		meta2_char },
	{ "META3_CHARACTER",		meta3_char },
	{ "META4_CHARACTER",		meta4_char },
	{ "META5_CHARACTER",		meta5_char },
	{ "META6_CHARACTER",		meta6_char },
	{ "META7_CHARACTER",		meta7_char },
	{ "META8_CHARACTER",		meta8_char },
	{ "NEXT_WINDOW",		next_window },
	{ "NOTHING",			NULL },
	{ "PARSE_COMMAND",		parse_text },
	{ "PREVIOUS_WINDOW",		previous_window },
	{ "QUIT_IRC",			irc_quit },
	{ "QUOTE_CHARACTER",		quote_char },
	{ "REFRESH_INPUTLINE",		refresh_inputline },
	{ "REFRESH_SCREEN",		refresh_screen },
	{ "SCROLL_BACKWARD",		scrollback_backwards },
	{ "SCROLL_END",			scrollback_end },
	{ "SCROLL_FORWARD",		scrollback_forwards },
	{ "SCROLL_START",		scrollback_start },
	{ "SELF_INSERT",		input_add_character },
	{ "SEND_LINE",			send_line },
	{ "STOP_IRC",			term_pause },
	{ "SWAP_LAST_WINDOW",		swap_last_window },
	{ "SWAP_NEXT_WINDOW",		swap_next_window },
	{ "SWAP_PREVIOUS_WINDOW",	swap_previous_window },
	{ "SWITCH_CHANNELS",		switch_channels },
	{ "TOGGLE_INSERT_MODE",		toggle_insert_mode },
	{ "TOGGLE_STOP_SCREEN",		toggle_stop_screen },
	{ "TRANSPOSE_CHARACTERS",	input_transpose_characters },
	{ "TYPE_TEXT",			type_text },
	{ "UNSTOP_ALL_WINDOWS",		unstop_all_windows },
	{ "YANK_FROM_CUTBUFFER",	input_yank_cut_buffer }
};

/* Do we default to having the Emacs like meta keys? */
#ifdef WITH_EMACS_META_KEYS
# define EMACS_SCROLL_START		SCROLL_START
# define EMACS_SCROLL_END		SCROLL_END
# define EMACS_FORWARD_WORD		FORWARD_WORD
# define EMACS_BACKWARD_WORD		BACKWARD_WORD
# define EMACS_DELETE_NEXT_WORD		DELETE_NEXT_WORD
# define EMACS_DELETE_PREVIOUS_WORD	DELETE_PREVIOUS_WORD
# define EMACS_SCROLL_END		SCROLL_END
#else
# define EMACS_SCROLL_START		SELF_INSERT
# define EMACS_SCROLL_END		SELF_INSERT
# define EMACS_FORWARD_WORD		SELF_INSERT
# define EMACS_BACKWARD_WORD		SELF_INSERT
# define EMACS_DELETE_NEXT_WORD		SELF_INSERT
# define EMACS_DELETE_PREVIOUS_WORD	SELF_INSERT
# define EMACS_SCROLL_END		SELF_INSERT
#endif

static KeyMap	keys[] =
{
	{ SELF_INSERT,		0, 0,	NULL },	/* 0 */
	{ BEGINNING_OF_LINE,	0, 0,	NULL },
	{ BACKWARD_CHARACTER,	0, 0,	NULL },
	{ QUIT_IRC,		0, 0,	NULL },
	{ DELETE_CHARACTER,	0, 0,	NULL },
	{ END_OF_LINE,		0, 0,	NULL },
	{ FORWARD_CHARACTER,	0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },

	{ BACKSPACE,		0, 0,	NULL },	/* 8 */
	{ TOGGLE_INSERT_MODE,	0, 0,	NULL },
	{ SEND_LINE,		0, 0,	NULL },
	{ ERASE_TO_END_OF_LINE,	0, 0,	NULL },
	{ REFRESH_SCREEN,	0, 0,	NULL },
	{ SEND_LINE,		0, 0,	NULL },
	{ FORWARD_HISTORY,	0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },

	{ BACKWARD_HISTORY,	0, 0,	NULL },	/* 16 */
	{ QUOTE_CHARACTER,	0, 0,	NULL },
	{ ENTER_MENU,		0, 0,	NULL },
	{ TOGGLE_STOP_SCREEN,	0, 0,	NULL },
	{ TRANSPOSE_CHARACTERS,	0, 0,	NULL },
	{ ERASE_TO_BEG_OF_LINE,	0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },

	{ META2_CHARACTER,	0, 0,	NULL },	/* 24 */
	{ YANK_FROM_CUTBUFFER,	0, 0,	NULL },
			/* And I moved STOP_IRC to META1 26 */
	{ ENTER_DIGRAPH,	0, 0,	NULL },
	{ META1_CHARACTER,	0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },

	{ SELF_INSERT,		0, 0,	NULL },	/* 32 */
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },

	{ SELF_INSERT,		0, 0,	NULL },	/* 40 */
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },

	{ SELF_INSERT,		0, 0,	NULL },	/* 48 */
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },

	{ SELF_INSERT,		0, 0,	NULL },	/* 56 */
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },

	{ SELF_INSERT,		0, 0,	NULL },	/* 64 */
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },

	{ SELF_INSERT,		0, 0,	NULL },	/* 72 */
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },

	{ SELF_INSERT,		0, 0,	NULL },	/* 80 */
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },

	{ SELF_INSERT,		0, 0,	NULL },	/* 88 */
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },

	{ SELF_INSERT,		0, 0,	NULL },	/* 96 */
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },

	{ SELF_INSERT,		0, 0,	NULL },	/* 104 */
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
 
	{ SELF_INSERT,		0, 0,	NULL },	/* 112 */
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },

	{ SELF_INSERT,		0, 0,	NULL },	/* 120 */
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ BACKSPACE,		0, 0,	NULL },

	{ SELF_INSERT,		0, 0,	NULL },	/* 128 */
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },

	{ SELF_INSERT,		0, 0,	NULL },	/* 136 */
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },

	{ SELF_INSERT,		0, 0,	NULL },	/* 144 */
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },

	{ SELF_INSERT,		0, 0,	NULL },	/* 152 */
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },

	{ SELF_INSERT,		0, 0,	NULL },	/* 160 */
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },

	{ SELF_INSERT,		0, 0,	NULL },	/* 168 */
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },

	{ SELF_INSERT,		0, 0,	NULL },	/* 176 */
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },

	{ SELF_INSERT,		0, 0,	NULL },	/* 184 */
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ EMACS_SCROLL_START,	0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ EMACS_SCROLL_END,	0, 0,	NULL },

	{ SELF_INSERT,		0, 0,	NULL },	/* 192 */
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
 
	{ SELF_INSERT,		0, 0,	NULL },	/* 200 */
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },

	{ SELF_INSERT,		0, 0,	NULL },	/* 208 */
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },

	{ SELF_INSERT,		0, 0,	NULL },	/* 216 */
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },

	{ SELF_INSERT,		0, 0,	NULL },	/* 224 */
	{ SELF_INSERT,		0, 0,	NULL },
	{ EMACS_BACKWARD_WORD,	0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ EMACS_DELETE_NEXT_WORD, 0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ EMACS_SCROLL_END,	0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL }, /* EMACS_FORWARD_WORD */

	{ EMACS_DELETE_PREVIOUS_WORD, 0, 0,	NULL },	/* 232 */
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },

	{ SELF_INSERT,		0, 0,	NULL },	/* 240 */
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },

	{ SELF_INSERT,		0, 0,	NULL },	/* 248 */
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ SELF_INSERT,		0, 0,	NULL },
	{ EMACS_DELETE_PREVIOUS_WORD, 0, 0,	NULL },
};

static KeyMap	meta1_keys[] =
{
	{ NOTHING,		0, 0,	NULL },	/* 0 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	
	{ NOTHING,		0, 0,	NULL },	/* 8 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 16 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 24 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ COMMAND_COMPLETION,	0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 32 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 40 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ CLEAR_SCREEN,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 48 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 56 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ SCROLL_START,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ SCROLL_END,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 64 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 72 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 80 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 88 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ META3_CHARACTER,	0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 96 */
	{ NOTHING,		0, 0,	NULL },
	{ BACKWARD_WORD,	0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ DELETE_NEXT_WORD,	0, 0,	NULL },
	{ SCROLL_END,		0, 0,	NULL },
	{ FORWARD_WORD,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ DELETE_PREVIOUS_WORD,	0, 0,	NULL },	/* 104 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ SCROLL_FORWARD,	0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ SCROLL_BACKWARD,	0, 0,	NULL },	/* 112 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 120 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ DELETE_PREVIOUS_WORD,	0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 128 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 136 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 144 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 152 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 160 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 168 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 176 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 184 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 192 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 200 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 208 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 216 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 224 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 232 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 240 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 248 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL }
};

static KeyMap	meta2_keys[] =
{
	{ NOTHING,		0, 0,	NULL },	/* 0 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 8 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 16 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 24 */
	{ NOTHING,		0, 0,	NULL },
#ifdef ALLOW_STOP_IRC
	{ STOP_IRC,		0, 0,	NULL },
#else
	{ NOTHING,		0, 0,	NULL },
#endif /* ALLOW_STOP_IRC */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 32 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 40 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 48 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 56 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 64 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 72 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 80 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 88 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 96 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 104 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NEXT_WINDOW,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ PREVIOUS_WINDOW,	0, 0,	NULL },	/* 112 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 120 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 128 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 136 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 144 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 152 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 160 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 168 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 176 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 184 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 192 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 200 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 208 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 216 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 224 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 232 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 240 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 248 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL }
};

static KeyMap	meta3_keys[] =
#include "empty_metakeys.inc"

static KeyMap	meta4_keys[] =
{
	{ NOTHING,		0, 0,	NULL },	/* 0 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ BACKWARD_CHARACTER,	0, 0,	NULL },	/* 8 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 16 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 24 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ FORWARD_CHARACTER,	0, 0,	NULL },	/* 32 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 40 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 48 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 56 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 64 */
	{ META4_CHARACTER,	0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ BACKWARD_CHARACTER,	0, 0,	NULL },	/* 72 */
	{ META4_CHARACTER,	0, 0,	NULL },
	{ FORWARD_HISTORY,	0, 0,	NULL },
	{ BACKWARD_HISTORY,	0, 0,	NULL },
	{ FORWARD_CHARACTER,	0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 80 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ DELETE_CHARACTER,	0, 0,	NULL },	/* 88 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 96 */
	{ META4_CHARACTER,	0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ BACKWARD_CHARACTER,	0, 0,	NULL },	/* 104 */
	{ META4_CHARACTER,	0, 0,	NULL },
	{ FORWARD_HISTORY,	0, 0,	NULL },
	{ BACKWARD_HISTORY,	0, 0,	NULL },
	{ FORWARD_CHARACTER,	0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 112 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ DELETE_CHARACTER,	0, 0,	NULL },	/* 120 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 128 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 136 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 144 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 152 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 160 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 168 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 176 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 184 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 192 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 200 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 208 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 216 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 224 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 232 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 240 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },

	{ NOTHING,		0, 0,	NULL },	/* 248 */
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL },
	{ NOTHING,		0, 0,	NULL }
};

static KeyMap	meta5_keys[] =
#include "empty_metakeys.inc"

static KeyMap	meta6_keys[] =
#include "empty_metakeys.inc"

static KeyMap	meta7_keys[] =
#include "empty_metakeys.inc"

static KeyMap	meta8_keys[] =
#include "empty_metakeys.inc"

/*
 * lookup_function: looks up an irc function by name, and returns the
 * number of functions that match the name, and sets where index points
 * to to be the index of the (first) function found.
 */
static int
lookup_function(u_char *name, int *func_index)
{
	size_t	len;
	int	cnt,
		i;

	if (name)
	{
		upper(name);
		len = my_strlen(name);
		cnt = 0;
		*func_index = -1;
		for (i = 0; i < NUMBER_OF_FUNCTIONS; i++)
		{
			if (my_strncmp(name, key_names[i].name, len) == 0)
			{
				cnt++;
				if (*func_index == -1)
					*func_index = i;
			}
		}
		if (*func_index == -1)
			return (0);
		if (my_strcmp(name, key_names[*func_index].name) == 0)
			return (1);
		else
			return (cnt);
	}
	return (0);
}

/*
 * display_key: converts the character c to a displayable form and returns
 * it.  Very simple indeed 
 */
static	u_char	*
display_key(u_int c)
{
	static	u_char key[3];

	key[2] = (u_char) 0;
	if (c < 32)
	{
		key[0] = '^';
		key[1] = c + 64;
	}
	else if (c == '\177')
	{
		key[0] = '^';
		key[1] = '?';
	}
	else
	{
		key[0] = c;
		key[1] = (u_char) 0;
	}
	return (key);
}

/*
 * show_binding: given the ascii value of a key and a meta key status (1 for
 * meta1 keys, 2 for meta2 keys, anything else for normal keys), this will
 * display the key binding for the key in a nice way
 */
static void
show_binding(u_int c, int m)
{
	KeyMap	*map;
	char	*meta_str;

	switch (m)
	{
	case 1:
		map = meta1_keys;
		meta_str = "META1-";
		break;
	case 2:
		map = meta2_keys;
		meta_str = "META2-";
		break;
	case 3:
		map = meta3_keys;
		meta_str = "META3-";
		break;
	case 4:
		map = meta4_keys;
		meta_str = "META4-";
		break;
	case 5:
		map = meta5_keys;
		meta_str = "META5-";
		break;
	case 6:
		map = meta6_keys;
		meta_str = "META6-";
		break;
	case 7:
		map = meta7_keys;
		meta_str = "META7-";
		break;
	case 8:
		map = meta8_keys;
		meta_str = "META8-";
		break;
	default:
		map = keys;
		meta_str = CP(empty_string());
		break;
	}
	say("%s%s is bound to %s %s", meta_str, display_key(c),
		key_names[map[c].index].name, (map[c].stuff &&
		(*(map[c].stuff))) ? map[c].stuff : empty_string());
}

/*
 * parse_key: converts a key string. Accepts any key, or ^c where c is any
 * key (representing control characters), or META1- or META2- for meta1 or
 * meta2 keys respectively.  The string itself is converted to true ascii
 * value, thus "^A" is converted to 1, etc.  Meta key info is removed and
 * returned as the function value, 0 for no meta key, 1 for meta1, and 2 for
 * meta2.  Thus, "META1-a" is converted to "a" and a 1 is returned.
 * Furthermore, if ^X is bound to META2_CHARACTER, and "^Xa" is passed to
 * parse_key(), it is converted to "a" and 2 is returned.  Do ya understand
 * this? 
 */
static int
parse_key(u_char *key_str)
{
	u_char	*ptr1, *ptr2;
	u_char	c;
	int	m = 0;

	ptr2 = ptr1 = key_str;
	while (*ptr1)
	{
		if (*ptr1 == '^')
		{
			ptr1++;
			switch (*ptr1)
			{
			case 0:
				*(ptr2++) = '^';
				break;
			case '?':
				*(ptr2++) = '\177';
				ptr1++;
				break;
			default:
				c = *(ptr1++);
				if (islower(c))
					c = toupper(c);
				if (c < 64)
				{
					say("Illegal key sequence: ^%c", c);
					return (-1);
				}
				*(ptr2++) = c - 64;
			}
		}
		else
			*(ptr2++) = *(ptr1++);
	}
	*ptr2 = (u_char) 0;
	if ((int) my_strlen(key_str) > 1)
	{
		u_char	*cmd = NULL;

		malloc_strcpy(&cmd, key_str);
		upper(cmd);
		if (my_strncmp(cmd, "META1-", 6) == 0)
		{
			my_strcpy(key_str, key_str + 6);
			m = 1;
		}
		else if (my_strncmp(cmd, "META2-", 6) == 0)
		{
			my_strcpy(key_str, key_str + 6);
			m = 2;
		}
		else if (my_strncmp(cmd, "META3-", 6) == 0)
		{
			my_strcpy(key_str, key_str + 6);
			m = 3;
		}
		else if (my_strncmp(cmd, "META4-", 6) == 0)
		{
			my_strcpy(key_str, key_str + 6);
			m = 4;
		}
		else if (my_strncmp(cmd, "META5-", 6) == 0)
		{
			my_strcpy(key_str, key_str + 6);
			m = 5;
		}
		else if (my_strncmp(cmd, "META6-", 6) == 0)
		{
			my_strcpy(key_str, key_str + 6);
			m = 6;
		}
		else if (my_strncmp(cmd, "META7-", 6) == 0)
		{
			my_strcpy(key_str, key_str + 6);
			m = 7;
		}
		else if (my_strncmp(cmd, "META8-", 6) == 0)
		{
			my_strcpy(key_str, key_str + 6);
			m = 8;
		}
		else if (keys[(u_char) *key_str].index == META1_CHARACTER)
		{
			m = 1;
			my_strcpy(key_str, key_str + 1);
		}
		else if (keys[(u_char) *key_str].index == META2_CHARACTER)
		{
			m = 2;
			my_strcpy(key_str, key_str + 1);
		}
		else if (keys[(u_char) *key_str].index == META3_CHARACTER)
		{
			m = 3;
			my_strcpy(key_str, key_str + 1);
		}
		else if (keys[(u_char) *key_str].index == META4_CHARACTER)
		{
			m = 4;
			my_strcpy(key_str, key_str + 1);
		}
		else if (keys[(u_char) *key_str].index == META5_CHARACTER)
		{
			m = 5;
			my_strcpy(key_str, key_str + 1);
		}
		else if (keys[(u_char) *key_str].index == META6_CHARACTER)
		{
			m = 6;
			my_strcpy(key_str, key_str + 1);
		}
		else if (keys[(u_char) *key_str].index == META7_CHARACTER)
		{
			m = 7;
			my_strcpy(key_str, key_str + 1);
		}
		else if (keys[(u_char) *key_str].index == META8_CHARACTER)
		{
			m = 8;
			my_strcpy(key_str, key_str + 1);
		}
		else
		{
			say("Illegal key sequence: %s is not a meta-key", display_key(*key_str));
			return (-1);
		}
		new_free(&cmd);
	}
	return (m);
}

/*
 * bind_it: does the actually binding of the function to the key with the
 * given meta modifier
 */
static	void
bind_it(u_char *function, u_char *string, u_int key, int m)
{
	KeyMap	*km;
	int	cnt,
		func_index,
		i;

	switch (m)
	{
	case 0:
		km = keys;
		break;
	case 1:
		km = meta1_keys;
		break;
	case 2:
		km = meta2_keys;
		break;
	case 3:
		km = meta3_keys;
		break;
	case 4:
		km = meta4_keys;
		break;
	case 5:
		km = meta5_keys;
		break;
	case 6:
		km = meta6_keys;
		break;
	case 7:
		km = meta7_keys;
		break;
	case 8:
		km = meta8_keys;
		break;
	default:
		km = keys;
	}
	if (*string == (u_char) 0)
		string = NULL;
	switch (cnt = lookup_function(function, &func_index))
	{
	case 0:
		say("No such function: %s", function);
		break;
	case 1:
		if (! km[key].changed)
		{
			if ((km[key].index != func_index) ||
					((string == NULL) &&
					km[key].stuff) ||
					((km[key].stuff == NULL) &&
					string) || (string && km[key].stuff &&
					my_strcmp(km[key].stuff,string)))
				km[key].changed = 1;
		}
		km[key].index = func_index;
		km[key].global = loading_global();
		malloc_strcpy(&(km[key].stuff), string);
		show_binding(key, m);
		break;
	default:
		say("Ambiguous function name: %s", function);
		for (i = 0; i < cnt; i++, func_index++)
			put_it("%s", key_names[func_index].name);
		break;
	}
}

/* parsekeycmd: does the PARSEKEY command.  */
void
parsekeycmd(u_char *command, u_char *args, u_char *subargs)
{
	int	i;
	u_char	*arg;

	if ((arg = next_arg(args, &args)) != NULL)
	{
		switch (lookup_function(arg, &i))
		{
		case 0:
			say("No such function %s", arg);
			return;
		case 1:
			key_names[i].func(0, args);
			break;
		default:
			say("Ambigious function %s", arg);
			break;
		}
	}
}

/*
 * bindcmd: the bind command, takes a key sequence followed by a function
 * name followed by option arguments (used depending on the function) and
 * binds a key.  If no function is specified, the current binding for that
 * key is shown 
 */
void
bindcmd(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*key;
	u_char	*function;
	int	m;

	if ((key = (u_char *) next_arg(args, &args)) != NULL)
	{
		if ((m = parse_key(key)) == -1)
			return;
		if (my_strlen(key) > 1)
		{
			say("Key sequences may not contain more than two keys");
			return;
		}
		if ((function = next_arg(args, &args)) != NULL)
			bind_it(function, args, *key, m);
		else
			show_binding(*key, m);
	}
	else
	{
		u_int	i;
		int	charsize = charset_size();

		for (i = 0; i < charsize; i++)
		{
			if ((keys[i].index != NOTHING) && (keys[i].index !=
					SELF_INSERT))
				show_binding(i, 0);
		}
		for (i = 0; i < charsize; i++)
		{
			if ((meta1_keys[i].index != NOTHING) &&
					(meta1_keys[i].index != SELF_INSERT))
				show_binding(i, 1);
		}
		for (i = 0; i < charsize; i++)
		{
			if ((meta2_keys[i].index != NOTHING) &&
					(meta2_keys[i].index != SELF_INSERT))
				show_binding(i, 2);
		}
		for (i = 0; i < charsize; i++)
		{
			if ((meta3_keys[i].index != NOTHING) &&
					(meta3_keys[i].index != SELF_INSERT))
				show_binding(i, 3);
		}
		for (i = 0; i < charsize; i++)
		{
			if ((meta4_keys[i].index != NOTHING) &&
					(meta4_keys[i].index != SELF_INSERT))
				show_binding(i, 4);
		}
		for (i = 0; i < charsize; i++)
		{
			if ((meta5_keys[i].index != NOTHING) &&
					(meta5_keys[i].index != SELF_INSERT))
				show_binding(i, 5);
		}
		for (i = 0; i < charsize; i++)
		{
			if ((meta6_keys[i].index != NOTHING) &&
					(meta6_keys[i].index != SELF_INSERT))
				show_binding(i, 6);
		}
		for (i = 0; i < charsize; i++)
		{
			if ((meta7_keys[i].index != NOTHING) &&
					(meta7_keys[i].index != SELF_INSERT))
				show_binding(i, 7);
		}
		for (i = 0; i < charsize; i++)
		{
			if ((meta8_keys[i].index != NOTHING) &&
					(meta8_keys[i].index != SELF_INSERT))
				show_binding(i, 8);
		}
	}
}

/*
 * rbindcmd: does the rbind command.  you give it a string that something
 * is bound to and it tells you all the things that are bound to that
 * functions
 */
void
rbindcmd(u_char *command, u_char *args, u_char *subargs)
{
	int	f;
	u_char	*arg;

	if ((arg = next_arg(args, &args)) != NULL)
	{
		u_int	i;
		int	charsize = charset_size();

		switch (lookup_function(arg, &f))
		{
		case 0:
			say("No such function %s", arg);
			return;

		case 1:
			break;

		default:
			say("Ambigious function %s", arg);
			return;
		}

		for (i = 0; i < charsize; i++)
			if (f == keys[i].index)
				show_binding(i, 0);
		for (i = 0; i < charsize; i++)
			if (f == meta1_keys[i].index)
				show_binding(i, 1);
		for (i = 0; i < charsize; i++)
			if (f == meta2_keys[i].index)
				show_binding(i, 2);
		for (i = 0; i < charsize; i++)
			if (f == meta3_keys[i].index)
				show_binding(i, 3);
		for (i = 0; i < charsize; i++)
			if (f == meta4_keys[i].index)
				show_binding(i, 4);
		for (i = 0; i < charsize; i++)
			if (f == meta5_keys[i].index)
				show_binding(i, 5);
		for (i = 0; i < charsize; i++)
			if (f == meta6_keys[i].index)
				show_binding(i, 6);
		for (i = 0; i < charsize; i++)
			if (f == meta7_keys[i].index)
				show_binding(i, 7);
		for (i = 0; i < charsize; i++)
			if (f == meta8_keys[i].index)
				show_binding(i, 8);
	}
}

/*
 * typecmd: The TYPE command.  This parses the given string and treats each
 * character as tho it were typed in by the user.  Thus key bindings are used
 * for each character parsed.  Special case characters are control character
 * sequences, specified by a ^ follow by a legal control key.  Thus doing
 * "/TYPE ^B" will be as tho ^B were hit at the keyboard, probably moving the
 * cursor backward one character.
 */
void
typecmd(u_char *command, u_char *args, u_char *subargs)
{
	int	c;
	u_char	key;

	while (*args)
	{
		if (*args == '^')
		{
			switch (*(++args))
			{
			case '?':
				key = '\177';
				args++;
				break;
			default:
				c = *(args++);
				if (islower(c))
					c = toupper(c);
				if (c < 64)
				{
					say("Illegal key sequence: ^%c", c);
					return;
				}
				key = c - 64;
				break;
			}
		}
		else if (*args == '\\')
		{
			key = *++args;
			args++;
		}
		else
			key = *(args++);
		edit_char((u_int)key);
	}
}

/*
 * write_binding: This will write to the given FILE pointer the information
 * about the specified key binding.  The format it writes it out is such that
 * it can be parsed back in later using LOAD or with the -l switch 
 */
static	void
write_binding(u_int c, u_int m, FILE *fp, int do_all)
{
	KeyMap	*map;
	char	*meta_str;

	if (c == 32)
		return;
	switch (m)
	{
	case 1:
		map = meta1_keys;
		meta_str = "META1-";
		break;
	case 2:
		map = meta2_keys;
		meta_str = "META2-";
		break;
	case 3:
		map = meta3_keys;
		meta_str = "META3-";
		break;
	case 4:
		map = meta4_keys;
		meta_str = "META4-";
		break;
	case 5:
		map = meta5_keys;
		meta_str = "META5-";
		break;
	case 6:
		map = meta6_keys;
		meta_str = "META6-";
		break;
	case 7:
		map = meta7_keys;
		meta_str = "META7-";
		break;
	case 8:
		map = meta8_keys;
		meta_str = "META8-";
		break;
	default:
		map = keys;
		meta_str = CP(empty_string());
		break;
	}
	if (map[c].changed)
	{
		fprintf(fp, "BIND %s%s %s", meta_str, display_key(c),
			key_names[map[c].index].name);
		if (map[c].stuff && (*(map[c].stuff)))
		{
			fprintf(fp, " %s\n", map[c].stuff);
		}
		else
			fprintf(fp, "\n");
	}
}

/*
 * save_bindings: this writes all the keys bindings for IRCII to the given
 * FILE pointer using the write_binding function 
 */
void
save_bindings(FILE *fp, int do_all)
{
	int	i;
	int	charsize = charset_size();

	for (i = 0; i < charsize; i++)
		write_binding((u_char) i, 0, fp, do_all);
	for (i = 0; i < charsize; i++)
		write_binding((u_char) i, 1, fp, do_all);
	for (i = 0; i < charsize; i++)
		write_binding((u_char) i, 2, fp, do_all);
	for (i = 0; i < charsize; i++)
		write_binding((u_char) i, 3, fp, do_all);
	for (i = 0; i < charsize; i++)
		write_binding((u_char) i, 4, fp, do_all);
	for (i = 0; i < charsize; i++)
		write_binding((u_char) i, 5, fp, do_all);
	for (i = 0; i < charsize; i++)
		write_binding((u_char) i, 6, fp, do_all);
	for (i = 0; i < charsize; i++)
		write_binding((u_char) i, 7, fp, do_all);
	for (i = 0; i < charsize; i++)
		write_binding((u_char) i, 8, fp, do_all);
}

KeyMap_Func
keys_get_func_from_index_and_meta(u_char key, int meta_char)
{
	switch (meta_char) {
	case 1:
		return key_names[meta1_keys[key].index].func;
	case 2:
		return key_names[meta2_keys[key].index].func;
	case 3:
		return key_names[meta3_keys[key].index].func;
	case 4:
		return key_names[meta4_keys[key].index].func;
	case 5:
		return key_names[meta5_keys[key].index].func;
	case 6:
		return key_names[meta6_keys[key].index].func;
	case 7:
		return key_names[meta7_keys[key].index].func;
	case 8:
		return key_names[meta8_keys[key].index].func;
	}
	return key_names[keys[key].index].func;
}

u_char *
keys_get_str_from_key_and_meta(u_char key, int meta_char)
{
	switch (meta_char) {
	case 1:
		return meta1_keys[key].stuff;
	case 2:
		return meta2_keys[key].stuff;
	case 3:
		return meta3_keys[key].stuff;
	case 4:
		return meta4_keys[key].stuff;
	case 5:
		return meta5_keys[key].stuff;
	case 6:
		return meta6_keys[key].stuff;
	case 7:
		return meta7_keys[key].stuff;
	case 8:
		return meta8_keys[key].stuff;
	}
	return keys[key].stuff;
}
