/*
 * names.c: This here is used to maintain a list of all the people currently
 * on your channel.  Seems to work 
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
IRCII_RCSID("@(#)$eterna: names.c,v 1.135 2014/12/20 20:32:39 mrg Exp $");

#include "ircaux.h"
#include "names.h"
#include "window.h"
#include "screen.h"
#include "server.h"
#include "lastlog.h"
#include "list.h"
#include "output.h"
#include "notify.h"
#include "vars.h"

/* NickList: structure for the list of nicknames of people on a channel */
struct nick_stru
{
	NickList *next;		/* pointer to next nickname entry */
	u_char	*nick;		/* nickname of person on channel */
	int	chanop;		/* True if the given nick has chanop */
	int	hasvoice;	/* Has voice? (Notice this is a bit
				 * unreliable if chanop) */
};

/* ChannelList: structure for the list of channels you are current on */
struct	channel_stru
{
	ChannelList *next;	/* pointer to next channel entry */
	u_char	*channel;	/* channel name */
	int	server;		/* server index for this channel */
	u_long	mode;		/* current mode settings for channel */
	u_char	*s_mode;	/* string representation of the above */
	int	limit;		/* max users for the channel */
	u_char	*key;		/* key for this channel */
	ChanListConnected connected;	/* connection status */
	Window	*window;	/* the window that the channel is "on" */
	NickList *nicks;	/* pointer to list of nicks on channel */
	ChanListStatus status;	/* different flags */
};

/* from names.h */
static	u_char	mode_str[] = MODE_STRING;

static	int	same_channel(ChannelList *, u_char *);
static	void	free_channel(ChannelList **);
static	void	show_channel(ChannelList *);
static	void	clear_channel(ChannelList *);
static	u_char	*recreate_mode(ChannelList *);
static	int	decifer_mode(u_char *, u_long *, ChanListStatus *,
			     NickList **, u_char **);
static	int	switch_channels_backend(ChannelList *);

/* clear_channel: erases all entries in a nick list for the given channel */
static	void
clear_channel(ChannelList *chan)
{
	NickList *tmp,
		*next;

	for (tmp = chan->nicks; tmp; tmp = next)
	{
		next = tmp->next;
		new_free(&tmp->nick);
		new_free(&tmp);
	}
	chan->nicks = NULL;
	chan->status &= ~CHAN_NAMES;
}

/*
 * we need this to deal with !channels.
 */
static	int
same_channel(ChannelList *chan, u_char	*channel)
{
	size_t	len, len2;

	/* take the easy way out */
	if (*chan->channel != '!' && *channel != '!')
		return (!my_stricmp(chan->channel, channel));

	/*
	 * OK, so what we have is chan->channel = "!!foo" and
	 * channel = "!JUNKfoo".
	 */
	len = my_strlen(chan->channel);	
	len2 = my_strlen(channel);	

	/* bail out on known stuff */
	if (len > len2)
		return (0);
	if (len == len2)
		return (!my_stricmp(chan->channel, channel));

	/*
	 * replace the channel name if we are the same!
	 */
	if (!my_stricmp(chan->channel + 2, channel + 2 + (len2 - len)))
	{
		malloc_strcpy(&chan->channel, channel);
		return 1;
	}
	return 0;
}

ChannelList *
lookup_channel(u_char *channel, int server, int do_unlink)
{
	ChannelList	*chan, *last = NULL;

	if (!channel || !*channel ||
	    (server == -1 && (server = get_primary_server()) == -1))
		return NULL;
	chan = server_get_chan_list(server);
	while (chan)
	{
		if (chan->server == server && same_channel(chan, channel))
		{
			if (do_unlink == CHAN_UNLINK)
			{
				if (last)
					last->next = chan->next;
				else
					server_set_chan_list(server, chan->next);
			}
			break;
		}
		last = chan;
		chan = chan->next;
	}
	return chan;
}

/*
 * add_channel: adds the named channel to the channel list.  If the channel
 * is already in the list, its attributes are modified accordingly with the
 * connected and copy parameters.
 */
