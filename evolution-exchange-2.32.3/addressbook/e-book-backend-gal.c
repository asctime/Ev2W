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

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "e-book-backend-gal"
#undef DEBUG

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <glib/gprintf.h>

#include <e2k-utils.h>
#include <e2k-global-catalog-ldap.h>
#include <exchange-account.h>

#define d(x)

#include <sys/time.h>
#include <time.h>
#include <libedataserver/e-sexp.h>
#include <libebackend/e-db3-utils.h>
#include <libedataserver/e-data-server-util.h>
#include <libebook/e-contact.h>

#include <libedata-book/e-book-backend-sexp.h>
#include <libedata-book/e-data-book.h>
#include <libedata-book/e-data-book-view.h>
#include "libedata-book/e-book-backend-summary.h"
#include "tools/exchange-share-config-listener.h"
#include "e-book-backend-gal.h"
#include <libical/ical.h>

#ifdef _WIN32
#include <winber.h>
#endif

#ifndef LDAP_CONTROL_PAGEDRESULTS
#ifdef ENABLE_CACHE
#undef ENABLE_CACHE
#endif
#define ENABLE_CACHE 0
#endif

#if defined(ENABLE_CACHE) && ENABLE_CACHE
#include  "e-book-backend-db-cache.h"
#include "db.h"
#endif

/* interval for our poll_ldap timeout */
#define LDAP_POLL_INTERVAL 20

/* timeout for ldap_result */
#define LDAP_RESULT_TIMEOUT_MILLIS 10

#define TV_TO_MILLIS(timeval) ((timeval).tv_sec * 1000 + (timeval).tv_usec / 1000)

static const gchar *query_prop_to_ldap (const gchar *query_prop);
static void build_query (EBookBackendGAL *bl, const gchar *query, const gchar *ldap_filter, gchar **ldap_query, GError **perror);

#define EDB_ERROR(_code) e_data_book_create_error (E_DATA_BOOK_STATUS_ ## _code, NULL)
#define EDB_ERROR_EX(_code, _msg) e_data_book_create_error (E_DATA_BOOK_STATUS_ ## _code, _msg)
#define EDB_ERROR_MSG_TYPE(_msg_type) e_data_book_create_error_fmt (E_DATA_BOOK_STATUS_INVALID_ARG, "Incorrect msg type %d passed to %s", _msg_type, G_STRFUNC)

#define PARENT_TYPE E_TYPE_BOOK_BACKEND
static EBookBackendClass *parent_class;

typedef struct LDAPOp LDAPOp;

static GList *supported_fields;
static const gchar **search_attrs;

struct _EBookBackendGALPrivate {
	gchar             *gal_uri;
	gboolean          connected;

	E2kGlobalCatalog *gc;
	LDAP             *ldap;
	ExchangeAccount  *account;

	gboolean marked_for_offline;
	GMutex		*ldap_lock;

	/* our operations */
	GStaticRecMutex op_hash_mutex;
	GHashTable *id_to_op;
	gint active_ops;
	gint mode;
	gint poll_timeout;
#if defined(ENABLE_CACHE) && ENABLE_CACHE
	DB *file_db;
	DB_ENV *env;
	time_t last_best_time;
	time_t cache_time;
#endif
	/* Summary */
	gchar *summary_file_name;
	gboolean is_summary_ready;
	EBookBackendSummary *summary;
};

#define SUMMARY_FLUSH_TIMEOUT 5000
#if defined(ENABLE_CACHE) && ENABLE_CACHE
static GStaticMutex global_env_lock = G_STATIC_MUTEX_INIT;
static struct {
	gint ref_count;
	DB_ENV *env;
} global_env;
#endif

typedef void (*LDAPOpHandler)(LDAPOp *op, LDAPMessage *res);
typedef void (*LDAPOpDtor)(LDAPOp *op);

struct LDAPOp {
	LDAPOpHandler  handler;
	LDAPOpDtor     dtor;
	EBookBackend  *backend;
	EDataBook     *book;
	EDataBookView *view;
	guint32        opid; /* the libebook operation id */
	gint            id;   /* the ldap msg id */
};

static void     ldap_op_add (LDAPOp *op, EBookBackend *backend, EDataBook *book,
			     EDataBookView *view, gint opid, gint msgid, LDAPOpHandler handler, LDAPOpDtor dtor);
static void     ldap_op_finished (LDAPOp *op);

static gboolean poll_ldap (EBookBackendGAL *bl);

static EContact *build_contact_from_entry (EBookBackendGAL *bl, LDAPMessage *e, GList **existing_objectclasses);

static void manager_populate (EContact *contact, gchar **values, EBookBackendGAL *bl, E2kOperation *op);

static void member_populate (EContact *contact, gchar **values, EBookBackendGAL *bl, E2kOperation *op);

static void last_mod_time_populate (EContact *contact, gchar **values, EBookBackendGAL *bl, E2kOperation *op);

struct prop_info {
	EContactField field_id;
	const gchar *ldap_attr;
#define PROP_TYPE_STRING   0x01
#define PROP_TYPE_COMPLEX  0x02
#define PROP_TYPE_GROUP    0x04
	gint prop_type;

	/* the remaining items are only used for the TYPE_COMPLEX props */

	/* used when reading from the ldap server populates EContact with the values in **values. */
	void (*populate_contact_func)(EContact *contact, gchar **values, EBookBackendGAL *bl, E2kOperation *op);

} prop_info[] = {

#define COMPLEX_PROP(fid,a,ctor) {fid, a, PROP_TYPE_COMPLEX, ctor}
#define STRING_PROP(fid,a) {fid, a, PROP_TYPE_STRING}
#define GROUP_PROP(fid,a,ctor) {fid, a, PROP_TYPE_GROUP, ctor}

	/* name fields */
	STRING_PROP   (E_CONTACT_FULL_NAME,   "displayName" ),
	STRING_PROP   (E_CONTACT_GIVEN_NAME,  "givenName" ),
	STRING_PROP   (E_CONTACT_FAMILY_NAME, "sn" ),
	STRING_PROP   (E_CONTACT_NICKNAME,    "mailNickname" ),

	/* email addresses */
	STRING_PROP   (E_CONTACT_EMAIL_1,     "mail" ),
	GROUP_PROP    (E_CONTACT_EMAIL,       "member", member_populate),

	/* phone numbers */
	STRING_PROP   (E_CONTACT_PHONE_BUSINESS,     "telephoneNumber"),
	STRING_PROP   (E_CONTACT_PHONE_BUSINESS_2,   "otherTelephone"),
	STRING_PROP   (E_CONTACT_PHONE_HOME,         "homePhone"),
	STRING_PROP   (E_CONTACT_PHONE_HOME_2,       "otherHomePhone"),
	STRING_PROP   (E_CONTACT_PHONE_MOBILE,       "mobile"),
	STRING_PROP   (E_CONTACT_PHONE_BUSINESS_FAX, "facsimileTelephoneNumber"),
	STRING_PROP   (E_CONTACT_PHONE_OTHER_FAX,    "otherFacsimileTelephoneNumber"),
	STRING_PROP   (E_CONTACT_PHONE_PAGER,        "pager"),

	/* org information */
	STRING_PROP   (E_CONTACT_ORG,                "company"),
	STRING_PROP   (E_CONTACT_ORG_UNIT,           "department"),
	STRING_PROP   (E_CONTACT_OFFICE,             "physicalDeliveryOfficeName"),
	STRING_PROP   (E_CONTACT_TITLE,              "title"),

	COMPLEX_PROP  (E_CONTACT_MANAGER,            "manager", manager_populate),

	/* FIXME: we should aggregate streetAddress, l, st, c, postalCode
	 * into business_address
	 */

	/* misc fields */
	STRING_PROP   (E_CONTACT_HOMEPAGE_URL,       "wWWHomePage"),
	STRING_PROP   (E_CONTACT_FREEBUSY_URL,       "msExchFBURL"),
	STRING_PROP   (E_CONTACT_NOTE,               "info"),
	STRING_PROP   (E_CONTACT_FILE_AS,            "fileAs"),

	/* whenChanged is a string value, but since we need to re-format it,
	 * defining it as a complex property
	 */
	COMPLEX_PROP   (E_CONTACT_REV,                "whenChanged", last_mod_time_populate),

#undef STRING_PROP
#undef COMPLEX_PROP
#undef GROUP_PROP
};

static gboolean
can_browse (EBookBackend *backend)
{
	return backend &&
		e_book_backend_get_source (backend) &&
		e_source_get_property (e_book_backend_get_source (backend), "can-browse") &&
		strcmp (e_source_get_property (e_book_backend_get_source (backend), "can-browse"), "1") == 0;
}

static gboolean
can_expand_groups (EBookBackend *backend)
{
	return backend &&
		e_book_backend_get_source (backend) &&
		e_source_get_property (e_book_backend_get_source (backend), "expand-groups") &&
		strcmp (e_source_get_property (e_book_backend_get_source (backend), "expand-groups"), "1") == 0;
}

static void
book_view_notify_status (EDataBookView *view, const gchar *status)
{
	if (!view)
		return;
	e_data_book_view_notify_status_message (view, status);
}

static EDataBookView*
find_book_view (EBookBackendGAL *bl)
{
	EList *views = e_book_backend_get_book_views (E_BOOK_BACKEND (bl));
	EIterator *iter = e_list_get_iterator (views);
	EDataBookView *rv = NULL;

	if (e_iterator_is_valid (iter)) {
		/* just always use the first book view */
		EDataBookView *v = (EDataBookView*)e_iterator_get(iter);
		if (v)
			rv = v;
	}

	g_object_unref (iter);
	g_object_unref (views);

	return rv;
}

static gboolean
gal_connect (EBookBackendGAL *bl, GError **perror)
{
	EBookBackendGALPrivate *blpriv = bl->priv;
	gint ldap_error = 0;

#ifdef DEBUG
	{
		gint debug_level = 1;
		ldap_set_option (NULL, LDAP_OPT_DEBUG_LEVEL, &debug_level);
	}
#endif

	blpriv->gc = NULL;
	blpriv->connected = FALSE;

	blpriv->account = exchange_share_config_listener_get_account_for_uri (NULL, blpriv->gal_uri);
	if (!blpriv->account) {
		g_propagate_error (perror, EDB_ERROR (REPOSITORY_OFFLINE));
		return FALSE;
	}
	blpriv->gc = exchange_account_get_global_catalog (blpriv->account);
	if (!blpriv->gc) {
		g_propagate_error (perror, EDB_ERROR (REPOSITORY_OFFLINE));
		return FALSE;
	}

	g_object_ref (blpriv->gc);
	g_mutex_lock (blpriv->ldap_lock);
	blpriv->ldap = e2k_global_catalog_get_ldap (blpriv->gc, NULL, &ldap_error);
	if (!blpriv->ldap) {
		g_mutex_unlock (blpriv->ldap_lock);
		d(printf ("%s: Cannot get ldap, error 0x%x (%s)\n", G_STRFUNC, ldap_error, ldap_err2string (ldap_error) ? ldap_err2string (ldap_error) : "Unknown error"));
		if (ldap_error == LDAP_AUTH_METHOD_NOT_SUPPORTED) {
			g_propagate_error (perror, EDB_ERROR (UNSUPPORTED_AUTHENTICATION_METHOD));
		} else {
			g_propagate_error (perror, e_data_book_create_error_fmt (E_DATA_BOOK_STATUS_REPOSITORY_OFFLINE, "Cannot get ldap, error 0x%x (%s)", ldap_error, ldap_err2string (ldap_error) ? ldap_err2string (ldap_error) : "Unknown error"));
		}

		return FALSE;
	}
	g_mutex_unlock (blpriv->ldap_lock);

	blpriv->connected = TRUE;
	e_book_backend_set_is_loaded (E_BOOK_BACKEND (bl), TRUE);
	return TRUE;
}

static gboolean
ldap_reconnect (EBookBackendGAL *bl, EDataBookView *book_view, LDAP **ldap, gint status) {

	if (!ldap || !*ldap)
		return FALSE;

	if (status == LDAP_SERVER_DOWN) {

		if (book_view)
			book_view_notify_status (book_view, _("Reconnecting to LDAP server..."));

		ldap_unbind (*ldap);
		*ldap = e2k_global_catalog_get_ldap (bl->priv->gc, NULL, NULL);
		if (book_view)
			book_view_notify_status (book_view, "");

		if (*ldap)
			return TRUE;
	}

	return FALSE;
}

