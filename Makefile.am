
SUBDIRS = . build-aux doc src test

ACLOCAL_AMFLAGS = -I m4

DISTCHECK_CONFIGURE_FLAGS = \
	--enable-more-warnings=error     \
	CFLAGS="-O2 -D_FORTIFY_SOURCE=2"

EXTRA_DIST = autogen.sh gtester.make

DISTCLEANFILES = ChangeLog
doc_DATA       = ChangeLog

# Build changelog from git history
.PHONY: ChangeLog
ChangeLog:
	$(AM_V_GEN)if test -d $(top_srcdir)/.git; then \
		prev=$$(git describe --tags --always --match '[0-9]*' 2> /dev/null) ; \
		for tag in $$(git log --decorate=full --simplify-by-decoration --pretty=oneline HEAD | sed -r -e 's#^[^\(]*\(([^\)]*)\).*$$#\1#' -e 's#,#\n#g' | grep 'tag:' | sed -r -e 's#[[:space:]]*tag:[[:space:]]*##'); do \
			if [ x"$$prev" = x ]; then prev=$$tag ; fi ; \
			if [ x"$$prev" = x"$$tag" ]; then continue; fi ; \
			echo "$$prev [$$(git log $$prev -1 --pretty=format:'%ai')]:" ; \
			echo "" ; \
			git log --pretty=' - [%h] %s' $$tag..$$prev ; \
			echo "" ; \
			prev=$$tag ; \
		done > $@ ; \
	else \
		touch $@ ; \
	fi
