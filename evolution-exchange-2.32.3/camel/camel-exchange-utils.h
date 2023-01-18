/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Copyright (C) 2001-2004 Novell, Inc. */

#ifndef CAMEL_EXCHANGE_UTILS_H
#define CAMEL_EXCHANGE_UTILS_H

#include <camel/camel.h>

G_BEGIN_DECLS

gboolean camel_exchange_utils_connect (CamelService *service,
					const gchar *pwd,
					guint32 *status, /* out */
					GError **error);

gboolean camel_exchange_utils_get_folder (CamelService *service,
					const gchar *name,
					gboolean create,
					GPtrArray *uids,
					GByteArray *flags,
					GPtrArray *hrefs,
					guint32 high_article_num,
					guint32 *folder_flags, /* out */
					gchar **folder_uri, /* out */
					gboolean *readonly, /* out */
					GError **error);

gboolean camel_exchange_utils_get_trash_name (CamelService *service,
					gchar **trash_name, /* out */
					GError **error);

gboolean camel_exchange_utils_refresh_folder (CamelService *service,
					const gchar *folder_name,
					GError **error);

gboolean camel_exchange_utils_sync_count (CamelService *service,
					const gchar *folder_name,
					guint32 *unread_count, /* out */
					guint32 *visible_count, /* out */
					GError **error);

gboolean camel_exchange_utils_expunge_uids (CamelService *service,
					const gchar *folder_name,
					GPtrArray *uids,
					GError **error);

gboolean camel_exchange_utils_append_message (CamelService *service,
					const gchar *folder_name,
					guint32 flags,
					const gchar *subject,
					const GByteArray *message,
					gchar **new_uid, /* out */
					GError **error);

gboolean camel_exchange_utils_set_message_flags (CamelService *service,
					const gchar *folder_name,
					const gchar *uid,
					guint32 flags,
					guint32 mask,
					GError **error);

gboolean camel_exchange_utils_set_message_tag (CamelService *service,
					const gchar *folder_name,
					const gchar *uid,
					const gchar *name,
					const gchar *value,
					GError **error);

gboolean camel_exchange_utils_get_message (CamelService *service,
					const gchar *folder_name,
					const gchar *uid,
					GByteArray **message_bytes, /* out */
					GError **error);

gboolean camel_exchange_utils_search (CamelService *service,
					const gchar *folder_name,
					const gchar *text,
					GPtrArray **found_uids, /* out */
					GError **error);

gboolean camel_exchange_utils_transfer_messages (CamelService *service,
					const gchar *source_name,
					const gchar *dest_name,
					GPtrArray *uids,
					gboolean delete_originals,
					GPtrArray **ret_uids, /* out */
					GError **error);

gboolean camel_exchange_utils_get_folder_info (CamelService *service,
					const gchar *top,
					guint32 store_flags,
					GPtrArray **folder_names, /* out */
					GPtrArray **folder_uris, /* out */
					GArray **unread_counts, /* out */
					GArray **folder_flags, /* out */
					GError **error);

gboolean camel_exchange_utils_send_message (CamelService *service,
					const gchar *from,
					GPtrArray *recipients,
					const GByteArray *message,
					GError **error);

gboolean camel_exchange_utils_create_folder (CamelService *service,
					const gchar *parent_name,
					const gchar *folder_name,
					gchar **folder_uri, /* out */
					guint32 *unread_count, /* out */
					guint32 *flags, /* out */
					GError **error);

gboolean camel_exchange_utils_delete_folder (CamelService *service,
					const gchar *folder_name,
					GError **error);

gboolean camel_exchange_utils_rename_folder (CamelService *service,
					const gchar *old_name,
					const gchar *new_name,
					GPtrArray **folder_names, /* out */
					GPtrArray **folder_uris, /* out */
					GArray **unread_counts, /* out */
					GArray **folder_flags, /* out */
					GError **error);

gboolean camel_exchange_utils_subscribe_folder (CamelService *service,
					const gchar *folder_name,
					GError **error);

gboolean camel_exchange_utils_unsubscribe_folder (CamelService *service,
					const gchar *folder_name,
					GError **error);

gboolean camel_exchange_utils_is_subscribed_folder (CamelService *service,
					const gchar *folder_name,
					gboolean *is_subscribed, /* out */
					GError **error);

G_END_DECLS

#endif /* CAMEL_EXCHANGE_UTILS_H */
