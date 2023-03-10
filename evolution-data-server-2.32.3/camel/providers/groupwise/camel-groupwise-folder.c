/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-groupwise-folder.c: class for an groupwise folder */

/*
 * Authors:
 *  Sivaiah Nallagatla <snallagatla@novell.com>
 *  parthasarathi susarla <sparthasarathi@novell.com>
 *  Sankar P <psankar@novell.com>
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 */

/* This file is broken and suffers from multiple author syndrome.
This needs to be rewritten with a lot of functions cleaned up.

There are a lot of places where code is unneccesarily duplicated,
which needs to be better organized via functions */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <glib/gi18n-lib.h>

#include <e-gw-connection.h>
#include <e-gw-item.h>

#include "camel-groupwise-folder.h"
#include "camel-groupwise-journal.h"
#include "camel-groupwise-private.h"
#include "camel-groupwise-store.h"
#include "camel-groupwise-summary.h"
#include "camel-groupwise-utils.h"

#define ADD_JUNK_ENTRY 1
#define REMOVE_JUNK_ENTRY -1
#define JUNK_FOLDER "Junk Mail"
#define READ_CURSOR_MAX_IDS 50
#define MAX_ATTACHMENT_SIZE 1*1024*1024   /*In bytes*/
#define GROUPWISE_BULK_DELETE_LIMIT 100

#define CAMEL_GROUPWISE_FOLDER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), CAMEL_TYPE_GROUPWISE_FOLDER, CamelGroupwiseFolderPrivate))

struct _CamelGroupwiseFolderPrivate {

#ifdef ENABLE_THREADS
	GStaticMutex search_lock;	/* for locking the search object */
	GStaticRecMutex cache_lock;	/* for locking the cache object */
#endif

};

extern gint camel_application_is_exiting;

/*prototypes*/
static gboolean groupwise_transfer_messages_to (CamelFolder *source, GPtrArray *uids, CamelFolder *destination, GPtrArray **transferred_uids, gboolean delete_originals, GError **error);
void convert_to_calendar (EGwItem *item, gchar **str, gint *len);
static void convert_to_task (EGwItem *item, gchar **str, gint *len);
static void convert_to_note (EGwItem *item, gchar **str, gint *len);
static void gw_update_all_items ( CamelFolder *folder, GList *item_list, GError **error);
static void groupwise_populate_details_from_item (CamelMimeMessage *msg, EGwItem *item);
static void groupwise_populate_msg_body_from_item (EGwConnection *cnc, CamelMultipart *multipart, EGwItem *item, gchar *body);
static void groupwise_msg_set_recipient_list (CamelMimeMessage *msg, EGwItem *item);
static void gw_update_cache ( CamelFolder *folder, GList *item_list, GError **error, gboolean uid_flag);
static CamelMimeMessage *groupwise_folder_item_to_msg ( CamelFolder *folder, EGwItem *item, GError **error );
static gchar * groupwise_get_filename (CamelFolder *folder, const gchar *uid, GError **error);
static const gchar *get_from_from_org (EGwItemOrganizer *org);
static void groupwise_refresh_folder(CamelFolder *folder, GError **error);
static gboolean groupwise_sync (CamelFolder *folder, gboolean expunge, CamelMessageInfo *update_single, GError **error);

#define d(x)

static const gchar * GET_ITEM_VIEW_WITH_CACHE = "peek default recipient threading attachments subject status priority startDate created delivered size recurrenceKey message notification";
static const gchar * GET_ITEM_VIEW_WITHOUT_CACHE = "peek default recipient threading hasAttachment subject status priority startDate created delivered size recurrenceKey";

G_DEFINE_TYPE (CamelGroupwiseFolder, camel_groupwise_folder, CAMEL_TYPE_OFFLINE_FOLDER)

static gchar *
groupwise_get_filename (CamelFolder *folder, const gchar *uid, GError **error)
{
	CamelGroupwiseFolder *gw_folder = CAMEL_GROUPWISE_FOLDER(folder);

	return camel_data_cache_get_filename (gw_folder->cache, "cache", uid, error);
}

/* Get a message from cache if available otherwise get it from server */
static CamelMimeMessage *
groupwise_folder_get_message( CamelFolder *folder, const gchar *uid, GError **error )
{
	CamelMimeMessage *msg = NULL;
	CamelGroupwiseFolder *gw_folder;
	CamelGroupwiseStore *gw_store;
	CamelGroupwiseMessageInfo *mi = NULL;
	CamelStore *parent_store;
	gchar *container_id;
	EGwConnectionStatus status;
	EGwConnection *cnc;
	EGwItem *item;
	CamelStream *stream, *cache_stream;
	const gchar *full_name;

	full_name = camel_folder_get_full_name (folder);
	parent_store = camel_folder_get_parent_store (folder);

	gw_folder = CAMEL_GROUPWISE_FOLDER (folder);
	gw_store = CAMEL_GROUPWISE_STORE (parent_store);

	/* see if it is there in cache */

	mi = (CamelGroupwiseMessageInfo *) camel_folder_summary_uid (folder->summary, uid);
	if (mi == NULL) {
		g_set_error (
			error, CAMEL_FOLDER_ERROR,
			CAMEL_FOLDER_ERROR_INVALID_UID,
			_("Cannot get message: %s\n  %s"), uid, _("No such message"));
		return NULL;
	}
	cache_stream  = camel_data_cache_get (gw_folder->cache, "cache", uid, NULL);
	stream = camel_stream_mem_new ();
	if (cache_stream) {
		msg = camel_mime_message_new ();
		camel_stream_reset (stream, NULL);
		camel_stream_write_to_stream (cache_stream, stream, NULL);
		camel_stream_reset (stream, NULL);
		if (camel_data_wrapper_construct_from_stream ((CamelDataWrapper *) msg, stream, error) == -1) {
			if (errno == EINTR) {
				g_object_unref (msg);
				g_object_unref (cache_stream);
				g_object_unref (stream);
				camel_message_info_free (&mi->info);
				return NULL;
			} else {
				g_prefix_error (
					error, _("Cannot get message %s: "), uid);
				g_object_unref (msg);
				msg = NULL;
			}
		}
		g_object_unref (cache_stream);
	}
	g_object_unref (stream);

	if (msg != NULL) {
		camel_message_info_free (&mi->info);
		return msg;
	}

	if (((CamelOfflineStore *) gw_store)->state == CAMEL_OFFLINE_STORE_NETWORK_UNAVAIL) {
		g_set_error (
			error, CAMEL_SERVICE_ERROR,
			CAMEL_SERVICE_ERROR_UNAVAILABLE,
			_("This message is not available in offline mode."));
		camel_message_info_free (&mi->info);
		return NULL;
	}

	/* Check if we are really offline */
	if (!camel_groupwise_store_connected (gw_store, NULL)) {
		g_set_error (
			error, CAMEL_SERVICE_ERROR,
			CAMEL_SERVICE_ERROR_UNAVAILABLE,
			_("This message is not available in offline mode."));
		camel_message_info_free (&mi->info);
		return NULL;
	}

	container_id =  g_strdup (camel_groupwise_store_container_id_lookup (gw_store, full_name));

	cnc = cnc_lookup (gw_store->priv);

	status = e_gw_connection_get_item (cnc, container_id, uid, GET_ITEM_VIEW_WITH_CACHE, &item);
	if (status == E_GW_CONNECTION_STATUS_INVALID_CONNECTION)
		status = e_gw_connection_get_item (cnc, container_id, uid, GET_ITEM_VIEW_WITH_CACHE, &item);
	if (status != E_GW_CONNECTION_STATUS_OK) {
		g_free (container_id);
		g_set_error (
			error, CAMEL_SERVICE_ERROR,
			CAMEL_SERVICE_ERROR_INVALID,
			_("Could not get message"));
		camel_message_info_free (&mi->info);
		return NULL;
	}

	msg = groupwise_folder_item_to_msg (folder, item, NULL);
	if (!msg) {
		g_set_error (
			error, CAMEL_SERVICE_ERROR,
			CAMEL_SERVICE_ERROR_INVALID,
			_("Could not get message"));
		g_free (container_id);
		camel_message_info_free (&mi->info);

		return NULL;
	}

	if (msg) {
		camel_medium_set_header (CAMEL_MEDIUM (msg), "X-Evolution-Source", groupwise_base_url_lookup (gw_store->priv));
		mi->info.dirty = TRUE;
		camel_folder_summary_touch (folder->summary);
	}

	/* add to cache */
	CAMEL_GROUPWISE_FOLDER_REC_LOCK (folder, cache_lock);
	if ((cache_stream = camel_data_cache_add (gw_folder->cache, "cache", uid, NULL))) {
		if (camel_data_wrapper_write_to_stream ((CamelDataWrapper *) msg, cache_stream, NULL) == -1
				|| camel_stream_flush (cache_stream, NULL) == -1)
			camel_data_cache_remove (gw_folder->cache, "cache", uid, NULL);
		g_object_unref (cache_stream);
	}

	CAMEL_GROUPWISE_FOLDER_REC_UNLOCK (folder, cache_lock);

	camel_message_info_free (&mi->info);
	g_free (container_id);
	g_object_unref (item);
	return msg;
}

/* create a mime message out of an gwitem */
static void
groupwise_set_mail_message_dates (CamelMimeMessage *msg, EGwItem *item)
{
	gchar *dtstring = NULL;

	dtstring = e_gw_item_get_creation_date (item);
	if (dtstring) {
		gint offset = 0;
		time_t actual_time = e_gw_connection_get_date_from_string (dtstring);
		camel_mime_message_set_date (msg, actual_time, offset);
	} else {
		time_t actual_time;
		gint offset = 0;
		dtstring = e_gw_item_get_delivered_date (item);
		if (dtstring) {
			actual_time = e_gw_connection_get_date_from_string (dtstring);
		} else
			actual_time = (time_t) 0;
		camel_mime_message_set_date (msg, actual_time, offset);
	}
}

static void
groupwise_set_mail_mi_dates (CamelGroupwiseMessageInfo *mi, EGwItem *item)
{
	gchar *sent_date = NULL, *received_date = NULL;
	time_t actual_time = (time_t) 0;

	sent_date = e_gw_item_get_creation_date(item);
	received_date = e_gw_item_get_delivered_date (item);

	if (sent_date) {
		actual_time = e_gw_connection_get_date_from_string (sent_date);
		mi->info.date_sent = actual_time;
	}

	if (received_date) {
		actual_time = e_gw_connection_get_date_from_string (received_date);
		mi->info.date_received = actual_time;
	} else
		mi->info.date_received = actual_time;

	if (!sent_date)
		mi->info.date_sent = actual_time;
}

static void
groupwise_populate_details_from_item (CamelMimeMessage *msg, EGwItem *item)
{
	EGwItemType type;
	gchar *dtstring = NULL;
	gchar *temp_str = NULL;

	temp_str = (gchar *)e_gw_item_get_subject(item);
	if (temp_str)
		camel_mime_message_set_subject (msg, temp_str);
	type = e_gw_item_get_item_type (item);

	if (type == E_GW_ITEM_TYPE_APPOINTMENT  || type == E_GW_ITEM_TYPE_NOTE || type == E_GW_ITEM_TYPE_TASK) {
		time_t actual_time;
		gint offset = 0;
		dtstring = e_gw_item_get_start_date (item);
		actual_time = e_gw_connection_get_date_from_string (dtstring);
		camel_mime_message_set_date (msg, actual_time, offset);
		return;
	}

	groupwise_set_mail_message_dates (msg, item);
}

/* convert an item to a msg body. set content type etc. */
static void
groupwise_populate_msg_body_from_item (EGwConnection *cnc, CamelMultipart *multipart, EGwItem *item, gchar *body)
{
	CamelMimePart *part;
	EGwItemType type;
	const gchar *temp_body = NULL;

	part = camel_mime_part_new ();
	camel_mime_part_set_encoding(part, CAMEL_TRANSFER_ENCODING_8BIT);

	if (!body) {
		temp_body = e_gw_item_get_message (item);
		if (!temp_body) {
			gint len = 0;
			EGwConnectionStatus status;
			status = e_gw_connection_get_attachment (cnc,
					e_gw_item_get_msg_body_id (item), 0, -1,
					(const gchar **)&temp_body, &len);
			if (status == E_GW_CONNECTION_STATUS_INVALID_CONNECTION)
				status = e_gw_connection_get_attachment (cnc,
					e_gw_item_get_msg_body_id (item), 0, -1,
					(const gchar **)&temp_body, &len);
			if (status != E_GW_CONNECTION_STATUS_OK) {
				g_warning ("Could not get Messagebody\n");
			}
		}
	}

	type = e_gw_item_get_item_type (item);
	switch (type) {

		case E_GW_ITEM_TYPE_APPOINTMENT:
		case E_GW_ITEM_TYPE_TASK:
		case E_GW_ITEM_TYPE_NOTE:
			{
				gchar *cal_buffer = NULL;
				gint len = 0;
				if (type==E_GW_ITEM_TYPE_APPOINTMENT)
					convert_to_calendar (item, &cal_buffer, &len);
				else if (type == E_GW_ITEM_TYPE_TASK)
					convert_to_task (item, &cal_buffer, &len);
				else
					convert_to_note (item, &cal_buffer, &len);

				camel_mime_part_set_content(part, cal_buffer, len, "text/calendar");
				g_free (cal_buffer);
				break;
			}
		case E_GW_ITEM_TYPE_NOTIFICATION:
		case E_GW_ITEM_TYPE_MAIL:
			if (body)
				camel_mime_part_set_content(part, body, strlen(body), "text/html");
			else if (temp_body)
				camel_mime_part_set_content(part, temp_body, strlen(temp_body), e_gw_item_get_msg_content_type (item));
			else
				camel_mime_part_set_content(part, " ", strlen(" "), "text/html");
			break;

		default:
			break;

	}

	camel_multipart_set_boundary (multipart, NULL);
	camel_multipart_add_part (multipart, part);
	g_object_unref (part);
}

