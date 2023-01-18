/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Copyright (C) 2000-2004 Novell, Inc.
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

#include <time.h>
#include "e-cal-backend-exchange-tasks.h"
#include <e2k-properties.h>
#include <e2k-propnames.h>
#include "libecal/e-cal-component.h"
#include "e2k-cal-utils.h"
#include <e2k-context.h>
#include <exchange-account.h>
#include <e-folder-exchange.h>
#include <e2k-operation.h>
#include <e2k-restriction.h>
#include <e2k-utils.h>

#define d(x)

/* Placeholder for each component and its recurrences */
typedef struct {
        ECalComponent *full_object;
        GHashTable *recurrences;
} ECalBackendExchangeTasksObject;

/* Private part of the ECalBackendExchangeTasks structure */
struct _ECalBackendExchangeTasksPrivate {
	/* URI where the task data is stored */
	gchar *uri;

	/* Top level VTODO component */
	icalcomponent *icalcomp;

	/* All the objects in the calendar, hashed by UID.  The
         * hash key *is* the uid returned by cal_component_get_uid(); it is not
         * copied, so don't free it when you remove an object from the hash
         * table. Each item in the hash table is a ECalBackendExchangeTasksObject.
         */
        GHashTable *comp_uid_hash;

	GList *comp;

	GMutex *mutex;

	gboolean is_loaded;

	gint dummy;
};

#define PARENT_TYPE E_TYPE_CAL_BACKEND_EXCHANGE
static ECalBackendExchange *parent_class = NULL;

static void
get_from (ECalBackendSync *backend, ECalComponent *comp, gchar **from_name, gchar **from_addr)
{
	if (!g_ascii_strcasecmp(e_cal_backend_exchange_get_owner_email (backend), exchange_account_get_email_id (E_CAL_BACKEND_EXCHANGE (backend)->account)))
		e_cal_backend_exchange_get_from (backend, comp, from_name, from_addr);
	else
		e_cal_backend_exchange_get_sender (backend, comp, from_name, from_addr);

#if 0
	ECalComponentOrganizer org;

	e_cal_component_get_organizer (E_CAL_COMPONENT (comp), &org);

	if (org.cn && org.cn[0] && org.value && org.value[0]) {
                *from_name = org.cn;
                if (!g_ascii_strncasecmp (org.value, "mailto:", 7))
                        *from_addr = org.value + 7;
                else
                        *from_addr = org.value;
        } else {
                *from_name = e_cal_backend_exchange_get_cal_owner (E_CAL_BACKEND_SYNC (backend));
                *from_addr = e_cal_backend_exchange_get_cal_address (E_CAL_BACKEND_SYNC (backend));
        }
#endif
}

static void
set_uid (E2kProperties *props, ECalComponent *comp)
{
        const gchar *uid;

	e_cal_component_get_uid (E_CAL_COMPONENT (comp), &uid);
        e2k_properties_set_string (props, E2K_PR_CALENDAR_UID, g_strdup (uid));
}

static void
set_summary (E2kProperties *props, ECalComponent *comp)
{
        static ECalComponentText summary;

	e_cal_component_get_summary (E_CAL_COMPONENT (comp), &summary);
        if (summary.value) {
                e2k_properties_set_string (props, E2K_PR_HTTPMAIL_THREAD_TOPIC,
                                           g_strdup (summary.value));
        } else
                e2k_properties_remove (props, E2K_PR_HTTPMAIL_THREAD_TOPIC);
}

static void
set_priority (E2kProperties *props, ECalComponent *comp)
{
        gint *priority, value = 0;

	e_cal_component_get_priority (E_CAL_COMPONENT (comp), &priority);
        if (priority) {
                if (*priority == 0)
                        value = 0;
                else if (*priority <= 4)
                        value = 1;
                else if (*priority == 5)
                        value = 0;
                else
                        value = -1;
                e_cal_component_free_priority (priority);
        }
        e2k_properties_set_int (props, E2K_PR_MAPI_PRIORITY, value);
}

static void
set_sensitivity (E2kProperties *props, ECalComponent *comp)
{
        ECalComponentClassification classif;
        gint sensitivity;

	e_cal_component_get_classification (E_CAL_COMPONENT (comp), &classif);
        switch (classif) {
        case E_CAL_COMPONENT_CLASS_PRIVATE:
                sensitivity = 2;
                break;
        case E_CAL_COMPONENT_CLASS_CONFIDENTIAL:
                sensitivity = 1;
                break;
        default:
                sensitivity = 0;
                break;
        }

	e2k_properties_set_int (props, E2K_PR_MAPI_SENSITIVITY, sensitivity);
}

icaltimezone *
get_default_timezone (void)
{
	GConfClient *client;
	icaltimezone *local_timezone;
	const gchar *key;
	gchar *location;

	client = gconf_client_get_default ();
	key = "/apps/evolution/calendar/display/timezone";
	location = gconf_client_get_string (client, key, NULL);

	if (location != NULL && *location != '\0')
		local_timezone = icaltimezone_get_builtin_timezone (location);
	else
		local_timezone = icaltimezone_get_utc_timezone ();

	g_free (location);

	g_object_unref (client);

	return local_timezone;
}

gchar *
calcomponentdatetime_to_string (ECalComponentDateTime *dt,
                                icaltimezone *izone)
{
        time_t tt;

	g_return_val_if_fail (dt != NULL, NULL);
        g_return_val_if_fail (dt->value != NULL, NULL);

	if (izone != NULL)
                tt = icaltime_as_timet_with_zone (*dt->value, izone);
        else
                tt = icaltime_as_timet (*dt->value);

	return e2k_make_timestamp (tt);
}

static gchar *
convert_to_utc (ECalComponentDateTime *dt)
{
        icaltimezone *from_zone;
        icaltimezone *utc_zone;

	from_zone = icaltimezone_get_builtin_timezone_from_tzid (dt->tzid);
        utc_zone = icaltimezone_get_utc_timezone ();
        if (!from_zone)
                from_zone = get_default_timezone ();
        dt->value->is_date = 0;
        icaltimezone_convert_time (dt->value, from_zone, utc_zone);

	return calcomponentdatetime_to_string (dt, utc_zone);
}

static void
set_dtstart (E2kProperties *props, ECalComponent *comp)
{
        ECalComponentDateTime dt;
        gchar *dtstart_str;

	e_cal_component_get_dtstart (E_CAL_COMPONENT (comp), &dt);
        if (!dt.value || icaltime_is_null_time (*dt.value)) {
                e_cal_component_free_datetime (&dt);
                e2k_properties_remove (props, E2K_PR_MAPI_COMMON_START);
                return;
        }

	dtstart_str = convert_to_utc (&dt);
        e_cal_component_free_datetime (&dt);
        e2k_properties_set_date (props, E2K_PR_MAPI_COMMON_START, dtstart_str);
}