void
add_channel(u_char *channel, u_char *key, int server, int connected,
	    ChannelList *copy)
{
	ChannelList *new;
	int	do_add = 0;

	/*
	 * avoid adding channel "0"
	 */
	if (channel[0] == '0' && channel[1] == '\0')
		return;

	if ((new = lookup_channel(channel, server, CHAN_NOUNLINK)) == NULL)
	{
		ChannelList *full_list;

		new = new_malloc(sizeof *new);
		new->channel = NULL;
		new->status = 0;
		new->key = 0;
		if (key)
			malloc_strcpy(&new->key, key);
		new->nicks = NULL;
		new->s_mode = empty_string();
		malloc_strcpy(&new->channel, channel);
		new->mode = 0;
		new->limit = 0;
		if ((new->window = is_bound(channel, server)) == NULL)
			new->window = curr_scr_win;
		do_add = 1;
		full_list = server_get_chan_list(server);
		add_to_list((List **)(void *)&full_list, (List *) new);
		server_set_chan_list(server, full_list);
	}
	else
	{
		if (new->connected != CHAN_LIMBO &&
		    new->connected != CHAN_JOINING)
			yell("--- add_channel: add_channel found channel "
			     "not CHAN_LIMBO/JOINING: %s", new->channel);
	}
	if (do_add || (connected == CHAN_JOINED))
	{
		new->server = server;
		clear_channel(new);
	}
	if (copy)
	{
		new->mode = copy->mode;
		new->limit = copy->limit;
		new->window = copy->window;
		malloc_strcpy(&new->key, copy->key);
	}
	new->connected = connected;
	if (connected == CHAN_JOINED && !is_current_channel(channel, server, 0))
	{
		Win_Trav wt;
		Window	*tmp, *expected,
			*possible = NULL;

		expected = new->window;

		wt.init = 1;
		while ((tmp = window_traverse(&wt)))
		{
			if (window_get_server(tmp) == server)
			{
				if (tmp == expected)
				{	
					set_channel_by_refnum(window_get_refnum(tmp),
							      channel);
					new->window = tmp;
					update_all_status();
					return;
				}
				else if (!possible)
					possible = tmp;
			}
		}
		if (possible)
		{
			set_channel_by_refnum(window_get_refnum(possible), channel);
			new->window = possible;
			update_all_status();
			return;
		}
		set_channel_by_refnum(0, channel);
		new->window = curr_scr_win;
	}
	update_all_windows();
}

void
rename_channel(u_char *oldchan, u_char *newchan, int server)
{
	ChannelList *new;

	if (!oldchan || !*oldchan || !newchan || !*newchan)
		return;
	new = lookup_channel(oldchan, server, CHAN_NOUNLINK);
	if (!new)
		return;

	malloc_strcpy(&new->channel, newchan);
	if (new->window)
		set_channel_by_refnum(window_get_refnum(new->window), newchan);

	/* are these necessary? */
	update_all_status();
	update_all_windows();
}

/*
 * add_to_channel: adds the given nickname to the given channel.  If the
 * nickname is already on the channel, nothing happens.  If the channel is
 * not on the channel list, nothing happens (although perhaps the channel
 * should be addded to the list?  but this should never happen) 
 */
void
add_to_channel(u_char *channel, u_char *nick, int server, int oper, int voice)
{
	NickList *new;
	ChannelList *chan;
	int	ischop = oper;
	int	hasvoice = voice;

	if ((chan = lookup_channel(channel, server, CHAN_NOUNLINK)))
	{
		if (*nick == '+')
		{
			hasvoice = 1;
			nick++;
		}
		if (*nick == '@')
		{
			nick++;
			if (!my_stricmp(nick, server_get_nickname(server))
			    && !((chan->status & CHAN_NAMES)
				 && (chan->status & CHAN_MODE)))
			{
				u_char	*mode = recreate_mode(chan);

				if (*mode)
				{
					int	old_server;

					old_server = set_from_server(server);
					send_to_server("MODE %s %s",
					    chan->channel, mode);
					set_from_server(old_server);
				}
				chan->status |= CHAN_CHOP;
			}
			ischop = 1;
		}

		if ((new = (NickList *)
		     remove_from_list((List **)(void *)&(chan->nicks), nick)))
		{
			new_free(&new->nick);
			new_free(&new);
		}
		new = new_malloc(sizeof *new);
		new->nick = NULL;
		new->chanop = ischop;
		new->hasvoice = hasvoice;
		malloc_strcpy(&(new->nick), nick);
		add_to_list((List **)(void *)&(chan->nicks), (List *) new);
	}
	notify_mark(nick, 1, 0);
}


/*
 * recreate_mode: converts the bitmap representation of a channels mode into
 * a string 
 */
