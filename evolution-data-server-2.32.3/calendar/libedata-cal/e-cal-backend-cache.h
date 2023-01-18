/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution calendar - generic backend class
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * Authors: Rodrigo Moya <rodrigo@ximian.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef E_CAL_BACKEND_CACHE_H
#define E_CAL_BACKEND_CACHE_H

#include "libebackend/e-file-cache.h"
#include <libecal/e-cal-component.h>
#include <libecal/e-cal.h>

/* Standard GObject macros */
#define E_TYPE_CAL_BACKEND_CACHE \
	(e_cal_backend_cache_get_type ())
#define E_CAL_BACKEND_CACHE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CAL_BACKEND_CACHE, ECalBackendCache))
#define E_CAL_BACKEND_CACHE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CAL_BACKEND_CACHE, ECalBackendCacheClass))
#define E_IS_CAL_BACKEND_CACHE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CAL_BACKEND_CACHE))
#define E_IS_CAL_BACKEND_CACHE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CAL_BACKEND_CACHE))
#define E_CAL_BACKEND_CACHE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CAL_BACKEND_CACHE, ECalBackendCacheClass))

G_BEGIN_DECLS

typedef struct _ECalBackendCache ECalBackendCache;
typedef struct _ECalBackendCacheClass ECalBackendCacheClass;
typedef struct _ECalBackendCachePrivate ECalBackendCachePrivate;

struct _ECalBackendCache {
	EFileCache parent;
	ECalBackendCachePrivate *priv;
};

struct _ECalBackendCacheClass {
	EFileCacheClass parent_class;
};

GType		e_cal_backend_cache_get_type	(void);
ECalBackendCache *
		e_cal_backend_cache_new		(const gchar *filename);
ECalComponent *	e_cal_backend_cache_get_component
						(ECalBackendCache *cache,
						 const gchar *uid,
						 const gchar *rid);
gboolean	e_cal_backend_cache_put_component
						(ECalBackendCache *cache,
						 ECalComponent *comp);
gboolean	e_cal_backend_cache_remove_component
						(ECalBackendCache *cache,
						 const gchar *uid,
						 const gchar *rid);
GList *		e_cal_backend_cache_get_components
						(ECalBackendCache *cache);
GSList *	e_cal_backend_cache_get_components_by_uid
						(ECalBackendCache *cache,
						 const gchar *uid);
const icaltimezone *
		e_cal_backend_cache_get_timezone(ECalBackendCache *cache,
						 const gchar *tzid);
gboolean	e_cal_backend_cache_put_timezone(ECalBackendCache *cache,
						 const icaltimezone *zone);
gboolean	e_cal_backend_cache_remove_timezone
						(ECalBackendCache *cache,
						 const gchar *tzid);
gboolean	e_cal_backend_cache_put_default_timezone
						(ECalBackendCache *cache,
						 icaltimezone *default_zone);
icaltimezone *	e_cal_backend_cache_get_default_timezone
						(ECalBackendCache *cache);
GSList *	e_cal_backend_cache_get_keys	(ECalBackendCache *cache);
const gchar *	e_cal_backend_cache_get_marker	(ECalBackendCache *cache);
void		e_cal_backend_cache_set_marker	(ECalBackendCache *cache);
gboolean	e_cal_backend_cache_put_server_utc_time
						(ECalBackendCache *cache,
						 const gchar *utc_str);
const gchar *	e_cal_backend_cache_get_server_utc_time
						(ECalBackendCache *cache);
gboolean	e_cal_backend_cache_put_key_value
						(ECalBackendCache *cache,
						 const gchar *key,
						 const gchar *value);
const gchar *	e_cal_backend_cache_get_key_value
						(ECalBackendCache *cache,
						 const gchar *key);
gboolean	e_cal_backend_cache_remove	(const gchar *dirname,
						 const gchar *basename);

G_END_DECLS

#endif /* E_CAL_BACKEND_CACHE_H */
