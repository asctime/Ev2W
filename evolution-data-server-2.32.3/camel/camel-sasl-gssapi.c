/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

/* If building without Kerberos support, this class is an empty shell. */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>

#include <string.h>
#include <sys/types.h>

#ifndef _WIN32
#include <netdb.h>
#include <sys/socket.h>
#endif

#include <gio/gio.h>
#include <glib/gi18n-lib.h>

#include "camel-net-utils.h"
#include "camel-sasl-gssapi.h"
#include "camel-session.h"

#ifdef HAVE_KRB5

#ifdef HAVE_HEIMDAL_KRB5
#include <krb5.h>
#else
#include <krb5/krb5.h>
#endif /* HAVE_HEIMDAL_KRB5 */

#ifdef HAVE_ET_COM_ERR_H
#include <et/com_err.h>
#else
#ifdef HAVE_COM_ERR_H
#include <com_err.h>
#endif /* HAVE_COM_ERR_H */
#endif /* HAVE_ET_COM_ERR_H */

#ifdef HAVE_MIT_KRB5
#include <gssapi/gssapi.h>
#include <gssapi/gssapi_generic.h>
#endif /* HAVE_MIT_KRB5 */

#ifdef HAVE_HEIMDAL_KRB5
#include <gssapi.h>
#else
#ifdef HAVE_SUN_KRB5
#include <gssapi/gssapi.h>
#include <gssapi/gssapi_ext.h>
extern gss_OID gss_nt_service_name;
#endif /* HAVE_SUN_KRB5 */
#endif /* HAVE_HEIMDAL_KRB5 */

#ifndef GSS_C_OID_KRBV5_DES
#define GSS_C_OID_KRBV5_DES GSS_C_NO_OID
#endif

#define DBUS_PATH		"/org/gnome/KrbAuthDialog"
#define DBUS_INTERFACE		"org.gnome.KrbAuthDialog"
#define DBUS_METHOD		"org.gnome.KrbAuthDialog.acquireTgt"

#define CAMEL_SASL_GSSAPI_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), CAMEL_TYPE_SASL_GSSAPI, CamelSaslGssapiPrivate))

CamelServiceAuthType camel_sasl_gssapi_authtype = {
	N_("GSSAPI"),

	N_("This option will connect to the server using "
	   "Kerberos 5 authentication."),

	"GSSAPI",
	FALSE
};

enum {
	GSSAPI_STATE_INIT,
	GSSAPI_STATE_CONTINUE_NEEDED,
	GSSAPI_STATE_COMPLETE,
	GSSAPI_STATE_AUTHENTICATED
};

#define GSSAPI_SECURITY_LAYER_NONE       (1 << 0)
#define GSSAPI_SECURITY_LAYER_INTEGRITY  (1 << 1)
#define GSSAPI_SECURITY_LAYER_PRIVACY    (1 << 2)

#define DESIRED_SECURITY_LAYER  GSSAPI_SECURITY_LAYER_NONE

struct _CamelSaslGssapiPrivate {
	gint state;
	gss_ctx_id_t ctx;
	gss_name_t target;
};

#endif /* HAVE_KRB5 */

G_DEFINE_TYPE (CamelSaslGssapi, camel_sasl_gssapi, CAMEL_TYPE_SASL)

#ifdef HAVE_KRB5

static void
gssapi_set_exception (OM_uint32 major,
                      OM_uint32 minor,
                      GError **error)
{
	const gchar *str;

	switch (major) {
	case GSS_S_BAD_MECH:
		str = _("The specified mechanism is not supported by the "
			"provided credential, or is unrecognized by the "
			"implementation.");
		break;
	case GSS_S_BAD_NAME:
		str = _("The provided target_name parameter was ill-formed.");
		break;
	case GSS_S_BAD_NAMETYPE:
		str = _("The provided target_name parameter contained an "
			"invalid or unsupported type of name.");
		break;
	case GSS_S_BAD_BINDINGS:
		str = _("The input_token contains different channel "
			"bindings to those specified via the "
			"input_chan_bindings parameter.");
		break;
	case GSS_S_BAD_SIG:
		str = _("The input_token contains an invalid signature, or a "
			"signature that could not be verified.");
		break;
	case GSS_S_NO_CRED:
		str = _("The supplied credentials were not valid for context "
			"initiation, or the credential handle did not "
			"reference any credentials.");
		break;
	case GSS_S_NO_CONTEXT:
		str = _("The supplied context handle did not refer to a valid context.");
		break;
	case GSS_S_DEFECTIVE_TOKEN:
		str = _("The consistency checks performed on the input_token failed.");
		break;
	case GSS_S_DEFECTIVE_CREDENTIAL:
		str = _("The consistency checks performed on the credential failed.");
		break;
	case GSS_S_CREDENTIALS_EXPIRED:
		str = _("The referenced credentials have expired.");
		break;
	case GSS_S_FAILURE:
		str = error_message (minor);
		break;
	default:
		str = _("Bad authentication response from server.");
	}

	g_set_error (
		error, CAMEL_SERVICE_ERROR,
		CAMEL_SERVICE_ERROR_CANT_AUTHENTICATE,
		"%s", str);
}

