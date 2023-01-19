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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "mail-utils.h"
#include <e2k-propnames.h>
#include <e2k-utils.h>
#include <mapi.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <libedataserver/e-data-server-util.h>
#include <e-util/e-html-utils.h>
#include <e-util/e-util.h>
#include <libical/ical.h>

/**
 * mail_util_mapi_to_smtp_headers:
 * @props: an #E2kProperties containing MAPI header properties.
 *
 * Constructs a fake set of RFC822 headers based on the MAPI header
 * properties in @props.
 *
 * Return value: the headers, which must be free when you are done
 * with them.
 **/
gchar *
mail_util_mapi_to_smtp_headers (E2kProperties *props)
{
	GString *headers;
	gchar *prop, *buf;
	time_t dt;

	headers = g_string_new (NULL);

#define GET_HEADER(propname,headername) \
	prop = e2k_properties_get_prop (props, propname); \
	if (prop && *prop) \
		g_string_append_printf (headers, headername ": %s\r\n", prop);

	GET_HEADER (E2K_PR_MAILHEADER_RECEIVED, "Received");

	prop = e2k_properties_get_prop (props, E2K_PR_MAILHEADER_DATE);
	dt = prop ? e2k_parse_timestamp (prop) : 0;
	buf = e2k_make_timestamp_rfc822 (dt);
	g_string_append_printf (headers, "Date: %s\r\n", buf);
	g_free (buf);

	GET_HEADER (E2K_PR_MAILHEADER_SUBJECT, "Subject");
	GET_HEADER (E2K_PR_MAILHEADER_FROM, "From");
	GET_HEADER (E2K_PR_MAILHEADER_TO, "To");
	GET_HEADER (E2K_PR_MAILHEADER_CC, "Cc");
	GET_HEADER (E2K_PR_MAILHEADER_MESSAGE_ID, "Message-ID");
	GET_HEADER (E2K_PR_MAILHEADER_IN_REPLY_TO, "In-Reply-To");
	GET_HEADER (E2K_PR_MAILHEADER_REFERENCES, "References");
	GET_HEADER (E2K_PR_MAILHEADER_THREAD_INDEX, "Thread-Index");

#undef GET_HEADER

	prop = e2k_properties_get_prop (props, E2K_PR_DAV_CONTENT_TYPE);
	if (!prop || g_ascii_strncasecmp (prop, "message/", 8) != 0) {
		g_string_append_printf (headers,
					"Content-Type: %s\r\n"
					"Content-Transfer-Encoding: binary\r\n"
					"Content-Disposition: attachment",
					prop ? prop : "application/octet-stream");
		prop = e2k_properties_get_prop (props, E2K_PR_MAILHEADER_SUBJECT);
		if (prop)
			g_string_append_printf (headers, "; filename=\"%s\"", prop);
		g_string_append (headers, "\r\n");
	}

	g_string_append (headers, "\r\n");

	buf = headers->str;
	g_string_free (headers, FALSE);
	return buf;
}

/**
 * mail_util_props_to_camel_flags:
 * @props: an #E2kProperties containing MAPI message properties.
 * @obey_read_flag: if %FALSE, the %E2K_PR_HTTPMAIL_READ flag in
 * @props will be ignored.
 *
 * Converts the values of the %E2K_PR_HTTPMAIL_READ,
 * %E2K_PR_HTTPMAIL_HAS_ATTACHMENT, %PR_ACTION_FLAG, and
 * %PR_DELEGATED_BY_RULE properties to an equivalent set of
 * Camel message flags.
 *
 * Return value: the message flags
 **/
