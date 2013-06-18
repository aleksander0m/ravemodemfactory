
noinst_PROGRAMS = test-message-private

TESTS = $(noinst_PROGRAMS)

test_message_private_SOURCES = \
	test-message-private.c
test_message_private_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir)/src/librmf-common
test_message_private_LDADD = \
	$(top_builddir)/src/librmf-common/librmf-common.la \
	$(GLIB_LIBS)
