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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

/*
 * LDIF importer.  LDIF is the file format of an exported Netscape
 * addressbook.
 *
 * Framework copied from evolution-gnomecard-importer.c
 *
 * Michael M. Morrison (mmorrison@kqcorp.com)
 *
 * Multi-line value support, mailing list support, base64 support, and
 * various fixups: Chris Toshok (toshok@ximian.com)
 *
 * Made re-entrant, converted to eplugin, Michael Zucchi <notzed@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <libebook/e-book.h>
#include <libedataserverui/e-source-selector.h>

#include <libebook/e-destination.h>

#include "e-util/e-import.h"

#include "evolution-addressbook-importers.h"

typedef struct {
	EImport *import;
	EImportTarget *target;

	guint idle_id;

	GHashTable *dn_contact_hash;

	gint state;		/* 0 - initial scan, 1 - list cards, 2 - cancelled/complete */
	FILE *file;
	gulong size;

	EBook *book;

	GSList *contacts;
	GSList *list_contacts;
	GSList *list_iterator;
} LDIFImporter;

static void ldif_import_done(LDIFImporter *gci);

static struct {
	const gchar *ldif_attribute;
	EContactField contact_field;
#define FLAG_HOME_ADDRESS	0x01
#define FLAG_WORK_ADDRESS	0x02
#define FLAG_LIST		0x04
#define FLAG_BOOLEAN		0x08
	gint flags;
}
ldif_fields[] = {
	{ "cn", E_CONTACT_FULL_NAME },
	{ "mail", E_CONTACT_EMAIL, FLAG_LIST },
	{ "mozillaSecondEmail", E_CONTACT_EMAIL_2},
#if 0
	{ "givenname", E_CONTACT_GIVEN_NAME },
#endif
	{ "sn", E_CONTACT_FAMILY_NAME },
	{ "xmozillanickname", E_CONTACT_NICKNAME },

	{ "o", E_CONTACT_ORG },
	{ "ou", E_CONTACT_ORG_UNIT },
	{ "title", E_CONTACT_TITLE },

	{ "locality", 0, FLAG_WORK_ADDRESS },
	{ "l", 0, FLAG_WORK_ADDRESS },
	{ "mozillahomelocalityname", 0, FLAG_HOME_ADDRESS },
	{ "st", 0, FLAG_WORK_ADDRESS },
	{ "mozillaHomeState", 0, FLAG_HOME_ADDRESS },
	{ "streetaddress", 0, FLAG_WORK_ADDRESS },
	{ "postalcode", 0, FLAG_WORK_ADDRESS },
	{ "mozillaHomePostalCode", 0, FLAG_HOME_ADDRESS },
	{ "countryname", 0, FLAG_WORK_ADDRESS },
	{ "c", 0, FLAG_WORK_ADDRESS },
	{ "mozillaHomeCountryName", 0, FLAG_HOME_ADDRESS },
	{ "postalAddress", 0, FLAG_WORK_ADDRESS },
	{ "homePostalAddress", 0, FLAG_HOME_ADDRESS },
	{ "mozillaPostalAddress2", 0, FLAG_WORK_ADDRESS },
	{ "mozillaHomePostalAddress2", 0, FLAG_HOME_ADDRESS },

	{ "telephonenumber", E_CONTACT_PHONE_BUSINESS},
	{ "homephone", E_CONTACT_PHONE_HOME },
	{ "facsimiletelephonenumber", E_CONTACT_PHONE_BUSINESS_FAX },
	{ "pagerphone", E_CONTACT_PHONE_PAGER },
	{ "cellphone", E_CONTACT_PHONE_MOBILE },
	{ "mobile", E_CONTACT_PHONE_MOBILE },

	{ "homeurl", E_CONTACT_HOMEPAGE_URL },
	{ "mozillaHomeUrl", E_CONTACT_HOMEPAGE_URL },

	{ "description", E_CONTACT_NOTE },

	{ "xmozillausehtmlmail", E_CONTACT_WANTS_HTML, FLAG_BOOLEAN },

	{ "nsAIMid", E_CONTACT_IM_AIM, FLAG_LIST },
	{ "mozilla_AimScreenName", E_CONTACT_IM_AIM, FLAG_LIST }
};