static void
set_due_date (E2kProperties *props, ECalComponent *comp)
{
        ECalComponentDateTime dt;
        gchar *due_str;

	e_cal_component_get_due (E_CAL_COMPONENT (comp), &dt);
        if (!dt.value || icaltime_is_null_time (*dt.value)) {
                e_cal_component_free_datetime (&dt);
                e2k_properties_remove (props, E2K_PR_MAPI_COMMON_END);
                return;
        }

	due_str = convert_to_utc (&dt);
        e_cal_component_free_datetime (&dt);
        e2k_properties_set_date (props, E2K_PR_MAPI_COMMON_END, due_str);
}

gchar *
icaltime_to_e2k_time (struct icaltimetype *itt)
{
        time_t tt;

	g_return_val_if_fail (itt != NULL, NULL);

	tt = icaltime_as_timet_with_zone (*itt, icaltimezone_get_utc_timezone ());
        return e2k_make_timestamp (tt);
}

static void
set_date_completed (E2kProperties *props, ECalComponent *comp)
{
        struct icaltimetype *itt;
        gchar *tstr;

	e_cal_component_get_completed (E_CAL_COMPONENT (comp), &itt);
        if (!itt || icaltime_is_null_time (*itt)) {
                e2k_properties_remove (props, E2K_PR_OUTLOOK_TASK_DONE_DT);
                return;
        }

	icaltimezone_convert_time (itt,
				   icaltimezone_get_builtin_timezone ((const gchar *)itt->zone),
				   icaltimezone_get_utc_timezone ());
        tstr = icaltime_to_e2k_time (itt);
        e_cal_component_free_icaltimetype (itt);

	e2k_properties_set_date (props, E2K_PR_OUTLOOK_TASK_DONE_DT, tstr);
}

static void
set_status (E2kProperties *props, ECalComponent *comp)
{
        icalproperty_status ical_status;
        gint status;

	e_cal_component_get_status (E_CAL_COMPONENT (comp), &ical_status);
        switch (ical_status) {
        case ICAL_STATUS_NONE :
        case ICAL_STATUS_NEEDSACTION :
                /* Not Started */
                status = 0;
                break;
        case ICAL_STATUS_INPROCESS :
                /* In Progress */
                status = 1;
                break;
        case ICAL_STATUS_COMPLETED :
                /* Completed */
                status = 2;
                break;
        case ICAL_STATUS_CANCELLED :
                /* Deferred */
                status = 4;
                break;
        default :
                status = 0;
        }

	e2k_properties_set_int (props, E2K_PR_OUTLOOK_TASK_STATUS, status);
        e2k_properties_set_bool (props, E2K_PR_OUTLOOK_TASK_IS_DONE, status == 2);
}

static void
set_percent (E2kProperties *props, ECalComponent *comp)
{
        gint *percent;
        gfloat res;

	e_cal_component_get_percent (E_CAL_COMPONENT (comp), &percent);
        if (percent) {
                res = (gfloat) *percent / 100.0;
                e_cal_component_free_percent (percent);
        } else
                res = 0.;

	e2k_properties_set_float (props, E2K_PR_OUTLOOK_TASK_PERCENT, res);
}

static void
set_categories (E2kProperties *props, ECalComponent *comp)
{
        GSList *categories;
        GSList *sl;
        GPtrArray *array;

	e_cal_component_get_categories_list (E_CAL_COMPONENT (comp), &categories);
        if (!categories) {
                e2k_properties_remove (props, E2K_PR_EXCHANGE_KEYWORDS);
                return;
        }

	array = g_ptr_array_new ();
        for (sl = categories; sl != NULL; sl = sl->next) {
                gchar *cat = (gchar *) sl->data;

		if (cat)
                        g_ptr_array_add (array, g_strdup (cat));
        }
        e_cal_component_free_categories_list (categories);

	e2k_properties_set_string_array (props, E2K_PR_EXCHANGE_KEYWORDS, array);
}

static void
set_url (E2kProperties *props, ECalComponent *comp)
{
        const gchar *url;

	e_cal_component_get_url (E_CAL_COMPONENT (comp), &url);
        if (url)
                e2k_properties_set_string (props, E2K_PR_CALENDAR_URL, g_strdup (url));
        else
                e2k_properties_remove (props, E2K_PR_CALENDAR_URL);
}

static void
update_props (ECalComponent *comp, E2kProperties **properties)
{
	E2kProperties *props = *properties;

	set_uid (props, E_CAL_COMPONENT (comp));
	set_summary (props, E_CAL_COMPONENT (comp));
	set_priority (props, E_CAL_COMPONENT (comp));
	set_sensitivity (props, E_CAL_COMPONENT (comp));

	set_dtstart (props, E_CAL_COMPONENT (comp));
	set_due_date (props, E_CAL_COMPONENT (comp));
	set_date_completed (props, E_CAL_COMPONENT (comp));

	set_status (props, E_CAL_COMPONENT (comp));
	set_percent (props, E_CAL_COMPONENT (comp));

	set_categories (props, E_CAL_COMPONENT (comp));
	set_url (props, E_CAL_COMPONENT (comp));
}

static const gchar *
get_priority (ECalComponent *comp)
{
	gint *priority;
	const gchar *result;

	e_cal_component_get_priority (E_CAL_COMPONENT (comp), &priority);

	if (!priority)
		return "normal";

	if (*priority == 0)
		result = "normal";
	else if (*priority <= 4)
		result = "high";
	else if (*priority == 5)
		result = "normal";
	else
		result = "low";

	e_cal_component_free_priority (priority);

	return result;
}

static const gchar *
get_uid (ECalComponent *comp)
{
	const gchar *uid;

	e_cal_component_get_uid (E_CAL_COMPONENT(comp), &uid);
	return uid;
}

static const gchar *
get_summary (ECalComponent *comp)
{
	ECalComponentText summary;

	e_cal_component_get_summary(E_CAL_COMPONENT (comp), &summary);

	return summary.value;
}

static gint
put_body (ECalComponent *comp, E2kContext *ctx, E2kOperation *op,
         const gchar *uri, const gchar *from_name, const gchar *from_addr,
	 const gchar *attach_body, const gchar *boundary,
         gchar **repl_uid)

