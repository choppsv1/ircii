/*
 * dcc.c: Things dealing client to client connections. 
 *
 * Written By Troy Rollo <troy@cbme.unsw.oz.au> 
 *
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
IRCII_RCSID("@(#)$eterna: dcc.c,v 1.173 2014/08/14 01:51:25 mrg Exp $");

#include <sys/stat.h>

#ifdef HAVE_WRITEV
#include <sys/uio.h>
#endif

#include "server.h"
#include "ircaux.h"
#include "whois.h"
#include "lastlog.h"
#include "ctcp.h"
#include "dcc.h"
#include "hook.h"
#include "vars.h"
#include "window.h"
#include "output.h"
#include "newio.h"
#include "irccrypt.h"
#include "parse.h"

typedef	struct	DCC_struct
{
	unsigned	flags;
	int	read;
	int	write;
	int	file;
	off_t	filesize;
	u_char	*description;
	u_char	*user;
	u_char	*othername;
	u_char	*remname;
	u_short	remport;
	off_t	bytes_read;
	off_t	bytes_sent;
	time_t	lasttime;
	time_t	starttime;
	u_char	*buffer;
	struct DCC_struct	*next;
} DCC_list;

static	u_char	*dcc_source_host;		/* source host to use for DCC */

static	void	dcc_chat(u_char *);
static	void	dcc_chat_rename(u_char *);
static	void	dcc_filesend(u_char *);
static	void	dcc_getfile(u_char *);
static	void	dcc_close(u_char *);
static	void	dcc_rename(u_char *);
static	void	dcc_send_raw(u_char *);
static	void	process_incoming_chat(DCC_list *);
static	void	process_outgoing_file(DCC_list *);
static	void	process_incoming_file(DCC_list *);
static	void	process_incoming_raw(DCC_list *);
static	void	process_incoming_listen(DCC_list *);

static	DCC_list *dcc_searchlist(u_char *, u_char *, int, int, u_char *);
static	void	dcc_erase(DCC_list *);

#ifndef O_BINARY
#define O_BINARY 0
#endif /* O_BINARY */

static struct dcc_command_stru
{
	u_char	*name;	/* *MUST* be in ALL CAPITALS */
	int	uniq; /* minimum length to be a unique command */
	void	(*function)(u_char *);
} dcc_commands[] = {
	{ UP("CHAT"),	2, dcc_chat },
	{ UP("LIST"),	1, dcc_list },
	{ UP("SEND"),	2, dcc_filesend },
	{ UP("GET"),	1, dcc_getfile },
	{ UP("CLOSE"),	2, dcc_close },
	{ UP("RENAME"),	2, dcc_rename },
	{ UP("RAW"),	2, dcc_send_raw },
	{ NULL,		0, (void (*)(u_char *)) NULL }
};

/*
 * this list needs to be kept in sync with the DCC_TYPES defines
 * in dcc.h
 */
static	u_char	*dcc_types[] =
{
	UP("<null>"),
	UP("CHAT"),
	UP("SEND"),
	UP("GET"),
	UP("RAW_LISTEN"),
	UP("RAW"),
	NULL
};

static	struct	deadlist
{
	DCC_list *it;
	struct deadlist *next;
}	*deadlist = NULL;

static	off_t	filesize = 0;

static	DCC_list *ClientList = NULL;

static	void	add_to_dcc_buffer(DCC_list *, u_char *);
static	void	dcc_really_erase(void);
static	void	dcc_add_deadclient(DCC_list *);
static	int	dcc_open(DCC_list *);
static	u_char	*dcc_time(time_t);
static	u_char	*dcc_sockname(SOCKADDR_STORAGE *, int);

static	u_char *
dcc_sockname(SOCKADDR_STORAGE *ss, int salen)
{
	static u_char buf[NI_MAXHOST + NI_MAXSERV + 2];
	u_char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

	if (getnameinfo((struct sockaddr *)ss, salen,
	    CP(hbuf), sizeof hbuf, CP(sbuf), sizeof sbuf,
	    NI_NUMERICSERV | NI_NUMERICHOST))
	{
		my_strmcpy(hbuf, "[unknown]", sizeof(hbuf) - 1);
		my_strmcpy(sbuf, "[unknown]", sizeof(sbuf) - 1);
	}
	snprintf(CP(buf), sizeof buf, "%s:%s", hbuf, sbuf);
	return buf;
}

/*
 * dcc_searchlist searches through the dcc_list and finds the client
 * with the the flag described in type set.
 */
static DCC_list *
dcc_searchlist(u_char *name, u_char *user, int type, int flag, u_char *othername)
{
	DCC_list **Client, *NewClient;

	for (Client = (&ClientList); *Client ; Client = (&(**Client).next))
	{
		if ((((**Client).flags&DCC_TYPES) == type) &&
		    ((!name || (!my_stricmp(name, (**Client).description))) ||
		    (othername && (**Client).othername && (!my_stricmp(othername, (**Client).othername)))) &&
		    (my_stricmp(user, (**Client).user)==0))
			return *Client;
	}
	if (!flag)
		return NULL;
	*Client = NewClient = new_malloc(sizeof *NewClient);
	NewClient->flags = type;
	NewClient->read = NewClient->write = NewClient->file = -1;
	NewClient->filesize = filesize;
	NewClient->next = NULL;
	NewClient->user = NewClient->description = NewClient->othername = NULL;
	NewClient->bytes_read = NewClient->bytes_sent = 0L;
	NewClient->starttime = 0;
	NewClient->buffer = 0;
	NewClient->remname = 0;
	malloc_strcpy(&NewClient->description, name);
	malloc_strcpy(&NewClient->user, user);
	malloc_strcpy(&NewClient->othername, othername);
	time(&NewClient->lasttime);
	return NewClient;
}

static	void
dcc_add_deadclient(DCC_list *client)
{
	struct deadlist *new;

	new = new_malloc(sizeof *new);
	new->next = deadlist;
	new->it = client;
	deadlist = new;
}

/*
 * dcc_erase searches for the given entry in the dcc_list and
 * removes it
 */
static void
dcc_erase(DCC_list *Element)
{
	DCC_list	**Client;

	for (Client = &ClientList; *Client; Client = &(**Client).next)
		if (*Client == Element)
		{
			*Client = Element->next;

			if ((Element->flags & DCC_TYPES) != DCC_RAW_LISTEN)
				new_close(Element->write);
			new_close(Element->read);
			if (Element->file != -1)
				new_close(Element->file);
			new_free(&Element->description);
			new_free(&Element->user);
			new_free(&Element->othername);
			new_free(&Element->buffer);
			new_free(&Element->remname);
			new_free(&Element);
			return;
		}
}

static	void
dcc_really_erase(void)
{
	struct deadlist *dies;

	while ((dies = deadlist) != NULL)
	{
		deadlist = deadlist->next;
		dcc_erase(dies->it);
		new_free(&dies);
	}
}

/*
 * Set the descriptor set to show all fds in Client connections to
 * be checked for data.
 */
void
set_dcc_bits(fd_set *rd, fd_set *wd)
{
	DCC_list	*Client;

	for (Client = ClientList; Client != NULL; Client = Client->next)
	{
#ifdef DCC_CNCT_PEND
		if (Client->write != -1 && (Client->flags & DCC_CNCT_PEND))
			FD_SET(Client->write, wd);
#endif /* DCC_CNCT_PEND */
		if (Client->read != -1)
			FD_SET(Client->read, rd);
	}
}

