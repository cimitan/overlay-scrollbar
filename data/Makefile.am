EXTRA_DIST = 
gsettings_ENUM_NAMESPACE = com.canonical.desktop.interface
gsettings_ENUM_FILES = $(top_srcdir)/os/os-scrollbar.h
gsettings_SCHEMAS = com.canonical.desktop.interface.gschema.xml

@GSETTINGS_RULES@

EXTRA_DIST += $(gsettings_SCHEMAS)

xsessiondir = $(sysconfdir)/X11/Xsession.d

xsession_DATA = 81overlay-scrollbar

EXTRA_DIST += $(xsession_DATA)
