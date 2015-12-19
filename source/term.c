/*
 * term.c: termcap stuff...
 *
 * Written By Michael Sandrof
 * HP-UX modifications by Mark T. Dame (Mark.Dame@uc.edu)
 * Termio modifications by Stellan Klebom (d88-skl@nada.kth.se)
 * Many, many cleanups, modifications, and some new broken code
 * added by Scott Reynolds (scottr@edsi.org), June 1995.
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
IRCII_RCSID("@(#)$eterna: term.c,v 1.117 2015/09/05 07:37:40 mrg Exp $");

#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif /* HAVE_SYS_IOCTL_H */

#include <sys/stat.h>
#include <termios.h>

#ifndef CBREAK
# define CBREAK RAW
#endif

#include "ircterm.h"
#include "translat.h"
#include "window.h"
#include "screen.h"
#include "output.h"

#ifndef	STTY_ONLY
static int	term_CE_clear_to_eol(void);
static int	term_CS_scroll(int, int, int);
static int	term_ALDL_scroll(int, int, int);
static int	term_param_ALDL_scroll(int, int, int);
static int	term_IC_insert(u_int);
static int	term_IMEI_insert(u_int);
static int	term_DC_delete(void);
static int	term_null_function(void);
static int	term_BS_cursor_left(void);
static int	term_LE_cursor_left(void);
static int	term_ND_cursor_right(void);
static void	tputs_x(const char *);
#endif /* STTY_ONLY */


static	int	tty_des;		/* descriptor for the tty */

static struct termios	oldb,
			newb;

#ifndef STTY_ONLY

#if defined(__CYGWIN__) || defined(__CYGWIN32__)
#define TGETENT_BUFSIZ 2048
#else
#define TGETENT_BUFSIZ 1024
#endif

static	char	termcap[TGETENT_BUFSIZ];

/*
 * Function variables: each returns 1 if the function is not supported on the
 * current term type, otherwise they do their thing and return 0
 */
static	int	(*term_scroll_func)(int, int, int);	/* best scroll */
static	int	(*term_insert_func)(u_int);		/* best insert */
static	int	(*term_delete_func)(void);		/* best delete */
static	int	(*term_cursor_left_func)(void);		/* best left */
static	int	(*term_cursor_right_func)(void);	/* best right */
static	int	(*term_clear_to_eol_func)(void);	/* figure it out */

/* The termcap variables */
static	char	*CM,
		*CE,
		*CL,
		*CR,
		*NL,
		*AL,
		*DL,
		*CS,
		*DC,
		*IC,
		*IM,
		*EI,
		*SO,
		*SE,
		*US,
		*UE,
		*MD,
		*ME,
		*SF,
		*SR,
		*ND,
		*LE,
		*BL,
		*TI,
		*TE;
static	int	SG;

/*
 * term_reset_flag: set to true whenever the terminal is reset, thus letting
 * the calling program work out what to do
 */
int	term_reset_flag = 0;

static	FILE	*term_fp = 0;
static	int	li, co;

void
term_set_fp(FILE *fp)
{
	term_fp = fp;
}

/* putchar_x: the putchar function used by tputs */
TPUTSRETVAL
putchar_x(TPUTSARGVAL c)
{
	fputc(c, term_fp);
#ifndef TPUTSVOIDRET	/* what the hell is this value used for anyway? */
	return (0);
#endif
}

void
term_flush(void)
{
	fflush(term_fp);
}

/*
 * term_reset: sets terminal attributed back to what they were before the
 * program started
 */
void
term_reset(void)
{
	tcsetattr(tty_des, TCSADRAIN, &oldb);

	if (CS)
		tputs_x(tgoto(CS, get_li() - 1, 0));
	if (!use_termcap_enterexit() && TE)
		tputs_x(TE);
	term_move_cursor(0, get_li() - 1);
	term_reset_flag = 1;
	term_flush();
}

/*
 * term_cont: sets the terminal back to IRCII stuff when it is restarted
 * after a SIGSTOP.  Somewhere, this must be used in a signal() call
 */
void
term_cont(int signo)
{
	tcsetattr(tty_des, TCSADRAIN, &newb);

	if (!use_termcap_enterexit() && TI)
		tputs_x(TI);

	on_signal_occurred(signo);
}

/*
 * term_pause: sets terminal back to pre-program days, then SIGSTOPs itself.
 */
void
term_pause(u_int key, u_char *ptr)
{
#if !defined(SIGSTOP) || !defined(SIGTSTP)
	say("The STOP_IRC function does not work on this system type.");
#else
	term_reset();
	kill(getpid(), SIGSTOP);
#endif /* !SIGSTOP */
}
#endif /* STTY_ONLY */

