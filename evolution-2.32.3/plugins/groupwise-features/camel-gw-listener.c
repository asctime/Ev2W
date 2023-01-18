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
 *		Sivaiah Nallagatla <snallagatla@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "camel-gw-listener.h"
#include <string.h>
#include <glib/gi18n.h>
#include <camel/camel.h>
#include <e-gw-connection.h>
#include <libedataserverui/e-passwords.h>
#include "e-util/e-alert-dialog.h"
#include <libedataserver/e-account.h>
#include <libecal/e-cal.h>
#include <shell/e-shell.h>

/*stores some info about all currently existing groupwise accounts
  list of GwAccountInfo structures */

static	GList *groupwise_accounts = NULL;

struct _CamelGwListenerPrivate {
	GConfClient *gconf_client;
	/* we get notification about mail account changes form this object */
	EAccountList *account_list;
};

struct _GwAccountInfo {
	gchar *uid;
	gchar *name;
	gchar *source_url;
	gboolean auto_check;
	guint auto_check_time;
};

typedef struct _GwAccountInfo GwAccountInfo;

#define GROUPWISE_URI_PREFIX   "groupwise://"
#define GROUPWISE_PREFIX_LENGTH 12

#define PARENT_TYPE G_TYPE_OBJECT

static GObjectClass *parent_class = NULL;

static void dispose (GObject *object);
static void finalize (GObject *object);

static void
camel_gw_listener_class_init (CamelGwListenerClass *class)
{
	GObjectClass *object_class;

	parent_class =  g_type_class_ref (PARENT_TYPE);
	object_class = G_OBJECT_CLASS (class);

	/* virtual method override */
	object_class->dispose = dispose;
	object_class->finalize = finalize;
}

static void
camel_gw_listener_init (CamelGwListener *config_listener,  CamelGwListenerClass *class)
{
	config_listener->priv = g_new0 (CamelGwListenerPrivate, 1);
}