static gboolean
gal_reconnect (EBookBackendGAL *bl, EDataBookView *book_view, gint ldap_status)
{
	/* we need to reconnect if we were previously connected */
	g_mutex_lock (bl->priv->ldap_lock);
	if ((bl->priv->connected && ldap_status == LDAP_SERVER_DOWN) || (!bl->priv->ldap && !bl->priv->connected)) {
		if (book_view)
			book_view_notify_status (book_view, _("Reconnecting to LDAP server..."));
		if (bl->priv->ldap)
			ldap_unbind (bl->priv->ldap);
		bl->priv->ldap = e2k_global_catalog_get_ldap (bl->priv->gc, NULL, NULL);
		if (book_view)
			book_view_notify_status (book_view, "");

		if (bl->priv->ldap != NULL) {
			bl->priv->connected = TRUE;
			g_mutex_unlock (bl->priv->ldap_lock);
			return TRUE;
		} else {
			g_mutex_unlock (bl->priv->ldap_lock);
			return FALSE;
		}
	}
	else {
		g_mutex_unlock (bl->priv->ldap_lock);
		d(printf("Connected and ldap is null sigh\n"));
		return FALSE;
	}
}

static void
ldap_op_add (LDAPOp *op, EBookBackend *backend,
	     EDataBook *book, EDataBookView *view,
	     gint opid,
	     gint msgid,
	     LDAPOpHandler handler, LDAPOpDtor dtor)
{
	EBookBackendGAL *bl = E_BOOK_BACKEND_GAL (backend);

	op->backend = backend;
	op->book = book;
	op->view = view;
	op->opid = opid;
	op->id = msgid;
	op->handler = handler;
	op->dtor = dtor;

	g_static_rec_mutex_lock (&bl->priv->op_hash_mutex);
	if (g_hash_table_lookup (bl->priv->id_to_op, &op->id)) {
		g_warning ("conflicting ldap msgid's");
	}

	g_hash_table_insert (bl->priv->id_to_op,
			     &op->id, op);

	bl->priv->active_ops++;

	if (bl->priv->poll_timeout == -1)
		bl->priv->poll_timeout = g_timeout_add (LDAP_POLL_INTERVAL,
							(GSourceFunc) poll_ldap,
							bl);

	g_static_rec_mutex_unlock (&bl->priv->op_hash_mutex);
}

static void
ldap_op_finished (LDAPOp *op)
{
	EBookBackend *backend = op->backend;
	EBookBackendGAL *bl = E_BOOK_BACKEND_GAL (backend);

	g_static_rec_mutex_lock (&bl->priv->op_hash_mutex);
	g_hash_table_remove (bl->priv->id_to_op, &op->id);

	/* should handle errors here */
	g_mutex_lock (bl->priv->ldap_lock);
	if (bl->priv->ldap)
		ldap_abandon (bl->priv->ldap, op->id);
	g_mutex_unlock (bl->priv->ldap_lock);

	op->dtor (op);

	bl->priv->active_ops--;

	if (bl->priv->active_ops == 0) {
		if (bl->priv->poll_timeout != -1)
			g_source_remove (bl->priv->poll_timeout);
		bl->priv->poll_timeout = -1;
	}
	g_static_rec_mutex_unlock (&bl->priv->op_hash_mutex);
}

static GError *
ldap_error_to_response (gint ldap_error)
{
	if (ldap_error == LDAP_SUCCESS)
		return NULL;
	else if (LDAP_NAME_ERROR (ldap_error))
		return EDB_ERROR (CONTACT_NOT_FOUND);
	else if (ldap_error == LDAP_INSUFFICIENT_ACCESS)
		return EDB_ERROR (PERMISSION_DENIED);
	else if (ldap_error == LDAP_SERVER_DOWN)
		return EDB_ERROR (REPOSITORY_OFFLINE);
	else if (ldap_error == LDAP_ALREADY_EXISTS)
		return EDB_ERROR (CONTACTID_ALREADY_EXISTS);

	return e_data_book_create_error_fmt (E_DATA_BOOK_STATUS_OTHER_ERROR, "Failed with ldap error 0x%x (%s)", ldap_error, ldap_err2string (ldap_error) ? ldap_err2string (ldap_error) : "Unknown error");
}



static void
create_contact (EBookBackend *backend,
		EDataBook    *book,
		guint32       opid,
		const gchar   *vcard)
{
	e_data_book_respond_create (book, opid,
				    EDB_ERROR (PERMISSION_DENIED),
				    NULL);
}

static void
remove_contacts (EBookBackend *backend,
		 EDataBook    *book,
		 guint32       opid,
		 GList        *ids)
{
	e_data_book_respond_remove_contacts (book, opid,
					     EDB_ERROR (PERMISSION_DENIED),
					     NULL);
}

static void
modify_contact (EBookBackend *backend,
		EDataBook    *book,
		guint32       opid,
		const gchar   *vcard)
{
	e_data_book_respond_modify (book, opid,
				    EDB_ERROR (PERMISSION_DENIED),
				    NULL);
}

typedef struct {
	LDAPOp op;
} LDAPGetContactOp;

static void
get_contact_handler (LDAPOp *op, LDAPMessage *res)
{
	EBookBackendGAL *bl = E_BOOK_BACKEND_GAL (op->backend);
	gint msg_type;

	g_mutex_lock (bl->priv->ldap_lock);
	if (!bl->priv->ldap) {
		g_mutex_unlock (bl->priv->ldap_lock);
		e_data_book_respond_get_contact (op->book, op->opid,
					EDB_ERROR_EX (OTHER_ERROR, "Not connected"), "");
		ldap_op_finished (op);
		return;
	}
	g_mutex_unlock (bl->priv->ldap_lock);

	/* the msg_type will be either SEARCH_ENTRY (if we're
	   successful) or SEARCH_RESULT (if we're not), so we finish
	   the op after either */
	msg_type = ldap_msgtype (res);
	if (msg_type == LDAP_RES_SEARCH_ENTRY) {
		LDAPMessage *e;
		EContact *contact;
		gchar *vcard;

		g_mutex_lock (bl->priv->ldap_lock);
		e = ldap_first_entry(bl->priv->ldap, res);
		g_mutex_unlock (bl->priv->ldap_lock);

		if (!e) {
			g_warning ("uh, this shouldn't happen");
			e_data_book_respond_get_contact (op->book,
							 op->opid,
							 EDB_ERROR_EX (OTHER_ERROR, "ldap_first_entry call failed"),
							 "");
			ldap_op_finished (op);
			return;
		}

		contact = build_contact_from_entry (bl, e, NULL);
		vcard = e_vcard_to_string (E_VCARD (contact), EVC_FORMAT_VCARD_30);
		e_data_book_respond_get_contact (op->book,
						 op->opid,
						 NULL /* Success */,
						 vcard);
		g_free (vcard);
		g_object_unref (contact);
		ldap_op_finished (op);
	}
	else if (msg_type == LDAP_RES_SEARCH_RESULT) {
		gchar *ldap_error_msg;
		gint ldap_error;

		g_mutex_lock (bl->priv->ldap_lock);
		ldap_parse_result (bl->priv->ldap, res, &ldap_error,
				   NULL, &ldap_error_msg, NULL, NULL, 0);
		g_mutex_unlock (bl->priv->ldap_lock);

		if (ldap_error != LDAP_SUCCESS) {
			g_warning ("get_contact_handler: %02X (%s), additional info: %s",
				   ldap_error,
				   ldap_err2string (ldap_error), ldap_error_msg);
		}
		ldap_memfree (ldap_error_msg);

		e_data_book_respond_get_contact (op->book,
						 op->opid,
						 ldap_error_to_response (ldap_error),
						 "");
		ldap_op_finished (op);
	}
	else {
		g_warning ("unhandled result type %d returned", msg_type);
		e_data_book_respond_get_contact (op->book,
						 op->opid,
						 EDB_ERROR_MSG_TYPE (msg_type),
						 "");
		ldap_op_finished (op);
	}

}

static void
get_contact_dtor (LDAPOp *op)
{
	LDAPGetContactOp *get_contact_op = (LDAPGetContactOp*)op;

	g_free (get_contact_op);
}

static void
get_contact (EBookBackend *backend,
	     EDataBook    *book,
	     guint32       opid,
	     const gchar   *id)
{
	LDAPGetContactOp *get_contact_op;
	EBookBackendGAL *bl = E_BOOK_BACKEND_GAL (backend);
	gint get_contact_msgid;
	EDataBookView *book_view;
	gint ldap_error;

	d(printf("get contact\n"));
	switch (bl->priv->mode) {
	case E_DATA_BOOK_MODE_LOCAL:
#if defined(ENABLE_CACHE) && ENABLE_CACHE
		if (bl->priv->marked_for_offline && bl->priv->file_db) {
			EContact *contact = e_book_backend_db_cache_get_contact (bl->priv->file_db, id);
			gchar *vcard_str;

			if (!contact) {
				e_data_book_respond_get_contact (book, opid, EDB_ERROR (CONTACT_NOT_FOUND), "");
				return;
			}

			vcard_str = e_vcard_to_string (E_VCARD (contact), EVC_FORMAT_VCARD_30);

			e_data_book_respond_get_contact (book,
							 opid,
							 NULL /* Success */,
							 vcard_str);
			g_free (vcard_str);
			g_object_unref (contact);
			return;
		}
#endif
		e_data_book_respond_get_contact(book, opid, EDB_ERROR (REPOSITORY_OFFLINE), "");
		return;

	case E_DATA_BOOK_MODE_REMOTE :
#if defined(ENABLE_CACHE) && ENABLE_CACHE
		d(printf("Mode:Remote\n"));
		if (bl->priv->marked_for_offline && bl->priv->file_db) {
			EContact *contact = e_book_backend_db_cache_get_contact (bl->priv->file_db, id);
			gchar *vcard_str;
			if (!contact) {
				e_data_book_respond_get_contact (book, opid, EDB_ERROR (CONTACT_NOT_FOUND), "");
				return;
			}

			vcard_str = e_vcard_to_string (E_VCARD (contact), EVC_FORMAT_VCARD_30);

			e_data_book_respond_get_contact (book,
							 opid,
							 NULL /* Success */,
							 vcard_str);
			g_free (vcard_str);
			g_object_unref (contact);
			return;
		}
		else {
#endif
			g_mutex_lock (bl->priv->ldap_lock);
			if (!bl->priv->ldap) {
				g_mutex_unlock (bl->priv->ldap_lock);
				e_data_book_respond_get_contact (book, opid, EDB_ERROR_EX (OTHER_ERROR, "Not connected"), "");
				return;
			}
			g_mutex_unlock (bl->priv->ldap_lock);

			get_contact_op = g_new0 (LDAPGetContactOp, 1);

			book_view = find_book_view (bl);

			do {
				g_mutex_lock (bl->priv->ldap_lock);
				ldap_error = ldap_search_ext (bl->priv->ldap, id,
							      LDAP_SCOPE_BASE,
							      "(objectclass=*)",
							      (gchar **) search_attrs, 0, NULL, NULL,
							      NULL, /* XXX timeout */
							      1, &get_contact_msgid);
				g_mutex_unlock (bl->priv->ldap_lock);
			} while (gal_reconnect (bl, book_view, ldap_error));

			if (ldap_error == LDAP_SUCCESS) {
				ldap_op_add ((LDAPOp*)get_contact_op, backend, book,
					     book_view, opid, get_contact_msgid,
					     get_contact_handler, get_contact_dtor);
			}
			else {
				e_data_book_respond_get_contact (book,
								 opid,
								 ldap_error_to_response (ldap_error),
								 "");
				get_contact_dtor ((LDAPOp*)get_contact_op);
			}
#if defined(ENABLE_CACHE) && ENABLE_CACHE
		}
#endif
	}
}

typedef struct {
	LDAPOp op;
	GList *contacts;
} LDAPGetContactListOp;