/* Set the recipients list in the message from the item */
static void
groupwise_msg_set_recipient_list (CamelMimeMessage *msg, EGwItem *item)
{
	GSList *recipient_list;
	EGwItemOrganizer *org;
	struct _camel_header_address *ha;
	gchar *subs_email;
	struct _camel_header_address *to_list = NULL, *cc_list = NULL, *bcc_list=NULL;

	org = e_gw_item_get_organizer (item);
	recipient_list = e_gw_item_get_recipient_list (item);

	if (recipient_list) {
		GSList *rl;
		gchar *status_opt = NULL;
		gboolean enabled;

		for (rl = recipient_list; rl != NULL; rl = rl->next) {
			EGwItemRecipient *recp = (EGwItemRecipient *) rl->data;
			enabled = recp->status_enabled;

			if (!recp->email) {
				ha=camel_header_address_new_group(recp->display_name);
			} else {
				ha=camel_header_address_new_name(recp->display_name,recp->email);
			}

			if (recp->type == E_GW_ITEM_RECIPIENT_TO) {
				if (recp->status_enabled)
					status_opt = g_strconcat (status_opt ? status_opt : "" , "TO", ";",NULL);
				camel_header_address_list_append(&to_list, ha);
			} else if (recp->type == E_GW_ITEM_RECIPIENT_CC) {
				if (recp->status_enabled)
					status_opt = g_strconcat (status_opt ? status_opt : "", "CC", ";",NULL);
				camel_header_address_list_append(&cc_list,ha);

			} else if (recp->type == E_GW_ITEM_RECIPIENT_BC) {
				if (recp->status_enabled)
					status_opt = g_strconcat (status_opt ? status_opt : "", "BCC", ";",NULL);
				camel_header_address_list_append(&bcc_list,ha);
			} else {
				camel_header_address_unref(ha);
			}
			if (recp->status_enabled) {
				status_opt = g_strconcat (status_opt,
						recp->display_name,";",
						recp->email,";",
						recp->delivered_date ? recp->delivered_date :  "", ";",
						recp->opened_date ? recp->opened_date : "", ";",
						recp->accepted_date ? recp->accepted_date : "", ";",
						recp->deleted_date ? recp->deleted_date : "", ";",
						recp->declined_date ? recp->declined_date : "", ";",
						recp->completed_date ? recp->completed_date : "", ";",
						recp->undelivered_date ? recp->undelivered_date : "", ";",
						"::", NULL);

			}
		}

		/* The status tracking code is working fine. someone need to remove this */
		if (enabled) {
			camel_medium_add_header ( CAMEL_MEDIUM (msg), "X-gw-status-opt", (const gchar *)status_opt);
			g_free (status_opt);
		}
	}

	if (to_list) {
		subs_email=camel_header_address_list_encode(to_list);
		camel_medium_set_header( CAMEL_MEDIUM(msg), "To", subs_email);
		g_free(subs_email);
		camel_header_address_list_clear(&to_list);
	}

	if (cc_list) {
		subs_email=camel_header_address_list_encode(cc_list);
		camel_medium_set_header( CAMEL_MEDIUM(msg), "Cc", subs_email);
		g_free(subs_email);
		camel_header_address_list_clear(&cc_list);
	}

	if (bcc_list) {
		subs_email=camel_header_address_list_encode(bcc_list);
		camel_medium_set_header( CAMEL_MEDIUM(msg), "Bcc", subs_email);
		g_free(subs_email);
		camel_header_address_list_clear(&bcc_list);
	}

	if (org) {
		if (org->display_name && org->display_name[0] && org->email != NULL && org->email[0] != '\0') {
			org->display_name = g_strdelimit (org->display_name, "<>", ' ');
			ha=camel_header_address_new_name(org->display_name, org->email);
		} else if (org->email)
			ha=camel_header_address_new_name(org->email, org->email);
		else if (org->display_name)
			ha=camel_header_address_new_group(org->display_name);
		else
			ha = NULL;
		if (ha) {
			subs_email = camel_header_address_list_encode (ha);
			camel_medium_set_header (CAMEL_MEDIUM (msg), "From", subs_email);
			camel_header_address_unref (ha);
			g_free (subs_email);
		}
	}
}

/* code to rename a folder. all the "meta nonsense" code should simply go away */
static void
groupwise_folder_rename (CamelFolder *folder, const gchar *new)
{
	CamelGroupwiseFolder *gw_folder;
	CamelGroupwiseStore *gw_store;
	CamelStore *parent_store;
	gchar *folder_dir, *summary_path, *state_file, *storage_path;
	gchar *folders;

	parent_store = camel_folder_get_parent_store (folder);

	gw_folder = CAMEL_GROUPWISE_FOLDER (folder);
	gw_store = CAMEL_GROUPWISE_STORE (parent_store);

	storage_path = storage_path_lookup (gw_store->priv);

	folders = g_strconcat (storage_path, "/folders", NULL);
	folder_dir = e_path_to_physical (folders, new);
	g_free (folders);

	summary_path = g_strdup_printf ("%s/summary", folder_dir);

	CAMEL_GROUPWISE_FOLDER_REC_LOCK (folder, cache_lock);
	camel_data_cache_set_path (gw_folder->cache, folder_dir);
	CAMEL_GROUPWISE_FOLDER_REC_UNLOCK (folder, cache_lock);

	((CamelFolderClass *)camel_groupwise_folder_parent_class)->rename(folder, new);
	camel_folder_summary_set_filename (folder->summary, summary_path);

	state_file = g_strdup_printf ("%s/cmeta", folder_dir);
	camel_object_set_state_filename (CAMEL_OBJECT (folder), state_file);
	g_free (state_file);

	g_free (summary_path);
	g_free (folder_dir);
}

static GPtrArray *
groupwise_folder_search_by_expression (CamelFolder *folder, const gchar *expression, GError **error)
{
	CamelGroupwiseFolder *gw_folder = CAMEL_GROUPWISE_FOLDER(folder);
	GPtrArray *matches;

	CAMEL_GROUPWISE_FOLDER_LOCK(gw_folder, search_lock);
	camel_folder_search_set_folder (gw_folder->search, folder);
	matches = camel_folder_search_search(gw_folder->search, expression, NULL, error);
	CAMEL_GROUPWISE_FOLDER_UNLOCK(gw_folder, search_lock);

	return matches;
}

static guint32
groupwise_folder_count_by_expression (CamelFolder *folder, const gchar *expression, GError **error)
{
	CamelGroupwiseFolder *gw_folder = CAMEL_GROUPWISE_FOLDER(folder);
	guint32 matches;

	CAMEL_GROUPWISE_FOLDER_LOCK(gw_folder, search_lock);
	camel_folder_search_set_folder (gw_folder->search, folder);
	matches = camel_folder_search_count (gw_folder->search, expression, error);
	CAMEL_GROUPWISE_FOLDER_UNLOCK(gw_folder, search_lock);

	return matches;
}

static GPtrArray *
groupwise_folder_search_by_uids(CamelFolder *folder, const gchar *expression, GPtrArray *uids, GError **error)
{
	CamelGroupwiseFolder *gw_folder = CAMEL_GROUPWISE_FOLDER(folder);
	GPtrArray *matches;

	if (uids->len == 0)
		return g_ptr_array_new();

	CAMEL_GROUPWISE_FOLDER_LOCK(gw_folder, search_lock);

	camel_folder_search_set_folder(gw_folder->search, folder);
	matches = camel_folder_search_search(gw_folder->search, expression, uids, error);

	CAMEL_GROUPWISE_FOLDER_UNLOCK(gw_folder, search_lock);

	return matches;
}

static void
groupwise_folder_search_free (CamelFolder *folder, GPtrArray *uids)
{
	CamelGroupwiseFolder *gw_folder = CAMEL_GROUPWISE_FOLDER(folder);

	g_return_if_fail (gw_folder->search);

	CAMEL_GROUPWISE_FOLDER_LOCK(gw_folder, search_lock);

	camel_folder_search_free_result (gw_folder->search, uids);

	CAMEL_GROUPWISE_FOLDER_UNLOCK(gw_folder, search_lock);

}

/******************* functions specific to Junk Mail Handling**************/
static void
free_node (EGwJunkEntry *entry)
{
	if (entry) {
		g_free (entry->id);
		g_free (entry->match);
		g_free (entry->matchType);
		g_free (entry->lastUsed);
		g_free (entry->modified);
		g_free (entry);
	}
}

/* This is a point of contention. We behave like the GW client and update our junk list
in the same way as the GroupWise client. Add senders to junk list */
static void
update_junk_list (CamelStore *store, CamelMessageInfo *info, gint flag)
{
	gchar **email = NULL, *from = NULL;
	gint index = 0;
	CamelGroupwiseStore *gw_store= CAMEL_GROUPWISE_STORE(store);
	CamelGroupwiseStorePrivate  *priv = gw_store->priv;
	EGwConnection *cnc = cnc_lookup (priv);
	EGwConnectionStatus status;

	if (!(from = g_strdup (camel_message_info_from (info))))
		goto error;

	email = g_strsplit_set (from, "<>", -1);

	if (from[0] == '<') {
		/* g_strsplit_set will add a dummy empty string as the first string if the first character in the
		original string is one of the delimiters that you want to suppress.
		Refer to g_strsplit_set documentation */
		index = 1;
	}

	if (!email || !email[index])
		goto error;

	if (flag == ADD_JUNK_ENTRY) {
		status = e_gw_connection_create_junk_entry (cnc, email[index], "email", "junk");
		if (status == E_GW_CONNECTION_STATUS_INVALID_CONNECTION)
			status = e_gw_connection_create_junk_entry (cnc, email[index], "email", "junk");

	} else if (flag == REMOVE_JUNK_ENTRY) {
		GList *list = NULL;
		EGwJunkEntry *entry;
		status = e_gw_connection_get_junk_entries (cnc, &list);
		if (status == E_GW_CONNECTION_STATUS_INVALID_CONNECTION)
			status = e_gw_connection_get_junk_entries (cnc, &list);

		if (status == E_GW_CONNECTION_STATUS_OK) {
			while (list) {
				entry = list->data;
				if (!g_ascii_strcasecmp (entry->match, email[index])) {
					e_gw_connection_remove_junk_entry (cnc, entry->id);
				}
				list = list->next;
			}
			g_list_foreach (list, (GFunc) free_node, NULL);
		}
	}

error:
	g_free (from);
	g_strfreev (email);
}

static void
move_to_mailbox (CamelFolder *folder, CamelMessageInfo *info, GError **error)
{
	CamelFolder *dest;
	CamelStore *parent_store;
	GPtrArray *uids;
	const gchar *uid = camel_message_info_uid (info);

	parent_store = camel_folder_get_parent_store (folder);

	uids = g_ptr_array_new ();
	g_ptr_array_add (uids, (gpointer) uid);

	dest = camel_store_get_folder (parent_store, "Mailbox", 0, error);
	camel_message_info_set_flags (info, CAMEL_MESSAGE_DELETED|CAMEL_MESSAGE_JUNK|CAMEL_MESSAGE_JUNK_LEARN|CAMEL_GW_MESSAGE_NOJUNK|CAMEL_GW_MESSAGE_JUNK, 0);
	if (dest)
		groupwise_transfer_messages_to (folder, uids, dest, NULL, TRUE, error);
	else
		g_warning ("No Mailbox folder found");

	update_junk_list (parent_store, info, REMOVE_JUNK_ENTRY);
}

static void
move_to_junk (CamelFolder *folder, CamelMessageInfo *info, GError **error)
{
	CamelFolder *dest;
	CamelFolderInfo *fi;
	CamelStore *parent_store;
	GPtrArray *uids;
	const gchar *uid = camel_message_info_uid (info);

	parent_store = camel_folder_get_parent_store (folder);

	uids = g_ptr_array_new ();
	g_ptr_array_add (uids, (gpointer) uid);

	dest = camel_store_get_folder (parent_store, JUNK_FOLDER, 0, error);

	if (dest)
		groupwise_transfer_messages_to (folder, uids, dest, NULL, TRUE, error);
	else {
		fi = create_junk_folder (parent_store);
		dest = camel_store_get_folder (parent_store, JUNK_FOLDER, 0, error);
		if (!dest)
			g_warning ("Could not get JunkFolder:Message not moved");
		else
			groupwise_transfer_messages_to (folder, uids, dest, NULL, TRUE, error);
	}
	update_junk_list (parent_store, info, ADD_JUNK_ENTRY);
}

/********************* back to folder functions*************************/

static gboolean
groupwise_sync_summary (CamelFolder *folder, GError **error)
{
	CamelStoreInfo *si;
	CamelStore *parent_store;
	guint32 unread, total;
	const gchar *full_name;

	full_name = camel_folder_get_full_name (folder);
	parent_store = camel_folder_get_parent_store (folder);

	camel_folder_summary_save_to_db (folder->summary, error);

	si = camel_store_summary_path ((CamelStoreSummary *) ((CamelGroupwiseStore *) parent_store)->summary, full_name);

	total = camel_folder_summary_count (folder->summary);
	unread = folder->summary->unread_count;

	if (si) {
		si->unread = unread;
		si->total = total;
	}

	camel_store_summary_touch ((CamelStoreSummary *)((CamelGroupwiseStore *)parent_store)->summary);
	camel_store_summary_save ((CamelStoreSummary *)((CamelGroupwiseStore *)parent_store)->summary);

	return TRUE;
}

static void
sync_flags (CamelFolder *folder, GList *uids)
{
	GList *l;
	CamelMessageInfo *info = NULL;
	CamelGroupwiseMessageInfo *gw_info;

	for (l = uids; l != NULL; l = g_list_next (l))
	{
		info = camel_folder_summary_uid (folder->summary, l->data);
		gw_info = (CamelGroupwiseMessageInfo *) info;

		if (!info)
			continue;

		gw_info->info.flags &= ~CAMEL_MESSAGE_FOLDER_FLAGGED;
		gw_info->info.dirty = 1;
		gw_info->server_flags = gw_info->info.flags;
		camel_folder_summary_touch (folder->summary);

		camel_message_info_free (info);
	}
}

static gboolean
groupwise_set_message_flags (CamelFolder *folder, const gchar *uid, guint32 flags, guint32 set)
{
	CamelMessageInfo *info;
	gint res;
	const gchar *sync_immediately;

	g_return_val_if_fail (folder->summary != NULL, FALSE);

	info = camel_folder_summary_uid (folder->summary, uid);
	if (info == NULL)
		return FALSE;

	res = camel_message_info_set_flags (info, flags, set);

	sync_immediately = g_getenv ("GW_SYNC_IMMEDIATE");

	if (sync_immediately)
		groupwise_sync (folder, FALSE, info, NULL);

	camel_message_info_free (info);
	return res;
}

static gboolean
groupwise_sync_all (CamelFolder *folder, gboolean expunge, GError **error)
{
	return groupwise_sync (folder, expunge, NULL, error);
}

