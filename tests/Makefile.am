if USE_GTK3
VER=3
else
VER=
endif

EXTRA_DIST = \
	test-os-pager.c \
	test-os-pixmap.c \
	test-os-treeview.c \
	test-os-wm-state.c

noinst_PROGRAMS = \
	test-os

test_os_CFLAGS = \
	-I$(top_srcdir) \
	$(OS_CFLAGS)
test_os_LDFLAGS = \
	$(top_builddir)/os/liboverlay-scrollbar$(VER)-@OS_MAJORMINOR@.la \
	$(OS_LIBADD)

# FIXME(loicm): Figure out what tests must be compiled and add them here.
