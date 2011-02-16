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

#ifndef __OVERLAY_PAGER_H__
#define __OVERLAY_PAGER_H__

G_BEGIN_DECLS

#define OS_TYPE_OVERLAY_PAGER            (overlay_pager_get_type ())
#define OVERLAY_PAGER(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), OS_TYPE_OVERLAY_PAGER, OverlayPager))
#define OVERLAY_PAGER_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass),  OS_TYPE_OVERLAY_PAGER, OverlayPagerClass))
#define OS_IS_OVERLAY_PAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OS_TYPE_OVERLAY_PAGER))
#define OS_IS_OVERLAY_PAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  OS_TYPE_OVERLAY_PAGER))
#define OVERLAY_PAGER_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj),  OS_TYPE_OVERLAY_PAGER, OverlayPagerClass))

typedef struct _OverlayPager OverlayPager;
typedef struct _OverlayPagerClass OverlayPagerClass;

struct _OverlayPager
{
  GObject parent_instance;
};

struct _OverlayPagerClass
{
  GObjectClass parent_class;
};

GType overlay_pager_get_type (void) G_GNUC_CONST;

void overlay_pager_draw (OverlayPager *overlay);

GObject* overlay_pager_new (GdkWindow    *window,
                            gint          width,
                            gint          height,
                            GdkRectangle *mask);

G_END_DECLS

#endif /* __OVERLAY_PAGER_H__ */
