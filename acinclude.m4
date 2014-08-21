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


dnl @synopsis ACX_PTHREAD([ACTION-IF-FOUND[, ACTION-IF-NOT-FOUND]])
dnl
dnl This macro figures out how to build C programs using POSIX
dnl threads.  It sets the PTHREAD_LIBS output variable to the threads
dnl library and linker flags, and the PTHREAD_CFLAGS output variable
dnl to any special C compiler flags that are needed.  (The user can also
dnl force certain compiler flags/libs to be tested by setting these
dnl environment variables.)
dnl
dnl Also sets PTHREAD_CC to any special C compiler that is needed for
dnl multi-threaded programs (defaults to the value of CC otherwise).
dnl (This is necessary on AIX to use the special cc_r compiler alias.)
dnl
dnl If you are only building threads programs, you may wish to
dnl use these variables in your default LIBS, CFLAGS, and CC:
dnl
dnl        LIBS="$PTHREAD_LIBS $LIBS"
dnl        CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
dnl        CC="$PTHREAD_CC"
dnl
dnl In addition, if the PTHREAD_CREATE_JOINABLE thread-attribute
dnl constant has a nonstandard name, defines PTHREAD_CREATE_JOINABLE
dnl to that name (e.g. PTHREAD_CREATE_UNDETACHED on AIX).
dnl
dnl ACTION-IF-FOUND is a list of shell commands to run if a threads
dnl library is found, and ACTION-IF-NOT-FOUND is a list of commands
dnl to run it if it is not found.  If ACTION-IF-FOUND is not specified,
dnl the default action will define HAVE_PTHREAD.
dnl
dnl Please let the authors know if this macro fails on any platform,
dnl or if you have any other suggestions or comments.  This macro was
dnl based on work by SGJ on autoconf scripts for FFTW (www.fftw.org)
dnl (with help from M. Frigo), as well as ac_pthread and hb_pthread
dnl macros posted by AFC to the autoconf macro repository.  We are also
dnl grateful for the helpful feedback of numerous users.
dnl
dnl @version $Id: acinclude.m4,v 1.2 2001/12/21 03:12:50 robs Exp $
dnl @author Steven G. Johnson <stevenj@alum.mit.edu> and Alejandro Forero Cuervo <bachue@bachue.com>

