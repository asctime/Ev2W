/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Copyright (C) 2001-2004 Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* ExchangeShareConfigListener: a class that listens to the config database
 * and handles creating the ExchangeAccount object and making sure that
 * default folders are updated as needed.
 */

#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <camel/camel.h>
#include <libedataserver/e-source.h>
#include <libedataserver/e-source-list.h>
#include <libedataserver/e-source-group.h>
#include <libedataserver/e-xml-utils.h>
#include <libedataserver/e-xml-hash-utils.h>

#include <e2k-marshal.h>
#include <e2k-uri.h>
#include <exchange-account.h>
#include <e-folder-exchange.h>

#include "exchange-share-config-listener.h"

struct _ExchangeShareConfigListenerPrivate {
	GConfClient *gconf;

	gchar *configured_uri, *configured_name;
	EAccount *configured_account;

	ExchangeAccount *exchange_account;
};

typedef struct {
	const gchar *name;
	const gchar *uri;
	gint type;
}FolderInfo;

enum {
	EXCHANGE_ACCOUNT_CREATED,
	EXCHANGE_ACCOUNT_REMOVED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

#define PARENT_TYPE E_TYPE_ACCOUNT_LIST

#define CONF_KEY_SELECTED_CAL_SOURCES "/apps/evolution/calendar/display/selected_calendars"
#define CONF_KEY_SELECTED_TASKS_SOURCES "/apps/evolution/calendar/tasks/selected_tasks"

static GStaticMutex ecl_mutex = G_STATIC_MUTEX_INIT;

static EAccountListClass *parent_class = NULL;

static void dispose (GObject *object);
static void finalize (GObject *object);

static void account_added   (EAccountList *account_listener,
			     EAccount     *account);
static void account_changed (EAccountList *account_listener,
			     EAccount     *account);
static void account_removed (EAccountList *account_listener,
			     EAccount     *account);

static gboolean exchange_camel_urls_is_equal (const gchar *url1,
					      const gchar *url2);

static void
class_init (GObjectClass *object_class)
{
	EAccountListClass *e_account_list_class =
		E_ACCOUNT_LIST_CLASS (object_class);

	parent_class = g_type_class_ref (PARENT_TYPE);

	/* virtual method override */
	object_class->dispose = dispose;
	object_class->finalize = finalize;

	e_account_list_class->account_added   = account_added;
	e_account_list_class->account_changed = account_changed;
	e_account_list_class->account_removed = account_removed;

	/* signals */
	signals[EXCHANGE_ACCOUNT_CREATED] =
		g_signal_new ("exchange_account_created",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ExchangeShareConfigListenerClass, exchange_account_created),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);
	signals[EXCHANGE_ACCOUNT_REMOVED] =
		g_signal_new ("exchange_account_removed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ExchangeShareConfigListenerClass, exchange_account_removed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);
}

static void
init (GObject *object)
{
	ExchangeShareConfigListener *config_listener =
		EXCHANGE_SHARE_CONFIG_LISTENER (object);

	config_listener->priv = g_new0 (ExchangeShareConfigListenerPrivate, 1);
}

static void
dispose (GObject *object)
{
	ExchangeShareConfigListener *config_listener =
		EXCHANGE_SHARE_CONFIG_LISTENER (object);

	if (config_listener->priv->gconf) {
		g_object_unref (config_listener->priv->gconf);
		config_listener->priv->gconf = NULL;
	}

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
finalize (GObject *object)
{
	ExchangeShareConfigListener *config_listener =
		EXCHANGE_SHARE_CONFIG_LISTENER (object);

	g_free (config_listener->priv->configured_name);
	g_free (config_listener->priv->configured_uri);
	g_free (config_listener->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

E2K_MAKE_TYPE (exchange_share_config_listener, ExchangeShareConfigListener, class_init, init, PARENT_TYPE)

#define EVOLUTION_URI_PREFIX     "evolution:/"
#define EVOLUTION_URI_PREFIX_LEN (sizeof (EVOLUTION_URI_PREFIX) - 1)

static gboolean
is_active_exchange_account (EAccount *account)
{
	if (!account->enabled)
		return FALSE;
	if (!account->source || !account->source->url)
		return FALSE;
	return (strncmp (account->source->url, EXCHANGE_URI_PREFIX, 11) == 0);
}

static void
update_foreign_uri (const gchar *path, const gchar *account_uri)
{
	gchar *file_path, *phy_uri, *foreign_uri, *new_phy_uri;
	GHashTable *old_props = NULL;
	xmlDoc *old_doc = NULL, *new_doc = NULL;

	if (!path)
		return;

	file_path = g_build_filename (path, "hierarchy.xml", NULL);
	if (!g_file_test (file_path, G_FILE_TEST_EXISTS))
		goto cleanup;

	old_doc = e_xml_parse_file (file_path);
	if (!old_doc)
		goto cleanup;

	old_props = e_xml_to_hash (old_doc, E_XML_HASH_TYPE_PROPERTY);
	xmlFreeDoc (old_doc);

	phy_uri = g_hash_table_lookup (old_props, "physical_uri_prefix");
	if (!phy_uri)
		goto cleanup;

	foreign_uri = strstr (phy_uri, "://");
	if (!foreign_uri)
		goto cleanup;
	foreign_uri = strchr (foreign_uri + 3, '/');
	if (!foreign_uri)
		goto cleanup;

	if ((foreign_uri + 1) && (*(foreign_uri + 1) == ';'))
		goto cleanup;

	new_phy_uri = g_strdup_printf ("exchange://%s/;%s", account_uri, foreign_uri + 1);
	g_hash_table_remove (old_props, "physical_uri_prefix");
	g_hash_table_insert (old_props, (gchar *)g_strdup ("physical_uri_prefix"), new_phy_uri);

	new_doc = e_xml_from_hash (old_props, E_XML_HASH_TYPE_PROPERTY, "foreign-hierarchy");
	e_xml_save_file (file_path, new_doc);

	xmlFreeDoc (new_doc);
	g_free (new_phy_uri);
cleanup:
	g_free (file_path);
	if (old_props)
		e_xml_destroy_hash (old_props);
	return;
}

static void
migrate_foreign_hierarchy (ExchangeAccount *account)
{
	GDir *d;
	const gchar *dentry;
	gchar *dir;

	d = g_dir_open (account->storage_dir, 0, NULL);
	if (d) {
		while ((dentry = g_dir_read_name (d))) {
			if (!strchr (dentry, '@'))
				continue;
			dir = g_strdup_printf ("%s/%s", account->storage_dir,
							dentry);
			update_foreign_uri (dir, account->account_filename);
			g_free (dir);
		}
		g_dir_close (d);
	}
}

static void
ex_set_relative_uri (ESource *source, const gchar *url)
{
	const gchar *rel_uri = e_source_peek_relative_uri (source);
	gchar *folder_name;
	gchar *new_rel_uri;

	if (!rel_uri)
		return;

	folder_name = strchr (rel_uri, '/');
	if (!folder_name)
		return;

	if ((folder_name + 1) && *(folder_name + 1) == ';')
		return;

	new_rel_uri = g_strdup_printf ("%s;%s", url, folder_name + 1);
	e_source_set_relative_uri (source, new_rel_uri);
	g_free (new_rel_uri);
}

static void
migrate_account_esource (EAccount *account,
			FolderType folder_type)
{
	ESourceGroup *group;
	ESource *source = NULL;
	GSList *groups;
	GSList *sources;
	gboolean found_group;
	const gchar *user_name, *authtype;
	GConfClient *client;
	ESourceList *source_list = NULL;
	CamelURL *camel_url;
	gchar *url_string;

	camel_url = camel_url_new (account->source->url, NULL);
	if (!camel_url)
		return;
	user_name = camel_url->user;
	authtype  = camel_url->authmech;
	url_string = camel_url_to_string (camel_url, CAMEL_URL_HIDE_PASSWORD | CAMEL_URL_HIDE_PARAMS);

	if (!user_name) {
		g_free (url_string);
		camel_url_free (camel_url);
		return;
	}

	client = gconf_client_get_default ();

	if (folder_type == EXCHANGE_CONTACTS_FOLDER)
		source_list = e_source_list_new_for_gconf ( client,
							CONF_KEY_CONTACTS);
	else if (folder_type == EXCHANGE_CALENDAR_FOLDER)
		source_list = e_source_list_new_for_gconf ( client,
							CONF_KEY_CAL);
	else if (folder_type == EXCHANGE_TASKS_FOLDER)
		source_list = e_source_list_new_for_gconf ( client,
							CONF_KEY_TASKS);

	groups = e_source_list_peek_groups (source_list);
	found_group = FALSE;

	for (; groups != NULL && !found_group; groups = g_slist_next (groups)) {
		group = E_SOURCE_GROUP (groups->data);

		if (strcmp (e_source_group_peek_name (group), account->name) == 0
                    &&
                    strcmp (e_source_group_peek_base_uri (group), EXCHANGE_URI_PREFIX) == 0) {
			sources = e_source_group_peek_sources (group);

			found_group = TRUE;
			for (; sources != NULL; sources = g_slist_next (sources)) {
				source = E_SOURCE (sources->data);

				ex_set_relative_uri (source, url_string + strlen ("exchange://"));
				e_source_set_property (source, "username", user_name);
				e_source_set_property (source, "auth-domain", "Exchange");
				if (authtype)
					e_source_set_property (source, "auth-type", authtype);
				if (folder_type == EXCHANGE_CONTACTS_FOLDER)
					e_source_set_property (source, "auth", "plain/password");
				else
					e_source_set_property (source, "auth", "1");
				e_source_list_sync (source_list, NULL);
			}
		}
	}
	g_free (url_string);
	camel_url_free (camel_url);
	g_object_unref (source_list);
	g_object_unref (client);
}

void
exchange_share_config_listener_migrate_esources (ExchangeShareConfigListener *config_listener)
{
	EAccount *account;

	g_return_if_fail (config_listener != NULL);

	account = config_listener->priv->configured_account;
	migrate_account_esource (account, EXCHANGE_CALENDAR_FOLDER);
	migrate_account_esource (account, EXCHANGE_TASKS_FOLDER);
	migrate_account_esource (account, EXCHANGE_CONTACTS_FOLDER);
	migrate_foreign_hierarchy (config_listener->priv->exchange_account);
}

static void
account_added (EAccountList *account_list, EAccount *account)
{
	ExchangeShareConfigListener *config_listener;
	ExchangeAccount *exchange_account;

	if (!is_active_exchange_account (account))
		return;

	config_listener = EXCHANGE_SHARE_CONFIG_LISTENER (account_list);
	if (config_listener->priv->configured_account) {
		/* Multiple accounts configured. */
		return;
	}

	/* New account! Yippee! */
	exchange_account = exchange_account_new (account_list, account);
	if (!exchange_account) {
		g_warning ("Could not parse exchange uri '%s'",
			   account->source->url);
		return;
	}

	config_listener->priv->exchange_account = exchange_account;
	config_listener->priv->configured_account = account;

	g_free (config_listener->priv->configured_uri);
	config_listener->priv->configured_uri = g_strdup (account->source->url);
	g_free (config_listener->priv->configured_name);
	config_listener->priv->configured_name = g_strdup (account->name);

	g_signal_emit (config_listener, signals[EXCHANGE_ACCOUNT_CREATED], 0,
		       exchange_account);
	exchange_share_config_listener_migrate_esources (config_listener);
}

struct account_update_data {
	EAccountList *account_list;
	EAccount *account;
};

static void
configured_account_destroyed (gpointer user_data, GObject *where_account_was)
{
	struct account_update_data *aud = user_data;

	if (!EXCHANGE_SHARE_CONFIG_LISTENER (aud->account_list)->priv->configured_account)
		account_added (aud->account_list, aud->account);

	g_object_unref (aud->account_list);
	g_object_unref (aud->account);
	g_free (aud);
}

static gboolean
requires_relogin (gchar *current_url, gchar *new_url)
{
	E2kUri *current_uri, *new_uri;
	const gchar *current_param_val, *new_param_val;
	const gchar *params [] = { "owa_url", "ad_server", "use_ssl" };
	const gint n_params = G_N_ELEMENTS (params);
	gint i;
	gboolean relogin = FALSE;

	current_uri = e2k_uri_new (current_url);
	new_uri = e2k_uri_new (new_url);

	if (strcmp (current_uri->user, new_uri->user) ||
	    strcmp (current_uri->host, new_uri->host)) {
		relogin = TRUE;
		goto end;
	}

	if (current_uri->authmech || new_uri->authmech) {
		if (current_uri->authmech && new_uri->authmech) {
			if (strcmp (current_uri->authmech, new_uri->authmech)) {
				/* Auth mechanism has changed */
				relogin = TRUE;
				goto end;
			}
		}
		else {
			/* Auth mechanism is set for the first time */
			relogin = TRUE;
			goto end;
		}
	}

	for (i=0; i<n_params; i++) {
		current_param_val = e2k_uri_get_param (current_uri, params[i]);
		new_param_val = e2k_uri_get_param (new_uri, params[i]);

		if (current_param_val && new_param_val) {
			/* both the urls have params to be compared */
			if (strcmp (current_param_val, new_param_val)) {
				relogin = TRUE;
				break;
			}
		}
		else if (current_param_val || new_param_val) {
			/* check for added or deleted parameter */
			relogin = TRUE;
			break;
		}
	}
end:
	e2k_uri_free (new_uri);
	e2k_uri_free (current_uri);
	return relogin;
}

static void
account_changed (EAccountList *account_list, EAccount *account)
{
	ExchangeShareConfigListener *config_listener =
		EXCHANGE_SHARE_CONFIG_LISTENER (account_list);
	ExchangeShareConfigListenerPrivate *priv = config_listener->priv;

	if (account != config_listener->priv->configured_account) {
		if (!is_active_exchange_account (account))
			return;

		/* The user has converted an existing non-Exchange
		 * account to an Exchange account, so treat it like an
		 * add.
		 */
		account_added (account_list, account);
		return;
	} else if (!is_active_exchange_account (account)) {
		/* The user has disabled the Exchange account or
		 * converted it to non-Exchange, so treat it like a
		 * remove.
		 */
		account_removed (account_list, account);
		return;
	}

	/* FIXME: The order of the parameters in the Camel URL string is not in
	 * order for the two given strings. So, we will not be able to use
	 * plain string comparison. Instead compare the parameters one by one.
	 */
	if (exchange_camel_urls_is_equal (config_listener->priv->configured_uri,
					  account->source->url) &&
	    !strcmp (config_listener->priv->configured_name, account->name)) {
		/* The user changed something we don't care about. */
		return;
	}

	/* OK, so he modified the active account in a way we care
	 * about. If the user hasn't connected yet, we're still ok.
	 */
	if (!exchange_account_get_context (config_listener->priv->exchange_account)) {
		/* Good. Remove the current account, and wait for it
		 * to actually go away (which may not happen immediately
		 * since there may be a function higher up on the stack
		 * still holding a ref on it). Then create the new one.
		 * (We have to wait for it to go away because the new
		 * storage probably still has the same name as the old
		 * one, so trying to create it before the old one is
		 * removed would fail.)
		 */
		struct account_update_data *aud;

		aud = g_new (struct account_update_data, 1);
		aud->account = g_object_ref (account);
		aud->account_list = g_object_ref (account_list);
		g_object_weak_ref (G_OBJECT (config_listener->priv->exchange_account), configured_account_destroyed, aud);

		account_removed (account_list, account);
		return;
	}

	/* If account name has changed, or the url value has changed, which
	 * could be due to change in hostname or some parameter value,
	 * remove old e-sources
	 */
	if (requires_relogin (config_listener->priv->configured_uri,
			      account->source->url)) {
		exchange_account_forget_password (priv->exchange_account);
	} else if (strcmp (config_listener->priv->configured_name, account->name)) {
		g_free (config_listener->priv->configured_name);
		config_listener->priv->configured_name = g_strdup (account->name);
		return;
	} else {
		/* FIXME: Do ESources need to be modified? */
		return;
	}

	/* But note the new URI so if he changes something else, we
	 * only warn again if he changes again.
	 */
	g_free (config_listener->priv->configured_uri);
	config_listener->priv->configured_uri = g_strdup (account->source->url);
}

static void
account_removed (EAccountList *account_list, EAccount *account)
{
	ExchangeShareConfigListener *config_listener =
		EXCHANGE_SHARE_CONFIG_LISTENER (account_list);
	ExchangeShareConfigListenerPrivate *priv = config_listener->priv;

	if (account != priv->configured_account)
		return;

	exchange_account_forget_password (priv->exchange_account);

	if (!exchange_account_get_context (priv->exchange_account)) {
		/* The account isn't connected yet, so we can destroy
		 * it without problems.
		 */
		g_signal_emit (config_listener,
			       signals[EXCHANGE_ACCOUNT_REMOVED], 0,
			       priv->exchange_account);

		priv->configured_account = NULL;
		g_free (priv->configured_uri);
		priv->configured_uri = NULL;
		g_free (priv->configured_name);
		priv->configured_name = NULL;
	}
}

/**
 * exchange_share_config_listener_new:
 *
 * This creates and returns a new #ExchangeShareConfigListener, which
 * monitors GConf and creates and (theoretically) destroys accounts
 * accordingly. It will emit an %account_created signal when a new
 * account is created (or shortly after the listener itself is created
 * if an account already exists).
 *
 * Due to various constraints, the user is currently limited to a
 * single account, and it is not possible to destroy an existing
 * account. Thus, the %account_created signal will never be emitted
 * more than once currently.
 *
 * Return value: the new config listener.
 **/
ExchangeShareConfigListener *
exchange_share_config_listener_new (void)
{
	ExchangeShareConfigListener *config_listener;

	config_listener = g_object_new (EXCHANGE_TYPE_SHARE_CONFIG_LISTENER, NULL);
	config_listener->priv->gconf = gconf_client_get_default ();

	e_account_list_construct (E_ACCOUNT_LIST (config_listener),
				  config_listener->priv->gconf);

	return config_listener;
}

GSList *
exchange_share_config_listener_get_accounts (ExchangeShareConfigListener *config_listener)
{
	g_return_val_if_fail (EXCHANGE_IS_SHARE_CONFIG_LISTENER (config_listener), NULL);

	if (config_listener->priv->exchange_account)
		return g_slist_append (NULL, config_listener->priv->exchange_account);
	else
		return NULL;
}

/**
 * exchange_camel_urls_is_equal
 *
 * @url1: CAMEL URL string 1
 * @url2: CAMEL URL string 2
 *
 * This function checks if the parameters present in two given CAMEL URLS are
 * identical and returns the result.
 *
 * Return Value: Boolean result of the comparision.
 *
 **/
static gboolean
exchange_camel_urls_is_equal (const gchar *url1, const gchar *url2)
{
	CamelURL *curl1, *curl2;
	const gchar *param1, *param2;
	const gchar *params[] = {
		"auth",
		"owa_url",
		"owa_path",
		"mailbox",
		"ad_server",
	};
	const gint n_params = 5;
	gint i;

	curl1 = camel_url_new (url1, NULL);
	curl2 = camel_url_new (url2, NULL);

	for (i = 0; i < n_params; ++i) {
		param1 = (gchar *) camel_url_get_param (curl1, params[i]);
		param2 = (gchar *) camel_url_get_param (curl2, params[i]);
		if ((param1 && !param2) || (!param1 && param2) || /* Missing */
		    (param1 && param2 && strcmp (param1, param2))) { /* Differing */
			camel_url_free (curl1);
			camel_url_free (curl2);
			return FALSE;
		}
	}
	camel_url_free (curl1);
	camel_url_free (curl2);
	return TRUE;
}

struct create_excl_struct
{
	ExchangeShareConfigListener **excl;
	GMutex *mutex;
	GCond *done;
};

static gboolean
create_excl_in_main_thread (gpointer data)
{
	struct create_excl_struct *ces = (struct create_excl_struct *) data;

	g_return_val_if_fail (data != NULL, FALSE);

	g_mutex_lock (ces->mutex);

	*ces->excl = exchange_share_config_listener_new ();
	g_cond_signal (ces->done);

	g_mutex_unlock (ces->mutex);

	return FALSE;
}

ExchangeShareConfigListener *
exchange_share_config_listener_get_global (void)
{
	static ExchangeShareConfigListener *excl = NULL;

	g_static_mutex_lock (&ecl_mutex);
	if (!excl) {
		if (!g_main_context_is_owner (g_main_context_default ())) {
			/* it is called from a thread, do the creation in a main thread;
			   every other call will wait until it is done */
			struct create_excl_struct ces;

			ces.excl = &excl;
			ces.mutex = g_mutex_new ();
			ces.done = g_cond_new ();

			g_mutex_lock (ces.mutex);
			g_timeout_add (1, create_excl_in_main_thread, &ces);
			g_cond_wait (ces.done, ces.mutex);
			g_mutex_unlock (ces.mutex);

			g_mutex_free (ces.mutex);
			g_cond_free (ces.done);
		} else {
			excl = exchange_share_config_listener_new ();
		}
	}
	g_static_mutex_unlock (&ecl_mutex);

	return excl;
}

ExchangeAccount *
exchange_share_config_listener_get_account_for_uri (ExchangeShareConfigListener *excl, const gchar *uri)
{
	GSList *accounts, *a;
	ExchangeAccount *res = NULL;

	if (!excl)
		excl = exchange_share_config_listener_get_global ();

	g_return_val_if_fail (excl != NULL, NULL);

	accounts = exchange_share_config_listener_get_accounts (excl);

	/* FIXME the hack to return at least something */
	if (g_slist_length (accounts) == 1) {
		res = accounts->data;
		g_slist_free (accounts);
		return res;
	}

	for (a = accounts; a; a = a->next) {
		ExchangeAccount *account = a->data;

		g_return_val_if_fail (account != NULL, NULL);

		/* Kludge for while we don't support multiple accounts */
		if (!uri) {
			res = account;
			break;
		}

		if (exchange_account_get_folder (account, uri)) {
			res = account;
			break;
		} else {
			g_static_mutex_lock (&ecl_mutex);
			exchange_account_rescan_tree (account);
			g_static_mutex_unlock (&ecl_mutex);
			if (exchange_account_get_folder (account, uri)) {
				res = account;
				break;
			}
		}
	}

	g_slist_free (accounts);

	return res;
}
