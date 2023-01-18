/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright 2004 Novell, Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>

#include <glib/gi18n-lib.h>

#include "camel-exchange-journal.h"
#include "camel-exchange-store.h"
#include "camel-exchange-summary.h"
#include "camel-exchange-utils.h"

#define d(x)

G_DEFINE_TYPE (CamelExchangeJournal, camel_exchange_journal, CAMEL_TYPE_OFFLINE_JOURNAL)

static void
exchange_message_info_dup_to (CamelMessageInfoBase *dest,
                              CamelMessageInfoBase *src)
{
	camel_flag_list_copy (&dest->user_flags, &src->user_flags);
	camel_tag_list_copy (&dest->user_tags, &src->user_tags);
	dest->date_received = src->date_received;
	dest->date_sent = src->date_sent;
	dest->flags = src->flags;
	dest->size = src->size;
}

static gint
exchange_entry_play_append (CamelOfflineJournal *journal,
                            CamelExchangeJournalEntry *entry,
                            GError **error)
{
	CamelExchangeFolder *exchange_folder = (CamelExchangeFolder *) journal->folder;
	CamelFolder *folder = journal->folder;
	CamelMimeMessage *message;
	CamelMessageInfo *info, *real;
	CamelStream *stream;
	gchar *uid = NULL;

	/* if the message isn't in the cache, the user went behind our backs so "not our problem" */
	if (!exchange_folder->cache || !(stream = camel_data_cache_get (exchange_folder->cache, "cache", entry->uid, NULL)))
		goto done;

	message = camel_mime_message_new ();
	if (camel_data_wrapper_construct_from_stream ((CamelDataWrapper *) message, stream, NULL) == -1) {
		g_object_unref (message);
		g_object_unref (stream);
		goto done;
	}

	g_object_unref (stream);

	if (!(info = camel_folder_summary_uid (folder->summary, entry->uid))) {
		/* Should have never happened, but create a new info to avoid further crashes */
		info = camel_message_info_new (NULL);
	}

	if (!camel_folder_append_message (folder, message, info, &uid, error))
		return -1;

	real = camel_folder_summary_info_new_from_message (folder->summary, message, NULL);
	g_object_unref (message);

	if (uid != NULL && real) {
		real->uid = camel_pstring_strdup (uid);
		exchange_message_info_dup_to ((CamelMessageInfoBase *) real, (CamelMessageInfoBase *) info);
		camel_folder_summary_add (folder->summary, real);
		/* FIXME: should a folder_changed event be triggered? */
	}
	camel_message_info_free (info);
	g_free (uid);

 done:

	camel_exchange_folder_remove_message (exchange_folder, entry->uid);

	return 0;
}

static gint
exchange_entry_play_transfer (CamelOfflineJournal *journal,
                              CamelExchangeJournalEntry *entry,
                              GError **error)
{
	CamelExchangeFolder *exchange_folder = (CamelExchangeFolder *) journal->folder;
	CamelFolder *folder = journal->folder;
	CamelMessageInfo *info, *real;
	GPtrArray *xuids, *uids;
	CamelFolder *src;
	CamelExchangeStore *store;
	CamelStream *stream;
	CamelMimeMessage *message;
	CamelStore *parent_store;

	if (!exchange_folder->cache || !(stream = camel_data_cache_get (exchange_folder->cache, "cache", entry->uid, NULL)))
		goto done;

	message = camel_mime_message_new ();
	if (camel_data_wrapper_construct_from_stream ((CamelDataWrapper *) message, stream, NULL) == -1) {
		g_object_unref (message);
		g_object_unref (stream);
		goto done;
	}

	g_object_unref (stream);

	if (!(info = camel_folder_summary_uid (folder->summary, entry->uid))) {
		/* Note: this should never happen, but rather than crash lets make a new info */
		info = camel_message_info_new (NULL);
	}

	if (!entry->folder_name) {
		g_set_error (
			error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			_("No folder name found"));
		goto exception;
	}

	parent_store = camel_folder_get_parent_store (folder);
	store = CAMEL_EXCHANGE_STORE (parent_store);

	g_mutex_lock (store->folders_lock);
	src = (CamelFolder *) g_hash_table_lookup (store->folders, entry->folder_name);
	g_mutex_unlock (store->folders_lock);

	if (src) {
		gboolean success;
		uids = g_ptr_array_sized_new (1);
		g_ptr_array_add (uids, entry->original_uid);

		success = camel_folder_transfer_messages_to (
			src, uids, folder, &xuids,
			entry->delete_original, error);
		if (!success)
			goto exception;

		real = camel_folder_summary_info_new_from_message (
			folder->summary, message, NULL);
		g_object_unref (message);
		real->uid = camel_pstring_strdup (
			(gchar *)xuids->pdata[0]);
		/* Transfer flags */
		exchange_message_info_dup_to (
			(CamelMessageInfoBase *) real,
			(CamelMessageInfoBase *) info);
		camel_folder_summary_add (folder->summary, real);
		/* FIXME: should a folder_changed event be triggered? */

		g_ptr_array_free (xuids, TRUE);
		g_ptr_array_free (uids, TRUE);
		/* g_object_unref (src); FIXME: should we? */

	} else {
		g_set_error (
			error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			_("Folder doesn't exist"));
		goto exception;
	}

	camel_message_info_free (info);
done:
	camel_exchange_folder_remove_message (exchange_folder, entry->uid);

	return 0;

exception:

	camel_message_info_free (info);

	return -1;
}

