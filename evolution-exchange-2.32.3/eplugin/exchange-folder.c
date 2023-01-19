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
 *		Shakti Sen <shprasad@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>
#include <e-folder-exchange.h>
#include <exchange-hierarchy.h>
#include <libedataserverui/e-source-selector.h>
#include <e-util/e-alert-dialog.h>
#include <mail/mail-mt.h>
#include <mail/mail-ops.h>
#include <shell/e-shell.h>

#include "exchange-operations.h"
#include "exchange-folder-subscription.h"

#define CONF_KEY_SELECTED_CAL_SOURCES "/apps/evolution/calendar/display/selected_calendars"

static CamelFolderInfo *
ex_create_folder_info (CamelStore *store, gchar *name, gchar *uri,
                  gint unread_count, gint flags)
{
	CamelFolderInfo *info;
	const gchar *path;

	path = strstr (uri, "://");
	if (!path)
		return NULL;
	path = strchr (path + 3, '/');
	if (!path)
		return NULL;

	info = camel_folder_info_new ();
	info->name = name;
	info->uri = uri;
	info->full_name = g_strdup (path + 1);
	info->unread = unread_count;

	return info;
}

static void
exchange_get_folder (gchar *uri, CamelFolder *folder, gpointer data)
{
	CamelStore *store;
	CamelFolderInfo *info;
	gchar *name = NULL;
	gchar *stored_name = NULL;
	gchar *target_uri = (gchar *)data;
	ExchangeAccount *account = NULL;

	g_return_if_fail (folder != NULL);

	account = exchange_operations_get_exchange_account ();

	if (!account) {
		g_free (target_uri);
		return;
	}

	if (strlen (target_uri) <= strlen ("exchange://") + strlen (account->account_filename)) {
		g_free (target_uri);
		return;
	}

	/* Get the subscribed folder name. */
	name = target_uri + strlen ("exchange://") + strlen (account->account_filename);
	stored_name = strrchr (name + 1, '/');

	if (stored_name)
		name[stored_name - name] = '\0';

	store = camel_folder_get_parent_store (folder);

	/* Construct the CamelFolderInfo */
	info = ex_create_folder_info (store, name, target_uri, -1, 0);
	camel_store_folder_unsubscribed (store, info);
	g_free (target_uri);
}

static void
eex_folder_inbox_unsubscribe (const gchar *uri)
{
	ExchangeAccount *account = NULL;
	gchar *path = NULL;
	gchar *stored_path = NULL;
	const gchar *inbox_uri = NULL;
	const gchar *inbox_physical_uri = NULL;
	const gchar *err_msg = NULL;
	gchar *target_uri = NULL;
	EFolder *inbox;
	ExchangeAccountFolderResult result;

	account = exchange_operations_get_exchange_account ();

	if (!account)
		return;

	if (strlen (uri) <= strlen ("exchange://") + strlen (account->account_filename))
		return;

	target_uri = g_strdup (uri);
	path = g_strdup (uri + strlen ("exchange://") + strlen (account->account_filename));
	/* User will be able to unsubscribe by doing a right click on
	   any one of this two-<other user's>Inbox or the
	   <other user's folder> tree.
	  */
	stored_path = strrchr (path + 1, '/');

	if (stored_path)
		path[stored_path - path] = '\0';

	result = exchange_account_remove_shared_folder (account, path);
	g_free (path);

	switch (result) {
		case EXCHANGE_ACCOUNT_FOLDER_OK:
			break;
		case EXCHANGE_ACCOUNT_FOLDER_ALREADY_EXISTS:
			err_msg = ERROR_DOMAIN ":folder-exists-error";
			break;
		case EXCHANGE_ACCOUNT_FOLDER_DOES_NOT_EXIST:
			err_msg = ERROR_DOMAIN ":folder-doesnt-exist-error";
			break;
		case EXCHANGE_ACCOUNT_FOLDER_UNKNOWN_TYPE:
			err_msg = ERROR_DOMAIN ":folder-unknown-type";
			break;
		case EXCHANGE_ACCOUNT_FOLDER_PERMISSION_DENIED:
			err_msg = ERROR_DOMAIN ":folder-perm-error";
			break;
		case EXCHANGE_ACCOUNT_FOLDER_OFFLINE:
			err_msg = ERROR_DOMAIN ":folder-offline-error";
			break;
		case EXCHANGE_ACCOUNT_FOLDER_UNSUPPORTED_OPERATION:
			err_msg = ERROR_DOMAIN ":folder-unsupported-error";
			break;
		case EXCHANGE_ACCOUNT_FOLDER_GENERIC_ERROR:
			err_msg = ERROR_DOMAIN ":folder-generic-error";
			break;
		case EXCHANGE_ACCOUNT_FOLDER_GC_NOTREACHABLE:
			err_msg = ERROR_DOMAIN ":folder-no-gc-error";
			break;
		case EXCHANGE_ACCOUNT_FOLDER_NO_SUCH_USER:
			err_msg = ERROR_DOMAIN ":no-user-error";
			break;
	}

	if (err_msg) {
		e_alert_run_dialog_for_args (e_shell_get_active_window (NULL), err_msg, NULL);
		return;
	}

	/* We need to get the physical uri for the Inbox */
	inbox_uri = exchange_account_get_standard_uri (account, "inbox");
	inbox = exchange_account_get_folder (account, inbox_uri);
	inbox_physical_uri = e_folder_get_physical_uri (inbox);

	/* To get the CamelStore/Folder */
	mail_get_folder (inbox_physical_uri, 0, exchange_get_folder, target_uri, mail_msg_unordered_push);
}

