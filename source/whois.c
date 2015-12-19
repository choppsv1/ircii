/*
 * whois.c: Some tricky routines for querying the server for information
 * about a nickname using WHOIS.... all the time hiding this from the user.  
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

#undef MONITOR_Q /* this one is for monitoring of the 'whois queue' (debug) */

#include "irc.h"
IRCII_RCSID("@(#)$eterna: whois.c,v 1.78 2015/02/22 12:43:19 mrg Exp $");

#include "whois.h"
#include "hook.h"
#include "lastlog.h"
#include "vars.h"
#include "server.h"
#include "ignore.h"
#include "ircaux.h"
#include "notify.h"
#include "numbers.h"
#include "window.h"
#include "edit.h"
#include "output.h"
#include "parse.h"
#include "ctcp.h"

struct whois_queue_stru
{
	u_char	*nick;		/* nickname of whois'ed person(s) */
	u_char	*text;		/* additional text */
	int	type;		/* Type of WHOIS queue entry */
	/*
	 * called with func((WhoisStuff *)stuff, nick, text) 
	 */
	void	 (*func)(WhoisStuff *, u_char *, u_char *);
	WhoisQueue	*next;	/* next element in queue */
};

/* current setting for BEEP_ON_MSG */
static	int	beep_on_level;

static	int	ignore_whois_crap = 0;
static	int	eat_away = 0;
/*
static	WhoisStuff whois_stuff =
{
	NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, 0, 0, 0
};
*/

static	u_char	show_away_flag = 0;

static	void	typed_add_to_whois_queue(int, u_char *, void (*)(WhoisStuff *, u_char *, u_char *), char *, va_list);
static	u_char	*whois_queue_head(int);
static	int	whois_type_head(int);
static	void	(*whois_func_head(int))(WhoisStuff *, u_char *, u_char *);
static	WhoisQueue	*remove_from_whois_queue(int);

void
set_beep_on_msg(u_char *str)
{
	beep_on_level = parse_lastlog_level(str);
	set_string_var(BEEP_ON_MSG_VAR, bits_to_lastlog_level(beep_on_level));
}

/*
 * whois_queue_head: returns the nickname at the head of the whois queue, or
 * NULL if the queue is empty.  It does not modify the queue in any way. 
 */
static	u_char	*
whois_queue_head(int server_index)
{
	WhoisQueue *wq;

	if ((wq = server_get_qhead(server_index)) != NULL)
		return wq->nick;
	return NULL;
}

static	int
whois_type_head(int server_index)
{
	WhoisQueue *wq;

	if ((wq = server_get_qhead(server_index)) != NULL)
		return wq->type;
	return -1;
}

static	void (*
whois_func_head (int server_index))(WhoisStuff *, u_char *, u_char *)
{
	WhoisQueue *wq;

	if ((wq = server_get_qhead(server_index)) != NULL)
		return wq->func;
	else
		return NULL;
}

/*
 * remove_from_whois_queue: removes the top element of the whois queue and
 * returns the element as its function value.  This routine repairs handles
 * all queue stuff, but the returned element is mallocd and must be freed by
 * the calling routine 
 */
static	WhoisQueue *
remove_from_whois_queue(int server_index)
{
	WhoisQueue *new;

	if (server_get_version(server_index) == ServerICB)
	{
		yell("--- remove_from_whois_queue: panic?  attempted on ICB server..");
	}
	new = (WhoisQueue *) server_get_qhead(server_index);
	if (new)
	{
		server_set_qhead(server_index, new->next);
		if (new->next == NULL)
			server_set_qtail(server_index, NULL);
	}
	return (new);
}

/*
 * clean_whois_queue: this empties out the whois queue.  This is used after
 * server reconnection to assure that no bogus entries are left in the whois
 * queue 
 */
void
clean_whois_queue(void)
{
	WhoisQueue *thing;
	const	int	from_server = get_from_server();

	while (whois_queue_head(from_server))
	{
		thing = remove_from_whois_queue(from_server);
		new_free(&thing->nick);
		new_free(&thing->text);
		new_free(&thing);
	}
	ignore_whois_crap = 0;
	eat_away = 0;
}

/*
 * ison_returned: this is called when numeric 303 is received in
 * numbers.c. ISON must always be the property of the WHOIS queue.
 * Although we will check first that the top element expected is
 * actually an ISON.
 */
void
ison_returned(u_char *from, u_char **ArgList)
{
	WhoisQueue *thing;

	if (whois_type_head(parsing_server()) == WHOIS_ISON)
	{
		thing = remove_from_whois_queue(parsing_server());
		thing->func(NULL, thing->nick, ArgList[0]? ArgList[0] : empty_string());
		new_free(&thing->nick);
		new_free(&thing->text);
		new_free(&thing);
	}
	else
		ison_now(NULL, ArgList[0] ? ArgList[0] : empty_string(), NULL);
}

