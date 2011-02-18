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

#include <cairo-xlib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/X.h>
#include <X11/Xlib.h>

#include "overlay-scrollbar.h"
#include "overlay-scrollbar-cairo-support.h"
#include "overlay-scrollbar-support.h"
#include "overlay-pager.h"
#include "overlay-thumb.h"

#define OVERLAY_SCROLLBAR_WIDTH 15 /* width/height of the overlay scrollbar, in pixels */
#define OVERLAY_SCROLLBAR_HEIGHT 80 /* height/width of the overlay scrollbar, in pixels */
#define PROXIMITY_WIDTH 40 /* width/height of the proximity effect, in pixels */
#define TIMEOUT_HIDE 1000 /* timeout before hiding, in milliseconds */

enum {
  PROP_0,

  PROP_RANGE,

  LAST_ARG
};

G_DEFINE_TYPE (OverlayScrollbar, overlay_scrollbar, GTK_TYPE_WINDOW);

#define OVERLAY_SCROLLBAR_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), OS_TYPE_OVERLAY_SCROLLBAR, OverlayScrollbarPrivate))

typedef struct _OverlayScrollbarPrivate OverlayScrollbarPrivate;

struct _OverlayScrollbarPrivate
{
  GdkRectangle trough;
  GdkRectangle overlay;
  GdkRectangle slider;

  OverlayPager *pager;
  GtkWidget *thumb;

  GtkAllocation range_all;
  GtkOrientation orientation;
  GtkWidget *range;
  GdkWindow *overlay_window;

  gboolean button_press_event;
  gboolean enter_notify_event;
  gboolean motion_notify_event;
  gboolean value_changed_event;
  gboolean toplevel_connected;

  gboolean can_hide;
  gboolean can_rgba;

  gint win_x;
  gint win_y;

  gint slide_initial_slider_position;
  gint slide_initial_coordinate;

  gint pointer_x;
  gint pointer_y;
};

/* SUBCLASS FUNCTIONS */
static void overlay_scrollbar_get_property (GObject    *object,
                                            guint       prop_id,
                                            GValue     *value,
                                            GParamSpec *pspec);

static void overlay_scrollbar_map (GtkWidget *widget);

static void overlay_scrollbar_set_property (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec);

/*static void overlay_scrollbar_hide (GtkWidget *widget);*/

static void overlay_scrollbar_show (GtkWidget *widget);

/* THUMB FUNCTIONS */
static gboolean overlay_thumb_button_press_event_cb (GtkWidget      *widget,
                                                     GdkEventButton *event,
                                                     gpointer        user_data);

static gboolean overlay_thumb_button_release_event_cb (GtkWidget      *widget,
                                                       GdkEventButton *event,
                                                       gpointer        user_data);

static gboolean overlay_thumb_enter_notify_event_cb (GtkWidget        *widget,
                                                     GdkEventCrossing *event,
                                                     gpointer          user_data);

static gboolean overlay_thumb_leave_notify_event_cb (GtkWidget        *widget,
                                                     GdkEventCrossing *event,
                                                     gpointer          user_data);

static gboolean overlay_thumb_motion_notify_event_cb (GtkWidget      *widget,
                                                      GdkEventMotion *event,
                                                      gpointer        user_data);

/* HELPER FUNCTIONS */
static void overlay_scrollbar_calc_layout_range (OverlayScrollbar *scrollbar,
                                                 gdouble           adjustment_value);

static gdouble overlay_scrollbar_coord_to_value (OverlayScrollbar *scrollbar,
                                                 gint              coord);

static gboolean overlay_scrollbar_hide (gpointer user_data);

static void overlay_scrollbar_move (OverlayScrollbar *scrollbar,
                                    gint              mouse_x,
                                    gint              mouse_y);

static void overlay_scrollbar_store_window_position (OverlayScrollbar *scrollbar);

/* OVERLAY FUNCTIONS */
static void overlay_create_window (OverlayScrollbar *scrollbar);

static void overlay_draw_bitmap (GdkBitmap *bitmap);

static void overlay_draw_pixmap (GdkPixmap *pixmap);

static void overlay_move (OverlayScrollbar *scrollbar);

static void overlay_resize_window (OverlayScrollbar *scrollbar);

/* RANGE FUNCTIONS */
static gboolean range_expose_event_cb (GtkWidget      *widget,
                                       GdkEventExpose *event,
                                       gpointer        user_data);

static void range_size_allocate_cb (GtkWidget     *widget,
                                    GtkAllocation *allocation,
                                    gpointer       user_data);