/*
 * term_init: does all terminal initialization... reads termcap info, sets
 * the terminal to CBREAK, no ECHO mode.   Chooses the best of the terminal
 * attributes to use ..  for the version of this function that is called for
 * wserv, we set the termial to RAW, no ECHO, so that all the signals are
 * ignored.. fixes quite a few problems...  -phone, jan 1993..
 */
void
term_init(void)
{
#ifndef	STTY_ONLY
	char	bp[TGETENT_BUFSIZ],
		*term,
		*ptr;

	if ((term = getenv("TERM")) == NULL)
	{
		fprintf(stderr, "irc: No TERM variable set!\n");
		fprintf(stderr,"irc: You may still run irc by using the -d switch\n");
		exit(1);
	}
	if (tgetent(bp, term) < 1)
	{
		fprintf(stderr, "irc: No termcap entry for %s.\n", term);
		fprintf(stderr,"irc: You may still run irc by using the -d switch\n");
		exit(1);
	}

	if ((co = tgetnum("co")) == -1)
		co = 80;
	if ((li = tgetnum("li")) == -1)
		li = 24;
	ptr = termcap;

	/*
	 * Thanks to Max Bell (mbell@cie.uoregon.edu) for info about TVI
	 * terminals and the sg terminal capability
	 */
	SG = tgetnum("sg");
	CM = tgetstr("cm", &ptr);
	CL = tgetstr("cl", &ptr);
	if ((CM == NULL) ||
	    (CL == NULL))
	{
		fprintf(stderr, "This terminal does not have the necessary "
				 "capabilities to run IRCII\n"
				 "in full screen mode. You may still run "
				 "irc by using the -d switch\n");
		exit(1);
	}
	if ((CR = tgetstr("cr", &ptr)) == NULL)
		CR = "\r";
	if ((NL = tgetstr("nl", &ptr)) == NULL)
		NL = "\n";

	if ((CE = tgetstr("ce", &ptr)) != NULL)
		term_clear_to_eol_func = term_CE_clear_to_eol;
	else
		term_clear_to_eol_func = term_null_function;

	TE = tgetstr("te", &ptr);
	if (!use_termcap_enterexit() && TE && (TI = tgetstr("ti", &ptr)) != NULL )
		tputs_x(TI);
	else
		TE = TI = NULL;

	/* if ((ND = tgetstr("nd", &ptr)) || (ND = tgetstr("kr", &ptr))) */
	if ((ND = tgetstr("nd", &ptr)) != NULL)
		term_cursor_right_func = term_ND_cursor_right;
	else
		term_cursor_right_func = term_null_function;

	/* if ((LE = tgetstr("le", &ptr)) || (LE = tgetstr("kl", &ptr))) */
	if ((LE = tgetstr("le", &ptr)) != NULL)
		term_cursor_left_func = term_LE_cursor_left;
	else if (tgetflag("bs"))
		term_cursor_left_func = term_BS_cursor_left;
	else
		term_cursor_left_func = term_null_function;

	SF = tgetstr("sf", &ptr);
	SR = tgetstr("sr", &ptr);

	if ((CS = tgetstr("cs", &ptr)) != NULL)
		term_scroll_func = term_CS_scroll;
	else if ((AL = tgetstr("AL", &ptr)) && (DL = tgetstr("DL", &ptr)))
		term_scroll_func = term_param_ALDL_scroll;
	else if ((AL = tgetstr("al", &ptr)) && (DL = tgetstr("dl", &ptr)))
		term_scroll_func = term_ALDL_scroll;
	else
		term_scroll_func = (int (*)(int, int, int)) term_null_function;

	if ((IC = tgetstr("ic", &ptr)) != NULL)
		term_insert_func = term_IC_insert;
	else
	{
		if ((IM = tgetstr("im", &ptr)) && (EI = tgetstr("ei", &ptr)))
			term_insert_func = term_IMEI_insert;
		else
			term_insert_func = (int (*)(u_int)) term_null_function;
	}

	if ((DC = tgetstr("dc", &ptr)) != NULL)
		term_delete_func = term_DC_delete;
	else
		term_delete_func = term_null_function;

	SO = tgetstr("so", &ptr);
	SE = tgetstr("se", &ptr);
	if ((SO == NULL) || (SE == NULL))
	{
		SO = CP(empty_string());
		SE = CP(empty_string());
	}
	US = tgetstr("us", &ptr);
	UE = tgetstr("ue", &ptr);
	if ((US == NULL) || (UE == NULL))
	{
		US = CP(empty_string());
		UE = CP(empty_string());
	}
	MD = tgetstr("md", &ptr);
	ME = tgetstr("me", &ptr);
	if ((MD == NULL) || (ME == NULL))
	{
		MD = CP(empty_string());
		ME = CP(empty_string());
	}
	if ((BL = tgetstr("bl", &ptr)) == NULL)
		BL = "\007";
#endif /* STTY_ONLY */

	if (getenv("IRC_DEBUG")|| (tty_des = open("/dev/tty", O_RDWR, 0)) == -1)
		tty_des = 0;

	tcgetattr(tty_des, &oldb);

	newb = oldb;
	newb.c_lflag &= ~(ICANON | ECHO);	/* set equivalent of
						 * CBREAK and no ECHO
						 */
	newb.c_cc[VMIN] = 1;	/* read() satified after 1 char */
	newb.c_cc[VTIME] = 0;	/* No timer */

#ifndef _POSIX_VDISABLE
# define _POSIX_VDISABLE 0
#endif

	newb.c_cc[VQUIT] = _POSIX_VDISABLE;
#ifdef VDISCARD
	newb.c_cc[VDISCARD] = _POSIX_VDISABLE;
#endif
#ifdef VDSUSP
	newb.c_cc[VDSUSP] = _POSIX_VDISABLE;
#endif
#ifdef VSUSP
	newb.c_cc[VSUSP] = _POSIX_VDISABLE;
#endif

#ifndef STTY_ONLY
	if (!use_flow_control())
		newb.c_iflag &= ~IXON;	/* No XON/XOFF */
#endif /* STTY_ONLY */

	tcsetattr(tty_des, TCSADRAIN, &newb);
}


