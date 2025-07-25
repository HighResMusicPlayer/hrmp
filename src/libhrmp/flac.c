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
#include <playback.h>

/* system */
#include <alsa/asoundlib.h>
#include <FLAC/metadata.h>

int
hrmp_flac_get_metadata(char* filename, struct file_metadata** file_metadata)
{
   FLAC__StreamMetadata* metadata = NULL;
   FLAC__StreamMetadata_StreamInfo* info = NULL;
   struct file_metadata* fm = NULL;

   *file_metadata = NULL;

   metadata = (FLAC__StreamMetadata*)malloc(sizeof(FLAC__StreamMetadata));
   if (metadata == NULL)
   {
      goto error;
   }
   memset(metadata, 0, sizeof(FLAC__StreamMetadata));

   fm = (struct file_metadata*)malloc(sizeof(struct file_metadata));
   if (fm == NULL)
   {
      goto error;
   }
   memset(fm, 0, sizeof(struct file_metadata));

   fm->type = TYPE_FLAC;
   memcpy(fm->name, filename, strlen(filename));

   if (!FLAC__metadata_get_streaminfo(filename, metadata))
   {
      hrmp_log_error("Error reading FLAC metadata for '%s'", filename);
      goto error;
   }

   if (metadata->type != FLAC__METADATA_TYPE_STREAMINFO)
   {
      hrmp_log_error("No STREAMINFO block found for '%s'", filename);
      goto error;
   }

   info = &metadata->data.stream_info;

   fm->sample_rate = info->sample_rate;
   fm->channels = info->channels;

   if (fm->channels != 2)
   {
      hrmp_log_error("Unsupported number of channels for '%s' (%d channels)", filename, fm->channels);
      goto error;
   }

   fm->bits_per_sample = info->bits_per_sample;
   fm->total_samples = info->total_samples;
   if (info->sample_rate > 0)
   {
      fm->duration = (double)((double)info->total_samples / info->sample_rate);
   }
   else
   {
      fm->duration = 0.0;
   }
   fm->min_blocksize = info->min_blocksize;
   fm->max_blocksize = info->max_blocksize;
   fm->min_framesize = info->min_framesize;
   fm->min_framesize = info->min_framesize;

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

   *file_metadata = fm;

   FLAC__metadata_object_delete(metadata);

   return 0;

error:

   if (metadata != NULL)
   {
      FLAC__metadata_object_delete(metadata);
   }

   free(fm);

   return 1;
}
