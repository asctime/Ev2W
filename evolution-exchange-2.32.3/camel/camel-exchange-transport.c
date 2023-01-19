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

/* camel-exchange-transport.c: Exchange transport class. */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib/gi18n-lib.h>

#include "camel-exchange-transport.h"
#include "camel-exchange-utils.h"

G_DEFINE_TYPE (CamelExchangeTransport, camel_exchange_transport, CAMEL_TYPE_TRANSPORT)

static gboolean
exchange_transport_send_to (CamelTransport *transport,
                            CamelMimeMessage *message,
                            CamelAddress *from,
                            CamelAddress *recipients,
                            GError **error)
{
	CamelService *service = CAMEL_SERVICE (transport);
	CamelStore *store = NULL;
	gchar *url_string;
	CamelInternetAddress *cia;
	const gchar *addr;
	GPtrArray *recipients_array;
	gboolean success;
	CamelStream *stream;
	CamelStream *filtered_stream;
	CamelMimeFilter *crlffilter;
	GByteArray *byte_array;
	struct _camel_header_raw *header;
	GSList *h, *bcc = NULL;
	gint len, i;

	url_string = camel_session_get_password (
		service->session, service, NULL,
		"ignored", "popb4smtp_uri", 0, error);
	if (!url_string)
		return FALSE;
	if (strncmp (url_string, "exchange:", 9) != 0) {
		g_set_error (
			error, CAMEL_SERVICE_ERROR,
			CAMEL_SERVICE_ERROR_UNAVAILABLE,
			_("Exchange transport can only be used with Exchange mail source"));
		g_free (url_string);
		return FALSE;
	}

	g_free (url_string);

	recipients_array = g_ptr_array_new ();
	len = camel_address_length (recipients);
	cia = CAMEL_INTERNET_ADDRESS (recipients);
	for (i = 0; i < len; i++) {
		if (!camel_internet_address_get (cia, i, NULL, &addr)) {
			g_set_error (
				error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
				_("Cannot send message: one or more invalid recipients"));
			g_ptr_array_free (recipients_array, TRUE);
			return FALSE;
		}
		g_ptr_array_add (recipients_array, (gchar *)addr);
	}

	if (!camel_internet_address_get (CAMEL_INTERNET_ADDRESS (from), 0, NULL, &addr)) {
		g_set_error (
			error, CAMEL_SERVICE_ERROR,
			CAMEL_SERVICE_ERROR_UNAVAILABLE,
			_("Could not find 'From' address in message"));
		g_ptr_array_free (recipients_array, TRUE);
		return FALSE;
	}

	byte_array = g_byte_array_new ();
	stream = camel_stream_mem_new_with_byte_array (byte_array);
	crlffilter = camel_mime_filter_crlf_new (CAMEL_MIME_FILTER_CRLF_ENCODE, CAMEL_MIME_FILTER_CRLF_MODE_CRLF_ONLY);
	filtered_stream = camel_stream_filter_new (stream);
	camel_stream_filter_add (
		CAMEL_STREAM_FILTER (filtered_stream),
		CAMEL_MIME_FILTER (crlffilter));
	g_object_unref (CAMEL_OBJECT (crlffilter));

	/* Gross hack copied from camel-smtp-transport. ugh. FIXME */
	/* copy and remove the bcc headers */
	header = CAMEL_MIME_PART (message)->headers;
	while (header) {
		if (!g_ascii_strcasecmp (header->name, "Bcc"))
			bcc = g_slist_append (bcc, g_strdup (header->value));
		header = header->next;
	}

	camel_medium_remove_header (CAMEL_MEDIUM (message), "Bcc");

	camel_data_wrapper_write_to_stream (
		CAMEL_DATA_WRAPPER (message),
		CAMEL_STREAM (filtered_stream), NULL);
	camel_stream_flush (CAMEL_STREAM (filtered_stream), NULL);
	g_object_unref (CAMEL_OBJECT (filtered_stream));

	/* add the bcc headers back */
	if (bcc) {
		h = bcc;
		while (h) {
			camel_medium_add_header (CAMEL_MEDIUM (message), "Bcc", h->data);
			g_free (h->data);
			h = h->next;
		}
		g_slist_free (bcc);
	}

	success = camel_exchange_utils_send_message (
		CAMEL_SERVICE (transport), addr,
		recipients_array, byte_array, error);

	g_ptr_array_free (recipients_array, TRUE);
	g_object_unref (stream);

	if (store)
		g_object_unref (CAMEL_OBJECT (store));

	return success;
}

static void
camel_exchange_transport_class_init (CamelExchangeTransportClass *class)
{
	CamelTransportClass *transport_class;

	transport_class = CAMEL_TRANSPORT_CLASS (class);
	transport_class->send_to = exchange_transport_send_to;
}

static void
camel_exchange_transport_init (CamelExchangeTransport *transport)
{
}

