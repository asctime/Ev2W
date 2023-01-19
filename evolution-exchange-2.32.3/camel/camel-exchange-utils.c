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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <exchange-constants.h>
#include <exchange-account.h>
#include <e-folder-exchange.h>
#include <e2k-propnames.h>
#include <e2k-restriction.h>
#include <e2k-uri.h>
#include <e2k-utils.h>
#include <exchange-hierarchy.h>
#include <mapi.h>

#include "camel-exchange-folder.h"
#include "camel-exchange-store.h"
#include "camel-exchange-summary.h"
#include "camel-exchange-utils.h"
#include "mail-utils.h"

#include "exchange-share-config-listener.h"

#define d(x)

typedef struct {
	/* the first two are set immediately, the rest after connect */
	CamelExchangeStore *estore;
	ExchangeAccount *account;
	GHashTable *folders_by_name;

	E2kContext *ctx;
	const gchar *mail_submission_uri;
	EFolder *inbox, *deleted_items, *sent_items;

	GStaticRecMutex changed_msgs_mutex;

	guint new_folder_id, removed_folder_id;
	const gchar *ignore_new_folder, *ignore_removed_folder;
} ExchangeData;

typedef struct {
	gchar *uid, *href;
	guint32 seq, flags;
	guint32 change_flags, change_mask;
	GData *tag_updates;
} ExchangeMessage;

typedef enum {
	EXCHANGE_FOLDER_REAL,
	EXCHANGE_FOLDER_POST,
	EXCHANGE_FOLDER_NOTES,
	EXCHANGE_FOLDER_OTHER
} ExchangeFolderType;

typedef struct {
	ExchangeData *ed;

	EFolder *folder;
	const gchar *name;
	ExchangeFolderType type;
	guint32 access;

	GPtrArray *messages;
	GHashTable *messages_by_uid, *messages_by_href;
	guint32 seq, high_article_num, deleted_count;

	guint32 unread_count;
	gboolean scanned;

	GPtrArray *changed_messages;
	guint flag_timeout, pending_delete_ops;

	time_t last_activity;
	guint sync_deletion_timeout;
} ExchangeFolder;

static const gchar *mapi_message_props[] = {
	E2K_PR_MAILHEADER_SUBJECT,
	E2K_PR_MAILHEADER_FROM,
	E2K_PR_MAILHEADER_TO,
	E2K_PR_MAILHEADER_CC,
	E2K_PR_MAILHEADER_DATE,
	E2K_PR_MAILHEADER_RECEIVED,
	E2K_PR_MAILHEADER_MESSAGE_ID,
	E2K_PR_MAILHEADER_IN_REPLY_TO,
	E2K_PR_MAILHEADER_REFERENCES,
	E2K_PR_MAILHEADER_THREAD_INDEX,
	E2K_PR_DAV_CONTENT_TYPE
};

static gboolean
is_same_ed (CamelExchangeStore *estore, ExchangeAccount *eaccount, CamelService *service)
{
	g_return_val_if_fail (eaccount != NULL, FALSE);
	g_return_val_if_fail (service != NULL, FALSE);
	g_return_val_if_fail (CAMEL_IS_SERVICE (service), FALSE);

	if (CAMEL_IS_EXCHANGE_STORE (service) && estore && estore == CAMEL_EXCHANGE_STORE (service))
		return TRUE;

	if (service->url) {
		if (estore && camel_url_equal (CAMEL_SERVICE (estore)->url, service->url))
			return TRUE;

		if (eaccount) {
			EAccount *account = exchange_account_fetch (eaccount);

			/* source url and transport url are same for exchange */
			if (account && e_account_get_string (account, E_ACCOUNT_SOURCE_URL)) {
				CamelURL *url = camel_url_new (e_account_get_string (account, E_ACCOUNT_SOURCE_URL), NULL);

				if (url) {
					CamelProvider *provider = camel_service_get_provider (service);

					if ((provider && provider->url_equal && provider->url_equal (url, service->url))
					    || camel_url_equal (url, service->url)) {
						camel_url_free (url);
						return TRUE;
					}

					camel_url_free (url);
				}
			}
		}
	}

	return FALSE;
}

static void free_folder (gpointer value);

static ExchangeData *
get_data_for_service (CamelService *service)
{
	static GSList *edies = NULL;
	G_LOCK_DEFINE_STATIC (edies);

	GSList *p, *accounts;
	ExchangeData *res = NULL;

	g_return_val_if_fail (service != NULL, NULL);
	g_return_val_if_fail (CAMEL_IS_SERVICE (service), NULL);

	G_LOCK (edies);
	for (p = edies; p; p = p->next) {
		ExchangeData *ed = p->data;

		if (ed && is_same_ed (ed->estore, ed->account, service)) {
			G_UNLOCK (edies);
			return ed;
		}
	}

	accounts = exchange_share_config_listener_get_accounts (exchange_share_config_listener_get_global ());
	for (p = accounts; p; p = p->next) {
		ExchangeAccount *a = p->data;

		if (a && is_same_ed (NULL, a, service)) {
			res = g_new0 (ExchangeData, 1);
			res->account = a;
			if (CAMEL_IS_EXCHANGE_STORE (service))
				res->estore = CAMEL_EXCHANGE_STORE (service);
			res->folders_by_name = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, free_folder);
			g_static_rec_mutex_init (&res->changed_msgs_mutex);

			edies = g_slist_prepend (edies, res);
			break;
		}
	}

	g_slist_free (accounts);

	G_UNLOCK (edies);

	return res;
}

static gint
is_online (ExchangeData *ed)
{
	CamelSession *session;

	g_return_val_if_fail (ed != NULL, OFFLINE_MODE);
	g_return_val_if_fail (ed->estore != NULL, OFFLINE_MODE);

	session = camel_service_get_session (CAMEL_SERVICE (ed->estore));
	g_return_val_if_fail (session != NULL, OFFLINE_MODE);

	return camel_session_get_online (session) ? ONLINE_MODE : OFFLINE_MODE;
}

static void
set_exception (GError **error, const gchar *err)
{
	g_return_if_fail (err != NULL);

	g_set_error (error, CAMEL_ERROR, CAMEL_ERROR_GENERIC, "%s", err);
}

static CamelFolder *
get_camel_folder (ExchangeFolder *mfld)
{
	CamelFolder *folder;

	g_return_val_if_fail (mfld != NULL, NULL);
	g_return_val_if_fail (mfld->name != NULL, NULL);
	g_return_val_if_fail (mfld->ed != NULL, NULL);
	g_return_val_if_fail (mfld->ed->estore != NULL, NULL);
	g_return_val_if_fail (mfld->ed->estore->folders != NULL, NULL);

	g_mutex_lock (mfld->ed->estore->folders_lock);
	folder = g_hash_table_lookup (mfld->ed->estore->folders, mfld->name);
	g_mutex_unlock (mfld->ed->estore->folders_lock);

	return folder;
}

static void
folder_changed (ExchangeFolder *mfld)
{
	e_folder_set_unread_count (mfld->folder, mfld->unread_count);
}

static gint
find_message_index (ExchangeFolder *mfld, gint seq)
{
	ExchangeMessage *mmsg;
	gint low, high, mid;

	low = 0;
	high = mfld->messages->len - 1;

	while (low <= high) {
		mid = (low + high) / 2;
		mmsg = mfld->messages->pdata[mid];
		if (seq == mmsg->seq)
			return mid;
		else if (seq < mmsg->seq)
			high = mid - 1;
		else
			low = mid + 1;
	}

	return -1;
}

static inline ExchangeMessage *
find_message (ExchangeFolder *mfld, const gchar *uid)
{
	return g_hash_table_lookup (mfld->messages_by_uid, uid);
}

static inline ExchangeMessage *
find_message_by_href (ExchangeFolder *mfld, const gchar *href)
{
	return g_hash_table_lookup (mfld->messages_by_href, href);
}

static ExchangeMessage *
new_message (const gchar *uid, const gchar *uri, guint32 seq, guint32 flags)
{
	ExchangeMessage *mmsg;

	mmsg = g_new0 (ExchangeMessage, 1);
	mmsg->uid = g_strdup (uid);
	mmsg->href = g_strdup (uri);
	mmsg->seq = seq;
	mmsg->flags = flags;

	return mmsg;
}

static void
message_remove_at_index (ExchangeFolder *mfld, CamelFolder *folder, gint index)
{
	ExchangeMessage *mmsg;
	CamelMessageInfo *info;

	mmsg = mfld->messages->pdata[index];
	d(printf("Deleting mmsg %p\n", mmsg));
	g_static_rec_mutex_lock (&mfld->ed->changed_msgs_mutex);
	g_ptr_array_remove_index (mfld->messages, index);
	g_hash_table_remove (mfld->messages_by_uid, mmsg->uid);
	if (mmsg->href)
		g_hash_table_remove (mfld->messages_by_href, mmsg->href);
	if (!(mmsg->flags & CAMEL_MESSAGE_SEEN)) {
		mfld->unread_count--;
		folder_changed (mfld);
	}
	g_static_rec_mutex_unlock (&mfld->ed->changed_msgs_mutex);

	if (mmsg->change_mask || mmsg->tag_updates) {
		gint i;

		g_static_rec_mutex_lock (&mfld->ed->changed_msgs_mutex);

		for (i = 0; i < mfld->changed_messages->len; i++) {
			if (mfld->changed_messages->pdata[i] == (gpointer)mmsg) {
				g_ptr_array_remove_index_fast (mfld->changed_messages, i);
				break;
			}
		}
		g_static_rec_mutex_unlock (&mfld->ed->changed_msgs_mutex);

		g_datalist_clear (&mmsg->tag_updates);
	}

	if (folder && (info = camel_folder_summary_uid (folder->summary, mmsg->uid))) {
		camel_message_info_free (info);
		camel_exchange_folder_remove_message (CAMEL_EXCHANGE_FOLDER (folder), mmsg->uid);
	}

	g_free (mmsg->uid);
	g_free (mmsg->href);
	g_free (mmsg);
}

static void
message_removed (ExchangeFolder *mfld, CamelFolder *folder, const gchar *href)
{
	ExchangeMessage *mmsg;
	guint index;

	g_static_rec_mutex_lock (&mfld->ed->changed_msgs_mutex);
	mmsg = g_hash_table_lookup (mfld->messages_by_href, href);
	if (!mmsg) {
		g_static_rec_mutex_unlock (&mfld->ed->changed_msgs_mutex);
		return;
	}
	index = find_message_index (mfld, mmsg->seq);
	g_return_if_fail (index != -1);

	message_remove_at_index (mfld, folder, index);
	g_static_rec_mutex_unlock (&mfld->ed->changed_msgs_mutex);
}

static const gchar *
uidstrip (const gchar *repl_uid)
{
	/* The first two cases are just to prevent crashes in the face
	 * of extreme lossage. They shouldn't ever happen, and the
	 * rest of the code probably won't work right if they do.
	 */
	if (strncmp (repl_uid, "rid:", 4))
		return repl_uid;
	else if (strlen (repl_uid) < 36)
		return repl_uid;
	else
		return repl_uid + 36;
}

struct refresh_message {
	gchar *uid, *href, *headers, *fff, *reply_by, *completed;
	guint32 flags, size, article_num;
};

static gint
refresh_message_compar (gconstpointer a, gconstpointer b)
{
	const struct refresh_message *rma = a, *rmb = b;

	return strcmp (rma->uid, rmb->uid);
}

static void
change_flags (ExchangeFolder *mfld, CamelFolder *folder, ExchangeMessage *mmsg, guint32 new_flags)
{
	if ((mmsg->flags ^ new_flags) & CAMEL_MESSAGE_SEEN) {
		if (mmsg->flags & CAMEL_MESSAGE_SEEN)
			mfld->unread_count++;
		else
			mfld->unread_count--;
		folder_changed (mfld);
	}
	mmsg->flags = new_flags;

	if (folder)
		camel_exchange_folder_update_message_flags (CAMEL_EXCHANGE_FOLDER (folder), mmsg->uid, mmsg->flags);
}

