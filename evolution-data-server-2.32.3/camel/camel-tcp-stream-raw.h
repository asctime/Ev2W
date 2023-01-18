/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 *
 */

#if !defined (__CAMEL_H_INSIDE__) && !defined (CAMEL_COMPILATION)
#error "Only <camel/camel.h> can be included directly."
#endif

#ifndef CAMEL_TCP_STREAM_RAW_H
#define CAMEL_TCP_STREAM_RAW_H

#include <camel/camel-tcp-stream.h>

/* Standard GObject macros */
#define CAMEL_TYPE_TCP_STREAM_RAW \
	(camel_tcp_stream_raw_get_type ())
#define CAMEL_TCP_STREAM_RAW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), CAMEL_TYPE_TCP_STREAM_RAW, CamelTcpStreamRaw))
#define CAMEL_TCP_STREAM_RAW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), CAMEL_TYPE_TCP_STREAM_RAW, CamelTcpStreamRawClass))
#define CAMEL_IS_TCP_STREAM_RAW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), CAMEL_TYPE_TCP_STREAM_RAW))
#define CAMEL_IS_TCP_STREAM_RAW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), CAMEL_TYPE_TCP_STREAM_RAW))
#define CAMEL_TCP_STREAM_RAW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), CAMEL_TYPE_TCP_STREAM_RAW, CAmelTcpStreamRawClass))

G_BEGIN_DECLS

/**
 * CAMEL_PROXY_ERROR:
 *
 * Since: 2.32
 **/
#define CAMEL_PROXY_ERROR (camel_proxy_error_quark ())

typedef struct _CamelTcpStreamRaw CamelTcpStreamRaw;
typedef struct _CamelTcpStreamRawClass CamelTcpStreamRawClass;

struct _CamelTcpStreamRaw {
	CamelTcpStream parent;

	struct _CamelTcpStreamRawPrivate *priv;
};

struct _CamelTcpStreamRawClass {
	CamelTcpStreamClass parent_class;
};

/**
 * CamelProxyError:
 *
 * Since: 2.32
 */
typedef enum {
	CAMEL_PROXY_ERROR_PROXY_NOT_SUPPORTED,
	CAMEL_PROXY_ERROR_CANT_AUTHENTICATE
} CamelProxyError;

GQuark camel_proxy_error_quark (void) G_GNUC_CONST;

GType camel_tcp_stream_raw_get_type (void);

/* public methods */
CamelStream *camel_tcp_stream_raw_new (void);

void _camel_tcp_stream_raw_replace_file_desc (CamelTcpStreamRaw *raw, PRFileDesc *new_file_desc);

void _set_errno_from_pr_error (gint pr_code);
void _set_g_error_from_errno (GError **error, gboolean eintr_means_cancelled);

G_END_DECLS

#endif /* CAMEL_TCP_STREAM_RAW_H */
