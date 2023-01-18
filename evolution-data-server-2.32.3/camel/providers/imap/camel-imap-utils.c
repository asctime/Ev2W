/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
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

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <glib/gstdio.h>

#include "camel-imap-store.h"
#include "camel-imap-summary.h"
#include "camel-imap-store-summary.h"
#include "camel-imap-utils.h"

#define d(x)

#define SUBFOLDER_DIR_NAME     "subfolders"
#define SUBFOLDER_DIR_NAME_LEN 10

const gchar *
imap_next_word (const gchar *buf)
{
	const gchar *word;

	/* skip over current word */
	word = buf;
	while (*word && *word != ' ')
		word++;

	/* skip over white space */
	while (*word && *word == ' ')
		word++;

	return word;
}

static void
imap_namespace_destroy (struct _namespace *namespace)
{
	struct _namespace *node, *next;

	node = namespace;
	while (node) {
		next = node->next;
		g_free (node->prefix);
		g_free (node);
		node = next;
	}
}

void
imap_namespaces_destroy (struct _namespaces *namespaces)
{
	if (namespaces) {
		imap_namespace_destroy (namespaces->personal);
		imap_namespace_destroy (namespaces->other);
		imap_namespace_destroy (namespaces->shared);
		g_free (namespaces);
	}
}

static gboolean
imap_namespace_decode (const gchar **in, struct _namespace **namespace)
{
	struct _namespace *list, *tail, *node;
	const gchar *inptr;
	gchar *astring;
	gsize len;

	inptr = *in;

	list = NULL;
	tail = (struct _namespace *) &list;

	if (g_ascii_strncasecmp (inptr, "NIL", 3) != 0) {
		if (*inptr++ != '(')
			goto exception;

		while (*inptr && *inptr != ')') {
			if (*inptr++ != '(')
				goto exception;

			node = g_new (struct _namespace, 1);
			node->next = NULL;

			/* get the namespace prefix */
			astring = imap_parse_astring (&inptr, &len);
			if (!astring) {
				g_free (node);
				goto exception;
			}

			/* decode IMAP's modified UTF-7 into UTF-8 */
			node->prefix = imap_mailbox_decode ((const guchar *) astring, len);
			g_free (astring);
			if (!node->prefix) {
				g_free (node);
				goto exception;
			}

			tail->next = node;
			tail = node;

			/* get the namespace directory delimiter */
			inptr = imap_next_word (inptr);

			if (!g_ascii_strncasecmp (inptr, "NIL", 3)) {
				inptr = imap_next_word (inptr);
				node->delim = '\0';
			} else if (*inptr++ == '"') {
				if (*inptr == '\\')
					inptr++;

				node->delim = *inptr++;

				if (*inptr++ != '"')
					goto exception;
			} else
				goto exception;

			if (*inptr == ' ') {
				/* parse extra flags... for now we
                                   don't save them, but in the future
                                   we may want to? */
				while (*inptr == ' ')
					inptr++;

				while (*inptr && *inptr != ')') {
					/* this should be a QSTRING or ATOM */
					inptr = imap_next_word (inptr);
					if (*inptr == '(') {
						/* skip over the param list */
						imap_skip_list (&inptr);
					}

					while (*inptr == ' ')
						inptr++;
				}
			}

			if (*inptr++ != ')')
				goto exception;

			/* there shouldn't be spaces according to the
                           ABNF grammar, but we all know how closely
                           people follow specs */
			while (*inptr == ' ')
				inptr++;
		}

		if (*inptr == ')')
			inptr++;
	} else {
		inptr += 3;
	}

	*in = inptr;
	*namespace = list;

	return TRUE;

 exception:

	/* clean up any namespaces we may have allocated */
	imap_namespace_destroy (list);

	return FALSE;
}

#if d(!)0
static void
namespace_dump (struct _namespace *namespace)
{
	struct _namespace *node;

	if (namespace) {
		printf ("(");
		node = namespace;
		while (node) {
			printf ("(\"%s\" ", node->prefix);
			if (node->delim)
				printf ("\"%c\")", node->delim);
			else
				printf ("NUL)");

			node = node->next;
			if (node)
				printf (" ");
		}

		printf (")");
	} else {
		printf ("NIL");
	}
}

static void
namespaces_dump (struct _namespaces *namespaces)
{
	printf ("namespace dump: ");
	namespace_dump (namespaces->personal);
	printf (" ");
	namespace_dump (namespaces->other);
	printf (" ");
	namespace_dump (namespaces->shared);
	printf ("\n");
}
#endif

