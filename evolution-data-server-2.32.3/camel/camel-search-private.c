/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *	     Michael Zucchi <NotZed@Ximian.com>
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

/* POSIX requires <sys/types.h> be included before <regex.h> */
#include <sys/types.h>

#include <ctype.h>
#include <regex.h>
#include <stdio.h>
#include <string.h>

#include <glib/gi18n-lib.h>

#include "camel-mime-message.h"
#include "camel-multipart.h"
#include "camel-search-private.h"
#include "camel-stream-mem.h"

#define d(x)

/* builds the regex into pattern */
/* taken from camel-folder-search, with added isregex & exception parameter */
/* Basically, we build a new regex, either based on subset regex's, or substrings,
   that can be executed once over the whoel body, to match anything suitable.
   This is more efficient than multiple searches, and probably most (naive) strstr
   implementations, over long content.

   A small issue is that case-insenstivity wont work entirely correct for utf8 strings. */
gint
camel_search_build_match_regex (regex_t *pattern,
                                camel_search_flags_t type,
                                gint argc,
                                struct _ESExpResult **argv,
                                GError **error)
{
	GString *match = g_string_new("");
	gint c, i, count=0, err;
	gchar *word;
	gint flags;

	/* build a regex pattern we can use to match the words, we OR them together */
	if (argc>1)
		g_string_append_c (match, '(');
	for (i = 0; i < argc; i++) {
		if (argv[i]->type == ESEXP_RES_STRING) {
			if (count > 0)
				g_string_append_c (match, '|');

			word = argv[i]->value.string;
			if (type & CAMEL_SEARCH_MATCH_REGEX) {
				/* no need to escape because this should already be a valid regex */
				g_string_append (match, word);
			} else {
				/* escape any special chars (not sure if this list is complete) */
				if (type & CAMEL_SEARCH_MATCH_START)
					g_string_append_c (match, '^');
				while ((c = *word++)) {
					if (strchr ("*\\.()[]^$+", c) != NULL) {
						g_string_append_c (match, '\\');
					}
					g_string_append_c (match, c);
				}
				if (type & CAMEL_SEARCH_MATCH_END)
					g_string_append_c (match, '^');
			}
			count++;
		} else {
			g_warning("Invalid type passed to body-contains match function");
		}
	}
	if (argc > 1)
		g_string_append_c (match, ')');
	flags = REG_EXTENDED|REG_NOSUB;
	if (type & CAMEL_SEARCH_MATCH_ICASE)
		flags |= REG_ICASE;
	if (type & CAMEL_SEARCH_MATCH_NEWLINE)
		flags |= REG_NEWLINE;
	err = regcomp (pattern, match->str, flags);
	if (err != 0) {
		/* regerror gets called twice to get the full error string
		   length to do proper posix error reporting */
		gint len = regerror (err, pattern, NULL, 0);
		gchar *buffer = g_malloc0 (len + 1);

		regerror (err, pattern, buffer, len);
		g_set_error (
			error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			_("Regular expression compilation failed: %s: %s"),
			match->str, buffer);

		regfree (pattern);
	}
	d(printf("Built regex: '%s'\n", match->str));
	g_string_free (match, TRUE);

	return err;
}

static guchar soundex_table[256] = {
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0, 49, 50, 51,  0, 49, 50,  0,  0, 50, 50, 52, 53, 53,  0,
	 49, 50, 54, 50, 51,  0, 49,  0, 50,  0, 50,  0,  0,  0,  0,  0,
	  0,  0, 49, 50, 51,  0, 49, 50,  0,  0, 50, 50, 52, 53, 53,  0,
	 49, 50, 54, 50, 51,  0, 49,  0, 50,  0, 50,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};

static void
soundexify (const gchar *sound, gchar code[5])
{
	guchar *c, last = '\0';
	gint n;

	for (c = (guchar *) sound; *c && !isalpha (*c); c++);
	code[0] = toupper (*c);
	memset (code + 1, '0', 3);
	for (n = 1; *c && n < 5; c++) {
		guchar ch = soundex_table[*c];

		if (ch && ch != last) {
			code[n++] = ch;
			last = ch;
		}
	}
	code[4] = '\0';
}

static gboolean
header_soundex (const gchar *header, const gchar *match)
{
	gchar mcode[5], hcode[5];
	const gchar *p;
	gchar c;
	GString *word;
	gint truth = FALSE;

	soundexify (match, mcode);

	/* split the header into words, and soundexify and compare each one */
	/* FIXME: Should this convert to utf8, and split based on that, and what not?
	   soundex only makes sense for us-ascii though ... */

	word = g_string_new("");
	p = header;
	do {
		c = *p++;
		if (c == 0 || isspace (c)) {
			if (word->len > 0) {
				soundexify (word->str, hcode);
				if (strcmp (hcode, mcode) == 0)
					truth = TRUE;
			}
			g_string_truncate (word, 0);
		} else if (isalpha (c))
			g_string_append_c (word, c);
	} while (c && !truth);
	g_string_free (word, TRUE);

	return truth;
}

