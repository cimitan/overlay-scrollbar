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

#include "os-private.h"

#include <cairo-xlib.h>
#include <gdk/gdkx.h>

/* Duration of the fade-in. */
#define DURATION_FADE_IN 200

/* Duration of the fade-out. */
#define DURATION_FADE_OUT 400

/* Max duration of the retracting tail. */
#define MAX_DURATION_TAIL 300

/* Min duration of the retracting tail. */
#define MIN_DURATION_TAIL 100

struct _OsBarPrivate {
  GdkRectangle bar_mask;
  GdkRectangle tail_mask; /* In theory not needed, but easier to read. */
  GdkRectangle allocation;
  GdkWindow *bar_window;
  GdkWindow *tail_window;
  GtkWidget *parent;
  OsAnimation *state_animation;
  OsAnimation *tail_animation;
  gboolean active;
  gboolean detached;
  gboolean visible;
  gfloat weight;
};

static void os_bar_dispose (GObject *object);
static void os_bar_finalize (GObject *object);

/* Draw on the bar_window. */
static void
draw_bar (OsBar *bar)
{
#ifdef USE_GTK3
  GdkRGBA c1, c2, color;
  GtkStyleContext *style_context;
#else
  GdkColor color;
  GtkStyle *style;
#endif
  OsBarPrivate *priv;
  gfloat weight;

  priv = bar->priv;

  weight = priv->weight;

#ifdef USE_GTK3
  style_context = gtk_widget_get_style_context (priv->parent);

  if (priv->active == FALSE)
    {
      gtk_style_context_get_background_color (style_context, GTK_STATE_FLAG_INSENSITIVE, &c1);
      gtk_style_context_get_background_color (style_context, GTK_STATE_FLAG_SELECTED, &c2);
    }
  else
    {
      gtk_style_context_get_background_color (style_context, GTK_STATE_FLAG_SELECTED, &c1);
      gtk_style_context_get_background_color (style_context, GTK_STATE_FLAG_INSENSITIVE, &c2);
    }

  color.red   = weight * c1.red   + (1.0 - weight) * c2.red;
  color.green = weight * c1.green + (1.0 - weight) * c2.green;
  color.blue  = weight * c1.blue  + (1.0 - weight) * c2.blue;
  color.alpha = 1.0;

  gdk_window_set_background_rgba (priv->bar_window, &color);
#else
  style = gtk_widget_get_style (priv->parent);

  color = style->bg[GTK_STATE_SELECTED];

  gdk_colormap_alloc_color (gdk_drawable_get_colormap (priv->bar_window), &color, FALSE, TRUE);

  gdk_window_set_background (priv->bar_window, &color);
#endif

  gdk_window_invalidate_rect (gtk_widget_get_window (priv->parent), &priv->allocation, TRUE);
}

/* Draw on the tail_window. */
static void
draw_tail (OsBar *bar)
{
#ifdef USE_GTK3
  GdkRGBA color;
  GtkStyleContext *style_context;
#else
  GdkColor color;
  GtkStyle *style;
#endif
  OsBarPrivate *priv;

  priv = bar->priv;

#ifdef USE_GTK3
  style_context = gtk_widget_get_style_context (priv->parent);

  gtk_style_context_get_background_color (style_context, GTK_STATE_FLAG_ACTIVE, &color);

  color.alpha = 1.0;

  gdk_window_set_background_rgba (priv->tail_window, &color);
#else
  style = gtk_widget_get_style (priv->parent);

  color = style->bg[GTK_STATE_ACTIVE];

  gdk_colormap_alloc_color (gdk_drawable_get_colormap (priv->tail_window), &color, FALSE, TRUE);

  gdk_window_set_background (priv->tail_window, &color);
#endif

  gdk_window_invalidate_rect (gtk_widget_get_window (priv->parent), &priv->allocation, TRUE);
}

/* Callback called by the change-state animation. */
static void
change_state_cb (gfloat   weight,
                 gpointer user_data)
{
  OsBar *bar;
  OsBarPrivate *priv;

  bar = OS_BAR (user_data);

  priv = bar->priv;

  priv->weight = weight;

  if (priv->parent == NULL)
    return;

  draw_bar (bar);
}

/* Stop function called by the change-state animation. */
static void
change_state_stop_cb (gpointer user_data)
{
  OsBar *bar;
  OsBarPrivate *priv;

  bar = OS_BAR (user_data);

  priv = bar->priv;

  priv->weight = 1.0f;

  draw_bar (bar);
}

