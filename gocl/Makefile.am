MAINTAINERCLEANFILES = \
	Makefile.in \
	@PRJ_GIR_API_NAME@.gir

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = @PRJ_API_NAME@.pc

library_includedir=$(includedir)/@PRJ_API_NAME@
library_include_HEADERS = gocl.h

CLEANFILES = *~ @PRJ_API_NAME@.pc

AM_CFLAGS = $(GLIB_CFLAGS) -Wall \
	-DPKGDATADIR="\"$(pkgdatadir)\"" \
	-DENABLE_TESTS="\"$(enable_tests)\""

if ENABLE_DEBUG
AM_CFLAGS += -Werror -g3 -O0 -ggdb
else
AM_CFLAGS += -DG_DISABLE_ASSERT -DG_DISABLE_CHECKS
endif

if HAVE_COGL
AM_CFLAGS += \
	$(COGL_CFLAGS) \
	-DHAS_COGL="1"
endif

@PRJ_NAME@_marshal_list = $(addprefix $(srcdir)/, @PRJ_NAME@-marshal.list)

@PRJ_NAME@-marshal.h: $(@PRJ_NAME@_marshal_list)
	glib-genmarshal --header \
		--prefix=@PRJ_NAME@_marshal $(@PRJ_NAME@_marshal_list) > @PRJ_NAME@-marshal.h

@PRJ_NAME@-marshal.c: $(@PRJ_NAME@_marshal_list)
	glib-genmarshal --body \
		--prefix=@PRJ_NAME@_marshal $(@PRJ_NAME@_marshal_list) > @PRJ_NAME@-marshal.c

BUILT_SOURCES = @PRJ_NAME@-marshal.c @PRJ_NAME@-marshal.h
EXTRA_DIST = $(@PRJ_NAME@_marshal_list)

# libraries
lib_LTLIBRARIES = lib@PRJ_API_NAME@.la

source_c = \
	gocl-error.c \
	gocl-context.c \
	gocl-device.c \
	gocl-buffer.c \
	gocl-program.c \
	gocl-kernel.c \
	gocl-queue.c \
	gocl-event.c \
	gocl-image.c

source_h = \
	gocl.h \
	gocl-decls.h \
	gocl-error.h \
	gocl-context.h \
	gocl-device.h \
	gocl-buffer.h \
	gocl-program.h \
	gocl-kernel.h \
	gocl-queue.h \
	gocl-event.h \
	gocl-image.h

source_h_priv = \
	gocl-error-private.h

lib@PRJ_API_NAME@_la_LIBADD = \
	$(GLIB_LIBS) \
	-lOpenCL

if HAVE_COGL
lib@PRJ_API_NAME@_la_LIBADD += \
	$(COGL_LIBS)
endif

lib@PRJ_API_NAME@_la_CFLAGS  = \
	-I OpenCL \
	$(AM_CFLAGS)

lib@PRJ_API_NAME@_la_LDFLAGS = \
	-version-info 0:1:0 \
	-no-undefined

lib@PRJ_API_NAME@_la_SOURCES = \
	$(source_c) \
	$(source_h) \
	$(source_h_priv)

nodist_lib@PRJ_API_NAME@_la_SOURCES = \
	@PRJ_NAME@-marshal.c \
	@PRJ_NAME@-marshal.h

@PRJ_NAME@dir = $(includedir)/@PRJ_API_NAME@
@PRJ_NAME@_HEADERS = \
	$(source_h) \
	$(source_h_priv)

# introspection support
if HAVE_INTROSPECTION

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir) --warn-all --symbol-prefix=@PRJ_NAME@
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

introspection_sources = $(@PRJ_NAME@_HEADERS) $(source_c)

@PRJ_GIR_API_NAME@.gir: lib@PRJ_API_NAME@.la Makefile
@PRJ_GIR_TARGET_NAME@_INCLUDES = GObject-2.0 Gio-2.0
@PRJ_GIR_TARGET_NAME@_CFLAGS = $(INCLUDES)
@PRJ_GIR_TARGET_NAME@_LIBS = @PRJ_API_NAME@ OpenCL
@PRJ_GIR_TARGET_NAME@_NAMESPACE = @PRJ_GIR_NAMESPACE@
@PRJ_GIR_TARGET_NAME@_FILES = $(addprefix $(srcdir)/,$(introspection_sources))
@PRJ_GIR_TARGET_NAME@_SCANNERFLAGS = \
	--warn-all \
	--c-include='@PRJ_NAME@.h' \
	--pkg-export=@PRJ_API_NAME@

INTROSPECTION_GIRS += @PRJ_GIR_API_NAME@.gir

girdir = @INTROSPECTION_GIRDIR@
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = @INTROSPECTION_TYPELIBDIR@
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(dist_gir_DATA) $(typelib_DATA)
endif

maintainer-clean-local:
	rm -rf tmp-introspect*
