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
 *		Chris Lahey <clahey@ximian.com>
 *		Miguel de Icaza <miguel@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_TABLE_COLUMN_H_
#define _E_TABLE_COLUMN_H_

#include <glib-object.h>
#include <gdk/gdk.h>
#include <table/e-table-sort-info.h>
#include <table/e-table-col.h>

G_BEGIN_DECLS

typedef struct _ETableHeader ETableHeader;

#define E_TABLE_HEADER_TYPE        (e_table_header_get_type ())
#define E_TABLE_HEADER(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TABLE_HEADER_TYPE, ETableHeader))
#define E_TABLE_HEADER_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TABLE_HEADER_TYPE, ETableHeaderClass))
#define E_IS_TABLE_HEADER(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TABLE_HEADER_TYPE))
#define E_IS_TABLE_HEADER_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TABLE_HEADER_TYPE))
#define E_TABLE_HEADER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), E_TABLE_HEADER_TYPE, ETableHeaderClass))

typedef gboolean (*ETableColCheckFunc) (ETableCol *col, gpointer user_data);

/*
 * A Columnar header.
 */
struct _ETableHeader {
	GObject base;

	gint col_count;
	gint width;
	gint nominal_width;
	gint width_extras;

	ETableSortInfo *sort_info;
	gint sort_info_group_change_id;

	ETableCol **columns;

	GSList *change_queue, *change_tail;
	gint idle;
};

typedef struct {
	GObjectClass parent_class;

	void (*structure_change) (ETableHeader *eth);
	void (*dimension_change) (ETableHeader *eth, gint width);
	void (*expansion_change) (ETableHeader *eth);
	gint (*request_width) (ETableHeader *eth, gint col);
} ETableHeaderClass;

GType         e_table_header_get_type                     (void);
ETableHeader *e_table_header_new                          (void);

void          e_table_header_add_column                   (ETableHeader       *eth,
							   ETableCol          *tc,
							   gint                 pos);
ETableCol    *e_table_header_get_column                   (ETableHeader       *eth,
							   gint                 column);
ETableCol    *e_table_header_get_column_by_col_idx        (ETableHeader       *eth,
							   gint                 col_idx);
gint           e_table_header_count                        (ETableHeader       *eth);
gint           e_table_header_index                        (ETableHeader       *eth,
							   gint                 col);
gint           e_table_header_get_index_at                 (ETableHeader       *eth,
							   gint                 x_offset);
ETableCol   **e_table_header_get_columns            (ETableHeader *eth);
gint           e_table_header_get_selected                 (ETableHeader       *eth);

gint           e_table_header_total_width                  (ETableHeader       *eth);
gint           e_table_header_min_width                    (ETableHeader       *eth);
void          e_table_header_move                         (ETableHeader       *eth,
							   gint                 source_index,
							   gint                 target_index);
void          e_table_header_remove                       (ETableHeader       *eth,
							   gint                 idx);
void          e_table_header_set_size                     (ETableHeader       *eth,
							   gint                 idx,
							   gint                 size);
void          e_table_header_set_selection                (ETableHeader       *eth,
							   gboolean            allow_selection);
gint           e_table_header_col_diff                     (ETableHeader       *eth,
							   gint                 start_col,
							   gint                 end_col);

void          e_table_header_calc_widths                  (ETableHeader       *eth);
GList        *e_table_header_get_selected_indexes         (ETableHeader       *eth);
void          e_table_header_update_horizontal            (ETableHeader       *eth);
gint           e_table_header_prioritized_column           (ETableHeader       *eth);
ETableCol    *e_table_header_prioritized_column_selected  (ETableHeader       *eth,
							   ETableColCheckFunc  check_func,
							   gpointer            user_data);

G_END_DECLS

#endif /* _E_TABLE_HEADER_H_ */

