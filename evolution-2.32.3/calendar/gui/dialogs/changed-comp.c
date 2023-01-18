/*
 * Evolution calendar - Send calendar component dialog
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
 *		JP Rosevear <jpr@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include "changed-comp.h"



/**
 * changed_component_dialog:
 * @parent: Parent window for the dialog.
 * @comp: A calendar component
 * @deleted: Whether the object is being deleted or updated
 * @changed: Whether or not the user has made changes
 *
 * Pops up a dialog box asking the user whether changes made (if any)
 * should be thrown away because the item has been updated elsewhere
 *
 * Return value: TRUE if the user clicked Yes, FALSE otherwise.
 **/
gboolean
changed_component_dialog (GtkWindow *parent, ECalComponent *comp, gboolean deleted, gboolean changed)
{
	GtkWidget *dialog;
	ECalComponentVType vtype;
	gchar *str;
	gint response;

	vtype = e_cal_component_get_vtype (comp);

	if (deleted) {
		switch (vtype) {
		case E_CAL_COMPONENT_EVENT:
			str = _("This event has been deleted.");
			break;

		case E_CAL_COMPONENT_TODO:
			str = _("This task has been deleted.");
			break;

		case E_CAL_COMPONENT_JOURNAL:
			str = _("This memo has been deleted.");
			break;

		default:
			g_message ("changed_component_dialog(): "
				   "Cannot handle object of type %d", vtype);
			return FALSE;
		}
		if (changed)
			str = g_strdup_printf (_("%s  You have made changes. Forget those changes and close the editor?"), str);
		else
			str = g_strdup_printf (_("%s  You have made no changes, close the editor?"), str);

	} else {
		switch (vtype) {
		case E_CAL_COMPONENT_EVENT:
			str = _("This event has been changed.");
			break;

		case E_CAL_COMPONENT_TODO:
			str = _("This task has been changed.");
			break;

		case E_CAL_COMPONENT_JOURNAL:
			str = _("This memo has been changed.");
			break;

		default:
			g_message ("changed_component_dialog(): "
				   "Cannot handle object of type %d", vtype);
			return FALSE;
		}
		if (changed)
			str = g_strdup_printf (_("%s  You have made changes. Forget those changes and update the editor?"), str);
		else
			str = g_strdup_printf (_("%s  You have made no changes, update the editor?"), str);
	}

	dialog = gtk_message_dialog_new (parent, GTK_DIALOG_MODAL,
					 GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_YES_NO, "%s", str);

	gtk_window_set_icon_name (GTK_WINDOW (dialog), "x-office-calendar");

	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	if (response == GTK_RESPONSE_YES)
		return TRUE;
	else
		return FALSE;
}
