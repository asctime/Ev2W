/*
 * e-menu-tool-button.h
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

/* EMenuToolButton is a variation of GtkMenuToolButton where the
 * button icon always reflects the first menu item, and clicking
 * the button activates the first menu item. */

#ifndef E_MENU_TOOL_BUTTON_H
#define E_MENU_TOOL_BUTTON_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_MENU_TOOL_BUTTON \
	(e_menu_tool_button_get_type ())
#define E_MENU_TOOL_BUTTON(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MENU_TOOL_BUTTON, EMenuToolButton))
#define E_MENU_TOOL_BUTTON_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MENU_TOOL_BUTTON, EMenuToolButtonClass))
#define E_IS_MENU_TOOL_BUTTON(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MENU_TOOL_BUTTON))
#define E_IS_MENU_TOOL_BUTTON_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MENU_TOOL_BUTTON))
#define E_MENU_TOOL_BUTTON_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MENU_TOOL_BUTTON, EMenuToolButtonClass))

G_BEGIN_DECLS

typedef struct _EMenuToolButton EMenuToolButton;
typedef struct _EMenuToolButtonClass EMenuToolButtonClass;

struct _EMenuToolButton {
	GtkMenuToolButton parent;
};

struct _EMenuToolButtonClass {
	GtkMenuToolButtonClass parent_class;
};

GType		e_menu_tool_button_get_type	(void);
GtkToolItem *	e_menu_tool_button_new		(const gchar *label);

G_END_DECLS

#endif /* E_MENU_TOOL_BUTTON_H */
