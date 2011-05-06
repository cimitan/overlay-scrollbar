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

#include "os-private.h"
#include "math.h"
#include <stdlib.h>

/* Rate of the fade-out. */
#define RATE_FADE_OUT 30

/* Duration of the fade-out. */
#define DURATION_FADE_OUT 2000

/* Timeout before the fade-out. */
#define TIMEOUT_FADE_OUT 250

/* Number of tolerance pixels. */
#define TOLERANCE_PIXELS 3

/* Thumb radius in pixels (higher values are automatically clamped). */
#define THUMB_RADIUS 9

struct _OsThumbPrivate {
  GtkOrientation orientation;
  GtkWidget *grabbed_widget;
  OsAnimation *animation;
  gboolean button_press_event;
  gboolean motion_notify_event;
  gboolean can_rgba;
  gboolean use_tolerance;
  gboolean detached;
  gint pointer_x;
  gint pointer_y;
  guint32 source_id;
};

enum {
  PROP_0,
  PROP_ORIENTATION,
  LAST_ARG
};

static void os_thumb_fade_out_cb (gfloat weight, gpointer user_data);
static gboolean os_thumb_timeout_fade_out_cb (gpointer user_data);
static gboolean os_thumb_button_press_event (GtkWidget *widget, GdkEventButton *event);
static gboolean os_thumb_button_release_event (GtkWidget *widget, GdkEventButton *event);
static void os_thumb_composited_changed (GtkWidget *widget);
static gboolean os_thumb_expose (GtkWidget *widget, GdkEventExpose *event);
static gboolean os_thumb_leave_notify_event (GtkWidget *widget, GdkEventCrossing *event);
static gboolean os_thumb_motion_notify_event (GtkWidget *widget, GdkEventMotion *event);
static void os_thumb_map (GtkWidget *widget);
static void os_thumb_screen_changed (GtkWidget *widget, GdkScreen *old_screen);
static gboolean os_thumb_scroll_event (GtkWidget *widget, GdkEventScroll *event);
static void os_thumb_unmap (GtkWidget *widget);
static GObject* os_thumb_constructor (GType type, guint n_construct_properties, GObjectConstructParam *construct_properties);
static void os_thumb_dispose (GObject *object);
static void os_thumb_finalize (GObject *object);
static void os_thumb_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void os_thumb_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);

/* Private functions. */

/* Draw an arror. */
static void
os_cairo_draw_arrow (cairo_t *cr,
                     gdouble  x,
                     gdouble  y,
                     gdouble  width,
                     gdouble  height)
{
	cairo_save (cr);

  cairo_translate (cr, x, y);
	cairo_move_to (cr, -width / 2, -height / 2);
	cairo_line_to (cr, 0, height / 2);
	cairo_line_to (cr, width / 2, -height / 2);
	cairo_close_path (cr);

	cairo_set_source_rgba (cr, 0.3, 0.3, 0.3, 0.75);
	cairo_fill_preserve (cr);

	cairo_set_source_rgba (cr, 0.3, 0.3, 0.3, 1.0);
	cairo_stroke (cr);

	cairo_restore (cr);
}

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

static void
os_thumb_fade_out_cb (gfloat weight,
                      gpointer user_data)
{
  OsThumb *thumb;
  OsThumbPrivate *priv;

  thumb = OS_THUMB (user_data);

  priv = thumb->priv;

  if (weight < 1.0f)
    gtk_window_set_opacity (GTK_WINDOW (thumb), fabs (weight - 1.0f));
  else
    gtk_widget_hide (GTK_WIDGET (thumb));
}