/* userhost_returned: this is called when numeric 302 is received in
 * numbers.c. USERHOST must also remain the property of the WHOIS
 * queue. Sending it without going via the WHOIS queue will cause
 * the queue to become corrupted.
 *
 * While USERHOST gives us similar information to WHOIS, this routine's
 * format is a little different to those that handle the WHOIS numerics.
 * A list of nicks can be supplied to USERHOST, and any of those
 * nicks are not currently signed on, it will just omit reporting it.
 * this means we must go through the list of nicks returned and check
 * them for missing entries. This means stepping through the requested
 * nicks one at a time.
 *
 * A side effect of this is that user initiated USERHOST requests get
 * reported as separate replies even though one request gets made and
 * only one reply is actually received. This should make it easier for
 * people wanting to use USERHOST for their own purposes.
 *
 * In this routine, cnick points to all the information in the next
 * entry from the returned data.
 */
void
userhost_returned(u_char *from, u_char **ArgList)
{
	WhoisQueue *thing;
	WhoisStuff *whois_stuff = NULL;
	u_char	*nick,
		*cnick = NULL,
		*tnick;
	u_char	*user,
		*cuser = NULL;
	u_char	*host,
		*chost = NULL;
	int	ishere,
		cishere = 1;
	int	isoper,
		cisoper = 0;
	int	noton,
		isuser,
		parsed;
	u_char	*queue_nicks;

	if (!ArgList[0])
		return;
	if (whois_type_head(parsing_server()) == WHOIS_USERHOST)
	{
		isuser = (whois_func_head(parsing_server()) == USERHOST_USERHOST);
		whois_stuff = server_get_whois_stuff(parsing_server());
		thing = remove_from_whois_queue(parsing_server());
		queue_nicks = thing->nick;
	}
	else
	{
		isuser = 1;
		thing = NULL;
		queue_nicks = NULL;
		whois_stuff = NULL;
	}
	parsed = 0;
	while ((parsed || (cnick = next_arg(ArgList[0], ArgList))) ||
			queue_nicks)
	{
		if (queue_nicks)
		{
			tnick = next_arg(queue_nicks, &queue_nicks);
			if (!*queue_nicks)
				queue_nicks = NULL;
		}
		else
			tnick = NULL;
		if (cnick && !parsed)
		{
			if (!(cuser = my_index(cnick,'=')))
				break;
			if (*(cuser - 1) == '*')
			{
				*(cuser - 1) = '\0';
				cisoper = 1;
			}
			else
				cisoper = 0;
			*cuser++ = '\0';
			if (*cuser++ == '+')
				cishere = 1;
			else
				cishere = 0;
			if (!(chost = my_index(cuser, '@')))
				break;
			*chost++ = '\0';
			parsed = 1;
		}
		if (!cnick || (tnick && my_stricmp(cnick, tnick)))
		{
			if (tnick)
				nick = tnick;
			else
				nick = cnick;
			user = host = UP("<UNKNOWN>");
			isoper = 0;
			ishere = 1;
			noton = 1;
		}
		else
		{
			nick = cnick;
			user = cuser;
			host = chost;
			isoper = cisoper;
			ishere = cishere;
			noton = parsed = 0;
		}
		if (!isuser)
		{
			malloc_strcpy(&whois_stuff->nick, nick);
			malloc_strcpy(&whois_stuff->user, user);
			malloc_strcpy(&whois_stuff->host, host);
			whois_stuff->oper = isoper;
			whois_stuff->not_on = noton;
			if (!ishere)
				malloc_strcpy(&whois_stuff->away, empty_string());
			else
				new_free(&whois_stuff->away);
			thing->func(whois_stuff, tnick, thing->text);
			new_free(&whois_stuff->away);
		}
		else
		{
			if (do_hook(current_numeric(), "%s %s %s %s %s", nick,
			    isoper ? "+" : "-", ishere ? "+" : "-", user, host))
				put_it("%s %s is %s@%s%s%s", numeric_banner(),
					nick, user, host, isoper ?
					(u_char *) " (Is an IRC operator)" : empty_string(),
					ishere ? empty_string() : (u_char *) " (away)");
		}
	}
	if (thing)
	{
		new_free(&thing->nick);
		new_free(&thing->text);
		new_free(&thing);
	}
}

/*
 * whois_name: routine that is called when numeric 311 is received in
 * numbers.c. This routine parses out the information in the numeric and
 * saves it until needed (see whois_server()).  If the whois queue is empty,
 * this routine simply displays the data in the normal fashion.  Why would
 * the queue ever be empty, you ask? If the user does a "WHOIS *" or any
 * other whois with a wildcard, you will get multiple returns from the
 * server.  So, instead of attempting to handle each of these, only the first
 * is handled, and the others fall through.  It is up to the programmer to
 * prevent wildcards from interfering with what they want done.  See
 * channel() in edit.c 
 */