static	u_char	*
recreate_mode(ChannelList *chan)
{
	int	mode_pos = 0,
		mode,
		showkey;
	static	u_char	*s;
	u_char	buffer[BIG_BUFFER_SIZE],
		modes[33],
		limit[33];

	limit[0] = '\0';	
	s = modes;
	mode = chan->mode;
	while (mode)
	{
		if (mode % 2)
			*s++ = mode_str[mode_pos];
		mode /= 2;
		mode_pos++;
	}
	*s = '\0';
	showkey = chan->key && *chan->key && !get_int_var(HIDE_CHANNEL_KEYS_VAR);
	if (chan->limit)
		snprintf(CP(limit), sizeof limit, " %d", chan->limit);

	snprintf(CP(buffer), sizeof buffer, "%s%s%s%s",
		 modes,
		 showkey ? UP(" ") : empty_string(),
		 showkey ? chan->key : empty_string(),
		 limit);

	malloc_strcpy(&chan->s_mode, buffer);
	return chan->s_mode;
}

/*
 * decifer_mode: This will figure out the mode string as returned by mode
 * commands and convert that mode string into a one byte bit map of modes 
 */
static	int
decifer_mode(u_char *mode_string, u_long *mode, ChanListStatus *chop,
	     NickList **nicks, u_char **key)
{
	u_char	*limit = 0;
	u_char	*person;
	int	add = 0;
	int	limit_set = 0;
	int	limit_reset = 0;
	u_char	*rest,
		*the_key;
	NickList *ThisNick;
	u_long	value = 0;

	if (!(mode_string = next_arg(mode_string, &rest)))
		return -1;
	for (; *mode_string; mode_string++)
	{
		switch (*mode_string)
		{
		case '+':
			add = 1;
			value = 0;
			break;
		case '-':
			add = 0;
			value = 0;
			break;
		case 'a':
			value = MODE_ANONYMOUS;
			break;
		case 'c':
			value = MODE_COLOURLESS;
			break;
		case 'i':
			value = MODE_INVITE;
			break;
		case 'k':
			value = MODE_KEY;
			the_key = next_arg(rest, &rest);
			if (add)
				malloc_strcpy(key, the_key);
			else
				new_free(key);
			break;	
		case 'l':
			value = MODE_LIMIT;
			if (add)
			{
				limit_set = 1;
				if (!(limit = next_arg(rest, &rest)))
					limit = empty_string();
				else if (0 == my_strncmp(limit, zero(), 1))
					limit_reset = 1, limit_set = 0, add = 0;
			}
			else
				limit_reset = 1;
			break;
		case 'm':
			value = MODE_MODERATED;
			break;
		case 'o':
			if ((person = next_arg(rest, &rest)) &&
			    !my_stricmp(person,
					server_get_nickname(get_from_server()))) {
				if (add)
					*chop |= CHAN_CHOP;
				else
					*chop &= ~CHAN_CHOP;
			}
			ThisNick = (NickList *)
				list_lookup((List **)(void *)nicks, person,
					    0, 0);
			if (ThisNick)
				ThisNick->chanop = add;
			break;
		case 'n':
			value = MODE_MSGS;
			break;
		case 'p':
			value = MODE_PRIVATE;
			break;
		case 'q':
			value = MODE_QUIET;
			break;
		case 'r':
			value = MODE_REOP;
			break;
		case 's':
			value = MODE_SECRET;
			break;
		case 't':
			value = MODE_TOPIC;
			break;
		case 'v':
			person = next_arg(rest, &rest);
			ThisNick = (NickList *)
				list_lookup((List **) nicks, person,
					    0, 0);
			if (ThisNick)
				ThisNick->hasvoice = add;
			break;
		case 'b':
		case 'e':
		case 'I':
		case 'O': /* this is a weird special case */
			(void) next_arg(rest, &rest);
			break;
		case 'R':
			value = MODE_REGONLY;
			break;
		}
		if (add)
			*mode |= value;
		else
			*mode &= ~value;
	}
	if (limit_set)
		return (my_atoi(limit));
	else if (limit_reset)
		return(0);
	else
		return(-1);
}

/*
 * get_channel_mode: returns the current mode string for the given channel
 */
u_char	*
get_channel_mode(u_char *channel, int server)
{
	ChannelList *tmp;

	if ((tmp = lookup_channel(channel, server, CHAN_NOUNLINK)) &&
	    (tmp->status & CHAN_MODE))
		return recreate_mode(tmp);
	return empty_string();
}

