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
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <libgnomecanvas/gnome-canvas-rect-ellipse.h>

#include "gal-a11y-e-tree.h"
#include <glib/gi18n.h>
#include "e-util/e-util.h"
#include "misc/e-canvas.h"
#include "misc/e-canvas-utils.h"
#include "misc/e-canvas-background.h"
#include "text/e-text.h"

#include "e-table-column-specification.h"
#include "e-table-header-item.h"
#include "e-table-header.h"
#include "e-table-item.h"
#include "e-table-sort-info.h"
#include "e-table-utils.h"
#ifdef E_TREE_USE_TREE_SELECTION
#include "e-tree-selection-model.h"
#else
#include "e-table-selection-model.h"
#endif
#include "e-tree.h"
#include "e-tree-table-adapter.h"

#define COLUMN_HEADER_HEIGHT 16

#define d(x)

enum {
	CURSOR_CHANGE,
	CURSOR_ACTIVATED,
	SELECTION_CHANGE,
	DOUBLE_CLICK,
	RIGHT_CLICK,
	CLICK,
	KEY_PRESS,
	START_DRAG,
	STATE_CHANGE,
	WHITE_SPACE_EVENT,

	CUT_CLIPBOARD,
	COPY_CLIPBOARD,
	PASTE_CLIPBOARD,
	SELECT_ALL,

	TREE_DRAG_BEGIN,
	TREE_DRAG_END,
	TREE_DRAG_DATA_GET,
	TREE_DRAG_DATA_DELETE,

	TREE_DRAG_LEAVE,
	TREE_DRAG_MOTION,
	TREE_DRAG_DROP,
	TREE_DRAG_DATA_RECEIVED,

	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_LENGTH_THRESHOLD,
	PROP_HORIZONTAL_DRAW_GRID,
	PROP_VERTICAL_DRAW_GRID,
	PROP_DRAW_FOCUS,
	PROP_ETTA,
	PROP_UNIFORM_ROW_HEIGHT,
	PROP_ALWAYS_SEARCH
};

enum {
	ET_SCROLL_UP = 1 << 0,
	ET_SCROLL_DOWN = 1 << 1,
	ET_SCROLL_LEFT = 1 << 2,
	ET_SCROLL_RIGHT = 1 << 3
};

struct ETreePriv {
	ETreeModel *model;
	ETreeTableAdapter *etta;

	ETableHeader *full_header, *header;

	guint structure_change_id, expansion_change_id;

	ETableSortInfo *sort_info;
	ESorter   *sorter;

	guint sort_info_change_id, group_info_change_id;

	ESelectionModel *selection;
	ETableSpecification *spec;

	ETableSearch     *search;

	ETableCol        *current_search_col;

	guint	  search_search_id;
	guint	  search_accept_id;

	gint reflow_idle_id;
	gint scroll_idle_id;
	gint hover_idle_id;

	gboolean show_cursor_after_reflow;

	gint table_model_change_id;
	gint table_row_change_id;
	gint table_cell_change_id;
	gint table_rows_delete_id;

	GnomeCanvasItem *info_text;
	guint info_text_resize_id;

	GnomeCanvas *header_canvas, *table_canvas;

	GnomeCanvasItem *header_item, *root;

	GnomeCanvasItem *white_item;
	GnomeCanvasItem *item;

	gint length_threshold;

	/*
	 * Configuration settings
	 */
	guint alternating_row_colors : 1;
	guint horizontal_draw_grid : 1;
	guint vertical_draw_grid : 1;
	guint draw_focus : 1;
	guint row_selection_active : 1;

	guint horizontal_scrolling : 1;

	guint scroll_direction : 4;

	guint do_drag : 1;

	guint uniform_row_height : 1;

	guint search_col_set : 1;
	guint always_search : 1;

	ECursorMode cursor_mode;

	gint drop_row;
	ETreePath drop_path;
	gint drop_col;

	GnomeCanvasItem *drop_highlight;
	gint last_drop_x;
	gint last_drop_y;
	gint last_drop_time;
	GdkDragContext *last_drop_context;

	gint hover_x;
	gint hover_y;

	gint drag_row;
	ETreePath drag_path;
	gint drag_col;
	ETreeDragSourceSite *site;

	GList *expanded_list;
};

static guint et_signals[LAST_SIGNAL] = { 0, };

static void et_grab_focus (GtkWidget *widget);

static void et_drag_begin (GtkWidget *widget,
			   GdkDragContext *context,
			   ETree *et);
static void et_drag_end (GtkWidget *widget,
			 GdkDragContext *context,
			 ETree *et);
static void et_drag_data_get (GtkWidget *widget,
			     GdkDragContext *context,
			     GtkSelectionData *selection_data,
			     guint info,
			     guint time,
			     ETree *et);
static void et_drag_data_delete (GtkWidget *widget,
				GdkDragContext *context,
				ETree *et);

static void et_drag_leave (GtkWidget *widget,
			  GdkDragContext *context,
			  guint time,
			  ETree *et);
static gboolean et_drag_motion (GtkWidget *widget,
			       GdkDragContext *context,
			       gint x,
			       gint y,
			       guint time,
			       ETree *et);
static gboolean et_drag_drop (GtkWidget *widget,
			     GdkDragContext *context,
			     gint x,
			     gint y,
			     guint time,
			     ETree *et);
static void et_drag_data_received (GtkWidget *widget,
				  GdkDragContext *context,
				  gint x,
				  gint y,
				  GtkSelectionData *selection_data,
				  guint info,
				  guint time,
				  ETree *et);

static void scroll_off (ETree *et);
static void scroll_on (ETree *et, guint scroll_direction);
static void hover_off (ETree *et);
static void hover_on (ETree *et, gint x, gint y);
static void context_destroyed (gpointer data, GObject *ctx);

G_DEFINE_TYPE (ETree, e_tree, GTK_TYPE_TABLE)

static void
et_disconnect_from_etta (ETree *et)
{
	if (et->priv->table_model_change_id != 0)
		g_signal_handler_disconnect (G_OBJECT (et->priv->etta),
					     et->priv->table_model_change_id);
	if (et->priv->table_row_change_id != 0)
		g_signal_handler_disconnect (G_OBJECT (et->priv->etta),
					     et->priv->table_row_change_id);
	if (et->priv->table_cell_change_id != 0)
		g_signal_handler_disconnect (G_OBJECT (et->priv->etta),
					     et->priv->table_cell_change_id);
	if (et->priv->table_rows_delete_id != 0)
		g_signal_handler_disconnect (G_OBJECT (et->priv->etta),
					     et->priv->table_rows_delete_id);

	et->priv->table_model_change_id = 0;
	et->priv->table_row_change_id = 0;
	et->priv->table_cell_change_id = 0;
	et->priv->table_rows_delete_id = 0;
}

static void
clear_current_search_col (ETree *et)
{
	et->priv->search_col_set = FALSE;
}

static ETableCol *
current_search_col (ETree *et)
{
	if (!et->priv->search_col_set) {
		et->priv->current_search_col =
			e_table_util_calculate_current_search_col (et->priv->header,
								   et->priv->full_header,
								   et->priv->sort_info,
								   et->priv->always_search);
		et->priv->search_col_set = TRUE;
	}

	return et->priv->current_search_col;
}

static void
e_tree_state_change (ETree *et)
{
	g_signal_emit (G_OBJECT (et), et_signals[STATE_CHANGE], 0);
}

static void
change_trigger (GtkObject *object, ETree *et)
{
	e_tree_state_change (et);
}

static void
search_col_change_trigger (GtkObject *object, ETree *et)
{
	clear_current_search_col (et);
	e_tree_state_change (et);
}

static void
disconnect_header (ETree *e_tree)
{
	if (e_tree->priv->header == NULL)
		return;

	if (e_tree->priv->structure_change_id)
		g_signal_handler_disconnect (G_OBJECT (e_tree->priv->header),
					     e_tree->priv->structure_change_id);
	if (e_tree->priv->expansion_change_id)
		g_signal_handler_disconnect (G_OBJECT (e_tree->priv->header),
					     e_tree->priv->expansion_change_id);
	if (e_tree->priv->sort_info) {
		if (e_tree->priv->sort_info_change_id)
			g_signal_handler_disconnect (G_OBJECT (e_tree->priv->sort_info),
						     e_tree->priv->sort_info_change_id);
		if (e_tree->priv->group_info_change_id)
			g_signal_handler_disconnect (G_OBJECT (e_tree->priv->sort_info),
						     e_tree->priv->group_info_change_id);

		g_object_unref (e_tree->priv->sort_info);
	}
	g_object_unref (e_tree->priv->header);
	e_tree->priv->header = NULL;
	e_tree->priv->sort_info = NULL;
}

static void
connect_header (ETree *e_tree, ETableState *state)
{
	GValue *val = g_new0 (GValue, 1);

	if (e_tree->priv->header != NULL)
		disconnect_header (e_tree);

	e_tree->priv->header = e_table_state_to_header (
		GTK_WIDGET (e_tree), e_tree->priv->full_header, state);

	e_tree->priv->structure_change_id = g_signal_connect (
		e_tree->priv->header, "structure_change",
		G_CALLBACK (search_col_change_trigger), e_tree);

	e_tree->priv->expansion_change_id = g_signal_connect (
		e_tree->priv->header, "expansion_change",
		G_CALLBACK (change_trigger), e_tree);

	if (state->sort_info) {
		e_tree->priv->sort_info = e_table_sort_info_duplicate (state->sort_info);
		e_table_sort_info_set_can_group (e_tree->priv->sort_info, FALSE);
		e_tree->priv->sort_info_change_id = g_signal_connect (
			e_tree->priv->sort_info, "sort_info_changed",
			G_CALLBACK (search_col_change_trigger), e_tree);

		e_tree->priv->group_info_change_id = g_signal_connect (
			e_tree->priv->sort_info, "group_info_changed",
			G_CALLBACK (search_col_change_trigger), e_tree);
	} else
		e_tree->priv->sort_info = NULL;

	g_value_init (val, G_TYPE_OBJECT);
	g_value_set_object (val, e_tree->priv->sort_info);
	g_object_set_property (G_OBJECT(e_tree->priv->header), "sort_info", val);
	g_free (val);
}

static void
et_dispose (GObject *object)
{
	ETree *et = E_TREE (object);

	if (et->priv) {

		if (et->priv->search) {
			if (et->priv->search_search_id)
				g_signal_handler_disconnect (et->priv->search,
							     et->priv->search_search_id);
			if (et->priv->search_accept_id)
				g_signal_handler_disconnect (et->priv->search,
							     et->priv->search_accept_id);
			g_object_unref (et->priv->search);
		}

		if (et->priv->reflow_idle_id)
			g_source_remove (et->priv->reflow_idle_id);
		et->priv->reflow_idle_id = 0;

		scroll_off (et);
		hover_off (et);
		g_list_foreach (
			et->priv->expanded_list,
			(GFunc) g_free, NULL);
		g_list_free (et->priv->expanded_list);

		et_disconnect_from_etta (et);

		g_object_unref (et->priv->etta);
		g_object_unref (et->priv->model);
		g_object_unref (et->priv->full_header);
		disconnect_header (et);
		g_object_unref (et->priv->selection);
		if (et->priv->spec)
			g_object_unref (et->priv->spec);
		et->priv->spec = NULL;

		if (et->priv->sorter)
			g_object_unref (et->priv->sorter);
		et->priv->sorter = NULL;

		if (et->priv->header_canvas)
			gtk_widget_destroy (GTK_WIDGET (et->priv->header_canvas));
		et->priv->header_canvas = NULL;

		if (et->priv->site)
			e_tree_drag_source_unset (et);

		if (et->priv->last_drop_context)
			g_object_weak_unref (G_OBJECT (et->priv->last_drop_context), context_destroyed, et);
		et->priv->last_drop_context = NULL;

		if (et->priv->info_text)
			gtk_object_destroy (GTK_OBJECT (et->priv->info_text));
		et->priv->info_text = NULL;
		et->priv->info_text_resize_id = 0;

		gtk_widget_destroy (GTK_WIDGET (et->priv->table_canvas));

		g_free (et->priv);
		et->priv = NULL;
	}

	if (G_OBJECT_CLASS (e_tree_parent_class)->dispose)
		G_OBJECT_CLASS (e_tree_parent_class)->dispose (object);
}

static void
et_unrealize (GtkWidget *widget)
{
	scroll_off (E_TREE (widget));
	hover_off (E_TREE (widget));

	if (GTK_WIDGET_CLASS (e_tree_parent_class)->unrealize)
		GTK_WIDGET_CLASS (e_tree_parent_class)->unrealize (widget);
}

typedef struct {
	ETree *et;
	gchar *string;
} SearchSearchStruct;

static gboolean
search_search_callback (ETreeModel *model, ETreePath path, gpointer data)
{
	SearchSearchStruct *cb_data = data;
	gconstpointer value;
	ETableCol *col = current_search_col (cb_data->et);

	value = e_tree_model_value_at (
		model, path, cb_data->et->priv->current_search_col->col_idx);

	return col->search (value, cb_data->string);
}

static gboolean
et_search_search (ETableSearch *search,
                  gchar *string,
                  ETableSearchFlags flags,
                  ETree *et)
{
	ETreePath cursor;
	ETreePath found;
	SearchSearchStruct cb_data;
	ETableCol *col = current_search_col (et);

	if (col == NULL)
		return FALSE;

	cb_data.et = et;
	cb_data.string = string;

	cursor = e_tree_get_cursor (et);

	if (cursor && (flags & E_TABLE_SEARCH_FLAGS_CHECK_CURSOR_FIRST)) {
		gconstpointer value;

		value = e_tree_model_value_at (et->priv->model, cursor, col->col_idx);

		if (col->search (value, string)) {
			return TRUE;
		}
	}

	found = e_tree_model_node_find (
			et->priv->model, cursor, NULL,
			E_TREE_FIND_NEXT_FORWARD,
			search_search_callback, &cb_data);
	if (found == NULL)
		found = e_tree_model_node_find (
			et->priv->model, NULL, cursor,
			E_TREE_FIND_NEXT_FORWARD,
			search_search_callback, &cb_data);

	if (found && found != cursor) {
		gint model_row;

		e_tree_table_adapter_show_node (et->priv->etta, found);
		model_row = e_tree_table_adapter_row_of_node (et->priv->etta, found);

		e_selection_model_select_as_key_press (
			E_SELECTION_MODEL (et->priv->selection),
			model_row, col->col_idx, GDK_CONTROL_MASK);
		return TRUE;
	} else if (cursor && !(flags & E_TABLE_SEARCH_FLAGS_CHECK_CURSOR_FIRST)) {
		gconstpointer value;

		value = e_tree_model_value_at (et->priv->model, cursor, col->col_idx);

		return col->search (value, string);
	} else
		return FALSE;
}