static GString *
getValue( gchar **src )
{
	GString *dest = g_string_new("");
	gchar *s = *src;
	gboolean need_base64 = (*s == ':');

 copy_line:
	while (*s != 0 && *s != '\n' && *s != '\r')
		dest = g_string_append_c (dest, *s++);

	if (*s == '\r') s++;
	if (*s == '\n')	s++;

	/* check for continuation here */
	if (*s == ' ') {
		s++;
		goto copy_line;
	}

	if (need_base64) {
		guchar *data;
		gsize length;

		/* XXX g_string_assign_len() would be nice here */
		data = g_base64_decode (dest->str + 2, &length);
		g_string_truncate (dest, 0);
		g_string_append_len (dest, (gchar *) data, length);
		g_free (data);
	}

	*src = s;

	return dest;
}

static void
populate_contact_address (EContactAddress *address, gchar *attr, gchar *value)
{
	if (!g_ascii_strcasecmp (attr, "locality") ||
	    !g_ascii_strcasecmp (attr, "l") ||
	    !g_ascii_strcasecmp (attr, "mozillaHomeLocalityName"))
		address->locality = g_strdup (value);
	else if (!g_ascii_strcasecmp (attr, "countryname") ||
		 !g_ascii_strcasecmp (attr, "c") ||
		 !g_ascii_strcasecmp (attr, "mozillaHomeCountryName"))
		address->country = g_strdup (value);
	else if (!g_ascii_strcasecmp (attr, "postalcode") ||
		 !g_ascii_strcasecmp (attr, "mozillaHomePostalCode"))
		address->code = g_strdup (value);
	else if (!g_ascii_strcasecmp (attr, "st") ||
		 !g_ascii_strcasecmp (attr, "mozillaHomeState"))
		address->region = g_strdup (value);
	else if (!g_ascii_strcasecmp (attr, "streetaddress"))
		address->street = g_strdup (value);
	else if (!g_ascii_strcasecmp (attr, "mozillaPostalAddress2") ||
		 !g_ascii_strcasecmp (attr, "mozillaHomePostalAddress2")) {
		if (address->ext && *address->ext) {
			gchar *temp = g_strdup (address->ext);
			g_free (address->ext);
			address->ext = g_strconcat (temp, ",\n", value, NULL);
			g_free (temp);
		}
		else {
			address->ext = g_strdup (value);
		}
	}
	else if (!g_ascii_strcasecmp (attr, "postalAddress") ||
		 !g_ascii_strcasecmp (attr, "homepostalAddress")) {
		gchar *c, *i, *addr_field;

		addr_field =  g_strdup (value);
		i = addr_field;
		for (c = addr_field; *c != '\0'; c++) {
			i++;
			if (*c == ',' &&  *i != '\0' && *i == ' ') {
				*i = '\n';
			}
		}
		if (address->ext && *address->ext) {
			gchar *temp = g_strdup (address->ext);
			g_free (address->ext);
			address->ext = g_strconcat (addr_field, ",\n", temp, NULL);
			g_free (temp);
			g_free (addr_field);
		}
		else {
			address->ext = addr_field;
		}
	}
}

