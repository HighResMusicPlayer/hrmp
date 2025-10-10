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
#include <devices.h>
#include <files.h>
#include <logging.h>
#include <utils.h>

/* system */
#include <sndfile.h>
#include <stdbool.h>
#include <stdio.h>
#include <alsa/asoundlib.h>

static int init_metadata(char* filename, int type, struct file_metadata** file_metadata);
static int get_metadata(char* filename, int type, struct file_metadata** file_metadata);
static int get_metadata_dsf(int device, char* filename, struct file_metadata** file_metadata);
static bool metadata_supported(int device, struct file_metadata* fm);

int
hrmp_file_metadata(int device, char* f, struct file_metadata** fm)
{
   int type = TYPE_UNKNOWN;
   struct file_metadata* m = NULL;
   struct configuration* config = NULL;

   config = (struct configuration*)shmem;

   *fm = NULL;

   if (hrmp_ends_with(f, ".wav"))
   {
      type = TYPE_WAV;
   }
   else if (hrmp_ends_with(f, ".flac"))
   {
      type = TYPE_FLAC;
   }
   else if (hrmp_ends_with(f, ".mp3"))
   {
      type = TYPE_MP3;
   }
   else if (hrmp_ends_with(f, ".dsf"))
   {
      type = TYPE_DSF;
   }

   if (type == TYPE_WAV || type == TYPE_FLAC || type == TYPE_MP3)
   {
      if (get_metadata(f, type, &m))
      {
         if (!config->quiet)
         {
            printf("Unsupported metadata for %s\n", f);
         }
         goto error;
      }
   }
   else if (type == TYPE_DSF)
   {
      if (get_metadata_dsf(device, f, &m))
      {
         if (!config->quiet)
         {
            printf("Unsupported metadata for %s\n", f);
         }
         goto error;
      }
   }
   else
   {
      if (!config->quiet)
      {
         printf("Unsupported file extension for %s\n", f);
      }
      goto error;
   }

   if (!metadata_supported(device, m))
   {
      if (!config->quiet)
      {
         printf("Unsupported file: %s/ch%d/%dHz/%dbits\n", f, m->channels,
                m->sample_rate, m->bits_per_sample);
      }
      goto error;
   }

   *fm = m;

   return 0;

error:

   free(m);

   return 1;
}

