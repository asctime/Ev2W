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
 *		Vivek Jain <jvivek@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include "share-folder.h"
#include <glib/gi18n-lib.h>
#include <libedataserverui/e-contact-store.h>
#include <e-util/e-util.h>
#include <e-util/e-alert-dialog.h>
#include <e-util/e-util-private.h>
#include <e-gw-container.h>
#include <e-gw-connection.h>
#define ROOTNODE "vboxSharing"
#define NROOTNODE "vbox191"
#define d(x)

struct _SharedUser {
	EShUsers *user_node;
	gint flag;
};
typedef struct _SharedUser SharedUser;

static void share_folder_class_init (ShareFolderClass *class);
static void share_folder_init       (ShareFolder *sf);
static void share_folder_destroy    (GtkObject *obj);
static void share_folder_finalise   (GObject *obj);
static void free_user_node(EShUsers *user);
static void free_node(SharedUser *user);
static void free_all(ShareFolder *sf);
static SharedUser * find_node(GList *list, gchar *email);
static void get_container_list (ShareFolder *sf);
static void user_selected(GtkTreeSelection *selection, ShareFolder *sf);
static void not_shared_clicked (GtkRadioButton *button, ShareFolder *sf);
static void shared_clicked (GtkRadioButton *button, ShareFolder *sf);
static void add_clicked(GtkButton *button, ShareFolder *sf);
static void remove_clicked(GtkButton *button, ShareFolder *sf);
static void not_ok_clicked(GtkButton *button, ShareFolder *sf);
static void not_cancel_clicked(GtkButton *button, GtkWidget *window);
static void share_folder_construct (ShareFolder *sf);
GType share_folder_get_type (void);

static GtkVBoxClass *parent_class = NULL;

GType
share_folder_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (ShareFolderClass),
			NULL, NULL,
			(GClassInitFunc) share_folder_class_init,
			NULL, NULL,
			sizeof (ShareFolder),
			0,
			(GInstanceInitFunc) share_folder_init
		};

		type = g_type_register_static (gtk_vbox_get_type (), "ShareFolder", &info, 0);
	}

	return type;
}

static void
share_folder_class_init (ShareFolderClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);

	parent_class = g_type_class_ref (gtk_vbox_get_type ());
	object_class->destroy = share_folder_destroy;
	gobject_class->finalize = share_folder_finalise;
}

