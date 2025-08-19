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
#include <alsa.h>
#include <devices.h>
#include <files.h>
#include <logging.h>
#include <playback.h>
#include <stdint.h>
#include <string.h>
#include <utils.h>
#include <wav.h>

/* system */
#include <stdio.h>
#include <stdlib.h>
#include <FLAC/all.h>
#include <alsa/asoundlib.h>

/* static int wav_read(struct wav* wav, void* data, int length); */

static FLAC__bool flac_write_pcm(const FLAC__int32*const buffer[], size_t samples, struct playback* ctx);
static FLAC__StreamDecoderWriteStatus flac_write_callback(const FLAC__StreamDecoder* decoder, const FLAC__Frame* frame, const FLAC__int32* const buffer[], void* client_data);
static void flac_metadata_callback(const FLAC__StreamDecoder* decoder, const FLAC__StreamMetadata* metadata, void* client_data);
static void flac_error_callback(const FLAC__StreamDecoder* decoder, FLAC__StreamDecoderErrorStatus status, void* client_data);

static int playback_identifier(struct file_metadata* fm, char** identifer);

static void print_progress(struct playback* pb);

int
hrmp_playback_wav(int device, int number, int total, struct file_metadata* fm)
{
   char* desc = NULL;
   snd_pcm_t* pcm_handle = NULL;
   snd_pcm_uframes_t pcm_buffer_size = 0;
   snd_pcm_uframes_t pcm_period_size = 0;
   struct wav* wav = NULL;
   struct playback* pb = NULL;
   struct configuration* config = NULL;

   config = (struct configuration*)shmem;

   if (hrmp_alsa_init_handle(config->devices[device].device, fm->format, fm->sample_rate, &pcm_handle))
   {
      hrmp_log_error("Could not initialize '%s' for '%s'", config->devices[device].name, fm->name);
      goto error;
   }

   pb = (struct playback*)malloc(sizeof(struct playback));
   memset(pb, 0, sizeof(struct playback));

   playback_identifier(fm, &desc);

   pb->device = device;
   pb->file_number = number;
   pb->total_number = total;
   memcpy(&pb->identifier, desc, strlen(desc));
   pb->current_samples = 0;
   pb->pcm_handle = pcm_handle;
   pb->fm = fm;

   if (hrmp_wav_open(fm->name, WAV_INTERLEAVED, &wav))
   {
      printf("Could not open '%s'\n", fm->name);
      goto error;
   }

   if (snd_pcm_get_params(pcm_handle, &pcm_buffer_size, &pcm_period_size) < 0)
   {
      printf("Could not get parameters for '%s'\n", fm->name);
      goto error;
   }

   wav->buffer_size = (size_t)(pcm_period_size * wav->header.channels * (wav->header.bits_per_sample / 8));
   wav->buffer = malloc(wav->buffer_size);

   fseek(wav->file, sizeof(struct wav_header), SEEK_SET);

   while (1)
   {
      int read = fread(wav->buffer, 1, wav->buffer_size, wav->file);

      if (read == 0)
      {
         break;
      }

      if (read < wav->buffer_size)
      {
         memset(wav->buffer + read, 0, wav->buffer_size - read);
      }

      if (snd_pcm_writei(pcm_handle, wav->buffer, pcm_period_size) < 0)
      {
         snd_pcm_prepare(pcm_handle);
      }

      print_progress(pb);
      pb->current_samples += pcm_period_size;
   }

   free(desc);

   hrmp_wav_close(wav);

   hrmp_alsa_close_handle(pcm_handle);

   free(pb);

   return 0;

error:

   free(desc);

   hrmp_wav_close(wav);

   hrmp_alsa_close_handle(pcm_handle);

   free(pb);

   return 1;
}

