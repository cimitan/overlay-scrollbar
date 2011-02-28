# liboverlay-scrollbar

source_h = \
	os.h \
	os-scrollbar.h

source_c = \
	$(srcdir)/os-scrollbar.c

osincludedir = $(includedir)/overlay-scrollbar-@OS_MAJORMINOR@/os
osinclude_HEADERS = $(source_h)

liboverlay_scrollbar_@OS_MAJORMINOR@_la_SOURCES = $(source_c)

lib_LTLIBRARIES = liboverlay-scrollbar-@OS_MAJORMINOR@.la

liboverlay_scrollbar_@OS_MAJORMINOR@_la_CFLAGS = $(OS_CFLAGS)

liboverlay_scrollbar_@OS_MAJORMINOR@_la_LIBADD = $(OS_LIBADD)

liboverlay_scrollbar_@OS_MAJORMINOR@_la_LDFLAGS = $(OS_LDFLAGS)

liboverlay_scrollbar_@OS_MAJORMINOR@_includedir = \
	$(includedir)/overlay-scrollbar-@OS_MAJORMINOR@/os

liboverlay_scrollbar_@OS_MAJORMINOR@_include_HEADERS = \
	$(source_h) \
	os-version.h

EXTRA_DIST = \
	os-version.h.in

DISTCLEANFILES = \
	os-version.h

# pkg-config

pcfiles = overlay-scrollbar-@OS_MAJORMINOR@.pc

all-local: $(pcfiles)

%-@OS_MAJORMINOR@.pc: %.pc
	cp $< $@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(pcfiles)

EXTRA_DIST = overlay-scrollbar.pc.in

CLEANFILES = $(pcfiles)
