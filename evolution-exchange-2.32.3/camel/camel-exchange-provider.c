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

/* camel-exchange-provider.c: exchange provider registration code */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <libedataserver/e-data-server-util.h>

#include <glib/gi18n-lib.h>

#include "camel-exchange-store.h"
#include "camel-exchange-transport.h"

static guint exchange_url_hash (gconstpointer key);
static gint exchange_url_equal (gconstpointer a, gconstpointer b);

#ifdef G_OS_WIN32

static const gchar *
get_localedir (void)
{
	return e_util_replace_prefix (PREFIX, e_util_get_cp_prefix (), CONNECTOR_LOCALEDIR);
}

#undef CONNECTOR_LOCALEDIR
#define CONNECTOR_LOCALEDIR get_localedir ()

#endif

static const gchar *auth_types[] = {
	N_("Secure or Plaintext Password"),
	N_("Plaintext Password"),
	N_("Secure Password"),
	NULL
};

CamelProviderConfEntry exchange_conf_entries[] = {
	{ CAMEL_PROVIDER_CONF_SECTION_START, "mailcheck", NULL,
	  N_("Checking for New Mail") },
	{ CAMEL_PROVIDER_CONF_CHECKBOX, "check_all", NULL,
	  N_("C_heck for new messages in all folders"), "1" },
	{ CAMEL_PROVIDER_CONF_SECTION_END },

	/* override the labels/defaults of the standard settings */
	{ CAMEL_PROVIDER_CONF_LABEL, "username", NULL,
	  /* i18n: the '_' should appear before the same letter it
	     does in the evolution:mail-config.glade "User_name"
	     translation (or not at all) */
	  N_("Windows User_name:") },

	/* extra Exchange configuration settings */
	{ CAMEL_PROVIDER_CONF_SECTION_START, "activedirectory", NULL,
	  /* i18n: GAL is an Outlookism, AD is a Windowsism */
	  N_("Global Address List/Active Directory") },
	{ CAMEL_PROVIDER_CONF_ENTRY, "ad_server", NULL,
	  /* i18n: "Global Catalog" is a Windowsism, but it's a
	     technical term and may not have translations? */
	  N_("_Global Catalog server name:") },
	{ CAMEL_PROVIDER_CONF_CHECKSPIN, "ad_limit", NULL,
	  N_("_Limit number of GAL responses: %s"), "y:1:500:10000" },
	{ CAMEL_PROVIDER_CONF_OPTIONS, "ad_auth", NULL,
	  N_("Authentication _Type:"), "default:Secure or Plaintext Password:basic:Plaintext Password:ntlm:Secure Password" },
	{ CAMEL_PROVIDER_CONF_CHECKBOX, "ad_browse", NULL,
	  N_("Allow _browsing of the GAL until download limit is reached"), "0" },
	{ CAMEL_PROVIDER_CONF_CHECKBOX, "ad_expand_groups", NULL,
	  N_("_Expand groups of contacts in GAL to contact lists"), "0" },
	{ CAMEL_PROVIDER_CONF_SECTION_END },
	{ CAMEL_PROVIDER_CONF_SECTION_START, "generals", NULL,
	  N_("Options") },
	{ CAMEL_PROVIDER_CONF_CHECKSPIN, "passwd_exp_warn_period", NULL,
	  N_("_Password Expiry Warning period: %s"), "y:1:7:90" },
	{ CAMEL_PROVIDER_CONF_CHECKBOX, "sync_offline", NULL,
	  N_("Automatically synchroni_ze account locally"), "0" },
	{ CAMEL_PROVIDER_CONF_CHECKBOX, "filter", NULL,
	  /* i18n: copy from evolution:camel-imap-provider.c */
	  N_("_Apply filters to new messages in Inbox on this server"), "0" },
	{ CAMEL_PROVIDER_CONF_CHECKBOX, "filter_junk", NULL,
	  N_("Check new messages for _Junk contents"), "0" },
	{ CAMEL_PROVIDER_CONF_CHECKBOX, "filter_junk_inbox", "filter_junk",
	  N_("Only check for Junk messag_es in the Inbox folder"), "0" },
	{ CAMEL_PROVIDER_CONF_HIDDEN, "auth-domain", NULL,
	  NULL, "Exchange" },

	{ CAMEL_PROVIDER_CONF_SECTION_END },
	{ CAMEL_PROVIDER_CONF_END }
};

