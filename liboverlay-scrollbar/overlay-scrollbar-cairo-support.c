/* liboverlay-scrollbar
 * Copyright (C) 2011 Canonical Ltd
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
 *
 * Authored by Andrea Cimitan <andrea.cimitan@canonical.com>
 *
 */

#include <cairo.h>

void
os_cairo_draw_rounded_rect (cairo_t *cr,
                            gdouble x,
                            gdouble y,
                            gdouble width,
                            gdouble height,
                            gdouble radius)
{
  if (radius < 1)
    {
      cairo_rectangle (cr, x, y, width, height);
      return;
    }

  radius = MIN (radius, MIN (width/2.0, height/2.0));

  cairo_move_to (cr, x+radius, y);

  cairo_arc (cr, x+width-radius, y+radius, radius, G_PI*1.5, G_PI*2);
  cairo_arc (cr, x+width-radius, y+height-radius, radius, 0, G_PI*0.5);
  cairo_arc (cr, x+radius, y+height-radius, radius, G_PI*0.5, G_PI);
  cairo_arc (cr, x+radius, y+radius, radius, G_PI, G_PI*1.5);
}
