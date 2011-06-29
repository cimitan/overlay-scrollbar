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

#include <cairo-xlib.h>
#include <gdk/gdkx.h>
#include "os-private.h"

/* Rate of the fade */
#define RATE_FADE 30

/* Duration of the fade-in */
#define DURATION_FADE_IN 200

/* Duration of the fade-out */
#define DURATION_FADE_OUT 400

#ifdef USE_GTK3
#define SHAPE_REGION(x) (cairo_region_create_rectangle (x))
#else
#define SHAPE_REGION(x) (gdk_region_rectangle (x))
#endif

struct _OsPagerPrivate {
  GdkWindow *pager_window;
  GdkWindow *connection_window;
  GtkWidget *parent;
  GdkRectangle mask;
  GdkRectangle connection_mask; /* in theory not needed, but easier to read. */
  GdkRectangle allocation;
  OsAnimation *animation;
  gboolean active;
  gboolean detached;
  gboolean visible;
  gfloat weight;
  gulong handler_id;
};

static void os_pager_dispose (GObject *object);
static void os_pager_finalize (GObject *object);

/* Private functions */

/* draw on the connection_window */
static void
draw_connection (OsPager *pager)
{
#ifdef USE_GTK3
  GdkRGBA color;
  GtkStyleContext *style_context;
#else
  GdkColor color;
  GtkStyle *style;
#endif
  OsPagerPrivate *priv;

  priv = pager->priv;

#ifdef USE_GTK3
  style_context = gtk_widget_get_style_context (priv->parent);

  gtk_style_context_get_background_color (style_context, GTK_STATE_FLAG_ACTIVE, &color);

  gdk_window_set_background_rgba (priv->connection_window, &color);
#else
  style = gtk_widget_get_style (priv->parent);

  color = style->bg[GTK_STATE_ACTIVE];

  gdk_colormap_alloc_color (gdk_drawable_get_colormap (priv->connection_window), &color, FALSE, TRUE);

  gdk_window_set_background (priv->connection_window, &color);
#endif

  gdk_window_invalidate_rect (gtk_widget_get_window (priv->parent), &priv->allocation, TRUE);
}

/* draw on the pager_window */
static void
draw_pager (OsPager *pager)
{
#ifdef USE_GTK3
  GdkRGBA c1, c2, color;
  GtkStyleContext *style_context;
#else
  GdkColor c1, c2, color;
  GtkStyle *style;
#endif
  OsPagerPrivate *priv;
  gfloat weight;

  priv = pager->priv;

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
  color.alpha = weight * c1.alpha + (1.0 - weight) * c2.alpha;

  gdk_window_set_background_rgba (priv->pager_window, &color);
#else
  style = gtk_widget_get_style (priv->parent);

  if (priv->active == FALSE)
    {
      c1 = style->bg[GTK_STATE_INSENSITIVE];
      c2 = style->bg[GTK_STATE_SELECTED];
    }
  else
    {
      c1 = style->bg[GTK_STATE_SELECTED];
      c2 = style->bg[GTK_STATE_INSENSITIVE];
    }

  color.red   = weight * c1.red   + (1.0 - weight) * c2.red;
  color.green = weight * c1.green + (1.0 - weight) * c2.green;
  color.blue  = weight * c1.blue  + (1.0 - weight) * c2.blue;

  gdk_colormap_alloc_color (gdk_drawable_get_colormap (priv->pager_window), &color, FALSE, TRUE);

  gdk_window_set_background (priv->pager_window, &color);
#endif

  gdk_window_invalidate_rect (gtk_widget_get_window (priv->parent), &priv->allocation, TRUE);
}

/* callback called by the change-state animation */
static void
change_state_cb (gfloat   weight,
                 gpointer user_data)
{
  OsPager *pager;
  OsPagerPrivate *priv;

  pager = OS_PAGER (user_data);

  priv = pager->priv;

  priv->weight = weight;

  if (priv->parent == NULL)
    return;

  draw_pager (pager);
}

/* stop_func called by the change-state animation */
static void
change_state_stop_cb (gpointer user_data)
{
  OsPager *pager;
  OsPagerPrivate *priv;

  pager = OS_PAGER (user_data);

  priv = pager->priv;

  priv->weight = 1.0f;

  draw_pager (pager);
}

