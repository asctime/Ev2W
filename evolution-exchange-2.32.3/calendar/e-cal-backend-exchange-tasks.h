/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* Copyright (C) 2000-2004 Novell, Inc. */

#ifndef E_CAL_BACKEND_EXCHANGE_TASKS_H
#define E_CAL_BACKEND_EXCHANGE_TASKS_H

#include "e-cal-backend-exchange.h"

G_BEGIN_DECLS

#define E_TYPE_CAL_BACKEND_EXCHANGE_TASKS            (e_cal_backend_exchange_tasks_get_type ())
#define E_CAL_BACKEND_EXCHANGE_TASKS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_CAL_BACKEND_EXCHANGE_TASKS, ECalBackendExchangeTasks))
#define E_CAL_BACKEND_EXCHANGE_TASKS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_CAL_BACKEND_EXCHANGE_TASKS, ECalBackendExchangeTasksClass))
#define E_IS_CAL_BACKEND_EXCHANGE_TASKS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_CAL_BACKEND_EXCHANGE_TASKS))
#define E_IS_CAL_BACKEND_EXCHANGE_TASKS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), E_TYPE_CAL_BACKEND_EXCHANGE_TASKS))
#define E_CAL_BACKEND_EXCHANGE_TASKS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), E_TYPE_CAL_BACKEND_EXCHANGE_TASKS, ECalBackendExchangeTasksClass))

typedef struct _ECalBackendExchangeTasks ECalBackendExchangeTasks;
typedef struct _ECalBackendExchangeTasksClass ECalBackendExchangeTasksClass;

typedef struct _ECalBackendExchangeTasksPrivate ECalBackendExchangeTasksPrivate;

struct _ECalBackendExchangeTasks {
	ECalBackendExchange parent;

	ECalBackendExchangeTasksPrivate *priv;
};

struct _ECalBackendExchangeTasksClass {
	ECalBackendExchangeClass parent_class;

};

GType    e_cal_backend_exchange_tasks_get_type         (void);
icaltimezone * get_default_timezone (void);
gchar * calcomponentdatetime_to_string (ECalComponentDateTime *dt, icaltimezone *izone);
gchar * icaltime_to_e2k_time (struct icaltimetype *itt);

G_END_DECLS

#endif
