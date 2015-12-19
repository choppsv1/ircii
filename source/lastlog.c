/*
 * lastlog.c: handles the lastlog features of irc. 
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
IRCII_RCSID("@(#)$eterna: lastlog.c,v 1.50 2015/11/20 09:23:54 mrg Exp $");

#include "lastlog.h"
#include "window.h"
#include "screen.h"
#include "vars.h"
#include "ircaux.h"
#include "output.h"
#include "ircterm.h"

/* Keep this private to lastlog.c */
struct	lastlog_stru
{
	int	level;
	u_char	*msg;
	u_char	**lines;
	int	cols;	/* If this doesn't match the current columns,
			 * we have to recalculate the whole thing. */
	struct	lastlog_stru	*next;
	struct	lastlog_stru	*prev;
};

struct lastlog_info_stru
{
	Lastlog	*lastlog_head;		/* pointer to top of lastlog list */
	Lastlog	*lastlog_tail;		/* pointer to bottom of lastlog list */
	int	lastlog_level;		/* The LASTLOG_LEVEL, determines what
					 * messages go to lastlog */
	int	lastlog_size;		/* Max number of messages for the window
					 * lastlog */
};

static	void	remove_from_lastlog(LastlogInfo *);
static	void	lastlog_print_one_line(FILE *, u_char *);

/*
 * lastlog_level: current bitmap setting of which things should be stored in
 * the lastlog.  The LOG_MSG, LOG_NOTICE, etc., defines tell more about this 
 */
static	int	lastlog_level;
static	int	notify_level;

/*
 * msg_level: the mask for the current message level.  What?  Did he really
 * say that?  This is set in the set_lastlog_msg_level() routine as it
 * compared to the lastlog_level variable to see if what ever is being added
 * should actually be added 
 */
static	int	msg_level = LOG_CRAP;

static	char	*levels[] =
{
	"CRAP",		"PUBLIC",	"MSGS",		"NOTICES",
	"WALLS",	"WALLOPS",	"NOTES",	"OPNOTES",
	"SNOTES",	"ACTIONS",	"DCC",		"CTCP",
	"USERLOG1",	"USERLOG2",	"USERLOG3",	"USERLOG4",
	"BEEP",		"HELP"
};
#define NUMBER_OF_LEVELS ARRAY_SIZE(levels)

/*
 * bits_to_lastlog_level: converts the bitmap of lastlog levels into a nice
 * string format.
 */
u_char	*
bits_to_lastlog_level(int level)
{
	static	u_char	lbuf[128]; /* this *should* be enough for this */
	int	i,
		p;
	int	first = 1;

	if (level == LOG_ALL)
		my_strcpy(lbuf, "ALL");
	else if (level == 0)
		my_strcpy(lbuf, "NONE");
	else
	{
		*lbuf = '\0';
		for (i = 0, p = 1; i < NUMBER_OF_LEVELS; i++, p <<= 1)
		{
			if (level & p)
			{
				if (first)
					first = 0;
				else
					my_strmcat(lbuf, " ", 127);
				my_strmcat(lbuf, levels[i], 127);
			}
		}
	}
	return (lbuf);
}

int
parse_lastlog_level(u_char *str)
{
	u_char	*ptr,
		*rest,
		*s;
	int	i,
		p,
		level,
		neg;
	size_t	len;

	level = 0;
	while ((str = next_arg(str, &rest)) != NULL)
	{
		while (str)
		{
			if ((ptr = my_index(str, ',')) != NULL)
				*ptr++ = '\0';
			if ((len = my_strlen(str)) != 0)
			{
				u_char	*cmd = NULL;

				malloc_strcpy(&cmd, str);
				upper(cmd);
				if (my_strncmp(cmd, "ALL", len) == 0)
					level = LOG_ALL;
				else if (my_strncmp(cmd, "NONE", len) == 0)
					level = 0;
				else
				{
					if (*str == '-')
					{
						str++;
						s = cmd + 1;
						neg = 1;
					}
					else {
						neg = 0;
						s = cmd;
					}
					for (i = 0, p = 1; i < NUMBER_OF_LEVELS; i++, p <<= 1)
					{
						if (!my_strncmp(s, levels[i], len))
						{
							if (neg)
								level &= (LOG_ALL ^ p);
							else
								level |= p;
							break;
						}
					}
					if (i == NUMBER_OF_LEVELS)
						say("Unknown lastlog level: %s",
							str);
				}
				new_free(&cmd);
			}
			str = ptr;
		}
		str = rest;
	}
	return (level);
}

