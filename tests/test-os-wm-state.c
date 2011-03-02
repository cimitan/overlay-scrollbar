/* overlay-scrollbar
 *
 * Copyright © 2011 Canonical Ltd
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 *
 * Authored by Andrea Cimitan <andrea.cimitan@canonical.com>
 */

/* 
 * This test checks for the focused window
 */

#include <gtk/gtk.h>

static gboolean window_property_notify_event_cb (GtkWidget        *widget,
                                                 GdkEventProperty *event,
                                                 gpointer          user_data);

static void window_destroy_cb (GtkWidget *widget,
                               gpointer   user_data);

/**
 * window_property_notify_event_cb 
 * property_notify_event callback for window
 **/
static gboolean
window_property_notify_event_cb (GtkWidget        *widget,
                                 GdkEventProperty *event,
                                 gpointer          user_data)
{
  GdkScreen *screen;
  GdkWindow *active_window;
  GtkWidget *label;
  gchar *atom_name;

  atom_name = gdk_atom_name (event->atom);

  label = GTK_WIDGET (user_data);

  printf ("%s\n", atom_name);

  screen = gtk_widget_get_screen (widget);

  active_window = gdk_screen_get_active_window (screen);

/*  printf ("%s\n", gdk_atom_name (event->atom));*/

/*gtk_label_set_text (GTK_LABEL (label), atom_name);*/

  if (active_window == gtk_widget_get_window (widget))
    gtk_label_set_text (GTK_LABEL (label), "Window is focused");
  else
    gtk_label_set_text (GTK_LABEL (label), "Window is unfocused");

  return FALSE;
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
  GtkWidget *window;
  GtkWidget *label;

  gtk_init (&argc, &argv);

  /* window */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_has_resize_grip (GTK_WINDOW (window), FALSE);
  gtk_window_set_default_size (GTK_WINDOW (window), 160, 160);
  gtk_window_set_title (GTK_WINDOW (window), "Test PropertyNotify");
  gtk_widget_add_events (window, GDK_PROPERTY_CHANGE_MASK);

  /* label */
  label = gtk_label_new ("Window State");

  gtk_container_add (GTK_CONTAINER (window), label);

  gtk_widget_show_all (window);

  /* signals */
  g_signal_connect (G_OBJECT (window), "property-notify-event",
                    G_CALLBACK (window_property_notify_event_cb), label);
  g_signal_connect (G_OBJECT (window), "destroy",
                    G_CALLBACK (window_destroy_cb), NULL);
  gtk_main ();

  return 0;
}