static gboolean
os_thumb_timeout_fade_out_cb (gpointer user_data)
{
  OsThumb *thumb;
  OsThumbPrivate *priv;

  thumb = OS_THUMB (user_data);

  priv = thumb->priv;

  os_animation_start (priv->animation);
  priv->source_id = 0;

  return FALSE;
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
  widget_class->expose_event         = os_thumb_expose;
  widget_class->leave_notify_event   = os_thumb_leave_notify_event;
  widget_class->map                  = os_thumb_map;
  widget_class->motion_notify_event  = os_thumb_motion_notify_event;
  widget_class->screen_changed       = os_thumb_screen_changed;
  widget_class->scroll_event         = os_thumb_scroll_event;
  widget_class->unmap                = os_thumb_unmap;

  gobject_class->constructor  = os_thumb_constructor;
  gobject_class->dispose      = os_thumb_dispose;
  gobject_class->finalize     = os_thumb_finalize;
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

  thumb->priv = G_TYPE_INSTANCE_GET_PRIVATE (thumb,
                                             OS_TYPE_THUMB,
                                             OsThumbPrivate);
  priv = thumb->priv;

  priv->can_rgba = FALSE;
  priv->detached = FALSE;

  priv->source_id = 0;
  priv->animation = os_animation_new (RATE_FADE_OUT, DURATION_FADE_OUT,
                                      os_thumb_fade_out_cb, NULL, thumb);

  gtk_window_set_skip_pager_hint (GTK_WINDOW (thumb), TRUE);
  gtk_window_set_skip_taskbar_hint (GTK_WINDOW (thumb), TRUE);
  /* gtk_window_set_has_resize_grip (GTK_WINDOW (thumb), FALSE); */
  gtk_window_set_decorated (GTK_WINDOW (thumb), FALSE);
  gtk_window_set_focus_on_map (GTK_WINDOW (thumb), FALSE);
  gtk_window_set_accept_focus (GTK_WINDOW (thumb), FALSE);
  gtk_widget_set_app_paintable (GTK_WIDGET (thumb), TRUE);
  gtk_widget_add_events (GTK_WIDGET (thumb), GDK_BUTTON_PRESS_MASK |
                                             GDK_BUTTON_RELEASE_MASK |
                                             GDK_POINTER_MOTION_MASK |
                                             GDK_POINTER_MOTION_HINT_MASK);

  os_thumb_screen_changed (GTK_WIDGET (thumb), NULL);
  os_thumb_composited_changed (GTK_WIDGET (thumb));
}

static void
os_thumb_dispose (GObject *object)
{
  OsThumb *thumb;
  OsThumbPrivate *priv;

  thumb = OS_THUMB (object);
  priv = thumb->priv;

  if (priv->source_id != 0)
    {
      g_source_remove (priv->source_id);
      priv->source_id = 0;
    }

  if (priv->animation != NULL)
    {
      g_object_unref (priv->animation);
      priv->animation = NULL;
    }

  if (priv->grabbed_widget != NULL)
    {
      g_object_unref (priv->grabbed_widget);
      priv->grabbed_widget = NULL;
    }

  G_OBJECT_CLASS (os_thumb_parent_class)->dispose (object);
}

static void
os_thumb_finalize (GObject *object)
{
  G_OBJECT_CLASS (os_thumb_parent_class)->finalize (object);
}


static gboolean
os_thumb_button_press_event (GtkWidget      *widget,
                             GdkEventButton *event)
{
  OsThumb *thumb;
  OsThumbPrivate *priv;

  thumb = OS_THUMB (widget);
  priv = thumb->priv;

  if (priv->source_id != 0)
    {
      g_source_remove (priv->source_id);
      priv->source_id = 0;
    }

  /* Stop the animation on user interaction,
   * the button_press_event. */
  os_animation_stop (priv->animation);
  gtk_window_set_opacity (GTK_WINDOW (widget), 1.0f);

  if (event->type == GDK_BUTTON_PRESS)
    {
      if (event->button == 1)
        {
          gtk_grab_add (widget);

          priv->pointer_x = event->x;
          priv->pointer_y = event->y;

          priv->button_press_event = TRUE;
          priv->motion_notify_event = FALSE;

          priv->use_tolerance = TRUE;

          gtk_widget_queue_draw (widget);
        }
    }

  return FALSE;
}

static gboolean
os_thumb_button_release_event (GtkWidget      *widget,
                               GdkEventButton *event)
{
  GtkAllocation allocation;
  OsThumb *thumb;
  OsThumbPrivate *priv;

  thumb = OS_THUMB (widget);
  priv = thumb->priv;

  gtk_widget_get_allocation (widget, &allocation);

  if (event->type == GDK_BUTTON_RELEASE)
    {
      if (event->button == 1)
        {
          gtk_grab_remove (widget);

          priv->button_press_event = FALSE;
          priv->motion_notify_event = FALSE;

          gtk_widget_queue_draw (widget);
        }
    }

  return FALSE;
}

