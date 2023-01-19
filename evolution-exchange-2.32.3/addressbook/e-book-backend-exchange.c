/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Copyright (C) 2000-2004 Novell, Inc.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <fcntl.h>
#include <time.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#include <camel/camel.h>
#include <libedataserver/e-sexp.h>
#include <libedataserver/e-uid.h>
#include <libebook/e-address-western.h>
#include <libebook/e-contact.h>
#include <libedata-book/e-data-book.h>
#include <libedata-book/e-data-book-view.h>
#include <libedata-book/e-book-backend-sexp.h>
#include <libedata-book/e-book-backend-summary.h>
#include <libedata-book/e-book-backend-cache.h>
#include <libedataserver/e-xml-hash-utils.h>

#include <e2k-context.h>
#include <e2k-propnames.h>
#include <e2k-restriction.h>
#include <e2k-uri.h>
#include <e2k-utils.h>
#include <e2k-xml-utils.h>
#include <mapi.h>
#include <exchange-account.h>
#include <exchange-hierarchy.h>
#include <exchange-esource.h>
#include <e-folder-exchange.h>

#include "tools/exchange-share-config-listener.h"
#include "e-book-backend-exchange.h"

#define DEBUG
#define d(x)

#define SUMMARY_FLUSH_TIMEOUT 5000

#define PARENT_TYPE E_TYPE_BOOK_BACKEND_SYNC
static EBookBackendClass *parent_class;

struct EBookBackendExchangePrivate {
	gchar     *exchange_uri;
	gchar	 *original_uri;
	EFolder  *folder;

	E2kRestriction *base_rn;

	ExchangeAccount *account;
	E2kContext *ctx;
	gboolean connected;
	GHashTable *ops;
	gint mode;
	gboolean is_writable;
	gboolean is_cache_ready;
	gboolean marked_for_offline;

	GMutex *cache_lock;

	EBookBackendSummary *summary;
	EBookBackendCache *cache;
};

#define LOCK(x) g_mutex_lock (x->cache_lock)
#define UNLOCK(x) g_mutex_unlock (x->cache_lock)

/* vCard parameter name in contact list */
#define EEX_X_MEMBERID "X-EEX-MEMBER-ID"

#define EDB_ERROR(_code) e_data_book_create_error (E_DATA_BOOK_STATUS_ ## _code, NULL)
#define EDB_ERROR_EX(_code, _msg) e_data_book_create_error (E_DATA_BOOK_STATUS_ ## _code, _msg)

typedef struct PropMapping PropMapping;

static void subscription_notify (E2kContext *ctx, const gchar *uri, E2kContextChangeType type, gpointer user_data);
static void proppatch_address(PropMapping *prop_mapping, EContact *new_contact, EContact *cur_contact, E2kProperties *props);
static void proppatch_email(PropMapping *prop_mapping, EContact *new_contact, EContact *cur_contact, E2kProperties *props);
static void proppatch_date(PropMapping *prop_mapping, EContact *new_contact, EContact *cur_contact, E2kProperties *props);
static void populate_address(EContactField field, EContact *new_contact, gpointer data);
static void populate_date(EContactField field, EContact *new_contact, gpointer data);
static void proppatch_categories (PropMapping *prop_mapping, EContact *new_contact, EContact *cur_contact, E2kProperties *props);
static void populate_categories (EContactField field, EContact *new_contact, gpointer data);
static E2kRestriction *e_book_backend_exchange_build_restriction (const gchar *sexp,
							       E2kRestriction *base_rn);

static const gchar *contact_name (EContact *contact);
static void proppatch_im (PropMapping *prop_mapping, EContact *new_contact, EContact *cur_contact, E2kProperties *props);
static void populate_im (EContactField field, EContact *new_contact, gpointer data);

static GPtrArray *field_names_array;
static const gchar **field_names;
static gint n_field_names;

static void
http_status_to_error (E2kHTTPStatus status, GError **perror)
{
	if (E2K_HTTP_STATUS_IS_SUCCESSFUL (status))
		return;

	switch (status) {
	case E2K_HTTP_UNAUTHORIZED:
		g_propagate_error (perror, EDB_ERROR (PERMISSION_DENIED));
		break;
	case E2K_HTTP_CANT_CONNECT:
		g_propagate_error (perror, EDB_ERROR (REPOSITORY_OFFLINE));
		break;
	default:
		g_propagate_error (perror, e_data_book_create_error_fmt (E_DATA_BOOK_STATUS_OTHER_ERROR, _("Operation failed with status %d"), status));
		break;
	}
}

#define DATE_FIELD(x,z) { E_CONTACT_##x, E2K_PR_CONTACTS_##x, z, FLAG_COMPOSITE | FLAG_UNLIKEABLE, proppatch_date, populate_date }
#define EMAIL_FIELD(x,z) { E_CONTACT_##x, E2K_PR_MAPI_##x##_ADDRESS, z, FLAG_EMAIL | FLAG_COMPOSITE, proppatch_email }
#define ADDRESS_FIELD(x,z) { E_CONTACT_##x, E2K_PR_CONTACTS_##x, z, FLAG_COMPOSITE, proppatch_address, populate_address }

#define CONTACT_FIELD(x,z) { E_CONTACT_##x, E2K_PR_CONTACTS_##x, z, 0 }
#define MAPI_ID_FIELD(x,y,z) { E_CONTACT_##x, y, z, FLAG_UNLIKEABLE, NULL }
#define CAL_FIELD(x,z) { E_CONTACT_##x, E2K_PR_CALENDAR_##x, z, FLAG_UNLIKEABLE, NULL }
#define HTTPMAIL_FIELD(x,y,z) { E_CONTACT_##x, E2K_PR_HTTPMAIL_##y, z, FLAG_UNLIKEABLE | FLAG_PUT, NULL }
#define EXCHANGE_FIELD(x,y,z,w,v) { E_CONTACT_##x, E2K_PR_EXCHANGE_##y, z, FLAG_COMPOSITE, w, v }
#define IM_FIELD(x,z) { E_CONTACT_##x, E2K_PR_OUTLOOK_CONTACT_IM_ADDR, z, FLAG_COMPOSITE, proppatch_im, populate_im }

struct PropMapping {
	EContactField field;
	const gchar *prop_name;
	const gchar *e_book_field;
	gint flags;
#define FLAG_UNLIKEABLE 0x001  /* can't use LIKE with it */
#define FLAG_COMPOSITE  0x002  /* read-only fields that can be written
                                 to only by specifying the values of
                                 individual parts (as other fields) */
#define FLAG_PUT        0x020  /* requires a PUT request */
#define FLAG_EMAIL      0x100  /* email field, so we know to invoke our magic email address handling */
	void (*composite_proppatch_func)(PropMapping *prop_mapping, EContact *new_contact, EContact *cur_contact, E2kProperties *props);
	void (*composite_populate_func)(EContactField e_book_field, EContact *new_contact, gpointer data);
};

static PropMapping
prop_mappings[] = {
	CONTACT_FIELD (FULL_NAME, "full_name"),
	CONTACT_FIELD (FAMILY_NAME, "family_name"),
	CONTACT_FIELD (GIVEN_NAME, "given_name"),
/*	CONTACT_FIELD (ADDITIONAL_NAME, "middle_name"), FIXME */
/*	CONTACT_FIELD (NAME_SUFFIX, "name_suffix"), FIXME */
	CONTACT_FIELD (TITLE, "title"),
	CONTACT_FIELD (ORG, "org"),
	CONTACT_FIELD (FILE_AS, "file_as"),

	CONTACT_FIELD (PHONE_CALLBACK, "callback_phone"),
	CONTACT_FIELD (PHONE_BUSINESS_FAX, "business_fax"),
	CONTACT_FIELD (PHONE_HOME_FAX, "home_fax"),
	CONTACT_FIELD (PHONE_HOME, "home_phone"),
	CONTACT_FIELD (PHONE_HOME_2, "home_phone_2"),
	CONTACT_FIELD (PHONE_ISDN, "isdn"),
	CONTACT_FIELD (PHONE_MOBILE, "mobile_phone"),
	CONTACT_FIELD (PHONE_COMPANY, "company_phone"),
	CONTACT_FIELD (PHONE_OTHER_FAX, "other_fax"),
	CONTACT_FIELD (PHONE_PAGER, "pager"),
	CONTACT_FIELD (PHONE_BUSINESS, "business_phone"),
	CONTACT_FIELD (PHONE_BUSINESS_2, "business_phone_2"),
	CONTACT_FIELD (PHONE_TELEX, "telex"),
	CONTACT_FIELD (PHONE_TTYTDD, "tty"),
	CONTACT_FIELD (PHONE_ASSISTANT, "assistant_phone"),
	CONTACT_FIELD (PHONE_CAR, "car_phone"),
	CONTACT_FIELD (PHONE_OTHER, "other_phone"),
	MAPI_ID_FIELD (PHONE_RADIO, PR_RADIO_TELEPHONE_NUMBER, "radio"),
	MAPI_ID_FIELD (PHONE_PRIMARY, PR_PRIMARY_TELEPHONE_NUMBER, "primary_phone"),

	EMAIL_FIELD (EMAIL_1, "email_1"),
	EMAIL_FIELD (EMAIL_2, "email_2"),
	EMAIL_FIELD (EMAIL_3, "email_3"),

	ADDRESS_FIELD (ADDRESS_WORK, "business_address"),
	ADDRESS_FIELD (ADDRESS_HOME, "home_address"),
	ADDRESS_FIELD (ADDRESS_OTHER, "other_address"),

	CONTACT_FIELD (HOMEPAGE_URL, "url"),
	CONTACT_FIELD (ORG_UNIT, "org_unit"),
	CONTACT_FIELD (OFFICE, "office"),
	CONTACT_FIELD (ROLE, "role"),
	CONTACT_FIELD (MANAGER, "manager"),
	CONTACT_FIELD (ASSISTANT, "assistant"),
	CONTACT_FIELD (NICKNAME, "nickname"),
	CONTACT_FIELD (SPOUSE, "spouse"),

	DATE_FIELD (BIRTH_DATE, "birth_date"),
	DATE_FIELD (ANNIVERSARY, "anniversary"),
	CAL_FIELD (FREEBUSY_URL, "fburl"),
	IM_FIELD (IM_ICQ, "icq"),
	IM_FIELD (IM_AIM, "aim"),
	IM_FIELD (IM_MSN, "msn"),
	IM_FIELD (IM_YAHOO, "yahoo"),
	IM_FIELD (IM_JABBER, "jabber"),
	IM_FIELD (IM_GROUPWISE, "nov"),

	HTTPMAIL_FIELD (NOTE, TEXT_DESCRIPTION, "note"),

	EXCHANGE_FIELD (CATEGORIES, KEYWORDS, "categories", proppatch_categories, populate_categories)
};

struct ContactListMember
{
	gchar *memberID;
	gchar *name;
	gchar *email;
};

static const gchar *
e_book_backend_exchange_prop_to_exchange (const gchar *propname)
{
	gint i;

	for (i = 0; i < G_N_ELEMENTS (prop_mappings); i++)
		if (prop_mappings[i].e_book_field && !strcmp (prop_mappings[i].e_book_field, propname))
			return prop_mappings[i].prop_name;

	return NULL;
}

static void
get_email_field_from_props (ExchangeAccount *account,
			    PropMapping *prop_mapping, E2kResult *result,
			    EContact *contact, gchar *data)
{
	gchar *emailtype;
	const gchar *typeselector;

	/* here's where we do the super involved
	   conversion from local email addresses to
	   internet addresses for display in
	   evolution. */
	if (prop_mapping->field == E_CONTACT_EMAIL_1)
		typeselector = E2K_PR_MAPI_EMAIL_1_ADDRTYPE;
	else if (prop_mapping->field == E_CONTACT_EMAIL_2)
		typeselector = E2K_PR_MAPI_EMAIL_2_ADDRTYPE;
	else if (prop_mapping->field == E_CONTACT_EMAIL_3)
		typeselector = E2K_PR_MAPI_EMAIL_3_ADDRTYPE;
	else {
		g_warning ("invalid email field");
		return;
	}

	emailtype = e2k_properties_get_prop (result->props, typeselector);
	if (!emailtype) {
		g_warning ("no email info for \"%s\"", data);
		return;
	}
	if (!strcmp (emailtype, "SMTP")) {
		/* it's a normal email address, just set the value as we usually would */
		e_contact_set (contact, prop_mapping->field, data);
	}
	else if (!strcmp (emailtype, "EX")) {
		E2kGlobalCatalog *gc;
		E2kGlobalCatalogEntry *entry = NULL;

		gc = exchange_account_get_global_catalog (account);
		if (gc) {
			e2k_global_catalog_lookup (
				gc, NULL, /* FIXME: cancellable */
				E2K_GLOBAL_CATALOG_LOOKUP_BY_LEGACY_EXCHANGE_DN,
				data, E2K_GLOBAL_CATALOG_LOOKUP_EMAIL,
				&entry);
		}
		if (entry) {
			e_contact_set (contact, prop_mapping->field,
				       entry->email);
			e2k_global_catalog_entry_free (gc, entry);
		} else {
			g_warning ("invalid EX address");
			return;
		}
	}
	else if (emailtype[0] == '\0') {
		/* empty emailtype == no email address here */
		return;
	}
	else {
		g_warning ("email address type `%s' not handled, using the value as is", emailtype);
		e_contact_set (contact, prop_mapping->field, data);
	}

	return;
}

