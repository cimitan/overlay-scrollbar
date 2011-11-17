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

#ifndef __OS_PRIVATE_H__
#define __OS_PRIVATE_H__

#include <gtk/gtk.h>

/* Tell GCC not to export internal functions. */
#ifdef __GNUC__
#pragma GCC visibility push(hidden)
#endif /* __GNUC__ */

/* Rate of the animations (frames per second). */
#define RATE_ANIMATION 30

/* Size of the thumb in pixels. */
#define MIN_THUMB_HEIGHT 35
#define THUMB_WIDTH  17
#define THUMB_HEIGHT 68

/* Number of tolerance pixels on pageup/down, while intercepting a motion-notify-event. */
#define TOLERANCE_MOTION 2

G_BEGIN_DECLS

typedef struct
{
  gint x;
  gint y;
} OsCoordinate;

typedef enum
{
  OS_EVENT_NONE = 0,
  OS_EVENT_BUTTON_PRESS = 1,
  OS_EVENT_ENTER_NOTIFY = 2,
  OS_EVENT_MOTION_NOTIFY = 4,
  OS_EVENT_SCROLL_PRESS = 8
} OsEventFlags;

/* os-log.c */

/* Severity levels. */
typedef enum {
  OS_INFO,  /* Informative message (white colored). */
  OS_WARN,  /* Warning message (yellow colored). */
  OS_ERROR  /* Error message (red colored). */
} OsLogLevel;

/* Logging level threshold, message logged with a lower level are discarded.
 * Initialized to OS_INFO for debug builds and OS_WARN for release builds. */
extern OsLogLevel threshold;

/* Log a message to stderr at the given level following the printf() syntax. */
void
G_GNUC_NO_INSTRUMENT
G_GNUC_PRINTF (5, 6)
os_log_message (OsLogLevel level, const gchar *function, const gchar *file,
                gint32 line, const gchar *format, ...);

/* Macro logging a message to stderr using the given level. */
#define OS_LOG(level,...)                                                  \
  G_STMT_START {                                                           \
    if (level >= threshold)                                                \
      os_log_message ((level), __func__, __FILE__, __LINE__, __VA_ARGS__); \
  } G_STMT_END

/* Macro conditionally logging a message to stderr using the given level. */
#define OS_LOG_IF(level,cond,...)                                          \
  G_STMT_START {                                                           \
    if (level >= threshold) && (cond == TRUE)) {                           \
      os_log_message ((level), __func__, __FILE__, __LINE__, __VA_ARGS__); \
    }                                                                      \
  } G_STMT_END

/* Macro logging an error message to stderr and breaking the program execution
 * if the assertion fails. */