/*
 * Check all DCCs for data, and if they have any, perform whatever
 * actions are required.
 */
void
dcc_check(fd_set *rd, fd_set *wd)
{
	DCC_list	**Client;
	struct	timeval	time_out;
	int	previous_server;
	int	lastlog_level;

	previous_server = set_from_server(-1);
	time_out.tv_sec = time_out.tv_usec = 0;
	lastlog_level = set_lastlog_msg_level(LOG_DCC);
	for (Client = (&ClientList); *Client != NULL; )
	{
#ifdef NON_BLOCKING_CONNECTS
		/*
		 * run all connect-pending sockets.. suggested by deraadt@theos.com
		 */
		if ((*Client)->flags & DCC_CNCT_PEND)
		{
			SOCKADDR_STORAGE remaddr;
			socklen_t	rl = sizeof(remaddr);

			if (getpeername((*Client)->read, (struct sockaddr *) &remaddr, &rl) != -1)
			{
				if ((*Client)->flags & DCC_OFFER)
				{
					(*Client)->flags &= ~DCC_OFFER;
					save_message_from();
					message_from((*Client)->user, LOG_DCC);
					if (((*Client)->flags & DCC_TYPES) != DCC_RAW)
						say("DCC %s connection with %s[%s] established",
							dcc_types[(*Client)->flags&DCC_TYPES],
							(*Client)->user,
							dcc_sockname(&remaddr, rl));
					restore_message_from();
				}
				(*Client)->starttime = time(NULL);
				(*Client)->flags &= ~DCC_CNCT_PEND;
				set_blocking((*Client)->read);
				if ((*Client)->read != (*Client)->write)
					set_blocking((*Client)->write);
			} /* else we're not connected yet */
		}
#endif /* NON_BLOCKING_CONNECTS */
		if ((*Client)->read != -1 && FD_ISSET((*Client)->read, rd))
		{
			switch((*Client)->flags & DCC_TYPES)
			{
			case DCC_CHAT:
				process_incoming_chat(*Client);
				break;
			case DCC_RAW_LISTEN:
				process_incoming_listen(*Client);
				break;
			case DCC_RAW:
				process_incoming_raw(*Client);
				break;
			case DCC_FILEOFFER:
				process_outgoing_file(*Client);
				break;
			case DCC_FILEREAD:
				process_incoming_file(*Client);
				break;
			}
		}
		if ((*Client)->flags & DCC_DELETE)
			dcc_add_deadclient(*Client);
		Client = (&(**Client).next);
	}
	(void) set_lastlog_msg_level(lastlog_level);
	dcc_really_erase();
	set_from_server(previous_server);
}

/*
 * Process a DCC command from the user.
 */
void
process_dcc(u_char *args)
{
	u_char	*command;
	int	i;
	size_t	len;

	if (!(command = next_arg(args, &args)))
		return;
	len = my_strlen(command);
	upper(command);
	for (i = 0; dcc_commands[i].name != NULL; i++)
	{
		if (!my_strncmp(dcc_commands[i].name, command, len))
		{
			if (len < dcc_commands[i].uniq)
			{
				say("DCC command not unique: %s", command );
				return;
			}
			save_message_from();
			message_from(NULL, LOG_DCC);
			dcc_commands[i].function(args);
			restore_message_from();
			return;
		}
	}
	say("Unknown DCC command: %s", command);
}

int listen_dcc(u_char *);

int
listen_dcc(u_char *src_host)
{
	SOCKADDR_STORAGE *ss;
	struct addrinfo hints, *res, *res0;
	int err, s;

	if (!get_int_var(BIND_LOCAL_DCCHOST_VAR))
		src_host = NULL;
	server_get_local_ip_info(get_from_server(), &ss, NULL);
	memset(&hints, 0, sizeof hints);
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = 0;
	hints.ai_addrlen = 0;
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_UNSPEC;
	errno = 0;
	err = getaddrinfo(CP(src_host), CP(zero()), &hints, &res0);
	if (err != 0)
	{
		errno = err;
		return -2;
	}
	for (res = res0; res; res = res->ai_next) {
		if (ss && SS_FAMILY(ss) != res->ai_family)
			continue;

		if ((s = socket(res->ai_family, res->ai_socktype,
		    res->ai_protocol)) < 0)
			continue;
		set_socket_options(s);
		if (res->ai_family == AF_INET)
			((struct sockaddr_in *)res->ai_addr)->sin_port = htons(get_int_var(DCCPORT_VAR));
#ifdef INET6
		else if (res->ai_family == AF_INET6)
			((struct sockaddr_in6 *)res->ai_addr)->sin6_port = htons(get_int_var(DCCPORT_VAR));
#endif
		if (bind(s, res->ai_addr, res->ai_addrlen) == 0 &&
		    listen(s, 1) == 0)
		{
			freeaddrinfo(res0);
			return s;
		}
		close(s);
	}

	freeaddrinfo(res0);
	return -1;
}

