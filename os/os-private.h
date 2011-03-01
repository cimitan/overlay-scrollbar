/* overlay-scrollbar
 *
 * Copyright © 2011 Canonical Ltd
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
 */

#ifndef __OS_PRIVATE_H__
#define __OS_PRIVATE_H__

#include <gtk/gtk.h>

/* Tell GCC not to export internal functions. */
#ifdef __GNUC__
#pragma GCC visibility push(hidden)
#endif /* __GNUC__ */

G_BEGIN_DECLS

/* os-thumb.c */

#define OS_TYPE_THUMB (os_thumb_get_type ())
#define OS_THUMB(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), OS_TYPE_THUMB, OsThumb))
#define OS_THUMB_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass),  OS_TYPE_THUMB, OsThumbClass))
#define OS_IS_THUMB(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OS_TYPE_THUMB))
#define OS_IS_THUMB_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),  OS_TYPE_THUMB))
#define OS_THUMB_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),  OS_TYPE_THUMB, OsThumbClass))

typedef struct _OsThumb      OsThumb;
typedef struct _OsThumbClass OsThumbClass;

struct _OsThumb {
  GtkWindow parent_object;
};

struct _OsThumbClass {
  GtkWindowClass parent_class;
};

GType      os_thumb_get_type (void) G_GNUC_CONST;

GtkWidget* os_thumb_new      (GtkOrientation orientation);

/* os-pager.c */

#define OS_TYPE_PAGER (os_pager_get_type ())
#define OS_PAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), OS_TYPE_PAGER, OsPager))
#define OS_PAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), OS_TYPE_PAGER, OsPagerClass))
#define OS_IS_PAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OS_TYPE_PAGER))
#define OS_IS_PAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), OS_TYPE_PAGER))
#define OS_PAGER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),  OS_TYPE_PAGER, OsPagerClass))

typedef struct _OsPager OsPager;
typedef struct _OsPagerClass OsPagerClass;

struct _OsPager {
  GObject parent_instance;
};

struct _OsPagerClass {
  GObjectClass parent_class;
};

GType    os_pager_get_type      (void) G_GNUC_CONST;

GObject *os_pager_new           (GtkWidget *widget);

void     os_pager_hide          (OsPager *overlay);

void     os_pager_move_resize   (OsPager *overlay,
                                 GdkRectangle  mask);

void     os_pager_size_allocate (OsPager *overlay,
                                 GdkRectangle rectangle);

void     os_pager_show          (OsPager *overlay);

void     os_pager_set_active    (OsPager *overlay,
                                 gboolean active);

G_END_DECLS

#ifdef __GNUC__
#pragma GCC visibility pop
#endif /* __GNUC__ */

#endif /* __OS_PRIVATE_H__ */