#ifndef STTY_ONLY
/*
 * term_resize: gets the terminal height and width.  Trys to get the info
 * from the tty driver about size, if it can't... uses the termcap values. If
 * the terminal size has changed since last time term_resize() has been
 * called, 1 is returned.  If it is unchanged, 0 is returned.
 */
int
term_resize(void)
{
	int	new_li = li, new_co = co;

	/*
	 * if we're not the main screen, we've probably arrived here via
	 * the wserv message path, and we should have already setup the
	 * values of "li" and "co".
	 */
	if (is_main_screen(get_current_screen()))
	{
#ifdef TIOCGWINSZ
		struct	winsize window;

		if (ioctl(tty_des, TIOCGWINSZ, &window) == 0)
		{
			new_li = window.ws_row;
			new_co = window.ws_col;
		}
#endif /* TIOCGWINSZ */
#ifndef TERM_USE_LAST_COLUMN
		new_co--;
#endif
	}
	return screen_set_size(new_li, new_co);
}

/*
 * term_null_function: used when a terminal is missing a particulary useful
 * feature, such as scrolling, to warn the calling program that no such
 * function exists
 */
static	int
term_null_function(void)
{
	return (1);
}

/* term_CE_clear_to_eol(): the clear to eol function, right? */
static	int
term_CE_clear_to_eol(void)
{
	tputs_x(CE);
	return (0);
}

/* * term_space_erase: this can be used if term_CE_clear_to_eol() returns 1.
 * This will erase from x to the end of the screen uses space.  Actually, it
 * doesn't reposition the cursor at all, so the cursor must be in the correct
 * spot at the beginning and you must move it back afterwards
 */
void
term_space_erase(int x)
{
	int	i,
		cnt;

	cnt = get_co() - x;
	for (i = 0; i < cnt; i++)
		fputc(' ', term_fp);
}

/*
 * term_CS_scroll: should be used if the terminal has the CS capability by
 * setting term_scroll equal to it
 */
static	int
term_CS_scroll(int line1, int line2, int n)
{
	int	i;
	u_char	*thing;

	if (n > 0)
		thing = UP(SF ? SF : NL);
	else if (n < 0)
	{
		if (SR)
			thing = UP(SR);
		else
			return 1;
	}
	else
		return 0;
	tputs_x(tgoto(CS, line2, line1));  /* shouldn't do this each time */
	if (n < 0)
	{
		term_move_cursor(0, line1);
		n = -n;
	}
	else
		term_move_cursor(0, line2);
	for (i = 0; i < n; i++)
		tputs_x(CP(thing));
	tputs_x(tgoto(CS, get_li() - 1, 0));	/* shouldn't do this each time */
	return (0);
}

/*
 * term_ALDL_scroll: should be used for scrolling if the term has AL and DL
 * by setting the term_scroll function to it
 */
