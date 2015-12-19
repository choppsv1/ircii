/*
 * ircio.c: A quaint little program to make irc life PING free 
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
 */

#include "irc.h"
IRCII_RCSID("@(#)$eterna: ircio.c,v 2.34 2014/07/11 09:56:27 mrg Exp $");

#include <errno.h>

#include "defs.h"
#include "newio.h"

#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
static	int	connect_to_unix(char *);
#endif /* HAVE_SYS_UN_H */

#undef NON_BLOCKING

	void	new_free(char **);
	void	*new_malloc(size_t);
static	int	connect_by_number(char *, char *);
	int	main(int, char *[], char *[]);

void	*
new_malloc(size_t size)
{
	void	*ptr;

	if ((ptr = malloc(size)) == NULL)
	{
		printf("-1 0\n");
		exit(1);
	}
	return (ptr);
}

/*
 * new_free:  Why do this?  Why not?  Saves me a bit of trouble here and
 * there 
 */
void
new_free(char **ptr)
{
	if (*ptr)
	{
		free(*ptr);
		*ptr = 0;
	}
}

#ifdef DEBUG
void
debug(int level, char *fmt, ...)
{
}
#endif

ssize_t
ssl_read(SslInfo *info, int fd, void *buf, size_t buflen)
{
	return read(fd, buf, buflen);
}

/*
 * connect_by_number Performs a connecting to socket 'service' on host
 * 'host'.  Host can be a hostname or ip-address.  If 'host' is null, the
 * local host is assumed.   The parameter full_hostname will, on return,
 * contain the expanded hostname (if possible).  Note that full_hostname is a
 * pointer to a char *, and is allocated by connect_by_numbers() 
 *
 * Errors: 
 *
 * -1 get service failed 
 *
 * -2 get host failed 
 *
 * -3 socket call failed 
 *
 * -4 connect call failed 
 */
static	int
connect_by_number(char *service, char *host)
{
	int	s = -1, err;
	char	buf[256];
	struct	addrinfo hints, *res = 0, *res0 = 0;

	if (host == NULL)
	{
		gethostname(buf, sizeof(buf));
		host = buf;
	}
	memset(&hints, 0, sizeof hints);
	hints.ai_flags = 0;
	hints.ai_protocol = 0;
	hints.ai_addrlen = 0;
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_UNSPEC;
	err = getaddrinfo(host, service, &hints, &res0);
	if (err != 0) 
		return -2;
	for (res = res0; res; res = res->ai_next)
	{
		err = 0;
		if ((s = socket(res->ai_family, res->ai_socktype,
		    res->ai_protocol)) < 0)
			continue;
		set_socket_options(s);
		err = connect(s, res->ai_addr, res->ai_addrlen);
		if (err == 0)
			break;
		close(s);
	}
	freeaddrinfo(res0);
	if (err)
		return -4;
	return (s);
}

/*
 * ircio: This little program connects to the server (given as arg 1) on
 * the given port (given as arg 2).  It then accepts input from stdin and
 * sends it to that server. Likewise, it reads stuff sent from the server and
 * sends it to stdout.  Simple?  Yes, it is.  But wait!  There's more!  It
 * also intercepts server PINGs and automatically responds to them.   This
 * frees up the process that starts ircio (such as IRCII) to pause without
 * fear of being pooted off the net. 
 *
 * Future enhancements: No-blocking io.  It will either discard or dynamically
 * buffer anything that would block. 
 */
