/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * Authors: Michael Zucchi <notzed@ximian.com>
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
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <glib/gstdio.h>

#include "camel-block-file.h"
#include "camel-file-utils.h"
#include "camel-list-utils.h"

#define d(x) /*(printf("%s(%d):%s: ",  __FILE__, __LINE__, __PRETTY_FUNCTION__),(x))*/

/* Locks must be obtained in the order defined */

struct _CamelBlockFilePrivate {
	/* We use the private structure to form our lru list from */
	struct _CamelBlockFilePrivate *next;
	struct _CamelBlockFilePrivate *prev;

	struct _CamelBlockFile *base;

	GStaticMutex root_lock; /* for modifying the root block */
	GStaticMutex cache_lock; /* for refcounting, flag manip, cache manip */
	GStaticMutex io_lock; /* for all io ops */

	guint deleted:1;
};

#define CAMEL_BLOCK_FILE_LOCK(kf, lock) (g_static_mutex_lock(&(kf)->priv->lock))
#define CAMEL_BLOCK_FILE_TRYLOCK(kf, lock) (g_static_mutex_trylock(&(kf)->priv->lock))
#define CAMEL_BLOCK_FILE_UNLOCK(kf, lock) (g_static_mutex_unlock(&(kf)->priv->lock))

#define LOCK(x) g_static_mutex_lock(&x)
#define UNLOCK(x) g_static_mutex_unlock(&x)

static GStaticMutex block_file_lock = G_STATIC_MUTEX_INIT;

/* lru cache of block files */
static CamelDList block_file_list = CAMEL_DLIST_INITIALISER(block_file_list);
/* list to store block files that are actually intialised */
static CamelDList block_file_active_list = CAMEL_DLIST_INITIALISER(block_file_active_list);
static gint block_file_count = 0;
static gint block_file_threshhold = 10;

static gint sync_nolock(CamelBlockFile *bs);
static gint sync_block_nolock(CamelBlockFile *bs, CamelBlock *bl);

G_DEFINE_TYPE (CamelBlockFile, camel_block_file, CAMEL_TYPE_OBJECT)

static gint
block_file_validate_root(CamelBlockFile *bs)
{
	CamelBlockRoot *br;
	struct stat st;
	gint retval;

	br = bs->root;

	retval = fstat (bs->fd, &st);

	d(printf("Validate root: '%s'\n", bs->path));
	d(printf("version: %.8s (%.8s)\n", bs->root->version, bs->version));
	d(printf("block size: %d (%d)%s\n", br->block_size, bs->block_size,
		br->block_size != bs->block_size ? " BAD":" OK"));
	d(printf("free: %ld (%d add size < %ld)%s\n", (glong)br->free, br->free / bs->block_size * bs->block_size, (glong)st.st_size,
		(br->free > st.st_size) || (br->free % bs->block_size) != 0 ? " BAD":" OK"));
	d(printf("last: %ld (%d and size: %ld)%s\n", (glong)br->last, br->last / bs->block_size * bs->block_size, (glong)st.st_size,
		(br->last != st.st_size) || ((br->last % bs->block_size) != 0) ? " BAD": " OK"));
	d(printf("flags: %s\n", (br->flags & CAMEL_BLOCK_FILE_SYNC)?"SYNC":"unSYNC"));

	if (br->last == 0
	    || memcmp(bs->root->version, bs->version, 8) != 0
	    || br->block_size != bs->block_size
	    || (br->free % bs->block_size) != 0
	    || (br->last % bs->block_size) != 0
	    || retval == -1
	    || st.st_size != br->last
	    || br->free > st.st_size
	    || (br->flags & CAMEL_BLOCK_FILE_SYNC) == 0) {
#if 0
		if (retval != -1 && st.st_size > 0) {
			g_warning("Invalid root: '%s'", bs->path);
			g_warning("version: %.8s (%.8s)", bs->root->version, bs->version);
			g_warning("block size: %d (%d)%s", br->block_size, bs->block_size,
				  br->block_size != bs->block_size ? " BAD":" OK");
			g_warning("free: %ld (%d add size < %ld)%s", (glong)br->free, br->free / bs->block_size * bs->block_size, (glong)st.st_size,
				  (br->free > st.st_size) || (br->free % bs->block_size) != 0 ? " BAD":" OK");
			g_warning("last: %ld (%d and size: %ld)%s", (glong)br->last, br->last / bs->block_size * bs->block_size, (glong)st.st_size,
				  (br->last != st.st_size) || ((br->last % bs->block_size) != 0) ? " BAD": " OK");
			g_warning("flags: %s", (br->flags & CAMEL_BLOCK_FILE_SYNC)?"SYNC":"unSYNC");
		}
#endif
		return -1;
	}

	return 0;
}

