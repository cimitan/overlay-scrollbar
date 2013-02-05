/* overlay-scrollbar
 *
 * Copyright © 2011 Canonical Ltd
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation.
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

#include <math.h>
#include <stdlib.h>
#include <gdk/gdkx.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/XInput2.h>

/* Size of the bar in pixels. */
#define BAR_SIZE 3

/* Size of the proximity effect in pixels. */
#define PROXIMITY_SIZE 34

/* Max duration of the scrolling. */
#define MAX_DURATION_SCROLLING 1000

/* Min duration of the scrolling. */
#define MIN_DURATION_SCROLLING 250

/* Modifier key used to slow down actions. */
#define MODIFIER_KEY GDK_CONTROL_MASK

/* Timeout assumed for PropertyNotify _NET_ACTIVE_WINDOW event. */
#define TIMEOUT_PRESENT_WINDOW 400

/* Timeout before hiding in ms, after leaving the proximity area. */
#define TIMEOUT_PROXIMITY_HIDE 200

/* Timeout before hiding in ms, after leaving the thumb. */
#define TIMEOUT_THUMB_HIDE 200

/* Timeout before showing in ms, after entering the proximity. */
#define TIMEOUT_THUMB_SHOW 100

/* Timeout before hiding in ms, after leaving the toplevel. */
#define TIMEOUT_TOPLEVEL_HIDE 200

typedef enum {
  OS_SCROLL_PAGE,
  OS_SCROLL_STEP
} OsScrollType;

typedef enum
{
  OS_SIDE_TOP, /* Scrollbar is at top. */
  OS_SIDE_BOTTOM, /* Scrollbar is at bottom. */
  OS_SIDE_LEFT, /* Scrollbar is at left. */
  OS_SIDE_RIGHT /* Scrollbar is at right. */
} OsSide;

typedef enum
{
  OS_STATE_NONE = 0, /* No state. */
  OS_STATE_CONNECTED = 1, /* Thumb and bar move connected, like a native scrollbar. */
  OS_STATE_DETACHED = 2, /* The thumb is visually detached from the bar, and you can see the tail. */
  OS_STATE_FINE_SCROLL = 4, /* A fine scroll is currently running, the modifier key must be pressed. */
  OS_STATE_FULLSIZE = 8, /* The scrollbar is fullsize, so we hide it. */
  OS_STATE_INTERNAL = 16, /* The thumb is touching a strut or a screen edge, it's internal. */
  OS_STATE_LOCKED = 32, /* Thumb is locked in its position when moving in the proximity area. */
  OS_STATE_RECONNECTING = 64 /* The thumb is reconnecting with the bar, there's likely an animation in progress. */
} OsStateFlags;

typedef enum
{
  OS_STRUT_SIDE_NONE = 0, /* No strut. */
  OS_STRUT_SIDE_TOP = 1, /* Strut at top. */
  OS_STRUT_SIDE_BOTTOM = 2, /* Strut at bottom. */
  OS_STRUT_SIDE_LEFT = 4, /* Strut at left. */
  OS_STRUT_SIDE_RIGHT = 8 /* Strut at right. */
} OsStrutSideFlags;

typedef struct
{
  gboolean proximity;
  gboolean running;
} OsWindowFilter;

typedef struct
{
  GdkRectangle overlay;
  GdkRectangle slider;
  GdkRectangle trough;
  GtkAllocation bar_all;
  GtkAllocation thumb_all;
  GtkAdjustment *adjustment;
  GtkOrientation orientation;
  GtkWidget *thumb;
  GtkWindowGroup *window_group;
  OsAnimation *animation;
  OsBar *bar;
  OsCoordinate pointer;
  OsCoordinate thumb_win;
  OsEventFlags event;
  OsStateFlags state;
  OsSide side;
  OsWindowFilter filter;
  gboolean active_window;
  gboolean allow_resize;
  gboolean allow_resize_paned;
  gboolean resizing_paned;
#ifdef USE_GTK3
  gboolean deactivable_bar;
#endif
  gboolean hidable_thumb;
  gboolean window_button_press; /* FIXME(Cimi) to replace with X11 input events. */
  gdouble value;
  gfloat fine_scroll_multiplier;
  gfloat slide_initial_slider_position;
  gfloat slide_initial_coordinate;
  gint64 present_time;
  guint32 source_deactivate_bar_id;
  guint32 source_hide_thumb_id;
  guint32 source_show_thumb_id;
  guint32 source_unlock_thumb_id;
} OsScrollbarPrivate;

#ifdef USE_GTK3
static GdkInputSource os_input_source = GDK_SOURCE_MOUSE;
static gint os_device_id = 0;
static GtkCssProvider *provider = NULL;
#endif
static Atom net_active_window_atom = None;
static Atom unity_net_workarea_region_atom = None;
static GSList *os_root_list = NULL;
static GSList *scrollbar_list = NULL;
static GQuark os_quark_placement = 0;
static GQuark os_quark_qdata = 0;
static ScrollbarMode scrollbar_mode = SCROLLBAR_MODE_NORMAL;
static cairo_region_t *os_workarea = NULL;

static void adjustment_changed_cb (GtkAdjustment *adjustment, gpointer user_data);
static void adjustment_value_changed_cb (GtkAdjustment *adjustment, gpointer user_data);
static OsScrollbarPrivate* get_private (GtkWidget *widget);
static void notify_adjustment_cb (GObject *object, gpointer user_data);
static void notify_orientation_cb (GObject *object, gpointer user_data);
static GdkFilterReturn root_filter_func (GdkXEvent *gdkxevent, GdkEvent *event, gpointer user_data);
static void scrolling_cb (gfloat weight, gpointer user_data);
static void scrolling_end_cb (gpointer user_data);
static void swap_adjustment (GtkScrollbar *scrollbar, GtkAdjustment *adjustment);
static void swap_thumb (GtkScrollbar *scrollbar, GtkWidget *thumb);
static gboolean thumb_button_press_event_cb (GtkWidget *widget, GdkEventButton *event, gpointer user_data);
static gboolean thumb_button_release_event_cb (GtkWidget *widget, GdkEventButton *event, gpointer user_data);
static gboolean thumb_enter_notify_event_cb (GtkWidget *widget, GdkEventCrossing *event, gpointer user_data);
static gboolean thumb_leave_notify_event_cb (GtkWidget *widget, GdkEventCrossing *event, gpointer user_data);
static void thumb_map_cb (GtkWidget *widget, gpointer user_data);
static gboolean thumb_motion_notify_event_cb (GtkWidget *widget, GdkEventMotion *event, gpointer user_data);
static gboolean thumb_scroll_event_cb (GtkWidget *widget, GdkEventScroll *event, gpointer user_data);
static void thumb_unmap_cb (GtkWidget *widget, gpointer user_data);

/* GtkScrollbar vfunc pointers. */
#ifdef USE_GTK3
static gboolean (* pre_hijacked_scrollbar_draw) (GtkWidget *widget, cairo_t *cr);
static void (* pre_hijacked_scrollbar_get_preferred_width) (GtkWidget *widget, gint *minimal_width, gint *natural_width);
static void (* pre_hijacked_scrollbar_get_preferred_height) (GtkWidget *widget, gint *minimal_height, gint *natural_height);
static void (* pre_hijacked_scrollbar_state_flags_changed) (GtkWidget *widget, GtkStateFlags flags);
#else
static gboolean (* pre_hijacked_scrollbar_expose_event) (GtkWidget *widget, GdkEventExpose *event);
static void (* pre_hijacked_scrollbar_size_request) (GtkWidget *widget, GtkRequisition *requisition);
static void (* pre_hijacked_scrollbar_state_changed) (GtkWidget *widget, GtkStateType state);
#endif
static void (* pre_hijacked_scrollbar_grab_notify) (GtkWidget *widget, gboolean was_grabbed);
static void (* pre_hijacked_scrollbar_hide) (GtkWidget *widget);
static void (* pre_hijacked_scrollbar_map) (GtkWidget *widget);
static void (* pre_hijacked_scrollbar_realize) (GtkWidget *widget);
static void (* pre_hijacked_scrollbar_show) (GtkWidget *widget);
static void (* pre_hijacked_scrollbar_size_allocate) (GtkWidget *widget, GdkRectangle *allocation);
static void (* pre_hijacked_scrollbar_unmap) (GtkWidget *widget);
static void (* pre_hijacked_scrollbar_unrealize) (GtkWidget *widget);
static void (* pre_hijacked_scrollbar_dispose) (GObject *object);

/* Hijacked GtkScrollbar vfunc pointers. */
#ifdef USE_GTK3
static gboolean hijacked_scrollbar_draw (GtkWidget *widget, cairo_t *cr);
static void hijacked_scrollbar_get_preferred_width (GtkWidget *widget, gint *minimal_width, gint *natural_width);
static void hijacked_scrollbar_get_preferred_height (GtkWidget *widget, gint *minimal_height, gint *natural_height);
static void hijacked_scrollbar_state_flags_changed (GtkWidget *widget, GtkStateFlags flags);
#else
static gboolean hijacked_scrollbar_expose_event (GtkWidget *widget, GdkEventExpose *event);
static void hijacked_scrollbar_size_request (GtkWidget *widget, GtkRequisition *requisition);
static void hijacked_scrollbar_state_changed (GtkWidget *widget, GtkStateType state);
#endif
static void hijacked_scrollbar_grab_notify (GtkWidget *widget, gboolean was_grabbed);
static void hijacked_scrollbar_hide (GtkWidget *widget);
static void hijacked_scrollbar_map (GtkWidget *widget);
static void hijacked_scrollbar_realize (GtkWidget *widget);
static void hijacked_scrollbar_show (GtkWidget *widget);
static void hijacked_scrollbar_size_allocate (GtkWidget *widget, GdkRectangle *allocation);
static void hijacked_scrollbar_unmap (GtkWidget *widget);
static void hijacked_scrollbar_unrealize (GtkWidget *widget);

/* GtkWidget vfunc pointers. */
static void (* widget_class_hide) (GtkWidget *widget);
static void (* widget_class_map) (GtkWidget *widget);
static void (* widget_class_realize) (GtkWidget *widget);
static void (* widget_class_show) (GtkWidget *widget);
static void (* widget_class_unmap) (GtkWidget *widget);
static void (* widget_class_unrealize) (GtkWidget *widget);

/* Calculate bar layout info. */
static void
calc_layout_bar (GtkScrollbar *scrollbar,
                 gdouble       adjustment_value)
{
  OsScrollbarPrivate *priv;

  priv = get_private (GTK_WIDGET (scrollbar));

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      gint y, trough_length, height;

      y = 0;
      trough_length = priv->trough.height;

      if (gtk_adjustment_get_upper (priv->adjustment) - gtk_adjustment_get_lower (priv->adjustment) != 0)
        height = (trough_length * (gtk_adjustment_get_page_size (priv->adjustment) /
                                   (gtk_adjustment_get_upper (priv->adjustment) - 
                                    gtk_adjustment_get_lower (priv->adjustment))));
      else
        height = gtk_range_get_min_slider_size (GTK_RANGE (scrollbar));

      height = MAX (height, gtk_range_get_min_slider_size (GTK_RANGE (scrollbar)));

      if (gtk_adjustment_get_upper (priv->adjustment) -
          gtk_adjustment_get_lower (priv->adjustment) -
          gtk_adjustment_get_page_size (priv->adjustment) != 0)
        y = (trough_length - height) * ((adjustment_value - gtk_adjustment_get_lower (priv->adjustment)) /
                                        (gtk_adjustment_get_upper (priv->adjustment) -
                                         gtk_adjustment_get_lower (priv->adjustment) -
                                         gtk_adjustment_get_page_size (priv->adjustment)));

      y = CLAMP (y, 0, trough_length);

      priv->overlay.y = y;
      priv->overlay.height = height;
    }
  else
    {
      gint x, trough_length, width;

      x = 0;
      trough_length = priv->trough.width;

      if (gtk_adjustment_get_upper (priv->adjustment) - gtk_adjustment_get_lower (priv->adjustment) != 0)
        width = (trough_length * (gtk_adjustment_get_page_size (priv->adjustment) /
                                  (gtk_adjustment_get_upper (priv->adjustment) -
                                   gtk_adjustment_get_lower (priv->adjustment))));
      else
        width =  gtk_range_get_min_slider_size (GTK_RANGE (scrollbar));

      width = MAX (width, gtk_range_get_min_slider_size (GTK_RANGE (scrollbar)));

      if (gtk_adjustment_get_upper (priv->adjustment) -
          gtk_adjustment_get_lower (priv->adjustment) -
          gtk_adjustment_get_page_size (priv->adjustment) != 0)
        x = (trough_length - width) * ((adjustment_value - gtk_adjustment_get_lower (priv->adjustment)) /
                                       (gtk_adjustment_get_upper (priv->adjustment) -
                                        gtk_adjustment_get_lower (priv->adjustment) -
                                        gtk_adjustment_get_page_size (priv->adjustment)));

      x = CLAMP (x, 0, trough_length);

      priv->overlay.x = x;
      priv->overlay.width = width;
    }
}

/* Calculate slider (thumb) layout info. */
static void
calc_layout_slider (GtkScrollbar *scrollbar,
                    gdouble       adjustment_value)
{
  OsScrollbarPrivate *priv;

  priv = get_private (GTK_WIDGET (scrollbar));

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      gint y, trough_length, height;

      y = 0;
      trough_length = priv->trough.height;
      height = priv->slider.height;

      if (gtk_adjustment_get_upper (priv->adjustment) -
          gtk_adjustment_get_lower (priv->adjustment) -
          gtk_adjustment_get_page_size (priv->adjustment) != 0)
        y = (trough_length - height) * ((adjustment_value - gtk_adjustment_get_lower (priv->adjustment)) /
                                        (gtk_adjustment_get_upper (priv->adjustment) -
                                         gtk_adjustment_get_lower (priv->adjustment) -
                                         gtk_adjustment_get_page_size (priv->adjustment)));

      y = CLAMP (y, 0, trough_length);

      priv->slider.y = y;
    }
  else
    {
      gint x, trough_length, width;

      x = 0;
      trough_length = priv->trough.width;
      width = priv->slider.width;

      if (gtk_adjustment_get_upper (priv->adjustment) -
          gtk_adjustment_get_lower (priv->adjustment) -
          gtk_adjustment_get_page_size (priv->adjustment) != 0)
        x = (trough_length - width) * ((adjustment_value - gtk_adjustment_get_lower (priv->adjustment)) /
                                       (gtk_adjustment_get_upper (priv->adjustment) -
                                        gtk_adjustment_get_lower (priv->adjustment) -
                                        gtk_adjustment_get_page_size (priv->adjustment)));

      x = CLAMP (x, 0, trough_length);

      priv->slider.x = x;
    }
}

/* Calculate slide_initial_slider_position with more precision. */
static void
calc_precise_slide_values (GtkScrollbar *scrollbar,
                           gfloat        x_coordinate,
                           gfloat        y_coordinate)
{
  OsScrollbarPrivate *priv;
  gdouble adjustment_value;

  priv = get_private (GTK_WIDGET (scrollbar));

  adjustment_value = gtk_adjustment_get_value (priv->adjustment);

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      gdouble y1, y2, trough_length, height;

      y1 = 0;
      trough_length = priv->trough.height;

      if (gtk_adjustment_get_upper (priv->adjustment) - gtk_adjustment_get_lower (priv->adjustment) != 0)
        height = (trough_length * (gtk_adjustment_get_page_size (priv->adjustment) /
                                   (gtk_adjustment_get_upper (priv->adjustment) - 
                                    gtk_adjustment_get_lower (priv->adjustment))));
      else
        height = gtk_range_get_min_slider_size (GTK_RANGE (scrollbar));

      height = MAX (height, gtk_range_get_min_slider_size (GTK_RANGE (scrollbar)));

      if (gtk_adjustment_get_upper (priv->adjustment) -
          gtk_adjustment_get_lower (priv->adjustment) -
          gtk_adjustment_get_page_size (priv->adjustment) != 0)
        y1 = (trough_length - height) * ((adjustment_value - gtk_adjustment_get_lower (priv->adjustment)) /
                                         (gtk_adjustment_get_upper (priv->adjustment) -
                                          gtk_adjustment_get_lower (priv->adjustment) -
                                          gtk_adjustment_get_page_size (priv->adjustment)));

      y2 = 0;
      height = priv->slider.height;

      if (gtk_adjustment_get_upper (priv->adjustment) -
          gtk_adjustment_get_lower (priv->adjustment) -
          gtk_adjustment_get_page_size (priv->adjustment) != 0)
        y2 = (trough_length - height) * ((adjustment_value - gtk_adjustment_get_lower (priv->adjustment)) /
                                         (gtk_adjustment_get_upper (priv->adjustment) -
                                          gtk_adjustment_get_lower (priv->adjustment) -
                                          gtk_adjustment_get_page_size (priv->adjustment)));

      priv->slide_initial_slider_position = CLAMP (MIN (y1, y2), 0, trough_length);
      priv->slide_initial_coordinate = y_coordinate;
    }
  else
    {
      gdouble x1, x2, trough_length, width;

      x1 = 0;
      trough_length = priv->trough.width;

      if (gtk_adjustment_get_upper (priv->adjustment) - gtk_adjustment_get_lower (priv->adjustment) != 0)
        width = (trough_length * (gtk_adjustment_get_page_size (priv->adjustment) /
                                  (gtk_adjustment_get_upper (priv->adjustment) -
                                   gtk_adjustment_get_lower (priv->adjustment))));
      else
        width =  gtk_range_get_min_slider_size (GTK_RANGE (scrollbar));

      width = MAX (width, gtk_range_get_min_slider_size (GTK_RANGE (scrollbar)));

      if (gtk_adjustment_get_upper (priv->adjustment) -
          gtk_adjustment_get_lower (priv->adjustment) -
          gtk_adjustment_get_page_size (priv->adjustment) != 0)
        x1 = (trough_length - width) * ((adjustment_value - gtk_adjustment_get_lower (priv->adjustment)) /
                                        (gtk_adjustment_get_upper (priv->adjustment) -
                                         gtk_adjustment_get_lower (priv->adjustment) -
                                         gtk_adjustment_get_page_size (priv->adjustment)));

      x2 = 0;
      width = priv->slider.width;

      if (gtk_adjustment_get_upper (priv->adjustment) -
          gtk_adjustment_get_lower (priv->adjustment) -
          gtk_adjustment_get_page_size (priv->adjustment) != 0)
        x2 = (trough_length - width) * ((adjustment_value - gtk_adjustment_get_lower (priv->adjustment)) /
                                        (gtk_adjustment_get_upper (priv->adjustment) -
                                         gtk_adjustment_get_lower (priv->adjustment) -
                                         gtk_adjustment_get_page_size (priv->adjustment)));

      priv->slide_initial_slider_position = CLAMP (MIN (x1, x2), 0, trough_length);
      priv->slide_initial_coordinate = x_coordinate;
    }
}

