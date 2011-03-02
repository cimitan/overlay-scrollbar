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

#ifndef __OS_UTILS_H__
#define __OS_UTILS_H__

#include <glib.h>

G_BEGIN_DECLS

gboolean os_utils_is_blacklisted (const gchar* program);

G_END_DECLS

#endif /* __OS_UTILS_H__ */
