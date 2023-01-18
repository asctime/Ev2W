/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Copyright (C) 2003, 2004 Novell, Inc.
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

/* exchange-autoconfig-wizard: Automatic account configuration wizard */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "e2k-autoconfig.h"
#include <e2k-utils.h>
#include "exchange-autoconfig-wizard.h"

#include <libedataserver/e-account.h>
#include <libedataserver/e-account-list.h>
#include <libedataserverui/e-passwords.h>

#include <gconf/gconf-client.h>
#include <gtk/gtk.h>

#ifdef G_OS_WIN32

#ifdef G_OS_WIN32
#include <libedataserver/e-data-server-util.h>
#endif

#endif

typedef struct {
	GtkWidget *assistant;
	gint active_page;
	gboolean changing;

	E2kAutoconfig *ac;
	E2kOperation op;

	/* OWA Page */
	GtkEntry *owa_uri_entry;
	GtkEntry *username_entry;
	GtkEntry *password_entry;
	GtkToggleButton *remember_password_check;

	/* Global Catalog Page */
	GtkEntry *gc_server_entry;

	/* Failure Page */
	GtkBox *failure_vbox;
	GtkLabel *failure_label;
	GtkWidget *failure_href;

	/* Verify Page */
	GtkEntry *name_entry;
	GtkEntry *email_entry;
	GtkToggleButton *default_account_check;
} ExchangeAutoconfigGUI;

enum {
	EXCHANGE_AUTOCONFIG_PAGE_START,
	EXCHANGE_AUTOCONFIG_PAGE_OWA,
	EXCHANGE_AUTOCONFIG_PAGE_GC,
	EXCHANGE_AUTOCONFIG_PAGE_FAILURE,
	EXCHANGE_AUTOCONFIG_PAGE_VERIFY,
	EXCHANGE_AUTOCONFIG_PAGE_FINISH
};

static void assistant_page_content_changed (GtkWidget *sender, ExchangeAutoconfigGUI *gui);

static inline gboolean
check_field (GtkEntry *entry)
{
	return (*gtk_entry_get_text (entry) != '\0');
}

static void
fill_failure_page (ExchangeAutoconfigGUI *gui, const gchar *url, const gchar *fmt, ...)
{
	va_list ap;
	gchar *text;

	va_start (ap, fmt);
	text = g_strdup_vprintf (fmt, ap);
	va_end (ap);

	gtk_label_set_text (gui->failure_label, text);

	g_free (text);

	if (gui->failure_href)
		gtk_widget_destroy (gui->failure_href);

	if (url) {
		gui->failure_href = gtk_link_button_new (url);
		gtk_box_pack_start (gui->failure_vbox, gui->failure_href, FALSE, FALSE, 0);
		gtk_widget_show (gui->failure_href);
	} else {
		gui->failure_href = NULL;
	}
}

#define CONNECT_CHANGE_SIGNAL(_where,_signal_name) g_signal_connect (_where, _signal_name, G_CALLBACK (assistant_page_content_changed), gui)

static GtkWidget *
create_page_vbox (GtkAssistant *assistant, GdkPixbuf *logo, const gchar *page_title, const gchar *page_info, GtkAssistantPageType page_type, GtkWidget **page, gint *page_index)
{
	GtkWidget *vbox;
	gboolean is_edge = page_type == GTK_ASSISTANT_PAGE_INTRO || page_type == GTK_ASSISTANT_PAGE_CONFIRM;

	g_return_val_if_fail (assistant != NULL, NULL);
	g_return_val_if_fail (page != NULL, NULL);
	g_return_val_if_fail (page_index != NULL, NULL);

	vbox = gtk_vbox_new (FALSE, 0);

	gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);

	*page = vbox;
	*page_index = gtk_assistant_append_page (assistant, vbox);

	gtk_assistant_set_page_type (assistant, vbox, page_type);
	gtk_assistant_set_page_title (assistant, vbox, page_title);
	gtk_assistant_set_page_complete (assistant, vbox, is_edge);
	gtk_assistant_set_page_header_image (assistant, vbox, logo);

	if (page_info) {
		GtkWidget *label;

		label = gtk_label_new (page_info);
		gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
		gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

		gtk_box_pack_start (GTK_BOX (vbox), label, is_edge, is_edge, 0);
	}

	return vbox;
}

