/*
 *  Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 *  Authors: Not Zed <notzed@lostzed.mmc.com.au>
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

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <glib/gi18n-lib.h>

#include "camel-mh-summary.h"
#include "camel-local-private.h"

#define d(x) /*(printf("%s(%d): ", __FILE__, __LINE__),(x))*/

#define CAMEL_MH_SUMMARY_VERSION (0x2000)

#define CAMEL_MH_SUMMARY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), CAMEL_TYPE_MH_SUMMARY, CamelMhSummaryPrivate))

static gint mh_summary_check(CamelLocalSummary *cls, CamelFolderChangeInfo *changeinfo, GError **error);
static gint mh_summary_sync(CamelLocalSummary *cls, gboolean expunge, CamelFolderChangeInfo *changeinfo, GError **error);
/*static gint mh_summary_add(CamelLocalSummary *cls, CamelMimeMessage *msg, CamelMessageInfo *info, CamelFolderChangeInfo *, GError **error);*/

static gchar *mh_summary_next_uid_string(CamelFolderSummary *s);

struct _CamelMhSummaryPrivate {
	gchar *current_uid;
};

G_DEFINE_TYPE (CamelMhSummary, camel_mh_summary, CAMEL_TYPE_LOCAL_SUMMARY)

static void
camel_mh_summary_class_init (CamelMhSummaryClass *class)
{
	CamelFolderSummaryClass *folder_summary_class;
	CamelLocalSummaryClass *local_summary_class;

	g_type_class_add_private (class, sizeof (CamelMhSummaryPrivate));

	folder_summary_class = CAMEL_FOLDER_SUMMARY_CLASS (class);
	folder_summary_class->next_uid_string = mh_summary_next_uid_string;

	local_summary_class = CAMEL_LOCAL_SUMMARY_CLASS (class);
	local_summary_class->check = mh_summary_check;
	local_summary_class->sync = mh_summary_sync;
}

static void
camel_mh_summary_init (CamelMhSummary *mh_summary)
{
	CamelFolderSummary *folder_summary;

	mh_summary->priv = CAMEL_MH_SUMMARY_GET_PRIVATE (mh_summary);

	folder_summary = CAMEL_FOLDER_SUMMARY (mh_summary);

	/* set unique file version */
	folder_summary->version += CAMEL_MH_SUMMARY_VERSION;
}

/**
 * camel_mh_summary_new:
 *
 * Create a new CamelMhSummary object.
 *
 * Returns: A new #CamelMhSummary object.
 **/
CamelMhSummary *
camel_mh_summary_new (CamelFolder *folder,
                      const gchar *filename,
                      const gchar *mhdir,
                      CamelIndex *index)
{
	CamelMhSummary *o;

	o = g_object_new (CAMEL_TYPE_MH_SUMMARY, NULL);
	((CamelFolderSummary *)o)->folder = folder;
	if (folder) {
		CamelStore *parent_store;

		parent_store = camel_folder_get_parent_store (folder);
		camel_db_set_collate (parent_store->cdb_r, "uid", "mh_uid_sort", (CamelDBCollate)camel_local_frompos_sort);
		((CamelFolderSummary *)o)->sort_by = "uid";
		((CamelFolderSummary *)o)->collate = "mh_uid_sort";
	}

	camel_local_summary_construct((CamelLocalSummary *)o, filename, mhdir, index);
	return o;
}

