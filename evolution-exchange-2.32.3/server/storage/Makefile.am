noinst_LTLIBRARIES = \
	libexchange-storage.la

libexchange_storage_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-DG_LOG_DOMAIN=\""evolution-exchange-storage"\" \
	-DPREFIX=\""$(prefix)"\" \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DDATADIR=\""$(datadir)"\" \
	-DLIBDIR=\""$(datadir)"\" \
	-DCONNECTOR_LOCALEDIR=\""$(localedir)"\" \
	-DCONNECTOR_GLADEDIR=\""$(gladedir)"\" \
	-I$(top_srcdir) \
	-I$(top_srcdir)/server/lib \
	-I$(top_srcdir)/server/xntlm \
	-I$(top_builddir)/server/lib \
	$(GNOME_PLATFORM_CFLAGS) \
	$(EVOLUTION_DATA_SERVER_CFLAGS) \
	$(KRB5_CFLAGS) \
	$(LDAP_CFLAGS) \
	$(SOUP_CFLAGS)

libexchange_storage_la_SOURCES = \
	e-folder-exchange.c \
	e-folder-exchange.h \
	e-folder-tree.c \
	e-folder-tree.h \
	e-folder-type-registry.c \
	e-folder-type-registry.h \
	e-folder.c \
	e-folder.h \
	e-storage.c \
	e-storage.h \
	exchange-account.c \
	exchange-account.h \
	exchange-constants.h \
	exchange-esource.c \
	exchange-esource.h \
	exchange-folder-size.c \
	exchange-folder-size.h \
	exchange-hierarchy-favorites.c \
	exchange-hierarchy-favorites.h \
	exchange-hierarchy-foreign.c \
	exchange-hierarchy-foreign.h \
	exchange-hierarchy-gal.c \
	exchange-hierarchy-gal.h \
	exchange-hierarchy-somedav.c \
	exchange-hierarchy-somedav.h \
	exchange-hierarchy-webdav.c \
	exchange-hierarchy-webdav.h \
	exchange-hierarchy.c \
	exchange-hierarchy.h \
	exchange-oof.c \
	exchange-oof.h \
	exchange-types.h

libexchange_storage_la_LIBADD = \
	$(top_builddir)/server/lib/libexchange.la \
	$(top_builddir)/server/xntlm/libxntlm.la \
	$(GNOME_PLATFORM_LIBS) \
	$(EVOLUTION_DATA_SERVER_LIBS) \
	$(LDAP_LIBS) \
	$(SOUP_LIBS) \
	$(SOCKET_LIBS)

libexchange_storage_la_LDFLAGS = \
	$(KRB5_LIBS) -avoid-version $(NO_UNDEFINED)

-include $(top_srcdir)/git.mk