static void
share_folder_finalise (GObject *obj)
{
	ShareFolder *sf = (ShareFolder *) obj;
	g_object_unref (sf->builder);
	free_all(sf);
	G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
share_folder_destroy (GtkObject *obj)
{

	ShareFolder *sf = (ShareFolder *) obj;
	free_all (sf);
	GTK_OBJECT_CLASS (parent_class)->destroy (obj);
}

static void
share_folder_init (ShareFolder *sf)
{
	sf->gcontainer = NULL;
	sf->users = 0;
	sf->flag_for_ok = 0;
	sf->shared = FALSE;
	sf->container_list = NULL;
	sf->users_list = NULL;
	sf->sub = "Shared Folder notification";
	sf->message = NULL;
}

static void
free_user_node(EShUsers *user)
{
	if (user) {
		g_free(user->email);
		user->email = NULL;
	}
	return;
}

static void
free_node(SharedUser *usr)
{
	EShUsers *user = usr->user_node;
	if (user) {
		g_free(user->email);
		user->email = NULL;
	}
	return;
}

static SharedUser *
find_node(GList *list, gchar *email)
{
	SharedUser *user = NULL;
	EShUsers *usr = NULL;
	GList *tmp;
	gint i;

	if (list) {
		tmp = g_list_first(list);
		for (i=0; tmp; i++)
		{
			user = tmp->data;
			usr = user->user_node;
			if (!g_ascii_strcasecmp(usr->email, email)) {
				return user; /*if found, it returns that user*/
			}
			tmp= g_list_next(tmp);
		}
	}
	return NULL;
}

static void
free_all (ShareFolder *sf)
{
	if (sf->users_list) {
		g_list_foreach (sf->users_list,(GFunc) free_node, NULL);
		g_list_free (sf->users_list);
		sf->users_list = NULL;
	}

	/* e_gw_connection_free_container_list (sf->container_list); */
}

static void
display_container (EGwContainer *container , ShareFolder *sf)
{
	gboolean byme = FALSE;
	gboolean tome = FALSE;
	gchar *email = NULL;
	gchar *msg;
	GList *user_list = NULL;
	EShUsers *user = NULL;

	sf->gcontainer = container;
	if (!(byme = e_gw_container_get_is_shared_by_me(container)))
		tome = e_gw_container_get_is_shared_to_me(container);
	if (byme || tome) {
		e_gw_container_get_user_list (sf->gcontainer, &user_list);
		sf->users = g_list_length (user_list);
		if (sf->users != 0) {
			sf->is_shared = TRUE;
			gtk_toggle_button_set_active((GtkToggleButton *) sf->shared, TRUE);
			shared_clicked(sf->shared , sf);
			if (tome) {
				g_print ("Entered: inside too\n");

				gtk_widget_set_sensitive (GTK_WIDGET (sf->not_shared), FALSE);
				gtk_widget_set_sensitive (GTK_WIDGET (sf->add_button), FALSE);
				gtk_widget_set_sensitive (GTK_WIDGET (sf->remove), FALSE);
				gtk_widget_set_sensitive (GTK_WIDGET (sf->add_book), FALSE);
				gtk_widget_set_sensitive (GTK_WIDGET (sf->notification), FALSE);
				gtk_widget_set_sensitive (GTK_WIDGET (sf->user_list), FALSE);
				email = g_strdup (e_gw_container_get_owner (sf->gcontainer));
				msg = g_strconcat (email, "  (Owner)", NULL);
				gtk_list_store_append (GTK_LIST_STORE (sf->model), &(sf->iter));
				gtk_list_store_set (GTK_LIST_STORE (sf->model), &(sf->iter), 0, msg, -1);
				g_free (msg);
				g_free (email);

			} else
				gtk_widget_set_sensitive (GTK_WIDGET (sf->table), TRUE);
			/* I populate the list and set flags to 0 for the existing users*/
			while (user_list) {
				SharedUser *shared_user = g_new0 (SharedUser , 1);
				gboolean add, edit, delete;
				add = edit = delete = FALSE;
				user = user_list->data;
				shared_user->user_node = user;
				shared_user->flag = 0;
				email = g_strdup (user->email);
				if (user->rights & 0x1)
					add = TRUE;
				if (user->rights & 0x2)
					edit = TRUE;
				if (user->rights & 0x4)
					delete = TRUE;

				msg = g_strdup_printf ("%s", email);
				gtk_list_store_append (GTK_LIST_STORE (sf->model), &(sf->iter));
				gtk_list_store_set (GTK_LIST_STORE (sf->model), &(sf->iter), 0, msg, 1, add, 2, edit, 3, delete, -1);
				sf->users_list = g_list_append (sf->users_list, shared_user);
				g_free (msg);
				g_free (email);
				msg = NULL;
				email = NULL;
				user_list = user_list->next;
			}
			/* i also need to display status*/
		} else {

			gtk_toggle_button_set_active ((GtkToggleButton *) sf->not_shared,  TRUE);
			not_shared_clicked (sf->not_shared , sf);
		}
	}

}

static void
get_container_list (ShareFolder *sf)
{
	sf->container_list = NULL;
	if (E_IS_GW_CONNECTION (sf->cnc)) {
		/* get list of containers */
		if (e_gw_connection_get_container_list (sf->cnc, "folders", &(sf->container_list)) == E_GW_CONNECTION_STATUS_OK) {
			GList *container = NULL;

			for (container = sf->container_list; container != NULL; container = container->next) {
				gchar **tail;
				gchar *id_shared = NULL;
				gchar *id_unshared = NULL;

				id_shared = g_strdup(e_gw_container_get_id (container->data));

				/* this has to be done since id changes after the folder is shared*/
				if ( g_str_has_suffix (id_shared, "35")) {
					tail = g_strsplit(id_shared, "@", 2);
					id_unshared = g_strconcat(tail[0], "@", "13", NULL);
					g_strfreev(tail);
				}

				if ((id_shared && !g_ascii_strcasecmp(id_shared, sf->container_id)) ||(id_unshared && !g_ascii_strcasecmp(id_unshared, sf->container_id))) {

					display_container (E_GW_CONTAINER (container->data), sf);
					g_free (id_shared);
					g_free (id_unshared);
					break;
				}

				g_free (id_shared);
				g_free (id_unshared);
			}

		} else
			g_warning("Could not get the Container List");
	}
}

static void
shared_clicked (GtkRadioButton *button, ShareFolder *sf)
{

	gtk_widget_set_sensitive (GTK_WIDGET (sf->table) ,TRUE);
	sf->flag_for_ok = 0;
}

static void
not_shared_clicked (GtkRadioButton *button, ShareFolder *sf)
{
	if (!sf->is_shared) {
		sf->flag_for_ok = 0;
	} else {
		sf->flag_for_ok = 2;
	}
	gtk_widget_set_sensitive (GTK_WIDGET (sf->table), FALSE);

}

static void
add_clicked(GtkButton *button, ShareFolder *sf)
{
	const gchar *email = NULL;
	const gchar *self_email = NULL;
	SharedUser *new_user = NULL;
	EShUsers *usr = NULL;
	gint rights = 0;
	gchar *msg = NULL;
	EDestinationStore *destination_store;
	GList *destinations, *tmp;
	ENameSelectorEntry *name_selector_entry;

	name_selector_entry = e_name_selector_peek_section_entry (sf->name_selector, "Add User");
	destination_store = e_name_selector_entry_peek_destination_store (E_NAME_SELECTOR_ENTRY (
				name_selector_entry));
	destinations = e_destination_store_list_destinations (destination_store);
	tmp = destinations;
	self_email = g_strdup (e_gw_connection_get_user_email (sf->cnc));
	for (; tmp != NULL; tmp = g_list_next (tmp)) {
		email = e_destination_get_email (tmp->data);
		/* You can't share a folder with yourself*/
		if (g_strrstr (email, "@") == NULL || (!g_ascii_strcasecmp (email , self_email)))
			e_alert_run_dialog_for_args (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (button))),
						     "org.gnome.evolution.mail_shared_folder:invalid-user",
						     email, NULL);
		else {
			if (!g_ascii_strcasecmp (email, "" )) {
				e_alert_run_dialog_for_args (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (button))),
							     "org.gnome.evolution.mail_shared_folder:no-user", NULL);

				return;
			}

			/*check whether already exists*/
			if (sf->users_list && email) {
				new_user = find_node (sf->users_list, (gchar *)email);
				if (new_user)
					return;

			}
			usr = g_new0 (EShUsers, 1);
			new_user = g_new0 (SharedUser, 1);
			usr->email = g_strdup(email);
			usr->rights = rights;
			new_user->user_node = usr;
			new_user->flag = 1;
			msg = g_strdup (email);
			gtk_list_store_append (GTK_LIST_STORE (sf->model), &(sf->iter));

		/*  have to add code for rights*/
			gtk_list_store_set (GTK_LIST_STORE (sf->model), &(sf->iter), 0, msg, -1);
			g_free(msg);
			sf->users_list = g_list_append (sf->users_list, new_user);
			sf->flag_for_ok = 0;
		}
	}
	gtk_entry_set_text (GTK_ENTRY(name_selector_entry), "");

}

