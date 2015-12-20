/*
 * output.c: handles a variety of tasks dealing with the output from the irc
 * program 
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
IRCII_RCSID("@(#)$eterna: output.c,v 1.78 2015/11/20 09:23:54 mrg Exp $");

#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif /* HAVE_SYS_IOCTL_H */

#ifdef HAVE_ICONV_H
# include <iconv.h>
#endif /* HAVE ICONV_H */

#include "output.h"
#include "vars.h"
#include "input.h"
#include "ircterm.h"
#include "ircaux.h"
#include "lastlog.h"
#include "window.h"
#include "screen.h"
#include "hook.h"
#include "ctcp.h"
#include "log.h"
#include "alias.h"
#include "translat.h"
#include "buffer.h"

static	void	display_text(const u_char *, size_t);
static	void	display_nonshift(void);

#ifdef NEED_PUTBUF_DECLARED
/*
 * put_it has to be reentrant - this works because the text is used before
 * it's overwritten by the reentering, but it's not The Right Thing ...
 */
static	u_char	putbuf[4*BIG_BUFFER_SIZE] = "";
#endif

static	int	in_help = 0;

/* value for underline mode, is 0 when on!  -lynx */
static	int	underline = 1;

/*
 * window_display: this controls the display, 1 being ON, 0 being OFF.
 * The DISPLAY var sets this. 
 */
static	unsigned window_display = 1;

/*
 * refresh_screen: Whenever the REFRESH_SCREEN function is activated, this
 * swoops into effect 
 */
void
refresh_screen(u_int key, u_char *ptr)
{
	term_clear_screen();
	if (term_resize())
#if 1
		balance_windows();
#else
		recalculate_windows();
#endif
	else
		redraw_all_windows();
	update_all_windows();
	update_input(UPDATE_ALL);
}

/* init_windows:  */
void
init_screen(void)
{
	new_window();
	term_init();
	term_clear_screen();
	term_resize();
	recalculate_windows();
	update_all_windows();
	init_input();
	term_move_cursor(0, 0);
}

/* put_file: uses put_it() to display the contents of a file to the display */
void
put_file(u_char *filename)
{
	FILE	*fp;
	char	line[1024];		/* too big?  too small?  who cares? */
	size_t	len;

	if ((fp = fopen(CP(filename), "r")) != NULL)
	{
		while (fgets(line, 1024, fp))
		{
			if (*line)
			{
				if ((len = my_strlen(line)))
				{
					if (*(line + len - 1) == '\n')
						*(line + len - 1) = (u_char) 0;
				}
				put_it("%s", line);
			}
			else
				put_it(" ");
		}
		fclose(fp);
	}
}

/*
 * put_it: the irc display routine.  Use this routine to display anything to
 * the main irc window.  It handles sending text to the display or stdout as
 * needed, add stuff to the lastlog and log file, etc.  Things NOT to do:
 * Dont send any text that contains \n, very unpredictable.  Tabs will also
 * screw things up.  The calling routing is responsible for not overwriting
 * the 1K buffer allocated.  
 */
void
put_it(const char *format, ...)
{
	va_list vl;

	if (get_display())
	{
		PUTBUF_INIT
		va_start(vl, format);
		PUTBUF_SPRINTF(format, vl)
		va_end(vl);
		add_to_screen(putbuf);
		PUTBUF_END
	}
}

/* This is an alternative form of put_it which writes three asterisks
 * before actually putting things out.
 */
void
say(const char *format, ...)
{
	va_list vl;

	if (get_display())
	{
		u_char *fmt = NULL;
		PUTBUF_INIT

		if (get_int_var(SHOW_STARS_VAR))
		{
			u_char *s;
			int flag = 0;

			s = expand_alias(NULL,
			    get_string_var(STAR_PREFIX_VAR),
			    empty_string(), &flag, NULL);

			if (s)
			{
				malloc_strcpy(&fmt, s);
				new_free(&s);
			}
			malloc_strcat(&fmt, UP(format));
			format = CP(fmt);
		}
		va_start(vl, format);
		PUTBUF_SPRINTF(format, vl)
		va_end(vl);
		add_to_screen(putbuf);
		PUTBUF_END
		if (fmt)
			new_free(&fmt);
	}
}

