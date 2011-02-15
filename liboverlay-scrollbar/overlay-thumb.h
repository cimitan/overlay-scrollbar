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

#ifndef __OVERLAY_THUMB_H__
#define __OVERLAY_THUMB_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OS_TYPE_OVERLAY_THUMB            (overlay_thumb_get_type ())
#define OVERLAY_THUMB(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), OS_TYPE_OVERLAY_THUMB, OverlayThumb))
#define OVERLAY_THUMB_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass),  OS_TYPE_OVERLAY_THUMB, OverlayThumbClass))
#define OS_IS_OVERLAY_THUMB(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OS_TYPE_OVERLAY_THUMB))
#define OS_IS_OVERLAY_THUMB_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  OS_TYPE_OVERLAY_THUMB))
#define OVERLAY_THUMB_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj),  OS_TYPE_OVERLAY_THUMB, OverlayThumbClass))

typedef struct _OverlayThumb      OverlayThumb;
typedef struct _OverlayThumbClass OverlayThumbClass;

struct _OverlayThumb
{
  GtkWindow parent_object;
};

struct _OverlayThumbClass
{
  GtkWindowClass parent_class;
};

GType      overlay_thumb_get_type (void) G_GNUC_CONST;
GtkWidget* overlay_thumb_new (void);

G_END_DECLS

#endif /* __OVERLAY_THUMB_H__ */
