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
 *		Mike Kestner  <mkestner@ximian.com>
 *	    JP Rosevear  <jpr@ximian.com>
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
#include <libebook/e-book.h>
#include <libebook/e-vcard.h>
#include <libecal/e-cal-component.h>
#include <libecal/e-cal-util.h>
#include <libecal/e-cal-time-util.h>
#include <libedataserverui/e-name-selector.h>
#include "calendar-config.h"
#include "e-meeting-list-view.h"
#include "itip-utils.h"
#include <misc/e-cell-renderer-combo.h>
#include <libebook/e-destination.h>
#include "e-select-names-renderer.h"

struct _EMeetingListViewPrivate {
	EMeetingStore *store;

	ENameSelector *name_selector;

	GHashTable *renderers;
};

#define BUF_SIZE 1024

/* Signal IDs */
enum {
	ATTENDEE_ADDED,
	LAST_SIGNAL
};

static guint e_meeting_list_view_signals[LAST_SIGNAL] = { 0 };

static void name_selector_dialog_close_cb (ENameSelectorDialog *dialog, gint response, gpointer data);

static const gchar *sections[] = {N_("Chair Persons"),
				  N_("Required Participants"),
				  N_("Optional Participants"),
				  N_("Resources"),
				  NULL};

static icalparameter_role roles[] = {ICAL_ROLE_CHAIR,
				     ICAL_ROLE_REQPARTICIPANT,
				     ICAL_ROLE_OPTPARTICIPANT,
				     ICAL_ROLE_NONPARTICIPANT,
				     ICAL_ROLE_NONE};

G_DEFINE_TYPE (EMeetingListView, e_meeting_list_view, GTK_TYPE_TREE_VIEW)

static void
e_meeting_list_view_finalize (GObject *obj)
{
	EMeetingListView *view = E_MEETING_LIST_VIEW (obj);
	EMeetingListViewPrivate *priv = view->priv;

	if (priv->name_selector) {
		g_object_unref (priv->name_selector);
		priv->name_selector = NULL;
	}

	if (priv->renderers) {
		g_hash_table_destroy (priv->renderers);
		priv->renderers = NULL;
	}

	g_free (priv);

	if (G_OBJECT_CLASS (e_meeting_list_view_parent_class)->finalize)
		(* G_OBJECT_CLASS (e_meeting_list_view_parent_class)->finalize) (obj);
}

static void
e_meeting_list_view_class_init (EMeetingListViewClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = e_meeting_list_view_finalize;

	e_meeting_list_view_signals[ATTENDEE_ADDED] =
		g_signal_new ("attendee_added",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EMeetingListViewClass, attendee_added),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);
}

static void
add_section (ENameSelector *name_selector, const gchar *name)
{
	ENameSelectorModel *name_selector_model;

	name_selector_model = e_name_selector_peek_model (name_selector);
	e_name_selector_model_add_section (name_selector_model, name, gettext (name), NULL);
}

static void
e_meeting_list_view_init (EMeetingListView *view)
{
	EMeetingListViewPrivate *priv;
	ENameSelectorDialog *name_selector_dialog;
	gint i;

	priv = g_new0 (EMeetingListViewPrivate, 1);

	view->priv = priv;

	priv->renderers = g_hash_table_new (g_direct_hash, g_int_equal);

	priv->name_selector = e_name_selector_new ();

	for (i = 0; sections[i]; i++)
		add_section (priv->name_selector, sections[i]);

	name_selector_dialog = e_name_selector_peek_dialog (view->priv->name_selector);
	gtk_window_set_title (GTK_WINDOW (name_selector_dialog), _("Attendees"));
	g_signal_connect (name_selector_dialog, "response",
			  G_CALLBACK (name_selector_dialog_close_cb), view);

}

static GList *
get_type_strings (void)
{
	GList *strings = NULL;

	strings = g_list_append (strings, (gchar *) _("Individual"));
        strings = g_list_append (strings, (gchar *) _("Group"));
        strings = g_list_append (strings, (gchar *) _("Resource"));
        strings = g_list_append (strings, (gchar *) _("Room"));
        strings = g_list_append (strings, (gchar *) _("Unknown"));

	return strings;
}

static GList *
get_role_strings (void)
{
	GList *strings = NULL;

	strings = g_list_append (strings, (gchar *) _("Chair"));
	strings = g_list_append (strings, (gchar *) _("Required Participant"));
	strings = g_list_append (strings, (gchar *) _("Optional Participant"));
	strings = g_list_append (strings, (gchar *) _("Non-Participant"));
	strings = g_list_append (strings, (gchar *) _("Unknown"));

	return strings;
}

