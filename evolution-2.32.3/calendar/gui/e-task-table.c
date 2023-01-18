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
 *		Damon Chaplin <damon@ximian.com>
 *		Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/*
 * ETaskTable - displays the ECalComponent objects in a table (an ETable).
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/stat.h>
#include <unistd.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <misc/e-selectable.h>
#include <table/e-cell-checkbox.h>
#include <table/e-cell-toggle.h>
#include <table/e-cell-text.h>
#include <table/e-cell-combo.h>
#include <table/e-cell-date.h>
#include <e-util/e-binding.h>
#include <e-util/e-selection.h>
#include <e-util/e-dialog-utils.h>
#include <e-util/e-util-private.h>
#include <table/e-cell-date-edit.h>
#include <table/e-cell-percent.h>
#include <table/e-table-sorting-utils.h>
#include <libecal/e-cal-time-util.h>
#include <libedataserver/e-time-utils.h>

#include "calendar-config.h"
#include "dialogs/delete-comp.h"
#include "dialogs/delete-error.h"
#include "dialogs/task-editor.h"
#include "e-cal-model-tasks.h"
#include "e-task-table.h"
#include "e-calendar-view.h"
#include "e-cell-date-edit-text.h"
#include "print.h"
#include <e-util/e-util-private.h>
#include <e-util/e-icon-factory.h>
#include "misc.h"

#define E_TASK_TABLE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_TASK_TABLE, ETaskTablePrivate))

struct _ETaskTablePrivate {
	gpointer shell_view;  /* weak pointer */
	ECalModel *model;

	GtkTargetList *copy_target_list;
	GtkTargetList *paste_target_list;
};

enum {
	PROP_0,
	PROP_COPY_TARGET_LIST,
	PROP_MODEL,
	PROP_PASTE_TARGET_LIST,
	PROP_SHELL_VIEW
};

enum {
	OPEN_COMPONENT,
	POPUP_EVENT,
	STATUS_MESSAGE,
	LAST_SIGNAL
};

static struct tm e_task_table_get_current_time (ECellDateEdit *ecde, gpointer data);

static gpointer parent_class;
static guint signals[LAST_SIGNAL];

/* The icons to represent the task. */
static const gchar *icon_names[] = {
	"stock_task",
	"stock_task-recurring",
	"stock_task-assigned",
	"stock_task-assigned-to"
};

static void
task_table_emit_open_component (ETaskTable *task_table,
                                ECalModelComponent *comp_data)
{
	guint signal_id = signals[OPEN_COMPONENT];

	g_signal_emit (task_table, signal_id, 0, comp_data);
}

static void
task_table_emit_popup_event (ETaskTable *task_table,
                             GdkEvent *event)
{
	guint signal_id = signals[POPUP_EVENT];

	g_signal_emit (task_table, signal_id, 0, event);
}

static void
task_table_emit_status_message (ETaskTable *task_table,
                                const gchar *message,
                                gdouble percent)
{
	guint signal_id = signals[STATUS_MESSAGE];

	g_signal_emit (task_table, signal_id, 0, message, percent);
}

static gint
task_table_percent_compare_cb (gconstpointer a, gconstpointer b, gpointer cmp_cache)
{
	gint percent1 = GPOINTER_TO_INT (a);
	gint percent2 = GPOINTER_TO_INT (b);

	return (percent1 < percent2) ? -1 : (percent1 > percent2);
}

static gint
task_table_priority_compare_cb (gconstpointer a, gconstpointer b, gpointer cmp_cache)
{
	gint priority1, priority2;

	priority1 = e_cal_util_priority_from_string ((const gchar *) a);
	priority2 = e_cal_util_priority_from_string ((const gchar *) b);

	/* We change undefined priorities so they appear after 'Low'. */
	if (priority1 <= 0)
		priority1 = 10;
	if (priority2 <= 0)
		priority2 = 10;

	/* We'll just use the ordering of the priority values. */
	return (priority1 < priority2) ? -1 : (priority1 > priority2);
}

static const gchar *
get_cache_str (gpointer cmp_cache, const gchar *str)
{
	const gchar *value;

	if (!cmp_cache || !str)
		return str;

	value = e_table_sorting_utils_lookup_cmp_cache (cmp_cache, str);
	if (!value) {
		gchar *ckey;

		ckey = g_utf8_collate_key (str, -1);
		e_table_sorting_utils_add_to_cmp_cache (cmp_cache, (gchar *) str, ckey);
		value = ckey;
	}

	return value;
}

static gboolean
same_cache_string (gpointer cmp_cache, const gchar *str_a, const gchar *str_b)
{
	if (!cmp_cache)
		return g_utf8_collate (str_a, str_b) == 0;

	str_b = get_cache_str (cmp_cache, str_b);

	g_return_val_if_fail (str_a != NULL, FALSE);
	g_return_val_if_fail (str_b != NULL, FALSE);

	return strcmp (str_a, str_b) == 0;
}

static gint
task_table_status_compare_cb (gconstpointer a, gconstpointer b, gpointer cmp_cache)
{
	const gchar *string_a = a;
	const gchar *string_b = b;
	gint status_a = -2;
	gint status_b = -2;

	if (string_a == NULL || *string_a == '\0')
		status_a = -1;
	else {
		const gchar *cache_str = get_cache_str (cmp_cache, string_a);

		if (same_cache_string (cmp_cache, cache_str, _("Not Started")))
			status_a = 0;
		else if (same_cache_string (cmp_cache, cache_str, _("In Progress")))
			status_a = 1;
		else if (same_cache_string (cmp_cache, cache_str, _("Completed")))
			status_a = 2;
		else if (same_cache_string (cmp_cache, cache_str, _("Canceled")))
			status_a = 3;
	}

	if (string_b == NULL || *string_b == '\0')
		status_b = -1;
	else {
		const gchar *cache_str = get_cache_str (cmp_cache, string_b);

		if (same_cache_string (cmp_cache, cache_str, _("Not Started")))
			status_b = 0;
		else if (same_cache_string (cmp_cache, cache_str, _("In Progress")))
			status_b = 1;
		else if (same_cache_string (cmp_cache, cache_str, _("Completed")))
			status_b = 2;
		else if (same_cache_string (cmp_cache, cache_str, _("Canceled")))
			status_b = 3;
	}

	return (status_a < status_b) ? -1 : (status_a > status_b);
}

static void
task_table_model_cal_view_progress_cb (ETaskTable *task_table,
                                       const gchar *message,
                                       gint progress,
                                       ECalSourceType type)
{
	gdouble percent = (gdouble) progress;

	task_table_emit_status_message (task_table, message, percent);
}

