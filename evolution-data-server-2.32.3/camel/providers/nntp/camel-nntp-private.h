/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *  camel-nntp-private.h: Private info for nntp.
 *
 * Authors: Michael Zucchi <notzed@ximian.com>
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

#ifndef CAMEL_NNTP_PRIVATE_H
#define CAMEL_NNTP_PRIVATE_H

/* need a way to configure and save this data, if this header is to
   be installed.  For now, dont install it */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

G_BEGIN_DECLS

struct _CamelNNTPStorePrivate {
	gint dummy;
};

#define CAMEL_NNTP_STORE_LOCK(f, l) (e_mutex_lock(((CamelNNTPStore *)f)->priv->l))
#define CAMEL_NNTP_STORE_UNLOCK(f, l) (e_mutex_unlock(((CamelNNTPStore *)f)->priv->l))

struct _CamelNNTPFolderPrivate {
	GMutex *search_lock;	/* for locking the search object */
	GMutex *cache_lock;     /* for locking the cache object */
};

#define CAMEL_NNTP_FOLDER_LOCK(f, l) (g_mutex_lock(((CamelNNTPFolder *)f)->priv->l))
#define CAMEL_NNTP_FOLDER_UNLOCK(f, l) (g_mutex_unlock(((CamelNNTPFolder *)f)->priv->l))
#else
#define CAMEL_NNTP_FOLDER_LOCK(f, l)
#define CAMEL_NNTP_FOLDER_UNLOCK(f, l)

G_END_DECLS

#endif /* CAMEL_NNTP_PRIVATE_H */
