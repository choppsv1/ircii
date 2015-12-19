/*
 * notify.c: a few handy routines to notify you when people enter and leave irc 
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
 * Revamped by lynX - Dec '91
 */

#include "irc.h"
IRCII_RCSID("@(#)$eterna: notify.c,v 1.53 2014/03/12 06:17:05 mrg Exp $");

#include "list.h"
#include "notify.h"
#include "ircaux.h"
#include "whois.h"
#include "hook.h"
#include "server.h"
#include "output.h"
#include "vars.h"

/* NotifyList: the structure for the notify stuff */
typedef	struct	notify_stru
{
	struct	notify_stru	*next;	/* pointer to next notify person */
	u_char	*nick;			/* nickname of person to notify about */
	int	flag;			/* 1=person on irc, 0=person not on irc */
}	NotifyList;

static	NotifyList	*notify_list = NULL;
static	u_char		*last_notify_nick;	/* last detected nickname */

u_char *
get_notify_list(int which)
{
	u_char	*list = NULL;
	NotifyList	*tmp;
	int first = 0;

	malloc_strcpy(&list, empty_string());
	for (tmp = notify_list; tmp; tmp = tmp->next)
	{
		if ((which & NOTIFY_LIST_ALL) == NOTIFY_LIST_ALL ||
		   ((which & NOTIFY_LIST_HERE) && tmp->flag) ||
		   ((which & NOTIFY_LIST_GONE) && !tmp->flag))
		{
			if (first++)
				malloc_strcat(&list, UP(" "));
			malloc_strcat(&list, tmp->nick);
		}
	}
	return list;
}

/* Rewritten, -lynx */
void
show_notify_list(int all)
{
	u_char	*list;

	list = get_notify_list(NOTIFY_LIST_HERE);
	if (*list)
		say("Currently present: %s", list);
	if (all)
	{
		new_free(&list);
		list = get_notify_list(NOTIFY_LIST_GONE);
		if (*list)
			say("Currently absent: %s", list);
	}
	new_free(&list);
}

/* notify: the NOTIFY command.  Does the whole ball-o-wax */
void
notify(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*nick,
		*list = NULL,
		*ptr;
	int	no_nicks = 1;
	int	do_ison = 0;
	int	old_server;
	NotifyList	*new;

	malloc_strcpy(&list, empty_string());
	while ((nick = next_arg(args, &args)) != NULL)
	{
		no_nicks = 0;
		while (nick)
		{
			if ((ptr = my_index(nick, ',')) != NULL)
				*ptr++ = '\0';
			if (*nick == '-')
			{
				nick++;
				if (*nick)
				{
					if ((new = (NotifyList *) remove_from_list((List **)(void *)&notify_list, nick)) != NULL)
					{
						new_free(&(new->nick));
						new_free(&new);
						say("%s removed from notification list", nick);
					}
					else
						say("%s is not on the notification list", nick);
				}
				else
				{
					while ((new = notify_list))
					{
						notify_list = new->next;
						new_free(&new->nick);
						new_free(&new);
					}
					say("Notify list cleared");
				}
			}
			else
			{
				/* compatibility */
				if (*nick == '+')
					nick++;
				if (*nick)
				{
					do_ison = 1;
					if (my_index(nick, '*'))
						say("Wildcards not allowed in NOTIFY nicknames!");
					else
					{

						if ((new = (NotifyList *) remove_from_list((List **)(void *)&notify_list, nick)) != NULL)
						{
							new_free(&(new->nick));
							new_free(&new);
						}
						new = new_malloc(sizeof *new);
						new->nick = NULL;
						malloc_strcpy(&(new->nick), nick);
						new->flag = 0;
						add_to_list((List **)(void *)&notify_list, (List *) new);
						old_server = set_from_server(get_primary_server());
						if (server_get_2_6_2(get_from_server()))
						{
							malloc_strcat(&list, new->nick);
							malloc_strcat(&list, UP(" "));
						}
						else
							add_to_whois_queue(new->nick, whois_notify, NULL);
						say("%s added to the notification list", nick);
						set_from_server(old_server);
					}
				} else
					show_notify_list(0);
			}
			nick = ptr;
		}
	}
	if (do_ison) {
		old_server = set_from_server(get_primary_server());
		add_ison_to_whois(list, ison_notify);
		set_from_server(old_server);
	}
	new_free(&list);
	if (no_nicks)
		show_notify_list(1);
}

