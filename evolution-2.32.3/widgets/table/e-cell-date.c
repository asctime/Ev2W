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

#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

#include <glib/gi18n.h>
#include "e-util/e-util.h"
#include "e-util/e-unicode.h"
#include "e-util/e-datetime-format.h"

#include "e-cell-date.h"

G_DEFINE_TYPE (ECellDate, e_cell_date, E_TYPE_CELL_TEXT)

static gchar *
ecd_get_text (ECellText *cell, ETableModel *model, gint col, gint row)
{
	time_t date = GPOINTER_TO_INT (e_table_model_value_at (model, col, row));
	const gchar *fmt_component, *fmt_part = NULL;

	if (date == 0) {
		return g_strdup (_("?"));
	}

	fmt_component = g_object_get_data ((GObject *) cell, "fmt-component");
	if (!fmt_component || !*fmt_component)
		fmt_component = "Default";
	else
		fmt_part = "table";
	return e_datetime_format_format (fmt_component, fmt_part, DTFormatKindDateTime, date);
}

static void
ecd_free_text (ECellText *cell, gchar *text)
{
	g_free (text);
}

static void
e_cell_date_class_init (ECellDateClass *klass)
{
	ECellTextClass *ectc = E_CELL_TEXT_CLASS (klass);

	ectc->get_text  = ecd_get_text;
	ectc->free_text = ecd_free_text;
}

static void
e_cell_date_init (ECellDate *ecd)
{
}

/**
 * e_cell_date_new:
 * @fontname: font to be used to render on the screen
 * @justify: Justification of the string in the cell.
 *
 * Creates a new ECell renderer that can be used to render dates that
 * that come from the model.  The value returned from the model is
 * interpreted as being a time_t.
 *
 * The ECellDate object support a large set of properties that can be
 * configured through the Gtk argument system and allows the user to have
 * a finer control of the way the string is displayed.  The arguments supported
 * allow the control of strikeout, bold, color and a date filter.
 *
 * The arguments "strikeout_column", "underline_column", "bold_column"
 * and "color_column" set and return an integer that points to a
 * column in the model that controls these settings.  So controlling
 * the way things are rendered is achieved by having special columns
 * in the model that will be used to flag whether the date should be
 * rendered with strikeout, underline, or bolded.  In the case of the
 * "color_column" argument, the column in the model is expected to
 * have a string that can be parsed by gdk_color_parse().
 *
 * Returns: an ECell object that can be used to render dates.
 */
ECell *
e_cell_date_new (const gchar *fontname, GtkJustification justify)
{
	ECellDate *ecd = g_object_new (E_TYPE_CELL_DATE, NULL);

	e_cell_text_construct (E_CELL_TEXT (ecd), fontname, justify);

	return (ECell *) ecd;
}

void
e_cell_date_set_format_component (ECellDate *ecd, const gchar *fmt_component)
{
	g_return_if_fail (ecd != NULL);

	g_object_set_data_full (
		G_OBJECT (ecd), "fmt-component",
		g_strdup (fmt_component), g_free);
}
