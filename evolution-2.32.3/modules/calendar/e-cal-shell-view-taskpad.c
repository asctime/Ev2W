/*
 * e-cal-shell-view-taskpad.c
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-cal-shell-view-private.h"

/* Much of this file is based on e-task-shell-view-actions.c. */

static void
action_calendar_taskpad_assign_cb (GtkAction *action,
                                   ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	ECalModelComponent *comp_data;
	ETaskTable *task_table;
	GSList *list;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	task_table = e_cal_shell_content_get_task_table (cal_shell_content);

	list = e_task_table_get_selected (task_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;
	g_slist_free (list);

	/* XXX We only open the first selected task. */
	e_cal_shell_view_taskpad_open_task (cal_shell_view, comp_data);

	/* FIXME Need to actually assign the task. */
}

static void
action_calendar_taskpad_forward_cb (GtkAction *action,
                                    ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	ECalModelComponent *comp_data;
	ETaskTable *task_table;
	ECalComponent *comp;
	icalcomponent *clone;
	GSList *list;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	task_table = e_cal_shell_content_get_task_table (cal_shell_content);

	list = e_task_table_get_selected (task_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;
	g_slist_free (list);

	/* XXX We only forward the first selected task. */
	comp = e_cal_component_new ();
	clone = icalcomponent_new_clone (comp_data->icalcomp);
	e_cal_component_set_icalcomponent (comp, clone);
	itip_send_comp (
		E_CAL_COMPONENT_METHOD_PUBLISH, comp,
		comp_data->client, NULL, NULL, NULL, TRUE, FALSE);
	g_object_unref (comp);
}

static void
action_calendar_taskpad_mark_complete_cb (GtkAction *action,
                                          ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	ETaskTable *task_table;
	ECalModel *model;
	GSList *list, *iter;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	task_table = e_cal_shell_content_get_task_table (cal_shell_content);
	list = e_task_table_get_selected (task_table);
	model = e_task_table_get_model (task_table);

	for (iter = list; iter != NULL; iter = iter->next) {
		ECalModelComponent *comp_data = iter->data;
		e_cal_model_tasks_mark_comp_complete (
			E_CAL_MODEL_TASKS (model), comp_data);
	}

	g_slist_free (list);
}

static void
action_calendar_taskpad_mark_incomplete_cb (GtkAction *action,
                                            ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	ETaskTable *task_table;
	ECalModel *model;
	GSList *list, *iter;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	task_table = e_cal_shell_content_get_task_table (cal_shell_content);
	list = e_task_table_get_selected (task_table);
	model = e_task_table_get_model (task_table);

	for (iter = list; iter != NULL; iter = iter->next) {
		ECalModelComponent *comp_data = iter->data;
		e_cal_model_tasks_mark_comp_incomplete (
			E_CAL_MODEL_TASKS (model), comp_data);
	}

	g_slist_free (list);
}

static void
action_calendar_taskpad_new_cb (GtkAction *action,
                                ECalShellView *cal_shell_view)
{
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	ECalShellContent *cal_shell_content;
	ECalModelComponent *comp_data;
	ETaskTable *task_table;
	ECal *client;
	ECalComponent *comp;
	CompEditor *editor;
	GSList *list;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	task_table = e_cal_shell_content_get_task_table (cal_shell_content);

	list = e_task_table_get_selected (task_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;
	g_slist_free (list);

	client = comp_data->client;
	editor = task_editor_new (client, shell, COMP_EDITOR_NEW_ITEM);
	comp = cal_comp_task_new_with_defaults (client);
	comp_editor_edit_comp (editor, comp);

	gtk_window_present (GTK_WINDOW (editor));

	g_object_unref (comp);
	g_object_unref (client);
}

static void
action_calendar_taskpad_open_cb (GtkAction *action,
                                 ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	ECalModelComponent *comp_data;
	ETaskTable *task_table;
	GSList *list;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	task_table = e_cal_shell_content_get_task_table (cal_shell_content);

	list = e_task_table_get_selected (task_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;
	g_slist_free (list);

	/* XXX We only open the first selected task. */
	e_cal_shell_view_taskpad_open_task (cal_shell_view, comp_data);
}

static void
action_calendar_taskpad_open_url_cb (GtkAction *action,
                                     ECalShellView *cal_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	ECalShellContent *cal_shell_content;
	ECalModelComponent *comp_data;
	ETaskTable *task_table;
	icalproperty *prop;
	const gchar *uri;
	GSList *list;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	task_table = e_cal_shell_content_get_task_table (cal_shell_content);

	list = e_task_table_get_selected (task_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;

	/* XXX We only open the URI of the first selected task. */
	prop = icalcomponent_get_first_property (
		comp_data->icalcomp, ICAL_URL_PROPERTY);
	g_return_if_fail (prop != NULL);

	uri = icalproperty_get_url (prop);
	e_show_uri (GTK_WINDOW (shell_window), uri);
}

static void
action_calendar_taskpad_print_cb (GtkAction *action,
                                  ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	ECalModelComponent *comp_data;
	ETaskTable *task_table;
	ECalComponent *comp;
	icalcomponent *clone;
	GtkPrintOperationAction print_action;
	GSList *list;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	task_table = e_cal_shell_content_get_task_table (cal_shell_content);

	list = e_task_table_get_selected (task_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;
	g_slist_free (list);

	/* XXX We only print the first selected task. */
	comp = e_cal_component_new ();
	clone = icalcomponent_new_clone (comp_data->icalcomp);
	print_action = GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG;
	e_cal_component_set_icalcomponent (comp, clone);
	print_comp (comp, comp_data->client, print_action);
	g_object_unref (comp);
}

static void
action_calendar_taskpad_save_as_cb (GtkAction *action,
                                    ECalShellView *cal_shell_view)
{
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellBackend *shell_backend;
	ECalShellContent *cal_shell_content;
	ECalModelComponent *comp_data;
	ETaskTable *task_table;
	EActivity *activity;
	GSList *list;
	GFile *file;
	gchar *string;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	task_table = e_cal_shell_content_get_task_table (cal_shell_content);

	list = e_task_table_get_selected (task_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;
	g_slist_free (list);

	/* Translators: Default filename part saving a task to a file when
	 * no summary is filed, the '.ics' extension is concatenated to it. */
	string = icalcomp_suggest_filename (comp_data->icalcomp, _("task"));
	file = e_shell_run_save_dialog (
		shell, _("Save as iCalendar"), string,
		"*.ics:text/calendar", NULL, NULL);
	g_free (string);
	if (file == NULL)
		return;

	string = e_cal_get_component_as_string (
		comp_data->client, comp_data->icalcomp);
	if (string == NULL) {
		g_warning ("Could not convert task to a string");
		g_object_unref (file);
		return;
	}

	/* XXX No callback means errors are discarded. */
	activity = e_file_replace_contents_async (
		file, string, strlen (string), NULL, FALSE,
		G_FILE_CREATE_NONE, (GAsyncReadyCallback) NULL, NULL);
	e_shell_backend_add_activity (shell_backend, activity);

	/* Free the string when the activity is finalized. */
	g_object_set_data_full (
		G_OBJECT (activity),
		"file-content", string,
		(GDestroyNotify) g_free);

	g_object_unref (file);
}

static GtkActionEntry calendar_taskpad_entries[] = {

	{ "calendar-taskpad-assign",
	  NULL,
	  N_("_Assign Task"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_calendar_taskpad_assign_cb) },

	{ "calendar-taskpad-forward",
	  "mail-forward",
	  N_("_Forward as iCalendar..."),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_calendar_taskpad_forward_cb) },

	{ "calendar-taskpad-mark-complete",
	  NULL,
	  N_("_Mark as Complete"),
	  NULL,
	  N_("Mark selected tasks as complete"),
	  G_CALLBACK (action_calendar_taskpad_mark_complete_cb) },

	{ "calendar-taskpad-mark-incomplete",
	  NULL,
	  N_("_Mark as Incomplete"),
	  NULL,
	  N_("Mark selected tasks as incomplete"),
	  G_CALLBACK (action_calendar_taskpad_mark_incomplete_cb) },

	{ "calendar-taskpad-new",
	  "stock_task",
	  N_("New _Task"),
	  NULL,
	  N_("Create a new task"),
	  G_CALLBACK (action_calendar_taskpad_new_cb) },

	{ "calendar-taskpad-open",
	  GTK_STOCK_OPEN,
	  N_("_Open Task"),
	  NULL,
	  N_("View the selected task"),
	  G_CALLBACK (action_calendar_taskpad_open_cb) },

	{ "calendar-taskpad-open-url",
	  "applications-internet",
	  N_("Open _Web Page"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_calendar_taskpad_open_url_cb) },

	{ "calendar-taskpad-save-as",
	  GTK_STOCK_SAVE_AS,
	  N_("_Save as iCalendar..."),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_calendar_taskpad_save_as_cb) }
};

static GtkActionEntry lockdown_printing_entries[] = {

	{ "calendar-taskpad-print",
	  GTK_STOCK_PRINT,
	  NULL,
	  NULL,
	  N_("Print the selected task"),
	  G_CALLBACK (action_calendar_taskpad_print_cb) }
};

void
e_cal_shell_view_taskpad_actions_init (ECalShellView *cal_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	GtkActionGroup *action_group;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	/* Calendar Actions */
	action_group = ACTION_GROUP (CALENDAR);
	gtk_action_group_add_actions (
		action_group, calendar_taskpad_entries,
		G_N_ELEMENTS (calendar_taskpad_entries), cal_shell_view);

	/* Lockdown Printing Actions */
	action_group = ACTION_GROUP (LOCKDOWN_PRINTING);
	gtk_action_group_add_actions (
		action_group, lockdown_printing_entries,
		G_N_ELEMENTS (lockdown_printing_entries), cal_shell_view);
}

void
e_cal_shell_view_taskpad_actions_update (ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	EShellWindow *shell_window;
	EShellView *shell_view;
	ETaskTable *task_table;
	GtkAction *action;
	GSList *list, *iter;
	gboolean assignable = TRUE;
	gboolean editable = TRUE;
	gboolean has_url = FALSE;
	gboolean sensitive;
	gint n_selected;
	gint n_complete = 0;
	gint n_incomplete = 0;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	task_table = e_cal_shell_content_get_task_table (cal_shell_content);

	n_selected = e_table_selected_count (E_TABLE (task_table));

	list = e_task_table_get_selected (task_table);
	for (iter = list; iter != NULL; iter = iter->next) {
		ECalModelComponent *comp_data = iter->data;
		icalproperty *prop;
		const gchar *cap;
		gboolean read_only;

		e_cal_is_read_only (comp_data->client, &read_only, NULL);
		editable &= !read_only;

		cap = CAL_STATIC_CAPABILITY_NO_TASK_ASSIGNMENT;
		if (e_cal_get_static_capability (comp_data->client, cap))
			assignable = FALSE;

		cap = CAL_STATIC_CAPABILITY_NO_CONV_TO_ASSIGN_TASK;
		if (e_cal_get_static_capability (comp_data->client, cap))
			assignable = FALSE;

		prop = icalcomponent_get_first_property (
			comp_data->icalcomp, ICAL_URL_PROPERTY);
		has_url |= (prop != NULL);

		prop = icalcomponent_get_first_property (
			comp_data->icalcomp, ICAL_COMPLETED_PROPERTY);
		if (prop != NULL)
			n_complete++;
		else
			n_incomplete++;
	}
	g_slist_free (list);

	action = ACTION (CALENDAR_TASKPAD_ASSIGN);
	sensitive = (n_selected == 1) && editable && assignable;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CALENDAR_TASKPAD_FORWARD);
	sensitive = (n_selected == 1);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CALENDAR_TASKPAD_MARK_COMPLETE);
	sensitive = (n_selected > 0) && editable && (n_incomplete > 0);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CALENDAR_TASKPAD_MARK_INCOMPLETE);
	sensitive = (n_selected > 0) && editable && (n_complete > 0);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CALENDAR_TASKPAD_OPEN);
	sensitive = (n_selected == 1);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CALENDAR_TASKPAD_OPEN_URL);
	sensitive = (n_selected == 1) && has_url;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CALENDAR_TASKPAD_PRINT);
	sensitive = (n_selected == 1);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CALENDAR_TASKPAD_SAVE_AS);
	sensitive = (n_selected == 1);
	gtk_action_set_sensitive (action, sensitive);
}

