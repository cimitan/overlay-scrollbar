SUBDIRS = \
	misc \
	data \
	build \
	pkgconfig \
	os

if ENABLE_TESTS
SUBDIRS += tests
endif

ACLOCAL_AMFLAGS = -I build

EXTRA_DIST = autogen.sh COPYING RELEASE

DISTCHECK_CONFIGURE_FLAGS = --disable-scrollkeeper

MAINTAINERCLEANFILES = \
	build/compile \
	build/config.guess \
	build/config.sub \
	build/depcomp \
	build/install-sh \
	build/ltmain.sh \
	build/missing \
	Makefile.in \
	config.h.in \
	configure \
	aclocal.m4

release: dist
	make $(PACKAGE)-$(VERSION).tar.gz.md5
	make $(PACKAGE)-$(VERSION).tar.bz2.md5

%.md5: %
	md5sum $< > $@
