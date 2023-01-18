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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>

#include <libecal/e-cal-check-timezones.h>

#include "e-cal-backend-exchange-calendar.h"

#include "e2k-cal-utils.h"
#include <e2k-freebusy.h>
#include <e2k-propnames.h>
#include <e2k-restriction.h>
#include <e2k-utils.h>
#include <e2k-xml-utils.h>
#include <e-folder-exchange.h>
#include <exchange-account.h>
#include <mapi.h>

struct ECalBackendExchangeCalendarPrivate {
	gint dummy;
	GMutex *mutex;
	gboolean is_loaded;
};

enum {
	EX_NO_RECEIPTS = 0,
	EX_DELIVERED_RECEIPTS,
	EX_READ_AND_DELIVERED,
	EX_ALL
};

#define PARENT_TYPE E_TYPE_CAL_BACKEND_EXCHANGE
static ECalBackendExchange *parent_class = NULL;

#define d(x)

static gboolean modify_object_with_href (ECalBackendSync *backend, EDataCal *cal, const gchar *calobj, CalObjModType mod, gchar **old_object, gchar **new_object, const gchar *href, const gchar *rid_to_remove, GError **error);

static icalproperty *find_attendee_prop (icalcomponent *ical_comp, const gchar *address);
static gboolean check_owner_partstatus_for_declined (ECalBackendSync *backend,
						     icalcomponent *icalcomp);

gboolean check_for_send_options (icalcomponent *icalcomp, E2kProperties *props);
static void update_x_properties (ECalBackendExchange *cbex, ECalComponent *comp);

static void
add_timezones_from_comp (ECalBackendExchange *cbex, icalcomponent *icalcomp)
{
	icalcomponent *subcomp;

	switch (icalcomponent_isa (icalcomp)) {
	case ICAL_VTIMEZONE_COMPONENT:
		e_cal_backend_exchange_add_timezone (cbex, icalcomp, NULL);
		break;

	case ICAL_VCALENDAR_COMPONENT:
		subcomp = icalcomponent_get_first_component (
			icalcomp, ICAL_VTIMEZONE_COMPONENT);
		while (subcomp) {
			e_cal_backend_exchange_add_timezone (cbex, subcomp, NULL);
			subcomp = icalcomponent_get_next_component (
				icalcomp, ICAL_VTIMEZONE_COMPONENT);
		}
		break;

	default:
		break;
	}
}

static gboolean
add_vevent (ECalBackendExchange *cbex,
	    const gchar *href, const gchar *lastmod,
	    icalcomponent *icalcomp)
{
	icalproperty *prop, *transp;
	gboolean res;

	/* We have to do this here, since if we do it inside the loop
	 * it will mess up the ICAL_X_PROPERTY iterator.
	 */
	transp = icalcomponent_get_first_property (icalcomp, ICAL_TRANSP_PROPERTY);

	/* Check all X-MICROSOFT-CDO properties to fix any needed stuff */
	prop = icalcomponent_get_first_property (icalcomp, ICAL_X_PROPERTY);
	while (prop) {
		const gchar *x_name, *x_val;
		struct icaltimetype itt;

		x_name = icalproperty_get_x_name (prop);
		x_val = icalproperty_get_x (prop);

		if (!strcmp (x_name, "X-MICROSOFT-CDO-ALLDAYEVENT") &&
		    !strcmp (x_val, "TRUE")) {
			/* All-day event. Fix DTSTART/DTEND to be DATE
			 * values rather than DATE-TIME.
			 */
			itt = icalcomponent_get_dtstart (icalcomp);
			itt.is_date = TRUE;
			itt.hour = itt.minute = itt.second = 0;
			icalcomponent_set_dtstart (icalcomp, itt);

			itt = icalcomponent_get_dtend (icalcomp);
			itt.is_date = TRUE;
			itt.hour = itt.minute = itt.second = 0;
			icalcomponent_set_dtend (icalcomp, itt);
		}

		if (!strcmp (x_name, "X-MICROSOFT-CDO-BUSYSTATUS")) {
			/* It seems OWA sometimes doesn't set the
			 * TRANSP property, so set it from the busy
			 * status.
			 */
			if (transp) {
				icalcomponent_remove_property (icalcomp,transp);
				icalproperty_free (transp);
				transp = NULL;
			}

			if (!strcmp (x_val, "BUSY"))
				transp = icalproperty_new_transp (ICAL_TRANSP_OPAQUE);
			else if (!strcmp (x_val, "FREE"))
				transp = icalproperty_new_transp (ICAL_TRANSP_TRANSPARENT);

			if (transp)
				icalcomponent_add_property (icalcomp, transp);
		}

		prop = icalcomponent_get_next_property (icalcomp, ICAL_X_PROPERTY);
	}

	/* OWA seems to be broken, and sets the component class to
	 * "CLASS:", by which it means PUBLIC. Evolution treats this
	 * as PRIVATE, so we have to work around.
	 */
	prop = icalcomponent_get_first_property (icalcomp, ICAL_CLASS_PROPERTY);
	if (!prop) {
		prop = icalproperty_new_class (ICAL_CLASS_PUBLIC);
		icalcomponent_add_property (icalcomp, prop);
	}

	/* Exchange sets an ORGANIZER on all events. RFC2445 says:
	 *
	 *   This property MUST NOT be specified in an iCalendar
	 *   object that specifies only a time zone definition or
	 *   that defines calendar entities that are not group
	 *   scheduled entities, but are entities only on a single
	 *   user's calendar.
	 */
	prop = icalcomponent_get_first_property (icalcomp, ICAL_ORGANIZER_PROPERTY);
	if (prop && !icalcomponent_get_first_property (icalcomp, ICAL_ATTENDEE_PROPERTY)) {
		icalcomponent_remove_property (icalcomp, prop);
		icalproperty_free (prop);
	}

	e_cal_backend_exchange_cache_lock (cbex);
	/* Now add to the cache */
	res = e_cal_backend_exchange_add_object (cbex, href, lastmod, icalcomp);
	e_cal_backend_exchange_cache_unlock (cbex);

	return res;
}

/* Add the event to the cache, Notify the backend if it is sucessfully added */
static gboolean
add_ical (ECalBackendExchange *cbex, const gchar *href, const gchar *lastmod,
	  const gchar *uid, const gchar *body, gint len, gint receipts)
{
	const gchar *start, *end;
	gchar *ical_body;
	icalcomponent *icalcomp, *subcomp, *new_comp;
	icalcomponent_kind kind;
	icalproperty *icalprop;
	ECalComponent *ecomp;
	GSList *attachment_list = NULL;
	gboolean status;
	ECalBackend *backend = E_CAL_BACKEND (cbex);
	GError *error = NULL;
	gboolean retval = TRUE;

	/* Check for attachments */
	if (uid)
		attachment_list = get_attachment (cbex, uid, body, len);

	start = g_strstr_len (body, len, "\nBEGIN:VCALENDAR");
	if (!start)
		return FALSE;
	start++;
	end = g_strstr_len (start, len - (start - body), "\nEND:VCALENDAR");
	if (!end)
		return FALSE;
	end += sizeof ("\nEND:VCALENDAR");

	ical_body = g_strndup (start, end - start);
	icalcomp = icalparser_parse_string (ical_body);
	g_free (ical_body);
	if (!icalcomp)
		return FALSE;

	if (!icalcomponent_get_uid (icalcomp)) {
		icalcomponent_free (icalcomp);
		return FALSE;
	}

	kind = icalcomponent_isa (icalcomp);
	if (kind == ICAL_VEVENT_COMPONENT) {
		if (receipts) {
			icalprop = icalproperty_new_x (g_strdup (GINT_TO_POINTER (receipts)));
			icalproperty_set_x_name (icalprop, "X-EVOLUTION-OPTIONS-TRACKINFO");
			icalcomponent_add_property (icalcomp, icalprop);
		}
		if (attachment_list) {
			ecomp = e_cal_component_new ();
			e_cal_component_set_icalcomponent (ecomp, icalcomp);
			e_cal_component_set_attachment_list (ecomp, attachment_list);
			icalcomp = icalcomponent_new_clone (e_cal_component_get_icalcomponent (ecomp));
			g_object_unref (ecomp);
			g_slist_foreach (attachment_list, (GFunc) g_free, NULL);
			g_slist_free (attachment_list);
		}
		status = add_vevent (cbex, href, lastmod, icalcomp);

		if (status) {
			gchar *object = icalcomponent_as_ical_string_r (icalcomp);
			e_cal_backend_notify_object_created (backend, object);
			g_free (object);
		}

		icalcomponent_free (icalcomp);
		return status;
	} else if (kind != ICAL_VCALENDAR_COMPONENT) {
		retval = FALSE;
		goto cleanup;
	}

	/* map time zones against system time zones and handle conflicting definitions */
	if (!e_cal_check_timezones (icalcomp,
				    NULL,
				    e_cal_backend_exchange_lookup_timezone,
				    cbex,
				    &error)) {
		g_warning ("checking timezones failed: %s", error->message);
		g_clear_error (&error);
		retval = FALSE;
		goto cleanup;
	}

	add_timezones_from_comp (cbex, icalcomp);

	subcomp = icalcomponent_get_first_component (
		icalcomp, ICAL_VEVENT_COMPONENT);
	while (subcomp) {
		if (uid && !strcmp (uid, icalcomponent_get_uid (subcomp)) && attachment_list) {
			ecomp = e_cal_component_new ();
			e_cal_component_set_icalcomponent (ecomp, icalcomponent_new_clone (subcomp));
			e_cal_component_set_attachment_list (ecomp, attachment_list);
			new_comp = icalcomponent_new_clone (e_cal_component_get_icalcomponent (ecomp));
			g_object_unref (ecomp);
		} else {
			new_comp = icalcomponent_new_clone (subcomp);
		}

		if (new_comp) {
			status = add_vevent (cbex, href, lastmod, new_comp);

			if (status) {
				gchar *object = icalcomponent_as_ical_string_r (new_comp);
				e_cal_backend_notify_object_created (backend, object);
				g_free (object);
			}

			icalcomponent_free (new_comp);
		}
		subcomp = icalcomponent_get_next_component (
				icalcomp, ICAL_VEVENT_COMPONENT);
	}

 cleanup:
	icalcomponent_free (icalcomp);

	if (attachment_list) {
		g_slist_foreach (attachment_list, (GFunc) g_free, NULL);
		g_slist_free (attachment_list);
	}
	return retval;
}

static const gchar *event_properties[] = {
	E2K_PR_CALENDAR_UID,
	PR_CAL_RECURRING_ID,
	E2K_PR_DAV_LAST_MODIFIED,
	E2K_PR_HTTPMAIL_HAS_ATTACHMENT,
	PR_READ_RECEIPT_REQUESTED,
	PR_ORIGINATOR_DELIVERY_REPORT_REQUESTED
};
static const gint n_event_properties = G_N_ELEMENTS (event_properties);

static const gchar *new_event_properties[] = {
	PR_INTERNET_CONTENT,
	PR_READ_RECEIPT_REQUESTED,
	PR_ORIGINATOR_DELIVERY_REPORT_REQUESTED
};
static const gint n_new_event_properties = G_N_ELEMENTS (new_event_properties);

