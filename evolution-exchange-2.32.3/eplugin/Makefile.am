### EVO_PLUGIN_RULE ### begin ###

%.eplug: %.eplug.in
	sed -e 's|\@PLUGINDIR\@|$(plugindir)|'		\
	-e 's|\@SOEXT\@|$(SOEXT)|'			\
	-e 's|\@GETTEXT_PACKAGE\@|$(GETTEXT_PACKAGE)|'	\
	-e 's|\@LOCALEDIR\@|$(localedir)|' $< > $@

%.eplug.in: %.eplug.xml
	LC_ALL=C $(INTLTOOL_MERGE) -x -u /tmp $< $@

%.error: %.error.xml
	LC_ALL=C $(INTLTOOL_MERGE) -x -u /tmp $< $@

### EVO_PLUGIN_RULE ### end ###


plugin_DATA = org-gnome-exchange-operations.eplug

plugin_LTLIBRARIES = liborg-gnome-exchange-operations.la

liborg_gnome_exchange_operations_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_srcdir)/server/lib \
	-I$(top_srcdir)/server/storage \
	-I$(top_builddir)/server/lib \
	$(EVOLUTION_DATA_SERVER_CFLAGS) \
	$(EVOLUTION_PLUGIN_CFLAGS) \
	$(EVOLUTION_SHELL_CFLAGS) \
	$(EVOLUTION_MAIL_CFLAGS) \
	$(LDAP_CFLAGS)


liborg_gnome_exchange_operations_la_SOURCES = \
	exchange-operations.c \
	exchange-operations.h \
	exchange-config-listener.c \
	exchange-config-listener.h \
	exchange-calendar.c \
	exchange-contacts.c \
	exchange-change-password.c \
	exchange-change-password.h \
	exchange-delegates-user.c \
	exchange-delegates-user.h \
	exchange-delegates.c \
	exchange-delegates.h \
	exchange-user-dialog.c \
	exchange-user-dialog.h \
	exchange-folder-size-display.c \
	exchange-folder-size-display.h \
	exchange-account-setup.c \
	exchange-permissions-dialog.c \
	exchange-permissions-dialog.h \
	exchange-folder-permission.c \
	exchange-folder-subscription.c \
	exchange-folder-subscription.h \
	exchange-folder.c \
	exchange-mail-send-options.c \
	exchange-send-options.c \
	exchange-send-options.h

liborg_gnome_exchange_operations_la_LIBADD = \
	$(top_builddir)/server/storage/libexchange-storage.la \
	$(EVOLUTION_DATA_SERVER_LIBS) \
	$(EVOLUTION_PLUGIN_LIBS) \
	$(EVOLUTION_SHELL_LIBS) \
	$(EVOLUTION_MAIL_LIBS) \
	$(LDAP_LIBS) \
	$(DNS_LIBS)

liborg_gnome_exchange_operations_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED)

error_DATA = org-gnome-exchange-operations.error
errordir = $(EVOLUTION_PLUGIN_errordir)

EXTRA_DIST = \
	org-gnome-exchange-operations.eplug.xml \
	org-gnome-exchange-operations.error.xml

BUILT_SOURCES = $(error_DATA) org-gnome-exchange-operations.eplug 

CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