void
e_cal_shell_view_taskpad_open_task (ECalShellView *cal_shell_view,
                                    ECalModelComponent *comp_data)
{
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	CompEditor *editor;
	CompEditorFlags flags = 0;
	ECalComponent *comp;
	icalcomponent *clone;
	icalproperty *prop;
	const gchar *uid;

	g_return_if_fail (E_IS_CAL_SHELL_VIEW (cal_shell_view));
	g_return_if_fail (E_IS_CAL_MODEL_COMPONENT (comp_data));

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	uid = icalcomponent_get_uid (comp_data->icalcomp);
	editor = comp_editor_find_instance (uid);

	if (editor != NULL)
		goto exit;

	comp = e_cal_component_new ();
	clone = icalcomponent_new_clone (comp_data->icalcomp);
	e_cal_component_set_icalcomponent (comp, clone);

	prop = icalcomponent_get_first_property (
		comp_data->icalcomp, ICAL_ATTENDEE_PROPERTY);
	if (prop != NULL)
		flags |= COMP_EDITOR_IS_ASSIGNED;

	if (itip_organizer_is_user (comp, comp_data->client))
		flags |= COMP_EDITOR_USER_ORG;

	if (!e_cal_component_has_attendees (comp))
		flags |= COMP_EDITOR_USER_ORG;

	editor = task_editor_new (comp_data->client, shell, flags);
	comp_editor_edit_comp (editor, comp);

	g_object_ref (comp);

	if (flags & COMP_EDITOR_IS_ASSIGNED)
		task_editor_show_assignment (TASK_EDITOR (editor));

exit:
	gtk_window_present (GTK_WINDOW (editor));
}

void
e_cal_shell_view_taskpad_set_status_message (ECalShellView *cal_shell_view,
                                             const gchar *status_message,
                                             gdouble percent)
{
	EActivity *activity;
	EShellView *shell_view;
	EShellBackend *shell_backend;

	g_return_if_fail (E_IS_CAL_SHELL_VIEW (cal_shell_view));

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);

	activity = cal_shell_view->priv->taskpad_activity;

	if (status_message == NULL || *status_message == '\0') {
		if (activity != NULL) {
			e_activity_complete (activity);
			g_object_unref (activity);
			activity = NULL;
		}

	} else if (activity == NULL) {
		activity = e_activity_new (status_message);
		e_activity_set_percent (activity, percent);
		e_shell_backend_add_activity (shell_backend, activity);

	} else {
		e_activity_set_percent (activity, percent);
		e_activity_set_primary_text (activity, status_message);
	}

	cal_shell_view->priv->taskpad_activity = activity;
}
