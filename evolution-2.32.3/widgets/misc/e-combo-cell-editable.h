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

#ifndef __E_COMBO_CELL_EDITABLE_H__
#define __E_COMBO_CELL_EDITABLE_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define E_TYPE_COMBO_CELL_EDITABLE	   (e_combo_cell_editable_get_type ())
#define E_COMBO_CELL_EDITABLE(o)	   (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TYPE_COMBO_CELL_EDITABLE, EComboCellEditable))
#define E_COMBO_CELL_EDITABLE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), E_TYPE_COMBO_CELL_EDITABLE, EComboCellEditableClass))
#define E_IS_COMBO_CELL_EDITABLE(o)	   (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TYPE_COMBO_CELL_EDITABLE))
#define E_IS_COMBO_CELL_EDITABLE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((o), E_TYPE_COMBO_CELL_EDITABLE))
#define E_COMBO_CELL_EDITABLE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), E_TYPE_COMBO_CELL_EDITABLE, EComboCellEditableClass))

typedef struct _EComboCellEditable      EComboCellEditable;
typedef struct _EComboCellEditableClass EComboCellEditableClass;
typedef struct _EComboCellEditablePriv  EComboCellEditablePriv;

struct _EComboCellEditable
{
	GtkEventBox  parent;

	EComboCellEditablePriv *priv;
};

struct _EComboCellEditableClass
{
	GtkEventBoxClass parent_class;
};

GType      e_combo_cell_editable_get_type (void);

GtkCellEditable *e_combo_cell_editable_new (void);

const GList *e_combo_cell_editable_get_list (EComboCellEditable *editable);
void         e_combo_cell_editable_set_list (EComboCellEditable *editable, GList *list);

const gchar *e_combo_cell_editable_get_text (EComboCellEditable *editable);
void         e_combo_cell_editable_set_text (EComboCellEditable *editable, const gchar *text);

gboolean e_combo_cell_editable_cancelled (EComboCellEditable *editable);

G_END_DECLS

#endif /* __E_COMBO_CELL_EDITABLE_H__ */