static GList *
get_rsvp_strings (void)
{
	GList *strings = NULL;

	strings = g_list_append (strings, (gchar *) _("Yes"));
	strings = g_list_append (strings, (gchar *) _("No"));

	return strings;
}

static GList *
get_status_strings (void)
{
	GList *strings = NULL;

	strings = g_list_append (strings, (gchar *) _("Needs Action"));
	strings = g_list_append (strings, (gchar *) _("Accepted"));
	strings = g_list_append (strings, (gchar *) _("Declined"));
	strings = g_list_append (strings, (gchar *) _("Tentative"));
	strings = g_list_append (strings, (gchar *) _("Delegated"));

	return strings;
}

static void
value_edited (GtkTreeView *view, gint col, const gchar *path, const gchar *text)
{
	EMeetingStore *model = E_MEETING_STORE (gtk_tree_view_get_model (view));
	GtkTreePath *treepath = gtk_tree_path_new_from_string (path);
	gint row = gtk_tree_path_get_indices (treepath)[0];

	e_meeting_store_set_value (model, row, col, text);
	gtk_tree_path_free (treepath);
}

static guint
get_index_from_role (icalparameter_role role)
{
	switch (role)	{
		case ICAL_ROLE_CHAIR:
			return 0;
		case ICAL_ROLE_REQPARTICIPANT:
			return 1;
		case ICAL_ROLE_OPTPARTICIPANT:
			return 2;
		case ICAL_ROLE_NONPARTICIPANT:
			return 3;
		default:
			return 1;
	}
}

void
e_meeting_list_view_add_attendee_to_name_selector (EMeetingListView *view, EMeetingAttendee *ma)
{
	EDestinationStore *destination_store;
	ENameSelectorModel *name_selector_model;
	EDestination *des;
	EMeetingListViewPrivate *priv;
	guint i = 1;

	priv = view->priv;

	name_selector_model = e_name_selector_peek_model (priv->name_selector);
	i = get_index_from_role (e_meeting_attendee_get_role (ma));
	e_name_selector_model_peek_section (name_selector_model, sections[i],
					    NULL, &destination_store);
	des = e_destination_new ();
	e_destination_set_email (des, itip_strip_mailto (e_meeting_attendee_get_address (ma)));
	e_destination_set_name (des, e_meeting_attendee_get_cn (ma));
	e_destination_store_append_destination (destination_store, des);
	g_object_unref (des);
}

void
e_meeting_list_view_remove_attendee_from_name_selector (EMeetingListView *view, EMeetingAttendee *ma)
{
	GList             *destinations, *l;
	EDestinationStore *destination_store;
	ENameSelectorModel *name_selector_model;
	const gchar *madd = NULL;
	EMeetingListViewPrivate *priv;
	guint i = 1;

	priv = view->priv;

	name_selector_model = e_name_selector_peek_model (priv->name_selector);
	i = get_index_from_role (e_meeting_attendee_get_role (ma));
	e_name_selector_model_peek_section (name_selector_model, sections[i],
					    NULL, &destination_store);
	destinations = e_destination_store_list_destinations (destination_store);
	madd = itip_strip_mailto (e_meeting_attendee_get_address (ma));

	for (l = destinations; l; l = g_list_next (l)) {
		const gchar *attendee = NULL;
		EDestination *des = l->data;

		if (e_destination_is_evolution_list (des)) {
			GList *l, *dl;

			dl = (GList *)e_destination_list_get_dests (des);

			for (l = dl; l; l = l->next) {
				attendee = e_destination_get_email (l->data);
				if (madd && attendee && g_str_equal (madd, attendee)) {
					g_object_unref (l->data);
					l = g_list_remove (l, l->data);
					break;
				}
			}
		} else {
			attendee = e_destination_get_email (des);
			if (madd && attendee && g_str_equal (madd, attendee)) {
				e_destination_store_remove_destination (destination_store, des);
			}
		}
	}

	g_list_free (destinations);
}

