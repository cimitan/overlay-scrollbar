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

/* Default size of the pager in pixels. */
#define DEFAULT_PAGER_WIDTH 3

/* Default size of the scrollbar in pixels. */
#define DEFAULT_SCROLLBAR_WIDTH  15
#define DEFAULT_SCROLLBAR_HEIGHT 80

/* Width of the proximity effect in pixels. */
#define PROXIMITY_WIDTH 40

/* Timeout before hiding in milliseconds. */
#define TIMEOUT_HIDE 1000

#define OS_SCROLLBAR_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), OS_TYPE_SCROLLBAR, OsScrollbarPrivate))

typedef struct _OsScrollbarPrivate OsScrollbarPrivate;

struct _OsScrollbarPrivate
{
  GdkRectangle trough;
  GdkRectangle overlay;
  GdkRectangle slider;
  GtkAllocation overlay_all;
  GtkAllocation thumb_all;
  GObject *pager;
  GtkWidget *thumb;
  GtkWidget *parent;
  GtkAdjustment *adjustment;
  GtkOrientation orientation;
  gboolean button_press_event;
  gboolean enter_notify_event;
  gboolean motion_notify_event;
  gboolean value_changed_event;
  gboolean toplevel_connected;
  gboolean fullsize;
  gboolean proximity;
  gboolean can_hide;
  gboolean can_rgba;
  gint win_x;
  gint win_y;
  gint slide_initial_slider_position;
  gint slide_initial_coordinate;
  gint pointer_x;
  gint pointer_y;
};

static gboolean os_scrollbar_expose_event (GtkWidget *widget, GdkEventExpose *event);
static void os_scrollbar_hide (GtkWidget *widget);
static void os_scrollbar_map (GtkWidget *widget);
static void os_scrollbar_parent_set (GtkWidget *widget, GtkWidget *old_parent);
static void os_scrollbar_realize (GtkWidget *widget);
static void os_scrollbar_show (GtkWidget *widget);
static void os_scrollbar_unmap (GtkWidget *widget);
static void os_scrollbar_unrealize (GtkWidget *widget);
static void os_scrollbar_dispose (GObject *object);
static void os_scrollbar_finalize (GObject *object);
static void os_scrollbar_calc_layout_pager (OsScrollbar *scrollbar, gdouble adjustment_value);
static void os_scrollbar_calc_layout_slider (OsScrollbar *scrollbar, gdouble adjustment_value);
static gdouble os_scrollbar_coord_to_value (OsScrollbar *scrollbar, gint coord);
static void os_scrollbar_hide_thumb (OsScrollbar *scrollbar);
static gboolean os_scrollbar_hide_thumb_cb (gpointer user_data);
static void os_scrollbar_move (OsScrollbar *scrollbar, gint mouse_x, gint mouse_y);
static void os_scrollbar_move_thumb (OsScrollbar *scrollbar, gint x, gint y);
static void os_scrollbar_notify_adjustment_cb (GObject *object, gpointer user_data);
static void os_scrollbar_notify_orientation_cb (GObject *object, gpointer user_data);
static gint os_scrollbar_sanitize_x (OsScrollbar *scrollbar, gint x);
static gint os_scrollbar_sanitize_y (OsScrollbar *scrollbar, gint y);
static void os_scrollbar_store_window_position (OsScrollbar *scrollbar);
static void os_scrollbar_swap_adjustment (OsScrollbar *scrollbar, GtkAdjustment *adjustment);
static void os_scrollbar_swap_parent (OsScrollbar *scrollbar, GtkWidget *parent);
static void os_scrollbar_swap_thumb (OsScrollbar *scrollbar, GtkWidget *thumb);
static void os_scrollbar_toplevel_connect (OsScrollbar *scrollbar);
static gboolean thumb_button_press_event_cb (GtkWidget *widget, GdkEventButton *event, gpointer user_data);
static gboolean thumb_button_release_event_cb (GtkWidget *widget, GdkEventButton *event, gpointer user_data);
static gboolean thumb_enter_notify_event_cb (GtkWidget *widget, GdkEventCrossing *event, gpointer user_data);
static gboolean thumb_leave_notify_event_cb (GtkWidget *widget, GdkEventCrossing *event, gpointer user_data);
static gboolean thumb_motion_notify_event_cb (GtkWidget *widget, GdkEventMotion *event, gpointer user_data);
static void pager_move (OsScrollbar *scrollbar);
static void pager_set_allocation (OsScrollbar *scrollbar);
static void adjustment_changed_cb (GtkAdjustment *adjustment, gpointer user_data);
static void adjustment_value_changed_cb (GtkAdjustment *adjustment, gpointer user_data);
static gboolean parent_expose_event_cb (GtkWidget *widget, GdkEventExpose *event, gpointer user_data);
static void parent_size_allocate_cb (GtkWidget *widget, GtkAllocation *allocation, gpointer user_data);
static gboolean toplevel_configure_event_cb (GtkWidget *widget, GdkEventConfigure *event, gpointer user_data);
static GdkFilterReturn toplevel_filter_func (GdkXEvent *gdkxevent, GdkEvent *event, gpointer user_data);
static gboolean toplevel_leave_notify_event_cb (GtkWidget *widget, GdkEventCrossing *event, gpointer user_data);

