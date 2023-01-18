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
 *		Not Zed <notzed@lostzed.mmc.com.au>
 *      Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _EM_FILTER_CONTEXT_H
#define _EM_FILTER_CONTEXT_H

#include "filter/e-rule-context.h"

#define EM_TYPE_FILTER_CONTEXT            (em_filter_context_get_type ())
#define EM_FILTER_CONTEXT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FILTER_TYPE_CONTEXT, EMFilterContext))
#define EM_FILTER_CONTEXT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FILTER_TYPE_CONTEXT, EMFilterContextClass))
#define EM_IS_FILTER_CONTEXT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FILTER_TYPE_CONTEXT))
#define EM_IS_FILTER_CONTEXT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FILTER_TYPE_CONTEXT))
#define EM_FILTER_CONTEXT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FILTER_TYPE_CONTEXT, EMFilterContextClass))

typedef struct _EMFilterContext EMFilterContext;
typedef struct _EMFilterContextClass EMFilterContextClass;

struct _EMFilterContext {
	ERuleContext parent_object;

	GList *actions;
};

struct _EMFilterContextClass {
	ERuleContextClass parent_class;
};

GType em_filter_context_get_type (void);
EMFilterContext *em_filter_context_new (void);

/* methods */
void em_filter_context_add_action (EMFilterContext *fc, EFilterPart *action);
EFilterPart *em_filter_context_find_action (EMFilterContext *fc, const gchar *name);
EFilterPart *em_filter_context_create_action (EMFilterContext *fc, const gchar *name);
EFilterPart *em_filter_context_next_action (EMFilterContext *fc, EFilterPart *last);

#endif /* _EM_FILTER_CONTEXT_H */
