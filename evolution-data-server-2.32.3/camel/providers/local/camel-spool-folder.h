/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Author: Michael Zucchi <notzed@ximian.com>
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

#ifndef CAMEL_SPOOL_FOLDER_H
#define CAMEL_SPOOL_FOLDER_H

#include "camel-mbox-folder.h"
#include "camel-spool-summary.h"

/* Standard GObject macros */
#define CAMEL_TYPE_SPOOL_FOLDER \
	(camel_spool_folder_get_type ())
#define CAMEL_SPOOL_FOLDER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), CAMEL_TYPE_SPOOL_FOLDER, CamelSpoolFolder))
#define CAMEL_SPOOL_FOLDER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), CAMEL_TYPE_SPOOL_FOLDER, CamelSpoolFolderClass))
#define CAMEL_IS_SPOOL_FOLDER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), CAMEL_TYPE_SPOOL_FOLDER))
#define CAMEL_IS_SPOOL_FOLDER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), CAMEL_TYPE_SPOOL_FOLDER))
#define CAMEL_SPOOL_FOLDER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), CAMEL_TYPE_SPOOL_FOLDER, CamelSpoolFolderClass))

G_BEGIN_DECLS

typedef struct _CamelSpoolFolder CamelSpoolFolder;
typedef struct _CamelSpoolFolderClass CamelSpoolFolderClass;
typedef struct _CamelSpoolFolderPrivate CamelSpoolFolderPrivate;

struct _CamelSpoolFolder {
	CamelMboxFolder parent;
	CamelSpoolFolderPrivate *priv;

	gint lockid;		/* lock id for dot locking */
};

struct _CamelSpoolFolderClass {
	CamelMboxFolderClass parent_class;
};

GType camel_spool_folder_get_type(void);

CamelFolder *camel_spool_folder_new(CamelStore *parent_store, const gchar *full_name, guint32 flags, GError **error);

G_END_DECLS

#endif /* CAMEL_SPOOL_FOLDER_H */