/* Callback called when the Gtk+ theme changes. */
static void
notify_gtk_theme_name_cb (GObject*    gobject,
                          GParamSpec* pspec,
                          gpointer    user_data)
{
  OsBar *bar;
  OsBarPrivate *priv;

  bar = OS_BAR (user_data);
  priv = bar->priv;

  if (priv->parent == NULL ||
      priv->bar_window == NULL ||
      priv->tail_window == NULL)
    return;

  draw_tail (bar);
  draw_bar (bar);
}

/* Check if two GdkRectangle are different. */
static gboolean
rectangle_changed (GdkRectangle rectangle1,
                   GdkRectangle rectangle2)
{
  if (rectangle1.x != rectangle2.x) return TRUE;
  if (rectangle1.y != rectangle2.y) return TRUE;
  if (rectangle1.width  != rectangle2.width)  return TRUE;
  if (rectangle1.height != rectangle2.height) return TRUE;

  return FALSE;
}

/* wrapper around gdk_window_shape_combine_region() */
static void
os_bar_window_shape_combine_region (GdkWindow          * window,
                                    const GdkRectangle * shape_rect,
                                    gint                 offset_x,
                                    gint                 offset_y)
{
#ifdef USE_GTK3

  cairo_region_t * shape_region = cairo_region_create_rectangle (shape_rect);
  gdk_window_shape_combine_region (window, shape_region, offset_x, offset_y);
  cairo_region_destroy (shape_region);

#else

  GdkRegion * shape_region = gdk_region_rectangle (shape_rect);
  gdk_window_shape_combine_region (window, shape_region, offset_x, offset_y);
  gdk_region_destroy (shape_region);

#endif
}

/* Callback called by the retract-tail animation. */
static void
retract_tail_cb (gfloat   weight,
                 gpointer user_data)
{
  GdkRectangle tail_mask;
  OsBar *bar;
  OsBarPrivate *priv;

  bar = OS_BAR (user_data);

  priv = bar->priv;

  if (priv->parent == NULL)
    return;

  tail_mask = priv->tail_mask;

  if (priv->allocation.height >= priv->allocation.width)
    {
      tail_mask.height = tail_mask.height * (1.0 - weight);

      if (priv->tail_mask.y + priv->tail_mask.height < priv->bar_mask.y + priv->bar_mask.height)
        tail_mask.y = priv->tail_mask.y + priv->tail_mask.height - tail_mask.height;
    }
  else
    {
      tail_mask.width = tail_mask.width * (1.0 - weight);

      if (priv->tail_mask.x + priv->tail_mask.width < priv->bar_mask.x + priv->bar_mask.width)
        tail_mask.x = priv->tail_mask.x + priv->tail_mask.width - tail_mask.width;
    }

  if (weight < 1.0)
    {
      os_bar_window_shape_combine_region (priv->tail_window,
                                          &tail_mask,
                                          0, 0);
    }
  else
    {
      /* Store the new tail_mask and hide the tail_window. */
      priv->tail_mask = tail_mask;
      gdk_window_hide (priv->tail_window);
    }
}

/* Stop function called by the retract-tail animation. */
static void
retract_tail_stop_cb (gpointer user_data)
{
  OsBar *bar;
  OsBarPrivate *priv;

  bar = OS_BAR (user_data);

  priv = bar->priv;

  if (priv->parent == NULL)
    return;

  os_bar_window_shape_combine_region (priv->tail_window,
                                      &priv->tail_mask,
                                      0, 0);
}

G_DEFINE_TYPE (OsBar, os_bar, G_TYPE_OBJECT);

static void
os_bar_class_init (OsBarClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->dispose  = os_bar_dispose;
  gobject_class->finalize = os_bar_finalize;

  g_type_class_add_private (gobject_class, sizeof (OsBarPrivate));
}

static void
os_bar_init (OsBar *bar)
{
  GdkRectangle allocation, mask;
  OsBarPrivate *priv;

  bar->priv = G_TYPE_INSTANCE_GET_PRIVATE (bar,
                                           OS_TYPE_BAR,
                                           OsBarPrivate);
  priv = bar->priv;

  allocation.x = 0;
  allocation.y = 0;
  allocation.width = 1;
  allocation.height = 1;

  priv->allocation = allocation;

  mask.x = 0;
  mask.y = 0;
  mask.width = 1;
  mask.height = 1;

  priv->bar_mask = mask;
  priv->tail_mask = mask;

  priv->weight = 1.0f;

  priv->state_animation = os_animation_new (RATE_ANIMATION, DURATION_FADE_OUT,
                                            change_state_cb, NULL, bar);
  priv->tail_animation = os_animation_new (RATE_ANIMATION, MAX_DURATION_TAIL,
                                           retract_tail_cb, NULL, bar);

  g_signal_connect (gtk_settings_get_default (), "notify::gtk-theme-name",
                    G_CALLBACK (notify_gtk_theme_name_cb), bar);
}

