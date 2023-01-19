/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-stream-fs.h :stream based on unix filesystem */

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

#ifndef CAMEL_STREAM_FS_H
#define CAMEL_STREAM_FS_H

/* for open flags */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <camel/camel-seekable-stream.h>

/* Standard GObject macros */
#define CAMEL_TYPE_STREAM_FS \
	(camel_stream_fs_get_type ())
#define CAMEL_STREAM_FS(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), CAMEL_TYPE_STREAM_FS, CamelStreamFs))
#define CAMEL_STREAM_FS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), CAMEL_TYPE_STREAM_FS, CamelStreamFsClass))
#define CAMEL_IS_STREAM_FS(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), CAMEL_TYPE_STREAM_FS))
#define CAMEL_IS_STREAM_FS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), CAMEL_TYPE_STREAM_FS))
#define CAMEL_STREAM_FS_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), CAMEL_TYPE_STREAM_FS, CamelStreamFsClass))

G_BEGIN_DECLS

typedef struct _CamelStreamFs CamelStreamFs;
typedef struct _CamelStreamFsClass CamelStreamFsClass;
typedef struct _CamelStreamFsPrivate CamelStreamFsPrivate;

struct _CamelStreamFs {
	CamelSeekableStream parent;
	CamelStreamFsPrivate *priv;
};

struct _CamelStreamFsClass {
	CamelSeekableStreamClass parent_class;
};

GType		camel_stream_fs_get_type	(void);
CamelStream *	camel_stream_fs_new_with_name	(const gchar *name,
						 gint flags,
						 mode_t mode,
						 GError **error);
CamelStream *	camel_stream_fs_new_with_name_and_bounds
						(const gchar *name,
						 gint flags,
						 mode_t mode,
						 goffset start,
						 goffset end,
						 GError **error);
CamelStream *	camel_stream_fs_new_with_fd	(gint fd);
CamelStream *	camel_stream_fs_new_with_fd_and_bounds
						(gint fd,
						 goffset start,
						 goffset end,
						 GError **error);
gint		camel_stream_fs_get_fd		(CamelStreamFs *stream);

G_END_DECLS

#endif /* CAMEL_STREAM_FS_H */
