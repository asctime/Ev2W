/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors :
 *	Harish Krishnaswamy <kharish@novell.com>
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

#ifndef E_GW_RECUR_UTILS_H
#define E_GW_RECUR_UTILS_H

#include <glib.h>

#define E_GW_ITEM_RECURRENCE_FREQUENCY_DAILY "Daily"
#define E_GW_ITEM_RECURRENCE_FREQUENCY_WEEKLY "Weekly"
#define E_GW_ITEM_RECURRENCE_FREQUENCY_MONTHLY "Monthly"
#define E_GW_ITEM_RECURRENCE_FREQUENCY_YEARLY "Yearly"

#define E_GW_ITEM_RECUR_OCCURRENCE_TYPE_FIRST  "First"
#define E_GW_ITEM_RECUR_OCCURRENCE_TYPE_SECOND "Second"
#define E_GW_ITEM_RECUR_OCCURRENCE_TYPE_THIRD  "Third"
#define E_GW_ITEM_RECUR_OCCURRENCE_TYPE_FOURTH "Fourth"
#define E_GW_ITEM_RECUR_OCCURRENCE_TYPE_FIFTH  "Fifth"
#define E_GW_ITEM_RECUR_OCCURRENCE_TYPE_LAST   "Last"

/* XXX - an array would probably be better */
#define E_GW_ITEM_RECUR_WEEKDAY_SUNDAY "Sunday"
#define E_GW_ITEM_RECUR_WEEKDAY_MONDAY "Monday"
#define E_GW_ITEM_RECUR_WEEKDAY_TUESDAY "Tuesday"
#define E_GW_ITEM_RECUR_WEEKDAY_WEDNESDAY "Wednesday"
#define E_GW_ITEM_RECUR_WEEKDAY_THURSDAY "Thursday"
#define E_GW_ITEM_RECUR_WEEKDAY_FRIDAY "Friday"
#define E_GW_ITEM_RECUR_WEEKDAY_SATURDAY "Saturday"

#define E_GW_ITEM_BY_DAY_SIZE 364 /* 7 days * 52 weeks */
#define E_GW_ITEM_BY_MONTHDAY_SIZE 32
#define E_GW_ITEM_BY_YEARDAY_SIZE 367
#define E_GW_ITEM_BY_MONTH_SIZE 13
#define E_GW_ITEM_BY_SETPOS_SIZE 367

typedef struct {
	const gchar *frequency;
	gchar *until;
	gint count;
	gint interval;
	gshort by_day[E_GW_ITEM_BY_DAY_SIZE];
	gshort by_month_day[E_GW_ITEM_BY_MONTHDAY_SIZE];
	gshort by_year_day[E_GW_ITEM_BY_YEARDAY_SIZE];
	gshort by_month[E_GW_ITEM_BY_MONTH_SIZE];
	gshort by_setpos[E_GW_ITEM_BY_SETPOS_SIZE];
} EGwItemRecurrenceRule;

#define E_GW_ITEM_RECUR_END_MARKER  0x7f7f

const gchar *e_gw_recur_get_day_of_week (gshort day);

#endif