static void range_value_changed_cb (GtkWidget      *widget,
                                    gpointer        user_data);

/* TOPLEVEL FUNCTIONS */
static gboolean toplevel_configure_event_cb (GtkWidget         *widget,
                                             GdkEventConfigure *event,
                                             gpointer           user_data);

static GdkFilterReturn toplevel_filter_func (GdkXEvent *gdkxevent,
                                             GdkEvent  *event,
                                             gpointer   user_data);

static gboolean toplevel_leave_notify_event_cb (GtkWidget        *widget,
                                                GdkEventCrossing *event,
                                                gpointer          user_data);

/* SUBCLASS FUNCTIONS */
/**
 * overlay_scrollbar_button_press_event:
 * override class function
 **/


/**
 * overlay_scrollbar_class_init:
 * class init function
 **/
static void
overlay_scrollbar_class_init (OverlayScrollbarClass *class)
{
  DEBUG
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS (class);
  widget_class = GTK_WIDGET_CLASS (class);

/*  widget_class->button_press_event   = overlay_scrollbar_button_press_event;*/
/*  widget_class->button_release_event = overlay_scrollbar_button_release_event;*/
/*  widget_class->composited_changed   = overlay_scrollbar_composited_changed;*/
/*  widget_class->enter_notify_event   = overlay_scrollbar_enter_notify_event;*/
/*  widget_class->expose_event         = overlay_scrollbar_expose;*/
/*  widget_class->leave_notify_event   = overlay_scrollbar_leave_notify_event;*/
/*  widget_class->map                  = overlay_scrollbar_map;*/
/*  widget_class->hide = overlay_scrollbar_hide;*/
  widget_class->show = overlay_scrollbar_show;
/*  widget_class->motion_notify_event  = overlay_scrollbar_motion_notify_event;*/
/*  widget_class->screen_changed       = overlay_scrollbar_screen_changed;*/

  gobject_class->get_property = overlay_scrollbar_get_property;
  gobject_class->set_property = overlay_scrollbar_set_property;

  g_type_class_add_private (gobject_class, sizeof (OverlayScrollbarPrivate));

  g_object_class_install_property (gobject_class,
                                   PROP_RANGE,
                                   g_param_spec_object ("range",
                                                        "Range",
                                                        "Attach a GtkRange",
                                                        GTK_TYPE_RANGE,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));
}

/**
 * overlay_scrollbar_composited_changed:
 * override class function
 **/
static void
overlay_scrollbar_composited_changed (GtkWidget *widget)
{
  DEBUG
}

/**
 * overlay_scrollbar_enter_notify_event:
 * override class function
 **/
static gboolean
overlay_thumb_enter_notify_event_cb (GtkWidget        *widget,
                                     GdkEventCrossing *event,
                                     gpointer          user_data)
{
  DEBUG
  OverlayScrollbar *scrollbar;
  OverlayScrollbarPrivate *priv;

  scrollbar = OVERLAY_SCROLLBAR (user_data);
  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (OVERLAY_SCROLLBAR (scrollbar));

  priv->enter_notify_event = TRUE;
  priv->can_hide = FALSE;

  return TRUE;
}

/**
 * overlay_scrollbar_get_property:
 * override class function
 **/
static void
overlay_scrollbar_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  DEBUG
  OverlayScrollbar *scrollbar;
  OverlayScrollbarPrivate *priv;

  scrollbar = OVERLAY_SCROLLBAR (object);

  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (scrollbar);

  switch (prop_id)
    {
      case PROP_RANGE:
        g_value_set_object (value, priv->range);
        break;
    }
}

/**
 * overlay_scrollbar_init:
 * init function
 **/
static void
overlay_scrollbar_init (OverlayScrollbar *scrollbar)
{
  DEBUG
  OverlayScrollbarPrivate *priv;

  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (scrollbar);

  priv->orientation = GTK_ORIENTATION_VERTICAL;
  priv->can_hide = TRUE;
  priv->can_rgba = FALSE;

  priv->thumb = overlay_thumb_new (priv->orientation);

  g_signal_connect (G_OBJECT (priv->thumb), "button-press-event",
                    G_CALLBACK (overlay_thumb_button_press_event_cb), scrollbar);
  g_signal_connect (G_OBJECT (priv->thumb), "button-release-event",
                    G_CALLBACK (overlay_thumb_button_release_event_cb), scrollbar);
  g_signal_connect (G_OBJECT (priv->thumb), "motion-notify-event",
                    G_CALLBACK (overlay_thumb_motion_notify_event_cb), scrollbar);
  g_signal_connect (G_OBJECT (priv->thumb), "enter-notify-event",
                    G_CALLBACK (overlay_thumb_enter_notify_event_cb), scrollbar);
  g_signal_connect (G_OBJECT (priv->thumb), "leave-notify-event",
                    G_CALLBACK (overlay_thumb_leave_notify_event_cb), scrollbar);
}

