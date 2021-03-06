AC_PREREQ([2.60])

AC_INIT([RaveModemFactory],[1.30.9],[aleksander@aleksander.es],[RaveModemFactory])
AM_INIT_AUTOMAKE([1.11 foreign subdir-objects tar-ustar no-dist-gzip dist-xz -Wno-portability])
AM_SILENT_RULES([yes])
AM_MAINTAINER_MODE([enable])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS(config.h)

dnl Required programs
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CXX
AC_PROG_INSTALL

dnl Optional C++11 features
AX_CXX_COMPILE_STDCXX_11([noext],[optional])

dnl Initialize libtool
LT_PREREQ([2.2])
LT_INIT

dnl Specific warnings to always use
RMF_COMPILER_WARNINGS

dnl Require glib
GLIB_BUILD_SYMBOLS="-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_48 -DGLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_2_48 -DGLIB_DISABLE_DEPRECATION_WARNINGS"
PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.48 gio-2.0 gio-unix-2.0)
GLIB_CFLAGS="$GLIB_CFLAGS $GLIB_BUILD_SYMBOLS"
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

dnl Require libqmi
PKG_CHECK_MODULES(QMI, qmi-glib >= 1.26.0)
AC_SUBST(QMI_CFLAGS)
AC_SUBST(QMI_LIBS)

dnl Require gudev
PKG_CHECK_MODULES(GUDEV, gudev-1.0 >= 147)
AC_SUBST(GUDEV_CFLAGS)
AC_SUBST(GUDEV_LIBS)

# Some required utilities
GLIB_MKENUMS=`$PKG_CONFIG --variable=glib_mkenums glib-2.0`
AC_SUBST(GLIB_MKENUMS)

AC_CONFIG_FILES([Makefile
                 build-aux/Makefile
                 doc/Makefile
                 src/Makefile
                 src/librmf-common/Makefile
                 src/librmf-common/test/Makefile
                 src/librmf/Makefile
                 src/rmfcli/Makefile
                 src/rmfd/Makefile
                 src/rmfd/test/Makefile
                 test/Makefile])
AC_OUTPUT

echo "
    RaveModemFactory $VERSION
    ==============================================

    C compiler:          ${CC}
    cflags:              ${CFLAGS}
    C++ compiler:        ${CXX}
    cxxflags:            ${CXXFLAGS}
    Maintainer mode:     ${USE_MAINTAINER_MODE}
"