static	int
dcc_open(DCC_list *Client)
{
	u_char    *user, *Type;
	int	old_server, error;
	struct	addrinfo hints, *res0;
	int	ctcptype;
#ifndef NON_BLOCKING_CONNECTS
	SOCKADDR_STORAGE	remaddr;
	socklen_t	rl = sizeof(remaddr);
#endif /* NON_BLOCKING_CONNECTS */

	if ((ctcptype = in_ctcp()) == -1)
	{
		say("You may not use the CTCP command in an ON CTCP_REPLY!");
		return 0;
	}

	user = Client->user;
	old_server = get_from_server();
	if (old_server == -1)
		set_from_server(get_window_server(0));

	Type = dcc_types[Client->flags & DCC_TYPES];
	if (Client->flags & DCC_OFFER)
	{
#ifdef DCC_CNCT_PEND
		Client->flags |= DCC_CNCT_PEND;
#endif /* DCC_CNCT_PEND */
		if ((Client->write = connect_by_number(Client->remport,
		    Client->remname, 1, 0, 0)) < 0)
		{
			save_message_from();
			message_from(user, LOG_DCC);
			say("Unable to create connection: %s",
				errno ? strerror(errno) : "Unknown Host");
			restore_message_from();
			dcc_erase(Client);
			set_from_server(old_server);
			return 0;
		}
		Client->read = Client->write;
		Client->bytes_read = Client->bytes_sent = 0L;
		Client->flags |= DCC_ACTIVE;
#ifndef NON_BLOCKING_CONNECTS
		Client->flags &= ~DCC_OFFER;
		Client->starttime = time(NULL);
		if (getpeername(Client->read, (struct sockaddr *) &remaddr, &rl) == -1)
		{
			save_message_from();
			message_from(user, LOG_DCC);
			say("DCC error: getpeername failed: %s", strerror(errno));
			restore_message_from();
			dcc_erase(Client);
			set_from_server(old_server);
			return 0;
		}
		if ((Client->flags & DCC_TYPES) != DCC_RAW)
		{
			save_message_from();
			message_from(user, LOG_DCC);
			say("DCC %s connection with %s[%s] established",
				Type, user, dcc_sockname(&remaddr, rl));
			restore_message_from();
		}
#endif /* NON_BLOCKING_CONNECTS */
		set_from_server(old_server);
		return 1;
	}
	else
	{
#ifdef DCC_CNCT_PEND
		Client->flags |= DCC_WAIT|DCC_CNCT_PEND;
#else
		Client->flags |= DCC_WAIT;
#endif /* DCC_CNCT_PEND */
		if ((Client->read = listen_dcc(dcc_source_host)) < 0)
		{
			save_message_from();
			message_from(user, LOG_DCC);
			say("Unable to initialise connection: %s",
				Client->read ? gai_strerror(errno) : strerror(errno));
			restore_message_from();
			dcc_erase(Client);
			set_from_server(old_server);
			return 0;
		}
		if (Client->flags & DCC_TWOCLIENTS)
		{
			SOCKADDR_STORAGE	locaddr;
			SOCKADDR_STORAGE	*myip, me;
			u_char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
			u_char *printhbuf = 0;
			u_char	*nopath, *sh;
			socklen_t	sla;
			int	times = 0;

			if ((Client->flags & DCC_FILEOFFER) &&
			    (nopath = my_rindex(Client->description, '/')))
				nopath++;
			else
				nopath = Client->description;

			sla = sizeof locaddr;
			getsockname(Client->read,
			    (struct sockaddr *) &locaddr, &sla);

			if ((error = getnameinfo((struct sockaddr *)&locaddr,
			    sla, 0, 0, CP(sbuf), sizeof sbuf, NI_NUMERICSERV)))
			{
				save_message_from();
				message_from(user, LOG_DCC);
				say("Unable to get socket port address: %s",
					gai_strerror(error));
				restore_message_from();
				dcc_erase(Client);
				set_from_server(old_server);
				return 0;
			}

			myip = 0;
			sh = dcc_source_host;
			if (sh && *sh)
			{
do_it_again:
				memset(&hints, 0, sizeof hints);
				hints.ai_flags = 0;
				hints.ai_protocol = 0;
				hints.ai_addrlen = 0;
				hints.ai_canonname = NULL;
				hints.ai_addr = NULL;
				hints.ai_next = NULL;
				hints.ai_socktype = SOCK_STREAM;
				hints.ai_family = PF_UNSPEC;
				error = getaddrinfo(CP(sh), 0, &hints, &res0);
				if (error == 0)
				{
					memmove(&me, (char *) res0->ai_addr, sizeof me);
					myip = &me;
					sla = res0->ai_addrlen;
					freeaddrinfo(res0);
				}
				else
				{
					save_message_from();
					message_from(user, LOG_DCC);
					say("Unable to create address from %s: %s", sh,
						gai_strerror(error));
					restore_message_from();
					dcc_erase(Client);
					set_from_server(old_server);
					return 0;
				}
			}
			else
				server_get_local_ip_info(get_from_server(), &myip, &sla);

			if (!myip)
				myip = &locaddr;

			error = getnameinfo((struct sockaddr *) myip, sla,
				CP(hbuf), sizeof hbuf, 0, 0, NI_NUMERICHOST);
			if (error)
			{
				save_message_from();
				message_from(user, LOG_DCC);
				say("Unable to getnameinfo: %s", gai_strerror(error));
				restore_message_from();
				dcc_erase(Client);
				set_from_server(old_server);
				return 0;
			}

#ifdef INET6
			/* Make sure IPv4 goes as a single number */
			if (SS_FAMILY(myip) == PF_INET)
#endif
			{
				struct sockaddr_in *in = (struct sockaddr_in *) myip;
				uint32_t l;

				malloc_strcpy(&printhbuf, hbuf);
				memmove(&l, (char *) &in->sin_addr.s_addr, sizeof l);
				snprintf(CP(hbuf), sizeof hbuf, "%lu", (unsigned long)ntohl(l));
			}

			/* if the host is "0", force it to source_host or then hostname */
			if (my_strcmp(hbuf, zero()) == 0)
			{
				if (times == 0)
				{
					sh = get_irchost();
					if (!sh || !*sh)
						times++;
				}
				if (times == 1)
					sh = my_hostname();
				else
				{
					save_message_from();
					message_from(user, LOG_DCC);
					say("DCC: Unable to generate a source address");
					restore_message_from();
					dcc_erase(Client);
					set_from_server(old_server);
					if (printhbuf)
						new_free(&printhbuf);
					return 0;
				}
				times++;
				goto do_it_again;
			}


			if (Client->filesize)
				send_ctcp(ctcp_type(ctcptype), user, UP("DCC"),
					 "%s %s %s %s %llu", Type, nopath, hbuf, sbuf,
					 (unsigned long long)Client->filesize);
			else
				send_ctcp(ctcp_type(ctcptype), user, UP("DCC"),
					 "%s %s %s %s", Type, nopath, hbuf, sbuf);
			save_message_from();
			message_from(user, LOG_DCC);
			say("Sent DCC %s [%s:%s] request to %s",
			    Type, printhbuf ? printhbuf : hbuf, sbuf, user);
			restore_message_from();
			if (printhbuf)
				new_free(&printhbuf);
		}
		Client->starttime = 0;
		set_from_server(old_server);
		return 2;
	}
}

static void
dcc_chat(u_char *args)
{
	u_char	*user;
	DCC_list	*Client;

	if ((user = next_arg(args, &args)) == NULL)
	{
		say("You must supply a nickname for DCC CHAT");
		return;
	}
	Client = dcc_searchlist(UP("chat"), user, DCC_CHAT, 1, NULL);
	if ((Client->flags&DCC_ACTIVE) || (Client->flags&DCC_WAIT))
	{
		say("A previous DCC CHAT to %s exists", user);
		return;
	}
	Client->flags |= DCC_TWOCLIENTS;
	dcc_open(Client);
}

u_char	*
dcc_raw_listen(u_int iport)
{
	DCC_list	*Client;
	u_char	PortName[10];
	struct sockaddr_in locaddr;	/* XXX DCC IPv6: this one doesn't matter; for now only support DCC RAW for ipv4 */
	u_char	*RetName = NULL;
	socklen_t	size;
	int	lastlog_level;
	u_short	port = (u_short) iport;

	lastlog_level = set_lastlog_msg_level(LOG_DCC);
	if (port && port < 1025)
	{
		say("Cannot bind to a privileged port");
		(void) set_lastlog_msg_level(lastlog_level);
		return NULL;
	}
	snprintf(CP(PortName), sizeof PortName, "%d", port);
	Client = dcc_searchlist(UP("raw_listen"), PortName, DCC_RAW_LISTEN, 1, NULL);
	if (Client->flags & DCC_ACTIVE)
	{
		say("A previous DCC RAW_LISTEN on %s exists", PortName);
		(void) set_lastlog_msg_level(lastlog_level);
		return RetName;
	}
	if (0 > (Client->read = socket(AF_INET, SOCK_STREAM, 0)))
	{
		dcc_erase(Client);
		say("socket() failed: %s", strerror(errno));
		(void) set_lastlog_msg_level(lastlog_level);
		return RetName;
	}
	set_socket_options(Client->read);
	memset(&locaddr, 0, sizeof(locaddr));
	locaddr.sin_family = AF_INET;
	locaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	locaddr.sin_port = htons(port);
	if (bind(Client->read, (struct sockaddr *) &locaddr, sizeof(locaddr))
				== -1)
	{
		dcc_erase(Client);
		say("Could not bind port: %s", strerror(errno));
		(void) set_lastlog_msg_level(lastlog_level);
		return RetName;
	}
	listen(Client->read, 4);
	size = sizeof(locaddr);
	Client->starttime = time(NULL);
	getsockname(Client->read, (struct sockaddr *) &locaddr, &size);
	Client->write = ntohs(locaddr.sin_port);
	Client->flags |= DCC_ACTIVE;
	snprintf(CP(PortName), sizeof PortName, "%d", Client->write);
	malloc_strcpy(&Client->user, PortName);
	malloc_strcpy(&RetName, PortName);
	(void) set_lastlog_msg_level(lastlog_level);
	return RetName;
}

