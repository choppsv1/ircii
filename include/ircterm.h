/*
 * term.h: header file for term.c 
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
 * @(#)$eterna: ircterm.h,v 1.47 2015/09/05 07:37:40 mrg Exp $
 */

#ifndef irc__ircterm_h_
# define irc__ircterm_h_

#ifdef INCLUDE_CURSES_H
# include <curses.h>
#endif /* INCLUDE_CURSES_H */

#ifdef NCURSES_VERSION
# define TPUTSRETVAL int
# define TPUTSARGVAL int
#else
# ifdef HPUX
#  define TPUTSRETVAL int
#  define TPUTSARGVAL char
# else /* HPUX */
#  ifdef __sgi
#   define TPUTSRETVAL int
#   define TPUTSARGVAL char
#  endif
#  if (defined(__sgi) && defined(SEEKLIMIT32)) || defined(__osf__) || defined(__SVR4)
/*
 * if this causes your compile to fail, then just delete it.  and
 * please tell me (use the `ircbug' command).  thanks.
 */
char *tgetstr(char *, char **);
#  endif
#  ifndef __sgi
#   if defined(__linux__) || defined(_AIX) || defined(__GNU__) || defined(__FreeBSD__) || (defined(__NetBSD_Version__) && __NetBSD_Version__ >= 104100000)
#    define TPUTSRETVAL int
#    define TPUTSARGVAL int
#   else
#    define TPUTSVOIDRET 1
#    define TPUTSRETVAL void
#    define TPUTSARGVAL int
#   endif /* __linux || _AIX */
#  endif /* __sgi */
# endif /* HPUX */
#endif /* NCURSES_VERSION */

	TPUTSRETVAL putchar_x(TPUTSARGVAL);

	void	term_cont(int);
	void	term_set_fp(FILE *);
	void	term_init(void);
	int	term_resize(void);
	void	term_pause(u_int, u_char *);
	void	term_flush(void);
	void	term_space_erase(int);
	void	term_reset(void);
	void    copy_window_size(int *, int *);
	void	term_underline_on(void);
	void	term_underline_off(void);
	void	term_standout_on(void);
	void	term_standout_off(void);
	void	term_clear_screen(void);
	void	term_move_cursor(int, int);
	void	term_cr(void);
	void	term_newline(void);
	void	term_beep(void);
	void	term_bold_on(void);
	void	term_bold_off(void);
	int	term_eight_bit(void);
	void	set_term_eight_bit(int);
	void	term_check_refresh(void);

	int	term_scroll(int, int, int);
	int	term_insert(u_int);
	int	term_delete(void);
	int	term_cursor_right(void);
	int	term_cursor_left(void);
	int	term_clear_to_eol(void);

#if defined(_HPUX_SOURCE)

#ifndef SIGWINCH
# define    SIGWINCH    SIGWINDOW
#endif /* SIGWINCH */

#endif /* _HPUX_SOURCE */

/*
 * comment this if you have last-column scroll issues.  this will be
 * removed at some point in the future (2017?)
 */
#define TERM_USE_LAST_COLUMN

#endif /* irc__ircterm_h_ */