{
        GSList *desc_list;
        GString *desc;
        gchar *desc_crlf;
        gchar *body, *date;
        gint status;

        /* get the description */
        e_cal_component_get_description_list (E_CAL_COMPONENT (comp), &desc_list);
        desc = g_string_new ("");
        if (desc_list != NULL) {
                GSList *sl;

                for (sl = desc_list; sl; sl = sl->next) {
                        ECalComponentText *text = (ECalComponentText *) sl->data;

                        if (text)
                                desc = g_string_append (desc, text->value);
                }
        }

	/* PUT the component on the server */
        desc_crlf = e2k_lf_to_crlf ((const gchar *) desc->str);
        date = e2k_make_timestamp_rfc822 (time (NULL));

	if (attach_body) {
		body = g_strdup_printf ("content-class: urn:content-classes:task\r\n"
                                "Subject: %s\r\n"
                                "Date: %s\r\n"
                                "Message-ID: <%s>\r\n"
                                "MIME-Version: 1.0\r\n"
                                "Content-Type: multipart/mixed;\r\n"
				"\tboundary=\"%s\";\r\n"
				"X-MS_Has-Attach: yes\r\n"
                                "From: \"%s\" <%s>\r\n"
				"\r\n--%s\r\n"
				"content-class: urn:content-classes:task\r\n"
				"Content-Type: text/plain;\r\n"
                                "\tcharset=\"utf-8\"\r\n"
                                "Content-Transfer-Encoding: 8bit\r\n"
                                "Thread-Topic: %s\r\n"
                                "Priority: %s\r\n"
                                "Importance: %s\r\n"
                                "\r\n%s\r\n%s",
                                get_summary (comp),
                                date,
                                get_uid (comp),
				boundary,
				from_name ? from_name : "Evolution",
				from_addr ? from_addr : "",
				boundary,
                                get_summary (comp),
                                get_priority (comp),
                                get_priority (comp),
                                desc_crlf,
				attach_body);

	} else {
		body = g_strdup_printf ("content-class: urn:content-classes:task\r\n"
                                "Subject: %s\r\n"
                                "Date: %s\r\n"
                                "Message-ID: <%s>\r\n"
                                "MIME-Version: 1.0\r\n"
                                "Content-Type: text/plain;\r\n"
                                "\tcharset=\"utf-8\"\r\n"
                                "Content-Transfer-Encoding: 8bit\r\n"
                                "Thread-Topic: %s\r\n"
                                "Priority: %s\r\n"
                                "Importance: %s\r\n"
                                "From: \"%s\" <%s>\r\n"
                                "\r\n%s",
                                get_summary (comp),
                                date,
                                get_uid (comp),
                                get_summary (comp),
                                get_priority (comp),
                                get_priority (comp),
                                from_name ? from_name : "Evolution",
				from_addr ? from_addr : "",
                                desc_crlf);
	}

        status = e2k_context_put (ctx, NULL, uri, "message/rfc822",
				  body, strlen (body), NULL);

        /* free memory */
        g_free (body);
        g_free (desc_crlf);
        g_free (date);
        e_cal_component_free_text_list (desc_list);
        g_string_free (desc, TRUE);

        return status;
}

static const gchar *task_props[] = {
        E2K_PR_EXCHANGE_MESSAGE_CLASS,
        E2K_PR_DAV_UID,
        E2K_PR_CALENDAR_UID,
        E2K_PR_DAV_LAST_MODIFIED,
        E2K_PR_HTTPMAIL_SUBJECT,
        E2K_PR_HTTPMAIL_TEXT_DESCRIPTION,
        E2K_PR_HTTPMAIL_DATE,
	E2K_PR_HTTPMAIL_HAS_ATTACHMENT,
        E2K_PR_CALENDAR_LAST_MODIFIED,
        E2K_PR_HTTPMAIL_FROM_EMAIL,
        E2K_PR_HTTPMAIL_FROM_NAME,
        E2K_PR_MAILHEADER_IMPORTANCE,
        E2K_PR_MAPI_SENSITIVITY,
        E2K_PR_MAPI_COMMON_START,
        E2K_PR_MAPI_COMMON_END,
        E2K_PR_OUTLOOK_TASK_STATUS,
        E2K_PR_OUTLOOK_TASK_PERCENT,
        E2K_PR_OUTLOOK_TASK_DONE_DT,
        E2K_PR_EXCHANGE_KEYWORDS,
        E2K_PR_CALENDAR_URL
};

