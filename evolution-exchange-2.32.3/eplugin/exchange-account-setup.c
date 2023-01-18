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
 *		Sushma Rai <rsushma@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "libedataserver/e-account.h"
#include "e-util/e-alert-dialog.h"

#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gconf/gconf-client.h>
#include <e-util/e-dialog-utils.h>
#include <e2k-validate.h>
#include <exchange-oof.h>
#include <exchange-account.h>
#include "exchange-config-listener.h"
#include "exchange-operations.h"
#include "exchange-folder-size-display.h"
#include "mail/em-account-editor.h"
#include "mail/em-config.h"
#include "exchange-delegates.h"
#include "exchange-change-password.h"
#include "exchange-folder-size-display.h"

GtkWidget* org_gnome_exchange_settings(EPlugin *epl, EConfigHookItemFactoryData *data);
GtkWidget *org_gnome_exchange_owa_url(EPlugin *epl, EConfigHookItemFactoryData *data);
gboolean org_gnome_exchange_check_options(EPlugin *epl, EConfigHookPageCheckData *data);
GtkWidget *org_gnome_exchange_auth_section (EPlugin *epl, EConfigHookItemFactoryData *data);
void org_gnome_exchange_commit (EPlugin *epl, EMConfigTargetAccount *target_account);
GtkWidget* org_gnome_exchange_show_folder_size_factory (EPlugin *epl, EConfigHookItemFactoryData *data);

CamelServiceAuthType camel_exchange_ntlm_authtype = {
	/* i18n: "Secure Password Authentication" is an Outlookism */
	(gchar *) N_("Secure Password"),

	/* i18n: "NTLM" probably doesn't translate */
	(gchar *) N_("This option will connect to the Exchange server "
	"using secure password (NTLM) authentication."),

	(gchar *) "NTLM",
	TRUE
};

CamelServiceAuthType camel_exchange_password_authtype = {
	(gchar *) N_("Plaintext Password"),

	(gchar *) N_("This option will connect to the Exchange server "
	"using standard plaintext password authentication."),

	(gchar *) "Basic",
	TRUE
};

typedef struct {
	gboolean state;
	gchar *message;
	GtkWidget *text_view;
}OOFData;

static OOFData *oof_data = NULL;

static void
update_state (GtkTextBuffer *buffer, gpointer data)
{
	if (gtk_text_buffer_get_modified (buffer)) {
		GtkTextIter start, end;
		if (oof_data->message)
			g_free (oof_data->message);
		gtk_text_buffer_get_bounds (buffer, &start, &end);
		oof_data->message =  gtk_text_buffer_get_text (buffer, &start,
							       &end, FALSE);
		gtk_text_buffer_set_modified (buffer, FALSE);
	}
}

static void
toggled_state (GtkToggleButton *button, gpointer data)
{
	gboolean current_oof_state = gtk_toggle_button_get_active (button);

	if (current_oof_state == oof_data->state)
		return;
	oof_data->state = current_oof_state;
	gtk_widget_set_sensitive (oof_data->text_view, current_oof_state);
}

#ifdef HAVE_KRB5
static void
btn_chpass_clicked (GtkButton *button, gpointer data)
{
	ExchangeAccount *account;
	gchar *old_password, *new_password;
	ExchangeAccountResult result;

	account = exchange_operations_get_exchange_account ();
	if (!account)
		return;

	old_password = exchange_account_get_password (account);
	if (!old_password) {
		g_print ("Could not fetch old password\n");
		return;
	}
	new_password = exchange_get_new_password (old_password, TRUE);
	if (!new_password) {
		/* "Cacel" button was hit */
		return;
	}
	/* g_print ("Current password is \"%s\"\n", old_password); */
	result = exchange_account_set_password (account, old_password, new_password);
	if (result != EXCHANGE_ACCOUNT_CONNECT_SUCCESS)
		exchange_operations_report_error (account, result);

	g_free (old_password);
	g_free (new_password);
}
#endif

static void
btn_dass_clicked (GtkButton *button, gpointer data)
{
	ExchangeAccount *account;
	account = exchange_operations_get_exchange_account ();
	if (!account)
		return;
	exchange_delegates (account, gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_WINDOW));
}

static void
btn_fsize_clicked (GtkButton *button, gpointer data)
{
	ExchangeAccount *account;
	GtkListStore *model;

	account = exchange_operations_get_exchange_account ();
	if (!account)
		return;

	model = exchange_account_folder_size_get_model (account);
	if (model)
		exchange_folder_size_display (model, gtk_widget_get_toplevel (GTK_WIDGET (button)));
}

