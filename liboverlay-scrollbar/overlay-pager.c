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


#include <cairo-xlib.h>
#include <gdk/gdkx.h>

#include "overlay-pager.h"

#define OVERLAY_PAGER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), OS_TYPE_OVERLAY_PAGER, OverlayPagerPrivate))

G_DEFINE_TYPE (OverlayPager, overlay_pager, G_TYPE_OBJECT);

typedef struct _OverlayPagerPrivate OverlayPagerPrivate;

struct _OverlayPagerPrivate
{
};

static void
overlay_pager_class_init (OverlayPagerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
}

static void
overlay_pager_init (OverlayPager *overlay)
{
}