/* Private functions. */

/* Present a X11 window. */
static void
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

/* Present a GDK window. */
static void
os_present_gdk_window_with_timestamp (GtkWidget *widget,
                                      guint32    timestamp)
{
  os_present_window_with_timestamp (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                                    GDK_SCREEN_XSCREEN (gtk_widget_get_screen (widget)),
                                    GDK_WINDOW_XID (gtk_widget_get_window (widget)),
                                    timestamp);
}

/* Calculate layout and store info. */
static void
os_scrollbar_calc_layout_pager (OsScrollbar *scrollbar,
                                gdouble      adjustment_value)
{
  OsScrollbarPrivate *priv;

  priv = OS_SCROLLBAR_GET_PRIVATE (scrollbar);

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      gint tmp_height;
      gint y, bottom, top, height;

      tmp_height = priv->overlay.height;

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

      if (tmp_height != height)
        if (priv->pager != NULL)
          pager_move (scrollbar);
    }
  else
    {
      gint tmp_width;
      gint x, left, right, width;

      tmp_width = priv->overlay.width;

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

      if (tmp_width != width)
        if (priv->pager != NULL)
          pager_move (scrollbar);
    }
}

/* Calculate OsScrollbar layout and store info. */
static void
os_scrollbar_calc_layout_slider (OsScrollbar *scrollbar,
                                 gdouble      adjustment_value)
{
  OsScrollbarPrivate *priv;

  priv = OS_SCROLLBAR_GET_PRIVATE (scrollbar);

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

  priv = OS_SCROLLBAR_GET_PRIVATE (scrollbar);

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

/* Hide if it's ok to hide. */
static void
os_scrollbar_hide_thumb (OsScrollbar *scrollbar)
{
  OsScrollbarPrivate *priv;

  priv = OS_SCROLLBAR_GET_PRIVATE (scrollbar);

  if (priv->can_hide)
    {
      priv->value_changed_event = FALSE;
      gtk_widget_hide (GTK_WIDGET (priv->thumb));
    }
}

static gboolean
os_scrollbar_hide_thumb_cb (gpointer user_data)
{
  OsScrollbar *scrollbar = OS_SCROLLBAR (user_data);

  os_scrollbar_hide_thumb (scrollbar);
  g_object_unref (scrollbar);

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

  priv = OS_SCROLLBAR_GET_PRIVATE (scrollbar);

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

  priv = OS_SCROLLBAR_GET_PRIVATE (scrollbar);

  gtk_window_move (GTK_WINDOW (priv->thumb),
                   os_scrollbar_sanitize_x (scrollbar, x),
                   os_scrollbar_sanitize_y (scrollbar, y));
}

static void
os_scrollbar_notify_adjustment_cb (GObject *object,
                                   gpointer user_data)
{
  OsScrollbarPrivate *priv;

  priv = OS_SCROLLBAR_GET_PRIVATE (OS_SCROLLBAR (object));

  os_scrollbar_swap_adjustment (OS_SCROLLBAR (object), gtk_range_get_adjustment (GTK_RANGE (object)));
}

static void
os_scrollbar_notify_orientation_cb (GObject *object,
                                    gpointer user_data)
{
  OsScrollbarPrivate *priv;

  priv = OS_SCROLLBAR_GET_PRIVATE (OS_SCROLLBAR (object));

  priv->orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (object));

  os_scrollbar_swap_thumb (OS_SCROLLBAR (object), os_thumb_new (priv->orientation));
}

