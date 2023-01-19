/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-imap-search.c: IMAP folder search */

/*
 *  Authors:
 *    Dan Winship <danw@ximian.com>
 *    Michael Zucchi <notzed@ximian.com>
 *
 *  Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "camel-search-private.h"

#include "camel-imap-command.h"
#include "camel-imap-folder.h"
#include "camel-imap-search.h"
#include "camel-imap-store.h"
#include "camel-imap-summary.h"
#include "camel-imap-utils.h"

#define d(x)

#ifdef G_OS_WIN32
/* The strtok() in Microsoft's C library is MT-safe (but still uses
 * only one buffer pointer per thread, but for the use of strtok_r()
 * here that's enough).
 */
#define strtok_r(s,sep,lasts) (*(lasts)=strtok((s),(sep)))
#endif

/*
  File is:
   BODY	 (as in body search)
   Last uid when search performed
   termcount: number of search terms
   matchcount: number of matches
   term0, term1 ...
   match0, match1, match2, ...
*/

/* size of in-memory cache */
#define MATCH_CACHE_SIZE (32)

/* Also takes care of 'endianness' file magic */
#define MATCH_MARK (('B' << 24) | ('O' << 16) | ('D' << 8) | 'Y')

/* on-disk header, in native endianness format, matches follow */
struct _match_header {
	guint32 mark;
	guint32 validity;	/* uidvalidity for this folder */
	guint32 lastuid;
	guint32 termcount;
	guint32 matchcount;
};

/* in-memory record */
struct _match_record {
	struct _match_record *next;
	struct _match_record *prev;

	gchar hash[17];

	guint32 lastuid;
	guint32 validity;

	guint termcount;
	gchar **terms;
	GArray *matches;
};

static ESExpResult *imap_body_contains (struct _ESExp *f, gint argc, struct _ESExpResult **argv, CamelFolderSearch *s);

G_DEFINE_TYPE (CamelImapSearch, camel_imap_search, CAMEL_TYPE_FOLDER_SEARCH)

static void
free_match(CamelImapSearch *is, struct _match_record *mr)
{
	gint i;

	for (i=0;i<mr->termcount;i++)
		g_free(mr->terms[i]);
	g_free(mr->terms);
	g_array_free(mr->matches, TRUE);
	g_free(mr);
}

