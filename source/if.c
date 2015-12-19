/*
 * if.c: handles the IF command for IRCII 
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
IRCII_RCSID("@(#)$eterna: if.c,v 1.38 2015/09/01 22:42:31 mrg Exp $");

#include "alias.h"
#include "ircaux.h"
#include "window.h"
#include "vars.h"
#include "output.h"
#include "if.h"

static	int	charcount(u_char *, int);

/*
 * next_expr finds the next expression delimited by brackets. The type
 * of bracket expected is passed as a parameter. Returns NULL on error.
 */
u_char	*
next_expr(u_char **args, int itype)
{
	u_char	*ptr,
		*ptr2,
		*ptr3;
	u_char	type = (u_char)itype;

	if (!*args)
		return NULL;
	ptr2 = *args;
	if (!*ptr2)
		return 0;
	if (*ptr2 != type)
	{
		say("Expression syntax");
		return 0;
	}							/* { */
	ptr = MatchingBracket(ptr2 + 1, type, (type == '(') ? ')' : '}');
	if (!ptr)
	{
		say("Unmatched '%c'", type);
		return 0;
	}
	*ptr = '\0';
	do
	{
		ptr2++;
	}
	while (isspace(*ptr2));
	ptr3 = ptr+1;
	while (isspace(*ptr3))
		ptr3++;
	*args = ptr3;
	if (*ptr2)
	{
		ptr--;
		while (isspace(*ptr))
			*ptr-- = '\0';
	}
	return ptr2;
}

void
ifcmd(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*expr;
	u_char	*sub;
	int	flag = 0;
	int	result;

	if (!(expr = next_expr(&args, '(')))
	{
		yell("Missing CONDITION in IF");
		return;
	}
	sub = parse_inline(expr, subargs ? subargs : empty_string(), &flag);
	if (get_int_var(DEBUG_VAR) & DEBUG_EXPANSIONS)
		yell("If expression expands to: (%s)", sub);
	if (!*sub || *sub == '0')
		result = 0;
	else
		result = 1;
	new_free(&sub);
	if (!(expr = next_expr(&args, '{')))
	{
		yell("Missing THEN portion in IF");
		return;
	}
	if (!result && !(expr = next_expr(&args, '{')))
		return;
	parse_line(NULL, expr, subargs ? subargs : empty_string(), 0, 0, 0);
		return;
}

void
whilecmd(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*expr = NULL,
		*ptr,
		*body = NULL,
		*newexp = NULL;
	int	args_used;	/* this isn't used here, but is passed
				 * to expand_alias() */

	if ((ptr = next_expr(&args, '(')) == NULL)
	{
		yell("WHILE: missing boolean expression");
		return;
	}
	malloc_strcpy(&expr, ptr);
	if ((ptr = next_expr(&args, '{')) == NULL)
	{
		say("WHILE: missing expression");
		new_free(&expr);
		return;
	}
	malloc_strcpy(&body, ptr);
	while (1)
	{
		malloc_strcpy(&newexp, expr);
		ptr = parse_inline(newexp, subargs ? subargs : empty_string(),
			&args_used);
		if (*ptr && *ptr !='0')
		{
			new_free(&ptr);
			parse_line(NULL, body, subargs ?
				subargs : empty_string(), 0, 0, 0);
		}
		else
			break;
	}
	new_free(&newexp);
	new_free(&ptr);
	new_free(&expr);
	new_free(&body);
}

static int
charcount(u_char *string, int what)
{
	int	x = 0;
	u_char	*place = string - 1;

	while ((place = my_index(place + 1, what)))
		x++;
	return x;
}

/*
 * How it works -- if There are no parenthesis, it must be a
 * foreach array command.  If there are parenthesis, and there are
 * exactly two commas, it must be a C-like for command, else it must
 * must be an foreach word command
 */