/* This may need to be reorganized. */
static gboolean
groupwise_sync (CamelFolder *folder, gboolean expunge, CamelMessageInfo *update_single, GError **error)
{
	CamelGroupwiseStore *gw_store;
	CamelGroupwiseFolder *gw_folder;
	CamelMessageInfo *info = NULL;
	CamelGroupwiseMessageInfo *gw_info;
	CamelStore *parent_store;
	GList *read_items = NULL, *deleted_read_items = NULL, *unread_items = NULL;
	flags_diff_t diff, unset_flags;
	const gchar *container_id;
	CamelFolderChangeInfo *changes;
	EGwConnectionStatus status;
	EGwConnection *cnc;
	const gchar *full_name;
	gint count, i;
	gboolean success;
	GList *deleted_items, *deleted_head;

	full_name = camel_folder_get_full_name (folder);
	parent_store = camel_folder_get_parent_store (folder);

	gw_folder = CAMEL_GROUPWISE_FOLDER (folder);
	gw_store = CAMEL_GROUPWISE_STORE (parent_store);

	deleted_items = deleted_head = NULL;

	if (((CamelOfflineStore *) gw_store)->state == CAMEL_OFFLINE_STORE_NETWORK_UNAVAIL)
		return groupwise_sync_summary (folder, error);

	camel_service_lock (CAMEL_SERVICE (gw_store), CAMEL_SERVICE_REC_CONNECT_LOCK);
	if (!camel_groupwise_store_connected (gw_store, NULL)) {
		camel_service_unlock (CAMEL_SERVICE (gw_store), CAMEL_SERVICE_REC_CONNECT_LOCK);
		return TRUE;
	}
	camel_service_unlock (CAMEL_SERVICE (gw_store), CAMEL_SERVICE_REC_CONNECT_LOCK);

	cnc = cnc_lookup (gw_store->priv);
	container_id =  camel_groupwise_store_container_id_lookup (gw_store, full_name);

	if (folder->folder_flags & CAMEL_FOLDER_HAS_BEEN_DELETED)
		return TRUE;

	changes = camel_folder_change_info_new ();
	camel_folder_summary_prepare_fetch_all (folder->summary, error);
	count = camel_folder_summary_count (folder->summary);
	CAMEL_GROUPWISE_FOLDER_REC_LOCK (folder, cache_lock);
	for (i=0; i < count; i++) {
		guint32 flags = 0;

		if (update_single != NULL) {
			info = update_single;
			camel_message_info_ref (info);
			count = 1;
		} else
			info = camel_folder_summary_index (folder->summary, i);

		gw_info = (CamelGroupwiseMessageInfo *) info;

		/**Junk Mail handling**/
		if (!info)
			continue;
		flags = camel_message_info_flags (info);

		if (!(flags & CAMEL_MESSAGE_FOLDER_FLAGGED)) {
				camel_message_info_free(info);
				continue;
		}

		if ((flags & CAMEL_MESSAGE_JUNK) && strcmp(camel_folder_get_name(folder), JUNK_FOLDER)) {
			/*marked a message junk*/
			move_to_junk (folder, info, error);
			camel_folder_summary_remove_uid (folder->summary, camel_message_info_uid(info));
			camel_data_cache_remove (gw_folder->cache, "cache", camel_message_info_uid(info), NULL);
			continue;
		}

		if ((flags & CAMEL_GW_MESSAGE_NOJUNK) && !strcmp(camel_folder_get_name(folder), JUNK_FOLDER)) {
			/*message was marked as junk, now unjunk*/
			move_to_mailbox (folder, info, error);
			camel_folder_summary_remove_uid (folder->summary, camel_message_info_uid(info));
			camel_data_cache_remove (gw_folder->cache, "cache", camel_message_info_uid(info), NULL);
			continue;
		}

		if (gw_info && (gw_info->info.flags & CAMEL_MESSAGE_FOLDER_FLAGGED)) {
			do_flags_diff (&diff, gw_info->server_flags, gw_info->info.flags);
			do_flags_diff (&unset_flags, flags, gw_info->server_flags);

			diff.changed &= folder->permanent_flags;

			/* weed out flag changes that we can't sync to the server */
			if (!diff.changed) {
				camel_message_info_free(info);
				continue;
			} else {
				const gchar *uid;

				uid = camel_message_info_uid (info);
				if (diff.bits & CAMEL_MESSAGE_DELETED) {

					/* In case a new message is READ and then deleted immediately */
					if (diff.bits & CAMEL_MESSAGE_SEEN)
						deleted_read_items = g_list_prepend (deleted_read_items, (gchar *) uid);

					if (deleted_items) {
						deleted_items = g_list_prepend (deleted_items, (gchar *)camel_message_info_uid (info));
					} else {
						g_list_free (deleted_head);
						deleted_head = NULL;
						deleted_head = deleted_items = g_list_prepend (deleted_items, (gchar *)camel_message_info_uid (info));
					}

					if (g_list_length (deleted_items) == GROUPWISE_BULK_DELETE_LIMIT ) {
						camel_service_lock (CAMEL_SERVICE (gw_store), CAMEL_SERVICE_REC_CONNECT_LOCK);

						/*
							Sync up the READ changes before deleting the message.
							Note that if a message is marked as unread and then deleted,
							Evo doesnt not take care of it, as I find that scenario to be impractical.
						*/

						if (deleted_read_items) {

							/* FIXME: As in many places, we need to handle the return value
							and do some error handling. But, we do not have all error codes also
							and errors are not returned always either */

							status = e_gw_connection_mark_read (cnc, deleted_read_items);
							if (status == E_GW_CONNECTION_STATUS_INVALID_CONNECTION)
								status = e_gw_connection_mark_read (cnc, deleted_read_items);
							g_list_free (deleted_read_items);
							deleted_read_items = NULL;
						}

						/* And now delete the messages */
						status = e_gw_connection_remove_items (cnc, container_id, deleted_items);
						if (status == E_GW_CONNECTION_STATUS_INVALID_CONNECTION)
							status = e_gw_connection_remove_items (cnc, container_id, deleted_items);
						camel_service_unlock (CAMEL_SERVICE (gw_store), CAMEL_SERVICE_REC_CONNECT_LOCK);
						if (status == E_GW_CONNECTION_STATUS_OK) {
							gchar *uid;
							while (deleted_items) {
								uid = (gchar *)deleted_items->data;
								CAMEL_GROUPWISE_FOLDER_REC_LOCK (folder, cache_lock);
								camel_folder_summary_remove_uid (folder->summary, uid);
								camel_data_cache_remove(gw_folder->cache, "cache", uid, NULL);
								camel_folder_change_info_remove_uid (changes, uid);
								CAMEL_GROUPWISE_FOLDER_REC_UNLOCK (folder, cache_lock);
								deleted_items = g_list_next (deleted_items);
								count -= GROUPWISE_BULK_DELETE_LIMIT;
								i -= GROUPWISE_BULK_DELETE_LIMIT;
							}
						}
					}
				} else if (diff.bits & CAMEL_MESSAGE_SEEN) {
					read_items = g_list_prepend (read_items, (gchar *)uid);
				} else if (unset_flags.bits & CAMEL_MESSAGE_SEEN) {
					unread_items = g_list_prepend (unread_items, (gchar *)uid);
				}
			}
		}

		camel_message_info_free (info);
	}

	CAMEL_GROUPWISE_FOLDER_REC_UNLOCK (folder, cache_lock);

	/* Do things in bulk. Reduces server calls, network latency etc.  */
	if (deleted_read_items)
		read_items = g_list_concat (read_items, deleted_read_items);

	if (read_items) {
		camel_service_lock (CAMEL_SERVICE (gw_store), CAMEL_SERVICE_REC_CONNECT_LOCK);
		status = e_gw_connection_mark_read (cnc, read_items);
		if (status == E_GW_CONNECTION_STATUS_INVALID_CONNECTION)
			status = e_gw_connection_mark_read (cnc, read_items);

		if (status == E_GW_CONNECTION_STATUS_OK)
			sync_flags (folder, read_items);

		g_list_free (read_items);
		camel_service_unlock (CAMEL_SERVICE (gw_store), CAMEL_SERVICE_REC_CONNECT_LOCK);
	}

	if (deleted_items) {
		camel_service_lock (CAMEL_SERVICE (gw_store), CAMEL_SERVICE_REC_CONNECT_LOCK);
		if (!strcmp (full_name, "Trash")) {
			status = e_gw_connection_purge_selected_items (cnc, deleted_items);
			if (status == E_GW_CONNECTION_STATUS_INVALID_CONNECTION)
				status = e_gw_connection_purge_selected_items (cnc, deleted_items);
		} else {
			status = e_gw_connection_remove_items (cnc, container_id, deleted_items);
			if (status == E_GW_CONNECTION_STATUS_INVALID_CONNECTION)
				status = e_gw_connection_remove_items (cnc, container_id, deleted_items);
		}
		if (status == E_GW_CONNECTION_STATUS_OK) {
			gchar *uid;
			while (deleted_items) {
				uid = (gchar *)deleted_items->data;
				CAMEL_GROUPWISE_FOLDER_REC_LOCK (folder, cache_lock);
				camel_folder_summary_remove_uid (folder->summary, uid);
				camel_data_cache_remove(gw_folder->cache, "cache", uid, NULL);
				camel_folder_change_info_remove_uid (changes, uid);
				CAMEL_GROUPWISE_FOLDER_REC_UNLOCK (folder, cache_lock);
				deleted_items = g_list_next (deleted_items);
			}
		}
		g_list_free (deleted_items);
		camel_service_unlock (CAMEL_SERVICE (gw_store), CAMEL_SERVICE_REC_CONNECT_LOCK);
	}

	if (unread_items) {
		camel_service_lock (CAMEL_SERVICE (gw_store), CAMEL_SERVICE_REC_CONNECT_LOCK);
		status = e_gw_connection_mark_unread (cnc, unread_items);
		if (status == E_GW_CONNECTION_STATUS_INVALID_CONNECTION)
			status = e_gw_connection_mark_unread (cnc, unread_items);

		if (status == E_GW_CONNECTION_STATUS_OK)
			sync_flags (folder, unread_items);

		g_list_free (unread_items);
		camel_service_unlock (CAMEL_SERVICE (gw_store), CAMEL_SERVICE_REC_CONNECT_LOCK);
	}

	if (expunge) {
		camel_service_lock (CAMEL_SERVICE (gw_store), CAMEL_SERVICE_REC_CONNECT_LOCK);
		status = e_gw_connection_purge_deleted_items (cnc);
		if (status == E_GW_CONNECTION_STATUS_OK) {
			g_message ("Purged deleted items in %s", camel_folder_get_name (folder));
		}
		camel_service_unlock (CAMEL_SERVICE (gw_store), CAMEL_SERVICE_REC_CONNECT_LOCK);
	}

	camel_service_lock (CAMEL_SERVICE (gw_store), CAMEL_SERVICE_REC_CONNECT_LOCK);
	success = groupwise_sync_summary (folder, error);
	camel_service_unlock (CAMEL_SERVICE (gw_store), CAMEL_SERVICE_REC_CONNECT_LOCK);

	camel_folder_changed (folder, changes);
	camel_folder_change_info_free (changes);

	return success;
}

CamelFolder *
camel_gw_folder_new(CamelStore *store, const gchar *folder_name, const gchar *folder_dir, GError **error)
{
	CamelFolder *folder;
	CamelGroupwiseFolder *gw_folder;
	gchar *summary_file, *state_file, *journal_file;
	gchar *short_name;

	short_name = strrchr (folder_name, '/');
	if (short_name)
		short_name++;
	else
		short_name = (gchar *) folder_name;

	folder = g_object_new (
		CAMEL_TYPE_GROUPWISE_FOLDER,
		"name", short_name, "full-name", folder_name,
		"parent_store", store, NULL);

	gw_folder = CAMEL_GROUPWISE_FOLDER(folder);

	summary_file = g_strdup_printf ("%s/summary",folder_dir);
	folder->summary = camel_groupwise_summary_new(folder, summary_file);
	g_free(summary_file);
	if (!folder->summary) {
		g_object_unref (CAMEL_OBJECT (folder));
		g_set_error (
			error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			_("Could not load summary for %s"), folder_name);
		return NULL;
	}

	/* set/load persistent state */
	state_file = g_strdup_printf ("%s/cmeta", folder_dir);
	camel_object_set_state_filename (CAMEL_OBJECT (folder), state_file);
	g_free(state_file);
	camel_object_state_read (CAMEL_OBJECT (folder));

	gw_folder->cache = camel_data_cache_new (folder_dir, error);
	if (!gw_folder->cache) {
		g_object_unref (folder);
		return NULL;
	}

	journal_file = g_strdup_printf ("%s/journal",folder_dir);
	gw_folder->journal = camel_groupwise_journal_new (gw_folder, journal_file);
	g_free (journal_file);
	if (!gw_folder->journal) {
		g_object_unref (folder);
		return NULL;
	}

	if (!strcmp (folder_name, "Mailbox")) {
		if (camel_url_get_param (((CamelService *) store)->url, "filter"))
			folder->folder_flags |= CAMEL_FOLDER_FILTER_RECENT;
	}

	gw_folder->search = camel_folder_search_new ();
	if (!gw_folder->search) {
		g_object_unref (folder);
		return NULL;
	}

	return folder;
}

struct _folder_update_msg {
	CamelSessionThreadMsg msg;

	EGwConnection *cnc;
	CamelFolder *folder;
	gchar *container_id;
	gchar *t_str;
	GSList *slist;
};

static void
update_update (CamelSession *session, CamelSessionThreadMsg *msg)
{
	struct _folder_update_msg *m = (struct _folder_update_msg *)msg;
	EGwConnectionStatus status;
	CamelGroupwiseStore *gw_store;
	CamelStore *parent_store;
	GList *item_list, *items_full_list = NULL, *last_element=NULL;
	gint cursor = 0;
	const gchar *position = E_GW_CURSOR_POSITION_END;
	gboolean done;

	parent_store = camel_folder_get_parent_store (m->folder);
	gw_store = CAMEL_GROUPWISE_STORE (parent_store);

	/* Hold the connect_lock.
	   In case if user went offline, don't do anything.
	   m->cnc would have become invalid, as the store disconnect unrefs it.
	 */
	camel_service_lock (CAMEL_SERVICE (gw_store), CAMEL_SERVICE_REC_CONNECT_LOCK);
	if (((CamelOfflineStore *) gw_store)->state == CAMEL_OFFLINE_STORE_NETWORK_UNAVAIL ||
			((CamelService *)gw_store)->status == CAMEL_SERVICE_DISCONNECTED) {
		goto end1;
	}

	camel_operation_start (
		NULL, _("Checking for deleted messages %s"),
		camel_folder_get_name (m->folder));

	status = e_gw_connection_create_cursor (m->cnc, m->container_id, "id", NULL, &cursor);
	if (status == E_GW_CONNECTION_STATUS_INVALID_CONNECTION)
		status = e_gw_connection_create_cursor (m->cnc, m->container_id, "id", NULL, &cursor);
	if (status != E_GW_CONNECTION_STATUS_OK) {
		g_warning ("ERROR update update\n");
		goto end1;
	}

	done = FALSE;
	m->slist = NULL;

	while (!done) {

		if (camel_application_is_exiting) {
				camel_service_unlock (CAMEL_SERVICE (gw_store), CAMEL_SERVICE_REC_CONNECT_LOCK);
				return;
		}

		item_list = NULL;
		status = e_gw_connection_get_all_mail_uids (m->cnc, m->container_id, cursor, FALSE, READ_CURSOR_MAX_IDS, position, &item_list);
		if (status != E_GW_CONNECTION_STATUS_OK) {
			g_warning ("ERROR update update\n");
			e_gw_connection_destroy_cursor (m->cnc, m->container_id, cursor);
			goto end1;
		}

		if (!item_list)
			done = TRUE;
		else {

			/* item_list is prepended to items_full_list and not the other way
			   because when we have a large number of items say 50000,
			   for each iteration there will be more elements in items_full_list
			   and less elements in item_list */

			last_element = g_list_last (item_list);
			if (items_full_list) {
				last_element->next = items_full_list;
				items_full_list->prev = last_element;
			}
			items_full_list = item_list;
		}
		position = E_GW_CURSOR_POSITION_CURRENT;
	}
	e_gw_connection_destroy_cursor (m->cnc, m->container_id, cursor);

	camel_service_unlock (CAMEL_SERVICE (gw_store), CAMEL_SERVICE_REC_CONNECT_LOCK);
	/* Take out only the first part in the list until the @ since it is guaranteed
	   to be unique only until that symbol */

	/*if (items_full_list) {
	  gint i;
	  item_list = items_full_list;

	  while (item_list->next) {
	  i = 0;
	  while (((const gchar *)item_list->data)[i++]!='@');
	  ((gchar *)item_list->data)[i-1] = '\0';
	  item_list = item_list->next;
	  }

	  i = 0;
	  while (((const gchar *)item_list->data)[i++]!='@');
	  ((gchar *)item_list->data)[i-1] = '\0';
	  }*/

	g_print ("\nNumber of items in the folder: %d \n", g_list_length(items_full_list));
	gw_update_all_items (m->folder, items_full_list, NULL);
	camel_operation_end (NULL);

	return;
 end1:
	camel_service_unlock (CAMEL_SERVICE (gw_store), CAMEL_SERVICE_REC_CONNECT_LOCK);
	camel_operation_end (NULL);
	if (items_full_list) {
		g_list_foreach (items_full_list, (GFunc)g_free, NULL);
		g_list_free (items_full_list);
	}
	return;
}

