/*
 * irc_std.h: header to define things used in all the programs ircii
 * comes with
 *
 * hacked together from various other files by matthew green
 *
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
 * @(#)$eterna: irc_std.h,v 1.61 2015/12/12 20:44:36 mrg Exp $
 */

#ifndef irc__irc_std_h
#define irc__irc_std_h

/*
 * these things help with converting to/from char* and u_char*
 */
#define UP(s)			((u_char *)(s))
#define UPP(s)			((u_char **)(s))
#define CP(s)			((char *)(s))
#define CPP(s)			((char **)(s))
#define my_strlen(s)		strlen(CP(s))
#define my_strcmp(d,s)		strcmp(CP(d), CP(s))
#define my_strncmp(d,s,n)	strncmp(CP(d), CP(s), (n))
#define my_strcat(d,s)		strcat(CP(d), CP(s))
#define my_strncat(d,s,n)	strncat(CP(d), CP(s), (n))
#define my_strmcat(d,s,n)	strmcat(UP(d), UP(s), (n))
#define my_strcpy(d,s)		strcpy(CP(d), CP(s))
#define my_strncpy(d,s,n)	strncpy(CP(d), CP(s), (n))
#define my_strmcpy(d,s,n)	strmcpy(UP(d), UP(s), (n))
#define my_index(s,c)		UP(strchr(CP(s), (c)))
#define my_rindex(s,c)		UP(strrchr(CP(s), (c)))
#define my_atoi(s)		atoi(CP(s))
#define my_atol(s)		atol(CP(s))
#define my_getenv(s)		UP(getenv(CP(s)))

#define ARRAY_SIZE(a)		(sizeof(a) / sizeof(a[0]))

#if !defined(__GNUC__) || __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 5)
#define __attribute__(x)        /* delete __attribute__ if non-gcc or gcc1 */
#endif

#ifndef lint
#define IRCII_RCSID_NAMED(x,n) static const char n[] __attribute__((__used__)) = x
#else
#define IRCII_RCSID_NAMED(x,n)
#endif
#define IRCII_RCSID(x) IRCII_RCSID_NAMED(x,rcsid)

#include <errno.h>

typedef void sigfunc(int);

sigfunc *my_signal(int, sigfunc *, int);
#define MY_SIGNAL(s_n, s_h, m_f) my_signal(s_n, s_h, m_f)

#if defined(__svr4__) || defined(SVR4) || defined(__SVR4)
# if !defined(__svr4__)
#  define __svr4__
# endif /* __svr4__ */
# if !defined(SVR4)
#  define SVR4
# endif /* SVR4 */
# if !defined(__SVR4)
#  define __SVR4
# endif /* __SVR4 */
#endif /* __svr4__ || SVR4 || __SVR4 */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#if defined(HAVE_MEMORY_H)
# include <memory.h>
#endif /* HAVE_MEMORY_H */

#define IS_ABSOLUTE_PATH(file) ((file)[0] == '/')

#if !defined(HAVE_RFC2553_NETDB)

				/* RFC 2553 */
#undef	EAI_ADDRFAMILY
#define	EAI_ADDRFAMILY	 1	/* address family for hostname not supported */
#undef	EAI_AGAIN
#define	EAI_AGAIN	 2	/* temporary failure in name resolution */
#undef	EAI_BADFLAGS
#define	EAI_BADFLAGS	 3	/* invalid value for ai_flags */
#undef	EAI_FAIL
#define	EAI_FAIL	 4	/* non-recoverable failure in name resolution */
#undef	EAI_FAMILY
#define	EAI_FAMILY	 5	/* ai_family not supported */
#undef	EAI_MEMORY
#define	EAI_MEMORY	 6	/* memory allocation failure */
#undef	EAI_NODATA
#define	EAI_NODATA	 7	/* no address associated with hostname */
#undef	EAI_NONAME
#define	EAI_NONAME	 8	/* hostname nor servname provided, or not known */
#undef	EAI_SERVICE
#define	EAI_SERVICE	 9	/* servname not supported for ai_socktype */
#undef	EAI_SOCKTYPE
#define	EAI_SOCKTYPE	10	/* ai_socktype not supported */
#undef	EAI_SYSTEM
#define	EAI_SYSTEM	11	/* system error returned in errno */

				/* KAME extensions? */
