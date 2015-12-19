/*
 * log.c: handles the irc session logging functions 
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
IRCII_RCSID("@(#)$eterna: log.c,v 1.36 2015/11/20 09:23:54 mrg Exp $");

#include <sys/stat.h>

#include "log.h"
#include "vars.h"
#include "output.h"
#include "ircaux.h"

static	FILE	*irclog_fp;

void
do_log(int flag, u_char *logfile, FILE **fpp)
{
	time_t	t;

	if (logfile == NULL)
	{
		*fpp = NULL;
		return;
	}
	t = time(0);
	if (flag)
	{
		if (*fpp)
			say("Logging is already on");
		else
		{
#ifdef DAEMON_UID
			if (getuid() == DAEMON_UID)
			{
				say("You are not permitted to use LOG");
				/* *fpp = NULL;  unused */
			}
			else
#endif /* DAEMON_UID */
			{
				say("Starting logfile %s", logfile);
				if ((*fpp = fopen(CP(logfile), "a")) != NULL)
				{
#ifdef NEED_FCHMOD
					chmod(logfile, S_IREAD | S_IWRITE);
#else
					fchmod(fileno(*fpp),S_IREAD | S_IWRITE);
#endif /* NEED_FCHMOD */
					fprintf(*fpp, "IRC log started %.16s\n",
							ctime(&t));
					fflush(*fpp);
				}
				else
				{
					say("Couldn't open logfile %s: %s",
						logfile, strerror(errno));
					*fpp = NULL;
				}
			}
		}
	}
	else
	{
		if (*fpp)
		{
			fprintf(*fpp, "IRC log ended %.16s\n", ctime(&t));
			fflush(*fpp);
			fclose(*fpp);
			*fpp = NULL;
			say("Logfile ended");
		}
	}
	return;
}

/* logger: if flag is 0, logging is turned off, else it's turned on */
void
logger(int flag)
{
	u_char	*logfile;

	if ((logfile = get_string_var(LOGFILE_VAR)) == NULL)
	{
		say("You must set the LOGFILE variable first!");
		set_int_var(LOG_VAR, 0);
		return;
	}
	do_log(flag, logfile, &irclog_fp);
	if ((irclog_fp == NULL) && flag)
		set_int_var(LOG_VAR, 0);
}

/*
 * set_log_file: sets the log file name.  If logging is on already, this
 * closes the last log file and reopens it with the new name.  This is called
 * automatically when you SET LOGFILE. 
 */
void
set_log_file(u_char *filename)
{
	u_char	*expanded;

	if (filename)
	{
		if (my_strcmp(filename, get_string_var(LOGFILE_VAR)))
			expanded = expand_twiddle(filename);
		else
			expanded = expand_twiddle(get_string_var(LOGFILE_VAR));
		set_string_var(LOGFILE_VAR, expanded);
		new_free(&expanded);
		if (irclog_fp)
		{
			logger(0);
			logger(1);
		}
	}
}

/*
 * add_to_log: add the given line to the log file.  If no log file is open
 * this function does nothing. 
 */
void
add_to_log(FILE *fp, u_char *line)
{
	if (fp == NULL)
		fp = irclog_fp;

	if (fp)
	{
		fprintf(fp, "%s\n", line);
		fflush(fp);
	}
}