/* Calculate the workarea using _UNITY_NET_WORKAREA_REGION. */
static void
calc_workarea (Display *display,
               Window   root)
{
  Atom type;
  gint result, fmt;
  gulong nitems, nleft;
  guchar *property_data;
  gulong *long_data;

  result = XGetWindowProperty (display, root,
                               unity_net_workarea_region_atom,
                               0L, 4096L, FALSE, XA_CARDINAL,
                               &type, &fmt, &nitems, &nleft, &property_data);

  /* Clear the os_workarea region,
   * before the union with the new rectangles.
   * Maybe it'd be better to place this call
   * inside the if statement below. */
  cairo_region_subtract (os_workarea, os_workarea);

  if (result == Success && property_data)
    {
      long_data = (gulong*) property_data;

      if (fmt == 32 && type == XA_CARDINAL && nitems % 4 == 0)
        {
          guint count;
          guint i;

          count = nitems / 4;

          for (i = 0; i < count; i++)
            {
              cairo_rectangle_int_t rect;

              rect.x = long_data[i * 4 + 0];
              rect.y = long_data[i * 4 + 1];
              rect.width = long_data[i * 4 + 2];
              rect.height = long_data[i * 4 + 3];

              cairo_region_union_rectangle (os_workarea, &rect);
            }
        }
    }
}

/* Check whether the thumb movement can be considered connected or not. */
static void
check_connection (GtkScrollbar *scrollbar)
{
  OsScrollbarPrivate *priv;
  gint x_pos, y_pos;

  priv = get_private (GTK_WIDGET (scrollbar));

  /* This seems to be required to get proper values. */
  calc_layout_bar (scrollbar, gtk_adjustment_get_value (priv->adjustment));
  calc_layout_slider (scrollbar, gtk_adjustment_get_value (priv->adjustment));

  gdk_window_get_origin (gtk_widget_get_window (priv->thumb), &x_pos, &y_pos);

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      if (priv->overlay.height > priv->slider.height)
        {
          if (y_pos >= priv->thumb_win.y + priv->overlay.y &&
              y_pos + priv->slider.height <= priv->thumb_win.y + priv->overlay.y + priv->overlay.height)
            priv->state |= OS_STATE_CONNECTED;
          else
            priv->state &= ~(OS_STATE_CONNECTED);
        }
      else
        {
          if (y_pos == priv->thumb_win.y + priv->slider.y)
            priv->state |= OS_STATE_CONNECTED;
          else
            priv->state &= ~(OS_STATE_CONNECTED);
        }
    }
  else
    {
      if (priv->overlay.width > priv->slider.width)
        {
          if (x_pos >= priv->thumb_win.x + priv->overlay.x &&
              x_pos + priv->slider.width <= priv->thumb_win.x + priv->overlay.x + priv->overlay.width)
            priv->state |= OS_STATE_CONNECTED;
          else
            priv->state &= ~(OS_STATE_CONNECTED);
        }
      else
        {
          if (x_pos == priv->thumb_win.x + priv->slider.x)
            priv->state |= OS_STATE_CONNECTED;
          else
            priv->state &= ~(OS_STATE_CONNECTED);
        }
    }
}

/* Convert coordinates into GtkRange values. */
static inline gdouble
coord_to_value (GtkScrollbar *scrollbar,
                gfloat        coord)
{
  OsScrollbarPrivate *priv;
  gdouble frac;
  gdouble value;
  gint    trough_length;
  gint    slider_length;

  priv = get_private (GTK_WIDGET (scrollbar));

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      trough_length = priv->trough.height;
      slider_length = MAX (priv->slider.height, priv->overlay.height);
    }
  else
    {
      trough_length = priv->trough.width;
      slider_length = MAX (priv->slider.width, priv->overlay.width);
    }

  if (trough_length == slider_length)
    frac = 1.0;
  else
    frac = (MAX (0, coord) / (gdouble) (trough_length - slider_length));

  value = gtk_adjustment_get_lower (priv->adjustment) + frac * (gtk_adjustment_get_upper (priv->adjustment) -
                                                                gtk_adjustment_get_lower (priv->adjustment) -
                                                                gtk_adjustment_get_page_size (priv->adjustment));

  value = CLAMP (value,
                 gtk_adjustment_get_lower (priv->adjustment),
                 gtk_adjustment_get_upper (priv->adjustment) - gtk_adjustment_get_page_size (priv->adjustment));

  return value;
}

#ifdef USE_GTK3
/* Deactivate the bar if it's the case. */
static void
deactivate_bar (GtkScrollbar *scrollbar)
{
  OsScrollbarPrivate *priv;

  priv = get_private (GTK_WIDGET (scrollbar));

  if (priv->bar != NULL && priv->deactivable_bar)
    os_bar_set_active (priv->bar, FALSE, TRUE);
}

/* Timeout before deactivating the bar. */
static gboolean
deactivate_bar_cb (gpointer user_data)
{
  GtkScrollbar *scrollbar;
  OsScrollbarPrivate *priv;

  scrollbar = GTK_SCROLLBAR (user_data);
  priv = get_private (GTK_WIDGET (scrollbar));

  OS_DCHECK (!priv->active_window);

  deactivate_bar (scrollbar);
  priv->source_deactivate_bar_id = 0;

  return FALSE;
}
#endif

/* destroy the private struct */
static void
destroy_private (gpointer priv)
{
  g_slice_free (OsScrollbarPrivate, priv);
}

/* Get the private struct. If there isn't one, return NULL */
static OsScrollbarPrivate*
lookup_private (GtkWidget *widget)
{
  return g_object_get_qdata (G_OBJECT (widget), os_quark_qdata);
}

/* Get the private struct. If there isn't one, create it */
static OsScrollbarPrivate*
get_private (GtkWidget *widget)
{
  OsScrollbarPrivate *priv;

  /* Fetch the private qdata struct. */
  priv = lookup_private (widget);

  if (!priv)
    {
      /* The widget doesn't have an associated qdata,
       * initialize and store it. */
      OsScrollbarPrivate *qdata;

      /* Describe the widget for theming. */
      gtk_widget_set_name (widget, "OsScrollbar");

      if (os_root_list == NULL)
        {
          GdkScreen *screen;
          GdkWindow *root;

          /* Create the static linked list prepending the object. */
          os_root_list = g_slist_prepend (os_root_list, widget);

          if (os_workarea == NULL)
            os_workarea = cairo_region_create ();

          /* Apply the root_filter_func. */
          screen = gtk_widget_get_screen (widget);
          root = gdk_screen_get_root_window (screen);
          gdk_window_set_events (root, gdk_window_get_events (root) |
                                       GDK_PROPERTY_CHANGE_MASK);
          gdk_window_add_filter (root, root_filter_func, NULL);
        }
      else
        {
          /* Prepend the object to the static linked list. */
          os_root_list = g_slist_prepend (os_root_list, widget);
        }

      /* Initialize memory. */
      qdata = g_slice_new0 (OsScrollbarPrivate);

      /* Initialize struct variables. */
      qdata->side = OS_SIDE_RIGHT;
#ifdef USE_GTK3
      qdata->deactivable_bar = TRUE;
#endif
      qdata->hidable_thumb = TRUE;
      qdata->fine_scroll_multiplier = 1.0;
      qdata->bar = os_bar_new ();
      qdata->window_group = gtk_window_group_new ();
      qdata->animation = os_animation_new (RATE_ANIMATION, MAX_DURATION_SCROLLING,
                                           scrolling_cb, scrolling_end_cb, widget);

      /* Store qdata. */
      g_object_set_qdata_full (G_OBJECT (widget), os_quark_qdata, qdata, destroy_private);
      priv = qdata;

      /* Create adjustment and thumb. */
      if (gtk_range_get_adjustment (GTK_RANGE (widget)))
        swap_adjustment (GTK_SCROLLBAR (widget), gtk_range_get_adjustment (GTK_RANGE (widget)));
      priv->orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (widget));
      swap_thumb (GTK_SCROLLBAR (widget), os_thumb_new (priv->orientation));

      priv->resizing_paned = FALSE;

      /* Connect to adjustment and orientation signals. */
      g_signal_connect (G_OBJECT (widget), "notify::adjustment",
                        G_CALLBACK (notify_adjustment_cb), NULL);
      g_signal_connect (G_OBJECT (widget), "notify::orientation",
                        G_CALLBACK (notify_orientation_cb), NULL);
    }

  return priv;
}

/* Hide the thumb if it's the case. */
static void
hide_thumb (GtkScrollbar *scrollbar)
{
  OsScrollbarPrivate *priv;

  priv = get_private (GTK_WIDGET (scrollbar));

  if (priv->hidable_thumb)
    gtk_widget_hide (priv->thumb);
}

/* Timeout before hiding the thumb. */
static gboolean
hide_thumb_cb (gpointer user_data)
{
  GtkScrollbar *scrollbar;
  OsScrollbarPrivate *priv;

  scrollbar = GTK_SCROLLBAR (user_data);
  priv = get_private (GTK_WIDGET (scrollbar));

  hide_thumb (scrollbar);
  priv->source_hide_thumb_id = 0;

  return FALSE;
}

/* Return TRUE if the widget is insensitive. */
static gboolean
is_insensitive (GtkScrollbar *scrollbar)
{
#ifdef USE_GTK3
  return (gtk_widget_get_state_flags (GTK_WIDGET (scrollbar)) & GTK_STATE_FLAG_INSENSITIVE) != 0;
#else
  return gtk_widget_get_state (GTK_WIDGET (scrollbar)) == GTK_STATE_INSENSITIVE;
#endif
}

/* Move the bar. */
static void
move_bar (GtkScrollbar *scrollbar)
{
  GdkRectangle mask;
  OsScrollbarPrivate *priv;

  priv = get_private (GTK_WIDGET (scrollbar));

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      mask.x = 0;
      mask.y = priv->overlay.y;
      mask.width = priv->bar_all.width;
      mask.height = priv->overlay.height;
    }
  else
    {
      mask.x = priv->overlay.x;
      mask.y = 0;
      mask.width = priv->overlay.width;
      mask.height = priv->bar_all.height;
    }

  os_bar_move_resize (priv->bar, mask);
}

/* Callback called by the scrolling animation. */
static void
scrolling_cb (gfloat   weight,
              gpointer user_data)
{
  GtkScrollbar *scrollbar;
  OsScrollbarPrivate *priv;
  gdouble new_value;

  scrollbar = GTK_SCROLLBAR (user_data);
  priv = get_private (GTK_WIDGET (scrollbar));

  if (weight < 1.0f)
    {
      gdouble current_value;
      gdouble diff;

      current_value = gtk_adjustment_get_value (priv->adjustment);
      diff = priv->value - current_value;
      new_value = current_value + diff * weight;
    }
  else
    new_value = priv->value;

  gtk_adjustment_set_value (priv->adjustment, new_value);
}

/* End function called by the scrolling animation. */
static void
scrolling_end_cb (gpointer user_data)
{
  GtkScrollbar *scrollbar;
  OsScrollbarPrivate *priv;

  scrollbar = GTK_SCROLLBAR (user_data);
  priv = get_private (GTK_WIDGET (scrollbar));

  /* Only update the slide values at the end of a reconnection,
   * with the button pressed, otherwise it's not needed. */
  if ((priv->state & OS_STATE_RECONNECTING) &&
      (priv->event & OS_EVENT_BUTTON_PRESS))
    {
      gint x_pos, y_pos;

      gdk_window_get_origin (gtk_widget_get_window (priv->thumb), &x_pos, &y_pos);

      calc_precise_slide_values (scrollbar, x_pos + priv->pointer.x, y_pos + priv->pointer.y);
    }

  /* Check if the thumb can be considered connected after the animation. */
  check_connection (scrollbar);

  /* Unset OS_STATE_RECONNECTING since the animation ended. */
  priv->state &= ~(OS_STATE_RECONNECTING);
}

/* Sanitize x coordinate of thumb window. */
static gint
sanitize_x (GtkScrollbar *scrollbar,
            gint          x,
            gint          y)
{
#ifndef USE_GTK3
  GdkRectangle gdk_rect;
#endif
  GdkScreen *screen;
  OsScrollbarPrivate *priv;
  cairo_rectangle_int_t rect;
  gint screen_x, screen_width, n_monitor, monitor_x;

  priv = get_private (GTK_WIDGET (scrollbar));

  /* The x - 1 coordinate shift is done
   * to calculate monitor boundaries. */
  monitor_x = priv->side == OS_SIDE_LEFT ? x : x - 1;

  screen = gtk_widget_get_screen (GTK_WIDGET (scrollbar));
  n_monitor = gdk_screen_get_monitor_at_point (screen, monitor_x, y);
#ifdef USE_GTK3
  gdk_screen_get_monitor_geometry (screen, n_monitor, &rect);
#else
  gdk_screen_get_monitor_geometry (screen, n_monitor, &gdk_rect);

  rect.x = gdk_rect.x;
  rect.y = gdk_rect.y;
  rect.width = gdk_rect.width;
  rect.height = gdk_rect.height;
#endif

  screen_x = rect.x;
  screen_width = rect.x + rect.width;

  if (!cairo_region_is_empty (os_workarea))
    {
      cairo_region_t *monitor_workarea;
      cairo_region_t *struts_region;
      cairo_rectangle_int_t tmp_rect;
      gint i, reg_x, reg_width;

      reg_x = rect.x;
      reg_width = rect.x + rect.width;

      /* Full monitor region. */
      monitor_workarea = cairo_region_create_rectangle (&rect);
      struts_region = cairo_region_copy (monitor_workarea);

      /* Workarea region for current monitor. */
      cairo_region_intersect (monitor_workarea, os_workarea);

      /* Struts region for current monitor. */
      cairo_region_subtract (struts_region, monitor_workarea);

      for (i = 0; i < cairo_region_num_rectangles (struts_region); i++)
        {
          OsStrutSideFlags strut_side;
          gint count;

          cairo_region_get_rectangle (struts_region, i, &tmp_rect);

          strut_side = OS_STRUT_SIDE_NONE;
          count = 0;

          /* Determine which side the strut is on. */
          if (tmp_rect.y == rect.y)
            {
              strut_side |= OS_STRUT_SIDE_TOP;
              count++;
            }

          if (tmp_rect.x == rect.x)
            {
              strut_side |= OS_STRUT_SIDE_LEFT;
              count++;
            }

          if (tmp_rect.x + tmp_rect.width == rect.x + rect.width)
            {
              strut_side |= OS_STRUT_SIDE_RIGHT;
              count++;
            }

          if (tmp_rect.y + tmp_rect.height == rect.y + rect.height)
            {
              strut_side |= OS_STRUT_SIDE_BOTTOM;
              count++;
            }

          /* Handle multiple sides. */
          if (count >= 2)
            {
              if (tmp_rect.width > tmp_rect.height)
                strut_side &= ~(OS_STRUT_SIDE_LEFT | OS_STRUT_SIDE_RIGHT);
              else if (tmp_rect.width < tmp_rect.height)
                strut_side &= ~(OS_STRUT_SIDE_TOP | OS_STRUT_SIDE_BOTTOM);
            }

          /* Get the monitor boundaries using the strut. */
          if (strut_side & OS_STRUT_SIDE_LEFT)
            {
              if (tmp_rect.x + tmp_rect.width > reg_x)
                reg_x = tmp_rect.x + tmp_rect.width;
            }

          if (strut_side & OS_STRUT_SIDE_RIGHT)
            {
              if (tmp_rect.x < reg_width)
                reg_width = tmp_rect.x;
            }
        }

      screen_x = reg_x;
      screen_width = reg_width;

      cairo_region_destroy (monitor_workarea);
      cairo_region_destroy (struts_region);
    }

  if (priv->side == OS_SIDE_RIGHT &&
      (n_monitor != gdk_screen_get_monitor_at_point (screen, monitor_x + priv->thumb_all.width, y) ||
       monitor_x + priv->thumb_all.width >= screen_width))
    {
      priv->state |= OS_STATE_INTERNAL;
      return MAX (x - priv->thumb_all.width, screen_width - priv->thumb_all.width);
    }

  if (priv->side == OS_SIDE_LEFT &&
      (n_monitor != gdk_screen_get_monitor_at_point (screen, monitor_x - priv->thumb_all.width, y) ||
       monitor_x - priv->thumb_all.width <= screen_x))
    {
      priv->state |= OS_STATE_INTERNAL;
      return MAX (x, screen_x);
    }

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    priv->state &= ~(OS_STATE_INTERNAL);

  return x;
}

