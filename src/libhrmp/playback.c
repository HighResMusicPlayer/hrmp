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
#include <keyboard.h>
#include <logging.h>
#include <playback.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <utils.h>
#include <wav.h>

/* system */
#include <stdio.h>
#include <stdlib.h>
#include <FLAC/all.h>
#include <FLAC/stream_decoder.h>
#include <alsa/asoundlib.h>

/* static int wav_read(struct wav* wav, void* data, int length); */

static int flac_open(struct playback* playback);

static FLAC__StreamDecoderReadStatus flac_read_callback(const FLAC__StreamDecoder* decoder, FLAC__byte buffer[], size_t* bytes, void* client_data);
static FLAC__StreamDecoderSeekStatus flac_seek_callback(const FLAC__StreamDecoder* decoder, FLAC__uint64 absolute_byte_offset, void* client_data);
static FLAC__StreamDecoderTellStatus flac_tell_callback(const FLAC__StreamDecoder* decoder, FLAC__uint64* absolute_byte_offset, void* client_data);
static FLAC__StreamDecoderLengthStatus flac_length_callback(const FLAC__StreamDecoder* decoder, FLAC__uint64* stream_length, void* client_data);
static FLAC__bool flac_eof_callback(const FLAC__StreamDecoder* decoder, void* client_data);
static FLAC__StreamDecoderWriteStatus flac_write_callback(const FLAC__StreamDecoder* decoder, const FLAC__Frame* frame, const FLAC__int32*const buffer[], void* client_data);
static void flac_metadata_callback(const FLAC__StreamDecoder* decoder, const FLAC__StreamMetadata* metadata, void* client_data);
static void flac_error_callback(const FLAC__StreamDecoder* decoder, FLAC__StreamDecoderErrorStatus status, void* client_data);

static int playback_init(int device, int number, int total, snd_pcm_t* pcm_handle, struct file_metadata* fm, struct playback** playback);
static int playback_identifier(struct file_metadata* fm, char** identifer);