static void
et_search_accept (ETableSearch *search, ETree *et)
{
	ETableCol *col = current_search_col (et);
	gint cursor;

	if (col == NULL)
		return;

	g_object_get (et->priv->selection, "cursor_row", &cursor, NULL);

	e_selection_model_select_as_key_press (
		E_SELECTION_MODEL (et->priv->selection),
		cursor, col->col_idx, 0);
}

static void
e_tree_init (ETree *e_tree)
{
	gtk_widget_set_can_focus (GTK_WIDGET (e_tree), TRUE);

	gtk_table_set_homogeneous (GTK_TABLE (e_tree), FALSE);

	e_tree->priv                         = g_new (ETreePriv, 1);

	e_tree->priv->model                  = NULL;
	e_tree->priv->etta                   = NULL;

	e_tree->priv->full_header            = NULL;
	e_tree->priv->header                 = NULL;

	e_tree->priv->structure_change_id    = 0;
	e_tree->priv->expansion_change_id    = 0;
	e_tree->priv->sort_info_change_id    = 0;
	e_tree->priv->group_info_change_id   = 0;

	e_tree->priv->sort_info              = NULL;
	e_tree->priv->sorter                 = NULL;
	e_tree->priv->reflow_idle_id         = 0;
	e_tree->priv->scroll_idle_id         = 0;
	e_tree->priv->hover_idle_id          = 0;

	e_tree->priv->show_cursor_after_reflow = FALSE;

	e_tree->priv->table_model_change_id  = 0;
	e_tree->priv->table_row_change_id    = 0;
	e_tree->priv->table_cell_change_id   = 0;
	e_tree->priv->table_rows_delete_id   = 0;

	e_tree->priv->alternating_row_colors = 1;
	e_tree->priv->horizontal_draw_grid   = 1;
	e_tree->priv->vertical_draw_grid     = 1;
	e_tree->priv->draw_focus             = 1;
	e_tree->priv->cursor_mode            = E_CURSOR_SIMPLE;
	e_tree->priv->length_threshold       = 200;
	e_tree->priv->uniform_row_height     = FALSE;

	e_tree->priv->row_selection_active   = FALSE;
	e_tree->priv->horizontal_scrolling   = FALSE;
	e_tree->priv->scroll_direction       = 0;

	e_tree->priv->drop_row               = -1;
	e_tree->priv->drop_path              = NULL;
	e_tree->priv->drop_col               = -1;
	e_tree->priv->drop_highlight         = NULL;

	e_tree->priv->last_drop_x            = 0;
	e_tree->priv->last_drop_y            = 0;
	e_tree->priv->last_drop_time         = 0;
	e_tree->priv->last_drop_context      = NULL;

	e_tree->priv->hover_x                = 0;
	e_tree->priv->hover_y                = 0;

	e_tree->priv->drag_row               = -1;
	e_tree->priv->drag_path              = NULL;
	e_tree->priv->drag_col               = -1;

	e_tree->priv->expanded_list          = NULL;

	e_tree->priv->site                   = NULL;
	e_tree->priv->do_drag                = FALSE;

#ifdef E_TREE_USE_TREE_SELECTION
	e_tree->priv->selection =
		E_SELECTION_MODEL (e_tree_selection_model_new ());
#else
	e_tree->priv->selection =
		E_SELECTION_MODEL (e_table_selection_model_new ());
#endif
	e_tree->priv->spec                   = NULL;

	e_tree->priv->info_text              = NULL;
	e_tree->priv->info_text_resize_id    = 0;

	e_tree->priv->header_canvas          = NULL;
	e_tree->priv->table_canvas           = NULL;

	e_tree->priv->header_item            = NULL;
	e_tree->priv->root                   = NULL;

	e_tree->priv->white_item             = NULL;
	e_tree->priv->item                   = NULL;

	e_tree->priv->search                 = e_table_search_new ();

	e_tree->priv->search_search_id       =
		g_signal_connect (G_OBJECT (e_tree->priv->search), "search",
				  G_CALLBACK (et_search_search), e_tree);
	e_tree->priv->search_accept_id       =
		g_signal_connect (G_OBJECT (e_tree->priv->search), "accept",
				  G_CALLBACK (et_search_accept), e_tree);

	e_tree->priv->current_search_col     = NULL;
	e_tree->priv->search_col_set         = FALSE;
	e_tree->priv->always_search          = g_getenv ("GAL_ALWAYS_SEARCH") ? TRUE : FALSE;
}

/* Grab_focus handler for the ETree */
static void
et_grab_focus (GtkWidget *widget)
{
	ETree *e_tree;

	e_tree = E_TREE (widget);

	gtk_widget_grab_focus (GTK_WIDGET (e_tree->priv->table_canvas));
}

/* Focus handler for the ETree */
static gint
et_focus (GtkWidget *container, GtkDirectionType direction)
{
	ETree *e_tree;

	e_tree = E_TREE (container);

	if (gtk_container_get_focus_child (GTK_CONTAINER (container))) {
		gtk_container_set_focus_child (GTK_CONTAINER (container), NULL);
		return FALSE;
	}

	return gtk_widget_child_focus (GTK_WIDGET (e_tree->priv->table_canvas), direction);
}

static void
set_header_canvas_width (ETree *e_tree)
{
	gdouble oldwidth, oldheight, width;

	if (!(e_tree->priv->header_item &&
		e_tree->priv->header_canvas && e_tree->priv->table_canvas))
		return;

	gnome_canvas_get_scroll_region (GNOME_CANVAS (e_tree->priv->table_canvas),
					NULL, NULL, &width, NULL);
	gnome_canvas_get_scroll_region (GNOME_CANVAS (e_tree->priv->header_canvas),
					NULL, NULL, &oldwidth, &oldheight);

	if (oldwidth != width ||
	    oldheight != E_TABLE_HEADER_ITEM (e_tree->priv->header_item)->height - 1)
		gnome_canvas_set_scroll_region (
						GNOME_CANVAS (e_tree->priv->header_canvas),
						0, 0, width, /*  COLUMN_HEADER_HEIGHT - 1 */
						E_TABLE_HEADER_ITEM (e_tree->priv->header_item)->height - 1);

}

static void
header_canvas_size_allocate (GtkWidget *widget, GtkAllocation *alloc, ETree *e_tree)
{
	GtkAllocation allocation;

	set_header_canvas_width (e_tree);

	widget = GTK_WIDGET (e_tree->priv->header_canvas);
	gtk_widget_get_allocation (widget, &allocation);

	/* When the header item is created ->height == 0,
	   as the font is only created when everything is realized.
	   So we set the usize here as well, so that the size of the
	   header is correct */
	if (allocation.height != E_TABLE_HEADER_ITEM (e_tree->priv->header_item)->height)
		gtk_widget_set_size_request (
			widget, -1,
			E_TABLE_HEADER_ITEM (e_tree->priv->header_item)->height);
}

static void
e_tree_setup_header (ETree *e_tree)
{
	GtkWidget *widget;
	gchar *pointer;

	widget = e_canvas_new ();
	gtk_widget_set_can_focus (widget, FALSE);
	e_tree->priv->header_canvas = GNOME_CANVAS (widget);
	gtk_widget_show (widget);

	pointer = g_strdup_printf("%p", (gpointer) e_tree);

	e_tree->priv->header_item = gnome_canvas_item_new (
		gnome_canvas_root (e_tree->priv->header_canvas),
		e_table_header_item_get_type (),
		"ETableHeader", e_tree->priv->header,
		"full_header", e_tree->priv->full_header,
		"sort_info", e_tree->priv->sort_info,
		"dnd_code", pointer,
		"tree", e_tree,
		NULL);

	g_free (pointer);

	g_signal_connect (
		e_tree->priv->header_canvas, "size_allocate",
		G_CALLBACK (header_canvas_size_allocate), e_tree);

	gtk_widget_set_size_request (
		GTK_WIDGET (e_tree->priv->header_canvas), -1,
		E_TABLE_HEADER_ITEM (e_tree->priv->header_item)->height);
}

static void
scroll_to_cursor (ETree *e_tree)
{
	ETreePath path;
	GtkAdjustment *adjustment;
	GtkLayout *layout;
	gint x, y, w, h;
	gdouble page_size;
	gdouble lower;
	gdouble upper;
	gdouble value;

	path = e_tree_get_cursor (e_tree);
	x = y = w = h = 0;

	if (path) {
		gint row = e_tree_row_of_node (e_tree, path);
		gint col = 0;

		if (row >= 0)
			e_table_item_get_cell_geometry (
				E_TABLE_ITEM (e_tree->priv->item),
				&row, &col, &x, &y, &w, &h);
	}

	layout = GTK_LAYOUT (e_tree->priv->table_canvas);
	adjustment = gtk_layout_get_vadjustment (layout);

	page_size = gtk_adjustment_get_page_size (adjustment);
	lower = gtk_adjustment_get_lower (adjustment);
	upper = gtk_adjustment_get_upper (adjustment);
	value = gtk_adjustment_get_value (adjustment);

	if (y < value || y + h > value + page_size) {
		value = CLAMP (y - page_size / 2, lower, upper - page_size);
		gtk_adjustment_set_value (adjustment, value);
	}
}

static gboolean
tree_canvas_reflow_idle (ETree *e_tree)
{
	gdouble height, width;
	gdouble oldheight, oldwidth;
	GtkAllocation allocation;
	GtkWidget *widget;

	widget = GTK_WIDGET (e_tree->priv->table_canvas);
	gtk_widget_get_allocation (widget, &allocation);

	g_object_get (
		e_tree->priv->item,
		"height", &height, "width", &width, NULL);

	height = MAX ((gint)height, allocation.height);
	width = MAX ((gint)width, allocation.width);

	/* I have no idea why this needs to be -1, but it works. */
	gnome_canvas_get_scroll_region (
		GNOME_CANVAS (e_tree->priv->table_canvas),
		NULL, NULL, &oldwidth, &oldheight);

	if (oldwidth != width - 1 ||
	    oldheight != height - 1) {
		gnome_canvas_set_scroll_region (
			GNOME_CANVAS (e_tree->priv->table_canvas),
			0, 0, width - 1, height - 1);
		set_header_canvas_width (e_tree);
	}

	e_tree->priv->reflow_idle_id = 0;

	if (e_tree->priv->show_cursor_after_reflow) {
		e_tree->priv->show_cursor_after_reflow = FALSE;
		scroll_to_cursor (e_tree);
	}

	return FALSE;
}

static void
tree_canvas_size_allocate (GtkWidget *widget, GtkAllocation *alloc,
			    ETree *e_tree)
{
	gdouble width;
	gdouble height;
	GValue *val = g_new0 (GValue, 1);
	g_value_init (val, G_TYPE_DOUBLE);

	width = alloc->width;
	g_value_set_double (val, width);
	g_object_get (e_tree->priv->item,
		      "height", &height,
		      NULL);
	height = MAX ((gint)height, alloc->height);

	g_object_set (e_tree->priv->item,
		      "width", width,
		      NULL);
	g_object_set_property (G_OBJECT (e_tree->priv->header), "width", val);
	g_free (val);

	if (e_tree->priv->reflow_idle_id)
		g_source_remove (e_tree->priv->reflow_idle_id);
	tree_canvas_reflow_idle (e_tree);
	scroll_to_cursor (e_tree);
}

static void
tree_canvas_reflow (GnomeCanvas *canvas, ETree *e_tree)
{
	if (!e_tree->priv->reflow_idle_id)
		e_tree->priv->reflow_idle_id = g_idle_add_full (
			400, (GSourceFunc) tree_canvas_reflow_idle,
			e_tree, NULL);
}

static void
item_cursor_change (ETableItem *eti, gint row, ETree *et)
{
	ETreePath path = e_tree_table_adapter_node_at_row (et->priv->etta, row);

	g_signal_emit (et, et_signals[CURSOR_CHANGE], 0, row, path);
}

static void
item_cursor_activated (ETableItem *eti, gint row, ETree *et)
{
	ETreePath path = e_tree_table_adapter_node_at_row (et->priv->etta, row);

	g_signal_emit (et, et_signals[CURSOR_ACTIVATED], 0, row, path);
}

static void
item_double_click (ETableItem *eti, gint row, gint col, GdkEvent *event, ETree *et)
{
	ETreePath path = e_tree_table_adapter_node_at_row (et->priv->etta, row);

	g_signal_emit (et, et_signals[DOUBLE_CLICK], 0, row, path, col, event);
}

static gboolean
item_right_click (ETableItem *eti, gint row, gint col, GdkEvent *event, ETree *et)
{
	ETreePath path = e_tree_table_adapter_node_at_row (et->priv->etta, row);
	gboolean return_val = 0;

	g_signal_emit (
		et, et_signals[RIGHT_CLICK], 0,
		row, path, col, event, &return_val);

	return return_val;
}

static gboolean
item_click (ETableItem *eti, gint row, gint col, GdkEvent *event, ETree *et)
{
	gboolean return_val = 0;
	ETreePath path = e_tree_table_adapter_node_at_row (et->priv->etta, row);

	g_signal_emit (
		et, et_signals[CLICK], 0, row, path, col, event, &return_val);

	return return_val;
}

