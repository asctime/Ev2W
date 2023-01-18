/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Copyright (C) 2002-2004 Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <camel/camel.h>
#include <libebackend/e-data-server-module.h>
#include <libedata-book/e-book-backend-factory.h>

#include "e-book-backend-exchange.h"
#include "e-book-backend-gal.h"

E_BOOK_BACKEND_FACTORY_SIMPLE (exchange, Exchange, e_book_backend_exchange_new)
E_BOOK_BACKEND_FACTORY_SIMPLE (gal, Gal, e_book_backend_gal_new)

static GType exchange_types[2];

void
eds_module_initialize (GTypeModule *type_module)
{
	exchange_types[0] = _exchange_factory_get_type (type_module);
	exchange_types[1] = _gal_factory_get_type (type_module);
}

void
eds_module_shutdown (void)
{
}

void
eds_module_list_types (const GType **types, gint *num_types)
{
	*types = exchange_types;
	*num_types = G_N_ELEMENTS (exchange_types);
}