/* Exchange uses \r in some strings and \r\n in others. EContact always
 * wants just \n.
 */
static gchar *
unixify (const gchar *string)
{
	gchar *out = g_malloc (strlen (string) + 1), *d = out;
	const gchar *s = string;

	do {
		if (*s == '\r') {
			if (*(s + 1) != '\n')
				*d++ = '\n';
		} else
			*d++ = *s;
	} while (*s++);

	return out;
}

/**
 * Returns GSList of ContactListMember structures describing members of
 * the distribution list stored on the address list_href. The list_href
 * should end with ".EML".
 * If there are no members or any other error, the NULL is returned.
 **/
static GSList *
get_contact_list_members (E2kContext *ctx, const gchar *list_href)
{
	GSList *members = NULL;

	gchar *url;
	xmlDoc *doc;
	xmlNode *member;
	E2kHTTPStatus status;
	SoupBuffer *response = NULL;

	url = g_strconcat (list_href, "?Cmd=viewmembers", NULL);
	status = e2k_context_get_owa (ctx, NULL, url, TRUE, &response);
	g_free (url);

	/* hopefully doesn't exist only */
	if (!E2K_HTTP_STATUS_IS_SUCCESSFUL (status))
		return NULL;

	doc = e2k_parse_xml (response->data, response->length);
	soup_buffer_free (response);

	if (!doc)
		return NULL;

	member = doc->children;
	while (member = e2k_xml_find (member, "member"), member) {
		xmlNode *dn, *email, *memid;

		dn = e2k_xml_find_in (member, member, "dn");
		email = e2k_xml_find_in (member, member, "email");
		memid = e2k_xml_find_in (member, member, "memberid");
		if (email && email->children && email->children->content &&
		    memid && memid->children && memid->children->content) {
			struct ContactListMember *m;

			m = g_new0 (struct ContactListMember, 1);

			m->memberID = g_strdup ((const gchar *)memid->children->content);
			m->email = g_strdup ((const gchar *) (email->children->content));
			m->name = (dn && dn->children && dn->children->content) ? g_strdup ((const gchar *) dn->children->content) : NULL;

			/* do not pass both name and email if they are same, use only email member instead */
			if (m->name && m->email && g_str_equal (m->name, m->email)) {
				g_free (m->name);
				m->name = NULL;
			}

			members = g_slist_append (members, m);
		}
	}

	return members;
}

static void
free_contact_list_member (struct ContactListMember *member)
{
	g_return_if_fail (member != NULL);

	g_free (member->memberID);
	g_free (member->name);
	g_free (member->email);
	g_free (member);
}

/* expects list of struct ContactListMember returned from get_contact_list_members */
static void
free_members_list (GSList *list)
{
	if (!list)
		return;

	g_slist_foreach (list, (GFunc)free_contact_list_member, NULL);
	g_slist_free (list);
}

static EContact *
e_contact_from_props (EBookBackendExchange *be, E2kResult *result)
{
	EContact *contact;
	gchar *data;
	const gchar *filename;
	E2kHTTPStatus status;
	SoupBuffer *response;
	CamelStream *stream;
	CamelMimeMessage *msg;
	CamelDataWrapper *content;
	CamelMultipart *multipart;
	CamelMimePart *part;
	gint i;

	contact = e_contact_new ();

	e_contact_set (contact, E_CONTACT_UID, result->href);

	for (i = 0; i < G_N_ELEMENTS (prop_mappings); i++) {
		data = e2k_properties_get_prop (result->props,
						prop_mappings[i].prop_name);
		if (!data)
			continue;

		if (prop_mappings[i].flags & FLAG_EMAIL) {
			get_email_field_from_props (be->priv->account,
						    &prop_mappings[i],
						    result, contact, data);
		} else if (prop_mappings[i].flags & FLAG_COMPOSITE) {
			prop_mappings[i].composite_populate_func (
				prop_mappings[i].field, contact, data);
		} else {
			gchar *unix_data;

			unix_data = strchr (data, '\r') ?
				unixify (data) : data;
			e_contact_set (contact, prop_mappings[i].field,
				       unix_data);
			if (unix_data != data)
				g_free (unix_data);
		}
	}

	data = e2k_properties_get_prop (result->props, E2K_PR_DAV_CONTENT_CLASS);
	if (data && g_ascii_strcasecmp (data, "urn:content-classes:group") == 0) {
		GSList *members, *m, *attrs = NULL;

		members = get_contact_list_members (be->priv->ctx, result->href);

		/* do not ignore empty lists */
		/*if (!members) {
			g_object_unref (contact);
			return NULL;
		}*/

		/* it's a contact list/distribution list, fetch members and return it */
		e_contact_set (contact, E_CONTACT_IS_LIST, GINT_TO_POINTER (TRUE));
		/* we do not support this option, same as GroupWise */
		e_contact_set (contact, E_CONTACT_LIST_SHOW_ADDRESSES, GINT_TO_POINTER (TRUE));

		for (m = members; m; m = m->next) {
			struct ContactListMember *member = (struct ContactListMember *) m->data;
			EVCardAttribute *attr;
			gchar *value;
			CamelInternetAddress *addr;

			if (!member || !member->memberID || !member->email)
				continue;

			addr = camel_internet_address_new ();
			attr = e_vcard_attribute_new (NULL, EVC_EMAIL);
			attrs = g_slist_prepend (attrs, attr);

			camel_internet_address_add (addr, member->name, member->email);

			value = camel_address_encode (CAMEL_ADDRESS (addr));

			if (value)
				e_vcard_attribute_add_value (attr, value);

			g_free (value);
			g_object_unref (addr);

			e_vcard_attribute_add_param_with_value (attr,
					e_vcard_attribute_param_new (EEX_X_MEMBERID),
					member->memberID);
		}

		free_members_list (members);

		for (m = attrs; m; m = m->next) {
			e_vcard_add_attribute (E_VCARD (contact), m->data);
		}

		return contact;
	}

	data = e2k_properties_get_prop (result->props, E2K_PR_HTTPMAIL_HAS_ATTACHMENT);
	if (!data || !atoi(data))
		return contact;

	/* Fetch the body and parse out the photo */
	status = e2k_context_get (be->priv->ctx, NULL, result->href,
				  NULL, &response);
	if (!E2K_HTTP_STATUS_IS_SUCCESSFUL (status)) {
		g_warning ("e_contact_from_props: %d", status);
		return contact;
	}

	stream = camel_stream_mem_new_with_buffer (response->data, response->length);
	soup_buffer_free (response);
	msg = camel_mime_message_new ();
	camel_data_wrapper_construct_from_stream (CAMEL_DATA_WRAPPER (msg), stream, NULL);
	g_object_unref (stream);

	content = camel_medium_get_content (CAMEL_MEDIUM (msg));
	if (CAMEL_IS_MULTIPART (content)) {
		multipart = (CamelMultipart *)content;
		content = NULL;

		for (i = 0; i < camel_multipart_get_number (multipart); i++) {
			part = camel_multipart_get_part (multipart, i);
			filename = camel_mime_part_get_filename (part);
			if (filename && !strncmp (filename, "ContactPicture.", 15)) {
				content = camel_medium_get_content (CAMEL_MEDIUM (part));
				break;
			}
		}

		if (content) {
			EContactPhoto photo;
			GByteArray *byte_array;

			byte_array = g_byte_array_new ();
			stream = camel_stream_mem_new_with_byte_array (byte_array);
			camel_data_wrapper_decode_to_stream (content, stream, NULL);

			photo.type = E_CONTACT_PHOTO_TYPE_INLINED;
			photo.data.inlined.mime_type = NULL;
			photo.data.inlined.data = byte_array->data;
			photo.data.inlined.length = byte_array->len;
			e_contact_set (contact, E_CONTACT_PHOTO, &photo);

			g_object_unref (stream);
		}
	}

	g_object_unref (msg);
	return contact;
}

static gchar *
vcard_from_props (EBookBackendExchange *be, E2kResult *result)
{
	EContact *contact;
	gchar *vcard;

	contact = e_contact_from_props (be, result);
	if (!contact)
		return NULL;

	vcard = e_vcard_to_string (E_VCARD (contact), EVC_FORMAT_VCARD_30);
	g_object_unref (contact);

	return vcard;
}

static void
build_summary (EBookBackendExchange *be)
{
	EBookBackendExchangePrivate *bepriv = be->priv;
	E2kResultIter *iter;
	E2kResult *result;
	E2kHTTPStatus status;

	iter = e_folder_exchange_search_start (bepriv->folder, NULL,
					       field_names, n_field_names,
					       bepriv->base_rn, NULL, TRUE);
	while ((result = e2k_result_iter_next (iter))) {
		EContact *contact = e_contact_from_props (be, result);

		if (!contact) /* XXX should we error out here and destroy the summary? */
			continue;

		e_book_backend_summary_add_contact (bepriv->summary, contact);
		g_object_unref (contact);
	}
	status = e2k_result_iter_free (iter);

	if (!E2K_HTTP_STATUS_IS_SUCCESSFUL (status)) {
		g_warning ("build_summary: error building list\n");
		/* destroy the summary object here, so we don't try to query on it */
		g_object_unref (bepriv->summary);
		bepriv->summary = NULL;
		return;
	}

	e_book_backend_summary_save (bepriv->summary);
}

static const gchar *folder_props[] = {
	PR_ACCESS,
	E2K_PR_DAV_LAST_MODIFIED
};

static gpointer
build_cache (EBookBackendExchange *be)
{
	EBookBackendExchangePrivate *bepriv = be->priv;
	E2kResultIter *iter;
	E2kResult *result;
	EContact *contact;

	iter = e_folder_exchange_search_start (bepriv->folder, NULL,
				       field_names, n_field_names,
				       bepriv->base_rn, NULL, TRUE);

	LOCK (bepriv);
	e_file_cache_freeze_changes (E_FILE_CACHE (bepriv->cache));
	while ((result = e2k_result_iter_next (iter))) {
		contact = e_contact_from_props (be, result);
		if (!contact)
			continue;
		e_book_backend_cache_add_contact (bepriv->cache, contact);
		g_object_unref(contact);
	}
	e_book_backend_cache_set_populated (bepriv->cache);
	bepriv->is_cache_ready=TRUE;
	e_file_cache_thaw_changes (E_FILE_CACHE (bepriv->cache));
	UNLOCK (bepriv);
	return NULL;
}

static gboolean
update_cache (EBookBackendExchange *be)
{
	EBookBackendExchangePrivate *bepriv = be->priv;
	E2kResultIter *iter;
	E2kResult *result;
	EContact *contact;

	/* build hash table from storage file */

	/* FIXME: extract the difference and build update cache with changes */

	iter = e_folder_exchange_search_start (bepriv->folder, NULL,
				       field_names, n_field_names,
				       bepriv->base_rn, NULL, TRUE);

	LOCK (bepriv);
	e_file_cache_freeze_changes (E_FILE_CACHE (bepriv->cache));
	while ((result = e2k_result_iter_next (iter))) {
		contact = e_contact_from_props (be, result);
		if (!contact)
			continue;
		e_book_backend_cache_add_contact (bepriv->cache, contact);
		g_object_unref(contact);
	}
	e_book_backend_cache_set_populated (bepriv->cache);
	bepriv->is_cache_ready=TRUE;
	e_file_cache_thaw_changes (E_FILE_CACHE (bepriv->cache));
	UNLOCK (bepriv);
	return TRUE;
}