void
foreach_handler(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*temp = NULL;
	u_char	*placeholder;
	u_char	*temp2 = NULL;

	malloc_strcpy(&temp, args);
	placeholder = temp;
	if (*temp == '(')
	{
		if ((temp2 = next_expr(&temp, '(')) == NULL) {
			new_free(&placeholder);
			return;
		}
		if (charcount(temp2, ',') == 2)
			forcmd(command, args, subargs);
		else
			fe(command, args, subargs);
	}
	else
		foreach(command, args, subargs);
	new_free(&placeholder);
}

void
foreach(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*struc = NULL,
		*ptr,
		*body = NULL,
		*var = NULL;
	u_char	**sublist;
	int	total;
	int	i;
	int	slen;

	if ((ptr = new_next_arg(args, &args)) == NULL)
	{
		yell("FOREACH: missing structure expression");
		return;
	}
	malloc_strcpy(&struc, ptr);
	malloc_strcat(&struc, UP("."));
	upper(struc);
	if ((var = next_arg(args, &args)) == NULL)
	{
		new_free(&struc);
		yell("FOREACH: missing variable");
		return;
	}
	while (isspace(*args))
		args++;
	if ((body = next_expr(&args, '{')) == NULL)	/* } */
	{
		new_free(&struc);
		yell("FOREACH: missing statement");
		return;
	}
	sublist = match_alias(struc, &total, VAR_ALIAS);
	slen = my_strlen(struc);
	for (i = 0; i < total; i++)
	{
		unsigned	display;

		display = set_display_off();
		add_alias(VAR_ALIAS, var, sublist[i]+slen);
		set_display(display);
		parse_line(NULL, body, subargs ?
		    subargs : empty_string(), 0, 0, 0);
		new_free(&sublist[i]);
	}
	new_free(&sublist);
	new_free(&struc);
}

/*
 * FE:  Written by Jeremy Nelson (jnelson@iastate.edu)
 *
 * FE: replaces recursion
 *
 * The thing about it is that you can nest variables, as this command calls
 * expand_alias until the list doesnt change.  So you can nest lists in
 * lists, and hopefully that will work.  However, it also makes it 
 * impossible to have $s anywhere in the list.  Maybe ill change that
 * some day.
 */

void
fe(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*list = NULL,
		*templist = NULL,
		*placeholder,
		*oldlist = NULL,
		*sa,
		*vars,
		*var[255],
		*word = NULL,
		*todo = NULL;
	int	ind, x, y, count, args_flag;
	unsigned display;

	for (x = 0; x < 255; var[x++] = NULL)
		;

	list = next_expr(&args, '(');	/* ) */
	if (!list)
	{
		yell ("FE: Missing List for /FE");
		return;
	}

	sa = subargs ? subargs : (u_char *) " ";
	malloc_strcpy(&templist, list);
	do 
	{
		malloc_strcpy(&oldlist, templist);
		new_free(&templist);
		templist = expand_alias(NULL, oldlist, sa, &args_flag, NULL);
	} while (my_strcmp(templist, oldlist));

	new_free(&oldlist);

	if (*templist == '\0')
	{
		new_free(&templist);
		return;
	}

	vars = args;
	if (!(args = my_index(args, '{')))		/* } */
	{
		yell ("FE: Missing commands");
		new_free(&templist);
		return;
	}
	*(args-1) = '\0';
	ind = 0;
	while ((var[ind++] = next_arg(vars, &vars)))
	{
		if (ind == 255)
		{
			yell ("FE: Too many variables");
			new_free(&templist);
			return;
		}
	}
	ind = ind ? ind - 1: 0;

	if (!(todo = next_expr(&args, '{')))		/* } { */
	{
		yell ("FE: Missing }");		
		new_free(&templist);
		return;
	}

	count = word_count(templist);
	display = get_display();
	placeholder = templist;
	for (x = 0; x < count;)
	{
		set_display_off();
		for (y = 0; y < ind; y++)
		{
			word = ((x + y) < count)
			    ? next_arg(templist, &templist)
			    : NULL;
			add_alias(VAR_ALIAS, var[y], word);
		}
		set_display(display);
		x += ind;
		parse_line(NULL, todo, 
		    subargs ? subargs : empty_string(), 0, 0, 0);
	}
	set_display_off();
	for (y = 0; y < ind; y++)  {
		delete_alias(VAR_ALIAS, var[y]);
	}
	set_display(display);
	new_free(&placeholder);
}

