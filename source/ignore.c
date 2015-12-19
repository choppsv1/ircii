/*
 * ignore.c: handles the ingore command for irc 
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
IRCII_RCSID("@(#)$eterna: ignore.c,v 1.45 2014/03/12 06:17:05 mrg Exp $");

#include "ignore.h"
#include "ircaux.h"
#include "list.h"
#include "vars.h"
#include "output.h"

#define NUMBER_OF_IGNORE_LEVELS 9

#define IGNORE_REMOVE 1
#define IGNORE_DONT 2
#define IGNORE_HIGH -1

/*
 * Ignore: the ignore list structure,  consists of the nickname, and the type
 * of ignorance which is to take place 
 */
typedef struct	IgnoreStru
{
	struct	IgnoreStru *next;
	u_char	*nick;
	int	type;
	int	dont;
	int	high;
}	Ignore;

static	int	remove_ignore(u_char *);
static	u_char	*ignore_list(u_char *, int);
static	int	ignore_usernames_mask(int, int);
static	void	ignore_nickname(u_char *, int, int);
static	void	ignore_info(Ignore *, u_char *, size_t, u_char *,
			    u_char *, u_char *);

static	int	do_ignore_usernames = 0;
static	u_char	highlight_chars[2] = { '\0', '\0' };
static	int	ignore_usernames_sums[NUMBER_OF_IGNORE_LEVELS] =
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/* ignored_nicks: pointer to the head of the ignore list */
static	Ignore *ignored_nicks = NULL;

static	int
ignore_usernames_mask(int mask, int thing)
{
	int	i;
	int	p;

	for (i = 0, p = 1; i < NUMBER_OF_IGNORE_LEVELS; i++, p *= 2)
		if (mask & p)
			ignore_usernames_sums[i] += thing;

	mask = 0;
	for (i = 0, p = 1; i < NUMBER_OF_IGNORE_LEVELS; i++, p *= 2)
		if (ignore_usernames_sums[i])
			mask += p;

	return (mask);
}

/*
 * ignore_nickname: adds nick to the ignore list, using type as the type of
 * ignorance to take place.  
 */
