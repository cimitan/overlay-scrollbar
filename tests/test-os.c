/* overlay-scrollbar
 *
 * Copyright © 2011 Canonical Ltd
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
 */

/*
 * This test only creates an overlay scrollbar, subclass of GtkWindow,
 * to test g_object creation.
 */

#include <os/os.h>

static char text0[] = "Ubuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!";

static char text1[] = "Ubuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!\n\
Ubuntu is gonna rock!\nUbuntu is gonna rock!\nUbuntu is gonna rock!";

typedef struct
{
  const gboolean  check;
  const gchar    *description;
}
List;

enum
{
  COLUMN_CHECK,
  COLUMN_DESCRIPTION,
  COLUMN_ACTIVE,
  NUM_COLUMNS
};

static List data[] =
{
  { FALSE, "Ubuntu really rocks." },
  { TRUE,  "Ubuntu will rock soon!" },
  { FALSE, "Nokia and Microsoft are gonna rock." },
  { TRUE,  "Cimi needs a vacation!" },
  { FALSE, "I prefer rain to sunshine." },
  { TRUE,  "I wonna flight with Mark's jet." },
  { FALSE, "Gtk+ 2.0 and X11 rocks." },
  { FALSE, "Nokia won't kill Qt with this move." },
  { TRUE,  "Can't wait for the sun to ride my bike." },
  { TRUE,  "UDS in Florida was awesome!" },
  { FALSE, "I'm not bored at all of writing there." },
  { TRUE,  "A developer should be tanned." },
  { FALSE, "Firefox is faster than Chromium." },
  { TRUE, "Please Cimi, stop writing!" },
};

static GtkTreeModel* model_create (void);

static void renderer_check_toggled_cb (GtkCellRendererToggle *cell,
                                       gchar                 *path_str,
                                       gpointer               user_data);

static void tree_view_add_columns (GtkTreeView *treeview);

static void window_destroy_cb (GtkWidget *widget,
                               gpointer   user_data);

/**
 * model_create:
 * create GtkTreeModel
 **/
static GtkTreeModel*
model_create (void)
{
  gint i = 0;
  gint length;
  GtkListStore *store;
  GtkTreeIter iter;

  /* create list store */
  store = gtk_list_store_new (NUM_COLUMNS,
                              G_TYPE_BOOLEAN,
                              G_TYPE_STRING,
                              G_TYPE_BOOLEAN);

  /* add data to the list store */
  length = (gint) G_N_ELEMENTS (data);
  for (i = 0; i < length; i++)
    {
      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter,
                          COLUMN_CHECK, data[i].check,
                          COLUMN_DESCRIPTION, data[i].description,
                          COLUMN_ACTIVE, FALSE,
                          -1);
    }

  return GTK_TREE_MODEL (store);
}

/**
 * renderer_check_toggled_cb:
 * callback for "toggled" signal
 **/
static void
renderer_check_toggled_cb (GtkCellRendererToggle *cell,
                           gchar                 *path_str,
                           gpointer               user_data)
{
  GtkTreeModel *model = (GtkTreeModel*)user_data;
  GtkTreeIter  iter;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
  gboolean check;

  /* get toggled iter */
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, COLUMN_CHECK, &check, -1);

  /* do something with the value */
  check ^= 1;

  /* set new value */
  gtk_list_store_set (GTK_LIST_STORE (model), &iter, COLUMN_CHECK, check, -1);

  /* clean up */
  gtk_tree_path_free (path);
}

/**
 * tree_view_add_columns:
 * add columns to the GtkTreeView
 **/
