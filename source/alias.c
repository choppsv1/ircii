/*
 * alias.c Handles command aliases for irc.c 
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
IRCII_RCSID("@(#)$eterna: alias.c,v 1.171 2015/02/22 12:43:18 mrg Exp $");

#include "alias.h"
#include "dcc.h"
#include "status.h"
#include "edit.h"
#include "history.h"
#include "vars.h"
#include "ircaux.h"
#include "server.h"
#include "screen.h"
#include "window.h"
#include "input.h"
#include "names.h"
#include "server.h"
#include "output.h"
#include "names.h"
#include "parse.h"
#include "notify.h"
#include "ignore.h"
#include "exec.h"
#include "ircterm.h"
#include "numbers.h"

#include <sys/stat.h>

#ifdef HAVE_CRYPT_H
# include <crypt.h>
#endif

/* Alias: structure of each alias entry */
struct	alias_stru
{
	u_char	*name;		/* name of alias */
	u_char	*stuff;		/* what the alias is */
	int	mark;		/* used to prevent recursive aliasing */
	int	global;		/* set if loaded from `global' */
	Alias	*next;		/* pointer to next alias in list */
};

	int	parse_number(u_char **);
static	u_char	*next_unit(u_char *, u_char *, int *, int);
static	long	randm(long);
static	u_char	*lastop(u_char *);
static	u_char	*arg_number(int, int, u_char *);
static	Alias	*find_alias(Alias **, u_char *, int, int *);
static	void	insert_alias(Alias **, Alias *);
static	u_char	*alias_arg(u_char **, u_int *);
static	u_char	*find_inline(u_char *);
static	u_char	*built_in_alias(int);
static	void	expander_addition(u_char **, u_char *, int, u_char *);
static	u_char	*alias_special_char(u_char *, u_char **, u_char *, u_char *, u_char *, int *);

static	u_char	*alias_detected(void);
static	u_char	*alias_sent_nick(void);
static	u_char	*alias_recv_nick(void);
static	u_char	*alias_msg_body(void);
static	u_char	*alias_joined_nick(void);
static	u_char	*alias_public_nick(void);
static	u_char	*alias_dollar(void);
static	u_char	*alias_channel(void);
static	u_char	*alias_server(void);
static	u_char	*alias_query_nick(void);
static	u_char	*alias_target(void);
static	u_char	*alias_nick(void);
static	u_char	*alias_invite(void);
static	u_char	*alias_cmdchar(void);
static	u_char	*alias_line(void);
static	u_char	*alias_away(void);
static	u_char	*alias_oper(void);
static	u_char	*alias_chanop(void);
static	u_char	*alias_modes(void);
static	u_char	*alias_time(void);
static	u_char	*alias_version(void);
static	u_char	*alias_currdir(void);
static	u_char	*alias_current_numeric(void);
static	u_char	*alias_server_version(void);

typedef struct
{
	u_char	name;
	u_char	*(*func)(void);
}	BuiltIns;

static	BuiltIns built_in[] =
{
	{ '.',		alias_sent_nick },
	{ ',',		alias_recv_nick },
	{ ':',		alias_joined_nick },
	{ ';',		alias_public_nick },
	{ '$',		alias_dollar },
	{ 'A',		alias_away },
	{ 'B',		alias_msg_body },
	{ 'C',		alias_channel },
	{ 'D',		alias_detected },
/*
	{ 'E' },
	{ 'F' },
	{ 'G' },
*/
	{ 'H', 		alias_current_numeric },
	{ 'I',		alias_invite },
/*
	{ 'J' },
*/
	{ 'K',		alias_cmdchar },
	{ 'L',		alias_line },
	{ 'M',		alias_modes },
	{ 'N',		alias_nick },
	{ 'O',		alias_oper },
	{ 'P',		alias_chanop },
	{ 'Q',		alias_query_nick },
	{ 'R',		alias_server_version },
	{ 'S',		alias_server },
	{ 'T',		alias_target },
	{ 'U',		alias_buffer },
	{ 'V',		alias_version },
	{ 'W',		alias_currdir },
/*
	{ 'X' },
	{ 'Y' },
*/
	{ 'Z',		alias_time },
	{ (u_char) 0,	 NULL }
};

static	u_char	*function_left(u_char *);
static	u_char	*function_right(u_char *);
static	u_char	*function_mid(u_char *);
static	u_char	*function_rand(u_char *);
static	u_char	*function_srand(u_char *);
static	u_char	*function_time(u_char *);
static	u_char	*function_stime(u_char *);
static	u_char	*function_tdiff(u_char *);
static	u_char	*function_index(u_char *);
static	u_char	*function_rindex(u_char *);
static	u_char	*function_match(u_char *);
static	u_char	*function_rmatch(u_char *);
static	u_char	*function_userhost(u_char *);
static	u_char	*function_strip(u_char *);
static	u_char	*function_encode(u_char *);
static	u_char	*function_decode(u_char *);
static	u_char	*function_ischannel(u_char *);
static	u_char	*function_ischanop(u_char *);
#ifdef HAVE_CRYPT
static	u_char	*function_crypt(u_char *);
#endif /* HAVE_CRYPT */
static	u_char	*function_hasvoice(u_char *);
static	u_char	*function_dcclist(u_char *);
static	u_char	*function_chatpeers(u_char *);
static	u_char	*function_word(u_char *);
static	u_char	*function_querynick(u_char *);
static	u_char	*function_windows(u_char *);
static	u_char	*function_screens(u_char *);
static	u_char	*function_notify(u_char *);
static	u_char	*function_ignored(u_char *);
static	u_char	*function_winserver(u_char *);
static	u_char	*function_winservergroup(u_char *);
static	u_char	*function_winvisible(u_char *);
static	u_char	*function_winnum(u_char *);
static	u_char	*function_winnam(u_char *);
static	u_char	*function_winrows(u_char *);
static	u_char	*function_wincols(u_char *);
static	u_char	*function_connect(u_char *);
static	u_char	*function_listen(u_char *);
static	u_char	*function_toupper(u_char *);
static	u_char	*function_tolower(u_char *);
static	u_char	*function_channels(u_char *);
static	u_char	*function_servers(u_char *);
static	u_char	*function_servertype(u_char *);
static	u_char	*function_onchannel(u_char *);
static	u_char	*function_pid(u_char *);
static	u_char	*function_ppid(u_char *);
static	u_char	*function_idle(u_char *);
#ifdef HAVE_STRFTIME
static	u_char	*function_strftime(u_char *);
#endif /* HAVE_STRFTIME */
static	u_char	*function_urlencode(u_char *);
static	u_char	*function_shellfix(u_char *);
static	u_char	*function_filestat(u_char *);

typedef struct
{
	u_char	*name;
	u_char	*(*func)(u_char *);
}	BuiltInFunctions;

static BuiltInFunctions	built_in_functions[] =
{
	{ UP("LEFT"),		function_left },
	{ UP("RIGHT"),		function_right },
	{ UP("MID"),		function_mid },
	{ UP("RAND"),		function_rand },
	{ UP("SRAND"),		function_srand },
	{ UP("TIME"),		function_time },
	{ UP("TDIFF"),		function_tdiff },
	{ UP("STIME"),		function_stime },
	{ UP("INDEX"),		function_index },
	{ UP("RINDEX"),		function_rindex },
	{ UP("MATCH"),		function_match },
	{ UP("RMATCH"),		function_rmatch },
	{ UP("USERHOST"),	function_userhost },
	{ UP("STRIP"),		function_strip },
	{ UP("ENCODE"),		function_encode },
	{ UP("DECODE"),		function_decode },
	{ UP("ISCHANNEL"),	function_ischannel },
	{ UP("ISCHANOP"),	function_ischanop },
#ifdef HAVE_CRYPT
	{ UP("CRYPT"),		function_crypt },
#endif /* HAVE_CRYPT */
	{ UP("HASVOICE"),	function_hasvoice },
	{ UP("DCCLIST"),	function_dcclist },
	{ UP("CHATPEERS"),	function_chatpeers },
	{ UP("WORD"),		function_word },
	{ UP("WINNUM"),		function_winnum },
	{ UP("WINNAM"),		function_winnam },
	{ UP("WINVIS"),		function_winvisible },
	{ UP("WINROWS"),	function_winrows },
	{ UP("WINCOLS"),	function_wincols },
	{ UP("WINSERVER"),	function_winserver },
	{ UP("WINSERVERGROUP"),	function_winservergroup },
	{ UP("CONNECT"),	function_connect },
	{ UP("LISTEN"),		function_listen },
	{ UP("TOUPPER"),	function_toupper },
	{ UP("TOLOWER"),	function_tolower },
	{ UP("MYCHANNELS"),	function_channels },
	{ UP("MYSERVERS"),	function_servers },
	{ UP("SERVERTYPE"),	function_servertype },
	{ UP("CURPOS"),		function_curpos },
	{ UP("ONCHANNEL"),	function_onchannel },
	{ UP("PID"),		function_pid },
	{ UP("PPID"),		function_ppid },
	{ UP("CHANUSERS"),	function_chanusers },
#ifdef HAVE_STRFTIME
	{ UP("STRFTIME"),	function_strftime },
#endif /* HAVE_STRFTIME */
	{ UP("IDLE"),		function_idle },
	{ UP("QUERYNICK"),	function_querynick },
	{ UP("WINDOWS"),	function_windows },
	{ UP("SCREENS"),	function_screens },
	{ UP("NOTIFY"),		function_notify },
	{ UP("IGNORED"),	function_ignored },
	{ UP("URLENCODE"),	function_urlencode },
	{ UP("SHELLFIX"),	function_shellfix },
	{ UP("FILESTAT"),	function_filestat },
	{ UP(0),		NULL }
};

/* alias_illegals: characters that are illegal in alias names */
	u_char	alias_illegals[] = " #+-*/\\()={}[]<>!@$%^~`,?;:|'\"";

static	Alias	*alias_list[] =
{
	NULL,
	NULL
};

static	int	eval_args;

/* function_stack and function_stkptr - hold the return values from functions */
static	u_char	* function_stack[128] =
{ 
	NULL
};
static	int	function_stkptr = 0;

/*
 * find_alias: looks up name in in alias list.  Returns the Alias	entry if
 * found, or null if not found.   If do_unlink is set, the found entry is
 * removed from the list as well.  If match is null, only perfect matches
 * will return anything.  Otherwise, the number of matches will be returned. 
 */
