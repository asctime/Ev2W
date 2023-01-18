/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Copyright (C) 2001-2004 Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* camel-exchange-store.c: class for a exchange store */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <glib/gi18n-lib.h>

#include <libedataserver/e-data-server-util.h>

#include "camel-exchange-store.h"
#include "camel-exchange-folder.h"
#include "camel-exchange-summary.h"
#include "camel-exchange-utils.h"

#define SUBFOLDER_DIR_NAME     "subfolders"
#define SUBFOLDER_DIR_NAME_LEN 10
#define d(x)

#define EXCHANGE_STOREINFO_VERSION 1

/* Even if we are disconnected, we need to exchange_store_connect()
   to get the offline data */
#define RETURN_VAL_IF_NOT_CONNECTED(store, ex, val)\
	if (!camel_exchange_store_connected(store, ex) && \
	    !exchange_store_connect (CAMEL_SERVICE (store), ex)) \
		return val;

extern CamelServiceAuthType camel_exchange_password_authtype;
extern CamelServiceAuthType camel_exchange_ntlm_authtype;

G_DEFINE_TYPE (CamelExchangeStore, camel_exchange_store, CAMEL_TYPE_OFFLINE_STORE)

/* Note: steals @name and @uri */
static CamelFolderInfo *
make_folder_info (CamelExchangeStore *exch,
                  gchar *name,
                  const gchar *uri,
                  gint unread_count,
                  gint flags)
{
	CamelFolderInfo *info;
	const gchar *path;
	gchar **components;
	gchar *new_uri;
	gchar *temp;

	d(printf ("make folder info : %s flags : %d\n", name, flags));
	path = strstr (uri, "://");
	if (!path)
		return NULL;
	path = strstr (path + 3, "/;");
	if (!path)
		return NULL;

	components = g_strsplit (uri, "/;", 2);
	if (components[0] && components[1])
		new_uri = g_strdup_printf ("%s/%s", components[0], components[1]);
	else
		new_uri = g_strdup (uri);
	g_strfreev (components);

	d(printf ("new_uri is : %s\n", new_uri));
	info = camel_folder_info_new ();
	info->name = name;
	info->uri = new_uri;

	/* Process the full-path and decode if required */
	temp = strrchr (path+2, '/');
	if (temp) {
		/* info->full_name should not have encoded path */
		info->full_name = camel_url_decode_path (path+2);
	} else {
		/* If there are no sub-directories, decoded(name) will be
		   equal to that of path+2.
		   Ex: personal
		*/
		info->full_name = g_strdup (path+2);
	}
	info->unread = unread_count;

	if (flags & CAMEL_FOLDER_NOSELECT)
		info->flags = CAMEL_FOLDER_NOSELECT;

	if (flags & CAMEL_FOLDER_SYSTEM)
		info->flags |= CAMEL_FOLDER_SYSTEM;

	if ((flags & CAMEL_FOLDER_TYPE_MASK) == CAMEL_FOLDER_TYPE_INBOX)
		info->flags |= CAMEL_FOLDER_TYPE_INBOX;

	if ((flags & CAMEL_FOLDER_TYPE_MASK) == CAMEL_FOLDER_TYPE_TRASH)
		info->flags |= CAMEL_FOLDER_TYPE_TRASH;

	if ((flags & CAMEL_FOLDER_TYPE_MASK) == CAMEL_FOLDER_TYPE_SENT)
		info->flags |= CAMEL_FOLDER_TYPE_SENT;

	if (flags & CAMEL_FOLDER_SUBSCRIBED)
		info->flags |= CAMEL_FOLDER_SUBSCRIBED;

	if (flags & CAMEL_FOLDER_NOCHILDREN)
		info->flags |= CAMEL_FOLDER_NOCHILDREN;
	return info;
}

static CamelFolderInfo *
postprocess_tree (CamelFolderInfo *info)
{
	CamelFolderInfo *sibling;

	if (info->child)
		info->child = postprocess_tree (info->child);
	if (info->next)
		info->next = postprocess_tree (info->next);

	/* If the node still has children, keep it */
	if (info->child)
		return info;

	/* info->flags |= CAMEL_FOLDER_NOCHILDREN; */

	/* If it's a mail folder (not noselect), keep it */
	if (!(info->flags & CAMEL_FOLDER_NOSELECT))
		return info;

	/* Otherwise delete it and return its sibling */
	sibling = info->next;
	info->next = NULL;
	camel_folder_info_free (info);
	return sibling;
}

