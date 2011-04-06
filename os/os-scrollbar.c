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

#ifndef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include "os-scrollbar.h"
#include "os-private.h"
#include <gdk/gdkx.h>
#include "math.h"

/* Default size of the pager in pixels. */
#define DEFAULT_PAGER_WIDTH 3

/* Width of the proximity effect in pixels. */
#define PROXIMITY_WIDTH 30

/* Timeout before hiding in ms, after leaving the thumb. */
#define TIMEOUT_THUMB_HIDE 1000

/* Timeout before hiding in ms, after leaving the toplevel. */
#define TIMEOUT_TOPLEVEL_HIDE 200

struct _OsScrollbarPrivate
{
  GdkRectangle trough;
  GdkRectangle overlay;
  GdkRectangle slider;
  GtkAllocation overlay_all;
  GtkAllocation thumb_all;
  GObject *pager;
  GtkWidget *thumb;
  GtkAdjustment *adjustment;
  GtkOrientation orientation;
  gboolean button_press_event;
  gboolean enter_notify_event;
  gboolean motion_notify_event;
  gboolean value_changed_event;
  gboolean can_deactivate_pager;
  gboolean can_hide;
  gboolean filter;
  gboolean fullsize;
  gboolean internal;
  gboolean lock_position;
  gboolean proximity;
  gint win_x;
  gint win_y;
  gint slide_initial_slider_position;
  gint slide_initial_coordinate;
  gint pointer_x;
  gint pointer_y;
  guint32 source_deactivate_pager_id;
  guint32 source_hide_thumb_id;
  guint32 source_unlock_thumb_id;
};

static gboolean os_scrollbar_expose_event (GtkWidget *widget, GdkEventExpose *event);
static void os_scrollbar_grab_notify (GtkWidget *widget, gboolean was_grabbed);
static void os_scrollbar_hide (GtkWidget *widget);
static void os_scrollbar_map (GtkWidget *widget);
static void os_scrollbar_realize (GtkWidget *widget);
static void os_scrollbar_show (GtkWidget *widget);
static void os_scrollbar_size_allocate (GtkWidget *widget, GdkRectangle *allocation);
static void os_scrollbar_unmap (GtkWidget *widget);
static void os_scrollbar_unrealize (GtkWidget *widget);
static void os_scrollbar_dispose (GObject *object);
static void os_scrollbar_finalize (GObject *object);
static void os_scrollbar_calc_layout_pager (OsScrollbar *scrollbar, gdouble adjustment_value);
static void os_scrollbar_calc_layout_slider (OsScrollbar *scrollbar, gdouble adjustment_value);
static gdouble os_scrollbar_coord_to_value (OsScrollbar *scrollbar, gint coord);
static void os_scrollbar_deactivate_pager (OsScrollbar *scrollbar);
static gboolean os_scrollbar_deactivate_pager_cb (gpointer user_data);
static gdouble os_scrollbar_get_wheel_delta (OsScrollbar *scrollbar, GdkScrollDirection direction);
static void os_scrollbar_hide_thumb (OsScrollbar *scrollbar);
static gboolean os_scrollbar_hide_thumb_cb (gpointer user_data);
static gboolean os_scrollbar_unlock_thumb_cb (gpointer user_data);
static void os_scrollbar_move (OsScrollbar *scrollbar, gint mouse_x, gint mouse_y);
static void os_scrollbar_move_thumb (OsScrollbar *scrollbar, gint x, gint y);
static void os_scrollbar_notify_adjustment_cb (GObject *object, gpointer user_data);
static void os_scrollbar_notify_orientation_cb (GObject *object, gpointer user_data);
static gint os_scrollbar_sanitize_x (OsScrollbar *scrollbar, gint x, gint y);
static gint os_scrollbar_sanitize_y (OsScrollbar *scrollbar, gint x, gint y);
static void os_scrollbar_store_window_position (OsScrollbar *scrollbar);
static void os_scrollbar_swap_adjustment (OsScrollbar *scrollbar, GtkAdjustment *adjustment);
static void os_scrollbar_swap_thumb (OsScrollbar *scrollbar, GtkWidget *thumb);
static gboolean thumb_button_press_event_cb (GtkWidget *widget, GdkEventButton *event, gpointer user_data);
static gboolean thumb_button_release_event_cb (GtkWidget *widget, GdkEventButton *event, gpointer user_data);
static gboolean thumb_enter_notify_event_cb (GtkWidget *widget, GdkEventCrossing *event, gpointer user_data);
static gboolean thumb_leave_notify_event_cb (GtkWidget *widget, GdkEventCrossing *event, gpointer user_data);
static void thumb_map_cb (GtkWidget *widget, gpointer user_data);
static gboolean thumb_motion_notify_event_cb (GtkWidget *widget, GdkEventMotion *event, gpointer user_data);
static gboolean thumb_scroll_event_cb (GtkWidget *widget, GdkEventScroll *event, gpointer user_data);
static void thumb_unmap_cb (GtkWidget *widget, gpointer user_data);
static void pager_move (OsScrollbar *scrollbar);
static void pager_set_state_from_pointer (OsScrollbar *scrollbar, gint x, gint y);
static void adjustment_changed_cb (GtkAdjustment *adjustment, gpointer user_data);
static void adjustment_value_changed_cb (GtkAdjustment *adjustment, gpointer user_data);
static gboolean toplevel_configure_event_cb (GtkWidget *widget, GdkEventConfigure *event, gpointer user_data);
static GdkFilterReturn toplevel_filter_func (GdkXEvent *gdkxevent, GdkEvent *event, gpointer user_data);
static gboolean toplevel_leave_notify_event_cb (GtkWidget *widget, GdkEventCrossing *event, gpointer user_data);

/* Private functions. */

/* Present a X11 window. */
static void
os_present_window_with_timestamp (Screen  *screen,
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

  XSendEvent (display, root, False,
              SubstructureRedirectMask | SubstructureNotifyMask,
              &xev);

  gdk_flush ();

  gdk_error_trap_pop ();
}

/* Present a GDK window. */
static void
os_present_gdk_window_with_timestamp (GtkWidget *widget,
                                      guint32    timestamp)
{
  os_present_window_with_timestamp (GDK_SCREEN_XSCREEN (gtk_widget_get_screen (widget)),
                                    GDK_WINDOW_XID (gtk_widget_get_window (widget)),
                                    timestamp);
}

