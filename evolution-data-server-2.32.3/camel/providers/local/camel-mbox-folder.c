/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; fill-column: 160 -*-
 *
 * Authors: Michael Zucchi <notzed@ximian.com>
 *          Jeffrey Stedfast <fejj@ximian.com>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <inttypes.h>

#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include "camel-mbox-folder.h"
#include "camel-mbox-store.h"
#include "camel-mbox-summary.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

#define d(x) /*(printf("%s(%d): ", __FILE__, __LINE__),(x))*/

static gint mbox_lock(CamelLocalFolder *lf, CamelLockType type, GError **error);
static void mbox_unlock(CamelLocalFolder *lf);

static gboolean mbox_append_message(CamelFolder *folder, CamelMimeMessage * message, const CamelMessageInfo * info,	gchar **appended_uid, GError **error);
static CamelMimeMessage *mbox_get_message(CamelFolder *folder, const gchar * uid, GError **error);
static CamelLocalSummary *mbox_create_summary(CamelLocalFolder *lf, const gchar *path, const gchar *folder, CamelIndex *index);
static gchar * mbox_get_filename (CamelFolder *folder, const gchar *uid, GError **error);
static gint mbox_cmp_uids (CamelFolder *folder, const gchar *uid1, const gchar *uid2);
static void mbox_sort_uids (CamelFolder *folder, GPtrArray *uids);

G_DEFINE_TYPE (CamelMboxFolder, camel_mbox_folder, CAMEL_TYPE_LOCAL_FOLDER)

static void
camel_mbox_folder_class_init (CamelMboxFolderClass *class)
{
	CamelFolderClass *folder_class;
	CamelLocalFolderClass *local_folder_class;

	folder_class = CAMEL_FOLDER_CLASS (class);
	folder_class->append_message = mbox_append_message;
	folder_class->get_message = mbox_get_message;
	folder_class->get_filename = mbox_get_filename;
	folder_class->cmp_uids = mbox_cmp_uids;
	folder_class->sort_uids = mbox_sort_uids;

	local_folder_class = CAMEL_LOCAL_FOLDER_CLASS (class);
	local_folder_class->create_summary = mbox_create_summary;
	local_folder_class->lock = mbox_lock;
	local_folder_class->unlock = mbox_unlock;
}

static void
camel_mbox_folder_init (CamelMboxFolder *mbox_folder)
{
	mbox_folder->lockfd = -1;
}

CamelFolder *
camel_mbox_folder_new(CamelStore *parent_store, const gchar *full_name, guint32 flags, GError **error)
{
	CamelFolder *folder;
	gchar *basename;

	basename = g_path_get_basename (full_name);

	folder = g_object_new (
		CAMEL_TYPE_MBOX_FOLDER,
		"name", basename, "full-name", full_name,
		"parent-store", parent_store, NULL);
	folder = (CamelFolder *)camel_local_folder_construct (
		(CamelLocalFolder *)folder, flags, error);

	g_free (basename);

	return folder;
}

static CamelLocalSummary *mbox_create_summary(CamelLocalFolder *lf, const gchar *path, const gchar *folder, CamelIndex *index)
{
	return (CamelLocalSummary *)camel_mbox_summary_new((CamelFolder *)lf, path, folder, index);
}

static gint mbox_lock(CamelLocalFolder *lf, CamelLockType type, GError **error)
{
#ifndef G_OS_WIN32
	CamelMboxFolder *mf = (CamelMboxFolder *)lf;

	/* make sure we have matching unlocks for locks, camel-local-folder class should enforce this */
	g_assert(mf->lockfd == -1);

	mf->lockfd = open(lf->folder_path, O_RDWR|O_LARGEFILE, 0);
	if (mf->lockfd == -1) {
		g_set_error (
			error, G_IO_ERROR,
			g_io_error_from_errno (errno),
			_("Cannot create folder lock on %s: %s"),
			lf->folder_path, g_strerror (errno));
		return -1;
	}

	if (camel_lock_folder(lf->folder_path, mf->lockfd, type, error) == -1) {
		close(mf->lockfd);
		mf->lockfd = -1;
		return -1;
	}
#endif
	return 0;
}

static void mbox_unlock(CamelLocalFolder *lf)
{
#ifndef G_OS_WIN32
	CamelMboxFolder *mf = (CamelMboxFolder *)lf;

	g_assert(mf->lockfd != -1);
	camel_unlock_folder(lf->folder_path, mf->lockfd);
	close(mf->lockfd);
	mf->lockfd = -1;
#endif
}