static gint
exchange_entry_play_delete (CamelOfflineJournal *journal,
                            CamelExchangeJournalEntry *entry,
                            GError **error)
{
	CamelFolder *folder;
	CamelStore *parent_store;
	const gchar *full_name;
	gboolean success;

	folder = CAMEL_FOLDER (journal->folder);
	full_name = camel_folder_get_full_name (folder);
	parent_store = camel_folder_get_parent_store (folder);

	success = camel_exchange_utils_set_message_flags (
		CAMEL_SERVICE (parent_store), full_name,
		entry->uid, entry->set, entry->flags, error);

	return success ? 0 : -1;
}

static void
exchange_journal_entry_free (CamelOfflineJournal *journal,
                             CamelDListNode *entry)
{
	CamelExchangeJournalEntry *exchange_entry;

	exchange_entry = (CamelExchangeJournalEntry *) entry;

	g_free (exchange_entry->uid);
	g_free (exchange_entry->original_uid);
	g_free (exchange_entry->folder_name);
	g_free (exchange_entry);
}

static CamelDListNode *
exchange_journal_entry_load (CamelOfflineJournal *journal,
                             FILE *in)
{
	CamelExchangeJournalEntry *entry;
	gchar *tmp;

	entry = g_malloc0 (sizeof (CamelExchangeJournalEntry));

	if (camel_file_util_decode_uint32 (in, (guint32 *) &entry->type) == -1)
		goto exception;

	switch (entry->type) {
	case CAMEL_EXCHANGE_JOURNAL_ENTRY_APPEND:
		if (camel_file_util_decode_string (in, &entry->uid) == -1)
			goto exception;

		break;
	case CAMEL_EXCHANGE_JOURNAL_ENTRY_TRANSFER:
		if (camel_file_util_decode_string (in, &entry->uid) == -1)
			goto exception;
		if (camel_file_util_decode_string (in, &entry->original_uid) == -1)
			goto exception;
		if (camel_file_util_decode_string (in, &entry->folder_name) == -1)
			goto exception;
		if (camel_file_util_decode_string (in, &tmp) == -1)
			goto exception;
		if (g_ascii_strcasecmp (tmp, "True") == 0)
			entry->delete_original = TRUE;
		else
			entry->delete_original = FALSE;
		g_free (tmp);
		break;
	case CAMEL_EXCHANGE_JOURNAL_ENTRY_DELETE:
		if (camel_file_util_decode_string (in, &entry->uid) == -1)
			goto exception;
		if (camel_file_util_decode_string (in, &tmp) == -1)
			goto exception;
		entry->flags = atoi (tmp);
		g_free (tmp);
		if (camel_file_util_decode_string (in, &tmp) == -1)
			goto exception;
		entry->set = atoi (tmp);
		g_free (tmp);
		break;
	default:
		goto exception;
	}

	return (CamelDListNode *) entry;

 exception:

	g_free (entry->folder_name);
	g_free (entry->original_uid);
	g_free (entry->uid);
	g_free (entry);

	return NULL;
}

static gint
exchange_journal_entry_write (CamelOfflineJournal *journal,
                              CamelDListNode *entry,
                              FILE *out)
{
	CamelExchangeJournalEntry *exchange_entry = (CamelExchangeJournalEntry *) entry;
	const gchar *string;
	gchar *tmp;

	if (camel_file_util_encode_uint32 (out, exchange_entry->type) == -1)
		return -1;

	switch (exchange_entry->type) {
	case CAMEL_EXCHANGE_JOURNAL_ENTRY_APPEND:
		if (camel_file_util_encode_string (out, exchange_entry->uid))
			return -1;
		break;
	case CAMEL_EXCHANGE_JOURNAL_ENTRY_TRANSFER:
		if (camel_file_util_encode_string (out, exchange_entry->uid))
			return -1;
		if (camel_file_util_encode_string (out, exchange_entry->original_uid))
			return -1;
		if (camel_file_util_encode_string (out, exchange_entry->folder_name))
			return -1;
		string = exchange_entry->delete_original ? "True" : "False";
		if (camel_file_util_encode_string (out, string))
			return -1;
		break;
	case CAMEL_EXCHANGE_JOURNAL_ENTRY_DELETE:
		if (camel_file_util_encode_string (out, exchange_entry->uid))
			return -1;
		tmp = g_strdup_printf ("%u", exchange_entry->flags);
		if (camel_file_util_encode_string (out, tmp))
			return -1;
		g_free (tmp);
		tmp = g_strdup_printf ("%u", exchange_entry->set);
		if (camel_file_util_encode_string (out, tmp))
			return -1;
		g_free (tmp);
		break;
	default:
		g_critical ("%s: Uncaught case (%d)", G_STRLOC, exchange_entry->type);
		return -1;
	}

	return 0;
}

