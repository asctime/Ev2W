/*
 * e-attachment-store.c
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

#include "e-attachment-store.h"

#include <errno.h>
#include <config.h>
#include <glib/gi18n.h>

#include "e-util/e-util.h"
#include "e-util/e-mktemp.h"
#include "e-util/gconf-bridge.h"

#define E_ATTACHMENT_STORE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_ATTACHMENT_STORE, EAttachmentStorePrivate))

struct _EAttachmentStorePrivate {
	GHashTable *attachment_index;
	gchar *current_folder;

	guint ignore_row_changed : 1;
};

enum {
	PROP_0,
	PROP_CURRENT_FOLDER,
	PROP_NUM_ATTACHMENTS,
	PROP_NUM_LOADING,
	PROP_TOTAL_SIZE
};

G_DEFINE_TYPE (
	EAttachmentStore,
	e_attachment_store,
	GTK_TYPE_LIST_STORE)

static void
attachment_store_set_property (GObject *object,
                               guint property_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CURRENT_FOLDER:
			e_attachment_store_set_current_folder (
				E_ATTACHMENT_STORE (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
attachment_store_get_property (GObject *object,
                               guint property_id,
                               GValue *value,
                               GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CURRENT_FOLDER:
			g_value_set_string (
				value,
				e_attachment_store_get_current_folder (
				E_ATTACHMENT_STORE (object)));
			return;

		case PROP_NUM_ATTACHMENTS:
			g_value_set_uint (
				value,
				e_attachment_store_get_num_attachments (
				E_ATTACHMENT_STORE (object)));
			return;

		case PROP_NUM_LOADING:
			g_value_set_uint (
				value,
				e_attachment_store_get_num_loading (
				E_ATTACHMENT_STORE (object)));
			return;

		case PROP_TOTAL_SIZE:
			g_value_set_uint64 (
				value,
				e_attachment_store_get_total_size (
				E_ATTACHMENT_STORE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
attachment_store_dispose (GObject *object)
{
	EAttachmentStorePrivate *priv;

	priv = E_ATTACHMENT_STORE_GET_PRIVATE (object);

	g_hash_table_remove_all (priv->attachment_index);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_attachment_store_parent_class)->dispose (object);
}

static void
attachment_store_finalize (GObject *object)
{
	EAttachmentStorePrivate *priv;

	priv = E_ATTACHMENT_STORE_GET_PRIVATE (object);

	g_hash_table_destroy (priv->attachment_index);

	g_free (priv->current_folder);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_attachment_store_parent_class)->finalize (object);
}

static void
attachment_store_constructed (GObject *object)
{
	GConfBridge *bridge;
	const gchar *key;

	bridge = gconf_bridge_get ();

	key = "/apps/evolution/shell/file_chooser_folder";
	gconf_bridge_bind_property (bridge, key, object, "current-folder");
}

static void
e_attachment_store_class_init (EAttachmentStoreClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EAttachmentStorePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = attachment_store_set_property;
	object_class->get_property = attachment_store_get_property;
	object_class->dispose = attachment_store_dispose;
	object_class->finalize = attachment_store_finalize;
	object_class->constructed = attachment_store_constructed;

	g_object_class_install_property (
		object_class,
		PROP_CURRENT_FOLDER,
		g_param_spec_string (
			"current-folder",
			"Current Folder",
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_NUM_ATTACHMENTS,
		g_param_spec_uint (
			"num-attachments",
			"Num Attachments",
			NULL,
			0,
			G_MAXUINT,
			0,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_NUM_LOADING,
		g_param_spec_uint (
			"num-loading",
			"Num Loading",
			NULL,
			0,
			G_MAXUINT,
			0,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_TOTAL_SIZE,
		g_param_spec_uint64 (
			"total-size",
			"Total Size",
			NULL,
			0,
			G_MAXUINT64,
			0,
			G_PARAM_READABLE));
}

static void
e_attachment_store_init (EAttachmentStore *store)
{
	GType types[E_ATTACHMENT_STORE_NUM_COLUMNS];
	GHashTable *attachment_index;
	gint column = 0;

	attachment_index = g_hash_table_new_full (
		g_direct_hash, g_direct_equal,
		(GDestroyNotify) g_object_unref,
		(GDestroyNotify) gtk_tree_row_reference_free);

	store->priv = E_ATTACHMENT_STORE_GET_PRIVATE (store);
	store->priv->attachment_index = attachment_index;

	types[column++] = E_TYPE_ATTACHMENT;	/* COLUMN_ATTACHMENT */
	types[column++] = G_TYPE_STRING;	/* COLUMN_CAPTION */
	types[column++] = G_TYPE_STRING;	/* COLUMN_CONTENT_TYPE */
	types[column++] = G_TYPE_STRING;	/* COLUMN_DESCRIPTION */
	types[column++] = G_TYPE_ICON;		/* COLUMN_ICON */
	types[column++] = G_TYPE_BOOLEAN;	/* COLUMN_LOADING */
	types[column++] = G_TYPE_INT;		/* COLUMN_PERCENT */
	types[column++] = G_TYPE_BOOLEAN;	/* COLUMN_SAVING */
	types[column++] = G_TYPE_UINT64;	/* COLUMN_SIZE */

	g_assert (column == E_ATTACHMENT_STORE_NUM_COLUMNS);

	gtk_list_store_set_column_types (
		GTK_LIST_STORE (store), G_N_ELEMENTS (types), types);
}

