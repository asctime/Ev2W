/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Authors: Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>

#include <glib/gi18n-lib.h>

#include "camel-maildir-store.h"
#include "camel-mbox-store.h"
#include "camel-mh-store.h"
#include "camel-spool-store.h"

#define d(x)

#ifndef G_OS_WIN32

static CamelProviderConfEntry mh_conf_entries[] = {
	CAMEL_PROVIDER_CONF_DEFAULT_PATH,
	{ CAMEL_PROVIDER_CONF_SECTION_START, "general", NULL, N_("Options") },
	{ CAMEL_PROVIDER_CONF_CHECKBOX, "dotfolders", NULL,
	  N_("_Use the '.folders' folder summary file (exmh)"), "0" },
	{ CAMEL_PROVIDER_CONF_SECTION_END },
	{ CAMEL_PROVIDER_CONF_END }
};

static CamelProvider mh_provider = {
	"mh",
	N_("MH-format mail directories"),
	N_("For storing local mail in MH-like mail directories."),
	"mail",
	CAMEL_PROVIDER_IS_SOURCE | CAMEL_PROVIDER_IS_STORAGE | CAMEL_PROVIDER_IS_LOCAL,
	CAMEL_URL_NEED_PATH | CAMEL_URL_NEED_PATH_DIR | CAMEL_URL_PATH_IS_ABSOLUTE | CAMEL_URL_FRAGMENT_IS_PATH,
	mh_conf_entries,
	/* ... */
};

#endif

static CamelProviderConfEntry mbox_conf_entries[] = {
	CAMEL_PROVIDER_CONF_DEFAULT_PATH,
	{ CAMEL_PROVIDER_CONF_END }
};

static CamelProvider mbox_provider = {
	"mbox",
	N_("Local delivery"),
	N_("For retrieving (moving) local mail from standard mbox-formatted spools into folders managed by Evolution."),
	"mail",
	CAMEL_PROVIDER_IS_SOURCE | CAMEL_PROVIDER_IS_STORAGE | CAMEL_PROVIDER_IS_LOCAL,
	CAMEL_URL_NEED_PATH | CAMEL_URL_PATH_IS_ABSOLUTE | CAMEL_URL_FRAGMENT_IS_PATH,
	mbox_conf_entries,
	/* ... */
};

/* MinGW can support maildir on Windows */
#if defined (__MINGW32__) || !defined (G_OS_WIN32)
static CamelProviderConfEntry maildir_conf_entries[] = {
	CAMEL_PROVIDER_CONF_DEFAULT_PATH,
	{ CAMEL_PROVIDER_CONF_SECTION_START, "general", NULL, N_("Options") },
	{ CAMEL_PROVIDER_CONF_CHECKBOX, "filter", NULL,
	  N_("_Apply filters to new messages in INBOX"), "0" },
	{ CAMEL_PROVIDER_CONF_SECTION_END },
	{ CAMEL_PROVIDER_CONF_END }
};

static CamelProvider maildir_provider = {
	"maildir",
	N_("Maildir-format mail directories"),
	N_("For storing local mail in maildir directories."),
	"mail",
	CAMEL_PROVIDER_IS_SOURCE | CAMEL_PROVIDER_IS_STORAGE | CAMEL_PROVIDER_IS_LOCAL,
	CAMEL_URL_NEED_PATH | CAMEL_URL_NEED_PATH_DIR | CAMEL_URL_PATH_IS_ABSOLUTE | CAMEL_URL_FRAGMENT_IS_PATH,
	maildir_conf_entries,
	/* ... */
};

#ifndef __MINGW32__
static CamelProviderConfEntry spool_conf_entries[] = {
	CAMEL_PROVIDER_CONF_DEFAULT_PATH,
	{ CAMEL_PROVIDER_CONF_SECTION_START, "general", NULL, N_("Options") },
	{ CAMEL_PROVIDER_CONF_CHECKBOX, "filter", NULL, N_("_Apply filters to new messages in INBOX"), "0" },
	{ CAMEL_PROVIDER_CONF_CHECKBOX, "xstatus", NULL, N_("_Store status headers in Elm/Pine/Mutt format"), "0" },
	{ CAMEL_PROVIDER_CONF_SECTION_END },
	{ CAMEL_PROVIDER_CONF_END }
};

static CamelProvider spool_file_provider = {
	"spool",
	N_("Standard Unix mbox spool file"),
	N_("For reading and storing local mail in external standard mbox spool files.\nMay also be used to read a tree of Elm, Pine, or Mutt style folders."),
	"mail",
	CAMEL_PROVIDER_IS_SOURCE | CAMEL_PROVIDER_IS_STORAGE,
	CAMEL_URL_NEED_PATH | CAMEL_URL_PATH_IS_ABSOLUTE | CAMEL_URL_FRAGMENT_IS_PATH,
	spool_conf_entries,
	/* ... */
};