static GtkWidget *
add_table_row (GtkTable *table, gint row, const gchar *label_text, GtkWidget *action_widget)
{
	GtkWidget *w;

	w = gtk_label_new_with_mnemonic (label_text);
	gtk_misc_set_alignment (GTK_MISC (w), 0.0, 0.5);
	gtk_label_set_mnemonic_widget (GTK_LABEL (w), action_widget);
	gtk_table_attach (table, w, 0, 1, row, row + 1, GTK_FILL, 0, 0, 0);

	w = action_widget;
	gtk_table_attach (table, w, 1, 2, row, row + 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);

	return w;
}

static void
start_page_create (ExchangeAutoconfigGUI *gui, GdkPixbuf *logo, GtkWidget **page, gint *page_index)
{
	GtkWidget *vbox;

	vbox = create_page_vbox (GTK_ASSISTANT (gui->assistant), logo, _("Welcome"), _("Welcome to Evolution Connector for Microsoft Exchange.\nThe next few screens will help you configure Evolution\nto connect to your Exchange account.\n\nPlease click the \"Forward\" button to continue."), GTK_ASSISTANT_PAGE_INTRO, page, page_index);
	g_return_if_fail (vbox != NULL);

	gtk_widget_show_all (vbox);
}

static void
owa_page_create (ExchangeAutoconfigGUI *gui, GdkPixbuf *logo, GtkWidget **page, gint *page_index)
{
	GtkWidget *vbox, *w;
	GtkTable *table;

	vbox = create_page_vbox (GTK_ASSISTANT (gui->assistant), logo, _("Exchange Configuration"), _("Evolution Connector for Microsoft Exchange can use account information from your existing Outlook Web Access (OWA) account.\n\nEnter your OWA site address (URL), username, and password, then click \"Forward\".\n"), GTK_ASSISTANT_PAGE_CONTENT, page, page_index);
	g_return_if_fail (vbox != NULL);

	table = GTK_TABLE (gtk_table_new (4, 2, FALSE));
	gtk_container_set_border_width (GTK_CONTAINER (table), 6);
	gtk_table_set_row_spacings (table, 6);
	gtk_table_set_col_spacings (table, 6);

	w = add_table_row (table, 0, _("OWA _URL:"), gtk_entry_new ());
	CONNECT_CHANGE_SIGNAL (w, "changed");
	gui->owa_uri_entry = GTK_ENTRY (w);

	w = add_table_row (table, 1, _("User_name:"), gtk_entry_new ());
	CONNECT_CHANGE_SIGNAL (w, "changed");
	gui->username_entry = GTK_ENTRY (w);

	w = add_table_row (table, 2, _("_Password:"), gtk_entry_new ());
	CONNECT_CHANGE_SIGNAL (w, "changed");
	gui->password_entry = GTK_ENTRY (w);
	gtk_entry_set_visibility (gui->password_entry, FALSE);

	w = gtk_check_button_new_with_mnemonic (_("_Remember this password"));
	CONNECT_CHANGE_SIGNAL (w, "toggled");
	gtk_table_attach (table, w, 0, 2, 3, 4, GTK_FILL, 0, 0, 0);

	gui->remember_password_check = GTK_TOGGLE_BUTTON (w);

	gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (table), TRUE, TRUE, 0);

	gtk_widget_show_all (vbox);
}

static void
owa_page_prepare (ExchangeAutoconfigGUI *gui)
{
	if (gui->ac->username)
		gtk_entry_set_text (gui->username_entry, gui->ac->username);

	if (gui->ac->owa_uri)
		gtk_entry_set_text (gui->owa_uri_entry, gui->ac->owa_uri);
}

static gboolean
owa_page_check (ExchangeAutoconfigGUI *gui)
{
	return (check_field (gui->owa_uri_entry) &&
		check_field (gui->username_entry) &&
		check_field (gui->password_entry));
}