static gint
exchange_journal_entry_play (CamelOfflineJournal *journal,
                             CamelDListNode *entry,
                             GError **error)
{
	CamelExchangeJournalEntry *exchange_entry = (CamelExchangeJournalEntry *) entry;

	switch (exchange_entry->type) {
	case CAMEL_EXCHANGE_JOURNAL_ENTRY_APPEND:
		return exchange_entry_play_append (
			journal, exchange_entry, error);
	case CAMEL_EXCHANGE_JOURNAL_ENTRY_TRANSFER:
		return exchange_entry_play_transfer (
			journal, exchange_entry, error);
	case CAMEL_EXCHANGE_JOURNAL_ENTRY_DELETE:
		return exchange_entry_play_delete (
			journal, exchange_entry, error);
	default:
		g_critical ("%s: Uncaught case (%d)", G_STRLOC, exchange_entry->type);
		return -1;
	}
}

static void
camel_exchange_journal_class_init (CamelExchangeJournalClass *class)
{
	CamelOfflineJournalClass *offline_journal_class;

	offline_journal_class = CAMEL_OFFLINE_JOURNAL_CLASS (class);
	offline_journal_class->entry_free = exchange_journal_entry_free;
	offline_journal_class->entry_load = exchange_journal_entry_load;
	offline_journal_class->entry_write = exchange_journal_entry_write;
	offline_journal_class->entry_play = exchange_journal_entry_play;
}

static void
camel_exchange_journal_init (CamelExchangeJournal *journal)
{
}

CamelOfflineJournal *
camel_exchange_journal_new (CamelExchangeFolder *folder, const gchar *filename)
{
	CamelOfflineJournal *journal;

	g_return_val_if_fail (CAMEL_IS_EXCHANGE_FOLDER (folder), NULL);

	journal = g_object_new (CAMEL_TYPE_EXCHANGE_JOURNAL, NULL);
	camel_offline_journal_construct (journal, (CamelFolder *) folder, filename);

	return journal;
}

static gboolean
update_cache (CamelExchangeJournal *exchange_journal,
              CamelMimeMessage *message,
              const CamelMessageInfo *mi,
              gchar **updated_uid,
              GError **error)
{
	CamelOfflineJournal *journal = (CamelOfflineJournal *) exchange_journal;
	CamelExchangeFolder *exchange_folder = (CamelExchangeFolder *) journal->folder;
	CamelFolder *folder = (CamelFolder *) journal->folder;
	CamelMessageInfo *info;
	CamelStream *cache;
	guint32 nextuid;
	gchar *uid;

	if (exchange_folder->cache == NULL) {
		g_set_error (
			error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			_("Cannot append message in offline mode: "
			  "cache unavailable"));
		return FALSE;
	}

	nextuid = camel_folder_summary_next_uid (folder->summary);
	uid = g_strdup_printf ("-%u", nextuid);

	cache = camel_data_cache_add (
		exchange_folder->cache, "cache", uid, error);
	if (cache == NULL) {
		folder->summary->nextuid--;
		g_free (uid);
		return FALSE;
	}

	if (camel_data_wrapper_write_to_stream (
		(CamelDataWrapper *) message, cache, error) == -1
	    || camel_stream_flush (cache, error) == -1) {
		g_prefix_error (
			error, _("Cannot append message in offline mode: "));
		camel_data_cache_remove (
			exchange_folder->cache, "cache", uid, NULL);
		folder->summary->nextuid--;
		g_object_unref (cache);
		g_free (uid);
		return FALSE;
	}

	g_object_unref (cache);

	info = camel_folder_summary_info_new_from_message (
		folder->summary, message, NULL);
	info->uid = camel_pstring_strdup (uid);

	exchange_message_info_dup_to (
		(CamelMessageInfoBase *) info,
		(CamelMessageInfoBase *) mi);

	camel_folder_summary_add (folder->summary, info);

	if (updated_uid)
		*updated_uid = g_strdup (uid);

	g_free (uid);

	return TRUE;
}

