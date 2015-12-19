/*
 * hook.c: Does those naughty hook functions. 
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
IRCII_RCSID("@(#)$eterna: hook.c,v 1.87 2014/03/14 01:57:16 mrg Exp $");

#include "hook.h"
#include "vars.h"
#include "ircaux.h"
#include "alias.h"
#include "list.h"
#include "window.h"
#include "server.h"
#include "output.h"
#include "edit.h"
#include "buffer.h"

struct	hook_stru
{
	Hook	*next;		/* pointer to next element in list */
	u_char	*nick;		/* The Nickname */
	int	not;		/* If true, this entry should be
				 * ignored when matched, otherwise it
				 * is a normal entry */
	int	noisy;		/* flag indicating how much output
				 * should be given */
	int	server;		/* the server in which this hook
				 * applies. (-1 if none). If bit 0x1000
				 * is set, then no other hooks are
				 * tried in the given server if all the
				 * server specific ones fail
				 */
	int	sernum;		/* The serial number for this hook. This
				 * is used for hooks which will be
				 * concurrent with others of the same
				 * pattern. The default is 0, which
				 * means, of course, no special
				 * behaviour. If any 1 hook suppresses
				 * the * default output, output will be
				 * suppressed.
				 */
	u_char	*stuff;		/* The this that gets done */
	int	global;		/* set if loaded from `global' */
};

struct hook_func_stru
{
	u_char	*name;		/* name of the function */
	Hook	*list;		/* pointer to head of the list for this
				 * function */
	int	params;		/* number of parameters expected */
	int	mark;
	unsigned flags;
};

struct numeric_list_stru
{
	NumericList *next;
	u_char	*name;
	Hook	*list;
};

static	u_char	*fill_it_out(u_char *, int);
static	void	setup_struct(int, int, int, int);
static	int	Add_Remove_Check(List *, u_char *);
static	int	Add_Remove_Check_List(List *, List*);
static	void	add_numeric_hook(int, u_char *, u_char *, int, int, int, int);
static	void	add_hook(int, u_char *, u_char *, int, int, int, int);
static	int	show_numeric_list(int);
static	int	show_list(int);
static	void	remove_numeric_hook(int, u_char *, int, int, int);
static	void	write_hook(FILE *, Hook *, u_char *);

#define SILENT 0
#define QUIET 1
#define NORMAL 2
#define NOISY 3

/*
 * The various ON levels: SILENT means the DISPLAY will be OFF and it will
 * suppress the default action of the event, QUIET means the display will be
 * OFF but the default action will still take place, NORMAL means you will be
 * notified when an action takes place and the default action still occurs,
 * NOISY means you are notified when an action occur plus you see the action
 * in the display and the default actions still occurs 
 */
static	char	*noise_level[] = { "SILENT", "QUIET", "NORMAL", "NOISY" };

#define	HS_NOGENERIC	0x1000
#define	HF_LOOKONLY	0x0001
#define HF_NORECURSE	0x0002
#define HF_GLOBAL	0x0004

static	int	in_on_who_real = 0,
		current_hook = -1;	/* used in the send_text()
					   routine */

static	NumericList *numeric_list = NULL;

