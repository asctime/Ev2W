/*
 *
 * Evolution calendar - Data model for ETable
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_CAL_MODEL_H
#define E_CAL_MODEL_H

#include <table/e-table-model.h>
#include <libecal/e-cal.h>
#include "e-cell-date-edit-text.h"

/* Standard GObject macros */
#define E_TYPE_CAL_MODEL \
	(e_cal_model_get_type ())
#define E_CAL_MODEL(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CAL_MODEL, ECalModel))
#define E_CAL_MODEL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CAL_MODEL, ECalModelClass))
#define E_IS_CAL_MODEL(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CAL_MODEL))
#define E_IS_CAL_MODEL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CAL_MODEL))
#define E_CAL_MODEL_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CAL_MODEL, ECalModelClass))

/* Standard GObject macros */
#define E_TYPE_CAL_MODEL_COMPONENT \
	(e_cal_model_component_get_type ())
#define E_CAL_MODEL_COMPONENT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CAL_MODEL_COMPONENT, ECalModelComponent))
#define E_CAL_MODEL_COMPONENT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CAL_MODEL_COMPONENT, ECalModelComponentClass))
#define E_IS_CAL_MODEL_COMPONENT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CAL_MODEL_COMPONENT))
#define E_IS_CAL_MODEL_COMPONENT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CAL_MODEL_COMPONENT))
#define E_CAL_MODEL_COMPONENT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CAL_MODEL_COMPONENT, ECalModelComponentClass))

G_BEGIN_DECLS

typedef enum {
	/* If you add new items here or reorder them, you have to update the
	   .etspec files for the tables using this model */
	E_CAL_MODEL_FIELD_CATEGORIES,
	E_CAL_MODEL_FIELD_CLASSIFICATION,
	E_CAL_MODEL_FIELD_COLOR,            /* not a real field */
	E_CAL_MODEL_FIELD_COMPONENT,        /* not a real field */
	E_CAL_MODEL_FIELD_DESCRIPTION,
	E_CAL_MODEL_FIELD_DTSTART,
	E_CAL_MODEL_FIELD_HAS_ALARMS,       /* not a real field */
	E_CAL_MODEL_FIELD_ICON,             /* not a real field */
	E_CAL_MODEL_FIELD_SUMMARY,
	E_CAL_MODEL_FIELD_UID,
	E_CAL_MODEL_FIELD_CREATED,
	E_CAL_MODEL_FIELD_LASTMODIFIED,
	E_CAL_MODEL_FIELD_LAST
} ECalModelField;

typedef enum {
	E_CAL_MODEL_FLAGS_INVALID            = -1,
	E_CAL_MODEL_FLAGS_EXPAND_RECURRENCES = 0x01
} ECalModelFlags;

typedef struct _ECalModel ECalModel;
typedef struct _ECalModelClass ECalModelClass;
typedef struct _ECalModelPrivate ECalModelPrivate;

typedef struct _ECalModelComponent ECalModelComponent;
typedef struct _ECalModelComponentClass ECalModelComponentClass;
typedef struct _ECalModelComponentPrivate ECalModelComponentPrivate;

struct _ECalModelComponent {
	GObject object;

	ECal *client;
	icalcomponent *icalcomp;
	time_t instance_start;
	time_t instance_end;

	/* Private data used by ECalModelCalendar and ECalModelTasks */
	/* keep these public to avoid many accessor functions */
	ECellDateEditValue *dtstart;
	ECellDateEditValue *dtend;
	ECellDateEditValue *due;
	ECellDateEditValue *completed;
	ECellDateEditValue *created;
	ECellDateEditValue *lastmodified;
	gchar *color;

	ECalModelComponentPrivate *priv;
};

struct _ECalModelComponentClass {
	GObjectClass parent_class;
};

typedef struct {
	ECalModelComponent *comp_data;
	gpointer cb_data;
} ECalModelGenerateInstancesData;

struct _ECalModel {
	ETableModel model;
	ECalModelPrivate *priv;
};

struct _ECalModelClass {
	ETableModelClass parent_class;

	/* virtual methods */
	const gchar *	(*get_color_for_component)
						(ECalModel *model,
						 ECalModelComponent *comp_data);
	void		(*fill_component_from_model)
						(ECalModel *model,
						 ECalModelComponent *comp_data,
						 ETableModel *source_model,
						 gint row);