/*
 * do_notify: This simply goes through the notify list, sending out a WHOIS
 * for each person on it.  This uses the fancy whois stuff in whois.c to
 * figure things out.  Look there for more details, if you can figure it out.
 * I wrote it and I can't figure it out.
 *
 * Thank you Michael... leaving me bugs to fix :) Well I fixed them!
 */
void
do_notify(void)
{
	static	int	location = 0;
	int	count,
		c2,
		old_server;
	u_char	buf[BIG_BUFFER_SIZE];
	NotifyList	*tmp;

	/* ICB doesn't support /notify */
	if (server_get_version(get_primary_server()) == ServerICB)
		return;
	old_server = set_from_server(get_primary_server());
	*buf = '\0';
	for (tmp = notify_list, c2 = count = 0; tmp; tmp = tmp->next, count++)
	{
		if (count >= location && count < location + 40)
		{
			c2++;
			my_strmcat(buf, " ", sizeof buf);
			my_strmcat(buf, tmp->nick, sizeof buf);
		}
	}
	if (c2)
		add_ison_to_whois(buf, ison_notify);
	if ((location += 40) > count)
		location = 0;
	set_from_server(old_server);
}

/*
 * notify_mark: This marks a given person on the notify list as either on irc
 * (if flag is 1), or not on irc (if flag is 0).  If the person's status has
 * changed since the last check, a message is displayed to that effect.  If
 * the person is not on the notify list, this call is ignored 
 * doit if passed as 0 means it comes from a join, or a msg, etc, not from
 * an ison reply.  1 is the other..
 * we also bail out if the get_primary_server() != from_server and the from-server
 * is valid.
 *
 * NOTE:  this function should be called with no particular to_window or other
 * variables set that would affect what window it would be displayed in.
 * ideally, a message_from(NULL, LOG_CURRENT) should be the what is the
 * current window level.
 */
void
notify_mark(u_char *nick, int flag, int doit)
{
	NotifyList	*tmp;
	u_char	*s = get_string_var(NOTIFY_HANDLER_VAR);
	const	int	from_server = get_from_server();

	if (from_server != get_primary_server() && from_server != -1)
		return;
	/* if (!s || (!doit && 'O' == *s)) - this broke notify -gkm *//* old notify */
	if (!doit && 'O' == *s)		/* old notify */
		return;	
	if ('N' == *s)			/* noisy notify */
		doit = 1;
	if ((tmp = (NotifyList *) list_lookup((List **)(void *)&notify_list, nick,
			0, 0)) != NULL)
	{
		if (flag)
		{
			if (tmp->flag != 1)
			{
				if (tmp->flag != -1 && doit && do_hook(NOTIFY_SIGNON_LIST, "%s", nick))
					say("Signon by %s detected", nick);
				/*
				 * copy the correct case of the nick
				 * into our array  ;)
				 */
				malloc_strcpy(&(tmp->nick), nick);
				malloc_strcpy(&last_notify_nick, nick);
				tmp->flag = 1;
			}
		}
		else
		{
			if (tmp->flag == 1 && doit && do_hook(NOTIFY_SIGNOFF_LIST, "%s", nick))
				say("Signoff by %s detected", nick);
			tmp->flag = 0;
		}
	}
}

void
save_notify(FILE *fp)
{
	NotifyList	*tmp;

	if (notify_list)
	{
		fprintf(fp, "NOTIFY");
		for (tmp = notify_list; tmp; tmp = tmp->next)
			fprintf(fp, " %s", tmp->nick);
		fprintf(fp, "\n");
	}
}

/* I hate broken compilers -mrg */
static	char	*vals[] = { "NOISY", "QUIET", "OLD", NULL };

void
set_notify_handler(u_char *value)
{
	size_t	len;
	int	i;
	char	*s;

	if (!value)
		value = empty_string();
	for (i = 0, len = my_strlen(value); (s = vals[i]); i++)
		if (0 == my_strnicmp(value, UP(s), len))
			break;
	set_string_var(NOTIFY_HANDLER_VAR, UP(s));
	return;
}

u_char *
get_last_notify_nick(void)
{
	return last_notify_nick;
}