static gboolean
parseLine (GHashTable *dn_contact_hash, EContact *contact,
	   EContactAddress *work_address, EContactAddress *home_address,
	   gchar **buf)
{
	gchar *ptr;
	gchar *colon, *value;
	gboolean field_handled;
	GString *ldif_value;

	ptr = *buf;

	/* if the string is empty, return */
	if (*ptr == '\0') {
		*buf = NULL;
		return TRUE;
	}

	/* skip comment lines */
	if (*ptr == '#') {
		ptr = strchr (ptr, '\n');
		if (!ptr)
			*buf = NULL;
		else
			*buf = ptr + 1;
		return TRUE;
	}

	/* first, check for a 'continuation' line */
	if (ptr[0] == ' ' && ptr[1] != '\n') {
		g_warning ("unexpected continuation line");
		return FALSE;
	}

	colon = (gchar *)strchr( ptr, ':' );
	if (colon) {
		gint i;

		*colon = 0;
		value = colon + 1;
		while (isspace(*value))
			value++;

		ldif_value = getValue(&value );

		field_handled = FALSE;
		for (i = 0; i < G_N_ELEMENTS (ldif_fields); i++) {
			if (!g_ascii_strcasecmp (ptr, ldif_fields[i].ldif_attribute)) {
				if (ldif_fields[i].flags & FLAG_WORK_ADDRESS) {
					populate_contact_address (work_address, ptr, ldif_value->str);
				}
				else if (ldif_fields[i].flags & FLAG_HOME_ADDRESS) {
					populate_contact_address (home_address, ptr, ldif_value->str);
				}
				else if (ldif_fields[i].flags & FLAG_LIST) {
					GList *list;

					list = e_contact_get (contact, ldif_fields[i].contact_field);
					list = g_list_append (list, g_strdup (ldif_value->str));
					e_contact_set (contact, ldif_fields[i].contact_field, list);

					g_list_free_full (list, g_free);
				}
				else if (ldif_fields[i].flags & FLAG_BOOLEAN) {
					if (!g_ascii_strcasecmp (ldif_value->str, "true")) {
						e_contact_set (contact, ldif_fields[i].contact_field, GINT_TO_POINTER (TRUE));
					}
					else {
						e_contact_set (contact, ldif_fields[i].contact_field, GINT_TO_POINTER (FALSE));
					}
					g_message ("set %s to %s", ptr, ldif_value->str);
				}
				else {
					/* FIXME is everything a string? */
					e_contact_set (contact, ldif_fields[i].contact_field, ldif_value->str);
					g_message ("set %s to %s", ptr, ldif_value->str);
				}
				field_handled = TRUE;
				break;
			}
		}

		/* handle objectclass/dn/member out here */
		if (!field_handled) {
			if (!g_ascii_strcasecmp (ptr, "dn"))
				g_hash_table_insert (
					dn_contact_hash,
					g_strdup (ldif_value->str), contact);
			else if (!g_ascii_strcasecmp (ptr, "objectclass") &&
				!g_ascii_strcasecmp (ldif_value->str, "groupofnames")) {
				e_contact_set (
					contact, E_CONTACT_IS_LIST,
					GINT_TO_POINTER (TRUE));
			} else if (!g_ascii_strcasecmp (ptr, "member")) {
				GList *email;

				email = e_contact_get (contact, E_CONTACT_EMAIL);
				email = g_list_append (email, g_strdup (ldif_value->str));
				e_contact_set (contact, E_CONTACT_EMAIL, email);

				g_list_free_full (email, g_free);
			}
		}

		/* put the colon back the way it was, just for kicks */
		*colon = ':';

		g_string_free (ldif_value, TRUE);

	} else {
		g_warning ("unrecognized entry %s", ptr);
		return FALSE;
	}

	*buf = value;

	return TRUE;
}