static void
imap_search_dispose (GObject *object)
{
	CamelImapSearch *search;

	search = CAMEL_IMAP_SEARCH (object);

	if (search->cache != NULL) {
		g_object_unref (search->cache);
		search->cache = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (camel_imap_search_parent_class)->dispose (object);
}

static void
imap_search_finalize (GObject *object)
{
	CamelImapSearch *search;
	struct _match_record *mr;

	search = CAMEL_IMAP_SEARCH (object);

	while ((mr = (struct _match_record *)camel_dlist_remtail(&search->matches)))
		free_match (search, mr);

	g_hash_table_destroy (search->matches_hash);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (camel_imap_search_parent_class)->finalize (object);
}

static void
camel_imap_search_class_init (CamelImapSearchClass *class)
{
	GObjectClass *object_class;
	CamelFolderSearchClass *folder_search_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = imap_search_dispose;
	object_class->finalize = imap_search_finalize;

	folder_search_class = CAMEL_FOLDER_SEARCH_CLASS (class);
	folder_search_class->body_contains = imap_body_contains;
}

static void
camel_imap_search_init (CamelImapSearch *is)
{
	camel_dlist_init(&is->matches);
	is->matches_hash = g_hash_table_new(g_str_hash, g_str_equal);
	is->matches_count = 0;
	is->lastuid = 0;
}

/**
 * camel_imap_search_new:
 *
 * Returns: A new CamelImapSearch widget.
 **/
CamelFolderSearch *
camel_imap_search_new (const gchar *cachedir)
{
	CamelFolderSearch *new = g_object_new (CAMEL_TYPE_IMAP_SEARCH, NULL);
	CamelImapSearch *is = (CamelImapSearch *)new;

	camel_folder_search_construct (new);

	is->cache = camel_data_cache_new(cachedir, NULL);
	if (is->cache) {
		/* Expire entries after 14 days of inactivity */
		camel_data_cache_set_expire_access(is->cache, 60*60*24*14);
	}

	return new;
}

static void
hash_match(gchar hash[17], gint argc, struct _ESExpResult **argv)
{
	GChecksum *checksum;
	guint8 *digest;
	gsize length;
	gint state = 0, save = 0;
	gint i;

	length = g_checksum_type_get_length (G_CHECKSUM_MD5);
	digest = g_alloca (length);

	checksum = g_checksum_new (G_CHECKSUM_MD5);
	for (i=0;i<argc;i++) {
		if (argv[i]->type == ESEXP_RES_STRING)
			g_checksum_update (
				checksum, (guchar *) argv[i]->value.string, -1);
	}
	g_checksum_get_digest (checksum, digest, &length);
	g_checksum_free (checksum);

	g_base64_encode_step ((guchar *) digest, 12, FALSE, hash, &state, &save);
	g_base64_encode_close (FALSE, hash, &state, &save);

	for (i=0;i<16;i++) {
		if (hash[i] == '+')
			hash[i] = ',';
		if (hash[i] == '/')
			hash[i] = '_';
	}

	hash[16] = 0;
}

static gint
save_match(CamelImapSearch *is, struct _match_record *mr)
{
	guint32 mark = MATCH_MARK;
	gint ret = 0;
	struct _match_header header;
	CamelStream *stream;

	/* since its a cache, doesn't matter if it doesn't save, at least we have the in-memory cache
	   for this session */
	if (is->cache == NULL)
		return -1;

	stream = camel_data_cache_add(is->cache, "search/body-contains", mr->hash, NULL);
	if (stream == NULL)
		return -1;

	d(printf("Saving search cache entry to '%s': %s\n", mr->hash, mr->terms[0]));

	/* we write the whole thing, then re-write the header magic, saves fancy sync code */
	memcpy(&header.mark, "    ", 4);
	header.termcount = 0;
	header.matchcount = mr->matches->len;
	header.lastuid = mr->lastuid;
	header.validity = mr->validity;

	if (camel_stream_write(stream, (gchar *)&header, sizeof(header), NULL) != sizeof(header)
	    || camel_stream_write(stream, mr->matches->data, mr->matches->len*sizeof(guint32), NULL) != mr->matches->len*sizeof(guint32)
	    || camel_seekable_stream_seek((CamelSeekableStream *)stream, 0, CAMEL_STREAM_SET, NULL) == -1
	    || camel_stream_write(stream, (gchar *)&mark, sizeof(mark), NULL) != sizeof(mark)) {
		d(printf(" saving failed, removing cache entry\n"));
		camel_data_cache_remove(is->cache, "search/body-contains", mr->hash, NULL);
		ret = -1;
	}

	g_object_unref (stream);
	return ret;
}

static struct _match_record *
load_match(CamelImapSearch *is, gchar hash[17], gint argc, struct _ESExpResult **argv)
{
	struct _match_record *mr;
	CamelStream *stream = NULL;
	struct _match_header header;
	gint i;

	mr = g_malloc0(sizeof(*mr));
	mr->matches = g_array_new(0, 0, sizeof(guint32));
	g_assert(strlen(hash) == 16);
	strcpy(mr->hash, hash);
	mr->terms = g_malloc0(sizeof(mr->terms[0]) * argc);
	for (i=0;i<argc;i++) {
		if (argv[i]->type == ESEXP_RES_STRING) {
			mr->termcount++;
			mr->terms[i] = g_strdup(argv[i]->value.string);
		}
	}

	d(printf("Loading search cache entry to '%s': %s\n", mr->hash, mr->terms[0]));

	memset(&header, 0, sizeof(header));
	if (is->cache)
		stream = camel_data_cache_get(is->cache, "search/body-contains", mr->hash, NULL);
	if (stream != NULL) {
		/* 'cause i'm gonna be lazy, i'm going to have the termcount == 0 for now,
		   and not load or save them since i can't think of a nice way to do it, the hash
		   should be sufficient to key it */
		/* This check should also handle endianness changes, we just throw away
		   the data (its only a cache) */
		if (camel_stream_read(stream, (gchar *)&header, sizeof(header), NULL) == sizeof(header)
		    && header.validity == is->validity
		    && header.mark == MATCH_MARK
		    && header.termcount == 0) {
			d(printf(" found %d matches\n", header.matchcount));
			g_array_set_size(mr->matches, header.matchcount);
			camel_stream_read(stream, mr->matches->data, sizeof(guint32)*header.matchcount, NULL);
		} else {
			d(printf(" file format invalid/validity changed\n"));
			memset(&header, 0, sizeof(header));
		}
		g_object_unref (stream);
	} else {
		d(printf(" no cache entry found\n"));
	}

	mr->validity = header.validity;
	if (mr->validity != is->validity)
		mr->lastuid = 0;
	else
		mr->lastuid = header.lastuid;

	return mr;
}

static gint
sync_match(CamelImapSearch *is, struct _match_record *mr)
{
	gchar *p, *result, *lasts = NULL;
	CamelImapResponse *response = NULL;
	guint32 uid;
	CamelFolder *folder = ((CamelFolderSearch *)is)->folder;
	CamelStore *parent_store;
	CamelImapStore *store;
	struct _camel_search_words *words;
	GString *search;
	gint i;

	parent_store = camel_folder_get_parent_store (folder);
	store = CAMEL_IMAP_STORE (parent_store);

	if (mr->lastuid >= is->lastuid && mr->validity == is->validity)
		return 0;

	d(printf ("updating match record for uid's %d:%d\n", mr->lastuid+1, is->lastuid));

	/* TODO: Handle multiple search terms */

	/* This handles multiple search words within a single term */
	words = camel_search_words_split ((const guchar *) mr->terms[0]);
	search = g_string_new ("");
	g_string_append_printf (search, "UID %d:%d", mr->lastuid + 1, is->lastuid);
	for (i = 0; i < words->len; i++) {
		gchar *w = words->words[i]->word, c;

		g_string_append_printf (search, " BODY \"");
		while ((c = *w++)) {
			if (c == '\\' || c == '"')
				g_string_append_c (search, '\\');
			g_string_append_c (search, c);
		}
		g_string_append_c (search, '"');
	}
	camel_search_words_free (words);

	/* We only try search using utf8 if its non us-ascii text? */
	if ((words->type & CAMEL_SEARCH_WORD_8BIT) &&  (store->capabilities & IMAP_CAPABILITY_utf8_search)) {
		response = camel_imap_command (store, folder, NULL,
					       "UID SEARCH CHARSET UTF-8 %s", search->str);
		/* We can't actually tell if we got a NO response, so assume always */
		if (response == NULL)
			store->capabilities &= ~IMAP_CAPABILITY_utf8_search;
	}
	if (response == NULL)
		response = camel_imap_command (store, folder, NULL,
					       "UID SEARCH %s", search->str);
	g_string_free(search, TRUE);

	if (!response)
		return -1;
	result = camel_imap_response_extract (store, response, "SEARCH", NULL);
	if (!result)
		return -1;

	p = result + sizeof ("* SEARCH");
	for (p = strtok_r (p, " ", &lasts); p; p = strtok_r (NULL, " ", &lasts)) {
		uid = strtoul(p, NULL, 10);
		g_array_append_vals(mr->matches, &uid, 1);
	}
	g_free(result);

	mr->validity = is->validity;
	mr->lastuid = is->lastuid;
	save_match(is, mr);

	return 0;
}

static struct _match_record *
get_match(CamelImapSearch *is, gint argc, struct _ESExpResult **argv)
{
	gchar hash[17];
	struct _match_record *mr;

	hash_match(hash, argc, argv);

	mr = g_hash_table_lookup(is->matches_hash, hash);
	if (mr == NULL) {
		while (is->matches_count >= MATCH_CACHE_SIZE) {
			mr = (struct _match_record *)camel_dlist_remtail(&is->matches);
			if (mr) {
				printf("expiring match '%s' (%s)\n", mr->hash, mr->terms[0]);
				g_hash_table_remove(is->matches_hash, mr->hash);
				free_match(is, mr);
				is->matches_count--;
			} else {
				is->matches_count = 0;
			}
		}
		mr = load_match(is, hash, argc, argv);
		g_hash_table_insert(is->matches_hash, mr->hash, mr);
		is->matches_count++;
	} else {
		camel_dlist_remove((CamelDListNode *)mr);
	}

	camel_dlist_addhead(&is->matches, (CamelDListNode *)mr);

	/* what about offline mode? */
	/* We could cache those results too, or should we cache them elsewhere? */
	sync_match(is, mr);

	return mr;
}

static ESExpResult *
imap_body_contains (struct _ESExp *f, gint argc, struct _ESExpResult **argv, CamelFolderSearch *s)
{
	CamelStore *parent_store;
	CamelImapStore *store;
	CamelImapSearch *is = (CamelImapSearch *)s;
	gchar *uid;
	ESExpResult *r;
	GHashTable *uid_hash = NULL;
	GPtrArray *array;
	gint i, j;
	struct _match_record *mr;
	guint32 uidn, *uidp;

	parent_store = camel_folder_get_parent_store (s->folder);
	store = CAMEL_IMAP_STORE (parent_store);

	d(printf("Performing body search '%s'\n", argv[0]->value.string));

	/* TODO: Cache offline searches too? */

	/* If offline, search using the parent class, which can handle this manually */
	if (CAMEL_OFFLINE_STORE (store)->state == CAMEL_OFFLINE_STORE_NETWORK_UNAVAIL)
		return CAMEL_FOLDER_SEARCH_CLASS (camel_imap_search_parent_class)->body_contains(f, argc, argv, s);

	/* optimise the match "" case - match everything */
	if (argc == 1 && argv[0]->value.string[0] == '\0') {
		if (s->current) {
			r = e_sexp_result_new(f, ESEXP_RES_BOOL);
			r->value.boolean = TRUE;
		} else {
			r = e_sexp_result_new(f, ESEXP_RES_ARRAY_PTR);
			r->value.ptrarray = g_ptr_array_new ();
			for (i = 0; i < s->summary->len; i++) {
				g_ptr_array_add(r->value.ptrarray, (gchar *)g_ptr_array_index(s->summary, i));
			}
		}
	} else if (argc == 0 || s->summary->len == 0) {
		/* nothing to match case, do nothing (should be handled higher up?) */
		if (s->current) {
			r = e_sexp_result_new(f, ESEXP_RES_BOOL);
			r->value.boolean = FALSE;
		} else {
			r = e_sexp_result_new(f, ESEXP_RES_ARRAY_PTR);
			r->value.ptrarray = g_ptr_array_new ();
		}
	} else {
		gint truth = FALSE;

		/* setup lastuid/validity for synchronising */
		is->lastuid = strtoul((gchar *)g_ptr_array_index(s->summary, s->summary->len-1), NULL, 10);
		is->validity = ((CamelImapSummary *)(s->folder->summary))->validity;

		mr = get_match(is, argc, argv);

		if (s->current) {
			uidn = strtoul(camel_message_info_uid(s->current), NULL, 10);
			uidp = (guint32 *)mr->matches->data;
			j = mr->matches->len;
			for (i=0;i<j && !truth;i++)
				truth = *uidp++ == uidn;
			r = e_sexp_result_new(f, ESEXP_RES_BOOL);
			r->value.boolean = truth;
		} else {
			r = e_sexp_result_new(f, ESEXP_RES_ARRAY_PTR);
			array = r->value.ptrarray = g_ptr_array_new();

			/* We use a hash to map the uid numbers to uid strings as required by the search api */
			/* We use the summary's strings so we dont need to alloc more */
			uid_hash = g_hash_table_new(NULL, NULL);
			for (i = 0; i < s->summary->len; i++) {
				uid = (gchar *)s->summary->pdata[i];
				uidn = strtoul(uid, NULL, 10);
				g_hash_table_insert(uid_hash, GUINT_TO_POINTER(uidn), uid);
			}

			uidp = (guint32 *)mr->matches->data;
			j = mr->matches->len;
			for (i=0;i<j && !truth;i++) {
				uid = g_hash_table_lookup(uid_hash, GUINT_TO_POINTER(*uidp++));
				if (uid)
					g_ptr_array_add(array, uid);
			}

			g_hash_table_destroy(uid_hash);
		}
	}

	return r;
}