/* hook_functions: the list of all hook functions available */
static	HookFunc hook_functions[] =
{
	/* name, list, number of args, mark, flags */
	{ UP("ACTION"),		NULL,	3,	0,	0 },
	{ UP("CHANNEL_NICK"),	NULL,	3,	0,	0 },
	{ UP("CHANNEL_SIGNOFF"),NULL,	3,	0,	0 },
	{ UP("CONNECT"),	NULL,	1,	0,	0 },
	{ UP("CTCP"),		NULL,	4,	0,	0 },
	{ UP("CTCP_REPLY"),	NULL,	3,	0,	0 },
	{ UP("DCC_CHAT"),	NULL,	2,	0,	0 },
	{ UP("DCC_CONNECT"),	NULL,	2,	0,	0 },
	{ UP("DCC_ERROR"),	NULL,	6,	0,	0 },
	{ UP("DCC_LIST"),	NULL,	6,	0,	0 },
	{ UP("DCC_LOST"),	NULL,	2,	0,	0 },
	{ UP("DCC_RAW"),	NULL,	3,	0,	0 },
	{ UP("DCC_REQUEST"),	NULL,	4,	0,	0 },
	{ UP("DISCONNECT"),	NULL,	1,	0,	0 },
	{ UP("ENCRYPTED_NOTICE"),NULL,	3,	0,	0 },
	{ UP("ENCRYPTED_PRIVMSG"),NULL,	3,	0,	0 },
	{ UP("EXEC"),		NULL,	2,	0,	0 },
	{ UP("EXEC_ERRORS"),	NULL,	2,	0,	0 },
	{ UP("EXEC_EXIT"),	NULL,	3,	0,	0 },
	{ UP("EXEC_PROMPT"),	NULL,	2,	0,	0 },
	{ UP("EXIT"),		NULL,	1,	0,	0 },
	{ UP("FLOOD"),		NULL,	3,	0,	0 },
	{ UP("HELP"),		NULL,	2,	0,	0 },
	{ UP("HOOK"),		NULL,	1,	0,	0 },
	{ UP("ICB_CMDOUT"),	NULL,	1,	0,	0 },
	{ UP("ICB_ERROR"),	NULL,	1,	0,	0 },
	{ UP("ICB_STATUS"),	NULL,	2,	0,	0 },
	{ UP("ICB_WHO"),	NULL,	6,	0,	0 },
	{ UP("IDLE"),		NULL,	1,	0,	0 },
	{ UP("INPUT"),		NULL,	1,	0,	0 },
	{ UP("INVITE"),		NULL,	2,	0,	0 },
	{ UP("JOIN"),		NULL,	3,	0,	0 },
	{ UP("KICK"),		NULL,	3,	0,	HF_LOOKONLY },
	{ UP("LEAVE"),		NULL,	2,	0,	0 },
	{ UP("LIST"),		NULL,	3,	0,	HF_LOOKONLY },
	{ UP("MAIL"),		NULL,	2,	0,	0 },
	{ UP("MODE"),		NULL,	3,	0,	0 },
	{ UP("MSG"),		NULL,	2,	0,	0 },
	{ UP("MSG_GROUP"),	NULL,	3,	0,	0 },
	{ UP("NAMES"),		NULL,	2,	0,	HF_LOOKONLY },
	{ UP("NICKNAME"),	NULL,	2,	0,	0 },
	{ UP("NOTE"),		NULL,	10,	0,	0 },
	{ UP("NOTICE"),		NULL,	2,	0,	0 },
	{ UP("NOTIFY_SIGNOFF"),	NULL,	1,	0,	0 },
	{ UP("NOTIFY_SIGNON"),	NULL,	1,	0,	0 },
	{ UP("OS_SIGNAL"),	NULL,	1,	0,	0 },
	{ UP("PUBLIC"),		NULL,	3,	0,	0 },
	{ UP("PUBLIC_MSG"),	NULL,	3,	0,	0 },
	{ UP("PUBLIC_NOTICE"),	NULL,	3,	0,	0 },
	{ UP("PUBLIC_OTHER"),	NULL,	3,	0,	0 },
	{ UP("RAW_IRC"),	NULL,	1,	0,	0 },
	{ UP("RAW_SEND"),	NULL,	1,	0,	0 },
	{ UP("SEND_ACTION"),	NULL,	2,	0,	0 },
	{ UP("SEND_DCC_CHAT"),	NULL,	2,	0,	0 },
	{ UP("SEND_MSG"),	NULL,	2,	0,	0 },
	{ UP("SEND_NOTICE"),	NULL,	2,	0,	0 },
	{ UP("SEND_PUBLIC"),	NULL,	2,	0,	0 },
	{ UP("SEND_TALK"),	NULL,	2,	0,	0 },
	{ UP("SERVER_NOTICE"),	NULL,	1,	0,	0 },
	{ UP("SIGNOFF"),	NULL,	1,	0,	0 },
	{ UP("TALK"),		NULL,	2,	0,	0 },
	{ UP("TIMER"),		NULL,	1,	0,	0 },
	{ UP("TOPIC"),		NULL,	3,	0,	0 },
	{ UP("WALL"),		NULL,	2,	0,	HF_LOOKONLY },
	{ UP("WALLOP"),		NULL,	3,	0,	HF_LOOKONLY },
	{ UP("WHO"),		NULL,	6,	0,	HF_LOOKONLY },
	{ UP("WIDELIST"),	NULL,	1,	0,	HF_LOOKONLY },
	{ UP("WINDOW"),		NULL,	2,	0,	HF_NORECURSE },
	{ UP("WINDOW_KILL"),	NULL,	1,	0,	0 },
	{ UP("WINDOW_LIST"),	NULL,	8,	0,	0 },
	{ UP("WINDOW_SWAP"),	NULL,	2,	0,	0 }
};

