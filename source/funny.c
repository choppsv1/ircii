/*
 * funny.c: Well, I put some stuff here and called it funny.  So sue me. 
 *
 * Written by Michael Sandrof
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
IRCII_RCSID("@(#)$eterna: funny.c,v 1.51 2014/03/14 01:57:16 mrg Exp $");

#include "ircaux.h"
#include "hook.h"
#include "vars.h"
#include "funny.h"
#include "names.h"
#include "server.h"
#include "lastlog.h"
#include "ircterm.h"
#include "output.h"
#include "numbers.h"
#include "parse.h"
#include "screen.h"

static	u_char	*match_str = NULL;

static	int	funny_min;
static	int	funny_max;
static	int	funny_flags;

void
funny_match(u_char *stuff)
{
	malloc_strcpy(&match_str, stuff);
}

void
set_funny_flags(int min, int max, int flags)
{
	funny_min = min;
	funny_max = max;
	funny_flags = flags;
}

struct	WideListInfoStru
{
	u_char	*channel;
	int	users;
};

typedef	struct WideListInfoStru WideList;

static	WideList **wide_list = NULL;
static	int	wl_size = 0;
static	size_t	wl_elements = 0;

static	int	funny_widelist_users(WideList **, WideList **);
static	int	funny_widelist_names(WideList **, WideList **);

static	int
funny_widelist_users(WideList **left, WideList **right)
{
	if ((**left).users > (**right).users)
		return -1;
	else if ((**right).users > (**left).users)
		return 1;
	else
		return my_stricmp((**left).channel, (**right).channel);
}

static	int
funny_widelist_names(WideList **left, WideList **right)
{
	int	comp;

	if ((comp = my_stricmp((**left).channel, (**right).channel)) != 0)
		return comp;
	else if ((**left).users > (**right).users)
		return -1;
	else if ((**right).users > (**left).users)
		return 1;
	else
		return 0;
}


void
funny_print_widelist(void)
{
	int	i;
	u_char	buffer1[BIG_BUFFER_SIZE];
	u_char	buffer2[BIG_BUFFER_SIZE];

	if (!wide_list)
		return;

	if (funny_flags & FUNNY_NAME)
		qsort((void *) wide_list, wl_elements, sizeof(WideList *),
			(int (*)(const void *, const void *)) funny_widelist_names);
	else if (funny_flags & FUNNY_USERS)
		qsort((void *) wide_list, wl_elements, sizeof(WideList *),
			(int (*)(const void *, const void *)) funny_widelist_users);

	*buffer1 = '\0';
	for (i = 0; i < wl_elements; i++)
	{
		snprintf(CP(buffer2), sizeof buffer2, "%s(%d) ", wide_list[i]->channel,
				wide_list[i]->users);
		if (my_strlen(buffer1) + my_strlen(buffer2) > get_co() - 5)
		{
			if (do_hook(WIDELIST_LIST, "%s", buffer1))
				say("%s", buffer1);
			*buffer1 = '\0';
		}
		my_strmcat(buffer1, buffer2, sizeof buffer1);
	}
	if (*buffer1 && do_hook(WIDELIST_LIST, "%s", buffer1))
		say("%s" , buffer1);
	for (i = 0; i < wl_elements; i++)
	{
		new_free(&wide_list[i]->channel);
		new_free(&wide_list[i]);
	}
	new_free(&wide_list);
	wl_elements = wl_size = 0;
}

void
funny_list(u_char *from, u_char **ArgList)
{
	u_char	*channel,
		*user_cnt,
		*line;
	WideList **new_list;
	int	cnt;
	static	u_char	format[25];
	static	int	last_width = -1;

	if (last_width != get_int_var(CHANNEL_NAME_WIDTH_VAR))
	{
		if ((last_width = get_int_var(CHANNEL_NAME_WIDTH_VAR)) != 0)
			snprintf(CP(format), sizeof format, "*** %%-%u.%us %%-5s  %%s",
				(u_char) last_width,
				(u_char) last_width);
		else
			my_strmcpy(format, "*** %s\t%-5s  %s", sizeof format);
	}
	channel = ArgList[0];
	user_cnt = ArgList[1];
	line = PasteArgs(ArgList, 2);
	if (funny_flags & FUNNY_TOPIC && !(line && *line))
			return;
	cnt = my_atoi(user_cnt);
	if (funny_min && (cnt < funny_min))
		return;
	if (funny_max && (cnt > funny_max))
		return;
	if ((funny_flags & FUNNY_PRIVATE) && (*channel != '*'))
		return;
	if ((funny_flags & FUNNY_PUBLIC) && (*channel == '*'))
		return;
	if (match_str)
	{
		if (wild_match(match_str, channel) == 0)
			return;
	}
	if (funny_flags & FUNNY_WIDE)
	{
		if (wl_elements >= wl_size)
		{
			const size_t increase = 50;
			new_list = new_malloc(sizeof(*new_list) * (wl_size + increase));
			memset(new_list, 0, sizeof(*new_list) * (wl_size + increase));
			if (wl_size)
				memmove(new_list, wide_list, sizeof(*new_list) * wl_size);
			wl_size += increase;
			new_free(&wide_list);
			wide_list = new_list;
		}
		wide_list[wl_elements] = new_malloc(sizeof *wide_list);
		wide_list[wl_elements]->channel = NULL;
		wide_list[wl_elements]->users = cnt;
		malloc_strcpy(&wide_list[wl_elements]->channel,
				(*channel != '*') ? channel : (u_char *) "Prv");
		wl_elements++;
		return;
	}
	if (do_hook(current_numeric(), "%s %s %s %s", from,  channel, user_cnt,
	    line) && do_hook(LIST_LIST, "%s %s %s", channel, user_cnt, line))
	{
		if (channel && user_cnt)
		{
			if (*channel == '*')
				put_it(CP(format), "Prv", user_cnt, line);
			else
				put_it(CP(format), channel, user_cnt, line);
		}
	}
}

void
funny_namreply(u_char *from, u_char **Args)
{
	u_char	*type,
		*nick,
		*channel;
	static	u_char	format[40];
	static	int	last_width = -1;
	int	cnt;
	u_char	*ptr;
	u_char	*line;

	PasteArgs(Args, 2);
	type = Args[0];
	channel = Args[1];
	line = Args[2];
	save_message_from();
	message_from(channel, LOG_CRAP);
	if (channel_mode_lookup(channel, CHAN_NAMES | CHAN_MODE, CHAN_NAMES))
	{
		if (do_hook(current_numeric(), "%s %s %s %s", from, type, channel,
			line) && get_int_var(SHOW_CHANNEL_NAMES_VAR))
			say("Users on %s: %s", channel, line);
		while ((nick = next_arg(line, &line)) != NULL)
			add_to_channel(channel, nick, parsing_server(), 0, 0);
		goto out;
	}
	if (last_width != get_int_var(CHANNEL_NAME_WIDTH_VAR))
	{
		if ((last_width = get_int_var(CHANNEL_NAME_WIDTH_VAR)) != 0)
			snprintf(CP(format), sizeof format, "%%s: %%-%u.%us %%s",
				(u_char) last_width,
				(u_char) last_width);
		else
			my_strmcpy(format, "%s: %s\t%s", sizeof format);
	}
	ptr = line;
	for (cnt = -1; ptr; cnt++)
	{
		if ((ptr = my_index(ptr, ' ')) != NULL)
			ptr++;
	}
	if (funny_min && (cnt < funny_min))
		return;
	else if (funny_max && (cnt > funny_max))
		return;
	if ((funny_flags & FUNNY_PRIVATE) && (*type == '='))
		return;
	if ((funny_flags & FUNNY_PUBLIC) && (*type == '*'))
		return;
	if (type && channel)
	{
		if (match_str)
		{
			if (wild_match(match_str, channel) == 0)
				return;
		}
		if (do_hook(current_numeric(), "%s %s %s %s", from, type, channel,
			line) && do_hook(NAMES_LIST, "%s %s", channel, line))
		{
			switch (*type)
			{
			case '=':
				if (last_width && (my_strlen(channel) > last_width))
				{
					channel[last_width-1] = '>';
					channel[last_width] = (u_char) 0;
				}
				put_it(CP(format), "Pub", channel, line);
				break;
			case '*':
				put_it(CP(format), "Prv", channel, line);
				break;
			case '@':
				put_it(CP(format), "Sec", channel, line);
				break;
			}
		}
	}
out:
	restore_message_from();
}

void
funny_mode(u_char *from, u_char **ArgList)
{
	u_char	*mode, *channel;

	if (!ArgList[0])
		return;
	if (server_get_version(parsing_server()) < Server2_6)
	{
		channel = NULL;
		mode = ArgList[0];
		PasteArgs(ArgList, 0);
	}
	else
	{
		channel = ArgList[0];
		mode = ArgList[1];
		PasteArgs(ArgList, 1);
	}
	/* if (ignore_mode) */
	if (channel && channel_mode_lookup(channel, CHAN_NAMES | CHAN_MODE, CHAN_MODE))
	{
		update_channel_mode(channel, parsing_server(), mode);
		update_all_status();
	}
	else
	{
		save_message_from();
		message_from(channel, LOG_CRAP);
		if (channel)
		{
			if (do_hook(current_numeric(), "%s %s %s", from,
					channel, mode))
				put_it("%s Mode for channel %s is \"%s\"",
					numeric_banner(), channel, mode);
		}
		else
		{
			if (do_hook(current_numeric(), "%s %s", from, mode))
				put_it("%s Channel mode is \"%s\"",
					numeric_banner(), mode);
		}
		restore_message_from();
	}
}