static void
update_free (CamelSession *session, CamelSessionThreadMsg *msg)
{
	struct _folder_update_msg *m = (struct _folder_update_msg *)msg;

	g_free (m->t_str);
	g_free (m->container_id);
	g_object_unref (m->folder);
	g_slist_foreach (m->slist, (GFunc) g_free, NULL);
	g_slist_free (m->slist);
	m->slist = NULL;
}

static CamelSessionThreadOps update_ops = {
	update_update,
	update_free,
};

static gboolean
groupwise_refresh_info(CamelFolder *folder, GError **error)
{
	CamelGroupwiseSummary *summary = (CamelGroupwiseSummary *) folder->summary;
	CamelStoreInfo *si;
	CamelGroupwiseStore *gw_store;
	CamelStore *parent_store;
	const gchar *full_name;

	full_name = camel_folder_get_full_name (folder);
	parent_store = camel_folder_get_parent_store (folder);

	gw_store = CAMEL_GROUPWISE_STORE (parent_store);

	/*
	 * Checking for the summary->time_string here since the first the a
	 * user views a folder, the read cursor is in progress, and the getQM
	 * should not interfere with the process
	 */
	if (summary->time_string && (strlen (summary->time_string) > 0))  {
		groupwise_refresh_folder(folder, error);
		si = camel_store_summary_path ((CamelStoreSummary *)((CamelGroupwiseStore *)parent_store)->summary, full_name);
		if (si) {
			guint32 unread, total;

			total = camel_folder_summary_count (folder->summary);
			unread = folder->summary->unread_count;

			if (si->total != total || si->unread != unread) {
				si->total = total;
				si->unread = unread;
				camel_store_summary_touch ((CamelStoreSummary *)((CamelGroupwiseStore *)parent_store)->summary);
			}
			camel_store_summary_info_free ((CamelStoreSummary *)((CamelGroupwiseStore *)parent_store)->summary, si);
		}
		/* camel_folder_summary_save_to_db (folder->summary, ex); */
		camel_store_summary_save ((CamelStoreSummary *)((CamelGroupwiseStore *)parent_store)->summary);
	} else {
		/* We probably could not get the messages the first time. (get_folder) failed???!
		 * so do a get_folder again. And hope that it works
		 */
		g_print("Reloading folder...something wrong with the summary....\n");
		gw_store_reload_folder (gw_store, folder, 0, error);
	}

	return TRUE;
}

static gint
check_for_new_mails_count (CamelGroupwiseSummary *gw_summary, GSList *ids)
{
	CamelFolderSummary *summary = (CamelFolderSummary *) gw_summary;
	GSList *l = NULL;
	gint count = 0;

	for (l = ids; l != NULL; l = g_slist_next (l)) {
		EGwItem *item = l->data;
		const gchar *id = e_gw_item_get_id (item);
		CamelMessageInfo *info	= camel_folder_summary_uid (summary, id);

		if (!info)
			count++;
		else
			camel_message_info_free (info);
	}

	return count;
}

static gint
compare_ids (gpointer a, gpointer b, gpointer data)
{
	EGwItem *item1 = (EGwItem *) a;
	EGwItem *item2 = (EGwItem *) b;
	const gchar *id1 = NULL, *id2 = NULL;

	id1 = e_gw_item_get_id (item1);
	id2 = e_gw_item_get_id (item2);

	return strcmp (id1, id2);
}

static gint
get_merge_lists_new_count (CamelGroupwiseSummary *summary, GSList *new, GSList *modified, GSList **merged)
{
	GSList *l, *element;
	gint count = 0;

	if (new == NULL && modified == NULL) {
		*merged = NULL;
		return 0;
	} if (new == NULL) {
		*merged = modified;

		return check_for_new_mails_count (summary, modified);
	} else if (modified == NULL) {
		*merged = new;

		return check_for_new_mails_count (summary, new);
	}

	/* now merge both the lists */
	for (l = new; l != NULL; l = g_slist_next (l)) {
		element = g_slist_find_custom (modified, l->data, (GCompareFunc) compare_ids);
		if (element != NULL) {
			g_object_unref (element->data);
			element->data = NULL;
			modified = g_slist_delete_link (modified, element);
		}
	}

	/* There might some new items which come through modified also */
	*merged = g_slist_concat (new, modified);
	count = check_for_new_mails_count (summary, *merged);

	return count;
}

static void
update_summary_string (CamelFolder *folder, const gchar *time_string)
{
	CamelGroupwiseSummary *summary = (CamelGroupwiseSummary *) folder->summary;

	if (summary->time_string)
		g_free (summary->time_string);

	((CamelGroupwiseSummary *) folder->summary)->time_string = g_strdup (time_string);
	camel_folder_summary_touch (folder->summary);
	camel_folder_summary_save_to_db (folder->summary, NULL);
}

static void
groupwise_refresh_folder(CamelFolder *folder, GError **error)
{
	CamelGroupwiseStore *gw_store;
	CamelGroupwiseFolder *gw_folder;
	CamelGroupwiseSummary *summary = (CamelGroupwiseSummary *)folder->summary;
	EGwConnection *cnc;
	CamelSession *session;
	CamelStore *parent_store;
	gboolean is_proxy;
	gboolean is_locked = TRUE;
	gint status;
	GList *list = NULL;
	GSList *new_items = NULL, *modified_items = NULL, *merged = NULL;
	const gchar *full_name;
	gchar *container_id = NULL;
	gchar *old_sync_time = NULL, *new_sync_time = NULL, *modified_sync_time = NULL;
	struct _folder_update_msg *msg;
	gboolean sync_deleted = FALSE;
	EGwContainer *container;
	gint new_item_count = 0;

	full_name = camel_folder_get_full_name (folder);
	parent_store = camel_folder_get_parent_store (folder);

	session = CAMEL_SERVICE (parent_store)->session;
	is_proxy = (parent_store->flags & CAMEL_STORE_PROXY);

	gw_folder = CAMEL_GROUPWISE_FOLDER (folder);
	gw_store = CAMEL_GROUPWISE_STORE (parent_store);

	cnc = cnc_lookup (gw_store->priv);

	/* Sync-up the (un)read changes before getting updates,
	so that the getFolderList will reflect the most recent changes too */
	groupwise_sync_all (folder, FALSE, error);

	if (((CamelOfflineStore *) gw_store)->state == CAMEL_OFFLINE_STORE_NETWORK_UNAVAIL) {
		g_warning ("In offline mode. Cannot refresh!!!\n");
		return;
	}

	container_id = g_strdup (camel_groupwise_store_container_id_lookup (gw_store, full_name));

	if (!container_id) {
		d (printf ("\nERROR - Container id not present. Cannot refresh info for %s\n", full_name));
		return;
	}

	if (!cnc)
		return;

	if (camel_folder_is_frozen (folder) ) {
		gw_folder->need_refresh = TRUE;
	}

	camel_service_lock (CAMEL_SERVICE (gw_store), CAMEL_SERVICE_REC_CONNECT_LOCK);

	if (!camel_groupwise_store_connected (gw_store, error))
		goto end1;

	if (!strcmp (full_name, "Trash")) {
		is_proxy = TRUE;
	}

	/*Get the New Items*/
	if (!is_proxy) {
		const gchar *source;

		if (!strcmp (full_name, RECEIVED) || !strcmp(full_name, SENT)) {
			source = NULL;
		} else {
			source = "sent received";
		}

		old_sync_time = g_strdup (((CamelGroupwiseSummary *) folder->summary)->time_string);
		new_sync_time = g_strdup (old_sync_time);

		status = e_gw_connection_get_quick_messages (cnc, container_id,
				"peek id",
				&new_sync_time, "New", NULL, source, -1, &new_items);
		if (status == E_GW_CONNECTION_STATUS_INVALID_CONNECTION)
			status = e_gw_connection_get_quick_messages (cnc, container_id,
					"peek id",
					&new_sync_time, "New", NULL, source, -1, &new_items);
		if (status != E_GW_CONNECTION_STATUS_OK) {
			g_set_error (
				error, CAMEL_SERVICE_ERROR,
				CAMEL_SERVICE_ERROR_INVALID,
				_("Authentication failed"));
			goto end2;
		}

		modified_sync_time = g_strdup (old_sync_time);

		/*Get those items which have been modifed*/
		status = e_gw_connection_get_quick_messages (cnc, container_id,
				"peek id",
				&modified_sync_time, "Modified", NULL, source, -1, &modified_items);
		if (status == E_GW_CONNECTION_STATUS_INVALID_CONNECTION)
			status = e_gw_connection_get_quick_messages (cnc, container_id,
					"peek id",
					&modified_sync_time, "Modified", NULL, source, -1, &modified_items);
		if (status != E_GW_CONNECTION_STATUS_OK) {
			g_set_error (
				error, CAMEL_SERVICE_ERROR,
				CAMEL_SERVICE_ERROR_INVALID,
				_("Authentication failed"));
			goto end3;
		}

		if (gw_store->current_folder != folder)
			groupwise_store_set_current_folder (gw_store, folder);

		new_item_count = get_merge_lists_new_count (summary, new_items, modified_items, &merged);

		/* FIXME need to cleanup the code which uses GList. Ideally GSList would just suffice. */
		if (merged != NULL) {
			GSList *sl = NULL;

			for (sl = merged; sl != NULL; sl = g_slist_next (sl))
				list = g_list_prepend (list, sl->data);
		}
		g_slist_free (merged);

		container = e_gw_connection_get_container (cnc, container_id);
		if (container) {
			/* HACK: Refer to Novell bugzilla bug #464379 */
			if ((camel_folder_summary_count (folder->summary) + new_item_count) == e_gw_container_get_total_count (container))
				sync_deleted = FALSE;
			else
				sync_deleted = TRUE;

		} else
			sync_deleted = FALSE;

		g_object_unref (container);

		if (list)
			gw_update_cache (folder, list, error, FALSE);

		/* update the new_sync_time to summary */
		update_summary_string (folder, new_sync_time);
	}

	camel_service_unlock (CAMEL_SERVICE (gw_store), CAMEL_SERVICE_REC_CONNECT_LOCK);
	is_locked = FALSE;

	/*
	 * The New and Modified items in the server have been updated in the summary.
	 * Now we have to make sure that all the deleted items in the server are deleted
	 * from Evolution as well. So we get the id's of all the items on the sever in
	 * this folder, and update the summary.
	 */
	/*create a new session thread for the update all operation*/
	if (sync_deleted || is_proxy) {
		msg = camel_session_thread_msg_new (session, &update_ops, sizeof(*msg));
		msg->cnc = cnc;
		msg->t_str = g_strdup (old_sync_time);
		msg->container_id = g_strdup (container_id);
		msg->folder = g_object_ref (folder);
		camel_session_thread_queue (session, &msg->msg, 0);
		/*thread creation and queueing done*/
	}

end3:
	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);
	list = NULL;
end2:
	g_free (old_sync_time);
	g_free (new_sync_time);
	g_free (modified_sync_time);
	g_free (container_id);
end1:
	if (is_locked)
		camel_service_unlock (CAMEL_SERVICE (gw_store), CAMEL_SERVICE_REC_CONNECT_LOCK);
	return;
}

static guint8*
get_md5_digest (const guchar *str)
{
	guint8 *digest;
	gsize length;
	GChecksum *checksum;

	length = g_checksum_type_get_length (G_CHECKSUM_MD5);
	digest = g_alloca (length);

	checksum = g_checksum_new (G_CHECKSUM_MD5);
	g_checksum_update (checksum, str, -1);
	g_checksum_get_digest (checksum, digest, &length);
	g_checksum_free (checksum);

	return digest;
}

static void
groupwise_folder_set_threading_data (CamelGroupwiseMessageInfo *mi, EGwItem *item)
{
	const gchar *parent_threads;
	gint count = 0;
	const gchar *message_id = e_gw_item_get_message_id (item);
	struct _camel_header_references *refs, *scan;
	guint8 *digest;
	gchar *msgid;

	if (!message_id)
		return;

	/* set message id */
	msgid = camel_header_msgid_decode(message_id);
	digest = get_md5_digest ((const guchar *)msgid);
	memcpy(mi->info.message_id.id.hash, digest, sizeof(mi->info.message_id.id.hash));
	g_free (msgid);

	parent_threads = e_gw_item_get_parent_thread_ids (item);

	if (!parent_threads)
		return;

	refs = camel_header_references_decode (parent_threads);
	count = camel_header_references_list_size(&refs);
	mi->info.references = g_malloc(sizeof(*mi->info.references) + ((count-1) * sizeof(mi->info.references->references[0])));
	scan = refs;
	count = 0;

	while (scan) {
		digest = get_md5_digest ((const guchar *) scan->id);
		memcpy(mi->info.references->references[count].id.hash, digest, sizeof(mi->info.message_id.id.hash));

		count++;
		scan = scan->next;
	}

	mi->info.references->size = count;
	camel_header_references_list_clear(&refs);
}

