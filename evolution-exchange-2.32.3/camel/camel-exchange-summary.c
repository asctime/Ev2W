/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Copyright (C) 2000-2004 Novell, Inc.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "camel-exchange-folder.h"
#include "camel-exchange-journal.h"
#include "camel-exchange-summary.h"
#include "camel-exchange-utils.h"

#define CAMEL_EXCHANGE_SUMMARY_VERSION (2)

#define EXTRACT_FIRST_DIGIT(val) val=strtoul (part, &part, 10);
#define EXTRACT_DIGIT(val) part++; val=strtoul (part, &part, 10);
#define EXTRACT_FIRST_STRING(val) len=strtoul (part, &part, 10); if (*part) part++; val=g_strndup (part, len); part+=len;
#define EXTRACT_STRING(val) if (*part) part++; len=strtoul (part, &part, 10); if (*part) part++; val=g_strndup (part, len); part+=len;

#define d(x)

G_DEFINE_TYPE (CamelExchangeSummary, camel_exchange_summary, CAMEL_TYPE_FOLDER_SUMMARY)

static gboolean
exchange_summary_check_for_trash (CamelFolder *folder)
{
	CamelStore *parent_store;
	CamelFolder *trash;

	parent_store = camel_folder_get_parent_store (folder);
	trash = camel_store_get_trash (parent_store, NULL);

	if (trash == NULL)
		return FALSE;

	return folder == trash;
}

static gboolean
exchange_summary_expunge_mail (CamelFolder *folder,
                               CamelMessageInfo *info,
                               GError **error)
{
	GPtrArray *uids = g_ptr_array_new ();
	CamelStore *parent_store;
	gchar *uid = g_strdup (info->uid);
	const gchar *full_name;
	gboolean success;

	full_name = camel_folder_get_full_name (folder);
	parent_store = camel_folder_get_parent_store (folder);

	g_ptr_array_add (uids, uid);

	success = camel_exchange_utils_expunge_uids (
		CAMEL_SERVICE (parent_store), full_name, uids, error);

	g_ptr_array_free (uids, TRUE);

	return success;
}

static gint
exchange_summary_header_load (CamelFolderSummary *summary,
                              FILE *in)
{
	CamelExchangeSummary *exchange = (CamelExchangeSummary *) summary;
	CamelFolderSummaryClass *folder_summary_class;
	guint32 version, readonly, high_article_num = 0;

	folder_summary_class = CAMEL_FOLDER_SUMMARY_CLASS (
		camel_exchange_summary_parent_class);

	if (folder_summary_class->summary_header_load (summary, in) == -1)
		return -1;

	if (camel_file_util_decode_uint32 (in, &version) == -1)
		return -1;

	if (camel_file_util_decode_uint32 (in, &readonly) == -1)
		return -1;

	/* Old summary file - We need to migrate.  Migration automagically happens when
	   camel_folder_summary_save is called
	*/
	if (camel_file_util_decode_uint32 (in, &high_article_num) == -1) {
		if (version > CAMEL_EXCHANGE_SUMMARY_VERSION)
			return -1;
	}

	/* During migration we will not have high_article_num stored in the summary and
	   essentially we will end up computing it atleast once.
	*/
	exchange->readonly = readonly;
	exchange->high_article_num = high_article_num;
	exchange->version = version;

	d(g_print ("%s:%s: high_article_num = [%d]\n", G_STRLOC, G_STRFUNC, high_article_num));

	return 0;
}

static gint
exchange_summary_header_save (CamelFolderSummary *summary,
                              FILE *out)
{
	CamelExchangeSummary *exchange = (CamelExchangeSummary *) summary;
	CamelFolderSummaryClass *folder_summary_class;

	folder_summary_class = CAMEL_FOLDER_SUMMARY_CLASS (
		camel_exchange_summary_parent_class);

	if (folder_summary_class->summary_header_save (summary, out) == -1)
		return -1;

	if (camel_file_util_encode_uint32 (out, exchange->version) == -1)
		return -1;

	if (camel_file_util_encode_uint32 (out, exchange->readonly) == -1)
		return -1;

	if (camel_file_util_encode_uint32 (out, exchange->high_article_num) == -1)
		return -1;

	d(g_print ("%s:%s: high_article_num = [%d]\n", G_STRLOC, G_STRFUNC, exchange->high_article_num));

	return 0;
}

