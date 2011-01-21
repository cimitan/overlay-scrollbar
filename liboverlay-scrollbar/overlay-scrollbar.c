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
#include "overlay-scrollbar-support.h"
#include "overlay-scrollbar-cairo-support.h"

#define DEVELOPMENT_FLAG FALSE

#if DEVELOPMENT_FLAG
#define DEBUG g_debug("%s()\n", __func__);
#else
#define DEBUG
#endif

#define OVERLAY_SCROLLBAR_HEIGHT 100

enum {
  PROP_0,

  PROP_SLIDER,

  LAST_ARG
};

G_DEFINE_TYPE (OverlayScrollbar, overlay_scrollbar, GTK_TYPE_WINDOW);

#define OVERLAY_SCROLLBAR_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), OS_TYPE_OVERLAY_SCROLLBAR, OverlayScrollbarPrivate))

typedef struct _OverlayScrollbarPrivate OverlayScrollbarPrivate;

struct _OverlayScrollbarPrivate
{
  GdkRectangle stepper_a;
  GdkRectangle stepper_b;
  GdkRectangle stepper_c;
  GdkRectangle stepper_d;

  GdkRectangle trough;
  GdkRectangle slider;

  GdkRectangle range_rect;
  GtkOrientation orientation;
  GtkWidget *range;

  gboolean button_press_event;
  gboolean enter_notify_event;
  gboolean motion_notify_event;
  gboolean value_changed_event;

  gint slide_initial_slider_position;
  gint slide_initial_coordinate;

  gint win_x;
  gint win_y;

  gint pointer_x;
  gint pointer_y;
  gint pointer_x_root;
  gint pointer_y_root;
  gint slider_start;
  gint slider_end;
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
static void overlay_scrollbar_calc_layout (OverlayScrollbar *scrollbar,
                                           gdouble           adjustment_value);

static gdouble overlay_scrollbar_coord_to_value (OverlayScrollbar *scrollbar,
                                                 gint              coord);

static void overlay_scrollbar_move (OverlayScrollbar *scrollbar,
                                    gint              mouse_x,
                                    gint              mouse_y);

static void overlay_scrollbar_store_window_position (OverlayScrollbar *scrollbar);

/* SLIDER FUNCTIONS */
static gboolean slider_expose_event_cb (GtkWidget      *widget,
                                        GdkEventExpose *event,
                                        gpointer        user_data);

static void slider_size_allocate_cb (GtkWidget     *widget,
                                     GtkAllocation *allocation,
                                     gpointer       user_data);

static void slider_value_changed_cb (GtkWidget      *widget,
                                     gpointer        user_data);

/* -------------------------------------------------------------------------- */

/* SUBCLASS FUNCTIONS */
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
              priv->slide_initial_slider_position = priv->slider.y;
              priv->slide_initial_coordinate = event->y;
            }
          else
            {
              priv->slide_initial_slider_position = priv->slider.x;
              priv->slide_initial_coordinate = event->x;
            }

          priv->pointer_x = event->x;
          priv->pointer_y = event->y;
          priv->pointer_x_root = event->x_root;
          priv->pointer_y_root = event->y_root;

          gtk_widget_queue_draw (widget);

          overlay_scrollbar_store_window_position (OVERLAY_SCROLLBAR (widget));
        }
    }

  return FALSE;
}

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

          priv->button_press_event = FALSE;
          priv->motion_notify_event = FALSE;

          gtk_widget_queue_draw (widget);
        }
    }

  return FALSE;
}

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
  widget_class->map                  = overlay_scrollbar_map;
  widget_class->motion_notify_event  = overlay_scrollbar_motion_notify_event;
  widget_class->screen_changed       = overlay_scrollbar_screen_changed;

  gobject_class->constructor  = overlay_scrollbar_constructor;
  gobject_class->get_property = overlay_scrollbar_get_property;
  gobject_class->set_property = overlay_scrollbar_set_property;

  g_type_class_add_private (gobject_class, sizeof (OverlayScrollbarPrivate));

  g_object_class_install_property (gobject_class,
                                   PROP_SLIDER,
                                   g_param_spec_object ("slider",
                                                        "Scrollbar Slider",
                                                        "The slider of the attached scrollbar",
                                                        GTK_TYPE_RANGE,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));
}

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

