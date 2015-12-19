/*
 * irc.h: header file for all of ircII! 
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
 * @(#)$eterna: irc.h,v 1.122 2014/03/17 18:51:41 mrg Exp $
 */

#ifndef irc__irc_h
#define irc__irc_h

/* we need this early as possible */
#ifdef _AIX
#undef _ALL_SOURCE
#define _ALL_SOURCE 1
#endif

#define IRCII_COMMENT   "propelling cheese in the 21st century and beyond!"

#define IRCRC_NAME "/.ircrc"
#define IRCQUICK_NAME "/.ircquick"

/*
 * Here you can set the in-line quote character, normally backslash, to
 * whatever you want.  Note that we use two backslashes since a backslash is
 * also C's quote character.  You do not need two of any other character.
 */
#define QUOTE_CHAR '\\'

#include "defs.h"
#include "config.h"
#include "socks_compat.h"

#include <stdio.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/param.h>

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif /* HAVE_SYS_TIME_H */
#endif /* TIME_WITH_SYS_TIME */

#ifdef HAVE_SYS_FCNTL_H
# include <sys/fcntl.h>
#endif /* HAVE_SYS_FCNTL_H */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif /* HAVE_FCNTL_H */

#include <unistd.h>

#ifdef HAVE_SYS_FILE_H
# include <sys/file.h>
#endif /* HAVE_SYS_FILE_H */

#ifdef HAVE_NETDB_H
# include <netdb.h>
#endif /* HAVE_NETDB_H */

#ifdef HAVE_PROCESS_H
# include <process.h>
#endif /* HAVE_PROCESS_H */

#ifdef HAVE_TERMCAP_H
# include <termcap.h>
#endif /* HAVE_TERMCAP_H */

#include <stdarg.h>

#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif /* HAVE_SYS_SELECT_H */

#include "irc_std.h"
#include "debug.h"

/* these define what characters do, colour inverse, underline, bold and all off */
#define COLOUR_TAG	'\003'		/* ^C */
#define REV_TOG		'\026'		/* ^V */
#define UND_TOG		'\037'		/* ^_ */
#define BOLD_TOG	'\002'		/* ^B */
#define ALL_OFF		'\017'		/* ^O */
#define FULL_OFF	'\004'		/* internal, should be different than all others */

#define IRCD_BUFFER_SIZE	1024
#define BIG_BUFFER_SIZE		(IRCD_BUFFER_SIZE * 4)

#ifndef INPUT_BUFFER_SIZE
#define INPUT_BUFFER_SIZE	1536
/* INPUT_BUFFER_SIZE:
   irc servers generally accept 512 bytes of input per line.
   Assuming the shortest considerable command begins
   with "PRIVMSG x :" and ends with a linefeed, this
   leaves 500 bytes of space for text.
   In dcc-chats, the line length is unlimited.
   In ircII, the input buffer is utf-8 encoded. This means
   that certain european characters take two bytes of space
   and most asian characters take three bytes of space.
   ( Reference: http://www.utf-8.com/ )
   However, in some encodings that are used in IRC
   (such as ISO-8859-1 and SHIFT-JIS), some to most
   of those characters only take 1 byte of space.
   ( References: http://en.wikipedia.com/wiki/ISO_8859-1
     http://en.wikipeda.com/wiki/SJIS )
   Therefore, to be able to send a full 500-character line
   regardless of what characters it contains, we need 1500
   bytes of free buffer space.
   We also need room for the command used to send the message
   - for example, "/msg bisqwit,#nipponfever,#gurb ".
   For that space, we use an arbitrarily chosen number
   that is bigger than 20.
   As a result, INPUT_BUFFER_SIZE is now set to 1536 (0x600).
                         - Bisqwit
 */
#endif /* INPUT_BUFFER_SIZE */

/*
 * Put these here because we use them at least as pointers all over,
 * for now.
 */
typedef	struct screen_stru Screen;
typedef	struct window_stru Window;

#ifdef notdef
# define DAEMON_UID 1
#endif

#define NAME_LEN 255
#define REALNAME_LEN 50
#define PATH_LEN 1024

#if defined(__hpux) || defined(hpux) || defined(_HPUX_SOURCE)
# undef HPUX
# define HPUX
# define killpg(pgrp,sig) kill(-pgrp,sig)
#endif /* __hpux || hpux || _HPUX_SOURCE */

/*
 * Lame Linux doesn't define X_OK in a non-broken header file, so
 * we define it here.. 
 */
#if !defined(X_OK)
# define X_OK  1
#endif

#ifdef __BORLANDC__
# define F_OK 0
# define W_OK 2
# define R_OK 4
#endif

#ifdef __osf__
# if __osf__
#  define _BSD
# endif /* __osf__ */
#endif /* __osf__ */

/* systems without getwd() can lose, if this dies */
#if defined(NEED_GETCWD)
# define getcwd(b, c)	getwd(b);
#endif /* NEED_GETCWD */

#ifndef MIN
# define MIN(a,b) ((a < b) ? (a) : (b))
#endif /* MIN */

#ifndef MAX
# define MAX(a,b) ((a < b) ? (b) : (a))
#endif /* MAX */

/* flags used by who() and whoreply() for who_mask */
#define WHO_OPS		0x0001
#define WHO_NAME	0x0002
#define WHO_ZERO	0x0004
#define WHO_CHOPS	0x0008
#define WHO_FILE	0x0010
#define WHO_HOST	0x0020
#define WHO_SERVER	0x0040
#define	WHO_HERE	0x0080
#define	WHO_AWAY	0x0100
#define	WHO_NICK	0x0200
#define	WHO_LUSERS	0x0400
#define	WHO_REAL	0x0800

#ifdef ICONV_CONST_ARG2
#define iconv_const const
#else
#define iconv_const
#endif

	void	set_irchost(u_char *);
	void	irc_exit(void);
	void	irc_quit(u_int, u_char *);
	void	on_signal_occurred(int);
	int	irc_port(void);
	int	using_ircio(void);
	u_char	*program_name(void);
	u_char	*irc_version(void);
	time_t	idle_time(void);
	int	client_default_is_icb(void);
	int	use_flow_control(void);
	int	use_termcap_enterexit(void);
	int	use_background_mode(void);
	int	ignore_ircrc(void);
	u_char	*my_path(void);
	u_char	*get_irchost(void);
	u_char	*my_hostname(void);
	u_char	*my_realname(void);
	void	set_realname(u_char *);
	u_char	*my_username(void);
	void	set_username(u_char *);
	u_char	*my_irc_lib(void);
	u_char	*irc_umode(void);
	u_char	*irc_extra_args(void);
	int	term_basic(void);
	u_char	*my_nickname(void);
	void	set_nickname(u_char *);
	u_char	*ircquick_file_path(void);
	u_char	*ircrc_file_path(void);
	void	set_ircrc_file_path(u_char *);
	void	break_io_processing(void);
	u_char	*zero(void);
	u_char	*one(void);
	u_char	*empty_string(void);

	/* reg.h */
	int	wild_match(u_char *, u_char *);

#endif /* irc__irc_h */
