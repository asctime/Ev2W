/* e-book-backend-webdav.c - Webdav contact backend.
 *
 * Copyright (C) 2008 Matthias Braun <matze@braunis.de>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Author: Matthias Braun <matze@braunis.de>
 */

/*
 * Implementation notes:
 *   We use the DavResource URIs as UID in the evolution contact
 *   ETags are saved in the E_CONTACT_REV field so we know which cached contacts
 *   are outdated.
 */
#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <glib/gi18n-lib.h>

#include <libedataserver/e-url.h>
#include <libedataserver/e-flag.h>
#include <libedataserver/e-proxy.h>
#include <libebook/e-contact.h>
#include <libebook/e-address-western.h>

#include <libedata-book/e-book-backend-sexp.h>
#include <libedata-book/e-book-backend-summary.h>
#include <libedata-book/e-data-book.h>
#include <libedata-book/e-data-book-view.h>
#include <libedata-book/e-book-backend-cache.h>
#include "e-book-backend-webdav.h"

#include <libsoup/soup.h>

#include <libxml/parser.h>
#include <libxml/xmlreader.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#define EDB_ERROR(_code) e_data_book_create_error (E_DATA_BOOK_STATUS_ ## _code, NULL)
#define EDB_ERROR_EX(_code, _msg) e_data_book_create_error (E_DATA_BOOK_STATUS_ ## _code, _msg)

#define USERAGENT             "Evolution/" VERSION
#define WEBDAV_CLOSURE_NAME   "EBookBackendWebdav.BookView::closure"
#define WEBDAV_CTAG_KEY "WEBDAV_CTAG"

G_DEFINE_TYPE (EBookBackendWebdav, e_book_backend_webdav, E_TYPE_BOOK_BACKEND)

static EBookBackendClass *parent_class;

struct _EBookBackendWebdavPrivate {
	gint                mode;
	gboolean           marked_for_offline;
	SoupSession       *session;
	EProxy		  *proxy;
	gchar             *uri;
	gchar              *username;
	gchar              *password;
	gboolean supports_getctag;

	EBookBackendCache *cache;
};

typedef struct {
	EBookBackendWebdav *webdav;
	GThread            *thread;
	EFlag              *running;
} WebdavBackendSearchClosure;

static void
webdav_debug_setup (SoupSession *session)
{
	const gchar *debug_str;
	SoupLogger *logger;
	SoupLoggerLogLevel level;

	g_return_if_fail (session != NULL);

	debug_str = g_getenv ("WEBDAV_DEBUG");
	if (!debug_str || !*debug_str)
		return;

	if (g_ascii_strcasecmp (debug_str, "all") == 0)
		level = SOUP_LOGGER_LOG_BODY;
	else if (g_ascii_strcasecmp (debug_str, "headers") == 0)
		level = SOUP_LOGGER_LOG_HEADERS;
	else
		level = SOUP_LOGGER_LOG_MINIMAL;

	logger = soup_logger_new (level, 100 * 1024 * 1024);
	soup_session_add_feature (session, SOUP_SESSION_FEATURE (logger));
	g_object_unref (logger);
}

static void
closure_destroy(WebdavBackendSearchClosure *closure)
{
	e_flag_free(closure->running);
	g_free(closure);
}

static WebdavBackendSearchClosure*
init_closure(EDataBookView *book_view, EBookBackendWebdav *webdav)
{
	WebdavBackendSearchClosure *closure = g_new (WebdavBackendSearchClosure, 1);

	closure->webdav  = webdav;
	closure->thread  = NULL;
	closure->running = e_flag_new();

	g_object_set_data_full(G_OBJECT(book_view), WEBDAV_CLOSURE_NAME, closure,
			(GDestroyNotify)closure_destroy);

	return closure;
}

static WebdavBackendSearchClosure*
get_closure(EDataBookView *book_view)
{
	return g_object_get_data(G_OBJECT(book_view), WEBDAV_CLOSURE_NAME);
}

static EContact*
download_contact(EBookBackendWebdav *webdav, const gchar *uri)
{
	SoupMessage *message;
	const gchar  *etag;
	EContact    *contact;
	guint        status;

	message = soup_message_new(SOUP_METHOD_GET, uri);
	soup_message_headers_append (message->request_headers, "User-Agent", USERAGENT);
	soup_message_headers_append (message->request_headers, "Connection", "close");

	status = soup_session_send_message(webdav->priv->session, message);
	if (status != 200) {
		g_warning("Couldn't load '%s' (http status %d)", uri, status);
		g_object_unref(message);
		return NULL;
	}

	if (message->response_body == NULL) {
		g_message("no response body after requesting '%s'", uri);
		g_object_unref(message);
		return NULL;
	}

	if (message->response_body->length <= 11 || 0 != g_ascii_strncasecmp ((const gchar *) message->response_body->data, "BEGIN:VCARD", 11)) {
		g_object_unref(message);
		return NULL;
	}

	etag = soup_message_headers_get(message->response_headers, "ETag");

	contact = e_contact_new_from_vcard(message->response_body->data);
	if (contact == NULL) {
		g_warning("Invalid vcard at '%s'", uri);
		g_object_unref(message);
		return NULL;
	}

	/* we use our URI as UID
	 * the etag is rememebered in the revision field */
	e_contact_set(contact, E_CONTACT_UID, (gconstpointer) uri);
	if (etag != NULL) {
		e_contact_set(contact, E_CONTACT_REV, (gconstpointer) etag);
	}

	g_object_unref(message);
	return contact;
}

