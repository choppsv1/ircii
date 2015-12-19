/*
 * ctcp.h: header file for ctcp.c
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
 * @(#)$eterna: ctcp.h,v 1.34 2015/02/22 12:43:18 mrg Exp $
 */

#ifndef irc__ctcp_h_
#define irc__ctcp_h_

#define CTCP_DELIM_CHAR '\001'
#define CTCP_DELIM_STR "\001"
#define CTCP_QUOTE_CHAR '\\'
#define CTCP_QUOTE_STR "\\"

#define CTCP_QUOTE_EM "\n\r\001\\"

#define CTCP_PRIVMSG 0
#define CTCP_NOTICE 1

typedef struct ctcp_flood_stru CtcpFlood;

	u_char	*do_ctcp(u_char *, u_char *, u_char *);
	u_char	*ctcp_quote_it(u_char *, size_t);
	u_char	*ctcp_unquote_it(u_char *, size_t *);
	u_char	*do_notice_ctcp(u_char *, u_char *, u_char *);
	int	in_ctcp(void);
	void	send_ctcp_reply(u_char *, u_char *, char *, ...)
			__attribute__((__format__ (__printf__, 3, 4)));
	void	send_ctcp(const u_char *, u_char *, u_char *, char *, ...)
			__attribute__((__format__ (__printf__, 4, 5)));
	CtcpFlood *ctcp_new_flood(void);
	void	ctcp_refresh_flood(CtcpFlood *, int);
	void	ctcp_clear_flood(CtcpFlood **);
	const u_char *ctcp_type(int);
	int	ctcp_was_crypted(void);
	void	set_ctcp_was_crypted(int);

#endif /* irc__ctcp_h_ */
