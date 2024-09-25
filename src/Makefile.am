## Process this file with automake to produce Makefile.in

noinst_PROGRAMS =	iipsrv.fcgi


iipsrv_fcgi_LDADD = Main.o

if ENABLE_KAKADU
iipsrv_fcgi_LDADD += KakaduImage.o @KAKADU_OBJS@
endif

if ENABLE_OPENJPEG
iipsrv_fcgi_LDADD += OpenJPEGImage.o
endif

if ENABLE_PNG
iipsrv_fcgi_LDADD += PNGCompressor.o
endif

if ENABLE_WEBP
iipsrv_fcgi_LDADD += WebPCompressor.o
endif

if ENABLE_AVIF
iipsrv_fcgi_LDADD += AVIFCompressor.o
endif

if ENABLE_MODULES
iipsrv_fcgi_LDADD += DSOImage.o
endif

EXTRA_iipsrv_fcgi_SOURCES = Main.cc DSOImage.h DSOImage.cc \
			KakaduImage.h KakaduImage.cc \
			OpenJPEGImage.h OpenJPEGImage.cc \
			PNGCompressor.h PNGCompressor.cc \
			WebPCompressor.h WebPCompressor.cc \
			AVIFCompressor.h AVIFCompressor.cc

iipsrv_fcgi_SOURCES = \
			IIPImage.h \
			IIPImage.cc \
			TPTImage.h \
			TPTImage.cc \
			JPEGImage.h \
			JPEGImage.cc \
			Compressor.h \
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
			URL.h \
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
			PFL.cc \
			IIIF.cc \
			Watermark.h \
			Watermark.cc \
			Logger.h \
			Memcached.h


# Rename and install/uninstall to /sbin/
install-exec-local:
	$(INSTALL_PROGRAM) -m 755 iipsrv.fcgi "$(DESTDIR)$(sbindir)/iipsrv"

uninstall-local:
	rm -f "$(DESTDIR)$(sbindir)/iipsrv"