u_char	*
dcc_raw_connect(u_char *host, u_int iport)
{
	DCC_list	*Client;
	struct	addrinfo hints, *res = 0, *res0 = 0;
	struct	sockaddr_in address;
	u_char	addr[NI_MAXHOST];
	u_char	PortName[10], *RetName = NULL;
	u_short	port = (u_short)iport;
	int	lastlog_level, err;

	lastlog_level = set_lastlog_msg_level(LOG_DCC);

	memset(&hints, 0, sizeof hints);
	hints.ai_flags = 0;
	hints.ai_protocol = 0;
	hints.ai_addrlen = 0;
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_INET;
	err = getaddrinfo(CP(host), 0, &hints, &res0);
	if (err == 0)
	{
		for (res = res0; res; res = res->ai_next)
			if (res->ai_family == PF_INET)
			{
				memmove(&address, res->ai_addr, sizeof address);
				break;
			}
		freeaddrinfo(res0);
	}
	if (!res || err)
	{
		say("Unknown host: %s", host);
		(void) set_lastlog_msg_level(lastlog_level);
		goto out;
	}

	snprintf(CP(PortName), sizeof PortName, "%d", port);
	Client = dcc_searchlist(host, PortName, DCC_RAW, 1, NULL);
	if (Client->flags & DCC_ACTIVE)
	{
		say("A previous DCC RAW to %s on %s exists", host, PortName);
		(void) set_lastlog_msg_level(lastlog_level);
		return RetName;
	}
	Client->remport = port;
	err = getnameinfo((struct sockaddr *) &address, sizeof address, CP(addr), NI_MAXHOST, 0, 0, NI_NUMERICHOST);
	if (err != 0)
	{
		my_strncpy(addr, "[unknown]", sizeof(addr) - 1);
		yell("dcc_raw_connect: getnameinfo failed: %s", gai_strerror(err));
	}
	malloc_strcpy(&Client->remname, addr);
	Client->flags = DCC_OFFER | DCC_RAW;
	if (!dcc_open(Client))
		return RetName;
	snprintf(CP(PortName), sizeof PortName, "%d", Client->read);
	malloc_strcpy(&Client->user, PortName);
	if (do_hook(DCC_RAW_LIST, "%s %s E %d", PortName, host, port))
		put_it("DCC RAW connection to %s on %s via %d established",
				host, PortName, port);
	malloc_strcpy(&RetName, PortName);
	(void) set_lastlog_msg_level(lastlog_level);
out:
	return RetName;
}

static	void
dcc_filesend(u_char *args)
{
	u_char	*user;
	u_char	*filename,
		*fullname;
	DCC_list *Client;
	u_char	FileBuf[BIG_BUFFER_SIZE];
	struct	stat	stat_buf;

#ifdef  DAEMON_UID
	if (DAEMON_UID == getuid())
	{
		say("You are not permitted to use DCC to exchange files");
		return;
	}
#endif /* DAEMON_UID */
	if (0 == (user = next_arg(args, &args)) ||
	    0 == (filename = next_arg(args, &args)))
	{
		say("You must supply a nickname and filename for DCC SEND");
		return;
	}
	if (IS_ABSOLUTE_PATH(filename))
	{
		my_strmcpy(FileBuf, filename, sizeof FileBuf);
	}
	else if (*filename == '~')
	{
		if (0 == (fullname = expand_twiddle(filename)))
		{
			yell("Unable to expand %s", filename);
			return;
		}
		my_strmcpy(FileBuf, fullname, sizeof FileBuf);
		new_free(&fullname);
	}
	else
	{
		if (getcwd(CP(FileBuf), sizeof(FileBuf))) {
			FileBuf[0] = '.';
			FileBuf[1] = '\0';
		}
		my_strmcat(FileBuf, "/", sizeof FileBuf);
		my_strmcat(FileBuf, filename, sizeof FileBuf);
	}
	if (0 != access(CP(FileBuf), R_OK))
	{
		yell("Cannot access %s", FileBuf);
		return;
	}
	stat(CP(FileBuf), &stat_buf);
/* some unix didn't have this ???? */
#ifdef S_IFDIR
	if (stat_buf.st_mode & S_IFDIR)
	{
		yell("Cannot send a directory");
		return;
	}
#endif /* S_IFDER */
	if (scanstr(FileBuf, UP("/etc/")))
	{
		yell("Send request rejected");
		return;
	}
	if ((int) my_strlen(FileBuf) >= 7 && 0 == my_strcmp(FileBuf + my_strlen(FileBuf) - 7, "/passwd"))
	{
		yell("Send request rejected");
		return;
	}
	filesize = stat_buf.st_size;
	Client = dcc_searchlist(FileBuf, user, DCC_FILEOFFER, 1, filename);
	if ((Client->file = open(CP(Client->description), O_RDONLY | O_BINARY)) == -1)
	{
		say("Unable to open %s: %s\n", Client->description,
			errno ? strerror(errno) : "Unknown Host");
		new_close(Client->read);
		Client->read = Client->write = (-1);
		Client->flags |= DCC_DELETE;
		return;
	}
	filesize = 0;
	if ((Client->flags & DCC_ACTIVE) || (Client->flags & DCC_WAIT))
	{
		say("A previous DCC SEND:%s to %s exists", FileBuf, user);
		return;
	}
	Client->flags |= DCC_TWOCLIENTS;
	dcc_open(Client);
}


static	void
dcc_getfile(u_char *args)
{
	u_char	*user;
	u_char	*filename;
	DCC_list	*Client;
	u_char	*fullname = NULL;

#ifdef  DAEMON_UID
	if (DAEMON_UID == getuid())
	{
		say("You are not permitted to use DCC to exchange files");
		return;
	}
#endif /* DAEMON_UID */
	if (0 == (user = next_arg(args, &args)))
	{
		say("You must supply a nickname for DCC GET");
		return;
	}
	filename = next_arg(args, &args);
	if (0 == (Client = dcc_searchlist(filename, user, DCC_FILEREAD, 0, NULL)))
	{
		if (filename)
			say("No file (%s) offered in SEND mode by %s",
					filename, user);
		else
			say("No file offered in SEND mode by %s", user);
		return;
	}
	if ((Client->flags & DCC_ACTIVE) || (Client->flags & DCC_WAIT))
	{
		if (filename)
			say("A previous DCC GET:%s to %s exists", filename, user);
		else
			say("A previous DCC GET to %s exists", user);
		return;
	}
	if (0 == (Client->flags & DCC_OFFER))
	{
		say("I'm a teapot!");
		dcc_erase(Client);
		return;
	}

	if (0 == (fullname = expand_twiddle(Client->description)))
		malloc_strcpy(&fullname, Client->description);
	Client->file = open(CP(fullname), O_BINARY | O_WRONLY | O_TRUNC | O_CREAT, 0644);
	new_free(&fullname);
	if (-1 == Client->file)
	{
		say("Unable to open %s: %s", Client->description,
				errno ? strerror(errno) : "<No Error>");
		return;
	}
	Client->flags |= DCC_TWOCLIENTS;
	Client->bytes_sent = Client->bytes_read = 0L;
	if (!dcc_open(Client))
		close(Client->file);
}