static guint
get_changed_events (ECalBackendExchange *cbex)
{
	GPtrArray *hrefs;
	GHashTable *modtimes;
	GHashTable *attachments;
	E2kRestriction *rn;
	E2kResultIter *iter;
	E2kResult *result;
	const gchar *prop, *uid, *modtime, *attach_prop, *receipts, *rid;
	guint status;
	E2kContext *ctx;
	gint i, status_tracking = EX_NO_RECEIPTS;
	const gchar *since = NULL;
	ECalBackendExchangeCalendar *cbexc = E_CAL_BACKEND_EXCHANGE_CALENDAR (cbex);

	g_return_val_if_fail (E_IS_CAL_BACKEND_EXCHANGE (cbex), SOUP_STATUS_CANCELLED);

	g_mutex_lock (cbexc->priv->mutex);

	rn = e2k_restriction_andv (
		e2k_restriction_prop_string (E2K_PR_DAV_CONTENT_CLASS,
					     E2K_RELOP_EQ,
					     "urn:content-classes:appointment"),
		e2k_restriction_orv (
			e2k_restriction_prop_int (E2K_PR_CALENDAR_INSTANCE_TYPE,
						  E2K_RELOP_EQ, cdoSingle),
			e2k_restriction_prop_int (E2K_PR_CALENDAR_INSTANCE_TYPE,
						  E2K_RELOP_EQ, cdoMaster),
			e2k_restriction_prop_int (E2K_PR_CALENDAR_INSTANCE_TYPE,
						  E2K_RELOP_EQ, cdoException),
			NULL),
		NULL);
	if (cbex->private_item_restriction) {
		e2k_restriction_ref (cbex->private_item_restriction);
		rn = e2k_restriction_andv (rn,
					   cbex->private_item_restriction,
					   NULL);
	}
	e_cal_backend_exchange_cache_lock (cbex);
	if (since) {
		rn = e2k_restriction_andv (
			rn,
			e2k_restriction_prop_date (E2K_PR_DAV_LAST_MODIFIED,
						   E2K_RELOP_GT, since),
			NULL);
	} else
		e_cal_backend_exchange_cache_sync_start (cbex);
	e_cal_backend_exchange_cache_unlock (cbex);

	iter = e_folder_exchange_search_start (cbex->folder, NULL,
					       event_properties,
					       n_event_properties,
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
		if (!uid)
			continue;
		modtime = e2k_properties_get_prop (result->props,
						   E2K_PR_DAV_LAST_MODIFIED);
		rid = e2k_properties_get_prop (result->props, PR_CAL_RECURRING_ID);

		attach_prop = e2k_properties_get_prop (result->props,
						E2K_PR_HTTPMAIL_HAS_ATTACHMENT);

		receipts = e2k_properties_get_prop (result->props,
						PR_ORIGINATOR_DELIVERY_REPORT_REQUESTED);
		if (receipts && atoi (receipts))
			status_tracking = EX_DELIVERED_RECEIPTS;

		receipts = NULL;
		receipts = e2k_properties_get_prop (result->props,
						PR_READ_RECEIPT_REQUESTED);
		if (receipts && atoi (receipts)) {
			if (status_tracking == EX_DELIVERED_RECEIPTS)
				status_tracking = EX_ALL;
			else
				status_tracking = EX_READ_AND_DELIVERED;
		}

		e_cal_backend_exchange_cache_lock (cbex);
		if (!e_cal_backend_exchange_in_cache (cbex, uid, modtime, result->href, rid)) {
			g_ptr_array_add (hrefs, g_strdup (result->href));
			g_hash_table_insert (modtimes, g_strdup (result->href),
					     g_strdup (modtime));
			if (attach_prop && atoi (attach_prop))
				g_hash_table_insert (attachments, g_strdup (result->href),
						g_strdup (uid));
		}
		e_cal_backend_exchange_cache_unlock (cbex);
	}
	status = e2k_result_iter_free (iter);

	if (!SOUP_STATUS_IS_SUCCESSFUL (status)) {
		if (!since)
			e_cal_backend_exchange_cache_sync_end (cbex);

		g_ptr_array_free (hrefs, TRUE);
		g_hash_table_destroy (modtimes);
		g_hash_table_destroy (attachments);
		g_mutex_unlock (cbexc->priv->mutex);
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
		cbexc->priv->is_loaded = TRUE;
		g_mutex_unlock (cbexc->priv->mutex);
		return SOUP_STATUS_OK;
	}

	/* Now get the full text of any that weren't already cached. */
	/* OWA usually sends the attachment and whole event body as part of
		PR_INTERNET_CONTENT property. Fetch events created from OWA */
	prop = PR_INTERNET_CONTENT;
	iter = e_folder_exchange_bpropfind_start (cbex->folder, NULL,
						  (const gchar **)hrefs->pdata,
						  hrefs->len,
						  new_event_properties, n_new_event_properties);
	for (i = 0; i < hrefs->len; i++)
		g_free (hrefs->pdata[i]);
	g_ptr_array_set_size (hrefs, 0);

	while ((result = e2k_result_iter_next (iter))) {
		GByteArray *ical_data;
		status_tracking = EX_NO_RECEIPTS;

		/* XXX e2k_properties_get_prop() ought to return a GString. */
		ical_data = e2k_properties_get_prop (result->props, PR_INTERNET_CONTENT);
		if (!ical_data) {
			/* We didn't get the body, so postpone. */
			g_ptr_array_add (hrefs, g_strdup (result->href));
			continue;
		}
		receipts = e2k_properties_get_prop (result->props,
					PR_ORIGINATOR_DELIVERY_REPORT_REQUESTED);
		if (receipts && atoi (receipts))
			status_tracking = EX_DELIVERED_RECEIPTS;

		receipts = NULL;
		receipts = e2k_properties_get_prop (result->props,
					PR_READ_RECEIPT_REQUESTED);
		if (receipts && atoi (receipts)) {
			if (status_tracking == EX_DELIVERED_RECEIPTS)
				status_tracking = EX_ALL;
			else
				status_tracking = EX_READ_AND_DELIVERED;
		}

		modtime = g_hash_table_lookup (modtimes, result->href);
		uid = g_hash_table_lookup (attachments, result->href);
		/* The icaldata already has the attachment. So no need to
			re-fetch it from the server. */
		add_ical (cbex, result->href, modtime, uid,
			  (gchar *) ical_data->data, ical_data->len, status_tracking);
	}
	status = e2k_result_iter_free (iter);

	if (!SOUP_STATUS_IS_SUCCESSFUL (status)) {
		g_ptr_array_free (hrefs, TRUE);
		g_hash_table_destroy (modtimes);
		g_hash_table_destroy (attachments);
		g_mutex_unlock (cbexc->priv->mutex);
		return status;
	}
	if (!hrefs->len) {
		g_ptr_array_free (hrefs, TRUE);
		g_hash_table_destroy (modtimes);
		g_hash_table_destroy (attachments);
		cbexc->priv->is_loaded = TRUE;
		g_mutex_unlock (cbexc->priv->mutex);
		return SOUP_STATUS_OK;
	}

	/* Get the remaining ones the hard way */
	ctx = exchange_account_get_context (cbex->account);
	if (!ctx) {
		g_mutex_unlock (cbexc->priv->mutex);
		/* This either means we lost connection or we are in offline mode */
		return SOUP_STATUS_CANT_CONNECT;
	}
	for (i = 0; i < hrefs->len; i++) {
		SoupBuffer *response;

		status = e2k_context_get (ctx, NULL, hrefs->pdata[i],
					  NULL, &response);
		if (!SOUP_STATUS_IS_SUCCESSFUL (status))
			continue;
		modtime = g_hash_table_lookup (modtimes, hrefs->pdata[i]);

		uid = g_hash_table_lookup (attachments, hrefs->pdata[i]);

		add_ical (cbex, hrefs->pdata[i], modtime, uid,
			  response->data, response->length, 0);
		soup_buffer_free (response);
	}

	for (i = 0; i < hrefs->len; i++)
		g_free (hrefs->pdata[i]);
	g_ptr_array_free (hrefs, TRUE);
	g_hash_table_destroy (modtimes);
	g_hash_table_destroy (attachments);

	if (status == SOUP_STATUS_OK)
		cbexc->priv->is_loaded = TRUE;

	g_mutex_unlock (cbexc->priv->mutex);
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

	get_changed_events (ecalbex);

}

static void
open_calendar (ECalBackendSync *backend, EDataCal *cal,
	       gboolean only_if_exists,
	       const gchar *username, const gchar *password, GError **perror)
{
	GThread *thread = NULL;
	GError *error = NULL;
	ECalBackendExchangeCalendar *cbexc = E_CAL_BACKEND_EXCHANGE_CALENDAR (backend);

	/* Do the generic part */
	E_CAL_BACKEND_SYNC_CLASS (parent_class)->open_sync (
		backend, cal, only_if_exists, username, password, &error);
	if (error) {
		g_propagate_error (perror, error);
		return;
	}

	if (!e_cal_backend_exchange_is_online (E_CAL_BACKEND_EXCHANGE (backend))) {
		return; /* Success */
	}

	if (cbexc->priv->is_loaded) {
		return; /*Success */
	}

	e_folder_exchange_subscribe (E_CAL_BACKEND_EXCHANGE (backend)->folder,
			E2K_CONTEXT_OBJECT_CHANGED, 30,
			notify_changes, backend);
        e_folder_exchange_subscribe (E_CAL_BACKEND_EXCHANGE (backend)->folder,
			E2K_CONTEXT_OBJECT_ADDED, 30,
			notify_changes, backend);
        e_folder_exchange_subscribe (E_CAL_BACKEND_EXCHANGE (backend)->folder,
			E2K_CONTEXT_OBJECT_REMOVED, 30,
			notify_changes, backend);

	thread = g_thread_create ((GThreadFunc) get_changed_events, E_CAL_BACKEND_EXCHANGE (backend), FALSE, &error);
	if (!thread) {
		g_warning (G_STRLOC ": %s", error->message);
		g_propagate_error (perror, EDC_ERROR_EX (OtherError, error->message));
		g_error_free (error);
	}
}

static void
refresh_calendar (ECalBackendSync *backend, EDataCal *cal, GError **perror)
{
	g_return_if_fail (E_IS_CAL_BACKEND_EXCHANGE (backend));

	get_changed_events (E_CAL_BACKEND_EXCHANGE (backend));
}

struct _cb_data {
	ECalBackendSync *be;
	icalcomponent *vcal_comp;
	EDataCal *cal;
};

static void
add_timezone_cb (icalparameter *param, gpointer data)
{
	struct _cb_data *cbdata = (struct _cb_data *) data;
	icalcomponent *vtzcomp;
	const gchar *tzid;
	icaltimezone *zone = NULL;

	g_return_if_fail (cbdata != NULL);

	tzid = icalparameter_get_tzid (param);
	if (tzid == NULL)
		return;
	if (icalcomponent_get_timezone (cbdata->vcal_comp, tzid))
		return;

	zone = e_cal_backend_internal_get_timezone ((ECalBackend *) cbdata->be, tzid);
	if (zone == NULL)
		return;

	vtzcomp = icalcomponent_new_clone (icaltimezone_get_component (zone));
	if (vtzcomp)
		icalcomponent_add_component (cbdata->vcal_comp, vtzcomp);
}

gboolean
check_for_send_options (icalcomponent *icalcomp, E2kProperties *props)
{
	icalproperty *icalprop;
	gboolean exists = FALSE;
	const gchar *x_name, *x_val;

	icalprop = icalcomponent_get_first_property (icalcomp, ICAL_X_PROPERTY);
	while (icalprop && !exists) {
		x_name = icalproperty_get_x_name (icalprop);
		if (!strcmp(x_name, "X-EVOLUTION-OPTIONS-TRACKINFO")) {
			exists = TRUE;
			x_val = icalproperty_get_x (icalprop);
			switch (atoi (x_val)) {
				case EX_ALL: /* Track if delivered and opened */
				case EX_READ_AND_DELIVERED: /* Track if delivered and opened */
					e2k_properties_set_int (props,
					PR_READ_RECEIPT_REQUESTED, 1);
					/* Fall Through */
				case EX_DELIVERED_RECEIPTS : /* Track if delivered */
					e2k_properties_set_int (props,
					PR_ORIGINATOR_DELIVERY_REPORT_REQUESTED, 1);
					break;
				default : /* None */
					exists = FALSE;
					break;
			}
		}
		icalprop = icalcomponent_get_next_property (icalcomp, ICAL_X_PROPERTY);
	}

	return exists;
}