/*
 * update_channel_mode: This will modify the mode for the given channel
 * according the the new mode given.  
 */
void
update_channel_mode(u_char *channel, int server, u_char *mode)
{
	ChannelList *tmp;
	int	limit;

	if ((tmp = lookup_channel(channel, server, CHAN_NOUNLINK)) &&
	    (limit = decifer_mode(mode, &tmp->mode, &tmp->status,
				  &tmp->nicks, &tmp->key)) != -1)
		tmp->limit = limit;
}

/*
 * is_channel_mode: returns the logical AND of the given mode with the
 * channels mode.  Useful for testing a channels mode 
 */
int
is_channel_mode(u_char *channel, int mode, int server_index)
{
	ChannelList *tmp;

	if ((tmp = lookup_channel(channel, server_index, CHAN_NOUNLINK)))
		return (tmp->mode & mode);
	return 0;
}

static	void
free_channel(ChannelList **channel)
{
	clear_channel(*channel);
	new_free(&(*channel)->channel);
	new_free(&(*channel)->key);
	new_free(&(*channel)->s_mode);
	new_free(&(*channel));
}

/*
 * remove_channel: removes the named channel from the specified
 * server's channel list.
 * If the channel is not on the this list, nothing happens.
 * If the channel was the current channel, this will select the top of
 * the channel list to be the current_channel, or 0 if the list is empty. 
 */
void
remove_channel(u_char *channel, int server)
{
	ChannelList *tmp;

	if (channel)
	{
		int refnum = -1;

		tmp = lookup_channel(channel, server, CHAN_UNLINK);
		if (tmp)
		{
			if (tmp->window)
				refnum = window_get_refnum(tmp->window);
			free_channel(&tmp);
		}

		(void)is_current_channel(channel, server, refnum);
	}
	else
	{
		ChannelList *next;

		for (tmp = server_get_chan_list(server); tmp; tmp = next)
		{
			next = tmp->next;
			free_channel(&tmp);
		}
		server_set_chan_list(server, NULL);
	}
	update_all_windows();
}

/*
 * remove_from_channel: removes the given nickname from the given channel. If
 * the nickname is not on the channel or the channel doesn't exist, nothing
 * happens. 
 */
void
remove_from_channel(u_char *channel, u_char *nick, int server)
{
	ChannelList *chan;
	NickList *tmp;

	if (channel)
	{
		if ((chan = lookup_channel(channel, server, CHAN_NOUNLINK)))
		{
			if ((tmp = (NickList *)
				list_lookup((List **)(void *)&chan->nicks, nick,
					    0, REMOVE_FROM_LIST)))
			{
				new_free(&tmp->nick);
				new_free(&tmp);
			}
		}
	}
	else
	{
		for (chan = server_get_chan_list(server); chan; chan = chan->next)
		{
			if ((tmp = (NickList *)
				list_lookup((List **)(void *)&chan->nicks, nick,
					    0, REMOVE_FROM_LIST)))
			{
				new_free(&tmp->nick);
				new_free(&tmp);
			}
		}
	}
}

/*
 * rename_nick: in response to a changed nickname, this looks up the given
 * nickname on all you channels and changes it the new_nick 
 */
void
rename_nick(u_char *old_nick, u_char *new_nick, int server)
{
	ChannelList *chan;
	NickList *tmp;

	for (chan = server_get_chan_list(server); chan; chan = chan->next)
	{
		if ((chan->server == server) != 0)
		{
			if ((tmp = (NickList *)
				list_lookup((List **)(void *)&chan->nicks,
					    old_nick, 0, 0)))
			{
				new_free(&tmp->nick);
				malloc_strcpy(&tmp->nick, new_nick);
			}
		}
	}
}

/*
 * is_on_channel: returns true if the given nickname is in the given channel,
 * false otherwise.  Also returns false if the given channel is not on the
 * channel list. 
 */
int
is_on_channel(u_char *channel, int server, u_char *nick)
{
	ChannelList *chan;

	chan = lookup_channel(channel, server, CHAN_NOUNLINK);
	if (chan && (chan->connected == CHAN_JOINED)
	    && list_lookup((List **)(void *)&(chan->nicks), nick, 0, 0))
		return 1;
	return 0;
}

