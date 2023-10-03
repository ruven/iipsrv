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
