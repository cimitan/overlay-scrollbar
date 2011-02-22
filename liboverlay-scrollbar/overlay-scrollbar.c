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
  PROP_ADJUSTMENT,
  PROP_ORIENTATION,
  LAST_ARG
};

G_DEFINE_TYPE (OverlayScrollbar, overlay_scrollbar, GTK_TYPE_WIDGET);

#define OVERLAY_SCROLLBAR_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), OS_TYPE_OVERLAY_SCROLLBAR, OverlayScrollbarPrivate))

typedef struct _OverlayScrollbarPrivate OverlayScrollbarPrivate;

struct _OverlayScrollbarPrivate
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
static void overlay_scrollbar_map (GtkWidget *widget);

/*static void overlay_scrollbar_hide (GtkWidget *widget);*/

static void overlay_scrollbar_parent_set (GtkWidget *widget,
                                          GtkWidget *old_parent);

static void overlay_scrollbar_show (GtkWidget *widget);

/* GOBJECT CLASS FUNCTIONS */
static void overlay_scrollbar_dispose (GObject *object);

static void overlay_scrollbar_get_property (GObject    *object,
                                            guint       prop_id,
                                            GValue     *value,
                                            GParamSpec *pspec);

static void overlay_scrollbar_set_property (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec);

/* HELPER FUNCTIONS */
static void overlay_scrollbar_calc_layout_pager (OverlayScrollbar *scrollbar,
                                                 gdouble           adjustment_value);

static gdouble overlay_scrollbar_coord_to_value (OverlayScrollbar *scrollbar,
                                                 gint              coord);

static gboolean overlay_scrollbar_hide (gpointer user_data);

static void overlay_scrollbar_move (OverlayScrollbar *scrollbar,
                                    gint              mouse_x,
                                    gint              mouse_y);

static void overlay_scrollbar_store_window_position (OverlayScrollbar *scrollbar);

static void overlay_scrollbar_swap_adjustment (OverlayScrollbar *scrollbar,
                                               GtkAdjustment    *adjustment);

static void overlay_scrollbar_swap_parent (OverlayScrollbar *scrollbar,
                                           GtkWidget        *parent);

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

/* OVERLAY FUNCTIONS */
static void overlay_move (OverlayScrollbar *scrollbar);

static void overlay_resize_window (OverlayScrollbar *scrollbar);

/* ADJUSTMENT FUNCTIONS */
static void adjustment_value_changed_cb (GtkAdjustment *adjustment,
                                         gpointer       user_data);

/* PARENT FUNCTIONS */
static gboolean parent_expose_event_cb (GtkWidget      *widget,
                                        GdkEventExpose *event,
                                        gpointer        user_data);

