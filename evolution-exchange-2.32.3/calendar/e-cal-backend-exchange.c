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

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <libedataserver/e-time-utils.h>
#include <libedataserver/e-data-server-util.h>

#include <glib.h>
#include <gio/gio.h>
#include <glib/gstdio.h>

#include <camel/camel.h>

#include "e-cal-backend-exchange.h"
#include "e2k-cal-utils.h"
#include <e2k-uri.h>

#include <e2k-propnames.h>
#include <e2k-restriction.h>
#include <mapi.h>

#include <e-folder-exchange.h>
#include <exchange-account.h>
#include <exchange-hierarchy.h>

#include <e2k-utils.h>

#include "tools/exchange-share-config-listener.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

struct ECalBackendExchangePrivate {
	gboolean read_only;

	/* Objects */
	GHashTable *objects, *cache_unseen;
	gchar *object_cache_file;
	gchar *lastmod;
	gchar *local_attachment_store;
	guint save_timeout_id;
	GMutex *set_lock;
	GMutex *open_lock;
	GMutex *cache_lock;

	/* Timezones */
	GHashTable *timezones;
	icaltimezone *default_timezone;
	gboolean is_loaded;
	CalMode mode;
};

#define PARENT_TYPE E_TYPE_CAL_BACKEND_SYNC
static GObjectClass *parent_class = NULL;

#define d(x)

static icaltimezone *
internal_get_timezone (ECalBackend *backend, const gchar *tzid);

static void
is_read_only (ECalBackendSync *backend, EDataCal *cal, gboolean *read_only, GError **perror)
{
	ECalBackendExchange *cbex = E_CAL_BACKEND_EXCHANGE (backend);

	d(printf("ecbe_is_read_only(%p, %p) -> %d\n", backend, cal, cbex->priv->read_only));

	*read_only = cbex->priv->read_only;
}

static void
get_cal_address (ECalBackendSync *backend, EDataCal *cal, gchar **address, GError **perror)
{
	ECalBackendExchange *cbex = E_CAL_BACKEND_EXCHANGE (backend);
	ExchangeHierarchy *hier;

	hier = e_folder_exchange_get_hierarchy (cbex->folder);
	d(printf("ecbe_get_cal_address(%p, %p) -> %s\n", backend, cal, hier->owner_email));
	*address = g_strdup (hier->owner_email);
}

static void
get_cal_owner (ECalBackendSync *backend, gchar **name)
{
	ECalBackendExchange *cbex = E_CAL_BACKEND_EXCHANGE (backend);
	ExchangeHierarchy *hier;

	g_return_if_fail (E_IS_CAL_BACKEND_EXCHANGE (cbex));

	hier = e_folder_exchange_get_hierarchy (cbex->folder);
	*name = g_strdup (hier->owner_name);
}

static void
get_alarm_email_address (ECalBackendSync *backend, EDataCal *cal, gchar **address, GError **perror)
{
	d(printf("ecbe_get_alarm_email_address(%p, %p)\n", backend, cal));

	/* We don't support email alarms.
	 * This should not have been called.
	 */
	*address = NULL;

	g_propagate_error (perror, EDC_ERROR (NotSupported));
}

static void
get_ldap_attribute (ECalBackendSync *backend, EDataCal *cal, gchar **attribute, GError **perror)
{
	d(printf("ecbe_get_ldap_attribute(%p, %p)\n", backend, cal));

	if (!attribute) {
		g_propagate_error (perror, EDC_ERROR (InvalidObject));
		return;
	}

	/* This is just a hack for SunONE */
	*attribute = NULL;
}

static void
get_static_capabilities (ECalBackendSync *backend, EDataCal *cal, gchar **capabilities, GError **perror)
{
	d(printf("ecbe_get_static_capabilities(%p, %p)\n", backend, cal));

	*capabilities = g_strdup (
		CAL_STATIC_CAPABILITY_NO_EMAIL_ALARMS ","
		CAL_STATIC_CAPABILITY_NO_TASK_ASSIGNMENT ","
		CAL_STATIC_CAPABILITY_NO_THISANDFUTURE ","
		CAL_STATIC_CAPABILITY_NO_THISANDPRIOR ","
		CAL_STATIC_CAPABILITY_REMOVE_ALARMS ","
		CAL_STATIC_CAPABILITY_REFRESH_SUPPORTED);
}

static gboolean
load_cache (ECalBackendExchange *cbex, E2kUri *e2kuri, GError **perror)
{
	icalcomponent *vcalcomp, *comp, *tmp_comp;
	struct icaltimetype comp_last_mod, folder_last_mod;
	icalcomponent_kind kind;
	icalproperty *prop;
	gchar *lastmod, *mangled_uri, *storage_dir;
	const gchar *user_cache_dir;
	const gchar *uristr;
	gint i;
	struct stat buf;

	uristr = e_cal_backend_get_uri (E_CAL_BACKEND (cbex));
	cbex->priv->object_cache_file =
		e_folder_exchange_get_storage_file (cbex->folder, "cache.ics");
	if (!cbex->priv->object_cache_file) {
		printf ("could not load cache for %s\n", uristr);
		g_propagate_error (perror, EDC_ERROR (OfflineUnavailable));
		return FALSE;
	}

	/* Fixme : Try avoiding to do this everytime we come here */
	mangled_uri = g_strdup (uristr);
	for (i = 0; i < strlen (mangled_uri); i++) {
		switch (mangled_uri[i]) {
		case ':' :
		case '/' :
#ifdef G_OS_WIN32
		case '\\' :
		case '<' :
		case '>' :
		case '|' :
#endif
			mangled_uri[i] = '_';
		}
	}

	user_cache_dir = e_get_user_cache_dir ();
	cbex->priv->local_attachment_store = g_build_filename (
		user_cache_dir, "calendar", mangled_uri, NULL);
	storage_dir = g_path_get_dirname (cbex->priv->object_cache_file);

	if (g_lstat(cbex->priv->local_attachment_store , &buf) < 0) {
#ifdef G_OS_UNIX
		gint failed = TRUE;

 again:
		if (symlink (storage_dir, cbex->priv->local_attachment_store) < 0)
			g_warning ("%s: symlink() failed: %s", G_STRFUNC, g_strerror (errno));
		else
			failed = FALSE;

		if (failed) {
			gchar *parent_dir = g_build_filename (user_cache_dir, "calendar", NULL);

			if (!g_file_test (parent_dir, G_FILE_TEST_IS_DIR)) {
				g_mkdir_with_parents (parent_dir, 0700);
				g_free (parent_dir);

				failed = FALSE;
				goto again;
			}

			g_free (parent_dir);
		}
#else
		g_warning ("should symlink %s->%s, huh?",
			   cbex->priv->local_attachment_store,
			   storage_dir);

		g_mkdir_with_parents (cbex->priv->local_attachment_store, 0700);
#endif
	}
	g_free (storage_dir);
	g_free (mangled_uri);

       /* Check if the cache file is present. If it is not present the account might
          be newly created one. It will be created while save the cache */
       if (!g_file_test (cbex->priv->object_cache_file, G_FILE_TEST_EXISTS))
               return TRUE;

	vcalcomp = e_cal_util_parse_ics_file (cbex->priv->object_cache_file);
	if (!vcalcomp) {
		g_propagate_error (perror, EDC_ERROR (InvalidObject));
		return FALSE;
	}

	if (icalcomponent_isa (vcalcomp) != ICAL_VCALENDAR_COMPONENT) {
		icalcomponent_free (vcalcomp);
		g_propagate_error (perror, EDC_ERROR (InvalidObject));
		return FALSE;
	}

	kind = e_cal_backend_get_kind (E_CAL_BACKEND (cbex));

	folder_last_mod = icaltime_null_time ();
	for (comp = icalcomponent_get_first_component (vcalcomp, kind);
	     comp;
	     comp = icalcomponent_get_next_component (vcalcomp, kind)) {
		prop = icalcomponent_get_first_property (comp, ICAL_LASTMODIFIED_PROPERTY);
		if (prop) {
			comp_last_mod = icalproperty_get_lastmodified (prop);
			if (icaltime_compare (comp_last_mod, folder_last_mod) > 0)
				folder_last_mod = comp_last_mod;
		}

		lastmod = e2k_timestamp_from_icaltime (comp_last_mod);
		e_cal_backend_exchange_add_object (cbex, NULL, lastmod, comp);
		g_free (lastmod);
	}
	cbex->priv->lastmod = e2k_timestamp_from_icaltime (folder_last_mod);

	for (comp = icalcomponent_get_first_component (vcalcomp, ICAL_VTIMEZONE_COMPONENT);
	     comp;
	     comp = icalcomponent_get_next_component (vcalcomp, ICAL_VTIMEZONE_COMPONENT)) {
		tmp_comp = icalcomponent_new_clone (comp);
		if (tmp_comp) {
			e_cal_backend_exchange_add_timezone (cbex, tmp_comp, perror);
			icalcomponent_free (tmp_comp);
		}
	}

	icalcomponent_free (vcalcomp);
	return !perror || !*perror;
}

static void
save_timezone (gpointer key, gpointer tz, gpointer vcalcomp)
{
	icalcomponent *tzcomp;

	tzcomp = icalcomponent_new_clone (icaltimezone_get_component (tz));
	icalcomponent_add_component (vcalcomp, tzcomp);
}

static void
save_object (gpointer key, gpointer value, gpointer vcalcomp)
{
	ECalBackendExchangeComponent *ecomp = value;
	icalcomponent *icalcomp;
	GList *l;

	if (ecomp->icomp) {
		icalcomp = icalcomponent_new_clone (ecomp->icomp);
		icalcomponent_add_component (vcalcomp, icalcomp);
	}

	for (l = ecomp->instances; l; l = l->next) {
		if (l->data) {
			icalcomp = icalcomponent_new_clone (l->data);
			icalcomponent_add_component (vcalcomp, icalcomp);
		}
	}
}

