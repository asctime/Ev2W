/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Authors:
 *	Michael Zucchi <notzed@ximian.com>
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

#ifndef CAMEL_MH_FOLDER_H
#define CAMEL_MH_FOLDER_H

#include "camel-local-folder.h"

#define CAMEL_TYPE_MH_FOLDER \
	(camel_mh_folder_get_type ())
#define CAMEL_MH_FOLDER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), CAMEL_TYPE_MH_FOLDER, CamelMhFolder))
#define CAMEL_MH_FOLDER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), CAMEL_TYPE_MH_FOLDER, CamelMhFolderClass))
#define CAMEL_IS_MH_FOLDER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), CAMEL_TYPE_MH_FOLDER))
#define CAMEL_IS_MH_FOLDER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), CAMEL_TYPE_MH_FOLDER))
#define CAMEL_MH_FOLDER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), CAMEL_TYPE_MH_FOLDER, CamelMhFolderClass))

G_BEGIN_DECLS

typedef struct _CamelMhFolder CamelMhFolder;
typedef struct _CamelMhFolderClass CamelMhFolderClass;

struct _CamelMhFolder {
	CamelLocalFolder parent;
};

struct _CamelMhFolderClass {
	CamelLocalFolderClass parent_class;
};

/* public methods */
CamelFolder *camel_mh_folder_new(CamelStore *parent_store, const gchar *full_name, guint32 flags, GError **error);

GType camel_mh_folder_get_type(void);

G_END_DECLS

#endif /* CAMEL_MH_FOLDER_H */