/* stolen from e-itip-control.c with some modifications */
static icalproperty *
find_attendee_prop (icalcomponent *ical_comp, const gchar *address)
{
	icalproperty *prop;

	if (address == NULL)
		return NULL;

	for (prop = icalcomponent_get_first_property (ical_comp, ICAL_ATTENDEE_PROPERTY);
	     prop != NULL;
	     prop = icalcomponent_get_next_property (ical_comp, ICAL_ATTENDEE_PROPERTY)) {
		const gchar *attendee;
		gchar *text = NULL;

		attendee = icalproperty_get_value_as_string_r (prop);
		if (!attendee)
			continue;

		if (!g_ascii_strncasecmp (attendee, "mailto:", 7))
			text = g_strdup (attendee + 7);
		text = g_strstrip (text);

		if (!g_ascii_strcasecmp (address, text)) {
			g_free (text);
			break;
		}
		g_free (text);
	}
	return prop;
}

static gboolean
check_owner_partstatus_for_declined (ECalBackendSync *backend,
				     icalcomponent *icalcomp)
{
	icalproperty *icalprop;
	icalparameter *param;
	gchar *email;

	email = e_cal_backend_exchange_get_owner_email (backend);
	icalprop = find_attendee_prop (icalcomp, email);

	g_free (email);
	if (!icalprop)
		return FALSE;

	param = icalproperty_get_first_parameter (icalprop, ICAL_PARTSTAT_PARAMETER);

	if (icalparameter_get_partstat (param) == ICAL_PARTSTAT_DECLINED) {
		return TRUE;
	}
	return FALSE;
}

static void
create_object (ECalBackendSync *backend, EDataCal *cal,
	       gchar **calobj, gchar **uid, GError **error)
{
	/* FIXME : Return some value in uid */
	ECalBackendExchangeCalendar *cbexc;
	ECalBackendExchange *cbex;
	icalcomponent *icalcomp, *real_icalcomp;
	icalcomponent_kind kind;
	icalproperty *icalprop;
	const gchar *temp_comp_uid;
	gchar *lastmod;
	struct icaltimetype current;
	gchar *location = NULL, *ru_header = NULL;
	ECalComponent *comp;
	gchar *body_crlf, *msg;
	gchar *from, *date;
	const gchar *summary;
	gchar *attach_body = NULL;
	gchar *attach_body_crlf = NULL;
	gchar *boundary = NULL;
	E2kHTTPStatus http_status;
	E2kProperties *props = e2k_properties_new ();
	E2kContext *e2kctx;
	struct _cb_data *cbdata;
	gboolean send_options;
	ECalComponentClassification classif;

	d(printf ("ecbexc_create_object(%p, %p, %s, %s)", backend, cal, *calobj ? *calobj : NULL, *uid ? *uid : NULL));

	cbexc =	E_CAL_BACKEND_EXCHANGE_CALENDAR (backend);
	cbex = E_CAL_BACKEND_EXCHANGE (backend);

	e_return_data_cal_error_if_fail (E_IS_CAL_BACKEND_EXCHANGE_CALENDAR (cbexc), InvalidArg);
	e_return_data_cal_error_if_fail (calobj != NULL, InvalidArg);

	if (!e_cal_backend_exchange_is_online (E_CAL_BACKEND_EXCHANGE (backend))) {
		g_propagate_error (error, EDC_ERROR (RepositoryOffline));
		return;
	}

	/* check for permission denied:: priv->writable??
	   ....
	 */

	icalcomp = icalparser_parse_string (*calobj);
	if (!icalcomp) {
		g_propagate_error (error, EDC_ERROR (InvalidObject));
		return;
	}

	kind = icalcomponent_isa (icalcomp);

	if (kind != e_cal_backend_get_kind (E_CAL_BACKEND (backend))) {
		icalcomponent_free (icalcomp);
		g_propagate_error (error, EDC_ERROR (InvalidObject));
		return;
	}

	temp_comp_uid = icalcomponent_get_uid (icalcomp);
	if (!temp_comp_uid) {
		icalcomponent_free (icalcomp);
		g_propagate_error (error, EDC_ERROR (InvalidObject));
		return;
	}
	#if 0
	if (lookup_component (E_CAL_BACKEND_EXCHANGE (cbexc), comp_uid))
	{
		icalcomponent_free (icalcomp);
		g_propagate_error (error, EDC_ERROR (OtherError));
		return;
	}
	#endif

	/* Delegated calendar */
	if (g_ascii_strcasecmp(e_cal_backend_exchange_get_owner_email (backend), exchange_account_get_email_id (cbex->account)))
		process_delegated_cal_object (icalcomp, e_cal_backend_exchange_get_owner_name (backend), e_cal_backend_exchange_get_owner_email (backend), exchange_account_get_email_id (cbex->account));

	/* Send options */
	send_options = check_for_send_options (icalcomp, props);

	/*set created and last_modified*/
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

	/* Fetch summary */
	summary = icalcomponent_get_summary (icalcomp);
	if (!summary)
		summary = "";

	lastmod = e2k_timestamp_from_icaltime (current);

	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (comp, icalcomp);

	/* Check for attachments */
	if (e_cal_component_has_attachments (comp)) {
		d(printf ("This comp has attachments !!\n"));
		attach_body = build_msg (E_CAL_BACKEND_EXCHANGE (cbexc), comp, summary, &boundary);
		attach_body_crlf = e_cal_backend_exchange_lf_to_crlf (attach_body);
	}

	update_x_properties (E_CAL_BACKEND_EXCHANGE (cbexc), comp);

	cbdata = g_new0 (struct _cb_data, 1);
	cbdata->be = backend;
	cbdata->vcal_comp = e_cal_util_new_top_level ();
	cbdata->cal = cal;

	/* Though OWA produces "CLASS:" (which we map to
	 * "CLASS:PUBLIC" above), it will accept "CLASS:PUBLIC".
	 * However, some other exchange clients, notably Windows
	 * Mobile Outlook, don't work unless we map "CLASS:PUBLIC"
	 * back to "CLASS:". For details, see
	 * https://bugzilla.gnome.org/show_bug.cgi?id=403903#c23
	 */
	e_cal_component_get_classification (comp, &classif);
	if (classif == E_CAL_COMPONENT_CLASS_PUBLIC)
		e_cal_component_set_classification (comp, E_CAL_COMPONENT_CLASS_NONE);

	/* Remove X parameters from properties */
	/* This is specifically for X-EVOLUTION-END-DATE,
	   but removing anything else is probably ok too */
	for (icalprop = icalcomponent_get_first_property (icalcomp, ICAL_ANY_PROPERTY);
	     icalprop != NULL;
	     icalprop = icalcomponent_get_next_property (icalcomp, ICAL_ANY_PROPERTY))
	{
		icalproperty_remove_parameter (icalprop, ICAL_X_PARAMETER);
	}

	/* add the timezones information and the component itself
	   to the VCALENDAR object */
	e_cal_component_commit_sequence (comp);
	*calobj = e_cal_component_get_as_string (comp);
	if (!*calobj) {
		g_object_unref (comp);
		icalcomponent_free (cbdata->vcal_comp);
		g_free (cbdata);
		g_propagate_error (error, EDC_ERROR_EX (OtherError, "Cannot get comp as string"));
		return;
	}
	real_icalcomp = icalparser_parse_string (*calobj);

	icalcomponent_foreach_tzid (real_icalcomp, add_timezone_cb, cbdata);
	icalcomponent_add_component (cbdata->vcal_comp, real_icalcomp);

	body_crlf = icalcomponent_as_ical_string_r (cbdata->vcal_comp);

	date = e_cal_backend_exchange_make_timestamp_rfc822 (time (NULL));
	if (!g_ascii_strcasecmp(e_cal_backend_exchange_get_owner_email (backend), exchange_account_get_email_id (cbex->account)))
		from = e_cal_backend_exchange_get_from_string (backend, comp);
	else
		from = e_cal_backend_exchange_get_sender_string (backend, comp);

	if (attach_body) {
		msg = g_strdup_printf ("Subject: %s\r\n"
				       "Date: %s\r\n"
				       "MIME-Version: 1.0\r\n"
				       "Content-Type: multipart/mixed;\r\n"
				       "\tboundary=\"%s\";\r\n"
				       "X-MS_Has-Attach: yes\r\n"
				       "From: %s\r\n"
					"\r\n--%s\r\n"
				       "content-class: urn:content-classes:appointment\r\n"
				       "Content-Type: text/calendar;\r\n"
				       "\tmethod=REQUEST;\r\n"
				       "\tcharset=\"utf-8\"\r\n"
				       "Content-Transfer-Encoding: 8bit\r\n"
				       "Importance: normal\r\n"
				       "Priority: normal\r\n"
				       "\r\n%s\r\n%s", summary, date, boundary,
				       from ? from : "Evolution", boundary,
				       body_crlf, attach_body_crlf);
		g_free (boundary);

	} else {
		msg = g_strdup_printf ("Subject: %s\r\n"
				       "Date: %s\r\n"
				       "MIME-Version: 1.0\r\n"
				       "Content-Type: text/calendar;\r\n"
				       "\tmethod=REQUEST;\r\n"
				       "\tcharset=\"utf-8\"\r\n"
				       "Content-Transfer-Encoding: 8bit\r\n"
				       "content-class: urn:content-classes:appointment\r\n"
				       "Importance: normal\r\n"
				       "Priority: normal\r\n"
				       "From: %s\r\n"
				       "\r\n%s", summary, date,
				       from ? from : "Evolution",
				       body_crlf);
	}

	http_status = e_folder_exchange_put_new (E_CAL_BACKEND_EXCHANGE (cbexc)->folder, NULL, summary,
						NULL, NULL, "message/rfc822",
						msg, strlen(msg), &location, &ru_header);

	if ((http_status == E2K_HTTP_CREATED) && send_options) {
		e2kctx = exchange_account_get_context (E_CAL_BACKEND_EXCHANGE (cbexc)->account);
		http_status = e2k_context_proppatch (e2kctx, NULL, location, props, FALSE, NULL);
	}

	g_free (date);
	g_free (from);
	g_free (body_crlf);
	g_free (msg);
	icalcomponent_free (cbdata->vcal_comp); // not sure
	g_free (cbdata);

	if (!E2K_HTTP_STATUS_IS_SUCCESSFUL (http_status)) {
		g_object_unref (comp);
		g_free (location);
		g_free (lastmod);
		g_propagate_error (error, EDC_ERROR_HTTP_STATUS (http_status));
		return;
	}

	e_cal_backend_exchange_cache_lock (cbex);
	/*add object*/
	e_cal_backend_exchange_add_object (E_CAL_BACKEND_EXCHANGE (cbexc), location, lastmod, icalcomp);
	e_cal_backend_exchange_cache_unlock (cbex);
	*uid = g_strdup (temp_comp_uid);

	g_object_unref (comp);
	g_free (lastmod);
	g_free (location);
	e2k_properties_free (props);
}

#define BUSYSTATUS	0x01
#define INSTTYPE	0x02
#define ALLDAY		0x04
#define IMPORTANCE	0x08

