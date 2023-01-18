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
 *		Miguel de Icaza <miguel@ximian.com>
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_CELL_H_
#define _E_CELL_H_

#include <gtk/gtk.h>
#include <table/e-table-model.h>

G_BEGIN_DECLS

#define E_CELL_TYPE         (e_cell_get_type ())
#define E_CELL(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), E_CELL_TYPE, ECell))
#define E_CELL_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), E_CELL_TYPE, ECellClass))
#define E_CELL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), E_CELL_TYPE, ECellClass))
#define E_IS_CELL(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_CELL_TYPE))
#define E_IS_CELL_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), E_CELL_TYPE))

typedef gboolean (*ETableSearchFunc) (gconstpointer haystack,
				      const gchar *needle);

typedef enum {
	E_CELL_SELECTED       = 1 << 0,

	E_CELL_JUSTIFICATION  = 3 << 1,
	E_CELL_JUSTIFY_CENTER = 0 << 1,
	E_CELL_JUSTIFY_LEFT   = 1 << 1,
	E_CELL_JUSTIFY_RIGHT  = 2 << 1,
	E_CELL_JUSTIFY_FILL   = 3 << 1,

	E_CELL_ALIGN_LEFT     = 1 << 1,
	E_CELL_ALIGN_RIGHT    = 1 << 2,

	E_CELL_FOCUSED        = 1 << 3,

	E_CELL_EDITING        = 1 << 4,

	E_CELL_CURSOR         = 1 << 5,

	E_CELL_PREEDIT        = 1 << 6
} ECellFlags;

typedef enum {
	E_CELL_GRAB           = 1 << 0,
	E_CELL_UNGRAB         = 1 << 1
} ECellActions;

typedef struct {
	GObject parent;
} ECell;

typedef struct _ECellView {
	ECell *ecell;
	ETableModel *e_table_model;
	void        *e_table_item_view;

	gint   focus_x1, focus_y1, focus_x2, focus_y2;
	gint   focus_col, focus_row;

	void  (*kill_view_cb) (struct _ECellView*, gpointer );
	GList *kill_view_cb_data;
} ECellView;

#define E_CELL_IS_FOCUSED(ecell_view) (ecell_view->focus_x1 != -1)

typedef struct {
	GObjectClass parent_class;

	ECellView *(*new_view)         (ECell *ecell, ETableModel *table_model, gpointer e_table_item_view);
	void       (*kill_view)        (ECellView *ecell_view);

	void       (*realize)          (ECellView *ecell_view);
	void       (*unrealize)        (ECellView *ecell_view);

	void	   (*draw)             (ECellView *ecell_view, GdkDrawable *drawable,
					gint model_col, gint view_col, gint row,
					ECellFlags flags, gint x1, gint y1, gint x2, gint y2);
	gint	   (*event)            (ECellView *ecell_view, GdkEvent *event, gint model_col, gint view_col, gint row, ECellFlags flags, ECellActions *actions);
	void	   (*focus)            (ECellView *ecell_view, gint model_col, gint view_col,
					gint row, gint x1, gint y1, gint x2, gint y2);
	void	   (*unfocus)          (ECellView *ecell_view);
	gint        (*height)           (ECellView *ecell_view, gint model_col, gint view_col, gint row);

	void      *(*enter_edit)       (ECellView *ecell_view, gint model_col, gint view_col, gint row);
	void       (*leave_edit)       (ECellView *ecell_view, gint model_col, gint view_col, gint row, gpointer context);
	void      *(*save_state)       (ECellView *ecell_view, gint model_col, gint view_col, gint row, gpointer context);
	void       (*load_state)       (ECellView *ecell_view, gint model_col, gint view_col, gint row, gpointer context, gpointer save_state);
	void       (*free_state)       (ECellView *ecell_view, gint model_col, gint view_col, gint row, gpointer save_state);
	void       (*print)            (ECellView *ecell_view, GtkPrintContext *context,
					gint model_col, gint view_col, gint row,
					gdouble width, gdouble height);
	gdouble    (*print_height)     (ECellView *ecell_view,GtkPrintContext *context,
					gint model_col, gint view_col, gint row, gdouble width);
	gint        (*max_width)        (ECellView *ecell_view, gint model_col, gint view_col);
	gint        (*max_width_by_row) (ECellView *ecell_view, gint model_col, gint view_col, gint row);
	gchar     *(*get_bg_color)     (ECellView *ecell_view, gint row);

	void       (*style_set)        (ECellView *ecell_view, GtkStyle *previous_style);
} ECellClass;