static void
refresh_folder_internal (ExchangeFolder *mfld, GError **error)
{
	static const gchar *new_message_props[] = {
		E2K_PR_REPL_UID,
		PR_INTERNET_ARTICLE_NUMBER,
		PR_TRANSPORT_MESSAGE_HEADERS,
		E2K_PR_HTTPMAIL_READ,
		E2K_PR_HTTPMAIL_HAS_ATTACHMENT,
		PR_ACTION_FLAG,
		PR_IMPORTANCE,
		PR_DELEGATED_BY_RULE,
		E2K_PR_HTTPMAIL_MESSAGE_FLAG,
		E2K_PR_MAILHEADER_REPLY_BY,
		E2K_PR_MAILHEADER_COMPLETED,
		E2K_PR_DAV_CONTENT_LENGTH
	};

	E2kRestriction *rn;
	GArray *messages;
	GHashTable *mapi_message_hash;
	GPtrArray *mapi_hrefs;
	gboolean has_read_flag = (mfld->access & MAPI_ACCESS_READ);
	E2kResultIter *iter;
	E2kResult *result;
	gchar *prop, *uid, *href;
	struct refresh_message rm, *rmp;
	E2kHTTPStatus status;
	gint got, total, i, n;
	gpointer key, value;
	ExchangeMessage *mmsg;
	CamelFolder *folder;

	if (is_online (mfld->ed) != ONLINE_MODE) {
		return;
	}

	messages = g_array_new (FALSE, FALSE, sizeof (struct refresh_message));
	mapi_message_hash = g_hash_table_new (g_str_hash, g_str_equal);
	mapi_hrefs = g_ptr_array_new ();

	/*
	 * STEP 1: Fetch information about new messages, including SMTP
	 * headers when available.
	 */

	rn = e2k_restriction_andv (
		e2k_restriction_prop_bool (E2K_PR_DAV_IS_COLLECTION,
					   E2K_RELOP_EQ, FALSE),
		e2k_restriction_prop_bool (E2K_PR_DAV_IS_HIDDEN,
					   E2K_RELOP_EQ, FALSE),
		e2k_restriction_prop_int (PR_INTERNET_ARTICLE_NUMBER,
					  E2K_RELOP_GT,
					  mfld->high_article_num),
		NULL);
	iter = e_folder_exchange_search_start (mfld->folder, NULL,
					       new_message_props,
					       G_N_ELEMENTS (new_message_props),
					       rn, NULL, TRUE);
	e2k_restriction_unref (rn);

	got = 0;
	total = e2k_result_iter_get_total (iter);
	while ((result = e2k_result_iter_next (iter))) {
		if (!E2K_HTTP_STATUS_IS_SUCCESSFUL (result->status)) {
			g_message ("%s: got unsuccessful at %s (%s)", G_STRFUNC, mfld->name, result->href ? result->href : "[null]");
			continue;
		}

		uid = e2k_properties_get_prop (result->props, E2K_PR_REPL_UID);
		if (!uid)
			continue;
		prop = e2k_properties_get_prop (result->props,
						PR_INTERNET_ARTICLE_NUMBER);
		if (!prop)
			continue;

		rm.uid = g_strdup (uidstrip (uid));
		rm.href = g_strdup (result->href);
		rm.article_num = strtoul (prop, NULL, 10);

		rm.flags = mail_util_props_to_camel_flags (result->props,
							   has_read_flag);

		prop = e2k_properties_get_prop (result->props,
						E2K_PR_HTTPMAIL_MESSAGE_FLAG);
		if (prop)
			rm.fff = g_strdup (prop);
		else
			rm.fff = NULL;
		prop = e2k_properties_get_prop (result->props,
						E2K_PR_MAILHEADER_REPLY_BY);
		if (prop)
			rm.reply_by = g_strdup (prop);
		else
			rm.reply_by = NULL;
		prop = e2k_properties_get_prop (result->props,
						E2K_PR_MAILHEADER_COMPLETED);
		if (prop)
			rm.completed = g_strdup (prop);
		else
			rm.completed = NULL;

		prop = e2k_properties_get_prop (result->props,
						E2K_PR_DAV_CONTENT_LENGTH);
		rm.size = prop ? strtoul (prop, NULL, 10) : 0;

		rm.headers = mail_util_extract_transport_headers (result->props);

		g_array_append_val (messages, rm);

		if (rm.headers) {
			got++;
			camel_operation_progress (NULL, (got * 100) / total);
		} else {
			href = strrchr (rm.href, '/');
			if (!href++)
				href = rm.href;

			g_hash_table_insert (mapi_message_hash, href,
					     GINT_TO_POINTER (messages->len - 1));
			g_ptr_array_add (mapi_hrefs, href);
		}
	}
	status = e2k_result_iter_free (iter);

	if (!E2K_HTTP_STATUS_IS_SUCCESSFUL (status)) {
		g_warning ("got_new_smtp_messages: %d", status);
		set_exception (error, _("Could not get new messages"));
		goto done;
	}

	if (mapi_hrefs->len == 0)
		goto return_data;

	/*
	 * STEP 2: Fetch MAPI property data for non-SMTP messages.
	 */

	iter = e_folder_exchange_bpropfind_start (mfld->folder, NULL,
						  (const gchar **)mapi_hrefs->pdata,
						  mapi_hrefs->len,
						  mapi_message_props,
						  G_N_ELEMENTS (mapi_message_props));
	while ((result = e2k_result_iter_next (iter))) {
		if (!E2K_HTTP_STATUS_IS_SUCCESSFUL (result->status))
			continue;

		href = strrchr (result->href, '/');
		if (!href++)
			href = result->href;

		if (!g_hash_table_lookup_extended (mapi_message_hash, href,
						   &key, &value))
			continue;
		n = GPOINTER_TO_INT (value);

		rmp = &((struct refresh_message *)messages->data)[n];
		rmp->headers = mail_util_mapi_to_smtp_headers (result->props);

		got++;
		camel_operation_progress (NULL, (got * 100) / total);
	}
	status = e2k_result_iter_free (iter);

	if (!E2K_HTTP_STATUS_IS_SUCCESSFUL (status)) {
		g_warning ("got_new_mapi_messages: %d", status);
		set_exception (error, _("Could not get new messages"));
		goto done;
	}

	/*
	 * STEP 3: Organize the data, update our records and Camel's
	 */

 return_data:
	camel_operation_progress (NULL, 100);
	folder = get_camel_folder (mfld);
	if (folder)
		camel_folder_freeze (folder);

	g_static_rec_mutex_lock (&mfld->ed->changed_msgs_mutex);
	qsort (messages->data, messages->len,
	       sizeof (rm), refresh_message_compar);
	for (i = 0; i < messages->len; i++) {
		rm = g_array_index (messages, struct refresh_message, i);

		/* If we already have a message with this UID, then
		 * that means it's not a new message, it's just that
		 * the article number changed.
		 */
		mmsg = find_message (mfld, rm.uid);
		if (mmsg) {
			if (rm.flags != mmsg->flags)
				change_flags (mfld, folder, mmsg, rm.flags);
		} else {
			if (g_hash_table_lookup (mfld->messages_by_href, rm.href)) {
				mfld->deleted_count++;
				message_removed (mfld, folder, rm.href);
			}

			mmsg = new_message (rm.uid, rm.href, mfld->seq++, rm.flags);
			g_ptr_array_add (mfld->messages, mmsg);
			g_hash_table_insert (mfld->messages_by_uid, mmsg->uid, mmsg);
			g_hash_table_insert (mfld->messages_by_href, mmsg->href, mmsg);

			if (!(mmsg->flags & CAMEL_MESSAGE_SEEN))
				mfld->unread_count++;

			if (folder)
				camel_exchange_folder_add_message (CAMEL_EXCHANGE_FOLDER (folder), rm.uid, rm.flags, rm.size, rm.headers, rm.href);
		}

		if (rm.article_num > mfld->high_article_num) {
			mfld->high_article_num = rm.article_num;
			if (folder)
				camel_exchange_summary_set_article_num (folder->summary, mfld->high_article_num);
		}

		if (rm.fff && folder)
			camel_exchange_folder_update_message_tag (CAMEL_EXCHANGE_FOLDER (folder), rm.uid, "follow-up", rm.fff);
		if (rm.reply_by && folder)
			camel_exchange_folder_update_message_tag (CAMEL_EXCHANGE_FOLDER (folder), rm.uid, "due-by", rm.reply_by);
		if (rm.completed && folder)
			camel_exchange_folder_update_message_tag (CAMEL_EXCHANGE_FOLDER (folder), rm.uid, "completed-on", rm.completed);
	}

	if (folder)
		camel_folder_thaw (folder);

	mfld->scanned = TRUE;
	g_static_rec_mutex_unlock (&mfld->ed->changed_msgs_mutex);
	folder_changed (mfld);

 done:
	/*
	 * CLEANUP
	 */
	rmp = (struct refresh_message *)messages->data;
	for (i = 0; i < messages->len; i++) {
		g_free (rmp[i].uid);
		g_free (rmp[i].href);
		g_free (rmp[i].headers);
		g_free (rmp[i].fff);
		g_free (rmp[i].reply_by);
		g_free (rmp[i].completed);
	}
	g_array_free (messages, TRUE);

	g_hash_table_destroy (mapi_message_hash);
	g_ptr_array_free (mapi_hrefs, TRUE);
}

static void
sync_deletions (ExchangeFolder *mfld)
{
	static const gchar *sync_deleted_props[] = {
		PR_DELETED_COUNT_TOTAL,
		E2K_PR_DAV_VISIBLE_COUNT
	};

	E2kHTTPStatus status;
	E2kResult *results;
	gint nresults = 0;
	const gchar *prop;
	gint deleted_count = -1, visible_count = -1;
	E2kRestriction *rn;
	E2kResultIter *iter;
	E2kResult *result;
	gint my_i, read;
	ExchangeMessage *mmsg;
	GHashTable *known_messages;
	CamelFolder *folder;

	g_return_if_fail (mfld != NULL);
	g_return_if_fail (mfld->ed != NULL);

	if (is_online (mfld->ed) != ONLINE_MODE)
		return;

	status = e_folder_exchange_propfind (mfld->folder, NULL,
					     sync_deleted_props,
					     G_N_ELEMENTS (sync_deleted_props),
					     &results, &nresults);

	if (!E2K_HTTP_STATUS_IS_SUCCESSFUL (status) || !nresults) {
		g_warning ("got_sync_deleted_props: %d", status);
		return;
	}

	prop = e2k_properties_get_prop (results[0].props,
					PR_DELETED_COUNT_TOTAL);
	if (prop)
		deleted_count = atoi (prop);

	prop = e2k_properties_get_prop (results[0].props,
					E2K_PR_DAV_VISIBLE_COUNT);
	if (prop)
		visible_count = atoi (prop);

	e2k_results_free (results, nresults);

	g_static_rec_mutex_lock (&mfld->ed->changed_msgs_mutex);
	if (visible_count >= mfld->messages->len) {
		if (mfld->deleted_count == deleted_count) {
			g_static_rec_mutex_unlock (&mfld->ed->changed_msgs_mutex);
			return;
		}

		if (mfld->deleted_count == 0) {
			mfld->deleted_count = deleted_count;
			g_static_rec_mutex_unlock (&mfld->ed->changed_msgs_mutex);
			return;
		}
	}

	prop = E2K_PR_HTTPMAIL_READ;
	rn = e2k_restriction_andv (
		e2k_restriction_prop_bool (E2K_PR_DAV_IS_COLLECTION,
					   E2K_RELOP_EQ, FALSE),
		e2k_restriction_prop_bool (E2K_PR_DAV_IS_HIDDEN,
					   E2K_RELOP_EQ, FALSE),
		NULL);

	iter = e_folder_exchange_search_start (mfld->folder, NULL,
					       &prop, 1, rn,
					       E2K_PR_DAV_CREATION_DATE,
					       FALSE);
	e2k_restriction_unref (rn);

	known_messages = g_hash_table_new (g_direct_hash, g_direct_equal);

	folder = get_camel_folder (mfld);

	my_i = mfld->messages->len - 1;
	while ((result = e2k_result_iter_next (iter))) {
		mmsg = find_message_by_href (mfld, result->href);
		if (!mmsg) {
			/* oops, message from the server not found in our list;
			   return failure to possibly do full resync again? */
			g_message ("%s: Oops, message %s not found in %s", G_STRFUNC, result->href, mfld->name);
			continue;
		}

		g_hash_table_insert (known_messages, mmsg, mmsg);

		/* See if its read flag changed while we weren't watching */
		prop = e2k_properties_get_prop (result->props,
						E2K_PR_HTTPMAIL_READ);
		read = (prop && atoi (prop)) ? CAMEL_MESSAGE_SEEN : 0;
		if ((mmsg->flags & CAMEL_MESSAGE_SEEN) != read) {
			change_flags (mfld, folder, mmsg, mmsg->flags ^ CAMEL_MESSAGE_SEEN);
		}

	}
	status = e2k_result_iter_free (iter);

	if (!E2K_HTTP_STATUS_IS_SUCCESSFUL (status))
		g_warning ("synced_deleted: %d", status);

	/* Clear out removed messages from mfld */
	for (my_i = mfld->messages->len - 1; my_i >= 0; my_i--) {
		mmsg = mfld->messages->pdata[my_i];
		if (!g_hash_table_lookup (known_messages, mmsg)) {
			mfld->deleted_count++;
			message_remove_at_index (mfld, folder, my_i);
		}
	}

	g_hash_table_destroy (known_messages);
	g_static_rec_mutex_unlock (&mfld->ed->changed_msgs_mutex);
}

static void
storage_folder_changed (EFolder *folder, gpointer user_data)
{
	ExchangeFolder *mfld = user_data;

	if (e_folder_get_unread_count (folder) > mfld->unread_count)
		refresh_folder_internal (mfld, NULL);
}

static void
free_message (ExchangeMessage *mmsg)
{
	g_datalist_clear (&mmsg->tag_updates);
	g_free (mmsg->uid);
	g_free (mmsg->href);
	g_free (mmsg);
}

static void
free_folder (gpointer value)
{
	ExchangeFolder *mfld = value;
	gint i;

	d(g_print ("%s:%s:%d: freeing mfld: name=[%s]\n", __FILE__, __PRETTY_FUNCTION__, __LINE__,
		   mfld->name));

	e_folder_exchange_unsubscribe (mfld->folder);
	g_signal_handlers_disconnect_by_func (mfld->folder, storage_folder_changed, mfld);
	g_object_unref (mfld->folder);
	mfld->folder = NULL;

	for (i = 0; i < mfld->messages->len; i++)
		free_message (mfld->messages->pdata[i]);
	g_ptr_array_free (mfld->messages, TRUE);
	g_hash_table_destroy (mfld->messages_by_uid);
	g_hash_table_destroy (mfld->messages_by_href);

	g_ptr_array_free (mfld->changed_messages, TRUE);
	if (mfld->flag_timeout) {
		g_warning ("unreffing mfld with unsynced flags");
		g_source_remove (mfld->flag_timeout);
	}
	if (mfld->sync_deletion_timeout)
		g_source_remove (mfld->sync_deletion_timeout);
	g_free (mfld);
}

static void
got_folder_error (ExchangeFolder *mfld, GError **error, const gchar *err)
{
	set_exception (error, err);

	g_return_if_fail (mfld != NULL);
	g_return_if_fail (mfld->ed != NULL);

	/* DO NOT remove folder here, it is pretty confused when gets back online */

	/* this also calls free_folder() */
	/* g_hash_table_remove (mfld->ed->folders_by_name, mfld->name); */
}

static void
mfld_get_folder_online_sync_updates (gpointer key, gpointer value, gpointer user_data)
{
	guint index, seq, i;
	ExchangeFolder *mfld = (ExchangeFolder *) user_data;
	ExchangeMessage *mmsg = NULL;

	index = GPOINTER_TO_UINT (key);
	seq = GPOINTER_TO_UINT (value);

	g_static_rec_mutex_lock (&mfld->ed->changed_msgs_mutex);

	/* Camel DB Summary changes are not fetching all the messages at start-up.
	   Use this else it would crash badly.
	*/
	if (index >= mfld->messages->len) {
		g_static_rec_mutex_unlock (&mfld->ed->changed_msgs_mutex);
		return;
	}

	mmsg = mfld->messages->pdata[index];
	if (mmsg->seq != seq) {
		for (i = 0; i < mfld->messages->len; i++) {
			mmsg = mfld->messages->pdata[i];
			if (mmsg->seq == seq)
				break;
		}
		seq = i;
	}
	g_static_rec_mutex_unlock (&mfld->ed->changed_msgs_mutex);

	/* FIXME FIXME FIXME: Some miscalculation happens here,though,
	not a serious one
	*/
	/* message_remove_at_index already handles lock/unlock */
	/*message_remove_at_index (stub, mfld, seq);*/

}