static void
contact_list_handler (LDAPOp *op, LDAPMessage *res)
{
	LDAPGetContactListOp *contact_list_op = (LDAPGetContactListOp*)op;
	EBookBackendGAL *bl = E_BOOK_BACKEND_GAL (op->backend);
	LDAPMessage *e;
	gint msg_type;

	g_mutex_lock (bl->priv->ldap_lock);
	if (!bl->priv->ldap) {
		g_mutex_unlock (bl->priv->ldap_lock);
		e_data_book_respond_get_contact_list (op->book, op->opid, EDB_ERROR_EX (OTHER_ERROR, "Not connected"), NULL);
		ldap_op_finished (op);
		return;
	}
	g_mutex_unlock (bl->priv->ldap_lock);

	msg_type = ldap_msgtype (res);
	if (msg_type == LDAP_RES_SEARCH_ENTRY) {
		g_mutex_lock (bl->priv->ldap_lock);
		e = ldap_first_entry (bl->priv->ldap, res);
		g_mutex_unlock (bl->priv->ldap_lock);

		while (NULL != e) {
			EContact *contact = build_contact_from_entry (bl, e, NULL);
			gchar *vcard = e_vcard_to_string (E_VCARD (contact), EVC_FORMAT_VCARD_30);

			d(printf ("vcard = %s\n", vcard));

			contact_list_op->contacts = g_list_append (contact_list_op->contacts,
								   vcard);

			g_object_unref (contact);
			g_mutex_lock (bl->priv->ldap_lock);
			e = ldap_next_entry (bl->priv->ldap, e);
			g_mutex_unlock (bl->priv->ldap_lock);
		}
	}
	else if (msg_type == LDAP_RES_SEARCH_RESULT) {
		gchar *ldap_error_msg;
		gint ldap_error;

		g_mutex_lock (bl->priv->ldap_lock);
		ldap_parse_result (bl->priv->ldap, res, &ldap_error,
				   NULL, &ldap_error_msg, NULL, NULL, 0);
		g_mutex_unlock (bl->priv->ldap_lock);
		if (ldap_error != LDAP_SUCCESS) {
			g_warning ("contact_list_handler: %02X (%s), additional info: %s",
				   ldap_error,
				   ldap_err2string (ldap_error), ldap_error_msg);
		}
		ldap_memfree (ldap_error_msg);

		d(printf ("search returned %d\n", ldap_error));

		if (ldap_error == LDAP_TIMELIMIT_EXCEEDED)
			e_data_book_respond_get_contact_list (op->book,
							      op->opid,
							      EDB_ERROR (SEARCH_TIME_LIMIT_EXCEEDED),
							      contact_list_op->contacts);
		else if (ldap_error == LDAP_SIZELIMIT_EXCEEDED)
			e_data_book_respond_get_contact_list (op->book,
							      op->opid,
							      EDB_ERROR (SEARCH_SIZE_LIMIT_EXCEEDED),
							      contact_list_op->contacts);
		else if (ldap_error == LDAP_SUCCESS)
			e_data_book_respond_get_contact_list (op->book,
							      op->opid,
							      NULL /* Success */,
							      contact_list_op->contacts);
		else
			e_data_book_respond_get_contact_list (op->book,
							      op->opid,
							      ldap_error_to_response (ldap_error),
							      contact_list_op->contacts);

		ldap_op_finished (op);
	}
	else {
		g_warning ("unhandled search result type %d returned", msg_type);
		e_data_book_respond_get_contact_list (op->book,
						      op->opid,
						      EDB_ERROR_MSG_TYPE (msg_type),
						      NULL);
		ldap_op_finished (op);
	}

}

static void
contact_list_dtor (LDAPOp *op)
{
	LDAPGetContactListOp *contact_list_op = (LDAPGetContactListOp*)op;

	g_free (contact_list_op);
}

static void
get_contact_list (EBookBackend *backend,
		  EDataBook    *book,
		  guint32       opid,
		  const gchar   *query)
{
	LDAPGetContactListOp *contact_list_op;
	EBookBackendGAL *bl = E_BOOK_BACKEND_GAL (backend);
	gint contact_list_msgid;
	EDataBookView *book_view;
	gint ldap_error;
	gchar *ldap_query;
	GError *error = NULL;

	d(printf("get contact list\n"));
	switch (bl->priv->mode) {
	case E_DATA_BOOK_MODE_LOCAL:
#if defined(ENABLE_CACHE) && ENABLE_CACHE
		if (bl->priv->marked_for_offline && bl->priv->file_db) {
			GList *contacts;
			GList *vcard_strings = NULL;
			GList *l;

			contacts = e_book_backend_db_cache_get_contacts (bl->priv->file_db, query);

			for (l = contacts; l; l = g_list_next (l)) {
				EContact *contact = l->data;
				vcard_strings = g_list_prepend (vcard_strings, e_vcard_to_string (E_VCARD (contact),
								EVC_FORMAT_VCARD_30));
				g_object_unref (contact);
			}

			g_list_free (contacts);
			e_data_book_respond_get_contact_list (book, opid, NULL /* Success */, vcard_strings);
			return;
		}
#endif
		e_data_book_respond_get_contact_list (book, opid, EDB_ERROR (REPOSITORY_OFFLINE), NULL);
		return;

	case E_DATA_BOOK_MODE_REMOTE:
#if defined(ENABLE_CACHE) && ENABLE_CACHE
		d(printf("Mode : Remote\n"));
		if (bl->priv->marked_for_offline && bl->priv->file_db) {
			GList *contacts;
			GList *vcard_strings = NULL;
			GList *l;

			contacts = e_book_backend_db_cache_get_contacts (bl->priv->file_db, query);

			for (l = contacts; l;l = g_list_next (l)) {
				EContact *contact = l->data;
				vcard_strings = g_list_prepend (vcard_strings, e_vcard_to_string (E_VCARD (contact),
								EVC_FORMAT_VCARD_30));
				g_object_unref (contact);
			}

			g_list_free (contacts);
			e_data_book_respond_get_contact_list (book, opid, NULL /* Success */, vcard_strings);
			return;
		}
		else {
#endif
			g_mutex_lock (bl->priv->ldap_lock);
			if (!bl->priv->ldap) {
				g_mutex_unlock (bl->priv->ldap_lock);
				e_data_book_respond_get_contact_list (book, opid, EDB_ERROR_EX (OTHER_ERROR, "Not connected"), NULL);
				return;
			}
			g_mutex_unlock (bl->priv->ldap_lock);

			contact_list_op = g_new0 (LDAPGetContactListOp, 1);
			book_view = find_book_view (bl);

			build_query (bl, query, NULL, &ldap_query, &error);
			if (error || !ldap_query) {
				e_data_book_respond_get_contact_list (book, opid, error, NULL);
				return;
			}

			d(printf ("getting contact list with filter: %s\n", ldap_query));

			do {
				g_mutex_lock (bl->priv->ldap_lock);
				ldap_error = ldap_search_ext (bl->priv->ldap, LDAP_ROOT_DSE,
							      LDAP_SCOPE_SUBTREE,
							      ldap_query,
							      (gchar **) search_attrs, 0, NULL, NULL,
							      NULL, /* XXX timeout */
							      LDAP_NO_LIMIT, &contact_list_msgid);
				g_mutex_unlock (bl->priv->ldap_lock);
			} while (gal_reconnect (bl, book_view, ldap_error));

			g_free (ldap_query);

			if (ldap_error == LDAP_SUCCESS) {
				ldap_op_add ((LDAPOp*)contact_list_op, backend, book,
					     book_view, opid, contact_list_msgid,
					     contact_list_handler, contact_list_dtor);
			}
			else {
				e_data_book_respond_get_contact_list (book,
								      opid,
								      ldap_error_to_response (ldap_error),
								      NULL);
				contact_list_dtor ((LDAPOp*)contact_list_op);
			}
#if defined(ENABLE_CACHE) && ENABLE_CACHE
		}
#endif
	}
}

#define IS_RFC2254_CHAR(c) ((c) == '*' || (c) =='\\' || (c) == '(' || (c) == ')' || (c) == '\0')
static gchar *
rfc2254_escape(gchar *str)
{
	gint i;
	gint len = strlen(str);
	gint newlen = 0;

	for (i = 0; i < len; i++) {
		if (IS_RFC2254_CHAR(str[i]))
			newlen += 3;
		else
			newlen++;
	}

	if (len == newlen) {
		return g_strdup (str);
	}
	else {
		gchar *newstr = g_malloc0 (newlen + 1);
		gint j = 0;
		for (i = 0; i < len; i++) {
			if (IS_RFC2254_CHAR(str[i])) {
				sprintf (newstr + j, "\\%02x", str[i]);
				j+= 3;
			}
			else {
				newstr[j++] = str[i];
			}
		}
		return newstr;
	}
}

static ESExpResult *
func_and(ESExp *f, gint argc, ESExpResult **argv, gpointer data)
{
	ESExpResult *r;
	GString *string;
	gint i;

	/* Check for short circuit */
	for (i = 0; i < argc; i++) {
		if (argv[i]->type == ESEXP_RES_BOOL &&
		    argv[i]->value.boolean == FALSE) {
			r = e_sexp_result_new(f, ESEXP_RES_BOOL);
			r->value.boolean = FALSE;
			return r;
		} else if (argv[i]->type == ESEXP_RES_UNDEFINED)
			return e_sexp_result_new(f, ESEXP_RES_UNDEFINED);
	}

	string = g_string_new("(&");
	for (i = 0; i < argc; i++) {
		if (argv[i]->type != ESEXP_RES_STRING)
			continue;
		g_string_append(string, argv[i]->value.string);
	}
	g_string_append(string, ")");

	r = e_sexp_result_new(f, ESEXP_RES_STRING);
	r->value.string = string->str;
	g_string_free(string, FALSE);

	return r;
}

static ESExpResult *
func_or(ESExp *f, gint argc, ESExpResult **argv, gpointer data)
{
	ESExpResult *r;
	GString *string;
	gint i;

	/* Check for short circuit */
	for (i = 0; i < argc; i++) {
		if (argv[i]->type == ESEXP_RES_BOOL &&
		    argv[i]->value.boolean == TRUE) {
			r = e_sexp_result_new(f, ESEXP_RES_BOOL);
			r->value.boolean = TRUE;
			return r;
		} else if (argv[i]->type == ESEXP_RES_UNDEFINED)
			return e_sexp_result_new(f, ESEXP_RES_UNDEFINED);
	}

	string = g_string_new("(|");
	for (i = 0; i < argc; i++) {
		if (argv[i]->type != ESEXP_RES_STRING)
			continue;
		g_string_append(string, argv[i]->value.string);
	}
	g_string_append(string, ")");

	r = e_sexp_result_new(f, ESEXP_RES_STRING);
	r->value.string = string->str;
	g_string_free(string, FALSE);

	return r;
}

static ESExpResult *
func_not(ESExp *f, gint argc, ESExpResult **argv, gpointer data)
{
	ESExpResult *r;

	if (argc != 1 ||
	    (argv[0]->type != ESEXP_RES_STRING &&
	     argv[0]->type != ESEXP_RES_BOOL))
		return e_sexp_result_new(f, ESEXP_RES_UNDEFINED);

	if (argv[0]->type == ESEXP_RES_STRING) {
		r = e_sexp_result_new(f, ESEXP_RES_STRING);
		r->value.string = g_strdup_printf ("(!%s)",
						   argv[0]->value.string);
	} else {
		r = e_sexp_result_new(f, ESEXP_RES_BOOL);
		r->value.boolean = !argv[0]->value.boolean;
	}

	return r;
}

static const gchar *
query_prop_to_ldap (const gchar *query_prop)
{
	gint i;

	if (!strcmp (query_prop, "email"))
		query_prop = "email_1";

	for (i = 0; i < G_N_ELEMENTS (prop_info); i++)
		if (!strcmp (query_prop, e_contact_field_name (prop_info[i].field_id)))
			return prop_info[i].ldap_attr;

	return NULL;
}

static ESExpResult *
func_contains(ESExp *f, gint argc, ESExpResult **argv, gpointer data)
{
	ESExpResult *r;
	const gchar *ldap_attr;
	gchar *propname, *str;

	if (argc != 2 ||
	    argv[0]->type != ESEXP_RES_STRING ||
	    argv[1]->type != ESEXP_RES_STRING)
		return e_sexp_result_new(f, ESEXP_RES_UNDEFINED);

	propname = argv[0]->value.string;
	str = argv[1]->value.string;

	if (!strcmp(propname, "x-evolution-any-field")) {
		/* This gui does (contains "x-evolution-any-field" ""),
		 * when you hit "Clear". We want that to be empty. But
		 * other "any field contains" searches should give an
		 * error.
		 */
		if (strlen(str) == 0) {
			r = e_sexp_result_new(f, ESEXP_RES_BOOL);
			r->value.boolean = FALSE;
		} else {
			r = e_sexp_result_new(f, ESEXP_RES_STRING);
			r->value.string = g_strdup_printf ("(mailNickname=%s)", str);
		}

		return r;
	}

	ldap_attr = query_prop_to_ldap(argv[0]->value.string);
	if (!ldap_attr) {
		/* Attribute doesn't exist, so it can't possibly match */
		r = e_sexp_result_new(f, ESEXP_RES_BOOL);
		r->value.boolean = FALSE;
		return r;
	}

	/* AD doesn't do substring indexes, so we only allow
	 * (contains FIELD ""), meaning "FIELD exists".
	 */
	if (strlen(str) == 0) {
		r = e_sexp_result_new(f, ESEXP_RES_STRING);
		r->value.string = g_strdup_printf ("(%s=*)", ldap_attr);
	} else if (!strcmp(propname, "file_as")) {
		r = e_sexp_result_new(f, ESEXP_RES_STRING);
		r->value.string = g_strdup_printf ("(|(displayName=*%s*)(sn=*%s*)(%s=*%s*))", str, str, ldap_attr, str);
	} else if (g_str_equal (ldap_attr, "displayName")) {
		r = e_sexp_result_new(f, ESEXP_RES_STRING);
		r->value.string = g_strdup_printf("(|(displayName=*%s*)(sn=*%s*)(givenName=*%s*))", str, str, str);
	} else
		r = e_sexp_result_new(f, ESEXP_RES_UNDEFINED);
	return r;
}