static void
remove_clicked(GtkButton *button, ShareFolder *sf)
{

	SharedUser *usr = NULL;
	gchar *email;

	gtk_tree_model_get ((GtkTreeModel *) sf->model, &(sf->iter), 0, &email, -1);
	usr = find_node (sf->users_list, email);
	if (usr->flag & 0x1) {
		sf->users_list = g_list_remove (sf->users_list, usr);
		free_node(usr);
	} else {
		usr->flag = 0;
		usr->flag |= 0x4;
	}
	g_free (email);
	if (!gtk_list_store_remove (GTK_LIST_STORE (sf->model), &(sf->iter)))
		gtk_widget_set_sensitive (GTK_WIDGET (sf->remove), FALSE);
	sf->flag_for_ok = 1;
}

void
share_folder (ShareFolder *sf)
{

	GList *new_list = NULL;
	GList *update_list = NULL;
	GList *remove_list = NULL;
	GList *node = NULL;
	SharedUser *user = NULL;

	for (node = sf->users_list; node; node = node->next)
	{
		user = node->data;
		if (user->flag & 0x1)
			new_list = g_list_append (new_list, user->user_node);
		else if (user->flag & 0x2)
			update_list = g_list_append (update_list, user->user_node);
		else if (user->flag & 0x4) {
			remove_list = g_list_append (remove_list, user->user_node);
		}
	}

	if (E_IS_GW_CONNECTION (sf->cnc)) {
		if (sf->flag_for_ok == 2){  /* you have to remove all the users*/
		GList *list = NULL;

			if (new_list) {
				g_list_foreach (new_list, (GFunc) free_user_node, NULL);
				g_list_free (new_list);
			}
			if (update_list) {
				g_list_foreach (update_list, (GFunc) free_user_node, NULL);
				g_list_free (update_list);
			}

			new_list = NULL;
			if (remove_list) {
				g_list_foreach (remove_list,(GFunc) free_user_node, NULL);
				g_list_free (remove_list);
			}
			remove_list = NULL;
			if (sf->gcontainer) {
				e_gw_container_get_user_list (sf->gcontainer, &list);
				remove_list = g_list_copy (list);

			} else {
				g_warning("Container is Null");
			}

		} else {
			if (new_list) {
				if (e_gw_connection_share_folder (sf->cnc, sf->container_id, new_list, sf->sub, sf->mesg, 0) == E_GW_CONNECTION_STATUS_OK) {
					;
				}
			}

			if (update_list) {
				sf->sub = "Shared Folder rights updated";

				if (e_gw_connection_share_folder (sf->cnc, sf->container_id, update_list, sf->sub, sf->mesg, 2) == E_GW_CONNECTION_STATUS_OK) {
					;
				}
			}
		}
		if (remove_list) {
			sf->sub = "Shared Folder removed";
			if (e_gw_connection_share_folder (sf->cnc, sf->container_id, remove_list, sf->sub, sf->mesg, 1) == E_GW_CONNECTION_STATUS_OK) {
				;
			}
		}

	}
}

