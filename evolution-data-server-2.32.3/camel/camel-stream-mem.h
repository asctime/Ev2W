/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-stream-mem.h: stream based on memory buffer */

/*
 * Authors: Bertrand Guiheneuf <bertrand@helixcode.com>
 *	    Michael Zucchi <notzed@ximian.com>
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

#ifndef CAMEL_STREAM_MEM_H
#define CAMEL_STREAM_MEM_H

#include <sys/types.h>
#include <camel/camel-seekable-stream.h>

/* Standard GObject macros */
#define CAMEL_TYPE_STREAM_MEM \
	(camel_stream_mem_get_type ())
#define CAMEL_STREAM_MEM(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), CAMEL_TYPE_STREAM_MEM, CamelStreamMem))
#define CAMEL_STREAM_MEM_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), CAMEL_TYPE_STREAM_MEM, CamelStreamMemClass))
#define CAMEL_IS_STREAM_MEM(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), CAMEL_TYPE_STREAM_MEM))
#define CAMEL_IS_STREAM_MEM_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), CAMEL_TYPE_STREAM_MEM))
#define CAMEL_STREAM_MEM_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), CAMEL_TYPE_STREAM_MEM, CamelStreamMemClass))

G_BEGIN_DECLS

typedef struct _CamelStreamMem CamelStreamMem;
typedef struct _CamelStreamMemClass CamelStreamMemClass;
typedef struct _CamelStreamMemPrivate CamelStreamMemPrivate;

struct _CamelStreamMem {
	CamelSeekableStream parent;
	CamelStreamMemPrivate *priv;
};

struct _CamelStreamMemClass {
	CamelSeekableStreamClass parent_class;
};

GType		camel_stream_mem_get_type	(void);
CamelStream *	camel_stream_mem_new		(void);
CamelStream *	camel_stream_mem_new_with_byte_array
						(GByteArray *buffer);
CamelStream *	camel_stream_mem_new_with_buffer(const gchar *buffer,
						 gsize len);
void		camel_stream_mem_set_secure	(CamelStreamMem *mem);
GByteArray *	camel_stream_mem_get_byte_array	(CamelStreamMem *mem);
void		camel_stream_mem_set_byte_array	(CamelStreamMem *mem,
						 GByteArray *buffer);
void		camel_stream_mem_set_buffer	(CamelStreamMem *mem,
						 const gchar *buffer,
						 gsize len);

G_END_DECLS

#endif /* CAMEL_STREAM_MEM_H */