static gint
block_file_init_root(CamelBlockFile *bs)
{
	CamelBlockRoot *br = bs->root;

	memset(br, 0, bs->block_size);
	memcpy(br->version, bs->version, 8);
	br->last = bs->block_size;
	br->flags = CAMEL_BLOCK_FILE_SYNC;
	br->free = 0;
	br->block_size = bs->block_size;

	return 0;
}

static void
block_file_finalize(GObject *object)
{
	CamelBlockFile *bs = CAMEL_BLOCK_FILE (object);
	CamelBlock *bl, *bn;
	struct _CamelBlockFilePrivate *p;

	p = bs->priv;

	if (bs->root_block)
		camel_block_file_sync(bs);

	/* remove from lru list */
	LOCK(block_file_lock);
	if (bs->fd != -1)
		block_file_count--;
	camel_dlist_remove((CamelDListNode *)p);
	UNLOCK(block_file_lock);

	bl = (CamelBlock *)bs->block_cache.head;
	bn = bl->next;
	while (bn) {
		if (bl->refcount != 0)
			g_warning("Block '%u' still referenced", bl->id);
		g_free(bl);
		bl = bn;
		bn = bn->next;
	}

	g_hash_table_destroy (bs->blocks);

	if (bs->root_block)
		camel_block_file_unref_block(bs, bs->root_block);
	g_free(bs->path);
	if (bs->fd != -1)
		close(bs->fd);

	g_static_mutex_free (&p->io_lock);
	g_static_mutex_free (&p->cache_lock);
	g_static_mutex_free (&p->root_lock);

	g_free(p);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (camel_block_file_parent_class)->finalize (object);
}

static void
camel_block_file_class_init(CamelBlockFileClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = block_file_finalize;

	class->validate_root = block_file_validate_root;
	class->init_root = block_file_init_root;
}

static guint
block_hash_func(gconstpointer v)
{
	return ((camel_block_t) GPOINTER_TO_UINT(v)) >> CAMEL_BLOCK_SIZE_BITS;
}

static void
camel_block_file_init(CamelBlockFile *bs)
{
	struct _CamelBlockFilePrivate *p;

	bs->fd = -1;
	bs->block_size = CAMEL_BLOCK_SIZE;
	camel_dlist_init(&bs->block_cache);
	bs->blocks = g_hash_table_new((GHashFunc)block_hash_func, NULL);
	/* this cache size and the text index size have been tuned for about the best
	   with moderate memory usage.  Doubling the memory usage barely affects performance. */
	bs->block_cache_limit = 256;

	p = bs->priv = g_malloc0(sizeof(*bs->priv));
	p->base = bs;

	g_static_mutex_init (&p->root_lock);
	g_static_mutex_init (&p->cache_lock);
	g_static_mutex_init (&p->io_lock);

	/* link into lru list */
	LOCK(block_file_lock);
	camel_dlist_addhead(&block_file_list, (CamelDListNode *)p);

#if 0
	{
		printf("dumping block list\n");
		printf(" head = %p p = %p\n", block_file_list.head, p);
		p = block_file_list.head;
		while (p->next) {
			printf(" '%s'\n", p->base->path);
			p = p->next;
		}
	}
#endif

	UNLOCK(block_file_lock);
}

/* 'use' a block file for io */
static gint
block_file_use(CamelBlockFile *bs)
{
	struct _CamelBlockFilePrivate *nw, *nn, *p = bs->priv;
	CamelBlockFile *bf;
	gint err;

	/* We want to:
	    remove file from active list
	    lock it

	   Then when done:
	    unlock it
	    add it back to end of active list
	*/

	CAMEL_BLOCK_FILE_LOCK(bs, io_lock);

	if (bs->fd != -1)
		return 0;
	else if (p->deleted) {
		CAMEL_BLOCK_FILE_UNLOCK(bs, io_lock);
		errno = ENOENT;
		return -1;
	} else {
		d(printf("Turning block file online: %s\n", bs->path));
	}

	if ((bs->fd = g_open(bs->path, bs->flags|O_BINARY, 0600)) == -1) {
		err = errno;
		CAMEL_BLOCK_FILE_UNLOCK(bs, io_lock);
		errno = err;
		return -1;
	}

	LOCK(block_file_lock);
	camel_dlist_remove((CamelDListNode *)p);
	camel_dlist_addtail(&block_file_active_list, (CamelDListNode *)p);

	block_file_count++;

	nw = (struct _CamelBlockFilePrivate *)block_file_list.head;
	nn = nw->next;
	while (block_file_count > block_file_threshhold && nn) {
		/* We never hit the current blockfile here, as its removed from the list first */
		bf = nw->base;
		if (bf->fd != -1) {
			/* Need to trylock, as any of these lock levels might be trying
			   to lock the block_file_lock, so we need to check and abort if so */
			if (CAMEL_BLOCK_FILE_TRYLOCK (bf, root_lock)) {
				if (CAMEL_BLOCK_FILE_TRYLOCK (bf, cache_lock)) {
					if (CAMEL_BLOCK_FILE_TRYLOCK (bf, io_lock)) {
						d(printf("[%d] Turning block file offline: %s\n", block_file_count-1, bf->path));
						sync_nolock(bf);
						close(bf->fd);
						bf->fd = -1;
						block_file_count--;
						CAMEL_BLOCK_FILE_UNLOCK(bf, io_lock);
					}
					CAMEL_BLOCK_FILE_UNLOCK(bf, cache_lock);
				}
				CAMEL_BLOCK_FILE_UNLOCK(bf, root_lock);
			}
		}
		nw = nn;
		nn = nw->next;
	}

	UNLOCK(block_file_lock);

	return 0;
}

