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

#include <hrmp.h>
#include <logging.h>
#include <wav.h>

#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

int
hrmp_wav_open(char* path, enum wav_channel_format chanfmt, struct wav** w)
{
   bool additional_data = false;
   size_t ret;
   struct wav* wav;

   *w = NULL;

   wav = (struct wav*)malloc(sizeof(struct wav));
   wav->file = fopen(path, "rb");

   ret = fread(&wav->header, sizeof(struct wav_header), 1, wav->file);
   if (ret < sizeof(struct wav_header))
   {
      goto error;
   }

   /* Skip over any other chunks before the "data" chunk */
   while (wav->header.subchunk2_id != htonl(0x64617461))
   {
      fseek(wav->file, 4, SEEK_CUR);
      ret = fread(&wav->header.subchunk2_id, 4, 1, wav->file);

      if (ret < 4)
      {
         goto error;
      }

      additional_data = true;
   }

   /* Update the value of subchunk2_size */
   if (additional_data)
   {
      ret = fread(&wav->header.subchunk2_size, 4, 1, wav->file);

      if (ret < 4)
      {
         goto error;
      }
   }

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

   wav->number_of_frames = wav->header.subchunk2_size / (wav->header.num_channels * wav->sample_format);
   wav->total_frames_read = 0;

   *w = wav;

   return 0;

error:

   return 1;
}

int
hrmp_wav_read(struct wav* wav, void* data, int len)
{
   switch (wav->sample_format)
   {
      case WAV_INT16:
      {
         int16_t* interleaved_data = (int16_t*)alloca(wav->header.num_channels * len * sizeof(int16_t));
         size_t samples_read = fread(interleaved_data, sizeof(int16_t), wav->header.num_channels * len, wav->file);
         int valid_len = (int) samples_read / wav->header.num_channels;
         switch (wav->channel_format)
         {
            case WAV_INTERLEAVED: /* [LRLRLRLR] */
            {
               for (int pos = 0; pos < wav->header.num_channels * valid_len; pos++)
               {
                  ((float*)data)[pos] = (float)interleaved_data[pos] / INT16_MAX;
               }
               return valid_len;
            }
            case WAV_INLINE: /* [LLLLRRRR] */
            {
               for (int i = 0, pos = 0; i < wav->header.num_channels; i++)
               {
                  for (int j = i; j < valid_len * wav->header.num_channels; j += wav->header.num_channels, ++pos)
                  {
                     ((float*)data)[pos] = (float)interleaved_data[j] / INT16_MAX;
                  }
               }
               return valid_len;
            }
            case WAV_SPLIT: /* [[LLLL],[RRRR]] */
            {
               for (int i = 0, pos = 0; i < wav->header.num_channels; i++)
               {
                  for (int j = 0; j < valid_len; j++, ++pos)
                  {
                     ((float**)data)[i][j] = (float)interleaved_data[j * wav->header.num_channels + i] / INT16_MAX;
                  }
               }
               return valid_len;
            }
            default:
               return 0;
         }
      }
      case WAV_FLOAT32:
      {
         float* interleaved_data = (float*) alloca(wav->header.num_channels * len * sizeof(float));
         size_t samples_read = fread(interleaved_data, sizeof(float), wav->header.num_channels * len, wav->file);
         int valid_len = (int) samples_read / wav->header.num_channels;
         switch (wav->channel_format)
         {
            case WAV_INTERLEAVED: /* [LRLRLRLR] */
            {
               memcpy(data, interleaved_data, wav->header.num_channels * valid_len * sizeof(float));
               return valid_len;
            }
            case WAV_INLINE: /* [LLLLRRRR] */
            {
               for (int i = 0, pos = 0; i < wav->header.num_channels; i++)
               {
                  for (int j = i; j < valid_len * wav->header.num_channels; j += wav->header.num_channels, ++pos)
                  {
                     ((float*) data)[pos] = interleaved_data[j];
                  }
               }
               return valid_len;
            }
            case WAV_SPLIT: /* [[LLLL],[RRRR]] */
            {
               for (int i = 0, pos = 0; i < wav->header.num_channels; i++)
               {
                  for (int j = 0; j < valid_len; j++, ++pos)
                  {
                     ((float**) data)[i][j] = interleaved_data[j * wav->header.num_channels + i];
                  }
               }
               return valid_len;
            }
            default:
               return 0;
         }
      }
      default:
         return 0;
   }

   return len;
}

void
hrmp_wav_close(struct wav* wav)
{
   if (wav->file != NULL)
   {
      fclose(wav->file);
   }

   wav->file = NULL;
}
