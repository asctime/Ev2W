/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Copyright (C) 2001-2004 Novell, Inc. */

/* camel-exchange-search.h: exchange folder search */

#ifndef CAMEL_EXCHANGE_SEARCH_H
#define CAMEL_EXCHANGE_SEARCH_H

#include <camel/camel.h>

/* Standard GObject macros */
#define CAMEL_TYPE_EXCHANGE_SEARCH \
	(camel_exchange_search_get_type ())
#define CAMEL_EXCHANGE_SEARCH(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), CAMEL_TYPE_EXCHANGE_SEARCH, CamelExchangeSearch))
#define CAMEL_EXCHANGE_SEARCH_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), CAMEL_TYPE_EXCHANGE_SEARCH, CamelExchangeSearchClass))
#define CAMEL_IS_EXCHANGE_SEARCH(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), CAMEL_TYPE_EXCHANGE_SEARCH))
#define CAMEL_IS_EXCHANGE_SEARCH_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), CAMEL_TYPE_EXCHANGE_SEARCH))
#define CAMEL_IS_EXCHANGE_SEARCH_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), CAMEL_TYPE_EXCHANGE_SEARCH, CamelExchangeSearchClass))

G_BEGIN_DECLS

typedef struct _CamelExchangeSearch CamelExchangeSearch;
typedef struct _CamelExchangeSearchClass CamelExchangeSearchClass;

struct _CamelExchangeSearch {
	CamelFolderSearch parent;
};

struct _CamelExchangeSearchClass {
	CamelFolderSearchClass parent_class;
};

GType		camel_exchange_search_get_type	(void);
CamelFolderSearch *
		camel_exchange_search_new	(void);

G_END_DECLS

#endif /* CAMEL_EXCHANGE_SEARCH_H */