/* Calculate layout and store info. */
static void
os_scrollbar_calc_layout_pager (OsScrollbar *scrollbar,
                                gdouble      adjustment_value)
{
  OsScrollbarPrivate *priv;

  priv = scrollbar->priv;

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      gint y, bottom, top, height;

      top = priv->trough.y;
      bottom = priv->trough.y + priv->trough.height;

      /* overlay height is the fraction (page_size /
       * total_adjustment_range) times the trough height in pixels
       */

      if (priv->adjustment->upper - priv->adjustment->lower != 0)
        height = ((bottom - top) * (priv->adjustment->page_size /
                                 (priv->adjustment->upper - priv->adjustment->lower)));
      else
        height = gtk_range_get_min_slider_size (GTK_RANGE (scrollbar));

      height = MAX (height, gtk_range_get_min_slider_size (GTK_RANGE (scrollbar)));

      height = MIN (height, priv->trough.height);

      y = top;

      if (priv->adjustment->upper - priv->adjustment->lower - priv->adjustment->page_size != 0)
        y += (bottom - top - height) * ((adjustment_value - priv->adjustment->lower) /
                                        (priv->adjustment->upper - priv->adjustment->lower - priv->adjustment->page_size));

      y = CLAMP (y, top, bottom);

      priv->overlay.y = y;
      priv->overlay.height = height;
    }
  else
    {
      gint x, left, right, width;

      left = priv->trough.x;
      right = priv->trough.x + priv->trough.width;

      /* overlay width is the fraction (page_size /
       * total_adjustment_range) times the trough width in pixels
       */

      if (priv->adjustment->upper - priv->adjustment->lower != 0)
        width = ((right - left) * (priv->adjustment->page_size /
                                  (priv->adjustment->upper - priv->adjustment->lower)));
      else
        width =  gtk_range_get_min_slider_size (GTK_RANGE (scrollbar));

      width = MAX (width, gtk_range_get_min_slider_size (GTK_RANGE (scrollbar)));

      width = MIN (width, priv->trough.width);

      x = left;

      if (priv->adjustment->upper - priv->adjustment->lower - priv->adjustment->page_size != 0)
        x += (right - left - width) * ((adjustment_value - priv->adjustment->lower) /
                                       (priv->adjustment->upper - priv->adjustment->lower - priv->adjustment->page_size));

      x = CLAMP (x, left, right);

      priv->overlay.x = x;
      priv->overlay.width = width;
    }
}

/* Calculate OsScrollbar layout and store info. */
static void
os_scrollbar_calc_layout_slider (OsScrollbar *scrollbar,
                                 gdouble      adjustment_value)
{
  OsScrollbarPrivate *priv;

  priv = scrollbar->priv;

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      gint y, bottom, top, height;

      top = priv->trough.y;
      bottom = priv->trough.y + priv->trough.height;

      height = priv->slider.height;

      height = MIN (height, priv->trough.height);

      y = top;

      if (priv->adjustment->upper - priv->adjustment->lower - priv->adjustment->page_size != 0)
        y += (bottom - top - height) * ((adjustment_value - priv->adjustment->lower) /
                                        (priv->adjustment->upper - priv->adjustment->lower - priv->adjustment->page_size));

      y = CLAMP (y, top, bottom);

      priv->slider.y = y;
      priv->slider.height = height;
    }
  else
    {
      gint x, left, right, width;

      left = priv->trough.x;
      right = priv->trough.x + priv->trough.width;

      width = priv->slider.width;

      width = MIN (width, priv->trough.width);

      x = left;

      if (priv->adjustment->upper - priv->adjustment->lower - priv->adjustment->page_size != 0)
        x += (right - left - width) * ((adjustment_value - priv->adjustment->lower) /
                                       (priv->adjustment->upper - priv->adjustment->lower - priv->adjustment->page_size));

      x = CLAMP (x, left, right);

      priv->slider.x = x;
      priv->slider.width = width;
    }
}

/* Traduce pixels into proper values. */
static gdouble
os_scrollbar_coord_to_value (OsScrollbar *scrollbar,
                             gint         coord)
{
  OsScrollbarPrivate *priv;
  gdouble frac;
  gdouble value;
  gint    trough_length;
  gint    trough_start;
  gint    slider_length;

  priv = scrollbar->priv;

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      trough_length = priv->trough.height;
      trough_start  = priv->trough.y;
      slider_length = MAX (priv->slider.height, priv->overlay.height);
    }
  else
    {
      trough_length = priv->trough.width;
      trough_start  = priv->trough.y;
      slider_length = MAX (priv->slider.width, priv->overlay.width);
    }

  if (trough_length == slider_length)
    frac = 1.0;
  else
    frac = (MAX (0, coord - trough_start) /
            (gdouble) (trough_length - slider_length));

  value = priv->adjustment->lower + frac * (priv->adjustment->upper -
                                            priv->adjustment->lower -
                                            priv->adjustment->page_size);

  value = CLAMP (value, priv->adjustment->lower, priv->adjustment->upper - priv->adjustment->page_size);

  return value;
}

/* Deactivate pager if it's ok to make it inactive. */
static void
os_scrollbar_deactivate_pager (OsScrollbar *scrollbar)
{
  OsScrollbarPrivate *priv;

  priv = scrollbar->priv;

  if (priv->pager != NULL && priv->can_deactivate_pager)
    os_pager_set_active (OS_PAGER (priv->pager), FALSE);
}

static gboolean
os_scrollbar_deactivate_pager_cb (gpointer user_data)
{
  OsScrollbar *scrollbar;
  OsScrollbarPrivate *priv;

  scrollbar = OS_SCROLLBAR (user_data);
  priv = scrollbar->priv;

  os_scrollbar_deactivate_pager (scrollbar);
  priv->source_deactivate_pager_id = 0;

  return FALSE;
}

static gdouble
os_scrollbar_get_wheel_delta (OsScrollbar       *scrollbar,
                              GdkScrollDirection direction)
{
  OsScrollbarPrivate *priv;
  gdouble delta;

  priv = scrollbar->priv;

  delta = pow (priv->adjustment->page_size, 2.0 / 3.0);

  if (direction == GDK_SCROLL_UP ||
      direction == GDK_SCROLL_LEFT)
    delta = - delta;

  return delta;
}

/* Hide if it's ok to hide. */
static void
os_scrollbar_hide_thumb (OsScrollbar *scrollbar)
{
  OsScrollbarPrivate *priv;

  priv = scrollbar->priv;

  if (priv->can_hide)
    {
      priv->value_changed_event = FALSE;
      gtk_widget_hide (GTK_WIDGET (priv->thumb));
    }
}

static gboolean
os_scrollbar_hide_thumb_cb (gpointer user_data)
{
  OsScrollbar *scrollbar;
  OsScrollbarPrivate *priv;

  scrollbar = OS_SCROLLBAR (user_data);
  priv = scrollbar->priv;

  os_scrollbar_hide_thumb (scrollbar);
  priv->source_hide_thumb_id = 0;

  return FALSE;
}

