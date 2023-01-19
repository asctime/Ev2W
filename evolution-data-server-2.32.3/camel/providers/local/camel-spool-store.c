/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
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
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>

#include "camel-spool-folder.h"
#include "camel-spool-store.h"

#define d(x)

static gboolean construct (CamelService *service, CamelSession *session, CamelProvider *provider, CamelURL *url, GError **error);
static CamelFolder *get_folder(CamelStore* store, const gchar *folder_name, guint32 flags, GError **error);
static gchar *get_name(CamelService *service, gboolean brief);
static CamelFolder *get_inbox (CamelStore *store, GError **error);
static gboolean rename_folder(CamelStore *store, const gchar *old_name, const gchar *new_name, GError **error);
static CamelFolderInfo *get_folder_info (CamelStore *store, const gchar *top, guint32 flags, GError **error);
static void free_folder_info (CamelStore *store, CamelFolderInfo *fi);

static gboolean delete_folder(CamelStore *store, const gchar *folder_name, GError **error);

static gchar *spool_get_meta_path(CamelLocalStore *ls, const gchar *full_name, const gchar *ext);
static gchar *spool_get_full_path(CamelLocalStore *ls, const gchar *full_name);

G_DEFINE_TYPE (CamelSpoolStore, camel_spool_store, CAMEL_TYPE_MBOX_STORE)

static void
camel_spool_store_class_init (CamelSpoolStoreClass *class)
{
	CamelServiceClass *service_class;
	CamelStoreClass *store_class;
	CamelLocalStoreClass *local_store_class;

	service_class = CAMEL_SERVICE_CLASS (class);
	service_class->construct = construct;
	service_class->get_name = get_name;

	store_class = CAMEL_STORE_CLASS (class);
	store_class->get_folder = get_folder;
	store_class->get_inbox = get_inbox;
	store_class->get_folder_info = get_folder_info;
	store_class->free_folder_info = free_folder_info;
	store_class->delete_folder = delete_folder;
	store_class->rename_folder = rename_folder;

	local_store_class = CAMEL_LOCAL_STORE_CLASS (class);
	local_store_class->get_full_path = spool_get_full_path;
	local_store_class->get_meta_path = spool_get_meta_path;
}

static void
camel_spool_store_init (CamelSpoolStore *spool_store)
{
}

static gboolean
construct (CamelService *service,
           CamelSession *session,
           CamelProvider *provider,
           CamelURL *url,
           GError **error)
{
	CamelServiceClass *service_class;
	struct stat st;

	d(printf("constructing store of type %s '%s:%s'\n",
		 G_OBJECT_CLASS_NAME(((CamelObject *)service)->s.type), url->protocol, url->path));

	/* Chain up to parent's construct() method. */
	service_class = CAMEL_SERVICE_CLASS (camel_spool_store_parent_class);
	if (!service_class->construct (service, session, provider, url, error))
		return FALSE;

	if (service->url->path[0] != '/') {
		g_set_error (
			error, CAMEL_STORE_ERROR,
			CAMEL_STORE_ERROR_NO_FOLDER,
			_("Store root %s is not an absolute path"),
			service->url->path);
		return FALSE;
	}

	if (g_stat(service->url->path, &st) == -1) {
		g_set_error (
			error, G_IO_ERROR,
			g_io_error_from_errno (errno),
			_("Spool '%s' cannot be opened: %s"),
			service->url->path, g_strerror (errno));
		return FALSE;
	}

	if (S_ISREG(st.st_mode))
		((CamelSpoolStore *)service)->type = CAMEL_SPOOL_STORE_MBOX;
	else if (S_ISDIR(st.st_mode))
		/* we could check here for slight variations */
		((CamelSpoolStore *)service)->type = CAMEL_SPOOL_STORE_ELM;
	else {
		g_set_error (
			error, CAMEL_STORE_ERROR,
			CAMEL_STORE_ERROR_NO_FOLDER,
			_("Spool '%s' is not a regular file or directory"),
			service->url->path);
		return FALSE;
	}

	return TRUE;
}