static	Alias	*
find_alias(Alias **list, u_char *name, int do_unlink, int *match)
{
	Alias	*tmp,
		*last = NULL;
	int	cmp;
	size_t	len;
	int	(*cmp_func)(const u_char *, const u_char *, size_t);

	if (match)
	{
		*match = 0;
		cmp_func = my_strnicmp;
	}
	else
		cmp_func = (int (*)(const u_char *, const u_char *, size_t)) my_stricmp;
	if (name)
	{
		len = my_strlen(name);
		for (tmp = *list; tmp; tmp = tmp->next)
		{
			if ((cmp = cmp_func(name, tmp->name, len)) == 0)
			{
				if (do_unlink)
				{
					if (last)
						last->next = tmp->next;
					else
						*list = tmp->next;
				}
				if (match)
				{
					(*match)++;
					if (my_strlen(tmp->name) == len)
					{
						*match = 0;
						return (tmp);
					}
				}
				else
					return (tmp);
			}
			else if (cmp < 0)
				break;
			last = tmp;
		}
	}
	if (match && (*match == 1))
		return (last);
	else
		return (NULL);
}

/*
 * insert_alias: adds the given alias to the alias list.  The alias list is
 * alphabetized by name 
 */
static	void	
insert_alias(Alias **list, Alias *nalias)
{
	Alias	*tmp,
		*last,
		*foo;

	last = NULL;
	for (tmp = *list; tmp; tmp = tmp->next)
	{
		if (my_strcmp(nalias->name, tmp->name) < 0)
			break;
		last = tmp;
	}
	if (last)
	{
		foo = last->next;
		last->next = nalias;
		nalias->next = foo;
	}
	else
	{
		nalias->next = *list;
		*list = nalias;
	}
}

/*
 * add_alias: given the alias name and "stuff" that makes up the alias,
 * add_alias() first checks to see if the alias name is already in use... if
 * so, the old alias is replaced with the new one.  If the alias is not
 * already in use, it is added. 
 */
void	
add_alias(int type, u_char *name, u_char *stuff)
{
	Alias	*tmp;
	u_char	*ptr;

	upper(name);
	if (type == COMMAND_ALIAS)
		say("Alias	%s added", name);
	else
	{
		if (!my_strcmp(name, "FUNCTION_RETURN"))
		{
			if (function_stack[function_stkptr])
				new_free(&function_stack[function_stkptr]);
			malloc_strcpy(&function_stack[function_stkptr], stuff);
			return;
		}
		if ((ptr = sindex(name, alias_illegals)) != NULL)
		{
			yell("Assign names may not contain '%c'", *ptr);
			return;
		}
		say("Assign %s added", name);
	}
	if ((tmp = find_alias(&(alias_list[type]), name, 1, NULL)) ==
			NULL)
	{
		tmp = new_malloc(sizeof *tmp);
		if (tmp == NULL)
		{
			yell("Couldn't allocate memory for new alias!");
			return;
		}
		tmp->name = NULL;
		tmp->stuff = NULL;
	}
	malloc_strcpy(&(tmp->name), name);
	malloc_strcpy(&(tmp->stuff), stuff);
	tmp->mark = 0;
	tmp->global = loading_global();
	insert_alias(&(alias_list[type]), tmp);
}

/* alias_arg: a special version of next_arg for aliases */
static	u_char	*
alias_arg(u_char **str, u_int *pos)
{
	u_char	*ptr;

	if (!*str)
		return NULL;
	*pos = 0;
	ptr = *str;
	while (' ' == *ptr)
	{
		ptr++;
		(*pos)++;
	}
	if (*ptr == '\0')
	{
		*str = empty_string();
		return (NULL);
	}
	if ((*str = sindex(ptr, UP(" "))) != NULL)
		*((*str)++) = '\0';
	else
		*str = empty_string();
	return (ptr);
}

/* word_count: returns the number of words in the given string */
int	
word_count(u_char *str)
{
	int	cnt = 0;
	u_char	*ptr;

	while (1)
	{
		if ((ptr = sindex(str, UP("^ "))) != NULL)
		{
			cnt++;
			if ((str = sindex(ptr, UP(" "))) == NULL)
				return (cnt);
		}
		else
			return (cnt);
	}
}

static	u_char	*
built_in_alias(int c)
{
	BuiltIns	*tmp;
	u_char	*ret = NULL;

	for (tmp = built_in; tmp->name; tmp++)
		if ((u_char)c == tmp->name)
		{
			malloc_strcpy(&ret, tmp->func());
			break;
		}
	return (ret);
}

/*
 * find_inline: This simply looks up the given str.  It first checks to see
 * if its a user variable and returns it if so.  If not, it checks to see if
 * it's an IRC variable and returns it if so.  If not, it checks to see if
 * its and environment variable and returns it if so.  If not, it returns
 * null.  It mallocs the returned string 
 */
static	u_char	*
find_inline(u_char *str)
{
	Alias	*nalias;
	u_char	*ret = NULL;
	u_char	*tmp;

	if ((nalias = find_alias(&(alias_list[VAR_ALIAS]), str, 0, (int *) NULL))
			!= NULL)
	{
		malloc_strcpy(&ret, nalias->stuff);
		return (ret);
	}
	if ((my_strlen(str) == 1) && (ret = built_in_alias(*str)))
		return(ret);
	if ((ret = make_string_var(str)) != NULL)
		return (ret);
#ifdef DAEMON_UID
	if (getuid() == DAEMON_UID)
	malloc_strcpy(&ret, (getuid() != DAEMON_UID) && (tmp = my_getenv(str)) ?
				tmp : empty_string());
#else
	malloc_strcpy(&ret, (tmp = my_getenv(str)) ? tmp : empty_string());
#endif /* DAEMON_UID */
	return (ret);
}

u_char	*
call_function(u_char *name, u_char *f_args, u_char *args, int *args_flag)
{
	u_char	*tmp;
	u_char	*result = NULL;
	u_char	*sub_buffer = NULL;
	int	builtnum;
	u_char	*debug_copy = NULL;
	u_char	*cmd = NULL;

	malloc_strcpy(&cmd, name);
	upper(cmd);
	tmp = expand_alias(NULL, f_args, args, args_flag, NULL);
	if (get_int_var(DEBUG_VAR) & DEBUG_FUNCTIONS)
		malloc_strcpy(&debug_copy, tmp);
	for (builtnum = 0; built_in_functions[builtnum].name != NULL &&
			my_strcmp(built_in_functions[builtnum].name, cmd);
			builtnum++)
		;
	new_free(&cmd);
	if (built_in_functions[builtnum].name)
		result = built_in_functions[builtnum].func(tmp);
	else
	{
		sub_buffer = new_malloc(my_strlen(name)+my_strlen(tmp)+2);
		my_strcpy(sub_buffer, name);
		my_strcat(sub_buffer, " ");
		my_strcat(sub_buffer, tmp);
		function_stack[++function_stkptr] = NULL;
		parse_command(sub_buffer, 0, empty_string());
		new_free(&sub_buffer);
		eval_args=1;
		result = function_stack[function_stkptr];
		function_stack[function_stkptr] = NULL;
		if (!result)
			malloc_strcpy(&result, empty_string());
		function_stkptr--;
	}
	if (debug_copy)
	{
		yell("Function %s(%s) returned %s",
		    name, debug_copy, result);
		new_free(&debug_copy);
	}
	new_free(&tmp);
	return (result);
}


/* Given a pointer to an operator, find the last operator in the string */
static	u_char	*
lastop(u_char *ptr)
{
	while (ptr[1] && my_index("!=<>&^|#+/-*", ptr[1]))
		ptr++;
	return ptr;
}

#define	NU_EXPR	0
#define	NU_CONJ NU_EXPR
#define	NU_ASSN	1
#define	NU_COMP 2
#define	NU_ADD  3
#define	NU_MULT 4
#define	NU_UNIT 5
#define	NU_TERT 6
#define	NU_BITW 8

