# The name of the module.
DOC_MODULE=evolution-exchange

# The top-level SGML file.
DOC_MAIN_SGML_FILE=$(DOC_MODULE)-docs.sgml

# Files/directories to ignore when scanning.
IGNORE_HFILES= \
	config.h \
	config-defines.h \
	config-renaming.h \
	Evolution-Addressbook-SelectNames.h \
	connector-public-key.h \
	e2k-marshal.h \
	e2k-propnames.h \
	e2k-proptags.h \
	e2k-types.h \
	addressbook \
	calendar \
	camel \
	mail \
	shell \
	storage

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR=../../

# Used for dependencies. The docs will be rebuilt if any of these change.
HFILE_GLOB=$(top_srcdir)/*/*.h
CFILE_GLOB=$(top_srcdir)/*/*.c

# CFLAGS and LDFLAGS for compiling gtkdoc-scangobj with your library.
# Only needed if you are using gtkdoc-scangobj to dynamically query widget
# signals and properties.
GTKDOC_CFLAGS = \
	-I$(top_srcdir)/server/lib \
	-I$(top_srcdir)/server/storage \
	$(GNOME_PLATFORM_CFLAGS) \
	$(LDAP_CFLAGS)

GTKDOC_LIBS = \
	$(GNOME_PLATFORM_LIBS) \
	$(top_builddir)/server/storage/libexchange-storage.la \
	$(LDAP_LIBS)

# This includes the standard gtk-doc make rules, copied by gtkdocize.
include $(top_srcdir)/gtk-doc.make

dist-hook-local:
	mkdir $(distdir)/features
	-cp $(srcdir)/features/*.xml $(distdir)/features

-include $(top_srcdir)/git.mk
