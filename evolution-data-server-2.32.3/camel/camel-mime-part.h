/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; fill-column: 160 -*- */
/* camel-mime-part.h : class for a mime part */

/*
 *
 * Authors: Bertrand Guiheneuf <bertrand@helixcode.com>
 *	    Michael Zucchi <notzed@ximian.com>
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

#if !defined (__CAMEL_H_INSIDE__) && !defined (CAMEL_COMPILATION)
#error "Only <camel/camel.h> can be included directly."
#endif

#ifndef CAMEL_MIME_PART_H
#define CAMEL_MIME_PART_H

#include <camel/camel-medium.h>
#include <camel/camel-mime-utils.h>
#include <camel/camel-mime-parser.h>

/* Standard GObject macros */
#define CAMEL_TYPE_MIME_PART \
	(camel_mime_part_get_type ())
#define CAMEL_MIME_PART(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), CAMEL_TYPE_MIME_PART, CamelMimePart))
#define CAMEL_MIME_PART_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), CAMEL_TYPE_MIME_PART, CamelMimePartClass))
#define CAMEL_IS_MIME_PART(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), CAMEL_TYPE_MIME_PART))
#define CAMEL_IS_MIME_PART_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), CAMEL_TYPE_MIME_PART))
#define CAMEL_MIME_PART_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), CAMEL_TYPE_MIME_PART, CamelMimePartClass))

G_BEGIN_DECLS

typedef struct _CamelMimePart CamelMimePart;
typedef struct _CamelMimePartClass CamelMimePartClass;
typedef struct _CamelMimePartPrivate CamelMimePartPrivate;

struct _CamelMimePart {
	CamelMedium parent;
	CamelMimePartPrivate *priv;

	struct _camel_header_raw *headers; /* mime headers */
};

struct _CamelMimePartClass {
	CamelMediumClass parent_class;

	gint		(*construct_from_parser)(CamelMimePart *mime_part,
						 CamelMimeParser *parser,
						 GError **error);
};

GType camel_mime_part_get_type (void);

/* public methods */
CamelMimePart *  camel_mime_part_new                    (void);

void		 camel_mime_part_set_description	(CamelMimePart *mime_part, const gchar *description);
const     gchar  *camel_mime_part_get_description	(CamelMimePart *mime_part);

void		 camel_mime_part_set_disposition	(CamelMimePart *mime_part, const gchar *disposition);
const     gchar  *camel_mime_part_get_disposition	(CamelMimePart *mime_part);
const CamelContentDisposition *camel_mime_part_get_content_disposition (CamelMimePart *mime_part);

void		 camel_mime_part_set_filename		(CamelMimePart *mime_part, const gchar *filename);
const	  gchar  *camel_mime_part_get_filename		(CamelMimePart *mime_part);

void             camel_mime_part_set_content_id		(CamelMimePart *mime_part, const gchar *contentid);
const	  gchar  *camel_mime_part_get_content_id		(CamelMimePart *mime_part);

void		 camel_mime_part_set_content_md5	(CamelMimePart *mime_part, const gchar *md5sum);
const	  gchar  *camel_mime_part_get_content_md5	(CamelMimePart *mime_part);

void		 camel_mime_part_set_content_location	(CamelMimePart *mime_part, const gchar *location);
const	  gchar  *camel_mime_part_get_content_location	(CamelMimePart *mime_part);

void		 camel_mime_part_set_encoding		(CamelMimePart *mime_part, CamelTransferEncoding encoding);
CamelTransferEncoding camel_mime_part_get_encoding	(CamelMimePart *mime_part);

void		 camel_mime_part_set_content_languages	(CamelMimePart *mime_part, GList *content_languages);
const	  GList *camel_mime_part_get_content_languages	(CamelMimePart *mime_part);

/* FIXME: what about content-type parameters?   what about major/minor parts? */
void               camel_mime_part_set_content_type	(CamelMimePart *mime_part, const gchar *content_type);
CamelContentType  *camel_mime_part_get_content_type	(CamelMimePart *mime_part);

/* construction */
gint		camel_mime_part_construct_from_parser  (CamelMimePart *mime_part, CamelMimeParser *parser, GError **error);

/* utility functions */
void	camel_mime_part_set_content	       (CamelMimePart *mime_part,
							const gchar *data, gint length, const gchar *type);

gsize          camel_mime_part_get_content_size       (CamelMimePart *mime_part);

G_END_DECLS

#endif /* CAMEL_MIME_PART_H */