/* only used in editor */
GtkWidget *
org_gnome_exchange_settings(EPlugin *epl, EConfigHookItemFactoryData *data)
{
	EMConfigTargetAccount *target_account;
	ExchangeAccount *account = NULL;
	CamelURL *url;
	const gchar *source_url;
	gchar *message = NULL, *txt = NULL, *oof_message;
	gboolean oof_state = FALSE;
	gint offline_status;

	GtkVBox *vbox_settings;

	/* OOF fields */
	GtkFrame *frm_oof;
	GtkVBox *vbox_oof;
	GtkLabel *lbl_oof_desc;
	GtkTable *tbl_oof_status;
	GtkLabel *lbl_status;
	GtkRadioButton *radio_iof, *radio_oof;
	GtkScrolledWindow *scrwnd_oof;
	GtkTextView *txtview_oof;

	/* Authentication settings */
	GtkFrame *frm_auth;
	GtkVBox *vbox_auth;
	GtkTable *tbl_auth;
#ifdef HAVE_KRB5
	GtkLabel *lbl_chpass;
	GtkButton *btn_chpass;
#endif
	GtkLabel *lbl_dass;
	GtkButton *btn_dass;

	/* Miscelleneous setting */
	GtkFrame *frm_misc;
	GtkVBox *vbox_misc;
	GtkTable *tbl_misc;
	GtkLabel *lbl_fsize;
	GtkButton *btn_fsize;

	GtkTextBuffer *buffer;
	GtkTextIter start, end;

	target_account = (EMConfigTargetAccount *)data->config->target;
	source_url = e_account_get_string (target_account->account,  E_ACCOUNT_SOURCE_URL);
	url = camel_url_new(source_url, NULL);
	if (url == NULL
	    || strcmp(url->protocol, "exchange") != 0) {
		if (url)
			camel_url_free(url);
		return NULL;
	}

	if (data->old) {
		camel_url_free(url);
		return data->old;
	}
	if (url)
		camel_url_free (url);

	account = exchange_operations_get_exchange_account ();

	exchange_config_listener_get_offline_status (exchange_global_config_listener,
								    &offline_status);
	if (offline_status == OFFLINE_MODE) {
		e_alert_run_dialog_for_args (GTK_WINDOW (data->config->target->widget), ERROR_DOMAIN ":exchange-settings-offline", NULL);

		return NULL;
	}

	oof_data = g_new0 (OOFData, 1);

	oof_data->state = FALSE;
	oof_data->message = NULL;
	oof_data->text_view = NULL;

	/* See if oof info found already */

	if (account && !exchange_oof_get (account, &oof_state, &message)) {

		e_alert_run_dialog_for_args (GTK_WINDOW (data->config->target->widget), ERROR_DOMAIN ":state-read-error", NULL);

                return NULL;
        }

	if (message && *message)
		oof_data->message = g_strdup (message);
	else
		oof_data->message = NULL;
	oof_data->state = oof_state;

	/* construct page */

	vbox_settings = (GtkVBox*) g_object_new (GTK_TYPE_VBOX, "homogeneous", FALSE, "spacing", 6, NULL);
	gtk_container_set_border_width (GTK_CONTAINER (vbox_settings), 12);

	frm_oof = (GtkFrame*) g_object_new (GTK_TYPE_FRAME, "label", _("Out of Office"), NULL);
	gtk_box_pack_start (GTK_BOX (vbox_settings), GTK_WIDGET (frm_oof), FALSE, FALSE, 0);

	vbox_oof = (GtkVBox*) g_object_new (GTK_TYPE_VBOX, NULL, "homogeneous", FALSE, "spacing", 12, NULL);
	gtk_container_set_border_width (GTK_CONTAINER (vbox_oof), 6);
	gtk_container_add (GTK_CONTAINER (frm_oof), GTK_WIDGET (vbox_oof));

	lbl_oof_desc = (GtkLabel*) g_object_new (GTK_TYPE_LABEL, "label", _("The message specified below will be automatically sent to \neach person who sends mail to you while you are out of the office."), "justify", GTK_JUSTIFY_LEFT, NULL);
	gtk_misc_set_alignment (GTK_MISC (lbl_oof_desc), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (vbox_oof), GTK_WIDGET (lbl_oof_desc), FALSE, FALSE, 0);

	tbl_oof_status = (GtkTable*) g_object_new (GTK_TYPE_TABLE, "n-rows", 2, "n-columns", 2, "homogeneous", FALSE, "row-spacing", 6, "column-spacing", 6, NULL);
	txt = g_strdup_printf ("<b>%s</b>", _("Status:"));
	lbl_status = (GtkLabel*) g_object_new (GTK_TYPE_LABEL, "label", txt, "use-markup", TRUE, NULL);
	g_free (txt);
	gtk_misc_set_alignment (GTK_MISC (lbl_status), 0, 0.5);
	gtk_misc_set_padding (GTK_MISC (lbl_status), 0, 0);

	if (oof_data->state) {
		radio_oof = (GtkRadioButton*) g_object_new (GTK_TYPE_RADIO_BUTTON, "label", _("I am out of the office"), NULL);
		radio_iof = (GtkRadioButton*) g_object_new (GTK_TYPE_RADIO_BUTTON, "label", _("I am in the office"), "group", radio_oof, NULL);
	}
	else {
		radio_iof = (GtkRadioButton*) g_object_new (GTK_TYPE_RADIO_BUTTON, "label", _("I am in the office"), NULL);
		radio_oof = (GtkRadioButton*) g_object_new (GTK_TYPE_RADIO_BUTTON, "label", _("I am out of the office"), "group", radio_iof, NULL);
	}
	g_signal_connect (radio_oof, "toggled", G_CALLBACK (toggled_state), NULL);

	gtk_table_attach (tbl_oof_status, GTK_WIDGET (lbl_status), 0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
	gtk_table_attach (tbl_oof_status, GTK_WIDGET (radio_iof), 1, 2, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
	gtk_table_attach (tbl_oof_status, GTK_WIDGET (radio_oof), 1, 2, 1, 2, GTK_FILL, GTK_FILL, 0, 0);

	gtk_box_pack_start (GTK_BOX (vbox_oof), GTK_WIDGET (tbl_oof_status), FALSE, FALSE, 0);

	scrwnd_oof = (GtkScrolledWindow*) g_object_new (GTK_TYPE_SCROLLED_WINDOW, "hscrollbar-policy", GTK_POLICY_AUTOMATIC, "vscrollbar-policy", GTK_POLICY_AUTOMATIC, "shadow-type", GTK_SHADOW_IN, NULL);
	gtk_box_pack_start (GTK_BOX (vbox_oof), GTK_WIDGET (scrwnd_oof), FALSE, FALSE, 0);
	txtview_oof = (GtkTextView*) g_object_new (GTK_TYPE_TEXT_VIEW, "justification", GTK_JUSTIFY_LEFT, "wrap-mode", GTK_WRAP_WORD, "editable", TRUE, NULL);
	buffer = gtk_text_view_get_buffer (txtview_oof);
	gtk_text_buffer_get_bounds (buffer, &start, &end);
	oof_message = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
	if (oof_message && *oof_message) {
		g_free (oof_data->message);
		/* Will this ever happen? */
		oof_data->message = oof_message;
	}
	if (oof_data->message) {
		/* previuosly set message */
		gtk_text_buffer_set_text (buffer, oof_data->message, -1);
		gtk_text_view_set_buffer (txtview_oof, buffer);

	}
	gtk_text_buffer_set_modified (buffer, FALSE);
	if (!oof_data->state)
		gtk_widget_set_sensitive (GTK_WIDGET (txtview_oof), FALSE);
	oof_data->text_view = GTK_WIDGET (txtview_oof);
	g_signal_connect (buffer, "changed", G_CALLBACK (update_state), NULL);
	gtk_container_add (GTK_CONTAINER (scrwnd_oof), GTK_WIDGET (txtview_oof));

	/* Security settings */
	frm_auth = (GtkFrame*) g_object_new (GTK_TYPE_FRAME, "label", _("Security"), NULL);
	gtk_box_pack_start (GTK_BOX (vbox_settings), GTK_WIDGET (frm_auth), FALSE, FALSE, 0);

	vbox_auth = (GtkVBox*) g_object_new (GTK_TYPE_VBOX, "homogeneous", FALSE, "spacing", 6, NULL);
	gtk_container_set_border_width (GTK_CONTAINER (vbox_auth), 6);
	gtk_container_add (GTK_CONTAINER (frm_auth), GTK_WIDGET (vbox_auth));

	tbl_auth = (GtkTable*) g_object_new (GTK_TYPE_TABLE, "n-rows", 2, "n-columns", 2, "homogeneous", FALSE, "row-spacing", 6, "column-spacing", 6, NULL);

#ifdef HAVE_KRB5
	/* Change Password */
	lbl_chpass = (GtkLabel*) g_object_new (GTK_TYPE_LABEL, "label", _("Change the password for Exchange account"), NULL);
	gtk_misc_set_alignment (GTK_MISC (lbl_chpass), 0, 0.5);
	btn_chpass = (GtkButton*) g_object_new (GTK_TYPE_BUTTON, "label", _("Change Password"), NULL);
	g_signal_connect (GTK_OBJECT (btn_chpass), "clicked", G_CALLBACK (btn_chpass_clicked), NULL);
#endif

	/* Delegation Assistant */
	lbl_dass = (GtkLabel*) g_object_new (GTK_TYPE_LABEL, "label", _("Manage the delegate settings for Exchange account"), NULL);
	gtk_misc_set_alignment (GTK_MISC (lbl_dass), 0, 0.5);
	btn_dass = (GtkButton*) g_object_new (GTK_TYPE_BUTTON, "label", _("Delegation Assistant"), NULL);
	g_signal_connect (btn_dass, "clicked", G_CALLBACK (btn_dass_clicked), NULL);
	/* Add items to the table */
#ifdef HAVE_KRB5
	gtk_table_attach_defaults (tbl_auth, GTK_WIDGET (lbl_chpass), 0, 1, 0, 1);
	gtk_table_attach (tbl_auth, GTK_WIDGET (btn_chpass), 1, 2, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
#endif
	gtk_table_attach_defaults (tbl_auth, GTK_WIDGET (lbl_dass), 0, 1, 1, 2);
	gtk_table_attach (tbl_auth, GTK_WIDGET (btn_dass), 1, 2, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
	gtk_box_pack_start (GTK_BOX (vbox_auth), GTK_WIDGET (tbl_auth), FALSE, FALSE, 0);

	/* Miscelleneous settings */
	frm_misc = (GtkFrame*) g_object_new (GTK_TYPE_FRAME, "label", _("Miscellaneous"), NULL);
	gtk_box_pack_start (GTK_BOX (vbox_settings), GTK_WIDGET (frm_misc), FALSE, FALSE, 0);

	vbox_misc = (GtkVBox*) g_object_new (GTK_TYPE_VBOX, "homogeneous", FALSE, "spacing", 6, NULL);
	gtk_container_set_border_width (GTK_CONTAINER (vbox_misc), 6);
	gtk_container_add (GTK_CONTAINER (frm_misc), GTK_WIDGET (vbox_misc));

	tbl_misc = (GtkTable*) g_object_new (GTK_TYPE_TABLE, "n-rows", 1, "n-columns", 1, "homogeneous", FALSE, "row-spacing", 6, "column-spacing", 6, NULL);

	/* Folder Size */
	lbl_fsize = (GtkLabel*) g_object_new (GTK_TYPE_LABEL, "label", _("View the size of all Exchange folders"), NULL);
	gtk_misc_set_alignment (GTK_MISC (lbl_fsize), 0, 0.5);
	btn_fsize = (GtkButton*) g_object_new (GTK_TYPE_BUTTON, "label", _("Folder Size"), NULL);
	g_signal_connect (btn_fsize, "clicked", G_CALLBACK (btn_fsize_clicked), NULL);
	gtk_table_attach_defaults (tbl_misc, GTK_WIDGET (lbl_fsize), 0, 1, 0, 1);
	gtk_table_attach (tbl_misc, GTK_WIDGET (btn_fsize), 1, 2, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
	gtk_box_pack_start (GTK_BOX (vbox_misc), GTK_WIDGET (tbl_misc), FALSE, FALSE, 0);

	gtk_widget_show_all (GTK_WIDGET (vbox_settings));
	gtk_notebook_insert_page (GTK_NOTEBOOK (data->parent), GTK_WIDGET (vbox_settings), gtk_label_new(_("Exchange Settings")), 4);
	return GTK_WIDGET (vbox_settings);

}

static void
print_error (GtkWidget *parent, const gchar *owa_url, E2kAutoconfigResult result)
{
	const gchar *err_msg = NULL;

	switch (result) {

		case E2K_AUTOCONFIG_CANT_CONNECT:
			e_alert_run_dialog_for_args (GTK_WINDOW (parent), ERROR_DOMAIN ":account-connect-error", "", NULL);
			break;

		case E2K_AUTOCONFIG_CANT_RESOLVE:
			e_alert_run_dialog_for_args (GTK_WINDOW (parent), ERROR_DOMAIN ":account-resolve-error", "", NULL);
			break;

		case E2K_AUTOCONFIG_AUTH_ERROR:
		case E2K_AUTOCONFIG_AUTH_ERROR_TRY_NTLM:
		case E2K_AUTOCONFIG_AUTH_ERROR_TRY_BASIC:
			err_msg = ERROR_DOMAIN ":password-incorrect";
			break;

		case E2K_AUTOCONFIG_AUTH_ERROR_TRY_DOMAIN:
			err_msg = ERROR_DOMAIN ":account-domain-error";
			break;

		case E2K_AUTOCONFIG_NO_OWA:
		case E2K_AUTOCONFIG_NOT_EXCHANGE:
			err_msg = ERROR_DOMAIN ":account-wss-error";
			break;

		case E2K_AUTOCONFIG_CANT_BPROPFIND:
			e_alert_run_dialog_for_args (GTK_WINDOW (parent), ERROR_DOMAIN ":connect-exchange-error",
				     "http://support.novell.com/cgi-bin/search/searchtid.cgi?/ximian/ximian328.html",
				     NULL);
			break;

		case E2K_AUTOCONFIG_EXCHANGE_5_5:
			err_msg = ERROR_DOMAIN ":account-version-error";
			break;

		default:
			err_msg = ERROR_DOMAIN ":configure-error";
			break;

	}

	if (err_msg)
		e_alert_run_dialog_for_args (GTK_WINDOW (parent), err_msg, NULL);
}

static const gchar *
gal_auth_to_string (E2kAutoconfigGalAuthPref ad_auth)
{
	switch (ad_auth) {
	case E2K_AUTOCONFIG_USE_GAL_DEFAULT: return "default";
	case E2K_AUTOCONFIG_USE_GAL_BASIC:   return "basic";
	case E2K_AUTOCONFIG_USE_GAL_NTLM:    return "ntlm";
	}

	return "default";
}

static void
owa_authenticate_user(GtkWidget *button, EConfig *config)
{
	EMConfigTargetAccount *target_account = (EMConfigTargetAccount *)config->target;
	E2kAutoconfigResult result;
	CamelURL *url=NULL;
	gboolean remember_password;
	gchar *url_string, *key;
	const gchar *source_url, *id_name, *owa_url;
	gchar *at, *user;
	gboolean valid = FALSE;
	ExchangeParams *exchange_params;
	GtkWidget *mailbox_entry = g_object_get_data (G_OBJECT (button), "mailbox-entry");

	exchange_params = g_new0 (ExchangeParams, 1);
	exchange_params->host = NULL;
	exchange_params->ad_server = NULL;
	exchange_params->ad_auth = E2K_AUTOCONFIG_USE_GAL_DEFAULT;
	exchange_params->mailbox = NULL;
	exchange_params->owa_path = NULL;
	exchange_params->is_ntlm = TRUE;

	source_url = e_account_get_string (target_account->account, E_ACCOUNT_SOURCE_URL);

	if (source_url && source_url[0] != '\0')
		url = camel_url_new(source_url, NULL);
	if (url && url->user == NULL) {
		id_name = e_account_get_string (target_account->account, E_ACCOUNT_ID_ADDRESS);
		if (id_name) {
			at = strchr(id_name, '@');
			user = g_alloca(at-id_name+1);
			memcpy(user, id_name, at-id_name);
			user[at-id_name] = 0;
			camel_url_set_user (url, user);
		}
	}

	/* validate_user() CALLS GTK!!!

	   THIS IS TOTALLY UNNACCEPTABLE!!!!!!!!

	   It must use camel_session_ask_password, and it should return an exception for any problem,
	   which should then be shown using e-alert */

	owa_url = camel_url_get_param (url, "owa_url");
	if (camel_url_get_param (url, "authmech"))
		exchange_params->is_ntlm = TRUE;
	else
		exchange_params->is_ntlm = FALSE;
	camel_url_set_authmech (url, exchange_params->is_ntlm ? "NTLM" : "Basic");

	key = camel_url_to_string (url, CAMEL_URL_HIDE_PASSWORD | CAMEL_URL_HIDE_PARAMS);
	/* Supress the trailing slash */
	key[strlen(key) -1] = 0;

	/* set the mailbox before function call to let it use our, not create one */
	exchange_params->mailbox = g_strdup (camel_url_get_param (url, "mailbox"));

	valid =  e2k_validate_user (owa_url, key, &url->user, exchange_params,
						&remember_password, &result,
						GTK_WINDOW (gtk_widget_get_toplevel (button)));
	g_free (key);

	if (!valid && result != E2K_AUTOCONFIG_CANCELLED)
		print_error (config->target->widget, owa_url, result);

	camel_url_set_host (url, valid ? exchange_params->host : "");

	if (valid)
		camel_url_set_param (url, "save-passwd", remember_password? "true" : "false");

	camel_url_set_param (url, "ad_server", valid ? exchange_params->ad_server: NULL);
	camel_url_set_param (url, "mailbox", valid ? exchange_params->mailbox : NULL);
	camel_url_set_param (url, "owa_path", valid ? exchange_params->owa_path : NULL);
	camel_url_set_param (url, "ad_auth", valid ? gal_auth_to_string (exchange_params->ad_auth) : NULL);

	if (mailbox_entry) {
		const gchar *par = camel_url_get_param (url, "mailbox");

		gtk_entry_set_text (GTK_ENTRY (mailbox_entry), par ? par : "");
	}

	g_free (exchange_params->owa_path);
	g_free (exchange_params->mailbox);
	g_free (exchange_params->host);
	g_free (exchange_params->ad_server);
	g_free (exchange_params);

	if (valid) {
		url_string = camel_url_to_string (url, 0);
		e_account_set_string (target_account->account, E_ACCOUNT_SOURCE_URL, url_string);
		e_account_set_string (target_account->account, E_ACCOUNT_TRANSPORT_URL, url_string);
		e_account_set_bool (target_account->account, E_ACCOUNT_SOURCE_SAVE_PASSWD, remember_password);
		g_free (url_string);
	}
	camel_url_free (url);
}

static void
owa_editor_entry_changed(GtkWidget *entry, EConfig *config)
{
	const gchar *ssl = NULL;
	CamelURL *url, *owaurl = NULL;
	gchar *url_string, *uri;
	EMConfigTargetAccount *target = (EMConfigTargetAccount *)config->target;
	GtkWidget *button = g_object_get_data((GObject *)entry, "authenticate-button");
	gint active = FALSE;

	/* NB: we set the button active only if we have a parsable uri entered */

	const gchar * target_url = e_account_get_string(target->account, E_ACCOUNT_SOURCE_URL);
	if (target_url && target_url[0] != '\0')
		url = camel_url_new(target_url, NULL);
	else url = NULL;
	uri = g_strdup (gtk_entry_get_text((GtkEntry *)entry));

	g_strstrip (uri);

	if (uri && uri[0]) {
		camel_url_set_param(url, "owa_url", uri);
		owaurl = camel_url_new(uri, NULL);
		if (owaurl) {
			active = TRUE;

			/* Reading the owa url and setting use_ssl paramemter */
			if (!strcmp(owaurl->protocol, "https"))
				ssl = "always";
			camel_url_free(owaurl);
		}
	} else {
		camel_url_set_param(url, "owa_url", NULL);
	}

	camel_url_set_param(url, "use_ssl", ssl);
	gtk_widget_set_sensitive(button, active);

	url_string = camel_url_to_string(url, 0);
	e_account_set_string(target->account, E_ACCOUNT_SOURCE_URL, url_string);
	g_free(url_string);
	camel_url_free (url);
	g_free (uri);
}

static void
update_mailbox_param_in_url (EAccount *account, e_account_item_t item, const gchar *mailbox)
{
	CamelURL *url;
	gchar *url_string;
	const gchar *target_url;

	if (!account)
		return;

	target_url = e_account_get_string (account, item);
	if (target_url && target_url[0] != '\0')
		url = camel_url_new (target_url, NULL);
	else
		return;

	if (mailbox && *mailbox)
		camel_url_set_param (url, "mailbox", mailbox);
	else
		camel_url_set_param (url, "mailbox", NULL);

	url_string = camel_url_to_string (url, 0);
	e_account_set_string (account, item, url_string);
	g_free (url_string);
	camel_url_free (url);
}

static void
mailbox_editor_entry_changed (GtkWidget *entry, EConfig *config)
{
	EMConfigTargetAccount *target;
	gchar *mailbox;

	target = (EMConfigTargetAccount *)config->target;
	mailbox = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));

	g_strstrip (mailbox);

	update_mailbox_param_in_url (target->account, E_ACCOUNT_SOURCE_URL, mailbox);
	update_mailbox_param_in_url (target->account, E_ACCOUNT_TRANSPORT_URL, mailbox);

	g_free (mailbox);
}

static void
want_mailbox_toggled (GtkWidget *toggle, EConfig *config)
{
	GtkWidget *entry;

	g_return_if_fail (toggle != NULL);
	g_return_if_fail (config != NULL);

	entry = g_object_get_data (G_OBJECT (toggle), "mailbox-entry");
	if (entry) {
		gboolean is_active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle));
		EMConfigTargetAccount *target;
		const gchar *mailbox;

		gtk_widget_set_sensitive (entry, is_active);

		target = (EMConfigTargetAccount *)config->target;
		mailbox = gtk_entry_get_text (GTK_ENTRY (entry));

		update_mailbox_param_in_url (target->account, E_ACCOUNT_SOURCE_URL, is_active ? mailbox : NULL);
		update_mailbox_param_in_url (target->account, E_ACCOUNT_TRANSPORT_URL, is_active ? mailbox : NULL);
	}
}

static gchar *
construct_owa_url (CamelURL *url)
{
	const gchar *owa_path, *use_ssl = NULL;
	const gchar *protocol = "http", *mailbox_name;
	gchar *owa_url;

	use_ssl = camel_url_get_param (url, "use_ssl");
	if (use_ssl) {
		if (!strcmp (use_ssl, "always"))
			protocol = "https";
	}

	owa_path = camel_url_get_param (url, "owa_path");
	if (!owa_path)
		owa_path = "/exchange";
	mailbox_name = camel_url_get_param (url, "mailbox");

	if (mailbox_name)
		owa_url = g_strdup_printf("%s://%s%s/%s", protocol, url->host, owa_path, mailbox_name);
	else
		owa_url = g_strdup_printf("%s://%s%s", protocol, url->host, owa_path );

	return owa_url;
}

/* used by editor and assistant - same code */
GtkWidget *
org_gnome_exchange_owa_url(EPlugin *epl, EConfigHookItemFactoryData *data)
{
	EMConfigTargetAccount *target_account;
	const gchar *source_url;
	gchar *owa_url = NULL, *mailbox_name, *username;
	GtkWidget *owa_entry, *mailbox_entry, *want_mailbox_check;
	CamelURL *url;
	gint row;
	GtkWidget *hbox, *label, *button;

	target_account = (EMConfigTargetAccount *)data->config->target;
	source_url = e_account_get_string (target_account->account,  E_ACCOUNT_SOURCE_URL);
	if (source_url && source_url[0] != '\0')
		url = camel_url_new(source_url, NULL);
	else
		url = NULL;
	if (url == NULL
	    || strcmp(url->protocol, "exchange") != 0) {
		if (url)
			camel_url_free(url);

		if (data->old
		    && (label = g_object_get_data((GObject *)data->old, "authenticate-label")))
			gtk_widget_destroy(label);

		/* TODO: we could remove 'owa-url' from the url,
		   but that will lose it if we come back.  Maybe a commit callback could do it */

		return NULL;
	}

	if (data->old) {
		camel_url_free(url);
		return data->old;
	}

	owa_url = g_strdup (camel_url_get_param(url, "owa_url"));
	mailbox_name = g_strdup (camel_url_get_param (url, "mailbox"));
	username = g_strdup (url->user);

	/* if the host is null, then user+other info is dropped silently, force it to be kept */
	if (url->host == NULL) {
		gchar *uri;

		camel_url_set_host(url, "");
		uri = camel_url_to_string(url, 0);
		e_account_set_string(target_account->account,  E_ACCOUNT_SOURCE_URL, uri);
		g_free(uri);
	}

	g_object_get (data->parent, "n-rows", &row, NULL);

	hbox = gtk_hbox_new (FALSE, 6);
	label = gtk_label_new_with_mnemonic(_("_OWA URL:"));
	gtk_widget_show(label);

	owa_entry = gtk_entry_new();

	if (!owa_url) {
		if (url->host[0] != 0) {
			gchar *uri;

			/* url has hostname but not owa_url.
			 * Account has been created using x-c-s or evo is upgraded to 2.2
			 * When invoked from assistant, hostname will get set after validation,
			 * so this condition will never be true during account creation.
			 */
			owa_url = construct_owa_url (url);
			camel_url_set_param (url, "owa_url", owa_url);
			uri = camel_url_to_string(url, 0);
			e_account_set_string(target_account->account,  E_ACCOUNT_SOURCE_URL, uri);
			g_free(uri);
		}
	}
	camel_url_free (url);
	if (owa_url)
		gtk_entry_set_text(GTK_ENTRY (owa_entry), owa_url);
	gtk_label_set_mnemonic_widget((GtkLabel *)label, owa_entry);

	button = gtk_button_new_with_mnemonic (_("A_uthenticate"));
	gtk_widget_set_sensitive (button, owa_url && owa_url[0]);

	gtk_box_pack_start (GTK_BOX (hbox), owa_entry, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	gtk_widget_show_all(hbox);

	gtk_table_attach (GTK_TABLE (data->parent), label, 0, 1, row, row+1, 0, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (data->parent), hbox, 1, 2, row, row+1, GTK_FILL|GTK_EXPAND, GTK_FILL, 0, 0);

	g_signal_connect (owa_entry, "changed", G_CALLBACK(owa_editor_entry_changed), data->config);
	g_object_set_data((GObject *)owa_entry, "authenticate-button", button);
	g_signal_connect (button, "clicked", G_CALLBACK(owa_authenticate_user), data->config);

	/* Track the authenticate label, so we can destroy it if e-config is to destroy the hbox */
	g_object_set_data((GObject *)hbox, "authenticate-label", label);

	/* check for correctness of the input in the owa_entry */
	owa_editor_entry_changed (owa_entry, data->config);

	row++;
	want_mailbox_check = gtk_check_button_new_with_mnemonic (
		_("Mailbox name is _different from username"));
	gtk_widget_show (want_mailbox_check);
	gtk_table_attach (GTK_TABLE (data->parent), want_mailbox_check, 1, 2, row, row+1, GTK_FILL, GTK_FILL, 0, 0);
	if (!username || !*username || !mailbox_name || !*mailbox_name ||
	    g_ascii_strcasecmp (username, mailbox_name) == 0 ||
	    (strchr (username, '/') && g_ascii_strcasecmp (strchr (username, '/') + 1, mailbox_name) == 0)) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (want_mailbox_check), FALSE);
	} else {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (want_mailbox_check), TRUE);
	}
	g_signal_connect (want_mailbox_check, "toggled", G_CALLBACK (want_mailbox_toggled), data->config);

	row++;
	label = gtk_label_new_with_mnemonic (_("_Mailbox:"));
	gtk_widget_show (label);

	mailbox_entry = gtk_entry_new ();
	gtk_widget_show (mailbox_entry);
	if (mailbox_name)
		gtk_entry_set_text (GTK_ENTRY (mailbox_entry), mailbox_name);

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), mailbox_entry);

	gtk_widget_set_sensitive (mailbox_entry, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (want_mailbox_check)));

	g_signal_connect (mailbox_entry, "changed", G_CALLBACK (mailbox_editor_entry_changed), data->config);
	g_object_set_data (G_OBJECT (button), "mailbox-entry", mailbox_entry);
	g_object_set_data (G_OBJECT (want_mailbox_check), "mailbox-entry", mailbox_entry);

	gtk_table_attach (GTK_TABLE (data->parent), label, 0, 1, row, row+1, 0, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (data->parent), mailbox_entry, 1, 2, row, row+1, GTK_FILL|GTK_EXPAND, GTK_FILL, 0, 0);

	g_free (owa_url);
	g_free (mailbox_name);
	g_free (username);

	return hbox;
}

