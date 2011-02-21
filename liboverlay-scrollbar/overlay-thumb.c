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

#include "overlay-thumb.h"
#include "overlay-scrollbar-cairo-support.h"
#include "overlay-scrollbar-support.h"

#define OVERLAY_THUMB_WIDTH 15 /* width/height of the OverlayThumb, in pixels */
#define OVERLAY_THUMB_HEIGHT 80 /* height/width of the OverlayThumb, in pixels */

#define OVERLAY_THUMB_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), OS_TYPE_OVERLAY_THUMB, OverlayThumbPrivate))

G_DEFINE_TYPE (OverlayThumb, overlay_thumb, GTK_TYPE_WINDOW);

typedef struct _OverlayThumbPrivate OverlayThumbPrivate;

struct _OverlayThumbPrivate
{
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

/* WIDGET CLASS FUNCTIONS */
static gboolean overlay_thumb_button_press_event (GtkWidget      *widget,
                                                  GdkEventButton *event);

static gboolean overlay_thumb_button_release_event (GtkWidget      *widget,
                                                    GdkEventButton *event);

static void overlay_thumb_composited_changed (GtkWidget *widget);

static gboolean overlay_thumb_enter_notify_event (GtkWidget        *widget,
                                                  GdkEventCrossing *event);

static gboolean overlay_thumb_expose (GtkWidget      *widget,
                                      GdkEventExpose *event);

static gboolean overlay_thumb_leave_notify_event (GtkWidget        *widget,
                                                  GdkEventCrossing *event);

static gboolean overlay_thumb_motion_notify_event (GtkWidget      *widget,
                                                   GdkEventMotion *event);

static void overlay_thumb_screen_changed (GtkWidget *widget,
                                          GdkScreen *old_screen);

/* GOBJECT CLASS FUNCTIONS */
static GObject* overlay_thumb_constructor (GType                  type,
                                           guint                  n_construct_properties,
                                           GObjectConstructParam *construct_properties);

static void overlay_thumb_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec);

static void overlay_thumb_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec);

/* CLASS FUNCTIONS */
/**
 * overlay_thumb_class_init:
 * class init function
 **/
static void
overlay_thumb_class_init (OverlayThumbClass *class)
{
  DEBUG
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS (class);
  widget_class = GTK_WIDGET_CLASS (class);

  widget_class->button_press_event   = overlay_thumb_button_press_event;
  widget_class->button_release_event = overlay_thumb_button_release_event;
  widget_class->composited_changed   = overlay_thumb_composited_changed;
  widget_class->enter_notify_event   = overlay_thumb_enter_notify_event;
  widget_class->expose_event         = overlay_thumb_expose;
  widget_class->leave_notify_event   = overlay_thumb_leave_notify_event;
  widget_class->motion_notify_event  = overlay_thumb_motion_notify_event;
  widget_class->screen_changed       = overlay_thumb_screen_changed;

  gobject_class->constructor  = overlay_thumb_constructor;
  gobject_class->get_property = overlay_thumb_get_property;
  gobject_class->set_property = overlay_thumb_set_property;

  g_object_class_install_property (gobject_class,
                                   PROP_ORIENTATION,
                                   g_param_spec_enum ("orientation",
                                                      "Orientation",
                                                      "GtkOrientation of the OverlayThumb",
                                                      GTK_TYPE_ORIENTATION,
                                                      GTK_ORIENTATION_VERTICAL,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_STATIC_NAME |
                                                      G_PARAM_STATIC_NICK |
                                                      G_PARAM_STATIC_BLURB));

  g_type_class_add_private (gobject_class, sizeof (OverlayThumbPrivate));
}

/**
 * overlay_thumb_init:
 * init function
 **/
static void
overlay_thumb_init (OverlayThumb *thumb)
{
  DEBUG
  OverlayThumbPrivate *priv;

  priv = OVERLAY_THUMB_GET_PRIVATE (thumb);

  priv->orientation = GTK_ORIENTATION_VERTICAL;
  priv->can_hide = TRUE;
  priv->can_rgba = FALSE;

  gtk_window_set_default_size (GTK_WINDOW (thumb),
                               OVERLAY_THUMB_WIDTH,
                               OVERLAY_THUMB_HEIGHT);
  gtk_window_set_skip_pager_hint (GTK_WINDOW (thumb), TRUE);
  gtk_window_set_skip_taskbar_hint (GTK_WINDOW (thumb), TRUE);
  gtk_window_set_has_resize_grip (GTK_WINDOW (thumb), FALSE);
  gtk_window_set_decorated (GTK_WINDOW (thumb), FALSE);
  gtk_window_set_focus_on_map (GTK_WINDOW (thumb), FALSE);
  gtk_window_set_accept_focus (GTK_WINDOW (thumb), FALSE);
  gtk_widget_set_app_paintable (GTK_WIDGET (thumb), TRUE);
  gtk_widget_add_events (GTK_WIDGET (thumb), GDK_BUTTON_PRESS_MASK |
                                             GDK_BUTTON_RELEASE_MASK |
                                             GDK_POINTER_MOTION_MASK);

  overlay_thumb_screen_changed (GTK_WIDGET (thumb), NULL);
  overlay_thumb_composited_changed (GTK_WIDGET (thumb));
}

/* WIDGET CLASS FUNCTIONS */
/**
 * overlay_thumb_button_press_event:
 * override class function
 **/
