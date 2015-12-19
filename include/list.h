/*
 * list.h: header for list.c 
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
 *
 * @(#)$eterna: list.h,v 1.20 2014/02/05 07:20:11 mrg Exp $
 */

#ifndef irc__list_h_
#define irc__list_h_

/*
 * List: your list structure must start with these members:
 *	pointer to your next member
 *	u_char pointer to your name
 */
typedef	struct	list_stru List;
struct	list_stru
{
	List	*next;
	u_char	*name;
};

	void	add_to_list(List **, List *);
	List	*find_in_list(List **, u_char *, int);
	List	*remove_from_list(List **, u_char *);
	List	*list_lookup_ext(List **, u_char *, int, int, int (*)(List *, u_char *));
	List	*list_lookup(List **, u_char *, int, int);
	List	*remove_from_list_ext(List **, u_char *, int (*)(List *, u_char *));
	void	add_to_list_ext(List **, List *, int (*)(List *, List *));
	List	*find_in_list_ext(List **, u_char *, int, int (*)(List *, u_char *));

#define REMOVE_FROM_LIST 1
#define USE_WILDCARDS 1

#endif /* irc__list_h_ */
