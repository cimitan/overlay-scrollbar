INCLUDES = \
	-I$(top_builddir)/liboverlay-scrollbar \
	$(GTK_CFLAGS)

DEPS = \
	$(top_builddir)/liboverlay-scrollbar/liboverlay-scrollbar.la

LDADDS = \
	$(top_builddir)/liboverlay-scrollbar/liboverlay-scrollbar.la \
	$(GTK_LIBS)

bin_PROGRAMS = \
	overlay-scrollbar-test

overlay_scrollbar_test_DEPENDENCIES = $(DEPS)

overlay_scrollbar_test_LDADD = $(LDADDS)

overlay_scrollbar_test_SOURCES = overlay-scrollbar-test.c

#EXTRA_DIST +=
