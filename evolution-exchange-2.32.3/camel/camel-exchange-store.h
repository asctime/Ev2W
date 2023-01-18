/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Copyright (C) 2001-2004 Novell, Inc. */

/* camel-exchange-store.h: class for a exchange store */

#ifndef CAMEL_EXCHANGE_STORE_H
#define CAMEL_EXCHANGE_STORE_H

#include <camel/camel.h>

/* Standard GObject macros */
#define CAMEL_TYPE_EXCHANGE_STORE \
	(camel_exchange_store_get_type ())
#define CAMEL_EXCHANGE_STORE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), CAMEL_TYPE_EXCHANGE_STORE, CamelExchangeStore))
#define CAMEL_EXCHANGE_STORE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), CAMEL_TYPE_EXCHANGE_STORE, CamelExchangeStoreClass))
#define CAMEL_IS_EXCHANGE_STORE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), CAMEL_TYPE_EXCHANGE_STORE))
#define CAMEL_IS_EXCHANGE_STORE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), CAMEL_TYPE_EXCHANGE_STORE))
#define CAMEL_EXCHANGE_STORE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), CAMEL_TYPE_EXCHANGE_STORE, CamelExchangeStoreClass))

G_BEGIN_DECLS

typedef struct _CamelExchangeStore CamelExchangeStore;
typedef struct _CamelExchangeStoreClass CamelExchangeStoreClass;

struct _CamelExchangeStore {
	CamelOfflineStore parent;

	gchar *storage_path, *base_url;
	gchar *trash_name;
	GHashTable *folders;
	GMutex *folders_lock;
	gboolean reprompt_password;

	GMutex *connect_lock;
};

struct _CamelExchangeStoreClass {
	CamelOfflineStoreClass parent_class;
};

GType		camel_exchange_store_get_type	(void);
gboolean	camel_exchange_store_connected	(CamelExchangeStore *store,
						 GError **error);
void		camel_exchange_store_folder_created
						(CamelExchangeStore *estore,
						 const gchar *name,
						 const gchar *uri);
void		camel_exchange_store_folder_deleted
						(CamelExchangeStore *estore,
						 const gchar *name,
						 const gchar *uri);

G_END_DECLS

#endif /* CAMEL_EXCHANGE_STORE_H */