static gboolean
e_book_backend_exchange_connect (EBookBackendExchange *be, GError **perror)
{
	EBookBackendExchangePrivate *bepriv = be->priv;
	ExchangeHierarchy *hier;
	gchar *summary_path;
	gchar *date_prop, *access_prop;
	gint access;
	E2kResult *results;
	gint nresults = 0;
	E2kHTTPStatus status;
	time_t folder_mtime;

	if (!bepriv->account) {
		bepriv->account = exchange_share_config_listener_get_account_for_uri (NULL, bepriv->exchange_uri);
		if (!bepriv->account) {
			g_propagate_error (perror, EDB_ERROR (REPOSITORY_OFFLINE));
			return FALSE;
		}
	}
	if (!bepriv->ctx) {
		bepriv->ctx = exchange_account_get_context (bepriv->account);
		if (!bepriv->ctx) {
			g_propagate_error (perror, EDB_ERROR (REPOSITORY_OFFLINE));
			return FALSE;
		}
	}

	bepriv->folder = exchange_account_get_folder (bepriv->account, bepriv->exchange_uri);
	if (!bepriv->folder) {
		ExchangeHierarchy *hier;

		/* Rescan the hierarchy to see if any new addressbooks got added */
		hier = exchange_account_get_hierarchy_by_type (bepriv->account, EXCHANGE_HIERARCHY_PERSONAL);
		if (!hier) {
			g_propagate_error (perror, EDB_ERROR (REPOSITORY_OFFLINE));
			return FALSE;
		}
		g_object_ref (hier->toplevel);
		e_folder_exchange_set_rescan_tree (hier->toplevel, TRUE);
		exchange_hierarchy_scan_subtree (hier, hier->toplevel, ONLINE_MODE);
		e_folder_exchange_set_rescan_tree (hier->toplevel, FALSE);
		g_object_unref (hier->toplevel);

		bepriv->folder = exchange_account_get_folder (bepriv->account, bepriv->exchange_uri);
		if (!bepriv->folder) {
			g_propagate_error (perror, EDB_ERROR (REPOSITORY_OFFLINE));
			return FALSE;
		}
	}
	g_object_ref (bepriv->folder);

	/* check permissions on the folder */
	status = e_folder_exchange_propfind (bepriv->folder, NULL,
					     folder_props, G_N_ELEMENTS (folder_props),
					     &results, &nresults);

	if (status != E2K_HTTP_MULTI_STATUS) {
		bepriv->connected = FALSE;
		http_status_to_error (status, perror);
		return FALSE;
	}

	access_prop = e2k_properties_get_prop (results[0].props, PR_ACCESS);
	if (access_prop)
		access = atoi (access_prop);
	else
		access = ~0;

	if (!(access & MAPI_ACCESS_READ)) {
		bepriv->connected = FALSE;
		if (nresults)
			e2k_results_free (results, nresults);
		g_propagate_error (perror, EDB_ERROR (PERMISSION_DENIED));
		return FALSE;
	}

	bepriv->is_writable = ((access & MAPI_ACCESS_CREATE_CONTENTS) != 0);
	e_book_backend_set_is_writable (E_BOOK_BACKEND (be),
					bepriv->is_writable);
	e_book_backend_notify_writable (E_BOOK_BACKEND (be),
					bepriv->is_writable);

	bepriv->base_rn = e2k_restriction_orv (
		e2k_restriction_prop_string (E2K_PR_DAV_CONTENT_CLASS,
					     E2K_RELOP_EQ,
					     "urn:content-classes:person"),
		e2k_restriction_prop_string (E2K_PR_DAV_CONTENT_CLASS,
					     E2K_RELOP_EQ,
					     "urn:content-classes:contact"),
		e2k_restriction_prop_string (E2K_PR_DAV_CONTENT_CLASS,
					     E2K_RELOP_EQ,
					     "urn:content-classes:group"),
		NULL);

	bepriv->base_rn = e2k_restriction_andv (
		bepriv->base_rn,
		e2k_restriction_prop_bool (E2K_PR_DAV_IS_COLLECTION,
					   E2K_RELOP_EQ, FALSE),
		NULL);

	hier = e_folder_exchange_get_hierarchy (bepriv->folder);
	if (hier->hide_private_items) {
		bepriv->base_rn = e2k_restriction_andv (
			bepriv->base_rn,
			e2k_restriction_prop_int (E2K_PR_MAPI_SENSITIVITY,
						  E2K_RELOP_NE, 2),
			NULL);
	}

	/* once we're connected deal with the summary */
	date_prop = e2k_properties_get_prop (results[0].props, E2K_PR_DAV_LAST_MODIFIED);
	if (date_prop)
		folder_mtime = e2k_parse_timestamp (date_prop);

	else
		folder_mtime = 0;

	/* open the summary file */
	summary_path = e_folder_exchange_get_storage_file (bepriv->folder, "summary");

	bepriv->summary = e_book_backend_summary_new (summary_path, SUMMARY_FLUSH_TIMEOUT);
	if (e_book_backend_summary_is_up_to_date (bepriv->summary, folder_mtime) == FALSE
	    || e_book_backend_summary_load (bepriv->summary) == FALSE ) {
		/* generate the summary here */
		build_summary (be);
	}

	g_free (summary_path);

	/* now subscribe to our folder so we notice when it changes */
	e_folder_exchange_subscribe (bepriv->folder,
				     E2K_CONTEXT_OBJECT_CHANGED, 30,
				     subscription_notify, be);
	e_folder_exchange_subscribe (bepriv->folder,
				     E2K_CONTEXT_OBJECT_ADDED, 30,
				     subscription_notify, be);
	e_folder_exchange_subscribe (bepriv->folder,
				     E2K_CONTEXT_OBJECT_REMOVED, 30,
				     subscription_notify, be);

	bepriv->connected = TRUE;
	e_book_backend_set_is_loaded (E_BOOK_BACKEND (be), TRUE);
	if (nresults)
		e2k_results_free (results, nresults);

	return TRUE;
}

static gboolean
value_changed (const gchar *old, const gchar *new)
{
	if (old == NULL)
		return new != NULL;
	else if (new == NULL)
		return old != NULL;
	else
		return strcmp (old, new) != 0;
}

static const EContactField email_fields[3] = {
	E_CONTACT_EMAIL_1,
	E_CONTACT_EMAIL_2,
	E_CONTACT_EMAIL_3
};

static const gchar *email1_props[] = {
	E2K_PR_MAPI_EMAIL_1_ENTRYID,
	E2K_PR_MAPI_EMAIL_1_ADDRTYPE,
	E2K_PR_MAPI_EMAIL_1_ADDRESS,
	E2K_PR_MAPI_EMAIL_1_DISPLAY_NAME,
	E2K_PR_CONTACTS_EMAIL1,
};
static const gchar *email2_props[] = {
	E2K_PR_MAPI_EMAIL_2_ENTRYID,
	E2K_PR_MAPI_EMAIL_2_ADDRTYPE,
	E2K_PR_MAPI_EMAIL_2_ADDRESS,
	E2K_PR_MAPI_EMAIL_2_DISPLAY_NAME,
	E2K_PR_CONTACTS_EMAIL2,
};
static const gchar *email3_props[] = {
	E2K_PR_MAPI_EMAIL_3_ENTRYID,
	E2K_PR_MAPI_EMAIL_3_ADDRTYPE,
	E2K_PR_MAPI_EMAIL_3_ADDRESS,
	E2K_PR_MAPI_EMAIL_3_DISPLAY_NAME,
	E2K_PR_CONTACTS_EMAIL3,
};

static const gchar **email_props[] = {
	email1_props, email2_props, email3_props
};

static void
proppatch_email (PropMapping *prop_mapping,
		 EContact *new_contact, EContact *cur_contact,
		 E2kProperties *props)
{
	gboolean changed;
	gchar *new_email, *cur_email;
	gint field, prop, emaillisttype = 0;

	/* We do all three email addresses (plus some additional data)
	 * when invoked for E_CONTACT_EMAIL_1. So skip EMAIL_2
	 * and EMAIL_3.
	 */
	if (prop_mapping->field != E_CONTACT_EMAIL_1)
		return;

	for (field = 0; field < 3; field++) {
		new_email = e_contact_get (new_contact, email_fields[field]);
		cur_email = cur_contact ? e_contact_get (cur_contact, email_fields[field]) : NULL;

		if (new_email)
			emaillisttype |= (1 << field);

		changed = value_changed (cur_email, new_email);
		g_free (cur_email);

		if (!changed) {
			g_free (new_email);
			continue;
		}

		if (!new_email || !*new_email) {
			g_free (new_email);
			if (!cur_contact)
				continue;

			for (prop = 0; prop < 5; prop++) {
				e2k_properties_remove (
					props, email_props[field][prop]);
			}
			continue;
		}

		/* Clear originalentryid */
		e2k_properties_remove (props, email_props[field][0]);

		/* type is SMTP */
		e2k_properties_set_string (props, email_props[field][1],
					   g_strdup ("SMTP"));

		for (prop = 2; prop < 5; prop++) {
			e2k_properties_set_string (props,
						   email_props[field][prop],
						   g_strdup (new_email));
		}
		g_free (new_email);
	}

	e2k_properties_set_int (props, E2K_PR_MAPI_EMAIL_LIST_TYPE,
				emaillisttype);

	if (emaillisttype) {
		GPtrArray *fields;

		fields = g_ptr_array_new ();
		for (field = 0; field < 3; field++) {
			if (emaillisttype & (1 << field))
				g_ptr_array_add (fields, g_strdup_printf ("%d", field));
		}
		e2k_properties_set_int_array (props,
					      E2K_PR_MAPI_EMAIL_ADDRESS_LIST,
					      fields);
	} else if (cur_contact)
		e2k_properties_remove (props, E2K_PR_MAPI_EMAIL_ADDRESS_LIST);
}

static void
proppatch_categories (PropMapping *prop_mapping,
		      EContact *new_contact, EContact *cur_contact,
		      E2kProperties *props)
{
	GList *categories_list = NULL;
	GList *l = NULL;
	GPtrArray *prop_array = NULL;

	categories_list = e_contact_get (new_contact, E_CONTACT_CATEGORY_LIST);

	/* Dont send a NULL array to the server */
	if (!categories_list)
		return;

	prop_array = g_ptr_array_new ();

	for (l = categories_list; l; l = g_list_next (l)) {
		g_ptr_array_add (prop_array, g_strdup (l->data));
	}

	e2k_properties_set_string_array (props, prop_mapping->prop_name, prop_array);
}

static void
populate_categories (EContactField field, EContact *new_contact, gpointer data)
{
	GList *updated_list = NULL;
	GPtrArray *categories_list = data;
	gint i;

	for (i = 0; i < categories_list->len; i++) {
		updated_list = g_list_append (updated_list,
					      (gchar *)categories_list->pdata[i]);
	}

	e_contact_set (new_contact, E_CONTACT_CATEGORY_LIST, updated_list);

}

static void
proppatch_date (PropMapping *prop_mapping,
		EContact *new_contact, EContact *cur_contact,
		E2kProperties *props)
{
	gboolean changed;
	EContactDate *new_dt, *cur_dt;
	time_t tt;
	struct tm then;

	new_dt = e_contact_get (new_contact, prop_mapping->field);
	if (cur_contact)
		cur_dt = e_contact_get (cur_contact, prop_mapping->field);
	else
		cur_dt = NULL;

	changed = !e_contact_date_equal (cur_dt, new_dt);
	e_contact_date_free (cur_dt);

	if (!changed) {
		e_contact_date_free (new_dt);
		return;
	}

	if (new_dt) {
		memset (&then, 0, sizeof(then));
		then.tm_year = new_dt->year - 1900;
		then.tm_mon  = new_dt->month - 1;
		then.tm_mday = new_dt->day;
		then.tm_isdst = -1;
		tt = mktime (&then);

		e2k_properties_set_date (props, prop_mapping->prop_name,
					 e2k_make_timestamp (tt));
	}
	else {
		/* remove the dates set */
		e2k_properties_set_date (props, prop_mapping->prop_name,
					 e2k_make_timestamp (time (NULL)));
	}
	e_contact_date_free (new_dt);
}

static void
populate_date(EContactField field, EContact *new_contact, gpointer data)
{
	gchar *date = (gchar *)data;
	time_t tt;
	struct tm *then;
	EContactDate dt;

	tt = e2k_parse_timestamp (date);

	then = gmtime (&tt);

	dt.year = then->tm_year + 1900;
	dt.month = then->tm_mon + 1;
	dt.day = then->tm_mday;

	e_contact_set (new_contact, field, &dt);
}

enum addressprop {
	ADDRPROP_POSTOFFICEBOX,
	ADDRPROP_STREET,
	ADDRPROP_CITY,
	ADDRPROP_STATE,
	ADDRPROP_POSTALCODE,
	ADDRPROP_COUNTRY,
	ADDRPROP_LAST
};

static const gchar *homeaddrpropnames[] = {
	E2K_PR_CONTACTS_HOME_PO_BOX,
	E2K_PR_CONTACTS_HOME_STREET,
	E2K_PR_CONTACTS_HOME_CITY,
	E2K_PR_CONTACTS_HOME_STATE,
	E2K_PR_CONTACTS_HOME_ZIP,
	E2K_PR_CONTACTS_HOME_COUNTRY,
};

static const gchar *otheraddrpropnames[] = {
	E2K_PR_CONTACTS_OTHER_PO_BOX,
	E2K_PR_CONTACTS_OTHER_STREET,
	E2K_PR_CONTACTS_OTHER_CITY,
	E2K_PR_CONTACTS_OTHER_STATE,
	E2K_PR_CONTACTS_OTHER_ZIP,
	E2K_PR_CONTACTS_OTHER_COUNTRY,
};

static const gchar *workaddrpropnames[] = {
	E2K_PR_CONTACTS_WORK_PO_BOX,
	E2K_PR_CONTACTS_WORK_STREET,
	E2K_PR_CONTACTS_WORK_CITY,
	E2K_PR_CONTACTS_WORK_STATE,
	E2K_PR_CONTACTS_WORK_ZIP,
	E2K_PR_CONTACTS_WORK_COUNTRY,
};

