/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef E_CATEGORIES_DIALOG_H
#define E_CATEGORIES_DIALOG_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_CATEGORIES_DIALOG \
	(e_categories_dialog_get_type ())
#define E_CATEGORIES_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CATEGORIES_DIALOG, ECategoriesDialog))
#define E_CATEGORIES_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CATEGORIES_DIALOG, ECategoriesDialogClass))
#define E_IS_CATEGORIES_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CATEGORIES_DIALOG))
#define E_IS_CATEGORIES_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CATEGORIES_DIALOG))
#define E_CATEGORIES_DIALOG_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CATEGORIES_DIALOG, ECategoriesDialogClass))

G_BEGIN_DECLS

typedef struct _ECategoriesDialog ECategoriesDialog;
typedef struct _ECategoriesDialogClass ECategoriesDialogClass;
typedef struct _ECategoriesDialogPrivate ECategoriesDialogPrivate;

struct _ECategoriesDialog {
	GtkDialog parent;
	ECategoriesDialogPrivate *priv;
};

struct _ECategoriesDialogClass {
	GtkDialogClass parent_class;
};

GType		e_categories_dialog_get_type	(void);
GtkWidget *	e_categories_dialog_new		(const gchar *categories);
const gchar *	e_categories_dialog_get_categories
						(ECategoriesDialog *dialog);
void		e_categories_dialog_set_categories
						(ECategoriesDialog *dialog,
						 const gchar *categories);

G_END_DECLS

#endif /* E_CATEGORIES_DIALOG_H */
