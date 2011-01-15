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

#include "overlay-scrollbar.h"
#include "overlay-scrollbar-cairo-support.h"

#define DEVELOPMENT_FLAG TRUE

#ifdef DEVELOPMENT_FLAG
#define DEBUG g_debug("%s()\n", __func__);
#else
#define DEBUG
#endif

enum {
  PROP_0,

  PROP_SLIDER,

  LAST_ARG
};

G_DEFINE_TYPE (OverlayScrollbar, overlay_scrollbar, GTK_TYPE_WINDOW);

#define OVERLAY_SCROLLBAR_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), OS_TYPE_OVERLAY_SCROLLBAR, OverlayScrollbarPrivate))

typedef struct _OverlayScrollbarPrivate OverlayScrollbarPrivate;

struct _OverlayScrollbarPrivate
{
  GtkWidget *slider;

  gboolean button_press_event;
  gboolean enter_notify_event;
  gboolean motion_notify_event;
  gboolean value_changed_event;
};

/* internal functions definitions */
static gboolean overlay_scrollbar_enter_notify_event (GtkWidget *widget,
                                                      GdkEventCrossing *event);
static gboolean overlay_scrollbar_expose (GtkWidget *widget,
                                          GdkEventExpose *event);
static gboolean slider_expose_event_cb (GtkWidget *widget,
                                        GdkEventExpose *event,
                                        gpointer user_data);
static GObject* overlay_scrollbar_constructor (GType type,
                                               guint n_construct_properties,
                                               GObjectConstructParam *construct_properties);
static void overlay_scrollbar_get_property (GObject *object,
                                            guint prop_id,
                                            GValue *value,
                                            GParamSpec *pspec);
static void overlay_scrollbar_map (GtkWidget *widget);
static void overlay_scrollbar_screen_changed (GtkWidget *widget,
                                              GdkScreen *old_screen);
static void overlay_scrollbar_set_property (GObject *object,
                                            guint prop_id,
                                            const GValue *value,
                                            GParamSpec *pspec);
static void slider_size_allocate_cb (GtkWidget *widget,
                                     GtkAllocation *allocation,
                                     gpointer user_data);

/* overlay scrollbar subclass */
static void
overlay_scrollbar_class_init (OverlayScrollbarClass *class)
{
  DEBUG
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS (class);
  widget_class = GTK_WIDGET_CLASS (class);

  widget_class->enter_notify_event = overlay_scrollbar_enter_notify_event;
  widget_class->expose_event       = overlay_scrollbar_expose;
  widget_class->map                = overlay_scrollbar_map;
  widget_class->screen_changed     = overlay_scrollbar_screen_changed;

  gobject_class->constructor       = overlay_scrollbar_constructor;
  gobject_class->get_property      = overlay_scrollbar_get_property;
  gobject_class->set_property      = overlay_scrollbar_set_property;

  g_type_class_add_private (gobject_class, sizeof (OverlayScrollbarPrivate));

  g_object_class_install_property (gobject_class,
                                   PROP_SLIDER,
                                   g_param_spec_object ("slider",
                                                        "Scrollbar Slider",
                                                        "The slider of the attached scrollbar",
                                                        GTK_TYPE_RANGE,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));
}

static GObject*
overlay_scrollbar_constructor (GType type,
                               guint n_construct_properties,
                               GObjectConstructParam *construct_properties)
{
  DEBUG
  GObject *object;

  object = G_OBJECT_CLASS (overlay_scrollbar_parent_class)->constructor (type,
                                                                         n_construct_properties,
                                                                         construct_properties);

  g_object_set (object, "type", GTK_WINDOW_POPUP, NULL);

  return object;
}

static gboolean
overlay_scrollbar_enter_notify_event (GtkWidget *widget,
                                      GdkEventCrossing *event)
{
  DEBUG
  OverlayScrollbarPrivate *priv;

  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (OVERLAY_SCROLLBAR (widget));

  priv->enter_notify_event = TRUE;

  return TRUE;
}

