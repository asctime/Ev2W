/* Libart_LGPL - library of basic graphic primitives
 * Copyright (C) 1998 Raph Levien
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __ART_VPATH_BPATH_H__
#define __ART_VPATH_BPATH_H__

#include <libart_lgpl/art_bpath.h>
#include <libart_lgpl/art_vpath.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

ArtPoint *art_bezier_to_vec (gdouble x0, gdouble y0,
			     gdouble x1, gdouble y1,
			     gdouble x2, gdouble y2,
			     gdouble x3, gdouble y3,
			     ArtPoint *p,
			     gint level);

ArtVpath *art_bez_path_to_vec (const ArtBpath *bez, gdouble flatness);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __ART_VPATH_BPATH_H__ */
