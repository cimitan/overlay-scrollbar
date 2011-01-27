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

#include "overlay-scrollbar.h"
#include "overlay-scrollbar-cairo-support.h"
#include "overlay-scrollbar-support.h"


#define DEVELOPMENT_FLAG FALSE

#if DEVELOPMENT_FLAG
#define DEBUG printf("%s()\n", __func__);
#else
#define DEBUG
#endif

#define OVERLAY_SCROLLBAR_HEIGHT 100 /* height/width of the overlay scrollbar, in pixels */
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

  GtkAllocation range_all;
  GtkOrientation orientation;
  GtkWidget *range;

  gboolean button_press_event;
  gboolean enter_notify_event;
  gboolean motion_notify_event;
  gboolean value_changed_event;

  gboolean toplevel_connected;

  gboolean can_hide;

  gint win_x;
  gint win_y;

  gint slide_initial_slider_position;
  gint slide_initial_coordinate;

  gint pointer_x;
  gint pointer_y;
};

/* SUBCLASS FUNCTIONS */
static gboolean overlay_scrollbar_button_press_event (GtkWidget      *widget,
                                                      GdkEventButton *event);

static gboolean overlay_scrollbar_button_release_event (GtkWidget      *widget,
                                                        GdkEventButton *event);

static GObject* overlay_scrollbar_constructor (GType                  type,
                                               guint                  n_construct_properties,
                                               GObjectConstructParam *construct_properties);

static gboolean overlay_scrollbar_enter_notify_event (GtkWidget        *widget,
                                                      GdkEventCrossing *event);

static gboolean overlay_scrollbar_expose (GtkWidget      *widget,
                                          GdkEventExpose *event);

static void overlay_scrollbar_get_property (GObject    *object,
                                            guint       prop_id,
                                            GValue     *value,
                                            GParamSpec *pspec);

static gboolean overlay_scrollbar_leave_notify_event (GtkWidget        *widget,
                                                      GdkEventCrossing *event);

static void overlay_scrollbar_map (GtkWidget *widget);

static gboolean overlay_scrollbar_motion_notify_event (GtkWidget      *widget,
                                                       GdkEventMotion *event);

static void overlay_scrollbar_screen_changed (GtkWidget *widget,
                                              GdkScreen *old_screen);

static void overlay_scrollbar_set_property (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec);

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

static gboolean
overlay_scrollbar_button_press_event (GtkWidget      *widget,
                                      GdkEventButton *event)
{
  DEBUG
  if (event->type == GDK_BUTTON_PRESS)
    {
      if (event->button == 1)
        {
          GtkRange *range;
          OverlayScrollbarPrivate *priv;

          priv = OVERLAY_SCROLLBAR_GET_PRIVATE (OVERLAY_SCROLLBAR (widget));

          overlay_scrollbar_map (widget);
          gtk_window_set_transient_for (GTK_WINDOW (widget), GTK_WINDOW (gtk_widget_get_toplevel (priv->range)));
          os_present_gdk_window_with_timestamp (priv->range, event->time);

          range = GTK_RANGE (priv->range);

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

          overlay_scrollbar_store_window_position (OVERLAY_SCROLLBAR (widget));
        }
    }

  return FALSE;
}

/**
 * overlay_scrollbar_button_release_event:
 * override class function
 **/