static guint
get_changed_tasks (ECalBackendExchange *cbex)
{
	ECalBackendExchangeComponent *ecalbexcomp;
	E2kRestriction *rn;
	E2kResultIter *iter;
	GPtrArray *hrefs, *array;
	GHashTable *modtimes, *attachments;
	GSList *attachment_list = NULL;
	E2kResult *result;
	E2kContext *ctx;
	const gchar *modtime, *str, *prop;
	gchar *uid;
	const gchar *tzid;
	gint status, i, priority, percent;
	gfloat f_percent;
	ECalComponent *ecal, *ecomp;
	struct icaltimetype itt;
	const icaltimezone *itzone;
	ECalComponentDateTime ecdatetime;
	icalcomponent *icalcomp;
	const gchar *since = NULL;
	ECalBackendExchangeTasks *cbext = E_CAL_BACKEND_EXCHANGE_TASKS (cbex);

        g_return_val_if_fail (E_IS_CAL_BACKEND_EXCHANGE (cbex), SOUP_STATUS_CANCELLED);

	g_mutex_lock (cbext->priv->mutex);

	rn = e2k_restriction_prop_string (E2K_PR_DAV_CONTENT_CLASS,
					  E2K_RELOP_EQ,
					  "urn:content-classes:task");

	e_cal_backend_exchange_cache_lock (cbex);
	if (since) {
		rn = e2k_restriction_andv (rn,
					   e2k_restriction_prop_date (
						   E2K_PR_DAV_LAST_MODIFIED,
						   E2K_RELOP_GT,
						   since),
					   NULL);
	} else
		e_cal_backend_exchange_cache_sync_start (cbex);
	e_cal_backend_exchange_cache_unlock (cbex);

        if (cbex->private_item_restriction) {
                e2k_restriction_ref (cbex->private_item_restriction);
                rn = e2k_restriction_andv (rn, cbex->private_item_restriction, NULL);
        }

        iter = e_folder_exchange_search_start (cbex->folder, NULL,
					       task_props,
					       G_N_ELEMENTS (task_props),
					       rn, NULL, TRUE);
        e2k_restriction_unref (rn);

	hrefs = g_ptr_array_new ();
	modtimes = g_hash_table_new_full (g_str_hash, g_str_equal,
					  g_free, g_free);
	attachments = g_hash_table_new_full (g_str_hash, g_str_equal,
					  g_free, g_free);

	while ((result = e2k_result_iter_next (iter))) {
		uid = e2k_properties_get_prop (result->props,
					       E2K_PR_CALENDAR_UID);
		if (!uid) {
			uid = e2k_properties_get_prop (result->props,
						       E2K_PR_DAV_UID);
		}
		if (!uid)
			continue;

		ecal = e_cal_component_new ();
		icalcomp = icalcomponent_new_vtodo ();
		e_cal_component_set_icalcomponent (ecal, icalcomp);
		e_cal_component_set_uid (ecal, (const gchar *)uid);

		modtime = e2k_properties_get_prop (result->props,
						   E2K_PR_DAV_LAST_MODIFIED);

		e_cal_backend_exchange_cache_lock (cbex);
		if (!e_cal_backend_exchange_in_cache (cbex, uid, modtime, result->href, NULL)) {
			g_ptr_array_add (hrefs, g_strdup (result->href));
			g_hash_table_insert (modtimes, g_strdup (result->href),
					     g_strdup (modtime));
		}
		e_cal_backend_exchange_cache_unlock (cbex);

		e_cal_backend_exchange_add_timezone (cbex, icalcomp, NULL);

		itt = icaltime_from_timet (e2k_parse_timestamp (modtime), 0);
		if (!icaltime_is_null_time (itt)) {
			e_cal_backend_exchange_ensure_utc_zone (E_CAL_BACKEND (cbex), &itt);
			e_cal_component_set_last_modified (ecal, &itt);
		}

		/* Set Priority */
		if ((str = e2k_properties_get_prop (result->props,
				E2K_PR_MAILHEADER_IMPORTANCE))) {
			if (!strcmp (str, "high"))
				priority = 3;
			else if (!strcmp (str, "low"))
				priority = 7;
			else if (!strcmp (str, "normal"))
				priority = 5;
			else
				priority = 0;
		} else
			priority = 5;
		e_cal_component_set_priority (ecal, &priority);

		/* Set Summary */
		if ((str = e2k_properties_get_prop (result->props,
				E2K_PR_HTTPMAIL_SUBJECT))) {
			ECalComponentText summary;
			summary.value = str;
			summary.altrep = result->href;
			e_cal_component_set_summary (E_CAL_COMPONENT (ecal), &summary);
		}

		/* Set DTSTAMP */
		if ((str = e2k_properties_get_prop (result->props,
				E2K_PR_HTTPMAIL_DATE))) {
			itt = icaltime_from_timet (e2k_parse_timestamp (str), 0);
			if (!icaltime_is_null_time (itt)) {
				e_cal_backend_exchange_ensure_utc_zone (E_CAL_BACKEND (cbex), &itt);

				e_cal_component_set_dtstamp (
					E_CAL_COMPONENT (ecal), &itt);
				e_cal_component_set_created (
					E_CAL_COMPONENT (ecal), &itt);
			}
		}

		/* Set DESCRIPTION */
		if ((str = e2k_properties_get_prop (result->props,
				E2K_PR_HTTPMAIL_TEXT_DESCRIPTION))) {
			GSList sl;
			ECalComponentText text;

			text.value = e2k_crlf_to_lf (str);
			text.altrep = result->href;
			sl.data = &text;
			sl.next = NULL;
			e_cal_component_set_description_list (E_CAL_COMPONENT (ecal), &sl);
			g_free ((gchar *)text.value);
		}

		/* Set DUE */
		if ((str = e2k_properties_get_prop (result->props, E2K_PR_MAPI_COMMON_END))) {
			itzone = get_default_timezone ();
			itt = icaltime_from_timet_with_zone (e2k_parse_timestamp (str), 0, itzone);
			if (!icaltime_is_null_time (itt)) {
				tzid = icaltimezone_get_tzid ((icaltimezone *)itzone);
				ecdatetime.value = &itt;
				ecdatetime.tzid = tzid;
				e_cal_component_set_due (ecal, &ecdatetime);
			}
		}

		/* Set DTSTART */
		if ((str = e2k_properties_get_prop (result->props,
				E2K_PR_MAPI_COMMON_START))) {
			itzone = get_default_timezone ();
			itt = icaltime_from_timet_with_zone (e2k_parse_timestamp (str), 0, itzone);
			if (!icaltime_is_null_time (itt)) {
				tzid = icaltimezone_get_tzid ((icaltimezone *)itzone);
				ecdatetime.value = &itt;
				ecdatetime.tzid = tzid;
				e_cal_component_set_dtstart (ecal, &ecdatetime);
			}
		}

		/* Set CLASSIFICATION */
		if ((str = e2k_properties_get_prop (result->props,
				E2K_PR_MAPI_SENSITIVITY))) {
			if (!strcmp (str, "0"))
				e_cal_component_set_classification (ecal,
					E_CAL_COMPONENT_CLASS_PUBLIC);
			else if (!strcmp (str, "1"))
				e_cal_component_set_classification (ecal,
					E_CAL_COMPONENT_CLASS_CONFIDENTIAL);
			else if (!strcmp (str, "2"))
				e_cal_component_set_classification (ecal,
					E_CAL_COMPONENT_CLASS_PRIVATE);
		}

		/* Set Percent COMPLETED */
		if ((str = e2k_properties_get_prop (result->props,
				E2K_PR_OUTLOOK_TASK_PERCENT))) {

			f_percent = atof (str);
			percent = (gint) (f_percent * 100);
			e_cal_component_set_percent (ecal, &percent);
		}

		/* Set STATUS */
		if ((str = e2k_properties_get_prop (result->props,
				E2K_PR_OUTLOOK_TASK_STATUS))) {
			if (!strcmp (str, "0")) {
				/* Not Started */
				e_cal_component_set_status (ecal,
					ICAL_STATUS_NEEDSACTION);
			} else if (!strcmp (str, "1")) {
				/* In Progress */
				e_cal_component_set_status (ecal,
					ICAL_STATUS_INPROCESS);
			} else if (!strcmp (str, "2")) {
				/* Completed */
				e_cal_component_set_status (ecal,
					ICAL_STATUS_COMPLETED);
			} else if (!strcmp (str, "3")) {
				/* Waiting on someone else */
				e_cal_component_set_status (ecal,
					ICAL_STATUS_INPROCESS);
			} else if (!strcmp (str, "4")) {
				/* Deferred */
				e_cal_component_set_status (ecal,
					ICAL_STATUS_CANCELLED);
			}
		}

		/* Set DATE COMPLETED */
		if ((str = e2k_properties_get_prop (result->props,
				E2K_PR_OUTLOOK_TASK_DONE_DT))) {
			itt = icaltime_from_timet (e2k_parse_timestamp (str), 0);
			if (!icaltime_is_null_time (itt))
				e_cal_component_set_completed (ecal, &itt);
		}

		/* Set LAST MODIFIED */
		if ((str = e2k_properties_get_prop (result->props,
				E2K_PR_CALENDAR_LAST_MODIFIED))) {
			itt = icaltime_from_timet (e2k_parse_timestamp(str), 0);
			if (!icaltime_is_null_time (itt)) {
				e_cal_backend_exchange_ensure_utc_zone (E_CAL_BACKEND (cbex), &itt);
				e_cal_component_set_last_modified (ecal, &itt);
			}
		}

		/* Set CATEGORIES */
		if ((array = e2k_properties_get_prop (result->props,
				E2K_PR_EXCHANGE_KEYWORDS))) {
			GSList *list = NULL;
			gint i;

			for (i = 0; i < array->len; i++)
				list = g_slist_prepend (list, array->pdata[i]);

			e_cal_component_set_categories_list (ecal, list);
			g_slist_free (list);
		}

		/* Set URL */
		if ((str = e2k_properties_get_prop (result->props,
				E2K_PR_CALENDAR_URL))) {
			e_cal_component_set_url (ecal, str);
		}

		/* Set Attachments */
		if ((str = e2k_properties_get_prop (result->props,
				E2K_PR_HTTPMAIL_HAS_ATTACHMENT))) {
			g_hash_table_insert (attachments, g_strdup (result->href),
				g_strdup (uid));
		}
		e_cal_component_commit_sequence (ecal);
		icalcomp = e_cal_component_get_icalcomponent (ecal);
		if (icalcomp) {
			gboolean status = FALSE;
			icalcomponent_kind kind = icalcomponent_isa (icalcomp);

			e_cal_backend_exchange_cache_lock (cbex);
			status = e_cal_backend_exchange_add_object (cbex, result->href,
					modtime, icalcomp);
			e_cal_backend_exchange_cache_unlock (cbex);

			if (status && kind == ICAL_VTODO_COMPONENT) {
				gchar *str = icalcomponent_as_ical_string_r (icalcomp);
				e_cal_backend_notify_object_created (E_CAL_BACKEND (cbex), str);
				g_free (str);
			}

		}

		g_object_unref (ecal);
	} /* End while */
	status = e2k_result_iter_free (iter);

	if (!SOUP_STATUS_IS_SUCCESSFUL (status)) {
		if (!since)
			e_cal_backend_exchange_cache_sync_end (cbex);

		g_ptr_array_free (hrefs, TRUE);
		g_hash_table_destroy (modtimes);
		g_hash_table_destroy (attachments);
		g_mutex_unlock (cbext->priv->mutex);
		return status;
	}

	e_cal_backend_exchange_cache_lock (cbex);
	if (!since)
		e_cal_backend_exchange_cache_sync_end (cbex);
	e_cal_backend_exchange_cache_unlock (cbex);

	if (!hrefs->len) {
		g_ptr_array_free (hrefs, TRUE);
		g_hash_table_destroy (modtimes);
		g_hash_table_destroy (attachments);
		cbext->priv->is_loaded = TRUE;
		g_mutex_unlock (cbext->priv->mutex);
		return SOUP_STATUS_OK;
	}

	prop = PR_INTERNET_CONTENT;
	iter = e_folder_exchange_bpropfind_start (cbex->folder, NULL,
						(const gchar **)hrefs->pdata,
						hrefs->len, &prop, 1);
	for (i = 0; i < hrefs->len; i++)
		g_free (hrefs->pdata[i]);
	g_ptr_array_set_size (hrefs, 0);

	while ((result = e2k_result_iter_next (iter))) {
		GByteArray *ical_data;

		/* XXX e2k_properties_get_prop() ought to return a GString. */
		ical_data = e2k_properties_get_prop (result->props, PR_INTERNET_CONTENT);
		if (!ical_data) {
			g_ptr_array_add (hrefs, g_strdup (result->href));
			continue;
		}

		uid = g_hash_table_lookup (attachments, result->href);
		/* Fetch component from cache and update it */

		e_cal_backend_exchange_cache_lock (cbex);
		ecalbexcomp = get_exchange_comp (cbex, uid);
		attachment_list = get_attachment (cbex, uid, (gchar *) ical_data->data, ical_data->len);
		if (attachment_list) {
			ecomp = e_cal_component_new ();
			e_cal_component_set_icalcomponent (ecomp, icalcomponent_new_clone (ecalbexcomp->icomp));
			e_cal_component_set_attachment_list (ecomp, attachment_list);
			icalcomponent_free (ecalbexcomp->icomp);
			ecalbexcomp->icomp = icalcomponent_new_clone (e_cal_component_get_icalcomponent (ecomp));
			g_object_unref (ecomp);
			g_slist_foreach (attachment_list, (GFunc) g_free, NULL);
			g_slist_free (attachment_list);
		}
		e_cal_backend_exchange_cache_unlock (cbex);
	}
	status = e2k_result_iter_free (iter);

	if (!SOUP_STATUS_IS_SUCCESSFUL (status)) {
		g_ptr_array_free (hrefs, TRUE);
		g_hash_table_destroy (attachments);
		g_mutex_unlock (cbext->priv->mutex);
		return status;
	}

	if (!hrefs->len) {
		g_ptr_array_free (hrefs, TRUE);
		g_hash_table_destroy (attachments);
		cbext->priv->is_loaded = TRUE;
		g_mutex_unlock (cbext->priv->mutex);
		return SOUP_STATUS_OK;
	}

	ctx = exchange_account_get_context (cbex->account);
	if (!ctx) {
		g_mutex_unlock (cbext->priv->mutex);
		/* This either means we lost connection or we are in offline mode */
		return SOUP_STATUS_CANT_CONNECT;
	}

	for (i = 0; i < hrefs->len; i++) {
		SoupBuffer *response;

		status = e2k_context_get (ctx, NULL, hrefs->pdata[i],
					  NULL, &response);
		if (!SOUP_STATUS_IS_SUCCESSFUL (status))
			continue;
		uid = g_hash_table_lookup (attachments, hrefs->pdata[i]);
		e_cal_backend_exchange_cache_lock (cbex);
		/* Fetch component from cache and update it */
		ecalbexcomp = get_exchange_comp (cbex, uid);
		attachment_list = get_attachment (cbex, uid, response->data, response->length);
		if (attachment_list) {
			ecomp = e_cal_component_new ();
			e_cal_component_set_icalcomponent (ecomp, icalcomponent_new_clone (ecalbexcomp->icomp));
			e_cal_component_set_attachment_list (ecomp, attachment_list);
			icalcomponent_free (ecalbexcomp->icomp);
			ecalbexcomp->icomp = icalcomponent_new_clone (e_cal_component_get_icalcomponent (ecomp));
			g_object_unref (ecomp);
			g_slist_foreach (attachment_list, (GFunc) g_free, NULL);
			g_slist_free (attachment_list);
		}
		e_cal_backend_exchange_cache_unlock (cbex);
		soup_buffer_free (response);
	}

	for (i = 0; i < hrefs->len; i++)
		g_free (hrefs->pdata[i]);
	g_ptr_array_free (hrefs, TRUE);
	g_hash_table_destroy (modtimes);
	g_hash_table_destroy (attachments);

	if (status == SOUP_STATUS_OK)
		cbext->priv->is_loaded = TRUE;

	g_mutex_unlock (cbext->priv->mutex);
	return status;
}

