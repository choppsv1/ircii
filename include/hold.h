/*
 * hold.h: header for hold.c
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
 * @(#)$eterna: hold.h,v 1.22 2014/02/20 23:43:25 mrg Exp $
 */

#ifndef irc__hold_h_
#define irc__hold_h_

typedef	struct hold_stru Hold;
typedef struct hold_info_stru HoldInfo;

	void	remove_from_hold_list(HoldInfo *);
	void	add_to_hold_list(Window *, HoldInfo *, u_char *, int);
	void	hold_mode(Window *, HoldInfo *, int, int);
	int	hold_output(Window *, HoldInfo *);
	u_char	*hold_queue(HoldInfo *);
	void	reset_hold(Window *);
	int	hold_queue_logged(HoldInfo *);
	void	toggle_stop_screen(u_int, u_char *);
	HoldInfo *alloc_hold_info(void);
	void	free_hold_info(HoldInfo **);
	int	held_lines(HoldInfo *);
	int	hold_is_held(HoldInfo *);

#endif /* irc__hold_h_ */