static	void
ignore_nickname(u_char *nick, int type, int flag)
{
	Ignore	*new;
	char	*msg;
	u_char	*ptr;
	u_char	buffer[BIG_BUFFER_SIZE];

	while (nick)
	{
		if ((ptr = my_index(nick, ',')) != NULL)
			*ptr = '\0';
		if (my_index(nick, '@'))
			do_ignore_usernames = ignore_usernames_mask(type, 1);
		if (*nick)
		{
			if (!(new = (Ignore *) list_lookup((List **)(void *)&ignored_nicks, nick, 0, 0)))
			{
				if (flag == IGNORE_REMOVE)
				{
					say("%s is not on the ignorance list",
							nick);
					if (ptr)
						*(ptr++) = ',';
					nick = ptr;
					continue;
				}
				else
				{
					if ((new = (Ignore *) remove_from_list((List **)(void *)&ignored_nicks, nick)) != NULL)
					{
						new_free(&(new->nick));
						new_free(&new);
					}
					new = new_malloc(sizeof *new);
					new->nick = NULL;
					new->type = 0;
					new->dont = 0;
					new->high = 0;
					malloc_strcpy(&(new->nick), nick);
					upper(new->nick);
					add_to_list((List **)(void *)&ignored_nicks, (List *) new);
				}
			}
			switch (flag)
			{
			case IGNORE_REMOVE:
				new->type &= (~type);
				new->high &= (~type);
				new->dont &= (~type);
				msg = "Not ignoring";
				break;
			case IGNORE_DONT:
				new->dont |= type;
				new->type &= (~type);
				new->high &= (~type);
				msg = "Never ignoring";
				break;
			case IGNORE_HIGH:
				new->high |= type;
				new->type &= (~type);
				new->dont &= (~type);
				msg = "Highlighting";
				break;
			default:
				new->type |= type;
				new->high &= (~type);
				new->dont &= (~type);
				msg = "Ignoring";
				break;
			}
			if (type == IGNORE_ALL)
			{
				switch (flag)
				{
				case IGNORE_REMOVE:
					say("%s removed from ignorance list", new->nick);
					remove_ignore(new->nick);
					break;
				case IGNORE_HIGH:
				    say("Highlighting ALL messages from %s",
					new->nick);
					break;
				case IGNORE_DONT:
				    say("Never ignoring messages from %s",
					new->nick);
					break;
				default:
				    say("Ignoring ALL messages from %s",
					new->nick);
					break;
				}
				return;
			}
			else if (type)
			{
				my_strmcpy(buffer, msg, sizeof buffer);
				if (type & IGNORE_MSGS)
					my_strmcat(buffer, " MSGS", sizeof buffer);
				if (type & IGNORE_PUBLIC)
					my_strmcat(buffer, " PUBLIC", sizeof buffer);
				if (type & IGNORE_WALLS)
					my_strmcat(buffer, " WALLS", sizeof buffer);
				if (type & IGNORE_WALLOPS)
					my_strmcat(buffer, " WALLOPS", sizeof buffer);
				if (type & IGNORE_INVITES)
					my_strmcat(buffer, " INVITES", sizeof buffer);
				if (type & IGNORE_NOTICES)
					my_strmcat(buffer, " NOTICES", sizeof buffer);
				if (type & IGNORE_NOTES)
					my_strmcat(buffer, " NOTES", sizeof buffer);
				if (type & IGNORE_CTCPS)
					my_strmcat(buffer, " CTCPS", sizeof buffer);
				if (type & IGNORE_CRAP)
					my_strmcat(buffer, " CRAP", sizeof buffer);
				say("%s from %s", buffer, new->nick);
			}
			if (new->type == 0 && new->high == 0 && new->dont == 0)
				remove_ignore(new->nick);
		}
		if (ptr)
			*(ptr++) = ',';
		nick = ptr;
	}
}

/*
 * remove_ignore: removes the given nick from the ignore list and returns 0.
 * If the nick wasn't in the ignore list to begin with, 1 is returned. 
 */
static	int
remove_ignore(u_char *nick)
{
	Ignore	*tmp;

	if ((tmp = (Ignore *) list_lookup((List **)(void *)&ignored_nicks, nick, 0, REMOVE_FROM_LIST)) != NULL)
	{
		if (my_index(nick, '@'))
			do_ignore_usernames = ignore_usernames_mask(tmp->type, -1);
		new_free(&(tmp->nick));
		new_free(&tmp);
		return (0);
	}
	return (1);
}

/*
 * is_ignored: checks to see if nick is being ignored (poor nick).  Checks
 * against type to see if ignorance is to take place.  If nick is marked as
 * IGNORE_ALL or ignorace types match, 1 is returned, otherwise 0 is
 * returned.  
 */
int
is_ignored(u_char *nick, int type)
{
	Ignore	*tmp;

	if (ignored_nicks)
	{
		if ((tmp = (Ignore *) list_lookup((List **)(void *)&ignored_nicks, nick, USE_WILDCARDS, 0)) != NULL)
		{
			if (tmp->dont & type)
				return (DONT_IGNORE);
			if (tmp->type & type)
				return (IGNORED);
			if (tmp->high & type)
				return (HIGHLIGHTED);
		}
	}
	return (0);
}

/*
 * ignore_info: given an ignore entry, return a string about it.
 * used by listing and by /save -ignore.
 */
