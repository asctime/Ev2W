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

#include <string.h>

#include <glib/gi18n-lib.h>

#include "camel-internet-address.h"
#include "camel-sasl-anonymous.h"

CamelServiceAuthType camel_sasl_anonymous_authtype = {
	N_("Anonymous"),

	N_("This option will connect to the server using an anonymous login."),

	"ANONYMOUS",
	FALSE
};

G_DEFINE_TYPE (CamelSaslAnonymous, camel_sasl_anonymous, CAMEL_TYPE_SASL)

static void
sasl_anonymous_finalize (GObject *object)
{
	CamelSaslAnonymous *sasl = CAMEL_SASL_ANONYMOUS (object);

	g_free (sasl->trace_info);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (camel_sasl_anonymous_parent_class)->finalize (object);
}

static GByteArray *
sasl_anonymous_challenge (CamelSasl *sasl,
                          GByteArray *token,
                          GError **error)
{
	CamelSaslAnonymous *sasl_anon = CAMEL_SASL_ANONYMOUS (sasl);
	CamelInternetAddress *cia;
	GByteArray *ret = NULL;

	if (token) {
		g_set_error (
			error, CAMEL_SERVICE_ERROR,
			CAMEL_SERVICE_ERROR_CANT_AUTHENTICATE,
			_("Authentication failed."));
		return NULL;
	}

	switch (sasl_anon->type) {
	case CAMEL_SASL_ANON_TRACE_EMAIL:
		cia = camel_internet_address_new ();
		if (camel_internet_address_add (cia, NULL, sasl_anon->trace_info) != 1) {
			g_set_error (
				error, CAMEL_SERVICE_ERROR,
				CAMEL_SERVICE_ERROR_CANT_AUTHENTICATE,
				_("Invalid email address trace information:\n%s"),
				sasl_anon->trace_info);
			g_object_unref (cia);
			return NULL;
		}
		g_object_unref (cia);
		ret = g_byte_array_new ();
		g_byte_array_append (ret, (guint8 *) sasl_anon->trace_info, strlen (sasl_anon->trace_info));
		break;
	case CAMEL_SASL_ANON_TRACE_OPAQUE:
		if (strchr (sasl_anon->trace_info, '@')) {
			g_set_error (
				error, CAMEL_SERVICE_ERROR,
				CAMEL_SERVICE_ERROR_CANT_AUTHENTICATE,
				_("Invalid opaque trace information:\n%s"),
				sasl_anon->trace_info);
			return NULL;
		}
		ret = g_byte_array_new ();
		g_byte_array_append (ret, (guint8 *) sasl_anon->trace_info, strlen (sasl_anon->trace_info));
		break;
	case CAMEL_SASL_ANON_TRACE_EMPTY:
		ret = g_byte_array_new ();
		break;
	default:
		g_set_error (
			error, CAMEL_SERVICE_ERROR,
			CAMEL_SERVICE_ERROR_CANT_AUTHENTICATE,
			_("Invalid trace information:\n%s"),
			sasl_anon->trace_info);
		return NULL;
	}

	camel_sasl_set_authenticated (sasl, TRUE);
	return ret;
}

static void
camel_sasl_anonymous_class_init (CamelSaslAnonymousClass *class)
{
	GObjectClass *object_class;
	CamelSaslClass *sasl_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = sasl_anonymous_finalize;

	sasl_class = CAMEL_SASL_CLASS (class);
	sasl_class->challenge = sasl_anonymous_challenge;
}

static void
camel_sasl_anonymous_init (CamelSaslAnonymous *sasl_anonymous)
{
}

/**
 * camel_sasl_anonymous_new:
 * @type: trace type
 * @trace_info: trace info
 *
 * Create a new #CamelSaslAnonymous object.
 *
 * Returns: a new #CamelSasl object
 **/
CamelSasl *
camel_sasl_anonymous_new (CamelSaslAnonTraceType type, const gchar *trace_info)
{
	CamelSaslAnonymous *sasl_anon;

	if (!trace_info && type != CAMEL_SASL_ANON_TRACE_EMPTY)
		return NULL;

	sasl_anon = g_object_new (CAMEL_TYPE_SASL_ANONYMOUS, NULL);
	sasl_anon->trace_info = g_strdup (trace_info);
	sasl_anon->type = type;

	return CAMEL_SASL (sasl_anon);
}
