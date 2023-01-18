/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Copyright (C) 2001-2004 Novell, Inc. */

/* camel-exchange-folder.h: class for a exchange folder */

#ifndef CAMEL_EXCHANGE_FOLDER_H
#define CAMEL_EXCHANGE_FOLDER_H 1

#include <camel/camel.h>

/* Standard GObject macros */
#define CAMEL_TYPE_EXCHANGE_FOLDER \
	(camel_exchange_folder_get_type ())
#define CAMEL_EXCHANGE_FOLDER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), CAMEL_TYPE_EXCHANGE_FOLDER, CamelExchangeFolder))
#define CAMEL_EXCHANGE_FOLDER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), CAMEL_TYPE_EXCHANGE_FOLDER, CamelExchangeFolderClass))
#define CAMEL_IS_EXCHANGE_FOLDER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), CAMEL_TYPE_EXCHANGE_FOLDER))
#define CAMEL_IS_EXCHANGE_FOLDER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), CAMEL_TYPE_EXCHANGE_FOLDER))
#define CAMEL_EXCHANGE_FOLDER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), CAMEL_TYPE_EXCHANGE_FOLDER, CamelExchangeFolderClass))

G_BEGIN_DECLS

typedef struct _CamelExchangeFolder CamelExchangeFolder;
typedef struct _CamelExchangeFolderClass CamelExchangeFolderClass;

struct _CamelExchangeFolder {
	CamelOfflineFolder parent_object;

	CamelDataCache *cache;
	CamelOfflineJournal *journal;
	gchar *source;

	GHashTable *thread_index_to_message_id;
};

struct _CamelExchangeFolderClass {
	CamelOfflineFolderClass parent_class;
};

GType    camel_exchange_folder_get_type (void);

gboolean camel_exchange_folder_construct            (CamelFolder *folder,
						     guint32 camel_flags,
						     const gchar *folder_dir,
						     gint offline_state,
						     GError **error);

void     camel_exchange_folder_add_message          (CamelExchangeFolder *exch,
						     const gchar *uid,
						     guint32 flags,
						     guint32 size,
						     const gchar *headers,
						     const gchar *href);

void     camel_exchange_folder_remove_message       (CamelExchangeFolder *exch,
						     const gchar *uid);

void     camel_exchange_folder_uncache_message      (CamelExchangeFolder *exch,
						     const gchar *uid);

void     camel_exchange_folder_update_message_flags (CamelExchangeFolder *exch,
						     const gchar *uid,
						     guint32 flags);

void     camel_exchange_folder_update_message_flags_ex (CamelExchangeFolder *exch,
							const gchar *uid,
							guint32 flags,
							guint32 mask);

void     camel_exchange_folder_update_message_tag   (CamelExchangeFolder *exch,
						     const gchar *uid,
						     const gchar *name,
						     const gchar *value);

G_END_DECLS

#endif /* CAMEL_EXCHANGE_FOLDER_H */