guint32
mail_util_props_to_camel_flags (E2kProperties *props, gboolean obey_read_flag)
{
	const gchar *prop;
	guint32 flags;
	gint val;

	flags = 0;

	prop = e2k_properties_get_prop (props, E2K_PR_HTTPMAIL_READ);
	if ((prop && atoi (prop)) || !obey_read_flag)
		flags |= CAMEL_MESSAGE_SEEN;

	prop = e2k_properties_get_prop (props, E2K_PR_HTTPMAIL_HAS_ATTACHMENT);
	if (prop && atoi (prop))
		flags |= CAMEL_MESSAGE_ATTACHMENTS;

	prop = e2k_properties_get_prop (props, PR_ACTION_FLAG);
	if (prop) {
		val = atoi (prop);
		if (val == MAPI_ACTION_FLAG_REPLIED_TO_SENDER)
			flags |= CAMEL_MESSAGE_ANSWERED;
		else if (val == MAPI_ACTION_FLAG_REPLIED_TO_ALL) {
			flags |= (CAMEL_MESSAGE_ANSWERED |
				  CAMEL_MESSAGE_ANSWERED_ALL);
		}
	}

	prop = e2k_properties_get_prop (props, PR_DELEGATED_BY_RULE);
	if (prop && atoi (prop))
		flags |= EXMAIL_DELEGATED;

	prop = e2k_properties_get_prop (props, PR_IMPORTANCE);
	if (prop) {
		val = atoi (prop);
		if (val == MAPI_IMPORTANCE_HIGH)
			flags |= CAMEL_MESSAGE_FLAGGED;
	}

	return flags;
}

/**
 * mail_util_extract_transport_headers:
 * @props: an #E2kProperties containing mail headers.
 *
 * Takes the %PR_TRANSPORT_MESSAGE_HEADERS property from @props and
 * cleans it up, returning just the RFC822 headers (slightly edited,
 * as described below).
 *
 * Return value: the headers, or %NULL if the property could not be
 * found or was malformed. You must free the return value when you
 * are done with it.
 **/
gchar *
mail_util_extract_transport_headers (E2kProperties *props)
{
	const gchar *prop, *hstart, *hend, *ctstart, *ctend;
	gchar *headers;

	prop = e2k_properties_get_prop (props, PR_TRANSPORT_MESSAGE_HEADERS);
	if (!prop)
		return NULL;

	/* The format is:
	 *
	 *     Microsoft Mail Internet Headers Version 2.0
	 *     [RFC822 headers here]
	 *
	 *     [MIME content boundaries and part headers here]
	 *
	 * The RFC822 headers are slightly modified from their original
	 * form: if there is raw 8-bit data in them, Exchange attempts
	 * to convert it to UTF8 (based on the charset of the message).
	 * Also, libxml translates "\r\n" to "\n".
	 *
	 * We strip off the MS header at the top and the MIME data
	 * at the bottom, and change the Content-Type header to claim
	 * that the body contains UTF-8-encoded plaintext. That way,
	 * when camel sees it later, it will treat the 8-bit data
	 * correctly (and it won't emit warnings about the lack of
	 * boundaries if the original message was multipart).
	 */
	hstart = strchr (prop, '\n');
	if (!hstart++)
		return NULL;
	hend = strstr (hstart, "\n\n");
	if (!hend)
		hend = hstart + strlen (hstart);

	ctstart = e_util_strstrcase (hstart - 1, "\nContent-Type:");
	if (ctstart && ctstart < hend) {
		ctend = strchr (ctstart, '\n');

		headers = g_strdup_printf ("%.*s\nContent-Type: text/plain; charset=\"UTF-8\"%.*s\n\n",
					   (gint) (ctstart - hstart), hstart,
					   (gint) (hend - ctend), ctend);
	} else {
		headers = g_strdup_printf ("%.*s\nContent-Type: text/plain; charset=\"UTF-8\"\n\n\n",
					   (gint) (hend - hstart), hstart);
	}

	return headers;
}

static const gchar *note_colors[] = {
	"#CCCCFF", "#CCFFCC", "#FFCCCC", "#FFFFCC", "#FFFFFF"
};
#define DEFAULT_NOTE_COLOR 3

/**
 * mail_util_stickynote_to_rfc822:
 * @props: a set of stickynote properties
 *
 * Creates a fake RFC822 message based on the stickynote described by
 * @props.
 *
 * Return value: the fake message, which must be freed with
 * g_string_free() when you are done with it.
 **/