static	u_char	*
next_unit(u_char *str, u_char *args, int *arg_flag, int stage)
{
	u_char	*ptr,
		*ptr2,
		*right;
	int	got_sloshed = 0;
	u_char	*lastc;
	u_char	tmp[40];
	u_char	*result1 = NULL,
		*result2 = NULL;
	long	value1 = 0,
		value2,
		value3;
	u_char	op;
	int	display;
	u_char	*ArrayIndex,
		*EndIndex;

	while (isspace(*str))
		++str;
	if (!*str)
	{
		malloc_strcpy(&result1, empty_string());
		return result1;
	}
	lastc = str+my_strlen(str)-1;
	while (isspace(*lastc))
		*lastc-- = '\0';
	if (stage == NU_UNIT && *lastc == ')' && *str == '(')
	{
		str++, *lastc-- = '\0';
		return next_unit(str, args, arg_flag, NU_EXPR);
	}
	if (!*str)
	{
		malloc_strcpy(&result1, empty_string());
		return result1;
	}
	for (ptr = str; *ptr; ptr++)
	{
		if (got_sloshed) /* Help! I'm drunk! */
		{
			got_sloshed = 0;
			continue;
		}
		switch(*ptr)
		{
		case '\\':
			got_sloshed = 1;
			continue;
		case '(':
			if (stage != NU_UNIT || ptr == str)
			{
				if (!(ptr2 = MatchingBracket(ptr+1, (int)'(', (int)')')))
					ptr = ptr+my_strlen(ptr)-1;
				else
					ptr = ptr2;
				break;
			}
			*ptr++ = '\0';
			right = ptr;
			ptr = MatchingBracket(right, (int)LEFT_PAREN, (int)RIGHT_PAREN);
			if (ptr)
				*ptr++ = '\0';
			result1 = call_function(str, right, args, arg_flag);
			if (ptr && *ptr)
			{
				malloc_strcat(&result1, ptr);
				result2 = next_unit(result1, args, arg_flag,
						stage);
				new_free(&result1);
				result1 = result2;
			}
			return result1;
		case '[':
			if (stage != NU_UNIT)
			{
				if (!(ptr2 = MatchingBracket(ptr+1, (int)'[', (int)']')))
					ptr = ptr+my_strlen(ptr)-1;
				else
					ptr = ptr2;
				break;
			}
			*ptr++ = '\0';
			right = ptr;
			ptr = MatchingBracket(right, (int)LEFT_BRACKET, (int)RIGHT_BRACKET);
			if (ptr)
				*ptr++ = '\0';
			result1 = expand_alias(NULL, right, args, arg_flag, NULL);
			if (*str)
			{
				result2 = new_malloc(my_strlen(str)+
						(result1?my_strlen(result1):0)+
						(ptr?my_strlen(ptr):0) + 2);
				my_strcpy(result2, str);
				my_strcat(result2, ".");
				my_strcat(result2, result1);
				new_free(&result1);
				if (ptr && *ptr)
				{
					my_strcat(result2, ptr);
					result1 = next_unit(result2, args,
						arg_flag, stage);
				}
				else
				{
					result1 = find_inline(result2);
					if (!result1)
						malloc_strcpy(&result1,
							empty_string());
				}
				new_free(&result2);
			}
			else if (ptr && *ptr)
			{
				malloc_strcat(&result1, ptr);
				result2 = next_unit(result1, args, arg_flag,
					stage);
				new_free(&result1);
				result1 = result2;
			}
			return result1;
		case '-':
		case '+':
			if (*(ptr+1) == *(ptr))  /* index operator */
			{
				u_char *tptr;
				int r;

				*ptr++ = '\0';
				if (ptr == str + 1)	/* Its a prefix */
					tptr = str + 2;
				else			/* Its a postfix */
					tptr = str;
			    	result1 = find_inline(tptr);
				if (!result1) 
					malloc_strcpy(&result1, zero());

				r = my_atoi(result1);
				if (*ptr == '+')
					r++;
				else
					r--;
				snprintf(CP(tmp), sizeof tmp, "%d", r);
				display = set_display_off();
				add_alias(VAR_ALIAS, tptr, tmp);
				set_display(display);
				/* A kludge?  Cheating?  Maybe.... */
				if (ptr == str + 1) 
				{
					*(ptr-1) = ' ';
					*ptr = ' ';
				}
				else
				{
					if (*ptr == '+')
						*(ptr-1) = '-';
					else
						*(ptr-1) = '+';
					*ptr = '1';
				}
				ptr = str; 
				new_free(&result1);
				break;
			}
			if (ptr == str) /* It's unary..... do nothing */
				break;
			if (stage != NU_ADD)
			{
				ptr = lastop(ptr);
				break;
			}
			op = *ptr;
			*ptr++ = '\0';
			result1 = next_unit(str, args, arg_flag, stage);
			result2 = next_unit(ptr, args, arg_flag, stage);
			value1 = my_atol(result1);
			value2 = my_atol(result2);
			new_free(&result1);
			new_free(&result2);
			if (op == '-')
				value3 = value1 - value2;
			else
				value3 = value1 + value2;
			snprintf(CP(tmp), sizeof tmp, "%ld", value3);
			malloc_strcpy(&result1, tmp);
			return result1;
		case '/':
		case '*':
  		case '%':
			if (stage != NU_MULT)
			{
				ptr = lastop(ptr);
				break;
			}
			op = *ptr;
			*ptr++ = '\0';
			result1 = next_unit(str, args, arg_flag, stage);
			result2 = next_unit(ptr, args, arg_flag, stage);
			value1 = my_atol(result1);
			value2 = my_atol(result2);
			new_free(&result1);
			new_free(&result2);
			if (op == '/')
			{
				if (value2)
					value3 = value1 / value2;
				else
				{
					value3 = 0;
					say("Division by zero");
				}
			}
			else
			if (op == '*')
				value3 = value1 * value2;
			else
			{
				if (value2)
				    value3 = value1 % value2;
				else
				{
					value3 = 0;
					say("Mod by zero");
				}
			}
			snprintf(CP(tmp), sizeof tmp, "%ld", value3);
			malloc_strcpy(&result1, tmp);
			return result1;
		case '#':
			if (stage != NU_ADD || ptr[1] != '#')
			{
				ptr = lastop(ptr);
				break;
			}
			*ptr = '\0';
			ptr += 2;
			result1 = next_unit(str, args, arg_flag, stage);
			result2 = next_unit(ptr, args, arg_flag, stage);
			malloc_strcat(&result1, result2);
			new_free(&result2);
			return result1;
	/* Reworked - Jeremy Nelson, Feb 1994
	 * & or && should both be supported, each with different
	 * stages, same with || and ^^.  Also, they should be
	 * short-circuit as well.
	 */
		case '&':
			if (ptr[0] == ptr[1])
			{
				if (stage != NU_CONJ)
				{
					ptr = lastop(ptr);
					break;
				}
				*ptr = '\0';
				ptr += 2;
				result1 = next_unit(str, args, arg_flag, stage);
				value1 = my_atol(result1);
				if (value1)
				{
					result2 = next_unit(ptr, args, arg_flag, stage);
					value2 = my_atol(result2);
					value3 = value1 && value2;
				}
				else
					value3 = 0;
				new_free(&result1);
				new_free(&result2);
				tmp[0] = '0' + (value3?1:0);
				tmp[1] = '\0';
				malloc_strcpy(&result1, tmp);
 				return result1;
			}
			else
			{
				if (stage != NU_BITW)
				{
					ptr = lastop(ptr);
					break;
				}
				*ptr = '\0';
				ptr += 1;
				result1 = next_unit(str, args, arg_flag, stage);
				result2 = next_unit(ptr, args, arg_flag, stage);
				value1 = my_atol(result1);
				value2 = my_atol(result2);
				new_free(&result1);
				new_free(&result2);
				value3 = value1 & value2;
				snprintf(CP(tmp), sizeof tmp, "%ld", value3);
				malloc_strcpy(&result1, tmp);
 				return result1;
			}
		case '|':
			if (ptr[0] == ptr[1])
			{
				if (stage != NU_CONJ)
				{
					ptr = lastop(ptr);
					break;
				}
				*ptr = '\0';
				ptr += 2;
				result1 = next_unit(str, args, arg_flag, stage);
				value1 = my_atol(result1);
				if (!value1)
				{
					result2 = next_unit(ptr, args, arg_flag, stage);
					value2 = my_atol(result2);
					value3 = value1 | value2;
				}
				else	
					value3 = 1;
				new_free(&result1);
				new_free(&result2);
				tmp[0] = '0' + (value3 ? 1 : 0);
				tmp[1] = '\0';
				malloc_strcpy(&result1, tmp);
 				return result1;
			}
			else
			{
				if (stage != NU_BITW)
				{
					ptr = lastop(ptr);
					break;
				}
				*ptr = '\0';
				ptr += 1;
				result1 = next_unit(str, args, arg_flag, stage);
				result2 = next_unit(ptr, args, arg_flag, stage);
				value1 = my_atol(result1);
				value2 = my_atol(result2);
				new_free(&result1);
				new_free(&result2);
				value3 = value1 | value2;
				snprintf(CP(tmp), sizeof tmp, "%ld", value3);
				malloc_strcpy(&result1, tmp);
 				return result1;
			}
		case '^':
			if (ptr[0] == ptr[1])
			{
				if (stage != NU_CONJ)
				{
					ptr = lastop(ptr);
					break;
				}
				*ptr = '\0';
				ptr += 2;
				result1 = next_unit(str, args, arg_flag, stage);
				result2 = next_unit(ptr, args, arg_flag, stage);
				value1 = my_atol(result1);
				value2 = my_atol(result2);
				value1 = value1?1:0;
				value2 = value2?1:0;
				value3 = value1 ^ value2;
				new_free(&result1);
				new_free(&result2);
				tmp[0] = '0' + (value3 ? 1 : 0);
				tmp[1] = '\0';
				malloc_strcpy(&result1, tmp);
 				return result1;
			}
			else
			{
				if (stage != NU_BITW)
				{
					ptr = lastop(ptr);
					break;
				}
				*ptr = '\0';
				ptr += 1;
				result1 = next_unit(str, args, arg_flag, stage);
				result2 = next_unit(ptr, args, arg_flag, stage);
				value1 = my_atol(result1);
				value2 = my_atol(result2);
				new_free(&result1);
				new_free(&result2);
				value3 = value1 ^ value2;
				snprintf(CP(tmp), sizeof tmp, "%ld", value3);
				malloc_strcpy(&result1, tmp);
 				return result1;
			}
			case '?':
			if (stage != NU_TERT)
			{
				ptr = lastop(ptr);
				break;
			}
			*ptr++ = '\0';
			result1 = next_unit(str, args, arg_flag, stage);
			ptr2 = my_index(ptr, ':');
			*ptr2++ = '\0';
			right = result1;
			value1 = parse_number(&right);
			if (value1 == -1 && *right == '\0')
				value1 = 0;
			if (value1 == 0)
				while (isspace(*right))
					*(right++) = '\0';
			if (value1 || *right)
				result2 = next_unit(ptr, args, arg_flag, stage);
			else
				result2 = next_unit(ptr2, args, arg_flag, stage);
			*(ptr2-1) = ':';
			new_free(&result1);
			return result2;
		case '=':
			if (ptr[1] != '=')
			{
				if (stage != NU_ASSN)
				{
					ptr = lastop(ptr);
					break;
				}
				*ptr++ = '\0';
				result1 = expand_alias(NULL, str,
					args, arg_flag, NULL);
				result2 = next_unit(ptr, args, arg_flag, stage);
				display = set_display_off();
				lastc = result1 + my_strlen(result1) - 1;
				while (lastc > result1 && *lastc == ' ')
					*lastc-- = '\0';
				for (ptr = result1; *ptr == ' '; ptr++);
				while ((ArrayIndex = my_index(ptr, '['))
						!= NULL)
				{
					*ArrayIndex++='.';
					if ((EndIndex = MatchingBracket(
					    ArrayIndex, (int)LEFT_BRACKET,
					    (int)RIGHT_BRACKET)) != NULL)
					{
						*EndIndex++='\0';
						my_strcat(ptr, EndIndex);
					}
					else
						break;
				}
				if (*ptr)
					add_alias(VAR_ALIAS, ptr, result2);
				else
					yell("Invalid assignment expression");
				set_display(display);
				new_free(&result1);
				return result2;
			}
			if (stage != NU_COMP)
			{
				ptr = lastop(ptr);
				break;
			}
			*ptr = '\0';
			ptr += 2;
			result1 = next_unit(str, args, arg_flag, stage);
			result2 = next_unit(ptr, args, arg_flag, stage);
			if (!my_stricmp(result1, result2))
				malloc_strcpy(&result1, one());
			else
				malloc_strcpy(&result1, zero());
			new_free(&result2);
			return result1;
		case '>':
		case '<':
			if (stage != NU_COMP)
			{
				ptr = lastop(ptr);
				break;
			}
			op = *ptr;
			if (ptr[1] == '=')
				value3 = 1, *ptr++ = '\0';
			else
				value3 = 0;
			*ptr++ = '\0';
			result1 = next_unit(str, args, arg_flag, stage);
			result2 = next_unit(ptr, args, arg_flag, stage);
			if (isdigit(*result1) && isdigit(*result2))
			{
				value1 = my_atol(result1);
				value2 = my_atol(result2);
				value1 = (value1 == value2) ? 0 : ((value1 <
					value2) ? -1 : 1);
			}
			else
				value1 = my_stricmp(result1, result2);
			if (value1)
			{
				value2 = (value1 > 0) ? 1 : 0;
				if (op == '<')
					value2 = 1 - value2;
			}
			else
				value2 = value3;
			new_free(&result2);
			snprintf(CP(tmp), sizeof tmp, "%ld", value2);
			malloc_strcpy(&result1, tmp);
			return result1;
		case '~':
			if (ptr == str)
			{
				if (stage != NU_BITW)
					break;
				result1 = next_unit(str+1, args, arg_flag,
					stage);
				if (isdigit(*result1))
				{
					value1 = my_atol(result1);
					value2 = ~value1;
				}
				else
					value2 = 0;
				snprintf(CP(tmp), sizeof tmp, "%ld", value2);
				malloc_strcpy(&result1, tmp);
				return result1;
			}
			else
			{
				ptr = lastop(ptr);
				break;
			}
		case '!':
			if (ptr == str)
			{
				if (stage != NU_UNIT)
					break;
				result1 = next_unit(str+1, args, arg_flag,
					stage);
				if (isdigit(*result1))
				{
					value1 = my_atol(result1);
					value2 = value1 ? 0 : 1;
				}
				else
				{
					value2 = ((*result1)?0:1);
				}
				snprintf(CP(tmp), sizeof tmp, "%ld", value2);
				malloc_strcpy(&result1, tmp);
				return result1;
			}
			if (stage != NU_COMP || ptr[1] != '=')
			{
				ptr = lastop(ptr);
				break;
			}
			*ptr = '\0';
			ptr += 2;
			result1 = next_unit(str, args, arg_flag, stage);
			result2 = next_unit(ptr, args, arg_flag, stage);
			if (!my_stricmp(result1, result2))
				malloc_strcpy(&result1, zero());
			else
				malloc_strcpy(&result1, one());
			new_free(&result2);
			return result1;
			case ',': 
			/*
			 * this utterly kludge code is needed (?) to get
			 * around bugs introduced from hop's patches to
			 * alias.c.  the $, variable stopped working
			 * because of this.  -mrg, july 94.
			 */
			if (ptr == str || (ptr > str && ptr[-1] == '$'))
				break;
			if (stage != NU_EXPR)
			{
				ptr = lastop(ptr);
				break;
			}
			*ptr++ = '\0';
			result1 = next_unit(str, args, arg_flag, stage);
			result2 = next_unit(ptr, args, arg_flag, stage);
			new_free(&result1);
			return result2;
		}
	}
	if (stage != NU_UNIT)
		return next_unit(str, args, arg_flag, stage+1);
	if (isdigit(*str) || *str == '+' || *str == '-')
		malloc_strcpy(&result1, str);
	else
	{
		if (*str == '#' || *str=='@')
			op = *str++;
		else
			op = '\0';
		result1 = find_inline(str);
		if (!result1)
			malloc_strcpy(&result1, empty_string());
		if (op)
		{
			if (op == '#')
				value1 = word_count(result1);
			else if (op == '@')
				value1 = my_strlen(result1);
			snprintf(CP(tmp), sizeof tmp, "%ld", value1);
			malloc_strcpy(&result1, tmp);
		}
	}
	return result1;
}

