/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 *  Authors:
 *	parthasrathi susarla <sparthasrathi@novell.com>
 * Based on the IMAP summary class implementation by:
 *    Michael Zucchi <notzed@ximian.com>
 *    Dan Winship <danw@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "camel-groupwise-folder.h"
#include "camel-groupwise-summary.h"

#define CAMEL_GW_SUMMARY_VERSION (1)

#define EXTRACT_FIRST_DIGIT(val) part ? val=strtoul (part, &part, 10) : 0;
#define EXTRACT_DIGIT(val) part++; part ? val=strtoul (part, &part, 10) : 0;

#define d(x)

/*Prototypes*/
static gint gw_summary_header_load (CamelFolderSummary *, FILE *);
static gint gw_summary_header_save (CamelFolderSummary *, FILE *);

static CamelMessageInfo *gw_message_info_migrate (CamelFolderSummary *s, FILE *in);

static CamelMessageContentInfo * gw_content_info_migrate (CamelFolderSummary *s, FILE *in);
static gboolean gw_info_set_flags(CamelMessageInfo *info, guint32 flags, guint32 set);

static gint summary_header_from_db (CamelFolderSummary *s, CamelFIRecord *mir);
static CamelFIRecord * summary_header_to_db (CamelFolderSummary *s, GError **error);
static CamelMIRecord * message_info_to_db (CamelFolderSummary *s, CamelMessageInfo *info);
static CamelMessageInfo * message_info_from_db (CamelFolderSummary *s, CamelMIRecord *mir);
static gint content_info_to_db (CamelFolderSummary *s, CamelMessageContentInfo *info, CamelMIRecord *mir);
static CamelMessageContentInfo * content_info_from_db (CamelFolderSummary *s, CamelMIRecord *mir);

/*End of Prototypes*/

G_DEFINE_TYPE (CamelGroupwiseSummary, camel_groupwise_summary, CAMEL_TYPE_FOLDER_SUMMARY)

static CamelMessageInfo *
gw_message_info_clone(CamelFolderSummary *s, const CamelMessageInfo *mi)
{
	CamelGroupwiseMessageInfo *to;
	const CamelGroupwiseMessageInfo *from = (const CamelGroupwiseMessageInfo *)mi;

	to = (CamelGroupwiseMessageInfo *)CAMEL_FOLDER_SUMMARY_CLASS (camel_groupwise_summary_parent_class)->message_info_clone(s, mi);
	to->server_flags = from->server_flags;

	/* FIXME: parent clone should do this */
	to->info.content = camel_folder_summary_content_info_new(s);

	return (CamelMessageInfo *)to;
}

static void
camel_groupwise_summary_class_init (CamelGroupwiseSummaryClass *class)
{
	CamelFolderSummaryClass *folder_summary_class;

	folder_summary_class = CAMEL_FOLDER_SUMMARY_CLASS (class);
	folder_summary_class->message_info_size = sizeof (CamelGroupwiseMessageInfo);
	folder_summary_class->content_info_size = sizeof (CamelGroupwiseMessageContentInfo);
	folder_summary_class->message_info_clone = gw_message_info_clone;
	folder_summary_class->summary_header_load = gw_summary_header_load;
	folder_summary_class->summary_header_save = gw_summary_header_save;
	folder_summary_class->message_info_migrate = gw_message_info_migrate;
	folder_summary_class->content_info_migrate = gw_content_info_migrate;
	folder_summary_class->info_set_flags = gw_info_set_flags;
	folder_summary_class->summary_header_to_db = summary_header_to_db;
	folder_summary_class->summary_header_from_db = summary_header_from_db;
	folder_summary_class->message_info_to_db = message_info_to_db;
	folder_summary_class->message_info_from_db = message_info_from_db;
	folder_summary_class->content_info_to_db = content_info_to_db;
	folder_summary_class->content_info_from_db = content_info_from_db;
}

