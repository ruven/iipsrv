# Process this file with autoconf to produce a configure script.

AC_INIT( [iipsrv], [1.2], [ruven@users.sourceforge.net] )
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE


AC_PREFIX_DEFAULT( `pwd` )


# Checks for programs.
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AM_PROG_CC_C_O

AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_SIZE_T

LT_INIT


# Checks for header files.
AC_CHECK_HEADERS(glob.h)
AC_CHECK_HEADERS(time.h)
AC_CHECK_HEADERS(sys/time.h)
AC_CHECK_HEADERS(syslog.h, [LOGGING="file, syslog"], [LOGGING="file"])
AC_FUNC_MALLOC
AC_CHECK_LIB(m, log2, AC_DEFINE(HAVE_LOG2))
AC_CHECK_FUNCS([setenv])

AC_LANG_SAVE
AC_LANG_CPLUSPLUS
AC_CHECK_HEADERS(ext/pool_allocator.h)
AC_LANG_RESTORE


# For our windows build
AC_CHECK_HEADERS(windows.h)

# we want largefile support, if possible
AC_SYS_LARGEFILE

# Check for C99 isfinite function
AX_CXX_HAVE_ISFINITE

# Check for OpenMP
AC_OPENMP
OPENMP=false
AS_IF([test "x$enable_openmp" != "xyes"], [
 INCLUDES="$INCLUDES $OPENMP_CFLAGS"
 LIBS="$LIBS $OPENMP_CFLAGS"
 OPENMP=true
])

# Check for C++ chrono timers and availability of various hash map implementations
AC_LANG_PUSH([C++])
AC_CHECK_HEADER([chrono], [AC_DEFINE(HAVE_STL_CHRONO)])
AC_CHECK_HEADER([unordered_map], [AC_DEFINE(HAVE_UNORDERED_MAP)])
AC_CHECK_HEADER([tr1/unordered_map], [AC_DEFINE(HAVE_TR1_UNORDERED_MAP)])
AC_CHECK_HEADER([ext/hash_map], [AC_DEFINE(HAVE_EXT_HASH_MAP)])
AC_LANG_POP([C++])


#************************************************************
# Enable debugging

AC_ARG_ENABLE([debug-mode],
              [  --enable-debug-mode     enable debugging mode (no FCGI)],
              [AC_DEFINE(DEBUG)
	       AC_DEFINE(KAKADU_DEBUG)
               AC_DEFINE(OPENJPEG_DEBUG)
               DEBUG=true
	       AC_MSG_RESULT([configure: enabling debug mode])]
)



#************************************************************
# Check for libjpeg

FIND_JPEG(,[AC_MSG_ERROR([libjpeg not found])])



#************************************************************
# Check for a standard libz

AC_SEARCH_LIBS(gzopen, z)



#************************************************************
# Check for libmemcached

AC_CHECK_HEADER( libmemcached/memcached.h,
	AC_SEARCH_LIBS( memcached_create,
		memcached,
		MEMCACHED=true,
		MEMCACHED=false,
		-lpthread ),
	MEMCACHED=false
)
if test "x${MEMCACHED}" = xtrue; then
	AC_DEFINE(HAVE_MEMCACHED)
	LIBS="$LIBS -lpthread"
fi


#************************************************************
# Check for libtiff

FIND_TIFF(,[AC_MSG_ERROR([libtiff not found])])


#************************************************************
# Check for little cms library

#AC_CHECK_HEADERS(lcms2.h,
#	AC_SEARCH_LIBS( cmsOpenProfileFromMem, lcms2,
#		LCMS=true; AC_DEFINE(LCMS),
#		LCMS=false )
#)
#LCMS=false


#************************************************************
# Check for user specified locations for fast cgi library

AC_ARG_WITH( fcgi-incl,
   [  --with-fcgi-incl=DIR          location of the fcgi include files],
   [LIBFCGI_INCLUDES="-I$withval"]
)

AC_ARG_WITH( fcgi-lib,
   [  --with-fcgi-lib=DIR           location of the fcgi libraries],
   [LIBFCGI_LDFLAGS="-L$withval"]
)

AC_CHECK_LIB( fcgi, FCGI_Accept,
		AC_MSG_RESULT([configure: using system libfcgi]);
			LIBFCGI_LIBS="-lfcgi";
			AM_CONDITIONAL([BUNDLED_FCGI], [false]),
		AC_MSG_RESULT([configure: unable to find system libfcgi - using bundled version]);
			LIBFCGI_INCLUDES="-I../fcgi/include -I../fcgi";
			LIBFCGI_LIBS="../fcgi/libfcgi/libfcgi.a";
			AM_CONDITIONAL([BUNDLED_FCGI], [true])
)