void
e_meeting_list_view_remove_all_attendees_from_name_selector (EMeetingListView *view)
{
	ENameSelectorModel *name_selector_model;
	EMeetingListViewPrivate *priv;
	guint i;

	priv = view->priv;

	name_selector_model = e_name_selector_peek_model (priv->name_selector);

	for (i = 0; sections[i] != NULL; i++) {
		EDestinationStore *destination_store = NULL;
		GList             *destinations = NULL, *l = NULL;

		e_name_selector_model_peek_section (name_selector_model, sections[i],
						    NULL, &destination_store);
		if (!destination_store) {
			g_warning ("destination store is NULL\n");
			continue;
		}

		destinations = e_destination_store_list_destinations (destination_store);
		for (l = destinations; l; l = g_list_next (l)) {
			EDestination *des = l->data;

			if (e_destination_is_evolution_list (des)) {
				GList *m, *dl;

				dl = (GList *)e_destination_list_get_dests (des);

				for (m = dl; m; m = m->next) {
					g_object_unref (m->data);
					m = g_list_remove (m, l->data);
				}
			} else {
				e_destination_store_remove_destination (destination_store, des);
			}
		}
		g_list_free (destinations);
	}
}

static void
attendee_edited_cb (GtkCellRenderer *renderer, const gchar *path, GList *addresses, GList *names, GtkTreeView *view)
{
	EMeetingStore *model = E_MEETING_STORE (gtk_tree_view_get_model (view));
	GtkTreePath *treepath = gtk_tree_path_new_from_string (path);
	gint row = gtk_tree_path_get_indices (treepath)[0];
	EMeetingAttendee *existing_attendee;

	existing_attendee = e_meeting_store_find_attendee_at_row (model, row);

	if (g_list_length (addresses) > 1) {
		EMeetingAttendee *attendee;
		GList *l, *m;
		gboolean can_remove = TRUE;

		for (l = addresses, m = names; l && m; l = l->next, m = m->next) {
			gchar *name = m->data, *email = l->data;

			if (!((name && *name) || (email && *email)))
					continue;

			attendee = e_meeting_store_find_attendee (model, email, NULL);
			if (attendee != NULL) {
				if (attendee == existing_attendee)
					can_remove = FALSE;
				continue;
			}

			attendee = e_meeting_store_add_attendee_with_defaults (model);
			e_meeting_attendee_set_address (attendee, g_strdup_printf ("MAILTO:%s", (gchar *)l->data));
			e_meeting_attendee_set_cn (attendee, g_strdup (m->data));
			if (existing_attendee) {
				/* FIXME Should we copy anything else? */
				e_meeting_attendee_set_cutype (attendee, e_meeting_attendee_get_cutype (existing_attendee));
				e_meeting_attendee_set_role (attendee, e_meeting_attendee_get_role (existing_attendee));
				e_meeting_attendee_set_rsvp (attendee, e_meeting_attendee_get_rsvp (existing_attendee));
				e_meeting_attendee_set_status (attendee, ICAL_PARTSTAT_NEEDSACTION);
				e_meeting_attendee_set_delfrom (attendee, (gchar *)e_meeting_attendee_get_delfrom (existing_attendee));
			}
			e_meeting_list_view_add_attendee_to_name_selector (E_MEETING_LIST_VIEW (view), attendee);
			g_signal_emit_by_name (G_OBJECT (view), "attendee_added", (gpointer) attendee);
		}

		if (existing_attendee && can_remove) {
			e_meeting_list_view_remove_attendee_from_name_selector (E_MEETING_LIST_VIEW (view), existing_attendee);
			e_meeting_store_remove_attendee (model, existing_attendee);
		}
	} else if (g_list_length (addresses) == 1) {
		gchar *name = names->data, *email = addresses->data;
		gint existing_row;

		if (!((name && *name) || (email && *email)) || ((e_meeting_store_find_attendee (model, email, &existing_row) != NULL) && existing_row != row)) {
			if (existing_attendee) {
				e_meeting_list_view_remove_attendee_from_name_selector (E_MEETING_LIST_VIEW (view), existing_attendee);
				e_meeting_store_remove_attendee (model, existing_attendee);
			}
		} else {
			gboolean address_changed = FALSE;
			EMeetingAttendee *attendee = e_meeting_store_add_attendee_with_defaults (model);

			if (existing_attendee) {
				const gchar *addr = e_meeting_attendee_get_address (existing_attendee);

				if (addr && g_ascii_strncasecmp (addr, "MAILTO:", 7) == 0)
					addr += 7;

				address_changed = addr && g_ascii_strcasecmp (addr, email) != 0;

				e_meeting_list_view_remove_attendee_from_name_selector (E_MEETING_LIST_VIEW (view), existing_attendee);
				e_meeting_store_remove_attendee (model, existing_attendee);
			}

			value_edited (view, E_MEETING_STORE_ADDRESS_COL, path, email);
			value_edited (view, E_MEETING_STORE_CN_COL, path, name);

			e_meeting_attendee_set_address (attendee, g_strdup_printf ("MAILTO:%s", email));
			e_meeting_attendee_set_cn (attendee, g_strdup (name));
			e_meeting_attendee_set_role (attendee, ICAL_ROLE_REQPARTICIPANT);
			e_meeting_list_view_add_attendee_to_name_selector (E_MEETING_LIST_VIEW (view), attendee);

			if (address_changed)
				e_meeting_attendee_set_status (existing_attendee, ICAL_PARTSTAT_NEEDSACTION);

			g_signal_emit_by_name (G_OBJECT (view), "attendee_added", (gpointer) attendee);
		}
	} else if (existing_attendee) {
		const gchar *address = e_meeting_attendee_get_address (existing_attendee);

		if (!(address && *address)) {
			e_meeting_list_view_remove_attendee_from_name_selector (E_MEETING_LIST_VIEW (view), existing_attendee);
			e_meeting_store_remove_attendee (model, existing_attendee);
		}
	}

	gtk_tree_path_free (treepath);
}

