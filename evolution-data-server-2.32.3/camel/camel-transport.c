/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-transport.c : Abstract class for an email transport */

/*
 *
 * Author :
 *  Dan Winship <danw@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "camel-address.h"
#include "camel-debug.h"
#include "camel-mime-message.h"
#include "camel-transport.h"

#define CAMEL_TRANSPORT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), CAMEL_TYPE_TRANSPORT, CamelTransportPrivate))

struct _CamelTransportPrivate {
	GMutex *send_lock;   /* for locking send operations */
};

G_DEFINE_ABSTRACT_TYPE (CamelTransport, camel_transport, CAMEL_TYPE_SERVICE)

static void
transport_finalize (GObject *object)
{
	CamelTransportPrivate *priv;

	priv = CAMEL_TRANSPORT_GET_PRIVATE (object);

	g_mutex_free (priv->send_lock);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (camel_transport_parent_class)->finalize (object);
}

static void
camel_transport_class_init (CamelTransportClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (CamelTransportPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = transport_finalize;
}

static void
camel_transport_init (CamelTransport *transport)
{
	transport->priv = CAMEL_TRANSPORT_GET_PRIVATE (transport);

	transport->priv->send_lock = g_mutex_new ();
}

/**
 * camel_transport_send_to:
 * @transport: a #CamelTransport object
 * @message: a #CamelMimeMessage to send
 * @from: a #CamelAddress to send from
 * @recipients: a #CamelAddress containing all recipients
 * @error: return location for a #GError, or %NULL
 *
 * Sends the message to the given recipients, regardless of the contents
 * of @message. If the message contains a "Bcc" header, the transport
 * is responsible for stripping it.
 *
 * Return %TRUE on success or %FALSE on fail
 **/
gboolean
camel_transport_send_to (CamelTransport *transport,
                         CamelMimeMessage *message,
                         CamelAddress *from,
                         CamelAddress *recipients,
                         GError **error)
{
	CamelTransportClass *class;
	gboolean success;

	g_return_val_if_fail (CAMEL_IS_TRANSPORT (transport), FALSE);
	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), FALSE);
	g_return_val_if_fail (CAMEL_IS_ADDRESS (from), FALSE);
	g_return_val_if_fail (CAMEL_IS_ADDRESS (recipients), FALSE);

	class = CAMEL_TRANSPORT_GET_CLASS (transport);
	g_return_val_if_fail (class->send_to != NULL, FALSE);

	camel_transport_lock (transport, CAMEL_TRANSPORT_SEND_LOCK);

	success = class->send_to (transport, message, from, recipients, error);
	CAMEL_CHECK_GERROR (transport, send_to, success, error);

	camel_transport_unlock (transport, CAMEL_TRANSPORT_SEND_LOCK);

	return success;
}

/**
 * camel_transport_lock:
 * @transport: a #CamelTransport
 * @lock: lock type to lock
 *
 * Locks #transport's #lock. Unlock it with camel_transport_unlock().
 *
 * Since: 2.32
 **/
void
camel_transport_lock (CamelTransport *transport,
                      CamelTransportLock lock)
{
	g_return_if_fail (CAMEL_IS_TRANSPORT (transport));

	switch (lock) {
		case CAMEL_TRANSPORT_SEND_LOCK:
			g_mutex_lock (transport->priv->send_lock);
			break;
		default:
			g_return_if_reached ();
	}
}

/**
 * camel_transport_unlock:
 * @transport: a #CamelTransport
 * @lock: lock type to unlock
 *
 * Unlocks #transport's #lock, previously locked with camel_transport_lock().
 *
 * Since: 2.32
 **/
void
camel_transport_unlock (CamelTransport *transport,
                        CamelTransportLock lock)
{
	g_return_if_fail (CAMEL_IS_TRANSPORT (transport));

	switch (lock) {
		case CAMEL_TRANSPORT_SEND_LOCK:
			g_mutex_unlock (transport->priv->send_lock);
			break;
		default:
			g_return_if_reached ();
	}
}