static void
update_x_properties (ECalBackendExchange *cbex, ECalComponent *comp)
{
	icalcomponent *icalcomp;
	icalproperty *icalprop;
	const gchar *x_name, *x_val;
	ECalComponentTransparency transp;
	ECalComponentDateTime dtstart;
	gint *priority;
	const gchar *busystatus, *insttype, *allday, *importance;
	gint prop_set = 0;
	GSList *props = NULL, *l = NULL;

	e_cal_component_get_transparency (comp, &transp);
	if (transp == E_CAL_COMPONENT_TRANSP_TRANSPARENT)
		busystatus = "FREE";
	else
		busystatus = "BUSY";

	if (e_cal_component_has_recurrences (comp))
		insttype = "1";
	else
		insttype = "0";

	e_cal_component_get_dtstart (comp, &dtstart);
	if (dtstart.value->is_date)
		allday = "TRUE";
	else
		allday = "FALSE";
	e_cal_component_free_datetime (&dtstart);

	e_cal_component_get_priority (comp, &priority);
	if (priority) {
		importance = *priority < 5 ? "2" : *priority > 5 ? "0" : "1";
		e_cal_component_free_priority (priority);
	} else
		importance = "1";

	/* Go through the existing X-MICROSOFT-CDO- properties first */
	icalcomp = e_cal_component_get_icalcomponent (comp);
	icalprop = icalcomponent_get_first_property (icalcomp, ICAL_X_PROPERTY);
	while (icalprop) {
		x_name = icalproperty_get_x_name (icalprop);
		x_val = icalproperty_get_x (icalprop);

		if (!strcmp (x_name, "X-MICROSOFT-CDO-BUSYSTATUS")) {
			/* If TRANSP was TRANSPARENT, BUSYSTATUS must
			 * be FREE. But if it was OPAQUE, it can
			 * be BUSY, TENTATIVE, or OOF, so only change
			 * it if it was FREE.
			 */
			if (busystatus && strcmp (busystatus, "FREE") == 0)
				icalproperty_set_x (icalprop, "FREE");
			else if (strcmp (x_val, "FREE") == 0)
				icalproperty_set_x (icalprop, "BUSY");
			prop_set |= BUSYSTATUS;
		} else if (!strcmp (x_name, "X-MICROSOFT-CDO-INSTTYPE")) {
			icalproperty_set_x (icalprop, insttype);
			prop_set |= INSTTYPE;
		} else if (!strcmp (x_name, "X-MICROSOFT-CDO-ALLDAYEVENT")) {
			icalproperty_set_x (icalprop, allday);
			prop_set |= ALLDAY;
		} else if (!strcmp (x_name, "X-MICROSOFT-CDO-IMPORTANCE")) {
			icalproperty_set_x (icalprop, importance);
			prop_set |= IMPORTANCE;
		} else if (!strcmp (x_name, "X-MICROSOFT-CDO-MODPROPS")) {
			props = g_slist_append (props, icalprop);
		}

		icalprop = icalcomponent_get_next_property (icalcomp, ICAL_X_PROPERTY);
	}

	/* Remove all CDO-MODPROPS properties, else server would return error for the operation
	   performed */
	for (l = props; l != NULL; l = l->next) {
		icalprop = l->data;
		icalcomponent_remove_property (icalcomp, icalprop);
		icalproperty_free (icalprop);
	}

	g_slist_free (props);

	/* Now set the ones that weren't set. */
	if (!(prop_set & BUSYSTATUS)) {
		icalprop = icalproperty_new_x (busystatus);
		icalproperty_set_x_name (icalprop, "X-MICROSOFT-CDO-BUSYSTATUS");
		icalcomponent_add_property (icalcomp, icalprop);
	}

	if (!(prop_set & INSTTYPE)) {
		icalprop = icalproperty_new_x (insttype);
		icalproperty_set_x_name (icalprop, "X-MICROSOFT-CDO-INSTTYPE");
		icalcomponent_add_property (icalcomp, icalprop);
	}

	if (!(prop_set & ALLDAY)) {
		icalprop = icalproperty_new_x (allday);
		icalproperty_set_x_name (icalprop, "X-MICROSOFT-CDO-ALLDAYEVENT");
		icalcomponent_add_property (icalcomp, icalprop);
	}

	if (!(prop_set & IMPORTANCE)) {
		icalprop = icalproperty_new_x (importance);
		icalproperty_set_x_name (icalprop, "X-MICROSOFT-CDO-IMPORTANCE");
		icalcomponent_add_property (icalcomp, icalprop);
	}
}

static void
modify_object (ECalBackendSync *backend, EDataCal *cal,
	       const gchar *calobj, CalObjModType mod,
	       gchar **old_object, gchar **new_object, GError **perror)
{
	d(printf ("ecbexc_modify_object(%p, %p, %d, %s)", backend, cal, mod, *old_object ? *old_object : NULL));

	modify_object_with_href (backend, cal, calobj, mod, old_object, new_object, NULL, NULL, perror);
}

