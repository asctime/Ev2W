/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#if !defined (__CAMEL_H_INSIDE__) && !defined (CAMEL_COMPILATION)
#error "Only <camel/camel.h> can be included directly."
#endif

#ifndef CAMEL_MULTIPART_ENCRYPTED_H
#define CAMEL_MULTIPART_ENCRYPTED_H

#include <camel/camel-multipart.h>

/* Standard GObject macros */
#define CAMEL_TYPE_MULTIPART_ENCRYPTED \
	(camel_multipart_encrypted_get_type ())
#define CAMEL_MULTIPART_ENCRYPTED(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), CAMEL_TYPE_MULTIPART_ENCRYPTED, CamelMultipartEncrypted))
#define CAMEL_MULTIPART_ENCRYPTED_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), CAMEL_TYPE_MULTIPART_ENCRYPTED, CamelMultipartEncryptedClass))
#define CAMEL_IS_MULTIPART_ENCRYPTED(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), CAMEL_TYPE_MULTIPART_ENCRYPTED))
#define CAMEL_IS_MULTIPART_ENCRYPTED_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), CAMEL_TYPE_MULTIPART_ENCRYPTED))
#define CAMEL_MULTIPART_ENCRYPTED_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), CAMEL_TYPE_MULTIPART_ENCRYPTED, CamelMultipartEncryptedClass))

G_BEGIN_DECLS

typedef struct _CamelMultipartEncrypted CamelMultipartEncrypted;
typedef struct _CamelMultipartEncryptedClass CamelMultipartEncryptedClass;

/* 'handy' enums for getting the internal parts of the multipart */
enum {
	CAMEL_MULTIPART_ENCRYPTED_VERSION,
	CAMEL_MULTIPART_ENCRYPTED_CONTENT
};

struct _CamelMultipartEncrypted {
	CamelMultipart parent;

	CamelMimePart *version;
	CamelMimePart *content;
	CamelMimePart *decrypted;

	gchar *protocol;
};

struct _CamelMultipartEncryptedClass {
	CamelMultipartClass parent_class;

};

GType camel_multipart_encrypted_get_type (void);

CamelMultipartEncrypted *camel_multipart_encrypted_new (void);

G_END_DECLS

#endif /* CAMEL_MULTIPART_ENCRYPTED_H */