const gchar *
camel_ustrstrcase (const gchar *haystack, const gchar *needle)
{
	gunichar *nuni, *puni;
	gunichar u;
	const guchar *p;

	g_return_val_if_fail (haystack != NULL, NULL);
	g_return_val_if_fail (needle != NULL, NULL);

	if (strlen (needle) == 0)
		return haystack;
	if (strlen (haystack) == 0)
		return NULL;

	puni = nuni = g_alloca (sizeof (gunichar) * strlen (needle));

	p = (const guchar *) needle;
	while ((u = camel_utf8_getc(&p)))
		*puni++ = g_unichar_tolower (u);

	/* NULL means there was illegal utf-8 sequence */
	if (!p)
		return NULL;

	p = (const guchar *)haystack;
	while ((u = camel_utf8_getc(&p))) {
		gunichar c;

		c = g_unichar_tolower (u);
		/* We have valid stripped gchar */
		if (c == nuni[0]) {
			const guchar *q = p;
			gint npos = 1;

			while (nuni + npos < puni) {
				u = camel_utf8_getc(&q);
				if (!q || !u)
					return NULL;

				c = g_unichar_tolower (u);
				if (c != nuni[npos])
					break;

				npos++;
			}

			if (nuni + npos == puni)
				return (const gchar *) p;
		}
	}

	return NULL;
}

#define CAMEL_SEARCH_COMPARE(x, y, z) G_STMT_START {   \
	if ((x) == (z)) {                              \
		if ((y) == (z))                        \
			return 0;                      \
		else                                   \
			return -1;                     \
	} else if ((y) == (z))                         \
		return 1;                              \
} G_STMT_END

static gint
camel_ustrcasecmp (const gchar *ps1, const gchar *ps2)
{
	gunichar u1, u2 = 0;
	const guchar *s1 = (const guchar *)ps1;
	const guchar *s2 = (const guchar *)ps2;

	CAMEL_SEARCH_COMPARE (s1, s2, NULL);

	u1 = camel_utf8_getc(&s1);
	u2 = camel_utf8_getc(&s2);
	while (u1 && u2) {
		u1 = g_unichar_tolower (u1);
		u2 = g_unichar_tolower (u2);
		if (u1 < u2)
			return -1;
		else if (u1 > u2)
			return 1;

		u1 = camel_utf8_getc(&s1);
		u2 = camel_utf8_getc(&s2);
	}

	/* end of one of the strings ? */
	CAMEL_SEARCH_COMPARE (u1, u2, 0);

	/* if we have invalid utf8 sequence ?  */
	CAMEL_SEARCH_COMPARE (s1, s2, NULL);

	return 0;
}

static gint
camel_ustrncasecmp (const gchar *ps1, const gchar *ps2, gsize len)
{
	gunichar u1, u2 = 0;
	const guchar *s1 = (const guchar *)ps1;
	const guchar *s2 = (const guchar *)ps2;

	CAMEL_SEARCH_COMPARE (s1, s2, NULL);

	u1 = camel_utf8_getc(&s1);
	u2 = camel_utf8_getc(&s2);
	while (len > 0 && u1 && u2) {
		u1 = g_unichar_tolower (u1);
		u2 = g_unichar_tolower (u2);
		if (u1 < u2)
			return -1;
		else if (u1 > u2)
			return 1;

		len--;
		u1 = camel_utf8_getc(&s1);
		u2 = camel_utf8_getc(&s2);
	}

	if (len == 0)
		return 0;

	/* end of one of the strings ? */
	CAMEL_SEARCH_COMPARE (u1, u2, 0);

	/* if we have invalid utf8 sequence ?  */
	CAMEL_SEARCH_COMPARE (s1, s2, NULL);

	return 0;
}

