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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include "camel-test.h"
#include "session.h"

#define CAMEL_TYPE_PGP_SESSION     (camel_pgp_session_get_type ())
#define CAMEL_PGP_SESSION(obj)     (G_TYPE_CHECK_INSTANCE_CAST((obj), CAMEL_TYPE_PGP_SESSION, CamelPgpSession))
#define CAMEL_PGP_SESSION_CLASS(k) (G_TYPE_CHECK_CLASS_CAST ((k), CAMEL_TYPE_PGP_SESSION, CamelPgpSessionClass))
#define CAMEL_PGP_IS_SESSION(o)    (G_TYPE_CHECK_INSTANCE_TYPE((o), CAMEL_TYPE_PGP_SESSION))

typedef struct _CamelPgpSession {
	CamelSession parent_object;

} CamelPgpSession;

typedef struct _CamelPgpSessionClass {
	CamelSessionClass parent_class;

} CamelPgpSessionClass;

GType camel_pgp_session_get_type (void);

G_DEFINE_TYPE (CamelPgpSession, camel_pgp_session, camel_test_session_get_type ())

static gchar *
pgp_session_get_password (CamelSession *session,
                          CamelService *service,
                          const gchar *domain,
                          const gchar *prompt,
                          const gchar *item,
                          guint32 flags,
                          GError **error)
{
	return g_strdup ("no.secret");
}

static void
camel_pgp_session_class_init (CamelPgpSessionClass *class)
{
	CamelSessionClass *session_class;

	session_class = CAMEL_SESSION_CLASS (class);
	session_class->get_password = pgp_session_get_password;
}

static void
camel_pgp_session_init (CamelPgpSession *session)
{
}

static CamelSession *
camel_pgp_session_new (const gchar *path)
{
	CamelSession *session;

	session = g_object_new (CAMEL_TYPE_PGP_SESSION, NULL);
	camel_session_construct (session, path);

	return session;
}

gint main (gint argc, gchar **argv)
{
	CamelSession *session;
	CamelCipherContext *ctx;
	CamelCipherValidity *valid;
	CamelStream *stream1, *stream2;
	GByteArray *buffer1, *buffer2;
	struct _CamelMimePart *sigpart, *conpart, *encpart, *outpart;
	CamelDataWrapper *dw;
	GPtrArray *recipients;
	gchar *before, *after;
	gint ret;
	GError *error = NULL;

	if (getenv("CAMEL_TEST_GPG") == NULL)
		return 77;

	camel_test_init (argc, argv);

	/* clear out any camel-test data */
	system ("/bin/rm -rf /tmp/camel-test");
	system ("/bin/mkdir /tmp/camel-test");
	setenv ("GNUPGHOME", "/tmp/camel-test/.gnupg", 1);

	/* import the gpg keys */
	if ((ret = system ("gpg < /dev/null > /dev/null 2>&1")) == -1)
		return 77;
	else if (WEXITSTATUS (ret) == 127)
		return 77;

	g_message ("gpg --import " TEST_DATA_DIR "/camel-test.gpg.pub > /dev/null 2>&1");
	system ("gpg --import " TEST_DATA_DIR "/camel-test.gpg.pub > /dev/null 2>&1");
	g_message ("gpg --import " TEST_DATA_DIR "/camel-test.gpg.sec > /dev/null 2>&1");
	system ("gpg --import " TEST_DATA_DIR "/camel-test.gpg.sec > /dev/null 2>&1");

	session = camel_pgp_session_new ("/tmp/camel-test");

	ctx = camel_gpg_context_new (session);
	camel_gpg_context_set_always_trust (CAMEL_GPG_CONTEXT (ctx), TRUE);

	camel_test_start ("Test of PGP functions");

	stream1 = camel_stream_mem_new ();
	camel_stream_write (stream1, "Hello, I am a test stream.\n", 27, NULL);
	camel_stream_reset (stream1, NULL);

	conpart = camel_mime_part_new();
	dw = camel_data_wrapper_new();
	camel_data_wrapper_construct_from_stream(dw, stream1, NULL);
	camel_medium_set_content ((CamelMedium *)conpart, dw);
	g_object_unref (stream1);
	g_object_unref (dw);

	sigpart = camel_mime_part_new();

	camel_test_push ("PGP signing");
	camel_cipher_sign (ctx, "no.user@no.domain", CAMEL_CIPHER_HASH_SHA1, conpart, sigpart, &error);
	if (error != NULL) {
		printf("PGP signing failed assuming non-functional environment\n%s", error->message);
		camel_test_pull();
		return 77;
	}
	camel_test_pull ();

	g_clear_error (&error);

	camel_test_push ("PGP verify");
	valid = camel_cipher_verify (ctx, sigpart, &error);
	check_msg (error == NULL, "%s", error->message);
	check_msg (camel_cipher_validity_get_valid (valid), "%s", camel_cipher_validity_get_description (valid));
	camel_cipher_validity_free (valid);
	camel_test_pull ();

	g_object_unref (conpart);
	g_object_unref (sigpart);

	stream1 = camel_stream_mem_new ();
	camel_stream_write (stream1, "Hello, I am a test of encryption/decryption.", 44, NULL);
	camel_stream_reset (stream1, NULL);

	conpart = camel_mime_part_new();
	dw = camel_data_wrapper_new();
	camel_stream_reset(stream1, NULL);
	camel_data_wrapper_construct_from_stream(dw, stream1, NULL);
	camel_medium_set_content ((CamelMedium *)conpart, dw);
	g_object_unref (stream1);
	g_object_unref (dw);

	encpart = camel_mime_part_new();

	g_clear_error (&error);

	camel_test_push ("PGP encrypt");
	recipients = g_ptr_array_new ();
	g_ptr_array_add (recipients, (guint8 *) "no.user@no.domain");
	camel_cipher_encrypt (ctx, "no.user@no.domain", recipients, conpart, encpart, &error);
	check_msg (error == NULL, "%s", error->message);
	g_ptr_array_free (recipients, TRUE);
	camel_test_pull ();

	g_clear_error (&error);

	camel_test_push ("PGP decrypt");
	outpart = camel_mime_part_new();
	valid = camel_cipher_decrypt (ctx, encpart, outpart, &error);
	check_msg (error == NULL, "%s", error->message);
	check_msg (valid->encrypt.status == CAMEL_CIPHER_VALIDITY_ENCRYPT_ENCRYPTED, "%s", valid->encrypt.description);

	buffer1 = g_byte_array_new ();
	stream1 = camel_stream_mem_new_with_byte_array (buffer1);
	buffer2 = g_byte_array_new ();
	stream2 = camel_stream_mem_new_with_byte_array (buffer2);

	camel_data_wrapper_write_to_stream((CamelDataWrapper *)conpart, stream1, NULL);
	camel_data_wrapper_write_to_stream((CamelDataWrapper *)outpart, stream2, NULL);

	before = g_strndup ((gchar *) buffer1->data, buffer1->len);
	after = g_strndup ((gchar *) buffer2->data, buffer2->len);
	check_msg (string_equal (before, after), "before = '%s', after = '%s'", before, after);
	g_free (before);
	g_free (after);

	g_object_unref (stream1);
	g_object_unref (stream2);
	g_object_unref (conpart);
	g_object_unref (encpart);
	g_object_unref (outpart);

	camel_test_pull ();

	g_object_unref (CAMEL_OBJECT (ctx));
	g_object_unref (CAMEL_OBJECT (session));

	camel_test_end ();

	return 0;
}
