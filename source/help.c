/*
 * help.c: handles the help stuff for irc 
 *
 * Written by Michael Sandrof, Troy Rollo and Matthew R. Green.
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
IRCII_RCSID("@(#)$eterna: help.c,v 1.96 2014/03/14 20:59:19 mrg Exp $");

#include <dirent.h>
#define NLENGTH(d) (my_strlen((d)->d_name)

#include <sys/stat.h>

#include "ircterm.h"
#include "server.h"
#include "vars.h"
#include "ircaux.h"
#include "input.h"
#include "window.h"
#include "screen.h"
#include "output.h"
#include "help.h"
#include "scandir.h"
#include "sl.h"


static	void	help_me(u_char *, u_char *);
static	void	help_show_paused_topic(u_char *, u_char *);
static	void	create_help_window(void);
static	void	set_help_screen(Screen *);
static	int	compar(const void *, const void *);
static	int	selectent(const struct dirent *);
static	int	show_help(Window *, u_char *);
static	void	help_prompt(u_char *, u_char *);
static	void	help_topic(u_char *, u_char *);

static	Window	*help_window = NULL;
static	FILE	*help_fp;
static	u_char	no_help[] = "NOHELP";
static	int	entry_size;
static	u_char	*this_arg;
static	int	finished_help_paging = 0;
static	int	help_show_directory = 0;
static	int	dont_pause_topic = 0;
static	Screen  *help_screen = NULL;
static	u_char	paused_topic[128];
static	u_char	help_topic_list[BIG_BUFFER_SIZE] = "";
static	int	use_help_window = 0;
static	StringList *help_paused_lines;

/* we are piglet */

/* compar: used by scandir to alphabetize the help entries */
static	int
compar(const void *e1v, const void *e2v)
{
	const struct dirent * const *e1 = e1v;
	const struct dirent * const *e2 = e2v;

	return (my_stricmp((u_char *) (*e1)->d_name, (u_char *) (*e2)->d_name));
}

/*
 * selectent: used by scandir to decide which entries to include in the help
 * listing.  
 */
static	int
selectent(const struct dirent *entry)
{
	if (*(entry->d_name) == '.')
		return (0);
	if (my_strnicmp((u_char *)entry->d_name, this_arg, my_strlen(this_arg)))
		return (0);
	else
	{
		int len = my_strlen(entry->d_name);
#ifdef ZCAT
		const char *temp;
	/*
	 * Handle major length of filename is case of suffix .Z:
	 * stripping suffix length
	 */
		temp = &(entry->d_name[len - my_strlen(ZSUFFIX)]);
		if (!my_strcmp(temp, ZSUFFIX))
			len -= my_strlen(ZSUFFIX);
#endif /* ZCAT */
		entry_size = (len > entry_size) ? len : entry_size;
		return (1);
	}
}

/*
 * show_help:  show's either a page of text from a help_fp, or the whole
 * thing, depending on the value of HELP_PAGER_VAR.  If it gets to the end,
 * (in either case it will eventally), it closes the file, and returns 0
 * to indicate this.
 */ 
static	int
show_help(Window *window, u_char *name)
{
	int	rows = 0;
	u_char	line[81];

	if (!help_fp)
		return (0);
	if (window)
	{
		set_curr_scr_win(window);
	}
	else
	{
		window = curr_scr_win;
	}
	if (get_int_var(HELP_PAGER_VAR))
		rows = window_get_display_size(window);
	while (--rows)
	{
 		if (fgets(CP(line), 80, help_fp))
		{
			u_char	*p = line + my_strlen(line) - 1;

			if (*p == '\n')
				*p = '\0';

			if (*line != '!' && *line != '#')
				help_put_it(name, "%s", line);
			else
				rows++;
		}
		else
			return (0);
	}
	return (1);
}

/*
 * help_prompt: The main procedure called to display the help file
 * currently being accessed.  Using add_wait_prompt(), it sets it
 * self up to be recalled when the next page is asked for.   If
 * called when we have finished paging the help file, we exit, as
 * there is nothing left to show.  If line is 'q' or 'Q', exit the
 * help pager, clean up, etc..  If all is cool for now, we call
 * show_help, and either if its finished, exit, or prompt for the
 * next page.   From here, if we've finished the help page, and
 * doing help prompts, prompt for the help..
 */