static void
camel_groupwise_summary_init (CamelGroupwiseSummary *gw_summary)
{
	CamelFolderSummary *summary = CAMEL_FOLDER_SUMMARY (gw_summary);

	/* Meta-summary - Overriding UID len */
	summary->meta_summary->uid_len = 2048;
}

/**
 * camel_groupwise_summary_new:
 * @filename: the file to store the summary in.
 *
 * This will create a new CamelGroupwiseSummary object and read in the
 * summary data from disk, if it exists.
 *
 * Returns: A new CamelGroupwiseSummary object.
 **/
CamelFolderSummary *
camel_groupwise_summary_new (struct _CamelFolder *folder, const gchar *filename)
{
	CamelFolderSummary *summary;

	summary = g_object_new (CAMEL_TYPE_GROUPWISE_SUMMARY, NULL);
	summary->folder = folder;
	camel_folder_summary_set_build_content (summary, TRUE);
	camel_folder_summary_set_filename (summary, filename);

	camel_folder_summary_load_from_db (summary, NULL);

	return summary;
}

static gint
summary_header_from_db (CamelFolderSummary *s, CamelFIRecord *mir)
{
	CamelGroupwiseSummary *gms = CAMEL_GROUPWISE_SUMMARY (s);
	gchar *part;

	if (CAMEL_FOLDER_SUMMARY_CLASS (camel_groupwise_summary_parent_class)->summary_header_from_db (s, mir) == -1)
		return -1;

	part = mir->bdata;

	if (part)
		EXTRACT_FIRST_DIGIT(gms->version);

	if (part)
		EXTRACT_DIGIT (gms->validity);

	if (part && part++) {
		gms->time_string = g_strdup (part);
	}

	return 0;
}

static gint
gw_summary_header_load (CamelFolderSummary *s, FILE *in)
{
	CamelGroupwiseSummary *gms = CAMEL_GROUPWISE_SUMMARY (s);

	if (CAMEL_FOLDER_SUMMARY_CLASS (camel_groupwise_summary_parent_class)->summary_header_load (s, in) == -1)
		return -1;

	if (camel_file_util_decode_fixed_int32(in, &gms->version) == -1
			|| camel_file_util_decode_fixed_int32(in, &gms->validity) == -1)
		return -1;

	if (camel_file_util_decode_string (in, &gms->time_string) == -1)
		return -1;
	return 0;
}

static CamelFIRecord *
summary_header_to_db (CamelFolderSummary *s, GError **error)
{
	CamelGroupwiseSummary *ims = CAMEL_GROUPWISE_SUMMARY(s);
	struct _CamelFIRecord *fir;

	fir = CAMEL_FOLDER_SUMMARY_CLASS (camel_groupwise_summary_parent_class)->summary_header_to_db (s, error);
	if (!fir)
		return NULL;

	fir->bdata = g_strdup_printf ("%d %d %s", CAMEL_GW_SUMMARY_VERSION, ims->validity, ims->time_string);

	return fir;

}

static gint
gw_summary_header_save (CamelFolderSummary *s, FILE *out)
{
	CamelGroupwiseSummary *gms = CAMEL_GROUPWISE_SUMMARY(s);

	if (CAMEL_FOLDER_SUMMARY_CLASS (camel_groupwise_summary_parent_class)->summary_header_save (s, out) == -1)
		return -1;

	camel_file_util_encode_fixed_int32(out, CAMEL_GW_SUMMARY_VERSION);
	camel_file_util_encode_fixed_int32(out, gms->validity);
	return camel_file_util_encode_string (out, gms->time_string);
}

static CamelMessageInfo *
message_info_from_db (CamelFolderSummary *s, CamelMIRecord *mir)
{
	CamelMessageInfo *info;
	CamelGroupwiseMessageInfo *iinfo;

	info = CAMEL_FOLDER_SUMMARY_CLASS (camel_groupwise_summary_parent_class)->message_info_from_db (s, mir);
	if (info) {
		gchar *part = mir->bdata;
		iinfo = (CamelGroupwiseMessageInfo *)info;
		EXTRACT_FIRST_DIGIT (iinfo->server_flags)
	}

	return info;}

