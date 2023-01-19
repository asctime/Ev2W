/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; fill-column: 160 -*-
 *
 *  Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 *  Authors: Michael Zucchi <notzed@ximian.com>
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
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>

#include "camel-spool-summary.h"
#include "camel-local-private.h"
#include "camel-win32.h"

#define io(x)
#define d(x) /*(printf("%s(%d): ", __FILE__, __LINE__),(x))*/

#define CAMEL_SPOOL_SUMMARY_VERSION (0x400)

static gint spool_summary_load(CamelLocalSummary *cls, gint forceindex, GError **error);
static gint spool_summary_check(CamelLocalSummary *cls, CamelFolderChangeInfo *changeinfo, GError **error);

static gint spool_summary_sync_full(CamelMboxSummary *cls, gboolean expunge, CamelFolderChangeInfo *changeinfo, GError **error);
static gint spool_summary_need_index(void);

G_DEFINE_TYPE (CamelSpoolSummary, camel_spool_summary, CAMEL_TYPE_MBOX_SUMMARY)

static void
camel_spool_summary_class_init (CamelSpoolSummaryClass *class)
{
	CamelLocalSummaryClass *local_summary_class;
	CamelMboxSummaryClass *mbox_summary_class;

	local_summary_class = CAMEL_LOCAL_SUMMARY_CLASS (class);
	local_summary_class->load = spool_summary_load;
	local_summary_class->check = spool_summary_check;
	local_summary_class->need_index = spool_summary_need_index;

	mbox_summary_class = CAMEL_MBOX_SUMMARY_CLASS (class);
	mbox_summary_class->sync_full = spool_summary_sync_full;
}

static void
camel_spool_summary_init(CamelSpoolSummary *spool_summary)
{
	CamelFolderSummary *folder_summary;

	folder_summary = CAMEL_FOLDER_SUMMARY (spool_summary);

	/* message info size is from mbox parent */

	/* and a unique file version */
	folder_summary->version += CAMEL_SPOOL_SUMMARY_VERSION;
}

CamelSpoolSummary *
camel_spool_summary_new (CamelFolder *folder,
                         const gchar *mbox_name)
{
	CamelSpoolSummary *new;

	new = g_object_new (CAMEL_TYPE_SPOOL_SUMMARY, NULL);
	((CamelFolderSummary *)new)->folder = folder;
	if (folder) {
		CamelStore *parent_store;

		parent_store = camel_folder_get_parent_store (folder);
		camel_db_set_collate (parent_store->cdb_r, "bdata", "spool_frompos_sort", (CamelDBCollate)camel_local_frompos_sort);
		((CamelFolderSummary *)new)->sort_by = "bdata";
		((CamelFolderSummary *)new)->collate = "spool_frompos_sort";
	}
	camel_local_summary_construct((CamelLocalSummary *)new, NULL, mbox_name, NULL);
	camel_folder_summary_load_from_db ((CamelFolderSummary *)new, NULL);
	return new;
}

static gint
spool_summary_load (CamelLocalSummary *cls,
                    gint forceindex,
                    GError **error)
{
	g_warning("spool summary - not loading anything\n");
	return 0;
}