static gboolean
get_folder_contents_online (ExchangeFolder *mfld, GError **error)
{
	static const gchar *open_folder_sync_props[] = {
		E2K_PR_REPL_UID,
		PR_INTERNET_ARTICLE_NUMBER,
		PR_ACTION_FLAG,
		PR_IMPORTANCE,
		PR_DELEGATED_BY_RULE,
		E2K_PR_HTTPMAIL_READ,
		E2K_PR_HTTPMAIL_MESSAGE_FLAG,
		E2K_PR_MAILHEADER_REPLY_BY,
		E2K_PR_MAILHEADER_COMPLETED
	};

	ExchangeMessage *mmsg, *mmsg_cpy;
	E2kHTTPStatus status;
	gboolean readonly = FALSE;
	E2kRestriction *rn;
	E2kResultIter *iter;
	E2kResult *result;
	const gchar *prop, *uid;
	guint32 article_num, camel_flags, high_article_num;
	gint i, total = -1;
	guint m;
	CamelFolder *folder;

	GPtrArray *msgs_copy = NULL;
	GHashTable *rm_idx_uid = NULL;

	/* Make a copy of the mfld->messages array for our processing */
	msgs_copy = g_ptr_array_new ();

	/* Store the index/seq of the messages to be removed from mfld->messages */
	rm_idx_uid = g_hash_table_new (g_direct_hash, g_direct_equal);

	g_static_rec_mutex_lock (&mfld->ed->changed_msgs_mutex);
	for (i = 0; i < mfld->messages->len; i++) {
		mmsg = mfld->messages->pdata[i];
		mmsg_cpy = new_message (mmsg->uid, mmsg->href, mmsg->seq, mmsg->flags);
		g_ptr_array_add (msgs_copy, mmsg_cpy);
	}
	high_article_num = 0;
	g_static_rec_mutex_unlock (&mfld->ed->changed_msgs_mutex);

	rn = e2k_restriction_andv (
		e2k_restriction_prop_bool (E2K_PR_DAV_IS_COLLECTION,
					   E2K_RELOP_EQ, FALSE),
		e2k_restriction_prop_bool (E2K_PR_DAV_IS_HIDDEN,
					   E2K_RELOP_EQ, FALSE),
		NULL);

	iter = e_folder_exchange_search_start (mfld->folder, NULL,
					       open_folder_sync_props,
					       G_N_ELEMENTS (open_folder_sync_props),
					       rn, E2K_PR_DAV_CREATION_DATE,
					       TRUE);
	e2k_restriction_unref (rn);

	folder = get_camel_folder (mfld);

	m = 0;
	total = e2k_result_iter_get_total (iter);
	while (m < msgs_copy->len && (result = e2k_result_iter_next (iter))) {
		prop = e2k_properties_get_prop (result->props,
						PR_INTERNET_ARTICLE_NUMBER);
		if (!prop)
			continue;
		article_num = strtoul (prop, NULL, 10);

		prop = e2k_properties_get_prop (result->props,
						E2K_PR_REPL_UID);
		if (!prop)
			continue;
		uid = uidstrip (prop);

		camel_flags = mail_util_props_to_camel_flags (result->props,
							      !readonly);

		mmsg_cpy = msgs_copy->pdata[m];
		while (strcmp (uid, mmsg_cpy->uid)) {
			/* Remove mmsg from our msgs_copy array */
			g_ptr_array_remove_index (msgs_copy, m);

			/* Put the index/uid as key/value in the rm_idx_uid hashtable.
			   This hashtable will be used to sync with mfld->messages.
			 */
			g_hash_table_insert (rm_idx_uid, GUINT_TO_POINTER(m),
					     GUINT_TO_POINTER(mmsg_cpy->seq));
			g_free (mmsg_cpy->uid);
			g_free (mmsg_cpy->href);
			g_free (mmsg_cpy);

			if (m == msgs_copy->len) {
				mmsg_cpy = NULL;
				if (article_num < high_article_num)
					high_article_num = article_num - 1;
				break;
			}
			mmsg_cpy = msgs_copy->pdata[m];
		}
		if (!mmsg_cpy)
			break;

		if (article_num > high_article_num)
			high_article_num = article_num;

		g_static_rec_mutex_lock (&mfld->ed->changed_msgs_mutex);
		mmsg = mfld->messages->pdata[m];

		/* Validate mmsg == mmsg_cpy - this may fail if user has deleted some messages,
		   while we were updating in a separate thread.
		*/
		if (mmsg->seq != mmsg_cpy->seq) {
			/* We don't want to scan all of mfld->messages, as some new messages
			   would have got added to the array and hence restrict to the original
			   array of messages that we loaded from summary.
			*/
			for (i = 0; i < msgs_copy->len; i++) {
				mmsg = mfld->messages->pdata[i];
				if (mmsg->seq == mmsg_cpy->seq)
					break;
			}
		}

		if (!mmsg->href) {
			mmsg->href = g_strdup (result->href);
			if (mmsg_cpy->href)
				g_free (mmsg_cpy->href);
			mmsg_cpy->href = g_strdup (result->href);
			/* Do not allow duplicates */
			if (!g_hash_table_lookup (mfld->messages_by_href, mmsg->href))
				g_hash_table_insert (mfld->messages_by_href, mmsg->href, mmsg);
		}

		if (mmsg->flags != camel_flags)
			change_flags (mfld, folder, mmsg, camel_flags);

		g_static_rec_mutex_unlock (&mfld->ed->changed_msgs_mutex);

		if (article_num > high_article_num)
			high_article_num = article_num;

		prop = e2k_properties_get_prop (result->props, E2K_PR_HTTPMAIL_MESSAGE_FLAG);
		if (prop && folder)
			camel_exchange_folder_update_message_tag (CAMEL_EXCHANGE_FOLDER (folder), mmsg->uid, "follow-up", prop);
		prop = e2k_properties_get_prop (result->props, E2K_PR_MAILHEADER_REPLY_BY);
		if (prop && folder)
			camel_exchange_folder_update_message_tag (CAMEL_EXCHANGE_FOLDER (folder), mmsg->uid, "due-by", prop);
		prop = e2k_properties_get_prop (result->props, E2K_PR_MAILHEADER_COMPLETED);
		if (prop && folder)
			camel_exchange_folder_update_message_tag (CAMEL_EXCHANGE_FOLDER (folder), mmsg->uid, "completed-on", prop);

		m++;
#if 0
		if (ex) {
			camel_operation_progress (NULL, (m * 100) / total);
		}
#endif
	}

	/* If there are further messages beyond mfld->messages->len,
	 * then that means camel doesn't know about them yet, and so
	 * we need to ignore them for a while. But if any of them have
	 * an article number lower than the highest article number
	 * we've seen, bump high_article_num down so that that message
	 * gets caught by refresh_info later too.
	 */
	while ((result = e2k_result_iter_next (iter))) {
		prop = e2k_properties_get_prop (result->props,
						PR_INTERNET_ARTICLE_NUMBER);
		if (prop) {
			article_num = strtoul (prop, NULL, 10);
			if (article_num <= high_article_num)
				high_article_num = article_num - 1;
		}

		m++;
#if 0
		if (ex) {
			camel_operation_progress (NULL, (m * 100) / total);
		}
#endif
	}
	status = e2k_result_iter_free (iter);
	if (!E2K_HTTP_STATUS_IS_SUCCESSFUL (status)) {
		g_warning ("got_folder: %d", status);
		got_folder_error (mfld, error, _("Could not open folder"));
		return FALSE;
	}

	/* Discard remaining messages that no longer exist.
	   Do not increment 'i', because the remove_index is decrementing array length. */
	for (i = 0; i < msgs_copy->len;) {
		mmsg_cpy = msgs_copy->pdata[i];
		if (!mmsg_cpy->href) {
			/* Put the index/uid as key/value in the rm_idx_uid hashtable.
			   This hashtable will be used to sync with mfld->messages.
			 */
			g_hash_table_insert (rm_idx_uid, GUINT_TO_POINTER(m),
					     GUINT_TO_POINTER(mmsg_cpy->seq));
		}

		/* Remove mmsg from our msgs_copy array */
		g_ptr_array_remove_index (msgs_copy, i);

		g_free (mmsg_cpy->uid);
		g_free (mmsg_cpy->href);
		g_free (mmsg_cpy);
	}

	g_static_rec_mutex_lock (&mfld->ed->changed_msgs_mutex);
	mfld->high_article_num = high_article_num;
	g_static_rec_mutex_unlock (&mfld->ed->changed_msgs_mutex);

	if (folder)
		camel_exchange_summary_set_article_num (folder->summary, mfld->high_article_num);

	g_hash_table_foreach (rm_idx_uid, mfld_get_folder_online_sync_updates, mfld);

	g_ptr_array_free (msgs_copy, TRUE);
	g_hash_table_destroy (rm_idx_uid);

	return TRUE;
}

static gpointer
get_folder_contents_online_func (gpointer data)
{
	ExchangeFolder *mfld = data;

	if (!mfld)
		return NULL;

	get_folder_contents_online (mfld, NULL);

	return NULL;
}

#define FIVE_SECONDS (5)
#define  ONE_MINUTE  (60)
#define FIVE_MINUTES (60*5)

static gboolean
timeout_sync_deletions (gpointer user_data)
{
	ExchangeFolder *mfld = user_data;

	sync_deletions (mfld);
	return FALSE;
}

static void
notify_cb (E2kContext *ctx, const gchar *uri, E2kContextChangeType type, gpointer user_data)
{
	ExchangeFolder *mfld = user_data;
	time_t now;

	if (type == E2K_CONTEXT_OBJECT_ADDED)
		refresh_folder_internal (mfld, NULL);
	else {
		now = time (NULL);

		/* If the user did something in Evolution in the
		 * last 5 seconds, assume that this notification is
		 * a result of that and ignore it.
		 */
		if (now < mfld->last_activity + FIVE_SECONDS)
			return;

		/* sync_deletions() is somewhat server-intensive, so
		 * we don't want to run it unnecessarily. In
		 * particular, if the user leaves Evolution running,
		 * goes home for the night, and then reads mail from
		 * home, we don't want to run sync_deletions() every
		 * time the user deletes a message; we just need to
		 * make sure we do it by the time the user gets back
		 * in the morning. On the other hand, if the user just
		 * switches to Outlook for just a moment and then
		 * comes back, we'd like to update fairly quickly.
		 *
		 * So, if the user has been idle for less than a
		 * minute, we update right away. Otherwise, we set a
		 * timer, and keep resetting it with each new
		 * notification, meaning we (hopefully) only sync
		 * after the user stops changing things.
		 *
		 * If the user returns to Evolution while we have a
		 * timer set, then folder_from_name() will immediately
		 * call sync_deletions.
		 */

		if (mfld->sync_deletion_timeout) {
			g_source_remove (mfld->sync_deletion_timeout);
			mfld->sync_deletion_timeout = 0;
		}

		if (now < mfld->last_activity + ONE_MINUTE)
			sync_deletions (mfld);
		else if (now < mfld->last_activity + FIVE_MINUTES) {
			mfld->sync_deletion_timeout =
				g_timeout_add (ONE_MINUTE * 1000,
					       timeout_sync_deletions,
					       mfld);
		} else {
			mfld->sync_deletion_timeout =
				g_timeout_add (FIVE_MINUTES * 1000,
					       timeout_sync_deletions,
					       mfld);
		}
	}
}

static gboolean
get_folder_online (ExchangeFolder *mfld, GError **error)
{
	static const gchar *open_folder_props[] = {
		PR_ACCESS,
		PR_DELETED_COUNT_TOTAL
	};

	E2kHTTPStatus status;
	E2kResult *results;
	gint nresults = 0;
	gboolean readonly;
	const gchar *prop;

	mfld->changed_messages = g_ptr_array_new ();

	status = e_folder_exchange_propfind (mfld->folder, NULL,
					     open_folder_props,
					     G_N_ELEMENTS (open_folder_props),
					     &results, &nresults);
	if (status == E2K_HTTP_UNAUTHORIZED) {
		got_folder_error (mfld, error, _("Could not open folder: Permission denied"));
		return FALSE;
	} else if (!E2K_HTTP_STATUS_IS_SUCCESSFUL (status)) {
		g_warning ("got_folder_props: %d", status);
		got_folder_error (mfld, error, _("Could not open folder"));
		return FALSE;
	}

	if (nresults) {
		prop = e2k_properties_get_prop (results[0].props, PR_ACCESS);
		if (prop)
			mfld->access = atoi (prop);
		else
			mfld->access = ~0;
	} else
		mfld->access = ~0;

	if (!(mfld->access & MAPI_ACCESS_READ)) {
		got_folder_error (mfld, error, _("Could not open folder: Permission denied"));
		if (nresults)
			e2k_results_free (results, nresults);
		return FALSE;
	}
	readonly = (mfld->access & (MAPI_ACCESS_MODIFY | MAPI_ACCESS_CREATE_CONTENTS)) == 0;

	prop = e2k_properties_get_prop (results[0].props, PR_DELETED_COUNT_TOTAL);
	if (prop)
		mfld->deleted_count = atoi (prop);

	/*
	   TODO: Varadhan - June 16, 2007 - Compare deleted_count with
	   that of CamelFolder and appropriately sync mfld->messages.
	   Also, sync flags and camel_flags of all messages - No reliable
	   way to fetch only changed messages as Read/UnRead flags do not
	   change the PR_LAST_MODIFICATION_TIME property of a message.
	*/
	if (g_hash_table_size (mfld->messages_by_href) < 1) {
		if (!get_folder_contents_online (mfld, error))
			return FALSE;
	} else {
		/* FIXME: Pass a GError and handle the error */
		g_thread_create (get_folder_contents_online_func,
				 mfld, FALSE, NULL);
	}

	e_folder_exchange_subscribe (mfld->folder,
				     E2K_CONTEXT_OBJECT_ADDED, 30,
				     notify_cb, mfld);
	e_folder_exchange_subscribe (mfld->folder,
				     E2K_CONTEXT_OBJECT_REMOVED, 30,
				     notify_cb, mfld);
	e_folder_exchange_subscribe (mfld->folder,
				     E2K_CONTEXT_OBJECT_MOVED, 30,
				     notify_cb, mfld);
	if (nresults)
		e2k_results_free (results, nresults);

	return TRUE;
}

static ExchangeFolder *
folder_from_name (ExchangeData *ed, const gchar *folder_name, guint32 perms, GError **error)
{
	ExchangeFolder *mfld;

	mfld = g_hash_table_lookup (ed->folders_by_name, folder_name);
	if (!mfld) {
		set_exception (error, _("No such folder"));
		return NULL;
	}

	/* If sync_deletion_timeout is set, that means the user has been
	 * idle in Evolution for longer than a minute, during which
	 * time he has deleted messages using another email client,
	 * which we haven't bothered to sync up with yet. Do that now.
	 */
	if (mfld->sync_deletion_timeout) {
		g_source_remove (mfld->sync_deletion_timeout);
		mfld->sync_deletion_timeout = 0;
		sync_deletions (mfld);
	}

	if ((perms == MAPI_ACCESS_MODIFY || perms == MAPI_ACCESS_DELETE) &&
	    !(mfld->access & perms)) {
		/* try with MAPI_ACCESS_CREATE_CONTENTS */
		perms = MAPI_ACCESS_CREATE_CONTENTS;
	}

	if (perms && !(mfld->access & perms)) {
		set_exception (error, _("Permission denied"));
		return NULL;
	}

	mfld->last_activity = time (NULL);
	return mfld;
}

static void
mark_one_read (E2kContext *ctx, const gchar *uri, gboolean read)
{
	E2kProperties *props;
	E2kHTTPStatus status;

	props = e2k_properties_new ();
	e2k_properties_set_bool (props, E2K_PR_HTTPMAIL_READ, read);

	status = e2k_context_proppatch (ctx, NULL, uri, props, FALSE, NULL);
	e2k_properties_free (props);
	if (!E2K_HTTP_STATUS_IS_SUCCESSFUL (status))
		g_warning ("mark_one_read: %d", status);
}

static void
mark_read (EFolder *folder, GPtrArray *hrefs, gboolean read)
{
	E2kProperties *props;
	E2kResultIter *iter;
	E2kHTTPStatus status;

	props = e2k_properties_new ();
	e2k_properties_set_bool (props, E2K_PR_HTTPMAIL_READ, read);

	iter = e_folder_exchange_bproppatch_start (folder, NULL,
						   (const gchar **)hrefs->pdata,
						   hrefs->len, props, FALSE);
	e2k_properties_free (props);

	while (e2k_result_iter_next (iter))
		;
	status = e2k_result_iter_free (iter);

	if (!E2K_HTTP_STATUS_IS_SUCCESSFUL (status))
		g_warning ("mark_read: %d", status);
}