AC_DEFUN([ACX_PTHREAD], [
AC_REQUIRE([AC_CANONICAL_HOST])
acx_pthread_ok=no

# First, check if the POSIX threads header, pthread.h, is available.
# If it isn't, don't bother looking for the threads libraries.
AC_CHECK_HEADER(pthread.h, , acx_pthread_ok=noheader)

# We must check for the threads library under a number of different
# names; the ordering is very important because some systems
# (e.g. DEC) have both -lpthread and -lpthreads, where one of the
# libraries is broken (non-POSIX).

# First of all, check if the user has set any of the PTHREAD_LIBS,
# etcetera environment variables, and if threads linking works using
# them:
if test x"$PTHREAD_LIBS$PTHREAD_CFLAGS" != x; then
        save_CFLAGS="$CFLAGS"
        CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
        save_LIBS="$LIBS"
        LIBS="$PTHREAD_LIBS $LIBS"
        AC_MSG_CHECKING([for pthread_join in LIBS=$PTHREAD_LIBS with CFLAGS=$PTHREAD_CFLAGS])
        AC_TRY_LINK_FUNC(pthread_join, acx_pthread_ok=yes)
        AC_MSG_RESULT($acx_pthread_ok)
        if test x"$acx_pthread_ok" = xno; then
                PTHREAD_LIBS=""
                PTHREAD_CFLAGS=""
        fi
        LIBS="$save_LIBS"
        CFLAGS="$save_CFLAGS"
fi

# Create a list of thread flags to try.  Items starting with a "-" are
# C compiler flags, and other items are library names, except for "none"
# which indicates that we try without any flags at all.

acx_pthread_flags="pthreads none -Kthread -kthread lthread -pthread -pthreads -mthreads pthread --thread-safe -mt"

# The ordering *is* (sometimes) important.  Some notes on the
# individual items follow:

# pthreads: AIX (must check this before -lpthread)
# none: in case threads are in libc; should be tried before -Kthread and
#       other compiler flags to prevent continual compiler warnings
# -Kthread: Sequent (threads in libc, but -Kthread needed for pthread.h)
# -kthread: FreeBSD kernel threads (preferred to -pthread since SMP-able)
# lthread: LinuxThreads port on FreeBSD (also preferred to -pthread)
# -pthread: Linux/gcc (kernel threads), BSD/gcc (userland threads)
# -pthreads: Solaris/gcc
# -mthreads: Mingw32/gcc, Lynx/gcc
# -mt: Sun Workshop C (may only link SunOS threads [-lthread], but it
#      doesn't hurt to check since this sometimes defines pthreads too;
#      also defines -D_REENTRANT)
# pthread: Linux, etcetera
# --thread-safe: KAI C++

case "${host_cpu}-${host_os}" in
        *solaris*)

        # On Solaris (at least, for some versions), libc contains stubbed
        # (non-functional) versions of the pthreads routines, so link-based
        # tests will erroneously succeed.  (We need to link with -pthread or
        # -lpthread.)  (The stubs are missing pthread_cleanup_push, or rather
        # a function called by this macro, so we could check for that, but
        # who knows whether they'll stub that too in a future libc.)  So,
        # we'll just look for -pthreads and -lpthread first:

        acx_pthread_flags="-pthread -pthreads pthread -mt $acx_pthread_flags"
        ;;
esac

if test x"$acx_pthread_ok" = xno; then
for flag in $acx_pthread_flags; do

        case $flag in
                none)
                AC_MSG_CHECKING([whether pthreads work without any flags])
                ;;

                -*)
                AC_MSG_CHECKING([whether pthreads work with $flag])
                PTHREAD_CFLAGS="$flag"
                ;;

                *)
                AC_MSG_CHECKING([for the pthreads library -l$flag])
                PTHREAD_LIBS="-l$flag"
                ;;
        esac

        save_LIBS="$LIBS"
        save_CFLAGS="$CFLAGS"
        LIBS="$PTHREAD_LIBS $LIBS"
        CFLAGS="$CFLAGS $PTHREAD_CFLAGS"

        # Check for various functions.  We must include pthread.h,
        # since some functions may be macros.  (On the Sequent, we
        # need a special flag -Kthread to make this header compile.)
        # We check for pthread_join because it is in -lpthread on IRIX
        # while pthread_create is in libc.  We check for pthread_attr_init
        # due to DEC craziness with -lpthreads.  We check for
        # pthread_cleanup_push because it is one of the few pthread
        # functions on Solaris that doesn't have a non-functional libc stub.
        # We try pthread_create on general principles.
        AC_TRY_LINK([#include <pthread.h>],
                    [pthread_t th; pthread_join(th, 0);
                     pthread_attr_init(0); pthread_cleanup_push(0, 0);
                     pthread_create(0,0,0,0); pthread_cleanup_pop(0); ],
                    [acx_pthread_ok=yes])

        LIBS="$save_LIBS"
        CFLAGS="$save_CFLAGS"

        AC_MSG_RESULT($acx_pthread_ok)
        if test "x$acx_pthread_ok" = xyes; then
                break;
        fi

        PTHREAD_LIBS=""
        PTHREAD_CFLAGS=""
done
fi

# Various other checks:
if test "x$acx_pthread_ok" = xyes; then
        save_LIBS="$LIBS"
        LIBS="$PTHREAD_LIBS $LIBS"
        save_CFLAGS="$CFLAGS"
        CFLAGS="$CFLAGS $PTHREAD_CFLAGS"

        # Detect AIX lossage: threads are created detached by default
        # and the JOINABLE attribute has a nonstandard name (UNDETACHED).
        AC_MSG_CHECKING([for joinable pthread attribute])
        AC_TRY_LINK([#include <pthread.h>],
                    [int attr=PTHREAD_CREATE_JOINABLE;],
                    ok=PTHREAD_CREATE_JOINABLE, ok=unknown)
        if test x"$ok" = xunknown; then
                AC_TRY_LINK([#include <pthread.h>],
                            [int attr=PTHREAD_CREATE_UNDETACHED;],
                            ok=PTHREAD_CREATE_UNDETACHED, ok=unknown)
        fi
        if test x"$ok" != xPTHREAD_CREATE_JOINABLE; then
                AC_DEFINE(PTHREAD_CREATE_JOINABLE, $ok,
                          [Define to the necessary symbol if this constant
                           uses a non-standard name on your system.])
        fi
        AC_MSG_RESULT(${ok})
        if test x"$ok" = xunknown; then
                AC_MSG_WARN([we do not know how to create joinable pthreads])
        fi

        AC_MSG_CHECKING([if more special flags are required for pthreads])
        flag=no
        case "${host_cpu}-${host_os}" in
                *-aix* | *-freebsd*)     flag="-D_THREAD_SAFE";;
                *solaris* | alpha*-osf*) flag="-D_REENTRANT";;
        esac
        AC_MSG_RESULT(${flag})
        if test "x$flag" != xno; then
                PTHREAD_CFLAGS="$flag $PTHREAD_CFLAGS"
        fi

        LIBS="$save_LIBS"
        CFLAGS="$save_CFLAGS"

        # More AIX lossage: must compile with cc_r
        AC_CHECK_PROG(PTHREAD_CC, cc_r, cc_r, ${CC})
else
        PTHREAD_CC="$CC"
fi

AC_SUBST(PTHREAD_LIBS)
AC_SUBST(PTHREAD_CFLAGS)
AC_SUBST(PTHREAD_CC)

# Finally, execute ACTION-IF-FOUND/ACTION-IF-NOT-FOUND:
if test x"$acx_pthread_ok" = xyes; then
        ifelse([$1],,AC_DEFINE(HAVE_PTHREAD,1,[Define if you have POSIX threads libraries and header files.]),[$1])
        :
else
        acx_pthread_ok=no
        $2
fi

])dnl ACX_PTHREAD