static void
task_table_model_cal_view_complete_cb ( ETaskTable *task_table,
					ECalendarStatus status,
					const gchar *error_msg,
					ECalSourceType type)
{
	task_table_emit_status_message (task_table, NULL, -1.0);
}

/* Deletes all of the selected components in the table */
static void
delete_selected_components (ETaskTable *task_table)
{
	GSList *objs, *l;
	const gchar *status_message;

	objs = e_task_table_get_selected (task_table);

	status_message = _("Deleting selected objects");
	task_table_emit_status_message (task_table, status_message, -1.0);

	for (l = objs; l; l = l->next) {
		ECalModelComponent *comp_data = (ECalModelComponent *) l->data;
		GError *error = NULL;

		e_cal_remove_object (comp_data->client,
				     icalcomponent_get_uid (comp_data->icalcomp), &error);
		delete_error_dialog (error, E_CAL_COMPONENT_TODO);
		g_clear_error (&error);
	}

	task_table_emit_status_message (task_table, NULL, -1.0);

	g_slist_free (objs);
}

static void
task_table_set_model (ETaskTable *task_table,
                      ECalModel *model)
{
	g_return_if_fail (task_table->priv->model == NULL);

	task_table->priv->model = g_object_ref (model);

	g_signal_connect_swapped (
		model, "cal-view-progress",
		G_CALLBACK (task_table_model_cal_view_progress_cb),
		task_table);

	g_signal_connect_swapped (
		model, "cal-view-complete",
		G_CALLBACK (task_table_model_cal_view_complete_cb),
		task_table);
}

static void
task_table_set_shell_view (ETaskTable *task_table,
                           EShellView *shell_view)
{
	g_return_if_fail (task_table->priv->shell_view == NULL);

	task_table->priv->shell_view = shell_view;

	g_object_add_weak_pointer (
		G_OBJECT (shell_view),
		&task_table->priv->shell_view);
}