gboolean
org_gnome_exchange_check_options(EPlugin *epl, EConfigHookPageCheckData *data)
{
	EMConfigTargetAccount *target = (EMConfigTargetAccount *)data->config->target;
	gint status = TRUE;

	/* We assume that if the host is set, then the setting is valid.
	   The host gets set when the provider validate() call is made */
	/* We do this check for receive page also, so that user can
	 * proceed with the account set up only after user is validated,
	 * and host name is reset by validate() call
	 */
	if (data->pageid == NULL ||
	    strcmp (data->pageid, "10.receive") == 0 ||
	    strcmp (data->pageid, "20.receive_options") == 0) {
		CamelURL *url;

		const gchar * target_url = e_account_get_string(target->account,  E_ACCOUNT_SOURCE_URL);
		if (target_url && target_url[0] != '\0')
			url = camel_url_new(target_url, NULL);
		else
			url = NULL;
		/* Note: we only care about exchange url's, we WILL get called on all other url's too. */
		if (url != NULL
		    && strcmp(url->protocol, "exchange") == 0
		    && (url->host == NULL || url->host[0] == 0))
			status = FALSE;

		if (url)
			camel_url_free(url);
	}

	return status;
}

static void
set_oof_info (GtkWidget *parent)
{
	ExchangeAccount *account;

	g_return_if_fail (oof_data != NULL);

	account = exchange_operations_get_exchange_account ();

	if (account && !exchange_oof_set (account, oof_data->state, oof_data->message)) {

		e_alert_run_dialog_for_args (GTK_WINDOW (parent), ERROR_DOMAIN ":state-update-error", NULL);
	}
}

