/*
 * server.h: header for server.c 
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
 * @(#)$eterna: server.h,v 1.95 2015/05/14 01:47:38 mrg Exp $
 */

#ifndef irc__server_h_
#define irc__server_h_
  
#include "names.h"	/* for ChannelList */
#include "ctcp.h"	/* for CtcpFlood */
#include "whois.h"	/* for WhoisQueue */
#include "edit.h"	/* for WhoInfo */

/*
 * type definition to distinguish different
 * server versions
 */
#define ServerICB	-5
#define Server2_6	1
#define Server2_7	2
#define Server2_8	3
#define Server2_9	4
#define Server2_10	5
#define Server2_11	6

typedef	unsigned	short	ServerType;

typedef enum server_ssl_level {
	SSL_UNKNOWN,		/* use the default */
	SSL_OFF,		/* don't do ssl */
	SSL_ON,			/* do ssl, but don't worry about
				   veryfing if certificates are
				   valid */
	SSL_VERIFY,		/* do ssl, and check certificates are
				   valid */
} server_ssl_level;

typedef void (*server_private_cb_type)(void **);

	int	find_server_group(u_char *, int);
	u_char	*find_server_group_name(int);
	void	add_to_server_list(u_char *, int, u_char *, int,
				   u_char *, u_char *, int, int, int);
/* flags for add_to_server_list() */
#define SL_ADD_OVERWRITE	0x1
#define SL_ADD_DO_SSL		0x2
#define SL_ADD_DO_SSL_VERIFY	0x4
	void	build_server_list(u_char *);
	int	connect_to_server(u_char *, int, u_char *, int);
	void	get_connected(int);
	int	read_server_file(void);
	void	display_server_list(void);
	void	do_server(fd_set *, fd_set *);
	void	send_to_server(const char *, ...) 
			__attribute__((__format__ (__printf__, 1, 2)));
	int	server_get_whois(int);

	WhoisStuff	*server_get_whois_stuff(int);
	WhoisQueue	*server_get_qhead(int);
	WhoisQueue	*server_get_qtail(int);

	void	add_server_to_server_group(int, u_char *);
	void	servercmd(u_char *, u_char *, u_char *);
	u_char	*server_get_nickname(int);
	u_char	*server_get_name(int);
	u_char	*server_get_itsname(int);
	void	server_set_flag(int, int, int);
	int	find_in_server_list(u_char *, int, u_char *);
	u_char	*create_server_list(void);
	void	remove_from_server_list(int);
	void	server_set_motd(int, int);
	int	server_get_motd(int);
	int	server_get_operator(int);
	int	server_get_2_6_2(int);
	int	server_get_version(int);
	u_char	*server_get_password(int);
	u_char	*server_get_icbgroup(int);
	u_char	*server_get_icbmode(int);
	void	close_server(int, u_char *);
	void	MarkAllAway(u_char *, u_char *);
	int	is_server_connected(int);
	void	flush_server(void);
	int	server_get_flag(int, int);
	void	server_set_operator(int, int);
	void	server_is_connected(int, int);
	int	parse_server_index(u_char *);
	void	parse_server_info(u_char **, u_char **, u_char **,
				  u_char **, u_char **, u_char **,
				  int *, server_ssl_level *,
				  u_char **, int *);
	void	server_set_bits(fd_set *, fd_set *);
	void	server_set_itsname(int, u_char *);
	void	server_set_version(int, int);
	int	is_server_open(int);
	int	server_get_port(int);
	u_char	*server_set_password(int, u_char *);
	void	server_set_nickname(int, u_char *);
	void	server_set_2_6_2(int, int);
	void	server_set_qhead(int, WhoisQueue *);
	void	server_set_qtail(int, WhoisQueue *);
	void	server_set_whois(int, int);
	void	server_set_icbgroup(int, u_char *);
	void	server_set_icbmode(int, u_char *);
	int	server_get_server_group(int);
	void	server_set_server_group(int, int);
	void	close_all_server(void);
	void	disconnectcmd(u_char *, u_char *, u_char *);
	void	ctcp_reply_backlog_change(int);
	int	active_server_group(int sgroup);
	void	server_set_version_string(int, u_char *);
	u_char	*server_get_version_string(int);
	void	server_set_away(int, u_char *);
	u_char	*server_get_away(int);
	void	server_get_local_ip_info(int, SOCKADDR_STORAGE **, socklen_t *);
	void	server_set_chan_list(int, ChannelList *);
	ChannelList *server_get_chan_list(int);
	void	server_set_attempting_to_connect(int, int);
	int	server_get_attempting_to_connect(int);
	void	server_set_sent(int, int);
	int	server_get_sent(int);
	CtcpFlood *server_get_ctcp_flood(int);
	int	number_of_servers(void);
	void	unset_never_connected(void);
	int	never_connected(void);
	void	set_connected_to_server(int);
	int	connected_to_server(void);
	WhoInfo *server_get_who_info(void);
	int	server_get_oper_command(void);
	void	server_set_oper_command(int);
	int	parsing_server(void);
	int	get_primary_server(void);
	void	set_primary_server(int);
	int	get_from_server(void);
	int	set_from_server(int);
	server_ssl_level server_do_ssl(int);
	void	server_default_encryption(u_char *, u_char *);
	void	*server_get_server_private(int);
	void	server_set_server_private(int, void *, server_private_cb_type);
	int	server_get_proxy_port(int, int);
	u_char	*server_get_proxy_name(int, int);
	void	server_set_default_proxy(u_char *);
	int	ssl_level_to_sa_flags(server_ssl_level level);

#define	USER_MODE_I	0x0001
#define	USER_MODE_W	0x0002
#define	USER_MODE_S	0x0004 /* obsolete */
#define	USER_MODE_R	0x0008
#define	USER_MODE_A	0x0010 /* away status, not really used */
#define	USER_MODE_Z	0x0020
#define	CONNECTED	0x0040
#define	SSL_DONE	0x0080 /* SSL is initialised */
#define	SERVER_2_6_2	0x0100
#define CLOSE_PENDING	0x0200	/* set for servers who are being switched
				away from, but have not yet connected. */
#define LOGGED_IN	0x0400
#define	CLEAR_PENDING	0x0800	/* set for servers whose channels are to be
				removed when a connect has been established. */
#define	SERVER_FAKE	0x1000	/* server is fake entry; not connected to yet.
				will be GCed after connect_to_server_*(). */
#define	PROXY_CONNECT	0x2000	/* have sent CONNECT to proxy. */
#define	PROXY_REPLY	0x4000	/* got "200" from proxy. */
#define	PROXY_DONE	0x8000	/* got blank line from proxy - done. */

/* pick the default port if none is given. */
#define	CHOOSE_PORT(type) \
	(((type) == ServerICB || ((type) == -1 && client_default_is_icb())) \
		? icb_port() : irc_port())

#endif /* irc__server_h_ */