#define e_return_data_cal_error_and_val_if_fail(expr, _code, _val)		\
	G_STMT_START {								\
		if (G_LIKELY(expr)) {						\
		} else {							\
			g_log (G_LOG_DOMAIN,					\
				G_LOG_LEVEL_CRITICAL,				\
				"file %s: line %d (%s): assertion `%s' failed",	\
				__FILE__, __LINE__, G_STRFUNC, #expr);		\
			g_set_error (error, E_DATA_CAL_ERROR, (_code),		\
				"file %s: line %d (%s): assertion `%s' failed",	\
				__FILE__, __LINE__, G_STRFUNC, #expr);		\
			return _val;						\
		}								\
	} G_STMT_END

static gboolean
modify_object_with_href (ECalBackendSync *backend, EDataCal *cal,
	       const gchar *calobj, CalObjModType mod,
	       gchar **old_object, gchar **new_object, const gchar *href, const gchar *rid_to_remove, GError **error)
{
	ECalBackendExchangeCalendar *cbexc;
	ECalBackendExchangeComponent *ecomp;
	ECalBackendExchange *cbex;
	icalcomponent *icalcomp, *real_icalcomp, *updated_icalcomp;
	ECalComponent *real_ecomp, *cached_ecomp = NULL, *updated_ecomp;
	const gchar *comp_uid;
	gchar *updated_ecomp_str, *real_comp_str;
	gchar *body_crlf, *msg;
	gchar *attach_body = NULL;
	gchar *attach_body_crlf = NULL;
	gchar *boundary = NULL;
	struct icaltimetype last_modified, key_rid;
	icalcomponent_kind kind;
	ECalComponentDateTime dt;
	struct _cb_data *cbdata;
	icalproperty *icalprop;
	E2kHTTPStatus http_status;
	gchar *from, *date;
	const gchar *summary, *new_href;
	gboolean send_options;
	E2kContext *ctx;
	E2kProperties *props = e2k_properties_new ();
	GList *l = NULL;
	struct icaltimetype inst_rid;
	gboolean remove = FALSE;
	GList *to_remove = NULL;

	cbexc =	E_CAL_BACKEND_EXCHANGE_CALENDAR (backend);
	cbex = E_CAL_BACKEND_EXCHANGE (backend);

	e_return_data_cal_error_and_val_if_fail (E_IS_CAL_BACKEND_EXCHANGE_CALENDAR (cbexc), InvalidArg, FALSE);
	e_return_data_cal_error_and_val_if_fail (calobj != NULL, InvalidArg, FALSE);

	if (!e_cal_backend_exchange_is_online (E_CAL_BACKEND_EXCHANGE (backend))) {
                g_propagate_error (error, EDC_ERROR (RepositoryOffline));
		return FALSE;
        }

	if (rid_to_remove)
		remove = TRUE;

	icalcomp = icalparser_parse_string (calobj);
	if (!icalcomp) {
		g_propagate_error (error, EDC_ERROR (InvalidObject));
		return FALSE;
	}

	kind = icalcomponent_isa (icalcomp);

	if (kind != e_cal_backend_get_kind (E_CAL_BACKEND (backend))) {
		icalcomponent_free (icalcomp);
		g_propagate_error (error, EDC_ERROR (InvalidObject));
		return FALSE;
	}
	comp_uid = icalcomponent_get_uid (icalcomp);

	if (!remove)
		key_rid = icalcomponent_get_recurrenceid (icalcomp);
	else
		key_rid = icaltime_from_string (rid_to_remove);

	e_cal_backend_exchange_cache_lock (cbex);
	ecomp = get_exchange_comp (cbex, comp_uid);

	if (!ecomp) {
		icalcomponent_free (icalcomp);
		e_cal_backend_exchange_cache_unlock (cbex);
		g_propagate_error (error, EDC_ERROR (ObjectNotFound));
		return FALSE;
	}

	/* Fetch summary */
	summary = icalcomponent_get_summary (icalcomp);
	if (!summary)
		summary = "";

	/* If rid is present and mod is ALL we need to set the times of the master object */
	if ((mod == CALOBJ_MOD_ALL) && e_cal_util_component_has_recurrences (icalcomp) && !icaltime_is_null_time (key_rid)) {
		icaltimetype start;

		start = icalcomponent_get_dtstart (icalcomp);

		if (!key_rid.zone)
			key_rid.zone = start.zone;

		/* This means its a instance generated from master object. So replace
		   the dates stored dates from the master object */

		if (icaltime_compare_date_only (start, key_rid) == 0) {
			icaltimetype m_end, m_start, end;
			icalproperty *prop;

			m_start = icalcomponent_get_dtstart (ecomp->icomp);
			m_end = icalcomponent_get_dtend (ecomp->icomp);
			end = icalcomponent_get_dtend (icalcomp);

			if (icaltime_compare (start, key_rid) != 0) {
					m_start.hour = start.hour;
					m_start.minute = start.minute;
					m_start.second = start.second;

			}

			if (!((m_end.hour == end.hour) && (m_end.minute == end.minute) && (m_end.second == end.second))) {
					m_end.hour = end.hour;
					m_end.minute = end.minute;
					m_end.second = end.second;
			}

			icalcomponent_set_dtstart (icalcomp, m_start);
			icalcomponent_set_dtend (icalcomp, m_end);

			prop = icalcomponent_get_first_property (icalcomp, ICAL_RECURRENCEID_PROPERTY);
			icalcomponent_remove_property (icalcomp, prop);
			icalproperty_free (prop);
		}
	}

	updated_ecomp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (updated_ecomp, icalcomp);

	update_x_properties (E_CAL_BACKEND_EXCHANGE (cbexc), updated_ecomp);

	last_modified = icaltime_current_time_with_zone (icaltimezone_get_utc_timezone ());
	e_cal_component_set_last_modified (updated_ecomp, &last_modified);

	if (e_cal_component_has_attachments (updated_ecomp)) {
		d(printf ("This comp has attachments !!\n"));
		attach_body = build_msg (E_CAL_BACKEND_EXCHANGE (cbexc), updated_ecomp, summary, &boundary);
		attach_body_crlf = e_cal_backend_exchange_lf_to_crlf (attach_body);
	}

	e_cal_component_commit_sequence (updated_ecomp);
	updated_ecomp_str = e_cal_component_get_as_string (updated_ecomp);
	updated_icalcomp = icalparser_parse_string (updated_ecomp_str);
	g_free (updated_ecomp_str);
	if (!updated_icalcomp) {
		g_object_unref (updated_ecomp);
		e_cal_backend_exchange_cache_unlock (cbex);
		g_propagate_error (error, EDC_ERROR_EX (OtherError, "Faild to parse updated ecomp string"));
		return FALSE;
	}

	/* Delegated calendar */
	if (g_ascii_strcasecmp(e_cal_backend_exchange_get_owner_email (backend), exchange_account_get_email_id (cbex->account)))
		process_delegated_cal_object (updated_icalcomp, e_cal_backend_exchange_get_owner_name (backend), e_cal_backend_exchange_get_owner_email (backend), exchange_account_get_email_id (cbex->account));

	/* send options */
	send_options = check_for_send_options (updated_icalcomp, props);

	/* Remove X parameters from properties */
	/* This is specifically for X-EVOLUTION-END-DATE,
	   but removing anything else is probably ok too */
	for (icalprop = icalcomponent_get_first_property (updated_icalcomp, ICAL_ANY_PROPERTY);
	     icalprop != NULL;
	     icalprop = icalcomponent_get_next_property (updated_icalcomp, ICAL_ANY_PROPERTY))
	{
		icalproperty_remove_parameter (icalprop, ICAL_X_PARAMETER);
	}

	real_ecomp = e_cal_component_new ();
	if (!e_cal_component_set_icalcomponent (real_ecomp, updated_icalcomp)) {
		g_object_unref (real_ecomp);
		g_object_unref (updated_ecomp);
		e_cal_backend_exchange_cache_unlock (cbex);
		g_propagate_error (error, EDC_ERROR_EX (OtherError, "Failed to set icalcomp to ECalComp"));
		return FALSE;
	}

	cbdata = g_new0 (struct _cb_data, 1);
	cbdata->be = backend;
	cbdata->vcal_comp = e_cal_util_new_top_level ();
	cbdata->cal = cal;

	e_cal_component_get_dtstart (real_ecomp, &dt);
	if (dt.value->is_date) {
		icaltimezone *zone;

		zone = e_cal_backend_exchange_get_default_time_zone (backend);
		if (!zone)
			zone = icaltimezone_get_utc_timezone ();

		dt.value->is_date = FALSE;
		dt.value->is_utc = FALSE;
		dt.value->hour = dt.value->minute = dt.value->second = 0;
		dt.value->zone = zone;

		g_free ((gchar *)dt.tzid);
		dt.tzid = g_strdup (icaltimezone_get_tzid (zone));
		e_cal_component_set_dtstart (real_ecomp, &dt);
		e_cal_component_free_datetime (&dt);

		e_cal_component_get_dtend (real_ecomp, &dt);
		dt.value->is_date = FALSE;
		dt.value->is_utc = FALSE;
		dt.value->hour = dt.value->minute = dt.value->second = 0;
		dt.value->zone = zone;

		g_free ((gchar *)dt.tzid);
		dt.tzid = g_strdup (icaltimezone_get_tzid (zone));
		e_cal_component_set_dtend (real_ecomp, &dt);
	}
	e_cal_component_free_datetime (&dt);

	/* Fix UNTIL date in a simple recurrence */
	if (e_cal_component_has_recurrences (real_ecomp)
	    && e_cal_component_has_simple_recurrence (real_ecomp)) {
		GSList *rrule_list;
		struct icalrecurrencetype *r;

		e_cal_component_get_rrule_list (real_ecomp, &rrule_list);
		r = rrule_list->data;

		if (!icaltime_is_null_time (r->until) && r->until.is_date) {
			icaltimezone *from_zone, *to_zone;

			e_cal_component_get_dtstart (real_ecomp, &dt);

			if (dt.tzid == NULL)
				from_zone = icaltimezone_get_utc_timezone ();
			else {
				from_zone = e_cal_backend_internal_get_timezone ((ECalBackend *) backend, dt.tzid);
			}

			to_zone = icaltimezone_get_utc_timezone ();

			r->until.hour = dt.value->hour;
			r->until.minute = dt.value->minute;
			r->until.second = dt.value->second;
			r->until.is_date = FALSE;

			icaltimezone_convert_time (&r->until, from_zone, to_zone);
			r->until.is_utc = TRUE;

			e_cal_component_set_rrule_list (real_ecomp, rrule_list);
			e_cal_component_free_datetime (&dt);
		}

		e_cal_component_free_recur_list (rrule_list);
	}

	if (mod == CALOBJ_MOD_ALL && ecomp->icomp) {
		cached_ecomp = e_cal_component_new ();
		e_cal_component_set_icalcomponent (cached_ecomp, icalcomponent_new_clone (ecomp->icomp));
		if (e_cal_component_has_recurrences (real_ecomp))
			e_cal_component_set_recurid (real_ecomp, NULL);
	}

	/* add the timezones information and the component itself
	   to the VCALENDAR object */
	e_cal_component_commit_sequence (real_ecomp);
	real_comp_str = e_cal_component_get_as_string (real_ecomp);
	if (!real_comp_str) {
		g_object_unref (real_ecomp);
		g_object_unref (updated_ecomp);
		icalcomponent_free (cbdata->vcal_comp);
		g_free (cbdata);
		e_cal_backend_exchange_cache_unlock (cbex);
		g_propagate_error (error, EDC_ERROR_EX (OtherError, "Failed to get real ECalComp as string"));
		return FALSE;
	}
	real_icalcomp = icalparser_parse_string (real_comp_str);
	g_free (real_comp_str);

	icalcomponent_foreach_tzid (real_icalcomp, add_timezone_cb, cbdata);

	/* Master object should be added first */
	if (!remove && mod == CALOBJ_MOD_ALL)
		icalcomponent_add_component (cbdata->vcal_comp, real_icalcomp);

	/* We need to add all the instances to the VCalendar component while sending to
	   the server, so that we don't lose any detached instances */
	if (ecomp->icomp && mod == CALOBJ_MOD_THIS && remove) {
		icalcomponent_add_component (cbdata->vcal_comp, icalcomponent_new_clone (ecomp->icomp));
	} else if (!remove && mod == CALOBJ_MOD_THIS)
		icalcomponent_add_component (cbdata->vcal_comp, real_icalcomp);

	for (l = ecomp->instances; l != NULL; l = l->next) {
		icalcomponent *icomp = l->data;

		inst_rid = icalcomponent_get_recurrenceid (icomp);
		if (icaltime_compare (inst_rid, key_rid) == 0) {
			cached_ecomp = e_cal_component_new ();

			e_cal_component_set_icalcomponent (cached_ecomp,
					icalcomponent_new_clone (icomp));
			if (remove)
				to_remove = l;
			continue;
		} else {
			ECalComponent *comp = e_cal_component_new ();
			e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (icomp));

			update_x_properties (E_CAL_BACKEND_EXCHANGE (cbexc), comp);

			icalcomponent_add_component (cbdata->vcal_comp,
					icalcomponent_new_clone (e_cal_component_get_icalcomponent (comp)));

			g_object_unref (comp);
		}
	}

	if (to_remove) {
		icalcomponent_free (to_remove->data);
		ecomp->instances = g_list_remove_link (ecomp->instances, to_remove);
	}

	e_cal_backend_exchange_cache_unlock (cbex);

	if (!cached_ecomp && remove)
		*new_object = icalcomponent_as_ical_string_r (icalcomp);

	body_crlf = icalcomponent_as_ical_string_r (cbdata->vcal_comp);

	date = e_cal_backend_exchange_make_timestamp_rfc822 (time (NULL));
	if (!g_ascii_strcasecmp(e_cal_backend_exchange_get_owner_email (backend), exchange_account_get_email_id (cbex->account)))
		from = e_cal_backend_exchange_get_from_string (backend, updated_ecomp);
	else
		from = e_cal_backend_exchange_get_sender_string (backend, updated_ecomp);

	if (attach_body) {
		msg = g_strdup_printf ("Subject: %s\r\n"
				       "Date: %s\r\n"
				       "MIME-Version: 1.0\r\n"
				       "Content-Type: multipart/mixed;\r\n"
				       "\tboundary=\"%s\";\r\n"
				       "X-MS_Has-Attach: yes\r\n"
				       "From: %s\r\n"
					"\r\n--%s\r\n"
				       "content-class: urn:content-classes:appointment\r\n"
				       "Content-Type: text/calendar;\r\n"
				       "\tmethod=REQUEST;\r\n"
				       "\tcharset=\"utf-8\"\r\n"
				       "Content-Transfer-Encoding: 8bit\r\n"
				       "Importance: normal\r\n"
				       "Priority: normal\r\n"
				       "\r\n%s\r\n%s", summary, date, boundary,
				       from ? from : "Evolution", boundary,
				       body_crlf, attach_body_crlf);
		g_free (boundary);

	} else {

		msg = g_strdup_printf ("Subject: %s\r\n"
				       "Date: %s\r\n"
				       "MIME-Version: 1.0\r\n"
				       "Content-Type: text/calendar;\r\n"
				       "\tmethod=REQUEST;\r\n"
				       "\tcharset=\"utf-8\"\r\n"
				       "Content-Transfer-Encoding: 8bit\r\n"
				       "content-class: urn:content-classes:appointment\r\n"
				       "Importance: normal\r\n"
				       "Priority: normal\r\n"
				       "From: %s\r\n"
				       "\r\n%s", summary, date,
				       from ? from : "Evolution",
				       body_crlf);
	}

	g_free (date);
	g_free (from);
	g_free (body_crlf);

	if (cached_ecomp) {
		e_cal_component_commit_sequence (cached_ecomp);
		*old_object = e_cal_component_get_as_string (cached_ecomp);
	}

	ctx = exchange_account_get_context (E_CAL_BACKEND_EXCHANGE (cbexc)->account);

	/* PUT the iCal object in the Exchange server */
	if (href)
		new_href = href;
	else
		new_href = ecomp->href;

	http_status = e2k_context_put (ctx, NULL, new_href, "message/rfc822",
						msg, strlen (msg), NULL);

	if ((E2K_HTTP_STATUS_IS_SUCCESSFUL (http_status)) && send_options)
		http_status = e2k_context_proppatch (ctx, NULL, new_href, props, FALSE, NULL);

	if (E2K_HTTP_STATUS_IS_SUCCESSFUL (http_status)) {
		e_cal_backend_exchange_cache_lock (cbex);
		e_cal_backend_exchange_modify_object (E_CAL_BACKEND_EXCHANGE (cbexc),
							e_cal_component_get_icalcomponent (real_ecomp), mod, remove);
		e_cal_backend_exchange_cache_unlock (cbex);

		if (!remove)
			*new_object = e_cal_component_get_as_string (real_ecomp);
	} else {
		g_propagate_error (error, EDC_ERROR_HTTP_STATUS (http_status));
	}

	g_free (msg);
	g_object_unref (real_ecomp);
	g_object_unref (updated_ecomp);

	if (cached_ecomp)
		g_object_unref (cached_ecomp);

	icalcomponent_free (cbdata->vcal_comp);
	g_free (cbdata);
	e2k_properties_free (props);

	return !error || !*error;
}

static void
remove_object (ECalBackendSync *backend, EDataCal *cal,
	       const gchar *uid, const gchar *rid, CalObjModType mod,
	       gchar **old_object, gchar **object, GError **error)
{
	ECalBackendExchangeCalendar *cbexc;
	ECalBackendExchange *cbex;
	ECalBackendExchangeComponent *ecomp;
	E2kHTTPStatus status;
	E2kContext *ctx;
	ECalComponent *comp;
	gchar *calobj, *obj = NULL;
	struct icaltimetype time_rid;

	/* Will handle only CALOBJ_MOD_THIS and CALOBJ_MOD_ALL for mod.
	   CALOBJ_MOD_THISANDPRIOR and CALOBJ_MOD_THISANDFUTURE are not
	   handled. */

	cbexc = E_CAL_BACKEND_EXCHANGE_CALENDAR (backend);
	cbex = E_CAL_BACKEND_EXCHANGE (backend);

	e_return_data_cal_error_if_fail (E_IS_CAL_BACKEND_EXCHANGE_CALENDAR (cbexc), InvalidArg);

	if (!e_cal_backend_exchange_is_online (E_CAL_BACKEND_EXCHANGE (backend))) {
		g_propagate_error (error, EDC_ERROR (RepositoryOffline));
                return;
        }

	e_cal_backend_exchange_cache_lock (cbex);
	ecomp = get_exchange_comp (E_CAL_BACKEND_EXCHANGE (cbexc), uid);

	if (!ecomp) {
		e_cal_backend_exchange_cache_unlock (cbex);
		g_propagate_error (error, EDC_ERROR (ObjectNotFound));
		return;
	}

	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (ecomp->icomp));
	if (old_object) {
		e_cal_component_commit_sequence (comp);
		*old_object = e_cal_component_get_as_string (comp);
	}

	/*TODO How handle multiple detached intances with no master object ?*/
	if (mod == CALOBJ_MOD_THIS && rid && *rid && ecomp->icomp) {
		gchar *new_object = NULL;
		gboolean res;

		/*remove a single instance of a recurring event and modify */
		time_rid = icaltime_from_string (rid);
		e_cal_util_remove_instances (ecomp->icomp, time_rid, mod);
		calobj  = (gchar *) icalcomponent_as_ical_string_r (ecomp->icomp);

		e_cal_backend_exchange_cache_unlock (cbex);
		res = modify_object_with_href (backend, cal, calobj, mod, &obj, &new_object, NULL, rid, error);
		g_object_unref (comp);
		g_free (calobj);
		if (!res)
			return;
		if (obj) {
			g_free (*old_object);
			*old_object = obj;
		}

		g_free (new_object);

		return;
	} else
		e_cal_backend_exchange_cache_unlock (cbex);

	g_object_unref (comp);

	ctx = exchange_account_get_context (E_CAL_BACKEND_EXCHANGE (cbexc)->account);

	status = e2k_context_delete (ctx, NULL, ecomp->href);
	if (E2K_HTTP_STATUS_IS_SUCCESSFUL (status)) {
		e_cal_backend_exchange_cache_lock (cbex);
		if (e_cal_backend_exchange_remove_object (E_CAL_BACKEND_EXCHANGE (cbexc), uid)) {
			e_cal_backend_exchange_cache_unlock (cbex);
			return;
		}
		e_cal_backend_exchange_cache_unlock (cbex);
	}
	*object = NULL;

	g_propagate_error (error, EDC_ERROR_HTTP_STATUS (status));
}