int
main(int argc, char *argv[], char *envp[])
{
	int	des;
	fd_set	rd;
	int	done = 0,
		c;
	char	*ptr,
		lbuf[BUFSIZ + 1],
		pong[BUFSIZ + 1];
#ifdef NON_BLOCKING
	char	block_buffer[BUFSIZ + 1];
	fd_set	*wd_ptr = NULL,
		wd;
	int	wrote;
#endif /* NON_BLOCKING */

	if (argc < 3)
		exit(1);
#ifdef	SOCKS
	SOCKSinit(*argv);
#endif /* SOCKS */
#ifdef HAVE_SYS_UN_H
	if (*argv[1] == '/')
		des = connect_to_unix(argv[1]);
	else
#endif /* HAVE_SYS_UN_H */
		des = connect_by_number(argv[2], argv[1]);
	if (des < 0)
		exit(des);
	fflush(stdout);

	(void) MY_SIGNAL(SIGTERM, (sigfunc *) SIG_IGN, 0);
	(void) MY_SIGNAL(SIGSEGV, (sigfunc *) SIG_IGN, 0);
	(void) MY_SIGNAL(SIGBUS, (sigfunc *) SIG_IGN, 0);
	(void) MY_SIGNAL(SIGPIPE, (sigfunc *) SIG_IGN, 0);
#ifdef SIGWINCH
	(void) MY_SIGNAL(SIGWINCH, (sigfunc *) SIG_IGN, 0);
#endif /* SIGWINCH */

#ifdef NON_BLOCKING
	if (fcntl(1, F_SETFL, FNDELAY))
		exit(1);
#endif /* NON_BLOCKING */
	while (!done)
	{
		fflush(stderr);
		FD_ZERO(&rd);
		FD_SET(0, &rd);
		FD_SET(des, &rd);
#ifdef NON_BLOCKING
		if (wd_ptr)
		{
			FD_ZERO(wd_ptr);
			FD_SET(1, wd_ptr);
		}
		switch (new_select(&rd, wd_ptr, NULL))
		{
#else
		switch (new_select(&rd, NULL, NULL))
		{
#endif /* NON_BLOCKING */
		case -1:
		case 0:
			break;
		default:
#ifdef NON_BLOCKING
			if (wd_ptr)
			{
				if (FD_ISSET(1, wd_ptr))
				{
					c = strlen(block_buffer);
					if ((wrote = write(1, block_buffer,
							c)) == -1)
					{
						wd_ptr = &wd;
					}
					else if (wrote < c)
					{
						strcpy(block_buffer,
							&(block_buffer[wrote]));
						wd_ptr = &wd;
					}
					else
						wd_ptr = NULL;
				}
			}
#endif /* NON_BLOCKING */
			if (FD_ISSET(0, &rd))
			{
				c = dgets(UP(lbuf), BUFSIZ, 0);
				switch (c) {
				case -1:
				case 0:
					done = 1;
					break;
				default:
					if (write(des, lbuf, (size_t)c) != c)
						done = 1;
				}
			}
			if (FD_ISSET(des, &rd))
			{
				c = dgets(UP(lbuf), BUFSIZ, des);
				switch (c) {
				case -1:
				case 0:
					done = 1;
					break;
				default:
					if (strncmp(lbuf, "PING ", 5) == 0)
					{
						if ((ptr = (char *) 
						    my_index(lbuf, ' ')) != NULL)
						{
							snprintf(pong, sizeof pong, "PONG user@host %s\n", ptr + 1);
							if (write(des, pong, strlen(pong)) != strlen(pong))
								done = 1;
						}
					}
					else
					{
#ifdef NON_BLOCKING
						if ((wrote = write(1, lbuf,
								(size_t)c)) == -1)
							wd_ptr = &wd;
						else if (wrote < c)
						{
							strcpy(block_buffer,
							    &(lbuf[wrote]));
							wd_ptr = &wd;
						}
#else
						if (write(1, lbuf, (size_t)c) != c)
							done = 1;
#endif /* NON_BLOCKING */
					}
				}
			}
		}
	}
	return 0;
}


#ifdef HAVE_SYS_UN_H
/*
 * Connect to a UNIX domain socket. Only works for servers.
 * submitted by Avalon for use with server 2.7.2 and beyond.
 */
static	int
connect_to_unix(char *path)
{
	struct	sockaddr_un	un;
	int	sock;

	sock = socket(AF_UNIX, SOCK_STREAM, 0);

	un.sun_family = AF_UNIX;
	strcpy(un.sun_path, path);
	if (connect(sock, (struct sockaddr *)&un, (int)strlen(path)+2) == -1)
	{
		new_close(sock);
		return -1;
	}
	return sock;
}
#endif /* HAVE_SYS_UN_H */