static gboolean
process_flags (gpointer user_data)
{
	ExchangeFolder *mfld = user_data;
	ExchangeData *ed = mfld->ed;
	ExchangeMessage *mmsg;
	GPtrArray *seen = NULL, *unseen = NULL, *deleted = NULL;
	gint i;
	guint32 hier_type = e_folder_exchange_get_hierarchy (mfld->folder)->type;

	g_static_rec_mutex_lock (&ed->changed_msgs_mutex);

	for (i = 0; i < mfld->changed_messages->len; i++) {
		mmsg = mfld->changed_messages->pdata[i];
		d(printf("Process flags %p\n", mmsg));
		if (!mmsg->href) {
			d(g_print ("%s:%s:%d: mfld = [%s], type=[%d]\n", __FILE__, G_STRFUNC,
				   __LINE__, mfld->name, mfld->type));
		}

		if (mmsg->change_mask & CAMEL_MESSAGE_SEEN) {
			if (mmsg->change_flags & CAMEL_MESSAGE_SEEN) {
				if (!seen)
					seen = g_ptr_array_new ();
				g_ptr_array_add (seen, g_strdup (strrchr (mmsg->href, '/') + 1));
				mmsg->flags |= CAMEL_MESSAGE_SEEN;
			} else {
				if (!unseen)
					unseen = g_ptr_array_new ();
				g_ptr_array_add (unseen, g_strdup (strrchr (mmsg->href, '/') + 1));
				mmsg->flags &= ~CAMEL_MESSAGE_SEEN;
			}
			mmsg->change_mask &= ~CAMEL_MESSAGE_SEEN;
		}

		if (mmsg->change_mask & CAMEL_MESSAGE_ANSWERED) {
			E2kProperties *props;
			E2kHTTPStatus status;

			props = e2k_properties_new ();

			if (mmsg->change_flags & CAMEL_MESSAGE_ANSWERED) {
				e2k_properties_set_int (props, PR_ACTION, MAPI_ACTION_REPLIED);
				e2k_properties_set_int (props, PR_ACTION_FLAG, (mmsg->change_flags & CAMEL_MESSAGE_ANSWERED_ALL) ?
							MAPI_ACTION_FLAG_REPLIED_TO_ALL :
							MAPI_ACTION_FLAG_REPLIED_TO_SENDER);
				e2k_properties_set_date (props, PR_ACTION_DATE,
							e2k_make_timestamp (time (NULL)));
			} else {
				e2k_properties_remove (props, PR_ACTION);
				e2k_properties_remove (props, PR_ACTION_FLAG);
				e2k_properties_remove (props, PR_ACTION_DATE);
			}

			status = e2k_context_proppatch (ed->ctx, NULL, mmsg->href, props, FALSE, NULL);

			e2k_properties_free (props);

			if (!E2K_HTTP_STATUS_IS_SUCCESSFUL (status))
				g_warning ("set_replied_flags: %d", status);

			mmsg->change_mask &= ~(CAMEL_MESSAGE_ANSWERED | CAMEL_MESSAGE_ANSWERED_ALL);
		}

		if (mmsg->change_mask & CAMEL_MESSAGE_FLAGGED) {
			E2kProperties *props;
			E2kHTTPStatus status;

			props = e2k_properties_new ();

			if (mmsg->change_flags & CAMEL_MESSAGE_FLAGGED) {
				e2k_properties_set_int (props, PR_IMPORTANCE, MAPI_IMPORTANCE_HIGH);
			} else {
				e2k_properties_set_int (props, PR_IMPORTANCE, MAPI_IMPORTANCE_NORMAL);
			}

			status = e2k_context_proppatch (ed->ctx, NULL, mmsg->href, props, FALSE, NULL);

			e2k_properties_free (props);

			if (!E2K_HTTP_STATUS_IS_SUCCESSFUL (status))
				g_warning ("set_important_flag: %d", status);

			mmsg->change_mask &= ~CAMEL_MESSAGE_FLAGGED;
		}

		if (mmsg->tag_updates) {
			E2kProperties *props;
			const gchar *value;
			gint flag_status;
			E2kHTTPStatus status;

			flag_status = MAPI_FOLLOWUP_UNFLAGGED;
			props = e2k_properties_new ();

			value = g_datalist_get_data (&mmsg->tag_updates, "follow-up");
			if (value) {
				if (*value) {
					e2k_properties_set_string (props, E2K_PR_HTTPMAIL_MESSAGE_FLAG, g_strdup (value));
					flag_status = MAPI_FOLLOWUP_FLAGGED;
				} else {
					e2k_properties_remove (props, E2K_PR_HTTPMAIL_MESSAGE_FLAG);
				}
			}

			value = g_datalist_get_data (&mmsg->tag_updates, "due-by");
			if (value) {
				if (*value) {
					e2k_properties_set_string (props, E2K_PR_MAILHEADER_REPLY_BY, g_strdup (value));
				} else {
					e2k_properties_remove (props, E2K_PR_MAILHEADER_REPLY_BY);
				}
			}

			value = g_datalist_get_data (&mmsg->tag_updates, "completed-on");
			if (value) {
				if (*value) {
					e2k_properties_set_string (props, E2K_PR_MAILHEADER_COMPLETED, g_strdup (value));
					flag_status = MAPI_FOLLOWUP_COMPLETED;
				} else {
					e2k_properties_remove (props, E2K_PR_MAILHEADER_COMPLETED);
				}
			}
			g_datalist_clear (&mmsg->tag_updates);

			e2k_properties_set_int (props, PR_FLAG_STATUS, flag_status);

			status = e2k_context_proppatch (ed->ctx, NULL, mmsg->href, props, FALSE, NULL);

			e2k_properties_free (props);

			if (!E2K_HTTP_STATUS_IS_SUCCESSFUL (status))
				g_warning ("update_tags: %d", status);
		}

		if (!mmsg->change_mask)
			g_ptr_array_remove_index_fast (mfld->changed_messages, i--);
	}

	g_static_rec_mutex_unlock (&ed->changed_msgs_mutex);

	if (seen || unseen) {
		if (seen) {
			mark_read (mfld->folder, seen, TRUE);
			g_ptr_array_foreach (seen, (GFunc)g_free, NULL);
			g_ptr_array_free (seen, TRUE);
		}
		if (unseen) {
			mark_read (mfld->folder, unseen, FALSE);
			g_ptr_array_foreach (unseen, (GFunc)g_free, NULL);
			g_ptr_array_free (unseen, TRUE);
		}

		if (mfld->changed_messages->len == 0) {
			mfld->flag_timeout = 0;
			/* change_complete (mfld); */
			return FALSE;
		} else
			return TRUE;
	}

	g_static_rec_mutex_lock (&ed->changed_msgs_mutex);

	for (i = 0; i < mfld->changed_messages->len; i++) {
		mmsg = mfld->changed_messages->pdata[i];
		if (mmsg->change_mask & mmsg->change_flags & CAMEL_MESSAGE_DELETED) {
			if (!deleted)
				deleted = g_ptr_array_new ();
			g_ptr_array_add (deleted, strrchr (mmsg->href, '/') + 1);
		}
	}
	g_static_rec_mutex_unlock (&ed->changed_msgs_mutex);

	if (deleted) {
		CamelFolder *folder = get_camel_folder (mfld);
		E2kResultIter *iter;
		E2kResult *result;
		E2kHTTPStatus status;

		/* change_pending (mfld); */
		mfld->pending_delete_ops++;
		if (folder)
			camel_folder_freeze (folder);

		if (hier_type == EXCHANGE_HIERARCHY_PERSONAL) {
			iter = e_folder_exchange_transfer_start (mfld->folder, NULL,
								 ed->deleted_items,
								 deleted, TRUE);
		} else {
			/* This is for public folder hierarchy. We cannot move
			   a mail item deleted from a public folder to the
			   deleted items folder. This code updates the UI to
			   show the mail folder again if the deletion fails in
			   such public folder */
			iter = e_folder_exchange_bdelete_start (mfld->folder, NULL,
								(const gchar **)deleted->pdata,
								deleted->len);
		}
		g_ptr_array_free (deleted, FALSE);
		while ((result = e2k_result_iter_next (iter))) {
			if (hier_type == EXCHANGE_HIERARCHY_PERSONAL) {
				if (!e2k_properties_get_prop (result->props,
							      E2K_PR_DAV_LOCATION)) {
					continue;
				}
			} else if (result->status == E2K_HTTP_UNAUTHORIZED) {
				camel_exchange_folder_update_message_flags_ex (CAMEL_EXCHANGE_FOLDER (folder), mmsg->uid, 0, CAMEL_MESSAGE_DELETED);
				continue;
			}

			message_removed (mfld, folder, result->href);
			mfld->deleted_count++;
		}
		status = e2k_result_iter_free (iter);

		if (folder)
			camel_folder_thaw (folder);

		if (!E2K_HTTP_STATUS_IS_SUCCESSFUL (status))
			g_warning ("deleted: %d", status);

		mfld->pending_delete_ops--;
		/* change_complete (mfld); */
	}

	if (mfld->changed_messages->len) {
		g_ptr_array_set_size (mfld->changed_messages, 0);
		/* change_complete (mfld); */
	}

	mfld->flag_timeout = 0;

	return FALSE;
}

static E2kHTTPStatus
get_stickynote (E2kContext *ctx, E2kOperation *op, const gchar *uri, gchar **body, gint *len)
{
	static const gchar *stickynote_props[] = {
		E2K_PR_MAILHEADER_SUBJECT,
		E2K_PR_DAV_LAST_MODIFIED,
		E2K_PR_OUTLOOK_STICKYNOTE_COLOR,
		E2K_PR_OUTLOOK_STICKYNOTE_HEIGHT,
		E2K_PR_OUTLOOK_STICKYNOTE_WIDTH,
		E2K_PR_HTTPMAIL_TEXT_DESCRIPTION,
	};

	E2kHTTPStatus status;
	E2kResult *results;
	gint nresults = 0;
	GString *message;

	status = e2k_context_propfind (ctx, op, uri,
				       stickynote_props, G_N_ELEMENTS (stickynote_props),
				       &results, &nresults);

	if (E2K_HTTP_STATUS_IS_SUCCESSFUL (status)) {
		message = mail_util_stickynote_to_rfc822 (results[0].props);
		*body = message->str;
		*len = message->len;
		g_string_free (message, FALSE);
		e2k_results_free (results, nresults);
	}

	return status;
}

static E2kHTTPStatus
build_message_from_document (E2kContext *ctx, E2kOperation *op, const gchar *uri, gchar **body, gint *len)
{
	E2kHTTPStatus status;
	E2kResult *results;
	gint nresults = 0;
	GString *message;
	gchar *headers;

	status = e2k_context_propfind (ctx, op, uri, mapi_message_props, G_N_ELEMENTS (mapi_message_props), &results, &nresults);
	if (!E2K_HTTP_STATUS_IS_SUCCESSFUL (status))
		return status;
	if (!nresults)
		return E2K_HTTP_MALFORMED;

	headers = mail_util_mapi_to_smtp_headers (results[0].props);

	message = g_string_new (headers);
	g_string_append_len (message, *body, *len);

	g_free (headers);
	g_free (*body);

	*len = message->len;
	*body = g_string_free (message, FALSE);

	e2k_results_free (results, nresults);
	return status;
}

static E2kHTTPStatus
unmangle_delegated_meeting_request (ExchangeData *ed, E2kOperation *op, const gchar *uri, gchar **body, gint *len)
{
	const gchar *prop = PR_RCVD_REPRESENTING_EMAIL_ADDRESS;
	GString *message;
	gchar *delegator_dn, *delegator_uri, *delegator_folder_physical_uri = NULL;
	ExchangeAccount *account;
	E2kGlobalCatalog *gc;
	E2kGlobalCatalogEntry *entry;
	E2kGlobalCatalogStatus gcstatus;
	EFolder *folder = NULL;
	E2kHTTPStatus status;
	E2kResult *results;
	gint nresults = 0;
	MailUtilDemangleType unmangle_type = MAIL_UTIL_DEMANGLE_DELGATED_MEETING;

	status = e2k_context_propfind (ed->ctx, op, uri, &prop, 1, &results, &nresults);
	if (!E2K_HTTP_STATUS_IS_SUCCESSFUL (status))
		return status;
	if (!nresults)
		return E2K_HTTP_MALFORMED;

	delegator_dn = e2k_properties_get_prop (results[0].props, PR_RCVD_REPRESENTING_EMAIL_ADDRESS);
	if (!delegator_dn) {
		e2k_results_free (results, nresults);
		return E2K_HTTP_OK;
	}

	account = ed->account;
	gc = exchange_account_get_global_catalog (account);
	if (!gc) {
		g_warning ("\nNo GC: could not unmangle meeting request");
		e2k_results_free (results, nresults);
		return E2K_HTTP_OK;
	}

	gcstatus = e2k_global_catalog_lookup (
		gc, NULL, /* FIXME; cancellable */
		E2K_GLOBAL_CATALOG_LOOKUP_BY_LEGACY_EXCHANGE_DN,
		delegator_dn, E2K_GLOBAL_CATALOG_LOOKUP_MAILBOX,
		&entry);
	if (gcstatus != E2K_GLOBAL_CATALOG_OK) {
		g_warning ("\nGC lookup failed: could not unmangle meeting request");
		e2k_results_free (results, nresults);
		return E2K_HTTP_OK;
	}

	delegator_uri = exchange_account_get_foreign_uri (
		account, entry, E2K_PR_STD_FOLDER_CALENDAR);

	if (delegator_uri) {
		folder = exchange_account_get_folder (account, delegator_uri);
		if (folder)
			delegator_folder_physical_uri = g_strdup (e_folder_get_physical_uri (folder));
		g_free (delegator_uri);
	}

	message = g_string_new_len (*body, *len);
	mail_util_demangle_meeting_related_message (message, entry->display_name,
						entry->email,
						delegator_folder_physical_uri,
						exchange_account_get_email_id (account),
						unmangle_type);
	g_free (*body);
	*body = message->str;
	*len = message->len;
	*body = g_string_free (message, FALSE);

	e2k_global_catalog_entry_free (gc, entry);
	g_free (delegator_folder_physical_uri);

	e2k_results_free (results, nresults);
	return E2K_HTTP_OK;
}

static gboolean
is_foreign_folder (ExchangeData *ed, const gchar *folder_name, gchar **owner_email)
{
	EFolder *folder;
	ExchangeHierarchy *hier;
	gchar *path;

	path = g_build_filename ("/", folder_name, NULL);
	folder = exchange_account_get_folder (ed->account, path);
	if (!folder) {
		g_free (path);
		return FALSE;
	}

	g_free (path);
	g_object_ref (folder);

	hier = e_folder_exchange_get_hierarchy (folder);

	if (hier->type != EXCHANGE_HIERARCHY_FOREIGN) {
		g_object_unref (folder);
		return FALSE;
	}

	*owner_email = g_strdup (hier->owner_email);

	g_object_unref (folder);
	return TRUE;
}