static CamelMessageInfo *
exchange_summary_message_info_migrate (CamelFolderSummary *summary,
                                       FILE *in)
{
	CamelMessageInfo *info;
	CamelExchangeMessageInfo *einfo;
	CamelFolderSummaryClass *folder_summary_class;
	gchar *thread_index, *href = NULL;

	folder_summary_class = CAMEL_FOLDER_SUMMARY_CLASS (
		camel_exchange_summary_parent_class);

	info = folder_summary_class->message_info_migrate (summary, in);
	if (info) {
		einfo = (CamelExchangeMessageInfo *)info;

		if (camel_file_util_decode_string (in, &thread_index) == -1)
			goto error;

		if (thread_index && *thread_index)
			einfo->thread_index = thread_index;
		else
			g_free (thread_index);

		/* Old summary file - We need to migrate.  Migration automagically happens when
		   camel_folder_summary_save is called
		*/
		if (camel_file_util_decode_string (in, &href) == -1) {
			if (CAMEL_EXCHANGE_SUMMARY (summary)->version > CAMEL_EXCHANGE_SUMMARY_VERSION)
				goto error;
		}

		einfo->href = href;
		d(g_print ("%s:%s: einfo->href = [%s]\n", G_STRLOC, G_STRFUNC, einfo->href));
	}

	return info;
error:
	camel_message_info_free (info);
	return NULL;
}

static CamelMessageInfo *
exchange_summary_message_info_new_from_header (CamelFolderSummary *summary,
                                               struct _camel_header_raw *h)
{
	CamelMessageInfo *info;
	CamelExchangeMessageInfo *einfo;
	CamelFolderSummaryClass *folder_summary_class;
	const gchar *thread_index;

	folder_summary_class = CAMEL_FOLDER_SUMMARY_CLASS (
		camel_exchange_summary_parent_class);

	info = folder_summary_class->message_info_new_from_header (summary, h);
	if (!info)
		return info;

	einfo = (CamelExchangeMessageInfo *)info;
	thread_index = camel_header_raw_find (&h, "Thread-Index", NULL);
	if (thread_index)
		einfo->thread_index = g_strdup (thread_index + 1);

	return info;
}

static void
exchange_summary_message_info_free (CamelFolderSummary *summary,
                                    CamelMessageInfo *info)
{
	CamelExchangeMessageInfo *einfo;
	CamelFolderSummaryClass *folder_summary_class;

	folder_summary_class = CAMEL_FOLDER_SUMMARY_CLASS (
		camel_exchange_summary_parent_class);

	einfo = (CamelExchangeMessageInfo *)info;

	g_free (einfo->href);
	g_free (einfo->thread_index);

	einfo->href = NULL;
	einfo->thread_index = NULL;

	folder_summary_class->message_info_free (summary, info);
}

static CamelFIRecord *
exchange_summary_summary_header_to_db (CamelFolderSummary *s,
                                       GError **error)
{
	CamelExchangeSummary *exchange = (CamelExchangeSummary *) s;
	CamelFolderSummaryClass *folder_summary_class;
	struct _CamelFIRecord *fir;

	folder_summary_class = CAMEL_FOLDER_SUMMARY_CLASS (
		camel_exchange_summary_parent_class);

	fir = folder_summary_class->summary_header_to_db (s, error);
	if (!fir)
		return NULL;
	fir->bdata = g_strdup_printf ("%u %u %u", exchange->version, exchange->readonly, exchange->high_article_num);

	return fir;
}