GtkTreeModel *
e_attachment_store_new (void)
{
	return g_object_new (E_TYPE_ATTACHMENT_STORE, NULL);
}

void
e_attachment_store_add_attachment (EAttachmentStore *store,
                                   EAttachment *attachment)
{
	GtkTreeRowReference *reference;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;

	g_return_if_fail (E_IS_ATTACHMENT_STORE (store));
	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	gtk_list_store_append (GTK_LIST_STORE (store), &iter);

	gtk_list_store_set (
		GTK_LIST_STORE (store), &iter,
		E_ATTACHMENT_STORE_COLUMN_ATTACHMENT, attachment, -1);

	model = GTK_TREE_MODEL (store);
	path = gtk_tree_model_get_path (model, &iter);
	reference = gtk_tree_row_reference_new (model, path);
	gtk_tree_path_free (path);

	g_hash_table_insert (
		store->priv->attachment_index,
		g_object_ref (attachment), reference);

	/* This lets the attachment tell us when to update. */
	e_attachment_set_reference (attachment, reference);

	g_object_freeze_notify (G_OBJECT (store));
	g_object_notify (G_OBJECT (store), "num-attachments");
	g_object_notify (G_OBJECT (store), "total-size");
	g_object_thaw_notify (G_OBJECT (store));
}

gboolean
e_attachment_store_remove_attachment (EAttachmentStore *store,
                                      EAttachment *attachment)
{
	GtkTreeRowReference *reference;
	GHashTable *hash_table;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;

	g_return_val_if_fail (E_IS_ATTACHMENT_STORE (store), FALSE);
	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), FALSE);

	hash_table = store->priv->attachment_index;
	reference = g_hash_table_lookup (hash_table, attachment);

	if (reference == NULL)
		return FALSE;

	if (!gtk_tree_row_reference_valid (reference)) {
		g_hash_table_remove (hash_table, attachment);
		return FALSE;
	}

	e_attachment_cancel (attachment);
	e_attachment_set_reference (attachment, NULL);

	model = gtk_tree_row_reference_get_model (reference);
	path = gtk_tree_row_reference_get_path (reference);
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_path_free (path);

	gtk_list_store_remove (GTK_LIST_STORE (store), &iter);
	g_hash_table_remove (hash_table, attachment);

	g_object_freeze_notify (G_OBJECT (store));
	g_object_notify (G_OBJECT (store), "num-attachments");
	g_object_notify (G_OBJECT (store), "total-size");
	g_object_thaw_notify (G_OBJECT (store));

	return TRUE;
}

void
e_attachment_store_add_to_multipart (EAttachmentStore *store,
                                     CamelMultipart *multipart,
                                     const gchar *default_charset)
{
	GList *list, *iter;

	g_return_if_fail (E_IS_ATTACHMENT_STORE (store));
	g_return_if_fail (CAMEL_MULTIPART (multipart));

	list = e_attachment_store_get_attachments (store);

	for (iter = list; iter != NULL; iter = iter->next) {
		EAttachment *attachment = iter->data;

		/* Skip the attachment if it's still loading. */
		if (!e_attachment_get_loading (attachment))
			e_attachment_add_to_multipart (
				attachment, multipart, default_charset);
	}

	g_list_free_full (list, g_object_unref);
}

GList *
e_attachment_store_get_attachments (EAttachmentStore *store)
{
	GList *list = NULL;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean valid;

	g_return_val_if_fail (E_IS_ATTACHMENT_STORE (store), NULL);

	model = GTK_TREE_MODEL (store);
	valid = gtk_tree_model_get_iter_first (model, &iter);

	while (valid) {
		EAttachment *attachment;
		gint column_id;

		column_id = E_ATTACHMENT_STORE_COLUMN_ATTACHMENT;
		gtk_tree_model_get (model, &iter, column_id, &attachment, -1);

		list = g_list_prepend (list, attachment);

		valid = gtk_tree_model_iter_next (model, &iter);
	}

	return g_list_reverse (list);
}

