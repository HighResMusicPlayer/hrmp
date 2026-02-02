/*
 * Copyright (C) 2026 The HighResMusicPlayer community
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/* hrmp */
#include <hrmp.h>
#include <alsa.h>
#include <files.h>
#include <logging.h>
#include <utils.h>

#include <stdbool.h>
#include <stdio.h>
#include <alsa/pcm.h>

static int
find_best_format(struct file_metadata* fm, snd_pcm_format_t* format);


#define MAX_BUFFER_SIZE 131072


int
hrmp_alsa_init_handle(struct file_metadata* fm, snd_pcm_t** handle)
{
   int err;
   snd_pcm_t* h = NULL;
   snd_pcm_hw_params_t* hw_params = NULL;
   snd_pcm_uframes_t buffer_size = 32768;
   snd_pcm_uframes_t period_size = 4096;
   unsigned int r = (unsigned int)fm->pcm_rate;
   snd_pcm_format_t fmt;
   struct configuration* config = NULL;

   config = (struct configuration*)shmem;

   if (strlen(config->active_device.device) == 0)
   {
      hrmp_log_error("Active device is not set");
      goto error;
   }

   *handle = NULL;

   if (find_best_format(fm, &fmt))
   {
      goto error;
   }

   if ((err = snd_pcm_open(&h, &config->active_device.device[0], SND_PCM_STREAM_PLAYBACK, 0)) < 0)
   {
      hrmp_log_error("snd_pcm_open %s/%s", &config->active_device.name[0], snd_strerror(err));
      goto error;
   }

   if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0)
   {
      hrmp_log_error("snd_pcm_hw_params_malloc %s/%s", &config->active_device.name[0], snd_strerror(err));
      goto error;
   }

   if ((err = snd_pcm_hw_params_any(h, hw_params)) < 0)
   {
      hrmp_log_error("snd_pcm_hw_params_any %s/%s", &config->active_device.name[0], snd_strerror(err));
      goto error;
   }

   if ((err = snd_pcm_hw_params_set_rate_resample(h, hw_params, 0)) < 0)
   {
      hrmp_log_error("snd_pcm_hw_params_set_rate_resample %s/%s",
                     &config->active_device.name[0], snd_strerror(err));
      goto error;
   }

   if ((err = snd_pcm_hw_params_set_access(h, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
   {
      hrmp_log_error("snd_pcm_hw_params_set_access %s/%s",
                     &config->active_device.name[0], snd_strerror(err));
      goto error;
   }

   if ((err = snd_pcm_hw_params_set_rate_near(h, hw_params, &r, 0)) < 0)
   {
      hrmp_log_error("snd_pcm_hw_params_set_rate_near %s/%s",
                     &config->active_device.name[0], snd_strerror(err));
      goto error;
   }

   if ((err = snd_pcm_hw_params_set_channels(h, hw_params, 2)) < 0)
   {
      hrmp_log_error("snd_pcm_hw_params_set_channels %s/%s",
                     &config->active_device.name[0], snd_strerror(err));
      goto error;
   }

   unsigned int rate = fm->pcm_rate;
   int direction = 0;
   if ((err = snd_pcm_hw_params_set_rate_near(h, hw_params, &rate, &direction)) < 0)
   {
      hrmp_log_error("snd_pcm_hw_params_set_rate_near %s/%s",
                     &config->active_device.name[0], snd_strerror(err));
      goto error;
   }

   if ((err = snd_pcm_hw_params_set_period_size_near(h, hw_params, &period_size, &direction)) < 0)
   {
      snd_pcm_hw_params_get_buffer_size_max(hw_params, &buffer_size);
      buffer_size = MIN(buffer_size, (snd_pcm_uframes_t)MAX_BUFFER_SIZE);

      snd_pcm_hw_params_get_period_size_min(hw_params, &period_size, NULL);
      if (!period_size)
      {
         period_size = buffer_size / 4;
      }

      if ((err = snd_pcm_hw_params_set_period_size_near(h, hw_params, &period_size, NULL)) < 0)
      {
         hrmp_log_error("snd_pcm_hw_params_set_period_size_near %s/%s",
                        &config->active_device.name[0], snd_strerror(err));
         goto error;
      }
   }

   if ((err = snd_pcm_hw_params_set_buffer_size_near(h, hw_params, &buffer_size)) < 0)
   {
      hrmp_log_error("snd_pcm_hw_params_set_buffer_size_near %s/%s",
                     &config->active_device.name[0], snd_strerror(err));
      goto error;
   }

   if ((err = snd_pcm_hw_params_set_rate_resample(h, hw_params, 0)) < 0)
   {
      hrmp_log_error("snd_pcm_hw_params_set_rate_resample %s/%s",
                     &config->active_device.name[0], snd_strerror(err));
      goto error;
   }

   if ((err = snd_pcm_hw_params_set_format(h, hw_params, fmt)) < 0)
   {
      hrmp_log_error("snd_pcm_hw_params_set_format %s/%d/%s",
                     &config->active_device.name[0], fmt, snd_strerror(err));
      goto error;
   }

   fm->alsa_snd = fmt;

   if ((err = snd_pcm_hw_params(h, hw_params)) < 0)
   {
      hrmp_log_error("snd_pcm_hw_params %s/%s", &config->active_device.name[0],
                     snd_strerror(err));
      goto error;
   }

   if (hrmp_alsa_reset_handle(h))
   {
      goto error;
   }

   snd_pcm_hw_params_free(hw_params);

   *handle = h;

   return 0;

error:

   if (hw_params != NULL)
   {
      snd_pcm_hw_params_free(hw_params);
   }

   if (h != NULL)
   {
      snd_pcm_drain(h);
      snd_pcm_close(h);
   }

   *handle = NULL;

   return 1;
}

int
hrmp_alsa_reset_handle(snd_pcm_t* handle)
{
   int err;

   if (handle != NULL)
   {
      if ((err = snd_pcm_drop(handle)) < 0)
      {
         hrmp_log_error("snd_pcm_drop %s", snd_strerror(err));
         goto error;
      }

      if ((err = snd_pcm_prepare(handle)) < 0)
      {
         hrmp_log_error("snd_pcm_prepare %s", snd_strerror(err));
         goto error;
      }
   }

   return 0;

error:

   return 1;
}

int
hrmp_alsa_close_handle(snd_pcm_t* handle)
{
   if (handle != NULL)
   {
      snd_pcm_hw_params_t* hw = NULL;
      snd_pcm_format_t fmt = SND_PCM_FORMAT_UNKNOWN;
      bool use_drop = false;
      struct configuration* config = (struct configuration*)shmem;

      if (snd_pcm_hw_params_malloc(&hw) == 0)
      {
         if (snd_pcm_hw_params_current(handle, hw) == 0)
         {
            snd_pcm_hw_params_get_format(hw, &fmt);
         }
         snd_pcm_hw_params_free(hw);
      }

      if (fmt == SND_PCM_FORMAT_DSD_U32_BE || fmt == SND_PCM_FORMAT_DSD_U32_LE || config->dop)
      {
         use_drop = true;
      }

      if (use_drop)
      {
         snd_pcm_drop(handle);
      }
      else
      {
         snd_pcm_drain(handle);
      }

      snd_pcm_close(handle);
   }

   return 0;
}

int
hrmp_alsa_init_volume(void)
{
   int current_volume = -1;
   int volume = -1;
   struct configuration* config = NULL;

   config = (struct configuration*)shmem;

   if (hrmp_alsa_get_volume(&current_volume))
   {
      current_volume = 100;
   }

   if (strlen(&config->active_device.device[0]) >= 0)
   {
      volume = config->active_device.volume;
   }
   else
   {
      volume = 70;
   }

   if (volume < 0)
   {
      volume = config->volume;
   }

   if (volume >= 0)
   {
      if (volume > 100)
      {
         volume = 100;
      }

      hrmp_alsa_set_volume(volume);
   }
   else
   {
      volume = current_volume;
   }

   config->volume = volume;
   config->prev_volume = volume;

   return 0;
}

int
hrmp_alsa_get_volume(int* volume)
{
   int err = 0;
   snd_mixer_t* handle = NULL;
   snd_mixer_selem_id_t* sid = NULL;
   snd_mixer_elem_t* elem = NULL;
   snd_mixer_selem_channel_id_t chn = SND_MIXER_SCHN_FRONT_LEFT;
   long vol = 0;
   char address[MISC_LENGTH];
   struct configuration* config = NULL;

   config = (struct configuration*)shmem;

   if ((err = snd_mixer_open(&handle, 0)) < 0)
   {
      hrmp_log_error("Error: snd_mixer_open: %s", snd_strerror(err));
      goto error;
   }

   memset(&address[0], 0, sizeof(address));
   hrmp_snprintf(&address[0], sizeof(address), "hw:%d", config->active_device.hardware);

   if ((err = snd_mixer_attach(handle, &address[0])) < 0)
   {
      hrmp_log_error("Error: snd_mixer_attach(%s): %s", &config->active_device.name[0], snd_strerror(err));
      goto error;
   }

   if ((err = snd_mixer_selem_register(handle, NULL, NULL)) < 0)
   {
      hrmp_log_error("Error: snd_mixer_selem_register: %s", snd_strerror(err));
      goto error;
   }

   if ((err = snd_mixer_load(handle)) < 0)
   {
      hrmp_log_error("Error: snd_mixer_load: %s", snd_strerror(err));
      goto error;
   }

   if (snd_mixer_selem_id_malloc(&sid) < 0 || sid == NULL)
   {
      hrmp_log_error("Error: failed to allocate selem id");
      goto error;
   }

   snd_mixer_selem_id_set_index(sid, 0);
   snd_mixer_selem_id_set_name(sid, config->active_device.selem);

   elem = snd_mixer_find_selem(handle, sid);
   snd_mixer_selem_id_free(sid);
   sid = NULL;

   if (elem != NULL)
   {
      if (snd_mixer_selem_has_playback_volume(elem) == 0)
      {
         goto error;
      }

      if ((err = snd_mixer_selem_get_playback_volume(elem, chn, &vol)) < 0)
      {
         goto error;
      }
   }
   else
   {
      goto error;
   }

   config->active_device.has_volume = true;
   *volume = (int)vol;

   snd_mixer_close(handle);

   return 0;

error:

   if (sid != NULL)
   {
      snd_mixer_selem_id_free(sid);
   }

   if (handle != NULL)
   {
      snd_mixer_close(handle);
   }

   config->active_device.has_volume = false;
   *volume = 70;

   return 1;
}

int
hrmp_alsa_set_volume(int volume)
{
   int err = 0;
   snd_mixer_t* handle = NULL;
   snd_mixer_selem_id_t* sid = NULL;
   snd_mixer_elem_t* elem = NULL;
   long minv = 0;
   long maxv = 0;
   long vol = 0;
   char address[MISC_LENGTH];
   struct configuration* config = NULL;

   config = (struct configuration*)shmem;

   config->prev_volume = config->volume;

   if (volume < 0)
   {
      volume = 0;
   }
   else if (volume > 100)
   {
      volume = 100;
   }

   if ((err = snd_mixer_open(&handle, 0)) < 0)
   {
      hrmp_log_error("Error: snd_mixer_open: %s", snd_strerror(err));
      goto error;
   }

   memset(&address[0], 0, sizeof(address));
   hrmp_snprintf(&address[0], sizeof(address), "hw:%d", config->active_device.hardware);

   if ((err = snd_mixer_attach(handle, &address[0])) < 0)
   {
      hrmp_log_error("Error: snd_mixer_attach(%s): %s", &config->active_device.name[0], snd_strerror(err));
      goto error;
   }

   if ((err = snd_mixer_selem_register(handle, NULL, NULL)) < 0)
   {
      hrmp_log_error("Error: snd_mixer_selem_register: %s", snd_strerror(err));
      goto error;
   }

   if ((err = snd_mixer_load(handle)) < 0)
   {
      hrmp_log_error("Error: snd_mixer_load: %s", snd_strerror(err));
      goto error;
   }

   if (snd_mixer_selem_id_malloc(&sid) < 0 || sid == NULL)
   {
      hrmp_log_error("Error: failed to allocate selem id");
      goto error;
   }

   snd_mixer_selem_id_set_index(sid, 0);
   snd_mixer_selem_id_set_name(sid, &config->active_device.selem[0]);

   elem = snd_mixer_find_selem(handle, sid);
   snd_mixer_selem_id_free(sid);
   sid = NULL;

   if (elem != NULL)
   {
      if (snd_mixer_selem_has_playback_volume(elem) == 0)
      {
         goto error;
      }

      snd_mixer_selem_get_playback_volume_range(elem, &minv, &maxv);
      vol = minv + (volume * (maxv - minv)) / 100;

      if ((err = snd_mixer_selem_set_playback_volume_all(elem, vol)) < 0)
      {
         goto error;
      }
   }
   else
   {
      goto error;
   }

   config->active_device.has_volume = true;
   config->volume = volume;

   snd_mixer_close(handle);

   return 0;

error:

   if (sid != NULL)
   {
      snd_mixer_selem_id_free(sid);
   }

   if (handle != NULL)
   {
      snd_mixer_close(handle);
   }

   config->active_device.has_volume = false;
   config->volume = 70;

   return 1;
}


static int
find_best_format(struct file_metadata* fm, snd_pcm_format_t* format)
{
   bool found = false;
   snd_pcm_format_t fmt;
   struct configuration* config = NULL;

   config = (struct configuration*)shmem;

   *format = SND_PCM_FORMAT_UNKNOWN;

   if (fm == NULL)
   {
      goto error;
   }

   if (fm->format == FORMAT_16)
   {
      if (config->active_device.capabilities.s16_le)
      {
         fm->container = 16;
         fmt = SND_PCM_FORMAT_S16_LE;
         found = true;
      }
   }
   else if (fm->format == FORMAT_24)
   {
      if (config->active_device.capabilities.s24_3le)
      {
         fm->container = 24;
         fmt = SND_PCM_FORMAT_S24_3LE;
         found = true;
      }
      else if (config->active_device.capabilities.s32_le)
      {
         fm->container = 32;
         fmt = SND_PCM_FORMAT_S32_LE;
         found = true;
      }
   }
   else if (fm->format == FORMAT_32)
   {
      if (config->active_device.capabilities.s32_le)
      {
         fm->container = 32;
         fmt = SND_PCM_FORMAT_S32_LE;
         found = true;
      }
   }
   else if (fm->format == FORMAT_1)
   {
      if (!config->dop)
      {
         if (config->active_device.capabilities.dsd_u32_be)
         {
            fm->container = 32;
            fmt = SND_PCM_FORMAT_DSD_U32_BE;
            found = true;
         }
      }

      if (!found)
      {
         if (config->active_device.capabilities.s32_le)
         {
            fm->container = 32;
            fmt = SND_PCM_FORMAT_S32_LE;
            found = true;
         }
      }
   }
   else
   {
      goto error;
   }

   if (!found)
   {
      goto error;
   }

   *format = fmt;

   return 0;

error:

   return 1;
}

