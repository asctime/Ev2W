/*
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

#if !defined (__CAMEL_H_INSIDE__) && !defined (CAMEL_COMPILATION)
#error "Only <camel/camel.h> can be included directly."
#endif

#ifndef CAMEL_INDEX_H
#define CAMEL_INDEX_H

#include <camel/camel-object.h>

/* Standard GObject macros */
#define CAMEL_TYPE_INDEX \
	(camel_index_get_type ())
#define CAMEL_INDEX(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), CAMEL_TYPE_INDEX, CamelIndex))
#define CAMEL_INDEX_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), CAMEL_TYPE_INDEX, CamelIndexClass))
#define CAMEL_IS_INDEX(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), CAMEL_TYPE_INDEX))
#define CAMEL_IS_INDEX_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), CAMEL_TYPE_INDEX))
#define CAMEL_INDEX_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), CAMEL_TYPE_INDEX, CamelIndexClass))

#define CAMEL_TYPE_INDEX_NAME \
	(camel_index_name_get_type ())
#define CAMEL_INDEX_NAME(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), CAMEL_TYPE_INDEX_NAME, CamelIndexName))
#define CAMEL_INDEX_NAME_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), CAMEL_TYPE_INDEX_NAME, CamelIndexNameClass))
#define CAMEL_IS_INDEX_NAME(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), CAMEL_TYPE_INDEX_NAME))
#define CAMEL_IS_INDEX_NAME_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), CAMEL_TYPE_INDEX_NAME))
#define CAMEL_INDEX_NAME_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), CAMEL_TYPE_INDEX_NAME, CamelIndexNameClass))

#define CAMEL_TYPE_INDEX_CURSOR \
	(camel_index_cursor_get_type ())
#define CAMEL_INDEX_CURSOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), CAMEL_TYPE_INDEX_CURSOR, CamelIndexCursor))
#define CAMEL_INDEX_CURSOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), CAMEL_TYPE_INDEX_CURSOR, CamelIndexCursorClass))
#define CAMEL_IS_INDEX_CURSOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), CAMEL_TYPE_INDEX_CURSOR))
#define CAMEL_IS_INDEX_CURSOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), CAMEL_TYPE_INDEX_CURSOR))
#define CAMEL_INDEX_CURSOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), CAMEL_TYPE_INDEX_CURSOR, CamelIndexCursorClass))

G_BEGIN_DECLS

typedef struct _CamelIndex CamelIndex;
typedef struct _CamelIndexClass CamelIndexClass;
typedef struct _CamelIndexPrivate CamelIndexPrivate;

typedef struct _CamelIndexName CamelIndexName;
typedef struct _CamelIndexNameClass CamelIndexNameClass;
typedef struct _CamelIndexNamePrivate CamelIndexNamePrivate;

typedef struct _CamelIndexCursor CamelIndexCursor;
typedef struct _CamelIndexCursorClass CamelIndexCursorClass;
typedef struct _CamelIndexCursorPrivate CamelIndexCursorPrivate;

typedef gchar * (*CamelIndexNorm)(CamelIndex *index, const gchar *word, gpointer data);

/* ********************************************************************** */

struct _CamelIndexCursor {
	CamelObject parent;
	CamelIndexCursorPrivate *priv;

	CamelIndex *index;
};

struct _CamelIndexCursorClass {
	CamelObjectClass parent;

	const gchar * (*next) (CamelIndexCursor *idc);
	void         (*reset) (CamelIndexCursor *idc);
};

GType           camel_index_cursor_get_type(void);

CamelIndexCursor  *camel_index_cursor_new(CamelIndex *index, const gchar *name);

const gchar        *camel_index_cursor_next(CamelIndexCursor *idc);
void               camel_index_cursor_reset(CamelIndexCursor *idc);

/* ********************************************************************** */

struct _CamelIndexName {
	CamelObject parent;
	CamelIndexNamePrivate *priv;

	CamelIndex *index;

	gchar *name;		/* name being indexed */

	GByteArray *buffer;	/* used for normalisation */
	GHashTable *words;	/* unique list of words */
};

struct _CamelIndexNameClass {
	CamelObjectClass parent;

	gint (*sync)(CamelIndexName *name);
	void (*add_word)(CamelIndexName *name, const gchar *word);
	gsize (*add_buffer)(CamelIndexName *name, const gchar *buffer, gsize len);
};

GType           camel_index_name_get_type	(void);

CamelIndexName    *camel_index_name_new(CamelIndex *index, const gchar *name);

void               camel_index_name_add_word(CamelIndexName *name, const gchar *word);
gsize             camel_index_name_add_buffer(CamelIndexName *name, const gchar *buffer, gsize len);

/* ********************************************************************** */

struct _CamelIndex {
	CamelObject parent;
	CamelIndexPrivate *priv;

	gchar *path;
	guint32 version;
	guint32 flags;		/* open flags */
	guint32 state;

	CamelIndexNorm normalize;
	gpointer normalize_data;
};

struct _CamelIndexClass {
	CamelObjectClass parent_class;

	gint		(*sync)			(CamelIndex *index);
	gint		(*compress)		(CamelIndex *index);
	gint		(*delete)		(CamelIndex *index);
	gint		(*rename)		(CamelIndex *index,
						 const gchar *path);
	gint		(*has_name)		(CamelIndex *index,
						 const gchar *name);
	CamelIndexName *(*add_name)		(CamelIndex *index,
						 const gchar *name);
	gint		(*write_name)		(CamelIndex *index,
						 CamelIndexName *idn);
	CamelIndexCursor *
			(*find_name)		(CamelIndex *index,
						 const gchar *name);
	void		(*delete_name)		(CamelIndex *index,
						 const gchar *name);
	CamelIndexCursor *
			(*find)			(CamelIndex *index,
						 const gchar *word);
	CamelIndexCursor *
			(*words)		(CamelIndex *index);
	CamelIndexCursor *
			(*names)		(CamelIndex *index);
};

/* flags, stored in 'state', set with set_state */
#define CAMEL_INDEX_DELETED (1<<0)

GType		camel_index_get_type		(void);
CamelIndex *	camel_index_new			(const gchar *path,
						 gint flags);
void		camel_index_construct		(CamelIndex *index,
						 const gchar *path,
						 gint flags);
gint		camel_index_rename		(CamelIndex *index,
						 const gchar *path);
void		camel_index_set_normalize	(CamelIndex *index,
						 CamelIndexNorm func,
						 gpointer data);
gint		camel_index_sync		(CamelIndex *index);
gint		camel_index_compress		(CamelIndex *index);
gint		camel_index_delete		(CamelIndex *index);
gint		camel_index_has_name		(CamelIndex *index,
						 const gchar *name);
CamelIndexName *camel_index_add_name		(CamelIndex *index,
						 const gchar *name);
gint		camel_index_write_name		(CamelIndex *index,
						 CamelIndexName *idn);
CamelIndexCursor *
		camel_index_find_name		(CamelIndex *index,
						 const gchar *name);
void		camel_index_delete_name		(CamelIndex *index,
						 const gchar *name);
CamelIndexCursor *
		camel_index_find		(CamelIndex *index,
						 const gchar *word);
CamelIndexCursor *
		camel_index_words		(CamelIndex *index);
CamelIndexCursor *
		camel_index_names		(CamelIndex *index);

G_END_DECLS

#endif /* CAMEL_INDEX_H */
