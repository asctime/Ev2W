/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; fill-column: 160 -*- */
/* camel-mime-message.c : class for a mime_message */

/*
 * Authors: Bertrand Guiheneuf <bertrand@helixcode.com>
 *	    Michael Zucchi <notzed@ximian.com>
 *          Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <libedataserver/e-time-utils.h>

#include "camel-iconv.h"
#include "camel-mime-filter-bestenc.h"
#include "camel-mime-filter-charset.h"
#include "camel-mime-message.h"
#include "camel-multipart.h"
#include "camel-stream-filter.h"
#include "camel-stream-mem.h"
#include "camel-stream-null.h"
#include "camel-string-utils.h"
#include "camel-url.h"

#ifdef G_OS_WIN32
#ifdef gmtime_r
#undef gmtime_r
#endif

/* The gmtime() in Microsoft's C library is MT-safe */
#define gmtime_r(tp,tmp) (gmtime(tp)?(*(tmp)=*gmtime(tp),(tmp)):0)
#endif
#define d(x)

extern gint camel_verbose_debug;

/* these 2 below should be kept in sync */
typedef enum {
	HEADER_UNKNOWN,
	HEADER_FROM,
	HEADER_REPLY_TO,
	HEADER_SUBJECT,
	HEADER_TO,
	HEADER_RESENT_TO,
	HEADER_CC,
	HEADER_RESENT_CC,
	HEADER_BCC,
	HEADER_RESENT_BCC,
	HEADER_DATE,
	HEADER_MESSAGE_ID
} CamelHeaderType;

static const gchar *header_names[] = {
	/* dont include HEADER_UNKNOWN string */
	"From", "Reply-To", "Subject", "To", "Resent-To", "Cc", "Resent-Cc",
	"Bcc", "Resent-Bcc", "Date", "Message-ID", NULL
};

static const gchar *recipient_names[] = {
	"To", "Cc", "Bcc", "Resent-To", "Resent-Cc", "Resent-Bcc", NULL
};

static GHashTable *header_name_table;

G_DEFINE_TYPE (CamelMimeMessage, camel_mime_message, CAMEL_TYPE_MIME_PART)

/* FIXME: check format of fields. */
static gboolean
process_header (CamelMedium *medium,
                const gchar *name,
                const gchar *value)
{
	CamelHeaderType header_type;
	CamelMimeMessage *message = CAMEL_MIME_MESSAGE (medium);
	CamelInternetAddress *addr;
	const gchar *charset;
	gchar *unfolded;

	header_type = (CamelHeaderType) g_hash_table_lookup (header_name_table, name);
	switch (header_type) {
	case HEADER_FROM:
		addr = camel_internet_address_new();
		unfolded = camel_header_unfold (value);
		if (camel_address_decode ((CamelAddress *) addr, unfolded) <= 0) {
			g_object_unref (addr);
		} else {
			if (message->from)
				g_object_unref (message->from);
			message->from = addr;
		}
		g_free (unfolded);
		break;
	case HEADER_REPLY_TO:
		addr = camel_internet_address_new();
		unfolded = camel_header_unfold (value);
		if (camel_address_decode ((CamelAddress *) addr, unfolded) <= 0) {
			g_object_unref (addr);
		} else {
			if (message->reply_to)
				g_object_unref (message->reply_to);
			message->reply_to = addr;
		}
		g_free (unfolded);
		break;
	case HEADER_SUBJECT:
		g_free (message->subject);
		if (((CamelDataWrapper *) message)->mime_type) {
			charset = camel_content_type_param (((CamelDataWrapper *) message)->mime_type, "charset");
			charset = camel_iconv_charset_name (charset);
		} else
			charset = NULL;

		unfolded = camel_header_unfold (value);
		message->subject = g_strstrip (camel_header_decode_string (unfolded, charset));
		g_free (unfolded);
		break;
	case HEADER_TO:
	case HEADER_CC:
	case HEADER_BCC:
	case HEADER_RESENT_TO:
	case HEADER_RESENT_CC:
	case HEADER_RESENT_BCC:
		addr = g_hash_table_lookup (message->recipients, name);
		if (value) {
			unfolded = camel_header_unfold (value);
			camel_address_decode (CAMEL_ADDRESS (addr), unfolded);
			g_free (unfolded);
		} else {
			camel_address_remove (CAMEL_ADDRESS (addr), -1);
		}
		return FALSE;
	case HEADER_DATE:
		if (value) {
			message->date = camel_header_decode_date (value, &message->date_offset);
		} else {
			message->date = CAMEL_MESSAGE_DATE_CURRENT;
			message->date_offset = 0;
		}
		break;
	case HEADER_MESSAGE_ID:
		g_free (message->message_id);
		if (value)
			message->message_id = camel_header_msgid_decode (value);
		else
			message->message_id = NULL;
		break;
	default:
		return FALSE;
	}

	return TRUE;
}

static void
unref_recipient (gpointer key, gpointer value, gpointer user_data)
{
	g_object_unref (value);
}

