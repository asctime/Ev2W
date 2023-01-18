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
 *		Chenthill Palanisamy <pchenthill@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include "mail/em-account-editor.h"
#include "mail/em-config.h"
#include "libedataserver/e-account.h"
#include <misc/e-send-options.h>
#include <mail/em-config.h>
#include <e-gw-connection.h>
#include <libecal/e-cal-time-util.h>
#include <libedataserver/e-source-list.h>
#include <libedataserverui/e-passwords.h>

ESendOptionsDialog *sod = NULL;
GtkWidget *parent;
EGwConnection *n_cnc;
EGwSendOptions *opts = NULL;
gboolean changed = FALSE;
EAccount *account;

GtkWidget* org_gnome_send_options (EPlugin *epl, EConfigHookItemFactoryData *data);
void send_options_commit (EPlugin *epl, EConfigHookItemFactoryData *data);
void send_options_changed (EPlugin *epl, EConfigHookItemFactoryData *data);
void send_options_abort (EPlugin *epl, EConfigHookItemFactoryData *data);

static EGwConnection *
get_cnc (GtkWindow *parent_window)
{
	EGwConnection *cnc;
	const gchar *failed_auth;
	gchar *uri, *key, *prompt, *password = NULL;
	CamelURL *url;
	const gchar *poa_address, *use_ssl, *soap_port;
	gboolean remember;

	url = camel_url_new (account->source->url, NULL);
	if (url == NULL)
		return NULL;
	poa_address = url->host;
	if (!poa_address || strlen (poa_address) ==0)
		return NULL;

        soap_port = camel_url_get_param (url, "soap_port");
        if (!soap_port || strlen (soap_port) == 0)
                soap_port = "7191";
	use_ssl = camel_url_get_param (url, "use_ssl");

	key =  g_strdup_printf ("groupwise://%s@%s/", url->user, poa_address);

	if (use_ssl && !g_str_equal (use_ssl, "never"))
		uri = g_strdup_printf ("https://%s:%s/soap", poa_address, soap_port);
	else
		uri = g_strdup_printf ("http://%s:%s/soap", poa_address, soap_port);

	failed_auth = "";
	cnc = NULL;

	prompt = g_strdup_printf (_("%sEnter password for %s (user %s)"),
			failed_auth, poa_address, url->user);

	password = e_passwords_get_password ("Groupwise", key);
	if (!password)
		password = e_passwords_ask_password (prompt, "Groupwise", key, prompt,
				E_PASSWORDS_REMEMBER_FOREVER|E_PASSWORDS_SECRET, &remember, parent_window);
	g_free (prompt);

	cnc = e_gw_connection_new (uri, url->user, password);
	if (!E_IS_GW_CONNECTION(cnc) && use_ssl && g_str_equal (use_ssl, "when-possible")) {
		gchar *http_uri = g_strconcat ("http://", uri + 8, NULL);
		cnc = e_gw_connection_new (http_uri, url->user, password);
		g_free (http_uri);
	}

	camel_url_free (url);
	return cnc;
}

static void
e_send_options_load_general_opts (ESendOptionsGeneral *gopts,
                                  EGwSendOptionsGeneral *ggopts)
{
	time_t temp;

	temp = time (NULL);

	gopts->priority = ggopts->priority;

	gopts->reply_enabled = ggopts->reply_enabled;
	gopts->reply_convenient = ggopts->reply_convenient;
	gopts->reply_within = ggopts->reply_within;

	gopts->expiration_enabled = ggopts->expiration_enabled;
	gopts->expire_after = ggopts->expire_after;

	gopts->delay_enabled = ggopts->delay_enabled;

	/* TODO convert gint to timet comparing the current day */
	if (ggopts->delay_until) {
		gopts->delay_until = time_add_day_with_zone (temp, ggopts->delay_until, NULL);
	} else
		gopts->delay_until = 0;
}

static void
e_send_options_load_status_options (ESendOptionsStatusTracking *sopts,
                                    EGwSendOptionsStatusTracking *gsopts)
{
	sopts->tracking_enabled = gsopts->tracking_enabled;
	sopts->track_when = gsopts->track_when;

	sopts->autodelete = gsopts->autodelete;

	sopts->opened = gsopts->opened;
	sopts->accepted = gsopts->accepted;
	sopts->declined = gsopts->declined;
	sopts->completed = gsopts->completed;
}

