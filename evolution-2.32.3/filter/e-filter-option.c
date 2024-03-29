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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gmodule.h>

#include <libedataserver/e-sexp.h>

#include "e-filter-option.h"
#include "e-filter-part.h"

G_DEFINE_TYPE (
	EFilterOption,
	e_filter_option,
	E_TYPE_FILTER_ELEMENT)

static void
free_option (struct _filter_option *opt)
{
	g_free (opt->title);
	g_free (opt->value);
	g_free (opt->code);
	g_free (opt);
}

static struct _filter_option *
find_option (EFilterOption *option,
             const gchar *name)
{
	GList *link;

	for (link = option->options; link != NULL; link = g_list_next (link)) {
		struct _filter_option *opt = link->data;

		if (strcmp (name, opt->value) == 0)
			return opt;
	}

	return NULL;
}

static void
filter_option_combobox_changed (GtkComboBox *combo_box,
                                EFilterElement *element)
{
	EFilterOption *option = E_FILTER_OPTION (element);
	gint active;

	active = gtk_combo_box_get_active (combo_box);
	option->current = g_list_nth_data (option->options, active);
}

static GSList *
filter_option_get_dynamic_options (EFilterOption *option)
{
	GModule *module;
	GSList *(*get_func)(void);
	GSList *res = NULL;

	if (!option || !option->dynamic_func)
		return res;

	module = g_module_open (NULL, G_MODULE_BIND_LAZY);

	if (g_module_symbol (module, option->dynamic_func, (gpointer) &get_func)) {
		res = get_func ();
	} else {
		g_warning ("optionlist dynamic fill function '%s' not found", option->dynamic_func);
	}

	g_module_close (module);

	return res;
}

static void
filter_option_finalize (GObject *object)
{
	EFilterOption *option = E_FILTER_OPTION (object);

	g_list_free_full (option->options, (GDestroyNotify)free_option);

	g_free (option->dynamic_func);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_filter_option_parent_class)->finalize (object);
}

static gint
filter_option_eq (EFilterElement *element_a,
                  EFilterElement *element_b)
{
	EFilterOption *option_a = E_FILTER_OPTION (element_a);
	EFilterOption *option_b = E_FILTER_OPTION (element_b);

	/* Chain up to parent's eq() method. */
	if (!E_FILTER_ELEMENT_CLASS (e_filter_option_parent_class)->
		eq (element_a, element_b))
		return FALSE;

	if (option_a->current == NULL && option_b->current == NULL)
		return TRUE;

	if (option_a->current == NULL || option_b->current == NULL)
		return FALSE;

	return (g_strcmp0 (option_a->current->value, option_b->current->value) == 0);
}

static void
filter_option_xml_create (EFilterElement *element,
                          xmlNodePtr node)
{
	EFilterOption *option = E_FILTER_OPTION (element);
	xmlNodePtr n, work;

	/* Chain up to parent's xml_create() method. */
	E_FILTER_ELEMENT_CLASS (e_filter_option_parent_class)->
		xml_create (element, node);

	n = node->children;
	while (n) {
		if (!strcmp ((gchar *)n->name, "option")) {
			gchar *tmp, *value, *title = NULL, *code = NULL;

			value = (gchar *)xmlGetProp (n, (xmlChar *)"value");
			work = n->children;
			while (work) {
				if (!strcmp ((gchar *)work->name, "title") ||
					!strcmp ((gchar *)work->name, "_title")) {
					if (!title) {
						if (!(tmp = (gchar *)xmlNodeGetContent (work)))
							tmp = (gchar *)xmlStrdup ((xmlChar *)"");

						title = g_strdup (tmp);
						xmlFree (tmp);
					}
				} else if (!strcmp ((gchar *)work->name, "code")) {
					if (!code) {
						if (!(tmp = (gchar *)xmlNodeGetContent (work)))
							tmp = (gchar *)xmlStrdup ((xmlChar *)"");

						code = g_strdup (tmp);
						xmlFree (tmp);
					}
				}
				work = work->next;
			}

			e_filter_option_add (option, value, title, code, FALSE);
			xmlFree (value);
			g_free (title);
			g_free (code);
		} else if (g_str_equal ((gchar *)n->name, "dynamic")) {
			if (option->dynamic_func) {
				g_warning (
					"Only one 'dynamic' node is "
					"acceptable in the optionlist '%s'",
					element->name);
			} else {
				/* Expecting only one <dynamic func="cb" /> in the option list,
				   The 'cb' should be of this prototype:
				   GSList *cb (void);
				   returning GSList of struct _filter_option, all newly allocated, because it'll
				   be freed with g_free and g_slist_free. 'is_dynamic' member is ignored here.
				*/
				xmlChar *fn;

				fn = xmlGetProp (n, (xmlChar *)"func");
				if (fn && *fn) {
					GSList *items, *i;
					struct _filter_option *op;

					option->dynamic_func = g_strdup ((const gchar *)fn);

					/* get options now, to have them available when reading saved rules */
					items = filter_option_get_dynamic_options (option);
					for (i = items; i; i = i->next) {
						op = i->data;

						if (op) {
							e_filter_option_add (option, op->value, op->title, op->code, TRUE);
							free_option (op);
						}
					}

					g_slist_free (items);
				} else {
					g_warning (
						"Missing 'func' attribute within "
						"'%s' node in optionlist '%s'",
						n->name, element->name);
				}

				xmlFree (fn);
			}
		} else if (n->type == XML_ELEMENT_NODE) {
			g_warning ("Unknown xml node within optionlist: %s\n", n->name);
		}
		n = n->next;
	}
}

