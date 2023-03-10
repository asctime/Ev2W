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

#include "config.h"
#include "art_rgb_affine.h"

#include <math.h>
#include "art_misc.h"
#include "art_point.h"
#include "art_affine.h"
#include "art_rgb_affine_private.h"

/* This module handles compositing of affine-transformed rgb images
   over rgb pixel buffers. */

/**
 * art_rgb_affine: Affine transform source RGB image and composite.
 * @dst: Destination image RGB buffer.
 * @x0: Left coordinate of destination rectangle.
 * @y0: Top coordinate of destination rectangle.
 * @x1: Right coordinate of destination rectangle.
 * @y1: Bottom coordinate of destination rectangle.
 * @dst_rowstride: Rowstride of @dst buffer.
 * @src: Source image RGB buffer.
 * @src_width: Width of source image.
 * @src_height: Height of source image.
 * @src_rowstride: Rowstride of @src buffer.
 * @affine: Affine transform.
 * @level: Filter level.
 * @alphagamma: #ArtAlphaGamma for gamma-correcting the compositing.
 *
 * Affine transform the source image stored in @src, compositing over
 * the area of destination image @dst specified by the rectangle
 * (@x0, @y0) - (@x1, @y1). As usual in libart, the left and top edges
 * of this rectangle are included, and the right and bottom edges are
 * excluded.
 *
 * The @alphagamma parameter specifies that the alpha compositing be done
 * in a gamma-corrected color space. Since the source image is opaque RGB,
 * this argument only affects the edges. In the current implementation,
 * it is ignored.
 *
 * The @level parameter specifies the speed/quality tradeoff of the
 * image interpolation. Currently, only ART_FILTER_NEAREST is
 * implemented.
 **/
void
art_rgb_affine (art_u8 *dst, gint x0, gint y0, gint x1, gint y1, gint dst_rowstride,
		const art_u8 *src,
		gint src_width, gint src_height, gint src_rowstride,
		const gdouble affine[6],
		ArtFilterLevel level,
		ArtAlphaGamma *alphagamma)
{
  /* Note: this is a slow implementation, and is missing all filter
     levels other than NEAREST. It is here for clarity of presentation
     and to establish the interface. */
  gint x, y;
  gdouble inv[6];
  art_u8 *dst_p, *dst_linestart;
  const art_u8 *src_p;
  ArtPoint pt, src_pt;
  gint src_x, src_y;
  gint run_x0, run_x1;

  dst_linestart = dst;
  art_affine_invert (inv, affine);
  for (y = y0; y < y1; y++)
    {
      pt.y = y + 0.5;
      run_x0 = x0;
      run_x1 = x1;
      art_rgb_affine_run (&run_x0, &run_x1, y, src_width, src_height,
			  inv);
      dst_p = dst_linestart + (run_x0 - x0) * 3;
      for (x = run_x0; x < run_x1; x++)
	{
	  pt.x = x + 0.5;
	  art_affine_point (&src_pt, &pt, inv);
	  src_x = floor (src_pt.x);
	  src_y = floor (src_pt.y);
	  src_p = src + (src_y * src_rowstride) + src_x * 3;
	  dst_p[0] = src_p[0];
	  dst_p[1] = src_p[1];
	  dst_p[2] = src_p[2];
	  dst_p += 3;
	}
      dst_linestart += dst_rowstride;
    }
}
