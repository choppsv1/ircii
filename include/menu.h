/*
 * Here we define how our menus are held
 *
 * Copyright (c) 1992 Troy Rollo.
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
 * @(#)$eterna: menu.h,v 1.21 2014/02/05 07:20:11 mrg Exp $
 */

#ifndef irc__menu_h_
#define irc__menu_h_

#define	SMF_ERASE	0x0001
#define	SMF_NOCURSOR	0x0002
#define	SMF_CURSONLY	0x0004
#define	SMF_CALCONLY	0x0008

typedef	struct window_menu_stru	WindowMenu;

/* Below are our known menu functions */
	void	menu_previous(u_char *);	/* Go to previous menu */
	void	menu_submenu(u_char *);	/* Invoke a submenu */
	void	menu_exit(u_char *);		/* Exit the menu */
	void	menu_channels(u_char *);	/* List of channels menu */
	void	menu_command(u_char *);	/* Invoke an IRCII command */
	void	menu_key(u_int);
	void	load_menu(u_char *);
	int	ShowMenu(u_char *);
	int	ShowMenuByWindow(Window *, int);
	void	enter_menu(u_int, u_char *);
	void	set_menu(u_char *);
	WindowMenu	*menu_init(void);
	u_int	menu_lines(WindowMenu *);
	int	menu_active(WindowMenu *);

#endif /* irc__menu_h_ */
