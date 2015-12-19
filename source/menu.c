/*
 * menu.c: stuff for the menu's..
 *
 * Written By Troy Rollo <troy@cbme.unsw.oz.au>
 *
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
IRCII_RCSID("@(#)$eterna: menu.c,v 1.50 2014/03/11 20:35:05 mrg Exp $");

#include "menu.h"
#include "list.h"
#include "ircaux.h"
#include "ircterm.h"
#include "window.h"
#include "screen.h"
#include "input.h"
#include "vars.h"
#include "output.h"

#ifdef lines
#undef lines
#endif /* lines */

typedef	struct
{
	u_char	*Name;
	u_char	*Arguments;
	void	(*Func)(u_char *);
} MenuOption;

typedef struct	menu_stru Menu;
struct	menu_stru
{
	Menu	*next;
	u_char	*Name;
	u_int	TotalOptions;
	MenuOption	**Options;
};

struct	window_menu_stru
{
	Menu	*menu;
	u_int	lines;
	u_int	items_per_line;
	u_int	cursor;
};

static	Menu	*MenuList = NULL;

struct	OptionTableTag
{
	char	*name;
	void	(*func)(u_char *);
};

typedef	struct	OptionTableTag OptionTable;

static	OptionTable	OptionsList[] =
{
	{ "PREVIOUS",	menu_previous },
	{ "MENU",	menu_submenu },
	{ "EXIT",	menu_exit },
	{ "CHANNELS",	menu_channels },
	{ "COMMAND",	menu_command },
	{ NULL,	NULL }
};

static	MenuOption *create_new_option(void);
static	Menu	* create_new_menu(void);
static	void	install_menu(Menu *, int);

static	MenuOption *
create_new_option(void)
{
	MenuOption *new;

	new = new_malloc(sizeof *new);
	new->Name = NULL;
	new->Arguments = NULL;
	new->Func = NULL;
	return new;
}

static	Menu	*
create_new_menu(void)
{
	Menu	*new;

	new = new_malloc(sizeof *new);
	new->Name = NULL;
	new->TotalOptions = 0;
	new->Options = NULL;
	return new;
}


static	void
install_menu(Menu *NewMenu, int TotalMade)
{
	MenuOption **NewOptions;
	int	i;

	if (!NewMenu)
		return;
	if (TotalMade != NewMenu->TotalOptions)
	{
		NewOptions = (MenuOption **) malloc(TotalMade *
							sizeof(MenuOption *));
		for (i = 0; i < TotalMade; i++)
			NewOptions[i] = NewMenu->Options[i];
		new_free(&NewMenu->Options);
		NewMenu->Options = NewOptions;
		NewMenu->TotalOptions = TotalMade;
	}
	add_to_list((List **)(void *)&MenuList, (List *) NewMenu);
	say("Menu \"%s\" added", NewMenu->Name);
}