void
register_dcc_offer(u_char *user, u_char *type, u_char *description, u_char *address, u_char *port, u_char *size)
{
	DCC_list	*Client;
	u_char	*c, *s, *cmd = NULL;
	unsigned	TempInt;
	int	CType;
	int	do_auto = 0;	/* used in dcc chat collisions */
	int	lastlog_level;

	lastlog_level = set_lastlog_msg_level(LOG_DCC);
	if (0 != (c = my_rindex((description), '/')))
		description = c + 1;
	if ('.' == *description)
		*description = '_';
	if (size && *size)
		filesize = my_atoi(size);
	else
		filesize = 0;
	malloc_strcpy(&cmd, type);
	upper(cmd);
	if (!my_strcmp(cmd, "CHAT"))
		CType = DCC_CHAT;
#ifndef  DAEMON_UID
	else if (!my_strcmp(cmd, "SEND"))
#else
	else if (!my_strcmp(cmd, "SEND") && DAEMON_UID != getuid())
#endif /* DAEMON_UID */
		CType = DCC_FILEREAD;
	else
	{
		say("Unknown DCC %s (%s) received from %s", type, description, user);
		goto out;
	}
	Client = dcc_searchlist(description, user, CType, 1, NULL);
	filesize = 0;
	if (Client->flags & DCC_WAIT)
	{
		new_close(Client->read);
		dcc_erase(Client);
		if (DCC_CHAT == CType)
		{
			Client = dcc_searchlist(description, user, CType, 1, NULL);
			do_auto = 1;
		}
		else
		{
			say("DCC %s collision for %s:%s", type, user,
				description);
			send_ctcp_reply(user, UP("DCC"), "DCC %s collision occured while connecting to %s (%s)", type, my_nickname(), description);
			goto out;
		}
	}
	if (Client->flags & DCC_ACTIVE)
	{
		say("Received DCC %s request from %s while previous session still active", type, user);
		goto out;
	}
	Client->flags |= DCC_OFFER;

	sscanf(CP(port), "%u", &TempInt);
	if (TempInt < 1024)
	{
		say("DCC %s (%s) request from %s rejected [addr = %s, port = %d]", type, description, user, address, TempInt);
		dcc_erase(Client);
		goto out;
	}

	for (s = address; *s; s++)
		if (!isdigit(*s))
			break;
	if (*s)
		malloc_strcpy(&Client->remname, address);
	else
	{
		/* This is definately an IPv4 address, convert to a.b.c.d */
		u_char	buf[20];
		u_long	TempLong;
		u_int	dots[4], i;

		sscanf(CP(address), "%lu", &TempLong);
		if (0 == TempLong)
		{
			say("DCC %s (%s) request from %s rejected [addr = %s, port = %d]", type, description, user, address, (int)TempLong);
			dcc_erase(Client);
			goto out;
		}
		for (i = 0; i < 4; i++)
		{
			dots[i] = TempLong & 0xff;
			TempLong >>= 8;
		}
		snprintf(CP(buf), sizeof buf, "%u.%u.%u.%u", dots[3], dots[2], dots[1], dots[0]);
		malloc_strcpy(&Client->remname, buf);
	}
	Client->remport = TempInt;
	if (do_auto)
	{
		say("DCC CHAT already requested by %s, connecting to [%s:%s] ...", user, Client->remname, port);
		dcc_chat(user);
	}
	else if (Client->filesize)
		say("DCC %s (%s %lu) request received from %s [%s:%s]", type, description, (u_long)Client->filesize, user, Client->remname, port);
	else
		say("DCC %s (%s) request received from %s [%s:%s]", type, description, user, Client->remname, port);
	if (do_beep_on_level(LOG_CTCP))
		beep_em(1);
out:
	set_lastlog_msg_level(lastlog_level);
	new_free(&cmd);
}

static	void
process_incoming_chat(DCC_list *Client)
{
	SOCKADDR_STORAGE	remaddr;
	socklen_t	sra;
	u_char	tmp[BIG_BUFFER_SIZE];
	u_char	tmpuser[IRCD_BUFFER_SIZE];
	u_char	*s, *bufptr;
	long	bytesread;
	int	old_timeout;
	size_t	len;

	save_message_from();
	message_from(Client->user, LOG_DCC);
	if (Client->flags & DCC_WAIT)
	{
		sra = sizeof remaddr;
		Client->write = accept(Client->read, (struct sockaddr *)
			&remaddr, &sra);
		if (Client->write == -1)
		{
			say("DCC chat connect to %s failed in accept: %s",
			    Client->user, strerror(errno));
			Client->read = -1;
			Client->flags |= DCC_DELETE;
			goto out;
		}
		new_close(Client->read);
		Client->read = Client->write;
		Client->flags &= ~DCC_WAIT;
		Client->flags |= DCC_ACTIVE;
		say("DCC chat connection to %s[%s] established", Client->user, dcc_sockname(&remaddr, sra));
		Client->starttime = time(NULL);
		goto out;
	}
	s = Client->buffer;
	bufptr = tmp;
	if (s && *s)
	{
		len = my_strlen(s);
		my_strncpy(tmp, s, len);
		bufptr += len;
	}
	else
		len = 0;
	old_timeout = dgets_timeout(1);
	bytesread = dgets(bufptr, ((sizeof(tmp)/2) - len), Client->read);
	(void) dgets_timeout(old_timeout);
	switch ((int)bytesread)
	{
	case -2:
		/* SSL retry */
		break;
	case -1:
		add_to_dcc_buffer(Client, bufptr);
		if (Client->buffer && (my_strlen(Client->buffer) > sizeof(tmp)/2))
		{
			new_free(&Client->buffer);
			say("*** dropped long DCC CHAT message from %s", Client->user);
		}
		break;
	case 0:
		say("DCC CHAT connection to %s lost: %s", Client->user,
		    dgets_errno() == -1 ?
			"Remote end closed connection" : strerror(dgets_errno()));
		new_close(Client->read);
		Client->read = Client->write = -1;
		Client->flags |= DCC_DELETE;
		break;
	default:
		new_free(&Client->buffer);
		len = my_strlen(tmp);
		if (len > sizeof(tmp)/2)
			len = sizeof(tmp)/2;
		Client->bytes_read += len;
		*tmpuser = '=';
		strmcpy(tmpuser+1, Client->user, sizeof(tmpuser)-2);
		s = do_ctcp(tmpuser, my_nickname(), tmp);
		s[my_strlen(s) - 1] = '\0';	/* remove newline */
		if (s && *s)
		{
			/* stop dcc long messages, stupid but "safe"? */
			s[sizeof(tmp)/2-1] = '\0';
			if (do_hook(DCC_CHAT_LIST, "%s %s", Client->user, s))
			{
				if (is_away_set())
				{
					time_t	t;

					t = time(0);
					snprintf(CP(tmp), sizeof tmp, "%s <%.16s>", s, ctime(&t));
					s = tmp;
				}
				put_it("=%s= %s", Client->user, s);
				if (do_beep_on_level(LOG_CTCP))
					beep_em(1);
			}
		}
	}
out:
	restore_message_from();
}