static void
block_file_unuse(CamelBlockFile *bs)
{
	LOCK(block_file_lock);
	camel_dlist_remove((CamelDListNode *)bs->priv);
	camel_dlist_addtail(&block_file_list, (CamelDListNode *)bs->priv);
	UNLOCK(block_file_lock);

	CAMEL_BLOCK_FILE_UNLOCK(bs, io_lock);
}

/*
o = camel_cache_get(c, key);
camel_cache_unref(c, key);
camel_cache_add(c, key, o);
camel_cache_remove(c, key);
*/

/**
 * camel_block_file_new:
 * @path:
 * @:
 * @block_size:
 *
 * Allocate a new block file, stored at @path.  @version contains an 8 character
 * version string which must match the head of the file, or the file will be
 * intitialised.
 *
 * @block_size is currently ignored and is set to CAMEL_BLOCK_SIZE.
 *
 * Returns: The new block file, or NULL if it could not be created.
 **/
CamelBlockFile *
camel_block_file_new (const gchar *path,
                      gint flags,
                      const gchar version[8],
                      gsize block_size)
{
	CamelBlockFileClass *class;
	CamelBlockFile *bs;

	bs = g_object_new (CAMEL_TYPE_BLOCK_FILE, NULL);
	memcpy(bs->version, version, 8);
	bs->path = g_strdup(path);
	bs->flags = flags;

	bs->root_block = camel_block_file_get_block (bs, 0);
	if (bs->root_block == NULL) {
		g_object_unref (bs);
		return NULL;
	}
	camel_block_file_detach_block(bs, bs->root_block);
	bs->root = (CamelBlockRoot *)&bs->root_block->data;

	/* we only need these flags on first open */
	bs->flags &= ~(O_CREAT|O_EXCL|O_TRUNC);

	class = CAMEL_BLOCK_FILE_GET_CLASS (bs);

	/* Do we need to init the root block? */
	if (class->validate_root(bs) == -1) {
		d(printf("Initialise root block: %.8s\n", version));

		class->init_root(bs);
		camel_block_file_touch_block(bs, bs->root_block);
		if (block_file_use(bs) == -1) {
			g_object_unref (bs);
			return NULL;
		}
		if (sync_block_nolock(bs, bs->root_block) == -1
		    || ftruncate(bs->fd, bs->root->last) == -1) {
			block_file_unuse(bs);
			g_object_unref (bs);
			return NULL;
		}
		block_file_unuse(bs);
	}

	return bs;
}

gint
camel_block_file_rename(CamelBlockFile *bs, const gchar *path)
{
	gint ret;
	struct stat st;
	gint err;

	g_return_val_if_fail (CAMEL_IS_BLOCK_FILE (bs), -1);
	g_return_val_if_fail (path != NULL, -1);

	CAMEL_BLOCK_FILE_LOCK(bs, io_lock);

	ret = g_rename(bs->path, path);
	if (ret == -1) {
		/* Maybe the rename actually worked */
		err = errno;
		if (g_stat(path, &st) == 0
		    && g_stat(bs->path, &st) == -1
		    && errno == ENOENT)
			ret = 0;
		errno = err;
	}

	if (ret != -1) {
		g_free(bs->path);
		bs->path = g_strdup(path);
	}

	CAMEL_BLOCK_FILE_UNLOCK(bs, io_lock);

	return ret;
}

