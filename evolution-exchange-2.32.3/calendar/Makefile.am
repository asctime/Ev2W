extension_LTLIBRARIES = libecalbackendexchange.la

libecalbackendexchange_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/server/lib \
	-I$(top_srcdir)/server/storage \
	-I$(top_builddir)/server/lib \
	-DG_LOG_DOMAIN=\"e-cal-backend-exchange\" \
	$(EVOLUTION_DATA_SERVER_CFLAGS) \
	$(EVOLUTION_PLUGIN_CFLAGS) \
	$(LDAP_CFLAGS)

libecalbackendexchange_la_SOURCES = \
	e-cal-backend-exchange.c \
	e-cal-backend-exchange.h \
	e-cal-backend-exchange-calendar.c \
	e-cal-backend-exchange-calendar.h \
	e-cal-backend-exchange-tasks.c \
	e-cal-backend-exchange-tasks.h \
	e-cal-backend-exchange-factory.c \
	e-cal-backend-exchange-factory.h \
	e2k-cal-query.c \
	e2k-cal-query.h \
	e2k-cal-utils.c \
	e2k-cal-utils.h

libecalbackendexchange_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED)

libecalbackendexchange_la_LIBADD = \
	$(top_builddir)/tools/libevolution-exchange-shared.la \
	$(EVOLUTION_DATA_SERVER_LIBS) \
	$(EVOLUTION_PLUGIN_LIBS) \
	$(LDAP_LIBS) \
	$(DNS_LIBS)

-include $(top_srcdir)/git.mk