dnl @synopsis AC_PROG_CC_WARNINGS([ANSI])
dnl
dnl Enables a reasonable set of warnings for the C compiler.  Optionally,
dnl if the first argument is nonempty, turns on flags which enforce and/or
dnl enable proper ANSI C if such flags are known to the compiler used.
dnl
dnl Currently this macro knows about GCC, Solaris C compiler,
dnl Digital Unix C compiler, C for AIX Compiler, HP-UX C compiler,
dnl and IRIX C compiler.
dnl
dnl @version $Id: acinclude.m4,v 1.2 2001/12/21 03:12:50 robs Exp $
dnl @author Ville Laurikari <vl@iki.fi>
dnl
AC_DEFUN([AC_PROG_CC_WARNINGS], [
  ansi=$1
  if test -z "$ansi"; then
    msg="for C compiler warning flags"
  else
    msg="for C compiler warning and ANSI conformance flags"
  fi
  AC_CACHE_CHECK($msg, ac_cv_prog_cc_warnings, [
    if test -n "$CC"; then
      cat > conftest.c <<EOF
int main(int argc, char **argv) { return 0; }
EOF

      dnl GCC
      if test "$GCC" = "yes"; then
        if test -z "$ansi"; then
          ac_cv_prog_cc_warnings="-Wall"
        else
          ac_cv_prog_cc_warnings="-Wall -ansi -pedantic"
        fi

      dnl Solaris C compiler
      elif $CC -flags 2>&1 | grep "Xc.*strict ANSI C" > /dev/null 2>&1 &&
           $CC -c -v -Xc conftest.c > /dev/null 2>&1 &&
           test -f conftest.o; then
        if test -z "$ansi"; then
          ac_cv_prog_cc_warnings="-v"
        else
          ac_cv_prog_cc_warnings="-v -Xc"
        fi

      dnl HP-UX C compiler
      elif $CC > /dev/null 2>&1 &&
           $CC -c -Aa +w1 conftest.c > /dev/null 2>&1 &&
           test -f conftest.o; then
        if test -z "$ansi"; then
          ac_cv_prog_cc_warnings="+w1"
        else
          ac_cv_prog_cc_warnings="+w1 -Aa"
        fi

      dnl Digital Unix C compiler
      elif ! $CC > /dev/null 2>&1 &&
           $CC -c -verbose -w0 -warnprotos -std1 conftest.c > /dev/null 2>&1 &&
           test -f conftest.o; then
        if test -z "$ansi"; then
          ac_cv_prog_cc_warnings="-verbose -w0 -warnprotos"
        else
          ac_cv_prog_cc_warnings="-verbose -w0 -warnprotos -std1"
        fi

      dnl C for AIX Compiler
      elif $CC > /dev/null 2>&1 | grep AIX > /dev/null 2>&1 &&
           $CC -c -qlanglvl=ansi -qinfo=all conftest.c > /dev/null 2>&1 &&
           test -f conftest.o; then
        if test -z "$ansi"; then
          ac_cv_prog_cc_warnings="-qsrcmsg -qinfo=all:noppt:noppc:noobs:nocnd"
        else
          ac_cv_prog_cc_warnings="-qsrcmsg -qinfo=all:noppt:noppc:noobs:nocnd -qlanglvl=ansi"
        fi

      dnl IRIX C compiler
      elif $CC -fullwarn -ansi -ansiE > /dev/null 2>&1 &&
           test -f conftest.o; then
        if test -z "$ansi"; then
          ac_cv_prog_cc_warnings="-fullwarn"
        else
          ac_cv_prog_cc_warnings="-fullwarn -ansi -ansiE"
        fi

      fi
      rm -f conftest.*
    fi
    if test -n "$ac_cv_prog_cc_warnings"; then
      CFLAGS="$CFLAGS $ac_cv_prog_cc_warnings"
    else
      ac_cv_prog_cc_warnings="unknown"
    fi
  ])
])