static void
e_send_options_load_default_data (EGwSendOptions *opts, ESendOptionsDialog *sod)
{
	EGwSendOptionsGeneral *ggopts;
	EGwSendOptionsStatusTracking *gmopts;
	EGwSendOptionsStatusTracking *gcopts;
	EGwSendOptionsStatusTracking *gtopts;

	ggopts = e_gw_sendoptions_get_general_options (opts);
	gmopts = e_gw_sendoptions_get_status_tracking_options (opts, "mail");
	gcopts = e_gw_sendoptions_get_status_tracking_options (opts, "calendar");
	gtopts = e_gw_sendoptions_get_status_tracking_options (opts, "task");

	e_send_options_load_general_opts (sod->data->gopts, ggopts);
	e_send_options_load_status_options (sod->data->mopts, gmopts);
	e_send_options_load_status_options (sod->data->copts, gcopts);
	e_send_options_load_status_options (sod->data->topts, gtopts);
}

static void
e_send_options_clicked_cb (GtkWidget *button, gpointer data)
{
	EGwConnectionStatus status;
	account = (EAccount *) data;
	if (!sod) {
		sod = e_send_options_dialog_new ();
		e_send_options_set_global (sod, TRUE);
		if (!n_cnc)
			n_cnc = get_cnc (GTK_WINDOW (gtk_widget_get_toplevel (button)));

		if (!n_cnc) {
			g_warning ("Send Options: Could not get the connection to the server \n");
			return;
		}

		status = e_gw_connection_get_settings (n_cnc, &opts);
		if (status == E_GW_CONNECTION_STATUS_INVALID_CONNECTION)
			status = e_gw_connection_get_settings (n_cnc, &opts);
		if (status != E_GW_CONNECTION_STATUS_OK) {
			g_warning ("Send Options: Could not get the settings from the server");
			return;
		}
		e_send_options_load_default_data (opts, sod);
	}

	if (n_cnc)
		e_send_options_dialog_run (sod, parent ? parent : NULL, E_ITEM_NONE);
	else
		return;
}

GtkWidget *
org_gnome_send_options (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	EMConfigTargetAccount *target_account;
	GtkWidget *frame, *button, *label, *vbox;
	gchar *markup;

	target_account = (EMConfigTargetAccount *)data->config->target;
	account = target_account->account;

	if (!g_strrstr (account->source->url, "groupwise://"))
		return NULL;

	vbox = gtk_vbox_new (FALSE, 0);
	frame = gtk_frame_new ("");
	label = gtk_frame_get_label_widget (GTK_FRAME (frame));
	markup = g_strdup_printf("<b>%s</b>", _("Send Options"));
	gtk_label_set_markup (GTK_LABEL (label), markup);
	button = gtk_button_new_with_label (_("Advanced send options"));
	gtk_widget_show (button);
	g_free (markup);

	g_signal_connect(button, "clicked",
			    G_CALLBACK (e_send_options_clicked_cb), account);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (data->parent));
	if (!gtk_widget_is_toplevel (parent))
		parent = NULL;

	gtk_widget_set_size_request (button, 10, -1);
	gtk_box_pack_start (GTK_BOX (vbox), frame, 0, 0, 0);
	gtk_container_add (GTK_CONTAINER (frame), button);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
	gtk_widget_show (frame);
	gtk_box_set_spacing (GTK_BOX (data->parent), 12);
	gtk_box_pack_start (GTK_BOX (data->parent), vbox, FALSE, FALSE, 0);

	return vbox;
}

static void
send_options_finalize (void)
{
	if (n_cnc) {
		g_object_unref (n_cnc);
		n_cnc = NULL;
	}

	if (sod) {
		g_object_unref (sod);
		sod = NULL;
	}

	if (opts) {
		g_object_unref (opts);
		opts = NULL;
	}
}

static void
e_send_options_copy_general_opts (ESendOptionsGeneral *gopts,
                                  EGwSendOptionsGeneral *ggopts)
{
	ggopts->priority = gopts->priority;

	ggopts->reply_enabled = gopts->reply_enabled;
	ggopts->reply_convenient = gopts->reply_convenient;
	ggopts->reply_within = gopts->reply_within;

	ggopts->expire_after = gopts->expire_after;

	if (gopts->expire_after == 0) {
		ggopts->expiration_enabled = FALSE;
		gopts->expiration_enabled = FALSE;
	} else
		ggopts->expiration_enabled = gopts->expiration_enabled;

	ggopts->delay_enabled = gopts->delay_enabled;

	if (gopts->delay_until) {
		gint diff;
		icaltimetype temp, current;

		temp = icaltime_from_timet (gopts->delay_until, 0);
		current = icaltime_today ();
		diff = temp.day - current.day;
		ggopts->delay_until = diff;
	} else
		ggopts->delay_until = 0;
}

