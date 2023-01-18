/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* Copyright (C) 2000-2004 Novell, Inc. */

#ifndef E_CAL_BACKEND_EXCHANGE_H
#define E_CAL_BACKEND_EXCHANGE_H

#include <libedata-cal/e-cal-backend-sync.h>
#include <libedataserver/e-xml-hash-utils.h>

#include <exchange-types.h>
#include <e-folder.h>

G_BEGIN_DECLS

#define EDC_ERROR(_code) e_data_cal_create_error (_code, NULL)
#define EDC_ERROR_EX(_code, _msg) e_data_cal_create_error (_code, _msg)
#define EDC_ERROR_HTTP_STATUS(_status) e_data_cal_create_error_fmt (OtherError, _("Failed with E2K HTTP status %d"), _status)

#define E_TYPE_CAL_BACKEND_EXCHANGE            (e_cal_backend_exchange_get_type ())
#define E_CAL_BACKEND_EXCHANGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_CAL_BACKEND_EXCHANGE, ECalBackendExchange))
#define E_CAL_BACKEND_EXCHANGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_CAL_BACKEND_EXCHANGE, ECalBackendExchangeClass))
#define E_IS_CAL_BACKEND_EXCHANGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_CAL_BACKEND_EXCHANGE))
#define E_IS_CAL_BACKEND_EXCHANGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), E_TYPE_CAL_BACKEND_EXCHANGE))
#define E_CAL_BACKEND_EXCHANGE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), E_TYPE_CAL_BACKEND_EXCHANGE, ECalBackendExchangeClass))

typedef struct ECalBackendExchange ECalBackendExchange;
typedef struct ECalBackendExchangeClass ECalBackendExchangeClass;

typedef struct ECalBackendExchangeComponent ECalBackendExchangeComponent;

typedef struct ECalBackendExchangePrivate ECalBackendExchangePrivate;

struct ECalBackendExchange {
	ECalBackendSync parent;

	ECalBackendExchangePrivate *priv;

	ExchangeAccount *account;
	EFolder *folder;
	E2kRestriction *private_item_restriction;

};

struct ECalBackendExchangeClass {
	ECalBackendSyncClass parent_class;

};

struct ECalBackendExchangeComponent {
	gchar *uid, *href, *lastmod;
	icalcomponent *icomp;
	GList *instances;
};

GType     e_cal_backend_exchange_get_type         (void);

void      e_cal_backend_exchange_cache_sync_start (ECalBackendExchange *cbex);
gboolean  e_cal_backend_exchange_in_cache         (ECalBackendExchange *cbex,
						   const gchar          *uid,
						   const gchar          *lastmod,
						   const gchar	       *href,
						   const gchar	       *rid
						   );

void      e_cal_backend_exchange_cache_sync_end   (ECalBackendExchange *cbex);

gboolean  e_cal_backend_exchange_add_object       (ECalBackendExchange *cbex,
						   const gchar          *href,
						   const gchar          *lastmod,
						   icalcomponent       *comp);
gboolean  e_cal_backend_exchange_modify_object    (ECalBackendExchange *cbex,
						   icalcomponent       *comp,
						   CalObjModType mod,
						   gboolean remove_detached);
gboolean  e_cal_backend_exchange_remove_object    (ECalBackendExchange *cbex,
						   const gchar          *uid);

void  e_cal_backend_exchange_add_timezone     (ECalBackendExchange *cbex,
						   icalcomponent       *vtzcomp,
						   GError **perror);

icaltimezone * e_cal_backend_exchange_get_default_time_zone (ECalBackendSync *backend);

gchar *	  e_cal_backend_exchange_lf_to_crlf	(const gchar *in);
gchar *	  e_cal_backend_exchange_make_timestamp_rfc822	(time_t when);

/** lookup function for e_cal_check_timezones() */
icaltimezone *
e_cal_backend_exchange_lookup_timezone (const gchar *tzid,
					gconstpointer custom,
					GError **error);

ECalBackendExchangeComponent * get_exchange_comp (ECalBackendExchange *cbex,
						  const gchar *uid);

gboolean  e_cal_backend_exchange_extract_components (const gchar *calobj,
                                           icalproperty_method *method,
                                           GList **comp_list, GError **perror);

/* Utility functions */

void e_cal_backend_exchange_get_from (ECalBackendSync *backend, ECalComponent *comp,
					gchar **from_name, gchar **from_addr);
gchar * e_cal_backend_exchange_get_from_string (ECalBackendSync *backend, ECalComponent *comp);
void e_cal_backend_exchange_get_sender (ECalBackendSync *backend, ECalComponent *comp,
					gchar **from_name, gchar **from_addr);
gchar * e_cal_backend_exchange_get_sender_string (ECalBackendSync *backend, ECalComponent *comp);
gboolean e_cal_backend_exchange_is_online (ECalBackendExchange *cbex);
GSList * get_attachment (ECalBackendExchange *cbex, const gchar *uid, const gchar *body, gint len);
GSList *receive_attachments (ECalBackendExchange *cbex, ECalComponent *comp);
void process_delegated_cal_object (icalcomponent *icalcomp, const gchar *delegator_name,
					const gchar *delegator_email, const gchar *delegatee_email);
gchar * build_msg ( ECalBackendExchange *cbex, ECalComponent *comp, const gchar *subject, gchar **boundary);
gchar *e_cal_backend_exchange_get_owner_email (ECalBackendSync *backend);
gchar *e_cal_backend_exchange_get_owner_name (ECalBackendSync *backend);
void e_cal_backend_exchange_cache_lock (ECalBackendExchange *cbex);
void e_cal_backend_exchange_cache_unlock (ECalBackendExchange *cbex);
void e_cal_backend_exchange_ensure_utc_zone (ECalBackend *cb, struct icaltimetype *itt);

G_END_DECLS

#endif
