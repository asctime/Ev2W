# The name of the module.
DOC_MODULE = libedataserverui

# The top-level SGML file.
DOC_MAIN_SGML_FILE = libedataserverui-docs.sgml

# Extra options to supply to gtkdoc-scan.
SCAN_OPTIONS = --deprecated-guards="EDS_DISABLE_DEPRECATED"

# The directory containing the source code. Relative to $(srcdir).
DOC_SOURCE_DIR = $(top_srcdir)/libedataserverui

# Used for dependencies. The docs will be rebuilt if any of these change.
HFILE_GLOB = $(top_srcdir)/libedataserverui/*.h
CFILE_GLOB = $(top_srcdir)/libedataserverui/*.c

IGNORE_HFILES = 			\
	e-data-server-ui-marshal.h	\
	gtk-compat.h

GTKDOC_CFLAGS  =					\
	-I$(top_srcdir)/libedataserverui		\
	-I$(top_srcdir)/libedataserver			\
	-I$(top_srcdir)/addressbook			\
	-I$(top_srcdir) 				\
	-I$(top_builddir)				\
	$(E_DATA_SERVER_UI_CFLAGS)

if ENABLE_GTK3
GTKDOC_LIBS = 								\
	$(top_builddir)/libedataserverui/libedataserverui-3.0.la	\
	$(top_builddir)/libedataserver/libedataserver-1.2.la		\
	$(E_DATA_SERVER_UI_LIBS)
else
GTKDOC_LIBS = 								\
	$(top_builddir)/libedataserverui/libedataserverui-1.2.la	\
	$(top_builddir)/libedataserver/libedataserver-1.2.la		\
	$(E_DATA_SERVER_UI_LIBS)
endif

# This includes the standard gtk-doc make rules, copied by gtkdocize.
include $(top_srcdir)/gtk-doc.make

#TESTS = $(GTKDOC_CHECK)

-include $(top_srcdir)/git.mk