static gboolean
timeout_save_cache (gpointer user_data)
{
	ECalBackendExchange *cbex = user_data;
	icalcomponent *vcalcomp;
	gchar *data = NULL, *tmpfile;
	gsize len, nwrote;
	FILE *f;

	d(printf("timeout_save_cache\n"));
	cbex->priv->save_timeout_id = 0;

	vcalcomp = e_cal_util_new_top_level ();
	g_hash_table_foreach (cbex->priv->timezones, save_timezone, vcalcomp);
	g_hash_table_foreach (cbex->priv->objects, save_object, vcalcomp);
	data = icalcomponent_as_ical_string_r (vcalcomp);
	icalcomponent_free (vcalcomp);

	tmpfile = g_strdup_printf ("%s~", cbex->priv->object_cache_file);
	f = g_fopen (tmpfile, "wb");
	if (!f)
		goto error;

	len = strlen (data);
	nwrote = fwrite (data, 1, len, f);
	if (fclose (f) != 0 || nwrote != len)
		goto error;

	if (g_rename (tmpfile, cbex->priv->object_cache_file) != 0)
		g_unlink (tmpfile);
error:
	g_free (tmpfile);
	g_free (data);
	return FALSE;
}

static void
save_cache (ECalBackendExchange *cbex)
{
	/* This is just a cache, so if we crash with unsaved changes,
	 * it's not a big deal. So we use a reasonably large timeout.
	 */
	if (cbex->priv->save_timeout_id)
		g_source_remove (cbex->priv->save_timeout_id);
	cbex->priv->save_timeout_id = g_timeout_add (6 * 1000,
						     timeout_save_cache,
						     cbex);
}

static void
open_calendar (ECalBackendSync *backend, EDataCal *cal, gboolean only_if_exists,
	       const gchar *username, const gchar *password, GError **perror)
{
	ECalBackendExchange *cbex = E_CAL_BACKEND_EXCHANGE (backend);
	const gchar *uristr;
	ExchangeHierarchy *hier;
	ExchangeAccountResult acresult;
	const gchar *prop = PR_ACCESS;
	E2kHTTPStatus status;
	gboolean load_result;
	E2kResult *results;
	E2kUri *euri = NULL;
	gint nresults = 0;
	guint access = 0;

	d(printf("ecbe_open_calendar(%p, %p, %sonly if exists, user=%s, pass=%s)\n", backend, cal, only_if_exists?"":"not ", username?username:"(null)", password?password:"(null)"));

	uristr = e_cal_backend_get_uri (E_CAL_BACKEND (backend));

	g_mutex_lock (cbex->priv->open_lock);

	if (cbex->priv->mode == CAL_MODE_LOCAL) {
		ESource *source;
		const gchar *display_contents = NULL;

		d(printf ("ECBE : cal is offline .. load cache\n"));

		cbex->priv->read_only = TRUE;
		source = e_cal_backend_get_source (E_CAL_BACKEND (cbex));
		display_contents = e_source_get_property (source, "offline_sync");

		if (!display_contents || !g_str_equal (display_contents, "1")) {
			g_mutex_unlock (cbex->priv->open_lock);
			g_propagate_error (perror, EDC_ERROR (RepositoryOffline));
			return;
		}

		cbex->account = exchange_share_config_listener_get_account_for_uri (NULL, uristr);

		if (cbex->account) {
			exchange_account_set_offline (cbex->account);
			if (!exchange_account_connect (cbex->account, NULL, &acresult)) {
				cbex->folder = exchange_account_get_folder (cbex->account, uristr);
			}
		}

		if (cbex->priv->is_loaded) {
			g_mutex_unlock (cbex->priv->open_lock);
			return;
		}

		euri = e2k_uri_new (uristr);
		load_result = load_cache (cbex, euri, perror);
		e2k_uri_free (euri);

		if (load_result)
			cbex->priv->is_loaded = TRUE;
		g_mutex_unlock (cbex->priv->open_lock);
		return;
	}

	/* What else to check */
	if (cbex->priv->is_loaded && cbex->account && exchange_account_get_context (cbex->account)) {
		g_mutex_unlock (cbex->priv->open_lock);
		return;
	}

	/* Make sure we have an open connection */
	/* This steals the ExchangeAccount from ExchangeComponent */
	if (!cbex->account) {
		cbex->account = exchange_share_config_listener_get_account_for_uri (NULL, uristr);
	}

	if (!cbex->account) {
		g_mutex_unlock (cbex->priv->open_lock);
		g_propagate_error (perror, EDC_ERROR (NoSuchCal));
		return;
	}

	exchange_account_set_online (cbex->account);

	exchange_account_connect (cbex->account, password, &acresult);
	if (acresult != EXCHANGE_ACCOUNT_CONNECT_SUCCESS) {
		g_mutex_unlock (cbex->priv->open_lock);
		g_propagate_error (perror, EDC_ERROR (AuthenticationFailed));
		return;
	}

	cbex->folder = exchange_account_get_folder (cbex->account, uristr);
	if (!cbex->folder) {
		ESource *source;
		const gchar *foreign;
		ExchangeHierarchy *hier_to_rescan = NULL;
		/* FIXME: theoretically we should create it if
		 * only_if_exists is FALSE.
		 */

		source = e_cal_backend_get_source (E_CAL_BACKEND (cbex));
		foreign = e_source_get_property (source, "foreign");

		if (foreign && (g_str_equal (foreign, "1"))) {
			gchar **split_path;
			const gchar *email, *path;

			path = strrchr (uristr, ';');
			split_path = g_strsplit (++path, "/", -1);
			email = split_path[0];

			exchange_account_scan_foreign_hierarchy (cbex->account, email);

			cbex->folder = exchange_account_get_folder (cbex->account, uristr);
			if (!cbex->folder) {
				/* Folder is not known at the moment, thus try to rescan foreign
				   folder, just in case we didn't scan it fully yet. */
				hier_to_rescan = exchange_account_get_hierarchy_by_email (cbex->account, email);
			}

			g_strfreev (split_path);
		} else {
			/* Rescan to see if this is any new calendar */
			hier_to_rescan = exchange_account_get_hierarchy_by_type (cbex->account, EXCHANGE_HIERARCHY_PERSONAL);
			if (!hier_to_rescan) {
				g_mutex_unlock (cbex->priv->open_lock);
				g_propagate_error (perror, EDC_ERROR (RepositoryOffline));
				return;
			}
		}

		if (hier_to_rescan) {
			g_object_ref (hier_to_rescan->toplevel);
			e_folder_exchange_set_rescan_tree (hier_to_rescan->toplevel, TRUE);
			exchange_hierarchy_scan_subtree (hier_to_rescan, hier_to_rescan->toplevel, ONLINE_MODE);
			e_folder_exchange_set_rescan_tree (hier_to_rescan->toplevel, FALSE);
			g_object_unref (hier_to_rescan->toplevel);

			cbex->folder = exchange_account_get_folder (cbex->account, uristr);
		}

		if (!cbex->folder) {
			g_mutex_unlock (cbex->priv->open_lock);
			g_propagate_error (perror, EDC_ERROR (NoSuchCal));
			return;
		}
	}
	g_object_ref (cbex->folder);

	hier = e_folder_exchange_get_hierarchy (cbex->folder);
	if (hier->hide_private_items) {
		cbex->private_item_restriction =
			e2k_restriction_prop_int (
				E2K_PR_MAPI_SENSITIVITY, E2K_RELOP_NE, 2);
	} else
		cbex->private_item_restriction = NULL;

	status = e_folder_exchange_propfind (cbex->folder, NULL,
					     &prop, 1,
					     &results, &nresults);
	if (E2K_HTTP_STATUS_IS_SUCCESSFUL (status) && nresults >= 1) {
		prop = e2k_properties_get_prop (results[0].props, PR_ACCESS);
		if (prop)
			access = atoi (prop);
	}

	if (!(access & MAPI_ACCESS_READ)) {
		g_mutex_unlock (cbex->priv->open_lock);
		if (nresults)
			e2k_results_free (results, nresults);
		g_propagate_error (perror, EDC_ERROR (PermissionDenied));
		return;
	}

	cbex->priv->read_only = ((access & MAPI_ACCESS_CREATE_CONTENTS) == 0);

	if (load_cache (cbex, euri, perror))
		cbex->priv->is_loaded = TRUE;

	g_mutex_unlock (cbex->priv->open_lock);

	if (nresults)
		e2k_results_free (results, nresults);
}

static void
remove_calendar (ECalBackendSync *backend, EDataCal *cal, GError **perror)
{
	ECalBackendExchange *cbex = E_CAL_BACKEND_EXCHANGE (backend);
	ExchangeAccountFolderResult result;
	const gchar *uri;

	d(printf("ecbe_remove_calendar(%p, %p)\n", backend, cal));

	/* If we do not have a folder, then that means there is no corresponding folder on the server,
	   thus pretend we removed it successfully. It's not there anyway, thus should be fine. */
	if (!cbex->folder)
		return;

	uri = e_folder_exchange_get_internal_uri (cbex->folder);
	result = exchange_account_remove_folder (cbex->account, uri);
	if (result == EXCHANGE_ACCOUNT_FOLDER_OK)
		/* Success */;
	else if (result == EXCHANGE_ACCOUNT_FOLDER_DOES_NOT_EXIST)
		g_propagate_error (perror, EDC_ERROR (NoSuchCal));
	else if (result == EXCHANGE_ACCOUNT_FOLDER_UNSUPPORTED_OPERATION)
		g_propagate_error (perror, EDC_ERROR (PermissionDenied));
	else if (result == EXCHANGE_ACCOUNT_FOLDER_OFFLINE)
		g_propagate_error (perror, EDC_ERROR (OfflineUnavailable));
	else if (result == EXCHANGE_ACCOUNT_FOLDER_PERMISSION_DENIED)
		g_propagate_error (perror, EDC_ERROR (PermissionDenied));
	else
		g_propagate_error (perror, e_data_cal_create_error_fmt (OtherError, "Failed with FolderResult %d", result));
}

