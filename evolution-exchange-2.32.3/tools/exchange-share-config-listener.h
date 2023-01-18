/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* Copyright (C) 2001-2004 Novell, Inc. */

#ifndef __EXCHANGE_SHARE_CONFIG_LISTENER_H__
#define __EXCHANGE_SHARE_CONFIG_LISTENER_H__

#include <exchange-types.h>
#include <exchange-constants.h>
#include <libedataserver/e-account-list.h>
#include <libedataserver/e-source-list.h>
#include <libedataserver/e-source-group.h>

G_BEGIN_DECLS

#define EXCHANGE_TYPE_SHARE_CONFIG_LISTENER            (exchange_share_config_listener_get_type ())
#define EXCHANGE_SHARE_CONFIG_LISTENER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EXCHANGE_TYPE_SHARE_CONFIG_LISTENER, ExchangeShareConfigListener))
#define EXCHANGE_SHARE_CONFIG_LISTENER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EXCHANGE_TYPE_SHARE_CONFIG_LISTENER, ExchangeShareConfigListenerClass))
#define EXCHANGE_IS_SHARE_CONFIG_LISTENER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EXCHANGE_TYPE_SHARE_CONFIG_LISTENER))
#define EXCHANGE_IS_SHARE_CONFIG_LISTENER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), EXCHANGE_TYPE_SHARE_CONFIG_LISTENER))

typedef struct _ExchangeShareConfigListener           ExchangeShareConfigListener;
typedef struct _ExchangeShareConfigListenerPrivate    ExchangeShareConfigListenerPrivate;
typedef struct _ExchangeShareConfigListenerClass      ExchangeShareConfigListenerClass;

struct _ExchangeShareConfigListener {
	EAccountList parent;

	ExchangeShareConfigListenerPrivate *priv;
};

struct _ExchangeShareConfigListenerClass {
	EAccountListClass parent_class;

	/* signals */
	void (*exchange_account_created) (ExchangeShareConfigListener *,
					  ExchangeAccount *);
	void (*exchange_account_removed) (ExchangeShareConfigListener *,
					  ExchangeAccount *);
};

#if 0
typedef enum {
	EXCHANGE_CALENDAR_FOLDER,
	EXCHANGE_TASKS_FOLDER,
	EXCHANGE_CONTACTS_FOLDER
}FolderType;
#endif

#define CONF_KEY_CAL "/apps/evolution/calendar/sources"
#define CONF_KEY_TASKS "/apps/evolution/tasks/sources"
#define CONF_KEY_CONTACTS "/apps/evolution/addressbook/sources"
#define EXCHANGE_URI_PREFIX "exchange://"

GType                        exchange_share_config_listener_get_type (void);
ExchangeShareConfigListener *exchange_share_config_listener_new      (void);
ExchangeShareConfigListener *exchange_share_config_listener_get_global (void);

GSList                 *exchange_share_config_listener_get_accounts (ExchangeShareConfigListener *config_listener);

void			exchange_share_config_listener_migrate_esources (ExchangeShareConfigListener *config_listener);

/*void			add_folder_esource (ExchangeAccount *account, FolderType folder_type, const gchar *folder_name, const gchar *physical_uri);
void			remove_folder_esource (ExchangeAccount *account, FolderType folder_type, const gchar *physical_uri);*/

ExchangeAccount *exchange_share_config_listener_get_account_for_uri (ExchangeShareConfigListener *excl, const gchar *uri);

G_END_DECLS

#endif /* __EXCHANGE_SHARE_CONFIG_LISTENER_H__ */
