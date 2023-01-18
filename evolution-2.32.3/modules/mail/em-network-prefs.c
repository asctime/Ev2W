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
 *		Veerapuram Varadhan  <vvaradhan@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "em-network-prefs.h"

#include <gdk/gdkkeysyms.h>
#include <gconf/gconf-client.h>

#include <glib/gstdio.h>

#include "e-util/e-util.h"
#include "e-util/e-util-private.h"

#include "mail-config.h"
#include "em-config.h"
#include "em-folder-selection-button.h"

#define d(x)

#define GCONF_E_SHELL_NETWORK_CONFIG_PATH "/apps/evolution/shell/network_config/"
#define GCONF_E_HTTP_HOST_KEY GCONF_E_SHELL_NETWORK_CONFIG_PATH "http_host"
#define GCONF_E_HTTP_PORT_KEY GCONF_E_SHELL_NETWORK_CONFIG_PATH "http_port"
#define GCONF_E_HTTPS_HOST_KEY GCONF_E_SHELL_NETWORK_CONFIG_PATH "secure_host"
#define GCONF_E_HTTPS_PORT_KEY GCONF_E_SHELL_NETWORK_CONFIG_PATH "secure_port"
#define GCONF_E_SOCKS_HOST_KEY GCONF_E_SHELL_NETWORK_CONFIG_PATH "socks_host"
#define GCONF_E_SOCKS_PORT_KEY GCONF_E_SHELL_NETWORK_CONFIG_PATH "socks_port"
#define GCONF_E_IGNORE_HOSTS_KEY GCONF_E_SHELL_NETWORK_CONFIG_PATH "ignore_hosts"
#define GCONF_E_USE_AUTH_KEY GCONF_E_SHELL_NETWORK_CONFIG_PATH "use_authentication"
#define GCONF_E_PROXY_TYPE_KEY GCONF_E_SHELL_NETWORK_CONFIG_PATH "proxy_type"
#define GCONF_E_AUTH_USER_KEY GCONF_E_SHELL_NETWORK_CONFIG_PATH "authentication_user"
#define GCONF_E_AUTH_PWD_KEY  GCONF_E_SHELL_NETWORK_CONFIG_PATH "authentication_password"
#define GCONF_E_USE_PROXY_KEY GCONF_E_SHELL_NETWORK_CONFIG_PATH "use_http_proxy"
#define GCONF_E_AUTOCONFIG_URL_KEY GCONF_E_SHELL_NETWORK_CONFIG_PATH "autoconfig_url"

G_DEFINE_TYPE (
	EMNetworkPrefs,
	em_network_prefs,
	GTK_TYPE_VBOX)

static void
em_network_prefs_class_init (EMNetworkPrefsClass *class)
{
}

static void
em_network_prefs_init (EMNetworkPrefs *prefs)
{
}

static void
toggle_button_toggled (GtkToggleButton *toggle, EMNetworkPrefs *prefs)
{
	const gchar *key;

	key = g_object_get_data ((GObject *) toggle, "key");
	gconf_client_set_bool (prefs->gconf, key, gtk_toggle_button_get_active (toggle), NULL);
	if (toggle == prefs->use_auth) {
		gboolean sensitivity = gtk_toggle_button_get_active (prefs->use_auth);
		gtk_widget_set_sensitive ((GtkWidget *) prefs->lbl_auth_user, sensitivity);
		gtk_widget_set_sensitive ((GtkWidget *) prefs->lbl_auth_pwd, sensitivity);
		gtk_widget_set_sensitive ((GtkWidget *) prefs->auth_user, sensitivity);
		gtk_widget_set_sensitive ((GtkWidget *) prefs->auth_pwd, sensitivity);
	}
}

static void
toggle_button_init (EMNetworkPrefs *prefs, GtkToggleButton *toggle, const gchar *key)
{
	gboolean bool;

	bool = gconf_client_get_bool (prefs->gconf, key, NULL);
	gtk_toggle_button_set_active (toggle, bool);

	g_object_set_data ((GObject *) toggle, "key", (gpointer) key);
	g_signal_connect (toggle, "toggled", G_CALLBACK (toggle_button_toggled), prefs);

	if (!gconf_client_key_is_writable (prefs->gconf, key, NULL))
		gtk_widget_set_sensitive ((GtkWidget *) toggle, FALSE);
}