static guint
upload_contact(EBookBackendWebdav *webdav, EContact *contact)
{
	ESource     *source = e_book_backend_get_source(E_BOOK_BACKEND(webdav));
	SoupMessage *message;
	gchar       *uri;
	gchar       *etag;
	const gchar  *new_etag, *redir_uri;
	gchar        *request;
	guint        status;
	const gchar  *property;
	gboolean     avoid_ifmatch;

	uri = e_contact_get(contact, E_CONTACT_UID);
	if (uri == NULL) {
		g_warning("can't upload contact without UID");
		return 400;
	}

	message = soup_message_new(SOUP_METHOD_PUT, uri);
	soup_message_headers_append (message->request_headers, "User-Agent", USERAGENT);
	soup_message_headers_append (message->request_headers, "Connection", "close");

	property = e_source_get_property(source, "avoid_ifmatch");
	if (property != NULL && strcmp(property, "1") == 0) {
		avoid_ifmatch = TRUE;
	} else {
		avoid_ifmatch = FALSE;
	}

	/* some servers (like apache < 2.2.8) don't handle If-Match, correctly so
	 * we can leave it out */
	if (!avoid_ifmatch) {
		/* only override if etag is still the same on the server */
		etag = e_contact_get(contact, E_CONTACT_REV);
		if (etag == NULL) {
			soup_message_headers_append(message->request_headers,
						    "If-None-Match", "*");
		} else if (etag[0] == 'W' && etag[1] == '/') {
			g_warning("we only have a weak ETag, don't use If-Match synchronisation");
		} else {
			soup_message_headers_append(message->request_headers,
						    "If-Match", etag);
			g_free(etag);
		}
	}

	/* don't upload the UID and REV fields, they're only interesting inside
	 * evolution and not on the webdav server */
	e_contact_set(contact, E_CONTACT_UID, NULL);
	e_contact_set(contact, E_CONTACT_REV, NULL);
	request = e_vcard_to_string(E_VCARD(contact), EVC_FORMAT_VCARD_30);
	soup_message_set_request(message, "text/vcard", SOUP_MEMORY_TEMPORARY,
				 request, strlen(request));

	status   = soup_session_send_message(webdav->priv->session, message);
	new_etag = soup_message_headers_get(message->response_headers, "ETag");

	redir_uri = soup_message_headers_get (message->response_headers, "Location");

	/* set UID and REV fields */
	e_contact_set(contact, E_CONTACT_REV, (gconstpointer) new_etag);
	if (redir_uri && *redir_uri) {
		if (!strstr (redir_uri, "://")) {
			/* it's a relative URI */
			SoupURI *suri = soup_uri_new (uri);
			gchar *full_uri;

			soup_uri_set_path (suri, redir_uri);
			full_uri = soup_uri_to_string (suri, TRUE);

			e_contact_set (contact, E_CONTACT_UID, full_uri);

			g_free (full_uri);
			soup_uri_free (suri);
		} else {
			e_contact_set (contact, E_CONTACT_UID, redir_uri);
		}
	} else {
		e_contact_set (contact, E_CONTACT_UID, uri);
	}

	g_object_unref(message);
	g_free(request);
	g_free(uri);

	return status;
}

static GError *
webdav_handle_auth_request(EBookBackendWebdav *webdav)
{
	EBookBackendWebdavPrivate *priv = webdav->priv;

	if (priv->username != NULL) {
		g_free(priv->username);
		priv->username = NULL;
		g_free(priv->password);
		priv->password = NULL;

		return EDB_ERROR (AUTHENTICATION_FAILED);
	} else {
		return EDB_ERROR (AUTHENTICATION_REQUIRED);
	}
}

static void
e_book_backend_webdav_create_contact(EBookBackend *backend,
		EDataBook *book, guint32 opid, const gchar *vcard)
{
	EBookBackendWebdav        *webdav = E_BOOK_BACKEND_WEBDAV (backend);
	EBookBackendWebdavPrivate *priv   = webdav->priv;
	EContact                  *contact;
	gchar                     *uid;
	guint                      status;

	if (priv->mode == E_DATA_BOOK_MODE_LOCAL) {
		e_data_book_respond_create (book, opid, EDB_ERROR (REPOSITORY_OFFLINE), NULL);
		return;
	}

	contact = e_contact_new_from_vcard(vcard);

	/* do 3 rand() calls to construct a unique ID... poor way but should be
	 * good enough for us */
	uid = g_strdup_printf("%s%08X-%08X-%08X.vcf", priv->uri, rand(), rand(),
			      rand());
	e_contact_set(contact, E_CONTACT_UID, uid);

	/* kill revision field (might have been set by some other backend) */
	e_contact_set(contact, E_CONTACT_REV, NULL);

	status = upload_contact(webdav, contact);
	if (status != 201 && status != 204) {
		g_object_unref(contact);
		if (status == 401 || status == 407) {
			e_data_book_respond_create (book, opid, webdav_handle_auth_request (webdav), NULL);
		} else {
			e_data_book_respond_create (book, opid,
					e_data_book_create_error_fmt (E_DATA_BOOK_STATUS_OTHER_ERROR,
						_("Create resource '%s' failed with HTTP status: %d"), uid, status),
					NULL);
		}
		g_free(uid);
		return;
	}
	/* PUT request didn't return an etag? try downloading to get one */
	if (e_contact_get_const(contact, E_CONTACT_REV) == NULL) {
		const gchar *new_uid;
		EContact *new_contact;

		g_warning("Server didn't return etag for new address resource");
		new_uid = e_contact_get_const (contact, E_CONTACT_UID);
		new_contact = download_contact (webdav, new_uid);
		g_object_unref(contact);

		if (new_contact == NULL) {
			e_data_book_respond_create (book, opid,
					EDB_ERROR (OTHER_ERROR), NULL);
			g_free(uid);
			return;
		}
		contact = new_contact;
	}

	e_book_backend_cache_add_contact(priv->cache, contact);
	e_data_book_respond_create (book, opid, EDB_ERROR (SUCCESS), contact);

	if (contact)
		g_object_unref(contact);
	g_free(uid);
}