static void print_progress(struct playback* pb);
static void print_progress_done(struct playback* pb);

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

   if (playback_init(device, number, total, pcm_handle, fm, &pb))
   {
      hrmp_log_error("Could not initialize '%s' for '%s'", config->devices[device].name, fm->name);
      goto error;
   }

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

   fseek(pb->file, sizeof(struct wav_header), SEEK_SET);

   while (1)
   {
      int keyboard_action;
      int read = fread(wav->buffer, 1, wav->buffer_size, pb->file);

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

      keyboard_action = hrmp_keyboard_get();
      if (keyboard_action == KEYBOARD_Q)
      {
         printf("\n");
         hrmp_keyboard_mode(false);
         exit(0);
      }
      else if (keyboard_action == KEYBOARD_ENTER)
      {
         break;
      }
      else if (keyboard_action == KEYBOARD_UP || keyboard_action == KEYBOARD_DOWN ||
               keyboard_action == KEYBOARD_LEFT || keyboard_action == KEYBOARD_RIGHT)
      {
         int delta = pcm_period_size;
         int new_position = pb->current_samples;

         if (keyboard_action == KEYBOARD_UP)
         {
            delta = 30 * delta;
         }
         else if (keyboard_action == KEYBOARD_DOWN)
         {
            delta = -30 * delta;
         }
         else if (keyboard_action == KEYBOARD_LEFT)
         {
            delta = (int)(7.5 * delta);
         }
         else if (keyboard_action == KEYBOARD_DOWN)
         {
            delta = (int)(-7.5 * delta);
         }

         new_position += delta;

         if (new_position >= pb->fm->total_samples)
         {
            fseek(pb->file, 0, SEEK_END);
            pb->current_samples = pb->fm->total_samples;
         }
         else if (new_position <= 0)
         {
            fseek(pb->file, sizeof(struct wav_header), SEEK_SET);
            pb->current_samples = 0;
         }
         else
         {
            fseek(pb->file, delta, SEEK_CUR);
            pb->current_samples += delta;
         }

         print_progress(pb);
      }
   }

   print_progress_done(pb);

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

   if (playback_init(device, number, total, pcm_handle, fm, &pb))
   {
      hrmp_log_error("Could not initialize '%s' for '%s'", config->devices[device].name, fm->name);
      goto error;
   }

   status = FLAC__stream_decoder_init_stream(decoder,
                                             flac_read_callback,
                                             flac_seek_callback,
                                             flac_tell_callback,
                                             flac_length_callback,
                                             flac_eof_callback,
                                             flac_write_callback,
                                             flac_metadata_callback,
                                             flac_error_callback,
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

   print_progress_done(pb);

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
/*          size_t samples_read = fread(interleaved_data, sizeof(int16_t), wav->header.channels * length, pb->file); */
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
/*          size_t samples_read = fread(interleaved_data, sizeof(float), wav->header.channels * length, pb->file); */
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

static int
flac_open(struct playback* pb)
{
   FILE* file = NULL;

   file = pb->file;

   if (file == NULL)
   {
      file = fopen(pb->fm->name, "rb");

      if (file == NULL)
      {
         goto error;
      }

      fseek(file, 0, SEEK_SET);

      pb->file = file;
   }

   return 0;

error:

   return 1;
}

static FLAC__StreamDecoderReadStatus
flac_read_callback(const FLAC__StreamDecoder* decoder, FLAC__byte buffer[], size_t* bytes, void* client_data)
{
   struct playback* pb = NULL;

   pb = (struct playback*)client_data;

   flac_open(pb);

   if (*bytes > 0)
   {
      *bytes = fread(buffer, sizeof(FLAC__byte), *bytes, pb->file);
      if (ferror(pb->file))
      {
         return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
      }
      else if (*bytes == 0)
      {
         return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
      }
      else
      {
         return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
      }
   }
   else
   {
      return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
   }

   return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

static FLAC__StreamDecoderSeekStatus
flac_seek_callback(const FLAC__StreamDecoder* decoder, FLAC__uint64 absolute_byte_offset, void* client_data)
{
   struct playback* pb = NULL;

   pb = (struct playback*)client_data;

   if (fseek(pb->file, (long)absolute_byte_offset, SEEK_SET) < 0)
   {
      goto error;
   }

   return FLAC__STREAM_DECODER_SEEK_STATUS_OK;

error:

   return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
}

static FLAC__StreamDecoderTellStatus
flac_tell_callback(const FLAC__StreamDecoder* decoder, FLAC__uint64* absolute_byte_offset, void* client_data)
{
   long v = 0;
   struct playback* pb = NULL;

   pb = (struct playback*)client_data;

   v = ftell(pb->file);

   if (v < 0)
   {
      return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;
   }

   *absolute_byte_offset = (FLAC__uint64)v;

   return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

static FLAC__StreamDecoderLengthStatus
flac_length_callback(const FLAC__StreamDecoder* decoder, FLAC__uint64* stream_length, void* client_data)
{
   struct playback* pb = NULL;

   pb = (struct playback*)client_data;

   *stream_length = (FLAC__uint64)pb->file_size;

   return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

static FLAC__bool
flac_eof_callback(const FLAC__StreamDecoder* decoder, void* client_data)
{
   struct playback* pb = NULL;

   pb = (struct playback*)client_data;

   flac_open(pb);

   return feof(pb->file) ? true : false;
}

static FLAC__StreamDecoderWriteStatus
flac_write_callback(const FLAC__StreamDecoder* decoder,
                    const FLAC__Frame* frame, const FLAC__int32*const buffer[],
                    void* client_data)
{
   int keyboard_action = KEYBOARD_IGNORE;
   int byte_per_frame = 0;
   int frame_size = 0;
   size_t bs = 0;
   snd_pcm_uframes_t samples = frame->header.blocksize;
   unsigned char* out = NULL;
   struct playback* pb = NULL;

   pb = (struct playback*)client_data;

   byte_per_frame = pb->fm->format == SND_PCM_FORMAT_S16_LE ? 2 : 4;
   frame_size = pb->fm->channels * byte_per_frame;
   bs = samples * frame_size;

   out = (unsigned char*)malloc(bs);
   if (out == NULL)
   {
      return false;
   }

   memset(out, 0, bs);

   for (size_t i = 0; i < samples; i++)
   {
      int root = (i * frame_size);
      for (size_t ch = 0; ch < pb->fm->channels; ch++)
      {
         int32_t sample = buffer[ch][i];
         int base = root + (ch * byte_per_frame);

         out[base] = sample & 0xFF;
         out[base + 1] = (sample >> 8) & 0xFF;

         if (byte_per_frame == 4)
         {
            out[base + 2] = (sample >> 16) & 0xFF;
            if (pb->fm->bits_per_sample == 32)
            {
               out[base + 3] = (sample >> 24) & 0xFF;
            }
         }
      }
   }

   print_progress(pb);

   /* pcm_handle has the format and channels which are multiplied with samples  */
   snd_pcm_writei(pb->pcm_handle, (const void*)out, samples);

   pb->current_samples += samples;

   keyboard_action = hrmp_keyboard_get();
   if (keyboard_action == KEYBOARD_Q)
   {
      printf("\n");
      hrmp_keyboard_mode(false);
      free(out);
      exit(0);
   }
   else if (keyboard_action == KEYBOARD_ENTER)
   {
      goto quit;
   }
   else if (keyboard_action == KEYBOARD_UP || keyboard_action == KEYBOARD_DOWN ||
            keyboard_action == KEYBOARD_LEFT || keyboard_action == KEYBOARD_RIGHT)
   {
      long delta = pb->fm->sample_rate;
      long new_position = pb->current_samples;

      if (keyboard_action == KEYBOARD_UP)
      {
         delta = 60 * delta;
      }
      else if (keyboard_action == KEYBOARD_DOWN)
      {
         delta = -60 * delta;
      }
      else if (keyboard_action == KEYBOARD_LEFT)
      {
         delta = -15 * delta;
      }
      else if (keyboard_action == KEYBOARD_RIGHT)
      {
         delta = 15 * delta;
      }

      FLAC__stream_decoder_flush((FLAC__StreamDecoder*)decoder);

      new_position += delta;

      if (new_position <= 0)
      {
         pb->current_samples = 0;
         FLAC__stream_decoder_reset((FLAC__StreamDecoder*)decoder);
         FLAC__stream_decoder_set_md5_checking((FLAC__StreamDecoder*)decoder, false);
      }
      else if ((unsigned long)new_position >= pb->fm->total_samples)
      {
         pb->current_samples = pb->fm->total_samples;
         goto quit;
      }
      else
      {
         pb->current_samples += delta;
         FLAC__stream_decoder_seek_absolute((FLAC__StreamDecoder*)decoder, pb->current_samples);
      }

      print_progress(pb);
   }

   free(out);

   return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;

quit:

   free(out);

   return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
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
      /* Forward / rewind */
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
playback_init(int device, int number, int total, snd_pcm_t* pcm_handle, struct file_metadata* fm, struct playback** playback)
{
   char* desc = NULL;
   struct playback* pb = NULL;

   *playback = NULL;

   pb = (struct playback*)malloc(sizeof(struct playback));

   if (pb == NULL)
   {
      goto error;
   }

   memset(pb, 0, sizeof(struct playback));

   playback_identifier(fm, &desc);

   pb->device = device;
   pb->file = NULL;
   pb->file_size = hrmp_get_file_size(fm->name);
   pb->file_number = number;
   pb->total_number = total;
   memcpy(&pb->identifier, desc, strlen(desc));
   pb->current_samples = 0;
   pb->pcm_handle = pcm_handle;
   pb->fm = fm;

   *playback = pb;

   free(desc);

   return 0;

error:

   free(desc);
   free(pb);

   return 1;
}

static int
playback_identifier(struct file_metadata* fm, char** identifer)
{
   char* id = NULL;

   *identifer = NULL;

   id = hrmp_append_char(id, '[');

   if (fm->type == TYPE_WAV)
   {
      id = hrmp_append(id, "WAV/");
   }
   else if (fm->type == TYPE_FLAC)
   {
      id = hrmp_append(id, "FLAC/");
   }

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
      char t[MAX_PATH];
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

      printf("\r[%d/%d] %s: %s %s (%s) (%d%%)", pb->file_number, pb->total_number, config->devices[pb->device].name,
             pb->fm->name, pb->identifier, &t[0], percent);

      fflush(stdout);
   }
}

static void
print_progress_done(struct playback* pb)
{
   struct configuration* config = NULL;

   config = (struct configuration*)shmem;

   if (!config->quiet)
   {
      char t[MAX_PATH];
      int total_min = 0;
      int total_sec = 0;

      memset(&t[0], 0, sizeof(t));

      total_min = (int)(pb->fm->duration) / 60;
      total_sec = pb->fm->duration - (total_min * 60);

      snprintf(&t[0], sizeof(t), "%d:%02d/%d:%02d", total_min, total_sec,
               total_min, total_sec);

      printf("\r[%d/%d] %s: %s %s (%s) (100%%)\n", pb->file_number, pb->total_number,
             config->devices[pb->device].name, pb->fm->name, pb->identifier,
             &t[0]);

      fflush(stdout);
   }
}
