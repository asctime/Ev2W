/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-cell-tree.c - Tree cell object.
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * Authors:
 *   Chris Toshok <toshok@ximian.com>
 *
 * A majority of code taken from:
 *
 * the ECellText renderer.
 * Copyright 1998, The Free Software Foundation
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <config.h>

#include <ctype.h>
#include <math.h>
#include <stdio.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <libgnomecanvas/gnome-canvas.h>

#include "gal-a11y-e-cell-registry.h"
#include "gal-a11y-e-cell-tree.h"
#include "e-util/e-util.h"

#include "e-cell-tree.h"
#include "e-table-item.h"
#include "e-tree.h"
#include "e-tree-model.h"
#include "e-tree-table-adapter.h"

#include "tree-expanded.xpm"
#include "tree-unexpanded.xpm"

G_DEFINE_TYPE (ECellTree, e_cell_tree, E_CELL_TYPE)

typedef struct {
	ECellView    cell_view;
	ECellView   *subcell_view;
	GdkGC       *gc;

	GnomeCanvas *canvas;
	gboolean retro_look;
	gboolean prelit;
	gint animate_timeout;

} ECellTreeView;

#define INDENT_AMOUNT 16

ECellView *
e_cell_tree_view_get_subcell_view (ECellView *ect)
{
	return ((ECellTreeView *)ect)->subcell_view;
}

static ETreePath
e_cell_tree_get_node (ETableModel *table_model, gint row)
{
	return e_table_model_value_at (table_model, -1, row);
}

static ETreeModel*
e_cell_tree_get_tree_model (ETableModel *table_model, gint row)
{
	return e_table_model_value_at (table_model, -2, row);
}

static ETreeTableAdapter *
e_cell_tree_get_tree_table_adapter (ETableModel *table_model, gint row)
{
	return e_table_model_value_at (table_model, -3, row);
}

static gint
visible_depth_of_node (ETableModel *model, gint row)
{
	ETreeModel *tree_model = e_cell_tree_get_tree_model (model, row);
	ETreeTableAdapter *adapter = e_cell_tree_get_tree_table_adapter (model, row);
	ETreePath path = e_cell_tree_get_node (model, row);
	return (e_tree_model_node_depth (tree_model, path)
		- (e_tree_table_adapter_root_node_is_visible (adapter) ? 0 : 1));
}

/* If this is changed to not include the width of the expansion pixmap
   if the path is not expandable, then max_width needs to change as
   well. */
static gint
offset_of_node (ETableModel *table_model, gint row)
{
	ETreeModel *tree_model = e_cell_tree_get_tree_model (table_model, row);
	ETreePath path = e_cell_tree_get_node (table_model, row);

	if (visible_depth_of_node (table_model, row) >= 0 ||
	    e_tree_model_node_is_expandable (tree_model, path)) {
		return (visible_depth_of_node (table_model, row) + 1) * INDENT_AMOUNT;
	} else {
		return 0;
	}
}

/*
 * ECell::new_view method
 */
static ECellView *
ect_new_view (ECell *ecell, ETableModel *table_model, gpointer e_table_item_view)
{
	ECellTree *ect = E_CELL_TREE (ecell);
	ECellTreeView *tree_view = g_new0 (ECellTreeView, 1);
	GnomeCanvas *canvas = GNOME_CANVAS_ITEM (e_table_item_view)->canvas;

	tree_view->cell_view.ecell = ecell;
	tree_view->cell_view.e_table_model = table_model;
	tree_view->cell_view.e_table_item_view = e_table_item_view;
        tree_view->cell_view.kill_view_cb = NULL;
        tree_view->cell_view.kill_view_cb_data = NULL;

	/* create our subcell view */
	tree_view->subcell_view = e_cell_new_view (ect->subcell, table_model, e_table_item_view /* XXX */);

	tree_view->canvas = canvas;

	return (ECellView *)tree_view;
}

/*
 * ECell::kill_view method
 */