static gint
item_key_press (ETableItem *eti, gint row, gint col, GdkEvent *event, ETree *et)
{
	gint return_val = 0;
	GdkEventKey *key = (GdkEventKey *) event;
	ETreePath path;
	gint y, row_local, col_local;
	GtkAdjustment *adjustment;
	GtkLayout *layout;
	gdouble page_size;
	gdouble upper;
	gdouble value;

	layout = GTK_LAYOUT (et->priv->table_canvas);
	adjustment = gtk_layout_get_vadjustment (layout);

	page_size = gtk_adjustment_get_page_size (adjustment);
	upper = gtk_adjustment_get_upper (adjustment);
	value = gtk_adjustment_get_value (adjustment);

	switch (key->keyval) {
	case GDK_Page_Down:
	case GDK_KP_Page_Down:
		y = CLAMP (value + (2 * page_size - 50), 0, upper);
		y -= value;
		e_tree_get_cell_at (et, 30, y, &row_local, &col_local);

		if (row_local == -1)
			row_local = e_table_model_row_count (E_TABLE_MODEL (et->priv->etta)) - 1;

		row_local = e_tree_view_to_model_row (et, row_local);
		col_local = e_selection_model_cursor_col (
			E_SELECTION_MODEL (et->priv->selection));
		e_selection_model_select_as_key_press (
			E_SELECTION_MODEL (et->priv->selection),
			row_local, col_local, key->state);

		return_val = 1;
		break;
	case GDK_Page_Up:
	case GDK_KP_Page_Up:
		y = CLAMP (value - (page_size - 50), 0, upper);
		y -= value;
		e_tree_get_cell_at (et, 30, y, &row_local, &col_local);

		if (row_local == -1)
			row_local = e_table_model_row_count (E_TABLE_MODEL (et->priv->etta)) - 1;

		row_local = e_tree_view_to_model_row (et, row_local);
		col_local = e_selection_model_cursor_col (
			E_SELECTION_MODEL (et->priv->selection));
		e_selection_model_select_as_key_press (
			E_SELECTION_MODEL (et->priv->selection),
			row_local, col_local, key->state);

		return_val = 1;
		break;
	case GDK_plus:
	case GDK_KP_Add:
	case GDK_Right:
	case GDK_KP_Right:
		/* Only allow if the Shift modifier is used.
		 * eg. Ctrl-Equal shouldn't be handled.  */
		if ((key->state & (GDK_SHIFT_MASK | GDK_LOCK_MASK |
			GDK_MOD1_MASK)) != GDK_SHIFT_MASK)
			break;
		if (row != -1) {
			path = e_tree_table_adapter_node_at_row (et->priv->etta, row);
			if (path)
				e_tree_table_adapter_node_set_expanded (et->priv->etta, path, TRUE);
		}
		return_val = 1;
		break;
	case GDK_underscore:
	case GDK_KP_Subtract:
	case GDK_Left:
	case GDK_KP_Left:
		/* Only allow if the Shift modifier is used.
		 * eg. Ctrl-Minus shouldn't be handled.  */
		if ((key->state & (GDK_SHIFT_MASK | GDK_LOCK_MASK |
			GDK_MOD1_MASK)) != GDK_SHIFT_MASK)
			break;
		if (row != -1) {
			path = e_tree_table_adapter_node_at_row (et->priv->etta, row);
			if (path)
				e_tree_table_adapter_node_set_expanded (et->priv->etta, path, FALSE);
		}
		return_val = 1;
		break;
	case GDK_BackSpace:
		if (e_table_search_backspace (et->priv->search))
			return TRUE;
		/* Fallthrough */
	default:
		if ((key->state & ~(GDK_SHIFT_MASK | GDK_LOCK_MASK |
			GDK_MOD1_MASK | GDK_MOD2_MASK | GDK_MOD3_MASK |
			GDK_MOD4_MASK | GDK_MOD5_MASK)) == 0
		    && ((key->keyval >= GDK_a && key->keyval <= GDK_z) ||
			(key->keyval >= GDK_A && key->keyval <= GDK_Z) ||
			(key->keyval >= GDK_0 && key->keyval <= GDK_9))) {
			e_table_search_input_character (et->priv->search, key->keyval);
		}
		path = e_tree_table_adapter_node_at_row (et->priv->etta, row);
		g_signal_emit (et,
			       et_signals[KEY_PRESS], 0,
			       row, path, col, event, &return_val);
		break;
	}
	return return_val;
}

static gint
item_start_drag (ETableItem *eti, gint row, gint col, GdkEvent *event, ETree *et)
{
	ETreePath path;
	gint return_val = 0;

	path = e_tree_table_adapter_node_at_row (et->priv->etta, row);

	g_signal_emit (
		et, et_signals[START_DRAG], 0,
		row, path, col, event, &return_val);

	return return_val;
}

static void
et_selection_model_selection_changed (ETableSelectionModel *etsm, ETree *et)
{
	g_signal_emit (et, et_signals[SELECTION_CHANGE], 0);
}

static void
et_selection_model_selection_row_changed (ETableSelectionModel *etsm,
                                          gint row,
                                          ETree *et)
{
	g_signal_emit (et, et_signals[SELECTION_CHANGE], 0);
}

static void
et_build_item (ETree *et)
{
	et->priv->item = gnome_canvas_item_new (
		GNOME_CANVAS_GROUP (
			gnome_canvas_root (et->priv->table_canvas)),
		e_table_item_get_type (),
		"ETableHeader", et->priv->header,
		"ETableModel", et->priv->etta,
		"selection_model", et->priv->selection,
		"alternating_row_colors", et->priv->alternating_row_colors,
		"horizontal_draw_grid", et->priv->horizontal_draw_grid,
		"vertical_draw_grid", et->priv->vertical_draw_grid,
		"drawfocus", et->priv->draw_focus,
		"cursor_mode", et->priv->cursor_mode,
		"length_threshold", et->priv->length_threshold,
		"uniform_row_height", et->priv->uniform_row_height,
		NULL);

	g_signal_connect (et->priv->item, "cursor_change",
			  G_CALLBACK (item_cursor_change), et);
	g_signal_connect (et->priv->item, "cursor_activated",
			  G_CALLBACK (item_cursor_activated), et);
	g_signal_connect (et->priv->item, "double_click",
			  G_CALLBACK (item_double_click), et);
	g_signal_connect (et->priv->item, "right_click",
			  G_CALLBACK (item_right_click), et);
	g_signal_connect (et->priv->item, "click",
			  G_CALLBACK (item_click), et);
	g_signal_connect (et->priv->item, "key_press",
			  G_CALLBACK (item_key_press), et);
	g_signal_connect (et->priv->item, "start_drag",
			  G_CALLBACK (item_start_drag), et);
}

static void
et_canvas_style_set (GtkWidget *widget, GtkStyle *prev_style)
{
	GtkStyle *style;

	style = gtk_widget_get_style (widget);

	gnome_canvas_item_set (
		E_TREE (widget)->priv->white_item,
		"fill_color_gdk", &style->base[GTK_STATE_NORMAL],
		NULL);
}

static gboolean
white_item_event (GnomeCanvasItem *white_item, GdkEvent *event, ETree *e_tree)
{
	gboolean return_val = 0;
	g_signal_emit (e_tree,
		       et_signals[WHITE_SPACE_EVENT], 0,
		       event, &return_val);
	return return_val;
}

static gint
et_canvas_root_event (GnomeCanvasItem *root, GdkEvent *event, ETree *e_tree)
{
	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_BUTTON_RELEASE:
		if (event->button.button != 4 && event->button.button != 5) {
			if (gtk_widget_has_focus (GTK_WIDGET (root->canvas))) {
				GnomeCanvasItem *item = GNOME_CANVAS (root->canvas)->focused_item;

				if (E_IS_TABLE_ITEM (item)) {
					e_table_item_leave_edit (E_TABLE_ITEM (item));
					return TRUE;
				}
			}
		}
		break;
	default:
		break;
	}

	return FALSE;
}

/* Handler for focus events in the table_canvas; we have to repaint ourselves
 * and give the focus to some ETableItem.
 */
static gint
table_canvas_focus_event_cb (GtkWidget *widget, GdkEventFocus *event, gpointer data)
{
	GnomeCanvas *canvas;
	ETree *tree;

	gtk_widget_queue_draw (widget);

	if (!event->in)
		return TRUE;

	canvas = GNOME_CANVAS (widget);
	tree = E_TREE (data);

	if (!canvas->focused_item ||
		(e_selection_model_cursor_row (tree->priv->selection) == -1)) {
		e_table_item_set_cursor (E_TABLE_ITEM (tree->priv->item), 0, 0);
		gnome_canvas_item_grab_focus (tree->priv->item);
	}

	return TRUE;
}

static void
e_tree_setup_table (ETree *e_tree)
{
	GtkWidget *widget;
	GtkStyle *style;

	e_tree->priv->table_canvas = GNOME_CANVAS (e_canvas_new ());
	g_signal_connect (
		e_tree->priv->table_canvas, "size_allocate",
		G_CALLBACK (tree_canvas_size_allocate), e_tree);
	g_signal_connect (
		e_tree->priv->table_canvas, "focus_in_event",
		G_CALLBACK (table_canvas_focus_event_cb), e_tree);
	g_signal_connect (
		e_tree->priv->table_canvas, "focus_out_event",
		G_CALLBACK (table_canvas_focus_event_cb), e_tree);

	g_signal_connect (
		e_tree->priv->table_canvas, "drag_begin",
		G_CALLBACK (et_drag_begin), e_tree);
	g_signal_connect (
		e_tree->priv->table_canvas, "drag_end",
		G_CALLBACK (et_drag_end), e_tree);
	g_signal_connect (
		e_tree->priv->table_canvas, "drag_data_get",
		G_CALLBACK (et_drag_data_get), e_tree);
	g_signal_connect (
		e_tree->priv->table_canvas, "drag_data_delete",
		G_CALLBACK (et_drag_data_delete), e_tree);
	g_signal_connect (
		e_tree, "drag_motion",
		G_CALLBACK (et_drag_motion), e_tree);
	g_signal_connect (
		e_tree, "drag_leave",
		G_CALLBACK (et_drag_leave), e_tree);
	g_signal_connect (
		e_tree, "drag_drop",
		G_CALLBACK (et_drag_drop), e_tree);
	g_signal_connect (
		e_tree, "drag_data_received",
		G_CALLBACK (et_drag_data_received), e_tree);

	g_signal_connect (e_tree->priv->table_canvas, "reflow",
			  G_CALLBACK (tree_canvas_reflow), e_tree);

	widget = GTK_WIDGET (e_tree->priv->table_canvas);
	style = gtk_widget_get_style (widget);

	gtk_widget_show (widget);

	e_tree->priv->white_item = gnome_canvas_item_new (
		gnome_canvas_root (e_tree->priv->table_canvas),
		e_canvas_background_get_type (),
		"fill_color_gdk", &style->base[GTK_STATE_NORMAL],
		NULL);

	g_signal_connect (
		e_tree->priv->white_item, "event",
		G_CALLBACK (white_item_event), e_tree);
	g_signal_connect (
		gnome_canvas_root (e_tree->priv->table_canvas), "event",
		G_CALLBACK (et_canvas_root_event), e_tree);

	et_build_item (e_tree);
}

/**
 * e_tree_set_search_column:
 * @e_tree: #ETree object that will be modified
 * @col: Column index to use for searches
 *
 * This routine sets the current search column to be used for keypress
 * searches of the #ETree. If -1 is passed in for column, the current
 * search column is cleared.
 */
void
e_tree_set_search_column (ETree *e_tree, gint  col)
{
	if (col == -1) {
		clear_current_search_col (e_tree);
		return;
	}

	e_tree->priv->search_col_set = TRUE;
	e_tree->priv->current_search_col = e_table_header_get_column (
		e_tree->priv->full_header, col);
}

void
e_tree_set_state_object (ETree *e_tree, ETableState *state)
{
	GValue *val;
	GtkAllocation allocation;
	GtkWidget *widget;

	val = g_new0 (GValue, 1);
	g_value_init (val, G_TYPE_DOUBLE);

	connect_header (e_tree, state);

	widget = GTK_WIDGET (e_tree->priv->table_canvas);
	gtk_widget_get_allocation (widget, &allocation);

	g_value_set_double (val, (gdouble) allocation.width);
	g_object_set_property (G_OBJECT (e_tree->priv->header), "width", val);
	g_free (val);

	if (e_tree->priv->header_item)
		g_object_set (e_tree->priv->header_item,
			     "ETableHeader", e_tree->priv->header,
			     "sort_info", e_tree->priv->sort_info,
			     NULL);

	if (e_tree->priv->item)
		g_object_set (e_tree->priv->item,
			     "ETableHeader", e_tree->priv->header,
			     NULL);

	if (e_tree->priv->etta)
		e_tree_table_adapter_set_sort_info (e_tree->priv->etta, e_tree->priv->sort_info);

	e_tree_state_change (e_tree);
}

/**
 * e_tree_set_state:
 * @e_tree: #ETree object that will be modified
 * @state_str: a string with the XML representation of the #ETableState.
 *
 * This routine sets the state (as described by #ETableState) of the
 * #ETree object.
 */
void
e_tree_set_state (ETree      *e_tree,
		   const gchar *state_str)
{
	ETableState *state;

	g_return_if_fail (e_tree != NULL);
	g_return_if_fail (E_IS_TREE (e_tree));
	g_return_if_fail (state_str != NULL);

	state = e_table_state_new ();
	e_table_state_load_from_string (state, state_str);

	if (state->col_count > 0)
		e_tree_set_state_object (e_tree, state);

	g_object_unref (state);
}

/**
 * e_tree_load_state:
 * @e_tree: #ETree object that will be modified
 * @filename: name of the file containing the state to be loaded into the #ETree
 *
 * An #ETableState will be loaded form the file pointed by @filename into the
 * @e_tree object.
 */