void
whois_name(u_char *from, u_char **ArgList)
{
	u_char	*nick,
		*user,
		*host,
		*channel,
		*ptr,
		*name;
	WhoisStuff *whois_stuff;

	PasteArgs(ArgList, 4);
	nick = ArgList[0];
	user = ArgList[1];
	host = ArgList[2];
	channel = ArgList[3];
	name = ArgList[4];
	if (!nick || !user || !host || !channel || !name)
		return;
	whois_stuff = server_get_whois_stuff(parsing_server());
	if ((ptr = whois_queue_head(parsing_server())) &&
	    (whois_type_head(parsing_server()) &
	     (WHOIS_WHOIS|WHOIS_ISON2)) &&
	    my_stricmp(ptr, nick) == 0)
	{
		malloc_strcpy(&whois_stuff->nick, nick);
		malloc_strcpy(&whois_stuff->user, user);
		malloc_strcpy(&whois_stuff->host, host);
		malloc_strcpy(&whois_stuff->name, name);
		malloc_strcpy(&whois_stuff->channel, channel);
		new_free(&whois_stuff->away);
		whois_stuff->oper = 0;
		whois_stuff->chop = 0;
		whois_stuff->not_on = 0;
		ignore_whois_crap = 1;
		eat_away = 1;
	}
	else
	{
		ignore_whois_crap = 0;
		eat_away = 0;
		if (do_hook(current_numeric(), "%s %s %s %s %s %s", from, nick,
				user, host, channel, name))
			put_it("%s %s is %s@%s (%s)", numeric_banner(), nick,
					user, host, name);
	}
}

/*
 * whowas_name: same as whois_name() above but it is called with a numeric of
 * 314 when the user does a WHOWAS or when a WHOIS'd user is no longer on IRC 
 * and has set the AUTO_WHOWAS variable.
 */
void
whowas_name(u_char *from, u_char **ArgList)
{
	u_char	*nick,
		*user,
		*host,
		*channel,
		*ptr,
		*name;
	WhoisStuff *whois_stuff;
	int	lastlog_level;

	PasteArgs(ArgList, 4);
	nick = ArgList[0];
	user = ArgList[1];
	host = ArgList[2];
	channel = ArgList[3];
	name = ArgList[4];
	if (!nick || !user || !host || !channel || !name)
		return;

	lastlog_level = set_lastlog_msg_level(LOG_CRAP);
	whois_stuff = server_get_whois_stuff(parsing_server());
	if ((ptr = whois_queue_head(parsing_server())) &&
	    (whois_type_head(parsing_server()) &
	     (WHOIS_WHOIS|WHOIS_ISON2)) &&
	    my_stricmp(ptr, nick) == 0)
	{
		malloc_strcpy(&whois_stuff->nick, nick);
		malloc_strcpy(&whois_stuff->user, user);
		malloc_strcpy(&whois_stuff->host, host);
		malloc_strcpy(&whois_stuff->name, name);
		malloc_strcpy(&whois_stuff->channel, channel);
		new_free(&whois_stuff->away);
		whois_stuff->oper = 0;
		whois_stuff->chop = 0;
		whois_stuff->not_on = 1;
		ignore_whois_crap = 1;
	}
	else
	{
		ignore_whois_crap = 0;
		if (do_hook(current_numeric(), "%s %s %s %s %s %s", from, nick,
				user, host, channel, name))
			put_it("%s %s was %s@%s (%s) on channel %s",
				numeric_banner(), nick, user, host, name,
				(*channel == '*') ? (u_char *) "*private*" : channel);
	}
	set_lastlog_msg_level(lastlog_level);
}

void
whois_channels(u_char *from, u_char **ArgList)
{
	u_char	*ptr;
	u_char	*line;
	WhoisStuff *whois_stuff;

	PasteArgs(ArgList, 1);
	line = ArgList[1];
	whois_stuff = server_get_whois_stuff(parsing_server());
	if ((ptr = whois_queue_head(parsing_server())) &&
	    (whois_type_head(parsing_server()) &
	     (WHOIS_WHOIS|WHOIS_ISON2)) &&
	    whois_stuff->nick && my_stricmp(ptr, whois_stuff->nick) == 0)
	{
		if (whois_stuff->channels == NULL)
			malloc_strcpy(&whois_stuff->channels, line);
		else
			malloc_strcat(&whois_stuff->channels, line);
	}
	else
	{
		if (do_hook(current_numeric(), "%s %s", from, line))
			put_it("%s on channels: %s", numeric_banner(), line);
	}
}

