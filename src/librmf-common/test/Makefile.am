include $(top_srcdir)/gtester.make

noinst_PROGRAMS = test-message-private test-message

TEST_PROGS += $(noinst_PROGRAMS)

test_message_private_SOURCES = \
	test-message-private.c
test_message_private_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir)/src/librmf-common
test_message_private_LDADD = \
	$(top_builddir)/src/librmf-common/librmf-common.la \
	$(GLIB_LIBS)

test_message_SOURCES = \
	test-message.c
test_message_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir)/src/librmf-common
test_message_LDADD = \
	$(top_builddir)/src/librmf-common/librmf-common.la \
	$(GLIB_LIBS)