static CamelFolder *
get_folder (CamelStore *store,
            const gchar *folder_name,
            guint32 flags,
            GError **error)
{
	CamelFolder *folder = NULL;
	struct stat st;
	gchar *name;

	d(printf("opening folder %s on path %s\n", folder_name, path));

	/* we only support an 'INBOX' in mbox mode */
	if (((CamelSpoolStore *)store)->type == CAMEL_SPOOL_STORE_MBOX) {
		if (strcmp(folder_name, "INBOX") != 0) {
			g_set_error (
				error, CAMEL_STORE_ERROR,
				CAMEL_STORE_ERROR_NO_FOLDER,
				_("Folder '%s/%s' does not exist."),
				((CamelService *)store)->url->path, folder_name);
		} else {
			folder = camel_spool_folder_new(store, folder_name, flags, error);
		}
	} else {
		name = g_strdup_printf("%s%s", CAMEL_LOCAL_STORE(store)->toplevel_dir, folder_name);
		if (g_stat(name, &st) == -1) {
			if (errno != ENOENT) {
				g_set_error (
					error, G_IO_ERROR,
					g_io_error_from_errno (errno),
					_("Could not open folder '%s':\n%s"),
					folder_name, g_strerror (errno));
			} else if ((flags & CAMEL_STORE_FOLDER_CREATE) == 0) {
				g_set_error (
					error, CAMEL_STORE_ERROR,
					CAMEL_STORE_ERROR_NO_FOLDER,
					_("Folder '%s' does not exist."),
					folder_name);
			} else {
				if (creat (name, 0600) == -1) {
					g_set_error (
						error, G_IO_ERROR,
						g_io_error_from_errno (errno),
						_("Could not create folder '%s':\n%s"),
						folder_name, g_strerror (errno));
				} else {
					folder = camel_spool_folder_new(store, folder_name, flags, error);
				}
			}
		} else if (!S_ISREG(st.st_mode)) {
			g_set_error (
				error, CAMEL_STORE_ERROR,
				CAMEL_STORE_ERROR_NO_FOLDER,
				_("'%s' is not a mailbox file."), name);
		} else {
			folder = camel_spool_folder_new(store, folder_name, flags, error);
		}
		g_free(name);
	}

	return folder;
}

static CamelFolder *
get_inbox (CamelStore *store,
           GError **error)
{
	if (((CamelSpoolStore *)store)->type == CAMEL_SPOOL_STORE_MBOX)
		return get_folder (store, "INBOX", CAMEL_STORE_FOLDER_CREATE, error);
	else {
		g_set_error (
			error, CAMEL_STORE_ERROR,
			CAMEL_STORE_ERROR_NO_FOLDER,
			_("Store does not support an INBOX"));
		return NULL;
	}
}

static gchar *
get_name (CamelService *service,
          gboolean brief)
{
	if (brief)
		return g_strdup(service->url->path);
	else
		return g_strdup_printf(((CamelSpoolStore *)service)->type == CAMEL_SPOOL_STORE_MBOX?
				       _("Spool mail file %s"):_("Spool folder tree %s"), service->url->path);
}

/* default implementation, rename all */
static gboolean
rename_folder (CamelStore *store,
               const gchar *old,
               const gchar *new,
               GError **error)
{
	g_set_error (
		error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
		_("Spool folders cannot be renamed"));

	return FALSE;
}

/* default implementation, only delete metadata */
static gboolean
delete_folder (CamelStore *store,
               const gchar *folder_name,
               GError **error)
{
	g_set_error (
		error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
		_("Spool folders cannot be deleted"));

	return FALSE;
}

static void
free_folder_info (CamelStore *store,
                  CamelFolderInfo *fi)
{
	if (fi) {
		g_free(fi->uri);
		g_free(fi->name);
		g_free(fi->full_name);
		g_slice_free(CamelFolderInfo, fi);
	}
}

/* partially copied from mbox */
static void
spool_fill_fi (CamelStore *store,
               CamelFolderInfo *fi,
               guint32 flags)
{
	CamelFolder *folder;

	fi->unread = -1;
	fi->total = -1;
	folder = camel_object_bag_peek (store->folders, fi->full_name);
	if (folder) {
		if ((flags & CAMEL_STORE_FOLDER_INFO_FAST) == 0)
			camel_folder_refresh_info(folder, NULL);
		fi->unread = camel_folder_get_unread_message_count(folder);
		fi->total = camel_folder_get_message_count(folder);
		g_object_unref (folder);
	}
}

