/*
 * dcc.h: Things dealing client to client connections. 
 *
 * Written By Troy Rollo <troy@plod.cbme.unsw.oz.au> 
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
 * @(#)$eterna: dcc.h,v 1.34 2014/03/09 08:35:46 mrg Exp $
 */

/*
 * this file must be included after irc.h as i needed <sys/types.h>
 * <netinet/in.h> and <apra/inet.h> and, possibly, <sys/select.h>
 */

#ifndef irc__dcc_h_
#define irc__dcc_h_

/*
 * this list needs to be kept in sync with the dcc_types[] array
 * in dcc.c
 */
#define DCC_CHAT	((unsigned) 0x0001)
#define DCC_FILEOFFER	((unsigned) 0x0002)
#define DCC_FILEREAD	((unsigned) 0x0003)
#define DCC_RAW_LISTEN	((unsigned) 0x0004)
#define DCC_RAW		((unsigned) 0x0005)
#define DCC_TYPES	((unsigned) 0x000f)

#define DCC_WAIT	((unsigned) 0x0010)
#define DCC_ACTIVE	((unsigned) 0x0020)
#define DCC_OFFER	((unsigned) 0x0040)
#define DCC_DELETE	((unsigned) 0x0080)
#define DCC_TWOCLIENTS	((unsigned) 0x0100)
#ifdef NON_BLOCKING_CONNECTS
#define DCC_CNCT_PEND	((unsigned) 0x0200)
#endif /* NON_BLOCKING_CONNECTS */
#define DCC_STATES	((unsigned) 0xfff0)

	void	register_dcc_offer(u_char *, u_char *, u_char *, u_char *, u_char *, u_char *);
	void	process_dcc(u_char *);
	u_char	*dcc_raw_connect(u_char *, u_int);
	u_char	*dcc_raw_listen(u_int);
	void	dcc_list(u_char *);
	void	dcc_chat_transmit(u_char *, u_char *);
	void	dcc_message_transmit(u_char *, u_char *, int, int);
	void	close_all_dcc(void);
	void	set_dcc_bits(fd_set *, fd_set *);
	void	dcc_check(fd_set *, fd_set *);
	u_char	*dcc_list_func(u_char *);
	u_char	*dcc_chatpeers_func(void);
	void	set_dcchost(u_char *);
	u_char	*get_dcchost(void);

#endif /* irc__dcc_h_ */
