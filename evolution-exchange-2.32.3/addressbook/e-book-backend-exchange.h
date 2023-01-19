/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* Copyright (C) 2000-2004 Novell, Inc. */

#ifndef __E_BOOK_BACKEND_EXCHANGE_H__
#define __E_BOOK_BACKEND_EXCHANGE_H__

#include <libedata-book/e-book-backend-sync.h>

#define E_TYPE_BOOK_BACKEND_EXCHANGE        (e_book_backend_exchange_get_type ())
#define E_BOOK_BACKEND_EXCHANGE(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TYPE_BOOK_BACKEND_EXCHANGE, EBookBackendExchange))
#define E_BOOK_BACKEND_EXCHANGE_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), E_TYPE_BOOK_BACKEND, EBookBackendExchangeClass))
#define E_IS_BOOK_BACKEND_EXCHANGE(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TYPE_BOOK_BACKEND_EXCHANGE))
#define E_IS_BOOK_BACKEND_EXCHANGE_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TYPE_BOOK_BACKEND_EXCHANGE))

typedef struct EBookBackendExchangePrivate EBookBackendExchangePrivate;

typedef struct {
	EBookBackendSync parent_object;

	EBookBackendExchangePrivate *priv;

} EBookBackendExchange;

typedef struct {
	EBookBackendSyncClass parent_class;

} EBookBackendExchangeClass;

GType         e_book_backend_exchange_get_type (void);

EBookBackend *e_book_backend_exchange_new      (void);

#endif /* __E_BOOK_BACKEND_EXCHANGE_H__ */

