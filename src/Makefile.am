INCLUDES = -I$(top_srcdir)

AM_CPPFLAGS = \
  -DPKGDATADIR=\"$(pkgdatadir)\" \
  -DDATADIR=\""$(datadir)"\" -D_GNU_SOURCE

AM_CFLAGS = -Wall -pedantic -std=c99
#@DEPENDENCIES_CFLAGS@

# add the name of your application
bin_PROGRAMS = bootchart-lite

# add the sources to compile for your application
bootchart_lite_SOURCES = \
  bootchart-lite.c

#fbootchart_LDADD  = @DEPENDENCIES_LIBS@

#MAINTAINERCLEANFILES  = config.h.in Makefile.in

