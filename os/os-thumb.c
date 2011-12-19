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

#include <math.h>
#include <stdlib.h>

/* Duration of the fade-out. */
#define DURATION_FADE_OUT 2000

/* Timeout before the fade-out. */
#define TIMEOUT_FADE_OUT 250

/* Thumb radius in pixels (higher values are automatically clamped). */
#define THUMB_RADIUS 3

/* Number of tolerance pixels, before hiding the thumb. */
#define TOLERANCE_FADE 3

#ifndef USE_GTK3
typedef struct {
  gdouble red;
  gdouble green;
  gdouble blue;
  gdouble alpha;
} GdkRGBA;
#endif

struct _OsThumbPrivate {
  GtkOrientation orientation;
  GtkWidget *grabbed_widget;
  OsAnimation *animation;
  OsCoordinate pointer;
  OsCoordinate pointer_root;
  OsEventFlags event;
  gboolean rgba;
  gboolean detached;
  gboolean tolerance;
  guint32 source_id;
};

enum {
  PROP_0,
  PROP_ORIENTATION,
  LAST_ARG
};

static gboolean os_thumb_button_press_event (GtkWidget *widget, GdkEventButton *event);
static gboolean os_thumb_button_release_event (GtkWidget *widget, GdkEventButton *event);
static void os_thumb_composited_changed (GtkWidget *widget);
#ifdef USE_GTK3
static gboolean os_thumb_draw (GtkWidget *widget, cairo_t *cr);
#else
static gboolean os_thumb_expose (GtkWidget *widget, GdkEventExpose *event);
#endif
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

/* Callback called by the fade-out animation. */
static void
fade_out_cb (gfloat   weight,
             gpointer user_data)
{
  OsThumb *thumb;

  thumb = OS_THUMB (user_data);

  if (weight < 1.0f)
    gtk_window_set_opacity (GTK_WINDOW (thumb), fabs (weight - 1.0f));
  else
    gtk_widget_hide (GTK_WIDGET (thumb));
}

/* Stop function called by the fade-out animation. */
static void
fade_out_stop_cb (gpointer user_data)
{
  OsThumb *thumb;

  thumb = OS_THUMB (user_data);

  gtk_window_set_opacity (GTK_WINDOW (thumb), 1.0f);
}

/* Timeout before starting the fade-out animation. */
static gboolean
timeout_fade_out_cb (gpointer user_data)
{
  OsThumb *thumb;
  OsThumbPrivate *priv;

  thumb = OS_THUMB (user_data);

  priv = thumb->priv;

  os_animation_start (priv->animation);
  priv->source_id = 0;

  return FALSE;
}

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
#ifdef USE_GTK3
  widget_class->draw                 = os_thumb_draw;
#else
  widget_class->expose_event         = os_thumb_expose;