static void
receive_objects (ECalBackendSync *backend, EDataCal *cal,
		 const gchar *calobj, GError **error)
{
	ECalBackendExchangeCalendar *cbexc;
	ECalBackendExchange *cbex;
	ECalBackendExchangeComponent *ecomp;
	ECalComponent *comp = NULL;
	GList *comps, *l;
	struct icaltimetype current;
	icalproperty_method method;
	icalcomponent *subcomp, *icalcomp;
	GError *err = NULL;

	d(printf ("ecbexc_modify_object(%p, %p, %s)", backend, cal, calobj ? calobj : NULL));
	cbexc =	E_CAL_BACKEND_EXCHANGE_CALENDAR (backend);
	cbex =	E_CAL_BACKEND_EXCHANGE (backend);

	e_return_data_cal_error_if_fail (E_IS_CAL_BACKEND_EXCHANGE_CALENDAR (cbexc), InvalidArg);
	e_return_data_cal_error_if_fail (calobj != NULL, InvalidArg);

	if (!e_cal_backend_exchange_is_online (E_CAL_BACKEND_EXCHANGE (backend))) {
		g_propagate_error (error, EDC_ERROR (RepositoryOffline));
                return;
        }

	if (!e_cal_backend_exchange_extract_components (calobj, &method, &comps, error))
		return;

	icalcomp = icalparser_parse_string (calobj);

	/* map time zones against system time zones and handle conflicting definitions */
	if (icalcomp &&
	    !e_cal_check_timezones (icalcomp,
				    comps,
				    e_cal_backend_exchange_lookup_timezone,
				    cbex,
				    &err)) {
		g_propagate_error (error, EDC_ERROR_EX (InvalidObject, err->message));
		g_warning ("checking timezones failed: %s", err->message);
		icalcomponent_free (icalcomp);
		g_clear_error (&err);
		return;
	}

	add_timezones_from_comp (E_CAL_BACKEND_EXCHANGE (backend), icalcomp);
	icalcomponent_free (icalcomp);

	for (l = comps; l; l= l->next) {
		const gchar *uid;
		gchar *icalobj, *rid = NULL;
		gchar *object = NULL;

		subcomp = l->data;

		comp = e_cal_component_new ();
		e_cal_component_set_icalcomponent (comp, subcomp);

		/*create time and last modified*/
		current = icaltime_current_time_with_zone (icaltimezone_get_utc_timezone ());
		e_cal_component_set_created (comp, &current);
		e_cal_component_set_last_modified (comp, &current);

		e_cal_component_get_uid (comp, &uid);
		rid = e_cal_component_get_recurid_as_string (comp);

		switch (method) {
		case ICAL_METHOD_PUBLISH:
		case ICAL_METHOD_REQUEST:
		case ICAL_METHOD_REPLY:
			e_cal_backend_exchange_cache_lock (cbex);
			if ((ecomp = get_exchange_comp (E_CAL_BACKEND_EXCHANGE (cbexc), uid)) != NULL ) {
				gchar *old_object = NULL;

				d(printf ("uid : %s : found in the cache\n", uid));

				e_cal_backend_exchange_cache_unlock (cbex);
				if (check_owner_partstatus_for_declined (backend, subcomp)) {
					ECalComponentId *id = NULL;
					remove_object (backend, cal, uid, NULL,
								CALOBJ_MOD_ALL, &old_object,
								NULL, &err);
					if (err) {
						g_free (rid);
						g_propagate_error (error, err);
						goto error;
					}
					id = e_cal_component_get_id (comp);
					e_cal_backend_notify_object_removed (E_CAL_BACKEND (backend), id,
									     old_object, NULL);
					e_cal_component_free_id (id);
				} else {
					gchar *new_object = NULL;
					CalObjModType mod = CALOBJ_MOD_ALL;
					GSList *attachment_list;

					attachment_list = receive_attachments (cbex, comp);
					if (attachment_list) {
						e_cal_component_set_attachment_list (comp, attachment_list);
						g_slist_foreach (attachment_list, (GFunc) g_free, NULL);
						g_slist_free (attachment_list);
					}

					if (e_cal_util_component_is_instance (subcomp))
						mod = CALOBJ_MOD_THIS;

					icalobj = e_cal_component_get_as_string (comp);
					if (!modify_object_with_href (backend, cal, icalobj,
									  mod,
									  &old_object, &new_object, NULL, NULL, error)) {
						g_free (rid);
						goto error;
					}
					e_cal_backend_notify_object_modified (E_CAL_BACKEND (backend),
									      old_object, new_object);
					g_free (new_object);
					d(printf ("Notify that the new object after modication is : %s\n", icalobj));
					g_free (icalobj);
				}

				g_free (old_object);
			} else if (!check_owner_partstatus_for_declined (backend, subcomp)) {
				gchar *returned_uid, *object;
				GSList *attachment_list;

				attachment_list = receive_attachments (cbex, comp);
				if (attachment_list) {
					e_cal_component_set_attachment_list (comp, attachment_list);
					g_slist_foreach (attachment_list, (GFunc) g_free, NULL);
					g_slist_free (attachment_list);
				}

				d(printf ("object : %s .. not found in the cache\n", uid));
				icalobj = e_cal_component_get_as_string (comp);
				d(printf ("Create a new object : %s\n", icalobj));

				e_cal_backend_exchange_cache_unlock (cbex);
				create_object (backend, cal, &icalobj, &returned_uid, &err);
				if (err) {
					g_propagate_error (error, err);
					g_free (rid);
					goto error;
				}

				object = icalobj;
				e_cal_backend_notify_object_created (E_CAL_BACKEND (backend), icalobj);
				d(printf ("Notify that the new object is created : %s\n", icalobj));
				g_free (object);
			} else {
				e_cal_backend_exchange_cache_unlock (cbex);
			}

			break;
		case ICAL_METHOD_ADD:
			/* FIXME This should be doable once all the recurid stuff is done ??*/
			break;

		case ICAL_METHOD_CANCEL:
			icalobj = (gchar *) icalcomponent_as_ical_string_r (subcomp);
			if (rid)
				remove_object (backend, cal, uid, rid, CALOBJ_MOD_THIS, &icalobj, &object, &err);
			else
				remove_object (backend, cal, uid, NULL, CALOBJ_MOD_ALL, &icalobj, &object, &err);
			if (!err) {
				ECalComponentId *id = e_cal_component_get_id (comp);
				e_cal_backend_notify_object_removed (E_CAL_BACKEND (backend), id, icalobj, NULL);
				e_cal_component_free_id (id);

			} else {
				g_propagate_error (error, err);
			}
			if (object) {
				g_free (object);
				object = NULL;
			}
			g_free (icalobj);
			break;
		default:
			g_propagate_error (error, EDC_ERROR (UnsupportedMethod));
			g_free (rid);
			goto error;
		}
		g_object_unref (comp);
		g_free (rid);
	}
	g_list_free (comps);
	return;

 error:
	if (comp)
		g_object_unref (comp);
}

typedef enum {
	E_CAL_BACKEND_EXCHANGE_BOOKING_OK,
	E_CAL_BACKEND_EXCHANGE_BOOKING_NO_SUCH_USER,
	E_CAL_BACKEND_EXCHANGE_BOOKING_BUSY,
	E_CAL_BACKEND_EXCHANGE_BOOKING_PERMISSION_DENIED,
	E_CAL_BACKEND_EXCHANGE_BOOKING_ERROR
} ECalBackendExchangeBookingResult;