static void
ignore_info(Ignore *entry, u_char *buffer, size_t buflen, u_char *pre, u_char *post, u_char *dont)
{
	char	s[BIG_BUFFER_SIZE];

	if (!pre)
		pre = empty_string();
	if (!post)
		post = empty_string();

	*buffer = (u_char) 0;
	if (entry->type == IGNORE_ALL)
		my_strmcat(buffer," ALL", buflen);
	else if (entry->high == IGNORE_ALL)
	{
		snprintf(s, sizeof s, " %sALL%s", pre, post);
		my_strmcat(buffer, s, buflen);
	}
	else if (entry->dont == IGNORE_ALL)
	{
		snprintf(s, sizeof s, " %sALL", dont);
		my_strmcat(buffer, s, buflen);
	}
	else
	{
		if (entry->type & IGNORE_PUBLIC)
			my_strmcat(buffer, " PUBLIC", buflen);
		else if (entry->high & IGNORE_PUBLIC)
		{
			snprintf(s, sizeof s, " %sPUBLIC%s", pre, post);
			my_strmcat(buffer, s, buflen);
		}
		else if (entry->dont & IGNORE_PUBLIC)
		{
			snprintf(s, sizeof s, " %sPUBLIC", dont);
			my_strmcat(buffer, s, buflen);
		}

		if (entry->type & IGNORE_MSGS)
			my_strmcat(buffer, " MSGS", buflen);
		else if (entry->high & IGNORE_MSGS)
		{
			snprintf(s, sizeof s, " %sMSGS%s", pre, post);
			my_strmcat(buffer, s, buflen);
		}
		else if (entry->dont & IGNORE_MSGS)
		{
			snprintf(s, sizeof s, " %sMSGS", dont);
			my_strmcat(buffer, s, buflen);
		}

		if (entry->type & IGNORE_WALLS)
			my_strmcat(buffer, " WALLS", buflen);
		else if (entry->high & IGNORE_WALLS)
		{
			snprintf(s, sizeof s, " %sWALLS%s", pre, post);
			my_strmcat(buffer, s, buflen);
		}
		else if (entry->dont & IGNORE_WALLS)
		{
			snprintf(s, sizeof s, " %sWALLS", dont);
			my_strmcat(buffer, s, buflen);
		}

		if (entry->type & IGNORE_WALLOPS)
			my_strmcat(buffer, " WALLOPS", buflen);
		else if (entry->high & IGNORE_WALLOPS)
		{
			snprintf(s, sizeof s, " %sWALLOPS%s", pre, post);
			my_strmcat(buffer, s, buflen);
		}
		else if (entry->dont & IGNORE_WALLOPS) {
			snprintf(s, sizeof s, " %sWALLOPS", dont);
			my_strmcat(buffer, s, buflen);
		}

		if (entry->type & IGNORE_INVITES)
			my_strmcat(buffer, " INVITES", buflen);
		else if (entry->high & IGNORE_INVITES)
		{
			snprintf(s, sizeof s, " %sINVITES%s", pre, post);
			my_strmcat(buffer, s, buflen);
		}
		else if (entry->dont & IGNORE_INVITES)
		{
			snprintf(s, sizeof s, " %sINVITES", dont);
			my_strmcat(buffer, s, buflen);
		}

		if (entry->type & IGNORE_NOTICES)
			my_strmcat(buffer, " NOTICES", buflen);
		else if (entry->high & IGNORE_NOTICES)
		{
			snprintf(s, sizeof s, " %sNOTICES%s", pre, post);
			my_strmcat(buffer, s, buflen);
		}
		else if (entry->dont & IGNORE_NOTICES)
		{
			snprintf(s, sizeof s, " %sNOTICES", dont);
			my_strmcat(buffer, s, buflen);
		}

		if (entry->type & IGNORE_NOTES)
			my_strmcat(buffer, " NOTES", buflen);
		else if (entry->high & IGNORE_NOTES)
		{
			snprintf(s, sizeof s, " %sNOTES%s", pre, post);
			my_strmcat(buffer, s, buflen);
		}
		else if (entry->dont & IGNORE_NOTES)
		{
			snprintf(s, sizeof s, " %sNOTES", dont);
			my_strmcat(buffer, s, buflen);
		}

		if (entry->type & IGNORE_CTCPS)
			my_strmcat(buffer, " CTCPS", buflen);
		else if (entry->high & IGNORE_CTCPS)
		{
			snprintf(s, sizeof s, " %sCTCPS%s", pre, post);
			my_strmcat(buffer, s, buflen);
		}
		else if (entry->dont & IGNORE_CTCPS)
		{
			snprintf(s, sizeof s, " %sCTCPS", dont);
			my_strmcat(buffer, s, buflen);
		}

		if (entry->type & IGNORE_CRAP)
			my_strmcat(buffer, " CRAP", buflen);
		else if (entry->high & IGNORE_CRAP)
		{
			snprintf(s, sizeof s, " %sCRAP%s", pre, post);
			my_strmcat(buffer, s, buflen);
		}
		else if (entry->dont & IGNORE_CRAP)
		{
			snprintf(s, sizeof s, " %sCRAP", dont);
			my_strmcat(buffer, s, buflen);
		}
	}
}

