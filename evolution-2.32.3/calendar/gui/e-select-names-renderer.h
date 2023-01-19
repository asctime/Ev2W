/*
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
 *		Mike Kestner  <mkestner@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __E_SELECT_NAMES_RENDERER_H__
#define __E_SELECT_NAMES_RENDERER_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define E_TYPE_SELECT_NAMES_RENDERER	   (e_select_names_renderer_get_type ())
#define E_SELECT_NAMES_RENDERER(o)	   (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TYPE_SELECT_NAMES_RENDERER, ESelectNamesRenderer))
#define E_SELECT_NAMES_RENDERER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), E_TYPE_SELECT_NAMES_RENDERER, ESelectNamesRendererClass))
#define E_IS_SELECT_NAMES_RENDERER(o)	   (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TYPE_SELECT_NAMES_RENDERER))
#define E_IS_SELECT_NAMES_RENDERER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((o), E_TYPE_SELECT_NAMES_RENDERER))
#define E_SELECT_NAMES_RENDERER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), E_TYPE_SELECT_NAMES_RENDERER, ESelectNamesRendererClass))

typedef struct _ESelectNamesRenderer      ESelectNamesRenderer;
typedef struct _ESelectNamesRendererClass ESelectNamesRendererClass;
typedef struct _ESelectNamesRendererPriv  ESelectNamesRendererPriv;

struct _ESelectNamesRenderer
{
	GtkCellRendererText  parent;

	ESelectNamesRendererPriv *priv;
};

struct _ESelectNamesRendererClass
{
	GtkCellRendererTextClass parent_class;

	void (* cell_edited) (ESelectNamesRenderer *renderer,
			      const gchar *path,
			      GList *addresses,
			      GList *names);
};

GType            e_select_names_renderer_get_type (void);
GtkCellRenderer *e_select_names_renderer_new      (void);

G_END_DECLS

#endif /* __E_SELECT_NAMES_RENDERER_H__ */