static void
unsubscribe_dialog_ab_response (GtkDialog *dialog, gint response, ESource *source)
{

	if (response == GTK_RESPONSE_OK) {
		ExchangeAccount *account = NULL;
		gchar *path = NULL;
		gchar *uri = NULL;
		const gchar *source_uid = NULL;
		ESourceGroup *source_group = NULL;

		g_return_if_fail (source != NULL);

		account = exchange_operations_get_exchange_account ();

		if (!account)
			return;

		uri = e_source_get_uri (source);
		if (!uri || strlen (uri) <= strlen ("exchange://") + strlen (account->account_filename)) {
			g_free (uri);
			return;
		}

		path = g_strdup (uri + strlen ("exchange://") + strlen (account->account_filename));
		source_uid = e_source_peek_uid (source);

		exchange_account_remove_shared_folder (account, path);

		source_group = e_source_peek_group (source);
		e_source_group_remove_source_by_uid (source_group, source_uid);
		g_free (path);
		g_free (uri);
		gtk_widget_destroy (GTK_WIDGET (dialog));
	}
	if (response == GTK_RESPONSE_CANCEL)
		gtk_widget_destroy (GTK_WIDGET (dialog));
	if (response == GTK_RESPONSE_DELETE_EVENT)
		gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
unsubscribe_dialog_response (GtkDialog *dialog, gint response, ESource *source)
{

	if (response == GTK_RESPONSE_OK) {
		GSList *ids, *node_to_be_deleted;
		ExchangeAccount *account = NULL;
		gchar *path = NULL;
		gchar *ruri = NULL;
		const gchar *source_uid = NULL;
		GConfClient *client;
		ESourceGroup *source_group = NULL;

		g_return_if_fail (source != NULL);

		client = gconf_client_get_default ();

		account = exchange_operations_get_exchange_account ();

		if (!account)
			return;

		ruri = (gchar *) e_source_peek_relative_uri (source);
		source_uid = e_source_peek_uid (source);

		if (!ruri || strlen (ruri) <= strlen (account->account_filename))
			return;

		path = g_strdup (ruri + strlen (account->account_filename));
		exchange_account_remove_shared_folder (account, path);
		ids = gconf_client_get_list (client,
					     CONF_KEY_SELECTED_CAL_SOURCES,
					     GCONF_VALUE_STRING, NULL);
		if (ids) {
			node_to_be_deleted = g_slist_find_custom (
						ids,
						source_uid,
						(GCompareFunc) strcmp);
			if (node_to_be_deleted) {
				g_free (node_to_be_deleted->data);
				ids = g_slist_delete_link (ids,
						node_to_be_deleted);
				gconf_client_set_list (client,
					CONF_KEY_SELECTED_CAL_SOURCES,
					GCONF_VALUE_STRING, ids, NULL);
			}
			g_slist_foreach (ids, (GFunc) g_free, NULL);
			g_slist_free (ids);
		}

		source_group = e_source_peek_group (source);
		e_source_group_remove_source_by_uid (source_group, source_uid);
		g_free (path);
		gtk_widget_destroy (GTK_WIDGET (dialog));
	}
	if (response == GTK_RESPONSE_CANCEL)
		gtk_widget_destroy (GTK_WIDGET (dialog));
	if (response == GTK_RESPONSE_DELETE_EVENT)
		gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
eex_addresssbook_unsubscribe (ESource *source)
{
	GtkWidget *dialog = NULL;
	GtkWidget *content_area;
	ExchangeAccount *account = NULL;
	gchar *title = NULL;
	gchar *displayed_folder_name = NULL;
	gint response;
	gint mode;
	ExchangeConfigListenerStatus status;

	g_return_if_fail (source != NULL);

	account = exchange_operations_get_exchange_account ();
	if (!account)
		return;

	status = exchange_is_offline (&mode);
	if (status != CONFIG_LISTENER_STATUS_OK) {
		g_warning ("Config listener not found");
		return;
	} else if (mode == OFFLINE_MODE) {
		e_alert_run_dialog_for_args (e_shell_get_active_window (NULL), ERROR_DOMAIN ":account-offline-generic", NULL);
		return;
	}

	displayed_folder_name = (gchar *) e_source_peek_name (source);
	dialog = gtk_message_dialog_new (NULL,
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_NONE,
					 _("Really unsubscribe from folder \"%s\"?"),
					 displayed_folder_name);

	gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_REMOVE, GTK_RESPONSE_OK);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	gtk_container_set_border_width (GTK_CONTAINER (dialog), 6);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_box_set_spacing (GTK_BOX (content_area), 6);

	title = g_strdup_printf (_("Unsubscribe from \"%s\""), displayed_folder_name);
	gtk_window_set_title (GTK_WINDOW (dialog), title);
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	g_free (title);
	g_free (displayed_folder_name);

	gtk_widget_show (dialog);
	unsubscribe_dialog_ab_response (GTK_DIALOG (dialog), response, source);
}

static void
eex_calendar_unsubscribe (ESource *source)
{
	GtkWidget *dialog = NULL;
	GtkWidget *content_area;
	ExchangeAccount *account = NULL;
	gchar *title = NULL;
	const gchar *displayed_folder_name;
	gint response;
	gint mode;
	ExchangeConfigListenerStatus status;

	g_return_if_fail (source != NULL);

	account = exchange_operations_get_exchange_account ();
	if (!account)
		return;

	status = exchange_is_offline (&mode);

	if (status != CONFIG_LISTENER_STATUS_OK) {
		g_warning ("Config listener not found");
		return;
	} else if (mode == OFFLINE_MODE) {
		e_alert_run_dialog_for_args (e_shell_get_active_window (NULL), ERROR_DOMAIN ":account-offline-generic", NULL);
		return;
	}

	displayed_folder_name =  e_source_peek_name (source);
	dialog = gtk_message_dialog_new (NULL,
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_NONE,
					 _("Really unsubscribe from folder \"%s\"?"),
					 displayed_folder_name);

	gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_REMOVE, GTK_RESPONSE_OK);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	gtk_container_set_border_width (GTK_CONTAINER (dialog), 6);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_box_set_spacing (GTK_BOX (content_area), 6);

	title = g_strdup_printf (_("Unsubscribe from \"%s\""), displayed_folder_name);
	gtk_window_set_title (GTK_WINDOW (dialog), title);
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	g_free (title);

	gtk_widget_show (dialog);
	unsubscribe_dialog_response (GTK_DIALOG (dialog), response, source);
}