static void
ect_kill_view (ECellView *ecv)
{
	ECellTreeView *tree_view = (ECellTreeView *) ecv;

        if (tree_view->cell_view.kill_view_cb)
            (tree_view->cell_view.kill_view_cb)(ecv, tree_view->cell_view.kill_view_cb_data);

        if (tree_view->cell_view.kill_view_cb_data)
            g_list_free (tree_view->cell_view.kill_view_cb_data);

	/* kill our subcell view */
	e_cell_kill_view (tree_view->subcell_view);

	g_free (tree_view);
}

/*
 * ECell::realize method
 */
static void
ect_realize (ECellView *ecell_view)
{
	ECellTreeView *tree_view = (ECellTreeView *) ecell_view;
	GdkWindow *window;

	/* realize our subcell view */
	e_cell_realize (tree_view->subcell_view);

	window = gtk_widget_get_window (GTK_WIDGET (tree_view->canvas));
	tree_view->gc = gdk_gc_new (window);

	gdk_gc_set_line_attributes (tree_view->gc, 1,
				    GDK_LINE_ON_OFF_DASH,
				    GDK_CAP_NOT_LAST,
				    GDK_JOIN_MITER);
	gdk_gc_set_dashes (tree_view->gc, 0, (gint8 *)"\1\1", 2);

	if (E_CELL_CLASS (e_cell_tree_parent_class)->realize)
		(* E_CELL_CLASS (e_cell_tree_parent_class)->realize) (ecell_view);
}

/*
 * ECell::unrealize method
 */
static void
ect_unrealize (ECellView *ecv)
{
	ECellTreeView *tree_view = (ECellTreeView *) ecv;

	/* unrealize our subcell view. */
	e_cell_unrealize (tree_view->subcell_view);

	g_object_unref (tree_view->gc);
	tree_view->gc = NULL;

	if (E_CELL_CLASS (e_cell_tree_parent_class)->unrealize)
		(* E_CELL_CLASS (e_cell_tree_parent_class)->unrealize) (ecv);
}

static void
draw_retro_expander (ECellTreeView *ectv, GdkDrawable *drawable, gboolean expanded, GdkRectangle *rect)
{
	GdkPixbuf *image;
	gint image_width, image_height;
	ECellTree *ect = E_CELL_TREE (ectv->cell_view.ecell);

	image = expanded ? ect->open_pixbuf : ect->closed_pixbuf;

	image_width = gdk_pixbuf_get_width (image);
	image_height = gdk_pixbuf_get_height (image);

	gdk_draw_pixbuf (drawable,
			 NULL,
			 image,
			 rect->x, rect->y,
			 rect->width - image_width / 2,
			 rect->height - image_height / 2,
			 image_width, image_height,
			 GDK_RGB_DITHER_NORMAL,
			 image_width, 0);
}

static void
draw_expander (ECellTreeView *ectv, GdkDrawable *drawable, GtkExpanderStyle expander_style, GtkStateType state, GdkRectangle *rect)
{
	GtkStyle *style;
	GtkWidget *tree;
	gint exp_size;

	tree = gtk_widget_get_parent (GTK_WIDGET (ectv->canvas));
	style = gtk_widget_get_style (tree);

	gtk_widget_style_get (tree, "expander_size", &exp_size, NULL);

	gtk_paint_expander (
		style, drawable, state, rect, tree, "treeview",
		rect->x + rect->width - exp_size / 2,
		rect->y + rect->height / 2, expander_style);
}

/*
 * ECell::draw method
 */