static gint
owa_page_get_next (ExchangeAutoconfigGUI *gui)
{
	E2kAutoconfigResult result;
	const gchar *old, *new;
	gint next_page = EXCHANGE_AUTOCONFIG_PAGE_FAILURE;

	e2k_autoconfig_set_owa_uri (gui->ac, gtk_entry_get_text (gui->owa_uri_entry));
	e2k_autoconfig_set_username (gui->ac, gtk_entry_get_text (gui->username_entry));
	e2k_autoconfig_set_password (gui->ac, gtk_entry_get_text (gui->password_entry));

	gtk_widget_set_sensitive (GTK_WIDGET (gui->assistant), FALSE);
	e2k_operation_init (&gui->op);
	result = e2k_autoconfig_check_exchange (gui->ac, &gui->op);

	if (result == E2K_AUTOCONFIG_OK) {
		result = e2k_autoconfig_check_global_catalog (gui->ac, &gui->op);
		e2k_operation_free (&gui->op);
		gtk_widget_set_sensitive (GTK_WIDGET (gui->assistant), TRUE);

		if (result == E2K_AUTOCONFIG_OK)
			next_page = EXCHANGE_AUTOCONFIG_PAGE_VERIFY;
		else
			next_page = EXCHANGE_AUTOCONFIG_PAGE_GC;

		return next_page;
	}

	/* Update the entries with anything we autodetected */
	owa_page_prepare (gui);
	e2k_operation_free (&gui->op);
	gtk_widget_set_sensitive (GTK_WIDGET (gui->assistant), TRUE);

	switch (result) {
	case E2K_AUTOCONFIG_CANT_CONNECT:
		if (!strncmp (gui->ac->owa_uri, "http:", 5)) {
			old = "http";
			new = "https";
		} else {
			old = "https";
			new = "http";
		}

		fill_failure_page (gui, NULL,
			  _("Could not connect to the Exchange "
			    "server.\nMake sure the URL is correct "
			    "(try \"%s\" instead of \"%s\"?) "
			    "and try again."), new, old);
		break;

	case E2K_AUTOCONFIG_CANT_RESOLVE:
		fill_failure_page (gui, NULL,
			  _("Could not locate Exchange server.\n"
			    "Make sure the server name is spelled correctly "
			    "and try again."));
		break;

	case E2K_AUTOCONFIG_AUTH_ERROR:
	case E2K_AUTOCONFIG_AUTH_ERROR_TRY_NTLM:
	case E2K_AUTOCONFIG_AUTH_ERROR_TRY_BASIC:
		fill_failure_page (gui, NULL,
			  _("Could not authenticate to the Exchange "
			    "server.\nMake sure the username and "
			    "password are correct and try again."));
		break;

	case E2K_AUTOCONFIG_AUTH_ERROR_TRY_DOMAIN:
		fill_failure_page (gui, NULL,
			  _("Could not authenticate to the Exchange "
			    "server.\nMake sure the username and "
			    "password are correct and try again.\n\n"
			    "You may need to specify the Windows "
			    "domain name as part of your username "
			    "(eg, \"MY-DOMAIN\\%s\")."),
			  gui->ac->username);
		break;

	case E2K_AUTOCONFIG_NO_OWA:
	case E2K_AUTOCONFIG_NOT_EXCHANGE:
		fill_failure_page (gui, NULL,
			  _("Could not find OWA data at the indicated URL.\n"
			    "Make sure the URL is correct and try again."));
		break;

	case E2K_AUTOCONFIG_CANT_BPROPFIND:
		fill_failure_page (gui, "http://support.novell.com/cgi-bin/search/searchtid.cgi?/ximian/ximian328.html",
			_("Evolution Connector for Microsoft Exchange requires "
			  "access to certain functionality on the Exchange "
			  "server that appears to be disabled or blocked.  "
			  "(This is usually unintentional.)  Your Exchange "
			  "administrator will need to enable this "
			  "functionality in order for you to be able to use "
			  "the Evolution Connector.\n\n"
			  "For information to provide to your Exchange "
			  "administrator, please follow the link below:"));
		break;

	case E2K_AUTOCONFIG_EXCHANGE_5_5:
		fill_failure_page (gui, NULL,
			_("The Exchange server URL you provided is for an "
			  "Exchange 5.5 server. Evolution Connector for "
			  "Microsoft Exchange supports Microsoft Exchange 2000 "
			  "and 2003 only."));
		break;

	default:
		fill_failure_page (gui, NULL,
			  _("Could not configure Exchange account because "
			    "an unknown error occurred. Check the URL, "
			    "username, and password, and try again."));
		break;
	}

	/* can move forward only when not requested to stay on the actual page */
	return next_page;
}

