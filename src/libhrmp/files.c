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
#include <files.h>
#include <utils.h>

/* system */
#include <stdio.h>
#include <alsa/asoundlib.h>

static char* get_format_string(int format);

int
hrmp_is_file_supported(char* f)
{
   if (hrmp_ends_with(f, ".flac"))
   {
      return TYPE_FLAC;
   }

   return TYPE_UNKNOWN;
}

bool
hrmp_is_file_metadata_supported(int device, struct file_metadata* fm)
{
   struct configuration* config = NULL;

   config = (struct configuration*)shmem;

   if (device >= 0 && fm != NULL)
   {
      if ((fm->bits_per_sample == 16 && config->devices[device].capabilities.s16_le) ||
          (fm->bits_per_sample == 24 && config->devices[device].capabilities.s24_le) ||
          (fm->bits_per_sample == 32 && config->devices[device].capabilities.s32_le))
      {
         if (fm->sample_rate == 44100)
         {
            return true;
         }
      }
   }

   return false;
}

int
hrmp_print_file_metadata(struct file_metadata* fm)
{
   if (fm != NULL)
   {
      printf("%s\n", fm->name);
      printf("  Bits: %d\n", fm->bits_per_sample);
      printf("  Format: %s\n", get_format_string(fm->format));
      printf("  Rate: %d\n", fm->sample_rate);
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
