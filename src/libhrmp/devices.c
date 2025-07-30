/*
 * Copyright (C) 2025 HighResMusicPlayer
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list
 * of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this
 * list of conditions and the following disclaimer in the documentation and/or other
 * materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may
 * be used to endorse or promote products derived from this software without specific
 * prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* hrmp */
#include <hrmp.h>
#include <devices.h>
#include <logging.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <utils.h>

/* system */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <alsa/asoundlib.h>

static void check_capabilities(char* device, int index);
static bool support_mask(char* device, snd_pcm_format_mask_t* fm, int format);
static char* clean_description(char* s);
static bool is_device_active(char* device);

void
hrmp_check_devices(void)
{
   struct configuration* config = NULL;

   config = (struct configuration*)shmem;

   for (int i = 0; i < config->number_of_devices; i++)
   {
      config->devices[i].active = false;

      if (is_device_active(config->devices[i].device))
      {
         if (!config->quiet)
         {
            printf("Device: %s (Active)\n", config->devices[i].name);
         }
         check_capabilities(config->devices[i].device, i);
         config->devices[i].active = true;
      }
      else
      {
         if (!config->quiet)
         {
            printf("Device: %s (Inactive)\n", config->devices[i].name);
         }
      }
   }
}

bool
hrmp_is_device_known(char* name)
{
   struct configuration* config = NULL;

   config = (struct configuration*)shmem;

   for (int i = 0; i < config->number_of_devices; i++)
   {
      if (name != NULL && !strcmp(name, config->devices[i].name))
      {
         return true;
      }
   }

   return false;
}

int
hrmp_active_device(char* name)
{
   struct configuration* config = NULL;

   config = (struct configuration*)shmem;

   for (int i = 0; i < config->number_of_devices; i++)
   {
      if (name != NULL && !strcmp(name, config->devices[i].name) && config->devices[i].active)
      {
         return i;
      }
   }

   if (name == NULL || strlen(name) == 0)
   {
      for (int i = 0; i < config->number_of_devices; i++)
      {
         if (config->devices[i].active)
         {
            return i;
         }
      }
   }

   return -1;
}

void
hrmp_print_devices(void)
{
   struct configuration* config = NULL;

   config = (struct configuration*)shmem;

   if (config->quiet)
   {
      return;
   }

   for (int i = 0; i < config->number_of_devices; i++)
   {
      printf("%s\n", config->devices[i].name);
      printf("  Device: %s\n", config->devices[i].device);
      printf("  Description: %s\n", config->devices[i].description);
      printf("  Active: %s\n", config->devices[i].active ? "Yes" : "No");
      printf("  16bit: %s/%s/%s/%s/%s/%s\n",
             config->devices[i].capabilities.s16 ? "Yes" : "No",
             config->devices[i].capabilities.s16_le ? "Yes" : "No",
             config->devices[i].capabilities.s16_be ? "Yes" : "No",
             config->devices[i].capabilities.u16 ? "Yes" : "No",
             config->devices[i].capabilities.u16_le ? "Yes" : "No",
             config->devices[i].capabilities.u16_be ? "Yes" : "No");
      printf("  24bit: %s/%s/%s/%s/%s/%s\n",
             config->devices[i].capabilities.s24 ? "Yes" : "No",
             config->devices[i].capabilities.s24_le ? "Yes" : "No",
             config->devices[i].capabilities.s24_be ? "Yes" : "No",
             config->devices[i].capabilities.u24 ? "Yes" : "No",
             config->devices[i].capabilities.u24_le ? "Yes" : "No",
             config->devices[i].capabilities.u24_be ? "Yes" : "No");
      printf("  32bit: %s/%s/%s/%s/%s/%s\n",
             config->devices[i].capabilities.s32 ? "Yes" : "No",
             config->devices[i].capabilities.s32_le ? "Yes" : "No",
             config->devices[i].capabilities.s32_be ? "Yes" : "No",
             config->devices[i].capabilities.u32 ? "Yes" : "No",
             config->devices[i].capabilities.u32_le ? "Yes" : "No",
             config->devices[i].capabilities.u32_be ? "Yes" : "No");
      printf("  DSD: %s/%s/%s/%s\n",
             config->devices[i].capabilities.dsd_u16_le ? "Yes" : "No",
             config->devices[i].capabilities.dsd_u16_be ? "Yes" : "No",
             config->devices[i].capabilities.dsd_u32_le ? "Yes" : "No",
             config->devices[i].capabilities.dsd_u32_be ? "Yes" : "No");

      if (i < config->number_of_devices - 1)
      {
         printf("\n");
      }
   }
}

