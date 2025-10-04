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
#include <utils.h>

/* system */
#include <math.h>
#include <sndfile.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <alsa/asoundlib.h>

static int playback_init(int device, int number, int total, snd_pcm_t* pcm_handle, struct file_metadata* fm, struct playback** playback);
static int playback_identifier(struct file_metadata* fm, char** identifer);

static int set_volume(int device, int volume);

static void print_progress(struct playback* pb);
static void print_progress_done(struct playback* pb);

static int playback_sndfile(snd_pcm_t* pcm_handle,
                            struct playback* pb, int device, int number,
                            int total, struct file_metadata* fm);
static int playback_dsf(snd_pcm_t* pcm_handle,
                        struct playback* pb, int device, int number, int total,
                        struct file_metadata* fm);

int
hrmp_playback(int device, int number, int total, struct file_metadata* fm)
{
   int ret = 1;
   snd_pcm_t* pcm_handle = NULL;
   struct playback* pb = NULL;
   struct configuration* config = NULL;

   config = (struct configuration*)shmem;

   if (hrmp_alsa_init_handle(config->devices[device].device, fm, &pcm_handle))
   {
      hrmp_log_error("Could not initialize '%s' for '%s'", config->devices[device].name, fm->name);
      goto error;
   }

   config->devices[device].is_paused = false;

   if (playback_init(device, number, total, pcm_handle, fm, &pb))
   {
      hrmp_log_error("Could not initialize '%s' for '%s'", config->devices[device].name, fm->name);
      goto error;
   }

   if (fm->type == TYPE_WAV || fm->type == TYPE_FLAC || fm->type == TYPE_MP3)
   {
      ret = playback_sndfile(pcm_handle, pb, device, number, total, fm);
   }
   else if (fm->type == TYPE_DSF)
   {
      ret = playback_dsf(pcm_handle, pb, device, number, total, fm);
   }
   else
   {
      goto error;
   }

   hrmp_alsa_close_handle(pcm_handle);
   free(pb);

   return ret;

error:

   if (pcm_handle != NULL)
   {
      hrmp_alsa_close_handle(pcm_handle);
   }
   free(pb);

   return 1;
}