static void
add_to_unseen (gpointer key, gpointer value, gpointer data)
{
	ECalBackendExchange *cbex = data;

	g_hash_table_insert (cbex->priv->cache_unseen, key, value);
}

void
e_cal_backend_exchange_cache_sync_start (ECalBackendExchange *cbex)
{
	g_return_if_fail (cbex->priv->cache_unseen == NULL);

	cbex->priv->cache_unseen = g_hash_table_new (NULL, NULL);
	g_hash_table_foreach (cbex->priv->objects, add_to_unseen, cbex);
}

static gboolean
find_instance (ECalBackendExchange *cbex, ECalBackendExchangeComponent *ecomp, const gchar *rid, const gchar *lastmod)
{
	GList *l;
	gboolean found = FALSE;

	if (!ecomp->instances)
		return FALSE;

	for (l = ecomp->instances; l != NULL; l = l->next) {
		ECalComponent *comp = e_cal_component_new ();
		ECalComponentRange recur_id;
		struct icaltimetype inst_rid, new_rid;
		time_t rtime;
		icaltimezone *f_zone;

		e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (l->data));
		e_cal_component_get_recurid (comp, &recur_id);

		rtime = e2k_parse_timestamp (rid);
		new_rid = icaltime_from_timet (rtime, FALSE);

		f_zone = (recur_id.datetime.tzid && *recur_id.datetime.tzid) ? internal_get_timezone ((ECalBackend *) cbex, recur_id.datetime.tzid) : icaltimezone_get_utc_timezone ();
		recur_id.datetime.value->zone = f_zone;
		inst_rid = icaltime_convert_to_zone (*recur_id.datetime.value, icaltimezone_get_utc_timezone ());

		e_cal_component_free_datetime (&recur_id.datetime);
		g_object_unref (comp);

		if (icaltime_compare (inst_rid, new_rid) == 0) {
			found = TRUE;
			break;
		}
	}
	return found;
}

/**
 * e_cal_backend_exchange_ensure_utc_zone:
 * Makes sure the given icaltimetype is in UTC, because
 * RFC 2445 says CREATED/DTSTAMP/LAST-MODIFIED always in UTC.
 * @param cb ECalbackendExchage descendant.
 * @param itt What to convert to UTC, if required.
 **/
void
e_cal_backend_exchange_ensure_utc_zone (ECalBackend *cb, struct icaltimetype *itt)
{
	g_return_if_fail (cb != NULL);
	g_return_if_fail (itt != NULL);

	/* RFC 2445 - CREATED/DTSTAMP/LAST-MODIFIED always in UTC */
	if (!icaltime_is_null_time (*itt) && !icaltime_is_utc (*itt)) {
		if (!itt->zone)
			icaltime_set_timezone (itt, e_cal_backend_internal_get_default_timezone (cb));

		icaltimezone_convert_time (itt, (icaltimezone*) icaltime_get_timezone (*itt), icaltimezone_get_utc_timezone ());
		icaltime_set_timezone (itt, icaltimezone_get_utc_timezone ());
	}
}

gboolean
e_cal_backend_exchange_in_cache (ECalBackendExchange *cbex,
				 const gchar          *uid,
				 const gchar          *lastmod,
				 const gchar	     *href,
				 const gchar	     *rid
				 )
{
	ECalBackendExchangeComponent *ecomp;

	g_return_val_if_fail (cbex->priv->cache_unseen != NULL, FALSE);

	ecomp = g_hash_table_lookup (cbex->priv->objects, uid);
	if (!ecomp)
		return FALSE;
	g_hash_table_remove (cbex->priv->cache_unseen, ecomp->uid);

	if (rid)
		return find_instance (cbex, ecomp, rid, lastmod);

	if (strcmp (ecomp->lastmod, lastmod) < 0) {
		g_hash_table_remove (cbex->priv->objects, uid);
		return FALSE;
	}

	/* Update the cache with the new href */
	if (href) {
		if (ecomp->href)
			g_free (ecomp->href);
		ecomp->href = g_strdup (href);
	}

	return TRUE;
}

static void
uncache (gpointer key, gpointer value, gpointer data)
{
	ECalBackendExchange *cbex = data;
	ECalBackend *backend = E_CAL_BACKEND (cbex);
	ECalComponentId *id = g_new0 (ECalComponentId, 1);
	ECalBackendExchangeComponent *ecomp;

	ecomp = (ECalBackendExchangeComponent *)value;

	/* FIXME Need get the recurrence id here */
	id->uid = g_strdup (key);
	id->rid = NULL;
	if (ecomp->icomp) {
		gchar *str = NULL;
		/* FIXME somehow the query does not match with the component in some cases, so user needs to press a
		   clear to get rid of the component from the view in that case*/
		str = icalcomponent_as_ical_string_r (ecomp->icomp);
		e_cal_backend_notify_object_removed (backend, id, icalcomponent_as_ical_string_r (ecomp->icomp)
				, NULL);
		g_free (str);
	}
	g_hash_table_remove (cbex->priv->objects, key);
	e_cal_component_free_id (id);
}

void
e_cal_backend_exchange_cache_sync_end (ECalBackendExchange *cbex)
{
	g_return_if_fail (cbex->priv->cache_unseen != NULL);

	g_hash_table_foreach (cbex->priv->cache_unseen, uncache, cbex);

	g_hash_table_destroy (cbex->priv->cache_unseen);
	cbex->priv->cache_unseen = NULL;

	save_cache (cbex);
}

/**
 * e_cal_backend_exchange_add_object:
 * @cbex: an #ECalBackendExchange
 * @href: the object's href
 * @lastmod: the object's last modtime, as an Exchange timestamp
 * @comp: the object
 *
 * Adds @comp to @cbex's cache
 *
 * Return value: %TRUE on success, %FALSE if @comp was already in
 * @cbex.
 **/
gboolean
e_cal_backend_exchange_add_object (ECalBackendExchange *cbex,
				   const gchar *href, const gchar *lastmod,
				   icalcomponent *comp)
{
	ECalBackendExchangeComponent *ecomp;
	gboolean is_instance;
	const gchar *uid;
	struct icaltimetype rid;

	d(printf("ecbe_add_object(%p, %s, %s)\n", cbex, href, lastmod));

	uid = icalcomponent_get_uid (comp);
	if (!uid)
		return FALSE;

	ecomp = g_hash_table_lookup (cbex->priv->objects, uid);

	is_instance = (icalcomponent_get_first_property (comp, ICAL_RECURRENCEID_PROPERTY) != NULL);

	d(printf("ecbe_add_object: is_instance: %d\n", is_instance));

	if (ecomp && ecomp->icomp && !is_instance)
		return FALSE;

	if (!ecomp) {
		ecomp = g_new0 (ECalBackendExchangeComponent, 1);
		ecomp->uid = g_strdup (uid);
		g_hash_table_insert (cbex->priv->objects, ecomp->uid, ecomp);
	}

	if (href) {
		g_free (ecomp->href);
		ecomp->href = g_strdup (href);
	}
	if (lastmod && (!ecomp->lastmod || strcmp (ecomp->lastmod, lastmod) > 0)) {
		g_free (ecomp->lastmod);
		ecomp->lastmod = g_strdup (lastmod);
	}

	if (is_instance) {
		GList *l;
		struct icaltimetype inst_rid;
		gboolean inst_found = FALSE;

		rid = icalcomponent_get_recurrenceid (comp);
		for (l = ecomp->instances; l; l = l->next) {
			inst_rid = icalcomponent_get_recurrenceid (l->data);
			if (icaltime_compare (inst_rid, rid) == 0) {
				inst_found = TRUE;
				break;
			}
		}
		if (!inst_found) {

			ecomp->instances = g_list_prepend (ecomp->instances,
						   icalcomponent_new_clone (comp));
			if (ecomp->icomp)
				e_cal_util_remove_instances (ecomp->icomp, rid, CALOBJ_MOD_THIS);
		}
	} else
		ecomp->icomp = icalcomponent_new_clone (comp);

	save_cache (cbex);
	return TRUE;
}

static void
discard_detached_instance (ECalBackendExchangeComponent *ecomp,
			   struct icaltimetype rid)
{
	GList *inst;
	struct icaltimetype inst_rid;

	for (inst = ecomp->instances; inst; inst = inst->next) {
		inst_rid = icalcomponent_get_recurrenceid (inst->data);
		if (icaltime_compare (inst_rid, rid) == 0) {
			ecomp->instances = g_list_remove (ecomp->instances, inst->data);
			icalcomponent_free (inst->data);
			break;
		}
	}

	if (ecomp->icomp) /* This check should ideally not be needed. */
		e_cal_util_remove_instances (ecomp->icomp, rid, CALOBJ_MOD_THIS);

	return;
}

/**
 * e_cal_backend_exchange_modify_object:
 * @cbex: an #ECalBackendExchange
 * @comp: the object
 * @mod: what parts of @comp to modify
 *
 * Modifies the component identified by @comp's UID, in the manner
 * specified by @comp and @mod.
 *
 * Return value: %TRUE on success, %FALSE if @comp was not found.
 **/
gboolean
e_cal_backend_exchange_modify_object (ECalBackendExchange *cbex,
				      icalcomponent *comp,
				      CalObjModType mod,
				      gboolean discard_detached)
{
	ECalBackendExchangeComponent *ecomp;
	const gchar *uid;
	struct icaltimetype rid;

	d(printf("ecbe_modify_object(%p)\n", cbex));

	g_return_val_if_fail (mod == CALOBJ_MOD_THIS || mod == CALOBJ_MOD_ALL,
			      FALSE);

	uid = icalcomponent_get_uid (comp);
	if (!uid)
		return FALSE;

	rid = icalcomponent_get_recurrenceid (comp);

	ecomp = g_hash_table_lookup (cbex->priv->objects, uid);
	if (!ecomp)
		return FALSE;

	if (mod == CALOBJ_MOD_ALL || icaltime_is_null_time (rid) || discard_detached) {
		if (ecomp->icomp)
			icalcomponent_free (ecomp->icomp);
		ecomp->icomp = icalcomponent_new_clone (comp);
		if (discard_detached && !icaltime_is_null_time (rid))
			discard_detached_instance (ecomp, rid);
	} else {
		ecomp->instances = g_list_prepend (ecomp->instances,
						   icalcomponent_new_clone (comp));
		/*FIXME This is a workaround to cover the timezone issue while setting rid from evolution */
		if (ecomp->icomp)
			e_cal_util_remove_instances (ecomp->icomp, rid, CALOBJ_MOD_THIS);
	}

	save_cache (cbex);
	return TRUE;
}

