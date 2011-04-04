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

/* Timeout of the fade */
#define TIMEOUT_FADE 34

/* Max lenght of the fade */
#define DURATION_FADE 250

struct _OsPagerPrivate {
  GdkWindow *pager_window;
  GtkWidget *parent;
  GdkRectangle mask;
  GdkRectangle allocation;
  OsAnimation *animation;
  gboolean active;
  gboolean visible;
  gfloat weight;
  gint width;
  gint height;
};

static gboolean rectangle_changed (GdkRectangle rectangle1, GdkRectangle rectangle2);
static void os_pager_dispose (GObject *object);
static void os_pager_finalize (GObject *object);
static void os_pager_change_state_cb (gfloat weight, gpointer user_data);
static void os_pager_create (OsPager *pager);
static void os_pager_draw (OsPager *pager);
static void os_pager_mask (OsPager *pager);

/* Private functions */

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

/* Create a pager. */
static void
os_pager_create (OsPager *pager)
{
  OsPagerPrivate *priv;

  priv = pager->priv;

  if (priv->pager_window != NULL)
    {
      gdk_window_reparent (priv->pager_window,
                           gtk_widget_get_window (priv->parent),
                           priv->allocation.x,
                           priv->allocation.y);
    }
  else
    {
      GdkWindowAttr attributes;

      attributes.width = priv->allocation.width;
      attributes.height = priv->allocation.height;
      attributes.wclass = GDK_INPUT_OUTPUT;
      attributes.window_type = GDK_WINDOW_CHILD;
      attributes.visual = gtk_widget_get_visual (priv->parent);
      attributes.colormap = gtk_widget_get_colormap (priv->parent);

      priv->pager_window = gdk_window_new (gtk_widget_get_window (priv->parent),
                                           &attributes,
                                           GDK_WA_VISUAL | GDK_WA_COLORMAP);

      gdk_window_set_transient_for (priv->pager_window,
                                    gtk_widget_get_window (priv->parent));

      gdk_window_input_shape_combine_region (priv->pager_window,
                                             gdk_region_new (),
                                             0, 0);
    }
}

static void
os_pager_change_state_cb (gfloat weight,
                          gpointer user_data)
{
  OsPager *pager;
  OsPagerPrivate *priv;

  pager = OS_PAGER (user_data);

  priv = pager->priv;

  priv->weight = weight;

  os_pager_draw (pager);
}

/* Draw on the pager. */
static void
os_pager_draw (OsPager *pager)
{
  GdkColor c1, c2, color;
  GtkStyle *style;
  OsPagerPrivate *priv;
  gfloat weight;

  priv = pager->priv;

  style = gtk_widget_get_style (priv->parent);

  if (priv->active == FALSE)
    {
      c1 = style->base[GTK_STATE_INSENSITIVE];
      c2 = style->base[GTK_STATE_SELECTED];
    }
  else
    {
      c1 = style->base[GTK_STATE_SELECTED];
      c2 = style->base[GTK_STATE_INSENSITIVE];
    }

  weight = priv->weight;

  color.red   = weight * c1.red   + (1.0 - weight) * c2.red;
  color.green = weight * c1.green + (1.0 - weight) * c2.green;
  color.blue  = weight * c1.blue  + (1.0 - weight) * c2.blue;

  gdk_colormap_alloc_color (gdk_drawable_get_colormap (priv->pager_window), &color, FALSE, TRUE);

  gdk_window_invalidate_rect (gtk_widget_get_window (priv->parent), &priv->allocation, TRUE);

  gdk_window_set_background (priv->pager_window, &color);

  gdk_window_clear (priv->pager_window);
}

/* Mask the pager. */
static void
os_pager_mask (OsPager *pager)
{
  OsPagerPrivate *priv;

  priv = pager->priv;

  gdk_window_shape_combine_region (priv->pager_window,
                                   gdk_region_rectangle (&priv->mask),
                                   0, 0);

  gdk_window_clear (priv->pager_window);
}

/* Type definition. */

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

  priv->mask = mask;

  priv->active = FALSE;
  priv->visible = FALSE;

  priv->weight = 1.0f;

  priv->animation = os_animation_new (TIMEOUT_FADE, DURATION_FADE,
                                      os_pager_change_state_cb, NULL, pager);
}

static void
os_pager_dispose (GObject *object)
{
  OsPager *pager;
  OsPagerPrivate *priv;

  pager = OS_PAGER (object);
  priv = pager->priv;

  g_object_unref (priv->animation);

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
GObject*
os_pager_new (void)
{
  return g_object_new (OS_TYPE_PAGER, NULL);
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

  priv = pager->priv;

  priv->visible = FALSE;

  if (priv->parent == NULL)
    return;

  gdk_window_hide (priv->pager_window);
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

  os_pager_mask (pager);
}

/**
 * os_pager_set_active:
 * @pager: a #OsPager
 * @active: whether is active or not
 *
 * Changes the activity state of @pager.
 **/
void
os_pager_set_active (OsPager *pager,
                     gboolean active)
{
  OsPagerPrivate *priv;

  g_return_if_fail (OS_PAGER (pager));

  priv = pager->priv;

  if (priv->active != active)
    {
      priv->active = active;

      if (priv->parent == NULL)
        return;

      os_animation_stop (priv->animation);
      os_animation_start (priv->animation);
    }
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

  if (priv->parent != NULL)
    {
      g_object_unref (priv->parent);
    }

  priv->parent = parent;

  if (priv->parent != NULL)
    {
      g_object_ref_sink (priv->parent);

      priv->weight = 1.0f;

      os_pager_create (pager);
      os_pager_draw (pager);
      os_pager_mask (pager);

      gdk_window_move_resize (priv->pager_window,
                              priv->allocation.x,
                              priv->allocation.y,
                              priv->allocation.width,
                              priv->allocation.height);

      if (priv->visible)
        {
          gdk_window_show (priv->pager_window);

          gdk_window_clear (priv->pager_window);
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

  gdk_window_clear (priv->pager_window);
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

  gdk_window_move_resize (priv->pager_window,
                          rectangle.x,
                          rectangle.y,
                          rectangle.width,
                          rectangle.height);
}
