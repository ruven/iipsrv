dnl
dnl AX_CHECK_LIBTIFF
dnl
dnl Check for libtiff headers and libraries. Test compilation and linkage.
dnl

AC_DEFUN([AX_CHECK_LIBTIFF], [

	AC_LANG_PUSH([C])

	AC_CHECK_HEADER([tiffio.h],
			[AC_CHECK_LIB(
				[tiff],
				[TIFFOpen],
				[LIBS="-ltiff $LIBS"],
				[AC_MSG_ERROR([libtiff not found])]
		)],
                [AC_MSG_ERROR([No libtiff headers found])]
	)

	# Make sure we can compile
	AC_MSG_CHECKING([whether libtiff can be compiled])
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
				[[#include <tiffio.h>]],
				[[int a]])],
			[AC_MSG_RESULT([yes])],
			[AC_MSG_ERROR([unable to compile libtiff])]
	)

	AC_MSG_CHECKING([whether libtiff can be linked])
	AC_LINK_IFELSE([AC_LANG_PROGRAM([[
				#include <tiffio.h>]],
				[[TIFFGetVersion();]])],
			[AC_MSG_RESULT([yes])],
			[AC_MSG_ERROR([unable to link libtiff])]
	)

	AC_LANG_POP([C])

])
