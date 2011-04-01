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

/* Default size of the thumb in pixels. */
#define DEFAULT_THUMB_WIDTH  15
#define DEFAULT_THUMB_HEIGHT 67

G_BEGIN_DECLS

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
os_log_message (OsLogLevel level, const gchar* function, const gchar* file,
                gint32 line, const gchar* format, ...);

/* Macro logging a message to stderr using the given level. */
#define OS_LOG(level,...)                                                  \
  G_STMT_START {                                                           \
    if (level >= threshold)                                                \
      os_log_message ((level), __func__, __FILE__, __LINE__, __VA_ARGS__); \
  } G_STMT_END

/* Macro conditionally logging a message to stderr using the given level. */
#define OS_LOG_IF(level,cond,...)                                          \
  G_STMT_START {                                                           \
    if (level >= threshold) && (cond == true)) {                           \
      os_log_message ((level), __func__, __FILE__, __LINE__, __VA_ARGS__); \
    }                                                                      \
  } G_STMT_END

/* Macro loggging an error message to stderr and breaking the program execution
 * if the assertion fails. */
#define OS_CHECK(cond)                                            \
  G_STMT_START {                                                  \
    if (G_UNLIKELY((cond) == false)) {                            \
      os_log_message (OS_LOG_ERROR, __func__, __FILE__, __LINE__, \
                      "assertion `"#cond"' failed");              \
      G_BREAKPOINT ();                                            \
    }                                                             \
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

typedef void (*OsAnimationUpdateFunc) (gfloat weight, gpointer user_data);
typedef void (*OsAnimationEndFunc) (gpointer user_data);

typedef struct _OsAnimation OsAnimation;

struct _OsAnimation {
  OsAnimationUpdateFunc update_func;
  OsAnimationEndFunc end_func;
  gpointer user_data;
  gint64 start_time;
  gint64 duration;
  gboolean stopped;
};

OsAnimation* os_animation_spawn_animation (gint32 rate,
                                           gint32 duration,
                                           OsAnimationUpdateFunc update_func,
                                           OsAnimationEndFunc end_func,
                                           gpointer user_data);

void         os_animation_stop            (OsAnimation* animation);

/* os-thumb.c */

#define OS_TYPE_THUMB (os_thumb_get_type ())
#define OS_THUMB(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), OS_TYPE_THUMB, OsThumb))
#define OS_THUMB_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass),  OS_TYPE_THUMB, OsThumbClass))
#define OS_IS_THUMB(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OS_TYPE_THUMB))
#define OS_IS_THUMB_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),  OS_TYPE_THUMB))
#define OS_THUMB_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),  OS_TYPE_THUMB, OsThumbClass))

typedef struct _OsThumb      OsThumb;
typedef struct _OsThumbPrivate OsThumbPrivate;
typedef struct _OsThumbClass OsThumbClass;

struct _OsThumb {
  GtkWindow parent_object;

  OsThumbPrivate *priv;
};

struct _OsThumbClass {
  GtkWindowClass parent_class;
};

GType      os_thumb_get_type (void) G_GNUC_CONST;

GtkWidget* os_thumb_new      (GtkOrientation orientation);

/* os-pager.c */

#define OS_TYPE_PAGER (os_pager_get_type ())
#define OS_PAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), OS_TYPE_PAGER, OsPager))
#define OS_PAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), OS_TYPE_PAGER, OsPagerClass))
#define OS_IS_PAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OS_TYPE_PAGER))
#define OS_IS_PAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), OS_TYPE_PAGER))
#define OS_PAGER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),  OS_TYPE_PAGER, OsPagerClass))

typedef struct _OsPager OsPager;
typedef struct _OsPagerPrivate OsPagerPrivate;
typedef struct _OsPagerClass OsPagerClass;

struct _OsPager {
  GObject parent_instance;

  OsPagerPrivate *priv;
};

struct _OsPagerClass {
  GObjectClass parent_class;
};

GType    os_pager_get_type      (void) G_GNUC_CONST;

GObject* os_pager_new           (void);

void     os_pager_hide          (OsPager *overlay);

void     os_pager_move_resize   (OsPager     *overlay,
                                 GdkRectangle mask);

void     os_pager_set_active    (OsPager *overlay,
                                 gboolean active);

void     os_pager_set_parent    (OsPager   *pager,
                                 GtkWidget *parent);

void     os_pager_show          (OsPager *overlay);

void     os_pager_size_allocate (OsPager     *overlay,
                                 GdkRectangle rectangle);


G_END_DECLS

#ifdef __GNUC__
#pragma GCC visibility pop
#endif /* __GNUC__ */

#endif /* __OS_PRIVATE_H__ */