/* start_time and end_time are in e2k_timestamp format. */
static ECalBackendExchangeBookingResult
book_resource (ECalBackendExchange *cbex,
	       EDataCal *cal,
	       const gchar *resource_email,
	       ECalComponent *comp,
	       icalproperty_method method,
	       icalparameter *part_param,
	       gboolean book_all)
{
	E2kGlobalCatalog *gc;
	E2kGlobalCatalogEntry *entry;
	E2kGlobalCatalogStatus gcstatus;
	ECalBackendExchangeComponent *ecomp;
	ECalComponentText old_text, new_text;
	E2kHTTPStatus status = 0;
	E2kResult *result;
	E2kResultIter *iter;
	ECalComponentDateTime dt;
	E2kContext *ctx;
	icaltimezone *izone;
	guint32 access = 0;
	time_t tt;
	const gchar *uid, *prop_name = PR_ACCESS;
	const gchar *access_prop = NULL, *meeting_prop = NULL, *cal_uid = NULL;
	gboolean bookable;
	gchar *top_uri = NULL, *cal_uri = NULL, *returned_uid = NULL, *sanitized_uid;
	gchar *startz, *endz, *href = NULL, *old_object = NULL, *calobj = NULL, *new_object = NULL;
	E2kRestriction *rn;
	gint nresult;
	ECalBackendExchangeBookingResult retval = E_CAL_BACKEND_EXCHANGE_BOOKING_ERROR;
	const gchar *localfreebusy_path = "NON_IPM_SUBTREE/Freebusy%20Data/LocalFreebusy.EML";

	g_object_ref (comp);

	/* Look up the resource's mailbox */
	gc = exchange_account_get_global_catalog (cbex->account);
	if (!gc)
		goto cleanup;

	gcstatus = e2k_global_catalog_lookup (
		gc, NULL, E2K_GLOBAL_CATALOG_LOOKUP_BY_EMAIL, resource_email,
		E2K_GLOBAL_CATALOG_LOOKUP_MAILBOX, &entry);

	switch (gcstatus) {
		case E2K_GLOBAL_CATALOG_OK:
			break;

		case E2K_GLOBAL_CATALOG_NO_SUCH_USER:
			retval = E_CAL_BACKEND_EXCHANGE_BOOKING_NO_SUCH_USER;
			goto cleanup;

		default:
			retval = E_CAL_BACKEND_EXCHANGE_BOOKING_ERROR;
			goto cleanup;
	}

	top_uri = exchange_account_get_foreign_uri (cbex->account,
						    entry, NULL);
	cal_uri = exchange_account_get_foreign_uri (cbex->account, entry,
						    E2K_PR_STD_FOLDER_CALENDAR);
	e2k_global_catalog_entry_free (gc, entry);
	if (!top_uri || !cal_uri || !*cal_uri) {
		retval = E_CAL_BACKEND_EXCHANGE_BOOKING_PERMISSION_DENIED;
		goto cleanup;
	}

	ctx = exchange_account_get_context (cbex->account);
	status = e2k_context_propfind (ctx, NULL, cal_uri,
					&prop_name, 1,
					&result, &nresult);
	if (E2K_HTTP_STATUS_IS_SUCCESSFUL (status) && nresult >= 1) {
		access_prop = e2k_properties_get_prop (result[0].props, PR_ACCESS);
		if (access_prop)
			access = atoi (access_prop);
	}
	e2k_results_free (result, nresult);

	if (!(access & MAPI_ACCESS_CREATE_CONTENTS)) {
		retval = E_CAL_BACKEND_EXCHANGE_BOOKING_PERMISSION_DENIED;
		goto cleanup;
	}

	prop_name = PR_PROCESS_MEETING_REQUESTS;
	iter = e2k_context_bpropfind_start (ctx, NULL, top_uri,
						&localfreebusy_path, 1,
						&prop_name, 1);
	result = e2k_result_iter_next (iter);
	if (result && E2K_HTTP_STATUS_IS_SUCCESSFUL (result->status))  {
		meeting_prop = e2k_properties_get_prop (result[0].props, PR_PROCESS_MEETING_REQUESTS);
	}
	if (meeting_prop)
		bookable = atoi (meeting_prop);
	else
		bookable = FALSE;
	status = e2k_result_iter_free (iter);

	if ((!E2K_HTTP_STATUS_IS_SUCCESSFUL (status)) || (!bookable)) {
		retval = E_CAL_BACKEND_EXCHANGE_BOOKING_PERMISSION_DENIED;
		goto cleanup;
	}

	e_cal_component_get_uid (E_CAL_COMPONENT (comp), &uid);
	sanitized_uid = g_strdup (uid);
	g_strdelimit (sanitized_uid, " /'\"`&();|<>$%{}!\\:*?#@", '_');
	href = g_strdup_printf ("%s%s%s.EML", cal_uri [strlen (cal_uri) - 1] == '/' ? "" : "/", cal_uri, sanitized_uid);
	g_free (sanitized_uid);

	e_cal_backend_exchange_cache_lock (cbex);
	ecomp = get_exchange_comp (E_CAL_BACKEND_EXCHANGE (cbex), uid);
	e_cal_backend_exchange_cache_unlock (cbex);

	if (method == ICAL_METHOD_CANCEL) {
		gchar *object = NULL;

		/* g_object_unref (comp); */
		/* If there is nothing to cancel, we're good */
		if (!ecomp) {
			retval = E_CAL_BACKEND_EXCHANGE_BOOKING_OK;
			goto cleanup;
		}

		/* Mark the cancellation properly in the resource's calendar */

		/* Mark the item as cancelled */
		e_cal_component_get_summary (E_CAL_COMPONENT (comp), &old_text);
		if (old_text.value)
			new_text.value = g_strdup_printf ("Cancelled: %s", old_text.value);
		else
			new_text.value = g_strdup_printf ("Cancelled");
		new_text.altrep = NULL;
		e_cal_component_set_summary (E_CAL_COMPONENT (comp), &new_text);

		e_cal_component_set_transparency (E_CAL_COMPONENT (comp), E_CAL_COMPONENT_TRANSP_TRANSPARENT);

		status = e2k_context_delete (ctx, NULL, href);
		if (E2K_HTTP_STATUS_IS_SUCCESSFUL (status)) {
			retval = E_CAL_BACKEND_EXCHANGE_BOOKING_OK;
		} else {
			retval = E_CAL_BACKEND_EXCHANGE_BOOKING_ERROR;
		}

		g_free (object);
		goto cleanup;
	} else {
		/* Check that the new appointment doesn't conflict with any
		 * existing appointment.
		 */
		e_cal_component_get_dtstart (E_CAL_COMPONENT (comp), &dt);
		izone = e_cal_backend_internal_get_timezone (E_CAL_BACKEND (cbex), dt.tzid);
		tt = icaltime_as_timet_with_zone (*dt.value, izone);
		e_cal_component_free_datetime (&dt);
		startz = e2k_make_timestamp (tt);

		e_cal_component_get_dtend (E_CAL_COMPONENT (comp), &dt);
		izone = e_cal_backend_internal_get_timezone (E_CAL_BACKEND (cbex), dt.tzid);
		tt = icaltime_as_timet_with_zone (*dt.value, izone);
		e_cal_component_free_datetime (&dt);
		endz = e2k_make_timestamp (tt);

		prop_name = E2K_PR_CALENDAR_UID;
		rn = e2k_restriction_andv (
			e2k_restriction_prop_bool (
				E2K_PR_DAV_IS_COLLECTION, E2K_RELOP_EQ, FALSE),
			e2k_restriction_prop_string (
				E2K_PR_DAV_CONTENT_CLASS, E2K_RELOP_EQ,
				"urn:content-classes:appointment"),
			e2k_restriction_prop_string (
				E2K_PR_CALENDAR_UID, E2K_RELOP_NE, uid),
			e2k_restriction_prop_date (
				E2K_PR_CALENDAR_DTEND, E2K_RELOP_GT, startz),
			e2k_restriction_prop_date (
				E2K_PR_CALENDAR_DTSTART, E2K_RELOP_LT, endz),
			e2k_restriction_prop_string (
				E2K_PR_CALENDAR_BUSY_STATUS, E2K_RELOP_NE, "FREE"),
			NULL);

		iter = e2k_context_search_start (ctx, NULL, cal_uri,
						     &prop_name, 1, rn, NULL, FALSE);
		g_free (startz);
		g_free (endz);

		result = e2k_result_iter_next (iter);
		if (result) {
			cal_uid = e2k_properties_get_prop (result[0].props, E2K_PR_CALENDAR_UID);
		}
		if (result && cal_uid) {
			retval = E_CAL_BACKEND_EXCHANGE_BOOKING_BUSY;

			status = e2k_result_iter_free (iter);

			if (!E2K_HTTP_STATUS_IS_SUCCESSFUL (status)) {
				if (status == E2K_HTTP_UNAUTHORIZED)
					retval = E_CAL_BACKEND_EXCHANGE_BOOKING_PERMISSION_DENIED;
				else
					retval = E_CAL_BACKEND_EXCHANGE_BOOKING_ERROR;
			}
			e2k_restriction_unref (rn);
			goto cleanup;
		}
		e2k_restriction_unref (rn);
	}

	/* We're good. Book it. */
	/* e_cal_component_set_href (comp, href); */

	icalparameter_set_partstat (part_param, ICAL_PARTSTAT_ACCEPTED);

	e_cal_component_commit_sequence (comp);
	calobj = (gchar *) e_cal_component_get_as_string (comp);

	/* status = e_cal_component_update (comp, method, FALSE  ); */
	if (ecomp) {
		gboolean modify_ok = FALSE;
		/* Use the PUT method to create the meeting item in the resource's calendar. */
		if (modify_object_with_href (E_CAL_BACKEND_SYNC (cbex), cal, calobj, book_all ? CALOBJ_MOD_ALL : CALOBJ_MOD_THIS, &old_object, &new_object, href, NULL, NULL)) {
			/* Need this to update the participation status of the resource
			   in the organizer's calendar. */
			modify_ok = modify_object_with_href (E_CAL_BACKEND_SYNC (cbex), cal, calobj, book_all ? CALOBJ_MOD_ALL : CALOBJ_MOD_THIS, &old_object, &new_object, NULL, NULL, NULL);
		} else {
			retval = E_CAL_BACKEND_EXCHANGE_BOOKING_ERROR;
			goto cleanup;
		}
		if (modify_ok) {
			e_cal_backend_notify_object_modified (E_CAL_BACKEND (cbex), old_object, calobj);
			retval = E_CAL_BACKEND_EXCHANGE_BOOKING_OK;
		}
		g_free (old_object);
		g_free (new_object);
	} else {
		GError *err = NULL;

		create_object (E_CAL_BACKEND_SYNC (cbex), cal, &calobj, &returned_uid, &err);
		if (!err) {
			e_cal_backend_notify_object_created (E_CAL_BACKEND (cbex), calobj);
			retval = E_CAL_BACKEND_EXCHANGE_BOOKING_OK;
		} else {
			g_error_free (err);
		}
	}

 cleanup:
	g_object_unref (comp);
	if (href)
		g_free (href);
	if (cal_uri)
		g_free (cal_uri);
	if (top_uri)
		g_free (top_uri);
	g_free (calobj);

	return retval;
}

static void
send_objects (ECalBackendSync *backend, EDataCal *cal,
	      const gchar *calobj,
	      GList **users, gchar **modified_calobj, GError **error)
{
	ECalBackendExchange *cbex = (ECalBackendExchange *) backend;
	ECalBackendExchangeBookingResult result;
	ECalComponent *comp = NULL;
	icalcomponent *top_level = NULL, *icalcomp;
	icalproperty *prop;
	icalproperty_method method;
	GError *err = NULL;

	e_return_data_cal_error_if_fail (E_IS_CAL_BACKEND_EXCHANGE (cbex), InvalidArg);

	if (!e_cal_backend_exchange_is_online (E_CAL_BACKEND_EXCHANGE (cbex))) {
		g_propagate_error (error, EDC_ERROR (RepositoryOffline));
		return;
	}

	*users = NULL;
	*modified_calobj = NULL;

	top_level = icalparser_parse_string (calobj);

	/* map time zones against system time zones and handle conflicting definitions */
	if (top_level &&
	    !e_cal_check_timezones (top_level,
				    NULL,
				    e_cal_backend_exchange_lookup_timezone,
				    cbex,
				    &err)) {
		g_warning ("checking timezones failed: %s", err->message);
		g_propagate_error (error, EDC_ERROR_EX (OtherError, err->message));
		g_clear_error (&err);
		goto cleanup;
	}

	icalcomp = icalcomponent_new_clone (icalcomponent_get_inner (top_level));

	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (E_CAL_COMPONENT (comp),
					   icalcomp);

	method = icalcomponent_get_method (top_level);
	if (icalcomponent_isa (icalcomp) != ICAL_VEVENT_COMPONENT
	    || (method != ICAL_METHOD_REQUEST && method != ICAL_METHOD_CANCEL)) {
		*modified_calobj = g_strdup (calobj);
		goto cleanup;
	}

	/* traverse all timezones to add them to the backend */
	add_timezones_from_comp (cbex, top_level);

	for (prop = icalcomponent_get_first_property (icalcomp, ICAL_ATTENDEE_PROPERTY);
	     prop != NULL;
	     prop = icalcomponent_get_next_property (icalcomp, ICAL_ATTENDEE_PROPERTY))
	{
		icalvalue *value;
		icalparameter *param;
		const gchar *attendee;

		param = icalproperty_get_first_parameter (prop, ICAL_CUTYPE_PARAMETER);
		if (!param)
			continue;
		if (icalparameter_get_cutype (param) != ICAL_CUTYPE_RESOURCE)
			continue;

		value = icalproperty_get_value (prop);
		if (!value)
			continue;

		attendee = icalvalue_get_string (value);
		if (g_ascii_strncasecmp ("mailto:", attendee, 7))
			continue;

		param = icalproperty_get_first_parameter (prop, ICAL_PARTSTAT_PARAMETER);
		/* modify all instances for the recurring event */
		result = book_resource (cbex, cal, attendee + 7, comp, method, param, icalcomponent_get_first_property (icalcomp, ICAL_RRULE_PROPERTY) || icalcomponent_get_first_property (icalcomp, ICAL_RDATE_PROPERTY));
		switch (result) {
		case E_CAL_BACKEND_EXCHANGE_BOOKING_OK:
			*users = g_list_append (*users, g_strdup (attendee));
			break;

		case E_CAL_BACKEND_EXCHANGE_BOOKING_BUSY:
#if 0
			g_snprintf (error_msg, 256,
				    _("The resource '%s' is busy during the selected time period."),
				    attendee + 7);
#endif
			g_propagate_error (error, EDC_ERROR (ObjectIdAlreadyExists));
			goto cleanup;

		case E_CAL_BACKEND_EXCHANGE_BOOKING_PERMISSION_DENIED:
		case E_CAL_BACKEND_EXCHANGE_BOOKING_NO_SUCH_USER:
			/* Do nothing, we fallback to iMip */
			break;
		case E_CAL_BACKEND_EXCHANGE_BOOKING_ERROR:
			/* What should we do here? */
			g_propagate_error (error, EDC_ERROR (PermissionDenied));
			goto cleanup;
		}
	}

	e_cal_component_commit_sequence (comp);
	*modified_calobj = g_strdup (e_cal_component_get_as_string (comp));

 cleanup:
	if (top_level) {
		icalcomponent_free (top_level);
	}
	if (comp) {
		g_object_unref (comp);
	}
}

