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
 *
 * Authors:
 *		Not Zed <notzed@lostzed.mmc.com.au>
 *      Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_FILTER_ELEMENT_H
#define E_FILTER_ELEMENT_H

#include <gtk/gtk.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <e-util/e-alert.h>

#define E_TYPE_FILTER_ELEMENT \
	(e_filter_element_get_type ())
#define E_FILTER_ELEMENT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_FILTER_ELEMENT, EFilterElement))
#define E_FILTER_ELEMENT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_FILTER_ELEMENT, EFilterElementClass))
#define E_IS_FILTER_ELEMENT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_FILTER_ELEMENT))
#define E_IS_FILTER_ELEMENT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_FILTER_ELEMENT))
#define E_FILTER_ELEMENT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_FILTER_ELEMENT, EFilterElementClass))

G_BEGIN_DECLS

struct _EFilterPart;

typedef struct _EFilterElement EFilterElement;
typedef struct _EFilterElementClass EFilterElementClass;
typedef struct _EFilterElementPrivate EFilterElementPrivate;

typedef EFilterElement * (*EFilterElementFunc) (gpointer data);

struct _EFilterElement {
	GObject parent;
	EFilterElementPrivate *priv;

	gchar *name;
	gpointer data;
};

struct _EFilterElementClass {
	GObjectClass parent_class;

	gboolean	(*validate)		(EFilterElement *element,
						 EAlert **alert);
	gint		(*eq)			(EFilterElement *element_a,
						 EFilterElement *element_b);

	void		(*xml_create)		(EFilterElement *element,
						 xmlNodePtr node);
	xmlNodePtr	(*xml_encode)		(EFilterElement *element);
	gint		(*xml_decode)		(EFilterElement *element,
						 xmlNodePtr node);

	EFilterElement *(*clone)		(EFilterElement *element);
	void		(*copy_value)		(EFilterElement *dst_element,
						 EFilterElement *src_element);

	GtkWidget *	(*get_widget)		(EFilterElement *element);
	void		(*build_code)		(EFilterElement *element,
						 GString *out,
						 struct _EFilterPart *part);
	void		(*format_sexp)		(EFilterElement *element,
						 GString *out);
};

GType		e_filter_element_get_type	(void);
EFilterElement	*e_filter_element_new		(void);
void		e_filter_element_set_data	(EFilterElement *element,
						 gpointer data);
gboolean	e_filter_element_validate	(EFilterElement *element,
						 EAlert **alert);
gint		e_filter_element_eq		(EFilterElement *element_a,
						 EFilterElement *element_b);
void		e_filter_element_xml_create	(EFilterElement *element,
						 xmlNodePtr node);
xmlNodePtr	e_filter_element_xml_encode	(EFilterElement *element);
gint		e_filter_element_xml_decode	(EFilterElement *element,
						 xmlNodePtr node);
EFilterElement *e_filter_element_clone		(EFilterElement *element);
void		e_filter_element_copy_value	(EFilterElement *dst_element,
						 EFilterElement *src_element);
GtkWidget *	e_filter_element_get_widget	(EFilterElement *element);
void		e_filter_element_build_code	(EFilterElement *element,
						 GString *out,
						 struct _EFilterPart *part);
void		e_filter_element_format_sexp	(EFilterElement *element,
						 GString *out);

G_END_DECLS

#endif /* E_FILTER_ELEMENT_H */
