/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; fill-column: 160 -*- */
/* camel-stream.h : class for an abstract stream */

/*
 * Author:
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
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

#if !defined (__CAMEL_H_INSIDE__) && !defined (CAMEL_COMPILATION)
#error "Only <camel/camel.h> can be included directly."
#endif

#ifndef CAMEL_STREAM_H
#define CAMEL_STREAM_H

#include <stdarg.h>
#include <unistd.h>
#include <camel/camel-object.h>

/* Standard GObject macros */
#define CAMEL_TYPE_STREAM \
	(camel_stream_get_type ())
#define CAMEL_STREAM(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), CAMEL_TYPE_STREAM, CamelStream))
#define CAMEL_STREAM_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), CAMEL_TYPE_STREAM, CamelStreamClass))
#define CAMEL_IS_STREAM(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), CAMEL_TYPE_STREAM))
#define CAMEL_IS_STREAM_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), CAMEL_TYPE_STREAM))
#define CAMEL_STREAM_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), CAMEL_TYPE_STREAM, CamelStreamClass))

G_BEGIN_DECLS

typedef struct _CamelStream CamelStream;
typedef struct _CamelStreamClass CamelStreamClass;

struct _CamelStream {
	CamelObject parent;

	gboolean eos;
};

struct _CamelStreamClass {
	CamelObjectClass parent_class;

	gssize		(*read)			(CamelStream *stream,
						 gchar *buffer,
						 gsize n,
						 GError **error);
	gssize		(*write)		(CamelStream *stream,
						 const gchar *buffer,
						 gsize n,
						 GError **error);
	gint		(*close)		(CamelStream *stream,
						 GError **error);
	gint		(*flush)		(CamelStream *stream,
						 GError **error);
	gboolean	(*eos)			(CamelStream *stream);
	gint		(*reset)		(CamelStream *stream,
						 GError **error);
};

GType		camel_stream_get_type		(void);
gssize		camel_stream_read		(CamelStream *stream,
						 gchar *buffer,
						 gsize n,
						 GError **error);
gssize		camel_stream_write		(CamelStream *stream,
						 const gchar *buffer,
						 gsize n,
						 GError **error);
gint		camel_stream_flush		(CamelStream *stream,
						 GError **error);
gint		camel_stream_close		(CamelStream *stream,
						 GError **error);
gboolean	camel_stream_eos		(CamelStream *stream);
gint		camel_stream_reset		(CamelStream *stream,
						 GError **error);

/* utility macros and funcs */
gssize		camel_stream_write_string	(CamelStream *stream,
						 const gchar *string,
						 GError **error);
gssize		camel_stream_printf		(CamelStream *stream,
						 const gchar *fmt,
						 ...) G_GNUC_PRINTF (2, 3);
gssize		camel_stream_vprintf		(CamelStream *stream,
						 const gchar *fmt,
						 va_list ap);

/* Write a whole stream to another stream, until eof or error on
 * either stream.  */
gssize		camel_stream_write_to_stream	(CamelStream *stream,
						 CamelStream *output_stream,
						 GError **error);

G_END_DECLS

#endif /* CAMEL_STREAM_H */