static void
mime_message_dispose (GObject *object)
{
	CamelMimeMessage *message = CAMEL_MIME_MESSAGE (object);

	if (message->reply_to != NULL) {
		g_object_unref (message->reply_to);
		message->reply_to = NULL;
	}

	if (message->from != NULL) {
		g_object_unref (message->from);
		message->from = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (camel_mime_message_parent_class)->dispose (object);
}

static void
mime_message_finalize (GObject *object)
{
	CamelMimeMessage *message = CAMEL_MIME_MESSAGE (object);

	g_free (message->subject);

	g_free (message->message_id);

	g_hash_table_foreach (message->recipients, unref_recipient, NULL);
	g_hash_table_destroy (message->recipients);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (camel_mime_message_parent_class)->finalize (object);
}

static gssize
mime_message_write_to_stream (CamelDataWrapper *data_wrapper,
                              CamelStream *stream,
                              GError **error)
{
	CamelDataWrapperClass *data_wrapper_class;
	CamelMimeMessage *mm = CAMEL_MIME_MESSAGE (data_wrapper);

	/* force mandatory headers ... */
	if (mm->from == NULL) {
		/* FIXME: should we just abort?  Should we make one up? */
		g_warning ("No from set for message");
		camel_medium_set_header ((CamelMedium *)mm, "From", "");
	}
	if (!camel_medium_get_header ((CamelMedium *)mm, "Date"))
		camel_mime_message_set_date (mm, CAMEL_MESSAGE_DATE_CURRENT, 0);

	if (mm->subject == NULL)
		camel_mime_message_set_subject (mm, "No Subject");

	if (mm->message_id == NULL)
		camel_mime_message_set_message_id (mm, NULL);

	/* FIXME: "To" header needs to be set explicitly as well ... */

	if (!camel_medium_get_header ((CamelMedium *)mm, "Mime-Version"))
		camel_medium_set_header ((CamelMedium *)mm, "Mime-Version", "1.0");

	/* Chain up to parent's write_to_stream() method. */
	data_wrapper_class = CAMEL_DATA_WRAPPER_CLASS (camel_mime_message_parent_class);
	return data_wrapper_class->write_to_stream (data_wrapper, stream, error);
}

static void
mime_message_add_header (CamelMedium *medium,
                         const gchar *name,
                         gconstpointer value)
{
	CamelMediumClass *medium_class;

	medium_class = CAMEL_MEDIUM_CLASS (camel_mime_message_parent_class);

	/* if we process it, then it must be forced unique as well ... */
	if (process_header (medium, name, value))
		medium_class->set_header (medium, name, value);
	else
		medium_class->add_header (medium, name, value);
}

static void
mime_message_set_header (CamelMedium *medium,
                         const gchar *name,
                         gconstpointer value)
{
	process_header (medium, name, value);

	/* Chain up to parent's set_header() method. */
	CAMEL_MEDIUM_CLASS (camel_mime_message_parent_class)->set_header (medium, name, value);
}

static void
mime_message_remove_header (CamelMedium *medium,
                            const gchar *name)
{
	process_header (medium, name, NULL);

	/* Chain up to parent's remove_header() method. */
	CAMEL_MEDIUM_CLASS (camel_mime_message_parent_class)->remove_header (medium, name);
}

static gint
mime_message_construct_from_parser (CamelMimePart *dw,
                                    CamelMimeParser *mp,
                                    GError **error)
{
	CamelMimePartClass *mime_part_class;
	gchar *buf;
	gsize len;
	gint state;
	gint ret;
	gint err;

	d(printf("constructing mime-message\n"));

	d(printf("mime_message::construct_from_parser()\n"));

	/* let the mime-part construct the guts ... */
	mime_part_class = CAMEL_MIME_PART_CLASS (camel_mime_message_parent_class);
	ret = mime_part_class->construct_from_parser (dw, mp, error);

	if (ret == -1)
		return -1;

	/* ... then clean up the follow-on state */
	state = camel_mime_parser_step (mp, &buf, &len);
	switch (state) {
	case CAMEL_MIME_PARSER_STATE_EOF:
	case CAMEL_MIME_PARSER_STATE_FROM_END:
		/* these doesn't belong to us */
		camel_mime_parser_unstep (mp);
	case CAMEL_MIME_PARSER_STATE_MESSAGE_END:
		break;
	default:
		g_error ("Bad parser state: Expecing MESSAGE_END or EOF or EOM, got: %u", camel_mime_parser_state (mp));
		camel_mime_parser_unstep (mp);
		return -1;
	}

	d(printf("mime_message::construct_from_parser() leaving\n"));
	err = camel_mime_parser_errno(mp);
	if (err != 0) {
		errno = err;
		g_set_error (
			error, G_IO_ERROR,
			g_io_error_from_errno (errno),
			"%s", g_strerror (errno));
		ret = -1;
	}

	return ret;
}

static void
camel_mime_message_class_init (CamelMimeMessageClass *class)
{
	GObjectClass *object_class;
	CamelDataWrapperClass *data_wrapper_class;
	CamelMimePartClass *mime_part_class;
	CamelMediumClass *medium_class;
	gint ii;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = mime_message_dispose;
	object_class->finalize = mime_message_finalize;

	data_wrapper_class = CAMEL_DATA_WRAPPER_CLASS (class);
	data_wrapper_class->write_to_stream = mime_message_write_to_stream;
	data_wrapper_class->decode_to_stream = mime_message_write_to_stream;

	medium_class = CAMEL_MEDIUM_CLASS (class);
	medium_class->add_header = mime_message_add_header;
	medium_class->set_header = mime_message_set_header;
	medium_class->remove_header = mime_message_remove_header;

	mime_part_class = CAMEL_MIME_PART_CLASS (class);
	mime_part_class->construct_from_parser = mime_message_construct_from_parser;

	header_name_table = g_hash_table_new (
		camel_strcase_hash, camel_strcase_equal);
	for (ii = 0; header_names[ii] != NULL; ii++)
		g_hash_table_insert (
			header_name_table,
			(gpointer) header_names[ii],
			GINT_TO_POINTER (ii + 1));
}

static void
camel_mime_message_init (CamelMimeMessage *mime_message)
{
	gint ii;

	mime_message->recipients = g_hash_table_new (
		camel_strcase_hash, camel_strcase_equal);
	for (ii = 0; recipient_names[ii] != NULL; ii++) {
		g_hash_table_insert (
			mime_message->recipients,
			(gpointer) recipient_names[ii],
			camel_internet_address_new ());
	}

	mime_message->subject = NULL;
	mime_message->reply_to = NULL;
	mime_message->from = NULL;
	mime_message->date = CAMEL_MESSAGE_DATE_CURRENT;
	mime_message->date_offset = 0;
	mime_message->date_received = CAMEL_MESSAGE_DATE_CURRENT;
	mime_message->date_received_offset = 0;
	mime_message->message_id = NULL;
}

/**
 * camel_mime_message_new:
 *
 * Create a new #CamelMimeMessage object.
 *
 * Returns: a new #CamelMimeMessage object
 **/
CamelMimeMessage *
camel_mime_message_new (void)
{
	return g_object_new (CAMEL_TYPE_MIME_MESSAGE, NULL);
}

/* **** Date: */

/**
 * camel_mime_message_set_date:
 * @message: a #CamelMimeMessage object
 * @date: a time_t date
 * @offset: an offset from GMT
 *
 * Set the date on a message.
 **/
void
camel_mime_message_set_date (CamelMimeMessage *message,  time_t date, gint offset)
{
	gchar *datestr;

	g_assert(message);

	if (date == CAMEL_MESSAGE_DATE_CURRENT) {
		struct tm local;
		gint tz;

		date = time(NULL);
		e_localtime_with_offset(date, &local, &tz);
		offset = (((tz/60/60) * 100) + (tz/60 % 60));
	}
	message->date = date;
	message->date_offset = offset;

	datestr = camel_header_format_date (date, offset);
	CAMEL_MEDIUM_CLASS (camel_mime_message_parent_class)->set_header ((CamelMedium *)message, "Date", datestr);
	g_free (datestr);
}

/**
 * camel_mime_message_get_date:
 * @message: a #CamelMimeMessage object
 * @offset: output for the GMT offset
 *
 * Get the date and GMT offset of a message.
 *
 * Returns: the date of the message
 **/
time_t
camel_mime_message_get_date (CamelMimeMessage *msg, gint *offset)
{
	if (offset)
		*offset = msg->date_offset;

	return msg->date;
}

/**
 * camel_mime_message_get_date_received:
 * @message: a #CamelMimeMessage object
 * @offset: output for the GMT offset
 *
 * Get the received date and GMT offset of a message.
 *
 * Returns: the received date of the message
 **/
time_t
camel_mime_message_get_date_received (CamelMimeMessage *msg, gint *offset)
{
	if (msg->date_received == CAMEL_MESSAGE_DATE_CURRENT) {
		const gchar *received;

		received = camel_medium_get_header ((CamelMedium *)msg, "received");
		if (received)
			received = strrchr (received, ';');
		if (received)
			msg->date_received = camel_header_decode_date (received + 1, &msg->date_received_offset);
	}

	if (offset)
		*offset = msg->date_received_offset;

	return msg->date_received;
}

/* **** Message-ID: */

/**
 * camel_mime_message_set_message_id:
 * @message: a #CamelMimeMessage object
 * @message_id: id of the message
 *
 * Set the message-id on a message.
 **/
void
camel_mime_message_set_message_id (CamelMimeMessage *mime_message, const gchar *message_id)
{
	gchar *id;

	g_assert (mime_message);

	g_free (mime_message->message_id);

	if (message_id) {
		id = g_strstrip (g_strdup (message_id));
	} else {
		id = camel_header_msgid_generate ();
	}

	mime_message->message_id = id;
	id = g_strdup_printf ("<%s>", mime_message->message_id);
	CAMEL_MEDIUM_CLASS (camel_mime_message_parent_class)->set_header (CAMEL_MEDIUM (mime_message), "Message-ID", id);
	g_free (id);
}

/**
 * camel_mime_message_get_message_id:
 * @message: a #CamelMimeMessage object
 *
 * Get the message-id of a message.
 *
 * Returns: the message-id of a message
 **/
const gchar *
camel_mime_message_get_message_id (CamelMimeMessage *mime_message)
{
	g_assert (mime_message);

	return mime_message->message_id;
}

/* **** Reply-To: */

/**
 * camel_mime_message_set_reply_to:
 * @message: a #CamelMimeMessage object
 * @reply_to: a #CamelInternetAddress object
 *
 * Set the Reply-To of a message.
 **/
void
camel_mime_message_set_reply_to (CamelMimeMessage *msg, CamelInternetAddress *reply_to)
{
	gchar *addr;

	g_assert(msg);

	if (msg->reply_to) {
		g_object_unref (msg->reply_to);
		msg->reply_to = NULL;
	}

	if (reply_to == NULL) {
		CAMEL_MEDIUM_CLASS (camel_mime_message_parent_class)->remove_header (CAMEL_MEDIUM (msg), "Reply-To");
		return;
	}

	msg->reply_to = (CamelInternetAddress *)camel_address_new_clone ((CamelAddress *)reply_to);
	addr = camel_address_encode ((CamelAddress *)msg->reply_to);
	CAMEL_MEDIUM_CLASS (camel_mime_message_parent_class)->set_header (CAMEL_MEDIUM (msg), "Reply-To", addr);
	g_free (addr);
}

/**
 * camel_mime_message_get_reply_to:
 * @message: a #CamelMimeMessage object
 *
 * Get the Reply-To of a message.
 *
 * Returns: the Reply-Toa ddress of the message
 **/
CamelInternetAddress *
camel_mime_message_get_reply_to (CamelMimeMessage *mime_message)
{
	g_assert (mime_message);

	/* TODO: ref for threading? */

	return mime_message->reply_to;
}

/* **** Subject: */

/**
 * camel_mime_message_set_subject:
 * @message: a #CamelMimeMessage object
 * @subject: UTF-8 message subject
 *
 * Set the subject text of a message.
 **/
void
camel_mime_message_set_subject (CamelMimeMessage *message, const gchar *subject)
{
	gchar *text;

	g_assert(message);

	g_free (message->subject);

	if (subject) {
		message->subject = g_strstrip (g_strdup (subject));
		text = camel_header_encode_string ((guchar *) message->subject);
	} else {
		message->subject = NULL;
		text = NULL;
	}

	CAMEL_MEDIUM_CLASS (camel_mime_message_parent_class)->set_header (CAMEL_MEDIUM (message), "Subject", text);
	g_free (text);
}

/**
 * camel_mime_message_get_subject:
 * @message: a #CamelMimeMessage object
 *
 * Get the UTF-8 subject text of a message.
 *
 * Returns: the message subject
 **/
const gchar *
camel_mime_message_get_subject (CamelMimeMessage *mime_message)
{
	g_assert(mime_message);

	return mime_message->subject;
}

/* *** From: */

/* Thought: Since get_from/set_from are so rarely called, it is probably not useful
   to cache the from (and reply_to) addresses as InternetAddresses internally, we
   could just get it from the headers and reprocess every time. */

/**
 * camel_mime_message_set_from:
 * @message: a #CamelMimeMessage object
 * @from: a #CamelInternetAddress object
 *
 * Set the from address of a message.
 **/
void
camel_mime_message_set_from (CamelMimeMessage *msg, CamelInternetAddress *from)
{
	gchar *addr;

	g_assert(msg);

	if (msg->from) {
		g_object_unref (msg->from);
		msg->from = NULL;
	}

	if (from == NULL || camel_address_length((CamelAddress *)from) == 0) {
		CAMEL_MEDIUM_CLASS(camel_mime_message_parent_class)->remove_header(CAMEL_MEDIUM(msg), "From");
		return;
	}

	msg->from = (CamelInternetAddress *)camel_address_new_clone((CamelAddress *)from);
	addr = camel_address_encode((CamelAddress *)msg->from);
	CAMEL_MEDIUM_CLASS (camel_mime_message_parent_class)->set_header(CAMEL_MEDIUM(msg), "From", addr);
	g_free(addr);
}

/**
 * camel_mime_message_get_from:
 * @message: a #CamelMimeMessage object
 *
 * Get the from address of a message.
 *
 * Returns: the from address of the message
 **/
CamelInternetAddress *
camel_mime_message_get_from (CamelMimeMessage *mime_message)
{
	g_assert (mime_message);

	/* TODO: we should really ref this for multi-threading to work */

	return mime_message->from;
}

/*  **** To: Cc: Bcc: */

/**
 * camel_mime_message_set_recipients:
 * @message: a #CamelMimeMessage object
 * @type: recipient type (one of #CAMEL_RECIPIENT_TYPE_TO, #CAMEL_RECIPIENT_TYPE_CC, or #CAMEL_RECIPIENT_TYPE_BCC)
 * @recipients: a #CamelInternetAddress with the recipient addresses set
 *
 * Set the recipients of a message.
 **/
void
camel_mime_message_set_recipients(CamelMimeMessage *mime_message, const gchar *type, CamelInternetAddress *r)
{
	gchar *text;
	CamelInternetAddress *addr;

	g_assert(mime_message);

	addr = g_hash_table_lookup (mime_message->recipients, type);
	if (addr == NULL) {
		g_warning ("trying to set a non-valid receipient type: %s", type);
		return;
	}

	if (r == NULL || camel_address_length ((CamelAddress *)r) == 0) {
		camel_address_remove ((CamelAddress *)addr, -1);
		CAMEL_MEDIUM_CLASS (camel_mime_message_parent_class)->remove_header (CAMEL_MEDIUM (mime_message), type);
		return;
	}

	/* note this does copy, and not append (cat) */
	camel_address_copy ((CamelAddress *)addr, (CamelAddress *)r);

	/* and sync our headers */
	text = camel_address_encode (CAMEL_ADDRESS (addr));
	CAMEL_MEDIUM_CLASS (camel_mime_message_parent_class)->set_header (CAMEL_MEDIUM (mime_message), type, text);
	g_free(text);
}

/**
 * camel_mime_message_get_recipients:
 * @message: a #CamelMimeMessage object
 * @type: recipient type
 *
 * Get the message recipients of a specified type.
 *
 * Returns: the requested recipients
 **/
CamelInternetAddress *
camel_mime_message_get_recipients (CamelMimeMessage *mime_message, const gchar *type)
{
	g_assert(mime_message);

	return g_hash_table_lookup (mime_message->recipients, type);
}

void
camel_mime_message_set_source (CamelMimeMessage *mime_message, const gchar *src)
{
	CamelURL *url;
	gchar *uri;

	g_assert (mime_message);

	url = camel_url_new (src, NULL);
	if (url) {
		uri = camel_url_to_string (url, CAMEL_URL_HIDE_ALL);
		camel_medium_add_header (CAMEL_MEDIUM (mime_message), "X-Evolution-Source", uri);
		g_free (uri);
		camel_url_free (url);
	}
}

const gchar *
camel_mime_message_get_source (CamelMimeMessage *mime_message)
{
	const gchar *src;

	g_assert(mime_message);

	src = camel_medium_get_header (CAMEL_MEDIUM (mime_message), "X-Evolution-Source");
	if (src) {
		while (*src && isspace ((unsigned) *src))
			++src;
	}
	return src;
}

typedef gboolean (*CamelPartFunc)(CamelMimeMessage *, CamelMimePart *, gpointer data);

static gboolean
message_foreach_part_rec (CamelMimeMessage *msg, CamelMimePart *part, CamelPartFunc callback, gpointer data)
{
	CamelDataWrapper *containee;
	gint parts, i;
	gint go = TRUE;

	if (callback (msg, part, data) == FALSE)
		return FALSE;

	containee = camel_medium_get_content (CAMEL_MEDIUM (part));

	if (containee == NULL)
		return go;

	/* using the object types is more accurate than using the mime/types */
	if (CAMEL_IS_MULTIPART (containee)) {
		parts = camel_multipart_get_number (CAMEL_MULTIPART (containee));
		for (i = 0; go && i < parts; i++) {
			CamelMimePart *mpart = camel_multipart_get_part (CAMEL_MULTIPART (containee), i);

			go = message_foreach_part_rec (msg, mpart, callback, data);
		}
	} else if (CAMEL_IS_MIME_MESSAGE (containee)) {
		go = message_foreach_part_rec (msg, (CamelMimePart *)containee, callback, data);
	}

	return go;
}

/* dont make this public yet, it might need some more thinking ... */
/* MPZ */
static void
camel_mime_message_foreach_part (CamelMimeMessage *msg, CamelPartFunc callback, gpointer data)
{
	message_foreach_part_rec (msg, (CamelMimePart *)msg, callback, data);
}

static gboolean
check_8bit (CamelMimeMessage *msg, CamelMimePart *part, gpointer data)
{
	CamelTransferEncoding encoding;
	gint *has8bit = data;

	/* check this part, and stop as soon as we are done */
	encoding = camel_mime_part_get_encoding (part);

	*has8bit = encoding == CAMEL_TRANSFER_ENCODING_8BIT || encoding == CAMEL_TRANSFER_ENCODING_BINARY;

	return !(*has8bit);
}

/**
 * camel_mime_message_has_8bit_parts:
 * @message: a #CamelMimeMessage object
 *
 * Find out if a message contains 8bit or binary encoded parts.
 *
 * Returns: %TRUE if the message contains 8bit parts or %FALSE otherwise
 **/
gboolean
camel_mime_message_has_8bit_parts (CamelMimeMessage *msg)
{
	gint has8bit = FALSE;

	camel_mime_message_foreach_part (msg, check_8bit, &has8bit);

	return has8bit;
}

/* finds the best charset and transfer encoding for a given part */
static CamelTransferEncoding
find_best_encoding (CamelMimePart *part, CamelBestencRequired required, CamelBestencEncoding enctype, gchar **charsetp)
{
	CamelMimeFilter *charenc = NULL;
	CamelTransferEncoding encoding;
	CamelMimeFilter *bestenc;
	guint flags, callerflags;
	CamelDataWrapper *content;
	CamelStream *filter;
	const gchar *charsetin = NULL;
	gchar *charset = NULL;
	CamelStream *null;
	gint idb, idc = -1;
	gboolean istext;

	/* we use all these weird stream things so we can do it with streams, and
	   not have to read the whole lot into memory - although i have a feeling
	   it would make things a fair bit simpler to do so ... */

	d(printf("starting to check part\n"));

	content = camel_medium_get_content ((CamelMedium *)part);
	if (content == NULL) {
		/* charset might not be right here, but it'll get the right stuff
		   if it is ever set */
		*charsetp = NULL;
		return CAMEL_TRANSFER_ENCODING_DEFAULT;
	}

	istext = camel_content_type_is (((CamelDataWrapper *) part)->mime_type, "text", "*");
	if (istext) {
		flags = CAMEL_BESTENC_GET_CHARSET | CAMEL_BESTENC_GET_ENCODING;
		enctype |= CAMEL_BESTENC_TEXT;
	} else {
		flags = CAMEL_BESTENC_GET_ENCODING;
	}

	/* when building the message, any encoded parts are translated already */
	flags |= CAMEL_BESTENC_LF_IS_CRLF;
	/* and get any flags the caller passed in */
	callerflags = (required & CAMEL_BESTENC_NO_FROM);
	flags |= callerflags;

	/* first a null stream, so any filtering is thrown away; we only want the sideeffects */
	null = (CamelStream *)camel_stream_null_new ();
	filter = camel_stream_filter_new (null);

	/* if we're looking for the best charset, then we need to convert to UTF-8 */
	if (istext && (required & CAMEL_BESTENC_GET_CHARSET) != 0
	    && (charsetin = camel_content_type_param (content->mime_type, "charset"))) {
		charenc = camel_mime_filter_charset_new (charsetin, "UTF-8");
		if (charenc != NULL)
			idc = camel_stream_filter_add (
				CAMEL_STREAM_FILTER (filter), charenc);
		charsetin = NULL;
	}

	bestenc = camel_mime_filter_bestenc_new (flags);
	idb = camel_stream_filter_add (
		CAMEL_STREAM_FILTER (filter), bestenc);
	d(printf("writing to checking stream\n"));
	camel_data_wrapper_decode_to_stream (content, filter, NULL);
	camel_stream_filter_remove (CAMEL_STREAM_FILTER (filter), idb);
	if (idc != -1) {
		camel_stream_filter_remove (CAMEL_STREAM_FILTER (filter), idc);
		g_object_unref (charenc);
		charenc = NULL;
	}

	if (istext && (required & CAMEL_BESTENC_GET_CHARSET) != 0) {
		charsetin = camel_mime_filter_bestenc_get_best_charset (
			CAMEL_MIME_FILTER_BESTENC (bestenc));
		d(printf("best charset = %s\n", charsetin ? charsetin : "(null)"));
		charset = g_strdup (charsetin);

		charsetin = camel_content_type_param (content->mime_type, "charset");
	} else {
		charset = NULL;
	}

	/* if we have US-ASCII, or we're not doing text, we dont need to bother with the rest */
	if (istext && charsetin && charset && (required & CAMEL_BESTENC_GET_CHARSET) != 0) {
		d(printf("have charset, trying conversion/etc\n"));

		/* now that 'bestenc' has told us what the best encoding is, we can use that to create
		   a charset conversion filter as well, and then re-add the bestenc to filter the
		   result to find the best encoding to use as well */

		charenc = camel_mime_filter_charset_new (charsetin, charset);
		if (charenc != NULL) {
			/* otherwise, try another pass, converting to the real charset */

			camel_mime_filter_reset (bestenc);
			camel_mime_filter_bestenc_set_flags (
				CAMEL_MIME_FILTER_BESTENC (bestenc),
				CAMEL_BESTENC_GET_ENCODING |
				CAMEL_BESTENC_LF_IS_CRLF | callerflags);

			camel_stream_filter_add (
				CAMEL_STREAM_FILTER (filter), charenc);
			camel_stream_filter_add (
				CAMEL_STREAM_FILTER (filter), bestenc);

			/* and write it to the new stream */
			camel_data_wrapper_write_to_stream (
				content, filter, NULL);

			g_object_unref (charenc);
		}
	}

	encoding = camel_mime_filter_bestenc_get_best_encoding (
		CAMEL_MIME_FILTER_BESTENC (bestenc), enctype);

	g_object_unref (filter);
	g_object_unref (bestenc);
	g_object_unref (null);

	d(printf("done, best encoding = %d\n", encoding));

	if (charsetp)
		*charsetp = charset;
	else
		g_free (charset);

	return encoding;
}

struct _enc_data {
	CamelBestencRequired required;
	CamelBestencEncoding enctype;
};

static gboolean
best_encoding (CamelMimeMessage *msg, CamelMimePart *part, gpointer datap)
{
	struct _enc_data *data = datap;
	CamelTransferEncoding encoding;
	CamelDataWrapper *wrapper;
	gchar *charset;

	wrapper = camel_medium_get_content (CAMEL_MEDIUM (part));
	if (!wrapper)
		return FALSE;

	/* we only care about actual content objects */
	if (!CAMEL_IS_MULTIPART (wrapper) && !CAMEL_IS_MIME_MESSAGE (wrapper)) {
		encoding = find_best_encoding (part, data->required, data->enctype, &charset);
		/* we always set the encoding, if we got this far.  GET_CHARSET implies
		   also GET_ENCODING */
		camel_mime_part_set_encoding (part, encoding);

		if ((data->required & CAMEL_BESTENC_GET_CHARSET) != 0) {
			if (camel_content_type_is (((CamelDataWrapper *) part)->mime_type, "text", "*")) {
				gchar *newct;

				/* FIXME: ick, the part content_type interface needs fixing bigtime */
				camel_content_type_set_param (((CamelDataWrapper *) part)->mime_type, "charset",
							       charset ? charset : "us-ascii");
				newct = camel_content_type_format (((CamelDataWrapper *) part)->mime_type);
				if (newct) {
					d(printf("Setting content-type to %s\n", newct));

					camel_mime_part_set_content_type (part, newct);
					g_free (newct);
				}
			}
		}

		g_free (charset);
	}

	return TRUE;
}

/**
 * camel_mime_message_set_best_encoding:
 * @message: a #CamelMimeMessage object
 * @required: a bitwise ORing of #CAMEL_BESTENC_GET_ENCODING and #CAMEL_BESTENC_GET_CHARSET
 * @enctype: an encoding to enforce
 *
 * Re-encode all message parts to conform with the required encoding rules.
 *
 * If @enctype is #CAMEL_BESTENC_7BIT, then all parts will be re-encoded into
 * one of the 7bit transfer encodings. If @enctype is #CAMEL_BESTENC_8bit, all
 * parts will be re-encoded to either a 7bit encoding or, if the part is 8bit
 * text, allowed to stay 8bit. If @enctype is #CAMEL_BESTENC_BINARY, then binary
 * parts will be encoded as binary and 8bit textual parts will be encoded as 8bit.
 **/
void
camel_mime_message_set_best_encoding (CamelMimeMessage *msg, CamelBestencRequired required, CamelBestencEncoding enctype)
{
	struct _enc_data data;

	if ((required & (CAMEL_BESTENC_GET_ENCODING|CAMEL_BESTENC_GET_CHARSET)) == 0)
		return;

	data.required = required;
	data.enctype = enctype;

	camel_mime_message_foreach_part (msg, best_encoding, &data);
}

/**
 * camel_mime_message_encode_8bit_parts:
 * @message: a #CamelMimeMessage object
 *
 * Encode all message parts to a suitable transfer encoding for transport (7bit clean).
 **/
void
camel_mime_message_encode_8bit_parts (CamelMimeMessage *mime_message)
{
	camel_mime_message_set_best_encoding (mime_message, CAMEL_BESTENC_GET_ENCODING, CAMEL_BESTENC_7BIT);
}

struct _check_content_id {
	CamelMimePart *part;
	const gchar *content_id;
};

static gboolean
check_content_id (CamelMimeMessage *message, CamelMimePart *part, gpointer data)
{
	struct _check_content_id *check = (struct _check_content_id *) data;
	const gchar *content_id;
	gboolean found;

	content_id = camel_mime_part_get_content_id (part);

	found = content_id && !strcmp (content_id, check->content_id) ? TRUE : FALSE;
	if (found)
		check->part = g_object_ref (part);

	return !found;
}

/**
 * camel_mime_message_get_part_by_content_id:
 * @message: a #CamelMimeMessage object
 * @content_id: content-id to search for
 *
 * Get a MIME part by id from a message.
 *
 * Returns: the MIME part with the requested id or %NULL if not found
 **/
CamelMimePart *
camel_mime_message_get_part_by_content_id (CamelMimeMessage *message, const gchar *id)
{
	struct _check_content_id check;

	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), NULL);

	if (id == NULL)
		return NULL;

	check.content_id = id;
	check.part = NULL;

	camel_mime_message_foreach_part (message, check_content_id, &check);

	return check.part;
}