void
hrmp_sample_configuration(void)
{
   char** hints;
   int err;
   char** n;
   char* name;
   char* desc;
   char* cdesc;
   char* ptr = NULL;
   int dn = 0;
   bool active = false;
   struct configuration* config = NULL;

   config = (struct configuration*)shmem;

   err = snd_device_name_hint(-1, "pcm", (void***)&hints);
   if (err != 0)
   {
      hrmp_log_error("ALSA: Cannot get device names");
      return;
   }

   n = hints;
   while (*n != NULL)
   {
      name = snd_device_name_get_hint(*n, "NAME");
      desc = snd_device_name_get_hint(*n, "DESC");
      cdesc = NULL;

      if (hrmp_starts_with(name, "iec958"))
      {
         config->devices[dn].active = true;

         cdesc = clean_description(desc);

         ptr = strtok(cdesc, ",");

         memcpy(config->devices[dn].name, ptr, strlen(ptr));
         memcpy(config->devices[dn].device, name, strlen(name));

         config->devices[dn].volume = 50;

         ptr = strtok(NULL, ",");
         if (ptr != NULL)
         {
            char* d = NULL;

            d = hrmp_append(d, ptr + 1);
            while ((ptr = strtok(NULL, ",")) != NULL)
            {
               d = hrmp_append(d, ptr);
            }

            memcpy(config->devices[dn].description, d, strlen(d));

            free(d);
            d = NULL;
         }

         hrmp_log_trace("Device: %s (%s)", config->devices[dn].name, config->devices[dn].device);

         dn++;
      }

      free(name);
      name = NULL;
      free(desc);
      desc = NULL;
      free(cdesc);
      cdesc = NULL;

      n++;
   }

   config->number_of_devices = dn;

   snd_device_name_free_hint((void**)hints);

   printf("[hrmp]\n");
   printf("\n");

   for (int i = 0; i < config->number_of_devices; i++)
   {
      if (config->devices[i].active)
      {
         printf("device=%s\n", config->devices[i].name);
         active = true;
         break;
      }
   }

   if (!active && config->number_of_devices > 0)
   {
      printf("# device=%s\n", config->devices[0].name);
   }

   printf("volume = 70\n");
   printf("\n");

   printf("log_type = console\n");
   printf("log_level = debug5\n");
   printf("\n");

   for (int i = 0; i < config->number_of_devices; i++)
   {
      if (config->devices[i].active)
      {
         printf("[%s]\n", config->devices[i].name);
         printf("device=%s\n", config->devices[i].device);
         printf("description=%s\n", config->devices[i].description);
      }
      else
      {
         printf("# [%s]\n", config->devices[i].name);
         printf("# device=%s\n", config->devices[i].device);
         printf("# description=%s\n", config->devices[i].description);
      }

      if (i < config->number_of_devices - 1)
      {
         printf("\n");
      }
   }
}