GString *
mail_util_stickynote_to_rfc822 (E2kProperties *props)
{
	const gchar *prop;
	GString *message;
	gchar *html, *p;
	gint color;

	message = g_string_new (NULL);
	prop = e2k_properties_get_prop (props, E2K_PR_MAILHEADER_SUBJECT);
	if (prop)
		g_string_append_printf (message, "Subject: %s\r\n", prop);
	prop = e2k_properties_get_prop (props, E2K_PR_DAV_LAST_MODIFIED);
	if (prop) {
		time_t dt;
		gchar *buf;

		dt = e2k_parse_timestamp (prop);
		buf = e2k_make_timestamp_rfc822 (dt);
		g_string_append_printf (message, "Date: %s\r\n", buf);
		g_free (buf);
	}
	g_string_append (message, "Content-Type: text/html; charset=\"UTF-8\"\r\n\r\n");

	prop = e2k_properties_get_prop (props, E2K_PR_HTTPMAIL_TEXT_DESCRIPTION);
	if (!prop)
		return message;

	html = e_text_to_html (prop, (E_TEXT_TO_HTML_CONVERT_NL |
				      E_TEXT_TO_HTML_CONVERT_SPACES |
				      E_TEXT_TO_HTML_CONVERT_URLS));
	for (p = strchr (html, '\r'); p; p = strchr (p, '\r'))
		*p = ' ';

	g_string_append (message, "<html><body>\r\n");

	prop = e2k_properties_get_prop (props, E2K_PR_OUTLOOK_STICKYNOTE_COLOR);
	if (prop) {
		color = atoi (prop);
		if (color < 0 || color >= G_N_ELEMENTS (note_colors))
			color = DEFAULT_NOTE_COLOR;
	} else
		color = DEFAULT_NOTE_COLOR;

	g_string_append_printf (message, "<table bgcolor=\"%s\"",
				note_colors[color]);
	prop = e2k_properties_get_prop (props, E2K_PR_OUTLOOK_STICKYNOTE_WIDTH);
	if (prop)
		g_string_append_printf (message, " width=%s", prop);
	g_string_append (message, " border=1 cellpadding=10>\r\n<tr><td");
	prop = e2k_properties_get_prop (props, E2K_PR_OUTLOOK_STICKYNOTE_HEIGHT);
	if (prop)
		g_string_append_printf (message, " height=%s", prop);
	g_string_append (message, " valign=top>\r\n");
	g_string_append (message, html);
	g_string_append (message, "\r\n</td></tr>\r\n</table></body></html>");
	g_free (html);

	return message;
}

/**
 * mail_util_demangle_meeting_related_message:
 * @body: the body of the message
 * @owner_cn: the iCalendar "CN" (common name) of the delegator
 * @owner_email: the email address of the delegator
 * @owner_cal_uri: the exchange: URI of the delegator's Calendar
 * @subscriber_email: the email address of the delegatee
 *
 * When delegatees have to respond to meeting-related messages, Exchange
 * mangles the iCalendar, such that we need to demangle it in order to get the
 * iTIP control to do the right thing with it. That happens here.
 *
 *
 * Return value: %TRUE if we successfully demangled @body (in place).
 **/
