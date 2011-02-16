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

#include <cairo.h>
#include <cairo-xlib.h>
#include <gdk/gdkx.h>

#include "overlay-pager.h"
#include "overlay-scrollbar-support.h"

#define OVERLAY_PAGER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), OS_TYPE_OVERLAY_PAGER, OverlayPagerPrivate))

G_DEFINE_TYPE (OverlayPager, overlay_pager, G_TYPE_OBJECT);

typedef struct _OverlayPagerPrivate OverlayPagerPrivate;

struct _OverlayPagerPrivate
{
/*  GdkDrawable *overlay_drawable;*/
  GdkWindow *overlay_window;
  GdkWindow *parent;

  GdkRectangle *mask;

  gint width;
  gint height;
};

enum {
  PROP_0,
  PROP_PARENT,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_MASK,
  LAST_ARG
};

/* GOBJECT CLASS FUNCTIONS */
static void overlay_pager_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec);

static void overlay_pager_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec);

/* HELPER FUNCTIONS */
static void overlay_pager_draw_bitmap (GdkPixmap    *pixmap,
                                       GdkRectangle *mask);

static void overlay_pager_draw_pixmap (GdkPixmap *pixmap);

/* CLASS FUNCTIONS */
/**
 * overlay_pager_class_init:
 * override class function
 **/