static void
destroy_oof_data (void)
{
	if (oof_data && oof_data->message) {
		g_free (oof_data->message);
		oof_data->message = NULL;
	}

	if (oof_data) {
		g_free (oof_data);
		oof_data = NULL;
	}
}

void
org_gnome_exchange_commit (EPlugin *epl, EMConfigTargetAccount *target_account)
{
	const gchar *source_url;
	CamelURL *url;
	gint offline_status;

	source_url = e_account_get_string (target_account->account,  E_ACCOUNT_SOURCE_URL);
	if (source_url && source_url[0] != '\0')
		url = camel_url_new (source_url, NULL);
	else
		url = NULL;
	if (url == NULL
	    || strcmp (url->protocol, "exchange") != 0) {
		if (url)
			camel_url_free (url);

		return;
	}

	camel_url_free (url);

	exchange_config_listener_get_offline_status (exchange_global_config_listener,
								    &offline_status);

	if (offline_status == OFFLINE_MODE) {
                return;
	}

	/* Set oof data in exchange account */
	set_oof_info (target_account->target.widget);
	destroy_oof_data ();
	return;
}

static void
exchange_check_authtype (GtkWidget *w, EConfig *config)
{
	return;
}

static void
exchange_authtype_changed (GtkComboBox *dropdown, EConfig *config)
{
	EMConfigTargetAccount *target = (EMConfigTargetAccount *)config->target;
	gint id = gtk_combo_box_get_active(dropdown);
	GtkTreeModel *model;
	GtkTreeIter iter;
	CamelServiceAuthType *authtype;
	CamelURL *url_source, *url_transport;
	const gchar *source_url, *transport_url;
	gchar *source_url_string, *transport_url_string;

	source_url = e_account_get_string (target->account,
					   E_ACCOUNT_SOURCE_URL);
	if (id == -1)
		return;

	url_source = camel_url_new (source_url, NULL);

	transport_url = e_account_get_string (target->account,
					      E_ACCOUNT_TRANSPORT_URL);
	url_transport = camel_url_new (transport_url, NULL);

	model = gtk_combo_box_get_model(dropdown);
	if (gtk_tree_model_iter_nth_child(model, &iter, NULL, id)) {
		gtk_tree_model_get(model, &iter, 1, &authtype, -1);
		if (authtype) {
			camel_url_set_authmech(url_source, authtype->authproto);
			camel_url_set_authmech(url_transport, authtype->authproto);
		}
		else {
			camel_url_set_authmech(url_source, NULL);
			camel_url_set_authmech(url_transport, NULL);
		}

		source_url_string = camel_url_to_string(url_source, 0);
		transport_url_string = camel_url_to_string(url_transport, 0);
		e_account_set_string(target->account, E_ACCOUNT_SOURCE_URL, source_url_string);
		e_account_set_string(target->account, E_ACCOUNT_TRANSPORT_URL, transport_url_string);
		g_free(source_url_string);
		g_free(transport_url_string);
	}
	camel_url_free(url_source);
	camel_url_free(url_transport);
}