const gchar *
e_attachment_store_get_current_folder (EAttachmentStore *store)
{
	g_return_val_if_fail (E_IS_ATTACHMENT_STORE (store), NULL);

	return store->priv->current_folder;
}

void
e_attachment_store_set_current_folder (EAttachmentStore *store,
                                       const gchar *current_folder)
{
	g_return_if_fail (E_IS_ATTACHMENT_STORE (store));

	if (current_folder == NULL)
		current_folder = g_get_home_dir ();

	g_free (store->priv->current_folder);
	store->priv->current_folder = g_strdup (current_folder);

	g_object_notify (G_OBJECT (store), "current-folder");
}

guint
e_attachment_store_get_num_attachments (EAttachmentStore *store)
{
	g_return_val_if_fail (E_IS_ATTACHMENT_STORE (store), 0);

	return g_hash_table_size (store->priv->attachment_index);
}

guint
e_attachment_store_get_num_loading (EAttachmentStore *store)
{
	GList *list, *iter;
	guint num_loading = 0;

	g_return_val_if_fail (E_IS_ATTACHMENT_STORE (store), 0);

	list = e_attachment_store_get_attachments (store);

	for (iter = list; iter != NULL; iter = iter->next) {
		EAttachment *attachment = iter->data;

		if (e_attachment_get_loading (attachment))
			num_loading++;
	}

	g_list_free_full (list, g_object_unref);

	return num_loading;
}

goffset
e_attachment_store_get_total_size (EAttachmentStore *store)
{
	GList *list, *iter;
	goffset total_size = 0;

	g_return_val_if_fail (E_IS_ATTACHMENT_STORE (store), 0);

	list = e_attachment_store_get_attachments (store);

	for (iter = list; iter != NULL; iter = iter->next) {
		EAttachment *attachment = iter->data;
		GFileInfo *file_info;

		file_info = e_attachment_get_file_info (attachment);
		if (file_info != NULL)
			total_size += g_file_info_get_size (file_info);
	}

	g_list_free_full (list, g_object_unref);

	return total_size;
}

gint
e_attachment_store_run_file_chooser_dialog (EAttachmentStore *store,
                                            GtkWidget *dialog)
{
	GtkFileChooser *file_chooser;
	gint response = GTK_RESPONSE_NONE;
	const gchar *current_folder;
	gboolean update_folder;

	g_return_val_if_fail (E_IS_ATTACHMENT_STORE (store), response);
	g_return_val_if_fail (GTK_IS_FILE_CHOOSER_DIALOG (dialog), response);

	file_chooser = GTK_FILE_CHOOSER (dialog);
	current_folder = e_attachment_store_get_current_folder (store);
	gtk_file_chooser_set_current_folder (file_chooser, current_folder);

	response = gtk_dialog_run (GTK_DIALOG (dialog));

	update_folder =
		(response == GTK_RESPONSE_ACCEPT) ||
		(response == GTK_RESPONSE_OK) ||
		(response == GTK_RESPONSE_YES) ||
		(response == GTK_RESPONSE_APPLY);

	if (update_folder) {
		gchar *folder;

		folder = gtk_file_chooser_get_current_folder (file_chooser);
		e_attachment_store_set_current_folder (store, folder);
		g_free (folder);
	}

	return response;
}

void
e_attachment_store_run_load_dialog (EAttachmentStore *store,
                                    GtkWindow *parent)
{
	GtkFileChooser *file_chooser;
	GtkWidget *dialog;
	GtkWidget *option;
	GSList *files, *iter;
	const gchar *disposition;
	gboolean active;
	gint response;

	g_return_if_fail (E_IS_ATTACHMENT_STORE (store));
	g_return_if_fail (GTK_IS_WINDOW (parent));

	dialog = gtk_file_chooser_dialog_new (
		_("Add Attachment"), parent,
		GTK_FILE_CHOOSER_ACTION_OPEN,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		_("A_ttach"), GTK_RESPONSE_OK, NULL);

	file_chooser = GTK_FILE_CHOOSER (dialog);
	gtk_file_chooser_set_local_only (file_chooser, FALSE);
	gtk_file_chooser_set_select_multiple (file_chooser, TRUE);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), "mail-attachment");

	option = gtk_check_button_new_with_mnemonic (
		_("_Suggest automatic display of attachment"));
	gtk_file_chooser_set_extra_widget (file_chooser, option);
	gtk_widget_show (option);

	response = e_attachment_store_run_file_chooser_dialog (store, dialog);

	if (response != GTK_RESPONSE_OK)
		goto exit;

	files = gtk_file_chooser_get_files (file_chooser);
	active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (option));
	disposition = active ? "inline" : "attachment";

	for (iter = files; iter != NULL; iter = g_slist_next (iter)) {
		EAttachment *attachment;
		GFile *file = iter->data;

		attachment = e_attachment_new ();
		e_attachment_set_file (attachment, file);
		e_attachment_set_disposition (attachment, disposition);
		e_attachment_store_add_attachment (store, attachment);
		e_attachment_load_async (
			attachment, (GAsyncReadyCallback)
			e_attachment_load_handle_error, parent);
		g_object_unref (attachment);
	}

	g_slist_free_full (files, g_object_unref);