/*
 * set_lastlog_level: called whenever a "SET LASTLOG_LEVEL" is done.  It
 * parses the settings and sets the lastlog_level variable appropriately.  It
 * also rewrites the LASTLOG_LEVEL variable to make it look nice 
 */
void
set_lastlog_level(u_char *str)
{
	LastlogInfo *info = window_get_lastlog_info(curr_scr_win);

	lastlog_level = parse_lastlog_level(str);
	set_string_var(LASTLOG_LEVEL_VAR, bits_to_lastlog_level(lastlog_level));
	info->lastlog_level = lastlog_level;
}

static	void
free_lastlog_lines(Lastlog *log)
{
	u_char **lines;

	for (lines = log->lines; *lines; lines++)
		new_free(lines);
	new_free(&log->lines);
}

static	void
free_lastlog_entry(Lastlog *log)
{
	new_free(&log->msg);
	free_lastlog_lines(log);
	new_free(&log);
}

static	void
remove_from_lastlog(LastlogInfo *info)
{
	Lastlog *tmp;

	if (info->lastlog_tail)
	{
		tmp = info->lastlog_tail->prev;
		free_lastlog_entry(info->lastlog_tail);
		info->lastlog_tail = tmp;
		if (tmp)
			tmp->next = NULL;
		else
			info->lastlog_head = NULL;
		info->lastlog_size--;
	}
	else
		info->lastlog_size = 0;
}

/*
 * set_lastlog_size: sets up a lastlog buffer of size given.  If the lastlog
 * has gotten larger than it was before, all previous lastlog entry remain.
 * If it get smaller, some are deleted from the end. 
 */
void
set_lastlog_size(int size)
{
	LastlogInfo *info = window_get_lastlog_info(curr_scr_win);
	int	i,
		diff;

	if (info->lastlog_size > size)
	{
		diff = info->lastlog_size - size;
		for (i = 0; i < diff; i++)
			remove_from_lastlog(info);
	}
}

/*
 * lastlog_print_one_line: print one line to the lastlog file or screen,
 * cutting off the trailing ^O (ALL_OFF) that may be present.
 */
static void
lastlog_print_one_line(FILE *fp, u_char *msg)
{
	size_t	len = my_strlen(msg);
	int	hacked = 0;

	if (len == 0)
		return;
	if (msg[len - 1] == ALL_OFF)
	{
		msg[len - 1] = '\0';
		hacked = 1;
	}
	if (fp)
		fprintf(fp, "%s\n", msg);
	else
		put_it("%s", msg);
	if (hacked)
		msg[len - 1] = ALL_OFF;
}

/*
 * lastlog: the /LASTLOG command.  Displays the lastlog to the screen. If
 * args contains a valid integer, only that many lastlog entries are shown
 * (if the value is less than lastlog_size), otherwise the entire lastlog is
 * displayed 
 *
 * /lastlog -save filename
 * by StElb <stlb@cs.tu-berlin.de>
 */
