/*
 * icb.h - header for icb.c
 *
 * written by matthew green
 *
 * Copyright (c) 1995-2014 Matthew R. Green.
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
 * @(#)$eterna: icb.h,v 1.13 2014/03/12 08:04:41 mrg Exp $
 */

#ifndef irc__icb_h_
# define irc__icb_h_

/* external hooks for client code */
	void	icb_parse_server(u_char *);
	void	icb_login_to_server(int);
	void	icb_put_public(u_char *);
	void	icb_put_group(u_char *);
	void	icb_put_invite(u_char *);
	void	icb_put_msg2(u_char *, u_char *);
	void	icb_put_motd(u_char *);
	void	icb_put_nick(u_char *);
	void	icb_put_topic(u_char *);
	void	icb_put_version(u_char *);
	void	icb_put_who(u_char *);
	void	icb_put_funny_stuff(u_char *, u_char *, u_char *);
	void	icb_put_action(u_char *, u_char *);

/*
 * our hook for users to send stuff to the server.  this calls most of
 * the above functions, plus some more that aren't copies of IRC commands
 * and thus are "/icb command arg1 arg2" with some script front end.
 */
	void	icbcmd(u_char *, u_char *, u_char *);
	int	icb_port(void);
	void	set_icb_port(int);

/* icb field separator */
#define ICB_SEP '\01'

/*
 * each icb packet type:  
 * we only send one login packet at connect time.  we send pong
 * packets to repsonse to pings.  the only types that users use
 * are public messages and commands.  commands implement
 * *everything*.
 */
#define ICB_LOGIN	'a'	/* login packet */
#define ICB_PUBLIC	'b'	/* open msg to group */
#define ICB_PERSONAL	'c'	/* personal msg */
#define ICB_STATUS	'd'	/* status update message */
#define ICB_ERROR	'e'	/* error message */
#define ICB_IMPORTANT	'f'	/* special important announcement */
#define ICB_EXIT	'g'	/* tell other side to exit */
#define ICB_COMMAND	'h'	/* send a command from user */
#define ICB_CMDOUT	'i'	/* output from a command */
#define ICB_PROTO	'j'	/* protocol version information */
#define ICB_BEEP	'k'	/* beeps */
#define ICB_PING	'l'	/* ping packet */
#define ICB_PONG	'm'	/* return for ping packet */

/* fields needed for each type; perhaps put this in a table? */
#define	ICB_GET_LOGIN_MAXFIELD		1
#define	ICB_GET_PUBLIC_MAXFIELD		2
#define	ICB_GET_MSG_MAXFIELD		2
#define	ICB_GET_STATUS_MAXFIELD		2
#define	ICB_GET_IMPORTANT_MAXFIELD	2
#define	ICB_GET_CMDOUT_MAXFIELD		10
#define	ICB_GET_CMDOUT_MINFIELD		2
#define	ICB_GET_WHOOUT_MAXFIELD		8

#define	ICB_PUT_LOGIN_MAXFIELD		7
#define	ICB_PUT_PUBLIC_MAXFIELD		1

#endif /* irc__icb_h_ */
