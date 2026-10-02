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

#include <stdlib.h>
#include <string.h>

#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include "camel-debug.h"
#include "camel-session.h"
#include "camel-url.h"

#ifdef G_OS_WIN32
#include <windows.h>
#endif
#include "camel-mime-utils.h"
#include "camel-sasl-cram-md5.h"
#include "camel-sasl-digest-md5.h"
#include "camel-sasl-gssapi.h"
#include "camel-sasl-login.h"
#include "camel-sasl-ntlm.h"
#include "camel-sasl-plain.h"
#include "camel-sasl-popb4smtp.h"
#include "camel-sasl.h"
#include "camel-service.h"

#define w(x)

#define CAMEL_SASL_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), CAMEL_TYPE_SASL, CamelSaslPrivate))

struct _CamelSaslPrivate {
	CamelService *service;
	gboolean authenticated;
	gchar *service_name;
	gchar *mechanism;
};

enum {
	PROP_0,
	PROP_AUTHENTICATED,
	PROP_MECHANISM,
	PROP_SERVICE,
	PROP_SERVICE_NAME
};

G_DEFINE_ABSTRACT_TYPE (CamelSasl, camel_sasl, CAMEL_TYPE_OBJECT)

static void
sasl_set_mechanism (CamelSasl *sasl,
                    const gchar *mechanism)
{
	g_return_if_fail (mechanism != NULL);
	g_return_if_fail (sasl->priv->mechanism == NULL);

	sasl->priv->mechanism = g_strdup (mechanism);
}

static void
sasl_set_service (CamelSasl *sasl,
                  CamelService *service)
{
	g_return_if_fail (CAMEL_IS_SERVICE (service));
	g_return_if_fail (sasl->priv->service == NULL);

	sasl->priv->service = g_object_ref (service);
}

static void
sasl_set_service_name (CamelSasl *sasl,
                       const gchar *service_name)
{
	g_return_if_fail (service_name != NULL);
	g_return_if_fail (sasl->priv->service_name == NULL);

	sasl->priv->service_name = g_strdup (service_name);
}