GtkWidget *
org_gnome_exchange_auth_section (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	EMConfigTargetAccount *target_account;
	const gchar *source_url;
	gchar *label_text, *exchange_account_authtype = NULL;
	CamelURL *url;
	GtkWidget *hbox, *button, *auth_label, *vbox, *label_hide;
	GtkComboBox *dropdown;
	GtkTreeIter iter;
	GtkListStore *store;
	gint i, active=0, auth_changed_id = 0;
	GList *authtypes, *l, *ll;
	ExchangeAccount *account;

	target_account = (EMConfigTargetAccount *)data->config->target;
	source_url = e_account_get_string (target_account->account,
					   E_ACCOUNT_SOURCE_URL);
	url = camel_url_new (source_url, NULL);
	if (url == NULL
	    || strcmp (url->protocol, "exchange") != 0) {
		if (url)
			camel_url_free (url);

		return NULL;
	}

	if (data->old) {
		camel_url_free(url);
		return data->old;
	}

	account = exchange_operations_get_exchange_account ();
	if (account)
		exchange_account_authtype = exchange_account_get_authtype (account);

	vbox = gtk_vbox_new (FALSE, 6);

	label_text = g_strdup_printf("<b>%s</b>", _("_Authentication Type"));
	auth_label = gtk_label_new_with_mnemonic (label_text);
	g_free (label_text);
	gtk_label_set_justify (GTK_LABEL (auth_label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (auth_label), 0, 0.5);
	gtk_misc_set_padding (GTK_MISC (auth_label), 0, 0);
	gtk_label_set_use_markup (GTK_LABEL (auth_label), TRUE);

	label_hide = gtk_label_new("\n");

	hbox = gtk_hbox_new (FALSE, 6);
	dropdown = (GtkComboBox * )gtk_combo_box_new ();
	gtk_label_set_mnemonic_widget (GTK_LABEL (auth_label), GTK_WIDGET (dropdown));

	button = gtk_button_new_with_mnemonic (_("Ch_eck for Supported Types"));

	authtypes = g_list_prepend (g_list_prepend (NULL, &camel_exchange_password_authtype),
				    &camel_exchange_ntlm_authtype);
	store = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_BOOLEAN);

	for (i=0, l=authtypes; l; l=l->next, i++) {
		CamelServiceAuthType *authtype = l->data;
		gint avail = TRUE;

		if (authtypes) {
			for (ll = authtypes; ll; ll = g_list_next(ll))
				if (!strcmp(authtype->authproto,
					((CamelServiceAuthType *)ll->data)->authproto))
					break;
			avail = ll != NULL;
		}
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, authtype->name, 1,
				    authtype, 2, !avail, -1);

		if (url && url->authmech && !strcmp(url->authmech, authtype->authproto)) {
			active = i;
		}
		else if (url && exchange_account_authtype &&
			 !strcmp (exchange_account_authtype, authtype->authproto)) {
			/* if the url doesn't contain authmech, read the value from
			 * exchange account and set the tab selection and
			 * also set the authmech back to url
			 */
			camel_url_set_authmech (url, exchange_account_authtype);
			active = i;
		}
	}

	gtk_combo_box_set_model (dropdown, (GtkTreeModel *)store);
	gtk_combo_box_set_active (dropdown, -1);

	if (auth_changed_id == 0) {
		GtkCellRenderer *cell = gtk_cell_renderer_text_new();

		gtk_cell_layout_pack_start ((GtkCellLayout *)dropdown, cell, TRUE);
		gtk_cell_layout_set_attributes ((GtkCellLayout *)dropdown, cell,
						"text", 0, "strikethrough", 2, NULL);

		auth_changed_id = g_signal_connect (dropdown,
						    "changed",
						    G_CALLBACK (exchange_authtype_changed),
						    data->config);
		g_signal_connect (button,
				  "clicked",
				  G_CALLBACK(exchange_check_authtype),
				  data->config);
	}

	gtk_combo_box_set_active(dropdown, active);

	gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (dropdown), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (vbox), auth_label, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), label_hide, TRUE, TRUE, 0);
	gtk_widget_show_all (vbox);

	gtk_box_pack_start (GTK_BOX (data->parent), vbox, TRUE, TRUE, 0);

	if (url)
		camel_url_free(url);
	g_list_free (authtypes);
	g_free (exchange_account_authtype);

	return vbox;
}

