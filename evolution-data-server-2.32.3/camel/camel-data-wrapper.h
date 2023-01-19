/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Bertrand Guiheneuf <bertrand@helixcode.com>
 *           Michael Zucchi <notzed@ximian.com>
 *           Jeffrey Stedfast <fejj@ximian.com>
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

#if !defined (__CAMEL_H_INSIDE__) && !defined (CAMEL_COMPILATION)
#error "Only <camel/camel.h> can be included directly."
#endif

#ifndef CAMEL_DATA_WRAPPER_H
#define CAMEL_DATA_WRAPPER_H

#include <sys/types.h>

#include <camel/camel-mime-utils.h>
#include <camel/camel-stream.h>

/* Standard GObject macros */
#define CAMEL_TYPE_DATA_WRAPPER \
	(camel_data_wrapper_get_type ())
#define CAMEL_DATA_WRAPPER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), CAMEL_TYPE_DATA_WRAPPER, CamelDataWrapper))
#define CAMEL_DATA_WRAPPER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), CAMEL_TYPE_DATA_WRAPPER, CamelDataWrapperClass))
#define CAMEL_IS_DATA_WRAPPER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), CAMEL_TYPE_DATA_WRAPPER))
#define CAMEL_IS_DATA_WRAPPER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), CAMEL_TYPE_DATA_WRAPPER))
#define CAMEL_DATA_WRAPPER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), CAMEL_TYPE_DATA_WRAPPER, CamelDataWrapperClass))

G_BEGIN_DECLS

typedef struct _CamelDataWrapper CamelDataWrapper;
typedef struct _CamelDataWrapperClass CamelDataWrapperClass;
typedef struct _CamelDataWrapperPrivate CamelDataWrapperPrivate;

/**
 * CamelDataWrapperLock:
 *
 * Since: 2.32
 **/
typedef enum {
	CAMEL_DATA_WRAPPER_STREAM_LOCK
} CamelDataWrapperLock;

struct _CamelDataWrapper {
	CamelObject parent;
	CamelDataWrapperPrivate *priv;

	CamelTransferEncoding encoding;

	CamelContentType *mime_type;
	CamelStream *stream;

	guint offline:1;
};

struct _CamelDataWrapperClass {
	CamelObjectClass parent_class;

	void		(*set_mime_type)	(CamelDataWrapper *data_wrapper,
						 const gchar *mime_type);
	gchar *		(*get_mime_type)	(CamelDataWrapper *data_wrapper);
	CamelContentType *
			(*get_mime_type_field)	(CamelDataWrapper *data_wrapper);
	void		(*set_mime_type_field)	(CamelDataWrapper *data_wrapper,
						 CamelContentType *mime_type_field);
	gssize		(*write_to_stream)	(CamelDataWrapper *data_wrapper,
						 CamelStream *stream,
						 GError **error);
	gssize		(*decode_to_stream)	(CamelDataWrapper *data_wrapper,
						 CamelStream *stream,
						 GError **error);
	gint		(*construct_from_stream)(CamelDataWrapper *data_wrapper,
						 CamelStream *stream,
						 GError **error);
	gboolean	(*is_offline)		(CamelDataWrapper *data_wrapper);
};

GType		camel_data_wrapper_get_type	(void);
CamelDataWrapper *
		camel_data_wrapper_new		(void);
gssize		camel_data_wrapper_write_to_stream
						(CamelDataWrapper *data_wrapper,
						 CamelStream *stream,
						 GError **error);
gssize		camel_data_wrapper_decode_to_stream
						(CamelDataWrapper *data_wrapper,
						 CamelStream *stream,
						 GError **error);
void		camel_data_wrapper_set_mime_type(CamelDataWrapper *data_wrapper,
						 const gchar *mime_type);
gchar *		camel_data_wrapper_get_mime_type(CamelDataWrapper *data_wrapper);
CamelContentType *
		camel_data_wrapper_get_mime_type_field
						(CamelDataWrapper *data_wrapper);
void		camel_data_wrapper_set_mime_type_field
						(CamelDataWrapper *data_wrapper,
						 CamelContentType *mime_type);
gint		camel_data_wrapper_construct_from_stream
						(CamelDataWrapper *data_wrapper,
						 CamelStream *stream,
						 GError **error);
gboolean	camel_data_wrapper_is_offline	(CamelDataWrapper *data_wrapper);
void		camel_data_wrapper_lock		(CamelDataWrapper *data_wrapper,
						 CamelDataWrapperLock lock);
void		camel_data_wrapper_unlock	(CamelDataWrapper *data_wrapper,
						 CamelDataWrapperLock lock);

G_END_DECLS

#endif /* CAMEL_DATA_WRAPPER_H */