static void
attendee_editing_canceled_cb (GtkCellRenderer *renderer, GtkTreeView *view)
{
	EMeetingStore *model = E_MEETING_STORE (gtk_tree_view_get_model (view));
	GtkTreePath *path;
	EMeetingAttendee *existing_attendee;
	gint row;

	/* This is for newly added attendees when the editing is cancelled */
	gtk_tree_view_get_cursor (view, &path, NULL);
	if (!path)
		return;

	row = gtk_tree_path_get_indices (path)[0];
	existing_attendee = e_meeting_store_find_attendee_at_row (model, row);
	if (existing_attendee) {
		if (!e_meeting_attendee_is_set_cn (existing_attendee) && !e_meeting_attendee_is_set_address (existing_attendee))
			e_meeting_store_remove_attendee (model, existing_attendee);
	}

	gtk_tree_path_free (path);
}

static void
type_edited_cb (GtkCellRenderer *renderer, const gchar *path, const gchar *text, GtkTreeView *view)
{
	value_edited (view, E_MEETING_STORE_TYPE_COL, path, text);
}

static void
role_edited_cb (GtkCellRenderer *renderer, const gchar *path, const gchar *text, GtkTreeView *view)
{
	/* This is a little more complex than the other callbacks because
	 * we also need to update the "Required Participants" dialog. */

	EMeetingStore *model = E_MEETING_STORE (gtk_tree_view_get_model (view));
	GtkTreePath *treepath = gtk_tree_path_new_from_string (path);
	gint row = gtk_tree_path_get_indices (treepath)[0];
	EMeetingAttendee *attendee;

	attendee = e_meeting_store_find_attendee_at_row (model, row);
	e_meeting_list_view_remove_attendee_from_name_selector (E_MEETING_LIST_VIEW (view), attendee);
	e_meeting_store_set_value (model, row, E_MEETING_STORE_ROLE_COL, text);
	e_meeting_list_view_add_attendee_to_name_selector (E_MEETING_LIST_VIEW (view), attendee);

	gtk_tree_path_free (treepath);
}

static void
rsvp_edited_cb (GtkCellRenderer *renderer, const gchar *path, const gchar *text, GtkTreeView *view)
{
	value_edited (view, E_MEETING_STORE_RSVP_COL, path, text);
}

static void
status_edited_cb (GtkCellRenderer *renderer, const gchar *path, const gchar *text, GtkTreeView *view)
{
	value_edited (view, E_MEETING_STORE_STATUS_COL, path, text);
}

static void
ense_update (GtkWidget *w, gpointer data1, gpointer user_data)
{
	gtk_cell_editable_editing_done ((GtkCellEditable *)w);
}

static void
editing_started_cb (GtkCellRenderer *renderer,
		    GtkCellEditable *editable,
		    gchar           *path,
		    gpointer         user_data)
{
		g_signal_connect (editable, "updated", G_CALLBACK(ense_update), NULL);
}