static guint
delete_contact(EBookBackendWebdav *webdav, const gchar *uri)
{
	SoupMessage *message;
	guint        status;

	message = soup_message_new(SOUP_METHOD_DELETE, uri);
	soup_message_headers_append (message->request_headers, "User-Agent", USERAGENT);
	soup_message_headers_append (message->request_headers, "Connection", "close");

	status = soup_session_send_message(webdav->priv->session, message);
	g_object_unref(message);

	return status;
}

static void
e_book_backend_webdav_remove_contacts(EBookBackend *backend,
		EDataBook *book, guint32 opid, GList *id_list)
{
	EBookBackendWebdav        *webdav      = E_BOOK_BACKEND_WEBDAV(backend);
	EBookBackendWebdavPrivate *priv        = webdav->priv;
	GList                     *deleted_ids = NULL;
	GList                     *list;

	if (priv->mode == E_DATA_BOOK_MODE_LOCAL) {
		e_data_book_respond_remove_contacts (book, opid,
				EDB_ERROR (REPOSITORY_OFFLINE), NULL);
		return;
	}

	for (list = id_list; list != NULL; list = list->next) {
		const gchar *uid = (const gchar *) list->data;
		guint       status;

		status = delete_contact(webdav, uid);
		if (status != 204) {
			if (status == 401 || status == 407) {
				e_data_book_respond_remove_contacts (book, opid, webdav_handle_auth_request (webdav),
								    deleted_ids);
			} else {
				g_warning("DELETE failed with HTTP status %d", status);
			}
			continue;
		}
		e_book_backend_cache_remove_contact(priv->cache, uid);
		deleted_ids = g_list_append (deleted_ids, list->data);
	}

	e_data_book_respond_remove_contacts (book, opid,
			EDB_ERROR (SUCCESS),  deleted_ids);
}

static void
e_book_backend_webdav_modify_contact(EBookBackend *backend,
		EDataBook *book, guint32 opid, const gchar *vcard)
{
	EBookBackendWebdav        *webdav  = E_BOOK_BACKEND_WEBDAV(backend);
	EBookBackendWebdavPrivate *priv    = webdav->priv;
	EContact                  *contact = e_contact_new_from_vcard(vcard);
	const gchar                *uid;
	const gchar                *etag;
	guint status;

	if (priv->mode == E_DATA_BOOK_MODE_LOCAL) {
		e_data_book_respond_create(book, opid,
				EDB_ERROR (REPOSITORY_OFFLINE), NULL);
		g_object_unref(contact);
		return;
	}

	/* modify contact */
	status = upload_contact(webdav, contact);
	if (status != 201 && status != 204) {
		g_object_unref(contact);
		if (status == 401 || status == 407) {
			e_data_book_respond_remove_contacts (book, opid, webdav_handle_auth_request (webdav), NULL);
			return;
		}
		/* data changed on server while we were editing */
		if (status == 412) {
			/* too bad no special error code in evolution for this... */
			e_data_book_respond_modify (book, opid,
					e_data_book_create_error_fmt (E_DATA_BOOK_STATUS_OTHER_ERROR,
						"Contact on server changed -> not modifying"),
					NULL);
			return;
		}

		e_data_book_respond_modify (book, opid,
				e_data_book_create_error_fmt (E_DATA_BOOK_STATUS_OTHER_ERROR,
					"Modify contact failed with HTTP status: %d", status),
				NULL);
		return;
	}

	uid = e_contact_get_const(contact, E_CONTACT_UID);
	e_book_backend_cache_remove_contact(priv->cache, uid);

	etag = e_contact_get_const(contact, E_CONTACT_REV);

	/* PUT request didn't return an etag? try downloading to get one */
	if (etag == NULL || (etag[0] == 'W' && etag[1] == '/')) {
		EContact *new_contact;

		g_warning("Server didn't return etag for modified address resource");
		new_contact = download_contact(webdav, uid);
		if (new_contact != NULL) {
			contact = new_contact;
		}
	}
	e_book_backend_cache_add_contact(priv->cache, contact);

	e_data_book_respond_modify (book, opid, EDB_ERROR (SUCCESS), contact);

	g_object_unref(contact);
}

static void
e_book_backend_webdav_get_contact(EBookBackend *backend, EDataBook *book,
		guint32 opid, const gchar *uid)
{
	EBookBackendWebdav        *webdav = E_BOOK_BACKEND_WEBDAV(backend);
	EBookBackendWebdavPrivate *priv   = webdav->priv;
	EContact                  *contact;
	gchar                      *vcard;

	if (priv->mode == E_DATA_BOOK_MODE_LOCAL) {
		contact = e_book_backend_cache_get_contact(priv->cache, uid);
	} else {
		contact = download_contact(webdav, uid);
		/* update cache as we possibly have changes */
		if (contact != NULL) {
			e_book_backend_cache_remove_contact(priv->cache, uid);
			e_book_backend_cache_add_contact(priv->cache, contact);
		}
	}

	if (contact == NULL) {
		e_data_book_respond_get_contact (book, opid, EDB_ERROR (CONTACT_NOT_FOUND), NULL);
		return;
	}

	vcard = e_vcard_to_string(E_VCARD(contact), EVC_FORMAT_VCARD_30);
	e_data_book_respond_get_contact (book, opid, EDB_ERROR (SUCCESS), vcard);
	g_free(vcard);
	g_object_unref(contact);
}

typedef struct parser_strings_t {
	const xmlChar *multistatus;
	const xmlChar *dav;
	const xmlChar *href;
	const xmlChar *response;
	const xmlChar *propstat;
	const xmlChar *prop;
	const xmlChar *getetag;
} parser_strings_t;

typedef struct response_element_t response_element_t;
struct response_element_t {
	xmlChar            *href;
	xmlChar            *etag;
	response_element_t *next;
};

