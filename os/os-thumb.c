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

#ifndef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include "os-private.h"

/* Default size of the thumb in pixels. */
#define DEFAULT_THUMB_WIDTH  15
#define DEFAULT_THUMB_HEIGHT 80

#define OS_THUMB_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), OS_TYPE_THUMB, OsThumbPrivate))

typedef struct _OsThumbPrivate OsThumbPrivate;

struct _OsThumbPrivate {
  GtkOrientation orientation;
  gboolean button_press_event;
  gboolean enter_notify_event;
  gboolean motion_notify_event;
  gboolean can_hide;
  gboolean can_rgba;
  gint pointer_x;
  gint pointer_y;
};

enum {
  PROP_0,
  PROP_ORIENTATION,
  LAST_ARG
};

static gboolean os_thumb_button_press_event (GtkWidget *widget, GdkEventButton *event);
static gboolean os_thumb_button_release_event (GtkWidget *widget, GdkEventButton *event);
static void os_thumb_composited_changed (GtkWidget *widget);
static gboolean os_thumb_enter_notify_event (GtkWidget *widget, GdkEventCrossing *event);
static gboolean os_thumb_expose (GtkWidget *widget, GdkEventExpose *event);
static gboolean os_thumb_leave_notify_event (GtkWidget *widget, GdkEventCrossing *event);
static gboolean os_thumb_motion_notify_event (GtkWidget *widget, GdkEventMotion *event);
static void os_thumb_screen_changed (GtkWidget *widget, GdkScreen *old_screen);
static GObject* os_thumb_constructor (GType type, guint n_construct_properties, GObjectConstructParam *construct_properties);
static void os_thumb_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void os_thumb_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);

/* Private functions. */

/* Draw a rounded rectangle. */
static void
os_cairo_draw_rounded_rect (cairo_t *cr,
                            gdouble  x,
                            gdouble  y,
                            gdouble  width,
                            gdouble  height,
                            gdouble  radius)
{
  if (radius < 1)
    {
      cairo_rectangle (cr, x, y, width, height);
      return;
    }

  radius = MIN (radius, MIN (width / 2.0, height / 2.0));

  cairo_move_to (cr, x + radius, y);

  cairo_arc (cr, x + width - radius, y + radius, radius, G_PI * 1.5, G_PI * 2);
  cairo_arc (cr, x + width - radius, y + height - radius, radius, 0, G_PI * 0.5);
  cairo_arc (cr, x + radius, y + height - radius, radius, G_PI * 0.5, G_PI);
  cairo_arc (cr, x + radius, y + radius, radius, G_PI, G_PI * 1.5);
}

/* Type definition. */

G_DEFINE_TYPE (OsThumb, os_thumb, GTK_TYPE_WINDOW);

