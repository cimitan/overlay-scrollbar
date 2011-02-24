/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"
#include <math.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "overlay-scrollbar.h"

/* Code copied from the original header. */

#define GTK_TYPE_TWEAKED_SCROLLED_WINDOW            (gtk_tweaked_scrolled_window_get_type ())
#define GTK_TWEAKED_SCROLLED_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_TWEAKED_SCROLLED_WINDOW, GtkTweakedScrolledWindow))
#define GTK_TWEAKED_SCROLLED_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_TWEAKED_SCROLLED_WINDOW, GtkTweakedScrolledWindowClass))
#define GTK_IS_TWEAKED_SCROLLED_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_TWEAKED_SCROLLED_WINDOW))
#define GTK_IS_TWEAKED_SCROLLED_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_TWEAKED_SCROLLED_WINDOW))
#define GTK_TWEAKED_SCROLLED_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_TWEAKED_SCROLLED_WINDOW, GtkTweakedScrolledWindowClass))

typedef struct _GtkTweakedScrolledWindow       GtkTweakedScrolledWindow;
typedef struct _GtkTweakedScrolledWindowClass  GtkTweakedScrolledWindowClass;

struct _GtkTweakedScrolledWindow
{
  GtkBin container;

  /*< public >*/
  GtkWidget *GSEAL (hscrollbar);
  GtkWidget *GSEAL (vscrollbar);

  /*< private >*/
  guint GSEAL (hscrollbar_policy)      : 2;
  guint GSEAL (vscrollbar_policy)      : 2;
  guint GSEAL (hscrollbar_visible)     : 1;
  guint GSEAL (vscrollbar_visible)     : 1;
  guint GSEAL (window_placement)       : 2;
  guint GSEAL (focus_out)              : 1;        /* Flag used by ::move-focus-out implementation */

  guint16 GSEAL (shadow_type);
};

struct _GtkTweakedScrolledWindowClass
{
  GtkBinClass parent_class;

  gint scrollbar_spacing;

  /* Action signals for keybindings. Do not connect to these signals
   */

  /* Unfortunately, GtkScrollType is deficient in that there is
   * no horizontal/vertical variants for GTK_SCROLL_START/END,
   * so we have to add an additional boolean flag.
   */
  gboolean (*scroll_child) (GtkTweakedScrolledWindow *scrolled_window,
                              GtkScrollType      scroll,
                            gboolean           horizontal);

  void (* move_focus_out) (GtkTweakedScrolledWindow *scrolled_window,
                           GtkDirectionType   direction);

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

/* Code copied and adapted from GTK+. */

#define P_
#define I_(string) g_intern_static_string (string)

#define GTK_PARAM_READABLE G_PARAM_READABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB
#define GTK_PARAM_WRITABLE G_PARAM_WRITABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB
#define GTK_PARAM_READWRITE G_PARAM_READWRITE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB

#define g_marshal_value_peek_boolean(v)  (v)->data[0].v_int
#define g_marshal_value_peek_enum(v)     (v)->data[0].v_long

#define _gtk_marshal_VOID__ENUM g_cclosure_marshal_VOID__ENUM

static void
_gtk_marshal_BOOLEAN__ENUM_BOOLEAN (GClosure     *closure,
                                    GValue       *return_value G_GNUC_UNUSED,
                                    guint         n_param_values,
                                    const GValue *param_values,
                                    gpointer      invocation_hint G_GNUC_UNUSED,
                                    gpointer      marshal_data)
{
  typedef gboolean (*GMarshalFunc_BOOLEAN__ENUM_BOOLEAN) (gpointer     data1,
                                                          gint         arg_1,
                                                          gboolean     arg_2,
                                                          gpointer     data2);
  register GMarshalFunc_BOOLEAN__ENUM_BOOLEAN callback;
  register GCClosure *cc = (GCClosure*) closure;
  register gpointer data1, data2;
  gboolean v_return;

  g_return_if_fail (return_value != NULL);
  g_return_if_fail (n_param_values == 3);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_BOOLEAN__ENUM_BOOLEAN) (marshal_data ? marshal_data : cc->callback);

  v_return = callback (data1,
                       g_marshal_value_peek_enum (param_values + 1),
                       g_marshal_value_peek_boolean (param_values + 2),
                       data2);

  g_value_set_boolean (return_value, v_return);
}

static GtkWidgetAuxInfo*
__gtk_widget_get_aux_info (GtkWidget *widget,
                           gboolean   create)
{
  static GQuark quark_aux_info = 0;
  GtkWidgetAuxInfo *aux_info;

  if (G_UNLIKELY (quark_aux_info == 0))
    quark_aux_info = g_quark_from_static_string ("gtk-aux-info");

  aux_info = g_object_get_qdata (G_OBJECT (widget), quark_aux_info);
  if (!aux_info && create)
    {
      aux_info = g_slice_new (GtkWidgetAuxInfo);

      aux_info->width = -1;
      aux_info->height = -1;
      aux_info->x = 0;
      aux_info->y = 0;
      aux_info->x_set = FALSE;
      aux_info->y_set = FALSE;
      g_object_set_qdata (G_OBJECT (widget), quark_aux_info, aux_info);
    }
  
  return aux_info;
}

static gdouble
__gtk_range_get_wheel_delta (GtkRange           *range,
                             GdkScrollDirection  direction)
{
  GtkAdjustment *adj = range->adjustment;
  gdouble delta;

  if (GTK_IS_SCROLLBAR (range))
    delta = pow (adj->page_size, 2.0 / 3.0);
  else
    delta = adj->step_increment * 2;
  
  if (direction == GDK_SCROLL_UP ||
      direction == GDK_SCROLL_LEFT)
    delta = - delta;
  
  if (range->inverted)
    delta = - delta;

  return delta;
}

static gint _gtk_tweaked_scrolled_window_get_scrollbar_spacing (GtkTweakedScrolledWindow *scrolled_window);

/* Code from the original gtkscrolledwindow adapted for the overlay. */

#define DEFAULT_SCROLLBAR_SPACING  3

typedef struct {
  gboolean window_placement_set;
  GtkCornerType real_window_placement;
} GtkTweakedScrolledWindowPrivate;

#define GTK_TWEAKED_SCROLLED_WINDOW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_TWEAKED_SCROLLED_WINDOW, GtkTweakedScrolledWindowPrivate))

enum {
  PROP_0,
  PROP_HADJUSTMENT,
  PROP_VADJUSTMENT,
  PROP_HSCROLLBAR_POLICY,
  PROP_VSCROLLBAR_POLICY,
  PROP_WINDOW_PLACEMENT,
  PROP_WINDOW_PLACEMENT_SET,
  PROP_SHADOW_TYPE
};

/* Signals */
enum
{
  SCROLL_CHILD,
  MOVE_FOCUS_OUT,
  LAST_SIGNAL
};

static void     gtk_tweaked_scrolled_window_destroy            (GtkObject         *object);
static void     gtk_tweaked_scrolled_window_set_property       (GObject           *object,
                                                        guint              prop_id,
                                                        const GValue      *value,
                                                        GParamSpec        *pspec);
static void     gtk_tweaked_scrolled_window_get_property       (GObject           *object,
                                                        guint              prop_id,
                                                        GValue            *value,
                                                        GParamSpec        *pspec);

static void     gtk_tweaked_scrolled_window_screen_changed     (GtkWidget         *widget,
                                                        GdkScreen         *previous_screen);
static gboolean gtk_tweaked_scrolled_window_expose             (GtkWidget         *widget,
                                                        GdkEventExpose    *event);
static void     gtk_tweaked_scrolled_window_size_request       (GtkWidget         *widget,
                                                        GtkRequisition    *requisition);
static void     gtk_tweaked_scrolled_window_size_allocate      (GtkWidget         *widget,
                                                        GtkAllocation     *allocation);
static gboolean gtk_tweaked_scrolled_window_scroll_event       (GtkWidget         *widget,
                                                        GdkEventScroll    *event);
static gboolean gtk_tweaked_scrolled_window_focus              (GtkWidget         *widget,
                                                        GtkDirectionType   direction);
static void     gtk_tweaked_scrolled_window_add                (GtkContainer      *container,
                                                        GtkWidget         *widget);
static void     gtk_tweaked_scrolled_window_remove             (GtkContainer      *container,
                                                        GtkWidget         *widget);
static void     gtk_tweaked_scrolled_window_forall             (GtkContainer      *container,
                                                        gboolean           include_internals,
                                                        GtkCallback        callback,
                                                        gpointer           callback_data);
static gboolean gtk_tweaked_scrolled_window_scroll_child       (GtkTweakedScrolledWindow *scrolled_window,
                                                        GtkScrollType      scroll,
                                                        gboolean           horizontal);
static void     gtk_tweaked_scrolled_window_move_focus_out     (GtkTweakedScrolledWindow *scrolled_window,
                                                        GtkDirectionType   direction_type);

static void     gtk_tweaked_scrolled_window_relative_allocation(GtkWidget         *widget,
                                                        GtkAllocation     *allocation);
static void     gtk_tweaked_scrolled_window_adjustment_changed (GtkAdjustment     *adjustment,
                                                        gpointer           data);

static void  gtk_tweaked_scrolled_window_update_real_placement (GtkTweakedScrolledWindow *scrolled_window);

static guint signals[LAST_SIGNAL] = {0};

G_DEFINE_TYPE (GtkTweakedScrolledWindow, gtk_tweaked_scrolled_window, GTK_TYPE_BIN)