/* This has been now removed from evolution/e-util. So implemented this here.
 * Also note that this is similar to the call in e2k-path.c. The name of the
 * function has been changed to avoid any conflicts.
 */
static gchar *
exchange_store_path_to_physical (const gchar *prefix,
                                 const gchar *vpath)
{
	const gchar *p, *newp;
	gchar *dp;
	gchar *ppath;
	gint ppath_len;
	gint prefix_len;

	while (*vpath == '/')
		vpath++;
	if (!prefix)
		prefix = "";

	/* Calculate the length of the real path. */
	ppath_len = strlen (vpath);
	ppath_len++;	/* For the ending zero.  */

	prefix_len = strlen (prefix);
	ppath_len += prefix_len;
	ppath_len++;	/* For the separating slash.  */

	/* Take account of the fact that we need to translate every
	 * separator into `subfolders/'.
	 */
	p = vpath;
	while (1) {
		newp = strchr (p, '/');
		if (newp == NULL)
			break;

		ppath_len += SUBFOLDER_DIR_NAME_LEN;
		ppath_len++; /* For the separating slash.  */

		/* Skip consecutive slashes.  */
		while (*newp == '/')
			newp++;

		p = newp;
	};

	ppath = g_malloc (ppath_len);
	dp = ppath;

	memcpy (dp, prefix, prefix_len);
	dp += prefix_len;
	*(dp++) = '/';

	/* Copy the mangled path.  */
	p = vpath;
	while (1) {
		newp = strchr (p, '/');
		if (newp == NULL) {
			strcpy (dp, p);
			break;
		}

		memcpy (dp, p, newp - p + 1); /* `+ 1' to copy the slash too.  */
		dp += newp - p + 1;

		memcpy (dp, SUBFOLDER_DIR_NAME, SUBFOLDER_DIR_NAME_LEN);
		dp += SUBFOLDER_DIR_NAME_LEN;

		*(dp++) = '/';

		/* Skip consecutive slashes.  */
		while (*newp == '/')
			newp++;

		p = newp;
	}

	return ppath;
}

static void
exchange_store_finalize (GObject *object)
{
	CamelExchangeStore *exch = CAMEL_EXCHANGE_STORE (object);

	g_free (exch->trash_name);

	if (exch->folders_lock)
		g_mutex_free (exch->folders_lock);

	if (exch->connect_lock)
		g_mutex_free (exch->connect_lock);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (camel_exchange_store_parent_class)->finalize (object);
}

static gboolean
exchange_store_construct (CamelService *service,
                          CamelSession *session,
                          CamelProvider *provider,
                          CamelURL *url,
                          GError **error)
{
	CamelExchangeStore *exch = CAMEL_EXCHANGE_STORE (service);
	CamelServiceClass *service_class;
	gchar *p;

	service_class = CAMEL_SERVICE_CLASS (
		camel_exchange_store_parent_class);

	if (!service_class->construct (service, session, provider, url, error))
		return FALSE;

	exch->base_url = camel_url_to_string (url, CAMEL_URL_HIDE_ALL);
	/* Strip path */
	p = strstr (exch->base_url, "//");
	if (p) {
		p = strchr (p + 2, '/');
		if (p)
			*p = '\0';
	}

	exch->storage_path = camel_session_get_storage_path (
		session, service, error);

	return (exch->storage_path != NULL);
}

static GList *
exchange_store_query_auth_types (CamelService *service,
                                 GError **error)
{
	GList *list = NULL;

	list = g_list_prepend (list, &camel_exchange_password_authtype);
	list = g_list_prepend (list, &camel_exchange_ntlm_authtype);

	return list;
}

static gchar *
exchange_store_get_name (CamelService *service,
                         gboolean brief)
{
	if (brief) {
		return g_strdup_printf (
			_("Exchange server %s"),
			service->url->host);
	} else {
		return g_strdup_printf (
			_("Exchange account for %s on %s"),
			service->url->user, service->url->host);
	}
}

