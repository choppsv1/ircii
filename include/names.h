/*
 * names.h: Header for names.c
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
 * @(#)$eterna: names.h,v 1.48 2014/02/20 10:31:39 mrg Exp $
 */

#ifndef irc__names_h_
#define irc__names_h_

#include "irc.h"

/*
 * MODE_STRING and other MODE_FOO defines need to be kept in sync.
 * MODE_STRING refers to the bits in the mode bitmask.  the letters
 * and numbers need to match up.
 */
#define MODE_STRING	"aciklmnpqrstR"
#define MODE_ANONYMOUS	((u_long) 0x0001)
#define MODE_COLOURLESS	((u_long) 0x0002)
#define MODE_INVITE	((u_long) 0x0004)
#define MODE_KEY	((u_long) 0x0008)
#define MODE_LIMIT	((u_long) 0x0010)
#define MODE_MODERATED	((u_long) 0x0020)
#define MODE_MSGS	((u_long) 0x0040)
#define MODE_PRIVATE	((u_long) 0x0080)
#define MODE_QUIET	((u_long) 0x0100)
#define MODE_REOP	((u_long) 0x0200)
#define MODE_SECRET	((u_long) 0x0400)
#define MODE_TOPIC	((u_long) 0x0800)
#define MODE_REGONLY	((u_long) 0x1000)

/* for lookup_channel() */
#define	CHAN_NOUNLINK	1
#define CHAN_UNLINK	2

typedef	struct channel_stru ChannelList;

typedef enum {
	CHAN_LIMBO = 	-1,
	CHAN_JOINING =	0,
	CHAN_JOINED =	1,
} ChanListConnected;

typedef enum {
	CHAN_CHOP =	0x01,
	CHAN_NAMES =	0x04,
	CHAN_MODE =	0x08,
} ChanListStatus;

typedef struct nick_stru NickList;

	int	is_channel_mode(u_char *, int, int);
	int	is_chanop(u_char *, u_char *);
	int	has_voice(u_char *, u_char *, int);
	ChannelList	*lookup_channel(u_char *, int, int);
	u_char	*get_channel_mode(u_char *, int);
	void	add_channel(u_char *, u_char *, int, int, ChannelList *);
	void	rename_channel(u_char *, u_char *, int);
	void	add_to_channel(u_char *, u_char *, int, int, int);
	void	remove_channel(u_char *, int);
	void	remove_from_channel(u_char *, u_char *, int);
	int	is_on_channel(u_char *, int, u_char *);
	void	list_channels(void);
	void	reconnect_all_channels(int);
	void	switch_channels(u_int, u_char *);
	u_char	*what_channel(u_char *, int);
	u_char	*walk_channels(u_char *, int, int);
	void	rename_nick(u_char *, u_char *, int);
	void	update_channel_mode(u_char *, int, u_char *);
	void	set_channel_window(Window *, u_char *, int);
	u_char	*create_channel_list(Window *);
	int	get_channel_oper(u_char *, int);
	void	channel_server_delete(int);
	void	change_server_channels(int, int);
	void	clear_channel_list(int);
	void	set_waiting_channel(int);
	int	chan_is_connected(u_char *, int);
	void	mark_not_connected(int);
	void	free_nicks(Window *);
	void	display_nicks_info(Window *);
	int	nicks_add_to_window(Window *, u_char *);
	int	nicks_remove_from_window(Window *, u_char *);
	u_char	*function_chanusers(u_char *);
	int	channel_mode_lookup(u_char *, int, int);
	Window *channel_window(u_char *, int);
	int	channel_is_current_window(u_char *, int);
	int	channel_is_on_window(u_char *, int, Window *, int);
	int	channels_move_server_simple(int, int);
	void	channels_move_server_complex(Window *, Window *, int, int, int, int);
	void	window_list_channels(Window *);
	void	realloc_channels(Window *);
	void	channel_swap_win_ptr(Window *, Window *);
	int	nicks_has_who_from(NickList **);

#endif /* irc__names_h_ */