static u_char	*
fill_it_out(u_char *str, int params)
{
	u_char	lbuf[BIG_BUFFER_SIZE];
	u_char	*arg,
		*free_ptr = NULL,
		*ptr;
	int	i = 0;

	malloc_strcpy(&free_ptr, str);
	ptr = free_ptr;
	*lbuf = (u_char) 0;
	while ((arg = next_arg(ptr, &ptr)) != NULL)
	{
		if (*lbuf)
			my_strmcat(lbuf, " ", sizeof lbuf);
		my_strmcat(lbuf, arg, sizeof lbuf);
		if (++i == params)
			break;
	}
	for (; i < params; i++)
		my_strmcat(lbuf, (i < params-1) ? " %" : " *", sizeof lbuf);
	if (*ptr)
	{
		my_strmcat(lbuf, " ", sizeof lbuf);
		my_strmcat(lbuf, ptr, sizeof lbuf);
	}
	malloc_strcpy(&free_ptr, lbuf);
	return (free_ptr);
}


/*
 * A variety of comparison functions used by the hook routines follow.
 */

struct	CmpInfoStruc
{
	int	ServerRequired;
	int	SkipSerialNum;
	int	SerialNumber;
	int	Flags;
}	cmp_info;

#define	CIF_NOSERNUM	0x0001
#define	CIF_SKIP	0x0002

int	cmpinfodone = 0;

static void
setup_struct(int ServReq, int SkipSer, int SerNum, int flags)
{
	cmp_info.ServerRequired = ServReq;
	cmp_info.SkipSerialNum = SkipSer;
	cmp_info.SerialNumber = SerNum;
	cmp_info.Flags = flags;
}

static	int
Add_Remove_Check(List *_Item, u_char *Name)
{
	int	comp;
	Hook	*Item = (Hook *)_Item;

	if (cmp_info.SerialNumber != Item->sernum)
		return (Item->sernum > cmp_info.SerialNumber) ? 1 : -1;
	if ((comp = my_stricmp(Item->nick, Name)) != 0)
			return comp;
	if (Item->server != cmp_info.ServerRequired)
		return (Item->server > cmp_info.ServerRequired) ? 1 : -1;
	return 0;
}

static	int
Add_Remove_Check_List(List *_Item, List *_Item2)
{
	return Add_Remove_Check(_Item, _Item->name);
}