static void
sasl_set_property (GObject *object,
                   guint property_id,
                   const GValue *value,
                   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_AUTHENTICATED:
			camel_sasl_set_authenticated (
				CAMEL_SASL (object),
				g_value_get_boolean (value));
			return;

		case PROP_MECHANISM:
			sasl_set_mechanism (
				CAMEL_SASL (object),
				g_value_get_string (value));
			return;

		case PROP_SERVICE:
			sasl_set_service (
				CAMEL_SASL (object),
				g_value_get_object (value));
			return;

		case PROP_SERVICE_NAME:
			sasl_set_service_name (
				CAMEL_SASL (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
sasl_get_property (GObject *object,
                   guint property_id,
                   GValue *value,
                   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_AUTHENTICATED:
			g_value_set_boolean (
				value, camel_sasl_get_authenticated (
				CAMEL_SASL (object)));
			return;

		case PROP_MECHANISM:
			g_value_set_string (
				value, camel_sasl_get_mechanism (
				CAMEL_SASL (object)));
			return;

		case PROP_SERVICE:
			g_value_set_object (
				value, camel_sasl_get_service (
				CAMEL_SASL (object)));
			return;

		case PROP_SERVICE_NAME:
			g_value_set_string (
				value, camel_sasl_get_service_name (
				CAMEL_SASL (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
sasl_dispose (GObject *object)
{
	CamelSaslPrivate *priv;

	priv = CAMEL_SASL_GET_PRIVATE (object);

	if (priv->service != NULL) {
		g_object_unref (priv->service);
		priv->service = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (camel_sasl_parent_class)->dispose (object);
}

static void
sasl_finalize (GObject *object)
{
	CamelSaslPrivate *priv;

	priv = CAMEL_SASL_GET_PRIVATE (object);

	g_free (priv->mechanism);
	g_free (priv->service_name);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (camel_sasl_parent_class)->finalize (object);
}

static void
camel_sasl_class_init (CamelSaslClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (CamelSaslPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = sasl_set_property;
	object_class->get_property = sasl_get_property;
	object_class->dispose = sasl_dispose;
	object_class->finalize = sasl_finalize;

	g_object_class_install_property (
		object_class,
		PROP_AUTHENTICATED,
		g_param_spec_boolean (
			"authenticated",
			"Authenticated",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_MECHANISM,
		g_param_spec_string (
			"mechanism",
			"Mechanism",
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (
		object_class,
		PROP_SERVICE,
		g_param_spec_object (
			"service",
			"Service",
			NULL,
			CAMEL_TYPE_SERVICE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (
		object_class,
		PROP_SERVICE_NAME,
		g_param_spec_string (
			"service-name",
			"Service Name",
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));
}

static void
camel_sasl_init (CamelSasl *sasl)
{
	sasl->priv = CAMEL_SASL_GET_PRIVATE (sasl);
}

/**
 * camel_sasl_challenge:
 * @sasl: a #CamelSasl object
 * @token: a token, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * If @token is %NULL, generate the initial SASL message to send to
 * the server. (This will be %NULL if the client doesn't initiate the
 * exchange.) Otherwise, @token is a challenge from the server, and
 * the return value is the response.
 *
 * Returns: the SASL response or %NULL. If an error occurred, @ex will
 * also be set.
 **/
GByteArray *
camel_sasl_challenge (CamelSasl *sasl,
                      GByteArray *token,
                      GError **error)
{
	CamelSaslClass *class;
	GByteArray *response;

	g_return_val_if_fail (CAMEL_IS_SASL (sasl), NULL);

	class = CAMEL_SASL_GET_CLASS (sasl);
	g_return_val_if_fail (class->challenge != NULL, NULL);

	response = class->challenge (sasl, token, error);
	if (token)
		CAMEL_CHECK_GERROR (sasl, challenge, response != NULL, error);

	return response;
}

/**
 * camel_sasl_challenge_base64:
 * @sasl: a #CamelSasl object
 * @token: a base64-encoded token
 * @error: return location for a #GError, or %NULL
 *
 * As with #camel_sasl_challenge, but the challenge @token and the
 * response are both base64-encoded.
 *
 * Returns: the base64 encoded challenge string
 **/
gchar *
camel_sasl_challenge_base64 (CamelSasl *sasl,
                             const gchar *token,
                             GError **error)
{
	GByteArray *token_binary, *ret_binary;
	gchar *ret;

	g_return_val_if_fail (CAMEL_IS_SASL (sasl), NULL);

	if (token && *token) {
		guchar *data;
		gsize length = 0;

		data = g_base64_decode (token, &length);
		token_binary = g_byte_array_new ();
		g_byte_array_append (token_binary, data, length);
		g_free (data);
	} else
		token_binary = NULL;

	ret_binary = camel_sasl_challenge (sasl, token_binary, error);
	if (token_binary)
		g_byte_array_free (token_binary, TRUE);
	if (!ret_binary)
		return NULL;

	if (ret_binary->len > 0)
		ret = g_base64_encode (ret_binary->data, ret_binary->len);
	else
		ret = g_strdup ("");
	g_byte_array_free (ret_binary, TRUE);

	return ret;
}

/*
 * XOAUTH2 for Microsoft Outlook / Office 365.
 *
 * The client id is the published Thunderbird desktop id. There is no
 * client secret. The refresh token is a file named oauth2-refresh in
 * the service storage directory. It is not put in the account URL and
 * not stored through camel_session_get_password(), because that key
 * ignores the item name and would collide with the mail password.
 *
 * IMAP and SMTP each have their own storage directory, so each one
 * asks the user to sign in the first time it connects.
 */

#define XOAUTH2_CLIENT_ID "9e5f94bc-e8a4-4e73-b8be-63364c29d753"
#define XOAUTH2_SCOPE "https://outlook.office.com/IMAP.AccessAsUser.All https://outlook.office.com/POP.AccessAsUser.All https://outlook.office.com/SMTP.Send offline_access"
#define XOAUTH2_HOST "login.microsoftonline.com"
#define XOAUTH2_DEVICE_PATH "/common/oauth2/v2.0/devicecode"
#define XOAUTH2_TOKEN_PATH "/common/oauth2/v2.0/token"

typedef struct _CamelSaslXoauth2 CamelSaslXoauth2;
typedef struct _CamelSaslXoauth2Class CamelSaslXoauth2Class;

struct _CamelSaslXoauth2 {
	CamelSasl parent;
};

struct _CamelSaslXoauth2Class {
	CamelSaslClass parent_class;
};

GType camel_sasl_xoauth2_get_type (void);

static CamelServiceAuthType camel_sasl_xoauth2_authtype = {
	N_("OAuth2"),
	N_("This option signs in to Microsoft Outlook with OAuth2."),
	"XOAUTH2",
	FALSE
};

G_DEFINE_TYPE (CamelSaslXoauth2, camel_sasl_xoauth2, CAMEL_TYPE_SASL)

static const gchar *
json_value_at (const gchar *json, const gchar *key)
{
	gchar pattern[80];
	const gchar *p;
	gsize keylen;

	if (json == NULL || key == NULL)
		return NULL;

	keylen = strlen (key);
	if (keylen + 3 >= sizeof (pattern))
		return NULL;

	pattern[0] = '"';
	memcpy (pattern + 1, key, keylen);
	pattern[keylen + 1] = '"';
	pattern[keylen + 2] = '\0';

	p = strstr (json, pattern);
	if (p == NULL)
		return NULL;

	p += keylen + 2;
	while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
		p++;
	if (*p != ':')
		return NULL;
	p++;
	while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
		p++;

	return p;
}

static gchar *
json_get_string (const gchar *json, const gchar *key)
{
	const gchar *p;
	GString *out;

	p = json_value_at (json, key);
	if (p == NULL || *p != '"')
		return NULL;

	p++;
	out = g_string_new (NULL);
	while (*p != '\0' && *p != '"') {
		if (*p == '\\' && p[1] != '\0') {
			p++;
			if (*p == 'n')
				g_string_append_c (out, '\n');
			else if (*p == 'u' &&
				 g_ascii_isxdigit ((gchar) p[1]) &&
				 g_ascii_isxdigit ((gchar) p[2]) &&
				 g_ascii_isxdigit ((gchar) p[3]) &&
				 g_ascii_isxdigit ((gchar) p[4])) {
				guint code;

				code = (guint) ((g_ascii_xdigit_value ((gchar) p[1]) << 12) |
					(g_ascii_xdigit_value ((gchar) p[2]) << 8) |
					(g_ascii_xdigit_value ((gchar) p[3]) << 4) |
					g_ascii_xdigit_value ((gchar) p[4]));
				if (code >= 32 && code < 127)
					g_string_append_c (out, (gchar) code);
				p += 4;
			} else
				g_string_append_c (out, *p);
		} else
			g_string_append_c (out, *p);
		p++;
	}

	return g_string_free (out, FALSE);
}

static gint
json_get_int (const gchar *json, const gchar *key, gint fallback)
{
	const gchar *p;
	gchar *end;
	glong value;

	p = json_value_at (json, key);
	if (p == NULL)
		return fallback;
	if (*p != '-' && !g_ascii_isdigit (*p))
		return fallback;

	value = strtol (p, &end, 10);
	if (end == (gchar *) p)
		return fallback;

	return (gint) value;
}

#ifdef G_OS_WIN32

static gchar *
xoauth2_http_post (const gchar *path, const gchar *body, GError **error)
{
	typedef void * (WINAPI *EvoHttpOpen) (const WCHAR *, DWORD, const WCHAR *, const WCHAR *, DWORD);
	typedef void * (WINAPI *EvoHttpConnect) (void *, const WCHAR *, unsigned short, DWORD);
	typedef void * (WINAPI *EvoHttpOpenRequest) (void *, const WCHAR *, const WCHAR *, const WCHAR *, const WCHAR *, const WCHAR **, DWORD);
	typedef int (WINAPI *EvoHttpSend) (void *, const WCHAR *, DWORD, void *, DWORD, DWORD, DWORD_PTR);
	typedef int (WINAPI *EvoHttpReceive) (void *, void *);
	typedef int (WINAPI *EvoHttpQuery) (void *, DWORD *);
	typedef int (WINAPI *EvoHttpRead) (void *, void *, DWORD, DWORD *);
	typedef int (WINAPI *EvoHttpClose) (void *);
	typedef int (WINAPI *EvoHttpSetOption) (void *, DWORD, void *, DWORD);
	HMODULE module = NULL;
	void *session = NULL;
	void *connect = NULL;
	void *request = NULL;
	WCHAR *whost = NULL;
	WCHAR *wpath = NULL;
	EvoHttpOpen p_open = NULL;
	EvoHttpConnect p_connect = NULL;
	EvoHttpOpenRequest p_open_request = NULL;
	EvoHttpSend p_send = NULL;
	EvoHttpReceive p_receive = NULL;
	EvoHttpQuery p_query = NULL;
	EvoHttpRead p_read = NULL;
	EvoHttpClose p_close = NULL;
	EvoHttpSetOption p_setopt = NULL;
	DWORD protocols;
	DWORD available;
	DWORD nread;
	GString *response = NULL;
	gchar buf[2048];
	gchar *result = NULL;
	gchar *errtxt;
	const gchar *payload;

	payload = body != NULL ? body : "";

	module = LoadLibraryW (L"winhttp.dll");
	if (module == NULL)
		goto winerr;

	p_open = (EvoHttpOpen) GetProcAddress (module, "WinHttpOpen");
	p_connect = (EvoHttpConnect) GetProcAddress (module, "WinHttpConnect");
	p_open_request = (EvoHttpOpenRequest) GetProcAddress (module, "WinHttpOpenRequest");
	p_send = (EvoHttpSend) GetProcAddress (module, "WinHttpSendRequest");
	p_receive = (EvoHttpReceive) GetProcAddress (module, "WinHttpReceiveResponse");
	p_query = (EvoHttpQuery) GetProcAddress (module, "WinHttpQueryDataAvailable");
	p_read = (EvoHttpRead) GetProcAddress (module, "WinHttpReadData");
	p_close = (EvoHttpClose) GetProcAddress (module, "WinHttpCloseHandle");
	p_setopt = (EvoHttpSetOption) GetProcAddress (module, "WinHttpSetOption");
	if (p_open == NULL || p_connect == NULL || p_open_request == NULL ||
	    p_send == NULL || p_receive == NULL || p_query == NULL ||
	    p_read == NULL || p_close == NULL) {
		g_set_error (
			error, CAMEL_SERVICE_ERROR,
			CAMEL_SERVICE_ERROR_UNAVAILABLE,
			_("winhttp.dll is missing the calls needed to sign in."));
		goto cleanup;
	}

	whost = (WCHAR *) g_utf8_to_utf16 (XOAUTH2_HOST, -1, NULL, NULL, NULL);
	wpath = (WCHAR *) g_utf8_to_utf16 (path, -1, NULL, NULL, NULL);
	if (whost == NULL || wpath == NULL) {
		g_set_error (
			error, CAMEL_SERVICE_ERROR,
			CAMEL_SERVICE_ERROR_UNAVAILABLE,
			_("Could not encode the Microsoft login URL."));
		goto cleanup;
	}

	session = p_open (L"Evolution", 0, NULL, NULL, 0);
	if (session == NULL)
		goto winerr;

	connect = p_connect (session, whost, 443, 0);
	if (connect == NULL)
		goto winerr;

	/* 0x00800000 is WINHTTP_FLAG_SECURE. */
	request = p_open_request (connect, L"POST", wpath, NULL, NULL, NULL, 0x00800000);
	if (request == NULL)
		goto winerr;

	/* 84 is WINHTTP_OPTION_SECURE_PROTOCOLS, 0x800 is TLS 1.2. */
	if (p_setopt != NULL) {
		protocols = 0x00000800;
		p_setopt (request, 84, &protocols, sizeof (protocols));
	}

	if (!p_send (request,
		     L"Content-Type: application/x-www-form-urlencoded\r\n",
		     (DWORD) -1,
		     (void *) payload,
		     (DWORD) strlen (payload),
		     (DWORD) strlen (payload),
		     0))
		goto winerr;
	if (!p_receive (request, NULL))
		goto winerr;

	response = g_string_new (NULL);
	for (;;) {
		available = 0;
		if (!p_query (request, &available))
			goto winerr;
		if (available == 0)
			break;
		if (available > sizeof (buf))
			available = (DWORD) sizeof (buf);
		nread = 0;
		if (!p_read (request, buf, available, &nread) || nread == 0)
			break;
		g_string_append_len (response, buf, (gssize) nread);
		if (response->len > 262144)
			break;
	}

	result = g_string_free (response, FALSE);
	response = NULL;
	goto cleanup;

 winerr:
	errtxt = g_win32_error_message (GetLastError ());
	g_set_error (
		error, CAMEL_SERVICE_ERROR,
		CAMEL_SERVICE_ERROR_UNAVAILABLE,
		_("Microsoft login request failed: %s"),
		errtxt != NULL ? errtxt : _("unknown Windows error"));
	g_free (errtxt);

 cleanup:
	if (response != NULL)
		g_string_free (response, TRUE);
	if (request != NULL && p_close != NULL)
		p_close (request);
	if (connect != NULL && p_close != NULL)
		p_close (connect);
	if (session != NULL && p_close != NULL)
		p_close (session);
	if (module != NULL)
		FreeLibrary (module);
	g_free (whost);
	g_free (wpath);

	return result;
}

static void
xoauth2_open_url (const gchar *url)
{
	typedef void * (WINAPI *EvoShellExecuteA) (void *, const char *, const char *, const char *, const char *, int);
	HMODULE shell;
	EvoShellExecuteA execute;

	if (url == NULL || url[0] == '\0')
		return;

	shell = LoadLibraryW (L"shell32.dll");
	if (shell == NULL)
		return;

	execute = (EvoShellExecuteA) GetProcAddress (shell, "ShellExecuteA");
	if (execute != NULL)
		execute (NULL, "open", url, NULL, NULL, 1);
	FreeLibrary (shell);
}

#else

static gchar *
xoauth2_http_post (const gchar *path, const gchar *body, GError **error)
{
	(void) path;
	(void) body;
	g_set_error (
		error, CAMEL_SERVICE_ERROR,
		CAMEL_SERVICE_ERROR_UNAVAILABLE,
		_("Microsoft OAuth2 is only available in the Windows build."));
	return NULL;
}

static void
xoauth2_open_url (const gchar *url)
{
	(void) url;
}

#endif

static gboolean
xoauth2_microsoft_host (const gchar *host)
{
	gchar *fold;
	gboolean ok;

	if (host == NULL || host[0] == '\0')
		return FALSE;

	fold = g_ascii_strdown (host, -1);
	ok = g_str_has_suffix (fold, ".office365.com") ||
		g_str_has_suffix (fold, ".outlook.com") ||
		g_str_has_suffix (fold, ".office.com") ||
		g_str_has_suffix (fold, ".live.com") ||
		strcmp (fold, "outlook.com") == 0 ||
		strcmp (fold, "outlook.office365.com") == 0 ||
		strcmp (fold, "smtp.office365.com") == 0;
	g_free (fold);

	return ok;
}

static gchar *
xoauth2_take_token (const gchar *json, const gchar *refresh_path, gboolean *redo, GError **error)
{
	gchar *access;
	gchar *refresh;
	gchar *err;
	gchar *desc;

	if (redo != NULL)
		*redo = FALSE;

	access = json_get_string (json, "access_token");
	if (access != NULL && access[0] != '\0') {
		refresh = json_get_string (json, "refresh_token");
		if (refresh != NULL && refresh[0] != '\0' && refresh_path != NULL)
			g_file_set_contents (refresh_path, refresh, -1, NULL);
		g_free (refresh);
		return access;
	}
	g_free (access);

	err = json_get_string (json, "error");
	if (err != NULL &&
	    (strcmp (err, "invalid_grant") == 0 ||
	     strcmp (err, "interaction_required") == 0)) {
		if (redo != NULL)
			*redo = TRUE;
		g_free (err);
		return NULL;
	}

	desc = json_get_string (json, "error_description");
	g_set_error (
		error, CAMEL_SERVICE_ERROR,
		CAMEL_SERVICE_ERROR_CANT_AUTHENTICATE,
		_("Microsoft login failed: %s%s%s"),
		err != NULL ? err : _("unknown error"),
		desc != NULL ? ": " : "",
		desc != NULL ? desc : "");
	g_free (err);
	g_free (desc);

	return NULL;
}

static gchar *
xoauth2_form (const gchar *client_encoded, const gchar *grant, const gchar *extra_key, const gchar *extra_value, gboolean with_scope)
{
	gchar *grant_enc;
	gchar *extra_enc;
	gchar *scope_enc;
	gchar *body;

	grant_enc = NULL;
	extra_enc = NULL;
	scope_enc = NULL;
	if (grant != NULL)
		grant_enc = camel_url_encode (grant, NULL);
	if (extra_value != NULL)
		extra_enc = camel_url_encode (extra_value, NULL);
	if (with_scope)
		scope_enc = camel_url_encode (XOAUTH2_SCOPE, NULL);

	if (extra_enc != NULL && scope_enc != NULL)
		body = g_strdup_printf (
			"client_id=%s&grant_type=%s&%s=%s&scope=%s",
			client_encoded, grant_enc, extra_key, extra_enc, scope_enc);
	else if (extra_enc != NULL)
		body = g_strdup_printf (
			"client_id=%s&grant_type=%s&%s=%s",
			client_encoded, grant_enc, extra_key, extra_enc);
	else if (scope_enc != NULL)
		body = g_strdup_printf (
			"client_id=%s&scope=%s",
			client_encoded, scope_enc);
	else
		body = g_strdup_printf ("client_id=%s", client_encoded);

	g_free (grant_enc);
	g_free (extra_enc);
	g_free (scope_enc);

	return body;
}

static gchar *
xoauth2_refresh (const gchar *refresh, const gchar *refresh_path, gboolean *redo, GError **error)
{
	gchar *client_enc;
	gchar *body;
	gchar *json;
	gchar *access;

	client_enc = camel_url_encode (XOAUTH2_CLIENT_ID, NULL);
	body = xoauth2_form (client_enc, "refresh_token", "refresh_token", refresh, TRUE);
	g_free (client_enc);

	json = xoauth2_http_post (XOAUTH2_TOKEN_PATH, body, error);
	g_free (body);
	if (json == NULL)
		return NULL;

	access = xoauth2_take_token (json, refresh_path, redo, error);
	g_free (json);

	return access;
}

static gchar *
xoauth2_device (CamelService *service, const gchar *refresh_path, GError **error)
{
	CamelSession *session;
	gchar *client_enc;
	gchar *body;
	gchar *json;
	gchar *user_code;
	gchar *device_code;
	gchar *uri;
	gchar *message;
	gchar *prompt;
	gchar *access;
	gchar *err;
	gint interval;
	gint expires;
	gint elapsed;
	gboolean redo;

	session = camel_service_get_session (service);
	client_enc = camel_url_encode (XOAUTH2_CLIENT_ID, NULL);
	body = xoauth2_form (client_enc, NULL, NULL, NULL, TRUE);
	g_free (client_enc);

	json = xoauth2_http_post (XOAUTH2_DEVICE_PATH, body, error);
	g_free (body);
	if (json == NULL)
		return NULL;

	user_code = json_get_string (json, "user_code");
	device_code = json_get_string (json, "device_code");
	uri = json_get_string (json, "verification_uri");
	if (uri == NULL)
		uri = json_get_string (json, "verification_url");
	message = json_get_string (json, "message");
	interval = json_get_int (json, "interval", 5);
	expires = json_get_int (json, "expires_in", 180);

	if (user_code == NULL || device_code == NULL || uri == NULL) {
		access = xoauth2_take_token (json, refresh_path, NULL, error);
		g_free (json);
		g_free (user_code);
		g_free (device_code);
		g_free (uri);
		g_free (message);
		g_free (access);
		return NULL;
	}
	g_free (json);

	if (interval < 5)
		interval = 5;
	if (interval > 30)
		interval = 30;
	if (expires < 30)
		expires = 30;
	if (expires > 900)
		expires = 900;

	xoauth2_open_url (uri);

	prompt = g_strdup_printf (
		_("Sign in with Microsoft so Evolution can read and send Outlook mail.\n\n"
		  "A browser window should have opened. If it did not, go to:\n%s\n\n"
		  "Enter this code:\n%s\n\n"
		  "%s\n"
		  "Click OK after you finish signing in. Click Cancel to stop."),
		uri,
		user_code,
		message != NULL ? message : "");

	if (session == NULL ||
	    !camel_session_alert_user (session, CAMEL_SESSION_ALERT_INFO, prompt, TRUE)) {
		g_set_error (
			error, G_IO_ERROR,
			G_IO_ERROR_CANCELLED,
			_("Microsoft sign-in was cancelled."));
		g_free (prompt);
		g_free (user_code);
		g_free (device_code);
		g_free (uri);
		g_free (message);
		return NULL;
	}
	g_free (prompt);
	g_free (user_code);
	g_free (uri);
	g_free (message);

	client_enc = camel_url_encode (XOAUTH2_CLIENT_ID, NULL);
	elapsed = 0;
	access = NULL;

	while (elapsed < expires) {
		body = xoauth2_form (client_enc, "urn:ietf:params:oauth:grant-type:device_code", "device_code", device_code, FALSE);
		json = xoauth2_http_post (XOAUTH2_TOKEN_PATH, body, error);
		g_free (body);
		if (json == NULL)
			break;

		err = json_get_string (json, "error");
		if (err != NULL && strcmp (err, "authorization_pending") == 0) {
			g_free (err);
			g_free (json);
			g_usleep ((gulong) interval * G_USEC_PER_SEC);
			elapsed += interval;
			continue;
		}
		if (err != NULL && strcmp (err, "slow_down") == 0) {
			g_free (err);
			g_free (json);
			interval += 5;
			if (interval > 30)
				interval = 30;
			g_usleep ((gulong) interval * G_USEC_PER_SEC);
			elapsed += interval;
			continue;
		}
		g_free (err);

		redo = FALSE;
		access = xoauth2_take_token (json, refresh_path, &redo, error);
		g_free (json);
		if (access != NULL || !redo)
			break;
		g_clear_error (error);
		g_usleep ((gulong) interval * G_USEC_PER_SEC);
		elapsed += interval;
	}

	g_free (client_enc);
	g_free (device_code);

	if (access == NULL && (error == NULL || *error == NULL))
		g_set_error (
			error, CAMEL_SERVICE_ERROR,
			CAMEL_SERVICE_ERROR_CANT_AUTHENTICATE,
			_("Microsoft sign-in timed out. Try again."));

	return access;
}

static gchar *
xoauth2_acquire (CamelService *service, GError **error)
{
	CamelSession *session;
	CamelURL *url;
	gchar *dir;
	gchar *path;
	gchar *refresh;
	gchar *access;
	gboolean redo;

	url = service->url;
	if (url == NULL || url->user == NULL || url->user[0] == '\0') {
		g_set_error (
			error, CAMEL_SERVICE_ERROR,
			CAMEL_SERVICE_ERROR_CANT_AUTHENTICATE,
			_("OAuth2 needs the account user name to be the full email address."));
		return NULL;
	}

	if (!xoauth2_microsoft_host (url->host)) {
		g_set_error (
			error, CAMEL_SERVICE_ERROR,
			CAMEL_SERVICE_ERROR_CANT_AUTHENTICATE,
			_("OAuth2 in this build signs in to Microsoft Outlook. "
			  "%s is not an Outlook server."),
			url->host != NULL ? url->host : _("(no host)"));
		return NULL;
	}

	session = camel_service_get_session (service);
	if (session == NULL) {
		g_set_error (
			error, CAMEL_SERVICE_ERROR,
			CAMEL_SERVICE_ERROR_UNAVAILABLE,
			_("OAuth2 has no mail session to ask for sign-in."));
		return NULL;
	}

	dir = camel_session_get_storage_path (session, service, error);
	if (dir == NULL)
		return NULL;

	path = g_build_filename (dir, "oauth2-refresh", NULL);
	g_free (dir);

	access = NULL;
	redo = FALSE;
	if (g_file_get_contents (path, &refresh, NULL, NULL)) {
		g_strstrip (refresh);
		if (refresh[0] != '\0')
			access = xoauth2_refresh (refresh, path, &redo, error);
		g_free (refresh);
		if (access != NULL || !redo) {
			g_free (path);
			return access;
		}
		g_clear_error (error);
		g_unlink (path);
	}

	access = xoauth2_device (service, path, error);
	g_free (path);

	return access;
}

static GByteArray *
sasl_xoauth2_challenge (CamelSasl *sasl, GByteArray *token, GError **error)
{
	CamelService *service;
	CamelURL *url;
	gchar *access;
	GByteArray *buf;

	(void) token;

	/* A second continuation is Microsoft's error blob. An empty
	 * SASL response lets SMTP and IMAP finish that handshake. */
	if (camel_sasl_get_authenticated (sasl))
		return g_byte_array_new ();

	service = camel_sasl_get_service (sasl);
	if (service == NULL || service->url == NULL) {
		g_set_error (
			error, CAMEL_SERVICE_ERROR,
			CAMEL_SERVICE_ERROR_CANT_AUTHENTICATE,
			_("OAuth2 has no mail service to authenticate."));
		return NULL;
	}

	url = service->url;
	access = xoauth2_acquire (service, error);
	if (access == NULL)
		return NULL;

	buf = g_byte_array_new ();
	g_byte_array_append (buf, (guint8 *) "user=", 5);
	g_byte_array_append (buf, (guint8 *) url->user, strlen (url->user));
	g_byte_array_append (buf, (guint8 *) "\001auth=Bearer ", 13);
	g_byte_array_append (buf, (guint8 *) access, strlen (access));
	g_byte_array_append (buf, (guint8 *) "\001\001", 2);
	g_free (access);

	camel_sasl_set_authenticated (sasl, TRUE);

	return buf;
}

static void
camel_sasl_xoauth2_class_init (CamelSaslXoauth2Class *class)
{
	CamelSaslClass *sasl_class;

	sasl_class = CAMEL_SASL_CLASS (class);
	sasl_class->challenge = sasl_xoauth2_challenge;
}

static void
camel_sasl_xoauth2_init (CamelSaslXoauth2 *sasl)
{
	(void) sasl;
}

/**
 * camel_sasl_new:
 * @service_name: the SASL service name
 * @mechanism: the SASL mechanism
 * @service: the CamelService that will be using this SASL
 *
 * Returns: a new #CamelSasl object for the given @service_name,
 * @mechanism, and @service, or %NULL if the mechanism is not
 * supported.
 **/
CamelSasl *
camel_sasl_new (const gchar *service_name,
                const gchar *mechanism,
                CamelService *service)
{
	GType type;

	g_return_val_if_fail (service_name != NULL, NULL);
	g_return_val_if_fail (mechanism != NULL, NULL);
	g_return_val_if_fail (CAMEL_IS_SERVICE (service), NULL);

	/* We don't do ANONYMOUS here, because it's a little bit weird. */

	if (!strcmp (mechanism, "CRAM-MD5"))
		type = CAMEL_TYPE_SASL_CRAM_MD5;
	else if (!strcmp (mechanism, "DIGEST-MD5"))
		type = CAMEL_TYPE_SASL_DIGEST_MD5;
#ifdef HAVE_KRB5
	else if (!strcmp (mechanism, "GSSAPI"))
		type = CAMEL_TYPE_SASL_GSSAPI;
#endif
	else if (!strcmp (mechanism, "PLAIN"))
		type = CAMEL_TYPE_SASL_PLAIN;
	else if (!strcmp (mechanism, "LOGIN"))
		type = CAMEL_TYPE_SASL_LOGIN;
	else if (!strcmp (mechanism, "POPB4SMTP"))
		type = CAMEL_TYPE_SASL_POPB4SMTP;
	else if (!strcmp (mechanism, "NTLM"))
		type = CAMEL_TYPE_SASL_NTLM;
	else if (!strcmp (mechanism, "XOAUTH2"))
		type = camel_sasl_xoauth2_get_type ();
	else
		return NULL;

	return g_object_new (
		type, "mechanism", mechanism, "service",
		service, "service-name", service_name, NULL);
}

/**
 * camel_sasl_get_authenticated:
 * @sasl: a #CamelSasl object
 *
 * Returns: whether or not @sasl has successfully authenticated the
 * user. This will be %TRUE after it returns the last needed response.
 * The caller must still pass that information on to the server and
 * verify that it has accepted it.
 **/
gboolean
camel_sasl_get_authenticated (CamelSasl *sasl)
{
	g_return_val_if_fail (CAMEL_IS_SASL (sasl), FALSE);

	return sasl->priv->authenticated;
}

/**
 * camel_sasl_set_authenticated:
 * @sasl: a #CamelSasl
 * @authenticated: whether we have successfully authenticated
 *
 * Since: 2.32
 **/
void
camel_sasl_set_authenticated (CamelSasl *sasl,
                              gboolean authenticated)
{
	g_return_if_fail (CAMEL_IS_SASL (sasl));

	sasl->priv->authenticated = authenticated;

	g_object_notify (G_OBJECT (sasl), "authenticated");
}

/**
 * camel_sasl_get_mechanism:
 * @sasl: a #CamelSasl
 *
 * Since: 2.32
 **/
const gchar *
camel_sasl_get_mechanism (CamelSasl *sasl)
{
	g_return_val_if_fail (CAMEL_IS_SASL (sasl), NULL);

	return sasl->priv->mechanism;
}

/**
 * camel_sasl_get_service:
 * @sasl: a #CamelSasl
 *
 * Since: 2.32
 **/
CamelService *
camel_sasl_get_service (CamelSasl *sasl)
{
	g_return_val_if_fail (CAMEL_IS_SASL (sasl), NULL);

	return sasl->priv->service;
}

/**
 * camel_sasl_get_service_name:
 * @sasl: a #CamelSasl
 *
 * Since: 2.32
 **/
const gchar *
camel_sasl_get_service_name (CamelSasl *sasl)
{
	g_return_val_if_fail (CAMEL_IS_SASL (sasl), NULL);

	return sasl->priv->service_name;
}

/**
 * camel_sasl_authtype_list:
 * @include_plain: whether or not to include the PLAIN mechanism
 *
 * Returns: a #GList of SASL-supported authtypes. The caller must
 * free the list, but not the contents.
 **/
GList *
camel_sasl_authtype_list (gboolean include_plain)
{
	GList *types = NULL;

	types = g_list_prepend (types, &camel_sasl_xoauth2_authtype);
	types = g_list_prepend (types, &camel_sasl_cram_md5_authtype);
	types = g_list_prepend (types, &camel_sasl_digest_md5_authtype);
#ifdef HAVE_KRB5
	types = g_list_prepend (types, &camel_sasl_gssapi_authtype);
#endif
	types = g_list_prepend (types, &camel_sasl_ntlm_authtype);
	if (include_plain)
		types = g_list_prepend (types, &camel_sasl_plain_authtype);

	return types;
}

/**
 * camel_sasl_authtype:
 * @mechanism: the SASL mechanism to get an authtype for
 *
 * Returns: a #CamelServiceAuthType for the given mechanism, if
 * it is supported.
 **/
CamelServiceAuthType *
camel_sasl_authtype (const gchar *mechanism)
{
	if (!strcmp (mechanism, "CRAM-MD5"))
		return &camel_sasl_cram_md5_authtype;
	else if (!strcmp (mechanism, "DIGEST-MD5"))
		return &camel_sasl_digest_md5_authtype;
#ifdef HAVE_KRB5
	else if (!strcmp (mechanism, "GSSAPI"))
		return &camel_sasl_gssapi_authtype;
#endif
	else if (!strcmp (mechanism, "PLAIN"))
		return &camel_sasl_plain_authtype;
	else if (!strcmp (mechanism, "LOGIN"))
		return &camel_sasl_login_authtype;
	else if (!strcmp(mechanism, "POPB4SMTP"))
		return &camel_sasl_popb4smtp_authtype;
	else if (!strcmp (mechanism, "NTLM"))
		return &camel_sasl_ntlm_authtype;
	else if (!strcmp (mechanism, "XOAUTH2"))
		return &camel_sasl_xoauth2_authtype;
	else
		return NULL;
}