static void
os_thumb_composited_changed (GtkWidget *widget)
{
  OsThumb *thumb;
  OsThumbPrivate *priv;

  thumb = OS_THUMB (widget);
  priv = thumb->priv;

  priv->can_rgba = FALSE;

  if (gdk_screen_is_composited (gtk_widget_get_screen (widget)))
    {
      GdkVisual *visual;

      visual = gtk_widget_get_visual (widget);

      if (visual->depth == 32 && (visual->red_mask   == 0xff0000 &&
                                  visual->green_mask == 0x00ff00 &&
                                  visual->blue_mask  == 0x0000ff))
        priv->can_rgba = TRUE;
    }

  gtk_widget_queue_draw (widget);
}

static gboolean
os_thumb_expose (GtkWidget      *widget,
                 GdkEventExpose *event)
{
  GtkAllocation allocation;
  GtkStateType state_type_down, state_type_up;
  GtkStyle *style;
  OsThumb *thumb;
  OsThumbPrivate *priv;
  cairo_pattern_t *pat;
  cairo_t *cr;
  gint x, y, width, height;
  gint radius;

  style = gtk_widget_get_style (widget);

  thumb = OS_THUMB (widget);
  priv = thumb->priv;

  state_type_down = GTK_STATE_NORMAL;
  state_type_up = GTK_STATE_NORMAL;

  gtk_widget_get_allocation (widget, &allocation);

  x = 0;
  y = 0;
  width = allocation.width;
  height = allocation.height;
  radius = priv->can_rgba ? THUMB_RADIUS : 0;

  cr = gdk_cairo_create (gtk_widget_get_window (widget));

  cairo_save (cr);

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

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    pat = cairo_pattern_create_linear (x, y, width + x, y);
  else
    pat = cairo_pattern_create_linear (x, y, x, height + y);

  cairo_pattern_add_color_stop_rgba (pat, 0.0, 0.95, 0.95, 0.95, 1.0);
  cairo_pattern_add_color_stop_rgba (pat, 1.0, 0.8, 0.8, 0.8, 1.0);
  cairo_set_source (cr, pat);
  cairo_pattern_destroy (pat);
  cairo_fill (cr);

  os_cairo_draw_rounded_rect (cr, x, y, width, height, radius);

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    pat = cairo_pattern_create_linear (x, y, x, height + y);
  else
    pat = cairo_pattern_create_linear (x, y, width + x, y);

  if (priv->button_press_event && !priv->motion_notify_event)
    {
      if ((priv->orientation == GTK_ORIENTATION_VERTICAL && (priv->pointer_y < height / 2)) ||
          (priv->orientation == GTK_ORIENTATION_HORIZONTAL && (priv->pointer_x < width / 2)))
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


  if (priv->motion_notify_event)
    {
      cairo_fill_preserve (cr);
      cairo_set_source_rgba (cr, 0.5, 0.5, 0.5, 0.2);
      cairo_fill (cr);
    }
  else
    cairo_fill (cr);

  cairo_set_line_width (cr, 2.0);
  os_cairo_draw_rounded_rect (cr, x + 0.5, y + 0.5, width - 1, height - 1, radius - 1);
  if (!priv->detached)
    cairo_set_source_rgba (cr, style->bg[GTK_STATE_SELECTED].red/65535.0,
                               style->bg[GTK_STATE_SELECTED].green/65535.0,
                               style->bg[GTK_STATE_SELECTED].blue/65535.0, 1.0f);
  else
    cairo_set_source_rgba (cr, 0.6, 0.6, 0.6, 1.0f);
  cairo_stroke (cr);

  cairo_set_line_width (cr, 1.0);
  os_cairo_draw_rounded_rect (cr, x + 1, y + 1, width - 2, height - 2, radius - 1);
  cairo_set_source_rgba (cr, 0.1, 0.1, 0.1, 0.1);
  cairo_stroke (cr);

  os_cairo_draw_rounded_rect (cr, x + 2, y + 2, width - 4, height - 4, radius - 3);
  cairo_set_source_rgba (cr, 0.6, 0.6, 0.6, 0.5);
  cairo_stroke (cr);

  os_cairo_draw_rounded_rect (cr, x + 3, y + 3, width - 6, height - 6, radius - 4);
  cairo_set_source_rgba (cr, 1, 1, 1, 0.2);
  cairo_stroke (cr);

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      cairo_move_to (cr, x + 2.5, y - 1 + height / 2);
      cairo_line_to (cr, width - 2.5, y - 1 + height / 2);
      cairo_set_source_rgba (cr, 0.6, 0.6, 0.6, 0.4);
      cairo_stroke (cr);

      cairo_move_to (cr, x + 2.5, y + height / 2);
      cairo_line_to (cr, width - 2.5, y + height / 2);
      cairo_set_source_rgba (cr, 1, 1, 1, 0.5);
      cairo_stroke (cr);
    }
  else
    {
      cairo_move_to (cr, x - 1 + width / 2, y + 2.5);
      cairo_line_to (cr, x - 1 + width / 2, height - 2.5);
      cairo_set_source_rgba (cr, 0.6, 0.6, 0.6, 0.4);
      cairo_stroke (cr);

      cairo_move_to (cr, x + width / 2, y + 2.5);
      cairo_line_to (cr, x + width / 2, height - 2.5);
      cairo_set_source_rgba (cr, 1, 1, 1, 0.5);
      cairo_stroke (cr);
    }

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      /* direction UP. */
      cairo_save (cr);
  		cairo_translate (cr, 8.5, 8.5);
	    cairo_rotate (cr, G_PI);  
      os_cairo_draw_arrow (cr, 0.5, 0, 4, 3);
      cairo_restore (cr);

      /* direction DOWN. */
      cairo_save (cr);
  		cairo_translate (cr, 8.5, height - 8.5);
	    cairo_rotate (cr, 0);
      os_cairo_draw_arrow (cr, -0.5, 0, 4, 3);
      cairo_restore (cr);
    }
  else
    {
      /* direction LEFT. */
      cairo_save (cr);
  		cairo_translate (cr, 8.5, 8.5);
	    cairo_rotate (cr, G_PI * 0.5);  
      os_cairo_draw_arrow (cr, -0.5, 0, 4, 3);
      cairo_restore (cr);

      /* direction RIGHT. */
      cairo_save (cr);
  		cairo_translate (cr, width - 8.5, 8.5);
	    cairo_rotate (cr, G_PI * 1.5);
      os_cairo_draw_arrow (cr, 0.5, 0, 4, 3);
      cairo_restore (cr);
    }

  cairo_restore (cr);

  cairo_restore (cr);

  cairo_destroy (cr);

  return FALSE;
}

