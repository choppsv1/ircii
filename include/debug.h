/*
 * debug.h - header file for phone's debug routine..
 *
 * Copyright (c) 1993-2014 Matthew R. Green.
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
 * @(#)$eterna: debug.h,v 1.29 2015/02/22 12:43:18 mrg Exp $
 */

#ifndef irc__debug_h_
# define irc__debug_h_

/* List of debug levels; see debug.c for more details. */
enum {
	DB_SCROLL,
	DB_LOAD,
	DB_HISTORY,
	DB_CURSOR,
	DB_IRCIO,
	DB_LASTLOG,
	DB_WINCREATE,
	DB_STATUS, 
	DB_NEWIO,
	DB_SSL,
	DB_SERVER,
	DB_WINDOW,
	DB_PROXY,
};

/*
 * This depends upon C99 varadic macros.
 */
# ifdef DEBUG
#  define Debug(level, fmt, ...) \
	debug(level, "%s:%d: " fmt, __func__, __LINE__, ##__VA_ARGS__)

	void    debug(int, char *, ...)
			__attribute__((__format__ (__printf__, 2, 3)));
	void	setdlevel(u_char *);

# else
#  define Debug(level, fmt, ...)
# endif /* DEBUG */

#endif /* irc__debug_h_ */
