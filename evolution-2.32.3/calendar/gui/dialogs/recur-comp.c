/*
 * Evolution calendar - Recurring calendar component dialog
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
#include "recur-comp.h"



gboolean
recur_component_dialog (ECal *client,
			ECalComponent *comp,
			CalObjModType *mod,
			GtkWindow *parent, gboolean delegated)
{
	gchar *str;
	GtkWidget *dialog, *rb_this, *rb_prior, *rb_future, *rb_all, *hbox;
	GtkWidget *placeholder, *vbox;
	GtkWidget *content_area;
	ECalComponentVType vtype;
	gboolean ret;

	g_return_val_if_fail (E_IS_CAL_COMPONENT (comp), FALSE);

	vtype = e_cal_component_get_vtype (comp);

	switch (vtype) {
	case E_CAL_COMPONENT_EVENT:
		if (!delegated)
			str = g_strdup_printf (_("You are modifying a recurring event. What would you like to modify?"));
		else
			str = g_strdup_printf (_("You are delegating a recurring event. What would you like to delegate?"));
		break;

	case E_CAL_COMPONENT_TODO:
		str = g_strdup_printf (_("You are modifying a recurring task. What would you like to modify?"));
		break;

	case E_CAL_COMPONENT_JOURNAL:
		str = g_strdup_printf (_("You are modifying a recurring memo. What would you like to modify?"));
		break;

	default:
		g_message ("recur_component_dialog(): Cannot handle object of type %d", vtype);
		return FALSE;
	}

	dialog = gtk_message_dialog_new (parent, 0, GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL, "%s", str);
	g_free (str);
	gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

	hbox = gtk_hbox_new (FALSE, 12);
	gtk_container_add (GTK_CONTAINER (content_area), hbox);

	placeholder = gtk_label_new ("");
	gtk_widget_set_size_request (placeholder, 48, 48);
	gtk_box_pack_start (GTK_BOX (hbox), placeholder, FALSE, FALSE, 0);
	gtk_widget_show (placeholder);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
	gtk_widget_show (vbox);

	rb_this = gtk_radio_button_new_with_label (NULL, _("This Instance Only"));
	gtk_container_add (GTK_CONTAINER (vbox), rb_this);

	if (!e_cal_get_static_capability (client, CAL_STATIC_CAPABILITY_NO_THISANDPRIOR)) {
		rb_prior = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (rb_this), _("This and Prior Instances"));
		gtk_container_add (GTK_CONTAINER (vbox), rb_prior);
	} else
		rb_prior = NULL;

	if (!e_cal_get_static_capability (client, CAL_STATIC_CAPABILITY_NO_THISANDFUTURE)) {
		rb_future = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (rb_this), _("This and Future Instances"));
		gtk_container_add (GTK_CONTAINER (vbox), rb_future);
	} else
		rb_future = NULL;

	rb_all = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (rb_this), _("All Instances"));
	gtk_container_add (GTK_CONTAINER (vbox), rb_all);

	gtk_widget_show_all (hbox);

	placeholder = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX (content_area), placeholder, FALSE, FALSE, 0);
	gtk_widget_show (placeholder);

	ret = gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (rb_this)))
		*mod = CALOBJ_MOD_THIS;
	else if (rb_prior && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (rb_prior)))
		*mod = CALOBJ_MOD_THISANDPRIOR;
	else if (rb_future && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (rb_future)))
		*mod = CALOBJ_MOD_THISANDFUTURE;
	else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (rb_all))) {
		*mod = CALOBJ_MOD_ALL;

	}

	gtk_widget_destroy (dialog);

	return ret;
}
