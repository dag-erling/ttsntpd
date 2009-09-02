AC_PREREQ(2.59)
AC_REVISION([$Id: configure.ac 138397 2008-02-29 11:55:45Z des $])
AC_INIT([rtcd], [1.0], [des@des.no])
AC_COPYRIGHT([2007 TANDBERG Telecom AS])
AC_CONFIG_SRCDIR(rtcd.c)
AM_CONFIG_HEADER(config.h)

AC_LANG(C)

AM_INIT_AUTOMAKE

AC_PROG_CC
AC_PROG_CC_C99
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_MAKE_SET

if test "$ac_cv_prog_cc_c99" = no; then
	AC_MSG_ERROR([A C99 compiler is required])
fi

AC_CHECK_LIB(socket, socket)
AC_CHECK_LIB(nsl, getaddrinfo)
AC_CHECK_LIB(rt, clock_gettime)

AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h])
AC_CHECK_HEADERS([termios.h])
AC_CHECK_HEADERS([unistd.h])

# for getopt() and certain other POSIX APIs
AC_DEFINE([_XOPEN_SOURCE], [600], [Include POSIX and XPG APIs])

# for adjtime()
AC_DEFINE([_BSD_SOURCE], [1], [Include BSD APIs])
AC_CHECK_FUNCS([adjtime adjtimex])

X_CFLAGS="-Wall -Wextra -Werror"
AC_ARG_ENABLE(debugging-symbols,
	AS_HELP_STRING([--enable-debugging-symbols],[enable debugging symbols (default is NO)]),
	X_CFLAGS="${X_CFLAGS} -O0 -g -fno-inline")
CFLAGS="${X_CFLAGS} ${CFLAGS}"

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
