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
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <string.h>
#include <glib/gi18n.h>
#include <e-util/e-util.h>

#include "em-folder-tree.h"
#include "em-folder-selector.h"
#include "em-folder-utils.h"

#define d(x)

extern CamelSession *session;
static gpointer parent_class;

static void
folder_selector_finalize (GObject *object)
{
	EMFolderSelector *emfs = EM_FOLDER_SELECTOR (object);

	g_free (emfs->selected_path);
	g_free (emfs->selected_uri);
	g_free (emfs->created_uri);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
folder_selector_destroy (GtkObject *object)
{
	EMFolderSelector *emfs = EM_FOLDER_SELECTOR (object);
	GtkTreeModel *model;

	if (emfs->created_id != 0) {
		model = gtk_tree_view_get_model (GTK_TREE_VIEW (emfs->emft));
		g_signal_handler_disconnect (model, emfs->created_id);
		emfs->created_id = 0;
	}

	/* Chain up to parent's destroy() method. */
	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
folder_selector_class_init (EMFolderSelectorClass *class)
{
	GObjectClass *object_class;
	GtkObjectClass *gtk_object_class;

	parent_class = g_type_class_peek_parent (class);

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = folder_selector_finalize;

	gtk_object_class = GTK_OBJECT_CLASS (class);
	gtk_object_class->destroy = folder_selector_destroy;
}

static void
folder_selector_init (EMFolderSelector *emfs)
{
	emfs->selected_path = NULL;
	emfs->selected_uri = NULL;
}

GType
em_folder_selector_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EMFolderSelectorClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) folder_selector_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EMFolderSelector),
			0,     /* n_preallocs */
			(GInstanceInitFunc) folder_selector_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			GTK_TYPE_DIALOG, "EMFolderSelector", &type_info, 0);
	}

	return type;
}

static void
emfs_response (GtkWidget *dialog, gint response, EMFolderSelector *emfs)
{
	if (response != EM_FOLDER_SELECTOR_RESPONSE_NEW)
		return;

	g_object_set_data ((GObject *)emfs->emft, "select", GUINT_TO_POINTER (1));

	em_folder_utils_create_folder (NULL, emfs->emft, GTK_WINDOW (dialog));

	g_signal_stop_emission_by_name (emfs, "response");
}

static void
emfs_create_name_changed (GtkEntry *entry, EMFolderSelector *emfs)
{
	gchar *path;
	const gchar *text = NULL;
	gboolean active;

	if (gtk_entry_get_text_length (emfs->name_entry) > 0)
		text = gtk_entry_get_text (emfs->name_entry);

	path = em_folder_tree_get_selected_uri(emfs->emft);
	active = text && path && !strchr (text, '/');
	g_free(path);

	gtk_dialog_set_response_sensitive ((GtkDialog *) emfs, GTK_RESPONSE_OK, active);
}

static void
folder_selected_cb (EMFolderTree *emft, const gchar *path, const gchar *uri, guint32 flags, EMFolderSelector *emfs)
{
	if (emfs->name_entry)
		emfs_create_name_changed (emfs->name_entry, emfs);
	else
		gtk_dialog_set_response_sensitive (GTK_DIALOG (emfs), GTK_RESPONSE_OK, TRUE);
}

static void
folder_activated_cb (EMFolderTree *emft, const gchar *path, const gchar *uri, EMFolderSelector *emfs)
{
	gtk_dialog_response ((GtkDialog *) emfs, GTK_RESPONSE_OK);
}