/* Sanitize y coordinate of thumb window. */
static gint
sanitize_y (GtkScrollbar *scrollbar,
            gint          x,
            gint          y)
{
#ifndef USE_GTK3
  GdkRectangle gdk_rect;
#endif
  GdkScreen *screen;
  OsScrollbarPrivate *priv;
  cairo_rectangle_int_t rect;
  gint screen_y, screen_height, n_monitor, monitor_y;

  priv = get_private (GTK_WIDGET (scrollbar));

  /* The y - 1 coordinate shift is done
   * to calculate monitor boundaries. */
  monitor_y = priv->side == OS_SIDE_TOP ? y : y - 1;

  screen = gtk_widget_get_screen (GTK_WIDGET (scrollbar));
  n_monitor = gdk_screen_get_monitor_at_point (screen, x, monitor_y);
#ifdef USE_GTK3
  gdk_screen_get_monitor_geometry (screen, n_monitor, &rect);
#else
  gdk_screen_get_monitor_geometry (screen, n_monitor, &gdk_rect);

  rect.x = gdk_rect.x;
  rect.y = gdk_rect.y;
  rect.width = gdk_rect.width;
  rect.height = gdk_rect.height;
#endif

  screen_y = rect.y;
  screen_height = rect.y + rect.height;

  if (!cairo_region_is_empty (os_workarea))
    {
      cairo_region_t *monitor_workarea;
      cairo_region_t *struts_region;
      cairo_rectangle_int_t tmp_rect;
      gint i, reg_y, reg_height;

      reg_y = rect.y;
      reg_height = rect.y + rect.height;

      /* Full monitor region. */
      monitor_workarea = cairo_region_create_rectangle (&rect);
      struts_region = cairo_region_copy (monitor_workarea);

      /* Workarea region for current monitor. */
      cairo_region_intersect (monitor_workarea, os_workarea);

      /* Struts region for current monitor. */
      cairo_region_subtract (struts_region, monitor_workarea);

      for (i = 0; i < cairo_region_num_rectangles (struts_region); i++)
        {
          OsStrutSideFlags strut_side;
          gint count;

          cairo_region_get_rectangle (struts_region, i, &tmp_rect);

          strut_side = OS_STRUT_SIDE_NONE;
          count = 0;

          /* Determine which side the strut is on. */
          if (tmp_rect.y == rect.y)
            {
              strut_side |= OS_STRUT_SIDE_TOP;
              count++;
            }

          if (tmp_rect.x == rect.x)
            {
              strut_side |= OS_STRUT_SIDE_LEFT;
              count++;
            }

          if (tmp_rect.x + tmp_rect.width == rect.x + rect.width)
            {
              strut_side |= OS_STRUT_SIDE_RIGHT;
              count++;
            }

          if (tmp_rect.y + tmp_rect.height == rect.y + rect.height)
            {
              strut_side |= OS_STRUT_SIDE_BOTTOM;
              count++;
            }

          /* Handle multiple sides. */
          if (count >= 2)
            {
              if (tmp_rect.width > tmp_rect.height)
                strut_side &= ~(OS_STRUT_SIDE_LEFT | OS_STRUT_SIDE_RIGHT);
              else if (tmp_rect.width < tmp_rect.height)
                strut_side &= ~(OS_STRUT_SIDE_TOP | OS_STRUT_SIDE_BOTTOM);
            }

          /* Get the monitor boundaries using the strut. */
          if (strut_side & OS_STRUT_SIDE_TOP)
            {
              if (tmp_rect.y + tmp_rect.height > reg_y)
                reg_y = tmp_rect.y + tmp_rect.height;
            }

          if (strut_side & OS_STRUT_SIDE_BOTTOM)
            {
              if (tmp_rect.y < reg_height)
                reg_height = tmp_rect.y;
            }
        }

      screen_y = reg_y;
      screen_height = reg_height;

      cairo_region_destroy (monitor_workarea);
      cairo_region_destroy (struts_region);
    }

  if (priv->side == OS_SIDE_BOTTOM &&
      (n_monitor != gdk_screen_get_monitor_at_point (screen, x, monitor_y + priv->thumb_all.height) ||
       monitor_y + priv->thumb_all.height >= screen_height))
    {
      priv->state |= OS_STATE_INTERNAL;
      return MAX (y - priv->thumb_all.height, screen_height - priv->thumb_all.height);
    }

  if (priv->side == OS_SIDE_TOP &&
      (n_monitor != gdk_screen_get_monitor_at_point (screen, x, monitor_y - priv->thumb_all.height) ||
       monitor_y - priv->thumb_all.height <= screen_y))
    {
      priv->state |= OS_STATE_INTERNAL;
      return MAX (y, screen_y);
    }

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    priv->state &= ~(OS_STATE_INTERNAL);

  return y;
}

/* Move the thumb window. */
static void
move_thumb (GtkScrollbar *scrollbar,
            gint          x,
            gint          y)
{
  OsScrollbarPrivate *priv;

  priv = get_private (GTK_WIDGET (scrollbar));

  gtk_window_move (GTK_WINDOW (priv->thumb),
                   sanitize_x (scrollbar, x, y),
                   sanitize_y (scrollbar, x, y));
}

/* Callback called when the adjustment changes. */
static void
notify_adjustment_cb (GObject *object,
                      gpointer user_data)
{
  GtkScrollbar *scrollbar;

  scrollbar = GTK_SCROLLBAR (object);

  swap_adjustment (scrollbar, gtk_range_get_adjustment (GTK_RANGE (object)));
}

/* Callback called when the orientation changes. */
static void
notify_orientation_cb (GObject *object,
                       gpointer user_data)
{
  GtkScrollbar *scrollbar;
  OsScrollbarPrivate *priv;

  scrollbar = GTK_SCROLLBAR (object);
  priv = get_private (GTK_WIDGET (scrollbar));

  priv->orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (object));

  swap_thumb (scrollbar, os_thumb_new (priv->orientation));
}

/* Stop function called by the scrolling animation. */
static void
scrolling_stop_cb (gpointer user_data)
{
  GtkScrollbar *scrollbar;
  OsScrollbarPrivate *priv;

  scrollbar = GTK_SCROLLBAR (user_data);
  priv = get_private (GTK_WIDGET (scrollbar));

  /* No slide values update here,
   * handle them separately! */

  /* Check if the thumb can be considered connected after the animation. */
  check_connection (scrollbar);

  /* Unset OS_STATE_RECONNECTING since the animation ended. */
  priv->state &= ~(OS_STATE_RECONNECTING);
}

/* Swap adjustment pointer. */
static void
swap_adjustment (GtkScrollbar  *scrollbar,
                 GtkAdjustment *adjustment)
{
  OsScrollbarPrivate *priv;

  priv = get_private (GTK_WIDGET (scrollbar));

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

/* Swap thumb pointer. */
static void
swap_thumb (GtkScrollbar *scrollbar,
            GtkWidget    *thumb)
{
  OsScrollbarPrivate *priv;

  priv = get_private (GTK_WIDGET (scrollbar));

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

/* Timeout before unlocking the thumb. */
static gboolean
unlock_thumb_cb (gpointer user_data)
{
  GtkScrollbar *scrollbar;
  OsScrollbarPrivate *priv;

  scrollbar = GTK_SCROLLBAR (user_data);
  priv = get_private (GTK_WIDGET (scrollbar));

  if (priv->hidable_thumb)
    priv->state &= ~(OS_STATE_LOCKED);

  priv->source_unlock_thumb_id = 0;

  return FALSE;
}

/* Get the window at pointer. */
static GdkWindow*
window_at_pointer (GdkWindow *window,
                   gint      *x,
                   gint      *y)
{
#ifdef USE_GTK3
  GdkDeviceManager *device_manager;
  GdkDevice *device;

  device_manager = gdk_display_get_device_manager (gdk_window_get_display (window));
  device = gdk_device_manager_get_client_pointer (device_manager);

  return gdk_device_get_window_at_position (device, x, y);
#else
  return gdk_window_at_pointer (x, y);
#endif
}

/* Get the position of the pointer. */
static GdkWindow*
window_get_pointer (GdkWindow       *window,
                    gint            *x,
                    gint            *y,
                    GdkModifierType *mask)
{
#ifdef USE_GTK3
  GdkDeviceManager *device_manager;
  GdkDevice *device;

  device_manager = gdk_display_get_device_manager (gdk_window_get_display (window));
  device = gdk_device_manager_get_client_pointer (device_manager);

  return gdk_window_get_device_position (window, device, x, y, mask);
#else
  return gdk_window_get_pointer (window, x, y, mask);
#endif
}

/* Adjustment functions. */

/* Calculate fine_scroll_multiplier. */
static void
calc_fine_scroll_multiplier (GtkScrollbar *scrollbar)
{
  OsScrollbarPrivate *priv;

  priv = get_private (GTK_WIDGET (scrollbar));

  /* FIXME(Cimi) Not sure about this calculation...
   * However seems to work "enough" well. */
  priv->fine_scroll_multiplier = MIN ((priv->orientation == GTK_ORIENTATION_VERTICAL ?
                                       priv->trough.height : priv->trough.width) /
                                      (gtk_adjustment_get_upper (priv->adjustment) -
                                       gtk_adjustment_get_lower (priv->adjustment) -
                                       gtk_adjustment_get_page_size (priv->adjustment)),
                                      1);
}

static void
adjustment_changed_cb (GtkAdjustment *adjustment,
                       gpointer       user_data)
{
  GtkScrollbar *scrollbar;
  OsScrollbarPrivate *priv;

  scrollbar = GTK_SCROLLBAR (user_data);
  priv = get_private (GTK_WIDGET (scrollbar));

  /* FIXME(Cimi) we should control each time os_bar_show ()/hide ()
   * is called here and in map ()/unmap ().
   * We are arbitrary calling that and I'm frightened we should show or keep
   * hidden a bar that is meant to be hidden/shown.
   * I don't want to see bars reappearing because
   * of a change in the adjustment of an invisible bar or viceversa. */
  if (gtk_adjustment_get_upper (adjustment) - gtk_adjustment_get_lower (adjustment) >
      gtk_adjustment_get_page_size (adjustment))
    {
      priv->state &= ~(OS_STATE_FULLSIZE);
      if (priv->filter.proximity)
        os_bar_show (priv->bar);
    }
  else
    {
      priv->state |= OS_STATE_FULLSIZE;
      if (priv->filter.proximity)
        {
          os_bar_hide (priv->bar);

          gtk_widget_hide (priv->thumb);
        }
    }

  calc_layout_bar (scrollbar, gtk_adjustment_get_value (adjustment));
  calc_layout_slider (scrollbar, gtk_adjustment_get_value (adjustment));
  calc_fine_scroll_multiplier (scrollbar);

  if (!(priv->event & OS_EVENT_ENTER_NOTIFY) &&
      !(priv->event & OS_EVENT_MOTION_NOTIFY))
    gtk_widget_hide (priv->thumb);

  move_bar (scrollbar);
}

/* Update the tail (visual connection) between bar and thumb. */
static void
update_tail (GtkScrollbar *scrollbar)
{
  OsScrollbarPrivate *priv;
  gint x_pos, y_pos;

  priv = get_private (GTK_WIDGET (scrollbar));

  gdk_window_get_origin (gtk_widget_get_window (priv->thumb), &x_pos, &y_pos);

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      if (priv->thumb_win.y + priv->overlay.y >= y_pos + priv->slider.height)
        {
          GdkRectangle mask;

          mask.x = 0;
          mask.y = y_pos + priv->slider.height / 2 - priv->thumb_win.y;
          mask.width = priv->bar_all.width;
          mask.height = priv->overlay.y - mask.y;

          os_bar_connect (priv->bar, mask);

          priv->state |= OS_STATE_DETACHED;

          os_bar_set_detached (priv->bar, TRUE, FALSE);
          os_thumb_set_detached (OS_THUMB (priv->thumb), TRUE);
        }
      else if (priv->thumb_win.y + priv->overlay.y + priv->overlay.height <= y_pos)
        {
          GdkRectangle mask;

          mask.x = 0;
          mask.y = priv->overlay.y + priv->overlay.height;
          mask.width = priv->bar_all.width;
          mask.height = y_pos + priv->slider.height / 2 - priv->thumb_win.y - mask.y;

          os_bar_connect (priv->bar, mask);

          priv->state |= OS_STATE_DETACHED;

          os_bar_set_detached (priv->bar, TRUE, FALSE);
          os_thumb_set_detached (OS_THUMB (priv->thumb), TRUE);
        }
      else
        {
          priv->state &= ~(OS_STATE_DETACHED);

          os_bar_set_detached (priv->bar, FALSE, FALSE);
          os_thumb_set_detached (OS_THUMB (priv->thumb), FALSE);
        }
    }
  else
    {
      if (priv->thumb_win.x + priv->overlay.x >= x_pos + priv->slider.width)
        {
          GdkRectangle mask;

          mask.x = x_pos + priv->slider.width / 2 - priv->thumb_win.x;
          mask.y = 0;
          mask.width = priv->overlay.x - mask.x;
          mask.height = priv->bar_all.height;

          os_bar_connect (priv->bar, mask);

          priv->state |= OS_STATE_DETACHED;

          os_bar_set_detached (priv->bar, TRUE, FALSE);
          os_thumb_set_detached (OS_THUMB (priv->thumb), TRUE);
        }
      else if (priv->thumb_win.x + priv->overlay.x + priv->overlay.width <= x_pos)
        {
          GdkRectangle mask;

          mask.x = priv->overlay.x + priv->overlay.width;
          mask.y = 0;
          mask.width = x_pos + priv->slider.width / 2 - priv->thumb_win.x - mask.x;
          mask.height = priv->bar_all.height;

          os_bar_connect (priv->bar, mask);

          priv->state |= OS_STATE_DETACHED;

          os_bar_set_detached (priv->bar, TRUE, FALSE);
          os_thumb_set_detached (OS_THUMB (priv->thumb), TRUE);
        }
      else
        {
          priv->state &= ~(OS_STATE_DETACHED);

          os_bar_set_detached (priv->bar, FALSE, FALSE);
          os_thumb_set_detached (OS_THUMB (priv->thumb), FALSE);
        }
    }
}

static void
adjustment_value_changed_cb (GtkAdjustment *adjustment,
                             gpointer       user_data)
{
  GtkScrollbar *scrollbar;
  OsScrollbarPrivate *priv;

  scrollbar = GTK_SCROLLBAR (user_data);
  priv = get_private (GTK_WIDGET (scrollbar));

  calc_layout_bar (scrollbar, gtk_adjustment_get_value (adjustment));
  calc_layout_slider (scrollbar, gtk_adjustment_get_value (adjustment));

  if (!(priv->event & OS_EVENT_ENTER_NOTIFY) &&
      !(priv->event & OS_EVENT_MOTION_NOTIFY))
    gtk_widget_hide (priv->thumb);

  if (gtk_widget_get_mapped (priv->thumb) &&
      !((priv->event & OS_EVENT_MOTION_NOTIFY) &&
        (priv->state & OS_STATE_CONNECTED)))
    update_tail (scrollbar);

  move_bar (scrollbar);
}

#ifdef USE_GTK3
/* Bar functions. */

/* Set the state of the bar checking mouse position. */
static void
bar_set_state_from_pointer (GtkScrollbar *scrollbar,
                            gint          x,
                            gint          y)
{
  GtkAllocation allocation;
  OsScrollbarPrivate *priv;

  priv = get_private (GTK_WIDGET (scrollbar));

  OS_DCHECK (!priv->active_window);

  gtk_widget_get_allocation (gtk_widget_get_parent (GTK_WIDGET (scrollbar)), &allocation);

  if ((x > allocation.x && x < allocation.x + allocation.width) &&
      (y > allocation.y && y < allocation.y + allocation.height))
    {
      priv->deactivable_bar = FALSE;
      os_bar_set_active (priv->bar, TRUE, TRUE);
    }
  else
    {
      priv->deactivable_bar = TRUE;
      os_bar_set_active (priv->bar, FALSE, TRUE);
    }
}
#endif

/* Root window functions. */

/* Filter function applied to the root window. */
static GdkFilterReturn
root_filter_func (GdkXEvent *gdkxevent,
                  GdkEvent  *event,
                  gpointer   user_data)
{
  XEvent* xev;

  xev = gdkxevent;

  if (xev->type == PropertyNotify)
    {
      if (xev->xproperty.atom == unity_net_workarea_region_atom)
        {
          calc_workarea (xev->xany.display, xev->xany.window);
        }
    }

  return GDK_FILTER_CONTINUE;
}

/* Thumb functions. */

/* Present a X11 window. */
static void
present_window_with_timestamp (Screen  *screen,
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

#ifdef USE_GTK3
  gdk_error_trap_pop_ignored ();
#else
  gdk_error_trap_pop ();
#endif
}

/* Present a Gdk window. */
static void
present_gdk_window_with_timestamp (GtkWidget *widget,
                                   guint32    timestamp)
{
  /* Check if the widget is realized, to protect against:
   * https://bugs.launchpad.net/ayatana-scrollbar/+bug/846562 */
  if (gtk_widget_get_realized (widget))
    present_window_with_timestamp (GDK_SCREEN_XSCREEN (gtk_widget_get_screen (widget)),
                                   GDK_WINDOW_XID (gtk_widget_get_window (widget)),
                                   timestamp);
}

static gboolean
thumb_button_press_event_cb (GtkWidget      *widget,
                             GdkEventButton *event,
                             gpointer        user_data)
{
  if (event->type == GDK_BUTTON_PRESS)
    {
      if (event->button == 1 ||
          event->button == 2)
        {
          GtkScrollbar *scrollbar;
          OsScrollbarPrivate *priv;

          scrollbar = GTK_SCROLLBAR (user_data);
          priv = get_private (GTK_WIDGET (scrollbar));

          gtk_window_set_transient_for (GTK_WINDOW (widget),
                                        GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (scrollbar))));

          priv->present_time = g_get_monotonic_time ();
          present_gdk_window_with_timestamp (GTK_WIDGET (scrollbar), event->time);

          priv->event |= OS_EVENT_BUTTON_PRESS;
          priv->event &= ~(OS_EVENT_MOTION_NOTIFY);

          /* Middle-click or shift+left-click for "jump to" action. */
          if (event->button == 2 ||
              (event->button == 1 && (event->state & GDK_SHIFT_MASK)))
            {
              /* Reconnect the thumb with the bar. */
              gdouble new_value;
              gfloat c;
              gint32 duration;

              if (priv->orientation == GTK_ORIENTATION_VERTICAL)
                c = event->y_root - priv->thumb_win.y - event->y;
              else
                c = event->x_root - priv->thumb_win.x - event->x;

              /* If a scrolling animation is running,
               * stop it and add the new value. */
              os_animation_stop (priv->animation, scrolling_stop_cb);

              new_value = coord_to_value (scrollbar, c);

              /* Only start the animation if needed. */
              if (new_value != gtk_adjustment_get_value (priv->adjustment))
                {
                  priv->state |= OS_STATE_RECONNECTING;

                  priv->value = new_value;

                  /* Calculate and set the duration. */
                  if (priv->value > gtk_adjustment_get_value (priv->adjustment))
                    duration = MIN_DURATION_SCROLLING + ((priv->value - gtk_adjustment_get_value (priv->adjustment)) /
                                                         (gtk_adjustment_get_upper (priv->adjustment) -
                                                          gtk_adjustment_get_lower (priv->adjustment))) *
                                                        (MAX_DURATION_SCROLLING - MIN_DURATION_SCROLLING);
                  else
                    duration = MIN_DURATION_SCROLLING + ((gtk_adjustment_get_value (priv->adjustment) - priv->value) /
                                                         (gtk_adjustment_get_upper (priv->adjustment) -
                                                          gtk_adjustment_get_lower (priv->adjustment))) *
                                                        (MAX_DURATION_SCROLLING - MIN_DURATION_SCROLLING);
                  os_animation_set_duration (priv->animation, duration);

                  /* Start the scrolling animation. */
                  os_animation_start (priv->animation);
                }
            }

          calc_precise_slide_values (scrollbar, event->x_root, event->y_root);

          priv->pointer.x = event->x;
          priv->pointer.y = event->y;
        }
    }

  return FALSE;
}