static void parent_size_allocate_cb (GtkWidget     *widget,
                                     GtkAllocation *allocation,
                                     gpointer       user_data);

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

  widget_class->parent_set = overlay_scrollbar_parent_set;
  widget_class->show       = overlay_scrollbar_show;

  gobject_class->dispose      = overlay_scrollbar_dispose;
  gobject_class->get_property = overlay_scrollbar_get_property;
  gobject_class->set_property = overlay_scrollbar_set_property;

  g_object_class_install_property (gobject_class,
                                   PROP_ADJUSTMENT,
                                   g_param_spec_object ("adjustment",
                                                        "Adjustment",
                                                        "GtkAdjustment of the OverlayScrollbar",
                                                        GTK_TYPE_ADJUSTMENT,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  g_object_class_install_property (gobject_class,
                                   PROP_ORIENTATION,
                                   g_param_spec_enum ("orientation",
                                                      "Orientation",
                                                      "GtkOrientation of the OverlayScrollbar",
                                                      GTK_TYPE_ORIENTATION,
                                                      GTK_ORIENTATION_VERTICAL,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_STATIC_NAME |
                                                      G_PARAM_STATIC_NICK |
                                                      G_PARAM_STATIC_BLURB));

  g_type_class_add_private (gobject_class, sizeof (OverlayScrollbarPrivate));
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
  g_object_ref_sink (priv->thumb);

  /* thumb callbacks */
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
}

/**
 * overlay_scrollbar_hide:
 * override class function
 */
/*static void*/
/*overlay_scrollbar_hide (GtkWidget *widget)*/
/*{*/
/*  DEBUG*/
/*  OverlayScrollbarPrivate *priv;*/

/*  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (OVERLAY_SCROLLBAR (widget));*/
/*  gtk_widget_hide (GTK_WIDGET (priv->thumb));*/
/*}*/

static void
overlay_scrollbar_parent_set (GtkWidget *widget,
                              GtkWidget *old_parent)
{
  DEBUG
  overlay_scrollbar_swap_parent (OVERLAY_SCROLLBAR (widget), gtk_widget_get_parent (widget));
}

/**
 * overlay_scrollbar_show:
 * override class function
 */
static void
overlay_scrollbar_show (GtkWidget *widget)
{
  DEBUG
  OverlayScrollbarPrivate *priv;

  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (OVERLAY_SCROLLBAR (widget));
  gtk_widget_show (GTK_WIDGET (priv->thumb));
}

/* GOBJECT CLASS FUNCTIONS */
/**
 * overlay_scrollbar_dispose:
 * override class function
 */
static void
overlay_scrollbar_dispose (GObject *object)
{
  DEBUG
  OverlayScrollbarPrivate *priv = OVERLAY_SCROLLBAR_GET_PRIVATE (object);

  if (priv->thumb)
    {
      g_signal_handlers_disconnect_by_func (G_OBJECT (priv->thumb),
                                            overlay_thumb_button_press_event_cb, object);
      g_signal_handlers_disconnect_by_func (G_OBJECT (priv->thumb),
                                            overlay_thumb_button_release_event_cb, object);
      g_signal_handlers_disconnect_by_func (G_OBJECT (priv->thumb),
                                            overlay_thumb_motion_notify_event_cb, object);
      g_signal_handlers_disconnect_by_func (G_OBJECT (priv->thumb),
                                            overlay_thumb_enter_notify_event_cb, object);
      g_signal_handlers_disconnect_by_func (G_OBJECT (priv->thumb),
                                            overlay_thumb_leave_notify_event_cb, object);

      g_object_unref (priv->thumb);
      priv->thumb = NULL;
    }

  overlay_scrollbar_swap_adjustment (OVERLAY_SCROLLBAR (object), NULL);
  overlay_scrollbar_swap_parent (OVERLAY_SCROLLBAR (object), NULL);

  if (priv->pager != NULL)
    {
      g_object_unref (priv->pager);
      priv->pager = NULL;
    }

  G_OBJECT_CLASS (overlay_scrollbar_parent_class)->dispose (object);
  return;
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
      case PROP_ADJUSTMENT:
        g_value_set_object (value, priv->adjustment);
        break;
      case PROP_ORIENTATION:
        g_value_set_enum (value, priv->orientation);
        break;
    }
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
      case PROP_ADJUSTMENT:
        {
          overlay_scrollbar_swap_adjustment (scrollbar, g_value_get_object (value));
          break;
        }
      case PROP_ORIENTATION:
        priv->orientation = g_value_get_enum (value);
        break;
    }
}

/* PUBLIC FUNCTIONS*/
/**
 * overlay_scrollbar_new:
 * @orientation: the #GtkOrientation
 * @adjustment: the pointer to the #GtkAdjustment to connect
 *
 * Creates a new overlay scrollbar.
 *
 * Returns: the new overlay scrollbar as a #GtkWidget
 */
GtkWidget*
overlay_scrollbar_new (GtkOrientation  orientation,
                       GtkAdjustment  *adjustment)
{
  DEBUG
  return g_object_new (OS_TYPE_OVERLAY_SCROLLBAR,
                       "orientation", orientation,
                       "adjustment",  adjustment,
                       NULL);
}

/* HELPER FUNCTIONS */
/**
 * overlay_scrollbar_calc_layout_pager:
 * calculate layout and store info
 **/