static gint
os_scrollbar_sanitize_x (OsScrollbar *scrollbar,
                         gint         x)
{
  OsScrollbarPrivate *priv;
  gint screen_width;

  priv = OS_SCROLLBAR_GET_PRIVATE (scrollbar);

  /* FIXME we could store screen_width in priv
   * and change it at screen-changed signal */
  screen_width = gdk_screen_get_width (gtk_widget_get_screen (GTK_WIDGET (scrollbar)));

  /* FIXME we could apply a static offest we
   * set in size-allocate and configure-event */
  if (priv->orientation == GTK_ORIENTATION_VERTICAL &&
      (x + priv->slider.width > screen_width))
    return x - DEFAULT_PAGER_WIDTH - priv->slider.width;

  return x;
}

static gint
os_scrollbar_sanitize_y (OsScrollbar *scrollbar,
                         gint         y)
{
  OsScrollbarPrivate *priv;
  gint screen_height;

  priv = OS_SCROLLBAR_GET_PRIVATE (scrollbar);

  /* FIXME we could store screen_height in priv
   * and change it at screen-changed signal */
  screen_height = gdk_screen_get_height (gtk_widget_get_screen (GTK_WIDGET (scrollbar)));

  /* FIXME we could apply a static offest we
   * set in size-allocate and configure-event */
  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL &&
      (y + priv->slider.height > screen_height))
    return y - DEFAULT_PAGER_WIDTH - priv->slider.height;

  return y;
}

/* Store scrollbar window position. */
static void
os_scrollbar_store_window_position (OsScrollbar *scrollbar)
{
  OsScrollbarPrivate *priv;
  gint win_x, win_y;

  priv = OS_SCROLLBAR_GET_PRIVATE (scrollbar);

  if (GDK_IS_WINDOW (gtk_widget_get_window (priv->parent)))
    gdk_window_get_position (gtk_widget_get_window (priv->parent), &win_x, &win_y);

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
  OsScrollbarPrivate *priv = OS_SCROLLBAR_GET_PRIVATE (scrollbar);

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

/* Swap the parent pointer. */
static void
os_scrollbar_swap_parent (OsScrollbar *scrollbar,
                          GtkWidget   *parent)
{
  OsScrollbarPrivate *priv = OS_SCROLLBAR_GET_PRIVATE (scrollbar);

  if (priv->parent != NULL)
    {
      g_signal_handlers_disconnect_by_func (G_OBJECT (priv->parent),
                                            G_CALLBACK (parent_expose_event_cb), scrollbar);
      g_signal_handlers_disconnect_by_func (G_OBJECT (priv->parent),
                                            G_CALLBACK (parent_size_allocate_cb), scrollbar);

      g_object_unref (priv->parent);
    }

  priv->parent = parent;

  if (priv->parent != NULL)
    {
      g_object_ref_sink (priv->parent);

      g_signal_connect (G_OBJECT (priv->parent), "expose-event",
                        G_CALLBACK (parent_expose_event_cb), scrollbar);
      g_signal_connect (G_OBJECT (priv->parent), "size-allocate",
                        G_CALLBACK (parent_size_allocate_cb), scrollbar);
    }
}

/* Swap the thumb pointer. */
static void
os_scrollbar_swap_thumb (OsScrollbar *scrollbar,
                         GtkWidget   *thumb)
{
  OsScrollbarPrivate *priv = OS_SCROLLBAR_GET_PRIVATE (scrollbar);

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
                                            thumb_motion_notify_event_cb, scrollbar);

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
      g_signal_connect (G_OBJECT (priv->thumb), "motion-notify-event",
                        G_CALLBACK (thumb_motion_notify_event_cb), scrollbar);
    }
}

