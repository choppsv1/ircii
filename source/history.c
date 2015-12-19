/*
 * history.c: stuff to handle command line history 
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
IRCII_RCSID("@(#)$eterna: history.c,v 1.33 2014/03/14 08:07:03 mrg Exp $");

#include "ircaux.h"
#include "vars.h"
#include "history.h"
#include "output.h"
#include "input.h"
#include "debug.h"

static	int	parse_history(u_char *, u_char **);
static	u_char	*history_match(u_char *);
static	void	add_to_history_file(int, u_char *);
static	void	add_to_history_list(int, u_char *);
static	u_char	*get_from_history_file(int);
static	u_char	*get_from_history_buffer(int);

static	FILE *hist_file = NULL;

typedef struct	HistoryStru
{
	int	number;
	u_char	*stuff;
	struct	HistoryStru *next;
	struct	HistoryStru *prev;
}	History;

/* command_history: pointer to head of command_history list */
static	History *command_history_head = NULL;
static	History *command_history_tail = NULL;
static	History *command_history_pos = NULL;

/* hist_size: the current size of the command_history array */
static	int	hist_size = 0;

/* hist_count: the absolute counter for the history list */
static	int	hist_count = 0;

/*
 * last_dir: the direction (next or previous) of the last get_from_history()
 * call.... reset by add to history 
 */
static	int	last_dir = -1;

/*
 * file_pos: The position in the history file of the last history entry
 * zwooped out by get_from_history_file()... look there for how this is used 
 */
static	off_t	file_pos = 0;

/*
 * history pointer
 */
static	History	*history_tmp = NULL;

/*
 * history_match: using wild_match(), this finds the latest match in the
 * history file and returns it as the function result.  Returns null if there
 * is no match.  Note that this sticks a '*' at the end if one is not already
 * there. 
 */
static	u_char	*
history_match(u_char *match)
{
	u_char	*ptr;
	u_char	*match_str = NULL;
	u_char	buffer[BIG_BUFFER_SIZE];

	if (*(match + my_strlen(match) - 1) == '*')
		malloc_strcpy(&match_str, match);
	else
	{
		match_str = new_malloc(my_strlen(match) + 2);
		my_strcpy(match_str, match);
		my_strcat(match_str, "*");
	}
	if (hist_file)
	{
		if (last_dir == -1)
			fseek(hist_file, 0L, 2);
		else
			fseek(hist_file, (long)file_pos, 0);
		while (rfgets(CP(buffer), sizeof buffer, hist_file))
		{
			parse_history(buffer, &ptr);
			if (wild_match(match_str, ptr))
			{
				new_free(&match_str);
				*(ptr + my_strlen(ptr) - 1) = '\0';
				file_pos = ftell(hist_file);
				last_dir = PREV;
				return (ptr);
			}
		}
		file_pos = 0;
	}
	if (!hist_file && get_int_var(HISTORY_VAR))
	{
		if ((last_dir == -1) || (history_tmp == NULL))
			history_tmp = command_history_head;
		else
			history_tmp = history_tmp->next;
		for (; history_tmp; history_tmp = history_tmp->next)
			if (wild_match(match_str, history_tmp->stuff))
			{
				new_free(&match_str);
				last_dir = PREV;
				return (history_tmp->stuff);
			}
	}
	last_dir = -1;
	new_free(&match_str);
	return NULL;
}

/*
 * add_to_history_file: This adds the given line of text to the end of the
 * history file using cnt as the history index number. 
 */
static	void
add_to_history_file(int cnt, u_char *line)
{
	if (hist_file)
	{
		fseek(hist_file, 0L, 2);
		fprintf(hist_file, "%d: %s\n", cnt, line);
		fflush(hist_file);
		file_pos = ftell(hist_file);
	}
}

static	void
add_to_history_list(int cnt, u_char *stuff)
{
	History *new;

	if (get_int_var(HISTORY_VAR) == 0)
		return;
	if ((hist_size == get_int_var(HISTORY_VAR)) && command_history_tail)
	{
		if (hist_size == 1)
		{
			malloc_strcpy(&command_history_tail->stuff, stuff);
			return;
		}
		new = command_history_tail;
		command_history_tail = command_history_tail->prev;
		command_history_tail->next = NULL;
		new_free(&new->stuff);
		new_free(&new);
		if (command_history_tail == NULL)
			command_history_head = NULL;
	}
	else
		hist_size++;
	new = new_malloc(sizeof *new);
	new->stuff = NULL;
	new->number = cnt;
	new->next = command_history_head;
	new->prev = NULL;
	malloc_strcpy(&(new->stuff), stuff);
	if (command_history_head)
		command_history_head->prev = new;
	command_history_head = new;
	if (command_history_tail == NULL)
		command_history_tail = new;
	command_history_pos = NULL;
}