static	void
add_numeric_hook(int numeric, u_char *nick, u_char *stuff, int noisy, int not, int server, int sernum)
{
	NumericList *entry;
	Hook	*new;
	u_char	buf[4];

	snprintf(CP(buf), sizeof buf, "%3.3u", numeric);
	if ((entry = (NumericList *) find_in_list((List **)(void *)&numeric_list, buf, 0)) == NULL)
	{
		entry = new_malloc(sizeof *entry);
		entry->name = NULL;
		entry->list = NULL;
		malloc_strcpy(&(entry->name), buf);
		add_to_list((List **)(void *)&numeric_list, (List *) entry);
	}

	setup_struct((server==-1) ? -1 : (server & ~HS_NOGENERIC), sernum-1, sernum, 0);
	if ((new = (Hook *) remove_from_list_ext((List **)(void *)&(entry->list), nick, Add_Remove_Check)) != NULL)
	{
		new->not = 1;
		new_free(&(new->nick));
		new_free(&(new->stuff));
		wait_new_free((u_char **)(void *)&new);
	}
	new = new_malloc(sizeof *new);
	new->nick = NULL;
	new->noisy = noisy;
	new->server = server;
	new->sernum = sernum;
	new->not = not;
	new->global = loading_global();
	new->stuff = NULL;
	malloc_strcpy(&new->nick, nick);
	malloc_strcpy(&new->stuff, stuff);
	upper(new->nick);
	add_to_list_ext((List **)(void *)&(entry->list), (List *) new, Add_Remove_Check_List);
}

/*
 * add_hook: Given an index into the hook_functions array, this adds a new
 * entry to the list as specified by the rest of the parameters.  The new
 * entry is added in alphabetical order (by nick). 
 */
static	void
add_hook(int which, u_char *nick, u_char *stuff, int noisy, int not, int server, int sernum)
{
	Hook	*new;

	if (which < 0)
	{
		add_numeric_hook(-which, nick, stuff, noisy, not, server,
			sernum);
		return;
	}
	if (which >= NUMBER_OF_LISTS)
	{
		yell("add_hook: which > NUMBER_OF_LISTS");
		return;
	}
	setup_struct((server == -1) ? -1 : (server & ~HS_NOGENERIC), sernum-1, sernum, 0);
	if ((new = (Hook *) remove_from_list_ext((List **)(void *)&(hook_functions[which].list), nick, Add_Remove_Check)) != NULL)
	{
		new->not = 1;
		new_free(&(new->nick));
		new_free(&(new->stuff));
		wait_new_free((u_char **)(void *)&new);
	}
	new = new_malloc(sizeof *new);
	new->nick = NULL;
	new->noisy = noisy;
	new->server = server;
	new->sernum = sernum;
	new->not = not;
	new->stuff = NULL;
	new->global = loading_global();
	malloc_strcpy(&new->nick, nick);
	malloc_strcpy(&new->stuff, stuff);
	upper(new->nick);
	add_to_list_ext((List **)(void *)&(hook_functions[which].list), (List *) new, Add_Remove_Check_List);
}

/* show_hook shows a single hook */
void
show_hook(Hook *list, u_char *name)
{
	if (list->server != -1)
		say("On %s from \"%s\" do %s [%s] <%d> (Server %d)%s",
		    name, list->nick,
		    (list->not ? (u_char *) "nothing" : list->stuff),
		    noise_level[list->noisy], list->sernum,
		    list->server&~HS_NOGENERIC,
		    (list->server&HS_NOGENERIC) ? (u_char *) " Exclusive" : empty_string());
	else
		say("On %s from \"%s\" do %s [%s] <%d>",
		    name, list->nick,
		    (list->not ? (u_char *) "nothing" : list->stuff),
		    noise_level[list->noisy],
		    list->sernum);
}

/*
 * show_numeric_list: If numeric is 0, then all numeric lists are displayed.
 * If numeric is non-zero, then that particular list is displayed.  The total
 * number of entries displayed is returned 
 */
static	int
show_numeric_list(int numeric)
{
	NumericList *tmp;
	Hook	*list;
	u_char	buf[4];
	int	cnt = 0;

	if (numeric)
	{
		snprintf(CP(buf), sizeof buf, "%3.3u", numeric);
		if ((tmp = (NumericList *) find_in_list((List **)(void *)&numeric_list, buf, 0))
				!= NULL)
		{
			for (list = tmp->list; list; list = list->next, cnt++)
				show_hook(list, tmp->name);
		}
	}
	else
	{
		for (tmp = numeric_list; tmp; tmp = tmp->next)
		{
			for (list = tmp->list; list; list = list->next, cnt++)
				show_hook(list, tmp->name);
		}
	}
	return (cnt);
}