static void
overlay_scrollbar_calc_layout_pager (OverlayScrollbar *scrollbar,
                                     gdouble           adjustment_value)
{
  DEBUG
  OverlayScrollbarPrivate *priv;

  gint tmp_height;

  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (scrollbar);
  tmp_height = priv->overlay.height;

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      gint y, bottom, top, height;

      top = priv->trough.y;
      bottom = priv->trough.y + priv->trough.height;

      /* overlay height is the fraction (page_size /
       * total_adjustment_range) times the trough height in pixels
       */

/*      if (priv->adjustment->upper - priv->adjustment->lower != 0)*/
      height = ((bottom - top) * (priv->adjustment->page_size /
                                 (priv->adjustment->upper - priv->adjustment->lower)));
/*      else*/
/*        height = range->min_slider_size;*/

/*      if (height < range->min_slider_size ||*/
/*          range->slider_size_fixed)*/
/*        height = range->min_slider_size;*/

      height = MIN (height, priv->trough.height);

      y = top;

      if (priv->adjustment->upper - priv->adjustment->lower - priv->adjustment->page_size != 0)
        y += (bottom - top - height) * ((adjustment_value - priv->adjustment->lower) /
                                        (priv->adjustment->upper - priv->adjustment->lower - priv->adjustment->page_size));

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

/*      if (priv->adjustment->upper - priv->adjustment->lower != 0)*/
      width = ((right - left) * (priv->adjustment->page_size /
                                (priv->adjustment->upper - priv->adjustment->lower)));
/*      else*/
/*        width = range->min_slider_size;*/

/*      if (width < range->min_slider_size ||*/
/*          range->slider_size_fixed)*/
/*        width = range->min_slider_size;*/

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

/**
 * overlay_scrollbar_calc_layout_slider:
 * calculate OverlayScrollbar layout and store info
 **/
static void
overlay_scrollbar_calc_layout_slider (OverlayScrollbar *scrollbar,
                                      gdouble           adjustment_value)
{
  DEBUG
  OverlayScrollbarPrivate *priv;

  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (scrollbar);

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

/**
 * overlay_scrollbar_coord_to_value:
 * traduce pixels into proper values
 **/
static gdouble
overlay_scrollbar_coord_to_value (OverlayScrollbar *scrollbar,
                                  gint              coord)
{
  DEBUG
  OverlayScrollbarPrivate *priv;
  gdouble frac;
  gdouble value;
  gint    trough_length;
  gint    trough_start;
  gint    slider_length;

  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (scrollbar);

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
  OverlayScrollbarPrivate *priv;
  gint delta;
  gint c;
  gdouble new_value;

  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (scrollbar);

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    delta = mouse_y - priv->slide_initial_coordinate;
  else
    delta = mouse_x - priv->slide_initial_coordinate;

  c = priv->slide_initial_slider_position + delta;

  new_value = overlay_scrollbar_coord_to_value (scrollbar, c);

  gtk_adjustment_set_value (priv->adjustment, new_value);
  gtk_adjustment_value_changed (priv->adjustment);
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

/**
 * overlay_scrollbar_swap_adjustment:
 * swap the adjustment pointer
 */
static void
overlay_scrollbar_swap_adjustment (OverlayScrollbar *scrollbar,
                                   GtkAdjustment    *adjustment)
{
  DEBUG
  OverlayScrollbarPrivate *priv = OVERLAY_SCROLLBAR_GET_PRIVATE (scrollbar);

  if (priv->adjustment != NULL)
    {
      g_signal_handlers_disconnect_by_func (G_OBJECT (priv->adjustment),
                                            G_CALLBACK (adjustment_value_changed_cb), scrollbar);

      g_object_unref (priv->adjustment);
    }

  priv->adjustment = adjustment;

  if (priv->adjustment != NULL)
    {
      g_object_ref_sink (priv->adjustment);

      g_signal_connect (G_OBJECT (priv->adjustment), "value-changed",
                        G_CALLBACK (adjustment_value_changed_cb), scrollbar);
    }

  return;
}

/**
 * overlay_scrollbar_swap_parent:
 * swap the parent pointer
 */
static void
overlay_scrollbar_swap_parent (OverlayScrollbar *scrollbar,
                               GtkWidget        *parent)
{
  DEBUG
  OverlayScrollbarPrivate *priv = OVERLAY_SCROLLBAR_GET_PRIVATE (scrollbar);

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

  return;
}

/* THUMB FUNCTIONS */
/**
 * overlay_thumb_button_press_event_cb:
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
/*              GtkAllocation allocation;*/

/*              gtk_widget_get_allocation (widget, &allocation);*/

/*              if (priv->orientation == GTK_ORIENTATION_VERTICAL)*/
/*                {*/
/*                  if (priv->pointer_y < allocation.height / 2)*/
/*                    g_signal_emit_by_name (priv->range, "move-slider", GTK_SCROLL_PAGE_UP);*/
/*                  else*/
/*                    g_signal_emit_by_name (priv->range, "move-slider", GTK_SCROLL_PAGE_DOWN);*/
/*                }*/
/*              else*/
/*                {*/
/*                  if (priv->pointer_x < allocation.width / 2)*/
/*                    g_signal_emit_by_name (priv->range, "move-slider", GTK_SCROLL_PAGE_UP);*/
/*                  else*/
/*                    g_signal_emit_by_name (priv->range, "move-slider", GTK_SCROLL_PAGE_DOWN);*/
/*                }*/

              priv->value_changed_event = TRUE;
            }

          priv->button_press_event = FALSE;
          priv->motion_notify_event = FALSE;

          gtk_widget_queue_draw (widget);
        }
    }

  return FALSE;
}