static void
build_table (EMeetingListView *lview)
{
	GtkCellRenderer *renderer;
	GtkTreeView *view = GTK_TREE_VIEW (lview);
	EMeetingListViewPrivate *priv;
	GHashTable *edit_table;
	GtkTreeViewColumn *col;
	gint pos;

	priv = lview->priv;
	edit_table = priv->renderers;
	gtk_tree_view_set_headers_visible (view, TRUE);
	gtk_tree_view_set_rules_hint (view, TRUE);

	renderer = e_select_names_renderer_new ();
	g_object_set (G_OBJECT (renderer), "editable", TRUE, NULL);
	/* The extra space is just a hack to occupy more space for Attendee */
	pos = gtk_tree_view_insert_column_with_attributes (view, -1, _("Attendee                          "), renderer,
						     "text", E_MEETING_STORE_ATTENDEE_COL,
						     "name", E_MEETING_STORE_CN_COL,
						     "email", E_MEETING_STORE_ADDRESS_COL,
						     "underline", E_MEETING_STORE_ATTENDEE_UNDERLINE_COL,
						     NULL);
	col = gtk_tree_view_get_column (view, pos -1);
	gtk_tree_view_column_set_resizable (col, TRUE);
	gtk_tree_view_column_set_reorderable(col, TRUE);
	gtk_tree_view_column_set_expand (col, TRUE);
	g_object_set (col, "min-width", 50, NULL);
	g_object_set_data (G_OBJECT (col), "mtg-store-col", GINT_TO_POINTER (E_MEETING_STORE_ATTENDEE_COL));
	g_signal_connect (renderer, "cell_edited", G_CALLBACK (attendee_edited_cb), view);
	g_signal_connect (renderer, "editing-canceled", G_CALLBACK (attendee_editing_canceled_cb), view);
	g_signal_connect (renderer, "editing-started", G_CALLBACK (editing_started_cb), view);

	g_hash_table_insert (edit_table, GINT_TO_POINTER (E_MEETING_STORE_ATTENDEE_COL), renderer);

	renderer = e_cell_renderer_combo_new ();
	g_object_set (G_OBJECT (renderer), "list", get_type_strings (), "editable", TRUE, NULL);
	pos = gtk_tree_view_insert_column_with_attributes (view, -1, _("Type"), renderer,
						     "text", E_MEETING_STORE_TYPE_COL,
						     NULL);
	col = gtk_tree_view_get_column (view, pos -1);
	gtk_tree_view_column_set_resizable (col, TRUE);
	gtk_tree_view_column_set_reorderable(col, TRUE);
	g_object_set_data (G_OBJECT (col), "mtg-store-col", GINT_TO_POINTER (E_MEETING_STORE_TYPE_COL));
	g_signal_connect (renderer, "edited", G_CALLBACK (type_edited_cb), view);
	g_hash_table_insert (edit_table, GINT_TO_POINTER (E_MEETING_STORE_TYPE_COL), renderer);

	renderer = e_cell_renderer_combo_new ();
	g_object_set (G_OBJECT (renderer), "list", get_role_strings (), "editable", TRUE, NULL);
	pos = gtk_tree_view_insert_column_with_attributes (view, -1, _("Role"), renderer,
						     "text", E_MEETING_STORE_ROLE_COL,
						     NULL);
	col = gtk_tree_view_get_column (view, pos -1);
	gtk_tree_view_column_set_resizable (col, TRUE);
	gtk_tree_view_column_set_reorderable(col, TRUE);
	g_object_set_data (G_OBJECT (col), "mtg-store-col", GINT_TO_POINTER (E_MEETING_STORE_ROLE_COL));
	g_signal_connect (renderer, "edited", G_CALLBACK (role_edited_cb), view);
	g_hash_table_insert (edit_table, GINT_TO_POINTER (E_MEETING_STORE_ROLE_COL), renderer);

	renderer = e_cell_renderer_combo_new ();
	g_object_set (G_OBJECT (renderer), "list", get_rsvp_strings (), "editable", TRUE, NULL);
	/* To translators: RSVP means "please reply" */
	pos = gtk_tree_view_insert_column_with_attributes (view, -1, _("RSVP"), renderer,
						     "text", E_MEETING_STORE_RSVP_COL,
						     NULL);
	col = gtk_tree_view_get_column (view, pos -1);
	gtk_tree_view_column_set_resizable (col, TRUE);
	gtk_tree_view_column_set_reorderable(col, TRUE);
	g_object_set_data (G_OBJECT (col), "mtg-store-col", GINT_TO_POINTER (E_MEETING_STORE_RSVP_COL));
	g_signal_connect (renderer, "edited", G_CALLBACK (rsvp_edited_cb), view);
	g_hash_table_insert (edit_table, GINT_TO_POINTER (E_MEETING_STORE_RSVP_COL), renderer);

	renderer = e_cell_renderer_combo_new ();
	g_object_set (G_OBJECT (renderer), "list", get_status_strings (), "editable", TRUE, NULL);
	pos = gtk_tree_view_insert_column_with_attributes (view, -1, _("Status"), renderer,
						     "text", E_MEETING_STORE_STATUS_COL,
						     NULL);
	col = gtk_tree_view_get_column (view, pos -1);
	gtk_tree_view_column_set_resizable (col, TRUE);
	gtk_tree_view_column_set_reorderable(col, TRUE);
	g_object_set_data (G_OBJECT (col), "mtg-store-col", GINT_TO_POINTER (E_MEETING_STORE_STATUS_COL));
	g_signal_connect (renderer, "edited", G_CALLBACK (status_edited_cb), view);
	g_hash_table_insert (edit_table, GINT_TO_POINTER (E_MEETING_STORE_STATUS_COL), renderer);

	priv->renderers = edit_table;
}