static int
playback_sndfile(snd_pcm_t* pcm_handle,
                 struct playback* pb, int device, int number,
                 int total, struct file_metadata* fm)
{
   int err;
   int keyboard_action;
   SNDFILE* f = NULL;
   SF_INFO* info = NULL;
   int bytes_per_sample = 2;
   size_t bytes_per_frame;
   snd_pcm_uframes_t pcm_buffer_size = 0;
   snd_pcm_uframes_t pcm_period_size = 0;
   snd_pcm_sframes_t frames_to_write;
   snd_pcm_sframes_t w;
   sf_count_t frames_read;
   size_t input_buffer_size = 0;
   int32_t* input_buffer = NULL;
   size_t output_buffer_size = 0;
   unsigned char* output_buffer = NULL;
   struct configuration* config = NULL;

   config = (struct configuration*)shmem;

   info = (SF_INFO*)malloc(sizeof(SF_INFO));
   if (info == NULL)
   {
      goto error;
   }
   memset(info, 0, sizeof(SF_INFO));

   f = sf_open(fm->name, SFM_READ, info);
   if (f == NULL)
   {
      goto error;
   }

   if (snd_pcm_get_params(pcm_handle, &pcm_buffer_size, &pcm_period_size) < 0)
   {
      printf("Could not get parameters for '%s'\n", fm->name);
      goto error;
   }

   // Playback loop
   switch (fm->container)
   {
      case 16:
         bytes_per_sample = 2;
         break;
      case 24:
         bytes_per_sample = 3;
         break;
      case 32:
         bytes_per_sample = 4;
         break;
      default:
         bytes_per_sample = 4;
         break;
   }
   bytes_per_frame = (size_t)(bytes_per_sample * info->channels);

   /* We'll read libsndfile data into a 32-bit int buffer (interleaved) */
   input_buffer_size = sizeof(int32_t) * pcm_period_size * info->channels;
   input_buffer = (int32_t*)malloc(input_buffer_size);
   if (input_buffer == NULL)
   {
      goto error;
   }

   output_buffer_size = bytes_per_frame * pcm_period_size;
   output_buffer = (unsigned char*)malloc(output_buffer_size);
   if (output_buffer == NULL)
   {
      goto error;
   }

   memset(input_buffer, 0, input_buffer_size);
   memset(output_buffer, 0, output_buffer_size);

   /* Playback loop */
   while ((frames_read = sf_readf_int(f, input_buffer, pcm_period_size)) > 0)
   {
      /* Pack samples into output_buffer according to chosen_format */
      size_t outpos = 0;
      for (sf_count_t f = 0; f < frames_read; ++f)
      {
         for (int ch = 0; ch < info->channels; ++ch)
         {
            int32_t sample = input_buffer[f * info->channels + ch];
            /* libsndfile gives signed int32_t; if file is 24-bit it returns it in low 24 bits. */
            if (fm->container == 16)
            {
               /* convert 32-bit to 16-bit by shifting (arith shift) */
               int16_t s16 = (int16_t)(sample >> 16); /* lose lower bits */
               output_buffer[outpos++] = (uint8_t)(s16 & 0xFF);
               output_buffer[outpos++] = (uint8_t)((s16 >> 8) & 0xFF);
            }
            else if (fm->container == 24)
            {
               /* pack lower 3 bytes little-endian */
               output_buffer[outpos++] = (uint8_t)(sample & 0xFF);
               output_buffer[outpos++] = (uint8_t)((sample >> 8) & 0xFF);
               output_buffer[outpos++] = (uint8_t)((sample >> 16) & 0xFF);
            }
            else
            {
               /* 4 bytes little-endian */
               output_buffer[outpos++] = (uint8_t)(sample & 0xFF);
               output_buffer[outpos++] = (uint8_t)((sample >> 8) & 0xFF);
               output_buffer[outpos++] = (uint8_t)((sample >> 16) & 0xFF);
               output_buffer[outpos++] = (uint8_t)((sample >> 24) & 0xFF);
            }
         }
      }

      /* writei expects frames, not bytes; compute frames to write */
      frames_to_write = frames_read;
      w = snd_pcm_writei(pcm_handle, output_buffer, frames_to_write);

      if (w == -EPIPE)
      {
         /* underrun */
         snd_pcm_prepare(pcm_handle);
         w = snd_pcm_writei(pcm_handle, output_buffer, frames_to_write);
      }

      if (w < 0)
      {
         /* attempt recovery and continue */
         if ((err = snd_pcm_recover(pcm_handle, (int)w, 0)) < 0)
         {
            break;
         }
      }

      print_progress(pb);
      pb->current_samples += pcm_period_size;

keyboard:
      keyboard_action = hrmp_keyboard_get();

      if (keyboard_action == KEYBOARD_Q)
      {
         print_progress_done(pb);
         hrmp_keyboard_mode(false);
         exit(0);
      }
      else if (keyboard_action == KEYBOARD_ENTER)
      {
         break;
      }
      else if (keyboard_action == KEYBOARD_SPACE)
      {
         if (config->devices[device].is_paused)
         {
            config->devices[device].is_paused = false;
         }
         else
         {
            config->devices[device].is_paused = true;
            SLEEP_AND_GOTO(10000L, keyboard);
         }
      }
      else if (keyboard_action == KEYBOARD_UP ||
               keyboard_action == KEYBOARD_DOWN ||
               keyboard_action == KEYBOARD_LEFT ||
               keyboard_action == KEYBOARD_RIGHT)
      {
         size_t delta = (size_t)(fm->total_samples / fm->duration);
         size_t new_position = (size_t)pb->current_samples;

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

         new_position += delta;

         if (new_position >= pb->fm->total_samples)
         {
            sf_seek(f, 0, SEEK_END);
            pb->current_samples = pb->fm->total_samples;
         }
         else if (new_position <= 0)
         {
            sf_seek(f, 0, SEEK_SET);
            pb->current_samples = 0;
         }
         else
         {
            sf_seek(f, delta, SEEK_CUR);
            pb->current_samples += delta;
         }

         print_progress(pb);
      }
      else if (keyboard_action == KEYBOARD_COMMA)
      {
         int new_volume = config->volume - 5;

         if (!config->is_muted)
         {
            set_volume(device, new_volume);
         }
      }
      else if (keyboard_action == KEYBOARD_PERIOD)
      {
         int new_volume = config->volume + 5;

         if (!config->is_muted)
         {
            set_volume(device, new_volume);
         }
      }
      else if (keyboard_action == KEYBOARD_M)
      {
         int new_volume;

         if (config->is_muted)
         {
            new_volume = config->prev_volume;
            config->is_muted = false;
         }
         else
         {
            new_volume = 0;
            config->is_muted = true;
         }

         set_volume(device, new_volume);
      }
      else if (keyboard_action == KEYBOARD_SLASH)
      {
         int new_volume = 100;

         config->is_muted = false;
         set_volume(device, new_volume);
      }
      else
      {
         if (config->devices[device].is_paused)
         {
            SLEEP_AND_GOTO(10000L, keyboard);
         }
      }

      memset(input_buffer, 0, input_buffer_size);
      memset(output_buffer, 0, output_buffer_size);
   }

   print_progress_done(pb);

   free(input_buffer);
   free(output_buffer);
   free(info);

   sf_close(f);

   return 0;

error:

   free(input_buffer);
   free(output_buffer);
   free(info);

   sf_close(f);

   return 1;
}