/* callback called when the Gtk+ theme changes */
static void
notify_gtk_theme_name_cb (GObject*    gobject,
                          GParamSpec* pspec,
                          gpointer    user_data)
{
  OsPager *pager;
  OsPagerPrivate *priv;

  pager = OS_PAGER (user_data);
  priv = pager->priv;

  if (priv->parent == NULL ||
      priv->pager_window == NULL ||
      priv->connection_window == NULL)
    return;

  draw_connection (pager);
  draw_pager (pager);
}

/* check if two GdkRectangle are different */
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

G_DEFINE_TYPE (OsPager, os_pager, G_TYPE_OBJECT);

static void
os_pager_class_init (OsPagerClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->dispose  = os_pager_dispose;
  gobject_class->finalize = os_pager_finalize;

  g_type_class_add_private (gobject_class, sizeof (OsPagerPrivate));
}

static void
os_pager_init (OsPager *pager)
{
  GdkRectangle allocation, mask;
  OsPagerPrivate *priv;

  pager->priv = G_TYPE_INSTANCE_GET_PRIVATE (pager,
                                             OS_TYPE_PAGER,
                                             OsPagerPrivate);
  priv = pager->priv;

  allocation.x = 0;
  allocation.y = 0;
  allocation.width = 1;
  allocation.height = 1;

  priv->allocation = allocation;

  mask.x = 0;
  mask.y = 0;
  mask.width = 1;
  mask.height = 1;

  priv->connection_mask = mask;
  priv->mask = mask;

  priv->active = FALSE;
  priv->detached = FALSE;
  priv->visible = FALSE;

  priv->weight = 1.0f;

  priv->animation = os_animation_new (RATE_FADE, DURATION_FADE_OUT,
                                      change_state_cb, NULL, pager);

  priv->handler_id = g_signal_connect (gtk_settings_get_default (), "notify::gtk-theme-name",
                                       G_CALLBACK (notify_gtk_theme_name_cb), pager);
}

static void
os_pager_dispose (GObject *object)
{
  OsPager *pager;
  OsPagerPrivate *priv;

  pager = OS_PAGER (object);
  priv = pager->priv;

  if (priv->animation != NULL)
    {
      g_object_unref (priv->animation);
      priv->animation = NULL;
    }

  if (priv->connection_window != NULL)
    {
      /* From the Gdk documentation:
       * "Note that a window will not be destroyed
       *  automatically when its reference count
       *  reaches zero. You must call
       *  gdk_window_destroy ()
       *  yourself before that happens". */
      gdk_window_destroy (priv->connection_window);

      g_object_unref (priv->connection_window);
      priv->connection_window = NULL;
    }

  if (priv->pager_window != NULL)
    {
      /* From the Gdk documentation:
       * "Note that a window will not be destroyed
       *  automatically when its reference count
       *  reaches zero. You must call
       *  gdk_window_destroy ()
       *  yourself before that happens". */
      gdk_window_destroy (priv->pager_window);

      g_object_unref (priv->pager_window);
      priv->pager_window = NULL;
    }

  os_pager_set_parent (pager, NULL);

  g_signal_handler_disconnect (gtk_settings_get_default (),
                               priv->handler_id);

  G_OBJECT_CLASS (os_pager_parent_class)->dispose (object);
}

static void
os_pager_finalize (GObject *object)
{
  G_OBJECT_CLASS (os_pager_parent_class)->finalize (object);
}

/* Public functions. */

/**
 * os_pager_new:
 *
 * Creates a new #OsPager instance.
 *
 * Returns: the new #OsPager instance.
 */
OsPager*
os_pager_new (void)
{
  return g_object_new (OS_TYPE_PAGER, NULL);
}

/* move a mask on the connection_window, fake movement */
static void
mask_connection (OsPager *pager)
{
  OsPagerPrivate *priv;

  priv = pager->priv;

  gdk_window_shape_combine_region (priv->connection_window,
                                   SHAPE_REGION(&priv->connection_mask),
                                   0, 0);
}

/**
 * os_pager_connect:
 * @pager: a #OsPager
 * @mask: a #GdkRectangle with the position and dimension of the connection
 *
 * Moves and resizes connection.
 **/
void
os_pager_connect (OsPager      *pager,
                  GdkRectangle  mask)
{
  OsPagerPrivate *priv;

  g_return_if_fail (OS_PAGER (pager));

  priv = pager->priv;

  if (!rectangle_changed (priv->connection_mask, mask))
    return;

  priv->connection_mask = mask;

  if (priv->parent == NULL)
    return;

  mask_connection (pager);
}