static void
change_edit_cols_for_user (gpointer key, gpointer value, gpointer user_data)
{
       GtkCellRenderer *renderer = (GtkCellRenderer *) value;
       gint key_val = GPOINTER_TO_INT (key);
       switch (key_val)
       {
               case E_MEETING_STORE_ATTENDEE_COL:
                       g_object_set (G_OBJECT (renderer), "editable", FALSE, NULL);
               break;
               case E_MEETING_STORE_ROLE_COL:
                       g_object_set (G_OBJECT (renderer), "editable", FALSE, NULL);
               break;
               case E_MEETING_STORE_TYPE_COL:
                       g_object_set (G_OBJECT (renderer), "editable", FALSE, NULL);
               break;
               case E_MEETING_STORE_RSVP_COL:
                       g_object_set (G_OBJECT (renderer), "editable", TRUE, NULL);
               break;
               case E_MEETING_STORE_STATUS_COL:
                       g_object_set (G_OBJECT (renderer), "editable", TRUE, NULL);
               break;
       }
}

static void
change_edit_cols_for_organizer (gpointer key, gpointer value, gpointer user_data)
{
       GtkCellRenderer *renderer = (GtkCellRenderer *) value;
       guint edit_level = GPOINTER_TO_INT (user_data);
       g_object_set (G_OBJECT (renderer), "editable", GINT_TO_POINTER (edit_level), NULL);
}

static void
row_activated_cb (GtkTreeSelection *selection, EMeetingListView *view)
{
       EMeetingAttendee *existing_attendee;
       EMeetingListViewPrivate *priv;
       gint row;
       EMeetingAttendeeEditLevel el;
       gint  edit_level;
       GtkTreeModel *model;
       GtkTreePath *path = NULL;
       GList *paths=NULL;

       priv = view->priv;

       if (!(paths = gtk_tree_selection_get_selected_rows (selection, &model)))
               return;
       if (g_list_length (paths) > 1)
               return;
       path = g_list_nth_data (paths, 0);
       if (!path)
	       return;

       row = gtk_tree_path_get_indices (path)[0];
       existing_attendee = e_meeting_store_find_attendee_at_row (priv->store, row);
       el = e_meeting_attendee_get_edit_level (existing_attendee);

       switch (el)
       {
               case  E_MEETING_ATTENDEE_EDIT_NONE:
               edit_level = FALSE;
               g_hash_table_foreach (priv->renderers, change_edit_cols_for_organizer, GINT_TO_POINTER (edit_level));
               break;

               case E_MEETING_ATTENDEE_EDIT_FULL:
               edit_level = TRUE;
               g_hash_table_foreach (priv->renderers, change_edit_cols_for_organizer, GINT_TO_POINTER (edit_level));
               break;

               case E_MEETING_ATTENDEE_EDIT_STATUS:
               edit_level = FALSE;
               g_hash_table_foreach (priv->renderers, change_edit_cols_for_user, GINT_TO_POINTER (edit_level));
               break;
       }

}

EMeetingListView *
e_meeting_list_view_new (EMeetingStore *store)
{
	EMeetingListView *view = g_object_new (E_TYPE_MEETING_LIST_VIEW, NULL);
	GtkTreeSelection *selection;

	if (view) {
		view->priv->store = store;
		gtk_tree_view_set_model (GTK_TREE_VIEW (view), GTK_TREE_MODEL (store));
		build_table (view);
	}

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(view));
	g_signal_connect (selection, "changed", G_CALLBACK (row_activated_cb), view);
	return view;
}

void
e_meeting_list_view_column_set_visible (EMeetingListView *view, EMeetingStoreColumns column, gboolean visible)
{
	GList *cols, *l;

	cols = gtk_tree_view_get_columns (GTK_TREE_VIEW (view));

	for (l = cols; l; l = l->next) {
		GtkTreeViewColumn *col = (GtkTreeViewColumn *) l->data;
		EMeetingStoreColumns store_colum = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (col), "mtg-store-col"));

		if (store_colum == column) {
			gtk_tree_view_column_set_visible (col, visible);
			break;
		}
	}
}

