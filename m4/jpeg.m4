dnl
dnl AX_CHECK_LIBJPEG
dnl
dnl Check for libjpeg headers and libraries. Test compilation and linkage
dnl

AC_DEFUN([AX_CHECK_LIBJPEG], [

	AC_CHECK_HEADER([jpeglib.h],
			[AC_CHECK_LIB(
				[jpeg],
				[jpeg_abort],
				[],
				[AC_MSG_ERROR([libjpeg not found])]
		)],
                [AC_MSG_ERROR([No libjpeg headers found])]
	)

	# Make sure we can compile
	AC_MSG_CHECKING([whether libjpeg can be compiled])
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
				#include <stdio.h>
				#include <jpeglib.h>]],
				[[int a]])],
			[AC_MSG_RESULT([yes])],
			[AC_MSG_ERROR([unable to compile])]
	)

	AC_MSG_CHECKING([whether libjpeg can be linked])
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
				#include <stdio.h>
				#include <jpeglib.h>]],
				[[jpeg_abort((void*)0)]])],
			[AC_MSG_RESULT([yes])],
			[AC_MSG_ERROR([unable to link])]
	)

])