static void
ect_draw (ECellView *ecell_view, GdkDrawable *drawable,
	  gint model_col, gint view_col, gint row, ECellFlags flags,
	  gint x1, gint y1, gint x2, gint y2)
{
	ECellTreeView *tree_view = (ECellTreeView *)ecell_view;
	ETreeModel *tree_model = e_cell_tree_get_tree_model (ecell_view->e_table_model, row);
	ETreeTableAdapter *tree_table_adapter = e_cell_tree_get_tree_table_adapter (ecell_view->e_table_model, row);
	ETreePath node;
	GdkRectangle rect, *clip_rect = NULL;
	GtkWidget *canvas = GTK_WIDGET (tree_view->canvas);
	GtkStyle *style;
	GdkGC *fg_gc;
	GdkColor *foreground;
	gboolean selected;

	gint offset, subcell_offset;

	style = gtk_widget_get_style (canvas);
	fg_gc = style->fg_gc[GTK_STATE_ACTIVE];

	selected = flags & E_CELL_SELECTED;

	/* only draw the tree effects if we're the active sort */
	if (/* XXX */ TRUE) {
		GdkPixbuf *node_image;
		gint node_image_width = 0, node_image_height = 0;
		ETreePath parent_node;
		ETree *tree;

		tree = E_TREE (gtk_widget_get_parent (canvas));

		gtk_widget_style_get (GTK_WIDGET (tree),
				      "retro_look", &tree_view->retro_look,
				      NULL);
		tree_view->prelit = FALSE;

		node = e_cell_tree_get_node (ecell_view->e_table_model, row);

		offset = offset_of_node (ecell_view->e_table_model, row);
		subcell_offset = offset;

		node_image = e_tree_model_icon_at (tree_model, node);

		if (node_image) {
			node_image_width = gdk_pixbuf_get_width (node_image);
			node_image_height = gdk_pixbuf_get_height (node_image);
		}

		/*
		 * Be a nice citizen: clip to the region we are supposed to draw on
		 */
		rect.x = x1;
		rect.y = y1;
		rect.width = subcell_offset + node_image_width;
		rect.height = y2 - y1;

		gdk_gc_set_clip_rectangle (tree_view->gc, &rect);
		gdk_gc_set_clip_rectangle (fg_gc, &rect);
		clip_rect = &rect;

		if (selected) {
			foreground = &style->text[GTK_STATE_SELECTED];
		} else {
			foreground = &style->text[GTK_STATE_NORMAL];
		}

		gdk_gc_set_foreground (tree_view->gc, foreground);

		/* draw our lines */
		if (tree_view->retro_look && E_CELL_TREE (tree_view->cell_view.ecell)->draw_lines) {

			gint depth;

			if (visible_depth_of_node (ecell_view->e_table_model, row) > 0
			    || e_tree_model_node_get_children (tree_model, node, NULL) > 0)
				gdk_draw_line (drawable, tree_view->gc,
					       rect.x + offset - INDENT_AMOUNT / 2 + 1,
					       rect.y + rect.height / 2,
					       rect.x + offset,
					       rect.y + rect.height / 2);

			if (visible_depth_of_node (ecell_view->e_table_model, row) != 0) {
				gdk_draw_line (drawable, tree_view->gc,
					       rect.x + offset - INDENT_AMOUNT / 2,
					       rect.y,
					       rect.x + offset - INDENT_AMOUNT / 2,
					       (e_tree_table_adapter_node_get_next (tree_table_adapter, node)
						? rect.y + rect.height
						: rect.y + rect.height / 2));
			}

			/* now traverse back up to the root of the tree, checking at
			   each level if the node has siblings, and drawing the
			   correct vertical pipe for it's configuration. */
			parent_node = e_tree_model_node_get_parent (tree_model, node);
			offset -= INDENT_AMOUNT;
			depth = visible_depth_of_node (ecell_view->e_table_model, row) - 1;
			while (parent_node && depth != 0) {
				if (e_tree_table_adapter_node_get_next (tree_table_adapter, parent_node)) {
					gdk_draw_line (drawable, tree_view->gc,
						       rect.x + offset - INDENT_AMOUNT / 2,
						       rect.y,
						       rect.x + offset - INDENT_AMOUNT / 2,
						       rect.y + rect.height);
				}
				parent_node = e_tree_model_node_get_parent (tree_model, parent_node);
				depth--;
				offset -= INDENT_AMOUNT;
			}
		}

		/* now draw our icon if we're expandable */
		if (e_tree_model_node_is_expandable (tree_model, node)) {
			gboolean expanded = e_tree_table_adapter_node_is_expanded (tree_table_adapter, node);
			GdkRectangle r;
			if (tree_view->retro_look) {
				r.x = 0;
				r.y = 0;
				r.width = x1 + subcell_offset - INDENT_AMOUNT / 2,
				r.height = y1 + (y2 - y1) / 2,
				draw_retro_expander (tree_view, drawable, expanded, &r);
			} else {
				r = rect;
				r.width -= node_image_width + 2;
				draw_expander (tree_view, drawable, expanded ? GTK_EXPANDER_EXPANDED : GTK_EXPANDER_COLLAPSED, GTK_STATE_NORMAL, &r);
			}
		}

		if (node_image) {
			gdk_draw_pixbuf (drawable,
					 NULL,
					 node_image,
					 0, 0,
					 x1 + subcell_offset,
					 y1 + (y2 - y1) / 2 - node_image_height / 2,
					 node_image_width, node_image_height,
					 GDK_RGB_DITHER_NORMAL,
					 node_image_width, 0);
			subcell_offset += node_image_width;
		}
	}

	/* Now cause our subcell to draw its contents, shifted by
	   subcell_offset pixels */
	e_cell_draw (tree_view->subcell_view, drawable,
		     model_col, view_col, row, flags,
		     x1 + subcell_offset, y1, x2, y2);

	if (clip_rect) {
		gdk_gc_set_clip_rectangle (tree_view->gc, NULL);
		gdk_gc_set_clip_rectangle (fg_gc, NULL);
	}
}