/* Update the GroupWise cache with the list of items passed. should happen in thread. */
static void
gw_update_cache (CamelFolder *folder, GList *list, GError **error, gboolean uid_flag)
{
	CamelGroupwiseMessageInfo *mi = NULL;
	CamelMessageInfo *pmi = NULL;
	CamelGroupwiseStore *gw_store;
	CamelGroupwiseFolder *gw_folder;
	CamelOfflineFolder *offline_folder;
	CamelStore *parent_store;
	EGwConnection *cnc;
	guint32 item_status, status_flags = 0;
	CamelFolderChangeInfo *changes = NULL;
	gboolean exists = FALSE;
	GString *str_to = g_string_new (NULL);
	GString *str_cc = g_string_new (NULL);
	const gchar *priority = NULL;
	gchar *container_id = NULL;
	gboolean is_junk = FALSE;
	EGwConnectionStatus status;
	GList *item_list = list;
	gint total_items = g_list_length (item_list), i=0;
	gboolean is_proxy;
	const gchar *full_name;

	gboolean folder_needs_caching;

	full_name = camel_folder_get_full_name (folder);
	parent_store = camel_folder_get_parent_store (folder);

	is_proxy = (parent_store->flags & CAMEL_STORE_WRITE);

	gw_folder = CAMEL_GROUPWISE_FOLDER (folder);
	gw_store = CAMEL_GROUPWISE_STORE (parent_store);

	cnc = cnc_lookup (gw_store->priv);

	offline_folder = CAMEL_OFFLINE_FOLDER (folder);
	folder_needs_caching = camel_offline_folder_get_offline_sync (offline_folder);

	changes = camel_folder_change_info_new ();
	container_id = g_strdup (camel_groupwise_store_container_id_lookup (gw_store, full_name));
	if (!container_id) {
		d (printf("\nERROR - Container id not present. Cannot refresh info\n"));
		camel_folder_change_info_free (changes);
		return;
	}

	if (!strcmp (full_name, JUNK_FOLDER)) {
		is_junk = TRUE;
	}

	camel_operation_start (
		NULL, _("Fetching summary information for new messages in %s"),
		camel_folder_get_name (folder));

	for (; item_list != NULL; item_list = g_list_next (item_list) ) {
		EGwItem *temp_item;
		EGwItem *item;
		EGwItemType type = E_GW_ITEM_TYPE_UNKNOWN;
		EGwItemOrganizer *org;
		gchar *temp_date = NULL;
		const gchar *id;
		GSList *recp_list = NULL;
		CamelStream *cache_stream, *t_cache_stream;
		CamelMimeMessage *mail_msg = NULL;
		const gchar *recurrence_key = NULL;
		gint rk;

		exists = FALSE;
		status_flags = 0;

		if (uid_flag == FALSE) {
			temp_item = (EGwItem *)item_list->data;
			id = e_gw_item_get_id (temp_item);
		} else
			id = (gchar *) item_list->data;

		camel_operation_progress (NULL, (100*i)/total_items);

		if (folder_needs_caching)
			status = e_gw_connection_get_item (cnc, container_id, id, GET_ITEM_VIEW_WITH_CACHE, &item);
		else
			status = e_gw_connection_get_item (cnc, container_id, id, GET_ITEM_VIEW_WITHOUT_CACHE, &item);

		if (status != E_GW_CONNECTION_STATUS_OK) {
			g_message ("Could not get the item from the server, item id %s \n", id);
			i++;
			continue;
		}

		/************************ First populate summary *************************/

		item_status = e_gw_item_get_item_status (item);

		/* skip the deleted items */
		if (item_status & E_GW_ITEM_STAT_DELETED && strcmp (full_name, "Trash")) {
			i++;
			continue;
		}

		mi = NULL;
		pmi = NULL;
		pmi = camel_folder_summary_uid (folder->summary, id);
		if (pmi) {
			exists = TRUE;
			camel_message_info_ref (pmi);
			mi = (CamelGroupwiseMessageInfo *)pmi;
		}

		type = e_gw_item_get_item_type (item);
		if (!exists) {
			if ((type == E_GW_ITEM_TYPE_CONTACT) || (type == E_GW_ITEM_TYPE_UNKNOWN)) {
				exists = FALSE;
				continue;
			}

			mi = (CamelGroupwiseMessageInfo *)camel_message_info_new (folder->summary);
			if (mi->info.content == NULL) {
				mi->info.content = camel_folder_summary_content_info_new (folder->summary);
				mi->info.content->type = camel_content_type_new ("multipart", "mixed");
			}

			mi->info.flags = 0;

			if (type == E_GW_ITEM_TYPE_APPOINTMENT || type == E_GW_ITEM_TYPE_TASK || type == E_GW_ITEM_TYPE_NOTE)
				camel_message_info_set_user_flag ((CamelMessageInfo*)mi, "$has_cal", TRUE);
		}

		rk = e_gw_item_get_recurrence_key (item);
		if (rk > 0) {
			recurrence_key = g_strdup_printf("%d", rk);
			camel_message_info_set_user_tag ((CamelMessageInfo*)mi, "recurrence-key", recurrence_key);
		}

		/*all items in the Junk Mail folder should have this flag set*/
		if (is_junk)
			mi->info.flags |= CAMEL_GW_MESSAGE_JUNK;

		if (item_status & E_GW_ITEM_STAT_READ)
			mi->info.flags |= CAMEL_MESSAGE_SEEN;
		else
			mi->info.flags &= ~CAMEL_MESSAGE_SEEN;

		if (item_status & E_GW_ITEM_STAT_REPLIED)
			mi->info.flags |= CAMEL_MESSAGE_ANSWERED;

		priority = e_gw_item_get_priority (item);
		if (priority && !(g_ascii_strcasecmp (priority,"High"))) {
			mi->info.flags |= CAMEL_MESSAGE_FLAGGED;
		}

		if (e_gw_item_has_attachment (item))
			mi->info.flags |= CAMEL_MESSAGE_ATTACHMENTS;
                if (is_proxy)
                        mi->info.flags |= CAMEL_MESSAGE_USER_NOT_DELETABLE;

		mi->server_flags = mi->info.flags;

		org = e_gw_item_get_organizer (item);
		mi->info.from = get_from_from_org (org);

		g_string_truncate (str_to, 0);
		g_string_truncate (str_cc, 0);
		recp_list = e_gw_item_get_recipient_list (item);
		if (recp_list) {
			GSList *rl;
			gint i_to = 0, i_cc = 0;
			for (rl = recp_list; rl != NULL; rl = rl->next) {
				EGwItemRecipient *recp = (EGwItemRecipient *) rl->data;
				if (recp->type == E_GW_ITEM_RECIPIENT_TO) {
					if (i_to)
						str_to = g_string_append (str_to, ", ");
					g_string_append_printf (str_to,"%s <%s>", recp->display_name, recp->email);
					i_to++;
				} else if (recp->type == E_GW_ITEM_RECIPIENT_CC) {
					if (i_cc)
						str_cc = g_string_append (str_cc, ", ");
					g_string_append_printf (str_cc,"%s <%s>", recp->display_name, recp->email);
					i_cc++;
				}
			}
			if (exists)
				camel_pstring_free(mi->info.to);
			mi->info.to = camel_pstring_strdup (str_to->str);
			mi->info.cc = camel_pstring_strdup (str_cc->str);

			g_string_truncate (str_to, 0);
			g_string_truncate (str_cc, 0);
		}

		if (type == E_GW_ITEM_TYPE_APPOINTMENT
				|| type ==  E_GW_ITEM_TYPE_NOTE
				|| type ==  E_GW_ITEM_TYPE_TASK ) {
			temp_date = e_gw_item_get_start_date (item);
			if (temp_date) {
				time_t actual_time = e_gw_connection_get_date_from_string (temp_date);
				mi->info.date_sent = mi->info.date_received = actual_time;
			}
		} else
			groupwise_set_mail_mi_dates (mi, item);

		mi->info.dirty = TRUE;
		if (exists) {
			camel_folder_change_info_change_uid (changes, mi->info.uid);
			camel_message_info_free (pmi);
		} else {
			mi->info.uid = camel_pstring_strdup (e_gw_item_get_id(item));
			mi->info.size = e_gw_item_get_mail_size (item);
			mi->info.subject = camel_pstring_strdup(e_gw_item_get_subject(item));
			groupwise_folder_set_threading_data (mi, item);

			camel_folder_summary_add (folder->summary,(CamelMessageInfo *)mi);
			camel_folder_change_info_add_uid (changes, mi->info.uid);
			camel_folder_change_info_recent_uid (changes, mi->info.uid);
		}

		/********************* Summary ends *************************/
		if (!strcmp (full_name, "Junk Mail"))
			continue;

		if (folder_needs_caching) {
				/******************** Begine Caching ************************/
				/* add to cache if its a new message*/
				t_cache_stream  = camel_data_cache_get (gw_folder->cache, "cache", id, error);
				if (t_cache_stream) {
						g_object_unref (t_cache_stream);

						mail_msg = groupwise_folder_item_to_msg (folder, item, error);
						if (mail_msg)
								camel_medium_set_header (CAMEL_MEDIUM (mail_msg), "X-Evolution-Source", groupwise_base_url_lookup (gw_store->priv));

						CAMEL_GROUPWISE_FOLDER_REC_LOCK (folder, cache_lock);
						if ((cache_stream = camel_data_cache_add (gw_folder->cache, "cache", id, NULL))) {
								if (camel_data_wrapper_write_to_stream ((CamelDataWrapper *) mail_msg,	cache_stream, NULL) == -1 || camel_stream_flush (cache_stream, NULL) == -1)
										camel_data_cache_remove (gw_folder->cache, "cache", id, NULL);
								g_object_unref (cache_stream);
						}

						g_object_unref (mail_msg);
						CAMEL_GROUPWISE_FOLDER_REC_UNLOCK (folder, cache_lock);
				}
				/******************** Caching stuff ends *************************/
		}

		i++;
		g_object_unref (item);
	}
	camel_operation_end (NULL);
	g_free (container_id);
	g_string_free (str_to, TRUE);
	g_string_free (str_cc, TRUE);
	groupwise_sync_summary (folder, error);

	camel_folder_changed (folder, changes);
	camel_folder_change_info_free (changes);
}

static const gchar *
get_from_from_org (EGwItemOrganizer *org)
{
	const gchar *ret = NULL;

	if (org) {
		GString *str;

		str = g_string_new ("");
		if (org->display_name && org->display_name[0]) {
			org->display_name = g_strdelimit (org->display_name, "<>", ' ');
			str = g_string_append (str, org->display_name);
			str = g_string_append (str, " ");
		} else if (org->email && org->email[0]) {
			str = g_string_append (str, org->email);
			str = g_string_append (str, " ");
		}

		if (org->email && org->email[0]) {
			g_string_append (str, "<");
			str = g_string_append (str, org->email);
			g_string_append (str, ">");
		}
		ret = camel_pstring_strdup (str->str);
		g_string_free (str, TRUE);

		return ret;
	} else
	       return camel_pstring_strdup ("");
}

/* Update summary, if there is none existing, create one */
void
gw_update_summary (CamelFolder *folder, GList *list,GError **error)
{
	CamelGroupwiseMessageInfo *mi = NULL;
	CamelGroupwiseStore *gw_store;
	guint32 item_status, status_flags = 0;
	CamelFolderChangeInfo *changes = NULL;
	CamelStore *parent_store;
	gboolean exists = FALSE;
	GString *str_to = g_string_new (NULL);
	GString *str_cc = g_string_new (NULL);
	const gchar *priority = NULL;
	gchar *container_id = NULL;
	gboolean is_junk = FALSE;
	GList *item_list = list;
	const gchar *full_name;
	gboolean is_proxy;

	full_name = camel_folder_get_full_name (folder);
	parent_store = camel_folder_get_parent_store (folder);

	is_proxy = (parent_store->flags & CAMEL_STORE_WRITE);

	gw_store = CAMEL_GROUPWISE_STORE (parent_store);

	/*Assert lock???*/
	changes = camel_folder_change_info_new ();
	container_id = g_strdup (camel_groupwise_store_container_id_lookup (gw_store, full_name));
	if (!container_id) {
		d (printf("\nERROR - Container id not present. Cannot refresh info\n"));
		camel_folder_change_info_free (changes);
		return;
	}

	if (!strcmp (full_name, JUNK_FOLDER)) {
		is_junk = TRUE;
	}

	for (; item_list != NULL; item_list = g_list_next (item_list) ) {
		EGwItem *item = (EGwItem *)item_list->data;
		EGwItemType type = E_GW_ITEM_TYPE_UNKNOWN;
		EGwItemOrganizer *org;
		gchar *temp_date = NULL;
		const gchar *id;
		GSList *recp_list = NULL;
		const gchar *recurrence_key = NULL;
		gint rk;

		status_flags = 0;
		id = e_gw_item_get_id (item);

		mi = (CamelGroupwiseMessageInfo *)camel_folder_summary_uid (folder->summary, id);
		if (mi)
			exists = TRUE;

		type = e_gw_item_get_item_type (item);
		if (!exists) {
			if ((type == E_GW_ITEM_TYPE_CONTACT) || (type == E_GW_ITEM_TYPE_UNKNOWN)) {
				exists = FALSE;
				continue;
			}

			mi = camel_message_info_new (folder->summary);
			if (mi->info.content == NULL) {
				mi->info.content = camel_folder_summary_content_info_new (folder->summary);
				mi->info.content->type = camel_content_type_new ("multipart", "mixed");
			}

			if (type == E_GW_ITEM_TYPE_APPOINTMENT || type == E_GW_ITEM_TYPE_TASK || type == E_GW_ITEM_TYPE_NOTE)
				camel_message_info_set_user_flag ((CamelMessageInfo*)mi, "$has_cal", TRUE);
		}

		rk = e_gw_item_get_recurrence_key (item);
		if (rk > 0) {
			recurrence_key = g_strdup_printf("%d", rk);
			camel_message_info_set_user_tag ((CamelMessageInfo*)mi, "recurrence-key", recurrence_key);
		}

		/*all items in the Junk Mail folder should have this flag set*/
		if (is_junk)
			mi->info.flags |= CAMEL_GW_MESSAGE_JUNK;

		item_status = e_gw_item_get_item_status (item);
		if (item_status & E_GW_ITEM_STAT_READ)
			status_flags |= CAMEL_MESSAGE_SEEN;
		if (item_status & E_GW_ITEM_STAT_REPLIED)
			status_flags |= CAMEL_MESSAGE_ANSWERED;

		if (!strcmp (full_name, "Trash"))
			status_flags |= CAMEL_MESSAGE_SEEN;

		mi->info.flags |= status_flags;

		priority = e_gw_item_get_priority (item);
		if (priority && !(g_ascii_strcasecmp (priority,"High"))) {
			mi->info.flags |= CAMEL_MESSAGE_FLAGGED;
		}

		if (e_gw_item_has_attachment (item))
			mi->info.flags |= CAMEL_MESSAGE_ATTACHMENTS;

		if (is_proxy)
			mi->info.flags |= CAMEL_MESSAGE_USER_NOT_DELETABLE;

		mi->server_flags = mi->info.flags;

		org = e_gw_item_get_organizer (item);
		mi->info.from = get_from_from_org (org);

		g_string_truncate (str_to, 0);
		g_string_truncate (str_cc, 0);
		recp_list = e_gw_item_get_recipient_list (item);
		if (recp_list) {
			GSList *rl;
			gint i_to = 0, i_cc = 0;
			for (rl = recp_list; rl != NULL; rl = rl->next) {
				EGwItemRecipient *recp = (EGwItemRecipient *) rl->data;
				if (recp->type == E_GW_ITEM_RECIPIENT_TO) {
					if (i_to)
						str_to = g_string_append (str_to, ", ");
					g_string_append_printf (str_to,"%s <%s>", recp->display_name, recp->email);
					i_to++;
				} else if (recp->type == E_GW_ITEM_RECIPIENT_CC) {
					if (i_cc)
						str_cc = g_string_append (str_cc, ", ");
					g_string_append_printf (str_cc,"%s <%s>", recp->display_name, recp->email);
					i_cc++;
				}
			}
			mi->info.to = camel_pstring_strdup (str_to->str);
			mi->info.cc = camel_pstring_strdup (str_cc->str);

			g_string_truncate (str_to, 0);
			g_string_truncate (str_cc, 0);
		}

		if (type == E_GW_ITEM_TYPE_APPOINTMENT ||
		    type ==  E_GW_ITEM_TYPE_NOTE ||
		    type ==  E_GW_ITEM_TYPE_TASK ) {
			temp_date = e_gw_item_get_start_date (item);
			if (temp_date) {
				time_t actual_time = e_gw_connection_get_date_from_string (temp_date);
				mi->info.date_sent = mi->info.date_received = actual_time;
			}
		} else
			groupwise_set_mail_mi_dates (mi, item);

		mi->info.uid = camel_pstring_strdup(e_gw_item_get_id(item));
		if (!exists)
			mi->info.size = e_gw_item_get_mail_size (item);
		mi->info.subject = camel_pstring_strdup(e_gw_item_get_subject(item));
		groupwise_folder_set_threading_data (mi, item);

		if (exists) {
			camel_folder_change_info_change_uid (changes, e_gw_item_get_id (item));
			camel_message_info_free (&mi->info);
		} else {
			camel_folder_summary_add (folder->summary,(CamelMessageInfo *)mi);
			camel_folder_change_info_add_uid (changes, mi->info.uid);
			camel_folder_change_info_recent_uid (changes, mi->info.uid);
		}

		exists = FALSE;
	}
	g_free (container_id);
	g_string_free (str_to, TRUE);
	g_string_free (str_cc, TRUE);

	camel_folder_changed (folder, changes);
	camel_folder_change_info_free (changes);
}

