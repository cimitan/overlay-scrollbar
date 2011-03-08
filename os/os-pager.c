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

#define OS_PAGER_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), OS_TYPE_PAGER, OsPagerPrivate))

typedef struct _OsPagerPrivate OsPagerPrivate;

struct _OsPagerPrivate {
  GdkWindow *pager_window;
  GtkWidget *parent;
  GdkRectangle mask;
  GdkRectangle allocation;
  gboolean active;
  gboolean visible;
  gint width;
  gint height;
};

static void os_pager_dispose (GObject *object);
static void os_pager_finalize (GObject *object);
static void os_pager_create (OsPager *pager);
static void os_pager_draw (OsPager *pager);
static void os_pager_draw_bitmap (GdkPixmap *pixmap, GdkRectangle mask);
static void os_pager_draw_pixmap (GdkPixmap *pixmap, gboolean active);
static void os_pager_mask (OsPager *pager);

/* Private functions */

/* Create a pager. */
static void
os_pager_create (OsPager *pager)
{
  GdkWindowAttr attributes;
  OsPagerPrivate *priv;

  priv = OS_PAGER_GET_PRIVATE (pager);

  attributes.width = priv->allocation.width;
  attributes.height = priv->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.window_type = GDK_WINDOW_CHILD;

  priv->pager_window = gdk_window_new (gtk_widget_get_window (priv->parent),
                                       &attributes, 0);

  gdk_window_set_transient_for (priv->pager_window,
                                gtk_widget_get_window (priv->parent));
}

/* Draw on the pager. */
static void
os_pager_draw (OsPager *pager)
{
  GdkPixmap *pixmap;
  OsPagerPrivate *priv;

  priv = OS_PAGER_GET_PRIVATE (pager);

  pixmap = gdk_pixmap_new (NULL, priv->allocation.width,
                           priv->allocation.height, 24);
  os_pager_draw_pixmap (pixmap, priv->active);

  gdk_window_set_back_pixmap (priv->pager_window, pixmap, FALSE);
  gdk_window_clear (priv->pager_window);
}

/* Mask the pager. */
static void
os_pager_mask (OsPager *pager)
{
  GdkBitmap *bitmap;
  OsPagerPrivate *priv;

  priv = OS_PAGER_GET_PRIVATE (pager);

  bitmap = gdk_pixmap_new (NULL, priv->allocation.width,
                           priv->allocation.height, 1);
  os_pager_draw_bitmap (bitmap, priv->mask);

  gdk_window_shape_combine_mask (priv->pager_window, bitmap, 0, 0);
}

/* Draw on the bitmap of the pager, to get a mask. */
static void
os_pager_draw_bitmap (GdkBitmap    *bitmap,
                      GdkRectangle  mask)
{
  cairo_t *cr_surface;
  cairo_surface_t *surface;
  gint width, height;

  gdk_drawable_get_size (bitmap, &width, &height);

  surface = cairo_xlib_surface_create_for_bitmap
      (GDK_DRAWABLE_XDISPLAY (bitmap), gdk_x11_drawable_get_xid (bitmap),
       GDK_SCREEN_XSCREEN (gdk_drawable_get_screen (bitmap)), width, height);

  cr_surface = cairo_create (surface);

  cairo_set_operator (cr_surface, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr_surface);

  cairo_set_operator (cr_surface, CAIRO_OPERATOR_OVER);
  cairo_rectangle (cr_surface, mask.x, mask.y, mask.width, mask.height);
  cairo_set_source_rgb (cr_surface, 1.0, 1.0, 1.0);
  cairo_fill (cr_surface);

  cairo_destroy (cr_surface);
}

/* Draw on the pixmap of the pager, the real drawing. */
static void
os_pager_draw_pixmap (GdkPixmap *pixmap,
                      gboolean   active)
{
  cairo_t *cr_surface;
  cairo_surface_t *surface;
  gint width, height;

  gdk_drawable_get_size (pixmap, &width, &height);

  surface = cairo_xlib_surface_create
      (GDK_DRAWABLE_XDISPLAY (pixmap), gdk_x11_drawable_get_xid (pixmap),
       GDK_VISUAL_XVISUAL (gdk_drawable_get_visual (pixmap)), width, height);

  cr_surface = cairo_create (surface);

  if (active)
    cairo_set_source_rgb (cr_surface, 240.0 / 255.0, 119.0 / 255.0, 70.0 / 255.0);
  else
    cairo_set_source_rgb (cr_surface, 0.85, 0.85, 0.85);

  cairo_paint (cr_surface);

  cairo_destroy (cr_surface);
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
  GdkRectangle allocation;
  OsPagerPrivate *priv;

  priv = OS_PAGER_GET_PRIVATE (pager);

  allocation.x = 0;
  allocation.y = 0;
  allocation.width = 1;
  allocation.height = 1;

  priv->allocation = allocation;

  priv->active = TRUE;
  priv->visible = FALSE;
}

static void
os_pager_dispose (GObject *object)
{
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

  priv = OS_PAGER_GET_PRIVATE (pager);

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

  priv = OS_PAGER_GET_PRIVATE (pager);

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

  priv = OS_PAGER_GET_PRIVATE (pager);

  if (priv->active != active)
    {
      priv->active = active;

      if (priv->parent == NULL)
        return;

      os_pager_draw (pager);
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

  priv = OS_PAGER_GET_PRIVATE (pager);

  if (priv->parent != NULL)
    {
      g_object_unref (priv->parent);
    }

  priv->parent = parent;

  if (priv->parent != NULL)
    {
      g_object_ref_sink (priv->parent);

      os_pager_create (pager);
      os_pager_draw (pager);
      os_pager_mask (pager);

      gdk_window_move_resize (priv->pager_window,
                              priv->allocation.x,
                              priv->allocation.y,
                              priv->allocation.width,
                              priv->allocation.height);

      if (priv->visible)
        gdk_window_show (priv->pager_window);
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

  priv = OS_PAGER_GET_PRIVATE (pager);

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

  priv = OS_PAGER_GET_PRIVATE (pager);

  priv->allocation = rectangle;

  if (priv->parent == NULL)
    return;

  gdk_window_move_resize (priv->pager_window,
                          rectangle.x,
                          rectangle.y,
                          rectangle.width,
                          rectangle.height);
}