static const gchar tz_months[][4] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static const gchar tz_days[][4] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

/**
 * camel_mime_message_build_mbox_from:
 * @message: a #CamelMimeMessage object
 *
 * Build an MBox from-line from @message.
 *
 * Returns: an MBox from-line suitable for use in an mbox file
 **/
gchar *
camel_mime_message_build_mbox_from (CamelMimeMessage *message)
{
	CamelMimePart *mime_part;
	struct _camel_header_raw *header = ((CamelMimePart *)message)->headers;
	GString *out = g_string_new("From ");
	gchar *ret;
	const gchar *tmp;
	time_t thetime;
	gint offset;
	struct tm tm;

	mime_part = CAMEL_MIME_PART (message);

	tmp = camel_header_raw_find (&header, "Sender", NULL);
	if (tmp == NULL)
		tmp = camel_header_raw_find (&header, "From", NULL);
	if (tmp != NULL) {
		struct _camel_header_address *addr = camel_header_address_decode (tmp, NULL);

		tmp = NULL;
		if (addr) {
			if (addr->type == CAMEL_HEADER_ADDRESS_NAME) {
				g_string_append (out, addr->v.addr);
				tmp = "";
			}
			camel_header_address_unref (addr);
		}
	}

	if (tmp == NULL)
		g_string_append (out, "unknown@nodomain.now.au");

	/* try use the received header to get the date */
	tmp = camel_header_raw_find (&header, "Received", NULL);
	if (tmp) {
		tmp = strrchr(tmp, ';');
		if (tmp)
			tmp++;
	}

	/* if there isn't one, try the Date field */
	if (tmp == NULL)
		tmp = camel_header_raw_find (&header, "Date", NULL);

	thetime = camel_header_decode_date (tmp, &offset);
	thetime += ((offset / 100) * (60 * 60)) + (offset % 100) * 60;
	gmtime_r (&thetime, &tm);
	g_string_append_printf (out, " %s %s %2d %02d:%02d:%02d %4d\n",
				tz_days[tm.tm_wday], tz_months[tm.tm_mon],
				tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
				tm.tm_year + 1900);

	ret = out->str;
	g_string_free (out, FALSE);

	return ret;
}