static	void
help_prompt(u_char *name, u_char *line)
{
	if (finished_help_paging)
	{
		if (*paused_topic)
			help_show_paused_topic(paused_topic, NULL);
		return;
	}

	if (line && ((*line == 'q') || (*line == 'Q')))
	{
		finished_help_paging = 1;
		if (help_fp)
		{
			fclose(help_fp);
			help_fp = NULL;
		}
		set_help_screen(NULL);
		return;
	}

	if (show_help(help_window, name))
		if (term_basic())
			help_prompt(name, NULL);
		else
			add_wait_prompt(UP("*** Hit any key for more, 'q' to quit ***"),
				help_prompt, name, WAIT_PROMPT_KEY);
	else
	{
		finished_help_paging = 1;
		if (help_fp)
		{
			fclose(help_fp);
			help_fp = NULL;
		}
		if (help_show_directory)
		{
			if (get_int_var(HELP_PAGER_VAR))
				if (term_basic())
					help_show_paused_topic(paused_topic, NULL);
				else
					add_wait_prompt(UP("*** Hit any key to end ***"),
						help_show_paused_topic, paused_topic,
						WAIT_PROMPT_KEY);
			else
			{
				help_show_paused_topic(paused_topic, NULL);
				set_help_screen(NULL);
			}
			help_show_directory = 0;
			return;
		}
	}

	if (finished_help_paging)
	{
		if (get_int_var(HELP_PROMPT_VAR))
		{
			u_char	tmp[BIG_BUFFER_SIZE];

			snprintf(CP(tmp), sizeof tmp, "%s%sHelp? ", help_topic_list,
				*help_topic_list ? " " : "");
			if (!term_basic())
				add_wait_prompt(tmp, help_me, help_topic_list,
					WAIT_PROMPT_LINE);
		}
		else
		{
			if (*paused_topic)
				help_show_paused_topic(paused_topic, NULL);
			set_help_screen(NULL);
		}
	}
}

/*
 * help_topic:  Given a topic, we search the help directory, and try to
 * find the right file, if all is cool, and we can open it, or zcat it,
 * then we call help_prompt to get the actually displaying of the file
 * on the road.
 */
static	void
help_topic(u_char *path, u_char *name)
{
	struct	stat	stat_buf;
	u_char	filename[BIG_BUFFER_SIZE];

#ifdef ZCAT
	u_char	*name_z = NULL;
	u_char	*temp;
#endif /* ZCAT */

	if (name == NULL)
		return;

	/*
	 * Check the existence of <name> or <name>.Z .. Handle suffix
	 * .Z if present.  Open the file if it isn't present, zcat the
	 * file if it is present, and ends with .Z ..
	 */

	snprintf(CP(filename), sizeof filename, "%s/%s", path, name);

#ifdef ZCAT

	if (my_strcmp(name + (my_strlen(name) - my_strlen(ZSUFFIX)), ZSUFFIX))
	{
		malloc_strcpy(&name_z, name);
		malloc_strcat(&name_z, UP(ZSUFFIX));
	}
	if (stat(CP(filename), &stat_buf) == -1)
	{
		snprintf(CP(filename), sizeof filename, "%s/%s", path, name_z);
		if (stat(CP(filename), &stat_buf) == -1)
		{
			help_put_it(name, "*** No help available on %s: Use \
? for list of topics", name);
			return;
		}
		else
			name = name_z;
	}
	else
		new_free(&name_z);
#else
	if (stat(filename, &stat_buf) == -1)
	{
		help_put_it(name, "*** No help available on %s: Use \
? for list of topics", name);
		return;

	}

#endif /* ZCAT */

	if (stat_buf.st_mode & S_IFDIR)
		return;

	if (help_fp)
		fclose(help_fp);
#ifdef ZCAT

	if (my_strcmp(filename + (my_strlen(filename) - my_strlen(ZSUFFIX)), ZSUFFIX))
	{

#endif /* ZCAT */

		if ((help_fp = fopen(CP(filename), "r")) == NULL)
		{
			help_put_it(name, "*** No help available on %s: Use \
? for list of topics", name);
			return;
		}
#ifdef ZCAT

	}
	else
	{
		if ((help_fp = zcat(filename)) == NULL)
		{
			help_put_it(name, "*** No help available on %s: Use \
? for list of topics", name);
			return;
		}
	}

	/*
	 * If the name ended in a .Z, truncate it, so we display the name
	 * with out the .Z
	 */

	temp = &(name[my_strlen(name) - my_strlen(ZSUFFIX)]);
	if (!my_strcmp(temp, ZSUFFIX))
		temp[0] = '\0';
	
#endif /* ZCAT */

	/*
	 * Hopefully now we have got a file descriptor <help_fp>, a name
	 * so we start displaying the help file, calling help_prompt for
	 * the first time.
	 */

	help_put_it(name, "*** Help on %s", name);
	help_prompt(name, NULL);
}

