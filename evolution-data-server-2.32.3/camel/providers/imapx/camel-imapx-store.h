/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-imap-store.h : class for an imap store */

/*
 * Authors: Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef CAMEL_IMAPX_STORE_H
#define CAMEL_IMAPX_STORE_H

#include <camel/camel.h>

#include "camel-imapx-server.h"
#include "camel-imapx-store-summary.h"
#include "camel-imapx-conn-manager.h"

/* Standard GObject macros */
#define CAMEL_TYPE_IMAPX_STORE \
	(camel_imapx_store_get_type ())
#define CAMEL_IMAPX_STORE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), CAMEL_TYPE_IMAPX_STORE, CamelIMAPXStore))
#define CAMEL_IMAPX_STORE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), CAMEL_TYPE_IMAPX_STORE, CamelIMAPXStoreClass))
#define CAMEL_IS_IMAPX_STORE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), CAMEL_TYPE_IMAPX_STORE))
#define CAMEL_IS_IMAPX_STORE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), CAMEL_TYPE_IMAPX_STORE))
#define CAMEL_IMAPX_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), CAMEL_TYPE_IMAPX_STORE, CamelIMAPXStoreClass))

#define IMAPX_OVERRIDE_NAMESPACE	(1 << 0)
#define IMAPX_CHECK_ALL			(1 << 1)
#define IMAPX_FILTER_INBOX		(1 << 2)
#define IMAPX_FILTER_JUNK		(1 << 3)
#define IMAPX_FILTER_JUNK_INBOX		(1 << 4)
#define IMAPX_SUBSCRIPTIONS		(1 << 5)
#define IMAPX_CHECK_LSUB		(1 << 6)
#define IMAPX_USE_IDLE			(1 << 7)
#define IMAPX_USE_QRESYNC		(1 << 8)

G_BEGIN_DECLS

typedef struct _CamelIMAPXStore CamelIMAPXStore;
typedef struct _CamelIMAPXStoreClass CamelIMAPXStoreClass;

struct _CamelIMAPXStore {
	CamelOfflineStore parent;

	CamelIMAPXConnManager *con_man;

	CamelIMAPXStoreSummary *summary; /* in-memory list of folders */
	gchar *namespace, dir_sep, *base_url, *storage_path;

	guint32 rec_options;

	/* Used for syncronizing get_folder_info. Check for re-use of any other lock. At the
	   moment, could not find anything suitable for this */
	GMutex *get_finfo_lock;
	time_t last_refresh_time;

	/* hash table of UIDs to ignore as recent when updating folder */
	GHashTable *ignore_recent;

	/* if we had a login error, what to show to user */
	gchar *login_error;

	GPtrArray *pending_list;
};

struct _CamelIMAPXStoreClass {
	CamelOfflineStoreClass parent_class;
};

GType			camel_imapx_store_get_type	(void);
CamelIMAPXServer *	camel_imapx_store_get_server	(CamelIMAPXStore *store,
							const gchar *folder_name,
							GError **error);
void			camel_imapx_store_op_done	(CamelIMAPXStore *istore,
							CamelIMAPXServer *server,
							const gchar *folder_name);

G_END_DECLS

#endif /* CAMEL_IMAPX_STORE_H */

