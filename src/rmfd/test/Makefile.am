include $(top_srcdir)/gtester.make

noinst_PROGRAMS = \
	test-stats

TEST_PROGS += $(noinst_PROGRAMS)

test_stats_SOURCES = test-stats.c
test_stats_CPPFLAGS = \
	-I$(top_srcdir)          \
	-I$(top_srcdir)/src/rmfd \
	$(GLIB_CFLAGS)
test_stats_LDADD = \
	$(top_builddir)/src/rmfd/librmfd-stats.la \
	$(GLIB_LIBS)
