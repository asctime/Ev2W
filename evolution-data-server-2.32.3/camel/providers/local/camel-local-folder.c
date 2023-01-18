/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *
 *  Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#if !defined (G_OS_WIN32) && !defined (_POSIX_PATH_MAX)
#include <posix1_lim.h>
#endif

#include "camel-local-folder.h"
#include "camel-local-private.h"
#include "camel-local-store.h"
#include "camel-local-summary.h"

#define d(x) /*(printf("%s(%d): ", __FILE__, __LINE__),(x))*/

#ifndef PATH_MAX
#define PATH_MAX _POSIX_PATH_MAX
#endif

#define CAMEL_LOCAL_FOLDER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), CAMEL_TYPE_LOCAL_FOLDER, CamelLocalFolderPrivate))

/* The custom property ID is a CamelArg artifact.
 * It still identifies the property in state files. */
enum {
	PROP_0,
	PROP_INDEX_BODY = 0x2400
};

static gint local_lock(CamelLocalFolder *lf, CamelLockType type, GError **error);
static void local_unlock(CamelLocalFolder *lf);

static gboolean local_refresh_info(CamelFolder *folder, GError **error);

static gboolean local_sync(CamelFolder *folder, gboolean expunge, GError **error);
static gboolean local_expunge(CamelFolder *folder, GError **error);

static GPtrArray *local_search_by_expression(CamelFolder *folder, const gchar *expression, GError **error);
static guint32 local_count_by_expression(CamelFolder *folder, const gchar *expression, GError **error);
static GPtrArray *local_search_by_uids(CamelFolder *folder, const gchar *expression, GPtrArray *uids, GError **error);
static void local_search_free(CamelFolder *folder, GPtrArray * result);
static GPtrArray * local_get_uncached_uids (CamelFolder *folder, GPtrArray * uids, GError **error);

static void local_delete(CamelFolder *folder);
static void local_rename(CamelFolder *folder, const gchar *newname);

G_DEFINE_TYPE (CamelLocalFolder, camel_local_folder, CAMEL_TYPE_FOLDER)