static bool
metadata_supported(int device, struct file_metadata* fm)
{
   struct configuration* config = NULL;

   config = (struct configuration*)shmem;

   if (device >= 0 && fm != NULL)
   {
      if (fm->channels != 2)
      {
         return false;
      }

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
         if (config->devices[device].capabilities.s24_3le ||
             config->devices[device].capabilities.s32_le)
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
         if (config->devices[device].capabilities.s24_3le ||
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
      else if (fm->bits_per_sample == 1)
      {
         if (config->devices[device].capabilities.dsd_u8 ||
             config->devices[device].capabilities.dsd_u16_le ||
             config->devices[device].capabilities.dsd_u16_be ||
             config->devices[device].capabilities.dsd_u32_le ||
             config->devices[device].capabilities.dsd_u32_be)
         {
            switch (fm->sample_rate)
            {
               case 2822400:
               case 5644800:
               case 11289600:
                  return true;
                  break;
               case 22579200:
                  if (config->dop)
                  {
                     return false;
                  }
                  else
                  {
                     return true;
                  }
                  break;
               default:
                  if (config->experimental)
                  {
                     switch (fm->sample_rate)
                     {
                        case 45158400:
                           return true;
                           break;
                        default:
                           break;
                     }
                  }
                  break;
            }
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
      else if (fm->type == TYPE_MP3)
      {
         printf("  Type: TYPE_MP3\n");
      }
      else if (fm->type == TYPE_DSF)
      {
         printf("  Type: TYPE_DSF\n");
      }
      else
      {
         printf("  Type: TYPE_UNKNOWN (%d)\n", fm->type);
      }
      if (fm->format == FORMAT_1)
      {
         printf("  Format: FORMAT_1\n");
      }
      else if (fm->format == FORMAT_16)
      {
         printf("  Format: FORMAT_16\n");
      }
      else if (fm->format == FORMAT_24)
      {
         printf("  Format: FORMAT_24\n");
      }
      else if (fm->format == FORMAT_32)
      {
         printf("  Format: FORMAT_32\n");
      }
      else
      {
         printf("  Format: FORMAT_UNKNOWN (%d)\n", fm->format);
      }
      printf("  Bits: %d\n", fm->bits_per_sample);
      printf("  Container: %d\n", fm->container);
      printf("  Channels: %d\n", fm->channels);
      printf("  Size: %zu\n", fm->file_size);
      printf("  Rate: %d Hz\n", fm->sample_rate);
      printf("  PCM: %d Hz\n", fm->pcm_rate);

      if (fm->alsa_snd == SND_PCM_FORMAT_S16)
      {
         printf("  ALSA: SND_PCM_FORMAT_S16\n");
      }
      else if (fm->alsa_snd == SND_PCM_FORMAT_S16_LE)
      {
         printf("  ALSA: SND_PCM_FORMAT_S16_LE\n");
      }
      else if (fm->alsa_snd == SND_PCM_FORMAT_S16_BE)
      {
         printf("  ALSA: SND_PCM_FORMAT_S16_BE\n");
      }
      else if (fm->alsa_snd == SND_PCM_FORMAT_S24)
      {
         printf("  ALSA: SND_PCM_FORMAT_S24\n");
      }
      else if (fm->alsa_snd == SND_PCM_FORMAT_S24_3LE)
      {
         printf("  ALSA: SND_PCM_FORMAT_S24_3LE\n");
      }
      else if (fm->alsa_snd == SND_PCM_FORMAT_S24_LE)
      {
         printf("  ALSA: SND_PCM_FORMAT_S24_LE\n");
      }
      else if (fm->alsa_snd == SND_PCM_FORMAT_S24_3BE)
      {
         printf("  ALSA: SND_PCM_FORMAT_S24_3BE\n");
      }
      else if (fm->alsa_snd == SND_PCM_FORMAT_S24_BE)
      {
         printf("  ALSA: SND_PCM_FORMAT_S24_BE\n");
      }
      else if (fm->alsa_snd == SND_PCM_FORMAT_S32)
      {
         printf("  ALSA: SND_PCM_FORMAT_S32\n");
      }
      else if (fm->alsa_snd == SND_PCM_FORMAT_S32_LE)
      {
         printf("  ALSA: SND_PCM_FORMAT_S32_LE\n");
      }
      else if (fm->alsa_snd == SND_PCM_FORMAT_S32_BE)
      {
         printf("  ALSA: SND_PCM_FORMAT_S32_BE\n");
      }
      else if (fm->alsa_snd == SND_PCM_FORMAT_DSD_U8)
      {
         printf("  ALSA: SND_PCM_FORMAT_DSD_U8\n");
      }
      else if (fm->alsa_snd == SND_PCM_FORMAT_DSD_U16_LE)
      {
         printf("  ALSA: SND_PCM_FORMAT_DSD_U16_LE\n");
      }
      else if (fm->alsa_snd == SND_PCM_FORMAT_DSD_U32_LE)
      {
         printf("  ALSA: SND_PCM_FORMAT_DSD_U32_LE\n");
      }
      else if (fm->alsa_snd == SND_PCM_FORMAT_DSD_U16_BE)
      {
         printf("  ALSA: SND_PCM_FORMAT_DSD_U16_BE\n");
      }
      else if (fm->alsa_snd == SND_PCM_FORMAT_DSD_U32_BE)
      {
         printf("  ALSA: SND_PCM_FORMAT_DSD_U32_BE\n");
      }
      else
      {
         printf("  ALSA: UNKNOWN (%d)\n", fm->alsa_snd);
      }
      printf("  Samples: %lu\n", fm->total_samples);
      printf("  Duration: %lf\n", fm->duration);
      printf("  Block size: %d\n", fm->block_size);
      printf("  Data size: %lu\n", fm->data_size);
   }

   return 0;
}

static int
init_metadata(char* filename, int type, struct file_metadata** file_metadata)
{
   struct file_metadata* fm = NULL;

   *file_metadata = NULL;

   fm = (struct file_metadata*)malloc(sizeof(struct file_metadata));
   if (fm == NULL)
   {
      goto error;
   }
   memset(fm, 0, sizeof(struct file_metadata));

   fm->type = type;
   memcpy(fm->name, filename, strlen(filename));
   fm->format = type;
   fm->file_size = hrmp_get_file_size(filename);
   fm->sample_rate = 0;
   fm->pcm_rate = 0;
   fm->channels = 0;
   fm->bits_per_sample = 0;
   if (type == TYPE_MP3)
   {
      fm->bits_per_sample = 16;
   }
   fm->total_samples = 0;
   fm->duration = 0.0;
   fm->alsa_snd = 0;

   *file_metadata = fm;

   return 0;

error:

   free(fm);

   return 1;
}

static int
get_metadata(char* filename, int type, struct file_metadata** file_metadata)
{
   SNDFILE* f = NULL;
   SF_INFO* info = NULL;
   struct file_metadata* fm = NULL;
   struct configuration* config = NULL;

   *file_metadata = NULL;

   config = (struct configuration*)shmem;

   if (init_metadata(filename, type, &fm))
   {
      goto error;
   }

   if (type == TYPE_WAV || type == TYPE_FLAC || type == TYPE_MP3)
   {
      info = (SF_INFO*)malloc(sizeof(SF_INFO));
      if (info == NULL)
      {
         goto error;
      }
      memset(info, 0, sizeof(SF_INFO));

      f = sf_open(filename, SFM_READ, info);
      if (f == NULL)
      {
         printf("%s (Unsupported due to %s)\n", filename, sf_strerror(f));
         goto error;
      }

      if (type == TYPE_WAV)
      {
         if (!(info->format & SF_FORMAT_WAV))
         {
            goto error;
         }
      }
      else if (type == TYPE_FLAC)
      {
         if (!(info->format & SF_FORMAT_FLAC))
         {
            goto error;
         }
      }
      else if (type == TYPE_MP3)
      {
         if (!(info->format & SF_FORMAT_MPEG_LAYER_III))
         {
            goto error;
         }
      }
      else
      {
         goto error;
      }

      fm->sample_rate = info->samplerate;
      fm->pcm_rate = info->samplerate;
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
   }

   fm->file_size = hrmp_get_file_size(filename);

   *file_metadata = fm;

   if (f != NULL)
   {
      sf_close(f);
   }

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

static int
get_metadata_dsf(int device, char* filename, struct file_metadata** file_metadata)
{
   FILE* f = NULL;
   char id4[5] = {0};
   uint32_t channel_number = 0;
   uint32_t srate = 0;
   uint32_t bps = 0;
   uint64_t samples = 0;
   uint32_t block_size = 0;
   uint64_t data_size = 0;
   struct file_metadata* fm = NULL;
   struct configuration* config = NULL;

   config = (struct configuration*)shmem;

   *file_metadata = NULL;

   if (init_metadata(filename, TYPE_DSF, &fm))
   {
      goto error;
   }

   f = fopen(filename, "rb");
   if (f == NULL)
   {
      hrmp_log_error("fopen input: %s", filename);
      goto error;
   }

   memset(&id4[0], 0, sizeof(id4));
   if (fread(id4, 1, 4, f) != 4)
   {
      hrmp_log_error("Failed to read file id\n");
      goto error;
   }

   if (strncmp(id4, "DSD ", 4) != 0)
   {
      hrmp_log_error("Not a DSF file (missing 'DSD ' header). Read: '%.4s'\n", id4);
      goto error;
   }

   hrmp_read_le_u64(f); /* chunk_size */
   hrmp_read_le_u64(f); /* file_size */
   hrmp_read_le_u64(f); /* metadata_chunk */

   memset(&id4[0], 0, sizeof(id4));
   if (fread(id4, 1, 4, f) != 4)
   {
      hrmp_log_error("Failed to read file format\n");
      goto error;
   }

   if (strncmp(id4, "fmt ", 4) != 0)
   {
      hrmp_log_error("Not a format header (missing 'fmt ' header). Read: '%.4s'\n", id4);
      goto error;
   }

   hrmp_read_le_u64(f); /* format_chunk */
   hrmp_read_le_u32(f); /* format_version */
   hrmp_read_le_u32(f); /* format_id */
   hrmp_read_le_u32(f); /* channel_type */
   channel_number = hrmp_read_le_u32(f);
   srate = hrmp_read_le_u32(f);
   bps = hrmp_read_le_u32(f);
   samples = hrmp_read_le_u64(f);
   block_size = hrmp_read_le_u32(f);
   hrmp_read_le_u32(f); /* reserved */

   hrmp_read_le_u32(f); /* DATA */
   data_size = hrmp_read_le_u64(f); /* data_size */

   if (srate % 16)
   {
      hrmp_log_error("Error: DSD sample rate is not divisible by 16 (%lu)", srate);
      goto error;
   }

   fm->format = FORMAT_1;
   fm->file_size = hrmp_get_file_size(filename);
   fm->sample_rate = srate;
   if (config->dop && (config->devices[device].capabilities.s32 ||
                       config->devices[device].capabilities.s32_le))
   {
      fm->pcm_rate = srate / 16;
   }
   else
   {
      fm->pcm_rate = srate / 32;
   }
   fm->channels = channel_number;
   fm->bits_per_sample = bps;
   fm->total_samples = samples;
   fm->duration = (double)((double)samples / srate);
   fm->block_size = block_size;
   fm->data_size = data_size;

   *file_metadata = fm;

   fclose(f);

   return 0;

error:

   if (f != NULL)
   {
      fclose(f);
   }

   free(fm);

   return 1;
}