static void
not_ok_clicked(GtkButton *button, ShareFolder *sf)
{

	gchar *subj = NULL;
	gchar *msg = NULL;
	GtkTextIter *start, *end;
	GtkTextBuffer *buffer;

	start = g_new0 (GtkTextIter, 1);
	end = g_new0 (GtkTextIter, 1);
	subj = g_strdup (gtk_entry_get_text (sf->subject));
	if (subj)
		sf->sub = subj;
	buffer = gtk_text_view_get_buffer (sf->message);
	gtk_text_buffer_get_start_iter (buffer, start);
	gtk_text_buffer_get_end_iter (buffer, end);
	msg = gtk_text_buffer_get_text (buffer, start, end, FALSE);
	if (msg)
		sf->mesg = msg;
	gtk_widget_destroy (GTK_WIDGET (sf->window));

}

static void
not_cancel_clicked(GtkButton *button, GtkWidget *window)
{
	gtk_widget_destroy(window);
}

static void
notification_clicked(GtkButton *button, ShareFolder *sf)
{
	static  GtkBuilder *builder;
	GtkButton *not_ok;
	GtkButton *not_cancel;
	GtkWidget *vbox;

	sf->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_type_hint (GTK_WINDOW (sf->window), GDK_WINDOW_TYPE_HINT_DIALOG);

	builder = gtk_builder_new ();
	e_load_ui_builder_definition (builder, "properties.ui");

	vbox = GTK_WIDGET (e_builder_get_widget (builder, "vbox191"));
	gtk_container_add (GTK_CONTAINER (sf->window), vbox);
	sf->subject = GTK_ENTRY (e_builder_get_widget (builder, "entry3"));
	gtk_entry_set_text(GTK_ENTRY (sf->subject) , sf->sub);
	sf->message = GTK_TEXT_VIEW (e_builder_get_widget (builder, "textview1"));
	not_ok = GTK_BUTTON (e_builder_get_widget (builder, "nOK"));
	g_signal_connect ((gpointer) not_ok, "clicked", G_CALLBACK (not_ok_clicked), sf);
	not_cancel = GTK_BUTTON (e_builder_get_widget (builder, "nCancel"));
	g_signal_connect ((gpointer) not_cancel, "clicked", G_CALLBACK (not_cancel_clicked), sf->window);
	gtk_window_set_title (GTK_WINDOW (sf->window), _("Custom Notification"));
	gtk_window_set_position (GTK_WINDOW (sf->window) , GTK_WIN_POS_CENTER_ALWAYS);
	gtk_window_set_default_size (GTK_WINDOW (sf->window), 100, 200);
	gtk_window_set_transient_for ((GtkWindow *)sf->window, GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (sf->table))));
	gtk_window_set_modal ((GtkWindow *)sf->window, TRUE);
	gtk_widget_show_all (sf->window);
	gtk_window_present ((GtkWindow *)sf->window);
}