/**
 * e_cal_backend_exchange_remove_object:
 * @cbex: an #ECalBackendExchange
 * @uid: the UID of the object to remove
 *
 * Removes all instances of the component with UID @uid.
 *
 * Return value: %TRUE on success, %FALSE if @comp was not found.
 **/
gboolean
e_cal_backend_exchange_remove_object (ECalBackendExchange *cbex, const gchar *uid)
{
	d(printf("ecbe_remove_object(%p, %s)\n", cbex, uid));

	if (!g_hash_table_lookup (cbex->priv->objects, uid))
		return FALSE;

	g_hash_table_remove (cbex->priv->objects, uid);

	save_cache (cbex);
	return TRUE;
}

static void
discard_alarm (ECalBackendSync *backend, EDataCal *cal,
	       const gchar *uid, const gchar *auid, GError **perror)
{
	/* To be called from the Calendar derived class */
	g_propagate_error (perror, EDC_ERROR (NotSupported));
}

/*To be overriden by Calendar and Task classes*/
static void
create_object (ECalBackendSync *backend, EDataCal *cal,
	       gchar **calobj, gchar **uid, GError **perror)
{
	g_propagate_error (perror, EDC_ERROR (NotSupported));
}

/*To be overriden by Calendar and Task classes*/
static void
modify_object (ECalBackendSync *backend, EDataCal *cal,
			const gchar * calobj, CalObjModType mod, gchar **old_object, gchar **new_object, GError **perror)
{
	g_propagate_error (perror, EDC_ERROR (NotSupported));
}

void
e_cal_backend_exchange_cache_lock (ECalBackendExchange *cbex)
{
	g_mutex_lock (cbex->priv->cache_lock);
}

void
e_cal_backend_exchange_cache_unlock (ECalBackendExchange *cbex)
{
	g_mutex_unlock (cbex->priv->cache_lock);
}

ECalBackendExchangeComponent *
get_exchange_comp (ECalBackendExchange *cbex, const gchar *uid)
{
	ECalBackendExchangeComponent *ecomp;

	if (!uid)
		return NULL;

	ecomp = g_hash_table_lookup (cbex->priv->objects, uid);
	if (ecomp)
		return ecomp;

	return NULL;
}

static void
add_instances_to_vcal (gpointer value, gpointer user_data)
{
	icalcomponent *recurrence = value;
	icalcomponent *vcalendar = user_data;

	icalcomponent_add_component (
		vcalendar,
		icalcomponent_new_clone (recurrence));

}

static void
get_object (ECalBackendSync *backend, EDataCal *cal,
	    const gchar *uid, const gchar *rid, gchar **object, GError **error)
{
	ECalBackendExchange *cbex = E_CAL_BACKEND_EXCHANGE (backend);
	ECalBackendExchangeComponent *ecomp;

	d(printf("ecbe_get_object(%p, %p, uid=%s, rid=%s)\n", backend, cal, uid, rid));

	e_return_data_cal_error_if_fail (uid != NULL, InvalidArg);
	/*any other asserts?*/

	*object = NULL;

	g_mutex_lock (cbex->priv->cache_lock);
	ecomp = g_hash_table_lookup (cbex->priv->objects, uid);
	if (!ecomp) {
		g_mutex_unlock (cbex->priv->cache_lock);
		g_propagate_error (error, EDC_ERROR (ObjectNotFound));
		return;
	}

	if (!rid && (!(ecomp->icomp)))	{
		g_mutex_unlock (cbex->priv->cache_lock);
		g_propagate_error (error, EDC_ERROR (ObjectNotFound));
		return;
	}

	if (rid && *rid) {
		GList *l;
		struct icaltimetype inst_rid, key_rid;
		gboolean inst_found = FALSE;

		for (l = ecomp->instances; l; l = l->next) {
			key_rid = icaltime_from_string (rid);
			inst_rid = icalcomponent_get_recurrenceid (l->data);
			if (icaltime_compare (inst_rid, key_rid) == 0) {
				inst_found = TRUE;
				break;
			}
		}

		if (inst_found) {
			*object = icalcomponent_as_ical_string_r (l->data);
		} else {
			/* Instance is not found. Send the master object instead */
			if (ecomp->icomp) {
				icalcomponent *new_inst;
				struct icaltimetype itt;

				itt = icaltime_from_string (rid);
				new_inst = e_cal_util_construct_instance (ecomp->icomp, itt);
				if (!new_inst) {
					g_mutex_unlock (cbex->priv->cache_lock);
					g_propagate_error (error, EDC_ERROR (ObjectNotFound));
					return;
				}

				*object = icalcomponent_as_ical_string_r (new_inst);
				icalcomponent_free (new_inst);
			} else {
				/* Oops. No instance and no master as well !! */
				g_mutex_unlock (cbex->priv->cache_lock);
				g_propagate_error (error, EDC_ERROR (ObjectNotFound));
				return;
			} /* Close check for master object */
		} /* Close check for instance */
	}/* Close check if rid is being asked */
	else {
		/* rid is not asked */
		if (g_list_length (ecomp->instances) > 0) {
			/* Ok, we have detached instances, return a VCALENDAR */
			/* Send the whole object including all its detatched instances */
			icalcomponent *vcalcomp;

			vcalcomp = e_cal_util_new_top_level ();

			if (ecomp->icomp)
				icalcomponent_add_component (vcalcomp,
					icalcomponent_new_clone (ecomp->icomp));
			g_list_foreach (ecomp->instances, (GFunc) add_instances_to_vcal, vcalcomp);

			*object = icalcomponent_as_ical_string_r (vcalcomp);
			icalcomponent_free (vcalcomp);
		} else if (ecomp->icomp) {
			/* There are no detached instances. Send only the master object */
			*object = icalcomponent_as_ical_string_r (ecomp->icomp);
		}
	}
	g_mutex_unlock (cbex->priv->cache_lock);
}

gboolean
e_cal_backend_exchange_extract_components (const gchar *calobj,
					   icalproperty_method *method,
					   GList **comp_list, GError **perror)
{
	icalcomponent *icalcomp, *comp = NULL;
	icalcomponent *subcomp;
	icalcomponent_kind kind;
	GList *comps;

	icalcomp = icalparser_parse_string (calobj);
	if (!icalcomp) {
		g_propagate_error (perror, EDC_ERROR (InvalidObject));
		return FALSE;
	}

	kind = icalcomponent_isa (icalcomp);
	if (kind != ICAL_VCALENDAR_COMPONENT) {
		comp = icalcomp;
		icalcomp = e_cal_util_new_top_level ();
		icalcomponent_add_component (icalcomp, comp);
	}

	*method = icalcomponent_get_method (icalcomp);

#if 0
	/* Might have to include this later */
	/*time zone?*/
	subcomp = icalcomponent_get_first_component (icalcomp, ICAL_VTIMEZONE_COMPONENT);
	while (subcomp) {
		e_cal_backend_exchange_add_timezone (cbex, icalcomponent_new_clone (subcomp), perror);
		subcomp = icalcomponent_get_next_component (icalcomp, ICAL_VTIMEZONE_COMPONENT);
	}
#endif

	comps = NULL;
	subcomp = icalcomponent_get_first_component (icalcomp, ICAL_ANY_COMPONENT);
	while (subcomp)	{
		icalcomponent_kind child_kind = icalcomponent_isa (subcomp);
		switch (child_kind)	{
			case ICAL_VEVENT_COMPONENT:
			case ICAL_VTODO_COMPONENT:

				/*
					check time zone .....
					icalcomponent_foreach_tzid
				*/

				if (!icalcomponent_get_uid (subcomp)) {
					g_propagate_error (perror, EDC_ERROR (InvalidObject));
					return FALSE;
				}
				comps = g_list_prepend (comps, subcomp);
				break;
				/*journal?*/
			default:
				break;
		}
		subcomp = icalcomponent_get_next_component (icalcomp, ICAL_ANY_COMPONENT);
	}

	*comp_list = comps;

	return TRUE;
}

static void
send_objects (ECalBackendSync *backend, EDataCal *cal,
	      const gchar *calobj,
	      GList **users, gchar **modified_calobj, GError **perror)
{
	d(printf("ecbe_send_objects(%p, %p, %s)\n", backend, cal, calobj));

	g_propagate_error (perror, EDC_ERROR (NotSupported));
}

static void
get_default_object (ECalBackendSync *backend, EDataCal *cal, gchar **object, GError **perror)
{
	icalcomponent *comp;
	gchar *ical_obj;

	d(printf("ecbe_get_default_object(%p, %p)\n", backend, cal));

	comp = e_cal_util_new_component (e_cal_backend_get_kind (E_CAL_BACKEND (backend)));
	ical_obj = icalcomponent_as_ical_string_r (comp);
	*object = ical_obj;

	icalcomponent_free (comp);
}

