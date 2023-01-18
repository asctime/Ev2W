/* Evolution calendar - Live search query implementation
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
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

#ifndef E_DATA_CAL_VIEW_H
#define E_DATA_CAL_VIEW_H

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <libedata-cal/e-data-cal-common.h>
#include <libedata-cal/e-cal-backend-sexp.h>
#include <libedata-cal/e-data-cal-types.h>

G_BEGIN_DECLS



#define E_DATA_CAL_VIEW_TYPE            (e_data_cal_view_get_type ())
#define E_DATA_CAL_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_DATA_CAL_VIEW_TYPE, EDataCalView))
#define E_DATA_CAL_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), E_DATA_CAL_VIEW_TYPE, EDataCalViewClass))
#define E_IS_DATA_CAL_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_DATA_CAL_VIEW_TYPE))
/* Deprecated macros */
#define QUERY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_DATA_CAL_VIEW_TYPE, EDataCalView))
#define IS_QUERY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_DATA_CAL_VIEW_TYPE))

typedef struct _EDataCalViewPrivate EDataCalViewPrivate;

struct _EDataCalView {
	GObject parent;
	EDataCalViewPrivate *priv;
};

struct _EDataCalViewClass {
	GObjectClass parent_class;
};

GType                 e_data_cal_view_get_type (void);
EDataCalView         *e_data_cal_view_new (ECalBackend *backend, ECalBackendSExp *sexp);

guint e_data_cal_view_register_gdbus_object (EDataCalView *query, GDBusConnection *connection, const gchar *object_path, GError **error);

const gchar           *e_data_cal_view_get_text (EDataCalView *query);
ECalBackendSExp      *e_data_cal_view_get_object_sexp (EDataCalView *query);
gboolean              e_data_cal_view_object_matches (EDataCalView *query, const gchar *object);

GList                *e_data_cal_view_get_matched_objects (EDataCalView *query);
gboolean              e_data_cal_view_is_started (EDataCalView *query);
gboolean              e_data_cal_view_is_done (EDataCalView *query);
gboolean              e_data_cal_view_is_stopped (EDataCalView *query);

void                  e_data_cal_view_notify_objects_added (EDataCalView       *query,
							    const GList *objects);
void                  e_data_cal_view_notify_objects_added_1 (EDataCalView       *query,
							      const gchar *object);
void                  e_data_cal_view_notify_objects_modified (EDataCalView       *query,
							       const GList *objects);
void                  e_data_cal_view_notify_objects_modified_1 (EDataCalView       *query,
								 const gchar *object);
void                  e_data_cal_view_notify_objects_removed (EDataCalView       *query,
							      const GList *ids);
void                  e_data_cal_view_notify_objects_removed_1 (EDataCalView       *query,
								const ECalComponentId *id);
void                  e_data_cal_view_notify_progress (EDataCalView      *query,
						       const gchar *message,
						       gint         percent);
void                  e_data_cal_view_notify_done (EDataCalView                               *query,
						   const GError *error);

G_END_DECLS

#endif