static gboolean
os_scrollbar_unlock_thumb_cb (gpointer user_data)
{
  OsScrollbar *scrollbar;
  OsScrollbarPrivate *priv;

  scrollbar = OS_SCROLLBAR (user_data);
  priv = scrollbar->priv;

  if (priv->can_hide)
    priv->lock_position = FALSE;

  priv->source_unlock_thumb_id = 0;

  return FALSE;
}

static void
os_scrollbar_move (OsScrollbar *scrollbar,
                   gint         mouse_x,
                   gint         mouse_y)
{
  OsScrollbarPrivate *priv;
  gint delta;
  gint c;
  gdouble new_value;

  priv = scrollbar->priv;

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    delta = mouse_y - priv->slide_initial_coordinate;
  else
    delta = mouse_x - priv->slide_initial_coordinate;

  c = priv->slide_initial_slider_position + delta;

  new_value = os_scrollbar_coord_to_value (scrollbar, c);

  gtk_adjustment_set_value (priv->adjustment, new_value);
}

static void
os_scrollbar_move_thumb (OsScrollbar *scrollbar,
                         gint         x,
                         gint         y)
{
  OsScrollbarPrivate *priv;

  priv = scrollbar->priv;

  gtk_window_move (GTK_WINDOW (priv->thumb),
                   os_scrollbar_sanitize_x (scrollbar, x, y),
                   os_scrollbar_sanitize_y (scrollbar, x, y));
}

static void
os_scrollbar_notify_adjustment_cb (GObject *object,
                                   gpointer user_data)
{
  OsScrollbar *scrollbar;
  OsScrollbarPrivate *priv;

  scrollbar = OS_SCROLLBAR (object);
  priv = scrollbar->priv;

  os_scrollbar_swap_adjustment (scrollbar, gtk_range_get_adjustment (GTK_RANGE (object)));
}

static void
os_scrollbar_notify_orientation_cb (GObject *object,
                                    gpointer user_data)
{
  OsScrollbar *scrollbar;
  OsScrollbarPrivate *priv;

  scrollbar = OS_SCROLLBAR (object);
  priv = scrollbar->priv;

  priv->orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (object));

  os_scrollbar_swap_thumb (scrollbar, os_thumb_new (priv->orientation));
}

static gint
os_scrollbar_sanitize_x (OsScrollbar *scrollbar,
                         gint         x,
                         gint         y)
{
  GdkRectangle rect;
  OsScrollbarPrivate *priv;
  gint screen_width, n_monitor;
  GdkScreen *screen;
  
  priv = scrollbar->priv;

  /* the x - 1 coordinate shift is done 
   * to calculate monitor boundaries. */
  screen = gtk_widget_get_screen (GTK_WIDGET (scrollbar)); 
  n_monitor = gdk_screen_get_monitor_at_point (screen, x - 1, y);
  gdk_screen_get_monitor_geometry (screen, n_monitor, &rect);

  screen_width = rect.x + rect.width;

  if (priv->orientation == GTK_ORIENTATION_VERTICAL &&
      (n_monitor != gdk_screen_get_monitor_at_point (screen, x - 1 + priv->slider.width, y) ||
       (x - 1 + priv->slider.width) >= screen_width))
    {
      priv->internal = TRUE;
      return x - DEFAULT_PAGER_WIDTH - priv->slider.width;
    }

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    priv->internal = FALSE;

  return x;
}

static gint
os_scrollbar_sanitize_y (OsScrollbar *scrollbar,
                         gint         x,
                         gint         y)
{
  GdkRectangle rect;
  OsScrollbarPrivate *priv;
  gint screen_height, n_monitor;
  GdkScreen *screen;
  
  priv = scrollbar->priv;

  /* the y - 1 coordinate shift is done 
   * to calculate monitor boundaries. */
  screen = gtk_widget_get_screen (GTK_WIDGET (scrollbar)); 
  n_monitor = gdk_screen_get_monitor_at_point (screen, x, y - 1);
  gdk_screen_get_monitor_geometry (screen, n_monitor, &rect);
  
  screen_height = rect.y + rect.height;

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL &&
      (n_monitor != gdk_screen_get_monitor_at_point (screen, x, y - 1 + priv->slider.height) ||
       (y - 1 + priv->slider.height) >= screen_height))
    {
      priv->internal = TRUE;
      return y - DEFAULT_PAGER_WIDTH - priv->slider.height;
    }

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    priv->internal = FALSE;

  return y;
}

/* Store scrollbar window position. */
static void
os_scrollbar_store_window_position (OsScrollbar *scrollbar)
{
  OsScrollbarPrivate *priv;
  gint win_x, win_y;

  priv = scrollbar->priv;

  gdk_window_get_origin (gtk_widget_get_window (GTK_WIDGET (scrollbar)), &win_x, &win_y);

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      priv->win_x = win_x + priv->thumb_all.x;
      priv->win_y = win_y + priv->thumb_all.y;
    }
  else
    {
      priv->win_x = win_x + priv->thumb_all.x;
      priv->win_y = win_y + priv->thumb_all.y;
    }
}

/* Swap the adjustment pointer. */
static void
os_scrollbar_swap_adjustment (OsScrollbar   *scrollbar,
                              GtkAdjustment *adjustment)
{
  OsScrollbarPrivate *priv;

  priv = scrollbar->priv;

  if (priv->adjustment != NULL)
    {
      g_signal_handlers_disconnect_by_func (G_OBJECT (priv->adjustment),
                                            G_CALLBACK (adjustment_changed_cb), scrollbar);
      g_signal_handlers_disconnect_by_func (G_OBJECT (priv->adjustment),
                                            G_CALLBACK (adjustment_value_changed_cb), scrollbar);

      g_object_unref (priv->adjustment);
    }

  priv->adjustment = adjustment;

  if (priv->adjustment != NULL)
    {
      g_object_ref_sink (priv->adjustment);

      g_signal_connect (G_OBJECT (priv->adjustment), "changed",
                        G_CALLBACK (adjustment_changed_cb), scrollbar);
      g_signal_connect (G_OBJECT (priv->adjustment), "value-changed",
                        G_CALLBACK (adjustment_value_changed_cb), scrollbar);
    }
}

