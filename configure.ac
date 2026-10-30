# Process this file with autoconf to produce a configure script.

AC_INIT([iipsrv],[1.4],[ruven@users.sourceforge.net],[iipsrv],[https://iipimage.sourceforge.io])
AC_CONFIG_SRCDIR([src/Main.cc])
AC_CONFIG_MACRO_DIR([m4])

# Print the banner first, before any other configure output.
# Write directly to the message stream (fd 6) to avoid the "configure: " prefix.
cat <<'EOF' >&6
Configuring iipsrv ...

EOF

AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_RANLIB
AM_PROG_AR

AC_C_INLINE

# Required C++ types (fail if missing)
AC_CHECK_TYPE([size_t],   [], [AC_MSG_ERROR([size_t type not found])], [[#include <stddef.h>]])
AC_CHECK_TYPE([uint8_t],  [], [AC_MSG_ERROR([uint8_t type not found])], [[#include <stdint.h>]])
AC_CHECK_TYPE([uint32_t], [], [AC_MSG_ERROR([uint32_t type not found])], [[#include <stdint.h>]])
AC_CHECK_TYPE([uint64_t], [], [AC_MSG_ERROR([uint64_t type not found])], [[#include <stdint.h>]])


# Checks for headers that are used for conditional compilation
AC_CHECK_HEADERS([sys/time.h])
AC_CHECK_HEADERS([sys/stat.h], [], [AC_MSG_ERROR([sys/stat.h not found - required for stat()])])
AC_CHECK_HEADERS([syslog.h], [LOGGING="file, syslog"], [LOGGING="file"])

# Checks for libraries and functions
AC_SEARCH_LIBS([pow], [m])
AC_CHECK_FUNCS([log2])
AC_CHECK_FUNCS([setenv])
AC_CHECK_FUNCS([stat], [], [AC_MSG_ERROR([stat() not found])])

# Allow user to disable glob
AC_ARG_ENABLE([glob],
  [AS_HELP_STRING([--disable-glob], [disable globbing])])
AS_IF([test "x$enable_glob" = "xno"],
  [AC_MSG_NOTICE([configure: glob support disabled])],
  [AC_CHECK_HEADERS([glob.h])])

# We want largefile support, if possible
AC_SYS_LARGEFILE

# Check for C99 isfinite function
AX_CXX_HAVE_ISFINITE


# Pthread support
AX_PTHREAD
AM_CPPFLAGS=""
AM_CXXFLAGS="$PTHREAD_CFLAGS"
LIBS="$PTHREAD_LIBS $LIBS"


# Check for OpenMP
AC_ARG_ENABLE([openmp],
  [AS_HELP_STRING([--disable-openmp], [disable OpenMP support])])
OPENMP=false
AS_IF([test "x$enable_openmp" != "xno"],
  [AC_LANG_PUSH([C++])
   AX_OPENMP([OPENMP=true])
   AC_LANG_POP([C++])])
AS_IF([test "x$OPENMP" = "xtrue"],
  [AM_CXXFLAGS="${OPENMP_CXXFLAGS} ${AM_CXXFLAGS}"
   LIBS="$OPENMP_LIBS $LIBS"])


# Check for C++ chrono timers and availability of various hash map implementations
AX_CXX_COMPILE_STDCXX(11)
AC_LANG_PUSH([C++])
AC_CHECK_HEADERS([chrono])
AC_CHECK_HEADER([thread], [AC_DEFINE([HAVE_STL_THREAD], [1], [Define if C++ <thread> is available])])
AC_LANG_POP([C++])



# Check for pkg-config, required for the optional library detection below
PKG_PROG_PKG_CONFIG


# Test for Eigen - allow user to disable
AC_ARG_ENABLE([eigen],
  [AS_HELP_STRING([--disable-eigen], [disable Eigen support])])
EIGEN=false
AS_IF([test "x$enable_eigen" = "xno"],
  [AC_MSG_NOTICE([configure: Eigen support disabled])],
  [PKG_CHECK_MODULES([EIGEN], [eigen3],
    [EIGEN=true
     AC_DEFINE([HAVE_EIGEN3], [1], [Define if Eigen3 is available])
     AM_CPPFLAGS="${EIGEN_CFLAGS} ${AM_CPPFLAGS}"],
    [EIGEN=false])])


# Enable debugging
AC_ARG_ENABLE([debug-mode],
  [AS_HELP_STRING([--enable-debug-mode], [enable debugging mode (no FCGI)])])
DEBUG=false
AS_IF([test "x$enable_debug_mode" = "xyes"],
  [AC_DEFINE([DEBUG], [1], [Define to enable debug mode])
   AC_DEFINE([KAKADU_DEBUG], [1], [Define to enable Kakadu debug output])
   AC_DEFINE([OPENJPEG_DEBUG], [1], [Define to enable OpenJPEG debug output])
   AC_DEFINE([JPEG_DEBUG], [1], [Define to enable libjpeg debug output])
   DEBUG=true
   AC_MSG_NOTICE([configure: enabling debug mode])])




# Check for libtiff and libjpeg
AX_CHECK_LIBTIFF
AX_CHECK_LIBJPEG


#************************************************************
# Check for libmemcached
#************************************************************
AC_ARG_ENABLE([libmemcached],
  [AS_HELP_STRING([--disable-libmemcached], [disable libmemcached support])])
if test "x$enable_libmemcached" = "xno"; then
  MEMCACHED=false
  AC_MSG_NOTICE([configure: libmemcached support disabled])
else
  AC_CHECK_HEADER( [libmemcached/memcached.h],
	[AC_SEARCH_LIBS( [memcached_create],
		[memcached],
		[MEMCACHED=true],
		[MEMCACHED=false],
		[-lpthread] )],
	[MEMCACHED=false]
  )

  if test "x${MEMCACHED}" = xtrue; then
	AC_DEFINE([HAVE_MEMCACHED], [1], [Define if libmemcached is available])
  fi
fi




#************************************************************
# Check for little cms library

#AC_CHECK_HEADERS(lcms2.h,
#	AC_SEARCH_LIBS( cmsOpenProfileFromMem, lcms2,
#		LCMS=true; AC_DEFINE(LCMS),
#		LCMS=false )
#)
#LCMS=false



#************************************************************
#     Check for PNG support
#************************************************************

PNG=false
AC_ARG_ENABLE( png,
    [  --disable-png           disable PNG])


if test "x$enable_png" = "xno"; then
   AC_MSG_NOTICE([configure: disabling PNG support])
   AM_CONDITIONAL([ENABLE_PNG], [false])
else
   AC_CHECK_HEADER( [png.h],
     [AC_SEARCH_LIBS(
       [png_create_write_struct],
       [png],
       [PNG=true],
       [PNG=false] )]
   )
   if test "x${PNG}" = xtrue; then
	AM_CONDITIONAL([ENABLE_PNG], [true])
	AC_DEFINE([HAVE_PNG], [1], [Define if libpng is available])
   else
	AM_CONDITIONAL([ENABLE_PNG], [false])
   fi
fi


#************************************************************
#     Check for WebP support
#************************************************************

WEBP=false
AC_ARG_ENABLE( webp,
    [AS_HELP_STRING([--disable-webp], [disable WebP])])


if test "x$enable_webp" = "xno"; then
   AC_MSG_NOTICE([configure: disabling WebP support])
   AM_CONDITIONAL([ENABLE_WEBP], [false])
else
   AC_CHECK_HEADER( [webp/encode.h], [WEBP=true], [WEBP=false] )
   if test "x$WEBP" = xtrue; then
      AC_SEARCH_LIBS( [WebPEncode], [webp], [WEBP=true], [WEBP=false] )
   fi
   if test "x$WEBP" = xtrue; then
      AC_SEARCH_LIBS( [WebPMuxAssemble], [webpmux], [WEBP=true], [WEBP=false] )
   fi

   if test "x${WEBP}" = xtrue; then
      AM_CONDITIONAL([ENABLE_WEBP], [true])
      AC_DEFINE([HAVE_WEBP], [1], [Define if libwebp is available])
   else
      AM_CONDITIONAL([ENABLE_WEBP], [false])
   fi

fi


#************************************************************
#     Check for AVIF support
#************************************************************

AVIF=false
AC_ARG_ENABLE( avif,
    [AS_HELP_STRING([--disable-avif], [disable AVIF])])


if test "x$enable_avif" = "xno"; then
   AC_MSG_NOTICE([configure: disabling AVIF support])
   AM_CONDITIONAL([ENABLE_AVIF], [false])
else
   AC_CHECK_HEADER( [avif/avif.h], [AVIF=true], [AVIF=false] )
   if test "x$AVIF" = xtrue; then
      AC_SEARCH_LIBS( [avifEncoderAddImage], [avif], [AVIF=true], [AVIF=false] )
   fi

   if test "x${AVIF}" = xtrue; then
      AM_CONDITIONAL([ENABLE_AVIF], [true])
      AC_DEFINE([HAVE_AVIF], [1], [Define if libavif is available])
   else
      AM_CONDITIONAL([ENABLE_AVIF], [false])
   fi

fi


#************************************************************
# Check for libdl for dynamic library loading
#************************************************************

AC_ARG_ENABLE(modules,
    [AS_HELP_STRING([--enable-modules], [enable dynamic module loading])] )

if test "$enable_modules" = "yes"; then

	modules=true
	AC_CHECK_HEADER([dlfcn.h],
		[AC_SEARCH_LIBS( dlopen, dl, AC_DEFINE([ENABLE_DL], [1], [Define if dlopen is available]), AC_MSG_ERROR(unable to find libdl) )],
		[AC_MSG_ERROR(dlfcn.h not found - required for dynamic module loading)])

else
	modules=false
	AC_MSG_NOTICE( [configure: optional dynamic module loading not enabled] )

fi

AM_CONDITIONAL( [ENABLE_MODULES], [test x$modules = xtrue] )
AC_SUBST(DL_LIBS)




#************************************************************
# Check for JPEG2000 support from either Kakadu or OpenJPEG
#************************************************************

JPEG2000_CODEC=false


# First check for Kakadu

AC_ARG_WITH( kakadu,
   [AS_HELP_STRING([--with-kakadu=DIR], [location of Kakadu source files (Kakadu will disable OpenJPEG support)])],
   kakadu_path=$withval)

if test -n "$kakadu_path"; then

	AS_IF([test -f "$kakadu_path/managed/all_includes/jpx.h"],
		[AC_MSG_NOTICE([configure: Found Kakadu sources. Will compile JPEG2000 support])
		 KAKADU=true],
		[AC_MSG_NOTICE([configure: No Kakadu JPEG2000 Sources Found])
		 KAKADU=false])
	AM_CONDITIONAL([ENABLE_KAKADU],[test x$KAKADU = xtrue])

	if test "x${KAKADU}" = xtrue; then
		AC_DEFINE([HAVE_KAKADU], [1], [Define if Kakadu JPEG2000 codec is available])
		AM_CPPFLAGS="-I\"$kakadu_path/managed/all_includes/\" ${AM_CPPFLAGS}"
		KAKADU_OBJS="$kakadu_path/apps/make/jpx.o $kakadu_path/apps/make/jp2.o $kakadu_path/apps/make/kdu_stripe_decompressor.o"

		AC_MSG_CHECKING([for Kakadu library])
		kakadu_lib=""
		for f in "$kakadu_path"/apps/make/libkdu*.so \
		         "$kakadu_path"/apps/make/libkdu*.dylib; do
			if test -f "$f"; then
				kakadu_lib="$f"
				break
			fi
		done
		if test -n "$kakadu_lib"; then
			AC_MSG_RESULT([$kakadu_lib])
			LIBS="$LIBS \"$kakadu_lib\""
		else
			AC_MSG_ERROR([unable to find Kakadu library in $kakadu_path/apps/make])
		fi

		# Check for SSSE3 support in Kakadu
		AS_IF([test -f "$kakadu_path/apps/make/ssse3_stripe_transfer.o"],
			[AC_MSG_NOTICE([configure: Kakadu has support for SSE3])
			 KDU_SSSE3=true])
		if test "x${KDU_SSSE3}" = xtrue; then
			KAKADU_OBJS="$KAKADU_OBJS $kakadu_path/apps/make/ssse3_stripe_transfer.o"
		fi

		# Check for AVX2 support in Kakadu
		AS_IF([test -f "$kakadu_path/apps/make/avx2_stripe_transfer.o"],
			[AC_MSG_NOTICE([configure: Kakadu has support for AVX2])
			 KDU_AVX2=true])
		if test "x${KDU_AVX2}" = xtrue; then
			KAKADU_OBJS="$KAKADU_OBJS $kakadu_path/apps/make/avx2_stripe_transfer.o"
		fi

		# Check for addition of kdu_client_window in versions 7.5 and upwards
		AS_IF([test -f "$kakadu_path/apps/make/kdu_client_window.o"],
			[AC_MSG_NOTICE([configure: Kakadu is >= v7.5])
			 KDU_75=true],
			[AC_MSG_NOTICE([configure: Kakadu is < v7.5])])
		if test "x${KDU_75}" = xtrue; then
			KAKADU_OBJS="$KAKADU_OBJS $kakadu_path/apps/make/kdu_client_window.o"
		fi

		AS_IF([test -f "$kakadu_path/apps/make/supp_local.o"],
			[AC_MSG_NOTICE([configure: Kakadu is >= v7.10])
			 KDU_7A=true],
			[AC_MSG_NOTICE([configure: Kakadu is < v7.10])])
		if test "x${KDU_7A}" = xtrue; then
			KAKADU_OBJS="$KAKADU_OBJS $kakadu_path/apps/make/supp_local.o"
			AC_DEFINE([HAVE_KAKADUMETANODE], [1], [Define if Kakadu metanode support is available])
		fi

		JPEG2000_CODEC="true (Kakadu)"

		AC_SUBST(KAKADU_OBJS)

		AC_CHECK_FUNC([get_nprocs_conf],[AC_DEFINE([NPROCS], [1], [Define if get_nprocs_conf is available])])
	fi
else
	AM_CONDITIONAL([ENABLE_KAKADU],[false])
fi





# Check for OpenJPEG if no Kakadu support has been found

OPENJPEG=false

AC_ARG_ENABLE(openjpeg,
    [AS_HELP_STRING([--disable-openjpeg], [disable OpenJPEG])] )

AC_ARG_WITH( openjpeg,
   [AS_HELP_STRING([--with-openjpeg=DIR], [location of OpenJPEG source files])],
   openjpeg_path=$withval )



# Only check for OpenJPEG if Kakadu has not been enabled
if test "x${KAKADU}" = xtrue; then
	AM_CONDITIONAL([ENABLE_OPENJPEG], [false])
else

	if test "x$enable_openjpeg" = "xno"; then
		AC_MSG_NOTICE([disabling OpenJPEG support])
		AM_CONDITIONAL([ENABLE_OPENJPEG], [false])
	else

		if test -n "$openjpeg_path"; then
		    AS_IF([test -f "$openjpeg_path/src/lib/openjp2/openjpeg.h"],
		        [AC_MSG_NOTICE([configure: Using specified OpenJPEG sources])
		         OPENJPEG=true],
		        [AC_MSG_NOTICE([configure: No OpenJPEG sources found])
		         OPENJPEG=false])
		    if test "x$OPENJPEG" = xtrue; then
		        AM_CPPFLAGS="-I\"$openjpeg_path/src/lib/openjp2/\" ${AM_CPPFLAGS}"
		        LIBS="$LIBS \"$openjpeg_path/bin/libopenjp2.so\""
		    fi
		else
		    PKG_CHECK_MODULES([OPENJPEG], [libopenjp2], [OPENJPEG=true], [OPENJPEG=false])
		    if test "x$OPENJPEG" = xtrue; then
		        AM_CPPFLAGS="${OPENJPEG_CFLAGS} ${AM_CPPFLAGS}"
		        LIBS="$LIBS ${OPENJPEG_LIBS}"
		    else
			AC_MSG_NOTICE([configure: Unable to find OpenJPEG development files])
		    fi
		fi

		if test "x$OPENJPEG" = xtrue; then
		    AC_DEFINE([HAVE_OPENJPEG], [1], [Define if OpenJPEG JPEG2000 codec is available])
		    JPEG2000_CODEC="true (OpenJPEG)"
		    AM_CONDITIONAL([ENABLE_OPENJPEG], [test x$OPENJPEG = xtrue])
		else
		    AM_CONDITIONAL([ENABLE_OPENJPEG], [false])
		fi

	fi
fi




#************************************************************
#     Check for FCGI library
#************************************************************

AC_CHECK_HEADER([fcgiapp.h],
	[AC_CHECK_LIB(
		[fcgi],
		[FCGI_Accept],
		[BUILD_FCGI=false],
		[BUILD_FCGI=true])],
	[BUILD_FCGI=true]
)
if test "x${BUILD_FCGI}" = xtrue; then
	AC_MSG_NOTICE([configure: unable to find libfcgi - using bundled version])
	AC_CHECK_LIB([nsl],       [gethostbyname])
	AC_CHECK_LIB([socket],    [socket])
	FCGI_COMMON_CHECKS
	AC_REPLACE_FUNCS([strerror])
	AM_CPPFLAGS="-I\$(top_srcdir)/fcgi/include -I\$(top_srcdir)/fcgi ${AM_CPPFLAGS}"
	AM_CONDITIONAL([BUNDLED_FCGI], [true])
else
	LIBS="$LIBS -lfcgi"
	AM_CONDITIONAL([BUNDLED_FCGI], [false])
fi



# Pass flags to Makefiles
AC_SUBST(AM_CPPFLAGS)
AC_SUBST(AM_CXXFLAGS)
AC_SUBST(AM_LDFLAGS)
AC_SUBST(LIBS)


# Generate Make files
AC_CONFIG_FILES([Makefile \
	src/Makefile \
	man/Makefile \
	fcgi/Makefile \
	fcgi/include/Makefile \
	fcgi/libfcgi/Makefile])
AC_OUTPUT


# Print out option status
cat <<EOF >&6

Options Enabled:
-----------------------------------
 Memcached      :  ${MEMCACHED}
 JPEG2000       :  ${JPEG2000_CODEC}
 OpenMP         :  ${OPENMP}
 Eigen          :  ${EIGEN}
 Loggers        :  ${LOGGING}
 PNG  Output    :  ${PNG}
 WebP Output    :  ${WEBP}
 AVIF Output    :  ${AVIF}
EOF

if test "x${DEBUG}" = xtrue; then
  cat <<EOF >&6
 Debug mode  :  activated
EOF
fi

cat <<EOF >&6
-----------------------------------
EOF
