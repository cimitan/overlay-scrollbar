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

#ifndef __OS_VERSION_H__
#define __OS_VERSION_H__

#include <glib.h>

G_BEGIN_DECLS

/* The major version of scrollbar-overlay at compile time. */
#define OS_VERSION_MAJOR (0)

/* The minor version of scrollbar-overlay at compile time. */
#define OS_VERSION_MINOR (2)

/* The micro version of scrollbar-overlay at compile time. */
#define OS_VERSION_MICRO (13)

/* The nano version of scrollbar-overlay at compile time, actual releases have
 * 0, Bazaar trunk check-outs have 1, pre-release versions have [2-n]. */
#define OS_VERSION_NANO (0)

G_END_DECLS

#endif /* __OS_VERSION_H__ */