exit:
	gtk_widget_destroy (dialog);
}

GFile *
e_attachment_store_run_save_dialog (EAttachmentStore *store,
                                    GList *attachment_list,
                                    GtkWindow *parent)
{
	GtkFileChooser *file_chooser;
	GtkFileChooserAction action;
	GtkWidget *dialog;
	GFile *destination;
	const gchar *title;
	gint response;
	guint length;

	g_return_val_if_fail (E_IS_ATTACHMENT_STORE (store), NULL);

	length = g_list_length (attachment_list);

	if (length == 0)
		return NULL;

	title = ngettext ("Save Attachment", "Save Attachments", length);

	if (length == 1)
		action = GTK_FILE_CHOOSER_ACTION_SAVE;
	else
		action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;

	dialog = gtk_file_chooser_dialog_new (
		title, parent, action,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_SAVE, GTK_RESPONSE_OK, NULL);

	file_chooser = GTK_FILE_CHOOSER (dialog);
	gtk_file_chooser_set_local_only (file_chooser, FALSE);
	gtk_file_chooser_set_do_overwrite_confirmation (file_chooser, TRUE);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), "mail-attachment");

	if (action == GTK_FILE_CHOOSER_ACTION_SAVE) {
		EAttachment *attachment;
		GFileInfo *file_info;
		const gchar *name = NULL;

		attachment = attachment_list->data;
		file_info = e_attachment_get_file_info (attachment);
		if (file_info != NULL)
			name = g_file_info_get_display_name (file_info);
		if (name == NULL)
			/* Translators: Default attachment filename. */
			name = _("attachment.dat");
		gtk_file_chooser_set_current_name (file_chooser, name);
	}

	response = e_attachment_store_run_file_chooser_dialog (store, dialog);

	if (response == GTK_RESPONSE_OK)
		destination = gtk_file_chooser_get_file (file_chooser);
	else
		destination = NULL;

	gtk_widget_destroy (dialog);

	return destination;
}

/******************** e_attachment_store_get_uris_async() ********************/

typedef struct _UriContext UriContext;

struct _UriContext {
	GSimpleAsyncResult *simple;
	GList *attachment_list;
	GError *error;
	gchar **uris;
	gint index;
};

static UriContext *
attachment_store_uri_context_new (EAttachmentStore *store,
                                  GList *attachment_list,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
	UriContext *uri_context;
	GSimpleAsyncResult *simple;
	guint length;
	gchar **uris;

	simple = g_simple_async_result_new (
		G_OBJECT (store), callback, user_data,
		e_attachment_store_get_uris_async);

	/* Add one for NULL terminator. */
	length = g_list_length (attachment_list) + 1;
	uris = g_malloc0 (sizeof (gchar *) * length);

	uri_context = g_slice_new0 (UriContext);
	uri_context->simple = simple;
	uri_context->attachment_list = g_list_copy (attachment_list);
	uri_context->uris = uris;

	g_list_foreach (
		uri_context->attachment_list,
		(GFunc) g_object_ref, NULL);

	return uri_context;
}

static void
attachment_store_uri_context_free (UriContext *uri_context)
{
	g_object_unref (uri_context->simple);

	/* The attachment list should be empty now. */
	g_warn_if_fail (uri_context->attachment_list == NULL);

	/* So should the error. */
	g_warn_if_fail (uri_context->error == NULL);

	g_strfreev (uri_context->uris);

	g_slice_free (UriContext, uri_context);
}

