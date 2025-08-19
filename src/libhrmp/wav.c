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

#include "utils.h"
#include <hrmp.h>
#include <files.h>
#include <logging.h>
#include <wav.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <alsa/asoundlib.h>
#include <arpa/inet.h>

static void wav_print_header(struct wav_header* header);

int
hrmp_wav_get_metadata(char* filename, struct file_metadata** file_metadata)
{
   size_t ret;
   struct file_metadata* fm = NULL;
   struct wav* wav = NULL;

   *file_metadata = NULL;

   wav = (struct wav*)malloc(sizeof(struct wav));
   if (wav == NULL)
   {
      hrmp_log_error("Unable to allocate for '%s'", filename);
      goto error;
   }

   memset(wav, 0, sizeof(struct wav));

   wav->file = fopen(filename, "rb");
   if (wav->file == NULL)
   {
      hrmp_log_error("Unable to open '%s'", filename);
      goto error;
   }

   ret = fread(&wav->header, 1, sizeof(struct wav_header), wav->file);
   if (ret < sizeof(struct wav_header))
   {
      hrmp_log_error("Unable to read '%s'", filename);
      goto error;
   }

   /* wav_print_header(&wav->header); */

   fm = (struct file_metadata*)malloc(sizeof(struct file_metadata));
   if (fm == NULL)
   {
      hrmp_log_error("Unable to allocate for '%s'", filename);
      goto error;
   }

   memset(fm, 0, sizeof(struct file_metadata));

   fm->type = TYPE_WAV;
   memcpy(fm->name, filename, strlen(filename));

   fm->file_size = hrmp_get_file_size(filename);

   fm->sample_rate = wav->header.sample_rate;
   fm->channels = wav->header.channels;

   if (fm->channels != 2)
   {
      hrmp_log_error("Unsupported number of channels for '%s' (%d channels)", filename, fm->channels);
      goto error;
   }

   fm->bits_per_sample = wav->header.bits_per_sample;
   fm->total_samples = fm->file_size / (wav->header.bits_per_sample / 8);

   if (wav->header.sample_rate > 0)
   {
      fm->duration = (double)((double)fm->total_samples / wav->header.sample_rate);
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
      case 32:
         fm->format = SND_PCM_FORMAT_S32_LE;
         break;
      default:
         hrmp_log_error("Unsupported bit rate for '%s' (%d rate)", filename, fm->bits_per_sample);
         goto error;
   }

   hrmp_wav_close(wav);

   *file_metadata = fm;

   return 0;

error:

   hrmp_wav_close(wav);

   return 1;
}

int
hrmp_wav_open(char* path, enum wav_channel_format chanfmt, struct wav** w)
{
   /* bool additional_data = false; */
   size_t ret;
   struct wav* wav;

   *w = NULL;

   wav = (struct wav*)malloc(sizeof(struct wav));
   wav->file = fopen(path, "rb");
   if (wav->file == NULL)
   {
      hrmp_log_error("Could not open WAV for '%s'", path);
      goto error;
   }

   ret = fread(&wav->header, 1, sizeof(struct wav_header), wav->file);
   if (ret < sizeof(struct wav_header))
   {
      hrmp_log_error("Could not read WAV header for '%s'", path);
      goto error;
   }

   /* /\* Skip over any other chunks before the "data" chunk *\/ */
   /* while (wav->header.subchunk2_id != htonl(0x64617461)) */
   /* { */
   /*    fseek(wav->file, 4, SEEK_CUR); */
   /*    ret = fread(&wav->header.subchunk2_id, 4, 1, wav->file); */

   /*    if (ret < 4) */
   /*    { */
   /*      hrmp_log_error("Could not read WAV subchunk2_id for '%s'", path); */
   /*      goto error; */
   /*    } */

   /*    additional_data = true; */
   /* } */

   /* /\* Update the value of subchunk2_size *\/ */
   /* if (additional_data) */
   /* { */
   /*    ret = fread(&wav->header.subchunk2_size, 4, 1, wav->file); */

   /*    if (ret < 4) */
   /*    { */
   /*      hrmp_log_error("Could not read WAV subchunk2_size for '%s'", path); */
   /*      goto error; */
   /*    } */
   /* } */

   wav->channel_format = chanfmt;

   if (wav->header.bits_per_sample == 32 && wav->header.audio_format == 3)
   {
      wav->sample_format = WAV_FLOAT32;
   }
   else if (wav->header.bits_per_sample == 16 && wav->header.audio_format == 1)
   {
      wav->sample_format = WAV_INT16;
   }
   else
   {
      hrmp_log_error("WAV file has %d bits per sample and audio format %d which isn't supported yet",
                     wav->header.bits_per_sample, wav->header.audio_format);
      wav->sample_format = WAV_FLOAT32;
   }

   wav->number_of_frames = wav->header.subchunk2_size / (wav->header.channels * wav->sample_format);
   wav->total_frames_read = 0;

   *w = wav;

   return 0;

error:

   return 1;
}

void
hrmp_wav_close(struct wav* wav)
{
   if (wav != NULL)
   {
      if (wav->file != NULL)
      {
         fclose(wav->file);
      }
      wav->file = NULL;

      if (wav->buffer != NULL)
      {
         free(wav->buffer);
      }
      wav->buffer = NULL;
   }

   free(wav);
   wav = NULL;
}

__attribute__((used))
static void
wav_print_header(struct wav_header* header)
{
   printf("chunk_id: %c%c%c%c\n",
          header->chunk_id[0], header->chunk_id[1],
          header->chunk_id[2], header->chunk_id[3]);
   printf("chunk_size: %" PRIu32 "\n", header->chunk_size);
   printf("format: %c%c%c%c\n",
          header->format[0], header->format[1],
          header->format[2], header->format[3]);
   printf("subchunk1_id: %" PRIu32 "\n", header->subchunk1_id);
   printf("subchunk1_size: %" PRIu32 "\n", header->subchunk1_size);
   printf("audio_format: %" PRIu16 "\n", header->audio_format);
   printf("channels: %" PRIu16 "\n", header->channels);
   printf("sample_rate: %" PRIu32 "\n", header->sample_rate);
   printf("byte_rate: %" PRIu32 "\n", header->byte_rate);
   printf("block_align: %" PRIu16 "\n", header->block_align);
   printf("bits_per_sample: %" PRIu16 "\n", header->bits_per_sample);
   printf("subchunk2_id: %" PRIu32 "\n", header->subchunk2_id);
   printf("subchunk2_size: %" PRIu32 "\n", header->subchunk2_size);
}