static gboolean
overlay_scrollbar_enter_notify_event (GtkWidget        *widget,
                                      GdkEventCrossing *event)
{
  DEBUG
  OverlayScrollbarPrivate *priv;

  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (OVERLAY_SCROLLBAR (widget));

  priv->enter_notify_event = TRUE;

  return TRUE;
}

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
  pat = cairo_pattern_create_linear (x, y, width+x, y);
  cairo_pattern_add_color_stop_rgba (pat, 0.0, 0.95, 0.95, 0.95, 1.0);
  cairo_pattern_add_color_stop_rgba (pat, 1.0, 0.8, 0.8, 0.8, 1.0);
  cairo_set_source (cr, pat);
  cairo_pattern_destroy (pat);
  cairo_fill (cr);

  os_cairo_draw_rounded_rect (cr, x, y, width, height, 6);
  pat = cairo_pattern_create_linear (x, y, x, height+y);
  if (priv->button_press_event && !priv->motion_notify_event)
    {
      if (priv->pointer_y < height/2)
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

  os_cairo_draw_rounded_rect (cr, x+1, y+1, width-2, height-2, 7);
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
                   width-8,
                   width-8);

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
                   height-(width-8)-4,
                   width-8,
                   width-8);

  cairo_destroy (cr);

  return FALSE;
}

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
      case PROP_SLIDER:
        g_value_set_object (value, priv->range);
        break;
    }
}

static void
overlay_scrollbar_init (OverlayScrollbar *scrollbar)
{
  DEBUG

  OverlayScrollbarPrivate *priv;

  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (scrollbar);

  priv->orientation = GTK_ORIENTATION_VERTICAL;

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
      priv->motion_notify_event = TRUE;

/*      printf ("event_y; %f %i %f\n", event->y_root-priv->win_y, priv->win_y, event->y);*/

      overlay_scrollbar_move (OVERLAY_SCROLLBAR (widget), event->x_root-priv->win_x, event->y_root-priv->win_y);

      gtk_widget_queue_draw (widget);
    }

  return TRUE;
}

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
      case PROP_SLIDER:
        {
          priv->range = g_value_get_object (value);
          g_signal_connect (G_OBJECT (priv->range), "size-allocate",
                            G_CALLBACK (slider_size_allocate_cb), scrollbar);
          g_signal_connect (G_OBJECT (priv->range), "expose-event",
                            G_CALLBACK (slider_expose_event_cb), scrollbar);
          g_signal_connect (G_OBJECT (priv->range), "value-changed",
                            G_CALLBACK (slider_value_changed_cb), scrollbar);
          break;
        }
    }
}

/* public functions */

/**
 * overlay_scrollbar_new:
 * @slider: a pointer to the GtkRange slider to connect
 *
 * Creates a new overlay scrollbar.
 *
 * Returns: the new overlay scrollbar as a #GtkWidget
 */
GtkWidget*
overlay_scrollbar_new (GtkRange *slider)
{
  DEBUG
  return g_object_new (OS_TYPE_OVERLAY_SCROLLBAR , "slider", slider, NULL);
}

/**
 * overlay_scrollbar_set_slider:
 * @widget: an OverlayScrollbar widget
 * @slider: a pointer to a GtkScrollbar slider
 *
 * Sets the GtkScrollbar slider to control trough the OverlayScrollbar.
 *
 */
void
overlay_scrollbar_set_slider (GtkWidget *widget,
                              GtkWidget *slider)
{
  DEBUG
  OverlayScrollbarPrivate *priv;

  g_return_if_fail (OS_IS_OVERLAY_SCROLLBAR (widget) && GTK_RANGE (slider));

  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (widget);

  priv->range = slider;
}

