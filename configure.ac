AC_PREREQ(2.63)

# Package

AC_INIT([overlay-scrollbar],[0.2.11],[https://bugs.launchpad.net/ayatana-scrollbar],[overlay-scrollbar])
AC_CONFIG_SRCDIR([os/os-scrollbar.h])
AC_CONFIG_MACRO_DIR([build])
AC_CONFIG_AUX_DIR([build])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([1.9 foreign dist-bzip2])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# ABI versionning

# CURRENT, REVISION, AGE
# - library source changed -> increment REVISION
# - interfaces added/removed/changed -> increment CURRENT, REVISION = 0
# - interfaces added -> increment AGE
# - interfaces removed -> AGE = 0
OS_LT_CURRENT=0
OS_LT_REV=11
OS_LT_AGE=0
OS_LT_VERSION="$OS_LT_CURRENT:$OS_LT_REV:$OS_LT_AGE"
OS_LT_LDFLAGS="-version-info $OS_LT_VERSION"
AC_SUBST(OS_LT_CURRENT)
AC_SUBST(OS_LT_VERSION)
AC_SUBST(OS_LT_LDFLAGS)

# API versionning

PACKAGE_VERSION_MAJOR=`echo AC_PACKAGE_VERSION | cut -d'.' -f1`
PACKAGE_VERSION_MINOR=`echo AC_PACKAGE_VERSION | cut -d'.' -f2`
PACKAGE_VERSION_MICRO=`echo AC_PACKAGE_VERSION | cut -d'.' -f3`
AC_SUBST(PACKAGE_VERSION_MAJOR)
AC_SUBST(PACKAGE_VERSION_MINOR)
AC_SUBST(PACKAGE_VERSION_MICRO)

NANO=`echo AC_PACKAGE_VERSION | cut -d'.' -f4`
if test x"$NANO" = x || test "x$NANO" = "x0" ; then
    OS_TRUNK="no"
    PACKAGE_VERSION_NANO="0"
else
    OS_TRUNK="yes"
    PACKAGE_VERSION_NANO="$NANO"
fi
AC_SUBST(OS_TRUNK)
AC_SUBST(PACKAGE_VERSION_NANO)

OS_MAJORMINOR=$PACKAGE_VERSION_MAJOR.$PACKAGE_VERSION_MINOR
AC_SUBST(OS_MAJORMINOR)
AC_DEFINE_UNQUOTED(OS_MAJORMINOR, "OS_MAJORMINOR",
                   [library major.minor version])

AC_USE_SYSTEM_EXTENSIONS
AC_PROG_CC
AM_PROG_CC_C_O
LT_INIT
AC_C_CONST
AC_C_INLINE

# Build dependencies

AC_PATH_PROG([GLIB_MKENUMS], [glib-mkenums])
AC_PATH_PROG([GLIB_GENMARSHAL], [glib-genmarshal])
PKG_PROG_PKG_CONFIG

AC_SUBST(glib_req, 2.26.0)
AC_SUBST(gtk_req, 2.22.0)
AC_SUBST(gtk3_req, 3.0.9)
AC_SUBST(cairo_req, 1.10)

AC_ARG_WITH([gtk],
  [AS_HELP_STRING([--with-gtk],
    [Which version of gtk to use @<:@default=3@:>@])],
  [],
  [with_gtk=3])
AS_IF([test "x$with_gtk" = x3],
        [PKG_CHECK_MODULES(DEPS, glib-2.0 >= $glib_req gtk+-3.0 >= $gtk3_req cairo >= $cairo_req)
         AC_SUBST(DEPS_CFLAGS)
         AC_SUBST(DEPS_LIBS)
        ],
      [test "x$with_gtk" = x2],
        [PKG_CHECK_MODULES(DEPS, glib-2.0 >= $glib_req gtk+-2.0 >= $gtk_req cairo >= $cairo_req)
         AC_SUBST(DEPS_CFLAGS)
         AC_SUBST(DEPS_LIBS)
        ],
      [AC_MSG_FAILURE([Value for --with-gtk was neither 2 nor 3])]
)
AM_CONDITIONAL(USE_GTK3, [test "x$with_gtk" = x3])

# Debug flags

if test x$OS_TRUNK = xyes; then
    DEFAULT_DEBUG="yes"
else
    DEFAULT_DEBUG="no"
fi
AC_SUBST(DEFAULT_DEBUG)
AC_ARG_ENABLE([debug],
              [AS_HELP_STRING([--enable-debug=@<:@no/yes/full@:>@],[Control debug level @<:@default=debug_default@:>@])],
              [], [enable_debug=$DEFAULT_DEBUG])
AS_CASE([$enable_debug],
        [yes],
        [
          test "$cflags_set" = set || CFLAGS="$CFLAGS -g"
          DEBUG_CFLAGS="-D_DEBUG"
        ],
        [no],
        [DEBUG_CFLAGS="-DNDEBUG"],
        [AC_MSG_ERROR([Unknown argument for --enable-debug])]
)
AC_SUBST(DEBUG_CFLAGS)

# Maintainer flags

if test x$OS_TRUNK = xyes; then
    DEFAULT_MAINTAINER_FLAGS="yes"
else
    DEFAULT_MAINTAINER_FLAGS="no"
fi
AC_SUBST(DEFAULT_MAINTAINER_FLAGS)
AC_ARG_ENABLE([maintainer-flags],
              [AS_HELP_STRING([--enable-maintainer-flags=@<:@no/yes@:>@],[Use strict compiler flags @<:@default=maintainer_flags_default@:>@])],
              [],
              [enable_maintainer_flags=$DEFAULT_MAINTAINER_FLAGS])
AS_IF([test "x$enable_maintainer_flags" = "xyes" && test "x$GCC" = "xyes"],
      [
        AS_COMPILER_FLAGS([MAINTAINER_CFLAGS],
                          ["-fno-common -Wall -Wextra -Werror -Wempty-body
                            -Wno-missing-field-initializers -Wstrict-aliasing=2
                            -Wno-unused-parameter -Wdeclaration-after-statement
                            -Wpointer-arith -Wcast-align -Wwrite-strings
                            -Wformat-security -Wformat-nonliteral -Winit-self
                            -Wmissing-declarations -Wnested-externs -Wundef
                            -Wold-style-definition -Wswitch-default
                            -Wredundant-decls"])
      ]
)
AC_SUBST(MAINTAINER_CFLAGS)

# Variables

OS_LIBADD="\$(DEPS_LIBS)"
AC_SUBST(OS_LIBADD)

OS_CFLAGS="-I\$(top_srcdir) -DOS_COMPILATION \$(DEPS_CFLAGS) \$(DEBUG_CFLAGS) \$(MAINTAINER_CFLAGS)"
AC_SUBST(OS_CFLAGS)

OS_LDFLAGS="\$(OS_LT_LDFLAGS) -export-symbols-regex \^[_]*\(os_\|Os\|OS_\).* -no-undefined"
AC_SUBST(OS_LDFLAGS)

# Files

AC_CONFIG_FILES([
  Makefile
  misc/Makefile
  data/Makefile
  build/Makefile
  pkgconfig/Makefile
  pkgconfig/overlay-scrollbar.pc
  os/Makefile
  os/os-version.h
  tests/Makefile
])
AC_OUTPUT

# Summary

echo ""
echo "  overlay-scrollbar $PACKAGE_VERSION"
echo "  ========================="
echo ""
echo "  Gtk+:   ${with_gtk}"
echo "  Debug:  ${enable_debug}"
echo "  Prefix: ${prefix}"
echo ""
echo "  Compiler flags: ${CPPFLAGS} ${DEBUG_CFLAGS} ${MAINTAINER_CFLAGS}"
echo ""