static	void
process_incoming_listen(DCC_list *Client)
{
	SOCKADDR_STORAGE	remaddr;
	DCC_list *NewClient;
	u_char host[NI_MAXHOST], FdName[10], *Name;
	socklen_t	sra;
	int	new_socket, err;

	sra = sizeof remaddr;
	new_socket = accept(Client->read, (struct sockaddr *) &remaddr, &sra);
	err = getnameinfo((struct sockaddr *) &remaddr, sra, CP(host), NI_MAXHOST, 0, 0, 0);
	if (err != 0)
	{
		my_strncpy(host, "[unknown]", sizeof(host) - 1);
		yell("process_incoming_listen: getnameinfo failed?");
	}
	Name = host;

	snprintf(CP(FdName), sizeof FdName, "%d", new_socket);
	NewClient = dcc_searchlist(Name, FdName, DCC_RAW, 1, NULL);
	NewClient->starttime = time(NULL);
	NewClient->read = NewClient->write = new_socket;
	NewClient->flags |= DCC_ACTIVE;
	NewClient->bytes_read = NewClient->bytes_sent = 0L;
	malloc_strcpy(&NewClient->remname, Name);
	if (SS_FAMILY(&remaddr) == PF_INET)
	{
		struct sockaddr_in *in = (struct sockaddr_in *) &remaddr;
		NewClient->remport = in->sin_port;
	}
#ifdef INET6
	else
	if (SS_FAMILY(&remaddr) == PF_INET6)
	{
		struct sockaddr_in6 *in = (struct sockaddr_in6 *) &remaddr;
		NewClient->remport = in->sin6_port;
	}
#endif
	save_message_from();
	message_from(NewClient->user, LOG_DCC);
	if (do_hook(DCC_RAW_LIST, "%s %s N %d", NewClient->user,
						NewClient->description,
						Client->write))
		say("DCC RAW connection to %s on %s via %d established",
					NewClient->description,
					NewClient->user,
					Client->write);
	restore_message_from();
}

static	void
process_incoming_raw(DCC_list *Client)
{
	u_char	tmp[BIG_BUFFER_SIZE];
	u_char	*s, *bufptr;
	long	bytesread;
	int     old_timeout;
	size_t	len;

	save_message_from();
	message_from(Client->user, LOG_DCC);

	s = Client->buffer;
	bufptr = tmp;
	if (s && *s)
	{
		len = my_strlen(s);
		my_strncpy(tmp, s, len);
		bufptr += len;
	}
	else
		len = 0;
	old_timeout = dgets_timeout(1);
	switch((int)(bytesread = dgets(bufptr, ((sizeof(tmp)/2) - len), Client->read)))
	{
	case -2:
		/* SSL retry */
		break;
	case -1:
		add_to_dcc_buffer(Client, bufptr);
		if (Client->buffer && (my_strlen(Client->buffer) > sizeof(tmp)/2))
		{
			new_free(&Client->buffer);
			say("*** dropping long DCC message from %s", Client->user);
		}
		break;
	case 0:
		if (do_hook(DCC_RAW_LIST, "%s %s C",
				Client->user, Client->description))
			say("DCC RAW connection to %s on %s lost",
				Client->user, Client->description);
		new_close(Client->read);
		Client->read = Client->write = -1;
		Client->flags |= DCC_DELETE;
		(void) dgets_timeout(old_timeout);
		break;
	default:
		new_free(&Client->buffer);
		len = my_strlen(tmp);
		if (len > sizeof(tmp) / 2)
			len = sizeof(tmp) / 2;
		tmp[len - 1] = '\0';
		Client->bytes_read += len;
		if (do_hook(DCC_RAW_LIST, "%s %s D %s",
				Client->user, Client->description, tmp))
			say("Raw data on %s from %s: %s",
				Client->user, Client->description, tmp);
		(void) dgets_timeout(old_timeout);
	}
	restore_message_from();
}

static	void
process_outgoing_file(DCC_list *Client)
{
	SOCKADDR_STORAGE	remaddr;
	socklen_t sra;
	u_char	tmp[BIG_BUFFER_SIZE];
	uint32_t bytesrecvd;
	int	bytesread;
	int	BlockSize;

	save_message_from();
	message_from(Client->user, LOG_DCC);
	if (Client->flags & DCC_WAIT)
	{
		sra = sizeof remaddr;
		Client->write = accept(Client->read,
				(struct sockaddr *) &remaddr, &sra);
		new_close(Client->read);
		Client->read = Client->write;
		Client->flags &= ~DCC_WAIT;
		Client->flags |= DCC_ACTIVE;
		Client->bytes_sent = 0L;
		Client->starttime = time(NULL);
		say("DCC SEND connection to %s[%s] established", Client->user,
		    dcc_sockname(&remaddr, sra));
	}
	else
	{
		if ((bytesread = recv(Client->read, (char *) &bytesrecvd, sizeof(uint32_t), 0)) < sizeof(uint32_t))
		{
	       		say("DCC SEND:%s connection to %s lost: %s", Client->description, Client->user, strerror(errno));
			new_close(Client->read);
			Client->read = Client->write = (-1);
			Client->flags |= DCC_DELETE;
			new_close(Client->file);
			goto out;
		}
		else
			if (ntohl(bytesrecvd) != Client->bytes_sent)
				goto out;
	}
	BlockSize = get_int_var(DCC_BLOCK_SIZE_VAR);
	if (BlockSize > sizeof tmp)
		BlockSize = sizeof tmp;
	else if (BlockSize < 16)
		BlockSize = 16;
	if ((bytesread = read(Client->file, tmp, sizeof tmp)) != 0)
	{
		send(Client->write, CP(tmp), (size_t)bytesread, 0);
		Client->bytes_sent += bytesread;
	}
	else
	{
		/*
		 * put_it() can't be calld with a float.
		 */

		u_char	lame_put_it[10];	/* should be plenty */
		time_t	xtime = time(NULL) - Client->starttime;
		double	sent = (double)Client->bytes_sent;

		if (sent <= 0)
			sent = 1;
		sent /= (double)1024.0;
		if (xtime <= 0)
			xtime = 1;
		snprintf(CP(lame_put_it), sizeof lame_put_it, "%2.4g", (sent / (double)xtime));
		say("DCC SEND:%s to %s completed %s kb/sec",
			Client->description, Client->user, lame_put_it);
		new_close(Client->read);
		Client->read = Client->write = -1;
		Client->flags |= DCC_DELETE;
		new_close(Client->file);
	}
out:
	restore_message_from();
}