void
load_menu(u_char *FileName)
{
	FILE	*fp;
	Menu	*NewMenu = NULL;
	MenuOption **NewOptions;
	u_char	*line, *command;
	u_char	*name, *func;
	u_char	buffer[BIG_BUFFER_SIZE];
	int	linenum = 0;
	int	CurTotal = 0;
	int	FuncNum;
	int	i;

	if ((fp = fopen(CP(FileName), "r")) == NULL)
	{
		say("Unable to open %s", FileName);
		return;
	}
	while (fgets(CP(buffer), sizeof buffer, fp))
	{
		buffer[my_strlen(buffer)-1] = '\0';
		linenum++;
		line = buffer;
		while (isspace(*line))
			line++;
		if (*line == '#' || !(command = next_arg(line, &line)))
			continue;
		if (!my_stricmp(command, UP("MENU")))
		{
			if (!line || !*line)
			{
				put_it("Missing argument in line %d of %s",
						linenum, FileName);
				continue;
			}
			install_menu(NewMenu, CurTotal);
			NewMenu = create_new_menu();
			malloc_strcpy(&NewMenu->Name, line);
		}
		else if (!my_stricmp(command, UP("OPTION")))
		{
			if (!(name = new_next_arg(line, &line)) ||
			    !(func = next_arg(line, &line)))
			{
				say("Missing argument in line %d of %s",
						linenum, FileName);
				continue;
			}
			for (i=0; OptionsList[i].name; i++)
				if (!my_stricmp(func, UP(OptionsList[i].name)))
					break;
			if (!OptionsList[i].name)
			{
				say("Unknown menu function \"%s\" in line %d of %s",
					func, linenum, FileName);
				continue;
			}
			FuncNum = i;
			if (++CurTotal > NewMenu->TotalOptions)
			{
				NewOptions = new_malloc(sizeof(*NewOptions) *
						(CurTotal + 4));
				for (i = 0; i < NewMenu->TotalOptions; i++)
					NewOptions[i] = NewMenu->Options[i];
				new_free(&NewMenu->Options);
				NewMenu->Options = NewOptions;
				NewMenu->TotalOptions = CurTotal + 4;
			}
			NewMenu->Options[CurTotal-1] = create_new_option();
			malloc_strcpy(&NewMenu->Options[CurTotal-1]->Name,
				name);
			malloc_strcpy(&NewMenu->Options[CurTotal-1]->Arguments,
				line);
			NewMenu->Options[CurTotal-1]->Func =
					OptionsList[FuncNum].func;
		}
		else
			say("Unkown menu command in line %d of %s",
				linenum, FileName);
	}
	install_menu(NewMenu, CurTotal);
	fclose(fp);
}

int
ShowMenuByWindow(Window *window, int flags)
{
	int	i;
	int	largest;
	int	NumPerLine;
	int	len;
	WindowMenu *menu_info;
	Menu	*ThisMenu;
	int	CursorLoc;
	int	co = get_co();

	menu_info = window_get_menu(window);
	ThisMenu = menu_info->menu;
	CursorLoc = menu_info->cursor;
	largest = 0;
	for (i = 0; i < ThisMenu->TotalOptions; i++)
		if ((len = my_strlen(ThisMenu->Options[i]->Name))>largest)
			largest = len;
	NumPerLine = (co - co % (largest + 3)) / (largest + 3);
	menu_info->items_per_line = NumPerLine;
	menu_info->lines = 0;
	for (i = 0; i < ThisMenu->TotalOptions; i++)
	{
		if ((flags & SMF_ERASE) && !(i % NumPerLine) &&
		    !(flags & SMF_CALCONLY))
		{
			term_move_cursor(0, window_get_top(window) +
					    menu_info->lines);
			term_clear_to_eol();
		}
		if ((i == CursorLoc || !(flags&SMF_CURSONLY)) &&
		    !(flags & SMF_CALCONLY))
		{
			if (i == CursorLoc && !(flags & SMF_NOCURSOR) &&
			    get_inside_menu() == 1)
				term_standout_on();
			else
				term_bold_on();
			term_move_cursor((i % NumPerLine) * (largest + 3),
				window_get_top(window) + menu_info->lines);
			fwrite(ThisMenu->Options[i]->Name,
				my_strlen(ThisMenu->Options[i]->Name), 1, stdout);
			if (i == CursorLoc && !(flags & SMF_NOCURSOR))
				term_standout_off();
			else
				term_bold_off();
		}
		if (!((i + 1) % NumPerLine))
			menu_info->lines++;
	}
	if (i % NumPerLine)
		menu_info->lines++;
	window_set_display_size_normal(window);
	if (window_get_cursor(window) < 0)
		window_set_cursor(window, 0);
	fflush(stdout);
	update_input(UPDATE_JUST_CURSOR);
	return ThisMenu->TotalOptions;
}

int
ShowMenu(u_char *Name)
{
	Menu	*ThisMenu;
	Window	*window;
	WindowMenu *menu_info;

	window = curr_scr_win;
	menu_info = window_get_menu(window);
	ThisMenu = (Menu *) find_in_list((List **)(void *)&MenuList, Name, 0);
	if (!ThisMenu)
	{
		say("No such menu \"%s\"", Name);
		return -1;
	}
	menu_info->cursor = 0;
	menu_info->menu = ThisMenu;
	return ShowMenuByWindow(window, SMF_CALCONLY);
}

