AC_INIT([liboverlay-scrollbar], [0.0.1])
AC_CONFIG_SRCDIR([README])

AM_INIT_AUTOMAKE([1.9 no-dist-gzip dist-bzip2 -Wall])

# Use AM_SILENT_RULES if present
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl Initialize maintainer mode
AM_MAINTAINER_MODE

AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AM_PROG_CC_C_O
AM_PATH_GLIB_2_0

dnl Initialize libtool
AM_DISABLE_STATIC
AC_LIBTOOL_WIN32_DLL
AM_PROG_LIBTOOL

# Don't forget to update the README when changing this.
PKG_CHECK_MODULES(GTK, gtk+-2.0 >= 2.22.0, , AC_MSG_ERROR([GTK+-2.0 is required to compile liboverlay-scrollbar]))

AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)

GTK_VERSION=`$PKG_CONFIG --variable=gtk_binary_version gtk+-2.0`
AC_SUBST(GTK_VERSION)

AM_CONFIG_HEADER([config.h])

AC_CONFIG_FILES([
Makefile
liboverlay-scrollbar/Makefile
liboverlay-scrollbar/overlay-scrollbar.pc
tests/Makefile
])

AC_OUTPUT

echo "
        liboverlay-scrollbar $VERSION
        ====================================

        prefix:                   ${prefix}
        compiler:                 ${CC}
        cflags:                   ${CFLAGS}
"
