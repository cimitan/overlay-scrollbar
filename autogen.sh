#!/bin/sh

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

olddir=`pwd`
cd $srcdir

(autoconf --version) < /dev/null > /dev/null 2>&1 || {
  echo
  echo "You must have autoconf installed to compile liboverlay-scrollbar."
  echo "Install the appropriate package for your distribution,"
  echo "or get the source tarball at http://ftp.gnu.org/gnu/autoconf/"
  DIE=1
}

if test "$DIE" = "1"; then
  exit 1
fi

autoreconf --force --install --symlink || exit $?

cd $olddir

conf_flags=""

if test x$NOCONFIGURE = x; then
  echo Running $srcdir/configure $conf_flags "$@" ...
  $srcdir/configure $conf_flags "$@" \
  && echo Now type \`make\' to compile. || exit 1
else
  echo Skipping configure process.
fi