GtkWidget *
org_gnome_exchange_show_folder_size_factory (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	EMConfigTargetFolder *target=  (EMConfigTargetFolder *)data->config->target;
	CamelFolder *cml_folder = target->folder;
	CamelService *service;
	CamelProvider *provider;
	ExchangeAccount *account;
	GtkWidget *lbl_size, *lbl_size_val;
	GtkListStore *model;
	GtkVBox *vbx;
	GtkHBox *hbx_size;
	gchar *folder_name, *folder_size;
	gint mode;

	service = CAMEL_SERVICE (camel_folder_get_parent_store (cml_folder));
	if (!service)
		return NULL;

	provider = camel_service_get_provider (service);
	if (!provider)
		return NULL;

	if (g_ascii_strcasecmp (provider->protocol, "exchange"))
		return NULL;

	account = exchange_operations_get_exchange_account ();
	if (!account)
		return NULL;

	exchange_account_is_offline (account, &mode);
	if (mode == OFFLINE_MODE)
		return NULL;

	folder_name = (gchar *) camel_folder_get_name (cml_folder);
	if (!folder_name)
		folder_name = g_strdup ("name");

	model = exchange_account_folder_size_get_model (account);
	if (model)
		folder_size = g_strdup_printf (_("%s KB"), exchange_folder_size_get_val (model, folder_name));
	else
		folder_size = g_strdup (_("0 KB"));

	hbx_size = (GtkHBox*) gtk_hbox_new (FALSE, 0);
	vbx = (GtkVBox *)gtk_notebook_get_nth_page (GTK_NOTEBOOK (data->parent), 0);

	lbl_size = gtk_label_new_with_mnemonic (_("Size:"));
	lbl_size_val = gtk_label_new_with_mnemonic (_(folder_size));
	gtk_widget_show (lbl_size);
	gtk_widget_show (lbl_size_val);
	gtk_misc_set_alignment (GTK_MISC (lbl_size), 0.0, 0.5);
	gtk_misc_set_alignment (GTK_MISC (lbl_size_val), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (hbx_size), lbl_size, FALSE, TRUE, 12);
	gtk_box_pack_start (GTK_BOX (hbx_size), lbl_size_val, FALSE, TRUE, 10);
	gtk_widget_show_all (GTK_WIDGET (hbx_size));

	gtk_box_pack_start (GTK_BOX (vbx), GTK_WIDGET (hbx_size), FALSE, FALSE, 0);
	g_free (folder_size);

	return GTK_WIDGET (hbx_size);
}