/*
 * parse_inline:  This evaluates user-variable expression.  I'll talk more
 * about this at some future date. The ^ function and some fixes by
 * troy@cbme.unsw.EDU.AU (Troy Rollo) 
 */
u_char	*
parse_inline(u_char *str, u_char *args, int *args_flag)
{
	return next_unit(str, args, args_flag, NU_EXPR);
}

/*
 * arg_number: Returns the argument 'num' from 'str', or, if 'num' is
 * negative, returns from argument 'num' to the end of 'str'.  You might be
 * wondering what's going on down there... here goes.  First we copy 'str' to
 * malloced space.  Then, using next_arg(), we strip out each argument ,
 * putting them in arg_list, and putting their position in the original
 * string in arg_list_pos.  Anyway, once parsing is done, the arguments are
 * returned directly from the arg_list array... or in the case of negative
 * 'num', the arg_list_pos is used to return the postion of the rest of the
 * args in the original string... got it?  Anyway, the bad points of the
 * routine:  1) Always parses out everything, even if only one arg is used.
 * 2) The malloced stuff remains around until arg_number is called with a
 * different string. Even then, some malloced stuff remains around.  This can
 * be fixed. 
 */

#define	LAST_ARG 8000

static u_char *
arg_number(int lower_lim, int upper_lim, u_char *str)
{
	u_char	*ptr,
		*arg,
		c;
	int	use_full = 0;
	unsigned int	pos,
		start_pos;
	static	u_char	*last_args = NULL;
	static	u_char	*last_range = NULL;
	static	u_char	**arg_list = NULL;
	static	unsigned int	*arg_list_pos = NULL;
	static	unsigned int	*arg_list_end_pos = NULL;
	static	int	arg_list_size;

	if (eval_args)
	{
		int	arg_list_limit;

		eval_args = 0;
		new_free(&arg_list);
		new_free(&arg_list_pos);
		new_free(&arg_list_end_pos);
		arg_list_size = 0;
		arg_list_limit = 10;
		arg_list = new_malloc(sizeof(*arg_list) * arg_list_limit);
		arg_list_pos = new_malloc(sizeof(*arg_list_pos) * arg_list_limit);
		arg_list_end_pos = new_malloc(sizeof(*arg_list_end_pos) * arg_list_limit);
		malloc_strcpy(&last_args, str);
		ptr = last_args;
		pos = 0;
		while ((arg = alias_arg(&ptr, &start_pos)) != NULL)
		{
			arg_list_pos[arg_list_size] = pos;
			pos += start_pos + my_strlen(arg);
			arg_list_end_pos[arg_list_size] = pos++;
			arg_list[arg_list_size++] = arg;
			if (arg_list_size == arg_list_limit)
			{
				arg_list_limit += 10;
				arg_list = new_realloc(arg_list, sizeof(*arg_list) * arg_list_limit);
				arg_list_pos = new_realloc(arg_list_pos, sizeof(*arg_list_pos) * arg_list_limit);
				arg_list_end_pos = new_realloc(arg_list_end_pos, sizeof(*arg_list_end_pos) * arg_list_limit);
			}
		}
	}
	if (upper_lim == LAST_ARG && lower_lim == LAST_ARG)
		upper_lim = lower_lim = arg_list_size - 1;
	if (arg_list_size == 0)
		return (empty_string());
	if ((upper_lim >= arg_list_size) || (upper_lim < 0))
	{
		use_full = 1;
		upper_lim = arg_list_size - 1;
	}
	if (upper_lim < lower_lim)
		return (empty_string());
	if (lower_lim >= arg_list_size)
		lower_lim = arg_list_size - 1;
	else if (lower_lim < 0)
		lower_lim = 0;
	if ((use_full == 0) && (lower_lim == upper_lim))
		return (arg_list[lower_lim]);
	c = *(str + arg_list_end_pos[upper_lim]);
	*(str + arg_list_end_pos[upper_lim]) = (u_char) 0;
	malloc_strcpy(&last_range, str + arg_list_pos[lower_lim]);
	*(str + arg_list_end_pos[upper_lim]) = c;
	return (last_range);
}

/*
 * parse_number: returns the next number found in a string and moves the
 * string pointer beyond that point	in the string.  Here's some examples: 
 *
 * "123harhar"  returns 123 and str as "harhar" 
 *
 * while: 
 *
 * "hoohar"     returns -1  and str as "hoohar" 
 */
int	
parse_number(u_char **str)
{
	int	ret;
	u_char	*ptr;

	ptr = *str;
	if (isdigit(*ptr))
	{
		ret = my_atoi(ptr);
		for (; isdigit(*ptr); ptr++);
		*str = ptr;
	}
	else
		ret = -1;
	return (ret);
}

/*
 * expander_addition: This handles string width formatting for irc variables
 * when [] is specified.  
 */
static	void	
expander_addition(u_char **buff, u_char *add, int length, u_char *quote_em)
{
	u_char	format[40],
		buffer[BIG_BUFFER_SIZE],
		*ptr;

	if (length)
	{
		snprintf(CP(format), sizeof format, "%%%d.%ds", -length, (length < 0 ? -length :
			length));
		snprintf(CP(buffer), sizeof buffer, CP(format), add);
		add = buffer;
	}
	if (quote_em)
	{
		ptr = double_quote(add, quote_em);
		malloc_strcat(buff, ptr);
		new_free(&ptr);
	}
	else if (add && *add)
		malloc_strcat(buff, add);
}

/* MatchingBracket returns the next unescaped bracket of the given type */
u_char	*
MatchingBracket(u_char *string, int left, int right)
{
	int	bracket_count = 1;

	while (*string && bracket_count)
	{
		if (*string == (u_char)left)
			bracket_count++;
		else if (*string == (u_char)right)
		{
			if (!--bracket_count)
				return string;
		}
		else if (*string == '\\' && string[1])
			string++;
		string++;
	}
	return NULL;
}


/*
 * alias_special_char: Here we determin what to do with the character after
 * the $ in a line of text. The special characters are described more fulling
 * in the help/ALIAS file.  But they are all handled here. Paremeters are the
 * name of the alias (if applicable) to prevent deadly recursion, a
 * destination buffer (that we are malloc_str*ing) to which things are appended,
 * a ptr to the string (the first character of which is the special
 * character, the args to the alias, and a character indication what
 * characters in the string should be quoted with a backslash).  It returns a
 * pointer to the character right after the converted alias.

 The args_flag is set to 1 if any of the $n, $n-, $n-m, $-m, $*, or $() is used
 in the alias.  Otherwise it is left unchanged.
 */
static	u_char	*
alias_special_char(u_char *name, u_char **lbuf, u_char *ptr, u_char *args, u_char *quote_em, int *args_flag)
{
	u_char	*tmp,
		c;
	int	is_upper,
		is_lower,
		length;

	length = 0;
	if ((c = *ptr) == LEFT_BRACKET)
	{
		ptr++;
		if ((tmp = my_index(ptr, RIGHT_BRACKET)) != NULL)
		{
			*(tmp++) = (u_char) 0;
			length = my_atoi(ptr);
			ptr = tmp;
			c = *ptr;
		}
		else
		{
			say("Missing %c", RIGHT_BRACKET);
			return (ptr);
		}
	}
	tmp = ptr+1;
	switch (c)
	{
	case LEFT_PAREN:
		{
			u_char	*sub_buffer = NULL;

			if ((ptr = MatchingBracket(tmp, (int)LEFT_PAREN,
			    (int)RIGHT_PAREN)) || (ptr = my_index(tmp,
			    RIGHT_PAREN)))
				*(ptr++) = (u_char) 0;
			tmp = expand_alias(NULL, tmp, args, args_flag,
				NULL);
			malloc_strcpy(&sub_buffer, empty_string());
			alias_special_char(NULL, &sub_buffer, tmp,
				args, quote_em, args_flag);
			expander_addition(lbuf, sub_buffer, length, quote_em);
			new_free(&sub_buffer);
			new_free(&tmp);
			*args_flag = 1;
		}
		return (ptr);
	case '!':
		if ((ptr = my_index(tmp, '!')) != NULL)
			*(ptr++) = (u_char) 0;
		if ((tmp = do_history(tmp, empty_string())) != NULL)
		{
			expander_addition(lbuf, tmp, length, quote_em);
			new_free(&tmp);
		}
		return (ptr);
	case LEFT_BRACE:
		if ((ptr = my_index(tmp, RIGHT_BRACE)) != NULL)
			*(ptr++) = (u_char) 0;
		if ((tmp = parse_inline(tmp, args, args_flag)) != NULL)
		{
			expander_addition(lbuf, tmp, length, quote_em);
			new_free(&tmp);
		}
		return (ptr);
	case '*':
		expander_addition(lbuf, args, length, quote_em);
		*args_flag = 1;
		return (ptr + 1);
	default:
		if (isdigit(c) || (c == '-') || c == '~')
		{
			*args_flag = 1;
			if (*ptr == '~')
			{
				is_lower = is_upper = LAST_ARG;
				ptr++;
			}
			else
			{
				is_lower = parse_number(&ptr);
				if (*ptr == '-')
				{
					ptr++;
					is_upper = parse_number(&ptr);
				}
				else
					is_upper = is_lower;
			}
			expander_addition(lbuf, arg_number(is_lower, is_upper,
				args), length, quote_em);
			return (ptr ? ptr : empty_string());
		}
		else
		{
			u_char	*rest,
				c2 = (u_char) 0;

		/*
		 * Why use ptr+1?  Cause try to maintain backward compatability
		 * can be a pain in the butt.  Basically, we don't want any of
		 * the illegal characters in the alias, except that things like
		 * $* and $, were around first, so they must remain legal.  So
		 * we skip the first char after the $.  Does this make sense?
		 */
			/* special case for $ */
			if (*ptr == '$')
			{
				rest = ptr+1;
				c2 = *rest;
				*rest = (u_char) 0;
			}
			else if ((rest = sindex(ptr+1, alias_illegals)) != NULL)
			{
				if (isalpha(*ptr) || *ptr == '_')
					while ((*rest == LEFT_BRACKET ||
					    *rest == LEFT_PAREN) &&
					    (tmp = MatchingBracket(rest+1,
					    (int)*rest, (int)(*rest == LEFT_BRACKET) ?
					    RIGHT_BRACKET : RIGHT_PAREN)))
						rest = tmp + 1;
				c2 = *rest;
				*rest = (u_char) 0;
			}
			if ((tmp = parse_inline(ptr, args, args_flag)) != NULL)
			{
				expander_addition(lbuf, tmp, length,
					quote_em);
				new_free(&tmp);
			}
			if (rest)
				*rest = c2;
			return(rest);
		}
	}
	return NULL;
}