static void
sasl_gssapi_finalize (GObject *object)
{
	CamelSaslGssapi *sasl = CAMEL_SASL_GSSAPI (object);
	guint32 status;

	if (sasl->priv->ctx != GSS_C_NO_CONTEXT)
		gss_delete_sec_context (
			&status, &sasl->priv->ctx, GSS_C_NO_BUFFER);

	if (sasl->priv->target != GSS_C_NO_NAME)
		gss_release_name (&status, &sasl->priv->target);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (camel_sasl_gssapi_parent_class)->finalize (object);
}

/* DBUS Specific code */

static gboolean
send_dbus_message (gchar *name)
{
	gint success = FALSE;
	GError *error = NULL;
	GDBusConnection *connection;
	GDBusMessage *message, *reply;

	connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
	if (error) {
		g_warning ("could not get system bus: %s\n", error->message);
		g_error_free (error);

		return FALSE;
	}

	g_dbus_connection_set_exit_on_close (connection, FALSE);

	/* Create a new message on the DBUS_INTERFACE */
	message = g_dbus_message_new_method_call (DBUS_INTERFACE, DBUS_PATH, DBUS_INTERFACE, "acquireTgt");
	if (!message) {
		g_object_unref (connection);
		return FALSE;
	}

	/* Appends the data as an argument to the message */
	if (strchr (name, '\\'))
		name = strchr (name, '\\');
	g_dbus_message_set_body (message, g_variant_new ("(s)", name));

	/* Sends the message: Have a 300 sec wait timeout  */
	reply = g_dbus_connection_send_message_with_reply_sync (connection, message, G_DBUS_SEND_MESSAGE_FLAGS_NONE, 300 * 1000, NULL, NULL, &error);

	if (error) {
		g_warning ("%s: %s\n", G_STRFUNC, error->message);
		g_error_free (error);
	}

        if (reply) {
		GVariant *body = g_dbus_message_get_body (reply);

		success = body && g_variant_get_boolean (body);

                g_object_unref (reply);
        }

	/* Free the message */
	g_object_unref (message);
	g_object_unref (connection);

	return success;
}

/* END DBus stuff */