static void
proppatch_address (PropMapping *prop_mapping,
		   EContact *new_contact, EContact *cur_contact,
		   E2kProperties *props)
{
	EContactAddress *new_address, *cur_address = NULL;
	gchar *new_addrprops[ADDRPROP_LAST], *cur_addrprops[ADDRPROP_LAST];
	const gchar **propnames;
	gchar *value;
	gint i;

	new_address = e_contact_get (new_contact, prop_mapping->field);
	if (cur_contact)
		cur_address = e_contact_get (cur_contact, prop_mapping->field);

	switch (prop_mapping->field) {
	case E_CONTACT_ADDRESS_HOME:
		propnames = homeaddrpropnames;
		break;

	case E_CONTACT_ADDRESS_WORK:
		propnames = workaddrpropnames;
		break;

	case E_CONTACT_ADDRESS_OTHER:
	default:
		propnames = otheraddrpropnames;
		break;
	}

	if (!new_address) {
		if (cur_address) {
			for (i = 0; i < ADDRPROP_LAST; i++)
				e2k_properties_remove (props, propnames[i]);
			e_contact_address_free (cur_address);
		}
		return;
	}

	new_addrprops[ADDRPROP_POSTOFFICEBOX]		= new_address->po;
	new_addrprops[ADDRPROP_STREET]			= new_address->street;
	new_addrprops[ADDRPROP_CITY]			= new_address->locality;
	new_addrprops[ADDRPROP_STATE]			= new_address->region;
	new_addrprops[ADDRPROP_POSTALCODE]		= new_address->code;
	new_addrprops[ADDRPROP_COUNTRY]		= new_address->country;
	if (cur_address) {
		cur_addrprops[ADDRPROP_POSTOFFICEBOX]	= cur_address->po;
		cur_addrprops[ADDRPROP_STREET]		= cur_address->street;
		cur_addrprops[ADDRPROP_CITY]		= cur_address->locality;
		cur_addrprops[ADDRPROP_STATE]		= cur_address->region;
		cur_addrprops[ADDRPROP_POSTALCODE]	= cur_address->code;
		cur_addrprops[ADDRPROP_COUNTRY]	= cur_address->country;
	}

	for (i = 0; i < ADDRPROP_LAST; i++) {
		if (!new_addrprops[i]) {
			if (cur_addrprops[i])
				e2k_properties_remove (props, propnames[i]);
			continue;
		}

		/* If a contact is being modified, we need to also check if the extension field has changed*/
		if (cur_address && cur_addrprops[i] &&
		    !strcmp (new_addrprops[i], cur_addrprops[i]) &&
		    !strcmp (new_address->ext, cur_address->ext)) {
			/* they're the same */
			continue;
		}

		if (i == ADDRPROP_STREET && new_address->ext) {
			value = g_strdup_printf ("%s %s", new_addrprops[i],
						 new_address->ext);
		} else
			value = g_strdup (new_addrprops[i]);
		e2k_properties_set_string (props, propnames[i], value);
	}

	e_contact_address_free (new_address);
	if (cur_address)
		e_contact_address_free (cur_address);
}

static void
proppatch_im (PropMapping *prop_mapping,
	      EContact *new_contact, EContact *cur_contact,
	      E2kProperties *props)
{
	GList *new_list;

	new_list = e_contact_get (new_contact, prop_mapping->field);
	while (new_list) {
		if (prop_mapping->field == E_CONTACT_IM_MSN) {
			e2k_properties_set_string (props,
						   E2K_PR_OUTLOOK_CONTACT_IM_ADDR,
						   g_strdup (new_list->data));
			break;
		}
		new_list = g_list_next (new_list);
	}
}

static void
populate_im (EContactField field, EContact *new_contact, gpointer data)
{
	GList *im_attr_list = NULL;
	EVCardAttribute *attr;

	if (field == E_CONTACT_IM_MSN) {
		attr = e_vcard_attribute_new ("", e_contact_vcard_attribute (field));
		e_vcard_attribute_add_param_with_value (attr,
				e_vcard_attribute_param_new (EVC_TYPE), "WORK");

		e_vcard_attribute_add_value (attr, (const gchar *)data);

		im_attr_list = g_list_append (im_attr_list, attr);
	}

	e_contact_set_attributes (new_contact, field, im_attr_list);
}

static void
populate_address (EContactField field, EContact *new_contact, gpointer data)
{
	EAddressWestern *waddr;
	EContactAddress addr;

	waddr = e_address_western_parse ((const gchar *)data);
	addr.address_format = (gchar *) "us"; /* FIXME? */
	addr.po = waddr->po_box;
	addr.ext = waddr->extended;
	addr.street = waddr->street;
	addr.locality = waddr->locality;
	addr.region = waddr->region;
	addr.code = waddr->postal_code;
	addr.country = waddr->country;

	e_contact_set (new_contact, field, &addr);
	e_address_western_free (waddr);
}

static const gchar *
contact_name (EContact *contact)
{
	const gchar *contact_name;

	contact_name = e_contact_get_const (contact, E_CONTACT_FULL_NAME);
	if (contact_name && *contact_name)
		return contact_name;
	contact_name = e_contact_get_const (contact, E_CONTACT_FILE_AS);
	if (contact_name && *contact_name)
		return contact_name;
	contact_name = e_contact_get_const (contact, E_CONTACT_EMAIL_1);
	if (contact_name && *contact_name)
		return contact_name;
	contact_name = e_contact_get_const (contact, E_CONTACT_EMAIL_2);
	if (contact_name && *contact_name)
		return contact_name;
	contact_name = e_contact_get_const (contact, E_CONTACT_EMAIL_3);
	if (contact_name && *contact_name)
		return contact_name;
	contact_name = e_contact_get_const (contact, E_CONTACT_ORG);
	if (contact_name && *contact_name)
		return contact_name;
	contact_name = e_contact_get_const (contact, E_CONTACT_TITLE);
	if (contact_name && *contact_name)
		return contact_name;

	return NULL;
}

static E2kProperties *
props_from_contact (EBookBackendExchange *be,
		    EContact *contact,
		    EContact *old_contact)
{
	E2kProperties *props;
	gint i;
	gboolean is_list = GPOINTER_TO_INT (e_contact_get (contact, E_CONTACT_IS_LIST));

	props = e2k_properties_new ();

	if (!old_contact) {
		const gchar *subject;

		subject = contact_name (contact);

		/* Set up some additional fields when creating a new contact */
		e2k_properties_set_string (
			props, E2K_PR_EXCHANGE_MESSAGE_CLASS,
			g_strdup (is_list ? "IPM.DistList" : "IPM.Contact"));

		if (is_list) {
			e2k_properties_set_string (
				props, E2K_PR_CONTACTS_FILE_AS,
				g_strdup (subject ? subject : ""));

			return props;
		}

		e2k_properties_set_string (
			props, E2K_PR_HTTPMAIL_SUBJECT,
			g_strdup (subject ? subject : ""));

		e2k_properties_set_int (props, E2K_PR_MAPI_SIDE_EFFECTS, 16);
		e2k_properties_set_int (props, PR_ACTION, 512);
		e2k_properties_set_bool (props, E2K_PR_OUTLOOK_CONTACT_JOURNAL, FALSE);
		e2k_properties_set_bool (props, E2K_PR_MAPI_SENSITIVITY, 0);
	}

	if (is_list) {
		const gchar *new_value, *current_value;

		new_value = e_contact_get_const (contact, E_CONTACT_FILE_AS);
		if (new_value && !*new_value)
			new_value = NULL;
		current_value = old_contact ? e_contact_get_const (old_contact, E_CONTACT_FILE_AS) : NULL;
		if (current_value && !*current_value)
			current_value = NULL;

		if (value_changed (current_value, new_value)) {
			if (new_value) {
				e2k_properties_set_string (
					props,
					E2K_PR_CONTACTS_FILE_AS,
					g_strdup (new_value));
			} else {
				e2k_properties_remove (
					props,
					E2K_PR_CONTACTS_FILE_AS);
			}
		}
	} else {
		for (i = 0; i < G_N_ELEMENTS (prop_mappings); i++) {
			/* handle composite attributes here (like addresses) */
			if (prop_mappings[i].flags & FLAG_COMPOSITE) {
				prop_mappings[i].composite_proppatch_func (&prop_mappings[i], contact, old_contact, props);
			} else if (prop_mappings[i].flags & FLAG_PUT) {
				continue; /* FIXME */
			} else {
				const gchar *new_value, *current_value;

				new_value = e_contact_get_const (contact, prop_mappings[i].field);
				if (new_value && !*new_value)
					new_value = NULL;
				current_value = old_contact ? e_contact_get_const (old_contact, prop_mappings[i].field) : NULL;
				if (current_value && !*current_value)
					current_value = NULL;

				if (value_changed (current_value, new_value)) {
					if (new_value) {
						e2k_properties_set_string (
							props,
							prop_mappings[i].prop_name,
							g_strdup (new_value));
					} else {
						e2k_properties_remove (
							props,
							prop_mappings[i].prop_name);
					}
				}
			}
		}
	}

	if (e2k_properties_empty (props)) {
		e2k_properties_free (props);
		return NULL;
	}

	return props;
}

static GByteArray *
build_message (const gchar *from_name, const gchar *from_email,
	       const gchar *subject, const gchar *note, EContactPhoto *photo)
{
	CamelDataWrapper *wrapper = NULL;
	CamelContentType *type;
	CamelMimeMessage *msg;
	CamelInternetAddress *from;
	CamelStream *stream;
	CamelMimePart *text_part, *photo_part;
	GByteArray *buffer;

	msg = camel_mime_message_new ();
	camel_medium_add_header (CAMEL_MEDIUM (msg), "content-class",
				 "urn:content-classes:person");
	camel_mime_message_set_subject (msg, subject);
	camel_medium_add_header (CAMEL_MEDIUM (msg), "X-MS-Has-Attach", "yes");

	from = camel_internet_address_new ();
	camel_internet_address_add (from, from_name, from_email);
	camel_mime_message_set_from (msg, from);
	g_object_unref (from);

	/* Create the body */
	if (note) {
		stream = camel_stream_mem_new_with_buffer (note, strlen (note));
		wrapper = camel_data_wrapper_new ();
		camel_data_wrapper_construct_from_stream (wrapper, stream, NULL);
		g_object_unref (stream);

		type = camel_content_type_new ("text", "plain");
		camel_content_type_set_param (type, "charset", "UTF-8");
		camel_data_wrapper_set_mime_type_field (wrapper, type);
		camel_content_type_unref (type);
	}
	text_part = NULL;

	if (note && photo)
		text_part = camel_mime_part_new ();
	else if (note)
		text_part = CAMEL_MIME_PART (msg);

	if (text_part) {
		camel_medium_set_content (CAMEL_MEDIUM (text_part), wrapper);
		camel_mime_part_set_encoding (text_part, CAMEL_TRANSFER_ENCODING_8BIT);
	}
	if (photo) {
		CamelMultipart *multipart;
		GByteArray *photo_ba;
		GdkPixbufLoader *loader;
		GdkPixbufFormat *format;
		const gchar *content_type, *extension;
		gchar **list, *filename;

		/* Determine the MIME type of the photo */
		loader = gdk_pixbuf_loader_new ();
		gdk_pixbuf_loader_write (loader, photo->data.inlined.data,
					 photo->data.inlined.length, NULL);
		gdk_pixbuf_loader_close (loader, NULL);

		format = gdk_pixbuf_loader_get_format (loader);
		g_object_unref (loader);

		if (format) {
			list = gdk_pixbuf_format_get_mime_types (format);
			content_type = list[0];
			list = gdk_pixbuf_format_get_extensions (format);
			extension = list[0];
		} else {
			content_type = "application/octet-stream";
			extension = "dat";
		}
		filename = g_strdup_printf ("ContactPicture.%s", extension);

		/* Build the MIME part */
		photo_ba = g_byte_array_new ();
		g_byte_array_append (photo_ba, photo->data.inlined.data,
				     photo->data.inlined.length);
		stream = camel_stream_mem_new_with_byte_array (photo_ba);

		wrapper = camel_data_wrapper_new ();
		camel_data_wrapper_construct_from_stream (wrapper, stream, NULL);
		g_object_unref (stream);
		camel_data_wrapper_set_mime_type (wrapper, content_type);

		photo_part = camel_mime_part_new ();
		camel_medium_set_content (CAMEL_MEDIUM (photo_part),
						 wrapper);
		camel_mime_part_set_encoding (photo_part, CAMEL_TRANSFER_ENCODING_BASE64);
		camel_mime_part_set_description (photo_part, filename);
		camel_mime_part_set_filename (photo_part, filename);

		g_free (filename);

		/* Build the multipart */
		multipart = camel_multipart_new ();
		camel_multipart_set_boundary (multipart, NULL);
		if (text_part) {
			camel_multipart_add_part (multipart, text_part);
			g_object_unref (text_part);
		}
		camel_multipart_add_part (multipart, photo_part);
		g_object_unref (photo_part);

		camel_medium_set_content (CAMEL_MEDIUM (msg),
						 CAMEL_DATA_WRAPPER (multipart));
		g_object_unref (multipart);
	}

	buffer = g_byte_array_new();
	stream = camel_stream_mem_new ();
	camel_stream_mem_set_byte_array (CAMEL_STREAM_MEM (stream), buffer);
	camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (msg), stream, NULL);
	g_object_unref (stream);
	g_object_unref (msg);

	return buffer;
}

static E2kHTTPStatus
do_put (EBookBackendExchange *be, EDataBook *book,
	const gchar *uri, const gchar *subject,
	const gchar *note, EContactPhoto *photo)
{
	ExchangeHierarchy *hier;
	E2kHTTPStatus status;
	GByteArray *body;

	hier = e_folder_exchange_get_hierarchy (be->priv->folder);
	body = build_message (hier->owner_name, hier->owner_email,
			      subject, note, photo);
	status = e2k_context_put (be->priv->ctx, NULL, uri, "message/rfc822",
				  (gchar *) body->data, body->len, NULL);
	g_byte_array_free (body, TRUE);

	return status;
}

static gboolean
test_name (E2kContext *ctx, const gchar *name, gpointer summary)
{
	return !e_book_backend_summary_check_contact (summary, name);
}

/* Posts the command of the contactlist to the backend folder; the backend should be
   already connected in time of the call to this function. */
