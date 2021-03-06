/*
 * wserv.c - little program to be a pipe between a screen or
 * xterm window to the calling ircII process.
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
 */

/*
 * Works by opening up the unix domain socket that ircII bind's
 * before calling wserv, and which ircII also deleted after the
 * connection has been made.
 */

#include "irc.h"
IRCII_RCSID("@(#)$eterna: wserv.c,v 1.51 2014/03/17 18:51:42 mrg Exp $");

#include "defs.h"

#ifdef HAVE_SYS_UN_H

# include "ircterm.h"

# include <sys/un.h>

# ifdef HAVE_SYS_IOCTL_H
#  include <sys/ioctl.h>
# endif /* HAVE_SYS_IOCTL_H */

static	int	control_sock;

/* declare the signal handler */
# if defined(SIGWINCH)
void	got_sigwinch(int);
# endif /* SIGWINCH */

int main(int, char *[], char *[]);
/*
 * ./wserv /path/to/socket /path/to/control
 */
int
main(int argc, char *argv[], char *envp[])
{
	char	lbuf[1024];
	struct	sockaddr_un *addr = (struct sockaddr_un *) lbuf, esock;
	size_t	nread;
	fd_set	reads;
	int	s;
	int	zero_count = 0;
	char	*tmp;

	/* Set up the signal hander to pass SIGWINCH to ircII */
#if defined(SIGWINCH)
	(void) MY_SIGNAL(SIGWINCH, (sigfunc *)got_sigwinch, 0);
#endif /* SIGWINCH */

	if (3 != argc)    /* no socket is passed */
		return 0;
#ifdef	SOCKS
	SOCKSinit(*argv);
#endif /* SOCKS */

	/*
	 * Set up the socket, from the path passed, connect it, etc.
	 */
	addr->sun_family = AF_UNIX;
	strcpy(addr->sun_path, argv[1]);
	s = socket(AF_UNIX, SOCK_STREAM, 0);
	if (0 > connect(s, (struct sockaddr *) addr, (int)(2 +
						strlen(addr->sun_path))))
		return 0;

	/* 
	 * Next connect to the control socket.
	 */
	esock.sun_family = AF_UNIX;
	strcpy(esock.sun_path, argv[2]);
	control_sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (0 > connect(control_sock, (struct sockaddr *)&esock, (int)(2 +
						strlen(esock.sun_path))))
		return 0;

	/*
	 * first line to for a wserv program is the tty.  this is so ircii
	 * can grab the size of the tty, and have it changed.
	 */

	tmp = ttyname(0);
	if (write(s, tmp, strlen(tmp)) <= 0 ||
	    write(s, "\n", 1) <= 0)
		return 0;

	/*
	 * Initialise with the minimal wterm.c
	 */
	term_init();

	/*
	 * Tell the master our screen size.
	 */
#if defined(SIGWINCH)
	got_sigwinch(0);
#endif

	/*
	 * The select call..  reads from the socket, and from the window..
	 * and pipes the output from out to the other..  nice and simple
	 */
	while (1)
	{
		FD_ZERO(&reads);
		FD_SET(0, &reads);
		FD_SET(s, &reads);
		switch (select(s + 1, &reads, NULL, NULL, NULL))
		{
		case 0:
			if (++zero_count < 10)
				break;
			/* FALLTHROUGH */
		case -1:
			if (errno == EINTR)
				continue;
			exit(-1);
		}
		zero_count = 0;

		if (FD_ISSET(0, &reads))
		{
			if (0 != (nread = read(0, lbuf, sizeof(lbuf))))
			{
				if (write(s, lbuf, nread) != nread)
					return 0;
			}
			else
				return 0;
		}
		if (FD_ISSET(s, &reads))
		{
			if (0 != (nread = read(s, lbuf, sizeof(lbuf))))
			{
				if (write(0, lbuf, nread) != nread)
					return 0;
			}
			else
				return 0;
		}
	}
}

/* got_sigwinch: we got a SIGWINCH, so we send it back to ircII */
# if defined(SIGWINCH)
void
got_sigwinch(int signo)
{
#  ifdef TIOCGWINSZ
	static int old_li = -1, old_co = -1;
	int li, co;
	struct	winsize window;
#  endif

#  ifdef TIOCGWINSZ

	if (ioctl(STDIN_FILENO, TIOCGWINSZ, &window) < 0)
	{
		li = -1;
		co = -1;
	}
	else
	{
		if ((li = window.ws_row) == 0)
			li = -1;
		if ((co = window.ws_col) == 0)
			co = -1;
	}

	co--;
	if ((old_li != li) || (old_co != co))
	{
		char buf[32];
		size_t len;

		sprintf(buf, "%d,%d\n", li, co);
		len = strlen(buf);
		if (write(control_sock, buf, len) != len)
		{
			perror("write failed in wserv");
			sleep(1);
		}
		else
		{
			old_li = li;
			old_co = co;
		}
	}
#  endif /* TIOCGWINSZ */
}
# endif /* SIGWINCH */

#else

int
main(void)
{
	return 0;
}

#endif /* HAVE_SYS_UH_H */