static GByteArray *
sasl_gssapi_challenge (CamelSasl *sasl,
                       GByteArray *token,
                       GError **error)
{
	CamelSaslGssapiPrivate *priv;
	CamelService *service;
	OM_uint32 major, minor, flags, time;
	gss_buffer_desc inbuf, outbuf;
	GByteArray *challenge = NULL;
	gss_buffer_t input_token;
	gint conf_state;
	gss_qop_t qop;
	gss_OID mech;
	gchar *str;
	struct addrinfo *ai, hints;
	const gchar *service_name;

	priv = CAMEL_SASL_GSSAPI_GET_PRIVATE (sasl);

	service = camel_sasl_get_service (sasl);
	service_name = camel_sasl_get_service_name (sasl);

	switch (priv->state) {
	case GSSAPI_STATE_INIT:
		memset(&hints, 0, sizeof(hints));
		hints.ai_flags = AI_CANONNAME;
		ai = camel_getaddrinfo(service->url->host?service->url->host:"localhost", NULL, &hints, error);
		if (ai == NULL)
			return NULL;

		str = g_strdup_printf("%s@%s", service_name, ai->ai_canonname);
		camel_freeaddrinfo(ai);

		inbuf.value = str;
		inbuf.length = strlen (str);
		major = gss_import_name (&minor, &inbuf, GSS_C_NT_HOSTBASED_SERVICE, &priv->target);
		g_free (str);

		if (major != GSS_S_COMPLETE) {
			gssapi_set_exception (major, minor, error);
			return NULL;
		}

		input_token = GSS_C_NO_BUFFER;

		goto challenge;
		break;
	case GSSAPI_STATE_CONTINUE_NEEDED:
		if (token == NULL) {
			g_set_error (
				error, CAMEL_SERVICE_ERROR,
				CAMEL_SERVICE_ERROR_CANT_AUTHENTICATE,
				_("Bad authentication response from server."));
			return NULL;
		}

		inbuf.value = token->data;
		inbuf.length = token->len;
		input_token = &inbuf;

	challenge:
		major = gss_init_sec_context (&minor, GSS_C_NO_CREDENTIAL, &priv->ctx, priv->target,
					      GSS_C_OID_KRBV5_DES, GSS_C_MUTUAL_FLAG |
					      GSS_C_REPLAY_FLAG | GSS_C_SEQUENCE_FLAG,
					      0, GSS_C_NO_CHANNEL_BINDINGS,
					      input_token, &mech, &outbuf, &flags, &time);

		switch (major) {
		case GSS_S_COMPLETE:
			priv->state = GSSAPI_STATE_COMPLETE;
			break;
		case GSS_S_CONTINUE_NEEDED:
			priv->state = GSSAPI_STATE_CONTINUE_NEEDED;
			break;
		default:
			if (major == (OM_uint32)GSS_S_FAILURE &&
			    (minor == (OM_uint32)KRB5KRB_AP_ERR_TKT_EXPIRED ||
			     minor == (OM_uint32)KRB5KDC_ERR_NEVER_VALID) &&
			    send_dbus_message (service->url->user))
					goto challenge;

			gssapi_set_exception (major, minor, error);
			return NULL;
		}

		challenge = g_byte_array_new ();
		g_byte_array_append (challenge, outbuf.value, outbuf.length);
#ifndef HAVE_HEIMDAL_KRB5
		gss_release_buffer (&minor, &outbuf);
#endif
		break;
	case GSSAPI_STATE_COMPLETE:
		if (token == NULL) {
			g_set_error (
				error, CAMEL_SERVICE_ERROR,
				CAMEL_SERVICE_ERROR_CANT_AUTHENTICATE,
				_("Bad authentication response from server."));
			return NULL;
		}

		inbuf.value = token->data;
		inbuf.length = token->len;

		major = gss_unwrap (&minor, priv->ctx, &inbuf, &outbuf, &conf_state, &qop);
		if (major != GSS_S_COMPLETE) {
			gssapi_set_exception (major, minor, error);
			return NULL;
		}

		if (outbuf.length < 4) {
			g_set_error (
				error, CAMEL_SERVICE_ERROR,
				CAMEL_SERVICE_ERROR_CANT_AUTHENTICATE,
				_("Bad authentication response from server."));
#ifndef HAVE_HEIMDAL_KRB5
			gss_release_buffer (&minor, &outbuf);
#endif
			return NULL;
		}

		/* check that our desired security layer is supported */
		if ((((guchar *) outbuf.value)[0] & DESIRED_SECURITY_LAYER) != DESIRED_SECURITY_LAYER) {
			g_set_error (
				error, CAMEL_SERVICE_ERROR,
				CAMEL_SERVICE_ERROR_CANT_AUTHENTICATE,
				_("Unsupported security layer."));
#ifndef HAVE_HEIMDAL_KRB5
			gss_release_buffer (&minor, &outbuf);
#endif
			return NULL;
		}

		inbuf.length = 4 + strlen (service->url->user);
		inbuf.value = str = g_malloc (inbuf.length);
		memcpy (inbuf.value, outbuf.value, 4);
		str[0] = DESIRED_SECURITY_LAYER;
		memcpy (str + 4, service->url->user, inbuf.length - 4);

#ifndef HAVE_HEIMDAL_KRB5
		gss_release_buffer (&minor, &outbuf);
#endif

		major = gss_wrap (&minor, priv->ctx, FALSE, qop, &inbuf, &conf_state, &outbuf);
		if (major != GSS_S_COMPLETE) {
			gssapi_set_exception (major, minor, error);
			g_free (str);
			return NULL;
		}

		g_free (str);
		challenge = g_byte_array_new ();
		g_byte_array_append (challenge, outbuf.value, outbuf.length);

#ifndef HAVE_HEIMDAL_KRB5
		gss_release_buffer (&minor, &outbuf);
#endif

		priv->state = GSSAPI_STATE_AUTHENTICATED;

		camel_sasl_set_authenticated (sasl, TRUE);
		break;
	default:
		return NULL;
	}

	return challenge;
}

#endif /* HAVE_KRB5 */

static void
camel_sasl_gssapi_class_init (CamelSaslGssapiClass *class)
{
#ifdef HAVE_KRB5
	GObjectClass *object_class;
	CamelSaslClass *sasl_class;

	g_type_class_add_private (class, sizeof (CamelSaslGssapiPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = sasl_gssapi_finalize;

	sasl_class = CAMEL_SASL_CLASS (class);
	sasl_class->challenge = sasl_gssapi_challenge;
#endif /* HAVE_KRB5 */
}

static void
camel_sasl_gssapi_init (CamelSaslGssapi *sasl)
{
#ifdef HAVE_KRB5
	sasl->priv = CAMEL_SASL_GSSAPI_GET_PRIVATE (sasl);

	sasl->priv->state = GSSAPI_STATE_INIT;
	sasl->priv->ctx = GSS_C_NO_CONTEXT;
	sasl->priv->target = GSS_C_NO_NAME;
#endif /* HAVE_KRB5 */
}