static void
adjust_event_position (GdkEvent *event, gint offset)
{
	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_BUTTON_RELEASE:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		event->button.x += offset;
		break;
	case GDK_MOTION_NOTIFY:
		event->motion.x += offset;
		break;
	default:
		break;
	}
}

static gboolean
event_in_expander (GdkEvent *event, gint offset, gint height)
{
	switch (event->type) {
	case GDK_BUTTON_PRESS:
		return (event->button.x > (offset - INDENT_AMOUNT) && event->button.x < offset);
	case GDK_MOTION_NOTIFY:
		return (event->motion.x > (offset - INDENT_AMOUNT) && event->motion.x < offset &&
			event->motion.y > 2 && event->motion.y < (height - 2));
	default:
		break;
	}

	return FALSE;
}

/*
 * ECell::height method
 */
static gint
ect_height (ECellView *ecell_view, gint model_col, gint view_col, gint row)
{
	ECellTreeView *tree_view = (ECellTreeView *) ecell_view;

	return (((e_cell_height (tree_view->subcell_view, model_col, view_col, row)) + 1) / 2) * 2;
}

typedef struct {
	ECellTreeView *ectv;
	ETreeTableAdapter *etta;
	ETreePath node;
	gboolean expanded;
	gboolean finish;
	GdkRectangle area;
} animate_closure_t;

static gboolean
animate_expander (gpointer data)
{
	GtkLayout *layout;
	GdkWindow *window;
	animate_closure_t *closure = (animate_closure_t *) data;

	if (closure->finish) {
		e_tree_table_adapter_node_set_expanded (closure->etta, closure->node, !closure->expanded);
		closure->ectv->animate_timeout = 0;
		g_free (data);
		return FALSE;
	}

	layout = GTK_LAYOUT (closure->ectv->canvas);
	window = gtk_layout_get_bin_window (layout);

	draw_expander (
		closure->ectv, window, closure->expanded ?
		GTK_EXPANDER_SEMI_COLLAPSED :
		GTK_EXPANDER_SEMI_EXPANDED,
		GTK_STATE_NORMAL, &closure->area);
	closure->finish = TRUE;

	return TRUE;
}