/* Scroll down, with animation. */
static void
scroll_down (GtkScrollbar *scrollbar,
             OsScrollType  scroll_type)
{
  OsScrollbarPrivate *priv;
  gdouble new_value, increment;
  gint32 duration;

  priv = get_private (GTK_WIDGET (scrollbar));

  /* Either step down or page down. */
  if (scroll_type == OS_SCROLL_STEP)
    increment = gtk_adjustment_get_step_increment (priv->adjustment);
  else
    increment = gtk_adjustment_get_page_increment (priv->adjustment);

  /* If a scrolling animation is running,
   * stop it and add the new value. */
  if (os_animation_is_running (priv->animation))
    {
      os_animation_stop (priv->animation, scrolling_stop_cb);
      new_value = priv->value + increment;
    }
  else
      new_value = gtk_adjustment_get_value (priv->adjustment) + increment;

  priv->value = CLAMP (new_value,
                       gtk_adjustment_get_lower (priv->adjustment),
                       gtk_adjustment_get_upper (priv->adjustment) - gtk_adjustment_get_page_size (priv->adjustment));

  /* There's no need to do start a new animation. */
  if (priv->value == gtk_adjustment_get_value (priv->adjustment))
    return;

  /* Calculate and set the duration. */
  if (scroll_type == OS_SCROLL_STEP)
    duration = MIN_DURATION_SCROLLING;
  else
    duration = MIN_DURATION_SCROLLING + ((priv->value - gtk_adjustment_get_value (priv->adjustment)) / increment) *
                                      (MAX_DURATION_SCROLLING - MIN_DURATION_SCROLLING);
  os_animation_set_duration (priv->animation, duration);

  /* Start the scrolling animation. */
  os_animation_start (priv->animation);
}

/* Scroll up, with animation. */
static void
scroll_up (GtkScrollbar *scrollbar,
           OsScrollType  scroll_type)
{
  OsScrollbarPrivate *priv;
  gdouble new_value, increment;
  gint32 duration;

  priv = get_private (GTK_WIDGET (scrollbar));

  /* Either step up or page up. */
  if (scroll_type == OS_SCROLL_STEP)
    increment = gtk_adjustment_get_step_increment (priv->adjustment);
  else
    increment = gtk_adjustment_get_page_increment (priv->adjustment);

  /* If a scrolling animation is running,
   * stop it and subtract the new value. */
  if (os_animation_is_running (priv->animation))
    {
      os_animation_stop (priv->animation, scrolling_stop_cb);
      new_value = priv->value - increment;
    }
  else
      new_value = gtk_adjustment_get_value (priv->adjustment) - increment;

  priv->value = CLAMP (new_value,
                       gtk_adjustment_get_lower (priv->adjustment),
                       gtk_adjustment_get_upper (priv->adjustment) - gtk_adjustment_get_page_size (priv->adjustment));

  /* There's no need to do start a new animation. */
  if (priv->value == gtk_adjustment_get_value (priv->adjustment))
    return;

  /* Calculate and set the duration. */
  if (scroll_type == OS_SCROLL_STEP)
    duration = MIN_DURATION_SCROLLING;
  else
    duration = MIN_DURATION_SCROLLING + ((gtk_adjustment_get_value (priv->adjustment) - priv->value) / increment) *
                                        (MAX_DURATION_SCROLLING - MIN_DURATION_SCROLLING);
  os_animation_set_duration (priv->animation, duration);

  /* Start the scrolling animation. */
  os_animation_start (priv->animation);
}

static gboolean
thumb_button_release_event_cb (GtkWidget      *widget,
                               GdkEventButton *event,
                               gpointer        user_data)
{
  if (event->type == GDK_BUTTON_RELEASE)
    {
      if (event->button == 1 ||
          event->button == 2)
        {
          GtkScrollbar *scrollbar;
          OsScrollbarPrivate *priv;

          scrollbar = GTK_SCROLLBAR (user_data);
          priv = get_private (GTK_WIDGET (scrollbar));

          gtk_window_set_transient_for (GTK_WINDOW (widget), NULL);

          /* Don't trigger actions on thumb dragging or jump-to scrolling. */
          if (event->button == 1 &&
              !(event->state & GDK_SHIFT_MASK) &&
              !(priv->event & OS_EVENT_MOTION_NOTIFY))
            {
              OsScrollType scroll_type;

              /* Type of the scroll to perform. */
              if (event->state & MODIFIER_KEY)
                scroll_type = OS_SCROLL_STEP;
              else
                scroll_type = OS_SCROLL_PAGE;

              if (priv->orientation == GTK_ORIENTATION_VERTICAL)
                {
                  if (priv->pointer.y < priv->slider.height / 2)
                    scroll_up (scrollbar, scroll_type);
                  else
                    scroll_down (scrollbar, scroll_type);
                }
              else
                {
                  if (priv->pointer.x < priv->slider.width / 2)
                    scroll_up (scrollbar, scroll_type);
                  else
                    scroll_down (scrollbar, scroll_type);
                }
            }

          priv->event &= ~(OS_EVENT_BUTTON_PRESS | OS_EVENT_MOTION_NOTIFY);
        }
    }

  return FALSE;
}

static void
enter_event (GtkScrollbar *scrollbar)
{
  OsScrollbarPrivate *priv;

  priv = get_private (GTK_WIDGET (scrollbar));

  priv->event |= OS_EVENT_ENTER_NOTIFY;

#ifdef USE_GTK3
  priv->deactivable_bar = FALSE;
#endif
  priv->hidable_thumb = FALSE;

  if (priv->state & OS_STATE_INTERNAL)
    priv->state |= OS_STATE_LOCKED;
}

static gboolean
thumb_enter_notify_event_cb (GtkWidget        *widget,
                             GdkEventCrossing *event,
                             gpointer          user_data)
{
  GtkScrollbar *scrollbar;

  scrollbar = GTK_SCROLLBAR (user_data);

#ifdef USE_GTK3
  /* Gtk+ 3.3.18 emits more GdkEventCrossing
   * with touch devices, skip few events. */
  if (event->mode == GDK_CROSSING_TOUCH_BEGIN ||
      event->mode == GDK_CROSSING_TOUCH_END ||
      event->mode == GDK_CROSSING_DEVICE_SWITCH)
    return FALSE;
#endif

  enter_event (scrollbar);

  return FALSE;
}

static gboolean
thumb_leave_notify_event_cb (GtkWidget        *widget,
                             GdkEventCrossing *event,
                             gpointer          user_data)
{
  GtkScrollbar *scrollbar;
  OsScrollbarPrivate *priv;

  scrollbar = GTK_SCROLLBAR (user_data);
  priv = get_private (GTK_WIDGET (scrollbar));

#ifdef USE_GTK3
  /* Gtk+ 3.3.18 emits more GdkEventCrossing
   * with touch devices, skip few events.
   * Last line skips the event if the pointer is still in the window:
   * this happens with touch devices because Gtk+ emits 
   * GDK_CROSSING_UNGRAB to the touch device, thus calling leave-notify,
   * and we want to skip those events.
   * 
   * FIXME the logic of this event should be rewritten. */
  if (event->mode == GDK_CROSSING_TOUCH_BEGIN ||
      event->mode == GDK_CROSSING_TOUCH_END ||
      event->mode == GDK_CROSSING_DEVICE_SWITCH ||
      window_at_pointer (event->window, NULL, NULL) == event->window)
    return FALSE;
#endif

  /* When exiting the thumb horizontally (or vertically),
   * in LOCKED state, remove the lock. */
  if ((priv->state & OS_STATE_LOCKED) &&
      ((priv->side == OS_SIDE_RIGHT && event->x < 0) ||
       (priv->side == OS_SIDE_BOTTOM && event->y < 0) ||
       (priv->side == OS_SIDE_LEFT && event->x - priv->thumb_all.width >= 0) ||
       (priv->side == OS_SIDE_TOP && event->y - priv->thumb_all.height >= 0)))
    priv->state &= ~(OS_STATE_LOCKED);

  /* Add the timeouts only if you are
   * not interacting with the thumb. */
  if (!(priv->event & OS_EVENT_BUTTON_PRESS))
    {
#ifdef USE_GTK3
      /* Never deactivate the bar in an active window. */
      if (!priv->active_window)
        {
          priv->deactivable_bar = TRUE;

          if (priv->source_deactivate_bar_id != 0)
            g_source_remove (priv->source_deactivate_bar_id);

          priv->source_deactivate_bar_id = g_timeout_add (TIMEOUT_THUMB_HIDE,
                                                          deactivate_bar_cb,
                                                          scrollbar);
        }
#endif

      priv->event &= ~(OS_EVENT_ENTER_NOTIFY);

      priv->hidable_thumb = TRUE;

      if (priv->source_hide_thumb_id != 0)
        g_source_remove (priv->source_hide_thumb_id);

      priv->source_hide_thumb_id = g_timeout_add (TIMEOUT_THUMB_HIDE,
                                                  hide_thumb_cb,
                                                  scrollbar);
    }

  return FALSE;
}

static void
thumb_map_cb (GtkWidget *widget,
              gpointer   user_data)
{
  Display *display;
  GtkScrollbar *scrollbar;
  XWindowChanges changes;
  guint32 xid, xid_parent;
  unsigned int value_mask = CWSibling | CWStackMode;
  int res;

  scrollbar = GTK_SCROLLBAR (user_data);

#ifdef USE_GTK3
  OsScrollbarPrivate *priv;

  /* Immediately set the bar to be active. */
  priv = get_private (GTK_WIDGET (scrollbar));

  priv->deactivable_bar = FALSE;
  os_bar_set_active (priv->bar, TRUE, FALSE);
#endif

  xid = GDK_WINDOW_XID (gtk_widget_get_window (widget));
  xid_parent = GDK_WINDOW_XID (gtk_widget_get_window (gtk_widget_get_toplevel (GTK_WIDGET (scrollbar))));
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
       * Should work on window managers that supports this, like compiz.
       * Unfortunately, metacity doesn't yet, so we might decide to implement
       * this atom in metacity/mutter as well. 
       * We need to restack the window because the thumb window can be above
       * every window, noticeable when you make the thumb of an unfocused window
       * appear, and it could be above other windows (like the focused one).
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

#ifdef USE_GTK3
      gdk_error_trap_pop_ignored ();
#else
      gdk_error_trap_pop ();
#endif
    }
}

/* From pointer movement, set adjustment value. */
static void
capture_movement (GtkScrollbar *scrollbar,
                  gint          mouse_x,
                  gint          mouse_y)
{
  OsScrollbarPrivate *priv;
  gfloat c;
  gint delta;
  gdouble new_value;

  priv = get_private (GTK_WIDGET (scrollbar));

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    delta = mouse_y - priv->slide_initial_coordinate;
  else
    delta = mouse_x - priv->slide_initial_coordinate;

  /* With fine scroll, slow down the scroll. */
  if (priv->state & OS_STATE_FINE_SCROLL)
    c = priv->slide_initial_slider_position + delta * priv->fine_scroll_multiplier;
  else
    c = priv->slide_initial_slider_position + delta;

  new_value = coord_to_value (scrollbar, c);

  gtk_adjustment_set_value (priv->adjustment, new_value);
}