static gint
exchange_summary_summary_header_from_db (CamelFolderSummary *s,
                                         CamelFIRecord *mir)
{
	CamelExchangeSummary *exchange = (CamelExchangeSummary *) s;
	CamelFolderSummaryClass *folder_summary_class;
	gchar *part;

	folder_summary_class = CAMEL_FOLDER_SUMMARY_CLASS (
		camel_exchange_summary_parent_class);

	if (folder_summary_class->summary_header_from_db (s, mir) == -1)
		return -1;

	part = mir->bdata;

	if (part) {
		EXTRACT_FIRST_DIGIT (exchange->version)
	}

	if (part) {
		EXTRACT_DIGIT (exchange->readonly)
	}

	if (part) {
		EXTRACT_DIGIT (exchange->high_article_num)
	}

	return 0;
}

static CamelMIRecord *
exchange_summary_message_info_to_db (CamelFolderSummary *s,
                                     CamelMessageInfo *info)
{
	CamelExchangeMessageInfo *einfo = (CamelExchangeMessageInfo *)info;
	CamelFolderSummaryClass *folder_summary_class;
	struct _CamelMIRecord *mir;

	folder_summary_class = CAMEL_FOLDER_SUMMARY_CLASS (
		camel_exchange_summary_parent_class);

	mir = folder_summary_class->message_info_to_db (s, info);
	if (mir)
		mir->bdata = g_strdup_printf ("%d-%s %d-%s", einfo->thread_index ? (gint)strlen(einfo->thread_index):0 , einfo->thread_index ? einfo->thread_index : "", einfo->href ? (gint)strlen(einfo->href):0, einfo->href ? einfo->href:"");

	return mir;
}

static CamelMessageInfo *
exchange_summary_message_info_from_db (CamelFolderSummary *s,
                                       CamelMIRecord *mir)
{
	CamelMessageInfo *info;
	CamelExchangeMessageInfo *einfo;
	CamelFolderSummaryClass *folder_summary_class;

	folder_summary_class = CAMEL_FOLDER_SUMMARY_CLASS (
		camel_exchange_summary_parent_class);

	info = folder_summary_class->message_info_from_db (s, mir);
	if (info) {
		gchar *part = mir->bdata;
		gint len;
		einfo = (CamelExchangeMessageInfo *)info;
		EXTRACT_FIRST_STRING (einfo->thread_index)
		EXTRACT_FIRST_STRING (einfo->href)
	}

	return info;
}

static gboolean
exchange_summary_info_set_flags (CamelMessageInfo *info,
                                 guint32 flags,
                                 guint32 set)
{
	CamelFolder *folder;
	CamelStore *parent_store;
	CamelOfflineStore *offline_store;
	CamelFolderSummaryClass *folder_summary_class;
	const gchar *full_name;

	if (CAMEL_EXCHANGE_SUMMARY (info->summary)->readonly)
		return FALSE;

	folder = info->summary->folder;
	full_name = camel_folder_get_full_name (folder);
	parent_store = camel_folder_get_parent_store (folder);

	offline_store = CAMEL_OFFLINE_STORE (parent_store);

	folder_summary_class = CAMEL_FOLDER_SUMMARY_CLASS (
		camel_exchange_summary_parent_class);

	if (offline_store->state != CAMEL_OFFLINE_STORE_NETWORK_UNAVAIL) {
		if (folder && info->uid) {
			if ((flags & set & CAMEL_MESSAGE_DELETED) &&
			    exchange_summary_check_for_trash (folder)) {
				return exchange_summary_expunge_mail (folder, info, NULL);
			} else {
				camel_exchange_utils_set_message_flags (
					CAMEL_SERVICE (parent_store),
					full_name, info->uid, set, flags, NULL);
				return folder_summary_class->info_set_flags(info, flags, set);
			}
		}
	}
	else {
		if (folder && info->uid) {
			if ((flags & set & CAMEL_MESSAGE_DELETED) &&
			    exchange_summary_check_for_trash (folder)) {
				/* FIXME: should add a separate journal entry for this case. */ ;
			} else {
				CamelExchangeFolder *exchange_folder = (CamelExchangeFolder *) folder;
				CamelExchangeJournal *journal = (CamelExchangeJournal *) exchange_folder->journal;
				camel_exchange_journal_delete (journal, info->uid, flags, set, NULL);
				return folder_summary_class->info_set_flags(info, flags, set);
			}
		}
	}
	return FALSE;
}

