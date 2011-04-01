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

static gboolean os_animation_cb (gpointer user_data);

static gboolean
os_animation_cb (gpointer user_data)
{
  OsAnimation* animation = (OsAnimation*) user_data;
  const gint64 current_time = g_get_monotonic_time ();
  const gint64 end_time = animation->start_time + animation->duration;
  gboolean stopped = animation->stopped;

  /* Update the animation if it's not been stopped. */
  if (stopped == FALSE)
    {
      gfloat weight;

      if (current_time < end_time)
        weight = (gfloat) (current_time - animation->start_time) / animation->duration;
      else
        {
          weight = 1.0f;
          stopped = TRUE;
        }

      animation->update_func (weight, animation->user_data);
    }

  /* Clean up the animation and remove the source from the mainloop if the
   * animation ended or has been stopped. */
  if (stopped == TRUE)
    {
      g_slice_free (OsAnimation, animation);
      return FALSE;
    }
  else
    return TRUE;
}

/* Public functions. */

/**
 * os_animation_spawn_animation:
 * @rate: rate of the update
 * @duration: duration of the animation
 * @update_func: function to call on update
 * @end_func: function to call at the end
 * @user_data: pointer to the user data
 *
 * Spawns a new animation
 *
 * Returns: the pointer to the #OsAnimation
 */
OsAnimation*
os_animation_spawn_animation (gint32 rate,
                              gint32 duration,
                              OsAnimationUpdateFunc update_func,
                              OsAnimationEndFunc end_func,
                              gpointer user_data)
{
  OsAnimation* animation;

  g_return_val_if_fail (rate != 0, NULL);
  g_return_val_if_fail (duration != 0, NULL);
  g_return_val_if_fail (update_func != NULL, NULL);

  animation = g_slice_new (OsAnimation);
  animation->update_func = update_func;
  animation->end_func = end_func;
  animation->user_data = user_data;
  animation->start_time = g_get_monotonic_time ();
  animation->duration = duration * 1000;
  animation->stopped = FALSE;
  g_timeout_add (rate, os_animation_cb, animation);

  animation->update_func (0.0f, animation->user_data);

  return animation;
}

/**
 * os_animation_stop:
 * @animation: a #OsAnimation
 *
 * Stops the animation
 **/
void
os_animation_stop (OsAnimation* animation)
{
  animation->stopped = TRUE;
}
