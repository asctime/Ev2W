/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-message-cache.c: Class for a Camel cache.
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

#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>

#include "camel-data-cache.h"
#include "camel-object-bag.h"
#include "camel-stream-fs.h"
#include "camel-stream-mem.h"
#include "camel-file-utils.h"

#define d(x)

/* how many 'bits' of hash are used to key the toplevel directory */
#define CAMEL_DATA_CACHE_BITS (6)
#define CAMEL_DATA_CACHE_MASK ((1<<CAMEL_DATA_CACHE_BITS)-1)

/* timeout before a cache dir is checked again for expired entries,
   once an hour should be enough */
#define CAMEL_DATA_CACHE_CYCLE_TIME (60*60)

#define CAMEL_DATA_CACHE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), CAMEL_TYPE_DATA_CACHE, CamelDataCachePrivate))

struct _CamelDataCachePrivate {
	CamelObjectBag *busy_bag;

	gchar *path;

	time_t expire_age;
	time_t expire_access;

	time_t expire_last[1<<CAMEL_DATA_CACHE_BITS];
};

enum {
	PROP_0,
	PROP_PATH
};

G_DEFINE_TYPE (CamelDataCache, camel_data_cache, CAMEL_TYPE_OBJECT)

static void
data_cache_set_property (GObject *object,
                         guint property_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_PATH:
			camel_data_cache_set_path (
				CAMEL_DATA_CACHE (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
data_cache_get_property (GObject *object,
                         guint property_id,
                         GValue *value,
                         GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_PATH:
			g_value_set_string (
				value, camel_data_cache_get_path (
				CAMEL_DATA_CACHE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
data_cache_finalize (GObject *object)
{
	CamelDataCachePrivate *priv;

	priv = CAMEL_DATA_CACHE_GET_PRIVATE (object);

	camel_object_bag_destroy (priv->busy_bag);
	g_free (priv->path);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (camel_data_cache_parent_class)->finalize (object);
}

static void
camel_data_cache_class_init (CamelDataCacheClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (CamelDataCachePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = data_cache_set_property;
	object_class->get_property = data_cache_get_property;
	object_class->finalize = data_cache_finalize;

	g_object_class_install_property (
		object_class,
		PROP_PATH,
		g_param_spec_string (
			"path",
			"Path",
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));
}

static void
camel_data_cache_init (CamelDataCache *data_cache)
{
	CamelObjectBag *busy_bag;

	busy_bag = camel_object_bag_new (
		g_str_hash, g_str_equal,
		(CamelCopyFunc) g_strdup,
		(GFreeFunc) g_free);

	data_cache->priv = CAMEL_DATA_CACHE_GET_PRIVATE (data_cache);
	data_cache->priv->busy_bag = busy_bag;
	data_cache->priv->expire_age = -1;
	data_cache->priv->expire_access = -1;
}

/**
 * camel_data_cache_new:
 * @path: Base path of cache, subdirectories will be created here.
 * @error: return location for a #GError, or %NULL
 *
 * Create a new data cache.
 *
 * Returns: A new cache object, or NULL if the base path cannot
 * be written to.
 **/
CamelDataCache *
camel_data_cache_new (const gchar *path,
                      GError **error)
{
	g_return_val_if_fail (path != NULL, NULL);

	if (g_mkdir_with_parents (path, 0700) == -1) {
		g_set_error (
			error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			_("Unable to create cache path"));
		return NULL;
	}

	return g_object_new (CAMEL_TYPE_DATA_CACHE, "path", path, NULL);
}

/**
 * camel_data_cache_get_path:
 * @cdc: a #CamelDataCache
 *
 * Returns the path to the data cache.
 *
 * Returns: the path to the data cache
 *
 * Since: 2.32
 **/
const gchar *
camel_data_cache_get_path (CamelDataCache *cdc)
{
	g_return_val_if_fail (CAMEL_IS_DATA_CACHE (cdc), NULL);

	return cdc->priv->path;
}

/**
 * camel_data_cache_set_path:
 * @cdc: a #CamelDataCache
 * @path: path to the data cache
 *
 * Sets the path to the data cache.
 *
 * Since: 2.32
 **/
void
camel_data_cache_set_path (CamelDataCache *cdc,
                           const gchar *path)
{
	g_return_if_fail (CAMEL_IS_DATA_CACHE (cdc));
	g_return_if_fail (path != NULL);

	g_free (cdc->priv->path);
	cdc->priv->path = g_strdup (path);

	g_object_notify (G_OBJECT (cdc), "path");
}

/**
 * camel_data_cache_set_expire_age:
 * @cdc: A #CamelDataCache
 * @when: Timeout for age expiry, or -1 to disable.
 *
 * Set the cache expiration policy for aged entries.
 *
 * Items in the cache older than @when seconds may be
 * flushed at any time.  Items are expired in a lazy
 * manner, so it is indeterminate when the items will
 * physically be removed.
 *
 * Note you can set both an age and an access limit.  The
 * age acts as a hard limit on cache entries.
 **/
void
camel_data_cache_set_expire_age(CamelDataCache *cdc, time_t when)
{
	cdc->priv->expire_age = when;
}

/**
 * camel_data_cache_set_expire_access:
 * @cdc: A #CamelDataCache
 * @when: Timeout for access, or -1 to disable access expiry.
 *
 * Set the cache expiration policy for access times.
 *
 * Items in the cache which haven't been accessed for @when
 * seconds may be expired at any time.  Items are expired in a lazy
 * manner, so it is indeterminate when the items will
 * physically be removed.
 *
 * Note you can set both an age and an access limit.  The
 * age acts as a hard limit on cache entries.
 **/
void
camel_data_cache_set_expire_access(CamelDataCache *cdc, time_t when)
{
	cdc->priv->expire_access = when;
}

static void
data_cache_expire(CamelDataCache *cdc, const gchar *path, const gchar *keep, time_t now)
{
	GDir *dir;
	const gchar *dname;
	GString *s;
	struct stat st;
	CamelStream *stream;

	dir = g_dir_open(path, 0, NULL);
	if (dir == NULL)
		return;

	s = g_string_new("");
	while ((dname = g_dir_read_name(dir))) {
		if (strcmp(dname, keep) == 0)
			continue;

		g_string_printf (s, "%s/%s", path, dname);
		if (g_stat(s->str, &st) == 0
		    && S_ISREG(st.st_mode)
		    && ((cdc->priv->expire_age != -1 && st.st_mtime + cdc->priv->expire_age < now)
			|| (cdc->priv->expire_access != -1 && st.st_atime + cdc->priv->expire_access < now))) {
			g_unlink(s->str);
			stream = camel_object_bag_get(cdc->priv->busy_bag, s->str);
			if (stream) {
				camel_object_bag_remove(cdc->priv->busy_bag, stream);
				g_object_unref (stream);
			}
		}
	}
	g_string_free(s, TRUE);
	g_dir_close(dir);
}

/* Since we have to stat the directory anyway, we use this opportunity to
   lazily expire old data.
   If it is this directories 'turn', and we haven't done it for CYCLE_TIME seconds,
   then we perform an expiry run */
static gchar *
data_cache_path(CamelDataCache *cdc, gint create, const gchar *path, const gchar *key)
{
	gchar *dir, *real, *tmp;
	guint32 hash;

	hash = g_str_hash(key);
	hash = (hash>>5)&CAMEL_DATA_CACHE_MASK;
	dir = alloca(strlen(cdc->priv->path) + strlen(path) + 8);
	sprintf(dir, "%s/%s/%02x", cdc->priv->path, path, hash);

#ifdef G_OS_WIN32
	if (g_access(dir, F_OK) == -1) {
#else
	if (access (dir, F_OK) == -1) {
#endif
		if (create)
			g_mkdir_with_parents (dir, 0700);
	} else if (cdc->priv->expire_age != -1 || cdc->priv->expire_access != -1) {
		time_t now;

		/* This has a race, but at worst we re-run an expire cycle which is safe */
		now = time(NULL);
		if (cdc->priv->expire_last[hash] + CAMEL_DATA_CACHE_CYCLE_TIME < now) {
			cdc->priv->expire_last[hash] = now;
			data_cache_expire(cdc, dir, key, now);
		}
	}

	tmp = camel_file_util_safe_filename(key);
	real = g_strdup_printf("%s/%s", dir, tmp);
	g_free(tmp);

	return real;
}

/**
 * camel_data_cache_add:
 * @cdc: A #CamelDataCache
 * @path: Relative path of item to add.
 * @key: Key of item to add.
 * @error: return location for a #GError, or %NULL
 *
 * Add a new item to the cache.
 *
 * The key and the path combine to form a unique key used to store
 * the item.
 *
 * Potentially, expiry processing will be performed while this call
 * is executing.
 *
 * Returns: A CamelStream (file) opened in read-write mode.
 * The caller must unref this when finished.
 **/
CamelStream *
camel_data_cache_add (CamelDataCache *cdc,
                      const gchar *path,
                      const gchar *key,
                      GError **error)
{
	gchar *real;
	CamelStream *stream;

	real = data_cache_path(cdc, TRUE, path, key);
	/* need to loop 'cause otherwise we can call bag_add/bag_abort
	 * after bag_reserve returned a pointer, which is an invalid
	 * sequence. */
	do {
		stream = camel_object_bag_reserve(cdc->priv->busy_bag, real);
		if (stream) {
			g_unlink(real);
			camel_object_bag_remove(cdc->priv->busy_bag, stream);
			g_object_unref (stream);
		}
	} while (stream != NULL);

	stream = camel_stream_fs_new_with_name (
		real, O_RDWR|O_CREAT|O_TRUNC, 0600, error);
	if (stream)
		camel_object_bag_add(cdc->priv->busy_bag, real, stream);
	else
		camel_object_bag_abort(cdc->priv->busy_bag, real);

	g_free(real);

	return stream;
}

/**
 * camel_data_cache_get:
 * @cdc: A #CamelDataCache
 * @path: Path to the (sub) cache the item exists in.
 * @key: Key for the cache item.
 * @error: return location for a #GError, or %NULL
 *
 * Lookup an item in the cache.  If the item exists, a stream
 * is returned for the item.  The stream may be shared by
 * multiple callers, so ensure the stream is in a valid state
 * through external locking.
 *
 * Returns: A cache item, or NULL if the cache item does not exist.
 **/
CamelStream *
camel_data_cache_get (CamelDataCache *cdc,
                      const gchar *path,
                      const gchar *key,
                      GError **error)
{
	gchar *real;
	CamelStream *stream;

	real = data_cache_path(cdc, FALSE, path, key);
	stream = camel_object_bag_reserve(cdc->priv->busy_bag, real);
	if (!stream) {
		struct stat st;

		/* Return NULL if the file is empty. */
		if (g_stat (real, &st) == 0 && st.st_size > 0)
			stream = camel_stream_fs_new_with_name (
				real, O_RDWR, 0600, error);

		if (stream)
			camel_object_bag_add(cdc->priv->busy_bag, real, stream);
		else
			camel_object_bag_abort(cdc->priv->busy_bag, real);
	}
	g_free(real);

	return stream;
}

/**
 * camel_data_cache_get_filename:
 * @cdc: A #CamelDataCache
 * @path: Path to the (sub) cache the item exists in.
 * @key: Key for the cache item.
 * @error: return location for a #GError, or %NULL
 *
 * Lookup the filename for an item in the cache
 *
 * Returns: The filename for a cache item
 *
 * Since: 2.26
 **/
gchar *
camel_data_cache_get_filename (CamelDataCache *cdc,
                               const gchar *path,
                               const gchar *key,
                               GError **error)
{
	gchar *real;

	real = data_cache_path(cdc, FALSE, path, key);

	return real;
}

/**
 * camel_data_cache_remove:
 * @cdc: A #CamelDataCache
 * @path:
 * @key:
 * @error: return location for a #GError, or %NULL
 *
 * Remove/expire a cache item.
 *
 * Returns:
 **/
gint
camel_data_cache_remove (CamelDataCache *cdc,
                         const gchar *path,
                         const gchar *key,
                         GError **error)
{
	CamelStream *stream;
	gchar *real;
	gint ret;

	real = data_cache_path(cdc, FALSE, path, key);
	stream = camel_object_bag_get(cdc->priv->busy_bag, real);
	if (stream) {
		camel_object_bag_remove(cdc->priv->busy_bag, stream);
		g_object_unref (stream);
	}

	/* maybe we were a mem stream */
	if (g_unlink (real) == -1 && errno != ENOENT) {
		g_set_error (
			error, G_IO_ERROR,
			g_io_error_from_errno (errno),
			_("Could not remove cache entry: %s: %s"),
			real, g_strerror (errno));
		ret = -1;
	} else {
		ret = 0;
	}

	g_free(real);

	return ret;
}