void
lastlog(u_char *command, u_char *args, u_char *subargs)
{
	int	cnt,
		from = 0,
		p,
		i,
		level = 0,
		m_level,
		mask = 0,
		header = 1;
	Lastlog *start_pos;
	u_char	*match = NULL,
		*save = NULL,
		*expanded = NULL,
		*arg;
	FILE	*fp = NULL;
	u_char	*cmd = NULL;
	size_t	len;
	LastlogInfo *info = window_get_lastlog_info(curr_scr_win);

	save_message_from();
	message_from(NULL, LOG_CURRENT);
	cnt = info->lastlog_size;

	while ((arg = next_arg(args, &args)) != NULL)
	{
		if (*arg == '-')
		{
			arg++;
			if (!(len = my_strlen(arg)))
			{
				header = 0;
				continue;
			}
			malloc_strcpy(&cmd, arg);
			upper(cmd);
			if (!my_strncmp(cmd, "LITERAL", len))
			{
				if (match)
				{
					say("Second -LITERAL argument ignored");
					(void) next_arg(args, &args);
					continue;
				}
				if ((match = next_arg(args, &args)) != NULL)
					continue;
				say("Need pattern for -LITERAL");
				goto out;
			}
			else if (!my_strncmp(cmd, "BEEP", len))
			{
				if (match)
				{
					say("-BEEP is exclusive; ignored");
					continue;
				}
				else
					match = UP("\007");
			}
			else if (!my_strncmp(cmd, "SAVE", len))
			{
#ifdef DAEMON_UID
				if (getuid() == DAEMON_UID)
				{
					say("You are not permitted to use -SAVE flag");
					goto out;
				}
#endif /* DAEMON_UID */
				if (save)
				{
					say("Second -SAVE argument ignored");
					(void) next_arg(args, &args);
					continue;
				}
				if ((save = next_arg(args, &args)) != NULL)
				{
					if (!(expanded = expand_twiddle(save)))
					{
						say("Unknown user");
						goto out;
					}
					if ((fp = fopen(CP(expanded), "w")) != NULL)
						continue;
					say("Error opening %s: %s", save, strerror(errno));
					goto out;
				}
				say("Need filename for -SAVE");
				goto out;
			}
			else
			{
				for (i = 0, p = 1; i < NUMBER_OF_LEVELS; i++, p <<= 1)
				{
					if (my_strncmp(cmd, levels[i], len) == 0)
					{
						mask |= p;
						break;
					}
				}
				if (i == NUMBER_OF_LEVELS)
				{
					say("Unknown flag: %s", arg);
					message_from(NULL, LOG_CRAP);
					goto out;
				}
			}
			continue;
out:
			new_free(&cmd);
			restore_message_from();
			if (fp)
				fclose(fp);
			return;
		}
		else
		{
			if (level == 0)
			{
				if (match || isdigit(*arg))
				{
					cnt = my_atoi(arg);
					level++;
				}
				else
					match = arg;
			}
			else if (level == 1)
			{
				from = my_atoi(arg);
				level++;
			}
		}
	}
	if (cmd)
		new_free(&cmd);

	start_pos = info->lastlog_head;
	for (i = 0; (i < from) && start_pos; start_pos = start_pos->next)
		if (!mask || (mask & start_pos->level))
			i++;

	for (i = 0; (i < cnt) && start_pos; start_pos = start_pos->next)
		if (!mask || (mask & start_pos->level))
			i++;

	level = info->lastlog_level;
	m_level = set_lastlog_msg_level(0);
	if (start_pos == NULL)
		start_pos = info->lastlog_tail;
	else
		start_pos = start_pos->prev;

	/* Let's not get confused here, display a seperator.. -lynx */
	if (header && !save)
		say("Lastlog:");
	for (i = 0; (i < cnt) && start_pos; start_pos = start_pos->prev)
	{
		if (!mask || (mask & start_pos->level))
		{
			i++;
			if (!match || scanstr(start_pos->msg, match)) 
				lastlog_print_one_line(fp, start_pos->msg);
		}
	}
	if (save)
	{
		say("Saved Lastlog to %s", expanded);
		fclose(fp);
	}
	else if (header)
		say("End of Lastlog");
	set_lastlog_msg_level(m_level);
	restore_message_from();
}

/* set_lastlog_msg_level: sets the message level for recording in the lastlog */
int
set_lastlog_msg_level(int level)
{
	int	old;

	old = msg_level;
	msg_level = level;
	return (old);
}

/*
 * add_to_lastlog: adds the line to the lastlog, based upon the current
 * lastlog level.  if added, the split up lines ready for display are
 * returned.
 */