/* Swap the thumb pointer. */
static void
os_scrollbar_swap_thumb (OsScrollbar *scrollbar,
                         GtkWidget   *thumb)
{
  OsScrollbarPrivate *priv;

  priv = scrollbar->priv;

  if (priv->thumb != NULL)
    {
      g_signal_handlers_disconnect_by_func (G_OBJECT (priv->thumb),
                                            thumb_button_press_event_cb, scrollbar);
      g_signal_handlers_disconnect_by_func (G_OBJECT (priv->thumb),
                                            thumb_button_release_event_cb, scrollbar);
      g_signal_handlers_disconnect_by_func (G_OBJECT (priv->thumb),
                                            thumb_enter_notify_event_cb, scrollbar);
      g_signal_handlers_disconnect_by_func (G_OBJECT (priv->thumb),
                                            thumb_leave_notify_event_cb, scrollbar);
      g_signal_handlers_disconnect_by_func (G_OBJECT (priv->thumb),
                                            thumb_map_cb, scrollbar);
      g_signal_handlers_disconnect_by_func (G_OBJECT (priv->thumb),
                                            thumb_motion_notify_event_cb, scrollbar);
      g_signal_handlers_disconnect_by_func (G_OBJECT (priv->thumb),
                                            thumb_scroll_event_cb, scrollbar);
      g_signal_handlers_disconnect_by_func (G_OBJECT (priv->thumb),
                                            thumb_unmap_cb, scrollbar);

      gtk_widget_destroy (priv->thumb);

      g_object_unref (priv->thumb);
    }

  priv->thumb = thumb;

  if (priv->thumb != NULL)
    {
      g_object_ref_sink (priv->thumb);

      g_signal_connect (G_OBJECT (priv->thumb), "button-press-event",
                        G_CALLBACK (thumb_button_press_event_cb), scrollbar);
      g_signal_connect (G_OBJECT (priv->thumb), "button-release-event",
                        G_CALLBACK (thumb_button_release_event_cb), scrollbar);
      g_signal_connect (G_OBJECT (priv->thumb), "enter-notify-event",
                        G_CALLBACK (thumb_enter_notify_event_cb), scrollbar);
      g_signal_connect (G_OBJECT (priv->thumb), "leave-notify-event",
                        G_CALLBACK (thumb_leave_notify_event_cb), scrollbar);
      g_signal_connect (G_OBJECT (priv->thumb), "map",
                        G_CALLBACK (thumb_map_cb), scrollbar);
      g_signal_connect (G_OBJECT (priv->thumb), "motion-notify-event",
                        G_CALLBACK (thumb_motion_notify_event_cb), scrollbar);
      g_signal_connect (G_OBJECT (priv->thumb), "scroll-event",
                        G_CALLBACK (thumb_scroll_event_cb), scrollbar);
      g_signal_connect (G_OBJECT (priv->thumb), "unmap",
                        G_CALLBACK (thumb_unmap_cb), scrollbar);
    }
}

static gboolean
thumb_button_press_event_cb (GtkWidget      *widget,
                             GdkEventButton *event,
                             gpointer        user_data)
{
  if (event->type == GDK_BUTTON_PRESS)
    {
      if (event->button == 1)
        {
          OsScrollbar *scrollbar;
          OsScrollbarPrivate *priv;

          scrollbar = OS_SCROLLBAR (user_data);
          priv = scrollbar->priv;

          gtk_window_set_transient_for (GTK_WINDOW (widget), GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (scrollbar))));
          os_present_gdk_window_with_timestamp (GTK_WIDGET (scrollbar), event->time);

          priv->button_press_event = TRUE;
          priv->motion_notify_event = FALSE;

          if (priv->orientation == GTK_ORIENTATION_VERTICAL)
            {
              priv->slide_initial_slider_position = MIN (priv->slider.y, priv->overlay.y);
              priv->slide_initial_coordinate = event->y_root;
            }
          else
            {
              priv->slide_initial_slider_position = MIN (priv->slider.x, priv->overlay.x);
              priv->slide_initial_coordinate = event->x_root;
            }

          priv->pointer_x = event->x;
          priv->pointer_y = event->y;
        }
    }

  return FALSE;
}

static gboolean
thumb_button_release_event_cb (GtkWidget      *widget,
                               GdkEventButton *event,
                               gpointer        user_data)
{
  if (event->type == GDK_BUTTON_RELEASE)
    {
      if (event->button == 1)
        {
          OsScrollbar *scrollbar;
          OsScrollbarPrivate *priv;

          scrollbar = OS_SCROLLBAR (user_data);
          priv = scrollbar->priv;

          gtk_window_set_transient_for (GTK_WINDOW (widget), NULL);

          if (!priv->motion_notify_event)
            {
              if (priv->orientation == GTK_ORIENTATION_VERTICAL)
                {
                  if (priv->pointer_y < priv->slider.height / 2)
                    g_signal_emit_by_name (GTK_RANGE (scrollbar), "move-slider", GTK_SCROLL_PAGE_UP);
                  else
                    g_signal_emit_by_name (GTK_RANGE (scrollbar), "move-slider", GTK_SCROLL_PAGE_DOWN);
                }
              else
                {
                  if (priv->pointer_x < priv->slider.width / 2)
                    g_signal_emit_by_name (GTK_RANGE (scrollbar), "move-slider", GTK_SCROLL_PAGE_UP);
                  else
                    g_signal_emit_by_name (GTK_RANGE (scrollbar), "move-slider", GTK_SCROLL_PAGE_DOWN);
                }

              priv->value_changed_event = TRUE;
            }

          priv->button_press_event = FALSE;
          priv->motion_notify_event = FALSE;
        }
    }

  return FALSE;
}

static gboolean
thumb_enter_notify_event_cb (GtkWidget        *widget,
                             GdkEventCrossing *event,
                             gpointer          user_data)
{
  OsScrollbar *scrollbar;
  OsScrollbarPrivate *priv;

  scrollbar = OS_SCROLLBAR (user_data);
  priv = scrollbar->priv;

  priv->enter_notify_event = TRUE;
  priv->can_deactivate_pager = FALSE;
  priv->can_hide = FALSE;

  if (priv->internal)
    priv->lock_position = TRUE;

  return FALSE;
}

static gboolean
thumb_leave_notify_event_cb (GtkWidget        *widget,
                             GdkEventCrossing *event,
                             gpointer          user_data)
{
  OsScrollbar *scrollbar;
  OsScrollbarPrivate *priv;

  scrollbar = OS_SCROLLBAR (user_data);
  priv = scrollbar->priv;

  if (!priv->button_press_event)
    {
      priv->can_deactivate_pager = TRUE;
      priv->can_hide = TRUE;
    }

  if (priv->source_deactivate_pager_id != 0)
    g_source_remove (priv->source_deactivate_pager_id);

  priv->source_deactivate_pager_id = g_timeout_add (TIMEOUT_THUMB_HIDE,
                                                    os_scrollbar_deactivate_pager_cb,
                                                    scrollbar);

  if (priv->source_hide_thumb_id != 0)
    g_source_remove (priv->source_hide_thumb_id);

  priv->source_hide_thumb_id = g_timeout_add (TIMEOUT_THUMB_HIDE,
                                              os_scrollbar_hide_thumb_cb,
                                              scrollbar);

  return FALSE;
}