gint
camel_block_file_delete(CamelBlockFile *bs)
{
	gint ret;

	g_return_val_if_fail (CAMEL_IS_BLOCK_FILE (bs), -1);

	CAMEL_BLOCK_FILE_LOCK(bs, io_lock);

	if (bs->fd != -1) {
		LOCK(block_file_lock);
		block_file_count--;
		UNLOCK(block_file_lock);
		close(bs->fd);
		bs->fd = -1;
	}

	bs->priv->deleted = TRUE;
	ret = g_unlink(bs->path);

	CAMEL_BLOCK_FILE_UNLOCK(bs, io_lock);

	return ret;

}

/**
 * camel_block_file_new_block:
 * @bs:
 *
 * Allocate a new block, return a pointer to it.  Old blocks
 * may be flushed to disk during this call.
 *
 * Returns: The block, or NULL if an error occured.
 **/
CamelBlock *
camel_block_file_new_block (CamelBlockFile *bs)
{
	CamelBlock *bl;

	g_return_val_if_fail (CAMEL_IS_BLOCK_FILE (bs), NULL);

	CAMEL_BLOCK_FILE_LOCK(bs, root_lock);

	if (bs->root->free) {
		bl = camel_block_file_get_block (bs, bs->root->free);
		if (bl == NULL)
			goto fail;
		bs->root->free = ((camel_block_t *)bl->data)[0];
	} else {
		bl = camel_block_file_get_block (bs, bs->root->last);
		if (bl == NULL)
			goto fail;
		bs->root->last += CAMEL_BLOCK_SIZE;
	}

	bs->root_block->flags |= CAMEL_BLOCK_DIRTY;

	bl->flags |= CAMEL_BLOCK_DIRTY;
	memset(bl->data, 0, CAMEL_BLOCK_SIZE);
fail:
	CAMEL_BLOCK_FILE_UNLOCK(bs, root_lock);

	return bl;
}

/**
 * camel_block_file_free_block:
 * @bs:
 * @id:
 *
 *
 **/
gint
camel_block_file_free_block (CamelBlockFile *bs,
                             camel_block_t id)
{
	CamelBlock *bl;

	g_return_val_if_fail (CAMEL_IS_BLOCK_FILE (bs), -1);

	bl = camel_block_file_get_block (bs, id);
	if (bl == NULL)
		return -1;

	CAMEL_BLOCK_FILE_LOCK(bs, root_lock);

	((camel_block_t *)bl->data)[0] = bs->root->free;
	bs->root->free = bl->id;
	bs->root_block->flags |= CAMEL_BLOCK_DIRTY;
	bl->flags |= CAMEL_BLOCK_DIRTY;
	camel_block_file_unref_block(bs, bl);

	CAMEL_BLOCK_FILE_UNLOCK(bs, root_lock);

	return 0;
}

/**
 * camel_block_file_get_block:
 * @bs:
 * @id:
 *
 * Retreive a block @id.
 *
 * Returns: The block, or NULL if blockid is invalid or a file error
 * occured.
 **/
CamelBlock *
camel_block_file_get_block (CamelBlockFile *bs,
                            camel_block_t id)
{
	CamelBlock *bl, *flush, *prev;

	g_return_val_if_fail (CAMEL_IS_BLOCK_FILE (bs), NULL);

	/* Sanity check: Dont allow reading of root block (except before its been read)
	   or blocks with invalid block id's */
	if ((bs->root == NULL && id != 0)
	    || (bs->root != NULL && (id > bs->root->last || id == 0))
	    || (id % bs->block_size) != 0) {
		errno = EINVAL;
		return NULL;
	}

	CAMEL_BLOCK_FILE_LOCK(bs, cache_lock);

	bl = g_hash_table_lookup(bs->blocks, GUINT_TO_POINTER(id));

	d(printf("Get  block %08x: %s\n", id, bl?"cached":"must read"));

	if (bl == NULL) {
		/* LOCK io_lock */
		if (block_file_use(bs) == -1) {
			CAMEL_BLOCK_FILE_UNLOCK(bs, cache_lock);
			return NULL;
		}

		bl = g_malloc0(sizeof(*bl));
		bl->id = id;
		if (lseek(bs->fd, id, SEEK_SET) == -1 ||
		    camel_read (bs->fd, (gchar *) bl->data, CAMEL_BLOCK_SIZE, NULL) == -1) {
			block_file_unuse(bs);
			CAMEL_BLOCK_FILE_UNLOCK(bs, cache_lock);
			g_free(bl);
			return NULL;
		}

		bs->block_cache_count++;
		g_hash_table_insert(bs->blocks, GUINT_TO_POINTER(bl->id), bl);

		/* flush old blocks */
		flush = (CamelBlock *)bs->block_cache.tailpred;
		prev = flush->prev;
		while (bs->block_cache_count > bs->block_cache_limit && prev) {
			if (flush->refcount == 0) {
				if (sync_block_nolock(bs, flush) != -1) {
					g_hash_table_remove(bs->blocks, GUINT_TO_POINTER(flush->id));
					camel_dlist_remove((CamelDListNode *)flush);
					g_free(flush);
					bs->block_cache_count--;
				}
			}
			flush = prev;
			prev = prev->prev;
		}
		/* UNLOCK io_lock */
		block_file_unuse(bs);
	} else {
		camel_dlist_remove((CamelDListNode *)bl);
	}

	camel_dlist_addhead(&bs->block_cache, (CamelDListNode *)bl);
	bl->refcount++;

	CAMEL_BLOCK_FILE_UNLOCK(bs, cache_lock);

	d(printf("Got  block %08x\n", id));

	return bl;
}

