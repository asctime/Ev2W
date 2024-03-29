/*
 * e-signature-tree-view.c
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-signature-tree-view.h"

#define E_SIGNATURE_TREE_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SIGNATURE_TREE_VIEW, ESignatureTreeViewPrivate))

enum {
	COLUMN_STRING,
	COLUMN_SIGNATURE
};

enum {
	PROP_0,
	PROP_SELECTED,
	PROP_SIGNATURE_LIST
};

enum {
	REFRESHED,
	LAST_SIGNAL
};

struct _ESignatureTreeViewPrivate {
	ESignatureList *signature_list;
	GHashTable *index;
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (
	ESignatureTreeView,
	e_signature_tree_view,
	GTK_TYPE_TREE_VIEW)

static void
signature_tree_view_refresh_cb (ESignatureList *signature_list,
                                ESignature *unused,
                                ESignatureTreeView *tree_view)
{
	GtkListStore *store;
	GtkTreeModel *model;
	GtkTreeIter tree_iter;
	EIterator *signature_iter;
	ESignature *signature;
	GHashTable *index;
	GList *list = NULL;
	GList *iter;

	store = gtk_list_store_new (2, G_TYPE_STRING, E_TYPE_SIGNATURE);
	model = GTK_TREE_MODEL (store);
	index = tree_view->priv->index;

	g_hash_table_remove_all (index);

	if (signature_list == NULL)
		goto skip;

	/* Build a list of ESignatures to display. */
	signature_iter = e_list_get_iterator (E_LIST (signature_list));
	while (e_iterator_is_valid (signature_iter)) {

		/* XXX EIterator misuses const. */
		signature = (ESignature *) e_iterator_get (signature_iter);
		list = g_list_prepend (list, signature);
		e_iterator_next (signature_iter);
	}
	g_object_unref (signature_iter);

	list = g_list_reverse (list);

	/* Populate the list store and index. */
	for (iter = list; iter != NULL; iter = iter->next) {
		GtkTreeRowReference *reference;
		GtkTreePath *path;
		const gchar *name;

		signature = iter->data;

		/* Skip autogenerated signatures. */
		if (e_signature_get_autogenerated (signature))
			continue;

		name = e_signature_get_name (signature);

		gtk_list_store_append (store, &tree_iter);
		gtk_list_store_set (
			store, &tree_iter,
			COLUMN_STRING, name,
			COLUMN_SIGNATURE, signature, -1);

		path = gtk_tree_model_get_path (model, &tree_iter);
		reference = gtk_tree_row_reference_new (model, path);
		g_hash_table_insert (index, signature, reference);
		gtk_tree_path_free (path);
	}
  /* Gitlab Commit #31babe79 */
  g_list_free (list); 

skip:
	/* Restore the previously selected signature. */
	signature = e_signature_tree_view_get_selected (tree_view);
	if (signature != NULL)
		g_object_ref (signature);
	gtk_tree_view_set_model (GTK_TREE_VIEW (tree_view), model);
	e_signature_tree_view_set_selected (tree_view, signature);
	if (signature != NULL)
		g_object_unref (signature);

	g_signal_emit (tree_view, signals[REFRESHED], 0);
}

static void
signature_tree_view_selection_changed_cb (ESignatureTreeView *tree_view)
{
	g_object_notify (G_OBJECT (tree_view), "selected");
}

static GObject *
signature_tree_view_constructor (GType type,
                                 guint n_construct_properties,
                                 GObjectConstructParam *construct_properties)
{
	GObject *object;
	GtkTreeView *tree_view;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	/* Chain up to parent's constructor() method. */
	object = G_OBJECT_CLASS (
		e_signature_tree_view_parent_class)->constructor (
		type, n_construct_properties, construct_properties);

	tree_view = GTK_TREE_VIEW (object);
	gtk_tree_view_set_headers_visible (tree_view, FALSE);

	column = gtk_tree_view_column_new ();
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_add_attribute (
		column, renderer, "text", COLUMN_STRING);
	gtk_tree_view_append_column (tree_view, column);

	return object;
}