static gboolean
exchange_store_connect (CamelService *service,
                        GError **error)
{
	CamelExchangeStore *exch = CAMEL_EXCHANGE_STORE (service);
	gchar *password = NULL;
	guint32 connect_status;
	gboolean online_mode = FALSE;
	CamelSession *session = camel_service_get_session (service);
	GError *local_error = NULL;

	/* This lock is only needed for offline operation.
	 * exchange_store_connect() is called many times in offline. */

	g_mutex_lock (exch->connect_lock);

	online_mode = camel_session_get_online (session);

	if (online_mode) {
		if (!service->url->passwd) {
			gchar *prompt;
			guint32 prompt_flags = CAMEL_SESSION_PASSWORD_SECRET;

			if (exch->reprompt_password)
				prompt_flags |= CAMEL_SESSION_PASSWORD_REPROMPT;

			prompt = camel_session_build_password_prompt (
				"Exchange", service->url->user, service->url->host);

			service->url->passwd = camel_session_get_password (
				session, service, "Exchange", prompt,
				"password", prompt_flags, error);

			g_free (prompt);

			exch->reprompt_password = service->url->passwd == NULL;
		}

		if (service->url->passwd == NULL) {
			g_mutex_unlock (exch->connect_lock);
			return FALSE;
		}

		password = service->url->passwd;
	}

	/* Initialize the stub connection */
	if (!camel_exchange_utils_connect (service, password, &connect_status, &local_error)) {
		g_clear_error (error);

		/* The user cancelled the connection attempt. */
		if (local_error == NULL)
			g_set_error (
				error, G_IO_ERROR,
				G_IO_ERROR_CANCELLED,
				"Cancelled");
		else
			g_propagate_error (error, local_error);
		g_mutex_unlock (exch->connect_lock);
		return FALSE;
	}

	if (!connect_status) {
		exch->reprompt_password = TRUE;

		if (service->url->passwd) {
			g_free (service->url->passwd);
			service->url->passwd = NULL;
		}

		g_clear_error (error);
		g_set_error (
			error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			_("Could not authenticate to server. "
			  "(Password incorrect?)"));
		g_mutex_unlock (exch->connect_lock);
		return FALSE;
	}

	g_mutex_unlock (exch->connect_lock);

	return TRUE;
}

static gboolean
exchange_store_disconnect (CamelService *service,
                           gboolean clean,
                           GError **error)
{
	/* CamelExchangeStore *exch = CAMEL_EXCHANGE_STORE (service); */
	/* keep account connect as it can be used for other parts like cal, gal or addressbook? */
	return TRUE;
}

static CamelFolder *
exchange_store_get_trash (CamelStore *store,
                          GError **error)
{
	CamelExchangeStore *exch = CAMEL_EXCHANGE_STORE (store);

	RETURN_VAL_IF_NOT_CONNECTED (exch, error, NULL);

	if (!exch->trash_name) {
		if (!camel_exchange_utils_get_trash_name (
			CAMEL_SERVICE (store), &exch->trash_name, error))
			return NULL;
	}

	return camel_store_get_folder (store, exch->trash_name, 0, error);
}

static CamelFolder *
exchange_store_get_folder (CamelStore *store,
                           const gchar *folder_name,
                           guint32 flags,
                           GError **error)
{
	CamelExchangeStore *exch = CAMEL_EXCHANGE_STORE (store);
	CamelFolder *folder;
	const gchar *short_name;
	gchar *folder_dir;

	RETURN_VAL_IF_NOT_CONNECTED (exch, error, NULL);

	if (!folder_name || !*folder_name || g_ascii_strcasecmp (folder_name, "inbox") == 0)
		folder_name = "personal/Inbox";

	folder_dir = exchange_store_path_to_physical (
		exch->storage_path, folder_name);

	if (!camel_exchange_store_connected (exch, NULL)) {
		if (!folder_dir || !g_file_test (folder_dir, G_FILE_TEST_IS_DIR)) {
			g_free (folder_dir);
			g_set_error (
				error, CAMEL_STORE_ERROR,
				CAMEL_STORE_ERROR_NO_FOLDER,
				_("No such folder %s"), folder_name);
			return NULL;
		}
	}

	g_mutex_lock (exch->folders_lock);
	folder = g_hash_table_lookup (exch->folders, folder_name);
	if (folder) {
		/* This shouldn't actually happen, it should be caught
		 * by the store-level cache...
		 */
		g_mutex_unlock (exch->folders_lock);
		g_object_ref (folder);
		g_free (folder_dir);
		return folder;
	}

	short_name = strrchr (folder_name, '/');
	if (!short_name++)
		short_name = folder_name;

	folder = g_object_new (
		CAMEL_TYPE_EXCHANGE_FOLDER,
		"name", short_name, "full-name", folder_name,
		"parent-store", store, NULL);
	g_hash_table_insert (exch->folders, g_strdup (folder_name), folder);
	g_mutex_unlock (exch->folders_lock);

	if (!camel_exchange_folder_construct (
		folder, flags, folder_dir,
		((CamelOfflineStore *) store)->state, error)) {

		gchar *key;
		g_mutex_lock (exch->folders_lock);
		if (g_hash_table_lookup_extended (exch->folders, folder_name,
						  (gpointer) &key, NULL)) {
			g_hash_table_remove (exch->folders, key);
			g_free (key);
		}
		g_mutex_unlock (exch->folders_lock);
		g_free (folder_dir);
		g_object_unref (folder);
		return NULL;
	}
	g_free (folder_dir);

	/* If you move messages into a folder you haven't visited yet, it
	 * may create and then unref the folder. That's a waste. So don't
	 * let that happen. Probably not the best fix...
	 */
	g_object_ref (folder);

	return folder;
}