void
yell(const char *format, ...)
{
	va_list vl;
	PUTBUF_INIT

	va_start(vl, format);
	PUTBUF_SPRINTF(format, vl)
	va_end(vl);
	add_to_screen(putbuf);
	PUTBUF_END
}


/* help_put_it: works just like put_it, but is specially used by help */
void
help_put_it(const u_char *topic, char *format, ...)
{
	va_list vl;
	PUTBUF_INIT
	int	lastlog_level;
	va_start(vl, format);
	PUTBUF_SPRINTF(format, vl)
	va_end(vl);

	in_help = 1;
	lastlog_level = set_lastlog_msg_level(LOG_HELP);
	if (do_hook(HELP_LIST, "%s %s", topic, putbuf))
	{
		if (get_display())
		{
			add_to_screen(putbuf);
		}
	}
	(void) set_lastlog_msg_level(lastlog_level);
	in_help = 0;
	PUTBUF_END
}

/* display_highlight: turns off and on the display highlight.  */
u_char
display_highlight(int flag)
{
	static	int	highlight = OFF;

	term_flush();
	if (flag == highlight)
		return (flag);
	switch (flag)
	{
	case ON:
		highlight = ON;
		if (get_int_var(INVERSE_VIDEO_VAR))
			term_standout_on();
		return (OFF);
	case OFF:
		highlight = OFF;
		if (get_int_var(INVERSE_VIDEO_VAR))
			term_standout_off();
		return (ON);
	case TOGGLE:
		if (highlight == ON)
		{
			highlight = OFF;
			if (get_int_var(INVERSE_VIDEO_VAR))
				term_standout_off();
			return (ON);
		}
		else
		{
			highlight = ON;
			if (get_int_var(INVERSE_VIDEO_VAR))
				term_standout_on();
			return (OFF);
		}
	}
	return flag;
}

/* display_bold: turns off and on the display bolding.  */
u_char
display_bold(int flag)
{
	static	int	bold = OFF;

	term_flush();
	if (flag == bold)
		return (flag);
	switch (flag)
	{
	case ON:
		bold = ON;
		if (get_int_var(BOLD_VIDEO_VAR))
			term_bold_on();
		return (OFF);
	case OFF:
		bold = OFF;
		if (get_int_var(BOLD_VIDEO_VAR))
			term_bold_off();
		return (ON);
	case TOGGLE:
		if (bold == ON)
		{
			bold = OFF;
			if (get_int_var(BOLD_VIDEO_VAR))
				term_bold_off();
			return (ON);
		}
		else
		{
			bold = ON;
			if (get_int_var(BOLD_VIDEO_VAR))
				term_bold_on();
			return (OFF);
		}
	}
	return OFF;
}

/* display_colours sets the foreground and background colours of the display
 */