static	void
process_incoming_file(DCC_list *Client)
{
	u_char	tmp[BIG_BUFFER_SIZE];
	uint32_t bytestemp;
	ssize_t	bytesread;

	save_message_from();
	message_from(Client->user, LOG_DCC);

	if ((bytesread = recv(Client->read, CP(tmp), sizeof tmp, 0)) <= 0)
	{
		/*
		 * put_it() can't be calld with a float.
		 */

		u_char    lame_put_it[10];        /* should be plenty */
		time_t	xtime = time(NULL) - Client->starttime;
		double	sent = (double)Client->bytes_read;

		if (sent <= 0)
			sent = 1;
		sent /= (double)1024.0;
		if (xtime <= 0)
			xtime = 1;
		snprintf(CP(lame_put_it), sizeof lame_put_it, "%2.4g", (sent / (double)xtime));
		say("DCC GET:%s from %s completed %s kb/sec",
			Client->description, Client->user, lame_put_it);
		goto file_cleanup;
	}
	if (write(Client->file, tmp, (size_t)bytesread) != bytesread)
	{
		say("DCC GET:%s from %s file write failed: %s",
			Client->description, Client->user, strerror(errno));
		goto file_cleanup;
	}
	Client->bytes_read += bytesread;
	bytestemp = htonl(Client->bytes_read);
	if (send(Client->write, (char *)&bytestemp, sizeof(uint32_t), 0) != sizeof(uint32_t))
	{
		say("DCC GET:%s from %s remote ack send failed: %s",
			Client->description, Client->user, strerror(errno));
		goto file_cleanup;
	}
	goto out;

file_cleanup:
	new_close(Client->read);
	new_close(Client->file);
	Client->read = Client->write = (-1);
	Client->flags |= DCC_DELETE;
out:
	restore_message_from();
}

/* flag == 1 means show it.  flag == 0 used by redirect */

void
dcc_message_transmit(u_char *user, u_char *text, int type, int flag)
{
	DCC_list	*Client;
	u_char	tmp[BIG_BUFFER_SIZE];
	u_char	nickbuf[128];
	u_char	thing = '\0';
	u_char	*host = NULL;
	crypt_key	*key;
	u_char	*line;
	int	lastlog_level;
	int	list = 0;
	size_t	len;

	lastlog_level = set_lastlog_msg_level(LOG_DCC);
	switch(type)
	{
	case DCC_CHAT:
		host = UP("chat");
		thing = '=';
		list = SEND_DCC_CHAT_LIST;
		break;
	case DCC_RAW:
		host = next_arg(text, &text);
		if (!host)
		{
			say("No host specified for DCC RAW");
			goto out1;
		}
		break;
	}
	save_message_from();
	message_from(user, LOG_DCC);
	if (!(Client = dcc_searchlist(host, user, type, 0, NULL)) || !(Client->flags&DCC_ACTIVE))
	{
		say("No active DCC %s:%s connection for %s", dcc_types[type], host ? host : (u_char *) "<any>", user);
		goto out;
	}
#ifdef DCC_CNCT_PEND
	/*
	 * XXX - should make this buffer
	 *     - just for dcc chat ?  maybe raw dcc too.  hmm.
	 */
	if (Client->flags & DCC_CNCT_PEND)
	{
		say("DCC %s:%s connection to %s is still connecting...", dcc_types[type], host ? host : (u_char *) "<any>", user);
		goto out;
	}
#endif /* DCC_DCNT_PEND */
	strmcpy(tmp, text, sizeof tmp);
	if (type == DCC_CHAT) {
		nickbuf[0] = '=';
		strmcpy(nickbuf+1, user, sizeof(nickbuf) - 2);

		if ((key = is_crypted(nickbuf)) == 0 || (line = crypt_msg(tmp, key, 1)) == 0)
			line = tmp;
	}
	else
		line = tmp;
#ifdef HAVE_WRITEV
	{
		struct iovec iov[2];

		iov[0].iov_base = CP(line);
		iov[0].iov_len = len = my_strlen(line);
		iov[1].iov_base = "\n";
		iov[1].iov_len = 1;
		len++;
		(void)writev(Client->write, iov, 2);
	}
#else
	strmcat(line, "\n", (size_t)((line == tmp) ? sizeof tmp : CRYPT_BUFFER_SIZE));
	len = my_strlen(line);
	(void)send(Client->write, line, len, 0);
#endif
	Client->bytes_sent += len;
	if (flag && type != DCC_RAW) {
		if (do_hook(list, "%s %s", Client->user, text))
			put_it("=> %c%s%c %s", thing, Client->user, thing, text);
	}
out:
	restore_message_from();
out1:
	set_lastlog_msg_level(lastlog_level);
	return;
}

void
dcc_chat_transmit(u_char *user,	u_char *text)
{
	dcc_message_transmit(user, text, DCC_CHAT, 1);
}

static	void
dcc_send_raw(u_char *args)
{
	u_char	*name;

	if (!(name = next_arg(args, &args)))
	{
		int	lastlog_level;

		lastlog_level = set_lastlog_msg_level(LOG_DCC);
		say("No name specified for DCC RAW");
		(void) set_lastlog_msg_level(lastlog_level);
		return;
	}
	dcc_message_transmit(name, args, DCC_RAW, 1);
}

/*
 * dcc_time: Given a time value, it returns a string that is in the
 * format of "hours:minutes:seconds month day year" .  Used by
 * dcc_list() to show the start time.
 */
static	u_char	*
dcc_time(time_t the_time)
{
	struct	tm	*btime;
	u_char	*buf;
	static	char	*months[] =
	{
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};

	btime = localtime(&the_time);
	buf = (u_char *) malloc(22);
	if (snprintf(CP(buf), 22, "%-2.2d:%-2.2d:%-2.2d %s %-2.2d %d",
			btime->tm_hour, btime->tm_min, btime->tm_sec,
			months[btime->tm_mon], btime->tm_mday,
			btime->tm_year + 1900))
		return buf;
	else
		return empty_string();
}

#define DCC_FORM "%-7.7s %-9.9s %-10.10s %-20.20s %-8.8s %-8.8s %s"
#define DCC_FORM_HOOK "%s %s %s %s %s %s %s"
#define DCC_FORM_HEADER \
	"Type", "Nick", "Status", "Start time", "Sent", "Read", "Arguments"

void
dcc_list(u_char *args)
{
	DCC_list	*Client;
	unsigned	flags;
	int	lastlog_level;

	lastlog_level = set_lastlog_msg_level(LOG_DCC);
	if (do_hook(DCC_LIST_LIST, DCC_FORM_HOOK, DCC_FORM_HEADER))
		put_it(DCC_FORM, DCC_FORM_HEADER);
	for (Client = ClientList ; Client != NULL ; Client = Client->next)
	{
		u_char	sent[9],
			rd[9];
		u_char	*timestr;

		snprintf(CP(sent), sizeof sent, "%ld", (long)Client->bytes_sent);
		snprintf(CP(rd), sizeof rd, "%ld", (long)Client->bytes_read);
		timestr = (Client->starttime) ? dcc_time(Client->starttime) : empty_string();
		flags = Client->flags;

#ifdef DCC_DCNT_PEND
#define DCC_CONT_PEND_FORM	flags & DCC_CNCT_PEND ?	"Connecting" :
#else /* DCC_DCNT_PEND */
#define DCC_CONT_PEND_FORM	/* nothing */
#endif /* DCC_DCNT_PEND */

#define DCC_FORM_BODY		dcc_types[flags & DCC_TYPES],		\
				Client->user,				\
				flags & DCC_OFFER ? "Offered" :		\
				flags & DCC_DELETE ? "Closed" :		\
				flags & DCC_ACTIVE ? "Active" :		\
				flags & DCC_WAIT ? "Waiting" :		\
				DCC_CONT_PEND_FORM			\
				"Unknown",				\
				timestr,				\
				sent,					\
				rd,					\
				Client->description

		if (do_hook(DCC_LIST_LIST, DCC_FORM_HOOK, DCC_FORM_BODY))
			put_it(DCC_FORM, DCC_FORM_BODY);
		if (*timestr)
			new_free(&timestr);
	}
	(void) set_lastlog_msg_level(lastlog_level);
}

