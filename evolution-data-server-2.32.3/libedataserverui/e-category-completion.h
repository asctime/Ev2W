/*
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

#ifndef E_CATEGORY_COMPLETION_H
#define E_CATEGORY_COMPLETION_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_CATEGORY_COMPLETION \
	(e_category_completion_get_type ())
#define E_CATEGORY_COMPLETION(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CATEGORY_COMPLETION, ECategoryCompletion))
#define E_CATEGORY_COMPLETION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CATEGORY_COMPLETION, ECategoryCompletionClass))
#define E_IS_CATEGORY_COMPLETION(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CATEGORY_COMPLETION))
#define E_IS_CATEGORY_COMPLETION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CATEGORY_COMPLETION))
#define E_CATEGORY_COMPLETION_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CATEGORY_COMPLETION, ECategoryCompletionClass))

G_BEGIN_DECLS

typedef struct _ECategoryCompletion ECategoryCompletion;
typedef struct _ECategoryCompletionClass ECategoryCompletionClass;
typedef struct _ECategoryCompletionPrivate ECategoryCompletionPrivate;

/**
 * ECategoryCompletion:
 *
 * Since: 2.26
 **/
struct _ECategoryCompletion {
	GtkEntryCompletion parent;
	ECategoryCompletionPrivate *priv;
};

struct _ECategoryCompletionClass {
	GtkEntryCompletionClass parent_class;
};

GType		e_category_completion_get_type	(void);
GtkEntryCompletion *
		e_category_completion_new	(void);

G_END_DECLS

#endif /* E_CATEGORY_COMPLETION_H */