/*
 * expand_alias: Expands inline variables in the given string and returns the
 * expanded string in a new string which is malloced by expand_alias(). 
 *
 * Also unescapes anything that was quoted with a backslash
 *
 * Behaviour is modified by the following:
 *	Anything between brackets (...) {...} is left unmodified.
 *	If more_text is supplied, the text is broken up at
 *		semi-colons and returned one at a time. The unprocessed
 *		portion is written back into more_text.
 *	Backslash escapes are unescaped.
 */

u_char	*
expand_alias(u_char *name, u_char *string, u_char *args, int *args_flag, u_char **more_text)
{
	u_char	*lbuf = NULL,
		*ptr,
		*stuff = NULL,
		*free_stuff;
	u_char	*quote_em,
		*quote_str = NULL;
	u_char	ch;
	int	quote_cnt = 0;
	int	is_quote = 0;
	void	(*str_cat)(u_char **, u_char *);

	if (*string == '@' && more_text)
	{
		str_cat = malloc_strcat;
		*args_flag = 1; /* Stop the @ command from auto appending */
	}
	else
		str_cat = malloc_strcat_ue;
	malloc_strcpy(&stuff, string);
	free_stuff = stuff;
	malloc_strcpy(&lbuf, empty_string());
	eval_args = 1;
	ptr = stuff;
	if (more_text)
		*more_text = NULL;
	while (ptr && *ptr)
	{
		if (is_quote)
		{
			is_quote = 0;
			++ptr;
			continue;
		}
		switch(*ptr)
		{
		case '$':
	/*
	 * The test here ensures that if we are in the expression
	 * evaluation command, we don't expand $. In this case we
	 * are only coming here to do command separation at ';'s.
	 * If more_text is not defined, and the first character is
	 * '@', we have come here from [] in an expression.
	 */
			if (more_text && *string == '@')
			{
				ptr++;
				break;
			}
			*(ptr++) = (u_char) 0;
			(*str_cat)(&lbuf, stuff);
			while (*ptr == '^')
			{
				ptr++;
				if (quote_str)
					quote_str = new_realloc(quote_str,
						sizeof(*quote_str) *
						(quote_cnt + 2));
				else
					quote_str = new_malloc(sizeof(*quote_str) *
						(quote_cnt + 2));
				quote_str[quote_cnt++] = *(ptr++);
				quote_str[quote_cnt] = '\0';
			}
			quote_em = quote_str;
			stuff = alias_special_char(name, &lbuf, ptr, args,
				quote_em, args_flag);
			if (stuff)
				new_free(&quote_str);
			quote_cnt = 0;
			ptr = stuff;
			break;
		case ';':
			if (!more_text)
			{
				ptr++;
				break;
			}
			*more_text = string + (ptr - free_stuff) +1;
			*ptr = '\0'; /* To terminate the loop */
			break;
		case LEFT_PAREN:
		case LEFT_BRACE:
			ch = *ptr;
			*ptr = '\0';
			(*str_cat)(&lbuf, stuff);
			stuff = ptr;
			*args_flag = 1;
			if (!(ptr = MatchingBracket(stuff + 1, (int)ch,
					(int)(ch == LEFT_PAREN) ?
					RIGHT_PAREN : RIGHT_BRACE)))
			{
				yell("Unmatched %c", ch);
				ptr = stuff + my_strlen(stuff+1)+1;
			}
			else
				ptr++;
			*stuff = ch;
			ch = *ptr;
			*ptr = '\0';
			malloc_strcat(&lbuf, stuff);
			stuff = ptr;
			*ptr = ch;
			break;
		case '\\':
			is_quote = 1;
			ptr++;
			break;
		default:
			ptr++;
			break;
		}
	}
	if (stuff)
		(*str_cat)(&lbuf, stuff);
	ptr = NULL;
	new_free(&free_stuff);
	ptr = lbuf;
	if (get_int_var(DEBUG_VAR) & DEBUG_EXPANSIONS)
		yell("Expanded [%s] to [%s]",
			string, ptr);
	return (ptr);
}

/*
 * get_alias: returns the alias matching 'name' as the function value. 'args'
 * are expanded as needed, etc.  If no matching alias is found, null is
 * returned, cnt is 0, and full_name is null.  If one matching alias is
 * found, it is retuned, with cnt set to 1 and full_name set to the full name
 * of the alias.  If more than 1 match are found, null is returned, cnt is
 * set to the number of matches, and fullname is null. NOTE: get_alias()
 * mallocs the space for the full_name, but returns the actual value of the
 * alias if found! 
 */
u_char	*
get_alias(int type, u_char *name, int *cnt, u_char **full_name)
{
	Alias	*tmp;

	*full_name = NULL;
	if ((name == NULL) || (*name == (u_char) 0))
	{
		*cnt = 0;
		return (NULL);
	}
	if ((tmp = find_alias(&(alias_list[type]), name, 0, cnt)) != NULL)
	{
		if (*cnt < 2)
		{
			malloc_strcpy(full_name, tmp->name);
			return (tmp->stuff);
		}
	}
	return (NULL);
}

/*
 * match_alias: this returns a list of alias names that match the given name.
 * This is used for command completion etc.  Note that the returned array is
 * malloced in this routine.  Returns null if no matches are found 
 */
u_char	**
match_alias(u_char *name, int *cnt, int type)
{
	Alias	*tmp;
	u_char	**matches = NULL;
	int	matches_size = 5;
	size_t	len;
	u_char	*last_match = NULL;
	u_char	*dot;

	len = my_strlen(name);
	*cnt = 0;
	matches = new_malloc(sizeof(*matches) * matches_size);
	for (tmp = alias_list[type]; tmp; tmp = tmp->next)
	{
		if (my_strncmp(name, tmp->name, len) == 0)
		{
			if ((dot = my_index(tmp->name+len, '.')) != NULL)
			{
				if (type == COMMAND_ALIAS)
					continue;
				else
				{
					*dot = '\0';
					if (last_match && !my_strcmp(last_match,
							tmp->name))
					{
						*dot = '.';
						continue;
					}
				}
			}
			matches[*cnt] = NULL;
			malloc_strcpy(&(matches[*cnt]), tmp->name);
			last_match = matches[*cnt];
			if (dot)
				*dot = '.';
			if (++(*cnt) == matches_size)
			{
				matches_size += 5;
				matches = new_realloc(matches, sizeof(*matches) * matches_size);
			}
		}
		else if (*cnt)
			break;
	}
	if (*cnt)
	{
		matches = new_realloc(matches, sizeof(*matches) * (*cnt + 1));
		matches[*cnt] = NULL;
	}
	else
		new_free(&matches);
	return (matches);
}

/* delete_alias: The alias name is removed from the alias list. */
void
delete_alias(int type, u_char *name)
{
	Alias	*tmp;

	upper(name);
	if ((tmp = find_alias(&(alias_list[type]), name, 1, (int *) NULL))
			!= NULL)
	{
		new_free(&(tmp->name));
		new_free(&(tmp->stuff));
		new_free(&tmp);
		if (type == COMMAND_ALIAS)
			say("Alias	%s removed", name);
		else
			say("Assign %s removed", name);
	}
	else
		say("No such alias: %s", name);
}

/*
 * list_aliases: Lists all aliases matching 'name'.  If name is null, all
 * aliases are listed 
 */
void
list_aliases(int type, u_char *name)
{
	Alias	*tmp;
	size_t	len;
	int	lastlog_level;
	size_t	DotLoc,
		LastDotLoc = 0;
	u_char	*LastStructName = NULL;
	u_char	*s;

	lastlog_level = message_from_level(LOG_CRAP);
	if (type == COMMAND_ALIAS)
		say("Aliases:");
	else
		say("Assigns:");
	if (name)
	{
		upper(name);
		len = my_strlen(name);
	}
	else
		len = 0;
	for (tmp = alias_list[type]; tmp; tmp = tmp->next)
	{
		if (!name || !my_strncmp(tmp->name, name, len))
		{
			s = my_index(tmp->name + len, '.');
			if (!s)
				say("\t%s\t%s", tmp->name, tmp->stuff);
			else
			{
				DotLoc = s - tmp->name;
				if (!LastStructName || (DotLoc != LastDotLoc) || my_strncmp(tmp->name, LastStructName, DotLoc))
				{
					say("\t%*.*s\t<Structure>", (int)DotLoc, (int)DotLoc, tmp->name);
					LastStructName = tmp->name;
					LastDotLoc = DotLoc;
				}
			}
		}
	}
	(void)message_from_level(lastlog_level);
}

/*
 * mark_alias: sets the mark field of the given alias to 'flag', and returns
 * the previous value of the mark.  If the name is not found, -1 is returned.
 * This is used to prevent recursive aliases by marking and unmarking
 * aliases, and not reusing an alias that has previously been marked.  I'll
 * explain later 
 */
