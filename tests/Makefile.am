noinst_PROGRAMS = \
	test-os

test_os_CFLAGS = \
	-I$(top_srcdir) \
	$(OS_CFLAGS)
test_os_LDFLAGS = \
	$(top_builddir)/os/liboverlay-scrollbar-@OS_MAJORMINOR@.la \
	$(OS_LIBADD)

# FIXME(loicm): Figure out what tests must be compiled and add them here.