void
display_colours(int fgcolour, int bgcolour, int bold, int underline, int high)
{
	if (get_int_var(COLOUR_VAR))
	{
		/* Some people will say that I should use termcap values but
		 * since:
		 * 1- iso 6429 is the only used way for colour in the unix
		 *    realm for now
		 * 2- colour information in termcap entries is still rare
		 * 3- information about colour information in termcap entries
		 *    is still rare too
		 * ... I'll stick with this way for now. But having only 8-9
		 * colour is a pity.
		 *    -- Sarayan
		 */
		 
		/* Written by Bisqwit (bisqwit@iki.fi) */
		 
		/* mirc colours -> iso 6469 colours translation tables */
		static const u_char trans[] = "7042115332664507";
		static const u_char bolds[] = "1000100011011110";
		                            /* 0123456789ABCDEF */

		/*
		 * We use AIX codes (fg: 9x, bg 10x for bright colors
		 * to a allow for all 16 colors. It's not 6429 ECMA
		 * standard, but then we aren't using termcap/terminfo
		 * either, are we?
		 *
		 * Useful Colors ANSI <-> mIRC
		 *
		 * | Value | mIRC Colour | Value  | ANSI 8/16-color |
		 * |-------+-------------+--------+-----------------|
		 * |     0 | Light Gray  | 15 (7) | White           |
		 * |     1 | White       | 0      | Black           |
		 * |     2 | Light Red   | 4      | Blue            |
		 * |     3 | Blue        | 2      | Green           |
		 * |     4 | Light Green | 9 (1)  | Light Red       |
		 * |     5 | Black       | 1      | Red             |
		 * |     6 | Brown       | 5      | Magenta         |
		 * |     7 | Green       | 3      | Yellow          |
		 * |     8 | Light Cyan  | 11 (3) | Light Yellow    |
		 * |     9 | Cyan        | 10 (2) | Light Green     |
		 * |    10 | Purple      | 6      | Cyan            |
		 * |    11 | Gray        | 14 (6) | Light Cyan      |
		 * |    12 | Light Blue  | 12 (4) | Light Blue      |
		 * |    13 | Pink        | 13 (5) | Light Magenta   |
		 * |    14 | Yellow      | 8 (0)  | Dark Gray       |
		 * |    15 | Orange      | 7      | White/Gray      |
		 *
		 * Longest format stirng:
		 *
		 * e[0;1;5;7;38:2:RRR:GGG:BBB;48:2:RRR:GGG:BBBm
		 * 012345678901234567890123456789012345678901234
		 *   0         1         2         3         4
		 *
		 * - chopps
		 */
		u_char iso[45] = "\33[0";
		int len = 3;

		if (bold && get_int_var(BOLD_VIDEO_VAR)) {
			iso[len++] = ';';
			iso[len++] = '1';
		}
		if (underline && get_int_var(UNDERLINE_VIDEO_VAR)) {
			iso[len++] = ';';
			iso[len++] = '4';
		}
		if (high && get_int_var(INVERSE_VIDEO_VAR)) {
			iso[len++] = ';';
			iso[len++] = '7';
		}
		if (fgcolour >= 0 && fgcolour < 256) {
			if (fgcolour < 16) {
				iso[len++] = ';';
				if (bolds[fgcolour] == '1')
					iso[len++] = '9';
				else
					iso[len++] = '3';
				iso[len++] = trans[fgcolour];
			} else {
				/* ITU T.416 (use ; instead of : * for legacy */
				len += sprintf(CP(&iso[len]), ";38;5;%d",
				    fgcolour);
			}
		}
		if (bgcolour >= 0 && bgcolour < 256) {
			if (bgcolour < 16) {
				iso[len++] = ';';
				if (bolds[bgcolour] == '0')
					iso[len++] = '4';
				else {
					iso[len++] = '1';
					iso[len++] = '0';
				}
				iso[len++] = trans[bgcolour];
			} else {
				/* ITU T.416 (use ; instead of : * for legacy */
				len += sprintf(CP(&iso[len]), ";48;5;%d",
				    bgcolour);
			}
		}
		iso[len++] = 'm';
		fwrite(CP(iso), len, 1,
		    screen_get_fpout(get_current_screen()));
	}
}