static CamelMimeMessage *
groupwise_folder_item_to_msg( CamelFolder *folder,
		EGwItem *item,
		GError **error )
{
	CamelMimeMessage *msg = NULL;
	CamelGroupwiseStore *gw_store;
	const gchar *container_id = NULL;
	GSList *attach_list = NULL;
	EGwItemType type;
	EGwConnectionStatus status;
	EGwConnection *cnc;
	CamelMultipart *multipart = NULL;
	CamelStore *parent_store;
	gchar *body = NULL;
	gint body_len = 0;
	const gchar *uid = NULL, *message_id, *parent_threads;
	gboolean is_text_html = FALSE;
	gboolean has_mime_822 = FALSE, ignore_mime_822 = FALSE;
	gboolean is_text_html_embed = FALSE;
	gboolean is_base64_encoded = FALSE;
	CamelStream *temp_stream;
	const gchar *full_name;

	full_name = camel_folder_get_full_name (folder);
	parent_store = camel_folder_get_parent_store (folder);

	gw_store = CAMEL_GROUPWISE_STORE (parent_store);

	uid = e_gw_item_get_id(item);
	cnc = cnc_lookup (gw_store->priv);
	container_id = camel_groupwise_store_container_id_lookup (gw_store, full_name);

	/* The item is already in calendar. We need to ignore the mime 822 since it would not have the item id of the appointmnet
	   in calendar */
	if (e_gw_item_get_item_type (item) == E_GW_ITEM_TYPE_APPOINTMENT && e_gw_item_is_from_internet (item))
		ignore_mime_822 = TRUE;

	attach_list = e_gw_item_get_attach_id_list (item);
	if (attach_list) {
		/*int attach_count = g_slist_length (attach_list);*/
		GSList *al = attach_list;
		EGwItemAttachment *attach = (EGwItemAttachment *)al->data;
		gchar *attachment = NULL;
		gint len = 0;

		if (!g_ascii_strcasecmp (attach->name, "Text.htm") ||
		    !g_ascii_strcasecmp (attach->name, "Header")) {

			status = e_gw_connection_get_attachment (cnc,
					attach->id, 0, -1,
					(const gchar **)&attachment, &len);
			if (status != E_GW_CONNECTION_STATUS_OK) {
				g_warning ("Could not get attachment\n");
				g_set_error (
					error, CAMEL_SERVICE_ERROR,
					CAMEL_SERVICE_ERROR_INVALID,
					_("Could not get message"));
				return NULL;
			}
			if (attachment && attachment[0] && (len !=0) ) {
				if (!g_ascii_strcasecmp (attach->name, "TEXT.htm")) {
					body = g_strdup (attachment);
					g_free (attachment);
					is_text_html = TRUE;
				}
			}/* if attachment and len */
		} /* if Mime.822 or TEXT.htm */

		if (!ignore_mime_822) {
			for (al = attach_list; al != NULL; al = al->next) {
				EGwItemAttachment *attach = (EGwItemAttachment *)al->data;
				if (!g_ascii_strcasecmp (attach->name, "Mime.822")) {
					if (attach->size > MAX_ATTACHMENT_SIZE) {
						gint t_len , offset = 0, t_offset = 0;
						gchar *t_attach = NULL;
						GString *gstr = g_string_new (NULL);

						len = 0;
						do {
							status = e_gw_connection_get_attachment_base64 (cnc,
									attach->id, t_offset, MAX_ATTACHMENT_SIZE,
									(const gchar **)&t_attach, &t_len, &offset);
							if (status == E_GW_CONNECTION_STATUS_OK) {

								if (t_len) {
									gsize len_iter = 0;
									gchar *temp = NULL;

									temp = (gchar *) g_base64_decode(t_attach, &len_iter);
									gstr = g_string_append_len (gstr, temp, len_iter);
									g_free (temp);
									len += len_iter;
									g_free (t_attach);
									t_attach = NULL;
								}
								t_offset = offset;
							}
						} while (t_offset);
						body = gstr->str;
						body_len = len;
						g_string_free (gstr, FALSE);
					} else {
						status = e_gw_connection_get_attachment (cnc,
								attach->id, 0, -1,
								(const gchar **)&attachment, &len);
						if (status != E_GW_CONNECTION_STATUS_OK) {
							g_warning ("Could not get attachment\n");
							g_set_error (
								error, CAMEL_SERVICE_ERROR,
								CAMEL_SERVICE_ERROR_INVALID,
								_("Could not get message"));
							return NULL;
						}
						body = g_strdup (attachment);
						body_len = len;
						g_free (attachment);
					}
					has_mime_822 = TRUE;
				}
			}
		}

	} /* if attach_list */

	msg = camel_mime_message_new ();
	if (has_mime_822 && body) {
		temp_stream = camel_stream_mem_new_with_buffer (body, body_len);
		if (camel_data_wrapper_construct_from_stream ((CamelDataWrapper *) msg, temp_stream, error) == -1) {
			g_object_unref (msg);
			g_object_unref (temp_stream);
			msg = NULL;
			goto end;
		}
	} else {
		multipart = camel_multipart_new ();
	}

	if (!has_mime_822 ) {
		/* Set Message Id */
		message_id = e_gw_item_get_message_id (item);
		if (message_id)
			camel_medium_add_header (CAMEL_MEDIUM (msg), "Message-ID", message_id);

		/* Set parent threads */
		parent_threads = e_gw_item_get_parent_thread_ids (item);
		if (parent_threads)
			camel_medium_add_header (CAMEL_MEDIUM (msg), "References", parent_threads);
	}

	/* set item id */
	camel_medium_add_header (CAMEL_MEDIUM (msg), "X-GW-ITEM-ID", uid);

	type = e_gw_item_get_item_type (item);
	if (type == E_GW_ITEM_TYPE_NOTIFICATION)
		camel_medium_add_header ( CAMEL_MEDIUM (msg), "X-Notification", "shared-folder");

	/*If the reply-requested flag is set. Append the mail message with the
	 *          * approprite detail*/
	if (e_gw_item_get_reply_request (item)) {
		gchar *reply_within;
		const gchar *mess = e_gw_item_get_message (item);
		gchar *value;

		reply_within = e_gw_item_get_reply_within (item);
		if (reply_within) {
			time_t t;
			gchar *temp;

			t = e_gw_connection_get_date_from_string (reply_within);
			temp = ctime (&t);
			temp[strlen (temp)-1] = '\0';
			value = g_strconcat (N_("Reply Requested: by "), temp, "\n\n", mess ? mess : "", NULL);
			e_gw_item_set_message (item, (const gchar *) value);
			g_free (value);

		} else {
			value = g_strconcat (N_("Reply Requested: When convenient"), "\n\n", mess ? mess : "", NULL);
			e_gw_item_set_message (item, (const gchar *) value);
			g_free (value);
		}
	}

	if (has_mime_822)
		goto end;
	else
		groupwise_populate_msg_body_from_item (cnc, multipart, item, body);
	/*Set recipient details*/
	groupwise_msg_set_recipient_list (msg, item);
	groupwise_populate_details_from_item (msg, item);
	/*Now set attachments*/
	if (attach_list) {
		gboolean has_boundary = FALSE;
		GSList *al;

		for (al = attach_list; al != NULL; al = al->next) {
			EGwItemAttachment *attach = (EGwItemAttachment *)al->data;
			gchar *attachment = NULL;
			gint len = 0;
			CamelMimePart *part;
			EGwItem *temp_item;
			is_base64_encoded = FALSE;

			if (attach->contentid && (is_text_html_embed != TRUE))
				is_text_html_embed = TRUE;

			/* MIME.822 from server for the weak hearted client programmer */

			if ( (!g_ascii_strcasecmp (attach->name, "TEXT.htm") ||
			     !g_ascii_strcasecmp (attach->name, "Mime.822") ||
			     !g_ascii_strcasecmp (attach->name, "Header") ||
			     !g_ascii_strcasecmp (attach->name, "meeting.ics")) && (attach->hidden == TRUE))
				continue;

			if ((attach->item_reference) && (!g_ascii_strcasecmp (attach->item_reference, "1"))) {
				CamelMimeMessage *temp_msg = NULL;
				status = e_gw_connection_get_item (cnc, container_id, attach->id, GET_ITEM_VIEW_WITH_CACHE, &temp_item);
				if (status != E_GW_CONNECTION_STATUS_OK) {
					g_warning ("Could not get attachment\n");
					continue;
				}
				temp_msg = groupwise_folder_item_to_msg(folder, temp_item, error);
				if (temp_msg) {
					CamelContentType *ct = camel_content_type_new("message", "rfc822");
					part = camel_mime_part_new ();
					camel_data_wrapper_set_mime_type_field(CAMEL_DATA_WRAPPER (temp_msg), ct);
					camel_content_type_unref(ct);
					camel_medium_set_content (CAMEL_MEDIUM (part),CAMEL_DATA_WRAPPER(temp_msg));

					camel_multipart_add_part (multipart,part);
					g_object_unref (temp_msg);
					g_object_unref (part);
				}
				g_object_unref (temp_item);
			} else {
				if (attach->size > MAX_ATTACHMENT_SIZE) {
					gint t_len=0, offset=0, t_offset=0;
					gchar *t_attach = NULL;
					GString *gstr = g_string_new (NULL);

					len = 0;
					do {
						status = e_gw_connection_get_attachment_base64 (cnc,
								attach->id, t_offset, MAX_ATTACHMENT_SIZE,
								(const gchar **)&t_attach, &t_len, &offset);
						if (status == E_GW_CONNECTION_STATUS_OK) {

							if (t_len) {
								gsize len_iter = 0;
								gchar *temp = NULL;

								temp = (gchar *) g_base64_decode(t_attach, &len_iter);
								gstr = g_string_append_len (gstr, temp, len_iter);
								g_free (temp);
								len += len_iter;
								g_free (t_attach);
								t_attach = NULL;
								t_len = 0;
							}
							t_offset = offset;
						}
					} while (t_offset);
					attachment =  gstr->str;
					g_string_free (gstr, FALSE);
					is_base64_encoded = FALSE;
				} else {
					status = e_gw_connection_get_attachment (cnc,
							attach->id, 0, -1,
							(const gchar **)&attachment, &len);
				}
				if (status != E_GW_CONNECTION_STATUS_OK) {
					g_warning ("Could not get attachment\n");
					continue;
				}
				if (attachment && (len !=0) ) {
					part = camel_mime_part_new ();
					/*multiparts*/
					if (is_text_html_embed) {
						camel_mime_part_set_filename(part, g_strdup(attach->name));
						camel_data_wrapper_set_mime_type(CAMEL_DATA_WRAPPER (multipart), "multipart/related");
						has_boundary = TRUE;
						camel_content_type_set_param(CAMEL_DATA_WRAPPER (multipart)->mime_type, "type", "multipart/alternative");
						if (attach->contentid) {
							gchar **t;
							t= g_strsplit_set (attach->contentid, "<>", -1);
							if (!t[1])
								camel_mime_part_set_content_id (part, attach->contentid);
							else
								camel_mime_part_set_content_id (part, t[1]);
							g_strfreev (t);
							camel_mime_part_set_content_location (part, attach->name);
						}
					} else {
						camel_mime_part_set_filename(part, g_strdup(attach->name));
						camel_mime_part_set_content_id (part, attach->contentid);
					}

					/*camel_mime_part_set_filename(part, g_strdup(attach->name));*/
					if (attach->contentType) {
						if (is_base64_encoded)
							camel_mime_part_set_encoding (part, CAMEL_TRANSFER_ENCODING_BASE64);
						camel_mime_part_set_content(part, attachment, len, attach->contentType);
						camel_content_type_set_param (((CamelDataWrapper *) part)->mime_type, "name", attach->name);
					} else {
							camel_mime_part_set_content(part, attachment, len, "text/plain");
					}
					if (!has_boundary)
						camel_data_wrapper_set_mime_type(CAMEL_DATA_WRAPPER (multipart),"multipart/digest");

					camel_multipart_set_boundary(multipart, NULL);
					camel_multipart_add_part (multipart, part);

					g_object_unref (part);
					g_free (attachment);
				} /* if attachment */
			}
		} /* end of for*/

	}/* if attach_list */
	/********************/

	/* this is broken for translations. someone should care to fix these hacks. nobody except groupwise users are affected though */
	if (e_gw_item_get_priority (item))
		camel_medium_add_header ( CAMEL_MEDIUM (msg), "Priority", e_gw_item_get_priority(item));

	if (e_gw_item_get_security (item))
		camel_medium_add_header ( CAMEL_MEDIUM (msg), "Security", e_gw_item_get_security(item));

	camel_medium_set_content (CAMEL_MEDIUM (msg), CAMEL_DATA_WRAPPER(multipart));
	g_object_unref (multipart);

end:
	if (body)
		g_free (body);

	return msg;
}

static void
gw_update_all_items (CamelFolder *folder, GList *item_list, GError **error)
{
	CamelGroupwiseFolder *gw_folder = CAMEL_GROUPWISE_FOLDER (folder);
	GPtrArray *summary = NULL;
	gint index = 0;
	GList *temp;
	CamelFolderChangeInfo *changes = NULL;
	gchar *uid;

	changes = camel_folder_change_info_new ();

	item_list = g_list_reverse (item_list);

	summary = camel_folder_get_summary (folder);
	/*item_ids : List of ids from the summary*/
	while (index < summary->len) {
		uid = g_ptr_array_index (summary, index);
		temp = NULL;

		if (item_list) {
			temp = g_list_find_custom (item_list, (const gchar *)uid, (GCompareFunc) strcmp);
		}

		if (!temp) {
			CAMEL_GROUPWISE_FOLDER_REC_LOCK (folder, cache_lock);
			camel_folder_summary_remove_uid (folder->summary, uid);
			camel_data_cache_remove (gw_folder->cache, "cache", uid, NULL);
			camel_folder_change_info_remove_uid (changes, uid);
			CAMEL_GROUPWISE_FOLDER_REC_UNLOCK (folder, cache_lock);
		} else {
			g_free (temp->data);
			item_list = g_list_delete_link (item_list, temp);
		}
		index++;
	}

	groupwise_sync_summary (folder, error);
	camel_folder_changed (folder, changes);

	if (item_list) {
		CamelStore *parent_store;

		parent_store = camel_folder_get_parent_store (folder);

		camel_service_lock (CAMEL_SERVICE (parent_store), CAMEL_SERVICE_REC_CONNECT_LOCK);
		gw_update_cache (folder, item_list, error, TRUE);
		camel_service_unlock (CAMEL_SERVICE (parent_store), CAMEL_SERVICE_REC_CONNECT_LOCK);

		g_list_foreach (item_list, (GFunc)g_free, NULL);
		g_list_free (item_list);
	}

	camel_folder_free_summary (folder, summary);
}

