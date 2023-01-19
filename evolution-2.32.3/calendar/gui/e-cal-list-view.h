/*
 *
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
 *		Hans Petter Jansson  <hpj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_CAL_LIST_VIEW_H_
#define _E_CAL_LIST_VIEW_H_

#include <time.h>
#include <gtk/gtk.h>

#include <table/e-table.h>
#include <table/e-cell-date-edit.h>

#include "e-calendar-view.h"
#include "gnome-cal.h"

/*
 * ECalListView - displays calendar events in an ETable.
 */

/* Standard GObject macros */
#define E_TYPE_CAL_LIST_VIEW \
	(e_cal_list_view_get_type ())
#define E_CAL_LIST_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CAL_LIST_VIEW, ECalListView))
#define E_CAL_LIST_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CAL_LIST_VIEW, ECalListViewClass))
#define E_IS_CAL_LIST_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CAL_LIST_VIEW))
#define E_IS_CAL_LIST_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CAL_LIST_VIEW))
#define E_CAL_LIST_VIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CAL_LIST_VIEW, ECalListViewClass))

G_BEGIN_DECLS

typedef struct _ECalListView       ECalListView;
typedef struct _ECalListViewClass  ECalListViewClass;

struct _ECalListView {
	ECalendarView parent;

	/* The main display table */
	ETable *table;

	/* S-expression for query and the query object */
	ECalView *query;

	/* The default category for new events */
	gchar *default_category;

	/* Date editing cell */
	ECellDateEdit *dates_cell;

	/* The last ECalendarViewEvent we returned from e_cal_list_view_get_selected_events(), to be freed */
	ECalendarViewEvent *cursor_event;

	/* Idle handler ID for setting a new ETableModel */
	gint set_table_id;
};

struct _ECalListViewClass {
	ECalendarViewClass parent_class;
};

GType		   e_cal_list_view_get_type		(void);
ECalendarView *e_cal_list_view_new			(ECalModel *cal_model);
void e_cal_list_view_load_state (ECalListView *cal_list_view, gchar *filename);
void e_cal_list_view_save_state (ECalListView *cal_list_view, gchar *filename);

gboolean   e_cal_list_view_get_range_shown      (ECalListView *cal_list_view, GDate *start_date,
						 gint *days_shown);

G_END_DECLS

#endif /* _E_CAL_LIST_VIEW_H_ */