static ESExpResult *
func_is_or_begins_with(ESExp *f, gint argc, ESExpResult **argv, gboolean exact)
{
	ESExpResult *r;
	const gchar *star;
	const gchar *ldap_attr;
	gchar *propname, *str, *filter;

	if (argc != 2
	    || argv[0]->type != ESEXP_RES_STRING
	    || argv[1]->type != ESEXP_RES_STRING)
		return e_sexp_result_new(f, ESEXP_RES_UNDEFINED);

	propname = argv[0]->value.string;
	str = rfc2254_escape(argv[1]->value.string);
	star = exact ? "" : "*";

	if (!exact && strlen (str) == 0 && strcmp(propname, "file_as")) {
		/* Can't do (beginswith FIELD "") */
		return e_sexp_result_new(f, ESEXP_RES_UNDEFINED);
	}

	/* We use the query "(beginswith fileas "")" while building cache for
	 * GAL offline, where we try to retrive all the contacts and store it
	 * locally. Retrieving *all* the contacts may not be possible in case
	 * of large number of contacts and huge data, (for the same reason
	 * we don't support empty queries in GAL when online.) In such cases
	 * cache may not be complete.
	 */
	if (!strcmp(propname, "file_as")) {
		filter = g_strdup_printf("(displayName=%s%s)", str, star);
		goto done;
	}

	ldap_attr = query_prop_to_ldap(propname);
	if (!ldap_attr) {
		g_free (str);

		/* Property doesn't exist, so it can't ever match */
		r = e_sexp_result_new(f, ESEXP_RES_BOOL);
		r->value.boolean = FALSE;
		return r;
	}

	if (!strcmp (propname, "full_name")) {
		gchar *first, *last, *space;

		space = strchr (str, ' ');
		if (space && space > str) {
			if (*(space - 1) == ',') {
				first = g_strdup (space + 1);
				last = g_strndup (str, space - str - 1);
			} else {
				first = g_strndup (str, space - str);
				last = g_strdup (space + 1);
			}
			filter = g_strdup_printf("(|(displayName=%s%s)(sn=%s%s)(givenName=%s%s)(&(givenName=%s%s)(sn=%s%s)))",
						 str, star, str, star,
						 str, star, first, star,
						 last, star);
			g_free (first);
			g_free (last);
		} else {
			filter = g_strdup_printf("(|(displayName=%s%s)(sn=%s%s)(givenName=%s%s)(mailNickname=%s%s))",
						 str, star, str, star,
						 str, star, str, star);
		}
	} else
		filter = g_strdup_printf("(%s=%s%s)", ldap_attr, str, star);

 done:
	g_free (str);

	r = e_sexp_result_new(f, ESEXP_RES_STRING);
	r->value.string = filter;
	return r;
}

static ESExpResult *
func_is(struct _ESExp *f, gint argc, struct _ESExpResult **argv, gpointer data)
{
	return func_is_or_begins_with(f, argc, argv, TRUE);
}

static ESExpResult *
func_beginswith(struct _ESExp *f, gint argc, struct _ESExpResult **argv, gpointer data)
{
	return func_is_or_begins_with(f, argc, argv, FALSE);
}

static ESExpResult *
func_endswith(struct _ESExp *f, gint argc, struct _ESExpResult **argv, gpointer data)
{
	/* We don't allow endswith searches */
	return e_sexp_result_new(f, ESEXP_RES_UNDEFINED);
}

/* 'builtin' functions */
static struct {
	const gchar *name;
	ESExpFunc *func;
} symbols[] = {
	{ "and", func_and },
	{ "or", func_or },
	{ "not", func_not },
	{ "contains", func_contains },
	{ "is", func_is },
	{ "beginswith", func_beginswith },
	{ "endswith", func_endswith },
};

static void
build_query (EBookBackendGAL *bl, const gchar *query, const gchar *ldap_filter, gchar **ldap_query, GError **perror)
{
	ESExp *sexp;
	ESExpResult *r;
	gint i;

	sexp = e_sexp_new();

	for (i = 0; i < G_N_ELEMENTS (symbols); i++) {
		e_sexp_add_function(sexp, 0, (gchar *) symbols[i].name,
				    symbols[i].func, NULL);
	}

	e_sexp_input_text (sexp, query, strlen (query));
	e_sexp_parse (sexp);

	r = e_sexp_eval (sexp);
	if (!r) {
		/* Bad query or it isn't supported */
		*ldap_query = NULL;
		e_sexp_unref (sexp);
		g_propagate_error (perror, EDB_ERROR (QUERY_REFUSED));
		return;
	}

	if (r->type == ESEXP_RES_STRING) {
		if (!strcmp (r->value.string, "(mail=*)")) {
			/* If the query is empty,
			 * don't search for all the contats
			 */
			*ldap_query = NULL;
			g_propagate_error (perror, EDB_ERROR (QUERY_REFUSED));
		}
		else {
			gchar *addfilter = NULL;

			if (ldap_filter)
				addfilter = g_strdup_printf ("(%s)", ldap_filter);

			*ldap_query = g_strdup_printf ("(&(mail=*)(!(msExchHideFromAddressLists=TRUE))%s%s)", addfilter ? addfilter : "", r->value.string);
		}
	} else if (r->type == ESEXP_RES_BOOL) {
		/* If it's FALSE, that means "no matches". If it's TRUE
		 * that means "everything matches", but we don't support
		 * that, so it also means "no matches".
		 */
		*ldap_query = NULL;
	} else {
		/* Bad query */
		*ldap_query = NULL;
		g_propagate_error (perror, EDB_ERROR (QUERY_REFUSED));
	}

	e_sexp_result_free(sexp, r);
	e_sexp_unref (sexp);
}

static void
manager_populate(EContact *contact, gchar **values, EBookBackendGAL *bl, E2kOperation *op)
{
	E2kGlobalCatalogEntry *entry;
	E2kGlobalCatalogStatus status;

	status = e2k_global_catalog_lookup (bl->priv->gc,
					    op,
					    E2K_GLOBAL_CATALOG_LOOKUP_BY_DN,
					    values[0], 0, &entry);
	if (status != E2K_GLOBAL_CATALOG_OK)
		return;

	e_contact_set (contact, E_CONTACT_MANAGER,
		       entry->display_name);
	e2k_global_catalog_entry_free (bl->priv->gc, entry);
}

#define G_STRNDUP(str, len) g_strndup(str, len); \
				str += len;

static void
member_populate (EContact *contact, gchar **values, EBookBackendGAL *bl, E2kOperation *op)
{
	gint i;
	gchar **member_info;

	e_contact_set (contact, E_CONTACT_IS_LIST, GINT_TO_POINTER (TRUE));
	e_contact_set (contact, E_CONTACT_LIST_SHOW_ADDRESSES, GINT_TO_POINTER (TRUE));

	for (i=0; values[i]; i++) {
		EVCardAttribute *attr;

		member_info = g_strsplit (values [i], ";", -1);
		attr = e_vcard_attribute_new (NULL, EVC_EMAIL);
		e_vcard_attribute_add_param_with_value (attr, e_vcard_attribute_param_new (EVC_X_DEST_CONTACT_UID), member_info[1]);

		if (member_info[2]) {
			gint len = strlen (member_info[2]);
			gchar *value;

			if (member_info [2][0] == '\"' && member_info [2][len - 1] == '\"')
				value = g_strdup_printf ("%s <%s>", member_info [2], member_info [0]);
			else
				value = g_strdup_printf ("\"%s\" <%s>", member_info [2], member_info [0]);

			e_vcard_attribute_add_value (attr, value);
			g_free (value);
		} else {
			e_vcard_attribute_add_value (attr, member_info[0]);
		}

		e_vcard_add_attribute (E_VCARD (contact), attr);
		g_strfreev (member_info);
	}
}

static gchar *
get_time_stamp (gchar *serv_time_str, time_t *mtime)
{
	gchar *input_str = serv_time_str, *result_str = NULL;
	gchar *year, *month, *date, *hour, *minute, *second, *zone;
	struct tm mytime;
	/* input time string will be of the format 20050419162256.0Z
	 * out put string shd be of the format 2005-04-19T16:22:56.0Z
	 * ("%04d-%02d-%02dT%02d:%02d:%02dZ")
	 */

	/* FIXME : Add a check for proper input string */
	year = G_STRNDUP(input_str, 4)
	month = G_STRNDUP(input_str, 2)
	date = G_STRNDUP(input_str, 2)
	hour = G_STRNDUP(input_str, 2)
	minute = G_STRNDUP(input_str, 2)
	second = G_STRNDUP(input_str, 2)
	input_str++; // parse over the dot
	zone = G_STRNDUP(input_str, 1)

	mytime.tm_year = atoi(year)-1900;
	mytime.tm_mon = atoi(month)-1;
	mytime.tm_mday = atoi(date);
	mytime.tm_hour = atoi(hour);
	mytime.tm_min = atoi(minute);
	mytime.tm_sec = atoi(second);
	mytime.tm_isdst = 0;

	*mtime = mktime(&mytime);
	result_str = g_strdup_printf ("%s-%s-%sT%s:%s:%s.%sZ",
		year, month, date, hour, minute, second, zone);

	d(printf ("rev time : %s\n", result_str));

	g_free (year);
	g_free (month);
	g_free (date);
	g_free (hour);
	g_free (minute);
	g_free (second);
	g_free (zone);

	return result_str;
}

static void
last_mod_time_populate (EContact *contact, gchar **values,
			EBookBackendGAL *bl, E2kOperation *op)
{
	gchar *time_str;
	time_t mtime = 0;

	/* FIXME: Some better way to do this  */
	time_str = get_time_stamp (values[0], &mtime);
	if (time_str)
		e_contact_set (contact, E_CONTACT_REV, time_str);

#if defined(ENABLE_CACHE) && ENABLE_CACHE
	d(printf("%s: %d %d: %s\n", values[0], bl->priv->last_best_time, mtime, ctime(&mtime)));
	if (bl->priv->last_best_time < mtime)
		bl->priv->last_best_time = mtime;
#endif
	g_free (time_str);
}

typedef struct {
	LDAPOp op;
	EDataBookView *view;

	/* used to detect problems with start/stop_book_view racing */
	gboolean aborted;
	/* used by search_handler to only send the status messages once */
	gboolean notified_receiving_results;
} LDAPSearchOp;