static EContact *
getNextLDIFEntry(GHashTable *dn_contact_hash, FILE *f )
{
	EContact *contact;
	EContactAddress *work_address, *home_address;
	GString *str;
	gchar line[1024];
	gchar *buf;

	str = g_string_new ("");
	/* read from the file until we get to a blank line (or eof) */
	while (!feof (f)) {
		if (!fgets (line, sizeof(line), f))
			break;
		if (line[0] == '\n' || (line[0] == '\r' && line[1] == '\n'))
			break;
		str = g_string_append (str, line);
	}

	if (strlen (str->str) == 0) {
		g_string_free (str, TRUE);
		return NULL;
	}

	/* now parse that entry */
	contact = e_contact_new ();
	work_address = g_new0 (EContactAddress, 1);
	home_address = g_new0 (EContactAddress, 1);

	buf = str->str;
	while (buf) {
		if (!parseLine (dn_contact_hash, contact, work_address, home_address, &buf)) {
			/* parsing error */
			g_string_free (str, TRUE);
			e_contact_address_free (work_address);
			e_contact_address_free (home_address);
			g_object_unref (contact);
			return NULL;
		}
	}

	/* fill in the address */
	if (work_address->locality || work_address->country || work_address->ext ||
	    work_address->code || work_address->region || work_address->street) {
		e_contact_set (contact, E_CONTACT_ADDRESS_WORK, work_address);
	}
	if (home_address->locality || home_address->country || home_address->ext ||
	    home_address->code || home_address->region || home_address->street) {
		e_contact_set (contact, E_CONTACT_ADDRESS_HOME, home_address);
	}
	e_contact_address_free (work_address);
	e_contact_address_free (home_address);

	g_string_free (str, TRUE);

	return contact;
}

static void
resolve_list_card (LDIFImporter *gci, EContact *contact)
{
	GList *email, *l;
	GList *email_attrs = NULL;
	gchar *full_name;

	/* set file_as to full_name so we don't later try and figure
           out a first/last name for the list. */
	full_name = e_contact_get (contact, E_CONTACT_FULL_NAME);
	if (full_name)
		e_contact_set (contact, E_CONTACT_FILE_AS, full_name);
	g_free (full_name);

	/* FIMXE getting might not be implemented in ebook */
	email = e_contact_get (contact, E_CONTACT_EMAIL);
	for (l = email; l; l = l->next) {
		/* mozilla stuffs dn's in the EMAIL list for contact lists */
		gchar *dn = l->data;
		EContact *dn_contact = g_hash_table_lookup (gci->dn_contact_hash, dn);

		/* break list chains here, since we don't support them just yet */
		if (dn_contact && !e_contact_get (dn_contact, E_CONTACT_IS_LIST)) {
			EDestination *dest;
			EVCardAttribute *attr = e_vcard_attribute_new (NULL, EVC_EMAIL);

			/* Hard-wired for default e-mail, since netscape only exports 1 email address */
			dest = e_destination_new ();
			e_destination_set_contact (dest, dn_contact, 0);

			e_destination_export_to_vcard_attribute (dest, attr);

			g_object_unref (dest);

			email_attrs = g_list_append (email_attrs, attr);
		}
	}
	e_contact_set_attributes (contact, E_CONTACT_EMAIL, email_attrs);

	g_list_free_full (email, g_free);
	g_list_free_full (email_attrs, (GDestroyNotify)e_vcard_attribute_free);
}

static void
add_to_notes (EContact *contact, EContactField field)
{
	const gchar *old_text;
	const gchar *field_text;
	gchar       *new_text;

	old_text = e_contact_get_const (contact, E_CONTACT_NOTE);
	if (old_text && strstr (old_text, e_contact_pretty_name (field)))
		return;

	field_text = e_contact_get_const (contact, field);
	if (!field_text || !*field_text)
		return;

	new_text = g_strdup_printf ("%s%s%s: %s",
				    old_text ? old_text : "",
				    old_text && *old_text &&
				    *(old_text + strlen (old_text) - 1) != '\n' ? "\n" : "",
				    e_contact_pretty_name (field), field_text);
	e_contact_set (contact, E_CONTACT_NOTE, new_text);
	g_free (new_text);
}

