/* overlay-scrollbar
 *
 * Copyright © 2014 Canonical Ltd
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation.
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
 * Authored by Brandon Schaefer <brandon.schaefer@canonical.com>
 */

#ifndef EM_CONVERTER_H
#define EM_CONVERTER_H

#include <gtk/gtk.h>

typedef struct {
  int montior;
  int dpi;
  int old_dpi;
  GSettings *unity_settings;
  GtkWidget *parent;

} EMConverter;

extern double convert_pixels (EMConverter* converter, double pixel);
extern double dpi_scale (EMConverter* converter);
extern void test(EMConverter* converter);

extern EMConverter* new_converter (GtkWidget *parent);
extern void cleanup_converter (EMConverter* converter);

#endif /* EM_CONVERTER_H */