/**
 * os_pager_hide:
 * @pager: a #OsPager
 *
 * Hides the #OsPager.
 **/
void
os_pager_hide (OsPager *pager)
{
  OsPagerPrivate *priv;

  g_return_if_fail (OS_PAGER (pager));

  /* return if pager is NULL, happens on emacs23,
   * when emacs calls unrealize after dispose. */
  if (pager == NULL)
    return;

  priv = pager->priv;

  priv->visible = FALSE;

  if (priv->parent == NULL)
    return;

  /* if there's an animation currently running, stop it. */
  os_animation_stop (priv->animation, change_state_stop_cb);

  gdk_window_hide (priv->connection_window);
  gdk_window_hide (priv->pager_window);
}

/* move a mask on the pager_window, fake movement */
static void
mask_pager (OsPager *pager)
{
  OsPagerPrivate *priv;

  priv = pager->priv;

  gdk_window_shape_combine_region (priv->pager_window,
                                   SHAPE_REGION(&priv->mask),
                                   0, 0);
}

/**
 * os_pager_move_resize:
 * @pager: a #OsPager
 * @mask: a #GdkRectangle with the position and dimension of the #OsPager
 *
 * Moves and resizes @pager.
 **/
void
os_pager_move_resize (OsPager      *pager,
                      GdkRectangle  mask)
{
  OsPagerPrivate *priv;

  g_return_if_fail (OS_PAGER (pager));

  priv = pager->priv;

  if (!rectangle_changed (priv->mask, mask))
    return;

  priv->mask = mask;

  if (priv->parent == NULL)
    return;

  mask_pager (pager);
}

/**
 * os_pager_set_active:
 * @pager: a #OsPager
 * @active: whether is active or not
 * @animation: whether animate it or not
 *
 * Changes the activity state of @pager.
 **/
void
os_pager_set_active (OsPager *pager,
                     gboolean active,
                     gboolean animate)
{
  OsPagerPrivate *priv;

  g_return_if_fail (OS_PAGER (pager));

  priv = pager->priv;

  /* set the state and draw even if there's an animation running, that is
   * (!animate && os_animation_is_running (priv->animation)). */
  if ((priv->active != active) ||
      (!animate && os_animation_is_running (priv->animation)))
    {
      gboolean visible;

      priv->active = active;

      if (priv->parent == NULL)
        return;

      visible = gdk_window_is_visible (priv->pager_window);

      if (visible)
        os_animation_stop (priv->animation, NULL);

      if (visible && animate)
        {
          os_animation_set_duration (priv->animation, priv->active ? DURATION_FADE_IN :
                                                                     DURATION_FADE_OUT);
          os_animation_start (priv->animation);
        }

      if (!visible || !animate)
        {
          priv->weight = 1.0f;

          draw_pager (pager);
        }
    }
}

/**
 * os_pager_set_detached:
 * @pager: a #OsPager
 * @detached: whether the pager is detached or not
 *
 * Changes the detached state of @pager.
 **/
void
os_pager_set_detached (OsPager *pager,
                       gboolean detached)
{
  OsPagerPrivate *priv;

  g_return_if_fail (OS_PAGER (pager));

  priv = pager->priv;

  if (priv->detached != detached)
    {
      priv->detached = detached;

      if (priv->parent == NULL)
        return;

      if (priv->detached)
        {
          gdk_window_show (priv->connection_window);
          gdk_window_raise (priv->pager_window);
        }
      else
        gdk_window_hide (priv->connection_window);
    }
}

