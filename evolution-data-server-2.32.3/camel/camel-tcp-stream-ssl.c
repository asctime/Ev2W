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

/* NOTE: This is the default implementation of CamelTcpStreamSSL,
 * used when the Mozilla NSS libraries are used.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef CAMEL_HAVE_NSS

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <glib.h>
#include <nspr.h>
#include <prio.h>
#include <prerror.h>
#include <prerr.h>
#include <secerr.h>
#include <sslerr.h>
#include "nss.h"    /* Don't use <> here or it will include the system nss.h instead */
#include <ssl.h>
#include <sslt.h>
#include <sslproto.h>
#include <cert.h>
#include <certdb.h>
#include <pk11func.h>

#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include "camel-certdb.h"
#include "camel-file-utils.h"
#include "camel-net-utils.h"
#include "camel-operation.h"
#include "camel-session.h"
#include "camel-stream-fs.h"
#include "camel-tcp-stream-ssl.h"

#ifdef G_OS_WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#define d(x)

#define IO_TIMEOUT (PR_TicksPerSecond() * 4 * 60)
#define CONNECT_TIMEOUT (PR_TicksPerSecond () * 4 * 60)

#define CAMEL_TCP_STREAM_SSL_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), CAMEL_TYPE_TCP_STREAM_SSL, CamelTcpStreamSSLPrivate))

struct _CamelTcpStreamSSLPrivate {
	CamelSession *session;
	gchar *expected_host;
	gboolean ssl_mode;
	guint32 flags;
};

G_DEFINE_TYPE (CamelTcpStreamSSL, camel_tcp_stream_ssl, CAMEL_TYPE_TCP_STREAM_RAW)

static const gchar *
tcp_stream_ssl_get_cert_dir (void)
{
	static gchar *cert_dir = NULL;

	if (G_UNLIKELY (cert_dir == NULL)) {
		const gchar *data_dir;
		/* const gchar *home_dir;
		gchar *old_dir;             */

    /* Just be for upgrade migration? Doesn't work in WIN32 */
		/* home_dir = g_get_home_dir (); */
		data_dir = g_get_user_data_dir ();

		cert_dir = g_build_filename (data_dir, "camel_certs", NULL);

		/* Move the old certificate directory if present. */
		/* old_dir = g_build_filename (home_dir, ".camel_certs", NULL);
		if (g_file_test (old_dir, G_FILE_TEST_IS_DIR))
			g_rename (old_dir, cert_dir);
		g_free (old_dir); */

		g_mkdir_with_parents (cert_dir, 0700);
	}

	return cert_dir;
}