static EContact *
build_contact_from_entry (EBookBackendGAL *bl, LDAPMessage *e, GList **existing_objectclasses)
{
	LDAP *subldap = NULL;
	EContact *contact = e_contact_new ();
	gchar *dn;
	gchar *attr;
	BerElement *ber = NULL;
	gboolean is_group = FALSE;

	g_mutex_lock (bl->priv->ldap_lock);
	dn = ldap_get_dn (bl->priv->ldap, e);
	g_mutex_unlock (bl->priv->ldap_lock);

	e_contact_set (contact, E_CONTACT_UID, dn);
	ldap_memfree (dn);

	if (can_expand_groups (E_BOOK_BACKEND (bl))) {
		BerElement *tber = NULL;

		g_mutex_lock (bl->priv->ldap_lock);
		attr = ldap_first_attribute (bl->priv->ldap, e, &tber);
		while (attr) {
			if (!strcmp(attr, "member")) {
				d(printf("It is a DL\n"));
				is_group = TRUE;
				ldap_memfree (attr);
				break;
			}
			ldap_memfree (attr);
			attr = ldap_next_attribute (bl->priv->ldap, e, tber);
		}
		if (tber)
			ber_free (tber, 0);
		g_mutex_unlock (bl->priv->ldap_lock);
	}

	g_mutex_lock (bl->priv->ldap_lock);
	attr = ldap_first_attribute (bl->priv->ldap, e, &ber);
	g_mutex_unlock (bl->priv->ldap_lock);

	while (attr) {
		gint i;
		struct prop_info *info = NULL;
		gchar **values;

		if (existing_objectclasses && !g_ascii_strcasecmp (attr, "objectclass")) {
			g_mutex_lock (bl->priv->ldap_lock);
			values = ldap_get_values (bl->priv->ldap, e, attr);
			g_mutex_unlock (bl->priv->ldap_lock);
			for (i = 0; values[i]; i++) {
				if (!g_ascii_strcasecmp (values [i], "groupOfNames")) {
					d(printf ("groupOfNames\n"));
					e_contact_set (contact, E_CONTACT_IS_LIST, GINT_TO_POINTER (TRUE));
					e_contact_set (contact, E_CONTACT_LIST_SHOW_ADDRESSES, GINT_TO_POINTER (TRUE));
				}
				if (existing_objectclasses)
					*existing_objectclasses = g_list_append (*existing_objectclasses, g_strdup (values[i]));
			}
			ldap_value_free (values);
		}
		else {
			for (i = 0; i < G_N_ELEMENTS (prop_info); i++)
				if (!g_ascii_strcasecmp (attr, prop_info[i].ldap_attr)) {
					info = &prop_info[i];
					break;
				}

			d(printf ("attr = %s, ", attr));
			d(printf ("info = %p\n", info));

			if (info) {
				if (1) {
					g_mutex_lock (bl->priv->ldap_lock);
					values = ldap_get_values (bl->priv->ldap, e, attr);
					g_mutex_unlock (bl->priv->ldap_lock);

					if (values) {
						if (info->prop_type & PROP_TYPE_STRING && !(is_group && (info->field_id == E_CONTACT_EMAIL_1))) {
							d(printf ("value = %s %s\n", e_contact_field_name(info->field_id), values[0]));
							/* if it's a normal property just set the string */
							if (values[0])
								e_contact_set (contact, info->field_id, values[0]);
						}
						else if (info->prop_type & PROP_TYPE_COMPLEX) {
							/* if it's a list call the contact-populate function,
							   which calls g_object_set to set the property */
							info->populate_contact_func(contact, values, bl, NULL);
						}
						else if (is_group && (info->prop_type & PROP_TYPE_GROUP)) {
							gchar *grpattrs[3];
							gint i, view_limit = -1, ldap_error = LDAP_SUCCESS, count;
							EDataBookView *book_view;
							LDAPMessage *result;
							gchar **email_values, **cn_values, **member_info;

							if (!subldap) {
								subldap = e2k_global_catalog_get_ldap (bl->priv->gc, NULL, NULL);
							}
							grpattrs[0] = (gchar *) "cn";
							grpattrs[1] = (gchar *) "mail";
							grpattrs[2] = NULL;
							/*search for member attributes*/
							/*get the e-mail id for each member and add them to the list*/

							book_view = find_book_view (bl);
							if (book_view)
								view_limit = e_data_book_view_get_max_results (book_view);
							if (view_limit == -1 || view_limit > bl->priv->gc->response_limit)
								view_limit = bl->priv->gc->response_limit;

							count = ldap_count_values (values);
							member_info = g_new0 (gchar *, count+1);
							d(printf ("Fetching members\n"));
							for (i=0; values[i]; i++) {
								/* get the email id for the given dn */
								/* set base to DN and scope to base */
								d(printf("value (dn) = %s \n", values [i]));
								do {
									if (subldap && (ldap_error = ldap_search_ext_s (subldap,
												values[i],
												LDAP_SCOPE_BASE,
												"(objectclass=*)",
												grpattrs, 0,
												NULL,
												NULL,
												NULL,
												view_limit,
												&result)) == LDAP_SUCCESS) {
										/* find email ids of members */
										cn_values = ldap_get_values (subldap, result, "cn");
										email_values = ldap_get_values (subldap, result, "mail");

										if (email_values) {
											d(printf ("email = %s \n", email_values [0]));
											*(member_info+i) =
												g_strdup_printf ("%s;%s;",
														email_values[0], values[i]);
											ldap_value_free (email_values);
										}
										if (cn_values) {
											d(printf ("cn = %s \n", cn_values[0]));
											*(member_info+i) =
												g_strconcat (* (member_info +i),
														cn_values[0], NULL);
											ldap_value_free (cn_values);
										}
									}
								}
								while (ldap_reconnect (bl, book_view, &subldap, ldap_error));

								if (ldap_error != LDAP_SUCCESS) {
									book_view_notify_status (book_view,
											ldap_err2string(ldap_error));
									continue;
								}
							}
							/* call populate function */
							info->populate_contact_func (contact, member_info, bl, NULL);

							for (i=0; i<count; i++) {
								g_free (*(member_info+i));
							}
							g_free (member_info);
						}

						ldap_value_free (values);
					}
				}
			}
		}

		ldap_memfree (attr);
		g_mutex_lock (bl->priv->ldap_lock);
		attr = ldap_next_attribute (bl->priv->ldap, e, ber);
		g_mutex_unlock (bl->priv->ldap_lock);
	}

	if (ber)
		ber_free (ber, 0);

	if (subldap)
		ldap_unbind (subldap);

	return contact;
}

static gboolean
poll_ldap (EBookBackendGAL *bl)
{
	gint            rc;
	LDAPMessage    *res;
	struct timeval timeout;

	g_mutex_lock (bl->priv->ldap_lock);
	if (!bl->priv->ldap) {
		g_mutex_unlock (bl->priv->ldap_lock);
		bl->priv->poll_timeout = -1;
		return FALSE;
	}
	g_mutex_unlock (bl->priv->ldap_lock);

	if (!bl->priv->active_ops) {
		g_warning ("poll_ldap being called for backend with no active operations");
		bl->priv->poll_timeout = -1;
		return FALSE;
	}

	timeout.tv_sec = 0;
	timeout.tv_usec = LDAP_RESULT_TIMEOUT_MILLIS * 1000;

	g_mutex_lock (bl->priv->ldap_lock);
	rc = ldap_result (bl->priv->ldap, LDAP_RES_ANY, 0, &timeout, &res);
	g_mutex_unlock (bl->priv->ldap_lock);
	if (rc != 0) {/* rc == 0 means timeout exceeded */
		if (rc == -1) {
			EDataBookView *book_view = find_book_view (bl);
			d(printf ("ldap_result returned -1, restarting ops\n"));

			gal_reconnect (bl, book_view, LDAP_SERVER_DOWN);
#if 0
			if (bl->priv->connected)
				restart_ops (bl);
#endif
		}
		else {
			gint msgid = ldap_msgid (res);
			LDAPOp *op;

			g_static_rec_mutex_lock (&bl->priv->op_hash_mutex);
			op = g_hash_table_lookup (bl->priv->id_to_op, &msgid);

			d(printf ("looked up msgid %d, got op %p\n", msgid, op));

			if (op)
				op->handler (op, res);
			else
				g_warning ("unknown operation, msgid = %d", msgid);

			/* XXX should the call to op->handler be
			   protected by the lock? */
			g_static_rec_mutex_unlock (&bl->priv->op_hash_mutex);

			ldap_msgfree(res);
		}
	}

	return TRUE;
}

static void
ldap_search_handler (LDAPOp *op, LDAPMessage *res)
{
	LDAPSearchOp *search_op = (LDAPSearchOp*)op;
	EDataBookView *view = search_op->view;
	EBookBackendGAL *bl = E_BOOK_BACKEND_GAL (op->backend);
	LDAPMessage *e;
	GError *error = NULL;
	gint msg_type;

	d(printf ("ldap_search_handler (%p)\n", view));
	g_mutex_lock (bl->priv->ldap_lock);
	if (!bl->priv->ldap) {
		g_mutex_unlock (bl->priv->ldap_lock);
		d(printf("%s:%s: other error\n", G_STRLOC, G_STRFUNC));
		error = EDB_ERROR_EX (OTHER_ERROR, "Not connected");
		e_data_book_view_notify_complete (view, error);
		g_error_free (error);
		ldap_op_finished (op);
		return;
	}
	g_mutex_unlock (bl->priv->ldap_lock);

	if (!search_op->notified_receiving_results) {
		search_op->notified_receiving_results = TRUE;
		book_view_notify_status (op->view, _("Receiving LDAP search results..."));
	}

	msg_type = ldap_msgtype (res);
	if (msg_type == LDAP_RES_SEARCH_ENTRY) {
		g_mutex_lock (bl->priv->ldap_lock);
		e = ldap_first_entry (bl->priv->ldap, res);
		g_mutex_unlock (bl->priv->ldap_lock);

		while (NULL != e) {
			EContact *contact = build_contact_from_entry (bl, e, NULL);

			e_data_book_view_notify_update (view, contact);

			g_object_unref (contact);

			g_mutex_lock (bl->priv->ldap_lock);
			e = ldap_next_entry (bl->priv->ldap, e);
			g_mutex_unlock (bl->priv->ldap_lock);
		}
	}
	else if (msg_type == LDAP_RES_SEARCH_RESULT) {
		gchar *ldap_error_msg;
		gint ldap_error;

		g_mutex_lock (bl->priv->ldap_lock);
		ldap_parse_result (bl->priv->ldap, res, &ldap_error,
				   NULL, &ldap_error_msg, NULL, NULL, 0);
		g_mutex_unlock (bl->priv->ldap_lock);
		if (ldap_error != LDAP_SUCCESS) {
			g_warning ("ldap_search_handler: %02X (%s), additional info: %s",
				   ldap_error,
				   ldap_err2string (ldap_error), ldap_error_msg);
		}
		ldap_memfree (ldap_error_msg);

		if ((ldap_error == LDAP_TIMELIMIT_EXCEEDED || ldap_error == LDAP_SIZELIMIT_EXCEEDED) && can_browse ((EBookBackend *)bl))
			/* do not complain when search limit exceeded for browsable LDAPs */
			error = NULL;
		else if (ldap_error == LDAP_TIMELIMIT_EXCEEDED)
			error = EDB_ERROR (SEARCH_TIME_LIMIT_EXCEEDED);
		else if (ldap_error == LDAP_SIZELIMIT_EXCEEDED)
			error = EDB_ERROR (SEARCH_SIZE_LIMIT_EXCEEDED);
		else if (ldap_error == LDAP_SUCCESS)
			error = NULL;
		else
			error = ldap_error_to_response (ldap_error);

		e_data_book_view_notify_complete (view, error);
		if (error)
			g_error_free (error);
		ldap_op_finished (op);
	}
	else {
		g_warning ("unhandled search result type %d returned", msg_type);
		error = EDB_ERROR_MSG_TYPE (msg_type);
		e_data_book_view_notify_complete (view, error);
		g_error_free (error);
		ldap_op_finished (op);

	}
}

static void
ldap_search_dtor (LDAPOp *op)
{
	LDAPSearchOp *search_op = (LDAPSearchOp*) op;

	d(printf ("ldap_search_dtor (%p)\n", search_op->view));

	/* unhook us from our EDataBookView */
	d(printf ("ldap_search_dtor: Setting null inplace of %p in view %p\n", op, search_op->view));
	g_object_set_data (G_OBJECT (search_op->view), "EBookBackendGAL.BookView::search_op", NULL);

	e_data_book_view_unref (search_op->view);

	if (!search_op->aborted)
		g_free (search_op);
}

#if defined(ENABLE_CACHE) && ENABLE_CACHE
static void
get_contacts_from_cache (EBookBackendGAL *ebg,
			 const gchar *query,
			 GPtrArray *ids,
			 EDataBookView *book_view)

{
	gint i;

	for (i = 0; i < ids->len; i++) {
		gchar *uid = g_ptr_array_index (ids, i);

		EContact *contact =
			e_book_backend_db_cache_get_contact (ebg->priv->file_db, uid);
		if (contact) {
			e_data_book_view_notify_update (book_view, contact);
			g_object_unref (contact);
		}
	}
	e_data_book_view_notify_complete (book_view, NULL /* Success */);
}
#endif

