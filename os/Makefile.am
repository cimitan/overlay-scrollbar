VER=

source_h = \
	$(srcdir)/os-private.h \
	$(srcdir)/os-scrollbar.h

source_c = \
	$(srcdir)/os-animation.c \
	$(srcdir)/os-bar.c \
	$(srcdir)/os-log.c \
	$(srcdir)/os-scrollbar.c \
	$(srcdir)/os-thumb.c

liboverlay_scrollbar_LTLIBRARIES = liboverlay-scrollbar.la

liboverlay_scrollbardir = $(GTK_MODULES_DIR)

liboverlay_scrollbar_la_SOURCES = $(source_h) $(source_c)

liboverlay_scrollbar_la_CFLAGS = $(OS_CFLAGS)

liboverlay_scrollbar_la_LIBADD = $(OS_LIBADD) -lm

liboverlay_scrollbar_la_LDFLAGS = $(OS_LDFLAGS)
