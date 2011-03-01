pcfiles = overlay-scrollbar-@OS_MAJORMINOR@.pc

all-local: $(pcfiles)

%-@OS_MAJORMINOR@.pc: %.pc
	cp $< $@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(pcfiles)

EXTRA_DIST = overlay-scrollbar.pc.in

CLEANFILES = $(pcfiles)
