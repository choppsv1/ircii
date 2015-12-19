/*
 * list.c: some generic linked list managing stuff 
 *
 * Written By Michael Sandrof
 *
 * Copyright (c) 1990 Michael Sandrof.
 * Copyright (c) 1991, 1992 Troy Rollo.
 * Copyright (c) 1992-2013 Matthew R. Green.
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
IRCII_RCSID("@(#)$eterna: list.c,v 1.23 2013/12/30 16:42:38 mrg Exp $");

#include "list.h"
#include "ircaux.h"

static	int	add_list_strcmp(List *, List *);
static	int	list_strcmp(List *, u_char *);
static	int	list_match(List *, u_char *);

/*
 * These have now been made more general. You used to only be able to
 * order these lists by alphabetical order. You can now order them
 * arbitrarily. The functions are still called the same way if you
 * wish to use alphabetical order on the key string, and the old
 * function name now represents a stub function which calls the
 * new with the appropriate parameters.
 *
 * The new function name is the same in each case as the old function
 * name, with the addition of a new parameter, cmp_func, which is
 * used to perform comparisons.
 *
 */

static	int
add_list_strcmp(List *item1, List *item2)
{
	return my_stricmp(item1->name, item2->name);
}

static	int
list_strcmp(List *item1, u_char *str)
{
	return my_stricmp(item1->name, str);
}

static	int
list_match(List *item1, u_char *str)
{
	return wild_match(item1->name, str);
}

/*
 * add_to_list: This will add an element to a list.  The requirements for the
 * list are that the first element in each list structure be a pointer to the
 * next element in the list, and the second element in the list structure be
 * a pointer to a character (u_char *) which represents the sort key.  For
 * example 
 *
 * struct my_list{ struct my_list *next; u_char *name; <whatever else you want>}; 
 *
 * The parameters are:  "list" which is a pointer to the head of the list. "add"
 * which is a pre-allocated element to be added to the list.  
 */
void
add_to_list_ext(List **list, List *add, int (*cmp_func)(List *, List *))
{
	List	*tmp,
		*last;

	if (!cmp_func)
		cmp_func = add_list_strcmp;
	last = NULL;
	for (tmp = *list; tmp; tmp = tmp->next)
	{
		if (cmp_func(tmp, add) > 0)
			break;
		last = tmp;
	}
	if (last)
		last->next = add;
	else
		*list = add;
	add->next = tmp;
}

void
add_to_list(List **list, List *add)
{
	add_to_list_ext(list, add, NULL);
}


/*
 * find_in_list: This looks up the given name in the given list.  List and
 * name are as described above.  If wild is true, each name in the list is
 * used as a wild card expression to match name... otherwise, normal matching
 * is done 
 */
List	*
find_in_list_ext(List **list, u_char *name, int wild, int (*cmp_func)(List *, u_char *))
{
	List	*tmp;
	int	best_match,
		current_match;

	if (!cmp_func)
		cmp_func = wild ? list_match : list_strcmp;
	best_match = 0;

	if (wild)
	{
		List	*match = NULL;

		for (tmp = *list; tmp; tmp = tmp->next)
		{
			if ((current_match = cmp_func(tmp, name)) > best_match)
			{
				match = tmp;
				best_match = current_match;
			}
		}
		return (match);
	}
	else
	{
		for (tmp = *list; tmp; tmp = tmp->next)
			if (cmp_func(tmp, name) == 0)
				return (tmp);
	}
	return (NULL);
}

List	*
find_in_list(List **list, u_char *name, int wild)
{
	return find_in_list_ext(list, name, wild, NULL);
}

/*
 * remove_from_list: this remove the given name from the given list (again as
 * described above).  If found, it is removed from the list and returned
 * (memory is not deallocated).  If not found, null is returned. 
 */
List	*
remove_from_list_ext(List **list, u_char *name, int (*cmp_func)(List *, u_char *))
{
	List	*tmp,
		*last;

	if (!cmp_func)
		cmp_func = list_strcmp;
	last = NULL;
	for (tmp = *list; tmp; tmp = tmp->next)
	{
		if (cmp_func(tmp, name) == 0)
		{
			if (last)
				last->next = tmp->next;
			else
				*list = tmp->next;
			return (tmp);
		}
		last = tmp;
	}
	return (NULL);
}

List	*
remove_from_list(List **list, u_char *name)
{
	return remove_from_list_ext(list, name, NULL);
}


/*
 * list_lookup: this routine just consolidates remove_from_list and
 * find_in_list.  I did this cause it fit better with some already existing
 * code 
 */
List	*
list_lookup_ext( List **list, u_char *name, int wild, int delete, int (*cmp_func)(List *, u_char *))
{
	List	*tmp;

	if (delete)
		tmp = remove_from_list_ext(list, name, cmp_func);
	else
		tmp = find_in_list_ext(list, name, wild, cmp_func);
	return (tmp);
}

List	*
list_lookup(List **list, u_char *name, int wild, int delete)
{
	return list_lookup_ext(list, name, wild, delete, NULL);
}