/* Create elements. */
/* FIXME(Cimi): Needs to be improved. */
static void
os_scrollbar_toplevel_connect (OsScrollbar *scrollbar)
{
  OsScrollbarPrivate *priv;
  gint x_pos, y_pos;

  priv = OS_SCROLLBAR_GET_PRIVATE (OS_SCROLLBAR (scrollbar));

  g_return_if_fail (priv->parent != NULL);
  g_return_if_fail (GDK_IS_WINDOW (gtk_widget_get_window (priv->parent)));

  gdk_window_add_filter (gtk_widget_get_window (priv->parent), toplevel_filter_func, scrollbar);
  g_signal_connect (G_OBJECT (gtk_widget_get_toplevel (priv->parent)), "configure-event",
                    G_CALLBACK (toplevel_configure_event_cb), scrollbar);
  g_signal_connect (G_OBJECT (gtk_widget_get_toplevel (priv->parent)), "leave-notify-event",
                    G_CALLBACK (toplevel_leave_notify_event_cb), scrollbar);
  priv->toplevel_connected = TRUE;

  gdk_window_get_position (gtk_widget_get_window (priv->parent), &x_pos, &y_pos);

  os_scrollbar_calc_layout_pager (scrollbar, priv->adjustment->value);

  os_scrollbar_move_thumb (scrollbar, x_pos + priv->thumb_all.x, y_pos + priv->thumb_all.y);

  if (priv->pager != NULL)
    {
      g_object_unref (priv->pager);
      priv->pager = NULL;
    }

  priv->pager = os_pager_new (priv->parent);
  os_pager_show (OS_PAGER (priv->pager));

  os_scrollbar_store_window_position (scrollbar);
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
          priv = OS_SCROLLBAR_GET_PRIVATE (OS_SCROLLBAR (scrollbar));

/*          os_scrollbar_map (widget);*/
          gtk_window_set_transient_for (GTK_WINDOW (widget), GTK_WINDOW (gtk_widget_get_toplevel (priv->parent)));
          os_present_gdk_window_with_timestamp (priv->parent, event->time);

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

          gtk_widget_queue_draw (widget);
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
          priv = OS_SCROLLBAR_GET_PRIVATE (OS_SCROLLBAR (scrollbar));

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

          gtk_widget_queue_draw (widget);
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
  priv = OS_SCROLLBAR_GET_PRIVATE (OS_SCROLLBAR (scrollbar));

  priv->enter_notify_event = TRUE;
  priv->can_hide = FALSE;

  return TRUE;
}

static gboolean
thumb_leave_notify_event_cb (GtkWidget        *widget,
                             GdkEventCrossing *event,
                             gpointer          user_data)
{
  OsScrollbar *scrollbar;
  OsScrollbarPrivate *priv;

  scrollbar = OS_SCROLLBAR (user_data);
  priv = OS_SCROLLBAR_GET_PRIVATE (OS_SCROLLBAR (scrollbar));

  if (!priv->button_press_event)
    priv->can_hide = TRUE;

  g_timeout_add (TIMEOUT_HIDE, os_scrollbar_hide_thumb_cb,
                 g_object_ref (scrollbar));

  return TRUE;
}

static gboolean
thumb_motion_notify_event_cb (GtkWidget      *widget,
                              GdkEventMotion *event,
                              gpointer        user_data)
{
  OsScrollbar *scrollbar;
  OsScrollbarPrivate *priv;

  scrollbar = OS_SCROLLBAR (user_data);
  priv = OS_SCROLLBAR_GET_PRIVATE (OS_SCROLLBAR (scrollbar));

  /* XXX improve speed by not rendering when moving */
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

      if (!priv->motion_notify_event)
        gtk_widget_queue_draw (widget);

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

  return TRUE;
}

/* Move the pager to the right position. */
static void
pager_move (OsScrollbar *scrollbar)
{
  GdkRectangle mask;
  OsScrollbarPrivate *priv;

  priv = OS_SCROLLBAR_GET_PRIVATE (scrollbar);

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

/* Resize the overlay window. */
static void
pager_set_allocation (OsScrollbar *scrollbar)
{
  GdkRectangle rect;
  OsScrollbarPrivate *priv;

  priv = OS_SCROLLBAR_GET_PRIVATE (scrollbar);

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      rect.x = priv->overlay_all.x;
      rect.y = priv->overlay_all.y + 1;
      rect.width = DEFAULT_PAGER_WIDTH;
      rect.height = priv->overlay_all.height - 2;
    }
  else
    {
      rect.x = priv->overlay_all.x + 1;
      rect.y = priv->overlay_all.y;
      rect.width = priv->overlay_all.width - 2;
      rect.height = DEFAULT_PAGER_WIDTH;
    }

  os_pager_size_allocate (OS_PAGER (priv->pager), rect);
}

