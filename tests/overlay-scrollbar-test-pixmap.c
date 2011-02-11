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

/* 
 * This test only creates a GdkPixmap
 */

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <cairo.h>
#include <cairo-xlib.h>

#define DEBUG(msg) \
        do { fprintf (stderr, "DEBUG: %s\n", msg); } while (0)

/**
 * main:
 * main routine
 **/
int
main (int   argc,
      char *argv[])
{
  GdkPixmap *pixmap;
  cairo_t *cr_surface;
  cairo_surface_t *surface;
  cairo_status_t status;
  gint width, height;

  width = 100;
  height = 100;

  gtk_init (&argc, &argv);

  pixmap = gdk_pixmap_new (NULL, width, height, 24);

  surface = cairo_xlib_surface_create (GDK_DRAWABLE_XDISPLAY (pixmap), gdk_x11_drawable_get_xid (pixmap),
                                       GDK_VISUAL_XVISUAL (gdk_drawable_get_visual (pixmap)), width, height);

  cr_surface = cairo_create (surface);

  cairo_rectangle (cr_surface, 10, 10, width - 20, height - 20);
  cairo_set_source_rgba (cr_surface, 1.0, 0.6, 0.6, 0.5);
  cairo_fill_preserve (cr_surface);

  cairo_set_line_width (cr_surface, 2.0);
  cairo_set_source_rgba (cr_surface, 0.5, 0.3, 0.3, 1.0);
  cairo_stroke (cr_surface);

  status = cairo_surface_write_to_png (surface, "/tmp/overlay-scrollbar-test-pixmap.png");

  if (status == CAIRO_STATUS_SUCCESS)
    printf ("cairo_surface_t written to /tmp/overlay-scrollbar-test-pixmap.png\n");
  else
    DEBUG(cairo_status_to_string (status));

  cairo_destroy (cr_surface);
}