static gboolean
os_thumb_leave_notify_event (GtkWidget        *widget,
                             GdkEventCrossing *event)
{
  OsThumb *thumb;
  OsThumbPrivate *priv;

  thumb = OS_THUMB (widget);
  priv = thumb->priv;

  /* If we exit the thumb when a button is pressed,
   * there's no need to stop the animation, it should
   * already be stopped.
   * Stop it only if priv->button_press_event is FALSE. */
  if (!priv->button_press_event)
    {
      if (priv->source_id != 0)
        {
          g_source_remove (priv->source_id);
          priv->source_id = 0;
        }

      os_animation_stop (priv->animation);
    }

  priv->use_tolerance = FALSE;

  return FALSE;
}

static void
os_thumb_map (GtkWidget *widget)
{
  OsThumb *thumb;
  OsThumbPrivate *priv;

  thumb = OS_THUMB (widget);
  priv = thumb->priv;

  gtk_window_set_opacity (GTK_WINDOW (widget), 1.0f);

  if (priv->grabbed_widget != NULL)
    g_object_unref (priv->grabbed_widget);

  priv->grabbed_widget = gtk_grab_get_current ();

  if (priv->grabbed_widget != NULL)
    {
      g_object_ref_sink (priv->grabbed_widget);

      gtk_grab_remove (priv->grabbed_widget);
    }

  GTK_WIDGET_CLASS (os_thumb_parent_class)->map (widget);
}