static gboolean
thumb_motion_notify_event_cb (GtkWidget      *widget,
                              GdkEventMotion *event,
                              gpointer        user_data)
{
  GtkScrollbar *scrollbar;
  OsScrollbarPrivate *priv;

  scrollbar = GTK_SCROLLBAR (user_data);
  priv = get_private (GTK_WIDGET (scrollbar));

#ifdef USE_GTK3
  /* On touch devices with XI2 and Gtk+ >= 3.3.18,
   * the event enter-notify is not emitted.
   * Deal with it in motion-notify. */
  
  /* Should be fixed with:
   * https://bugs.launchpad.net/ubuntu/+source/gtk+3.0/+bug/949414 */
  // if (!(priv->event & OS_EVENT_ENTER_NOTIFY))
  //   enter_event (scrollbar);
#endif

  if (priv->event & OS_EVENT_BUTTON_PRESS)
    {
      gint x, y;
      gint f_x, f_y;

      f_x = abs (priv->pointer.x - event->x);
      f_y = abs (priv->pointer.y - event->y);

      /* Use tolerance at the first calls to this motion notify event. */
      if (!(priv->event & OS_EVENT_MOTION_NOTIFY) &&
          f_x <= TOLERANCE_MOTION &&
          f_y <= TOLERANCE_MOTION)
        return FALSE;

      /* The scrollbar allows resize, and a scrolling is not started yet.
       * Decide if the user will start a window resize or a normal scroll. */
      if (priv->allow_resize &&
          !(priv->event & OS_EVENT_MOTION_NOTIFY))
        {
          /* Trying to draw the area of interest,
           * in the case of a vertical scrollbar.
           * Everything is reflected for horizontal scrollbars.
           *
           * +                           +
           *   +       SCROLLING       +
           *     +                   +
           *     + +               + +
           *  R  +   +  +  +  +  +   +  R
           *  E  +                   +  E
           *  S  +     no action     +  S
           *  I  +       taken       +  I
           *  Z  +                   +  Z
           *  E  +   +  +  +  +  +   +  E
           *     + +               + +
           *     +                   +
           *   +       SCROLLING       +
           * +                           +
           *
           * The diagonal lines are inclined differently, using 0.5 * f_y.
           **/
          if (((priv->side == OS_SIDE_RIGHT || priv->side == OS_SIDE_LEFT) && f_x > 0.5 * f_y) ||
              ((priv->side == OS_SIDE_BOTTOM || priv->side == OS_SIDE_TOP) && f_y > 0.5 * f_x))
            {
              /* We're either in the 'RESIZE' or in the 'no action taken' area. */
              if (((priv->side == OS_SIDE_RIGHT || priv->side == OS_SIDE_LEFT) && f_x > TOLERANCE_DRAG) ||
                  ((priv->side == OS_SIDE_BOTTOM || priv->side == OS_SIDE_TOP) && f_y > TOLERANCE_DRAG))
                {
                  /* We're in the 'RESIZE' area.
                   * Set the right resize type and hide the thumb. */
                  GdkWindowEdge window_edge;

                  switch (priv->side)
                  {
                    case OS_SIDE_RIGHT:
                      window_edge = GDK_WINDOW_EDGE_EAST;
                      break;
                    case OS_SIDE_BOTTOM:
                      window_edge = GDK_WINDOW_EDGE_SOUTH;
                      break;
                    case OS_SIDE_LEFT:
                      window_edge = GDK_WINDOW_EDGE_WEST;
                      break;
                    case OS_SIDE_TOP:
                      window_edge = GDK_WINDOW_EDGE_NORTH;
                      break;
                  }

                  gdk_window_begin_resize_drag (gtk_widget_get_window (gtk_widget_get_toplevel (GTK_WIDGET (scrollbar))),
                                                window_edge,
                                                1,
                                                event->x_root,
                                                event->y_root,
                                                event->time);
                  gtk_widget_hide (widget);
                }
              /* We're in the 'no action taken' area.
               * Skip this event. */
              if (!priv->allow_resize_paned)
                return FALSE;
            }

          /* We're in the 'SCROLLING' area.
           * Continue processing the event. */
        }
        
        if (priv->allow_resize_paned &&
            !(priv->event & OS_EVENT_MOTION_NOTIFY))
        {
          if (((priv->side == OS_SIDE_RIGHT || priv->side == OS_SIDE_LEFT) && f_x > 0.5 * f_y) ||
              ((priv->side == OS_SIDE_BOTTOM || priv->side == OS_SIDE_TOP) && f_y > 0.5 * f_x))
            {
              /* We're either in the 'RESIZE' or in the 'no action taken' area. */
              if (((priv->side == OS_SIDE_RIGHT || priv->side == OS_SIDE_LEFT) && f_x > TOLERANCE_DRAG) ||
                  ((priv->side == OS_SIDE_BOTTOM || priv->side == OS_SIDE_TOP) && f_y > TOLERANCE_DRAG))
                {
                  /* We're in the 'RESIZE' area. */
                  GtkPaned *paned = GTK_PANED (gtk_widget_get_ancestor (GTK_WIDGET (scrollbar), GTK_TYPE_PANED));
                 
                  if (!paned)
                    return FALSE;

                  GdkWindow *handle_window = gtk_paned_get_handle_window (paned);

                  GdkEventButton *fwd_event = (GdkEventButton*) gdk_event_new (GDK_BUTTON_PRESS);

                  fwd_event->window = handle_window;
                  fwd_event->time =  event->time;
                  fwd_event->x = 1;
                  fwd_event->y = 1;
                  fwd_event->state = event->state;
                  fwd_event->button = 1;
                  fwd_event->x_root = event->x_root;
                  fwd_event->y_root = event->y_root;
                  fwd_event->device = event->device;

                  gdk_event_put ((GdkEvent*) fwd_event);

                  priv->resizing_paned = TRUE;

                  gtk_widget_hide (widget);
                }

              /* We're in the 'no action taken' area.
               * Skip this event. */
              return FALSE;
            }

        }

      if (!(priv->event & OS_EVENT_MOTION_NOTIFY))
        {
          /* Check if we can consider the thumb movement connected with the overlay. */
          check_connection (scrollbar);

          /* It is a scrolling event, set the flag. */
          priv->event |= OS_EVENT_MOTION_NOTIFY;
        }

      /* Before stopping the animation,
       * check if it's reconnecting.
       * In this case we need to update the slide values
       * with the current position. */
      if (os_animation_is_running (priv->animation))
        {
          if (priv->state & OS_STATE_RECONNECTING)
            {
              /* It's a reconnecting animation. */
              calc_precise_slide_values (scrollbar, event->x_root, event->y_root);
            }
          else
            {
              /* Stop the paging animation now. */
              os_animation_stop (priv->animation, scrolling_stop_cb);
            }
        }

      /* Check for modifier keys. */
      if (event->state & MODIFIER_KEY)
        {
          /* You pressed the modifier key for the first time,
           * let's deal with it. */
          if (!(priv->state & OS_STATE_FINE_SCROLL))
            {
              calc_fine_scroll_multiplier (scrollbar);
              calc_precise_slide_values (scrollbar, event->x_root, event->y_root);

              priv->state |= OS_STATE_FINE_SCROLL;
            }

          priv->state &= ~(OS_STATE_CONNECTED);
        }
      else
        {
          /* You released the modifier key for the first time,
           * let's deal with it. */
          if (priv->state & OS_STATE_FINE_SCROLL)
            {
              /* Recalculate slider positions. */
              calc_precise_slide_values (scrollbar, event->x_root, event->y_root);

              priv->state &= ~(OS_STATE_FINE_SCROLL);
            }
        }

      /* Behave differently when the thumb is connected or not. */
      if (priv->state & OS_STATE_CONNECTED)
        {
          /* This is a connected scroll,
           * the thumb movement is kept in sync with the overlay. */

          if (priv->orientation == GTK_ORIENTATION_VERTICAL)
            {
              if (priv->overlay.height > priv->slider.height)
                {
                  /* Limit x and y within the overlay. */
                  x = priv->thumb_win.x;
                  y = CLAMP (event->y_root - priv->pointer.y,
                             priv->thumb_win.y + priv->overlay.y,
                             priv->thumb_win.y + priv->overlay.y + priv->overlay.height - priv->slider.height);
                }
              else
                {
                  x = priv->thumb_win.x;
                  y = priv->thumb_win.y + priv->slider.y;
                }
            }
          else
            {
              if (priv->overlay.width > priv->slider.width)
                {
                  /* Limit x and y within the overlay. */
                  x = CLAMP (event->x_root - priv->pointer.x,
                             priv->thumb_win.x + priv->overlay.x,
                             priv->thumb_win.x + priv->overlay.x + priv->overlay.width - priv->slider.width);
                  y = priv->thumb_win.y;
                }
              else
                {
                  x = priv->thumb_win.x + priv->slider.x;
                  y = priv->thumb_win.y;
                }
            }

          /* There's no need to stop animations,
           * since the reconnecting animation should not have
           * state OS_STATE_CONNECTED, unless it's ended. 
           * Just capture the movement and change adjustment's value (scroll). */
          capture_movement (scrollbar, event->x_root, event->y_root);
        }
      else
        {
          /* This is a disconnected scroll, works subtly different.
           * It has to take care of reconnection,
           * and scrolling is not allowed when hitting an edge. */

          /* Limit x and y within the allocation. */
          if (priv->orientation == GTK_ORIENTATION_VERTICAL)
            {
              x = priv->thumb_win.x;
              y = CLAMP (event->y_root - priv->pointer.y,
                         priv->thumb_win.y,
                         priv->thumb_win.y + priv->thumb_all.height - priv->slider.height);
            }
          else
            {
              x = CLAMP (event->x_root - priv->pointer.x,
                         priv->thumb_win.x,
                         priv->thumb_win.x + priv->thumb_all.width - priv->slider.width);
              y = priv->thumb_win.y;
            }

          /* Disconnected scroll while detached,
           * do not scroll when hitting an edge. */
          if ((priv->orientation == GTK_ORIENTATION_VERTICAL &&
               y > priv->thumb_win.y &&
               y < priv->thumb_win.y + priv->thumb_all.height - priv->slider.height) ||
              (priv->orientation == GTK_ORIENTATION_HORIZONTAL &&
               x > priv->thumb_win.x &&
               x < priv->thumb_win.x + priv->thumb_all.width - priv->slider.width))
            {
              /* Stop the animation now.
               * Only the reconnecting animation can be running now,
               * because the paging animations were stop before. */
              os_animation_stop (priv->animation, NULL);

              /* Capture the movement and change adjustment's value (scroll). */
              capture_movement (scrollbar, event->x_root, event->y_root);
            }
          else if (!os_animation_is_running (priv->animation) &&
                   !(priv->state & OS_STATE_DETACHED) &&
                   !(priv->state & OS_STATE_FINE_SCROLL))
            {
              /* Animate scrolling till reaches the  edge. */
              if ((priv->orientation == GTK_ORIENTATION_VERTICAL && y <= priv->thumb_win.y) ||
                  (priv->orientation == GTK_ORIENTATION_HORIZONTAL && x <= priv->thumb_win.x))
                priv->value = gtk_adjustment_get_lower (priv->adjustment);
              else
                priv->value = gtk_adjustment_get_upper (priv->adjustment) -
                              gtk_adjustment_get_page_size (priv->adjustment);

              /* Proceed with the reconnection only if needed. */
              if (priv->value != gtk_adjustment_get_value (priv->adjustment))
                {
                  /* If the thumb is not detached, proceed with reconnection. */
                  priv->state |= OS_STATE_RECONNECTING;

                  os_animation_set_duration (priv->animation, MIN_DURATION_SCROLLING);

                  /* Start the scrolling animation. */
                  os_animation_start (priv->animation);
                }
            }
        }

      /* Move the thumb window. */
      move_thumb (scrollbar, x, y);

      /* Adjust slide values in some special situations,
       * update the tail if the thumb is detached and
       * check if the movement changed the thumb state to connected. */
      if (priv->orientation == GTK_ORIENTATION_VERTICAL)
        {
          if (gtk_adjustment_get_value (priv->adjustment) <= gtk_adjustment_get_lower (priv->adjustment))
            {
              if (priv->state & OS_STATE_DETACHED)
                update_tail (scrollbar);

              if (!(priv->state & OS_STATE_CONNECTED))
                check_connection (scrollbar);

              priv->slide_initial_slider_position = 0;
              priv->slide_initial_coordinate = MAX (event->y_root, priv->thumb_win.y + priv->pointer.y);
            }
          else if (priv->overlay.y + priv->overlay.height >= priv->trough.height)
            {
              if (priv->state & OS_STATE_DETACHED)
                update_tail (scrollbar);

              if (!(priv->state & OS_STATE_CONNECTED))
                check_connection (scrollbar);

              priv->slide_initial_slider_position = priv->trough.height - MAX (priv->overlay.height, priv->slider.height);
              priv->slide_initial_coordinate = MIN (event->y_root, (priv->thumb_win.y + priv->trough.height -
                                                                    priv->slider.height + priv->pointer.y));
            }
        }
      else
        {
          if (gtk_adjustment_get_value (priv->adjustment) <= gtk_adjustment_get_lower (priv->adjustment))
            {
              if (priv->state & OS_STATE_DETACHED)
                update_tail (scrollbar);

              if (!(priv->state & OS_STATE_CONNECTED))
                check_connection (scrollbar);

              priv->slide_initial_slider_position = 0;
              priv->slide_initial_coordinate = MAX (event->x_root, priv->thumb_win.x + priv->pointer.x);
            }
          else if (priv->overlay.x + priv->overlay.width >= priv->trough.width)
            {
              if (priv->state & OS_STATE_DETACHED)
                update_tail (scrollbar);

              if (!(priv->state & OS_STATE_CONNECTED))
                check_connection (scrollbar);

              priv->slide_initial_slider_position = priv->trough.width - MAX (priv->overlay.width, priv->slider.width);
              priv->slide_initial_coordinate = MIN (event->x_root, (priv->thumb_win.x + priv->trough.width -
                                                                    priv->slider.width + priv->pointer.x));
            }
        }
    }

  return FALSE;
}

/* Mouse wheel delta. */
static gdouble
get_wheel_delta (GtkScrollbar       *scrollbar,
                 GdkScrollDirection  direction)
{
  OsScrollbarPrivate *priv;
  gdouble delta;

  priv = get_private (GTK_WIDGET (scrollbar));

  delta = pow (gtk_adjustment_get_page_size (priv->adjustment), 2.0 / 3.0);

  if (direction == GDK_SCROLL_UP ||
      direction == GDK_SCROLL_LEFT)
    delta = - delta;

  return delta;
}

static gboolean
thumb_scroll_event_cb (GtkWidget      *widget,
                       GdkEventScroll *event,
                       gpointer        user_data)
{
  GtkScrollbar *scrollbar;
  OsScrollbarPrivate *priv;
  gdouble delta;

#ifdef USE_GTK3
  /* Gtk+ 3.3.18 adds a smooth scroll support,
   * but at the moment is not ready to be used without various issues.
   * Don't use it for thumb scrolling in overlay scrollbar. */
  if (event->direction == GDK_SCROLL_SMOOTH)
    return FALSE;
#endif

  scrollbar = GTK_SCROLLBAR (user_data);
  priv = get_private (GTK_WIDGET (scrollbar));

  /* Stop the scrolling animation if it's running. */
  os_animation_stop (priv->animation, NULL);

  /* Slow down scroll wheel with the modifier key pressed,
   * by a 0.2 factor. */
  if (event->state & MODIFIER_KEY)
    delta = get_wheel_delta (scrollbar, event->direction) * 0.2;
  else
    delta = get_wheel_delta (scrollbar, event->direction);

  gtk_adjustment_set_value (priv->adjustment,
                            CLAMP (gtk_adjustment_get_value (priv->adjustment) + delta,
                                   gtk_adjustment_get_lower (priv->adjustment),
                                   (gtk_adjustment_get_upper (priv->adjustment) -
                                    gtk_adjustment_get_page_size (priv->adjustment))));

  /* Deal with simultaneous events. */
  if (priv->event & OS_EVENT_BUTTON_PRESS)
    {
      priv->event &= ~(OS_EVENT_MOTION_NOTIFY);

      /* we need to update the slide values
       * with the current position. */
      calc_precise_slide_values (scrollbar, event->x_root, event->y_root);
    }

  return FALSE;
}

static void
thumb_unmap_cb (GtkWidget *widget,
                gpointer   user_data)
{
  GtkScrollbar *scrollbar;
  OsScrollbarPrivate *priv;

  scrollbar = GTK_SCROLLBAR (user_data);
  priv = get_private (GTK_WIDGET (scrollbar));

  priv->event = OS_EVENT_NONE;
  priv->hidable_thumb = TRUE;
  priv->state &= OS_STATE_FULLSIZE;

  /* Remove running hide timeout, if there is one. */
  if (priv->source_hide_thumb_id != 0)
    {
      g_source_remove (priv->source_hide_thumb_id);
      priv->source_hide_thumb_id = 0;
    }

  /* This could hardly still be running,
   * but it is not impossible. */
  if (priv->source_show_thumb_id != 0)
    {
      g_source_remove (priv->source_show_thumb_id);
      priv->source_show_thumb_id = 0;
    }

  os_bar_set_detached (priv->bar, FALSE, TRUE);
}

/* Toplevel functions. */

static gboolean
toplevel_configure_event_cb (GtkWidget         *widget,
                             GdkEventConfigure *event,
                             gpointer           user_data)
{
  GtkScrollbar *scrollbar = GTK_SCROLLBAR (user_data);
  OsScrollbarPrivate *priv = get_private (GTK_WIDGET (scrollbar));
  const gint64 current_time = g_get_monotonic_time ();
  const gint64 end_time = priv->present_time + TIMEOUT_PRESENT_WINDOW * 1000;

#ifdef USE_GTK3
  /* If the widget is mapped, is not insentitive
   * and the configure-event happens after
   * the PropertyNotify _NET_ACTIVE_WINDOW event,
   * see if the mouse pointer is over this window, if TRUE,
   * proceed with bar_set_state_from_pointer (). */
  if (!is_insensitive (scrollbar) &&
      (current_time > end_time) &&
      gtk_widget_get_mapped (GTK_WIDGET (scrollbar)))
    {
      if (!priv->active_window)
        {
          GdkWindow *parent;

          /* Loop through parent windows until it reaches
           * either an unknown GdkWindow (NULL),
           * or the toplevel window. */
          parent = window_at_pointer (gtk_widget_get_window (GTK_WIDGET (scrollbar)), NULL, NULL);
          while (parent != NULL)
            {
              if (event->window == parent)
                break;

              parent = gdk_window_get_parent (parent);
            }

          if (parent != NULL)
            {
              gint x, y;

              window_get_pointer (gtk_widget_get_window (GTK_WIDGET (scrollbar)), &x, &y, NULL);

              /* When the window is resized (maximize/restore),
               * check the position of the pointer
               * and set the state accordingly. */
              bar_set_state_from_pointer (scrollbar, x, y);
            }
        }
      else
        {
          priv->deactivable_bar = FALSE;
          os_bar_set_active (priv->bar, TRUE, TRUE);
        }
    }
#endif

  if (current_time > end_time)
    gtk_widget_hide (priv->thumb);

  priv->state &= ~(OS_STATE_LOCKED);

  calc_layout_bar (scrollbar, gtk_adjustment_get_value (priv->adjustment));
  calc_layout_slider (scrollbar, gtk_adjustment_get_value (priv->adjustment));

  return FALSE;
}

/* widget's window functions. */

