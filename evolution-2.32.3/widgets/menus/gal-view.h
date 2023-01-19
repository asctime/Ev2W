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
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef GAL_VIEW_H
#define GAL_VIEW_H

#include <gtk/gtk.h>
#include <libxml/tree.h>

/* Standard GObject macros */
#define GAL_TYPE_VIEW \
	(gal_view_get_type ())
#define GAL_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), GAL_TYPE_VIEW, GalView))
#define GAL_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), GAL_TYPE_VIEW, GalViewClass))
#define GAL_IS_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), GAL_TYPE_VIEW))
#define GAL_IS_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), GAL_TYPE_VIEW))
#define GAL_VIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), GAL_TYPE_VIEW, GalViewClass))

G_BEGIN_DECLS

typedef struct _GalView GalView;
typedef struct _GalViewClass GalViewClass;

struct _GalView {
	GObject parent;
};

struct _GalViewClass {
	GObjectClass parent_class;

	/* Methods */
	void		(*edit)			(GalView *view,
						 GtkWindow *parent_window);
	void		(*load)			(GalView *view,
						 const gchar *filename);
	void		(*save)			(GalView *view,
						 const gchar *filename);
	const gchar *	(*get_title)		(GalView *view);
	void		(*set_title)		(GalView *view,
						 const gchar *title);
	const gchar *	(*get_type_code)	(GalView *view);
	GalView *	(*clone)		(GalView *view);

	/* Signals */
	void		(*changed)		(GalView *view);
};

GType		gal_view_get_type		(void);
void		gal_view_edit			(GalView *view,
						 GtkWindow *parent);
void		gal_view_load			(GalView *view,
						 const gchar *filename);
void		gal_view_save			(GalView *view,
						 const gchar *filename);
const gchar *	gal_view_get_title		(GalView *view);
void		gal_view_set_title		(GalView *view,
						 const gchar *title);
const gchar *	gal_view_get_type_code		(GalView *view);
GalView *	gal_view_clone			(GalView *view);
void		gal_view_changed		(GalView *view);

G_END_DECLS

#endif /* GAL_VIEW_H */
