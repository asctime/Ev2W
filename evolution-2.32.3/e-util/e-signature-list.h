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
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __E_SIGNATURE_LIST__
#define __E_SIGNATURE_LIST__

#include <libedataserver/e-list.h>
#include <e-util/e-signature.h>

#include <gconf/gconf-client.h>

#define E_TYPE_SIGNATURE_LIST            (e_signature_list_get_type ())
#define E_SIGNATURE_LIST(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_SIGNATURE_LIST, ESignatureList))
#define E_SIGNATURE_LIST_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_SIGNATURE_LIST, ESignatureListClass))
#define E_IS_SIGNATURE_LIST(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_SIGNATURE_LIST))
#define E_IS_SIGNATURE_LIST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_SIGNATURE_LIST))

typedef struct _ESignatureList ESignatureList;
typedef struct _ESignatureListClass ESignatureListClass;

/* search options for the find command */
typedef enum {
	E_SIGNATURE_FIND_NAME,
	E_SIGNATURE_FIND_UID
} e_signature_find_t;

struct _ESignatureList {
	EList parent_object;

	struct _ESignatureListPrivate *priv;
};

struct _ESignatureListClass {
	EListClass parent_class;

	/* signals */
	void (* signature_added)   (ESignatureList *, ESignature *);
	void (* signature_changed) (ESignatureList *, ESignature *);
	void (* signature_removed) (ESignatureList *, ESignature *);
};

GType e_signature_list_get_type (void);

ESignatureList *e_signature_list_new (GConfClient *gconf);
void e_signature_list_construct (ESignatureList *signature_list, GConfClient *gconf);

void e_signature_list_save (ESignatureList *signature_list);

void e_signature_list_add (ESignatureList *signature_list, ESignature *signature);
void e_signature_list_change (ESignatureList *signature_list, ESignature *signature);
void e_signature_list_remove (ESignatureList *signature_list, ESignature *signature);

const ESignature *e_signature_list_find (ESignatureList *signature_list, e_signature_find_t type, const gchar *key);

#endif /* __E_SIGNATURE_LIST__ */