static CamelProvider spool_directory_provider = {
	"spooldir",
	N_("Standard Unix mbox spool directory"),
	N_("For reading and storing local mail in external standard mbox spool files.\nMay also be used to read a tree of Elm, Pine, or Mutt style folders."),
	"mail",
	CAMEL_PROVIDER_IS_SOURCE | CAMEL_PROVIDER_IS_STORAGE,
	CAMEL_URL_NEED_PATH | CAMEL_URL_NEED_PATH_DIR | CAMEL_URL_PATH_IS_ABSOLUTE | CAMEL_URL_FRAGMENT_IS_PATH,
	spool_conf_entries,
	/* ... */
};

#endif  /* MinGW32 */
#endif

/* build a canonical 'path' */
static gchar *
make_can_path(gchar *p, gchar *o)
{
	gchar c, last, *start = o;

	d(printf("canonical '%s' = ", p));

	last = 0;
	while ((c = *p++)) {
		if (c!='/'
		    || (c=='/' && last != '/'))
			*o++ = c;
		last = c;
	}
	if (o>start && o[-1] == '/')
		o[-1] = 0;
	else
		*o = 0;

	d(printf("'%s'\n", start));

	return start;
}

/* 'helper' function for it */
#define get_can_path(p) ((p == NULL) ? NULL : (make_can_path ((p), g_alloca (strlen (p) + 1))))

static guint
local_url_hash (gconstpointer v)
{
	const CamelURL *u = v;
	guint hash = 0;

#define ADD_HASH(s) if (s && *s) hash ^= g_str_hash (s);

	ADD_HASH (u->protocol);
	ADD_HASH (u->user);
	ADD_HASH (u->authmech);
	ADD_HASH (u->host);
	if (u->path)
		hash ^= g_str_hash(get_can_path(u->path));
	ADD_HASH (u->path);
	ADD_HASH (u->query);
	hash ^= u->port;

	return hash;
}

static gint
check_equal (gchar *s1, gchar *s2)
{
	if (s1 == NULL || *s1 == 0) {
		if (s2 == NULL || *s2 == 0)
			return TRUE;
		else
			return FALSE;
	}

	if (s2 == NULL)
		return FALSE;

	return strcmp (s1, s2) == 0;
}

static gint
local_url_equal(gconstpointer v, gconstpointer v2)
{
	const CamelURL *u1 = v, *u2 = v2;
	gchar *p1, *p2;

	p1 = get_can_path(u1->path);
	p2 = get_can_path(u2->path);
	return check_equal(p1, p2)
		&& check_equal(u1->protocol, u2->protocol);
}

void
camel_provider_module_init(void)
{
#ifndef G_OS_WIN32
	gchar *path;
#endif
	static gint init = 0;

	if (init)
		abort();
	init = 1;

#ifndef G_OS_WIN32
	mh_conf_entries[0].value = "";  /* default path */
	mh_provider.object_types[CAMEL_PROVIDER_STORE] = camel_mh_store_get_type ();
	mh_provider.url_hash = local_url_hash;
	mh_provider.url_equal = local_url_equal;
	mh_provider.translation_domain = GETTEXT_PACKAGE;
	camel_provider_register(&mh_provider);
#endif

#ifndef G_OS_WIN32
	if (!(path = getenv ("MAIL")))
		path = g_strdup_printf (SYSTEM_MAIL_DIR "/%s", g_get_user_name ());
	mbox_conf_entries[0].value = path;  /* default path */
#else
	mbox_conf_entries[0].value = "";  /* default path */
#endif
	mbox_provider.object_types[CAMEL_PROVIDER_STORE] = CAMEL_TYPE_MBOX_STORE;
	mbox_provider.url_hash = local_url_hash;
	mbox_provider.url_equal = local_url_equal;
	mbox_provider.translation_domain = GETTEXT_PACKAGE;
	camel_provider_register(&mbox_provider);

/* MinGW can support maildir on Windows */
#if defined (__MINGW32__) || !defined (G_OS_WIN32)
#ifndef __MINGW32__
	spool_conf_entries[0].value = path;  /* default path - same as mbox; it's for both file and directory */
	spool_file_provider.object_types[CAMEL_PROVIDER_STORE] = camel_spool_store_get_type ();
	spool_file_provider.url_hash = local_url_hash;
	spool_file_provider.url_equal = local_url_equal;
	spool_file_provider.translation_domain = GETTEXT_PACKAGE;
	camel_provider_register(&spool_file_provider);

	spool_directory_provider.object_types[CAMEL_PROVIDER_STORE] = camel_spool_store_get_type ();
	spool_directory_provider.url_hash = local_url_hash;
	spool_directory_provider.url_equal = local_url_equal;
	spool_directory_provider.translation_domain = GETTEXT_PACKAGE;
	camel_provider_register(&spool_directory_provider);
	
  path = getenv("MAILDIR");
	maildir_conf_entries[0].value = path ? path : "";  /* default path */
#else
	maildir_conf_entries[0].value = "";  /* default path */
#endif /* MINGW32 */
	maildir_provider.object_types[CAMEL_PROVIDER_STORE] = camel_maildir_store_get_type ();
	maildir_provider.url_hash = local_url_hash;
	maildir_provider.url_equal = local_url_equal;
	maildir_provider.translation_domain = GETTEXT_PACKAGE;
	camel_provider_register(&maildir_provider);
#endif
}
