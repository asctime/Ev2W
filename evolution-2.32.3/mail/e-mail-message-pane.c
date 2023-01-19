/*
 * e-mail-browser.c
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

#include "e-mail-message-pane.h"

#include <string.h>
#include <glib/gi18n.h>

#include "e-util/gconf-bridge.h"

#include "mail/e-mail-reader.h"

#define E_MAIL_MESSAGE_PANE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_MESSAGE_PANE, EMailMessagePanePrivate))

struct _EMailMessagePanePrivate {
	gint placeholder;
};

G_DEFINE_TYPE (EMailMessagePane, e_mail_message_pane, E_TYPE_MAIL_PANED_VIEW)

static void
mail_message_pane_constructed (GObject *object)
{
	EMailMessagePanePrivate *priv;

	priv = E_MAIL_MESSAGE_PANE_GET_PRIVATE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_message_pane_parent_class)->constructed (object);

	gtk_widget_hide (e_mail_reader_get_message_list (E_MAIL_READER(object)));
	e_mail_paned_view_hide_message_list_pane (E_MAIL_PANED_VIEW(object), FALSE);
}

static gboolean
mail_message_pane_get_preview_visible (EMailView *view)
{
	return TRUE;
}

static void
mail_message_pane_set_preview_visible (EMailView *view,
                                       gboolean preview_visible)
{
	/* Ignore the request. */
}

static void
e_mail_message_pane_class_init (EMailMessagePaneClass *class)
{
	GObjectClass *object_class;
	EMailViewClass *mail_view_class;

	g_type_class_add_private (class, sizeof (EMailMessagePanePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = mail_message_pane_constructed;

	mail_view_class = E_MAIL_VIEW_CLASS (class);
	mail_view_class->get_preview_visible = mail_message_pane_get_preview_visible;
	mail_view_class->set_preview_visible = mail_message_pane_set_preview_visible;
}

static void
e_mail_message_pane_init (EMailMessagePane *browser)
{
	browser->priv = E_MAIL_MESSAGE_PANE_GET_PRIVATE (browser);
}

EMailView *
e_mail_message_pane_new (EShellView *shell_view)
{
	EMailView *widget;

	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	widget = g_object_new (
		E_TYPE_MAIL_MESSAGE_PANE,
		"shell-view", shell_view, NULL);

	e_mail_paned_view_set_enable_show_folder (E_MAIL_PANED_VIEW(widget), TRUE);

	return widget;
}