static response_element_t*
parse_response_tag(const parser_strings_t *strings, xmlTextReaderPtr reader)
{
	xmlChar            *href  = NULL;
	xmlChar            *etag  = NULL;
	gint                 depth = xmlTextReaderDepth(reader);
	response_element_t *element;

	while (xmlTextReaderRead(reader) && xmlTextReaderDepth(reader) > depth) {
		const xmlChar *tag_name;
		if (xmlTextReaderNodeType(reader) != XML_READER_TYPE_ELEMENT)
			continue;

		if (xmlTextReaderConstNamespaceUri(reader) != strings->dav)
			continue;

		tag_name = xmlTextReaderConstLocalName(reader);
		if (tag_name == strings->href) {
			if (href != NULL) {
				/* multiple href elements?!? */
				xmlFree(href);
			}
			href = xmlTextReaderReadString(reader);
		} else if (tag_name == strings->propstat) {
			/* find <propstat><prop><getetag> hierarchy */
			gint depth2 = xmlTextReaderDepth(reader);
			while (xmlTextReaderRead(reader)
					&& xmlTextReaderDepth(reader) > depth2) {
				gint depth3;
				if (xmlTextReaderNodeType(reader) != XML_READER_TYPE_ELEMENT)
					continue;

				if (xmlTextReaderConstNamespaceUri(reader) != strings->dav
						|| xmlTextReaderConstLocalName(reader) != strings->prop)
					continue;

				depth3 = xmlTextReaderDepth(reader);
				while (xmlTextReaderRead(reader)
						&& xmlTextReaderDepth(reader) > depth3) {
					if (xmlTextReaderNodeType(reader) != XML_READER_TYPE_ELEMENT)
						continue;

					if (xmlTextReaderConstNamespaceUri(reader) != strings->dav
							|| xmlTextReaderConstLocalName(reader)
							!= strings->getetag)
						continue;

					if (etag != NULL) {
						/* multiple etag elements?!? */
						xmlFree(etag);
					}
					etag = xmlTextReaderReadString(reader);
				}
			}
		}
	}

	if (href == NULL) {
		g_warning("webdav returned response element without href");
		return NULL;
	}

	/* append element to list */
	element = g_malloc(sizeof(element[0]));
	element->href = href;
	element->etag = etag;
	return element;
}

static response_element_t*
parse_propfind_response(xmlTextReaderPtr reader)
{
	parser_strings_t    strings;
	response_element_t *elements;

	/* get internalized versions of some strings to avoid strcmp while
	 * parsing */
	strings.multistatus
		= xmlTextReaderConstString(reader, BAD_CAST "multistatus");
	strings.dav         = xmlTextReaderConstString(reader, BAD_CAST "DAV:");
	strings.href        = xmlTextReaderConstString(reader, BAD_CAST "href");
	strings.response    = xmlTextReaderConstString(reader, BAD_CAST "response");
	strings.propstat    = xmlTextReaderConstString(reader, BAD_CAST "propstat");
	strings.prop        = xmlTextReaderConstString(reader, BAD_CAST "prop");
	strings.getetag     = xmlTextReaderConstString(reader, BAD_CAST "getetag");

	while (xmlTextReaderRead(reader)
			&& xmlTextReaderNodeType(reader) != XML_READER_TYPE_ELEMENT) {
	}

	if (xmlTextReaderConstLocalName(reader) != strings.multistatus
			|| xmlTextReaderConstNamespaceUri(reader) != strings.dav) {
		g_warning("webdav PROPFIND result is not <DAV:multistatus>");
		return NULL;
	}

	elements = NULL;

	/* parse all DAV:response tags */
	while (xmlTextReaderRead(reader) && xmlTextReaderDepth(reader) > 0) {
		response_element_t *element;

		if (xmlTextReaderNodeType(reader) != XML_READER_TYPE_ELEMENT)
			continue;

		if (xmlTextReaderConstLocalName(reader) != strings.response
				|| xmlTextReaderConstNamespaceUri(reader) != strings.dav)
			continue;

		element = parse_response_tag(&strings, reader);
		if (element == NULL)
			continue;

		element->next = elements;
		elements      = element;
	}

	return elements;
}

static SoupMessage*
send_propfind(EBookBackendWebdav *webdav)
{
	SoupMessage               *message;
	EBookBackendWebdavPrivate *priv    = webdav->priv;
	const gchar               *request =
		"<?xml version=\"1.0\" encoding=\"utf-8\"?>"
		"<propfind xmlns=\"DAV:\"><prop><getetag/></prop></propfind>";

	message = soup_message_new(SOUP_METHOD_PROPFIND, priv->uri);
	soup_message_headers_append (message->request_headers, "User-Agent", USERAGENT);
	soup_message_headers_append (message->request_headers, "Connection", "close");
	soup_message_headers_append(message->request_headers, "Depth", "1");
	soup_message_set_request(message, "text/xml", SOUP_MEMORY_TEMPORARY,
			(gchar *) request, strlen(request));

	soup_session_send_message(priv->session, message);

	return message;
}

static xmlXPathObjectPtr
xpath_eval (xmlXPathContextPtr ctx, const gchar *format, ...)
{
	xmlXPathObjectPtr  result;
	va_list            args;
	gchar              *expr;

	if (ctx == NULL) {
		return NULL;
	}

	va_start (args, format);
	expr = g_strdup_vprintf (format, args);
	va_end (args);

	result = xmlXPathEvalExpression ((xmlChar *) expr, ctx);
	g_free (expr);

	if (result == NULL) {
		return NULL;
	}

	if (result->type == XPATH_NODESET &&
	    xmlXPathNodeSetIsEmpty (result->nodesetval)) {
		xmlXPathFreeObject (result);
		return NULL;
	}

	return result;
}

static gchar *
xp_object_get_string (xmlXPathObjectPtr result)
{
	gchar *ret = NULL;

	if (result == NULL)
		return ret;

	if (result->type == XPATH_STRING) {
		ret = g_strdup ((gchar *) result->stringval);
	}

	xmlXPathFreeObject (result);
	return ret;
}