/* HELPER FUNCTIONS */
static void
overlay_scrollbar_calc_layout (OverlayScrollbar *scrollbar,
                               gdouble           adjustment_value)
{
  DEBUG
  GdkRectangle range_rect;
  GtkBorder border;
  GtkRange *range;
  OverlayScrollbarPrivate *priv;
  gint slider_width, stepper_size, focus_width, trough_border, stepper_spacing;
  gint slider_length;
  gint n_steppers;
  gboolean has_steppers_ab;
  gboolean has_steppers_cd;
  gboolean trough_under_steppers;

  /* If we have a too-small allocation, we prefer the steppers over
   * the trough/slider, probably the steppers are a more useful
   * feature in small spaces.
   *
   * Also, we prefer to draw the range itself rather than the border
   * areas if there's a conflict, since the borders will be decoration
   * not controls. Though this depends on subclasses cooperating by
   * not drawing on range->range_rect.
   */

  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (scrollbar);
  range = GTK_RANGE (priv->range);

  os_gtk_range_get_props (range,
                          &slider_width, &stepper_size,
                          &focus_width, &trough_border,
                          &stepper_spacing, &trough_under_steppers,
                          NULL, NULL);

  os_gtk_range_calc_request (range,
                             slider_width, stepper_size,
                             focus_width, trough_border, stepper_spacing,
                             &range_rect, &border, &n_steppers,
                             &has_steppers_ab, &has_steppers_cd, &slider_length);

  /* We never expand to fill available space in the small dimension
   * (i.e. vertical scrollbars are always a fixed width)
   */
  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      os_clamp_dimensions (GTK_WIDGET (scrollbar), &range_rect, &border, TRUE);
    }
  else
    {
      os_clamp_dimensions (GTK_WIDGET (scrollbar), &range_rect, &border, FALSE);
    }

  range_rect.x = border.left;
  range_rect.y = border.top;

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      gint stepper_width, stepper_height;

      /* Steppers are the width of the range, and stepper_size in
       * height, or if we don't have enough height, divided equally
       * among available space.
       */
      stepper_width = range_rect.width - focus_width * 2;

      if (trough_under_steppers)
        stepper_width -= trough_border * 2;

      if (stepper_width < 1)
        stepper_width = range_rect.width; /* screw the trough border */

      if (n_steppers == 0)
        stepper_height = 0; /* avoid divide by n_steppers */
      else
        stepper_height = MIN (stepper_size, (range_rect.height / n_steppers));

      /* Stepper A */

      priv->stepper_a.x = range_rect.x + focus_width + trough_border * trough_under_steppers;
      priv->stepper_a.y = range_rect.y + focus_width + trough_border * trough_under_steppers;

      /* Stepper B */
      if (range->has_stepper_a)
        {
          priv->stepper_a.width = stepper_width;
          priv->stepper_a.height = stepper_height;
        }
      else
        {
          priv->stepper_a.width = 0;
          priv->stepper_a.height = 0;
        }

      /* Stepper B */

      priv->stepper_b.x = priv->stepper_a.x;
      priv->stepper_b.y = priv->stepper_a.y + priv->stepper_a.height;

      if (range->has_stepper_b)
        {
          priv->stepper_b.width = stepper_width;
          priv->stepper_b.height = stepper_height;
        }
      else
        {
          priv->stepper_b.width = 0;
          priv->stepper_b.height = 0;
        }

      /* Stepper D */

      if (range->has_stepper_d)
        {
          priv->stepper_d.width = stepper_width;
          priv->stepper_d.height = stepper_height;
        }
      else
        {
          priv->stepper_d.width = 0;
          priv->stepper_d.height = 0;
        }

      priv->stepper_d.x = priv->stepper_a.x;
      priv->stepper_d.y = range_rect.y + range_rect.height - priv->stepper_d.height - focus_width - trough_border * trough_under_steppers;

      /* Stepper C */

      if (range->has_stepper_c)
        {
          priv->stepper_c.width = stepper_width;
          priv->stepper_c.height = stepper_height;
        }
      else
        {
          priv->stepper_c.width = 0;
          priv->stepper_c.height = 0;
        }

      priv->stepper_c.x = priv->stepper_a.x;
      priv->stepper_c.y = priv->stepper_d.y - priv->stepper_c.height;

      /* Now the trough is the remaining space between steppers B and C,
       * if any, minus spacing
       */
      priv->trough.x = range_rect.x;
      priv->trough.y = priv->stepper_b.y + priv->stepper_b.height + stepper_spacing * has_steppers_ab;
      priv->trough.width = range_rect.width;
      priv->trough.height = priv->stepper_c.y - priv->trough.y - stepper_spacing * has_steppers_cd;

      /* Slider fits into the trough, with stepper_spacing on either side,
       * and the size/position based on the adjustment or fixed, depending.
       */
      priv->slider.x = priv->trough.x + focus_width + trough_border;
      priv->slider.width = priv->trough.width - (focus_width + trough_border) * 2;

      /* Compute slider position/length */
      {
        gint y, bottom, top, height;

        top = priv->trough.y;
        bottom = priv->trough.y + priv->trough.height;

        if (! trough_under_steppers)
          {
            top += trough_border;
            bottom -= trough_border;
          }

        /* slider height is the fraction (page_size /
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

        priv->slider.y = y;
        priv->slider.height = height;

        priv->slider_start = priv->slider.y;
        priv->slider_end = priv->slider.y + priv->slider.height;
      }
    }
  else /* priv->orientation == GTK_ORIENTATION_HORIZONTAL */
    {
      gint stepper_width, stepper_height;

      /* Steppers are the height of the range, and stepper_size in
       * width, or if we don't have enough width, divided equally
       * among available space.
       */
      stepper_height = range_rect.height + focus_width * 2;

      if (trough_under_steppers)
        stepper_height -= trough_border * 2;

      if (stepper_height < 1)
        stepper_height = range_rect.height; /* screw the trough border */

      if (n_steppers == 0)
        stepper_width = 0; /* avoid divide by n_steppers */
      else
        stepper_width = MIN (stepper_size, (range_rect.width / n_steppers));

      /* Stepper A */

      priv->stepper_a.x = range_rect.x + focus_width + trough_border * trough_under_steppers;
      priv->stepper_a.y = range_rect.y + focus_width + trough_border * trough_under_steppers;

      if (range->has_stepper_a)
        {
          priv->stepper_a.width = stepper_width;
          priv->stepper_a.height = stepper_height;
        }
      else
        {
          priv->stepper_a.width = 0;
          priv->stepper_a.height = 0;
        }

      /* Stepper B */

      priv->stepper_b.x = priv->stepper_a.x + priv->stepper_a.width;
      priv->stepper_b.y = priv->stepper_a.y;

      if (range->has_stepper_b)
        {
          priv->stepper_b.width = stepper_width;
          priv->stepper_b.height = stepper_height;
        }
      else
        {
          priv->stepper_b.width = 0;
          priv->stepper_b.height = 0;
        }

      /* Stepper D */

      if (range->has_stepper_d)
        {
          priv->stepper_d.width = stepper_width;
          priv->stepper_d.height = stepper_height;
        }
      else
        {
          priv->stepper_d.width = 0;
          priv->stepper_d.height = 0;
        }

      priv->stepper_d.x = range_rect.x + range_rect.width - priv->stepper_d.width - focus_width - trough_border * trough_under_steppers;
      priv->stepper_d.y = priv->stepper_a.y;


      /* Stepper C */

      if (range->has_stepper_c)
        {
          priv->stepper_c.width = stepper_width;
          priv->stepper_c.height = stepper_height;
        }
      else
        {
          priv->stepper_c.width = 0;
          priv->stepper_c.height = 0;
        }

      priv->stepper_c.x = priv->stepper_d.x - priv->stepper_c.width;
      priv->stepper_c.y = priv->stepper_a.y;

      /* Now the trough is the remaining space between steppers B and C,
       * if any
       */
      priv->trough.x = priv->stepper_b.x + priv->stepper_b.width + stepper_spacing * has_steppers_ab;
      priv->trough.y = range_rect.y;

      priv->trough.width = priv->stepper_c.x - priv->trough.x - stepper_spacing * has_steppers_cd;
      priv->trough.height = range_rect.height;

      /* Slider fits into the trough, with stepper_spacing on either side,
       * and the size/position based on the adjustment or fixed, depending.
       */
      priv->slider.y = priv->trough.y + focus_width + trough_border;
      priv->slider.height = priv->trough.height - (focus_width + trough_border) * 2;

      /* Compute slider position/length */
      {
        gint x, left, right, width;

        left = priv->trough.x;
        right = priv->trough.x + priv->trough.width;

        if (! trough_under_steppers)
          {
            left += trough_border;
            right -= trough_border;
          }

        /* slider width is the fraction (page_size /
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


        priv->slider.x = x;
        priv->slider.width = width;

        priv->slider_start = priv->slider.x;
        priv->slider_end = priv->slider.x + priv->slider.width;
      }
    }

  /* XXX missing SENSIVITY code */
}

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
  gint    trough_border;
  gint    trough_under_steppers;

  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (scrollbar);
  range = GTK_RANGE (priv->range);

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      trough_length = priv->trough.height;
      trough_start  = priv->trough.y;
      slider_length = priv->slider.height;
    }
  else
    {
      trough_length = priv->trough.width;
      trough_start  = priv->trough.x;
      slider_length = priv->slider.width;
    }

  os_gtk_range_get_props (range, NULL, NULL, NULL, &trough_border, NULL,
                          &trough_under_steppers, NULL, NULL);

  if (!trough_under_steppers)
    {
      trough_start += trough_border;
      trough_length -= 2 * trough_border;
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

/*  printf ("%f %i %i\n", new_value, mouse_y, priv->slide_initial_coordinate);*/

  gtk_window_move (GTK_WINDOW (scrollbar), priv->win_x, priv->win_y+c);

  g_signal_emit_by_name (range, "change-value",
                         GTK_SCROLL_JUMP, new_value, &handled, NULL);

}

static void
overlay_scrollbar_store_window_position (OverlayScrollbar *scrollbar)
{
  DEBUG
  OverlayScrollbarPrivate *priv;

  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (scrollbar);

  gdk_window_get_position (gtk_widget_get_window (GTK_WIDGET (scrollbar)), &priv->win_x, &priv->win_y);
}

/* SLIDER FUNCTIONS */
static gboolean
slider_expose_event_cb (GtkWidget      *widget,
                        GdkEventExpose *event,
                        gpointer        user_data)
{
  DEBUG
  GtkAllocation allocation;
  OverlayScrollbar *scrollbar;
  OverlayScrollbarPrivate *priv;

  gint x_pos, y_pos;
  scrollbar = OVERLAY_SCROLLBAR (user_data);
  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (scrollbar);

  gtk_widget_get_allocation (widget, &allocation);
  gdk_window_get_position (gtk_widget_get_window (widget), &x_pos, &y_pos);

  if (!priv->motion_notify_event)
    gtk_window_move (GTK_WINDOW (user_data), x_pos+allocation.x+20, y_pos+allocation.y);

  return FALSE;
}

static void
slider_size_allocate_cb (GtkWidget     *widget,
                         GtkAllocation *allocation,
                         gpointer       user_data)
{
  DEBUG
  OverlayScrollbar *scrollbar;
  OverlayScrollbarPrivate *priv;

  scrollbar = OVERLAY_SCROLLBAR (user_data);
  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (scrollbar);

  gtk_window_set_default_size (GTK_WINDOW (scrollbar), allocation->width, OVERLAY_SCROLLBAR_HEIGHT);

  overlay_scrollbar_calc_layout (scrollbar, gtk_range_get_value (GTK_RANGE (widget)));
}

static void
slider_value_changed_cb (GtkWidget      *widget,
                         gpointer        user_data)
{
  DEBUG
  OverlayScrollbar *scrollbar;
  OverlayScrollbarPrivate *priv;

  scrollbar = OVERLAY_SCROLLBAR (user_data);
  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (scrollbar);

  overlay_scrollbar_calc_layout (scrollbar, gtk_range_get_value (GTK_RANGE (widget)));

/*  overlay_scrollbar_store_window_position (scrollbar);*/

/*  gtk_window_move (GTK_WINDOW (scrollbar), priv->win_x, priv->win_y+priv->slider.y);*/
}
