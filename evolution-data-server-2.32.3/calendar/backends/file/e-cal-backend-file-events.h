/* Evolution calendar - iCalendar file backend
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef E_CAL_BACKEND_FILE_EVENTS_H
#define E_CAL_BACKEND_FILE_EVENTS_H

#include "e-cal-backend-file.h"

G_BEGIN_DECLS



#define E_TYPE_CAL_BACKEND_FILE_EVENTS            (e_cal_backend_file_events_get_type ())
#define E_CAL_BACKEND_FILE_EVENTS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_CAL_BACKEND_FILE_EVENTS,		\
					  ECalBackendFileEvents))
#define E_CAL_BACKEND_FILE_EVENTS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_CAL_BACKEND_FILE_EVENTS,	\
					  ECalBackendFileEventsClass))
#define E_IS_CAL_BACKEND_FILE_EVENTS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_CAL_BACKEND_FILE_EVENTS))
#define E_IS_CAL_BACKEND_FILE_EVENTS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), E_TYPE_CAL_BACKEND_FILE_EVENTS))

typedef struct _ECalBackendFileEvents ECalBackendFileEvents;
typedef struct _ECalBackendFileEventsClass ECalBackendFileEventsClass;

typedef struct _ECalBackendFileEventsPrivate ECalBackendFileEventsPrivate;

struct _ECalBackendFileEvents {
	ECalBackendFile backend;

	/* Private data */
	ECalBackendFileEventsPrivate *priv;
};

struct _ECalBackendFileEventsClass {
	ECalBackendFileClass parent_class;
};

GType e_cal_backend_file_events_get_type (void);



G_END_DECLS

#endif