static void
thumb_map_cb (GtkWidget *widget,
              gpointer   user_data)
{
  Display *display;
  OsScrollbar *scrollbar;
  OsScrollbarPrivate *priv;
  XWindowChanges changes;
  guint32 xid, xid_parent;
  unsigned int value_mask = CWSibling | CWStackMode;
  int res;

  scrollbar = OS_SCROLLBAR (user_data);
  priv = scrollbar->priv;

  xid = GDK_WINDOW_XID (gtk_widget_get_window (priv->thumb));
  xid_parent = GDK_WINDOW_XID (gtk_widget_get_window (GTK_WIDGET (scrollbar)));
  display = GDK_WINDOW_XDISPLAY (gtk_widget_get_window (GTK_WIDGET (scrollbar)));

  changes.sibling = xid_parent;
  changes.stack_mode = Above;

  gdk_error_trap_push ();
  XConfigureWindow (display, xid, value_mask, &changes);

  gdk_flush ();
  if ((res = gdk_error_trap_pop ()))
    {
      /* FIXME(Cimi) this code tries to restack the window using
       * the _NET_RESTACK_WINDOW atom. See:
       * http://standards.freedesktop.org/wm-spec/wm-spec-1.3.html#id2506866
       * 
       * Should work on window managers that supports this, like compiz.
       * Unfortunately, metacity doesn't yet, so we might decide to implement
       * this atom in metacity/mutter as well. 
       * 
       * We need to restack the window because the thumb window can be above
       * every window, noticeable when you make the thumb of an unfocused window
       * appear, and it could be above other windows (like the focused one).
       * 
       * XConfigureWindow doesn't always work (well, should work only with
       * compiz 0.8 or older), because it is not handling the reparenting done
       * at the WM level (metacity and compiz >= 0.9 do reparenting). */
      Window root = gdk_x11_get_default_root_xwindow ();
      XEvent xev;

      xev.xclient.type = ClientMessage;
      xev.xclient.display = display;
      xev.xclient.serial = 0;
      xev.xclient.send_event = True;
      xev.xclient.window = xid;
      xev.xclient.message_type = gdk_x11_get_xatom_by_name ("_NET_RESTACK_WINDOW");
      xev.xclient.format = 32;
      xev.xclient.data.l[0] = 2;
      xev.xclient.data.l[1] = xid_parent;
      xev.xclient.data.l[2] = Above;
      xev.xclient.data.l[3] = 0;
      xev.xclient.data.l[4] = 0;

      gdk_error_trap_push ();

      XSendEvent (display, root, False,
                  SubstructureRedirectMask | SubstructureNotifyMask,
                  &xev);

      gdk_flush ();

      gdk_error_trap_pop ();
    }
}

static gboolean
thumb_motion_notify_event_cb (GtkWidget      *widget,
                              GdkEventMotion *event,
                              gpointer        user_data)
{
  OsScrollbar *scrollbar;
  OsScrollbarPrivate *priv;

  scrollbar = OS_SCROLLBAR (user_data);
  priv = scrollbar->priv;

  if (priv->button_press_event)
    {
      gint x, y;

      /* reconnect slider and overlay after key press */
      if (priv->value_changed_event)
        {
          if (priv->orientation == GTK_ORIENTATION_VERTICAL)
            {
              priv->slide_initial_slider_position = event->y_root - priv->win_y - event->y;
              priv->slide_initial_coordinate = event->y_root;
            }
          else
            {
              priv->slide_initial_slider_position = event->x_root - priv->win_x - event->x;
              priv->slide_initial_coordinate = event->x_root;
            }

          os_scrollbar_move (scrollbar, event->x_root, event->y_root);
          priv->value_changed_event = FALSE;
        }

      priv->motion_notify_event = TRUE;

      os_scrollbar_move (scrollbar, event->x_root, event->y_root);

      if (priv->orientation == GTK_ORIENTATION_VERTICAL)
        {
          if (priv->overlay.height > priv->slider.height)
            {
              x = priv->win_x;
              y = CLAMP (event->y_root - priv->pointer_y,
                         priv->win_y + priv->overlay.y,
                         priv->win_y + priv->overlay.y + priv->overlay.height - priv->slider.height);

              if (priv->overlay.y == 0)
                {
                  priv->slide_initial_slider_position = 0;
                  priv->slide_initial_coordinate = MAX (event->y_root, priv->win_y + priv->pointer_y);
                }
              else if (priv->overlay.y + priv->overlay.height >= priv->trough.y + priv->trough.height)
                {
                  priv->slide_initial_slider_position = priv->trough.y + priv->trough.height - priv->overlay.height;
                  priv->slide_initial_coordinate = MAX (event->y_root, priv->win_y + priv->pointer_y);
                }
            }
          else
            {
              x = priv->win_x;
              y = priv->win_y + priv->slider.y;
            }
        }
      else
        {
          if (priv->overlay.width > priv->slider.width)
            {
              x = CLAMP (event->x_root - priv->pointer_x,
                         priv->win_x + priv->overlay.x,
                         priv->win_x + priv->overlay.x + priv->overlay.width - priv->slider.width);
              y = priv->win_y;

              if (priv->overlay.x == 0)
                {
                  priv->slide_initial_slider_position = 0;
                  priv->slide_initial_coordinate = MAX (event->x_root, priv->win_x + priv->pointer_x);
                }
              else if (priv->overlay.x + priv->overlay.width >= priv->trough.x + priv->trough.width)
                {
                  priv->slide_initial_slider_position = priv->trough.x + priv->trough.width - priv->overlay.width;
                  priv->slide_initial_coordinate = MAX (event->x_root, priv->win_x + priv->pointer_x);
                }
            }
          else
            {
              x = priv->win_x + priv->slider.x;
              y = priv->win_y;
            }
        }

      os_scrollbar_move_thumb (scrollbar, x, y);
    }

  return FALSE;
}

static gboolean
thumb_scroll_event_cb (GtkWidget      *widget,
                       GdkEventScroll *event,
                       gpointer        user_data)
{
  OsScrollbar *scrollbar;
  OsScrollbarPrivate *priv;
  gdouble delta;

  scrollbar = OS_SCROLLBAR (user_data);
  priv = scrollbar->priv;

  delta = os_scrollbar_get_wheel_delta (scrollbar, event->direction);

  gtk_adjustment_set_value (priv->adjustment, priv->adjustment->value + delta);

  return FALSE;
}

static void
thumb_unmap_cb (GtkWidget *widget,
                gpointer   user_data)
{
  OsScrollbar *scrollbar;
  OsScrollbarPrivate *priv;

  scrollbar = OS_SCROLLBAR (user_data);
  priv = scrollbar->priv;

  priv->button_press_event = FALSE;
  priv->motion_notify_event = FALSE;
  priv->enter_notify_event = FALSE;
}

