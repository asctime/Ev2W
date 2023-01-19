/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; fill-column: 160 -*-
 *
 * Authors: Michael Zucchi <notzed@ximian.com>
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

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <glib/gi18n-lib.h>

#include "camel-spool-folder.h"
#include "camel-spool-store.h"
#include "camel-spool-summary.h"

#define d(x) /*(printf("%s(%d): ", __FILE__, __LINE__),(x))*/

static CamelLocalSummary *spool_create_summary(CamelLocalFolder *lf, const gchar *path, const gchar *folder, CamelIndex *index);

static gint spool_lock(CamelLocalFolder *lf, CamelLockType type, GError **error);
static void spool_unlock(CamelLocalFolder *lf);

G_DEFINE_TYPE (CamelSpoolFolder, camel_spool_folder, CAMEL_TYPE_MBOX_FOLDER)

static void
camel_spool_folder_class_init (CamelSpoolFolderClass *class)
{
	CamelLocalFolderClass *local_folder_class;

	local_folder_class = CAMEL_LOCAL_FOLDER_CLASS (class);
	local_folder_class->create_summary = spool_create_summary;
	local_folder_class->lock = spool_lock;
	local_folder_class->unlock = spool_unlock;
}

static void
camel_spool_folder_init (CamelSpoolFolder *spool_folder)
{
	spool_folder->lockid = -1;
}

CamelFolder *
camel_spool_folder_new (CamelStore *parent_store,
                        const gchar *full_name,
                        guint32 flags,
                        GError **error)
{
	CamelFolder *folder;
	gchar *basename;

	basename = g_path_get_basename (full_name);

	folder = g_object_new (
		CAMEL_TYPE_SPOOL_FOLDER,
		"name", basename, "full-name", full_name,
		"parent-store", parent_store, NULL);

	if (parent_store->flags & CAMEL_STORE_FILTER_INBOX
	    && strcmp(full_name, "INBOX") == 0)
		folder->folder_flags |= CAMEL_FOLDER_FILTER_RECENT;
	flags &= ~CAMEL_STORE_FOLDER_BODY_INDEX;

	folder = (CamelFolder *)camel_local_folder_construct (
		(CamelLocalFolder *)folder, flags, error);
	if (folder) {
		if (camel_url_get_param(((CamelService *)parent_store)->url, "xstatus"))
			camel_mbox_summary_xstatus((CamelMboxSummary *)folder->summary, TRUE);
	}

	g_free (basename);

	return folder;
}

static CamelLocalSummary *
spool_create_summary (CamelLocalFolder *lf,
                      const gchar *path,
                      const gchar *folder,
                      CamelIndex *index)
{
	return (CamelLocalSummary *) camel_spool_summary_new (
		CAMEL_FOLDER (lf), folder);
}

static gint
spool_lock (CamelLocalFolder *lf,
            CamelLockType type,
            GError **error)
{
	gint retry = 0;
	CamelMboxFolder *mf = (CamelMboxFolder *)lf;
	CamelSpoolFolder *sf = (CamelSpoolFolder *)lf;
	GError *local_error = NULL;

	mf->lockfd = open(lf->folder_path, O_RDWR|O_LARGEFILE, 0);
	if (mf->lockfd == -1) {
		g_set_error (
			error, G_IO_ERROR,
			g_io_error_from_errno (errno),
			_("Cannot create folder lock on %s: %s"),
			lf->folder_path, g_strerror (errno));
		return -1;
	}

	while (retry < CAMEL_LOCK_RETRY) {
		if (retry > 0)
			sleep(CAMEL_LOCK_DELAY);

		g_clear_error (&local_error);

		if (camel_lock_fcntl(mf->lockfd, type, &local_error) == 0) {
			if (camel_lock_flock(mf->lockfd, type, &local_error) == 0) {
				if ((sf->lockid = camel_lock_helper_lock(lf->folder_path, &local_error)) != -1)
					return 0;
				camel_unlock_flock(mf->lockfd);
			}
			camel_unlock_fcntl(mf->lockfd);
		}
		retry++;
	}

	close (mf->lockfd);
	mf->lockfd = -1;

	if (local_error != NULL)
		g_propagate_error (error, local_error);

	return -1;
}

static void
spool_unlock (CamelLocalFolder *lf)
{
	CamelMboxFolder *mf = (CamelMboxFolder *)lf;
	CamelSpoolFolder *sf = (CamelSpoolFolder *)lf;

	camel_lock_helper_unlock(sf->lockid);
	sf->lockid = -1;
	camel_unlock_flock(mf->lockfd);
	camel_unlock_fcntl(mf->lockfd);

	close(mf->lockfd);
	mf->lockfd = -1;
}
