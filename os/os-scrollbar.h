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

#if !defined (__OS_H_INSIDE__) && !defined (OS_COMPILATION)
#error "Only <os/os.h> can be included directly."
#endif

#ifndef __OS_SCROLLBAR_H__
#define __OS_SCROLLBAR_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OS_TYPE_SCROLLBAR            (os_scrollbar_get_type ())
#define OS_SCROLLBAR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), OS_TYPE_SCROLLBAR, OsScrollbar))
#define OS_SCROLLBAR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), OS_TYPE_SCROLLBAR, OsScrollbarClass))
#define OS_IS_SCROLLBAR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OS_TYPE_SCROLLBAR))
#define OS_IS_SCROLLBAR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), OS_TYPE_SCROLLBAR))
#define OS_SCROLLBAR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), OS_TYPE_SCROLLBAR, OsScrollbarClass))

typedef struct _OsScrollbar OsScrollbar;
typedef struct _OsScrollbarClass OsScrollbarClass;
typedef struct _OsScrollbarPrivate OsScrollbarPrivate;

struct _OsScrollbar {
  GtkScrollbar parent_object;

  OsScrollbarPrivate *priv;
};

struct _OsScrollbarClass {
  GtkScrollbarClass parent_class;
};

GType      os_scrollbar_get_type (void) G_GNUC_CONST;

GtkWidget *os_scrollbar_new      (GtkOrientation orientation,
                                  GtkAdjustment *adjustment);

G_END_DECLS

#endif /* __OS_SCROLLBAR_H__ */