#undef	EAI_BADHINTS
#define	EAI_BADHINTS	12
#undef	EAI_PROTOCOL
#define	EAI_PROTOCOL	13
#undef	EAI_MAX
#define	EAI_MAX		14

				/* RFC 2553 */
#undef	NI_MAXHOST
#define	NI_MAXHOST	1025
#undef	NI_MAXSERV
#define	NI_MAXSERV	32

#undef	NI_NOFQDN
#define	NI_NOFQDN	0x00000001
#undef	NI_NUMERICHOST
#define	NI_NUMERICHOST	0x00000002
#undef	NI_NAMEREQD
#define	NI_NAMEREQD	0x00000004
#undef	NI_NUMERICSERV
#define	NI_NUMERICSERV	0x00000008
#undef	NI_DGRAM
#define	NI_DGRAM	0x00000010

				/* RFC 2553 */
#undef	AI_PASSIVE
#define	AI_PASSIVE	0x00000001 /* get address to use bind() */
#undef	AI_CANONNAME
#define	AI_CANONNAME	0x00000002 /* fill ai_canonname */

				/* KAME extensions ? */
#undef	AI_NUMERICHOST
#define	AI_NUMERICHOST	0x00000004 /* prevent name resolution */
#undef	AI_MASK
#define	AI_MASK		(AI_PASSIVE | AI_CANONNAME | AI_NUMERICHOST)

				/* RFC 2553 */
#undef	AI_ALL
#define	AI_ALL		0x00000100 /* IPv6 and IPv4-mapped (with AI_V4MAPPED) */
#undef	AI_V4MAPPED_CFG
#define	AI_V4MAPPED_CFG	0x00000200 /* accept IPv4-mapped if kernel supports */
#undef	AI_ADDRCONFIG
#define	AI_ADDRCONFIG	0x00000400 /* only if any address is assigned */
#undef	AI_V4MAPPED
#define	AI_V4MAPPED	0x00000800 /* accept IPv4-mapped IPv6 address */

#endif /* ! HAVE_RFC2553_NETDB */

#if !defined(HAVE_SOCKLEN_T)
typedef unsigned int socklen_t;
#endif /* HAVE_SOCKLEN_T */

#if !defined(HAVE_RFC2553_NETDB) && !defined(HAVE_ADDRINFO)

struct addrinfo {
	int	ai_flags;	/* AI_PASSIVE, AI_CANONNAME, AI_NUMERICHOST */
	int	ai_family;	/* PF_xxx */
	int	ai_socktype;	/* SOCK_xxx */
	int	ai_protocol;	/* 0 or IPPROTO_xxx for IPv4 and IPv6 */
	size_t	ai_addrlen;	/* length of ai_addr */
	char	*ai_canonname;	/* canonical name for hostname */
	struct sockaddr *ai_addr;	/* binary address */
	struct addrinfo *ai_next;	/* next structure in linked list */
};

int     getaddrinfo(const char *, const char *,
		const struct addrinfo *, struct addrinfo **);
int     getnameinfo(const struct sockaddr *, socklen_t, char *,
		size_t, char *, size_t, int);
void    freeaddrinfo(struct addrinfo *);
char   *gai_strerror(int);

#endif /* ! HAVE_RFC2553_NETDB && ! HAVE_ADDRINFO */

#ifndef HAVE_SNPRINTF
int snprintf(char *str, size_t count, const char *fmt, ...);
#endif /* HAVE_SNPRINTF */

#ifndef HAVE_VSNPRINTF
int vsnprintf(char *str, size_t count, const char *fmt, va_list args);
#endif /* HAVE_VSNPRINTF */

#ifdef INET6
# define SOCKADDR_STORAGE struct sockaddr_storage
# define SS_FAMILY(ss) (ss)->ss_family
#else
# define SOCKADDR_STORAGE struct sockaddr_in
# define SS_FAMILY(ss) (ss)->sin_family
#endif

/* AIX is Special.  This turns on Berkeley sockets which makes wserv work. */
#ifdef _AIX
# define COMPAT_43
#endif

#ifndef HAVE_SSIZE_T
typedef int ssize_t;
#endif

#endif /* irc__irc_std_h */