static guint
xp_object_get_status (xmlXPathObjectPtr result)
{
	gboolean res;
	guint    ret = 0;

	if (result == NULL)
		return ret;

	if (result->type == XPATH_STRING) {
		res = soup_headers_parse_status_line ((gchar *) result->stringval, NULL, &ret, NULL);
		if (!res) {
			ret = 0;
		}
	}

	xmlXPathFreeObject (result);
	return ret;
}

static gboolean
check_addressbook_changed (EBookBackendWebdav *webdav, gchar **new_ctag)
{
	gboolean res = TRUE;
	const gchar *request = "<?xml version=\"1.0\" encoding=\"utf-8\"?><propfind xmlns=\"DAV:\"><prop><getctag/></prop></propfind>";
	EBookBackendWebdavPrivate *priv;
	SoupMessage *message;

	g_return_val_if_fail (webdav != NULL, TRUE);
	g_return_val_if_fail (new_ctag != NULL, TRUE);

	*new_ctag = NULL;
	priv = webdav->priv;

	if (!priv->supports_getctag)
		return TRUE;

	priv->supports_getctag = FALSE;

	message = soup_message_new (SOUP_METHOD_PROPFIND, priv->uri);
	if (!message)
		return TRUE;

	soup_message_headers_append (message->request_headers, "User-Agent", USERAGENT);
	soup_message_headers_append (message->request_headers, "Connection", "close");
	soup_message_headers_append (message->request_headers, "Depth", "0");
	soup_message_set_request (message, "text/xml", SOUP_MEMORY_TEMPORARY, (gchar *) request, strlen (request));
	soup_session_send_message (priv->session, message);

	if (message->status_code == 207 && message->response_body) {
		xmlDocPtr xml;

		xml = xmlReadMemory (message->response_body->data, message->response_body->length, NULL, NULL, XML_PARSE_NOWARNING);
		if (xml) {
			const gchar *GETCTAG_XPATH_STATUS = "string(/D:multistatus/D:response/D:propstat/D:prop/D:getctag/../../D:status)";
			const gchar *GETCTAG_XPATH_VALUE = "string(/D:multistatus/D:response/D:propstat/D:prop/D:getctag)";
			xmlXPathContextPtr xpctx;

			xpctx = xmlXPathNewContext (xml);
			xmlXPathRegisterNs (xpctx, (xmlChar *) "D", (xmlChar *) "DAV:");

			if (xp_object_get_status (xpath_eval (xpctx, GETCTAG_XPATH_STATUS)) == 200) {
				gchar *txt = xp_object_get_string (xpath_eval (xpctx, GETCTAG_XPATH_VALUE));

				if (txt && *txt) {
					gint len = strlen (txt);

					if (*txt == '\"' && len > 2 && txt [len - 1] == '\"') {
						/* dequote */
						*new_ctag = g_strndup (txt + 1, len - 2);
					} else {
						*new_ctag = txt;
						txt = NULL;
					}

					if (*new_ctag) {
						const gchar *my_ctag;

						my_ctag = e_file_cache_get_object (E_FILE_CACHE (priv->cache), WEBDAV_CTAG_KEY);
						res = !my_ctag || !g_str_equal (my_ctag, *new_ctag);
						priv->supports_getctag = TRUE;
					}
				}

				g_free (txt);
			}

			xmlXPathFreeContext (xpctx);
			xmlFreeDoc (xml);
		}
	}

	g_object_unref (message);

	return res;
}

static GError *
download_contacts(EBookBackendWebdav *webdav, EFlag *running,
                  EDataBookView *book_view)
{
	EBookBackendWebdavPrivate *priv = webdav->priv;
	SoupMessage               *message;
	guint                      status;
	xmlTextReaderPtr           reader;
	response_element_t        *elements;
	response_element_t        *element;
	response_element_t        *next;
	gint                        count;
	gint                        i;
	gchar                     *new_ctag = NULL;

	if (!check_addressbook_changed (webdav, &new_ctag)) {
		if (book_view) {
			GList *contact_list, *cl;

			contact_list = e_book_backend_cache_get_contacts (priv->cache, e_data_book_view_get_card_query (book_view));
			for (cl = contact_list; cl != NULL; cl = g_list_next (cl)) {
				EContact *contact = cl->data;

				e_data_book_view_notify_update (book_view, contact);

				g_object_unref (contact);
			}
			g_list_free (contact_list);
		}
		g_free (new_ctag);
		return EDB_ERROR (SUCCESS);
	}

	if (book_view != NULL) {
		e_data_book_view_notify_status_message(book_view,
				"Loading Addressbook summary...");
	}

	message = send_propfind(webdav);
	status  = message->status_code;

	if (status == 401 || status == 407) {
		g_object_unref(message);
		g_free (new_ctag);
		return webdav_handle_auth_request (webdav);
	}
	if (status != 207) {
		g_object_unref(message);
		g_free (new_ctag);
		return e_data_book_create_error_fmt (E_DATA_BOOK_STATUS_OTHER_ERROR, "PROPFIND on webdav failed with HTTP status %d", status);
	}
	if (message->response_body == NULL) {
		g_warning("No response body in webdav PROPFIND result");
		g_object_unref(message);
		g_free (new_ctag);
		return e_data_book_create_error_fmt (E_DATA_BOOK_STATUS_OTHER_ERROR, "No response body in webdav PROPFIND result");
	}

	/* parse response */
	reader = xmlReaderForMemory(message->response_body->data,
				    message->response_body->length, NULL, NULL,
				    XML_PARSE_NOWARNING);

	elements = parse_propfind_response(reader);

	/* count contacts */
	count = 0;
	for (element = elements; element != NULL; element = element->next) {
		++count;
	}

	/* download contacts */
	i = 0;
	for (element = elements; element != NULL; element = element->next, ++i) {
		const gchar  *uri;
		const gchar *etag;
		EContact    *contact;
		gchar *complete_uri;

		/* stop downloading if search was aborted */
		if (running != NULL && !e_flag_is_set(running))
			break;

		if (book_view != NULL) {
			gfloat percent = 100.0 / count * i;
			gchar buf[100];
			snprintf(buf, sizeof(buf), "Loading Contacts (%d%%)", (gint)percent);
			e_data_book_view_notify_status_message(book_view, buf);
		}

		/* skip collections */
		uri = (const gchar *) element->href;
		if (uri[strlen(uri)-1] == '/')
			continue;

		/* uri might be relative, construct complete one */
		if (uri[0] == '/') {
			SoupURI *soup_uri = soup_uri_new(priv->uri);
			soup_uri->path    = g_strdup(uri);

			complete_uri = soup_uri_to_string(soup_uri, 0);
			soup_uri_free(soup_uri);
		} else {
			complete_uri = g_strdup(uri);
		}

		etag = (const gchar *) element->etag;

		contact = e_book_backend_cache_get_contact(priv->cache, complete_uri);
		/* download contact if it is not cached or its ETag changed */
		if (contact == NULL || etag == NULL ||
				strcmp(e_contact_get_const(contact, E_CONTACT_REV),etag) != 0) {
			contact = download_contact(webdav, complete_uri);
			if (contact != NULL) {
				e_book_backend_cache_remove_contact(priv->cache, complete_uri);
				e_book_backend_cache_add_contact(priv->cache, contact);
			}
		}

		if (contact != NULL && book_view != NULL) {
			e_data_book_view_notify_update(book_view, contact);
		}

		g_free(complete_uri);
	}

	/* free element list */
	for (element = elements; element != NULL; element = next) {
		next = element->next;

		xmlFree(element->href);
		xmlFree(element->etag);
		g_free(element);
	}

	xmlFreeTextReader(reader);
	g_object_unref(message);

	if (new_ctag) {
		if (!e_file_cache_replace_object (E_FILE_CACHE (priv->cache), WEBDAV_CTAG_KEY, new_ctag))
			e_file_cache_add_object (E_FILE_CACHE (priv->cache), WEBDAV_CTAG_KEY, new_ctag);
	}
	g_free (new_ctag);

	return EDB_ERROR (SUCCESS);
}