static void
attachment_store_get_uris_save_cb (EAttachment *attachment,
                                   GAsyncResult *result,
                                   UriContext *uri_context)
{
	GSimpleAsyncResult *simple;
	GFile *file;
	gchar **uris;
	gchar *uri;
	GError *error = NULL;

	file = e_attachment_save_finish (attachment, result, &error);

	/* Remove the attachment from the list. */
	uri_context->attachment_list = g_list_remove (
		uri_context->attachment_list, attachment);
	g_object_unref (attachment);

	if (file != NULL) {
		uri = g_file_get_uri (file);
		uri_context->uris[uri_context->index++] = uri;
		g_object_unref (file);

	} else if (error != NULL) {
		/* If this is the first error, cancel the other jobs. */
		if (uri_context->error == NULL) {
			g_propagate_error (&uri_context->error, error);
			g_list_foreach (
				uri_context->attachment_list,
				(GFunc) e_attachment_cancel, NULL);
			error = NULL;

		/* Otherwise, we can only report back one error.  So if
		 * this is something other than cancellation, dump it to
		 * the terminal. */
		} else if (!g_error_matches (
			error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			g_warning ("%s", error->message);
	}

	if (error != NULL)
		g_error_free (error);

	/* If there's still jobs running, let them finish. */
	if (uri_context->attachment_list != NULL)
		return;

	/* Steal the URI list. */
	uris = uri_context->uris;
	uri_context->uris = NULL;

	/* And the error. */
	error = uri_context->error;
	uri_context->error = NULL;

	simple = uri_context->simple;

	if (error == NULL)
		g_simple_async_result_set_op_res_gpointer (simple, uris, NULL);
	else {
		g_simple_async_result_set_from_error (simple, error);
		g_error_free (error);
	}

	g_simple_async_result_complete (simple);

	attachment_store_uri_context_free (uri_context);
}

void
e_attachment_store_get_uris_async (EAttachmentStore *store,
                                   GList *attachment_list,
                                   GAsyncReadyCallback callback,
                                   gpointer user_data)
{
	GFile *temp_directory;
	UriContext *uri_context;
	GList *iter, *trash = NULL;
	gchar *template;
	gchar *path;

	g_return_if_fail (E_IS_ATTACHMENT_STORE (store));

	uri_context = attachment_store_uri_context_new (
		store, attachment_list, callback, user_data);

	/* Grab the copied attachment list. */
	attachment_list = uri_context->attachment_list;

	/* First scan the list for attachments with a GFile. */
	for (iter = attachment_list; iter != NULL; iter = iter->next) {
		EAttachment *attachment = iter->data;
		GFile *file;
		gchar *uri;

		file = e_attachment_get_file (attachment);
		if (file == NULL)
			continue;

		uri = g_file_get_uri (file);
		uri_context->uris[uri_context->index++] = uri;

		/* Mark the list node for deletion. */
		trash = g_list_prepend (trash, iter);
		g_object_unref (attachment);
	}

	/* Expunge the list. */
	for (iter = trash; iter != NULL; iter = iter->next) {
		GList *link = iter->data;
		attachment_list = g_list_delete_link (attachment_list, link);
	}
	g_list_free (trash);

	uri_context->attachment_list = attachment_list;

	/* If we got them all then we're done. */
	if (attachment_list == NULL) {
		GSimpleAsyncResult *simple;
		gchar **uris;

		/* Steal the URI list. */
		uris = uri_context->uris;
		uri_context->uris = NULL;

		simple = uri_context->simple;
		g_simple_async_result_set_op_res_gpointer (simple, uris, NULL);
		g_simple_async_result_complete (simple);

		attachment_store_uri_context_free (uri_context);
		return;
	}

	/* Any remaining attachments in the list should have MIME parts
	 * only, so we need to save them all to a temporary directory.
	 * We use a directory so the files can retain their basenames.
	 * XXX This could trigger a blocking temp directory cleanup. */
	template = g_strdup_printf (PACKAGE "-%s-XXXXXX", g_get_user_name ());
	path = e_mkdtemp (template);
	g_free (template);

	/* XXX Let's hope errno got set properly. */
	if (path == NULL) {
		GSimpleAsyncResult *simple;

		simple = uri_context->simple;
		g_simple_async_result_set_error (
			simple, G_FILE_ERROR,
			g_file_error_from_errno (errno),
			"%s", g_strerror (errno));
		g_simple_async_result_complete (simple);

		attachment_store_uri_context_free (uri_context);
		return;
	}

	temp_directory = g_file_new_for_path (path);

	for (iter = attachment_list; iter != NULL; iter = iter->next)
		e_attachment_save_async (
			E_ATTACHMENT (iter->data),
			temp_directory, (GAsyncReadyCallback)
			attachment_store_get_uris_save_cb,
			uri_context);

	g_object_unref (temp_directory);
	g_free (path);
}

gchar **
e_attachment_store_get_uris_finish (EAttachmentStore *store,
                                    GAsyncResult *result,
                                    GError **error)
{
	GSimpleAsyncResult *simple;
	gchar **uris;

	g_return_val_if_fail (E_IS_ATTACHMENT_STORE (store), NULL);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (result), NULL);

	simple = G_SIMPLE_ASYNC_RESULT (result);
	uris = g_simple_async_result_get_op_res_gpointer (simple);
	g_simple_async_result_propagate_error (simple, error);

	return uris;
}

/********************** e_attachment_store_load_async() **********************/

typedef struct _LoadContext LoadContext;

struct _LoadContext {
	GSimpleAsyncResult *simple;
	GList *attachment_list;
	GError *error;
};

static LoadContext *
attachment_store_load_context_new (EAttachmentStore *store,
                                   GList *attachment_list,
                                   GAsyncReadyCallback callback,
                                   gpointer user_data)
{
	LoadContext *load_context;
	GSimpleAsyncResult *simple;

	simple = g_simple_async_result_new (
		G_OBJECT (store), callback, user_data,
		e_attachment_store_load_async);

	load_context = g_slice_new0 (LoadContext);
	load_context->simple = simple;
	load_context->attachment_list = g_list_copy (attachment_list);

	g_list_foreach (
		load_context->attachment_list,
		(GFunc) g_object_ref, NULL);

	return load_context;
}

static void
attachment_store_load_context_free (LoadContext *load_context)
{
	g_object_unref (load_context->simple);

	/* The attachment list should be empty now. */
	g_warn_if_fail (load_context->attachment_list == NULL);

	/* So should the error. */
	g_warn_if_fail (load_context->error == NULL);

	g_slice_free (LoadContext, load_context);
}

static void
attachment_store_load_ready_cb (EAttachment *attachment,
                                GAsyncResult *result,
                                LoadContext *load_context)
{
	GSimpleAsyncResult *simple;
	GError *error = NULL;

	e_attachment_load_finish (attachment, result, &error);

	/* Remove the attachment from the list. */
	load_context->attachment_list = g_list_remove (
		load_context->attachment_list, attachment);
	g_object_unref (attachment);

	if (error != NULL) {
		/* If this is the first error, cancel the other jobs. */
		if (load_context->error == NULL) {
			g_propagate_error (&load_context->error, error);
			g_list_foreach (
				load_context->attachment_list,
				(GFunc) e_attachment_cancel, NULL);
			error = NULL;

		/* Otherwise, we can only report back one error.  So if
		 * this is something other than cancellation, dump it to
		 * the terminal. */
		} else if (!g_error_matches (
			error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			g_warning ("%s", error->message);
	}

	if (error != NULL)
		g_error_free (error);

	/* If there's still jobs running, let them finish. */
	if (load_context->attachment_list != NULL)
		return;

	/* Steal the error. */
	error = load_context->error;
	load_context->error = NULL;

	simple = load_context->simple;

	if (error == NULL)
		g_simple_async_result_set_op_res_gboolean (simple, TRUE);
	else {
		g_simple_async_result_set_from_error (simple, error);
		g_error_free (error);
	}

	g_simple_async_result_complete (simple);

	attachment_store_load_context_free (load_context);
}

void
e_attachment_store_load_async (EAttachmentStore *store,
                               GList *attachment_list,
                               GAsyncReadyCallback callback,
                               gpointer user_data)
{
	LoadContext *load_context;
	GList *iter;

	g_return_if_fail (E_IS_ATTACHMENT_STORE (store));

	load_context = attachment_store_load_context_new (
		store, attachment_list, callback, user_data);

	if (attachment_list == NULL) {
		GSimpleAsyncResult *simple;

		simple = load_context->simple;
		g_simple_async_result_set_op_res_gboolean (simple, TRUE);
		g_simple_async_result_complete (simple);

		attachment_store_load_context_free (load_context);
		return;
	}

	for (iter = attachment_list; iter != NULL; iter = iter->next) {
		EAttachment *attachment = E_ATTACHMENT (iter->data);

		e_attachment_store_add_attachment (store, attachment);

		e_attachment_load_async (
			attachment, (GAsyncReadyCallback)
			attachment_store_load_ready_cb,
			load_context);
	}
}

gboolean
e_attachment_store_load_finish (EAttachmentStore *store,
                                GAsyncResult *result,
                                GError **error)
{
	GSimpleAsyncResult *simple;
	gboolean success;

	g_return_val_if_fail (E_IS_ATTACHMENT_STORE (store), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (result), FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (result);
	success = g_simple_async_result_get_op_res_gboolean (simple);
	g_simple_async_result_propagate_error (simple, error);

	return success;
}

/********************** e_attachment_store_save_async() **********************/

typedef struct _SaveContext SaveContext;

struct _SaveContext {
	GSimpleAsyncResult *simple;
	GFile *destination;
	GFile *fresh_directory;
	GFile *trash_directory;
	GList *attachment_list;
	GError *error;
	gchar **uris;
	gint index;
};

static SaveContext *
attachment_store_save_context_new (EAttachmentStore *store,
                                   GFile *destination,
                                   GAsyncReadyCallback callback,
                                   gpointer user_data)
{
	SaveContext *save_context;
	GSimpleAsyncResult *simple;
	GList *attachment_list;
	guint length;
	gchar **uris;

	simple = g_simple_async_result_new (
		G_OBJECT (store), callback, user_data,
		e_attachment_store_save_async);

	attachment_list = e_attachment_store_get_attachments (store);

	/* Add one for NULL terminator. */
	length = g_list_length (attachment_list) + 1;
	uris = g_malloc0 (sizeof (gchar *) * length);

	save_context = g_slice_new0 (SaveContext);
	save_context->simple = simple;
	save_context->destination = g_object_ref (destination);
	save_context->attachment_list = attachment_list;
	save_context->uris = uris;

	return save_context;
}

static void
attachment_store_save_context_free (SaveContext *save_context)
{
	g_object_unref (save_context->simple);

	/* The attachment list should be empty now. */
	g_warn_if_fail (save_context->attachment_list == NULL);

	/* So should the error. */
	g_warn_if_fail (save_context->error == NULL);

	if (save_context->destination) {
		g_object_unref (save_context->destination);
		save_context->destination = NULL;
	}

	if (save_context->fresh_directory) {
		g_object_unref (save_context->fresh_directory);
		save_context->fresh_directory = NULL;
	}

	if (save_context->trash_directory) {
		g_object_unref (save_context->trash_directory);
		save_context->trash_directory = NULL;
	}

	g_strfreev (save_context->uris);

	g_slice_free (SaveContext, save_context);
}

static void
attachment_store_save_cb (EAttachment *attachment,
                          GAsyncResult *result,
                          SaveContext *save_context)
{
	GSimpleAsyncResult *simple;
	GFile *file;
	gchar **uris;
	gchar *template;
	gchar *path;
	GError *error = NULL;

	file = e_attachment_save_finish (attachment, result, &error);

	/* Remove the attachment from the list. */
	save_context->attachment_list = g_list_remove (
		save_context->attachment_list, attachment);
	g_object_unref (attachment);

	if (file != NULL) {
		/* Assemble the file's final URI from its basename. */
		gchar *basename;
		gchar *uri;

		basename = g_file_get_basename (file);
		g_object_unref (file);

		file = save_context->destination;
		file = g_file_get_child (file, basename);
		uri = g_file_get_uri (file);
		g_object_unref (file);

		save_context->uris[save_context->index++] = uri;

	} else if (error != NULL) {
		/* If this is the first error, cancel the other jobs. */
		if (save_context->error == NULL) {
			g_propagate_error (&save_context->error, error);
			g_list_foreach (
				save_context->attachment_list,
				(GFunc) e_attachment_cancel, NULL);
			error = NULL;

		/* Otherwise, we can only report back one error.  So if
		 * this is something other than cancellation, dump it to
		 * the terminal. */
		} else if (!g_error_matches (
			error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			g_warning ("%s", error->message);
	}

	if (error != NULL)
		g_error_free (error);

	/* If there's still jobs running, let them finish. */
	if (save_context->attachment_list != NULL)
		return;

	/* If an error occurred while saving, we're done. */
	if (save_context->error != NULL) {

		/* Steal the error. */
		error = save_context->error;
		save_context->error = NULL;

		simple = save_context->simple;
		g_simple_async_result_set_from_error (simple, error);
		g_simple_async_result_complete (simple);

		attachment_store_save_context_free (save_context);
		g_error_free (error);
		return;
	}

	/* Attachments are all saved to a temporary directory.
	 * Now we need to move the existing destination directory
	 * out of the way (if it exists).  Instead of testing for
	 * existence we'll just attempt the move and ignore any
	 * G_IO_ERROR_NOT_FOUND errors. */

	/* First, however, we need another temporary directory to
	 * move the existing destination directory to.  Note we're
	 * not actually creating the directory yet, just picking a
	 * name for it.  The usual raciness with this approach
	 * applies here (read up on mktemp(3)), but worst case is
	 * we get a spurious G_IO_ERROR_WOULD_MERGE error and the
	 * user has to try saving attachments again. */
	template = g_strdup_printf (PACKAGE "-%s-XXXXXX", g_get_user_name ());
	path = e_mktemp (template);
	g_free (template);

	save_context->trash_directory = g_file_new_for_path (path);
	g_free (path);

	/* XXX No asynchronous move operation in GIO? */
	g_file_move (
		save_context->destination,
		save_context->trash_directory,
		G_FILE_COPY_NONE, NULL, NULL, NULL, &error);

	if (error != NULL && !g_error_matches (
		error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND)) {

		simple = save_context->simple;
		g_simple_async_result_set_from_error (simple, error);
		g_simple_async_result_complete (simple);

		attachment_store_save_context_free (save_context);
		g_error_free (error);
		return;
	}

	g_clear_error (&error);

	/* Now we can move the first temporary directory containing
	 * the newly saved files to the user-specified destination. */
	g_file_move (
		save_context->fresh_directory,
		save_context->destination,
		G_FILE_COPY_NONE, NULL, NULL, NULL, &error);

	if (error != NULL) {
		simple = save_context->simple;
		g_simple_async_result_set_from_error (simple, error);
		g_simple_async_result_complete (simple);

		attachment_store_save_context_free (save_context);
		g_error_free (error);
		return;
	}

	/* And the URI list. */
	uris = save_context->uris;
	save_context->uris = NULL;

	simple = save_context->simple;
	g_simple_async_result_set_op_res_gpointer (simple, uris, NULL);
	g_simple_async_result_complete (simple);

	attachment_store_save_context_free (save_context);
}

void
e_attachment_store_save_async (EAttachmentStore *store,
                               GFile *destination,
                               GAsyncReadyCallback callback,
                               gpointer user_data)
{
	SaveContext *save_context;
	GList *attachment_list, *iter;
	GFile *temp_directory;
	gchar *template;
	gchar *path;

	g_return_if_fail (E_IS_ATTACHMENT_STORE (store));
	g_return_if_fail (G_IS_FILE (destination));

	save_context = attachment_store_save_context_new (
		store, destination, callback, user_data);

	attachment_list = save_context->attachment_list;

	/* Deal with an empty attachment store.  The caller will get
	 * an empty NULL-terminated list as opposed to NULL, to help
	 * distinguish it from an error. */
	if (attachment_list == NULL) {
		GSimpleAsyncResult *simple;
		gchar **uris;

		/* Steal the URI list. */
		uris = save_context->uris;
		save_context->uris = NULL;

		simple = save_context->simple;
		g_simple_async_result_set_op_res_gpointer (simple, uris, NULL);
		g_simple_async_result_complete (simple);

		attachment_store_save_context_free (save_context);
		return;
	}

	/* Save all attachments to a temporary directory, which we'll
	 * then move to its proper location.  We use a directory so
	 * files can retain their basenames.
	 * XXX This could trigger a blocking temp directory cleanup. */
	template = g_strdup_printf (PACKAGE "-%s-XXXXXX", g_get_user_name ());
	path = e_mkdtemp (template);
	g_free (template);

	/* XXX Let's hope errno got set properly. */
	if (path == NULL) {
		GSimpleAsyncResult *simple;

		simple = save_context->simple;
		g_simple_async_result_set_error (
			simple, G_FILE_ERROR,
			g_file_error_from_errno (errno),
			"%s", g_strerror (errno));
		g_simple_async_result_complete (simple);

		attachment_store_save_context_free (save_context);
		return;
	}

	temp_directory = g_file_new_for_path (path);
	save_context->fresh_directory = temp_directory;
	g_free (path);

	for (iter = attachment_list; iter != NULL; iter = iter->next)
		e_attachment_save_async (
			E_ATTACHMENT (iter->data),
			temp_directory, (GAsyncReadyCallback)
			attachment_store_save_cb, save_context);
}

gchar **
e_attachment_store_save_finish (EAttachmentStore *store,
                                GAsyncResult *result,
                                GError **error)
{
	GSimpleAsyncResult *simple;
	gchar **uris;

	g_return_val_if_fail (E_IS_ATTACHMENT_STORE (store), NULL);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (result), NULL);

	simple = G_SIMPLE_ASYNC_RESULT (result);
	uris = g_simple_async_result_get_op_res_gpointer (simple);
	g_simple_async_result_propagate_error (simple, error);

	return uris;
}
