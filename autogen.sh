#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.
REQUIRED_AUTOMAKE_VERSION=1.11
PKG_NAME=RaveModemFactory

(test -f $srcdir/configure.ac) || {
    echo -n "**Error**: Directory "\`$srcdir\'" does not look like the"
    echo " top-level $PKG_NAME directory"
    exit 1
}

(cd $srcdir;
    mkdir -p m4
    touch ChangeLog
    autoreconf --force --install --verbose
    if test -z "$NOCONFIGURE"; then
        ./configure --enable-maintainer-mode $@
    fi
)