static void
display_text(const u_char *ustr, size_t length)
{
	iconv_const char *str = (iconv_const char *)ustr;

	if (length > 0)
	{
#ifdef HAVE_ICONV_OPEN
		static iconv_t converter = NULL;

		if (current_display_encoding())
 		{
			if (!str)
			{
				/* str = NULL means reinitialize iconv. */
				if (converter)
				{
					iconv_close(converter);
					converter = NULL;
				}
				return;
			}
			if (!converter)
			{
				converter = iconv_open(CP(current_display_encoding()), "UTF-8");
				if (converter == (iconv_t)(-1))
				{
					iconv_close(converter);
					converter = NULL;
				}
			}
		}

		if (converter)
		{
			char final = 0;
			while (!final)
			{
				char OutBuf[512], *outptr = OutBuf;
				size_t outsize = sizeof OutBuf;
				size_t retval = 0;

				if (length <= 0)
				{
					/* Reset the converter, create a reset-sequence */
					retval = iconv(converter,
					               NULL,    &length,
					               &outptr, &outsize);
					final = 1;
				}
				else
				{
					retval = iconv(converter,
					               &str, &length,
					               &outptr, &outsize);
				}

				/* Write out as much as we got */
				fwrite(OutBuf, sizeof(OutBuf)-outsize, 1, screen_get_fpout(get_current_screen()));

				if (retval == (size_t)-1)
				{
					switch (errno) {
					case E2BIG:
						/* Outbuf could not contain everything. */
						/* Try again with a new buffer. */
						continue;
					case EILSEQ:
						/* Ignore 1 illegal byte silently. */
						if (length > 0)
						{
							++str;
							--length;
							continue;
						}
						/* FALLTHROUGH */
					default:
						/* Input was terminated with a partial byte. */
						/* Ignore the error silently. */
						length = 0;
					}
				}
			}
			return;
		}
#endif /* HAVE_ICONV_OPEN */
		/* No usable iconv, assume output must be ISO-8859-1 */
		if (str != NULL)
		{
			char OutBuf[1024], *outptr=OutBuf;
			/* Convert the input to ISO-8859-1 character
			 * by character, flush to fpout in blocks.
			 * Ignore undisplayable characters silently.
			 */
			while (*str != '\0' && length > 0)
			{
				unsigned len    = calc_unival_length(UP(str));
				unsigned unival;

				if (!len)
				{
				    /* ignore illegal byte (shouldn't happen) */
					++str;
					continue;
				}
				if (len > length)
					break;
				length -= len;
				
				unival = calc_unival(UP(str));
				if (displayable_unival(unival, NULL))
				{
					if (outptr >= OutBuf+sizeof(OutBuf))
					{
						/* flush a block */
						fwrite(OutBuf, outptr-OutBuf, 1,
						       screen_get_fpout(get_current_screen()));
						outptr = OutBuf;
					}
					*outptr++ = unival;
				}
				str += len;
			}
			if (outptr > OutBuf)
			{
				/* Flush the last block */
				fwrite(OutBuf, outptr-OutBuf, 1,
				       screen_get_fpout(get_current_screen()));
			}
		}
	}
}

static void
display_nonshift(void)
{
	display_text(NULL, 1);
}

/*
 * output_line prints the given string at the current screen position,
 * performing adjustments for ^_, ^B, ^V, and ^O
 * If the input is longer than line may be, it cuts it.
 * Return value: Number of columns printed
 */