struct _namespaces *
imap_parse_namespace_response (const gchar *response)
{
	struct _namespaces *namespaces;
	const gchar *inptr;

	d(printf ("parsing: %s\n", response));

	if (*response != '*')
		return NULL;

	inptr = imap_next_word (response);
	if (g_ascii_strncasecmp (inptr, "NAMESPACE", 9) != 0)
		return NULL;

	inptr = imap_next_word (inptr);

	namespaces = g_new (struct _namespaces, 1);
	namespaces->personal = NULL;
	namespaces->other = NULL;
	namespaces->shared = NULL;

	if (!imap_namespace_decode (&inptr, &namespaces->personal))
		goto exception;

	if (*inptr != ' ')
		goto exception;

	while (*inptr == ' ')
		inptr++;

	if (!imap_namespace_decode (&inptr, &namespaces->other))
		goto exception;

	if (*inptr != ' ')
		goto exception;

	while (*inptr == ' ')
		inptr++;

	if (!imap_namespace_decode (&inptr, &namespaces->shared))
		goto exception;

	d(namespaces_dump (namespaces));

	return namespaces;

 exception:

	imap_namespaces_destroy (namespaces);

	return NULL;
}

/**
 * imap_parse_list_response:
 * @store: the IMAP store whose list response we're parsing
 * @buf: the LIST or LSUB response
 * @flags: a pointer to a variable to store the flags in, or %NULL
 * @sep: a pointer to a variable to store the hierarchy separator in, or %NULL
 * @folder: a pointer to a variable to store the folder name in, or %NULL
 *
 * Parses a LIST or LSUB response and returns the desired parts of it.
 * If @folder is non-%NULL, its value must be freed by the caller.
 *
 * Returns: whether or not the response was successfully parsed.
 **/
gboolean
imap_parse_list_response (CamelImapStore *store, const gchar *buf, gint *flags, gchar *sep, gchar **folder)
{
	gboolean is_lsub = FALSE;
	const gchar *word;
	gsize len;

	if (*buf != '*')
		return FALSE;

	word = imap_next_word (buf);
	if (g_ascii_strncasecmp (word, "LIST", 4) && g_ascii_strncasecmp (word, "LSUB", 4))
		return FALSE;

	/* check if we are looking at an LSUB response */
	if (word[1] == 'S' || word[1] == 's')
		is_lsub = TRUE;

	/* get the flags */
	word = imap_next_word (word);
	if (*word != '(')
		return FALSE;

	if (flags)
		*flags = 0;

	word++;
	while (*word != ')') {
		len = strcspn (word, " )");
		if (flags) {
			if (!g_ascii_strncasecmp (word, "\\NoInferiors", len))
				*flags |= CAMEL_FOLDER_NOINFERIORS;
			else if (!g_ascii_strncasecmp (word, "\\NoSelect", len))
				*flags |= CAMEL_FOLDER_NOSELECT;
			else if (!g_ascii_strncasecmp (word, "\\Marked", len))
				*flags |= CAMEL_IMAP_FOLDER_MARKED;
			else if (!g_ascii_strncasecmp (word, "\\Unmarked", len))
				*flags |= CAMEL_IMAP_FOLDER_UNMARKED;
			else if (!g_ascii_strncasecmp (word, "\\HasChildren", len))
				*flags |= CAMEL_FOLDER_CHILDREN;
			else if (!g_ascii_strncasecmp (word, "\\HasNoChildren", len))
				*flags |= CAMEL_FOLDER_NOCHILDREN;
		}

		word += len;
		while (*word == ' ')
			word++;
	}

	/* get the directory separator */
	word = imap_next_word (word);
	if (!strncmp (word, "NIL", 3)) {
		if (sep)
			*sep = '\0';
	} else if (*word++ == '"') {
		if (*word == '\\')
			word++;
		if (sep)
			*sep = *word;
		word++;
		if (*word++ != '"')
			return FALSE;
	} else
		return FALSE;

	if (folder) {
		gchar *astring;
		gchar *mailbox;

		/* get the folder name */
		word = imap_next_word (word);
		astring = imap_parse_astring (&word, &len);
		if (!astring)
			return FALSE;

		*folder = astring;

		mailbox = imap_mailbox_decode ((const guchar *) astring, strlen (astring));
		g_free (astring);
		if (!mailbox)
			return FALSE;

		/* Kludge around Courier imap's LSUB response for INBOX when it
		 * isn't subscribed to.
		 *
		 * Ignore any \Noselect flags for INBOX when parsing
		 * an LSUB response to work around the following response:
		 *
		 * * LSUB (\Noselect \HasChildren) "." "INBOX"
		 *
		 * Fixes bug #28929 (albeight in a very dodgy way imho, but what
		 * can ya do when ya got the ignorance of marketing breathing
		 * down your neck?)
		 */
		if (is_lsub && flags && !g_ascii_strcasecmp (mailbox, "INBOX"))
			*flags &= ~CAMEL_FOLDER_NOSELECT;

		*folder = mailbox;

	}

	return TRUE;
}

/**
 * imap_parse_folder_name:
 * @store:
 * @folder_name:
 *
 * Return an array of folder paths representing the folder heirarchy.
 * For example:
 *   Full/Path/"to / from"/Folder
 * Results in:
 *   Full, Full/Path, Full/Path/"to / from", Full/Path/"to / from"/Folder
 **/