gboolean
mail_util_demangle_meeting_related_message (GString *body,
				const gchar *owner_cn,
				const gchar *owner_email,
				const gchar *owner_cal_uri,
				const gchar *subscriber_email,
				MailUtilDemangleType unmangle_type)
{
	icalcomponent *vcal_comp, *event_comp;
	icalproperty *prop = NULL;
	gchar *vstart, *vend;
	gchar *ical_str, *str;
	gint oldlen, newlen;
	gboolean modify_prop = FALSE;

	vstart = strstr (body->str, "BEGIN:VCALENDAR");
	if (!vstart)
		return FALSE;
	vend = strstr (vstart, "END:VCALENDAR");
	if (!vend)
		return FALSE;
	vend += 13;
	while (isspace ((guchar)*vend))
		vend++;
	oldlen = vend - vstart;

	vcal_comp = icalparser_parse_string (vstart);
	if (!vcal_comp)
		return FALSE;

	event_comp = icalcomponent_get_first_component (vcal_comp, ICAL_VEVENT_COMPONENT);
	if (!event_comp) {
		icalcomponent_free (vcal_comp);
		return FALSE;
	}

	for (prop = icalcomponent_get_first_property (event_comp, ICAL_ATTENDEE_PROPERTY);
	     prop != NULL;
	     prop = icalcomponent_get_next_property (event_comp, ICAL_ATTENDEE_PROPERTY)) {
		const gchar *attendee;
		gchar *text = NULL;

		attendee = icalproperty_get_value_as_string_r (prop);
		if (!attendee)
			continue;

		if (!g_ascii_strncasecmp (attendee, "mailto:", 7))
			text = g_strdup (attendee+7);

		text = g_strstrip (text);
		if (text && !g_ascii_strcasecmp (owner_email, text)) {
			modify_prop = TRUE;
		/* We do not really need to set the CN parameter. However, setting it might improve usability. */
			icalproperty_remove_parameter_by_kind (prop, ICAL_CN_PARAMETER);
			icalproperty_add_parameter (prop, icalparameter_new_cn (g_strdup(owner_cn)));
			icalproperty_remove_parameter_by_kind (prop, ICAL_SENTBY_PARAMETER);
			icalproperty_add_parameter (prop,
				icalparameter_new_sentby (g_strdup_printf("MAILTO:%s", subscriber_email)));
			g_free (text);
			break;
		}
		g_free (text);
	}

	prop = icalcomponent_get_first_property (event_comp, ICAL_ORGANIZER_PROPERTY);
	if (!modify_prop && prop) {
		const gchar *organizer;
		gchar *text = NULL;

		organizer = icalproperty_get_value_as_string_r (prop);
		if (organizer) {
			if (!g_ascii_strncasecmp (organizer, "mailto:", 7))
				text = g_strdup (organizer+7);

			text = g_strstrip (text);

			switch (unmangle_type) {
				case MAIL_UTIL_DEMANGLE_DELGATED_MEETING:
					if (text && !g_ascii_strcasecmp (subscriber_email, text)) {
						icalproperty_set_organizer (prop, g_strdup_printf ("MAILTO:%s", owner_email));
						modify_prop = TRUE;
					}
					break;
				case MAIL_UTIL_DEMANGLE_MEETING_IN_SUBSCRIBED_INBOX:
				case MAIL_UTIL_DEMANGLE_SENDER_FIELD:
					if (text && !g_ascii_strcasecmp (owner_email, text))
						modify_prop = TRUE;
					break;
				default: break;
			}
			if (modify_prop) {
			/* We do not really need to set the CN parameter. However, setting it might improve usability. */
				icalproperty_remove_parameter_by_kind (prop, ICAL_CN_PARAMETER);
				icalproperty_add_parameter (prop, icalparameter_new_cn (g_strdup(owner_cn)));
				icalproperty_remove_parameter_by_kind (prop, ICAL_SENTBY_PARAMETER);
				icalproperty_add_parameter (prop,
					icalparameter_new_sentby (g_strdup_printf("MAILTO:%s", subscriber_email)));
			}
		}
		g_free (text);
	}

	switch (unmangle_type) {
		case MAIL_UTIL_DEMANGLE_DELGATED_MEETING:
		case MAIL_UTIL_DEMANGLE_MEETING_IN_SUBSCRIBED_INBOX: /* Now add the X-property */
			if (owner_cal_uri) {
				prop = icalproperty_new_x (owner_cal_uri);
				icalproperty_set_x_name (prop, "X-EVOLUTION-DELEGATOR-CALENDAR-URI");
				icalcomponent_add_property (event_comp, prop);
			}
			break;
		default: break;
	}

	/* Put the updated ical string back into the body */
	str = icalcomponent_as_ical_string_r (vcal_comp);
	ical_str = e2k_lf_to_crlf (str);
	g_free (str);
	newlen = strlen (ical_str);
	if (newlen < oldlen) {
		memcpy (vstart, ical_str, newlen);
		memcpy (vstart + newlen, vend, strlen (vend));
		g_string_set_size (body, body->len + newlen - oldlen);
	} else {
		g_string_set_size (body, body->len + newlen - oldlen);
		memmove (vstart + newlen, vend, strlen (vend));
		memcpy (vstart, ical_str, newlen);
	}

	icalcomponent_remove_component (vcal_comp, event_comp);
	icalcomponent_free (event_comp);

	icalcomponent_free (vcal_comp);
	g_free (ical_str);

	return TRUE;
}
