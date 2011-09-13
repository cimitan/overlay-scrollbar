if USE_GTK3
VER=3
lib_LTLIBRARIES = liboverlay-scrollbar3-@OS_MAJORMINOR@.la
else
VER=
lib_LTLIBRARIES = liboverlay-scrollbar-@OS_MAJORMINOR@.la
endif

noinst_HEADERS = \
	os-private.h

source_h = \
	os.h \
	os-utils.h \
	os-scrollbar.h

source_c = \
	$(srcdir)/os-log.c \
	$(srcdir)/os-utils.c \
	$(srcdir)/os-animation.c \
	$(srcdir)/os-pager.c \
	$(srcdir)/os-thumb.c \
	$(srcdir)/os-scrollbar.c

osincludedir = $(includedir)/overlay-scrollbar$(VER)-@OS_MAJORMINOR@/os
osinclude_HEADERS = $(source_h)

liboverlay_scrollbar_@OS_MAJORMINOR@_la_SOURCES = $(source_c)

liboverlay_scrollbar_@OS_MAJORMINOR@_la_CFLAGS = $(OS_CFLAGS)

liboverlay_scrollbar_@OS_MAJORMINOR@_la_LIBADD = $(OS_LIBADD)

liboverlay_scrollbar_@OS_MAJORMINOR@_la_LDFLAGS = $(OS_LDFLAGS)

liboverlay_scrollbar_@OS_MAJORMINOR@_includedir = \
	$(includedir)/overlay-scrollbar$(VER)-@OS_MAJORMINOR@/os

liboverlay_scrollbar_@OS_MAJORMINOR@_include_HEADERS = \
	$(source_h) \
	os-version.h

# We duplicate these here because Automake won't let us use $(VER) on the left hand side.
# Since we carefully use $(VER) in the right hand side above, we can assign the same values.
# Only one version of the library is every compiled at the same time, so it is safe to reuse
# the right hand sides like this.
os3includedir = $(osincludedir)
os3include_HEADERS = $(osinclude_HEADERS)
liboverlay_scrollbar3_@OS_MAJORMINOR@_la_SOURCES =  $(source_c)
liboverlay_scrollbar3_@OS_MAJORMINOR@_la_CFLAGS = $(OS_CFLAGS) \
  -DUSE_GTK3
liboverlay_scrollbar3_@OS_MAJORMINOR@_la_LIBADD = $(OS_LIBADD)
liboverlay_scrollbar3_@OS_MAJORMINOR@_la_LDFLAGS = $(OS_LDFLAGS)
liboverlay_scrollbar3_@OS_MAJORMINOR@_includedir = $(includedir)/overlay-scrollbar$(VER)-@OS_MAJORMINOR@/os
liboverlay_scrollbar3_@OS_MAJORMINOR@_include_HEADERS = \
	$(source_h) \
	os-version.h

EXTRA_DIST = \
	os-version.h.in

DISTCLEANFILES = \
	os-version.h