static	int
term_ALDL_scroll(int line1, int line2, int n)
{
	int	i;

	if (n > 0)
	{
		term_move_cursor(0, line1);
		for (i = 0; i < n; i++)
			tputs_x(DL);
		term_move_cursor(0, line2 - n + 1);
		for (i = 0; i < n; i++)
			tputs_x(AL);
	}
	else if (n < 0)
	{
		n = -n;
		term_move_cursor(0, line2-n+1);
		for (i = 0; i < n; i++)
			tputs_x(DL);
		term_move_cursor(0, line1);
		for (i = 0; i < n; i++)
			tputs_x(AL);
	}
	return (0);
}

/*
 * term_param_ALDL_scroll: Uses the parameterized version of AL and DL
 */
static	int
term_param_ALDL_scroll(int line1, int line2, int n)
{
	if (n > 0)
	{
		term_move_cursor(0, line1);
		tputs_x(tgoto(DL, n, n));
		term_move_cursor(0, line2 - n + 1);
		tputs_x(tgoto(AL, n, n));
	}
	else if (n < 0)
	{
		n = -n;
		term_move_cursor(0, line2-n+1);
		tputs_x(tgoto(DL, n, n));
		term_move_cursor(0, line1);
		tputs_x(tgoto(AL, n, n));
	}
	return (0);
}

/*
 * term_IC_insert: should be used for character inserts if the term has IC by
 * setting term_insert to it.
 */
static	int
term_IC_insert(u_int c)
{
	tputs_x(IC);
	putchar_x(c);
	return (0);
}

/*
 * term_IMEI_insert: should be used for character inserts if the term has IM
 * and EI by setting term_insert to it
 */
static	int
term_IMEI_insert(u_int c)
{
	tputs_x(IM);
	putchar_x(c);
	tputs_x(EI);
	return (0);
}

/*
 * term_DC_delete: should be used for character deletes if the term has DC by
 * setting term_delete to it
 */
static	int
term_DC_delete(void)
{
	tputs_x(DC);
	return (0);
}

/* term_ND_cursor_right: got it yet? */
static	int
term_ND_cursor_right(void)
{
	tputs_x(ND);
	return (0);
}

/* term_LE_cursor_left:  shouldn't you move on to something else? */
static	int
term_LE_cursor_left(void)
{
	tputs_x(LE);
	return (0);
}

static	int
term_BS_cursor_left(void)
{
	fputc('\010', term_fp);
	return (0);
}

void
copy_window_size(int *nlines, int *cols)
{
	if (nlines)
		*nlines = get_li();
	if (cols)
		*cols = get_co();
}

static void
tputs_x(const char *s)
{
	tputs(s, 0, putchar_x);
}

void
term_underline_on(void)
{
	tputs_x(US);
}

void
term_underline_off(void)
{
	tputs_x(UE);
}

void
term_standout_on(void)
{
	tputs_x(SO);
}

void
term_standout_off(void)
{
	tputs_x(SE);
}

void
term_clear_screen(void)
{
	tputs_x(CL);
}

void
term_move_cursor(int c, int r)
{
	tputs_x(tgoto(CM, c, r));
}

void
term_cr(void)
{
	tputs_x(CR);
}

void
term_newline(void)
{
	tputs_x(NL);
}

void
term_beep(void)
{
	tputs_x(BL);
	Screen *screen = get_current_screen();

	fflush(screen ? screen_get_fpout(screen) : stdout);
}

void
term_bold_on(void)
{
	tputs_x(MD);
}

void
term_bold_off(void)
{
	tputs_x(ME);
}

int
term_eight_bit(void)
{
	return (((oldb.c_cflag) & CSIZE) == CS8) ? 1 : 0;
}
#endif /* STTY_ONLY */

void
set_term_eight_bit(int value)
{
#ifndef	STTY_ONLY
	if (term_basic())
		return;
#endif /* STTY_ONLY */
	if (value == ON)
	{
		newb.c_cflag |= CS8;
		newb.c_iflag &= ~ISTRIP;
	}
	else
	{
		newb.c_cflag &= ~CS8;
		newb.c_iflag |= ISTRIP;
	}
	tcsetattr(tty_des, TCSADRAIN, &newb);
}

#ifndef	STTY_ONLY
void
term_check_refresh(void)
{
	if (term_reset_flag)
	{
		refresh_screen(0, NULL);
		term_reset_flag = 0;
	}
}

int
term_scroll(int line1, int line2, int n)
{
	return (*term_scroll_func)(line1, line2, n);
}

int
term_insert(u_int c)
{
	return (*term_insert_func)(c);
}

int
term_delete(void)
{
	return (*term_delete_func)();
}

int
term_cursor_right(void)
{
	return (*term_cursor_right_func)();
}

int
term_cursor_left(void)
{
	return (*term_cursor_left_func)();
}

int
term_clear_to_eol(void)
{
	return (*term_clear_to_eol_func)();
}
#endif /* STTY_ONLY */
