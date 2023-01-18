## Process this file with automake to produce Makefile.in

CAMEL_provider_LTLIBRARIES = libcamelexchange.la
CAMEL_provider_DATA = libcamelexchange.urls

libcamelexchange_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	$(EVOLUTION_PLUGIN_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/tools \
	-I$(top_srcdir)/server/lib \
	-I$(top_srcdir)/server/storage \
	-I$(top_builddir)/server/lib \
	-DPREFIX=\"$(prefix)\" \
	-DCONNECTOR_LOCALEDIR=\"$(localedir)\" \
	-DG_LOG_DOMAIN=\"camel-exchange-provider\"

libcamelexchange_la_SOURCES = \
	camel-exchange-folder.c \
	camel-exchange-provider.c \
	camel-exchange-journal.c \
	camel-exchange-search.c \
	camel-exchange-store.c \
	camel-exchange-summary.c \
	camel-exchange-transport.c \
	camel-exchange-utils.c \
	mail-utils.c

noinst_HEADERS = \
	camel-exchange-folder.h \
	camel-exchange-journal.h \
	camel-exchange-search.h \
	camel-exchange-store.h \
	camel-exchange-summary.h \
	camel-exchange-transport.h \
	camel-exchange-utils.h \
	mail-utils.h

libcamelexchange_la_LDFLAGS = \
	-avoid-version -module $(NO_UNDEFINED)

libcamelexchange_la_LIBADD = \
	$(top_builddir)/tools/libevolution-exchange-shared.la \
	$(EVOLUTION_PLUGIN_LIBS) \
	$(LDAP_LIBS) \
	$(SOCKET_LIBS) \
	$(DNS_LIBS)

EXTRA_DIST = libcamelexchange.urls

-include $(top_srcdir)/git.mk