static E2kHTTPStatus
unmangle_meeting_request_in_subscribed_inbox (ExchangeData *ed, const gchar *delegator_email, gchar **body, gint *len)
{
	GString *message;
	gchar *delegator_uri, *delegator_folder_physical_uri = NULL;
	ExchangeAccount *account;
	E2kGlobalCatalog *gc;
	E2kGlobalCatalogEntry *entry;
	E2kGlobalCatalogStatus gcstatus;
	EFolder *folder = NULL;
	MailUtilDemangleType unmangle_type = MAIL_UTIL_DEMANGLE_MEETING_IN_SUBSCRIBED_INBOX;

	account = ed->account;
	gc = exchange_account_get_global_catalog (account);
	if (!gc) {
		g_warning ("\nNo GC: could not unmangle meeting request in subscribed folder");
		return E2K_HTTP_OK;
	}

	gcstatus = e2k_global_catalog_lookup (
		gc, NULL, /* FIXME; cancellable */
		E2K_GLOBAL_CATALOG_LOOKUP_BY_EMAIL,
		delegator_email, E2K_GLOBAL_CATALOG_LOOKUP_MAILBOX,
		&entry);
	if (gcstatus != E2K_GLOBAL_CATALOG_OK) {
		g_warning ("\nGC lookup failed: could not unmangle meeting request in subscribed folder");
		return E2K_HTTP_OK;
	}

	delegator_uri = exchange_account_get_foreign_uri (
		account, entry, E2K_PR_STD_FOLDER_CALENDAR);

	if (delegator_uri) {
		folder = exchange_account_get_folder (account, delegator_uri);
		if (folder)
			delegator_folder_physical_uri = g_strdup (e_folder_get_physical_uri (folder));
		g_free (delegator_uri);
	}

	message = g_string_new_len (*body, *len);
	mail_util_demangle_meeting_related_message (message, entry->display_name,
						entry->email,
						delegator_folder_physical_uri,
						exchange_account_get_email_id (account),
						unmangle_type);
	g_free (*body);
	*body = message->str;
	*len = message->len;
	*body = g_string_free (message, FALSE);

	e2k_global_catalog_entry_free (gc, entry);
	g_free (delegator_folder_physical_uri);

	return E2K_HTTP_OK;
}

static E2kHTTPStatus
unmangle_sender_field (ExchangeData *ed, E2kOperation *op, const gchar *uri, gchar **body, gint *len)
{
	const gchar *props[] = { PR_SENT_REPRESENTING_EMAIL_ADDRESS, PR_SENDER_EMAIL_ADDRESS };
	GString *message;
	gchar *delegator_dn, *sender_dn;
	ExchangeAccount *account;
	E2kGlobalCatalog *gc;
	E2kGlobalCatalogEntry *delegator_entry;
	E2kGlobalCatalogEntry *sender_entry;
	E2kGlobalCatalogStatus gcstatus;
	E2kHTTPStatus status;
	E2kResult *results;
	gint nresults = 0;
	MailUtilDemangleType unmangle_type = MAIL_UTIL_DEMANGLE_SENDER_FIELD;

	status = e2k_context_propfind (ed->ctx, op, uri, props, 2, &results, &nresults);
	if (!E2K_HTTP_STATUS_IS_SUCCESSFUL (status))
		return status;
	if (!nresults)
		return E2K_HTTP_MALFORMED;

	delegator_dn = e2k_properties_get_prop (results[0].props, PR_SENT_REPRESENTING_EMAIL_ADDRESS);
	if (!delegator_dn) {
		e2k_results_free (results, nresults);
		return E2K_HTTP_OK;
	}

	sender_dn = e2k_properties_get_prop (results[0].props, PR_SENDER_EMAIL_ADDRESS);
	if (!sender_dn) {
		e2k_results_free (results, nresults);
		return E2K_HTTP_OK;
	}

	if (!g_ascii_strcasecmp (delegator_dn, sender_dn)) {
		e2k_results_free (results, nresults);
		return E2K_HTTP_OK;
	}

	account = ed->account;
	gc = exchange_account_get_global_catalog (account);
	if (!gc) {
		g_warning ("\nNo GC: could not unmangle sender field");
		e2k_results_free (results, nresults);
		return E2K_HTTP_OK;
	}

	gcstatus = e2k_global_catalog_lookup (
		gc, NULL, /* FIXME; cancellable */
		E2K_GLOBAL_CATALOG_LOOKUP_BY_LEGACY_EXCHANGE_DN,
		delegator_dn, E2K_GLOBAL_CATALOG_LOOKUP_MAILBOX,
		&delegator_entry);
	if (gcstatus != E2K_GLOBAL_CATALOG_OK) {
		g_warning ("\nGC lookup failed: for delegator_entry - could not unmangle sender field");
		e2k_results_free (results, nresults);
		return E2K_HTTP_OK;
	}

	gcstatus = e2k_global_catalog_lookup (
		gc, NULL, /* FIXME; cancellable */
		E2K_GLOBAL_CATALOG_LOOKUP_BY_LEGACY_EXCHANGE_DN,
		sender_dn, E2K_GLOBAL_CATALOG_LOOKUP_MAILBOX,
		&sender_entry);
	if (gcstatus != E2K_GLOBAL_CATALOG_OK) {
		g_warning ("\nGC lookup failed: for sender_entry - could not unmangle sender field");
		e2k_results_free (results, nresults);
		return E2K_HTTP_OK;
	}

	message = g_string_new_len (*body, *len);
	mail_util_demangle_meeting_related_message (message, delegator_entry->display_name,
						delegator_entry->email,
						NULL,
						sender_entry->email,
						unmangle_type);
	g_free (*body);
	*body = message->str;
	*len = message->len;
	*body = g_string_free (message, FALSE);

	e2k_global_catalog_entry_free (gc, delegator_entry);
	e2k_global_catalog_entry_free (gc, sender_entry);

	e2k_results_free (results, nresults);
	return E2K_HTTP_OK;
}

static void
foreign_new_folder_cb (ExchangeAccount *account, EFolder *folder, GPtrArray *folders)
{
	g_return_if_fail (folder != NULL);
	g_return_if_fail (folders != NULL);

	g_ptr_array_add (folders, folder);
}

static void
get_folder_info_data (ExchangeData *ed, const gchar *top, guint32 store_flags, GHashTable *known_uris, GPtrArray **names, GPtrArray **uris, GArray **unread, GArray **flags)
{
	GPtrArray *folders = NULL;
	ExchangeHierarchy *hier;
	EFolder *folder;
	const gchar *type, *name, *uri, *inbox_uri = NULL, *trash_uri = NULL, *sent_items_uri = NULL;
	gint unread_count, i, toplen = top ? strlen (top) : 0;
	guint32 folder_flags = 0;
	gboolean recursive, subscribed, subscription_list;
	gint mode = -1;
	gchar *full_path;
	GSList *check_children = NULL;

	recursive = (store_flags & CAMEL_STORE_FOLDER_INFO_RECURSIVE);
	subscribed = (store_flags & CAMEL_STORE_FOLDER_INFO_SUBSCRIBED);
	subscription_list = (store_flags & CAMEL_STORE_FOLDER_INFO_SUBSCRIPTION_LIST);

	mode = is_online (ed);
	if (!subscribed && subscription_list) {
		ExchangeAccountFolderResult result = -1;

		d(g_print ("%s(%d):%s: NOT SUBSCRIBED top = [%s]\n", __FILE__, __LINE__, G_STRFUNC, top));
		if (!toplen)
			result = exchange_account_open_folder (ed->account, "/public");
		if (result ==  EXCHANGE_ACCOUNT_FOLDER_DOES_NOT_EXIST) {
			hier = exchange_account_get_hierarchy_by_type (ed->account, EXCHANGE_HIERARCHY_PUBLIC);
			if (hier)
				exchange_hierarchy_scan_subtree (hier, hier->toplevel, mode);
		} else {
			d(g_print ("%s(%d):%s: NOT SUBSCRIBED - open_folder returned = [%d]\n", __FILE__, __LINE__, G_STRFUNC, result));
		}
	}

	/* No need to check for recursive flag, as I will always be returning a tree, instead of a single folder info object */
	if (toplen) {
		d(g_print ("%s(%d):%s: NOT RECURSIVE and toplen top = [%s]\n", __FILE__, __LINE__, G_STRFUNC, top));
		full_path = g_strdup_printf ("/%s", top);
		folders = exchange_account_get_folder_tree (ed->account, full_path);
		g_free (full_path);
	} else {
		d(g_print ("%s(%d):%s calling exchange_account_get_folders \n", __FILE__, __LINE__, G_STRFUNC));
		folders = exchange_account_get_folders (ed->account);
	}

	if (!*names)
		*names = g_ptr_array_new ();
	if (!*uris)
		*uris = g_ptr_array_new ();
	if (!*unread)
		*unread = g_array_new (FALSE, FALSE, sizeof (gint));
	if (!*flags)
		*flags = g_array_new (FALSE, FALSE, sizeof (gint));
	/* Can be NULL if started in offline mode */
	if (ed->inbox) {
		inbox_uri = e_folder_get_physical_uri (ed->inbox);
	}

	if (ed->deleted_items) {
		trash_uri = e_folder_get_physical_uri (ed->deleted_items);
	}

	if (ed->sent_items) {
		sent_items_uri = e_folder_get_physical_uri (ed->sent_items);
	}

	if (folders) {
		guint new_folder_handler_id = 0;

		if (ed->new_folder_id == 0)
			new_folder_handler_id = g_signal_connect (ed->account, "new_folder", G_CALLBACK (foreign_new_folder_cb), folders);

		for (i = 0; i < folders->len; i++) {
			folder = folders->pdata[i];
			hier = e_folder_exchange_get_hierarchy (folder);
			folder_flags = 0;

			if (subscribed) {
				if (hier->type != EXCHANGE_HIERARCHY_PERSONAL &&
				    hier->type != EXCHANGE_HIERARCHY_FAVORITES &&
				    hier->type != EXCHANGE_HIERARCHY_FOREIGN)
					continue;
			} else if (subscription_list) {
				if (hier->type != EXCHANGE_HIERARCHY_PUBLIC)
					continue;
			}

			type = e_folder_get_type_string (folder);
			name = e_folder_get_name (folder);
			uri = e_folder_get_physical_uri (folder);
			d(g_print ("Uri: %s\n", uri));
			d(g_print ("folder type is : %s\n", type));

			if (!strcmp (type, "noselect")) {
				unread_count = 0;
				folder_flags = CAMEL_FOLDER_NOSELECT;
			}

			switch (hier->type) {
				case EXCHANGE_HIERARCHY_FAVORITES:
					/* folder_flags will be set only if the type
					   is noselect and we need to include it */
					if (strcmp (type, "mail") && !folder_flags)
						continue;
					/* selectable */
					if (!folder_flags)
						unread_count = e_folder_get_unread_count (folder);
				case EXCHANGE_HIERARCHY_PUBLIC:
					if (exchange_account_is_favorite_folder (ed->account, folder)) {
						folder_flags |= CAMEL_FOLDER_SUBSCRIBED;
						d(printf ("marked the folder as subscribed\n"));
					}
					break;
				case EXCHANGE_HIERARCHY_FOREIGN:
					if ((folder_flags & CAMEL_FOLDER_NOSELECT) != 0 && ed->new_folder_id == 0) {
						/* Rescan the hierarchy - as we don't rescan
						   foreign hierarchies anywhere for mailer and
						   only when we are starting up
						*/
						exchange_hierarchy_scan_subtree (hier, hier->toplevel, mode);
					}
				case EXCHANGE_HIERARCHY_PERSONAL:
					if (!strcmp (type, "mail")) {
						unread_count = e_folder_get_unread_count (folder);
					}
					else if (!folder_flags) {
						continue;
					}
					break;
				default:
					break;
			}

			if (inbox_uri && !strcmp (uri, inbox_uri))
				folder_flags |= CAMEL_FOLDER_SYSTEM | CAMEL_FOLDER_TYPE_INBOX;

			if (trash_uri && !strcmp (uri, trash_uri))
				folder_flags |= CAMEL_FOLDER_SYSTEM | CAMEL_FOLDER_TYPE_TRASH;

			if (sent_items_uri && !strcmp (uri, sent_items_uri))
				folder_flags |= CAMEL_FOLDER_SYSTEM | CAMEL_FOLDER_TYPE_SENT;

			if (!e_folder_exchange_get_has_subfolders (folder)) {
				d(printf ("%s:%d:%s - %s has no subfolders", __FILE__, __LINE__, G_STRFUNC, name));
				folder_flags |= CAMEL_FOLDER_NOCHILDREN;
			} else if (recursive && !subscribed && subscription_list && known_uris && g_hash_table_lookup (known_uris, uri) == NULL) {
				gchar *path = strrchr (uri, ';');
				if (path && g_ascii_strcasecmp (path + 1, "public") != 0) {
					path = g_uri_unescape_string (path + 1, NULL);
					check_children = g_slist_prepend (check_children, path);
				}
			}

			d(g_print ("folder flags is : %d\n", folder_flags));

			uri = g_strdup (uri);
			if (known_uris)
				g_hash_table_insert (known_uris, (gchar *) uri, GINT_TO_POINTER (1));

			g_ptr_array_add (*names, g_strdup (name));
			g_ptr_array_add (*uris, (gchar *)uri);
			g_array_append_val (*unread, unread_count);
			g_array_append_val (*flags, folder_flags);
		}

		if (new_folder_handler_id)
			g_signal_handler_disconnect (ed->account, new_folder_handler_id);
		g_ptr_array_free (folders, TRUE);
	}

	if (check_children) {
		GSList *l;

		check_children = g_slist_reverse (check_children);
		for (l = check_children; l; l = l->next) {
			get_folder_info_data (ed, l->data, store_flags, known_uris, names, uris, unread, flags);
		}

		g_slist_foreach (check_children, (GFunc) g_free, NULL);
		g_slist_free (check_children);
	}
}

struct update_linestatus
{
	CamelExchangeStore *estore;
	gint linestatus;
	GError **error;
};

static void
folder_update_linestatus (gpointer key, gpointer value, gpointer data)
{
	ExchangeFolder *mfld = (ExchangeFolder *) value;
	struct update_linestatus *ul = (struct update_linestatus *) data;
	guint32 readonly;

	g_return_if_fail (ul != NULL);

	if (ul->linestatus == ONLINE_MODE) {
		CamelFolder *folder;

		if (!get_folder_online (mfld, ul->error))
			return;

		readonly = (mfld->access & (MAPI_ACCESS_MODIFY | MAPI_ACCESS_CREATE_CONTENTS)) ? 0 : 1;

		folder = get_camel_folder (mfld);
		if (folder) {
			camel_exchange_summary_set_readonly (folder->summary, readonly ? TRUE : FALSE);
		}
	} else {
		/* FIXME: need any undo for offline */ ;
	}
}