void
e_tree_load_state (ETree      *e_tree,
		    const gchar *filename)
{
	ETableState *state;

	g_return_if_fail (e_tree != NULL);
	g_return_if_fail (E_IS_TREE (e_tree));
	g_return_if_fail (filename != NULL);

	state = e_table_state_new ();
	e_table_state_load_from_file (state, filename);

	if (state->col_count > 0)
		e_tree_set_state_object (e_tree, state);

	g_object_unref (state);
}

/**
 * e_tree_get_state_object:
 * @e_tree: #ETree object to act on
 *
 * Builds an #ETableState corresponding to the current state of the
 * #ETree.
 *
 * Return value:
 * The %ETableState object generated.
 **/
ETableState *
e_tree_get_state_object (ETree *e_tree)
{
	ETableState *state;
	gint full_col_count;
	gint i, j;

	state = e_table_state_new ();
	state->sort_info = e_tree->priv->sort_info;
	if (state->sort_info)
		g_object_ref (state->sort_info);

	state->col_count = e_table_header_count (e_tree->priv->header);
	full_col_count = e_table_header_count (e_tree->priv->full_header);
	state->columns = g_new (int, state->col_count);
	state->expansions = g_new (double, state->col_count);
	for (i = 0; i < state->col_count; i++) {
		ETableCol *col = e_table_header_get_column (e_tree->priv->header, i);
		state->columns[i] = -1;
		for (j = 0; j < full_col_count; j++) {
			if (col->col_idx == e_table_header_index (e_tree->priv->full_header, j)) {
				state->columns[i] = j;
				break;
			}
		}
		state->expansions[i] = col->expansion;
	}

	return state;
}

/**
 * e_tree_get_state:
 * @e_tree: The #ETree to act on
 *
 * Builds a state object based on the current state and returns the
 * string corresponding to that state.
 *
 * Return value:
 * A string describing the current state of the #ETree.
 **/
gchar *
e_tree_get_state (ETree *e_tree)
{
	ETableState *state;
	gchar *string;

	state = e_tree_get_state_object (e_tree);
	string = e_table_state_save_to_string (state);
	g_object_unref (state);
	return string;
}

/**
 * e_tree_save_state:
 * @e_tree: The #ETree to act on
 * @filename: name of the file to save to
 *
 * Saves the state of the @e_tree object into the file pointed by
 * @filename.
 **/
void
e_tree_save_state (ETree      *e_tree,
		   const gchar *filename)
{
	ETableState *state;

	state = e_tree_get_state_object (e_tree);
	e_table_state_save_to_file (state, filename);
	g_object_unref (state);
}

/**
 * e_tree_get_spec:
 * @e_tree: The #ETree to query
 *
 * Returns the specification object.
 *
 * Return value:
 **/
ETableSpecification *
e_tree_get_spec (ETree *e_tree)
{
	return e_tree->priv->spec;
}

static void
et_table_model_changed (ETableModel *model, ETree *et)
{
	if (et->priv->horizontal_scrolling)
		e_table_header_update_horizontal (et->priv->header);
}

static void
et_table_row_changed (ETableModel *table_model, gint row, ETree *et)
{
	et_table_model_changed (table_model, et);
}

static void
et_table_cell_changed (ETableModel *table_model, gint view_col, gint row, ETree *et)
{
	et_table_model_changed (table_model, et);
}

static void
et_table_rows_deleted (ETableModel *table_model, gint row, gint count, ETree *et)
{
	ETreePath * node, * prev_node;

	/* If the cursor is still valid after this deletion, we're done */
	if (e_selection_model_cursor_row (et->priv->selection) >= 0
			|| row == 0)
		return;

	prev_node = e_tree_node_at_row (et, row-1);
	node = e_tree_get_cursor (et);

	/* Check if the cursor is a child of the node directly before the
	 * deleted region (implying that an expander was collapsed with
	 * the cursor inside it) */
	while (node) {
		node = e_tree_model_node_get_parent (et->priv->model, node);
		if (node == prev_node) {
			/* Set the cursor to the still-visible parent */
			e_tree_set_cursor (et, prev_node);
			return;
		}
	}
}

static void
et_connect_to_etta (ETree *et)
{
	et->priv->table_model_change_id = g_signal_connect (
		et->priv->etta, "model_changed",
		G_CALLBACK (et_table_model_changed), et);

	et->priv->table_row_change_id = g_signal_connect (
		et->priv->etta, "model_row_changed",
		G_CALLBACK (et_table_row_changed), et);

	et->priv->table_cell_change_id = g_signal_connect (
		et->priv->etta, "model_cell_changed",
		G_CALLBACK (et_table_cell_changed), et);

	et->priv->table_rows_delete_id = g_signal_connect (
		et->priv->etta, "model_rows_deleted",
		G_CALLBACK (et_table_rows_deleted), et);

}

static gboolean
et_real_construct (ETree *e_tree, ETreeModel *etm, ETableExtras *ete,
		   ETableSpecification *specification, ETableState *state)
{
	GtkAdjustment *adjustment;
	GtkLayout *layout;
	gint row = 0;

	if (ete)
		g_object_ref (ete);
	else
		ete = e_table_extras_new ();

	e_tree->priv->alternating_row_colors = specification->alternating_row_colors;
	e_tree->priv->horizontal_draw_grid = specification->horizontal_draw_grid;
	e_tree->priv->vertical_draw_grid = specification->vertical_draw_grid;
	e_tree->priv->draw_focus = specification->draw_focus;
	e_tree->priv->cursor_mode = specification->cursor_mode;
	e_tree->priv->full_header = e_table_spec_to_full_header (specification, ete);

	connect_header (e_tree, state);

	e_tree->priv->horizontal_scrolling = specification->horizontal_scrolling;

	e_tree->priv->model = etm;
	g_object_ref (etm);

	e_tree->priv->etta = E_TREE_TABLE_ADAPTER (
		e_tree_table_adapter_new (e_tree->priv->model,
		e_tree->priv->sort_info, e_tree->priv->full_header));

	et_connect_to_etta (e_tree);

	gtk_widget_push_colormap (gdk_rgb_get_colormap ());

	e_tree->priv->sorter = e_sorter_new ();

	g_object_set (e_tree->priv->selection,
		      "sorter", e_tree->priv->sorter,
#ifdef E_TREE_USE_TREE_SELECTION
		      "model", e_tree->priv->model,
		      "etta", e_tree->priv->etta,
#else
		      "model", e_tree->priv->etta,
#endif
		      "selection_mode", specification->selection_mode,
		      "cursor_mode", specification->cursor_mode,
		      NULL);

	g_signal_connect(e_tree->priv->selection, "selection_changed",
			 G_CALLBACK (et_selection_model_selection_changed), e_tree);
	g_signal_connect(e_tree->priv->selection, "selection_row_changed",
			 G_CALLBACK (et_selection_model_selection_row_changed), e_tree);

	if (!specification->no_headers) {
		e_tree_setup_header (e_tree);
	}
	e_tree_setup_table (e_tree);

	layout = GTK_LAYOUT (e_tree->priv->table_canvas);

	adjustment = gtk_layout_get_vadjustment (layout);
	gtk_adjustment_set_step_increment (adjustment, 20);

	adjustment = gtk_layout_get_hadjustment (layout);
	gtk_adjustment_set_step_increment (adjustment, 20);

	if (!specification->no_headers) {
		/*
		 * The header
		 */
		gtk_table_attach (GTK_TABLE (e_tree), GTK_WIDGET (e_tree->priv->header_canvas),
				  0, 1, 0 + row, 1 + row,
				  GTK_FILL | GTK_EXPAND,
				  GTK_FILL, 0, 0);
		row++;
	}
	gtk_table_attach (GTK_TABLE (e_tree), GTK_WIDGET (e_tree->priv->table_canvas),
			  0, 1, 0 + row, 1 + row,
			  GTK_FILL | GTK_EXPAND,
			  GTK_FILL | GTK_EXPAND,
			  0, 0);

	gtk_widget_pop_colormap ();

	g_object_unref (ete);

	return e_tree != NULL;
}

/**
 * e_tree_construct:
 * @e_tree: The newly created #ETree object.
 * @etm: The model for this table.
 * @ete: An optional #ETableExtras.  (%NULL is valid.)
 * @spec_str: The spec.
 * @state_str: An optional state.  (%NULL is valid.)
 *
 * This is the internal implementation of e_tree_new() for use by
 * subclasses or language bindings.  See e_tree_new() for details.
 *
 * Return value: %TRUE on success, %FALSE if an error occurred
 **/
gboolean
e_tree_construct (ETree *e_tree, ETreeModel *etm, ETableExtras *ete,
		   const gchar *spec_str, const gchar *state_str)
{
	ETableSpecification *specification;
	ETableState *state;

	g_return_val_if_fail (e_tree != NULL, FALSE);
	g_return_val_if_fail (E_IS_TREE (e_tree), FALSE);
	g_return_val_if_fail (etm != NULL, FALSE);
	g_return_val_if_fail (E_IS_TREE_MODEL (etm), FALSE);
	g_return_val_if_fail (ete == NULL || E_IS_TABLE_EXTRAS (ete), FALSE);
	g_return_val_if_fail (spec_str != NULL, FALSE);

	specification = e_table_specification_new ();
	if (!e_table_specification_load_from_string (specification, spec_str)) {
		g_object_unref (specification);
		return FALSE;
	}
	if (state_str) {
		state = e_table_state_new ();
		e_table_state_load_from_string (state, state_str);
		if (state->col_count <= 0) {
			g_object_unref (state);
			state = specification->state;
			g_object_ref (state);
		}
	} else {
		state = specification->state;
		g_object_ref (state);
	}

	if (!et_real_construct (e_tree, etm, ete, specification, state)) {
		g_object_unref (specification);
		g_object_unref (state);
		return FALSE;
	}

	e_tree->priv->spec = specification;

	g_object_unref (state);

	return TRUE;
}

/**
 * e_tree_construct_from_spec_file:
 * @e_tree: The newly created #ETree object.
 * @etm: The model for this tree
 * @ete: An optional #ETableExtras  (%NULL is valid.)
 * @spec_fn: The filename of the spec
 * @state_fn: An optional state file  (%NULL is valid.)
 *
 * This is the internal implementation of e_tree_new_from_spec_file()
 * for use by subclasses or language bindings.  See
 * e_tree_new_from_spec_file() for details.
 *
 * Return value: %TRUE on success, %FALSE if an error occurred
 **/
gboolean
e_tree_construct_from_spec_file (ETree *e_tree, ETreeModel *etm, ETableExtras *ete,
				  const gchar *spec_fn, const gchar *state_fn)
{
	ETableSpecification *specification;
	ETableState *state;

	g_return_val_if_fail (e_tree != NULL, FALSE);
	g_return_val_if_fail (E_IS_TREE (e_tree), FALSE);
	g_return_val_if_fail (etm != NULL, FALSE);
	g_return_val_if_fail (E_IS_TREE_MODEL (etm), FALSE);
	g_return_val_if_fail (ete == NULL || E_IS_TABLE_EXTRAS (ete), FALSE);
	g_return_val_if_fail (spec_fn != NULL, FALSE);

	specification = e_table_specification_new ();
	if (!e_table_specification_load_from_file (specification, spec_fn)) {
		g_object_unref (specification);
		return FALSE;
	}
	if (state_fn) {
		state = e_table_state_new ();
		if (!e_table_state_load_from_file (state, state_fn)) {
			g_object_unref (state);
			state = specification->state;
			g_object_ref (state);
		}
		if (state->col_count <= 0) {
			g_object_unref (state);
			state = specification->state;
			g_object_ref (state);
		}
	} else {
		state = specification->state;
		g_object_ref (state);
	}

	if (!et_real_construct (e_tree, etm, ete, specification, state)) {
		g_object_unref (specification);
		g_object_unref (state);
		return FALSE;
	}

	e_tree->priv->spec = specification;

	g_object_unref (state);

	return TRUE;
}

/**
 * e_tree_new:
 * @etm: The model for this tree
 * @ete: An optional #ETableExtras  (%NULL is valid.)
 * @spec: The spec
 * @state: An optional state  (%NULL is valid.)
 *
 * This function creates an #ETree from the given parameters.  The
 * #ETreeModel is a tree model to be represented.  The #ETableExtras
 * is an optional set of pixbufs, cells, and sorting functions to be
 * used when interpreting the spec.  If you pass in %NULL it uses the
 * default #ETableExtras.  (See e_table_extras_new()).
 *
 * @spec is the specification of the set of viewable columns and the
 * default sorting state and such.  @state is an optional string
 * specifying the current sorting state and such.  If @state is NULL,
 * then the default state from the spec will be used.
 *
 * Return value:
 * The newly created #ETree or %NULL if there's an error.
 **/
GtkWidget *
e_tree_new (ETreeModel *etm, ETableExtras *ete, const gchar *spec, const gchar *state)
{
	ETree *e_tree;

	g_return_val_if_fail (etm != NULL, NULL);
	g_return_val_if_fail (E_IS_TREE_MODEL (etm), NULL);
	g_return_val_if_fail (ete == NULL || E_IS_TABLE_EXTRAS (ete), NULL);
	g_return_val_if_fail (spec != NULL, NULL);

	e_tree = g_object_new (E_TREE_TYPE, NULL);

	if (!e_tree_construct (e_tree, etm, ete, spec, state)) {
		g_object_unref (e_tree);
		return NULL;
	}

	return (GtkWidget *) e_tree;
}

/**
 * e_tree_new_from_spec_file:
 * @etm: The model for this tree.
 * @ete: An optional #ETableExtras.  (%NULL is valid.)
 * @spec_fn: The filename of the spec.
 * @state_fn: An optional state file.  (%NULL is valid.)
 *
 * This is very similar to e_tree_new(), except instead of passing in
 * strings you pass in the file names of the spec and state to load.
 *
 * @spec_fn is the filename of the spec to load.  If this file doesn't
 * exist, e_tree_new_from_spec_file will return %NULL.
 *
 * @state_fn is the filename of the initial state to load.  If this is
 * %NULL or if the specified file doesn't exist, the default state
 * from the spec file is used.
 *
 * Return value:
 * The newly created #ETree or %NULL if there's an error.
 **/
