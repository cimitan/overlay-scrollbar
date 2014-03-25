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

#include "em-converter.h"

#include <stdio.h>
#include <stdlib.h>

#define BASE_DPI         96.0
#define PIXELS_PER_INCH  72.0
#define CONVERT_TO_SCALE 8.0

#define SCALE_FATOR            "scale-factor"
#define UNITY_GSETTINGS_SCHEMA "com.ubuntu.user-interface"

static void
update_dpi_value(EMConverter* converter, double scale_value)
{
  if (!converter)
    return;

  converter->old_dpi = converter->dpi;
  converter->dpi     = BASE_DPI * scale_value;
}

extern void test(EMConverter* converter)
{
  converter->old_dpi = converter->dpi;
}

static void
parse_font_scale_factor (EMConverter* converter)
{
  float value;
  int raw_value;
  GVariant* dict;
  GdkScreen *screen;
  gchar* monitor_name;

  g_settings_get(converter->unity_settings, SCALE_FATOR, "@a{si}", &dict);

  screen = gtk_widget_get_screen (converter->parent);

  monitor_name = gdk_screen_get_monitor_plug_name (screen, 0);
  if (g_variant_lookup(dict, monitor_name, "i", &raw_value))
    value = raw_value / CONVERT_TO_SCALE;

  g_free (monitor_name);

  update_dpi_value(converter, value);
}

static void
font_scale_changed_callback (GSettings* settings, gchar *key, gpointer data)
{
  EMConverter* converter = (EMConverter*)data;

  parse_font_scale_factor(converter);
}

static void
get_unity_settings (EMConverter* converter)
{
  converter->unity_settings = g_settings_new(UNITY_GSETTINGS_SCHEMA);

  g_signal_connect (converter->unity_settings, "changed::"SCALE_FATOR,
                      G_CALLBACK (font_scale_changed_callback), converter);
}


EMConverter*
new_converter (GtkWidget *parent)
{
  EMConverter* converter = (EMConverter*)malloc(sizeof(EMConverter));

  converter->montior        = 0;
  converter->dpi            = BASE_DPI;
  converter->unity_settings = NULL;
  converter->parent         = parent;

  get_unity_settings(converter);
  parse_font_scale_factor(converter);

  converter->old_dpi = converter->dpi;

  return converter;
}

void
cleanup_converter (EMConverter* converter)
{
  if (converter)
    {
      if (converter->unity_settings != NULL)
      {
        g_object_unref(converter->unity_settings);
        converter->unity_settings = NULL;
      }

      converter = NULL;
    }
}

double
convert_pixels (EMConverter* converter, double pixel)
{
  if (!converter || BASE_DPI == converter->dpi)
    return pixel;

  double base_ppe = BASE_DPI / PIXELS_PER_INCH;
  double pixels_em = pixel / base_ppe;

  double current_ppe = converter->dpi / PIXELS_PER_INCH;
  double new_pixels = pixels_em * current_ppe;

  return new_pixels;
}

double
dpi_scale (EMConverter* converter)
{
  if (!converter)
    return 1.0;

  return converter->dpi / BASE_DPI;
}
