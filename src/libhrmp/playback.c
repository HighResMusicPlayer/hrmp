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
#include <alsa.h>
#include <devices.h>
#include <files.h>
#include <logging.h>
#include <playback.h>
#include <stdint.h>
#include <wav.h>

/* system */
#include <stdio.h>
#include <stdlib.h>
#include <FLAC/all.h>
//#include <FLAC/stream_decoder.h>
#include <alsa/asoundlib.h>

static FLAC__bool write_pcm(const FLAC__int32* const buffer[], size_t samples, struct playback* ctx);
static FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder* decoder, const FLAC__Frame* frame, const FLAC__int32* const buffer[], void* client_data);
static void metadata_callback(const FLAC__StreamDecoder* decoder, const FLAC__StreamMetadata* metadata, void* client_data);
static void error_callback(const FLAC__StreamDecoder* decoder, FLAC__StreamDecoderErrorStatus status, void* client_data);

int
hrmp_playback_wav(char* fn, int device, struct file_metadata* fm)
{
   /* int err; */
   /* snd_pcm_t* pcm = NULL; */
   /* /\* struct device* device = NULL; *\/ */
   /* struct wav* wav = NULL; */

   /* /\* device = hrmp_get_device(); *\/ */

   /* if ((err = snd_pcm_open(&pcm, device->device, SND_PCM_STREAM_PLAYBACK, 0))
    * < 0) */
   /* { */
   /*    hrmp_log_fatal("Can not play %s due to %s", path, snd_strerror(err)); */
   /*    goto error; */
   /* } */

   /* if (hrmp_wav_open(path, WAV_INTERLEAVED, &wav)) */
   /* { */
   /*    hrmp_log_fatal("Could not open %s", path); */
   /*    goto error; */
   /* } */

   /* if ((err = snd_pcm_close(pcm)) < 0) */
   /* { */
   /*    hrmp_log_debug("Error in closing PCM: %s", snd_strerror(err)); */
   /* } */

   return 0;

   /* error: */

   /*    if (pcm != NULL) */
   /*    { */
   /*       if ((err = snd_pcm_close(pcm)) < 0) */
   /*       { */
   /*          hrmp_log_debug("Error in closing PCM: %s", snd_strerror(err)); */
   /*       } */
   /*    } */

   /*    return 1; */
}

int
hrmp_playback_flac(char* fn, int device, struct file_metadata* fm)
{
   FLAC__StreamDecoder* decoder = NULL;
   FLAC__StreamDecoderInitStatus status;
   snd_pcm_t* pcm_handle = NULL;
   struct playback* pb = NULL;
   struct configuration* config = NULL;

   config = (struct configuration*)shmem;

   if (hrmp_alsa_init_handle(config->devices[device].device, fm->format, fm->sample_rate, &pcm_handle))
   {
      hrmp_log_error("Could not initialize '%s' for '%s'", config->devices[device].name, fn);
      goto error;
   }

   decoder = FLAC__stream_decoder_new();
   if (decoder == NULL)
   {
      hrmp_log_error("Could not initialize decoder for '%s'", fn);
      goto error;
   }
   FLAC__stream_decoder_set_md5_checking(decoder, false);

   pb = (struct playback*)malloc(sizeof(struct playback));
   memset(pb, 0, sizeof(struct playback));

   pb->pcm_handle = pcm_handle;
   pb->fm = fm;

   status = FLAC__stream_decoder_init_file(decoder, fn, write_callback, metadata_callback, error_callback, pb);
   if (status == FLAC__STREAM_DECODER_INIT_STATUS_OK)
   {
      hrmp_log_debug("OK: %s", fn);
   }
   else if (status == FLAC__STREAM_DECODER_INIT_STATUS_UNSUPPORTED_CONTAINER)
   {
      hrmp_log_error("UNSUPPORTED_CONTAINER: %s", fn);
      goto error;
   }
   else if (status == FLAC__STREAM_DECODER_INIT_STATUS_INVALID_CALLBACKS)
   {
      hrmp_log_error("INVALID_CALLBACKS: %s", fn);
      goto error;
   }
   else if (status == FLAC__STREAM_DECODER_INIT_STATUS_MEMORY_ALLOCATION_ERROR)
   {
      hrmp_log_error("MEMORY_ALLOCATION_ERROR: %s", fn);
      goto error;
   }
   else if (status == FLAC__STREAM_DECODER_INIT_STATUS_ERROR_OPENING_FILE)
   {
      hrmp_log_error("ERROR_OPENING_FILE: %s", fn);
      goto error;
   }
   else if (status == FLAC__STREAM_DECODER_INIT_STATUS_ALREADY_INITIALIZED)
   {
      hrmp_log_error("ALREADY_INITIALIZED: %s", fn);
      goto error;
   }
   else
   {
      hrmp_log_debug("Unknown %d for %s", status, fn);
   }

   // Metadata
   FLAC__stream_decoder_process_until_end_of_metadata(decoder);

   // Decode and play
   FLAC__stream_decoder_process_until_end_of_stream(decoder);

   hrmp_alsa_close_handle(pcm_handle);
   FLAC__stream_decoder_delete(decoder);

   free(pb);

   return 0;

error:

   hrmp_alsa_close_handle(pcm_handle);

   if (decoder != NULL)
   {
      FLAC__stream_decoder_delete(decoder);
   }

   free(pb);

   return 1;
}

static FLAC__bool
write_pcm(const FLAC__int32* const buffer[], size_t samples, struct playback* ctx)
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
            out[base + 3] = (sample >> 24) & 0xFF;
         }
      }
   }

   snd_pcm_writei(ctx->pcm_handle, out, samples);

   free(out);

   return true;
}

static FLAC__StreamDecoderWriteStatus
write_callback(const FLAC__StreamDecoder* decoder,
               const FLAC__Frame* frame,
               const FLAC__int32* const buffer[],
               void* client_data)
{
   struct playback* ctx = (struct playback*)client_data;
   write_pcm(buffer, frame->header.blocksize, ctx);
   return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void
metadata_callback(const FLAC__StreamDecoder* decoder,
                  const FLAC__StreamMetadata* metadata,
                  void* client_data)
{
}

static void
error_callback(const FLAC__StreamDecoder* decoder,
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