static xmlNodePtr
filter_option_xml_encode (EFilterElement *element)
{
	EFilterOption *option = E_FILTER_OPTION (element);
	xmlNodePtr value;

	value = xmlNewNode (NULL, (xmlChar *) "value");
	xmlSetProp (value, (xmlChar *) "name", (xmlChar *) element->name);
	xmlSetProp (value, (xmlChar *) "type", (xmlChar *) option->type);
	if (option->current)
		xmlSetProp (value, (xmlChar *) "value", (xmlChar *)option->current->value);

	return value;
}

static gint
filter_option_xml_decode (EFilterElement *element,
                          xmlNodePtr node)
{
	EFilterOption *option = E_FILTER_OPTION (element);
	gchar *value;

	xmlFree (element->name);
	element->name = (gchar *)xmlGetProp (node, (xmlChar *)"name");

	value = (gchar *)xmlGetProp (node, (xmlChar *)"value");
	if (value) {
		option->current = find_option (option, value);
		xmlFree (value);
	} else {
		option->current = NULL;
	}

	return 0;
}

static EFilterElement *
filter_option_clone (EFilterElement *element)
{
	EFilterOption *option = E_FILTER_OPTION (element);
	EFilterOption *clone_option;
	EFilterElement *clone;
	GList *link;

	/* Chain up to parent's clone() method. */
	clone = E_FILTER_ELEMENT_CLASS (e_filter_option_parent_class)->
		clone (element);

	clone_option = E_FILTER_OPTION (clone);

	for (link = option->options; link != NULL; link = g_list_next (link)) {
		struct _filter_option *op = link->data;
		struct _filter_option *newop;

		newop = e_filter_option_add (
			clone_option, op->value,
			op->title, op->code, op->is_dynamic);
		if (option->current == op)
			clone_option->current = newop;
	}

	clone_option->dynamic_func = g_strdup (option->dynamic_func);

	return clone;
}