int
output_line(const u_char *str, int startpos)
{
	int     fgcolour_user = get_int_var(FOREGROUND_COLOUR_VAR),
		bgcolour_user = get_int_var(BACKGROUND_COLOUR_VAR);
	static	int	high = OFF,
			bold = OFF,
			fgcolour = -1,
			bgcolour = -1;
	int	rev_tog, und_tog, bld_tog, all_off;
	int     dobeep = 0;
	int     written = 0;
	int	first = 1;

	if (fgcolour == -1)
		fgcolour = fgcolour_user;
	if (bgcolour == -1)
		bgcolour = bgcolour_user;

	display_highlight(high);
	display_bold(bold);
	display_colours(fgcolour, bgcolour, bold, !underline, high);
	/* do processing on the string, handle inverse and bells */
	display_nonshift();
	while (*str)
	{
		switch (*str)
		{
		case REV_TOG:
		case UND_TOG:
		case BOLD_TOG:
		case ALL_OFF:
			display_nonshift();
			rev_tog = und_tog = bld_tog = all_off = 0;
			switch (*str++)
			{
			case REV_TOG:
				rev_tog = 1 - rev_tog;
				break;
			case UND_TOG:
				und_tog = 1 - und_tog;
				break;
			case BOLD_TOG:
				bld_tog = 1 - bld_tog;
				break;
			case ALL_OFF:
				all_off = 1;
				und_tog = rev_tog = bld_tog = 0;
				break;
			}
			if (all_off)
			{
				if (!underline)
				{
					term_underline_off();
					underline = 1;
				}
				display_highlight(OFF);
				display_bold(OFF);
				display_colours(
				    fgcolour = fgcolour_user,
				    bgcolour = bgcolour_user,
				    OFF, OFF, OFF);
				high = 0;
				bold = 0;
			}
			if (und_tog && get_int_var(UNDERLINE_VIDEO_VAR))
			{
				/*
				 * Fix up after termcap may have turned
				 * everything off.
				 */
				if (bold)
					display_bold(ON);
				if (high)
					display_highlight(ON);

				if ((underline = 1 - underline) != 0)
					term_underline_off();
				else
					term_underline_on();
			}
			if (rev_tog)
			{
				/*
				 * Fix up after termcap may have turned
				 * everything off.
				 */
				if (!underline)
					term_underline_on();
				if (bold)
					display_bold(ON);

				high = display_highlight(TOGGLE);
				high = 1 - high;
			}
			if (bld_tog)
			{
				/*
				 * Fix up after termcap may have turned
				 * everything off.
				 */
				if (!underline)
					term_underline_on();
				if (high)
					display_highlight(ON);

				bold = display_bold(TOGGLE);
				bold = 1 - bold;
			}
			break;
		case COLOUR_TAG:
			display_nonshift();
			while (*str == COLOUR_TAG)
			{
				/* parse all consequent colour settings */
				int fg, bg;

				fg = *++str;
				if (!fg--)
					break;
				
				bg = *++str;
				if (!bg--)
					break;
				
				if (fg < 16)
					fgcolour = fg;
				if (bg < 16)
					bgcolour = bg;
				
				++str;
			}
			display_colours(fgcolour, bgcolour, bold, !underline, high);
			break;
		case FULL_OFF:
			++str;
			display_nonshift();
			if (!underline)
			{
				term_underline_off();
				underline = 1;
			}
			display_highlight(OFF);
			display_bold(OFF);
			display_colours(
			    fgcolour = fgcolour_user,
			    bgcolour = bgcolour_user,
			    OFF, OFF, OFF);
			high = 0;
			bold = 0;
			break;
		case '\007':
			/* After we display everything, we beep the terminal */
			++dobeep;
			++str;
			break;
		default:
			{
				unsigned n = calc_unival_length(str);
				/* n should never be 0 (that would mean a broken
				 * character), but here we just ensure we
				 * don't get an infinite loop.
				 */
				if (n == 0)
					n = 1;

				/* Input is supposedly an UTF-8 character */
				if (written < get_co())
				{
					unsigned unival = calc_unival(str);
					written += calc_unival_width(unival);

					display_text(str, n);
				}
				else
				{
					if (first)
					{
						first = 0;
						Debug(DB_WINDOW,
						    "skipping extra data");
					}
				}
				str += n;
				break;
			}
		}
	}
	display_nonshift();
	if (dobeep)
		term_beep();
	return written;
}

/*
 * rite: this routine displays a line to the screen adding bold facing when
 * specified by ^Bs, etc.  It also does handles scrolling and paging, if
 * SCROLL is on, and HOLD_MODE is on, etc.  This routine assumes that str
 * already fits on one screen line.  If show is true, str is displayed
 * regardless of the hold mode state.  If redraw is true, it is assumed we a
 * redrawing the screen from the display_ip list, and we should not add what
 * we are displaying back to the display_ip list again. 
 *
 * Note that rite sets display_highlight() to what it was at then end of the
 * last rite().  Also, before returning, it sets display_highlight() to OFF.
 * This way, between susequent rites(), you can be assured that the state of
 * bold face will remain the same and the it won't interfere with anything
 * else (i.e. status line, input line). 
 */