static gboolean
overlay_thumb_button_press_event (GtkWidget      *widget,
                                  GdkEventButton *event)
{
  DEBUG
  if (event->type == GDK_BUTTON_PRESS)
    {
      if (event->button == 1)
        {
          OverlayThumbPrivate *priv;

          priv = OVERLAY_THUMB_GET_PRIVATE (OVERLAY_THUMB (widget));

          priv->pointer_x = event->x;
          priv->pointer_y = event->y;

          priv->button_press_event = TRUE;
          priv->motion_notify_event = FALSE;
        }
    }

  return FALSE;
}

/**
 * overlay_thumb_button_release_event:
 * override class function
 **/
static gboolean
overlay_thumb_button_release_event (GtkWidget      *widget,
                                    GdkEventButton *event)
{
  DEBUG
  if (event->type == GDK_BUTTON_RELEASE)
    {
      if (event->button == 1)
        {
          OverlayThumbPrivate *priv;

          priv = OVERLAY_THUMB_GET_PRIVATE (OVERLAY_THUMB (widget));

          priv->button_press_event = FALSE;
          priv->motion_notify_event = FALSE;
        }
    }

  return FALSE;
}

/**
 * overlay_thumb_composited_changed:
 * override class function
 **/
static void
overlay_thumb_composited_changed (GtkWidget *widget)
{
  DEBUG
  GdkScreen *screen;

  screen = gtk_widget_get_screen (widget);

  if (gdk_screen_is_composited (screen))
    {
      OverlayThumbPrivate *priv;

      priv = OVERLAY_THUMB_GET_PRIVATE (OVERLAY_THUMB (widget));

      priv->can_rgba = TRUE;
    }

  gtk_widget_queue_draw (widget);
}

/**
 * overlay_thumb_enter_notify_event:
 * override class function
 **/
static gboolean
overlay_thumb_enter_notify_event (GtkWidget        *widget,
                                  GdkEventCrossing *event)
{
  DEBUG
  OverlayThumbPrivate *priv;

  priv = OVERLAY_THUMB_GET_PRIVATE (OVERLAY_THUMB (widget));

  priv->enter_notify_event = TRUE;
  priv->can_hide = FALSE;

  return TRUE;
}

/**
 * overlay_thumb_expose:
 * override class function
 **/
static gboolean
overlay_thumb_expose (GtkWidget      *widget,
                      GdkEventExpose *event)
{
  DEBUG
  GtkAllocation allocation;
  GtkStateType state_type_down, state_type_up;
  OverlayThumbPrivate *priv;
  cairo_pattern_t *pat;
  cairo_t *cr;
  gint x, y, width, height;
  gint radius;

  priv = OVERLAY_THUMB_GET_PRIVATE (OVERLAY_THUMB (widget));

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

/**
 * overlay_thumb_leave_notify_event:
 * override class function
 **/
static gboolean
overlay_thumb_leave_notify_event (GtkWidget        *widget,
                                  GdkEventCrossing *event)
{
  DEBUG
  OverlayThumbPrivate *priv;

  priv = OVERLAY_THUMB_GET_PRIVATE (OVERLAY_THUMB (widget));

  if (!priv->button_press_event)
    priv->can_hide = TRUE;

/*  g_timeout_add (TIMEOUT_HIDE, overlay_thumb_hide, widget);*/

  return TRUE;
}

/**
 * overlay_thumb_motion_notify_event:
 * override class function
 **/
static gboolean
overlay_thumb_motion_notify_event (GtkWidget      *widget,
                                   GdkEventMotion *event)
{
  DEBUG
  OverlayThumbPrivate *priv;

  priv = OVERLAY_THUMB_GET_PRIVATE (OVERLAY_THUMB (widget));

  if (priv->button_press_event)
    {
      if (!priv->motion_notify_event)
        gtk_widget_queue_draw (widget);

      priv->motion_notify_event = TRUE;
    }

  return TRUE;
}

/**
 * overlay_thumb_screen_changed:
 * override class function
 **/
static void
overlay_thumb_screen_changed (GtkWidget *widget,
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

/* GOBJECT CLASS FUNCTIONS */
/**
 * overlay_thumb_constructor:
 * override class function
 **/
static GObject*
overlay_thumb_constructor (GType                  type,
                           guint                  n_construct_properties,
                           GObjectConstructParam *construct_properties)
{
  DEBUG
  GObject *object;

  object = G_OBJECT_CLASS (overlay_thumb_parent_class)->constructor (type,
                                                                     n_construct_properties,
                                                                     construct_properties);

  g_object_set (object, "type", GTK_WINDOW_POPUP, NULL);

  return object;
}

/**
 * overlay_thumb_get_property:
 * override class function
 **/
static void
overlay_thumb_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  DEBUG
  OverlayThumbPrivate *priv;

  priv = OVERLAY_THUMB_GET_PRIVATE (OVERLAY_THUMB (object));

  switch (prop_id)
    {
      case PROP_ORIENTATION:
        g_value_set_enum (value, priv->orientation);
        break;
    }
}

/**
 * overlay_thumb_set_property:
 * override class function
 **/
static void
overlay_thumb_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  DEBUG
  OverlayThumbPrivate *priv;

  priv = OVERLAY_THUMB_GET_PRIVATE (OVERLAY_THUMB (object));

  switch (prop_id)
    {
      case PROP_ORIENTATION:
        priv->orientation = g_value_get_enum (value);
        break;
    }
}

/* PUBLIC FUNCTIONS */
/**
 * overlay_thumb_new:
 *
 * Creates a new OverlayThumb.
 *
 * Returns: the new OverlayThumb as a #GtkWidget
 */
GtkWidget*
overlay_thumb_new (GtkOrientation orientation)
{
  DEBUG
  return g_object_new (OS_TYPE_OVERLAY_THUMB, "orientation", orientation, NULL);
}