/**
 * overlay_thumb_enter_notify_event:
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
 * overlay_thumb_leave_notify_event:
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

  g_timeout_add (TIMEOUT_HIDE, overlay_scrollbar_hide, scrollbar);

  return TRUE;
}

/**
 * overlay_thumb_motion_notify_event:
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

      gtk_window_move (GTK_WINDOW (widget), x, y);
    }

  return TRUE;
}

/* OVERLAY FUNCTIONS */
/**
 * overlay_move:
 * move the overlay_pager to the right position
 **/
static void
overlay_move (OverlayScrollbar *scrollbar)
{
  DEBUG
  GdkRectangle mask;
  OverlayScrollbarPrivate *priv;

  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (scrollbar);

  mask.x = 0;
  mask.y = priv->overlay.y;
  mask.width = 3;
  mask.height = priv->overlay.height;

/*   XXX missing horizontal and - 8 is hardcoded */
  overlay_pager_move_resize (OVERLAY_PAGER (priv->pager), mask);
}

/**
 * overlay_resize_window:
 * resize the overlay window
 **/
static void
overlay_resize_window (OverlayScrollbar *scrollbar)
{
  DEBUG
  GdkRectangle rect;
  OverlayScrollbarPrivate *priv;

  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (scrollbar);

  rect.x = priv->overlay_all.x;
  rect.y = priv->overlay_all.y + 1;
  rect.width = 3;
  rect.height = priv->overlay_all.height - 2;

  overlay_pager_size_allocate (OVERLAY_PAGER (priv->pager), rect);
}

/* ADJUSTMENT FUNCTIONS */
/**
 * adjustment_value_changed_cb:
 * react to "value-changed" signal, emitted when there's scrolling
 **/
static void
adjustment_value_changed_cb (GtkAdjustment *adjustment,
                             gpointer       user_data)
{
  DEBUG
  OverlayScrollbar *scrollbar;
  OverlayScrollbarPrivate *priv;

  scrollbar = OVERLAY_SCROLLBAR (user_data);
  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (scrollbar);

  overlay_scrollbar_calc_layout_pager (scrollbar, adjustment->value);
  overlay_scrollbar_calc_layout_slider (scrollbar, adjustment->value);

  if (!priv->motion_notify_event && !priv->enter_notify_event)
    gtk_widget_hide (GTK_WIDGET (priv->thumb));

  overlay_move (scrollbar);
}

/* PARENT FUNCTIONS */
/*
 * parent_expose_event_cb:
 * react to "expose-event", to connect other callbacks and useful things
 **/
