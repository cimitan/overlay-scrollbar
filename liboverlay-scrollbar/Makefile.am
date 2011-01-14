INCLUDES = $(GTK_CFLAGS)

EXTRA_DIST = \
	overlay-scrollbar.pc.in

liboverlay_scrollbarincludedir = $(includedir)/liboverlay-scrollbar

lib_LTLIBRARIES = liboverlay-scrollbar.la

liboverlay_scrollbar_headers = \
	overlay-scrollbar.h

liboverlay_scrollbarinclude_HEADERS = \
	$(liboverlay_scrollbar_headers)

liboverlay_scrollbar_la_SOURCES = \
	$(liboverlay_scrollbar_headers) \
	overlay-scrollbar.c \
	overlay-scrollbar-cairo-support.h \
	overlay-scrollbar-cairo-support.c \
	overlay-scrollbar-support.h \
	overlay-scrollbar-support.c

liboverlay_scrollbar_la_CFLAGS = \
	$(GTK_CFLAGS) \
	-DG_LOG_DOMAIN=\"liboverlay-scrollbar\" \
	-Wall -Werror

liboverlay_scrollbar_la_LDFLAGS = \
	-version-info 1:0:0 \
	-no-undefined \
	-export-symbols-regex "^[^_].*"

liboverlay_scrollbar_la_LIBADD = $(GTK_LIBS)

pkgconfig_DATA = overlay-scrollbar.pc
pkgconfigdir = $(libdir)/pkgconfig
