dnl $eterna: aclocal.m4,v 2.5 2003/07/12 19:25:38 mrg Exp $
define([AC_FIND_PROGRAM],dnl
[if test x$3 = x; then _PATH=$PATH; else _PATH=$3; fi
if test -z "[$]$1"; then
  # Extract the first word of `$2', so it can be a program name with args.
  set dummy $2; word=[$]2
  AC_MSG_CHECKING(for $word)
  IFS="${IFS= 	}"; saveifs="$IFS"; IFS="${IFS}:"
  for dir in $_PATH; do
    test -z "$dir" && dir=.
    if test -f $dir/$word; then
      $1=$dir/$word
      break
    fi
  done
  IFS="$saveifs"
fi
AC_MSG_RESULT([$]$1)
AC_SUBST($1)dnl
])dnl

dnl
dnl AC_MSG_TRY_COMPILE
dnl
dnl Written by Luke Mewburn <lukem@netbsd.org>
dnl
dnl Usage:
dnl	AC_MSG_TRY_COMPILE(Message, CacheVar, Includes, Code,
dnl			    ActionPass [,ActionFail] )
dnl
dnl effectively does:
dnl	AC_CACHE_CHECK(Message, CacheVar,
dnl		AC_TRY_COMPILE(Includes, Code, CacheVar = yes, CacheVar = no)
dnl		if CacheVar == yes
dnl			AC_MESSAGE_RESULT(yes)
dnl			ActionPass
dnl		else
dnl			AC_MESSAGE_RESULT(no)
dnl			ActionFail
dnl	)
dnl
AC_DEFUN(AC_MSG_TRY_COMPILE, [
	AC_CACHE_CHECK($1, $2, [
		AC_TRY_COMPILE([ $3 ], [ $4 ], [ $2=yes ], [ $2=no ])
	])
	if test "x[$]$2" = "xyes"; then
		$5
	else
		$6
		:
	fi
])

dnl uncached version of the above (by mrg)
dnl	AC_MSG_TRY_COMPILE(Message, Includes, Code,
dnl			    ActionPass [,ActionFail] )
AC_DEFUN(AC_MSG_TRY_COMPILE_NOCACHE, [
	AC_MSG_CHECKING([$1])
	AC_TRY_COMPILE([ $2 ], [ $3 ], [ $4 ], [ :; $5 ])
])