static E2kHTTPStatus
cl_post_command (EBookBackendExchange *be, GString *cmd, const gchar *uri, gchar **location)
{
	EBookBackendExchangePrivate *bepriv;
	SoupMessage *msg;
	E2kHTTPStatus status;

	g_return_val_if_fail (be != NULL, E2K_HTTP_IO_ERROR);

	if (location)
		*location = NULL;

	bepriv = be->priv;
	msg = e2k_soup_message_new_full (bepriv->ctx, uri, "POST",
					 "application/x-www-UTF8-encoded", SOUP_MEMORY_COPY, cmd ? cmd->str : "", cmd ? cmd->len : 0);
	soup_message_set_flags (msg, SOUP_MESSAGE_NO_REDIRECT);

	status = e2k_context_send_message (bepriv->ctx, NULL, msg);

	if (location) {
		const gchar *header;

		header = soup_message_headers_get (msg->response_headers, "Location");
		*location = g_strdup (header);

		if (*location) {
			gchar *p = strrchr (*location, '?');

			/* the location can be in the form of http://server/folder/list.EML?Cmd=Edit ,
			   thus strip the Cmd param from the location */
			if (p && p > strrchr (*location, '/'))
				*p = 0;
		}
	}

	/* this is the reason why cannot use e2k_context_post */
	if (status == SOUP_STATUS_MOVED_TEMPORARILY)
		status = E2K_HTTP_OK;

	g_object_unref (msg);

	return status;
}

struct ContactListRemoveInfo
{
	EBookBackendExchange *be;
	const gchar *list_href;
};

static void
remove_member (gpointer key, struct ContactListMember *m, struct ContactListRemoveInfo *nfo)
{
	g_return_if_fail (m != NULL);
	g_return_if_fail (nfo != NULL);
	g_return_if_fail (nfo->be != NULL);
	g_return_if_fail (nfo->list_href != NULL);

	if (m->memberID) {
		GString *cmd = g_string_new ("");

		g_string_append (cmd, "Cmd=deletemember\n");
		g_string_append (cmd, "msgclass=IPM.DistList\n");
		g_string_append_printf (cmd, "memberid=%s\n", m->memberID);

		/* ignore errors here */
		cl_post_command (nfo->be, cmd, nfo->list_href, NULL);

		g_string_free (cmd, TRUE);
	}
}

static E2kHTTPStatus
merge_contact_lists (EBookBackendExchange *be, const gchar *location, EContact *contact)
{
	E2kHTTPStatus status;
	GSList *server, *s;
	GList *local, *l;
	GHashTable *sh;
	struct ContactListRemoveInfo rm;

	g_return_val_if_fail (be != NULL, E2K_HTTP_MALFORMED);
	g_return_val_if_fail (location != NULL, E2K_HTTP_MALFORMED);
	g_return_val_if_fail (contact != NULL, E2K_HTTP_MALFORMED);

	status = E2K_HTTP_OK;

	server = get_contact_list_members (be->priv->ctx, location);
	local = e_contact_get_attributes (contact, E_CONTACT_EMAIL);

	sh = g_hash_table_new (g_str_hash, g_str_equal);
	for (s = server; s; s = s->next) {
		struct ContactListMember *m = s->data;

		g_hash_table_insert (sh, m->email, m);
	}

	for (l = local; l && status == E2K_HTTP_OK; l = l->next) {
		EVCardAttribute *attr = (EVCardAttribute *) l->data;
		gchar *raw;
		CamelInternetAddress *addr;

		if (!attr)
			continue;

		raw = e_vcard_attribute_get_value (attr);
		if (!raw)
			continue;

		addr = camel_internet_address_new ();
		if (camel_address_decode (CAMEL_ADDRESS (addr), raw) > 0) {
			const gchar *nm = NULL, *eml = NULL;

			camel_internet_address_get (addr, 0, &nm, &eml);
			if (eml) {
				struct ContactListMember *on_server = g_hash_table_lookup (sh, eml);

				if (on_server) {
					/* contact list on the server already contains same member,
					   thus remove it from the hash to not clear it later */
					g_hash_table_remove (sh, eml);
				} else {
					/* it's new for the server, add it there */
					GString *cmd = g_string_new ("");

					g_string_append (cmd, "Cmd=addmember\n");
					g_string_append (cmd, "msgclass=IPM.DistList\n");
					/* can store only email itself, without the name; what a pity */
					g_string_append_printf (cmd, "member=%s\n", eml);

					status = cl_post_command (be, cmd, location, NULL);

					if (E2K_HTTP_STATUS_IS_SUCCESSFUL (status))
						status = E2K_HTTP_OK;

					g_string_free (cmd, TRUE);
				}
			}
		}
		g_object_unref (addr);
	}

	/* remove all the members from the server which left - they has been removed during editing probably */
	rm.be = be;
	rm.list_href = location;
	g_hash_table_foreach (sh, (GHFunc)remove_member, &rm);

	g_hash_table_destroy (sh);
	g_list_foreach (local, (GFunc)e_vcard_attribute_free, NULL);
	g_list_free (local);
	free_members_list (server);

	return status;
}

static void
e_book_backend_exchange_create_contact (EBookBackendSync  *backend,
					EDataBook         *book,
					guint32            opid,
					const gchar        *vcard,
					EContact         **contact,
					GError **perror)
{
	EBookBackendExchange *be = E_BOOK_BACKEND_EXCHANGE (backend);
	EBookBackendExchangePrivate *bepriv = be->priv;
	E2kProperties *props = NULL;
	const gchar *name;
	E2kHTTPStatus status;
	gchar *location = NULL;

	d(printf("ebbe_create_contact(%p, %p, %s)\n", backend, book, vcard));

	LOCK (bepriv);

	switch (bepriv->mode) {

	case E_DATA_BOOK_MODE_LOCAL:
		*contact = NULL;
		UNLOCK (bepriv);
		g_propagate_error (perror, EDB_ERROR (REPOSITORY_OFFLINE));
		return;

	case E_DATA_BOOK_MODE_REMOTE:
		*contact = e_contact_new_from_vcard (vcard);

		/* figure out the right uri to be using */
		name = contact_name (*contact);
		if (!name)
			name = "No Subject";

		if (!bepriv->connected || !bepriv->ctx || !bepriv->summary) {
			if (!e_book_backend_exchange_connect (be, perror)) {
				UNLOCK (bepriv);
				return;
			}
		}

		props = props_from_contact (be, *contact, NULL);

		status = e_folder_exchange_proppatch_new (bepriv->folder, NULL, name,
							  test_name, bepriv->summary,
							  props, &location, NULL);

		if (E2K_HTTP_STATUS_IS_SUCCESSFUL (status) && GPOINTER_TO_INT (e_contact_get (*contact, E_CONTACT_IS_LIST))) {
			e_contact_set (*contact, E_CONTACT_UID, location);
			e_contact_set (*contact, E_CONTACT_LIST_SHOW_ADDRESSES, GINT_TO_POINTER (TRUE));
			status = merge_contact_lists (be, location, *contact);
		} else if (E2K_HTTP_STATUS_IS_SUCCESSFUL (status)) {
			gchar *note;
			EContactPhoto *photo;

			e_contact_set (*contact, E_CONTACT_UID, location);

			note = e_contact_get (*contact, E_CONTACT_NOTE);
			photo = e_contact_get (*contact, E_CONTACT_PHOTO);

			if (note || photo) {
				/* Do the PUT request. */
				status = do_put (be, book, location,
						 contact_name (*contact),
						 note, photo);
			}

			if (note)
				g_free (note);
			if (photo)
				e_contact_photo_free (photo);
		}

		g_free (location);
		if (props)
			e2k_properties_free (props);

		if (E2K_HTTP_STATUS_IS_SUCCESSFUL (status)) {
			e_book_backend_summary_add_contact (bepriv->summary,
							    *contact);
			e_book_backend_cache_add_contact (bepriv->cache, *contact);
			UNLOCK (bepriv);
			return;
		} else {
			g_object_unref (*contact);
			*contact = NULL;
			UNLOCK (bepriv);
			http_status_to_error (status, perror);
			return;
		}
	default:
		break;
	}
	UNLOCK (bepriv);
}

static void
e_book_backend_exchange_modify_contact (EBookBackendSync  *backend,
					EDataBook         *book,
					guint32	  opid,
					const gchar        *vcard,
					EContact         **contact,
					GError **perror)
{
	EBookBackendExchange *be = E_BOOK_BACKEND_EXCHANGE (backend);
	EBookBackendExchangePrivate *bepriv = be->priv;
	EContact *old_contact;
	const gchar *uri;
	E2kHTTPStatus status;
	E2kResult *results;
	gint nresults = 0;
	E2kProperties *props;

	d(printf("ebbe_modify_contact(%p, %p, %s)\n", backend, book, vcard));

	switch (bepriv->mode) {

	case E_DATA_BOOK_MODE_LOCAL:
		*contact = NULL;
		g_propagate_error (perror, EDB_ERROR (REPOSITORY_OFFLINE));
		return;

	case E_DATA_BOOK_MODE_REMOTE:

		*contact = e_contact_new_from_vcard (vcard);
		uri = e_contact_get_const (*contact, E_CONTACT_UID);

		if (!bepriv->connected || !bepriv->ctx || !bepriv->summary) {
			if (!e_book_backend_exchange_connect (be, perror)) {
				return;
			}
		}

		status = e2k_context_propfind (bepriv->ctx, NULL, uri,
					       field_names, n_field_names,
					       &results, &nresults);
		if (status == E2K_HTTP_CANCELLED) {
			g_object_unref (book);
			g_object_unref (*contact);
			*contact = NULL;
			g_propagate_error (perror, EDB_ERROR_EX (OTHER_ERROR, _("Cancelled")));
			return;
		}

		if (status == E2K_HTTP_MULTI_STATUS && nresults > 0)
			old_contact = e_contact_from_props (be, &results[0]);
		else
			old_contact = NULL;

		props = props_from_contact (be, *contact, old_contact);
		if (!props)
			status = E2K_HTTP_OK;
		else
			status = e2k_context_proppatch (bepriv->ctx, NULL, uri,
							props, FALSE, NULL);

		if (E2K_HTTP_STATUS_IS_SUCCESSFUL (status) && GPOINTER_TO_INT (e_contact_get (*contact, E_CONTACT_IS_LIST))) {
			status = merge_contact_lists (be, uri, *contact);
		} else  if (E2K_HTTP_STATUS_IS_SUCCESSFUL (status)) {
			/* Do the PUT request if we need to. */
			gchar *old_note, *new_note;
			EContactPhoto *old_photo, *new_photo;
			gboolean changed = FALSE;

			old_note = e_contact_get (old_contact, E_CONTACT_NOTE);
			old_photo = e_contact_get (old_contact, E_CONTACT_PHOTO);
			new_note = e_contact_get (*contact, E_CONTACT_NOTE);
			new_photo = e_contact_get (*contact, E_CONTACT_PHOTO);

			if ((old_note && !new_note) ||
			    (new_note && !old_note) ||
			    (old_note && new_note &&
			     strcmp (old_note, new_note) != 0))
				changed = TRUE;
			else if ((old_photo && !new_photo) || (new_photo && !old_photo))
				changed = TRUE;
			else if (old_photo && new_photo) {
				if ((old_photo->type == new_photo->type) &&
				     old_photo->type == E_CONTACT_PHOTO_TYPE_INLINED) {
					changed = ((old_photo->data.inlined.length == new_photo->data.inlined.length)
							    && !memcmp (old_photo->data.inlined.data,
								new_photo->data.inlined.data,
								old_photo->data.inlined.length));
				}
				else if ((old_photo->type == new_photo->type) &&
					  old_photo->type == E_CONTACT_PHOTO_TYPE_URI) {
					changed = !strcmp (old_photo->data.uri, new_photo->data.uri);
				}
			}

			if (changed) {
				status = do_put (be, book, uri,
						 contact_name (*contact),
						 new_note, new_photo);
			}

			g_free (old_note);
			g_free (new_note);
			if (old_photo)
				e_contact_photo_free (old_photo);
			if (new_photo)
				e_contact_photo_free (new_photo);
		}

		if (old_contact)
			g_object_unref (old_contact);

		if (nresults)
			e2k_results_free (results, nresults);

		if (E2K_HTTP_STATUS_IS_SUCCESSFUL (status)) {
			LOCK (bepriv);
			e_book_backend_summary_remove_contact (bepriv->summary,
							       uri);
			e_book_backend_summary_add_contact (bepriv->summary,
							    *contact);
			e_book_backend_cache_remove_contact (bepriv->cache, uri);
			e_book_backend_cache_add_contact (bepriv->cache, *contact);
			UNLOCK (bepriv);
			return;
		} else {
			g_object_unref (*contact);
			*contact = NULL;
			http_status_to_error (status, perror);
			return;
		}

	default:
		break;
	}
}

