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
#endif /* hAVE_CONFIG_H */

#include "os-utils.h"

/* Public functions. */

/**
 * os_utils_is_blacklisted :
 * @program: a gchar of the program to check
 *
 * Returns: TRUE if the program is blacklisted. 
 **/
gboolean
os_utils_is_blacklisted (const gchar *program)
{
  /* Black-list of program names retrieved with g_get_prgname (). */
  static const gchar *blacklist[] = {
    "acroread", /* https://bugs.launchpad.net/ayatana-scrollbar/+bug/876218 */
    "eclipse", /* https://bugs.launchpad.net/ayatana-scrollbar/+bug/769277 */
    "emacs", /* https://bugs.launchpad.net/ayatana-scrollbar/+bug/847940 */
    "emacs23", /* https://bugs.launchpad.net/ayatana-scrollbar/+bug/847940 */
    "firefox", /* https://bugs.launchpad.net/ayatana-scrollbar/+bug/847922 */
    "firefox-bin", /* https://bugs.launchpad.net/ayatana-scrollbar/+bug/847922 */
    "firefox-trunk", /* https://bugs.launchpad.net/ayatana-scrollbar/+bug/847922 */
    "gimp", /* https://bugs.launchpad.net/ayatana-scrollbar/+bug/803163 */
    "gimp-2.6", /* https://bugs.launchpad.net/ayatana-scrollbar/+bug/803163 */
    "gimp-2.7", /* https://bugs.launchpad.net/ayatana-scrollbar/+bug/803163 */
    "gimp-2.8", /* https://bugs.launchpad.net/ayatana-scrollbar/+bug/803163 */
    "gnucash", /* https://bugs.launchpad.net/ayatana-scrollbar/+bug/770304 */
    "gvim", /* https://bugs.launchpad.net/ayatana-scrollbar/+bug/847943 */
    "notes.bin", /* https://bugs.launchpad.net/ayatana-scrollbar/+bug/890986 */
    "soffice", /* https://bugs.launchpad.net/ayatana-scrollbar/+bug/847918 */
    "synaptic", /* https://bugs.launchpad.net/ayatana-scrollbar/+bug/755238 */
    "thunderbird-bin", /* https://bugs.launchpad.net/ayatana-scrollbar/+bug/847929 */
    "vinagre", /* https://bugs.launchpad.net/ayatana-scrollbar/+bug/847932 */
    "vmplayer", /* https://bugs.launchpad.net/ayatana-scrollbar/+bug/770625 */
    "vmware"/* https://bugs.launchpad.net/ayatana-scrollbar/+bug/770625 */
  };

  GModule *module;
  GSettings *interface_settings;
  gpointer func;
  gboolean settings_key;
  gint32 i;
  const gint32 nr_programs = G_N_ELEMENTS (blacklist);

  /* Read the gsettings key. */
  interface_settings = g_settings_new ("org.gnome.desktop.interface");
  settings_key = g_settings_get_boolean (interface_settings, "ubuntu-enable-overlay-scrollbars");
  g_object_unref (interface_settings);

  if (!settings_key)
    return TRUE;

  /* Black-list of symbols. */
  module = g_module_open (NULL, 0);
  /* https://bugs.launchpad.net/ayatana-scrollbar/+bug/847966 */
  if (g_module_symbol (module, "qt_startup_hook", &func))
    {
      g_module_close (module);
      return TRUE;
    }
  g_module_close (module);

  for (i = 0; i < nr_programs; i++)
    if (g_strcmp0 (blacklist[i], program) == 0)
      return TRUE;

  return FALSE;
}
