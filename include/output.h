/*
 * output.h: header for output.c 
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
 * @(#)$eterna: output.h,v 1.34 2015/02/22 12:43:18 mrg Exp $
 */

#ifndef irc__output_h_
#define irc__output_h_

	void	put_it(const char *, ...)
			__attribute__((__format__ (__printf__, 1, 2)));
	void	say(const char *, ...)
			__attribute__((__format__ (__printf__, 1, 2)));
	void	yell(const char *, ...)
			__attribute__((__format__ (__printf__, 1, 2)));
	void	help_put_it(const u_char *, char *, ...)
			__attribute__((__format__ (__printf__, 2, 3)));
	void	refresh_screen(u_int, u_char *);
	void	init_screen(void);
	void	put_file(u_char *);
	u_char	display_highlight(int);
	u_char	display_bold(int);
	void	display_colours(int, int);
	int	rite(Window *, u_char *, int, int, int, int);
	int	get_underline(void);
	void	set_underline(int);
	unsigned get_display(void);
	unsigned set_display(unsigned);
#define	set_display_on()	set_display(1)
#define	set_display_off()	set_display(0)

#endif /* irc__output_h_ */