static gboolean
mbox_append_message (CamelFolder *folder,
                     CamelMimeMessage *message,
                     const CamelMessageInfo *info,
                     gchar **appended_uid,
                     GError **error)
{
	CamelLocalFolder *lf = (CamelLocalFolder *)folder;
	CamelStream *output_stream = NULL, *filter_stream = NULL;
	CamelMimeFilter *filter_from;
	CamelMboxSummary *mbs = (CamelMboxSummary *)folder->summary;
	CamelMessageInfo *mi;
	gchar *fromline = NULL;
	struct stat st;
	gint retval;
#if 0
	gchar *xev;
#endif
	/* If we can't lock, dont do anything */
	if (camel_local_folder_lock(lf, CAMEL_LOCK_WRITE, error) == -1)
		return FALSE;

	d(printf("Appending message\n"));

	/* first, check the summary is correct (updates folder_size too) */
	retval = camel_local_summary_check ((CamelLocalSummary *)folder->summary, lf->changes, error);
	if (retval == -1)
		goto fail;

	/* add it to the summary/assign the uid, etc */
	mi = camel_local_summary_add((CamelLocalSummary *)folder->summary, message, info, lf->changes, error);
	if (mi == NULL)
		goto fail;

	d(printf("Appending message: uid is %s\n", camel_message_info_uid(mi)));

	if ((camel_message_info_flags (mi) & CAMEL_MESSAGE_ATTACHMENTS) && !camel_mime_message_has_attachment (message))
		camel_message_info_set_flags (mi, CAMEL_MESSAGE_ATTACHMENTS, 0);

	output_stream = camel_stream_fs_new_with_name (
		lf->folder_path, O_WRONLY | O_APPEND |
		O_LARGEFILE, 0666, error);
	if (output_stream == NULL) {
		g_prefix_error (
			error, _("Cannot open mailbox: %s: "),
			lf->folder_path);
		goto fail;
	}

	/* and we need to set the frompos/XEV explicitly */
	((CamelMboxMessageInfo *)mi)->frompos = mbs->folder_size;
#if 0
	xev = camel_local_summary_encode_x_evolution((CamelLocalSummary *)folder->summary, mi);
	if (xev) {
		/* the x-ev header should match the 'current' flags, no problem, so store as much */
		camel_medium_set_header((CamelMedium *)message, "X-Evolution", xev);
		mi->flags &= ~ CAMEL_MESSAGE_FOLDER_NOXEV|CAMEL_MESSAGE_FOLDER_FLAGGED;
		g_free(xev);
	}
#endif

	/* we must write this to the non-filtered stream ... */
	fromline = camel_mime_message_build_mbox_from(message);
	if (camel_stream_write(output_stream, fromline, strlen(fromline), error) == -1)
		goto fail_write;

	/* and write the content to the filtering stream, that translates '\nFrom' into '\n>From' */
	filter_stream = camel_stream_filter_new (output_stream);
	filter_from = camel_mime_filter_from_new();
	camel_stream_filter_add((CamelStreamFilter *) filter_stream, filter_from);
	g_object_unref (filter_from);

	if (camel_data_wrapper_write_to_stream (
		(CamelDataWrapper *) message, filter_stream, error) == -1 ||
	    camel_stream_write (filter_stream, "\n", 1, error) == -1 ||
	    camel_stream_flush (filter_stream, error) == -1)
		goto fail_write;

	/* filter stream ref's the output stream itself, so we need to unref it too */
	g_object_unref (filter_stream);
	g_object_unref (output_stream);
	g_free(fromline);

	if (!((CamelMessageInfoBase *)mi)->preview && camel_folder_summary_get_need_preview(folder->summary)) {
		if (camel_mime_message_build_preview ((CamelMimePart *)message, mi) && ((CamelMessageInfoBase *)mi)->preview)
			camel_folder_summary_add_preview (folder->summary, mi);
	}

	/* now we 'fudge' the summary  to tell it its uptodate, because its idea of uptodate has just changed */
	/* the stat really shouldn't fail, we just wrote to it */
	if (g_stat (lf->folder_path, &st) == 0) {
		((CamelFolderSummary *) mbs)->time = st.st_mtime;
		mbs->folder_size = st.st_size;
	}

	/* unlock as soon as we can */
	camel_local_folder_unlock(lf);

	if (camel_folder_change_info_changed(lf->changes)) {
		camel_folder_changed (folder, lf->changes);
		camel_folder_change_info_clear(lf->changes);
	}

	if (appended_uid)
		*appended_uid = g_strdup(camel_message_info_uid(mi));

	return TRUE;

fail_write:
	g_prefix_error (
		error, _("Cannot append message to mbox file: %s: "),
		lf->folder_path);

	if (output_stream) {
		gint fd;

		fd = camel_stream_fs_get_fd (CAMEL_STREAM_FS (output_stream));

		/* reset the file to original size */
		do {
			retval = ftruncate (fd, mbs->folder_size);
		} while (retval == -1 && errno == EINTR);

		g_object_unref (output_stream);
	}

	if (filter_stream)
		g_object_unref (filter_stream);

	g_free(fromline);

	/* remove the summary info so we are not out-of-sync with the mbox */
	camel_folder_summary_remove_uid (CAMEL_FOLDER_SUMMARY (mbs), camel_message_info_uid (mi));

	/* and tell the summary it's up-to-date */
	if (g_stat (lf->folder_path, &st) == 0) {
		((CamelFolderSummary *) mbs)->time = st.st_mtime;
		mbs->folder_size = st.st_size;
	}

fail:
	/* make sure we unlock the folder - before we start triggering events into appland */
	camel_local_folder_unlock(lf);

	/* cascade the changes through, anyway, if there are any outstanding */
	if (camel_folder_change_info_changed(lf->changes)) {
		camel_folder_changed (folder, lf->changes);
		camel_folder_change_info_clear(lf->changes);
	}

	return FALSE;
}

