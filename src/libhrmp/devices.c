/*
 * Copyright (C) 2025 The HighResMusicPlayer community
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
static bool support_mask(char* device, snd_pcm_format_t format);
static char* clean_description(char* s);
static bool is_device_active(char* device);
static int get_hardware_number(char* device);
static char* get_hardware_selem(int hardware);
static bool has_capabilities(struct capabilities c);

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
         char* selem = NULL;

         check_capabilities(config->devices[i].device, i);
         config->devices[i].hardware = get_hardware_number(config->devices[i].name);

         selem = get_hardware_selem(config->devices[i].hardware);
         if (selem != NULL)
         {
            memcpy(config->devices[i].selem, selem, strlen(selem));
         }

         config->devices[i].active = true;

         free(selem);
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
      printf("  Active:    %s\n", config->devices[i].active ? "Yes" : "No");
      printf("  Volume:    %d\n", config->devices[i].volume < 0 ? config->volume : config->devices[i].volume);

      if (config->devices[i].active ||
          has_capabilities(config->devices[i].capabilities))
      {
         printf("  16bit:\n");
         printf("    S16:     %s\n", config->devices[i].capabilities.s16 ? "Yes" : "No");
         printf("    S16_LE:  %s\n", config->devices[i].capabilities.s16_le ? "Yes" : "No");
         printf("    S16_BE:  %s\n", config->devices[i].capabilities.s16_be ? "Yes" : "No");
         printf("    U16:     %s\n", config->devices[i].capabilities.u16 ? "Yes" : "No");
         printf("    U16_LE:  %s\n", config->devices[i].capabilities.u16_le ? "Yes" : "No");
         printf("    U16_BE:  %s\n", config->devices[i].capabilities.u16_be ? "Yes" : "No");
         printf("  24bit:\n");
         printf("    S24:     %s\n", config->devices[i].capabilities.s24 ? "Yes" : "No");
         printf("    S24_3LE: %s\n", config->devices[i].capabilities.s24_3le ? "Yes" : "No");
         printf("    S24_LE:  %s\n", config->devices[i].capabilities.s24_le ? "Yes" : "No");
         printf("    S24_BE:  %s\n", config->devices[i].capabilities.s24_be ? "Yes" : "No");
         printf("    U24:     %s\n", config->devices[i].capabilities.u24 ? "Yes" : "No");
         printf("    U24_LE:  %s\n", config->devices[i].capabilities.u24_le ? "Yes" : "No");
         printf("    U24_BE:  %s\n", config->devices[i].capabilities.u24_be ? "Yes" : "No");
         printf("  32bit:\n");
         printf("    S32:     %s\n", config->devices[i].capabilities.s32 ? "Yes" : "No");
         printf("    S32_LE:  %s\n", config->devices[i].capabilities.s32_le ? "Yes" : "No");
         printf("    S32_BE:  %s\n", config->devices[i].capabilities.s32_be ? "Yes" : "No");
         printf("    U32:     %s\n", config->devices[i].capabilities.u32 ? "Yes" : "No");
         printf("    U32_LE:  %s\n", config->devices[i].capabilities.u32_le ? "Yes" : "No");
         printf("    U32_BE:  %s\n", config->devices[i].capabilities.u32_be ? "Yes" : "No");
         printf("  DSD:\n");
         printf("    U8:      %s\n", config->devices[i].capabilities.dsd_u8 ? "Yes" : "No");
         printf("    U16_LE:  %s\n", config->devices[i].capabilities.dsd_u16_le ? "Yes" : "No");
         printf("    U16_BE:  %s\n", config->devices[i].capabilities.dsd_u16_be ? "Yes" : "No");
         printf("    U32_LE:  %s\n", config->devices[i].capabilities.dsd_u32_le ? "Yes" : "No");
         printf("    U32_BE:  %s\n", config->devices[i].capabilities.dsd_u32_be ? "Yes" : "No");
      }
      else
      {
         printf("  16bit:\n");
         printf("    S16:     Unknown\n");
         printf("    S16_LE:  Unknown\n");
         printf("    S16_BE:  Unknown\n");
         printf("    U16:     Unknown\n");
         printf("    U16_LE:  Unknown\n");
         printf("    U16_BE:  Unknown\n");
         printf("  24bit:\n");
         printf("    S24:     Unknown\n");
         printf("    S24_3LE: Unknown\n");
         printf("    S24_LE:  Unknown\n");
         printf("    S24_BE:  Unknown\n");
         printf("    U24:     Unknown\n");
         printf("    U24_LE:  Unknown\n");
         printf("    U24_BE:  Unknown\n");
         printf("  32bit:\n");
         printf("    S32:     Unknown\n");
         printf("    S32_LE:  Unknown\n");
         printf("    S32_BE:  Unknown\n");
         printf("    U32:     Unknown\n");
         printf("    U32_LE:  Unknown\n");
         printf("    U32_BE:  Unknown\n");
         printf("  DSD:\n");
         printf("    U8:      Unknown\n");
         printf("    U16_LE:  Unknown\n");
         printf("    U16_BE:  Unknown\n");
         printf("    U32_LE:  Unknown\n");
         printf("    U32_BE:  Unknown\n");
      }

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
         cdesc = clean_description(desc);

         ptr = strtok(cdesc, ",");

         memcpy(config->devices[dn].name, ptr, strlen(ptr));
         memcpy(config->devices[dn].device, name, strlen(name));

         if (is_device_active(config->devices[dn].device))
         {
            config->devices[dn].active = true;
         }

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
      printf("device=%s\n", config->devices[0].name);
   }

   printf("\n");

   printf("log_type = console\n");
   printf("log_level = info\n");
   printf("\n");

   for (int i = 0; i < config->number_of_devices; i++)
   {
      printf("[%s]\n", config->devices[i].name);
      printf("device=%s\n", config->devices[i].device);
      printf("description=%s\n", config->devices[i].description);

      if (i < config->number_of_devices - 1)
      {
         printf("\n");
      }
   }
}

static void
check_capabilities(char* device, int index)
{
   struct configuration* config = NULL;

   config = (struct configuration*)shmem;

   memset(&config->devices[index].capabilities, 0, sizeof(struct capabilities));

   /* DSD */
   config->devices[index].capabilities.dsd_u8 =
      support_mask(device, SND_PCM_FORMAT_DSD_U8);
   config->devices[index].capabilities.dsd_u16_le =
      support_mask(device, SND_PCM_FORMAT_DSD_U16_LE);
   config->devices[index].capabilities.dsd_u16_be =
      support_mask(device, SND_PCM_FORMAT_DSD_U16_BE);

   config->devices[index].capabilities.dsd_u32_le =
      support_mask(device, SND_PCM_FORMAT_DSD_U32_LE);
   config->devices[index].capabilities.dsd_u32_be =
      support_mask(device, SND_PCM_FORMAT_DSD_U32_BE);

   /* 32-bit */
   config->devices[index].capabilities.s32 =
      support_mask(device, SND_PCM_FORMAT_S32);
   config->devices[index].capabilities.s32_le =
      support_mask(device, SND_PCM_FORMAT_S32_LE);
   config->devices[index].capabilities.s32_be =
      support_mask(device, SND_PCM_FORMAT_S32_BE);

   config->devices[index].capabilities.u32 =
      support_mask(device, SND_PCM_FORMAT_U32);
   config->devices[index].capabilities.u32_le =
      support_mask(device, SND_PCM_FORMAT_U32_LE);
   config->devices[index].capabilities.u32_be =
      support_mask(device, SND_PCM_FORMAT_U32_BE);

   /* 24-bit */
   config->devices[index].capabilities.s24 =
      support_mask(device, SND_PCM_FORMAT_S24);
   config->devices[index].capabilities.s24_3le =
      support_mask(device, SND_PCM_FORMAT_S24_3LE);
   config->devices[index].capabilities.s24_le =
      support_mask(device, SND_PCM_FORMAT_S24_LE);
   config->devices[index].capabilities.s24_be =
      support_mask(device, SND_PCM_FORMAT_S24_BE);

   config->devices[index].capabilities.u24 =
      support_mask(device, SND_PCM_FORMAT_U24);
   config->devices[index].capabilities.u24_le =
      support_mask(device, SND_PCM_FORMAT_U24_LE);
   config->devices[index].capabilities.u24_be =
      support_mask(device, SND_PCM_FORMAT_U24_BE);

   /* 16-bit */
   config->devices[index].capabilities.s16 =
      support_mask(device, SND_PCM_FORMAT_S16);
   config->devices[index].capabilities.s16_le =
      support_mask(device, SND_PCM_FORMAT_S16_LE);
   config->devices[index].capabilities.s16_be =
      support_mask(device, SND_PCM_FORMAT_S16_BE);

   config->devices[index].capabilities.u16 =
      support_mask(device, SND_PCM_FORMAT_U16);
   config->devices[index].capabilities.u16_le =
      support_mask(device, SND_PCM_FORMAT_U16_LE);
   config->devices[index].capabilities.u16_be =
      support_mask(device, SND_PCM_FORMAT_U16_BE);
}

