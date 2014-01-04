MAINTAINERCLEANFILES = \
	Makefile.in

examples_dir = `pwd`/

AM_CFLAGS = \
	$(GLIB_CFLAGS) \
	-I $(top_srcdir)/@PRJ_NAME@/ \
	-DEXAMPLES_DIR="\"$(examples_dir)\""

if ENABLE_DEBUG
AM_CFLAGS += -Wall -Werror -g3 -O0 -ggdb
else
AM_CFLAGS += -DG_DISABLE_ASSERT -DG_DISABLE_CHECKS
endif

AM_LIBS = \
	$(GLIB_LIBS) \
	$(top_builddir)/@PRJ_NAME@/lib@PRJ_API_NAME@.la

noinst_PROGRAMS = \
	hello-world \
	hello-world-sync

# hello-world
hello_world_CFLAGS = $(AM_CFLAGS)
hello_world_LDADD = $(AM_LIBS)
hello_world_SOURCES = hello-world.c

# hello-world-sync
hello_world_sync_CFLAGS = $(AM_CFLAGS)
hello_world_sync_LDADD = $(AM_LIBS)
hello_world_sync_SOURCES = hello-world-sync.c

if HAVE_COGL

noinst_PROGRAMS += \
	gaussian-blur

# gaussian-blur
gaussian_blur_CFLAGS = $(AM_CFLAGS) $(COGL_CFLAGS) -DHAS_COGL
gaussian_blur_LDADD = $(AM_LIBS) $(COGL_LIBS) -lGL -lm
gaussian_blur_SOURCES = gaussian-blur.c

endif

EXTRA_DIST =