void
e_meeting_list_view_edit (EMeetingListView *emlv, EMeetingAttendee *attendee)
{
	EMeetingListViewPrivate *priv;
	GtkTreePath *path;
	GtkTreeViewColumn *focus_col;

	priv = emlv->priv;

	g_return_if_fail (emlv != NULL);
	g_return_if_fail (E_IS_MEETING_LIST_VIEW (emlv));
	g_return_if_fail (attendee != NULL);

	path = e_meeting_store_find_attendee_path (priv->store, attendee);
	focus_col = gtk_tree_view_get_column (GTK_TREE_VIEW (emlv), 0);

	if (path) {
		gtk_tree_view_set_cursor (GTK_TREE_VIEW (emlv), path, focus_col, TRUE);

		gtk_tree_path_free (path);
	}
}

static void
process_section (EMeetingListView *view, GList *destinations, icalparameter_role role, GSList **la)
{
	EMeetingListViewPrivate *priv;
	GList *l;

	priv = view->priv;
	for (l = destinations; l; l = g_list_next (l)) {
		EDestination *destination = l->data, *des = NULL;
		const GList *list_dests = NULL, *l;
		GList card_dest;

		if (e_destination_is_evolution_list (destination)) {
			list_dests = e_destination_list_get_dests (destination);
		} else {
			EContact *contact = e_destination_get_contact (destination);
			/* check if the contact is contact list which is not expanded yet */
			/* we expand it by getting the list again from the server forming the query */
			if (contact && e_contact_get (contact , E_CONTACT_IS_LIST)) {
				EBook *book = NULL;
				ENameSelectorDialog *dialog;
				ENameSelectorModel *model;
				EContactStore *c_store;
				GList *books, *l;
				gchar *uri = e_contact_get (contact, E_CONTACT_BOOK_URI);

				dialog = e_name_selector_peek_dialog (view->priv->name_selector);
				model = e_name_selector_dialog_peek_model (dialog);
				c_store = e_name_selector_model_peek_contact_store (model);
				books = e_contact_store_get_books (c_store);

				for (l = books; l; l = l->next) {
					EBook *b = l->data;
					if (g_str_equal (uri, e_book_get_uri (b))) {
						book = b;
						break;
					}
				}

				if (book) {
					GList *contacts;
					EContact *n_con = NULL;
					gchar *qu;
					EBookQuery *query;

					qu = g_strdup_printf ("(is \"full_name\" \"%s\")",
							(gchar *) e_contact_get (contact, E_CONTACT_FULL_NAME));
					query = e_book_query_from_string (qu);

					if (!e_book_get_contacts (book, query, &contacts, NULL)) {
						g_warning ("Could not get contact from the book \n");
						return;
					} else {
						des = e_destination_new ();
						n_con = contacts->data;

						e_destination_set_contact (des, n_con, 0);
						list_dests = e_destination_list_get_dests (des);

						g_list_free_full (contacts, g_object_unref);
					}

					e_book_query_unref (query);
					g_free (qu);
				}
				g_list_free (books);
			} else {
				card_dest.next = NULL;
				card_dest.prev = NULL;
				card_dest.data = destination;
				list_dests = &card_dest;
			}
		}

		for (l = list_dests; l; l = l->next) {
			EDestination *dest = l->data;
			EContact *contact;
			const gchar *name, *attendee = NULL;
			gchar *attr = NULL, *fburi = NULL;

			name = e_destination_get_name (dest);

			/* Get the field as attendee from the backend */
			if (e_cal_get_ldap_attribute (e_meeting_store_get_client (priv->store),
						      &attr, NULL)) {
				/* FIXME this should be more general */
				if (!g_ascii_strcasecmp (attr, "icscalendar")) {

					/* FIXME: this does not work, have to use first
					   e_destination_use_contact() */
					contact = e_destination_get_contact (dest);
					if (contact) {
						attendee = e_contact_get (contact, E_CONTACT_FREEBUSY_URL);
						if (!attendee)
							attendee = e_contact_get (contact, E_CONTACT_CALENDAR_URI);
					}
				}
			}

			/* If we couldn't get the attendee prior, get the email address as the default */
			if (attendee == NULL || *attendee == '\0') {
				attendee = e_destination_get_email (dest);
			}

			if (attendee == NULL || *attendee == '\0')
				continue;

			contact = e_destination_get_contact (dest);
			if (contact)
				fburi = e_contact_get (contact, E_CONTACT_FREEBUSY_URL);

			if (e_meeting_store_find_attendee (priv->store, attendee, NULL) == NULL) {
				EMeetingAttendee *ia = e_meeting_store_add_attendee_with_defaults (priv->store);

				e_meeting_attendee_set_address (ia, g_strdup_printf ("MAILTO:%s", attendee));
				e_meeting_attendee_set_role (ia, role);
				if (role == ICAL_ROLE_NONPARTICIPANT)
					e_meeting_attendee_set_cutype (ia, ICAL_CUTYPE_RESOURCE);
				e_meeting_attendee_set_cn (ia, g_strdup (name));

				if (fburi)
					e_meeting_attendee_set_fburi (ia, fburi);
			} else {
				if (g_slist_length (*la) == 1) {
					g_slist_free (*la);
					*la = NULL;
				} else
					*la = g_slist_remove_link (*la, g_slist_find_custom (*la, attendee, (GCompareFunc)g_ascii_strcasecmp));
			}
		}

		if (des) {
			g_object_unref (des);
			des = NULL;
		}

	}
}

