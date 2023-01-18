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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_ADDRESSBOOK_MODEL_H
#define E_ADDRESSBOOK_MODEL_H

#include <libebook/e-book.h>
#include <libebook/e-book-query.h>
#include <libebook/e-book-view.h>

/* Standard GObject macros */
#define E_TYPE_ADDRESSBOOK_MODEL \
	(e_addressbook_model_get_type ())
#define E_ADDRESSBOOK_MODEL(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ADDRESSBOOK_MODEL, EAddressbookModel))
#define E_ADDRESSBOOK_MODEL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ADDRESSBOOK_MODEL, EAddressbookModelClass))
#define E_IS_ADDRESSBOOK_MODEL(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ADDRESSBOOK_MODEL))
#define E_IS_ADDRESSBOOK_MODEL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_ADDRESSBOOK_MODEL))
#define E_ADDRESSBOOK_MODEL_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_ADDRESSBOOK_MODEL))

G_BEGIN_DECLS

typedef struct _EAddressbookModel EAddressbookModel;
typedef struct _EAddressbookModelClass EAddressbookModelClass;
typedef struct _EAddressbookModelPrivate EAddressbookModelPrivate;

struct _EAddressbookModel {
	GObject parent;
	EAddressbookModelPrivate *priv;
};

struct _EAddressbookModelClass {
	GObjectClass parent_class;

	/* Signals */
	void		(*writable_status)	(EAddressbookModel *model,
						 gboolean writable);
	void		(*search_started)	(EAddressbookModel *model);
	void		(*search_result)	(EAddressbookModel *model,
						 EBookViewStatus status,
						 const gchar *error_msg);
	void		(*status_message)	(EAddressbookModel *model,
						 const gchar *message);
	void		(*folder_bar_message)	(EAddressbookModel *model,
						 const gchar *message);
	void		(*contact_added)	(EAddressbookModel *model,
						 gint index,
						 gint count);
	void		(*contacts_removed)	(EAddressbookModel *model,
						 gpointer id_list);
	void		(*contact_changed)	(EAddressbookModel *model,
						 gint index);
	void		(*model_changed)	(EAddressbookModel *model);
	void		(*stop_state_changed)	(EAddressbookModel *model);
	void		(*backend_died)		(EAddressbookModel *model);
};

GType		e_addressbook_model_get_type	(void);
EAddressbookModel *
		e_addressbook_model_new		(void);

/* Returns object with ref count of 1. */
EContact *	e_addressbook_model_get_contact	(EAddressbookModel *model,
						 gint row);

void		e_addressbook_model_stop	(EAddressbookModel *model);
gboolean	e_addressbook_model_can_stop	(EAddressbookModel *model);

void		e_addressbook_model_force_folder_bar_message
						(EAddressbookModel *model);

gint		e_addressbook_model_contact_count
						(EAddressbookModel *model);
EContact *	e_addressbook_model_contact_at	(EAddressbookModel *model,
						 gint index);
gint		e_addressbook_model_find	(EAddressbookModel *model,
						 EContact *contact);
EBook *		e_addressbook_model_get_book	(EAddressbookModel *model);
void		e_addressbook_model_set_book	(EAddressbookModel *model,
						 EBook *book);
gboolean	e_addressbook_model_get_editable(EAddressbookModel *model);
void		e_addressbook_model_set_editable(EAddressbookModel *model,
						 gboolean editable);
gchar *		e_addressbook_model_get_query	(EAddressbookModel *model);
void		e_addressbook_model_set_query	(EAddressbookModel *model,
						 const gchar *query);

G_END_DECLS

#endif /* E_ADDRESSBOOK_MODEL_H */