/*
 * ECell::event method
 */
static gint
ect_event (ECellView *ecell_view, GdkEvent *event, gint model_col, gint view_col, gint row, ECellFlags flags, ECellActions *actions)
{
	GtkLayout *layout;
	GdkWindow *window;
	ECellTreeView *tree_view = (ECellTreeView *) ecell_view;
	ETreeModel *tree_model = e_cell_tree_get_tree_model (ecell_view->e_table_model, row);
	ETreeTableAdapter *etta = e_cell_tree_get_tree_table_adapter (ecell_view->e_table_model, row);
	ETreePath node = e_cell_tree_get_node (ecell_view->e_table_model, row);
	gint offset = offset_of_node (ecell_view->e_table_model, row);
	gint result;

	layout = GTK_LAYOUT (tree_view->canvas);
	window = gtk_layout_get_bin_window (layout);

	switch (event->type) {
	case GDK_BUTTON_PRESS:

		if (event_in_expander (event, offset, 0)) {
			if (e_tree_model_node_is_expandable (tree_model, node)) {
				gboolean expanded = e_tree_table_adapter_node_is_expanded (etta, node);
				if (tree_view->retro_look)
					e_tree_table_adapter_node_set_expanded (etta, node, !expanded);
				else {
					gint tmp_row = row;
					GdkRectangle area;
					animate_closure_t *closure = g_new0 (animate_closure_t, 1);
					gint hgt;

					e_table_item_get_cell_geometry (tree_view->cell_view.e_table_item_view,
								&tmp_row, &view_col, &area.x, &area.y, NULL, &area.height);
					area.width = offset - 2;
					hgt = e_cell_height (ecell_view, model_col, view_col, row);

					if (hgt != area.height) /* Composite cells */
						area.height += hgt;

					draw_expander (
						tree_view, window, expanded ?
						GTK_EXPANDER_SEMI_EXPANDED :
						GTK_EXPANDER_SEMI_COLLAPSED,
						GTK_STATE_NORMAL, &area);
					closure->ectv = tree_view;
					closure->etta = etta;
					closure->node = node;
					closure->expanded = expanded;
					closure->area = area;
					tree_view->animate_timeout = g_timeout_add (50, animate_expander, closure);
				}
				return TRUE;
			}
		}
		else if (event->button.x < (offset - INDENT_AMOUNT))
			return FALSE;
		break;

	case GDK_MOTION_NOTIFY:

		if (!tree_view->retro_look && e_tree_model_node_is_expandable (tree_model, node)) {
			gint height = ect_height (ecell_view, model_col, view_col, row);
			GdkRectangle area;
			gboolean in_expander = event_in_expander (event, offset, height);

			if (tree_view->prelit ^ in_expander) {
				gint tmp_row = row;
				e_table_item_get_cell_geometry (tree_view->cell_view.e_table_item_view,
								&tmp_row, &view_col, &area.x, &area.y, NULL, &area.height);
				area.width = offset - 2;
				draw_expander (
					tree_view, window,
					e_tree_table_adapter_node_is_expanded (etta, node) ?
					GTK_EXPANDER_EXPANDED : GTK_EXPANDER_COLLAPSED,
					in_expander ? GTK_STATE_PRELIGHT : GTK_STATE_NORMAL, &area);
				tree_view->prelit = in_expander;
				return TRUE;
			}

		}
		break;

	case GDK_LEAVE_NOTIFY:

		if (tree_view->prelit) {
			gint tmp_row = row;
			GdkRectangle area;
			e_table_item_get_cell_geometry (tree_view->cell_view.e_table_item_view,
							&tmp_row, &view_col, &area.x, &area.y, NULL, &area.height);
			area.width = offset - 2;
			draw_expander (
				tree_view, window,
				e_tree_table_adapter_node_is_expanded (etta, node) ?
				GTK_EXPANDER_EXPANDED : GTK_EXPANDER_COLLAPSED,
				GTK_STATE_NORMAL, &area);
			tree_view->prelit = FALSE;
		}
		return TRUE;

	default:
		break;
	}

	adjust_event_position (event, -offset);
	result = e_cell_event (tree_view->subcell_view, event, model_col, view_col, row, flags, actions);
	adjust_event_position (event, offset);

	return result;
}