static gboolean
ldif_import_contacts(gpointer d)
{
	LDIFImporter *gci = d;
	EContact *contact;
	GSList *iter;
	gint count = 0;

	/* We process all normal cards immediately and keep the list
	   ones till the end */

	if (gci->state == 0) {
		while (count < 50 && (contact = getNextLDIFEntry(gci->dn_contact_hash, gci->file))) {
			if (e_contact_get (contact, E_CONTACT_IS_LIST)) {
				gci->list_contacts = g_slist_prepend(gci->list_contacts, contact);
			} else {
				add_to_notes(contact, E_CONTACT_OFFICE);
				add_to_notes(contact, E_CONTACT_SPOUSE);
				add_to_notes(contact, E_CONTACT_BLOG_URL);
				e_book_add_contact(gci->book, contact, NULL);
				gci->contacts = g_slist_prepend(gci->contacts, contact);
			}
			count++;
		}
		if (contact == NULL) {
			gci->state = 1;
			gci->list_iterator = gci->list_contacts;
		}
	}
	if (gci->state == 1) {
		for (iter = gci->list_iterator;count < 50 && iter;iter=iter->next) {
			contact = iter->data;
			resolve_list_card(gci, contact);
			e_book_add_contact(gci->book, contact, NULL);
			count++;
		}
		gci->list_iterator = iter;
		if (iter == NULL)
			gci->state = 2;
	}
	if (gci->state == 2) {
		ldif_import_done(gci);
		return FALSE;
	} else {
		e_import_status (
			gci->import, gci->target, _("Importing..."),
			ftell (gci->file) * 100 / gci->size);
		return TRUE;
	}
}

static void
primary_selection_changed_cb (ESourceSelector *selector, EImportTarget *target)
{
	g_datalist_set_data_full(&target->data, "ldif-source",
				 g_object_ref(e_source_selector_peek_primary_selection(selector)),
				 g_object_unref);
}

static GtkWidget *
ldif_getwidget(EImport *ei, EImportTarget *target, EImportImporter *im)
{
	GtkWidget *vbox, *selector;
	ESource *primary;
	ESourceList *source_list;

	/* FIXME Better error handling */
	if (!e_book_get_addressbooks (&source_list, NULL))
		return NULL;

	vbox = gtk_vbox_new (FALSE, FALSE);

	selector = e_source_selector_new (source_list);
	e_source_selector_show_selection (E_SOURCE_SELECTOR (selector), FALSE);
	gtk_box_pack_start (GTK_BOX (vbox), selector, FALSE, TRUE, 6);

	primary = g_datalist_get_data(&target->data, "ldif-source");
	if (primary == NULL) {
		primary = e_source_list_peek_source_any (source_list);
		g_object_ref(primary);
		g_datalist_set_data_full (
			&target->data, "ldif-source", primary,
			(GDestroyNotify) g_object_unref);
	}
	e_source_selector_set_primary_selection (
		E_SOURCE_SELECTOR (selector), primary);
	g_object_unref (source_list);

	g_signal_connect (
		selector, "primary_selection_changed",
		G_CALLBACK (primary_selection_changed_cb), target);

	gtk_widget_show_all (vbox);

	return vbox;
}

static const gchar *supported_extensions[3] = {
	".ldif", ".ldi", NULL
};

static gboolean
ldif_supported(EImport *ei, EImportTarget *target, EImportImporter *im)
{
	gchar *ext;
	gint i;
	EImportTargetURI *s;

	if (target->type != E_IMPORT_TARGET_URI)
		return FALSE;

	s = (EImportTargetURI *)target;
	if (s->uri_src == NULL)
		return TRUE;

	if (strncmp(s->uri_src, "file:///", 8) != 0)
		return FALSE;

	ext = strrchr(s->uri_src, '.');
	if (ext == NULL)
		return FALSE;

	for (i = 0; supported_extensions[i] != NULL; i++) {
		if (g_ascii_strcasecmp(supported_extensions[i], ext) == 0)
			return TRUE;
	}

	return FALSE;
}

static void
ldif_import_done(LDIFImporter *gci)
{
	if (gci->idle_id)
		g_source_remove(gci->idle_id);

	fclose (gci->file);
	g_object_unref(gci->book);
	g_slist_free_full(gci->contacts, g_object_unref);
	g_slist_free_full(gci->list_contacts, g_object_unref);
	g_hash_table_destroy(gci->dn_contact_hash);

	e_import_complete(gci->import, gci->target);
	g_object_unref(gci->import);

	g_free (gci);
}