int
mark_alias(u_char *name, int flag)
{
	int	old_mark;
	Alias	*tmp;
	int	match;

	if ((tmp = find_alias(&(alias_list[COMMAND_ALIAS]), name, 0, &match))
			!= NULL)
	{
		if (match < 2)
		{
			old_mark = tmp->mark;
		/* New handling of recursion */
			if (flag)
			{
				int	i;
				/* Count recursion */

				tmp->mark = tmp->mark + flag;
				if ((i = get_int_var(MAX_RECURSIONS_VAR)) > 1)
				{
					if (tmp->mark > i)
					{
						tmp->mark = 0;
						return(1); /* MAX exceeded. */
					}
					else return(0);
				/* In recursion but it's ok */
				}
				else
				{
					if (tmp->mark > 1)
					{
						tmp->mark = 0;
						return(1);
				/* max of 1 here.. exceeded */
					}
					else return(0);
				/* In recursion but it's ok */
				}
			}
			else
		/* Not in recursion at all */
			{
				tmp->mark = 0;
				return(old_mark);
			/* This one gets ignored anyway */
			}
		}
	}
	return (-1);
}

/*
 * execute_alias: After an alias has been identified and expanded, it is sent
 * here for proper execution.  This routine mainly prevents recursive
 * aliasing.  The name is the full name of the alias, and the alias is
 * already expanded alias (both of these parameters are returned by
 * get_alias()) 
 */
void
execute_alias(u_char *alias_name, u_char *ealias, u_char *args)
{
	if (mark_alias(alias_name, 1))
		say("Maximum recursion count exceeded in: %s", alias_name);
	else
	{
		parse_line(alias_name, ealias, args, 0, 1, 0);
		mark_alias(alias_name, 0);
	}
}

/*
 * save_aliases: This will write all of the aliases to the FILE pointer fp in
 * such a way that they can be read back in using LOAD or the -l switch 
 */
void
save_aliases(FILE *fp, int do_all)
{
	Alias	*tmp;

	for (tmp = alias_list[VAR_ALIAS]; tmp; tmp = tmp->next)
		if (!tmp->global || do_all)
			fprintf(fp, "ASSIGN %s %s\n", tmp->name, tmp->stuff);
	for (tmp = alias_list[COMMAND_ALIAS]; tmp; tmp = tmp->next)
		if (!tmp->global || do_all)
			fprintf(fp, "ALIAS %s %s\n", tmp->name, tmp->stuff);
}

/* The Built-In Alias expando functions */
static	u_char	*
alias_line(void)
{
	return (get_input());
}

static	u_char	*
alias_time(void)
{
	static u_char timestr[16];
	struct tm *tm;
	time_t	t;

	t = time(0);
	tm = localtime(&t);

	return format_clock(timestr, 16, tm->tm_hour, tm->tm_min);
}

static	u_char	*
alias_dollar(void)
{
	return UP("$");
}

static	u_char	*
alias_detected(void)
{
	return get_last_notify_nick();
}

static	u_char	*
alias_nick(void)
{
	const	int	from_server = get_from_server();

	return from_server >= 0 ? server_get_nickname(from_server) : my_nickname();
}

static	u_char	*
alias_away(void)
{
	const	int	from_server = get_from_server();

	return from_server >= 0 ? server_get_away(from_server) : empty_string();
}

static	u_char	*
alias_sent_nick(void)
{
	u_char *sent_nick = get_sent_nick();

	return sent_nick ? sent_nick : empty_string();
}

static	u_char	*
alias_recv_nick(void)
{
	u_char *recv_nick = get_recv_nick();

	return recv_nick ? recv_nick : empty_string();
}

static	u_char	*
alias_msg_body(void)
{
	u_char *sent_body = get_sent_body();

	return sent_body ? sent_body : empty_string();
}

static	u_char	*
alias_joined_nick(void)
{
	u_char *joined_nick = get_joined_nick();

	return joined_nick ? joined_nick : empty_string();
}

static	u_char	*
alias_public_nick(void)
{
	u_char *public_nick = get_public_nick();

	return public_nick ? public_nick : empty_string();
}

static	u_char	*
alias_channel(void)
{
	u_char	*tmp;

	return (tmp = get_channel_by_refnum(0)) ? tmp : zero();
}

static	u_char	*
alias_server(void)
{
	u_char	*tmp;

	if (parsing_server() != -1)
		tmp = server_get_itsname(parsing_server());
	else if (get_window_server(0) != -1)
		tmp = server_get_itsname(get_window_server(0));
	else
		tmp = empty_string();
	return tmp;
}

static	u_char	*
alias_query_nick(void)
{
	u_char	*tmp;

	return (tmp = query_nick()) ? tmp : empty_string();
}

static	u_char	*
alias_target(void)
{
	u_char	*tmp;

	return (tmp = get_target_by_refnum(0)) ? tmp : empty_string();
}

static	u_char	*
alias_invite(void)
{
	u_char	*invite_channel = get_invite_channel();

	return invite_channel ? invite_channel : empty_string();
}

static	u_char	*
alias_cmdchar(void)
{
	static	u_char	thing[2];
	u_char	*cmdchars;

	if ((cmdchars = get_string_var(CMDCHARS_VAR)) == NULL)
		cmdchars = UP(DEFAULT_CMDCHARS);
	thing[0] = cmdchars[0];
	thing[1] = (u_char) 0;
	return (thing);
}

static	u_char	*
alias_oper(void)
{
	const	int	from_server = get_from_server();

	return (from_server >= 0 && server_get_operator(from_server)) ?
	       get_string_var(STATUS_OPER_VAR) : empty_string();
}

static	u_char	*
alias_chanop(void)
{
	u_char	*tmp;
	const	int	from_server = get_from_server();

	return (from_server >= 0 &&
		(tmp = get_channel_by_refnum(0)) &&
		get_channel_oper(tmp, from_server)) ?
	       UP("@") : empty_string();
}

static	u_char	*
alias_modes(void)
{
	u_char	*tmp;
	const	int	from_server = get_from_server();

	return (from_server >= 0 &&
		(tmp = get_channel_by_refnum(from_server))) ?
	       get_channel_mode(tmp, from_server) : empty_string();
}

static	u_char	*
alias_version(void)
{
	return irc_version();
}

static	u_char	*
alias_currdir(void)
{
	static	u_char	dirbuf[1024];

	if (getcwd(CP(dirbuf), 1024) == NULL)
	{
		dirbuf[0] = '.';
		dirbuf[1] = '\0';
	}
	return (dirbuf);
}

static	u_char	*
alias_current_numeric(void)
{
	static	u_char	number[4];
	
	snprintf(CP(number), sizeof number, "%03d", -current_numeric());
	return (number);
}

static	u_char	*
alias_server_version(void)
{
	u_char	*s;
	const	int	from_server = get_from_server();

	return from_server >= 0 && (s = server_get_version_string(from_server)) ?
		s : empty_string();
}

/*
 * alias: the /ALIAS command.  Calls the correct alias function depending on
 * the args 
 */
void	
alias(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*name,
		*rest;
	int	type;
	u_char	*ArrayIndex;
	u_char	*EndIndex;

	type = *command - 48;	/*
				 * A trick!  Yes, well, what the hell.  Note
				 * the the command part of ALIAS is "0" and
				 * the command part of ASSIGN is "1" in the
				 * command array list
				 */
	if ((name = next_arg(args, &rest)) != NULL)
	{
		while ((ArrayIndex = my_index(name, '[')) != NULL)
		{
			*ArrayIndex++ = '.';
			if ((EndIndex = MatchingBracket(ArrayIndex,
					(int)LEFT_BRACKET, (int)RIGHT_BRACKET)) != NULL)
			{
				*EndIndex++ = '\0';
				my_strcat(name, EndIndex);
			}
			else
				break;
		}
		if (*rest)
		{
			if (*rest == LEFT_BRACE)
			{
				u_char	*ptr;

				ptr = MatchingBracket(++rest,
						(int)LEFT_BRACE, (int)RIGHT_BRACE);
				if (!ptr)
				    say("Unmatched brace in ALIAS or ASSIGN");
				else if (ptr[1])
				{
					say("Junk after closing brace in ALIAS or ASSIGN");
				}
				else
				{
					*ptr = '\0';
					add_alias(type, name, rest);
				}
			}
			else
				add_alias(type, name, rest);
		}
		else
		{
			if (*name == '-')
			{
				if (*(name + 1))
					delete_alias(type, name + 1);
				else
					say("You must specify an alias to be removed");
			}
			else
				list_aliases(type, name);
		}
	}
	else
		list_aliases(type, NULL);
}

static u_char	*
function_left(u_char *input)
{
	u_char	*result = NULL;
	u_char	*count;
	int	cvalue;

	count = next_arg(input, &input);
	if (count)
		cvalue = my_atoi(count);
	else
		cvalue = 0;
	if ((int) my_strlen(input) > cvalue)
		input[cvalue] = '\0';
	malloc_strcpy(&result, input);
	return (result);
}

static u_char	*
function_right(u_char *input)
{
	u_char	*result = NULL;
	u_char	*count;
	int	cvalue;
	int	len;

	count = next_arg(input, &input);
	if (count)
		cvalue = my_atoi(count);
	else
		cvalue = 0;
	if ((len = (int) my_strlen(input)) > cvalue)
		input += len - cvalue;
	malloc_strcpy(&result, input);
	return (result);
}

static u_char	*
function_mid(u_char *input)
{
	u_char	*result = NULL;
	u_char	*mid_index;
	int	ivalue;
	u_char	*count;
	int	cvalue;

	mid_index = next_arg(input, &input);
	if (mid_index)
		ivalue = my_atoi(mid_index);
	else
		ivalue = 0;
	count = next_arg(input, &input);
	if (count)
		cvalue = my_atoi(count);
	else
		cvalue = 0;
	if (ivalue >= 0 && (int) my_strlen(input) > ivalue)
		input += ivalue;
	else
		*input = '\0';
	if (cvalue > 0 && (int) my_strlen(input) > cvalue)
		input[cvalue] = '\0';
	malloc_strcpy(&result, input);
	return (result);
}

/* patch from Sarayan to make $rand() better */

#define RAND_A 16807L
#define RAND_M 2147483647L
#define RAND_Q 127773L
#define RAND_R 2836L

static	long	
randm(long l)
{
	static	u_long	z = 0;
	long	t;

	if (!z)
		z = (u_long) getuid();
	if (!l)
	{
		t = RAND_A * (z % RAND_Q) - RAND_R * (z / RAND_Q);
		if (t > 0)
			z = t;
		else
			z = t + RAND_M;
		return (z >> 8) | ((z & 255) << 23);
	}
	else
	{
		if (l < 0)
			z = (u_long) getuid();
		else
			z = l;
		return 0;
	}
}

static u_char	*
function_rand(u_char *input)
{
	u_char	*result = NULL;
	u_char	tmp[40];
	long	tempin;

	snprintf(CP(tmp), sizeof tmp, "%ld", (tempin = my_atol(input)) ? randm(0L) % tempin : 0);
	malloc_strcpy(&result, tmp);
	return (result);
}