dnl From vips acinclude.m4
dnl
dnl FIND_TIFF[ACTION-IF-FOUND[, ACTION-IF-NOT-FOUND]]
dnl ------------------------------------------------
dnl
dnl Find TIFF libraries and headers
dnl
dnl Put compile stuff in TIFF_INCLUDES
dnl Put link stuff in TIFF_LIBS
dnl
dnl Default ACTION-IF-FOUND defines HAVE_TIFF
dnl
AC_DEFUN([FIND_TIFF], [

TIFF_INCLUDES=""
TIFF_LIBS=""

dnl AC_ARG_WITH(tiff, 
dnl [  --without-tiff		do not use libtiff])

# Treat --without-tiff like --without-tiff-includes --without-tiff-libraries.
if test "$with_tiff" = "no"; then
	TIFF_INCLUDES=no
	TIFF_LIBS=no
fi

AC_ARG_WITH(tiff-includes,
[  --with-tiff-includes=DIR	TIFF include files are in DIR],
TIFF_INCLUDES="-I$withval")
AC_ARG_WITH(tiff-libraries,
[  --with-tiff-libraries=DIR	TIFF libraries are in DIR],
TIFF_LIBS="-L$withval -ltiff")

AC_MSG_CHECKING(for TIFF)

# Look for tiff.h 
if test "$TIFF_INCLUDES" = ""; then
	# Check the standard search path
	AC_TRY_COMPILE([#include <tiff.h>],[int a;],[
		TIFF_INCLUDES=""
	], [
		# tiff.h is not in the standard search path.

		# A whole bunch of guesses
		for dir in \
			"${prefix}"/*/include /usr/*/include \
			/usr/local/*/include "${prefix}"/include/* \
			/usr/include/* /usr/local/include/* /*/include; do
			if test -f "$dir/tiff.h"; then
				TIFF_INCLUDES="-I$dir"
				break
			fi
		done

		if test "$TIFF_INCLUDES" = ""; then
			TIFF_INCLUDES=no
		fi
	])
fi

# Now for the libraries
if test "$TIFF_LIBS" = ""; then
	tiff_save_LIBS="$LIBS"
	tiff_save_INCLUDES="$INCLUDES"

	LIBS="-ltiff -lm $LIBS"
	INCLUDES="$TIFF_INCLUDES $INCLUDES"

	# Try the standard search path first
	AC_TRY_LINK([#include <tiff.h>],[TIFFGetVersion();], [
		TIFF_LIBS="-ltiff"
	], [
		# libtiff is not in the standard search path.

		# A whole bunch of guesses
		for dir in \
			"${prefix}"/*/lib /usr/*/lib /usr/local/*/lib \
			"${prefix}"/lib/* /usr/lib/* \
			/usr/local/lib/* /*/lib; do
			if test -d "$dir" && test "`ls $dir/libtiff.* 2> /dev/null`" != ""; then
				TIFF_LIBS="-L$dir -ltiff"
				break
			fi
		done

		if test "$TIFF_LIBS" = ""; then
			TIFF_LIBS=no
		fi
	])

	LIBS="$tiff_save_LIBS"
	INCLUDES="$tiff_save_INCLUDES"
fi

AC_SUBST(TIFF_LIBS)
AC_SUBST(TIFF_INCLUDES)

# Print a helpful message
tiff_libraries_result="$TIFF_LIBS"
tiff_includes_result="$TIFF_INCLUDES"

if test x"$tiff_libraries_result" = x""; then
	tiff_libraries_result="in default path"
fi
if test x"$tiff_includes_result" = x""; then
	tiff_includes_result="in default path"
fi

if test "$tiff_libraries_result" = "no"; then
	tiff_libraries_result="(none)"
fi
if test "$tiff_includes_result" = "no"; then
	tiff_includes_result="(none)"
fi

AC_MSG_RESULT(
  [libraries $tiff_libraries_result, headers $tiff_includes_result])

# Finally, execute ACTION-IF-FOUND/ACTION-IF-NOT-FOUND:
if test "$TIFF_INCLUDES" != "no" && test "$TIFF_LIBS" != "no"; then
        ifelse([$1],,AC_DEFINE(HAVE_TIFF,1,[Define if you have tiff libraries and header files.]),[$1])
        :
else
	TIFF_INCLUDES=""
	TIFF_LIBS=""
        $2
fi

])dnl