static GtkWidget *
filter_option_get_widget (EFilterElement *element)
{
	EFilterOption *option = E_FILTER_OPTION (element);
	GtkWidget *combobox;
	GList *l;
	struct _filter_option *op;
	gint index = 0, current = 0;

	if (option->dynamic_func) {
		/* it is dynamically filled, thus remove all dynamics and put there the fresh ones */
		GSList *items, *i;
		GList *old_ops;
		struct _filter_option *old_cur;

		old_ops = option->options;
		old_cur = option->current;

		/* start with an empty list */
		option->current = NULL;
		option->options = NULL;

		for (l = option->options; l; l = l->next) {
			op = l->data;

			if (op->is_dynamic) {
				break;
			} else {
				e_filter_option_add (option, op->value, op->title, op->code, FALSE);
			}
		}

		items = filter_option_get_dynamic_options (option);
		for (i = items; i; i = i->next) {
			op = i->data;

			if (op) {
				e_filter_option_add (option, op->value, op->title, op->code, TRUE);
				free_option (op);
			}
		}

		g_slist_free (items);

		/* maybe some static left after those dynamic, add them too */
		for (; l; l = l->next) {
			op = l->data;

			if (!op->is_dynamic)
				e_filter_option_add (option, op->value, op->title, op->code, FALSE);
		}

		if (old_cur)
			e_filter_option_set_current (option, old_cur->value);

		/* free old list */
		g_list_free_full (old_ops, (GDestroyNotify)free_option);
	}

	combobox = gtk_combo_box_new_text ();
	l = option->options;
	while (l) {
		op = l->data;
		gtk_combo_box_append_text (GTK_COMBO_BOX (combobox), _(op->title));

		if (op == option->current)
			current = index;

		l = g_list_next (l);
		index++;
	}

	g_signal_connect (
		combobox, "changed",
		G_CALLBACK (filter_option_combobox_changed), element);

	gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), current);

	return combobox;
}

static void
filter_option_build_code (EFilterElement *element,
                          GString *out,
                          EFilterPart *part)
{
	EFilterOption *option = E_FILTER_OPTION (element);

	if (option->current && option->current->code)
		e_filter_part_expand_code (part, option->current->code, out);
}

static void
filter_option_format_sexp (EFilterElement *element,
                           GString *out)
{
	EFilterOption *option = E_FILTER_OPTION (element);

	if (option->current)
		e_sexp_encode_string (out, option->current->value);
}

static void
e_filter_option_class_init (EFilterOptionClass *class)
{
	GObjectClass *object_class;
	EFilterElementClass *filter_element_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = filter_option_finalize;

	filter_element_class = E_FILTER_ELEMENT_CLASS (class);
	filter_element_class->eq = filter_option_eq;
	filter_element_class->xml_create = filter_option_xml_create;
	filter_element_class->xml_encode = filter_option_xml_encode;
	filter_element_class->xml_decode = filter_option_xml_decode;
	filter_element_class->clone = filter_option_clone;
	filter_element_class->get_widget = filter_option_get_widget;
	filter_element_class->build_code = filter_option_build_code;
	filter_element_class->format_sexp = filter_option_format_sexp;
}

static void
e_filter_option_init (EFilterOption *option)
{
	option->type = "option";
	option->dynamic_func = NULL;
}

/**
 * filter_option_new:
 *
 * Create a new EFilterOption object.
 *
 * Return value: A new #EFilterOption object.
 **/
EFilterOption *
e_filter_option_new (void)
{
	return g_object_new (E_TYPE_FILTER_OPTION, NULL);
}

void
e_filter_option_set_current (EFilterOption *option,
                             const gchar *name)
{
	g_return_if_fail (E_IS_FILTER_OPTION (option));

	option->current = find_option (option, name);
}

/* used by implementers to add additional options */
struct _filter_option *
e_filter_option_add (EFilterOption *option,
                     const gchar *value,
                     const gchar *title,
                     const gchar *code,
                     gboolean is_dynamic)
{
	struct _filter_option *op;

	g_return_val_if_fail (E_IS_FILTER_OPTION (option), NULL);
	g_return_val_if_fail (find_option (option, value) == NULL, NULL);

	op = g_malloc (sizeof (*op));
	op->title = g_strdup (title);
	op->value = g_strdup (value);
	op->code = g_strdup (code);
	op->is_dynamic = is_dynamic;

	option->options = g_list_append (option->options, op);

	if (option->current == NULL)
		option->current = op;

	return op;
}

const gchar *
e_filter_option_get_current (EFilterOption *option)
{
	g_return_val_if_fail (E_IS_FILTER_OPTION (option), NULL);

	if (option->current == NULL)
		return NULL;

	return option->current->value;
}

void
e_filter_option_remove_all (EFilterOption *option)
{
	g_return_if_fail (E_IS_FILTER_OPTION (option));

	g_list_free_full (option->options, (GDestroyNotify)free_option);

	option->options = NULL;
	option->current = NULL;
}