AC_SUBST(LIBFCGI_INCLUDES)
AC_SUBST(LIBFCGI_LDFLAGS)
AC_SUBST(LIBFCGI_LIBS)



#************************************************************
#     Check for PNG support
#************************************************************

PNG=false
AC_ARG_ENABLE( png,
    [  --disable-png           disable PNG])


if test "x$enable_png" == "xno"; then
   AC_MSG_RESULT([disabling PNG support])
   AM_CONDITIONAL([ENABLE_PNG], [false])
else
   AC_CHECK_HEADER( png.h,
     AC_SEARCH_LIBS(
       png_create_write_struct,
       png,
       PNG=true,
       PNG=false )
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
   AC_MSG_RESULT([disabling WebP support])
   AM_CONDITIONAL([ENABLE_WEBP], [false])
else
   AC_CHECK_HEADER( webp/encode.h, [WEBP=true], [WEBP=false] )
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
   [  --with-kakadu=DIR             location of Kakadu source files (Kakadu will disable OpenJPEG support) ],
   kakadu_path=$withval)

if test -n "$kakadu_path"; then

	AC_CHECK_FILE( $kakadu_path/managed/all_includes/jpx.h,
		AC_MSG_RESULT([configure: Found Kakadu sources. Will compile JPEG2000 support]); KAKADU=true;,
		AC_MSG_RESULT([configure: No Kakadu JPEG2000 Sources Found]); KAKADU=false
	)
	AM_CONDITIONAL([ENABLE_KAKADU],[test x$KAKADU = xtrue])

	if test "x${KAKADU}" = xtrue; then
		AC_DEFINE(HAVE_KAKADU)
		INCLUDES="$INCLUDES -I$kakadu_path/managed/all_includes/"
		KAKADU_OBJS="$kakadu_path/apps/make/jpx.o $kakadu_path/apps/make/jp2.o $kakadu_path/apps/make/kdu_stripe_decompressor.o"
		kakadu_lib_name=`basename $kakadu_path/apps/make/*.so | sed 's/^lib//' | sed 's/.so$//'`
		LIBS="$LIBS -L$kakadu_path/apps/make -l$kakadu_lib_name -lpthread"

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

		INCLUDES="$INCLUDES -I."
		AC_SUBST(INCLUDES)
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
   [  --with-openjpeg=DIR           location of OpenJPEG source files],
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
		        INCLUDES="$INCLUDES -I$openjpeg_path/src/lib/openjp2/"
		        LIBS="$LIBS $openjpeg_path/bin/libopenjp2.so -lpthread"
		    fi
		else
		    PKG_CHECK_MODULES([OPENJPEG], [libopenjp2], [OPENJPEG=true], [OPENJPEG=false])
		    if test "x$OPENJPEG" = xtrue; then
		        INCLUDES="$INCLUDES $OPENJPEG_CFLAGS"
		        LIBS="$LIBS $OPENJPEG_LIBS"
		    else
			AC_MSG_RESULT([configure: Unable to find OpenJPEG development files])
		    fi
		fi

		if test "x$OPENJPEG" = xtrue; then
		    AC_DEFINE(HAVE_OPENJPEG)
		    JPEG2000_CODEC="true (OpenJPEG)"
		    AM_CONDITIONAL([ENABLE_OPENJPEG], [test x$OPENJPEG = xtrue])
		    AC_SUBST(INCLUDES)
		    AC_SUBST(LIBS)
		else
		    AM_CONDITIONAL([ENABLE_OPENJPEG], [false])
		fi

	fi
fi




#************************************************************
#     FCGI library configure
#************************************************************

AC_CHECK_LIB([nsl],       [gethostbyname])
AC_CHECK_LIB([socket],    [socket])
ACX_PTHREAD([THREADED=threaded${EXEEXT}])
AC_SUBST([THREADED])
FCGI_COMMON_CHECKS
AC_REPLACE_FUNCS([strerror])



SYSTEM=unix
AC_SUBST([SYSTEM])

AC_SUBST(LIBS)

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
---------------
 Memcached   :  ${MEMCACHED}
 JPEG2000    :  ${JPEG2000_CODEC}
 OpenMP      :  ${OPENMP}
 Loggers     :  ${LOGGING}
 PNG Output  :  ${PNG}
 WebP Output :  ${WEBP}])

if [test "x${DEBUG}" = xtrue]; then
  AC_MSG_RESULT([ Debug mode  :  activated])
fi
