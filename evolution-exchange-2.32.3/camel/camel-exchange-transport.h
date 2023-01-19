/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Copyright (C) 2001-2004 Novell, Inc. */

/* camel-exchange-transport.h: Exchange-based transport class */

#ifndef CAMEL_EXCHANGE_TRANSPORT_H
#define CAMEL_EXCHANGE_TRANSPORT_H

#include <camel/camel.h>

/* Standard GObject macros */
#define CAMEL_TYPE_EXCHANGE_TRANSPORT \
	(camel_exchange_transport_get_type ())
#define CAMEL_EXCHANGE_TRANSPORT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), CAMEL_TYPE_EXCHANGE_TRANSPORT, CamelExchangeTransport))
#define CAMEL_EXCHANGE_TRANSPORT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), CAMEL_TYPE_EXCHANGE_TRANSPORT, CamelExchangeTransportClass))
#define CAMEL_IS_EXCHANGE_TRANSPORT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), CAMEL_TYPE_EXCHANGE_TRANSPORT))
#define CAMEL_IS_EXCHANGE_TRANSPORT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), CAMEL_TYPE_EXCHANGE_TRANSPORT))
#define CAMEL_EXCHANGE_TRANSPORT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), CAMEL_TYPE_EXCHANGE_TRANSPORT, CamelExchangeTransportClass))

G_BEGIN_DECLS

typedef struct _CamelExchangeTransport CamelExchangeTransport;
typedef struct _CamelExchangeTransportClass CamelExchangeTransportClass;

struct _CamelExchangeTransport {
	CamelTransport parent;
};

struct _CamelExchangeTransportClass {
	CamelTransportClass parent_class;
};

GType		camel_exchange_transport_get_type (void);

G_END_DECLS

#endif /* CAMEL_EXCHANGE_TRANSPORT_H */
