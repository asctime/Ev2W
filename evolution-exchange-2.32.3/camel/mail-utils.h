/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* Copyright (C) 2001-2004 Novell, Inc. */

#ifndef __MAIL_UTILS_H__
#define __MAIL_UTILS_H__

#include <camel/camel.h>

#include "e2k-properties.h"

#define EXMAIL_DELEGATED CAMEL_MESSAGE_FOLDER_FLAGGED

G_BEGIN_DECLS

typedef enum {
	MAIL_UTIL_DEMANGLE_DELGATED_MEETING,
	MAIL_UTIL_DEMANGLE_MEETING_IN_SUBSCRIBED_INBOX,
	MAIL_UTIL_DEMANGLE_SENDER_FIELD
} MailUtilDemangleType;

gchar    *mail_util_mapi_to_smtp_headers (E2kProperties *props);

GString *mail_util_stickynote_to_rfc822 (E2kProperties *props);

guint32  mail_util_props_to_camel_flags (E2kProperties *props,
					 gboolean       obey_read_flag);

gchar *   mail_util_extract_transport_headers (E2kProperties *props);

gboolean
mail_util_demangle_meeting_related_message (GString *body,
				const gchar *owner_cn,
				const gchar *owner_email,
				const gchar *owner_cal_uri,
				const gchar *subscriber_email,
				MailUtilDemangleType unmangle_type);

G_END_DECLS

#endif /* __MAIL_UTILS_H__ */