/*
 * show_list: Displays the contents of the list specified by the index into
 * the hook_functions array.  This function returns the number of entries in
 * the list displayed 
 */
static	int
show_list(int which)
{
	Hook	*list;
	int	cnt = 0;

	if (which >= NUMBER_OF_LISTS)
	{
		yell("show_list: which >= NUMBER_OF_LISTS");
		return 0;
	}
	/* Less garbage when issueing /on without args. (lynx) */
	for (list = hook_functions[which].list; list; list = list->next, cnt++)
		show_hook(list, hook_functions[which].name);
	return (cnt);
}

/*
 * do_hook: This is what gets called whenever a MSG, INVITES, WALL, (you get
 * the idea) occurs.  The nick is looked up in the appropriate list. If a
 * match is found, the stuff field from that entry in the list is treated as
 * if it were a command. First it gets expanded as though it were an alias
 * (with the args parameter used as the arguments to the alias).  After it
 * gets expanded, it gets parsed as a command.  This will return as its value
 * the value of the noisy field of the found entry, or -1 if not found. 
 */
/* huh-huh.. this sucks.. im going to re-write it so that it works */
int
do_hook(int which, char *format, ...)
{
	va_list vl;
	Hook	*tmp, **list;
	u_char	*name = NULL;
	int	RetVal = 1;
	int	i,
		old_in_on_who;
	Hook	*hook_array[2048];	/* XXX ugh */
	int	hook_num = 0;
	static	int hook_level = 0;
	size_t	len;
	u_char	*foo;
	PUTBUF_INIT
	int currser = 0, oldser = 0;
	int currmatch = 0, oldmatch = 0;
	Hook *bestmatch = NULL;
	int nomorethisserial = 0;
#ifdef NEED_PUTBUF_DECLARED
	/* make this buffer *much* bigger than needed */
	u_char	putbuf[2*BIG_BUFFER_SIZE];

	putbuf[0] ='\0';
#endif

	hook_level++;

	va_start(vl, format);
	PUTBUF_SPRINTF(format, vl);
	va_end(vl);
	if (which < 0)
	{
		NumericList *hook;
		u_char	buf[4];

		snprintf(CP(buf), sizeof buf, "%3.3u", -which);
		if ((hook = (NumericList *) find_in_list((List **)(void *)&numeric_list, buf, 0))
				!= NULL)
		{
			name = hook->name;
			list = &hook->list;
		}
		else
			list = NULL;
	}
	else
	{
		if (which >= NUMBER_OF_LISTS)
		{
			yell("do_hook: which >= NUMBER_OF_LISTS");
			goto out;
		}
		if (hook_functions[which].mark && (hook_functions[which].flags & HF_NORECURSE))
			list = NULL;
		else
		{
			list = &(hook_functions[which].list);
			name = hook_functions[which].name;
		}
	}
	if (!list)
	{
		RetVal = 1;
		goto out;
	}

	if (which >= 0)
		hook_functions[which].mark++;

	for (tmp = *list; tmp; tmp = tmp->next)
	{
		currser = tmp->sernum;
		if (currser != oldser)      /* new serial number */
		{
			oldser = currser;
			currmatch = oldmatch = nomorethisserial = 0;
			if (bestmatch)
				hook_array[hook_num++] = bestmatch;
			bestmatch = NULL;
		}

		if (nomorethisserial) 
			continue;
				/* if there is a specific server
				hook and it doesnt match, then
				we make sure nothing from
				this serial number gets hooked */
		if ((tmp->server != -1) &&
		    (tmp->server & HS_NOGENERIC) &&
		    (tmp->server != (get_from_server() & HS_NOGENERIC)))
		{
			nomorethisserial = 1;
			bestmatch = NULL;
			continue;
		}
		currmatch = wild_match(tmp->nick, putbuf);
		if (currmatch > oldmatch)
		{
			oldmatch = currmatch;
			bestmatch = tmp;
		}
	}
	if (bestmatch)
		hook_array[hook_num++] = bestmatch;

	for (i = 0; i < hook_num; i++)
	{
		unsigned display;

		tmp = hook_array[i];
		if (!tmp)
		{
			if (which >= 0)
				hook_functions[which].mark--;
			goto out;
		}
		if (tmp->not)
			continue;
		current_hook = which;
		if (tmp->noisy > QUIET)
			say("%s activated by \"%s\"", name, putbuf);
		display = get_display();
		if (tmp->noisy < NOISY)
			set_display_off();

		save_message_from();
		old_in_on_who = in_on_who_real;
		if (which == WHO_LIST || (which <= -311 && which >= -318))
			in_on_who_real = 1;
		len = my_strlen(tmp->stuff) + 1; 
		foo = new_malloc(len);
		memmove(foo, tmp->stuff, len);
		parse_line(NULL, foo, putbuf, 0, 0, 1);
		new_free(&foo);
		in_on_who_real = old_in_on_who;
		set_display(display);
		current_hook = -1;
		restore_message_from();
		if (!tmp->noisy && !tmp->sernum)
			RetVal = 0;
	}
	if (which >= 0)
		hook_functions[which].mark--;
out:
	PUTBUF_END
	return really_free(--hook_level), RetVal;
}