static CamelMessageInfo *
gw_message_info_migrate (CamelFolderSummary *s, FILE *in)
{
	CamelMessageInfo *info;
	CamelGroupwiseMessageInfo *gw_info;

	info = CAMEL_FOLDER_SUMMARY_CLASS (camel_groupwise_summary_parent_class)->message_info_migrate (s,in);
	if (info) {
		gw_info = (CamelGroupwiseMessageInfo*) info;
		if (camel_file_util_decode_uint32 (in, &gw_info->server_flags) == -1)
			goto error;
	}

	return info;
error:
	camel_message_info_free (info);
	return NULL;
}

static CamelMIRecord *
message_info_to_db (CamelFolderSummary *s, CamelMessageInfo *info)
{
	CamelGroupwiseMessageInfo *iinfo = (CamelGroupwiseMessageInfo *)info;
	struct _CamelMIRecord *mir;

	mir = CAMEL_FOLDER_SUMMARY_CLASS (camel_groupwise_summary_parent_class)->message_info_to_db (s, info);
	if (mir)
		mir->bdata = g_strdup_printf ("%u", iinfo->server_flags);

	return mir;
}

static CamelMessageContentInfo *
content_info_from_db (CamelFolderSummary *s, CamelMIRecord *mir)
{
	gchar *part = mir->cinfo;
	guint32 type=0;

	if (part) {
		if (*part == ' ')
			part++;
		if (part) {
			EXTRACT_FIRST_DIGIT (type);
		}
	}
	mir->cinfo = part;
	if (type)
		return CAMEL_FOLDER_SUMMARY_CLASS (camel_groupwise_summary_parent_class)->content_info_from_db (s, mir);
	else
		return camel_folder_summary_content_info_new (s);
}

static CamelMessageContentInfo *
gw_content_info_migrate (CamelFolderSummary *s, FILE *in)
{
	if (fgetc (in))
		return CAMEL_FOLDER_SUMMARY_CLASS (camel_groupwise_summary_parent_class)->content_info_migrate (s, in);
	else
		return camel_folder_summary_content_info_new (s);
}

static gint
content_info_to_db (CamelFolderSummary *s, CamelMessageContentInfo *info, CamelMIRecord *mir)
{

	if (info->type) {
		mir->cinfo = g_strdup ("1");
		return CAMEL_FOLDER_SUMMARY_CLASS (camel_groupwise_summary_parent_class)->content_info_to_db (s, info, mir);
	} else {
		mir->cinfo = g_strdup ("0");
		return 0;
	}
}