/**
 * camel_block_file_detach_block:
 * @bs:
 * @bl:
 *
 * Detatch a block from the block file's cache.  The block should
 * be unref'd or attached when finished with.  The block file will
 * perform no writes of this block or flushing of it if the cache
 * fills.
 **/
void
camel_block_file_detach_block(CamelBlockFile *bs, CamelBlock *bl)
{
	g_return_if_fail (CAMEL_IS_BLOCK_FILE (bs));
	g_return_if_fail (bl != NULL);

	CAMEL_BLOCK_FILE_LOCK(bs, cache_lock);

	g_hash_table_remove(bs->blocks, GUINT_TO_POINTER(bl->id));
	camel_dlist_remove((CamelDListNode *)bl);
	bl->flags |= CAMEL_BLOCK_DETACHED;

	CAMEL_BLOCK_FILE_UNLOCK(bs, cache_lock);
}

/**
 * camel_block_file_attach_block:
 * @bs:
 * @bl:
 *
 * Reattach a block that has been detached.
 **/
void
camel_block_file_attach_block(CamelBlockFile *bs, CamelBlock *bl)
{
	g_return_if_fail (CAMEL_IS_BLOCK_FILE (bs));
	g_return_if_fail (bl != NULL);

	CAMEL_BLOCK_FILE_LOCK(bs, cache_lock);

	g_hash_table_insert(bs->blocks, GUINT_TO_POINTER(bl->id), bl);
	camel_dlist_addtail(&bs->block_cache, (CamelDListNode *)bl);
	bl->flags &= ~CAMEL_BLOCK_DETACHED;

	CAMEL_BLOCK_FILE_UNLOCK(bs, cache_lock);
}

/**
 * camel_block_file_touch_block:
 * @bs:
 * @bl:
 *
 * Mark a block as dirty.  The block will be written to disk if
 * it ever expires from the cache.
 **/
void
camel_block_file_touch_block(CamelBlockFile *bs, CamelBlock *bl)
{
	g_return_if_fail (CAMEL_IS_BLOCK_FILE (bs));
	g_return_if_fail (bl != NULL);

	CAMEL_BLOCK_FILE_LOCK(bs, root_lock);
	CAMEL_BLOCK_FILE_LOCK(bs, cache_lock);

	bl->flags |= CAMEL_BLOCK_DIRTY;

	if ((bs->root->flags & CAMEL_BLOCK_FILE_SYNC) && bl != bs->root_block) {
		d(printf("turning off sync flag\n"));
		bs->root->flags &= ~CAMEL_BLOCK_FILE_SYNC;
		bs->root_block->flags |= CAMEL_BLOCK_DIRTY;
		camel_block_file_sync_block(bs, bs->root_block);
	}

	CAMEL_BLOCK_FILE_UNLOCK(bs, cache_lock);
	CAMEL_BLOCK_FILE_UNLOCK(bs, root_lock);
}

/**
 * camel_block_file_unref_block:
 * @bs:
 * @bl:
 *
 * Mark a block as unused.  If a block is used it will not be
 * written to disk, or flushed from memory.
 *
 * If a block is detatched and this is the last reference, the
 * block will be freed.
 **/
void
camel_block_file_unref_block(CamelBlockFile *bs, CamelBlock *bl)
{
	g_return_if_fail (CAMEL_IS_BLOCK_FILE (bs));
	g_return_if_fail (bl != NULL);

	CAMEL_BLOCK_FILE_LOCK(bs, cache_lock);

	if (bl->refcount == 1 && (bl->flags & CAMEL_BLOCK_DETACHED))
		g_free(bl);
	else
		bl->refcount--;

	CAMEL_BLOCK_FILE_UNLOCK(bs, cache_lock);
}