static gchar *
mh_summary_next_uid_string (CamelFolderSummary *s)
{
	CamelMhSummary *mhs = (CamelMhSummary *)s;
	CamelLocalSummary *cls = (CamelLocalSummary *)s;
	gint fd = -1;
	guint32 uid;
	gchar *name;
	gchar *uidstr;

	/* if we are working to add an existing file, then use current_uid */
	if (mhs->priv->current_uid) {
		uidstr = g_strdup(mhs->priv->current_uid);
		/* tell the summary of this, so we always append numbers to the end */
		camel_folder_summary_set_uid(s, strtoul(uidstr, NULL, 10)+1);
	} else {
		/* else scan for one - and create it too, to make sure */
		do {
			if (fd != -1)
				close(fd);
			uid = camel_folder_summary_next_uid(s);
			name = g_strdup_printf("%s/%u", cls->folder_path, uid);
			/* O_EXCL isn't guaranteed, sigh.  Oh well, bad luck, mh has problems anyway */
			fd = open(name, O_WRONLY|O_CREAT|O_EXCL|O_LARGEFILE, 0600);
			g_free(name);
		} while (fd == -1 && errno == EEXIST);

		if (fd != -1)
			close(fd);

		uidstr = g_strdup_printf("%u", uid);
	}

	return uidstr;
}

static gint
camel_mh_summary_add (CamelLocalSummary *cls,
                      const gchar *name,
                      gint forceindex)
{
	CamelMhSummary *mhs = (CamelMhSummary *)cls;
	gchar *filename = g_strdup_printf("%s/%s", cls->folder_path, name);
	gint fd;
	CamelMimeParser *mp;

	d(printf("summarising: %s\n", name));

	fd = open(filename, O_RDONLY|O_LARGEFILE);
	if (fd == -1) {
		g_warning ("Cannot summarise/index: %s: %s", filename, g_strerror (errno));
		g_free(filename);
		return -1;
	}
	mp = camel_mime_parser_new();
	camel_mime_parser_scan_from(mp, FALSE);
	camel_mime_parser_init_with_fd(mp, fd);
	if (cls->index && (forceindex || !camel_index_has_name(cls->index, name))) {
		d(printf("forcing indexing of message content\n"));
		camel_folder_summary_set_index((CamelFolderSummary *)mhs, cls->index);
	} else {
		camel_folder_summary_set_index((CamelFolderSummary *)mhs, NULL);
	}
	mhs->priv->current_uid = (gchar *)name;
	camel_folder_summary_add_from_parser((CamelFolderSummary *)mhs, mp);
	g_object_unref (mp);
	mhs->priv->current_uid = NULL;
	camel_folder_summary_set_index((CamelFolderSummary *)mhs, NULL);
	g_free(filename);
	return 0;
}

static void
remove_summary (gchar *key,
                CamelMessageInfo *info,
                CamelLocalSummary *cls)
{
	d(printf("removing message %s from summary\n", key));
	if (cls->index)
		camel_index_delete_name(cls->index, camel_message_info_uid(info));
	camel_folder_summary_remove((CamelFolderSummary *)cls, info);
	camel_message_info_free(info);
}