gboolean
camel_exchange_utils_connect (CamelService *service,
				const gchar *pwd,
				guint32 *status, /* out */
				GError **error)
{
	ExchangeData *ed = get_data_for_service (service);
	ExchangeAccount *account;
	ExchangeAccountResult result;
	E2kContext *ctx;
	guint32 retval = 1;
	const gchar *uri;
	struct update_linestatus ul;

	if (ed == NULL) {
		g_set_error (error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			"Could not find Exchange account. Make sure you've only one Exchange account configured.");
		return FALSE;
	}

	g_return_val_if_fail (ed != NULL, FALSE);
	g_return_val_if_fail (status != NULL, FALSE);

	ul.linestatus = is_online (ed);

	account = ed->account;
	if (ul.linestatus == ONLINE_MODE)
		exchange_account_set_online (account);
	else if (ul.linestatus == OFFLINE_MODE)
		exchange_account_set_offline (account);

	ctx = exchange_account_get_context (account);
	if (!ctx) {
		ctx = exchange_account_connect (account, pwd, &result);
	}

	if (!ctx && ul.linestatus == ONLINE_MODE) {
		retval = 0;
		goto end;
	} else if (ul.linestatus == OFFLINE_MODE) {
		goto end;
	}

	ed->ctx = g_object_ref (ctx);

	ed->mail_submission_uri = exchange_account_get_standard_uri (account, "sendmsg");
	uri = exchange_account_get_standard_uri (account, "inbox");
	ed->inbox = exchange_account_get_folder (account, uri);
	uri = exchange_account_get_standard_uri (account, "deleteditems");
	ed->deleted_items = exchange_account_get_folder (account, uri);
	uri = exchange_account_get_standard_uri (account, "sentitems");
	ed->sent_items = exchange_account_get_folder (account, uri);

	/* Will be used for offline->online transition to initialize things for
	   the first time */

	ul.estore = ed->estore;
	ul.error = error;
	g_hash_table_foreach (ed->folders_by_name,
			      (GHFunc) folder_update_linestatus,
			      &ul);
 end:
	*status = retval;

	return TRUE;
}

gboolean
camel_exchange_utils_get_folder (CamelService *service,
				const gchar *name,
				gboolean create,
				GPtrArray *uids,
				GByteArray *flags,
				GPtrArray *hrefs,
				guint32 high_article_num,
				guint32 *folder_flags, /* out */
				gchar **folder_uri, /* out */
				gboolean *readonly, /* out */
				GError **error)
{
	ExchangeData *ed = get_data_for_service (service);
	ExchangeFolder *mfld;
	ExchangeMessage *mmsg;
	EFolder *folder;
	gchar *path;
	const gchar *outlook_class;
	guint32 camel_flags;
	gint i;
	ExchangeHierarchy *hier;

	g_return_val_if_fail (ed != NULL, FALSE);
	g_return_val_if_fail (folder_flags != NULL, FALSE);
	g_return_val_if_fail (folder_uri != NULL, FALSE);
	g_return_val_if_fail (readonly != NULL, FALSE);

	path = g_strdup_printf ("/%s", name);
	folder = exchange_account_get_folder (ed->account, path);
	if (!folder && !create) {
		set_exception (error, _("No such folder"));
		g_free (path);
		return FALSE;
	} else if (!folder) {
		ExchangeAccountFolderResult result;

		result = exchange_account_create_folder (ed->account, path, "mail");
		folder = exchange_account_get_folder (ed->account, path);
		if (result != EXCHANGE_ACCOUNT_FOLDER_OK || !folder) {
			set_exception (error, _("Could not create folder."));
			g_free (path);
			return FALSE;
		}
	}
	g_free (path);

	mfld = g_new0 (ExchangeFolder, 1);
	mfld->ed = ed;
	mfld->folder = folder;
	g_object_ref (folder);
	mfld->name = e_folder_exchange_get_path (folder) + 1;

	if (!strcmp (e_folder_get_type_string (folder), "mail/public"))
		mfld->type = EXCHANGE_FOLDER_POST;
	else {
		outlook_class = e_folder_exchange_get_outlook_class (folder);
		if (!outlook_class)
			mfld->type = EXCHANGE_FOLDER_OTHER;
		else if (!g_ascii_strncasecmp (outlook_class, "IPF.Note", 8))
			mfld->type = EXCHANGE_FOLDER_REAL;
		else if (!g_ascii_strncasecmp (outlook_class, "IPF.Post", 8))
			mfld->type = EXCHANGE_FOLDER_POST;
		else if (!g_ascii_strncasecmp (outlook_class, "IPF.StickyNote", 14))
			mfld->type = EXCHANGE_FOLDER_NOTES;
		else
			mfld->type = EXCHANGE_FOLDER_OTHER;
	}

	mfld->messages = g_ptr_array_new ();
	mfld->messages_by_uid = g_hash_table_new (g_str_hash, g_str_equal);
	mfld->messages_by_href = g_hash_table_new (g_str_hash, g_str_equal);
	for (i = 0; i < uids->len; i++) {
		mmsg = new_message (uids->pdata[i], NULL, mfld->seq++, flags->data[i]);
		g_ptr_array_add (mfld->messages, mmsg);
		g_hash_table_insert (mfld->messages_by_uid, mmsg->uid, mmsg);

		if (hrefs->pdata[i] && *((gchar *)hrefs->pdata[i])) {
			mmsg->href = g_strdup (hrefs->pdata[i]);
			g_hash_table_insert (mfld->messages_by_href, mmsg->href, mmsg);
		}
		if (!(mmsg->flags & CAMEL_MESSAGE_SEEN))
			mfld->unread_count++;
	}

	mfld->high_article_num = high_article_num;

	if (is_online (ed) == ONLINE_MODE) {
		if (!get_folder_online (mfld, error))
			return FALSE;
	}
	g_signal_connect (mfld->folder, "changed",
			  G_CALLBACK (storage_folder_changed), mfld);

	g_hash_table_insert (ed->folders_by_name, (gchar *)mfld->name, mfld);
	folder_changed (mfld);

	*readonly = ((mfld->access & (MAPI_ACCESS_MODIFY | MAPI_ACCESS_CREATE_CONTENTS)) == 0);

	camel_flags = 0;
	if (ed->account->filter_inbox && (mfld->folder == ed->inbox))
		camel_flags |= CAMEL_FOLDER_FILTER_RECENT;
	if (ed->account->filter_junk) {
		if ((mfld->folder != ed->deleted_items) &&
		    ((mfld->folder == ed->inbox) ||
		    !ed->account->filter_junk_inbox_only))
			camel_flags |= CAMEL_FOLDER_FILTER_JUNK;
	}

	hier = e_folder_exchange_get_hierarchy (mfld->folder);

	*folder_flags = camel_flags;
	*folder_uri = g_strdup (hier->source_uri);

	return TRUE;
}

gboolean
camel_exchange_utils_get_trash_name (CamelService *service,
				gchar **trash_name, /* out */
				GError **error)
{
	ExchangeData *ed = get_data_for_service (service);

	g_return_val_if_fail (ed != NULL, FALSE);
	g_return_val_if_fail (trash_name != NULL, FALSE);

	if (!ed->deleted_items) {
		set_exception (error, _("Could not open Deleted Items folder"));
		return FALSE;
	}

	*trash_name = g_strdup (e_folder_exchange_get_path (ed->deleted_items) + 1);

	return TRUE;
}

gboolean
camel_exchange_utils_refresh_folder (CamelService *service,
				const gchar *folder_name,
				GError **error)
{
	ExchangeData *ed = get_data_for_service (service);
	ExchangeFolder *mfld;

	g_return_val_if_fail (ed != NULL, FALSE);

	mfld = folder_from_name (ed, folder_name, 0, error);
	if (!mfld)
		return FALSE;

	refresh_folder_internal (mfld, error);
	sync_deletions (mfld);

	return TRUE;
}

gboolean
camel_exchange_utils_sync_count (CamelService *service,
				const gchar *folder_name,
				guint32 *unread_count, /* out */
				guint32 *visible_count, /* out */
				GError **error)
{
	ExchangeData *ed = get_data_for_service (service);
	ExchangeFolder *mfld;

	g_return_val_if_fail (ed != NULL, FALSE);
	g_return_val_if_fail (unread_count != NULL, FALSE);
	g_return_val_if_fail (visible_count != NULL, FALSE);

	mfld = folder_from_name (ed, folder_name, 0, error);
	if (mfld) {
		*unread_count = mfld->unread_count;
		*visible_count = mfld->messages->len;
	} else {
		*unread_count = 0;
		*visible_count = 0;
	}

	return TRUE;
}

gboolean
camel_exchange_utils_expunge_uids (CamelService *service,
				const gchar *folder_name,
				GPtrArray *uids,
				GError **error)
{
	ExchangeData *ed = get_data_for_service (service);
	ExchangeFolder *mfld;
	ExchangeMessage *mmsg;
	GPtrArray *hrefs;
	E2kResultIter *iter;
	E2kResult *result;
	E2kHTTPStatus status;
	gint i, ndeleted;
	gboolean some_error = FALSE;
	CamelFolder *folder;

	g_return_val_if_fail (ed != NULL, FALSE);

	if (!uids->len)
		return TRUE;

	mfld = folder_from_name (ed, folder_name, MAPI_ACCESS_DELETE, error);
	if (!mfld)
		return FALSE;

	g_static_rec_mutex_lock (&ed->changed_msgs_mutex);
	hrefs = g_ptr_array_new ();
	for (i = 0; i < uids->len; i++) {
		mmsg = find_message (mfld, uids->pdata[i]);
		if (mmsg)
			g_ptr_array_add (hrefs, strrchr (mmsg->href, '/') + 1);
	}

	if (!hrefs->len) {
		/* Can only happen if there's a bug somewhere else, but we
		 * don't want to crash.
		 */
		g_ptr_array_free (hrefs, TRUE);
		g_static_rec_mutex_unlock (&ed->changed_msgs_mutex);
		return TRUE;
	}

	folder = get_camel_folder (mfld);
	if (folder)
		camel_folder_freeze (folder);

	iter = e_folder_exchange_bdelete_start (mfld->folder, NULL,
						(const gchar **)hrefs->pdata,
						hrefs->len);
	ndeleted = 0;
	while ((result = e2k_result_iter_next (iter))) {
		if (result->status == E2K_HTTP_UNAUTHORIZED) {
			some_error = TRUE;
			continue;
		}
		message_removed (mfld, folder, result->href);
		mfld->deleted_count++;
		ndeleted++;

		camel_operation_progress (NULL, ndeleted * 100 / hrefs->len);
	}
	status = e2k_result_iter_free (iter);
	g_static_rec_mutex_unlock (&ed->changed_msgs_mutex);

	if (folder)
		camel_folder_thaw (folder);

	if (!E2K_HTTP_STATUS_IS_SUCCESSFUL (status)) {
		g_warning ("expunged: %d", status);
		some_error = TRUE;
		set_exception (error, _("Could not empty Deleted Items folder"));
	} else if (some_error) {
		set_exception (error, _("Permission denied. Could not delete certain messages."));
	}

	g_ptr_array_free (hrefs, TRUE);

	return !some_error;
}

static gboolean
test_uri (E2kContext *ctx, const gchar *test_name, gpointer messages_by_href)
{
	return g_hash_table_lookup (messages_by_href, test_name) == NULL;
}

gboolean
camel_exchange_utils_append_message (CamelService *service,
				const gchar *folder_name,
				guint32 flags,
				const gchar *subject,
				const GByteArray *message,
				gchar **new_uid, /* out */
				GError **error)
{
	ExchangeData *ed = get_data_for_service (service);
	ExchangeFolder *mfld;
	E2kHTTPStatus status;
	gchar *ru_header = NULL, *repl_uid, *location = NULL;

	g_return_val_if_fail (ed != NULL, FALSE);
	g_return_val_if_fail (new_uid != NULL, FALSE);
	g_return_val_if_fail (message != NULL, FALSE);

	mfld = folder_from_name (
		ed, folder_name, MAPI_ACCESS_CREATE_CONTENTS, error);
	if (!mfld)
		return FALSE;

	status = e_folder_exchange_put_new (mfld->folder, NULL, subject,
					    test_uri, mfld->messages_by_href,
					    "message/rfc822", (const gchar *)message->data, message->len,
					    &location, &ru_header);
	if (status != E2K_HTTP_CREATED) {
		g_warning ("appended_message: %d", status);
		set_exception (error, status == E2K_HTTP_INSUFFICIENT_SPACE_ON_RESOURCE ?
				   _("Could not append message; mailbox is over quota") :
				   _("Could not append message"));
		return FALSE;
	}

	if (location) {
		if (flags & CAMEL_MESSAGE_SEEN)
			mark_one_read (ed->ctx, location, TRUE);
		else
			mark_one_read (ed->ctx, location, FALSE);
	}

	if (ru_header && *ru_header == '<' && strlen (ru_header) > 3)
		repl_uid = g_strndup (ru_header + 1, strlen (ru_header) - 2);
	else
		repl_uid = NULL;

	*new_uid = g_strdup (repl_uid ? uidstrip (repl_uid) : "");

	g_free (repl_uid);
	g_free (ru_header);
	g_free (location);

	return TRUE;
}

static void
change_message (ExchangeFolder *mfld, ExchangeMessage *mmsg)
{
	gint i;

	g_static_rec_mutex_lock (&mfld->ed->changed_msgs_mutex);

	for (i=0; i<mfld->changed_messages->len; i++)  {
		if (mfld->changed_messages->pdata[i] == mmsg)
			break;
	}

	if (i == mfld->changed_messages->len) {
		/*change_pending (mfld); !!!TODO!!! */
		g_ptr_array_add (mfld->changed_messages, mmsg);
	}
	g_static_rec_mutex_unlock (&mfld->ed->changed_msgs_mutex);

	if (mfld->flag_timeout)
		g_source_remove (mfld->flag_timeout);
	mfld->flag_timeout = g_timeout_add (1000, process_flags, mfld);
}

gboolean
camel_exchange_utils_set_message_flags (CamelService *service,
					const gchar *folder_name,
					const gchar *uid,
					guint32 flags,
					guint32 mask,
					GError **error)
{
	ExchangeData *ed = get_data_for_service (service);
	ExchangeFolder *mfld;
	ExchangeMessage *mmsg;

	g_return_val_if_fail (ed != NULL, FALSE);

	mfld = folder_from_name (ed, folder_name, MAPI_ACCESS_MODIFY, error);
	if (!mfld)
		return FALSE;

	mmsg = find_message (mfld, uid);
	if (!mmsg)
		return FALSE;

	/* Although we don't actually process the flag change right
	 * away, we need to update the folder's unread count to match
	 * what the user now believes it is. (We take advantage of the
	 * fact that the mailer will never delete a message without
	 * also marking it read.)
	 */
	if (mask & CAMEL_MESSAGE_SEEN) {
		if (((mmsg->flags ^ flags) & CAMEL_MESSAGE_SEEN) == 0) {
			/* The user is just setting it to what it
			 * already is, so ignore it.
			 */
			mask &= ~CAMEL_MESSAGE_SEEN;
		} else {
			mmsg->flags ^= CAMEL_MESSAGE_SEEN;
			if (mmsg->flags & CAMEL_MESSAGE_SEEN)
				mfld->unread_count--;
			else
				mfld->unread_count++;
			folder_changed (mfld);
		}
	}

	/* If the user tries to delete a message in a non-person
	 * hierarchy, we ignore it (which will cause camel to delete
	 * it the hard way next time it syncs).
	 */

#if 0
	/* If we allow camel utils to delete these messages hard way, it may
	   fail to delete a mail because of permissions, but will append
	   a mail in deleted items */

	if (mask & flags & CAMEL_MESSAGE_DELETED) {
		ExchangeHierarchy *hier;

		hier = e_folder_exchange_get_hierarchy (mfld->folder);
		if (hier->type != EXCHANGE_HIERARCHY_PERSONAL)
			mask &= ~CAMEL_MESSAGE_DELETED;
	}
#endif

	/* If there's nothing left to change, return. */
	if (!mask)
		return TRUE;

	mmsg->change_flags |= (flags & mask);
	mmsg->change_flags &= ~(~flags & mask);
	mmsg->change_mask |= mask;

	change_message (mfld, mmsg);

	return TRUE;
}