/* folder subscription notify callback */
static void
notify_changes (E2kContext *ctx, const gchar *uri,
                     E2kContextChangeType type, gpointer user_data)
{

	ECalBackendExchange *ecalbex = E_CAL_BACKEND_EXCHANGE (user_data);

	g_return_if_fail (E_IS_CAL_BACKEND_EXCHANGE (ecalbex));
	g_return_if_fail (uri != NULL);

	get_changed_tasks (ecalbex);

}

static void
open_task (ECalBackendSync *backend, EDataCal *cal,
	   gboolean only_if_exits,
	   const gchar *username, const gchar *password, GError **perror)
{
	GThread *thread = NULL;
	GError *error = NULL;
	ECalBackendExchangeTasks *cbext = E_CAL_BACKEND_EXCHANGE_TASKS (backend);

	E_CAL_BACKEND_SYNC_CLASS (parent_class)->open_sync (backend,
				     cal, only_if_exits, username, password, &error);
	if (error) {
		g_propagate_error (perror, error);
		return;
	}

	if (!e_cal_backend_exchange_is_online (E_CAL_BACKEND_EXCHANGE (backend))) {
		d(printf ("ECBEC : calendar is offline\n"));
		return;
	}

	if (cbext->priv->is_loaded)
		return;

	/* Subscribe to the folder to notice changes */
        e_folder_exchange_subscribe (E_CAL_BACKEND_EXCHANGE (backend)->folder,
                                        E2K_CONTEXT_OBJECT_CHANGED, 30,
                                        notify_changes, backend);
        e_folder_exchange_subscribe (E_CAL_BACKEND_EXCHANGE (backend)->folder,
                                        E2K_CONTEXT_OBJECT_ADDED, 30,
                                        notify_changes, backend);
        e_folder_exchange_subscribe (E_CAL_BACKEND_EXCHANGE (backend)->folder,
                                        E2K_CONTEXT_OBJECT_REMOVED, 30,
                                        notify_changes, backend);

	thread = g_thread_create ((GThreadFunc) get_changed_tasks, E_CAL_BACKEND_EXCHANGE (backend), FALSE, &error);
	if (!thread) {
		g_propagate_error (perror, EDC_ERROR_EX (OtherError, error->message));
		g_error_free (error);
	}
}

