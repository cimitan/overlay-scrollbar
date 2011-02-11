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

static void os_cairo_draw_rounded_rect (cairo_t *cr,
                                        gdouble  x,
                                        gdouble  y,
                                        gdouble  width,
                                        gdouble  height,
                                        gdouble  radius);

static void pixmap_draw (GdkPixmap *pixmap);

static void window_destroy_cb (GtkWidget *widget,
                               gpointer   user_data);

/**
 * os_cairo_draw_rounded_rect:
 * draw a rounded rectangle
 **/
static void
os_cairo_draw_rounded_rect (cairo_t *cr,
                            gdouble  x,
                            gdouble  y,
                            gdouble  width,
                            gdouble  height,
                            gdouble  radius)
{
  if (radius < 1)
    {
      cairo_rectangle (cr, x, y, width, height);
      return;
    }

  radius = MIN (radius, MIN (width / 2.0, height / 2.0));

  cairo_move_to (cr, x + radius, y);

  cairo_arc (cr, x + width - radius, y + radius, radius, G_PI * 1.5, G_PI * 2);
  cairo_arc (cr, x + width - radius, y + height - radius, radius, 0, G_PI * 0.5);
  cairo_arc (cr, x + radius, y + height - radius, radius, G_PI * 0.5, G_PI);
  cairo_arc (cr, x + radius, y + radius, radius, G_PI, G_PI * 1.5);
}

/**
 * pixmap_draw:
 * draw on the pixmap
 **/
static void
pixmap_draw (GdkPixmap *pixmap)
{
  cairo_t *cr_surface;
  cairo_surface_t *surface;
  cairo_status_t status;
  gint width, height;

  gdk_pixmap_get_size (pixmap, &width, &height);

  surface = cairo_xlib_surface_create (GDK_DRAWABLE_XDISPLAY (pixmap), gdk_x11_drawable_get_xid (pixmap),
                                       GDK_VISUAL_XVISUAL (gdk_drawable_get_visual (pixmap)), width, height);

  cr_surface = cairo_create (surface);

  cairo_set_operator (cr_surface, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba (cr_surface, 1.0, 1.0, 1.0, 1.0);
  cairo_paint (cr_surface);

  cairo_set_operator (cr_surface, CAIRO_OPERATOR_OVER);
  os_cairo_draw_rounded_rect (cr_surface, 1, 1, width - 2, height - 2, 20);
  cairo_set_source_rgba (cr_surface, 1.0, 0.6, 0.6, 0.8);
  cairo_fill_preserve (cr_surface);

  cairo_set_line_width (cr_surface, 2.0);
  cairo_set_source_rgba (cr_surface, 0.8, 0.4, 0.4, 1.0);
  cairo_stroke (cr_surface);

  status = cairo_surface_write_to_png (surface, "/tmp/overlay-scrollbar-test-pixmap.png");

  if (status == CAIRO_STATUS_SUCCESS)
    printf ("cairo_surface_t written to /tmp/overlay-scrollbar-test-pixmap.png\n");
  else
    DEBUG (cairo_status_to_string (status));

  cairo_destroy (cr_surface);
}

/**
 * window_destroy_cb:
 * destroy callback for window
 **/
static void
window_destroy_cb (GtkWidget *widget,
                   gpointer   user_data)
{
  gtk_main_quit ();
}

/**
 * main:
 * main routine
 **/
int
main (int   argc,
      char *argv[])
{
  GtkAllocation allocation;
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *frame;
  GtkWidget *frame_window;
  GtkWidget *image;
  GdkPixmap *pixmap;
  GdkWindowAttr attributes;
  GdkWindow *child_window;
  gint attributes_mask;

  gtk_init (&argc, &argv);

  /* pixmap */
  pixmap = gdk_pixmap_new (NULL, 120, 100, 24);
  pixmap_draw (pixmap);

  /* window */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_has_resize_grip (GTK_WINDOW (window), FALSE);
  gtk_window_set_default_size (GTK_WINDOW (window), 160, 300);
  gtk_window_set_title (GTK_WINDOW (window), "Test GdkDrawable and GdkPixmap");

  /* vbox */
  vbox = gtk_vbox_new (TRUE, 2);

  /* frame */
  frame = gtk_frame_new ("GdkPixmap");

  /* frame_window */
  frame_window = gtk_frame_new ("GDK_WINDOW_CHILD");

  /* image */
  image = gtk_image_new_from_pixmap (pixmap, NULL);

  /* containers */
  gtk_container_set_border_width (GTK_CONTAINER (window), 10);
  gtk_container_add (GTK_CONTAINER (frame), image);
  gtk_container_add (GTK_CONTAINER (vbox), frame);
  gtk_container_add (GTK_CONTAINER (vbox), frame_window);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  /* signals */
  g_signal_connect (G_OBJECT (window), "destroy",
                    G_CALLBACK (window_destroy_cb), NULL);

  gtk_widget_show_all (window);

  gtk_widget_get_allocation (frame_window, &allocation);

  /* child_window */
  attributes.x = allocation.x+14;
  attributes.y = allocation.y+20;
  attributes.width = 120;
  attributes.height = 100;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes_mask = GDK_WA_X | GDK_WA_Y;
  child_window = gdk_window_new (gtk_widget_get_window (window), &attributes, attributes_mask);

  gdk_window_set_back_pixmap (child_window, pixmap, FALSE);
  gdk_window_show (child_window);

  gtk_main ();
}
