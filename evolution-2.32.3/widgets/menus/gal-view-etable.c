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

#include "table/e-table-config.h"

#include "gal-view-etable.h"

G_DEFINE_TYPE (GalViewEtable, gal_view_etable, GAL_TYPE_VIEW)

static void
detach_table (GalViewEtable *view)
{
	if (view->table == NULL)
		return;
	if (view->table_state_changed_id) {
		g_signal_handler_disconnect (view->table,
					     view->table_state_changed_id);
		view->table_state_changed_id = 0;
	}
	g_object_unref (view->table);
	view->table = NULL;
}

static void
detach_tree (GalViewEtable *view)
{
	if (view->tree == NULL)
		return;
	if (view->tree_state_changed_id) {
		g_signal_handler_disconnect (view->tree,
					     view->tree_state_changed_id);
		view->tree_state_changed_id = 0;
	}
	g_object_unref (view->tree);
	view->tree = NULL;
}

static void
config_changed (ETableConfig *config, GalViewEtable *view)
{
	ETableState *state;
	if (view->state)
		g_object_unref (view->state);
	g_object_get (config,
		      "state", &state,
		      NULL);
	view->state = e_table_state_duplicate (state);
	g_object_unref (state);

	gal_view_changed (GAL_VIEW (view));
}

static void
gal_view_etable_edit            (GalView *view, GtkWindow *parent)
{
	GalViewEtable *etable_view = GAL_VIEW_ETABLE (view);
	ETableConfig *config;

	config = e_table_config_new (etable_view->title,
				    etable_view->spec,
				    etable_view->state,
				    parent);

	g_signal_connect(config, "changed",
			 G_CALLBACK (config_changed), view);
}

static void
gal_view_etable_load  (GalView *view,
		       const gchar *filename)
{
	e_table_state_load_from_file (GAL_VIEW_ETABLE (view)->state, filename);
}

static void
gal_view_etable_save    (GalView *view,
			 const gchar *filename)
{
	e_table_state_save_to_file (GAL_VIEW_ETABLE (view)->state, filename);
}

static const gchar *
gal_view_etable_get_title       (GalView *view)
{
	return GAL_VIEW_ETABLE (view)->title;
}

static void
gal_view_etable_set_title (GalView *view,
			   const gchar *title)
{
	g_free (GAL_VIEW_ETABLE (view)->title);
	GAL_VIEW_ETABLE (view)->title = g_strdup (title);
}

static const gchar *
gal_view_etable_get_type_code (GalView *view)
{
	return "etable";
}

static GalView *
gal_view_etable_clone       (GalView *view)
{
	GalViewEtable *gve, *new;

	gve = GAL_VIEW_ETABLE (view);

	new = g_object_new (GAL_TYPE_VIEW_ETABLE, NULL);
	new->spec  = gve->spec;
	new->title = g_strdup (gve->title);
	new->state = e_table_state_duplicate (gve->state);

	g_object_ref (new->spec);

	return GAL_VIEW (new);
}

static void
gal_view_etable_dispose         (GObject *object)
{
	GalViewEtable *view = GAL_VIEW_ETABLE (object);

	gal_view_etable_detach (view);

	g_free (view->title);
	view->title = NULL;

	if (view->spec)
		g_object_unref (view->spec);
	view->spec = NULL;

	if (view->state)
		g_object_unref (view->state);
	view->state = NULL;

	if (G_OBJECT_CLASS (gal_view_etable_parent_class)->dispose)
		(* G_OBJECT_CLASS (gal_view_etable_parent_class)->dispose) (object);
}

static void
gal_view_etable_class_init      (GalViewEtableClass *klass)
{
	GalViewClass *gal_view_class  = GAL_VIEW_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	gal_view_class->edit          = gal_view_etable_edit;
	gal_view_class->load          = gal_view_etable_load;
	gal_view_class->save          = gal_view_etable_save;
	gal_view_class->get_title     = gal_view_etable_get_title;
	gal_view_class->set_title     = gal_view_etable_set_title;
	gal_view_class->get_type_code = gal_view_etable_get_type_code;
	gal_view_class->clone         = gal_view_etable_clone;

	object_class->dispose         = gal_view_etable_dispose;
}