/*
 * ECell::max_width method
 */
static gint
ect_max_width (ECellView *ecell_view, gint model_col, gint view_col)
{
	ECellTreeView *tree_view = (ECellTreeView *) ecell_view;
	gint row;
	gint number_of_rows;
	gint max_width = 0;
	gint width = 0;
	gint subcell_max_width = 0;
	gboolean per_row = e_cell_max_width_by_row_implemented (tree_view->subcell_view);

	number_of_rows = e_table_model_row_count (ecell_view->e_table_model);

	if (!per_row)
		subcell_max_width = e_cell_max_width (tree_view->subcell_view, model_col, view_col);

	for (row = 0; row < number_of_rows; row++) {
		ETreeModel *tree_model = e_cell_tree_get_tree_model (ecell_view->e_table_model, row);
		ETreePath node;
		GdkPixbuf *node_image;
		gint node_image_width = 0;

		gint offset, subcell_offset;
#if 0
		gboolean expanded, expandable;
		ETreeTableAdapter *tree_table_adapter = e_cell_tree_get_tree_table_adapter (ecell_view->e_table_model, row);
#endif

		node = e_cell_tree_get_node (ecell_view->e_table_model, row);

		offset = offset_of_node (ecell_view->e_table_model, row);
		subcell_offset = offset;

		node_image = e_tree_model_icon_at (tree_model, node);

		if (node_image) {
			node_image_width = gdk_pixbuf_get_width (node_image);
		}

		width = subcell_offset + node_image_width;

		if (per_row)
			width += e_cell_max_width_by_row (tree_view->subcell_view, model_col, view_col, row);
		else
			width += subcell_max_width;

#if 0
		expandable = e_tree_model_node_is_expandable (tree_model, node);
		expanded = e_tree_table_adapter_node_is_expanded (tree_table_adapter, node);

		/* This is unnecessary since this is already handled
                   by the offset_of_node function.  If that changes,
                   this will have to change too. */

		if (expandable) {
			GdkPixbuf *image;

			image = (expanded
				 ? E_CELL_TREE (tree_view->cell_view.ecell)->open_pixbuf
				 : E_CELL_TREE (tree_view->cell_view.ecell)->closed_pixbuf);

			width += gdk_pixbuf_get_width (image);
		}
#endif

		max_width = MAX (max_width, width);
	}

	return max_width;
}

/*
 * ECellView::get_bg_color method
 */
static gchar *
ect_get_bg_color (ECellView *ecell_view, gint row)
{
	ECellTreeView *tree_view = (ECellTreeView *) ecell_view;

	return e_cell_get_bg_color (tree_view->subcell_view, row);
}

/*
 * ECellView::enter_edit method
 */
static gpointer
ect_enter_edit (ECellView *ecell_view, gint model_col, gint view_col, gint row)
{
	/* just defer to our subcell's view */
	ECellTreeView *tree_view = (ECellTreeView *) ecell_view;

	return e_cell_enter_edit (tree_view->subcell_view, model_col, view_col, row);
}

/*
 * ECellView::leave_edit method
 */
static void
ect_leave_edit (ECellView *ecell_view, gint model_col, gint view_col, gint row, gpointer edit_context)
{
	/* just defer to our subcell's view */
	ECellTreeView *tree_view = (ECellTreeView *) ecell_view;

	e_cell_leave_edit (tree_view->subcell_view, model_col, view_col, row, edit_context);
}