static void
ldif_import(EImport *ei, EImportTarget *target, EImportImporter *im)
{
	LDIFImporter *gci;
	EBook *book;
	FILE *file = NULL;
	EImportTargetURI *s = (EImportTargetURI *)target;
	gchar *filename;

	book = e_book_new(g_datalist_get_data(&target->data, "ldif-source"), NULL);
	if (book == NULL) {
		g_message(G_STRLOC ":Couldn't create EBook.");
		e_import_complete(ei, target);
		return;
	}

	filename = g_filename_from_uri(s->uri_src, NULL, NULL);
	if (filename != NULL) {
/* Windows note: LDIF is essentially a line-by-textfile often with CR/LF
So we will keep ascii for now, but it's been flagged for a binary sanitizer instead ooohh.. */
		file = g_fopen(filename, "r");
		g_free (filename);
	}
	if (file == NULL) {
		g_message(G_STRLOC ":Can't open .ldif file");
		e_import_complete(ei, target);
		g_object_unref(book);
		return;
	}

	gci = g_malloc0(sizeof(*gci));
	g_datalist_set_data(&target->data, "ldif-data", gci);
	gci->import = g_object_ref(ei);
	gci->target = target;
	gci->book = book;
	gci->file = file;
	fseek(file, 0, SEEK_END);
	gci->size = ftell(file);
	fseek(file, 0, SEEK_SET);
	gci->dn_contact_hash = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) NULL);

	e_book_open(gci->book, FALSE, NULL);

	gci->idle_id = g_idle_add(ldif_import_contacts, gci);
}

static void
ldif_cancel(EImport *ei, EImportTarget *target, EImportImporter *im)
{
	LDIFImporter *gci = g_datalist_get_data(&target->data, "ldif-data");

	if (gci)
		gci->state = 2;
}

static GtkWidget *
ldif_get_preview (EImport *ei, EImportTarget *target, EImportImporter *im)
{
	GtkWidget *preview;
	GList *contacts = NULL;
	EContact *contact;
	EImportTargetURI *s = (EImportTargetURI *)target;
	gchar *filename;
	GHashTable *dn_contact_hash;
	FILE *file;

	filename = g_filename_from_uri (s->uri_src, NULL, NULL);
	if (filename == NULL) {
		g_message (G_STRLOC ": Couldn't get filename from URI '%s'", s->uri_src);
		return NULL;
	}

	file = g_fopen(filename, "r");
	g_free (filename);

	if (file == NULL) {
		g_message (G_STRLOC ": Can't open .ldif file");
		return NULL;
	}

	dn_contact_hash = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) NULL);

	while (contact = getNextLDIFEntry (dn_contact_hash, file), contact != NULL) {
		if (!e_contact_get (contact, E_CONTACT_IS_LIST)) {
			add_to_notes(contact, E_CONTACT_OFFICE);
			add_to_notes(contact, E_CONTACT_SPOUSE);
			add_to_notes(contact, E_CONTACT_BLOG_URL);
		}

		contacts = g_list_prepend (contacts, contact);
	}

	g_hash_table_destroy (dn_contact_hash);

	contacts = g_list_reverse (contacts);
	preview = evolution_contact_importer_get_preview_widget (contacts);

	g_list_free_full (contacts, g_object_unref);
	fclose (file);

	return preview;
}

static EImportImporter ldif_importer = {
	E_IMPORT_TARGET_URI,
	0,
	ldif_supported,
	ldif_getwidget,
	ldif_import,
	ldif_cancel,
	ldif_get_preview,
};

EImportImporter *
evolution_ldif_importer_peek(void)
{
	ldif_importer.name = _("LDAP Data Interchange Format (.ldif)");
	ldif_importer.description = _("Evolution LDIF importer");

	return &ldif_importer;
}
