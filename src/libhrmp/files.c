/*
 * Copyright (C) 2025 The HighResMusicPlayer community
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* hrmp */
#include <hrmp.h>
#include <files.h>
#include <logging.h>
#include <utils.h>

/* system */
#include <sndfile.h>
#include <stdbool.h>
#include <stdio.h>
#include <alsa/asoundlib.h>

static char* get_format_string(int format);
static int get_metadata(char* filename, unsigned long type, struct file_metadata** file_metadata);

int
hrmp_is_file_supported(char* f)
{
   if (hrmp_ends_with(f, ".flac"))
   {
      return TYPE_FLAC;
   }
   else if (hrmp_ends_with(f, ".wav"))
   {
      return TYPE_WAV;
   }

   return TYPE_UNKNOWN;
}

int
hrmp_file_metadata(char* f, struct file_metadata** fm)
{
   int type = hrmp_is_file_supported(f);
   struct file_metadata* m = NULL;

   *fm = NULL;

   if (type == TYPE_FLAC)
   {
      if (get_metadata(f, SF_FORMAT_FLAC, &m))
      {
         goto error;
      }
   }
   else if (type == TYPE_WAV)
   {
      if (get_metadata(f, SF_FORMAT_WAV, &m))
      {
         goto error;
      }
   }
   else
   {
      goto error;
   }

   *fm = m;

   return 0;

error:

   return 1;
}

bool
hrmp_is_file_metadata_supported(int device, struct file_metadata* fm)
{
   struct configuration* config = NULL;

   config = (struct configuration*)shmem;

   if (device >= 0 && fm != NULL)
   {
      if (fm->bits_per_sample == 16)
      {
         if (config->devices[device].capabilities.s16_le)
         {
            switch (fm->sample_rate)
            {
               case 44100:
               case 48000:
               case 88200:
               case 96000:
               case 176400:
               case 192000:
               case 352800:
               case 384000:
                  return true;
                  break;
               default:
                  if (config->experimental)
                  {
                     switch (fm->sample_rate)
                     {
                        case 705600:
                        case 768000:
                           return true;
                           break;
                        default:
                           break;
                     }
                  }
            }
         }
      }
      else if (fm->bits_per_sample == 24)
      {
         if (config->devices[device].capabilities.s24_3le)
         {
            switch (fm->sample_rate)
            {
               case 44100:
               case 48000:
               case 88200:
               case 96000:
               case 176400:
               case 192000:
               case 352800:
               case 384000:
                  return true;
                  break;
               default:
                  if (config->experimental)
                  {
                     switch (fm->sample_rate)
                     {
                        case 705600:
                        case 768000:
                           return true;
                           break;
                        default:
                           break;
                     }
                  }
            }
         }
      }
      else if (fm->bits_per_sample == 32)
      {
         if (config->devices[device].capabilities.s24_le ||
             config->devices[device].capabilities.s32_le)
         {
            if (fm->type == TYPE_FLAC)
            {
               return false;
            }

            switch (fm->sample_rate)
            {
               case 44100:
               case 48000:
               case 88200:
               case 96000:
               case 176400:
               case 192000:
               case 352800:
               case 384000:
                  return true;
                  break;
               default:
                  if (config->experimental)
                  {
                     switch (fm->sample_rate)
                     {
                        case 705600:
                        case 768000:
                           return true;
                           break;
                        default:
                           break;
                     }
                  }
            }
         }
      }
   }

   printf("%s %d/%dbits (Unsupported)\n", fm->name, fm->sample_rate, fm->bits_per_sample);

   return false;
}

int
hrmp_print_file_metadata(struct file_metadata* fm)
{
   if (fm != NULL)
   {
      printf("%s\n", fm->name);
      if (fm->type == TYPE_UNKNOWN)
      {
         printf("  Type: TYPE_UNKNOWN\n");
      }
      else if (fm->type == TYPE_WAV)
      {
         printf("  Type: TYPE_WAV\n");
      }
      else if (fm->type == TYPE_FLAC)
      {
         printf("  Type: TYPE_FLAC\n");
      }
      printf("  Bits: %d\n", fm->bits_per_sample);
      printf("  Format: %s\n", get_format_string(fm->format));
      printf("  Channels: %d\n", fm->channels);
      printf("  Size: %zu\n", fm->file_size);
      printf("  Rate: %d\n", fm->sample_rate);
      printf("  Samples: %lu\n", fm->total_samples);
      printf("  Duration: %lf\n", fm->duration);
   }

   return 0;
}