static GtkWidget *
emnp_widget_glade (EConfig *ec, EConfigItem *item, GtkWidget *parent, GtkWidget *old, gpointer data)
{
	EMNetworkPrefs *prefs = data;

	return e_builder_get_widget (prefs->builder, item->label);
}

static void
emnp_set_sensitiveness (EMNetworkPrefs *prefs, NetworkConfigProxyType type, gboolean sensitivity)
{
#if 0
	if (type == NETWORK_PROXY_AUTOCONFIG) {
		gtk_widget_set_sensitive ((GtkWidget *) prefs->auto_proxy_url, sensitivity);
		d(g_print ("Setting sensitivity of autoconfig to: %d\n", sensitivity));
	} else
#endif
	if (type == NETWORK_PROXY_MANUAL) {
		gboolean state;

		gtk_widget_set_sensitive ((GtkWidget *) prefs->http_host, sensitivity);
		gtk_widget_set_sensitive ((GtkWidget *) prefs->https_host, sensitivity);
		gtk_widget_set_sensitive ((GtkWidget *) prefs->ignore_hosts, sensitivity);
		gtk_widget_set_sensitive ((GtkWidget *) prefs->use_auth, sensitivity);
		gtk_widget_set_sensitive ((GtkWidget *) prefs->http_port, sensitivity);
		gtk_widget_set_sensitive ((GtkWidget *) prefs->https_port, sensitivity);
		gtk_widget_set_sensitive ((GtkWidget *) prefs->lbl_ignore_hosts, sensitivity);
		gtk_widget_set_sensitive ((GtkWidget *) prefs->lbl_http_host, sensitivity);
		gtk_widget_set_sensitive ((GtkWidget *) prefs->lbl_http_port, sensitivity);
		gtk_widget_set_sensitive ((GtkWidget *) prefs->lbl_https_host, sensitivity);
		gtk_widget_set_sensitive ((GtkWidget *) prefs->lbl_https_port, sensitivity);
#if 0
		gtk_widget_set_sensitive ((GtkWidget *) prefs->socks_host, sensitivity);
		gtk_widget_set_sensitive ((GtkWidget *) prefs->socks_port, sensitivity);
		gtk_widget_set_sensitive ((GtkWidget *) prefs->lbl_socks_host, sensitivity);
		gtk_widget_set_sensitive ((GtkWidget *) prefs->lbl_socks_port, sensitivity);
#endif
		state = sensitivity && gtk_toggle_button_get_active (prefs->use_auth);
		gtk_widget_set_sensitive ((GtkWidget *) prefs->lbl_auth_user, state);
		gtk_widget_set_sensitive ((GtkWidget *) prefs->lbl_auth_pwd, state);
		gtk_widget_set_sensitive ((GtkWidget *) prefs->auth_user, state);
		gtk_widget_set_sensitive ((GtkWidget *) prefs->auth_pwd, state);

		d(g_print ("Setting sensitivity of manual proxy to: %d\n", sensitivity));
	}
}

static void
notify_proxy_type_changed (GtkWidget *widget, EMNetworkPrefs *prefs)
{
	gint type;

	if (gtk_toggle_button_get_active (prefs->sys_proxy))
		type = NETWORK_PROXY_SYS_SETTINGS;
	else if (gtk_toggle_button_get_active (prefs->no_proxy))
		type = NETWORK_PROXY_DIRECT_CONNECTION;
	else if (gtk_toggle_button_get_active (prefs->manual_proxy))
		type = NETWORK_PROXY_MANUAL;
	else
#if 0
		type = NETWORK_PROXY_AUTOCONFIG;
#else
		type = NETWORK_PROXY_SYS_SETTINGS;
#endif

	gconf_client_set_int (prefs->gconf, "/apps/evolution/shell/network_config/proxy_type", type, NULL);

	if (type == NETWORK_PROXY_DIRECT_CONNECTION ||
	    type == NETWORK_PROXY_SYS_SETTINGS) {
		emnp_set_sensitiveness (prefs, NETWORK_PROXY_MANUAL, FALSE);
		emnp_set_sensitiveness (prefs, NETWORK_PROXY_AUTOCONFIG, FALSE);
	} else if (type == NETWORK_PROXY_AUTOCONFIG) {
		emnp_set_sensitiveness (prefs, NETWORK_PROXY_MANUAL, FALSE);
		emnp_set_sensitiveness (prefs, NETWORK_PROXY_AUTOCONFIG, TRUE);
	} else if (type == NETWORK_PROXY_MANUAL) {
		emnp_set_sensitiveness (prefs, NETWORK_PROXY_AUTOCONFIG, FALSE);
		emnp_set_sensitiveness (prefs, NETWORK_PROXY_MANUAL, TRUE);
	}

	if (type != NETWORK_PROXY_DIRECT_CONNECTION)
		gconf_client_set_bool (prefs->gconf, GCONF_E_USE_PROXY_KEY, TRUE, NULL);
	else if (type != NETWORK_PROXY_SYS_SETTINGS)
		gconf_client_set_bool (prefs->gconf, GCONF_E_USE_PROXY_KEY, FALSE, NULL);

}

