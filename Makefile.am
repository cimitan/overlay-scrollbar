SUBDIRS = \
	misc \
	build \
	pkgconfig \
	os \
	tests

ACLOCAL_AMFLAGS = -I build

EXTRA_DIST = autogen.sh COPYING RELEASE 81overlay-scrollbar.in

CLEANFILES = 81overlay-scrollbar

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

81overlay-scrollbar: 81overlay-scrollbar.in
	sed -e "s|\@libdir\@|$(libdir)|" -e "s|\@OS_MAJORMINOR\@|@OS_MAJORMINOR@|" -e "s|\@OS_LT_CURRENT\@|$(OS_LT_CURRENT)|" $< > $@

xsessiondir = $(sysconfdir)/X11/Xsession.d

nodist_xsession_DATA = 81overlay-scrollbar

release: dist
	make $(PACKAGE)-$(VERSION).tar.gz.md5
	make $(PACKAGE)-$(VERSION).tar.bz2.md5

%.md5: %
	md5sum $< > $@
