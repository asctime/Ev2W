/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *
 * Copyright (C) 2004 Novell, Inc.
 *
 * Authors: Devashish Sharma <sdevashish@novell.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef E_BOOK_BACKEND_DB_CACHE_H
#define E_BOOK_BACKEND_DB_CACHE_H

#include <libebook/e-contact.h>
#include "db.h"

EContact* e_book_backend_db_cache_get_contact (DB *db, const gchar *uid);
void string_to_dbt(const gchar *str, DBT *dbt);
gchar *e_book_backend_db_cache_get_filename(DB *db);
void e_book_backend_db_cache_set_filename(DB *db, const gchar *filename);
gchar *e_book_backend_db_cache_get_time(DB *db);
void e_book_backend_db_cache_set_time(DB *db, const gchar *time);
gboolean e_book_backend_db_cache_add_contact (DB *db,
					   EContact *contact);
gboolean e_book_backend_db_cache_remove_contact (DB *db,
					      const gchar *uid);
gboolean e_book_backend_db_cache_check_contact (DB *db, const gchar *uid);
GList*   e_book_backend_db_cache_get_contacts (DB *db, const gchar *query);
gboolean e_book_backend_db_cache_exists (const gchar *uri);
void     e_book_backend_db_cache_set_populated (DB *db);
gboolean e_book_backend_db_cache_is_populated (DB *db);
GPtrArray* e_book_backend_db_cache_search (DB *db, const gchar *query);

G_END_DECLS

#endif