static gboolean
overlay_scrollbar_expose (GtkWidget *widget,
                          GdkEventExpose *event)
{
  DEBUG
  GtkAllocation allocation;
  GtkStateType state_type_down, state_type_up;
  cairo_pattern_t *pat;
  cairo_t *cr;
  gint x, y, width, height;

  state_type_down = GTK_STATE_NORMAL;
  state_type_up = GTK_STATE_NORMAL;

  gtk_widget_get_allocation (widget, &allocation);

  x = 0;
  y = 0;
  width = allocation.width;
  height = allocation.height;

  cr = gdk_cairo_create (gtk_widget_get_window (widget));

  /* clear the background */
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba (cr, 0, 0, 0, 0);
  cairo_paint (cr);

  /* drawing code */
  cairo_save (cr);
  cairo_translate (cr, 0.5, 0.5);
  width--;
  height--;

  cairo_set_line_width (cr, 1);
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

  os_cairo_draw_rounded_rect (cr, x, y, width, height, 6);
  pat = cairo_pattern_create_linear (x, y, width+x, y);
  cairo_pattern_add_color_stop_rgba (pat, 0.0, 0.95, 0.95, 0.95, 1.0);
  cairo_pattern_add_color_stop_rgba (pat, 1.0, 0.8, 0.8, 0.8, 1.0);
  cairo_set_source (cr, pat);
  cairo_pattern_destroy (pat);
  cairo_fill (cr);

  os_cairo_draw_rounded_rect (cr, x, y, width, height, 6);
  pat = cairo_pattern_create_linear (x, y, x, height+y);
/*  if ((handler_data != NULL && pointer_data != NULL) &&*/
/*      (handler->button_press_event && !handler->motion_notify_event))*/
/*    {*/
/*      if (pointer->y < height/2)*/
/*        {*/
/*          state_type_up = GTK_STATE_ACTIVE;*/
/*          cairo_pattern_add_color_stop_rgba (pat, 0.0, 0.8, 0.8, 0.8, 0.8);*/
/*          cairo_pattern_add_color_stop_rgba (pat, 0.49, 1.0, 1.0, 1.0, 0.0);*/
/*          cairo_pattern_add_color_stop_rgba (pat, 0.49, 0.8, 0.8, 0.8, 0.5);*/
/*          cairo_pattern_add_color_stop_rgba (pat, 1.0, 0.8, 0.8, 0.8, 0.8);*/
/*        }*/
/*      else*/
/*        {*/
/*          state_type_down = GTK_STATE_ACTIVE;*/
/*          cairo_pattern_add_color_stop_rgba (pat, 0.0, 1.0, 1.0, 1.0, 0.8);*/
/*          cairo_pattern_add_color_stop_rgba (pat, 0.49, 1.0, 1.0, 1.0, 0.0);*/
/*          cairo_pattern_add_color_stop_rgba (pat, 0.49, 0.8, 0.8, 0.8, 0.5);*/
/*          cairo_pattern_add_color_stop_rgba (pat, 1.0, 0.6, 0.6, 0.6, 0.8);*/
/*        }*/
/*    }*/
/*  else*/
    {
      cairo_pattern_add_color_stop_rgba (pat, 0.0, 1.0, 1.0, 1.0, 0.8);
      cairo_pattern_add_color_stop_rgba (pat, 0.49, 1.0, 1.0, 1.0, 0.0);
      cairo_pattern_add_color_stop_rgba (pat, 0.49, 0.8, 0.8, 0.8, 0.5);
      cairo_pattern_add_color_stop_rgba (pat, 1.0, 0.8, 0.8, 0.8, 0.8);
    }
  cairo_set_source (cr, pat);
  cairo_pattern_destroy (pat);
  cairo_fill_preserve (cr);

  cairo_set_source_rgba (cr, 0.6, 0.6, 0.6, 1.0);
  cairo_stroke (cr);

  os_cairo_draw_rounded_rect (cr, x+1, y+1, width-2, height-2, 7);
  cairo_set_source_rgba (cr, 1, 1, 1, 0.5);
  cairo_stroke (cr);

  cairo_move_to (cr, x+0.5, y-1+height/2);
  cairo_line_to (cr, width-0.5, y-1+height/2);
  cairo_set_source_rgba (cr, 0.6, 0.6, 0.6, 0.4);
  cairo_stroke (cr);

  cairo_move_to (cr, x+0.5, y+height/2);
  cairo_line_to (cr, width-0.5, y+height/2);
  cairo_set_source_rgba (cr, 1, 1, 1, 0.5);
  cairo_stroke (cr);

  cairo_restore (cr);

  gtk_paint_arrow (gtk_widget_get_style (widget),
                   gtk_widget_get_window (widget),
                   state_type_up,
                   GTK_SHADOW_IN,
                   NULL,
                   widget,
                   "arrow",
                   GTK_ARROW_UP,
                   FALSE,
                   4,
                   4,
                   width-8,
                   width-8);

  gtk_paint_arrow (gtk_widget_get_style (widget),
                   gtk_widget_get_window (widget),
                   state_type_down,
                   GTK_SHADOW_NONE,
                   NULL,
                   widget,
                   "arrow",
                   GTK_ARROW_DOWN,
                   FALSE,
                   4,
                   height-(width-8)-4,
                   width-8,
                   width-8);

  cairo_destroy (cr);

  return FALSE;
}

static void
overlay_scrollbar_get_property (GObject *object,
                                guint prop_id,
                                GValue *value,
                                GParamSpec *pspec)
{
  DEBUG
  OverlayScrollbar *scrollbar;
  OverlayScrollbarPrivate *priv;

  scrollbar = OVERLAY_SCROLLBAR (object);

  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (scrollbar);

  switch (prop_id)
    {
      case PROP_SLIDER:
        g_value_set_object (value, priv->slider);
        break;
    }
}