static void
os_bar_dispose (GObject *object)
{
  OsBar *bar;
  OsBarPrivate *priv;

  bar = OS_BAR (object);
  priv = bar->priv;

  if (priv->tail_animation != NULL)
    {
      g_object_unref (priv->tail_animation);
      priv->tail_animation = NULL;
    }

  if (priv->state_animation != NULL)
    {
      g_object_unref (priv->state_animation);
      priv->state_animation = NULL;
    }

  if (priv->tail_window != NULL)
    {
      /* From the Gdk documentation:
       * "Note that a window will not be destroyed
       *  automatically when its reference count
       *  reaches zero. You must call
       *  gdk_window_destroy ()
       *  yourself before that happens". */
      gdk_window_destroy (priv->tail_window);

      g_object_unref (priv->tail_window);
      priv->tail_window = NULL;
    }

  if (priv->bar_window != NULL)
    {
      /* From the Gdk documentation:
       * "Note that a window will not be destroyed
       *  automatically when its reference count
       *  reaches zero. You must call
       *  gdk_window_destroy ()
       *  yourself before that happens". */
      gdk_window_destroy (priv->bar_window);

      g_object_unref (priv->bar_window);
      priv->bar_window = NULL;
    }

  g_signal_handlers_disconnect_by_func (gtk_settings_get_default (), notify_gtk_theme_name_cb, object);

  os_bar_set_parent (bar, NULL);

  G_OBJECT_CLASS (os_bar_parent_class)->dispose (object);
}

static void
os_bar_finalize (GObject *object)
{
  G_OBJECT_CLASS (os_bar_parent_class)->finalize (object);
}

/* Public functions. */

/**
 * os_bar_new:
 *
 * Creates a new #OsBar instance.
 *
 * Returns: the new #OsBar instance.
 **/
OsBar*
os_bar_new (void)
{
  return g_object_new (OS_TYPE_BAR, NULL);
}

/* Move a mask on the tail_window, fake movement. */
static void
mask_tail (OsBar *bar)
{
  OsBarPrivate *priv;

  priv = bar->priv;

  os_bar_window_shape_combine_region (priv->tail_window,
                                      &priv->tail_mask,
                                      0, 0);
}

/**
 * os_bar_connect:
 * @bar: a #OsBar
 * @mask: a #GdkRectangle with the position and dimension of the tail
 *
 * Moves and resizes tail.
 **/
void
os_bar_connect (OsBar       *bar,
                GdkRectangle mask)
{
  OsBarPrivate *priv;

  g_return_if_fail (OS_IS_BAR (bar));

  priv = bar->priv;

  if (!os_animation_is_running (priv->tail_animation) &&
      !rectangle_changed (priv->tail_mask, mask))
    return;

  /* If there's an animation currently running, stop it. */
  os_animation_stop (priv->tail_animation, NULL);

  priv->tail_mask = mask;

  if (priv->parent == NULL)
    return;

  mask_tail (bar);
}

/**
 * os_bar_hide:
 * @bar: a #OsBar
 *
 * Hides the #OsBar.
 **/
void
os_bar_hide (OsBar *bar)
{
  OsBarPrivate *priv;

  g_return_if_fail (OS_IS_BAR (bar));

  priv = bar->priv;

  priv->visible = FALSE;

  if (priv->parent == NULL)
    return;

  /* Immediately hide, then stop animations. */
  gdk_window_hide (priv->tail_window);
  gdk_window_hide (priv->bar_window);

  os_animation_stop (priv->tail_animation, retract_tail_stop_cb);
  os_animation_stop (priv->state_animation, change_state_stop_cb);
}

/* Move a mask on the bar_window, fake movement. */
static void
mask_bar (OsBar *bar)
{
  OsBarPrivate *priv;

  priv = bar->priv;

  os_bar_window_shape_combine_region (priv->bar_window,
                                      &priv->bar_mask,
                                      0, 0);
}

/**
 * os_bar_move_resize:
 * @bar: a #OsBar
 * @mask: a #GdkRectangle with the position and dimension of the #OsBar
 *
 * Moves and resizes @bar.
 **/