typedef struct {
	GList *obj_list;
	gboolean search_needed;
	const gchar *query;
	ECalBackendSExp *obj_sexp;
	ECalBackend *backend;
	icaltimezone *default_zone;
} MatchObjectData;
static void
match_recurrence_sexp (gpointer data, gpointer user_data)
{
	icalcomponent *icomp = data;
	MatchObjectData *match_data = user_data;
	ECalComponent *comp = NULL;
	gchar * comp_as_string = NULL;

	d(printf("ecbe_match_recurrence_sexp(%p, %p)\n", icomp, match_data));

	if (!icomp || !match_data)
		return;

	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (icomp));

	if ((!match_data->search_needed) ||
	    (e_cal_backend_sexp_match_comp (match_data->obj_sexp, comp, match_data->backend))) {
		comp_as_string = e_cal_component_get_as_string (comp);
		match_data->obj_list = g_list_append (match_data->obj_list, comp_as_string);
		d(printf ("ecbe_match_recurrence_sexp: match found, adding \n%s\n", comp_as_string));
	}
	g_object_unref (comp);
}

static void
match_object_sexp (gpointer key, gpointer value, gpointer data)
{
	ECalBackendExchangeComponent *ecomp = value;
	MatchObjectData *match_data = data;
	ECalComponent *comp;

	/*
	   In case of detached instances with no master object,
	   ecomp->icomp will be NULL and hence should be skipped
	   from matching.
	*/
	if (!ecomp || !match_data)
		return;

	if (ecomp->icomp) {
		comp = e_cal_component_new ();
		e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (ecomp->icomp));

		if ((!match_data->search_needed) ||
		    (e_cal_backend_sexp_match_comp (match_data->obj_sexp, comp, match_data->backend))) {
			match_data->obj_list = g_list_append (match_data->obj_list,
						      e_cal_component_get_as_string (comp));
		}
		g_object_unref (comp);
	}

	/* match also recurrences */
	g_list_foreach (ecomp->instances,
			(GFunc) match_recurrence_sexp,
			match_data);
}

static void
get_object_list (ECalBackendSync *backend, EDataCal *cal,
		 const gchar *sexp, GList **objects, GError **perror)
{

	ECalBackendExchange *cbex;
	ECalBackendExchangePrivate *priv;
	MatchObjectData match_data;

	cbex = E_CAL_BACKEND_EXCHANGE (backend);
	priv = cbex->priv;

	match_data.search_needed = TRUE;
	match_data.query = sexp;
	match_data.obj_list = NULL;
	match_data.backend = E_CAL_BACKEND (backend);
	match_data.default_zone = cbex->priv->default_timezone;

	if (!strcmp (sexp, "#t"))
		match_data.search_needed = FALSE;

	match_data.obj_sexp = e_cal_backend_sexp_new (sexp);
	if (!match_data.obj_sexp) {
		g_propagate_error (perror, EDC_ERROR (InvalidQuery));
		return;
	}

	g_mutex_lock (priv->cache_lock);
	g_hash_table_foreach (cbex->priv->objects, (GHFunc) match_object_sexp, &match_data);
	g_mutex_unlock (priv->cache_lock);

	*objects = match_data.obj_list;

	g_object_unref (match_data.obj_sexp);
}

icaltimezone *
e_cal_backend_exchange_get_default_time_zone (ECalBackendSync *backend)
{
	ECalBackendExchange *cbex = E_CAL_BACKEND_EXCHANGE (backend);
	ECalBackendExchangePrivate *priv;

	priv = cbex->priv;

	return priv->default_timezone;
}

void
e_cal_backend_exchange_add_timezone (ECalBackendExchange *cbex,
				     icalcomponent *vtzcomp, GError **perror)
{
	icalproperty *prop;
	icaltimezone *zone;
	const gchar *tzid;

	d(printf("ecbe_add_timezone(%p)\n", cbex));

	prop = icalcomponent_get_first_property (vtzcomp, ICAL_TZID_PROPERTY);
	if (!prop) {
		g_propagate_error (perror, EDC_ERROR (InvalidObject));
		return;
	}
	tzid = icalproperty_get_tzid (prop);
	d(printf("ecbe_add_timezone: tzid = %s\n", tzid));
	if (g_hash_table_lookup (cbex->priv->timezones, tzid)) {
		g_propagate_error (perror, EDC_ERROR (ObjectIdAlreadyExists));
		return;
	}

	zone = icaltimezone_new ();
	if (!icaltimezone_set_component (zone, icalcomponent_new_clone (vtzcomp))) {
		icaltimezone_free (zone, TRUE);
		g_propagate_error (perror, EDC_ERROR (InvalidObject));
		return;
	}

	g_hash_table_insert (cbex->priv->timezones, g_strdup (tzid), zone);
}

static void
add_timezone (ECalBackendSync *backend, EDataCal *cal,
	      const gchar *tzobj, GError **perror)
{
	ECalBackendExchange *cbex = E_CAL_BACKEND_EXCHANGE (backend);
	icalcomponent *vtzcomp;
	GError *err = NULL;

	if (!tzobj) {
		g_propagate_error (perror, EDC_ERROR (InvalidObject));
		return;
	}

	vtzcomp = icalcomponent_new_from_string ((gchar *)tzobj);
	if (!vtzcomp) {
		g_propagate_error (perror, EDC_ERROR (InvalidObject));
		return;
	}

	e_cal_backend_exchange_add_timezone (cbex, vtzcomp, &err);
	switch (err ? err->code : Success) {
	case ObjectIdAlreadyExists:
		/* fall through */

	case Success:
		break;

	default:
		g_propagate_error (perror, err);
	}
	icalcomponent_free (vtzcomp);
}

static void
set_default_zone (ECalBackendSync *backend, EDataCal *cal, const gchar *tz, GError **perror)
{
	ECalBackendExchange *cbex = E_CAL_BACKEND_EXCHANGE (backend);
	icalcomponent *icalcomp = icalparser_parse_string (tz);
	icaltimezone *zone = NULL;

	d(printf("ecbe_set_default_zone(%p, %p, %s)\n", backend, cal, tz));
	/*
	   We call this function before calling e_cal_open in client and
	   hence we set the timezone directly.  In the implementation of
	   e_cal_open, we set this timezone to every-objects that we create.
	*/

	if (icalcomp) {
		const gchar *tzid;

		zone = icaltimezone_new ();
		icaltimezone_set_component (zone, icalcomp);

		tzid = icaltimezone_get_tzid (zone);

		if (tzid) {
			icaltimezone *known_zone;

			known_zone = icaltimezone_get_builtin_timezone_from_tzid (tzid);
			if (!known_zone)
				known_zone = g_hash_table_lookup (cbex->priv->timezones, tzid);

			if (known_zone) {
				icaltimezone_free (zone, 1);
				zone = known_zone;
			} else {
				g_hash_table_insert (cbex->priv->timezones, g_strdup (tzid), zone);
			}
		} else {
			icaltimezone_free (zone, 1);
			zone = NULL;
		}
	}

	cbex->priv->default_timezone = zone;
}

static void
start_query (ECalBackend *backend, EDataCalView *view)
{
	const gchar *sexp = NULL;
	GList *m, *objects = NULL;
	GError *error = NULL;

	d(printf("ecbe_start_query(%p, %p)\n", backend, view));

	sexp = e_data_cal_view_get_text (view);
	if (!sexp) {
		error = EDC_ERROR (InvalidQuery);
		e_data_cal_view_notify_done (view, error);
		g_error_free (error);
		return;
	}
	get_object_list (E_CAL_BACKEND_SYNC (backend), NULL, sexp, &objects, &error);
	if (error) {
		e_data_cal_view_notify_done (view, error);
		g_error_free (error);
		return;
	}

	if (objects) {
		e_data_cal_view_notify_objects_added (view, objects);

		for (m = objects; m; m = m->next)
			g_free (m->data);
		g_list_free (objects);
	}

	e_data_cal_view_notify_done (view, NULL /* Success */);
}

gboolean
e_cal_backend_exchange_is_online (ECalBackendExchange *cbex)
{
	if (cbex->priv->mode == CAL_MODE_LOCAL)
		return FALSE;
	else
		return TRUE;
}

static gboolean
is_loaded (ECalBackend *backend)
{
	ECalBackendExchange *cbex;
	ECalBackendExchangePrivate *priv;

	cbex = E_CAL_BACKEND_EXCHANGE (backend);
	priv = cbex->priv;

	return priv->is_loaded;
}

static CalMode
get_mode (ECalBackend *backend)
{
	ECalBackendExchange *cbex;
	ECalBackendExchangePrivate *priv;

	cbex = E_CAL_BACKEND_EXCHANGE (backend);
	priv = cbex->priv;

	d(printf("ecbe_get_mode(%p)\n", backend));

	return priv->mode;
}

static void
set_mode (ECalBackend *backend, CalMode mode)
{
	ECalBackendExchange *cbex;
	ECalBackendExchangePrivate *priv;
	gboolean re_open = FALSE;

	cbex = E_CAL_BACKEND_EXCHANGE (backend);
	priv = cbex->priv;

	d(printf("ecbe_set_mode(%p) : mode : %d\n", backend, mode));

	if (priv->mode == mode) {
		e_cal_backend_notify_mode (
			backend, ModeSet,
			cal_mode_to_corba (mode));
	}

	g_mutex_lock (priv->set_lock);
	if ((priv->mode == CAL_MODE_LOCAL) && (mode == CAL_MODE_REMOTE))
		re_open = TRUE;

	switch (mode) {

	case CAL_MODE_REMOTE:
			e_cal_backend_notify_mode (backend,
				ModeSet,
				Remote);
			/* FIXME : Test if available for read already */
			priv->read_only = FALSE;
			priv->mode = CAL_MODE_REMOTE;

			if (is_loaded (backend) && re_open)
				e_cal_backend_notify_auth_required(backend);
			break;

	case CAL_MODE_LOCAL:
			d(printf ("set mode to offline\n"));
					priv->mode = CAL_MODE_LOCAL;
			priv->read_only = TRUE;
			e_cal_backend_notify_mode (backend,
				ModeSet,
				Local);
			break;

	default :
		e_cal_backend_notify_mode (
			backend, ModeNotSupported,
			cal_mode_to_corba (mode));
	}
	g_mutex_unlock (priv->set_lock);
}