GType      e_cell_get_type                      (void);

/* View creation methods. */
ECellView *e_cell_new_view                      (ECell             *ecell,
						 ETableModel       *table_model,
						 void              *e_table_item_view);
void       e_cell_kill_view                     (ECellView         *ecell_view);

/* Cell View methods. */
gint       e_cell_event                         (ECellView         *ecell_view,
						 GdkEvent          *event,
						 gint                model_col,
						 gint                view_col,
						 gint                row,
						 ECellFlags         flags,
						 ECellActions      *actions);
void       e_cell_realize                       (ECellView         *ecell_view);
void       e_cell_unrealize                     (ECellView         *ecell_view);
void       e_cell_draw                          (ECellView         *ecell_view,
						 GdkDrawable       *drawable,
						 gint                model_col,
						 gint                view_col,
						 gint                row,
						 ECellFlags         flags,
						 gint                x1,
						 gint                y1,
						 gint                x2,
						 gint                y2);
void       e_cell_print                         (ECellView         *ecell_view,
						 GtkPrintContext *context,
						 gint                model_col,
						 gint                view_col,
						 gint                row,
						 gdouble             width,
						 gdouble             height);
gdouble    e_cell_print_height                  (ECellView         *ecell_view,
						 GtkPrintContext *context,
						 gint                model_col,
						 gint                view_col,
						 gint                row,
						 gdouble            width);
gint        e_cell_max_width                     (ECellView         *ecell_view,
						 gint                model_col,
						 gint                view_col);
gint        e_cell_max_width_by_row              (ECellView         *ecell_view,
						 gint                model_col,
						 gint                view_col,
						 gint                row);
gboolean   e_cell_max_width_by_row_implemented  (ECellView         *ecell_view);
gchar     *e_cell_get_bg_color                  (ECellView         *ecell_view,
						 gint                row);
void       e_cell_style_set                     (ECellView         *ecell_view,
						 GtkStyle          *previous_style);

void       e_cell_focus                         (ECellView         *ecell_view,
						 gint                model_col,
						 gint                view_col,
						 gint                row,
						 gint                x1,
						 gint                y1,
						 gint                x2,
						 gint                y2);
void       e_cell_unfocus                       (ECellView         *ecell_view);
gint        e_cell_height                        (ECellView         *ecell_view,
						 gint                model_col,
						 gint                view_col,
						 gint                row);
void      *e_cell_enter_edit                    (ECellView         *ecell_view,
						 gint                model_col,
						 gint                view_col,
						 gint                row);
void       e_cell_leave_edit                    (ECellView         *ecell_view,
						 gint                model_col,
						 gint                view_col,
						 gint                row,
						 void              *edit_context);
void      *e_cell_save_state                    (ECellView         *ecell_view,
						 gint                model_col,
						 gint                view_col,
						 gint                row,
						 void              *edit_context);
void       e_cell_load_state                    (ECellView         *ecell_view,
						 gint                model_col,
						 gint                view_col,
						 gint                row,
						 void              *edit_context,
						 void              *state);
void       e_cell_free_state                    (ECellView         *ecell_view,
						 gint                model_col,
						 gint                view_col,
						 gint                row,
						 void              *state);

G_END_DECLS

#endif /* _E_CELL_H_ */