/* value is the match value suitable for exact match if required */
static gint
header_match(const gchar *value, const gchar *match, camel_search_match_t how)
{
	const guchar *p;
	gint vlen, mlen;
	gunichar c;

	if (how == CAMEL_SEARCH_MATCH_SOUNDEX)
		return header_soundex (value, match);

	vlen = strlen(value);
	mlen = strlen(match);
	if (vlen < mlen)
		return FALSE;

	/* from dan the man, if we have mixed case, perform a case-sensitive match,
	   otherwise not */
	p = (const guchar *)match;
	while ((c = camel_utf8_getc (&p))) {
		if (g_unichar_isupper(c)) {
			switch (how) {
			case CAMEL_SEARCH_MATCH_EXACT:
				return strcmp(value, match) == 0;
			case CAMEL_SEARCH_MATCH_CONTAINS:
				return strstr(value, match) != NULL;
			case CAMEL_SEARCH_MATCH_STARTS:
				return strncmp(value, match, mlen) == 0;
			case CAMEL_SEARCH_MATCH_ENDS:
				return strcmp(value + vlen - mlen, match) == 0;
			default:
				break;
			}
			return FALSE;
		}
	}

	switch (how) {
	case CAMEL_SEARCH_MATCH_EXACT:
		return camel_ustrcasecmp(value, match) == 0;
	case CAMEL_SEARCH_MATCH_CONTAINS:
		return camel_ustrstrcase(value, match) != NULL;
	case CAMEL_SEARCH_MATCH_STARTS:
		return camel_ustrncasecmp(value, match, mlen) == 0;
	case CAMEL_SEARCH_MATCH_ENDS:
		return camel_ustrcasecmp(value + vlen - mlen, match) == 0;
	default:
		break;
	}

	return FALSE;
}

/* searhces for match inside value, if match is mixed case, hten use case-sensitive,
   else insensitive */
gboolean
camel_search_header_match (const gchar *value, const gchar *match, camel_search_match_t how, camel_search_t type, const gchar *default_charset)
{
	const gchar *name, *addr;
	const guchar *ptr;
	gint truth = FALSE, i;
	CamelInternetAddress *cia;
	gchar *v, *vdom, *mdom;
	gunichar c;

	ptr = (const guchar *)value;
	while ((c = camel_utf8_getc(&ptr)) && g_unichar_isspace(c))
		value = (const gchar *)ptr;

	switch (type) {
	case CAMEL_SEARCH_TYPE_ENCODED:
		v = camel_header_decode_string(value, default_charset); /* FIXME: Find header charset */
		truth = header_match(v, match, how);
		g_free(v);
		break;
	case CAMEL_SEARCH_TYPE_MLIST:
		/* Special mailing list old-version domain hack
		   If one of the mailing list names doesn't have an @ in it, its old-style, so
		   only match against the pre-domain part, which should be common */

		vdom = strchr(value, '@');
		mdom = strchr(match, '@');
		if (mdom == NULL && vdom != NULL) {
			v = g_alloca(vdom-value+1);
			memcpy(v, value, vdom-value);
			v[vdom-value] = 0;
			value = (gchar *)v;
		} else if (mdom != NULL && vdom == NULL) {
			v = g_alloca(mdom-match+1);
			memcpy(v, match, mdom-match);
			v[mdom-match] = 0;
			match = (gchar *)v;
		}
		/* Falls through */
	case CAMEL_SEARCH_TYPE_ASIS:
		truth = header_match(value, match, how);
		break;
	case CAMEL_SEARCH_TYPE_ADDRESS_ENCODED:
	case CAMEL_SEARCH_TYPE_ADDRESS:
		/* possible simple case to save some work if we can */
		if (header_match(value, match, how))
			return TRUE;

		/* Now we decode any addresses, and try asis matches on name and address parts */
		cia = camel_internet_address_new();
		if (type == CAMEL_SEARCH_TYPE_ADDRESS_ENCODED)
			camel_address_decode((CamelAddress *)cia, value);
		else
			camel_address_unformat((CamelAddress *)cia, value);

		for (i=0; !truth && camel_internet_address_get(cia, i, &name, &addr);i++)
			truth = (name && header_match(name, match, how)) || (addr && header_match(addr, match, how));

		g_object_unref (cia);
		break;
	}

	return truth;
}

/* performs a 'slow' content-based match */
/* there is also an identical copy of this in camel-filter-search.c */
gboolean
camel_search_message_body_contains (CamelDataWrapper *object, regex_t *pattern)
{
	CamelDataWrapper *containee;
	gint truth = FALSE;
	gint parts, i;

	containee = camel_medium_get_content (CAMEL_MEDIUM (object));

	if (containee == NULL)
		return FALSE;

	/* using the object types is more accurate than using the mime/types */
	if (CAMEL_IS_MULTIPART (containee)) {
		parts = camel_multipart_get_number (CAMEL_MULTIPART (containee));
		for (i = 0; i < parts && truth == FALSE; i++) {
			CamelDataWrapper *part = (CamelDataWrapper *)camel_multipart_get_part (CAMEL_MULTIPART (containee), i);
			if (part)
				truth = camel_search_message_body_contains (part, pattern);
		}
	} else if (CAMEL_IS_MIME_MESSAGE (containee)) {
		/* for messages we only look at its contents */
		truth = camel_search_message_body_contains ((CamelDataWrapper *)containee, pattern);
	} else if (camel_content_type_is(CAMEL_DATA_WRAPPER (containee)->mime_type, "text", "*")
		|| camel_content_type_is(CAMEL_DATA_WRAPPER (containee)->mime_type, "x-evolution", "evolution-rss-feed")) {
		/* for all other text parts, we look inside, otherwise we dont care */
		CamelStream *stream;
		GByteArray *byte_array;

		byte_array = g_byte_array_new ();
		stream = camel_stream_mem_new_with_byte_array (byte_array);
		camel_data_wrapper_write_to_stream (containee, stream, NULL);
		camel_stream_write (stream, "", 1, NULL);
		truth = regexec (pattern, (gchar *) byte_array->data, 0, NULL, 0) == 0;
		g_object_unref (stream);
	}

	return truth;
}