static void
start_book_view (EBookBackend  *backend,
		 EDataBookView *view)
{
	EBookBackendGAL *bl = E_BOOK_BACKEND_GAL (backend);
#if defined(ENABLE_CACHE) && ENABLE_CACHE
	GList *contacts, *l;
#endif
	gchar *ldap_query;
	gint ldap_err = LDAP_SUCCESS;
	gint search_msgid;
	gint view_limit;
	GError *err = NULL;

	d(printf("start book view\n"));
	switch (bl->priv->mode) {
	case E_DATA_BOOK_MODE_LOCAL:
#if defined(ENABLE_CACHE) && ENABLE_CACHE
		if (!(bl->priv->marked_for_offline && bl->priv->file_db)) {
			err = EDB_ERROR (REPOSITORY_OFFLINE);
			e_data_book_view_notify_complete (view, err);
			g_error_free (err);
			return;
		}

		contacts = e_book_backend_db_cache_get_contacts (bl->priv->file_db,
							      e_data_book_view_get_card_query (view));

		for (l = contacts; l; l = g_list_next (l)) {
			EContact *contact = l->data;
			e_data_book_view_notify_update (view, contact);
			g_object_unref (contact);
		}

		g_list_free (contacts);

		e_data_book_view_notify_complete (view, NULL /* Success */);
		return;
#else
		err = EDB_ERROR (REPOSITORY_OFFLINE);
		e_data_book_view_notify_complete (view, err);
		g_error_free (err);
		return;
#endif
	case E_DATA_BOOK_MODE_REMOTE:
#if defined(ENABLE_CACHE) && ENABLE_CACHE
		d(printf("Mode:Remote\n"));
		if (bl->priv->marked_for_offline && bl->priv->file_db) {
			const gchar *query = e_data_book_view_get_card_query (view);
			GPtrArray *ids = NULL;
			err = NULL;
			d(printf("Marked for offline and cache present\n"));

			build_query (bl, e_data_book_view_get_card_query (view), NULL, &ldap_query, &err);

			/* search for anything */
			if (!ldap_query && (!err || err->code == E_DATA_BOOK_STATUS_QUERY_REFUSED) && can_browse ((EBookBackend *)bl)) {
				ldap_query = g_strdup ("(mail=*)");

				if (err)
					g_error_free (err);
				err = NULL;
			}

			if (err || !ldap_query) {
				e_data_book_view_notify_complete (view, err);
				if (err)
					g_error_free (err);
				if (ldap_query)
					g_free (ldap_query);
				return;
			}

			if (bl->priv->is_summary_ready &&
			    e_book_backend_summary_is_summary_query (bl->priv->summary, query)) {
				d(printf("Summary ready and summary_query, searching from summary \n"));
				ids = e_book_backend_summary_search (bl->priv->summary, query);
				if (ids && ids->len > 0) {
					get_contacts_from_cache (bl, query, ids, view);
					g_ptr_array_free (ids, TRUE);
				}
				return;
			}

			contacts = e_book_backend_db_cache_get_contacts (bl->priv->file_db,
									 e_data_book_view_get_card_query (view));
			for (l = contacts; l;l = g_list_next (l)) {
				EContact *contact = l->data;
				e_data_book_view_notify_update (view, contact);
				g_object_unref (contact);
			}

			g_list_free (contacts);

			e_data_book_view_notify_complete (view, NULL /* Success */);
			return;
		}
		else {
#endif
			if (!bl->priv->connected) {
				err = EDB_ERROR (REPOSITORY_OFFLINE);
				e_data_book_view_notify_complete (view, err);
				g_error_free (err);
				return;
			}

			d(printf("Not marked for offline or cache not there\n"));
			g_mutex_lock (bl->priv->ldap_lock);
			if (!bl->priv->ldap) {
				g_mutex_unlock (bl->priv->ldap_lock);
				if (!gal_reconnect (bl, view, 0)) {
					d(printf("%s:%s: no ldap :(\n", G_STRLOC, G_STRFUNC));
					err = EDB_ERROR (INVALID_QUERY);
					e_data_book_view_notify_complete (view, err);
					g_error_free (err);
					return;
				}
			}
			g_mutex_unlock (bl->priv->ldap_lock);

			/* we start at 1 so the user sees stuff as it appears and we
			   aren't left waiting for more cards to show up, if the
			   connection is slow. */
			e_data_book_view_set_thresholds (view, 1, 3000);

			view_limit = e_data_book_view_get_max_results (view);
			if (view_limit == -1 || view_limit > bl->priv->gc->response_limit)
				view_limit = bl->priv->gc->response_limit;

			d(printf ("start_book_view (%p)\n", view));

			build_query (bl, e_data_book_view_get_card_query (view), NULL, &ldap_query, &err);

			/* search for anything */
			if (!ldap_query &&  (!err || err->code == E_DATA_BOOK_STATUS_QUERY_REFUSED) && can_browse ((EBookBackend *)bl)) {
				ldap_query = g_strdup ("(mail=*)");
				if (err)
					g_error_free (err);
				err = NULL;
			}

			d(printf("%s:%s: %s\n", G_STRLOC, G_STRFUNC, ldap_query ? ldap_query : "No ldap_query produced!"));
			if (err || !ldap_query) {
				e_data_book_view_notify_complete (view, err);
				if (err)
					g_error_free (err);
				if (ldap_query)
					g_free (ldap_query);
				d(printf("%s:%s: failure \n", G_STRLOC, G_STRFUNC));
				return;
			}

			do {
				g_mutex_lock (bl->priv->ldap_lock);
				if (bl->priv->ldap) {
					g_mutex_unlock (bl->priv->ldap_lock);
					book_view_notify_status (view, _("Searching..."));

					g_mutex_lock (bl->priv->ldap_lock);
					d(printf("%s:%s: starting \n", G_STRLOC, G_STRFUNC));
					ldap_err = ldap_search_ext (bl->priv->ldap, LDAP_ROOT_DSE,
								    LDAP_SCOPE_SUBTREE,
								    ldap_query,
								    (gchar **) search_attrs, 0,
								    NULL, /* XXX */
								    NULL, /* XXX */
								    NULL, /* XXX timeout */
								    view_limit,
								    &search_msgid);
					g_mutex_unlock (bl->priv->ldap_lock);
					d(printf("%s:%s: %d\n", G_STRLOC, G_STRFUNC, ldap_err));
				} else {
					g_mutex_unlock (bl->priv->ldap_lock);
					bl->priv->connected = FALSE;
				}
			} while (gal_reconnect (bl, view, ldap_err));

			g_free (ldap_query);

			if (ldap_err != LDAP_SUCCESS) {
				d(printf("%s:%s: error\n", G_STRLOC, G_STRFUNC));
				book_view_notify_status (view, ldap_err2string(ldap_err));
				return;
			}
			else if (search_msgid == -1) {
				d(printf("%s:%s: error\n", G_STRLOC, G_STRFUNC));
				book_view_notify_status (view,
							 _("Error performing search"));
				return;
			}
			else {
				LDAPSearchOp *op = g_new0 (LDAPSearchOp, 1);

				d(printf ("adding search_op (%p, %d)\n", view, search_msgid));
				op->view = view;
				op->aborted = FALSE;

				e_data_book_view_ref (view);

				ldap_op_add ((LDAPOp*)op, E_BOOK_BACKEND (bl), NULL, view,
					     0, search_msgid,
					     ldap_search_handler, ldap_search_dtor);
				d(printf("start_book_view: Setting op %p in book %p\n", op, view));
				g_object_set_data (G_OBJECT (view), "EBookBackendGAL.BookView::search_op", op);
			}
#if defined(ENABLE_CACHE) && ENABLE_CACHE
		}
#endif
	}
}

static void
stop_book_view (EBookBackend  *backend,
		EDataBookView *view)
{
	LDAPSearchOp *op;

	d(printf ("stop_book_view (%p)\n", view));

	op = g_object_get_data (G_OBJECT (view), "EBookBackendGAL.BookView::search_op");
	d(printf("STOP BOOK VIEW: Getting op %p from view %p\n", op, view));
	if (op) {
		op->aborted = TRUE;
		ldap_op_finished ((LDAPOp*)op);
		g_free (op);
	}
}

static void
get_changes (EBookBackend *backend,
	     EDataBook    *book,
	     guint32	   opid,
	     const gchar   *change_id)
{
	/* FIXME: implement */
}

#if defined(ENABLE_CACHE) && ENABLE_CACHE
static gint pagedResults = 1;
static ber_int_t pageSize = 1000;
static ber_int_t entriesLeft = 0;
static ber_int_t morePagedResults = 1;
static struct berval cookie = { 0, NULL };
static gint npagedresponses;
static gint npagedentries;
static gint npagedreferences;
static gint npagedextended;
static gint npagedpartial;

/* Set server controls.  Add controls extra_c[0..count-1], if set. */
static void
tool_server_controls( LDAP *ld, LDAPControl *extra_c, gint count )
{
	gint i = 0, j, crit = 0, err;
	LDAPControl c[3], **ctrls;

	ctrls = (LDAPControl**) malloc(sizeof(c) + (count+1)*sizeof(LDAPControl*));
	if (ctrls == NULL) {
		fprintf( stderr, "No memory\n" );
		exit( EXIT_FAILURE );
	}

	while (count--) {
		ctrls[i++] = extra_c++;
	}
	ctrls[i] = NULL;

	err = ldap_set_option( ld, LDAP_OPT_SERVER_CONTROLS, ctrls );

	if (err != LDAP_SUCCESS) {
		for (j = 0; j < i; j++) {
			if (ctrls[j]->ldctl_iscritical) crit = 1;
		}
		fprintf( stderr, "Could not set %scontrols\n",
			crit ? "critical " : "" );
	}

	free( ctrls );
	if (crit) {
		exit( EXIT_FAILURE );
	}
}

#if defined (SUNLDAP) || defined (G_OS_WIN32)
static struct berval *
ber_dupbv( struct berval *dst, struct berval *src )
{
	struct berval *tmp;

	tmp = ber_bvdup(src);
	if (!tmp)
		return NULL;

	dst->bv_len = tmp->bv_len;
	dst->bv_val = tmp->bv_val;
	tmp->bv_len = 0;
	tmp->bv_val = NULL;
	ber_bvfree (tmp);

	return dst;
}
#endif

static gint
parse_page_control(
	LDAP *ld,
	LDAPMessage *result,
	struct berval *cookie )
{
	gint rc;
	gint err;
	LDAPControl **ctrl = NULL;
	LDAPControl *ctrlp = NULL;
	BerElement *ber;
	ber_tag_t tag;
	struct berval servercookie = { 0, NULL };

	rc = ldap_parse_result( ld, result,
		&err, NULL, NULL, NULL, &ctrl, 0 );

	if (rc != LDAP_SUCCESS) {
		ldap_perror(ld, "ldap_parse_result");
		exit( EXIT_FAILURE );
	}

	if (err != LDAP_SUCCESS) {
		fprintf( stderr, "Error: %s (%d)\n", ldap_err2string(err), err );
	}

	if (ctrl) {
		/* Parse the control value
		 * searchResult ::= SEQUENCE {
		 *		size	INTEGER (0..maxInt),
		 *				-- result set size estimate from server - unused
		 *		cookie	OCTET STRING
		 * }
		 */
		ctrlp = *ctrl;
		ber = ber_init( &ctrlp->ldctl_value );
		if (ber == NULL) {
			fprintf( stderr, "Internal error.\n");
			return EXIT_FAILURE;
		}

		tag = ber_scanf( ber, "{im}", &entriesLeft, &servercookie );
		ber_dupbv( cookie, &servercookie );
		(void) ber_free( ber, 1 );

		if (tag == LBER_ERROR) {
			fprintf( stderr,
				"Paged results response control could not be decoded.\n");
			return EXIT_FAILURE;
		}

		if (entriesLeft < 0) {
			fprintf( stderr,
				"Invalid entries estimate in paged results response.\n");
			return EXIT_FAILURE;
		}
		ldap_controls_free( ctrl );

	} else {
		morePagedResults = 0;
	}
	if (cookie->bv_len>0) {
		d(printf ("\n"));
	}
	else {
		morePagedResults = 0;
	}

	return err;
}

static gint dosearch(
	EBookBackendGAL *bl,
	const gchar *base,
	gint scope,
	gchar *filtpatt,
	gchar *value,
	gchar **attrs,
	gint attrsonly,
	LDAPControl **sctrls,
	LDAPControl **cctrls,
	struct timeval *timeout,
	const gchar *changed_filter,
	gint sizelimit )
{
	gint			rc;
	LDAPMessage		*res, *msg;
	ber_int_t		msgid;
	static gint count = 0;
	gchar *ssize = getenv("LDAP_LIMIT");
	gint size = 0;

	if (ssize && *ssize)
		size = atoi(ssize);

	g_mutex_lock (bl->priv->ldap_lock);
	rc = ldap_search_ext (bl->priv->ldap, base, scope, value, attrs, attrsonly,
		sctrls, cctrls, timeout, size /*LDAP_NO_LIMIT*/, &msgid );
	g_mutex_unlock (bl->priv->ldap_lock);

	if (rc != LDAP_SUCCESS) {
		return( rc );
	}

	res = NULL;

	g_mutex_lock (bl->priv->ldap_lock);
	while ((rc = ldap_result (bl->priv->ldap, LDAP_RES_ANY,
		0,
		NULL, &res )) > 0 )
	{
		for (msg = ldap_first_message (bl->priv->ldap, res);
			msg != NULL;
			msg = ldap_next_message (bl->priv->ldap, msg ) )
		{
			EContact *contact;
			const gchar *uid;

			switch (ldap_msgtype( msg )) {
			case LDAP_RES_SEARCH_ENTRY:
				count++;
				g_mutex_unlock (bl->priv->ldap_lock);
				contact = build_contact_from_entry (bl, msg, NULL);
				uid = e_contact_get_const (contact, E_CONTACT_UID);

				g_mutex_lock (bl->priv->ldap_lock);
				if (changed_filter && e_book_backend_summary_check_contact (bl->priv->summary, uid)) {
					gboolean status;

					/* This is a delta sync. So, lets delete if we have the same object on the cache,
					 * so that we can update the new ones. */
					e_book_backend_summary_remove_contact (bl->priv->summary, uid);
					status = e_book_backend_db_cache_remove_contact (bl->priv->file_db, uid);
					if (status)
						printf("Updating contact with uid %s from the server\n", uid);
				} else
					printf("New contact with uid %s, add to the DB\n", uid);
				e_book_backend_db_cache_add_contact (bl->priv->file_db, contact);
				e_book_backend_summary_add_contact (bl->priv->summary, contact);
				g_object_unref (contact);
				break;

			case LDAP_RES_SEARCH_RESULT:
				if (pageSize != 0) {
					/* we hold the lock already */
					rc = parse_page_control (bl->priv->ldap, msg, &cookie);
				}

				g_mutex_unlock (bl->priv->ldap_lock);
				goto done;
			}

		}

		ldap_msgfree( res );
	}
	g_mutex_unlock (bl->priv->ldap_lock);

	if (rc == -1) {
		g_mutex_lock (bl->priv->ldap_lock);
		ldap_perror (bl->priv->ldap, "ldap_result");
		g_mutex_unlock (bl->priv->ldap_lock);
		return( rc );
	}

done:
	ldap_msgfree( res );
	return( rc );
}