static	void
remove_numeric_hook(int numeric, u_char *nick, int server, int sernum, int quiet)
{
	NumericList *hook;
	Hook	*tmp,
		*next;
	u_char	buf[4];

	snprintf(CP(buf), sizeof buf, "%3.3u", numeric);
	if ((hook = (NumericList *) find_in_list((List **)(void *)&numeric_list, buf,0)) != NULL)
	{
		if (nick)
		{
			setup_struct((server == -1) ? -1 :
			    (server & ~HS_NOGENERIC), sernum - 1, sernum, 0);
			if ((tmp = (Hook *) remove_from_list((List **)(void *)&(hook->list), nick)) != NULL)
			{
				if (!quiet)
					say("\"%s\" removed from %s list", nick, buf);
				tmp->not = 1;
				new_free(&(tmp->nick));
				new_free(&(tmp->stuff));
				wait_new_free((u_char **)(void *)&tmp);
				if (hook->list == NULL)
				{
					if ((hook = (NumericList *) remove_from_list((List **)(void *)&numeric_list, buf)) != NULL)
					{
						new_free(&(hook->name));
						new_free(&hook);
					}
				}
				return;
			}
		}
		else
		{
			for (tmp = hook->list; tmp; tmp = next)
			{
				next = tmp->next;
				tmp->not = 1;
				new_free(&(tmp->nick));
				new_free(&(tmp->stuff));
				wait_new_free((u_char **)(void *)&tmp);
			}
			hook->list = NULL;
			if (!quiet)
				say("The %s list is empty", buf);
			return;
		}
	}
	if (quiet)
		return;
	if (nick)
		say("\"%s\" is not on the %s list", nick, buf);
	else
		say("The %s list is empty", buf);
}
 
void
remove_hook(int which, u_char *nick, int server, int sernum, int quiet)
{
	Hook	*tmp,
		*next;

	if (which < 0)
	{
		remove_numeric_hook(-which, nick, server, sernum, quiet);
		return;
	}
	if (which >= NUMBER_OF_LISTS)
	{
		yell("remove_hook: which >= NUMBER_OF_LISTS");
		return;
	}
	if (nick)
	{
		setup_struct((server == -1) ? -1 : (server & ~HS_NOGENERIC),
			sernum-1, sernum, 0);
		if ((tmp = (Hook *) remove_from_list_ext((List **)(void *)&hook_functions[which].list, nick, Add_Remove_Check)) != NULL)
		{
			if (!quiet)
				say("\"%s\" removed from %s list", nick, hook_functions[which].name);
			tmp->not = 1;
			new_free(&(tmp->nick));
			new_free(&(tmp->stuff));
			wait_new_free((u_char **)(void *)&tmp);
		}
		else if (!quiet)
			say("\"%s\" is not on the %s list", nick, hook_functions[which].name);
	}
	else
	{
		for(tmp = hook_functions[which].list; tmp; tmp=next)
		{
			next = tmp->next;
			tmp->not = 1;
			new_free(&(tmp->nick));
			new_free(&(tmp->stuff));
			wait_new_free((u_char **)(void *)&tmp);
		}
		hook_functions[which].list = NULL;
		if (!quiet)
			say("The %s list is empty", hook_functions[which].name);
	}
}