static CamelFolderInfo *
spool_new_fi (CamelStore *store,
              CamelFolderInfo *parent,
              CamelFolderInfo **fip,
              const gchar *full,
              guint32 flags)
{
	CamelFolderInfo *fi;
	const gchar *name;
	CamelURL *url;

	name = strrchr(full, '/');
	if (name)
		name++;
	else
		name = full;

	fi = camel_folder_info_new();
	url = camel_url_copy(((CamelService *)store)->url);
	camel_url_set_fragment(url, full);
	fi->uri = camel_url_to_string(url, 0);
	camel_url_free(url);
	fi->full_name = g_strdup(full);
	fi->name = g_strdup(name);
	fi->unread = -1;
	fi->total = -1;
	fi->flags = flags;

	fi->parent = parent;
	fi->next = *fip;
	*fip = fi;

	d(printf("Adding spoold info: '%s' '%s' '%s' '%s'\n", fi->path, fi->name, fi->full_name, fi->url));

	return fi;
}

/* used to find out where we've visited already */
struct _inode {
	dev_t dnode;
	ino_t inode;
};

/* returns number of records found at or below this level */
static gint
scan_dir (CamelStore *store,
          GHashTable *visited,
          gchar *root,
          const gchar *path,
          guint32 flags,
          CamelFolderInfo *parent,
          CamelFolderInfo **fip,
          GError **error)
{
	DIR *dir;
	struct dirent *d;
	gchar *name, *tmp, *fname;
	CamelFolderInfo *fi = NULL;
	struct stat st;
	CamelFolder *folder;
	gchar from[80];
	FILE *fp;

	d(printf("checking dir '%s' part '%s' for mbox content\n", root, path));

	/* look for folders matching the right structure, recursively */
	if (path) {
		name = alloca(strlen(root) + strlen(path) + 2);
		sprintf(name, "%s/%s", root, path);
	} else
		name = root;

	if (g_stat(name, &st) == -1) {
		g_set_error (
			error, G_IO_ERROR,
			g_io_error_from_errno (errno),
			_("Could not scan folder '%s': %s"),
			name, g_strerror (errno));
	} else if (S_ISREG(st.st_mode)) {
		/* incase we start scanning from a file.  messy duplication :-/ */
		if (path) {
			fi = spool_new_fi(store, parent, fip, path, CAMEL_FOLDER_NOINFERIORS|CAMEL_FOLDER_NOCHILDREN);
			spool_fill_fi(store, fi, flags);
		}
		return 0;
	}

	dir = opendir(name);
	if (dir == NULL) {
		g_set_error (
			error, G_IO_ERROR,
			g_io_error_from_errno (errno),
			_("Could not scan folder '%s': %s"),
			name, g_strerror (errno));
		return -1;
	}

	if (path != NULL) {
		fi = spool_new_fi(store, parent, fip, path, CAMEL_FOLDER_NOSELECT);
		fip = &fi->child;
		parent = fi;
	}

	while ((d = readdir(dir))) {
		if (strcmp(d->d_name, ".") == 0
		    || strcmp(d->d_name, "..") == 0)
			continue;

		tmp = g_strdup_printf("%s/%s", name, d->d_name);
		if (g_stat(tmp, &st) == 0) {
			if (path)
				fname = g_strdup_printf("%s/%s", path, d->d_name);
			else
				fname = g_strdup(d->d_name);

			if (S_ISREG(st.st_mode)) {
				gint isfolder = FALSE;

				/* first, see if we already have it open */
				folder = camel_object_bag_peek (store->folders, fname);
				if (folder == NULL) {
					fp = fopen(tmp, "r");
					if (fp != NULL) {
						isfolder = (st.st_size == 0
							    || (fgets(from, sizeof(from), fp) != NULL
								&& strncmp(from, "From ", 5) == 0));
						fclose(fp);
					}
				}

				if (folder != NULL || isfolder) {
					fi = spool_new_fi(store, parent, fip, fname, CAMEL_FOLDER_NOINFERIORS|CAMEL_FOLDER_NOCHILDREN);
					spool_fill_fi(store, fi, flags);
				}
				if (folder)
					g_object_unref (folder);

			} else if (S_ISDIR(st.st_mode)) {
				struct _inode in = { st.st_dev, st.st_ino };

				/* see if we've visited already */
				if (g_hash_table_lookup(visited, &in) == NULL) {
					struct _inode *inew = g_malloc(sizeof(*inew));

					*inew = in;
					g_hash_table_insert(visited, inew, inew);

					if (scan_dir(store, visited, root, fname, flags, parent, fip, error) == -1) {
						g_free(tmp);
						g_free(fname);
						closedir(dir);
						return -1;
					}
				}
			}
			g_free(fname);

		}
		g_free(tmp);
	}
	closedir(dir);

	return 0;
}