static int
playback_dsf(snd_pcm_t* pcm_handle,
             struct playback* pb, int device, int number, int total,
             struct file_metadata* fm)
{
   return 1;
}

static int
playback_init(int device, int number, int total,
              snd_pcm_t* pcm_handle, struct file_metadata* fm,
              struct playback** playback)
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

#ifdef DEBUG
   hrmp_print_file_metadata(fm);
#endif

   id = hrmp_append_char(id, '[');

   if (fm->type == TYPE_WAV)
   {
      id = hrmp_append(id, "WAV/");
   }
   else if (fm->type == TYPE_FLAC)
   {
      id = hrmp_append(id, "FLAC/");
   }
   else if (fm->type == TYPE_MP3)
   {
      id = hrmp_append(id, "MP3/");
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
      case 2822400:
         id = hrmp_append(id, "2.8224MHz");
         break;
      case 5644800:
         id = hrmp_append(id, "5.6448MHz");
         break;
      case 11289600:
         id = hrmp_append(id, "11.2896MHz");
         break;
      default:
         id = hrmp_append_int(id, (int)fm->sample_rate);
         id = hrmp_append(id, "Hz");
         printf("Unsupported sample rate: %dHz/%dbits\n", fm->sample_rate, fm->bits_per_sample);
         break;
   }

   id = hrmp_append(id, "/");

   switch (fm->bits_per_sample)
   {
      case 1:
         id = hrmp_append(id, "1bit");
         break;
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
         hrmp_log_error("Unsupported bits per sample: %dHz/%dbits", fm->sample_rate, fm->bits_per_sample);
         break;
   }

   id = hrmp_append_char(id, ']');

   *identifer = id;

   return 0;
}