int
is_chanop(u_char *channel, u_char *nick)
{
	ChannelList *chan;
	NickList *Nick;

	if ((chan = lookup_channel(channel, get_from_server(), CHAN_NOUNLINK)) &&
	    chan->connected == CHAN_JOINED &&
	    /* channel may be "surviving" from a disconnect/connect
						check here too -Sol */
	    (Nick = (NickList *) list_lookup((List **)(void *)&(chan->nicks),
					     nick, 0, 0)) &&
	    Nick->chanop)
		return 1;
	return 0;
}

int
has_voice(u_char *channel, u_char *nick, int server)
{
	ChannelList *chan;
	NickList *Nick;

	if (channel && *channel &&
	    (chan = lookup_channel(channel, server, CHAN_NOUNLINK)) &&
	    chan->connected == CHAN_JOINED &&
		/* channel may be "surviving" from a disconnect/connect
						   check here too -Sol */
	    (Nick = (NickList *) list_lookup((List **)(void *)&(chan->nicks), nick,
					     0, 0)) &&
	    (Nick->chanop || Nick->hasvoice))
		return 1;
	return 0;
}

static	void
show_channel(ChannelList *chan)
{
	NickList *tmp;
	int	buffer_len,
		len;
	u_char	*nicks = NULL;
	u_char	*s;
	u_char	buffer[BIG_BUFFER_SIZE];

	s = recreate_mode(chan);
	*buffer = (u_char) 0;
	buffer_len = 0;
	for (tmp = chan->nicks; tmp; tmp = tmp->next)
	{
		len = my_strlen(tmp->nick);
		if (buffer_len + len >= (sizeof(buffer) / 2))
		{
			malloc_strcpy(&nicks, buffer);
			say("\t%s +%s (%s): %s", chan->channel, s,
			    server_get_name(chan->server), nicks);
			*buffer = (u_char) 0;
			buffer_len = 0;
		}
		my_strmcat(buffer, tmp->nick, sizeof buffer);
		my_strmcat(buffer, " ", sizeof buffer);
		buffer_len += len + 1;
	}
	malloc_strcpy(&nicks, buffer);
	say("\t%s +%s (%s): %s", chan->channel, s,
	    server_get_name(chan->server), nicks);
	new_free(&nicks);
}

/* list_channels: displays your current channel and your channel list */
void
list_channels(void)
{
	ChannelList *tmp;
	int	server,
		no = 1;
	int	first;

	if (connected_to_server() < 1)
	{
		say("You are not connected to a server, use /SERVER to connect.");
		return;
	}
	if (get_channel_by_refnum(0))
		say("Current channel %s", get_channel_by_refnum(0));
	else
		say("No current channel for this window");
	first = 1;
	for (tmp = server_get_chan_list(get_window_server(0));
	     tmp; tmp = tmp->next)
	{
		if (tmp->connected != CHAN_JOINED)
			continue;
		if (first)
			say("You are on the following channels:");
		show_channel(tmp);
		first = 0;
		no = 0;
	}

	if (connected_to_server() > 1)
	{
		for (server = 0; server < number_of_servers(); server++)
		{
			if (server == get_window_server(0))
				continue;
			first = 1;
			for (tmp = server_get_chan_list(server);
			     tmp; tmp = tmp->next)
			{
				if (tmp->connected != CHAN_JOINED)
					continue;
				if (first)
					say("Other servers:");
				show_channel(tmp);
				first = 0;
				no = 0;
			}
		}
	}
	if (no)
		say("You are not on any channels");
}

/*
 * backend of switch_channels().  
 */
static int
switch_channels_backend(ChannelList * chan)
{
	u_char *newchan;
	const	int	from_server = get_from_server();

	for (; chan; chan = chan->next)
	{
		newchan = chan->channel;
		if (!is_current_channel(newchan, from_server, 0)
		    && !(is_bound(newchan, from_server)
			 && curr_scr_win != chan->window)
		    && (get_int_var(SWITCH_TO_QUIET_CHANNELS)
		        || !(chan->mode & MODE_QUIET)))
		{
			set_channel_by_refnum(0, newchan);
			update_all_windows();
			return 0;
		}
	}
	return 1;
}

void
switch_channels(u_int key, u_char *ptr)
{
	ChannelList *	tmp;
	const	int	from_server = get_from_server();

	if (server_get_chan_list(from_server) &&
	    get_channel_by_refnum(0) &&
	    (tmp = lookup_channel(get_channel_by_refnum(0), from_server,
				  CHAN_NOUNLINK)) &&
	    switch_channels_backend(tmp->next))
		switch_channels_backend(server_get_chan_list(from_server));
}