int
rite(Window *window, u_char *str, int show, int redraw, int backscroll, int logged)
{
	Screen	*old_current_screen = NULL;

	if (!redraw && !backscroll && window_get_scrolled_lines(window))
		window_add_new_scrolled_line(window);

	if (window_get_hold_mode(window) &&
	    window_get_hold_on_next_rite(window) && !redraw && !backscroll)
	{
		/* stops output from going to the window */
		window_set_hold_on_next_rite(window, 0);
		window_hold_mode(window, ON, 1);
		if (show)
			return (1);
	}
	/*
	 * Don't need to worry about the get_current_screen() if the window isn't
	 * visible, as hidden windows aren't attached to a screen anyway
	 */
	if (window_get_visible(window))
	{
		old_current_screen = get_current_screen();
		set_current_screen(window_get_screen(window));
	}
	if (!show &&
	    (window_hold_output(window) ||
	     hold_queue(window_get_hold_info(window))) &&
	    !in_help && !redraw && !backscroll)
		/* sends window output to the hold list for that window */
		add_to_hold_list(window, window_get_hold_info(window), str, logged);
	else
	{
		if (!redraw && !backscroll)
		{
			/*
			 * This isn't a screen refresh, so add the line to
			 * the display list for the window 
			 */
			window_add_display_line(window, str, logged);
		}
		if (window_get_visible(window))
		{
			int written;
			
			/* make sure the cursor is in the appropriate window */
			if (get_cursor_window() != window &&
			    !redraw && !backscroll)
			{
				set_cursor_window(window);
				Debug(DB_SCROLL, "screen %d cursor_window set "
						 "to window %d (%d)",
				    screen_get_screennum(get_current_screen()),
				    window_get_refnum(window),
				    screen_get_screennum(window_get_screen(window)));
				term_move_cursor(0,
				    window_get_cursor(window) +
				    window_get_top(window) +
				    window_menu_lines(window));
			}

			written = output_line(str, 0);
#ifdef TERM_USE_LAST_COLUMN
			/*
			 * If ignoring the last column, always erase it, but
			 * if it is in use, only do so if we haven't writtten
			 * there.
			 */
			if (written < get_co() && term_clear_to_eol())
#else
			if (term_clear_to_eol() && written < get_co())
#endif
			{
				/* EOL wasn't implemented, so do it with spaces */
				term_space_erase(get_co() - written);
			}
		}
		else if (!(window_get_miscflags(window) & WINDOW_NOTIFIED))
		{
			if ((current_who_level() & window_get_notify_level(window))
			    || ((window_get_notify_level(window) & LOG_BEEP)
				&& my_index(str, '\007')))
			{
				window_set_miscflags(window, WINDOW_NOTIFIED, 0);
				if (window_get_miscflags(window) & WINDOW_NOTIFY)
				{
					Window	*old_to_window;
					int	lastlog_level;

					lastlog_level =
						set_lastlog_msg_level(LOG_CRAP);
					old_to_window = get_to_window();
					set_to_window(curr_scr_win);
					say("Activity in window %d",
						window_get_refnum(window));
					set_to_window(old_to_window);
					set_lastlog_msg_level(lastlog_level);
				}
				update_all_status();
			}
		}
		if (!redraw && !backscroll)
		{
			int	line_cnt = window_get_line_cnt(window) + 1;
			int	cursor = window_get_cursor(window) + 1;

			window_set_cursor(window, cursor);
			window_set_line_cnt(window, line_cnt);
			if (window_get_scroll(window))
			{
				if (line_cnt >= window_get_display_size(window))
				{
					window_set_hold_on_next_rite(window, 1);
					window_set_line_cnt(window, 0);
				}
			}
			else
			{
				scroll_window(window);
				if (cursor ==
				    (window_get_display_size(window) - 1))
					window_set_hold_on_next_rite(window, 1);
			}
		}
		else if (window_get_visible(window))
		{
			term_cr();
			term_newline();
		}
		if (window_get_visible(window))
		{
			term_flush();
		}
	}
	if (old_current_screen)
		set_current_screen(old_current_screen);
	return (0);
}

int
get_underline(void)
{
	return underline;
}

void
set_underline(int val)
{
	underline = val;
}

unsigned
set_display(unsigned val)
{
	unsigned old = window_display;

	window_display = val;
	return old;
}

unsigned
get_display(void)
{
	return window_display;
}
