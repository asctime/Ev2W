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
 *		Jon Trowbridge <trow@ximian.com>
 *      Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>
#include "eab-book-util.h"

#include <string.h>
#include <glib.h>
#include <glib-object.h>

/*
 *
 * Specialized Queries
 *
 */

static gchar *
escape (const gchar *str)
{
	GString *s = g_string_new (NULL);
	const gchar *p = str;

	while (*p) {
		if (*p == '\\')
			g_string_append_len (s, "\\\\", 2);
		else if (*p == '"')
			g_string_append_len (s, "\\\"", 2);
		else
			g_string_append_c (s, *p);

		p++;
	}

	return g_string_free (s, FALSE);
}

guint
eab_name_and_email_query (EBook                  *book,
			  const gchar            *name,
			  const gchar            *email,
			  EBookListAsyncCallback  cb,
			  gpointer                closure)
{
	gchar *email_query=NULL, *name_query=NULL;
	EBookQuery *query;
	guint tag;
	gchar *escaped_name, *escaped_email;

	g_return_val_if_fail (book && E_IS_BOOK (book), 0);
	g_return_val_if_fail (cb != NULL, 0);

	if (name && !*name)
		name = NULL;
	if (email && !*email)
		email = NULL;

	if (name == NULL && email == NULL)
		return 0;

	escaped_name = name ? escape (name) : NULL;
	escaped_email = email ? escape (email) : NULL;

	/* Build our e-mail query.
	 * We only query against the username part of the address, to avoid not matching
	 * fred@foo.com and fred@mail.foo.com.  While their may be namespace collisions
	 * in the usernames of everyone out there, it shouldn't be that bad.  (Famous last words.)
	 * But if name is missing we query against complete email id to avoid matching emails like
	 * users@foo.org with users@bar.org
	 */
	if (escaped_email) {
		const gchar *t = escaped_email;
		while (*t && *t != '@')
			++t;
		if (*t == '@' && escaped_name) {
			email_query = g_strdup_printf ("(beginswith \"email\" \"%.*s@\")", (gint)(t-escaped_email), escaped_email);

		} else {
			email_query = g_strdup_printf ("(beginswith \"email\" \"%s\")", escaped_email);
		}
	}

	/* Build our name query.*/

	if (escaped_name)
		name_query = g_strdup_printf ("(or (beginswith \"file_as\" \"%s\") (beginswith \"full_name\" \"%s\"))", escaped_name, escaped_name);

	/* Assemble our e-mail & name queries */
	if (email_query && name_query) {
		gchar *full_query = g_strdup_printf ("(and %s %s)", email_query, name_query);
		query = e_book_query_from_string (full_query);
		g_free (full_query);
	}
	else if (email_query) {
		query = e_book_query_from_string (email_query);
	}
	else if (name_query) {
		query = e_book_query_from_string (name_query);
	}
	else
		return 0;

	tag = e_book_get_contacts_async (book, query, cb, closure);

	g_free (email_query);
	g_free (name_query);
	g_free (escaped_email);
	g_free (escaped_name);
	e_book_query_unref (query);

	return tag;
}

/*
 * Simple nickname query
 */
guint
eab_nickname_query (EBook                  *book,
		    const gchar            *nickname,
		    EBookListAsyncCallback  cb,
		    gpointer                closure)
{
	EBookQuery *query;
	gchar *query_string;
	guint retval;

	g_return_val_if_fail (E_IS_BOOK (book), 0);
	g_return_val_if_fail (nickname != NULL, 0);

	/* The empty-string case shouldn't generate a warning. */
	if (!*nickname)
		return 0;

	query_string = g_strdup_printf ("(is \"nickname\" \"%s\")", nickname);

	query = e_book_query_from_string (query_string);

	retval = e_book_get_contacts_async (book, query, cb, closure);

	g_free (query_string);
	e_book_query_unref (query);

	return retval;
}

/* Copied from camel_strstrcase */
static gchar *
eab_strstrcase (const gchar *haystack, const gchar *needle)
{
	/* find the needle in the haystack neglecting case */
	const gchar *ptr;
	guint len;

	g_return_val_if_fail (haystack != NULL, NULL);
	g_return_val_if_fail (needle != NULL, NULL);

	len = strlen (needle);
	if (len > strlen (haystack))
		return NULL;

	if (len == 0)
		return (gchar *) haystack;

	for (ptr = haystack; *(ptr + len - 1) != '\0'; ptr++)
		if (!g_ascii_strncasecmp (ptr, needle, len))
			return (gchar *) ptr;

	return NULL;
}