/*
 * help_pause_add_line: this procedure does a help_put_it() call, but
 * puts off the calling, until help_show_paused_topic() is called.
 * I do this because I need to create the list of help topics, but
 * not show them, until we've seen the whole file, so we called
 * help_show_paused_topic() when we've seen the file, if it is needed.
 */

static	void
help_pause_add_line(char *format, ...)
{
	va_list vl;

	u_char	buf[BIG_BUFFER_SIZE], *copy = 0;

	va_start(vl, format);
	vsnprintf(CP(buf), sizeof buf, format, vl);
	va_end(vl);
	malloc_strcpy(&copy, buf);
	if (!help_paused_lines)
		help_paused_lines = sl_init();
	sl_add(help_paused_lines, CP(copy));
}

/*
 * help_show_paused_topic:  see above.  Called when we've seen the
 * whole help file, and we have a list of topics to display.
 */
static	void
help_show_paused_topic(u_char *name, u_char *unused)
{
	int	i = 0;

	if (!help_paused_lines)
		return;
	for (i = 0; i < help_paused_lines->sl_cur; i++)
	{
		help_put_it(name, "%s", help_paused_lines->sl_str[i]);
		new_free(&help_paused_lines->sl_str[i]);
	}
	if (get_int_var(HELP_PROMPT_VAR))
	{
		u_char	buf[BIG_BUFFER_SIZE];

		snprintf(CP(buf), sizeof buf, "%s%sHelp? ", name, (name && *name) ? " " : "");
		if (!term_basic())
			add_wait_prompt(buf, help_me, name, WAIT_PROMPT_LINE);
	}
	else
		set_help_screen(NULL);

	dont_pause_topic = 0;
	sl_free(help_paused_lines, 0);
	help_paused_lines = 0;
}

/*
 * help_me:  The big one.  The help procedure that handles working out
 * what was actually requested, sets up the paused topic list if it is
 * needed, does pretty much all the hard work.
 */