static void
e_book_backend_exchange_remove_contacts (EBookBackendSync  *backend,
					 EDataBook         *book,
					 guint32	   opid,
					 GList             *id_list,
					 GList            **removed_ids,
					 GError           **perror)
{
	EBookBackendExchange *be = E_BOOK_BACKEND_EXCHANGE (backend);
	EBookBackendExchangePrivate *bepriv = be->priv;
	const gchar *uri;
	E2kHTTPStatus status;
	GList *l;

	 /* Remove one or more contacts */
	d(printf("ebbe_remove_contact(%p, %p, %s)\n", backend, book, (gchar *)id_list->data));

	switch (bepriv->mode) {

	case E_DATA_BOOK_MODE_LOCAL:
		*removed_ids = NULL;
		g_propagate_error (perror, EDB_ERROR (REPOSITORY_OFFLINE));
		return;

	case E_DATA_BOOK_MODE_REMOTE:

		for (l = id_list; l; l = l->next) {
			uri = l->data;
			status = e2k_context_delete (bepriv->ctx, NULL, uri);
			if (E2K_HTTP_STATUS_IS_SUCCESSFUL (status)) {
				LOCK (bepriv);
				e_book_backend_summary_remove_contact (
							bepriv->summary, uri);
				e_book_backend_cache_remove_contact (bepriv->cache, uri);
				*removed_ids = g_list_append (
						*removed_ids, g_strdup (uri));
				UNLOCK (bepriv);
			} else
				http_status_to_error (status, perror);
		}
		return;

	default:
		break;
	}
}

static ESExpResult *
func_not (ESExp *f, gint argc, ESExpResult **argv, gpointer data)
{
	ESExpResult *r;

	if (argc != 1 || argv[0]->type != ESEXP_RES_UNDEFINED) {
		e_sexp_fatal_error (f, "parse error");
		return NULL;
	}

	r = e_sexp_result_new (f, ESEXP_RES_UNDEFINED);
	r->value.string = (gchar *)
		e2k_restriction_not ((E2kRestriction *)argv[0]->value.string, TRUE);

	return r;
}

static ESExpResult *
func_and_or (ESExp *f, gint argc, ESExpResult **argv, gpointer and)
{
	ESExpResult *r;
	E2kRestriction *rn;
	GPtrArray *rns;
	gint i;

	rns = g_ptr_array_new ();

	for (i = 0; i < argc; i++) {
		if (argv[i]->type != ESEXP_RES_UNDEFINED) {
			g_ptr_array_free (rns, TRUE);
			for (i = 0; i < argc; i++) {
				if (argv[i]->type == ESEXP_RES_UNDEFINED)
					g_free (argv[i]->value.string);
			}

			e_sexp_fatal_error (f, "parse error");
			return NULL;
		}

		g_ptr_array_add (rns, argv[i]->value.string);
	}

	if (and)
		rn = e2k_restriction_and (rns->len, (E2kRestriction **)rns->pdata, TRUE);
	else
		rn = e2k_restriction_or (rns->len, (E2kRestriction **)rns->pdata, TRUE);
	g_ptr_array_free (rns, TRUE);

	r = e_sexp_result_new (f, ESEXP_RES_UNDEFINED);
	r->value.string = (gchar *)rn;
	return r;
}

static ESExpResult *
func_match (struct _ESExp *f, gint argc, struct _ESExpResult **argv, gpointer data)
{
	ESExpResult *r;
	E2kRestriction *rn;
	gchar *propname, *str;
	const gchar *exchange_prop;
	guint flags = GPOINTER_TO_UINT (data);

	if (argc != 2 ||
	    argv[0]->type != ESEXP_RES_STRING ||
	    argv[1]->type != ESEXP_RES_STRING) {
		e_sexp_fatal_error (f, "parse error");
		return NULL;
	}

	propname = argv[0]->value.string;
	str = argv[1]->value.string;

	if (!strcmp (propname, "x-evolution-any-field")) {
		GPtrArray *rns;
		gint i;

		rns = g_ptr_array_new ();
		for (i = 0; i < G_N_ELEMENTS (prop_mappings); i++) {
			if (prop_mappings[i].flags & FLAG_UNLIKEABLE)
				continue;

			exchange_prop = prop_mappings[i].prop_name;
			if (!*str)
				rn = e2k_restriction_exist (exchange_prop);
			else
				rn = e2k_restriction_content (exchange_prop, flags, str);
			g_ptr_array_add (rns, rn);
		}

		rn = e2k_restriction_or (rns->len, (E2kRestriction **)rns->pdata, TRUE);
		g_ptr_array_free (rns, TRUE);
	} else if (!strcmp (propname, "full_name")) {
		if (!*str) {
			rn = e2k_restriction_orv (
				e2k_restriction_exist (
					e_book_backend_exchange_prop_to_exchange ("full_name")),
				e2k_restriction_exist (
					e_book_backend_exchange_prop_to_exchange ("family_name")),
				NULL);
		}
		else {
			rn = e2k_restriction_orv (
				e2k_restriction_content (
					e_book_backend_exchange_prop_to_exchange ("full_name"),
					flags, str),
				e2k_restriction_content (
					e_book_backend_exchange_prop_to_exchange ("family_name"),
					flags, str),
				e2k_restriction_content (
					e_book_backend_exchange_prop_to_exchange ("nickname"),
					flags, str),
				NULL);
		}
	} else if (!strcmp (propname, "email")) {
		if (!*str) {
			rn = e2k_restriction_orv (
				e2k_restriction_exist (E2K_PR_MAPI_EMAIL_1_ADDRESS),
				e2k_restriction_exist (E2K_PR_MAPI_EMAIL_2_ADDRESS),
				e2k_restriction_exist (E2K_PR_MAPI_EMAIL_3_ADDRESS),
				NULL);
		}
		else {
			rn = e2k_restriction_orv (
				e2k_restriction_content (E2K_PR_MAPI_EMAIL_1_ADDRESS, flags, str),
				e2k_restriction_content (E2K_PR_MAPI_EMAIL_2_ADDRESS, flags, str),
				e2k_restriction_content (E2K_PR_MAPI_EMAIL_3_ADDRESS, flags, str),
				NULL);
		}
	} else {
		exchange_prop =
			e_book_backend_exchange_prop_to_exchange (propname);
		if (!exchange_prop) {
			/* FIXME. Do something better in 1.6 */
			e_sexp_fatal_error (f, "no prop");
			return NULL;
		}

		if (!*str)
			rn = e2k_restriction_exist (exchange_prop);
		else
			rn = e2k_restriction_content (exchange_prop, flags, str);
	}

	r = e_sexp_result_new (f, ESEXP_RES_UNDEFINED);
	r->value.string = (gchar *)rn;
	return r;
}

static struct {
	const gchar *name;
	ESExpFunc *func;
	guint flags;
} symbols[] = {
	{ "and", func_and_or, TRUE },
	{ "or", func_and_or, FALSE },
	{ "not", func_not, 0 },
	{ "contains", func_match, E2K_FL_SUBSTRING },
	{ "is", func_match, E2K_FL_FULLSTRING },
	{ "beginswith", func_match, E2K_FL_PREFIX },
	{ "endswith", func_match, E2K_FL_SUFFIX },
};

static E2kRestriction *
e_book_backend_exchange_build_restriction (const gchar *query,
					E2kRestriction *base_rn)
{
	ESExp *sexp;
	ESExpResult *r;
	E2kRestriction *rn;
	gint i;

	sexp = e_sexp_new ();

	for (i = 0; i < G_N_ELEMENTS (symbols); i++) {
		e_sexp_add_function (sexp, 0, (gchar *) symbols[i].name,
				     symbols[i].func,
				     GUINT_TO_POINTER (symbols[i].flags));
	}

	e_sexp_input_text (sexp, query, strlen (query));
	e_sexp_parse (sexp);

	r = e_sexp_eval (sexp);
	if (r && r->type == ESEXP_RES_UNDEFINED)
		rn = (E2kRestriction *)r->value.string;
	else
		rn = NULL;

	if (!rn)
		g_warning ("conversion to exchange restriction failed, query: '%s'", query ? query : "[null]");

	e_sexp_result_free (sexp, r);
	e_sexp_unref (sexp);

	if (base_rn && rn) {
		e2k_restriction_ref (base_rn);
		rn = e2k_restriction_andv (rn, base_rn, NULL);

		if (!rn)
			g_warning ("failed to concat with a base_rn, query: '%s'", query ? query : "[null]");
	}

	return rn;
}

static void
notify_remove (gpointer id, gpointer value, gpointer backend)
{
	EBookBackendExchange *be = backend;

	e_book_backend_notify_remove (backend, id);
	e_book_backend_summary_remove_contact (be->priv->summary, id);
}

static void
subscription_notify (E2kContext *ctx, const gchar *uri,
		     E2kContextChangeType type, gpointer user_data)
{
	EBookBackendExchange *be = user_data;
	EBookBackendExchangePrivate *bepriv = be->priv;
	EBookBackend *backend = user_data;
	GHashTable *unseen_ids;
	GPtrArray *ids;
	E2kResultIter *iter;
	E2kResult *result;
	E2kHTTPStatus status;
	EContact *contact;
	const gchar *uid;
	gint i;

	g_object_ref (be);

	unseen_ids = g_hash_table_new (g_str_hash, g_str_equal);
	ids = e_book_backend_summary_search (bepriv->summary,
					     "(contains \"x-evolution-any-field\" \"\")");
	for (i = 0; i < ids->len; i++) {
		g_hash_table_insert (unseen_ids, ids->pdata[i],
				     GUINT_TO_POINTER (1));
	}

	/* FIXME: don't search everything */

	/* execute the query */
	iter = e_folder_exchange_search_start (bepriv->folder, NULL,
					       field_names, n_field_names,
					       bepriv->base_rn, NULL, TRUE);
	while ((result = e2k_result_iter_next (iter))) {
		contact = e_contact_from_props (be, result);
		if (!contact)
			continue;
		uid = e_contact_get_const (contact, E_CONTACT_UID);

		g_hash_table_remove (unseen_ids, uid);
		e_book_backend_notify_update (backend, contact);

		e_book_backend_summary_remove_contact (bepriv->summary, uid);
		e_book_backend_summary_add_contact (bepriv->summary, contact);
		g_object_unref (contact);
	}
	status = e2k_result_iter_free (iter);

	if (status == E2K_HTTP_MULTI_STATUS)
		g_hash_table_foreach (unseen_ids, notify_remove, be);
	g_hash_table_destroy (unseen_ids);
	g_object_unref (be);
}

static void
e_book_backend_exchange_get_contact_list (EBookBackendSync  *backend,
					  EDataBook         *book,
					  guint32	     opid,
					  const gchar       *query,
					  GList            **contacts,
					  GError           **perror)
{
	EBookBackendExchange *be = E_BOOK_BACKEND_EXCHANGE (backend);
	EBookBackendExchangePrivate *bepriv = be->priv;
	E2kRestriction *rn;
	E2kResultIter *iter;
	E2kResult *result;
	E2kHTTPStatus status;
	gchar *vcard;
	GList *vcard_list = NULL, *temp, *offline_contacts;
	EBookBackendSExp *sexp = NULL;

	d(printf("ebbe_get_contact_list(%p, %p, %s)\n", backend, book, query));

	switch (bepriv->mode) {

	case E_DATA_BOOK_MODE_LOCAL:
		/* FIXME */
		offline_contacts = e_book_backend_cache_get_contacts (bepriv->cache,
							      query);
		temp = offline_contacts;
		for (; offline_contacts != NULL;
		       offline_contacts = g_list_next(offline_contacts)) {
			vcard_list = g_list_append (
					vcard_list,
					e_vcard_to_string (
						E_VCARD (offline_contacts->data),
						EVC_FORMAT_VCARD_30));
			g_object_unref (offline_contacts->data);
		}

		*contacts = vcard_list;
		if (temp)
			g_list_free (temp);
		return;

	case E_DATA_BOOK_MODE_REMOTE:

		rn = e_book_backend_exchange_build_restriction (query,
								bepriv->base_rn);

		if (!rn) {
			g_propagate_error (perror, EDB_ERROR_EX (OTHER_ERROR, "Failed to build restriction"));
			return;
		}

		iter = e_folder_exchange_search_start (bepriv->folder, NULL,
					       field_names, n_field_names,
					       rn, NULL, TRUE);

		e2k_restriction_unref (rn);

		if (query)
			sexp = e_book_backend_sexp_new (query);

		*contacts = NULL;
		while ((result = e2k_result_iter_next (iter))) {
			if (sexp) {
				EContact *contact;

				vcard = NULL;
				contact = e_contact_from_props (be, result);
				if (contact) {
					/* there is no suffix restriction, thus it's done by contains,
					   thus check for contact validity against the query is required */
					if (e_book_backend_sexp_match_contact (sexp, contact))
						vcard = e_vcard_to_string (E_VCARD (contact), EVC_FORMAT_VCARD_30);

					g_object_unref (contact);
				}
			} else {
				vcard = vcard_from_props (be, result);
			}

			if (!vcard)
				continue;

			*contacts = g_list_prepend (*contacts, vcard);
		}
		status = e2k_result_iter_free (iter);

		if (sexp)
			g_object_unref (sexp);

		http_status_to_error (status, perror);
		return;

	default:
		break;
	}
}