static CamelFolderInfo *
exchange_store_get_folder_info (CamelStore *store,
                                const gchar *top,
                                guint32 flags,
                                GError **error)
{
	CamelExchangeStore *exch = CAMEL_EXCHANGE_STORE (store);
	GPtrArray *folders, *folder_names = NULL, *folder_uris = NULL;
	GArray *unread_counts = NULL;
	GArray *folder_flags = NULL;
	CamelFolderInfo *info;
	guint32 store_flags = 0;
	gint i;

	/* If the backend crashed, don't keep returning an error
	 * each time auto-send/recv runs.
	 */

	RETURN_VAL_IF_NOT_CONNECTED (exch, error, NULL);

	if (flags & CAMEL_STORE_FOLDER_INFO_RECURSIVE)
		store_flags |= CAMEL_STORE_FOLDER_INFO_RECURSIVE;
	if (flags & CAMEL_STORE_FOLDER_INFO_SUBSCRIBED)
		store_flags |= CAMEL_STORE_FOLDER_INFO_SUBSCRIBED;
	if (flags & CAMEL_STORE_FOLDER_INFO_SUBSCRIPTION_LIST)
		store_flags |= CAMEL_STORE_FOLDER_INFO_SUBSCRIPTION_LIST;

	if (!camel_exchange_utils_get_folder_info (
		CAMEL_SERVICE (store), top, store_flags, &folder_names,
		&folder_uris, &unread_counts, &folder_flags, error))
		return NULL;

	if (!folder_names) {
		/* This means the storage hasn't finished scanning yet.
		 * We return NULL for now and will emit folder_created
		 * events later.
		 */
		return NULL;
	}

	folders = g_ptr_array_new ();
	for (i = 0; i < folder_names->len; i++) {
		info = make_folder_info (exch, folder_names->pdata[i],
					 folder_uris->pdata[i],
					 g_array_index (unread_counts, gint, i),
					 g_array_index (folder_flags, gint, i));
		if (info)
			g_ptr_array_add (folders, info);
	}
	g_ptr_array_free (folder_names, TRUE);
	g_ptr_array_foreach (folder_uris, (GFunc) g_free, NULL);
	g_ptr_array_free (folder_uris, TRUE);
	g_array_free (unread_counts, TRUE);
	g_array_free (folder_flags, TRUE);

	info = camel_folder_info_build (folders, top, '/', TRUE);

	if (info)
		info = postprocess_tree (info);
	g_ptr_array_free (folders, TRUE);

	return info;
}

static CamelFolderInfo *
exchange_store_create_folder (CamelStore *store,
                              const gchar *parent_name,
                              const gchar *folder_name,
                              GError **error)
{
	CamelExchangeStore *exch = CAMEL_EXCHANGE_STORE (store);
	gchar *folder_uri;
	guint32 unread_count, flags;
	CamelFolderInfo *info;

	if (!camel_exchange_store_connected (exch, NULL)) {
		g_set_error (
			error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			_("Cannot create folder in offline mode."));
		return NULL;
	}

	if (!camel_exchange_utils_create_folder (
		CAMEL_SERVICE (store), parent_name, folder_name,
		&folder_uri, &unread_count, &flags, error))
		return NULL;

	info = make_folder_info (
		exch, g_strdup (folder_name),
		folder_uri, unread_count, flags);

	info->flags |= CAMEL_FOLDER_NOCHILDREN;

	g_free (folder_uri);

	return info;
}