/*
 * set_history_file: this sets the file to be used by the command history
 * function to whatever you send as file.  This expands twiddles and opens
 * the file if all is well 
 */
void
set_history_file(u_char *file)
{
	u_char	*ptr;
	int	i,
		cnt,
		fd;
	History *tmp;

	if (file)
	{
#ifdef DAEMON_UID
		if (getuid() == DAEMON_UID)
		{
			say("You are not permitted to use a HISTORY_FILE");
			set_string_var(HISTORY_FILE_VAR, NULL);
			return;
		}
#endif /* DAEMON_UID */
		if ((ptr = expand_twiddle(file)) == NULL)
		{
			say("Bad filename: %s",file);
			set_string_var(HISTORY_FILE_VAR, NULL);
			return;
		}
		set_string_var(HISTORY_FILE_VAR, ptr);
		if (hist_file)
			fclose(hist_file);
		fd = open(CP(ptr), O_WRONLY|O_CREAT|O_APPEND, 0600);
		if (fd < 0 || ((hist_file = fdopen(fd, "w+")) == NULL))
		{
			say("Unable to open %s: %s", ptr, strerror(errno));
			set_string_var(HISTORY_FILE_VAR, NULL);
			hist_file = NULL;
		}
		else if (hist_size)
		{
			cnt = hist_size;
			tmp = command_history_tail;
			for (i = 0; i < cnt; i++, tmp = tmp->prev)
				add_to_history_file(tmp->number, tmp->stuff);
		}
		new_free(&ptr);
	}
	else if (hist_file)
	{
		fclose(hist_file);
		hist_file = NULL;
	}
}

/*
 * set_history_size: adjusts the size of the command_history to be size. If
 * the array is not yet allocated, it is set to size and all the entries
 * nulled.  If it exists, it is resized to the new size with a realloc.  Any
 * new entries are nulled. 
 */
void
set_history_size(int size)
{
	int	i,
		cnt;
	History *ptr;

	if (size < hist_size)
	{
		cnt = hist_size - size;
		for (i = 0; i < cnt; i++)
		{
			ptr = command_history_tail;
			command_history_tail = ptr->prev;
			new_free(&(ptr->stuff));
			new_free(&ptr);
		}
		if (command_history_tail == NULL)
			command_history_head = NULL;
		else
			command_history_tail->next = NULL;
		hist_size = size;
	}
}

/*
 * parse_history: given a string of the form "number: stuff", this returns
 * the number as an integer and points ret to stuff 
 */
static	int
parse_history(u_char *lbuf, u_char **ret)
{
	u_char	*ptr;
	int	entry;

	if ((ptr = my_index(lbuf, ':')) != NULL)
	{
		entry = my_atoi(lbuf);
		*ret = ptr + 2;
		return (entry);
	}
	*ret = NULL;
	return -1;
}

/*
 * add_to_history: adds the given line to the history array.  The history
 * array is a circular buffer, and add_to_history handles all that stuff. It
 * automagically allocted and deallocated memory as needed 
 */
void
add_to_history(u_char *line)
{
	u_char	*ptr;

	if (line && *line)
		while (line)
		{
			if ((ptr = sindex(line, UP("\n\r"))) != NULL)
				*(ptr++) = '\0';
			Debug(DB_HISTORY, "add_to_history: adding ``%s''", line);
			add_to_history_list(hist_count, line);
			add_to_history_file(hist_count, line);
			last_dir = PREV;
			hist_count++;
			line = ptr;
		}
}

static	u_char	*
get_from_history_file(int which)
{
	u_char	*ptr;
	u_char	buffer[BIG_BUFFER_SIZE];

	if (last_dir == -1)
		last_dir = which;
	else if (last_dir != which)
	{
		last_dir = which;
		get_from_history(which);
	}
	fseek(hist_file, (long)file_pos, 0);
	if (which == NEXT)
	{
		if (!fgets(CP(buffer), sizeof buffer, hist_file))
		{
			file_pos = 0L;
			fseek(hist_file, 0L, 0);
			if (!fgets(CP(buffer), sizeof buffer, hist_file))
				return NULL;
		}
	}
	else if (!rfgets(CP(buffer), sizeof buffer, hist_file))
	{
		fseek(hist_file, 0L, 2);
		file_pos = ftell(hist_file);
		if (!rfgets(CP(buffer), sizeof buffer, hist_file))
			return NULL;
	}
	file_pos = ftell(hist_file);
	buffer[my_strlen(buffer) - 1] = '\0';
	parse_history(buffer, &ptr);
	return (ptr);
}