static gpointer
book_view_thread(gpointer data)
{
	EDataBookView                          *book_view = data;
	WebdavBackendSearchClosure             *closure   = get_closure(book_view);
	EBookBackendWebdav                     *webdav    = closure->webdav;
	GError *error;

	e_flag_set(closure->running);

	/* ref the book view because it'll be removed and unrefed when/if
	 * it's stopped */
	e_data_book_view_ref(book_view);

	error = download_contacts (webdav, closure->running, book_view);

	/* report back status if query wasn't aborted */
	e_data_book_view_notify_complete (book_view, error);
	e_data_book_view_unref (book_view);

	if (error)
		g_error_free (error);
	return NULL;
}

static void
e_book_backend_webdav_start_book_view(EBookBackend *backend,
                                      EDataBookView *book_view)
{
	EBookBackendWebdav        *webdav = E_BOOK_BACKEND_WEBDAV(backend);
	EBookBackendWebdavPrivate *priv   = webdav->priv;

	if (priv->mode == E_DATA_BOOK_MODE_REMOTE) {
		WebdavBackendSearchClosure *closure
			= init_closure(book_view, E_BOOK_BACKEND_WEBDAV(backend));

		closure->thread
			= g_thread_create(book_view_thread, book_view, TRUE, NULL);

		e_flag_wait(closure->running);
	} else {
		const gchar *query = e_data_book_view_get_card_query(book_view);
		GList *contacts = e_book_backend_cache_get_contacts(priv->cache, query);
		GList *l;

		for (l = contacts; l != NULL; l = g_list_next(l)) {
			EContact *contact = l->data;
			e_data_book_view_notify_update(book_view, contact);
			g_object_unref(contact);
		}
		g_list_free(contacts);
		e_data_book_view_notify_complete (book_view, NULL /* Success */);
	}
}

static void
e_book_backend_webdav_stop_book_view(EBookBackend *backend,
                                     EDataBookView *book_view)
{
	EBookBackendWebdav         *webdav = E_BOOK_BACKEND_WEBDAV(backend);
	WebdavBackendSearchClosure *closure;
	gboolean                    need_join;

	if (webdav->priv->mode == E_DATA_BOOK_MODE_LOCAL)
		return;

	closure = get_closure(book_view);
	if (closure == NULL)
		return;

	need_join = e_flag_is_set(closure->running);
	e_flag_clear(closure->running);

	if (need_join) {
		g_thread_join(closure->thread);
	}
}

static void
e_book_backend_webdav_get_contact_list(EBookBackend *backend, EDataBook *book,
		guint32 opid, const gchar *query)
{
	EBookBackendWebdav        *webdav = E_BOOK_BACKEND_WEBDAV(backend);
	EBookBackendWebdavPrivate *priv   = webdav->priv;
	GList                     *contact_list;
	GList                     *vcard_list;
	GList                     *c;

	if (priv->mode == E_DATA_BOOK_MODE_REMOTE) {
		/* make sure the cache is up to date */
		GError *error = download_contacts (webdav, NULL, NULL);

		if (error) {
			e_data_book_respond_get_contact_list (book, opid, error, NULL);
			return;
		}
	}

	/* answer query from cache */
	contact_list = e_book_backend_cache_get_contacts(priv->cache, query);
	vcard_list   = NULL;
	for (c = contact_list; c != NULL; c = g_list_next(c)) {
		EContact *contact = c->data;
		gchar     *vcard
			= e_vcard_to_string(E_VCARD(contact), EVC_FORMAT_VCARD_30);
		vcard_list = g_list_append(vcard_list, vcard);
		g_object_unref(contact);
	}
	g_list_free(contact_list);

	e_data_book_respond_get_contact_list (book, opid, EDB_ERROR (SUCCESS), vcard_list);
}

