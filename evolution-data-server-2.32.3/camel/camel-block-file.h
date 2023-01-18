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

#if !defined (__CAMEL_H_INSIDE__) && !defined (CAMEL_COMPILATION)
#error "Only <camel/camel.h> can be included directly."
#endif

#ifndef CAMEL_BLOCK_FILE_H
#define CAMEL_BLOCK_FILE_H

#include <camel/camel-object.h>
#include <camel/camel-list-utils.h>
#include <stdio.h>
#include <sys/types.h>

/* Standard GObject macros */
#define CAMEL_TYPE_BLOCK_FILE \
	(camel_block_file_get_type ())
#define CAMEL_BLOCK_FILE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), CAMEL_TYPE_BLOCK_FILE, CamelBlockFile))
#define CAMEL_BLOCK_FILE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), CAMEL_TYPE_BLOCK_FILE, CamelBlockFileClass))
#define CAMEL_IS_BLOCK_FILE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), CAMEL_TYPE_BLOCK_FILE))
#define CAMEL_IS_BLOCK_FILE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), CAMEL_TYPE_BLOCK_FILE))
#define CAMEL_BLOCK_FILE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), CAMEL_TYPE_BLOCK_FILE, CamelBlockFileClass))

#define CAMEL_TYPE_KEY_FILE \
	(camel_key_file_get_type ())
#define CAMEL_KEY_FILE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), CAMEL_TYPE_KEY_FILE, CamelKeyFile))
#define CAMEL_KEY_FILE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), CAMEL_TYPE_KEY_FILE, CamelKeyFileClass))
#define CAMEL_IS_KEY_FILE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), CAMEL_TYPE_KEY_FILE))
#define CAMEL_IS_KEY_FILE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), CAMEL_TYPE_KEY_FILE))
#define CAMEL_KEY_FILE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), CAMEL_TYPE_KEY_FILE, CamelKeyFileClass))

G_BEGIN_DECLS

typedef guint32 camel_block_t;	/* block offset, absolute, bottom BLOCK_SIZE_BITS always 0 */
typedef guint32 camel_key_t;	/* this is a bitfield of (block offset:BLOCK_SIZE_BITS) */

typedef struct _CamelBlockRoot CamelBlockRoot;
typedef struct _CamelBlock CamelBlock;
typedef struct _CamelBlockFile CamelBlockFile;
typedef struct _CamelBlockFileClass CamelBlockFileClass;
typedef struct _CamelBlockFilePrivate CamelBlockFilePrivate;

#define CAMEL_BLOCK_FILE_SYNC (1<<0)

#define CAMEL_BLOCK_SIZE (1024)
#define CAMEL_BLOCK_SIZE_BITS (10) /* # bits to contain block_size bytes */

#define CAMEL_BLOCK_DIRTY (1<<0)
#define CAMEL_BLOCK_DETACHED (1<<1)

struct _CamelBlockRoot {
	gchar version[8];	/* version number */

	guint32 flags;		/* flags for file */
	guint32 block_size;	/* block size of this file */
	camel_block_t free;	/* free block list */
	camel_block_t last;	/* pointer to end of blocks */

	/* subclasses tack on, but no more than CAMEL_BLOCK_SIZE! */
};

/* LRU cache of blocks */
struct _CamelBlock {
	struct _CamelBlock *next;
	struct _CamelBlock *prev;

	camel_block_t id;
	guint32 flags;
	guint32 refcount;
	guint32 align00;

	guchar data[CAMEL_BLOCK_SIZE];
};

struct _CamelBlockFile {
	CamelObject parent;
	CamelBlockFilePrivate *priv;

	gchar version[8];
	gchar *path;
	gint flags;

	gint fd;
	gsize block_size;

	CamelBlockRoot *root;
	CamelBlock *root_block;

	/* make private? */
	gint block_cache_limit;
	gint block_cache_count;
	CamelDList block_cache;
	GHashTable *blocks;
};

struct _CamelBlockFileClass {
	CamelObjectClass parent;

	gint (*validate_root)(CamelBlockFile *);
	gint (*init_root)(CamelBlockFile *);
};

GType		camel_block_file_get_type	(void);
CamelBlockFile *camel_block_file_new		(const gchar *path,
						 gint flags,
						 const gchar version[8],
						 gsize block_size);
gint		camel_block_file_rename		(CamelBlockFile *bs,
						 const gchar *path);
gint		camel_block_file_delete		(CamelBlockFile *kf);
CamelBlock *	camel_block_file_new_block	(CamelBlockFile *bs);
gint		camel_block_file_free_block	(CamelBlockFile *bs,
						 camel_block_t id);
CamelBlock *	camel_block_file_get_block	(CamelBlockFile *bs,
						 camel_block_t id);
void		camel_block_file_detach_block	(CamelBlockFile *bs,
						 CamelBlock *bl);
void		camel_block_file_attach_block	(CamelBlockFile *bs,
						 CamelBlock *bl);
void		camel_block_file_touch_block	(CamelBlockFile *bs,
						 CamelBlock *bl);
void		camel_block_file_unref_block	(CamelBlockFile *bs,
						 CamelBlock *bl);
gint		camel_block_file_sync_block	(CamelBlockFile *bs,
						 CamelBlock *bl);
gint		camel_block_file_sync		(CamelBlockFile *bs);

/* ********************************************************************** */

typedef struct _CamelKeyFile CamelKeyFile;
typedef struct _CamelKeyFileClass CamelKeyFileClass;
typedef struct _CamelKeyFilePrivate CamelKeyFilePrivate;

struct _CamelKeyFile {
	CamelObject parent;
	CamelKeyFilePrivate *priv;

	FILE *fp;
	gchar *path;
	gint flags;
	goffset last;
};

struct _CamelKeyFileClass {
	CamelObjectClass parent;
};

GType      camel_key_file_get_type(void);

CamelKeyFile * camel_key_file_new(const gchar *path, gint flags, const gchar version[8]);
gint	       camel_key_file_rename(CamelKeyFile *kf, const gchar *path);
gint	       camel_key_file_delete(CamelKeyFile *kf);

gint            camel_key_file_write(CamelKeyFile *kf, camel_block_t *parent, gsize len, camel_key_t *records);
gint            camel_key_file_read(CamelKeyFile *kf, camel_block_t *start, gsize *len, camel_key_t **records);

G_END_DECLS

#endif /* CAMEL_BLOCK_FILE_H */