static u_char	*
function_srand(u_char *input)
{
	u_char	*result = NULL;

	if (input && *input)
		(void) randm(my_atol(input));
	else
		(void) randm((long) time(NULL));
	malloc_strcpy(&result, empty_string());
	return (result);
}

static u_char	*
function_time(u_char *input)
{
	u_char	*result = NULL;
	time_t	ltime;
	u_char	tmp[40];

	(void) time(&ltime);
	snprintf(CP(tmp), sizeof tmp, "%ld", (long)ltime);
	malloc_strcpy(&result, tmp);
	return (result);
}

static u_char	*
function_stime(u_char *input)
{
	u_char	*result = NULL;
	time_t	ltime;

	ltime = my_atol(input);
	malloc_strcpy(&result, UP(ctime(&ltime)));
	result[my_strlen(result) - 1] = (u_char) 0;
	return (result);
}

static u_char	*
function_tdiff(u_char *input)
{
	u_char	*result = NULL;
	time_t	ltime;
	time_t	days,
		hours,
		minutes,
		seconds;
	u_char	tmp[80];
	u_char	*tstr;

	ltime = my_atol(input);
	seconds = ltime % 60;
	ltime = (ltime - seconds) / 60;
	minutes = ltime%60;
	ltime = (ltime - minutes) / 60;
	hours = ltime % 24;
	days = (ltime - hours) / 24;
	tstr = tmp;
	if (days)
	{
		snprintf(CP(tstr), sizeof(tmp) - (tstr - tmp), "%ld day%s ", (long) days, (days==1)?"":"s");
		tstr += my_strlen(tstr);
	}
	if (hours)
	{
		snprintf(CP(tstr), sizeof(tmp) - (tstr - tmp), "%ld hour%s ", (long) hours, (hours==1)?"":"s");
		tstr += my_strlen(tstr);
	}
	if (minutes)
	{
		snprintf(CP(tstr), sizeof(tmp) - (tstr - tmp), "%ld minute%s ", (long) minutes, (minutes==1)?"":"s");
		tstr += my_strlen(tstr);
	}
	if (seconds || (!days && !hours && !minutes))
	{
		snprintf(CP(tstr), sizeof(tmp) - (tstr - tmp), "%ld second%s", (long) seconds, (seconds==1)?"":"s");
		tstr += my_strlen(tstr);
	}
	malloc_strcpy(&result, tmp);
	return (result);
}

static u_char	*
function_index(u_char *input)
{
	u_char	*result = NULL;
	u_char	*schars;
	u_char	*iloc;
	int	ival;
	u_char	tmp[40];

	schars = next_arg(input, &input);
	iloc = (schars) ? sindex(input, schars) : NULL;
	ival = (iloc) ? iloc - input : -1;
	snprintf(CP(tmp), sizeof tmp, "%d", ival);
	malloc_strcpy(&result, tmp);
	return (result);
}

static u_char	*
function_rindex(u_char *input)
{
	u_char	*result = NULL;
	u_char	*schars;
	u_char	*iloc;
	int	ival;
	u_char	tmp[40];

	schars = next_arg(input, &input);
	iloc = (schars) ? srindex(input, schars) : NULL;
	ival = (iloc) ? iloc - input : -1;
	snprintf(CP(tmp), sizeof tmp, "%d", ival);
	malloc_strcpy(&result, tmp);
	return (result);
}

static u_char	*
function_match(u_char *input)
{
	u_char	*result = NULL;
	u_char	*pattern;
	u_char	*word;
	int	current_match;
	int	best_match = 0;
	int	match = 0;
	int	match_index = 0;
	u_char	tmp[40];

	if ((pattern = next_arg(input, &input)) != NULL)
	{
		while ((word = next_arg(input, &input)) != NULL)
		{
			match_index++;
			if ((current_match = wild_match(pattern, word))
					> best_match)
			{
				match = match_index;
				best_match = current_match;
			}
		}
	}
	snprintf(CP(tmp), sizeof tmp, "%d", match);
	malloc_strcpy(&result, tmp);
	return (result);
}

static u_char	*
function_rmatch(u_char *input)
{
	u_char	*result = NULL;
	u_char	*pattern;
	u_char	*word;
	int	current_match;
	int	best_match = 0;
	int	match = 0;
	int	rmatch_index = 0;
	u_char	tmp[40];

	if ((pattern = next_arg(input, &input)) != NULL)
	{
		while ((word = next_arg(input, &input)) != NULL)
		{
			rmatch_index++;
			if ((current_match = wild_match(word, pattern)) >
					best_match)
			{
				match = rmatch_index;
				best_match = current_match;
			}
		}
	}
	snprintf(CP(tmp), sizeof tmp, "%d", match);
	malloc_strcpy(&result, tmp);
	return (result);
}

static u_char	*
function_userhost(u_char *input)
{
	u_char	*result = NULL;
	u_char	*userhost = from_user_host();

	malloc_strcpy(&result, userhost ? userhost : empty_string());
	return (result);
}

static u_char	*
function_strip(u_char *input)
{
	u_char	tmpbuf[128], *result;
	u_char	*retval = NULL;
	u_char	*chars;
	u_char	*cp, *dp;
	size_t len = 0;

	if ((chars = next_arg(input, &input)) && input)
	{
		len = my_strlen(input);
		if (len > 127)
			result = new_malloc(len + 1);
		else
			result = tmpbuf;

		for (cp = input, dp = result; *cp; cp++)
		{
			if (!my_index(chars, *cp))
				*dp++ = *cp;
		}
		*dp = '\0';
	}
	malloc_strcpy(&retval, result);
	if (len > 127)
		new_free(&result);	/* we could use this copy, but it might be extra-long */
	return (retval);
}

static u_char	*
function_encode(u_char *input)
{
	u_char	*result;
	u_char	*c;
	int	i = 0;

	result = new_malloc(my_strlen(input) * 2 + 1);
	for (c = input; *c; c++)
	{
		result[i++] = (*c >> 4) + 0x41;
		result[i++] = (*c & 0x0f) + 0x41;
	}
	result[i] = '\0';
	return (result);
}


static u_char	*
function_decode(u_char *input)
{
	u_char	*result;
	u_char	*c;
	u_char	d, e;
	int	i = 0;

	c = input;
	result = new_malloc(my_strlen(input) / 2 + 2);
	while((d = *c) && (e = *(c+1)))
	{
		result[i] = ((d - 0x41) << 4) | (e - 0x41);
		c += 2;
		i++;
	}
	result[i] = '\0';
	return (result);
}

static u_char	*
function_ischannel(u_char *input)
{
	u_char	*result = NULL;

	malloc_strcpy(&result, is_channel(input) ? one() : zero());
	return (result);
}

static u_char	*
function_ischanop(u_char *input)
{
	u_char	*result = NULL;
	u_char	*nick;
	u_char	*channel = NULL;

	if (!(nick = next_arg(input, &channel)))
		malloc_strcpy(&result, zero());
	else
		malloc_strcpy(&result, is_chanop(channel, nick) ? one() : zero());
	return (result);
}

#ifdef HAVE_CRYPT
static u_char *
function_crypt(u_char *input)
{
	u_char	*result = NULL;
	u_char	*salt;
	u_char	*key = NULL;

	if (!(salt = next_arg(input, &key)))
		malloc_strcpy(&result, zero());
	else
		malloc_strcpy(&result, UP(crypt(CP(key), CP(salt))));
	return (result);
}
#endif /* HAVE_CRYPT */

static u_char *
function_hasvoice(u_char *input)
{
	u_char	*result = NULL;
	u_char	*nick;
	u_char	*channel = NULL;

	if (!(nick = next_arg(input, &channel)))
		malloc_strcpy(&result, zero());
	else
		malloc_strcpy(&result, has_voice(channel, nick, get_from_server()) ? one() : zero());
	return (result);
}

static u_char *
function_dcclist(u_char *nick)
{
	if (!nick)
	{
		u_char *result;

		malloc_strcpy(&result, zero());
		return (result);
	}
	
	return dcc_list_func(nick);
}

static u_char *
function_chatpeers(u_char *dummy)
{
	return dcc_chatpeers_func();
}

static u_char	*
function_word(u_char *input)
{
	u_char	*result = NULL;
	u_char	*count;
	int	cvalue;
	u_char	*word;

	count = next_arg(input, &input);
	if (count)
		cvalue = my_atoi(count);
	else
		cvalue = 0;
	if (cvalue < 0)
		malloc_strcpy(&result, empty_string());
	else
	{
		for (word = next_arg(input, &input); word && cvalue--;
				word = next_arg(input, &input))
			;
		malloc_strcpy(&result, (word) ? word : empty_string());
	}
	return (result);
}

static u_char	*
function_querynick(u_char *input)
{
	u_char	*result = NULL;
	Window	*win;

	if (input && isdigit(*input))
		win = get_window_by_refnum((u_int)my_atoi(input));
	else
		win = curr_scr_win;
	malloc_strcpy(&result, win ? window_get_query_nick(win) : UP("-1"));
	return (result);
}

static u_char	*
function_windows(u_char *input)
{
	u_char	*result = NULL;
	Win_Trav wt;
	Window	*tmp;

	malloc_strcat(&result, empty_string());
	wt.init = 1;
	while ((tmp = window_traverse(&wt)))
	{
		u_char *name = window_get_name(tmp);

		if (name)
		{
			malloc_strcat(&result, name);
			malloc_strcat(&result, UP(" "));
		}
		else
		{
			u_char buf[32];

			snprintf(CP(buf), sizeof buf, "%u ", window_get_refnum(tmp));
			malloc_strcat(&result, buf);
		}
	}

	return (result);
}

static u_char	*
function_screens(u_char *input)
{
	Screen	*screen;
	u_char	*result = NULL;
	u_char buf[32];

	malloc_strcat(&result, empty_string());
	for (screen = screen_first(); screen; screen = screen_get_next(screen))
	{
		if (screen_get_alive(screen))
		{
			snprintf(CP(buf), sizeof buf, "%u ", screen_get_screennum(screen));
			malloc_strcat(&result, buf);
		}
	}

	return (result);
}

static u_char	*
function_notify(u_char *input)
{
	u_char	*result;

	if (input && my_stricmp(input, UP("gone")) == 0)
		result = get_notify_list(NOTIFY_LIST_GONE);
	else if (input && my_stricmp(input, UP("all")) == 0)
		result = get_notify_list(NOTIFY_LIST_ALL);
	else
		result = get_notify_list(NOTIFY_LIST_HERE);

	return result;
}

/*
 * $ignored(nick!user@host type) with type from:
 *	ALL MSGS PUBLIC WALLS WALLOPS INVITES NOTICES NOTES CTCP CRAP
 * $ignored(nick):
 *	gives ignore's matching nick
 * $ignored():
 *	gives all
 */