int
hrmp_playback_flac(int device, int number, int total, struct file_metadata* fm)
{
   char* desc = NULL;
   FLAC__StreamDecoder* decoder = NULL;
   FLAC__StreamDecoderInitStatus status;
   snd_pcm_t* pcm_handle = NULL;
   struct playback* pb = NULL;
   struct configuration* config = NULL;

   config = (struct configuration*)shmem;

   if (hrmp_alsa_init_handle(config->devices[device].device, fm->format, fm->sample_rate, &pcm_handle))
   {
      hrmp_log_error("Could not initialize '%s' for '%s'", config->devices[device].name, fm->name);
      goto error;
   }

   decoder = FLAC__stream_decoder_new();
   if (decoder == NULL)
   {
      hrmp_log_error("Could not initialize decoder for '%s'", fm->name);
      goto error;
   }
   FLAC__stream_decoder_set_md5_checking(decoder, false);

   pb = (struct playback*)malloc(sizeof(struct playback));
   memset(pb, 0, sizeof(struct playback));

   playback_identifier(fm, &desc);

   pb->device = device;
   pb->file_number = number;
   pb->total_number = total;
   memcpy(&pb->identifier, desc, strlen(desc));
   pb->current_samples = 0;
   pb->pcm_handle = pcm_handle;
   pb->fm = fm;

   status = FLAC__stream_decoder_init_file(decoder, fm->name,
                                           flac_write_callback, flac_metadata_callback, flac_error_callback,
                                           pb);
   if (status == FLAC__STREAM_DECODER_INIT_STATUS_OK)
   {
      fflush(stdout);
   }
   else if (status == FLAC__STREAM_DECODER_INIT_STATUS_UNSUPPORTED_CONTAINER)
   {
      hrmp_log_error("UNSUPPORTED_CONTAINER: %s", fm->name);
      goto error;
   }
   else if (status == FLAC__STREAM_DECODER_INIT_STATUS_INVALID_CALLBACKS)
   {
      hrmp_log_error("INVALID_CALLBACKS: %s", fm->name);
      goto error;
   }
   else if (status == FLAC__STREAM_DECODER_INIT_STATUS_MEMORY_ALLOCATION_ERROR)
   {
      hrmp_log_error("MEMORY_ALLOCATION_ERROR: %s", fm->name);
      goto error;
   }
   else if (status == FLAC__STREAM_DECODER_INIT_STATUS_ERROR_OPENING_FILE)
   {
      hrmp_log_error("ERROR_OPENING_FILE: %s", fm->name);
      goto error;
   }
   else if (status == FLAC__STREAM_DECODER_INIT_STATUS_ALREADY_INITIALIZED)
   {
      hrmp_log_error("ALREADY_INITIALIZED: %s", fm->name);
      goto error;
   }
   else
   {
      hrmp_log_debug("Unknown %d for %s", status, fm->name);
   }

   /* Metadata */
   FLAC__stream_decoder_process_until_end_of_metadata(decoder);

   /* Decode and play */
   FLAC__stream_decoder_process_until_end_of_stream(decoder);

   if (!config->quiet)
   {
      printf("\n");
   }

   hrmp_alsa_close_handle(pcm_handle);
   FLAC__stream_decoder_delete(decoder);

   free(pb);
   free(desc);

   return 0;

error:

   hrmp_alsa_close_handle(pcm_handle);

   if (decoder != NULL)
   {
      FLAC__stream_decoder_delete(decoder);
   }

   free(pb);
   free(desc);

   return 1;
}