GList*
eab_contact_list_from_string (const gchar *str)
{
	GList *contacts = NULL;
	GString *gstr = g_string_new (NULL);
	gchar *str_stripped;
	gchar *p = (gchar *)str;
	gchar *q;

	if (!p)
		return NULL;

	if (!strncmp (p, "Book: ", 6)) {
		p = strchr (p, '\n');
		if (!p) {
			g_warning (G_STRLOC ": Got book but no newline!");
			return NULL;
		}
		p++;
	}

	while (*p) {
		if (*p != '\r') g_string_append_c (gstr, *p);

		p++;
	}

	p = str_stripped = g_string_free (gstr, FALSE);

	/* Note: The vCard standard says
	 *
	 * vcard = "BEGIN" [ws] ":" [ws] "VCARD" [ws] 1*CRLF
	 *         items *CRLF "END" [ws] ":" [ws] "VCARD"
	 *
	 * which means we can have whitespace (e.g. "BEGIN : VCARD"). So we're not being
	 * fully compliant here, although I'm not sure it matters. The ideal solution
	 * would be to have a vcard parsing function that returned the end of the vcard
	 * parsed. Arguably, contact list parsing should all be in libebook's e-vcard.c,
	 * where we can do proper parsing and validation without code duplication. */

	for (p = eab_strstrcase (p, "BEGIN:VCARD"); p; p = eab_strstrcase (q, "\nBEGIN:VCARD")) {
		gchar *card_str;

		if (*p == '\n')
			p++;

		for (q = eab_strstrcase (p, "END:VCARD"); q; q = eab_strstrcase (q, "END:VCARD")) {
			gchar *temp;

			q += 9;
			temp = q;
			temp += strspn (temp, "\r\n\t ");

			if (*temp == '\0' || !g_ascii_strncasecmp (temp, "BEGIN:VCARD", 11))
				break;  /* Found the outer END:VCARD */
		}

		if (!q)
			break;

		card_str = g_strndup (p, q - p);
		contacts = g_list_append (contacts, e_contact_new_from_vcard (card_str));
		g_free (card_str);
	}

	g_free (str_stripped);

	return contacts;
}

gchar *
eab_contact_list_to_string (GList *contacts)
{
	GString *str = g_string_new ("");
	GList *l;

	for (l = contacts; l; l = l->next) {
		EContact *contact = l->data;
		gchar *vcard_str = e_vcard_to_string (E_VCARD (contact), EVC_FORMAT_VCARD_30);

		g_string_append (str, vcard_str);
		if (l->next)
			g_string_append (str, "\r\n\r\n");
	}

	return g_string_free (str, FALSE);
}

gboolean
eab_book_and_contact_list_from_string (const gchar *str, EBook **book, GList **contacts)
{
	const gchar *s0, *s1;
	gchar *uri;

	g_return_val_if_fail (str != NULL, FALSE);
	g_return_val_if_fail (book != NULL, FALSE);
	g_return_val_if_fail (contacts != NULL, FALSE);

	*contacts = eab_contact_list_from_string (str);

	if (!strncmp (str, "Book: ", 6)) {
		s0 = str + 6;
		s1 = strchr (str, '\r');

		if (!s1)
			s1 = strchr (str, '\n');
	} else {
		s0 = NULL;
		s1 = NULL;
	}

	if (!s0 || !s1) {
		*book = NULL;
		return FALSE;
	}

	uri = g_strndup (s0, s1 - s0);
	*book = e_book_new_from_uri (uri, NULL);
	g_free (uri);

	return *book ? TRUE : FALSE;
}

gchar *
eab_book_and_contact_list_to_string (EBook *book, GList *contacts)
{
	gchar *s0, *s1;

	s0 = eab_contact_list_to_string (contacts);
	if (!s0)
		s0 = g_strdup ("");

	if (book)
		s1 = g_strconcat ("Book: ", e_book_get_uri (book), "\r\n", s0, NULL);
	else
		s1 = g_strdup (s0);

	g_free (s0);
	return s1;
}

#ifdef notyet
/*
 *  Convenience routine to check for addresses in the local address book.
 */

typedef struct _HaveAddressInfo HaveAddressInfo;
struct _HaveAddressInfo {
	gchar *email;
	EBookHaveAddressCallback cb;
	gpointer closure;
};

static void
have_address_query_cb (EBook *book, EBookSimpleQueryStatus status, const GList *contacts, gpointer closure)
{
	HaveAddressInfo *info = (HaveAddressInfo *) closure;

	info->cb (book,
		  info->email,
		  contacts && (status == E_BOOK_ERROR_OK) ? E_CONTACT (contacts->data) : NULL,
		  info->closure);

	g_free (info->email);
	g_free (info);
}

static void
have_address_book_open_cb (EBook *book, gpointer closure)
{
	HaveAddressInfo *info = (HaveAddressInfo *) closure;

	if (book) {

		e_book_name_and_email_query (book, NULL, info->email, have_address_query_cb, info);

	} else {

		info->cb (NULL, info->email, NULL, info->closure);

		g_free (info->email);
		g_free (info);

	}
}

void
eab_query_address_default (const gchar *email,
			   EABHaveAddressCallback cb,
			   gpointer closure)
{
	HaveAddressInfo *info;

	g_return_if_fail (email != NULL);
	g_return_if_fail (cb != NULL);

	info = g_new0 (HaveAddressInfo, 1);
	info->email = g_strdup (email);
	info->cb = cb;
	info->closure = closure;

	e_book_use_default_book (have_address_book_open_cb, info);
}
#endif

/* bad place for this i know. */
gint
e_utf8_casefold_collate_len (const gchar *str1, const gchar *str2, gint len)
{
	gchar *s1 = g_utf8_casefold(str1, len);
	gchar *s2 = g_utf8_casefold(str2, len);
	gint rv;

	rv = g_utf8_collate (s1, s2);

	g_free (s1);
	g_free (s2);

	return rv;
}

gint
e_utf8_casefold_collate (const gchar *str1, const gchar *str2)
{
	return e_utf8_casefold_collate_len (str1, str2, -1);
}