static void
ect_print (ECellView *ecell_view, GtkPrintContext *context,
	   gint model_col, gint view_col, gint row,
	   gdouble width, gdouble height)
{
	ECellTreeView *tree_view = (ECellTreeView *) ecell_view;
	cairo_t *cr = gtk_print_context_get_cairo_context (context);

	cairo_save (cr);

	if (/* XXX only if we're the active sort */ TRUE) {
		ETreeModel *tree_model = e_cell_tree_get_tree_model (ecell_view->e_table_model, row);
		ETreeTableAdapter *tree_table_adapter = e_cell_tree_get_tree_table_adapter (ecell_view->e_table_model, row);
		ETreePath node = e_cell_tree_get_node (ecell_view->e_table_model, row);
		gint offset = offset_of_node (ecell_view->e_table_model, row);
		gint subcell_offset = offset;
		gboolean expandable = e_tree_model_node_is_expandable (tree_model, node);
		gboolean expanded = e_tree_table_adapter_node_is_expanded (tree_table_adapter, node);

		/* draw our lines */
		if (E_CELL_TREE (tree_view->cell_view.ecell)->draw_lines) {
			gint depth;

			if (!e_tree_model_node_is_root (tree_model, node)
			    || e_tree_model_node_get_children (tree_model, node, NULL) > 0) {
				cairo_move_to (cr,
					offset - INDENT_AMOUNT / 2,
					height / 2);
				cairo_line_to (cr, offset, height / 2);
			}

			if (visible_depth_of_node (ecell_view->e_table_model, row) != 0) {
				cairo_move_to (cr,
					offset - INDENT_AMOUNT / 2, height);
				cairo_line_to (cr,
					offset - INDENT_AMOUNT / 2,
					e_tree_table_adapter_node_get_next
					(tree_table_adapter, node) ? 0 :
					height / 2);
			}

			/* now traverse back up to the root of the tree, checking at
			   each level if the node has siblings, and drawing the
			   correct vertical pipe for it's configuration. */
			node = e_tree_model_node_get_parent (tree_model, node);
			depth = visible_depth_of_node (ecell_view->e_table_model, row) - 1;
			offset -= INDENT_AMOUNT;
			while (node && depth != 0) {
				if (e_tree_table_adapter_node_get_next (tree_table_adapter, node)) {
					cairo_move_to (cr,
						offset - INDENT_AMOUNT / 2,
						height);
					cairo_line_to (cr,
						offset - INDENT_AMOUNT / 2, 0);
				}
				node = e_tree_model_node_get_parent (tree_model, node);
				depth--;
				offset -= INDENT_AMOUNT;
			}
		}

		/* now draw our icon if we're expandable */
		if (expandable) {
			ECellTree *tree;
			GdkPixbuf *pixbuf;
			gint image_width;
			gint image_height;

			tree = E_CELL_TREE (tree_view->cell_view.ecell);

			if (expanded)
				pixbuf = tree->open_pixbuf;
			else
				pixbuf = tree->closed_pixbuf;

			image_width = gdk_pixbuf_get_width (pixbuf);
			image_height = gdk_pixbuf_get_height (pixbuf);

			gdk_cairo_set_source_pixbuf (
				cr, pixbuf, subcell_offset -
				INDENT_AMOUNT / 2 - image_width - 2,
				height / 2 - image_height / 2);
		}

		cairo_stroke (cr);

		cairo_translate (cr, subcell_offset, 0);
		width -= subcell_offset;
	}

	cairo_restore (cr);

	e_cell_print (tree_view->subcell_view, context, model_col, view_col, row, width, height);
}

static gdouble
ect_print_height (ECellView *ecell_view, GtkPrintContext *context,
		  gint model_col, gint view_col, gint row,
		  gdouble width)
{
	return 12; /* XXX */
}

/*
 * GObject::dispose method
 */
