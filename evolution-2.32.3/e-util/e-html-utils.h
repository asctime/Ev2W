/*
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
 *		Dan Winship <danw@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __E_HTML_UTILS__
#define __E_HTML_UTILS__

#include <glib.h>

#define E_TEXT_TO_HTML_PRE               (1 << 0)
#define E_TEXT_TO_HTML_CONVERT_NL        (1 << 1)
#define E_TEXT_TO_HTML_CONVERT_SPACES    (1 << 2)
#define E_TEXT_TO_HTML_CONVERT_URLS      (1 << 3)
#define E_TEXT_TO_HTML_MARK_CITATION     (1 << 4)
#define E_TEXT_TO_HTML_CONVERT_ADDRESSES (1 << 5)
#define E_TEXT_TO_HTML_ESCAPE_8BIT       (1 << 6)
#define E_TEXT_TO_HTML_CITE              (1 << 7)

gchar *e_text_to_html_full (const gchar *input, guint flags, guint32 color);
gchar *e_text_to_html      (const gchar *input, guint flags);

#endif /* __E_HTML_UTILS__ */