/* create connection_window and pager_window */
static void
create_windows (OsPager *pager)
{
  GdkWindowAttr attributes;
  OsPagerPrivate *priv;

  priv = pager->priv;

  /* Instead reparenting,
   * which doesn't seem to work well,
   * destroy the two windows. */
  if (priv->connection_window != NULL)
    {
      /* From the Gdk documentation:
       * "Note that a window will not be destroyed
       *  automatically when its reference count
       *  reaches zero. You must call
       *  gdk_window_destroy ()
       *  yourself before that happens". */
      gdk_window_destroy (priv->connection_window);

      g_object_unref (priv->connection_window);
      priv->connection_window = NULL;
    }

  if (priv->pager_window != NULL)
    {
      /* From the Gdk documentation:
       * "Note that a window will not be destroyed
       *  automatically when its reference count
       *  reaches zero. You must call
       *  gdk_window_destroy ()
       *  yourself before that happens". */
      gdk_window_destroy (priv->pager_window);

      g_object_unref (priv->pager_window);
      priv->pager_window = NULL;
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

  /* connection_window */
  priv->connection_window = gdk_window_new (gtk_widget_get_window (priv->parent),
                                            &attributes,
#ifdef USE_GTK3
                                            GDK_WA_VISUAL);
#else
                                            GDK_WA_VISUAL | GDK_WA_COLORMAP);
#endif

  g_object_ref_sink (priv->connection_window);

  gdk_window_set_transient_for (priv->connection_window,
                                gtk_widget_get_window (priv->parent));

  /* FIXME(Cimi) maybe this is not required with 0 as event mask */
  gdk_window_input_shape_combine_region (priv->connection_window,
#ifdef USE_GTK3
                                         cairo_region_create (),
#else
                                         gdk_region_new (),
#endif
                                         0, 0);

  /* pager_window */
  priv->pager_window = gdk_window_new (gtk_widget_get_window (priv->parent),
                                       &attributes,
#ifdef USE_GTK3
                                       GDK_WA_VISUAL);
#else
                                       GDK_WA_VISUAL | GDK_WA_COLORMAP);
#endif 

  g_object_ref_sink (priv->pager_window);

  gdk_window_set_transient_for (priv->pager_window,
                                gtk_widget_get_window (priv->parent));

  /* FIXME(Cimi) maybe this is not required with 0 as event mask */
  gdk_window_input_shape_combine_region (priv->pager_window,
#ifdef USE_GTK3
                                         cairo_region_create (),
#else
                                         gdk_region_new (),
#endif
                                         0, 0);
}

/**
 * os_pager_set_parent:
 * @pager: a #OsPager
 * @parent: a #GtkWidget
 *
 * Sets the parent widget
 **/
void
os_pager_set_parent (OsPager   *pager,
                     GtkWidget *parent)
{
  OsPagerPrivate *priv;

  g_return_if_fail (OS_PAGER (pager));

  priv = pager->priv;

  /* stop currently running animation. */
  if (priv->animation != NULL)
    os_animation_stop (priv->animation, NULL);

  if (priv->parent != NULL)
    {
      g_object_unref (priv->parent);
    }

  priv->parent = parent;

  if (priv->parent != NULL)
    {
      g_object_ref_sink (priv->parent);

      priv->weight = 1.0f;

      create_windows (pager);
      draw_connection (pager);
      draw_pager (pager);
      mask_pager (pager);

      gdk_window_move_resize (priv->connection_window,
                              priv->allocation.x,
                              priv->allocation.y,
                              priv->allocation.width,
                              priv->allocation.height);

      gdk_window_move_resize (priv->pager_window,
                              priv->allocation.x,
                              priv->allocation.y,
                              priv->allocation.width,
                              priv->allocation.height);

      if (priv->visible)
        {
          gdk_window_show (priv->pager_window);
        }
    }
}

/**
 * os_pager_show:
 * @pager: a #OsPager
 *
 * Shows @pager.
 **/
void
os_pager_show (OsPager *pager)
{
  OsPagerPrivate *priv;

  g_return_if_fail (OS_PAGER (pager));

  priv = pager->priv;

  priv->visible = TRUE;

  if (priv->parent == NULL)
    return;

  gdk_window_show (priv->pager_window);
}

/**
 * os_pager_size_allocate:
 * @pager: a #OsPager
 * @rectangle: a #GdkRectangle
 *
 * Sets the position and dimension of the whole area.
 **/
void
os_pager_size_allocate (OsPager     *pager,
                        GdkRectangle rectangle)
{
  OsPagerPrivate *priv;

  g_return_if_fail (OS_PAGER (pager));

  priv = pager->priv;

  if (!rectangle_changed (priv->allocation, rectangle))
    return;

  priv->allocation = rectangle;

  if (priv->parent == NULL)
    return;

  gdk_window_move_resize (priv->connection_window,
                          rectangle.x,
                          rectangle.y,
                          rectangle.width,
                          rectangle.height);

  gdk_window_move_resize (priv->pager_window,
                          rectangle.x,
                          rectangle.y,
                          rectangle.width,
                          rectangle.height);
}