static void
ect_dispose (GObject *object)
{
	ECellTree *ect = E_CELL_TREE (object);

	/* destroy our subcell */
	if (ect->subcell)
		g_object_unref (ect->subcell);
	ect->subcell = NULL;

	if (ect->open_pixbuf)
		g_object_unref (ect->open_pixbuf);
	ect->open_pixbuf = NULL;

	if (ect->closed_pixbuf)
		g_object_unref (ect->closed_pixbuf);
	ect->closed_pixbuf = NULL;

	G_OBJECT_CLASS (e_cell_tree_parent_class)->dispose (object);
}

static void
e_cell_tree_class_init (ECellTreeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	ECellClass *ecc = E_CELL_CLASS (klass);

	object_class->dispose = ect_dispose;

	ecc->new_view         = ect_new_view;
	ecc->kill_view        = ect_kill_view;
	ecc->realize          = ect_realize;
	ecc->unrealize        = ect_unrealize;
	ecc->draw             = ect_draw;
	ecc->event            = ect_event;
	ecc->height           = ect_height;
	ecc->enter_edit       = ect_enter_edit;
	ecc->leave_edit       = ect_leave_edit;
	ecc->print            = ect_print;
	ecc->print_height     = ect_print_height;
	ecc->max_width        = ect_max_width;
	ecc->get_bg_color     = ect_get_bg_color;

        gal_a11y_e_cell_registry_add_cell_type (NULL, E_CELL_TREE_TYPE, gal_a11y_e_cell_tree_new);
}

static void
e_cell_tree_init (ECellTree *ect)
{
	/* nothing to do */
}

/**
 * e_cell_tree_construct:
 * @ect: the ECellTree we're constructing.
 * @open_pixbuf: pixbuf to be used instead of the '-' icon.
 * @closed_pixbuf: pixbuf to be used instead of the '+' icon.
 * @draw_lines: whether or not to draw the lines between parents/children/siblings.
 * @subcell: the ECell to render to the right of the tree effects.
 *
 * Constructs an ECellTree.  used by subclasses that need to
 * initialize a nested ECellTree.  See e_cell_tree_new() for more info.
 *
 **/
void
e_cell_tree_construct (ECellTree *ect,
		       GdkPixbuf *open_pixbuf,
		       GdkPixbuf *closed_pixbuf,
		       gboolean draw_lines,
		       ECell *subcell)
{
	ect->subcell = subcell;
	if (subcell)
		g_object_ref_sink (subcell);
	if (open_pixbuf)
		ect->open_pixbuf = open_pixbuf;
	else
		ect->open_pixbuf = gdk_pixbuf_new_from_xpm_data ((const gchar **)tree_expanded_xpm);
	if (closed_pixbuf)
		ect->closed_pixbuf = closed_pixbuf;
	else
		ect->closed_pixbuf = gdk_pixbuf_new_from_xpm_data ((const gchar **)tree_unexpanded_xpm);

	ect->draw_lines = draw_lines;
}

/**
 * e_cell_tree_new:
 * @open_pixbuf: pixbuf to be used instead of the '-' icon.
 * @closed_pixbuf: pixbuf to be used instead of the '+' icon.
 * @draw_lines: whether or not to draw the lines between parents/children/siblings.
 * @subcell: the ECell to render to the right of the tree effects.
 *
 * Creates a new ECell renderer that can be used to render tree
 * effects that come from an ETreeModel.  Various assumptions are made
 * as to the fact that the ETableModel the ETable this cell is
 * associated with is in fact an ETreeModel.  The cell uses special
 * columns to get at structural information (needed to draw the
 * lines/icons.
 *
 * Return value: an ECell object that can be used to render trees.
 **/
ECell *
e_cell_tree_new (GdkPixbuf *open_pixbuf,
		 GdkPixbuf *closed_pixbuf,
		 gboolean draw_lines,
		 ECell *subcell)
{
	ECellTree *ect = g_object_new (E_CELL_TREE_TYPE, NULL);

	e_cell_tree_construct (ect, open_pixbuf, closed_pixbuf, draw_lines, subcell);

	return (ECell *) ect;
}

