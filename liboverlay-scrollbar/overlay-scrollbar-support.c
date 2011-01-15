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
#include <gdk/gdkx.h>
#include <X11/X.h>
#include <X11/Xlib.h>

#include "overlay-scrollbar-support.h"

void
os_present_gdk_window_with_timestamp (GtkWidget *widget,
                                      guint32 timestamp)
{
  os_present_window_with_timestamp (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                                    GDK_SCREEN_XSCREEN (gtk_widget_get_screen (widget)),
                                    GDK_WINDOW_XID (gtk_widget_get_window (widget)),
                                    timestamp);
}

void
os_present_window_with_timestamp (Display *default_display,
                                  Screen *screen,
                                  gint xid,
                                  guint32 timestamp)
{
  Display *display;
  Window root;
  XEvent xev;

  if (timestamp == 0)
    g_warning ("Received a timestamp of 0; window activation may not "
               "function properly.\n");

  display = DisplayOfScreen (screen);
  root = RootWindowOfScreen (screen);

  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = display;
  xev.xclient.window = xid;
  xev.xclient.message_type = gdk_x11_get_xatom_by_name ("_NET_ACTIVE_WINDOW");
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = 1;
  xev.xclient.data.l[1] = timestamp;
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;

  gdk_error_trap_push ();
  XSendEvent (display,
              root,
              False,
              SubstructureRedirectMask | SubstructureNotifyMask,
              &xev);
  XSync (default_display, False);
  gdk_error_trap_pop ();
}