/* ignore_list: shows the entired ignorance list */
static	u_char *
ignore_list(u_char *nick, int simple_string)
{
	Ignore	*tmp;
	u_char	buffer[BIG_BUFFER_SIZE], *rv = NULL;

	if (ignored_nicks)
	{
		if (!simple_string)
			say("Ignorance List:");
		if (nick)
			upper(nick);
		for (tmp = ignored_nicks; tmp; tmp = tmp->next)
		{
			u_char	*high;

			/*
			 * ignore if we're searching for a specific nick and
			 * this isn't the one.
			 */
			if (nick && my_strcmp(nick, tmp->nick))
				continue;

			if (simple_string)
			{
				if (!nick)
				{
					malloc_strncat(&rv, tmp->nick, 1);
					my_strcat(rv, " ");
					continue;
				}
				high = empty_string();
			}
			else
				high = highlight_chars;

			ignore_info(tmp, buffer, sizeof buffer, high, high,
				    UP("DONT-"));

			if (simple_string)
			{
				malloc_strcpy(&rv, buffer+1);
				break;
			}
			say("\t%s:\t%s", tmp->nick, buffer);
		}
	}
	else
	{
		say("There is nothing on the ignorance list.");
	}
	return rv;
}

u_char *
ignore_list_string(u_char *nick)
{
	return ignore_list(nick, 1);
}

/*
 * ignore: does the /IGNORE command.  Figures out what type of ignoring the
 * user wants to do and calls the proper ignorance command to do it. 
 */
void
ignore(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*nick,
		*type;
	int	flag, no_flags, ignore_type;

	if ((nick = next_arg(args, &args)) != NULL)
	{
		no_flags = 1;
		while ((type = next_arg(args, &args)) != NULL)
		{
			no_flags = 0;
			upper(type);
			switch (*type)
			{
			case '^':
				flag = IGNORE_DONT;
				type++;
				break;
			case '-':
				flag = IGNORE_REMOVE;
				type++;
				break;
			case '+':
				flag = IGNORE_HIGH;
				type++;
				break;
			default:
				flag = 0;
				break;
			}
			ignore_type = get_ignore_type(type);
			if (ignore_type != -1)
				ignore_nickname(nick, ignore_type, flag);
			else if (ignore_type == -1)
			{
				u_char	*ptr;

				while (nick)
				{
					if ((ptr = my_index(nick, ',')) != NULL)
						*ptr = (u_char) 0;
					if (*nick)
					{
						if (remove_ignore(nick))
							say("%s is not in the ignorance list!", nick);
						else
							say("%s removed from ignorance list", nick);
					}
					if (ptr)
						*(ptr++) = ',';
					nick = ptr;
				}
			}
			else
			{
				say("You must specify one of the following:");
				say("\tALL MSGS PUBLIC WALLS WALLOPS INVITES \
NOTICES NOTES CTCPS CRAP NONE");
			}
		}
		if (no_flags)
			ignore_list(nick, 0);
	} else
		ignore_list(NULL, 0);
}