/* static int */
/* wav_read(struct wav* wav, void* data, int length) */
/* { */
/*    switch (wav->sample_format) */
/*    { */
/*       case WAV_INT16: */
/*       { */
/*          int16_t* interleaved_data = (int16_t*)alloca(wav->header.channels * length * sizeof(int16_t)); */
/*          size_t samples_read = fread(interleaved_data, sizeof(int16_t), wav->header.channels * length, wav->file); */
/*          int valid_length = (int) samples_read / wav->header.channels; */
/*          switch (wav->channel_format) */
/*          { */
/*             case WAV_INTERLEAVED: /\* [LRLRLRLR] *\/ */
/*             { */
/*                for (int pos = 0; pos < wav->header.channels * valid_length; pos++) */
/*                { */
/*                   ((float*)data)[pos] = (float)interleaved_data[pos] / INT16_MAX; */
/*                } */
/*                return valid_length; */
/*             } */
/*             case WAV_INLINE: /\* [LLLLRRRR] *\/ */
/*             { */
/*                for (int i = 0, pos = 0; i < wav->header.channels; i++) */
/*                { */
/*                   for (int j = i; j < valid_length * wav->header.channels; j += wav->header.channels, ++pos) */
/*                   { */
/*                      ((float*)data)[pos] = (float)interleaved_data[j] / INT16_MAX; */
/*                   } */
/*                } */
/*                return valid_length; */
/*             } */
/*             case WAV_SPLIT: /\* [[LLLL],[RRRR]] *\/ */
/*             { */
/*                for (int i = 0, pos = 0; i < wav->header.channels; i++) */
/*                { */
/*                   for (int j = 0; j < valid_length; j++, ++pos) */
/*                   { */
/*                      ((float**)data)[i][j] = (float)interleaved_data[j * wav->header.channels + i] / INT16_MAX; */
/*                   } */
/*                } */
/*                return valid_length; */
/*             } */
/*             default: */
/*                return 0; */
/*          } */
/*       } */
/*       case WAV_FLOAT32: */
/*       { */
/*          float* interleaved_data = (float*) alloca(wav->header.channels * length * sizeof(float)); */
/*          size_t samples_read = fread(interleaved_data, sizeof(float), wav->header.channels * length, wav->file); */
/*          int valid_length = (int) samples_read / wav->header.channels; */
/*          switch (wav->channel_format) */
/*          { */
/*             case WAV_INTERLEAVED: /\* [LRLRLRLR] *\/ */
/*             { */
/*                memcpy(data, interleaved_data, wav->header.channels * valid_length * sizeof(float)); */
/*                return valid_length; */
/*             } */
/*             case WAV_INLINE: /\* [LLLLRRRR] *\/ */
/*             { */
/*                for (int i = 0, pos = 0; i < wav->header.channels; i++) */
/*                { */
/*                   for (int j = i; j < valid_length * wav->header.channels; j += wav->header.channels, ++pos) */
/*                   { */
/*                      ((float*) data)[pos] = interleaved_data[j]; */
/*                   } */
/*                } */
/*                return valid_length; */
/*             } */
/*             case WAV_SPLIT: /\* [[LLLL],[RRRR]] *\/ */
/*             { */
/*                for (int i = 0, pos = 0; i < wav->header.channels; i++) */
/*                { */
/*                   for (int j = 0; j < valid_length; j++, ++pos) */
/*                   { */
/*                      ((float**) data)[i][j] = interleaved_data[j * wav->header.channels + i]; */
/*                   } */
/*                } */
/*                return valid_length; */
/*             } */
/*             default: */
/*                return 0; */
/*          } */
/*       } */
/*       default: */
/*          return 0; */
/*    } */

/*    return length; */
/* } */

static FLAC__bool
flac_write_pcm(const FLAC__int32* const buffer[], size_t samples, struct playback* ctx)
{
   int byte_per_frame = ctx->fm->format == SND_PCM_FORMAT_S16_LE ? 2 : 4;
   int frame_size = ctx->fm->channels * byte_per_frame;
   size_t bs = samples * frame_size;
   unsigned char* out = (unsigned char*)malloc(bs);

   if (out == NULL)
   {
      return false;
   }

   memset(out, 0, bs);

   for (size_t i = 0; i < samples; i++)
   {
      int root = (i * frame_size);
      for (size_t ch = 0; ch < ctx->fm->channels; ch++)
      {
         int32_t sample = buffer[ch][i];
         int base = root + (ch * byte_per_frame);

         out[base] = sample & 0xFF;
         out[base + 1] = (sample >> 8) & 0xFF;

         if (byte_per_frame == 4)
         {
            out[base + 2] = (sample >> 16) & 0xFF;
            if (ctx->fm->bits_per_sample == 32)
            {
               out[base + 3] = (sample >> 24) & 0xFF;
            }
         }
      }
   }

   print_progress(ctx);

   /* pcm_handle has the format and channels which are multiplied with samples  */
   snd_pcm_writei(ctx->pcm_handle, (const void*)out, (snd_pcm_uframes_t)samples);

   ctx->current_samples += samples;

   free(out);

   return true;
}