static void
tcp_stream_ssl_dispose (GObject *object)
{
	CamelTcpStreamSSLPrivate *priv;

	priv = CAMEL_TCP_STREAM_SSL_GET_PRIVATE (object);

	if (priv->session != NULL) {
		g_object_unref (priv->session);
		priv->session = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (camel_tcp_stream_ssl_parent_class)->dispose (object);
}

static void
tcp_stream_ssl_finalize (GObject *object)
{
	CamelTcpStreamSSLPrivate *priv;

	priv = CAMEL_TCP_STREAM_SSL_GET_PRIVATE (object);

	g_free (priv->expected_host);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (camel_tcp_stream_ssl_parent_class)->finalize (object);
}


CamelCert *camel_certdb_nss_cert_get(CamelCertDB *certdb, CERTCertificate *cert);
CamelCert *camel_certdb_nss_cert_add(CamelCertDB *certdb, CERTCertificate *cert);
void camel_certdb_nss_cert_set(CamelCertDB *certdb, CamelCert *ccert, CERTCertificate *cert);

static gchar *
cert_fingerprint(CERTCertificate *cert)
{
	GChecksum *checksum;
	guint8 *digest;
	gsize length;
	guchar fingerprint[50], *f;
	gint i;
	const gchar tohex[16] = "0123456789abcdef";

	length = g_checksum_type_get_length (G_CHECKSUM_MD5);
	digest = g_alloca (length);

	checksum = g_checksum_new (G_CHECKSUM_MD5);
	g_checksum_update (checksum, cert->derCert.data, cert->derCert.len);
	g_checksum_get_digest (checksum, digest, &length);
	g_checksum_free (checksum);

	for (i=0,f = fingerprint; i < length; i++) {
		guint c = digest[i];

		*f++ = tohex[(c >> 4) & 0xf];
		*f++ = tohex[c & 0xf];
#ifndef G_OS_WIN32
		*f++ = ':';
#else
		/* The fingerprint is used as a file name, can't have
		 * colons in file names. Use underscore instead.
		 */
		*f++ = '_';
#endif
	}

	fingerprint[47] = 0;

	return g_strdup((gchar *) fingerprint);
}

/* lookup a cert uses fingerprint to index an on-disk file */
CamelCert *
camel_certdb_nss_cert_get (CamelCertDB *certdb,
                           CERTCertificate *cert)
{
	gchar *fingerprint;
	CamelCert *ccert;

	fingerprint = cert_fingerprint (cert);
	ccert = camel_certdb_get_cert (certdb, fingerprint);
	if (ccert == NULL) {
		g_free (fingerprint);
		return NULL;
	}

/*  First try looking in cert_db. Check cert_dir when that fails. 
    Return any existing certifcate but dont trust yet.       */
	if (ccert->rawcert == NULL) {
		GByteArray *array;
		gchar *filename;
		gchar *contents;
		gsize length;
		const gchar *cert_dir;
		GError *error = NULL;

		cert_dir = tcp_stream_ssl_get_cert_dir ();
		filename = g_build_filename (cert_dir, fingerprint, NULL);
    /* filename = g_build_filename (g_get_home_dir (), ".camel_certs", 
      fingerprint, NULL); */
		if (!g_file_get_contents (filename, &contents, &length, &error) ||
		    error != NULL) {
			g_warning (
				"Could not load cert %s: %s",
				filename, error ? error->message : "Unknown error");
			g_clear_error (&error);

			camel_cert_set_trust (
				certdb, ccert, CAMEL_CERT_TRUST_UNKNOWN);
			camel_certdb_touch (certdb);
			g_free (fingerprint);
			g_free (filename);

			return ccert;
		}
		g_free (filename);

		array = g_byte_array_sized_new (length);
		g_byte_array_append (array, (guint8 *) contents, length);
		g_free (contents);

		ccert->rawcert = array;
	}

	g_free (fingerprint);
	if (ccert->rawcert->len != cert->derCert.len
	    || memcmp (ccert->rawcert->data, cert->derCert.data, cert->derCert.len) != 0) {
		g_warning ("rawcert != derCer");
		camel_cert_set_trust (certdb, ccert, CAMEL_CERT_TRUST_UNKNOWN);
		camel_certdb_touch (certdb);
	}

	return ccert;
}

/* add a cert to the certdb */
CamelCert *
camel_certdb_nss_cert_add(CamelCertDB *certdb, CERTCertificate *cert)
{
	CamelCert *ccert;
	gchar *fingerprint;

	fingerprint = cert_fingerprint(cert);

	ccert = camel_certdb_cert_new(certdb);
	camel_cert_set_issuer(certdb, ccert, CERT_NameToAscii(&cert->issuer));
	camel_cert_set_subject(certdb, ccert, CERT_NameToAscii(&cert->subject));
	/* hostname is set in caller */
	/*camel_cert_set_hostname(certdb, ccert, ssl->priv->expected_host);*/
	camel_cert_set_fingerprint(certdb, ccert, fingerprint);
	camel_cert_set_trust(certdb, ccert, CAMEL_CERT_TRUST_UNKNOWN);
	g_free(fingerprint);

/* Future versions handle in ssl_bad_cert            */
	camel_certdb_nss_cert_set(certdb, ccert, cert);
	camel_certdb_add(certdb, ccert);               

	return ccert;
}

/* set the 'raw' cert (& save it) */
void
camel_certdb_nss_cert_set (CamelCertDB *certdb,
                           CamelCert *ccert,
                           CERTCertificate *cert)
{
	gchar *filename, *fingerprint;
	CamelStream *stream;
	const gchar *cert_dir;

	fingerprint = ccert->fingerprint;

	if (ccert->rawcert == NULL)
		ccert->rawcert = g_byte_array_new ();

	g_byte_array_set_size (ccert->rawcert, cert->derCert.len);
	memcpy (ccert->rawcert->data, cert->derCert.data, cert->derCert.len);

	cert_dir = tcp_stream_ssl_get_cert_dir ();
	filename = g_build_filename (cert_dir, fingerprint, NULL);

/* O_BINARY is added by camel_stream_fs_new_with_name */
	stream = camel_stream_fs_new_with_name (
		filename, O_WRONLY | O_CREAT | O_TRUNC, 0600, NULL);
	if (stream != NULL) {
		if (camel_stream_write (
			stream, (const gchar *) ccert->rawcert->data,
			ccert->rawcert->len, NULL) == -1) {
			g_warning (
				"Could not save cert: %s: %s",
				filename, g_strerror (errno));
			g_unlink (filename);
		}
		camel_stream_close (stream, NULL);
		g_object_unref (stream);
	} else {
		g_warning (
			"Could not save cert: %s: %s",
			filename, g_strerror (errno));
	}

	g_free (filename);
}

static SECStatus
ssl_bad_cert (gpointer data, PRFileDesc *sockfd)
{
	gboolean accept;
	CamelCertDB *certdb = NULL;
	CamelCert *ccert = NULL;
	gboolean ccert_is_new = FALSE;
	gchar *prompt, *cert_str, *fingerprint;
	CamelTcpStreamSSL *ssl;
	CERTCertificate *cert;
	SECStatus status = SECFailure;

	g_return_val_if_fail (data != NULL, SECFailure);
	g_return_val_if_fail (CAMEL_IS_TCP_STREAM_SSL (data), SECFailure);

	ssl = data;

	cert = SSL_PeerCertificate (sockfd);
	if (cert == NULL)
		return SECFailure;

	certdb = camel_certdb_get_default();
	ccert = camel_certdb_nss_cert_get(certdb, cert);
	if (ccert == NULL) {
		ccert = camel_certdb_nss_cert_add(certdb, cert);
		camel_cert_set_hostname(certdb, ccert, ssl->priv->expected_host);
		/* Don't put in the certdb yet.  Since we can only store one
		 * entry per hostname, we'd rather not ruin any existing entry
		 * for this hostname if the user rejects the new certificate. */
		ccert_is_new = TRUE;
	}

	if (ccert->trust == CAMEL_CERT_TRUST_UNKNOWN) {
		status = CERT_VerifyCertNow(cert->dbhandle, cert, TRUE, certUsageSSLClient, NULL);
		fingerprint = cert_fingerprint(cert);
		cert_str = g_strdup_printf (_("Issuer:            %s\n"
					      "Subject:           %s\n"
					      "Fingerprint:       %s\n"
					      "Signature:         %s"),
					    CERT_NameToAscii (&cert->issuer),
					    CERT_NameToAscii (&cert->subject),
					    fingerprint, status == SECSuccess?_("GOOD"):_("BAD"));
		g_free(fingerprint);

		/* construct our user prompt */
		prompt = g_strdup_printf (_("SSL Certificate check for %s:\n\n%s\n\nDo you wish to accept?"),
					  ssl->priv->expected_host, cert_str);
		g_free (cert_str);

		/* query the user to find out if we want to accept this certificate */
		accept = camel_session_alert_user (ssl->priv->session, CAMEL_SESSION_ALERT_WARNING, prompt, TRUE);
		g_free(prompt);
		if ((accept) && (ccert_is_new)) {
			camel_certdb_nss_cert_set(certdb, ccert, cert);
			camel_cert_set_trust(certdb, ccert, CAMEL_CERT_TRUST_FULLY);
			camel_certdb_touch(certdb);
		}
	} else {
		accept = ccert->trust != CAMEL_CERT_TRUST_NEVER;
	}

  /* Future versions */
	/* camel_certdb_cert_unref(certdb, ccert);
	camel_certdb_save (certdb);                 */
	g_object_unref (certdb);

	return accept ? SECSuccess : SECFailure;
}

static PRFileDesc *
enable_ssl (CamelTcpStreamSSL *ssl, PRFileDesc *fd)
{
	PRFileDesc *ssl_fd;

/* Redhat patch Evolution 3.10.4 not able to use TLSv1 or higher
 (only SSLv3) */
  static gchar v2_enabled = -1;
#if NSS_VMAJOR > 3 || (NSS_VMAJOR == 3 && NSS_VMINOR >= 14)
	SSLVersionRange versionStreamSup, versionStream;
#endif


	g_assert (fd != NULL);

	ssl_fd = SSL_ImportFD (NULL, fd);
	if (!ssl_fd)
		return NULL;

	SSL_OptionSet (ssl_fd, SSL_SECURITY, PR_TRUE);

	/* check camel.c for the same "CAMEL_SSL_V2_ENABLE" */
	if (v2_enabled == -1) {
		v2_enabled = g_strcmp0 (g_getenv ("CAMEL_SSL_V2_ENABLE"), "1") == 0 ? 1 : 0;
  }

	if (v2_enabled && (ssl->priv->flags & CAMEL_TCP_STREAM_SSL_ENABLE_SSL2) != 0) {
		static gchar v2_hello = -1;

		/* 2023 Many servers with disabled SSL v2 rejects connection when
		 * SSL v2 compatible hello is sent, thus disabled this by default.
		 * After all, TLS 1.2+ should be used in general these days anyway.
		*/
		if (v2_hello == -1)
			v2_hello = g_strcmp0 (g_getenv ("CAMEL_SSL_V2_HELLO"), "1") == 0 ? 1 : 0;

		SSL_OptionSet (ssl_fd, SSL_ENABLE_SSL2, PR_TRUE);
		SSL_OptionSet (ssl_fd, SSL_V2_COMPATIBLE_HELLO, v2_hello ? PR_TRUE : PR_FALSE);
	} else {
		SSL_OptionSet (ssl_fd, SSL_ENABLE_SSL2, PR_FALSE);
		SSL_OptionSet (ssl_fd, SSL_V2_COMPATIBLE_HELLO, PR_FALSE);
	}

	/* Implementation lines for SSL_ENABLE_ALPN and SSL_ENABLE_NPN */
	SSL_OptionSet (ssl_fd, SSL_ENABLE_ALPN, PR_TRUE);
	SSL_OptionSet (ssl_fd, SSL_ENABLE_NPN, PR_TRUE);

/* https://bugzilla.redhat.com/show_bug.cgi?id=1153052 */
#if NSS_VMAJOR < 3 || (NSS_VMAJOR == 3 && NSS_VMINOR < 14)
	if (ssl->priv->flags & CAMEL_TCP_STREAM_SSL_ENABLE_SSL3)
		SSL_OptionSet (ssl_fd, SSL_ENABLE_SSL3, PR_TRUE);
	else
		SSL_OptionSet (ssl_fd, SSL_ENABLE_SSL3, PR_FALSE);

	if (ssl->priv->flags & CAMEL_TCP_STREAM_SSL_ENABLE_TLS)
		SSL_OptionSet (ssl_fd, SSL_ENABLE_TLS, PR_TRUE);
	else
		SSL_OptionSet (ssl_fd, SSL_ENABLE_TLS, PR_FALSE);
#else
 	SSL_VersionRangeGetSupported (ssl_variant_stream, &versionStreamSup);

	if (ssl->priv->flags & CAMEL_TCP_STREAM_SSL_ENABLE_SSL3)
		versionStream.min = SSL_LIBRARY_VERSION_3_0;
	else
		versionStream.min = SSL_LIBRARY_VERSION_TLS_1_0;

	if (ssl->priv->flags & CAMEL_TCP_STREAM_SSL_ENABLE_TLS)
		versionStream.max = versionStreamSup.max;
	else
		versionStream.max = SSL_LIBRARY_VERSION_3_0;

	if (versionStream.max < versionStream.min) {
		PRUint16 tmp;

		tmp = versionStream.max;
		versionStream.max = versionStream.min;
		versionStream.min = tmp;
	}

	SSL_VersionRangeSet (ssl_fd, &versionStream);
#endif

	SSL_SetURL (ssl_fd, ssl->priv->expected_host);

	/* NSS provides _and_ installs a default implementation for the
	 * SSL_AuthCertificateHook callback so we _don't_ need to install one. */
	SSL_BadCertHook (ssl_fd, ssl_bad_cert, ssl);

	/* NSS provides a default implementation for the SSL_GetClientAuthDataHook callback
	 * but does not enable it by default. It must be explicltly requested by the application.
	 * See: http://www.mozilla.org/projects/security/pki/nss/ref/ssl/sslfnc.html#1126622 */
	if (SSL_GetClientAuthDataHook (ssl_fd, (SSLGetClientAuthData)&NSS_GetClientAuthData, NULL ) == SECSuccess) {
    g_warning ("ClientAuth data handshake with %s established.", ssl->priv->expected_host);
	  return ssl_fd;
  } else {
    g_warning ("ClientAuth data handshake with %s failed.", ssl->priv->expected_host);
    return NULL;
  }
}

static PRFileDesc *
enable_ssl_or_close_fd (CamelTcpStreamSSL *ssl, PRFileDesc *fd, GError **error)
{
	PRFileDesc *ssl_fd;

	ssl_fd = enable_ssl (ssl, fd);
	if (ssl_fd == NULL) {
		gint errnosave;

		_set_errno_from_pr_error (PR_GetError ());
		errnosave = errno;
		PR_Shutdown (fd, PR_SHUTDOWN_BOTH);
		PR_Close (fd);
		errno = errnosave;
		_set_g_error_from_errno (error, FALSE);

		return NULL;
	}

	return ssl_fd;
}

/* 3.1.4 API has cancellable argument & returns (status == SECSuccess) */
/* Otherwise it is step=-by-step functionally the same */

static gboolean
rehandshake_ssl (PRFileDesc *fd, GError **error)
{
  /* Fixme? Should this be a visible setting? GConf? Env? OS?        */
  /* Zero-Value for SSL_FHS breaks implicit TLS, so let's use that.  */
	int EVAL_IVAL = 100;
  /* IMO more than 10 second blocking means something really wrong   */ 
  int BTIMEOUT = EVAL_IVAL * 100;
  int counter = 0;

  if (SSL_ResetHandshake (fd, FALSE) == SECFailure) {
    g_warning ("Handshake reset failed.");
    _set_errno_from_pr_error (PR_GetError ());
    _set_g_error_from_errno (error, FALSE);
    return FALSE;
  }
  
  while (counter < BTIMEOUT && SSL_ForceHandshakeWithTimeout(fd, EVAL_IVAL) == SECFailure) {
    if (PR_GetError() == PR_WOULD_BLOCK_ERROR) {
      counter += EVAL_IVAL;
      continue;
    } else {
      g_warning ("ForceHandshake negotiation failed.");
      _set_errno_from_pr_error (PR_GetError ());
      _set_g_error_from_errno (error, FALSE);
      return FALSE;
    }
  }
	return TRUE;
}

static gint
tcp_stream_ssl_connect (CamelTcpStream *stream, const gchar *host, const gchar *service, gint fallback_port, GError **error)
{
	CamelTcpStreamSSL *ssl = CAMEL_TCP_STREAM_SSL (stream);
	gint retval;

	retval = CAMEL_TCP_STREAM_CLASS (camel_tcp_stream_ssl_parent_class)->connect (stream, host, service, fallback_port, error);
	if (retval != 0)
		return retval;

	if (ssl->priv->ssl_mode) {
		PRFileDesc *fd;
		PRFileDesc *ssl_fd;

		d (g_print ("  enabling SSL\n"));

		fd = camel_tcp_stream_get_file_desc (stream);
		ssl_fd = enable_ssl_or_close_fd (ssl, fd, error);
		_camel_tcp_stream_raw_replace_file_desc (CAMEL_TCP_STREAM_RAW (stream), ssl_fd);

		if (!ssl_fd) {
			d (g_print ("  could not enable SSL\n"));
		} else {
			d (g_print ("  re-handshaking SSL\n"));

			if (!rehandshake_ssl (ssl_fd, error)) {
				d (g_print ("  failed\n"));
				return -1;
			}
		}
	}

	return 0;
}

static void
camel_tcp_stream_ssl_class_init (CamelTcpStreamSSLClass *class)
{
	GObjectClass *object_class;
	CamelTcpStreamClass *tcp_stream_class;

	g_type_class_add_private (class, sizeof (CamelTcpStreamSSLPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = tcp_stream_ssl_dispose;
	object_class->finalize = tcp_stream_ssl_finalize;

	tcp_stream_class = CAMEL_TCP_STREAM_CLASS (class);
	tcp_stream_class->connect = tcp_stream_ssl_connect;
}

static void
camel_tcp_stream_ssl_init (CamelTcpStreamSSL *stream)
{
	stream->priv = CAMEL_TCP_STREAM_SSL_GET_PRIVATE (stream);
}

/**
 * camel_tcp_stream_ssl_new:
 * @session: an active #CamelSession object
 * @expected_host: host that the stream is expected to connect with
 * @flags: a bitwise combination of any of
 * #CAMEL_TCP_STREAM_SSL_ENABLE_SSL2,
 * #CAMEL_TCP_STREAM_SSL_ENABLE_SSL3 or
 * #CAMEL_TCP_STREAM_SSL_ENABLE_TLS
 *
 * Since the SSL certificate authenticator may need to prompt the
 * user, a #CamelSession is needed. @expected_host is needed as a
 * protection against an MITM attack.
 *
 * Returns: a new #CamelTcpStreamSSL stream preset in SSL mode
 **/
CamelStream *
camel_tcp_stream_ssl_new (CamelSession *session, const gchar *expected_host, guint32 flags)
{
	CamelTcpStreamSSL *stream;

	g_assert(CAMEL_IS_SESSION(session));

	stream = g_object_new (CAMEL_TYPE_TCP_STREAM_SSL, NULL);

	stream->priv->session = g_object_ref (session);
	stream->priv->expected_host = g_strdup (expected_host);
	stream->priv->ssl_mode = TRUE;
	stream->priv->flags = flags;

	return CAMEL_STREAM (stream);
}

/**
 * camel_tcp_stream_ssl_new_raw:
 * @session: an active #CamelSession object
 * @expected_host: host that the stream is expected to connect with
 * @flags: a bitwise combination of any of
 * #CAMEL_TCP_STREAM_SSL_ENABLE_SSL2,
 * #CAMEL_TCP_STREAM_SSL_ENABLE_SSL3 or
 * #CAMEL_TCP_STREAM_SSL_ENABLE_TLS
 *
 * Since the SSL certificate authenticator may need to prompt the
 * user, a CamelSession is needed. @expected_host is needed as a
 * protection against an MITM attack.
 *
 * Returns: a new #CamelTcpStreamSSL stream not yet toggled into SSL mode
 **/
CamelStream *
camel_tcp_stream_ssl_new_raw (CamelSession *session, const gchar *expected_host, guint32 flags)
{
	CamelTcpStreamSSL *stream;

	g_assert(CAMEL_IS_SESSION(session));

	stream = g_object_new (CAMEL_TYPE_TCP_STREAM_SSL, NULL);

	stream->priv->session = g_object_ref (session);
	stream->priv->expected_host = g_strdup (expected_host);
	stream->priv->ssl_mode = FALSE;
	stream->priv->flags = flags;

	return CAMEL_STREAM (stream);
}

/**
 * camel_tcp_stream_ssl_enable_ssl:
 * @ssl: a #CamelTcpStreamSSL object
 *
 * Toggles an ssl-capable stream into ssl mode (if it isn't already).
 *
 * Returns: %0 on success or %-1 on fail
 **/
gint
camel_tcp_stream_ssl_enable_ssl (CamelTcpStreamSSL *ssl)
{
	PRFileDesc *fd, *ssl_fd;
	g_return_val_if_fail (CAMEL_IS_TCP_STREAM_SSL (ssl), -1);

	fd = camel_tcp_stream_get_file_desc (CAMEL_TCP_STREAM (ssl));

  /* '!ssl->priv->ssl_mode' refers to STARTTLS opportunistic tls */
	if (fd && !ssl->priv->ssl_mode) {
		if (!(ssl_fd = enable_ssl (ssl, fd))) {
			_set_errno_from_pr_error (PR_GetError ());
			return -1;
		}

		_camel_tcp_stream_raw_replace_file_desc (CAMEL_TCP_STREAM_RAW (ssl), ssl_fd);
		ssl->priv->ssl_mode = TRUE;

		if (!rehandshake_ssl (ssl_fd, NULL)) /* NULL-GError */
			return -1;
	}

	ssl->priv->ssl_mode = TRUE;

	return 0;
}

#endif /* CAMEL_HAVE_NSS */
