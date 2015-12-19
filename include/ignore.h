/*
 * ignore.h: header for ignore.c 
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
 * @(#)$eterna: ignore.h,v 1.23 2014/03/02 05:40:35 mrg Exp $
 */

#ifndef irc__ignore_h_
#define irc__ignore_h_

/* declared in ignore.c */
	int	is_ignored(u_char *, int);
	int	ignore_combo(int, int);
	int	double_ignore(u_char *, u_char *, int);
	void	ignore(u_char *, u_char *, u_char *);
	int	get_ignore_type(u_char *);
	u_char	*ignore_list_string(u_char *);
	void	save_ignore(FILE *);
	u_char	*highlight_str(void);
	int	ignore_usernames(void);

/* Type of ignored nicks */
#define IGNORE_MSGS	0x0001
#define IGNORE_PUBLIC	0x0002
#define IGNORE_WALLS	0x0004
#define IGNORE_WALLOPS	0x0008
#define IGNORE_INVITES	0x0010
#define IGNORE_NOTICES	0x0020
#define IGNORE_NOTES	0x0040
#define IGNORE_CTCPS	0x0080
#define IGNORE_CRAP	0x0100
#define IGNORE_ALL (IGNORE_MSGS | IGNORE_PUBLIC | IGNORE_WALLS | \
	IGNORE_WALLOPS | IGNORE_INVITES | IGNORE_NOTICES | IGNORE_NOTES | \
	IGNORE_CTCPS | IGNORE_CRAP)

#define IGNORED 1
#define DONT_IGNORE 2
#define HIGHLIGHTED -1

#endif /* irc__ignore_h_ */
