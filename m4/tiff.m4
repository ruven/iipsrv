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

	# GPS directory support was added in libtiff 4.2.  It is optional: older
	# libtiff versions still build with EXIF support, but without GPS.
	AC_MSG_CHECKING([whether libtiff has GPS directory support])
	AC_LINK_IFELSE([AC_LANG_PROGRAM([[
				#include <tiffio.h>]],
				[[TIFF* t = 0;
				  TIFFReadGPSDirectory(t, 0);
				  TIFFCreateGPSDirectory(t);]])],
			[AC_MSG_RESULT([yes])
			 AC_DEFINE([HAVE_TIFF_GPS], [1], [Define if libtiff has GPS directory support])],
			[AC_MSG_RESULT([no])]
	)

	AC_LANG_POP([C])

])