__attribute__((disable_sanitizer_instrumentation))
int
hrmp_set_master_volume(int volume)
{
   int err;
   long min, max, vol;
   snd_mixer_t* handle = NULL;
   snd_mixer_selem_id_t* sid = NULL;
   snd_mixer_elem_t* elem = NULL;
   const char* card = "default";
   const char* selem_name = "Master";

   if ((err = snd_mixer_open(&handle, 0)) < 0)
   {
      hrmp_log_error("Master volume: Unable to open handle (%s)", snd_strerror(err));
      goto error;
   }

   if ((err = snd_mixer_attach(handle, card)) < 0)
   {
      hrmp_log_error("Master volume: Unable to attach handle (%s)", snd_strerror(err));
      goto error;
   }

   if ((err = snd_mixer_selem_register(handle, NULL, NULL)) < 0)
   {
      hrmp_log_error("Master volume: Unable to register handle (%s)", snd_strerror(err));
      goto error;
   }

   if ((err = snd_mixer_load(handle)) < 0)
   {
      hrmp_log_error("Master volume: Unable to load handle (%s)", snd_strerror(err));
      goto error;
   }

   snd_mixer_selem_id_alloca(&sid);
   if (sid == NULL)
   {
      hrmp_log_error("Master volume: Unable to initialize handle (%s)", snd_strerror(err));
      goto error;
   }

   snd_mixer_selem_id_set_index(sid, 0);
   snd_mixer_selem_id_set_name(sid, selem_name);

   elem = snd_mixer_find_selem(handle, sid);
   if (elem == NULL)
   {
      hrmp_log_error("Master volume: Unable to find handle (%s)", snd_strerror(err));
      goto error;
   }

   if ((err = snd_mixer_selem_get_playback_volume_range(elem, &min, &max)) < 0)
   {
      hrmp_log_error("Master volume: Unable to get playback volume (%s)", snd_strerror(err));
      goto error;
   }

   vol = (long)((volume / 100.0) * (max - min)) + min;

   if ((err = snd_mixer_selem_set_playback_volume_all(elem, vol)) < 0)
   {
      hrmp_log_error("Master volume: Unable to set playback volume (%s)", snd_strerror(err));
      goto error;
   }

   /* if ((err = snd_mixer_detach(handle, card)) < 0) */
   /* { */
   /*    hrmp_log_error("Master volume: Unable to detach handle (%s)", snd_strerror(err)); */
   /*    goto error; */
   /* } */

   if ((err = snd_mixer_close(handle)) < 0)
   {
      hrmp_log_error("Master volume: Unable to close handle (%s)", snd_strerror(err));
      goto error;
   }

   return 0;

error:

   if (handle != NULL)
   {
      if (snd_mixer_close(handle) < 0)
      {
         hrmp_log_error("Master volume: Unable to close handle");
         return 2;
      }
   }

   return 1;
}