gboolean
camel_exchange_utils_set_message_tag (CamelService *service,
				const gchar *folder_name,
				const gchar *uid,
				const gchar *name,
				const gchar *value,
				GError **error)
{
	ExchangeData *ed = get_data_for_service (service);
	ExchangeFolder *mfld;
	ExchangeMessage *mmsg;

	g_return_val_if_fail (ed != NULL, FALSE);

	mfld = folder_from_name (ed, folder_name, MAPI_ACCESS_MODIFY, error);
	if (!mfld)
		return FALSE;

	mmsg = find_message (mfld, uid);
	if (!mmsg)
		return FALSE;

	g_datalist_set_data_full (&mmsg->tag_updates, name, g_strdup (value), g_free);

	change_message (mfld, mmsg);

	return TRUE;
}

gboolean
camel_exchange_utils_get_message (CamelService *service,
				const gchar *folder_name,
				const gchar *uid,
				GByteArray **message_bytes, /* out */
				GError **error)
{
	ExchangeData *ed = get_data_for_service (service);
	ExchangeFolder *mfld;
	ExchangeMessage *mmsg;
	E2kHTTPStatus status;
	gchar *body = NULL, *content_type = NULL, *owner_email = NULL;
	gint len = 0;
	gboolean res = FALSE;
	CamelMessageInfo *info;
	CamelFolder *folder;

	g_return_val_if_fail (ed != NULL, FALSE);
	g_return_val_if_fail (message_bytes != NULL, FALSE);

	mfld = folder_from_name (ed, folder_name, MAPI_ACCESS_READ, error);
	if (!mfld)
		return FALSE;

	folder = get_camel_folder (mfld);

	mmsg = find_message (mfld, uid);
	if (!mmsg) {
		if (folder && (info = camel_folder_summary_uid (folder->summary, uid))) {
			camel_message_info_free (info);
			camel_exchange_folder_remove_message (CAMEL_EXCHANGE_FOLDER (folder), uid);
		}

		set_exception (error, _("No such message"));
		return FALSE;
	}

	if (mfld->type == EXCHANGE_FOLDER_NOTES) {
		status = get_stickynote (ed->ctx, NULL, mmsg->href, &body, &len);
		if (!E2K_HTTP_STATUS_IS_SUCCESSFUL (status))
			goto error;
		content_type = g_strdup ("message/rfc822");
	} else {
		SoupBuffer *response;

		status = e2k_context_get (ed->ctx, NULL, mmsg->href, &content_type, &response);
		if (!E2K_HTTP_STATUS_IS_SUCCESSFUL (status))
			goto error;

		len = response->length;
		body = g_strndup (response->data, response->length);
		soup_buffer_free (response);
	}

	/* Public folders especially can contain non-email objects.
	 * In that case, we fake the headers (which in this case
	 * should include Content-Type, Content-Disposition, etc,
	 * courtesy of mp:x67200102.
	 */
	if (!content_type || g_ascii_strncasecmp (content_type, "message/", 8)) {
		status = build_message_from_document (ed->ctx, NULL, mmsg->href, &body, &len);
		if (!E2K_HTTP_STATUS_IS_SUCCESSFUL (status))
			goto error;
	}

	/* If this is a delegated meeting request, we need to know who
	 * delegated it to us.
	 */
	if (mmsg->flags & EXMAIL_DELEGATED) {
		status = unmangle_delegated_meeting_request (ed, NULL, mmsg->href, &body, &len);
		if (!E2K_HTTP_STATUS_IS_SUCCESSFUL (status))
			goto error;
	}

	/* If the message is in a subscribed inbox,
	 * we need to modify the message appropriately.
	 */
	if (is_foreign_folder (ed, folder_name, &owner_email)) {
		status = unmangle_meeting_request_in_subscribed_inbox (ed, owner_email, &body, &len);
		if (!E2K_HTTP_STATUS_IS_SUCCESSFUL (status))
			goto error;
	}

	/* If there is a sender field in the meeting request/response,
	 * we need to know who it is.
	 */
	status = unmangle_sender_field (ed, NULL, mmsg->href, &body, &len);
	if (!E2K_HTTP_STATUS_IS_SUCCESSFUL (status))
		goto error;

	*message_bytes = g_byte_array_sized_new (len);
	g_byte_array_append (*message_bytes, (const guint8 *)body, len);

	res = TRUE;

	goto cleanup;

 error:
	g_warning ("get_message: %d", status);
	if (status == E2K_HTTP_NOT_FOUND) {
		/* We don't change mfld->deleted_count, because the
		 * message may actually have gone away before the last
		 * time we recorded that.
		 */
		message_removed (mfld, folder, mmsg->href);
		set_exception (error, _("Message has been deleted"));
	} else
		set_exception (error, _("Error retrieving message"));

 cleanup:
	g_free (body);
	g_free (content_type);
	g_free (owner_email);

	return res;
}

gboolean
camel_exchange_utils_search (CamelService *service,
				const gchar *folder_name,
				const gchar *text,
				GPtrArray **found_uids, /* out */
				GError **error)
{
	ExchangeData *ed = get_data_for_service (service);
	ExchangeFolder *mfld;
	E2kRestriction *rn;
	const gchar *prop, *repl_uid;
	E2kResultIter *iter;
	E2kResult *result;
	E2kHTTPStatus status;
	GPtrArray *matches;

	g_return_val_if_fail (ed != NULL, FALSE);
	g_return_val_if_fail (found_uids != NULL, FALSE);

	mfld = folder_from_name (ed, folder_name, 0, error);
	if (!mfld)
		return FALSE;

	matches = g_ptr_array_new ();

	prop = E2K_PR_REPL_UID;
	rn = e2k_restriction_content (PR_BODY, E2K_FL_SUBSTRING, text);

	iter = e_folder_exchange_search_start (mfld->folder, NULL, &prop, 1, rn, NULL, TRUE);
	e2k_restriction_unref (rn);

	while ((result = e2k_result_iter_next (iter))) {
		repl_uid = e2k_properties_get_prop (result->props, E2K_PR_REPL_UID);
		if (repl_uid)
			g_ptr_array_add (matches, g_strdup (uidstrip (repl_uid)));
	}
	status = e2k_result_iter_free (iter);

	if (status == E2K_HTTP_UNPROCESSABLE_ENTITY) {
		set_exception (error, _("Mailbox does not support full-text searching"));

		g_ptr_array_foreach (matches, (GFunc) g_free, NULL);
		g_ptr_array_free (matches, TRUE);
		matches = NULL;
	} else {
		*found_uids = matches;
	}

	return matches != NULL;
}

gboolean
camel_exchange_utils_transfer_messages (CamelService *service,
					const gchar *source_name,
					const gchar *dest_name,
					GPtrArray *uids,
					gboolean delete_originals,
					GPtrArray **ret_uids, /* out */
					GError **error)
{
	ExchangeData *ed = get_data_for_service (service);
	ExchangeFolder *source, *dest;
	ExchangeMessage *mmsg;
	GPtrArray *hrefs, *new_uids;
	GHashTable *order;
	gpointer key, value;
	E2kResultIter *iter;
	E2kResult *result;
	E2kHTTPStatus status;
	const gchar *uid;
	gint i, num;
	CamelFolder *folder;

	g_return_val_if_fail (ed != NULL, FALSE);
	g_return_val_if_fail (ret_uids != NULL, FALSE);

	source = folder_from_name (
		ed, source_name, delete_originals ?
		MAPI_ACCESS_DELETE : 0, error);
	if (!source)
		return FALSE;

	dest = folder_from_name (
		ed, dest_name, MAPI_ACCESS_CREATE_CONTENTS, error);
	if (!dest)
		return FALSE;

	order = g_hash_table_new (NULL, NULL);
	hrefs = g_ptr_array_new ();
	new_uids = g_ptr_array_new ();
	for (i = 0; i < uids->len; i++) {
		mmsg = find_message (source, uids->pdata[i]);
		if (!mmsg)
			continue;

		if (!mmsg->href || !strrchr (mmsg->href, '/')) {
			g_warning ("%s: Message '%s' with invalid href '%s'", G_STRFUNC, (gchar *)uids->pdata[i], mmsg->href ? mmsg->href : "NULL");
			continue;
		}

		g_hash_table_insert (order, mmsg, GINT_TO_POINTER (i));
		g_ptr_array_add (hrefs, strrchr (mmsg->href, '/') + 1);
		g_ptr_array_add (new_uids, g_strdup (""));
	}

	folder = get_camel_folder (source);

	if (delete_originals && hrefs->len > 1 && folder) {
		camel_folder_freeze (folder);
	}

	iter = e_folder_exchange_transfer_start (source->folder, NULL,
						 dest->folder, hrefs,
						 delete_originals);

	while ((result = e2k_result_iter_next (iter))) {
		if (!e2k_properties_get_prop (result->props, E2K_PR_DAV_LOCATION))
			continue;
		uid = e2k_properties_get_prop (result->props, E2K_PR_REPL_UID);
		if (!uid)
			continue;

		if (delete_originals)
			source->deleted_count++;

		mmsg = find_message_by_href (source, result->href);
		if (!mmsg)
			continue;

		if (!g_hash_table_lookup_extended (order, mmsg, &key, &value))
			continue;
		num = GPOINTER_TO_UINT (value);
		if (num > new_uids->len)
			continue;

		g_free (new_uids->pdata[num]);
		new_uids->pdata[num] = g_strdup (uidstrip (uid));

		if (delete_originals)
			message_removed (source, folder, result->href);
	}
	status = e2k_result_iter_free (iter);

	if (delete_originals && hrefs->len > 1 && folder) {
		camel_folder_thaw (folder);
	}

	if (E2K_HTTP_STATUS_IS_SUCCESSFUL (status)) {
		*ret_uids = new_uids;
	} else {
		g_warning ("transferred_messages: %d", status);
		set_exception (error, _("Unable to move/copy messages"));
		g_ptr_array_free (new_uids, TRUE);
		new_uids = NULL;
	}

	g_ptr_array_free (hrefs, TRUE);
	g_hash_table_destroy (order);

	return new_uids != NULL;
}

static void
account_new_folder (ExchangeAccount *account, EFolder *folder, gpointer user_data)
{
	ExchangeData *ed = user_data;
	ExchangeHierarchy *hier;

	g_return_if_fail (ed != NULL);

	if (strcmp (e_folder_get_type_string (folder), "mail") != 0 &&
	    strcmp (e_folder_get_type_string (folder), "mail/public") != 0)
		return;

	if (ed->ignore_new_folder &&
	    !strcmp (e_folder_exchange_get_path (folder), ed->ignore_new_folder))
		return;

	hier = e_folder_exchange_get_hierarchy (folder);
	if (hier->type != EXCHANGE_HIERARCHY_PERSONAL &&
	    hier->type != EXCHANGE_HIERARCHY_FAVORITES &&
	    hier->type != EXCHANGE_HIERARCHY_FOREIGN)
		return;

	camel_exchange_store_folder_created (ed->estore, e_folder_get_name (folder), e_folder_get_physical_uri (folder));
}

static void
account_removed_folder (ExchangeAccount *account, EFolder *folder, gpointer user_data)
{
	ExchangeData *ed = user_data;
	ExchangeHierarchy *hier;

	g_return_if_fail (ed != NULL);

	if (strcmp (e_folder_get_type_string (folder), "mail") != 0 &&
	    strcmp (e_folder_get_type_string (folder), "mail/public") != 0)
		return;

	if (ed->ignore_removed_folder &&
	    !strcmp (e_folder_exchange_get_path (folder), ed->ignore_removed_folder))
		return;

	hier = e_folder_exchange_get_hierarchy (folder);
	if (hier->type != EXCHANGE_HIERARCHY_PERSONAL &&
	    hier->type != EXCHANGE_HIERARCHY_FAVORITES &&
	    hier->type != EXCHANGE_HIERARCHY_FOREIGN)
		return;

	camel_exchange_store_folder_deleted (ed->estore, e_folder_get_name (folder), e_folder_get_physical_uri (folder));
}

gboolean
camel_exchange_utils_get_folder_info (CamelService *service,
					const gchar *top,
					guint32 store_flags,
					GPtrArray **folder_names, /* out */
					GPtrArray **folder_uris, /* out */
					GArray **unread_counts, /* out */
					GArray **folder_flags, /* out */
					GError **error)
{
	ExchangeData *ed = get_data_for_service (service);
	GHashTable *known_uris;

	g_return_val_if_fail (ed != NULL, FALSE);
	g_return_val_if_fail (folder_names != NULL, FALSE);
	g_return_val_if_fail (folder_uris != NULL, FALSE);
	g_return_val_if_fail (unread_counts != NULL, FALSE);
	g_return_val_if_fail (folder_flags != NULL, FALSE);

	/* use lock here to have done scanning of foreign hierarchy
	   only once, and to not call get_folder_info_data simultaneously
	   from more than one thread */
	g_static_rec_mutex_lock (&ed->changed_msgs_mutex);

	*folder_names = NULL;
	*folder_uris = NULL;
	*unread_counts = NULL;
	*folder_flags = NULL;

	/* hash table of known uris. The uri key is shared with folder_uris, thus no need to free it */
	known_uris = g_hash_table_new (g_str_hash, g_str_equal);
	get_folder_info_data (ed, top, store_flags, known_uris, folder_names, folder_uris, unread_counts, folder_flags);
	g_hash_table_destroy (known_uris);

	if (ed->new_folder_id == 0) {
		ed->new_folder_id = g_signal_connect (ed->account, "new_folder", G_CALLBACK (account_new_folder), ed);
		ed->removed_folder_id = g_signal_connect (ed->account, "removed_folder", G_CALLBACK (account_removed_folder), ed);
	}

	g_static_rec_mutex_unlock (&ed->changed_msgs_mutex);

	return TRUE;
}