/* FOR command..... prototype: 
 *  for (commence,evaluation,iteration)
 * in the same style of C's for, the for loop is just a specific
 * type of WHILE loop.
 *
 * IMPORTANT: Since ircII uses ; as a delimeter between commands,
 * commas were chosen to be the delimiter between expressions,
 * so that semicolons may be used in the expressions (think of this
 * as the reverse as C, where commas seperate commands in expressions,
 * and semicolons end expressions.
 */
/*  I suppose someone could make a case that since the
 *  foreach_handler() routine weeds out any for command that doesnt have
 *  two commands, that checking for those 2 commas is a waste.  I suppose.
 */
void
forcmd(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*working = NULL;
	u_char	*commence = NULL;
	u_char	*evaluation = NULL;
	u_char	*lameeval = NULL;
	u_char	*iteration = NULL;
	u_char	*sa = NULL;
	int	argsused = 0;
	u_char	*line = NULL;
	u_char	*commands = NULL;

	/* Get the whole () thing */
	if ((working = next_expr(&args, '(')) == NULL)	/* ) */
	{
		yell("FOR: missing closing parenthesis");
		return;
	}
	malloc_strcpy(&commence, working);

	/* Find the beginning of the second expression */
	evaluation = my_index(commence, ',');
	if (!evaluation)
	{
		yell("FOR: no components!");
		new_free(&commence);
		return;
	}
	do 
		*evaluation++ = '\0';
	while (isspace(*evaluation));

	/* Find the beginning of the third expression */
	iteration = my_index(evaluation, ',');
	if (!iteration)
	{
		yell("FOR: Only two components!");
		new_free(&commence);
		return;
	}
	do 
	{
		*iteration++ = '\0';
	}
	while (isspace(*iteration));

	working = args;
	while (isspace(*working))
		*working++ = '\0';

	if ((working = next_expr(&working, '{')) == NULL)	/* } */
	{
		yell("FOR: badly formed commands");
		new_free(&commence);
		return;
	}

	malloc_strcpy(&commands, working);

	sa = subargs ? subargs : empty_string();
	parse_line(NULL, commence, sa, 0, 0, 0);

	while (1)
	{
		malloc_strcpy(&lameeval, evaluation);
		line = parse_inline(lameeval, sa, &argsused);
		if (*line && *line != '0')
		{
			new_free(&line);
			parse_line(NULL, commands, sa, 0, 0, 0);
			parse_line(NULL, iteration, sa, 0, 0, 0);
		}
		else break;
	}
	new_free(&line);
	new_free(&lameeval);
	new_free(&commence);
	new_free(&commands);
}

/* fec - iterate over a list of characters */

void
fec(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*pointer;
	u_char	*list = NULL;
	u_char	*var = NULL;
	u_char	stuff[2];
	int	args_flag = 0;
	unsigned display;
	u_char	*sa, *todo;

	list = next_expr(&args, '(');		/* ) */
	if (list == NULL)
	{
		yell ("FEC: Missing List for /FEC");
		return;
	}

	sa = subargs ? subargs : empty_string();
	list = expand_alias(NULL, list, sa, &args_flag, NULL);
	pointer = list;

	var = next_arg(args, &args);
	args = my_index(args, '{');		/* } */

	if ((todo = next_expr(&args, '{')) == NULL)
	{
		yell ("FE: Missing }");
		return;
	}

	stuff[1] = '\0';

	while (*pointer)
	{
		display = set_display_off();
		stuff[0] = *pointer++;
		add_alias(VAR_ALIAS, var, stuff);
		set_display(display);
		parse_line(NULL, todo, 
		    subargs ? subargs : empty_string(), 0, 0, 0);
	}
	display = set_display_off();
	delete_alias(VAR_ALIAS, var);
	set_display(display);

	new_free(&list);
}
