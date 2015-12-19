/*
 * newio.c: This is some handy stuff to deal with file descriptors in a way
 * much like stdio's FILE pointers 
 *
 * IMPORTANT NOTE:  If you use the routines here-in, you shouldn't switch to
 * using normal reads() on the descriptors cause that will cause bad things
 * to happen.  If using any of these routines, use them all 
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
IRCII_RCSID("@(#)$eterna: newio.c,v 1.70 2014/07/11 09:56:27 mrg Exp $");

#include "ircaux.h"
#include "newio.h"
#include "irc_std.h"
#include "assert.h"
#include "debug.h"

#define IO_BUFFER_SIZE 4096

#ifdef FDSETSIZE
# define IO_ARRAYLEN FDSETSIZE
#else
# ifdef FD_SETSIZE
#  define IO_ARRAYLEN FD_SETSIZE
# else
#  define IO_ARRAYLEN NFDBITS
# endif /* FD_SETSIZE */
#endif /* FDSETSIZE */

typedef	struct	myio_struct
{
	char	buffer[IO_BUFFER_SIZE + 1];
	unsigned int	read_pos,
			write_pos;
	SslInfo	*ssl_info;
} MyIO;

#define IO_SOCKET 1

static	struct	timeval	right_away = { 0L, 0L };
static	MyIO	*io_rec[IO_ARRAYLEN];

static	struct	timeval	dgets_timer;
static	struct	timeval	*timer;
static	int	dgets_separator = '\n';
static	int	dgets_local_errno = 0;

static	void	init_io(void);
static	void	init_io_rec(int);

/*
 * dgets_timeout: does what you'd expect.  Sets a timeout in seconds for
 * dgets to read a line.  if second is -1, then make it a poll.
 */
time_t
dgets_timeout(int sec)
{
	time_t	old_timeout = dgets_timer.tv_sec;

	if (sec)
	{
		dgets_timer.tv_sec = (sec == -1) ? 0 : sec;
		dgets_timer.tv_usec = 0;
		timer = &dgets_timer;
	}
	else
		timer = NULL;
	return old_timeout;
}

/*
 * mainly for icb suport: changes end-of-line separator ("newline").
 * returns the old value so the changer can fix it up later...
 */
int
dgets_set_separator(int what)
{
	int	old_sep = dgets_separator;

	dgets_separator = what;
	return old_sep;
}

void
dgets_set_ssl_info(int des, SslInfo *ssl_info)
{
	init_io_rec(des);
	io_rec[des]->ssl_info = ssl_info;
	Debug(DB_NEWIO, "fd %d to %p", des, io_rec[des]->ssl_info);
}

void
dgets_clear_ssl_info(int des)
{
	if (io_rec[des])
	{
		Debug(DB_NEWIO, "fd %d was %p", des, io_rec[des]->ssl_info);
		io_rec[des]->ssl_info = NULL;
	}
}

static	void
init_io(void)
{
	static	int	first = 1;

	if (first)
	{
		int	c;

		Debug(DB_NEWIO, "setting up");
		for (c = 0; c < IO_ARRAYLEN; c++)
			io_rec[c] = NULL;
		(void) dgets_timeout(-1);
		first = 0;
	}
}

static	void
init_io_rec(des)
{
	init_io();
	if (io_rec[des] == NULL)
	{
		io_rec[des] = new_malloc(sizeof(MyIO));
		io_rec[des]->read_pos = 0;
		io_rec[des]->write_pos = 0;
		io_rec[des]->ssl_info = NULL;
		Debug(DB_NEWIO, "setting up io_rec[%d] = %p", des, io_rec[des]);
	}
}

/*
 * dgets: works much like fgets except on descriptor rather than file
 * pointers.  Returns the number of character read in.  Returns 0 on EOF and
 * -1 on a timeout (see dgets_timeout()), and -2 on SSL retry.
 */
int
dgets(u_char *str, size_t len, int des)
{
	char	*ptr;
	size_t	cnt = 0;
	ssize_t	c;
	fd_set	rd;
	MyIO	*rec;

	if (des >= IO_ARRAYLEN)
	{
		dgets_local_errno = EINVAL;
		return -1;
	}
	init_io_rec(des);

	rec = io_rec[des];

	while (1)
	{
		if (rec->read_pos == rec->write_pos)
		{
			rec->read_pos = 0;
			rec->write_pos = 0;
			FD_ZERO(&rd);
			FD_SET(des, &rd);
			switch (select(des + 1, &rd, 0, 0, timer))
			{
			case 0:
				str[cnt] = (char) 0;
				dgets_local_errno = 0;
				return (-1);
			default:
				c = ssl_read(rec->ssl_info, des, rec->buffer,
					     sizeof(rec->buffer));
				if (c <= 0)
				{
					if (c == -2)
						return -2;
					if (c == 0)
						dgets_local_errno = -1;
					else
						dgets_local_errno = errno;
					return 0;
				}
				rec->write_pos += c;
				break;
			}
		}
		ptr = rec->buffer;
		while (rec->read_pos < rec->write_pos)
		{
			char ch = str[cnt++] = ptr[(rec->read_pos)++];
			if (ch == (char)dgets_separator || cnt == len)
			{
				dgets_local_errno = 0;
				str[cnt] = (char) 0;
				return (cnt);
			}
		}
	}
}

/*
 * new_select: works just like select(), execpt I trimmed out the excess
 * parameters I didn't need.  
 */
int
new_select(fd_set *rd, fd_set *wd, struct timeval *time_out)
{
	int	i,
		set = 0;
		fd_set new;
	struct	timeval	*newtimeout,
			thetimeout;
	int	max_fd = -1;

	if (time_out)
	{
		newtimeout = &thetimeout;
		memmove(newtimeout, time_out, sizeof(struct timeval));
	}
	else
		newtimeout = NULL;
	init_io();
	FD_ZERO(&new);
	for (i = 0; i < IO_ARRAYLEN; i++)
	{
		if (i > max_fd && ((rd && FD_ISSET(i, rd)) || (wd && FD_ISSET(i, wd))))
			max_fd = i;
		if (io_rec[i] && io_rec[i]->read_pos < io_rec[i]->write_pos)
		{
			FD_SET(i, &new);
			set = 1;
		}
	}
	if (set)
	{
		set = 0;
		if (!(select(max_fd + 1, rd, wd, NULL, &right_away) > 0))
			FD_ZERO(rd);
		for (i = 0; i < IO_ARRAYLEN; i++)
		{
			if ((FD_ISSET(i, rd)) || (FD_ISSET(i, &new)))
			{
				set++;
				FD_SET(i, rd);
			}
			else
				FD_CLR(i, rd);
		}
		return (set);
	}
	return (select(max_fd + 1, rd, wd, NULL, newtimeout));
}

/* new_close: works just like close */
void
new_close(int des)
{
	if (des < 0 || des >= IO_ARRAYLEN)
		return;
	new_free(&(io_rec[des])); /* gkm */
	close(des);
}

/* sets socket options */
void
set_socket_options(int s)
{
	struct linger	lin;
	int	opt = 1;
	int	optlen = sizeof(opt);

	(void) setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *) &opt, optlen);
	opt = 1;
	(void) setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, (char *) &opt, optlen);
	lin.l_onoff = lin.l_linger = 0;
	(void) setsockopt(s, SOL_SOCKET, SO_LINGER, (char *) &lin, optlen);
}

int
dgets_errno(void)
{
	return dgets_local_errno;
}

void
dgets_set_errno(int num)
{
	dgets_local_errno = num;
}