static void
generate_cache (EBookBackendGAL *book_backend_gal, const gchar * changed_filter)
{
	LDAPGetContactListOp *contact_list_op = g_new0 (LDAPGetContactListOp, 1);
	EBookBackendGALPrivate *priv;
	gchar *ldap_query;
	gint  i = 0, rc;
	BerElement *prber = NULL;
	gchar t[15], *cachetime;
	LDAPControl c[6];
	GError *error = NULL;

	d(printf ("Generate Cache\n"));
	priv = book_backend_gal->priv;

	cachetime = e_book_backend_db_cache_get_time (priv->file_db);

	priv->cache_time = cachetime ? atoi(cachetime) : 0;
	g_free(cachetime);
	npagedresponses = npagedentries = npagedreferences =
		npagedextended = npagedpartial = 0;

	build_query (book_backend_gal,
		     "(beginswith \"file_as\" \"\")", changed_filter, &ldap_query, &error);

getNextPage:

	/*start iteration*/

	i = 0;
	if (pagedResults) {
#ifdef G_OS_WIN32
		struct berval **tmpBVPtr = NULL;
#endif
		if (( prber = ber_alloc_t(LBER_USE_DER)) == NULL ) {
			return;
		}
		ber_printf( prber, "{iO}", pageSize, &cookie );
#ifdef G_OS_WIN32
		if (ber_flatten( prber, tmpBVPtr) == -1) {
			ber_free( prber, 1 );
			ber_bvfree(*tmpBVPtr);
			return;
		}
		c[i].ldctl_value = **tmpBVPtr;
		ber_free( prber, 1 );
		ber_bvfree(*tmpBVPtr);
#else
		if (ber_flatten2( prber, &c[i].ldctl_value, 0 ) == -1) {
			return;
		}
#endif
		d(printf ("Setting parameters		\n"));
		c[i].ldctl_oid = (gchar *) LDAP_CONTROL_PAGEDRESULTS;
		c[i].ldctl_iscritical = pagedResults > 1;
		i++;
	}

	g_mutex_lock (priv->ldap_lock);
	tool_server_controls( priv->ldap, c, i );
	g_mutex_unlock (priv->ldap_lock);
	ber_free (prber, 1);

	g_mutex_lock (priv->ldap_lock);
	if (!priv->ldap) {
		g_mutex_unlock (priv->ldap_lock);
		g_free (contact_list_op);
		return;
	}
	g_mutex_unlock (priv->ldap_lock);

	rc = dosearch (book_backend_gal, LDAP_ROOT_DSE, LDAP_SCOPE_SUBTREE, NULL, ldap_query, NULL, 0, NULL, NULL, NULL, changed_filter, -1);

	/* loop to get the next set of entries */

	if ((pageSize !=0) && (morePagedResults != 0)) {
		d(printf ("Start next iteration\n"));
		goto getNextPage;
	} else {
		d(printf ("All the entries fetched and finished building the cache\n"));
	}

	/* Set the cache to populated and thaw the changes */

	e_book_backend_db_cache_set_populated (priv->file_db);
	if (priv->cache_time != priv->last_best_time)
		priv->last_best_time++;
	g_sprintf (t," %d", (gint)priv->last_best_time);
	printf("All done, cached time set to %d, %s(%d)\n", (gint) priv->last_best_time, ctime (&priv->last_best_time), (gint) priv->cache_time);
	e_book_backend_db_cache_set_time (priv->file_db, t);
	priv->is_summary_ready = TRUE;
	book_backend_gal->priv->file_db->sync (book_backend_gal->priv->file_db, 0);

	g_free (ldap_query);

}

static void
update_cache (EBookBackendGAL *gal)
{
	time_t t1;
	gchar *t = e_book_backend_db_cache_get_time (gal->priv->file_db);
	gchar *filter, *galtime;
	struct tm *tm;

	printf("Cache is populated, Refresh now... \n");
	if (t && *t)
		t1 = atoi (t);
	else
		t1=0;
	if (t1 == 0) {
		generate_cache (gal, NULL);
		return;
	}
	gal->priv->last_best_time = t1;
	tm = localtime (&t1);
	galtime = g_strdup_printf("%04d%02d%02d%02d%02d%02d.0Z",tm->tm_year+1900, tm->tm_mon+1,tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);

	filter = g_strdup_printf ("|(whenCreated>=%s)(whenChanged>=%s)", galtime, galtime);

	g_free(galtime);
	printf("Filter %s: Time %d\n", filter, (gint) t1);
	/* Download New contacts */
	generate_cache (gal, filter);

	/* TODO: Sync deleted contacts */
	g_free  (filter);
}
#endif

static void
authenticate_user (EBookBackend *backend,
		   EDataBook    *book,
		   guint32       opid,
		   const gchar   *user,
		   const gchar   *password,
		   const gchar   *auth_method)
{
	EBookBackendGAL *be = E_BOOK_BACKEND_GAL (backend);
	EBookBackendGALPrivate *bepriv = be->priv;
	ExchangeAccountResult result;
	ExchangeAccount *account = NULL;
	GError *err = NULL;
#if defined(ENABLE_CACHE) && ENABLE_CACHE
	GConfClient *gc = gconf_client_get_default();
	gint interval = gconf_client_get_int (gc, "/apps/evolution/addressbook/gal_cache_interval", NULL);

	g_object_unref (gc);
#endif

	/* We should not be here */
/*	e_data_book_respond_authenticate_user (book, */
/*					       opid, */
/*					       EDB_ERROR (UNSUPPORTED_AUTHENTICATION_METHOD)); */
/*	return; */

	d(printf("authenticate_user(%p, %p, %s, %s, %s)\n", backend, book, user, password, auth_method));

	switch (bepriv->mode) {

	case E_DATA_BOOK_MODE_LOCAL:
		e_book_backend_notify_writable (E_BOOK_BACKEND (backend), FALSE);
		e_book_backend_notify_connection_status (E_BOOK_BACKEND (backend), FALSE);
		e_data_book_respond_authenticate_user (book, opid, NULL /* Success */);
		return;

	case E_DATA_BOOK_MODE_REMOTE:

		account = exchange_share_config_listener_get_account_for_uri (NULL, bepriv->gal_uri);
		/* FIXME : Check for failures */
		if (!exchange_account_get_context (account)) {
			exchange_account_set_online (account);
			if (!exchange_account_connect (account, password, &result)) {
				d(printf("%s:%s: failed\n", G_STRLOC, G_STRFUNC));
				e_data_book_respond_authenticate_user (book, opid, EDB_ERROR (AUTHENTICATION_FAILED));
				return;
			}
		}

		if (!gal_connect (be, &err)) {
			e_data_book_respond_authenticate_user (book, opid, err);
			return;
		}
#if defined(ENABLE_CACHE) && ENABLE_CACHE
		if (be->priv->marked_for_offline) {
			if (e_book_backend_db_cache_is_populated (be->priv->file_db) ) {
				time_t t1, t2;
				gint diff;

				gchar *t = e_book_backend_db_cache_get_time (be->priv->file_db);
				d(printf("Cache is populated, check if refresh is required \n"));
				if (t && *t)
					t1 = atoi (t);
				else
					t1=0;
				t2 = time (NULL);
				diff = interval * 24 * 60 *60;
				/* We have a day specified, then we cache it. */
				//if (!diff || t2 - t1 > diff) {
				//	d(printf ("Cache older than specified period, refreshing \n"));
					update_cache (be);
				//}
				//else
				//	be->priv->is_summary_ready= TRUE;
			}
			else {
				d(printf("Cache not there, generate cache\n"));
				generate_cache(be, NULL);
			}
		}
#endif
		e_data_book_respond_authenticate_user (book, opid, NULL /* Success*/ );
		return;

	default:
		break;
	}

	/* We should not be here */
	e_data_book_respond_authenticate_user (book,
					       opid,
					       EDB_ERROR (UNSUPPORTED_AUTHENTICATION_METHOD));
	return;
}

#ifdef SUNLDAP
static gint
ber_flatten2( BerElement *ber, struct berval *bv, gint alloc )
{
	struct berval *tmp;

	if (ber_flatten( ber, &tmp) == -1) {
		return;
	}
	bv->bv_len = tmp->bv_len;
	bv->bv_val = tmp->bv_val;
	tmp->bv_len = 0;
	tmp->bv_val = NULL;
	ber_bvfree (tmp);

	return 0;
}
#endif

static void
ldap_cancel_op(gpointer key, gpointer value, gpointer data)
{
	EBookBackendGAL *bl = data;
	LDAPOp *op = value;

	/* ignore errors, its only best effort? */
	g_mutex_lock (bl->priv->ldap_lock);
	if (bl->priv->ldap)
		ldap_abandon (bl->priv->ldap, op->id);
	g_mutex_unlock (bl->priv->ldap_lock);
}

static void
cancel_operation (EBookBackend *backend, EDataBook *book, GError **perror)
{
	EBookBackendGAL *bl = E_BOOK_BACKEND_GAL (backend);

	g_static_rec_mutex_lock (&bl->priv->op_hash_mutex);
	g_hash_table_foreach (bl->priv->id_to_op, ldap_cancel_op, bl);
	g_static_rec_mutex_unlock (&bl->priv->op_hash_mutex);
}

static void
set_mode (EBookBackend *backend, EDataBookMode mode)
{
	EBookBackendGAL *be = E_BOOK_BACKEND_GAL (backend);
	EBookBackendGALPrivate *bepriv;

	bepriv = be->priv;

	if (bepriv->mode == mode)
		return;

	bepriv->mode = mode;

	/* Cancel all running operations */
	cancel_operation (backend, NULL, NULL);

	if (e_book_backend_is_loaded (backend)) {
		if (mode == E_DATA_BOOK_MODE_LOCAL) {
			e_book_backend_set_is_writable (backend, FALSE);
			e_book_backend_notify_writable (backend, FALSE);
			e_book_backend_notify_connection_status (backend, FALSE);
		} else if (mode == E_DATA_BOOK_MODE_REMOTE) {
			e_book_backend_set_is_writable (backend, FALSE);
			e_book_backend_notify_writable (backend, FALSE);
			e_book_backend_notify_connection_status (backend, TRUE);

			if (e_book_backend_is_loaded (backend)) {
				gal_connect (be, NULL);
				e_book_backend_notify_auth_required (backend);
#if defined(ENABLE_CACHE) && ENABLE_CACHE
				if (bepriv->marked_for_offline && bepriv->file_db) {
					if (e_book_backend_db_cache_is_populated (be->priv->file_db))
						update_cache (be);
					else
						generate_cache (be, NULL);
				}
#endif
			}
		}
	}
}

static void
get_supported_fields (EBookBackend *backend,
		      EDataBook    *book,
		      guint32	    opid)

{
	e_data_book_respond_get_supported_fields (book,
						  opid,
						  NULL /* Success */,
						  supported_fields);
}

static void
get_required_fields (EBookBackend *backend,
		     EDataBook *book,
		     guint32 opid)
{
	GList *fields = NULL;

	fields = g_list_append (fields, (gchar *) e_contact_field_name (E_CONTACT_FILE_AS));
	e_data_book_respond_get_required_fields (book,
						  opid,
						  NULL /* Success */,
						  fields);
	g_list_free (fields);

}

static void
get_supported_auth_methods (EBookBackend *backend,
			    EDataBook    *book,
			    guint32       opid)

{
	d(printf("%s:%s: NONE\n", G_STRLOC, G_STRFUNC));
	e_data_book_respond_get_supported_auth_methods (book,
							opid,
							NULL /* Success */,
							NULL);
}