/**
 * overlay_scrollbar_leave_notify_event:
 * override class function
 **/
static gboolean
overlay_thumb_leave_notify_event_cb (GtkWidget        *widget,
                                     GdkEventCrossing *event,
                                     gpointer          user_data)
{
  DEBUG
  OverlayScrollbar *scrollbar;
  OverlayScrollbarPrivate *priv;

  scrollbar = OVERLAY_SCROLLBAR (user_data);
  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (OVERLAY_SCROLLBAR (scrollbar));

  if (!priv->button_press_event)
    priv->can_hide = TRUE;

/*  g_timeout_add (TIMEOUT_HIDE, overlay_scrollbar_hide, widget);*/

  return TRUE;
}

/**
 * overlay_scrollbar_map:
 * override class function
 **/
static void
overlay_scrollbar_map (GtkWidget *widget)
{
  DEBUG

  GTK_WIDGET_CLASS (overlay_scrollbar_parent_class)->map (widget);

  Display *display;
  GtkWidget *parent;
  OverlayScrollbarPrivate *priv;
  XWindowChanges changes;
  guint32 xid, xid_parent;
  unsigned int value_mask = CWSibling | CWStackMode;
  int res;

  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (OVERLAY_SCROLLBAR (widget));

  parent = priv->range;

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
}

/*static void*/
/*overlay_scrollbar_hide (GtkWidget *widget)*/
/*{*/
/*  DEBUG*/
/*  OverlayScrollbarPrivate *priv;*/

/*  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (OVERLAY_SCROLLBAR (widget));*/
/*  gtk_widget_hide (GTK_WIDGET (priv->thumb));*/
/*}*/


static void
overlay_scrollbar_show (GtkWidget *widget)
{
  DEBUG
  OverlayScrollbarPrivate *priv;

  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (OVERLAY_SCROLLBAR (widget));
  gtk_widget_show (GTK_WIDGET (priv->thumb));
}

/**
 * overlay_scrollbar_motion_notify_event:
 * override class function
 **/
static gboolean
overlay_thumb_motion_notify_event_cb (GtkWidget      *widget,
                                      GdkEventMotion *event,
                                      gpointer        user_data)
{
  DEBUG
  OverlayScrollbar *scrollbar;
  OverlayScrollbarPrivate *priv;

  scrollbar = OVERLAY_SCROLLBAR (user_data);
  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (OVERLAY_SCROLLBAR (scrollbar));

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

          overlay_scrollbar_move (scrollbar, event->x_root, event->y_root);
          priv->value_changed_event = FALSE;
        }

      if (!priv->motion_notify_event)
        gtk_widget_queue_draw (widget);

      priv->motion_notify_event = TRUE;

      overlay_scrollbar_move (scrollbar, event->x_root, event->y_root);

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
          x = CLAMP (event->x_root - priv->pointer_x,
                     priv->win_x,
                     priv->win_x + priv->trough.width - priv->slider.width);;
          y = priv->win_y;
        }

      gtk_window_move (GTK_WINDOW (widget), x - 4, y);
    }

  return TRUE;
}

/**
 * overlay_scrollbar_set_property:
 * override class function
 **/
static void
overlay_scrollbar_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  DEBUG
  OverlayScrollbar *scrollbar;
  OverlayScrollbarPrivate *priv;

  scrollbar = OVERLAY_SCROLLBAR (object);

  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (scrollbar);

  switch (prop_id)
    {
      case PROP_RANGE:
        {
          priv->range = g_value_get_object (value);
          gtk_widget_set_size_request (priv->range, 0, -1);
          gtk_widget_hide (priv->range);
          g_signal_connect (G_OBJECT (priv->range), "size-allocate",
                            G_CALLBACK (range_size_allocate_cb), scrollbar);
          g_signal_connect (G_OBJECT (priv->range), "expose-event",
                            G_CALLBACK (range_expose_event_cb), scrollbar);
          g_signal_connect_after (G_OBJECT (priv->range), "value-changed",
                            G_CALLBACK (range_value_changed_cb), scrollbar);
          break;
        }
    }
}

