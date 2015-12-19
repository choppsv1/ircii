/*
 * whois.h: header for whois.c 
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
 * @(#)$eterna: whois.h,v 1.27 2014/03/01 02:51:44 mrg Exp $
 */

#ifndef irc__whois_h_
# define irc__whois_h_

typedef	struct whois_queue_stru	WhoisQueue;
typedef	struct whois_stuff_stru WhoisStuff;

/* Move into whois.c directly */
struct whois_stuff_stru
{
	u_char	*nick;
	u_char	*user;
	u_char	*host;
	u_char	*channel;
	u_char	*channels;
	u_char	*name;
	u_char	*server;
	u_char	*server_stuff;
	u_char	*away;
	int	oper;
	int	chop;
	int	not_on;
};

	void	add_to_whois_queue(u_char *, void (*) (WhoisStuff *, u_char *, u_char *), char *, ...);
	void	add_ison_to_whois(u_char *, void (*) (WhoisStuff *, u_char *, u_char *));
	void	whois_name(u_char *, u_char **);
	void	whowas_name(u_char *, u_char **);
	void	whois_channels(u_char *, u_char **);
	void	whois_server(u_char *, u_char **);
	void	whois_oper(u_char *, u_char **);
	void	whois_lastcom(u_char *, u_char **);
	void	whois_nickname(WhoisStuff *, u_char *, u_char *);
	void	whois_ignore_msgs(WhoisStuff *, u_char *, u_char *);
	void	whois_ignore_notices(WhoisStuff *, u_char *, u_char *);
	void	whois_ignore_walls(WhoisStuff *, u_char *, u_char *);
	void	whois_ignore_invites(WhoisStuff *, u_char *, u_char *);
	void	whois_notify(WhoisStuff *, u_char *, u_char *);
	void	whois_new_wallops(WhoisStuff *, u_char *, u_char *);
	void	clean_whois_queue(void);
	void	set_beep_on_msg(u_char *);
	void    userhost_cmd_returned(WhoisStuff *, u_char *, u_char *);
	void	user_is_away(u_char *, u_char **);
	void	userhost_returned(u_char *, u_char **);
	void	ison_returned(u_char *, u_char **);
	void	whois_chop(u_char *, u_char **);
	void	end_of_whois(u_char *, u_char **);
	void	whoreply(u_char *, u_char **);
	void	convert_to_whois(void);
	void	ison_notify(WhoisStuff *, u_char *, u_char *);
	void	no_such_nickname(u_char *, u_char **);
	int	do_beep_on_level(int);

#define	WHOIS_WHOIS	0x01
#define	WHOIS_USERHOST	0x02
#define	WHOIS_ISON	0x04
#define WHOIS_ISON2	0x08

#define	USERHOST_USERHOST ((void (*)(WhoisStuff *, u_char *, u_char *)) 1)

#endif /* irc__whois_h_ */