void
change_server_channels(int old, int new)
{
	ChannelList *tmp;

	if (new == old)
		return;
	if (old > -1)
	{
		for (tmp = server_get_chan_list(old); tmp ;tmp = tmp->next)
			tmp->server = new;
		server_set_chan_list(new, server_get_chan_list(old));
	}
	else
		server_set_chan_list(new, NULL);
}

void
clear_channel_list(int server)
{
	ChannelList *tmp, *next;
	Window	*ptr;
	Win_Trav wt;

	wt.init = 1;
	while ((ptr = window_traverse(&wt)))
		if (window_get_server(ptr) == server && window_get_current_channel(ptr))
			window_set_current_channel(ptr, NULL);
	
	for (tmp = server_get_chan_list(server); tmp; tmp = next)
	{
		next = tmp->next;
		free_channel(&tmp);
	}
	server_set_chan_list(server, NULL);
	return;
}

/*
 * reconnect_all_channels: used after you get disconnected from a server, 
 * clear each channel nickname list and re-JOINs each channel in the 
 * channel_list ..  
 */
void
reconnect_all_channels(int server)
{
	ChannelList *tmp = NULL;
	u_char	*mode, *chan;

	for (tmp = server_get_chan_list(server); tmp; tmp = tmp->next)
	{
		mode = recreate_mode(tmp);
		chan = tmp->channel;
		if (server_get_version(server) >= Server2_8)
			send_to_server("JOIN %s%s%s", chan, tmp->key ? " " : "",
				       tmp->key ? tmp->key : (u_char *) "");
		else
			send_to_server("JOIN %s%s%s", chan, mode ? " " : "",
				       mode ? mode : (u_char *) "");
		tmp->connected = CHAN_JOINING;
	}
}

u_char	*
what_channel(u_char *nick, int server)
{
	ChannelList *tmp;
	u_char *channel = window_get_current_channel(curr_scr_win);

	if (channel && is_on_channel(channel, window_get_server(curr_scr_win),
				     nick))
		return channel;

	for (tmp = server_get_chan_list(get_from_server()); tmp; tmp = tmp->next)
		if (list_lookup((List **)(void *)&(tmp->nicks), nick,
				0, 0))
			return tmp->channel;

	return NULL;
}

u_char	*
walk_channels(u_char *nick, int init, int server)
{
	static	ChannelList *tmp = NULL;

	if (server < 0)
		server = get_from_server();
	if (init)
		tmp = server_get_chan_list(server);
	else if (tmp)
		tmp = tmp->next;
	for (;tmp ; tmp = tmp->next)
		if (tmp->server == server &&
		    list_lookup((List **)(void *)&(tmp->nicks), nick,
				0, 0))
			return (tmp->channel);
	return NULL;
}

int
get_channel_oper(u_char *channel, int server)
{
	ChannelList *chan;

	if ((chan = lookup_channel(channel, server, CHAN_NOUNLINK)))
		return (chan->status & CHAN_CHOP);
	else
		return 1;
}

void
set_channel_window(Window *window, u_char *channel, int server)
{
	ChannelList	*tmp;

	if (!channel || server < 0)
		return;
	for (tmp = server_get_chan_list(server); tmp; tmp = tmp->next)
		if (!my_stricmp(channel, tmp->channel))
		{
			tmp->window = window;
			return;
		}
}

u_char	*
create_channel_list(Window *window)
{
	ChannelList	*tmp;
	u_char	*value = NULL;
	u_char	buffer[BIG_BUFFER_SIZE];

	*buffer = '\0';
	if (window_get_server(window) >= 0)
		for (tmp = server_get_chan_list(window_get_server(window));
		     tmp; tmp = tmp->next)
		{
			my_strmcat(buffer, tmp->channel, sizeof buffer);
			my_strmcat(buffer, " ", sizeof buffer);
		}
	malloc_strcpy(&value, buffer);

	return value;
}

void
channel_server_delete(int server)
{
	ChannelList *tmp;
	int	i;

	for (i = server + 1; i < number_of_servers(); i++)
		for (tmp = server_get_chan_list(i); tmp; tmp = tmp->next)
			if (tmp->server >= server)
				tmp->server--;
}

int
chan_is_connected(u_char *channel, int server)
{
	ChannelList *	cp = lookup_channel(channel, server, CHAN_NOUNLINK);

	if (!cp)
		return 0;

	return (cp->connected == CHAN_JOINED);
}

