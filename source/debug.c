/*
 * debug.c - generic debug routines.
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
 */

/*
 *
 * void debug(int level,char *format, ...); -- the function to call
 * void setdlevel(u_char *level); 	    -- turn on debugging for "level"
 */

#include "irc.h"		/* This is where DEBUG is defined or not */
IRCII_RCSID("@(#)$eterna: debug.c,v 1.32 2014/10/29 01:36:28 mrg Exp $");

#ifdef DEBUG

# include "debug.h"

static struct {
	int	dnum;
	const u_char *name;
	int	dlevel;
} debug_levels[] = {
	{ DB_SCROLL,		UP("scroll"),		0 },
	{ DB_LOAD,		UP("load"),		0 },
	{ DB_HISTORY,		UP("history"),		0 },
	{ DB_CURSOR,		UP("cursor"),		0 },
	{ DB_IRCIO,		UP("ircio"),		0 },
	{ DB_LASTLOG,		UP("lastlog"),		0 },
	{ DB_WINCREATE,		UP("wincreate"),	0 },
	{ DB_STATUS, 		UP("status"),		0 },
	{ DB_NEWIO, 		UP("newio"),		0 },
	{ DB_SSL, 		UP("ssl"),		0 },
	{ DB_SERVER, 		UP("server"),		0 },
	{ DB_WINDOW, 		UP("window"),		0 },
	{ DB_PROXY, 		UP("proxy"),		0 },
};

void
setdlevel(u_char *level)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(debug_levels); i++)
		if (!my_strcmp(debug_levels[i].name, level))
			debug_levels[i].dlevel = 1;
}

void
debug(int level, char *format, ...)
{
	va_list vlist;
	struct timeval tv;
	struct tm tm_store, *tm;

	if (level > ARRAY_SIZE(debug_levels) ||
	    debug_levels[level].dlevel == 0)
		return;

	if (gettimeofday(&tv, NULL) != 0)
		memset(&tv, 0, sizeof tv);
	tm = localtime(&tv.tv_sec);
	if (!tm)
	{
		memset(&tm_store, 0, sizeof tm_store);
		tm = &tm_store;
	}

	va_start(vlist, format);
	fprintf(stderr, "%04u-%02u-%02u %02u:%02u:%02u.%06u ",
	       1990+tm->tm_year, tm->tm_mon, tm->tm_mday,
	       tm->tm_hour, tm->tm_min, tm->tm_sec, tv.tv_usec);
	vfprintf(stderr, format, vlist);
	fputc('\n', stderr);
	fflush(stderr);
}
#endif /* DEBUG */