/*
 * whois_server: Called in numbers.c when a numeric of 312 is received.  If
 * all went well, this routine collects the needed information, pops the top
 * element off the queue and calls the function as described above in
 * WhoisQueue.  It then releases all the mallocd data.  If the queue is empty
 * (same case as described in whois_name() above), the information is simply
 * displayed in the normal fashion. Added a check to see if whois_stuff->nick
 * is NULL. This can happen if something is added to an empty whois queue
 * between the whois name being received and the server.
 */
void
whois_server(u_char *from, u_char **ArgList)
{
	u_char	*server,
		*ptr;
	u_char	*line;
	WhoisStuff *whois_stuff;

	show_away_flag = 1;
	if (!ArgList[0] || !ArgList[1])
		return;
	if (ArgList[2])
	{
		server = ArgList[1];
		line = ArgList[2];
	}
	else
	{
		server = ArgList[0];
		line = ArgList[1];
	}
	whois_stuff = server_get_whois_stuff(parsing_server());
	if ((ptr = whois_queue_head(parsing_server())) &&
	    (whois_type_head(parsing_server()) &
	     (WHOIS_WHOIS|WHOIS_ISON2)) &&
	    whois_stuff->nick && /* This is *weird* */
	    my_stricmp(ptr, whois_stuff->nick) == 0)
	{
		malloc_strcpy(&whois_stuff->server, server);
		malloc_strcpy(&whois_stuff->server_stuff, line);
	}
	else
	{
		if (do_hook(current_numeric(), "%s %s %s", from, server, line))
			put_it("%s on irc via server %s (%s)",
				numeric_banner(), server, line);
	}
}

/*
 * whois_oper: This displays the operator status of a user, as returned by
 * numeric 313 from the server.  If the ignore_whois_crap flag is set,
 * nothing is dispayed. 
 */
void
whois_oper(u_char *from, u_char **ArgList)
{
	WhoisStuff *whois_stuff;

	whois_stuff = server_get_whois_stuff(parsing_server());
	PasteArgs(ArgList, 1);
	if (ignore_whois_crap)
		whois_stuff->oper = 1;
	else
	{
		u_char	*nick;

		if ((nick = ArgList[0]) != NULL)
		{
			if (do_hook(current_numeric(), "%s %s %s", from, nick,
					ArgList[1]))
				put_it("%s %s %s%s", numeric_banner(), nick,
					ArgList[1], (server_get_version(parsing_server()) >
					Server2_7) ? empty_string()
						   : (u_char *) " (is an IRC operator)");
		}
	}
}

void
whois_lastcom(u_char *from, u_char **ArgList)
{
	if (!ignore_whois_crap)
	{
		u_char	flag, *nick, *idle_str;
		int	idle;

		PasteArgs(ArgList, 2);
		if ((nick = ArgList[0]) && (idle_str = ArgList[1]) &&
				do_hook(current_numeric(), "%s %s %s %s", from,
					nick, idle_str, ArgList[2]))
		{
			if ((idle = my_atoi(idle_str)) > 59)
			{
				idle = idle/60;
				flag = 1;
			}
			else
				flag = 0;
			put_it("%s %s has been idle %d %ss", numeric_banner(),
				nick, idle, flag? "minute": "second");
		}
	}
}

/*
 * whois_chop: This displays the operator status of a user, as returned by
 * numeric 313 from the server.  If the ignore_whois_crap flag is set,
 * nothing is dispayed. 
 */
void
whois_chop(u_char *from, u_char **ArgList)
{
	WhoisStuff *whois_stuff;

	whois_stuff = server_get_whois_stuff(parsing_server());
	PasteArgs(ArgList, 1);
	if (ignore_whois_crap)
		whois_stuff->chop = 1;
	else
	{
		u_char	*nick;

		if ((nick = ArgList[0]) != NULL)
		{
			u_char *arg1 = ArgList[1] ? ArgList[1] : empty_string();

			if (do_hook(current_numeric(), "%s %s %s", from,
				    nick, arg1))
				put_it("%s %s (is a channel operator): %s",
					numeric_banner(), nick, arg1);
		}
	}
}

