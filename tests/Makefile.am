INCLUDES = \
	-I$(top_builddir)/liboverlay-scrollbar \
	$(GTK_CFLAGS)

DEPS = \
	$(top_builddir)/liboverlay-scrollbar/liboverlay-scrollbar.la

LDADDS = \
	$(top_builddir)/liboverlay-scrollbar/liboverlay-scrollbar.la \
	$(GTK_LIBS)

noinst_PROGRAMS = \
	test-overlay-scrollbar-window

test_overlay_scrollbar_window_DEPENDENCIES = $(DEPS)

test_overlay_scrollbar_window_LDADD = $(LDADDS)

test_overlay_scrollbar_window_SOURCES = test-overlay-scrollbar-window.c

#EXTRA_DIST +=