static void
task_table_set_property (GObject *object,
                         guint property_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_MODEL:
			task_table_set_model (
				E_TASK_TABLE (object),
				g_value_get_object (value));
			return;

		case PROP_SHELL_VIEW:
			task_table_set_shell_view (
				E_TASK_TABLE (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
task_table_get_property (GObject *object,
                         guint property_id,
                         GValue *value,
                         GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_COPY_TARGET_LIST:
			g_value_set_boxed (
				value, e_task_table_get_copy_target_list (
				E_TASK_TABLE (object)));
			return;

		case PROP_MODEL:
			g_value_set_object (
				value, e_task_table_get_model (
				E_TASK_TABLE (object)));
			return;

		case PROP_PASTE_TARGET_LIST:
			g_value_set_boxed (
				value, e_task_table_get_paste_target_list (
				E_TASK_TABLE (object)));
			return;

		case PROP_SHELL_VIEW:
			g_value_set_object (
				value, e_task_table_get_shell_view (
				E_TASK_TABLE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
task_table_dispose (GObject *object)
{
	ETaskTablePrivate *priv;

	priv = E_TASK_TABLE_GET_PRIVATE (object);

	if (priv->shell_view != NULL) {
		g_object_remove_weak_pointer (
			G_OBJECT (priv->shell_view), &priv->shell_view);
		priv->shell_view = NULL;
	}

	if (priv->model != NULL) {
		g_object_unref (priv->model);
		priv->model = NULL;
	}

	if (priv->copy_target_list != NULL) {
		gtk_target_list_unref (priv->copy_target_list);
		priv->copy_target_list = NULL;
	}

	if (priv->paste_target_list != NULL) {
		gtk_target_list_unref (priv->paste_target_list);
		priv->paste_target_list = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
task_table_constructed (GObject *object)
{
	ETaskTable *task_table;
	ECalModel *model;
	ECell *cell, *popup_cell;
	ETableExtras *extras;
	GList *strings;
	AtkObject *a11y;
	gchar *etspecfile;
	gint percent;

	task_table = E_TASK_TABLE (object);
	model = e_task_table_get_model (task_table);

	/* Create the header columns */

	extras = e_table_extras_new ();

	/*
	 * Normal string fields.
	 */
	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (cell,
		      "strikeout_column", E_CAL_MODEL_TASKS_FIELD_STRIKEOUT,
		      "bold_column", E_CAL_MODEL_TASKS_FIELD_OVERDUE,
		      "bg_color_column", E_CAL_MODEL_FIELD_COLOR,
		      NULL);

	e_table_extras_add_cell (extras, "calstring", cell);

	/*
	 * Date fields.
	 */
	cell = e_cell_date_edit_text_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (cell,
		      "strikeout_column", E_CAL_MODEL_TASKS_FIELD_STRIKEOUT,
		      "bold_column", E_CAL_MODEL_TASKS_FIELD_OVERDUE,
		      "bg_color_column", E_CAL_MODEL_FIELD_COLOR,
		      NULL);

	e_mutual_binding_new (
		model, "timezone",
		cell, "timezone");

	e_mutual_binding_new (
		model, "use-24-hour-format",
		cell, "use-24-hour-format");

	popup_cell = e_cell_date_edit_new ();
	e_cell_popup_set_child (E_CELL_POPUP (popup_cell), cell);
	g_object_unref (cell);

	e_mutual_binding_new (
		model, "use-24-hour-format",
		popup_cell, "use-24-hour-format");

	e_table_extras_add_cell (extras, "dateedit", popup_cell);
	task_table->dates_cell = E_CELL_DATE_EDIT (popup_cell);

	e_cell_date_edit_set_get_time_callback (
		E_CELL_DATE_EDIT (popup_cell),
		e_task_table_get_current_time, task_table, NULL);

	/*
	 * Combo fields.
	 */

	/* Classification field. */
	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (cell,
		      "strikeout_column", E_CAL_MODEL_TASKS_FIELD_STRIKEOUT,
		      "bold_column", E_CAL_MODEL_TASKS_FIELD_OVERDUE,
		      "bg_color_column", E_CAL_MODEL_FIELD_COLOR,
		      "editable", FALSE,
		      NULL);

	popup_cell = e_cell_combo_new ();
	e_cell_popup_set_child (E_CELL_POPUP (popup_cell), cell);
	g_object_unref (cell);

	strings = NULL;
	strings = g_list_append (strings, (gchar *) _("Public"));
	strings = g_list_append (strings, (gchar *) _("Private"));
	strings = g_list_append (strings, (gchar *) _("Confidential"));
	e_cell_combo_set_popdown_strings (E_CELL_COMBO (popup_cell),
					  strings);
	g_list_free (strings);

	e_table_extras_add_cell (extras, "classification", popup_cell);

	/* Priority field. */
	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (G_OBJECT (cell),
		      "strikeout_column", E_CAL_MODEL_TASKS_FIELD_STRIKEOUT,
		      "bold_column", E_CAL_MODEL_TASKS_FIELD_OVERDUE,
		      "bg_color_column", E_CAL_MODEL_FIELD_COLOR,
		      "editable", FALSE,
		      NULL);

	popup_cell = e_cell_combo_new ();
	e_cell_popup_set_child (E_CELL_POPUP (popup_cell), cell);
	g_object_unref (cell);

	strings = NULL;
	strings = g_list_append (strings, (gchar *) _("High"));
	strings = g_list_append (strings, (gchar *) _("Normal"));
	strings = g_list_append (strings, (gchar *) _("Low"));
	strings = g_list_append (strings, (gchar *) _("Undefined"));
	e_cell_combo_set_popdown_strings (E_CELL_COMBO (popup_cell),
					  strings);
	g_list_free (strings);

	e_table_extras_add_cell (extras, "priority", popup_cell);

	/* Percent field. */
	cell = e_cell_percent_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (G_OBJECT (cell),
		      "strikeout_column", E_CAL_MODEL_TASKS_FIELD_STRIKEOUT,
		      "bold_column", E_CAL_MODEL_TASKS_FIELD_OVERDUE,
		      "bg_color_column", E_CAL_MODEL_FIELD_COLOR,
		      NULL);

	popup_cell = e_cell_combo_new ();
	e_cell_popup_set_child (E_CELL_POPUP (popup_cell), cell);
	g_object_unref (cell);

	strings = NULL;
	for (percent = 0; percent <= 100; percent += 10) {
		/* Translators: "%d%%" is the percentage of a task done.
		   %d is the actual value, %% is replaced with a percent sign.
		   Result values will be 0%, 10%, 20%, ... 100%
		*/
		strings = g_list_append (strings, g_strdup_printf (_("%d%%"), percent));
	}
	e_cell_combo_set_popdown_strings (E_CELL_COMBO (popup_cell),
					  strings);

	g_list_foreach (strings, (GFunc) g_free, NULL);
	g_list_free (strings);

	e_table_extras_add_cell (extras, "percent", popup_cell);

	/* Transparency field. */
	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (G_OBJECT (cell),
		      "strikeout_column", E_CAL_MODEL_TASKS_FIELD_STRIKEOUT,
		      "bold_column", E_CAL_MODEL_TASKS_FIELD_OVERDUE,
		      "bg_color_column", E_CAL_MODEL_FIELD_COLOR,
		      "editable", FALSE,
		      NULL);

	popup_cell = e_cell_combo_new ();
	e_cell_popup_set_child (E_CELL_POPUP (popup_cell), cell);
	g_object_unref (cell);

	strings = NULL;
	strings = g_list_append (strings, (gchar *) _("Free"));
	strings = g_list_append (strings, (gchar *) _("Busy"));
	e_cell_combo_set_popdown_strings (E_CELL_COMBO (popup_cell),
					  strings);
	g_list_free (strings);

	e_table_extras_add_cell (extras, "transparency", popup_cell);

	/* Status field. */
	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (G_OBJECT (cell),
		      "strikeout_column", E_CAL_MODEL_TASKS_FIELD_STRIKEOUT,
		      "bold_column", E_CAL_MODEL_TASKS_FIELD_OVERDUE,
		      "bg_color_column", E_CAL_MODEL_FIELD_COLOR,
		      "editable", FALSE,
		      NULL);

	popup_cell = e_cell_combo_new ();
	e_cell_popup_set_child (E_CELL_POPUP (popup_cell), cell);
	g_object_unref (cell);

	strings = NULL;
	strings = g_list_append (strings, (gchar *) _("Not Started"));
	strings = g_list_append (strings, (gchar *) _("In Progress"));
	strings = g_list_append (strings, (gchar *) _("Completed"));
	strings = g_list_append (strings, (gchar *) _("Canceled"));
	e_cell_combo_set_popdown_strings (E_CELL_COMBO (popup_cell),
					  strings);
	g_list_free (strings);

	e_table_extras_add_cell (extras, "calstatus", popup_cell);

	e_table_extras_add_compare (extras, "date-compare",
				    e_cell_date_edit_compare_cb);
	e_table_extras_add_compare (extras, "percent-compare",
				    task_table_percent_compare_cb);
	e_table_extras_add_compare (extras, "priority-compare",
				    task_table_priority_compare_cb);
	e_table_extras_add_compare (extras, "status-compare",
				    task_table_status_compare_cb);

	/* Create pixmaps */

	cell = e_cell_toggle_new (icon_names, G_N_ELEMENTS (icon_names));
	e_table_extras_add_cell (extras, "icon", cell);
	e_table_extras_add_icon_name (extras, "icon", "stock_task");

	e_table_extras_add_icon_name (extras, "complete", "stock_check-filled");

	/* set proper format component for a default 'date' cell renderer */
	cell = e_table_extras_get_cell (extras, "date");
	e_cell_date_set_format_component (E_CELL_DATE (cell), "calendar");

	/* Create the table */

	etspecfile = g_build_filename (
		EVOLUTION_ETSPECDIR, "e-calendar-table.etspec", NULL);
	e_table_construct_from_spec_file (
		E_TABLE (task_table), E_TABLE_MODEL (model),
		extras, etspecfile, NULL);
	g_free (etspecfile);

	gtk_widget_set_has_tooltip (GTK_WIDGET (task_table), TRUE);

	g_object_unref (extras);

	a11y = gtk_widget_get_accessible (GTK_WIDGET (task_table));
	if (a11y)
		atk_object_set_name (a11y, _("Tasks"));
}

static gboolean
task_table_popup_menu (GtkWidget *widget)
{
	ETaskTable *task_table;

	task_table = E_TASK_TABLE (widget);
	task_table_emit_popup_event (task_table, NULL);

	return TRUE;
}

static gboolean
task_table_query_tooltip (GtkWidget *widget,
                              gint x,
                              gint y,
                              gboolean keyboard_mode,
                              GtkTooltip *tooltip)
{
	ETaskTable *task_table;
	ECalModel *model;
	ECalModelComponent *comp_data;
	gint row = -1, col = -1;
	GtkWidget *box, *l, *w;
	GtkStyle *style = gtk_widget_get_default_style ();
	gchar *tmp;
	const gchar *str;
	GString *tmp2;
	gchar buff[1001];
	gboolean free_text = FALSE;
	gboolean use_24_hour_format;
	ECalComponent *new_comp;
	ECalComponentOrganizer organizer;
	ECalComponentDateTime dtstart, dtdue;
	icalcomponent *clone;
	icaltimezone *zone, *default_zone;
	GSList *desc, *p;
	gint len;
	ESelectionModel *esm;
	struct tm tmp_tm;

	if (keyboard_mode)
		return FALSE;

	task_table = E_TASK_TABLE (widget);

	e_table_get_mouse_over_cell (E_TABLE (task_table), &row, &col);
	if (row == -1)
		return FALSE;

	/* Respect sorting option; the 'e_table_get_mouse_over_cell'
	 * returns sorted row, not the model one. */
	esm = e_table_get_selection_model (E_TABLE (task_table));
	if (esm && esm->sorter && e_sorter_needs_sorting (esm->sorter))
		row = e_sorter_sorted_to_model (esm->sorter, row);

	model = e_task_table_get_model (task_table);
	comp_data = e_cal_model_get_component_at (model, row);
	if (!comp_data || !comp_data->icalcomp)
		return FALSE;

	new_comp = e_cal_component_new ();
	clone = icalcomponent_new_clone (comp_data->icalcomp);
	if (!e_cal_component_set_icalcomponent (new_comp, clone)) {
		g_object_unref (new_comp);
		return FALSE;
	}

	box = gtk_vbox_new (FALSE, 0);

	str = e_calendar_view_get_icalcomponent_summary (
		comp_data->client, comp_data->icalcomp, &free_text);
	if (!(str && *str)) {
		if (free_text)
			g_free ((gchar *)str);
		free_text = FALSE;
		str = _("* No Summary *");
	}

	l = gtk_label_new (NULL);
	tmp = g_markup_printf_escaped ("<b>%s</b>", str);
	gtk_label_set_line_wrap (GTK_LABEL (l), TRUE);
	gtk_label_set_markup (GTK_LABEL (l), tmp);
	gtk_misc_set_alignment (GTK_MISC (l), 0.0, 0.5);
	w = gtk_event_box_new ();

	gtk_widget_modify_bg (w, GTK_STATE_NORMAL, &(style->bg[GTK_STATE_SELECTED]));
	gtk_widget_modify_fg (l, GTK_STATE_NORMAL, &(style->text[GTK_STATE_SELECTED]));
	gtk_container_add (GTK_CONTAINER (w), l);
	gtk_box_pack_start (GTK_BOX (box), w, TRUE, TRUE, 0);
	g_free (tmp);

	if (free_text)
		g_free ((gchar *)str);
	free_text = FALSE;

	w = gtk_event_box_new ();
	gtk_widget_modify_bg (w, GTK_STATE_NORMAL, &(style->bg[GTK_STATE_NORMAL]));

	l = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (w), l);
	gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE, 0);
	w = l;

	e_cal_component_get_organizer (new_comp, &organizer);
	if (organizer.cn) {
		gchar *ptr;
		ptr = strchr( organizer.value, ':');

		if (ptr) {
			ptr++;
			/* To Translators: It will display "Organizer: NameOfTheUser <email@ofuser.com>" */
			tmp = g_strdup_printf (_("Organizer: %s <%s>"), organizer.cn, ptr);
		} else {
			/* With SunOne accounts, there may be no ':' in organiser.value */
			tmp = g_strdup_printf (_("Organizer: %s"), organizer.cn);
		}

		l = gtk_label_new (tmp);
		gtk_label_set_line_wrap (GTK_LABEL (l), FALSE);
		gtk_misc_set_alignment (GTK_MISC (l), 0.0, 0.5);
		gtk_box_pack_start (GTK_BOX (w), l, FALSE, FALSE, 0);
		g_free (tmp);
	}

	e_cal_component_get_dtstart (new_comp, &dtstart);
	e_cal_component_get_due (new_comp, &dtdue);

	default_zone = e_cal_model_get_timezone (model);
	use_24_hour_format = e_cal_model_get_use_24_hour_format (model);

	if (dtstart.tzid) {
		zone = icalcomponent_get_timezone (
			e_cal_component_get_icalcomponent (new_comp),
			dtstart.tzid);
		if (!zone)
			e_cal_get_timezone (
				comp_data->client, dtstart.tzid, &zone, NULL);
		if (!zone)
			zone = default_zone;
	} else {
		zone = NULL;
	}

	tmp2 = g_string_new ("");

	if (dtstart.value) {
		buff[0] = 0;

		tmp_tm = icaltimetype_to_tm_with_zone (
			dtstart.value, zone, default_zone);
		e_time_format_date_and_time (
			&tmp_tm, use_24_hour_format,
			FALSE, FALSE, buff, 1000);

		if (buff[0]) {
			g_string_append (tmp2, _("Start: "));
			g_string_append (tmp2, buff);
		}
	}

	if (dtdue.value) {
		buff[0] = 0;

		tmp_tm = icaltimetype_to_tm_with_zone (
			dtdue.value, zone, default_zone);
		e_time_format_date_and_time (
			&tmp_tm, use_24_hour_format,
			FALSE, FALSE, buff, 1000);

		if (buff[0]) {
			if (tmp2->len)
				g_string_append (tmp2, "; ");

			g_string_append (tmp2, _("Due: "));
			g_string_append (tmp2, buff);
		}
	}

	if (tmp2->len) {
		l = gtk_label_new (tmp2->str);
		gtk_misc_set_alignment (GTK_MISC (l), 0.0, 0.5);
		gtk_box_pack_start (GTK_BOX (w), l, FALSE, FALSE, 0);
	}

	g_string_free (tmp2, TRUE);

	e_cal_component_free_datetime (&dtstart);
	e_cal_component_free_datetime (&dtdue);

	tmp = e_calendar_view_get_attendees_status_info (
		new_comp, comp_data->client);
	if (tmp) {
		l = gtk_label_new (tmp);
		gtk_misc_set_alignment (GTK_MISC (l), 0.0, 0.5);
		gtk_box_pack_start (GTK_BOX (w), l, FALSE, FALSE, 0);

		g_free (tmp);
		tmp = NULL;
	}

	tmp2 = g_string_new ("");
	e_cal_component_get_description_list (new_comp, &desc);
	for (len = 0, p = desc; p != NULL; p = p->next) {
		ECalComponentText *text = p->data;

		if (text->value != NULL) {
			len += strlen (text->value);
			g_string_append (tmp2, text->value);
			if (len > 1024) {
				g_string_set_size (tmp2, 1020);
				g_string_append (tmp2, "...");
				break;
			}
		}
	}
	e_cal_component_free_text_list (desc);

	if (tmp2->len) {
		l = gtk_label_new (tmp2->str);
		gtk_label_set_line_wrap (GTK_LABEL (l), TRUE);
		gtk_misc_set_alignment (GTK_MISC (l), 0.0, 0.5);
		gtk_box_pack_start (GTK_BOX (box), l, FALSE, FALSE, 0);
	}

	g_string_free (tmp2, TRUE);

	gtk_widget_show_all (box);
	gtk_tooltip_set_custom (tooltip, box);

	g_object_unref (new_comp);

	return TRUE;
}

static void
task_table_double_click (ETable *table,
                         gint row,
                         gint col,
                         GdkEvent *event)
{
	ETaskTable *task_table;
	ECalModel *model;
	ECalModelComponent *comp_data;

	task_table = E_TASK_TABLE (table);
	model = e_task_table_get_model (task_table);
	comp_data = e_cal_model_get_component_at (model, row);
	task_table_emit_open_component (task_table, comp_data);
}

static gint
task_table_right_click (ETable *table,
                        gint row,
                        gint col,
                        GdkEvent *event)
{
	ETaskTable *task_table;

	task_table = E_TASK_TABLE (table);
	task_table_emit_popup_event (task_table, event);

	return TRUE;
}

static void
task_table_update_actions (ESelectable *selectable,
                           EFocusTracker *focus_tracker,
                           GdkAtom *clipboard_targets,
                           gint n_clipboard_targets)
{
	ETaskTable *task_table;
	GtkAction *action;
	GtkTargetList *target_list;
	GSList *list, *iter;
	gboolean can_paste = FALSE;
	gboolean sources_are_editable = TRUE;
	gboolean sensitive;
	const gchar *tooltip;
	gint n_selected;
	gint ii;

	task_table = E_TASK_TABLE (selectable);
	n_selected = e_table_selected_count (E_TABLE (task_table));

	list = e_task_table_get_selected (task_table);
	for (iter = list; iter != NULL; iter = iter->next) {
		ECalModelComponent *comp_data = iter->data;
		gboolean read_only;

		e_cal_is_read_only (comp_data->client, &read_only, NULL);
		sources_are_editable &= !read_only;
	}
	g_slist_free (list);

	target_list = e_selectable_get_paste_target_list (selectable);
	for (ii = 0; ii < n_clipboard_targets && !can_paste; ii++)
		can_paste = gtk_target_list_find (
			target_list, clipboard_targets[ii], NULL);

	action = e_focus_tracker_get_cut_clipboard_action (focus_tracker);
	sensitive = (n_selected > 0) && sources_are_editable;
	tooltip = _("Cut selected tasks to the clipboard");
	gtk_action_set_sensitive (action, sensitive);
	gtk_action_set_tooltip (action, tooltip);

	action = e_focus_tracker_get_copy_clipboard_action (focus_tracker);
	sensitive = (n_selected > 0);
	tooltip = _("Copy selected tasks to the clipboard");
	gtk_action_set_sensitive (action, sensitive);
	gtk_action_set_tooltip (action, tooltip);

	action = e_focus_tracker_get_paste_clipboard_action (focus_tracker);
	sensitive = sources_are_editable && can_paste;
	tooltip = _("Paste tasks from the clipboard");
	gtk_action_set_sensitive (action, sensitive);
	gtk_action_set_tooltip (action, tooltip);

	action = e_focus_tracker_get_delete_selection_action (focus_tracker);
	sensitive = (n_selected > 0) && sources_are_editable;
	tooltip = _("Delete selected tasks");
	gtk_action_set_sensitive (action, sensitive);
	gtk_action_set_tooltip (action, tooltip);

	action = e_focus_tracker_get_select_all_action (focus_tracker);
	sensitive = TRUE;
	tooltip = _("Select all visible tasks");
	gtk_action_set_sensitive (action, sensitive);
	gtk_action_set_tooltip (action, tooltip);
}

static void
task_table_cut_clipboard (ESelectable *selectable)
{
	ETaskTable *task_table;

	task_table = E_TASK_TABLE (selectable);

	e_selectable_copy_clipboard (selectable);
	delete_selected_components (task_table);
}

/* Helper for task_table_copy_clipboard() */
static void
copy_row_cb (gint model_row, gpointer data)
{
	ETaskTable *task_table;
	ECalModelComponent *comp_data;
	ECalModel *model;
	gchar *comp_str;
	icalcomponent *child;

	task_table = E_TASK_TABLE (data);

	g_return_if_fail (task_table->tmp_vcal != NULL);

	model = e_task_table_get_model (task_table);
	comp_data = e_cal_model_get_component_at (model, model_row);
	if (!comp_data)
		return;

	/* Add timezones to the VCALENDAR component. */
	e_cal_util_add_timezones_from_component (
		task_table->tmp_vcal, comp_data->icalcomp);

	/* Add the new component to the VCALENDAR component. */
	comp_str = icalcomponent_as_ical_string_r (comp_data->icalcomp);
	child = icalparser_parse_string (comp_str);
	if (child) {
		icalcomponent_add_component (
			task_table->tmp_vcal,
			icalcomponent_new_clone (child));
		icalcomponent_free (child);
	}
	g_free (comp_str);
}

static void
task_table_copy_clipboard (ESelectable *selectable)
{
	ETaskTable *task_table;
	GtkClipboard *clipboard;
	gchar *comp_str;

	task_table = E_TASK_TABLE (selectable);

	/* Create a temporary VCALENDAR object. */
	task_table->tmp_vcal = e_cal_util_new_top_level ();

	e_table_selected_row_foreach (
		E_TABLE (task_table), copy_row_cb, task_table);
	comp_str = icalcomponent_as_ical_string_r (task_table->tmp_vcal);

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
	e_clipboard_set_calendar (clipboard, comp_str, -1);
	gtk_clipboard_store (clipboard);

	g_free (comp_str);

	icalcomponent_free (task_table->tmp_vcal);
	task_table->tmp_vcal = NULL;
}

/* Helper for calenable_table_paste_clipboard() */
static void
clipboard_get_calendar_data (ETaskTable *task_table,
                             const gchar *text)
{
	icalcomponent *icalcomp;
	gchar *uid;
	ECalComponent *comp;
	ECalModel *model;
	ECal *client;
	icalcomponent_kind kind;
	const gchar *status_message;

	g_return_if_fail (E_IS_TASK_TABLE (task_table));

	if (!text || !*text)
		return;

	icalcomp = icalparser_parse_string (text);
	if (!icalcomp)
		return;

	/* check the type of the component */
	kind = icalcomponent_isa (icalcomp);
	if (kind != ICAL_VCALENDAR_COMPONENT &&
	    kind != ICAL_VEVENT_COMPONENT &&
	    kind != ICAL_VTODO_COMPONENT &&
	    kind != ICAL_VJOURNAL_COMPONENT) {
		return;
	}

	model = e_task_table_get_model (task_table);
	client = e_cal_model_get_default_client (model);

	status_message = _("Updating objects");
	task_table_emit_status_message (task_table, status_message, -1.0);

	if (kind == ICAL_VCALENDAR_COMPONENT) {
		icalcomponent_kind child_kind;
		icalcomponent *subcomp;
		icalcomponent *vcal_comp;

		vcal_comp = icalcomp;
		subcomp = icalcomponent_get_first_component (
			vcal_comp, ICAL_ANY_COMPONENT);
		while (subcomp) {
			child_kind = icalcomponent_isa (subcomp);
			if (child_kind == ICAL_VEVENT_COMPONENT ||
			    child_kind == ICAL_VTODO_COMPONENT ||
			    child_kind == ICAL_VJOURNAL_COMPONENT) {
				ECalComponent *tmp_comp;

				uid = e_cal_component_gen_uid ();
				tmp_comp = e_cal_component_new ();
				e_cal_component_set_icalcomponent (
					tmp_comp,
					icalcomponent_new_clone (subcomp));
				e_cal_component_set_uid (tmp_comp, uid);
				free (uid);

				/* FIXME should we convert start/due/complete
				 * times?  Also, need error handling. */
				e_cal_create_object (
					client, e_cal_component_get_icalcomponent (tmp_comp),
					NULL, NULL);

				g_object_unref (tmp_comp);
			}
			subcomp = icalcomponent_get_next_component (
				vcal_comp, ICAL_ANY_COMPONENT);
		}
	} else {
		comp = e_cal_component_new ();
		e_cal_component_set_icalcomponent (comp, icalcomp);
		uid = e_cal_component_gen_uid ();
		e_cal_component_set_uid (comp, (const gchar *) uid);
		free (uid);

		e_cal_create_object (
			client, e_cal_component_get_icalcomponent (comp),
			NULL, NULL);

		g_object_unref (comp);
	}

	task_table_emit_status_message (task_table, NULL, -1.0);
}

static void
task_table_paste_clipboard (ESelectable *selectable)
{
	ETaskTable *task_table;
	GtkClipboard *clipboard;
	GnomeCanvasItem *item;
	GnomeCanvas *table_canvas;

	task_table = E_TASK_TABLE (selectable);

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);

	table_canvas = E_TABLE (task_table)->table_canvas;
	item = table_canvas->focused_item;

	/* XXX Should ECellText implement GtkEditable? */

	/* Paste text into a cell being edited. */
	if (gtk_clipboard_wait_is_text_available (clipboard) &&
		gtk_widget_has_focus (GTK_WIDGET (table_canvas)) &&
		E_IS_TABLE_ITEM (item) &&
		E_TABLE_ITEM (item)->editing_col >= 0 &&
		E_TABLE_ITEM (item)->editing_row >= 0) {

		ETableItem *etable_item = E_TABLE_ITEM (item);

		e_cell_text_paste_clipboard (
			etable_item->cell_views[etable_item->editing_col],
			etable_item->editing_col,
			etable_item->editing_row);

	/* Paste iCalendar data into the table. */
	} else if (e_clipboard_wait_is_calendar_available (clipboard)) {
		gchar *calendar_source;

		calendar_source = e_clipboard_wait_for_calendar (clipboard);
		clipboard_get_calendar_data (task_table, calendar_source);
		g_free (calendar_source);
	}
}

/* Used from e_table_selected_row_foreach(); puts the selected row number in an
 * gint pointed to by the closure data.
 */
static void
get_selected_row_cb (gint model_row, gpointer data)
{
	gint *row;

	row = data;
	*row = model_row;
}

/*
 * Returns the component that is selected in the table; only works if there is
 * one and only one selected row.
 */
static ECalModelComponent *
get_selected_comp (ETaskTable *task_table)
{
	ECalModel *model;
	gint row;

	model = e_task_table_get_model (task_table);
	if (e_table_selected_count (E_TABLE (task_table)) != 1)
		return NULL;

	row = -1;
	e_table_selected_row_foreach (
		E_TABLE (task_table), get_selected_row_cb, &row);
	g_return_val_if_fail (row != -1, NULL);

	return e_cal_model_get_component_at (model, row);
}

static void
add_retract_data (ECalComponent *comp, const gchar *retract_comment)
{
	icalcomponent *icalcomp = NULL;
	icalproperty *icalprop = NULL;

	icalcomp = e_cal_component_get_icalcomponent (comp);
	if (retract_comment && *retract_comment)
		icalprop = icalproperty_new_x (retract_comment);
	else
		icalprop = icalproperty_new_x ("0");
	icalproperty_set_x_name (icalprop, "X-EVOLUTION-RETRACT-COMMENT");
	icalcomponent_add_property (icalcomp, icalprop);
}

static gboolean
check_for_retract (ECalComponent *comp, ECal *client)
{
	ECalComponentOrganizer org;
	gchar *email = NULL;
	const gchar *strip = NULL;
	gboolean ret_val;

	if (!e_cal_component_has_attendees (comp))
		return FALSE;

	if (!e_cal_get_save_schedules (client))
		return FALSE;

	e_cal_component_get_organizer (comp, &org);
	strip = itip_strip_mailto (org.value);

	ret_val =
		e_cal_get_cal_address (client, &email, NULL) &&
		g_ascii_strcasecmp (email, strip) == 0;

	g_free (email);

	return ret_val;
}

static void
task_table_delete_selection (ESelectable *selectable)
{
	ETaskTable *task_table;
	ECalModelComponent *comp_data;
	ECalComponent *comp = NULL;
	gboolean delete = FALSE;
	gint n_selected;
	GError *error = NULL;

	task_table = E_TASK_TABLE (selectable);

	n_selected = e_table_selected_count (E_TABLE (task_table));
	if (n_selected <= 0)
		return;

	if (n_selected == 1)
		comp_data = get_selected_comp (task_table);
	else
		comp_data = NULL;

	/* FIXME: this may be something other than a TODO component */

	if (comp_data) {
		comp = e_cal_component_new ();
		e_cal_component_set_icalcomponent (
			comp, icalcomponent_new_clone (comp_data->icalcomp));
	}

	if ((n_selected == 1) && comp && check_for_retract (comp, comp_data->client)) {
		gchar *retract_comment = NULL;
		gboolean retract = FALSE;

		delete = prompt_retract_dialog (
			comp, &retract_comment,
			GTK_WIDGET (task_table), &retract);
		if (retract) {
			GList *users = NULL;
			icalcomponent *icalcomp = NULL, *mod_comp = NULL;

			add_retract_data (comp, retract_comment);
			icalcomp = e_cal_component_get_icalcomponent (comp);
			icalcomponent_set_method (icalcomp, ICAL_METHOD_CANCEL);
			if (!e_cal_send_objects (comp_data->client, icalcomp, &users,
						&mod_comp, &error))	{
				delete_error_dialog (error, E_CAL_COMPONENT_TODO);
				g_clear_error (&error);
				error = NULL;
			} else {

				if (mod_comp)
					icalcomponent_free (mod_comp);

				if (users) {
					g_list_foreach (users, (GFunc) g_free, NULL);
					g_list_free (users);
				}
			}

		}
	} else {
		delete = delete_component_dialog (
			comp, FALSE, n_selected,
			E_CAL_COMPONENT_TODO, GTK_WIDGET (task_table));
	}

	if (delete)
		delete_selected_components (task_table);

	/* free memory */
	if (comp)
		g_object_unref (comp);
}

static void
task_table_select_all (ESelectable *selectable)
{
	e_table_select_all (E_TABLE (selectable));
}

static void
task_table_class_init (ETaskTableClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	ETableClass *table_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (ETaskTablePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = task_table_set_property;
	object_class->get_property = task_table_get_property;
	object_class->dispose = task_table_dispose;
	object_class->constructed = task_table_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->popup_menu = task_table_popup_menu;
	widget_class->query_tooltip = task_table_query_tooltip;

	table_class = E_TABLE_CLASS (class);
	table_class->double_click = task_table_double_click;
	table_class->right_click = task_table_right_click;

	/* Inherited from ESelectableInterface */
	g_object_class_override_property (
		object_class,
		PROP_COPY_TARGET_LIST,
		"copy-target-list");

	g_object_class_install_property (
		object_class,
		PROP_MODEL,
		g_param_spec_object (
			"model",
			"Model",
			NULL,
			E_TYPE_CAL_MODEL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	/* Inherited from ESelectableInterface */
	g_object_class_override_property (
		object_class,
		PROP_PASTE_TARGET_LIST,
		"paste-target-list");

	g_object_class_install_property (
		object_class,
		PROP_SHELL_VIEW,
		g_param_spec_object (
			"shell-view",
			"Shell View",
			NULL,
			E_TYPE_SHELL_VIEW,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	signals[OPEN_COMPONENT] = g_signal_new (
		"open-component",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (ETaskTableClass, open_component),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_CAL_MODEL_COMPONENT);

	signals[POPUP_EVENT] = g_signal_new (
		"popup-event",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (ETaskTableClass, popup_event),
		NULL, NULL,
		g_cclosure_marshal_VOID__BOXED,
		G_TYPE_NONE, 1,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

	signals[STATUS_MESSAGE] = g_signal_new (
		"status-message",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (ETaskTableClass, status_message),
		NULL, NULL,
		e_marshal_VOID__STRING_DOUBLE,
		G_TYPE_NONE, 2,
		G_TYPE_STRING, G_TYPE_DOUBLE);
}

static void
task_table_init (ETaskTable *task_table)
{
	GtkTargetList *target_list;

	task_table->priv = E_TASK_TABLE_GET_PRIVATE (task_table);

	target_list = gtk_target_list_new (NULL, 0);
	e_target_list_add_calendar_targets (target_list, 0);
	task_table->priv->copy_target_list = target_list;

	target_list = gtk_target_list_new (NULL, 0);
	e_target_list_add_calendar_targets (target_list, 0);
	task_table->priv->paste_target_list = target_list;
}

static void
task_table_selectable_init (ESelectableInterface *interface)
{
	interface->update_actions = task_table_update_actions;
	interface->cut_clipboard = task_table_cut_clipboard;
	interface->copy_clipboard = task_table_copy_clipboard;
	interface->paste_clipboard = task_table_paste_clipboard;
	interface->delete_selection = task_table_delete_selection;
	interface->select_all = task_table_select_all;
}

GType
e_task_table_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (ETaskTableClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) task_table_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (ETaskTable),
			0,     /* n_preallocs */
			(GInstanceInitFunc) task_table_init,
			NULL   /* value_table */
		};

		static const GInterfaceInfo selectable_info = {
			(GInterfaceInitFunc) task_table_selectable_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL   /* interface_data */
		};

		type = g_type_register_static (
			E_TABLE_TYPE, "ETaskTable", &type_info, 0);

		g_type_add_interface_static (
			type, E_TYPE_SELECTABLE, &selectable_info);
	}

	return type;
}

/**
 * e_task_table_new:
 * @shell_view: an #EShellView
 * @model: an #ECalModel for the table
 *
 * Returns a new #ETaskTable.
 *
 * Returns: a new #ETaskTable
 **/
GtkWidget *
e_task_table_new (EShellView *shell_view,
                  ECalModel *model)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	return g_object_new (
		E_TYPE_TASK_TABLE,
		"model", model, "shell-view", shell_view, NULL);
}

/**
 * e_task_table_get_model:
 * @task_table: A calendar table.
 *
 * Queries the calendar data model that a calendar table is using.
 *
 * Return value: A calendar model.
 **/
ECalModel *
e_task_table_get_model (ETaskTable *task_table)
{
	g_return_val_if_fail (E_IS_TASK_TABLE (task_table), NULL);

	return task_table->priv->model;
}

EShellView *
e_task_table_get_shell_view (ETaskTable *task_table)
{
	g_return_val_if_fail (E_IS_TASK_TABLE (task_table), NULL);

	return task_table->priv->shell_view;
}

struct get_selected_uids_closure {
	ETaskTable *task_table;
	GSList *objects;
};

/* Used from e_table_selected_row_foreach(), builds a list of the selected UIDs */
static void
add_uid_cb (gint model_row, gpointer data)
{
	struct get_selected_uids_closure *closure = data;
	ECalModelComponent *comp_data;
	ECalModel *model;

	model = e_task_table_get_model (closure->task_table);
	comp_data = e_cal_model_get_component_at (model, model_row);

	closure->objects = g_slist_prepend (closure->objects, comp_data);
}

/**
 * e_task_table_get_selected:
 * @task_table:
 *
 * Get the currently selected ECalModelComponent's on the table.
 *
 * Return value: A GSList of the components, which should be
 * g_slist_free'd when finished with.
 **/
GSList *
e_task_table_get_selected (ETaskTable *task_table)
{
	struct get_selected_uids_closure closure;

	closure.task_table = task_table;
	closure.objects = NULL;

	e_table_selected_row_foreach (
		E_TABLE (task_table), add_uid_cb, &closure);

	return closure.objects;
}

GtkTargetList *
e_task_table_get_copy_target_list (ETaskTable *task_table)
{
	g_return_val_if_fail (E_IS_TASK_TABLE (task_table), NULL);

	return task_table->priv->copy_target_list;
}

GtkTargetList *
e_task_table_get_paste_target_list (ETaskTable *task_table)
{
	g_return_val_if_fail (E_IS_TASK_TABLE (task_table), NULL);

	return task_table->priv->paste_target_list;
}

static void
hide_completed_rows (ECalModel *model,
                     GList *clients_list,
                     gchar *hide_sexp,
                     GPtrArray *comp_objects)
{
	GList *l, *m, *objects;
	ECal *client;
	gint pos;
	gboolean changed = FALSE;

	for (l = clients_list; l != NULL; l = l->next) {
		client = l->data;

		if (!e_cal_get_object_list (client, hide_sexp, &objects, NULL)) {
			g_warning (G_STRLOC ": Could not get the objects");

			continue;
		}

		for (m = objects; m; m = m->next) {
			ECalModelComponent *comp_data;
			ECalComponentId *id;
			ECalComponent *comp = e_cal_component_new ();

			e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (m->data));
			id = e_cal_component_get_id (comp);

			if ((comp_data =  e_cal_model_get_component_for_uid (model, id))) {
				e_table_model_pre_change (E_TABLE_MODEL (model));
				pos = get_position_in_array (comp_objects, comp_data);
				e_table_model_row_deleted (E_TABLE_MODEL (model), pos);
				changed = TRUE;

				if (g_ptr_array_remove (comp_objects, comp_data))
					g_object_unref (comp_data);
			}
			e_cal_component_free_id (id);
			g_object_unref (comp);
		}

		g_list_foreach (objects, (GFunc) icalcomponent_free, NULL);
		g_list_free (objects);
	}

	if (changed) {
		/* To notify about changes, because in call of
		 * row_deleted there are still all events. */
		e_table_model_changed (E_TABLE_MODEL (model));
	}
}

static void
show_completed_rows (ECalModel *model,
                     GList *clients_list,
                     gchar *show_sexp,
                     GPtrArray *comp_objects)
{
	GList *l, *m, *objects;
	ECal *client;

	for (l = clients_list; l != NULL; l = l->next) {
		client = l->data;

		if (!e_cal_get_object_list (client, show_sexp, &objects, NULL)) {
			g_warning (G_STRLOC ": Could not get the objects");

			continue;
		}

		for (m = objects; m; m = m->next) {
			ECalModelComponent *comp_data;
			ECalComponentId *id;
			ECalComponent *comp = e_cal_component_new ();

			e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (m->data));
			id = e_cal_component_get_id (comp);

			if (!(e_cal_model_get_component_for_uid (model, id))) {
				e_table_model_pre_change (E_TABLE_MODEL (model));
				comp_data = g_object_new (E_TYPE_CAL_MODEL_COMPONENT, NULL);
				comp_data->client = g_object_ref (client);
				comp_data->icalcomp = icalcomponent_new_clone (m->data);
				e_cal_model_set_instance_times (comp_data,
						e_cal_model_get_timezone (model));
				comp_data->dtstart = NULL;
				comp_data->dtend = NULL;
				comp_data->due = NULL;
				comp_data->completed = NULL;
				comp_data->color = NULL;

				g_ptr_array_add (comp_objects, comp_data);
				e_table_model_row_inserted (E_TABLE_MODEL (model), comp_objects->len - 1);
			}
			e_cal_component_free_id (id);
			g_object_unref (comp);
		}
	}
}

/* Returns the current time, for the ECellDateEdit items.
   FIXME: Should probably use the timezone of the item rather than the
   current timezone, though that may be difficult to get from here. */
static struct tm
e_task_table_get_current_time (ECellDateEdit *ecde, gpointer data)
{
	ETaskTable *task_table = data;
	ECalModel *model;
	icaltimezone *zone;
	struct tm tmp_tm = { 0 };
	struct icaltimetype tt;

	/* Get the current timezone. */
	model = e_task_table_get_model (task_table);
	zone = e_cal_model_get_timezone (model);

	tt = icaltime_from_timet_with_zone (time (NULL), FALSE, zone);

	/* Now copy it to the struct tm and return it. */
	tmp_tm.tm_year  = tt.year - 1900;
	tmp_tm.tm_mon   = tt.month - 1;
	tmp_tm.tm_mday  = tt.day;
	tmp_tm.tm_hour  = tt.hour;
	tmp_tm.tm_min   = tt.minute;
	tmp_tm.tm_sec   = tt.second;
	tmp_tm.tm_isdst = -1;

	return tmp_tm;
}

/**
 * e_task_table_hide_completed_tasks:
 * @table: A calendar table model.
 * @client_list: Clients List
 *
 * Hide completed tasks.
 */
void
e_task_table_process_completed_tasks (ETaskTable *task_table,
                                      GList *clients_list,
                                      gboolean config_changed)
{
	ECalModel *model;
	static GMutex *mutex = NULL;
	gchar *hide_sexp, *show_sexp;
	GPtrArray *comp_objects = NULL;

	if (!mutex)
		mutex = g_mutex_new ();

	g_mutex_lock (mutex);

	model = e_task_table_get_model (task_table);
	comp_objects = e_cal_model_get_object_array (model);

	hide_sexp = calendar_config_get_hide_completed_tasks_sexp (TRUE);
	show_sexp = calendar_config_get_hide_completed_tasks_sexp (FALSE);

	/* If hide option is unchecked */
	if (!(hide_sexp && show_sexp))
		show_sexp = g_strdup ("(is-completed?)");

	/* Delete rows from model*/
	if (hide_sexp) {
		hide_completed_rows (model, clients_list, hide_sexp, comp_objects);
	}

	/* Insert rows into model */
	if (config_changed) {
		show_completed_rows (model, clients_list, show_sexp, comp_objects);
	}

	g_free (hide_sexp);
	g_free (show_sexp);
	g_mutex_unlock (mutex);
}
