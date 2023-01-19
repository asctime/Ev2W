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
 *		Damon Chaplin <damon@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/*
 * ECellPopup - an ECell used to support popup selections like a GtkCombo
 * widget. It contains a child ECell, e.g. an ECellText, but when selected it
 * displays an arrow on the right edge which the user can click to show a
 * popup. It will support subclassing or signals so that different types of
 * popup can be provided.
 */

#ifndef _E_CELL_POPUP_H_
#define _E_CELL_POPUP_H_

#include <libgnomecanvas/gnome-canvas.h>
#include <table/e-cell.h>

#define E_CELL_POPUP_TYPE        (e_cell_popup_get_type ())
#define E_CELL_POPUP(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_CELL_POPUP_TYPE, ECellPopup))
#define E_CELL_POPUP_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_CELL_POPUP_TYPE, ECellPopupClass))
#define E_IS_CELL_POPUP(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_CELL_POPUP_TYPE))
#define E_IS_CELL_POPUP_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_CELL_POPUP_TYPE))

typedef struct _ECellPopupView ECellPopupView;

typedef struct {
	ECell parent;

	ECell *child;

	/* This is TRUE if the popup window is shown for the cell being
	   edited. While shown we display the arrow indented. */
	gboolean	 popup_shown;

	/* This is TRUE if the popup arrow is shown for the cell being edited.
	   This is needed to stop the first click on the cell from popping up
	   the popup window. We only popup the window after we have drawn the
	   arrow. */
	gboolean	 popup_arrow_shown;

	/* The view in which the popup is shown. */
	ECellPopupView	*popup_cell_view;

	gint		 popup_view_col;
	gint		 popup_row;
	ETableModel	*popup_model;
} ECellPopup;

typedef struct {
	ECellClass parent_class;

	/* Virtual function for subclasses to override. */
	gint	   (*popup)        (ECellPopup *ecp, GdkEvent *event, gint row, gint view_col);
} ECellPopupClass;

struct _ECellPopupView {
	ECellView	 cell_view;

	ECellView	*child_view;
};

GType    e_cell_popup_get_type           (void);
ECell   *e_cell_popup_new                (void);

/* Get and set the child ECell. */
ECell   *e_cell_popup_get_child          (ECellPopup *ecp);
void     e_cell_popup_set_child          (ECellPopup *ecp,
					  ECell      *child);

void     e_cell_popup_set_shown          (ECellPopup *ecp,
					  gboolean    shown);
void     e_cell_popup_queue_cell_redraw  (ECellPopup *ecp);

#endif /* _E_CELL_POPUP_H_ */