static	void
help_me(u_char *topics, u_char *args)
{
	u_char	*ptr;
	struct	dirent	**namelist = NULL;
	int	entries,
		free_cnt = 0,
		cnt,
		i,
		cols;
	struct	stat	stat_buf;
	u_char	path[BIG_BUFFER_SIZE];
	int	help_paused_first_call = 0;
	u_char	*help_paused_path = NULL;
	u_char	*help_paused_name = NULL;
	u_char	*temp;
	u_char	tmp[BIG_BUFFER_SIZE];
	u_char	buffer[BIG_BUFFER_SIZE];

#ifdef ZCAT
	u_char	*arg_z = NULL;
#endif /* ZCAT */ 

	my_strmcpy(help_topic_list, topics, sizeof help_topic_list);

#ifdef DAEMON_UID
	if (DAEMON_UID == getuid())
		ptr = DEFAULT_HELP_PATH;
	else
#endif /* DAEMON_UID */
		ptr = get_string_var(HELP_PATH_VAR);

	snprintf(CP(path), sizeof path, "%s/%s", ptr, topics);
	for (ptr = path; (ptr = my_index(ptr, ' '));)
		*ptr = '/';
 

	/*
	 * first we check access to the help dir, whinge if we can't, then
	 * work out we need to ask them for more help, else we check the
	 * args list, and do the stuff 
	 */

	if (help_show_directory)
	{
		help_show_paused_topic(paused_topic, NULL);
		help_show_directory = 0;
	}
		
	finished_help_paging = 0;
	if (access(CP(path), R_OK|X_OK))
	{
		help_put_it(no_help, "*** Cannot access help directory!");
		set_help_screen(NULL);
		return;
	}

	this_arg = next_arg(args, &args);
	if (!this_arg && *help_topic_list && get_int_var(HELP_PROMPT_VAR))
	{
		if ((temp = my_rindex(help_topic_list, ' ')) != NULL)
			*temp = '\0';
		else
			*help_topic_list = '\0';
		snprintf(CP(tmp), sizeof tmp, "%s%sHelp? ", help_topic_list,
			*help_topic_list ? " " : "");
		if (!term_basic())
			add_wait_prompt(tmp, help_me, help_topic_list,
				WAIT_PROMPT_LINE);
		return;
	}

	if (!this_arg)		/*  && *help_topic_list) */
	{
		set_help_screen(NULL);
		return;
	}

	create_help_window();
	while (this_arg)
	{
		save_message_from();
		message_from(NULL, LOG_CURRENT);
		if (*this_arg == (u_char) 0)
			help_topic(path, NULL);
		if (my_strcmp(this_arg, "?") == 0)
		{
			this_arg = empty_string();
			if (!dont_pause_topic)
				dont_pause_topic = 1;
		}
		entry_size = 0;

		/*
		 * here we clean the namelist if it exists, and then go to
		 * work on the directory.. working out if is dead, or if we
		 * can show some help, or create the paused topic list.
		 */

		if (namelist)
		{
			for (i = 0; i < free_cnt; i++)
				new_free(&(namelist[i]));
			new_free(&namelist);
		}
		free_cnt = entries = scandir(CP(path), &namelist,
					     selectent, compar);
		/* special case to handle stuff like LOG and LOGFILE */
		if (entries > 1)
		{
#ifdef ZCAT
		/* Check if exist compressed or uncompressed entries */
			malloc_strcpy(&arg_z, this_arg);
			malloc_strcat(&arg_z, UP(ZSUFFIX));
			if (my_stricmp(UP(namelist[0]->d_name), arg_z) == 0 ||
				my_stricmp(UP(namelist[0]->d_name), this_arg) == 0)
#else
			if (my_stricmp(UP(namelist[0]->d_name), this_arg) == 0)
#endif /* ZCAT */
				entries = 1;
#ifdef ZCAT
			new_free(&arg_z);
#endif /* ZCAT */
		}

		/*
		 * entries: -1 means something really died, 0 means there
		 * was no help, 1, means it wasn't a directory, and so to
		 * show the help file, and the default means to add the
		 * stuff to the paused topic list..
		 */

		if (!*help_topic_list)
			dont_pause_topic = 1;
		switch (entries)
		{
		case -1:
			help_put_it(no_help, "*** Error during help function: %s", strerror(errno));
			set_help_screen(NULL);
			if (help_paused_first_call)
			{
				help_topic(help_paused_path, help_paused_name);
				help_paused_first_call = 0;
				new_free(&help_paused_path);
				new_free(&help_paused_name);
			}
			return;
		case 0:
			help_put_it(this_arg, "*** No help available on %s: Use ? for list of topics", this_arg);
			if (!get_int_var(HELP_PROMPT_VAR))
			{
				set_help_screen(NULL);
				break;
			}
			snprintf(CP(tmp), sizeof tmp, "%s%sHelp? ", help_topic_list,
				*help_topic_list ? " " : "");
			if (!term_basic())
				add_wait_prompt(tmp, help_me, help_topic_list,
						WAIT_PROMPT_LINE);
			if (help_paused_first_call)
			{
				help_topic(help_paused_path, help_paused_name);
				help_paused_first_call = 0;
				new_free(&help_paused_path);
				new_free(&help_paused_name);
			}
			for (i = 0; i < free_cnt; i++)
			{
				new_free(&namelist[i]);
			}
			break;
		case 1:
			snprintf(CP(tmp), sizeof tmp, "%s/%s", path, namelist[0]->d_name);
			if (stat(CP(tmp), &stat_buf) == -1)
			{
				for (i = 0; i < free_cnt; i++)
				{
					new_free(&namelist[i]);
				}
				continue;
			}
			if (stat_buf.st_mode & S_IFDIR)
			{
				my_strmcpy(path, tmp, sizeof path);
				if (*help_topic_list)
					my_strmcat(help_topic_list, " ", sizeof help_topic_list);
				my_strmcat(help_topic_list, namelist[0]->d_name, sizeof help_topic_list);
				if ((this_arg = next_arg(args, &args)) ==
						NULL)
				{
					help_paused_first_call = 1;
					malloc_strcpy(&help_paused_path, path);
					malloc_strcpy(&help_paused_name,
						UP(namelist[0]->d_name));
					dont_pause_topic = -1;
					this_arg = UP("?");
				}
				for (i = 0; i < free_cnt; i++)
				{
					new_free(&namelist[i]);
				}
				continue;
			}
			else
			{
				help_topic(path, UP(namelist[0]->d_name));
				finished_help_paging = 0;	/* this is a big kludge */
				for (i = 0; i < free_cnt; i++)
				{
					new_free(&namelist[i]);
				}
				break;
			}
		default:
			help_show_directory = 1;
			my_strmcpy(paused_topic, help_topic_list, sizeof paused_topic);
			help_pause_add_line("*** %s choices:", help_topic_list);
			*buffer = (u_char) 0;
			cnt = 0;
			entry_size += 2;
			cols = (get_co() - 10) / entry_size;
			for (i = 0; i < entries; i++)
			{
#ifdef ZCAT
		/*
		 * In tmp store the actual help choice and strip .Z
		 * suffix in compressed files: put filename (without
		 * .Z) on the help screen.  If it is the first choice
		 * cat it to the buffer and save the last choice
		 */
				my_strmcpy(tmp, namelist[i]->d_name, sizeof tmp);
				temp = &(tmp[my_strlen(tmp) - my_strlen(ZSUFFIX)]);
				if (!my_strcmp(temp, ZSUFFIX))
					temp[0] = '\0';
				my_strmcat(buffer, tmp, sizeof buffer);
#else
				my_strmcat(buffer, namelist[i]->d_name, sizeof buffer);
#endif /* ZCAT */
				if (++cnt == cols)
				{
					help_pause_add_line("%s", buffer);
					*buffer = (u_char) 0;
					cnt = 0;
				}
				else
				{
					int	x,
						l;

					l = my_strlen(namelist[i]->d_name);
#ifdef ZCAT
					/* XXX - this needs to be fixed properly */
					if ((temp = my_rindex(namelist[i]->d_name, '.')) != NULL &&
						my_index(namelist[i]->d_name, *ZSUFFIX))
					l -= my_strlen(ZSUFFIX);
#endif /* ZCAT */
					for (x = l; x < entry_size; x++)
						my_strmcat(buffer, " ", sizeof buffer);
				}
			}
			help_pause_add_line("%s", buffer);
			if (help_paused_first_call)
			{
				help_topic(help_paused_path, help_paused_name);
				help_paused_first_call = 0;
				new_free(&help_paused_path);
				new_free(&help_paused_name);
			}
			if (dont_pause_topic == 1)
			{
				help_show_paused_topic(paused_topic, NULL);
				help_show_directory = 0;
			}
			break;
		}
		for (i = 0; i < free_cnt; i++)
		{
			new_free(&namelist[i]);
		}
		new_free(&namelist);
		restore_message_from();
		break;
	}
	/*
	 * This one is for when there was never a topic and the prompt
	 * never got a topic..  and help_screen was never reset..
	 * phone, jan 1993.
	 */
	if (!*help_topic_list && finished_help_paging)
		set_help_screen(NULL);
}

