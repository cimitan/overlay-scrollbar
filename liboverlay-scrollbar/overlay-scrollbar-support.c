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
#include <gdk/gdkx.h>
#include <X11/X.h>
#include <X11/Xlib.h>

#include "overlay-scrollbar-support.h"

/**
 * os_clamp_dimensions:
 * clamp the dimensions of the GtkRange
 **/
void
os_clamp_dimensions (GtkWidget    *widget,
                     GdkRectangle *rect,
                     GtkBorder    *border,
                     gboolean      border_expands_horizontally)
{
  gint extra, shortage;

  g_return_if_fail (rect->x == 0);
  g_return_if_fail (rect->y == 0);  
  g_return_if_fail (rect->width >= 0);
  g_return_if_fail (rect->height >= 0);

  /* Width */

  extra = widget->allocation.width - border->left - border->right - rect->width;
  if (extra > 0)
    {
      if (border_expands_horizontally)
        {
          border->left += extra / 2;
          border->right += extra / 2 + extra % 2;
        }
      else
        {
          rect->width += extra;
        }
    }

  /* See if we can fit rect, if not kill the border */
  shortage = rect->width - widget->allocation.width;
  if (shortage > 0)
    {
      rect->width = widget->allocation.width;
      /* lose the border */
      border->left = 0;
      border->right = 0;
    }
  else
    {
      /* See if we can fit rect with borders */
      shortage = rect->width + border->left + border->right -  widget->allocation.width;
      if (shortage > 0)
        {
          /* Shrink borders */
          border->left -= shortage / 2;
          border->right -= shortage / 2 + shortage % 2;
        }
    }

  /* Height */

  extra = widget->allocation.height - border->top - border->bottom - rect->height;
  if (extra > 0)
    {
      if (border_expands_horizontally)
        {
          /* don't expand border vertically */
          rect->height += extra;
        }
      else
        {
          border->top += extra / 2;
          border->bottom += extra / 2 + extra % 2;
        }
    }

  /* See if we can fit rect, if not kill the border */
  shortage = rect->height - widget->allocation.height;
  if (shortage > 0)
    {
      rect->height = widget->allocation.height;
      /* lose the border */
      border->top = 0;
      border->bottom = 0;
    }
  else
    {
      /* See if we can fit rect with borders */
      shortage = rect->height + border->top + border->bottom - widget->allocation.height;
      if (shortage > 0)
        {
          /* Shrink borders */
          border->top -= shortage / 2;
          border->bottom -= shortage / 2 + shortage % 2;
        }
    }
}

/**
 * os_gtk_range_calc_request:
 * calculate GtkRange range_rect
 **/
void
os_gtk_range_calc_request (GtkRange      *range,
                           gint           slider_width,
                           gint           stepper_size,
                           gint           focus_width,
                           gint           trough_border,
                           gint           stepper_spacing,
                           GdkRectangle  *range_rect,
                           GtkBorder     *border,
                           gint          *n_steppers_p,
                           gboolean      *has_steppers_ab,
                           gboolean      *has_steppers_cd,
                           gint          *slider_length_p)
{
  gint slider_length;
  gint n_steppers;
  gint n_steppers_ab;
  gint n_steppers_cd;

  border->left = 0;
  border->right = 0;
  border->top = 0;
  border->bottom = 0;

  if (GTK_RANGE_GET_CLASS (range)->get_range_border)
    GTK_RANGE_GET_CLASS (range)->get_range_border (range, border);

  n_steppers_ab = 0;
  n_steppers_cd = 0;

  if (range->has_stepper_a)
    n_steppers_ab += 1;
  if (range->has_stepper_b)
    n_steppers_ab += 1;
  if (range->has_stepper_c)
    n_steppers_cd += 1;
  if (range->has_stepper_d)
    n_steppers_cd += 1;

  n_steppers = n_steppers_ab + n_steppers_cd;

  slider_length = range->min_slider_size;

  range_rect->x = 0;
  range_rect->y = 0;

  /* We never expand to fill available space in the small dimension
   * (i.e. vertical scrollbars are always a fixed width)
   */
  if (range->orientation == GTK_ORIENTATION_VERTICAL)
    {
      range_rect->width = (focus_width + trough_border) * 2 + slider_width;
      range_rect->height = stepper_size*n_steppers + (focus_width + trough_border) * 2 + slider_length;

      if (n_steppers_ab > 0)
        range_rect->height += stepper_spacing;

      if (n_steppers_cd > 0)
        range_rect->height += stepper_spacing;
    }
  else
    {
      range_rect->width = stepper_size*n_steppers + (focus_width + trough_border) * 2 + slider_length;
      range_rect->height = (focus_width + trough_border) * 2 + slider_width;

      if (n_steppers_ab > 0)
        range_rect->width += stepper_spacing;

      if (n_steppers_cd > 0)
        range_rect->width += stepper_spacing;
    }

  if (n_steppers_p)
    *n_steppers_p = n_steppers;

  if (has_steppers_ab)
    *has_steppers_ab = (n_steppers_ab > 0);

  if (has_steppers_cd)
    *has_steppers_cd = (n_steppers_cd > 0);

  if (slider_length_p)
    *slider_length_p = slider_length;
}