void
end_of_whois(u_char *from, u_char **ArgList)
{
	u_char	*nick;
	u_char	*ptr;
	WhoisStuff *whois_stuff;

	whois_stuff = server_get_whois_stuff(parsing_server());

	show_away_flag = 0;
	server_set_whois(parsing_server(), 1);
	if ((nick = ArgList[0]) != NULL)
	{
		ptr = whois_queue_head(parsing_server());
		if (ptr &&
		    (whois_type_head(parsing_server()) &
		     (WHOIS_WHOIS|WHOIS_ISON2)) &&
		    my_stricmp(ptr, nick) == 0)
		{
			WhoisQueue *thing;

			thing = remove_from_whois_queue(parsing_server());
			whois_stuff->not_on = 0;
			thing->func(whois_stuff, thing->nick, thing->text);
			new_free(&whois_stuff->channels);
			new_free(&thing->nick);
			new_free(&thing->text);
			new_free(&thing);
			ignore_whois_crap = 0;
			return;
		}
		PasteArgs(ArgList, 0);
		if (do_hook(current_numeric(), "%s %s", from, ArgList[0]))
			if (get_int_var(SHOW_END_OF_MSGS_VAR))
				display_msg(from, ArgList);
	}
}

/*
 * no_such_nickname: Handler for numeric 401, the no such nickname error. If
 * the nickname given is at the head of the queue, then this routine pops the
 * top element from the queue, sets the whois_stuff->flag to indicate that the
 * user is no longer on irc, then calls the func() as normal.  It is up to
 * that function to set the ignore_whois_crap variable which will determine
 * if any other information is displayed or not. 
 *
 * that is, it used to.  now it does bugger all, seeing all the functions that
 * used to use it, now use no such command.  -phone, april 1993.
 */
void
no_such_nickname(u_char *from, u_char **ArgList)
{
	u_char	*nick;
	u_char		*ptr;
	WhoisStuff	*whois_stuff;

	whois_stuff = server_get_whois_stuff(parsing_server());
	ptr = whois_queue_head(parsing_server());
	PasteArgs(ArgList, 1);
	nick = ArgList[0];
	if (*nick == '!')
	{
		u_char	*name = nick+1;

		if (ptr &&
		    (whois_type_head(parsing_server()) &
		     (WHOIS_WHOIS|WHOIS_ISON2)) &&
		    my_strcmp(ptr, name) == 0)
		{
			WhoisQueue *thing;

			/* There's a query in the WhoisQueue : assume it's
			   completed and remove it from the queue -Sol */

			thing = remove_from_whois_queue(parsing_server());
			whois_stuff->not_on = 0;
			thing->func(whois_stuff, thing->nick, thing->text);
			new_free(&whois_stuff->channels);
			new_free(&thing->nick);
			new_free(&thing->text);
			new_free(&thing);
			ignore_whois_crap = 0;
			return;
		}
		return;
	}
	notify_mark(nick, 0, 0);
	if (ptr && (whois_type_head(parsing_server()) == WHOIS_ISON2) &&
	    !my_strcmp(ptr, nick))
	{
		WhoisQueue *thing;

		/* Remove query from queue. Don't display anything. -Sol */

		thing = remove_from_whois_queue(parsing_server());
		new_free(&whois_stuff->channels);
		new_free(&thing->nick);
		new_free(&thing->text);
		new_free(&thing);
		ignore_whois_crap = 0;
		return;
	}
	if (do_hook(current_numeric(), "%s %s %s", from, nick, ArgList[1]))
		put_it("%s %s: %s", numeric_banner(), nick, ArgList[1]);
}

/*
 * user_is_away: called when a 301 numeric is received.  Nothing is displayed
 * by this routine if the ignore_whois_crap flag is set 
 */
void
user_is_away(u_char *from, u_char **ArgList)
{
	static	u_char	*last_away_msg = NULL,
			*last_away_nick = NULL;
	u_char	*message,
		*who;
	WhoisStuff *whois_stuff;

	if (!from)
		return;

	PasteArgs(ArgList, 1);
	whois_stuff = server_get_whois_stuff(parsing_server());

	if ((who = ArgList[0]) && (message = ArgList[1]))
	{
		if (whois_stuff->nick && (!my_strcmp(who, whois_stuff->nick)) &&
				eat_away)
			malloc_strcpy(&whois_stuff->away, message);
		else
		{
			if (!show_away_flag && get_int_var(SHOW_AWAY_ONCE_VAR))
			{
				if (!last_away_msg ||
				    my_strcmp(last_away_nick, from) ||
				    my_strcmp(last_away_msg, message))
				{
					malloc_strcpy(&last_away_nick, from);
					malloc_strcpy(&last_away_msg, message);
				}
				else return;
			}
			if (do_hook(current_numeric(), "%s %s", who, message))
				put_it("%s %s is away: %s",numeric_banner(),
					who, message);
		}
		eat_away = 0;
	}
}

/*
 * The stuff below this point are all routines suitable for use in the
 * add_to_whois_queue() call as the func parameter 
 */

/*
 * whois_ignore_msgs: This is used when you are ignoring MSGs using the
 * user@hostname format 
 */
