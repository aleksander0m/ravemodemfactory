
bin_PROGRAMS = rmfcli

rmfcli_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src/librmf-common \
	-I$(top_srcdir)/src/librmf

rmfcli_SOURCES = rmfcli.cpp

rmfcli_LDADD = \
	$(top_builddir)/src/librmf/librmf.la \
	$(top_builddir)/src/librmf-common/librmf-common.la
