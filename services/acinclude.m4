dnl $Id$

dnl
dnl check for sys_errlist -- NS_CHECK_SYS_ERRLIST
dnl
define(NS_CHECK_SYS_ERRLIST,[
AC_MSG_CHECKING(for definition of sys_errlist)
AC_TRY_COMPILE([sys_errlist declaration],
[#include <stdio.h>
#include <errno.h>], [sys_nerr;], AC_MSG_RESULT(yes),
AC_MSG_RESULT(no) AC_DEFINE(NEED_SYS_ERRLIST))])dnl

dnl
dnl check if union wait is defined, or if WAIT_USES_INT -- CHECK_WAIT_TYPE
dnl
define(CHECK_WAIT_TYPE,[
AC_MSG_CHECKING(for union wait)
AC_TRY_COMPILE([union wait],
[#include <sys/wait.h>], [union wait i;], AC_MSG_RESULT(union wait),
AC_MSG_RESULT(int) AC_DEFINE(WAIT_USES_INT))])dnl

dnl
dnl check for yylineno -- NS_CHECK_YYLINENO
dnl
define(NS_CHECK_YYLINENO,[dnl
AC_REQUIRE_CPP()AC_REQUIRE([AC_PROG_LEX])dnl
AC_MSG_CHECKING(for yylineno declaration)
# some systems have yylineno, others don't...
  echo '%% \
%%' | ${LEX} -t > conftest.out
  if egrep yylineno conftest.out >/dev/null 2>&1; then
	AC_MSG_RESULT(yes)
        AC_DEFINE([HAVE_YYLINENO])
  else
	AC_MSG_RESULT(no)
	if test "${LEX}" = "flex" ; then
		AC_MSG_CHECKING(if flex -l defines yylineno)
		# perhaps this will give us line numbers
echo '%% \
%%' | ${LEX} -t -l > conftest.out
		if egrep yylineno conftest.out >/dev/null 2>&1; then
			AC_MSG_RESULT(yes)		
	        	AC_DEFINE([HAVE_YYLINENO])
			LEX="${LEX} -l"
		else
			AC_MSG_RESULT(no)
		fi
	fi
  fi
  rm -f conftest.out
])dnl