static void
gc_page_create (ExchangeAutoconfigGUI *gui, GdkPixbuf *logo, GtkWidget **page, gint *page_index)
{
	GtkWidget *vbox, *w;
	GtkTable *table;

	vbox = create_page_vbox (GTK_ASSISTANT (gui->assistant), logo, _("Exchange Configuration"), _("Evolution Connector for Microsoft Exchange could not find the Global Catalog replica for your site. Please enter the name of your Global Catalog server. You may need to ask your system administrator for the correct value."), GTK_ASSISTANT_PAGE_CONTENT, page, page_index);
	g_return_if_fail (vbox != NULL);

	table = GTK_TABLE (gtk_table_new (1, 2, FALSE));
	gtk_container_set_border_width (GTK_CONTAINER (table), 6);
	gtk_table_set_row_spacings (table, 6);
	gtk_table_set_col_spacings (table, 6);

	w = add_table_row (table, 0, _("GC _Server:"), gtk_entry_new ());
	CONNECT_CHANGE_SIGNAL (w, "changed");
	gui->gc_server_entry = GTK_ENTRY (w);

	gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (table), TRUE, TRUE, 0);

	gtk_widget_show_all (vbox);
}

static gboolean
gc_page_check (ExchangeAutoconfigGUI *gui)
{
	return check_field (gui->gc_server_entry);
}

static gint
gc_page_get_next (ExchangeAutoconfigGUI *gui)
{
	E2kAutoconfigResult result;
	gint next_page = EXCHANGE_AUTOCONFIG_PAGE_FAILURE;

	e2k_autoconfig_set_gc_server (gui->ac, gtk_entry_get_text (gui->gc_server_entry), -1, E2K_AUTOCONFIG_USE_GAL_DEFAULT);

	gtk_widget_set_sensitive (GTK_WIDGET (gui->assistant), FALSE);
	e2k_operation_init (&gui->op);
	result = e2k_autoconfig_check_global_catalog (gui->ac, &gui->op);
	e2k_operation_free (&gui->op);
	gtk_widget_set_sensitive (GTK_WIDGET (gui->assistant), TRUE);

	if (result == E2K_AUTOCONFIG_OK) {
		next_page = EXCHANGE_AUTOCONFIG_PAGE_VERIFY;
	} else if (result == E2K_AUTOCONFIG_AUTH_ERROR_TRY_DOMAIN) {
		fill_failure_page (gui, NULL,
			  _("Could not authenticate to the Global Catalog "
			    "server. You may need to go back and specify "
			    "the Windows domain name as part of your "
			    "username (eg, \"MY-DOMAIN\\%s\")."),
			  gui->ac->username);
	} else {
		fill_failure_page (gui, NULL,
			  _("Could not connect to specified server.\n"
			    "Please check the server name and try again."));
	}

	return next_page;
}

static void
failure_page_create (ExchangeAutoconfigGUI *gui, GdkPixbuf *logo, GtkWidget **page, gint *page_index)
{
	GtkWidget *vbox, *w;

	vbox = create_page_vbox (GTK_ASSISTANT (gui->assistant), logo, _("Configuration Failed"), _("Evolution Connector for Microsoft Exchange has encountered a problem configuring your Exchange account."), GTK_ASSISTANT_PAGE_CONTENT, page, page_index);
	g_return_if_fail (vbox != NULL);

	w = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (w), 0.0, 0.0);
	gtk_label_set_line_wrap (GTK_LABEL (w), TRUE);

	gui->failure_vbox = GTK_BOX (vbox);
	gui->failure_label = GTK_LABEL (w);

	gtk_box_pack_start (GTK_BOX (vbox), w, FALSE, FALSE, 0);

	gtk_widget_show_all (vbox);
}