static gboolean
parent_expose_event_cb (GtkWidget      *widget,
                        GdkEventExpose *event,
                        gpointer        user_data)
{
  DEBUG
  OverlayScrollbar *scrollbar;
  OverlayScrollbarPrivate *priv;

  scrollbar = OVERLAY_SCROLLBAR (user_data);
  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (scrollbar);

  if (!priv->toplevel_connected)
    {
      gint x_pos, y_pos;

      gdk_window_add_filter (gtk_widget_get_window (widget), toplevel_filter_func, scrollbar);
      g_signal_connect (G_OBJECT (gtk_widget_get_toplevel (widget)), "configure-event",
                        G_CALLBACK (toplevel_configure_event_cb), scrollbar);
      g_signal_connect (G_OBJECT (gtk_widget_get_toplevel (widget)), "leave-notify-event",
                        G_CALLBACK (toplevel_leave_notify_event_cb), scrollbar);
      priv->toplevel_connected = TRUE;

      GtkAllocation allocation;

      gtk_widget_get_allocation (widget, &allocation);
      gdk_window_get_position (gtk_widget_get_window (widget), &x_pos, &y_pos);

      overlay_scrollbar_calc_layout_pager (scrollbar, priv->adjustment->value);

      gtk_window_move (GTK_WINDOW (priv->thumb), x_pos + allocation.x, y_pos + allocation.y);

      if (priv->pager != NULL)
        {
          g_object_unref (priv->pager);
          priv->pager = NULL;
        }

      priv->pager = overlay_pager_new (widget);
      overlay_pager_show (OVERLAY_PAGER (priv->pager));

      overlay_scrollbar_store_window_position (scrollbar);
    }

  return FALSE;
}

/**
 * parent_size_allocate_cb:
 * react to "size-allocate", to set window dimensions
 **/
static void
parent_size_allocate_cb (GtkWidget     *widget,
                         GtkAllocation *allocation,
                         gpointer       user_data)
{
  DEBUG
  OverlayScrollbar *scrollbar;
  OverlayScrollbarPrivate *priv;

  scrollbar = OVERLAY_SCROLLBAR (user_data);
  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (scrollbar);

  priv->slider.width = OVERLAY_SCROLLBAR_WIDTH;
  priv->slider.height = OVERLAY_SCROLLBAR_HEIGHT;

  priv->trough.x = 0;
  priv->trough.y = 0;
  priv->trough.width = allocation->width;
  priv->trough.height = allocation->height;

  priv->overlay_all = *allocation;
  priv->thumb_all = *allocation;

  priv->overlay_all.x = allocation->x + allocation->width - 8;
  priv->thumb_all.x = allocation->x + allocation->width - 5;

  if (priv->adjustment != NULL)
    overlay_scrollbar_calc_layout_pager (scrollbar, priv->adjustment->value);

  if (priv->pager != NULL)
    overlay_resize_window (scrollbar);

  overlay_scrollbar_store_window_position (scrollbar);
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

  overlay_scrollbar_calc_layout_pager (scrollbar, priv->adjustment->value);
  overlay_scrollbar_calc_layout_slider (scrollbar, priv->adjustment->value);
  gtk_window_move (GTK_WINDOW (priv->thumb),
                   event->x + priv->thumb_all.x + priv->slider.x,
                   event->y + priv->thumb_all.y + priv->slider.y);

  overlay_scrollbar_store_window_position (scrollbar);

  overlay_resize_window (scrollbar);
  overlay_move (scrollbar);

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

  overlay_scrollbar_calc_layout_pager (scrollbar, priv->adjustment->value);
  overlay_scrollbar_calc_layout_slider (scrollbar, priv->adjustment->value);

  /* get the motion_notify_event trough XEvent */
  if (xevent->type == MotionNotify)
    {
      /* XXX missing horizontal */
      /* proximity area */
      if ((priv->thumb_all.x - xevent->xmotion.x < PROXIMITY_WIDTH &&
           priv->thumb_all.x + priv->slider.width - xevent->xmotion.x > 0) &&
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

              gtk_window_move (GTK_WINDOW (priv->thumb), x_pos + x, y_pos + y);
            }
          else
            {
              gtk_window_move (GTK_WINDOW (priv->thumb), priv->win_x, priv->win_y + priv->slider.y);
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