u_char	**
add_to_lastlog(Window *window, u_char *line)
{
	Lastlog *new = NULL;
	LastlogInfo *info;

	if (window == NULL)
		window = curr_scr_win;
	info = window_get_lastlog_info(window);
	if (info->lastlog_level & msg_level)
	{
		/* no nulls or empty lines (they contain "> ") */
		if (line && ((int) my_strlen(line) > 2))
		{
			new = new_malloc(sizeof *new);
			new->next = info->lastlog_head;
			new->prev = NULL;
			new->level = msg_level;
			new->msg = NULL;
			malloc_strcpy(&new->msg, line);
			copy_window_size(NULL, &new->cols);
			Debug(DB_LASTLOG, "columns = %d", new->cols);
			new->lines = split_up_line_alloc(line);

			if (info->lastlog_head)
				info->lastlog_head->prev = new;
			info->lastlog_head = new;

			if (info->lastlog_tail == NULL)
				info->lastlog_tail = info->lastlog_head;

			if (info->lastlog_size++ == get_int_var(LASTLOG_VAR))
				remove_from_lastlog(info);
		}
	}
	if (new)
		return new->lines;
	return NULL;
}

int
islogged(Window	*window)
{
	LastlogInfo *info = window_get_lastlog_info(window);

	return (info->lastlog_level & msg_level) ? 1 : 0;
}

int
real_notify_level(void)
{
	return (notify_level);
}

int
real_lastlog_level(void)
{
	return (lastlog_level);
}

void
set_notify_level(u_char *str)
{
	notify_level = parse_lastlog_level(str);
	set_string_var(NOTIFY_LEVEL_VAR, bits_to_lastlog_level(notify_level));
	window_set_notify_level(curr_scr_win, notify_level);
}

/*
 * free_lastlog: This frees all data and structures associated with the
 * lastlog of the given window 
 */
void
free_lastlog(Window *window)
{
	Lastlog *tmp, *next;
	LastlogInfo *info = window_get_lastlog_info(window);

	for (tmp = info->lastlog_head; tmp; tmp = next)
	{
		next = tmp->next;
		free_lastlog_entry(tmp);
	}
}

/*
 * lastlog_line_back():
 *
 * call initially with a non-NULL "window" argument to initialise
 * the count.  then, each further call to lastlog_line_back() will
 * return the next line of the screen output, going backwards
 * through the lastlog history.  use use split_up_line() from earlier
 * in this file to do the actual splitting of each individiual line.
 */

u_char	*
lastlog_line_back(Window *window)
{
	static	int	row;
	static	Lastlog	*LogLine;
	static	u_char	**TheirLines;

	if (window)
	{
		LastlogInfo *info = window_get_lastlog_info(window);

		LogLine = info->lastlog_head;
		row = -1;
	}
	if (row <= 0)
	{
		int cols;

		if (!window && LogLine)
			LogLine = LogLine->next;
		if (!LogLine)
			return NULL;
		copy_window_size(NULL, &cols);
		Debug(DB_LASTLOG, "save cols %d, new cols %d", LogLine->cols, cols);
		if (LogLine->cols != cols) {
			/* Must free and re-calculate */
			free_lastlog_lines(LogLine);
			LogLine->cols = cols;
			LogLine->lines = split_up_line_alloc(LogLine->msg);
		}
		TheirLines = LogLine->lines;
		for (row = 0; TheirLines[row]; row++)
			/* count the rows */;
		if (window)
			return NULL;
	}
	return TheirLines[--row];
}

LastlogInfo *
lastlog_new_window(void)
{
	LastlogInfo *new = new_malloc(sizeof *new);

	new->lastlog_head = 0;
	new->lastlog_tail = 0;
	new->lastlog_size = 0;
	new->lastlog_level = real_lastlog_level();

	return new;
}

int
lastlog_get_size(LastlogInfo *info)
{
	return info->lastlog_size;
}

int
lastlog_get_level(LastlogInfo *info)
{
	return info->lastlog_level;
}

void
lastlog_set_level(LastlogInfo *info, int level)
{
	info->lastlog_level = level;
}