static void
e_book_backend_webdav_authenticate_user(EBookBackend *backend, EDataBook *book,
		guint32 opid, const gchar *user, const gchar *password,
		const gchar *auth_method)
{
	EBookBackendWebdav        *webdav = E_BOOK_BACKEND_WEBDAV(backend);
	EBookBackendWebdavPrivate *priv   = webdav->priv;
	SoupMessage               *message;
	guint                      status;

	priv->username = g_strdup(user);
	priv->password = g_strdup(password);

	/* Evolution API requires a direct feedback on the authentication,
	 * so we send a PROPFIND to test wether user/password is correct */
	message = send_propfind(webdav);
	status  = message->status_code;
	g_object_unref(message);

	if (status == 401 || status == 407) {
		g_free(priv->username);
		priv->username = NULL;
		g_free(priv->password);
		priv->password = NULL;

		e_data_book_respond_authenticate_user (book, opid, EDB_ERROR (AUTHENTICATION_FAILED));
	} else {
		e_data_book_respond_authenticate_user (book, opid, EDB_ERROR (SUCCESS));
	}
}

static void
e_book_backend_webdav_get_supported_fields(EBookBackend *backend,
		EDataBook *book, guint32 opid)
{
	GList *fields = NULL;
	gint    i;

	/* we support everything */
	for (i = 1; i < E_CONTACT_FIELD_LAST; ++i) {
		fields = g_list_append(fields, g_strdup(e_contact_field_name(i)));
	}

	e_data_book_respond_get_supported_fields (book, opid, EDB_ERROR (SUCCESS), fields);
	g_list_foreach(fields, (GFunc) g_free, NULL);
	g_list_free(fields);
}

static void
e_book_backend_webdav_get_supported_auth_methods(EBookBackend *backend,
		EDataBook *book, guint32 opid)
{
	GList *auth_methods = NULL;

	auth_methods = g_list_append(auth_methods, g_strdup("plain/password"));

	e_data_book_respond_get_supported_auth_methods (book, opid, EDB_ERROR (SUCCESS), auth_methods);

	g_list_foreach(auth_methods, (GFunc) g_free, NULL);
	g_list_free(auth_methods);
}

static void
e_book_backend_webdav_get_required_fields(EBookBackend *backend,
		EDataBook *book, guint32 opid)
{
	GList       *fields = NULL;
	const gchar *field_name;

	field_name = e_contact_field_name(E_CONTACT_FILE_AS);
	fields     = g_list_append(fields , g_strdup(field_name));

	e_data_book_respond_get_supported_fields (book, opid, EDB_ERROR (SUCCESS), fields);
	g_list_free (fields);
}

/** authentication callback for libsoup */
static void soup_authenticate(SoupSession *session, SoupMessage *message,
                              SoupAuth *auth, gboolean retrying, gpointer data)
{
	EBookBackendWebdav        *webdav = data;
	EBookBackendWebdavPrivate *priv   = webdav->priv;

	if (retrying)
		return;

	if (priv->username != NULL) {
		soup_auth_authenticate(auth, g_strdup(priv->username),
				       g_strdup(priv->password));
	}
}

static void
proxy_settings_changed (EProxy *proxy, gpointer user_data)
{
	SoupURI *proxy_uri = NULL;
	EBookBackendWebdavPrivate *priv = (EBookBackendWebdavPrivate *) user_data;

	if (!priv || !priv->uri || !priv->session)
		return;

	/* use proxy if necessary */
	if (e_proxy_require_proxy_for_uri (proxy, priv->uri)) {
		proxy_uri = e_proxy_peek_uri_for (proxy, priv->uri);
	}

	g_object_set (priv->session, SOUP_SESSION_PROXY_URI, proxy_uri, NULL);
}

static void
e_book_backend_webdav_load_source(EBookBackend *backend,
                                  ESource *source, gboolean only_if_exists, GError **perror)
{
	EBookBackendWebdav        *webdav = E_BOOK_BACKEND_WEBDAV(backend);
	EBookBackendWebdavPrivate *priv   = webdav->priv;
	gchar                     *uri;
	const gchar               *cache_dir;
	const gchar               *offline;
	const gchar               *use_ssl;
	gchar                     *filename;
	SoupSession               *session;
	SoupURI                   *suri;

	/* will try fetch ctag for the first time, if it fails then sets this to FALSE */
	priv->supports_getctag = TRUE;

	cache_dir = e_book_backend_get_cache_dir (backend);

	uri = e_source_get_uri(source);
	if (uri == NULL) {
		g_propagate_error (perror, EDB_ERROR_EX (OTHER_ERROR, "No uri given for addressbook"));
		return;
	}

	suri = soup_uri_new (uri);
	g_free (uri);

	if (!suri) {
		g_propagate_error (perror, EDB_ERROR_EX (OTHER_ERROR, "Invalid uri given for addressbook"));
		return;
	}

	offline = e_source_get_property(source, "offline_sync");
	if (offline && g_str_equal(offline, "1"))
		priv->marked_for_offline = TRUE;

	if (priv->mode == E_DATA_BOOK_MODE_LOCAL
			&& !priv->marked_for_offline ) {
		soup_uri_free (suri);
		g_propagate_error (perror, EDB_ERROR (OFFLINE_UNAVAILABLE));
		return;
	}

	if (!suri->scheme || !g_str_equal (suri->scheme, "webdav")) {
		/* the book is not for us */
		soup_uri_free (suri);
		g_propagate_error (perror, EDB_ERROR_EX (OTHER_ERROR, "Not a webdav uri"));
		return;
	}

	use_ssl = e_source_get_property (source, "use_ssl");
	if (use_ssl != NULL && strcmp (use_ssl, "1") == 0) {
		soup_uri_set_scheme (suri, "https");
	} else {
		soup_uri_set_scheme (suri, "http");
	}

	/* append slash if missing */
	if (!suri->path || !*suri->path || suri->path[strlen (suri->path) - 1] != '/') {
		gchar *new_path = g_strconcat (suri->path ? suri->path : "", "/", NULL);
		soup_uri_set_path (suri, new_path);
		g_free (new_path);
	}

	if (suri->host && strchr (suri->host, '@')) {
		gchar *at = strchr (suri->host, '@');
		gchar *new_user;

		*at = '\0';

		new_user = g_strconcat (suri->user ? suri->user : "", "@", suri->host, NULL);

		*at = '@';

		soup_uri_set_host (suri, at + 1);
		soup_uri_set_user (suri, new_user);

		g_free (new_user);
	}

	priv->uri = soup_uri_to_string (suri, FALSE);
	if (!priv->uri) {
		soup_uri_free (suri);
		g_propagate_error (perror, EDB_ERROR_EX (OTHER_ERROR, "Cannot transform SoupURI to string"));
		return;
	}

	filename = g_build_filename (cache_dir, "cache.xml", NULL);
	priv->cache = e_book_backend_cache_new (filename);
	g_free (filename);

	session = soup_session_sync_new();
	g_signal_connect(session, "authenticate", G_CALLBACK(soup_authenticate),
			 webdav);

	priv->session = session;
	priv->proxy = e_proxy_new ();
	e_proxy_setup_proxy (priv->proxy);
	g_signal_connect (priv->proxy, "changed", G_CALLBACK (proxy_settings_changed), priv);
	proxy_settings_changed (priv->proxy, priv);
	webdav_debug_setup (priv->session);

	e_book_backend_notify_auth_required(backend);
	e_book_backend_set_is_loaded(backend, TRUE);
	e_book_backend_notify_connection_status(backend, TRUE);
	e_book_backend_set_is_writable(backend, TRUE);
	e_book_backend_notify_writable(backend, TRUE);

	soup_uri_free (suri);
}

