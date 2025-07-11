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
#include <logging.h>

int
hrmp_alsa_init_handle(char* device, int format, int rate, snd_pcm_t** handle)
{
   int err;
   snd_pcm_t* h = NULL;
   snd_pcm_hw_params_t* hw_params = NULL;

   *handle = NULL;

   if ((err = snd_pcm_open(&h, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0)
   {
      hrmp_log_error("snd_pcm_open %s/%s", device, snd_strerror(err));
      goto error;
   }
   if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0)
   {
      hrmp_log_error("snd_pcm_hw_params_malloc %s/%s", device, snd_strerror(err));
      goto error;
   }
   if ((err = snd_pcm_hw_params_any(h, hw_params)) < 0)
   {
      hrmp_log_error("snd_pcm_hw_params_any %s/%s", device, snd_strerror(err));
      goto error;
   }
   if ((err = snd_pcm_hw_params_set_access(h, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
   {
      hrmp_log_error("snd_pcm_hw_params_set_access %s/%s", device, snd_strerror(err));
      goto error;
   }
   if ((err = snd_pcm_hw_params_set_format(h, hw_params, format)) < 0)
   {
      hrmp_log_error("snd_pcm_hw_params_set_format %s/%s", device, snd_strerror(err));
      goto error;
   }
   if ((err = snd_pcm_hw_params_set_channels(h, hw_params, 2)) < 0) // TODO
   {
      hrmp_log_error("snd_pcm_hw_params_set_channels %s/%s", device, snd_strerror(err));
      goto error;
   }
   if ((err = snd_pcm_hw_params_set_rate(h, hw_params, rate, 0)) < 0)
   {
      hrmp_log_error("snd_pcm_hw_params_set_rate %s/%s", device, snd_strerror(err));
      goto error;
   }
   if ((err = snd_pcm_hw_params(h, hw_params)) < 0)
   {
      hrmp_log_error("snd_pcm_hw_params %s/%s", device, snd_strerror(err));
      goto error;
   }

   snd_pcm_hw_params_free(hw_params);

   *handle = h;

   return 0;

error:

   if (hw_params != NULL)
   {
      snd_pcm_hw_params_free(hw_params);
   }

   if (h != NULL)
   {
      snd_pcm_drain(h);
      snd_pcm_close(h);
   }

   return 1;
}

int
hrmp_alsa_close_handle(snd_pcm_t* handle)
{
   if (handle == NULL)
   {
      return 1;
   }

   snd_pcm_drain(handle);
   snd_pcm_close(handle);

   return 0;
}

// TODO ... (implement FLAC decoding callbacks, buffer handling, etc.)
/*     // 3. While decoding FLAC, send PCM data to ALSA */
/*     // FLAC__stream_decoder_process_single(...) or similar */
/*     // Inside write callback: */
/*     // snd_pcm_writei(pcm_handle, pcm_buffer, frames); */