/* Move the thumb in the proximity area. */
static void
adjust_thumb_position (GtkScrollbar *scrollbar,
                       gdouble       event_x,
                       gdouble       event_y)
{
  OsScrollbarPrivate *priv;
  gint x, y, x_pos, y_pos;

  priv = get_private (GTK_WIDGET (scrollbar));

  gdk_window_get_origin (gtk_widget_get_window (GTK_WIDGET (scrollbar)), &x_pos, &y_pos);

  if (priv->state & OS_STATE_LOCKED)
    {
      gint x_pos_t, y_pos_t;

      gdk_window_get_origin (gtk_widget_get_window (priv->thumb), &x_pos_t, &y_pos_t);

      /* If the pointer is moving in the area of the proximity
       * at the left of the thumb (so, not vertically intercepting the thumb),
       * unlock it. Viceversa for the horizontal orientation.
       * The flag OS_STATE_LOCKED is set only for a mapped thumb,
       * so we can freely ask for its position and do our calculations. */
      if ((priv->side == OS_SIDE_RIGHT && x_pos + event_x <= x_pos_t) ||
          (priv->side == OS_SIDE_BOTTOM && y_pos + event_y <= y_pos_t) ||
          (priv->side == OS_SIDE_LEFT && x_pos + event_x >= x_pos_t + priv->thumb_all.width) ||
          (priv->side == OS_SIDE_TOP && y_pos + event_y >= y_pos_t + priv->thumb_all.height))
        priv->state &= ~(OS_STATE_LOCKED);
      else
        return;
    }

  /* Calculate priv->thumb_win.x and priv->thumb_win.y
   * (thumb window allocation in root coordinates).
   * I guess it's faster to store these values,
   * instead calling everytime gdk_window_get_origin (),
   * because it requires less calls than Gdk, which, in fact,
   * loops through multiple functions to return them. */
  priv->thumb_win.x = x_pos + priv->thumb_all.x;
  priv->thumb_win.y = y_pos + priv->thumb_all.y;

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      x = priv->thumb_all.x;

      if (priv->overlay.height > priv->slider.height)
        {
          /* Overlay (bar) is longer than the slider (thumb).
           * The thumb is locked within the overlay,
           * until the mouse is on the middle of page up or page down buttons. */

          if (event_y < priv->thumb_all.y + priv->overlay.y + priv->slider.height / 4)
            {
              /* Align to page up. */
              y = event_y - priv->slider.height / 4;
            }
          else if (event_y > priv->thumb_all.y + priv->overlay.y + priv->overlay.height - priv->slider.height / 4)
            {
              /* Align to page down. */
              y = event_y - priv->slider.height * 3 / 4;
            }
          else
            {
              /* Lock within the overlay. */
              y = CLAMP (event_y - priv->slider.height / 2,
                         priv->thumb_all.y + priv->overlay.y,
                         priv->thumb_all.y + priv->overlay.y + priv->overlay.height - priv->slider.height);
            }
        }
      else
        {
          /* Slider (thumb) is longer than (or equal) the overlay (bar).
           * The thumb is locked to its natural position (priv->slider.y),
           * until the mouse is on the middle of page up or page down buttons. */

          if (event_y < priv->thumb_all.y + priv->slider.y + priv->slider.height / 4)
            {
              /* Align to page up. */
              y = event_y - priv->slider.height / 4;
            }
          else if (event_y > priv->thumb_all.y + priv->slider.y + priv->slider.height - priv->slider.height / 4)
            {
              /* Align to page down. */
              y = event_y - priv->slider.height * 3 / 4;
            }
          else
            {
              /* Lock to its natural position. */
              y = priv->thumb_all.y + priv->slider.y;
            }
        }
      /* Restrict y within the thumb allocation. */
      y = CLAMP (y, priv->thumb_all.y, priv->thumb_all.y + priv->thumb_all.height - priv->slider.height);
    }
  else
    {
      y = priv->thumb_all.y;

      if (priv->overlay.width > priv->slider.width)
        {
          /* Overlay (bar) is longer than the slider (thumb).
           * The thumb is locked within the overlay,
           * until the mouse is on the middle of page up or page down buttons. */

          if (event_x < priv->thumb_all.x + priv->overlay.x + priv->slider.width / 4)
            {
              /* Align to page up. */
              x = event_x - priv->slider.width / 4;
            }
          else if (event_x > priv->thumb_all.x + priv->overlay.x + priv->overlay.width - priv->slider.width / 4)
            {
              /* Align to page down. */
              x = event_x - priv->slider.width * 3 / 4;
            }
          else
            {
              /* Lock within the overlay. */
              x = CLAMP (event_x - priv->slider.width / 2,
                         priv->thumb_all.x + priv->overlay.x,
                         priv->thumb_all.x + priv->overlay.x + priv->overlay.width - priv->slider.width);
            }

        }
      else
        {
          /* Slider (thumb) is longer than (or equal) the overlay (bar).
           * The thumb is locked to its natural position (priv->slider.x),
           * until the mouse is on the middle of page up or page down buttons. */

          if (event_x < priv->thumb_all.x + priv->slider.x + priv->slider.width / 4)
            {
              /* Align to page up. */
              x = event_x - priv->slider.width / 4;
            }
          else if (event_x > priv->thumb_all.x + priv->slider.x + priv->slider.width - priv->slider.width / 4)
            {
              /* Align to page down. */
              x = event_x - priv->slider.width * 3 / 4;
            }
          else
            {
              /* Lock to its natural position. */
              x = priv->thumb_all.x + priv->slider.x;
            }
        }
      /* Restrict x within the thumb allocation. */
      x = CLAMP (x, priv->thumb_all.x, priv->thumb_all.x + priv->thumb_all.width - priv->slider.width);
    }

  move_thumb (scrollbar, x_pos + x, y_pos + y);
}

/* Checks if the pointer is in the proximity area. */
static gboolean
check_proximity (GtkScrollbar *scrollbar,
                 gint          x,
                 gint          y)
{
  OsScrollbarPrivate *priv;
  gint proximity_size;

  priv = get_private (GTK_WIDGET (scrollbar));

  proximity_size = PROXIMITY_SIZE;

  /* If the thumb is internal, enlarge the proximity area. */
  if (priv->state & OS_STATE_INTERNAL)
    {
      gint x_pos, y_pos;

      gdk_window_get_origin (gtk_widget_get_window (priv->thumb), &x_pos, &y_pos);

      /* This absolute value to add is obtained subtracting
       * the real position of the thumb from the theoretical position.
       * This difference should consist exactly in the amount of pixels (of the thumb)
       * falling in the proximity, that is the value we want to enlarge this area. */
      if (priv->orientation == GTK_ORIENTATION_VERTICAL)
        proximity_size += abs (priv->thumb_win.x - x_pos);
      else
        proximity_size += abs (priv->thumb_win.y - y_pos);
    }

  switch (priv->side)
  {
    case OS_SIDE_RIGHT:
      return (x >= priv->bar_all.x + priv->bar_all.width - proximity_size &&
              x <= priv->bar_all.x + priv->bar_all.width) &&
             (y >= priv->bar_all.y &&
              y <= priv->bar_all.y + priv->bar_all.height);
      break;
    case OS_SIDE_BOTTOM:
      return (y >= priv->bar_all.y + priv->bar_all.height - proximity_size &&
              y <= priv->bar_all.y + priv->bar_all.height) &&
             (x >= priv->bar_all.x &&
              x <= priv->bar_all.x + priv->bar_all.width);
      break;
    case OS_SIDE_LEFT:
      return (x <= priv->bar_all.x + priv->bar_all.width + proximity_size &&
              x >= priv->bar_all.x) &&
             (y >= priv->bar_all.y &&
              y <= priv->bar_all.y + priv->bar_all.height);
      break;
    case OS_SIDE_TOP:
      return (y <= priv->bar_all.y + priv->bar_all.height + proximity_size &&
              y >= priv->bar_all.y) &&
             (x >= priv->bar_all.x &&
              x <= priv->bar_all.x + priv->bar_all.width);
      break;
    default:
      break;
  }

  return FALSE;
}

/* Returns TRUE if touch mode is enabled. */
static gboolean
is_touch_mode (GtkWidget *widget,
               gint       device_id)
{
#ifdef USE_GTK3
  switch (scrollbar_mode)
  {
    case SCROLLBAR_MODE_OVERLAY_AUTO:
    default:
      /* Continue detecting source type. */
      break;
    case SCROLLBAR_MODE_OVERLAY_POINTER:
      /* Touch mode always disabled. */
      return FALSE;
      break;
    case SCROLLBAR_MODE_OVERLAY_TOUCH:
      /* Touch mode always enabled. */
      return TRUE;
      break;
  }

  /* Use some sort of cache for the device.
   * Update the input source only if the device_id
   * is different from the previous one. */
  if (os_device_id != device_id)
    {
      GdkDeviceManager *device_manager;
      GdkDevice *device;
      GdkWindow *window;

      /* Update the static os_device_id variable. */
      os_device_id = device_id;

      window = gtk_widget_get_window (widget);
      device_manager = gdk_display_get_device_manager (gdk_window_get_display (window));
      device = gdk_x11_device_manager_lookup (device_manager, os_device_id);

      /* Return FALSE if we don't recognize the device. */
      if (!device)
        return FALSE;

      /* Update the static os_input_source variable. */
      os_input_source = gdk_device_get_source (device);
    }

  /* Detect touch mode accordingly to the input source type. */
  if (os_input_source == GDK_SOURCE_TOUCHSCREEN)
    return TRUE;
  else
    return FALSE;
#else
  return scrollbar_mode == SCROLLBAR_MODE_OVERLAY_TOUCH;
#endif
}

/* Callback that shows the thumb if it's the case. */
static gboolean
show_thumb_cb (gpointer user_data)
{
  GtkScrollbar *scrollbar;
  OsScrollbarPrivate *priv;

  scrollbar = GTK_SCROLLBAR (user_data);
  priv = get_private (GTK_WIDGET (scrollbar));

  if (!priv->hidable_thumb)
    {
      gtk_widget_show (priv->thumb);

      update_tail (scrollbar);
    }

  priv->source_show_thumb_id = 0;

  return FALSE;
}

/* Adds a timeout to reveal the thumb. */
static void
show_thumb (GtkScrollbar *scrollbar)
{
  OsScrollbarPrivate *priv;

  priv = get_private (GTK_WIDGET (scrollbar));

  /* Just update the tail if the thumb is already mapped. */
  if (gtk_widget_get_mapped (priv->thumb))
    {
      update_tail (scrollbar);
      return;
    }

  if (priv->state & OS_STATE_INTERNAL)
    {
      /* If the scrollbar is close to one edge of the screen,
       * show it immediately, ignoring the timeout,
       * to preserve Fitts' law. */
      if (priv->source_show_thumb_id != 0)
        {
          g_source_remove (priv->source_show_thumb_id);
          priv->source_show_thumb_id = 0;
        }

      gtk_widget_show (priv->thumb);

      update_tail (scrollbar);
    }
  else if (priv->source_show_thumb_id == 0)
    priv->source_show_thumb_id = g_timeout_add (TIMEOUT_THUMB_SHOW,
                                                show_thumb_cb,
                                                scrollbar);
}

/* Filter function applied to the toplevel window. */
typedef enum
{
  OS_XEVENT_NONE,
  OS_XEVENT_BUTTON_PRESS,
  OS_XEVENT_BUTTON_RELEASE,
  OS_XEVENT_LEAVE,
  OS_XEVENT_MOTION
} OsXEvent;

static GdkFilterReturn
window_filter_func (GdkXEvent *gdkxevent,
                    GdkEvent  *event,
                    gpointer   user_data)
{
  GtkScrollbar *scrollbar;
  OsScrollbarPrivate *priv;
  XEvent *xev;

  g_return_val_if_fail (GTK_IS_SCROLLBAR (user_data), GDK_FILTER_CONTINUE);

  scrollbar = GTK_SCROLLBAR (user_data);
  priv = get_private (GTK_WIDGET (scrollbar));

  xev = gdkxevent;

  g_return_val_if_fail (OS_IS_BAR (priv->bar), GDK_FILTER_CONTINUE);
  g_return_val_if_fail (OS_IS_THUMB (priv->thumb), GDK_FILTER_CONTINUE);

  if (!(priv->state & OS_STATE_FULLSIZE))
    {
      OsXEvent os_xevent;
      gdouble event_x, event_y;
      gint sourceid;

      os_xevent = OS_XEVENT_NONE;

      event_x = 0;
      event_y = 0;

      sourceid = 0;

#ifdef USE_GTK3
      if (xev->type == GenericEvent)
        {
          /* Deal with XInput 2 events. */
          XIDeviceEvent *xiev;

          xiev = xev->xcookie.data;

          sourceid = xiev->sourceid;

          if (xiev->evtype == XI_ButtonPress)
            os_xevent = OS_XEVENT_BUTTON_PRESS;

          if (xiev->evtype == XI_ButtonRelease)
            {
              os_xevent = OS_XEVENT_BUTTON_RELEASE;
              event_x = xiev->event_x;
              event_y = xiev->event_y;
            }

          if (xiev->evtype == XI_Leave)
            os_xevent = OS_XEVENT_LEAVE;

          if (xiev->evtype == XI_Motion)
            {
              os_xevent = OS_XEVENT_MOTION;
              event_x = xiev->event_x;
              event_y = xiev->event_y;
            }
        }
      else
        {
#endif
          /* Deal with X core events, when apps (like rhythmbox),
           * are using gdk_disable_miltidevice (). */
          if (xev->type == ButtonPress)
            os_xevent = OS_XEVENT_BUTTON_PRESS;

          if (xev->type == ButtonRelease)
            {
              os_xevent = OS_XEVENT_BUTTON_RELEASE;
              event_x = xev->xbutton.x;
              event_y = xev->xbutton.y;
            }

          if (xev->type == LeaveNotify)
            os_xevent = OS_XEVENT_LEAVE;

          if (xev->type == MotionNotify)
            {
              os_xevent = OS_XEVENT_MOTION;
              event_x = xev->xmotion.x;
              event_y = xev->xmotion.y;
            }
#ifdef USE_GTK3
        }
#endif

        if (os_xevent == OS_XEVENT_BUTTON_PRESS)
          {
            priv->window_button_press = TRUE;

            if (priv->source_show_thumb_id != 0)
              {
                g_source_remove (priv->source_show_thumb_id);
                priv->source_show_thumb_id = 0;
              }

            gtk_widget_hide (priv->thumb);
          }

        if (priv->window_button_press && os_xevent == OS_XEVENT_BUTTON_RELEASE)
          {
            priv->window_button_press = FALSE;

            /* Proximity area. */
            if (check_proximity (scrollbar, event_x, event_y))
              {
                priv->hidable_thumb = FALSE;

                adjust_thumb_position (scrollbar, event_x, event_y);

                if (priv->state & OS_STATE_LOCKED)
                  return GDK_FILTER_CONTINUE;

                if (!is_touch_mode (GTK_WIDGET (scrollbar), sourceid) && !priv->resizing_paned)
                  show_thumb (scrollbar);
              }
          }

        if (os_xevent == OS_XEVENT_LEAVE)
          {
            priv->window_button_press = FALSE;

#ifdef USE_GTK3
            /* Never deactivate the bar in an active window. */
            if (!priv->active_window)
              {
                priv->deactivable_bar = TRUE;

                if (priv->source_deactivate_bar_id != 0)
                  g_source_remove (priv->source_deactivate_bar_id);

                priv->source_deactivate_bar_id = g_timeout_add (TIMEOUT_TOPLEVEL_HIDE,
                                                                deactivate_bar_cb,
                                                                scrollbar);
              }
#endif

            if (gtk_widget_get_mapped (priv->thumb) &&
                !(priv->event & OS_EVENT_BUTTON_PRESS))
              {
                priv->hidable_thumb = TRUE;

                if (priv->source_hide_thumb_id != 0)
                  g_source_remove (priv->source_hide_thumb_id);

                priv->source_hide_thumb_id = g_timeout_add (TIMEOUT_TOPLEVEL_HIDE,
                                                            hide_thumb_cb,
                                                            scrollbar);
              }

            if (priv->source_show_thumb_id != 0)
              {
                g_source_remove (priv->source_show_thumb_id);
                priv->source_show_thumb_id = 0;
              }

            if (priv->source_unlock_thumb_id != 0)
              g_source_remove (priv->source_unlock_thumb_id);

            priv->source_unlock_thumb_id = g_timeout_add (TIMEOUT_TOPLEVEL_HIDE,
                                                          unlock_thumb_cb,
                                                          scrollbar);
          }

        /* Get the motion_notify_event trough XEvent. */
        if (!priv->window_button_press && os_xevent == OS_XEVENT_MOTION)
          {
#ifdef USE_GTK3
            /* React to motion_notify_event
             * and set the state accordingly. */
            if (!is_insensitive (scrollbar) && !priv->active_window)
              bar_set_state_from_pointer (scrollbar, event_x, event_y);
#endif

            /* Proximity area. */
            if (check_proximity (scrollbar, event_x, event_y))
              {
                priv->hidable_thumb = FALSE;

                if (priv->source_hide_thumb_id != 0)
                  {
                    g_source_remove (priv->source_hide_thumb_id);
                    priv->source_hide_thumb_id = 0;
                  }

                adjust_thumb_position (scrollbar, event_x, event_y);

                if (priv->state & OS_STATE_LOCKED)
                  return GDK_FILTER_CONTINUE;

                if (!is_touch_mode (GTK_WIDGET (scrollbar), sourceid) && !priv->resizing_paned)
                  show_thumb (scrollbar);
              }
            else
              {
                priv->state &= ~(OS_STATE_LOCKED);

                if (priv->source_show_thumb_id != 0)
                  {
                    g_source_remove (priv->source_show_thumb_id);
                    priv->source_show_thumb_id = 0;
                  }

                if (gtk_widget_get_mapped (priv->thumb) &&
                    !(priv->event & OS_EVENT_BUTTON_PRESS))
                  {
                    priv->hidable_thumb = TRUE;

                    if (priv->source_hide_thumb_id == 0)
                      priv->source_hide_thumb_id = g_timeout_add (TIMEOUT_PROXIMITY_HIDE,
                                                                  hide_thumb_cb,
                                                                  scrollbar);
                  }
              }
        }
    }

  return GDK_FILTER_CONTINUE;
}

/* Add the window filter function. */
static void
add_window_filter (GtkScrollbar *scrollbar)
{
  OsScrollbarPrivate *priv;

  priv = get_private (GTK_WIDGET (scrollbar));

  /* Don't add duplicated filters. */
  if (!priv->filter.running &&
      gtk_widget_get_realized (GTK_WIDGET (scrollbar)))
    {
      priv->filter.running = TRUE;
      gdk_window_add_filter (gtk_widget_get_window (GTK_WIDGET (scrollbar)),
                             window_filter_func,
                             scrollbar);
    }
}

/* Remove the window filter function. */
static void
remove_window_filter (GtkScrollbar *scrollbar)
{
  OsScrollbarPrivate *priv;

  priv = get_private (GTK_WIDGET (scrollbar));

  /* Remove only if the filter is running. */
  if (priv->filter.running &&
      gtk_widget_get_realized (GTK_WIDGET (scrollbar)))
    {
      priv->filter.running = FALSE;
      gdk_window_remove_filter (gtk_widget_get_window (GTK_WIDGET (scrollbar)),
                                window_filter_func,
                                scrollbar);
    }
}

static gboolean
use_overlay_scrollbar (void)
{
#ifdef USE_GTK3
  return scrollbar_mode != SCROLLBAR_MODE_NORMAL;
#else
  return scrollbar_mode != SCROLLBAR_MODE_NORMAL && ubuntu_gtk_get_use_overlay_scrollbar ();
#endif
}