static gchar *
mbox_get_filename (CamelFolder *folder, const gchar *uid, GError **error)
{
	CamelLocalFolder *lf = (CamelLocalFolder *)folder;
	CamelMboxMessageInfo *info;
	goffset frompos;
	gchar *filename = NULL;

	d(printf("Getting message %s\n", uid));

	/* lock the folder first, burn if we can't, need write lock for summary check */
	if (camel_local_folder_lock(lf, CAMEL_LOCK_WRITE, error) == -1)
		return NULL;

	/* check for new messages always */
	if (camel_local_summary_check((CamelLocalSummary *)folder->summary, lf->changes, error) == -1) {
		camel_local_folder_unlock(lf);
		return NULL;
	}

	/* get the message summary info */
	info = (CamelMboxMessageInfo *) camel_folder_summary_uid(folder->summary, uid);

	if (info == NULL) {
		set_cannot_get_message_ex (
			error, CAMEL_FOLDER_ERROR_INVALID_UID,
			uid, lf->folder_path, _("No such message"));
		goto fail;
	}

	if (info->frompos == -1) {
		camel_message_info_free((CamelMessageInfo *)info);
		goto fail;
	}

	frompos = info->frompos;
	camel_message_info_free((CamelMessageInfo *)info);

	filename = g_strdup_printf ("%s%s!%" PRId64, lf->folder_path, G_DIR_SEPARATOR_S, (gint64) frompos);

fail:
	/* and unlock now we're finished with it */
	camel_local_folder_unlock(lf);

	return filename;
}

