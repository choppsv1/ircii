/*
 * ircaux.h: header file for ircaux.c 
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
 * @(#)$eterna: ircaux.h,v 1.36 2014/08/06 19:49:00 mrg Exp $
 */

#ifndef irc__ircaux_h_
#define irc__ircaux_h_

#include <stdio.h>

	u_char	*next_arg(u_char *, u_char **);
	u_char	*new_next_arg(u_char *, u_char **);
	u_char	*expand_twiddle(u_char *);
	u_char	*upper(u_char *);
	u_char	*lower(u_char *);
	u_char	*sindex(u_char *, u_char *);
	u_char	*srindex(u_char *, u_char *);
	char	*rfgets(char *, int, FILE *);
	u_char	*path_search(u_char *, u_char *);
	u_char	*double_quote(u_char *, u_char *);
	void	*new_malloc(size_t);
	void	*new_realloc(void *, size_t);
	void	malloc_strcpy(u_char **, u_char *);
	void	malloc_strncpy(u_char **, u_char *, size_t);
	void	malloc_strcat(u_char **, u_char *);
	void	malloc_strncat(u_char **, u_char *, size_t);
	void	malloc_strcat_ue(u_char **, u_char *);
	void	new_free(void *);
	void	wait_new_free(u_char **);
	FILE	*zcat(u_char *);
	int	is_number(u_char *);
	int	connect_by_number(int, u_char *, int, struct addrinfo **, struct addrinfo **);
	int	my_stricmp(const u_char *, const u_char *);
	int	my_strnicmp(const u_char *, const u_char *, size_t);
	int	set_non_blocking(int);
	int	set_blocking(int);
	int	scanstr(u_char *, u_char *);
	void	really_free(int);
	void	strmcpy(u_char *, u_char *, size_t);
	void	strmcat(u_char *, u_char *, size_t);
	void	strmcat_ue(u_char *, u_char *, size_t);

#ifndef SA_LEN
# ifdef HAVE_SOCKADDR_SA_LEN
#  define SA_LEN(x)	(x)->sa_len
# else
#  ifdef SIN6_LEN
#   define SA_LEN(x) SIN6_LEN(x)
#  else
#   ifdef SIN_LEN
#    define SA_LEN(x) SIN_LEN(x)
#   else
#    define SA_LEN(x) sizeof(*x)
#   endif
#  endif
# endif
#endif

#endif /* irc__ircaux_h_ */