static void
addressbook_entry_changed (GtkWidget *entry, gpointer   user_data)
{

}

static void
address_button_clicked_cb (GtkButton *button, ShareFolder *sf)
{
	e_name_selector_show_dialog (sf->name_selector, sf->window);
}

static void
user_selected (GtkTreeSelection *selection, ShareFolder *sf)
{
	GtkTreeModel *model;

	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
	if (gtk_tree_selection_get_selected (selection, &model, &sf->iter))
		gtk_widget_set_sensitive (GTK_WIDGET (sf->remove), TRUE);
}

static void
add_right_clicked (GtkCellRenderer *renderer, gchar *arg1, ShareFolder *sf )
{
	gboolean right = FALSE;
	SharedUser *usr = NULL;
	EShUsers *user = NULL;
	gchar *email = NULL;
	GtkTreePath *path = NULL;

	path = gtk_tree_path_new_from_string (arg1);
	if (!gtk_tree_model_get_iter ((GtkTreeModel *) sf->model, &(sf->iter), path)) {
		gtk_tree_path_free (path);
		return;
	}
	gtk_tree_model_get ((GtkTreeModel *) sf->model, &(sf->iter), 0, &email, 1, &right , -1);
	usr = find_node(sf->users_list, email);
	if (!usr) {
		gtk_tree_path_free (path);
		return;
	}
	if (!usr->flag)  /* if user was already existing one change the flag to update*/
		usr->flag = usr->flag | 0x2;
	user = usr->user_node;
	if (!right) {
		user->rights |= 0x1;
		gtk_list_store_set (GTK_LIST_STORE (sf->model), &(sf->iter), 1, TRUE, -1);
	} else {
		user->rights &= 0x6;
		gtk_list_store_set (GTK_LIST_STORE (sf->model), &(sf->iter), 1, FALSE, -1);
	}
	gtk_tree_path_free (path);
}

