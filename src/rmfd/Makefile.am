
sbin_PROGRAMS = rmfd

rmfd_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src/librmf-common \
	-I$(top_srcdir)/src/librmf \
	$(GLIB_CFLAGS) \
	$(QMI_CFLAGS) \
	$(GUDEV_CFLAGS)

rmfd_SOURCES = \
	rmfd.c \
	rmfd-manager.h \
	rmfd-manager.c

rmfd_LDADD = \
	$(top_builddir)/src/librmf/librmf.la \
	$(top_builddir)/src/librmf-common/librmf-common.la \
	$(GLIB_LIBS) \
	$(QMI_LIBS) \
	$(GUDEV_LIBS)