static gboolean
failure_page_check (ExchangeAutoconfigGUI *gui)
{
	return FALSE;
}

static void
verify_page_create (ExchangeAutoconfigGUI *gui, GdkPixbuf *logo, GtkWidget **page, gint *page_index)
{
	GtkWidget *vbox, *w;
	GtkTable *table;

	vbox = create_page_vbox (GTK_ASSISTANT (gui->assistant), logo, _("Exchange Configuration"), _("Your account information is as follows. Please correct any errors, then click \"Forward\"."), GTK_ASSISTANT_PAGE_CONTENT, page, page_index);
	g_return_if_fail (vbox != NULL);

	table = GTK_TABLE (gtk_table_new (3, 2, FALSE));
	gtk_container_set_border_width (GTK_CONTAINER (table), 6);
	gtk_table_set_row_spacings (table, 6);
	gtk_table_set_col_spacings (table, 6);

	w = add_table_row (table, 0, _("Full _Name:"), gtk_entry_new ());
	CONNECT_CHANGE_SIGNAL (w, "changed");
	gui->name_entry = GTK_ENTRY (w);

	w = add_table_row (table, 1, _("_Email Address:"), gtk_entry_new ());
	CONNECT_CHANGE_SIGNAL (w, "changed");
	gui->email_entry = GTK_ENTRY (w);

	w = gtk_check_button_new_with_label (_("Make this my _default account"));
	CONNECT_CHANGE_SIGNAL (w, "toggled");
	gtk_table_attach (table, w, 0, 2, 2, 3, GTK_FILL, 0, 0, 10);

	gui->default_account_check = GTK_TOGGLE_BUTTON (w);

	gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (table), TRUE, TRUE, 0);

	gtk_widget_show_all (vbox);
}

static void
verify_page_prepare (ExchangeAutoconfigGUI *gui)
{
	if (gui->ac->display_name)
		gtk_entry_set_text (gui->name_entry, gui->ac->display_name);
	else
		gtk_entry_set_text (gui->name_entry, _("Unknown"));
	if (gui->ac->email)
		gtk_entry_set_text (gui->email_entry, gui->ac->email);
	else
		gtk_entry_set_text (gui->email_entry, _("Unknown"));
}

static gboolean
verify_page_check (ExchangeAutoconfigGUI *gui)
{
	const gchar *email;

	g_free (gui->ac->display_name);
	gui->ac->display_name = g_strdup (gtk_entry_get_text (gui->name_entry));
	g_free (gui->ac->email);
	gui->ac->email = g_strdup (gtk_entry_get_text (gui->email_entry));

	email = gtk_entry_get_text (gui->email_entry);

	return (check_field (gui->name_entry) && strchr (email, '@'));
}

static void
finish_page_create (ExchangeAutoconfigGUI *gui, GdkPixbuf *logo, GtkWidget **page, gint *page_index)
{
	GtkWidget *vbox;

	vbox = create_page_vbox (GTK_ASSISTANT (gui->assistant), logo, _("Done"), _("Your Connector account is now ready to use. Click the \"Apply\" button to save your settings."), GTK_ASSISTANT_PAGE_CONFIRM, page, page_index);
	g_return_if_fail (vbox != NULL);

	gtk_widget_show_all (vbox);
}

#undef CONNECT_CHANGE_SIGNAL

static gboolean
is_active_exchange_account (EAccount *account)
{
	if (!account->enabled)
		return FALSE;
	if (!account->source || !account->source->url)
		return FALSE;
	return (strncmp (account->source->url, "exchange://", 11) == 0);
}

static void
autoconfig_notice (gpointer parent,
                   GtkMessageType type,
                   const gchar *string)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (
		parent, GTK_DIALOG_DESTROY_WITH_PARENT, type,
		GTK_BUTTONS_OK, "%s", string);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