static void
hijacked_scrollbar_dispose (GObject *object)
{
  if (use_overlay_scrollbar ())
    {
      GtkScrollbar *scrollbar;
      OsScrollbarPrivate *priv;

      scrollbar = GTK_SCROLLBAR (object);

      os_root_list = g_slist_remove (os_root_list, scrollbar);

      if (os_root_list == NULL)
        {
          if (os_workarea != NULL)
            {
              cairo_region_destroy (os_workarea);
              os_workarea = NULL;
            }

          gdk_window_remove_filter (gdk_get_default_root_window (),
                                    root_filter_func, NULL);
        }

      priv = lookup_private (GTK_WIDGET(scrollbar));
      if (priv != NULL)
        {
          if (priv->source_deactivate_bar_id != 0)
            {
              g_source_remove (priv->source_deactivate_bar_id);
              priv->source_deactivate_bar_id = 0;
            }

          if (priv->source_hide_thumb_id != 0)
            {
              g_source_remove (priv->source_hide_thumb_id);
              priv->source_hide_thumb_id = 0;
            }

          if (priv->source_show_thumb_id != 0)
            {
              g_source_remove (priv->source_show_thumb_id);
              priv->source_show_thumb_id = 0;
            }

          if (priv->source_unlock_thumb_id != 0)
            {
              g_source_remove (priv->source_unlock_thumb_id);
              priv->source_unlock_thumb_id = 0;
            }

          if (priv->animation != NULL)
            {
              g_object_unref (priv->animation);
              priv->animation = NULL;
            }

          if (priv->bar != NULL)
            {
              g_object_unref (priv->bar);
              priv->bar = NULL;
            }

          if (priv->window_group != NULL)
            {
              g_object_unref (priv->window_group);
              priv->window_group = NULL;
            }

          swap_adjustment (scrollbar, NULL);
          swap_thumb (scrollbar, NULL);
        }
    }

  (* pre_hijacked_scrollbar_dispose) (object);
}

#ifdef USE_GTK3
static gboolean
hijacked_scrollbar_draw (GtkWidget *widget,
                         cairo_t   *cr)
{
  if (use_overlay_scrollbar ())
    return TRUE;

  return (* pre_hijacked_scrollbar_draw) (widget, cr);
}
#else
static gboolean
hijacked_scrollbar_expose_event (GtkWidget      *widget,
                                 GdkEventExpose *event)
{
  if (use_overlay_scrollbar ())
    return TRUE;

  return (* pre_hijacked_scrollbar_expose_event) (widget, event);
}
#endif

#ifdef USE_GTK3
static void
hijacked_scrollbar_get_preferred_width (GtkWidget *widget,
                                        gint      *minimal_width,
                                        gint      *natural_width)
{
  if (use_overlay_scrollbar ())
    {
      OsScrollbarPrivate *priv;

      priv = get_private (widget);

      if (priv->orientation == GTK_ORIENTATION_VERTICAL)
        *minimal_width = *natural_width = 0;
      else
        {
          *minimal_width = MIN_THUMB_HEIGHT;
          *natural_width = THUMB_HEIGHT;
        }

      return;
    }

  (* pre_hijacked_scrollbar_get_preferred_width) (widget, minimal_width, natural_width);
}

static void
hijacked_scrollbar_get_preferred_height (GtkWidget *widget,
                                         gint      *minimal_height,
                                         gint      *natural_height)
{
  if (use_overlay_scrollbar ())
    {
      OsScrollbarPrivate *priv;

      priv = get_private (widget);

      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        *minimal_height = *natural_height = 0;
      else
        {
          *minimal_height = MIN_THUMB_HEIGHT;
          *natural_height = THUMB_HEIGHT;
        }

      return;
    }

  (* pre_hijacked_scrollbar_get_preferred_height) (widget, minimal_height, natural_height);
}
#endif

static void
hijacked_scrollbar_grab_notify (GtkWidget *widget,
                                gboolean   was_grabbed)
{
  if (use_overlay_scrollbar ())
    return;

  (* pre_hijacked_scrollbar_grab_notify) (widget, was_grabbed);
}

static void
hijacked_scrollbar_hide (GtkWidget *widget)
{
  if (use_overlay_scrollbar ())
    {
      (* widget_class_hide) (widget);

      return;
    }

  (* pre_hijacked_scrollbar_hide) (widget);
}

#ifdef USE_GTK3
/* Return TRUE if the widget is in backdrop window. */
static gboolean
is_backdrop_window (GtkWidget *widget)
{
  return (gtk_widget_get_state_flags (widget) & GTK_STATE_FLAG_BACKDROP) != 0;
}
#endif

static void
hijacked_scrollbar_map (GtkWidget *widget)
{
  if (use_overlay_scrollbar ())
    {
      GtkScrollbar *scrollbar;
      OsScrollbarPrivate *priv;

      scrollbar = GTK_SCROLLBAR (widget);
      priv = get_private (widget);

      (* widget_class_map) (widget);

#ifdef USE_GTK3
      /* On map, check for the active window. */
      if (!is_backdrop_window (GTK_WIDGET (scrollbar)))
        {
          /* Stops potential running timeout. */
          if (priv->source_deactivate_bar_id != 0)
            {
              g_source_remove (priv->source_deactivate_bar_id);
              priv->source_deactivate_bar_id = 0;
            }

          priv->active_window = TRUE;
        }
      else
        priv->active_window = FALSE;

      if (!is_insensitive (scrollbar))
        {
          if (!priv->active_window)
            {
              gint x, y;

              window_get_pointer (gtk_widget_get_window (widget), &x, &y, NULL);

              /* When the scrollbar appears on screen (mapped),
               * for example when switching notebook page,
               * check the position of the pointer
               * and set the state accordingly. */
              bar_set_state_from_pointer (scrollbar, x, y);
            }
          else
            {
              /* On map-event of an active window,
               * the bar should be active. */
              priv->deactivable_bar = FALSE;
              os_bar_set_active (priv->bar, TRUE, FALSE);
            }
        }
#endif

      if (!(priv->state & OS_STATE_FULLSIZE))
        os_bar_show (priv->bar);

      if (!is_insensitive (scrollbar))
        {
          priv->filter.proximity = TRUE;
          add_window_filter (scrollbar);
        }

      return;
    }

  (* pre_hijacked_scrollbar_map) (widget);
}

static void
hijacked_scrollbar_realize (GtkWidget *widget)
{
  scrollbar_list = g_slist_prepend (scrollbar_list, widget);

  if (use_overlay_scrollbar ())
    {
      GtkScrollbar *scrollbar;
      OsScrollbarPrivate *priv;

      scrollbar = GTK_SCROLLBAR (widget);
      priv = get_private (widget);

      (* widget_class_realize) (widget);

      gtk_window_group_add_window (priv->window_group, GTK_WINDOW (gtk_widget_get_toplevel (widget)));

      gdk_window_set_events (gtk_widget_get_window (widget),
                             gdk_window_get_events (gtk_widget_get_window (widget)) |
                             GDK_BUTTON_PRESS_MASK |
                             GDK_BUTTON_RELEASE_MASK |
                             GDK_POINTER_MOTION_MASK);

      if (priv->filter.proximity)
        add_window_filter (scrollbar);

      g_signal_connect (G_OBJECT (gtk_widget_get_toplevel (widget)), "configure-event",
                        G_CALLBACK (toplevel_configure_event_cb), scrollbar);

      calc_layout_bar (scrollbar, gtk_adjustment_get_value (priv->adjustment));

      os_bar_set_parent (priv->bar, widget);

      return;
    }

  (* pre_hijacked_scrollbar_realize) (widget);
}

static void
hijacked_scrollbar_show (GtkWidget *widget)
{
  if (use_overlay_scrollbar ())
    {
      (* widget_class_show) (widget);

      return;
    }

  (* pre_hijacked_scrollbar_show) (widget);
}

/* Retrieve the side of the scrollbar. */
static void
retrieve_side (GtkScrollbar *scrollbar)
{
  GtkCornerType corner;
  OsScrollbarPrivate *priv;

  priv = get_private (GTK_WIDGET (scrollbar));

  corner = GPOINTER_TO_INT (g_object_get_qdata (G_OBJECT (scrollbar), os_quark_placement));

  if (GTK_IS_SCROLLED_WINDOW (gtk_widget_get_parent (GTK_WIDGET (scrollbar))))
    corner = gtk_scrolled_window_get_placement (GTK_SCROLLED_WINDOW (gtk_widget_get_parent (GTK_WIDGET (scrollbar))));

  /* GtkCornerType to OsSide. */
  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (corner == GTK_CORNER_TOP_LEFT ||
          corner == GTK_CORNER_TOP_RIGHT)
        priv->side = OS_SIDE_BOTTOM;
      else
        priv->side = OS_SIDE_TOP;
    }
  else
    {
      if (gtk_widget_get_direction (GTK_WIDGET (scrollbar)) == GTK_TEXT_DIR_LTR)
        {
          if (corner == GTK_CORNER_TOP_LEFT ||
              corner == GTK_CORNER_BOTTOM_LEFT)
            priv->side = OS_SIDE_RIGHT;
          else
            priv->side = OS_SIDE_LEFT;
        }
      else
        {
          if (corner == GTK_CORNER_TOP_RIGHT ||
              corner == GTK_CORNER_BOTTOM_RIGHT)
            priv->side = OS_SIDE_RIGHT;
          else
            priv->side = OS_SIDE_LEFT;
        }
    }
}

/* Retrieve if the thumb can resize its toplevel window. */
static void
retrieve_resizability (GtkScrollbar *scrollbar)
{
  GdkWindow *scrollbar_window;
  GdkWindow *toplevel_window;
  OsScrollbarPrivate *priv;
  gint x, y;
  gint x_pos, y_pos;

  priv = get_private (GTK_WIDGET (scrollbar));

  /* By default, they don't allow resize. */
  priv->allow_resize = FALSE;

  scrollbar_window = gtk_widget_get_window (GTK_WIDGET (scrollbar));

  if (!scrollbar_window)
    return;

  toplevel_window = gtk_widget_get_window (gtk_widget_get_toplevel (GTK_WIDGET (scrollbar)));

  gdk_window_get_origin (toplevel_window, &x, &y);

  gdk_window_get_root_coords (scrollbar_window,
                              priv->thumb_all.x, priv->thumb_all.y,
                              &x_pos, &y_pos);

  /* Check if the thumb is next to a window edge,
   * if that's the case, set the allow_resize gboolean. */
  switch (priv->side)
  {
    case OS_SIDE_RIGHT:
      if (x + gdk_window_get_width (toplevel_window) - x_pos <= THUMB_WIDTH)
        priv->allow_resize = TRUE;
      break;
    case OS_SIDE_BOTTOM:
      if (y + gdk_window_get_height (toplevel_window) - y_pos <= THUMB_WIDTH)
        priv->allow_resize = TRUE;
      break;
    case OS_SIDE_LEFT:
      if (x_pos - x <= THUMB_WIDTH)
        priv->allow_resize = TRUE;
      break;
    case OS_SIDE_TOP:
      if (y_pos - y <= THUMB_WIDTH)
        priv->allow_resize = TRUE;
      break;
    default:
      break;
  }

  GtkPaned *paned = GTK_PANED (gtk_widget_get_ancestor (GTK_WIDGET (scrollbar), GTK_TYPE_PANED));
  if (!paned)
    return;

  GdkWindow *handle_window = gtk_paned_get_handle_window (paned);

  gdk_window_get_origin (handle_window, &x, &y);

  priv->allow_resize_paned = FALSE;
  priv->resizing_paned = FALSE;

  /* Check if the thumb is next to a paned handle,
   * if that's the case, set the allow_resize_paned gboolean. */
  if (GTK_IS_HPANED(paned))
  {
    switch (priv->side)
    {
      case OS_SIDE_RIGHT:
        if (x + gdk_window_get_width (handle_window) - x_pos <= THUMB_WIDTH)
          priv->allow_resize_paned = TRUE;
        break;
      case OS_SIDE_LEFT:
        if (x_pos - x <= THUMB_WIDTH)
          priv->allow_resize_paned = TRUE;
        break;
      default:
        break;
    }
  }
  else if (GTK_IS_VPANED(paned))
  {
    switch (priv->side)
    {
      case OS_SIDE_BOTTOM:
        if (y + gdk_window_get_height (handle_window) - y_pos <= THUMB_WIDTH)
          priv->allow_resize_paned = TRUE;
        break;
      case OS_SIDE_TOP:
        if (y_pos - y <= THUMB_WIDTH)
          priv->allow_resize_paned = TRUE;
        break;
      default:
        break;
    }
  }
}

static void
hijacked_scrollbar_size_allocate (GtkWidget    *widget,
                                  GdkRectangle *allocation)
{
  if (use_overlay_scrollbar ())
    {
      GtkScrollbar *scrollbar;
      OsScrollbarPrivate *priv;

      scrollbar = GTK_SCROLLBAR (widget);
      priv = get_private (widget);

      /* Get the side, then move thumb and bar accordingly. */
      retrieve_side (scrollbar);

      priv->trough.x = allocation->x;
      priv->trough.y = allocation->y;
      priv->trough.width = allocation->width;
      priv->trough.height = allocation->height;

      priv->bar_all = *allocation;
      priv->thumb_all = *allocation;

      if (priv->orientation == GTK_ORIENTATION_VERTICAL)
        {
          priv->slider.width = THUMB_WIDTH;
          if (priv->slider.height != MIN (THUMB_HEIGHT, allocation->height))
            {
              priv->slider.height = MIN (THUMB_HEIGHT, allocation->height);
              os_thumb_resize (OS_THUMB (priv->thumb), priv->slider.width, priv->slider.height);
            }

          if (priv->side == OS_SIDE_RIGHT)
            priv->bar_all.x = allocation->x - BAR_SIZE;

          priv->bar_all.width = BAR_SIZE;

          priv->thumb_all.width = THUMB_WIDTH;

          if (priv->side == OS_SIDE_RIGHT)
            priv->thumb_all.x = allocation->x - priv->bar_all.width;
          else
            priv->thumb_all.x = allocation->x + priv->bar_all.width - priv->thumb_all.width;

          allocation->width = 0;
        }
      else
        {
          priv->slider.height = THUMB_WIDTH;
          if (priv->slider.width != MIN (THUMB_HEIGHT, allocation->width))
            {
              priv->slider.width = MIN (THUMB_HEIGHT, allocation->width);
              os_thumb_resize (OS_THUMB (priv->thumb), priv->slider.width, priv->slider.height);
            }

          if (priv->side == OS_SIDE_BOTTOM)
            priv->bar_all.y = allocation->y - BAR_SIZE;

          priv->bar_all.height = BAR_SIZE;

          priv->thumb_all.height = THUMB_WIDTH;

          if (priv->side == OS_SIDE_BOTTOM)
            priv->thumb_all.y = allocation->y - priv->bar_all.height;
          else
            priv->thumb_all.y = allocation->y + priv->bar_all.height - priv->thumb_all.height;

          allocation->height = 0;
        }

      if (priv->adjustment != NULL)
        {
          calc_layout_bar (scrollbar, gtk_adjustment_get_value (priv->adjustment));
          calc_layout_slider (scrollbar, gtk_adjustment_get_value (priv->adjustment));
        }

      os_bar_size_allocate (priv->bar, priv->bar_all);

      move_bar (scrollbar);

      /* Set resizability. */
      retrieve_resizability (scrollbar);

      gtk_widget_set_allocation (widget, allocation);

      return;
    }

  (* pre_hijacked_scrollbar_size_allocate) (widget, allocation);
}

/* Set the scrollbar to be insensitive. */
static void
set_insensitive (GtkScrollbar *scrollbar)
{
  OsScrollbarPrivate *priv;

  priv = get_private (GTK_WIDGET (scrollbar));

  priv->filter.proximity = FALSE;
  remove_window_filter (scrollbar);

#ifdef USE_GTK3
  os_bar_set_active (priv->bar, FALSE, FALSE);
#endif

  gtk_widget_hide (priv->thumb);
}

/* Set the scrollbar to be sensitive. */
static void
set_sensitive (GtkScrollbar *scrollbar)
{
  OsScrollbarPrivate *priv;

  priv = get_private (GTK_WIDGET (scrollbar));

  /* Only add the filter if the scrollbar is mapped.
   * It makes sense to me, if we are missing proximity areas,
   * it might worth having a look here. */
  if (gtk_widget_get_mapped (GTK_WIDGET (scrollbar)))
    {
      priv->filter.proximity = TRUE;
      add_window_filter (scrollbar);
    }

#ifdef USE_GTK3
  if (priv->active_window)
    os_bar_set_active (priv->bar, TRUE, FALSE);
  else if (gtk_widget_get_realized (GTK_WIDGET (scrollbar)))
    {
      gint x, y;

      window_get_pointer (gtk_widget_get_window (GTK_WIDGET (scrollbar)), &x, &y, NULL);

      /* When the window is unfocused,
       * check the position of the pointer
       * and set the state accordingly. */
      bar_set_state_from_pointer (scrollbar, x, y);
    }
#endif
}

#ifdef USE_GTK3
/* React on active window changes. */
static void
backdrop_state_flag_changed (GtkScrollbar *scrollbar)
{
  OsScrollbarPrivate *priv;

  priv = get_private (GTK_WIDGET (scrollbar));

  OS_DCHECK (scrollbar != NULL);

  /* Return if the scrollbar is insensitive. */
  if (is_insensitive (scrollbar))
    return;

  if (gtk_widget_get_mapped (GTK_WIDGET (scrollbar)))
    {
      if (!is_backdrop_window (GTK_WIDGET (scrollbar)))
        {
          /* Stops potential running timeout. */
          if (priv->source_deactivate_bar_id != 0)
            {
              g_source_remove (priv->source_deactivate_bar_id);
              priv->source_deactivate_bar_id = 0;
            }

          priv->active_window = TRUE;

          priv->deactivable_bar = FALSE;
          os_bar_set_active (priv->bar, TRUE, TRUE);
        }
      else if (priv->active_window)
        {
          GdkWindow *parent;
          GdkWindow *window;
          const gint64 current_time = g_get_monotonic_time ();
          const gint64 end_time = priv->present_time + TIMEOUT_PRESENT_WINDOW * 1000;

          priv->active_window = FALSE;

          /* Loop through parent windows until it reaches
           * either an unknown GdkWindow (NULL),
           * or the toplevel window. */
          window = gtk_widget_get_window (GTK_WIDGET (scrollbar));
          parent = window_at_pointer (window, NULL, NULL);
          while (parent != NULL)
            {
              if (window == parent)
                break;

              parent = gdk_window_get_parent (parent);
            }

          if (parent != NULL)
            {
              gint x, y;

              window_get_pointer (window, &x, &y, NULL);

              /* When the window is unfocused,
               * check the position of the pointer
               * and set the state accordingly. */
              bar_set_state_from_pointer (scrollbar, x, y);
            }
          else
            {
              /* If the pointer is outside of the window, set it inactive. */
              priv->deactivable_bar = TRUE;
              os_bar_set_active (priv->bar, FALSE, TRUE);
            }

          if ((current_time > end_time) && priv->thumb != NULL)
            gtk_widget_hide (priv->thumb);
        }
    }
}