static void
edit_right_clicked(GtkCellRenderer *renderer, gchar *arg1, ShareFolder *sf )
{
	gboolean right = FALSE;
	SharedUser *usr = NULL;
	EShUsers *user = NULL;
	gchar *email = NULL;
	GtkTreePath *path = NULL;

	path = gtk_tree_path_new_from_string (arg1);
	if (!gtk_tree_model_get_iter ((GtkTreeModel *) sf->model, &(sf->iter), path)) {
		gtk_tree_path_free (path);
		return;
	}
	gtk_tree_model_get ((GtkTreeModel *) sf->model, &(sf->iter), 0, &email, 2, &right , -1);
	usr = find_node(sf->users_list, email);
	if (!usr) {
		gtk_tree_path_free (path);
		return;
	}
	if (!usr->flag)  /* if user was already existing one change the flag to update*/
		usr->flag = usr->flag | 0x2;
	user = usr->user_node;

	if (!right) {
		user->rights |= 0x2;
		gtk_list_store_set (GTK_LIST_STORE (sf->model), &(sf->iter), 2, TRUE, -1);
	} else {
		user->rights &= 0x5;
		gtk_list_store_set (GTK_LIST_STORE (sf->model), &(sf->iter), 2, FALSE, -1);
	}
	gtk_tree_path_free (path);
}

static void
delete_right_clicked(GtkCellRenderer *renderer, gchar *arg1, ShareFolder *sf )
{
	gboolean right = FALSE;
	SharedUser *usr = NULL;
	EShUsers *user = NULL;
	gchar *email = NULL;
	GtkTreePath *path = NULL;

	path = gtk_tree_path_new_from_string (arg1);
	if (!gtk_tree_model_get_iter ((GtkTreeModel *) sf->model, &(sf->iter), path)) {
		gtk_tree_path_free (path);
		return;
	}
	gtk_tree_model_get ((GtkTreeModel *) sf->model, &(sf->iter), 0, &email, 3, &right , -1);
	usr = find_node(sf->users_list, email);
	if (!usr) {
		gtk_tree_path_free (path);
		return;
	}
	if (!usr->flag)  /* if user was already existing one change the flag to update*/
		usr->flag = usr->flag | 0x2;
	user = usr->user_node;
	if (!right) {
		user->rights |= 0x4;
		gtk_list_store_set (GTK_LIST_STORE (sf->model), &(sf->iter), 3, TRUE, -1);
	} else {
		user->rights &= 0x3;
		gtk_list_store_set (GTK_LIST_STORE (sf->model), &(sf->iter), 3, FALSE, -1);
	}
	gtk_tree_path_free (path);
}

