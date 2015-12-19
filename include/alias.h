/*
 * alias.h: header for alias.c 
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
 * @(#)$eterna: alias.h,v 1.32 2014/02/15 14:15:50 mrg Exp $
 */

#ifndef irc__alias_h_
#define irc__alias_h_

#define COMMAND_ALIAS 0
#define VAR_ALIAS 1

#define LEFT_BRACE '{'
#define RIGHT_BRACE '}'
#define LEFT_BRACKET '['
#define RIGHT_BRACKET ']'
#define LEFT_PAREN '('
#define RIGHT_PAREN ')'
#define DOUBLE_QUOTE '"'

	void	add_alias(int, u_char *, u_char *);
	u_char	*get_alias(int, u_char *, int *, u_char **);
	u_char	*expand_alias(u_char *, u_char *, u_char *, int *, u_char **);
	void	execute_alias(u_char *, u_char *, u_char *);
	void	list_aliases(int, u_char *);
	int	mark_alias(u_char *, int);
	void	delete_alias(int, u_char *);
	u_char	**match_alias(u_char *, int *, int);
	void	alias(u_char *, u_char *, u_char *);
	u_char	*parse_inline(u_char *, u_char *, int *);
	u_char	*MatchingBracket(u_char *, int, int);
	void	save_aliases(FILE *, int);
	int	word_count(u_char *);
	u_char	*call_function(u_char *, u_char *, u_char *, int *);

typedef	struct alias_stru Alias;

#define MAX_CMD_ARGS 5

#endif /* irc__alias_h_ */