static void
add_to_list (gpointer data, gpointer u_data)
{
	GSList **user_data = u_data;

	*user_data = g_slist_append (*user_data, (gpointer)itip_strip_mailto (e_meeting_attendee_get_address (data)));
}

static void
name_selector_dialog_close_cb (ENameSelectorDialog *dialog, gint response, gpointer data)
{
	EMeetingListView   *view = E_MEETING_LIST_VIEW (data);
	ENameSelectorModel *name_selector_model;
	EMeetingStore *store;
	const GPtrArray *attendees;
	gint i;
	GSList		  *la = NULL, *l;

	name_selector_model = e_name_selector_peek_model (view->priv->name_selector);
	store = E_MEETING_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (view)));
	attendees = e_meeting_store_get_attendees (store);

	/* get all the email ids of the attendees */
	g_ptr_array_foreach ((GPtrArray *)attendees, (GFunc) add_to_list, &la);

	for (i = 0; sections[i] != NULL; i++) {
		EDestinationStore *destination_store;
		GList             *destinations;

		e_name_selector_model_peek_section (name_selector_model, sections[i],
						    NULL, &destination_store);
		if (!destination_store) {
			g_warning ("destination store is NULL\n");
			continue;
		}

		destinations = e_destination_store_list_destinations (destination_store);
		process_section (view, destinations, roles[i], &la);
		g_list_free (destinations);
	}

	/* remove the deleted attendees from name selector */
	for (l = la; l != NULL; l = l->next) {
		EMeetingAttendee *ma = NULL;
		const gchar *email = l->data;
		gint i;

		ma = e_meeting_store_find_attendee (store, email, &i);

		if (ma) {
			if (e_meeting_attendee_get_edit_level (ma) != E_MEETING_ATTENDEE_EDIT_FULL)
				g_warning ("Not enough rights to delete attendee: %s\n", e_meeting_attendee_get_address (ma));
			else
				e_meeting_store_remove_attendee (store, ma);
		}
	}

	g_slist_free (la);
	gtk_widget_hide (GTK_WIDGET (dialog));
}

void
e_meeting_list_view_invite_others_dialog (EMeetingListView *view)
{
	e_name_selector_show_dialog (view->priv->name_selector,
				     GTK_WIDGET (view));
}

void
e_meeting_list_view_set_editable (EMeetingListView *lview, gboolean set)
{
	EMeetingListViewPrivate *priv = lview->priv;

	gint edit_level = set;

	g_hash_table_foreach (priv->renderers, change_edit_cols_for_organizer, GINT_TO_POINTER (edit_level));
}

ENameSelector *
e_meeting_list_view_get_name_selector (EMeetingListView *lview)
{
	EMeetingListViewPrivate *priv;

	g_return_val_if_fail (lview != NULL, NULL);
	g_return_val_if_fail (E_IS_MEETING_LIST_VIEW (lview), NULL);

	priv = lview->priv;

	return priv->name_selector;
}

void
e_meeting_list_view_set_name_selector (EMeetingListView *lview, ENameSelector *name_selector)
{
	EMeetingListViewPrivate *priv;

	g_return_if_fail (lview != NULL);
	g_return_if_fail (E_IS_MEETING_LIST_VIEW (lview));

	priv = lview->priv;

	if (priv->name_selector) {
		g_object_unref (priv->name_selector);
		priv->name_selector = NULL;
	}

	priv->name_selector = g_object_ref (name_selector);
}

