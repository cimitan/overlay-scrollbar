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
 * This test only creates an overlay scrollbar, subclass of GtkWindow,
 * to test g_object creation.
 */

#include <gtk/gtk.h>

#include <overlay-scrollbar.h>

static void window_destroy_cb (GtkWidget *widget,
                               gpointer   user_data);

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
  GtkWidget *scrolled_window_text0, *scrolled_window_text1;
  GtkWidget *vbox;
  GtkWidget *text_view0, *text_view1;
  GtkTextBuffer *text_buffer0, *text_buffer1;
  GtkWidget *window;
  GtkWidget *vscrollbar0, *vscrollbar1;
  GtkWidget *overlay_scrollbar0, *overlay_scrollbar1;

  gtk_init (&argc, &argv);

  /* window */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 400, 600);
  gtk_window_set_title (GTK_WINDOW (window), "GtkScrolledWindow \"draw\" test");

  /* vbox */
  vbox = gtk_vbox_new (TRUE, 10);

  /* scrolled_window_text0 */
  scrolled_window_text0 = gtk_scrolled_window_new (NULL, NULL);

  /* text_view0 */
  text_view0 = gtk_text_view_new ();
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_window_text0), text_view0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window_text0), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  /* text_buffer0 */
  text_buffer0 = gtk_text_view_get_buffer(GTK_TEXT_VIEW (text_view0));
  gtk_text_buffer_set_text (text_buffer0, "Ubuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock", -1);

  /* overlar_scrollbar0 */
  vscrollbar0 = gtk_scrolled_window_get_vscrollbar (GTK_SCROLLED_WINDOW (scrolled_window_text0));
  overlay_scrollbar0 = overlay_scrollbar_new (GTK_RANGE (vscrollbar0));

  /* scrolled_window_text1 */
  scrolled_window_text1 = gtk_scrolled_window_new (NULL, NULL);

  /* text_view1 */
  text_view1 = gtk_text_view_new ();
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_window_text1), text_view1);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window_text1), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  /* text_buffer1 */
  text_buffer1 = gtk_text_view_get_buffer(GTK_TEXT_VIEW (text_view1));
  gtk_text_buffer_set_text (text_buffer1, "Ubuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock\nUbuntu is gonna rock", -1);

  /* overlar_scrollbar0 */
  vscrollbar1 = gtk_scrolled_window_get_vscrollbar (GTK_SCROLLED_WINDOW (scrolled_window_text1));
  overlay_scrollbar1 = overlay_scrollbar_new (GTK_RANGE (vscrollbar1));

  /* containers */
  gtk_container_set_border_width (GTK_CONTAINER (window), 10);
  gtk_container_add (GTK_CONTAINER (vbox), scrolled_window_text0);
  gtk_container_add (GTK_CONTAINER (vbox), scrolled_window_text1);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  /* signals */
  g_signal_connect (G_OBJECT (window), "destroy",
                    G_CALLBACK (window_destroy_cb), NULL);

  gtk_widget_show_all (window);

  gtk_widget_show (overlay_scrollbar0);
  gtk_widget_show (overlay_scrollbar1);

  gtk_main ();

  return 0;
}
