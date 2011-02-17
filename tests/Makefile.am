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

noinst_PROGRAMS = \
	overlay-scrollbar-test-pager
	overlay-scrollbar-test-pixmap

overlay_scrollbar_test_DEPENDENCIES = $(DEPS)

overlay_scrollbar_test_LDADD = $(LDADDS)

overlay_scrollbar_test_SOURCES = overlay-scrollbar-test.c

overlay_scrollbar_test_pager_DEPENDENCIES = $(DEPS)

overlay_scrollbar_test_pager_LDADD = $(LDADDS)

overlay_scrollbar_test_pager_SOURCES = overlay-scrollbar-test-pager.c

overlay_scrollbar_test_pixmap_DEPENDENCIES = $(DEPS)

overlay_scrollbar_test_pixmap_LDADD = $(LDADDS)

overlay_scrollbar_test_pixmap_SOURCES = overlay-scrollbar-test-pixmap.c

#EXTRA_DIST +=