void
whois_ignore_msgs(WhoisStuff *stuff, u_char *nick, u_char *text)
{
	u_char	*ptr;
	int	level;

	if (stuff)
	{
		ptr = new_malloc(my_strlen(stuff->user) + my_strlen(stuff->host) + 2);
		my_strcpy(ptr, stuff->user);
		my_strcat(ptr, "@");
		my_strcat(ptr, stuff->host);
		if (is_ignored(ptr, IGNORE_MSGS) != IGNORED)
		{
			save_message_from();
			level = set_lastlog_msg_level(LOG_MSG);
			message_from(stuff->nick, LOG_MSG);
			if (ctcp_was_crypted() == 1 && !do_hook(ENCRYPTED_PRIVMSG_LIST,"%s %s", stuff->nick,text))
			{
				set_lastlog_msg_level(level);
				restore_message_from();
				return;
			}
			if (do_hook(MSG_LIST, "%s %s", stuff->nick, text))
			{
				if (is_away_set())
				{
					time_t	t;
					u_char	*msg = NULL;
					size_t len = my_strlen(text) + 20;

					t = time(0);
					msg = new_malloc(len);
					snprintf(CP(msg), len, "%s <%.16s>", text,
						ctime(&t));
					put_it("*%s* %s", stuff->nick, msg);
					new_free(&msg);
					beep_em(get_int_var(BEEP_WHEN_AWAY_VAR));
				}
				else
				{
					put_it("*%s* %s", stuff->nick, text);
					beep_em(get_int_var(BEEP_ON_MSG_VAR));
				}
			}
			if (beep_on_level & LOG_MSG)
				beep_em(1);
			set_lastlog_msg_level(level);
			message_from(NULL, LOG_CURRENT);
			notify_mark(nick, 1, 0);
		}
		else
			send_to_server("NOTICE %s :%s is ignoring you.",
				nick, server_get_nickname(parsing_server()));
		new_free(&ptr);
	}
	restore_message_from();
}

void
whois_nickname(WhoisStuff *stuff, u_char *nick, u_char *text)
{
	if (stuff &&
	    my_stricmp(stuff->user, my_username()) == 0 &&
	    my_stricmp(stuff->host, my_hostname()) == 0)
		server_set_nickname(parsing_server(), nick);
}

/*
 * whois_ignore_notices: This is used when you are ignoring NOTICEs using the
 * user@hostname format 
 */
void
whois_ignore_notices(WhoisStuff *stuff, u_char *nick, u_char *text)
{
	u_char	*ptr;
	int	level;

	if (stuff)
	{
		ptr = new_malloc(my_strlen(stuff->user) + my_strlen(stuff->host) + 2);
		my_strcpy(ptr, stuff->user);
		my_strcat(ptr, "@");
		my_strcat(ptr, stuff->host);
		if (is_ignored(ptr, IGNORE_NOTICES) != IGNORED)
		{
			u_char *to;

			to = next_arg(text, &text);
			level = set_lastlog_msg_level(LOG_NOTICE);
			save_message_from();
			message_from(stuff->nick, LOG_NOTICE);
			if (ctcp_was_crypted() == 0 && !do_hook(ENCRYPTED_NOTICE_LIST,"%s %s %s", stuff->nick, to, text))
			{
				restore_message_from();
				return;
			}
			if (do_hook(NOTICE_LIST, "%s %s %s", stuff->nick, to, text))
				put_it("-%s- %s", stuff->nick, text);
			set_lastlog_msg_level(level);
			restore_message_from();
		}
		new_free(&ptr);
	}
}

/*
 * whois_ignore_invites: This is used when you are ignoring INVITES using the
 * user@hostname format 
 */
void
whois_ignore_invites(WhoisStuff *stuff, u_char *nick, u_char *text)
{
	u_char	*ptr;

	if (stuff)
	{
		ptr = new_malloc(my_strlen(stuff->user) + my_strlen(stuff->host) + 2);
		my_strcpy(ptr, stuff->user);
		my_strcat(ptr, "@");
		my_strcat(ptr, stuff->host);
		if (is_ignored(ptr, IGNORE_INVITES) != IGNORED)
		{
			if (do_hook(INVITE_LIST, "%s %s", stuff->nick, text))
				say("%s invites you to channel %s",
					stuff->nick, text);
			set_invite_channel(text);
		}
		else
			send_to_server("NOTICE %s :%s is ignoring you.",
				nick, server_get_nickname(parsing_server()));
		new_free(&ptr);
	}
}

/*
 * whois_ignore_walls: This is used when you are ignoring WALLS using the
 * user@hostname format 
 */
