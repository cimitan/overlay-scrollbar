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
 *             Loïc Molinari <loic.molinari@canonical.com>
 */

#ifndef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include "os-private.h"

struct _OsAnimationPrivate {
  OsAnimationUpdateFunc update_func;
  OsAnimationEndFunc end_func;
  gpointer user_data;
  gint64 start_time;
  gint64 duration;
  gint32 rate;
  guint32 source_id;
};

static void os_animation_dispose (GObject *object);
static void os_animation_finalize (GObject *object);

G_DEFINE_TYPE (OsAnimation, os_animation, G_TYPE_OBJECT);

static void
os_animation_class_init (OsAnimationClass *class)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (class);

  gobject_class->dispose  = os_animation_dispose;
  gobject_class->finalize = os_animation_finalize;

  g_type_class_add_private (gobject_class, sizeof (OsAnimationPrivate));
}

static void
os_animation_init (OsAnimation *animation)
{
  OsAnimationPrivate *priv;

  animation->priv = G_TYPE_INSTANCE_GET_PRIVATE (animation,
                                                 OS_TYPE_ANIMATION,
                                                 OsAnimationPrivate);
  priv = animation->priv;

  priv->source_id = 0;
}

static void
os_animation_dispose (GObject *object)
{
  OsAnimation *animation;
  OsAnimationPrivate *priv;

  animation = OS_ANIMATION (object);
  priv = animation->priv;

  if (priv->source_id != 0)
    {
      g_source_remove (priv->source_id);
      priv->source_id = 0;
    }

  G_OBJECT_CLASS (os_animation_parent_class)->dispose (object);
}

static void
os_animation_finalize (GObject *object)
{
  G_OBJECT_CLASS (os_animation_parent_class)->finalize (object);
}

/* Public functions. */

/**
 * os_animation_new:
 * @rate: rate of the update
 * @duration: duration of the animation
 * @update_func: function to call on update
 * @end_func: function to call at the end
 * @user_data: pointer to the user data
 *
 * Creates a new OsAnimation
 *
 * Returns: the pointer to the #OsAnimation
 */
OsAnimation*
os_animation_new (gint32                rate,
                  gint32                duration,
                  OsAnimationUpdateFunc update_func,
                  OsAnimationEndFunc    end_func,
                  gpointer              user_data)
{
  OsAnimation *animation;
  OsAnimationPrivate *priv;

  g_return_val_if_fail (rate != 0, NULL);
  g_return_val_if_fail (duration != 0, NULL);
  g_return_val_if_fail (update_func != NULL, NULL);

  animation = g_object_new (OS_TYPE_ANIMATION, NULL);
  priv = animation->priv;

  priv->update_func = update_func;
  priv->end_func = end_func;
  priv->user_data = user_data;
  priv->duration = (gint64) duration * G_GINT64_CONSTANT (1000);
  priv->rate = 1000 / rate;

  return animation;
}

/**
 * os_animation_is_running:
 * @animation: a #OsAnimation
 *
 * Returns TRUE if the animation is running
 **/
gboolean
os_animation_is_running (OsAnimation *animation)
{
  OsAnimationPrivate *priv;

  g_return_if_fail (animation != NULL);

  priv = animation->priv;

  return priv->source_id != 0;
}

/**
 * os_animation_set_duration:
 * @animation: a #OsAnimation
 * @duration: the new duration
 *
 * Sets the new duration of the animation
 **/
void
os_animation_set_duration (OsAnimation *animation,
                           gint32       duration)
{
  OsAnimationPrivate *priv;

  g_return_if_fail (animation != NULL);
  g_return_if_fail (duration != 0);

  priv = animation->priv;

  priv->duration = (gint64) duration * G_GINT64_CONSTANT (1000);
}

/* callback called by the animation */
static gboolean
update_cb (gpointer user_data)
{
  OsAnimation *animation = OS_ANIMATION (user_data);
  OsAnimationPrivate *priv = animation->priv;
  const gint64 current_time = g_get_monotonic_time ();
  const gint64 end_time = priv->start_time + priv->duration;

  if (current_time < end_time)
    {
      /* On-going animation. */
      const gfloat diff_time = current_time - priv->start_time;
      const gfloat weight = diff_time / priv->duration;

      priv->update_func (weight, priv->user_data);

      return TRUE;
    }
  else
    {
      /* Animation ended. */
      priv->update_func (1.0f, priv->user_data);
      priv->source_id = 0;

      if (priv->end_func != NULL)
        priv->end_func (priv->user_data);

      return FALSE;
    }
}

/**
 * os_animation_start:
 * @animation: a #OsAnimation
 *
 * Starts the animation
 **/
void
os_animation_start (OsAnimation *animation)
{
  OsAnimationPrivate *priv;

  g_return_if_fail (animation != NULL);

  priv = animation->priv;

  if (priv->source_id == 0)
    {
      priv->start_time = g_get_monotonic_time ();
      priv->source_id = g_timeout_add (priv->rate, update_cb, animation);
    }
}

/**
 * os_animation_stop:
 * @animation: a #OsAnimation
 * @stop_func: function to call at the stop
 *
 * Stops the animation.
 * Before stopping, calls stop_func (if not NULL),
 * or end_func (if not NULL).
 **/
void
os_animation_stop (OsAnimation        *animation,
                   OsAnimationStopFunc stop_func)
{
  OsAnimationPrivate *priv;

  g_return_if_fail (animation != NULL);

  priv = animation->priv;

  if (priv->source_id != 0)
    {
      if (stop_func != NULL)
        stop_func (priv->user_data);
      else if (priv->end_func != NULL)
        priv->end_func (priv->user_data);

      g_source_remove (priv->source_id);
      priv->source_id = 0;
    }
}
