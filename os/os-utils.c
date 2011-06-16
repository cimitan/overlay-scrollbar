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

#ifndef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include "os-utils.h"

/* Public functions. */

gboolean
os_utils_is_blacklisted (const gchar *program)
{
  /* Black-list of program names retrieved with g_get_prgname(). */
  static const gchar *blacklist[] = {
    "apport-gtk",
    "codeblocks",
    "codelite",
    "eclipse",
    "emacs",
    "firefox-bin",
    "gnucash",
    "gvim",
    "meld",
    "pgadmin3",
    "soffice",
    "synaptic",
    "thunderbird-bin",
    "update-manager",
    "vinagre",
    "vmplayer",
    "vmware"
  };

  GModule *module;
  gpointer func;
  gint32 i;
  const gint32 nr_programs = G_N_ELEMENTS (blacklist);

  module = g_module_open (NULL, 0);
  if (g_module_symbol (module, "qt_startup_hook", &func))
    {
      g_module_close (module);
      return TRUE;
    }
  g_module_close (module);

  /* Black list RTL languages, not supported yet */
  if (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL)
    return TRUE;

  for (i = 0; i < nr_programs; i++)
    if (g_strcmp0 (blacklist[i], program) == 0)
      return TRUE;

  return FALSE;
}
