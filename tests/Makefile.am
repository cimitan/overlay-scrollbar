if USE_GTK3
VER=3
else
VER=
endif

noinst_PROGRAMS = \
	test-os

if USE_GTK3
test_os_CFLAGS = -I$(top_srcdir) $(OS_CFLAGS) -DUSE_GTK3
else
test_os_CFLAGS = -I$(top_srcdir) $(OS_CFLAGS)
endif

test_os_LDFLAGS = $(OS_LIBADD)