static void
e_book_backend_webdav_remove(EBookBackend *backend,	EDataBook *book,
		guint32 opid)
{
	e_data_book_respond_remove (book, opid, EDB_ERROR (SUCCESS));
}

static void
e_book_backend_webdav_set_mode(EBookBackend *backend,
                               EDataBookMode mode)
{
	EBookBackendWebdav *webdav = E_BOOK_BACKEND_WEBDAV(backend);

	webdav->priv->mode = mode;

	/* set_mode is called before the backend is loaded */
	if (!e_book_backend_is_loaded(backend))
		return;

	if (mode == E_DATA_BOOK_MODE_LOCAL) {
		e_book_backend_set_is_writable(backend, FALSE);
		e_book_backend_notify_writable(backend, FALSE);
		e_book_backend_notify_connection_status(backend, FALSE);
	} else if (mode == E_DATA_BOOK_MODE_REMOTE) {
		e_book_backend_set_is_writable(backend, TRUE);
		e_book_backend_notify_writable(backend, TRUE);
		e_book_backend_notify_connection_status(backend, TRUE);
	}
}

static gchar *
e_book_backend_webdav_get_static_capabilities(EBookBackend *backend)
{
	return g_strdup("net,do-initial-query,contact-lists");
}

static void
e_book_backend_webdav_cancel_operation (EBookBackend *backend, EDataBook *book, GError **perror)
{
	g_propagate_error (perror, EDB_ERROR (COULD_NOT_CANCEL));
}

EBookBackend *
e_book_backend_webdav_new(void)
{
	return g_object_new (E_TYPE_BOOK_BACKEND_WEBDAV, NULL);
}

static void
e_book_backend_webdav_dispose(GObject *object)
{
	EBookBackendWebdav        *webdav = E_BOOK_BACKEND_WEBDAV(object);
	EBookBackendWebdavPrivate *priv   = webdav->priv;

	g_object_unref(priv->session);
	g_object_unref (priv->proxy);
	g_object_unref(priv->cache);
	g_free(priv->uri);
	g_free(priv->username);
	g_free(priv->password);

	G_OBJECT_CLASS(parent_class)->dispose(object);
}

static void
e_book_backend_webdav_class_init(EBookBackendWebdavClass *klass)
{
	GObjectClass      *object_class = G_OBJECT_CLASS(klass);
	EBookBackendClass *backend_class;

	parent_class = g_type_class_peek_parent(klass);

	backend_class = E_BOOK_BACKEND_CLASS(klass);

	/* Set the virtual methods. */
	backend_class->load_source                = e_book_backend_webdav_load_source;
	backend_class->get_static_capabilities    = e_book_backend_webdav_get_static_capabilities;

	backend_class->create_contact             = e_book_backend_webdav_create_contact;
	backend_class->remove_contacts            = e_book_backend_webdav_remove_contacts;
	backend_class->modify_contact             = e_book_backend_webdav_modify_contact;
	backend_class->get_contact                = e_book_backend_webdav_get_contact;
	backend_class->get_contact_list           = e_book_backend_webdav_get_contact_list;
	backend_class->start_book_view            = e_book_backend_webdav_start_book_view;
	backend_class->stop_book_view             = e_book_backend_webdav_stop_book_view;
	backend_class->authenticate_user          = e_book_backend_webdav_authenticate_user;
	backend_class->get_supported_fields       = e_book_backend_webdav_get_supported_fields;
	backend_class->get_required_fields        = e_book_backend_webdav_get_required_fields;
	backend_class->cancel_operation           = e_book_backend_webdav_cancel_operation;
	backend_class->get_supported_auth_methods = e_book_backend_webdav_get_supported_auth_methods;
	backend_class->remove                     = e_book_backend_webdav_remove;
	backend_class->set_mode                   = e_book_backend_webdav_set_mode;

	object_class->dispose                     = e_book_backend_webdav_dispose;

	g_type_class_add_private(object_class, sizeof(EBookBackendWebdavPrivate));
}

static void
e_book_backend_webdav_init(EBookBackendWebdav *backend)
{
	backend->priv = G_TYPE_INSTANCE_GET_PRIVATE(backend,
			E_TYPE_BOOK_BACKEND_WEBDAV, EBookBackendWebdavPrivate);
}