#define THIRTY_MINUTES (30 * 60)
#define E2K_FBCHAR_TO_BUSYSTATUS(ch) ((ch) - '0')

static icalproperty *
create_freebusy_prop (E2kBusyStatus fbstatus, time_t start, time_t end)
{
	icaltimezone *utc = icaltimezone_get_utc_timezone ();
	icalproperty *prop;
	icalparameter *param;
	struct icalperiodtype ipt;

	switch (fbstatus) {
	case E2K_BUSYSTATUS_FREE:
		param = icalparameter_new_fbtype (ICAL_FBTYPE_FREE);
		break;
	case E2K_BUSYSTATUS_TENTATIVE:
		param = icalparameter_new_fbtype (ICAL_FBTYPE_BUSYTENTATIVE);
		break;
	case E2K_BUSYSTATUS_BUSY:
		param = icalparameter_new_fbtype (ICAL_FBTYPE_BUSY);
		break;
	case E2K_BUSYSTATUS_OOF:
		param = icalparameter_new_fbtype (ICAL_FBTYPE_BUSYUNAVAILABLE);
		break;
	default:
		return NULL;
	}

	ipt.start = icaltime_from_timet_with_zone (start, 0, utc);
	ipt.end = icaltime_from_timet_with_zone (end, 0, utc);
	prop = icalproperty_new_freebusy (ipt);
	icalproperty_add_parameter (prop, param);

	return prop;
}

static void
set_freebusy_info (icalcomponent *vfb, const gchar *data, time_t start)
{
	const gchar *span_start, *span_end;
	E2kBusyStatus busy;
	icalproperty *prop;
	time_t end;

	for (span_start = span_end = data, end = start;
	     *span_start;
	     span_start = span_end, start = end) {
		busy = E2K_FBCHAR_TO_BUSYSTATUS (*span_start);
		while (*span_end == *span_start) {
			span_end++;
			end += THIRTY_MINUTES;
		}

		prop = create_freebusy_prop (busy, start, end);
		if (prop)
			icalcomponent_add_property (vfb, prop);
	}
}

static void
discard_alarm (ECalBackendSync *backend, EDataCal *cal,
		const gchar *uid, const gchar *auid, GError **error)
{
	ECalBackendExchange *cbex = NULL;
	ECalBackendExchangeComponent *ecbexcomp;
	ECalComponent *ecomp;
	gchar *ecomp_str;
	icalcomponent *icalcomp = NULL;

	e_return_data_cal_error_if_fail (E_IS_CAL_BACKEND_EXCHANGE_CALENDAR (backend), InvalidArg);
	e_return_data_cal_error_if_fail (E_IS_DATA_CAL (cal), InvalidArg);
	e_return_data_cal_error_if_fail (uid != NULL, InvalidArg);
	e_return_data_cal_error_if_fail (auid != NULL, InvalidArg);

	d(printf("ecbe_discard_alarm(%p, %p, uid=%s, auid=%s)\n", backend, cal, uid, auid));

	if (!e_cal_backend_exchange_is_online (E_CAL_BACKEND_EXCHANGE (backend))) {
		g_propagate_error (error, EDC_ERROR (RepositoryOffline));
                return;
        }

	cbex = E_CAL_BACKEND_EXCHANGE (backend);

	e_cal_backend_exchange_cache_lock (cbex);
	ecbexcomp = get_exchange_comp (cbex, uid);

	if (!ecbexcomp) {
		e_cal_backend_exchange_cache_unlock (cbex);
		g_propagate_error (error, EDC_ERROR (ObjectNotFound));
		return;
	}

	ecomp = e_cal_component_new ();
	if (e_cal_component_set_icalcomponent (ecomp, icalcomponent_new_clone (ecbexcomp->icomp)) && !e_cal_component_has_recurrences (ecomp))
	{
		e_cal_component_remove_alarm (ecomp, auid);
		ecomp_str = e_cal_component_get_as_string (ecomp);
		icalcomp = icalparser_parse_string (ecomp_str);
		if (!e_cal_backend_exchange_modify_object ( cbex,
					icalcomp, CALOBJ_MOD_ALL, FALSE)) {
			g_propagate_error (error, EDC_ERROR (OtherError));
		}
		icalcomponent_free (icalcomp);
		g_free (ecomp_str);
	}
	g_object_unref (ecomp);

	e_cal_backend_exchange_cache_unlock (cbex);
}

static void
get_free_busy (ECalBackendSync *backend, EDataCal *cal,
	       GList *users, time_t start, time_t end,
	       GList **freebusy, GError **perror)
{
	ECalBackendExchange *cbex = E_CAL_BACKEND_EXCHANGE (backend);
	gchar *start_str, *end_str;
	GList *l;
	GString *uri;
	SoupBuffer *response;
	E2kHTTPStatus http_status;
	icaltimezone *utc = icaltimezone_get_utc_timezone ();
	xmlNode *recipients, *item;
	xmlDoc *doc;

	if (!e_cal_backend_exchange_is_online (E_CAL_BACKEND_EXCHANGE (backend))) {
		g_propagate_error (perror, EDC_ERROR (RepositoryOffline));
                return;
        }

	/* The calendar component sets start to "exactly 24 hours
	 * ago". But since we're going to get the information in
	 * 30-minute intervals starting from "start", we want to round
	 * off to the nearest half hour.
	 */
	start = (start / THIRTY_MINUTES) * THIRTY_MINUTES;

	start_str = e2k_make_timestamp (start);
	end_str   = e2k_make_timestamp (end);

	uri = g_string_new (cbex->account->public_uri);
	g_string_append (uri, "/?Cmd=freebusy&start=");
	g_string_append (uri, start_str);
	g_string_append (uri, "&end=");
	g_string_append (uri, end_str);
	g_string_append (uri, "&interval=30");
	for (l = users; l; l = l->next) {
		g_string_append (uri, "&u=SMTP:");
		g_string_append (uri, (gchar *) l->data);
	}
	g_free (start_str);
	g_free (end_str);

	http_status = e2k_context_get_owa (exchange_account_get_context (cbex->account),
					   NULL, uri->str, TRUE, &response);
	g_string_free (uri, TRUE);
	if (http_status != E2K_HTTP_OK) {
		g_propagate_error (perror, EDC_ERROR_HTTP_STATUS (http_status));
		return;
	}

	/* Parse the XML free/busy response */
	doc = e2k_parse_xml (response->data, response->length);
	soup_buffer_free (response);
	if (!doc) {
		g_propagate_error (perror, EDC_ERROR_EX (OtherError, "Failed to parse server response"));
		return;
	}

	recipients = e2k_xml_find (doc->children, "recipients");
	if (!recipients) {
		xmlFreeDoc (doc);
		g_propagate_error (perror, EDC_ERROR_EX (OtherError, "No 'recipients' in returned XML"));
	}

	*freebusy = NULL;
	for (item = e2k_xml_find_in (recipients, recipients, "item");
	     item;
	     item = e2k_xml_find_in (item, recipients, "item")) {
		icalcomponent *vfb;
		icalproperty *organizer;
		xmlNode *node, *fbdata;
		gchar *org_uri, *calobj;
		gchar *content;

		fbdata = e2k_xml_find_in (item, item, "fbdata");
		if (!fbdata || !fbdata->children || !fbdata->children->content)
			continue;

		node = e2k_xml_find_in (item, item, "email");
		if (!node || !node->children || !node->children->content)
			continue;
		org_uri = g_strdup_printf ("MAILTO:%s", node->children->content);
		organizer = icalproperty_new_organizer (org_uri);
		g_free (org_uri);

		node = e2k_xml_find_in (item, item, "displayname");
		if (node && node->children && node->children->content) {
			icalparameter *cn;

			content = (gchar *) node->children->content;
			cn = icalparameter_new_cn (content);
			icalproperty_add_parameter (organizer, cn);
		}

		vfb = icalcomponent_new_vfreebusy ();
		icalcomponent_set_dtstart (vfb, icaltime_from_timet_with_zone (start, 0, utc));
		icalcomponent_set_dtend (vfb, icaltime_from_timet_with_zone (end, 0, utc));
		icalcomponent_add_property (vfb, organizer);

		content = (gchar *) fbdata->children->content;
		set_freebusy_info (vfb, content, start);

		calobj = icalcomponent_as_ical_string_r (vfb);
		*freebusy = g_list_prepend (*freebusy, calobj);
		icalcomponent_free (vfb);
	}
	xmlFreeDoc (doc);
}

static void
init (ECalBackendExchangeCalendar *cbexc)
{
	cbexc->priv = g_new0 (ECalBackendExchangeCalendarPrivate, 1);
	cbexc->priv->is_loaded = FALSE;
	cbexc->priv->mutex = g_mutex_new ();
}

static void
dispose (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
finalize (GObject *object)
{
	ECalBackendExchangeCalendar *cbexc =
		E_CAL_BACKEND_EXCHANGE_CALENDAR (object);

	if (cbexc->priv->mutex) {
		g_mutex_free (cbexc->priv->mutex);
		cbexc->priv->mutex = NULL;
	}

	g_free (cbexc->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
class_init (ECalBackendExchangeCalendarClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	ECalBackendSyncClass *sync_class = E_CAL_BACKEND_SYNC_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	sync_class->open_sync = open_calendar;
	sync_class->refresh_sync = refresh_calendar;
	sync_class->create_object_sync = create_object;
	sync_class->modify_object_sync = modify_object;
	sync_class->remove_object_sync = remove_object;
	sync_class->receive_objects_sync = receive_objects;
	sync_class->send_objects_sync = send_objects;
	sync_class->get_freebusy_sync = get_free_busy;
	sync_class->discard_alarm_sync = discard_alarm;

	object_class->dispose = dispose;
	object_class->finalize = finalize;
}

E2K_MAKE_TYPE (e_cal_backend_exchange_calendar, ECalBackendExchangeCalendar, class_init, init, PARENT_TYPE)