/* Move the pager to the right position. */
static void
pager_move (OsScrollbar *scrollbar)
{
  GdkRectangle mask;
  OsScrollbarPrivate *priv;

  priv = scrollbar->priv;

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      mask.x = 0;
      mask.y = priv->overlay.y;
      mask.width = DEFAULT_PAGER_WIDTH;
      mask.height = priv->overlay.height;
    }
  else
    {
      mask.x = priv->overlay.x;
      mask.y = 0;
      mask.width = priv->overlay.width;
      mask.height = DEFAULT_PAGER_WIDTH;
    }

  os_pager_move_resize (OS_PAGER (priv->pager), mask);
}

static void
pager_set_state_from_pointer (OsScrollbar *scrollbar,
                              gint x,
                              gint y)
{
  GtkAllocation allocation;
  OsScrollbarPrivate *priv;

  priv = scrollbar->priv;

  gtk_widget_get_allocation (gtk_widget_get_parent (GTK_WIDGET (scrollbar)), &allocation);

  if ((x > allocation.x && x < allocation.x + allocation.width) &&
      (y > allocation.y && y < allocation.y + allocation.height))
    {
      priv->can_deactivate_pager = FALSE;
      os_pager_set_active (OS_PAGER (priv->pager), TRUE);
    }
  else
    {
      priv->can_deactivate_pager = TRUE;
      os_scrollbar_deactivate_pager (scrollbar);
    }
}

static void
adjustment_changed_cb (GtkAdjustment *adjustment,
                       gpointer       user_data)
{
  OsScrollbar *scrollbar;
  OsScrollbarPrivate *priv;

  scrollbar = OS_SCROLLBAR (user_data);
  priv = scrollbar->priv;

  /* FIXME(Cimi) we should control each time os_pager_show()/hide()
   * is called here and in map()/unmap().
   * We are arbitrary calling that and I'm frightened we should show or keep
   * hidden a pager that is meant to be hidden/shown.
   * I don't want to see pagers reappearing because
   * of a change in the adjustment of an invisible pager or viceversa. */
  if ((adjustment->upper - adjustment->lower) > adjustment->page_size)
    {
      priv->fullsize = FALSE;
      if (priv->proximity != FALSE)
        os_pager_show (OS_PAGER (priv->pager));
    }
  else
    {
      priv->fullsize = TRUE;
      if (priv->proximity != FALSE)
        {
          os_pager_hide (OS_PAGER (priv->pager));

          gtk_widget_hide (priv->thumb);
        }
    }

  os_scrollbar_calc_layout_pager (scrollbar, adjustment->value);
  os_scrollbar_calc_layout_slider (scrollbar, adjustment->value);

  if (!priv->motion_notify_event && !priv->enter_notify_event)
    gtk_widget_hide (GTK_WIDGET (priv->thumb));

  pager_move (scrollbar);
}

static void
adjustment_value_changed_cb (GtkAdjustment *adjustment,
                             gpointer       user_data)
{
  OsScrollbar *scrollbar;
  OsScrollbarPrivate *priv;

  scrollbar = OS_SCROLLBAR (user_data);
  priv = scrollbar->priv;

  os_scrollbar_calc_layout_pager (scrollbar, adjustment->value);
  os_scrollbar_calc_layout_slider (scrollbar, adjustment->value);

  if (!priv->motion_notify_event && !priv->enter_notify_event)
    gtk_widget_hide (GTK_WIDGET (priv->thumb));

  pager_move (scrollbar);
}

static gboolean
toplevel_configure_event_cb (GtkWidget         *widget,
                             GdkEventConfigure *event,
                             gpointer           user_data)
{
  OsScrollbar *scrollbar;
  OsScrollbarPrivate *priv;
  gint x, y;

  scrollbar = OS_SCROLLBAR (user_data);
  priv = scrollbar->priv;

  gtk_widget_get_pointer (widget, &x, &y);

  pager_set_state_from_pointer (scrollbar, x, y);

  if (!priv->enter_notify_event)
    gtk_widget_hide (GTK_WIDGET (priv->thumb));

  priv->lock_position = FALSE;

  os_scrollbar_calc_layout_pager (scrollbar, priv->adjustment->value);
  os_scrollbar_calc_layout_slider (scrollbar, priv->adjustment->value);

  os_scrollbar_store_window_position (scrollbar);

  return FALSE;
}

/* Add a filter to the toplevel GdkWindow, to activate proximity effect. */
static GdkFilterReturn
toplevel_filter_func (GdkXEvent *gdkxevent,
                      GdkEvent  *event,
                      gpointer   user_data)
{
  OsScrollbar *scrollbar;
  OsScrollbarPrivate *priv;
  XEvent *xevent;

  g_return_val_if_fail (OS_SCROLLBAR (user_data), GDK_FILTER_CONTINUE);

  scrollbar = OS_SCROLLBAR (user_data);

  priv = scrollbar->priv;
  xevent = gdkxevent;

  g_return_val_if_fail (priv->pager != NULL, GDK_FILTER_CONTINUE);
  g_return_val_if_fail (priv->thumb != NULL, GDK_FILTER_CONTINUE);

  if (!priv->fullsize)
    {
      /* get the motion_notify_event trough XEvent */
      if (xevent->type == MotionNotify)
        {
          pager_set_state_from_pointer (scrollbar, xevent->xmotion.x, xevent->xmotion.y);

          /* proximity area */
          if (priv->orientation == GTK_ORIENTATION_VERTICAL)
            {
              if ((priv->thumb_all.x - xevent->xmotion.x <= PROXIMITY_WIDTH &&
                   priv->thumb_all.x - xevent->xmotion.x >= 0) &&
                  (xevent->xmotion.y >= priv->thumb_all.y + priv->overlay.y &&
                   xevent->xmotion.y <= priv->thumb_all.y + priv->overlay.y + priv->overlay.height))
                {
                  priv->can_hide = FALSE;

                  if (priv->lock_position)
                    return GDK_FILTER_CONTINUE;

                  if (priv->overlay.height > priv->slider.height)
                    {
                      gint x, y, x_pos, y_pos;

                      gdk_window_get_origin (gtk_widget_get_window (GTK_WIDGET (scrollbar)), &x_pos, &y_pos);

                      x = priv->thumb_all.x;
                      y = CLAMP (xevent->xmotion.y - priv->slider.height / 2,
                                 priv->thumb_all.y + priv->overlay.y,
                                 priv->thumb_all.y + priv->overlay.y + priv->overlay.height - priv->slider.height);

                      os_scrollbar_move_thumb (scrollbar, x_pos + x, y_pos + y);
                    }
                  else
                    {
                      os_scrollbar_move_thumb (scrollbar, priv->win_x, priv->win_y + priv->slider.y);
                    }

                  gtk_widget_show (GTK_WIDGET (priv->thumb));
                }
              else
                {
                  priv->can_hide = TRUE;
                  priv->lock_position = FALSE;
                  os_scrollbar_hide_thumb (scrollbar);
                }
            }
          else
            {
              if ((priv->thumb_all.y - xevent->xmotion.y <= PROXIMITY_WIDTH &&
                   priv->thumb_all.y - xevent->xmotion.y >= 0) &&
                  (xevent->xmotion.x >= priv->thumb_all.x + priv->overlay.x &&
                   xevent->xmotion.x <= priv->thumb_all.x + priv->overlay.x + priv->overlay.width))
                {
                  priv->can_hide = FALSE;

                  if (priv->lock_position)
                    return GDK_FILTER_CONTINUE;

                  if (priv->overlay.width > priv->slider.width)
                    {
                      gint x, y, x_pos, y_pos;

                      gdk_window_get_origin (gtk_widget_get_window (GTK_WIDGET (scrollbar)), &x_pos, &y_pos);

                      x = CLAMP (xevent->xmotion.x - priv->slider.width / 2,
                                 priv->thumb_all.x + priv->overlay.x,
                                 priv->thumb_all.x + priv->overlay.x + priv->overlay.width - priv->slider.width);
                      y = priv->thumb_all.y;

                      os_scrollbar_move_thumb (scrollbar, x_pos + x, y_pos + y);
                    }
                  else
                    {
                      os_scrollbar_move_thumb (scrollbar, priv->win_x, priv->win_y + priv->slider.y);
                    }

                  gtk_widget_show (GTK_WIDGET (priv->thumb));
                }
              else
                {
                  priv->can_hide = TRUE;
                  priv->lock_position = FALSE;
                  os_scrollbar_hide_thumb (scrollbar);
                }
            }
        }
    }

  return GDK_FILTER_CONTINUE;
}

