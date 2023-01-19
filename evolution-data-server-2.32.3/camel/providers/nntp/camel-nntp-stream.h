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

#ifndef CAMEL_NNTP_STREAM_H
#define CAMEL_NNTP_STREAM_H

#include <camel/camel.h>

/* Standard GObject macros */
#define CAMEL_TYPE_NNTP_STREAM \
	(camel_nntp_stream_get_type ())
#define CAMEL_NNTP_STREAM(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), CAMEL_TYPE_NNTP_STREAM, CamelNNTPStream))
#define CAMEL_NNTP_STREAM_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), CAMEL_TYPE_NNTP_STREAM, CamelNNTPStreamClass))
#define CAMEL_IS_NNTP_STREAM(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), CAMEL_TYPE_NNTP_STREAM))
#define CAMEL_IS_NNTP_STREAM_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), CAMEL_TYPE_NNTP_STREAM))
#define CAMEL_NNTP_STREAM_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), CAMEL_TYPE_NNTP_STREAM, CamelNNTPStreamClass))

G_BEGIN_DECLS

typedef struct _CamelNNTPStream CamelNNTPStream;
typedef struct _CamelNNTPStreamClass CamelNNTPStreamClass;

typedef enum {
	CAMEL_NNTP_STREAM_LINE,
	CAMEL_NNTP_STREAM_DATA,
	CAMEL_NNTP_STREAM_EOD	/* end of data, acts as if end of stream */
} camel_nntp_stream_mode_t;

struct _CamelNNTPStream {
	CamelStream parent;

	CamelStream *source;

	camel_nntp_stream_mode_t mode;
	gint state;

	guchar *buf, *ptr, *end;
	guchar *linebuf, *lineptr, *lineend;
};

struct _CamelNNTPStreamClass {
	CamelStreamClass parent_class;
};

GType		camel_nntp_stream_get_type	(void);

CamelStream *	camel_nntp_stream_new		(CamelStream *source);

void		camel_nntp_stream_set_mode	(CamelNNTPStream *is,
						 camel_nntp_stream_mode_t mode);
gint		camel_nntp_stream_line		(CamelNNTPStream *is,
						 guchar **data,
						 guint *len,
						 GError **error);
gint		camel_nntp_stream_gets		(CamelNNTPStream *is,
						 guchar **start,
						 guint *len);
gint		camel_nntp_stream_getd		(CamelNNTPStream *is,
						 guchar **start,
						 guint *len);

G_END_DECLS

#endif /* CAMEL_NNTP_STREAM_H */
