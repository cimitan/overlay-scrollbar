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

#include <gtk/gtk.h>

#include "overlay-scrollbar.h"

G_DEFINE_TYPE (OverlayScrollbar, overlay_scrollbar, GTK_TYPE_WINDOW);

static gboolean overlay_scrollbar_expose (GtkWidget *widget,
                                          GdkEventExpose *event);
static void overlay_scrollbar_map (GtkWidget *widget);

static void
overlay_scrollbar_class_init (OverlayScrollbarClass *class)
{
  GtkWidgetClass *widget_class;

  widget_class = GTK_WIDGET_CLASS (class);

  widget_class->expose_event = overlay_scrollbar_expose;
  widget_class->map          = overlay_scrollbar_map;
}

static gboolean
overlay_scrollbar_expose (GtkWidget *widget,
                          GdkEventExpose *event)
{
  GtkAllocation allocation;
  GtkStateType state_type_down, state_type_up;
  cairo_pattern_t *pat;
  cairo_t *cr;
  gint x, y, width, height;

  state_type_down = GTK_STATE_NORMAL;
  state_type_up = GTK_STATE_NORMAL;

  gtk_widget_get_allocation (widget, &allocation);

  x = 0;
  y = 0;
  width = allocation.width;
  height = allocation.height;

  cr = gdk_cairo_create (gtk_widget_get_window (widget));

  /* clear the background */
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba (cr, 0, 0, 0, 0);
  cairo_paint (cr);

  /* drawing code */
  cairo_save (cr);
  cairo_translate (cr, 0.5, 0.5);
  width--;
  height--;

  cairo_set_line_width (cr, 1);
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

  os_draw_rounded_rectangle (cr, x, y, width, height, 6);
  pat = cairo_pattern_create_linear (x, y, width+x, y);
  cairo_pattern_add_color_stop_rgba (pat, 0.0, 0.95, 0.95, 0.95, 1.0);
  cairo_pattern_add_color_stop_rgba (pat, 1.0, 0.8, 0.8, 0.8, 1.0);
  cairo_set_source (cr, pat);
  cairo_pattern_destroy (pat);
  cairo_fill (cr);

  os_draw_rounded_rectangle (cr, x, y, width, height, 6);
  pat = cairo_pattern_create_linear (x, y, x, height+y);
/*  if ((handler_data != NULL && pointer_data != NULL) &&*/
/*      (handler->button_press_event && !handler->motion_notify_event))*/
/*    {*/
/*      if (pointer->y < height/2)*/
/*        {*/
/*          state_type_up = GTK_STATE_ACTIVE;*/
/*          cairo_pattern_add_color_stop_rgba (pat, 0.0, 0.8, 0.8, 0.8, 0.8);*/
/*          cairo_pattern_add_color_stop_rgba (pat, 0.49, 1.0, 1.0, 1.0, 0.0);*/
/*          cairo_pattern_add_color_stop_rgba (pat, 0.49, 0.8, 0.8, 0.8, 0.5);*/
/*          cairo_pattern_add_color_stop_rgba (pat, 1.0, 0.8, 0.8, 0.8, 0.8);*/
/*        }*/
/*      else*/
/*        {*/
/*          state_type_down = GTK_STATE_ACTIVE;*/
/*          cairo_pattern_add_color_stop_rgba (pat, 0.0, 1.0, 1.0, 1.0, 0.8);*/
/*          cairo_pattern_add_color_stop_rgba (pat, 0.49, 1.0, 1.0, 1.0, 0.0);*/
/*          cairo_pattern_add_color_stop_rgba (pat, 0.49, 0.8, 0.8, 0.8, 0.5);*/
/*          cairo_pattern_add_color_stop_rgba (pat, 1.0, 0.6, 0.6, 0.6, 0.8);*/
/*        }*/
/*    }*/
/*  else*/
    {
      cairo_pattern_add_color_stop_rgba (pat, 0.0, 1.0, 1.0, 1.0, 0.8);
      cairo_pattern_add_color_stop_rgba (pat, 0.49, 1.0, 1.0, 1.0, 0.0);
      cairo_pattern_add_color_stop_rgba (pat, 0.49, 0.8, 0.8, 0.8, 0.5);
      cairo_pattern_add_color_stop_rgba (pat, 1.0, 0.8, 0.8, 0.8, 0.8);
    }
  cairo_set_source (cr, pat);
  cairo_pattern_destroy (pat);
  cairo_fill_preserve (cr);

  cairo_set_source_rgba (cr, 0.6, 0.6, 0.6, 1.0);
  cairo_stroke (cr);

  os_draw_rounded_rectangle (cr, x+1, y+1, width-2, height-2, 7);
  cairo_set_source_rgba (cr, 1, 1, 1, 0.5);
  cairo_stroke (cr);

  cairo_move_to (cr, x+0.5, y-1+height/2);
  cairo_line_to (cr, width-0.5, y-1+height/2);
  cairo_set_source_rgba (cr, 0.6, 0.6, 0.6, 0.4);
  cairo_stroke (cr);

  cairo_move_to (cr, x+0.5, y+height/2);
  cairo_line_to (cr, width-0.5, y+height/2);
  cairo_set_source_rgba (cr, 1, 1, 1, 0.5);
  cairo_stroke (cr);

  cairo_restore (cr);

/*  gtk_paint_arrow (gtk_widget_get_style (widget),*/
/*                   cr,*/
/*                   state_type_up,*/
/*                   GTK_SHADOW_IN,*/
/*                   widget,*/
/*                   "arrow",*/
/*                   GTK_ARROW_UP,*/
/*                   FALSE,*/
/*                   4,*/
/*                   4,*/
/*                   width-8,*/
/*                   width-8);*/

/*  gtk_paint_arrow (gtk_widget_get_style (widget),*/
/*                   cr,*/
/*                   state_type_down,*/
/*                   GTK_SHADOW_NONE,*/
/*                   widget,*/
/*                   "arrow",*/
/*                   GTK_ARROW_DOWN,*/
/*                   FALSE,*/
/*                   4,*/
/*                   height-(width-8)-4,*/
/*                   width-8,*/
/*                   width-8);*/

  cairo_destroy (cr);

  return FALSE;
}

static void
overlay_scrollbar_init (OverlayScrollbar *scrollbar)
{
}

static void
overlay_scrollbar_map (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (overlay_scrollbar_parent_class)->map (widget);
}

/**
 * overlay_scrollbar_new:
 *
 * Creates a new overlay scrollbar.
 *
 * Returns: the new overlay scrollbar as a #GtkWidget
 */
GtkWidget*
overlay_scrollbar_new (void)
{
  return g_object_new (OS_TYPE_OVERLAY_SCROLLBAR, NULL);
}