static void
refresh_task (ECalBackendSync *backend, EDataCal *cal, GError **perror)
{
	g_return_if_fail (E_IS_CAL_BACKEND_EXCHANGE (backend));

	get_changed_tasks (E_CAL_BACKEND_EXCHANGE (backend));
}

struct _cb_data {
        ECalBackendSync *be;
        icalcomponent *vcal_comp;
        EDataCal *cal;
};

static void
create_task_object (ECalBackendSync *backend, EDataCal *cal,
		    gchar **calobj, gchar **return_uid, GError **error)
{
	ECalBackendExchangeTasks *ecalbextask;
	ECalBackendExchange *ecalbex;
	E2kProperties *props;
	E2kContext *e2kctx;
	E2kHTTPStatus status;
	ECalComponent *comp;
	icalcomponent *icalcomp, *real_icalcomp;
	icalcomponent_kind kind;
	icalproperty *icalprop;
	struct icaltimetype current;
	gchar *from_name = NULL, *from_addr = NULL;
	gchar *boundary = NULL;
	gchar *attach_body = NULL;
	gchar *attach_body_crlf = NULL;
	const gchar *summary;
	gchar * modtime;
	gchar *location;
	const gchar *temp_comp_uid;

	ecalbextask = E_CAL_BACKEND_EXCHANGE_TASKS (backend);
	ecalbex = E_CAL_BACKEND_EXCHANGE (backend);

	e_return_data_cal_error_if_fail (calobj != NULL, InvalidArg);

	if (!e_cal_backend_exchange_is_online (E_CAL_BACKEND_EXCHANGE (backend))) {
		d(printf ("tasks are offline\n"));
		g_propagate_error (error, EDC_ERROR (RepositoryOffline));
		return;
	}

	/* Parse the icalendar text */
	icalcomp = icalparser_parse_string (*calobj);
	if (!icalcomp) {
		g_propagate_error (error, EDC_ERROR (InvalidObject));
		return;
	}

	/* Check kind with the parent */
	kind = e_cal_backend_get_kind (E_CAL_BACKEND (ecalbex));
        if (icalcomponent_isa (icalcomp) != kind) {
		icalcomponent_free (icalcomp);
		g_propagate_error (error, EDC_ERROR (InvalidObject));
		return;
        }

	current = icaltime_current_time_with_zone (icaltimezone_get_utc_timezone ());
	icalprop = icalcomponent_get_first_property (icalcomp, ICAL_CREATED_PROPERTY);
	if (icalprop)
		icalproperty_set_created (icalprop, current);
	else
		icalcomponent_add_property (icalcomp, icalproperty_new_created (current));

	icalprop = icalcomponent_get_first_property (icalcomp, ICAL_LASTMODIFIED_PROPERTY);
	if (icalprop)
		icalproperty_set_lastmodified (icalprop, current);
	else
		icalcomponent_add_property (icalcomp, icalproperty_new_lastmodified (current));

	modtime = e2k_timestamp_from_icaltime (current);

	/* Get the uid */
	temp_comp_uid = icalcomponent_get_uid (icalcomp);
	if (!temp_comp_uid) {
		icalcomponent_free (icalcomp);
		g_propagate_error (error, EDC_ERROR (InvalidObject));
		return;
	}

	e_cal_backend_exchange_cache_lock (ecalbex);
	/* check if the object is already present in our cache */
	if (e_cal_backend_exchange_in_cache (E_CAL_BACKEND_EXCHANGE (backend),
					     temp_comp_uid, modtime, NULL, NULL)) {
		e_cal_backend_exchange_cache_unlock (ecalbex);
		icalcomponent_free (icalcomp);
		g_propagate_error (error, EDC_ERROR (ObjectIdAlreadyExists));
		return;
	}
	e_cal_backend_exchange_cache_unlock (ecalbex);

	/* Delegated calendar */
	if (g_ascii_strcasecmp(e_cal_backend_exchange_get_owner_email (backend), exchange_account_get_email_id (ecalbex->account)))
		process_delegated_cal_object (icalcomp, e_cal_backend_exchange_get_owner_name (backend), e_cal_backend_exchange_get_owner_email (backend), exchange_account_get_email_id (ecalbex->account));

	summary = icalcomponent_get_summary (icalcomp);
	if (!summary)
		summary = "";

	/* Create the cal component */
        comp = e_cal_component_new ();
        e_cal_component_set_icalcomponent (comp, icalcomp);

	get_from (backend, comp, &from_name, &from_addr);

	/* Check for attachments */
	if (e_cal_component_has_attachments (comp)) {
		d(printf ("This task has attachments\n"));
		attach_body = build_msg (ecalbex, comp, summary, &boundary);
		attach_body_crlf = e_cal_backend_exchange_lf_to_crlf (attach_body);
	}

	props = e2k_properties_new ();

	/* FIXME Check for props and its members */

	e2k_properties_set_string (
		props, E2K_PR_EXCHANGE_MESSAGE_CLASS,
		g_strdup ("IPM.Task"));

	/* Magic number to make the context menu in Outlook work */
	e2k_properties_set_int (props, E2K_PR_MAPI_SIDE_EFFECTS, 272);

	/* I don't remember what happens if you don't set this. */
	e2k_properties_set_int (props, PR_ACTION, 1280);

	/* Various fields we don't support but should initialize
	 * so evo-created tasks look the same as Outlook-created
	 * ones.
	 */
	e2k_properties_set_bool (props, E2K_PR_MAPI_NO_AUTOARCHIVE, FALSE);
	e2k_properties_set_bool (props, E2K_PR_OUTLOOK_TASK_TEAM_TASK, FALSE);
	e2k_properties_set_bool (props, E2K_PR_OUTLOOK_TASK_RECURRING, FALSE);
	e2k_properties_set_int (props, E2K_PR_OUTLOOK_TASK_ACTUAL_WORK, 0);
	e2k_properties_set_int (props, E2K_PR_OUTLOOK_TASK_TOTAL_WORK, 0);
	e2k_properties_set_int (props, E2K_PR_OUTLOOK_TASK_ASSIGNMENT, 0);
	e2k_properties_set_string (props, E2K_PR_OUTLOOK_TASK_OWNER,
				   g_strdup (from_name));

	update_props (comp, &props);
	e_cal_component_commit_sequence (comp);
	*calobj = e_cal_component_get_as_string (comp);
	if (!*calobj) {
		g_object_unref (comp);
		g_free (from_name);
		g_free (from_addr);
		g_propagate_error (error, EDC_ERROR_EX (OtherError, "Cannot get ECalComp as string"));
		return;
	}

	real_icalcomp = icalparser_parse_string (*calobj);

	e2kctx = exchange_account_get_context (ecalbex->account);
	status = e_folder_exchange_proppatch_new (ecalbex->folder, NULL,
						  summary, NULL, NULL,
						  props, &location, NULL );

	if (E2K_HTTP_STATUS_IS_SUCCESSFUL (status)) {
		status = put_body(comp, e2kctx, NULL, location, from_name, from_addr,
						attach_body_crlf, boundary, NULL);
		if (E2K_HTTP_STATUS_IS_SUCCESSFUL (status)) {
			e_cal_backend_exchange_cache_lock (ecalbex);
			e_cal_backend_exchange_add_object (ecalbex, location, modtime, real_icalcomp);
			e_cal_backend_exchange_cache_unlock (ecalbex);
		}

		g_free (location);
		g_free (modtime);
	}

	if (!E2K_HTTP_STATUS_IS_SUCCESSFUL (status))
		g_propagate_error (error, EDC_ERROR_HTTP_STATUS (status));

	*return_uid = g_strdup (temp_comp_uid);
	icalcomponent_free (real_icalcomp);
	g_free (from_name);
	g_free (from_addr);
}

