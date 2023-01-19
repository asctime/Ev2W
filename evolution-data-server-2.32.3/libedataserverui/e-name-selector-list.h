/*
 * Single-line text entry widget for EDestinations.
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
 *		Srinivasa Ragavan <sragavan@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_NAME_SELECTOR_LIST_H
#define E_NAME_SELECTOR_LIST_H

#include <gtk/gtk.h>
#include <libebook/e-contact.h>
#include <libedataserverui/e-contact-store.h>
#include <libedataserverui/e-destination-store.h>
#include <libedataserverui/e-tree-model-generator.h>
#include <libedataserverui/e-name-selector-entry.h>

/* Standard GObject macros */
#define E_TYPE_NAME_SELECTOR_LIST \
	(e_name_selector_list_get_type ())
#define E_NAME_SELECTOR_LIST(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_NAME_SELECTOR_LIST, ENameSelectorList))
#define E_NAME_SELECTOR_LIST_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_NAME_SELECTOR_LIST, ENameSelectorListClass))
#define E_IS_NAME_SELECTOR_LIST(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_NAME_SELECTOR_LIST))
#define E_IS_NAME_SELECTOR_LIST_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_NAME_SELECTOR_LIST))
#define E_NAME_SELECTOR_LIST_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_NAME_SELECTOR_LIST, ENameSelectorListClass))

G_BEGIN_DECLS

typedef struct _ENameSelectorList ENameSelectorList;
typedef struct _ENameSelectorListClass ENameSelectorListClass;
typedef struct _ENameSelectorListPrivate ENameSelectorListPrivate;

struct _ENameSelectorList {
	ENameSelectorEntry parent;
	ENameSelectorListPrivate *priv;
};

struct _ENameSelectorListClass {
	ENameSelectorEntryClass parent_class;
};

GType		e_name_selector_list_get_type	(void);
ENameSelectorList *
		e_name_selector_list_new	(void);
void		e_name_selector_list_expand_clicked
						(ENameSelectorList *list);

G_END_DECLS

#endif /* E_NAME_SELECTOR_LIST_H */