void
call_folder_subscribe (const gchar *folder_name)
{
	ExchangeAccount *account = NULL;
	gint mode;
	ExchangeConfigListenerStatus status;

	g_return_if_fail (folder_name != NULL);

	account = exchange_operations_get_exchange_account ();
	if (!account)
		return;

	status = exchange_is_offline (&mode);
	if (status != CONFIG_LISTENER_STATUS_OK) {
		g_warning ("Config listener not found");
		return;
	} else if (mode == OFFLINE_MODE) {
		/* Translators: this error code can be used for any operation
		 * (like subscribing to other user's folders, unsubscribing
		 * etc,) which can not be performed in offline mode
		 */
		e_alert_run_dialog_for_args (e_shell_get_active_window (NULL), ERROR_DOMAIN ":account-offline-generic", NULL);
		return;
	}

	create_folder_subscription_dialog (account, folder_name);
}

void
call_folder_unsubscribe (const gchar *folder_type, const gchar *uri, ESource *source)
{
	g_return_if_fail (folder_type != NULL);
	g_return_if_fail (uri != NULL);

	if (g_str_equal (folder_type, N_("Inbox"))) {
		eex_folder_inbox_unsubscribe (uri);
	} else if (g_str_equal (folder_type, N_("Calendar"))) {
		g_return_if_fail (source != NULL);
		eex_calendar_unsubscribe (source);
	} else if (g_str_equal (folder_type, N_("Tasks"))) {
		g_return_if_fail (source != NULL);
		eex_calendar_unsubscribe (source);
	} else if (g_str_equal (folder_type, N_("Contacts"))) {
		g_return_if_fail (source != NULL);
		eex_addresssbook_unsubscribe (source);
	} else {
		g_return_if_reached ();
	}
}
