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

/* camel-exchange-search.c: exchange folder search */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "camel-exchange-search.h"
#include "camel-exchange-folder.h"
#include "camel-exchange-utils.h"

G_DEFINE_TYPE (CamelExchangeSearch, camel_exchange_search, CAMEL_TYPE_FOLDER_SEARCH)

static ESExpResult *
exchange_search_body_contains (struct _ESExp *f,
                               gint argc,
                               struct _ESExpResult **argv,
                               CamelFolderSearch *s)
{
	CamelFolderSearchClass *folder_search_class;
	gchar *value = argv[0]->value.string, *real_uid;
	const gchar *uid;
	ESExpResult *r;
	CamelMessageInfo *info;
	CamelOfflineStore *offline_store;
	CamelStore *parent_store;
	GHashTable *uid_hash = NULL;
	GPtrArray *found_uids;
	const gchar *full_name;
	gint i;

	folder_search_class = CAMEL_FOLDER_SEARCH_CLASS (
		camel_exchange_search_parent_class);

	full_name = camel_folder_get_full_name (s->folder);
	parent_store = camel_folder_get_parent_store (s->folder);

	offline_store = CAMEL_OFFLINE_STORE (parent_store);

	if (offline_store->state == CAMEL_OFFLINE_STORE_NETWORK_UNAVAIL)
		return folder_search_class->body_contains (f, argc, argv, s);

	if (s->current) {
		r = e_sexp_result_new (f, ESEXP_RES_BOOL);
		r->value.boolean = FALSE;
	} else {
		r = e_sexp_result_new (f, ESEXP_RES_ARRAY_PTR);
		r->value.ptrarray = g_ptr_array_new ();
	}

	if (argc == 1 && *value == '\0') {
		/* optimise the match "" case - match everything */

		if (s->current)
			r->value.boolean = TRUE;
		else {
			for (i = 0; i < s->summary->len; i++) {
				g_ptr_array_add (r->value.ptrarray, s->summary->pdata[i]);
			}
		}
		return r;
	}

	/* FIXME: what if we have multiple string args? */
	if (!camel_exchange_utils_search (
		CAMEL_SERVICE (parent_store),
		full_name, value, &found_uids, NULL))
		return r;

	if (!found_uids->len) {
		g_ptr_array_free (found_uids, TRUE);
		return r;
	}

	if (s->current) {
		uid = camel_message_info_uid (s->current);
		for (i = 0; i < found_uids->len; i++) {
			if (!strcmp (uid, found_uids->pdata[i]))
				r->value.boolean = TRUE;
			g_free (found_uids->pdata[i]);
		}
		g_ptr_array_free (found_uids, TRUE);
		return r;
	}

	/* if we need to setup a hash of summary items, this way we get
	   access to the summary memory which is locked for the duration of
	   the search, and wont vanish on us */
	if (uid_hash == NULL) {
		gint i;

		uid_hash = g_hash_table_new (g_str_hash, g_str_equal);
		for (i = 0; i < s->summary->len; i++) {
			info = camel_folder_summary_uid (s->folder->summary, s->summary->pdata[i]);
			g_hash_table_insert (uid_hash, s->summary->pdata[i], info);
		}
	}

	for (i = 0; i < found_uids->len; i++) {
		if (g_hash_table_lookup_extended (uid_hash, found_uids->pdata[i], (gpointer)&real_uid, (gpointer)&info))
			g_ptr_array_add (r->value.ptrarray, real_uid);
		g_free (found_uids->pdata[i]);
	}
	g_ptr_array_free (found_uids, TRUE);

	/* we could probably cache this globally, but its probably not worth it */
	if (uid_hash)
		g_hash_table_destroy (uid_hash);

	return r;
}

static void
camel_exchange_search_class_init (CamelExchangeSearchClass *class)
{
	CamelFolderSearchClass *folder_search_class;

	folder_search_class = CAMEL_FOLDER_SEARCH_CLASS (class);
	folder_search_class->body_contains = exchange_search_body_contains;
}

static void
camel_exchange_search_init (CamelExchangeSearch *exchange_search)
{
}

/**
 * camel_exchange_search_new:
 *
 * Creates a #CamelExchangeSearch object
 *
 * Return value: A new #CamelExchangeSearch
 **/
CamelFolderSearch *
camel_exchange_search_new (void)
{
	CamelFolderSearch *folder_search;

	folder_search = g_object_new (CAMEL_TYPE_EXCHANGE_SEARCH, NULL);
	camel_folder_search_construct (folder_search);

	return folder_search;
}