static void
dispose (GObject *object)
{
	CamelGwListener *config_listener = CAMEL_GW_LISTENER (object);

	g_object_unref (config_listener->priv->gconf_client);
	g_object_unref (config_listener->priv->account_list);

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
finalize (GObject *object)
{
	CamelGwListener *config_listener = CAMEL_GW_LISTENER (object);
	GList *list;
	GwAccountInfo *info;

	if (config_listener->priv) {
		g_free (config_listener->priv);
	}

	for (list = g_list_first (groupwise_accounts); list; list = g_list_next (list)) {

		info = (GwAccountInfo *) (list->data);

		if (info) {

			g_free (info->uid);
			g_free (info->name);
			g_free (info->source_url);
			g_free (info);
		}
	}

	g_list_free (groupwise_accounts);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

/*determines whehter the passed in account is groupwise or not by looking at source url */

static gboolean
is_groupwise_account (EAccount *account)
{
	if (account->source->url != NULL) {
		return (strncmp (account->source->url,  GROUPWISE_URI_PREFIX, GROUPWISE_PREFIX_LENGTH ) == 0);
	} else {
		return FALSE;
	}
}

/* looks up for an existing groupwise account info in the groupwise_accounts list based on uid */

static GwAccountInfo*
lookup_account_info (const gchar *key)
{
	GList *list;
        GwAccountInfo *info;
	gint found = 0;

        if (!key)
                return NULL;

	info = NULL;

        for (list = g_list_first (groupwise_accounts);  list;  list = g_list_next (list)) {
                info = (GwAccountInfo *) (list->data);
                found = (strcmp (info->uid, key) == 0);
		if (found)
			break;
	}
	if (found)
		return info;
	return NULL;
}

#define CALENDAR_SOURCES "/apps/evolution/calendar/sources"
#define TASKS_SOURCES "/apps/evolution/tasks/sources"
#define NOTES_SOURCES "/apps/evolution/memos/sources"
#define SELECTED_CALENDARS "/apps/evolution/calendar/display/selected_calendars"
#define SELECTED_TASKS   "/apps/evolution/calendar/tasks/selected_tasks"
#define SELECTED_NOTES   "/apps/evolution/calendar/memos/selected_memos"

static void
add_esource (const gchar *conf_key, GwAccountInfo *info,  const gchar *source_name, CamelURL *url, const gchar * parent_id_name, gboolean can_create)
{
	ESourceList *source_list;
	ESourceGroup *group;
	ESource *source;
        GConfClient* client;
	GSList *ids, *temp;
	const gchar *source_selection_key;
	gchar *relative_uri;
	const gchar *soap_port;
	const gchar * use_ssl;
	const gchar *poa_address;
	const gchar *offline_sync;
	const gchar *group_name;

	poa_address = url->host;
	if (!poa_address || strlen (poa_address) ==0)
		return;

	group_name = info->name;

	soap_port = camel_url_get_param (url, "soap_port");

	if (!soap_port || strlen (soap_port) == 0)
		soap_port = "7191";

	use_ssl = camel_url_get_param (url, "use_ssl");

	offline_sync = camel_url_get_param (url, "offline_sync");

	client = gconf_client_get_default();
	source_list = e_source_list_new_for_gconf (client, conf_key);
	group = e_source_group_new (group_name,  GROUPWISE_URI_PREFIX);

	if (!e_source_list_add_group (source_list, group, -1))
		return;

	if (!can_create)
		e_source_group_set_property (group, "create_source", "no");

	relative_uri = g_strdup_printf ("%s@%s/", url->user, poa_address);
	source = e_source_new (source_name, relative_uri);
	e_source_set_property (source, "auth", "1");
	e_source_set_property (source, "username", url->user);
	e_source_set_property (source, "port", soap_port);
	e_source_set_property (source, "auth-domain", "Groupwise");
	e_source_set_property (source, "use_ssl", use_ssl);

	if (info->auto_check) {
		gchar *str = g_strdup_printf ("%d", info->auto_check_time);

		e_source_set_property (source, "refresh", str);
		g_free (str);
	} else
		e_source_set_property (source, "refresh", NULL);

	e_source_set_property (source, "offline_sync", offline_sync ? "1" : "0" );
	e_source_set_property (source, "delete", "no");
	if (parent_id_name) {
		e_source_set_property (source, "parent_id_name", parent_id_name);
		e_source_set_color_spec (source, camel_url_get_param (url, "color"));
	} else
		e_source_set_color_spec (source, "#EEBC60");
	e_source_group_add_source (group, source, -1);
	e_source_list_sync (source_list, NULL);

	if (!strcmp (conf_key, CALENDAR_SOURCES))
		source_selection_key = SELECTED_CALENDARS;
	else if (!strcmp (conf_key, TASKS_SOURCES))
		source_selection_key = SELECTED_TASKS;
	else if (!strcmp (conf_key, NOTES_SOURCES))
		source_selection_key = SELECTED_NOTES;
	else
		source_selection_key = NULL;

	if (source_selection_key) {
		ids = gconf_client_get_list (client, source_selection_key , GCONF_VALUE_STRING, NULL);
		ids = g_slist_append (ids, g_strdup (e_source_peek_uid (source)));
		gconf_client_set_list (client,  source_selection_key, GCONF_VALUE_STRING, ids, NULL);
		temp  = ids;

		for (; temp != NULL; temp = g_slist_next (temp))
			g_free (temp->data);

		g_slist_free (ids);
	}

	g_object_unref (source);
	g_object_unref (group);
	g_object_unref (source_list);
	g_object_unref (client);
	g_free (relative_uri);
}

static void
remove_esource (const gchar *conf_key, const gchar *group_name, gchar * source_name, const gchar * relative_uri)
{
	ESourceList *list;
        GSList *groups;
	gboolean found_group;
	GConfClient* client;
	GSList *ids;
	GSList *node_tobe_deleted;
	const gchar *source_selection_key;

        client = gconf_client_get_default();
        list = e_source_list_new_for_gconf (client, conf_key);
	groups = e_source_list_peek_groups (list);

	found_group = FALSE;

	for (; groups != NULL && !found_group; groups = g_slist_next (groups)) {
		ESourceGroup *group = E_SOURCE_GROUP (groups->data);

		if (strcmp (e_source_group_peek_name (group), group_name) == 0 &&
		   strcmp (e_source_group_peek_base_uri (group), GROUPWISE_URI_PREFIX ) == 0) {
			GSList *sources = e_source_group_peek_sources (group);

			for (; sources != NULL; sources = g_slist_next (sources)) {
				ESource *source = E_SOURCE (sources->data);
				const gchar *source_relative_uri;

				source_relative_uri = e_source_peek_relative_uri (source);
				if (source_relative_uri == NULL)
					continue;
				if (strcmp (source_relative_uri, relative_uri) == 0) {

					if (!strcmp (conf_key, CALENDAR_SOURCES))
						source_selection_key = SELECTED_CALENDARS;
					else if (!strcmp (conf_key, TASKS_SOURCES))
						source_selection_key = SELECTED_TASKS;
					else if (!strcmp (conf_key, NOTES_SOURCES))
						source_selection_key = SELECTED_NOTES;
					else source_selection_key = NULL;
					if (source_selection_key) {
						ids = gconf_client_get_list (client, source_selection_key ,
									     GCONF_VALUE_STRING, NULL);
						node_tobe_deleted = g_slist_find_custom (ids, e_source_peek_uid (source), (GCompareFunc) strcmp);
						if (node_tobe_deleted) {
							g_free (node_tobe_deleted->data);
							ids = g_slist_delete_link (ids, node_tobe_deleted);
						}
						gconf_client_set_list (client,  source_selection_key,
								       GCONF_VALUE_STRING, ids, NULL);

					}
					e_source_list_remove_group (list, group);
					e_source_list_sync (list, NULL);
					found_group = TRUE;
					break;

				}
			}

		}

	}

	g_object_unref (list);
	g_object_unref (client);

}

/* looks up for e-source with having same info as old_account_info and changes its values passed in new values */

static void
modify_esource (const gchar * conf_key, GwAccountInfo *old_account_info, EAccount *a, CamelURL *new_url)
{
	ESourceList *list;
        GSList *groups;
	gchar *old_relative_uri;
	CamelURL *url;
	gboolean found_group;
	GConfClient* client;
	const gchar *poa_address;
	const gchar *new_poa_address;
	const gchar * new_group_name = a->name;

	url = camel_url_new (old_account_info->source_url, NULL);
	poa_address = url->host;
	if (!poa_address || strlen (poa_address) ==0)
		return;
	new_poa_address = new_url->host;

	old_relative_uri =  g_strdup_printf ("%s@%s/", url->user, poa_address);
	client = gconf_client_get_default ();
        list = e_source_list_new_for_gconf (client, conf_key);
	groups = e_source_list_peek_groups (list);

	found_group = FALSE;

	for (; groups != NULL &&  !found_group; groups = g_slist_next (groups)) {
		ESourceGroup *group = E_SOURCE_GROUP (groups->data);

		if (strcmp (e_source_group_peek_name (group), old_account_info->name) == 0 &&
		    strcmp (e_source_group_peek_base_uri (group), GROUPWISE_URI_PREFIX) == 0) {
			GSList *sources = e_source_group_peek_sources (group);

			for (; sources != NULL; sources = g_slist_next (sources)) {
				ESource *source = E_SOURCE (sources->data);
				const gchar *source_relative_uri;

				source_relative_uri = e_source_peek_relative_uri (source);
				if (source_relative_uri == NULL)
					continue;
				if (strcmp (source_relative_uri, old_relative_uri) == 0) {
					gchar *new_relative_uri;

					new_relative_uri = g_strdup_printf ("%s@%s/", new_url->user, new_poa_address);
					e_source_group_set_name (group, new_group_name);
					e_source_set_relative_uri (source, new_relative_uri);
					e_source_set_property (source, "username", new_url->user);
					e_source_set_property (source, "port", camel_url_get_param (new_url,"soap_port"));
					e_source_set_property (source, "use_ssl",  camel_url_get_param (url, "use_ssl"));
					e_source_set_property (source, "offline_sync",  camel_url_get_param (url, "offline_sync") ? "1" : "0");

					if (a->source->auto_check) {
						gchar *str = g_strdup_printf ("%d", a->source->auto_check_time);

						e_source_set_property (source, "refresh", str);
						g_free (str);
					} else
						e_source_set_property (source, "refresh", NULL);

					e_source_list_sync (list, NULL);
					found_group = TRUE;
					g_free (new_relative_uri);
					break;
				}
			}
		}
	}

	g_object_unref (list);
	g_object_unref (client);
	camel_url_free (url);
	g_free (old_relative_uri);

}
/* add sources for calendar and tasks if the account added is groupwise account
   adds the new account info to  groupwise_accounts list */

static void
add_calendar_tasks_sources (GwAccountInfo *info)
{
	CamelURL *url;

	url = camel_url_new (info->source_url, NULL);
	add_esource ("/apps/evolution/calendar/sources", info, _("Calendar"), url, NULL, FALSE);
	add_esource ("/apps/evolution/tasks/sources", info, _("Tasks"), url, NULL, FALSE);
	add_esource ("/apps/evolution/memos/sources", info, _("Notes"), url, NULL, TRUE);

	camel_url_free (url);

}

/* removes calendar and tasks sources if the account removed is groupwise account
   removes the the account info from groupwise_account list */

static void
remove_calendar_tasks_sources (GwAccountInfo *info)
{
	CamelURL *url;
	gchar *relative_uri;
	const gchar *poa_address;

	url = camel_url_new (info->source_url, NULL);

	poa_address = url->host;
	if (!poa_address || strlen (poa_address) ==0)
		return;

	relative_uri =  g_strdup_printf ("%s@%s/", url->user, poa_address);
	remove_esource ("/apps/evolution/calendar/sources", info->name, _("Calendar"), relative_uri);
	remove_esource ("/apps/evolution/tasks/sources", info->name,  _("Checklist"), relative_uri);
	remove_esource ("/apps/evolution/memos/sources", info->name,  _("Notes"), relative_uri);

	camel_url_free (url);
	g_free (relative_uri);

}

static GList*
get_addressbook_names_from_server (gchar *source_url)
{
	gchar *key;
        EGwConnection *cnc;
	gchar *password;
	GList *book_list = NULL;
	gint status, count = 0;
	const gchar *soap_port;
	CamelURL *url;
	gboolean remember;
	gchar *failed_auth = NULL;
	gchar *prompt;
	gchar *password_prompt;
	gchar *uri;
	const gchar *use_ssl;
	const gchar *poa_address;
	guint32 flags = E_PASSWORDS_REMEMBER_FOREVER|E_PASSWORDS_SECRET;

	url = camel_url_new (source_url, NULL);
        if (url == NULL) {
                return NULL;
        }
	poa_address = url->host;
	if (!poa_address || strlen (poa_address) ==0)
		return NULL;

        soap_port = camel_url_get_param (url, "soap_port");
        if (!soap_port || strlen (soap_port) == 0)
                soap_port = "7191";
	use_ssl = camel_url_get_param (url, "use_ssl");

	key =  g_strdup_printf ("groupwise://%s@%s/", url->user, poa_address);

	if (use_ssl && g_str_equal (use_ssl, "always"))
		uri = g_strdup_printf ("https://%s:%s/soap", poa_address, soap_port);
	else
		uri = g_strdup_printf ("http://%s:%s/soap", poa_address, soap_port);

	cnc = NULL;

	do {
		count++;
		/*we have to uncache the password before prompting again*/
		if (failed_auth) {
			e_passwords_forget_password ("Groupwise", key);
			password = NULL;
		}

		password = e_passwords_get_password ("Groupwise", key);
		if (!password) {
			password_prompt = g_strdup_printf (_("Enter password for %s (user %s)"),
					poa_address, url->user);
			prompt = g_strconcat (failed_auth ? failed_auth : "", password_prompt, NULL);
			g_free (password_prompt);
			password = e_passwords_ask_password (prompt, "Groupwise", key, prompt,
					flags, &remember,
					NULL);
			g_free (prompt);

			if (!password)
				break;
		}

		cnc = e_gw_connection_new (uri, url->user, password);
		g_free (password);
		if (!E_IS_GW_CONNECTION(cnc)) {
			if (count == 3)
				break;
		}

		failed_auth = _("Failed to authenticate.\n");
		flags |= E_PASSWORDS_REPROMPT;
	} while (cnc == NULL);

	g_free (key);

	if (E_IS_GW_CONNECTION(cnc))  {
		book_list = NULL;
		status = e_gw_connection_get_address_book_list (cnc, &book_list);
		if (status == E_GW_CONNECTION_STATUS_OK)
			return book_list;
	}

	/*FIXME: This error message should be relocated to addressbook and should reflect
	 * that it actually failed to get the addressbooks*/
	e_alert_run_dialog_for_args (e_shell_get_active_window (NULL),
				     "mail:gw-accountsetup-error", poa_address,
				     NULL);
	return NULL;
}

static void
add_proxy_sources (GwAccountInfo *info, const gchar *parent_name)
{
	CamelURL *url;
	gchar *color;

	url = camel_url_new (info->source_url, NULL);

	color = g_strdup_printf ("#%06X",  g_random_int_range (0x100000, 0xffffaa));
	/* The above range is chosen so that the colors are neither too light nor too dark
	and appealing in all the themes */

	camel_url_set_param (url, "color", color);

	add_esource ("/apps/evolution/calendar/sources", info, _("Calendar"), url, parent_name, FALSE);
	add_esource ("/apps/evolution/tasks/sources", info, _("Tasks"), url, parent_name, FALSE);
	add_esource ("/apps/evolution/memos/sources", info, _("Notes"), url, parent_name, TRUE);

	g_free (color);
	camel_url_free (url);
}

static gboolean
add_addressbook_sources (EAccount *account)
{
	CamelURL *url;
	ESourceList *list;
        ESourceGroup *group;
        ESource *source;
	gchar *base_uri;
	const gchar *soap_port;
	GList *books_list, *temp_list;
	GConfClient* client;
	const gchar * use_ssl;
	const gchar *poa_address;
	gboolean is_frequent_contacts = FALSE, is_writable = FALSE;

        url = camel_url_new (account->source->url, NULL);
	if (url == NULL) {
		return FALSE;
	}

	poa_address = url->host;
	if (!poa_address || strlen (poa_address) ==0)
		return FALSE;

	soap_port = camel_url_get_param (url, "soap_port");
	if (!soap_port || strlen (soap_port) == 0)
		soap_port = "7191";
	use_ssl = camel_url_get_param (url, "use_ssl");
	base_uri =  g_strdup_printf ("groupwise://%s@%s", url->user, poa_address);
	client = gconf_client_get_default ();
	list = e_source_list_new_for_gconf (client, "/apps/evolution/addressbook/sources" );
	group = e_source_group_new (account->name, base_uri);
	books_list = get_addressbook_names_from_server (account->source->url);
	temp_list = books_list;
	if (!temp_list)
		return FALSE;
	for (; temp_list != NULL; temp_list = g_list_next (temp_list)) {
		const gchar *book_name =  e_gw_container_get_name (E_GW_CONTAINER(temp_list->data));
		/* is_writable is set to TRUE if the book has isPersonal property,
		 * by e_gw_connection_get_address_book_list()
		 */
		is_writable = e_gw_container_get_is_writable (E_GW_CONTAINER(temp_list->data));
		if (is_writable &&
		    !g_ascii_strncasecmp (book_name, "Novell GroupWise Address Book", strlen (book_name))) {
			/* This is a hack to not to show multiple groupwise system address books
			 * if they are the personal address books with the name of system address book
			 * See http://bugzilla.gnome.org/show_bug.cgi?id=320119
			 * and http://bugzilla.gnome.org/show_bug.cgi?id=309511
			 */
			continue;
		}

		if (!is_frequent_contacts)
			is_frequent_contacts = e_gw_container_get_is_frequent_contacts (E_GW_CONTAINER (temp_list->data));
		source = e_source_new (book_name, g_strconcat (";",book_name, NULL));
		e_source_set_property (source, "auth", "plain/password");
		e_source_set_property (source, "auth-domain", "Groupwise");
		e_source_set_property (source, "port", soap_port);
		e_source_set_property(source, "user", url->user);
		/* mark system address book for offline usage */
		/* FIXME: add isPersonal flag to container and use that isFrequentContact
		 * properties, instead of using writable to distinguish between the
		 * system address book and other address books.
		 */
		if (!is_writable)
			e_source_set_property (source, "offline_sync", "1");
		else
			e_source_set_property (source, "offline_sync",
					       camel_url_get_param (url, "offline_sync") ? "1" : "0");
		if (!is_writable)
			e_source_set_property (source, "completion", "true");
		if (is_frequent_contacts)
			e_source_set_property (source, "completion", "true");
		e_source_set_property (source, "use_ssl", use_ssl);
		e_source_group_add_source (group, source, -1);
		g_object_unref (source);
	}
	e_source_list_add_group (list, group, -1);
	e_source_list_sync (list, NULL);
	g_object_unref (group);
	g_object_unref (list);
	g_object_unref (client);
	g_free (base_uri);

	if (!is_frequent_contacts) {
		/* display warning message */
		e_alert_run_dialog_for_args (e_shell_get_active_window (NULL),
					     "addressbook:gw-book-list-init", NULL);
	}
	return TRUE;
}

static void
modify_addressbook_sources ( EAccount *account, GwAccountInfo *existing_account_info )
{
	CamelURL *url;
	ESourceList *list;
        ESourceGroup *group;
	GSList *groups;
	gboolean found_group;
	gboolean delete_group;
	gchar *old_base_uri;
	gchar *new_base_uri;
	const gchar *soap_port;
	const gchar *use_ssl;
	GSList *sources;
	ESource *source;
	GConfClient *client;
	const gchar *poa_address;

	url = camel_url_new (existing_account_info->source_url, NULL);
	if (url == NULL) {
		return;
	}

	poa_address = url->host;
	if (!poa_address || strlen (poa_address) ==0)
		return;

	old_base_uri =  g_strdup_printf ("groupwise://%s@%s", url->user, poa_address);
	camel_url_free (url);

	url = camel_url_new (account->source->url, NULL);
	if (url == NULL)
		return;
	poa_address = url->host;
	if (!poa_address || strlen (poa_address) ==0)
		return;
	new_base_uri = g_strdup_printf ("groupwise://%s@%s", url->user, poa_address);
	soap_port = camel_url_get_param (url, "soap_port");
	if (!soap_port || strlen (soap_port) == 0)
		soap_port = "7191";
	use_ssl = camel_url_get_param (url, "use_ssl");

	client = gconf_client_get_default ();
	list = e_source_list_new_for_gconf (client, "/apps/evolution/addressbook/sources" );
	groups = e_source_list_peek_groups (list);
	delete_group = FALSE;
	if (strcmp (old_base_uri, new_base_uri) != 0)
		delete_group = TRUE;
	group = NULL;
	found_group = FALSE;
	for (; groups != NULL &&  !found_group; groups = g_slist_next (groups)) {

		group = E_SOURCE_GROUP (groups->data);
		if ( strcmp ( e_source_group_peek_base_uri(group), old_base_uri) == 0 && strcmp (e_source_group_peek_name (group), existing_account_info->name) == 0) {
			found_group = TRUE;
			if (!delete_group) {
				e_source_group_set_name (group, account->name);
				sources = e_source_group_peek_sources (group);
				for (; sources != NULL; sources = g_slist_next (sources)) {
					source = E_SOURCE (sources->data);
					e_source_set_property (source, "port", soap_port);
					e_source_set_property (source, "use_ssl", use_ssl);
				}

				e_source_list_sync (list, NULL);
			}

		}
	}
	if (found_group && delete_group) {
		e_source_list_remove_group (list, group);
		e_source_list_sync (list, NULL);
		g_object_unref (list);
		list = NULL;
		add_addressbook_sources (account);
	}
	g_free (old_base_uri);
	if (list)
		g_object_unref (list);
	camel_url_free (url);
	g_object_unref (client);

}

static void
remove_addressbook_sources (GwAccountInfo *existing_account_info)
{
	ESourceList *list;
        ESourceGroup *group;
	GSList *groups;
	gboolean found_group;
	CamelURL *url;
	gchar *base_uri;
	GConfClient *client;
	const gchar *poa_address;

	url = camel_url_new (existing_account_info->source_url, NULL);
	if (url == NULL) {
		return;
	}

	poa_address = url->host;
	if (!poa_address || strlen (poa_address) ==0)
		return;

	base_uri =  g_strdup_printf ("groupwise://%s@%s", url->user,  poa_address);
	client = gconf_client_get_default ();
	list = e_source_list_new_for_gconf (client, "/apps/evolution/addressbook/sources" );
	groups = e_source_list_peek_groups (list);

	found_group = FALSE;

	for (; groups != NULL &&  !found_group; groups = g_slist_next (groups)) {

		group = E_SOURCE_GROUP (groups->data);
		if ( strcmp ( e_source_group_peek_base_uri (group), base_uri) == 0 && strcmp (e_source_group_peek_name (group), existing_account_info->name) == 0) {

			e_source_list_remove_group (list, group);
			e_source_list_sync (list, NULL);
			found_group = TRUE;

		}
	}
	g_object_unref (list);
	g_object_unref (client);
	g_free (base_uri);
	camel_url_free (url);

}

static void
account_added (EAccountList *account_listener, EAccount *account)
{

	GwAccountInfo *info;
	EAccount *parent;
	gboolean status;
	CamelURL *parent_url;

	if (!is_groupwise_account (account))
		return;

	info = g_new0 (GwAccountInfo, 1);
	info->uid = g_strdup (account->uid);
	info->name = g_strdup (account->name);
	info->source_url = g_strdup (account->source->url);
	info->auto_check = account->source->auto_check;
	info->auto_check_time = account->source->auto_check_time;
	if (account->parent_uid) {
		parent = (EAccount *)e_account_list_find (account_listener, E_ACCOUNT_FIND_UID, account->parent_uid);

		if (!parent)
			return;

		parent_url = camel_url_new (e_account_get_string(parent, E_ACCOUNT_SOURCE_URL), NULL);
		add_proxy_sources (info, parent_url->user);
	} else {
		status = add_addressbook_sources (account);

		if (status)
			add_calendar_tasks_sources (info);
	}
	groupwise_accounts = g_list_append (groupwise_accounts, info);
}

static void
account_removed (EAccountList *account_listener, EAccount *account)
{
	GwAccountInfo *info;

	if (!is_groupwise_account (account))
		return;

	info = lookup_account_info (account->uid);
	if (info == NULL)
		return;

	remove_calendar_tasks_sources (info);
	remove_addressbook_sources (info);
	groupwise_accounts = g_list_remove (groupwise_accounts, info);
	g_free (info->uid);
	g_free (info->name);
	g_free (info->source_url);
        g_free (info);
}

static void
account_changed (EAccountList *account_listener, EAccount *account)
{
	gboolean is_gw_account;
	CamelURL *old_url, *new_url;
	const gchar *old_soap_port, *new_soap_port;
	GwAccountInfo *existing_account_info;
	const gchar *old_use_ssl, *new_use_ssl;
	const gchar *old_poa_address, *new_poa_address;

	is_gw_account = is_groupwise_account (account);

	existing_account_info = lookup_account_info (account->uid);

	if (existing_account_info == NULL && is_gw_account) {

		if (!account->enabled)
			return;

		/* some account of other type is changed to Groupwise */
		account_added (account_listener, account);

	} else if ( existing_account_info != NULL && !is_gw_account) {

		/*Groupwise account is changed to some other type */
		remove_calendar_tasks_sources (existing_account_info);
		remove_addressbook_sources (existing_account_info);
		groupwise_accounts = g_list_remove (groupwise_accounts, existing_account_info);
		g_free (existing_account_info->uid);
		g_free (existing_account_info->name);
		g_free (existing_account_info->source_url);
		g_free (existing_account_info);

	} else if (existing_account_info != NULL && is_gw_account) {

		if (!account->enabled) {
			account_removed (account_listener, account);
			return;
		}

		/* some info of groupwise account is changed . update the sources with new info if required */
		old_url = camel_url_new (existing_account_info->source_url, NULL);
		old_poa_address = old_url->host;
		old_soap_port = camel_url_get_param (old_url, "soap_port");
		old_use_ssl = camel_url_get_param (old_url, "use_ssl");
		new_url = camel_url_new (account->source->url, NULL);
		new_poa_address = new_url->host;

		if (!new_poa_address || strlen (new_poa_address) ==0)
			return;

		new_soap_port = camel_url_get_param (new_url, "soap_port");

		if (!new_soap_port || strlen (new_soap_port) == 0)
			new_soap_port = "7191";

		new_use_ssl = camel_url_get_param (new_url, "use_ssl");

		if ((old_poa_address && strcmp (old_poa_address, new_poa_address))
		   ||  (old_soap_port && strcmp (old_soap_port, new_soap_port))
		   ||  strcmp (old_url->user, new_url->user)
		   || (!old_use_ssl)
		   || strcmp (old_use_ssl, new_use_ssl)) {

			account_removed (account_listener, account);
			account_added (account_listener, account);
		} else if (strcmp (existing_account_info->name, account->name)) {

			modify_esource ("/apps/evolution/calendar/sources", existing_account_info, account, new_url);
			modify_esource ("/apps/evolution/tasks/sources", existing_account_info, account,  new_url);
			modify_esource ("/apps/evolution/memos/sources", existing_account_info, account,  new_url);
			modify_addressbook_sources (account, existing_account_info);

		}

		g_free (existing_account_info->name);
		g_free (existing_account_info->source_url);
		existing_account_info->name = g_strdup (account->name);
		existing_account_info->source_url = g_strdup (account->source->url);
		camel_url_free (old_url);
		camel_url_free (new_url);
	}
}

static void
prune_proxies (void) {

	GConfClient *client = gconf_client_get_default ();
	EAccountList *account_list;
	ESourceList *sources;
	ESourceGroup *group;
	GSList *groups, *e_sources, *l, *p;
	ESource *source;
	GError *err = NULL;
	const gchar *parent_id_name = NULL;
	gint i;
	ECalSourceType types[] = { E_CAL_SOURCE_TYPE_EVENT,
				    E_CAL_SOURCE_TYPE_TODO,
				    E_CAL_SOURCE_TYPE_JOURNAL
				  };

	account_list = e_account_list_new (client);
	/* Is this being leaked */
	g_object_unref (client);

	e_account_list_prune_proxies (account_list);

	for (i=0; i<3; i++) {
	if (e_cal_get_sources (&sources, types[i], &err)) {
		/* peek groupwise id and prune for proxies. */
		groups = e_source_list_peek_groups (sources);
		for (l = groups; l != NULL;) {
			group = (ESourceGroup *) l->data;
			l = l->next;
			if (!strcmp (e_source_group_peek_base_uri (group), "groupwise://")) {
				e_sources = e_source_group_peek_sources (group);
				for (p = e_sources; p != NULL; p = p->next) {
					source = (ESource *)p->data;
					parent_id_name = e_source_get_property (source, "parent_id_name");
					if (parent_id_name) {
						e_source_group_remove_source (group, source);
						e_source_list_remove_group (sources, group);
					}
				}
			}
		}
		e_source_list_sync (sources, NULL);
	}
	}

}
static void
camel_gw_listener_construct (CamelGwListener *config_listener)
{
	EIterator *iter;
	EAccount *account;
	GwAccountInfo *info;

	prune_proxies ();

	config_listener->priv->account_list = e_account_list_new (config_listener->priv->gconf_client);

	for (iter = e_list_get_iterator (E_LIST ( config_listener->priv->account_list) ); e_iterator_is_valid (iter); e_iterator_next (iter)) {

		account = E_ACCOUNT (e_iterator_get (iter));

		if ( is_groupwise_account (account) && account->enabled) {

			info = g_new0 (GwAccountInfo, 1);
			info->uid = g_strdup (account->uid);
			info->name = g_strdup (account->name);
			info->source_url = g_strdup (account->source->url);
			groupwise_accounts = g_list_append (groupwise_accounts, info);

		}

	}

	g_signal_connect (config_listener->priv->account_list, "account_added", G_CALLBACK (account_added), NULL);
	g_signal_connect (config_listener->priv->account_list, "account_changed", G_CALLBACK (account_changed), NULL);
	g_signal_connect (config_listener->priv->account_list, "account_removed", G_CALLBACK (account_removed), NULL);
}

GType
camel_gw_listener_get_type (void)
{
	static GType camel_gw_listener_type  = 0;

	if (!camel_gw_listener_type) {
		static GTypeInfo info = {
                        sizeof (CamelGwListenerClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) camel_gw_listener_class_init,
                        NULL, NULL,
                        sizeof (CamelGwListener),
                        0,
                        (GInstanceInitFunc) camel_gw_listener_init
                };
		camel_gw_listener_type = g_type_register_static (PARENT_TYPE, "CamelGwListener", &info, 0);
	}

	return camel_gw_listener_type;
}

CamelGwListener*
camel_gw_listener_new (void)
{
	CamelGwListener *config_listener;

	config_listener = g_object_new (CAMEL_TYPE_GW_LISTENER, NULL);
	config_listener->priv->gconf_client = gconf_client_get_default();

	camel_gw_listener_construct (config_listener);

	return config_listener;
}