/*
 * help: the HELP command, gives help listings for any and all topics out
 * there 
 */
void
help(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*help_path;

	finished_help_paging = 0;
	help_show_directory = 0;
	dont_pause_topic = 0;
	use_help_window = 0;

#ifdef DAEMON_UID
	if (DAEMON_UID == getuid())
		help_path = DEFAULT_HELP_PATH;
	else
#endif /* DAEMON_UID */
		help_path = get_string_var(HELP_PATH_VAR);
	if (!(help_path && *help_path && !access(CP(help_path), R_OK | X_OK)))
	{
		if (*help_path)
			help_put_it(no_help, "*** HELP_PATH error: %s", strerror(errno));
		else
			help_put_it(no_help, "*** No HELP_PATH variable set");
		return;
	}

	/*
	 * Check that we aren't doing HELP in a more than one screen, as this
	 * really leads to seriously difficult to fix problems!.  This is all
	 * due to the wildly popular /window create.  blah.
	 */
	if (help_path && help_screen && help_screen != get_current_screen())
	{
		say("You may not run help in two screens");
		return;
	}
	help_screen = get_current_screen();
	help_window = NULL;
	help_me(empty_string(), (args && *args) ? args : (u_char *) "?");
}

static	void
create_help_window(void)
{
	if (help_window)
		return;

	if (!term_basic() && get_int_var(HELP_WINDOW_VAR))
	{
		use_help_window = 1;
		help_window = new_window();
		window_set_hold_mode(help_window, OFF);
		update_all_windows();
	}
	else
		help_window = curr_scr_win;
}

static	void
set_help_screen(Screen *screen)
{
	help_screen = screen;
	if (!help_screen && help_window)
	{
		if (use_help_window)
		{
			unsigned display = set_display_off();
			delete_window(help_window);
			set_display(display);
		}
		help_window = NULL;
		update_all_windows();
	}
}