static void
output_c(GString *w, guint32 c, gint *type)
{
	gint utf8len;
	gchar utf8[8];

	if (!g_unichar_isalnum(c))
		*type = CAMEL_SEARCH_WORD_COMPLEX | (*type & CAMEL_SEARCH_WORD_8BIT);
	else
		c = g_unichar_tolower(c);

	if (c > 0x80)
		*type |= CAMEL_SEARCH_WORD_8BIT;

	/* FIXME: use camel_utf8_putc */
	utf8len = g_unichar_to_utf8(c, utf8);
	utf8[utf8len] = 0;
	g_string_append(w, utf8);
}

static void
output_w(GString *w, GPtrArray *list, gint type)
{
	struct _camel_search_word *word;

	if (w->len) {
		word = g_malloc0(sizeof(*word));
		word->word = g_strdup(w->str);
		word->type = type;
		g_ptr_array_add(list, word);
		g_string_truncate(w, 0);
	}
}

struct _camel_search_words *
camel_search_words_split(const guchar *in)
{
	gint type = CAMEL_SEARCH_WORD_SIMPLE, all = 0;
	GString *w;
	struct _camel_search_words *words;
	GPtrArray *list = g_ptr_array_new();
	guint32 c;
	gint inquote = 0;

	words = g_malloc0(sizeof(*words));
	w = g_string_new("");

	do {
		c = camel_utf8_getc(&in);

		if (c == 0
		    || (inquote && c == '"')
		    || (!inquote && g_unichar_isspace(c))) {
			output_w(w, list, type);
			all |= type;
			type = CAMEL_SEARCH_WORD_SIMPLE;
			inquote = 0;
		} else {
			if (c == '\\') {
				c = camel_utf8_getc(&in);
				if (c)
					output_c(w, c, &type);
				else {
					output_w(w, list, type);
					all |= type;
				}
			} else if (c == '\"') {
				inquote = 1;
			} else {
				output_c(w, c, &type);
			}
		}
	} while (c);

	g_string_free(w, TRUE);
	words->len = list->len;
	words->words = (struct _camel_search_word **)list->pdata;
	words->type = all;
	g_ptr_array_free(list, FALSE);

	return words;
}

/* takes an existing 'words' list, and converts it to another consisting of
   only simple words, with any punctuation etc stripped */
struct _camel_search_words *
camel_search_words_simple(struct _camel_search_words *wordin)
{
	gint i;
	const guchar *ptr, *start, *last;
	gint type = CAMEL_SEARCH_WORD_SIMPLE, all = 0;
	GPtrArray *list = g_ptr_array_new();
	struct _camel_search_word *word;
	struct _camel_search_words *words;
	guint32 c;

	words = g_malloc0(sizeof(*words));

	for (i=0;i<wordin->len;i++) {
		if ((wordin->words[i]->type & CAMEL_SEARCH_WORD_COMPLEX) == 0) {
			word = g_malloc0(sizeof(*word));
			word->type = wordin->words[i]->type;
			word->word = g_strdup(wordin->words[i]->word);
			g_ptr_array_add(list, word);
		} else {
			ptr = (const guchar *) wordin->words[i]->word;
			start = last = ptr;
			do {
				c = camel_utf8_getc(&ptr);
				if (c == 0 || !g_unichar_isalnum(c)) {
					if (last > start) {
						word = g_malloc0(sizeof(*word));
						word->word = g_strndup((gchar *) start, last-start);
						word->type = type;
						g_ptr_array_add(list, word);
						all |= type;
						type = CAMEL_SEARCH_WORD_SIMPLE;
					}
					start = ptr;
				}
				if (c > 0x80)
					type = CAMEL_SEARCH_WORD_8BIT;
				last = ptr;
			} while (c);
		}
	}

	words->len = list->len;
	words->words = (struct _camel_search_word **)list->pdata;
	words->type = all;
	g_ptr_array_free(list, FALSE);

	return words;
}

void
camel_search_words_free(struct _camel_search_words *words)
{
	gint i;

	for (i=0;i<words->len;i++) {
		struct _camel_search_word *word = words->words[i];

		g_free(word->word);
		g_free(word);
	}
	g_free(words->words);
	g_free(words);
}