static void
modify_task_object (ECalBackendSync *backend, EDataCal *cal,
	       const gchar *calobj, CalObjModType mod,
	       gchar **old_object, gchar **new_object, GError **error)
{
	ECalBackendExchangeTasks *ecalbextask;
	ECalBackendExchangeComponent *ecalbexcomp;
	ECalComponent *cache_comp, *new_comp;
	ECalBackendExchange *ecalbex;
	E2kProperties *props;
	icalcomponent *icalcomp;
	const gchar * comp_uid, *summary;
	gchar *from_name, *from_addr;
	gchar *comp_str;
	gchar *attach_body = NULL;
	gchar *attach_body_crlf = NULL;
	gchar *boundary = NULL;
	struct icaltimetype current;
	E2kContext *e2kctx;
	E2kHTTPStatus status;

	ecalbextask = E_CAL_BACKEND_EXCHANGE_TASKS (backend);
	ecalbex = E_CAL_BACKEND_EXCHANGE (backend);

	e_return_data_cal_error_if_fail (E_IS_CAL_BACKEND_EXCHANGE_TASKS (ecalbextask), InvalidArg);
	e_return_data_cal_error_if_fail (calobj != NULL, InvalidArg);

	if (!e_cal_backend_exchange_is_online (E_CAL_BACKEND_EXCHANGE (backend))) {
		d(printf ("tasks are offline\n"));
		g_propagate_error (error, EDC_ERROR (RepositoryOffline));
		return;
	}

	/* Parse the icalendar text */
        icalcomp = icalparser_parse_string ((gchar *) calobj);
        if (!icalcomp) {
                g_propagate_error (error, EDC_ERROR (InvalidObject));
		return;
	}

	/* Check kind with the parent */
        if (icalcomponent_isa (icalcomp) !=
			e_cal_backend_get_kind (E_CAL_BACKEND (backend))) {
                icalcomponent_free (icalcomp);
		g_propagate_error (error, EDC_ERROR (InvalidObject));
		return;
        }

	/* Get the uid */
        comp_uid = icalcomponent_get_uid (icalcomp);

	e_cal_backend_exchange_cache_lock (ecalbex);
	/* Get the object from our cache */
	ecalbexcomp = get_exchange_comp (E_CAL_BACKEND_EXCHANGE (backend),
								comp_uid);

	if (!ecalbexcomp) {
		icalcomponent_free (icalcomp);
		e_cal_backend_exchange_cache_unlock (ecalbex);
		g_propagate_error (error, EDC_ERROR (ObjectNotFound));
		return;
	}

        cache_comp = e_cal_component_new ();
        e_cal_component_set_icalcomponent (cache_comp, icalcomponent_new_clone (ecalbexcomp->icomp));
	*old_object = e_cal_component_get_as_string (cache_comp);
	g_object_unref (cache_comp);

	e_cal_backend_exchange_cache_unlock (ecalbex);

	/* Delegated calendar */
	if (g_ascii_strcasecmp(e_cal_backend_exchange_get_owner_email (backend), exchange_account_get_email_id (ecalbex->account)))
		process_delegated_cal_object (icalcomp, e_cal_backend_exchange_get_owner_name (backend), e_cal_backend_exchange_get_owner_email (backend), exchange_account_get_email_id (ecalbex->account));

	summary = icalcomponent_get_summary (icalcomp);
	if (!summary)
		summary = "";

	/* Create the cal component */
        new_comp = e_cal_component_new ();
        e_cal_component_set_icalcomponent (new_comp, icalcomp);

	/* Set the last modified time on the component */
        current = icaltime_current_time_with_zone (icaltimezone_get_utc_timezone ());
        e_cal_component_set_last_modified (new_comp, &current);

	/* Set Attachments */
	if (e_cal_component_has_attachments (new_comp)) {
		d(printf ("This task has attachments for modifications\n"));
		attach_body = build_msg (ecalbex, new_comp, summary, &boundary);
		attach_body_crlf = e_cal_backend_exchange_lf_to_crlf (attach_body);
	}
	comp_str = e_cal_component_get_as_string (new_comp);
	icalcomp = icalparser_parse_string (comp_str);
	g_free (comp_str);
	if (!icalcomp) {
		g_propagate_error (error, EDC_ERROR_EX (OtherError, "Failed to parse comp_str"));
		return;
	}
	icalcomponent_free (icalcomp);

	get_from (backend, new_comp, &from_name, &from_addr);

        props = e2k_properties_new ();

	update_props (new_comp, &props);
	e_cal_component_commit_sequence (new_comp);

        e2kctx = exchange_account_get_context (ecalbex->account);
	status = e2k_context_proppatch (e2kctx, NULL, ecalbexcomp->href, props, FALSE, NULL);
	comp_str = e_cal_component_get_as_string (new_comp);
	icalcomp = icalparser_parse_string (comp_str);
	g_free (comp_str);
	if (E2K_HTTP_STATUS_IS_SUCCESSFUL (status)) {
		status = put_body(new_comp, e2kctx, NULL, ecalbexcomp->href, from_name, from_addr,
					attach_body_crlf, boundary, NULL);
		if (E2K_HTTP_STATUS_IS_SUCCESSFUL (status)) {
			e_cal_backend_exchange_cache_lock (ecalbex);
			e_cal_backend_exchange_modify_object (ecalbex, icalcomp, mod, FALSE);
			e_cal_backend_exchange_cache_unlock (ecalbex);
		}
	}
	icalcomponent_free (icalcomp);

	if (!E2K_HTTP_STATUS_IS_SUCCESSFUL (status))
		g_propagate_error (error, EDC_ERROR_HTTP_STATUS (status));
}