/**
 * os_gtk_range_get_props:
 * get GtkRange properties
 **/
void
os_gtk_range_get_props (GtkRange  *range,
                        gint      *slider_width,
                        gint      *stepper_size,
                        gint      *focus_width,
                        gint      *trough_border,
                        gint      *stepper_spacing,
                        gboolean  *trough_under_steppers,
                        gint      *arrow_displacement_x,
                        gint      *arrow_displacement_y)
{
  GtkWidget *widget =  GTK_WIDGET (range);
  gint tmp_slider_width, tmp_stepper_size, tmp_focus_width, tmp_trough_border;
  gint tmp_stepper_spacing, tmp_trough_under_steppers;
  gint tmp_arrow_displacement_x, tmp_arrow_displacement_y;

  gtk_widget_style_get (widget,
                        "slider-width", &tmp_slider_width,
                        "trough-border", &tmp_trough_border,
                        "stepper-size", &tmp_stepper_size,
                        "stepper-spacing", &tmp_stepper_spacing,
                        "trough-under-steppers", &tmp_trough_under_steppers,
                        "arrow-displacement-x", &tmp_arrow_displacement_x,
                        "arrow-displacement-y", &tmp_arrow_displacement_y,
                        NULL);

  if (tmp_stepper_spacing > 0)
    tmp_trough_under_steppers = FALSE;

  if (gtk_widget_get_can_focus (GTK_WIDGET (range)))
    {
      gint focus_line_width;
      gint focus_padding;

      gtk_widget_style_get (GTK_WIDGET (range),
                            "focus-line-width", &focus_line_width,
                            "focus-padding", &focus_padding,
                            NULL);

      tmp_focus_width = focus_line_width + focus_padding;
    }
  else
    {
      tmp_focus_width = 0;
    }
  
  if (slider_width)
    *slider_width = tmp_slider_width;

  if (focus_width)
    *focus_width = tmp_focus_width;

  if (trough_border)
    *trough_border = tmp_trough_border;

  if (stepper_size)
    *stepper_size = tmp_stepper_size;

  if (stepper_spacing)
    *stepper_spacing = tmp_stepper_spacing;

  if (trough_under_steppers)
    *trough_under_steppers = tmp_trough_under_steppers;

  if (arrow_displacement_x)
    *arrow_displacement_x = tmp_arrow_displacement_x;

  if (arrow_displacement_y)
    *arrow_displacement_y = tmp_arrow_displacement_y;
}

/**
 * os_present_gdk_window_with_timestamp:
 * present a GdkWindow
 **/
void
os_present_gdk_window_with_timestamp (GtkWidget *widget,
                                      guint32    timestamp)
{
  os_present_window_with_timestamp (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                                    GDK_SCREEN_XSCREEN (gtk_widget_get_screen (widget)),
                                    GDK_WINDOW_XID (gtk_widget_get_window (widget)),
                                    timestamp);
}

/**
 * os_present_window_with_timestamp:
 * present a Window using its xid
 **/
void
os_present_window_with_timestamp (Display *default_display,
                                  Screen  *screen,
                                  gint     xid,
                                  guint32  timestamp)
{
  Display *display;
  Window root;
  XEvent xev;

  if (timestamp == 0)
    g_warning ("Received a timestamp of 0; window activation may not "
               "function properly.\n");

  display = DisplayOfScreen (screen);
  root = RootWindowOfScreen (screen);

  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = display;
  xev.xclient.window = xid;
  xev.xclient.message_type = gdk_x11_get_xatom_by_name ("_NET_ACTIVE_WINDOW");
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = 1;
  xev.xclient.data.l[1] = timestamp;
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;

  gdk_error_trap_push ();
  XSendEvent (display,
              root,
              False,
              SubstructureRedirectMask | SubstructureNotifyMask,
              &xev);
  XSync (default_display, False);
  gdk_error_trap_pop ();
}