static void
check_capabilities(char* device, int index)
{
   int err;
   snd_pcm_t* handle = NULL;
   snd_pcm_hw_params_t* hw = NULL;
   snd_pcm_format_mask_t* fm = NULL;
   struct configuration* config = NULL;

   config = (struct configuration*)shmem;

   memset(&config->devices[index].capabilities, 0, sizeof(struct capabilities));

   if ((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0)
   {
      hrmp_log_error("%s: %s", device, snd_strerror(err));
      goto error;
   }

   snd_pcm_hw_params_alloca(&hw);

   snd_pcm_hw_params_any(handle, hw);
   snd_pcm_hw_params_set_access(handle, hw, SND_PCM_ACCESS_RW_INTERLEAVED);
   snd_pcm_hw_params(handle, hw);

   if (snd_pcm_format_mask_malloc(&fm) != 0)
   {
      goto error;
   }

   snd_pcm_hw_params_get_format_mask(hw, fm);

   /* DSD */
   config->devices[index].capabilities.dsd_u16_le =
      support_mask(device, fm, SND_PCM_FORMAT_DSD_U16_LE);
   config->devices[index].capabilities.dsd_u16_be =
      support_mask(device, fm, SND_PCM_FORMAT_DSD_U16_BE);

   config->devices[index].capabilities.dsd_u32_le =
      support_mask(device, fm, SND_PCM_FORMAT_DSD_U32_LE);
   config->devices[index].capabilities.dsd_u32_be =
      support_mask(device, fm, SND_PCM_FORMAT_DSD_U32_BE);

   /* 32-bit */
   config->devices[index].capabilities.s32 =
      support_mask(device, fm, SND_PCM_FORMAT_S32);
   config->devices[index].capabilities.s32_le =
      support_mask(device, fm, SND_PCM_FORMAT_S32_LE);
   config->devices[index].capabilities.s32_be =
      support_mask(device, fm, SND_PCM_FORMAT_S32_BE);

   config->devices[index].capabilities.u32 =
      support_mask(device, fm, SND_PCM_FORMAT_U32);
   config->devices[index].capabilities.u32_le =
      support_mask(device, fm, SND_PCM_FORMAT_U32_LE);
   config->devices[index].capabilities.u32_be =
      support_mask(device, fm, SND_PCM_FORMAT_U32_BE);

   /* 24-bit */
   config->devices[index].capabilities.s24 =
      support_mask(device, fm, SND_PCM_FORMAT_S24);
   config->devices[index].capabilities.s24_le =
      support_mask(device, fm, SND_PCM_FORMAT_S24_LE);
   config->devices[index].capabilities.s24_be =
      support_mask(device, fm, SND_PCM_FORMAT_S24_BE);

   config->devices[index].capabilities.u24 =
      support_mask(device, fm, SND_PCM_FORMAT_U24);
   config->devices[index].capabilities.u24_le =
      support_mask(device, fm, SND_PCM_FORMAT_U24_LE);
   config->devices[index].capabilities.u24_be =
      support_mask(device, fm, SND_PCM_FORMAT_U24_BE);

   /* 16-bit */
   config->devices[index].capabilities.s16 =
      support_mask(device, fm, SND_PCM_FORMAT_S16);
   config->devices[index].capabilities.s16_le =
      support_mask(device, fm, SND_PCM_FORMAT_S16_LE);
   config->devices[index].capabilities.s16_be =
      support_mask(device, fm, SND_PCM_FORMAT_S16_BE);

   config->devices[index].capabilities.u16 =
      support_mask(device, fm, SND_PCM_FORMAT_U16);
   config->devices[index].capabilities.u16_le =
      support_mask(device, fm, SND_PCM_FORMAT_U16_LE);
   config->devices[index].capabilities.u16_be =
      support_mask(device, fm, SND_PCM_FORMAT_U16_BE);

   snd_pcm_format_mask_free(fm);
   snd_pcm_close(handle);

   return;

error:

   snd_pcm_format_mask_free(fm);
   if (handle != NULL)
   {
      snd_pcm_close(handle);
   }
}

static bool
support_mask(char* device, snd_pcm_format_mask_t* fm, int format)
{
   int err = 0;

   if ((err = snd_pcm_format_mask_test(fm, format)) < 0)
   {
      hrmp_log_debug("%d: %s/%s", format, device, snd_strerror(err));
      return false;
   }

   return true;
}

static char*
clean_description(char* s)
{
   char* result = NULL;
   int length;

   length = strlen(s);

   for (int i = 0; i < length; i++)
   {
      if (s[i] != '\n')
      {
         result = hrmp_append_char(result, s[i]);
      }
      else
      {
         result = hrmp_append_char(result, ' ');
      }
   }

   return result;
}

static bool
is_device_active(char* device)
{
   int err;
   snd_pcm_t* handle = NULL;
   snd_pcm_hw_params_t* hw = NULL;
   char** hints;
   char** n;
   char* name;
   bool found = false;

   err = snd_device_name_hint(-1, "pcm", (void***)&hints);
   if (err != 0)
   {
      hrmp_log_error("ALSA: Cannot get device names");
      goto error;
   }

   n = hints;
   while (!found && *n != NULL)
   {
      name = snd_device_name_get_hint(*n, "NAME");

      if (!strcmp(name, device))
      {
         found = true;
      }

      free(name);
      name = NULL;

      n++;
   }

   if (!found)
   {
      goto error;
   }

   if ((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0)
   {
      /* hrmp_log_debug("Open error: %s/%s", device, snd_strerror(err)); */
      goto error;
   }

   snd_pcm_hw_params_alloca(&hw);

   snd_pcm_hw_params_any(handle, hw);
   snd_pcm_hw_params_set_access(handle, hw, SND_PCM_ACCESS_RW_INTERLEAVED);
   snd_pcm_hw_params(handle, hw);

   snd_pcm_close(handle);

   snd_device_name_free_hint((void**)hints);

   return true;

error:

   if (handle != NULL)
   {
      snd_pcm_close(handle);
   }

   if (hints != NULL)
   {
      snd_device_name_free_hint((void**)hints);
   }

   return false;
}
