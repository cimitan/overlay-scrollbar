/* overlay-scrollbar
 *
 * Copyright © 2011 Canonical Ltd
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
 * Authored by Andrea Cimitan <andrea.cimitan@canonical.com>
 */

#ifndef OVERLAY_SCROLLBAR_H
#define OVERLAY_SCROLLBAR_H

#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
  SCROLLBAR_MODE_NORMAL,
  SCROLLBAR_MODE_OVERLAY_AUTO,
  SCROLLBAR_MODE_OVERLAY_POINTER,
  SCROLLBAR_MODE_OVERLAY_TOUCH
} ScrollbarMode;

G_END_DECLS

#endif /* OVERLAY_SCROLLBAR_H */