static CamelMimeMessage *
mbox_get_message(CamelFolder *folder, const gchar * uid, GError **error)
{
	CamelLocalFolder *lf = (CamelLocalFolder *)folder;
	CamelMimeMessage *message = NULL;
	CamelMboxMessageInfo *info;
	CamelMimeParser *parser = NULL;
	gint fd, retval;
	gint retried = FALSE;
	goffset frompos;

	d(printf("Getting message %s\n", uid));

	/* lock the folder first, burn if we can't, need write lock for summary check */
	if (camel_local_folder_lock(lf, CAMEL_LOCK_WRITE, error) == -1)
		return NULL;

	/* check for new messages always */
	if (camel_local_summary_check((CamelLocalSummary *)folder->summary, lf->changes, error) == -1) {
		camel_local_folder_unlock(lf);
		return NULL;
	}

retry:
	/* get the message summary info */
	info = (CamelMboxMessageInfo *) camel_folder_summary_uid(folder->summary, uid);

	if (info == NULL) {
		set_cannot_get_message_ex (
			error, CAMEL_FOLDER_ERROR_INVALID_UID,
			uid, lf->folder_path, _("No such message"));
		goto fail;
	}

	if (info->frompos == -1) {
		camel_message_info_free((CamelMessageInfo *)info);
		goto fail;
	}

	frompos = info->frompos;
	camel_message_info_free((CamelMessageInfo *)info);

	/* we use an fd instead of a normal stream here - the reason is subtle, camel_mime_part will cache
	   the whole message in memory if the stream is non-seekable (which it is when built from a parser
	   with no stream).  This means we dont have to lock the mbox for the life of the message, but only
	   while it is being created. */

	fd = g_open(lf->folder_path, O_LARGEFILE | O_RDONLY | O_BINARY, 0);
	if (fd == -1) {
		set_cannot_get_message_ex (
			error, CAMEL_ERROR_GENERIC,
			uid, lf->folder_path, g_strerror (errno));
		goto fail;
	}

	/* we use a parser to verify the message is correct, and in the correct position */
	parser = camel_mime_parser_new();
	camel_mime_parser_init_with_fd(parser, fd);
	camel_mime_parser_scan_from(parser, TRUE);

	camel_mime_parser_seek(parser, frompos, SEEK_SET);
	if (camel_mime_parser_step(parser, NULL, NULL) != CAMEL_MIME_PARSER_STATE_FROM
	    || camel_mime_parser_tell_start_from(parser) != frompos) {

		g_warning("Summary doesn't match the folder contents!  eek!\n"
			  "  expecting offset %ld got %ld, state = %d", (glong)frompos,
			  (glong)camel_mime_parser_tell_start_from(parser),
			  camel_mime_parser_state(parser));

		g_object_unref (parser);
		parser = NULL;

		if (!retried) {
			retried = TRUE;
			camel_local_summary_check_force((CamelLocalSummary *)folder->summary);
			retval = camel_local_summary_check((CamelLocalSummary *)folder->summary, lf->changes, error);
			if (retval != -1)
				goto retry;
		}

		set_cannot_get_message_ex (
			error, CAMEL_FOLDER_ERROR_INVALID,
			uid, lf->folder_path,
			_("The folder appears to be irrecoverably corrupted."));
		goto fail;
	}

	message = camel_mime_message_new();
	if (camel_mime_part_construct_from_parser((CamelMimePart *)message, parser, error) == -1) {
		g_prefix_error (
			error, _("Cannot get message %s from folder %s: "),
			uid, lf->folder_path);
		g_object_unref (message);
		message = NULL;
		goto fail;
	}

	camel_medium_remove_header((CamelMedium *)message, "X-Evolution");

fail:
	/* and unlock now we're finished with it */
	camel_local_folder_unlock(lf);

	if (parser)
		g_object_unref (parser);

	/* use the opportunity to notify of changes (particularly if we had a rebuild) */
	if (camel_folder_change_info_changed(lf->changes)) {
		camel_folder_changed (folder, lf->changes);
		camel_folder_change_info_clear(lf->changes);
	}

	return message;
}

static gint
mbox_cmp_uids (CamelFolder *folder, const gchar *uid1, const gchar *uid2)
{
	CamelMboxMessageInfo *a, *b;
	gint res;

	g_return_val_if_fail (folder != NULL, 0);
	g_return_val_if_fail (folder->summary != NULL, 0);

	a = (CamelMboxMessageInfo *) camel_folder_summary_uid (folder->summary, uid1);
	b = (CamelMboxMessageInfo *) camel_folder_summary_uid (folder->summary, uid2);

	g_return_val_if_fail (a != NULL, 0);
	g_return_val_if_fail (b != NULL, 0);

	res = a->frompos < b->frompos ? -1 : a->frompos == b->frompos ? 0 : 1;

	camel_message_info_free ((CamelMessageInfo *)a);
	camel_message_info_free ((CamelMessageInfo *)b);

	return res;
}

static void
mbox_sort_uids (CamelFolder *folder, GPtrArray *uids)
{
	g_return_if_fail (camel_mbox_folder_parent_class != NULL);
	g_return_if_fail (folder != NULL);

	if (uids && uids->len > 1)
		camel_folder_summary_prepare_fetch_all (folder->summary, NULL);

	CAMEL_FOLDER_CLASS (camel_mbox_folder_parent_class)->sort_uids (folder, uids);
}
