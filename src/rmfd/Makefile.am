
sbin_PROGRAMS = rmfd
dist_bin_SCRIPTS = rmfd-data-wwan-service

BUILT_SOURCES = \
	rmfd-error-types.h \
	rmfd-error-types.c

CLEANFILES = $(BUILT_SOURCES)

# rmfd daemon
rmfd_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src/librmf-common \
	-I$(top_srcdir)/src/librmf \
	$(GLIB_CFLAGS) \
	$(QMI_CFLAGS) \
	$(GUDEV_CFLAGS)

rmfd_SOURCES = \
	rmfd.c \
	rmfd-utils.h rmfd-utils.c \
	rmfd-error.h rmfd-error.c \
	rmfd-error-types.h rmfd-error-types.c \
	rmfd-manager.h rmfd-manager.c \
	rmfd-processor.h rmfd-processor.c \
	rmfd-data.h rmfd-data.c \
	rmfd-data-wwan.h rmfd-data-wwan.c


rmfd_LDADD = \
	$(top_builddir)/src/librmf/librmf.la \
	$(top_builddir)/src/librmf-common/librmf-common.la \
	$(GLIB_LIBS) \
	$(QMI_LIBS) \
	$(GUDEV_LIBS)

# Error types & quarks
rmfd-error-types.h: Makefile.am rmfd-error.h $(top_srcdir)/build-aux/rmfd-error-types-template.h
	$(AM_V_GEN) $(GLIB_MKENUMS) \
		--fhead "#ifndef __RMFD_ERROR_TYPES_H__\n#define __RMFD_ERROR_TYPES_H__\n" \
		--template $(top_srcdir)/build-aux/rmfd-error-types-template.h \
		--ftail "#endif /* __RMFD_ERROR_TYPES_H__ */\n" \
		rmfd-error.h > $@

rmfd-error-types.c: Makefile.am rmfd-error.h $(top_srcdir)/build-aux/rmfd-error-types-template.c rmfd-error-types.h
	$(AM_V_GEN) $(GLIB_MKENUMS) \
		--fhead "#include \"rmfd-error.h\"\n#include \"rmfd-error-types.h\"\n" \
		--template $(top_srcdir)/build-aux/rmfd-error-types-template.c \
		rmfd-error.h > $@