dnl
dnl FIND_JPEG[ACTION-IF-FOUND[, ACTION-IF-NOT-FOUND]]
dnl ------------------------------------------------
dnl
dnl Find JPEG libraries and headers
dnl
dnl Put compile stuff in JPEG_INCLUDES
dnl Put link stuff in JPEG_LIBS
dnl
dnl Default ACTION-IF-FOUND defines HAVE_JPEG
dnl
AC_DEFUN([FIND_JPEG], [

JPEG_INCLUDES=""
JPEG_LIBS=""

dnl AC_ARG_WITH(jpeg, 
dnl [  --without-jpeg		do not use libjpeg])
# Treat --without-jpeg like --without-jpeg-includes --without-jpeg-libraries.
if test "$with_jpeg" = "no"; then
	JPEG_INCLUDES=no
	JPEG_LIBS=no
fi

AC_ARG_WITH(jpeg-includes,
[  --with-jpeg-includes=DIR	JPEG include files are in DIR],
JPEG_INCLUDES="-I$withval")
AC_ARG_WITH(jpeg-libraries,
[  --with-jpeg-libraries=DIR	JPEG libraries are in DIR],
JPEG_LIBS="-L$withval -ljpeg")

AC_MSG_CHECKING(for JPEG)

# Look for jpeg.h 
if test "$JPEG_INCLUDES" = ""; then
	jpeg_save_LIBS="$LIBS"

	LIBS="-ljpeg $LIBS"

	# Check the standard search path
	AC_TRY_COMPILE([
		#include <stdio.h>
		#include <jpeglib.h>],[int a],[
		JPEG_INCLUDES=""
	], [
		# jpeg.h is not in the standard search path.

		# A whole bunch of guesses
		for dir in \
			"${prefix}"/*/include \
			/usr/local/include \
			/usr/*/include \
			/usr/local/*/include /usr/*/include \
			"${prefix}"/include/* \
			/usr/include/* /usr/local/include/* /*/include; do
			if test -f "$dir/jpeglib.h"; then
				JPEG_INCLUDES="-I$dir"
				break
			fi
		done

		if test "$JPEG_INCLUDES" = ""; then
			JPEG_INCLUDES=no
		fi
	])

	LIBS="$jpeg_save_LIBS"
fi

# Now for the libraries
if test "$JPEG_LIBS" = ""; then
	jpeg_save_LIBS="$LIBS"
	jpeg_save_INCLUDES="$INCLUDES"

	LIBS="-ljpeg $LIBS"
	INCLUDES="$JPEG_INCLUDES $INCLUDES"

	# Try the standard search path first
	AC_TRY_LINK([
		#include <stdio.h>
		#include <jpeglib.h>],[jpeg_abort((void*)0)], [
		JPEG_LIBS="-ljpeg"
	], [
		# libjpeg is not in the standard search path.

		# A whole bunch of guesses
		for dir in \
			"${prefix}"/*/lib \
			/usr/local/lib \
			/usr/*/lib \
			"${prefix}"/lib/* /usr/lib/* \
			/usr/local/lib/* /*/lib; do
			if test -d "$dir" && test "`ls $dir/libjpeg.* 2> /dev/null`" != ""; then
				JPEG_LIBS="-L$dir -ljpeg"
				break
			fi
		done

		if test "$JPEG_LIBS" = ""; then
			JPEG_LIBS=no
		fi
	])

	LIBS="$jpeg_save_LIBS"
	INCLUDES="$jpeg_save_INCLUDES"
fi

AC_SUBST(JPEG_LIBS)
AC_SUBST(JPEG_INCLUDES)

# Print a helpful message
jpeg_libraries_result="$JPEG_LIBS"
jpeg_includes_result="$JPEG_INCLUDES"

if test x"$jpeg_libraries_result" = x""; then
	jpeg_libraries_result="in default path"
fi
if test x"$jpeg_includes_result" = x""; then
	jpeg_includes_result="in default path"
fi

if test "$jpeg_libraries_result" = "no"; then
	jpeg_libraries_result="(none)"
fi
if test "$jpeg_includes_result" = "no"; then
	jpeg_includes_result="(none)"
fi

AC_MSG_RESULT(
  [libraries $jpeg_libraries_result, headers $jpeg_includes_result])

# Finally, execute ACTION-IF-FOUND/ACTION-IF-NOT-FOUND:
if test "$JPEG_INCLUDES" != "no" && test "$JPEG_LIBS" != "no"; then
        ifelse([$1],,AC_DEFINE(HAVE_JPEG,1,[Define if you have jpeg libraries and header files.]),[$1])
        :
else
	JPEG_INCLUDES=""
	JPEG_LIBS=""
        $2
fi

])
dnl


