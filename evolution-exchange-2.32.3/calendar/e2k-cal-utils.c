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

#include "e2k-cal-utils.h"
#include <e2k-utils.h>

/**
 * e2k_timestamp_to_icaltime:
 * @timestamp: an Exchange timestamp string
 *
 * Converts @timestamp to an #icaltimetype
 *
 * Return value: the #icaltimetype
 **/
struct icaltimetype
e2k_timestamp_to_icaltime (const gchar *timestamp)
{
	return icaltime_from_timet_with_zone (
		e2k_parse_timestamp (timestamp), FALSE,
		icaltimezone_get_utc_timezone ());
}

/**
 * e2k_timestamp_from_icaltime:
 * @itt: an #icaltimetype
 *
 * Converts @itt to an Exchange timestamp string
 *
 * Return value: the timestamp, which the caller must free.
 **/
gchar *
e2k_timestamp_from_icaltime (struct icaltimetype itt)
{
	return e2k_make_timestamp (icaltime_as_timet_with_zone (itt, itt.zone));
}
