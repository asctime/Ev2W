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

#include <stdlib.h>
#include <string.h>

#include "e-util/e-util.h"

#include "e-table-sorted.h"
#include "e-table-sorting-utils.h"

#define d(x)

#define INCREMENT_AMOUNT 100

/* workaround for avoding API breakage */
#define ets_get_type e_table_sorted_get_type
G_DEFINE_TYPE (ETableSorted, ets, E_TABLE_SUBSET_TYPE)

/* maximum insertions between an idle event that we will do without scheduling an idle sort */
#define ETS_INSERT_MAX (4)

static void ets_sort_info_changed        (ETableSortInfo *info, ETableSorted *ets);
static void ets_sort                     (ETableSorted *ets);
static void ets_proxy_model_changed      (ETableSubset *etss, ETableModel *source);
static void ets_proxy_model_row_changed  (ETableSubset *etss, ETableModel *source, gint row);
static void ets_proxy_model_cell_changed (ETableSubset *etss, ETableModel *source, gint col, gint row);
static void ets_proxy_model_rows_inserted (ETableSubset *etss, ETableModel *source, gint row, gint count);
static void ets_proxy_model_rows_deleted  (ETableSubset *etss, ETableModel *source, gint row, gint count);

static void
ets_dispose (GObject *object)
{
	ETableSorted *ets = E_TABLE_SORTED (object);

	if (ets->sort_idle_id)
		g_source_remove (ets->sort_idle_id);
	ets->sort_idle_id = 0;

	if (ets->insert_idle_id)
		g_source_remove (ets->insert_idle_id);
	ets->insert_idle_id = 0;

	if (ets->sort_info) {
		g_signal_handler_disconnect (G_OBJECT (ets->sort_info),
					     ets->sort_info_changed_id);
		g_object_unref (ets->sort_info);
		ets->sort_info = NULL;
	}

	if (ets->full_header)
		g_object_unref (ets->full_header);
	ets->full_header = NULL;

	G_OBJECT_CLASS (ets_parent_class)->dispose (object);
}

static void
ets_class_init (ETableSortedClass *klass)
{
	ETableSubsetClass *etss_class = E_TABLE_SUBSET_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	etss_class->proxy_model_changed = ets_proxy_model_changed;
	etss_class->proxy_model_row_changed = ets_proxy_model_row_changed;
	etss_class->proxy_model_cell_changed = ets_proxy_model_cell_changed;
	etss_class->proxy_model_rows_inserted = ets_proxy_model_rows_inserted;
	etss_class->proxy_model_rows_deleted = ets_proxy_model_rows_deleted;

	object_class->dispose = ets_dispose;
}

static void
ets_init (ETableSorted *ets)
{
	ets->full_header = NULL;
	ets->sort_info = NULL;

	ets->sort_info_changed_id = 0;

	ets->sort_idle_id = 0;
	ets->insert_count = 0;
}

static gboolean
ets_sort_idle (ETableSorted *ets)
{
	g_object_ref (ets);
	ets_sort (ets);
	ets->sort_idle_id = 0;
	ets->insert_count = 0;
	g_object_unref (ets);
	return FALSE;
}

static gboolean
ets_insert_idle (ETableSorted *ets)
{
	ets->insert_count = 0;
	ets->insert_idle_id = 0;
	return FALSE;
}

ETableModel *
e_table_sorted_new (ETableModel *source, ETableHeader *full_header, ETableSortInfo *sort_info)
{
	ETableSorted *ets = g_object_new (E_TABLE_SORTED_TYPE, NULL);
	ETableSubset *etss = E_TABLE_SUBSET (ets);

	if (E_TABLE_SUBSET_CLASS (ets_parent_class)->proxy_model_pre_change)
		(E_TABLE_SUBSET_CLASS (ets_parent_class)->proxy_model_pre_change) (etss, source);

	if (e_table_subset_construct (etss, source, 0) == NULL) {
		g_object_unref (ets);
		return NULL;
	}

	ets->sort_info = sort_info;
	g_object_ref (ets->sort_info);
	ets->full_header = full_header;
	g_object_ref (ets->full_header);

	ets_proxy_model_changed (etss, source);

	ets->sort_info_changed_id = g_signal_connect (G_OBJECT (sort_info), "sort_info_changed",
						      G_CALLBACK (ets_sort_info_changed), ets);

	return E_TABLE_MODEL (ets);
}

static void
ets_sort_info_changed (ETableSortInfo *info, ETableSorted *ets)
{
	ets_sort (ets);
}

static void
ets_proxy_model_changed (ETableSubset *subset, ETableModel *source)
{
	gint rows, i;

	rows = e_table_model_row_count (source);

	g_free (subset->map_table);
	subset->n_map = rows;
	subset->map_table = g_new (int, rows);

	for (i = 0; i < rows; i++) {
		subset->map_table[i] = i;
	}

	if (!E_TABLE_SORTED (subset)->sort_idle_id)
		E_TABLE_SORTED (subset)->sort_idle_id = g_idle_add_full (50, (GSourceFunc) ets_sort_idle, subset, NULL);

	e_table_model_changed (E_TABLE_MODEL (subset));
}

