
lib_LTLIBRARIES = librmf.la

librmf_la_SOURCES = \
	rmf-types.h \
	rmf-operations.h \
	rmf-operations.cpp
librmf_la_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src/librmf-common
librmf_la_LIBADD = \
	$(top_builddir)/src/librmf-common/librmf-common.la

includedir = @includedir@/librmf
include_HEADERS = \
	rmf-types.h \
	rmf-operations.h