static void
autoconfig_gui_apply (ExchangeAutoconfigGUI *gui)
{
	EAccountList *list;
	EAccount *account;
	EIterator *iter;
	GConfClient *gconf;
	gchar *pw_key;
	gboolean found = FALSE;

	/* Create the account. */
	gconf = gconf_client_get_default ();
	list = e_account_list_new (gconf);
	g_object_unref (gconf);
	if (!list) {
		autoconfig_notice (
			gui->assistant, GTK_MESSAGE_ERROR,
			_("Configuration system error.\n"
			  "Unable to create new account."));
		return;
	}

	/* Check if any account is already configured, and throw an error if so */
	/* NOTE: This condition check needs to be removed when we start
	   supporting multiple accounts */
	for (iter = e_list_get_iterator ((EList *)list);
	     e_iterator_is_valid (iter);
	     e_iterator_next (iter)) {
		account = (EAccount *)e_iterator_get (iter);
		if (account && (found = is_active_exchange_account (account))) {
			autoconfig_notice (
				gui->assistant, GTK_MESSAGE_ERROR,
				_("You may only configure a single Exchange account"));
			break;
		}
		account = NULL;
	}
	g_object_unref(iter);
	if (found)
		return;

	account = e_account_new ();

	account->name = g_strdup (gui->ac->email);
	account->enabled = TRUE;
	account->pgp_no_imip_sign = TRUE;

	account->id->name = g_strdup (gui->ac->display_name);
	account->id->address = g_strdup (gui->ac->email);

	account->source->url = g_strdup (gui->ac->account_uri);
	account->transport->url = g_strdup (gui->ac->account_uri);

	/* Remember the password at least for this session */
	pw_key = g_strdup_printf ("exchange://%s@%s",
				  gui->ac->username,
				  gui->ac->exchange_server);
	e_passwords_add_password (pw_key, gui->ac->password);

	/* Maybe longer */
	if (gtk_toggle_button_get_active (gui->remember_password_check)) {
		account->source->save_passwd = TRUE;
		e_passwords_remember_password ("Exchange", pw_key);
	}
	g_free (pw_key);

	e_account_list_add (list, account);
	if (gtk_toggle_button_get_active (gui->default_account_check))
		e_account_list_set_default (list, account);
	g_object_unref (account);
	e_account_list_save (list);
	g_object_unref (list);
}

static void
autoconfig_gui_free (ExchangeAutoconfigGUI *gui)
{
	e2k_autoconfig_free (gui->ac);
	g_free (gui);
}

static struct {
	GtkWidget *assistant_page;
	gint assistant_page_index;
	void (*create_func) (ExchangeAutoconfigGUI *gui, GdkPixbuf *logo, GtkWidget **page, gint *page_index);
	void (*prepare_func) (ExchangeAutoconfigGUI *gui);
	gboolean (*check_func) (ExchangeAutoconfigGUI *gui);
	gint (*get_next_page_func) (ExchangeAutoconfigGUI *gui);
} autoconfig_pages[] = {
	{ NULL, -1, start_page_create, NULL, NULL, NULL },
	{ NULL, -1, owa_page_create,     owa_page_prepare,    owa_page_check,     owa_page_get_next },
	{ NULL, -1, gc_page_create,      NULL,                gc_page_check,      gc_page_get_next  },
	{ NULL, -1, failure_page_create, NULL,                failure_page_check, NULL              },
	{ NULL, -1, verify_page_create,  verify_page_prepare, verify_page_check,  NULL              },
	{ NULL, -1, finish_page_create, NULL, NULL, NULL }
};

/* Autoconfig assistant */

static gint
find_page (ExchangeAutoconfigGUI *gui, GtkWidget *page)
{
	gint page_num;

	for (page_num = 0; page_num < G_N_ELEMENTS (autoconfig_pages); page_num++) {
		if (autoconfig_pages[page_num].assistant_page == page)
			return page_num;
	}

	return -1;
}

static void
assistant_prepare_cb (GtkAssistant *assistant, GtkWidget *page, ExchangeAutoconfigGUI *gui)
{
	gint page_num = find_page (gui, page);

	if (page_num == -1)
		return;

	gui->changing = TRUE;

	if (autoconfig_pages[page_num].prepare_func)
		autoconfig_pages[page_num].prepare_func (gui);

	gtk_assistant_set_page_complete (
		GTK_ASSISTANT (gui->assistant),
		autoconfig_pages[page_num].assistant_page,
		autoconfig_pages[page_num].check_func == NULL || autoconfig_pages[page_num].check_func (gui));

	gui->changing = FALSE;
	gui->active_page = page_num;
}

