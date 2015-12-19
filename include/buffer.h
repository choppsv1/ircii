/*
 * buffer.h: header buffer routines
 *
 * Written by Matthew Green
 *
 * Copyright (c) 2001-2014 Matthew R. Green.
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
 * @(#)$eterna: buffer.h,v 1.7 2014/02/05 07:20:11 mrg Exp $
 */

#ifndef irc__buffer_h_
#define irc__buffer_h_

/* This file should only be included once by files that need buffer ops */

#if defined(HAVE_VASPRINTF)

/* put_it() and friends need to be reentrant */
#define PUTBUF_INIT	u_char *putbuf;

# define PUTBUF_SPRINTF(f, v) 				\
if (vasprintf((char **)(void *)&putbuf, f, v) == -1)	\
{	/* EEK */					\
	(void)write(1, "out of memory?\n\r\n\r", 19);	\
	_exit(1);					\
}

# define PUTBUF_END	free(putbuf); putbuf = 0;

#else

/*
 * need the caller to define the `putbuf'. something like this, though
 * it might need to be automatic for recursive purposes.
 *	static	u_char	putbuf[4*BIG_BUFFER_SIZE + 1] = "";
 */
# define NEED_PUTBUF_DECLARED

# define PUTBUF_INIT

# define PUTBUF_SPRINTF(f, v) vsnprintf(CP(putbuf), sizeof putbuf, f, v);

# define PUTBUF_END

#endif /* HAVE_VASPRINTF */

#endif /* irc__buffer_h_ */
