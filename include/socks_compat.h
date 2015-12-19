/*
 * Copyright (c) 2014 Matthew R. Green
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * @(#)$eterna: socks_compat.h,v 1.1 2014/03/12 07:34:51 mrg Exp $
 */

/* header file for socks4/socks5 compat */

#ifdef SOCKS4
#define connect		Rconnect
#define getsockname	Rgetsockname
#define bind		Rbind
#define accept		Raccept
#define listen		Rlisten
#define select		Rselect
#else

# ifdef SOCKS5
#define connect		SOCKSconnect
#define getsockname	SOCKSgetsockname
#define getpeername	SOCKSgetpeername
#define bind		SOCKSbind
#define accept		SOCKSaccept
#define listen		SOCKSlisten
#define select		SOCKSselect
#define recvfrom	SOCKSrecvfrom
#define sendto		SOCKSsendto
#define recv		SOCKSrecv
#define send		SOCKSsend
#define read		SOCKSread
#define write		SOCKSwrite
#define rresvport	SOCKSrresvport
#define shutdown	SOCKSshutdown
#define close		SOCKSclose
#define dup		SOCKSdup
#define dup2		SOCKSdup2
#define fclose		SOCKSfclose
#define gethostbyname	SOCKSgethostbyname

# endif /* SOCKS5 */
#endif /* SOCKS4 */