static	u_char	*
get_from_history_buffer(int which)
{
	if ((get_int_var(HISTORY_VAR) == 0) || (hist_size == 0))
		return NULL;
	/*
	 * if (last_dir != which) { last_dir = which; get_from_history(which); }
	 */
	if (which == NEXT)
	{
		if (command_history_pos)
		{
			if (command_history_pos->prev)
				command_history_pos = command_history_pos->prev;
			else
				command_history_pos = command_history_tail;
		}
		else
		{
			add_to_history(get_input());
			command_history_pos = command_history_tail;
		}
		return (command_history_pos->stuff);
	}
	else
	{
		if (command_history_pos)
		{
			if (command_history_pos->next)
				command_history_pos = command_history_pos->next;
			else
				command_history_pos = command_history_head;
		}
		else
		{
			add_to_history(get_input());
			command_history_pos = command_history_head;
		}
		return (command_history_pos->stuff);
	}
}

u_char	*
get_from_history(int which)
{
	u_char	*str = NULL;

	if (get_string_var(HISTORY_FILE_VAR))
		str = get_from_history_file(which);
	if (!str)
		str = get_from_history_buffer(which);
	return str;
}

/* history: the /HISTORY command, shows the command history buffer. */
void
history(u_char *command, u_char *args, u_char *subargs)
{
	int	cnt,
		max;
	u_char	*value;
	u_char	buffer[BIG_BUFFER_SIZE];
	History *tmp;

	say("Command History:");
	if (hist_file)
	{
		if ((value = next_arg(args, &args)) != NULL)
		{
			if ((max = my_atoi(value)) > hist_count)
				max = 0;
			else
				max = hist_count - max + 1;
		}
		else
			max = 0;

		fseek(hist_file, 0L, 0);
		while (--max > 0)
			if (fgets(CP(buffer), sizeof buffer, hist_file) == NULL)
			{
				say("History file inconsistent, length, oops.");;
				return;
			}
		while (fgets(CP(buffer), sizeof buffer, hist_file))
		{
			buffer[my_strlen(buffer) - 1] = '\0';
			put_it("%s", buffer);
		}
	}
	else if (get_int_var(HISTORY_VAR))
	{
		if ((value = next_arg(args, &args)) != NULL)
		{
			if ((max = my_atoi(value)) > get_int_var(HISTORY_VAR))
				max = get_int_var(HISTORY_VAR);
		}
		else
			max = get_int_var(HISTORY_VAR);
		for (tmp = command_history_tail, cnt = 0; tmp && (cnt < max);
				tmp = tmp->prev, cnt++)
			put_it("%d: %s", tmp->number, tmp->stuff);
	}
}

/*
 * do_history: This finds the given history entry in either the history file,
 * or the in memory history buffer (if no history file is given). It then
 * returns the found entry as its function value, or null if the entry is not
 * found for some reason.  Note that this routine mallocs the string returned  
 */
u_char	*
do_history(u_char *com, u_char *rest)
{
	int	hist_num;
	u_char	*ptr,
		*ret = NULL;
	u_char	buffer[BIG_BUFFER_SIZE];
	static	u_char	*last_com = NULL;

	if (!com || !*com)
	{
		if (last_com)
			com = last_com;
		else
			com = empty_string();
	}
	else
		/*	last_dir = -1; */
		malloc_strcpy(&last_com, com);

	if (!is_number(com))
	{
		if ((ptr = history_match(com)) != NULL)
		{
			if (rest && *rest)
			{
				ret = new_malloc(my_strlen(ptr) + my_strlen(rest) + 2);
				my_strcpy(ret, ptr);
				my_strcat(ret, " ");
				my_strcat(ret, rest);
			}
			else
				malloc_strcpy(&ret, ptr);
		}
		else
			say("No Match");
		return (ret);
	}
	hist_num = my_atoi(com);
	if (hist_file)
	{
		fseek(hist_file, 0L, 0);
		while (fgets(CP(buffer), sizeof buffer, hist_file))
			if (parse_history(buffer, &ptr) == hist_num)
			{
				*(ptr + my_strlen(ptr) - 1) = '\0';
				if (rest && *rest)
				{
					ret = new_malloc(my_strlen(ptr) + my_strlen(rest) + 2);
					my_strcpy(ret, ptr);
					my_strcat(ret, " ");
					my_strcat(ret, rest);
				}
				else
					malloc_strcpy(&ret, ptr);
				last_dir = PREV;
				file_pos = ftell(hist_file);
				return (ret);
			}
		say("No such history entry: %d", hist_num);
		file_pos = 0;
	}
	else
	{
		History *tmp;

		for (tmp = command_history_head; tmp; tmp = tmp->next)
			if (tmp->number == hist_num)
			{
				if (rest && *rest)
				{
					ret = new_malloc(my_strlen(tmp->stuff) + my_strlen(rest) + 2);
					my_strcpy(ret, tmp->stuff);
					my_strcat(ret, " ");
					my_strcat(ret, rest);
				}
				else
					malloc_strcpy(&ret, tmp->stuff);
				return (ret);
			}
		say("No such history entry: %d", hist_num);
	}
	return NULL;
}
