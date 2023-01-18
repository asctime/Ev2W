/*
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
 *		Bolian Yin <bolian.yin@sun.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "ea-day-view.h"
#include "ea-cal-view-event.h"

#include "ea-calendar-helpers.h"
#include "ea-gnome-calendar.h"
#include <glib.h>
#include <glib/gi18n.h>

#include <e-util/gtk-compat.h>

static void ea_day_view_class_init (EaDayViewClass *klass);

static G_CONST_RETURN gchar * ea_day_view_get_name (AtkObject *accessible);
static G_CONST_RETURN gchar * ea_day_view_get_description (AtkObject *accessible);
static gint         ea_day_view_get_n_children      (AtkObject *obj);
static AtkObject*   ea_day_view_ref_child           (AtkObject *obj,
                                                     gint i);
static gpointer parent_class = NULL;

GType
ea_day_view_get_type (void)
{
	static GType type = 0;
	AtkObjectFactory *factory;
	GTypeQuery query;
	GType derived_atk_type;

	if (!type) {
		static GTypeInfo tinfo = {
			sizeof (EaDayViewClass),
			(GBaseInitFunc) NULL, /* base init */
			(GBaseFinalizeFunc) NULL, /* base finalize */
			(GClassInitFunc) ea_day_view_class_init, /* class init */
			(GClassFinalizeFunc) NULL, /* class finalize */
			NULL, /* class data */
			sizeof (EaDayView), /* instance size */
			0, /* nb preallocs */
			(GInstanceInitFunc) NULL, /* instance init */
			NULL /* value table */
		};

		/*
		 * Figure out the size of the class and instance
		 * we are run-time deriving from (EaCalView, in this case)
		 *
		 * Note: we must still use run-time deriving here, because
		 * our parent class EaCalView is run-time deriving.
		 */

		factory = atk_registry_get_factory (atk_get_default_registry (),
						    e_calendar_view_get_type());
		derived_atk_type = atk_object_factory_get_accessible_type (factory);
		g_type_query (derived_atk_type, &query);

		tinfo.class_size = query.class_size;
		tinfo.instance_size = query.instance_size;

		type = g_type_register_static (derived_atk_type,
					       "EaDayView", &tinfo, 0);
	}

	return type;
}

static void
ea_day_view_class_init (EaDayViewClass *klass)
{
	AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	class->get_name = ea_day_view_get_name;
	class->get_description = ea_day_view_get_description;

	class->get_n_children = ea_day_view_get_n_children;
	class->ref_child = ea_day_view_ref_child;
}

AtkObject*
ea_day_view_new (GtkWidget *widget)
{
	GObject *object;
	AtkObject *accessible;

	g_return_val_if_fail (E_IS_DAY_VIEW (widget), NULL);

	object = g_object_new (EA_TYPE_DAY_VIEW, NULL);

	accessible = ATK_OBJECT (object);
	atk_object_initialize (accessible, widget);

#ifdef ACC_DEBUG
	printf ("EvoAcc: ea_day_view created %p\n", (gpointer)accessible);
#endif

	return accessible;
}