void
em_folder_selector_construct (EMFolderSelector *emfs, EMFolderTree *emft, guint32 flags, const gchar *title, const gchar *text, const gchar *oklabel)
{
	GtkWidget *container;
	GtkWidget *widget;

	gtk_window_set_default_size (GTK_WINDOW (emfs), 350, 300);
	gtk_window_set_title (GTK_WINDOW (emfs), title);
	gtk_container_set_border_width (GTK_CONTAINER (emfs), 6);

	container = gtk_dialog_get_content_area (GTK_DIALOG (emfs));
	gtk_box_set_spacing (GTK_BOX (container), 6);
	gtk_container_set_border_width (GTK_CONTAINER (container), 6);

	emfs->flags = flags;
	if (flags & EM_FOLDER_SELECTOR_CAN_CREATE) {
		gtk_dialog_add_button (GTK_DIALOG (emfs), GTK_STOCK_NEW, EM_FOLDER_SELECTOR_RESPONSE_NEW);
		g_signal_connect (emfs, "response", G_CALLBACK (emfs_response), emfs);
	}

	gtk_dialog_add_buttons (GTK_DIALOG (emfs), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				oklabel?oklabel:GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);

	gtk_dialog_set_response_sensitive (GTK_DIALOG (emfs), GTK_RESPONSE_OK, FALSE);
	gtk_dialog_set_default_response (GTK_DIALOG (emfs), GTK_RESPONSE_OK);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_box_pack_end (GTK_BOX (container), widget, TRUE, TRUE, 6);
	gtk_widget_show (widget);

	emfs->emft = emft;
	gtk_container_add (GTK_CONTAINER (widget), GTK_WIDGET (emft));
	gtk_widget_show (GTK_WIDGET (emft));

	g_signal_connect (emfs->emft, "folder-selected", G_CALLBACK (folder_selected_cb), emfs);
	g_signal_connect (emfs->emft, "folder-activated", G_CALLBACK (folder_activated_cb), emfs);

	if (text != NULL) {
		widget = gtk_label_new (text);
		gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_LEFT);
		gtk_widget_show (widget);

		gtk_box_pack_end (GTK_BOX (container), widget, FALSE, TRUE, 6);
	}

	gtk_widget_grab_focus ((GtkWidget *) emfs->emft);
}

GtkWidget *
em_folder_selector_new (GtkWindow *parent,
                        EMFolderTree *emft,
                        guint32 flags,
                        const gchar *title,
                        const gchar *text,
                        const gchar *oklabel)
{
	EMFolderSelector *emfs;

	emfs = g_object_new (
		EM_TYPE_FOLDER_SELECTOR,
		"transient-for", parent, NULL);
	em_folder_selector_construct (emfs, emft, flags, title, text, oklabel);

	return (GtkWidget *) emfs;
}

static void
emfs_create_name_activate (GtkEntry *entry, EMFolderSelector *emfs)
{
	if (gtk_entry_get_text_length (emfs->name_entry) > 0) {
		gchar *path;
		const gchar *text;

		text = gtk_entry_get_text (emfs->name_entry);
		path = em_folder_tree_get_selected_uri(emfs->emft);

		if (text && path && !strchr (text, '/'))
			g_signal_emit_by_name (emfs, "response", GTK_RESPONSE_OK);
		g_free(path);
	}
}

GtkWidget *
em_folder_selector_create_new (GtkWindow *parent,
                               EMFolderTree *emft,
                               guint32 flags,
                               const gchar *title,
                               const gchar *text)
{
	EMFolderSelector *emfs;
	GtkWidget *hbox, *w;
	GtkWidget *container;

	/* remove the CREATE flag if it is there since that's the
	 * whole purpose of this dialog */
	flags &= ~EM_FOLDER_SELECTOR_CAN_CREATE;

	emfs = g_object_new (
		EM_TYPE_FOLDER_SELECTOR,
		"transient-for", parent, NULL);
	em_folder_selector_construct (emfs, emft, flags, title, text, _("C_reate"));
	em_folder_tree_set_excluded(emft, EMFT_EXCLUDE_NOINFERIORS);

	hbox = gtk_hbox_new (FALSE, 0);
	w = gtk_label_new_with_mnemonic (_("Folder _name:"));
	gtk_box_pack_start ((GtkBox *) hbox, w, FALSE, FALSE, 6);
	emfs->name_entry = (GtkEntry *) gtk_entry_new ();
	gtk_label_set_mnemonic_widget (GTK_LABEL (w), (GtkWidget *) emfs->name_entry);
	g_signal_connect (emfs->name_entry, "changed", G_CALLBACK (emfs_create_name_changed), emfs);
	g_signal_connect (emfs->name_entry, "activate", G_CALLBACK (emfs_create_name_activate), emfs);
	gtk_box_pack_start ((GtkBox *) hbox, (GtkWidget *) emfs->name_entry, TRUE, FALSE, 6);
	gtk_widget_show_all (hbox);

	container = gtk_dialog_get_content_area (GTK_DIALOG (emfs));
	gtk_box_pack_start (GTK_BOX (container), hbox, FALSE, TRUE, 0);

	gtk_widget_grab_focus ((GtkWidget *) emfs->name_entry);

	return (GtkWidget *) emfs;
}