/* PUBLIC FUNCTIONS*/
/**
 * overlay_scrollbar_new:
 * @widget: a pointer to the GtkRange to connect
 *
 * Creates a new overlay scrollbar.
 *
 * Returns: the new overlay scrollbar as a #GtkWidget
 */
GtkWidget*
overlay_scrollbar_new (GtkWidget *widget)
{
  DEBUG
  return g_object_new (OS_TYPE_OVERLAY_SCROLLBAR , "range", widget, NULL);
}

/**
 * overlay_scrollbar_set_range:
 * @widget: an OverlayScrollbar widget
 * @range: a pointer to a GtkRange
 *
 * Sets the GtkRange to control trough the OverlayScrollbar.
 *
 */
void
overlay_scrollbar_set_range (GtkWidget *widget,
                             GtkWidget *range)
{
  DEBUG
  OverlayScrollbarPrivate *priv;

  g_return_if_fail (OS_IS_OVERLAY_SCROLLBAR (widget) && GTK_RANGE (range));

  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (widget);

  priv->range = range;
}

/* HELPER FUNCTIONS */
/**
 * overlay_scrollbar_calc_layout_range:
 * calculate GtkRange layout and store info
 **/
static void
overlay_scrollbar_calc_layout_range (OverlayScrollbar *scrollbar,
                                     gdouble           adjustment_value)
{
  DEBUG
  GdkRectangle range_rect;
  GtkBorder border;
  GtkRange *range;
  OverlayScrollbarPrivate *priv;
  gint overlay_width, trough_border;
  gint overlay_length;

  gint tmp_height;

  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (scrollbar);
  range = GTK_RANGE (priv->range);

  tmp_height = priv->overlay.height;

  os_gtk_range_get_props (range,
                          &overlay_width, &trough_border,
                          NULL, NULL);

  os_gtk_range_calc_request (range,
                             overlay_width, trough_border,
                             &range_rect, &border, &overlay_length);

  /* We never expand to fill available space in the small dimension
   * (i.e. vertical scrollbars are always a fixed width)
   */
  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      os_clamp_dimensions (GTK_WIDGET (range), &range_rect, &border, TRUE);
    }
  else
    {
      os_clamp_dimensions (GTK_WIDGET (range), &range_rect, &border, FALSE);
    }

  range_rect.x = border.left;
  range_rect.y = border.top;

  priv->trough.x = range_rect.x;
  priv->trough.y = range_rect.y;
  priv->trough.width = range_rect.width;
  priv->trough.height = range_rect.height;

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      gint y, bottom, top, height;

      top = priv->trough.y;
      bottom = priv->trough.y + priv->trough.height;

      /* overlay height is the fraction (page_size /
       * total_adjustment_range) times the trough height in pixels
       */

      if (range->adjustment->upper - range->adjustment->lower != 0)
        height = ((bottom - top) * (range->adjustment->page_size /
                                   (range->adjustment->upper - range->adjustment->lower)));
      else
        height = range->min_slider_size;

      if (height < range->min_slider_size ||
          range->slider_size_fixed)
        height = range->min_slider_size;

      height = MIN (height, priv->trough.height);

      y = top;

      if (range->adjustment->upper - range->adjustment->lower - range->adjustment->page_size != 0)
        y += (bottom - top - height) * ((adjustment_value - range->adjustment->lower) /
                                        (range->adjustment->upper - range->adjustment->lower - range->adjustment->page_size));

      y = CLAMP (y, top, bottom);

      priv->overlay.y = y;
      priv->overlay.height = height;

      if (tmp_height != height);
        {
          if (priv->pager != NULL)
          {
/*          overlay_resize_window (scrollbar);*/
            overlay_move (scrollbar);
          }
        }
    }
  else
    {
      gint x, left, right, width;

      left = priv->trough.x;
      right = priv->trough.x + priv->trough.width;

      /* overlay width is the fraction (page_size /
       * total_adjustment_range) times the trough width in pixels
       */

      if (range->adjustment->upper - range->adjustment->lower != 0)
        width = ((right - left) * (range->adjustment->page_size /
                                  (range->adjustment->upper - range->adjustment->lower)));
      else
        width = range->min_slider_size;

      if (width < range->min_slider_size ||
          range->slider_size_fixed)
        width = range->min_slider_size;

      width = MIN (width, priv->trough.width);

      x = left;

      if (range->adjustment->upper - range->adjustment->lower - range->adjustment->page_size != 0)
        x += (right - left - width) * ((adjustment_value - range->adjustment->lower) /
                                       (range->adjustment->upper - range->adjustment->lower - range->adjustment->page_size));

      x = CLAMP (x, left, right);

      priv->overlay.x = x;
      priv->overlay.width = width;
    }
}