static void
assistant_page_content_changed (GtkWidget *sender, ExchangeAutoconfigGUI *gui)
{
	gint page_num = gtk_assistant_get_current_page (GTK_ASSISTANT (gui->assistant));

	if (page_num == -1)
		return;

	g_return_if_fail (page_num >= 0 && page_num < G_N_ELEMENTS (autoconfig_pages));

	gui->changing = TRUE;

	gtk_assistant_set_page_complete (
		GTK_ASSISTANT (gui->assistant),
		autoconfig_pages[page_num].assistant_page,
		autoconfig_pages[page_num].check_func == NULL || autoconfig_pages[page_num].check_func (gui));

	gui->changing = FALSE;
}

static void
assistant_apply_cb (GtkAssistant *assistant, ExchangeAutoconfigGUI *gui)
{
	autoconfig_gui_apply (gui);
}

static void
assistant_cancel_cb (GtkAssistant *assistant, ExchangeAutoconfigGUI *gui)
{
	gtk_widget_destroy (GTK_WIDGET (gui->assistant));
}

static void
assistant_destroyed (gpointer gui, GObject *where_druid_was)
{
	gtk_main_quit ();
}

static gint
assistant_forward_func (gint current_page, gpointer user_data)
{
	ExchangeAutoconfigGUI *gui = user_data;
	gint next_page = current_page + 1;

	if (!gui->changing && gui->active_page == current_page) {
		gint page_num;

		for (page_num = 0; page_num < G_N_ELEMENTS (autoconfig_pages); page_num++) {
			if (autoconfig_pages[page_num].assistant_page_index == current_page) {
				if (autoconfig_pages[page_num].get_next_page_func)
					next_page = autoconfig_pages[page_num].get_next_page_func (gui);
				break;
			}
		}
	}

	return next_page;
}

void
exchange_autoconfig_assistant_run (void)
{
	ExchangeAutoconfigGUI *gui;
	GdkPixbuf *icon;
	gchar *pngfile;
	gint i;

	gui = g_new0 (ExchangeAutoconfigGUI, 1);
	gui->ac = e2k_autoconfig_new (NULL, NULL, NULL, E2K_AUTOCONFIG_USE_EITHER);
	gui->assistant = gtk_assistant_new ();
	gui->active_page = EXCHANGE_AUTOCONFIG_PAGE_START;
	gui->changing = FALSE;

	gtk_window_set_title (GTK_WINDOW (gui->assistant), _("Evolution Connector for Microsoft Exchange Configuration"));

	gtk_assistant_set_forward_page_func (GTK_ASSISTANT (gui->assistant), assistant_forward_func, gui, NULL);

	g_signal_connect (gui->assistant, "cancel", G_CALLBACK (assistant_cancel_cb), gui);
	g_signal_connect (gui->assistant, "close", G_CALLBACK (assistant_cancel_cb), gui);
	g_signal_connect (gui->assistant, "apply", G_CALLBACK (assistant_apply_cb), gui);
	g_signal_connect (gui->assistant, "prepare", G_CALLBACK (assistant_prepare_cb), gui);

	pngfile = g_build_filename (CONNECTOR_IMAGESDIR,
				    "connector.png",
				    NULL);
	icon = gdk_pixbuf_new_from_file (pngfile, NULL);
	g_free (pngfile);

	for (i = 0; i < G_N_ELEMENTS (autoconfig_pages); i++) {
		autoconfig_pages[i].create_func (gui, icon, &autoconfig_pages[i].assistant_page, &autoconfig_pages[i].assistant_page_index);
	}

	g_object_unref (icon);

	g_object_weak_ref (G_OBJECT (gui->assistant), assistant_destroyed, gui);

	gtk_widget_show_all (GTK_WIDGET (gui->assistant));
	gtk_main ();
	autoconfig_gui_free (gui);
}
