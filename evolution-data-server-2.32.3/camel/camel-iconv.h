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

#ifndef CAMEL_ICONV_H
#define CAMEL_ICONV_H

#include <sys/types.h>
#include <iconv.h>
#include <glib.h>

G_BEGIN_DECLS

const gchar *	camel_iconv_locale_charset	(void);
const gchar *	camel_iconv_locale_language	(void);

const gchar *	camel_iconv_charset_name	(const gchar *charset);
const gchar *	camel_iconv_charset_language	(const gchar *charset);

iconv_t		camel_iconv_open		(const gchar *to,
						 const gchar *from);
gsize		camel_iconv			(iconv_t cd,
						 const gchar **inbuf,
						 gsize *inleft,
						 gchar **outbuf,
						 gsize *outleft);
void		camel_iconv_close		(iconv_t cd);

G_END_DECLS

#endif /* CAMEL_ICONV_H */