static void
gal_view_etable_init      (GalViewEtable *gve)
{
	gve->spec  = NULL;
	gve->state = e_table_state_new ();
	gve->title = NULL;
}

/**
 * gal_view_etable_new
 * @spec: The ETableSpecification that this view will be based upon.
 * @title: The name of the new view.
 *
 * Returns a new GalViewEtable.  This is primarily for use by
 * GalViewFactoryEtable.
 *
 * Returns: The new GalViewEtable.
 */
GalView *
gal_view_etable_new (ETableSpecification *spec,
		     const gchar *title)
{
	GalViewEtable *view;

	g_return_val_if_fail (E_IS_TABLE_SPECIFICATION (spec), NULL);

	view = g_object_new (GAL_TYPE_VIEW_ETABLE, NULL);

	return gal_view_etable_construct (view, spec, title);
}

/**
 * gal_view_etable_construct
 * @view: The view to construct.
 * @spec: The ETableSpecification that this view will be based upon.
 * @title: The name of the new view.
 *
 * constructs the GalViewEtable.  To be used by subclasses and
 * language bindings.
 *
 * Returns: The GalViewEtable.
 */
GalView *
gal_view_etable_construct  (GalViewEtable *view,
			    ETableSpecification *spec,
			    const gchar *title)
{
	g_return_val_if_fail (GAL_IS_VIEW_ETABLE (view), NULL);
	g_return_val_if_fail (E_IS_TABLE_SPECIFICATION (spec), NULL);

	if (spec)
		g_object_ref (spec);
	view->spec = spec;

	if (view->state)
		g_object_unref (view->state);
	view->state = e_table_state_duplicate (spec->state);

	view->title = g_strdup (title);

	return GAL_VIEW (view);
}

void
gal_view_etable_set_state (GalViewEtable *view, ETableState *state)
{
	g_return_if_fail (GAL_IS_VIEW_ETABLE (view));
	g_return_if_fail (E_IS_TABLE_STATE (state));

	if (view->state)
		g_object_unref (view->state);
	view->state = e_table_state_duplicate (state);

	gal_view_changed (GAL_VIEW (view));
}

static void
table_state_changed (ETable *table, GalViewEtable *view)
{
	ETableState *state;

	state = e_table_get_state_object (table);
	g_object_unref (view->state);
	view->state = state;

	gal_view_changed (GAL_VIEW (view));
}

static void
tree_state_changed (ETree *tree, GalViewEtable *view)
{
	ETableState *state;

	state = e_tree_get_state_object (tree);
	g_object_unref (view->state);
	view->state = state;

	gal_view_changed (GAL_VIEW (view));
}

void
gal_view_etable_attach_table (GalViewEtable *view, ETable *table)
{
	g_return_if_fail (GAL_IS_VIEW_ETABLE (view));
	g_return_if_fail (E_IS_TABLE (table));

	gal_view_etable_detach (view);

	view->table = table;

	e_table_set_state_object (view->table, view->state);
	g_object_ref (view->table);
	view->table_state_changed_id =
		g_signal_connect(view->table, "state_change",
				 G_CALLBACK (table_state_changed), view);
}

void
gal_view_etable_attach_tree (GalViewEtable *view, ETree *tree)
{
	g_return_if_fail (GAL_IS_VIEW_ETABLE (view));
	g_return_if_fail (E_IS_TREE (tree));

	gal_view_etable_detach (view);

	view->tree = tree;

	e_tree_set_state_object (view->tree, view->state);
	g_object_ref (view->tree);
	view->tree_state_changed_id =
		g_signal_connect(view->tree, "state_change",
				 G_CALLBACK (tree_state_changed), view);
}

void
gal_view_etable_detach (GalViewEtable *view)
{
	g_return_if_fail (GAL_IS_VIEW_ETABLE (view));

	if (view->table != NULL)
		detach_table (view);
	if (view->tree != NULL)
		detach_tree (view);
}