static void
add_scroll_binding (GtkBindingSet  *binding_set,
                    guint           keyval,
                    GdkModifierType mask,
                    GtkScrollType   scroll,
                    gboolean        horizontal)
{
  guint keypad_keyval = keyval - GDK_Left + GDK_KP_Left;
  
  gtk_binding_entry_add_signal (binding_set, keyval, mask,
                                "scroll-child", 2,
                                GTK_TYPE_SCROLL_TYPE, scroll,
                                G_TYPE_BOOLEAN, horizontal);
  gtk_binding_entry_add_signal (binding_set, keypad_keyval, mask,
                                "scroll-child", 2,
                                GTK_TYPE_SCROLL_TYPE, scroll,
                                G_TYPE_BOOLEAN, horizontal);
}

static void
add_tab_bindings (GtkBindingSet    *binding_set,
                  GdkModifierType   modifiers,
                  GtkDirectionType  direction)
{
  gtk_binding_entry_add_signal (binding_set, GDK_Tab, modifiers,
                                "move-focus-out", 1,
                                GTK_TYPE_DIRECTION_TYPE, direction);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Tab, modifiers,
                                "move-focus-out", 1,
                                GTK_TYPE_DIRECTION_TYPE, direction);
}

static void
gtk_tweaked_scrolled_window_class_init (GtkTweakedScrolledWindowClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;
  GtkBindingSet *binding_set;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  container_class = (GtkContainerClass*) class;

  gobject_class->set_property = gtk_tweaked_scrolled_window_set_property;
  gobject_class->get_property = gtk_tweaked_scrolled_window_get_property;

  object_class->destroy = gtk_tweaked_scrolled_window_destroy;

  widget_class->screen_changed = gtk_tweaked_scrolled_window_screen_changed;
  widget_class->expose_event = gtk_tweaked_scrolled_window_expose;
  widget_class->size_request = gtk_tweaked_scrolled_window_size_request;
  widget_class->size_allocate = gtk_tweaked_scrolled_window_size_allocate;
  widget_class->scroll_event = gtk_tweaked_scrolled_window_scroll_event;
  widget_class->focus = gtk_tweaked_scrolled_window_focus;

  container_class->add = gtk_tweaked_scrolled_window_add;
  container_class->remove = gtk_tweaked_scrolled_window_remove;
  container_class->forall = gtk_tweaked_scrolled_window_forall;

  class->scrollbar_spacing = -1;

  class->scroll_child = gtk_tweaked_scrolled_window_scroll_child;
  class->move_focus_out = gtk_tweaked_scrolled_window_move_focus_out;
  
  g_object_class_install_property (gobject_class,
                                   PROP_HADJUSTMENT,
                                   g_param_spec_object ("hadjustment",
                                                        P_("Horizontal Adjustment"),
                                                        P_("The GtkAdjustment for the horizontal position"),
                                                        GTK_TYPE_ADJUSTMENT,
                                                        GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  g_object_class_install_property (gobject_class,
                                   PROP_VADJUSTMENT,
                                   g_param_spec_object ("vadjustment",
                                                        P_("Vertical Adjustment"),
                                                        P_("The GtkAdjustment for the vertical position"),
                                                        GTK_TYPE_ADJUSTMENT,
                                                        GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  g_object_class_install_property (gobject_class,
                                   PROP_HSCROLLBAR_POLICY,
                                   g_param_spec_enum ("hscrollbar-policy",
                                                      P_("Horizontal Scrollbar Policy"),
                                                      P_("When the horizontal scrollbar is displayed"),
                                                      GTK_TYPE_POLICY_TYPE,
                                                      GTK_POLICY_ALWAYS,
                                                      GTK_PARAM_READABLE | GTK_PARAM_WRITABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_VSCROLLBAR_POLICY,
                                   g_param_spec_enum ("vscrollbar-policy",
                                                      P_("Vertical Scrollbar Policy"),
                                                      P_("When the vertical scrollbar is displayed"),
                                                      GTK_TYPE_POLICY_TYPE,
                                                      GTK_POLICY_ALWAYS,
                                                      GTK_PARAM_READABLE | GTK_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class,
                                   PROP_WINDOW_PLACEMENT,
                                   g_param_spec_enum ("window-placement",
                                                      P_("Window Placement"),
                                                      P_("Where the contents are located with respect to the scrollbars. This property only takes effect if \"window-placement-set\" is TRUE."),
                                                      GTK_TYPE_CORNER_TYPE,
                                                      GTK_CORNER_TOP_LEFT,
                                                      GTK_PARAM_READABLE | GTK_PARAM_WRITABLE));
  
  /**
   * GtkTweakedScrolledWindow:window-placement-set:
   *
   * Whether "window-placement" should be used to determine the location 
   * of the contents with respect to the scrollbars. Otherwise, the 
   * "gtk-scrolled-window-placement" setting is used.
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
                                   PROP_WINDOW_PLACEMENT_SET,
                                   g_param_spec_boolean ("window-placement-set",
                                                            P_("Window Placement Set"),
                                                         P_("Whether \"window-placement\" should be used to determine the location of the contents with respect to the scrollbars."),
                                                         FALSE,
                                                         GTK_PARAM_READABLE | GTK_PARAM_WRITABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_SHADOW_TYPE,
                                   g_param_spec_enum ("shadow-type",
                                                      P_("Shadow Type"),
                                                      P_("Style of bevel around the contents"),
                                                      GTK_TYPE_SHADOW_TYPE,
                                                      GTK_SHADOW_NONE,
                                                      GTK_PARAM_READABLE | GTK_PARAM_WRITABLE));

  /**
   * GtkTweakedScrolledWindow:scrollbars-within-bevel:
   *
   * Whether to place scrollbars within the scrolled window's bevel.
   *
   * Since: 2.12
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_boolean ("scrollbars-within-bevel",
                                                                 P_("Scrollbars within bevel"),
                                                                 P_("Place scrollbars within the scrolled window's bevel"),
                                                                 FALSE,
                                                                 GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("scrollbar-spacing",
                                                             P_("Scrollbar spacing"),
                                                             P_("Number of pixels between the scrollbars and the scrolled window"),
                                                             0,
                                                             G_MAXINT,
                                                             DEFAULT_SCROLLBAR_SPACING,
                                                             GTK_PARAM_READABLE));

  /**
   * GtkSettings:gtk-scrolled-window-placement:
   *
   * Where the contents of scrolled windows are located with respect to the 
   * scrollbars, if not overridden by the scrolled window's own placement.
   *
   * Since: 2.10
   */
  gtk_settings_install_property (g_param_spec_enum ("gtk-scrolled-window-placement",
                                                    P_("Scrolled Window Placement"),
                                                    P_("Where the contents of scrolled windows are located with respect to the scrollbars, if not overridden by the scrolled window's own placement."),
                                                    GTK_TYPE_CORNER_TYPE,
                                                    GTK_CORNER_TOP_LEFT,
                                                    G_PARAM_READABLE | G_PARAM_WRITABLE));


  signals[SCROLL_CHILD] =
    g_signal_new (I_("scroll-child"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkTweakedScrolledWindowClass, scroll_child),
                  NULL, NULL,
                  _gtk_marshal_BOOLEAN__ENUM_BOOLEAN,
                  G_TYPE_BOOLEAN, 2,
                  GTK_TYPE_SCROLL_TYPE,
                  G_TYPE_BOOLEAN);
  signals[MOVE_FOCUS_OUT] =
    g_signal_new (I_("move-focus-out"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkTweakedScrolledWindowClass, move_focus_out),
                  NULL, NULL,
                  _gtk_marshal_VOID__ENUM,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_DIRECTION_TYPE);
  
  binding_set = gtk_binding_set_by_class (class);

  add_scroll_binding (binding_set, GDK_Left,  GDK_CONTROL_MASK, GTK_SCROLL_STEP_BACKWARD, TRUE);
  add_scroll_binding (binding_set, GDK_Right, GDK_CONTROL_MASK, GTK_SCROLL_STEP_FORWARD,  TRUE);
  add_scroll_binding (binding_set, GDK_Up,    GDK_CONTROL_MASK, GTK_SCROLL_STEP_BACKWARD, FALSE);
  add_scroll_binding (binding_set, GDK_Down,  GDK_CONTROL_MASK, GTK_SCROLL_STEP_FORWARD,  FALSE);

  add_scroll_binding (binding_set, GDK_Page_Up,   GDK_CONTROL_MASK, GTK_SCROLL_PAGE_BACKWARD, TRUE);
  add_scroll_binding (binding_set, GDK_Page_Down, GDK_CONTROL_MASK, GTK_SCROLL_PAGE_FORWARD,  TRUE);
  add_scroll_binding (binding_set, GDK_Page_Up,   0,                GTK_SCROLL_PAGE_BACKWARD, FALSE);
  add_scroll_binding (binding_set, GDK_Page_Down, 0,                GTK_SCROLL_PAGE_FORWARD,  FALSE);

  add_scroll_binding (binding_set, GDK_Home, GDK_CONTROL_MASK, GTK_SCROLL_START, TRUE);
  add_scroll_binding (binding_set, GDK_End,  GDK_CONTROL_MASK, GTK_SCROLL_END,   TRUE);
  add_scroll_binding (binding_set, GDK_Home, 0,                GTK_SCROLL_START, FALSE);
  add_scroll_binding (binding_set, GDK_End,  0,                GTK_SCROLL_END,   FALSE);

  add_tab_bindings (binding_set, GDK_CONTROL_MASK, GTK_DIR_TAB_FORWARD);
  add_tab_bindings (binding_set, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_DIR_TAB_BACKWARD);

  g_type_class_add_private (class, sizeof (GtkTweakedScrolledWindowPrivate));
}

static void
gtk_tweaked_scrolled_window_init (GtkTweakedScrolledWindow *scrolled_window)
{
  gtk_widget_set_has_window (GTK_WIDGET (scrolled_window), FALSE);
  gtk_widget_set_can_focus (GTK_WIDGET (scrolled_window), TRUE);

  scrolled_window->hscrollbar = NULL;
  scrolled_window->vscrollbar = NULL;
  scrolled_window->hscrollbar_policy = GTK_POLICY_ALWAYS;
  scrolled_window->vscrollbar_policy = GTK_POLICY_ALWAYS;
  scrolled_window->hscrollbar_visible = FALSE;
  scrolled_window->vscrollbar_visible = FALSE;
  scrolled_window->focus_out = FALSE;
  scrolled_window->window_placement = GTK_CORNER_TOP_LEFT;
  gtk_tweaked_scrolled_window_update_real_placement (scrolled_window);
}

/**
 * gtk_tweaked_scrolled_window_new:
 * @hadjustment: (allow-none): horizontal adjustment
 * @vadjustment: (allow-none): vertical adjustment
 *
 * Creates a new scrolled window.
 *
 * The two arguments are the scrolled window's adjustments; these will be
 * shared with the scrollbars and the child widget to keep the bars in sync 
 * with the child. Usually you want to pass %NULL for the adjustments, which 
 * will cause the scrolled window to create them for you.
 *
 * Returns: a new scrolled window
 */
static GtkWidget*
gtk_tweaked_scrolled_window_new (GtkAdjustment *hadjustment,
                         GtkAdjustment *vadjustment)
{
  GtkWidget *scrolled_window;

  if (hadjustment)
    g_return_val_if_fail (GTK_IS_ADJUSTMENT (hadjustment), NULL);

  if (vadjustment)
    g_return_val_if_fail (GTK_IS_ADJUSTMENT (vadjustment), NULL);

  scrolled_window = g_object_new (GTK_TYPE_TWEAKED_SCROLLED_WINDOW,
                                    "hadjustment", hadjustment,
                                    "vadjustment", vadjustment,
                                    NULL);

  return scrolled_window;
}

/**
 * gtk_tweaked_scrolled_window_set_hadjustment:
 * @scrolled_window: a #GtkTweakedScrolledWindow
 * @hadjustment: horizontal scroll adjustment
 *
 * Sets the #GtkAdjustment for the horizontal scrollbar.
 */
static void
gtk_tweaked_scrolled_window_set_hadjustment (GtkTweakedScrolledWindow *scrolled_window,
                                     GtkAdjustment     *hadjustment)
{
  GtkBin *bin;

  g_return_if_fail (GTK_IS_TWEAKED_SCROLLED_WINDOW (scrolled_window));
  if (hadjustment)
    g_return_if_fail (GTK_IS_ADJUSTMENT (hadjustment));
  else
    hadjustment = (GtkAdjustment*) g_object_new (GTK_TYPE_ADJUSTMENT, NULL);

  bin = GTK_BIN (scrolled_window);

  if (!scrolled_window->hscrollbar)
    {
      gtk_widget_push_composite_child ();
      scrolled_window->hscrollbar = gtk_hscrollbar_new (hadjustment);
      gtk_widget_set_composite_name (scrolled_window->hscrollbar, "hscrollbar");
      gtk_widget_pop_composite_child ();

      gtk_widget_set_parent (scrolled_window->hscrollbar, GTK_WIDGET (scrolled_window));
      g_object_ref (scrolled_window->hscrollbar);
      gtk_widget_show (scrolled_window->hscrollbar);
    }
  else
    {
      GtkAdjustment *old_adjustment;
      
      old_adjustment = gtk_range_get_adjustment (GTK_RANGE (scrolled_window->hscrollbar));
      if (old_adjustment == hadjustment)
        return;

      g_signal_handlers_disconnect_by_func (old_adjustment,
                                            gtk_tweaked_scrolled_window_adjustment_changed,
                                            scrolled_window);
      gtk_range_set_adjustment (GTK_RANGE (scrolled_window->hscrollbar),
                                hadjustment);
    }
  hadjustment = gtk_range_get_adjustment (GTK_RANGE (scrolled_window->hscrollbar));
  g_signal_connect (hadjustment,
                    "changed",
                    G_CALLBACK (gtk_tweaked_scrolled_window_adjustment_changed),
                    scrolled_window);
  gtk_tweaked_scrolled_window_adjustment_changed (hadjustment, scrolled_window);
  
  if (bin->child)
    gtk_widget_set_scroll_adjustments (bin->child,
                                       gtk_range_get_adjustment (GTK_RANGE (scrolled_window->hscrollbar)),
                                       gtk_range_get_adjustment (GTK_RANGE (scrolled_window->vscrollbar)));

  g_object_notify (G_OBJECT (scrolled_window), "hadjustment");
}

/**
 * gtk_tweaked_scrolled_window_set_vadjustment:
 * @scrolled_window: a #GtkTweakedScrolledWindow
 * @vadjustment: vertical scroll adjustment
 *
 * Sets the #GtkAdjustment for the vertical scrollbar.
 */
static void
gtk_tweaked_scrolled_window_set_vadjustment (GtkTweakedScrolledWindow *scrolled_window,
                                     GtkAdjustment     *vadjustment)
{
  GtkBin *bin;

  g_return_if_fail (GTK_IS_TWEAKED_SCROLLED_WINDOW (scrolled_window));
  if (vadjustment)
    g_return_if_fail (GTK_IS_ADJUSTMENT (vadjustment));
  else
    vadjustment = (GtkAdjustment*) g_object_new (GTK_TYPE_ADJUSTMENT, NULL);

  bin = GTK_BIN (scrolled_window);

  if (!scrolled_window->vscrollbar)
    {
      gtk_widget_push_composite_child ();
      scrolled_window->vscrollbar = gtk_vscrollbar_new (vadjustment);
      gtk_widget_set_composite_name (scrolled_window->vscrollbar, "vscrollbar");
      gtk_widget_pop_composite_child ();

      gtk_widget_set_parent (scrolled_window->vscrollbar, GTK_WIDGET (scrolled_window));
      g_object_ref (scrolled_window->vscrollbar);
      gtk_widget_show (scrolled_window->vscrollbar);
    }
  else
    {
      GtkAdjustment *old_adjustment;
      
      old_adjustment = gtk_range_get_adjustment (GTK_RANGE (scrolled_window->vscrollbar));
      if (old_adjustment == vadjustment)
        return;

      g_signal_handlers_disconnect_by_func (old_adjustment,
                                            gtk_tweaked_scrolled_window_adjustment_changed,
                                            scrolled_window);
      gtk_range_set_adjustment (GTK_RANGE (scrolled_window->vscrollbar),
                                vadjustment);
    }
  vadjustment = gtk_range_get_adjustment (GTK_RANGE (scrolled_window->vscrollbar));
  g_signal_connect (vadjustment,
                    "changed",
                    G_CALLBACK (gtk_tweaked_scrolled_window_adjustment_changed),
                    scrolled_window);
  gtk_tweaked_scrolled_window_adjustment_changed (vadjustment, scrolled_window);

  if (bin->child)
    gtk_widget_set_scroll_adjustments (bin->child,
                                       gtk_range_get_adjustment (GTK_RANGE (scrolled_window->hscrollbar)),
                                       gtk_range_get_adjustment (GTK_RANGE (scrolled_window->vscrollbar)));

  g_object_notify (G_OBJECT (scrolled_window), "vadjustment");
}

/**
 * gtk_tweaked_scrolled_window_get_hadjustment:
 * @scrolled_window: a #GtkTweakedScrolledWindow
 *
 * Returns the horizontal scrollbar's adjustment, used to connect the
 * horizontal scrollbar to the child widget's horizontal scroll
 * functionality.
 *
 * Returns: (transfer none): the horizontal #GtkAdjustment
 */
static GtkAdjustment*
gtk_tweaked_scrolled_window_get_hadjustment (GtkTweakedScrolledWindow *scrolled_window)
{
  g_return_val_if_fail (GTK_IS_TWEAKED_SCROLLED_WINDOW (scrolled_window), NULL);

  return (scrolled_window->hscrollbar ?
          gtk_range_get_adjustment (GTK_RANGE (scrolled_window->hscrollbar)) :
          NULL);
}

/**
 * gtk_tweaked_scrolled_window_get_vadjustment:
 * @scrolled_window: a #GtkTweakedScrolledWindow
 * 
 * Returns the vertical scrollbar's adjustment, used to connect the
 * vertical scrollbar to the child widget's vertical scroll functionality.
 * 
 * Returns: (transfer none): the vertical #GtkAdjustment
 */
static GtkAdjustment*
gtk_tweaked_scrolled_window_get_vadjustment (GtkTweakedScrolledWindow *scrolled_window)
{
  g_return_val_if_fail (GTK_IS_TWEAKED_SCROLLED_WINDOW (scrolled_window), NULL);

  return (scrolled_window->vscrollbar ?
          gtk_range_get_adjustment (GTK_RANGE (scrolled_window->vscrollbar)) :
          NULL);
}

/**
 * gtk_tweaked_scrolled_window_get_hscrollbar:
 * @scrolled_window: a #GtkTweakedScrolledWindow
 *
 * Returns the horizontal scrollbar of @scrolled_window.
 *
 * Returns: (transfer none): the horizontal scrollbar of the scrolled window,
 *     or %NULL if it does not have one.
 *
 * Since: 2.8
 */
static GtkWidget*
gtk_tweaked_scrolled_window_get_hscrollbar (GtkTweakedScrolledWindow *scrolled_window)
{
  g_return_val_if_fail (GTK_IS_TWEAKED_SCROLLED_WINDOW (scrolled_window), NULL);
  
  return scrolled_window->hscrollbar;
}

/**
 * gtk_tweaked_scrolled_window_get_vscrollbar:
 * @scrolled_window: a #GtkTweakedScrolledWindow
 * 
 * Returns the vertical scrollbar of @scrolled_window.
 *
 * Returns: (transfer none): the vertical scrollbar of the scrolled window,
 *     or %NULL if it does not have one.
 *
 * Since: 2.8
 */
static GtkWidget*
gtk_tweaked_scrolled_window_get_vscrollbar (GtkTweakedScrolledWindow *scrolled_window)
{
  g_return_val_if_fail (GTK_IS_TWEAKED_SCROLLED_WINDOW (scrolled_window), NULL);

  return scrolled_window->vscrollbar;
}

/**
 * gtk_tweaked_scrolled_window_set_policy:
 * @scrolled_window: a #GtkTweakedScrolledWindow
 * @hscrollbar_policy: policy for horizontal bar
 * @vscrollbar_policy: policy for vertical bar
 * 
 * Sets the scrollbar policy for the horizontal and vertical scrollbars.
 *
 * The policy determines when the scrollbar should appear; it is a value
 * from the #GtkPolicyType enumeration. If %GTK_POLICY_ALWAYS, the
 * scrollbar is always present; if %GTK_POLICY_NEVER, the scrollbar is
 * never present; if %GTK_POLICY_AUTOMATIC, the scrollbar is present only
 * if needed (that is, if the slider part of the bar would be smaller
 * than the trough - the display is larger than the page size).
 */
static void
gtk_tweaked_scrolled_window_set_policy (GtkTweakedScrolledWindow *scrolled_window,
                                GtkPolicyType      hscrollbar_policy,
                                GtkPolicyType      vscrollbar_policy)
{
  GObject *object = G_OBJECT (scrolled_window);
  
  g_return_if_fail (GTK_IS_TWEAKED_SCROLLED_WINDOW (scrolled_window));

  if ((scrolled_window->hscrollbar_policy != hscrollbar_policy) ||
      (scrolled_window->vscrollbar_policy != vscrollbar_policy))
    {
      scrolled_window->hscrollbar_policy = hscrollbar_policy;
      scrolled_window->vscrollbar_policy = vscrollbar_policy;

      gtk_widget_queue_resize (GTK_WIDGET (scrolled_window));

      g_object_freeze_notify (object);
      g_object_notify (object, "hscrollbar-policy");
      g_object_notify (object, "vscrollbar-policy");
      g_object_thaw_notify (object);
    }
}

/**
 * gtk_tweaked_scrolled_window_get_policy:
 * @scrolled_window: a #GtkTweakedScrolledWindow
 * @hscrollbar_policy: location to store the policy for the horizontal 
 *     scrollbar, or %NULL.
 * @vscrollbar_policy: location to store the policy for the vertical
 *     scrollbar, or %NULL.
 * 
 * Retrieves the current policy values for the horizontal and vertical
 * scrollbars. See gtk_tweaked_scrolled_window_set_policy().
 */
static void
gtk_tweaked_scrolled_window_get_policy (GtkTweakedScrolledWindow *scrolled_window,
                                GtkPolicyType     *hscrollbar_policy,
                                GtkPolicyType     *vscrollbar_policy)
{
  g_return_if_fail (GTK_IS_TWEAKED_SCROLLED_WINDOW (scrolled_window));

  if (hscrollbar_policy)
    *hscrollbar_policy = scrolled_window->hscrollbar_policy;
  if (vscrollbar_policy)
    *vscrollbar_policy = scrolled_window->vscrollbar_policy;
}

static void
gtk_tweaked_scrolled_window_update_real_placement (GtkTweakedScrolledWindow *scrolled_window)
{
  GtkTweakedScrolledWindowPrivate *priv = GTK_TWEAKED_SCROLLED_WINDOW_GET_PRIVATE (scrolled_window);
  GtkSettings *settings;

  settings = gtk_widget_get_settings (GTK_WIDGET (scrolled_window));

  if (priv->window_placement_set || settings == NULL)
    priv->real_window_placement = scrolled_window->window_placement;
  else
    g_object_get (settings,
                  "gtk-scrolled-window-placement",
                  &priv->real_window_placement,
                  NULL);
}

static void
gtk_tweaked_scrolled_window_set_placement_internal (GtkTweakedScrolledWindow *scrolled_window,
                                            GtkCornerType      window_placement)
{
  if (scrolled_window->window_placement != window_placement)
    {
      scrolled_window->window_placement = window_placement;

      gtk_tweaked_scrolled_window_update_real_placement (scrolled_window);
      gtk_widget_queue_resize (GTK_WIDGET (scrolled_window));
      
      g_object_notify (G_OBJECT (scrolled_window), "window-placement");
    }
}

static void
gtk_tweaked_scrolled_window_set_placement_set (GtkTweakedScrolledWindow *scrolled_window,
                                       gboolean           placement_set,
                                       gboolean           emit_resize)
{
  GtkTweakedScrolledWindowPrivate *priv = GTK_TWEAKED_SCROLLED_WINDOW_GET_PRIVATE (scrolled_window);

  if (priv->window_placement_set != placement_set)
    {
      priv->window_placement_set = placement_set;

      gtk_tweaked_scrolled_window_update_real_placement (scrolled_window);
      if (emit_resize)
        gtk_widget_queue_resize (GTK_WIDGET (scrolled_window));

      g_object_notify (G_OBJECT (scrolled_window), "window-placement-set");
    }
}

/**
 * gtk_tweaked_scrolled_window_set_placement:
 * @scrolled_window: a #GtkTweakedScrolledWindow
 * @window_placement: position of the child window
 *
 * Sets the placement of the contents with respect to the scrollbars
 * for the scrolled window.
 * 
 * The default is %GTK_CORNER_TOP_LEFT, meaning the child is
 * in the top left, with the scrollbars underneath and to the right.
 * Other values in #GtkCornerType are %GTK_CORNER_TOP_RIGHT,
 * %GTK_CORNER_BOTTOM_LEFT, and %GTK_CORNER_BOTTOM_RIGHT.
 *
 * See also gtk_tweaked_scrolled_window_get_placement() and
 * gtk_tweaked_scrolled_window_unset_placement().
 */
static void
gtk_tweaked_scrolled_window_set_placement (GtkTweakedScrolledWindow *scrolled_window,
                                   GtkCornerType      window_placement)
{
  g_return_if_fail (GTK_IS_TWEAKED_SCROLLED_WINDOW (scrolled_window));

  gtk_tweaked_scrolled_window_set_placement_set (scrolled_window, TRUE, FALSE);
  gtk_tweaked_scrolled_window_set_placement_internal (scrolled_window, window_placement);
}

/**
 * gtk_tweaked_scrolled_window_get_placement:
 * @scrolled_window: a #GtkTweakedScrolledWindow
 *
 * Gets the placement of the contents with respect to the scrollbars
 * for the scrolled window. See gtk_tweaked_scrolled_window_set_placement().
 *
 * Return value: the current placement value.
 *
 * See also gtk_tweaked_scrolled_window_set_placement() and
 * gtk_tweaked_scrolled_window_unset_placement().
 **/
static GtkCornerType
gtk_tweaked_scrolled_window_get_placement (GtkTweakedScrolledWindow *scrolled_window)
{
  g_return_val_if_fail (GTK_IS_TWEAKED_SCROLLED_WINDOW (scrolled_window), GTK_CORNER_TOP_LEFT);

  return scrolled_window->window_placement;
}

/**
 * gtk_tweaked_scrolled_window_unset_placement:
 * @scrolled_window: a #GtkTweakedScrolledWindow
 *
 * Unsets the placement of the contents with respect to the scrollbars
 * for the scrolled window. If no window placement is set for a scrolled
 * window, it obeys the "gtk-scrolled-window-placement" XSETTING.
 *
 * See also gtk_tweaked_scrolled_window_set_placement() and
 * gtk_tweaked_scrolled_window_get_placement().
 *
 * Since: 2.10
 **/
static void
gtk_tweaked_scrolled_window_unset_placement (GtkTweakedScrolledWindow *scrolled_window)
{
  GtkTweakedScrolledWindowPrivate *priv = GTK_TWEAKED_SCROLLED_WINDOW_GET_PRIVATE (scrolled_window);

  g_return_if_fail (GTK_IS_TWEAKED_SCROLLED_WINDOW (scrolled_window));

  if (priv->window_placement_set)
    {
      priv->window_placement_set = FALSE;

      gtk_widget_queue_resize (GTK_WIDGET (scrolled_window));

      g_object_notify (G_OBJECT (scrolled_window), "window-placement-set");
    }
}

/**
 * gtk_tweaked_scrolled_window_set_shadow_type:
 * @scrolled_window: a #GtkTweakedScrolledWindow
 * @type: kind of shadow to draw around scrolled window contents
 *
 * Changes the type of shadow drawn around the contents of
 * @scrolled_window.
 * 
 **/
static void
gtk_tweaked_scrolled_window_set_shadow_type (GtkTweakedScrolledWindow *scrolled_window,
                                     GtkShadowType      type)
{
  g_return_if_fail (GTK_IS_TWEAKED_SCROLLED_WINDOW (scrolled_window));
  g_return_if_fail (type >= GTK_SHADOW_NONE && type <= GTK_SHADOW_ETCHED_OUT);
  
  if (scrolled_window->shadow_type != type)
    {
      scrolled_window->shadow_type = type;

      if (gtk_widget_is_drawable (GTK_WIDGET (scrolled_window)))
        gtk_widget_queue_draw (GTK_WIDGET (scrolled_window));

      gtk_widget_queue_resize (GTK_WIDGET (scrolled_window));

      g_object_notify (G_OBJECT (scrolled_window), "shadow-type");
    }
}

/**
 * gtk_tweaked_scrolled_window_get_shadow_type:
 * @scrolled_window: a #GtkTweakedScrolledWindow
 *
 * Gets the shadow type of the scrolled window. See 
 * gtk_tweaked_scrolled_window_set_shadow_type().
 *
 * Return value: the current shadow type
 **/
static GtkShadowType
gtk_tweaked_scrolled_window_get_shadow_type (GtkTweakedScrolledWindow *scrolled_window)
{
  g_return_val_if_fail (GTK_IS_TWEAKED_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_NONE);

  return scrolled_window->shadow_type;
}

static void
gtk_tweaked_scrolled_window_destroy (GtkObject *object)
{
  GtkTweakedScrolledWindow *scrolled_window = GTK_TWEAKED_SCROLLED_WINDOW (object);

  if (scrolled_window->hscrollbar)
    {
      g_signal_handlers_disconnect_by_func (gtk_range_get_adjustment (GTK_RANGE (scrolled_window->hscrollbar)),
                                            gtk_tweaked_scrolled_window_adjustment_changed,
                                            scrolled_window);
      gtk_widget_unparent (scrolled_window->hscrollbar);
      gtk_widget_destroy (scrolled_window->hscrollbar);
      g_object_unref (scrolled_window->hscrollbar);
      scrolled_window->hscrollbar = NULL;
    }
  if (scrolled_window->vscrollbar)
    {
      g_signal_handlers_disconnect_by_func (gtk_range_get_adjustment (GTK_RANGE (scrolled_window->vscrollbar)),
                                            gtk_tweaked_scrolled_window_adjustment_changed,
                                            scrolled_window);
      gtk_widget_unparent (scrolled_window->vscrollbar);
      gtk_widget_destroy (scrolled_window->vscrollbar);
      g_object_unref (scrolled_window->vscrollbar);
      scrolled_window->vscrollbar = NULL;
    }

  GTK_OBJECT_CLASS (gtk_tweaked_scrolled_window_parent_class)->destroy (object);
}

static void
gtk_tweaked_scrolled_window_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GtkTweakedScrolledWindow *scrolled_window = GTK_TWEAKED_SCROLLED_WINDOW (object);
  
  switch (prop_id)
    {
    case PROP_HADJUSTMENT:
      gtk_tweaked_scrolled_window_set_hadjustment (scrolled_window,
                                           g_value_get_object (value));
      break;
    case PROP_VADJUSTMENT:
      gtk_tweaked_scrolled_window_set_vadjustment (scrolled_window,
                                           g_value_get_object (value));
      break;
    case PROP_HSCROLLBAR_POLICY:
      gtk_tweaked_scrolled_window_set_policy (scrolled_window,
                                      g_value_get_enum (value),
                                      scrolled_window->vscrollbar_policy);
      break;
    case PROP_VSCROLLBAR_POLICY:
      gtk_tweaked_scrolled_window_set_policy (scrolled_window,
                                      scrolled_window->hscrollbar_policy,
                                      g_value_get_enum (value));
      break;
    case PROP_WINDOW_PLACEMENT:
      gtk_tweaked_scrolled_window_set_placement_internal (scrolled_window,
                                                        g_value_get_enum (value));
      break;
    case PROP_WINDOW_PLACEMENT_SET:
      gtk_tweaked_scrolled_window_set_placement_set (scrolled_window,
                                                   g_value_get_boolean (value),
                                             TRUE);
      break;
    case PROP_SHADOW_TYPE:
      gtk_tweaked_scrolled_window_set_shadow_type (scrolled_window,
                                           g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_tweaked_scrolled_window_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GtkTweakedScrolledWindow *scrolled_window = GTK_TWEAKED_SCROLLED_WINDOW (object);
  GtkTweakedScrolledWindowPrivate *priv = GTK_TWEAKED_SCROLLED_WINDOW_GET_PRIVATE (scrolled_window);
  
  switch (prop_id)
    {
    case PROP_HADJUSTMENT:
      g_value_set_object (value,
                          G_OBJECT (gtk_tweaked_scrolled_window_get_hadjustment (scrolled_window)));
      break;
    case PROP_VADJUSTMENT:
      g_value_set_object (value,
                          G_OBJECT (gtk_tweaked_scrolled_window_get_vadjustment (scrolled_window)));
      break;
    case PROP_HSCROLLBAR_POLICY:
      g_value_set_enum (value, scrolled_window->hscrollbar_policy);
      break;
    case PROP_VSCROLLBAR_POLICY:
      g_value_set_enum (value, scrolled_window->vscrollbar_policy);
      break;
    case PROP_WINDOW_PLACEMENT:
      g_value_set_enum (value, scrolled_window->window_placement);
      break;
    case PROP_WINDOW_PLACEMENT_SET:
      g_value_set_boolean (value, priv->window_placement_set);
      break;
    case PROP_SHADOW_TYPE:
      g_value_set_enum (value, scrolled_window->shadow_type);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
traverse_container (GtkWidget *widget,
                    gpointer   data)
{
  if (GTK_IS_TWEAKED_SCROLLED_WINDOW (widget))
    {
      gtk_tweaked_scrolled_window_update_real_placement (GTK_TWEAKED_SCROLLED_WINDOW (widget));
      gtk_widget_queue_resize (widget);
    }
  else if (GTK_IS_CONTAINER (widget))
    gtk_container_forall (GTK_CONTAINER (widget), traverse_container, NULL);
}

static void
gtk_tweaked_scrolled_window_settings_changed (GtkSettings *settings)
{
  GList *list, *l;

  list = gtk_window_list_toplevels ();

  for (l = list; l; l = l->next)
    gtk_container_forall (GTK_CONTAINER (l->data), 
                          traverse_container, NULL);

  g_list_free (list);
}

static void
gtk_tweaked_scrolled_window_screen_changed (GtkWidget *widget,
                                    GdkScreen *previous_screen)
{
  GtkSettings *settings;
  guint window_placement_connection;

  gtk_tweaked_scrolled_window_update_real_placement (GTK_TWEAKED_SCROLLED_WINDOW (widget));

  if (!gtk_widget_has_screen (widget))
    return;

  settings = gtk_widget_get_settings (widget);

  window_placement_connection = 
    GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (settings), 
                                         "gtk-scrolled-window-connection"));
  
  if (window_placement_connection)
    return;

  window_placement_connection =
    g_signal_connect (settings, "notify::gtk-scrolled-window-placement",
                      G_CALLBACK (gtk_tweaked_scrolled_window_settings_changed), NULL);
  g_object_set_data (G_OBJECT (settings), 
                     I_("gtk-scrolled-window-connection"),
                     GUINT_TO_POINTER (window_placement_connection));
}

static void
gtk_tweaked_scrolled_window_paint (GtkWidget    *widget,
                           GdkRectangle *area)
{
  GtkTweakedScrolledWindow *scrolled_window = GTK_TWEAKED_SCROLLED_WINDOW (widget);

  if (scrolled_window->shadow_type != GTK_SHADOW_NONE)
    {
      GtkAllocation relative_allocation;
      gboolean scrollbars_within_bevel;

      gtk_widget_style_get (widget, "scrollbars-within-bevel", &scrollbars_within_bevel, NULL);
      
      if (!scrollbars_within_bevel)
        {
          gtk_tweaked_scrolled_window_relative_allocation (widget, &relative_allocation);

          relative_allocation.x -= widget->style->xthickness;
          relative_allocation.y -= widget->style->ythickness;
          relative_allocation.width += 2 * widget->style->xthickness;
          relative_allocation.height += 2 * widget->style->ythickness;
        }
      else
        {
          GtkContainer *container = GTK_CONTAINER (widget);

          relative_allocation.x = container->border_width;
          relative_allocation.y = container->border_width;
          relative_allocation.width = widget->allocation.width - 2 * container->border_width;
          relative_allocation.height = widget->allocation.height - 2 * container->border_width;
        }

      gtk_paint_shadow (widget->style, widget->window,
                        GTK_STATE_NORMAL, scrolled_window->shadow_type,
                        area, widget, "scrolled_window",
                        widget->allocation.x + relative_allocation.x,
                        widget->allocation.y + relative_allocation.y,
                        relative_allocation.width,
                        relative_allocation.height);
    }
}

static gboolean
gtk_tweaked_scrolled_window_expose (GtkWidget      *widget,
                            GdkEventExpose *event)
{
  if (gtk_widget_is_drawable (widget))
    {
      gtk_tweaked_scrolled_window_paint (widget, &event->area);

      GTK_WIDGET_CLASS (gtk_tweaked_scrolled_window_parent_class)->expose_event (widget, event);
    }

  return FALSE;
}

static void
gtk_tweaked_scrolled_window_forall (GtkContainer *container,
                            gboolean          include_internals,
                            GtkCallback   callback,
                            gpointer      callback_data)
{
  g_return_if_fail (GTK_IS_TWEAKED_SCROLLED_WINDOW (container));
  g_return_if_fail (callback != NULL);

  GTK_CONTAINER_CLASS (gtk_tweaked_scrolled_window_parent_class)->forall (container,
                                              include_internals,
                                              callback,
                                              callback_data);
  if (include_internals)
    {
      GtkTweakedScrolledWindow *scrolled_window;

      scrolled_window = GTK_TWEAKED_SCROLLED_WINDOW (container);
      
      if (scrolled_window->vscrollbar)
        callback (scrolled_window->vscrollbar, callback_data);
      if (scrolled_window->hscrollbar)
        callback (scrolled_window->hscrollbar, callback_data);
    }
}

static gboolean
gtk_tweaked_scrolled_window_scroll_child (GtkTweakedScrolledWindow *scrolled_window,
                                  GtkScrollType      scroll,
                                  gboolean           horizontal)
{
  GtkAdjustment *adjustment = NULL;
  
  switch (scroll)
    {
    case GTK_SCROLL_STEP_UP:
      scroll = GTK_SCROLL_STEP_BACKWARD;
      horizontal = FALSE;
      break;
    case GTK_SCROLL_STEP_DOWN:
      scroll = GTK_SCROLL_STEP_FORWARD;
      horizontal = FALSE;
      break;
    case GTK_SCROLL_STEP_LEFT:
      scroll = GTK_SCROLL_STEP_BACKWARD;
      horizontal = TRUE;
      break;
    case GTK_SCROLL_STEP_RIGHT:
      scroll = GTK_SCROLL_STEP_FORWARD;
      horizontal = TRUE;
      break;
    case GTK_SCROLL_PAGE_UP:
      scroll = GTK_SCROLL_PAGE_BACKWARD;
      horizontal = FALSE;
      break;
    case GTK_SCROLL_PAGE_DOWN:
      scroll = GTK_SCROLL_PAGE_FORWARD;
      horizontal = FALSE;
      break;
    case GTK_SCROLL_PAGE_LEFT:
      scroll = GTK_SCROLL_STEP_BACKWARD;
      horizontal = TRUE;
      break;
    case GTK_SCROLL_PAGE_RIGHT:
      scroll = GTK_SCROLL_STEP_FORWARD;
      horizontal = TRUE;
      break;
    case GTK_SCROLL_STEP_BACKWARD:
    case GTK_SCROLL_STEP_FORWARD:
    case GTK_SCROLL_PAGE_BACKWARD:
    case GTK_SCROLL_PAGE_FORWARD:
    case GTK_SCROLL_START:
    case GTK_SCROLL_END:
      break;
    default:
      g_warning ("Invalid scroll type %u for GtkTweakedScrolledWindow::scroll-child", scroll);
      return FALSE;
    }

  if ((horizontal && (!scrolled_window->hscrollbar || !scrolled_window->hscrollbar_visible)) ||
      (!horizontal && (!scrolled_window->vscrollbar || !scrolled_window->vscrollbar_visible)))
    return FALSE;

  if (horizontal)
    {
      if (scrolled_window->hscrollbar)
        adjustment = gtk_range_get_adjustment (GTK_RANGE (scrolled_window->hscrollbar));
    }
  else
    {
      if (scrolled_window->vscrollbar)
        adjustment = gtk_range_get_adjustment (GTK_RANGE (scrolled_window->vscrollbar));
    }

  if (adjustment)
    {
      gdouble value = adjustment->value;
      
      switch (scroll)
        {
        case GTK_SCROLL_STEP_FORWARD:
          value += adjustment->step_increment;
          break;
        case GTK_SCROLL_STEP_BACKWARD:
          value -= adjustment->step_increment;
          break;
        case GTK_SCROLL_PAGE_FORWARD:
          value += adjustment->page_increment;
          break;
        case GTK_SCROLL_PAGE_BACKWARD:
          value -= adjustment->page_increment;
          break;
        case GTK_SCROLL_START:
          value = adjustment->lower;
          break;
        case GTK_SCROLL_END:
          value = adjustment->upper;
          break;
        default:
          g_assert_not_reached ();
          break;
        }

      value = CLAMP (value, adjustment->lower, adjustment->upper - adjustment->page_size);
      
      gtk_adjustment_set_value (adjustment, value);

      return TRUE;
    }

  return FALSE;
}

static void
gtk_tweaked_scrolled_window_move_focus_out (GtkTweakedScrolledWindow *scrolled_window,
                                    GtkDirectionType   direction_type)
{
  GtkWidget *toplevel;
  
  /* Focus out of the scrolled window entirely. We do this by setting
   * a flag, then propagating the focus motion to the notebook.
   */
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (scrolled_window));
  if (!gtk_widget_is_toplevel (toplevel))
    return;

  g_object_ref (scrolled_window);
  
  scrolled_window->focus_out = TRUE;
  g_signal_emit_by_name (toplevel, "move-focus", direction_type);
  scrolled_window->focus_out = FALSE;
  
  g_object_unref (scrolled_window);
}

static void
gtk_tweaked_scrolled_window_size_request (GtkWidget      *widget,
                                  GtkRequisition *requisition)
{
  GtkTweakedScrolledWindow *scrolled_window;
  GtkBin *bin;
  gint extra_width;
  gint extra_height;
  gint scrollbar_spacing;
  GtkRequisition hscrollbar_requisition;
  GtkRequisition vscrollbar_requisition;
  GtkRequisition child_requisition;

  g_return_if_fail (GTK_IS_TWEAKED_SCROLLED_WINDOW (widget));
  g_return_if_fail (requisition != NULL);

  scrolled_window = GTK_TWEAKED_SCROLLED_WINDOW (widget);
  bin = GTK_BIN (scrolled_window);

  scrollbar_spacing = _gtk_tweaked_scrolled_window_get_scrollbar_spacing (scrolled_window);

  extra_width = 0;
  extra_height = 0;
  requisition->width = 0;
  requisition->height = 0;
  
  gtk_widget_size_request (scrolled_window->hscrollbar,
                           &hscrollbar_requisition);
  gtk_widget_size_request (scrolled_window->vscrollbar,
                           &vscrollbar_requisition);
  
  if (bin->child && gtk_widget_get_visible (bin->child))
    {
      gtk_widget_size_request (bin->child, &child_requisition);

      if (scrolled_window->hscrollbar_policy == GTK_POLICY_NEVER)
        requisition->width += child_requisition.width;
      else
        {
          GtkWidgetAuxInfo *aux_info = __gtk_widget_get_aux_info (bin->child, FALSE);

          if (aux_info && aux_info->width > 0)
            {
              requisition->width += aux_info->width;
              extra_width = -1;
            }
          else
            requisition->width += vscrollbar_requisition.width;
        }

      if (scrolled_window->vscrollbar_policy == GTK_POLICY_NEVER)
        requisition->height += child_requisition.height;
      else
        {
          GtkWidgetAuxInfo *aux_info = __gtk_widget_get_aux_info (bin->child, FALSE);

          if (aux_info && aux_info->height > 0)
            {
              requisition->height += aux_info->height;
              extra_height = -1;
            }
          else
            requisition->height += hscrollbar_requisition.height;
        }
    }

  if (scrolled_window->hscrollbar_policy == GTK_POLICY_AUTOMATIC ||
      scrolled_window->hscrollbar_policy == GTK_POLICY_ALWAYS)
    {
      requisition->width = MAX (requisition->width, hscrollbar_requisition.width);
      if (!extra_height || scrolled_window->hscrollbar_policy == GTK_POLICY_ALWAYS)
        extra_height = scrollbar_spacing + hscrollbar_requisition.height;
    }

  if (scrolled_window->vscrollbar_policy == GTK_POLICY_AUTOMATIC ||
      scrolled_window->vscrollbar_policy == GTK_POLICY_ALWAYS)
    {
      requisition->height = MAX (requisition->height, vscrollbar_requisition.height);
      if (!extra_height || scrolled_window->vscrollbar_policy == GTK_POLICY_ALWAYS)
        extra_width = scrollbar_spacing + vscrollbar_requisition.width;
    }

  requisition->width += GTK_CONTAINER (widget)->border_width * 2 + MAX (0, extra_width);
  requisition->height += GTK_CONTAINER (widget)->border_width * 2 + MAX (0, extra_height);

  if (scrolled_window->shadow_type != GTK_SHADOW_NONE)
    {
      requisition->width += 2 * widget->style->xthickness;
      requisition->height += 2 * widget->style->ythickness;
    }
}

static void
gtk_tweaked_scrolled_window_relative_allocation (GtkWidget     *widget,
                                         GtkAllocation *allocation)
{
  GtkTweakedScrolledWindow *scrolled_window;
  GtkTweakedScrolledWindowPrivate *priv;
  gint scrollbar_spacing;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (allocation != NULL);

  scrolled_window = GTK_TWEAKED_SCROLLED_WINDOW (widget);
  scrollbar_spacing = _gtk_tweaked_scrolled_window_get_scrollbar_spacing (scrolled_window);

  priv = GTK_TWEAKED_SCROLLED_WINDOW_GET_PRIVATE (scrolled_window);

  allocation->x = GTK_CONTAINER (widget)->border_width;
  allocation->y = GTK_CONTAINER (widget)->border_width;

  if (scrolled_window->shadow_type != GTK_SHADOW_NONE)
    {
      allocation->x += widget->style->xthickness;
      allocation->y += widget->style->ythickness;
    }
  
  allocation->width = MAX (1, (gint)widget->allocation.width - allocation->x * 2);
  allocation->height = MAX (1, (gint)widget->allocation.height - allocation->y * 2);

  if (scrolled_window->vscrollbar_visible)
    {
      GtkRequisition vscrollbar_requisition;
      gboolean is_rtl;

      gtk_widget_get_child_requisition (scrolled_window->vscrollbar,
                                        &vscrollbar_requisition);
      is_rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;
  
      if ((!is_rtl && 
           (priv->real_window_placement == GTK_CORNER_TOP_RIGHT ||
            priv->real_window_placement == GTK_CORNER_BOTTOM_RIGHT)) ||
          (is_rtl && 
           (priv->real_window_placement == GTK_CORNER_TOP_LEFT ||
            priv->real_window_placement == GTK_CORNER_BOTTOM_LEFT)))
        allocation->x += (vscrollbar_requisition.width +  scrollbar_spacing);

      allocation->width = MAX (1, allocation->width - (vscrollbar_requisition.width + scrollbar_spacing));
    }
  if (scrolled_window->hscrollbar_visible)
    {
      GtkRequisition hscrollbar_requisition;
      gtk_widget_get_child_requisition (scrolled_window->hscrollbar,
                                        &hscrollbar_requisition);
  
      if (priv->real_window_placement == GTK_CORNER_BOTTOM_LEFT ||
          priv->real_window_placement == GTK_CORNER_BOTTOM_RIGHT)
        allocation->y += (hscrollbar_requisition.height + scrollbar_spacing);

      allocation->height = MAX (1, allocation->height - (hscrollbar_requisition.height + scrollbar_spacing));
    }
}

static void
gtk_tweaked_scrolled_window_size_allocate (GtkWidget     *widget,
                                   GtkAllocation *allocation)
{
  GtkTweakedScrolledWindow *scrolled_window;
  GtkTweakedScrolledWindowPrivate *priv;
  GtkBin *bin;
  GtkAllocation relative_allocation;
  GtkAllocation child_allocation;
  gboolean scrollbars_within_bevel;
  gint scrollbar_spacing;
  
  g_return_if_fail (GTK_IS_TWEAKED_SCROLLED_WINDOW (widget));
  g_return_if_fail (allocation != NULL);

  scrolled_window = GTK_TWEAKED_SCROLLED_WINDOW (widget);
  bin = GTK_BIN (scrolled_window);

  scrollbar_spacing = _gtk_tweaked_scrolled_window_get_scrollbar_spacing (scrolled_window);
  gtk_widget_style_get (widget, "scrollbars-within-bevel", &scrollbars_within_bevel, NULL);

  priv = GTK_TWEAKED_SCROLLED_WINDOW_GET_PRIVATE (scrolled_window);

  widget->allocation = *allocation;

  if (scrolled_window->hscrollbar_policy == GTK_POLICY_ALWAYS)
    scrolled_window->hscrollbar_visible = TRUE;
  else if (scrolled_window->hscrollbar_policy == GTK_POLICY_NEVER)
    scrolled_window->hscrollbar_visible = FALSE;
  if (scrolled_window->vscrollbar_policy == GTK_POLICY_ALWAYS)
    scrolled_window->vscrollbar_visible = TRUE;
  else if (scrolled_window->vscrollbar_policy == GTK_POLICY_NEVER)
    scrolled_window->vscrollbar_visible = FALSE;

  if (bin->child && gtk_widget_get_visible (bin->child))
    {
      gboolean previous_hvis;
      gboolean previous_vvis;
      guint count = 0;
      
      do
        {
          gtk_tweaked_scrolled_window_relative_allocation (widget, &relative_allocation);
          
          child_allocation.x = relative_allocation.x + allocation->x;
          child_allocation.y = relative_allocation.y + allocation->y;
          child_allocation.width = relative_allocation.width;
          child_allocation.height = relative_allocation.height;
          
          previous_hvis = scrolled_window->hscrollbar_visible;
          previous_vvis = scrolled_window->vscrollbar_visible;
          
          gtk_widget_size_allocate (bin->child, &child_allocation);

          /* If, after the first iteration, the hscrollbar and the
           * vscrollbar flip visiblity, then we need both.
           */
          if (count &&
              previous_hvis != scrolled_window->hscrollbar_visible &&
              previous_vvis != scrolled_window->vscrollbar_visible)
            {
              scrolled_window->hscrollbar_visible = TRUE;
              scrolled_window->vscrollbar_visible = TRUE;

              /* a new resize is already queued at this point,
               * so we will immediatedly get reinvoked
               */
              return;
            }
          
          count++;
        }
      while (previous_hvis != scrolled_window->hscrollbar_visible ||
             previous_vvis != scrolled_window->vscrollbar_visible);
    }
  else
    {
      scrolled_window->hscrollbar_visible = scrolled_window->hscrollbar_policy == GTK_POLICY_ALWAYS;
      scrolled_window->vscrollbar_visible = scrolled_window->vscrollbar_policy == GTK_POLICY_ALWAYS;
      gtk_tweaked_scrolled_window_relative_allocation (widget, &relative_allocation);
    }
  
  if (scrolled_window->hscrollbar_visible)
    {
      GtkRequisition hscrollbar_requisition;
      gtk_widget_get_child_requisition (scrolled_window->hscrollbar,
                                        &hscrollbar_requisition);
  
      if (!gtk_widget_get_visible (scrolled_window->hscrollbar))
        gtk_widget_show (scrolled_window->hscrollbar);

      child_allocation.x = relative_allocation.x;
      if (priv->real_window_placement == GTK_CORNER_TOP_LEFT ||
          priv->real_window_placement == GTK_CORNER_TOP_RIGHT)
        child_allocation.y = (relative_allocation.y +
                              relative_allocation.height +
                              scrollbar_spacing +
                              (scrolled_window->shadow_type == GTK_SHADOW_NONE ?
                               0 : widget->style->ythickness));
      else
        child_allocation.y = GTK_CONTAINER (scrolled_window)->border_width;

      child_allocation.width = relative_allocation.width;
      child_allocation.height = hscrollbar_requisition.height;
      child_allocation.x += allocation->x;
      child_allocation.y += allocation->y;

      if (scrolled_window->shadow_type != GTK_SHADOW_NONE)
        {
          if (!scrollbars_within_bevel)
            {
              child_allocation.x -= widget->style->xthickness;
              child_allocation.width += 2 * widget->style->xthickness;
            }
          else if (GTK_CORNER_TOP_RIGHT == priv->real_window_placement ||
                   GTK_CORNER_TOP_LEFT == priv->real_window_placement)
            {
              child_allocation.y -= widget->style->ythickness;
            }
          else
            {
              child_allocation.y += widget->style->ythickness;
            }
        }

      gtk_widget_size_allocate (scrolled_window->hscrollbar, &child_allocation);
    }
  else if (gtk_widget_get_visible (scrolled_window->hscrollbar))
    gtk_widget_hide (scrolled_window->hscrollbar);

  if (scrolled_window->vscrollbar_visible)
    {
      GtkRequisition vscrollbar_requisition;
      if (!gtk_widget_get_visible (scrolled_window->vscrollbar))
        gtk_widget_show (scrolled_window->vscrollbar);

      gtk_widget_get_child_requisition (scrolled_window->vscrollbar,
                                        &vscrollbar_requisition);

      if ((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL && 
           (priv->real_window_placement == GTK_CORNER_TOP_RIGHT ||
            priv->real_window_placement == GTK_CORNER_BOTTOM_RIGHT)) ||
          (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR && 
           (priv->real_window_placement == GTK_CORNER_TOP_LEFT ||
            priv->real_window_placement == GTK_CORNER_BOTTOM_LEFT)))
        child_allocation.x = (relative_allocation.x +
                              relative_allocation.width +
                              scrollbar_spacing +
                              (scrolled_window->shadow_type == GTK_SHADOW_NONE ?
                               0 : widget->style->xthickness));
      else
        child_allocation.x = GTK_CONTAINER (scrolled_window)->border_width;

      child_allocation.y = relative_allocation.y;
      child_allocation.width = vscrollbar_requisition.width;
      child_allocation.height = relative_allocation.height;
      child_allocation.x += allocation->x;
      child_allocation.y += allocation->y;

      if (scrolled_window->shadow_type != GTK_SHADOW_NONE)
        {
          if (!scrollbars_within_bevel)
            {
              child_allocation.y -= widget->style->ythickness;
              child_allocation.height += 2 * widget->style->ythickness;
            }
          else if (GTK_CORNER_BOTTOM_LEFT == priv->real_window_placement ||
                   GTK_CORNER_TOP_LEFT == priv->real_window_placement)
            {
              child_allocation.x -= widget->style->xthickness;
            }
          else
            {
              child_allocation.x += widget->style->xthickness;
            }
        }

      gtk_widget_size_allocate (scrolled_window->vscrollbar, &child_allocation);
    }
  else if (gtk_widget_get_visible (scrolled_window->vscrollbar))
    gtk_widget_hide (scrolled_window->vscrollbar);
}

static gboolean
gtk_tweaked_scrolled_window_scroll_event (GtkWidget      *widget,
                                  GdkEventScroll *event)
{
  GtkWidget *range;

  g_return_val_if_fail (GTK_IS_TWEAKED_SCROLLED_WINDOW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);  

  if (event->direction == GDK_SCROLL_UP || event->direction == GDK_SCROLL_DOWN)
    range = GTK_TWEAKED_SCROLLED_WINDOW (widget)->vscrollbar;
  else
    range = GTK_TWEAKED_SCROLLED_WINDOW (widget)->hscrollbar;

  if (range && gtk_widget_get_visible (range))
    {
      GtkAdjustment *adj = GTK_RANGE (range)->adjustment;
      gdouble delta, new_value;

      delta = __gtk_range_get_wheel_delta (GTK_RANGE (range), event->direction);

      new_value = CLAMP (adj->value + delta, adj->lower, adj->upper - adj->page_size);
      
      gtk_adjustment_set_value (adj, new_value);

      return TRUE;
    }

  return FALSE;
}

static gboolean
gtk_tweaked_scrolled_window_focus (GtkWidget        *widget,
                           GtkDirectionType  direction)
{
  GtkTweakedScrolledWindow *scrolled_window = GTK_TWEAKED_SCROLLED_WINDOW (widget);
  gboolean had_focus_child = GTK_CONTAINER (widget)->focus_child != NULL;
  
  if (scrolled_window->focus_out)
    {
      scrolled_window->focus_out = FALSE; /* Clear this to catch the wrap-around case */
      return FALSE;
    }
  
  if (gtk_widget_is_focus (widget))
    return FALSE;

  /* We only put the scrolled window itself in the focus chain if it
   * isn't possible to focus any children.
   */
  if (GTK_BIN (widget)->child)
    {
      if (gtk_widget_child_focus (GTK_BIN (widget)->child, direction))
        return TRUE;
    }

  if (!had_focus_child && gtk_widget_get_can_focus (widget))
    {
      gtk_widget_grab_focus (widget);
      return TRUE;
    }
  else
    return FALSE;
}

static void
gtk_tweaked_scrolled_window_adjustment_changed (GtkAdjustment *adjustment,
                                        gpointer       data)
{
  GtkTweakedScrolledWindow *scrolled_win;

  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (data != NULL);

  scrolled_win = GTK_TWEAKED_SCROLLED_WINDOW (data);

  if (scrolled_win->hscrollbar &&
      adjustment == gtk_range_get_adjustment (GTK_RANGE (scrolled_win->hscrollbar)))
    {
      if (scrolled_win->hscrollbar_policy == GTK_POLICY_AUTOMATIC)
        {
          gboolean visible;
          
          visible = scrolled_win->hscrollbar_visible;
          scrolled_win->hscrollbar_visible = (adjustment->upper - adjustment->lower >
                                              adjustment->page_size);
          if (scrolled_win->hscrollbar_visible != visible)
            gtk_widget_queue_resize (GTK_WIDGET (scrolled_win));
        }
    }
  else if (scrolled_win->vscrollbar &&
           adjustment == gtk_range_get_adjustment (GTK_RANGE (scrolled_win->vscrollbar)))
    {
      if (scrolled_win->vscrollbar_policy == GTK_POLICY_AUTOMATIC)
        {
          gboolean visible;

          visible = scrolled_win->vscrollbar_visible;
          scrolled_win->vscrollbar_visible = (adjustment->upper - adjustment->lower >
                                              adjustment->page_size);
          if (scrolled_win->vscrollbar_visible != visible)
            gtk_widget_queue_resize (GTK_WIDGET (scrolled_win));
        }
    }
}

static void
gtk_tweaked_scrolled_window_add (GtkContainer *container,
                         GtkWidget    *child)
{
  GtkTweakedScrolledWindow *scrolled_window;
  GtkBin *bin;

  bin = GTK_BIN (container);
  g_return_if_fail (bin->child == NULL);

  scrolled_window = GTK_TWEAKED_SCROLLED_WINDOW (container);

  bin->child = child;
  gtk_widget_set_parent (child, GTK_WIDGET (bin));

  /* this is a temporary message */
  if (!gtk_widget_set_scroll_adjustments (child,
                                          gtk_range_get_adjustment (GTK_RANGE (scrolled_window->hscrollbar)),
                                          gtk_range_get_adjustment (GTK_RANGE (scrolled_window->vscrollbar))))
    g_warning ("gtk_tweaked_scrolled_window_add(): cannot add non scrollable widget "
               "use gtk_tweaked_scrolled_window_add_with_viewport() instead");
}

static void
gtk_tweaked_scrolled_window_remove (GtkContainer *container,
                            GtkWidget    *child)
{
  g_return_if_fail (GTK_IS_TWEAKED_SCROLLED_WINDOW (container));
  g_return_if_fail (child != NULL);
  g_return_if_fail (GTK_BIN (container)->child == child);
  
  gtk_widget_set_scroll_adjustments (child, NULL, NULL);

  /* chain parent class handler to remove child */
  GTK_CONTAINER_CLASS (gtk_tweaked_scrolled_window_parent_class)->remove (container, child);
}

/**
 * gtk_tweaked_scrolled_window_add_with_viewport:
 * @scrolled_window: a #GtkTweakedScrolledWindow
 * @child: the widget you want to scroll
 *
 * Used to add children without native scrolling capabilities. This
 * is simply a convenience function; it is equivalent to adding the
 * unscrollable child to a viewport, then adding the viewport to the
 * scrolled window. If a child has native scrolling, use
 * gtk_container_add() instead of this function.
 *
 * The viewport scrolls the child by moving its #GdkWindow, and takes
 * the size of the child to be the size of its toplevel #GdkWindow. 
 * This will be very wrong for most widgets that support native scrolling;
 * for example, if you add a widget such as #GtkTreeView with a viewport,
 * the whole widget will scroll, including the column headings. Thus, 
 * widgets with native scrolling support should not be used with the 
 * #GtkViewport proxy.
 *
 * A widget supports scrolling natively if the 
 * set_scroll_adjustments_signal field in #GtkWidgetClass is non-zero,
 * i.e. has been filled in with a valid signal identifier.
 */
static void
gtk_tweaked_scrolled_window_add_with_viewport (GtkTweakedScrolledWindow *scrolled_window,
                                       GtkWidget         *child)
{
  GtkBin *bin;
  GtkWidget *viewport;

  g_return_if_fail (GTK_IS_TWEAKED_SCROLLED_WINDOW (scrolled_window));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (child->parent == NULL);

  bin = GTK_BIN (scrolled_window);

  if (bin->child != NULL)
    {
      g_return_if_fail (GTK_IS_VIEWPORT (bin->child));
      g_return_if_fail (GTK_BIN (bin->child)->child == NULL);

      viewport = bin->child;
    }
  else
    {
      viewport =
        gtk_viewport_new (gtk_tweaked_scrolled_window_get_hadjustment (scrolled_window),
                          gtk_tweaked_scrolled_window_get_vadjustment (scrolled_window));
      gtk_container_add (GTK_CONTAINER (scrolled_window), viewport);
    }

  gtk_widget_show (viewport);
  gtk_container_add (GTK_CONTAINER (viewport), child);
}

/*
 * _gtk_tweaked_scrolled_window_get_spacing:
 * @scrolled_window: a scrolled window
 * 
 * Gets the spacing between the scrolled window's scrollbars and
 * the scrolled widget. Used by GtkCombo
 * 
 * Return value: the spacing, in pixels.
 */
static gint
_gtk_tweaked_scrolled_window_get_scrollbar_spacing (GtkTweakedScrolledWindow *scrolled_window)
{
  GtkTweakedScrolledWindowClass *class;
    
  g_return_val_if_fail (GTK_IS_TWEAKED_SCROLLED_WINDOW (scrolled_window), 0);

  class = GTK_TWEAKED_SCROLLED_WINDOW_GET_CLASS (scrolled_window);

  if (class->scrollbar_spacing >= 0)
    return class->scrollbar_spacing;
  else
    {
      gint scrollbar_spacing;
      
      gtk_widget_style_get (GTK_WIDGET (scrolled_window),
                            "scrollbar-spacing", &scrollbar_spacing,
                            NULL);

      return scrollbar_spacing;
    }
}

/* Export an array storing the function pointers of our scrolled window
 * implementation. */

gpointer tweaked_scrolled_window[17] = {
  (gpointer) gtk_tweaked_scrolled_window_get_type,
  (gpointer) gtk_tweaked_scrolled_window_new,
  (gpointer) gtk_tweaked_scrolled_window_set_hadjustment,
  (gpointer) gtk_tweaked_scrolled_window_set_vadjustment,
  (gpointer) gtk_tweaked_scrolled_window_get_hadjustment,
  (gpointer) gtk_tweaked_scrolled_window_get_vadjustment,
  (gpointer) gtk_tweaked_scrolled_window_get_hscrollbar,
  (gpointer) gtk_tweaked_scrolled_window_get_vscrollbar,
  (gpointer) gtk_tweaked_scrolled_window_set_policy,
  (gpointer) gtk_tweaked_scrolled_window_get_policy,
  (gpointer) gtk_tweaked_scrolled_window_set_placement,
  (gpointer) gtk_tweaked_scrolled_window_unset_placement,
  (gpointer) gtk_tweaked_scrolled_window_get_placement,
  (gpointer) gtk_tweaked_scrolled_window_set_shadow_type,
  (gpointer) gtk_tweaked_scrolled_window_get_shadow_type,
  (gpointer) gtk_tweaked_scrolled_window_add_with_viewport,
  (gpointer) _gtk_tweaked_scrolled_window_get_scrollbar_spacing
};
