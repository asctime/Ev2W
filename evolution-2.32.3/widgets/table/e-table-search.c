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
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <string.h>

#include "e-util/e-util.h"

#include "e-table-search.h"

#define d(x)

d (static gint depth = 0)

struct _ETableSearchPrivate {
	guint timeout_id;

	gchar *search_string;
	gunichar last_character;
};

G_DEFINE_TYPE (ETableSearch, e_table_search, G_TYPE_OBJECT)

enum {
	SEARCH_SEARCH,
	SEARCH_ACCEPT,
	LAST_SIGNAL
};

static guint e_table_search_signals[LAST_SIGNAL] = { 0, };

static gboolean
e_table_search_search (ETableSearch *e_table_search,
                       gchar *string,
                       ETableSearchFlags flags)
{
	gboolean ret_val;
	g_return_val_if_fail (E_IS_TABLE_SEARCH (e_table_search), FALSE);

	g_signal_emit (G_OBJECT (e_table_search),
		       e_table_search_signals[SEARCH_SEARCH],
		       0, string, flags, &ret_val);

	return ret_val;
}

static void
e_table_search_accept (ETableSearch *e_table_search)
{
	g_return_if_fail (E_IS_TABLE_SEARCH (e_table_search));

	g_signal_emit (G_OBJECT (e_table_search),
		       e_table_search_signals[SEARCH_ACCEPT], 0);
}

static gboolean
ets_accept (gpointer data)
{
	ETableSearch *ets = data;
	e_table_search_accept (ets);
	g_free (ets->priv->search_string);

	ets->priv->timeout_id = 0;
	ets->priv->search_string = g_strdup ("");
	ets->priv->last_character = 0;

	return FALSE;
}

static void
drop_timeout (ETableSearch *ets)
{
	if (ets->priv->timeout_id) {
		g_source_remove (ets->priv->timeout_id);
	}
	ets->priv->timeout_id = 0;
}

static void
add_timeout (ETableSearch *ets)
{
	drop_timeout (ets);
	ets->priv->timeout_id = g_timeout_add_seconds (1, ets_accept, ets);
}

static void
e_table_search_finalize (GObject *object)
{
	ETableSearch *ets = (ETableSearch *) object;

	drop_timeout (ets);
	g_free (ets->priv->search_string);
	g_free (ets->priv);

	if (G_OBJECT_CLASS (e_table_search_parent_class)->finalize)
		(*G_OBJECT_CLASS (e_table_search_parent_class)->finalize)(object);
}

static void
e_table_search_class_init (ETableSearchClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = e_table_search_finalize;

	e_table_search_signals[SEARCH_SEARCH] =
		g_signal_new ("search",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETableSearchClass, search),
			      (GSignalAccumulator) NULL, NULL,
			      e_marshal_BOOLEAN__STRING_INT,
			      G_TYPE_BOOLEAN, 2, G_TYPE_STRING, G_TYPE_INT);

	e_table_search_signals[SEARCH_ACCEPT] =
		g_signal_new ("accept",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETableSearchClass, accept),
			      (GSignalAccumulator) NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	klass->search = NULL;
	klass->accept = NULL;
}

static void
e_table_search_init (ETableSearch *ets)
{
	ets->priv = g_new (ETableSearchPrivate, 1);

	ets->priv->timeout_id = 0;
	ets->priv->search_string = g_strdup ("");
	ets->priv->last_character = 0;
}

ETableSearch *
e_table_search_new (void)
{
	ETableSearch *ets = g_object_new (E_TABLE_SEARCH_TYPE, NULL);

	return ets;
}

/**
 * e_table_search_column_count:
 * @e_table_search: The e-table-search to operate on
 *
 * Returns: the number of columns in the table search.
 */
void
e_table_search_input_character (ETableSearch *ets, gunichar character)
{
	gchar character_utf8[7];
	gchar *temp_string;

	g_return_if_fail (ets != NULL);
	g_return_if_fail (E_IS_TABLE_SEARCH (ets));

	character_utf8[g_unichar_to_utf8 (character, character_utf8)] = 0;

	temp_string = g_strdup_printf ("%s%s", ets->priv->search_string, character_utf8);
	if (e_table_search_search (ets, temp_string,
				   ets->priv->last_character != 0 ? E_TABLE_SEARCH_FLAGS_CHECK_CURSOR_FIRST : 0)) {
		g_free (ets->priv->search_string);
		ets->priv->search_string = temp_string;
		add_timeout (ets);
		ets->priv->last_character = character;
		return;
	} else {
		g_free (temp_string);
	}

	if (character == ets->priv->last_character) {
		if (ets->priv->search_string &&
			e_table_search_search (ets, ets->priv->search_string, 0)) {
			add_timeout (ets);
		}
	}
}

gboolean
e_table_search_backspace (ETableSearch *ets)
{
	gchar *end;

	g_return_val_if_fail (ets != NULL, FALSE);
	g_return_val_if_fail (E_IS_TABLE_SEARCH (ets), FALSE);

	if (!ets->priv->search_string ||
	    !*ets->priv->search_string)
		return FALSE;

	end = ets->priv->search_string + strlen (ets->priv->search_string);
	end = g_utf8_prev_char (end);
	*end = 0;
	ets->priv->last_character = 0;
	add_timeout (ets);
	return TRUE;
}
