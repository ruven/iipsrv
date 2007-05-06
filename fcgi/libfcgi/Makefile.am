# $Id: Makefile.am,v 1.9 2001/12/22 03:16:20 robs Exp $

INCLUDEDIR  = ../include
AM_CPPFLAGS    = @CPPFLAGS@ -I$(top_srcdir)/fcgi/include -I$(top_srcdir)/fcgi

INCLUDE_FILES = $(INCLUDEDIR)/fastcgi.h     \
                $(INCLUDEDIR)/fcgiapp.h     \
                $(INCLUDEDIR)/fcgimisc.h    \
                $(INCLUDEDIR)/fcgi_stdio.h  \
                $(INCLUDEDIR)/fcgios.h

#lib_LTLIBRARIES   = libfcgi.la @LIBFCGIXX@
#EXTRA_LTLIBRARIES = libfcgi++.la

noinst_LIBRARIES   = libfcgi.a

libfcgi_a_SOURCES = $(INCLUDE_FILES)  \
                     fcgiapp.c         \
                     fcgi_stdio.c      \
                     os_@SYSTEM@.c
libfcgi_a_CC      = @PTHREAD_CC@
libfcgi_a_CFLAGS  = @PTHREAD_CFLAGS@

#libfcgi___a_SOURCES = $(INCLUDE_FILES)       \
                       $(INCLUDEDIR)/fcgio.h  \
                       fcgio.cpp
#libfcgi___a_CFLAGS  = @PTHREAD_CFLAGS@
#libfcgi___a_LDFLAGS = -lfcgi -rpath @libdir@