static bool
support_mask(char* device, snd_pcm_format_t format)
{
   int err;
   snd_pcm_t* h = NULL;
   snd_pcm_hw_params_t* hw = NULL;

   if ((err = snd_pcm_open(&h, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0)
   {
      goto format_no;
   }

   if ((err = snd_pcm_hw_params_malloc(&hw)) < 0)
   {
      goto format_no;
   }

   if ((err = snd_pcm_hw_params_any(h, hw)) < 0)
   {
      goto format_no;
   }

   if ((err = snd_pcm_hw_params_set_rate_resample(h, hw, 0)) < 0)
   {
      goto format_no;
   }

   if ((err = snd_pcm_hw_params_set_access(h, hw, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
   {
      goto format_no;
   }

   if ((err = snd_pcm_hw_params_set_format(h, hw, format)) < 0)
   {
      goto format_no;
   }

   if ((err = snd_pcm_hw_params_set_channels(h, hw, 2)) < 0)
   {
      goto format_no;
   }

   snd_pcm_hw_params_free(hw);
   snd_pcm_close(h);

   return true;

format_no:

   if (hw != NULL)
   {
      snd_pcm_hw_params_free(hw);
   }

   if (h != NULL)
   {
      snd_pcm_close(h);
   }

   return false;
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

static int
get_hardware_number(char* device)
{
   int result = -1;
   int err = 0;
   int card = -1;
   snd_ctl_t* ctl = NULL;
   snd_ctl_card_info_t* info = NULL;
   char ctlname[32];
   char* name = NULL;

   if ((err = snd_card_next(&card)) < 0)
   {
      hrmp_log_error("snd_card_next failed: %s", snd_strerror(err));
      goto done;
   }

   if (card < 0)
   {
      goto done;
   }

   while (card >= 0 && result == -1)
   {
      hrmp_snprintf(ctlname, sizeof(ctlname), "hw:%d", card);

      err = snd_ctl_open(&ctl, ctlname, 0);
      if (err < 0)
      {
         hrmp_log_error("snd_ctl_open(%s) failed: %s", ctlname, snd_strerror(err));
         /* try next card */
         if ((err = snd_card_next(&card)) < 0)
         {
            hrmp_log_error("snd_card_next failed: %s", snd_strerror(err));
            break;
         }
         continue;
      }

      snd_ctl_card_info_malloc(&info);
      if (!info)
      {
         hrmp_log_error("snd_ctl_card_info_malloc failed");
         snd_ctl_close(ctl);
         goto done;
      }

      err = snd_ctl_card_info(ctl, info);
      if (err < 0)
      {
         hrmp_log_error("snd_ctl_card_info failed for %s: %s", ctlname, snd_strerror(err));
         snd_ctl_card_info_free(info);
         snd_ctl_close(ctl);
         if ((err = snd_card_next(&card)) < 0)
         {
            hrmp_log_error("snd_card_next failed: %s", snd_strerror(err));
            break;
         }
         continue;
      }

      name = (char*)snd_ctl_card_info_get_name(info);

      if (!strcmp(device, name))
      {
         result = card;
      }

      snd_ctl_card_info_free(info);
      snd_ctl_close(ctl);

      info = NULL;
      ctl = NULL;

      if ((err = snd_card_next(&card)) < 0)
      {
         hrmp_log_error("snd_card_next failed: %s", snd_strerror(err));
         break;
      }
   }

done:

   if (info != NULL)
   {
      snd_ctl_card_info_free(info);
   }

   if (ctl != NULL)
   {
      snd_ctl_close(ctl);
   }

   return result;
}

static char*
get_hardware_selem(int hardware)
{
   char* result = NULL;
   snd_mixer_t* mixer = NULL;
   int err;
   char card[MISC_LENGTH];
   snd_mixer_elem_t* elem = NULL;
   unsigned int count = 0;

   if ((err = snd_mixer_open(&mixer, 0)) < 0)
   {
      hrmp_log_error("snd_mixer_open failed: %s", snd_strerror(err));
      goto error;
   }

   memset(&card[0], 0, sizeof(card));
   hrmp_snprintf(&card[0], sizeof(card), "hw:%d", hardware);

   if ((err = snd_mixer_attach(mixer, card)) < 0)
   {
      hrmp_log_error("snd_mixer_attach: %s", snd_strerror(err));
      goto error;
   }

   if ((err = snd_mixer_selem_register(mixer, NULL, NULL)) < 0)
   {
      hrmp_log_error("snd_mixer_selem_register: %s", snd_strerror(err));
      goto error;
   }

   if ((err = snd_mixer_load(mixer)) < 0)
   {
      hrmp_log_error("snd_mixer_load: %s", snd_strerror(err));
      goto error;
   }

   count = snd_mixer_get_count(mixer);
   if (count == 1)
   {
      snd_mixer_selem_id_t* sid;
      char* name = NULL;

      elem = snd_mixer_first_elem(mixer);

      snd_mixer_selem_id_alloca(&sid);
      snd_mixer_selem_get_id(elem, sid);
      name = (char*)snd_mixer_selem_id_get_name(sid);

      result = strdup(name);
   }
   else
   {
      elem = snd_mixer_first_elem(mixer);
      for (; elem && result == NULL; elem = snd_mixer_elem_next(elem))
      {
         snd_mixer_selem_id_t* sid;
         char* name = NULL;

         snd_mixer_selem_id_alloca(&sid);
         snd_mixer_selem_get_id(elem, sid);
         name = (char*)snd_mixer_selem_id_get_name(sid);

         if (!strcmp("Master", name))
         {
            result = strdup(name);
         }
      }
   }

   snd_mixer_close(mixer);

   return result;

error:

   if (mixer != NULL)
   {
      snd_mixer_close(mixer);
   }

   return NULL;
}

static bool
has_capabilities(struct capabilities c)
{
   if (c.s16 || c.s16_le || c.s16_be || c.u16 || c.u16_le || c.u16_be ||
       c.s24 || c.s24_3le || c.s24_le || c.s24_be || c.u24 || c.u24_le ||
       c.u24_be || c.s32 || c.s32_le || c.s32_be || c.u32 || c.u32_le ||
       c.u32_be || c.dsd_u8 || c.dsd_u16_le || c.dsd_u16_be || c.dsd_u32_le ||
       c.dsd_u32_be)
   {
      return true;
   }

   return false;
}