static void
share_folder_construct (ShareFolder *sf)
{
	ENameSelectorDialog *name_selector_dialog;
	ENameSelectorModel *name_selector_model;
	ENameSelectorEntry *name_selector_entry;
	GtkWidget *box;

	sf->builder = gtk_builder_new ();
	e_load_ui_builder_definition (sf->builder, "properties.ui");

	sf->vbox = GTK_VBOX (e_builder_get_widget(sf->builder, "vboxSharing"));
	sf->table = GTK_VBOX (e_builder_get_widget (sf->builder, "vbox194"));
	gtk_widget_set_sensitive (GTK_WIDGET (sf->table), FALSE);

	sf->shared = GTK_RADIO_BUTTON (e_builder_get_widget (sf->builder, "radShared"));
	g_signal_connect ((gpointer) sf->shared, "clicked", G_CALLBACK (shared_clicked), sf);

	sf->not_shared = GTK_RADIO_BUTTON (e_builder_get_widget (sf->builder, "radNotShared"));
	g_signal_connect ((gpointer) sf->not_shared, "clicked", G_CALLBACK (not_shared_clicked), sf);

	sf->add_book = GTK_BUTTON (e_builder_get_widget (sf->builder, "Address"));
	gtk_widget_set_sensitive (GTK_WIDGET (sf->add_book), TRUE);
	g_signal_connect((GtkWidget *) sf->add_book, "clicked", G_CALLBACK (address_button_clicked_cb), sf);

	sf->name_selector = e_name_selector_new ();
	name_selector_dialog = e_name_selector_peek_dialog (sf->name_selector);
	g_signal_connect (name_selector_dialog, "response",
			G_CALLBACK (gtk_widget_hide), sf);

	name_selector_model = e_name_selector_peek_model (sf->name_selector);
	e_name_selector_model_add_section (name_selector_model, "Add User", _("Add User"), NULL);

	name_selector_entry = e_name_selector_peek_section_entry (sf->name_selector, "Add User");
	g_signal_connect (name_selector_entry, "changed",
			G_CALLBACK (addressbook_entry_changed), sf);
	gtk_label_set_mnemonic_widget (GTK_LABEL (e_builder_get_widget (sf->builder, "label557")), GTK_WIDGET (name_selector_entry));

	sf->add_button = GTK_BUTTON (e_builder_get_widget(sf->builder, "Add"));
	g_signal_connect((GtkWidget *) sf->add_button, "clicked", G_CALLBACK (add_clicked), sf);

	sf->remove = GTK_BUTTON(e_builder_get_widget(sf->builder, "Remove"));
	g_signal_connect ((GtkWidget *) sf->remove, "clicked", G_CALLBACK (remove_clicked), sf);
	gtk_widget_set_sensitive(GTK_WIDGET (sf->remove), FALSE);

	sf->notification = GTK_BUTTON (e_builder_get_widget (sf->builder, "Notification"));
	g_signal_connect((GtkWidget *) sf->notification, "clicked", G_CALLBACK (notification_clicked), sf);

	sf->name = GTK_ENTRY (e_builder_get_widget (sf->builder, "entry4"));
		/*TODO:connect name and label*/
	gtk_widget_hide (GTK_WIDGET(sf->name));
	box = GTK_WIDGET (e_builder_get_widget (sf->builder, "hbox227"));
	gtk_box_pack_start (GTK_BOX (box), (GtkWidget *) name_selector_entry, TRUE, TRUE, 0);
	gtk_widget_show ((GtkWidget *) name_selector_entry);

	sf->scrolled_window = GTK_WIDGET (e_builder_get_widget (sf->builder,"scrolledwindow4"));

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sf->scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	sf->model = gtk_list_store_new (4, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);
	sf->user_list = (GtkTreeView *)gtk_tree_view_new ();
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (sf->scrolled_window), (GtkWidget *)sf->user_list);
	gtk_tree_view_set_model (GTK_TREE_VIEW (sf->user_list), GTK_TREE_MODEL (sf->model));
	gtk_widget_show (GTK_WIDGET (sf->user_list));

	sf->cell = gtk_cell_renderer_text_new ();
	sf->column = gtk_tree_view_column_new_with_attributes (_("Users"), sf->cell, "text", 0, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (sf->user_list),
			GTK_TREE_VIEW_COLUMN (sf->column));

	sf->cell = gtk_cell_renderer_toggle_new ();
	sf->column = gtk_tree_view_column_new_with_attributes (_("Add   "), sf->cell, "active" , 1, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (sf->user_list),
			GTK_TREE_VIEW_COLUMN (sf->column));
	g_signal_connect (sf->cell, "toggled", G_CALLBACK (add_right_clicked), sf);

	sf->cell = gtk_cell_renderer_toggle_new ();
	sf->column = gtk_tree_view_column_new_with_attributes (_("Modify"), sf->cell, "active", 2, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (sf->user_list),
			GTK_TREE_VIEW_COLUMN (sf->column));
	g_signal_connect (sf->cell, "toggled", G_CALLBACK (edit_right_clicked), sf);

	sf->cell = gtk_cell_renderer_toggle_new ();
	sf->column = gtk_tree_view_column_new_with_attributes (_("Delete"), sf->cell, "active", 3, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (sf->user_list),
			GTK_TREE_VIEW_COLUMN (sf->column));
	g_signal_connect (sf->cell, "toggled", G_CALLBACK (delete_right_clicked), sf);

	g_signal_connect(gtk_tree_view_get_selection(GTK_TREE_VIEW (sf->user_list)), "changed", G_CALLBACK (user_selected), sf);
}

ShareFolder *
share_folder_new (EGwConnection *ccnc, gchar *id)
{
	ShareFolder *new;
	new = (ShareFolder *) g_object_new (share_folder_get_type (), NULL);
	share_folder_construct (new);
	new->cnc = ccnc;
	new->container_id = id;
	if (ccnc && id)
	get_container_list(new);

	return (ShareFolder *) new;
}

