AUTOMAKE_OPTIONS = dist-bzip2
ACLOCAL_AMFLAGS = -I m4

if BUNDLED_FCGI
  FCGI_SRC = fcgi
endif

SUBDIRS = $(FCGI_SRC) src man

EXTRA_DIST = COPYING.FCGI doc windows docker