/* on: The ON command */
void
on(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*func,
		*nick,
		*serial,
		*cmd = NULL;
	/* int noisy = NORMAL, not = 0, do_remove = 0, -not used */
	int	noisy,
		not,
		server,
		sernum,
		do_remove,
		which = 0,
		cnt,
		i,
		ambiguous = 0;
	size_t	len;

	if (get_int_var(NOVICE_VAR) && !get_load_depth())
	{
	    yell("*** You may not type ON commands when you have the NOVICE");
	    yell("*** variable set to ON. Some ON commands may cause a");
	    yell("*** security breach on your machine, or enable another");
	    yell("*** user to control your IRC session. Read the help files");
	    yell("*** in /HELP ON before using ON");
	    return;
	}
	if ((func = next_arg(args, &args)) != NULL)
	{
		if (*func == '#')
		{
			if (!(serial = next_arg(args, &args)))
			{
				say("No serial number specified");
				return;
			}
			sernum = my_atoi(serial);
			func++;
		}
		else
			sernum = 0;
		switch (*func)
		{
		case '&':
			server = get_from_server();
			func++;
			break;
		case '@':
			server = get_from_server()|HS_NOGENERIC;
			func++;
			break;
		default:
			server = -1;
			break;
		}
		switch (*func)
		{
		case '-':
			noisy = QUIET;
			func++;
			break;
		case '^':
			noisy = SILENT;
			func++;
			break;
		case '+':
			noisy = NOISY;
			func++;
			break;
		default:
			noisy = NORMAL;
			break;
		}
		if ((len = my_strlen(func)) == 0)
		{
			say("You must specify an event type!");
			return;
		}
		malloc_strcpy(&cmd, func);
		upper(cmd);
		for (cnt = 0, i = 0; i < NUMBER_OF_LISTS; i++)
		{
			if (!my_strncmp(cmd, hook_functions[i].name, len))
			{
				if (my_strlen(hook_functions[i].name) == len)
				{
					cnt = 1;
					which = i;
					break;
				}
				else
				{
					cnt++;
					which = i;
				}
			}
			else if (cnt)
				break;
		}
		if (cnt == 0)
		{
			if (is_number(cmd))
			{
				which = my_atoi(cmd);
				if ((which < 0) || (which > 999))
				{
					say("Numerics must be between 001 and 999");
					goto out;
				}
				which = -which;
			}
			else
			{
				say("No such ON function: %s", func);
				goto out;
			}
		}
		else if (cnt > 1)
			ambiguous = 1;
		else 
		{
			if (get_int_var(INPUT_PROTECTION_VAR) && !my_strnicmp(UP(hook_functions[which].name), UP("INPUT"), 5))
			{
				say("You cannot use /ON INPUT with INPUT_PROTECTION set");
				say("Please read /HELP ON INPUT, and /HELP SET INPUT_PROTECTION");
				goto out;
			}
		}
		do_remove = 0;
		not = 0;
		switch (*args)
		{
		case '-':
			do_remove = 1;
			args++;
			break;
		case '^':
			not = 1;
			args++;
			break;
		}
		if ((nick = new_next_arg(args, &args)) != NULL)
		{
			if (ambiguous)
			{
				say("Ambiguous ON function: %s", func);
				goto out;
			}
			if (which < 0)
				nick = fill_it_out(nick, 1);
			else
				nick = fill_it_out(nick,
					hook_functions[which].params);
			if (do_remove)
			{
				if (my_strlen(nick) == 0)
					say("No expression specified");
				else
					remove_hook(which, nick, server,
						sernum, 0);
			}
			else
			{
				if (not)
					args = empty_string();
				if (*nick)
				{
					if (*args == LEFT_BRACE)
					{
						u_char	*ptr;

						ptr = MatchingBracket(++args,
								LEFT_BRACE, RIGHT_BRACE);
						if (!ptr)
						{
							say("Unmatched brace in ON");
							new_free(&nick);
							goto out;
						}
						else if (ptr[1])
						{
							say("Junk after closing brace in ON");
							new_free(&nick);
							goto out;
						}
						else
							*ptr = '\0';
					}
					add_hook(which, nick, args, noisy, not, server, sernum);
					if (which < 0)
						say("On %3.3u from \"%s\" do %s [%s] <%d>",
						    -which, nick, (not ? (u_char *) "nothing" : args),
						    noise_level[noisy], sernum);
					else
						say("On %s from \"%s\" do %s [%s] <%d>",
							hook_functions[which].name, nick,
							(not ? (u_char *) "nothing" : args),
							noise_level[noisy], sernum);
				}
			}
			new_free(&nick);
		}
		else
		{
			if (do_remove)
			{
				if (ambiguous)
				{
					say("Ambiguous ON function: %s", func);
					goto out;
				}
				remove_hook(which, NULL, server,
				    sernum, 0);
			}
			else
			{
				if (which < 0)
				{
					if (show_numeric_list(-which) == 0)
						say("The %3.3u list is empty.",
							-which);
				}
				else if (ambiguous)
				{
					for (i = 0; i < NUMBER_OF_LISTS; i++)
						if (!my_strncmp(cmd,
						   hook_functions[i].name, len))
							(void)show_list(i);

				}
				else if (show_list(which) == 0)
					say("The %s list is empty.",
						hook_functions[which].name);
			}
		}
	}
	else
	{
		int	total = 0;

		say("ON listings:");
		for (which = 0; which < NUMBER_OF_LISTS; which++)
			total += show_list(which);
		total += show_numeric_list(0);
		if (total == 0)
			say("All ON lists are empty.");
	}
out:
	new_free(&cmd);
}