static gint
sync_block_nolock(CamelBlockFile *bs, CamelBlock *bl)
{
	d(printf("Sync block %08x: %s\n", bl->id, (bl->flags & CAMEL_BLOCK_DIRTY)?"dirty":"clean"));

	if (bl->flags & CAMEL_BLOCK_DIRTY) {
		if (lseek(bs->fd, bl->id, SEEK_SET) == -1
		    || write(bs->fd, bl->data, CAMEL_BLOCK_SIZE) != CAMEL_BLOCK_SIZE) {
			return -1;
		}
		bl->flags &= ~CAMEL_BLOCK_DIRTY;
	}

	return 0;
}

static gint
sync_nolock(CamelBlockFile *bs)
{
	CamelBlock *bl, *bn;
	gint work = FALSE;

	bl = (CamelBlock *)bs->block_cache.head;
	bn = bl->next;
	while (bn) {
		if (bl->flags & CAMEL_BLOCK_DIRTY) {
			work = TRUE;
			if (sync_block_nolock(bs, bl) == -1)
				return -1;
		}
		bl = bn;
		bn = bn->next;
	}

	if (!work
	    && (bs->root_block->flags & CAMEL_BLOCK_DIRTY) == 0
	    && (bs->root->flags & CAMEL_BLOCK_FILE_SYNC) != 0)
		return 0;

	d(printf("turning on sync flag\n"));

	bs->root->flags |= CAMEL_BLOCK_FILE_SYNC;
	bs->root_block->flags |= CAMEL_BLOCK_DIRTY;

	return sync_block_nolock(bs, bs->root_block);
}

/**
 * camel_block_file_sync_block:
 * @bs:
 * @bl:
 *
 * Flush a block to disk immediately.  The block will only
 * be flushed to disk if it is marked as dirty (touched).
 *
 * Returns: -1 on io error.
 **/
gint
camel_block_file_sync_block(CamelBlockFile *bs, CamelBlock *bl)
{
	gint ret;

	g_return_val_if_fail (CAMEL_IS_BLOCK_FILE (bs), -1);
	g_return_val_if_fail (bl != NULL, -1);

	/* LOCK io_lock */
	if (block_file_use(bs) == -1)
		return -1;

	ret = sync_block_nolock(bs, bl);

	block_file_unuse(bs);

	return ret;
}

/**
 * camel_block_file_sync:
 * @bs:
 *
 * Sync all dirty blocks to disk, including the root block.
 *
 * Returns: -1 on io error.
 **/
gint
camel_block_file_sync(CamelBlockFile *bs)
{
	gint ret;

	g_return_val_if_fail (CAMEL_IS_BLOCK_FILE (bs), -1);

	CAMEL_BLOCK_FILE_LOCK(bs, root_lock);
	CAMEL_BLOCK_FILE_LOCK(bs, cache_lock);

	/* LOCK io_lock */
	if (block_file_use(bs) == -1)
		ret = -1;
	else {
		ret = sync_nolock(bs);
		block_file_unuse(bs);
	}

	CAMEL_BLOCK_FILE_UNLOCK(bs, cache_lock);
	CAMEL_BLOCK_FILE_UNLOCK(bs, root_lock);

	return ret;
}

/* ********************************************************************** */

struct _CamelKeyFilePrivate {
	struct _CamelKeyFilePrivate *next;
	struct _CamelKeyFilePrivate *prev;

	struct _CamelKeyFile *base;
	GStaticMutex lock;
	guint deleted:1;
};

#define CAMEL_KEY_FILE_LOCK(kf, lock) (g_static_mutex_lock(&(kf)->priv->lock))
#define CAMEL_KEY_FILE_TRYLOCK(kf, lock) (g_static_mutex_trylock(&(kf)->priv->lock))
#define CAMEL_KEY_FILE_UNLOCK(kf, lock) (g_static_mutex_unlock(&(kf)->priv->lock))

static GStaticMutex key_file_lock = G_STATIC_MUTEX_INIT;

/* lru cache of block files */
static CamelDList key_file_list = CAMEL_DLIST_INITIALISER(key_file_list);
static CamelDList key_file_active_list = CAMEL_DLIST_INITIALISER(key_file_active_list);
static gint key_file_count = 0;
static const gint key_file_threshhold = 10;

G_DEFINE_TYPE (CamelKeyFile, camel_key_file, CAMEL_TYPE_OBJECT)

static void
key_file_finalize(GObject *object)
{
	CamelKeyFile *bs = CAMEL_KEY_FILE (object);
	struct _CamelKeyFilePrivate *p = bs->priv;

	LOCK(key_file_lock);
	camel_dlist_remove((CamelDListNode *)p);

	if (bs-> fp) {
		key_file_count--;
		fclose(bs->fp);
	}

	UNLOCK(key_file_lock);

	g_free(bs->path);

	g_static_mutex_free (&p->lock);

	g_free(p);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (camel_key_file_parent_class)->finalize (object);
}