static void
local_folder_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_INDEX_BODY:
			camel_local_folder_set_index_body (
				CAMEL_LOCAL_FOLDER (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
local_folder_get_property (GObject *object,
                           guint property_id,
                           GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_INDEX_BODY:
			g_value_set_boolean (
				value, camel_local_folder_get_index_body (
				CAMEL_LOCAL_FOLDER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
local_folder_dispose (GObject *object)
{
	CamelFolder *folder;
	CamelLocalFolder *local_folder;

	folder = CAMEL_FOLDER (object);
	local_folder = CAMEL_LOCAL_FOLDER (object);

	if (folder->summary != NULL) {
		camel_local_summary_sync (
			CAMEL_LOCAL_SUMMARY (folder->summary),
			FALSE, local_folder->changes, NULL);
		g_object_unref (folder->summary);
		folder->summary = NULL;
	}

	if (local_folder->search != NULL) {
		g_object_unref (local_folder->search);
		local_folder->search = NULL;
	}

	if (local_folder->index != NULL) {
		g_object_unref (local_folder->index);
		local_folder->index = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (camel_local_folder_parent_class)->dispose (object);
}

static void
local_folder_finalize (GObject *object)
{
	CamelLocalFolder *local_folder;

	local_folder = CAMEL_LOCAL_FOLDER (object);

	while (local_folder->locked > 0)
		camel_local_folder_unlock (local_folder);

	g_free (local_folder->base_path);
	g_free (local_folder->folder_path);
	g_free (local_folder->summary_path);
	g_free (local_folder->index_path);

	camel_folder_change_info_free (local_folder->changes);

	g_mutex_free (local_folder->priv->search_lock);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (camel_local_folder_parent_class)->finalize (object);
}

static void
local_folder_constructed (GObject *object)
{
	CamelFolder *folder;
	CamelStore *parent_store;
	CamelURL *url;
	const gchar *full_name;
	const gchar *tmp;
	gchar *description;
	gchar *path;

	folder = CAMEL_FOLDER (object);
	full_name = camel_folder_get_full_name (folder);
	parent_store = camel_folder_get_parent_store (folder);

	url = CAMEL_SERVICE (parent_store)->url;
	if (url->path == NULL)
		return;

	path = g_strdup_printf ("%s/%s", url->path, full_name);

	if ((tmp = getenv ("HOME")) && strncmp (tmp, path, strlen (tmp)) == 0)
		/* Translators: This is used for a folder description,
		 * for folders being under $HOME.  The first %s is replaced
		 * with a relative path under $HOME, the second %s is
		 * replaced with a protocol name, like mbox/maldir/... */
		description = g_strdup_printf (
			_("~%s (%s)"),
			path + strlen (tmp),
			url->protocol);
	else if ((tmp = "/var/spool/mail") && strncmp (tmp, path, strlen (tmp)) == 0)
		/* Translators: This is used for a folder description, for
		 * folders being under /var/spool/mail.  The first %s is
		 * replaced with a relative path under /var/spool/mail,
		 * the second %s is replaced with a protocol name, like
		 * mbox/maldir/... */
		description = g_strdup_printf (
			_("mailbox: %s (%s)"),
			path + strlen (tmp),
			url->protocol);
	else if ((tmp = "/var/mail") && strncmp (tmp, path, strlen (tmp)) == 0)
		/* Translators: This is used for a folder description, for
		 * folders being under /var/mail.  The first %s is replaced
		 * with a relative path under /var/mail, the second %s is
		 * replaced with a protocol name, like mbox/maldir/... */
		description = g_strdup_printf (
			_("mailbox: %s (%s)"),
			path + strlen (tmp),
			url->protocol);
	else
		/* Translators: This is used for a folder description.
		 * The first %s is replaced with a folder's full path,
		 * the second %s is replaced with a protocol name, like
		 * mbox/maldir/... */
		description = g_strdup_printf (
			_("%s (%s)"), path,
			url->protocol);

	camel_folder_set_description (folder, description);

	g_free (description);
	g_free (path);
}

static void
camel_local_folder_class_init (CamelLocalFolderClass *class)
{
	GObjectClass *object_class;
	CamelFolderClass *folder_class;

	g_type_class_add_private (class, sizeof (CamelLocalFolderPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = local_folder_set_property;
	object_class->get_property = local_folder_get_property;
	object_class->dispose = local_folder_dispose;
	object_class->finalize = local_folder_finalize;
	object_class->constructed = local_folder_constructed;

	folder_class = CAMEL_FOLDER_CLASS (class);
	folder_class->refresh_info = local_refresh_info;
	folder_class->sync = local_sync;
	folder_class->expunge = local_expunge;
	folder_class->get_uncached_uids = local_get_uncached_uids;
	folder_class->search_by_expression = local_search_by_expression;
	folder_class->count_by_expression = local_count_by_expression;
	folder_class->search_by_uids = local_search_by_uids;
	folder_class->search_free = local_search_free;
	folder_class->delete = local_delete;
	folder_class->rename = local_rename;

	class->lock = local_lock;
	class->unlock = local_unlock;

	g_object_class_install_property (
		object_class,
		PROP_INDEX_BODY,
		g_param_spec_boolean (
			"index-body",
			"Index Body",
			N_("Index message body data"),
			FALSE,
			G_PARAM_READWRITE |
			CAMEL_PARAM_PERSISTENT));
}

static void
camel_local_folder_init (CamelLocalFolder *local_folder)
{
	CamelFolder *folder = CAMEL_FOLDER (local_folder);

	local_folder->priv = CAMEL_LOCAL_FOLDER_GET_PRIVATE (local_folder);
	local_folder->priv->search_lock = g_mutex_new();

	folder->folder_flags |= (CAMEL_FOLDER_HAS_SUMMARY_CAPABILITY |
				 CAMEL_FOLDER_HAS_SEARCH_CAPABILITY);

	folder->permanent_flags = CAMEL_MESSAGE_ANSWERED |
	    CAMEL_MESSAGE_DELETED | CAMEL_MESSAGE_DRAFT |
	    CAMEL_MESSAGE_FLAGGED | CAMEL_MESSAGE_SEEN |
	    CAMEL_MESSAGE_ANSWERED_ALL | CAMEL_MESSAGE_USER;

	folder->summary = NULL;
	local_folder->search = NULL;
}

CamelLocalFolder *
camel_local_folder_construct(CamelLocalFolder *lf, guint32 flags, GError **error)
{
	CamelFolder *folder;
	const gchar *root_dir_path;
	gchar *tmp, *statepath;
#ifndef G_OS_WIN32
	gchar folder_path[PATH_MAX];
	struct stat st;
#endif
	gint forceindex, len;
	CamelLocalStore *ls;
	CamelStore *parent_store;
	const gchar *full_name;
	const gchar *name;

	folder = CAMEL_FOLDER (lf);
	name = camel_folder_get_name (folder);
	full_name = camel_folder_get_full_name (folder);
	parent_store = camel_folder_get_parent_store (folder);

	ls = CAMEL_LOCAL_STORE (parent_store);

	root_dir_path = camel_local_store_get_toplevel_dir(ls);
	/* strip the trailing '/' which is always present */
	len = strlen (root_dir_path);
	tmp = g_alloca (len + 1);
	strcpy (tmp, root_dir_path);
	if (len>1 && G_IS_DIR_SEPARATOR(tmp[len-1]))
		tmp[len-1] = 0;

	lf->base_path = g_strdup(root_dir_path);

	lf->folder_path = camel_local_store_get_full_path(ls, full_name);
	lf->summary_path = camel_local_store_get_meta_path(ls, full_name, ".ev-summary");
	lf->index_path = camel_local_store_get_meta_path(ls, full_name, ".ibex");
	statepath = camel_local_store_get_meta_path(ls, full_name, ".cmeta");

	camel_object_set_state_filename (CAMEL_OBJECT (lf), statepath);
	g_free(statepath);

	lf->flags = flags;

	if (camel_object_state_read (CAMEL_OBJECT (lf)) == -1) {
		/* No metadata - load defaults and persitify */
		camel_local_folder_set_index_body (lf, TRUE);
		camel_object_state_write (CAMEL_OBJECT (lf));
	}
#ifndef G_OS_WIN32
	/* follow any symlinks to the mailbox */
	if (g_lstat (lf->folder_path, &st) != -1 && S_ISLNK (st.st_mode) &&
	    realpath (lf->folder_path, folder_path) != NULL) {
		g_free (lf->folder_path);
		lf->folder_path = g_strdup (folder_path);
	}
#endif
	lf->changes = camel_folder_change_info_new();

	/* TODO: Remove the following line, it is a temporary workaround to remove
	   the old-format 'ibex' files that might be lying around */
	g_unlink(lf->index_path);

	/* FIXME: Need to run indexing off of the setv method */

	/* if we have no/invalid index file, force it */
	forceindex = camel_text_index_check(lf->index_path) == -1;
	if (lf->flags & CAMEL_STORE_FOLDER_BODY_INDEX) {
		gint flag = O_RDWR|O_CREAT;

		if (forceindex)
			flag |= O_TRUNC;

		lf->index = (CamelIndex *)camel_text_index_new(lf->index_path, flag);
		if (lf->index == NULL) {
			/* yes, this isn't fatal at all */
			g_warning("Could not open/create index file: %s: indexing not performed", g_strerror (errno));
			forceindex = FALSE;
			/* record that we dont have an index afterall */
			lf->flags &= ~CAMEL_STORE_FOLDER_BODY_INDEX;
		}
	} else {
		/* if we do have an index file, remove it (?) */
		if (forceindex == FALSE)
			camel_text_index_remove(lf->index_path);
		forceindex = FALSE;
	}

	folder->summary = (CamelFolderSummary *)CAMEL_LOCAL_FOLDER_GET_CLASS(lf)->create_summary(lf, lf->summary_path, lf->folder_path, lf->index);
	if (!(flags & CAMEL_STORE_IS_MIGRATING) && camel_local_summary_load((CamelLocalSummary *)folder->summary, forceindex, NULL) == -1) {
		/* ? */
		if (camel_local_summary_check((CamelLocalSummary *)folder->summary, lf->changes, error) == 0) {
			/* we sync here so that any hard work setting up the folder isn't lost */
			if (camel_local_summary_sync((CamelLocalSummary *)folder->summary, FALSE, lf->changes, error) == -1) {
				g_object_unref (CAMEL_OBJECT (folder));
				return NULL;
			}
		}
	}

	/* We don't need to sync here ..., it can sync later on when it calls refresh info */
#if 0
	/*if (camel_local_summary_check((CamelLocalSummary *)folder->summary, lf->changes, ex) == -1) {*/
	/* we sync here so that any hard work setting up the folder isn't lost */
	/*if (camel_local_summary_sync((CamelLocalSummary *)folder->summary, FALSE, lf->changes, ex) == -1) {
		g_object_unref (CAMEL_OBJECT (folder));
		g_free(name);
		return NULL;
		}*/
#endif

	/* TODO: This probably shouldn't be here? */
	if ((flags & CAMEL_STORE_FOLDER_CREATE) != 0) {
		CamelFolderInfo *fi;

		fi = camel_store_get_folder_info (parent_store, full_name, 0, NULL);
		g_return_val_if_fail (fi != NULL, lf);

		camel_store_folder_created (parent_store, fi);
		camel_folder_info_free(fi);
	}

	return lf;
}

gboolean
camel_local_folder_get_index_body (CamelLocalFolder *local_folder)
{
	g_return_val_if_fail (CAMEL_IS_LOCAL_FOLDER (local_folder), FALSE);

	return (local_folder->flags & CAMEL_STORE_FOLDER_BODY_INDEX);
}

void
camel_local_folder_set_index_body (CamelLocalFolder *local_folder,
                                   gboolean index_body)
{
	g_return_if_fail (CAMEL_IS_LOCAL_FOLDER (local_folder));

	if (index_body)
		local_folder->flags |= CAMEL_STORE_FOLDER_BODY_INDEX;
	else
		local_folder->flags &= ~CAMEL_STORE_FOLDER_BODY_INDEX;

	g_object_notify (G_OBJECT (local_folder), "index-body");
}

/* lock the folder, may be called repeatedly (with matching unlock calls),
   with type the same or less than the first call */
gint camel_local_folder_lock(CamelLocalFolder *lf, CamelLockType type, GError **error)
{
	if (lf->locked > 0) {
		/* lets be anal here - its important the code knows what its doing */
		g_assert(lf->locktype == type || lf->locktype == CAMEL_LOCK_WRITE);
	} else {
		if (CAMEL_LOCAL_FOLDER_GET_CLASS(lf)->lock(lf, type, error) == -1)
			return -1;
		lf->locktype = type;
	}

	lf->locked++;

	return 0;
}

/* unlock folder */
gint camel_local_folder_unlock(CamelLocalFolder *lf)
{
	g_assert(lf->locked>0);
	lf->locked--;
	if (lf->locked == 0)
		CAMEL_LOCAL_FOLDER_GET_CLASS(lf)->unlock(lf);

	return 0;
}

static gint
local_lock(CamelLocalFolder *lf, CamelLockType type, GError **error)
{
	return 0;
}

static void
local_unlock(CamelLocalFolder *lf)
{
	/* nothing */
}

/* for auto-check to work */
static gboolean
local_refresh_info(CamelFolder *folder, GError **error)
{
	CamelLocalFolder *lf = (CamelLocalFolder *)folder;

	if (camel_local_summary_check((CamelLocalSummary *)folder->summary, lf->changes, error) == -1)
		return FALSE;

	if (camel_folder_change_info_changed(lf->changes)) {
		camel_folder_changed (folder, lf->changes);
		camel_folder_change_info_clear(lf->changes);
	}

	return TRUE;
}

static GPtrArray *
local_get_uncached_uids (CamelFolder *folder, GPtrArray * uids, GError **error)
{
	GPtrArray *result = g_ptr_array_new ();
	/* By default, we would have everything local. No need to fetch from anywhere. */
	return result;
}

static gboolean
local_sync(CamelFolder *folder, gboolean expunge, GError **error)
{
	CamelLocalFolder *lf = CAMEL_LOCAL_FOLDER(folder);
	gboolean success;

	d(printf("local sync '%s' , expunge=%s\n", folder->full_name, expunge?"true":"false"));

	if (camel_local_folder_lock(lf, CAMEL_LOCK_WRITE, error) == -1)
		return FALSE;

	camel_object_state_write (CAMEL_OBJECT (lf));

	/* if sync fails, we'll pass it up on exit through ex */
	success = (camel_local_summary_sync (
		(CamelLocalSummary *)folder->summary,
		expunge, lf->changes, error) == 0);
	camel_local_folder_unlock(lf);

	if (camel_folder_change_info_changed(lf->changes)) {
		camel_folder_changed (folder, lf->changes);
		camel_folder_change_info_clear(lf->changes);
	}

	return success;
}

static gboolean
local_expunge(CamelFolder *folder, GError **error)
{
	d(printf("expunge\n"));

	/* Just do a sync with expunge, serves the same purpose */
	/* call the callback directly, to avoid locking problems */
	return CAMEL_FOLDER_GET_CLASS (folder)->sync (folder, TRUE, error);
}

static void
local_delete(CamelFolder *folder)
{
	CamelLocalFolder *lf = (CamelLocalFolder *)folder;

	if (lf->index)
		camel_index_delete(lf->index);

	CAMEL_FOLDER_CLASS (camel_local_folder_parent_class)->delete (folder);
}

static void
local_rename(CamelFolder *folder, const gchar *newname)
{
	CamelLocalFolder *lf = (CamelLocalFolder *)folder;
	gchar *statepath;
	CamelLocalStore *ls;
	CamelStore *parent_store;

	parent_store = camel_folder_get_parent_store (folder);
	ls = CAMEL_LOCAL_STORE (parent_store);

	d(printf("renaming local folder paths to '%s'\n", newname));

	/* Sync? */

	g_free(lf->folder_path);
	g_free(lf->summary_path);
	g_free(lf->index_path);

	lf->folder_path = camel_local_store_get_full_path(ls, newname);
	lf->summary_path = camel_local_store_get_meta_path(ls, newname, ".ev-summary");
	lf->index_path = camel_local_store_get_meta_path(ls, newname, ".ibex");
	statepath = camel_local_store_get_meta_path(ls, newname, ".cmeta");
	camel_object_set_state_filename (CAMEL_OBJECT (lf), statepath);
	g_free(statepath);

	/* FIXME: Poke some internals, sigh */
	camel_folder_summary_set_filename(folder->summary, lf->summary_path);
	g_free(((CamelLocalSummary *)folder->summary)->folder_path);
	((CamelLocalSummary *)folder->summary)->folder_path = g_strdup(lf->folder_path);

	CAMEL_FOLDER_CLASS (camel_local_folder_parent_class)->rename (folder, newname);
}

static GPtrArray *
local_search_by_expression(CamelFolder *folder, const gchar *expression, GError **error)
{
	CamelLocalFolder *local_folder = CAMEL_LOCAL_FOLDER(folder);
	GPtrArray *matches;

	CAMEL_LOCAL_FOLDER_LOCK(folder, search_lock);

	if (local_folder->search == NULL)
		local_folder->search = camel_folder_search_new();

	camel_folder_search_set_folder(local_folder->search, folder);
	camel_folder_search_set_body_index(local_folder->search, local_folder->index);
	matches = camel_folder_search_search(local_folder->search, expression, NULL, error);

	CAMEL_LOCAL_FOLDER_UNLOCK(folder, search_lock);

	return matches;
}

static guint32
local_count_by_expression(CamelFolder *folder, const gchar *expression, GError **error)
{
	CamelLocalFolder *local_folder = CAMEL_LOCAL_FOLDER(folder);
	gint matches;

	CAMEL_LOCAL_FOLDER_LOCK(folder, search_lock);

	if (local_folder->search == NULL)
		local_folder->search = camel_folder_search_new();

	camel_folder_search_set_folder(local_folder->search, folder);
	camel_folder_search_set_body_index(local_folder->search, local_folder->index);
	matches = camel_folder_search_count (local_folder->search, expression, error);

	CAMEL_LOCAL_FOLDER_UNLOCK(folder, search_lock);

	return matches;
}

static GPtrArray *
local_search_by_uids(CamelFolder *folder, const gchar *expression, GPtrArray *uids, GError **error)
{
	CamelLocalFolder *local_folder = CAMEL_LOCAL_FOLDER(folder);
	GPtrArray *matches;

	if (uids->len == 0)
		return g_ptr_array_new();

	CAMEL_LOCAL_FOLDER_LOCK(folder, search_lock);

	if (local_folder->search == NULL)
		local_folder->search = camel_folder_search_new();

	camel_folder_search_set_folder(local_folder->search, folder);
	camel_folder_search_set_body_index(local_folder->search, local_folder->index);
	matches = camel_folder_search_search(local_folder->search, expression, uids, error);

	CAMEL_LOCAL_FOLDER_UNLOCK(folder, search_lock);

	return matches;
}

static void
local_search_free(CamelFolder *folder, GPtrArray * result)
{
	CamelLocalFolder *local_folder = CAMEL_LOCAL_FOLDER(folder);

	/* we need to lock this free because of the way search_free_result works */
	/* FIXME: put the lock inside search_free_result */
	CAMEL_LOCAL_FOLDER_LOCK(folder, search_lock);

	camel_folder_search_free_result(local_folder->search, result);

	CAMEL_LOCAL_FOLDER_UNLOCK(folder, search_lock);
}

void
set_cannot_get_message_ex (GError **error, gint err_code, const gchar *msgID, const gchar *folder_path, const gchar *detailErr)
{
	/* Translators: The first %s is replaced with a message ID,
	   the second %s is replaced with the folder path,
	   the third %s is replaced with a detailed error string */
	g_set_error (
		error, CAMEL_ERROR, err_code,
		_("Cannot get message %s from folder %s\n%s"),
		msgID, folder_path, detailErr);
}
