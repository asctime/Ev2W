/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		David Trowbridge <trowbrds@cs.colorado.edu>
 *		Gary Ekker <gekker@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef PUBLISH_LOCATION_H
#define PUBLISH_LOCATION_H

#include <glib.h>

G_BEGIN_DECLS

enum publish_frequency {
	URI_PUBLISH_DAILY,
	URI_PUBLISH_WEEKLY,
	URI_PUBLISH_MANUAL
};

static const gint publish_frequency_type_map[] = {
	URI_PUBLISH_DAILY,
	URI_PUBLISH_WEEKLY,
	URI_PUBLISH_MANUAL,
	-1,
};

enum publish_format {
	URI_PUBLISH_AS_ICAL,
	URI_PUBLISH_AS_FB
};

static const gint publish_format_type_mask[] = {
	URI_PUBLISH_AS_ICAL,
	URI_PUBLISH_AS_FB,
	-1
};

enum FBDurationType {
	FB_DURATION_DAYS,
	FB_DURATION_WEEKS,
	FB_DURATION_MONTHS
};

typedef struct _EPublishUri EPublishUri;
struct _EPublishUri {
	gboolean enabled;
	gchar *location;
	gint publish_frequency;
	gint publish_format;
	gchar *password;
	GSList *events;
	gchar *last_pub_time;
	gint fb_duration_value;
	gint fb_duration_type;

	gint service_type;
};

EPublishUri *e_publish_uri_from_xml (const gchar *xml);
gchar       *e_publish_uri_to_xml (EPublishUri *uri);

G_END_DECLS

#endif
