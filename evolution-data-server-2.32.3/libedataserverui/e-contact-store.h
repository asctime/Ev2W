/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* e-contact-store.h - Contacts store with GtkTreeModel interface.
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors: Hans Petter Jansson <hpj@novell.com>
 */

#ifndef E_CONTACT_STORE_H
#define E_CONTACT_STORE_H

#include <gtk/gtk.h>
#include <libebook/e-contact.h>
#include <libebook/e-book.h>
#include <libebook/e-book-query.h>
#include <libebook/e-book-types.h>

/* Standard GObject macros */
#define E_TYPE_CONTACT_STORE \
	(e_contact_store_get_type ())
#define E_CONTACT_STORE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CONTACT_STORE, EContactStore))
#define E_CONTACT_STORE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CONTACT_STORE, EContactStoreClass))
#define E_IS_CONTACT_STORE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CONTACT_STORE))
#define E_IS_CONTACT_STORE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CONTACT_STORE))
#define E_CONTACT_STORE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CONTACT_STORE, EContactStoreClass))

G_BEGIN_DECLS

typedef struct _EContactStore EContactStore;
typedef struct _EContactStoreClass EContactStoreClass;
typedef struct _EContactStorePrivate EContactStorePrivate;

struct _EContactStore {
	GObject parent;
	EContactStorePrivate *priv;
};

struct _EContactStoreClass {
	GObjectClass parent_class;
};

GType		e_contact_store_get_type	(void);
EContactStore *	e_contact_store_new		(void);

EBook *		e_contact_store_get_book	(EContactStore *contact_store,
						 GtkTreeIter *iter);
EContact *	e_contact_store_get_contact	(EContactStore *contact_store,
						 GtkTreeIter *iter);
gboolean	e_contact_store_find_contact	(EContactStore *contact_store,
						 const gchar *uid,
						 GtkTreeIter *iter);

/* Returns a shallow copy; free the list when done, but don't unref elements */
GList *		e_contact_store_get_books	(EContactStore *contact_store);
void		e_contact_store_add_book	(EContactStore *contact_store,
						 EBook *book);
void		e_contact_store_remove_book	(EContactStore *contact_store,
						 EBook *book);
void		e_contact_store_set_query	(EContactStore *contact_store,
						 EBookQuery *book_query);
EBookQuery *	e_contact_store_peek_query	(EContactStore *contact_store);
EBookView *	find_contact_source_by_book_return_view
						(EContactStore *contact_store,
						 EBook *book);

G_END_DECLS

#endif  /* E_CONTACT_STORE_H */