#define OS_CHECK(cond)                                        \
  G_STMT_START {                                              \
    if (G_UNLIKELY((cond) == FALSE)) {                        \
      os_log_message (OS_ERROR, __func__, __FILE__, __LINE__, \
                      "assertion `"#cond"' failed");          \
      G_BREAKPOINT ();                                        \
    }                                                         \
  } G_STMT_END

/* Debug mode logging macros, compiled away to nothing for release builds. */
#if !defined(NDEBUG)
#define OS_DLOG(level,...) OS_LOG((level), __VA_ARGS__)
#define OS_DLOG_IF(level,cond,...) OS_LOG_IF((level), (cond), __VA_ARGS__)
#define OS_DCHECK(cond) OS_CHECK((cond))
#else
#define OS_DLOG(level,...)
#define OS_DLOG_IF(level,cond,...)
#define OS_DCHECK(cond)
#endif

/* os-animation.c */

#define OS_TYPE_ANIMATION            (os_animation_get_type ())
#define OS_ANIMATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), OS_TYPE_ANIMATION, OsAnimation))
#define OS_ANIMATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), OS_TYPE_ANIMATION, OsAnimationClass))
#define OS_IS_ANIMATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OS_TYPE_ANIMATION))
#define OS_IS_ANIMATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), OS_TYPE_ANIMATION))
#define OS_ANIMATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), OS_TYPE_ANIMATION, OsAnimationClass))

typedef void (*OsAnimationUpdateFunc) (gfloat weight, gpointer user_data);
typedef void (*OsAnimationEndFunc)    (gpointer user_data);
typedef void (*OsAnimationStopFunc)   (gpointer user_data);

typedef struct _OsAnimation OsAnimation;
typedef struct _OsAnimationClass OsAnimationClass;
typedef struct _OsAnimationPrivate OsAnimationPrivate;

struct _OsAnimation {
  GObject parent_instance;

  OsAnimationPrivate *priv;
};

struct _OsAnimationClass {
  GObjectClass parent_class;
};

GType        os_animation_get_type     (void);

OsAnimation* os_animation_new          (gint32                rate,
                                        gint32                duration,
                                        OsAnimationUpdateFunc update_func,
                                        OsAnimationEndFunc    end_func,
                                        gpointer              user_data);

gboolean     os_animation_is_running   (OsAnimation *animation);

void         os_animation_set_duration (OsAnimation *animation,
                                        gint32       duration);

void         os_animation_start        (OsAnimation *animation);

void         os_animation_stop         (OsAnimation        *animation,
                                        OsAnimationStopFunc stop_func);

/* os-bar.c */

#define OS_TYPE_BAR            (os_bar_get_type ())
#define OS_BAR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), OS_TYPE_BAR, OsBar))
#define OS_BAR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), OS_TYPE_BAR, OsBarClass))
#define OS_IS_BAR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OS_TYPE_BAR))
#define OS_IS_BAR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), OS_TYPE_BAR))
#define OS_BAR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), OS_TYPE_BAR, OsBarClass))

typedef struct _OsBar OsBar;
typedef struct _OsBarClass OsBarClass;
typedef struct _OsBarPrivate OsBarPrivate;

struct _OsBar {
  GObject parent_instance;

  OsBarPrivate *priv;
};

struct _OsBarClass {
  GObjectClass parent_class;
};

GType  os_bar_get_type      (void) G_GNUC_CONST;

OsBar* os_bar_new           (void);

void   os_bar_hide          (OsBar *bar);

void   os_bar_connect       (OsBar       *bar,
                             GdkRectangle mask);

void   os_bar_move_resize   (OsBar       *bar,
                             GdkRectangle mask);

void   os_bar_set_active    (OsBar   *bar,
                             gboolean active,
                             gboolean animate);

void   os_bar_set_detached  (OsBar   *bar,
                             gboolean detached,
                             gboolean animate);

void   os_bar_set_parent    (OsBar     *bar,
                             GtkWidget *parent);

void   os_bar_show          (OsBar *bar);

void   os_bar_size_allocate (OsBar       *bar,
                             GdkRectangle rectangle);

/* os-thumb.c */

#define OS_TYPE_THUMB (os_thumb_get_type ())
#define OS_THUMB(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), OS_TYPE_THUMB, OsThumb))
#define OS_THUMB_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), OS_TYPE_THUMB, OsThumbClass))
#define OS_IS_THUMB(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OS_TYPE_THUMB))
#define OS_IS_THUMB_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), OS_TYPE_THUMB))
#define OS_THUMB_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), OS_TYPE_THUMB, OsThumbClass))

typedef struct _OsThumb OsThumb;
typedef struct _OsThumbClass OsThumbClass;
typedef struct _OsThumbPrivate OsThumbPrivate;

struct _OsThumb {
  GtkWindow parent_object;

  OsThumbPrivate *priv;
};

struct _OsThumbClass {
  GtkWindowClass parent_class;
};

GType      os_thumb_get_type     (void) G_GNUC_CONST;

GtkWidget* os_thumb_new          (GtkOrientation orientation);

void       os_thumb_resize       (OsThumb *thumb,
                                  gint     width,
                                  gint     height);

void       os_thumb_set_detached (OsThumb *thumb,
                                  gboolean detached);

G_END_DECLS

#ifdef __GNUC__
#pragma GCC visibility pop
#endif /* __GNUC__ */

#endif /* __OS_PRIVATE_H__ */