/*
 * set_highlight_char: what the name says..  the character to use
 * for highlighting..  either BOLD, INVERSE, or UNDERLINE..
 */
void
set_highlight_char(u_char *s)
{
	size_t	len;

	len = my_strlen(s);
	upper(s);

	if (!my_strncmp(s, "BOLD", len))
	{
		set_string_var(HIGHLIGHT_CHAR_VAR, UP("BOLD"));
		highlight_chars[0] = BOLD_TOG;
	}
	else if (!my_strncmp(s, "INVERSE", len))
	{
		set_string_var(HIGHLIGHT_CHAR_VAR, UP("INVERSE"));
		highlight_chars[0] = REV_TOG;
	}
	else if (!my_strncmp(s, "UNDERLINE", len))
	{
		set_string_var(HIGHLIGHT_CHAR_VAR, UP("UNDERLINE"));
		highlight_chars[0] = UND_TOG;
	}
	else
		say("HIGHLIGHT_CHAR must be one of BOLD, INVERSE, or \
			UNDERLINE");
}

int
ignore_combo(int flag1, int flag2)
{
        if (flag1 == DONT_IGNORE || flag2 == DONT_IGNORE)
                return DONT_IGNORE;
        if (flag1 == IGNORED || flag2 == IGNORED)
                return IGNORED;
        if (flag1 == HIGHLIGHTED || flag2 == HIGHLIGHTED)
                return HIGHLIGHTED;
        return 0;
}

/*
 * double_ignore - makes live simpiler when using doing ignore code
 * added, april 1993, phone.
 */
int
double_ignore(u_char *nick, u_char *userhost, int type)
{
	if (userhost)
		return (ignore_combo(is_ignored(nick, type),
			is_ignored(userhost, type)));
	else
		return (is_ignored(nick, type));
}

int
get_ignore_type(u_char *type)
{
	size_t len = my_strlen(type);
	int rv;

	if (len == 0)
		rv = 0;
	if (my_strnicmp(type, UP("ALL"), len) == 0)
		rv = IGNORE_ALL;
	else if (my_strnicmp(type, UP("MSGS"), len) == 0)
		rv = IGNORE_MSGS;
	else if (my_strnicmp(type, UP("PUBLIC"), len) == 0)
		rv = IGNORE_PUBLIC;
	else if (my_strnicmp(type, UP("WALLS"), len) == 0)
		rv = IGNORE_WALLS;
	else if (my_strnicmp(type, UP("WALLOPS"), len) == 0)
		rv = IGNORE_WALLOPS;
	else if (my_strnicmp(type, UP("INVITES"), len) == 0)
		rv = IGNORE_INVITES;
	else if (my_strnicmp(type, UP("NOTICES"), len) == 0)
		rv = IGNORE_NOTICES;
	else if (my_strnicmp(type, UP("NOTES"), len) == 0)
		rv = IGNORE_NOTES;
	else if (my_strnicmp(type, UP("CTCPS"), len) == 0)
		rv = IGNORE_CTCPS;
	else if (my_strnicmp(type, UP("CRAP"), len) == 0)
		rv = IGNORE_CRAP;
	else if (my_strnicmp(type, UP("NONE"), len) == 0)
		rv = -1;
	else
		rv = 0;

	return (rv);
}

void
save_ignore(FILE *fp)
{
	Ignore	*tmp;
	u_char	buffer[BIG_BUFFER_SIZE];

	if (!ignored_nicks)
		return;

	for (tmp = ignored_nicks; tmp; tmp = tmp->next)
	{
		ignore_info(tmp, buffer, sizeof buffer,
			    UP("+"), UP(""), UP("^"));

		fprintf(fp, "IGNORE %s %s\n", tmp->nick, buffer);
	}
}

u_char *
highlight_str(void)
{
	return highlight_chars;
}

int
ignore_usernames(void)
{
	return do_ignore_usernames;
}
