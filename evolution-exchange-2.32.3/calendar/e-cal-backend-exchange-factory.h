/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* e-cal-backend-exchange-factory.h
 *
 * Copyright (C) 2004  Novell, Inc.
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
 *
 */

#ifndef _E_CAL_BACKEND_EXCHANGE_FACTORY_H_
#define _E_CAL_BACKEND_EXCHANGE_FACTORY_H_

#include <glib-object.h>
#include <libedata-cal/e-cal-backend-factory.h>

/* Standard GObject macros */
#define E_TYPE_CAL_BACKEND_EXCHANGE_EVENTS_FACTORY \
	(e_cal_backend_exchange_events_factory_get_type ())
#define E_CAL_BACKEND_EXCHANGE_EVENTS_FACTORY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CAL_BACKEND_EXCHANGE_EVENTS_FACTORY, ECalBackendExchangeFactory))
#define E_CAL_BACKEND_EXCHANGE_EVENTS_FACTORY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CAL_BACKEND_EXCHANGE_EVENTS_FACTORY, ECalBackendExchangeFactoryClass))
#define E_IS_CAL_BACKEND_EXCHANGE_EVENTS_FACTORY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CAL_BACKEND_EXCHANGE_EVENTS_FACTORY))
#define E_IS_CAL_BACKEND_EXCHANGE_EVENTS_FACTORY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CAL_BACKEND_EXCHANGE_EVENTS_FACTORY))
#define E_CAL_BACKEND_EXCHANGE_EVENTS_FACTORY_GET_CLASS(cls) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CAL_BACKEND_EXCHANGE_EVENTS_FACTORY, ECalBackendExchangeFactoryClass))

/* Standard GObject macros */
#define E_TYPE_CAL_BACKEND_EXCHANGE_TODOS_FACTORY \
	(e_cal_backend_exchange_todos_factory_get_type ())
#define E_CAL_BACKEND_EXCHANGE_TODOS_FACTORY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CAL_BACKEND_EXCHANGE_TODOS_FACTORY, ECalBackendExchangeFactory))
#define E_CAL_BACKEND_EXCHANGE_TODOS_FACTORY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CAL_BACKEND_EXCHANGE_TODOS_FACTORY, ECalBackendExchangeFactoryClass))
#define E_IS_CAL_BACKEND_EXCHANGE_TODOS_FACTORY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CAL_BACKEND_EXCHANGE_TODOS_FACTORY))
#define E_IS_CAL_BACKEND_EXCHANGE_TODOS_FACTORY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CAL_BACKEND_EXCHANGE_TODOS_FACTORY))
#define E_CAL_BACKEND_EXCHANGE_TODOS_FACTORY_GET_CLASS(cls) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CAL_BACKEND_EXCHANGE_TODOS_FACTORY, ECalBackendExchangeFactoryClass))

G_BEGIN_DECLS

typedef struct _ECalBackendExchangeFactory ECalBackendExchangeFactory;
typedef struct _ECalBackendExchangeFactoryClass ECalBackendExchangeFactoryClass;

struct _ECalBackendExchangeFactory {
	ECalBackendFactory parent;
};

struct _ECalBackendExchangeFactoryClass {
	ECalBackendFactoryClass parent_class;
};

GType		e_cal_backend_exchange_events_factory_get_type (void);
void		e_cal_backend_exchange_events_factory_register_type
						(GTypeModule *type_module);

GType		e_cal_backend_exchange_todos_factory_get_type (void);
void		e_cal_backend_exchange_todos_factory_register_type
						(GTypeModule *type_module);

G_END_DECLS

#endif /* _E_CAL_BACKEND_EXCHANGE_FACTORY_H_ */