void
whois_ignore_walls(WhoisStuff *stuff, u_char *nick, u_char *text)
{
	u_char	*ptr;
	int	level;

	level = set_lastlog_msg_level(LOG_WALL);
	save_message_from();
	message_from(stuff->nick, LOG_WALL);
	if (stuff)
	{
		ptr = new_malloc(my_strlen(stuff->user) + my_strlen(stuff->host) + 2);
		my_strcpy(ptr, stuff->user);
		my_strcat(ptr, "@");
		my_strcat(ptr, stuff->host);
		if (is_ignored(ptr, IGNORE_WALLS) != IGNORED)
		{
			if (do_hook(WALL_LIST, "%s %s", stuff->nick, text))
				put_it("#%s# %s", stuff->nick, text);
			if (beep_on_level & LOG_WALL)
				beep_em(1);
		}
		new_free(&ptr);
	}
	set_lastlog_msg_level(level);
	save_message_from();
}

void
convert_to_whois(void)
{
	u_char	*NextAsked;
	u_char	*Names;
	WhoisQueue *thing;
	void	(*func)(WhoisStuff *, u_char *, u_char *);

	if (!(whois_type_head(parsing_server()) & (WHOIS_USERHOST|WHOIS_WHOIS|WHOIS_ISON2)))
	{
		say("Server does not support USERHOST");
		return; /* USERHOST sent interactively. */
	}
	thing = remove_from_whois_queue(parsing_server());
	switch(thing->type)
	{
	case WHOIS_ISON:
		Names = thing->nick;
		while ( (NextAsked = next_arg(Names, &Names)))
		{
			if (thing->func == ison_notify)
			{
				func = (void (*)(WhoisStuff *, u_char *, u_char *)) whois_notify;
				add_to_whois_queue(NextAsked, func, "%s", NextAsked);
			}
			else
				say("Server does not support ISON");
		}
		break;
	case WHOIS_USERHOST:
		add_to_whois_queue(thing->nick, thing->func, "%s", thing->text);
		break;
	}
	new_free(&thing->nick);
	new_free(&thing->text);
	new_free(&thing);
}

void
ison_notify(WhoisStuff *unused, u_char *AskedFor, u_char *AreOn)
{
	u_char	*NextAsked;
	u_char	*NextGot;

	NextGot = next_arg(AreOn, &AreOn);
	while ((NextAsked = next_arg(AskedFor, &AskedFor)) != NULL)
	{
		if (NextGot && !my_stricmp(NextAsked, NextGot))
		{
			notify_mark(NextAsked, 1, 1);
			NextGot = next_arg(AreOn, &AreOn);
		}
		else
			notify_mark(NextAsked, 0, 1);
	}
}

/*
 * whois_notify: used by the routines in notify.c to tell when someone has
 * signed on or off irc 
 */
void
whois_notify(WhoisStuff *stuff, u_char *nick, u_char *text)
{
	int	level;

	level = set_lastlog_msg_level(LOG_CRAP);
	if (stuff)
		notify_mark(stuff->nick, 1, 1);
	else
		notify_mark(nick, 0, 1);
	set_lastlog_msg_level(level);
}

void
whois_new_wallops(WhoisStuff *stuff, u_char *nick, u_char *text)
{
	int	flag,
	level;
	u_char	*high;

	flag = is_ignored(nick, IGNORE_WALLOPS);
	if (flag != IGNORED)
	{
		if (flag == HIGHLIGHTED)
			high = highlight_str();
		else
			high = empty_string();
		if (stuff && (ignore_usernames() & IGNORE_WALLOPS))
		{
			u_char	*ptr;

			ptr = new_malloc(my_strlen(stuff->user) + my_strlen(stuff->host) + 2);
			my_strcpy(ptr, stuff->user);
			my_strcat(ptr, "@");
			my_strcat(ptr, stuff->host);
			if (is_ignored(ptr, IGNORE_WALLOPS) == IGNORED)
			{
				new_free(&ptr);
				return;
			}
			new_free(&ptr);
		}
		save_message_from();
		message_from(nick, LOG_WALLOP);
		level = set_lastlog_msg_level(LOG_WALLOP);
		if (stuff)
		{
			if (do_hook(WALLOP_LIST, "%s %s %s", nick,
					(stuff->oper ? "+" : "-"), text))
				put_it("%s!%s%s!%s %s", high, nick,
					stuff->oper ? (u_char *) "*" : empty_string(),
					high, text);
		}
		else
		{
			if (do_hook(WALLOP_LIST, "%s - %s", nick, text))
				put_it("%s!%s!%s %s", high, nick, high, text);
		}
		if (beep_on_level & LOG_WALLOP)
			beep_em(1);
		set_lastlog_msg_level(level);
		restore_message_from();
	}
}

/* I put the next routine down here to keep my compile quiet */