#undef DCC_FORM
#undef DCC_FORM_HOOK
#undef DCC_FORM_HEADER
#undef DCC_CONT_PEND_FORM
#undef DCC_FORM_BODY

static	void
dcc_close(u_char *args)
{
	DCC_list	*Client;
	unsigned	flags;
	u_char	*Type;
	u_char	*user;
	u_char	*description;
	int	CType;
	u_char	*cmd = NULL;
	int	lastlog_level;

	lastlog_level = set_lastlog_msg_level(LOG_DCC);
	if (!(Type = next_arg(args, &args)) || !(user=next_arg(args, &args)))
	{
		say("you must specify a type and nick for DCC CLOSE");
		goto out;
	}
	description = next_arg(args, &args);
	malloc_strcpy(&cmd, Type);
	upper(cmd);
	for (CType = 0; dcc_types[CType] != NULL; CType++)
		if (!my_strcmp(cmd, dcc_types[CType]))
			break;
	if (!dcc_types[CType])
		say("Unknown DCC type: %s", Type);
	else if ((Client = dcc_searchlist(description, user, CType, 0, description)))
	{
		flags = Client->flags;
		if (flags & DCC_DELETE)
			goto out;
		if ((flags & DCC_WAIT) || (flags & DCC_ACTIVE))
		{
			new_close(Client->read);
			if (Client->file)
				new_close(Client->file);
		}
		say("DCC %s:%s to %s closed", Type,
			description ? description : (u_char *) "<any>", user);
		dcc_erase(Client);
	}
	else
		say("No DCC %s:%s to %s found", Type,
			description ? description : (u_char *) "<any>", user);
	new_free(&cmd);
out:
	(void) set_lastlog_msg_level(lastlog_level);
}

/* this depends on dcc_rename() setting loglevel */
static void
dcc_chat_rename(u_char *args)
{
	DCC_list	*Client;
	u_char	*user;
	u_char	*temp;

	if (!(user = next_arg(args, &args)) || !(temp = next_arg(args, &args)))
	{
		say("you must specify a current DCC CHAT connection, and a new name for it");
		return;
	}
	if (dcc_searchlist(UP("chat"), temp, DCC_CHAT, 0, NULL))
	{
		say("You already have a DCC CHAT connection with %s, unable to rename.", temp);
		return;
	}
	if ((Client = dcc_searchlist(UP("chat"), user, DCC_CHAT, 0, NULL)))
	{
		new_free(&(Client->user));
		malloc_strcpy(&(Client->user), temp);
		say("DCC CHAT connection with %s renamed to %s", user, temp);
	}
	else
		say("No DCC CHAT connection with %s", user);
}


static	void
dcc_rename(u_char *args)
{
	DCC_list	*Client;
	u_char	*user;
	u_char	*description;
	u_char	*newdesc;
	u_char	*temp;
	int	lastlog_level;

	lastlog_level = set_lastlog_msg_level(LOG_DCC);
	if ((user = next_arg(args, &args)) && my_strnicmp(user, UP("-chat"), my_strlen(user)) == 0)
	{
		dcc_chat_rename(args);
		return;
	}
	if (!user || !(temp = next_arg(args, &args)))
	{
		say("you must specify a nick and new filename for DCC RENAME");
		goto out;
	}
	if ((newdesc = next_arg(args, &args)) != NULL)
		description = temp;
	else
	{
		newdesc = temp;
		description = NULL;
	}
	if ((Client = dcc_searchlist(description, user, DCC_FILEREAD, 0, NULL)))
	{
		if (!(Client->flags & DCC_OFFER))
		{
			say("Too late to rename that file");
			goto out;
		}
		new_free(&(Client->description));
		malloc_strcpy(&(Client->description), newdesc);
		say("File %s from %s renamed to %s",
			 description ? description : (u_char *) "<any>", user, newdesc);
	}
	else
		say("No file %s from %s found",
			description ? description : (u_char *) "<any>", user);
out:
	(void) set_lastlog_msg_level(lastlog_level);
}

/*
 * close_all_dcc:  We call this when we create a new process so that
 * we don't leave any fd's lying around, that won't close when we
 * want them to..
 */

void
close_all_dcc(void)
{
	DCC_list *Client;

	while ((Client = ClientList))
		dcc_erase(Client);
}

static	void
add_to_dcc_buffer(DCC_list *Client, u_char *buf)
{
	if (buf && *buf)
	{
		if (Client->buffer)
			malloc_strcat(&Client->buffer, buf);
		else
			malloc_strcpy(&Client->buffer, buf);
	}
}

/*
 * backend for the $dcclist() function.
 */
u_char *
dcc_list_func(u_char *nick)
{
	u_char *result;
	DCC_list *Client;
	size_t len = 0;
	int i;

	for (i = 0, Client = ClientList; Client != NULL; Client = Client->next)
		if (!my_stricmp(nick, Client->user))
			len += 3;

	result = new_malloc(len + 1);

	for (i = 0, Client = ClientList; Client != NULL; Client = Client->next)
		if (!my_stricmp(nick, Client->user))
		{
			int b = Client->flags;
			int a = (b & DCC_TYPES);

			result[i++] =
					  (a == DCC_CHAT)		? 'C' /* CHAT */
					: (a == DCC_FILEOFFER)		? 'S' /* SEND */
					: (a == DCC_FILEREAD)		? 'G' /* GET */
					: (a == DCC_RAW_LISTEN)		? 'L' /* RAW_LISTEN */
					: (a == DCC_RAW)      		? 'R' /* RAW */
					: 'x';

			result[i++] =
					  (b & DCC_OFFER)		? 'O' /* OFFERED */
					: (b & DCC_DELETE)		? 'C' /* CLOSED */
					: (b & DCC_ACTIVE)		? 'A' /* ACTIVE */
					: (b & DCC_WAIT)		? 'W' /* WAITING */
					: 'x';

			result[i++] = ' ';
		}

	result[i] = '\0';

	return (result);
}

/*
 * backend for the $chatpeers() function.
 */
u_char *
dcc_chatpeers_func(void)
{
	u_char *result;
	DCC_list *Client;
	int	notfirst = 0;
	size_t len = 0;

	/* calculate size */
	for (Client = ClientList; Client != NULL; Client = Client->next)
		if ((Client->flags & (DCC_CHAT|DCC_ACTIVE)) == (DCC_CHAT|DCC_ACTIVE))
			len += (my_strlen(Client->user) + 1);
	result = new_malloc(len);
	*result = '\0';

	for (Client = ClientList; Client != NULL; Client = Client->next)
		if ((Client->flags & (DCC_CHAT|DCC_ACTIVE)) == (DCC_CHAT|DCC_ACTIVE))
		{
			if (notfirst)
				my_strmcat(result, ",", len);
			else
				notfirst = 1;
			my_strmcat(result, Client->user, len);
		}

	return (result);
}

/* 
 * set_dcchost: This sets the source host for subsequent connections.
 */
void
set_dcchost(u_char *dcchost)
{
	if (!dcchost || !*dcchost)
		new_free(&dcc_source_host);
	else
		malloc_strcpy(&dcc_source_host, dcchost);
}

u_char *
get_dcchost(void)
{
	return dcc_source_host;
}
