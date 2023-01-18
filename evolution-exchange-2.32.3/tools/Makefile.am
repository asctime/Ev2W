noinst_PROGRAMS = exchange-connector-setup

noinst_LTLIBRARIES = libevolution-exchange-shared.la

libevolution_exchange_shared_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/camel \
	-I$(top_srcdir)/server/lib \
	-I$(top_srcdir)/server/storage \
	-I$(top_builddir)/server/lib \
	-DG_LOG_DOMAIN="\"evolution-exchange-storage\"" \
	$(GNOME_PLATFORM_CFLAGS) \
	$(EVOLUTION_DATA_SERVER_CFLAGS) \
	$(LDAP_CFLAGS) \
	-DPREFIX=\""$(prefix)"\" \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DDATADIR=\""$(datadir)"\" \
	-DLIBDIR=\""$(datadir)"\" \
	-DCONNECTOR_IMAGESDIR=\""$(imagesdir)"\" \
	-DCONNECTOR_UIDIR=\""$(uidir)"\" \
	-DCONNECTOR_LOCALEDIR=\""$(localedir)"\"

libevolution_exchange_shared_la_SOURCES = \
	exchange-share-config-listener.h \
	exchange-share-config-listener.c

libevolution_exchange_shared_la_LIBADD = \
	$(top_builddir)/server/storage/libexchange-storage.la \
	$(GNOME_PLATFORM_LIBS) \
	$(EVOLUTION_DATA_SERVER_LIBS) \
	$(LDAP_LIBS)

libevolution_exchange_shared_la_LDFLAGS = -avoid-version $(NO_UNDEFINED)

exchange_connector_setup_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/camel \
	-I$(top_srcdir)/server/lib \
	-I$(top_srcdir)/server/storage \
	-I$(top_builddir)/server/lib \
	-DG_LOG_DOMAIN="\"evolution-exchange-storage\"" \
	$(GNOME_PLATFORM_CFLAGS) \
	$(EVOLUTION_DATA_SERVER_CFLAGS) \
	$(LDAP_CFLAGS) \
	-DPREFIX=\""$(prefix)"\" \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DDATADIR=\""$(datadir)"\" \
	-DLIBDIR=\""$(datadir)"\" \
	-DCONNECTOR_IMAGESDIR=\""$(imagesdir)"\" \
	-DCONNECTOR_UIDIR=\""$(uidir)"\" \
	-DCONNECTOR_LOCALEDIR=\""$(localedir)"\"

exchange_connector_setup_SOURCES = \
	ximian-connector-setup.c \
	exchange-autoconfig-wizard.h \
	exchange-autoconfig-wizard.c

exchange_connector_setup_LDADD = \
	$(top_builddir)/server/xntlm/libxntlm.la \
	$(top_builddir)/server/lib/libexchange.la \
	$(top_builddir)/server/storage/libexchange-storage.la \
	$(GNOME_PLATFORM_LIBS) \
	$(EVOLUTION_DATA_SERVER_LIBS) \
	$(LDAP_LIBS)\
	$(DNS_LIBS)

if OS_WIN32
exchange_connector_setup_LDFLAGS = -mwindows
endif

install-exec-local: exchange-connector-setup@EXEEXT@
	$(mkinstalldirs) $(DESTDIR)$(bindir)
	$(LIBTOOL) --mode=install $(INSTALL_PROGRAM) exchange-connector-setup@EXEEXT@ $(DESTDIR)$(bindir)/exchange-connector-setup-$(BASE_VERSION)@EXEEXT@

uninstall-local:
	rm -rf $(DESTDIR)$(bindir)/exchange-connector-setup-$(BASE_VERSION)@EXEEXT@

imagesdir = $(CONNECTOR_DATADIR)/images
images_DATA = \
	connector.png \
	connector-mini.png \
	exchange-delegates-48.png \
	exchange-oof-48.png

uidir   = $(CONNECTOR_DATADIR)/ui
ui_DATA = ximian-connector.xml

EXTRA_DIST = \
	$(images_DATA) \
	$(ui_DATA)

-include $(top_srcdir)/git.mk