static void
adjustment_changed_cb (GtkAdjustment *adjustment,
                       gpointer       user_data)
{
  OsScrollbar *scrollbar;
  OsScrollbarPrivate *priv;

  scrollbar = OS_SCROLLBAR (user_data);
  priv = OS_SCROLLBAR_GET_PRIVATE (scrollbar);

  /* FIXME(Cimi) we should control each time os_pager_show()/hide()
   * is called here and in map()/unmap().
   * We are arbitrary calling that and I'm frightened we should show or keep
   * hidden a pager that is meant to be hidden/shown.
   * I don't want to see pagers reappearing because
   * of a change in the adjustment of an invisible pager or viceversa. */
  if ((adjustment->upper - adjustment->lower) > adjustment->page_size)
    {
      priv->fullsize = FALSE;
      if (priv->pager != NULL && priv->proximity != FALSE)
        os_pager_show (OS_PAGER (priv->pager));
    }
  else
    {
      priv->fullsize = TRUE;
      if (priv->pager != NULL && priv->proximity != FALSE)
        os_pager_hide (OS_PAGER (priv->pager));
    }
}

static void
adjustment_value_changed_cb (GtkAdjustment *adjustment,
                             gpointer       user_data)
{
  OsScrollbar *scrollbar;
  OsScrollbarPrivate *priv;

  scrollbar = OS_SCROLLBAR (user_data);
  priv = OS_SCROLLBAR_GET_PRIVATE (scrollbar);

  os_scrollbar_calc_layout_pager (scrollbar, adjustment->value);
  os_scrollbar_calc_layout_slider (scrollbar, adjustment->value);

  if (!priv->motion_notify_event && !priv->enter_notify_event)
    gtk_widget_hide (GTK_WIDGET (priv->thumb));

  pager_move (scrollbar);
}

static gboolean
parent_expose_event_cb (GtkWidget      *widget,
                        GdkEventExpose *event,
                        gpointer        user_data)
{
  OsScrollbar *scrollbar;
  OsScrollbarPrivate *priv;

  scrollbar = OS_SCROLLBAR (user_data);
  priv = OS_SCROLLBAR_GET_PRIVATE (scrollbar);

  if (!priv->toplevel_connected)
    {
      os_scrollbar_toplevel_connect (scrollbar);
    }

  return FALSE;
}

static void
parent_size_allocate_cb (GtkWidget     *widget,
                         GtkAllocation *allocation,
                         gpointer       user_data)
{
  OsScrollbar *scrollbar;
  OsScrollbarPrivate *priv;

  scrollbar = OS_SCROLLBAR (user_data);
  priv = OS_SCROLLBAR_GET_PRIVATE (scrollbar);

  priv->trough.x = 0;
  priv->trough.y = 0;
  priv->trough.width = allocation->width;
  priv->trough.height = allocation->height;

  priv->overlay_all = *allocation;
  priv->thumb_all = *allocation;

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      priv->slider.width = DEFAULT_SCROLLBAR_WIDTH;
      priv->slider.height = DEFAULT_SCROLLBAR_HEIGHT;
      priv->overlay_all.x = allocation->x + allocation->width - DEFAULT_PAGER_WIDTH;
      priv->thumb_all.x = allocation->x + allocation->width;
    }
  else
    {
      priv->slider.width = DEFAULT_SCROLLBAR_HEIGHT;
      priv->slider.height = DEFAULT_SCROLLBAR_WIDTH;
      priv->overlay_all.y = allocation->y + allocation->height - DEFAULT_PAGER_WIDTH;
      priv->thumb_all.y = allocation->y + allocation->height;
    }

  if (priv->adjustment != NULL)
    os_scrollbar_calc_layout_pager (scrollbar, priv->adjustment->value);

  if (priv->pager != NULL)
    pager_set_allocation (scrollbar);

  os_scrollbar_store_window_position (scrollbar);
}