static gboolean
groupwise_append_message (CamelFolder *folder, CamelMimeMessage *message,
		const CamelMessageInfo *info, gchar **appended_uid,
		GError **error)
{
	const gchar *container_id = NULL;
	CamelGroupwiseStore *gw_store;
	CamelOfflineStore *offline;
	CamelStore *parent_store;
	EGwConnectionStatus status = { 0, };
	EGwConnection *cnc;
	EGwItem *item;
	const gchar *full_name;
	const gchar *name;
	gchar *id;
	gboolean is_ok = FALSE;

	name = camel_folder_get_name (folder);
	full_name = camel_folder_get_full_name (folder);
	parent_store = camel_folder_get_parent_store (folder);

	if (!strcmp (name, RECEIVED))
		is_ok = TRUE;
	if (!strcmp (name, SENT))
		is_ok = TRUE;

	if (!is_ok) {
		g_set_error (
			error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			_("Cannot append message to folder '%s': %s"),
			full_name, e_gw_connection_get_error_message (status));
		return FALSE;
	}

	gw_store = CAMEL_GROUPWISE_STORE (parent_store);
	offline = CAMEL_OFFLINE_STORE (parent_store);

	if (offline->state == CAMEL_OFFLINE_STORE_NETWORK_UNAVAIL) {
		camel_groupwise_journal_append ((CamelGroupwiseJournal *) ((CamelGroupwiseFolder *)folder)->journal, message, info, appended_uid, error);
		return FALSE;
	}
	cnc = cnc_lookup (gw_store->priv);

	camel_service_lock (CAMEL_SERVICE (parent_store), CAMEL_SERVICE_REC_CONNECT_LOCK);
	/*Get the container id*/
	container_id = camel_groupwise_store_container_id_lookup (gw_store, full_name);

	item = camel_groupwise_util_item_from_message (cnc, message, CAMEL_ADDRESS (message->from));
	/*Set the source*/
	/* FIXME: use flags and avoid such name comparisons in future */
	if (!strcmp (name, RECEIVED))
		e_gw_item_set_source (item, "received");
	if (!strcmp (name, SENT))
		e_gw_item_set_source (item, "sent");
	if (!strcmp (name, DRAFT))
		e_gw_item_set_source (item, "draft");
	if (!strcmp (name, PERSONAL))
		e_gw_item_set_source (item, "personal");
	/*set container id*/
	e_gw_item_set_container_id (item, container_id);

	status = e_gw_connection_create_item (cnc, item, &id);
	if (status != E_GW_CONNECTION_STATUS_OK) {
		g_set_error (
			error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			_("Cannot create message: %s"),
			e_gw_connection_get_error_message (status));

		if (appended_uid)
			*appended_uid = NULL;
		camel_service_unlock (CAMEL_SERVICE (parent_store), CAMEL_SERVICE_REC_CONNECT_LOCK);
		return FALSE;
	}

	status = e_gw_connection_add_item (cnc, container_id, id);
	g_message ("Adding %s to %s", id, container_id);
	if (status != E_GW_CONNECTION_STATUS_OK) {
		g_set_error (
			error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			_("Cannot append message to folder '%s': %s"),
			full_name, e_gw_connection_get_error_message (status));

		if (appended_uid)
			*appended_uid = NULL;

		camel_service_unlock (CAMEL_SERVICE (parent_store), CAMEL_SERVICE_REC_CONNECT_LOCK);
		return FALSE;
	}

	if (appended_uid)
		*appended_uid = g_strdup (id);
	g_free (id);
	camel_service_unlock (CAMEL_SERVICE (parent_store), CAMEL_SERVICE_REC_CONNECT_LOCK);

	return TRUE;
}

/* A function to compare uids, inspired by strcmp .
This code was used in some other provider also , imap4rev1 iirc */
static gint
uid_compar (gconstpointer va, gconstpointer vb)
{
	const gchar **sa = (const gchar **)va, **sb = (const gchar **)vb;
	gulong a, b;

	a = strtoul (*sa, NULL, 10);
	b = strtoul (*sb, NULL, 10);
	if (a < b)
		return -1;
	else if (a == b)
		return 0;
	else
		return 1;
}

/* move messages */
static gboolean
groupwise_transfer_messages_to (CamelFolder *source, GPtrArray *uids,
		CamelFolder *destination, GPtrArray **transferred_uids,
		gboolean delete_originals, GError **error)
{
	gint count, index = 0;
	GList *item_ids = NULL;
	const gchar *source_container_id = NULL, *dest_container_id = NULL;
	CamelGroupwiseStore *gw_store;
	CamelOfflineStore *offline;
	CamelStore *source_parent_store;
	CamelStore *destination_parent_store;
	EGwConnectionStatus status = E_GW_CONNECTION_STATUS_OK;
	EGwConnection *cnc;
	CamelFolderChangeInfo *changes = NULL;
	gboolean destination_is_trash;
	const gchar *source_full_name;
	const gchar *destination_full_name;

	source_full_name = camel_folder_get_full_name (source);
	source_parent_store = camel_folder_get_parent_store (source);

	destination_full_name = camel_folder_get_full_name (destination);
	destination_parent_store = camel_folder_get_parent_store (destination);

	gw_store = CAMEL_GROUPWISE_STORE (source_parent_store);
	offline = CAMEL_OFFLINE_STORE (destination_parent_store);

	if (destination == camel_store_get_trash (source_parent_store, NULL))
		destination_is_trash = TRUE;
	else
		destination_is_trash = FALSE;

	count = camel_folder_summary_count (destination->summary);
	qsort (uids->pdata, uids->len, sizeof (gpointer), uid_compar);

	changes = camel_folder_change_info_new ();
	while (index < uids->len) {
		item_ids = g_list_append (item_ids, g_ptr_array_index (uids, index));
		index++;
	}

	if (transferred_uids)
		*transferred_uids = NULL;

	if (delete_originals)
		source_container_id = camel_groupwise_store_container_id_lookup (gw_store, source_full_name);
	else
		source_container_id = NULL;
	dest_container_id = camel_groupwise_store_container_id_lookup (gw_store, destination_full_name);

	camel_service_lock (CAMEL_SERVICE (source_parent_store), CAMEL_SERVICE_REC_CONNECT_LOCK);
	/* check for offline operation */
	if (offline->state == CAMEL_OFFLINE_STORE_NETWORK_UNAVAIL) {
		CamelGroupwiseJournal *journal = (CamelGroupwiseJournal *) ((CamelGroupwiseFolder *) destination)->journal;
		CamelMimeMessage *message;
		GList *l;
		gint i;

		if (destination_is_trash)
			delete_originals = TRUE;

		for (l = item_ids, i = 0; l; l = l->next, i++) {
			CamelMessageInfo *info;
			gboolean success;

			if (!(info = camel_folder_summary_uid (source->summary, uids->pdata[i])))
				continue;

			if (!(message = groupwise_folder_get_message (source, camel_message_info_uid (info), error)))
				break;

			success = camel_groupwise_journal_transfer (journal, (CamelGroupwiseFolder *)source, message, info, uids->pdata[i], NULL, error);
			g_object_unref (message);

			if (!success)
				break;

			if (delete_originals) {
				if (!strcmp(source_full_name, SENT)) {
					g_set_error (
						error, CAMEL_SERVICE_ERROR,
						CAMEL_SERVICE_ERROR_UNAVAILABLE,
						_("This message is not available in offline mode."));

				} else {
					camel_folder_summary_remove_uid (source->summary, uids->pdata[i]);
					camel_folder_change_info_remove_uid (changes, uids->pdata[i]);
				}
			}
		}

		camel_service_unlock (CAMEL_SERVICE (source_parent_store), CAMEL_SERVICE_REC_CONNECT_LOCK);
		return TRUE;
	}

	cnc = cnc_lookup (gw_store->priv);
	index = 0;
	while (index < uids->len) {
		CamelMessageInfo *info = NULL;
		CamelGroupwiseMessageInfo *gw_info = NULL;
		flags_diff_t diff, unset_flags;
		gint count;
		count = camel_folder_summary_count (destination->summary);

		info = camel_folder_summary_uid (source->summary, uids->pdata[index]);
		if (!info) {
			g_warning ("Could not find the message: its either deleted or moved already");
			index++;
			continue;
		}

		gw_info = (CamelGroupwiseMessageInfo *) info;
		if (gw_info && (gw_info->info.flags & CAMEL_MESSAGE_FOLDER_FLAGGED)) {
			do_flags_diff (&diff, gw_info->server_flags, gw_info->info.flags);
			do_flags_diff (&unset_flags, gw_info->info.flags, gw_info->server_flags);
			diff.changed &= source->permanent_flags;

			/* sync the read changes */
			if (diff.changed) {
				const gchar *uid = camel_message_info_uid (info);
				GList *wrapper = NULL;
				gw_info->info.flags &= ~CAMEL_MESSAGE_FOLDER_FLAGGED;
				gw_info->server_flags = gw_info->info.flags;

				if (diff.bits & CAMEL_MESSAGE_SEEN) {

					/*
					wrapper is a list wrapper bcos e_gw_connection_mark_read
					is designed for passing multiple uids. Also, there are is not much
					need/use for a e_gw_connection_mark_ITEM_[un]read
					*/

					wrapper = g_list_prepend (wrapper, (gchar *)uid);
					camel_service_lock (CAMEL_SERVICE (source_parent_store), CAMEL_SERVICE_REC_CONNECT_LOCK);
					e_gw_connection_mark_read (cnc, wrapper);
					camel_service_unlock (CAMEL_SERVICE (source_parent_store), CAMEL_SERVICE_REC_CONNECT_LOCK);
					g_list_free (wrapper);
					wrapper = NULL;
				}

				/* A User may mark a message as Unread and then immediately move it to
				some other folder. The following piece of code take care of such scenario.

				However, Remember that When a mail is deleted after being marked as unread,
				I am not syncing the read-status.
				*/

				if (unset_flags.bits & CAMEL_MESSAGE_SEEN) {
					wrapper = g_list_prepend (wrapper, (gchar *)uid);
					camel_service_lock (CAMEL_SERVICE (source_parent_store), CAMEL_SERVICE_REC_CONNECT_LOCK);
					e_gw_connection_mark_unread (cnc, wrapper);
					camel_service_unlock (CAMEL_SERVICE (source_parent_store), CAMEL_SERVICE_REC_CONNECT_LOCK);
					g_list_free (wrapper);
					wrapper = NULL;
				}
			}
		}

		if (destination_is_trash) {
				e_gw_connection_remove_item (cnc, source_container_id, (const gchar *) uids->pdata[index]);
				camel_folder_summary_remove_uid (source->summary, uids->pdata[index]);
				camel_folder_change_info_remove_uid (changes, uids->pdata[index]);
		} else {
				if (delete_originals) {
					if (strcmp(source_full_name, "Sent Items")) {
							status = e_gw_connection_move_item (cnc, (const gchar *)uids->pdata[index],
											dest_container_id, source_container_id);
					} else {
							gchar *container_id = NULL;
							container_id = e_gw_connection_get_container_id (cnc, "Mailbox");
							status = e_gw_connection_move_item (cnc, (const gchar *)uids->pdata[index],
											dest_container_id, container_id);
							g_free (container_id);
					}

				} else
						status = e_gw_connection_move_item (cnc, (const gchar *)uids->pdata[index],
										dest_container_id, NULL);

				if (status == E_GW_CONNECTION_STATUS_OK) {
						if (delete_originals) {
								if (!(gw_info->info.flags & CAMEL_MESSAGE_SEEN))
										source->summary->unread_count--;

								camel_folder_summary_remove_uid (source->summary, uids->pdata[index]);
								camel_folder_change_info_remove_uid (changes, uids->pdata[index]);
								/*}*/
						}
				} else {
						g_warning ("Warning!! Could not move item : %s\n", (gchar *)uids->pdata[index]);
				}

		}
		index++;
	}

	camel_folder_changed (source, changes);
	camel_folder_change_info_free (changes);

	/* Refresh the destination folder, if its not refreshed already */
	if (gw_store->current_folder != destination )
		camel_folder_refresh_info (destination, error);

	camel_folder_summary_touch (source->summary);
	camel_folder_summary_touch (destination->summary);

	groupwise_store_set_current_folder (gw_store, source);

	camel_service_unlock (CAMEL_SERVICE (source_parent_store), CAMEL_SERVICE_REC_CONNECT_LOCK);

	return TRUE;
}

static gboolean
groupwise_expunge (CamelFolder *folder, GError **error)
{
	CamelGroupwiseStore *gw_store;
	CamelGroupwiseFolder *gw_folder;
	CamelGroupwiseMessageInfo *ginfo;
	CamelMessageInfo *info;
	CamelStore *parent_store;
	gchar *container_id;
	EGwConnection *cnc;
	EGwConnectionStatus status;
	CamelFolderChangeInfo *changes;
	gint i, max;
	gboolean delete = FALSE;
	GList *deleted_items, *deleted_head;
	const gchar *full_name;

	full_name = camel_folder_get_full_name (folder);
	parent_store = camel_folder_get_parent_store (folder);

	gw_folder = CAMEL_GROUPWISE_FOLDER (folder);
	gw_store = CAMEL_GROUPWISE_STORE (parent_store);

	deleted_items = deleted_head = NULL;
	cnc = cnc_lookup (gw_store->priv);
	if (!cnc)
		return TRUE;

	if (!strcmp (full_name, "Trash")) {
		camel_service_lock (CAMEL_SERVICE (parent_store), CAMEL_SERVICE_REC_CONNECT_LOCK);
		status = e_gw_connection_purge_deleted_items (cnc);
		if (status == E_GW_CONNECTION_STATUS_OK) {
			camel_folder_freeze (folder);
			groupwise_summary_clear (folder->summary, TRUE);
			camel_folder_thaw (folder);
		} else
			g_warning ("Could not Empty Trash\n");
		camel_service_unlock (CAMEL_SERVICE (parent_store), CAMEL_SERVICE_REC_CONNECT_LOCK);
		return TRUE;
	}

	changes = camel_folder_change_info_new ();

	container_id =  g_strdup (camel_groupwise_store_container_id_lookup (gw_store, full_name));

	camel_folder_summary_prepare_fetch_all (folder->summary, error);
	max = camel_folder_summary_count (folder->summary);
	for (i = 0; i < max; i++) {
		info = camel_folder_summary_index (folder->summary, i);
		ginfo = (CamelGroupwiseMessageInfo *) info;
		if (ginfo && (ginfo->info.flags & CAMEL_MESSAGE_DELETED)) {

			if (deleted_items)
				deleted_items = g_list_prepend (deleted_items, (gchar *)camel_message_info_uid (info));
			else {
				g_list_free (deleted_head);
				deleted_head = NULL;
				deleted_head = deleted_items = g_list_prepend (deleted_items, (gchar *)camel_message_info_uid (info));
			}
			if (g_list_length (deleted_items) == GROUPWISE_BULK_DELETE_LIMIT ) {
				/* Read the FIXME below */
				camel_service_lock (CAMEL_SERVICE (parent_store), CAMEL_SERVICE_REC_CONNECT_LOCK);
				status = e_gw_connection_remove_items (cnc, container_id, deleted_items);
				camel_service_unlock (CAMEL_SERVICE (parent_store), CAMEL_SERVICE_REC_CONNECT_LOCK);
				if (status == E_GW_CONNECTION_STATUS_OK) {
					gchar *uid;
					while (deleted_items) {
						uid = (gchar *)deleted_items->data;
						CAMEL_GROUPWISE_FOLDER_REC_LOCK (folder, cache_lock);
						camel_folder_change_info_remove_uid (changes, uid);
						camel_folder_summary_remove_uid (folder->summary, uid);
						camel_data_cache_remove(gw_folder->cache, "cache", uid, NULL);
						CAMEL_GROUPWISE_FOLDER_REC_UNLOCK (folder, cache_lock);
						deleted_items = g_list_next (deleted_items);
						max -= GROUPWISE_BULK_DELETE_LIMIT;
						i -= GROUPWISE_BULK_DELETE_LIMIT;
					}
				}
				delete = TRUE;
			}
		}
		camel_message_info_free (info);
	}

	if (deleted_items) {
		/* FIXME: Put these in a function and reuse it inside the above loop, here and in groupwise_sync*/
		camel_service_lock (CAMEL_SERVICE (parent_store), CAMEL_SERVICE_REC_CONNECT_LOCK);
		status = e_gw_connection_remove_items (cnc, container_id, deleted_items);
		camel_service_unlock (CAMEL_SERVICE (parent_store), CAMEL_SERVICE_REC_CONNECT_LOCK);
		if (status == E_GW_CONNECTION_STATUS_OK) {
			gchar *uid;
			while (deleted_items) {
				uid = (gchar *)deleted_items->data;
				CAMEL_GROUPWISE_FOLDER_REC_LOCK (folder, cache_lock);
				camel_folder_change_info_remove_uid (changes, uid);
				camel_folder_summary_remove_uid (folder->summary, uid);
				camel_data_cache_remove(gw_folder->cache, "cache", uid, NULL);
				CAMEL_GROUPWISE_FOLDER_REC_UNLOCK (folder, cache_lock);
				deleted_items = g_list_next (deleted_items);
			}
		}
		delete = TRUE;
		g_list_free (deleted_head);
	}

	if (delete)
		camel_folder_changed (folder, changes);

	g_free (container_id);
	camel_folder_change_info_free (changes);

	return TRUE;
}