void
set_menu(u_char *Value)
{
	Window	*window;
	WindowMenu *menu_info;
	ShrinkInfo SizeInfo;

	window = curr_scr_win;
	menu_info = window_get_menu(window);
	if (!Value)
	{
		menu_info->menu = NULL;
		menu_info->lines = 0;
		window_set_display_size_normal(window);
		SizeInfo = resize_display(window);
		redraw_resized(window, SizeInfo, 0);
		update_input(UPDATE_JUST_CURSOR);
		set_inside_menu(-1);
	}
	else
	{
		if (ShowMenu(Value) == -1)
		{
			set_string_var(MENU_VAR, NULL);
			return;
		}
		SizeInfo = resize_display(window);
		redraw_resized(window, SizeInfo, 0);
		ShowMenuByWindow(window, SMF_ERASE);
	}
}

void
enter_menu(u_int key, u_char *ptr)
{
	WindowMenu *menu_info;

	menu_info = window_get_menu(curr_scr_win);
	if (!menu_info->menu)
		return;
	set_inside_menu(1);
	ShowMenuByWindow(curr_scr_win, SMF_CURSONLY);
}


void
menu_previous(u_char *args)
{
}

void
menu_submenu(u_char *args)
{
}

void
menu_exit(u_char *args)
{
	set_inside_menu(-1);
}

void
menu_channels(u_char *args)
{
}

void
menu_key(u_int ikey)
{
	Window *window;
	WindowMenu *menu_info;
	Menu	*ThisMenu;
	u_char	key = (u_char)ikey;

	window = curr_scr_win;
	menu_info = window_get_menu(window);
	ThisMenu = menu_info->menu;
	ShowMenuByWindow(window, SMF_CURSONLY | SMF_NOCURSOR);
	switch(key)
	{
	case 'U':
	case 'u':
	case 'P':
	case 'p':
	case 'k':
	case 'K':
	case 'U' - '@':
	case 'P' - '@':
		menu_info->cursor -= menu_info->items_per_line;
		break;
	case 'n':
	case 'd':
	case 'j':
	case 'N':
	case 'D':
	case 'J':
	case 'D' - '@':
	case 'N' - '@':
		menu_info->cursor += menu_info->items_per_line;
		break;
	case 'b':
	case 'h':
	case 'B':
	case 'H':
	case 'B' - '@':
		menu_info->cursor--;
		break;
	case 'f':
	case 'l':
	case 'F':
	case 'L':
	case 'F' - '@':
		menu_info->cursor++;
		break;
	case '\033':
		break;
	case '\r':
	case '\n':
		window_hold_mode(NULL, OFF, 1);
		break;
	case ' ':
	case '.':
		set_inside_menu(0);
		ThisMenu->Options[menu_info->cursor]->Func(
			ThisMenu->Options[menu_info->cursor]->Arguments);
		if (get_inside_menu() != -1)
			set_inside_menu(1);
		else
			set_inside_menu(0);
		return; /* The menu may not be here any more */
	}
	if (menu_info->cursor >= menu_info->menu->TotalOptions)
		menu_info->cursor = menu_info->menu->TotalOptions - 1;
	if (menu_info->cursor < 0)
		menu_info->cursor = 0;
	if (get_inside_menu())
		ShowMenuByWindow(window, SMF_CURSONLY);
}

void
menu_command(u_char *args)
{
	parse_line(NULL, args, empty_string(), 0, 0, 0);
}

WindowMenu *
menu_init(void)
{
	WindowMenu *new = new_malloc(sizeof *new);

	new->lines = 0;
	new->menu = 0;
	
	return new;
}

u_int
menu_lines(WindowMenu *menu)
{
	return menu->lines;
}

int
menu_active(WindowMenu *menu)
{
	return menu->menu != NULL;
}