gchar **
imap_parse_folder_name (CamelImapStore *store, const gchar *folder_name)
{
	GPtrArray *heirarchy;
	gchar **paths;
	const gchar *p;

	p = folder_name;
	if (*p == store->dir_sep)
		p++;

	heirarchy = g_ptr_array_new ();

	while (*p) {
		if (*p == '"') {
			p++;
			while (*p && *p != '"')
				p++;
			if (*p)
				p++;
			continue;
		}

		if (*p == store->dir_sep)
			g_ptr_array_add (heirarchy, g_strndup (folder_name, p - folder_name));

		p++;
	}

	g_ptr_array_add (heirarchy, g_strdup (folder_name));
	g_ptr_array_add (heirarchy, NULL);

	paths = (gchar **) heirarchy->pdata;
	g_ptr_array_free (heirarchy, FALSE);

	return paths;
}

/*
 * rename_flag
 * Converts label flag name on server to name used in Evolution or back.
 * It will never return NULL, it will return empty string, instead.
 *
 * @flag: Flag to rename.
 * @len: Length of the flag name.
 * @server_to_evo: if TRUE, then converting server names to evo's names, if FALSE then opposite.
 */
static const gchar *
rename_label_flag (const gchar *flag, gint len, gboolean server_to_evo)
{
	gint i;
	const gchar *labels[] = {
		"$Label1", "$Labelimportant",
		"$Label2", "$Labelwork",
		"$Label3", "$Labelpersonal",
		"$Label4", "$Labeltodo",
		"$Label5", "$Labellater",
		NULL,      NULL };

	/* It really can pass zero-length flags inside, in that case it was able
	   to always add first label, which is definitely wrong. */
	if (!len || !flag || !*flag)
		return "";

	for (i = 0 + (server_to_evo ? 0 : 1); labels[i]; i = i + 2) {
		if (!g_ascii_strncasecmp (flag, labels[i], len))
			return labels[i + (server_to_evo ? 1 : -1)];
	}

	return "";
}

gchar *
imap_create_flag_list (guint32 flags, CamelMessageInfo *info, guint32 permanent_flags)
{
	GString *gstr = g_string_new ("(");

	if (flags & CAMEL_MESSAGE_ANSWERED)
		g_string_append (gstr, "\\Answered ");
	if (flags & CAMEL_MESSAGE_DELETED)
		g_string_append (gstr, "\\Deleted ");
	if (flags & CAMEL_MESSAGE_DRAFT)
		g_string_append (gstr, "\\Draft ");
	if (flags & CAMEL_MESSAGE_FLAGGED)
		g_string_append (gstr, "\\Flagged ");
	if (flags & CAMEL_MESSAGE_SEEN)
		g_string_append (gstr, "\\Seen ");
	if ((flags & CAMEL_MESSAGE_JUNK) != 0 && (permanent_flags & CAMEL_MESSAGE_JUNK) != 0)
		g_string_append (gstr, "Junk ");
	if ((flags & CAMEL_MESSAGE_NOTJUNK) != 0 && (permanent_flags & CAMEL_MESSAGE_NOTJUNK) != 0)
		g_string_append (gstr, "NotJunk ");

	/* send user flags to the server only when it supports it, otherwise store it locally only */
	if (info && (permanent_flags & CAMEL_MESSAGE_USER) != 0) {
		const CamelFlag *flag;
		const gchar *name;

		/* FIXME: All the custom flags are sent to the server. Not just the changed ones */
		flag = camel_message_info_user_flags(info);
		while (flag) {
			if (flag->name && *flag->name) {
				name = rename_label_flag (flag->name, strlen (flag->name), FALSE);

				if (name && *name)
					g_string_append (gstr, name);
				else
					g_string_append (gstr, flag->name);

				g_string_append (gstr, " ");
			}

			flag = flag->next;
		}
	}

	if (gstr->str[gstr->len - 1] == ' ')
		gstr->str[gstr->len - 1] = ')';
	else
		g_string_append_c (gstr, ')');

	return g_string_free (gstr, FALSE);
}

