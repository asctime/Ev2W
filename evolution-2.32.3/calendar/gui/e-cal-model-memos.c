/*
 * Evolution memos - Data model for ETable
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
 *		Rodrigo Moya <rodrigo@ximian.com>
 *      Nathan Owens <pianocomp81@yahoo.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib/gi18n.h>
#include "e-cal-model-memos.h"
#include "e-cell-date-edit-text.h"
#include "misc.h"

#define d(x) (x)

struct _ECalModelMemosPrivate {
	gint dummy;
};

static void e_cal_model_memos_finalize (GObject *object);

static gint ecmm_column_count (ETableModel *etm);
static gpointer ecmm_value_at (ETableModel *etm, gint col, gint row);
static void ecmm_set_value_at (ETableModel *etm, gint col, gint row, gconstpointer value);
static gboolean ecmm_is_cell_editable (ETableModel *etm, gint col, gint row);
static gpointer ecmm_duplicate_value (ETableModel *etm, gint col, gconstpointer value);
static void ecmm_free_value (ETableModel *etm, gint col, gpointer value);
static gpointer ecmm_initialize_value (ETableModel *etm, gint col);
static gboolean ecmm_value_is_empty (ETableModel *etm, gint col, gconstpointer value);
static gchar *ecmm_value_to_string (ETableModel *etm, gint col, gconstpointer value);

static void ecmm_fill_component_from_model (ECalModel *model, ECalModelComponent *comp_data,
					    ETableModel *source_model, gint row);

G_DEFINE_TYPE (ECalModelMemos, e_cal_model_memos, E_TYPE_CAL_MODEL)

static void
e_cal_model_memos_class_init (ECalModelMemosClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	ETableModelClass *etm_class = E_TABLE_MODEL_CLASS (klass);
	ECalModelClass *model_class = E_CAL_MODEL_CLASS (klass);

	object_class->finalize = e_cal_model_memos_finalize;

	etm_class->column_count = ecmm_column_count;
	etm_class->value_at = ecmm_value_at;
	etm_class->set_value_at = ecmm_set_value_at;
	etm_class->is_cell_editable = ecmm_is_cell_editable;
	etm_class->duplicate_value = ecmm_duplicate_value;
	etm_class->free_value = ecmm_free_value;
	etm_class->initialize_value = ecmm_initialize_value;
	etm_class->value_is_empty = ecmm_value_is_empty;
	etm_class->value_to_string = ecmm_value_to_string;

	model_class->fill_component_from_model = ecmm_fill_component_from_model;
}

static void
e_cal_model_memos_init (ECalModelMemos *model)
{
	ECalModelMemosPrivate *priv;

	priv = g_new0 (ECalModelMemosPrivate, 1);
	model->priv = priv;

	e_cal_model_set_component_kind (E_CAL_MODEL (model), ICAL_VJOURNAL_COMPONENT);
}

static void
e_cal_model_memos_finalize (GObject *object)
{
	ECalModelMemosPrivate *priv;
	ECalModelMemos *model = (ECalModelMemos *) object;

	g_return_if_fail (E_IS_CAL_MODEL_MEMOS (model));

	priv = model->priv;
	if (priv) {
		g_free (priv);
		model->priv = NULL;
	}

	if (G_OBJECT_CLASS (e_cal_model_memos_parent_class)->finalize)
		G_OBJECT_CLASS (e_cal_model_memos_parent_class)->finalize (object);
}

/* ETableModel methods */
static gint
ecmm_column_count (ETableModel *etm)
{
	return E_CAL_MODEL_MEMOS_FIELD_LAST;
}

static gpointer
ecmm_value_at (ETableModel *etm, gint col, gint row)
{
	ECalModelComponent *comp_data;
	ECalModelMemos *model = (ECalModelMemos *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL_MEMOS (model), NULL);

	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_MEMOS_FIELD_LAST, NULL);
	g_return_val_if_fail (row >= 0 && row < e_table_model_row_count (etm), NULL);

	if (col < E_CAL_MODEL_FIELD_LAST)
		return E_TABLE_MODEL_CLASS (e_cal_model_memos_parent_class)->value_at (etm, col, row);

	comp_data = e_cal_model_get_component_at (E_CAL_MODEL (model), row);
	if (!comp_data)
		return (gpointer) "";

	return (gpointer) "";
}