void
mark_not_connected(int server)
{
	ChannelList	*tmp;

	if (server >= 0)
		for (tmp = server_get_chan_list(server); tmp; tmp = tmp->next)
		{
			tmp->status = 0;
			tmp->connected = CHAN_LIMBO;
		}
}

void
free_nicks(Window *window)
{
	NickList *nicks = *window_get_nicks(window);
	NickList *tmp, *next;

	for (tmp = nicks; tmp; tmp = next)
	{
		next = tmp->next;
		new_free(&tmp->nick);
		new_free(&tmp);
	}
}

void
display_nicks_info(Window *window)
{
	NickList *nicks = *window_get_nicks(window);

	if (nicks)
	{
		say("\tName list:");
		for (; nicks; nicks = nicks->next)
			say("\t  %s", nicks->nick);
	}
}

int
nicks_add_to_window(Window *window, u_char *nick)
{
	NickList **nicks = window_get_nicks(window);

	if (!find_in_list((List **)(void *)nicks, nick, 0))
	{
		NickList *new = new_malloc(sizeof *new);
		new->nick = NULL;
		malloc_strcpy(&new->nick, nick);
		add_to_list((List **)(void *)nicks, (List *) new);
		return 1;
	}
	return 0;
}

int
nicks_remove_from_window(Window *window, u_char *nick)
{
	NickList **nicks = window_get_nicks(window);
	NickList *new;

	if ((new = (NickList *)
		remove_from_list((List **)(void *)nicks, nick)) != NULL)
	{
		new_free(&new->nick);
		new_free(&new);
		return 1;
	}
	return 0;
}

u_char	*
function_chanusers(u_char *input)
{
	ChannelList	*chan;
	NickList	*nicks;
	u_char	*result = NULL;
	int	len = 0;
	int	notfirst = 0;

	chan = lookup_channel(input, get_from_server(), CHAN_NOUNLINK);
	if (NULL == chan)
		return NULL;

	for (nicks = chan->nicks; nicks; nicks = nicks->next)
		len += (my_strlen(nicks->nick) + 1);
	result = new_malloc(len + 1);
	*result = '\0';

	for (nicks = chan->nicks; nicks; nicks = nicks->next)
	{
		if (notfirst)
			my_strmcat(result, " ", len);
		else
			notfirst = 1;
		my_strmcat(result, nicks->nick, len);
	}

	return (result);
}

int
channel_mode_lookup(u_char *channel, int check_modes, int set_modes)
{
	ChannelList *tmp;

	tmp = lookup_channel(channel, parsing_server(), CHAN_NOUNLINK);
	if (tmp && (tmp->status & check_modes) != check_modes) {
		tmp->status |= set_modes;
		return 1;
	}
	return 0;
}

/*
 * given a server and a channel, look up a window it belongs to.
 * NULL means the channel was not found.
 */
Window *
channel_window(u_char *channel, int server)
{
	ChannelList *chan;

	chan = lookup_channel(channel, server, CHAN_NOUNLINK);
	if (!chan)
		return NULL;
	return chan->window ? chan->window : curr_scr_win;
}

int
channel_is_current_window(u_char *channel, int server)
{
	ChannelList *chan;

	chan = lookup_channel(channel, server, CHAN_NOUNLINK);

	return chan && chan->window != curr_scr_win;
}

/*
 * backend for is_current_channel() that interrogates the channel
 * list for each server.
 */
int
channel_is_on_window(u_char *channel, int server, Window *found_window,
		     int refnum)
{
	Window		*tmp;
	ChannelList	*chan;
	ChannelList	*possible = NULL;
	int		found = 0;

	for (chan = server_get_chan_list(server); chan; chan = chan->next)
	{
		if (!my_stricmp(chan->channel, channel))
			continue;
		if (chan->window == found_window)
		{
			set_channel_by_refnum(window_get_refnum(found_window),
					      chan->channel);
			return 1;
		}
		if (!get_int_var(SAME_WINDOW_ONLY_VAR) &&
		    !is_bound(chan->channel, server) &&
		    chan->window != found_window)
		{
			int	is_current = 0;
			Win_Trav wt;

			wt.init = 1;
			while ((tmp = window_traverse(&wt)))
			{
				if (window_get_server(tmp) != server)
					continue;
				if (window_get_current_channel(tmp) &&
				    !my_stricmp(chan->channel,
						window_get_current_channel(tmp)))
					is_current = 1;
			}
			if (!is_current)
				possible = chan;
		}
	}
	if (!get_int_var(SAME_WINDOW_ONLY_VAR))
	{
		u_char *delete_channel;

		if (possible)
		{
			set_channel_by_refnum(window_get_refnum(found_window),
					      possible->channel);
			return 1;
		}
		delete_channel = get_channel_by_refnum((u_int)refnum);
		if (delete_channel &&
		    !(is_bound(delete_channel, server) &&
		    window_get_refnum(found_window) != refnum))
			set_channel_by_refnum(window_get_refnum(found_window),
					      get_channel_by_refnum((u_int)refnum));
	}
	return found;
}