static gboolean
toplevel_configure_event_cb (GtkWidget         *widget,
                             GdkEventConfigure *event,
                             gpointer           user_data)
{
  OsScrollbar *scrollbar;
  OsScrollbarPrivate *priv;

  scrollbar = OS_SCROLLBAR (user_data);
  priv = OS_SCROLLBAR_GET_PRIVATE (scrollbar);

  gtk_widget_hide (GTK_WIDGET (priv->thumb));

  os_scrollbar_calc_layout_pager (scrollbar, priv->adjustment->value);
  os_scrollbar_calc_layout_slider (scrollbar, priv->adjustment->value);
  os_scrollbar_move_thumb (scrollbar,
                           event->x + priv->thumb_all.x + priv->slider.x,
                           event->y + priv->thumb_all.y + priv->slider.y);

  os_scrollbar_store_window_position (scrollbar);

  pager_set_allocation (scrollbar);
  pager_move (scrollbar);

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

  priv = OS_SCROLLBAR_GET_PRIVATE (scrollbar);
  xevent = gdkxevent;

  g_return_val_if_fail (priv->parent != NULL, GDK_FILTER_CONTINUE);
  g_return_val_if_fail (priv->pager != NULL, GDK_FILTER_CONTINUE);
  g_return_val_if_fail (priv->thumb != NULL, GDK_FILTER_CONTINUE);

  if (priv->proximity && !priv->fullsize)
    {
      /* get the motion_notify_event trough XEvent */
      if (xevent->type == MotionNotify)
        {
          os_scrollbar_calc_layout_pager (scrollbar, priv->adjustment->value);
          os_scrollbar_calc_layout_slider (scrollbar, priv->adjustment->value);

          /* XXX missing horizontal */
          /* proximity area */
          if (priv->orientation == GTK_ORIENTATION_VERTICAL)
            {
              if ((priv->thumb_all.x - xevent->xmotion.x <= PROXIMITY_WIDTH &&
                   priv->thumb_all.x - xevent->xmotion.x >= 0) &&
                  (xevent->xmotion.y >= priv->thumb_all.y + priv->overlay.y &&
                   xevent->xmotion.y <= priv->thumb_all.y + priv->overlay.y + priv->overlay.height))
                {
                  priv->can_hide = FALSE;

                  if (priv->overlay.height > priv->slider.height)
                    {
                      gint x, y, x_pos, y_pos;

                      gdk_window_get_position (gtk_widget_get_window (priv->parent), &x_pos, &y_pos);

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
/*                  os_scrollbar_map (GTK_WIDGET (scrollbar));*/
                }
              else
                {
                  priv->can_hide = TRUE;
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

                  if (priv->overlay.width > priv->slider.width)
                    {
                      gint x, y, x_pos, y_pos;

                      gdk_window_get_position (gtk_widget_get_window (priv->parent), &x_pos, &y_pos);

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
/*                  os_scrollbar_map (GTK_WIDGET (scrollbar));*/
                }
              else
                {
                  priv->can_hide = TRUE;
                  os_scrollbar_hide_thumb (scrollbar);
                }
            }
        }
    }
    else
    {
      os_pager_hide (OS_PAGER (priv->pager));
    }

  /* code to check if the window is active */
  if (xevent->type == PropertyNotify)
    {
      GdkScreen *screen;
      GdkWindow *active_window;

      screen = gtk_widget_get_screen (priv->parent);

      active_window = gdk_screen_get_active_window (screen);

      if (active_window != gtk_widget_get_window (priv->parent))
        {
          gtk_widget_hide (GTK_WIDGET (priv->thumb));
          os_pager_set_active (OS_PAGER (priv->pager), FALSE);
        }
      else
        os_pager_set_active (OS_PAGER (priv->pager), TRUE);
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
  priv = OS_SCROLLBAR_GET_PRIVATE (scrollbar);

  g_timeout_add (TIMEOUT_HIDE, os_scrollbar_hide_thumb_cb,
                 g_object_ref (scrollbar));

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

  widget_class->expose_event = os_scrollbar_expose_event;
  widget_class->hide         = os_scrollbar_hide;
  widget_class->map          = os_scrollbar_map;
  widget_class->realize      = os_scrollbar_realize;
  widget_class->parent_set   = os_scrollbar_parent_set;
  widget_class->show         = os_scrollbar_show;
  widget_class->unmap        = os_scrollbar_unmap;
  widget_class->unrealize    = os_scrollbar_unrealize;

  gobject_class->dispose  = os_scrollbar_dispose;
  gobject_class->finalize = os_scrollbar_finalize;

  g_type_class_add_private (gobject_class, sizeof (OsScrollbarPrivate));
}

static void
os_scrollbar_init (OsScrollbar *scrollbar)
{
  OsScrollbarPrivate *priv;

  priv = OS_SCROLLBAR_GET_PRIVATE (scrollbar);

  priv->can_hide = TRUE;
  priv->can_rgba = FALSE;
  priv->fullsize = FALSE;
  priv->proximity = FALSE;

  g_signal_connect (G_OBJECT (scrollbar), "notify::adjustment",
                    G_CALLBACK (os_scrollbar_notify_adjustment_cb), NULL);

  g_signal_connect (G_OBJECT (scrollbar), "notify::orientation",
                    G_CALLBACK (os_scrollbar_notify_orientation_cb), NULL);
}

static void
os_scrollbar_dispose (GObject *object)
{
  G_OBJECT_CLASS (os_scrollbar_parent_class)->dispose (object);
}

static void
os_scrollbar_finalize (GObject *object)
{
  OsScrollbarPrivate *priv = OS_SCROLLBAR_GET_PRIVATE (object);

  os_scrollbar_swap_adjustment (OS_SCROLLBAR (object), NULL);
  os_scrollbar_swap_parent (OS_SCROLLBAR (object), NULL);
  os_scrollbar_swap_thumb (OS_SCROLLBAR (object), NULL);

  if (priv->pager != NULL)
    {
      g_object_unref (priv->pager);
      priv->pager = NULL;
    }

  G_OBJECT_CLASS (os_scrollbar_parent_class)->finalize (object);
}

static gboolean
os_scrollbar_expose_event (GtkWidget      *widget,
                           GdkEventExpose *event)
{
  return TRUE;
}

static void
os_scrollbar_hide (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (os_scrollbar_parent_class)->hide (widget);
}

static void
os_scrollbar_map (GtkWidget *widget)
{
  OsScrollbarPrivate *priv;

  priv = OS_SCROLLBAR_GET_PRIVATE (OS_SCROLLBAR (widget));

  GTK_WIDGET_CLASS (os_scrollbar_parent_class)->map (widget);

  priv->proximity = TRUE;

  if (priv->pager != NULL)
    os_pager_show (OS_PAGER (priv->pager));

#if 0
  Display *display;
  GtkWidget *parent;
  XWindowChanges changes;
  guint32 xid, xid_parent;
  unsigned int value_mask = CWSibling | CWStackMode;
  int res;

  parent = gtk_widget_get_parent (widget);

  xid = GDK_WINDOW_XID (gtk_widget_get_window (widget));
  xid_parent = GDK_WINDOW_XID (gtk_widget_get_window (parent));
  display = GDK_WINDOW_XDISPLAY (gtk_widget_get_window (widget));

  changes.sibling = xid_parent;
  changes.stack_mode = Above;

  gdk_error_trap_push ();
  XConfigureWindow (display, xid, value_mask, &changes);

  gdk_flush ();
  if ((res = gdk_error_trap_pop ()))
    {
      XEvent event;
      Window xroot = gdk_x11_get_default_root_xwindow ();

      /* Synthetic ConfigureRequest (so it looks to the window manager
       * like a normal ConfigureRequest) so it can handle that
       * and *actually* Configure the window without errors
       */
      event.type = ConfigureRequest;
      
      /* The WM will know the event is synthetic since the send_event
       * field is always set */
      event.xconfigurerequest.window = xid;
      event.xconfigurerequest.parent = xid_parent;
      event.xconfigurerequest.detail = changes.stack_mode;
      event.xconfigurerequest.above = changes.sibling;
      event.xconfigurerequest.value_mask = value_mask;

      /* Sends the event to the root window (which the WM has the Selection
       * on) so now Compiz will get a ConfigureRequest for the scrollbar
       * to stack relative to the reparented window */
      XSendEvent (display, xroot, FALSE, SubstructureRedirectMask | SubstructureNotifyMask, &event);

      g_warning ("Received X error: %d, working around\n", res);
    }
#endif
}

static void
os_scrollbar_parent_set (GtkWidget *widget,
                         GtkWidget *old_parent)
{
  os_scrollbar_swap_parent (OS_SCROLLBAR (widget), gtk_widget_get_parent (widget));
}

static void
os_scrollbar_realize (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (os_scrollbar_parent_class)->realize (widget);
}

static void
os_scrollbar_show (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (os_scrollbar_parent_class)->show (widget);
}

static void
os_scrollbar_unmap (GtkWidget *widget)
{
  OsScrollbarPrivate *priv;

  priv = OS_SCROLLBAR_GET_PRIVATE (OS_SCROLLBAR (widget));

  GTK_WIDGET_CLASS (os_scrollbar_parent_class)->unmap (widget);

  priv->proximity = FALSE;

  if (priv->pager != NULL)
    os_pager_hide (OS_PAGER (priv->pager));
}

static void
os_scrollbar_unrealize (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (os_scrollbar_parent_class)->unrealize (widget);
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