static void
camel_key_file_class_init(CamelKeyFileClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = key_file_finalize;
}

static void
camel_key_file_init(CamelKeyFile *bs)
{
	struct _CamelKeyFilePrivate *p;

	p = bs->priv = g_malloc0(sizeof(*bs->priv));
	p->base = bs;

	g_static_mutex_init (&p->lock);

	LOCK(key_file_lock);
	camel_dlist_addhead(&key_file_list, (CamelDListNode *)p);
	UNLOCK(key_file_lock);
}

/* 'use' a key file for io */
static gint
key_file_use(CamelKeyFile *bs)
{
	struct _CamelKeyFilePrivate *nw, *nn, *p = bs->priv;
	CamelKeyFile *bf;
	gint err, fd;
	const gchar *flag;

	/* We want to:
	    remove file from active list
	    lock it

	   Then when done:
	    unlock it
	    add it back to end of active list
	*/

	/* TODO: Check header on reset? */

	CAMEL_KEY_FILE_LOCK(bs, lock);

	if (bs->fp != NULL)
		return 0;
	else if (p->deleted) {
		CAMEL_KEY_FILE_UNLOCK(bs, lock);
		errno = ENOENT;
		return -1;
	} else {
		d(printf("Turning key file online: '%s'\n", bs->path));
	}

	if ((bs->flags & O_ACCMODE) == O_RDONLY)
		flag = "rb";
	else
		flag = "a+b";

	if ((fd = g_open(bs->path, bs->flags|O_BINARY, 0600)) == -1
	    || (bs->fp = fdopen(fd, flag)) == NULL) {
		err = errno;
		if (fd != -1)
			close(fd);
		CAMEL_KEY_FILE_UNLOCK(bs, lock);
		errno = err;
		return -1;
	}

	LOCK(key_file_lock);
	camel_dlist_remove((CamelDListNode *)p);
	camel_dlist_addtail(&key_file_active_list, (CamelDListNode *)p);

	key_file_count++;

	nw = (struct _CamelKeyFilePrivate *)key_file_list.head;
	nn = nw->next;
	while (key_file_count > key_file_threshhold && nn) {
		/* We never hit the current keyfile here, as its removed from the list first */
		bf = nw->base;
		if (bf->fp != NULL) {
			/* Need to trylock, as any of these lock levels might be trying
			   to lock the key_file_lock, so we need to check and abort if so */
			if (CAMEL_BLOCK_FILE_TRYLOCK (bf, lock)) {
				d(printf("Turning key file offline: %s\n", bf->path));
				fclose(bf->fp);
				bf->fp = NULL;
				key_file_count--;
				CAMEL_BLOCK_FILE_UNLOCK(bf, lock);
			}
		}
		nw = nn;
		nn = nw->next;
	}

	UNLOCK(key_file_lock);

	return 0;
}

static void
key_file_unuse(CamelKeyFile *bs)
{
	LOCK(key_file_lock);
	camel_dlist_remove((CamelDListNode *)bs->priv);
	camel_dlist_addtail(&key_file_list, (CamelDListNode *)bs->priv);
	UNLOCK(key_file_lock);

	CAMEL_KEY_FILE_UNLOCK(bs, lock);
}

/**
 * camel_key_file_new:
 * @path:
 * @flags: open flags
 * @version: Version string (header) of file.  Currently
 * written but not checked.
 *
 * Create a new key file.  A linked list of record blocks.
 *
 * Returns: A new key file, or NULL if the file could not
 * be opened/created/initialised.
 **/
CamelKeyFile *
camel_key_file_new(const gchar *path, gint flags, const gchar version[8])
{
	CamelKeyFile *kf;
	goffset last;
	gint err;

	d(printf("New key file '%s'\n", path));

	kf = g_object_new (CAMEL_TYPE_KEY_FILE, NULL);
	kf->path = g_strdup(path);
	kf->fp = NULL;
	kf->flags = flags;
	kf->last = 8;

	if (key_file_use(kf) == -1) {
		g_object_unref (kf);
		kf = NULL;
	} else {
		fseek(kf->fp, 0, SEEK_END);
		last = ftell(kf->fp);
		if (last == 0) {
			fwrite(version, 8, 1, kf->fp);
			last += 8;
		}
		kf->last = last;

		err = ferror(kf->fp);
		key_file_unuse(kf);

		/* we only need these flags on first open */
		kf->flags &= ~(O_CREAT|O_EXCL|O_TRUNC);

		if (err) {
			g_object_unref (kf);
			kf = NULL;
		}
	}

	return kf;
}