GtkWidget *
e_tree_new_from_spec_file (ETreeModel *etm,
                           ETableExtras *ete,
                           const gchar *spec_fn,
                           const gchar *state_fn)
{
	ETree *e_tree;

	g_return_val_if_fail (etm != NULL, NULL);
	g_return_val_if_fail (E_IS_TREE_MODEL (etm), NULL);
	g_return_val_if_fail (ete == NULL || E_IS_TABLE_EXTRAS (ete), NULL);
	g_return_val_if_fail (spec_fn != NULL, NULL);

	e_tree = g_object_new (E_TREE_TYPE, NULL);

	if (!e_tree_construct_from_spec_file (e_tree, etm, ete, spec_fn, state_fn)) {
		g_object_unref (e_tree);
		return NULL;
	}

	return (GtkWidget *) e_tree;
}

void
e_tree_show_cursor_after_reflow (ETree *e_tree)
{
	g_return_if_fail (e_tree != NULL);
	g_return_if_fail (E_IS_TREE (e_tree));

	e_tree->priv->show_cursor_after_reflow = TRUE;
}

void
e_tree_set_cursor (ETree *e_tree, ETreePath path)
{
#ifndef E_TREE_USE_TREE_SELECTION
	gint row;
#endif
	g_return_if_fail (e_tree != NULL);
	g_return_if_fail (E_IS_TREE (e_tree));
	g_return_if_fail (path != NULL);

#ifdef E_TREE_USE_TREE_SELECTION
	e_tree_selection_model_select_single_path (
		E_TREE_SELECTION_MODEL (e_tree->priv->selection), path);
	e_tree_selection_model_change_cursor (
		E_TREE_SELECTION_MODEL (e_tree->priv->selection), path);
#else
	row = e_tree_table_adapter_row_of_node (
		E_TREE_TABLE_ADAPTER (e_tree->priv->etta), path);

	if (row == -1)
		return;

	g_object_set (e_tree->priv->selection,
		     "cursor_row", row,
		     NULL);
#endif
}

ETreePath
e_tree_get_cursor (ETree *e_tree)
{
#ifdef E_TREE_USE_TREE_SELECTION
	return e_tree_selection_model_get_cursor (
		E_TREE_SELECTION_MODEL (e_tree->priv->selection));
#else
	gint row;
	g_return_val_if_fail (e_tree != NULL, NULL);
	g_return_val_if_fail (E_IS_TREE (e_tree), NULL);

	g_object_get (e_tree->priv->selection,
		     "cursor_row", &row,
		     NULL);
	if (row == -1)
		return NULL;

	return e_tree_table_adapter_node_at_row (
		E_TREE_TABLE_ADAPTER (e_tree->priv->etta), row);
#endif
}

void
e_tree_selected_row_foreach     (ETree *e_tree,
				  EForeachFunc callback,
				  gpointer closure)
{
	g_return_if_fail (e_tree != NULL);
	g_return_if_fail (E_IS_TREE (e_tree));

	e_selection_model_foreach (e_tree->priv->selection,
				  callback,
				  closure);
}

#ifdef E_TREE_USE_TREE_SELECTION
void
e_tree_selected_path_foreach     (ETree *e_tree,
				 ETreeForeachFunc callback,
				 gpointer closure)
{
	g_return_if_fail (e_tree != NULL);
	g_return_if_fail (E_IS_TREE (e_tree));

	e_tree_selection_model_foreach (
		E_TREE_SELECTION_MODEL (e_tree->priv->selection),
		callback, closure);
}

/* Standard functions */
static void
et_foreach_recurse (ETreeModel *model,
		    ETreePath path,
		    ETreeForeachFunc callback,
		    gpointer closure)
{
	ETreePath child;

	callback (path, closure);

	child = e_tree_model_node_get_first_child (E_TREE_MODEL (model), path);
	for (; child; child = e_tree_model_node_get_next (E_TREE_MODEL (model), child))
		if (child)
			et_foreach_recurse (model, child, callback, closure);
}

void
e_tree_path_foreach (ETree *e_tree,
		     ETreeForeachFunc callback,
		     gpointer closure)
{
	ETreePath root;

	g_return_if_fail (e_tree != NULL);
	g_return_if_fail (E_IS_TREE (e_tree));

	root = e_tree_model_get_root (e_tree->priv->model);

	if (root)
		et_foreach_recurse (e_tree->priv->model,
				    root,
				    callback,
				    closure);
}
#endif

EPrintable *
e_tree_get_printable (ETree *e_tree)
{
	g_return_val_if_fail (e_tree != NULL, NULL);
	g_return_val_if_fail (E_IS_TREE (e_tree), NULL);

	return e_table_item_get_printable (E_TABLE_ITEM (e_tree->priv->item));
}

