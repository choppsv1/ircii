#! /usr/bin/env perl

# this does not support RT signals yet - big deal

$rcsid = '$eterna: mkmksiginc.pl,v 1.13 2014/02/28 01:34:05 mrg Exp $';
($my_rcsid = $rcsid) =~ s/^\$(.*)\$$/$1/;
$from_rcsid = "/* This file is generated from: $my_rcsid */\n\n";

$part_one = <<'__eop1__';
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
IRCII_RCSID("@(#)$eterna: mkmksiginc.pl,v 1.13 2014/02/28 01:34:05 mrg Exp $");

#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

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

__eop1__

sub main {
	print $from_rcsid;
	print $part_one;
	while ($_ = <DATA>) {
		chomp;
		print "#if defined(SIG$_)\n";
		print "\tif (SIG$_ < MY_MAXSIG)\n";
		print "\t\tsignames[SIG$_] = \"$_\";\n";
		print "#endif\n\n";
	}
	print $part_two;
}

$part_two = <<'__eop2__';
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
__eop2__

&main();

__DATA__
ABRT
ALRM
ALRM1
BUS
CANCEL
CHLD
CLD
CONT
DANGER
DIL
EMT
FPE
FREEZE
GRANT
HUP
ILL
INFO
INT
IO
IOT
KAP
KILL
KILLTHR
LOST
LWP
MIGRATE
MSG
PIPE
POLL
PRE
PROF
PWR
QUIT
RETRACT
RTMAX
RTMIN
SAK
SEGV
SOUND
STOP
SYS
TERM
THAW
TRAP
TSTP
TTIN
TTOU
URG
USR1
USR2
VIRT
VTALRM
WAITING
WINCH
WINDOW
XCPU
XFSZ