/* Hide the OsScrollbar, when the pointer leaves the toplevel GtkWindow. */
static gboolean
toplevel_leave_notify_event_cb (GtkWidget        *widget,
                                GdkEventCrossing *event,
                                gpointer          user_data)
{
  OsScrollbar *scrollbar;
  OsScrollbarPrivate *priv;

  scrollbar = OS_SCROLLBAR (user_data);
  priv = scrollbar->priv;

  priv->can_deactivate_pager = TRUE;
  priv->can_hide = TRUE;

  if (priv->source_deactivate_pager_id != 0)
    g_source_remove (priv->source_deactivate_pager_id);

  priv->source_deactivate_pager_id = g_timeout_add (TIMEOUT_TOPLEVEL_HIDE,
                                                    os_scrollbar_deactivate_pager_cb,
                                                    scrollbar);

  if (priv->source_hide_thumb_id != 0)
    g_source_remove (priv->source_hide_thumb_id);

  priv->source_hide_thumb_id = g_timeout_add (TIMEOUT_TOPLEVEL_HIDE,
                                              os_scrollbar_hide_thumb_cb,
                                              scrollbar);

  if (priv->source_unlock_thumb_id != 0)
    g_source_remove (priv->source_unlock_thumb_id);

  priv->source_unlock_thumb_id = g_timeout_add (TIMEOUT_TOPLEVEL_HIDE,
                                                os_scrollbar_unlock_thumb_cb,
                                                scrollbar);

  return FALSE;
}

/* Type definition. */

G_DEFINE_TYPE (OsScrollbar, os_scrollbar, GTK_TYPE_SCROLLBAR);

static void
os_scrollbar_class_init (OsScrollbarClass *class)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS (class);
  widget_class = GTK_WIDGET_CLASS (class);

  widget_class->expose_event  = os_scrollbar_expose_event;
  widget_class->grab_notify   = os_scrollbar_grab_notify;
  widget_class->hide          = os_scrollbar_hide;
  widget_class->map           = os_scrollbar_map;
  widget_class->realize       = os_scrollbar_realize;
  widget_class->show          = os_scrollbar_show;
  widget_class->size_allocate = os_scrollbar_size_allocate;
  widget_class->unmap         = os_scrollbar_unmap;
  widget_class->unrealize     = os_scrollbar_unrealize;

  gobject_class->dispose  = os_scrollbar_dispose;
  gobject_class->finalize = os_scrollbar_finalize;

  g_type_class_add_private (gobject_class, sizeof (OsScrollbarPrivate));
}

static void
os_scrollbar_init (OsScrollbar *scrollbar)
{
  OsScrollbarPrivate *priv;

  scrollbar->priv = G_TYPE_INSTANCE_GET_PRIVATE (scrollbar,
                                                 OS_TYPE_SCROLLBAR,
                                                 OsScrollbarPrivate);
  priv = scrollbar->priv;

  priv->can_deactivate_pager = TRUE;
  priv->can_hide = TRUE;
  priv->filter = FALSE;
  priv->fullsize = FALSE;
  priv->internal = FALSE;
  priv->lock_position = FALSE;
  priv->proximity = FALSE;
  priv->source_deactivate_pager_id = 0;
  priv->source_hide_thumb_id = 0;
  priv->source_unlock_thumb_id = 0;

  priv->pager = os_pager_new ();

  g_signal_connect (G_OBJECT (scrollbar), "notify::adjustment",
                    G_CALLBACK (os_scrollbar_notify_adjustment_cb), NULL);

  g_signal_connect (G_OBJECT (scrollbar), "notify::orientation",
                    G_CALLBACK (os_scrollbar_notify_orientation_cb), NULL);
}

static void
os_scrollbar_dispose (GObject *object)
{
  OsScrollbar *scrollbar;
  OsScrollbarPrivate *priv;

  scrollbar = OS_SCROLLBAR (object);
  priv = scrollbar->priv;

  if (priv->source_deactivate_pager_id != 0)
    {
      g_source_remove (priv->source_deactivate_pager_id);
      priv->source_deactivate_pager_id = 0;
    }

  if (priv->source_hide_thumb_id != 0)
    {
      g_source_remove (priv->source_hide_thumb_id);
      priv->source_hide_thumb_id = 0;
    }

  if (priv->source_unlock_thumb_id != 0)
    {
      g_source_remove (priv->source_unlock_thumb_id);
      priv->source_unlock_thumb_id = 0;
    }

  if (priv->pager != NULL)
    {
      g_object_unref (priv->pager);
      priv->pager = NULL;
    }

  G_OBJECT_CLASS (os_scrollbar_parent_class)->dispose (object);
}

static void
os_scrollbar_finalize (GObject *object)
{
  OsScrollbar *scrollbar;
  OsScrollbarPrivate *priv;

  scrollbar = OS_SCROLLBAR (object);
  priv = scrollbar->priv;

  os_scrollbar_swap_adjustment (scrollbar, NULL);
  os_scrollbar_swap_thumb (scrollbar, NULL);

  G_OBJECT_CLASS (os_scrollbar_parent_class)->finalize (object);
}

static gboolean
os_scrollbar_expose_event (GtkWidget      *widget,
                           GdkEventExpose *event)
{
  return TRUE;
}

