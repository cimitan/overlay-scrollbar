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

#ifndef __OVERLAY_SCROLLBAR_H__
#define __OVERLAY_SCROLLBAR_H__

G_BEGIN_DECLS

#define OS_TYPE_OVERLAY_SCROLLBAR            (overlay_scrollbar_get_type ())
#define OVERLAY_SCROLLBAR(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), OS_TYPE_OVERLAY_SCROLLBAR, OverlayScrollbar))
#define OVERLAY_SCROLLBAR_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass),  OS_TYPE_OVERLAY_SCROLLBAR, OverlayScrollbarClass))
#define OS_IS_OVERLAY_SCROLLBAR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OS_TYPE_OVERLAY_SCROLLBAR))
#define OS_IS_OVERLAY_SCROLLBAR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  OS_TYPE_OVERLAY_SCROLLBAR))
#define OVERLAY_SCROLLBAR_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj),  OS_TYPE_OVERLAY_SCROLLBAR, OverlayScrollbarClass))

typedef struct _OverlayScrollbar      OverlayScrollbar;
typedef struct _OverlayScrollbarClass OverlayScrollbarClass;

struct _OverlayScrollbar
{
};

struct _OverlayScrollbarClass
{
  GtkWindowClass parent_class;
};


GType overlay_scrollbar_get_type (void) G_GNUC_CONST;

void os_create_overlay_scrollbar (GtkWidget *widget,
                                  gint x,
                                  gint y,
                                  gint width,
                                  gint height);

G_END_DECLS

#endif /* __OVERLAY_SCROLLBAR_H__ */
