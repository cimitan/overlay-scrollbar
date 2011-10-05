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

#include "os-private.h"

#if !defined(NDEBUG)
OsLogLevel threshold = OS_INFO;
#else
OsLogLevel threshold = OS_WARN;
#endif

/* Public functions. */

void
os_log_message (OsLogLevel level, const gchar *function, const gchar *file,
                gint32 line, const gchar *format, ...)
{
  static const gchar *prefix[3] = {
    "\033[37;01m", /* OS_INFO. */
    "\033[33;01m", /* OS_WARN. */
    "\033[31;01m"  /* OS_ERROR. */
  };
  gchar buffer[512];
  va_list args;

  va_start (args, format);
  vsnprintf (buffer, 512, format, args);
  va_end (args);

  fprintf (stderr, "%s%s\033[00m %s() %s %d\n", prefix[level], buffer, function,
           file, line);
}