static void
os_scrollbar_grab_notify (GtkWidget *widget,
                          gboolean   was_grabbed)
{
}

static void
os_scrollbar_hide (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (g_type_class_peek (GTK_TYPE_WIDGET))->hide (widget);
}

static void
os_scrollbar_map (GtkWidget *widget)
{
  OsScrollbar *scrollbar;
  OsScrollbarPrivate *priv;
  gint x, y;

  scrollbar = OS_SCROLLBAR (widget);
  priv = scrollbar->priv;

  GTK_WIDGET_CLASS (g_type_class_peek (GTK_TYPE_WIDGET))->map (widget);

  priv->proximity = TRUE;

  gtk_widget_get_pointer (gtk_widget_get_toplevel (widget), &x, &y);

  pager_set_state_from_pointer (scrollbar, x, y);

  if (priv->fullsize == FALSE)
    os_pager_show (OS_PAGER (priv->pager));

  if (gtk_widget_get_realized (widget) && priv->filter == FALSE)
    {
      priv->filter = TRUE;
      gdk_window_add_filter (gtk_widget_get_window (widget), toplevel_filter_func, scrollbar);
    }
}

static void
os_scrollbar_realize (GtkWidget *widget)
{
  OsScrollbar *scrollbar;
  OsScrollbarPrivate *priv;

  scrollbar = OS_SCROLLBAR (widget);
  priv = scrollbar->priv;

  GTK_WIDGET_CLASS (g_type_class_peek (GTK_TYPE_WIDGET))->realize (widget);

  if (priv->filter == FALSE && priv->proximity == TRUE)
    {
      priv->filter =  TRUE;
      gdk_window_add_filter (gtk_widget_get_window (widget), toplevel_filter_func, scrollbar);
    }

  g_signal_connect (G_OBJECT (gtk_widget_get_toplevel (widget)), "configure-event",
                    G_CALLBACK (toplevel_configure_event_cb), scrollbar);
  g_signal_connect (G_OBJECT (gtk_widget_get_toplevel (widget)), "leave-notify-event",
                    G_CALLBACK (toplevel_leave_notify_event_cb), scrollbar);

  os_scrollbar_calc_layout_pager (scrollbar, priv->adjustment->value);

  os_pager_set_parent (OS_PAGER (priv->pager), widget);

  os_scrollbar_store_window_position (scrollbar);
}

static void
os_scrollbar_show (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (g_type_class_peek (GTK_TYPE_WIDGET))->show (widget);
}

static void
os_scrollbar_size_allocate (GtkWidget    *widget,
                            GdkRectangle *allocation)
{
  GdkRectangle rect;
  OsScrollbar *scrollbar;
  OsScrollbarPrivate *priv;

  scrollbar = OS_SCROLLBAR (widget);
  priv = scrollbar->priv;

  priv->trough.x = 0;
  priv->trough.y = 0;
  priv->trough.width = allocation->width;
  priv->trough.height = allocation->height;

  priv->overlay_all = *allocation;
  priv->thumb_all = *allocation;

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      priv->slider.width = DEFAULT_THUMB_WIDTH;
      priv->slider.height = DEFAULT_THUMB_HEIGHT;
      priv->overlay_all.x = allocation->x - DEFAULT_PAGER_WIDTH;
      priv->thumb_all.x = allocation->x;

      rect.x = priv->overlay_all.x;
      rect.y = priv->overlay_all.y;
      rect.width = DEFAULT_PAGER_WIDTH;
      rect.height = priv->overlay_all.height;

      allocation->width = 0;
    }
  else
    {
      priv->slider.width = DEFAULT_THUMB_HEIGHT;
      priv->slider.height = DEFAULT_THUMB_WIDTH;
      priv->overlay_all.y = allocation->y - DEFAULT_PAGER_WIDTH ;
      priv->thumb_all.y = allocation->y;

      rect.x = priv->overlay_all.x;
      rect.y = priv->overlay_all.y;
      rect.width = priv->overlay_all.width;
      rect.height = DEFAULT_PAGER_WIDTH;

      allocation->height = 0;
    }

  if (priv->adjustment != NULL)
    {
      os_scrollbar_calc_layout_pager (scrollbar, priv->adjustment->value);
      os_scrollbar_calc_layout_slider (scrollbar, priv->adjustment->value);
    }

  os_pager_size_allocate (OS_PAGER (priv->pager), rect);

  pager_move (scrollbar);

  if (gtk_widget_get_realized (widget))
    os_scrollbar_store_window_position (scrollbar);

  widget->allocation = *allocation;
}

static void
os_scrollbar_unmap (GtkWidget *widget)
{
  OsScrollbar *scrollbar;
  OsScrollbarPrivate *priv;

  scrollbar = OS_SCROLLBAR (widget);
  priv = scrollbar->priv;

  GTK_WIDGET_CLASS (g_type_class_peek (GTK_TYPE_WIDGET))->unmap (widget);

  priv->proximity = FALSE;

  os_pager_hide (OS_PAGER (priv->pager));

  gtk_widget_hide (priv->thumb);

  if (gtk_widget_get_realized (widget) && priv->filter == TRUE)
    {
      priv->filter = FALSE;
      gdk_window_remove_filter (gtk_widget_get_window (widget), toplevel_filter_func, scrollbar);
    }
}

static void
os_scrollbar_unrealize (GtkWidget *widget)
{
  OsScrollbar *scrollbar;
  OsScrollbarPrivate *priv;

  scrollbar = OS_SCROLLBAR (widget);
  priv = scrollbar->priv;

  gtk_widget_hide (priv->thumb);

  gdk_window_remove_filter (gtk_widget_get_window (widget), toplevel_filter_func, scrollbar);

  g_signal_handlers_disconnect_by_func (G_OBJECT (gtk_widget_get_toplevel (widget)),
                                        G_CALLBACK (toplevel_configure_event_cb), scrollbar);
  g_signal_handlers_disconnect_by_func (G_OBJECT (gtk_widget_get_toplevel (widget)),
                                        G_CALLBACK (toplevel_leave_notify_event_cb), scrollbar);

  os_pager_set_parent (OS_PAGER (priv->pager), NULL);

  GTK_WIDGET_CLASS (g_type_class_peek (GTK_TYPE_WIDGET))->unrealize (widget);
}

/* Public functions. */

/**
 * os_scrollbar_new:
 * @orientation: the #GtkOrientation
 * @adjustment: the pointer to the #GtkAdjustment to connect
 *
 * Creates a new scrollbar instance.
 *
 * Returns: the new scrollbar instance.
 */
GtkWidget*
os_scrollbar_new (GtkOrientation  orientation,
                  GtkAdjustment  *adjustment)
{
  return g_object_new (OS_TYPE_SCROLLBAR, "orientation", orientation,
                       "adjustment",  adjustment, NULL);
}