static void
e_send_options_copy_status_options (ESendOptionsStatusTracking *sopts,
                                    EGwSendOptionsStatusTracking *gsopts)
{
	gsopts->tracking_enabled = sopts->tracking_enabled;
	gsopts->track_when = sopts->track_when;

	gsopts->autodelete = sopts->autodelete;

	gsopts->opened = sopts->opened;
	gsopts->accepted = sopts->accepted;
	gsopts->declined = sopts->declined;
	gsopts->completed = sopts->completed;
}

static gboolean
check_status_options_changed (EGwSendOptionsStatusTracking *n_sopts,
                              EGwSendOptionsStatusTracking *o_sopts)
{
	return (!(n_sopts->tracking_enabled == o_sopts->tracking_enabled
		&& n_sopts->track_when == o_sopts->track_when
		&& n_sopts->autodelete == o_sopts->autodelete
		&& n_sopts->opened == o_sopts->opened
		&& n_sopts->declined == o_sopts->declined
		&& n_sopts->accepted == o_sopts->accepted
		&& n_sopts->completed == o_sopts->completed));

}

static gboolean
check_general_changed (EGwSendOptionsGeneral *n_gopts,
                       EGwSendOptionsGeneral *o_gopts)
{
	return (!(n_gopts->priority == o_gopts->priority
		&& n_gopts->delay_enabled == o_gopts->delay_enabled
		&& n_gopts->delay_until == o_gopts->delay_until
		&& n_gopts->reply_enabled == o_gopts->reply_enabled
		&& n_gopts->reply_convenient == o_gopts->reply_convenient
		&& n_gopts->reply_within == o_gopts->reply_within
		&& n_gopts->expiration_enabled == o_gopts->expiration_enabled
		&& n_gopts->expire_after == o_gopts->expire_after));
}

static void
send_options_copy_check_changed (EGwSendOptions *n_opts)
{
	EGwSendOptionsGeneral *ggopts, *o_gopts;
	EGwSendOptionsStatusTracking *gmopts, *o_gmopts;
	EGwSendOptionsStatusTracking *gcopts, *o_gcopts;
	EGwSendOptionsStatusTracking *gtopts, *o_gtopts;

	ggopts = e_gw_sendoptions_get_general_options (n_opts);
	gmopts = e_gw_sendoptions_get_status_tracking_options (n_opts, "mail");
	gcopts = e_gw_sendoptions_get_status_tracking_options (n_opts, "calendar");
	gtopts = e_gw_sendoptions_get_status_tracking_options (n_opts, "task");

	o_gopts = e_gw_sendoptions_get_general_options (opts);
	o_gmopts = e_gw_sendoptions_get_status_tracking_options (opts, "mail");
	o_gcopts = e_gw_sendoptions_get_status_tracking_options (opts, "calendar");
	o_gtopts = e_gw_sendoptions_get_status_tracking_options (opts, "task");

	e_send_options_copy_general_opts (sod->data->gopts, ggopts);
	e_send_options_copy_status_options (sod->data->mopts, gmopts);
	e_send_options_copy_status_options (sod->data->copts, gcopts);
	e_send_options_copy_status_options (sod->data->topts, gtopts);

        if (check_general_changed (ggopts, o_gopts))
		changed = TRUE;
	if (check_status_options_changed (gmopts, o_gmopts))
		changed = TRUE;
	if (check_status_options_changed (gcopts, o_gcopts))
		changed = TRUE;
	if (check_status_options_changed (gtopts, o_gtopts))
		changed = TRUE;
}

static ESource *
get_source (ESourceList *list)
{
	GSList *p, *l;
	gchar **temp = g_strsplit (account->source->url, ";", -1);
	gchar *uri = temp[0];

	l = e_source_list_peek_groups (list);

	for (p = l; p != NULL; p = p->next) {
		gchar *so_uri;
		GSList *r, *s;
		ESourceGroup *group = E_SOURCE_GROUP (p->data);

		s = e_source_group_peek_sources (group);
		for (r = s; r != NULL; r = r->next) {
			ESource *so = E_SOURCE (r->data);
			so_uri = e_source_get_uri (so);

			if (so_uri) {
				if (!strcmp (so_uri, uri)) {
					g_free (so_uri), so_uri = NULL;
					g_strfreev (temp);
					return E_SOURCE (r->data);
				}
				g_free (so_uri), so_uri = NULL;
			}
		}
	}

	g_strfreev (temp);

	return NULL;
}

static void
add_return_value (EGwSendOptionsReturnNotify track,
                  ESource *source,
                  const gchar *notify)
{
	gchar *value;

	switch (track) {
		case E_GW_RETURN_NOTIFY_MAIL:
			value =  g_strdup ("mail");
			break;
		default:
			value = g_strdup ("none");
	}

	e_source_set_property (source, notify, value);
	g_free (value), value = NULL;
}