static void
get_freebusy (ECalBackendSync *backend, EDataCal *cal,
	      GList *users, time_t start, time_t end,
	      GList **freebusy, GError **perror)
{
	d(printf("ecbe_get_free_busy(%p, %p)\n", backend, cal));

	g_propagate_error (perror, EDC_ERROR (NotSupported));
}

/**
 * e_cal_backend_exchange_lf_to_crlf:
 * @in: input text in UNIX ("\n") format
 *
 * Return value: text with all LFs converted to CRLF. The caller must
 * free the text.
 **/
gchar *
e_cal_backend_exchange_lf_to_crlf (const gchar *in)
{
	gint len;
	const gchar *s;
	gchar *out, *d;

	g_return_val_if_fail (in != NULL, NULL);

	len = strlen (in);
	for (s = strchr (in, '\n'); s; s = strchr (s + 1, '\n'))
		len++;

	out = g_malloc (len + 1);
	for (s = in, d = out; *s; s++) {
		if (*s == '\n')
			*d++ = '\r';
		*d++ = *s;
	}
	*d = '\0';

	return out;
}

void
e_cal_backend_exchange_get_from (ECalBackendSync *backend, ECalComponent *comp,
			gchar **name, gchar **email)
{
	ECalComponentOrganizer org;

	g_return_if_fail (E_IS_CAL_BACKEND_EXCHANGE (backend));

	e_cal_component_get_organizer (comp, &org);
	if (org.cn) {
		*name = g_strdup (org.cn);
		*email = g_strdup (org.value);
	} else {
		get_cal_owner (backend, name);
		get_cal_address (backend, NULL, email, NULL);
	}
}

void
e_cal_backend_exchange_get_sender (ECalBackendSync *backend, ECalComponent *comp,
			gchar **name, gchar **email)
{
	ECalBackendExchange *cbex;

	g_return_if_fail (E_IS_CAL_BACKEND_EXCHANGE (backend));

	cbex = E_CAL_BACKEND_EXCHANGE (backend);

	*name = g_strdup (exchange_account_get_username (cbex->account));
	*email = g_strdup (exchange_account_get_email_id (cbex->account));
}

/* Do not internationalize */
static const gchar *e2k_rfc822_months[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

/**
 * e_cal_backend_exchange_make_timestamp_rfc822:
 * @when: the %time_t to convert to an RFC822 timestamp
 *
 * Creates an RFC822 Date header value corresponding to @when, in the
 * locale timezone.
 *
 * Return value: the timestamp, which the caller must free.
 **/
gchar *
e_cal_backend_exchange_make_timestamp_rfc822 (time_t when)
{
	struct tm tm;
	gint offset;

	e_localtime_with_offset (when, &tm, &offset);
	offset = (offset / 3600) * 100 + (offset / 60) % 60;

	return g_strdup_printf ("%02d %s %04d %02d:%02d:%02d %+05d",
				tm.tm_mday, e2k_rfc822_months[tm.tm_mon],
				tm.tm_year + 1900,
				tm.tm_hour, tm.tm_min, tm.tm_sec,
				offset);
}

gchar *
e_cal_backend_exchange_get_from_string (ECalBackendSync *backend, ECalComponent *comp)
{
	gchar *name = NULL, *addr = NULL, *from_string = NULL;

	e_cal_backend_exchange_get_from (backend, comp, &name, &addr);
	from_string = g_strdup_printf ("\"%s\" <%s>", name, addr);
	g_free (name);
	g_free (addr);
	return from_string;
}

gchar *
e_cal_backend_exchange_get_sender_string (ECalBackendSync *backend, ECalComponent *comp)
{
	gchar *name = NULL, *addr = NULL, *sender_string = NULL;

	e_cal_backend_exchange_get_sender (backend, comp, &name, &addr);
	sender_string = g_strdup_printf ("\"%s\" <%s>", name, addr);
	g_free (name);
	g_free (addr);
	return sender_string;
}

gchar *
e_cal_backend_exchange_get_owner_email (ECalBackendSync *backend)
{
	ECalBackendExchange *cbex = E_CAL_BACKEND_EXCHANGE (backend);
	ExchangeHierarchy *hier;

	hier = e_folder_exchange_get_hierarchy (cbex->folder);
	return g_strdup (hier->owner_email);
}

gchar *
e_cal_backend_exchange_get_owner_name (ECalBackendSync *backend)
{
	ECalBackendExchange *cbex = E_CAL_BACKEND_EXCHANGE (backend);
	ExchangeHierarchy *hier;

	hier = e_folder_exchange_get_hierarchy (cbex->folder);
	return g_strdup (hier->owner_name);
}

struct ChangeData {
	EXmlHash *ehash;
	GList *adds;
	GList *modifies;
};

static void
check_change_type (gpointer key, gpointer value, gpointer data)
{
	ECalBackendExchangeComponent *ecomp = value;
	struct ChangeData *change_data = data;
	gchar *calobj;
	ECalComponent *comp = NULL;
	gchar *uid = key;
	GList *l = NULL;
	icalcomponent *icomp = NULL;

	/*
	   In case of detached instances with no master object,
	   ecomp->icomp will be NULL and hence should be skipped
	   from matching.
	*/
	if (!ecomp)
		return;
	l = ecomp->instances;
	for (icomp = ecomp->icomp; l; icomp = l->data, l = l->next) {
		if (!icomp)
			continue;

		comp = e_cal_component_new ();
		/*
		  e_cal_component_set_icalcomponent does a icalcomponent_free of
		  previous icalcomponent before setting the new one.
		 */
		e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (icomp));

		calobj = e_cal_component_get_as_string (comp);
		switch (e_xmlhash_compare (change_data->ehash, uid, calobj)) {
		case E_XMLHASH_STATUS_SAME:
			break;
		case E_XMLHASH_STATUS_NOT_FOUND:
			change_data->adds = g_list_prepend (change_data->adds, g_strdup (calobj));
			e_xmlhash_add (change_data->ehash, uid, calobj);
			break;
		case E_XMLHASH_STATUS_DIFFERENT:
			change_data->modifies = g_list_prepend (change_data->modifies, g_strdup (calobj));
			e_xmlhash_add (change_data->ehash, uid, calobj);
		}

		g_free (calobj);
		g_object_unref (comp);
	}
}

struct cbe_data {
	ECalBackendExchange *cbex;
	icalcomponent_kind kind;
	GList *deletes;
	EXmlHash *ehash;
};

static gboolean
e_cal_backend_exchange_compute_changes_foreach_key (const gchar *key, const gchar *value, gpointer data)
{
	struct cbe_data *cbedata = data;
	ECalBackendExchangeComponent *ecomp;
	ecomp = g_hash_table_lookup (cbedata->cbex->priv->objects, key);

	if (ecomp) {
		ECalComponent *comp;
		comp = e_cal_component_new ();
		if (ecomp->icomp)
			e_cal_component_set_icalcomponent (comp,
							   icalcomponent_new_clone (ecomp->icomp));
		if (cbedata->kind == ICAL_VTODO_COMPONENT)
			e_cal_component_set_new_vtype (comp, E_CAL_COMPONENT_TODO);
		else
			e_cal_component_set_new_vtype (comp, E_CAL_COMPONENT_EVENT);
		e_cal_component_set_uid (comp, key);
		cbedata->deletes = g_list_prepend (cbedata->deletes, e_cal_component_get_as_string (comp));
		g_object_unref (comp);
		return TRUE;
	}
	return FALSE;
}

/* Attachments */
static gchar *
save_attach_file (const gchar *dest_file, gchar *file_contents, gint len)
{
	gchar *dest_url = NULL;
	gint fd;

	d(printf ("dest_file is :%s\n", dest_file));

	/* Write it to our local exchange store. */
	fd = g_open (dest_file, O_RDWR | O_CREAT | O_TRUNC | O_BINARY, 0600);
	if (fd < 0) {
		gchar *dir = g_path_get_dirname (dest_file);

		if (dir && (dir[0] != '.' || !dir[1]) &&
		    g_mkdir_with_parents (dir, 0700) >= 0)
			fd = g_open (dest_file, O_RDWR | O_CREAT | O_TRUNC | O_BINARY, 0600);

		g_free (dir);
	}

	if (fd < 0) {
		d(printf ("open of destination file for attachments failed\n"));
		goto end;
	}

	if (camel_write (fd, file_contents, len, NULL) < 0) {
		d(printf ("camel write to attach file failed\n"));
		goto end;
	}
	/* FIXME : Add a ATTACH:CID:someidentifier here */
	dest_url = g_filename_to_uri (dest_file, NULL, NULL);

end :
	close (fd);

	if (!dest_url)
		g_warning ("Failed to save attachment to file '%s', directory does not exist/disk full?", dest_file);

	return dest_url;
}

GSList *
get_attachment (ECalBackendExchange *cbex, const gchar *uid,
			const gchar *body, gint len)
{
	CamelStream *stream;
	CamelMimeMessage *msg;
	CamelDataWrapper *msg_content, *content = NULL;
	CamelMultipart *multipart;
	CamelMimePart *part;
	const gchar *filename = NULL;
	gchar *attach_file_url, *attach_file;
	gint i;
	GSList *list = NULL;
	guchar *attach_data;

	stream = camel_stream_mem_new_with_buffer (body, len);
	msg = camel_mime_message_new ();
	camel_data_wrapper_construct_from_stream (CAMEL_DATA_WRAPPER (msg), stream, NULL);
	g_object_unref (stream);

	msg_content = camel_medium_get_content (CAMEL_MEDIUM (msg));
	if (msg_content && CAMEL_IS_MULTIPART (msg_content)) {
		multipart = (CamelMultipart *)msg_content;

		for (i = 0; i < (gint)camel_multipart_get_number (multipart); i++) {
			part = camel_multipart_get_part (multipart, i);
			filename = camel_mime_part_get_filename (part);
			if (filename) {
				GByteArray *byte_array;

				content = camel_medium_get_content (CAMEL_MEDIUM (part));

				byte_array = g_byte_array_new ();
				stream = camel_stream_mem_new_with_byte_array (byte_array);
				camel_data_wrapper_decode_to_stream (content, stream, NULL);
				attach_data = g_memdup (byte_array->data, byte_array->len);
				attach_file = g_strdup_printf ("%s/%s-%s", cbex->priv->local_attachment_store, uid, filename);
				// Attach
				attach_file_url = save_attach_file (attach_file, (gchar *) attach_data, byte_array->len);
				g_free (attach_data);
				g_free (attach_file);

				if (attach_file_url) {
					d(printf ("attach file name : %s\n", attach_file_url));
					list = g_slist_append (list, attach_file_url);
				}

				g_object_unref (stream);
			}
		} /* Loop through each multipart */
	}

	g_object_unref (msg);
	return list;
}

