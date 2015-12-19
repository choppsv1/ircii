/*
 * lastlog.h: header for lastlog.c 
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
 * @(#)$eterna: lastlog.h,v 1.25 2014/02/20 09:29:13 mrg Exp $
 */

#ifndef irc__lastlog_h_
#define irc__lastlog_h_

/*
 * this list must agree with the list in lastlog.c
 */
#define LOG_NONE	0x000000
#define LOG_CURRENT	0x000000
#define LOG_CRAP	0x000001
#define LOG_PUBLIC	0x000002
#define LOG_MSG		0x000004
#define LOG_NOTICE	0x000008
#define LOG_WALL	0x000010
#define LOG_WALLOP	0x000020
#define LOG_NOTES	0x000040
#define LOG_OPNOTE	0x000080
#define	LOG_SNOTE	0x000100
#define	LOG_ACTION	0x000200
#define	LOG_DCC		0x000400
#define LOG_CTCP	0x000800
#define	LOG_USER1	0x001000
#define LOG_USER2	0x002000
#define LOG_USER3	0x004000
#define LOG_USER4	0x008000
#define LOG_BEEP	0x010000
#define LOG_HELP	0x020000

#define LOG_ALL (LOG_CRAP | LOG_PUBLIC | LOG_MSG | LOG_NOTICE | LOG_WALL | \
		LOG_WALLOP | LOG_NOTES | LOG_OPNOTE | LOG_SNOTE | LOG_ACTION | \
		LOG_CTCP | LOG_DCC | LOG_BEEP)

#define LOG_DEFAULT	LOG_NONE

typedef struct lastlog_stru Lastlog;
typedef struct lastlog_info_stru LastlogInfo;

	void	set_lastlog_level(u_char *);
	int	set_lastlog_msg_level(int);
	void	set_lastlog_size(int);
	void	set_notify_level(u_char *);
	void	lastlog(u_char *, u_char *, u_char *);
	u_char	**add_to_lastlog(Window *, u_char *);
	u_char	*bits_to_lastlog_level(int);
	int	real_lastlog_level(void);
	int	real_notify_level(void);
	int	parse_lastlog_level(u_char *);
	int	islogged(Window *);
	void	free_lastlog(Window *);
	u_char	*lastlog_line_back(Window *);
	LastlogInfo *lastlog_new_window(void);
	int	lastlog_get_size(LastlogInfo *);
	int	lastlog_get_level(LastlogInfo *);
	void	lastlog_set_level(LastlogInfo *, int);

#endif /* irc__lastlog_h_ */
