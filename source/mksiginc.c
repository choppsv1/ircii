/* This file is generated from: eterna: mkmksiginc.pl,v 1.13 2014/02/28 01:34:05 mrg Exp  */

/*
 * Copyright (c) 2003-2014 Matthew R. Green
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
 */

#include "irc.h"
IRCII_RCSID("@(#)$eterna: mksiginc.c,v 1.16 2014/03/14 01:57:16 mrg Exp $");

#ifndef NSIG
#define NSIG 64
#endif

#define MY_MAXSIG NSIG+1
char *signames[MY_MAXSIG];

int main(int, char *[]);
int
main(int argc, char *argv[])
{
	int i;

	signames[0] = "ZERO";
	for (i = 1; i < MY_MAXSIG; i++)
		signames[i] = 0;

#if defined(SIGABRT)
	if (SIGABRT < MY_MAXSIG)
		signames[SIGABRT] = "ABRT";
#endif

#if defined(SIGALRM)
	if (SIGALRM < MY_MAXSIG)
		signames[SIGALRM] = "ALRM";
#endif

#if defined(SIGALRM1)
	if (SIGALRM1 < MY_MAXSIG)
		signames[SIGALRM1] = "ALRM1";
#endif

#if defined(SIGBUS)
	if (SIGBUS < MY_MAXSIG)
		signames[SIGBUS] = "BUS";
#endif

#if defined(SIGCANCEL)
	if (SIGCANCEL < MY_MAXSIG)
		signames[SIGCANCEL] = "CANCEL";
#endif

#if defined(SIGCHLD)
	if (SIGCHLD < MY_MAXSIG)
		signames[SIGCHLD] = "CHLD";
#endif

#if defined(SIGCLD)
	if (SIGCLD < MY_MAXSIG)
		signames[SIGCLD] = "CLD";
#endif

#if defined(SIGCONT)
	if (SIGCONT < MY_MAXSIG)
		signames[SIGCONT] = "CONT";
#endif

#if defined(SIGDANGER)
	if (SIGDANGER < MY_MAXSIG)
		signames[SIGDANGER] = "DANGER";
#endif

#if defined(SIGDIL)
	if (SIGDIL < MY_MAXSIG)
		signames[SIGDIL] = "DIL";
#endif

#if defined(SIGEMT)
	if (SIGEMT < MY_MAXSIG)
		signames[SIGEMT] = "EMT";
#endif

#if defined(SIGFPE)
	if (SIGFPE < MY_MAXSIG)
		signames[SIGFPE] = "FPE";
#endif

#if defined(SIGFREEZE)
	if (SIGFREEZE < MY_MAXSIG)
		signames[SIGFREEZE] = "FREEZE";
#endif

#if defined(SIGGRANT)
	if (SIGGRANT < MY_MAXSIG)
		signames[SIGGRANT] = "GRANT";
#endif

#if defined(SIGHUP)
	if (SIGHUP < MY_MAXSIG)
		signames[SIGHUP] = "HUP";
#endif

#if defined(SIGILL)
	if (SIGILL < MY_MAXSIG)
		signames[SIGILL] = "ILL";
#endif

#if defined(SIGINFO)
	if (SIGINFO < MY_MAXSIG)
		signames[SIGINFO] = "INFO";
#endif

#if defined(SIGINT)
	if (SIGINT < MY_MAXSIG)
		signames[SIGINT] = "INT";
#endif

#if defined(SIGIO)
	if (SIGIO < MY_MAXSIG)
		signames[SIGIO] = "IO";
#endif

#if defined(SIGIOT)
	if (SIGIOT < MY_MAXSIG)
		signames[SIGIOT] = "IOT";
#endif

#if defined(SIGKAP)
	if (SIGKAP < MY_MAXSIG)
		signames[SIGKAP] = "KAP";
#endif

#if defined(SIGKILL)
	if (SIGKILL < MY_MAXSIG)
		signames[SIGKILL] = "KILL";
#endif

#if defined(SIGKILLTHR)
	if (SIGKILLTHR < MY_MAXSIG)
		signames[SIGKILLTHR] = "KILLTHR";
#endif

#if defined(SIGLOST)
	if (SIGLOST < MY_MAXSIG)
		signames[SIGLOST] = "LOST";
#endif

#if defined(SIGLWP)
	if (SIGLWP < MY_MAXSIG)
		signames[SIGLWP] = "LWP";
#endif

#if defined(SIGMIGRATE)
	if (SIGMIGRATE < MY_MAXSIG)
		signames[SIGMIGRATE] = "MIGRATE";
#endif

#if defined(SIGMSG)
	if (SIGMSG < MY_MAXSIG)
		signames[SIGMSG] = "MSG";
#endif

#if defined(SIGPIPE)
	if (SIGPIPE < MY_MAXSIG)
		signames[SIGPIPE] = "PIPE";
#endif

#if defined(SIGPOLL)
	if (SIGPOLL < MY_MAXSIG)
		signames[SIGPOLL] = "POLL";
#endif

#if defined(SIGPRE)
	if (SIGPRE < MY_MAXSIG)
		signames[SIGPRE] = "PRE";
#endif

#if defined(SIGPROF)
	if (SIGPROF < MY_MAXSIG)
		signames[SIGPROF] = "PROF";
#endif

#if defined(SIGPWR)
	if (SIGPWR < MY_MAXSIG)
		signames[SIGPWR] = "PWR";
#endif

#if defined(SIGQUIT)
	if (SIGQUIT < MY_MAXSIG)
		signames[SIGQUIT] = "QUIT";
#endif

#if defined(SIGRETRACT)
	if (SIGRETRACT < MY_MAXSIG)
		signames[SIGRETRACT] = "RETRACT";
#endif

#if defined(SIGRTMAX)
	if (SIGRTMAX < MY_MAXSIG)
		signames[SIGRTMAX] = "RTMAX";
#endif

#if defined(SIGRTMIN)
	if (SIGRTMIN < MY_MAXSIG)
		signames[SIGRTMIN] = "RTMIN";
#endif

#if defined(SIGSAK)
	if (SIGSAK < MY_MAXSIG)
		signames[SIGSAK] = "SAK";
#endif

#if defined(SIGSEGV)
	if (SIGSEGV < MY_MAXSIG)
		signames[SIGSEGV] = "SEGV";
#endif

#if defined(SIGSOUND)
	if (SIGSOUND < MY_MAXSIG)
		signames[SIGSOUND] = "SOUND";
#endif

#if defined(SIGSTOP)
	if (SIGSTOP < MY_MAXSIG)
		signames[SIGSTOP] = "STOP";
#endif

#if defined(SIGSYS)
	if (SIGSYS < MY_MAXSIG)
		signames[SIGSYS] = "SYS";
#endif

#if defined(SIGTERM)
	if (SIGTERM < MY_MAXSIG)
		signames[SIGTERM] = "TERM";
#endif

#if defined(SIGTHAW)
	if (SIGTHAW < MY_MAXSIG)
		signames[SIGTHAW] = "THAW";
#endif

#if defined(SIGTRAP)
	if (SIGTRAP < MY_MAXSIG)
		signames[SIGTRAP] = "TRAP";
#endif

#if defined(SIGTSTP)
	if (SIGTSTP < MY_MAXSIG)
		signames[SIGTSTP] = "TSTP";
#endif

#if defined(SIGTTIN)
	if (SIGTTIN < MY_MAXSIG)
		signames[SIGTTIN] = "TTIN";
#endif

#if defined(SIGTTOU)
	if (SIGTTOU < MY_MAXSIG)
		signames[SIGTTOU] = "TTOU";
#endif

#if defined(SIGURG)
	if (SIGURG < MY_MAXSIG)
		signames[SIGURG] = "URG";
#endif

#if defined(SIGUSR1)
	if (SIGUSR1 < MY_MAXSIG)
		signames[SIGUSR1] = "USR1";
#endif

#if defined(SIGUSR2)
	if (SIGUSR2 < MY_MAXSIG)
		signames[SIGUSR2] = "USR2";
#endif

#if defined(SIGVIRT)
	if (SIGVIRT < MY_MAXSIG)
		signames[SIGVIRT] = "VIRT";
#endif

#if defined(SIGVTALRM)
	if (SIGVTALRM < MY_MAXSIG)
		signames[SIGVTALRM] = "VTALRM";
#endif

#if defined(SIGWAITING)
	if (SIGWAITING < MY_MAXSIG)
		signames[SIGWAITING] = "WAITING";
#endif

#if defined(SIGWINCH)
	if (SIGWINCH < MY_MAXSIG)
		signames[SIGWINCH] = "WINCH";
#endif

#if defined(SIGWINDOW)
	if (SIGWINDOW < MY_MAXSIG)
		signames[SIGWINDOW] = "WINDOW";
#endif

#if defined(SIGXCPU)
	if (SIGXCPU < MY_MAXSIG)
		signames[SIGXCPU] = "XCPU";
#endif

#if defined(SIGXFSZ)
	if (SIGXFSZ < MY_MAXSIG)
		signames[SIGXFSZ] = "XFSZ";
#endif

	printf("static const int max_signo = %d;\n", NSIG);
	puts("static const char * const signals[] = { ");
	for (i = 0; i < MY_MAXSIG; i++)
		if (signames[i])
			printf("\"%s\", ", signames[i]);
		else
			printf("\"SIG%d\", ", i);
	puts("NULL };\n");

	exit(0);
}