gboolean
camel_exchange_journal_append (CamelExchangeJournal *exchange_journal,
                               CamelMimeMessage *message,
                               const CamelMessageInfo *mi,
                               gchar **appended_uid,
                               GError **error)
{
	CamelOfflineJournal *journal = (CamelOfflineJournal *) exchange_journal;
	CamelExchangeJournalEntry *entry;
	gchar *uid;

	if (!update_cache (exchange_journal, message, mi, &uid, error))
		return FALSE;

	entry = g_new (CamelExchangeJournalEntry, 1);
	entry->type = CAMEL_EXCHANGE_JOURNAL_ENTRY_APPEND;
	entry->uid = uid;

	camel_dlist_addtail (&journal->queue, (CamelDListNode *) entry);

	if (appended_uid)
		*appended_uid = g_strdup (uid);

	return TRUE;
}

static gint
find_real_source_for_message (CamelExchangeFolder *folder,
                              const gchar **folder_name,
                              const gchar **uid,
                              gboolean delete_original)
{
	CamelOfflineJournal *journal = folder->journal;
	CamelDListNode *entry, *next;
	CamelExchangeJournalEntry *ex_entry;
	const gchar *offline_uid = *uid;
	gint type = -1;

	if (*offline_uid != '-') {
		return CAMEL_EXCHANGE_JOURNAL_ENTRY_TRANSFER;
	}

	entry = journal->queue.head;
	while (entry->next) {
		next = entry->next;

		ex_entry = (CamelExchangeJournalEntry *) entry;
		if (!g_ascii_strcasecmp (ex_entry->uid, offline_uid)) {
			if (ex_entry->type == CAMEL_EXCHANGE_JOURNAL_ENTRY_TRANSFER) {
				*uid = ex_entry->original_uid;
				*folder_name = ex_entry->folder_name;
				type = CAMEL_EXCHANGE_JOURNAL_ENTRY_TRANSFER;
			} else if (ex_entry->type == CAMEL_EXCHANGE_JOURNAL_ENTRY_APPEND) {
				type = CAMEL_EXCHANGE_JOURNAL_ENTRY_APPEND;
			}

			if (delete_original) {
				camel_dlist_remove (entry);
			}
		}

		entry = next;
	}

	return type;
}

gboolean
camel_exchange_journal_transfer (CamelExchangeJournal *exchange_journal,
                                 CamelExchangeFolder *source_folder,
                                 CamelMimeMessage *message,
                                 const CamelMessageInfo *mi,
                                 const gchar *original_uid,
                                 gchar **transferred_uid,
                                 gboolean delete_original,
                                 GError **error)
{
	CamelOfflineJournal *journal = (CamelOfflineJournal *) exchange_journal;
	CamelExchangeJournalEntry *entry;
	gchar *uid;
	const gchar *real_source_folder = NULL, *real_uid = NULL;
	gint type;

	if (!update_cache (exchange_journal, message, mi, &uid, error))
		return FALSE;

	real_uid = original_uid;
	real_source_folder = camel_folder_get_full_name (
		CAMEL_FOLDER (source_folder));

	type = find_real_source_for_message (
		source_folder, &real_source_folder,
		&real_uid, delete_original);

	if (delete_original)
		camel_exchange_folder_remove_message (
			source_folder, original_uid);

	entry = g_new (CamelExchangeJournalEntry, 1);
	entry->type = type;
	entry->uid = uid;

	if (type == CAMEL_EXCHANGE_JOURNAL_ENTRY_TRANSFER) {
		entry->original_uid = g_strdup (real_uid);
		entry->folder_name = g_strdup (real_source_folder);
		entry->delete_original = delete_original;
	}

	camel_dlist_addtail (&journal->queue, (CamelDListNode *) entry);

	if (transferred_uid)
		*transferred_uid = g_strdup (uid);

	return TRUE;
}

gboolean
camel_exchange_journal_delete (CamelExchangeJournal *exchange_journal,
                               const gchar *uid,
                               guint32 flags,
                               guint32 set,
                               GError **error)
{
	CamelOfflineJournal *journal = (CamelOfflineJournal *) exchange_journal;
	CamelExchangeFolder *exchange_folder = (CamelExchangeFolder *) journal->folder;
	CamelExchangeJournalEntry *entry;

	if (set & flags & CAMEL_MESSAGE_DELETED)
		camel_exchange_folder_remove_message (exchange_folder, uid);

	entry = g_new0 (CamelExchangeJournalEntry, 1);
	entry->type = CAMEL_EXCHANGE_JOURNAL_ENTRY_DELETE;
	entry->uid = g_strdup (uid);
	entry->flags = flags;
	entry->set = set;

	camel_dlist_addtail (&journal->queue, (CamelDListNode *) entry);

	return TRUE;
}