static gboolean
overlay_scrollbar_button_release_event (GtkWidget      *widget,
                                        GdkEventButton *event)
{
  DEBUG
  if (event->type == GDK_BUTTON_RELEASE)
    {
      if (event->button == 1)
        {
          OverlayScrollbarPrivate *priv;

          priv = OVERLAY_SCROLLBAR_GET_PRIVATE (OVERLAY_SCROLLBAR (widget));

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

  widget_class->button_press_event   = overlay_scrollbar_button_press_event;
  widget_class->button_release_event = overlay_scrollbar_button_release_event;
  widget_class->enter_notify_event   = overlay_scrollbar_enter_notify_event;
  widget_class->expose_event         = overlay_scrollbar_expose;
  widget_class->leave_notify_event   = overlay_scrollbar_leave_notify_event;
  widget_class->map                  = overlay_scrollbar_map;
  widget_class->motion_notify_event  = overlay_scrollbar_motion_notify_event;
  widget_class->screen_changed       = overlay_scrollbar_screen_changed;

  gobject_class->constructor  = overlay_scrollbar_constructor;
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
 * overlay_scrollbar_constructor:
 * override class function
 **/
static GObject*
overlay_scrollbar_constructor (GType                  type,
                               guint                  n_construct_properties,
                               GObjectConstructParam *construct_properties)
{
  DEBUG
  GObject *object;

  object = G_OBJECT_CLASS (overlay_scrollbar_parent_class)->constructor (type,
                                                                         n_construct_properties,
                                                                         construct_properties);

  g_object_set (object, "type", GTK_WINDOW_POPUP, NULL);

  return object;
}

/**
 * overlay_scrollbar_enter_notify_event:
 * override class function
 **/
static gboolean
overlay_scrollbar_enter_notify_event (GtkWidget        *widget,
                                      GdkEventCrossing *event)
{
  DEBUG
  OverlayScrollbarPrivate *priv;

  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (OVERLAY_SCROLLBAR (widget));

  priv->enter_notify_event = TRUE;
  priv->can_hide = FALSE;

  return TRUE;
}

/**
 * overlay_scrollbar_expose:
 * override class function
 **/
static gboolean
overlay_scrollbar_expose (GtkWidget      *widget,
                          GdkEventExpose *event)
{
  DEBUG
  GtkAllocation allocation;
  GtkStateType state_type_down, state_type_up;
  OverlayScrollbarPrivate *priv;
  cairo_pattern_t *pat;
  cairo_t *cr;
  gint x, y, width, height;

  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (OVERLAY_SCROLLBAR (widget));

  state_type_down = GTK_STATE_NORMAL;
  state_type_up = GTK_STATE_NORMAL;

  gtk_widget_get_allocation (widget, &allocation);

  x = 0;
  y = 0;
  width = allocation.width;
  height = allocation.height;

  cr = gdk_cairo_create (gtk_widget_get_window (widget));

  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba (cr, 0, 0, 0, 0);
  cairo_paint (cr);

  cairo_save (cr);
  cairo_translate (cr, 0.5, 0.5);
  width--;
  height--;

  cairo_set_line_width (cr, 1);
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

  os_cairo_draw_rounded_rect (cr, x, y, width, height, 6);
  pat = cairo_pattern_create_linear (x, y, width + x, y);
  cairo_pattern_add_color_stop_rgba (pat, 0.0, 0.95, 0.95, 0.95, 1.0);
  cairo_pattern_add_color_stop_rgba (pat, 1.0, 0.8, 0.8, 0.8, 1.0);
  cairo_set_source (cr, pat);
  cairo_pattern_destroy (pat);
  cairo_fill (cr);

  os_cairo_draw_rounded_rect (cr, x, y, width, height, 6);
  pat = cairo_pattern_create_linear (x, y, x, height + y);
  if (priv->button_press_event && !priv->motion_notify_event)
    {
      if (priv->pointer_y < height / 2)
        {
          state_type_up = GTK_STATE_ACTIVE;
          cairo_pattern_add_color_stop_rgba (pat, 0.0, 0.8, 0.8, 0.8, 0.8);
          cairo_pattern_add_color_stop_rgba (pat, 0.49, 1.0, 1.0, 1.0, 0.0);
          cairo_pattern_add_color_stop_rgba (pat, 0.49, 0.8, 0.8, 0.8, 0.5);
          cairo_pattern_add_color_stop_rgba (pat, 1.0, 0.8, 0.8, 0.8, 0.8);
        }
      else
        {
          state_type_down = GTK_STATE_ACTIVE;
          cairo_pattern_add_color_stop_rgba (pat, 0.0, 1.0, 1.0, 1.0, 0.8);
          cairo_pattern_add_color_stop_rgba (pat, 0.49, 1.0, 1.0, 1.0, 0.0);
          cairo_pattern_add_color_stop_rgba (pat, 0.49, 0.8, 0.8, 0.8, 0.5);
          cairo_pattern_add_color_stop_rgba (pat, 1.0, 0.6, 0.6, 0.6, 0.8);
        }
    }
  else
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

  os_cairo_draw_rounded_rect (cr, x + 1, y + 1, width - 2, height - 2, 7);
  cairo_set_source_rgba (cr, 1, 1, 1, 0.5);
  cairo_stroke (cr);

  cairo_move_to (cr, x + 0.5, y - 1 + height / 2);
  cairo_line_to (cr, width - 0.5, y - 1 + height / 2);
  cairo_set_source_rgba (cr, 0.6, 0.6, 0.6, 0.4);
  cairo_stroke (cr);

  cairo_move_to (cr, x + 0.5, y + height / 2);
  cairo_line_to (cr, width - 0.5, y + height / 2);
  cairo_set_source_rgba (cr, 1, 1, 1, 0.5);
  cairo_stroke (cr);

  cairo_restore (cr);

  gtk_paint_arrow (gtk_widget_get_style (widget),
                   gtk_widget_get_window (widget),
                   state_type_up,
                   GTK_SHADOW_IN,
                   NULL,
                   widget,
                   "arrow",
                   GTK_ARROW_UP,
                   FALSE,
                   4,
                   4,
                   width - 8,
                   width - 8);

  gtk_paint_arrow (gtk_widget_get_style (widget),
                   gtk_widget_get_window (widget),
                   state_type_down,
                   GTK_SHADOW_NONE,
                   NULL,
                   widget,
                   "arrow",
                   GTK_ARROW_DOWN,
                   FALSE,
                   4,
                   height - (width - 8) - 4,
                   width - 8,
                   width - 8);

  cairo_destroy (cr);

  return FALSE;
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

  gtk_window_set_skip_pager_hint (GTK_WINDOW (scrollbar), TRUE);
  gtk_window_set_skip_taskbar_hint (GTK_WINDOW (scrollbar), TRUE);
  gtk_window_set_has_resize_grip (GTK_WINDOW (scrollbar), FALSE);
  gtk_window_set_decorated (GTK_WINDOW (scrollbar), FALSE);
  gtk_window_set_focus_on_map (GTK_WINDOW (scrollbar), FALSE);
  gtk_window_set_accept_focus (GTK_WINDOW (scrollbar), FALSE);
  gtk_widget_set_app_paintable (GTK_WIDGET (scrollbar), TRUE);
  gtk_widget_add_events (GTK_WIDGET (scrollbar), GDK_BUTTON_PRESS_MASK |
                                                 GDK_BUTTON_RELEASE_MASK |
                                                 GDK_POINTER_MOTION_MASK);

  overlay_scrollbar_screen_changed (GTK_WIDGET (scrollbar), NULL);
}

/**
 * overlay_scrollbar_leave_notify_event:
 * override class function
 **/
static gboolean
overlay_scrollbar_leave_notify_event (GtkWidget        *widget,
                                      GdkEventCrossing *event)
{
  DEBUG
  OverlayScrollbarPrivate *priv;

  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (OVERLAY_SCROLLBAR (widget));

  if (!priv->button_press_event)
    priv->can_hide = TRUE;

  g_timeout_add (TIMEOUT_HIDE, overlay_scrollbar_hide, widget);

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
  Display *display;
  GtkWidget *parent;
  OverlayScrollbarPrivate *priv;
  XWindowChanges changes;
  guint32 xid, xid_parent;
  int res;

  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (OVERLAY_SCROLLBAR (widget));

  parent = priv->range;

  xid = GDK_WINDOW_XID (gtk_widget_get_window (widget));
  xid_parent = GDK_WINDOW_XID (gtk_widget_get_window (parent));
  display = GDK_WINDOW_XDISPLAY (gtk_widget_get_window (widget));

  changes.sibling = xid_parent;
  changes.stack_mode = Above;

/*  printf ("xid: %i, xid_parent: %i\n", xid, xid_parent);*/

  gdk_error_trap_push ();
  XConfigureWindow (display, xid,  CWSibling | CWStackMode, &changes);

  gdk_flush ();
  if ((res = gdk_error_trap_pop ()))
    g_warning ("Received X error: %d\n", res);

  GTK_WIDGET_CLASS (overlay_scrollbar_parent_class)->map (widget);
}

/**
 * overlay_scrollbar_motion_notify_event:
 * override class function
 **/
static gboolean
overlay_scrollbar_motion_notify_event (GtkWidget *widget,
                                       GdkEventMotion *event)
{
  DEBUG
  OverlayScrollbarPrivate *priv;

  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (OVERLAY_SCROLLBAR (widget));

  /* XXX improve speed by not rendering when moving */
  if (priv->button_press_event)
    {
      gint x, y;

      priv->motion_notify_event = TRUE;

      overlay_scrollbar_move (OVERLAY_SCROLLBAR (widget), event->x_root, event->y_root);

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

      gtk_widget_queue_draw (widget);
    }

  return TRUE;
}

/**
 * overlay_scrollbar_screen_changed:
 * override class function
 **/
static void
overlay_scrollbar_screen_changed (GtkWidget *widget,
                                  GdkScreen *old_screen)
{
  DEBUG
  GdkScreen *screen;
  GdkColormap *colormap;

  screen = gtk_widget_get_screen (widget);
  colormap = gdk_screen_get_rgba_colormap (screen);

  if (colormap)
    gtk_widget_set_colormap (widget, colormap);
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

  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (scrollbar);
  range = GTK_RANGE (priv->range);

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
    gtk_widget_hide (GTK_WIDGET (scrollbar));

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
      priv->win_x = win_x + priv->range_all.x + 10;
      priv->win_y = win_y + priv->range_all.y;
    }
  else
    {
      priv->win_x = win_x + priv->range_all.x;
      priv->win_y = win_y + priv->range_all.y + 10;
    }
/*  printf ("%i %i\n", priv->win_x, priv->win_y);*/
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
      g_signal_connect (G_OBJECT (gtk_widget_get_toplevel (widget)), "configure-event",
                        G_CALLBACK (toplevel_configure_event_cb), scrollbar);
      g_signal_connect (G_OBJECT (gtk_widget_get_toplevel (widget)), "leave-notify-event",
                        G_CALLBACK (toplevel_leave_notify_event_cb), scrollbar);
      priv->toplevel_connected = TRUE;
/*    }*/

/*  if (!priv->motion_notify_event)*/
/*    {*/
      GtkAllocation allocation;

      gtk_widget_get_allocation (widget, &allocation);
      gdk_window_get_position (gtk_widget_get_window (widget), &x_pos, &y_pos);

      overlay_scrollbar_calc_layout_range (scrollbar, gtk_range_get_value (GTK_RANGE (widget)));
      gtk_window_move (GTK_WINDOW (scrollbar), x_pos + allocation.x + 10, y_pos + allocation.y);

      overlay_scrollbar_store_window_position (scrollbar);
/*      printf ("init: %i %i\n", priv->win_x, priv->win_y);*/
    }

  return FALSE;
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

  gtk_window_set_default_size (GTK_WINDOW (scrollbar), allocation->width, OVERLAY_SCROLLBAR_HEIGHT);

  priv->slider.width = allocation->width;
  priv->slider.height = OVERLAY_SCROLLBAR_HEIGHT;

  priv->range_all = *allocation;

  overlay_scrollbar_calc_layout_range (scrollbar, gtk_range_get_value (GTK_RANGE (widget)));
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

  if (!priv->motion_notify_event)
    gtk_widget_hide (GTK_WIDGET (scrollbar));
/*  gtk_window_move (GTK_WINDOW (scrollbar), priv->win_x + priv->slider.x, priv->win_y + priv->slider.y);*/

/*  overlay_scrollbar_store_window_position (scrollbar);*/
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

  gtk_widget_hide (GTK_WIDGET (scrollbar));

  /* XXX insted using allocation please do it by storing the position of the scrollbar window */

  GtkAllocation allocation;

  gtk_widget_get_allocation (GTK_WIDGET (priv->range), &allocation);

  overlay_scrollbar_calc_layout_range (scrollbar, gtk_range_get_value (GTK_RANGE (priv->range)));
  overlay_scrollbar_calc_layout_slider (scrollbar, gtk_range_get_value (GTK_RANGE (priv->range)));
  gtk_window_move (GTK_WINDOW (scrollbar), event->x+allocation.x + 10 + priv->slider.x, event->y+allocation.y + priv->slider.y);

  overlay_scrollbar_store_window_position (scrollbar);

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
          overlay_scrollbar_store_window_position (scrollbar);

          if (priv->overlay.height > priv->slider.height)
            {
              gint x, y, x_pos, y_pos;

              gdk_window_get_position (gtk_widget_get_window (priv->range), &x_pos, &y_pos);

              x = priv->range_all.x + 10;
              y = CLAMP (xevent->xmotion.y - priv->slider.height / 2,
                         priv->range_all.y + priv->overlay.y,
                         priv->range_all.y + priv->overlay.y + priv->overlay.height - priv->slider.height);

              gtk_window_move (GTK_WINDOW (scrollbar), x_pos + x, y_pos + y);
            }

          gtk_widget_show (GTK_WIDGET (scrollbar));
          overlay_scrollbar_map (GTK_WIDGET (scrollbar));
        }
      else
        {
          priv->can_hide = TRUE;
          overlay_scrollbar_hide (scrollbar);
        }
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

/*  if (!priv->button_press_event && !priv->enter_notify_event)*/
/*    {*/
      g_timeout_add (TIMEOUT_HIDE, overlay_scrollbar_hide, scrollbar);
/*      gtk_widget_hide (os->thumb);*/
/*    }*/

  return FALSE;
}