static void
load_source (EBookBackend *backend,
	     ESource      *source,
	     gboolean      only_if_exists,
	     GError      **error)
{
	EBookBackendGAL *bl = E_BOOK_BACKEND_GAL (backend);
	const gchar *host;
	gchar **tokens;
	const gchar *offline;
	gchar *uri;
	gchar *book_name = NULL;
	gint i;
#if defined(ENABLE_CACHE) && ENABLE_CACHE
	const gchar *cache_dir;
	gchar *dirname, *filename;
	gint db_error;
	DB *db;
	DB_ENV *env;
#endif
	e_return_data_book_error_if_fail (bl->priv->connected == FALSE, E_DATA_BOOK_STATUS_OTHER_ERROR);

	offline = e_source_get_property (source, "offline_sync");
	if (offline && g_str_equal (offline, "1"))
		bl->priv->marked_for_offline = TRUE;

	if (bl->priv->mode ==  E_DATA_BOOK_MODE_LOCAL &&
	    !bl->priv->marked_for_offline) {
		g_propagate_error (error, EDB_ERROR (OFFLINE_UNAVAILABLE));
		return;
	}

	uri = e_source_get_uri (source);
	host = uri + sizeof ("gal://") - 1;
	if (strncmp (uri, "gal://", host - uri)) {
		g_propagate_error (error, EDB_ERROR_EX (OTHER_ERROR, "Not a gal:// URI"));
		return;
	}

	bl->priv->gal_uri = g_strdup (uri);
	tokens = g_strsplit (uri, ";", 2);
	if (tokens[0]) {
		g_free (uri);
		uri = g_strdup (tokens[0]);
		book_name = g_strdup (tokens[1]);
	}
	g_strfreev (tokens);

	for (i=0; i< strlen (uri); i++) {
		switch (uri[i]) {
		case ':' :
		case '/' :
			uri[i] = '_';
		}
	}
#if defined(ENABLE_CACHE) && ENABLE_CACHE
	bl->priv->file_db = NULL;
#endif
	if (bl->priv->mode == E_DATA_BOOK_MODE_LOCAL && !bl->priv->marked_for_offline) {
		/* Offline */

		e_book_backend_set_is_loaded (backend, FALSE);
		e_book_backend_set_is_writable (backend, FALSE);
		e_book_backend_notify_writable (backend, FALSE);
		e_book_backend_notify_connection_status (backend, FALSE);

		g_free (book_name);
		g_free (uri);

		g_propagate_error (error, EDB_ERROR (REPOSITORY_OFFLINE));
		return;
	}
		d(printf("offlin==============\n"));
#if defined(ENABLE_CACHE) && ENABLE_CACHE
	if (bl->priv->marked_for_offline) {
		d(printf("offlin==============\n"));
		cache_dir = e_book_backend_get_cache_dir (backend);
		bl->priv->summary_file_name = g_build_filename (cache_dir, book_name, NULL);
		bl->priv->summary_file_name = g_build_filename (bl->priv->summary_file_name, "cache.summary", NULL);
		bl->priv->summary = e_book_backend_summary_new (bl->priv->summary_file_name,
							    SUMMARY_FLUSH_TIMEOUT);
		e_book_backend_summary_load (bl->priv->summary);

		dirname = g_build_filename (cache_dir, book_name, NULL);
		filename = g_build_filename (dirname, "cache.db", NULL);

		g_free (book_name);
		g_free (uri);

		db_error = e_db3_utils_maybe_recover (filename);
		if (db_error != 0) {
			g_warning ("db recovery failed with %d", db_error);
			g_free (dirname);
			g_free (filename);
			g_propagate_error (error, EDB_ERROR (OTHER_ERROR));
			return;
		}

		g_static_mutex_lock (&global_env_lock);
		if (global_env.ref_count > 0) {
			env = global_env.env;
			global_env.ref_count++;
		}
		else {
			db_error = db_env_create (&env, 0);
			if (db_error != 0) {
				g_warning ("db_env_create failed with %d", db_error);
				g_static_mutex_unlock (&global_env_lock);
				g_free (dirname);
				g_free (filename);
				g_propagate_error (error, EDB_ERROR (OTHER_ERROR));
				return;
			}

			db_error = (*env->open) (env, NULL, DB_CREATE | DB_INIT_MPOOL | DB_PRIVATE | DB_THREAD, 0);
			if (db_error != 0) {
				env->close (env, 0);
				g_warning ("db_env_open failed with %d", db_error);
				g_static_mutex_unlock (&global_env_lock);
				g_free(dirname);
				g_free(filename);
				g_propagate_error (error, EDB_ERROR (OTHER_ERROR));
				return;
			}

			//env->set_errcall (env, file_errcall);
			global_env.env = env;
			global_env.ref_count = 1;
		}
		g_static_mutex_unlock(&global_env_lock);

		bl->priv->env = env;
		db_error = db_create (&db, env, 0);
		if (db_error != 0) {
			g_warning ("db_create failed with %d", db_error);
			g_free (dirname);
			g_free (filename);
			g_propagate_error (error, EDB_ERROR (OTHER_ERROR));
			return;
		}

		db_error = (*db->open) (db, NULL, filename, NULL, DB_HASH, DB_THREAD, 0666);

		if (db_error == DB_OLD_VERSION) {
			db_error = e_db3_utils_upgrade_format (filename);

			if (db_error != 0) {
				g_warning ("db format upgrade failed with %d", db_error);
				g_free (filename);
				g_free (dirname);
				g_propagate_error (error, EDB_ERROR (OTHER_ERROR));
				return;
			}

			db_error = (*db->open) (db, NULL,filename, NULL, DB_HASH, DB_THREAD, 0666);
		}

		bl->priv->file_db = db;
		if (db_error != 0) {
			gint rv;

			/* the database didn't exist, so we create the directory then the .db */
			rv= g_mkdir_with_parents (dirname, 0777);
			if (rv == -1 && errno != EEXIST) {
				g_warning ("failed to make directory %s: %s", dirname, strerror (errno));
				g_free (dirname);
				g_free (filename);
				if (errno == EACCES || errno == EPERM) {
					g_propagate_error (error, EDB_ERROR (PERMISSION_DENIED));
					return;
				} else {
					g_propagate_error (error, EDB_ERROR (OTHER_ERROR));
					return;
				}
			}

			db_error = (*db->open) (db, NULL, filename, NULL, DB_HASH, DB_CREATE | DB_THREAD, 0666);
			if (db_error != 0) {
				g_warning ("db->open (...DB_CREATE...) failed with %d", db_error);
			}
		}

		bl->priv->file_db = db;

		if (db_error != 0 || bl->priv->file_db == NULL) {

			g_free (filename);
			g_free (dirname);
			g_propagate_error (error, EDB_ERROR (OTHER_ERROR));
			return;
		}

		e_book_backend_db_cache_set_filename (bl->priv->file_db, filename);
		g_free (filename);
		g_free (dirname);
	} else {
		g_free (book_name);
		g_free (uri);
	}
#endif
	/* Online */
	e_book_backend_set_is_writable (E_BOOK_BACKEND(backend), FALSE);
	e_book_backend_set_is_loaded (E_BOOK_BACKEND (backend), TRUE);
	e_book_backend_notify_writable (backend, FALSE);

	if (bl->priv->mode == E_DATA_BOOK_MODE_LOCAL)
		e_book_backend_notify_connection_status (E_BOOK_BACKEND (backend), FALSE);
	else
		e_book_backend_notify_connection_status (E_BOOK_BACKEND (backend), TRUE);
}

static void
remove_gal (EBookBackend *backend, EDataBook *book, guint32 opid)
{
	e_data_book_respond_remove (book, opid, EDB_ERROR (PERMISSION_DENIED));
}

static gchar *
get_static_capabilities (EBookBackend *backend)
{
	if (can_browse (backend))
		return g_strdup ("net,do-initial-query");
	else
		return g_strdup ("net");
}

/**
 * e_book_backend_gal_new:
 *
 * Creates a new #EBookBackendGAL.
 *
 * Return value: the new #EBookBackendGAL.
 */
EBookBackend *
e_book_backend_gal_new (void)
{
	return g_object_new (E_TYPE_BOOK_BACKEND_GAL, NULL);
}

static gboolean
call_dtor (gint msgid, LDAPOp *op, gpointer data)
{
	EBookBackendGAL *bl = E_BOOK_BACKEND_GAL (op->backend);

	g_mutex_lock (bl->priv->ldap_lock);
	ldap_abandon (bl->priv->ldap, op->id);
	g_mutex_unlock (bl->priv->ldap_lock);

	op->dtor (op);

	return TRUE;
}

static void
dispose (GObject *object)
{
	EBookBackendGAL *bl = E_BOOK_BACKEND_GAL (object);

	if (bl->priv) {
		g_static_rec_mutex_lock (&bl->priv->op_hash_mutex);
		g_hash_table_foreach_remove (bl->priv->id_to_op, (GHRFunc)call_dtor, NULL);
		g_hash_table_destroy (bl->priv->id_to_op);
		g_static_rec_mutex_unlock (&bl->priv->op_hash_mutex);
		g_static_rec_mutex_free (&bl->priv->op_hash_mutex);

		if (bl->priv->poll_timeout != -1) {
			d(printf ("removing timeout\n"));
			g_source_remove (bl->priv->poll_timeout);
		}

		g_mutex_lock (bl->priv->ldap_lock);
		if (bl->priv->ldap)
			ldap_unbind (bl->priv->ldap);
		g_mutex_unlock (bl->priv->ldap_lock);

		if (bl->priv->gc)
			g_object_unref (bl->priv->gc);

		if (bl->priv->summary_file_name) {
			g_free (bl->priv->summary_file_name);
			bl->priv->summary_file_name = NULL;
		}

		if (bl->priv->summary) {
			e_book_backend_summary_save (bl->priv->summary);
			g_object_unref (bl->priv->summary);
			bl->priv->summary = NULL;
		}
#if defined(ENABLE_CACHE) && ENABLE_CACHE
		if (bl->priv->file_db)
			bl->priv->file_db->close (bl->priv->file_db, 0);
		g_static_mutex_lock (&global_env_lock);
		global_env.ref_count--;
		if (global_env.ref_count == 0) {
			global_env.env->close (global_env.env, 0);
			global_env.env = NULL;
		}
		g_static_mutex_unlock(&global_env_lock);

#endif
		if (bl->priv->ldap_lock)
			g_mutex_free (bl->priv->ldap_lock);

		g_free (bl->priv->gal_uri);
		g_free (bl->priv);
		bl->priv = NULL;
	}

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
class_init (EBookBackendGALClass *klass)
{
	GObjectClass  *object_class = G_OBJECT_CLASS (klass);
	EBookBackendClass *backend_class = E_BOOK_BACKEND_CLASS (klass);
	gint i;

	parent_class = g_type_class_peek_parent (klass);

	/* Set the virtual methods. */
	backend_class->load_source                = load_source;
	backend_class->remove                     = remove_gal;
	backend_class->get_static_capabilities    = get_static_capabilities;

	backend_class->create_contact             = create_contact;
	backend_class->remove_contacts            = remove_contacts;
	backend_class->modify_contact             = modify_contact;
	backend_class->get_contact                = get_contact;
	backend_class->get_contact_list           = get_contact_list;
	backend_class->start_book_view            = start_book_view;
	backend_class->stop_book_view             = stop_book_view;
	backend_class->get_changes                = get_changes;
	backend_class->authenticate_user          = authenticate_user;
	backend_class->get_supported_fields       = get_supported_fields;
	backend_class->set_mode                   = set_mode;
	backend_class->get_required_fields        = get_required_fields;
	backend_class->get_supported_auth_methods = get_supported_auth_methods;
	backend_class->cancel_operation           = cancel_operation;

	object_class->dispose = dispose;

	/* Set up static data */
	supported_fields = NULL;
	for (i = 0; i < G_N_ELEMENTS (prop_info); i++) {
		supported_fields = g_list_append (supported_fields,
						  (gchar *)e_contact_field_name (prop_info[i].field_id));
	}
	supported_fields = g_list_append (supported_fields, (gpointer) "file_as");

	search_attrs = g_new (const gchar *, G_N_ELEMENTS (prop_info) + 1);
	for (i = 0; i < G_N_ELEMENTS (prop_info); i++)
		search_attrs[i] = prop_info[i].ldap_attr;
	search_attrs[G_N_ELEMENTS (prop_info)] = NULL;
}

static void
init (EBookBackendGAL *backend)
{
	EBookBackendGALPrivate *priv;

	priv                         = g_new0 (EBookBackendGALPrivate, 1);

	priv->id_to_op		     = g_hash_table_new (g_int_hash, g_int_equal);
	priv->poll_timeout	     = -1;
	priv->ldap_lock		     = g_mutex_new ();

	g_static_rec_mutex_init (&priv->op_hash_mutex);

	backend->priv = priv;
#if defined(ENABLE_CACHE) && ENABLE_CACHE
	priv->last_best_time = 0;
	priv->cache_time = 0;
#endif
}

E2K_MAKE_TYPE (e_book_backend_gal, EBookBackendGAL, class_init, init, PARENT_TYPE)