int
channels_move_server_simple(int old_serv, int server)
{
	ChannelList *tmp;
	int moved = 0;

	for (tmp = server_get_chan_list(old_serv); tmp; tmp = tmp->next)
	{
		if (!moved)	/* If we're here, it means
				   we're going to transfer
				   channels to the new server,
				   so we dump old channels
				   first, but only once -Sol */
		{
			moved++;
			clear_channel_list(server);
		}
		add_channel(tmp->channel, 0, server, CHAN_LIMBO, tmp);
	}
	return moved;
}

void
channels_move_server_complex(Window *window, Window *new_win, int old_serv,
			     int server, int misc, int moved)
{
	ChannelList *tmp;

	for (tmp = server_get_chan_list(old_serv); tmp; tmp = tmp->next)
	{
		if (tmp->window != window &&
		    (!window_get_server_group(window) ||
		     window_get_server_group(tmp->window) !=
		     window_get_server_group(window)))
			continue;

		/* Found a channel to be relocated -Sol */
		if (window_get_sticky(tmp->window) || (misc & WIN_FORCE))
		{	/* This channel moves -Sol */
			int	old;

			if (!moved)
			{
				moved++;
				clear_channel_list(server);
			}
			/* Copy it -Sol */
			add_channel(tmp->channel, 0, server, CHAN_LIMBO, tmp);
			/* On old_serv, leave it -Sol */
			old = set_from_server(old_serv);
			send_to_server("PART %s", tmp->channel);
			set_from_server(old);
			remove_channel(tmp->channel, old_serv);
		}
		else
			tmp->window = new_win;
	}
}


void
window_list_channels(Window *window)
{
	ChannelList *tmp;
	int	print_one = 0;

	if (!window)
		return;

	for (tmp = server_get_chan_list(window_get_server(window));
	     tmp; tmp = tmp->next)
		if (tmp->window == window)
		{
			if (!print_one)
			{
				say("Channels currently in this window:");
				print_one++;
			}
			say("\t%s", tmp->channel);
		}
	if (!print_one)
		say("There are no channels in this window");
}

/*
 * realloc_channels: Attempts to reallocate a channel to a window of the
 * same server
 *
 * XXX mrg this looks broken to me but i'm not sure.  blah.
 */
void
realloc_channels(Window *window)
{
	Window	*tmp;
	Win_Trav wt;
	ChannelList	*chan;
	int	server = window_get_server(window);

	wt.init = 1;
	while ((tmp = window_traverse(&wt)))
	{
		if (window != tmp && window_get_server(tmp) == server)
		{
			for (chan = server_get_chan_list(server);
			     chan; chan = chan->next)
				if (chan->window == window)
				{
					chan->window = tmp;
					if (!window_get_current_channel(tmp))
						set_channel_by_refnum(window_get_refnum(tmp), chan->channel);
				}
			return;
		}
	}

	for (chan = server_get_chan_list(server); chan; chan = chan->next)
	{
		chan->window = NULL;
	}
}

/* swap_channels_win_ptr: must be called because swap_window modifies the
 * content of *v_window and *window instead of swapping pointers
 */
void
channel_swap_win_ptr(Window *v_window, Window *window)
{
	ChannelList	*chan;
	int	server = window_get_server(window);
	int	v_server = window_get_server(v_window);

	if (v_server != -1)
	{
		for (chan = server_get_chan_list(v_server);
		     chan; chan = chan->next)
			if (chan->window == v_window)
				chan->window = window;
			else if (chan->window == window)
				chan->window = v_window;
	}
	if (server == v_server)
		return;
	if (server != -1)
		for (chan = server_get_chan_list(server);
		     chan; chan = chan->next)
			if (chan->window == window)
				chan->window = v_window;
}

int
nicks_has_who_from(NickList **nicks)
{
	return find_in_list((List **)(void *)nicks, current_who_from(), 0) != NULL;
}