/*
 * add_to_whois_queue: This routine is called whenever you want to do a WHOIS
 * or WHOWAS.  What happens is this... each time this function is called it
 * adds a new element to the whois queue using the nick and func as in
 * WhoisQueue, and creating the text element using the format and args.  It
 * then issues the WHOIS or WHOWAS.  
 */
void
add_to_whois_queue(u_char *nick, void (*func)(WhoisStuff *, u_char *, u_char *), char *format, ...)
{
	int	Type;
	va_list	vlist;

	va_start(vlist, format);

	if (func == USERHOST_USERHOST || func == userhost_cmd_returned)
		Type = WHOIS_USERHOST;
	else if (func == whois_notify)
		Type = WHOIS_ISON2;
	else
		Type = WHOIS_WHOIS;
	typed_add_to_whois_queue(Type, nick, func, format, vlist);
	va_end(vlist);
}

/*
 * note that typed_add_to_whois_queue() ignores the final (fifth and
 * beyond) parameter if the 4th is NULL.  we use the va_list variable
 * here instead of 0 to work around picky compilers.
 */
void
add_ison_to_whois(u_char *nick, void (*func)(WhoisStuff *, u_char *, u_char *))
{
	va_list vlist;

#ifdef __GNUC__	/* grr */
	memset(&vlist, 0, sizeof vlist);
#endif
	typed_add_to_whois_queue(WHOIS_ISON, nick, func, NULL, vlist);
}

static void
typed_add_to_whois_queue(int type, u_char *nick, void (*func)(WhoisStuff *, u_char *, u_char *), char *format, va_list vlist)
{
	u_char	lbuf[BIG_BUFFER_SIZE];
	WhoisQueue *new;
	u_char	*p = nick;
	const	int	from_server = get_from_server();

	if (server_get_version(from_server) == ServerICB)
	{
#if 0
		yell("--- typed_add_to_whois_queue: panic?  attempted on ICB server..");
#endif
		return;
	}
	if (nick == NULL ||
	    !is_server_connected(from_server) ||
	    !is_server_open(from_server))
		return;

	for (; *p == ' ' || *p == '\t'; p++);
	if (!*p)
		return;	/* nick should always be a non-blank string, but
			   I'd rather check because of a "ISON not enough
			   parameters" coming from the server -Sol */

	if (my_index(nick, '*') == NULL)
	{
		WhoisQueue *tail;

		new = new_malloc(sizeof *new);
		new->text = NULL;
		new->nick = NULL;
		new->func = func;
		new->next = NULL;
		new->type = type;
		if (format)
		{
			vsnprintf(CP(lbuf), sizeof lbuf, format, vlist);
			malloc_strcpy(&new->text, lbuf);
		}
		malloc_strcpy(&new->nick, nick);
		if (server_get_qhead(from_server) == NULL)
			server_set_qhead(from_server, new);
		tail = server_get_qtail(from_server);
		if (tail)
			tail->next = new;
		server_set_qtail(from_server, new);
		switch(type)
		{
		case WHOIS_ISON:
#ifdef MONITOR_Q
			put_it("+++ ISON %s", nick);
#endif /* MONITOR_Q */
			send_to_server("ISON %s", nick);
			break;
		case WHOIS_USERHOST:
#ifdef MONITOR_Q
			put_it("+++ USERHOST %s", nick);
#endif /* MONITOR_Q */
			send_to_server("USERHOST %s", nick);
			break;
		case WHOIS_WHOIS:
		case WHOIS_ISON2:
#ifdef MONITOR_Q
			put_it("+++ WHOIS %s", nick);
#endif /* MONITOR_Q */
			send_to_server("WHOIS %s", nick);
			if (!server_get_whois(from_server))
				send_to_server("WHOIS !%s", nick);
				/* postfix with nick so we know who we're
				   talking about -Sol */
				/* send "WHOIS !nick" and expect
				   "!nick: No such nick/channel" :
				   it means the real query was completed
				   and the dummy query is to be ignored
				   in no_such_nickname() -Sol */
			break;
		}
	}
}

void
userhost_cmd_returned(WhoisStuff *stuff, u_char *nick, u_char *text)
{
	u_char	args[BIG_BUFFER_SIZE];

	my_strmcpy(args, stuff->nick ? stuff->nick : empty_string(), sizeof args);
	my_strmcat(args, stuff->oper ? " + " : " - ", sizeof args);
	my_strmcat(args, stuff->away ? "+ " : "- ", sizeof args);
	my_strmcat(args, stuff->user ? stuff->user : empty_string(), sizeof args);
	my_strmcat(args, " ", sizeof args);
	my_strmcat(args, stuff->host ? stuff->host : empty_string(), sizeof args);
	parse_line(NULL, text, args, 0, 0, 1);
}

int
do_beep_on_level(int val)
{
	return (beep_on_level & val) != 0;
}
