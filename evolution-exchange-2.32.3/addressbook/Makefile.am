# GConf schemas

schemadir = $(GCONF_SCHEMA_FILE_DIR)
schema_in_files = apps_exchange_addressbook.schemas.in.in
schema_DATA = $(schema_in_files:.schemas.in.in=-$(BASE_VERSION).schemas)
%-$(BASE_VERSION).schemas.in: %.schemas.in.in
	cp $< $@

@INTLTOOL_SCHEMAS_RULE@

if GCONF_SCHEMAS_INSTALL

if OS_WIN32
install-data-local:
	if test -z "$(DESTDIR)" ; then							\
		for p in $(schema_DATA) ; do						\
			(echo set GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE);	\
			 echo $(GCONFTOOL) --makefile-install-rule $$p) >_temp.bat;	\
			cmd /c _temp.bat; 						\
			rm _temp.bat;							\
		done									\
	fi
else
install-data-local:
	if test -z "$(DESTDIR)" ; then							\
		for p in $(schema_DATA) ; do						\
			GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) 		\
				$(GCONFTOOL) --makefile-install-rule $$p;		\
		done									\
	fi
endif

endif

extension_LTLIBRARIES = libebookbackendexchange.la

libebookbackendexchange_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/server/lib \
	-I$(top_srcdir)/server/storage \
	-I$(top_builddir)/server/lib \
	-DG_LOG_DOMAIN=\"e-book-backend-exchange\" \
	$(EVOLUTION_DATA_SERVER_CFLAGS) \
	$(EVOLUTION_PLUGIN_CFLAGS) \
	$(LDAP_CFLAGS) \
	$(DB_CFLAGS)

libebookbackendexchange_la_SOURCES = \
	e-book-backend-exchange.c \
	e-book-backend-exchange.h \
	e-book-backend-exchange-factory.c \
	e-book-backend-gal.c \
	e-book-backend-gal.h

libebookbackendexchange_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED)

libebookbackendexchange_la_LIBADD = \
	$(top_builddir)/tools/libevolution-exchange-shared.la \
	$(EVOLUTION_DATA_SERVER_LIBS) \
	$(EVOLUTION_PLUGIN_LIBS) \
	$(LDAP_LIBS) \
	$(DNS_LIBS)

if HAVE_LIBDB
libebookbackendexchange_la_SOURCES += \
	e-book-backend-db-cache.c \
	e-book-backend-db-cache.h

libebookbackendexchange_la_LIBADD +=  \
	$(DB_LIBS)
endif

EXTRA_DIST = $(schema_in_files)

CLEANFILES = apps_exchange_addressbook-$(BASE_VERSION).schemas

-include $(top_srcdir)/git.mk
