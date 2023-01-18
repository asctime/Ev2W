/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Mike Kestner
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_MEETING_STORE_H
#define E_MEETING_STORE_H

#include <gtk/gtk.h>
#include <libecal/e-cal.h>
#include "e-meeting-attendee.h"

/* Standard GObject macros */
#define E_TYPE_MEETING_STORE \
	(e_meeting_store_get_type ())
#define E_MEETING_STORE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MEETING_STORE, EMeetingStore))
#define E_MEETING_STORE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MEETING_STORE, EMeetingStoreClass))
#define E_IS_MEETING_STORE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MEETING_STORE))
#define E_IS_MEETING_STORE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((obj), E_TYPE_MEETING_STORE))
#define E_MEETING_STORE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MEETING_STORE, EMeetingStoreClass))

G_BEGIN_DECLS

typedef struct _EMeetingStore EMeetingStore;
typedef struct _EMeetingStoreClass EMeetingStoreClass;
typedef struct _EMeetingStorePrivate EMeetingStorePrivate;

typedef enum {
	E_MEETING_STORE_ADDRESS_COL,
	E_MEETING_STORE_MEMBER_COL,
	E_MEETING_STORE_TYPE_COL,
	E_MEETING_STORE_ROLE_COL,
	E_MEETING_STORE_RSVP_COL,
	E_MEETING_STORE_DELTO_COL,
	E_MEETING_STORE_DELFROM_COL,
	E_MEETING_STORE_STATUS_COL,
	E_MEETING_STORE_CN_COL,
	E_MEETING_STORE_LANGUAGE_COL,
	E_MEETING_STORE_ATTENDEE_COL,
	E_MEETING_STORE_ATTENDEE_UNDERLINE_COL,
	E_MEETING_STORE_COLUMN_COUNT
} EMeetingStoreColumns;

struct _EMeetingStore {
	GtkListStore parent;
	EMeetingStorePrivate *priv;
};

struct _EMeetingStoreClass {
	GtkListStoreClass parent_class;
};

typedef gboolean (*EMeetingStoreRefreshCallback) (gpointer data);

GType		e_meeting_store_get_type	(void);
GObject *	e_meeting_store_new		(void);
void		e_meeting_store_set_value	(EMeetingStore *meeting_store,
						 gint row,
						 gint col,
						 const gchar *val);
ECal *		e_meeting_store_get_client	(EMeetingStore *meeting_store);
void		e_meeting_store_set_client	(EMeetingStore *meeting_store,
						 ECal *client);
const gchar *	e_meeting_store_get_free_busy_template
						(EMeetingStore *meeting_store);
void		e_meeting_store_set_free_busy_template
						(EMeetingStore *meeting_store,
						 const gchar *free_busy_template);
icaltimezone *	e_meeting_store_get_timezone	(EMeetingStore *meeting_store);
void		e_meeting_store_set_timezone	(EMeetingStore *meeting_store,
						 icaltimezone *timezone);
void		e_meeting_store_add_attendee	(EMeetingStore *meeting_store,
						 EMeetingAttendee *attendee);
EMeetingAttendee *
		e_meeting_store_add_attendee_with_defaults
						(EMeetingStore *meeting_store);
void		e_meeting_store_remove_attendee	(EMeetingStore *meeting_store,
						 EMeetingAttendee *attendee);
void		e_meeting_store_remove_all_attendees
						(EMeetingStore *meeting_store);
EMeetingAttendee *
		e_meeting_store_find_attendee	(EMeetingStore *meeting_store,
						 const gchar *address,
						 gint *row);
EMeetingAttendee *
		e_meeting_store_find_attendee_at_row
						(EMeetingStore *meeting_store,
						 gint row);
GtkTreePath *	e_meeting_store_find_attendee_path
						(EMeetingStore *meeting_store,
						 EMeetingAttendee *attendee);
gint		e_meeting_store_count_actual_attendees
						(EMeetingStore *meeting_store);
const GPtrArray *
		e_meeting_store_get_attendees	(EMeetingStore *meeting_store);
void		e_meeting_store_refresh_all_busy_periods
						(EMeetingStore *meeting_store,
						 EMeetingTime *start,
						 EMeetingTime *end,
						 EMeetingStoreRefreshCallback call_back,
						 gpointer data);
void		e_meeting_store_refresh_busy_periods
						(EMeetingStore *meeting_store,
						 gint row,
						 EMeetingTime *start,
						 EMeetingTime *end,
						 EMeetingStoreRefreshCallback call_back,
						 gpointer data);

guint		e_meeting_store_get_num_queries	(EMeetingStore *meeting_store);

G_END_DECLS

#endif