static	void
write_hook(FILE *fp, Hook *hook, u_char *name)
{
	char	*stuff = NULL;

	if (hook->server!=-1)
		return;
	switch (hook->noisy)
	{
	case SILENT:
		stuff = "^";
		break;
	case QUIET:
		stuff = "-";
		break;
	case NORMAL:
		stuff = CP(empty_string());
		break;
	case NOISY:
		stuff = "+";
		break;
	}
	if (hook->sernum)
		fprintf(fp, "ON #%s%s %d \"%s\"", stuff, name, hook->sernum,
			hook->nick);
	else
		fprintf(fp, "ON %s%s \"%s\"", stuff, name, hook->nick);
	fprintf(fp, " %s\n", hook->stuff);
}

/*
 * save_hooks: for use by the SAVE command to write the hooks to a file so it
 * can be interpreted by the LOAD command 
 */
void
save_hooks(FILE *fp, int do_all)
{
	Hook	*list;
	NumericList *numeric;
	int	which;

	for (which = 0; which < NUMBER_OF_LISTS; which++)
	{
		for (list = hook_functions[which].list; list; list = list->next)
			if (!list->global || do_all)
				write_hook(fp,list, hook_functions[which].name);
	}
	for (numeric = numeric_list; numeric; numeric = numeric->next)
	{
		for (list = numeric->list; list; list = list->next)
			if (!list->global)
				write_hook(fp, list, numeric->name);
	}
}

int
in_on_who(void)
{
	return in_on_who_real;
}

int
set_in_on_who(int val)
{
	int curval = in_on_who_real;

	in_on_who_real = val;
	return curval;
}

int
current_dcc_hook(void)
{
	return current_hook;
}