static void
e_book_backend_exchange_start_book_view (EBookBackend  *backend,
					 EDataBookView *book_view)
{
	EBookBackendExchange *be = E_BOOK_BACKEND_EXCHANGE (backend);
	EBookBackendExchangePrivate *bepriv = be->priv;
	const gchar *query = e_data_book_view_get_card_query (book_view);
	E2kRestriction *rn;
	E2kResultIter *iter;
	E2kResult *result;
	E2kHTTPStatus status;
	EContact *contact;
	GList *temp_list, *contacts;
	GError *err = NULL;

	d(printf("ebbe_start_book_view(%p, %p)\n", backend, book_view));

	e_data_book_view_ref (book_view);
	e_data_book_view_notify_status_message (book_view, _("Searching..."));

	switch (bepriv->mode) {

	case E_DATA_BOOK_MODE_LOCAL:
		if (!bepriv->marked_for_offline) {
			err = EDB_ERROR (OFFLINE_UNAVAILABLE);
			e_data_book_view_notify_complete (book_view, err);
			g_error_free (err);
			return;
		}
		if (!bepriv->cache) {
			e_data_book_view_notify_complete (book_view, NULL);
			return;
		}
		contacts = e_book_backend_cache_get_contacts (bepriv->cache,
							      query);
		temp_list = contacts;
		for (; contacts != NULL; contacts = g_list_next(contacts)) {
			/* FIXME: Need muex here?
			g_mutex_lock (closure->mutex);
			stopped = closure->stopped;
			g_mutex_unlock (closure->mutex);
			if (stopped)

			for (;contacts != NULL;
			      contacts = g_list_next (contacts))
				g_object_unref (contacts->data);
			break;
			}
			*/
			e_data_book_view_notify_update (book_view,
							E_CONTACT(contacts->data));
			g_object_unref (contacts->data);
		}
		//if (!stopped)
		e_data_book_view_notify_complete (book_view, NULL);
		if (temp_list)
			 g_list_free (temp_list);
		e_data_book_view_unref (book_view);
		return;

	case E_DATA_BOOK_MODE_REMOTE:

		if (!be->priv->ctx) {
			err = EDB_ERROR (AUTHENTICATION_REQUIRED);
			e_book_backend_notify_auth_required (backend);
			e_data_book_view_notify_complete (book_view, err);
			e_data_book_view_unref (book_view);
			g_error_free (err);
			return;
		}

		/* execute the query */
		rn = e_book_backend_exchange_build_restriction (query,
							bepriv->base_rn);

		if (!rn)
			return;

		iter = e_folder_exchange_search_start (bepriv->folder, NULL,
					       field_names, n_field_names,
					       rn, NULL, TRUE);
		e2k_restriction_unref (rn);

		while ((result = e2k_result_iter_next (iter))) {
			contact = e_contact_from_props (be, result);
			if (contact) {
				/* the function itself checks for validity of the contact against the query,
				   thus no need to do it here too (because of no suffix restriction) */
				e_data_book_view_notify_update (book_view,
								contact);
				g_object_unref (contact);
			}
		}
		status = e2k_result_iter_free (iter);

		http_status_to_error (status, &err);
		e_data_book_view_notify_complete (book_view, err);
		e_data_book_view_unref (book_view);
		if (err)
			g_error_free (err);

		/* also update the folder list */
		exchange_account_rescan_tree (bepriv->account);

	default:
		break;
	}
}

static void
e_book_backend_exchange_stop_book_view (EBookBackend  *backend,
				     EDataBookView *book_view)
{
	EBookBackendExchange *be = E_BOOK_BACKEND_EXCHANGE (backend);
	EBookBackendExchangePrivate *bepriv = be->priv;
	E2kOperation *op;

	d(printf("ebbe_stop_book_view(%p, %p)\n", backend, book_view));

	op = g_hash_table_lookup (bepriv->ops, book_view);
	if (op) {
		g_hash_table_remove (bepriv->ops, book_view);
		e2k_operation_cancel (op);
	}
}

typedef struct {
	EXmlHash *ehash;
	GHashTable *seen_ids;
	GList *changes;
} EBookBackendExchangeChangeContext;

static void
free_change (gpointer data, gpointer user_data)
{
	EDataBookChange *change = data;

	if (change) {
		g_free (change->vcard);
		g_free (change);
	}
}

static gboolean
find_deleted_ids (const gchar *id, const gchar *vcard, gpointer user_data)
{
	EBookBackendExchangeChangeContext *ctx = user_data;
	gboolean remove = FALSE;

	if (!g_hash_table_lookup (ctx->seen_ids, id)) {
		gchar *vcard = NULL;
		EContact *contact = e_contact_new ();
		if (contact) {
			e_contact_set (contact, E_CONTACT_UID, (gpointer) id);
			vcard = e_vcard_to_string (E_VCARD (contact), EVC_FORMAT_VCARD_30);
			if (vcard) {
				ctx->changes = g_list_prepend (
							       ctx->changes,
							       e_book_backend_change_delete_new (vcard));
				g_free (vcard);
			}
			g_object_unref (contact);
		}
		remove = TRUE;
	}
	return remove;
}

static void
e_book_backend_exchange_get_changes (EBookBackendSync  *backend,
				     EDataBook         *book,
				     guint32		opid,
				     const gchar        *change_id,
				     GList            **changes,
				     GError           **perror)
{
	EBookBackendExchange *be = E_BOOK_BACKEND_EXCHANGE (backend);
	EBookBackendExchangePrivate *bepriv = be->priv;
	EBookBackendExchangeChangeContext *ctx;
	gchar *filename, *path, *vcard;
	E2kResultIter *iter;
	E2kResult *result;
	E2kHTTPStatus status;

	d(printf("ebbe_get_changes(%p, %p, %s)\n", backend, book, change_id));

	switch (bepriv->mode) {

	case E_DATA_BOOK_MODE_LOCAL:
		*changes = NULL;
		g_propagate_error (perror, EDB_ERROR (REPOSITORY_OFFLINE));
		return;

	case E_DATA_BOOK_MODE_REMOTE:

		ctx = g_new0 (EBookBackendExchangeChangeContext, 1);
		ctx->seen_ids = g_hash_table_new_full (g_str_hash, g_str_equal,
					       g_free, NULL);

		/* open the changes file */
		filename = g_strdup_printf ("%s.changes", change_id);
		path = e_folder_exchange_get_storage_file (bepriv->folder,
							   filename);
		ctx->ehash = e_xmlhash_new (path);
		g_free (path);
		g_free (filename);

		iter = e_folder_exchange_search_start (bepriv->folder, NULL,
					       field_names, n_field_names,
					       bepriv->base_rn, NULL, TRUE);

		while ((result = e2k_result_iter_next (iter))) {
			vcard = vcard_from_props (be, result);
			if (!vcard)
				continue;

			g_hash_table_insert (ctx->seen_ids,
					     g_strdup (result->href),
					     GINT_TO_POINTER (1));

			/* Check what type of change has occurred, if any. */
			switch (e_xmlhash_compare (ctx->ehash, result->href,
						   vcard)) {
			case E_XMLHASH_STATUS_SAME:
				break;

			case E_XMLHASH_STATUS_NOT_FOUND:
				e_xmlhash_add (ctx->ehash, result->href, vcard);
				ctx->changes = g_list_prepend (
					ctx->changes,
					e_book_backend_change_add_new (vcard));
				break;

			case E_XMLHASH_STATUS_DIFFERENT:
				e_xmlhash_add (ctx->ehash, result->href, vcard);
				ctx->changes = g_list_prepend (
					ctx->changes,
					e_book_backend_change_modify_new (vcard));
				break;
			}

			g_free (vcard);
		}
		status = e2k_result_iter_free (iter);

		if (!E2K_HTTP_STATUS_IS_SUCCESSFUL (status)) {
			g_warning ("e_book_backend_exchange_changes: error building list (err = %d)\n", status);
			g_list_foreach (ctx->changes, free_change, NULL);
			ctx->changes = NULL;
		} else {
			e_xmlhash_foreach_key_remove (ctx->ehash, find_deleted_ids, ctx);
			e_xmlhash_write (ctx->ehash);
		}

		/* transfer ownership of result to caller before cleaning up */
		*changes = ctx->changes;
		ctx->changes = NULL;

		e_xmlhash_destroy (ctx->ehash);
		g_hash_table_destroy (ctx->seen_ids);
		g_free (ctx);

		http_status_to_error (status, perror);
		return;

	default:
		break;
	}
}

static void
e_book_backend_exchange_get_contact (EBookBackendSync  *backend,
				     EDataBook         *book,
				     guint32            opid,
				     const gchar        *id,
				     gchar             **vcard,
				     GError            **perror)
{
	EBookBackendExchange *be = E_BOOK_BACKEND_EXCHANGE (backend);
	EBookBackendExchangePrivate *bepriv = be->priv;
	EContact *contact;
	E2kResult *results=NULL;
	E2kHTTPStatus status;
	gint nresults = 0;

	d(printf("ebbe_get_contact(%p, %p, %s)\n", backend, book, id));

	be = E_BOOK_BACKEND_EXCHANGE (e_data_book_get_backend (book));

	switch (bepriv->mode) {

	case E_DATA_BOOK_MODE_LOCAL:
		contact = e_book_backend_cache_get_contact (bepriv->cache,
							    id);
		if (contact) {
			*vcard =  e_vcard_to_string (E_VCARD (contact),
						     EVC_FORMAT_VCARD_30);
			g_object_unref (contact);
			return;
		}
		else {
			*vcard = g_strdup ("");
			g_propagate_error (perror, EDB_ERROR (CONTACT_NOT_FOUND));
			return;
		}

	case E_DATA_BOOK_MODE_REMOTE:

		if (bepriv->marked_for_offline && e_book_backend_cache_is_populated (bepriv->cache)) {
			contact = e_book_backend_cache_get_contact (bepriv->cache,
								    id);
			if (contact) {
				*vcard =  e_vcard_to_string (E_VCARD (contact),
							     EVC_FORMAT_VCARD_30);
				g_object_unref (contact);
				return;
			}
			else {
				*vcard = g_strdup ("");
				g_propagate_error (perror, EDB_ERROR (CONTACT_NOT_FOUND));
				return;
			}

		} else {
			E2kUri *uri;

			/* Dont allow any non uri to go GAL UID is not a uri */
			uri = e2k_uri_new (id);
			if (!uri->protocol ||  !*uri->protocol) {
				e2k_uri_free (uri);
				*vcard = g_strdup ("");
				g_propagate_error (perror, EDB_ERROR (CONTACT_NOT_FOUND));
				return;
			}
			e2k_uri_free (uri);

			status = e2k_context_propfind (bepriv->ctx, NULL, id,
						       field_names, n_field_names,
						       &results, &nresults);

			if (status == E2K_HTTP_CANCELLED) {
				g_propagate_error (perror, EDB_ERROR_EX (OTHER_ERROR, _("Cancelled")));
				return;
			}

			if (status == E2K_HTTP_MULTI_STATUS && nresults > 0) {
				contact = e_contact_from_props (be, &results[0]);
				*vcard =  e_vcard_to_string (E_VCARD (contact),
							     EVC_FORMAT_VCARD_30);
				g_object_unref (contact);
				e2k_results_free (results, nresults);

				return;

			} else {
				*vcard = g_strdup ("");

				g_propagate_error (perror, EDB_ERROR (CONTACT_NOT_FOUND));
				return;
			}
		}

	default:
		break;
	}

	g_propagate_error (perror, EDB_ERROR (OTHER_ERROR));
}

static void
e_book_backend_exchange_authenticate_user (EBookBackend *backend,
					   EDataBook        *book,
					   guint32	     opid,
					   const gchar       *user,
					   const gchar       *password,
					   const gchar       *auth_method)
{
	EBookBackendExchange *be = E_BOOK_BACKEND_EXCHANGE (backend);
	EBookBackendExchangePrivate *bepriv = be->priv;
	ExchangeAccountResult result;
	ExchangeAccount *account = NULL;

	d(printf("ebbe_authenticate_user(%p, %p, %s, %s, %s)\n", backend, book, user, password, auth_method));

	switch (bepriv->mode) {

	case E_DATA_BOOK_MODE_LOCAL:
		e_book_backend_notify_writable (E_BOOK_BACKEND (backend), FALSE);
		e_book_backend_notify_connection_status (E_BOOK_BACKEND (backend), FALSE);
		e_data_book_respond_authenticate_user (book, opid, NULL);
		return;

	case E_DATA_BOOK_MODE_REMOTE:

		bepriv->account = account = exchange_share_config_listener_get_account_for_uri (NULL, bepriv->exchange_uri);
		/* FIXME : Check for failures */
		if (!(bepriv->ctx = exchange_account_get_context (account))) {
			exchange_account_set_online (account);
			if (!exchange_account_connect (account, password, &result)) {
				e_data_book_respond_authenticate_user (book, opid, EDB_ERROR (AUTHENTICATION_FAILED));
				return;
			}
		}
		if (!bepriv->connected)
			e_book_backend_exchange_connect (be, NULL);
		if (e_book_backend_cache_is_populated (bepriv->cache)) {
			if (bepriv->is_writable)
				g_thread_create ((GThreadFunc) update_cache,
						  be, FALSE, NULL);
		}
		else if (bepriv->is_writable || bepriv->marked_for_offline) {
			/* for personal books we always cache*/
			g_thread_create ((GThreadFunc) build_cache, be, FALSE, NULL);
		}
		e_data_book_respond_authenticate_user (book, opid, NULL);
		return;

	default:
		break;
	}
	e_data_book_respond_authenticate_user (book, opid, NULL);
}