static gboolean
exchange_summary_info_set_user_tag (CamelMessageInfo *info,
                                    const gchar *name,
                                    const gchar *value)
{
	CamelFolderSummaryClass *folder_summary_class;
	gint res;

	if (CAMEL_EXCHANGE_SUMMARY (info->summary)->readonly)
		return FALSE;

	folder_summary_class = CAMEL_FOLDER_SUMMARY_CLASS (
		camel_exchange_summary_parent_class);

	res = folder_summary_class->info_set_user_tag(info, name, value);
	if (res && info->summary->folder && info->uid) {
		CamelFolder *folder = info->summary->folder;
		CamelStore *parent_store;
		const gchar *full_name;

		folder = info->summary->folder;
		full_name = camel_folder_get_full_name (folder);
		parent_store = camel_folder_get_parent_store (folder);

		camel_exchange_utils_set_message_tag (
			CAMEL_SERVICE (parent_store),
			full_name, info->uid, name, value, NULL);
	}

	return res;
}

static void
camel_exchange_summary_class_init (CamelExchangeSummaryClass *class)
{
	CamelFolderSummaryClass *folder_summary_class;

	folder_summary_class = CAMEL_FOLDER_SUMMARY_CLASS (class);
	folder_summary_class->message_info_size = sizeof (CamelExchangeMessageInfo);
	folder_summary_class->content_info_size = sizeof (CamelMessageContentInfo);
	folder_summary_class->summary_header_load = exchange_summary_header_load;
	folder_summary_class->summary_header_save = exchange_summary_header_save;
	folder_summary_class->message_info_migrate = exchange_summary_message_info_migrate;
	folder_summary_class->message_info_new_from_header = exchange_summary_message_info_new_from_header;
	folder_summary_class->message_info_free = exchange_summary_message_info_free;
	folder_summary_class->summary_header_to_db = exchange_summary_summary_header_to_db;
	folder_summary_class->summary_header_from_db = exchange_summary_summary_header_from_db;
	folder_summary_class->message_info_to_db = exchange_summary_message_info_to_db;
	folder_summary_class->message_info_from_db = exchange_summary_message_info_from_db;
	folder_summary_class->info_set_flags = exchange_summary_info_set_flags;
	folder_summary_class->info_set_user_tag = exchange_summary_info_set_user_tag;
}

static void
camel_exchange_summary_init (CamelExchangeSummary *summary)
{
}

/**
 * camel_exchange_summary_new:
 * @filename: filename to use for the summary
 *
 * Creates a new #CamelExchangeSummary based on @filename.
 *
 * Return value: the summary object.
 **/
CamelFolderSummary *
camel_exchange_summary_new (struct _CamelFolder *folder, const gchar *filename)
{
	CamelFolderSummary *summary;
	GError *local_error = NULL;

	summary = g_object_new (CAMEL_TYPE_EXCHANGE_SUMMARY, NULL);
	summary->folder = folder;
	camel_folder_summary_set_filename (summary, filename);
	if (camel_folder_summary_load_from_db (summary, &local_error) == -1) {
		g_warning (
			"Unable to load Exchage summary for folder %s: %s\n",
			camel_folder_get_full_name (folder),
			local_error->message);
		camel_folder_summary_clear_db (summary);
		camel_folder_summary_touch (summary);
		g_error_free (local_error);
	}

	return summary;
}

/**
 * camel_exchange_summary_get_readonly:
 * @summary: the summary
 *
 * Tests if the folder represented by @summary is read-only.
 *
 * Return value: %TRUE or %FALSE
 **/
gboolean
camel_exchange_summary_get_readonly (CamelFolderSummary *summary)
{
	g_return_val_if_fail (CAMEL_IS_EXCHANGE_SUMMARY (summary), FALSE);

	return CAMEL_EXCHANGE_SUMMARY (summary)->readonly;
}