static gboolean
exchange_store_delete_folder (CamelStore *store,
                              const gchar *folder_name,
                              GError **error)
{
	CamelExchangeStore *exch = CAMEL_EXCHANGE_STORE (store);

	if (!camel_exchange_store_connected (exch, NULL)) {
		g_set_error (
			error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			_("Cannot delete folder in offline mode."));
		return FALSE;
	}

	return camel_exchange_utils_delete_folder (
		CAMEL_SERVICE (store), folder_name, error);
}

static gboolean
exchange_store_rename_folder (CamelStore *store,
                              const gchar *old_name,
                              const gchar *new_name,
                              GError **error)
{
	GPtrArray *folders = NULL, *folder_names = NULL, *folder_uris = NULL;
	GArray *unread_counts = NULL;
	GArray *folder_flags = NULL;
	CamelFolderInfo *info;
	gint i;
	CamelFolder *folder;

	CamelExchangeStore *exch = CAMEL_EXCHANGE_STORE (store);

	if (!camel_exchange_store_connected (exch, NULL)) {
		g_set_error (
			error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			_("Cannot rename folder in offline mode."));
		return FALSE;
	}

	if (!camel_exchange_utils_rename_folder (
		CAMEL_SERVICE (store), old_name, new_name,
		&folder_names, &folder_uris, &unread_counts,
		&folder_flags, error))
		return FALSE;

	if (!folder_names) {
		/* This means the storage hasn't finished scanning yet.
		 * We return NULL for now and will emit folder_created
		 * events later.
		 */
		return TRUE;
	}

	folders = g_ptr_array_new ();
	for (i = 0; i < folder_names->len; i++) {
		info = make_folder_info (exch, folder_names->pdata[i],
					 folder_uris->pdata[i],
					 g_array_index (unread_counts, int, i),
					 g_array_index (folder_flags, int, i));
		if (info)
			g_ptr_array_add (folders, info);
	}
	g_ptr_array_free (folder_names, TRUE);
	g_ptr_array_foreach (folder_uris, (GFunc) g_free, NULL);
	g_ptr_array_free (folder_uris, TRUE);
	g_array_free (unread_counts, TRUE);
	g_array_free (folder_flags, TRUE);

	info = camel_folder_info_build (folders, new_name, '/', TRUE);

	if (info)
		info = postprocess_tree (info);
	g_ptr_array_free (folders, TRUE);

	g_mutex_lock (exch->folders_lock);
	folder = g_hash_table_lookup (exch->folders, old_name);
	if (folder) {
		g_hash_table_remove (exch->folders, old_name);
		g_object_unref (folder);
	}
	g_mutex_unlock (exch->folders_lock);

	camel_store_folder_renamed (CAMEL_STORE (exch), old_name, info);
	camel_folder_info_free (info);

	return TRUE;
}

static gboolean
exchange_store_folder_is_subscribed (CamelStore *store,
                                     const gchar *folder_name)
{
	gboolean is_subscribed = FALSE;

	d(printf ("is subscribed folder : %s\n", folder_name));
	if (((CamelOfflineStore *) store)->state == CAMEL_OFFLINE_STORE_NETWORK_UNAVAIL) {
		return FALSE;
	}

	if (!camel_exchange_utils_is_subscribed_folder (CAMEL_SERVICE (store), folder_name, &is_subscribed, NULL)) {
		return FALSE;
	}

	return is_subscribed;
}

static gboolean
exchange_store_subscribe_folder (CamelStore *store,
                                 const gchar *folder_name,
                                 GError **error)
{
	CamelExchangeStore *exch = CAMEL_EXCHANGE_STORE (store);

	d(printf ("subscribe folder : %s\n", folder_name));
	if (!camel_exchange_store_connected (exch, NULL)) {
		g_set_error (
			error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			_("Cannot subscribe folder in offline mode."));
		return FALSE;
	}

	return camel_exchange_utils_subscribe_folder (
		CAMEL_SERVICE (store), folder_name, error);
}

static gboolean
exchange_store_unsubscribe_folder (CamelStore *store,
                                   const gchar *folder_name,
                                   GError **error)
{
	CamelExchangeStore *exch = CAMEL_EXCHANGE_STORE (store);

	d(printf ("unsubscribe folder : %s\n", folder_name));
	if (!camel_exchange_store_connected (exch, NULL)) {
		g_set_error (
			error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			_("Cannot unsubscribe folder in offline mode."));
		return FALSE;
	}

	return camel_exchange_utils_unsubscribe_folder (
		CAMEL_SERVICE (store), folder_name, error);
}