static u_char	*
function_ignored(u_char *input)
{
	u_char	*result = NULL, *userhost, *nick;
	int type;

	if ((nick = next_arg(input, &input)) != NULL)
	{
		if (!input || !*input)
			goto do_ignore_list;

		type = get_ignore_type(input);
		if (type == 0 || type == -1 || type == IGNORE_ALL)
			goto do_zero;

		if ((userhost = UP(my_index(nick, '!'))))
			*userhost++ = 0;
		switch (double_ignore(nick, userhost, type))
		{
		case DONT_IGNORE:
			malloc_strcpy(&result, UP("dont"));
			break;
		case HIGHLIGHTED:
			malloc_strcpy(&result, UP("highlighted"));
			break;
		case IGNORED:
			malloc_strcpy(&result, UP("ignored"));
			break;
		default:
			goto do_zero;
		}
	}
	else
do_ignore_list:
		result = ignore_list_string(nick);
	if (!result)
do_zero:
		malloc_strcpy(&result, zero());
	return (result);
}

static u_char	*
function_winserver(u_char *input)
{
	u_char	*result = NULL;
	u_char	tmp[10];
	Window	*win;

	if (input && isdigit(*input))
		win = get_window_by_refnum((u_int)my_atoi(input));
	else
		win = curr_scr_win;
	snprintf(CP(tmp), sizeof tmp, "%d",
		 win ? window_get_server(win) : -1);
	malloc_strcpy(&result, tmp);
	return (result);
}

static u_char	*
function_winservergroup(u_char *input)
{
	u_char	*result = NULL;
	u_char	tmp[10];
	Window	*win;

	if (input && isdigit(*input))
		win = get_window_by_refnum((u_int)my_atoi(input));
	else
		win = curr_scr_win;
	snprintf(CP(tmp), sizeof tmp, "%d",
		 win ? window_get_server_group(win) : -1);
	malloc_strcpy(&result, tmp);
	return (result);
}

static u_char	*
function_winvisible(u_char *input)
{
	u_char	*result = NULL;
	u_char	tmp[10];
	Window	*win;

	if (input && isdigit(*input))
		win = get_window_by_refnum((u_int)my_atoi(input));
	else
		win = curr_scr_win;
	snprintf(CP(tmp), sizeof tmp, "%d", win ? window_get_visible(win) : -1);
	malloc_strcpy(&result, tmp);
	return (result);
}

static u_char	*
function_winnum(u_char *input)
{
	u_char	*result = NULL;
	u_char	tmp[10];

	snprintf(CP(tmp), sizeof tmp, "%d",
		 curr_scr_win ? (int)window_get_refnum(curr_scr_win) : -1);
	malloc_strcpy(&result, tmp);
	return (result);
}

static u_char	*
function_winnam(u_char *input)
{
	u_char	*result = NULL;
	Window	*win;

	if (input && isdigit(*input))
		win = get_window_by_refnum((u_int)my_atoi(input));
	else
		win = curr_scr_win;
	malloc_strcpy(&result, (win && window_get_name(win)) ?
				window_get_name(win) : empty_string());
	return (result);
}

/*
 * returns the current window's display (len) size counting for double
 * status bars, etc.. -Toasty
 */
static u_char    *
function_winrows(u_char *input)
{
	u_char	*result = NULL;

	if (curr_scr_win)
	{
		u_char	tmp[10];

		snprintf(CP(tmp), sizeof tmp, "%d",
			 window_get_display_size(curr_scr_win));
		malloc_strcpy(&result, tmp);
	}
	else
		malloc_strcpy(&result, UP("-1"));
	return (result);
}

/*
 * returns the current screen's (since all windows have the same
 * column/width) column value -Toasty
 */
static u_char	*
function_wincols(u_char *input)
{
	u_char	*result = NULL;

	if (curr_scr_win)
	{
		u_char	tmp[10];

		snprintf(CP(tmp), sizeof tmp, "%d", get_co());
		malloc_strcpy(&result, tmp);
	}
	else
		malloc_strcpy(&result, UP("-1"));
	return (result);
}

static u_char	*
function_connect(u_char *input)
{
	u_char	*result = NULL;
	u_char	*host;

#ifdef DAEMON_UID
	if (getuid() == DAEMON_UID)
		put_it("You are not permitted to use CONNECT()");
	else
#endif /* DAEMON_ID */
		if ((host = next_arg(input, &input)) != NULL)
			result = dcc_raw_connect(host, (u_int) my_atoi(input));
	return (result);
}


static u_char	*
function_listen(u_char *input)
{
	u_char	*result = NULL;

#ifdef DAEMON_UID
	if (getuid() == DAEMON_UID)
		malloc_strcpy(&result, zero());
	else
#endif /* DAEMON_ID */
		result = dcc_raw_listen((u_int) my_atoi(input));
	return (result);
}

static u_char	*
function_toupper(u_char *input)
{
	u_char	*new = NULL,
		*ptr;

	if (!input)
		return empty_string();
	malloc_strcpy(&new, input);
	for (ptr = new; *ptr; ptr++)
		*ptr = islower(*ptr) ? toupper(*ptr) : *ptr;
	return new;
}

static u_char	*
function_tolower(u_char *input)
{
	u_char	*new = NULL,
		*ptr;

	if (!input)
		return empty_string();
	malloc_strcpy(&new, input);
	for (ptr = new; *ptr; ptr++)
		*ptr = (isupper(*ptr)) ? tolower(*ptr) : *ptr;
	return new;
}

static u_char	*
function_channels(u_char *input)
{
	Window	*window;

	if (input)
		window = isdigit(*input) ? get_window_by_refnum((u_int) my_atoi(input))
					 : curr_scr_win;
	else
		window = curr_scr_win;

	return create_channel_list(window);
}

static u_char	*
function_servers(u_char *input)
{
	return create_server_list();
}

static u_char	*
function_servertype(u_char *input)
{
	int server;
	u_char *s;
	u_char *result = NULL;
	const	int	from_server = get_from_server();

	if (from_server < 0)
		server = get_primary_server();
	else
		server = from_server;
	if (server < 0)
		s = empty_string();
	else	
		switch (server_get_version(server)) {
		case ServerICB:
			s = UP("ICB");
			break;
		case Server2_6:
			s = UP("IRC2.6");
			break;
		case Server2_7:
			s = UP("IRC2.7");
			break;
		case Server2_8:
			s = UP("IRC2.8");
			break;
		case Server2_9:
			s = UP("IRC2.9");
			break;
		case Server2_10:
			s = UP("IRC2.10");
			break;
		case Server2_11:
			s = UP("IRC2.11");
			break;
		default:
			s = UP("IRC unknown");
			break;
		}

	malloc_strcpy(&result, s);
	return (result);
}

static u_char	*
function_onchannel(u_char *input)
{
	u_char	*result = NULL;
	u_char	*nick;
	u_char	*channel = NULL;
	const	int	from_server = get_from_server();

	if (from_server < 0 || !(nick = next_arg(input, &channel)))
		malloc_strcpy(&result, zero());
	else
		malloc_strcpy(&result,
			is_on_channel(channel, from_server, nick) ? one() : zero());
	return (result);
}

static u_char	*
function_pid(u_char *input)
{
	u_char	*result = NULL;
	u_char	lbuf[32];	/* plenty big enough for %d */

	snprintf(CP(lbuf), sizeof lbuf, "%d", (int) getpid());
	malloc_strcpy(&result, lbuf);	
	return (result);
}

static u_char	*
function_ppid(u_char *input)
{
	u_char	*result = NULL;
	u_char	lbuf[32];	/* plenty big enough for %d */

	snprintf(CP(lbuf), sizeof lbuf, "%d", (int) getppid());
	malloc_strcpy(&result, lbuf);	
	return (result);
}

/*
 * idle() patch from scottr (scott.reynolds@plexus.com)
 */
static u_char	*
function_idle(u_char *input)
{
	u_char	*result = NULL;
	u_char	lbuf[20];

	snprintf(CP(lbuf), sizeof lbuf, "%ld", (long)(time(0) - idle_time()));
	malloc_strcpy(&result, lbuf);
	return (result);
}

/*
 * strftime() patch from hari (markc@arbld.unimelb.edu.au)
 */
#ifdef HAVE_STRFTIME
static u_char	*
function_strftime(u_char *input)
{
	u_char	result[128];
	time_t	ltime;
	u_char	*fmt = NULL;

	ltime = my_atol(input);
	fmt = input;
	/* skip the time field */
	while (isdigit(*fmt))
		++fmt; 
	if (*fmt && *++fmt)
	{
		struct tm	*tm;

		tm = localtime(&ltime);
		if (strftime(CP(result), 128, CP(fmt), tm))
		{
			u_char	*s = NULL;

			malloc_strcpy(&s, result);
			return s;
		}
		else
			return NULL;
	}
	else
	{
		return NULL;
	}
}
#endif /* HAVE_STRFTIME */

static u_char	*
function_urlencode(u_char *input)
{
	u_char	*result;
	u_char	*c;
	int	i = 0;
	
	for (c = input; *c; c++)
		if (*c == '+' || *c == '%' || *c == '&')
			i += 3;
		else
			++i;
	
	result = new_malloc(i + 1);

	for (i = 0, c = input; *c; c++)
	{
		if (*c == ' ')
			result[i++] = '+';
		else if (*c == '+')
		{
			result[i++] = '%';
			result[i++] = '2';
			result[i++] = 'B';
		}
		else if (*c == '&')
		{
			result[i++] = '%';
			result[i++] = '2';
			result[i++] = '6';
		}
		else
			result[i++] = *c;
	}
	result[i] = '\0';
	return (result);
}

static u_char	*
function_shellfix(u_char *input)
{
	u_char	*result;
	u_char	*c;
	int	i = 2;
	
	for (c = input; *c; c++)
		if (*c == '\'')
			i += 4;
		else
			++i;
	
	result = new_malloc(i + 1);
	
	i = 0;
	result[i++] = '\'';
	for (c = input; *c; c++)
	{
		if (*c == '\'')
		{
			result[i++] = '\'';
			result[i++] = '\\';
			result[i++] = '\'';
			result[i++] = '\'';
		}
		else
			result[i++] = *c;
	}
	result[i++] = '\'';
	result[i] = '\0';
	return (result);
}

static u_char	*
function_filestat(u_char *input)
{
	struct	stat statbuf;
	u_char	*result;
	u_char	lbuf[40];   /* 40 should be enough */


	if (stat(CP(input), &statbuf) == -1)
		malloc_strcpy(&result, empty_string());
	else
	{
		snprintf(CP(lbuf), sizeof lbuf, "%lu,%d,%d,%o,%s",
		    (unsigned long)statbuf.st_size,
		    (int)statbuf.st_uid,
		    (int)statbuf.st_gid,
		    statbuf.st_mode,
		    input);
		malloc_strcpy(&result, lbuf);
	}
	return (result);
}
