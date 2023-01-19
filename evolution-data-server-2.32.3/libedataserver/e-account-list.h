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

#ifndef __E_ACCOUNT_LIST__
#define __E_ACCOUNT_LIST__

#include "e-list.h"
#include "e-account.h"
#include <gconf/gconf-client.h>

G_BEGIN_DECLS

#define E_TYPE_ACCOUNT_LIST            (e_account_list_get_type ())
#define E_ACCOUNT_LIST(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_ACCOUNT_LIST, EAccountList))
#define E_ACCOUNT_LIST_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_ACCOUNT_LIST, EAccountListClass))
#define E_IS_ACCOUNT_LIST(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_ACCOUNT_LIST))
#define E_IS_ACCOUNT_LIST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_ACCOUNT_LIST))

typedef struct EAccountListPrivate EAccountListPrivate;

/* search options for the find command */
typedef enum _e_account_find_t {
	E_ACCOUNT_FIND_NAME,
	E_ACCOUNT_FIND_UID,
	E_ACCOUNT_FIND_ID_NAME,
	E_ACCOUNT_FIND_ID_ADDRESS,
	E_ACCOUNT_FIND_PARENT_UID
} e_account_find_t;

typedef struct _EAccountList {
	EList parent_object;

	EAccountListPrivate *priv;
} EAccountList;

typedef struct {
	EListClass parent_class;

	/* signals */
	void (*account_added)   (EAccountList *, EAccount *);
	void (*account_changed) (EAccountList *, EAccount *);
	void (*account_removed) (EAccountList *, EAccount *);
} EAccountListClass;

GType           e_account_list_get_type (void);

EAccountList   *e_account_list_new       (GConfClient  *gconf);
void            e_account_list_construct (EAccountList *account_list,
					  GConfClient  *gconf);

void            e_account_list_save      (EAccountList *account_list);

void            e_account_list_add       (EAccountList *, EAccount *);
void            e_account_list_change    (EAccountList *, EAccount *);
void            e_account_list_remove    (EAccountList *, EAccount *);

const EAccount *e_account_list_get_default(EAccountList *);
void            e_account_list_set_default(EAccountList *, EAccount *);
const EAccount *e_account_list_find       (EAccountList *, e_account_find_t type, const gchar *key);

void e_account_list_prune_proxies (EAccountList *);
void e_account_list_remove_account_proxies (EAccountList *, EAccount *);
gint e_account_list_account_has_proxies (EAccountList *, EAccount *);

G_END_DECLS

#endif /* __E_ACCOUNT_LIST__ */