/**
 * overlay_scrollbar_calc_layout_slider:
 * calculate OverlayScrollbar layout and store info
 **/
static void
overlay_scrollbar_calc_layout_slider (OverlayScrollbar *scrollbar,
                                      gdouble           adjustment_value)
{
  DEBUG
  GtkRange *range;
  OverlayScrollbarPrivate *priv;

  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (scrollbar);
  range = GTK_RANGE (priv->range);

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      gint y, bottom, top, height;

      top = priv->trough.y;
      bottom = priv->trough.y + priv->trough.height;

      height = priv->slider.height;

      height = MIN (height, priv->trough.height);

      y = top;

      if (range->adjustment->upper - range->adjustment->lower - range->adjustment->page_size != 0)
        y += (bottom - top - height) * ((adjustment_value - range->adjustment->lower) /
                                        (range->adjustment->upper - range->adjustment->lower - range->adjustment->page_size));

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

      if (range->adjustment->upper - range->adjustment->lower - range->adjustment->page_size != 0)
        x += (right - left - width) * ((adjustment_value - range->adjustment->lower) /
                                       (range->adjustment->upper - range->adjustment->lower - range->adjustment->page_size));

      x = CLAMP (x, left, right);

      priv->slider.x = x;
      priv->slider.width = width;
    }
}

/**
 * overlay_scrollbar_coord_to_value:
 * traduce pixels into proper GtkRange values
 **/
static gdouble
overlay_scrollbar_coord_to_value (OverlayScrollbar *scrollbar,
                                  gint              coord)
{
  DEBUG
  GtkRange *range;
  OverlayScrollbarPrivate *priv;
  gdouble frac;
  gdouble value;
  gint    trough_length;
  gint    trough_start;
  gint    slider_length;

  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (scrollbar);
  range = GTK_RANGE (priv->range);

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

  value = range->adjustment->lower + frac * (range->adjustment->upper -
                                             range->adjustment->lower -
                                             range->adjustment->page_size);

  return value;
}

/**
 * overlay_scrollbar_hide:
 * hide if it's ok to hide
 **/
static gboolean
overlay_scrollbar_hide (gpointer user_data)
{
  DEBUG
  OverlayScrollbar *scrollbar;
  OverlayScrollbarPrivate *priv;

  scrollbar = user_data;
  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (scrollbar);

  if (priv->can_hide)
  {
    priv->value_changed_event = FALSE;
    gtk_widget_hide (GTK_WIDGET (priv->thumb));
  }

  return FALSE;
}

/**
 * overlay_scrollbar_move:
 * move the scrollbar
 **/
static void
overlay_scrollbar_move (OverlayScrollbar *scrollbar,
                        gint              mouse_x,
                        gint              mouse_y)
{
  DEBUG
  GtkRange *range;
  OverlayScrollbarPrivate *priv;
  gint delta;
  gint c;
  gdouble new_value;
  gboolean handled;

  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (scrollbar);
  range = GTK_RANGE (priv->range);

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    delta = mouse_y - priv->slide_initial_coordinate;
  else
    delta = mouse_x - priv->slide_initial_coordinate;

  c = priv->slide_initial_slider_position + delta;

  new_value = overlay_scrollbar_coord_to_value (scrollbar, c);

  g_signal_emit_by_name (range, "change-value",
                         GTK_SCROLL_JUMP, new_value, &handled, NULL);
}

/**
 * overlay_scrollbar_store_window_position:
 * store scrollbar window position
 **/
static void
overlay_scrollbar_store_window_position (OverlayScrollbar *scrollbar)
{
  DEBUG
  OverlayScrollbarPrivate *priv;
  gint win_x, win_y;

  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (scrollbar);

  gdk_window_get_position (gtk_widget_get_window (priv->range), &win_x, &win_y);

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      priv->win_x = win_x + priv->range_all.x;
      priv->win_y = win_y + priv->range_all.y;
    }
  else
    {
      priv->win_x = win_x + priv->range_all.x;
      priv->win_y = win_y + priv->range_all.y;
    }
}

/* THUMB FUNCTIONS */
/**
 * overlay_thumb_button_release_event_cb:
 * connect to button-press-event
 **/