void
update_user_mode(u_char *modes)
{
	int	onoff = 1;

	while (*modes)
	{
		switch(*modes++)
		{
		case '-':
			onoff = 0;
			break;
		case '+':
			onoff = 1;
			break;
		case 'o':
		case 'O':
			server_set_operator(parsing_server(), onoff);
			break;
		case 's':
		case 'S':
			server_set_flag(parsing_server(), USER_MODE_S, onoff);
			break;
		case 'i':
		case 'I':
			server_set_flag(parsing_server(), USER_MODE_I, onoff);
			break;
		case 'w':
		case 'W':
			server_set_flag(parsing_server(), USER_MODE_W, onoff);
			break;
		case 'r':
			server_set_flag(parsing_server(), USER_MODE_R, onoff);
			break;
		case 'a':
			server_set_flag(parsing_server(), USER_MODE_A, onoff);
			break;
		case 'z':
			server_set_flag(parsing_server(), USER_MODE_Z, onoff);
			break;
		}
	}
}

void
reinstate_user_modes(void)
{
	u_char	modes[10];
	u_char	*c;

	if (server_get_version(parsing_server()) < Server2_7)
		return;
	c = modes;
	if (server_get_flag(parsing_server(), USER_MODE_W))
		*c++ = 'w';
	if (server_get_flag(parsing_server(), USER_MODE_S))
		*c++ = 's';
	if (server_get_flag(parsing_server(), USER_MODE_I))
		*c++ = 'i';
	*c = '\0';
	if (c != modes)
		send_to_server("MODE %s +%s",
			server_get_nickname(parsing_server()),
		modes);
}