static void
overlay_scrollbar_init (OverlayScrollbar *scrollbar)
{
  DEBUG
  gtk_window_set_skip_pager_hint (GTK_WINDOW (scrollbar), TRUE);
  gtk_window_set_skip_taskbar_hint (GTK_WINDOW (scrollbar), TRUE);
  gtk_window_set_has_resize_grip (GTK_WINDOW (scrollbar), FALSE);
  gtk_window_set_decorated (GTK_WINDOW (scrollbar), FALSE);
  gtk_window_set_focus_on_map (GTK_WINDOW (scrollbar), FALSE);
  gtk_window_set_accept_focus (GTK_WINDOW (scrollbar), FALSE);
  gtk_widget_set_app_paintable (GTK_WIDGET (scrollbar), TRUE);
  gtk_widget_add_events (GTK_WIDGET (scrollbar), GDK_BUTTON_PRESS_MASK |
                                                 GDK_BUTTON_RELEASE_MASK |
                                                 GDK_POINTER_MOTION_MASK);

  overlay_scrollbar_screen_changed (GTK_WIDGET (scrollbar), NULL);
}

static void
overlay_scrollbar_map (GtkWidget *widget)
{
  DEBUG
  Display *display;
  GtkWidget *parent;
  OverlayScrollbarPrivate *priv;
  XWindowChanges changes;
  guint32 xid, xid_parent;
  int res;

  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (OVERLAY_SCROLLBAR (widget));

  parent = priv->slider;

  xid = GDK_WINDOW_XID (gtk_widget_get_window (widget));
  xid_parent = GDK_WINDOW_XID (gtk_widget_get_window (parent));
  display = GDK_WINDOW_XDISPLAY (gtk_widget_get_window (widget));

  changes.sibling = xid_parent;
  changes.stack_mode = Above;

/*  printf ("xid: %i, xid_parent: %i\n", xid, xid_parent);*/

  gdk_error_trap_push ();
  XConfigureWindow (display, xid,  CWSibling | CWStackMode, &changes);

  gdk_flush ();
  if ((res = gdk_error_trap_pop ()))
    g_warning ("Received X error: %d\n", res);

  GTK_WIDGET_CLASS (overlay_scrollbar_parent_class)->map (widget);
}

/**
 * overlay_scrollbar_new:
 * @slider: a pointer to the GtkRange slider to connect
 *
 * Creates a new overlay scrollbar.
 *
 * Returns: the new overlay scrollbar as a #GtkWidget
 */
GtkWidget*
overlay_scrollbar_new (GtkRange *slider)
{
  DEBUG
  return g_object_new (OS_TYPE_OVERLAY_SCROLLBAR , "slider", slider, NULL);
}

static void
overlay_scrollbar_screen_changed (GtkWidget *widget,
                                  GdkScreen *old_screen)
{
  DEBUG
  GdkScreen *screen;
  GdkColormap *colormap;

  screen = gtk_widget_get_screen (widget);
  colormap = gdk_screen_get_rgba_colormap (screen);

  if (colormap)
    gtk_widget_set_colormap (widget, colormap);
}

static void
overlay_scrollbar_set_property (GObject *object,
                                guint prop_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
  DEBUG
  OverlayScrollbar *scrollbar;
  OverlayScrollbarPrivate *priv;

  scrollbar = OVERLAY_SCROLLBAR (object);

  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (scrollbar);

  switch (prop_id)
    {
      case PROP_SLIDER:
        {
          priv->slider = g_value_get_object (value);
          g_signal_connect (G_OBJECT (priv->slider), "size-allocate",
                            G_CALLBACK (slider_size_allocate_cb), scrollbar);
          g_signal_connect (G_OBJECT (priv->slider), "expose-event",
                            G_CALLBACK (slider_expose_event_cb), scrollbar);
          break;
        }
    }
}

/**
 * overlay_scrollbar_set_slider:
 * @widget: an OverlayScrollbar widget
 * @slider: a pointer to a GtkScrollbar slider
 *
 * Sets the GtkScrollbar slider to control trough the OverlayScrollbar.
 *
 */
void
overlay_scrollbar_set_slider (GtkWidget *widget,
                              GtkWidget *slider)
{
  DEBUG
  OverlayScrollbarPrivate *priv;

  g_return_if_fail (OS_IS_OVERLAY_SCROLLBAR (widget) && GTK_RANGE (slider));

  priv = OVERLAY_SCROLLBAR_GET_PRIVATE (widget);

  priv->slider = slider;
}

static gboolean
slider_expose_event_cb (GtkWidget *widget,
                        GdkEventExpose *event,
                        gpointer user_data)
{
  DEBUG
  GtkAllocation allocation;
  gint x_pos, y_pos;

  gtk_widget_get_allocation (widget, &allocation);
  gdk_window_get_position (gtk_widget_get_window (widget), &x_pos, &y_pos);

  gtk_window_move (GTK_WINDOW (user_data), x_pos+allocation.x, y_pos+allocation.y);

  return TRUE;
}

static void
slider_size_allocate_cb (GtkWidget *widget,
                         GtkAllocation *allocation,
                         gpointer user_data)
{
  DEBUG
  OverlayScrollbar *scrollbar;

  scrollbar = OVERLAY_SCROLLBAR (user_data);

  gtk_window_set_default_size (GTK_WINDOW (scrollbar), allocation->width, allocation->height);
}