/* perform a full sync */
static gint
spool_summary_sync_full (CamelMboxSummary *cls,
                         gboolean expunge,
                         CamelFolderChangeInfo *changeinfo,
                         GError **error)
{
	gint fd = -1, fdout = -1;
	gchar tmpname[64] = { '\0' };
	gchar *buffer, *p;
	goffset spoollen, outlen;
	gint size, sizeout;
	struct stat st;
	guint32 flags = (expunge?1:0);

	d(printf("performing full summary/sync\n"));

	camel_operation_start(NULL, _("Storing folder"));

	fd = open(((CamelLocalSummary *)cls)->folder_path, O_RDWR|O_LARGEFILE);
	if (fd == -1) {
		g_set_error (
			error, G_IO_ERROR,
			g_io_error_from_errno (errno),
			_("Could not open file: %s: %s"),
			((CamelLocalSummary *)cls)->folder_path,
			g_strerror (errno));
		camel_operation_end(NULL);
		return -1;
	}

	sprintf (tmpname, "/tmp/spool.camel.XXXXXX");
	fdout = g_mkstemp (tmpname);

	d(printf("Writing tmp file to %s\n", tmpname));
	if (fdout == -1) {
		g_set_error (
			error, G_IO_ERROR,
			g_io_error_from_errno (errno),
			_("Cannot open temporary mailbox: %s"),
			g_strerror (errno));
		goto error;
	}

	if (camel_mbox_summary_sync_mbox((CamelMboxSummary *)cls, flags, changeinfo, fd, fdout, error) == -1)
		goto error;

	/* sync out content */
	if (fsync(fdout) == -1) {
		g_warning("Cannot synchronize temporary folder: %s", g_strerror (errno));
		g_set_error (
			error, G_IO_ERROR,
			g_io_error_from_errno (errno),
			_("Could not synchronize temporary folder %s: %s"),
			((CamelLocalSummary *)cls)->folder_path,
			g_strerror (errno));
		goto error;
	}

	/* see if we can write this much to the spool file */
	if (fstat(fd, &st) == -1) {
		g_warning("Cannot synchronize temporary folder: %s", g_strerror (errno));
		g_set_error (
			error, G_IO_ERROR,
			g_io_error_from_errno (errno),
			_("Could not synchronize temporary folder %s: %s"),
			((CamelLocalSummary *)cls)->folder_path,
			g_strerror (errno));
		goto error;
	}
	spoollen = st.st_size;

	if (fstat(fdout, &st) == -1) {
		g_warning("Cannot synchronize temporary folder: %s", g_strerror (errno));
		g_set_error (
			error, G_IO_ERROR,
			g_io_error_from_errno (errno),
			_("Could not synchronize temporary folder %s: %s"),
			((CamelLocalSummary *)cls)->folder_path,
			g_strerror (errno));
		goto error;
	}
	outlen = st.st_size;

	/* I think this is the right way to do this - checking that the file will fit the new data */
	if (outlen>0
	    && (lseek(fd, outlen-1, SEEK_SET) == -1
		|| write(fd, "", 1) != 1
		|| fsync(fd) == -1
		|| lseek(fd, 0, SEEK_SET) == -1
		|| lseek(fdout, 0, SEEK_SET) == -1)) {
		g_warning("Cannot synchronize spool folder: %s", g_strerror (errno));
		g_set_error (
			error, G_IO_ERROR,
			g_io_error_from_errno (errno),
			_("Could not synchronize spool folder %s: %s"),
			((CamelLocalSummary *)cls)->folder_path,
			g_strerror (errno));
		/* incase we ran out of room, remove any trailing space first */
		ftruncate(fd, spoollen);
		goto error;
	}

	/* now copy content back */
	buffer = g_malloc(8192);
	size = 1;
	while (size>0) {
		do {
			size = read(fdout, buffer, 8192);
		} while (size == -1 && errno == EINTR);

		if (size > 0) {
			p = buffer;
			do {
				sizeout = write(fd, p, size);
				if (sizeout > 0) {
					p+= sizeout;
					size -= sizeout;
				}
			} while ((sizeout == -1 && errno == EINTR) && size > 0);
			size = sizeout;
		}

		if (size == -1) {
			g_set_error (
				error, G_IO_ERROR,
				g_io_error_from_errno (errno),
				_("Could not synchronize spool folder %s: %s\n"
				  "Folder may be corrupt, copy saved in '%s'"),
				((CamelLocalSummary *)cls)->folder_path,
				g_strerror (errno), tmpname);
			/* so we dont delete it */
			tmpname[0] = '\0';
			g_free(buffer);
			goto error;
		}
	}

	g_free(buffer);

	d(printf("Closing folders\n"));

	if (ftruncate(fd, outlen) == -1) {
		g_set_error (
			error, G_IO_ERROR,
			g_io_error_from_errno (errno),
			_("Could not synchronize spool folder %s: %s\n"
			  "Folder may be corrupt, copy saved in '%s'"),
			((CamelLocalSummary *)cls)->folder_path,
			g_strerror (errno), tmpname);
		tmpname[0] = '\0';
		goto error;
	}

	if (close(fd) == -1) {
		g_warning("Cannot close source folder: %s", g_strerror (errno));
		g_set_error (
			error, G_IO_ERROR,
			g_io_error_from_errno (errno),
			_("Could not synchronize spool folder %s: %s\n"
			  "Folder may be corrupt, copy saved in '%s'"),
			((CamelLocalSummary *)cls)->folder_path,
			g_strerror (errno), tmpname);
		tmpname[0] = '\0';
		fd = -1;
		goto error;
	}

	close(fdout);

	if (tmpname[0] != '\0')
		unlink(tmpname);

	camel_operation_end(NULL);

	return 0;
 error:
	if (fd != -1)
		close(fd);

	if (fdout != -1)
		close(fdout);

	if (tmpname[0] != '\0')
		unlink(tmpname);

	camel_operation_end(NULL);

	return -1;
}

static gint
spool_summary_check (CamelLocalSummary *cls,
                     CamelFolderChangeInfo *changeinfo,
                     GError **error)
{
	gint i, work, count;
	struct stat st;
	CamelFolderSummary *s = (CamelFolderSummary *)cls;

	if (CAMEL_LOCAL_SUMMARY_CLASS (camel_spool_summary_parent_class)->check(cls, changeinfo, error) == -1)
		return -1;

	/* check to see if we need to copy/update the file; missing xev headers prompt this */
	work = FALSE;
	camel_folder_summary_prepare_fetch_all (s, error);
	count = camel_folder_summary_count(s);
	for (i=0;!work && i<count; i++) {
		CamelMboxMessageInfo *info = (CamelMboxMessageInfo *)camel_folder_summary_index(s, i);
		g_assert(info);
		work = (info->info.info.flags & (CAMEL_MESSAGE_FOLDER_NOXEV)) != 0;
		camel_message_info_free((CamelMessageInfo *)info);
	}

	/* if we do, then write out the headers using sync_full, etc */
	if (work) {
		d(printf("Have to add new headers, re-syncing from the start to accomplish this\n"));
		if (CAMEL_MBOX_SUMMARY_GET_CLASS (cls)->sync_full (CAMEL_MBOX_SUMMARY (cls), FALSE, changeinfo, error) == -1)
			return -1;

		if (g_stat(cls->folder_path, &st) == -1) {
			g_set_error (
				error, G_IO_ERROR,
				g_io_error_from_errno (errno),
				_("Unknown error: %s"),
				g_strerror (errno));
			return -1;
		}

		((CamelMboxSummary *)cls)->folder_size = st.st_size;
		((CamelFolderSummary *)cls)->time = st.st_mtime;
	}

	return 0;
}

static gint
spool_summary_need_index(void) {
	return 0;
}