	/* Signals */
	void		(*time_range_changed)	(ECalModel *model,
						 time_t start,
						 time_t end);
	void		(*row_appended)		(ECalModel *model);
	void		(*comps_deleted)	(ECalModel *model,
						 gpointer list);
	void		(*cal_view_progress)	(ECalModel *model,
						 const gchar *message,
						 gint progress,
						 ECalSourceType type);
	#ifndef E_CAL_DISABLE_DEPRECATED
	void		(*cal_view_done)	(ECalModel *model,
						 ECalendarStatus status,
						 ECalSourceType type);
	#endif
	void		(*cal_view_complete)	(ECalModel *model,
						 ECalendarStatus status,
						 const gchar *error_msg,
						 ECalSourceType type);
	void		(*status_message)	(ECalModel *model,
						 const gchar *message,
						 gdouble percent);
	void		(*timezone_changed)	(ECalModel *model,
						 icaltimezone *old_zone,
						 icaltimezone *new_zone);
};

typedef time_t (*ECalModelDefaultTimeFunc) (ECalModel *model, gpointer user_data);

GType		e_cal_model_get_type		(void);
GType		e_cal_model_component_get_type	(void);
icalcomponent_kind
		e_cal_model_get_component_kind	(ECalModel *model);
void		e_cal_model_set_component_kind	(ECalModel *model,
						 icalcomponent_kind kind);
ECalModelFlags	e_cal_model_get_flags		(ECalModel *model);
void		e_cal_model_set_flags		(ECalModel *model,
						 ECalModelFlags flags);
icaltimezone *	e_cal_model_get_timezone	(ECalModel *model);
void		e_cal_model_set_timezone	(ECalModel *model,
						 icaltimezone *zone);
void		e_cal_model_set_default_category(ECalModel *model,
						 const gchar *default_cat);
gboolean	e_cal_model_get_use_24_hour_format
						(ECalModel *model);
void		e_cal_model_set_use_24_hour_format
						(ECalModel *model,
						 gboolean use24);
gint		e_cal_model_get_week_start_day	(ECalModel *model);
void		e_cal_model_set_week_start_day	(ECalModel *model,
						 gint week_start_day);
ECal *		e_cal_model_get_default_client	(ECalModel *model);
void		e_cal_model_set_default_client	(ECalModel *model,
						 ECal *client);
GList *		e_cal_model_get_client_list	(ECalModel *model);
ECal *		e_cal_model_get_client_for_uri	(ECalModel *model,
						 const gchar *uri);
void		e_cal_model_add_client		(ECalModel *model,
						 ECal *client);
void		e_cal_model_remove_client	(ECalModel *model,
						 ECal *client);
void		e_cal_model_remove_all_clients	(ECalModel *model);
void		e_cal_model_get_time_range	(ECalModel *model,
						 time_t *start,
						 time_t *end);
void		e_cal_model_set_time_range	(ECalModel *model,
						 time_t start,
						 time_t end);
const gchar *	e_cal_model_get_search_query	(ECalModel *model);
void		e_cal_model_set_search_query	(ECalModel *model,
						 const gchar *sexp);
icalcomponent *	e_cal_model_create_component_with_defaults
						(ECalModel *model,
						 gboolean all_day);
const gchar *	e_cal_model_get_color_for_component
						(ECalModel *model,
						 ECalModelComponent *comp_data);
gboolean	e_cal_model_get_rgb_color_for_component
						(ECalModel *model,
						 ECalModelComponent *comp_data,
						 gdouble *red,
						 gdouble *green,
						 gdouble *blue);
ECalModelComponent *
		e_cal_model_get_component_at	(ECalModel *model,
						 gint row);
ECalModelComponent *
		e_cal_model_get_component_for_uid
						(ECalModel *model,
						 const ECalComponentId *id);
gchar *		e_cal_model_date_value_to_string(ECalModel *model,
						 gconstpointer value);
void		e_cal_model_generate_instances	(ECalModel *model,
						 time_t start,
						 time_t end,
						 ECalRecurInstanceFn cb,
						 gpointer cb_data);
GPtrArray *	e_cal_model_get_object_array	(ECalModel *model);
void		e_cal_model_set_instance_times	(ECalModelComponent *comp_data,
						 const icaltimezone *zone);
void		e_cal_model_set_search_query_with_time_range
						(ECalModel *model,
						 const gchar *sexp,
						 time_t start,
						 time_t end);
gboolean	e_cal_model_test_row_editable	(ECalModel *model,
						 gint row);
void		e_cal_model_set_default_time_func
						(ECalModel *model,
						 ECalModelDefaultTimeFunc func,
						 gpointer user_data);

void		e_cal_model_update_comp_time	(ECalModel *model,
						 ECalModelComponent *comp_data,
						 gconstpointer time_value,
						 icalproperty_kind kind,
						 void (*set_func)(icalproperty *prop, struct icaltimetype v),
						 icalproperty * (*new_func)(struct icaltimetype v));

void		e_cal_model_update_status_message (ECalModel *model,
						 const gchar *message,
						 gdouble percent);

G_END_DECLS

#endif /* E_CAL_MODEL_H */
