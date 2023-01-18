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
 *		Damon Chaplin <damon@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/*
 * ECellDateEdit - a subclass of ECellPopup used to show a date with a popup
 * window to edit it.
 */

#ifndef _E_CELL_DATE_EDIT_H_
#define _E_CELL_DATE_EDIT_H_

#include <time.h>
#include <table/e-cell-popup.h>

#define E_CELL_DATE_EDIT_TYPE        (e_cell_date_edit_get_type ())
#define E_CELL_DATE_EDIT(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_CELL_DATE_EDIT_TYPE, ECellDateEdit))
#define E_CELL_DATE_EDIT_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), E_CELL_DATE_EDIT_TYPE, ECellDateEditClass))
#define E_IS_CELL_DATE_EDIT(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_CELL_DATE_EDIT_TYPE))
#define E_IS_CELL_DATE_EDIT_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_CELL_DATE_EDIT_TYPE))

typedef struct _ECellDateEdit ECellDateEdit;
typedef struct _ECellDateEditClass ECellDateEditClass;

/* The type of the callback function optionally used to get the current time.
 */
typedef struct tm (*ECellDateEditGetTimeCallback) (ECellDateEdit *ecde,
						   gpointer	  data);

struct _ECellDateEdit {
	ECellPopup parent;

	GtkWidget *popup_window;
	GtkWidget *calendar;
	GtkWidget *time_entry;
	GtkWidget *time_tree_view;

	GtkWidget *now_button;
	GtkWidget *today_button;
	GtkWidget *none_button;

	/* This is the range of hours we show in the time list. */
	gint lower_hour;
	gint upper_hour;

	/* TRUE if we use 24-hour format for the time list and entry. */
	gboolean use_24_hour_format;

	/* This is TRUE if we need to rebuild the list of times. */
	gboolean need_time_list_rebuild;

	/* The freeze count for rebuilding the time list. We only rebuild when
	   this is 0. */
	gint freeze_count;

	ECellDateEditGetTimeCallback time_callback;
	gpointer time_callback_data;
	GDestroyNotify time_callback_destroy;
};

struct _ECellDateEditClass {
	ECellPopupClass parent_class;
};

GType		e_cell_date_edit_get_type	(void);
ECell *		e_cell_date_edit_new		(void);

/* These freeze and thaw the rebuilding of the time list. They are useful when
   setting several properties which result in rebuilds of the list, e.g. the
   lower_hour, upper_hour and use_24_hour_format properties. */
void		e_cell_date_edit_freeze		(ECellDateEdit *ecde);
void		e_cell_date_edit_thaw		(ECellDateEdit *ecde);

/* Sets a callback to use to get the current time. This is useful if the
   application needs to use its own timezone data rather than rely on the
   Unix timezone. */
void		e_cell_date_edit_set_get_time_callback
						(ECellDateEdit *ecde,
						 ECellDateEditGetTimeCallback cb,
						 gpointer data,
						 GDestroyNotify destroy);

#endif /* _E_CELL_DATE_EDIT_H_ */