static gchar *
get_attach_file_contents (const gchar *filename, gint *length)
{
	gint fd, len = 0;
	struct stat sb;
	gchar *file_contents = NULL;

	fd = g_open (filename, O_RDONLY | O_BINARY, 0);
	if (fd < 0) {
		d(printf ("Could not open the attachment file : %s\n", filename));
		goto end;
	}
	if (fstat (fd, &sb) < 0) {
		d(printf ("fstat of attachment file failed\n"));
		goto end;
	}
	len = sb.st_size;

	if (len > 0) {
		file_contents = g_malloc0 (len + 1);

		if (camel_read (fd, file_contents, len, NULL) < 0) {
			d(printf ("reading from the attachment file failed\n"));
			g_free (file_contents);
			file_contents = NULL;
			goto end;
		}
		file_contents[len] = '\0';
	}

end :
	close (fd);
	*length = len;
	return file_contents;
}

void
process_delegated_cal_object (icalcomponent *icalcomp, const gchar *delegator_name, const gchar *delegator_email, const gchar *delegatee_email)
{
	icalproperty *prop = NULL;

	prop = icalcomponent_get_first_property (icalcomp, ICAL_ORGANIZER_PROPERTY);
	if (prop) {
		const gchar *organizer;
		gchar *text = NULL;

		organizer = icalproperty_get_value_as_string_r (prop);
		if (organizer) {
			if (!g_ascii_strncasecmp (organizer, "mailto:", 7))
				text = g_strdup (organizer+7);

			text = g_strstrip (text);
			if (text && (!g_ascii_strcasecmp (delegatee_email, text) || !g_ascii_strcasecmp (delegator_email, text))) {
				icalproperty_set_organizer (prop, g_strdup_printf ("MAILTO:%s", delegator_email));
				icalproperty_remove_parameter_by_kind (prop, ICAL_CN_PARAMETER);
				icalproperty_add_parameter (prop, icalparameter_new_cn (g_strdup(delegator_name)));
				icalproperty_remove_parameter_by_kind (prop, ICAL_SENTBY_PARAMETER);
				icalproperty_add_parameter (prop, icalparameter_new_sentby (g_strdup_printf("MAILTO:%s", delegatee_email)));
			}
		}
		if (text)
			g_free (text);
	}
	prop = NULL;

	for (prop = icalcomponent_get_first_property (icalcomp, ICAL_ATTENDEE_PROPERTY);
	     prop != NULL;
	     prop = icalcomponent_get_next_property (icalcomp, ICAL_ATTENDEE_PROPERTY)) {
		const gchar *attendee;
		gchar *text = NULL;

		attendee = icalproperty_get_value_as_string_r (prop);
		if (!attendee)
			continue;

		if (!g_ascii_strncasecmp (attendee, "mailto:", 7))
			text = g_strdup (attendee+7);

		text = g_strstrip (text);
		if (text && !g_ascii_strcasecmp (delegator_email, text)) {
			icalproperty_remove_parameter_by_kind (prop, ICAL_CN_PARAMETER);
			icalproperty_add_parameter (prop, icalparameter_new_cn (g_strdup(delegator_name)));
			icalproperty_remove_parameter_by_kind (prop, ICAL_SENTBY_PARAMETER);
			icalproperty_add_parameter (prop, icalparameter_new_sentby (g_strdup_printf("MAILTO:%s", delegatee_email)));
			g_free (text);
			break;
		}
		g_free (text);
	}
}

static gchar *
get_mime_type (const gchar *uri)
{
	GFile *file;
	GFileInfo *fi;
	gchar *mime_type;

	g_return_val_if_fail (uri != NULL, NULL);

	file = g_file_new_for_uri (uri);
	if (!file)
		return NULL;

	fi = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE, G_FILE_QUERY_INFO_NONE, NULL, NULL);
	if (!fi) {
		g_object_unref (file);
		return NULL;
	}

	mime_type = g_content_type_get_mime_type (g_file_info_get_content_type (fi));

	g_object_unref (fi);
	g_object_unref (file);

	return mime_type;
}

/* copies attachments to the backend's attachment store and returns new list of attachment URLs */
GSList *
receive_attachments (ECalBackendExchange *cbex, ECalComponent *comp)
{
	GSList *attach_urls = NULL;
	GSList *l, *attach_list = NULL;
	const gchar *uid = NULL;

	g_return_val_if_fail (cbex != NULL, NULL);
	g_return_val_if_fail (comp != NULL, NULL);

	if (!e_cal_component_has_attachments (comp))
		return NULL;

	e_cal_component_get_uid (comp, &uid);
	g_return_val_if_fail (uid != NULL, NULL);

	e_cal_component_get_attachment_list (comp, &attach_list);
	for (l = attach_list; l; l = l->next) {
		const gchar *fname;
		gchar *dest_url, *attach_file = NULL, *file_contents, *old_file = NULL;
		gint len = 0;

		if (!strncmp ((gchar *)l->data, "file://", 7)) {
			attach_file = g_filename_from_uri ((gchar *)l->data, NULL, NULL);
			fname = attach_file;

			if (fname && cbex->priv->local_attachment_store && !g_str_has_prefix (fname, cbex->priv->local_attachment_store)) {
				/* it's not in our store, thus save it there */
				gchar *base_name = g_path_get_basename (attach_file);

				old_file = attach_file;
				attach_file = g_build_filename (cbex->priv->local_attachment_store, uid, base_name, NULL);
				g_free (base_name);
			}
		} else {
			const gchar *filename;

			fname = (gchar *)(l->data);
			filename = g_strrstr (fname, "/");
			if (!filename) {
				/*
				 * some vcards contain e.g. ATTACH:CID:0fd601c67efb$ef66760d$_CDOEX
				 * -> ignore instead of crashing
				 */
				continue;
			}
			attach_file = g_strdup_printf ("%s/%s-%s", cbex->priv->local_attachment_store, uid, filename + 1);
		}

		/* attach_file should be g_freed */
		file_contents = get_attach_file_contents (fname, &len);
		g_free (old_file);
		if (!file_contents) {
			g_free (attach_file);
			continue;
		}

		dest_url = save_attach_file (attach_file, file_contents, len);
		g_free (attach_file);
		g_free (file_contents);
		if (!dest_url) {
			continue;
		}

		attach_urls = g_slist_append (attach_urls, dest_url);
	}

	return attach_urls;
}

gchar *
build_msg ( ECalBackendExchange *cbex, ECalComponent *comp, const gchar *subject, gchar **boundary)
{
	CamelMimeMessage *msg;
	CamelMimePart *mime_part;
	CamelDataWrapper *dw, *wrapper;
	CamelMultipart *multipart;
	CamelInternetAddress *from;
	CamelStream *stream;
	CamelContentType *type;
	GByteArray *byte_array;
	const gchar *uid;
	gchar *buffer = NULL, *cid;
	gchar *from_name = NULL, *from_email = NULL;
	GSList *attach_list = NULL, *l, *new_attach_list = NULL;
	gchar *fname, *file_contents = NULL, *filename, *dest_url, *mime_filename, *attach_file;
	gint len = 0;

	if (!g_ascii_strcasecmp(e_cal_backend_exchange_get_owner_email (E_CAL_BACKEND_SYNC (cbex)), exchange_account_get_email_id (cbex->account)))
		e_cal_backend_exchange_get_from (E_CAL_BACKEND_SYNC (cbex), comp, &from_name, &from_email);
	else
		e_cal_backend_exchange_get_sender (E_CAL_BACKEND_SYNC (cbex), comp, &from_name, &from_email);

	msg = camel_mime_message_new ();

	multipart = camel_multipart_new ();

	/* Headers */
	camel_mime_message_set_subject (msg, subject);

	from = camel_internet_address_new ();
	camel_internet_address_add (from, from_name, from_email);
	camel_mime_message_set_from (msg, from);
	g_free (from_name);
	g_free (from_email);
	g_object_unref (from);

	e_cal_component_get_uid (comp, &uid);
	e_cal_component_get_attachment_list (comp, &attach_list);
	for (l = attach_list; l; l = l->next) {
		gchar *mime_type;

		if (!strncmp ((gchar *)l->data, "file://", 7)) {
			fname = g_filename_from_uri ((gchar *)l->data, NULL, NULL);
			filename = g_path_get_basename (fname);
			mime_filename = g_strdup (filename + strlen(uid) + 1);
			g_free (filename);
			attach_file = fname;
		} else {
			fname = (gchar *)(l->data);
			filename = g_strrstr (fname, "/");
			if (!filename) {
				/*
				 * some vcards contain e.g. ATTACH:CID:0fd601c67efb$ef66760d$_CDOEX
				 * -> ignore instead of crashing
				 */
				continue;
			}
			mime_filename = g_strdup (filename+1);
			attach_file = g_strdup_printf ("%s/%s-%s", cbex->priv->local_attachment_store, uid, filename);
		}

		/* mime_filename and attach_file should be g_freed */

		file_contents = get_attach_file_contents (fname, &len);
		if (!file_contents) {
			g_free (attach_file);
			g_free (mime_filename);
			continue;
		}

		dest_url = save_attach_file (attach_file, file_contents, len);
		g_free (attach_file);
		if (!dest_url) {
			g_free (mime_filename);
			continue;
		}
		new_attach_list = g_slist_append (new_attach_list, dest_url);

		/* Content */
		stream = camel_stream_mem_new_with_buffer (file_contents, len);
		wrapper = camel_data_wrapper_new ();
		camel_data_wrapper_construct_from_stream (wrapper, stream, NULL);
		g_object_unref (stream);

		mime_type = get_mime_type (dest_url);
		if (mime_type) {
			type = camel_content_type_decode (mime_type);
			camel_data_wrapper_set_mime_type_field (wrapper, type);
			camel_content_type_unref (type);
			g_free (mime_type);
		}

		mime_part = camel_mime_part_new ();

		camel_medium_set_content (CAMEL_MEDIUM (mime_part), wrapper);
		camel_mime_part_set_filename (mime_part, mime_filename);
		camel_mime_part_set_encoding (mime_part, CAMEL_TRANSFER_ENCODING_BASE64);
		cid = camel_header_msgid_generate ();
		camel_mime_part_set_content_id (mime_part, cid);
		camel_mime_part_set_description (mime_part, mime_filename);
		g_free (mime_filename);
		camel_mime_part_set_disposition (mime_part, "attachment");
		camel_multipart_set_boundary (multipart, NULL);
		*boundary = g_strdup (camel_multipart_get_boundary (multipart));
		camel_multipart_add_part (multipart, mime_part);
		g_object_unref (mime_part);
		g_free (cid);
	}
	if (!new_attach_list) {
		g_object_unref (multipart);
		g_object_unref (msg);
		return NULL;
	}
	e_cal_component_set_attachment_list (comp, new_attach_list);

	camel_medium_set_content (CAMEL_MEDIUM (msg), CAMEL_DATA_WRAPPER (multipart));
	g_object_unref (multipart);

	byte_array = g_byte_array_new ();
	stream = camel_stream_mem_new_with_byte_array (byte_array);
	dw = camel_medium_get_content (CAMEL_MEDIUM (msg));
	camel_data_wrapper_decode_to_stream (dw, stream, NULL);
	g_byte_array_append (byte_array, (guint8 *) "", 1);
	buffer = g_memdup (byte_array->data, byte_array->len);
	d(printf ("|| Buffer: \n%s\n", buffer));
	g_object_unref (stream);
	g_object_unref (msg);

	return buffer;
}