static void
signature_tree_view_set_property (GObject *object,
                                  guint property_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SELECTED:
			e_signature_tree_view_set_selected (
				E_SIGNATURE_TREE_VIEW (object),
				g_value_get_object (value));
			return;

		case PROP_SIGNATURE_LIST:
			e_signature_tree_view_set_signature_list (
				E_SIGNATURE_TREE_VIEW (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
signature_tree_view_get_property (GObject *object,
                                  guint property_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SELECTED:
			g_value_set_object (
				value,
				e_signature_tree_view_get_selected (
				E_SIGNATURE_TREE_VIEW (object)));
			return;

		case PROP_SIGNATURE_LIST:
			g_value_set_object (
				value,
				e_signature_tree_view_get_signature_list (
				E_SIGNATURE_TREE_VIEW (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
signature_tree_view_dispose (GObject *object)
{
	ESignatureTreeViewPrivate *priv;

	priv = E_SIGNATURE_TREE_VIEW_GET_PRIVATE (object);

	if (priv->signature_list != NULL) {
		g_signal_handlers_disconnect_by_func (
			priv->signature_list,
			signature_tree_view_refresh_cb, object);
		g_object_unref (priv->signature_list);
		priv->signature_list = NULL;
	}

	g_hash_table_remove_all (priv->index);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_signature_tree_view_parent_class)->dispose (object);
}

static void
signature_tree_view_finalize (GObject *object)
{
	ESignatureTreeViewPrivate *priv;

	priv = E_SIGNATURE_TREE_VIEW_GET_PRIVATE (object);

	g_hash_table_destroy (priv->index);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_signature_tree_view_parent_class)->finalize (object);
}

static void
e_signature_tree_view_class_init (ESignatureTreeViewClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (ESignatureTreeViewPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructor = signature_tree_view_constructor;
	object_class->set_property = signature_tree_view_set_property;
	object_class->get_property = signature_tree_view_get_property;
	object_class->dispose = signature_tree_view_dispose;
	object_class->finalize = signature_tree_view_finalize;

	g_object_class_install_property (
		object_class,
		PROP_SELECTED,
		g_param_spec_object (
			"selected",
			"Selected Signature",
			NULL,
			E_TYPE_SIGNATURE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SIGNATURE_LIST,
		g_param_spec_object (
			"signature-list",
			"Signature List",
			NULL,
			E_TYPE_SIGNATURE_LIST,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	signals[REFRESHED] = g_signal_new (
		"refreshed",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_signature_tree_view_init (ESignatureTreeView *tree_view)
{
	GHashTable *index;
	GtkTreeSelection *selection;

	/* Reverse-lookup index */
	index = g_hash_table_new_full (
		g_direct_hash, g_direct_equal,
		(GDestroyNotify) g_object_unref,
		(GDestroyNotify) gtk_tree_row_reference_free);

	tree_view->priv = E_SIGNATURE_TREE_VIEW_GET_PRIVATE (tree_view);
	tree_view->priv->index = index;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));

	g_signal_connect_swapped (
		selection, "changed",
		G_CALLBACK (signature_tree_view_selection_changed_cb),
		tree_view);
}

GtkWidget *
e_signature_tree_view_new (void)
{
	return g_object_new (E_TYPE_SIGNATURE_TREE_VIEW, NULL);
}

ESignatureList *
e_signature_tree_view_get_signature_list (ESignatureTreeView *tree_view)
{
	g_return_val_if_fail (E_IS_SIGNATURE_TREE_VIEW (tree_view), NULL);

	return tree_view->priv->signature_list;
}

void
e_signature_tree_view_set_signature_list (ESignatureTreeView *tree_view,
                                          ESignatureList *signature_list)
{
	ESignatureTreeViewPrivate *priv;

	g_return_if_fail (E_IS_SIGNATURE_TREE_VIEW (tree_view));

	if (signature_list != NULL)
		g_return_if_fail (E_IS_SIGNATURE_LIST (signature_list));

	priv = E_SIGNATURE_TREE_VIEW_GET_PRIVATE (tree_view);

	if (priv->signature_list != NULL) {
		g_signal_handlers_disconnect_by_func (
			priv->signature_list,
			signature_tree_view_refresh_cb, tree_view);
		g_object_unref (priv->signature_list);
		priv->signature_list = NULL;
	}

	if (signature_list != NULL) {
		priv->signature_list = g_object_ref (signature_list);

		/* Listen for changes to the signature list. */
		g_signal_connect (
			priv->signature_list, "signature-added",
			G_CALLBACK (signature_tree_view_refresh_cb),
			tree_view);
		g_signal_connect (
			priv->signature_list, "signature-changed",
			G_CALLBACK (signature_tree_view_refresh_cb),
			tree_view);
		g_signal_connect (
			priv->signature_list, "signature-removed",
			G_CALLBACK (signature_tree_view_refresh_cb),
			tree_view);
	}

	signature_tree_view_refresh_cb (signature_list, NULL, tree_view);

	g_object_notify (G_OBJECT (tree_view), "signature-list");
}

ESignature *
e_signature_tree_view_get_selected (ESignatureTreeView *tree_view)
{
	ESignature *signature;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;

	g_return_val_if_fail (E_IS_SIGNATURE_TREE_VIEW (tree_view), NULL);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return NULL;

	gtk_tree_model_get (model, &iter, COLUMN_SIGNATURE, &signature, -1);

	return signature;
}

gboolean
e_signature_tree_view_set_selected (ESignatureTreeView *tree_view,
                                    ESignature *signature)
{
	GtkTreeRowReference *reference;
	GtkTreeSelection *selection;
	GtkTreePath *path;

	g_return_val_if_fail (E_IS_SIGNATURE_TREE_VIEW (tree_view), FALSE);

	if (signature != NULL)
		g_return_val_if_fail (E_IS_SIGNATURE (signature), FALSE);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));

	/* NULL means clear the selection. */
	if (signature == NULL) {
		gtk_tree_selection_unselect_all (selection);
		return TRUE;
	}

	/* Lookup the tree row reference for the signature. */
	reference = g_hash_table_lookup (tree_view->priv->index, signature);
	if (reference == NULL)
		return FALSE;

	/* Select the referenced path. */
	path = gtk_tree_row_reference_get_path (reference);
	gtk_tree_selection_select_path (selection, path);
	gtk_tree_path_free (path);

	g_object_notify (G_OBJECT (tree_view), "selected");

	return TRUE;
}