gboolean
imap_parse_flag_list (gchar **flag_list_p, guint32 *flags_out, gchar **custom_flags_out)
{
	gchar *flag_list = *flag_list_p;
	guint32 flags = 0;
	gint len;
	GString *custom_flags = NULL;
	gchar *iter;

	*flags_out = 0;

	if (*flag_list++ != '(') {
		*flag_list_p = NULL;
		return FALSE;
	}

	if (custom_flags_out)
		custom_flags = g_string_new ("");

	while (*flag_list && *flag_list != ')') {
		len = strcspn (flag_list, " )");
		if (!g_ascii_strncasecmp (flag_list, "\\Answered", len))
			flags |= CAMEL_MESSAGE_ANSWERED;
		else if (!g_ascii_strncasecmp (flag_list, "\\Deleted", len))
			flags |= CAMEL_MESSAGE_DELETED;
		else if (!g_ascii_strncasecmp (flag_list, "\\Draft", len))
			flags |= CAMEL_MESSAGE_DRAFT;
		else if (!g_ascii_strncasecmp (flag_list, "\\Flagged", len))
			flags |= CAMEL_MESSAGE_FLAGGED;
		else if (!g_ascii_strncasecmp (flag_list, "\\Seen", len))
			flags |= CAMEL_MESSAGE_SEEN;
		else if (!g_ascii_strncasecmp (flag_list, "\\Recent", len))
			flags |= CAMEL_IMAP_MESSAGE_RECENT;
		else if (!g_ascii_strncasecmp(flag_list, "\\*", len))
			flags |= CAMEL_MESSAGE_USER|CAMEL_MESSAGE_JUNK|CAMEL_MESSAGE_NOTJUNK;
		else if (!g_ascii_strncasecmp(flag_list, "Junk", len))
			flags |= CAMEL_MESSAGE_JUNK;
		else if (!g_ascii_strncasecmp(flag_list, "NotJunk", len))
			flags |= CAMEL_MESSAGE_NOTJUNK;
		else if (!g_ascii_strncasecmp(flag_list, "$Label1", len) ||
			 !g_ascii_strncasecmp(flag_list, "$Label2", len) ||
			 !g_ascii_strncasecmp(flag_list, "$Label3", len) ||
			 !g_ascii_strncasecmp(flag_list, "$Label4", len) ||
			 !g_ascii_strncasecmp(flag_list, "$Label5", len)) {
			if (custom_flags) {
				g_string_append (custom_flags, rename_label_flag (flag_list, len, TRUE));
				g_string_append_c (custom_flags, ' ');
			}
		} else {
			iter = flag_list;
			while (*iter != ' ' && *iter != ')') {
				if (custom_flags)
					g_string_append_c (custom_flags, *iter);
				++iter;
			}

			if (custom_flags)
				g_string_append_c (custom_flags, ' ');
		}

		flag_list += len;
		if (*flag_list == ' ')
			flag_list++;
	}

	if (*flag_list++ != ')') {
		*flag_list_p = NULL;

		if (custom_flags)
			g_string_free (custom_flags, TRUE);

		return FALSE;
	}

	*flag_list_p = flag_list;
	*flags_out = flags;

	if (custom_flags_out && custom_flags->len) {
		*custom_flags_out = g_string_free (custom_flags, FALSE);
	} else if (custom_flags)
		g_string_free (custom_flags, TRUE);

	return TRUE;
}

/*
 From RFC3501, which adds resp_specials over RFC2060

ATOM_CHAR       ::= <any CHAR except atom_specials>

atom_specials   ::= "(" / ")" / "{" / SPACE / CTL / list_wildcards /
                    quoted_specials / resp_secials

CHAR            ::= <any 7-bit US-ASCII character except NUL,
                     0x01 - 0x7f>

CTL             ::= <any ASCII control character and DEL,
                        0x00 - 0x1f, 0x7f>

SPACE           ::= <ASCII SP, space, 0x20>

list_wildcards  ::= "%" / "*"

quoted_specials ::= <"> / "\"

resp_specials   ::= "]"
*/

