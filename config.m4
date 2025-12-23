dnl config.m4 for extension terminal

PHP_ARG_ENABLE([terminal],
  [whether to enable terminal support],
  [AS_HELP_STRING([--enable-terminal], [Enable terminal support])],
  [no])

if test "$PHP_TERMINAL" != "no"; then
  dnl Check for required headers
  AC_CHECK_HEADERS([termios.h sys/ioctl.h sys/select.h signal.h unistd.h], [],
    [AC_MSG_ERROR([Required header file missing])])

  dnl Check for required functions
  AC_CHECK_FUNCS([tcgetattr tcsetattr ioctl select], [],
    [AC_MSG_ERROR([Required function missing])])

  dnl Check for clock_gettime (might need -lrt on older systems)
  AC_SEARCH_LIBS([clock_gettime], [rt], [],
    [AC_MSG_ERROR([clock_gettime not found])])

  dnl Check for POSIX signals
  AC_CHECK_FUNCS([sigaction sigemptyset sigaddset])

  dnl Compiler flags
  CFLAGS="$CFLAGS -Wall -Wextra -Wno-unused-parameter"

  dnl Define extension
  PHP_NEW_EXTENSION(terminal, terminal.c, $ext_shared)

  dnl Add include path
  PHP_ADD_BUILD_DIR($ext_builddir)
fi