static char*
get_format_string(int format)
{
   switch (format)
   {
      case SND_PCM_FORMAT_DSD_U16_LE:
         return "DSD_U16_LE";
         break;
      case SND_PCM_FORMAT_DSD_U16_BE:
         return "DSD_U16_BE";
         break;
      case SND_PCM_FORMAT_DSD_U32_LE:
         return "DSD_U32_LE";
         break;
      case SND_PCM_FORMAT_DSD_U32_BE:
         return "DSD_U32_BE";
         break;
      case SND_PCM_FORMAT_S32_LE:
         return "S32_LE";
         break;
      case SND_PCM_FORMAT_S32_BE:
         return "S32_BE";
         break;
      case SND_PCM_FORMAT_U32_LE:
         return "U32_LE";
         break;
      case SND_PCM_FORMAT_U32_BE:
         return "U32_BE";
         break;
      case SND_PCM_FORMAT_S24_3LE:
         return "S24_3LE";
         break;
      case SND_PCM_FORMAT_S24_LE:
         return "S24_LE";
         break;
      case SND_PCM_FORMAT_S24_BE:
         return "S24_BE";
         break;
      case SND_PCM_FORMAT_U24_LE:
         return "U24_LE";
         break;
      case SND_PCM_FORMAT_U24_BE:
         return "U24_BE";
         break;
      case SND_PCM_FORMAT_S16_LE:
         return "S16_LE";
         break;
      case SND_PCM_FORMAT_S16_BE:
         return "S16_BE";
         break;
      case SND_PCM_FORMAT_U16_LE:
         return "U16_LE";
         break;
      case SND_PCM_FORMAT_U16_BE:
         return "U16_BE";
         break;
      default:
         break;
   }

   return "Unkwown";
}

static int
get_metadata(char* filename, unsigned long type, struct file_metadata** file_metadata)
{
   SNDFILE* f = NULL;
   SF_INFO* info = NULL;
   struct file_metadata* fm = NULL;
   struct configuration* config = NULL;

   *file_metadata = NULL;

   config = (struct configuration*)shmem;

   info = (SF_INFO*)malloc(sizeof(SF_INFO));
   if (info == NULL)
   {
      goto error;
   }
   memset(info, 0, sizeof(SF_INFO));

   fm = (struct file_metadata*)malloc(sizeof(struct file_metadata));
   if (fm == NULL)
   {
      goto error;
   }
   memset(fm, 0, sizeof(struct file_metadata));

   if (type == SF_FORMAT_WAV)
   {
      fm->type = TYPE_WAV;
   }
   else if (type == SF_FORMAT_FLAC)
   {
      fm->type = TYPE_FLAC;
   }

   memcpy(fm->name, filename, strlen(filename));

   f = sf_open(filename, SFM_READ, info);
   if (f == NULL)
   {
      printf("%s (Unsupported due to %s)\n", filename, sf_strerror(f));
      goto error;
   }

   if (!(info->format & type))
   {
      goto error;
   }

   fm->file_size = hrmp_get_file_size(filename);

   fm->sample_rate = info->samplerate;
   fm->channels = info->channels;

   if (fm->channels != 2)
   {
      if (config->experimental)
      {
         printf("Unsupported number of channels for '%s' (%d channels)\n", filename, fm->channels);
      }
      goto error;
   }

   if ((info->format & 0xFF) == SF_FORMAT_PCM_16)
   {
      fm->bits_per_sample = 16;
   }
   else if ((info->format & 0xFF) == SF_FORMAT_PCM_24)
   {
      fm->bits_per_sample = 24;
   }
   else if ((info->format & 0xFF) == SF_FORMAT_PCM_32)
   {
      fm->bits_per_sample = 32;
   }

   fm->total_samples = info->frames;
   if (fm->sample_rate > 0)
   {
      fm->duration = (double)((double)fm->total_samples / fm->sample_rate);
   }
   else
   {
      fm->duration = 0.0;
   }

   switch (fm->bits_per_sample)
   {
      case 16:
         fm->format = SND_PCM_FORMAT_S16_LE;
         break;
      case 24:
         fm->format = SND_PCM_FORMAT_S24_3LE;
         break;
      case 32:
         fm->format = SND_PCM_FORMAT_S32_LE;
         break;
      default:
         hrmp_log_error("Unsupported bit rate for '%s' (%d rate)", filename, fm->bits_per_sample);
         goto error;
   }

   *file_metadata = fm;

   sf_close(f);

   free(info);

   return 0;

error:

   if (f != NULL)
   {
      sf_close(f);
   }

   free(info);
   free(fm);

   return 1;
}
