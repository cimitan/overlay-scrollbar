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

#ifndef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include "os-utils.h"

/* Public functions. */

gboolean
os_utils_is_blacklisted (const gchar* program)
{
  /* Black-list of program names retrieved with g_get_prgname(). */
  static const gchar *const blacklist[] = {
    "Fill me with blacklisted programs"
  };

  gint32 i;
  const gint32 nr_programs = G_N_ELEMENTS (blacklist);
  for (i = 0; i < nr_programs; i++)
    if (g_strcmp0 (blacklist[i], program) == 0)
      return TRUE;

  return FALSE;
}