static void
ecmm_set_value_at (ETableModel *etm, gint col, gint row, gconstpointer value)
{
	ECalModelComponent *comp_data;
	ECalModelMemos *model = (ECalModelMemos *) etm;

	g_return_if_fail (E_IS_CAL_MODEL_MEMOS (model));
	g_return_if_fail (col >= 0 && col < E_CAL_MODEL_MEMOS_FIELD_LAST);
	g_return_if_fail (row >= 0 && row < e_table_model_row_count (etm));

	if (col < E_CAL_MODEL_FIELD_LAST) {
		E_TABLE_MODEL_CLASS (e_cal_model_memos_parent_class)->set_value_at (etm, col, row, value);
		return;
	}

	comp_data = e_cal_model_get_component_at (E_CAL_MODEL (model), row);
	if (!comp_data) {
		g_warning("couldn't get component data: row == %d", row);
		return;
	}

	/* TODO ask about mod type */
	if (!e_cal_modify_object (comp_data->client, comp_data->icalcomp, CALOBJ_MOD_ALL, NULL)) {
		g_warning (G_STRLOC ": Could not modify the object!");

		/* TODO Show error dialog */
	}
}

static gboolean
ecmm_is_cell_editable (ETableModel *etm, gint col, gint row)
{
	ECalModelMemos *model = (ECalModelMemos *) etm;
	gboolean retval = FALSE;

	g_return_val_if_fail (E_IS_CAL_MODEL_MEMOS (model), FALSE);
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_MEMOS_FIELD_LAST, FALSE);
	g_return_val_if_fail (row >= -1 || (row >= 0 && row < e_table_model_row_count (etm)), FALSE);

	if (!e_cal_model_test_row_editable (E_CAL_MODEL (etm), row))
		return FALSE;

	if (col < E_CAL_MODEL_FIELD_LAST)
		retval = E_TABLE_MODEL_CLASS (e_cal_model_memos_parent_class)->is_cell_editable (etm, col, row);

	return retval;
}

static gpointer
ecmm_duplicate_value (ETableModel *etm, gint col, gconstpointer value)
{
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_MEMOS_FIELD_LAST, NULL);

	if (col < E_CAL_MODEL_FIELD_LAST)
		return E_TABLE_MODEL_CLASS (e_cal_model_memos_parent_class)->duplicate_value (etm, col, value);

	return NULL;
}

static void
ecmm_free_value (ETableModel *etm, gint col, gpointer value)
{
	g_return_if_fail (col >= 0 && col < E_CAL_MODEL_MEMOS_FIELD_LAST);

	if (col < E_CAL_MODEL_FIELD_LAST) {
		E_TABLE_MODEL_CLASS (e_cal_model_memos_parent_class)->free_value (etm, col, value);
		return;
	}
}

static gpointer
ecmm_initialize_value (ETableModel *etm, gint col)
{
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_MEMOS_FIELD_LAST, NULL);

	if (col < E_CAL_MODEL_FIELD_LAST)
		return E_TABLE_MODEL_CLASS (e_cal_model_memos_parent_class)->initialize_value (etm, col);

	return NULL;
}

static gboolean
ecmm_value_is_empty (ETableModel *etm, gint col, gconstpointer value)
{
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_MEMOS_FIELD_LAST, TRUE);

	if (col < E_CAL_MODEL_FIELD_LAST)
		return E_TABLE_MODEL_CLASS (e_cal_model_memos_parent_class)->value_is_empty (etm, col, value);

	return TRUE;
}

static gchar *
ecmm_value_to_string (ETableModel *etm, gint col, gconstpointer value)
{
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_MEMOS_FIELD_LAST, g_strdup (""));

	if (col < E_CAL_MODEL_FIELD_LAST)
		return E_TABLE_MODEL_CLASS (e_cal_model_memos_parent_class)->value_to_string (etm, col, value);

	return g_strdup ("");
}

/* ECalModel class methods */

static void
ecmm_fill_component_from_model (ECalModel *model, ECalModelComponent *comp_data,
				ETableModel *source_model, gint row)
{
	icaltimetype start;
	g_return_if_fail (E_IS_CAL_MODEL_MEMOS (model));
	g_return_if_fail (comp_data != NULL);
	g_return_if_fail (E_IS_TABLE_MODEL (source_model));

	start = icalcomponent_get_dtstart (comp_data->icalcomp);
	if (icaltime_compare_date_only (start, icaltime_null_time ()) == 0) {
		start = icaltime_today ();
		icalcomponent_set_dtstart (comp_data->icalcomp, start);
	}

}

/**
 * e_cal_model_memos_new
 */
ECalModel *
e_cal_model_memos_new (void)
{
	return g_object_new (E_TYPE_CAL_MODEL_MEMOS, NULL);
}