static void
hijacked_scrollbar_state_flags_changed (GtkWidget    *widget,
                                        GtkStateFlags flags)
{
  GtkScrollbar *scrollbar;

  scrollbar = GTK_SCROLLBAR (widget);

  if ((flags & GTK_STATE_FLAG_BACKDROP) !=
      (gtk_widget_get_state_flags (widget) & GTK_STATE_FLAG_BACKDROP))
    backdrop_state_flag_changed (scrollbar);

  /* Only set the new state if the right bit changed. */
  if ((flags & GTK_STATE_FLAG_INSENSITIVE) !=
      (gtk_widget_get_state_flags (widget) & GTK_STATE_FLAG_INSENSITIVE))
    {
      if ((gtk_widget_get_state_flags (widget) & GTK_STATE_FLAG_INSENSITIVE) != 0)
        set_insensitive (scrollbar);
      else
        set_sensitive (scrollbar);
    }
}
#else
static void
hijacked_scrollbar_size_request (GtkWidget      *widget,
                                 GtkRequisition *requisition)
{
  if (use_overlay_scrollbar ())
    {
      OsScrollbarPrivate *priv;

      priv = get_private (widget);

      if (priv->orientation == GTK_ORIENTATION_VERTICAL)
        requisition->width = 0;
      else
        requisition->height = 0;

      widget->requisition = *requisition;

      return;
    }

  (* pre_hijacked_scrollbar_size_request) (widget, requisition);
}

static void
hijacked_scrollbar_state_changed (GtkWidget    *widget,
                                  GtkStateType  state)
{
  if (use_overlay_scrollbar ())
    {
      GtkScrollbar *scrollbar;

      scrollbar = GTK_SCROLLBAR (widget);

      if (gtk_widget_get_state (widget) == GTK_STATE_INSENSITIVE)
        set_insensitive (scrollbar);
      else
        set_sensitive (scrollbar);

      return;
    }

  (* pre_hijacked_scrollbar_state_changed) (widget, state);
}
#endif

static void
hijacked_scrollbar_unmap (GtkWidget *widget)
{
  if (use_overlay_scrollbar ())
    {
      GtkScrollbar *scrollbar;
      OsScrollbarPrivate *priv;

      scrollbar = GTK_SCROLLBAR (widget);
      priv = get_private (widget);

      (* widget_class_unmap) (widget);

      os_bar_hide (priv->bar);

      gtk_widget_hide (priv->thumb);

      priv->filter.proximity = FALSE;
      remove_window_filter (scrollbar);

      return;
    }

  (* pre_hijacked_scrollbar_unmap) (widget);
}

static void
hijacked_scrollbar_unrealize (GtkWidget *widget)
{
  scrollbar_list = g_slist_remove (scrollbar_list, widget);

  if (use_overlay_scrollbar ())
    {
      GtkScrollbar *scrollbar;
      OsScrollbarPrivate *priv;

      scrollbar = GTK_SCROLLBAR (widget);
      priv = get_private (widget);

      os_bar_hide (priv->bar);

      /* There could be a race where the window is unrealized while
       * the pointer just reached the proximity area and started the timeout,
       * protect against it. */
      if (priv->source_show_thumb_id != 0)
        {
          g_source_remove (priv->source_show_thumb_id);
          priv->source_show_thumb_id = 0;
        }

      gtk_widget_hide (priv->thumb);

      priv->filter.running = FALSE;
      gdk_window_remove_filter (gtk_widget_get_window (widget), window_filter_func, scrollbar);

      g_signal_handlers_disconnect_by_func (G_OBJECT (gtk_widget_get_toplevel (widget)),
                                            G_CALLBACK (toplevel_configure_event_cb), scrollbar);

      os_bar_set_parent (priv->bar, NULL);

      (* widget_class_unrealize) (widget);

      return;
    }

  (* pre_hijacked_scrollbar_unrealize) (widget);
}

/* Check if the application is blacklisted. */
static gboolean
app_is_blacklisted (void)
{
  /* Black-list of program names retrieved with g_get_prgname (). */
  static const gchar *blacklist[] = {
    "acroread", /* https://bugs.launchpad.net/ayatana-scrollbar/+bug/876218 */
    "eclipse", /* https://bugs.launchpad.net/ayatana-scrollbar/+bug/769277 */
    "gnucash", /* https://bugs.launchpad.net/ayatana-scrollbar/+bug/770304 */
    "gvim", /* https://bugs.launchpad.net/ayatana-scrollbar/+bug/847943 */
    "lnotes", /* https://bugs.launchpad.net/ayatana-scrollbar/+bug/890986 */
    "soffice", /* https://bugs.launchpad.net/ayatana-scrollbar/+bug/847918 */
    "synaptic", /* https://bugs.launchpad.net/ayatana-scrollbar/+bug/755238 */
    "vinagre", /* https://bugs.launchpad.net/ayatana-scrollbar/+bug/847932 */
    "vmplayer", /* https://bugs.launchpad.net/ayatana-scrollbar/+bug/770625 */
    "vmware"/* https://bugs.launchpad.net/ayatana-scrollbar/+bug/770625 */
  };
  static const gchar *blacklist_prefix[] = {
    "emacs", /* https://bugs.launchpad.net/ayatana-scrollbar/+bug/847940 */
    "firefox", /* https://bugs.launchpad.net/ayatana-scrollbar/+bug/847922 */
    "gimp", /* https://bugs.launchpad.net/ayatana-scrollbar/+bug/803163 */
    "notes", /* https://bugs.launchpad.net/ayatana-scrollbar/+bug/890986 */
    "thunderbird", /* https://bugs.launchpad.net/ayatana-scrollbar/+bug/847929 */
  };

  GModule *module;
  gpointer func;
  gint32 i;
  const gint32 nr_programs = G_N_ELEMENTS (blacklist);
  const gint32 nr_programs_prefix = G_N_ELEMENTS (blacklist_prefix);
  const gchar *prgname;
  const gchar *flag;

  prgname = g_get_prgname ();
  flag = g_getenv ("LIBOVERLAY_SCROLLBAR");

  /* Check LIBOVERLAY_SCROLLBAR. */
  if (flag != NULL)
    {
      /* Blacklist if is set to 0. */
      if (*flag == '\0' || *flag == '0')
        return TRUE;

      /* Special mode to override the blacklist. */
      if (g_strcmp0 (flag, "override-blacklist") == 0)
        return FALSE;
    }

  /* Black-list of symbols. */
  module = g_module_open (NULL, 0);
  /* https://bugs.launchpad.net/ayatana-scrollbar/+bug/847966 */
  if (g_module_symbol (module, "qt_startup_hook", &func))
    {
      g_module_close (module);
      return TRUE;
    }
  g_module_close (module);

  /* Black-list of program names. */
  for (i = 0; i < nr_programs; i++)
    if (g_strcmp0 (blacklist[i], prgname) == 0)
      return TRUE;
  for (i = 0; i < nr_programs_prefix; i++)
    if (g_str_has_prefix (prgname, blacklist_prefix[i]))
      return TRUE;

  return FALSE;
}

/* Patch GtkScrollbar vtable. */
static void
patch_scrollbar_class_vtable (GType type)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GType *children;
  guint n, i;

  /* Patch vtable, assigning new pointers to vtable functions. */
  object_class = g_type_class_ref (type);
  widget_class = g_type_class_ref (type);

  if (object_class->dispose == pre_hijacked_scrollbar_dispose)
    object_class->dispose = hijacked_scrollbar_dispose;

#ifdef USE_GTK3
  if (widget_class->draw == pre_hijacked_scrollbar_draw)
    widget_class->draw = hijacked_scrollbar_draw;
  if (widget_class->get_preferred_width == pre_hijacked_scrollbar_get_preferred_width)
    widget_class->get_preferred_width = hijacked_scrollbar_get_preferred_width;
  if (widget_class->get_preferred_height == pre_hijacked_scrollbar_get_preferred_height)
    widget_class->get_preferred_height = hijacked_scrollbar_get_preferred_height;
  if (widget_class->state_flags_changed == pre_hijacked_scrollbar_state_flags_changed)
    widget_class->state_flags_changed = hijacked_scrollbar_state_flags_changed;
#else
  if (widget_class->expose_event == pre_hijacked_scrollbar_expose_event)
    widget_class->expose_event = hijacked_scrollbar_expose_event;
  if (widget_class->size_request == pre_hijacked_scrollbar_size_request)
    widget_class->size_request = hijacked_scrollbar_size_request;
  if (widget_class->state_changed == pre_hijacked_scrollbar_state_changed)
    widget_class->state_changed = hijacked_scrollbar_state_changed;
#endif
  if (widget_class->grab_notify == pre_hijacked_scrollbar_grab_notify)
    widget_class->grab_notify = hijacked_scrollbar_grab_notify;
  if (widget_class->hide == pre_hijacked_scrollbar_hide)
    widget_class->hide = hijacked_scrollbar_hide;
  if (widget_class->map == pre_hijacked_scrollbar_map)
    widget_class->map = hijacked_scrollbar_map;
  if (widget_class->realize == pre_hijacked_scrollbar_realize)
    widget_class->realize = hijacked_scrollbar_realize;
  if (widget_class->show == pre_hijacked_scrollbar_show)
    widget_class->show = hijacked_scrollbar_show;
  if (widget_class->size_allocate == pre_hijacked_scrollbar_size_allocate)
    widget_class->size_allocate = hijacked_scrollbar_size_allocate;
  if (widget_class->unmap == pre_hijacked_scrollbar_unmap)
    widget_class->unmap = hijacked_scrollbar_unmap;
  if (widget_class->unrealize == pre_hijacked_scrollbar_unrealize)
    widget_class->unrealize = hijacked_scrollbar_unrealize;

  /* Recurse GType children. */
  children = g_type_children (type, &n);
  for (i = 0; i < n; i++)
    patch_scrollbar_class_vtable (children[i]);
  g_free (children);
}

/* Load custom style for overlay scrollbar. */
static void
custom_style_load (void)
{
#ifdef USE_GTK3
  gtk_style_context_add_provider_for_screen (gdk_display_get_default_screen (gdk_display_get_default ()),
                                             GTK_STYLE_PROVIDER (provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
#else
  gtk_rc_parse_string ("style \"overlay-scrollbar\" {\n"
                       "    GtkScrolledWindow::scrollbar-spacing = 0\n"
                       "    GtkScrolledWindow::scrollbars-within-bevel = 1\n"
                       " }\n"
                       "\n"
                       "class \"GtkScrolledWindow\" style \"overlay-scrollbar\"");
#endif
}

#ifdef USE_GTK3
/* Unload custom style for overlay scrollbar. */
static void
custom_style_unload (void)
{
  gtk_style_context_remove_provider_for_screen (gdk_display_get_default_screen (gdk_display_get_default ()),
                                                GTK_STYLE_PROVIDER (provider));
}
#endif

/* Unload all scrollbars. */
static void
scrollbar_mode_changed_unload_gfunc (gpointer data,
                                     gpointer user_data)
{
  GtkWidget *widget;
  GSList **mapped_list;

  widget = GTK_WIDGET (data);
  mapped_list = user_data;

  /* The following unrealize will unmap the widget:
   * create a list of mapped scrollbars to remap them afterwards. */
  if (gtk_widget_get_mapped (widget))
    *mapped_list = g_slist_prepend (*mapped_list, widget);

  gtk_widget_unrealize (widget);
}

/* Load all scrollbars. */
static void
scrollbar_mode_changed_load_gfunc (gpointer data,
                                   gpointer user_data)
{
  gtk_widget_realize (GTK_WIDGET (data));
}

/* Complete load of all scrollbars. */
static void
scrollbar_mode_changed_load_end_gfunc (gpointer data,
                                       gpointer user_data)
{
  GtkWidget *widget;

  widget = GTK_WIDGET (data);

  /* Remap the list of scrollbars that were unmapped by the unload gfunc.
   * Request a resize to update widget allocation. */
  gtk_widget_map (widget);
  gtk_widget_queue_resize (widget);
}

/* Callback called when scrollbar-mode changes. */
static void
scrollbar_mode_changed_cb (GObject    *object,
                           GParamSpec *pspec,
                           gpointer    user_data)
{
  GSettings *settings;
  GSList *tmp_list, *mapped_list;

  settings = G_SETTINGS (object);
  tmp_list = g_slist_copy (scrollbar_list);

  /* Initialize the pointer by initializing its variable. */
  mapped_list = NULL;

  /* Unload all scrollbars, using previous scrollbar_mode. */
  g_slist_foreach (tmp_list, scrollbar_mode_changed_unload_gfunc, &mapped_list);

  /* Update the scrollbar_mode variable. */
  scrollbar_mode = g_settings_get_enum (settings, "scrollbar-mode");

#ifdef USE_GTK3
  /* Load or unload custom style for overlay scrollbar. */
  if (use_overlay_scrollbar ())
    custom_style_load ();
  else
    custom_style_unload ();
#else
  /* Gtk+ 2.0 doesn't support dynamic loading of styles.
   * Please contact me in case I'm wrong,
   * and I'll add the required bits here. */
#endif

  /* Load all scrollbars, using new scrollbar_mode. */
  g_slist_foreach (tmp_list, scrollbar_mode_changed_load_gfunc, NULL);

  /* Map the scrollbars that were unmapped by unload. */
  g_slist_foreach (mapped_list, scrollbar_mode_changed_load_end_gfunc, NULL);

  g_slist_free (mapped_list);
  g_slist_free (tmp_list);
}

/* Suppress the warning 'missing-declarations'. */
void gtk_module_init (void);

void
gtk_module_init (void)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GSettings *settings;

  /* Exit if the application is blacklisted. */
  if (app_is_blacklisted ())
    return;

  /* Initialize static variables. */
  net_active_window_atom = gdk_x11_get_xatom_by_name ("_NET_ACTIVE_WINDOW");
  unity_net_workarea_region_atom = gdk_x11_get_xatom_by_name ("_UNITY_NET_WORKAREA_REGION");
  os_quark_placement = g_quark_from_static_string ("os_quark_placement");
  os_quark_qdata = g_quark_from_static_string ("os-scrollbar");

  /* Store GtkScrollbar vfunc pointers. */
  object_class = g_type_class_ref (GTK_TYPE_SCROLLBAR);
  widget_class = g_type_class_ref (GTK_TYPE_SCROLLBAR);

  pre_hijacked_scrollbar_dispose  = object_class->dispose;

#ifdef USE_GTK3
  pre_hijacked_scrollbar_draw                 = widget_class->draw;
  pre_hijacked_scrollbar_get_preferred_width  = widget_class->get_preferred_width;
  pre_hijacked_scrollbar_get_preferred_height = widget_class->get_preferred_height;
  pre_hijacked_scrollbar_state_flags_changed  = widget_class->state_flags_changed;
#else
  pre_hijacked_scrollbar_expose_event         = widget_class->expose_event;
  pre_hijacked_scrollbar_size_request         = widget_class->size_request;
  pre_hijacked_scrollbar_state_changed        = widget_class->state_changed;
#endif
  pre_hijacked_scrollbar_grab_notify          = widget_class->grab_notify;
  pre_hijacked_scrollbar_hide                 = widget_class->hide;
  pre_hijacked_scrollbar_map                  = widget_class->map;
  pre_hijacked_scrollbar_realize              = widget_class->realize;
  pre_hijacked_scrollbar_show                 = widget_class->show;
  pre_hijacked_scrollbar_size_allocate        = widget_class->size_allocate;
  pre_hijacked_scrollbar_unmap                = widget_class->unmap;
  pre_hijacked_scrollbar_unrealize            = widget_class->unrealize;

  /* Store GtkWidget vfunc pointers. */
  widget_class = g_type_class_ref (GTK_TYPE_WIDGET);

  widget_class_hide      = widget_class->hide;
  widget_class_map       = widget_class->map;
  widget_class_realize   = widget_class->realize;
  widget_class_show      = widget_class->show;
  widget_class_unmap     = widget_class->unmap;
  widget_class_unrealize = widget_class->unrealize;

  /* Patch GtkScrollbar vtable. */
  patch_scrollbar_class_vtable (GTK_TYPE_SCROLLBAR);

  /* Connect to gsettings. */
  settings = g_settings_new ("com.canonical.desktop.interface");
  g_signal_connect (settings, "changed::scrollbar-mode",
                    G_CALLBACK (scrollbar_mode_changed_cb), NULL);
  scrollbar_mode = g_settings_get_enum (settings, "scrollbar-mode");

#ifdef USE_GTK3
  /* Initialize styling bits. */
  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (GTK_CSS_PROVIDER (provider),
                                   "* {\n"
                                   "    -GtkScrolledWindow-scrollbar-spacing: 0;\n"
                                   "    -GtkScrolledWindow-scrollbars-within-bevel: 1;\n"
                                   "}\n", -1, NULL);
#endif

#ifndef USE_GTK3
  ubuntu_gtk_set_use_overlay_scrollbar (TRUE);
#endif

  /* Load custom overlay scrollbar style. */
  if (use_overlay_scrollbar ())
    custom_style_load ();
}