static gboolean
os_thumb_motion_notify_event (GtkWidget      *widget,
                              GdkEventMotion *event)
{
  OsThumb *thumb;
  OsThumbPrivate *priv;

  thumb = OS_THUMB (widget);
  priv = thumb->priv;

  if (priv->source_id != 0)
    {
      g_source_remove (priv->source_id);
      priv->source_id = 0;
    }

  /* On motion, stop the fade-out. */
  os_animation_stop (priv->animation);
  gtk_window_set_opacity (GTK_WINDOW (widget), 1.0f);

  /* If you're not dragging, and you're outside
   * the tolerance pixels, enable the fade-out.
   * priv->motion_notify_event is TRUE only on dragging,
   * see code few lines below. */
  if (!priv->motion_notify_event)
  {
    if (!priv->use_tolerance ||
        (abs (priv->pointer_x - event->x) > TOLERANCE_PIXELS ||
         abs (priv->pointer_y - event->y) > TOLERANCE_PIXELS))
      {
        priv->use_tolerance = FALSE;
        priv->source_id = g_timeout_add (TIMEOUT_FADE_OUT,
                                         os_thumb_timeout_fade_out_cb,
                                         thumb);
      }
  }

  if (priv->button_press_event)
    {
      if (!priv->motion_notify_event)
        gtk_widget_queue_draw (widget);

      priv->motion_notify_event = TRUE;
    }

  return FALSE;
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

static gboolean
os_thumb_scroll_event (GtkWidget      *widget,
                       GdkEventScroll *event)
{
  OsThumb *thumb;
  OsThumbPrivate *priv;

  thumb = OS_THUMB (widget);
  priv = thumb->priv;

  if (priv->source_id != 0)
    {
      g_source_remove (priv->source_id);
      priv->source_id = 0;
    }

  /* if started, stop the fade-out. */
  os_animation_stop (priv->animation);
  gtk_window_set_opacity (GTK_WINDOW (widget), 1.0f);

  priv->source_id = g_timeout_add (TIMEOUT_FADE_OUT,
                                   os_thumb_timeout_fade_out_cb,
                                   thumb);

  return FALSE;
}

static void
os_thumb_unmap (GtkWidget *widget)
{
  OsThumb *thumb;
  OsThumbPrivate *priv;

  thumb = OS_THUMB (widget);
  priv = thumb->priv;

  priv->button_press_event = FALSE;
  priv->motion_notify_event = FALSE;

  priv->use_tolerance = FALSE;

  if (priv->grabbed_widget != NULL)
    gtk_grab_add (priv->grabbed_widget);

  GTK_WIDGET_CLASS (os_thumb_parent_class)->unmap (widget);
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
  OsThumb *thumb;
  OsThumbPrivate *priv;

  thumb = OS_THUMB (object);
  priv = thumb->priv;

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
  OsThumb *thumb;
  OsThumbPrivate *priv;

  thumb = OS_THUMB (object);
  priv = thumb->priv;

  switch (prop_id)
    {
      case PROP_ORIENTATION: {
        priv->orientation = g_value_get_enum (value);
        if (priv->orientation == GTK_ORIENTATION_VERTICAL)
          {
            gtk_window_resize (GTK_WINDOW (object), DEFAULT_THUMB_WIDTH,
                               DEFAULT_THUMB_HEIGHT);
          }
        else
          {
            gtk_window_resize (GTK_WINDOW (object), DEFAULT_THUMB_HEIGHT,
                               DEFAULT_THUMB_WIDTH);
          }
        break;
      }

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

/**
 * os_thumb_set_detached:
 * @thumb: a #OsThumb
 * @detached: a gboolean
 *
 * Sets the thumb to be detached.
 **/
void
os_thumb_set_detached (OsThumb *thumb,
                       gboolean detached)
{
  OsThumbPrivate *priv;

  g_return_if_fail (OS_THUMB (thumb));

  priv = thumb->priv;

  if (priv->detached != detached)
  {
    priv->detached = detached;
    gtk_widget_queue_draw (GTK_WIDGET (thumb));
  }
}
