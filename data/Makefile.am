EXTRA_DIST = 81overlay-scrollbar.in

CLEANFILES = 81overlay-scrollbar

81overlay-scrollbar: 81overlay-scrollbar.in
	sed -e "s|\@libdir\@|$(libdir)|" -e "s|\@OS_MAJORMINOR\@|@OS_MAJORMINOR@|" -e "s|\@OS_LT_CURRENT\@|$(OS_LT_CURRENT)|" $< > $@

xsessiondir = $(sysconfdir)/X11/Xsession.d

nodist_xsession_DATA = 81overlay-scrollbar