static void
et_get_property (GObject *object,
		 guint prop_id,
		 GValue *value,
		 GParamSpec *pspec)
{
	ETree *etree = E_TREE (object);

	switch (prop_id) {
	case PROP_ETTA:
		g_value_set_object (value, etree->priv->etta);
		break;
	case PROP_UNIFORM_ROW_HEIGHT:
		g_value_set_boolean (value, etree->priv->uniform_row_height);
		break;
	case PROP_ALWAYS_SEARCH:
		g_value_set_boolean (value, etree->priv->always_search);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

typedef struct {
	gchar     *arg;
	gboolean  setting;
} bool_closure;

static void
et_set_property (GObject *object,
		 guint prop_id,
		 const GValue *value,
		 GParamSpec *pspec)
{
	ETree *etree = E_TREE (object);

	switch (prop_id) {
	case PROP_LENGTH_THRESHOLD:
		etree->priv->length_threshold = g_value_get_int (value);
		if (etree->priv->item) {
			gnome_canvas_item_set (GNOME_CANVAS_ITEM (etree->priv->item),
					       "length_threshold", etree->priv->length_threshold,
					       NULL);
		}
		break;

	case PROP_HORIZONTAL_DRAW_GRID:
		etree->priv->horizontal_draw_grid = g_value_get_boolean (value);
		if (etree->priv->item) {
			gnome_canvas_item_set (GNOME_CANVAS_ITEM (etree->priv->item),
					       "horizontal_draw_grid", etree->priv->horizontal_draw_grid,
					       NULL);
		}
		break;

	case PROP_VERTICAL_DRAW_GRID:
		etree->priv->vertical_draw_grid = g_value_get_boolean (value);
		if (etree->priv->item) {
			gnome_canvas_item_set (GNOME_CANVAS_ITEM (etree->priv->item),
					       "vertical_draw_grid", etree->priv->vertical_draw_grid,
					       NULL);
		}
		break;

	case PROP_DRAW_FOCUS:
		etree->priv->draw_focus = g_value_get_boolean (value);
		if (etree->priv->item) {
			gnome_canvas_item_set (GNOME_CANVAS_ITEM (etree->priv->item),
					       "drawfocus", etree->priv->draw_focus,
					       NULL);
		}
		break;

	case PROP_UNIFORM_ROW_HEIGHT:
		etree->priv->uniform_row_height = g_value_get_boolean (value);
		if (etree->priv->item) {
			gnome_canvas_item_set (GNOME_CANVAS_ITEM (etree->priv->item),
					       "uniform_row_height", etree->priv->uniform_row_height,
					       NULL);
		}
		break;

	case PROP_ALWAYS_SEARCH:
		if (etree->priv->always_search == g_value_get_boolean (value))
			return;
		etree->priv->always_search = g_value_get_boolean (value);
		clear_current_search_col (etree);
		break;
	}
}

static void
set_scroll_adjustments   (ETree *tree,
			  GtkAdjustment *hadjustment,
			  GtkAdjustment *vadjustment)
{
	if (vadjustment != NULL)
		gtk_adjustment_set_step_increment (vadjustment, 20);

	if (hadjustment != NULL)
		gtk_adjustment_set_step_increment (hadjustment, 20);

	if (tree->priv) {
		GtkLayout *layout;

		layout = GTK_LAYOUT (tree->priv->table_canvas);

		gtk_layout_set_hadjustment (layout, hadjustment);
		gtk_layout_set_vadjustment (layout, vadjustment);

		if (tree->priv->header_canvas != NULL)
			gtk_layout_set_hadjustment (
				GTK_LAYOUT (tree->priv->header_canvas),
				hadjustment);
	}
}

gint
e_tree_get_next_row      (ETree *e_tree,
			   gint    model_row)
{
	g_return_val_if_fail (e_tree != NULL, -1);
	g_return_val_if_fail (E_IS_TREE (e_tree), -1);

	if (e_tree->priv->sorter) {
		gint i;
		i = e_sorter_model_to_sorted (E_SORTER (e_tree->priv->sorter), model_row);
		i++;
		if (i < e_table_model_row_count (E_TABLE_MODEL (e_tree->priv->etta))) {
			return e_sorter_sorted_to_model (E_SORTER (e_tree->priv->sorter), i);
		} else
			return -1;
	} else
		if (model_row < e_table_model_row_count (E_TABLE_MODEL (e_tree->priv->etta)) - 1)
			return model_row + 1;
		else
			return -1;
}

gint
e_tree_get_prev_row      (ETree *e_tree,
			  gint    model_row)
{
	g_return_val_if_fail (e_tree != NULL, -1);
	g_return_val_if_fail (E_IS_TREE (e_tree), -1);

	if (e_tree->priv->sorter) {
		gint i;
		i = e_sorter_model_to_sorted (E_SORTER (e_tree->priv->sorter), model_row);
		i--;
		if (i >= 0)
			return e_sorter_sorted_to_model (E_SORTER (e_tree->priv->sorter), i);
		else
			return -1;
	} else
		return model_row - 1;
}

gint
e_tree_model_to_view_row        (ETree *e_tree,
				  gint    model_row)
{
	g_return_val_if_fail (e_tree != NULL, -1);
	g_return_val_if_fail (E_IS_TREE (e_tree), -1);

	if (e_tree->priv->sorter)
		return e_sorter_model_to_sorted (E_SORTER (e_tree->priv->sorter), model_row);
	else
		return model_row;
}

gint
e_tree_view_to_model_row        (ETree *e_tree,
				  gint    view_row)
{
	g_return_val_if_fail (e_tree != NULL, -1);
	g_return_val_if_fail (E_IS_TREE (e_tree), -1);

	if (e_tree->priv->sorter)
		return e_sorter_sorted_to_model (E_SORTER (e_tree->priv->sorter), view_row);
	else
		return view_row;
}

gboolean
e_tree_node_is_expanded (ETree *et, ETreePath path)
{
	g_return_val_if_fail (path, FALSE);

	return e_tree_table_adapter_node_is_expanded (et->priv->etta, path);
}

void
e_tree_node_set_expanded (ETree *et, ETreePath path, gboolean expanded)
{
	g_return_if_fail (et != NULL);
	g_return_if_fail (E_IS_TREE (et));

	e_tree_table_adapter_node_set_expanded (et->priv->etta, path, expanded);
}

void
e_tree_node_set_expanded_recurse (ETree *et, ETreePath path, gboolean expanded)
{
	g_return_if_fail (et != NULL);
	g_return_if_fail (E_IS_TREE (et));

	e_tree_table_adapter_node_set_expanded_recurse (et->priv->etta, path, expanded);
}

void
e_tree_root_node_set_visible (ETree *et, gboolean visible)
{
	g_return_if_fail (et != NULL);
	g_return_if_fail (E_IS_TREE (et));

	e_tree_table_adapter_root_node_set_visible (et->priv->etta, visible);
}

ETreePath
e_tree_node_at_row (ETree *et, gint row)
{
	ETreePath path;

	path = e_tree_table_adapter_node_at_row (et->priv->etta, row);

	return path;
}

gint
e_tree_row_of_node (ETree *et, ETreePath path)
{
	return e_tree_table_adapter_row_of_node (et->priv->etta, path);
}

gboolean
e_tree_root_node_is_visible (ETree *et)
{
	return e_tree_table_adapter_root_node_is_visible (et->priv->etta);
}

void
e_tree_show_node (ETree *et, ETreePath path)
{
	g_return_if_fail (et != NULL);
	g_return_if_fail (E_IS_TREE (et));

	e_tree_table_adapter_show_node (et->priv->etta, path);
}

void
e_tree_save_expanded_state (ETree *et, gchar *filename)
{
	g_return_if_fail (et != NULL);
	g_return_if_fail (E_IS_TREE (et));

	e_tree_table_adapter_save_expanded_state (et->priv->etta, filename);
}

void
e_tree_load_expanded_state (ETree *et, gchar *filename)
{
	e_tree_table_adapter_load_expanded_state (et->priv->etta, filename);
}

xmlDoc *
e_tree_save_expanded_state_xml (ETree *et)
{
	g_return_val_if_fail (et != NULL, NULL);
	g_return_val_if_fail (E_IS_TREE (et), NULL);

	return e_tree_table_adapter_save_expanded_state_xml (et->priv->etta);
}

void
e_tree_load_expanded_state_xml (ETree *et, xmlDoc *doc)
{
	g_return_if_fail (et != NULL);
	g_return_if_fail (E_IS_TREE (et));
	g_return_if_fail (doc != NULL);

	e_tree_table_adapter_load_expanded_state_xml (et->priv->etta, doc);
}

/* state: <0 ... collapse; 0 ... no force - use default; >0 ... expand;
   when using this, be sure to reset to 0 once no forcing is required
   anymore, aka the build of the tree is done */
void
e_tree_force_expanded_state (ETree *et, gint state)
{
	e_tree_table_adapter_force_expanded_state (et->priv->etta, state);
}

gint
e_tree_row_count (ETree *et)
{
	return e_table_model_row_count (E_TABLE_MODEL (et->priv->etta));
}

GtkWidget *
e_tree_get_tooltip (ETree *et)
{
	return E_CANVAS (et->priv->table_canvas)->tooltip_window;
}

static ETreePath
find_next_in_range (ETree *et, gint start, gint end, ETreePathFunc func, gpointer data)
{
	ETreePath path;
	gint row;

	for (row = start; row <= end; row++) {
		path = e_tree_table_adapter_node_at_row (et->priv->etta, row);
		if (path && func (et->priv->model, path, data))
			return path;
	}

	return NULL;
}

static ETreePath
find_prev_in_range (ETree *et, gint start, gint end, ETreePathFunc func, gpointer data)
{
	ETreePath path;
	gint row;

	for (row = start; row >= end; row--) {
		path = e_tree_table_adapter_node_at_row (et->priv->etta, row);
		if (path && func (et->priv->model, path, data))
			return path;
	}

	return NULL;
}

gboolean
e_tree_find_next (ETree *et,
                  ETreeFindNextParams params,
                  ETreePathFunc func,
                  gpointer data)
{
	ETreePath cursor, found;
	gint row, row_count;

	cursor = e_tree_get_cursor (et);
	row = e_tree_table_adapter_row_of_node (et->priv->etta, cursor);
	row_count = e_table_model_row_count (E_TABLE_MODEL (et->priv->etta));

	if (params & E_TREE_FIND_NEXT_FORWARD)
		found = find_next_in_range (et, row + 1, row_count - 1, func, data);
	else
		found = find_prev_in_range (et, row == -1 ? -1 : row - 1, 0, func, data);

	if (found) {
		e_tree_table_adapter_show_node (et->priv->etta, found);
		e_tree_set_cursor (et, found);
		return TRUE;
	}

	if (params & E_TREE_FIND_NEXT_WRAP) {
		if (params & E_TREE_FIND_NEXT_FORWARD)
			found = find_next_in_range (et, 0, row, func, data);
		else
			found = find_prev_in_range (et, row_count - 1, row, func, data);

		if (found && found != cursor) {
			e_tree_table_adapter_show_node (et->priv->etta, found);
			e_tree_set_cursor (et, found);
			return TRUE;
		}
	}

	return FALSE;
}

void
e_tree_right_click_up (ETree *et)
{
	e_selection_model_right_click_up (et->priv->selection);
}

/**
 * e_tree_get_model:
 * @et: the ETree
 *
 * Returns the model upon which this ETree is based.
 *
 * Returns: the model
 **/
ETreeModel *
e_tree_get_model (ETree *et)
{
	g_return_val_if_fail (et != NULL, NULL);
	g_return_val_if_fail (E_IS_TREE (et), NULL);

	return et->priv->model;
}

/**
 * e_tree_get_selection_model:
 * @et: the ETree
 *
 * Returns the selection model of this ETree.
 *
 * Returns: the selection model
 **/
ESelectionModel *
e_tree_get_selection_model (ETree *et)
{
	g_return_val_if_fail (et != NULL, NULL);
	g_return_val_if_fail (E_IS_TREE (et), NULL);

	return et->priv->selection;
}

/**
 * e_tree_get_table_adapter:
 * @et: the ETree
 *
 * Returns the table adapter this ETree uses.
 *
 * Returns: the model
 **/
ETreeTableAdapter *
e_tree_get_table_adapter (ETree *et)
{
	g_return_val_if_fail (et != NULL, NULL);
	g_return_val_if_fail (E_IS_TREE (et), NULL);

	return et->priv->etta;
}

ETableItem *
e_tree_get_item (ETree * et)
{
	g_return_val_if_fail (et != NULL, NULL);
	g_return_val_if_fail (E_IS_TREE (et), NULL);

	return E_TABLE_ITEM (et->priv->item);
}

GnomeCanvasItem *
e_tree_get_header_item (ETree * et)
{
	g_return_val_if_fail (et != NULL, NULL);
	g_return_val_if_fail (E_IS_TREE (et), NULL);

	return et->priv->header_item;
}

struct _ETreeDragSourceSite
{
	GdkModifierType    start_button_mask;
	GtkTargetList     *target_list;        /* Targets for drag data */
	GdkDragAction      actions;            /* Possible actions */
	GdkColormap       *colormap;		 /* Colormap for drag icon */
	GdkPixmap         *pixmap;             /* Icon for drag data */
	GdkBitmap         *mask;

	/* Stored button press information to detect drag beginning */
	gint               state;
	gint               x, y;
	gint               row, col;
};

typedef enum
{
  GTK_DRAG_STATUS_DRAG,
  GTK_DRAG_STATUS_WAIT,
  GTK_DRAG_STATUS_DROP
} GtkDragStatus;

typedef struct _GtkDragDestInfo GtkDragDestInfo;
typedef struct _GtkDragSourceInfo GtkDragSourceInfo;

struct _GtkDragDestInfo
{
  GtkWidget         *widget;	   /* Widget in which drag is in */
  GdkDragContext    *context;	   /* Drag context */
  GtkDragSourceInfo *proxy_source; /* Set if this is a proxy drag */
  GtkSelectionData  *proxy_data;   /* Set while retrieving proxied data */
  guint              dropped : 1;     /* Set after we receive a drop */
  guint32            proxy_drop_time; /* Timestamp for proxied drop */
  guint              proxy_drop_wait : 1; /* Set if we are waiting for a
					   * status reply before sending
					   * a proxied drop on.
					   */
  gint               drop_x, drop_y; /* Position of drop */
};

struct _GtkDragSourceInfo
{
  GtkWidget         *widget;
  GtkTargetList     *target_list; /* Targets for drag data */
  GdkDragAction      possible_actions; /* Actions allowed by source */
  GdkDragContext    *context;	  /* drag context */
  GtkWidget         *icon_window; /* Window for drag */
  GtkWidget         *ipc_widget;  /* GtkInvisible for grab, message passing */
  GdkCursor         *cursor;	  /* Cursor for drag */
  gint hot_x, hot_y;		  /* Hot spot for drag */
  gint button;			  /* mouse button starting drag */

  GtkDragStatus      status;	  /* drag status */
  GdkEvent          *last_event;  /* motion event waiting for response */

  gint               start_x, start_y; /* Initial position */
  gint               cur_x, cur_y;     /* Current Position */

  GList             *selections;  /* selections we've claimed */

  GtkDragDestInfo   *proxy_dest;  /* Set if this is a proxy drag */

  guint              drop_timeout;     /* Timeout for aborting drop */
  guint              destroy_icon : 1; /* If true, destroy icon_window
					*/
};

/* Drag & drop stuff. */
/* Target */

void
e_tree_drag_get_data (ETree         *tree,
		      gint             row,
		      gint             col,
		      GdkDragContext *context,
		      GdkAtom         target,
		      guint32         time)
{
	g_return_if_fail (tree != NULL);
	g_return_if_fail (E_IS_TREE (tree));

	gtk_drag_get_data (GTK_WIDGET (tree),
			  context,
			  target,
			  time);

}

/**
 * e_tree_drag_highlight:
 * @tree:
 * @row:
 * @col:
 *
 * Set col to -1 to highlight the entire row.
 * Set row to -1 to turn off the highlight.
 */
void
e_tree_drag_highlight (ETree *tree,
		       gint     row,
		       gint     col)
{
	GtkAdjustment *adjustment;
	GtkAllocation allocation;
	GtkLayout *layout;
	GtkStyle *style;

	g_return_if_fail (E_IS_TREE (tree));

	layout = GTK_LAYOUT (tree->priv->table_canvas);
	style = gtk_widget_get_style (GTK_WIDGET (tree));
	gtk_widget_get_allocation (GTK_WIDGET (layout), &allocation);

	if (row != -1) {
		gint x, y, width, height;
		if (col == -1) {
			e_tree_get_cell_geometry (tree, row, 0, &x, &y, &width, &height);
			x = 0;
			width = allocation.width;
		} else {
			e_tree_get_cell_geometry (tree, row, col, &x, &y, &width, &height);
			adjustment = gtk_layout_get_hadjustment (layout);
			x += gtk_adjustment_get_value (adjustment);
		}

		adjustment = gtk_layout_get_vadjustment (layout);
		y += gtk_adjustment_get_value (adjustment);

		if (tree->priv->drop_highlight == NULL) {
			tree->priv->drop_highlight = gnome_canvas_item_new (
				gnome_canvas_root (tree->priv->table_canvas),
				gnome_canvas_rect_get_type (),
				"fill_color", NULL,
				"outline_color_gdk", &style->fg[GTK_STATE_NORMAL],
				NULL);
		}

		gnome_canvas_item_set (
			tree->priv->drop_highlight,
			"x1", (gdouble) x,
			"x2", (gdouble) x + width - 1,
			"y1", (gdouble) y,
			"y2", (gdouble) y + height - 1,
			NULL);
	} else {
		gtk_object_destroy (GTK_OBJECT (tree->priv->drop_highlight));
		tree->priv->drop_highlight = NULL;
	}
}

void
e_tree_drag_unhighlight (ETree *tree)
{
	g_return_if_fail (tree != NULL);
	g_return_if_fail (E_IS_TREE (tree));

	if (tree->priv->drop_highlight) {
		gtk_object_destroy (GTK_OBJECT (tree->priv->drop_highlight));
		tree->priv->drop_highlight = NULL;
	}
}

void e_tree_drag_dest_set   (ETree               *tree,
			     GtkDestDefaults       flags,
			     const GtkTargetEntry *targets,
			     gint                  n_targets,
			     GdkDragAction         actions)
{
	g_return_if_fail (tree != NULL);
	g_return_if_fail (E_IS_TREE (tree));

	gtk_drag_dest_set (GTK_WIDGET (tree),
			  flags,
			  targets,
			  n_targets,
			  actions);
}

void e_tree_drag_dest_set_proxy (ETree         *tree,
				 GdkWindow      *proxy_window,
				 GdkDragProtocol protocol,
				 gboolean        use_coordinates)
{
	g_return_if_fail (tree != NULL);
	g_return_if_fail (E_IS_TREE (tree));

	gtk_drag_dest_set_proxy (GTK_WIDGET (tree),
				proxy_window,
				protocol,
				use_coordinates);
}

/*
 * There probably should be functions for setting the targets
 * as a GtkTargetList
 */

void
e_tree_drag_dest_unset (GtkWidget *widget)
{
	g_return_if_fail (widget != NULL);
	g_return_if_fail (E_IS_TREE (widget));

	gtk_drag_dest_unset (widget);
}

/* Source side */

static gint
et_real_start_drag (ETree *tree, gint row, ETreePath path, gint col, GdkEvent *event)
{
	GtkDragSourceInfo *info;
	GdkDragContext *context;
	ETreeDragSourceSite *site;

	if (tree->priv->do_drag) {
		site = tree->priv->site;

		site->state = 0;
		context = e_tree_drag_begin (tree, row, col,
					     site->target_list,
					     site->actions,
					     1, event);

		if (context) {
			info = g_dataset_get_data (context, "gtk-info");

			if (info && !info->icon_window) {
				if (site->pixmap)
					gtk_drag_set_icon_pixmap (context,
								  site->colormap,
								  site->pixmap,
								  site->mask, -2, -2);
				else
					gtk_drag_set_icon_default (context);
			}
		}
		return TRUE;
	}
	return FALSE;
}

void
e_tree_drag_source_set  (ETree               *tree,
			 GdkModifierType       start_button_mask,
			 const GtkTargetEntry *targets,
			 gint                  n_targets,
			 GdkDragAction         actions)
{
	ETreeDragSourceSite *site;
	GtkWidget *canvas;

	g_return_if_fail (tree != NULL);
	g_return_if_fail (E_IS_TREE (tree));

	canvas = GTK_WIDGET (tree->priv->table_canvas);
	site = tree->priv->site;

	tree->priv->do_drag = TRUE;

	gtk_widget_add_events (canvas,
			       gtk_widget_get_events (canvas) |
			       GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
			       GDK_BUTTON_MOTION_MASK | GDK_STRUCTURE_MASK);

	if (site) {
		if (site->target_list)
			gtk_target_list_unref (site->target_list);
	} else {
		site = g_new0 (ETreeDragSourceSite, 1);
		tree->priv->site = site;
	}

	site->start_button_mask = start_button_mask;

	if (targets)
		site->target_list = gtk_target_list_new (targets, n_targets);
	else
		site->target_list = NULL;

	site->actions = actions;
}

void
e_tree_drag_source_unset (ETree *tree)
{
	ETreeDragSourceSite *site;

	g_return_if_fail (tree != NULL);
	g_return_if_fail (E_IS_TREE (tree));

	site = tree->priv->site;

	if (site) {
		if (site->target_list)
			gtk_target_list_unref (site->target_list);
		g_free (site);
		tree->priv->site = NULL;
	}
}

/* There probably should be functions for setting the targets
 * as a GtkTargetList
 */

GdkDragContext *
e_tree_drag_begin (ETree            *tree,
		   gint	       row,
		   gint	       col,
		   GtkTargetList     *targets,
		   GdkDragAction      actions,
		   gint               button,
		   GdkEvent          *event)
{
	ETreePath path;
	g_return_val_if_fail (tree != NULL, NULL);
	g_return_val_if_fail (E_IS_TREE (tree), NULL);

	path = e_tree_table_adapter_node_at_row (tree->priv->etta, row);

	tree->priv->drag_row = row;
	tree->priv->drag_path = path;
	tree->priv->drag_col = col;

	return gtk_drag_begin (GTK_WIDGET (tree->priv->table_canvas),
			      targets,
			      actions,
			      button,
			      event);
}

/**
 * e_tree_get_cell_at:
 * @tree: An ETree widget
 * @x: X coordinate for the pixel
 * @y: Y coordinate for the pixel
 * @row_return: Pointer to return the row value
 * @col_return: Pointer to return the column value
 *
 * Return the row and column for the cell in which the pixel at (@x, @y) is
 * contained.
 **/
void
e_tree_get_cell_at (ETree *tree,
		     gint x, gint y,
		     gint *row_return, gint *col_return)
{
	GtkAdjustment *adjustment;
	GtkLayout *layout;

	g_return_if_fail (E_IS_TREE (tree));
	g_return_if_fail (row_return != NULL);
	g_return_if_fail (col_return != NULL);

	/* FIXME it would be nice if it could handle a NULL row_return or
	 * col_return gracefully.  */

	if (row_return)
		*row_return = -1;
	if (col_return)
		*col_return = -1;

	layout = GTK_LAYOUT (tree->priv->table_canvas);

	adjustment = gtk_layout_get_hadjustment (layout);
	x += gtk_adjustment_get_value (adjustment);

	adjustment = gtk_layout_get_vadjustment (layout);
	y += gtk_adjustment_get_value (adjustment);

	e_table_item_compute_location (
		E_TABLE_ITEM (tree->priv->item),
		&x, &y, row_return, col_return);
}

/**
 * e_tree_get_cell_geometry:
 * @tree: The tree.
 * @row: The row to get the geometry of.
 * @col: The col to get the geometry of.
 * @x_return: Returns the x coordinate of the upper right hand corner
 * of the cell with respect to the widget.
 * @y_return: Returns the y coordinate of the upper right hand corner
 * of the cell with respect to the widget.
 * @width_return: Returns the width of the cell.
 * @height_return: Returns the height of the cell.
 *
 * Computes the data about this cell.
 **/
void
e_tree_get_cell_geometry (ETree *tree,
			  gint row, gint col,
			  gint *x_return, gint *y_return,
			  gint *width_return, gint *height_return)
{
	GtkAdjustment *adjustment;
	GtkLayout *layout;

	g_return_if_fail (E_IS_TREE (tree));
	g_return_if_fail (row >= 0);
	g_return_if_fail (col >= 0);

	/* FIXME it would be nice if it could handle a NULL row_return or
	 * col_return gracefully.  */

	e_table_item_get_cell_geometry (
		E_TABLE_ITEM (tree->priv->item),
		&row, &col, x_return, y_return,
		width_return, height_return);

	layout = GTK_LAYOUT (tree->priv->table_canvas);

	if (x_return) {
		adjustment = gtk_layout_get_hadjustment (layout);
		(*x_return) -= gtk_adjustment_get_value (adjustment);
	}

	if (y_return) {
		adjustment = gtk_layout_get_vadjustment (layout);
		(*y_return) -= gtk_adjustment_get_value (adjustment);
	}
}

static void
et_drag_begin (GtkWidget *widget,
	       GdkDragContext *context,
	       ETree *et)
{
	g_signal_emit (et,
		       et_signals[TREE_DRAG_BEGIN], 0,
		       et->priv->drag_row,
		       et->priv->drag_path,
		       et->priv->drag_col,
		       context);
}

static void
et_drag_end (GtkWidget *widget,
	     GdkDragContext *context,
	     ETree *et)
{
	g_signal_emit (et,
		       et_signals[TREE_DRAG_END], 0,
		       et->priv->drag_row,
		       et->priv->drag_path,
		       et->priv->drag_col,
		       context);
}

static void
et_drag_data_get (GtkWidget *widget,
		 GdkDragContext *context,
		 GtkSelectionData *selection_data,
		 guint info,
		 guint time,
		 ETree *et)
{
	g_signal_emit (et,
		       et_signals[TREE_DRAG_DATA_GET], 0,
		       et->priv->drag_row,
		       et->priv->drag_path,
		       et->priv->drag_col,
		       context,
		       selection_data,
		       info,
		       time);
}

static void
et_drag_data_delete (GtkWidget *widget,
		    GdkDragContext *context,
		    ETree *et)
{
	g_signal_emit (et,
		       et_signals[TREE_DRAG_DATA_DELETE], 0,
		       et->priv->drag_row,
		       et->priv->drag_path,
		       et->priv->drag_col,
		       context);
}

static gboolean
do_drag_motion (ETree *et,
	       GdkDragContext *context,
	       gint x,
	       gint y,
	       guint time)
{
	gboolean ret_val = FALSE;
	gint row, col;
	ETreePath path;

	e_tree_get_cell_at (et,
			    x,
			    y,
			    &row,
			    &col);
	if (row != et->priv->drop_row && col != et->priv->drop_col) {
		g_signal_emit (et,
			       et_signals[TREE_DRAG_LEAVE], 0,
			       et->priv->drop_row,
			       et->priv->drop_path,
			       et->priv->drop_col,
			       context,
			       time);
	}

	path = e_tree_table_adapter_node_at_row (et->priv->etta, row);

	et->priv->drop_row = row;
	et->priv->drop_path = path;
	et->priv->drop_col = col;
	g_signal_emit (et,
		       et_signals[TREE_DRAG_MOTION], 0,
		       et->priv->drop_row,
		       et->priv->drop_path,
		       et->priv->drop_col,
		       context,
		       x,
		       y,
		       time,
		       &ret_val);

	return ret_val;
}

static gboolean
scroll_timeout (gpointer data)
{
	ETree *et = data;
	gint dx = 0, dy = 0;
	GtkAdjustment *adjustment;
	GtkLayout *layout;
	gdouble old_h_value;
	gdouble new_h_value;
	gdouble old_v_value;
	gdouble new_v_value;
	gdouble page_size;
	gdouble lower;
	gdouble upper;

	if (et->priv->scroll_direction & ET_SCROLL_DOWN)
		dy += 20;
	if (et->priv->scroll_direction & ET_SCROLL_UP)
		dy -= 20;

	if (et->priv->scroll_direction & ET_SCROLL_RIGHT)
		dx += 20;
	if (et->priv->scroll_direction & ET_SCROLL_LEFT)
		dx -= 20;

	layout = GTK_LAYOUT (et->priv->table_canvas);

	adjustment = gtk_layout_get_hadjustment (layout);

	page_size = gtk_adjustment_get_page_size (adjustment);
	lower = gtk_adjustment_get_lower (adjustment);
	upper = gtk_adjustment_get_upper (adjustment);

	old_h_value = gtk_adjustment_get_value (adjustment);
	new_h_value = CLAMP (old_h_value + dx, lower, upper - page_size);

	gtk_adjustment_set_value (adjustment, new_h_value);

	adjustment = gtk_layout_get_vadjustment (layout);

	page_size = gtk_adjustment_get_page_size (adjustment);
	lower = gtk_adjustment_get_lower (adjustment);
	upper = gtk_adjustment_get_upper (adjustment);

	old_v_value = gtk_adjustment_get_value (adjustment);
	new_v_value = CLAMP (old_v_value + dy, lower, upper - page_size);

	gtk_adjustment_set_value (adjustment, new_v_value);

	if (new_h_value != old_h_value || new_v_value != old_v_value)
		do_drag_motion (
			et,
			et->priv->last_drop_context,
			et->priv->last_drop_x,
			et->priv->last_drop_y,
			et->priv->last_drop_time);

	return TRUE;
}

static void
scroll_on (ETree *et, guint scroll_direction)
{
	if (et->priv->scroll_idle_id == 0 || scroll_direction != et->priv->scroll_direction) {
		if (et->priv->scroll_idle_id != 0)
			g_source_remove (et->priv->scroll_idle_id);
		et->priv->scroll_direction = scroll_direction;
		et->priv->scroll_idle_id = g_timeout_add (100, scroll_timeout, et);
	}
}

static void
scroll_off (ETree *et)
{
	if (et->priv->scroll_idle_id) {
		g_source_remove (et->priv->scroll_idle_id);
		et->priv->scroll_idle_id = 0;
	}
}

static gboolean
hover_timeout (gpointer data)
{
	ETree *et = data;
	gint x = et->priv->hover_x;
	gint y = et->priv->hover_y;
	gint row, col;
	ETreePath path;

	e_tree_get_cell_at (et,
			    x,
			    y,
			    &row,
			    &col);

	path = e_tree_table_adapter_node_at_row (et->priv->etta, row);
	if (path && e_tree_model_node_is_expandable (et->priv->model, path)) {
		if (!e_tree_table_adapter_node_is_expanded (et->priv->etta, path)) {
			if (e_tree_model_has_save_id (et->priv->model) &&
				e_tree_model_has_get_node_by_id (et->priv->model))
				et->priv->expanded_list = g_list_prepend (
					et->priv->expanded_list,
					e_tree_model_get_save_id (
					et->priv->model, path));

			e_tree_table_adapter_node_set_expanded (
				et->priv->etta, path, TRUE);
		}
	}

	return TRUE;
}

static void
hover_on (ETree *et, gint x, gint y)
{
	et->priv->hover_x = x;
	et->priv->hover_y = y;
	if (et->priv->hover_idle_id != 0)
		g_source_remove (et->priv->hover_idle_id);
	et->priv->hover_idle_id = g_timeout_add (500, hover_timeout, et);
}

static void
hover_off (ETree *et)
{
	if (et->priv->hover_idle_id) {
		g_source_remove (et->priv->hover_idle_id);
		et->priv->hover_idle_id = 0;
	}
}

static void
collapse_drag (ETree *et, ETreePath drop)
{
	GList *list;

	/* We only want to leave open parents of the node dropped in.  Not the node itself. */
	if (drop) {
		drop = e_tree_model_node_get_parent (et->priv->model, drop);
	}

	for (list = et->priv->expanded_list; list; list = list->next) {
		gchar *save_id = list->data;
		ETreePath path;

		path = e_tree_model_get_node_by_id (et->priv->model, save_id);
		if (path) {
			ETreePath search;
			gboolean found = FALSE;

			for (search = drop; search;
				search = e_tree_model_node_get_parent (
				et->priv->model, search)) {
				if (path == search) {
					found = TRUE;
					break;
				}
			}

			if (!found)
				e_tree_table_adapter_node_set_expanded (
					et->priv->etta, path, FALSE);
		}
		g_free (save_id);
	}
	g_list_free (et->priv->expanded_list);
	et->priv->expanded_list = NULL;
}

static void
context_destroyed (gpointer data, GObject *ctx)
{
	ETree *et = data;
	if (et->priv) {
		et->priv->last_drop_x       = 0;
		et->priv->last_drop_y       = 0;
		et->priv->last_drop_time    = 0;
		et->priv->last_drop_context = NULL;
		collapse_drag (et, NULL);
		scroll_off (et);
		hover_off (et);
	}
	g_object_unref (et);
}

static void
context_connect (ETree *et, GdkDragContext *context)
{
	if (context == et->priv->last_drop_context)
		return;

	if (et->priv->last_drop_context)
		g_object_weak_unref (G_OBJECT (et->priv->last_drop_context), context_destroyed, et);
	else
		g_object_ref (et);

	g_object_weak_ref (G_OBJECT (context), context_destroyed, et);
}

static void
et_drag_leave (GtkWidget *widget,
	      GdkDragContext *context,
	      guint time,
	      ETree *et)
{
	g_signal_emit (et,
		       et_signals[TREE_DRAG_LEAVE], 0,
		       et->priv->drop_row,
		       et->priv->drop_path,
		       et->priv->drop_col,
		       context,
		       time);
	et->priv->drop_row = -1;
	et->priv->drop_col = -1;

	scroll_off (et);
	hover_off (et);
}

static gboolean
et_drag_motion (GtkWidget *widget,
	       GdkDragContext *context,
	       gint x,
	       gint y,
	       guint time,
	       ETree *et)
{
	GtkAllocation allocation;
	gint ret_val;
	guint direction = 0;

	et->priv->last_drop_x = x;
	et->priv->last_drop_y = y;
	et->priv->last_drop_time = time;
	context_connect (et, context);
	et->priv->last_drop_context = context;

	if (et->priv->hover_idle_id != 0) {
		if (abs (et->priv->hover_x - x) > 3 ||
		    abs (et->priv->hover_y - y) > 3) {
			hover_on (et, x, y);
		}
	} else {
		hover_on (et, x, y);
	}

	ret_val = do_drag_motion (et, context, x, y, time);

	gtk_widget_get_allocation (widget, &allocation);

	if (y < 20)
		direction |= ET_SCROLL_UP;
	if (y > allocation.height - 20)
		direction |= ET_SCROLL_DOWN;
	if (x < 20)
		direction |= ET_SCROLL_LEFT;
	if (x > allocation.width - 20)
		direction |= ET_SCROLL_RIGHT;

	if (direction != 0)
		scroll_on (et, direction);
	else
		scroll_off (et);

	return ret_val;
}

static gboolean
et_drag_drop (GtkWidget *widget,
	     GdkDragContext *context,
	     gint x,
	     gint y,
	     guint time,
	     ETree *et)
{
	gboolean ret_val = FALSE;
	gint row, col;
	ETreePath path;
	e_tree_get_cell_at (et,
			   x,
			   y,
			   &row,
			   &col);
	path = e_tree_table_adapter_node_at_row (et->priv->etta, row);

	if (row != et->priv->drop_row && col != et->priv->drop_row) {
		g_signal_emit (et,
			       et_signals[TREE_DRAG_LEAVE], 0,
			       et->priv->drop_row,
			       et->priv->drop_path,
			       et->priv->drop_col,
			       context,
			       time);
		g_signal_emit (et,
			       et_signals[TREE_DRAG_MOTION], 0,
			       row,
			       path,
			       col,
			       context,
			       x,
			       y,
			       time,
			       &ret_val);
	}
	et->priv->drop_row = row;
	et->priv->drop_path = path;
	et->priv->drop_col = col;

	g_signal_emit (et,
		       et_signals[TREE_DRAG_DROP], 0,
		       et->priv->drop_row,
		       et->priv->drop_path,
		       et->priv->drop_col,
		       context,
		       x,
		       y,
		       time,
		       &ret_val);

	et->priv->drop_row = -1;
	et->priv->drop_path = NULL;
	et->priv->drop_col = -1;

	collapse_drag (et, path);

	scroll_off (et);
	return ret_val;
}

static void
et_drag_data_received (GtkWidget *widget,
		      GdkDragContext *context,
		      gint x,
		      gint y,
		      GtkSelectionData *selection_data,
		      guint info,
		      guint time,
		      ETree *et)
{
	gint row, col;
	ETreePath path;
	e_tree_get_cell_at (et,
			   x,
			   y,
			   &row,
			   &col);
	path = e_tree_table_adapter_node_at_row (et->priv->etta, row);
	g_signal_emit (et,
		       et_signals[TREE_DRAG_DATA_RECEIVED], 0,
		       row,
		       path,
		       col,
		       context,
		       x,
		       y,
		       selection_data,
		       info,
		       time);
}

static void
e_tree_class_init (ETreeClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class                   = (GObjectClass *) class;
	widget_class                   = (GtkWidgetClass *) class;

	object_class->dispose          = et_dispose;
	object_class->set_property     = et_set_property;
	object_class->get_property     = et_get_property;

	widget_class->grab_focus       = et_grab_focus;
	widget_class->unrealize        = et_unrealize;
	widget_class->style_set        = et_canvas_style_set;
	widget_class->focus            = et_focus;

	class->cursor_change           = NULL;
	class->cursor_activated        = NULL;
	class->selection_change        = NULL;
	class->double_click            = NULL;
	class->right_click             = NULL;
	class->click                   = NULL;
	class->key_press               = NULL;
	class->start_drag              = et_real_start_drag;
	class->state_change            = NULL;
	class->white_space_event       = NULL;

	class->tree_drag_begin         = NULL;
	class->tree_drag_end           = NULL;
	class->tree_drag_data_get      = NULL;
	class->tree_drag_data_delete   = NULL;

	class->tree_drag_leave         = NULL;
	class->tree_drag_motion        = NULL;
	class->tree_drag_drop          = NULL;
	class->tree_drag_data_received = NULL;

	et_signals[CURSOR_CHANGE] =
		g_signal_new ("cursor_change",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETreeClass, cursor_change),
			      NULL, NULL,
			      e_marshal_NONE__INT_POINTER,
			      G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_POINTER);

	et_signals[CURSOR_ACTIVATED] =
		g_signal_new ("cursor_activated",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETreeClass, cursor_activated),
			      NULL, NULL,
			      e_marshal_NONE__INT_POINTER,
			      G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_POINTER);

	et_signals[SELECTION_CHANGE] =
		g_signal_new ("selection_change",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETreeClass, selection_change),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	et_signals[DOUBLE_CLICK] =
		g_signal_new ("double_click",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETreeClass, double_click),
			      NULL, NULL,
			      e_marshal_NONE__INT_POINTER_INT_BOXED,
			      G_TYPE_NONE, 4, G_TYPE_INT,
			      G_TYPE_POINTER, G_TYPE_INT, GDK_TYPE_EVENT);

	et_signals[RIGHT_CLICK] =
		g_signal_new ("right_click",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETreeClass, right_click),
			      g_signal_accumulator_true_handled, NULL,
			      e_marshal_BOOLEAN__INT_POINTER_INT_BOXED,
			      G_TYPE_BOOLEAN, 4, G_TYPE_INT, G_TYPE_POINTER,
			      G_TYPE_INT, GDK_TYPE_EVENT);

	et_signals[CLICK] =
		g_signal_new ("click",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETreeClass, click),
			      g_signal_accumulator_true_handled, NULL,
			      e_marshal_BOOLEAN__INT_POINTER_INT_BOXED,
			      G_TYPE_BOOLEAN, 4, G_TYPE_INT, G_TYPE_POINTER,
			      G_TYPE_INT, GDK_TYPE_EVENT);

	et_signals[KEY_PRESS] =
		g_signal_new ("key_press",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETreeClass, key_press),
			      g_signal_accumulator_true_handled, NULL,
			      e_marshal_BOOLEAN__INT_POINTER_INT_BOXED,
			      G_TYPE_BOOLEAN, 4, G_TYPE_INT, G_TYPE_POINTER,
			      G_TYPE_INT, GDK_TYPE_EVENT);

	et_signals[START_DRAG] =
		g_signal_new ("start_drag",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETreeClass, start_drag),
			      NULL, NULL,
			      e_marshal_NONE__INT_POINTER_INT_BOXED,
			      G_TYPE_NONE, 4, G_TYPE_INT, G_TYPE_POINTER,
			      G_TYPE_INT, GDK_TYPE_EVENT);

	et_signals[STATE_CHANGE] =
		g_signal_new ("state_change",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETreeClass, state_change),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	et_signals[WHITE_SPACE_EVENT] =
		g_signal_new ("white_space_event",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETreeClass, white_space_event),
			      g_signal_accumulator_true_handled, NULL,
			      e_marshal_BOOLEAN__POINTER,
			      G_TYPE_BOOLEAN, 1, GDK_TYPE_EVENT);

	et_signals[TREE_DRAG_BEGIN] =
		g_signal_new ("tree_drag_begin",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETreeClass, tree_drag_begin),
			      NULL, NULL,
			      e_marshal_NONE__INT_POINTER_INT_BOXED,
			      G_TYPE_NONE, 4,
			      G_TYPE_INT,
			      G_TYPE_POINTER,
			      G_TYPE_INT,
			      GDK_TYPE_DRAG_CONTEXT);
	et_signals[TREE_DRAG_END] =
		g_signal_new ("tree_drag_end",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETreeClass, tree_drag_end),
			      NULL, NULL,
			      e_marshal_NONE__INT_POINTER_INT_BOXED,
			      G_TYPE_NONE, 4,
			      G_TYPE_INT,
			      G_TYPE_POINTER,
			      G_TYPE_INT,
			      GDK_TYPE_DRAG_CONTEXT);
	et_signals[TREE_DRAG_DATA_GET] =
		g_signal_new ("tree_drag_data_get",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETreeClass, tree_drag_data_get),
			      NULL, NULL,
			      e_marshal_NONE__INT_POINTER_INT_OBJECT_BOXED_UINT_UINT,
			      G_TYPE_NONE, 7,
			      G_TYPE_INT,
			      G_TYPE_POINTER,
			      G_TYPE_INT,
			      GDK_TYPE_DRAG_CONTEXT,
			      GTK_TYPE_SELECTION_DATA | G_SIGNAL_TYPE_STATIC_SCOPE,
			      G_TYPE_UINT,
			      G_TYPE_UINT);
	et_signals[TREE_DRAG_DATA_DELETE] =
		g_signal_new ("tree_drag_data_delete",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETreeClass, tree_drag_data_delete),
			      NULL, NULL,
			      e_marshal_NONE__INT_POINTER_INT_OBJECT,
			      G_TYPE_NONE, 4,
			      G_TYPE_INT,
			      G_TYPE_POINTER,
			      G_TYPE_INT,
			      GDK_TYPE_DRAG_CONTEXT);

	et_signals[TREE_DRAG_LEAVE] =
		g_signal_new ("tree_drag_leave",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETreeClass, tree_drag_leave),
			      NULL, NULL,
			      e_marshal_NONE__INT_POINTER_INT_OBJECT_UINT,
			      G_TYPE_NONE, 5,
			      G_TYPE_INT,
			      G_TYPE_POINTER,
			      G_TYPE_INT,
			      GDK_TYPE_DRAG_CONTEXT,
			      G_TYPE_UINT);
	et_signals[TREE_DRAG_MOTION] =
		g_signal_new ("tree_drag_motion",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETreeClass, tree_drag_motion),
			      NULL, NULL,
			      e_marshal_BOOLEAN__INT_POINTER_INT_OBJECT_INT_INT_UINT,
			      G_TYPE_BOOLEAN, 7,
			      G_TYPE_INT,
			      G_TYPE_POINTER,
			      G_TYPE_INT,
			      GDK_TYPE_DRAG_CONTEXT,
			      G_TYPE_INT,
			      G_TYPE_INT,
			      G_TYPE_UINT);
	et_signals[TREE_DRAG_DROP] =
		g_signal_new ("tree_drag_drop",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETreeClass, tree_drag_drop),
			      NULL, NULL,
			      e_marshal_BOOLEAN__INT_POINTER_INT_OBJECT_INT_INT_UINT,
			      G_TYPE_BOOLEAN, 7,
			      G_TYPE_INT,
			      G_TYPE_POINTER,
			      G_TYPE_INT,
			      GDK_TYPE_DRAG_CONTEXT,
			      G_TYPE_INT,
			      G_TYPE_INT,
			      G_TYPE_UINT);
	et_signals[TREE_DRAG_DATA_RECEIVED] =
		g_signal_new ("tree_drag_data_received",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETreeClass, tree_drag_data_received),
			      NULL, NULL,
			      e_marshal_NONE__INT_POINTER_INT_OBJECT_INT_INT_BOXED_UINT_UINT,
			      G_TYPE_NONE, 9,
			      G_TYPE_INT,
			      G_TYPE_POINTER,
			      G_TYPE_INT,
			      GDK_TYPE_DRAG_CONTEXT,
			      G_TYPE_INT,
			      G_TYPE_INT,
			      GTK_TYPE_SELECTION_DATA,
			      G_TYPE_UINT,
			      G_TYPE_UINT);

	class->set_scroll_adjustments = set_scroll_adjustments;

	widget_class->set_scroll_adjustments_signal =
		g_signal_new ("set_scroll_adjustments",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETreeClass, set_scroll_adjustments),
			      NULL, NULL,
			      e_marshal_NONE__OBJECT_OBJECT,
			      G_TYPE_NONE, 2, GTK_TYPE_ADJUSTMENT,
			      GTK_TYPE_ADJUSTMENT);

	g_object_class_install_property (object_class, PROP_LENGTH_THRESHOLD,
					 g_param_spec_int ("length_threshold",
							   "Length Threshold",
							   "Length Threshold",
							   0, G_MAXINT, 0,
							   G_PARAM_WRITABLE));
	g_object_class_install_property (object_class, PROP_HORIZONTAL_DRAW_GRID,
					 g_param_spec_boolean ("horizontal_draw_grid",
							       "Horizontal Draw Grid",
							       "Horizontal Draw Grid",
							       FALSE,
							       G_PARAM_WRITABLE));
	g_object_class_install_property (object_class, PROP_VERTICAL_DRAW_GRID,
					 g_param_spec_boolean ("vertical_draw_grid",
							       "Vertical Draw Grid",
							       "Vertical Draw Grid",
							       FALSE,
							       G_PARAM_WRITABLE));
	g_object_class_install_property (object_class, PROP_DRAW_FOCUS,
					 g_param_spec_boolean ("drawfocus",
							       "Draw focus",
							       "Draw focus",
							       FALSE,
							       G_PARAM_WRITABLE));

	g_object_class_install_property (object_class, PROP_ETTA,
					 g_param_spec_object ("ETreeTableAdapter",
							      "ETree table adapter",
							      "ETree table adapter",
							      E_TREE_TABLE_ADAPTER_TYPE,
							      G_PARAM_READABLE));

	g_object_class_install_property (object_class, PROP_UNIFORM_ROW_HEIGHT,
					 g_param_spec_boolean ("uniform_row_height",
							       "Uniform row height",
							       "Uniform row height",
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_ALWAYS_SEARCH,
					 g_param_spec_boolean ("always_search",
							       "Always search",
							       "Always search",
							       FALSE,
							       G_PARAM_READWRITE));

	gtk_widget_class_install_style_property (widget_class,
			   g_param_spec_boolean ("retro_look",
						 "Retro Look",
						 "Draw lines and +/- expanders.",
						 FALSE,
						 G_PARAM_READABLE));

	gtk_widget_class_install_style_property (widget_class,
			   g_param_spec_int ("expander_size",
					     "Expander Size",
					     "Size of the expander arrow",
					     0,
					     G_MAXINT,
					     10,
					     G_PARAM_READABLE));

	gal_a11y_e_tree_init ();
}