static gint
mh_summary_check (CamelLocalSummary *cls,
                  CamelFolderChangeInfo *changeinfo,
                  GError **error)
{
	DIR *dir;
	struct dirent *d;
	gchar *p, c;
	CamelMessageInfo *info;
	CamelFolderSummary *s = (CamelFolderSummary *)cls;
	GHashTable *left;
	gint i, count;
	gint forceindex;

	/* FIXME: Handle changeinfo */

	d(printf("checking summary ...\n"));

	/* scan the directory, check for mail files not in the index, or index entries that
	   no longer exist */
	dir = opendir(cls->folder_path);
	if (dir == NULL) {
		g_set_error (
			error, G_IO_ERROR,
			g_io_error_from_errno (errno),
			_("Cannot open MH directory path: %s: %s"),
			cls->folder_path, g_strerror (errno));
		return -1;
	}

	/* keeps track of all uid's that have not been processed */
	left = g_hash_table_new(g_str_hash, g_str_equal);
	camel_folder_summary_prepare_fetch_all ((CamelFolderSummary *)cls, error);
	count = camel_folder_summary_count((CamelFolderSummary *)cls);
	forceindex = count == 0;
	for (i=0;i<count;i++) {
		info = camel_folder_summary_index((CamelFolderSummary *)cls, i);
		if (info) {
			g_hash_table_insert(left, (gchar *)camel_message_info_uid(info), info);
		}
	}

	while ((d = readdir(dir))) {
		/* FIXME: also run stat to check for regular file */
		p = d->d_name;
		while ((c = *p++)) {
			if (!isdigit(c))
				break;
		}
		if (c==0) {
			info = camel_folder_summary_uid((CamelFolderSummary *)cls, d->d_name);
			if (info == NULL || (cls->index && (!camel_index_has_name(cls->index, d->d_name)))) {
				/* need to add this file to the summary */
				if (info != NULL) {
					g_hash_table_remove(left, camel_message_info_uid(info));
					camel_folder_summary_remove((CamelFolderSummary *)cls, info);
					camel_message_info_free(info);
				}
				camel_mh_summary_add(cls, d->d_name, forceindex);
			} else {
				const gchar *uid = camel_message_info_uid(info);
				CamelMessageInfo *old = g_hash_table_lookup(left, uid);

				if (old) {
					camel_message_info_free(old);
					g_hash_table_remove(left, uid);
				}
				camel_message_info_free(info);
			}
		}
	}
	closedir(dir);
	g_hash_table_foreach(left, (GHFunc)remove_summary, cls);
	g_hash_table_destroy(left);

	/* sort the summary based on message number (uid), since the directory order is not useful */
	camel_folder_summary_lock (s, CAMEL_FOLDER_SUMMARY_SUMMARY_LOCK);
	camel_folder_summary_unlock (s, CAMEL_FOLDER_SUMMARY_SUMMARY_LOCK);

	return 0;
}

/* sync the summary file with the ondisk files */
static gint
mh_summary_sync (CamelLocalSummary *cls,
                 gboolean expunge,
                 CamelFolderChangeInfo *changes,
                 GError **error)
{
	CamelLocalSummaryClass *local_summary_class;
	gint count, i;
	CamelLocalMessageInfo *info;
	gchar *name;
	const gchar *uid;

	d(printf("summary_sync(expunge=%s)\n", expunge?"true":"false"));

	/* we could probably get away without this ... but why not use it, esp if we're going to
	   be doing any significant io already */
	if (camel_local_summary_check(cls, changes, error) == -1)
		return -1;

	/* FIXME: need to update/honour .mh_sequences or whatever it is */

	camel_folder_summary_prepare_fetch_all ((CamelFolderSummary *)cls, error);
	count = camel_folder_summary_count((CamelFolderSummary *)cls);
	for (i=count-1;i>=0;i--) {
		info = (CamelLocalMessageInfo *)camel_folder_summary_index((CamelFolderSummary *)cls, i);
		g_assert(info);
		if (expunge && (info->info.flags & CAMEL_MESSAGE_DELETED)) {
			uid = camel_message_info_uid(info);
			name = g_strdup_printf("%s/%s", cls->folder_path, uid);
			d(printf("deleting %s\n", name));
			if (unlink(name) == 0 || errno==ENOENT) {

				/* FIXME: put this in folder_summary::remove()? */
				if (cls->index)
					camel_index_delete_name(cls->index, (gchar *)uid);

				camel_folder_change_info_remove_uid(changes, uid);
				camel_folder_summary_remove((CamelFolderSummary *)cls, (CamelMessageInfo *)info);
			}
			g_free(name);
		} else if (info->info.flags & (CAMEL_MESSAGE_FOLDER_NOXEV|CAMEL_MESSAGE_FOLDER_FLAGGED)) {
			info->info.flags &= 0xffff;
		}
		camel_message_info_free(info);
	}

	/* Chain up to parent's sync() method. */
	local_summary_class = CAMEL_LOCAL_SUMMARY_CLASS (camel_mh_summary_parent_class);
	return local_summary_class->sync (cls, expunge, changes, error);
}