static void
widget_entry_changed_cb (GtkWidget *widget, gpointer data)
{
	const gchar *value;
	gint port = -1;
	GConfClient *gconf = mail_config_get_gconf_client ();

	/*
	   Do not change the order of comparison -
	   GtkSpinButton is an extended form of GtkEntry
	*/
	if (GTK_IS_SPIN_BUTTON (widget)) {
		port = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (widget));
		gconf_client_set_int (gconf, (const gchar *)data, port, NULL);
		d(g_print ("%s:%s: %s is SpinButton: value = [%d]\n", G_STRLOC, G_STRFUNC, (const gchar *)data, port));
	} else if (GTK_IS_ENTRY (widget)) {
		value = gtk_entry_get_text (GTK_ENTRY (widget));
		gconf_client_set_string (gconf, (const gchar *)data, value, NULL);
		d(g_print ("%s:%s: %s is Entry: value = [%s]\n", G_STRLOC, G_STRFUNC, (const gchar *)data, value));
	}

}

static void
ignore_hosts_entry_changed_cb (GtkWidget *widget, const gchar *key)
{
	const gchar *value;
	GSList *lst = NULL;
	GConfClient *gconf;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (key != NULL);
	g_return_if_fail (GTK_IS_ENTRY (widget));

	/* transform comma-separated list of ignore_hosts to a string-list */
	value = gtk_entry_get_text (GTK_ENTRY (widget));
	if (value && *value) {
		gchar **split = g_strsplit (value, ",", -1);

		if (split) {
			gint ii;

			for (ii = 0; split[ii]; ii++) {
				const gchar *tmp = split[ii];

				if (tmp && *tmp) {
					gchar *val = g_strstrip (g_strdup (tmp));

					if (val && *val)
						lst = g_slist_append (lst, val);
					else
						g_free (val);
				}
			}
		}

		g_strfreev (split);
	}

	gconf = mail_config_get_gconf_client ();
	if (!gconf_client_set_list (gconf, key, GCONF_VALUE_STRING, lst, NULL)) {
		/* for cases where migration didn't happen, get rid of the old GConf key and "re-type" it */
		gconf_client_unset (gconf, key, NULL);
		gconf_client_set_list (gconf, key, GCONF_VALUE_STRING, lst, NULL);
	}

	g_slist_foreach (lst, (GFunc) g_free, NULL);
	g_slist_free (lst);
}

/* plugin meta-data */
static EMConfigItem emnp_items[] = {
	{ E_CONFIG_BOOK, (gchar *) "", (gchar *) "network_preferences_toplevel", emnp_widget_glade },
	{ E_CONFIG_PAGE, (gchar *) "00.general", (gchar *) "vboxNetworkGeneral", emnp_widget_glade },
	{ E_CONFIG_SECTION, (gchar *) "00.general/00.proxy", (gchar *) "frameProxy", emnp_widget_glade },
};

static void
emnp_free (EConfig *ec, GSList *items, gpointer data)
{
	/* the prefs data is freed automagically */

	g_slist_free (items);
}

static void
emnp_set_markups (EMNetworkPrefs *prefs)
{
	GtkWidget *child;

	child = gtk_bin_get_child (GTK_BIN (prefs->sys_proxy));
	gtk_label_set_use_markup (GTK_LABEL (child), TRUE);

	child = gtk_bin_get_child (GTK_BIN (prefs->no_proxy));
	gtk_label_set_use_markup (GTK_LABEL (child), TRUE);

	child = gtk_bin_get_child (GTK_BIN (prefs->manual_proxy));
	gtk_label_set_use_markup (GTK_LABEL (child), TRUE);
}