static void
tree_size_allocate (GtkWidget *widget, GtkAllocation *alloc, ETree *tree)
{
	gdouble width;

	g_return_if_fail (tree != NULL);
	g_return_if_fail (tree->priv != NULL);
	g_return_if_fail (tree->priv->info_text != NULL);

	gnome_canvas_get_scroll_region (
		GNOME_CANVAS (tree->priv->table_canvas),
		NULL, NULL, &width, NULL);

	width -= 60.0;

	g_object_set (
		tree->priv->info_text, "width", width,
		"clip_width", width, NULL);
}

/**
 * e_tree_set_info_message:
 * @tree: #ETree instance
 * @info_message: Message to set. Can be NULL.
 *
 * Creates an info message in table area, or removes old.
 **/
void
e_tree_set_info_message (ETree *tree, const gchar *info_message)
{
	GtkAllocation allocation;
	GtkWidget *widget;

	g_return_if_fail (tree != NULL);
	g_return_if_fail (tree->priv != NULL);

	if (!tree->priv->info_text && (!info_message || !*info_message))
		return;

	if (!info_message || !*info_message) {
		g_signal_handler_disconnect (tree, tree->priv->info_text_resize_id);
		gtk_object_destroy (GTK_OBJECT (tree->priv->info_text));
		tree->priv->info_text = NULL;
		return;
	}

	widget = GTK_WIDGET (tree->priv->table_canvas);
	gtk_widget_get_allocation (widget, &allocation);

	if (!tree->priv->info_text) {
		tree->priv->info_text = gnome_canvas_item_new (
			GNOME_CANVAS_GROUP (gnome_canvas_root (tree->priv->table_canvas)),
			e_text_get_type (),
			"anchor", GTK_ANCHOR_NW,
			"line_wrap", TRUE,
			"clip", TRUE,
			"justification", GTK_JUSTIFY_LEFT,
			"text", info_message,
			"draw_background", FALSE,
			"width", (gdouble) allocation.width - 60.0,
			"clip_width", (gdouble) allocation.width - 60.0,
			NULL);

		e_canvas_item_move_absolute (tree->priv->info_text, 30, 30);

		tree->priv->info_text_resize_id = g_signal_connect (
			tree, "size_allocate",
			G_CALLBACK (tree_size_allocate), tree);
	} else
		gnome_canvas_item_set (tree->priv->info_text, "text", info_message, NULL);
}
