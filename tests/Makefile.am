if USE_GTK3
VER=3
else
VER=
endif

noinst_PROGRAMS = \
	test-os

test_os_CFLAGS = \
	-I$(top_srcdir) \
	$(OS_CFLAGS)
test_os_LDFLAGS = \
	$(OS_LIBADD)