static gboolean
gw_info_set_flags (CamelMessageInfo *info, guint32 flags, guint32 set)
{
		guint32 old;
		CamelMessageInfoBase *mi = (CamelMessageInfoBase *)info;
		gint read = 0 , deleted = 0;

		gint junk_flag = 0, junk_learn_flag = 0;

		/* TODO: locking? */

		if (flags & CAMEL_MESSAGE_SEEN && ((set & CAMEL_MESSAGE_SEEN) != (mi->flags & CAMEL_MESSAGE_SEEN)))
		{ read = set & CAMEL_MESSAGE_SEEN ? 1 : -1; d(printf("Setting read as %d\n", set & CAMEL_MESSAGE_SEEN ? 1 : 0));}

		if (flags & CAMEL_MESSAGE_DELETED && ((set & CAMEL_MESSAGE_DELETED) != (mi->flags & CAMEL_MESSAGE_DELETED)))
		{ deleted = set & CAMEL_MESSAGE_DELETED ? 1 : -1; d(printf("Setting deleted as %d\n", set & CAMEL_MESSAGE_DELETED ? 1 : 0));}

		old = mi->flags;
		mi->flags = (old & ~flags) | (set & flags);

		if (old != mi->flags) {
				mi->flags |= CAMEL_MESSAGE_FOLDER_FLAGGED;
				mi->dirty = TRUE;

				if (((old & ~CAMEL_MESSAGE_SYSTEM_MASK) == (mi->flags & ~CAMEL_MESSAGE_SYSTEM_MASK)) )
						return FALSE;

				if (mi->summary) {
						mi->summary->deleted_count += deleted;
						mi->summary->unread_count -= read;
						camel_folder_summary_touch(mi->summary);
				}
		}

		junk_flag = ((flags & CAMEL_MESSAGE_JUNK) && (set & CAMEL_MESSAGE_JUNK));
		junk_learn_flag = ((flags & CAMEL_MESSAGE_JUNK_LEARN) && (set & CAMEL_MESSAGE_JUNK_LEARN));

		/* This is a hack, we are using CAMEL_MESSAGE_JUNK justo to hide the item
		 * we make sure this doesn't have any side effects*/

		if (junk_learn_flag && !junk_flag  && (old & CAMEL_GW_MESSAGE_JUNK)) {
				/*
				   This has ugly side-effects. Evo will never learn unjunk.
				   We need to create one CAMEL_MESSAGE_HIDDEN flag which must be
				   used for all hiding operations. We must also get rid of the seperate file
				   that is maintained somewhere in evolution/mail/em-folder-browser.c for hidden messages
				 */
				mi->flags |= CAMEL_GW_MESSAGE_NOJUNK | CAMEL_MESSAGE_JUNK | CAMEL_MESSAGE_JUNK_LEARN;
		} else if (junk_learn_flag && junk_flag && !(old & CAMEL_GW_MESSAGE_JUNK)) {
				mi->flags |= CAMEL_GW_MESSAGE_JUNK | CAMEL_MESSAGE_JUNK | CAMEL_MESSAGE_JUNK_LEARN;
		}

		if (mi->summary && mi->summary->folder && mi->uid) {
				CamelFolderChangeInfo *changes = camel_folder_change_info_new();

				camel_folder_change_info_change_uid(changes, camel_message_info_uid(info));
				camel_folder_changed (mi->summary->folder, changes);
				camel_folder_change_info_free(changes);
				camel_folder_summary_touch(mi->summary);
		}

		return TRUE;
}

void
camel_gw_summary_add_offline (CamelFolderSummary *summary, const gchar *uid, CamelMimeMessage *message, const CamelMessageInfo *info)
{
	CamelGroupwiseMessageInfo *mi;
	const CamelFlag *flag;
	const CamelTag *tag;

	/* Create summary entry */
	mi = (CamelGroupwiseMessageInfo *)camel_folder_summary_info_new_from_message (summary, message, NULL);

	/* Copy flags 'n' tags */
	mi->info.flags = camel_message_info_flags(info);

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

	mi->info.size = camel_message_info_size(info);
	mi->info.uid = camel_pstring_strdup (uid);

	camel_folder_summary_add (summary, (CamelMessageInfo *)mi);

}

void
camel_gw_summary_add_offline_uncached (CamelFolderSummary *summary, const gchar *uid, const CamelMessageInfo *info)
{
	CamelGroupwiseMessageInfo *mi;

	mi = camel_message_info_clone(info);
	mi->info.uid = camel_pstring_strdup(uid);
	camel_folder_summary_add (summary, (CamelMessageInfo *)mi);
}

void
groupwise_summary_clear (CamelFolderSummary *summary, gboolean uncache)
{
	CamelFolderChangeInfo *changes;
	CamelMessageInfo *info;
	gint i, count;
	const gchar *uid;

	changes = camel_folder_change_info_new ();
	count = camel_folder_summary_count (summary);
	for (i = 0; i < count; i++) {
		if (!(info = camel_folder_summary_index (summary, i)))
			continue;

		uid = camel_message_info_uid (info);
		camel_folder_change_info_remove_uid (changes, uid);
		camel_folder_summary_remove_uid (summary, uid);
		camel_message_info_free(info);
	}

	camel_folder_summary_clear_db (summary);
	/*camel_folder_summary_save (summary);*/

	if (camel_folder_change_info_changed (changes))
		camel_folder_changed (summary->folder, changes);
	camel_folder_change_info_free (changes);
}