static gboolean
exchange_store_can_refresh_folder (CamelStore *store,
                                   CamelFolderInfo *info,
                                   GError **error)
{
	CamelStoreClass *store_class;
	gboolean res;

	store_class = CAMEL_STORE_CLASS (camel_exchange_store_parent_class);

	res = store_class->can_refresh_folder (store, info, error) ||
	      (camel_url_get_param (((CamelService *)store)->url, "check_all") != NULL);

	return res;
}

static void
camel_exchange_store_class_init (CamelExchangeStoreClass *class)
{
	GObjectClass *object_class;
	CamelServiceClass *service_class;
	CamelStoreClass *store_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = exchange_store_finalize;

	service_class = CAMEL_SERVICE_CLASS (class);
	service_class->construct = exchange_store_construct;
	service_class->query_auth_types = exchange_store_query_auth_types;
	service_class->get_name = exchange_store_get_name;
	service_class->connect = exchange_store_connect;
	service_class->disconnect = exchange_store_disconnect;

	store_class = CAMEL_STORE_CLASS (class);
	store_class->get_trash = exchange_store_get_trash;
	store_class->free_folder_info = camel_store_free_folder_info_full;
	store_class->get_folder = exchange_store_get_folder;
	store_class->get_folder_info = exchange_store_get_folder_info;
	store_class->create_folder = exchange_store_create_folder;
	store_class->delete_folder = exchange_store_delete_folder;
	store_class->rename_folder = exchange_store_rename_folder;
	store_class->folder_is_subscribed = exchange_store_folder_is_subscribed;
	store_class->subscribe_folder = exchange_store_subscribe_folder;
	store_class->unsubscribe_folder = exchange_store_unsubscribe_folder;
	store_class->can_refresh_folder = exchange_store_can_refresh_folder;
}

static void
camel_exchange_store_init (CamelExchangeStore *exch)
{
	CamelStore *store = CAMEL_STORE (exch);

	exch->folders_lock = g_mutex_new ();
	exch->folders = g_hash_table_new (g_str_hash, g_str_equal);

	store->flags |= CAMEL_STORE_SUBSCRIPTIONS;
	store->flags &= ~(CAMEL_STORE_VTRASH | CAMEL_STORE_VJUNK);
	/* FIXME: Like the GroupWise provider, Exchange should also
	have its own EXCAHNGE_JUNK flags so as to rightly handle
	the actual Junk & Trash folders */

	exch->connect_lock = g_mutex_new ();
}

/* Use this to ensure that the camel session is online and we are connected
   too. Also returns the current status of the store */
gboolean
camel_exchange_store_connected (CamelExchangeStore *store, GError **error)
{
	CamelService *service;
	CamelSession *session;

	g_return_val_if_fail (CAMEL_IS_EXCHANGE_STORE (store), FALSE);

	service = CAMEL_SERVICE (store);
	session = service->session;

	if (service->status != CAMEL_SERVICE_CONNECTED &&
	    camel_session_get_online (session) &&
	    !camel_service_connect (service, error)) {
		return FALSE;
	}

	return CAMEL_OFFLINE_STORE (store)->state != CAMEL_OFFLINE_STORE_NETWORK_UNAVAIL;
}

void
camel_exchange_store_folder_created (CamelExchangeStore *estore, const gchar *name, const gchar *uri)
{
	CamelFolderInfo *info;

	g_return_if_fail (estore != NULL);
	g_return_if_fail (CAMEL_IS_EXCHANGE_STORE (estore));

	info = make_folder_info (estore, g_strdup (name), uri, -1, 0);
	info->flags |= CAMEL_FOLDER_NOCHILDREN;

	camel_store_folder_subscribed (CAMEL_STORE (estore), info);

	camel_folder_info_free (info);
}

void
camel_exchange_store_folder_deleted (CamelExchangeStore *estore, const gchar *name, const gchar *uri)
{
	CamelFolderInfo *info;
	CamelFolder *folder;

	g_return_if_fail (estore != NULL);
	g_return_if_fail (CAMEL_IS_EXCHANGE_STORE (estore));

	info = make_folder_info (estore, g_strdup (name), uri, -1, 0);

	g_mutex_lock (estore->folders_lock);
	folder = g_hash_table_lookup (estore->folders, info->full_name);
	if (folder) {
		g_hash_table_remove (estore->folders, info->full_name);
		g_object_unref (folder);
	}
	g_mutex_unlock (estore->folders_lock);

	camel_store_folder_unsubscribed (CAMEL_STORE (estore), info);

	camel_folder_info_free (info);
}