static void
e_book_backend_exchange_get_supported_auth_methods (EBookBackend *backend,
						    EDataBook *book,
						    guint32 opid)
{
	GList *auth_methods = NULL;
	gchar *auth_method;

	d(printf ("ebbe_get_supported_auth_methods (%p, %p)\n", backend, book));

	auth_method = g_strdup_printf ("plain/password");
	auth_methods = g_list_append (auth_methods, auth_method);
	e_data_book_respond_get_supported_auth_methods (book, opid,
				NULL,
				auth_methods);

	g_free (auth_method);
	g_list_free (auth_methods);
}

static void
e_book_backend_exchange_get_supported_fields (EBookBackendSync  *backend,
					      EDataBook         *book,
					      guint32		 opid,
					      GList            **methods,
					      GError           **perror)
{
	gint i;

	d(printf("ebbe_get_supported_fields(%p, %p)\n", backend, book));

	*methods = NULL;
	for (i = 0; i < G_N_ELEMENTS (prop_mappings); i++) {
		if (prop_mappings[i].e_book_field) {
			*methods = g_list_prepend (*methods,
					g_strdup (e_contact_field_name(prop_mappings[i].field)));
		}
	}
}

static void
e_book_backend_exchange_get_required_fields (EBookBackendSync *backend,
					  EDataBook *book,
					  guint32 opid,
					  GList **fields_out,
					  GError **perror)
{
	GList *fields = NULL;

	fields = g_list_append (fields, g_strdup (e_contact_field_name (E_CONTACT_FILE_AS)));
	*fields_out = fields;

}

static void
e_book_backend_exchange_cancel_operation (EBookBackend *backend, EDataBook *book, GError **perror)
{
	EBookBackendExchange *be = E_BOOK_BACKEND_EXCHANGE (backend);
	EBookBackendExchangePrivate *bepriv = be->priv;
	E2kOperation *op;

	d(printf("ebbe_cancel_operation(%p, %p)\n", backend, book));

	op = g_hash_table_lookup (bepriv->ops, book);
	if (op) {
		e2k_operation_cancel (op);
	} else {
		g_propagate_error (perror, EDB_ERROR (COULD_NOT_CANCEL));
	}
}

static void
e_book_backend_exchange_load_source (EBookBackend *backend,
				     ESource      *source,
				     gboolean      only_if_exists,
				     GError      **error)
{
	EBookBackendExchange *be = E_BOOK_BACKEND_EXCHANGE (backend);
	EBookBackendExchangePrivate *bepriv = be->priv;
	const gchar *cache_dir;
	const gchar *offline;
	gchar *filename;

	e_return_data_book_error_if_fail (bepriv->connected == FALSE, E_DATA_BOOK_STATUS_OTHER_ERROR);

	d(printf("ebbe_load_source(%p, %p[%s])\n", backend, source, e_source_peek_name (source)));

	cache_dir = e_book_backend_get_cache_dir (backend);

	offline = e_source_get_property (source, "offline_sync");
	if (offline  && g_str_equal (offline, "1"))
		bepriv->marked_for_offline = TRUE;

	if (bepriv->mode ==  E_DATA_BOOK_MODE_LOCAL &&
	    !bepriv->marked_for_offline ) {
		g_propagate_error (error, EDB_ERROR (OFFLINE_UNAVAILABLE));
		return;
	}

	bepriv->exchange_uri = e_source_get_uri (source);
	if (bepriv->exchange_uri == NULL) {
		g_propagate_error (error, EDB_ERROR_EX (OTHER_ERROR, "Cannot get source's URI"));
		return;
	}
	bepriv->original_uri = g_strdup (bepriv->exchange_uri);

	filename = g_build_filename (cache_dir, "cache.xml", NULL);

	if (bepriv->mode == E_DATA_BOOK_MODE_LOCAL) {
		e_book_backend_set_is_writable (backend, FALSE);
		e_book_backend_notify_writable (backend, FALSE);
		e_book_backend_notify_connection_status (backend, FALSE);
		if (!g_file_test (filename, G_FILE_TEST_IS_REGULAR)) {
			g_propagate_error (error, EDB_ERROR (OFFLINE_UNAVAILABLE));
			g_free (filename);
			return;
		}
	}

	bepriv->cache = e_book_backend_cache_new (filename);

	g_free (filename);

	/* Once aunthentication in address book works this can be removed */
	if (bepriv->mode == E_DATA_BOOK_MODE_LOCAL) {
		return;
	}

	// writable property will be set in authenticate_user callback
	e_book_backend_set_is_writable (E_BOOK_BACKEND(backend), FALSE);
	e_book_backend_set_is_loaded (E_BOOK_BACKEND (be), TRUE);
	e_book_backend_notify_connection_status (E_BOOK_BACKEND (be), TRUE);
}

static void
e_book_backend_exchange_remove (EBookBackendSync *backend, EDataBook *book, guint32 opid, GError **perror)
{
	EBookBackendExchange *be = E_BOOK_BACKEND_EXCHANGE (backend);
	EBookBackendExchangePrivate *bepriv = be->priv;
	ExchangeAccountFolderResult result = EXCHANGE_ACCOUNT_FOLDER_GENERIC_ERROR;
	const gchar *int_uri = NULL;

	d(printf("ebbe_remove(%p, %p)\n", backend, book));
	int_uri = e_folder_exchange_get_internal_uri (bepriv->folder);
	if (int_uri)
		result = exchange_account_remove_folder (bepriv->account, int_uri);
	else {
		ExchangeAccount *account = NULL;

		/* internal URI is NULL, mostly the case where folder doesn't exists anymore
		 * in the server, update the gconf sources.
		 */
		account = exchange_share_config_listener_get_account_for_uri (NULL, bepriv->exchange_uri);
		if (exchange_account_get_context (account)) {
			remove_folder_esource (account, EXCHANGE_CONTACTS_FOLDER, bepriv->exchange_uri);
			result = EXCHANGE_ACCOUNT_FOLDER_OK;
		}
	}
	if (result == EXCHANGE_ACCOUNT_FOLDER_OK)
		return;
	else if (result == EXCHANGE_ACCOUNT_FOLDER_DOES_NOT_EXIST)
		g_propagate_error (perror, EDB_ERROR (NO_SUCH_BOOK));
	else if (result == EXCHANGE_ACCOUNT_FOLDER_UNSUPPORTED_OPERATION)
		g_propagate_error (perror, EDB_ERROR (PERMISSION_DENIED));
	else if (result == EXCHANGE_ACCOUNT_FOLDER_OFFLINE)
		g_propagate_error (perror, EDB_ERROR (OFFLINE_UNAVAILABLE));
	else if (result == EXCHANGE_ACCOUNT_FOLDER_PERMISSION_DENIED)
		g_propagate_error (perror, EDB_ERROR (PERMISSION_DENIED));
	else
		g_propagate_error (perror, e_data_book_create_error_fmt (E_DATA_BOOK_STATUS_OTHER_ERROR, "Failed with result code %d", result));
}

static gchar *
e_book_backend_exchange_get_static_capabilites (EBookBackend *backend)
{
	return g_strdup ("net,bulk-removes,do-initial-query,cache-completions,contact-lists");
}

static void
e_book_backend_exchange_set_mode (EBookBackend *backend,
                                  EDataBookMode mode)
{
	EBookBackendExchange *be = E_BOOK_BACKEND_EXCHANGE (backend);
	EBookBackendExchangePrivate *bepriv = be->priv;
	ExchangeAccount *account = NULL;

	bepriv->mode = mode;
	/* if (e_book_backend_is_loaded (backend)) { */
	if (mode == E_DATA_BOOK_MODE_LOCAL) {
		e_book_backend_set_is_writable (backend, FALSE);
		e_book_backend_notify_writable (backend, FALSE);
		e_book_backend_notify_connection_status (backend, FALSE);
		/* FIXME : free context ? */
	} else if (mode == E_DATA_BOOK_MODE_REMOTE) {
		e_book_backend_set_is_writable (backend, bepriv->is_writable);
		e_book_backend_notify_writable (backend, bepriv->is_writable);
		e_book_backend_notify_connection_status (backend, TRUE);
		account = exchange_share_config_listener_get_account_for_uri (NULL, bepriv->exchange_uri);
		if (!exchange_account_get_context (account))
			e_book_backend_notify_auth_required (backend);
	}
	/*}*/
}

/**
 * e_book_backend_exchange_new:
 *
 * Creates a new #EBookBackendExchange.
 *
 * Return value: the new #EBookBackendExchange.
 */
EBookBackend *
e_book_backend_exchange_new (void)
{
	exchange_share_config_listener_get_account_for_uri (NULL, NULL);

	return g_object_new (E_TYPE_BOOK_BACKEND_EXCHANGE, NULL);
}

static void
e_book_backend_exchange_dispose (GObject *object)
{
	EBookBackendExchange *be;

	be = E_BOOK_BACKEND_EXCHANGE (object);

	if (be->priv) {
		if (be->priv->folder) {
			e_folder_exchange_unsubscribe (be->priv->folder);
			g_object_unref (be->priv->folder);
		}

		if (be->priv->exchange_uri)
			g_free (be->priv->exchange_uri);

		if (be->priv->original_uri)
			g_free (be->priv->original_uri);

		if (be->priv->account)
			be->priv->account = NULL;

		if (be->priv->ops)
			g_hash_table_destroy (be->priv->ops);

		if (be->priv->cache)
			g_object_unref (be->priv->cache);

		if (be->priv->cache_lock)
			g_mutex_free (be->priv->cache_lock);

		g_free (be->priv);
		be->priv = NULL;
	}

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
e_book_backend_exchange_class_init (EBookBackendExchangeClass *klass)
{
	GObjectClass  *object_class = (GObjectClass *) klass;
	EBookBackendClass *backend_class = E_BOOK_BACKEND_CLASS (klass);
	EBookBackendSyncClass *sync_class = E_BOOK_BACKEND_SYNC_CLASS (klass);
	gint i;

	parent_class = g_type_class_ref (e_book_backend_get_type ());

	/* Static initialization */
	field_names_array = g_ptr_array_new ();
	g_ptr_array_add (field_names_array, (gpointer) E2K_PR_DAV_CONTENT_CLASS);
	g_ptr_array_add (field_names_array, (gpointer) E2K_PR_DAV_UID);
	g_ptr_array_add (field_names_array, (gpointer) E2K_PR_DAV_LAST_MODIFIED);
	g_ptr_array_add (field_names_array, (gpointer) E2K_PR_DAV_CREATION_DATE);
	g_ptr_array_add (field_names_array, (gpointer) E2K_PR_MAPI_EMAIL_1_ADDRTYPE);
	g_ptr_array_add (field_names_array, (gpointer) E2K_PR_MAPI_EMAIL_2_ADDRTYPE);
	g_ptr_array_add (field_names_array, (gpointer) E2K_PR_MAPI_EMAIL_3_ADDRTYPE);
	g_ptr_array_add (field_names_array, (gpointer) E2K_PR_HTTPMAIL_HAS_ATTACHMENT);
	for (i = 0; i < G_N_ELEMENTS (prop_mappings); i++)
		g_ptr_array_add (field_names_array, (gpointer) prop_mappings[i].prop_name);
	field_names = (const gchar **)field_names_array->pdata;
	n_field_names = field_names_array->len;

	/* Set the virtual methods. */
	backend_class->load_source             = e_book_backend_exchange_load_source;
	backend_class->get_static_capabilities = e_book_backend_exchange_get_static_capabilites;
	backend_class->start_book_view         = e_book_backend_exchange_start_book_view;
	backend_class->stop_book_view          = e_book_backend_exchange_stop_book_view;
	backend_class->cancel_operation        = e_book_backend_exchange_cancel_operation;
	backend_class->set_mode			= e_book_backend_exchange_set_mode;
	backend_class->get_supported_auth_methods = e_book_backend_exchange_get_supported_auth_methods;
	backend_class->authenticate_user     = e_book_backend_exchange_authenticate_user;

	sync_class->remove_sync                = e_book_backend_exchange_remove;
	sync_class->create_contact_sync        = e_book_backend_exchange_create_contact;
	sync_class->remove_contacts_sync       = e_book_backend_exchange_remove_contacts;
	sync_class->modify_contact_sync        = e_book_backend_exchange_modify_contact;
	sync_class->get_contact_sync           = e_book_backend_exchange_get_contact;
	sync_class->get_contact_list_sync      = e_book_backend_exchange_get_contact_list;
	sync_class->get_changes_sync           = e_book_backend_exchange_get_changes;
	sync_class->get_supported_fields_sync  = e_book_backend_exchange_get_supported_fields;
	sync_class->get_required_fields_sync   = e_book_backend_exchange_get_required_fields;

	object_class->dispose = e_book_backend_exchange_dispose;
}

static void
e_book_backend_exchange_init (EBookBackendExchange *backend)
{
	EBookBackendExchangePrivate *priv;

	priv		= g_new0 (EBookBackendExchangePrivate, 1);
	priv->ops	= g_hash_table_new (NULL, NULL);
	priv->is_cache_ready	= FALSE;
	priv->marked_for_offline= FALSE;
	priv->cache		= NULL;
	priv->original_uri	= NULL;
	priv->is_writable	= TRUE;

	priv->cache_lock      = g_mutex_new ();

	backend->priv		= priv;
}

E2K_MAKE_TYPE (e_book_backend_exchange, EBookBackendExchange, e_book_backend_exchange_class_init, e_book_backend_exchange_init, PARENT_TYPE)

