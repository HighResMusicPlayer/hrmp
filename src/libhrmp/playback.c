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

#define DOP_MARKER_8MSB 0xFA
#define DOP_MARKER_8LSB 0x05

static int playback_init(int device, int number, int total, snd_pcm_t* pcm_handle, struct file_metadata* fm, struct playback** playback);
static int playback_identifier(struct file_metadata* fm, char** identifer);

static int set_volume(int device, int volume);

static void print_progress(struct playback* pb);
static void print_progress_done(struct playback* pb);

static int read_exact(FILE* f, void* buf, size_t n);

static uint8_t bitrev8(uint8_t x);

static int playback_sndfile(snd_pcm_t* pcm_handle, struct playback* pb,
                            int number, int total);
static int playback_dsf(snd_pcm_t* pcm_handle, struct playback* pb,
                        int number, int total);

static int dop_s32le(FILE* f, struct playback* pb);
static int dsd_u32_be(FILE* f, struct playback* pb);

static int do_keyboard(FILE* f, SNDFILE* sndf, struct playback* pb);

int
hrmp_playback(int device, int number, int total, struct file_metadata* fm)
{
   int ret = 1;
   snd_pcm_t* pcm_handle = NULL;
   struct playback* pb = NULL;
   struct configuration* config = NULL;

   config = (struct configuration*)shmem;

   if (hrmp_alsa_init_handle(device, fm, &pcm_handle))
   {
      hrmp_log_error("Could not initialize '%s' for '%s'",
                     config->devices[device].name, fm->name);
      goto error;
   }

   config->devices[device].is_paused = false;

   if (playback_init(device, number, total, pcm_handle, fm, &pb))
   {
      hrmp_log_error("Could not initialize '%s' for '%s'",
                     config->devices[device].name, fm->name);
      goto error;
   }

   if (fm->type == TYPE_WAV || fm->type == TYPE_FLAC || fm->type == TYPE_MP3)
   {
      ret = playback_sndfile(pcm_handle, pb, number, total);
   }
   else if (fm->type == TYPE_DSF)
   {
      ret = playback_dsf(pcm_handle, pb, number, total);
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
playback_sndfile(snd_pcm_t* pcm_handle, struct playback* pb, int number, int total)
{
   int err;
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

   info = (SF_INFO*)malloc(sizeof(SF_INFO));
   if (info == NULL)
   {
      goto error;
   }
   memset(info, 0, sizeof(SF_INFO));

   f = sf_open(pb->fm->name, SFM_READ, info);
   if (f == NULL)
   {
      goto error;
   }

   if (snd_pcm_get_params(pcm_handle, &pcm_buffer_size, &pcm_period_size) < 0)
   {
      hrmp_log_error("Could not get parameters for '%s'", pb->fm->name);
      goto error;
   }

   // Playback loop
   switch (pb->fm->container)
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
            if (pb->fm->container == 16)
            {
               /* convert 32-bit to 16-bit by shifting (arith shift) */
               int16_t s16 = (int16_t)(sample >> 16); /* lose lower bits */
               output_buffer[outpos++] = (uint8_t)(s16 & 0xFF);
               output_buffer[outpos++] = (uint8_t)((s16 >> 8) & 0xFF);
            }
            else if (pb->fm->container == 24)
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

      if (do_keyboard(NULL, f, pb))
      {
         break;
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

static uint8_t
bitrev8(uint8_t x)
{
   x = (x >> 4) | (x << 4);
   x = ((x & 0xCC) >> 2) | ((x & 0x33) << 2);
   x = ((x & 0xAA) >> 1) | ((x & 0x55) << 1);

   return x;
}

static int
read_exact(FILE* f, void* buf, size_t n)
{
   return fread(buf, 1, n, f) == n ? 0 : -1;
}

static int
playback_dsf(snd_pcm_t* pcm_handle, struct playback* pb, int number, int total)
{
   FILE* f = NULL;
   struct configuration* config = NULL;

   config = (struct configuration*)shmem;

   f = fopen(pb->fm->name, "rb");
   if (f == NULL)
   {
      goto error;
   }

   /* Seek to the data segment */
   if (fseek(f, 92, SEEK_SET) != 0)
   {
      hrmp_log_error("fseek failed");
      goto error;
   }

   if (config->dop && (pb->fm->alsa_snd == SND_PCM_FORMAT_S32 ||
                       pb->fm->alsa_snd == SND_PCM_FORMAT_S32_LE))
   {
      dop_s32le(f, pb);
   }
   else
   {
      dsd_u32_be(f, pb);
   }

   fclose(f);

   return 0;

error:

   if (f != NULL)
   {
      fclose(f);
   }

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
   else if (fm->type == TYPE_DSF)
   {
      id = hrmp_append(id, "DSF/");
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
      case 22579200:
         id = hrmp_append(id, "22.5792MHz");
         break;
      case 45158400:
         id = hrmp_append(id, "45.1584MHz");
         break;
      default:
         printf("Unsupported sample rate: %s/%dHz/%dbits\n", fm->name, fm->sample_rate, fm->bits_per_sample);
         goto error;
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
         hrmp_log_error("Unsupported bits per sample: %s/%dHz/%dbits", fm->name, fm->sample_rate, fm->bits_per_sample);
         goto error;
         break;
   }

   id = hrmp_append_char(id, ']');

   *identifer = id;

   return 0;

error:

   free(id);

   return 1;
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

static int
dop_s32le(FILE* f, struct playback* pb)
{
   uint32_t N = pb->fm->block_size;
   uint8_t* blk = NULL;
   uint32_t frames_per_block = N / 2u;
   size_t bytes_per_frame = 8;
   uint8_t* out = NULL;

   blk = (uint8_t*)malloc(2u * N);
   if (blk == NULL)
   {
      hrmp_log_error("OOM");
      goto error;
   }

   out = (uint8_t*)malloc(frames_per_block * bytes_per_frame);
   if (out == NULL)
   {
      hrmp_log_error("OOM");
      goto error;
   }

   /* Pre-roll to help DAC lock */
   {
      unsigned pre = (pb->fm->sample_rate >= 11289600u) ? 4096 : 2048;
      uint8_t* pr = (uint8_t*)malloc(pre * bytes_per_frame);
      if (pr != NULL)
      {
         uint8_t m = DOP_MARKER_8LSB;
         for (unsigned i = 0; i < pre; ++i)
         {
            pr[i * 8 + 0] = 0x00;
            pr[i * 8 + 1] = 0x00;
            pr[i * 8 + 2] = 0x00;
            pr[i * 8 + 3] = m;
            pr[i * 8 + 4] = 0x00;
            pr[i * 8 + 5] = 0x00;
            pr[i * 8 + 6] = 0x00;
            pr[i * 8 + 7] = m;
            m = (m == DOP_MARKER_8LSB) ? DOP_MARKER_8MSB : DOP_MARKER_8LSB;
         }
         snd_pcm_writei(pb->pcm_handle, pr, pre);
         free(pr);
      }
      else
      {
         hrmp_log_error("DoP: Prefill error");
         goto error;
      }
   }

   uint8_t marker = DOP_MARKER_8LSB;
   uint64_t left = pb->fm->data_size;

   while (left >= (uint64_t)(2u * N))
   {
      if (read_exact(f, blk, 2u * N) < 0)
      {
         break;
      }
      left -= (uint64_t)(2u * N);

      uint8_t* L = blk;
      uint8_t* R = blk + N;

      for (uint32_t j = 0, i = 0; j < N; j += 2, ++i)
      {
         uint8_t l0 = L[j];
         uint8_t l1 = L[j + 1];
         uint8_t r0 = R[j];
         uint8_t r1 = R[j + 1];

         l0 = bitrev8(l0);
         l1 = bitrev8(l1);
         r0 = bitrev8(r0);
         r1 = bitrev8(r1);

         uint8_t t = l0;
         l0 = l1;
         l1 = t;
         t = r0;
         r0 = r1;
         r1 = t;

         uint8_t* frm = &out[i * 8];
         frm[0] = 0x00;
         frm[1] = l0;
         frm[2] = l1;
         frm[3] = marker;
         frm[4] = 0x00;
         frm[5] = r0;
         frm[6] = r1;
         frm[7] = marker;

         marker = (marker == DOP_MARKER_8LSB) ? DOP_MARKER_8MSB : DOP_MARKER_8LSB;
      }

      uint32_t to_write = frames_per_block;
      uint8_t* p = out;

      while (to_write)
      {
         snd_pcm_sframes_t n = snd_pcm_writei(pb->pcm_handle, p, to_write);
         if (n < 0)
         {
            n = snd_pcm_recover(pb->pcm_handle, (int)n, 1);
            if (n < 0)
            {
               hrmp_log_error("ALSA write failed: %s", snd_strerror((int)n));
               goto error;
            }
            continue;
         }

         if ((uint32_t)n > to_write)
         {
            n = to_write;
         }

         p += (size_t)n * bytes_per_frame;
         to_write -= (uint32_t)n;

         print_progress(pb);
         pb->current_samples += pb->fm->block_size * 8;

         if (do_keyboard(f, NULL, pb))
         {
            break;
         }
      }
   }

   print_progress_done(pb);

   free(out);
   free(blk);

   return 0;

error:

   print_progress_done(pb);

   free(out);
   free(blk);

   return 1;
}

static int
dsd_u32_be(FILE* f, struct playback* pb)
{
   uint32_t N = pb->fm->block_size;
   uint8_t* blk = NULL;
   uint32_t frames_per_block = N / 4u;
   size_t bytes_per_frame = 8;
   uint8_t* out = NULL;
   uint64_t left = 0;

   blk = (uint8_t*)malloc(2u * N);
   if (blk == NULL)
   {
      hrmp_log_error("OOM");
      goto error;
   }

   out = (uint8_t*)malloc((size_t)frames_per_block * bytes_per_frame);
   if (out == NULL)
   {
      hrmp_log_error("OOM");
      goto error;
   }

   left = pb->fm->data_size;

   while (left >= (uint64_t)(2u * N))
   {
      if (read_exact(f, blk, 2u * N) < 0)
      {
         break;
      }
      left -= (uint64_t)(2u * N);

      uint8_t* L = blk;
      uint8_t* R = blk + N;

      size_t w = 0;
      for (uint32_t i = 0; i < frames_per_block; ++i)
      {
         uint8_t* lp = &L[i * 4];
         uint8_t* rp = &R[i * 4];
         uint8_t lb0 = bitrev8(lp[0]);
         uint8_t lb1 = bitrev8(lp[1]);
         uint8_t lb2 = bitrev8(lp[2]);
         uint8_t lb3 = bitrev8(lp[3]);
         uint8_t rb0 = bitrev8(rp[0]);
         uint8_t rb1 = bitrev8(rp[1]);
         uint8_t rb2 = bitrev8(rp[2]);
         uint8_t rb3 = bitrev8(rp[3]);

         out[w + 0] = lb0;
         out[w + 1] = lb1;
         out[w + 2] = lb2;
         out[w + 3] = lb3;

         out[w + 4] = rb0;
         out[w + 5] = rb1;
         out[w + 6] = rb2;
         out[w + 7] = rb3;

         w += 8;
      }

      uint32_t to_write = frames_per_block;
      size_t off = 0;
      while (to_write)
      {
         snd_pcm_sframes_t n = snd_pcm_writei(pb->pcm_handle, &out[off], to_write);
         if (n < 0)
         {
            n = snd_pcm_recover(pb->pcm_handle, (int)n, 1);
            if (n < 0)
            {
               hrmp_log_error("ALSA write failed: %s", snd_strerror((int)n));
               goto error;
            }
            continue;
         }

         if ((uint32_t)n > to_write)
         {
            n = to_write;
         }

         off += (size_t)n * bytes_per_frame;
         to_write -= (uint32_t)n;

         print_progress(pb);
         pb->current_samples += pb->fm->block_size * 8;

         if (do_keyboard(f, NULL, pb))
         {
            break;
         }
      }
   }

   print_progress_done(pb);

   free(out);
   free(blk);

   return 0;

error:

   print_progress_done(pb);

   free(out);
   free(blk);

   return 1;
}

static int
do_keyboard(FILE* f, SNDFILE* sndf, struct playback* pb)
{
   int keyboard_action;
   struct configuration* config = NULL;

   config = (struct configuration*)shmem;

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
      goto skip;
   }
   else if (keyboard_action == KEYBOARD_SPACE)
   {
      if (config->devices[pb->device].is_paused)
      {
         config->devices[pb->device].is_paused = false;
      }
      else
      {
         config->devices[pb->device].is_paused = true;
         SLEEP_AND_GOTO(10000L, keyboard);
      }
   }
   else if (keyboard_action == KEYBOARD_UP ||
            keyboard_action == KEYBOARD_DOWN ||
            keyboard_action == KEYBOARD_LEFT ||
            keyboard_action == KEYBOARD_RIGHT)
   {
      size_t delta = (size_t)(pb->fm->total_samples / pb->fm->duration);
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
         if (f != NULL)
         {
            fseek(f, 0, SEEK_END);
         }
         else
         {
            sf_seek(sndf, 0, SEEK_END);
         }
         pb->current_samples = pb->fm->total_samples;
      }
      else if (new_position <= 0)
      {
         if (f != NULL)
         {
            fseek(f, 0, SEEK_SET);
         }
         else
         {
            sf_seek(sndf, 0, SEEK_SET);
         }
         pb->current_samples = 0;
      }
      else
      {
         if (f != NULL)
         {
            fseek(f, delta, SEEK_CUR);
         }
         else
         {
            sf_seek(sndf, delta, SEEK_CUR);
         }
         pb->current_samples += delta;
      }

      print_progress(pb);
   }
   else if (keyboard_action == KEYBOARD_COMMA)
   {
      int new_volume = config->volume - 5;

      if (!config->is_muted)
      {
         set_volume(pb->device, new_volume);
      }
   }
   else if (keyboard_action == KEYBOARD_PERIOD)
   {
      int new_volume = config->volume + 5;

      if (!config->is_muted)
      {
         set_volume(pb->device, new_volume);
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

      set_volume(pb->device, new_volume);
   }
   else if (keyboard_action == KEYBOARD_SLASH)
   {
      int new_volume = 100;

      config->is_muted = false;
      set_volume(pb->device, new_volume);
   }
   else
   {
      if (config->devices[pb->device].is_paused)
      {
         SLEEP_AND_GOTO(10000L, keyboard);
      }
   }

   return 0;

skip:

   return 1;
}