static G_CONST_RETURN gchar *
ea_day_view_get_name (AtkObject *accessible)
{
	EDayView *day_view;
	GnomeCalendar *gcal;
	const gchar *label_text;
	GnomeCalendarViewType view_type;
	GtkWidget *widget;
	gint n_events;
	gchar *event_str, *name_str;

	g_return_val_if_fail (EA_IS_DAY_VIEW (accessible), NULL);

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
	if (widget == NULL)
		return NULL;

	day_view = E_DAY_VIEW (widget);
	gcal = e_calendar_view_get_calendar (E_CALENDAR_VIEW (day_view));

	label_text = ea_gnome_calendar_get_label_description (gcal);

	n_events = atk_object_get_n_accessible_children (accessible);
	/* the child main item is always there */
	--n_events;
	if (n_events >= 1)
		/* To translators: Here, "It" is either like "Work Week View: July
		10th - July 14th, 2006." or "Day View: Thursday July 13th, 2006." */
		event_str = g_strdup_printf (
			ngettext ("It has %d event.",
			"It has %d events.", n_events), n_events);
	else
		/* To translators: Here, "It" is either like "Work Week View: July
		10th - July 14th, 2006." or "Day View: Thursday July 13th, 2006." */
		event_str = g_strdup (_("It has no events."));

	view_type = gnome_calendar_get_view (gcal);
	if (view_type == GNOME_CAL_WORK_WEEK_VIEW)
		/* To translators: First %s is the week, for example "July 10th -
		July 14th, 2006". Second %s is the number of events in this work
		week, for example "It has %d event/events." or  "It has no events." */
		name_str = g_strdup_printf (_("Work Week View: %s. %s"),
						label_text, event_str);
	else
		/* To translators: First %s is the day, for example "Thursday July
		13th, 2006". Second %s is the number of events on this day, for
		example "It has %d event/events." or  "It has no events." */
		name_str = g_strdup_printf (_("Day View: %s. %s"),
						label_text, event_str);

	ATK_OBJECT_CLASS (parent_class)->set_name (accessible, name_str);
	g_free (name_str);
	g_free (event_str);

	return accessible->name;
}

static G_CONST_RETURN gchar *
ea_day_view_get_description (AtkObject *accessible)
{
	EDayView *day_view;
	GtkWidget *widget;

	g_return_val_if_fail (EA_IS_DAY_VIEW (accessible), NULL);

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
	if (widget == NULL)
		return NULL;

	day_view = E_DAY_VIEW (widget);

	if (accessible->description)
		return accessible->description;
	else {
		GnomeCalendar *gcal;
		GnomeCalendarViewType view_type;

		gcal = e_calendar_view_get_calendar (E_CALENDAR_VIEW (day_view));
		view_type = gnome_calendar_get_view (gcal);

		if (view_type == GNOME_CAL_WORK_WEEK_VIEW)
			return _("calendar view for a work week");
		else
			return _("calendar view for one or more days");
	}
}

static gint
ea_day_view_get_n_children (AtkObject *accessible)
{
	EDayView *day_view;
	GtkWidget *widget;
	gint day;
	gint child_num = 0;

	g_return_val_if_fail (EA_IS_DAY_VIEW (accessible), -1);

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
	if (widget == NULL)
		return -1;

	day_view = E_DAY_VIEW (widget);

	child_num += day_view->long_events->len;

	for (day = 0; day < day_view->days_shown; day++) {
		child_num += day_view->events[day]->len;
	}

	/* "+1" for the main item */
	return child_num + 1;
}

static AtkObject *
ea_day_view_ref_child (AtkObject *accessible, gint index)
{
	EDayView *day_view;
	gint child_num;
	gint day;
	AtkObject *atk_object = NULL;
	EDayViewEvent *event = NULL;
	GtkWidget *widget;

	g_return_val_if_fail (EA_IS_DAY_VIEW (accessible), NULL);

	child_num = atk_object_get_n_accessible_children (accessible);
	if (child_num <= 0 || index < 0 || index >= child_num)
		return NULL;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
	if (widget == NULL)
		return NULL;

	day_view = E_DAY_VIEW (widget);

	if (index == 0) {
		/* index == 0 is the main item */
		atk_object = atk_gobject_accessible_for_object (
			G_OBJECT (day_view->main_canvas_item));
		g_object_ref (atk_object);
	}
	else {
		--index;
		/* a long event */
		if (index < day_view->long_events->len) {
			event = &g_array_index (day_view->long_events,
						EDayViewEvent, index);
		}
		else {
			index -= day_view->long_events->len;
			day = 0;
			while (index >= day_view->events[day]->len) {
				index -= day_view->events[day]->len;
				++day;
			}

			event = &g_array_index (day_view->events[day],
						EDayViewEvent, index);
		}
		if (event && event->canvas_item) {
			/* Not use atk_gobject_accessible_for_object here,
			 * we need to do special thing here
			 */
			atk_object = ea_calendar_helpers_get_accessible_for (event->canvas_item);
			g_object_ref (atk_object);
		}
	}
	return atk_object;
}
