MAINTAINERCLEANFILES = \
	Makefile.in

AM_CFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir)/gocl

if ENABLE_DEBUG
AM_CFLAGS += -Werror -g3 -O0 -ggdb
else
AM_CFLAGS += -DG_DISABLE_ASSERT -DG_DISABLE_CHECKS
endif

AM_LIBS = \
	$(GLIB_LIBS) \
	$(top_srcdir)/gocl/lib@PRJ_API_NAME@.la

noinst_PROGRAMS = \
	hello-world

# hello-world
hello_world_CFLAGS = $(AM_CFLAGS)
hello_world_LDADD = $(AM_LIBS)
hello_world_SOURCES = hello-world.c

EXTRA_DIST =