/**
 * camel_exchange_summary_set_readonly:
 * @summary: the summary
 * @readonly: the read-only state of @summary
 *
 * Sets @summary's read-only state to @readonly. (This means that
 * @summary is the summary for a read-only folder, not necessarily
 * that the file itself is read-only.)
 **/
void
camel_exchange_summary_set_readonly (CamelFolderSummary *summary,
				     gboolean readonly)
{
	CamelExchangeSummary *es;

	g_return_if_fail (CAMEL_IS_EXCHANGE_SUMMARY (summary));

	es = CAMEL_EXCHANGE_SUMMARY (summary);
	if (es->readonly != readonly)
		camel_folder_summary_touch (summary);
	es->readonly = readonly;
}

/**
 * camel_exchange_summary_get_article_num:
 * @summary: the summary
 *
 * Returns the highest article number of a message present in the folder represented by @summary.
 *
 * Return value: Highest article number for a message present in the folder.
 **/
guint32
camel_exchange_summary_get_article_num (CamelFolderSummary *summary)
{
	g_return_val_if_fail (CAMEL_IS_EXCHANGE_SUMMARY (summary), FALSE);

	return CAMEL_EXCHANGE_SUMMARY (summary)->high_article_num;
}

/**
 * camel_exchange_summary_set_article_num:
 * @summary: the summary
 * @article_num: Highest article number of a message present in the folder.
 *
 * Sets @summary's high-article-number to @article_num.
 **/
void
camel_exchange_summary_set_article_num (CamelFolderSummary *summary,
					guint32 article_num)
{
	CamelExchangeSummary *es;

	g_return_if_fail (CAMEL_IS_EXCHANGE_SUMMARY (summary));

	es = CAMEL_EXCHANGE_SUMMARY (summary);
	if (!es->readonly)
		camel_folder_summary_touch (summary);
	es->high_article_num = article_num;
}

/**
 * camel_exchange_summary_add_offline:
 * @summary: the summary
 * @uid: the UID of the new message
 * @message: the new message
 * @info: the message info
 *
 * Adds a new entry to @summary with UID @uid, corresponding to
 * @message and @info.
 **/
void
camel_exchange_summary_add_offline (CamelFolderSummary *summary,
				    const gchar *uid,
				    CamelMimeMessage *message,
				    CamelMessageInfo *info)
{
	CamelMessageInfoBase *mi;
	const CamelFlag *flag;
	const CamelTag *tag;

	/* Create summary entry */
	mi = (CamelMessageInfoBase *)camel_folder_summary_info_new_from_message (summary, message, NULL);

	/* Copy flags 'n' tags */
	mi->flags = camel_message_info_flags(info);

	flag = camel_message_info_user_flags(info);
	while (flag) {
		camel_message_info_set_user_flag((CamelMessageInfo *)mi, flag->name, TRUE);
		flag = flag->next;
	}
	tag = camel_message_info_user_tags(info);
	while (tag) {
		camel_message_info_set_user_tag((CamelMessageInfo *)mi, tag->name, tag->value);
		tag = tag->next;
	}

	mi->size = camel_message_info_size(info);
	mi->uid = camel_pstring_strdup (uid);
	camel_folder_summary_add (summary, (CamelMessageInfo *)mi);
}

/**
 * camel_exchange_summary_add_offline_uncached:
 * @summary: the summary
 * @uid: the UID of the new message
 * @info: the message info
 *
 * Adds a new entry to @summary with UID @uid, corresponding to
 * @info.
 **/
void
camel_exchange_summary_add_offline_uncached (CamelFolderSummary *summary,
					     const gchar *uid,
					     CamelMessageInfo *info)
{
	CamelMessageInfo *mi;

	/* Create summary entry */
	mi = camel_message_info_clone(info);

	/* Set uid and add to summary */
	mi->uid = camel_pstring_strdup (uid);
	camel_folder_summary_add (summary, mi);
}