static void
os_thumb_class_init (OsThumbClass *class)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS (class);
  widget_class = GTK_WIDGET_CLASS (class);

  widget_class->button_press_event   = os_thumb_button_press_event;
  widget_class->button_release_event = os_thumb_button_release_event;
  widget_class->composited_changed   = os_thumb_composited_changed;
  widget_class->enter_notify_event   = os_thumb_enter_notify_event;
  widget_class->expose_event         = os_thumb_expose;
  widget_class->leave_notify_event   = os_thumb_leave_notify_event;
  widget_class->motion_notify_event  = os_thumb_motion_notify_event;
  widget_class->screen_changed       = os_thumb_screen_changed;

  gobject_class->constructor  = os_thumb_constructor;
  gobject_class->get_property = os_thumb_get_property;
  gobject_class->set_property = os_thumb_set_property;

  g_object_class_install_property
      (gobject_class, PROP_ORIENTATION,
       g_param_spec_enum ("orientation", "Orientation",
                          "GtkOrientation of the OsThumb",
                          GTK_TYPE_ORIENTATION, GTK_ORIENTATION_VERTICAL,
                          G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                          G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_type_class_add_private (gobject_class, sizeof (OsThumbPrivate));
}

static void
os_thumb_init (OsThumb *thumb)
{
  OsThumbPrivate *priv;

  priv = OS_THUMB_GET_PRIVATE (thumb);

  priv->can_hide = TRUE;
  priv->can_rgba = FALSE;

  gtk_window_set_default_size (GTK_WINDOW (thumb),
                               DEFAULT_THUMB_WIDTH,
                               DEFAULT_THUMB_HEIGHT);
  gtk_window_set_skip_pager_hint (GTK_WINDOW (thumb), TRUE);
  gtk_window_set_skip_taskbar_hint (GTK_WINDOW (thumb), TRUE);
  /* gtk_window_set_has_resize_grip (GTK_WINDOW (thumb), FALSE); */
  gtk_window_set_decorated (GTK_WINDOW (thumb), FALSE);
  gtk_window_set_focus_on_map (GTK_WINDOW (thumb), FALSE);
  gtk_window_set_accept_focus (GTK_WINDOW (thumb), FALSE);
  gtk_widget_set_app_paintable (GTK_WIDGET (thumb), TRUE);
  gtk_widget_add_events (GTK_WIDGET (thumb), GDK_BUTTON_PRESS_MASK |
                                             GDK_BUTTON_RELEASE_MASK |
                                             GDK_POINTER_MOTION_MASK);

  os_thumb_screen_changed (GTK_WIDGET (thumb), NULL);
  os_thumb_composited_changed (GTK_WIDGET (thumb));
}

static gboolean
os_thumb_button_press_event (GtkWidget      *widget,
                             GdkEventButton *event)
{
  if (event->type == GDK_BUTTON_PRESS)
    {
      if (event->button == 1)
        {
          OsThumbPrivate *priv;

          priv = OS_THUMB_GET_PRIVATE (OS_THUMB (widget));

          priv->pointer_x = event->x;
          priv->pointer_y = event->y;

          priv->button_press_event = TRUE;
          priv->motion_notify_event = FALSE;
        }
    }

  return FALSE;
}

static gboolean
os_thumb_button_release_event (GtkWidget      *widget,
                               GdkEventButton *event)
{
  if (event->type == GDK_BUTTON_RELEASE)
    {
      if (event->button == 1)
        {
          OsThumbPrivate *priv;

          priv = OS_THUMB_GET_PRIVATE (OS_THUMB (widget));

          priv->button_press_event = FALSE;
          priv->motion_notify_event = FALSE;
        }
    }

  return FALSE;
}

static void
os_thumb_composited_changed (GtkWidget *widget)
{
  GdkScreen *screen;

  screen = gtk_widget_get_screen (widget);

  if (gdk_screen_is_composited (screen))
    {
      OsThumbPrivate *priv;

      priv = OS_THUMB_GET_PRIVATE (OS_THUMB (widget));

      priv->can_rgba = TRUE;
    }

  gtk_widget_queue_draw (widget);
}

static gboolean
os_thumb_enter_notify_event (GtkWidget        *widget,
                             GdkEventCrossing *event)
{
  OsThumbPrivate *priv;

  priv = OS_THUMB_GET_PRIVATE (OS_THUMB (widget));

  priv->enter_notify_event = TRUE;
  priv->can_hide = FALSE;

  return TRUE;
}

static gboolean
os_thumb_expose (GtkWidget      *widget,
                 GdkEventExpose *event)
{
  GtkAllocation allocation;
  GtkStateType state_type_down, state_type_up;
  OsThumbPrivate *priv;
  cairo_pattern_t *pat;
  cairo_t *cr;
  gint x, y, width, height;
  gint radius;

  priv = OS_THUMB_GET_PRIVATE (OS_THUMB (widget));

  state_type_down = GTK_STATE_NORMAL;
  state_type_up = GTK_STATE_NORMAL;

  gtk_widget_get_allocation (widget, &allocation);

  x = 0;
  y = 0;
  width = allocation.width;
  height = allocation.height;
  radius = priv->can_rgba ? 18 : 0;

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

  os_cairo_draw_rounded_rect (cr, x, y, width, height, radius);
  pat = cairo_pattern_create_linear (x, y, width + x, y);
  cairo_pattern_add_color_stop_rgba (pat, 0.0, 0.95, 0.95, 0.95, 1.0);
  cairo_pattern_add_color_stop_rgba (pat, 1.0, 0.8, 0.8, 0.8, 1.0);
  cairo_set_source (cr, pat);
  cairo_pattern_destroy (pat);
  cairo_fill (cr);

  os_cairo_draw_rounded_rect (cr, x, y, width, height, radius);
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

  if (priv->motion_notify_event)
    {
      cairo_set_source_rgba (cr, 0.5, 0.5, 0.5, 0.2);
      cairo_fill_preserve (cr);
      cairo_set_source_rgba (cr, 0.5, 0.5, 0.5, 1.0);
      cairo_stroke (cr);

      os_cairo_draw_rounded_rect (cr, x + 1, y + 1, width - 2, height - 2, radius + 1);
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
    }
  else
    {
      cairo_stroke (cr);

      os_cairo_draw_rounded_rect (cr, x + 1, y + 1, width - 2, height - 2, radius + 1);
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
    }

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

static gboolean
os_thumb_leave_notify_event (GtkWidget        *widget,
                             GdkEventCrossing *event)
{
  OsThumbPrivate *priv;

  priv = OS_THUMB_GET_PRIVATE (OS_THUMB (widget));

  if (!priv->button_press_event)
    priv->can_hide = TRUE;

/*  g_timeout_add (TIMEOUT_HIDE, os_thumb_hide, widget);*/

  return TRUE;
}

static gboolean
os_thumb_motion_notify_event (GtkWidget      *widget,
                              GdkEventMotion *event)
{
  OsThumbPrivate *priv;

  priv = OS_THUMB_GET_PRIVATE (OS_THUMB (widget));

  if (priv->button_press_event)
    {
      if (!priv->motion_notify_event)
        gtk_widget_queue_draw (widget);

      priv->motion_notify_event = TRUE;
    }

  return TRUE;
}

static void
os_thumb_screen_changed (GtkWidget *widget,
                         GdkScreen *old_screen)
{
  GdkScreen *screen;
  GdkColormap *colormap;

  screen = gtk_widget_get_screen (widget);
  colormap = gdk_screen_get_rgba_colormap (screen);

  if (colormap)
    gtk_widget_set_colormap (widget, colormap);
}

static GObject*
os_thumb_constructor (GType                  type,
                      guint                  n_construct_properties,
                      GObjectConstructParam *construct_properties)
{
  GObject *object;

  object = G_OBJECT_CLASS (os_thumb_parent_class)->constructor
      (type, n_construct_properties, construct_properties);

  g_object_set (object, "type", GTK_WINDOW_POPUP, NULL);

  return object;
}

static void
os_thumb_get_property (GObject    *object,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  OsThumbPrivate *priv;

  priv = OS_THUMB_GET_PRIVATE (OS_THUMB (object));

  switch (prop_id)
    {
      case PROP_ORIENTATION:
        g_value_set_enum (value, priv->orientation);
        break;

      default:
        break;
    }
}

static void
os_thumb_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  OsThumbPrivate *priv;

  priv = OS_THUMB_GET_PRIVATE (OS_THUMB (object));

  switch (prop_id)
    {
      case PROP_ORIENTATION:
        priv->orientation = g_value_get_enum (value);
        break;

      default:
        break;
    }
}

/* Public functions. */

/**
 * os_thumb_new:
 *
 * Creates a new OsThumb instance.
 *
 * Returns: a new OsThumb instance.
 */
GtkWidget*
os_thumb_new (GtkOrientation orientation)
{
  return g_object_new (OS_TYPE_THUMB, "orientation", orientation, NULL);
}