gint
camel_key_file_rename(CamelKeyFile *kf, const gchar *path)
{
	gint ret;
	struct stat st;
	gint err;

	g_return_val_if_fail (CAMEL_IS_KEY_FILE (kf), -1);
	g_return_val_if_fail (path != NULL, -1);

	CAMEL_KEY_FILE_LOCK(kf, lock);

	ret = g_rename(kf->path, path);
	if (ret == -1) {
		/* Maybe the rename actually worked */
		err = errno;
		if (g_stat(path, &st) == 0
		    && g_stat(kf->path, &st) == -1
		    && errno == ENOENT)
			ret = 0;
		errno = err;
	}

	if (ret != -1) {
		g_free(kf->path);
		kf->path = g_strdup(path);
	}

	CAMEL_KEY_FILE_UNLOCK(kf, lock);

	return ret;
}

gint
camel_key_file_delete(CamelKeyFile *kf)
{
	gint ret;

	g_return_val_if_fail (CAMEL_IS_KEY_FILE (kf), -1);

	CAMEL_KEY_FILE_LOCK(kf, lock);

	if (kf->fp) {
		LOCK(key_file_lock);
		key_file_count--;
		UNLOCK(key_file_lock);
		fclose(kf->fp);
		kf->fp = NULL;
	}

	kf->priv->deleted = TRUE;
	ret = g_unlink(kf->path);

	CAMEL_KEY_FILE_UNLOCK(kf, lock);

	return ret;

}

/**
 * camel_key_file_write:
 * @kf:
 * @parent:
 * @len:
 * @records:
 *
 * Write a new list of records to the key file.
 *
 * Returns: -1 on io error.  The key file will remain unchanged.
 **/
gint
camel_key_file_write(CamelKeyFile *kf, camel_block_t *parent, gsize len, camel_key_t *records)
{
	camel_block_t next;
	guint32 size;
	gint ret = -1;

	g_return_val_if_fail (CAMEL_IS_KEY_FILE (kf), -1);
	g_return_val_if_fail (parent != NULL, -1);
	g_return_val_if_fail (records != NULL, -1);

	d(printf("write key %08x len = %d\n", *parent, len));

	if (len == 0) {
		d(printf(" new parent = %08x\n", *parent));
		return 0;
	}

	/* LOCK */
	if (key_file_use(kf) == -1)
		return -1;

	size = len;

	/* FIXME: Use io util functions? */
	next = kf->last;
	fseek(kf->fp, kf->last, SEEK_SET);
	fwrite(parent, sizeof(*parent), 1, kf->fp);
	fwrite(&size, sizeof(size), 1, kf->fp);
	fwrite(records, sizeof(records[0]), len, kf->fp);

	if (ferror(kf->fp)) {
		clearerr(kf->fp);
	} else {
		kf->last = ftell(kf->fp);
		*parent = next;
		ret = len;
	}

	/* UNLOCK */
	key_file_unuse(kf);

	d(printf(" new parent = %08x\n", *parent));

	return ret;
}

/**
 * camel_key_file_read:
 * @kf:
 * @start: The record pointer.  This will be set to the next record pointer on success.
 * @len: Number of records read, if != NULL.
 * @records: Records, allocated, must be freed with g_free, if != NULL.
 *
 * Read the next block of data from the key file.  Returns the number of
 * records.
 *
 * Returns: -1 on io error.
 **/
gint
camel_key_file_read(CamelKeyFile *kf, camel_block_t *start, gsize *len, camel_key_t **records)
{
	guint32 size;
	glong pos;
	camel_block_t next;
	gint ret = -1;

	g_return_val_if_fail (CAMEL_IS_KEY_FILE (kf), -1);
	g_return_val_if_fail (start != NULL, -1);

	pos = *start;
	if (pos == 0)
		return 0;

	/* LOCK */
	if (key_file_use(kf) == -1)
		return -1;

	if (fseek(kf->fp, pos, SEEK_SET) == -1
	    || fread(&next, sizeof(next), 1, kf->fp) != 1
	    || fread(&size, sizeof(size), 1, kf->fp) != 1
	    || size > 1024) {
		clearerr(kf->fp);
		goto fail;
	}

	if (len)
		*len = size;

	if (records) {
		camel_key_t *keys = g_malloc(size * sizeof(camel_key_t));

		if (fread(keys, sizeof(camel_key_t), size, kf->fp) != size) {
			g_free(keys);
			goto fail;
		}
		*records = keys;
	}

	*start = next;

	ret = 0;
fail:
	/* UNLOCK */
	key_file_unuse(kf);

	return ret;
}