static guchar imap_atom_specials[256] = {
/*	0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F */
/* 00 */0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 10 */0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 20 */0, 1, 0, 1, 1, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1,
/* 30 */1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
/* 40 */1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
/* 50 */1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1,
/* 60 */1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
/* 70 */1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

#define imap_is_atom_char(c) ((imap_atom_specials[(c)&0xff] & 0x01) != 0)

gboolean
imap_is_atom(const gchar *in)
{
	register guchar c;
	register const gchar *p = in;

	while ((c = (guchar)*p)) {
		if (!imap_is_atom_char(c))
			return FALSE;
		p++;
	}

	/* check for empty string */
	return p!=in;
}

/**
 * imap_parse_string_generic:
 * @str_p: a pointer to a string
 * @len: a pointer to a gsize to return the length in
 * @type: type of string (#IMAP_STRING, #IMAP_ASTRING, or #IMAP_NSTRING)
 * to parse.
 *
 * This parses an IMAP "string" (quoted string or literal), "nstring"
 * (NIL or string), or "astring" (atom or string) starting at *@str_p.
 * On success, *@str_p will point to the first character after the end
 * of the string, and *@len will contain the length of the returned
 * string. On failure, *@str_p will be set to %NULL.
 *
 * This assumes that the string is in the form returned by
 * camel_imap_command(): that line breaks are indicated by LF rather
 * than CRLF.
 *
 * Returns: the parsed string, or %NULL if a NIL or no string
 * was parsed. (In the former case, *@str_p will be %NULL; in the
 * latter, it will point to the character after the NIL.)
 **/
gchar *
imap_parse_string_generic (const gchar **str_p, gsize *len, gint type)
{
	const gchar *str = *str_p;
	gchar *out;

	if (!str)
		return NULL;
	else if (*str == '"') {
		gchar *p;
		gsize size;

		str++;
		size = strcspn (str, "\"") + 1;
		p = out = g_malloc (size);

		/* a quoted string cannot be broken into multiple lines */
		while (*str && *str != '"' && *str != '\n') {
			if (*str == '\\')
				str++;
			*p++ = *str++;
			if (p - out == size) {
				out = g_realloc (out, size * 2);
				p = out + size;
				size *= 2;
			}
		}
		if (*str != '"') {
			*str_p = NULL;
			g_free (out);
			return NULL;
		}
		*p = '\0';
		*str_p = str + 1;
		*len = strlen (out);
		return out;
	} else if (*str == '{') {
		*len = strtoul (str + 1, (gchar **)&str, 10);
		if (*str++ != '}' || *str++ != '\n' || strlen (str) < *len) {
			*str_p = NULL;
			return NULL;
		}

		out = g_strndup (str, *len);
		*str_p = str + *len;
		return out;
	} else if (type == IMAP_NSTRING && !g_ascii_strncasecmp (str, "nil", 3)) {
		*str_p += 3;
		*len = 0;
		return NULL;
	} else if (type == IMAP_ASTRING && imap_is_atom_char ((guchar)*str)) {
		while (imap_is_atom_char ((guchar) *str))
			str++;

		*len = str - *str_p;
		out = g_strndup (*str_p, *len);
		*str_p += *len;
		return out;
	} else {
		*str_p = NULL;
		return NULL;
	}
}

static inline void
skip_char (const gchar **in, gchar ch)
{
	if (*in && **in == ch)
		*in = *in + 1;
	else
		*in = NULL;
}

/* Skip atom, string, or number */
static void
skip_asn (const gchar **str_p)
{
	const gchar *str = *str_p;

	if (!str)
		return;
	else if (*str == '"') {
		while (*++str && *str != '"') {
			if (*str == '\\') {
				str++;
				if (!*str)
					break;
			}
		}
		if (*str == '"')
			*str_p = str + 1;
		else
			*str_p = NULL;
	} else if (*str == '{') {
		gulong len;

		len = strtoul (str + 1, (gchar **) &str, 10);
		if (*str != '}' || *(str + 1) != '\n' ||
		    strlen (str + 2) < len) {
			*str_p = NULL;
			return;
		}
		*str_p = str + 2 + len;
	} else {
		/* We assume the string is well-formed and don't
		 * bother making sure it's a valid atom.
		 */
		while (*str && *str != ')' && *str != ' ')
			str++;
		*str_p = str;
	}
}

void
imap_skip_list (const gchar **str_p)
{
	skip_char (str_p, '(');
	while (*str_p && **str_p != ')') {
		if (**str_p == '(')
			imap_skip_list (str_p);
		else
			skip_asn (str_p);
		if (*str_p && **str_p == ' ')
			skip_char (str_p, ' ');
	}
	skip_char (str_p, ')');
}

static gint
parse_params (const gchar **parms_p, CamelContentType *type)
{
	const gchar *parms = *parms_p;
	gchar *name, *value;
	gsize len;

	if (!g_ascii_strncasecmp (parms, "nil", 3)) {
		*parms_p += 3;
		return 0;
	}

	if (*parms++ != '(')
		return -1;

	while (parms && *parms != ')') {
		name = imap_parse_nstring (&parms, &len);
		skip_char (&parms, ' ');
		value = imap_parse_nstring (&parms, &len);

		if (name && value)
			camel_content_type_set_param (type, name, value);
		g_free (name);
		g_free (value);

		if (parms && *parms == ' ')
			parms++;
	}

	if (!parms || *parms++ != ')')
		return -1;

	*parms_p = parms;

	return 0;
}

static CamelMessageContentInfo *
imap_body_decode (const gchar **in, CamelMessageContentInfo *ci, CamelFolder *folder, GPtrArray *cis)
{
	const gchar *inptr = *in;
	CamelMessageContentInfo *child = NULL;
	gchar *type, *subtype, *id = NULL;
	CamelContentType *ctype = NULL;
	gchar *description = NULL;
	gchar *encoding = NULL;
	gsize len;
	gsize size;
	gchar *p;

	if (*inptr++ != '(')
		return NULL;

	if (ci == NULL) {
		ci = camel_folder_summary_content_info_new (folder->summary);
		g_ptr_array_add (cis, ci);
	}

	if (*inptr == '(') {
		/* body_type_mpart */
		CamelMessageContentInfo *tail, *children = NULL;

		tail = (CamelMessageContentInfo *) &children;

		do {
			if (!(child = imap_body_decode (&inptr, NULL, folder, cis)))
				return NULL;

			child->parent = ci;
			tail->next = child;
			tail = child;
		} while (inptr && *inptr == '(');

		if (!inptr || *inptr++ != ' ')
			return NULL;

		if (g_ascii_strncasecmp (inptr, "nil", 3) != 0) {
			subtype = imap_parse_string (&inptr, &len);
		} else {
			subtype = NULL;
			inptr += 3;
		}

		ctype = camel_content_type_new ("multipart", subtype ? subtype : "mixed");
		g_free (subtype);

		if (!inptr || *inptr++ != ')') {
			camel_content_type_unref (ctype);
			return NULL;
		}

		ci->type = ctype;
		ci->childs = children;
	} else {
		/* body_type_1part */
		if (g_ascii_strncasecmp (inptr, "nil", 3) != 0) {
			type = imap_parse_string (&inptr, &len);
			if (inptr == NULL)
				return NULL;
		} else {
			return NULL;
		}

		if (*inptr++ != ' ') {
			g_free (type);
			return NULL;
		}

		if (g_ascii_strncasecmp (inptr, "nil", 3) != 0) {
			subtype = imap_parse_string (&inptr, &len);
			if (inptr == NULL) {
				g_free (type);
				return NULL;
			}
		} else {
			if (!g_ascii_strcasecmp (type, "text"))
				subtype = g_strdup ("plain");
			else
				subtype = NULL;
			inptr += 3;
		}

		camel_strdown (type);
		camel_strdown (subtype);
		ctype = camel_content_type_new (type, subtype);
		g_free (subtype);
		g_free (type);

		if (*inptr++ != ' ')
			goto exception;

		/* content-type params */
		if (parse_params (&inptr, ctype) == -1)
			goto exception;

		if (!inptr || *inptr++ != ' ')
			goto exception;

		/* content-id */
		if (g_ascii_strncasecmp (inptr, "nil", 3) != 0) {
			id = imap_parse_string (&inptr, &len);
			if (inptr == NULL)
				goto exception;
		} else
			inptr += 3;

		if (*inptr++ != ' ')
			goto exception;

		/* description */
		if (g_ascii_strncasecmp (inptr, "nil", 3) != 0) {
			description = imap_parse_string (&inptr, &len);
			if (inptr == NULL)
				goto exception;
		} else
			inptr += 3;

		if (*inptr++ != ' ')
			goto exception;

		/* encoding */
		if (g_ascii_strncasecmp (inptr, "nil", 3) != 0) {
			encoding = imap_parse_string (&inptr, &len);
			if (inptr == NULL)
				goto exception;
		} else
			inptr += 3;

		if (*inptr++ != ' ')
			goto exception;

		/* size */
		size = strtoul ((const gchar *) inptr, &p, 10);

		/* check if the size wasn't negative */
		if (p) {
			while (inptr < p && *inptr != '-')
				inptr++;

			if (inptr < p)
				size = 0;
		}

		if (size == 0)
			goto exception;

		inptr = (const gchar *) p;

		if (camel_content_type_is (ctype, "message", "rfc822")) {
			/* body_type_msg */
			if (!inptr || *inptr++ != ' ')
				goto exception;

			/* envelope */
			imap_skip_list (&inptr);

			if (!inptr || *inptr++ != ' ')
				goto exception;

			/* body */
			if (!(child = imap_body_decode (&inptr, NULL, folder, cis)))
				goto exception;
			child->parent = ci;

			if (!inptr || *inptr++ != ' ')
				goto exception;

			/* lines */
			strtoul ((const gchar *) inptr, &p, 10);
			inptr = (const gchar *) p;
		} else if (camel_content_type_is (ctype, "text", "*")) {
			if (!inptr || *inptr++ != ' ')
				goto exception;

			/* lines */
			strtoul ((const gchar *) inptr, &p, 10);
			inptr = (const gchar *) p;
		} else {
			/* body_type_basic */
		}

		if (!inptr || *inptr++ != ')')
			goto exception;

		ci->type = ctype;
		ci->id = id;
		ci->description = description;
		ci->encoding = encoding;
		ci->size = size;
		ci->childs = child;

		if (*inptr == ' ' && inptr[1] == '(' && inptr[2] == '\"')
			inptr++;
	}

	*in = inptr;

	return ci;

 exception:

	camel_content_type_unref (ctype);
	g_free (id);
	g_free (description);
	g_free (encoding);

	return NULL;
}

/**
 * imap_parse_body:
 * @body_p: pointer to the start of an IMAP "body"
 * @folder: an imap folder
 * @ci: a CamelMessageContentInfo to fill in
 *
 * This fills in @ci with data from *@body_p. On success *@body_p
 * will point to the character after the body. On failure, it will be
 * set to %NULL and @ci will be unchanged.
 **/
void
imap_parse_body (const gchar **body_p, CamelFolder *folder,
		 CamelMessageContentInfo *ci)
{
	const gchar *inptr = *body_p;
	CamelMessageContentInfo *child;
	GPtrArray *children;
	gint i;

	if (!inptr || *inptr != '(') {
		*body_p = NULL;
		return;
	}

	children = g_ptr_array_new ();

	if (!(imap_body_decode (&inptr, ci, folder, children))) {
		for (i = 0; i < children->len; i++) {
			child = children->pdata[i];

			/* content_info_free will free all the child
			 * nodes, but we don't want that. */
			child->next = NULL;
			child->parent = NULL;
			child->childs = NULL;

			camel_folder_summary_content_info_free (folder->summary, child);
		}
		*body_p = NULL;
	} else {
		*body_p = inptr;
	}

	g_ptr_array_free (children, TRUE);
}

/**
 * imap_quote_string:
 * @str: the string to quote, which must not contain CR or LF
 *
 * Returns: an IMAP "quoted" corresponding to the string, which
 * the caller must free.
 **/
gchar *
imap_quote_string (const gchar *str)
{
	const gchar *p;
	gchar *quoted, *q;
	gint len;

	g_assert (strchr (str, '\r') == NULL);

	len = strlen (str);
	p = str;
	while ((p = strpbrk (p, "\"\\"))) {
		len++;
		p++;
	}

	quoted = q = g_malloc (len + 3);
	*q++ = '"';
	for (p = str; *p; ) {
		if (strchr ("\"\\", *p))
			*q++ = '\\';
		*q++ = *p++;
	}
	*q++ = '"';
	*q = '\0';

	return quoted;
}

static inline gulong
get_summary_uid_numeric (CamelFolderSummary *summary, gint index)
{
	gulong uid;
	gchar *suid;

	suid = camel_folder_summary_uid_from_index (summary, index);
	uid = strtoul (suid, NULL, 10);
	g_free (suid);

	return uid;
}

/* the max number of chars that an unsigned 32-bit gint can be is 10 chars plus 1 for a possible : */
#define UID_SET_FULL(setlen, maxlen) (maxlen > 0 ? setlen + 11 >= maxlen : FALSE)

/**
 * imap_uid_array_to_set:
 * @summary: summary for the folder the UIDs come from
 * @uids: a (sorted) array of UIDs
 * @uid: uid index to start at
 * @maxlen: max length of the set string (or -1 for infinite)
 * @lastuid: index offset of the last uid used
 *
 * Creates an IMAP "set" up to @maxlen bytes long, covering the listed
 * UIDs starting at index @uid and not covering any UIDs that are in
 * @summary but not in @uids. It doesn't actually require that all (or
 * any) of the UIDs be in @summary.
 *
 * After calling, @lastuid will be set the index of the first uid
 * *not* included in the returned set string.
 *
 * Note: @uids MUST be in sorted order for this code to work properly.
 *
 * Returns: the set, which the caller must free with g_free()
 **/
gchar *
imap_uid_array_to_set (CamelFolderSummary *summary, GPtrArray *uids, gint uid, gssize maxlen, gint *lastuid)
{
	gulong last_uid, next_summary_uid, this_uid;
	gboolean range = FALSE;
	gint si, scount;
	GString *gset;
	gchar *set;

	g_return_val_if_fail (uids->len > uid, NULL);

	gset = g_string_new (uids->pdata[uid]);
	last_uid = strtoul (uids->pdata[uid], NULL, 10);
	next_summary_uid = 0;
	scount = camel_folder_summary_count (summary);

	for (uid++, si = 0; uid < uids->len && !UID_SET_FULL (gset->len, maxlen); uid++) {
		/* Find the next UID in the summary after the one we
		 * just wrote out.
		 */
		for (; last_uid >= next_summary_uid && si < scount; si++)
			next_summary_uid = get_summary_uid_numeric (summary, si);
		if (last_uid >= next_summary_uid)
			next_summary_uid = (gulong) -1;

		/* Now get the next UID from @uids */
		this_uid = strtoul (uids->pdata[uid], NULL, 10);
		if (this_uid == next_summary_uid || this_uid == last_uid + 1)
			range = TRUE;
		else {
			if (range) {
				g_string_append_printf (gset, ":%lu", last_uid);
				range = FALSE;
			}
			g_string_append_printf (gset, ",%lu", this_uid);
		}

		last_uid = this_uid;
	}

	if (range)
		g_string_append_printf (gset, ":%lu", last_uid);

	*lastuid = uid;

	set = gset->str;
	g_string_free (gset, FALSE);

	return set;
}

/**
 * imap_uid_set_to_array:
 * @summary: summary for the folder the UIDs come from
 * @uids: a pointer to the start of an IMAP "set" of UIDs
 *
 * Fills an array with the UIDs corresponding to @uids and @summary.
 * There can be text after the uid set in @uids, which will be
 * ignored.
 *
 * If @uids specifies a range of UIDs that extends outside the range
 * of @summary, the function will assume that all of the "missing" UIDs
 * do exist.
 *
 * Returns: the array of uids, which the caller must free with
 * imap_uid_array_free(). (Or %NULL if the uid set can't be parsed.)
 **/
GPtrArray *
imap_uid_set_to_array (CamelFolderSummary *summary, const gchar *uids)
{
	GPtrArray *arr;
	gchar *p, *q;
	gulong uid, suid;
	gint si, scount;

	arr = g_ptr_array_new ();
	scount = camel_folder_summary_count (summary);

	p = (gchar *)uids;
	si = 0;
	do {
		uid = strtoul (p, &q, 10);
		if (p == q)
			goto lose;
		g_ptr_array_add (arr, g_strndup (p, q - p));

		if (*q == ':') {
			/* Find the summary entry for the UID after the one
			 * we just saw.
			 */
			while (++si < scount) {
				suid = get_summary_uid_numeric (summary, si);
				if (suid > uid)
					break;
			}
			if (si >= scount)
				suid = uid + 1;

			uid = strtoul (q + 1, &p, 10);
			if (p == q + 1)
				goto lose;

			/* Add each summary UID until we find one
			 * larger than the end of the range
			 */
			while (suid <= uid) {
				g_ptr_array_add (arr, g_strdup_printf ("%lu", suid));
				if (++si < scount)
					suid = get_summary_uid_numeric (summary, si);
				else
					suid++;
			}
		} else
			p = q;
	} while (*p++ == ',');

	return arr;

 lose:
	g_warning ("Invalid uid set %s", uids);
	imap_uid_array_free (arr);
	return NULL;
}

/**
 * imap_uid_array_free:
 * @arr: an array returned from imap_uid_set_to_array()
 *
 * Frees @arr
 **/
void
imap_uid_array_free (GPtrArray *arr)
{
	gint i;

	for (i = 0; i < arr->len; i++)
		g_free (arr->pdata[i]);
	g_ptr_array_free (arr, TRUE);
}

gchar *
imap_concat (CamelImapStore *imap_store, const gchar *prefix, const gchar *suffix)
{
	gsize len;
	CamelImapStoreNamespace *ns = camel_imap_store_summary_get_main_namespace (imap_store->summary);

	len = strlen (prefix);
	if (len == 0 || !ns || prefix[len - 1] == ns->sep)
		return g_strdup_printf ("%s%s", prefix, suffix);
	else
		return g_strdup_printf ("%s%c%s", prefix, ns->sep, suffix);
}

gchar *
imap_mailbox_encode (const guchar *in, gsize inlen)
{
	gchar *buf;

	buf = g_alloca (inlen + 1);
	memcpy (buf, in, inlen);
	buf[inlen] = 0;

	return camel_utf8_utf7 (buf);
}

gchar *
imap_mailbox_decode (const guchar *in, gsize inlen)
{
	gchar *buf;

	buf = g_alloca (inlen + 1);
	memcpy (buf, in, inlen);
	buf[inlen] = 0;

	return camel_utf7_utf8 (buf);
}

gchar *
imap_path_to_physical (const gchar *prefix, const gchar *vpath)
{
	GString *out = g_string_new(prefix);
	const gchar *p = vpath;
	gchar c, *res;

	g_string_append_c(out, '/');
	p = vpath;
	while ((c = *p++)) {
		if (c == '/') {
			g_string_append(out, "/" SUBFOLDER_DIR_NAME "/");
			while (*p == '/')
				p++;
		} else
			g_string_append_c(out, c);
	}

	res = out->str;
	g_string_free(out, FALSE);

	return res;
}

static gboolean
find_folders_recursive (const gchar *physical_path, const gchar *path,
			IMAPPathFindFoldersCallback callback, gpointer data)
{
	GDir *dir;
	gchar *subfolder_directory_path;
	gboolean ok;

	if (*path) {
		if (!callback (physical_path, path, data))
			return FALSE;

		subfolder_directory_path = g_strdup_printf ("%s/%s", physical_path, SUBFOLDER_DIR_NAME);
	} else {
		/* On the top level, we have no folders and,
		 * consequently, no subfolder directory.
		 */

		subfolder_directory_path = g_strdup (physical_path);
	}

	/* Now scan the subfolders and load them. */
	dir = g_dir_open (subfolder_directory_path, 0, NULL);
	if (dir == NULL) {
		g_free (subfolder_directory_path);
		return TRUE;
	}

	ok = TRUE;
	while (ok) {
		struct stat file_stat;
		const gchar *dirent;
		gchar *file_path;
		gchar *new_path;

		dirent = g_dir_read_name (dir);
		if (dirent == NULL)
			break;

		file_path = g_strdup_printf ("%s/%s", subfolder_directory_path,
					     dirent);

		if (g_stat (file_path, &file_stat) < 0 ||
		    !S_ISDIR (file_stat.st_mode)) {
			g_free (file_path);
			continue;
		}

		new_path = g_strdup_printf ("%s/%s", path, dirent);

		ok = find_folders_recursive (file_path, new_path, callback, data);

		g_free (file_path);
		g_free (new_path);
	}

	g_dir_close (dir);
	g_free (subfolder_directory_path);

	return ok;
}

/**
 * imap_path_find_folders:
 * @prefix: directory to start from
 * @callback: Callback to invoke on each folder
 * @data: Data for @callback
 *
 * Walks the folder tree starting at @prefix and calls @callback
 * on each folder.
 *
 * Returns: %TRUE on success, %FALSE if an error occurs at any point
 **/
gboolean
imap_path_find_folders (const gchar *prefix, IMAPPathFindFoldersCallback callback, gpointer data)
{
	return find_folders_recursive (prefix, "", callback, data);
}