void
os_bar_move_resize (OsBar       *bar,
                    GdkRectangle mask)
{
  OsBarPrivate *priv;

  g_return_if_fail (OS_IS_BAR (bar));

  priv = bar->priv;

  if (!rectangle_changed (priv->bar_mask, mask))
    return;

  priv->bar_mask = mask;

  if (priv->parent == NULL)
    return;

  mask_bar (bar);
}

/**
 * os_bar_set_active:
 * @bar: a #OsBar
 * @active: whether is active or not
 * @animate: whether animate it or not
 *
 * Changes the activity state of @bar.
 **/
void
os_bar_set_active (OsBar   *bar,
                   gboolean active,
                   gboolean animate)
{
#ifdef USE_GTK3
  OsBarPrivate *priv;

  g_return_if_fail (OS_IS_BAR (bar));

  priv = bar->priv;

  /* Set the state and draw even if there's a state_animation running, that is
   * (!animate && os_animation_is_running (priv->state_animation)). */
  if ((priv->active != active) ||
      (!animate && os_animation_is_running (priv->state_animation)))
    {
      gboolean visible;

      priv->active = active;

      if (priv->parent == NULL)
        return;

      visible = gdk_window_is_visible (priv->bar_window);

      if (visible)
        os_animation_stop (priv->state_animation, NULL);

      if (visible && animate)
        {
          os_animation_set_duration (priv->state_animation, priv->active ? DURATION_FADE_IN :
                                                                           DURATION_FADE_OUT);
          os_animation_start (priv->state_animation);
        }

      if (!visible || !animate)
        {
          priv->weight = 1.0f;

          draw_bar (bar);
        }
    }
#endif
}

/**
 * os_bar_set_detached:
 * @bar: a #OsBar
 * @detached: whether the bar is detached or not
 * @animate: whether animate it or not
 *
 * Changes the detached state of @bar.
 **/
void
os_bar_set_detached (OsBar   *bar,
                     gboolean detached,
                     gboolean animate)
{
  OsBarPrivate *priv;

  g_return_if_fail (OS_IS_BAR (bar));

  priv = bar->priv;

  if (priv->detached != detached)
    {
      priv->detached = detached;

      if (priv->parent == NULL)
        return;

      if (priv->detached)
        {
          /* If there's a tail animation currently running, stop it. */
          os_animation_stop (priv->tail_animation, retract_tail_stop_cb);

          /* No tail connection animation yet. */
          gdk_window_show (priv->tail_window);
          gdk_window_raise (priv->bar_window);
        }
      else if (animate)
        {
          gint32 duration;

          /* The detached state should already stop this. */
          OS_DCHECK (!os_animation_is_running (priv->tail_animation));

          /* Calculate and set the duration. */
          if (priv->allocation.height >= priv->allocation.width)
            duration = MIN_DURATION_TAIL + ((gdouble) priv->tail_mask.height / priv->allocation.height) *
                                           (MAX_DURATION_TAIL - MIN_DURATION_TAIL);
          else
            duration = MIN_DURATION_TAIL + ((gdouble) priv->tail_mask.width / priv->allocation.width) *
                                           (MAX_DURATION_TAIL - MIN_DURATION_TAIL);
          os_animation_set_duration (priv->tail_animation, duration);

          os_animation_start (priv->tail_animation);
        }
      else
        gdk_window_hide (priv->tail_window);
    }
}

/* Create tail_window and bar_window. */
static void
create_windows (OsBar *bar)
{
  GdkWindowAttr attributes;
  OsBarPrivate *priv;

  priv = bar->priv;

  /* Instead reparenting,
   * which doesn't seem to work well,
   * destroy the two windows. */
  if (priv->tail_window != NULL)
    {
      /* From the Gdk documentation:
       * "Note that a window will not be destroyed
       *  automatically when its reference count
       *  reaches zero. You must call
       *  gdk_window_destroy ()
       *  yourself before that happens". */
      gdk_window_destroy (priv->tail_window);

      g_object_unref (priv->tail_window);
      priv->tail_window = NULL;
    }

  if (priv->bar_window != NULL)
    {
      /* From the Gdk documentation:
       * "Note that a window will not be destroyed
       *  automatically when its reference count
       *  reaches zero. You must call
       *  gdk_window_destroy ()
       *  yourself before that happens". */
      gdk_window_destroy (priv->bar_window);

      g_object_unref (priv->bar_window);
      priv->bar_window = NULL;
    }

  attributes.event_mask = 0;
  attributes.width = priv->allocation.width;
  attributes.height = priv->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.visual = gtk_widget_get_visual (priv->parent);
#ifndef USE_GTK3
  attributes.colormap = gtk_widget_get_colormap (priv->parent);
#endif

  /* tail_window. */
  priv->tail_window = gdk_window_new (gtk_widget_get_window (priv->parent),
                                            &attributes,
#ifdef USE_GTK3
                                            GDK_WA_VISUAL);
#else
                                            GDK_WA_VISUAL | GDK_WA_COLORMAP);
