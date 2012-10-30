## Process this file with automake to produce Makefile.in

noinst_PROGRAMS =	iipsrv.fcgi


INCLUDES =		@INCLUDES@ @LIBFCGI_INCLUDES@ @JPEG_INCLUDES@ @TIFF_INCLUDES@
LIBS =			@LIBS@ @LIBFCGI_LIBS@ @DL_LIBS@ @JPEG_LIBS@ @TIFF_LIBS@ -lz -lm
AM_LDFLAGS =		@LIBFCGI_LDFLAGS@

iipsrv_fcgi_LDADD = Main.o

if ENABLE_KAKADU
iipsrv_fcgi_LDADD += KakaduImage.o
endif

if ENABLE_MODULES
iipsrv_fcgi_LDADD += DSOImage.o
endif

EXTRA_iipsrv_fcgi_SOURCES = DSOImage.h DSOImage.cc KakaduImage.h KakaduImage.cc Memcached.h Main.cc

iipsrv_fcgi_SOURCES = \
			IIPImage.h \
			IIPImage.cc \
			TPTImage.h \
			TPTImage.cc \
			JPEGCompressor.h \
			JPEGCompressor.cc \
			RawTile.h \
			Timer.h \
			Cache.h \
			TileManager.h \
			TileManager.cc \
			Tokenizer.h \
			IIPResponse.h \
			IIPResponse.cc \
			View.h \
			View.cc \
			Transforms.h \
			Transforms.cc \
			Environment.h \
			Writer.h \
			Task.h \
			Task.cc \
			OBJ.cc \
			FIF.cc \
			JTL.cc \
			TIL.cc \
			ICC.cc \
			CVT.cc \
			Zoomify.cc \
			DeepZoom.cc \
			SPECTRA.cc \
			Watermark.h \
			Watermark.cc

