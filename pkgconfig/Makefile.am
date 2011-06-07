if USE_GTK3
VER=3
else
VER=
endif

pcfiles = overlay-scrollbar$(VER)-@OS_MAJORMINOR@.pc

all-local: $(pcfiles)

%$(VER)-@OS_MAJORMINOR@.pc: %.pc
	cp $< $@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(pcfiles)

EXTRA_DIST = overlay-scrollbar$(VER).pc.in

CLEANFILES = $(pcfiles)