#endif

  g_object_ref_sink (priv->tail_window);

  gdk_window_set_transient_for (priv->tail_window,
                                gtk_widget_get_window (priv->parent));

  /* FIXME(Cimi) maybe this is not required with 0 as event mask. */
  gdk_window_input_shape_combine_region (priv->tail_window,
#ifdef USE_GTK3
                                         cairo_region_create (),
#else
                                         gdk_region_new (),
#endif
                                         0, 0);

  /* bar_window. */
  priv->bar_window = gdk_window_new (gtk_widget_get_window (priv->parent),
                                     &attributes,
#ifdef USE_GTK3
                                     GDK_WA_VISUAL);
#else
                                     GDK_WA_VISUAL | GDK_WA_COLORMAP);
#endif 

  g_object_ref_sink (priv->bar_window);

  gdk_window_set_transient_for (priv->bar_window,
                                gtk_widget_get_window (priv->parent));

  /* FIXME(Cimi) maybe this is not required with 0 as event mask. */
  gdk_window_input_shape_combine_region (priv->bar_window,
#ifdef USE_GTK3
                                         cairo_region_create (),
#else
                                         gdk_region_new (),
#endif
                                         0, 0);
}

/**
 * os_bar_set_parent:
 * @bar: a #OsBar
 * @parent: a #GtkWidget
 *
 * Sets the parent widget
 **/
void
os_bar_set_parent (OsBar     *bar,
                   GtkWidget *parent)
{
  OsBarPrivate *priv;

  g_return_if_fail (OS_IS_BAR (bar));

  priv = bar->priv;

  /* Stop currently running animations. */
  if (priv->tail_animation != NULL)
    os_animation_stop (priv->tail_animation, retract_tail_stop_cb);
  if (priv->state_animation != NULL)
    os_animation_stop (priv->state_animation, NULL);

  if (priv->parent != NULL)
    g_object_unref (priv->parent);

  priv->parent = parent;

  if (priv->parent != NULL)
    {
      g_object_ref_sink (priv->parent);

      priv->weight = 1.0f;

      create_windows (bar);
      draw_tail (bar);
      draw_bar (bar);
      mask_bar (bar);

      gdk_window_move_resize (priv->tail_window,
                              priv->allocation.x,
                              priv->allocation.y,
                              priv->allocation.width,
                              priv->allocation.height);

      gdk_window_move_resize (priv->bar_window,
                              priv->allocation.x,
                              priv->allocation.y,
                              priv->allocation.width,
                              priv->allocation.height);

      if (priv->visible)
        gdk_window_show (priv->bar_window);
    }
}

/**
 * os_bar_show:
 * @bar: a #OsBar
 *
 * Shows @bar.
 **/
void
os_bar_show (OsBar *bar)
{
  OsBarPrivate *priv;

  g_return_if_fail (OS_IS_BAR (bar));

  priv = bar->priv;

  priv->visible = TRUE;

  if (priv->parent == NULL)
    return;

  gdk_window_show (priv->bar_window);
}

/**
 * os_bar_size_allocate:
 * @bar: a #OsBar
 * @rectangle: a #GdkRectangle
 *
 * Sets the position and dimension of the whole area.
 **/
void
os_bar_size_allocate (OsBar       *bar,
                      GdkRectangle rectangle)
{
  OsBarPrivate *priv;

  g_return_if_fail (OS_IS_BAR (bar));

  priv = bar->priv;

  if (!rectangle_changed (priv->allocation, rectangle))
    return;

  priv->allocation = rectangle;

  if (priv->parent == NULL)
    return;

  gdk_window_move_resize (priv->tail_window,
                          rectangle.x,
                          rectangle.y,
                          rectangle.width,
                          rectangle.height);

  gdk_window_move_resize (priv->bar_window,
                          rectangle.x,
                          rectangle.y,
                          rectangle.width,
                          rectangle.height);
}