static void
em_network_prefs_construct (EMNetworkPrefs *prefs)
{
	GtkWidget *toplevel;
	GSList *l, *ignore;
	gchar *buf;
	EMConfig *ec;
	EMConfigTargetPrefs *target;
	gboolean locked;
	gint i, val, port;

	prefs->gconf = mail_config_get_gconf_client ();

	/* Make sure our custom widget classes are registered with
	 * GType before we load the GtkBuilder definition file. */
	EM_TYPE_FOLDER_SELECTION_BUTTON;

	prefs->builder = gtk_builder_new ();
	e_load_ui_builder_definition (prefs->builder, "mail-config.ui");

	/** @HookPoint-EMConfig: Network Preferences
	 * @Id: org.gnome.evolution.mail.networkPrefs
	 * @Type: E_CONFIG_BOOK
	 * @Class: org.gnome.evolution.mail.config:1.0
	 * @Target: EMConfigTargetPrefs
	 *
	 * The network preferences settings page.
	 */
	ec = em_config_new(E_CONFIG_BOOK, "org.gnome.evolution.mail.networkPrefs");
	l = NULL;
	for (i = 0; i < G_N_ELEMENTS (emnp_items); i++)
		l = g_slist_prepend (l, &emnp_items[i]);
	e_config_add_items ((EConfig *)ec, l, NULL, NULL, emnp_free, prefs);

	/* Proxy tab */

	/* Default Behavior */
	locked = !gconf_client_key_is_writable (prefs->gconf, GCONF_E_PROXY_TYPE_KEY, NULL);

	val = gconf_client_get_int (prefs->gconf, GCONF_E_PROXY_TYPE_KEY, NULL);

	/* no auto-proxy at the moment */
	if (val == NETWORK_PROXY_AUTOCONFIG)
		val = NETWORK_PROXY_SYS_SETTINGS;

	prefs->sys_proxy = GTK_TOGGLE_BUTTON (e_builder_get_widget (prefs->builder, "rdoSysSettings"));
	gtk_toggle_button_set_active (prefs->sys_proxy, val == NETWORK_PROXY_SYS_SETTINGS);
	if (locked)
		gtk_widget_set_sensitive ((GtkWidget *) prefs->sys_proxy, FALSE);

	d(g_print ("Sys settings ----!!! \n"));

	prefs->no_proxy = GTK_TOGGLE_BUTTON (e_builder_get_widget (prefs->builder, "rdoNoProxy"));
	gtk_toggle_button_set_active (prefs->no_proxy, val == NETWORK_PROXY_DIRECT_CONNECTION);
	if (locked)
		gtk_widget_set_sensitive ((GtkWidget *) prefs->no_proxy, FALSE);

	d(g_print ("No proxy settings ----!!! \n"));

	/* no auto-proxy at the moment */
#if 0
	prefs->auto_proxy = GTK_TOGGLE_BUTTON (e_builder_get_widget (prefs->builder, "rdoAutoConfig"));
	prefs->auto_proxy_url = GTK_ENTRY (e_builder_get_widget (prefs->builder, "txtAutoConfigUrl"));

	gtk_toggle_button_set_active (prefs->auto_proxy, val == NETWORK_PROXY_AUTOCONFIG);

	g_signal_connect(prefs->auto_proxy_url, "changed", G_CALLBACK(widget_entry_changed_cb), GCONF_E_AUTOCONFIG_URL_KEY);
	if (locked)
		gtk_widget_set_sensitive ((GtkWidget *) prefs->auto_proxy, FALSE);
#endif

	d(g_print ("Auto config settings ----!!! \n"));

	prefs->manual_proxy = GTK_TOGGLE_BUTTON (e_builder_get_widget (prefs->builder, "rdoManualProxy"));
	prefs->http_host = GTK_ENTRY (e_builder_get_widget (prefs->builder, "txtHttpHost"));
	prefs->https_host = GTK_ENTRY (e_builder_get_widget (prefs->builder, "txtHttpsHost"));
	prefs->ignore_hosts = GTK_ENTRY (e_builder_get_widget (prefs->builder, "txtIgnoreHosts"));
	prefs->http_port = GTK_SPIN_BUTTON (e_builder_get_widget (prefs->builder, "spnHttpPort"));
	prefs->https_port = GTK_SPIN_BUTTON (e_builder_get_widget (prefs->builder, "spnHttpsPort"));
	prefs->lbl_http_host = GTK_LABEL (e_builder_get_widget (prefs->builder, "lblHttpHost"));
	prefs->lbl_http_port = GTK_LABEL (e_builder_get_widget (prefs->builder, "lblHttpPort"));
	prefs->lbl_https_host = GTK_LABEL (e_builder_get_widget (prefs->builder, "lblHttpsHost"));
	prefs->lbl_https_port = GTK_LABEL (e_builder_get_widget (prefs->builder, "lblHttpsPort"));
	prefs->lbl_ignore_hosts = GTK_LABEL (e_builder_get_widget (prefs->builder, "lblIgnoreHosts"));
	prefs->use_auth = GTK_TOGGLE_BUTTON (e_builder_get_widget (prefs->builder, "chkUseAuth"));
	toggle_button_init (prefs, prefs->use_auth, GCONF_E_USE_AUTH_KEY);
	prefs->lbl_auth_user = GTK_LABEL (e_builder_get_widget (prefs->builder, "lblAuthUser"));
	prefs->lbl_auth_pwd = GTK_LABEL (e_builder_get_widget (prefs->builder, "lblAuthPwd"));
	prefs->auth_user = GTK_ENTRY (e_builder_get_widget (prefs->builder, "txtAuthUser"));
	prefs->auth_pwd = GTK_ENTRY (e_builder_get_widget (prefs->builder, "txtAuthPwd"));

#if 0
	prefs->socks_host = GTK_ENTRY (e_builder_get_widget (prefs->builder, "txtSocksHost"));
	prefs->socks_port = GTK_SPIN_BUTTON (e_builder_get_widget (prefs->builder, "spnSocksPort"));
	prefs->lbl_socks_host = GTK_LABEL (e_builder_get_widget (prefs->builder, "lblSocksHost"));
	prefs->lbl_socks_port = GTK_LABEL (e_builder_get_widget (prefs->builder, "lblSocksPort"));
	g_signal_connect (prefs->socks_host, "changed",
			  G_CALLBACK (widget_entry_changed_cb), GCONF_E_SOCKS_HOST_KEY);
	g_signal_connect (prefs->socks_port, "value_changed",
			  G_CALLBACK (widget_entry_changed_cb), GCONF_E_SOCKS_PORT_KEY);
#endif

	/* Manual proxy options */
	g_signal_connect (prefs->http_host, "changed",
			  G_CALLBACK (widget_entry_changed_cb),
			  (gpointer) GCONF_E_HTTP_HOST_KEY);
	g_signal_connect (prefs->https_host, "changed",
			  G_CALLBACK (widget_entry_changed_cb),
			  (gpointer) GCONF_E_HTTPS_HOST_KEY);
	g_signal_connect (prefs->ignore_hosts, "changed",
			  G_CALLBACK (ignore_hosts_entry_changed_cb),
			  (gpointer) GCONF_E_IGNORE_HOSTS_KEY);
	g_signal_connect (prefs->http_port, "value_changed",
			  G_CALLBACK (widget_entry_changed_cb),
			  (gpointer) GCONF_E_HTTP_PORT_KEY);
	g_signal_connect (prefs->https_port, "value_changed",
			  G_CALLBACK (widget_entry_changed_cb),
			  (gpointer) GCONF_E_HTTPS_PORT_KEY);
	g_signal_connect (prefs->auth_user, "changed",
			  G_CALLBACK (widget_entry_changed_cb),
			  (gpointer) GCONF_E_AUTH_USER_KEY);
	g_signal_connect (prefs->auth_pwd, "changed",
			  G_CALLBACK (widget_entry_changed_cb),
			  (gpointer) GCONF_E_AUTH_PWD_KEY);

	gtk_toggle_button_set_active (prefs->manual_proxy, val == NETWORK_PROXY_MANUAL);
	g_signal_connect (prefs->sys_proxy, "toggled", G_CALLBACK (notify_proxy_type_changed), prefs);
	g_signal_connect (prefs->no_proxy, "toggled", G_CALLBACK (notify_proxy_type_changed), prefs);
#if 0
	g_signal_connect (prefs->auto_proxy, "toggled", G_CALLBACK (notify_proxy_type_changed), prefs);
#endif
	g_signal_connect (prefs->manual_proxy, "toggled", G_CALLBACK (notify_proxy_type_changed), prefs);

	if (locked)
		gtk_widget_set_sensitive ((GtkWidget *) prefs->manual_proxy, FALSE);
	d(g_print ("Manual settings ----!!! \n"));

	buf = gconf_client_get_string (prefs->gconf, GCONF_E_HTTP_HOST_KEY, NULL);
	gtk_entry_set_text (prefs->http_host, buf ? buf : "");
	g_free (buf);

	buf = gconf_client_get_string (prefs->gconf, GCONF_E_HTTPS_HOST_KEY, NULL);
	gtk_entry_set_text (prefs->https_host, buf ? buf : "");
	g_free (buf);

	buf = NULL;
	ignore = gconf_client_get_list (prefs->gconf, GCONF_E_IGNORE_HOSTS_KEY, GCONF_VALUE_STRING, NULL);
	if (ignore) {
		/* reconstruct comma-separated list */
		GSList *sl;
		GString *str = NULL;

		for (sl = ignore; sl; sl = sl->next) {
			const gchar *value = sl->data;

			if (value && *value) {
				if (!str) {
					str = g_string_new (value);
				} else {
					g_string_append (str, ",");
					g_string_append (str, value);
				}
			}
		}

		g_slist_foreach (ignore, (GFunc) g_free, NULL);
		g_slist_free (ignore);

		if (str)
			buf = g_string_free (str, FALSE);
	}

	gtk_entry_set_text (prefs->ignore_hosts, buf ? buf : "");
	g_free (buf);

	buf = gconf_client_get_string (prefs->gconf, GCONF_E_AUTH_USER_KEY, NULL);
	gtk_entry_set_text (prefs->auth_user, buf ? buf : "");
	g_free (buf);

	buf = gconf_client_get_string (prefs->gconf, GCONF_E_AUTH_PWD_KEY, NULL);
	gtk_entry_set_text (prefs->auth_pwd, buf ? buf : "");
	g_free (buf);

	port = gconf_client_get_int (prefs->gconf, GCONF_E_HTTP_PORT_KEY, NULL);
	gtk_spin_button_set_value (prefs->http_port, (gdouble)port);

	port = gconf_client_get_int (prefs->gconf, GCONF_E_HTTPS_PORT_KEY, NULL);
	gtk_spin_button_set_value (prefs->https_port, (gdouble)port);

#if 0
	buf = gconf_client_get_string (prefs->gconf, GCONF_E_SOCKS_HOST_KEY, NULL);
	gtk_entry_set_text (prefs->socks_host, buf ? buf : "");
	g_free (buf);

	port = gconf_client_get_int (prefs->gconf, GCONF_E_SOCKS_PORT_KEY, NULL);
	gtk_spin_button_set_value (prefs->socks_port, (gdouble)port);
#endif
	emnp_set_markups (prefs);

	if (val == NETWORK_PROXY_DIRECT_CONNECTION ||
	    val == NETWORK_PROXY_SYS_SETTINGS) {
		emnp_set_sensitiveness (prefs, NETWORK_PROXY_MANUAL, FALSE);
		emnp_set_sensitiveness (prefs, NETWORK_PROXY_AUTOCONFIG, FALSE);
	} else if (val == NETWORK_PROXY_AUTOCONFIG) {
		emnp_set_sensitiveness (prefs, NETWORK_PROXY_MANUAL, FALSE);
		emnp_set_sensitiveness (prefs, NETWORK_PROXY_AUTOCONFIG, TRUE);
	} else if (val == NETWORK_PROXY_MANUAL) {
		emnp_set_sensitiveness (prefs, NETWORK_PROXY_AUTOCONFIG, FALSE);
		emnp_set_sensitiveness (prefs, NETWORK_PROXY_MANUAL, TRUE);
	}

	/* get our toplevel widget */
	target = em_config_target_new_prefs (ec, prefs->gconf);
	e_config_set_target ((EConfig *)ec, (EConfigTarget *)target);
	toplevel = e_config_create_widget ((EConfig *)ec);
	gtk_container_add (GTK_CONTAINER (prefs), toplevel);
}

GtkWidget *
em_network_prefs_new (EPreferencesWindow *window)
{
	EMNetworkPrefs *new;

	new = (EMNetworkPrefs *) g_object_new (em_network_prefs_get_type (), NULL);
	em_network_prefs_construct (new);

	return (GtkWidget *) new;
}