static void
put_options_in_source (ESource *source,
                       EGwSendOptionsGeneral *gopts,
                       EGwSendOptionsStatusTracking *sopts)
{
	gchar *value;
	const gchar *val;
	icaltimetype tt;
	CamelURL  *url;

       url = camel_url_new (account->source->url, NULL);

	if (gopts) {
			/* priority */
		switch (gopts->priority) {
			case E_GW_PRIORITY_HIGH:
				value = g_strdup ("high");
				break;
			case E_GW_PRIORITY_STANDARD:
				value = g_strdup ("standard");
				break;
			case E_GW_PRIORITY_LOW:
				value =  g_strdup ("low");
				break;
			default:
				value = g_strdup ("undefined");
		}
		e_source_set_property (source, "priority", value);
		camel_url_set_param (url, "priority", value);
		g_free (value), value = NULL;

			/* Reply Requested */
		/*TODO Fill the value if it is not "convinient" */
		if (gopts->reply_enabled) {
			if (gopts->reply_convenient)
				value = g_strdup ("convinient");
			else
				value = g_strdup_printf ("%d",gopts->reply_within);
		 } else
			value = g_strdup ("none");
		e_source_set_property (source, "reply-requested", value);
		g_free (value), value = NULL;

			/* Delay delivery */
		if (gopts->delay_enabled) {
				tt = icaltime_today ();
				icaltime_adjust (&tt, gopts->delay_until, 0, 0, 0);
				val = icaltime_as_ical_string_r (tt);
		} else
			val = "none";
		e_source_set_property (source, "delay-delivery", val);

			/* Expiration date */
		if (gopts->expiration_enabled)
			value =  g_strdup_printf ("%d", gopts->expire_after);
		else
			value = g_strdup ("none");
		e_source_set_property (source, "expiration", value);
		g_free (value), value = NULL;
	}

	if (sopts) {
			/* status tracking */
		if (sopts->tracking_enabled) {
			switch (sopts->track_when) {
				case E_GW_DELIVERED :
					value = g_strdup ("delivered");
					break;
				case E_GW_DELIVERED_OPENED:
					value = g_strdup ("delivered-opened");
					break;
				default:
					value = g_strdup ("all");
			}
		} else
			value = g_strdup ("none");
		e_source_set_property (source, "status-tracking", value);
		g_free (value), value = NULL;

		add_return_value (sopts->opened, source, "return-open");
		add_return_value (sopts->accepted, source, "return-accept");
		add_return_value (sopts->declined, source, "return-decline");
		add_return_value (sopts->completed, source, "return-complete");
	}
}

static void
add_send_options_to_source (EGwSendOptions *n_opts)
{
	GConfClient *gconf = gconf_client_get_default ();
	ESource *csource, *tsource;
	ESourceList *list;
	EGwSendOptionsGeneral *gopts;
	EGwSendOptionsStatusTracking *topts, *copts;

	list = e_source_list_new_for_gconf (gconf, "/apps/evolution/calendar/sources");
	csource = get_source (list);

	list = e_source_list_new_for_gconf (gconf, "/apps/evolution/tasks/sources");
	tsource = get_source (list);

	gopts = e_gw_sendoptions_get_general_options (n_opts);
	copts = e_gw_sendoptions_get_status_tracking_options (n_opts, "calendar");
	topts = e_gw_sendoptions_get_status_tracking_options (n_opts, "task");

	if (csource)
		put_options_in_source (csource, gopts, copts);

	if (tsource)
		put_options_in_source (tsource, gopts, topts);

	g_object_unref (gconf);
}

void
send_options_commit (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	EGwSendOptions *n_opts;
	EGwConnectionStatus status = E_GW_CONNECTION_STATUS_OK;

	if (sod) {
		n_opts = e_gw_sendoptions_new ();
		send_options_copy_check_changed (n_opts);

		if (changed)
			status = e_gw_connection_modify_settings (n_cnc, n_opts);

		if (!changed || status != E_GW_CONNECTION_STATUS_OK) {
			g_warning (
				G_STRLOC "Cannot modify Send Options:  %s",
				e_gw_connection_get_error_message (status));
			g_object_unref (n_opts);
			n_opts = NULL;
		} else
			add_send_options_to_source (n_opts);
	}

	send_options_finalize ();
}

void
send_options_changed (EPlugin *epl, EConfigHookItemFactoryData *data)
{
}

void
send_options_abort (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	send_options_finalize ();
}