static CamelProvider exchange_provider = {
	"exchange",
	N_("Microsoft Exchange"),

	N_("For handling mail (and other data) on Microsoft Exchange servers"),

	"mail",

	CAMEL_PROVIDER_IS_REMOTE | CAMEL_PROVIDER_IS_SOURCE |
	CAMEL_PROVIDER_IS_STORAGE | CAMEL_PROVIDER_IS_EXTERNAL,

	CAMEL_URL_NEED_USER | CAMEL_URL_HIDDEN_AUTH | CAMEL_URL_HIDDEN_HOST,

	exchange_conf_entries

	/* ... */
};

CamelServiceAuthType camel_exchange_ntlm_authtype = {
	/* i18n: "Secure Password Authentication" is an Outlookism */
	N_("Secure Password"),

	/* i18n: "NTLM" probably doesn't translate */
	N_("This option will connect to the Exchange server using "
	   "secure password (NTLM) authentication."),

	"",
	TRUE
};

CamelServiceAuthType camel_exchange_password_authtype = {
	N_("Plaintext Password"),

	N_("This option will connect to the Exchange server using "
	   "standard plaintext password authentication."),

	"Basic",
	TRUE
};

static gint
exchange_auto_detect_cb (CamelURL *url, GHashTable **auto_detected,
			 GError **error)
{
	*auto_detected = g_hash_table_new (g_str_hash, g_str_equal);

	g_hash_table_insert (*auto_detected, g_strdup ("mailbox"),
			     g_strdup (url->user));
	g_hash_table_insert (*auto_detected, g_strdup ("pf_server"),
			     g_strdup (url->host));
	g_hash_table_insert (*auto_detected, g_strdup ("ad_server"),
			     g_strdup (camel_url_get_param (url, "ad_server")));

	return 0;
}

void
camel_provider_module_init (void)
{
	gint i;

	exchange_provider.translation_domain = (gchar *) GETTEXT_PACKAGE;
	exchange_provider.object_types[CAMEL_PROVIDER_STORE] = camel_exchange_store_get_type ();
	exchange_provider.object_types[CAMEL_PROVIDER_TRANSPORT] = camel_exchange_transport_get_type ();
	exchange_provider.authtypes = g_list_prepend (g_list_prepend (NULL, &camel_exchange_password_authtype), &camel_exchange_ntlm_authtype);
	exchange_provider.url_hash = exchange_url_hash;
	exchange_provider.url_equal = exchange_url_equal;
	exchange_provider.auto_detect = exchange_auto_detect_cb;

	bindtextdomain (GETTEXT_PACKAGE, CONNECTOR_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

	/* 'auth_types' is not used anywhere else, it's there just for localization of the 'al_auth' */
	for (i = 0; auth_types[i]; i++) {
		auth_types[i] = _(auth_types[i]);
	}

	camel_provider_register (&exchange_provider);
}

static const gchar *
exchange_username (const gchar *user)
{
	const gchar *p;

	if (user) {
		p = strpbrk (user, "\\/");
		if (p)
			return p + 1;
	}

	return user;
}

static guint
exchange_url_hash (gconstpointer key)
{
	const CamelURL *u = (CamelURL *)key;
	guint hash = 0;

	if (u->user)
		hash ^= g_str_hash (exchange_username (u->user));
	if (u->host)
		hash ^= g_str_hash (u->host);

	return hash;
}

static gboolean
check_equal (const gchar *s1, const gchar *s2)
{
	if (!s1)
		return s2 == NULL;
	else if (!s2)
		return FALSE;
	else
		return strcmp (s1, s2) == 0;
}

static gint
exchange_url_equal (gconstpointer a, gconstpointer b)
{
	const CamelURL *u1 = a, *u2 = b;

	return  check_equal (u1->protocol, u2->protocol) &&
		check_equal (exchange_username (u1->user),
			     exchange_username (u2->user)) &&
		check_equal (u1->host, u2->host);
}