static gint
groupwise_cmp_uids (CamelFolder *folder, const gchar *uid1, const gchar *uid2)
{
	g_return_val_if_fail (uid1 != NULL, 0);
	g_return_val_if_fail (uid2 != NULL, 0);

	return strcmp (uid1, uid2);
}

static void
groupwise_folder_dispose (GObject *object)
{
	CamelGroupwiseFolder *gw_folder = CAMEL_GROUPWISE_FOLDER (object);

	if (gw_folder->cache != NULL) {
		g_object_unref (gw_folder->cache);
		gw_folder->cache = NULL;
	}

	if (gw_folder->search != NULL) {
		g_object_unref (gw_folder->search);
		gw_folder->search = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (camel_groupwise_folder_parent_class)->dispose (object);
}

static void
groupwise_folder_constructed (GObject *object)
{
	CamelFolder *folder;
	CamelStore *parent_store;
	CamelURL *url;
	const gchar *full_name;
	gchar *description;

	folder = CAMEL_FOLDER (object);
	full_name = camel_folder_get_full_name (folder);
	parent_store = camel_folder_get_parent_store (folder);
	url = CAMEL_SERVICE (parent_store)->url;

	description = g_strdup_printf (
		"%s@%s:%s", url->user, url->host, full_name);
	camel_folder_set_description (folder, description);
	g_free (description);
}

static void
camel_groupwise_folder_class_init (CamelGroupwiseFolderClass *class)
{
	GObjectClass *object_class;
	CamelFolderClass *folder_class;

	g_type_class_add_private (class, sizeof (CamelGroupwiseFolderPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = groupwise_folder_dispose;
	object_class->constructed = groupwise_folder_constructed;

	folder_class = CAMEL_FOLDER_CLASS (class);
	folder_class->get_message = groupwise_folder_get_message;
	folder_class->rename = groupwise_folder_rename;
	folder_class->search_by_expression = groupwise_folder_search_by_expression;
	folder_class->count_by_expression = groupwise_folder_count_by_expression;
	folder_class->cmp_uids = groupwise_cmp_uids;
	folder_class->search_by_uids = groupwise_folder_search_by_uids;
	folder_class->search_free = groupwise_folder_search_free;
	folder_class->append_message = groupwise_append_message;
	folder_class->refresh_info = groupwise_refresh_info;
	folder_class->sync = groupwise_sync_all;
	folder_class->set_message_flags = groupwise_set_message_flags;
	folder_class->expunge = groupwise_expunge;
	folder_class->transfer_messages_to = groupwise_transfer_messages_to;
	folder_class->get_filename = groupwise_get_filename;
}

static void
camel_groupwise_folder_init (CamelGroupwiseFolder *gw_folder)
{
	CamelFolder *folder = CAMEL_FOLDER (gw_folder);

	gw_folder->priv = CAMEL_GROUPWISE_FOLDER_GET_PRIVATE (gw_folder);

	folder->permanent_flags = CAMEL_MESSAGE_ANSWERED | CAMEL_MESSAGE_DELETED |
		CAMEL_MESSAGE_DRAFT | CAMEL_MESSAGE_FLAGGED | CAMEL_MESSAGE_SEEN;

	folder->folder_flags = CAMEL_FOLDER_HAS_SUMMARY_CAPABILITY | CAMEL_FOLDER_HAS_SEARCH_CAPABILITY;

#ifdef ENABLE_THREADS
	g_static_mutex_init(&gw_folder->priv->search_lock);
	g_static_rec_mutex_init(&gw_folder->priv->cache_lock);
#endif

	gw_folder->need_rescan = TRUE;
}

void
convert_to_calendar (EGwItem *item, gchar **str, gint *len)
{
	EGwItemOrganizer *org = NULL;
	GSList *recp_list = NULL;
	GSList *attach_list = NULL;
	GString *gstr = g_string_new (NULL);
	gint recur_key = 0;
	gchar **tmp = NULL;
	const gchar *temp = NULL;
	const gchar *tmp_dt = NULL;

	tmp = g_strsplit (e_gw_item_get_id (item), "@", -1);

	gstr = g_string_append (gstr, "BEGIN:VCALENDAR\n");
	gstr = g_string_append (gstr, "METHOD:REQUEST\n");
	gstr = g_string_append (gstr, "BEGIN:VEVENT\n");

	if ((recur_key = e_gw_item_get_recurrence_key (item)) != 0) {
		gchar *recur_k = g_strdup_printf ("%d", recur_key);

		g_string_append_printf (gstr, "UID:%s\n", recur_k);
		g_string_append_printf (gstr, "X-GW-RECURRENCE-KEY:%s\n", recur_k);

		g_free (recur_k);
	} else
		g_string_append_printf (gstr, "UID:%s\n",e_gw_item_get_icalid (item));

	g_string_append_printf (gstr, "X-GWITEM-TYPE:APPOINTMENT\n");
	tmp_dt = e_gw_item_get_start_date (item);
	if (tmp_dt)
		g_string_append_printf (gstr, "DTSTART:%s\n", tmp_dt);
	g_string_append_printf (gstr, "SUMMARY:%s\n", e_gw_item_get_subject (item));

	temp = e_gw_item_get_message (item);
	if (temp) {
		g_string_append(gstr, "DESCRIPTION:");
		while (*temp) {
			if (*temp == '\n')
				g_string_append(gstr, "\\n");
			else
				g_string_append_c(gstr, *temp);
			temp++;
		}
		g_string_append(gstr, "\n");
	}

	tmp_dt = e_gw_item_get_creation_date (item);
	if (tmp_dt)
		g_string_append_printf (gstr, "DTSTAMP:%s\n", tmp_dt);
	g_string_append_printf (gstr, "X-GWMESSAGEID:%s\n", e_gw_item_get_id (item));
	g_string_append_printf (gstr, "X-GWSHOW-AS:BUSY\n");
	g_string_append_printf (gstr, "X-GWRECORDID:%s\n", tmp[0]);

	org = e_gw_item_get_organizer (item);
	if (org)
		g_string_append_printf (gstr, "ORGANIZER;CN= %s;ROLE= CHAIR;\n MAILTO:%s\n",
				org->display_name, org->email);

	recp_list = e_gw_item_get_recipient_list (item);
	if (recp_list) {
		GSList *rl;

		for (rl = recp_list; rl != NULL; rl = rl->next) {
			EGwItemRecipient *recp = (EGwItemRecipient *) rl->data;
			g_string_append_printf (gstr,
					"ATTENDEE;CN= %s;ROLE= REQ-PARTICIPANT:\nMAILTO:%s\n",
					recp->display_name, recp->email);
		}
	}

	tmp_dt = e_gw_item_get_end_date (item);
	if (tmp_dt)
		g_string_append_printf (gstr, "DTEND:%s\n", tmp_dt);

	temp = NULL;
	temp = e_gw_item_get_place (item);
	if (temp)
		g_string_append_printf (gstr, "LOCATION:%s\n", temp);

	temp = NULL;
	temp = e_gw_item_get_task_priority (item);
	if (temp)
		g_string_append_printf (gstr, "PRIORITY:%s\n", temp);

	temp = NULL;
	attach_list = e_gw_item_get_attach_id_list (item);
	if (attach_list) {
		GSList *al;

		for (al = attach_list; al != NULL; al = al->next) {
			EGwItemAttachment *attach = (EGwItemAttachment *)al->data;
			g_string_append_printf (gstr, "ATTACH:%s\n", attach->id);
		}
	}
	gstr = g_string_append (gstr, "END:VEVENT\n");
	gstr = g_string_append (gstr, "END:VCALENDAR\n");

	*str = gstr->str;
	*len = gstr->len;

	g_string_free (gstr, FALSE);
	g_strfreev (tmp);
}

static void
convert_to_task (EGwItem *item, gchar **str, gint *len)
{
	EGwItemOrganizer *org = NULL;
	GSList *recp_list = NULL;
	GString *gstr = g_string_new (NULL);
	gchar **tmp = NULL;
	const gchar *temp = NULL;
	const gchar *tmp_dt = NULL;

	tmp = g_strsplit (e_gw_item_get_id (item), "@", -1);

	gstr = g_string_append (gstr, "BEGIN:VCALENDAR\n");
	gstr = g_string_append (gstr, "METHOD:REQUEST\n");
	gstr = g_string_append (gstr, "BEGIN:VTODO\n");
	g_string_append_printf (gstr, "UID:%s\n",e_gw_item_get_icalid (item));
	tmp_dt = e_gw_item_get_start_date (item);
	if (tmp_dt)
		g_string_append_printf (gstr, "DTSTART:%s\n", tmp_dt);
	g_string_append_printf (gstr, "SUMMARY:%s\n", e_gw_item_get_subject (item));

	temp = e_gw_item_get_message (item);
	if (temp) {
		g_string_append(gstr, "DESCRIPTION:");
		while (*temp) {
			if (*temp == '\n')
				g_string_append(gstr, "\\n");
			else
				g_string_append_c(gstr, *temp);
			temp++;
		}
		g_string_append(gstr, "\n");
	}
	temp = NULL;

	tmp_dt = e_gw_item_get_creation_date (item);
	if (tmp_dt)
		g_string_append_printf (gstr, "DTSTAMP:%s\n", tmp_dt);
	g_string_append_printf (gstr, "X-GWMESSAGEID:%s\n", e_gw_item_get_id (item));
	g_string_append_printf (gstr, "X-GWSHOW-AS:BUSY\n");
	g_string_append_printf (gstr, "X-GWRECORDID:%s\n", tmp[0]);

	org = e_gw_item_get_organizer (item);
	if (org)
		g_string_append_printf (gstr, "ORGANIZER;CN= %s;ROLE= CHAIR;\n MAILTO:%s\n",
				org->display_name, org->email);

	recp_list = e_gw_item_get_recipient_list (item);
	if (recp_list) {
		GSList *rl;

		for (rl = recp_list; rl != NULL; rl = rl->next) {
			EGwItemRecipient *recp = (EGwItemRecipient *) rl->data;
			g_string_append_printf (gstr,
					"ATTENDEE;CN= %s;ROLE= REQ-PARTICIPANT:\nMAILTO:%s\n",
					recp->display_name, recp->email);
		}
	}

	tmp_dt = e_gw_item_get_end_date (item);
	if (tmp_dt)
		g_string_append_printf (gstr, "DTEND:%s\n", tmp_dt);

	temp = e_gw_item_get_place (item);
	if (temp)
		g_string_append_printf (gstr, "LOCATION:%s\n", temp);

	temp = NULL;
	temp = e_gw_item_get_task_priority (item);
	if (temp)
		g_string_append_printf (gstr, "PRIORITY:%s\n", temp);

	temp = NULL;
	temp = e_gw_item_get_due_date (item);
	if (temp)
		g_string_append_printf (gstr, "DUE:%s\n", temp);
	gstr = g_string_append (gstr, "END:VTODO\n");
	gstr = g_string_append (gstr, "END:VCALENDAR\n");

	*str = g_strdup (gstr->str);
	*len = gstr->len;

	g_string_free (gstr, TRUE);
	g_strfreev (tmp);
}

static void
convert_to_note (EGwItem *item, gchar **str, gint *len)
{
	EGwItemOrganizer *org = NULL;
	GString *gstr = g_string_new (NULL);
	gchar **tmp = NULL;
	const gchar *temp = NULL;
	const gchar *tmp_dt = NULL;

	tmp = g_strsplit (e_gw_item_get_id (item), "@", -1);

	gstr = g_string_append (gstr, "BEGIN:VCALENDAR\n");
	gstr = g_string_append (gstr, "METHOD:PUBLISH\n");
	gstr = g_string_append (gstr, "BEGIN:VJOURNAL\n");
	g_string_append_printf (gstr, "UID:%s\n",e_gw_item_get_icalid (item));
	tmp_dt = e_gw_item_get_start_date (item);
	if (tmp_dt)
		g_string_append_printf (gstr, "DTSTART:%s\n", tmp_dt);
	g_string_append_printf (gstr, "SUMMARY:%s\n", e_gw_item_get_subject (item));

	temp = e_gw_item_get_message (item);
	if (temp) {
		g_string_append(gstr, "DESCRIPTION:");
		while (*temp) {
			if (*temp == '\n')
				g_string_append(gstr, "\\n");
			else
				g_string_append_c(gstr, *temp);
			temp++;
		}
		g_string_append(gstr, "\n");
	}
	temp = NULL;

	tmp_dt = e_gw_item_get_creation_date (item);
	if (tmp_dt)
		g_string_append_printf (gstr, "DTSTAMP:%s\n", tmp_dt);
	g_string_append_printf (gstr, "X-GWMESSAGEID:%s\n", e_gw_item_get_id (item));
	g_string_append_printf (gstr, "X-GWRECORDID:%s\n", tmp[0]);

	org = e_gw_item_get_organizer (item);
	if (org)
		g_string_append_printf (gstr, "ORGANIZER;CN= %s;ROLE= CHAIR;\n MAILTO:%s\n",
				org->display_name, org->email);

	gstr = g_string_append (gstr, "END:VJOURNAL\n");
	gstr = g_string_append (gstr, "END:VCALENDAR\n");

	*str = g_strdup (gstr->str);
	*len = gstr->len;

	g_string_free (gstr, TRUE);
	g_strfreev (tmp);
}

/** End **/