static FLAC__StreamDecoderWriteStatus
flac_write_callback(const FLAC__StreamDecoder* decoder,
                    const FLAC__Frame* frame,
                    const FLAC__int32* const buffer[],
                    void* client_data)
{
   struct playback* ctx = (struct playback*)client_data;
   flac_write_pcm(buffer, frame->header.blocksize, ctx);
   return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void
flac_metadata_callback(const FLAC__StreamDecoder* decoder,
                       const FLAC__StreamMetadata* metadata,
                       void* client_data)
{
}

static void
flac_error_callback(const FLAC__StreamDecoder* decoder,
                    FLAC__StreamDecoderErrorStatus status,
                    void* client_data)
{
   if (status == FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC)
   {
      hrmp_log_error("LOST_SYNC");
   }
   else if (status == FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER)
   {
      hrmp_log_error("BAD_HEADER");
   }
   else if (status == FLAC__STREAM_DECODER_ERROR_STATUS_FRAME_CRC_MISMATCH)
   {
      hrmp_log_error("FRAME_CRC_MISMATCH");
   }
   else if (status == FLAC__STREAM_DECODER_ERROR_STATUS_BAD_METADATA)
   {
      hrmp_log_error("BAD_METADATA");
   }
   /* else if (status == FLAC__STREAM_DECODER_ERROR_STATUS_OUT_OF_BOUNDS) */
   /* { */
   /* } */
   /* else if (status == FLAC__STREAM_DECODER_ERROR_STATUS_MISSING_FRAME) */
   /* { */
   /* } */
   else
   {
      hrmp_log_error("UNKNOWN %d", status);
   }
}

static int
playback_identifier(struct file_metadata* fm, char** identifer)
{
   char* id = NULL;

   *identifer = NULL;

   id = hrmp_append_char(id, '[');

   switch (fm->sample_rate)
   {
      case 44100:
         id = hrmp_append(id, "44.1kHz");
         break;
      case 48000:
         id = hrmp_append(id, "48kHz");
         break;
      case 88200:
         id = hrmp_append(id, "88.2kHz");
         break;
      case 96000:
         id = hrmp_append(id, "96kHz");
         break;
      case 176400:
         id = hrmp_append(id, "176.4kHz");
         break;
      case 192000:
         id = hrmp_append(id, "192kHz");
         break;
      case 352800:
         id = hrmp_append(id, "352.8kHz");
         break;
      case 384000:
         id = hrmp_append(id, "384kHz");
         break;
      case 705600:
         id = hrmp_append(id, "705.6kHz");
         break;
      case 768000:
         id = hrmp_append(id, "768kHz");
         break;
      default:
         id = hrmp_append_int(id, (int)fm->sample_rate);
         id = hrmp_append(id, "Hz");
         hrmp_log_error("Unsupported sample rate: %dkHz/%dbits", fm->sample_rate, fm->bits_per_sample);
         break;
   }

   id = hrmp_append(id, "/");

   switch (fm->bits_per_sample)
   {
      case 16:
         id = hrmp_append(id, "16bits");
         break;
      case 24:
         id = hrmp_append(id, "24bits");
         break;
      case 32:
         id = hrmp_append(id, "32bits");
         break;
      default:
         id = hrmp_append_int(id, (int)fm->bits_per_sample);
         id = hrmp_append(id, "bits");
         hrmp_log_error("Unsupported bits per sample: %dkHz/%dbits", fm->sample_rate, fm->bits_per_sample);
         break;
   }

   id = hrmp_append_char(id, ']');

   *identifer = id;

   return 0;
}

static void
print_progress(struct playback* pb)
{
   struct configuration* config = NULL;

   config = (struct configuration*)shmem;

   if (!config->quiet)
   {
      char t[MISC_LENGTH];
      double current = 0.0;
      int current_min = 0;
      int current_sec = 0;
      int total_min = 0;
      int total_sec = 0;
      int percent = 0;

      memset(&t[0], 0, sizeof(t));

      current = (int)((double)(pb->current_samples) / pb->fm->sample_rate);

      current_min = (int)(current) / 60;
      current_sec = current - (current_min * 60);

      total_min = (int)(pb->fm->duration) / 60;
      total_sec = pb->fm->duration - (total_min * 60);

      percent = (int)(current * 100 / pb->fm->duration);

      snprintf(&t[0], sizeof(t), "%d:%02d/%d:%02d", current_min, current_sec,
               total_min, total_sec);

      printf("\r[%d/%d] %s: %s %s (%s) (%d%%)", pb->file_number, pb->total_number,
             config->devices[pb->device].name, pb->fm->name, pb->identifier,
             &t[0], percent);

      fflush(stdout);
   }
}