static gboolean
find_attachment (CamelMimeMessage *msg, CamelMimePart *part, gpointer data)
{
	const CamelContentDisposition *cd;
	gboolean *found = (gboolean *)data;

	g_return_val_if_fail (part != NULL, FALSE);

	cd = camel_mime_part_get_content_disposition (part);

	if (cd) {
		const struct _camel_header_param *param;

		*found = (cd->disposition && g_ascii_strcasecmp (cd->disposition, "attachment") == 0);

		for (param = cd->params; param && !(*found); param = param->next) {
			if (param->name && param->value && *param->value && g_ascii_strcasecmp (param->name, "filename") == 0)
				*found = TRUE;
		}
	}

	return !(*found);
}

/**
 * camel_mime_message_has_attachment:
 * @message: a #CamelMimeMessage object
 *
 * Returns whether message contains at least one attachment part.
 *
 * Since: 2.28
 **/
gboolean
camel_mime_message_has_attachment (CamelMimeMessage *message)
{
	gboolean found = FALSE;

	g_return_val_if_fail (message != NULL, FALSE);

	camel_mime_message_foreach_part (message, find_attachment, &found);

	return found;
}

static void
cmm_dump_rec(CamelMimeMessage *msg, CamelMimePart *part, gint body, gint depth)
{
	CamelDataWrapper *containee;
	gint parts, i;
	gint go = TRUE;
	gchar *s;

	s = alloca(depth+1);
	memset(s, ' ', depth);
	s[depth] = 0;
	/* yes this leaks, so what its only debug stuff */
	printf("%sclass: %s\n", s, G_OBJECT_TYPE_NAME (part));
	printf("%smime-type: %s\n", s, camel_content_type_format(((CamelDataWrapper *)part)->mime_type));

	containee = camel_medium_get_content ((CamelMedium *)part);

	if (containee == NULL)
		return;

	printf("%scontent class: %s\n", s, G_OBJECT_TYPE_NAME (containee));
	printf("%scontent mime-type: %s\n", s, camel_content_type_format(((CamelDataWrapper *)containee)->mime_type));

	/* using the object types is more accurate than using the mime/types */
	if (CAMEL_IS_MULTIPART(containee)) {
		parts = camel_multipart_get_number((CamelMultipart *)containee);
		for (i = 0; go && i < parts; i++) {
			CamelMimePart *mpart = camel_multipart_get_part((CamelMultipart *)containee, i);

			cmm_dump_rec(msg, mpart, body, depth+2);
		}
	} else if (CAMEL_IS_MIME_MESSAGE(containee)) {
		cmm_dump_rec(msg, (CamelMimePart *)containee, body, depth+2);
	}
}

/**
 * camel_mime_message_dump:
 * @msg:
 * @body:
 *
 * Dump information about the mime message to stdout.
 *
 * If body is TRUE, then dump body content of the message as well (currently unimplemented).
 **/
void
camel_mime_message_dump(CamelMimeMessage *msg, gint body)
{
	cmm_dump_rec(msg, (CamelMimePart *)msg, body, 0);
}