#endif
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

  priv->event = OS_EVENT_NONE;

  priv->pointer.x = 0;
  priv->pointer.y = 0;
  priv->pointer_root.x = 0;
  priv->pointer_root.y = 0;

  priv->rgba = FALSE;
  priv->detached = FALSE;
  priv->tolerance = FALSE;

  priv->source_id = 0;
  priv->animation = os_animation_new (RATE_ANIMATION, DURATION_FADE_OUT,
                                      fade_out_cb, NULL, thumb);

  gtk_window_set_skip_pager_hint (GTK_WINDOW (thumb), TRUE);
  gtk_window_set_skip_taskbar_hint (GTK_WINDOW (thumb), TRUE);
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
  os_animation_stop (priv->animation, fade_out_stop_cb);

  if (event->type == GDK_BUTTON_PRESS)
    {
      if (event->button == 1 ||
          event->button == 2)
        {
          gtk_grab_add (widget);

          priv->pointer.x = event->x;
          priv->pointer.y = event->y;
          priv->pointer_root.x = event->x_root;
          priv->pointer_root.y = event->y_root;

          priv->event |= OS_EVENT_BUTTON_PRESS;
          priv->event &= ~(OS_EVENT_MOTION_NOTIFY);

          priv->tolerance = TRUE;

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
      if (event->button == 1 ||
          event->button == 2)
        {
          gtk_grab_remove (widget);

          priv->event &= ~(OS_EVENT_BUTTON_PRESS | OS_EVENT_MOTION_NOTIFY);

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

  priv->rgba = FALSE;

  if (gdk_screen_is_composited (gtk_widget_get_screen (widget)))
    {
      GdkVisual *visual;
      guint32 red_mask;
      guint32 green_mask;
      guint32 blue_mask;

      visual = gtk_widget_get_visual (widget);

      gdk_visual_get_red_pixel_details (visual, &red_mask, NULL, NULL);
      gdk_visual_get_green_pixel_details (visual, &green_mask, NULL, NULL);
      gdk_visual_get_blue_pixel_details (visual, &blue_mask, NULL, NULL);

      if (gdk_visual_get_depth (visual) == 32 && (red_mask   == 0xff0000 &&
                                                  green_mask == 0x00ff00 &&
                                                  blue_mask  == 0x0000ff))   
        priv->rgba = TRUE;
    }

  gtk_widget_queue_draw (widget);
}

/* Simplified wrapper of cairo_pattern_add_color_stop_rgba. */
static void
pattern_add_gdk_rgba_stop (cairo_pattern_t *pat,
                           gdouble          stop,
                           const GdkRGBA   *color,
                           gdouble          alpha)
{
  cairo_pattern_add_color_stop_rgba (pat, stop, color->red, color->green, color->blue, alpha);
}

/* Simplified wrapper of cairo_set_source_rgba. */
static void
set_source_gdk_rgba (cairo_t       *cr,
                     const GdkRGBA *color,
                     gdouble        alpha)
{
  cairo_set_source_rgba (cr, color->red, color->green, color->blue, alpha);
}

/* Draw an arrow using cairo. */
static void
draw_arrow (cairo_t       *cr,
            const GdkRGBA *color,
            gdouble        x,
            gdouble        y,
            gdouble        width,
            gdouble        height)
{
  cairo_save (cr);

  cairo_translate (cr, x, y);
  cairo_move_to (cr, -width / 2, -height / 2);
  cairo_line_to (cr, 0, height / 2);
  cairo_line_to (cr, width / 2, -height / 2);
  cairo_close_path (cr);

  set_source_gdk_rgba (cr, color, 0.75);
  cairo_fill_preserve (cr);

  set_source_gdk_rgba (cr, color, 1.0);
  cairo_stroke (cr);

  cairo_restore (cr);
}

/* Draw a grip using cairo. */
static void
draw_grip (cairo_t       *cr,
           gdouble        x,
           gdouble        y,
           gint           nx,
           gint           ny)
{
  gint lx, ly;

  for (ly = 0; ly < ny; ly++)
    {
      for (lx = 0; lx < nx; lx++)
        {
          gint sx = lx * 3;
          gint sy = ly * 3;

          cairo_rectangle (cr, x + sx, y + sy, 1, 1);
        }
    }
}

/* Draw a rounded rectangle using cairo. */
static void
draw_round_rect (cairo_t *cr,
                 gdouble  x,
                 gdouble  y,
                 gdouble  width,
                 gdouble  height,
                 gdouble  radius)
{
  radius = MIN (radius, MIN (width / 2.0, height / 2.0));

  if (radius < 1)
    {
      cairo_rectangle (cr, x, y, width, height);
      return;
    }

  cairo_move_to (cr, x + radius, y);

  cairo_arc (cr, x + width - radius, y + radius, radius, G_PI * 1.5, G_PI * 2);
  cairo_arc (cr, x + width - radius, y + height - radius, radius, 0, G_PI * 0.5);
  cairo_arc (cr, x + radius, y + height - radius, radius, G_PI * 0.5, G_PI);
  cairo_arc (cr, x + radius, y + radius, radius, G_PI, G_PI * 1.5);
}

/* Convert RGB to HLS. */
static void
rgb_to_hls (gdouble *r,
            gdouble *g,
            gdouble *b)
{
  gdouble min;
  gdouble max;
  gdouble red;
  gdouble green;
  gdouble blue;
  gdouble h, l, s;
  gdouble delta;

  h = 0;

  red = *r;
  green = *g;
  blue = *b;

  if (red > green)
    {
      if (red > blue)
        max = red;
      else
        max = blue;

      if (green < blue)
        min = green;
      else
        min = blue;
    }
  else
    {
      if (green > blue)
        max = green;
      else
        max = blue;

      if (red < blue)
        min = red;
      else
        min = blue;
    }

  l = (max + min) / 2;
  if (fabs (max - min) < 0.0001)
    {
      h = 0;
      s = 0;
    }
  else
    {
      if (l <= 0.5)
        s = (max - min) / (max + min);
      else
        s = (max - min) / (2 - max - min);

      delta = max - min;
      if (red == max)
        h = (green - blue) / delta;
      else if (green == max)
        h = 2 + (blue - red) / delta;
      else if (blue == max)
        h = 4 + (red - green) / delta;

      h *= 60;
      if (h < 0.0)
        h += 360;
    }

  *r = h;
  *g = l;
  *b = s;
}

/* Convert HLS to RGB. */
static void
hls_to_rgb (gdouble *h,
            gdouble *l,
            gdouble *s)
{
  gdouble hue;
  gdouble lightness;
  gdouble saturation;
  gdouble m1, m2;
  gdouble r, g, b;

  lightness = *l;
  saturation = *s;

  if (lightness <= 0.5)
    m2 = lightness * (1 + saturation);
  else
    m2 = lightness + saturation - lightness * saturation;

  m1 = 2 * lightness - m2;

  if (saturation == 0)
    {
      *h = lightness;
      *l = lightness;
      *s = lightness;
    }
  else
    {
      hue = *h + 120;
      while (hue > 360)
        hue -= 360;
      while (hue < 0)
        hue += 360;

      if (hue < 60)
        r = m1 + (m2 - m1) * hue / 60;
      else if (hue < 180)
        r = m2;
      else if (hue < 240)
        r = m1 + (m2 - m1) * (240 - hue) / 60;
      else
        r = m1;

      hue = *h;
      while (hue > 360)
        hue -= 360;
      while (hue < 0)
        hue += 360;

      if (hue < 60)
        g = m1 + (m2 - m1) * hue / 60;
      else if (hue < 180)
        g = m2;
      else if (hue < 240)
        g = m1 + (m2 - m1) * (240 - hue) / 60;
      else
        g = m1;

      hue = *h-120;
      while (hue > 360)
        hue -= 360;
      while (hue < 0)
        hue += 360;

      if (hue < 60)
        b = m1 + (m2 - m1) * hue / 60;
      else if (hue < 180)
        b = m2;
      else if (hue < 240)
        b = m1 + (m2 - m1) * (240 - hue) / 60;
      else
        b = m1;

      *h = r;
      *l = g;
      *s = b;
    }
}

/* Shade a GdkRGBA color. */
static void
shade_gdk_rgba (const GdkRGBA *a,
                gfloat         k,
                GdkRGBA       *b)
{
  gdouble red;
  gdouble green;
  gdouble blue;

  red   = a->red;
  green = a->green;
  blue  = a->blue;

  if (k == 1.0)
    {
      b->red = red;
      b->green = green;
      b->blue = blue;
      return;
    }

  rgb_to_hls (&red, &green, &blue);

  green *= k;
  if (green > 1.0)
    green = 1.0;
  else if (green < 0.0)
    green = 0.0;

  blue *= k;
  if (blue > 1.0)
    blue = 1.0;
  else if (blue < 0.0)
    blue = 0.0;

  hls_to_rgb (&red, &green, &blue);

  b->red = red;
  b->green = green;
  b->blue = blue;
  b->alpha = a->alpha;
}

#ifndef USE_GTK3
/* Convert a GdkColor to GdkRGBA. */
static void
convert_gdk_color_to_gdk_rgba (GdkColor *color,
                               GdkRGBA  *rgba)
{  
  rgba->red = (gdouble) color->red / (gdouble) 65535;
  rgba->green = (gdouble) color->green / (gdouble) 65535;
  rgba->blue = (gdouble) color->blue / (gdouble) 65535;

  rgba->alpha = 1.0;
}
#endif

enum {
  ACTION_NORMAL,
  ACTION_DRAG,
  ACTION_PAGE_UP,
  ACTION_PAGE_DOWN
};

static gboolean
#ifdef USE_GTK3
os_thumb_draw (GtkWidget *widget,
               cairo_t   *cr)
{
  GtkStyleContext *style_context;
#else
os_thumb_expose (GtkWidget      *widget,
                 GdkEventExpose *event)
{
  GtkAllocation allocation;
  cairo_t *cr;
  GtkStyle *style;
#endif
  GdkRGBA bg, bg_active, bg_selected;
  GdkRGBA bg_arrow_up, bg_arrow_down;
  GdkRGBA bg_shadow, bg_dark_line, bg_bright_line;
  GdkRGBA arrow_color;
  OsThumb *thumb;
  OsThumbPrivate *priv;
  cairo_pattern_t *pat;
  gint width, height;
  gint radius;
  gint action;

  thumb = OS_THUMB (widget);
  priv = thumb->priv;

  radius = priv->rgba ? THUMB_RADIUS : 0;

#ifdef USE_GTK3
  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  style_context = gtk_widget_get_style_context (widget);

  gtk_style_context_get_background_color (style_context, gtk_widget_get_state_flags (widget), &bg);
  gtk_style_context_get_background_color (style_context, GTK_STATE_FLAG_ACTIVE, &bg_active);
  gtk_style_context_get_background_color (style_context, GTK_STATE_FLAG_SELECTED, &bg_selected);
  gtk_style_context_get_color (style_context, gtk_widget_get_state_flags (widget), &arrow_color);
#else
  gtk_widget_get_allocation (widget, &allocation);
 
  width = allocation.width;
  height = allocation.height;

  style = gtk_widget_get_style (widget);

  convert_gdk_color_to_gdk_rgba (&style->bg[gtk_widget_get_state (widget)], &bg);
  convert_gdk_color_to_gdk_rgba (&style->bg[GTK_STATE_ACTIVE], &bg_active);
  convert_gdk_color_to_gdk_rgba (&style->bg[GTK_STATE_SELECTED], &bg_selected);
  convert_gdk_color_to_gdk_rgba (&style->fg[gtk_widget_get_state (widget)], &arrow_color);

  cr = gdk_cairo_create (gtk_widget_get_window (widget));
#endif

  cairo_save (cr);

  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr);

  cairo_save (cr);
  cairo_translate (cr, 0.5, 0.5);
  width--;
  height--;

  cairo_set_line_width (cr, 1.0);
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

  /* Type of action. */
  action = ACTION_NORMAL;
  if (priv->event & OS_EVENT_BUTTON_PRESS)
    {
      if (priv->event & OS_EVENT_MOTION_NOTIFY)
        action = ACTION_DRAG;
      else if ((priv->orientation == GTK_ORIENTATION_VERTICAL && (priv->pointer.y < height / 2)) ||
               (priv->orientation == GTK_ORIENTATION_HORIZONTAL && (priv->pointer.x < width / 2)))
        action = ACTION_PAGE_UP;
      else
        action = ACTION_PAGE_DOWN;
    }

  /* Background. */
  draw_round_rect (cr, 0, 0, width, height, radius);

  set_source_gdk_rgba (cr, &bg, 1.0);
  cairo_fill_preserve (cr);

  /* Background pattern from top to bottom. */
  shade_gdk_rgba (&bg, 0.86, &bg_arrow_up);
  shade_gdk_rgba (&bg, 1.1, &bg_arrow_down);

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    pat = cairo_pattern_create_linear (0, 0, 0, height);
  else
    pat = cairo_pattern_create_linear (0, 0, width, 0);

  pattern_add_gdk_rgba_stop (pat, 0.0, &bg_arrow_up, 0.8);
  pattern_add_gdk_rgba_stop (pat, 1.0, &bg_arrow_down, 0.8);

  cairo_set_source (cr, pat);
  cairo_pattern_destroy (pat);

  if (action == ACTION_DRAG)
    {
      cairo_fill_preserve (cr);
      set_source_gdk_rgba (cr, &bg_arrow_up, 0.3);
      cairo_fill (cr);
    }
  else
    cairo_fill (cr);

  /* Page up or down pressed buttons. */
  if (action == ACTION_PAGE_UP ||
      action == ACTION_PAGE_DOWN)
    {
      if (priv->orientation == GTK_ORIENTATION_VERTICAL)
        {
          if (action == ACTION_PAGE_UP)
            cairo_rectangle (cr, 0, 0, width, height / 2);
          else
            cairo_rectangle (cr, 0, height / 2, width, height / 2);
        }
      else
        {
          if (action == ACTION_PAGE_UP)
            cairo_rectangle (cr, 0, 0, width / 2, height);
          else
            cairo_rectangle (cr, width / 2, 0, width / 2, height);
        }

      set_source_gdk_rgba (cr, &bg_arrow_up, 0.3);
      cairo_fill (cr);
    }

  /* 2px fat border around the thumb. */
  cairo_save (cr);

  cairo_set_line_width (cr, 2.0);
  draw_round_rect (cr, 0.5, 0.5, width - 1, height - 1, radius - 1);
  if (!priv->detached)
    set_source_gdk_rgba (cr, &bg_selected, 1.0);
  else
    set_source_gdk_rgba (cr, &bg_active, 1.0);
  cairo_stroke (cr);

  cairo_restore (cr);

  /* 1px subtle shadow around the background. */
  shade_gdk_rgba (&bg, 0.2, &bg_shadow);

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    pat = cairo_pattern_create_linear (0, 0, 0, height);
  else
    pat = cairo_pattern_create_linear (0, 0, width, 0);

  pattern_add_gdk_rgba_stop (pat, 0.5, &bg_shadow, 0.06);
  switch (action)
  {
    default:
    case ACTION_NORMAL:
      pattern_add_gdk_rgba_stop (pat, 0.0, &bg_shadow, 0.3);
      pattern_add_gdk_rgba_stop (pat, 1.0, &bg_shadow, 0.3);
      break;
    case ACTION_DRAG:
      pattern_add_gdk_rgba_stop (pat, 0.0, &bg_shadow, 0.24);
      pattern_add_gdk_rgba_stop (pat, 1.0, &bg_shadow, 0.24);
      break;
    case ACTION_PAGE_UP:
      pattern_add_gdk_rgba_stop (pat, 0.0, &bg_shadow, 0.14);
      pattern_add_gdk_rgba_stop (pat, 1.0, &bg_shadow, 0.3);
      break;
    case ACTION_PAGE_DOWN:
      pattern_add_gdk_rgba_stop (pat, 0.0, &bg_shadow, 0.3);
      pattern_add_gdk_rgba_stop (pat, 1.0, &bg_shadow, 0.14);
      break;
  }

  cairo_set_source (cr, pat);
  cairo_pattern_destroy (pat);

  draw_round_rect (cr, 1, 1, width - 2, height - 2, radius);
  cairo_stroke (cr);

  /* 1px frame around the background. */
  shade_gdk_rgba (&bg, 0.6, &bg_dark_line);
  shade_gdk_rgba (&bg, 1.4, &bg_bright_line);

  draw_round_rect (cr, 2, 2, width - 4, height - 4, radius - 1);
  set_source_gdk_rgba (cr, &bg_bright_line, 0.6);
  cairo_stroke (cr);

  /* Only draw the grip when the thumb is at full height. */
  if ((priv->orientation == GTK_ORIENTATION_VERTICAL && height == THUMB_HEIGHT - 1) ||
      (priv->orientation == GTK_ORIENTATION_HORIZONTAL && width == THUMB_HEIGHT - 1) )
    {
      if (priv->orientation == GTK_ORIENTATION_VERTICAL)
        pat = cairo_pattern_create_linear (0, 0, 0, height);
      else
        pat = cairo_pattern_create_linear (0, 0, width, 0);

      pattern_add_gdk_rgba_stop (pat, 0.0, &bg_dark_line, 0.0);
      pattern_add_gdk_rgba_stop (pat, 0.49, &bg_dark_line, 0.36);
      pattern_add_gdk_rgba_stop (pat, 0.49, &bg_dark_line, 0.36);
      pattern_add_gdk_rgba_stop (pat, 1.0, &bg_dark_line, 0.0);
      cairo_set_source (cr, pat);
      cairo_pattern_destroy (pat);

      /* Grip. */
      if (priv->orientation == GTK_ORIENTATION_VERTICAL)
        {
          /* Page UP. */
          draw_grip (cr, width / 2 - 6.5, 13.5, 5, 6);

          /* Page DOWN. */
          draw_grip (cr, width / 2 - 6.5, height / 2 + 3.5, 5, 6);
        }
      else
        {
          /* Page UP. */
          draw_grip (cr, 16.5, height / 2 - 6.5, 5, 6);

          /* Page DOWN. */
          draw_grip (cr, width / 2 + 3.5, height / 2 - 6.5, 5, 6);
        }

      cairo_fill (cr);
    }

  /* Separators between the two steppers. */
  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      cairo_move_to (cr, 1.5, height / 2);
      cairo_line_to (cr, width - 1.5, height / 2);
      set_source_gdk_rgba (cr, &bg_dark_line, 0.36);
      cairo_stroke (cr);

      cairo_move_to (cr, 1.5, 1 + height / 2);
      cairo_line_to (cr, width - 1.5, 1 + height / 2);
      set_source_gdk_rgba (cr, &bg_bright_line, 0.5);
      cairo_stroke (cr);
    }
  else
    {
      cairo_move_to (cr, width / 2, 1.5);
      cairo_line_to (cr, width / 2, height - 1.5);
      set_source_gdk_rgba (cr, &bg_dark_line, 0.36);
      cairo_stroke (cr);

      cairo_move_to (cr, 1 + width / 2, 1.5);
      cairo_line_to (cr, 1 + width / 2, height - 1.5);
      set_source_gdk_rgba (cr, &bg_bright_line, 0.5);
      cairo_stroke (cr);
    }

  /* Arrows. */
  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      /* Direction UP. */
      cairo_save (cr);
      cairo_translate (cr, width / 2 + 0.5, 8.5);
      cairo_rotate (cr, G_PI);  
      draw_arrow (cr, &arrow_color, 0.5, 0, 5, 3);
      cairo_restore (cr);

      /* Direction DOWN. */
      cairo_save (cr);
      cairo_translate (cr, width / 2 + 0.5, height - 8.5);
      cairo_rotate (cr, 0);
      draw_arrow (cr, &arrow_color, -0.5, 0, 5, 3);
      cairo_restore (cr);
    }
  else
    {
      /* Direction LEFT. */
      cairo_save (cr);
      cairo_translate (cr, 8.5, height / 2 + 0.5);
      cairo_rotate (cr, G_PI * 0.5);  
      draw_arrow (cr, &arrow_color, -0.5, 0, 5, 3);
      cairo_restore (cr);

      /* Direction RIGHT. */
      cairo_save (cr);
      cairo_translate (cr, width - 8.5, height / 2 + 0.5);
      cairo_rotate (cr, G_PI * 1.5);
      draw_arrow (cr, &arrow_color, 0.5, 0, 5, 3);
      cairo_restore (cr);
    }

  cairo_restore (cr);

  cairo_restore (cr);

#ifndef USE_GTK3
  cairo_destroy (cr);
#endif

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
   * Stop it only if OS_EVENT_BUTTON_PRESS is not set. */
  if (!(priv->event & OS_EVENT_BUTTON_PRESS))
    {
      if (priv->source_id != 0)
        {
          g_source_remove (priv->source_id);
          priv->source_id = 0;
        }

      os_animation_stop (priv->animation, NULL);
    }

  priv->tolerance = FALSE;

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
  os_animation_stop (priv->animation, fade_out_stop_cb);

  /* If you're not dragging, and you're outside
   * the tolerance pixels, enable the fade-out.
   * OS_EVENT_MOTION_NOTIFY is set only on dragging,
   * see code few lines below. */
  if (!(priv->event & OS_EVENT_MOTION_NOTIFY))
  {
    if (!priv->tolerance ||
        (abs (priv->pointer.x - event->x) > TOLERANCE_FADE ||
         abs (priv->pointer.y - event->y) > TOLERANCE_FADE))
      {
        priv->tolerance = FALSE;
        priv->source_id = g_timeout_add (TIMEOUT_FADE_OUT,
                                         timeout_fade_out_cb,
                                         thumb);
      }
  }

  if (priv->event & OS_EVENT_BUTTON_PRESS &&
      !(priv->event & OS_EVENT_MOTION_NOTIFY))
    {
      if (abs (priv->pointer_root.x - event->x_root) <= TOLERANCE_MOTION &&
          abs (priv->pointer_root.y - event->y_root) <= TOLERANCE_MOTION)
        return FALSE;

      priv->event |= OS_EVENT_MOTION_NOTIFY;

      gtk_widget_queue_draw (widget);
    }

  return FALSE;
}