gboolean
camel_exchange_utils_send_message (CamelService *service,
				const gchar *from,
				GPtrArray *recipients,
				const GByteArray *message,
				GError **error)
{
	ExchangeData *ed = get_data_for_service (service);
	SoupMessage *msg;
	E2kHTTPStatus status;
	gchar *timestamp, *errmsg;
	GString *data;
	gint i;
	gboolean res = FALSE;

	/* This function is called from a transport service, thus it has no idea
	   about underlying folders and such. The check for estore != NULL is
	   necessary to be sure the transport service was called after the connect,
	   because the estore is used in other functions. */
	g_return_val_if_fail (ed != NULL, FALSE);
	g_return_val_if_fail (ed->estore != NULL, FALSE);

	if (!ed->mail_submission_uri) {
		set_exception (error, _("No mail submission URI for this mailbox"));
		return FALSE;
	}

	data = g_string_new (NULL);
	g_string_append_printf (data, "MAIL FROM:<%s>\r\n", from);
	for (i = 0; i < recipients->len; i++) {
		g_string_append_printf (data, "RCPT TO:<%s>\r\n", (gchar *)recipients->pdata[i]);
	}
	g_string_append (data, "\r\n");

	/* Exchange doesn't add a "Received" header to messages
	 * received via WebDAV.
	 */
	timestamp = e2k_make_timestamp_rfc822 (time (NULL));
	g_string_append_printf (data, "Received: from %s by %s; %s\r\n",
				g_get_host_name (), ed->account->exchange_server,
				timestamp);
	g_free (timestamp);

	g_string_append_len (data, (const gchar *)message->data, message->len);

	msg = e2k_soup_message_new_full (ed->ctx, ed->mail_submission_uri,
					 SOUP_METHOD_PUT, "message/rfc821",
					 SOUP_MEMORY_TAKE,
					 data->str, data->len);
	g_string_free (data, FALSE);
	soup_message_headers_append (msg->request_headers, "Saveinsent", "f");

	status = e2k_context_send_message (ed->ctx, NULL, msg);
	if (E2K_HTTP_STATUS_IS_SUCCESSFUL (status)) {
		res = TRUE;
	} else if (status == E2K_HTTP_NOT_FOUND) {
		set_exception (error, _("Server won't accept mail via Exchange transport"));
	} else if (status == E2K_HTTP_FORBIDDEN) {
		errmsg = g_strdup_printf (_("Your account does not have permission "
					    "to use <%s>\nas a From address."),
					  from);
		set_exception (error, errmsg);
		g_free (errmsg);
	} else if (status == E2K_HTTP_INSUFFICIENT_SPACE_ON_RESOURCE ||
		   status == E2K_HTTP_INTERNAL_SERVER_ERROR) {
		/* (500 is what it actually returns, 507 is what it should
		 * return, so we handle that too in case the behavior
		 * changes in the future.)
		 */
		E2K_KEEP_PRECEDING_COMMENT_OUT_OF_PO_FILES;
		set_exception (error, _("Could not send message.\n"
				     "This might mean that your account is over quota."));
	} else {
		g_warning ("sent_message: %d", status);
		set_exception (error, _("Could not send message"));
	}

	return res;
}

gboolean
camel_exchange_utils_create_folder (CamelService *service,
				const gchar *parent_name,
				const gchar *folder_name,
				gchar **folder_uri, /* out */
				guint32 *unread_count, /* out */
				guint32 *flags, /* out */
				GError **error)
{
	ExchangeData *ed = get_data_for_service (service);
	ExchangeAccountFolderResult result;
	EFolder *folder;
	gchar *path;

	g_return_val_if_fail (ed != NULL, FALSE);
	g_return_val_if_fail (folder_uri != NULL, FALSE);
	g_return_val_if_fail (unread_count != NULL, FALSE);
	g_return_val_if_fail (flags != NULL, FALSE);

	path = g_build_filename ("/", parent_name, folder_name, NULL);
	result = exchange_account_create_folder (ed->account, path, "mail");
	folder = exchange_account_get_folder (ed->account, path);
	g_free (path);

	switch (result) {
	case EXCHANGE_ACCOUNT_FOLDER_OK:
		if (folder)
			break;
		/* fall through */
	default:
		set_exception (error, _("Generic error"));
		return FALSE;

	case EXCHANGE_ACCOUNT_FOLDER_ALREADY_EXISTS:
		set_exception (error, _("Folder already exists"));
		return FALSE;

	case EXCHANGE_ACCOUNT_FOLDER_PERMISSION_DENIED:
		set_exception (error, _("Permission denied"));
		return FALSE;
	}

	*folder_uri = g_strdup (e_folder_get_physical_uri (folder));
	*unread_count = e_folder_get_unread_count (folder);
	*flags = 0;

	return TRUE;
}

gboolean
camel_exchange_utils_delete_folder (CamelService *service,
				const gchar *folder_name,
				GError **error)
{
	ExchangeData *ed = get_data_for_service (service);
	ExchangeAccountFolderResult result;
	EFolder *folder;
	gchar *path;

	g_return_val_if_fail (ed != NULL, FALSE);

	path = g_build_filename ("/", folder_name, NULL);
	folder = exchange_account_get_folder (ed->account, path);
	if (!folder) {
		set_exception (error, _("Folder doesn't exist"));
		g_free (path);
		return FALSE;
	}
	g_object_ref (folder);

	result = exchange_account_remove_folder (ed->account, path);
	g_free (path);

	switch (result) {
	case EXCHANGE_ACCOUNT_FOLDER_OK:
	case EXCHANGE_ACCOUNT_FOLDER_DOES_NOT_EXIST:
		g_hash_table_remove (ed->folders_by_name, folder_name);
		break;

	case EXCHANGE_ACCOUNT_FOLDER_PERMISSION_DENIED: /* Fall through */
	case EXCHANGE_ACCOUNT_FOLDER_UNSUPPORTED_OPERATION:
		set_exception (error, _("Permission denied"));
		g_object_unref (folder);
		return FALSE;

	default:
		set_exception (error, _("Generic error"));
		g_object_unref (folder);
		return FALSE;

	}

	g_object_unref (folder);

	return TRUE;
}

gboolean
camel_exchange_utils_rename_folder (CamelService *service,
				const gchar *old_name,
				const gchar *new_name,
				GPtrArray **folder_names, /* out */
				GPtrArray **folder_uris, /* out */
				GArray **unread_counts, /* out */
				GArray **folder_flags, /* out */
				GError **error)
{
	ExchangeData *ed = get_data_for_service (service);
	ExchangeFolder *mfld;
	ExchangeAccountFolderResult result;
	EFolder *folder;
	gchar *old_path, *new_path;
	GPtrArray *names = NULL, *uris = NULL;
	GArray *unread = NULL, *flags = NULL;
	gint i = 0, j = 0;
	gchar **folder_name;
	const gchar *uri;
	gchar *new_name_mod, *old_name_remove, *uri_unescaped, *old_name_mod = NULL;

	g_return_val_if_fail (ed != NULL, FALSE);
	g_return_val_if_fail (folder_names != NULL, FALSE);
	g_return_val_if_fail (folder_uris != NULL, FALSE);
	g_return_val_if_fail (unread_counts != NULL, FALSE);
	g_return_val_if_fail (folder_flags != NULL, FALSE);

	old_path = g_build_filename ("/", old_name, NULL);
	folder = exchange_account_get_folder (ed->account, old_path);
	if (!folder) {
		set_exception (error, _("Folder doesn't exist"));
		g_free (old_path);
		return FALSE;
	}
	new_path = g_build_filename ("/", new_name, NULL);

	ed->ignore_removed_folder = old_path;
	ed->ignore_new_folder = new_path;
	result = exchange_account_xfer_folder (ed->account, old_path, new_path, TRUE);
	folder = exchange_account_get_folder (ed->account, new_path);
	ed->ignore_new_folder = ed->ignore_removed_folder = NULL;
	g_free (old_path);
	g_free (new_path);

	switch (result) {
	case EXCHANGE_ACCOUNT_FOLDER_OK:
		mfld = g_hash_table_lookup (ed->folders_by_name, old_name);
		if (!mfld)
			break;

		g_object_unref (mfld->folder);
		mfld->folder = g_object_ref (folder);
		mfld->name = e_folder_exchange_get_path (folder) + 1;

		g_hash_table_steal (ed->folders_by_name, old_name);
		g_hash_table_insert (ed->folders_by_name, (gchar *)mfld->name, mfld);

		get_folder_info_data (ed, new_name, CAMEL_STORE_FOLDER_INFO_SUBSCRIBED, NULL, &names, &uris, &unread, &flags);

		g_hash_table_remove_all (mfld->messages_by_href);

		for (i = 0; i < mfld->messages->len; i++) {
			ExchangeMessage *mmsg;
			mmsg = mfld->messages->pdata[i];
			g_free (mmsg->href);
			mmsg->href = NULL;
		}

		if (is_online (ed) == ONLINE_MODE) {
			if (!get_folder_online (mfld, error))
				return FALSE;
		}

		for (i = 0; i < uris->len; i++) {
			uri = uris->pdata[i];
			if (uri == NULL)
				continue;

			uri_unescaped = g_uri_unescape_string (uri, NULL);
			new_name_mod = g_strconcat (new_name, "/", NULL);
			folder_name = g_strsplit (uri_unescaped, new_name_mod, 2);

			if (!folder_name[1]) {
				g_strfreev (folder_name);
				old_name_mod = g_strconcat (old_name, "/", NULL);
				folder_name = g_strsplit (uri_unescaped, old_name_mod, 2);
				g_free (old_name_mod);

				if (!folder_name[1]) {
					goto cont_free;
				}
			}

			old_name_remove = g_build_filename (old_name, "/", folder_name[1], NULL);

			mfld = g_hash_table_lookup (ed->folders_by_name, old_name_remove);

			/* If the lookup for the ExchangeFolder doesn't succeed then do
			not modify the corresponding entry in the hash table*/
			if (!mfld) {
				g_free (old_name_remove);
				goto cont_free;
			}

			new_path = g_build_filename ("/", new_name_mod, folder_name[1], NULL);
			old_path = g_build_filename ("/", old_name_remove, NULL);

			ed->ignore_removed_folder = old_path;
			ed->ignore_new_folder = new_path;
			result = exchange_account_xfer_folder (ed->account, old_path, new_path, TRUE);
			folder = exchange_account_get_folder (ed->account, new_path);
			ed->ignore_new_folder = ed->ignore_removed_folder = NULL;

			g_object_unref (mfld->folder);
			mfld->folder = g_object_ref (folder);
			mfld->name = e_folder_exchange_get_path (folder) + 1;

			g_hash_table_steal (ed->folders_by_name, old_name_remove);
			g_hash_table_insert (ed->folders_by_name, (gchar *)mfld->name, mfld);

			g_hash_table_remove_all (mfld->messages_by_href);

			for (j = 0; j < mfld->messages->len; j++) {
				ExchangeMessage *mmsg;
				mmsg = mfld->messages->pdata[j];
				g_free (mmsg->href);
				mmsg->href = NULL;
			}

			if (is_online (ed) == ONLINE_MODE) {
				if (!get_folder_online (mfld, error))
					return FALSE;
			}

			g_free (old_path);
			g_free (new_path);
 cont_free:		g_free (new_name_mod);
			g_free (uri_unescaped);
			g_strfreev (folder_name);
		}

		*folder_names = names;
		*folder_uris = uris;
		*unread_counts = unread;
		*folder_flags = flags;
		break;

	case EXCHANGE_ACCOUNT_FOLDER_DOES_NOT_EXIST:
		set_exception (error, _("Folder doesn't exist"));
		return FALSE;

	case EXCHANGE_ACCOUNT_FOLDER_PERMISSION_DENIED:
		set_exception (error, _("Permission denied"));
		return FALSE;

	default:
		set_exception (error, _("Generic error"));
		return FALSE;

	}

	return TRUE;
}

gboolean
camel_exchange_utils_subscribe_folder (CamelService *service,
					const gchar *folder_name,
					GError **error)
{
	ExchangeData *ed = get_data_for_service (service);
	ExchangeAccountFolderResult result;
	EFolder *folder;
	gchar *path;

	g_return_val_if_fail (ed != NULL, FALSE);

	path = g_build_filename ("/", folder_name, NULL);
	folder = exchange_account_get_folder (ed->account, path);
	if (!folder) {
		set_exception (error, _("Folder doesn't exist"));
		g_free (path);
		return FALSE;
	}
	g_free (path);
	g_object_ref (folder);

	if (e_folder_exchange_get_hierarchy (folder)->type != EXCHANGE_HIERARCHY_PUBLIC) {
		g_object_unref (folder);
		return TRUE;
	}

	if (!strcmp (e_folder_get_type_string (folder), "noselect")) {
		g_object_unref (folder);
		return TRUE;
	}

	result = exchange_account_add_favorite (ed->account, folder);
	g_object_unref (folder);

	switch (result) {
	case EXCHANGE_ACCOUNT_FOLDER_OK:
	case EXCHANGE_ACCOUNT_FOLDER_DOES_NOT_EXIST:
		break;

	case EXCHANGE_ACCOUNT_FOLDER_PERMISSION_DENIED:
		set_exception (error, _("Permission denied"));
		return FALSE;

	default:
		set_exception (error, _("Generic error"));
		return FALSE;
	}

	return TRUE;
}

gboolean
camel_exchange_utils_unsubscribe_folder (CamelService *service,
					const gchar *folder_name,
					GError **error)
{
	ExchangeData *ed = get_data_for_service (service);
	ExchangeAccountFolderResult result;
	EFolder *folder;
	gchar *path, *pub_name;

	g_return_val_if_fail (ed != NULL, FALSE);

	path = g_build_filename ("/", folder_name, NULL);
	folder = exchange_account_get_folder (ed->account, path);
	if (!folder) {
		set_exception (error, _("Folder doesn't exist"));
		g_free (path);
		return FALSE;
	}
	g_free (path);
	g_object_ref (folder);

	/* if (e_folder_exchange_get_hierarchy (folder)->type != EXCHANGE_HIERARCHY_FAVORITES) {
	   Should use above check, but the internal uri is the same for both
	   public and favorite hierarchies and any of them can be used for the check */
	if (!exchange_account_is_favorite_folder (ed->account, folder)) {
		g_object_unref (folder);
		return TRUE;
	}

	g_object_unref (folder);

	pub_name = strrchr (folder_name, '/');
	path = g_build_filename ("/favorites", pub_name, NULL);
	folder = exchange_account_get_folder (ed->account, path);
	if (!folder) {
		set_exception (error, _("Folder doesn't exist"));
		g_free (path);
		return FALSE;
	}
	g_object_ref (folder);

	result = exchange_account_remove_favorite (ed->account, folder);

	switch (result) {
	case EXCHANGE_ACCOUNT_FOLDER_OK:
	case EXCHANGE_ACCOUNT_FOLDER_DOES_NOT_EXIST:
		g_hash_table_remove (ed->folders_by_name, path + 1);
		break;

	case EXCHANGE_ACCOUNT_FOLDER_PERMISSION_DENIED:
		set_exception (error, _("Permission denied"));
		g_object_unref (folder);
		g_free (path);
		return FALSE;

	default:
		set_exception (error, _("Generic error"));
		g_object_unref (folder);
		g_free (path);
		return FALSE;

	}

	g_object_unref (folder);
	g_free (path);

	return TRUE;
}

gboolean
camel_exchange_utils_is_subscribed_folder (CamelService *service,
					const gchar *folder_name,
					gboolean *is_subscribed, /* out */
					GError **error)
{
	ExchangeData *ed = get_data_for_service (service);
	EFolder *folder;
	gchar *path;

	g_return_val_if_fail (ed != NULL, FALSE);
	g_return_val_if_fail (is_subscribed != NULL, FALSE);

	*is_subscribed = FALSE;

	path = g_build_filename ("/", folder_name, NULL);
	folder = exchange_account_get_folder (ed->account, path);
	if (!folder) {
		g_free (path);
		return TRUE;
	}
	g_free (path);
	g_object_ref (folder);

	if (exchange_account_is_favorite_folder (ed->account, folder))
		*is_subscribed = TRUE;

	g_object_unref (folder);

	return TRUE;
}