static void
ets_proxy_model_row_changed (ETableSubset *subset, ETableModel *source, gint row)
{
	if (!E_TABLE_SORTED (subset)->sort_idle_id)
		E_TABLE_SORTED (subset)->sort_idle_id = g_idle_add_full (50, (GSourceFunc) ets_sort_idle, subset, NULL);

	if (E_TABLE_SUBSET_CLASS (ets_parent_class)->proxy_model_row_changed)
		(E_TABLE_SUBSET_CLASS (ets_parent_class)->proxy_model_row_changed) (subset, source, row);
}

static void
ets_proxy_model_cell_changed (ETableSubset *subset, ETableModel *source, gint col, gint row)
{
	ETableSorted *ets = E_TABLE_SORTED (subset);
	if (e_table_sorting_utils_affects_sort (ets->sort_info, ets->full_header, col))
		ets_proxy_model_row_changed (subset, source, row);
	else if (E_TABLE_SUBSET_CLASS (ets_parent_class)->proxy_model_cell_changed)
		(E_TABLE_SUBSET_CLASS (ets_parent_class)->proxy_model_cell_changed) (subset, source, col, row);
}

static void
ets_proxy_model_rows_inserted (ETableSubset *etss, ETableModel *source, gint row, gint count)
{
	ETableModel *etm = E_TABLE_MODEL (etss);
	ETableSorted *ets = E_TABLE_SORTED (etss);
	gint i;
	gboolean full_change = FALSE;

	if (count == 0) {
		e_table_model_no_change (etm);
		return;
	}

	if (row != etss->n_map) {
		full_change = TRUE;
		for (i = 0; i < etss->n_map; i++) {
			if (etss->map_table[i] >= row) {
				etss->map_table[i] += count;
			}
		}
	}

	etss->map_table = g_realloc (etss->map_table, (etss->n_map + count) * sizeof (gint));

	for (; count > 0; count--) {
		if (!full_change)
			e_table_model_pre_change (etm);
		i = etss->n_map;
		if (ets->sort_idle_id == 0) {
			/* this is to see if we're inserting a lot of things between idle loops.
			   If we are, we're busy, its faster to just append and perform a full sort later */
			ets->insert_count++;
			if (ets->insert_count > ETS_INSERT_MAX) {
				/* schedule a sort, and append instead */
				ets->sort_idle_id = g_idle_add_full (50, (GSourceFunc) ets_sort_idle, ets, NULL);
			} else {
				/* make sure we have an idle handler to reset the count every now and then */
				if (ets->insert_idle_id == 0) {
					ets->insert_idle_id = g_idle_add_full (40, (GSourceFunc) ets_insert_idle, ets, NULL);
				}
				i = e_table_sorting_utils_insert (etss->source, ets->sort_info, ets->full_header, etss->map_table, etss->n_map, row);
				memmove (etss->map_table + i + 1, etss->map_table + i, (etss->n_map - i) * sizeof (gint));
			}
		}
		etss->map_table[i] = row;
		etss->n_map++;
		if (!full_change) {
			e_table_model_row_inserted (etm, i);
		}

		d(g_print("inserted row %d", row));
		row++;
	}
	if (full_change)
		e_table_model_changed (etm);
	else
		e_table_model_no_change (etm);
	d (e_table_subset_print_debugging (etss));
}

static void
ets_proxy_model_rows_deleted (ETableSubset *etss, ETableModel *source, gint row, gint count)
{
	ETableModel *etm = E_TABLE_MODEL (etss);
	gint i;
	gboolean shift;
	gint j;

	shift = row == etss->n_map - count;

	for (j = 0; j < count; j++) {
		for (i = 0; i < etss->n_map; i++) {
			if (etss->map_table[i] == row + j) {
				if (shift)
					e_table_model_pre_change (etm);
				memmove (etss->map_table + i, etss->map_table + i + 1, (etss->n_map - i - 1) * sizeof (gint));
				etss->n_map--;
				if (shift)
					e_table_model_row_deleted (etm, i);
			}
		}
	}
	if (!shift) {
		for (i = 0; i < etss->n_map; i++) {
			if (etss->map_table[i] >= row)
				etss->map_table[i] -= count;
		}

		e_table_model_changed (etm);
	} else {
		e_table_model_no_change (etm);
	}

	d(g_print("deleted row %d count %d", row, count));
	d (e_table_subset_print_debugging (etss));
}

static void
ets_sort (ETableSorted *ets)
{
	ETableSubset *etss = E_TABLE_SUBSET (ets);
	static gint reentering = 0;
	if (reentering)
		return;
	reentering = 1;

	e_table_model_pre_change (E_TABLE_MODEL (ets));

	e_table_sorting_utils_sort (etss->source, ets->sort_info, ets->full_header, etss->map_table, etss->n_map);

	e_table_model_changed (E_TABLE_MODEL (ets));
	reentering = 0;
}
