
SUBDIRS = . build-aux doc src test

ACLOCAL_AMFLAGS = -I m4

DISTCHECK_CONFIGURE_FLAGS = \
	--enable-more-warnings=error     \
	CFLAGS="-O2 -D_FORTIFY_SOURCE=2"

EXTRA_DIST = autogen.sh gtester.make
