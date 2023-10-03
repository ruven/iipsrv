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