static guint
inode_hash (gconstpointer d)
{
	const struct _inode *v = d;

	return v->inode ^ v->dnode;
}

static gboolean
inode_equal (gconstpointer a, gconstpointer b)
{
	const struct _inode *v1 = a, *v2 = b;

	return v1->inode == v2->inode && v1->dnode == v2->dnode;
}

static void
inode_free (gpointer k, gpointer v, gpointer d)
{
	g_free(k);
}

static CamelFolderInfo *
get_folder_info_elm (CamelStore *store,
                     const gchar *top,
                     guint32 flags,
                     GError **error)
{
	CamelFolderInfo *fi = NULL;
	GHashTable *visited;

	visited = g_hash_table_new(inode_hash, inode_equal);

	if (scan_dir(store, visited, ((CamelService *)store)->url->path, top, flags, NULL, &fi, error) == -1 && fi != NULL) {
		camel_store_free_folder_info_full(store, fi);
		fi = NULL;
	}

	g_hash_table_foreach(visited, inode_free, NULL);
	g_hash_table_destroy(visited);

	return fi;
}

static CamelFolderInfo *
get_folder_info_mbox (CamelStore *store,
                      const gchar *top,
                      guint32 flags,
                      GError **error)
{
	CamelFolderInfo *fi = NULL, *fip = NULL;

	if (top == NULL || strcmp(top, "INBOX") == 0) {
		fi = spool_new_fi(store, NULL, &fip, "INBOX", CAMEL_FOLDER_NOINFERIORS|CAMEL_FOLDER_NOCHILDREN|CAMEL_FOLDER_SYSTEM);
		g_free(fi->name);
		fi->name = g_strdup(_("Inbox"));
		spool_fill_fi(store, fi, flags);
	}

	return fi;
}

static CamelFolderInfo *
get_folder_info (CamelStore *store,
                 const gchar *top,
                 guint32 flags,
                 GError **error)
{
	if (((CamelSpoolStore *)store)->type == CAMEL_SPOOL_STORE_MBOX)
		return get_folder_info_mbox (store, top, flags, error);
	else
		return get_folder_info_elm (store, top, flags, error);
}

static gchar *
spool_get_full_path (CamelLocalStore *ls,
                     const gchar *full_name)
{
	if (((CamelSpoolStore *)ls)->type == CAMEL_SPOOL_STORE_MBOX)
		/* a trailing / is always present on toplevel_dir from CamelLocalStore */
		return g_strndup(ls->toplevel_dir, strlen(ls->toplevel_dir)-1);
	else
		return g_strdup_printf("%s/%s", ls->toplevel_dir, full_name);
}

static gchar *
spool_get_meta_path (CamelLocalStore *ls,
                     const gchar *full_name,
                     const gchar *ext)
{
	gchar *root = camel_session_get_storage_path(((CamelService *)ls)->session, (CamelService *)ls, NULL);
	gchar *path, *key;

	if (root == NULL)
		return NULL;

	g_mkdir_with_parents(root, 0700);
	key = camel_file_util_safe_filename(full_name);
	path = g_strdup_printf("%s/%s%s", root, key, ext);
	g_free(key);
	g_free(root);

	return path;
}