static void
receive_task_objects (ECalBackendSync *backend, EDataCal *cal,
                 const gchar *calobj, GError **error)
{
	ECalBackendExchangeTasks *ecalbextask;
	ECalBackendExchange *cbex;
        ECalComponent *ecalcomp;
        GList *comps, *l;
        struct icaltimetype current;
        icalproperty_method method;
        icalcomponent *subcomp;
	GError *err = NULL;

	ecalbextask = E_CAL_BACKEND_EXCHANGE_TASKS (backend);
	cbex = E_CAL_BACKEND_EXCHANGE (backend);

	e_return_data_cal_error_if_fail (E_IS_CAL_BACKEND_EXCHANGE_TASKS (ecalbextask), InvalidArg);
        e_return_data_cal_error_if_fail (calobj != NULL, InvalidArg);

	if (!e_cal_backend_exchange_is_online (E_CAL_BACKEND_EXCHANGE (backend))) {
		d(printf ("tasks are offline\n"));
		g_propagate_error (error, EDC_ERROR (RepositoryOffline));
		return;
	}

	if (!e_cal_backend_exchange_extract_components (calobj, &method, &comps, error))
		return;

	for (l = comps; l; l = l->next) {
                const gchar *uid;
                gchar *calobj, *rid = NULL;

                subcomp = l->data;

                ecalcomp = e_cal_component_new ();
                e_cal_component_set_icalcomponent (ecalcomp, subcomp);

                current = icaltime_current_time_with_zone (icaltimezone_get_utc_timezone ());
                e_cal_component_set_created (ecalcomp, &current);
                e_cal_component_set_last_modified (ecalcomp, &current);

                /*sanitize?*/

		e_cal_component_get_uid (ecalcomp, &uid);
                rid = e_cal_component_get_recurid_as_string (ecalcomp);

                /*see if the object is there in the cache. if found, modify object, else create object*/

		e_cal_backend_exchange_cache_lock (cbex);
                if (get_exchange_comp (E_CAL_BACKEND_EXCHANGE (ecalbextask), uid)) {
                        gchar *old_object;

			e_cal_backend_exchange_cache_unlock (cbex);
                        modify_task_object (backend, cal, calobj, CALOBJ_MOD_THIS, &old_object, NULL, &err);
                        if (err) {
				g_free (rid);
				g_propagate_error (error, err);
                                return;
			}

                        e_cal_backend_notify_object_modified (E_CAL_BACKEND (backend), old_object, calobj);
			g_free (old_object);
                } else {
                        gchar *returned_uid;

			e_cal_backend_exchange_cache_unlock (cbex);
			calobj = (gchar *) icalcomponent_as_ical_string_r (subcomp);
			create_task_object (backend, cal, &calobj, &returned_uid, &err);
                        if (err) {
				g_free (calobj);
				g_free (rid);
				g_propagate_error (error, err);
                                return;
			}

                        e_cal_backend_notify_object_created (E_CAL_BACKEND (backend), calobj);
			g_free (calobj);
                }
		g_free (rid);
        }

        g_list_free (comps);
}

static void
remove_task_object (ECalBackendSync *backend, EDataCal *cal,
	       const gchar *uid, const gchar *rid, CalObjModType mod,
	       gchar **old_object, gchar **object, GError **error)
{
	ECalBackendExchange *ecalbex = E_CAL_BACKEND_EXCHANGE (backend);
	ECalBackendExchangeComponent *ecalbexcomp;
	ECalComponent *comp;
	E2kContext *ctx;
	E2kHTTPStatus status;

	e_return_data_cal_error_if_fail (E_IS_CAL_BACKEND_EXCHANGE (ecalbex), InvalidArg);

	if (!e_cal_backend_exchange_is_online (E_CAL_BACKEND_EXCHANGE (backend))) {
		d(printf ("tasks are offline\n"));
		g_propagate_error (error, EDC_ERROR (RepositoryOffline));
		return;
	}

	e_cal_backend_exchange_cache_lock (ecalbex);
	ecalbexcomp = get_exchange_comp (ecalbex, uid);

	if (!ecalbexcomp || !ecalbexcomp->href) {
		e_cal_backend_exchange_cache_unlock (ecalbex);
		g_propagate_error (error, EDC_ERROR (ObjectNotFound));
		return;
	}

        comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (ecalbexcomp->icomp));
	*old_object = e_cal_component_get_as_string (comp);
	g_object_unref (comp);

	e_cal_backend_exchange_cache_unlock (ecalbex);

	ctx = exchange_account_get_context (ecalbex->account);

	status = e2k_context_delete (ctx, NULL, ecalbexcomp->href);
	if (E2K_HTTP_STATUS_IS_SUCCESSFUL (status)) {
		if (e_cal_backend_exchange_remove_object (ecalbex, uid))
			return;
	}

	g_propagate_error (error, EDC_ERROR_HTTP_STATUS (status));
}

static void
init (ECalBackendExchangeTasks *cbext)
{
	cbext->priv = g_new0 (ECalBackendExchangeTasksPrivate, 1);

	cbext->priv->mutex = g_mutex_new ();
	cbext->priv->is_loaded = FALSE;
}

static void
dispose (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
finalize (GObject *object)
{
	ECalBackendExchangeTasks *cbext =
		E_CAL_BACKEND_EXCHANGE_TASKS (object);

	if (cbext->priv->mutex) {
		g_mutex_free (cbext->priv->mutex);
		cbext->priv->mutex = NULL;
	}

	g_free (cbext->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
class_init (ECalBackendExchangeTasksClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	ECalBackendSyncClass *sync_class = E_CAL_BACKEND_SYNC_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	sync_class->open_sync = open_task;
	sync_class->refresh_sync = refresh_task;
	sync_class->create_object_sync = create_task_object;
	sync_class->modify_object_sync = modify_task_object;
	sync_class->remove_object_sync = remove_task_object;
	sync_class->receive_objects_sync = receive_task_objects;

	object_class->dispose = dispose;
	object_class->finalize = finalize;
}

E2K_MAKE_TYPE (e_cal_backend_exchange_tasks, ECalBackendExchangeTasks, class_init, init, PARENT_TYPE)
