dnl     $Id: acinclude.m4,v 1.2 2001/12/21 03:12:50 robs Exp $

AC_DEFUN([FCGI_COMMON_CHECKS], [
    AC_CHECK_TYPE([ssize_t], [int]) 

    AC_MSG_CHECKING([for sun_len in sys/un.h])
    AC_EGREP_HEADER([sun_len], [sys/un.h],
	[AC_MSG_RESULT([yes])
	 AC_DEFINE([HAVE_SOCKADDR_UN_SUN_LEN], [1],
	   [Define if sockaddr_un in sys/un.h contains a sun_len component])],
	AC_MSG_RESULT([no]))

    AC_MSG_CHECKING([for fpos_t in stdio.h])
    AC_EGREP_HEADER([fpos_t], [stdio.h],
	[AC_MSG_RESULT([yes])
	 AC_DEFINE([HAVE_FPOS], [1], 
	    [Define if the fpos_t typedef is in stdio.h])],
	AC_MSG_RESULT([no]))

    AC_CHECK_HEADERS([sys/socket.h netdb.h netinet/in.h arpa/inet.h])
    AC_CHECK_HEADERS([sys/time.h limits.h sys/param.h unistd.h])

    AC_MSG_CHECKING([for a fileno() prototype in stdio.h])
    AC_EGREP_HEADER([fileno], [stdio.h], 
	    [AC_MSG_RESULT([yes]) 
	     AC_DEFINE([HAVE_FILENO_PROTO], [1], 
		   [Define if there's a fileno() prototype in stdio.h])],
	    AC_MSG_RESULT([no]))

    if test "$HAVE_SYS_SOCKET_H"; then
	AC_MSG_CHECKING([for socklen_t in sys/socket.h])
	AC_EGREP_HEADER([socklen_t], [sys/socket.h],
	    [AC_MSG_RESULT([yes])
	     AC_DEFINE([HAVE_SOCKLEN], [1],
			       [Define if the socklen_t typedef is in sys/socket.h])],
	   AC_MSG_RESULT([no]))
    fi

    #--------------------------------------------------------------------
    #  Do we need cross-process locking on this platform?
    #--------------------------------------------------------------------
    AC_MSG_CHECKING([whether cross-process locking is required by accept()])
    case "`uname -sr`" in
	IRIX\ 5.* | SunOS\ 5.* | UNIX_System_V\ 4.0)	
		    AC_MSG_RESULT([yes])
		    AC_DEFINE([USE_LOCKING], [1], 
		      [Define if cross-process locking is required by accept()])
	    ;;
	*)
		    AC_MSG_RESULT([no])
		;;
    esac

    #--------------------------------------------------------------------
    #  Does va_arg(arg, long double) crash the compiler?
    #  hpux 9.04 compiler does and so does Stratus FTX (uses HP's compiler)
    #--------------------------------------------------------------------
    AC_MSG_CHECKING([whether va_arg(arg, long double) crashes the compiler])
    AC_TRY_COMPILE([#include <stdarg.h>],
       [long double lDblArg; va_list arg; lDblArg = va_arg(arg, long double);],
       AC_MSG_RESULT([no]),
       [AC_MSG_RESULT([yes])
	AC_DEFINE([HAVE_VA_ARG_LONG_DOUBLE_BUG], [1],
	      [Define if va_arg(arg, long double) crashes the compiler])])

    AC_C_CONST 
])