static int
set_volume(int device, int volume)
{
   int err = 0;
   snd_mixer_t* handle = NULL;
   snd_mixer_selem_id_t* sid = NULL;
   snd_mixer_elem_t* elem = NULL;
   long minv = 0;
   long maxv = 0;
   long vol = 0;
   char address[MISC_LENGTH];
   struct configuration* config = NULL;

   config = (struct configuration*)shmem;

   config->prev_volume = config->volume;

   if (volume < 0)
   {
      volume = 0;
   }
   else if (volume > 100)
   {
      volume = 100;
   }

   if ((err = snd_mixer_open(&handle, 0)) < 0)
   {
      hrmp_log_error("Error: snd_mixer_open: %s", snd_strerror(err));
      goto error;
   }

   memset(&address[0], 0, sizeof(address));
   snprintf(&address[0], sizeof(address), "hw:%d", config->devices[device].hardware);

   if ((err = snd_mixer_attach(handle, &address[0])) < 0)
   {
      hrmp_log_error("Error: snd_mixer_attach(%s): %s", config->devices[device].name, snd_strerror(err));
      goto error;
   }

   if ((err = snd_mixer_selem_register(handle, NULL, NULL)) < 0)
   {
      hrmp_log_error("Error: snd_mixer_selem_register: %s", snd_strerror(err));
      goto error;
   }

   if ((err = snd_mixer_load(handle)) < 0)
   {
      hrmp_log_error("Error: snd_mixer_load: %s", snd_strerror(err));
      goto error;
   }

   snd_mixer_selem_id_malloc(&sid);
   if (!sid)
   {
      hrmp_log_error("Error: failed to allocate selem id");
      goto error;
   }

   snd_mixer_selem_id_set_index(sid, 0);
   snd_mixer_selem_id_set_name(sid, config->devices[device].selem);

   elem = snd_mixer_find_selem(handle, sid);
   snd_mixer_selem_id_free(sid);

   if (!elem)
   {
      hrmp_log_error("Error: simple element '%s' not found on card '%s'",
                     config->devices[device].selem, config->devices[device].name);
      goto error;
   }

   if (snd_mixer_selem_has_playback_volume(elem) == 0)
   {
      hrmp_log_error("Error: element '%s' has no playback volume",
                     config->devices[device].selem);
      goto error;
   }

   snd_mixer_selem_get_playback_volume_range(elem, &minv, &maxv);
   /* Map percent -> range */
   vol = minv + (volume * (maxv - minv)) / 100;

   /* Set volume for all channels (left/right) */
   if ((err = snd_mixer_selem_set_playback_volume_all(elem, vol)) < 0)
   {
      hrmp_log_error("Error: set playback volume: %s", snd_strerror(err));
      goto error;
   }

   config->volume = volume;

   snd_mixer_close(handle);

   return 0;

error:

   if (handle != NULL)
   {
      snd_mixer_close(handle);
   }

   return 1;
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
      int current_hour = 0;
      int current_min = 0;
      int current_sec = 0;
      int total_hour = 0;
      int total_min = 0;
      int total_sec = 0;
      int percent = 0;

      memset(&t[0], 0, sizeof(t));

      current = (int)((double)pb->current_samples / pb->fm->sample_rate);
      current_min = (int)(current) / 60;

      if (current_min >= 60)
      {
         current_hour = (int)(current_min / 60.0);
         current_min = current_min - (current_hour * 60);
      }

      current_sec = current - ((current_hour * 60 * 60) + (current_min * 60));

      total_min = (int)pb->fm->duration / 60;

      if (total_min >= 60)
      {
         total_hour = (int)(total_min / 60.0);
         total_min = total_min - (total_hour * 60);
      }

      total_sec = pb->fm->duration - ((total_hour * 60 * 60) + (total_min * 60));

      percent = (int)(current * 100 / pb->fm->duration);

      if (total_hour > 0)
      {
         snprintf(&t[0], sizeof(t), "%d:%02d:%02d/%d:%02d:%02d",
                  current_hour, current_min, current_sec,
                  total_hour, total_min, total_sec);
      }
      else
      {
         snprintf(&t[0], sizeof(t), "%d:%02d/%d:%02d",
                  current_min, current_sec,
                  total_min, total_sec);
      }

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
      int total_hour = 0;
      int total_min = 0;
      int total_sec = 0;

      memset(&t[0], 0, sizeof(t));

      total_min = (int)(pb->fm->duration) / 60;

      if (total_min >= 60)
      {
         total_hour = (int)(total_min / 60.0);
         total_min = total_min - (total_hour * 60);
      }

      total_sec = pb->fm->duration - ((total_hour * 60 * 60) + (total_min * 60));

      if (total_hour > 0)
      {
         snprintf(&t[0], sizeof(t), "%d:%02d:%02d/%d:%02d:%02d",
                  total_hour, total_min, total_sec,
                  total_hour, total_min, total_sec);
      }
      else
      {
         snprintf(&t[0], sizeof(t), "%d:%02d/%d:%02d", total_min, total_sec, total_min, total_sec);
      }

      printf("\r[%d/%d] %s: %s %s (%s) (100%%)\n", pb->file_number, pb->total_number,
             config->devices[pb->device].name, pb->fm->name, pb->identifier,
             &t[0]);

      fflush(stdout);
   }
}