# AC_HEADER_TR1_UNORDERED_MAP
AC_DEFUN([AC_HEADER_TR1_UNORDERED_MAP], [
  AC_CACHE_CHECK(for tr1/unordered_map,
  ac_cv_cxx_tr1_unordered_map,
  [AC_LANG_SAVE
  AC_LANG_CPLUSPLUS
  AC_TRY_COMPILE([#include <tr1/unordered_map>], [using std::tr1::unordered_map;],
  ac_cv_cxx_tr1_unordered_map=yes, ac_cv_cxx_tr1_unordered_map=no)
  AC_LANG_RESTORE
  ])
  if test "$ac_cv_cxx_tr1_unordered_map" = yes; then
    AC_DEFINE(HAVE_TR1_UNORDERED_MAP,,[Define if tr1/unordered_map is present. ])
  fi
])


# AC_COMPILE_STDCXX_OX
AC_DEFUN([AC_COMPILE_STDCXX_0X], [
  AC_CACHE_CHECK(if g++ supports C++0x features without additional flags,
  ac_cv_cxx_compile_cxx0x_native,
  [AC_LANG_SAVE
  AC_LANG_CPLUSPLUS
  AC_TRY_COMPILE([
  template <typename T>
    struct check
    {
      static_assert(sizeof(int) <= sizeof(T), "not big enough");
    };

    typedef check<check<bool>> right_angle_brackets;

    int a;
    decltype(a) b;

    typedef check<int> check_type;
    check_type c;
    check_type&& cr = c;],,
  ac_cv_cxx_compile_cxx0x_native=yes, ac_cv_cxx_compile_cxx0x_native=no)
  AC_LANG_RESTORE
  ])

  AC_CACHE_CHECK(if g++ supports C++0x features with -std=c++0x,
  ac_cv_cxx_compile_cxx0x_cxx,
  [AC_LANG_SAVE
  AC_LANG_CPLUSPLUS
  ac_save_CXXFLAGS="$CXXFLAGS"
  CXXFLAGS="$CXXFLAGS -std=c++0x"
  AC_TRY_COMPILE([
  template <typename T>
    struct check
    {
      static_assert(sizeof(int) <= sizeof(T), "not big enough");
    };

    typedef check<check<bool>> right_angle_brackets;

    int a;
    decltype(a) b;

    typedef check<int> check_type;
    check_type c;
    check_type&& cr = c;],,
  ac_cv_cxx_compile_cxx0x_cxx=yes, ac_cv_cxx_compile_cxx0x_cxx=no)
  CXXFLAGS="$ac_save_CXXFLAGS"
  AC_LANG_RESTORE
  ])

  AC_CACHE_CHECK(if g++ supports C++0x features with -std=gnu++0x,
  ac_cv_cxx_compile_cxx0x_gxx,
  [AC_LANG_SAVE
  AC_LANG_CPLUSPLUS
  ac_save_CXXFLAGS="$CXXFLAGS"
  CXXFLAGS="$CXXFLAGS -std=gnu++0x"
  AC_TRY_COMPILE([
  template <typename T>
    struct check
    {
      static_assert(sizeof(int) <= sizeof(T), "not big enough");
    };

    typedef check<check<bool>> right_angle_brackets;

    int a;
    decltype(a) b;

    typedef check<int> check_type;
    check_type c;
    check_type&& cr = c;],,
  ac_cv_cxx_compile_cxx0x_gxx=yes, ac_cv_cxx_compile_cxx0x_gxx=no)
  CXXFLAGS="$ac_save_CXXFLAGS"
  AC_LANG_RESTORE
  ])

  if test "$ac_cv_cxx_compile_cxx0x_native" = yes ||
     test "$ac_cv_cxx_compile_cxx0x_cxx" = yes ||
     test "$ac_cv_cxx_compile_cxx0x_gxx" = yes; then
    AC_DEFINE(HAVE_STDCXX_0X,,[Define if g++ supports C++0x features. ])
  fi
])


AU_ALIAS([AC_CXX_HEADER_UNORDERED_MAP], [AX_CXX_HEADER_UNORDERED_MAP])
AC_DEFUN([AX_CXX_HEADER_UNORDERED_MAP], [
  AC_CACHE_CHECK(for unordered_map,
  ax_cv_cxx_unordered_map,
  [AC_REQUIRE([AC_COMPILE_STDCXX_0X])
  AC_LANG_SAVE
  AC_LANG_CPLUSPLUS
  ac_save_CXXFLAGS="$CXXFLAGS"
  CXXFLAGS="$CXXFLAGS -std=gnu++0x"
  AC_TRY_COMPILE([#include <unordered_map>], [using std::unordered_map;],
  ax_cv_cxx_unordered_map=yes, ax_cv_cxx_unordered_map=no)
  CXXFLAGS="$ac_save_CXXFLAGS"
  AC_LANG_RESTORE
  ])
  if test "$ax_cv_cxx_unordered_map" = yes; then
    AC_DEFINE(HAVE_UNORDERED_MAP,,[Define if unordered_map is present. ])
    AC_SUBST([AM_CXXFLAGS], [-std=gnu++0x])
  fi
])


AU_ALIAS([AC_CXX_NAMESPACES], [AX_CXX_NAMESPACES])
AC_DEFUN([AX_CXX_NAMESPACES],
[AC_CACHE_CHECK(whether the compiler implements namespaces,
ax_cv_cxx_namespaces,
[AC_LANG_PUSH([C++])
 AC_COMPILE_IFELSE([AC_LANG_SOURCE([namespace Outer { namespace Inner { int i = 0; }}
                                   using namespace Outer::Inner; int foo(void) { return i;} ])],
                   ax_cv_cxx_namespaces=yes, ax_cv_cxx_namespaces=no)
 AC_LANG_POP
])
if test "$ax_cv_cxx_namespaces" = yes; then
  AC_DEFINE(HAVE_NAMESPACES,,[define if the compiler implements namespaces])
fi
])


AU_ALIAS([AC_CXX_HAVE_EXT_HASH_MAP], [AX_CXX_HAVE_EXT_HASH_MAP])
AC_DEFUN([AX_CXX_HAVE_EXT_HASH_MAP],
[AC_CACHE_CHECK(whether the compiler has ext/hash_map,
ax_cv_cxx_have_ext_hash_map,
[AC_REQUIRE([AX_CXX_NAMESPACES])
  AC_LANG_SAVE
  AC_LANG_CPLUSPLUS
  AC_TRY_COMPILE([#include <ext/hash_map>
#ifdef HAVE_NAMESPACES
using namespace __gnu_cxx;
#endif],[hash_map<int, int> t; return 0;],
  ax_cv_cxx_have_ext_hash_map=yes, ax_cv_cxx_have_ext_hash_map=no)
  AC_LANG_RESTORE
])
if test "$ax_cv_cxx_have_ext_hash_map" = yes; then
   AC_DEFINE(HAVE_EXT_HASH_MAP,,[define if the compiler has ext/hash_map])
fi
])


dnl @synopsis AX_CXX_HAVE_ISFINITE
dnl
dnl If isfinite() is available to the C++ compiler:
dnl define HAVE_ISFINITE
dnl add "-lm" to LIBS
dnl
AC_DEFUN([AX_CXX_HAVE_ISFINITE],
[ax_cxx_have_isfinite_save_LIBS=$LIBS
LIBS="$LIBS -lm"
AC_CACHE_CHECK(for isfinite, ax_cv_cxx_have_isfinite,
[AC_LANG_SAVE
AC_LANG_CPLUSPLUS
AC_LINK_IFELSE(
[AC_LANG_PROGRAM(
[[#include <math.h>]],
[[int f = isfinite( 3 );]])],
[ax_cv_cxx_have_isfinite=yes],
[ax_cv_cxx_have_isfinite=no])
AC_LANG_RESTORE])
if test "$ax_cv_cxx_have_isfinite" = yes; then
AC_DEFINE([HAVE_ISFINITE],1,[define if compiler has isfinite])
else
LIBS=$ax_cxx_have_isfinite_save_LIBS
fi
])