static void
os_thumb_screen_changed (GtkWidget *widget,
                         GdkScreen *old_screen)
{
#ifdef USE_GTK3
  GdkScreen *screen;
  GdkVisual *visual;

  screen = gtk_widget_get_screen (widget);
  visual = gdk_screen_get_rgba_visual (screen);

 if (visual)
   gtk_widget_set_visual (widget, visual);
#else
  GdkScreen *screen;
  GdkColormap *colormap;

  screen = gtk_widget_get_screen (widget);
  colormap = gdk_screen_get_rgba_colormap (screen);

  if (colormap)
    gtk_widget_set_colormap (widget, colormap);
#endif
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

  /* If started, stop the fade-out. */
  os_animation_stop (priv->animation, fade_out_stop_cb);

  if (priv->event & OS_EVENT_MOTION_NOTIFY)
    {
      priv->event &= ~(OS_EVENT_MOTION_NOTIFY);

      gtk_widget_queue_draw (widget);
    }

  return FALSE;
}

static void
os_thumb_unmap (GtkWidget *widget)
{
  OsThumb *thumb;
  OsThumbPrivate *priv;

  thumb = OS_THUMB (widget);
  priv = thumb->priv;

  priv->event = OS_EVENT_NONE;

  priv->tolerance = FALSE;

  if (priv->grabbed_widget != NULL && gtk_widget_get_mapped (priv->grabbed_widget))
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
    case PROP_ORIENTATION:
      {
        priv->orientation = g_value_get_enum (value);
        if (priv->orientation == GTK_ORIENTATION_VERTICAL)
          gtk_window_resize (GTK_WINDOW (object), THUMB_WIDTH, THUMB_HEIGHT);
        else
          gtk_window_resize (GTK_WINDOW (object), THUMB_HEIGHT, THUMB_WIDTH);
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
 **/
GtkWidget*
os_thumb_new (GtkOrientation orientation)
{
  return g_object_new (OS_TYPE_THUMB, "orientation", orientation, NULL);
}

/**
 * os_thumb_resize:
 * @thumb: a #OsThumb
 * @width: width in pixels
 * @height: height in pixels
 *
 * Resize the thumb.
 **/
void
os_thumb_resize (OsThumb *thumb,
                 gint     width,
                 gint     height)
{
  g_return_if_fail (OS_IS_THUMB (thumb));

  gtk_window_resize (GTK_WINDOW (thumb), width, height);
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

  g_return_if_fail (OS_IS_THUMB (thumb));

  priv = thumb->priv;

  if (priv->detached != detached)
    {
      priv->detached = detached;

      gtk_widget_queue_draw (GTK_WIDGET (thumb));
    }
}
