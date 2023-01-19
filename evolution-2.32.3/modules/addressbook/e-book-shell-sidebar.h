/*
 * e-book-shell-sidebar.h
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

#ifndef E_BOOK_SHELL_SIDEBAR_H
#define E_BOOK_SHELL_SIDEBAR_H

#include <libedataserverui/e-source-selector.h>

#include <shell/e-shell-sidebar.h>
#include <shell/e-shell-view.h>

/* Standard GObject macros */
#define E_TYPE_BOOK_SHELL_SIDEBAR \
	(e_book_shell_sidebar_get_type ())
#define E_BOOK_SHELL_SIDEBAR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_BOOK_SHELL_SIDEBAR, EBookShellSidebar))
#define E_BOOK_SHELL_SIDEBAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_BOOK_SHELL_SIDEBAR, EBookShellSidebarClass))
#define E_IS_BOOK_SHELL_SIDEBAR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_BOOK_SHELL_SIDEBAR))
#define E_IS_BOOK_SHELL_SIDEBAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_BOOK_SHELL_SIDEBAR))
#define E_BOOK_SHELL_SIDEBAR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_BOOK_SHELL_SIDEBAR, EBookShellSidebarClass))

G_BEGIN_DECLS

typedef struct _EBookShellSidebar EBookShellSidebar;
typedef struct _EBookShellSidebarClass EBookShellSidebarClass;
typedef struct _EBookShellSidebarPrivate EBookShellSidebarPrivate;

enum {
	E_BOOK_SHELL_SIDEBAR_HAS_PRIMARY_SOURCE		= 1 << 0,
	E_BOOK_SHELL_SIDEBAR_CAN_DELETE_PRIMARY_SOURCE	= 1 << 1,
	E_BOOK_SHELL_SIDEBAR_PRIMARY_SOURCE_IS_SYSTEM	= 1 << 2
};

struct _EBookShellSidebar {
	EShellSidebar parent;
	EBookShellSidebarPrivate *priv;
};

struct _EBookShellSidebarClass {
	EShellSidebarClass parent_class;
};

GType		e_book_shell_sidebar_get_type	(void);
void		e_book_shell_sidebar_register_type
					(GTypeModule *type_module);
GtkWidget *	e_book_shell_sidebar_new
					(EShellView *shell_view);
ESourceSelector *
		e_book_shell_sidebar_get_selector
					(EBookShellSidebar *book_shell_sidebar);

G_END_DECLS

#endif /* E_BOOK_SHELL_SIDEBAR_H */