static void
tree_view_add_columns (GtkTreeView *treeview)
{
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GtkTreeModel *model = gtk_tree_view_get_model (treeview);

  /* column for fixed toggles */
  renderer = gtk_cell_renderer_toggle_new ();
  g_signal_connect (renderer, "toggled",
                    G_CALLBACK (renderer_check_toggled_cb), model);

  column = gtk_tree_view_column_new_with_attributes ("True?",
                                                     renderer,
                                                     "active", COLUMN_CHECK,
                                                     NULL);

  /* set this column to a fixed sizing (of 50 pixels) */
  gtk_tree_view_column_set_sizing (GTK_TREE_VIEW_COLUMN (column),
                                   GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_column_set_fixed_width (GTK_TREE_VIEW_COLUMN (column), 50);
  gtk_tree_view_append_column (treeview, column);

  /* column for description */
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Description",
                                                     renderer,
                                                     "text",
                                                     COLUMN_DESCRIPTION,
                                                     NULL);
  gtk_tree_view_column_set_sort_column_id (column, COLUMN_DESCRIPTION);
  gtk_tree_view_append_column (treeview, column);
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
  GtkWidget *scrolled_window_text0, *scrolled_window_text1, *scrolled_window_tree_view;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *text_view0, *text_view1;
  GtkWidget *tree_view;
  GtkTreeModel *model;
  GtkTextBuffer *text_buffer0, *text_buffer1;
  GtkWidget *window;
/*  GtkWidget *vscrollbar0, *vscrollbar1, *vscrollbar2;*/
  /* GtkWidget *overlay_scrollbar0, *overlay_scrollbar1, *overlay_scrollbar2; */

  gtk_init (&argc, &argv);

  /* window */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 400, 500);
  gtk_window_set_title (GTK_WINDOW (window), "Vertical \"Overlay Scrollbar\" test");

  /* vbox */
  vbox = gtk_vbox_new (TRUE, 2);

  /* hbox */
  hbox = gtk_hbox_new (TRUE, 2);

  /* scrolled_window_text0 */
  scrolled_window_text0 = gtk_scrolled_window_new (NULL, NULL);

  /* text_view0 */
  text_view0 = gtk_text_view_new ();
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_window_text0), text_view0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window_text0), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  /* text_buffer0 */
  text_buffer0 = gtk_text_view_get_buffer(GTK_TEXT_VIEW (text_view0));
  gtk_text_buffer_set_text (text_buffer0, text0, -1);

  /* overlar_scrollbar0 */
/*  vscrollbar0 = gtk_scrolled_window_get_vscrollbar (GTK_SCROLLED_WINDOW (scrolled_window_text0));*/
/*  overlay_scrollbar0 = overlay_scrollbar_new (GTK_ORIENTATION_VERTICAL,*/
/*                                              gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scrolled_window_text0)));*/
/*  gtk_widget_set_parent (overlay_scrollbar0, scrolled_window_text0);*/

  /* scrolled_window_text1 */
  scrolled_window_text1 = gtk_scrolled_window_new (NULL, NULL);

  /* text_view1 */
  text_view1 = gtk_text_view_new ();
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_window_text1), text_view1);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window_text1), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  /* text_buffer1 */
  text_buffer1 = gtk_text_view_get_buffer(GTK_TEXT_VIEW (text_view1));
  gtk_text_buffer_set_text (text_buffer1, text1, -1);

  /* overlar_scrollbar1 */
/*  vscrollbar1 = gtk_scrolled_window_get_vscrollbar (GTK_SCROLLED_WINDOW (scrolled_window_text1));*/
/*  overlay_scrollbar1 = overlay_scrollbar_new (GTK_ORIENTATION_VERTICAL,*/
/*                                              gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scrolled_window_text1)));*/
/*  gtk_widget_set_parent (overlay_scrollbar1, scrolled_window_text1);*/

  /* model */
  model = model_create ();

  /* tree_view */
  tree_view = gtk_tree_view_new_with_model (model);
  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (tree_view), TRUE);
  gtk_tree_view_set_search_column (GTK_TREE_VIEW (tree_view),
                                   COLUMN_DESCRIPTION);
  tree_view_add_columns (GTK_TREE_VIEW (tree_view));

  g_object_unref (model);

  /* scrolled_window_tree_view */
  scrolled_window_tree_view = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window_tree_view),
                                       GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window_tree_view),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);
  gtk_container_add (GTK_CONTAINER (scrolled_window_tree_view), tree_view);

  /* overlar_scrollbar2 */
/*  vscrollbar2 = gtk_scrolled_window_get_vscrollbar (GTK_SCROLLED_WINDOW (scrolled_window_tree_view));*/
/*  overlay_scrollbar2 = overlay_scrollbar_new (GTK_ORIENTATION_VERTICAL,*/
/*                                              gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scrolled_window_tree_view)));*/
/*  gtk_widget_set_parent (overlay_scrollbar2, scrolled_window_tree_view);*/

  /* containers */
  gtk_container_set_border_width (GTK_CONTAINER (window), 2);
  gtk_container_add (GTK_CONTAINER (hbox), scrolled_window_text0);
  gtk_container_add (GTK_CONTAINER (hbox), scrolled_window_text1);
  gtk_container_add (GTK_CONTAINER (vbox), hbox);
  gtk_container_add (GTK_CONTAINER (vbox), scrolled_window_tree_view);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  /* signals */
  g_signal_connect (G_OBJECT (window), "destroy",
                    G_CALLBACK (window_destroy_cb), NULL);

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
