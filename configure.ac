# Process this file with autoconf to produce a configure script.

AC_INIT([iipsrv],[1.3],[ruven@users.sourceforge.net],[iipsrv],[https://iipimage.sourceforge.io])
AC_MSG_RESULT([Configuring iipsrv ...
])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE


# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_UINT8_T
AC_TYPE_UINT32_T

LT_INIT


# Checks for header specific files
AC_CHECK_HEADERS(sys/stat.h)
AC_CHECK_HEADERS(time.h)
AC_CHECK_HEADERS(sys/time.h)
AC_CHECK_HEADERS(syslog.h, [LOGGING="file, syslog"], [LOGGING="file"])

# Checks for libraries and functions
AC_SEARCH_LIBS([pow], [m])
AC_CHECK_LIB([m], [log2], [AC_DEFINE(HAVE_LOG2)])
AC_CHECK_FUNCS([setenv])
AC_FUNC_MALLOC

# Allow user to disable glob
AC_ARG_ENABLE([glob],[  --disable-glob          disable globbing])
if test "x$enable_glob" == "xno"; then
  AC_MSG_RESULT([configure: glob support disabled])
else
  AC_CHECK_HEADERS(glob.h)
fi


# For our windows build
AC_CHECK_HEADERS(windows.h)

# We want largefile support, if possible
AC_SYS_LARGEFILE

# Check for C99 isfinite function
AX_CXX_HAVE_ISFINITE


# Pthread support
AX_PTHREAD
CXXFLAGS="$CXXFLAGS $PTHREAD_CFLAGS"
LIBS="$PTHREAD_LIBS $LIBS"


# Check for OpenMP
AC_OPENMP
OPENMP=false
AS_IF([test "x$enable_openmp" != "xyes"], [
 AM_CXXFLAGS="${OPENMP_CFLAGS} ${AM_CXXFLAGS}"
 OPENMP=true
])


# Check for C++ chrono timers and availability of various hash map implementations
AC_LANG_PUSH([C++])
AC_CHECK_HEADERS(ext/pool_allocator.h)
AC_CHECK_HEADERS([chrono])
AC_CHECK_HEADERS([unordered_map])
AC_CHECK_HEADERS([tr1/unordered_map])
AC_CHECK_HEADERS([ext/hash_map])
AX_CXX_COMPILE_STDCXX(11)
AC_LANG_POP([C++])


# Enable debugging
AC_ARG_ENABLE([debug-mode],
              [  --enable-debug-mode     enable debugging mode (no FCGI)],
              [AC_DEFINE(DEBUG)
	       AC_DEFINE(KAKADU_DEBUG)
               AC_DEFINE(OPENJPEG_DEBUG)
               DEBUG=true
	       AC_MSG_RESULT([configure: enabling debug mode])]
)




# Check for libtiff and libjpeg
AX_CHECK_LIBTIFF
AX_CHECK_LIBJPEG


#************************************************************
# Check for libmemcached
#************************************************************
AC_ARG_ENABLE([libmemcached],[  --disable-libmemcached  disable libmemcached support])
if test "x$enable_libmemcached" == "xno"; then
  MEMCACHED=false
  AC_MSG_RESULT([configure: libmemcached support disabled])
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
	AC_DEFINE(HAVE_MEMCACHED)
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


if test "x$enable_png" == "xno"; then
   AC_MSG_RESULT([configure: disabling PNG support])
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
	AC_DEFINE(HAVE_PNG)
   else
	AM_CONDITIONAL([ENABLE_PNG], [false])
   fi
fi


#************************************************************
#     Check for WebP support
#************************************************************

WEBP=false
AC_ARG_ENABLE( webp,
    [  --disable-webp          disable WebP])


if test "x$enable_webp" == "xno"; then
   AC_MSG_RESULT([configure: disabling WebP support])
   AM_CONDITIONAL([ENABLE_WEBP], [false])
else
   AC_CHECK_HEADER( [webp/encode.h], [WEBP=true], [WEBP=false] )
   AC_SEARCH_LIBS( [WebPEncode], [webp], [WEBP=true], [WEBP=false] )
   AC_SEARCH_LIBS( [WebPMuxAssemble], [webpmux], [WEBP=true], [WEBP=false] )

   if test "x${WEBP}" = xtrue; then
      AM_CONDITIONAL([ENABLE_WEBP], [true])
      AC_DEFINE(HAVE_WEBP)
   else
      AM_CONDITIONAL([ENABLE_WEBP], [false])
   fi

fi



#************************************************************
# Check for libdl for dynamic library loading
#************************************************************

AC_ARG_ENABLE(modules,
    [  --enable-modules        enable dynamic module loading] )

if test "$enable_modules" = "yes"; then

	modules=true
	AC_CHECK_HEADERS(dlfcn.h,
		AC_SEARCH_LIBS( dlopen, dl, AC_DEFINE(ENABLE_DL), AC_MSG_ERROR(unable to find libdl) )
	)

else
	modules=false
	AC_MSG_RESULT( [configure: optional dynamic module loading not enabled] )

fi

AM_CONDITIONAL( [ENABLE_MODULES], [test x$modules = xtrue] )
AC_SUBST(DL_LIBS)




#************************************************************
# Check for JPEG2000 support from either Kakadu or OpenJPEG
#************************************************************

JPEG2000_CODEC=false


# First check for Kakadu

AC_ARG_WITH( kakadu,
   [  --with-kakadu=DIR       location of Kakadu source files (Kakadu will disable OpenJPEG support) ],
   kakadu_path=$withval)

if test -n "$kakadu_path"; then

	AC_CHECK_FILE( $kakadu_path/managed/all_includes/jpx.h,
		AC_MSG_RESULT([configure: Found Kakadu sources. Will compile JPEG2000 support]); KAKADU=true;,
		AC_MSG_RESULT([configure: No Kakadu JPEG2000 Sources Found]); KAKADU=false
	)
	AM_CONDITIONAL([ENABLE_KAKADU],[test x$KAKADU = xtrue])

	if test "x${KAKADU}" = xtrue; then
		AC_DEFINE(HAVE_KAKADU)
		AM_CXXFLAGS="-I$kakadu_path/managed/all_includes/ ${AM_CXXFLAGS}"
		KAKADU_OBJS="$kakadu_path/apps/make/jpx.o $kakadu_path/apps/make/jp2.o $kakadu_path/apps/make/kdu_stripe_decompressor.o"
		kakadu_lib_name=`basename $kakadu_path/apps/make/*.so | sed 's/^lib//' | sed 's/.so$//'`
		LIBS="$LIBS -L$kakadu_path/apps/make -l$kakadu_lib_name"

		# Check for SSSE3 support in Kakadu
		AC_CHECK_FILE( $kakadu_path/apps/make/ssse3_stripe_transfer.o,
			AC_MSG_RESULT([configure: Kakadu has support for SSE3]);
			KDU_SSSE3=true
		)
		if test "x${KDU_SSSE3}" = xtrue; then
			KAKADU_OBJS="$KAKADU_OBJS $kakadu_path/apps/make/ssse3_stripe_transfer.o"
		fi

		# Check for AVX2 support in Kakadu
		AC_CHECK_FILE( $kakadu_path/apps/make/avx2_stripe_transfer.o,
			AC_MSG_RESULT([configure: Kakadu has support for AVX2]);
			KDU_AVX2=true
		)
		if test "x${KDU_AVX2}" = xtrue; then
			KAKADU_OBJS="$KAKADU_OBJS $kakadu_path/apps/make/avx2_stripe_transfer.o"
		fi

		# Check for addition of kdu_client_window in versions 7.5 and upwards
		AC_CHECK_FILE( $kakadu_path/apps/make/kdu_client_window.o,
			AC_MSG_RESULT([configure: Kakadu is >= v7.5]);
			KDU_75=true,
			AC_MSG_RESULT([configure: Kakadu is < v7.5])
		)
		if test "x${KDU_75}" = xtrue; then
			KAKADU_OBJS="$KAKADU_OBJS $kakadu_path/apps/make/kdu_client_window.o"
		fi

		AC_CHECK_FILE( $kakadu_path/apps/make/supp_local.o,
			AC_MSG_RESULT([configure: Kakadu is >= v7.10]);
			KDU_7A=true,
                        AC_MSG_RESULT([configure: Kakadu is < v7.10])
                )
		if test "x${KDU_7A}" = xtrue; then
			KAKADU_OBJS="$KAKADU_OBJS $kakadu_path/apps/make/supp_local.o"
			AC_DEFINE([HAVE_KAKADUMETANODE])
		fi

		JPEG2000_CODEC="true (Kakadu)"

		AC_SUBST(KAKADU_OBJS)

		AC_LANG_PUSH([C++])
		AC_CHECK_HEADER([thread], [AC_DEFINE(HAVE_STL_THREAD)])
		AC_LANG_POP([C++])

		AC_CHECK_FUNC([get_nprocs_conf],[AC_DEFINE(NPROCS)])
	fi
else
	AM_CONDITIONAL([ENABLE_KAKADU],[false])
fi





# Check for OpenJPEG if no Kakadu support has been found

OPENJPEG=false

AC_ARG_ENABLE(openjpeg,
    [  --disable-openjpeg      disable OpenJPEG] )

AC_ARG_WITH( openjpeg,
   [  --with-openjpeg=DIR     location of OpenJPEG source files],
   openjpeg_path=$withval )


# Testing presence of pkg-config
AC_MSG_CHECKING([pkg-config m4 macros])
if test m4_ifdef([PKG_CHECK_MODULES], [yes], [no]) == yes; then
	AC_MSG_RESULT([yes]);
else
	AC_MSG_RESULT([no]);
	AC_MSG_ERROR([
		pkg-config is required.
		See pkg-config.freedesktop.org])
fi



# Only check for OpenJPEG if Kakadu has not been enabled
if [test "x${KAKADU}" = xtrue]; then
	AM_CONDITIONAL([ENABLE_OPENJPEG], [false])
else

	if test "x$enable_openjpeg" == "xno"; then
		AC_MSG_RESULT([disabling OpenJPEG support])
		AM_CONDITIONAL([ENABLE_OPENJPEG], [false])
	else

		if test -n "$openjpeg_path"; then
		    AC_CHECK_FILE($openjpeg_path/src/lib/openjp2/openjpeg.h,
		        AC_MSG_RESULT([configure: Using specified OpenJPEG sources]); OPENJPEG=true,
		        AC_MSG_RESULT([configure: No OpenJPEG sources found])
		    )
		    if test "x$OPENJPEG" = xtrue; then
		        AM_CXXFLAGS="-I$openjpeg_path/src/lib/openjp2/ ${AM_CXXFLAGS}"
		        LIBS="$LIBS $openjpeg_path/bin/libopenjp2.so"
		    fi
		else
		    PKG_CHECK_MODULES([OPENJPEG], [libopenjp2], [OPENJPEG=true], [OPENJPEG=false])
		    if test "x$OPENJPEG" = xtrue; then
		        AM_CXXFLAGS="${OPENJPEG_CFLAGS} ${AM_CXXFLAGS}"
		        LIBS="$LIBS ${OPENJPEG_LIBS}"
		    else
			AC_MSG_RESULT([configure: Unable to find OpenJPEG development files])
		    fi
		fi

		if test "x$OPENJPEG" = xtrue; then
		    AC_DEFINE(HAVE_OPENJPEG)
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
	AC_MSG_RESULT([configure: unable to find libfcgi - using bundled version])
	AC_CHECK_LIB([nsl],       [gethostbyname])
	AC_CHECK_LIB([socket],    [socket])
	FCGI_COMMON_CHECKS
	AC_REPLACE_FUNCS([strerror])
	AM_CXXFLAGS="-I../fcgi/include -I../fcgi ${AM_CXXFLAGS}"
	LIBS="../fcgi/libfcgi/libfcgi.a $LIBS"
	AM_CONDITIONAL([BUNDLED_FCGI], [true])
else
	LIBS="$LIBS -lfcgi"
	AM_CONDITIONAL([BUNDLED_FCGI], [false])
fi



# Pass flags to Makefiles
AC_SUBST(AM_CXXFLAGS)
AC_SUBST(AM_LDFLAGS)
AC_SUBST(LIBS)


# Generate Make files
AC_PROG_MAKE_SET
AC_CONFIG_FILES([Makefile \
	src/Makefile \
	man/Makefile \
	fcgi/Makefile \
	fcgi/include/Makefile \
	fcgi/libfcgi/Makefile])
AC_OUTPUT


# Print out option status
AC_MSG_RESULT([
Options Enabled:
--------------------------------
 Memcached   :  ${MEMCACHED}
 JPEG2000    :  ${JPEG2000_CODEC}
 OpenMP      :  ${OPENMP}
 Loggers     :  ${LOGGING}
 PNG Output  :  ${PNG}
 WebP Output :  ${WEBP}])

if [test "x${DEBUG}" = xtrue]; then
  AC_MSG_RESULT([ Debug mode  :  activated])
fi

AC_MSG_RESULT([--------------------------------
])