static gboolean
overlay_thumb_button_press_event_cb (GtkWidget      *widget,
                                     GdkEventButton *event,
                                     gpointer        user_data)
{
  DEBUG
  if (event->type == GDK_BUTTON_PRESS)
    {
      if (event->button == 1)
        {
          OverlayScrollbar *scrollbar;
          OverlayScrollbarPrivate *priv;

          scrollbar = OVERLAY_SCROLLBAR (user_data);
          priv = OVERLAY_SCROLLBAR_GET_PRIVATE (OVERLAY_SCROLLBAR (scrollbar));

/*          overlay_scrollbar_map (widget);*/
          gtk_window_set_transient_for (GTK_WINDOW (widget), GTK_WINDOW (gtk_widget_get_toplevel (priv->range)));
          os_present_gdk_window_with_timestamp (priv->range, event->time);

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

/**
 * overlay_thumb_button_release_event_cb:
 * connect to button-release-event
 **/
static gboolean
overlay_thumb_button_release_event_cb (GtkWidget      *widget,
                                       GdkEventButton *event,
                                       gpointer        user_data)
{
  DEBUG
  if (event->type == GDK_BUTTON_RELEASE)
    {
      if (event->button == 1)
        {
          OverlayScrollbar *scrollbar;
          OverlayScrollbarPrivate *priv;

          scrollbar = OVERLAY_SCROLLBAR (user_data);
          priv = OVERLAY_SCROLLBAR_GET_PRIVATE (OVERLAY_SCROLLBAR (scrollbar));

          gtk_window_set_transient_for (GTK_WINDOW (widget), NULL);

          if (!priv->motion_notify_event)
            {
              GtkAllocation allocation;

              gtk_widget_get_allocation (widget, &allocation);

              if (priv->orientation == GTK_ORIENTATION_VERTICAL)
                {
                  if (priv->pointer_y < allocation.height / 2)
                    g_signal_emit_by_name (priv->range, "move-slider", GTK_SCROLL_PAGE_UP);
                  else
                    g_signal_emit_by_name (priv->range, "move-slider", GTK_SCROLL_PAGE_DOWN);
                }
              else
                {
                  if (priv->pointer_x < allocation.width / 2)
                    g_signal_emit_by_name (priv->range, "move-slider", GTK_SCROLL_PAGE_UP);
                  else
                    g_signal_emit_by_name (priv->range, "move-slider", GTK_SCROLL_PAGE_DOWN);
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

/* OVERLAY FUNCTIONS */
/**
 * overlay_create_window:
 * create the overlay window
 **/
static void
overlay_create_window (OverlayScrollbar *scrollbar)
{
  DEBUG
  
  OverlayScrollbarPrivate *priv;

  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (scrollbar);

  priv->pager = overlay_pager_new (priv->range);
  overlay_resize_window (scrollbar);
  overlay_move (scrollbar);

  overlay_pager_show (priv->pager);
}


/**
 * overlay_move:
 * move the overlay_window to the right position
 **/
static void
overlay_move (OverlayScrollbar *scrollbar)
{
  DEBUG
  OverlayScrollbarPrivate *priv;

  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (scrollbar);

  GdkRectangle mask;
  mask.x = 0;
  mask.y = priv->overlay.y;
  mask.width = 3;
  mask.height = priv->overlay.height;

/*  printf ("move: %i\n", priv->pager);*/
/*   XXX missing horizontal and - 8 is hardcoded */
  overlay_pager_move_resize (priv->pager, mask);
}

/**
 * overlay_resize_window:
 * resize the overlay window
 **/
static void
overlay_resize_window (OverlayScrollbar *scrollbar)
{
  DEBUG
  OverlayScrollbarPrivate *priv;

  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (scrollbar);

  GtkAllocation allocation;
  gtk_widget_get_allocation (gtk_widget_get_parent (priv->range), &allocation);
  GdkRectangle rect;
  rect.x = allocation.x + allocation.width - 8;
  rect.y = allocation.y + 1;
  rect.width = 3;
  rect.height = allocation.height - 2;

  overlay_pager_size_allocate (priv->pager, rect);
}

/* RANGE FUNCTIONS */
/**
 * range_expose_event_cb:
 * react to "expose-event", to connect other callbacks and useful things
 **/
static gboolean
range_expose_event_cb (GtkWidget      *widget,
                       GdkEventExpose *event,
                       gpointer        user_data)
{
  DEBUG
  OverlayScrollbar *scrollbar;
  OverlayScrollbarPrivate *priv;

  gint x_pos, y_pos;
  scrollbar = OVERLAY_SCROLLBAR (user_data);
  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (scrollbar);

  if (!priv->toplevel_connected)
    {
      gdk_window_add_filter (gtk_widget_get_window (widget), toplevel_filter_func, scrollbar);
/*      g_signal_connect (G_OBJECT (gtk_widget_get_toplevel (widget)), "property-notify-event",*/
/*                        G_CALLBACK (toplevel_property_notify_event_cb), scrollbar);*/
      g_signal_connect (G_OBJECT (gtk_widget_get_toplevel (widget)), "configure-event",
                        G_CALLBACK (toplevel_configure_event_cb), scrollbar);
      g_signal_connect (G_OBJECT (gtk_widget_get_toplevel (widget)), "leave-notify-event",
                        G_CALLBACK (toplevel_leave_notify_event_cb), scrollbar);
      priv->toplevel_connected = TRUE;

      GtkAllocation allocation;

      gtk_widget_get_allocation (widget, &allocation);
      gdk_window_get_position (gtk_widget_get_window (widget), &x_pos, &y_pos);

      overlay_scrollbar_calc_layout_range (scrollbar, gtk_range_get_value (GTK_RANGE (widget)));


      gtk_window_move (GTK_WINDOW (priv->thumb), x_pos + allocation.x - 4, y_pos + allocation.y);

      priv->pager = overlay_pager_new (priv->range);
      overlay_pager_show (priv->pager);

      overlay_scrollbar_store_window_position (scrollbar);
    }

  return TRUE;
}

/**
 * range_size_allocate_cb:
 * react to "size-allocate", to set window dimensions
 **/
static void
range_size_allocate_cb (GtkWidget     *widget,
                        GtkAllocation *allocation,
                        gpointer       user_data)
{
  DEBUG
  OverlayScrollbar *scrollbar;
  OverlayScrollbarPrivate *priv;

  scrollbar = OVERLAY_SCROLLBAR (user_data);
  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (scrollbar);

/*  gtk_window_set_default_size (GTK_WINDOW (scrollbar), OVERLAY_SCROLLBAR_WIDTH, OVERLAY_SCROLLBAR_HEIGHT);*/

  priv->slider.width = OVERLAY_SCROLLBAR_WIDTH;
  priv->slider.height = OVERLAY_SCROLLBAR_HEIGHT;

  priv->range_all = *allocation;

  overlay_scrollbar_calc_layout_range (scrollbar, gtk_range_get_value (GTK_RANGE (widget)));
  if (priv->pager != NULL)
    overlay_resize_window (scrollbar);

      overlay_scrollbar_store_window_position (scrollbar);
}

/**
 * range_value_changed_cb:
 * react to "value-changed" signal, emitted when there's scrolling
 **/
static void
range_value_changed_cb (GtkWidget      *widget,
                        gpointer        user_data)
{
  DEBUG
  GtkRange *range = GTK_RANGE (widget);
  OverlayScrollbar *scrollbar;
  OverlayScrollbarPrivate *priv;

  scrollbar = OVERLAY_SCROLLBAR (user_data);
  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (scrollbar);

  overlay_scrollbar_calc_layout_range (scrollbar, gtk_range_get_value (GTK_RANGE (widget)));
  overlay_scrollbar_calc_layout_slider (scrollbar, range->adjustment->value);

  if (!priv->motion_notify_event && !priv->enter_notify_event)
    gtk_widget_hide (GTK_WIDGET (priv->thumb));

  overlay_move (scrollbar);
}

/* TOPLEVEL FUNCTIONS */
/**
 * toplevel_configure_event_cb:
 * react to "configure-event" signal: move windows
 **/
static gboolean
toplevel_configure_event_cb (GtkWidget         *widget,
                             GdkEventConfigure *event,
                             gpointer           user_data)
{
  DEBUG
  OverlayScrollbar *scrollbar;
  OverlayScrollbarPrivate *priv;

  scrollbar = OVERLAY_SCROLLBAR (user_data);
  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (scrollbar);

  gtk_widget_hide (GTK_WIDGET (priv->thumb));

  /* XXX insted using allocation please do it by storing the position of the scrollbar window */

  GtkAllocation allocation;

  gtk_widget_get_allocation (GTK_WIDGET (priv->range), &allocation);

  overlay_scrollbar_calc_layout_range (scrollbar, gtk_range_get_value (GTK_RANGE (priv->range)));
  overlay_scrollbar_calc_layout_slider (scrollbar, gtk_range_get_value (GTK_RANGE (priv->range)));
  gtk_window_move (GTK_WINDOW (priv->thumb), event->x + allocation.x + priv->slider.x - 4, event->y + allocation.y + priv->slider.y);

  overlay_scrollbar_store_window_position (scrollbar);


  overlay_resize_window (scrollbar);
  overlay_move (scrollbar);

  return FALSE;
}

/**
 * toplevel_enter_notify_event_cb:
 * triggered when the mouse is in the toplevel window
 **/
static gboolean
toplevel_enter_notify_event_cb (GtkWidget        *widget,
                                GdkEventCrossing *event,
                                gpointer          user_data)
{
  DEBUG
  OverlayScrollbar *scrollbar;
  OverlayScrollbarPrivate *priv;

  scrollbar = OVERLAY_SCROLLBAR (user_data);
  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (scrollbar);

  gdk_window_show (priv->overlay_window);

  return FALSE;
}

/**
 * toplevel_filter_func:
 * add a filter to the toplevel GdkWindow, to activate proximity effect
 **/
static GdkFilterReturn
toplevel_filter_func (GdkXEvent *gdkxevent,
                      GdkEvent  *event,
                      gpointer   user_data)
{
  DEBUG
  OverlayScrollbar *scrollbar;
  OverlayScrollbarPrivate *priv;
  XEvent *xevent;

  scrollbar = OVERLAY_SCROLLBAR (user_data);
  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (scrollbar);
  xevent = gdkxevent;

  overlay_scrollbar_calc_layout_range (scrollbar, gtk_range_get_value (GTK_RANGE (priv->range)));
  overlay_scrollbar_calc_layout_slider (scrollbar, gtk_range_get_value (GTK_RANGE (priv->range)));

  /* get the motion_notify_event trough XEvent */
  if (xevent->type == MotionNotify)
    {
      /* XXX missing horizontal */
      /* proximity area */
      if ((priv->range_all.x - xevent->xmotion.x < PROXIMITY_WIDTH &&
           priv->range_all.x + priv->trough.width - xevent->xmotion.x > 0) &&
          (xevent->xmotion.y >= priv->range_all.y + priv->overlay.y &&
           xevent->xmotion.y <= priv->range_all.y + priv->overlay.y + priv->overlay.height))
        {
          priv->can_hide = FALSE;

          if (priv->overlay.height > priv->slider.height)
            {
              gint x, y, x_pos, y_pos;

              gdk_window_get_position (gtk_widget_get_window (priv->range), &x_pos, &y_pos);

              x = priv->range_all.x;
              y = CLAMP (xevent->xmotion.y - priv->slider.height / 2,
                         priv->range_all.y + priv->overlay.y,
                         priv->range_all.y + priv->overlay.y + priv->overlay.height - priv->slider.height);

              gtk_window_move (GTK_WINDOW (priv->thumb), x_pos + x - 4, y_pos + y);
            }
          else
            {
              gtk_window_move (GTK_WINDOW (priv->thumb), priv->win_x - 4, priv->win_y + priv->slider.y);
            }

          gtk_widget_show (GTK_WIDGET (priv->thumb));
/*          overlay_scrollbar_map (GTK_WIDGET (scrollbar));*/
        }
      else
        {
          priv->can_hide = TRUE;
          overlay_scrollbar_hide (scrollbar);
        }
    }

  if (xevent->type == PropertyNotify)
    {
      GdkScreen *screen;
      GdkWindow *active_window;
      screen = gtk_widget_get_screen (priv->range);

      active_window = gdk_screen_get_active_window (screen);

      if (active_window != gtk_widget_get_window (priv->range))
        {
          gtk_widget_hide (GTK_WIDGET (priv->thumb));
          overlay_pager_set_active (OVERLAY_PAGER (priv->pager), FALSE);
        }
      else
        overlay_pager_set_active (OVERLAY_PAGER (priv->pager), TRUE);
    }

  return GDK_FILTER_CONTINUE;
}

/**
 * toplevel_leave_notify_event_cb:
 * hide the OverlayScrollbar, when the pointer leaves the toplevel GtkWindow
 **/
static gboolean
toplevel_leave_notify_event_cb (GtkWidget        *widget,
                                GdkEventCrossing *event,
                                gpointer          user_data)
{
  DEBUG
  OverlayScrollbar *scrollbar;
  OverlayScrollbarPrivate *priv;

  scrollbar = OVERLAY_SCROLLBAR (user_data);
  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (scrollbar);

  g_timeout_add (TIMEOUT_HIDE, overlay_scrollbar_hide, scrollbar);

  return FALSE;
}