static void
get_changes (ECalBackendSync *backend, EDataCal *cal,
	     const gchar *change_id,
	     GList **adds, GList **modifies, GList **deletes, GError **error)
{
	ECalBackendExchange *cbex = E_CAL_BACKEND_EXCHANGE (backend);
	gchar *path, *filename;
	EXmlHash *ehash;
	struct ChangeData data;
	struct cbe_data cbedata;

	d(printf("ecbe_get_changes(%p, %p, %s)\n", backend, cal, change_id));

	e_return_data_cal_error_if_fail (E_IS_CAL_BACKEND_EXCHANGE (cbex), InvalidArg);
	e_return_data_cal_error_if_fail (change_id != NULL, ObjectNotFound);

	/* open the changes file */
	filename = g_strdup_printf ("%s.changes", change_id);
	path = e_folder_exchange_get_storage_file (cbex->folder, filename);
	ehash = e_xmlhash_new (path);
	g_free (path);
	g_free (filename);

	/*calculate add/mod*/
	data.ehash = ehash;
	data.adds = NULL;
	data.modifies = NULL;
	g_hash_table_foreach (cbex->priv->objects, check_change_type, &data);

	*adds = data.adds;
	*modifies = data.modifies;
	ehash = data.ehash;

	/*deletes*/
	cbedata.cbex = cbex;
	cbedata.kind = e_cal_backend_get_kind (E_CAL_BACKEND (cbex));
	cbedata.deletes = NULL;
	cbedata.ehash = ehash;
	e_xmlhash_foreach_key_remove (ehash, (EXmlHashRemoveFunc)e_cal_backend_exchange_compute_changes_foreach_key, &cbedata);

	*deletes = cbedata.deletes;

	e_xmlhash_write (ehash);
	e_xmlhash_destroy (ehash);
}

static icaltimezone *
internal_get_default_timezone (ECalBackend *backend)
{
	ECalBackendExchange *cbex = E_CAL_BACKEND_EXCHANGE (backend);

	/* FIXME : This should never happen. Sometimes gets triggered while moving
	between online and offline. */
	if (!cbex->account)
		return NULL;

	if (!cbex->priv->default_timezone &&
	    cbex->account->default_timezone) {
		cbex->priv->default_timezone =
			g_hash_table_lookup (cbex->priv->timezones,
					     cbex->account->default_timezone);
	}

	return cbex->priv->default_timezone;
}

static icaltimezone *
internal_get_timezone (ECalBackend *backend, const gchar *tzid)
{
	ECalBackendExchange *cbex = E_CAL_BACKEND_EXCHANGE (backend);
	icaltimezone *zone;

	g_return_val_if_fail (cbex != NULL, NULL);
	g_return_val_if_fail (tzid != NULL, NULL);

	/* search in backend's time zone cache */
	zone = g_hash_table_lookup (cbex->priv->timezones, tzid);

	/* if not found, chain up */
	if (!zone && E_CAL_BACKEND_CLASS (parent_class)->internal_get_timezone)
		zone = E_CAL_BACKEND_CLASS (parent_class)->internal_get_timezone (backend, tzid);

	return zone;
}

icaltimezone *
e_cal_backend_exchange_lookup_timezone (const gchar *tzid,
					gconstpointer custom,
					GError **error)
{
	icaltimezone *zone = internal_get_timezone (E_CAL_BACKEND ((ECalBackendExchange *)custom), tzid);

	/* The UTC timezone is a fallback, which is not supposed
	   to be returned in call of e_cal_check_timezones, thus skip it */
	if (zone && zone == icaltimezone_get_utc_timezone ())
		zone = NULL;

	return zone;
}

static void
free_exchange_comp (gpointer value)
{
	ECalBackendExchangeComponent *ecomp = value;
	GList *inst;

	g_free (ecomp->uid);
	g_free (ecomp->href);
	g_free (ecomp->lastmod);

	if (ecomp->icomp)
		icalcomponent_free (ecomp->icomp);

	for (inst = ecomp->instances; inst; inst = inst->next)
		icalcomponent_free (inst->data);
	g_list_free (ecomp->instances);

	g_free (ecomp);
}

static void
init (ECalBackendExchange *cbex)
{
	cbex->priv = g_new0 (ECalBackendExchangePrivate, 1);

	cbex->priv->objects = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		NULL, free_exchange_comp);

	cbex->priv->timezones = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		g_free, (GDestroyNotify)icaltimezone_free);

	cbex->priv->set_lock = g_mutex_new ();
	cbex->priv->open_lock = g_mutex_new ();
	cbex->priv->cache_lock = g_mutex_new ();
	cbex->priv->cache_unseen = NULL;

	e_cal_backend_sync_set_lock (E_CAL_BACKEND_SYNC (cbex), TRUE);
}

static void
dispose (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
finalize (GObject *object)
{
	ECalBackendExchange *cbex = E_CAL_BACKEND_EXCHANGE (object);

	if (cbex->priv->save_timeout_id) {
		g_source_remove (cbex->priv->save_timeout_id);
		timeout_save_cache (cbex);
	}

	g_hash_table_destroy (cbex->priv->objects);
	if (cbex->priv->cache_unseen)
		g_hash_table_destroy (cbex->priv->cache_unseen);
	g_free (cbex->priv->object_cache_file);
	g_free (cbex->priv->lastmod);
	g_free (cbex->priv->local_attachment_store);

	g_hash_table_destroy (cbex->priv->timezones);

	if (cbex->priv->set_lock) {
		g_mutex_free (cbex->priv->set_lock);
		cbex->priv->set_lock = NULL;
	}

	if (cbex->priv->open_lock) {
		g_mutex_free (cbex->priv->open_lock);
		cbex->priv->open_lock = NULL;
	}

	if (cbex->priv->cache_lock) {
		g_mutex_free (cbex->priv->cache_lock);
		cbex->priv->cache_lock = NULL;
	}
	g_free (cbex->priv);

	if (G_OBJECT_CLASS (parent_class)->finalize)
		G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
class_init (ECalBackendExchangeClass *klass)
{
	GObjectClass *object_class;
	ECalBackendClass *backend_class = E_CAL_BACKEND_CLASS (klass);
	ECalBackendSyncClass *sync_class = E_CAL_BACKEND_SYNC_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class = (GObjectClass *) klass;

	sync_class->is_read_only_sync = is_read_only;
	sync_class->get_cal_address_sync = get_cal_address;
	sync_class->get_alarm_email_address_sync = get_alarm_email_address;
	sync_class->get_ldap_attribute_sync = get_ldap_attribute;
	sync_class->get_static_capabilities_sync = get_static_capabilities;
	sync_class->open_sync = open_calendar;
	sync_class->remove_sync = remove_calendar;
	sync_class->discard_alarm_sync = discard_alarm;
	sync_class->send_objects_sync = send_objects;
	sync_class->get_default_object_sync = get_default_object;
	sync_class->get_object_sync = get_object;
	sync_class->get_object_list_sync = get_object_list;
	sync_class->add_timezone_sync = add_timezone;
	sync_class->set_default_zone_sync = set_default_zone;
	sync_class->get_freebusy_sync = get_freebusy;
	sync_class->get_changes_sync = get_changes;
	sync_class->create_object_sync = create_object;
	sync_class->modify_object_sync = modify_object;

	backend_class->start_query = start_query;
	backend_class->get_mode = get_mode;
	backend_class->set_mode = set_mode;
	backend_class->is_loaded = is_loaded;
	backend_class->internal_get_default_timezone = internal_get_default_timezone;
	backend_class->internal_get_timezone = internal_get_timezone;

	object_class->dispose = dispose;
	object_class->finalize = finalize;
}

E2K_MAKE_TYPE (e_cal_backend_exchange, ECalBackendExchange, class_init, init, PARENT_TYPE)
