/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* Copyright (C) 2000-2004 Novell, Inc. */

#ifndef E_CAL_BACKEND_EXCHANGE_CALENDAR_H
#define E_CAL_BACKEND_EXCHANGE_CALENDAR_H

#include "e-cal-backend-exchange.h"

G_BEGIN_DECLS

#define E_TYPE_CAL_BACKEND_EXCHANGE_CALENDAR            (e_cal_backend_exchange_calendar_get_type ())
#define E_CAL_BACKEND_EXCHANGE_CALENDAR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_CAL_BACKEND_EXCHANGE_CALENDAR, ECalBackendExchangeCalendar))
#define E_CAL_BACKEND_EXCHANGE_CALENDAR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_CAL_BACKEND_EXCHANGE_CALENDAR, ECalBackendExchangeCalendarClass))
#define E_IS_CAL_BACKEND_EXCHANGE_CALENDAR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_CAL_BACKEND_EXCHANGE_CALENDAR))
#define E_IS_CAL_BACKEND_EXCHANGE_CALENDAR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), E_TYPE_CAL_BACKEND_EXCHANGE_CALENDAR))
#define E_CAL_BACKEND_EXCHANGE_CALENDAR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), E_TYPE_CAL_BACKEND_EXCHANGE_CALENDAR, ECalBackendExchangeCalendarClass))

typedef struct ECalBackendExchangeCalendar ECalBackendExchangeCalendar;
typedef struct ECalBackendExchangeCalendarClass ECalBackendExchangeCalendarClass;

typedef struct ECalBackendExchangeCalendarPrivate ECalBackendExchangeCalendarPrivate;

struct ECalBackendExchangeCalendar {
	ECalBackendExchange parent;

	ECalBackendExchangeCalendarPrivate *priv;
};

struct ECalBackendExchangeCalendarClass {
	ECalBackendExchangeClass parent_class;

};

GType    e_cal_backend_exchange_calendar_get_type         (void);

G_END_DECLS

#endif
