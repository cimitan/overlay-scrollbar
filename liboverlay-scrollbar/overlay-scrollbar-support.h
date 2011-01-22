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
#include <cairo.h>
#include <gdk/gdkx.h>
#include <X11/X.h>
#include <X11/Xlib.h>

#ifndef __OVERLAY_SCROLLBAR_SUPPORT_H__
#define __OVERLAY_SCROLLBAR_SUPPORT_H__

G_BEGIN_DECLS

G_GNUC_INTERNAL void os_clamp_dimensions (GtkWidget    *widget,
                                          GdkRectangle *rect,
                                          GtkBorder    *border,
                                          gboolean      border_expands_horizontally);

G_GNUC_INTERNAL void os_gtk_range_calc_request (GtkRange      *range,
                                                gint           slider_width,
                                                gint           stepper_size,
                                                gint           focus_width,
                                                gint           trough_border,
                                                gint           stepper_spacing,
                                                GdkRectangle  *range_rect,
                                                GtkBorder     *border,
                                                gint          *n_steppers_p,
                                                gboolean      *has_steppers_ab,
                                                gboolean      *has_steppers_cd,
                                                gint          *slider_length_p);

G_GNUC_INTERNAL void os_gtk_range_get_props (GtkRange  *range,
                                             gint      *slider_width,
                                             gint      *stepper_size,
                                             gint      *focus_width,
                                             gint      *trough_border,
                                             gint      *stepper_spacing,
                                             gboolean  *trough_under_steppers,
                                             gint      *arrow_displacement_x,
                                             gint      *arrow_displacement_y);

G_GNUC_INTERNAL void os_present_gdk_window_with_timestamp (GtkWidget *widget,
                                                           guint32    timestamp);

G_GNUC_INTERNAL void os_present_window_with_timestamp (Display *default_display,
                                                       Screen  *screen,
                                                       gint     xid,
                                                       guint32  timestamp);

G_GNUC_INTERNAL gboolean os_gtk_widget_hide (gpointer user_data);

G_END_DECLS

#endif /* __OVERLAY_SCROLLBAR_SUPPORT_H__ */