void
em_folder_selector_set_selected (EMFolderSelector *emfs, const gchar *uri)
{
	em_folder_tree_set_selected (emfs->emft, uri, FALSE);
}

void
em_folder_selector_set_selected_list (EMFolderSelector *emfs, GList *list)
{
	em_folder_tree_set_selected_list (emfs->emft, list, FALSE);
}

const gchar *
em_folder_selector_get_selected_uri (EMFolderSelector *emfs)
{
	gchar *uri;
	const gchar *name;

	if (!(uri = em_folder_tree_get_selected_uri (emfs->emft))) {
		d(printf ("no selected folder?\n"));
		return NULL;
	}

	if (uri && emfs->name_entry) {
		CamelProvider *provider;
		CamelURL *url;
		gchar *newpath;

		provider = camel_provider_get(uri, NULL);

		name = gtk_entry_get_text (emfs->name_entry);

		url = camel_url_new (uri, NULL);
		if (provider && (provider->url_flags & CAMEL_URL_FRAGMENT_IS_PATH)) {
			if (url->fragment)
				newpath = g_strdup_printf ("%s/%s", url->fragment, name);
			else
				newpath = g_strdup (name);

			camel_url_set_fragment (url, newpath);
		} else {
			gchar *path;

			path = g_strdup_printf("%s/%s", (url->path == NULL || strcmp(url->path, "/") == 0) ? "":url->path, name);
			camel_url_set_path(url, path);
			if (path[0] == '/') {
				newpath = g_strdup(path+1);
				g_free(path);
			} else
				newpath = path;
		}

		g_free (emfs->selected_path);
		emfs->selected_path = newpath;

		g_free (emfs->selected_uri);
		emfs->selected_uri = camel_url_to_string (url, 0);

		camel_url_free (url);
		uri = emfs->selected_uri;
	}

	return uri;
}

GList *
em_folder_selector_get_selected_uris (EMFolderSelector *emfs)
{
	return em_folder_tree_get_selected_uris (emfs->emft);
}

GList *
em_folder_selector_get_selected_paths (EMFolderSelector *emfs)
{
	return em_folder_tree_get_selected_paths (emfs->emft);
}

const gchar *
em_folder_selector_get_selected_path (EMFolderSelector *emfs)
{
	gchar *uri, *path;

	if (emfs->selected_path) {
		/* already did the work in a previous call */
		return emfs->selected_path;
	}

	if ((uri = em_folder_tree_get_selected_uri(emfs->emft)) == NULL) {
		d(printf ("no selected folder?\n"));
		return NULL;
	}
	g_free(uri);

	path = em_folder_tree_get_selected_path(emfs->emft);
	if (emfs->name_entry) {
		const gchar *name;
		gchar *newpath;

		name = gtk_entry_get_text (emfs->name_entry);
		newpath = g_strdup_printf ("%s/%s", path?path:"", name);

		g_free(path);
		emfs->selected_path = g_strdup (newpath);
	} else {
		g_free(emfs->selected_path);
		emfs->selected_path = path?path:g_strdup("");
	}

	return emfs->selected_path;
}