static void
overlay_pager_class_init (OverlayPagerClass *class)
{
  DEBUG
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->get_property = overlay_pager_get_property;
  gobject_class->set_property = overlay_pager_set_property;

  g_object_class_install_property (gobject_class,
                                   PROP_PARENT,
                                   g_param_spec_object ("parent",
                                                        "Parent",
                                                        "Reference to the parent GdkWindow",
                                                        G_TYPE_POINTER,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  g_object_class_install_property (gobject_class,
                                   PROP_WIDTH,
                                   g_param_spec_object ("width",
                                                        "Width",
                                                        "Initial width the GdkPixmap",
                                                        G_TYPE_INT,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  g_object_class_install_property (gobject_class,
                                   PROP_HEIGHT,
                                   g_param_spec_object ("height",
                                                        "Height",
                                                        "Initial height the GdkPixmap",
                                                        G_TYPE_INT,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  g_object_class_install_property (gobject_class,
                                   PROP_MASK,
                                   g_param_spec_object ("mask",
                                                        "Mask",
                                                        "Mask of the GdkPixmap",
                                                        G_TYPE_BOXED,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  g_type_class_add_private (gobject_class, sizeof (OverlayPagerPrivate));
}

/**
 * overlay_pager_init:
 * override class function
 **/
static void
overlay_pager_init (OverlayPager *overlay)
{
  DEBUG
  GdkWindowAttr attributes;
  OverlayPagerPrivate *priv;

  priv = OVERLAY_PAGER_GET_PRIVATE (overlay);

  /* overlay_window */
  attributes.width = priv->width;
  attributes.height = priv->height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.window_type = GDK_WINDOW_CHILD;
  priv->overlay_window = gdk_window_new (gtk_widget_get_window (priv->parent), &attributes, 0);

  gdk_window_raise (priv->overlay_window);
}

/* GOBJECT CLASS FUNCTIONS */
/**
 * overlay_pager_get_property:
 * override class function
 **/
static void
overlay_pager_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  DEBUG
  OverlayPagerPrivate *priv;

  priv = OVERLAY_PAGER_GET_PRIVATE (OVERLAY_PAGER (object));

  switch (prop_id)
    {
      case PROP_PARENT:
        g_value_set_object (value, priv->parent);
        break;
      case PROP_WIDTH:
        g_value_set_int (value, priv->width);
        break;
      case PROP_HEIGHT:
        g_value_set_int (value, priv->height);
        break;
      case PROP_MASK:
        g_value_set_boxed (value, priv->mask);
    }
}

/**
 * overlay_pager_set_property:
 * override class function
 **/
static void
overlay_pager_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  DEBUG
  OverlayPagerPrivate *priv;

  priv = OVERLAY_PAGER_GET_PRIVATE (OVERLAY_PAGER (object));

  switch (prop_id)
    {
      case PROP_PARENT:
        priv->parent = g_value_get_object (value);
        break;
      case PROP_WIDTH:
        priv->width = g_value_get_int (value);
        break;
      case PROP_HEIGHT:
        priv->height = g_value_get_int (value);
        break;
      case PROP_MASK:
        priv->mask = g_value_get_boxed (value);
        break;
    }
}

/* PUBLIC FUNCTIONS */
/**
 * overlay_pager_draw:
 * draw on the overlay
 **/
void
overlay_pager_draw (OverlayPager *overlay)
{
  DEBUG
  GdkPixmap *pixmap;
  OverlayPagerPrivate *priv;

  priv = OVERLAY_PAGER_GET_PRIVATE (overlay);

  pixmap = gdk_pixmap_new (NULL, priv->width, priv->height, 24);
  overlay_pager_draw_pixmap (pixmap);

  gdk_window_set_back_pixmap (priv->overlay_window, pixmap, FALSE);
}

/**
 * overlay_pager_new:
 * @window: the GdkWindow parent window
 * @width: the width of the pager
 * @height: the height of the pager
 * @mask: a GdkRectangle that masks the overlay
 *
 * Creates a new OverlayPager.
 *
 * Returns: the new OverlayPager as a #GObject
 */
GObject*
overlay_pager_new (GdkWindow    *window,
                   gint          width,
                   gint          height,
                   GdkRectangle *mask)
{
  DEBUG
  return g_object_new (OS_TYPE_OVERLAY_PAGER,
                       "parent", window,
                       "width", width,
                       "height", height,
                       "mask", mask,
                       NULL);
}

/* HELPER FUNCTIONS */
/**
 * overlay_pager_mask:
 * mask on the overlay
 **/
static void
overlay_pager_mask (OverlayPager *overlay)
{
  DEBUG
  GdkBitmap *bitmap;
  OverlayPagerPrivate *priv;

  priv = OVERLAY_PAGER_GET_PRIVATE (overlay);

  bitmap = gdk_pixmap_new (NULL, priv->width, priv->height, 1);
  overlay_pager_draw_bitmap (bitmap, priv->mask);

  gdk_window_shape_combine_mask (priv->overlay_window, bitmap, 0, 0);
}

/**
 * overlay_pager_draw_bitmap:
 * draw on the bitmap of the overlay, to get a mask
 **/
static void
overlay_pager_draw_bitmap (GdkBitmap    *bitmap,
                           GdkRectangle *mask)
{
  cairo_t *cr_surface;
  cairo_surface_t *surface;
  gint width, height;

  gdk_drawable_get_size (bitmap, &width, &height);

  surface = cairo_xlib_surface_create_for_bitmap (GDK_DRAWABLE_XDISPLAY (bitmap),
                                                  gdk_x11_drawable_get_xid (bitmap),
                                                  GDK_SCREEN_XSCREEN (gdk_drawable_get_screen (bitmap)),
                                                  width, height);

  cr_surface = cairo_create (surface);

  cairo_set_operator (cr_surface, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba (cr_surface, 0.0, 0.0, 0.0, 0.0);
  cairo_paint (cr_surface);

  cairo_set_operator (cr_surface, CAIRO_OPERATOR_OVER);
/*  os_cairo_draw_rounded_rect (cr_surface, 1, 0, 1, height, 5);*/
/*  cairo_set_source_rgb (cr_surface, 1.0, 1.0, 1.0);*/
  cairo_rectangle (cr_surface, mask->x, mask->y, mask->width, mask->height);
  cairo_set_source_rgb (cr_surface, 1.0, 1.0, 1.0);
  cairo_fill (cr_surface);

  cairo_destroy (cr_surface);
}

/**
 * overlay_pager_draw_pixmap:
 * draw on the pixmap of the overlay, the real drawing
 **/
static void
overlay_pager_draw_pixmap (GdkPixmap *pixmap)
{
  cairo_t *cr_surface;
  cairo_surface_t *surface;
  gint width, height;

  gdk_drawable_get_size (pixmap, &width, &height);

  surface = cairo_xlib_surface_create (GDK_DRAWABLE_XDISPLAY (pixmap),
                                       gdk_x11_drawable_get_xid (pixmap),
                                       GDK_VISUAL_XVISUAL (gdk_drawable_get_visual (pixmap)),
                                       width, height);

  cr_surface = cairo_create (surface);

  cairo_set_source_rgb (cr_surface, 240.0 / 255.0, 119.0 / 255.0, 70.0 / 255.0);
  cairo_paint (cr_surface);

/*  cairo_set_line_width (cr_surface, 1.0);*/
/*  os_cairo_draw_rounded_rect (cr_surface, 0.5, 0.5, width - 1, height - 1, 2);*/
/*  cairo_set_source_rgba (cr_surface, 0.8, 0.4, 0.4, 1.0);*/
/*  cairo_stroke (cr_surface);*/

  cairo_destroy (cr_surface);
}
