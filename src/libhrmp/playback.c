/*
 * Copyright (C) 2025 The HighResMusicPlayer community
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/* hrmp */
#include <hrmp.h>
#include <alsa.h>
#include <devices.h>
#include <files.h>
#include <keyboard.h>
#include <logging.h>
#include <mkv.h>
#include <playback.h>
#include <ringbuffer.h>
#include <utils.h>

/* system */
#include <limits.h>
#include <math.h>
#include <sndfile.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <alsa/asoundlib.h>

#define DOP_MARKER_8MSB      0xFA
#define DOP_MARKER_8LSB      0x05

#define HRMP_DSD_FADEOUT_MS  20u
#define HRMP_DSD_POSTROLL_MS 60u

static int playback_init(int number, int total, snd_pcm_t* pcm_handle, struct file_metadata* fm, struct playback** playback);
static int playback_identifier(struct file_metadata* fm, char** identifer);

static char* format_output(struct playback* pb);
static char* get_progress(struct playback* pb);
static void print_progress_done(struct playback* pb);

static int read_exact(FILE* f, struct ringbuffer* rb, void* buf, size_t n);

static uint8_t bitrev8(uint8_t x);

static int playback_sndfile(snd_pcm_t* pcm_handle, struct playback* pb,
                            int number, int total, bool* next);
static int playback_dsf(snd_pcm_t* pcm_handle, struct playback* pb,
                        int number, int total, bool* next);
static int playback_dff(snd_pcm_t* pcm_handle, struct playback* pb,
                        int number, int total, bool* next);
static int playback_mkv(snd_pcm_t* pcm_handle, struct playback* pb,
                        int number, int total, bool* next);

static void writei_all(snd_pcm_t* h, void* buf, snd_pcm_uframes_t frames, size_t bytes_per_frame);
static unsigned frames_from_ms(struct playback* pb, unsigned ms);

static void write_dsd_center_pad(struct playback* pb, unsigned frames, uint8_t* marker);
static void write_dsd_fadeout(struct playback* pb, unsigned ms, uint8_t* marker);
static int dsd_play_dop_s32le(FILE* f, struct playback* pb,
                              uint32_t in_channels, uint32_t stride_per_ch_hint,
                              uint64_t bytes_left, bool* next);
static int dsd_play_native_u32_be(FILE* f, struct playback* pb,
                                  uint32_t in_channels,
                                  uint32_t stride_per_ch_hint,
                                  uint64_t bytes_left,
                                  bool* next);

static int do_keyboard(FILE* f, SNDFILE* sndf, struct playback* pb, char** k);

static void
normalize_pcm_rate(struct configuration* config, struct file_metadata* fm)
{
   if (!fm)
   {
      return;
   }

   unsigned new_pcm_rate = fm->sample_rate;

   if (fm->bits_per_sample == 1)
   {
      if (config && config->dop)
      {
         if (fm->sample_rate >= 16u)
         {
            new_pcm_rate = fm->sample_rate / 16u;
         }
      }
      else
      {
         if (fm->sample_rate >= 32u)
         {
            new_pcm_rate = fm->sample_rate / 32u;
         }
      }
   }

   fm->pcm_rate = new_pcm_rate;
}

static void
writei_all(snd_pcm_t* h, void* buf, snd_pcm_uframes_t frames, size_t bytes_per_frame)
{
   uint8_t* p = (uint8_t*)buf;
   snd_pcm_sframes_t remaining = frames;

   while (remaining > 0)
   {
      snd_pcm_sframes_t w = snd_pcm_writei(h, p, remaining);
      if (w == -EPIPE)
      {
         snd_pcm_prepare(h);
         continue;
      }
      else if (w < 0)
      {
         break;
      }
      p += (size_t)w * bytes_per_frame;
      remaining -= w;
   }
}

static unsigned
frames_from_ms(struct playback* pb, unsigned ms)
{
   unsigned rate =
      (pb && pb->fm && pb->fm->pcm_rate) ? pb->fm->pcm_rate
                                         : (pb && pb->fm ? pb->fm->sample_rate : 0u);

   if (rate == 0)
   {
      return 0;
   }

   return (unsigned)((uint64_t)rate * (uint64_t)ms / 1000u);
}

static void
write_dsd_center_pad(struct playback* pb, unsigned frames, uint8_t* marker)
{
   struct configuration* config = (struct configuration*)shmem;
   uint32_t ch_out = 2;
   size_t bytes_per_frame = (size_t)ch_out * 4;
   size_t sz = (size_t)frames * bytes_per_frame;

   if (!frames)
   {
      return;
   }

   uint8_t* pr = (uint8_t*)malloc(sz);
   if (pr == NULL)
   {
      hrmp_log_error("Center pad OOM (%zu bytes)", sz);
      return;
   }

   if (config->dop)
   {
      uint8_t m = (marker != NULL) ? *marker : DOP_MARKER_8LSB;
      for (unsigned i = 0; i < frames; ++i)
      {
         uint8_t a = (i & 1) ? 0x55 : 0xAA;
         uint8_t b = (uint8_t)~a;
         for (uint32_t c = 0; c < ch_out; ++c)
         {
            size_t off = (size_t)i * bytes_per_frame + (size_t)c * 4u;
            pr[off + 0] = 0x00;
            pr[off + 1] = a;
            pr[off + 2] = b;
            pr[off + 3] = m;
         }
         m = (m == DOP_MARKER_8LSB) ? DOP_MARKER_8MSB : DOP_MARKER_8LSB;
      }
      writei_all(pb->pcm_handle, pr, frames, bytes_per_frame);
      if (marker != NULL)
      {
         *marker = m;
      }
   }
   else
   {
      for (unsigned i = 0; i < frames; ++i)
      {
         uint8_t a = (i & 1) ? 0x55 : 0xAA;
         uint8_t b = (uint8_t)~a;
         for (uint32_t c = 0; c < ch_out; ++c)
         {
            size_t off = (size_t)i * bytes_per_frame + (size_t)c * 4u;
            pr[off + 0] = a;
            pr[off + 1] = b;
            pr[off + 2] = a;
            pr[off + 3] = b;
         }
      }
      writei_all(pb->pcm_handle, pr, frames, bytes_per_frame);
   }

   free(pr);
}

static void
write_dsd_fadeout(struct playback* pb, unsigned ms, uint8_t* marker)
{
   unsigned frames = frames_from_ms(pb, ms);

   write_dsd_center_pad(pb, frames, marker);
}

int
hrmp_playback(int number, int total, struct file_metadata* fm, bool* next)
{
   int ret = 1;
   snd_pcm_t* pcm_handle = NULL;
   struct playback* pb = NULL;
   struct configuration* config = NULL;

   config = (struct configuration*)shmem;

   *next = true;

   normalize_pcm_rate(config, fm);

   if (hrmp_alsa_init_handle(fm, &pcm_handle))
   {
      hrmp_log_error("Could not initialize '%s' for '%s'", &config->active_device.name[0], fm->name);
      goto error;
   }

   config->active_device.is_paused = false;

   if (playback_init(number, total, pcm_handle, fm, &pb))
   {
      hrmp_log_error("Could not initialize '%s' for '%s'",
                     &config->active_device.name[0], fm->name);
      goto error;
   }

   if (config->metadata || config->developer)
   {
      hrmp_print_file_metadata(fm);
   }

   if (fm->type == TYPE_WAV || fm->type == TYPE_FLAC || fm->type == TYPE_MP3)
   {
      ret = playback_sndfile(pcm_handle, pb, number, total, next);
   }
   else if (fm->type == TYPE_DSF)
   {
      ret = playback_dsf(pcm_handle, pb, number, total, next);
   }
   else if (fm->type == TYPE_DFF)
   {
      ret = playback_dff(pcm_handle, pb, number, total, next);
   }
   else if (fm->type == TYPE_MKV)
   {
      ret = playback_mkv(pcm_handle, pb, number, total, next);
   }
   else
   {
      goto error;
   }

   hrmp_alsa_close_handle(pcm_handle);
   if (pb != NULL)
   {
      hrmp_ringbuffer_destroy(pb->rb);
      free(pb);
   }

   return ret;

error:

   if (pcm_handle != NULL)
   {
      hrmp_alsa_close_handle(pcm_handle);
   }
   if (pb != NULL)
   {
      hrmp_ringbuffer_destroy(pb->rb);
      free(pb);
   }

   return 1;
}

struct sndfile_vio_state
{
   FILE* fp;
   struct ringbuffer* rb;
   uint64_t pos;
   uint64_t file_size;
   struct playback* pb;
};

static size_t
ringbuffer_target_capacity(size_t file_size)
{
   if (file_size == 0)
   {
      return HRMP_RINGBUFFER_MIN_BYTES;
   }

   size_t target = MIN(HRMP_RINGBUFFER_MAX_BYTES, file_size);
   if (target < HRMP_RINGBUFFER_MIN_BYTES)
   {
      target = HRMP_RINGBUFFER_MIN_BYTES;
   }

   return target;
}

static size_t
ringbuffer_target_max(size_t file_size)
{
   if (file_size > 0 && file_size < HRMP_RINGBUFFER_MAX_BYTES)
   {
      return file_size < HRMP_RINGBUFFER_MIN_BYTES ? HRMP_RINGBUFFER_MIN_BYTES : file_size;
   }

   return HRMP_RINGBUFFER_MAX_BYTES;
}

static void
ensure_ringbuffer_target(struct ringbuffer* rb, size_t file_size)
{
   if (rb == NULL)
   {
      return;
   }

   size_t target = ringbuffer_target_capacity(file_size);
   size_t cur = hrmp_ringbuffer_size(rb);
   if (target <= cur)
   {
      return;
   }

   /* Ensure capacity >= target (need free space == target - cur) */
   (void)hrmp_ringbuffer_ensure_write(rb, target - cur);
}

static void
prefill_ringbuffer(FILE* f, struct ringbuffer* rb, size_t file_size)
{
   if (f == NULL || rb == NULL)
   {
      return;
   }

   off_t pos = ftello(f);
   if (pos >= 0 && (size_t)pos <= file_size)
   {
      ensure_ringbuffer_target(rb, file_size - (size_t)pos);
   }

   while (hrmp_ringbuffer_size(rb) < hrmp_ringbuffer_capacity(rb))
   {
      if (hrmp_ringbuffer_ensure_write(rb, 1))
      {
         break;
      }

      void* wp = NULL;
      size_t span = hrmp_ringbuffer_get_write_span(rb, &wp);
      if (span == 0)
      {
         break;
      }

      size_t got = fread(wp, 1, span, f);
      if (got == 0)
      {
         break;
      }

      if (hrmp_ringbuffer_produce(rb, got))
      {
         break;
      }
   }
}

static size_t
read_some(FILE* f, struct ringbuffer* rb, void* buf, size_t n, size_t file_size)
{
   if (rb == NULL)
   {
      return fread(buf, 1, n, f);
   }

   off_t pos = ftello(f);
   if (pos >= 0 && (size_t)pos <= file_size)
   {
      ensure_ringbuffer_target(rb, file_size - (size_t)pos);
   }

   uint8_t* out = (uint8_t*)buf;
   size_t off = 0;

   while (off < n)
   {
      void* rp = NULL;
      size_t have = hrmp_ringbuffer_peek(rb, &rp);
      if (have > 0)
      {
         size_t take = (n - off) < have ? (n - off) : have;
         memcpy(out + off, rp, take);
         hrmp_ringbuffer_consume(rb, take);
         off += take;
         continue;
      }

      if (hrmp_ringbuffer_ensure_write(rb, 1))
      {
         break;
      }

      void* wp = NULL;
      size_t span = hrmp_ringbuffer_get_write_span(rb, &wp);
      if (span == 0)
      {
         if (hrmp_ringbuffer_ensure_write(rb, hrmp_ringbuffer_capacity(rb) / 2u))
         {
            break;
         }
         continue;
      }

      size_t got = fread(wp, 1, span, f);
      if (got == 0)
      {
         break;
      }

      if (hrmp_ringbuffer_produce(rb, got))
      {
         break;
      }
   }

   return off;
}

static sf_count_t
sndfile_vio_get_filelen(void* user_data)
{
   struct sndfile_vio_state* st = (struct sndfile_vio_state*)user_data;
   return st ? (sf_count_t)st->file_size : 0;
}

static sf_count_t
sndfile_vio_seek(sf_count_t offset, int whence, void* user_data)
{
   struct sndfile_vio_state* st = (struct sndfile_vio_state*)user_data;
   if (!st)
   {
      return -1;
   }

   int64_t base = 0;
   if (whence == SEEK_SET)
   {
      base = 0;
   }
   else if (whence == SEEK_CUR)
   {
      base = (int64_t)st->pos;
   }
   else if (whence == SEEK_END)
   {
      base = (int64_t)st->file_size;
   }
   else
   {
      return -1;
   }

   int64_t newpos = base + (int64_t)offset;
   if (newpos < 0)
   {
      newpos = 0;
   }
   if ((uint64_t)newpos > st->file_size)
   {
      newpos = (int64_t)st->file_size;
   }

   if (fseeko(st->fp, (off_t)newpos, SEEK_SET) != 0)
   {
      return -1;
   }
   if (st->rb != NULL)
   {
      hrmp_ringbuffer_reset(st->rb);
      prefill_ringbuffer(st->fp, st->rb, st->file_size);
   }

   st->pos = (uint64_t)newpos;
   if (st->pb != NULL)
   {
      st->pb->bytes_left = st->file_size - st->pos;
   }

   return (sf_count_t)st->pos;
}

static sf_count_t
sndfile_vio_read(void* ptr, sf_count_t count, void* user_data)
{
   struct sndfile_vio_state* st = (struct sndfile_vio_state*)user_data;
   if (!st || count <= 0)
   {
      return 0;
   }

   size_t got = read_some(st->fp, st->rb, ptr, (size_t)count, (size_t)st->file_size);
   st->pos += (uint64_t)got;
   if (st->pb != NULL)
   {
      st->pb->bytes_left = (st->pos <= st->file_size) ? (st->file_size - st->pos) : 0;
   }
   return (sf_count_t)got;
}

static sf_count_t
sndfile_vio_write(const void* ptr, sf_count_t count, void* user_data)
{
   (void)ptr;
   (void)count;
   (void)user_data;
   return 0;
}

static sf_count_t
sndfile_vio_tell(void* user_data)
{
   struct sndfile_vio_state* st = (struct sndfile_vio_state*)user_data;
   return st ? (sf_count_t)st->pos : 0;
}

static int
playback_sndfile(snd_pcm_t* pcm_handle, struct playback* pb, int number, int total, bool* next)
{
   int err;
   FILE* fp = NULL;
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

   *next = true;

   info = (SF_INFO*)malloc(sizeof(SF_INFO));
   if (info == NULL)
   {
      goto error;
   }
   memset(info, 0, sizeof(SF_INFO));

   fp = fopen(pb->fm->name, "rb");
   if (fp == NULL)
   {
      goto error;
   }

   if (pb->rb != NULL)
   {
      hrmp_ringbuffer_reset(pb->rb);
   }
   pb->bytes_left = pb->file_size;

   struct sndfile_vio_state vio_state;
   memset(&vio_state, 0, sizeof(vio_state));
   vio_state.fp = fp;
   vio_state.rb = pb->rb;
   vio_state.pos = 0;
   vio_state.file_size = pb->file_size;
   vio_state.pb = pb;

   SF_VIRTUAL_IO vio;
   memset(&vio, 0, sizeof(vio));
   vio.get_filelen = sndfile_vio_get_filelen;
   vio.seek = sndfile_vio_seek;
   vio.read = sndfile_vio_read;
   vio.write = sndfile_vio_write;
   vio.tell = sndfile_vio_tell;

   f = sf_open_virtual(&vio, SFM_READ, info, &vio_state);
   if (f == NULL)
   {
      fclose(fp);
      goto error;
   }

   if (snd_pcm_get_params(pcm_handle, &pcm_buffer_size, &pcm_period_size) < 0)
   {
      hrmp_log_error("Could not get parameters for '%s'", pb->fm->name);
      goto error;
   }

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

   bytes_per_frame = (size_t)(bytes_per_sample * 2);

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

   while ((frames_read = sf_readf_int(f, input_buffer, pcm_period_size)) > 0)
   {
      size_t outpos = 0;
      int in_ch = info->channels;
      char* p = NULL;
      char* k = NULL;
      int kb = 0;

      for (sf_count_t fi = 0; fi < frames_read; ++fi)
      {
         if (in_ch == 2)
         {
            int32_t L = input_buffer[fi * in_ch + 0];
            int32_t R = input_buffer[fi * in_ch + 1];

            if (pb->fm->container == 16)
            {
               int16_t l16 = (int16_t)(L >> 16);
               int16_t r16 = (int16_t)(R >> 16);
               output_buffer[outpos++] = (uint8_t)(l16 & 0xFF);
               output_buffer[outpos++] = (uint8_t)((l16 >> 8) & 0xFF);
               output_buffer[outpos++] = (uint8_t)(r16 & 0xFF);
               output_buffer[outpos++] = (uint8_t)((r16 >> 8) & 0xFF);
            }
            else if (pb->fm->container == 24)
            {
               output_buffer[outpos++] = (uint8_t)(L & 0xFF);
               output_buffer[outpos++] = (uint8_t)((L >> 8) & 0xFF);
               output_buffer[outpos++] = (uint8_t)((L >> 16) & 0xFF);
               output_buffer[outpos++] = (uint8_t)(R & 0xFF);
               output_buffer[outpos++] = (uint8_t)((R >> 8) & 0xFF);
               output_buffer[outpos++] = (uint8_t)((R >> 16) & 0xFF);
            }
            else
            {
               output_buffer[outpos++] = (uint8_t)(L & 0xFF);
               output_buffer[outpos++] = (uint8_t)((L >> 8) & 0xFF);
               output_buffer[outpos++] = (uint8_t)((L >> 16) & 0xFF);
               output_buffer[outpos++] = (uint8_t)((L >> 24) & 0xFF);
               output_buffer[outpos++] = (uint8_t)(R & 0xFF);
               output_buffer[outpos++] = (uint8_t)((R >> 8) & 0xFF);
               output_buffer[outpos++] = (uint8_t)((R >> 16) & 0xFF);
               output_buffer[outpos++] = (uint8_t)((R >> 24) & 0xFF);
            }
         }
         else
         {
            int64_t acc = 0;
            for (int ch = 0; ch < in_ch; ++ch)
            {
               acc += (int64_t)input_buffer[fi * in_ch + ch];
            }
            int32_t mono = (int32_t)(acc / (int64_t)in_ch);

            if (pb->fm->container == 16)
            {
               int16_t s16 = (int16_t)(mono >> 16);
               /* L */
               output_buffer[outpos++] = (uint8_t)(s16 & 0xFF);
               output_buffer[outpos++] = (uint8_t)((s16 >> 8) & 0xFF);
               /* R */
               output_buffer[outpos++] = (uint8_t)(s16 & 0xFF);
               output_buffer[outpos++] = (uint8_t)((s16 >> 8) & 0xFF);
            }
            else if (pb->fm->container == 24)
            {
               /* L */
               output_buffer[outpos++] = (uint8_t)(mono & 0xFF);
               output_buffer[outpos++] = (uint8_t)((mono >> 8) & 0xFF);
               output_buffer[outpos++] = (uint8_t)((mono >> 16) & 0xFF);
               /* R */
               output_buffer[outpos++] = (uint8_t)(mono & 0xFF);
               output_buffer[outpos++] = (uint8_t)((mono >> 8) & 0xFF);
               output_buffer[outpos++] = (uint8_t)((mono >> 16) & 0xFF);
            }
            else
            {
               /* L */
               output_buffer[outpos++] = (uint8_t)(mono & 0xFF);
               output_buffer[outpos++] = (uint8_t)((mono >> 8) & 0xFF);
               output_buffer[outpos++] = (uint8_t)((mono >> 16) & 0xFF);
               output_buffer[outpos++] = (uint8_t)((mono >> 24) & 0xFF);
               /* R */
               output_buffer[outpos++] = (uint8_t)(mono & 0xFF);
               output_buffer[outpos++] = (uint8_t)((mono >> 8) & 0xFF);
               output_buffer[outpos++] = (uint8_t)((mono >> 16) & 0xFF);
               output_buffer[outpos++] = (uint8_t)((mono >> 24) & 0xFF);
            }
         }
      }

      frames_to_write = frames_read;
      w = snd_pcm_writei(pcm_handle, output_buffer, frames_to_write);

      if (w == -EPIPE)
      {
         snd_pcm_prepare(pcm_handle);
         w = snd_pcm_writei(pcm_handle, output_buffer, frames_to_write);
      }

      if (w < 0)
      {
         if ((err = snd_pcm_recover(pcm_handle, (int)w, 0)) < 0)
         {
            break;
         }
      }

      p = get_progress(pb);
      pb->current_samples += (unsigned long)frames_read;

      kb = do_keyboard(NULL, f, pb, &k);

      if (kb == 1 || kb == 2)
      {
         if (kb == 2)
         {
            *next = false;
         }

         free(p);
         p = NULL;
         break;
      }

      memset(input_buffer, 0, input_buffer_size);
      memset(output_buffer, 0, output_buffer_size);

      if (p != NULL)
      {
         printf("%s", p);
         fflush(stdout);
         free(p);
         p = NULL;
      }
      if (k != NULL)
      {
         p = hrmp_append(p, "\n");
         p = hrmp_append(p, k);
         printf("%s\n", p);
         free(k);
         free(p);
         p = NULL;
         k = NULL;
         fflush(stdout);
      }
   }

   snd_pcm_drain(pcm_handle);

   pb->bytes_left = 0;
   if (pb->rb != NULL)
   {
      hrmp_ringbuffer_reset(pb->rb);
   }
   print_progress_done(pb);

   free(input_buffer);
   free(output_buffer);
   free(info);

   sf_close(f);
   fclose(fp);

   return 0;

error:

   if (input_buffer)
   {
      free(input_buffer);
   }
   if (output_buffer)
   {
      free(output_buffer);
   }
   if (info)
   {
      free(info);
   }
   if (f)
   {
      sf_close(f);
   }
   if (fp)
   {
      fclose(fp);
   }

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
read_exact(FILE* f, struct ringbuffer* rb, void* buf, size_t n)
{
   if (rb == NULL)
   {
      return fread(buf, 1, n, f) == n ? 0 : -1;
   }

   uint8_t* out = (uint8_t*)buf;
   size_t off = 0;

   while (off < n)
   {
      size_t remaining = n - off;

      while (hrmp_ringbuffer_size(rb) < remaining)
      {
         if (hrmp_ringbuffer_ensure_write(rb, 1))
         {
            return -1;
         }

         void* wp = NULL;
         size_t span = hrmp_ringbuffer_get_write_span(rb, &wp);
         if (span == 0)
         {
            if (hrmp_ringbuffer_ensure_write(rb, hrmp_ringbuffer_capacity(rb) / 2u))
            {
               return -1;
            }
            continue;
         }

         size_t got = fread(wp, 1, span, f);
         if (got == 0)
         {
            return -1;
         }

         if (hrmp_ringbuffer_produce(rb, got))
         {
            return -1;
         }
      }

      void* rp = NULL;
      size_t have = hrmp_ringbuffer_peek(rb, &rp);
      if (have == 0)
      {
         continue;
      }

      size_t take = remaining < have ? remaining : have;
      memcpy(out + off, rp, take);
      hrmp_ringbuffer_consume(rb, take);
      off += take;
   }

   return 0;
}

static int
playback_dsf(snd_pcm_t* pcm_handle, struct playback* pb, int number, int total, bool* next)
{
   FILE* f = NULL;
   struct configuration* config = NULL;

   config = (struct configuration*)shmem;

   *next = true;

   f = fopen(pb->fm->name, "rb");
   if (f == NULL)
   {
      goto error;
   }

   if (pb->rb != NULL)
   {
      hrmp_ringbuffer_reset(pb->rb);
   }

   /* Seek to the data segment */
   if (fseek(f, 92, SEEK_SET) != 0)
   {
      hrmp_log_error("fseek failed");
      goto error;
   }

   if (pb->rb != NULL)
   {
      hrmp_ringbuffer_reset(pb->rb);
      prefill_ringbuffer(f, pb->rb, pb->file_size);
   }

   uint32_t ch_in = pb->fm->channels > 0 ? pb->fm->channels : 2;
   uint32_t stride = pb->fm->block_size > 0 ? pb->fm->block_size : 4096;
   uint64_t left = pb->fm->data_size;
   pb->bytes_left = left;

   if (config->dop && (pb->fm->alsa_snd == SND_PCM_FORMAT_S32 ||
                       pb->fm->alsa_snd == SND_PCM_FORMAT_S32_LE))
   {
      dsd_play_dop_s32le(f, pb, ch_in, stride, left, next);
   }
   else
   {
      dsd_play_native_u32_be(f, pb, ch_in, stride, left, next);
   }

   pb->bytes_left = 0;
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
playback_dff(snd_pcm_t* pcm_handle, struct playback* pb, int number, int total, bool* next)
{
   FILE* f = NULL;
   char id4[5] = {0};
   struct configuration* config = NULL;

   config = (struct configuration*)shmem;

   *next = true;

   f = fopen(pb->fm->name, "rb");
   if (f == NULL)
   {
      goto error;
   }

   if (pb->rb != NULL)
   {
      hrmp_ringbuffer_reset(pb->rb);
   }

   if (fread(id4, 1, 4, f) != 4 || strncmp(id4, "FRM8", 4) != 0)
   {
      hrmp_log_error("Not a DFF file for playback");
      goto error;
   }

   hrmp_read_be_u64(f);

   if (fread(id4, 1, 4, f) != 4 || strncmp(id4, "DSD ", 4) != 0)
   {
      hrmp_log_error("Invalid DFF form type");
      goto error;
   }

   for (;;)
   {
      if (fread(id4, 1, 4, f) != 4)
      {
         break;
      }
      uint64_t chunk_size = hrmp_read_be_u64(f);
      if (strncmp(id4, "DSD ", 4) == 0)
      {
         if (pb->rb != NULL)
         {
            hrmp_ringbuffer_reset(pb->rb);
            prefill_ringbuffer(f, pb->rb, pb->file_size);
         }

         uint32_t ch_in = pb->fm->channels > 0 ? pb->fm->channels : 2;
         uint32_t stride = 4096;
         if (config->dop && (pb->fm->alsa_snd == SND_PCM_FORMAT_S32 ||
                             pb->fm->alsa_snd == SND_PCM_FORMAT_S32_LE))
         {
            pb->bytes_left = chunk_size;
            dsd_play_dop_s32le(f, pb, ch_in, stride, chunk_size, next);
         }
         else
         {
            pb->bytes_left = chunk_size;
            dsd_play_native_u32_be(f, pb, ch_in, stride, chunk_size, next);
         }
         goto done;
      }
      else if (strncmp(id4, "DST ", 4) == 0)
      {
         hrmp_log_error("DST-compressed DFF is not supported (CMPR='DST ')");
         goto error;
      }
      else
      {
         if (fseek(f, (long)chunk_size, SEEK_CUR) != 0)
         {
            break;
         }
      }
   }

done:

   pb->bytes_left = 0;
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
playback_mkv(snd_pcm_t* pcm_handle, struct playback* pb, int number, int total, bool* next)
{
   MkvDemuxer* demux = NULL;
   MkvAudioInfo ai;

   *next = true;

   pb->bytes_left = pb->file_size;
   if (pb->rb != NULL)
   {
      hrmp_ringbuffer_reset(pb->rb);
   }

   if (hrmp_mkv_open_path_rb(pb->fm->name, pb->rb, pb->file_size, &pb->bytes_left, &demux) < 0 || !demux)
   {
      hrmp_log_error("MKV: open failed: %s", pb->fm->name);
      return 1;
   }
   if (hrmp_mkv_get_audio_info(demux, &ai) < 0)
   {
      hrmp_log_error("MKV: audio info failed");
      goto error;
   }

   unsigned in_channels = pb->fm->channels ? pb->fm->channels : (unsigned)ai.channels;
   unsigned bits = pb->fm->bits_per_sample ? pb->fm->bits_per_sample : (unsigned)ai.bit_depth;
   unsigned bps8 = bits / 8u;
   size_t in_bytes_per_frame = (size_t)in_channels * (size_t)bps8;
   unsigned sr = pb->fm->sample_rate ? pb->fm->sample_rate : (unsigned)(ai.sample_rate + 0.5);

   if (in_channels == 0 || bps8 == 0 || in_bytes_per_frame == 0 || sr == 0)
   {
      hrmp_log_error("MKV: invalid PCM geometry ch=%u bits=%u sr=%u", in_channels, bits, sr);
      goto error;
   }

   int64_t last_pts_ns = -1;

   for (;;)
   {
      int kb;
      char* p = NULL;
      char* k = NULL;

      kb = do_keyboard(NULL, NULL, pb, &k);
      if (kb == 1 || kb == 2)
      {
         break;
      }
      else if (kb == 3)
      {
         uint64_t target_samples = (uint64_t)pb->current_samples;
         uint64_t target_ns = (sr > 0) ? (target_samples * 1000000000ULL) / (uint64_t)sr : 0ULL;

         hrmp_mkv_close(demux);
         pb->bytes_left = pb->file_size;
         if (pb->rb != NULL)
         {
            hrmp_ringbuffer_reset(pb->rb);
         }
         if (hrmp_mkv_open_path_rb(pb->fm->name, pb->rb, pb->file_size, &pb->bytes_left, &demux) < 0 || !demux)
         {
            hrmp_log_error("MKV: reopen failed for seek");
            goto error;
         }
         for (;;)
         {
            MkvPacket spkt = {0};
            int sgot = hrmp_mkv_read_packet(demux, &spkt);
            if (sgot <= 0)
            {
               hrmp_mkv_free_packet(&spkt);
               break;
            }
            if (spkt.pts_ns >= 0 && (uint64_t)spkt.pts_ns >= target_ns)
            {
               hrmp_mkv_free_packet(&spkt);
               break;
            }
            hrmp_mkv_free_packet(&spkt);
         }

         hrmp_alsa_reset_handle(pb->pcm_handle);
         last_pts_ns = (int64_t)target_ns;
      }

      MkvPacket pkt = {0};
      int got = hrmp_mkv_read_packet(demux, &pkt);
      if (got < 0)
      {
         hrmp_mkv_free_packet(&pkt);
         hrmp_log_error("MKV: read error");
         goto error;
      }
      if (got == 0)
      {
         break;
      }

      size_t in_frames = in_bytes_per_frame ? (pkt.size / in_bytes_per_frame) : 0;
      if (in_frames == 0)
      {
         hrmp_mkv_free_packet(&pkt);
         continue;
      }

      if (in_channels == 2)
      {
         size_t out_bpf = (size_t)2 * (size_t)bps8;
         writei_all(pcm_handle, pkt.data, (snd_pcm_uframes_t)in_frames, out_bpf);
      }
      else
      {
         size_t out_bpf = (size_t)2 * (size_t)bps8;
         size_t out_bytes = in_frames * out_bpf;
         uint8_t* out = (uint8_t*)malloc(out_bytes);
         if (!out)
         {
            hrmp_log_error("MKV: OOM");
            hrmp_mkv_free_packet(&pkt);
            goto error;
         }

         if (bps8 == 2)
         {
            const int16_t* in = (const int16_t*)pkt.data;
            size_t w = 0;
            for (size_t i = 0; i < in_frames; ++i)
            {
               int64_t acc = 0;
               for (unsigned ch = 0; ch < in_channels; ++ch)
               {
                  acc += (int64_t)in[i * in_channels + ch];
               }
               int16_t s = (int16_t)(acc / (int64_t)in_channels);
               /* L */
               out[w++] = (uint8_t)(s & 0xFF);
               out[w++] = (uint8_t)((s >> 8) & 0xFF);
               /* R */
               out[w++] = (uint8_t)(s & 0xFF);
               out[w++] = (uint8_t)((s >> 8) & 0xFF);
            }
         }
         else if (bps8 == 3)
         {
            const uint8_t* in = (const uint8_t*)pkt.data;
            size_t w = 0;
            for (size_t i = 0; i < in_frames; ++i)
            {
               int64_t acc = 0;
               for (unsigned ch = 0; ch < in_channels; ++ch)
               {
                  const uint8_t* p = &in[(i * in_channels + ch) * 3];
                  int32_t v = (int32_t)(p[0] | (p[1] << 8) | (p[2] << 16));
                  if (p[2] & 0x80)
                  {
                     v |= 0xFF000000;
                  }
                  acc += (int64_t)v;
               }
               int32_t s = (int32_t)(acc / (int64_t)in_channels);
               /* L */
               out[w++] = (uint8_t)(s & 0xFF);
               out[w++] = (uint8_t)((s >> 8) & 0xFF);
               out[w++] = (uint8_t)((s >> 16) & 0xFF);
               /* R */
               out[w++] = (uint8_t)(s & 0xFF);
               out[w++] = (uint8_t)((s >> 8) & 0xFF);
               out[w++] = (uint8_t)((s >> 16) & 0xFF);
            }
         }
         else if (bps8 == 4)
         {
            const int32_t* in = (const int32_t*)pkt.data;
            size_t w = 0;
            for (size_t i = 0; i < in_frames; ++i)
            {
               int64_t acc = 0;
               for (unsigned ch = 0; ch < in_channels; ++ch)
               {
                  acc += (int64_t)in[i * in_channels + ch];
               }
               int32_t s = (int32_t)(acc / (int64_t)in_channels);
               /* L */
               out[w++] = (uint8_t)(s & 0xFF);
               out[w++] = (uint8_t)((s >> 8) & 0xFF);
               out[w++] = (uint8_t)((s >> 16) & 0xFF);
               out[w++] = (uint8_t)((s >> 24) & 0xFF);
               /* R */
               out[w++] = (uint8_t)(s & 0xFF);
               out[w++] = (uint8_t)((s >> 8) & 0xFF);
               out[w++] = (uint8_t)((s >> 16) & 0xFF);
               out[w++] = (uint8_t)((s >> 24) & 0xFF);
            }
         }
         else
         {
            free(out);
            hrmp_log_error("MKV: unsupported bit depth %u for downmix", bits);
            hrmp_mkv_free_packet(&pkt);
            goto error;
         }

         writei_all(pcm_handle, out, (snd_pcm_uframes_t)in_frames, out_bpf);
         free(out);
      }

      if (pkt.pts_ns >= 0)
      {
         last_pts_ns = pkt.pts_ns;
         uint64_t ns = (uint64_t)last_pts_ns;
         uint64_t samples64 = (sr > 0) ? (ns * (uint64_t)sr) / 1000000000ULL : 0ULL;
         pb->current_samples = (unsigned long)(samples64 > (uint64_t)ULONG_MAX ? (uint64_t)ULONG_MAX : samples64);
      }
      else
      {
         pb->current_samples += (unsigned long)in_frames;
      }

      p = get_progress(pb);

      if (p != NULL)
      {
         printf("%s", p);
         fflush(stdout);
         free(p);
         p = NULL;
      }

      if (k != NULL)
      {
         p = hrmp_append(p, "\n");
         p = hrmp_append(p, k);
         printf("%s\n", p);
         free(k);
         free(p);
         p = NULL;
         k = NULL;
         fflush(stdout);
      }

      hrmp_mkv_free_packet(&pkt);
   }

   if (last_pts_ns < 0 && pb->fm->total_samples > 0)
   {
      if (pb->current_samples > pb->fm->total_samples)
      {
         pb->current_samples = pb->fm->total_samples;
      }
   }

   snd_pcm_drain(pcm_handle);

   pb->bytes_left = 0;
   if (pb->rb != NULL)
   {
      hrmp_ringbuffer_reset(pb->rb);
   }
   print_progress_done(pb);
   hrmp_mkv_close(demux);
   return 0;

error:

   print_progress_done(pb);
   hrmp_mkv_close(demux);
   return 1;
}

static void
fmt2(int v, char out[3])
{
   if (v < 0)
   {
      v = 0;
   }
   v = v % 100;
   out[0] = (char)('0' + (v / 10));
   out[1] = (char)('0' + (v % 10));
   out[2] = '\0';
}

static char*
format_output(struct playback* pb)
{
   char* fmt = NULL;
   char* fname = NULL;
   char cur_m2[3], cur_s2[3], tot_m2[3], tot_s2[3];
   double current = 0.0;
   int current_hour = 0;
   int current_min = 0;
   int current_sec = 0;
   int total_hour = 0;
   int total_min = 0;
   int total_sec = 0;
   int percent = 0;
   char* out = NULL;
   struct configuration* config = (struct configuration*)shmem;

   if (config == NULL || pb == NULL || pb->fm == NULL || config->quiet)
   {
      return NULL;
   }

   fmt = (strlen(config->output) > 0) ? config->output : HRMP_DEFAULT_OUTPUT_FORMAT;

   /* Current time from samples and sample_rate */
   if (pb->fm->sample_rate > 0)
   {
      if (pb->current_samples >= pb->fm->total_samples)
      {
         pb->current_samples = pb->fm->total_samples;
      }

      current = (double)pb->current_samples / (double)pb->fm->sample_rate;
   }
   else
   {
      current = 0.0;
   }

   int icur = (int)current;
   current_min = icur / 60;
   current_sec = icur - (current_min * 60);
   if (current_min >= 60)
   {
      current_hour = current_min / 60;
      current_min = current_min - (current_hour * 60);
   }

   /* Total time from fm->duration */
   double totald = pb->fm->duration;
   int itot = (int)totald;
   total_min = itot / 60;
   total_sec = itot - (total_min * 60);
   if (total_min >= 60)
   {
      total_hour = total_min / 60;
      total_min = total_min - (total_hour * 60);
   }

   /* Manual zero-padding */
   fmt2(current_min, cur_m2);
   fmt2(current_sec, cur_s2);
   fmt2(total_min, tot_m2);
   fmt2(total_sec, tot_s2);

   out = hrmp_append(out, "\r");

   size_t len = strlen(fmt);

   for (size_t i = 0; i < len; ++i)
   {
      char c = fmt[i];

      if (c == '%' && (i + 1) < len)
      {
         ++i;
         switch (fmt[i])
         {
            case 'n':
               out = hrmp_append_int(out, pb->file_number);
               break;
            case 'N':
               out = hrmp_append_int(out, pb->total_number);
               break;
            case 'f':
               fname = strrchr(pb->fm->name, '/');
               fname = fname ? fname + 1 : pb->fm->name;
               out = hrmp_append(out, (char*)fname);
               break;
            case 'F':
               out = hrmp_append(out, (char*)pb->fm->name);
               break;
            case 'd':
               out = hrmp_append(out, &config->active_device.name[0]);
               break;
            case 'p':
               if (pb->fm->duration > 0.0)
               {
                  percent = (int)((current * 100.0) / pb->fm->duration);
               }
               else
               {
                  percent = 0;
               }

               if (percent < 0)
               {
                  percent = 0;
               }

               if (percent > 100 ||
                   (pb->current_samples >= pb->fm->total_samples))
               {
                  percent = 100;
               }

               out = hrmp_append_int(out, percent);
               out = hrmp_append_char(out, '%');
               break;
            case 't':
               if (total_hour > 0)
               {
                  out = hrmp_append_int(out, current_hour);
                  out = hrmp_append_char(out, ':');
                  out = hrmp_append(out, cur_m2);
                  out = hrmp_append_char(out, ':');
                  out = hrmp_append(out, cur_s2);
               }
               else
               {
                  out = hrmp_append(out, cur_m2);
                  out = hrmp_append_char(out, ':');
                  out = hrmp_append(out, cur_s2);
               }
               break;
            case 'T':
               if (total_hour > 0)
               {
                  out = hrmp_append_int(out, total_hour);
                  out = hrmp_append_char(out, ':');
                  out = hrmp_append(out, tot_m2);
                  out = hrmp_append_char(out, ':');
                  out = hrmp_append(out, tot_s2);
               }
               else
               {
                  out = hrmp_append(out, tot_m2);
                  out = hrmp_append_char(out, ':');
                  out = hrmp_append(out, tot_s2);
               }
               break;
            case 'i':
               out = hrmp_append(out, pb->identifier);
               break;

            case 'b':
            {
               uint64_t bytes = (pb->rb != NULL) ? (uint64_t)hrmp_ringbuffer_size(pb->rb) : 0;
               uint64_t denom = 1024u * 1024u;
               uint64_t tenths = (bytes * 10u + denom / 2u) / denom;
               out = hrmp_append_int(out, (int)(tenths / 10u));
               out = hrmp_append_char(out, '.');
               out = hrmp_append_char(out, (char)('0' + (int)(tenths % 10u)));
               break;
            }
            case 'B':
            {
               uint64_t bytes = (uint64_t)ringbuffer_target_max(pb->file_size);
               uint64_t denom = 1024u * 1024u;
               uint64_t tenths = (bytes * 10u + denom / 2u) / denom;
               out = hrmp_append_int(out, (int)(tenths / 10u));
               out = hrmp_append_char(out, '.');
               out = hrmp_append_char(out, (char)('0' + (int)(tenths % 10u)));
               break;
            }

            case '%':
               out = hrmp_append_char(out, '%');
               break;
            default:
               out = hrmp_append_char(out, '%');
               out = hrmp_append_char(out, fmt[i]);
               break;
         }
      }
      else if (c == '\\' && (i + 1) < len)
      {
         char n = fmt[i + 1];

         if (n == '0' && (i + 3) < len && fmt[i + 2] == '3' && fmt[i + 3] == '3')
         {
            /* Octal escape for ESC: \033 */
            out = hrmp_append_char(out, '\x1b');
            i += 3;
         }
         else if ((n == 'x' || n == 'X') &&
                  (i + 3) < len && (fmt[i + 2] == '1' && (fmt[i + 3] == 'b' || fmt[i + 3] == 'B')))
         {
            /* Hex escape for ESC: \x1b or \x1B */
            out = hrmp_append_char(out, '\x1b');
            i += 3;
         }
         else if (n == 'e' || n == 'E')
         {
            /* GNU-style \e escape for ESC */
            out = hrmp_append_char(out, '\x1b');
            ++i;
         }
         else if (n == 'n')
         {
            out = hrmp_append_char(out, '\n');
            ++i;
         }
         else if (n == 'r')
         {
            out = hrmp_append_char(out, '\r');
            ++i;
         }
         else if (n == 't')
         {
            out = hrmp_append_char(out, '\t');
            ++i;
         }
         else if (n == '\\')
         {
            out = hrmp_append_char(out, '\\');
            ++i;
         }
         else
         {
            /* Unknown escape, keep the backslash literal */
            out = hrmp_append_char(out, c);
         }
      }
      else
      {
         out = hrmp_append_char(out, c);
      }
   }

   return out;
}

static int
playback_init(int number, int total,
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

   if (playback_identifier(fm, &desc))
   {
      goto error;
   }

   pb->file_size = fm->file_size ? fm->file_size : hrmp_get_file_size(fm->name);
   pb->bytes_left = pb->file_size;
   pb->file_number = number;
   pb->total_number = total;
   memcpy(&pb->identifier, desc, strlen(desc));
   pb->current_samples = 0;
   pb->pcm_handle = pcm_handle;
   pb->fm = fm;

   size_t cap = ringbuffer_target_capacity(pb->file_size);

   /* If file < 256MiB, allow max up to file size (but never below the 4MiB minimum). */
   size_t max_size = (pb->file_size > 0 && pb->file_size < HRMP_RINGBUFFER_MAX_BYTES) ? pb->file_size : HRMP_RINGBUFFER_MAX_BYTES;
   if (max_size < HRMP_RINGBUFFER_MIN_BYTES)
   {
      max_size = HRMP_RINGBUFFER_MIN_BYTES;
   }

   /* Minimum must always be 4MiB so the buffer can shrink over time. */
   size_t min_size = HRMP_RINGBUFFER_MIN_BYTES;

   if (hrmp_ringbuffer_create(min_size, cap, max_size, &pb->rb))
   {
      goto error;
   }

   *playback = pb;

   free(desc);

   return 0;

error:

   if (desc)
   {
      free(desc);
   }
   if (pb)
   {
      hrmp_ringbuffer_destroy(pb->rb);
      free(pb);
   }

   return 1;
}

static int
playback_identifier(struct file_metadata* fm, char** identifer)
{
   char* id = NULL;
   struct configuration* config = NULL;

   config = (struct configuration*)shmem;

   *identifer = NULL;

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
   else if (fm->type == TYPE_MKV)
   {
      if (hrmp_ends_with(fm->name, ".webm"))
      {
         id = hrmp_append(id, "WEBM/");
      }
      else
      {
         id = hrmp_append(id, "MKV/");
      }
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
         if (config->dop)
         {
            id = hrmp_append(id, "176.4kHz");
         }
         else
         {
            id = hrmp_append(id, "2.8224MHz");
         }
         break;
      case 5644800:
         if (config->dop)
         {
            id = hrmp_append(id, "352.8kHz");
         }
         else
         {
            id = hrmp_append(id, "5.6448MHz");
         }
         break;
      case 11289600:
         if (config->dop)
         {
            id = hrmp_append(id, "705.6kHz");
         }
         else
         {
            id = hrmp_append(id, "11.2896MHz");
         }
         break;
      case 22579200:
         if (config->dop)
         {
            id = hrmp_append(id, "1.4112MHz");
         }
         else
         {
            id = hrmp_append(id, "22.5792MHz");
         }
         break;
      case 45158400:
         if (config->dop)
         {
            id = hrmp_append(id, "2.8224MHz");
         }
         else
         {
            id = hrmp_append(id, "45.1584MHz");
         }
         break;
      default:
         printf("Unsupported sample rate: %s/%dHz/%dbits\n", fm->name, fm->sample_rate, fm->bits_per_sample);
         goto error;
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
   }

   *identifer = id;

   return 0;

error:

   if (id)
   {
      free(id);
   }

   return 1;
}

static char*
get_progress(struct playback* pb)
{
   char* print = NULL;
   char* formatted = format_output(pb);

   if (formatted != NULL)
   {
      print = hrmp_append(print, formatted);
      free(formatted);
      formatted = NULL;
   }

   return print;
}

static void
print_progress_done(struct playback* pb)
{
   char* print = NULL;
   char* formatted = NULL;

   /* Change playback */
   pb->current_samples = pb->fm->total_samples;

   formatted = format_output(pb);

   if (formatted != NULL)
   {
      print = hrmp_append(print, "\x1b[2K");
      print = hrmp_append(print, formatted);

      printf("%s\n", print);

      free(formatted);
      free(print);

      fflush(stdout);
   }
}

static int
dsd_play_dop_s32le(FILE* f, struct playback* pb,
                   uint32_t in_channels, uint32_t stride_per_ch_hint, uint64_t bytes_left, bool* next)
{
   uint32_t ch_out = 2;
   size_t bytes_per_frame = (size_t)ch_out * 4u;
   uint32_t stride = (stride_per_ch_hint > 0) ? stride_per_ch_hint : 4096u;
   if (stride < 2)
   {
      stride = 2;
   }
   stride = (stride / 2u) * 2u;

   unsigned pre = (pb->fm->sample_rate >= 11289600u) ? 4096 : 2048;
   size_t pre_bytes = (size_t)pre * bytes_per_frame;
   uint8_t* pr = (uint8_t*)malloc(pre_bytes);

   *next = true;

   if (pr)
   {
      uint8_t m = DOP_MARKER_8LSB;
      for (unsigned i = 0; i < pre; ++i)
      {
         uint8_t a = (i & 1) ? 0x55 : 0xAA;
         uint8_t b = (uint8_t)~a;
         for (uint32_t c = 0; c < ch_out; ++c)
         {
            size_t off = (size_t)i * bytes_per_frame + (size_t)c * 4u;
            pr[off + 0] = 0x00;
            pr[off + 1] = a;
            pr[off + 2] = b;
            pr[off + 3] = m;
         }
         m = (m == DOP_MARKER_8LSB) ? DOP_MARKER_8MSB : DOP_MARKER_8LSB;
      }
      snd_pcm_writei(pb->pcm_handle, pr, pre);
      free(pr);
   }
   else
   {
      hrmp_log_error("DoP: Prefill error");
      return 1;
   }

   uint8_t marker = DOP_MARKER_8LSB;

   size_t in_batch_max = (size_t)in_channels * (size_t)stride;
   uint8_t* blk = (uint8_t*)malloc(in_batch_max);
   if (!blk)
   {
      hrmp_log_error("OOM");
      return 1;
   }

   size_t out_cap = (size_t)(stride / 2u) * bytes_per_frame;
   if (out_cap == 0)
   {
      out_cap = bytes_per_frame * 1u;
   }
   uint8_t* out = (uint8_t*)malloc(out_cap);
   if (!out)
   {
      free(blk);
      hrmp_log_error("OOM");
      return 1;
   }

   pb->bytes_left = bytes_left;

   while (bytes_left > 0)
   {
      uint64_t per_ch_avail = bytes_left / (uint64_t)in_channels;
      uint32_t per_ch = stride;
      if (per_ch_avail < (uint64_t)per_ch)
      {
         per_ch = (uint32_t)per_ch_avail;
      }
      per_ch = (per_ch / 2u) * 2u; /* align to 2 bytes */
      if (per_ch < 2u)
      {
         break;
      }

      size_t to_read = (size_t)in_channels * (size_t)per_ch;
      if (read_exact(f, pb->rb, blk, to_read) < 0)
      {
         break;
      }
      bytes_left -= (uint64_t)to_read;
      pb->bytes_left = bytes_left;

      size_t frames = (size_t)per_ch / 2u;
      size_t need = frames * bytes_per_frame;
      if (need > out_cap)
      {
         uint8_t* np = (uint8_t*)realloc(out, need);
         if (!np)
         {
            hrmp_log_error("OOM");
            break;
         }
         out = np;
         out_cap = need;
      }

      size_t woff = 0;
      for (size_t i = 0; i < frames; ++i)
      {
         /* Source ch0 and ch1 if present, else duplicate ch0 to both */
         uint32_t cL = 0;
         uint32_t cR = (in_channels >= 2 ? 1u : 0u);

         const uint8_t* lp = blk + (size_t)cL * (size_t)per_ch + (size_t)i * 2u;
         const uint8_t* rp = blk + (size_t)cR * (size_t)per_ch + (size_t)i * 2u;

         uint8_t l0 = bitrev8(lp[0]), l1 = bitrev8(lp[1]);
         uint8_t r0 = bitrev8(rp[0]), r1 = bitrev8(rp[1]);

         uint8_t t;
         t = l0;
         l0 = l1;
         l1 = t;
         t = r0;
         r0 = r1;
         r1 = t;

         /* L */
         out[woff + 0] = 0x00;
         out[woff + 1] = l0;
         out[woff + 2] = l1;
         out[woff + 3] = marker;
         /* R */
         out[woff + 4] = 0x00;
         out[woff + 5] = r0;
         out[woff + 6] = r1;
         out[woff + 7] = marker;
         woff += 8;

         marker = (marker == DOP_MARKER_8LSB) ? DOP_MARKER_8MSB : DOP_MARKER_8LSB;
      }

      /* Write */
      snd_pcm_sframes_t to_write = (snd_pcm_sframes_t)frames;
      const uint8_t* bytes = out;
      while (to_write > 0)
      {
         char* p = NULL;
         char* k = NULL;
         int kb = 0;
         snd_pcm_sframes_t n = snd_pcm_writei(pb->pcm_handle, bytes, to_write);
         if (n < 0)
         {
            n = snd_pcm_recover(pb->pcm_handle, (int)n, 1);
            if (n < 0)
            {
               hrmp_log_error("ALSA write failed: %s", snd_strerror((int)n));
               goto done;
            }
            continue;
         }
         if (n > to_write)
         {
            n = to_write;
         }
         bytes += (size_t)n * bytes_per_frame;
         to_write -= n;

         /* Each DoP PCM frame carries 16 DSD samples per channel */
         pb->current_samples += (unsigned long)(n * 16u);

         kb = do_keyboard(f, NULL, pb, &k);

         if (kb == 1 || kb == 2)
         {
            if (kb == 2)
            {
               *next = false;
            }
            free(k);
            goto done;
         }

         p = get_progress(pb);

         if (p != NULL)
         {
            printf("%s", p);
            fflush(stdout);
            free(p);
            p = NULL;
         }

         if (k != NULL)
         {
            p = hrmp_append(p, "\n");
            p = hrmp_append(p, k);
            printf("%s\n", p);
            free(k);
            free(p);
            p = NULL;
            k = NULL;
            fflush(stdout);
         }
      }
   }

done:
   write_dsd_fadeout(pb, HRMP_DSD_FADEOUT_MS, &marker);

   snd_pcm_uframes_t buffer_size = 0, period_size = 0;
   if (snd_pcm_get_params(pb->pcm_handle, &buffer_size, &period_size) == 0 && period_size > 0)
   {
      write_dsd_center_pad(pb, (unsigned)period_size, &marker);
   }
   unsigned post_frames = frames_from_ms(pb, HRMP_DSD_POSTROLL_MS);
   write_dsd_center_pad(pb, post_frames, &marker);

   snd_pcm_drain(pb->pcm_handle);

   pb->bytes_left = 0;
   if (pb->rb != NULL)
   {
      hrmp_ringbuffer_reset(pb->rb);
   }
   print_progress_done(pb);

   free(out);
   free(blk);
   return 0;
}

static int
dsd_play_native_u32_be(FILE* f, struct playback* pb,
                       uint32_t in_channels, uint32_t stride_per_ch_hint, uint64_t bytes_left, bool* next)
{
   uint32_t ch_out = 2;
   size_t bytes_per_frame = (size_t)ch_out * 4u;
   uint32_t stride = (stride_per_ch_hint > 0) ? stride_per_ch_hint : 4096u;
   if (stride < 4)
   {
      stride = 4;
   }
   stride = (stride / 4u) * 4u;

   size_t in_batch_max = (size_t)in_channels * (size_t)stride;
   uint8_t* blk = (uint8_t*)malloc(in_batch_max);
   if (!blk)
   {
      hrmp_log_error("OOM");
      return 1;
   }
   size_t out_cap = (size_t)(stride / 4u) * bytes_per_frame;
   if (out_cap == 0)
   {
      out_cap = bytes_per_frame;
   }
   uint8_t* out = (uint8_t*)malloc(out_cap);
   if (!out)
   {
      free(blk);
      hrmp_log_error("OOM");
      return 1;
   }

   bool need_bit_reverse = (pb->fm->type == TYPE_DSF);
   bool interleaved = (pb->fm->type == TYPE_DFF);

   pb->bytes_left = bytes_left;

   while (bytes_left > 0)
   {
      uint64_t per_ch_avail = bytes_left / (uint64_t)in_channels;
      uint32_t per_ch = stride;
      if (per_ch_avail < (uint64_t)per_ch)
      {
         per_ch = (uint32_t)per_ch_avail;
      }
      per_ch = (per_ch / 4u) * 4u;
      if (per_ch < 4u)
      {
         break;
      }

      size_t to_read = (size_t)in_channels * (size_t)per_ch;
      if (read_exact(f, pb->rb, blk, to_read) < 0)
      {
         break;
      }
      bytes_left -= (uint64_t)to_read;
      pb->bytes_left = bytes_left;

      size_t frames = to_read / ((size_t)in_channels * 4u);
      size_t need = frames * bytes_per_frame;
      if (need > out_cap)
      {
         uint8_t* np = (uint8_t*)realloc(out, need);
         if (!np)
         {
            hrmp_log_error("OOM");
            break;
         }
         out = np;
         out_cap = need;
      }

      size_t woff = 0;
      if (interleaved)
      {
         uint32_t cL = 0;
         uint32_t cR = (in_channels >= 2 ? 1u : 0u);

         for (size_t i = 0; i < frames; ++i)
         {
            size_t base = i * (size_t)in_channels * 4u;

            uint8_t lb0 = blk[base + 0 * (size_t)in_channels + cL];
            uint8_t lb1 = blk[base + 1 * (size_t)in_channels + cL];
            uint8_t lb2 = blk[base + 2 * (size_t)in_channels + cL];
            uint8_t lb3 = blk[base + 3 * (size_t)in_channels + cL];

            uint8_t rb0 = blk[base + 0 * (size_t)in_channels + cR];
            uint8_t rb1 = blk[base + 1 * (size_t)in_channels + cR];
            uint8_t rb2 = blk[base + 2 * (size_t)in_channels + cR];
            uint8_t rb3 = blk[base + 3 * (size_t)in_channels + cR];

            out[woff + 0] = lb0;
            out[woff + 1] = lb1;
            out[woff + 2] = lb2;
            out[woff + 3] = lb3;

            out[woff + 4] = rb0;
            out[woff + 5] = rb1;
            out[woff + 6] = rb2;
            out[woff + 7] = rb3;

            woff += 8;
         }
      }
      else
      {
         for (size_t i = 0; i < frames; ++i)
         {
            uint32_t cL = 0;
            uint32_t cR = (in_channels >= 2 ? 1u : 0u);

            const uint8_t* lp = blk + (size_t)cL * (size_t)per_ch + (size_t)i * 4u;
            const uint8_t* rp = blk + (size_t)cR * (size_t)per_ch + (size_t)i * 4u;

            if (need_bit_reverse)
            {
               out[woff + 0] = bitrev8(lp[0]);
               out[woff + 1] = bitrev8(lp[1]);
               out[woff + 2] = bitrev8(lp[2]);
               out[woff + 3] = bitrev8(lp[3]);

               out[woff + 4] = bitrev8(rp[0]);
               out[woff + 5] = bitrev8(rp[1]);
               out[woff + 6] = bitrev8(rp[2]);
               out[woff + 7] = bitrev8(rp[3]);
            }
            else
            {
               out[woff + 0] = lp[0];
               out[woff + 1] = lp[1];
               out[woff + 2] = lp[2];
               out[woff + 3] = lp[3];

               out[woff + 4] = rp[0];
               out[woff + 5] = rp[1];
               out[woff + 6] = rp[2];
               out[woff + 7] = rp[3];
            }

            woff += 8;
         }
      }

      snd_pcm_sframes_t to_write = (snd_pcm_sframes_t)frames;
      const uint8_t* bytes = out;
      while (to_write > 0)
      {
         char* p = NULL;
         char* k = NULL;
         int kb = 0;
         snd_pcm_sframes_t n = snd_pcm_writei(pb->pcm_handle, bytes, to_write);

         if (n < 0)
         {
            n = snd_pcm_recover(pb->pcm_handle, (int)n, 1);
            if (n < 0)
            {
               hrmp_log_error("ALSA write failed: %s", snd_strerror((int)n));
               goto done;
            }
            continue;
         }
         if (n > to_write)
         {
            n = to_write;
         }
         p += (size_t)n * bytes_per_frame;
         to_write -= n;

         p = get_progress(pb);
         /* Each native DSD_U32_BE frame carries 32 DSD samples per channel */
         pb->current_samples += (unsigned long)(n * 32u);

         kb = do_keyboard(f, NULL, pb, &k);

         if (kb == 1 || kb == 2)
         {
            if (kb == 2)
            {
               *next = false;
            }
            free(p);
            p = NULL;
            goto done;
         }

         if (p != NULL)
         {
            printf("%s", p);
            fflush(stdout);
            free(p);
            p = NULL;
         }

         if (k != NULL)
         {
            p = hrmp_append(p, "\n");
            p = hrmp_append(p, k);
            printf("%s\n", p);
            free(k);
            free(p);
            p = NULL;
            k = NULL;
            fflush(stdout);
         }
      }
   }

done:
{
   uint8_t m_ignored = DOP_MARKER_8LSB;
   write_dsd_fadeout(pb, HRMP_DSD_FADEOUT_MS, &m_ignored);
   snd_pcm_uframes_t buffer_size = 0, period_size = 0;
   if (snd_pcm_get_params(pb->pcm_handle, &buffer_size, &period_size) == 0 && period_size > 0)
   {
      write_dsd_center_pad(pb, (unsigned)period_size, &m_ignored);
   }
   unsigned post_frames = frames_from_ms(pb, HRMP_DSD_POSTROLL_MS);
   write_dsd_center_pad(pb, post_frames, &m_ignored);
}

   snd_pcm_drain(pb->pcm_handle);

   pb->bytes_left = 0;
   if (pb->rb != NULL)
   {
      hrmp_ringbuffer_reset(pb->rb);
   }
   print_progress_done(pb);

   free(out);
   free(blk);

   return 0;
}

static int
do_keyboard(FILE* f, SNDFILE* sndf, struct playback* pb, char** print)
{
   char* k = NULL;
   int keyboard_action;
   struct configuration* config = NULL;

   config = (struct configuration*)shmem;

   *print = NULL;

keyboard:
   k = NULL;
   keyboard_action = hrmp_keyboard_get(&k);

   if (keyboard_action == KEYBOARD_Q)
   {
      print_progress_done(pb);
      hrmp_keyboard_mode(false);
      free(k);
      exit(0);
   }
   else if (keyboard_action == KEYBOARD_ENTER)
   {
      free(k);
      return 1;
   }
   else if (keyboard_action == KEYBOARD_BACKSLASH)
   {
      free(k);
      return 2;
   }
   else if (keyboard_action == KEYBOARD_SPACE)
   {
      if (config->active_device.is_paused)
      {
         config->active_device.is_paused = false;
      }
      else
      {
         config->active_device.is_paused = true;
         free(k);
         SLEEP_AND_GOTO(10000L, keyboard);
      }
   }
   else if (keyboard_action == KEYBOARD_UP ||
            keyboard_action == KEYBOARD_DOWN ||
            keyboard_action == KEYBOARD_LEFT ||
            keyboard_action == KEYBOARD_RIGHT)
   {
      int64_t seconds = 0;
      int64_t delta_samples = 0;
      int64_t new_pos_samples = 0;

      if (keyboard_action == KEYBOARD_UP)
      {
         seconds = 60;
      }
      else if (keyboard_action == KEYBOARD_DOWN)
      {
         seconds = -60;
      }
      else if (keyboard_action == KEYBOARD_LEFT)
      {
         seconds = -15;
      }
      else if (keyboard_action == KEYBOARD_RIGHT)
      {
         seconds = 15;
      }

      if (pb->fm->type == TYPE_DSF || pb->fm->type == TYPE_DFF)
      {
         delta_samples = seconds * (int64_t)pb->fm->sample_rate;
      }
      else if (pb->fm->type == TYPE_MKV)
      {
         if (pb->fm->duration > 0.0 && pb->fm->total_samples > 0)
         {
            double samples_per_sec = (double)pb->fm->total_samples / pb->fm->duration;
            delta_samples = (int64_t)(samples_per_sec * (double)seconds);
         }
         else
         {
            delta_samples = seconds * (int64_t)pb->fm->sample_rate;
         }
      }
      else
      {
         delta_samples = (int64_t)((double)seconds * (pb->fm->total_samples / pb->fm->duration));
      }

      new_pos_samples = (int64_t)pb->current_samples + delta_samples;

      if (pb->fm->type == TYPE_DSF)
      {
         uint64_t aligned_bytes = 0;

         if (new_pos_samples <= 0)
         {
            fseek(f, 92L, SEEK_SET);
            pb->current_samples = 0;
         }
         else
         {
            uint64_t bytes_group = (uint64_t)pb->fm->channels * (uint64_t)pb->fm->block_size;
            if (bytes_group == 0)
            {
               bytes_group = (uint64_t)pb->fm->channels * 4096ULL;
            }
            uint64_t approx_target_bytes = (uint64_t)(new_pos_samples / 8) * (uint64_t)pb->fm->channels;
            aligned_bytes = bytes_group ? (approx_target_bytes / bytes_group) * bytes_group : approx_target_bytes;
            if (aligned_bytes > pb->fm->data_size)
            {
               aligned_bytes = (pb->fm->data_size / bytes_group) * bytes_group;
            }
            fseek(f, 92L + (long)aligned_bytes, SEEK_SET);
            pb->current_samples = (unsigned long)((aligned_bytes / (uint64_t)pb->fm->channels) * 8ULL);
            if (pb->current_samples >= pb->fm->total_samples)
            {
               pb->current_samples = pb->fm->total_samples;
            }
         }

         pb->bytes_left = pb->fm->data_size - aligned_bytes;

         if (pb->rb != NULL)
         {
            hrmp_ringbuffer_reset(pb->rb);
            prefill_ringbuffer(f, pb->rb, pb->file_size);
         }
         hrmp_alsa_reset_handle(pb->pcm_handle);
      }
      else if (pb->fm->type == TYPE_DFF)
      {
         free(k);
         return 1;
      }
      else if (pb->fm->type != TYPE_MKV)
      {
         if (new_pos_samples >= (int64_t)pb->fm->total_samples)
         {
            sf_seek(sndf, 0, SEEK_END);
            pb->current_samples = pb->fm->total_samples;
         }
         else if (new_pos_samples <= 0)
         {
            sf_seek(sndf, 0, SEEK_SET);
            pb->current_samples = 0;
         }
         else
         {
            sf_seek(sndf, (sf_count_t)new_pos_samples, SEEK_CUR);
            pb->current_samples = (unsigned long)new_pos_samples;
            if (pb->current_samples >= pb->fm->total_samples)
            {
               pb->current_samples = pb->fm->total_samples;
            }
         }

         hrmp_alsa_reset_handle(pb->pcm_handle);
      }
      else
      {
         if (new_pos_samples < 0)
         {
            new_pos_samples = 0;
         }
         if (pb->fm->total_samples > 0 && (uint64_t)new_pos_samples > (uint64_t)pb->fm->total_samples)
         {
            new_pos_samples = (int64_t)pb->fm->total_samples;
         }
         pb->current_samples = (unsigned long)new_pos_samples;
         if (pb->current_samples >= pb->fm->total_samples)
         {
            pb->current_samples = pb->fm->total_samples;
         }
         free(k);
         return 3;
      }
   }
   else if (keyboard_action == KEYBOARD_COMMA)
   {
      if (config->active_device.has_volume)
      {
         int new_volume = config->volume - 5;

         if (!config->is_muted)
         {
            if (new_volume < 0)
            {
               new_volume = 0;
            }

            hrmp_alsa_set_volume(new_volume);

            if (config->developer)
            {
               k = hrmp_append(k, " Volume: ");
               k = hrmp_append_int(k, new_volume);
            }
         }
      }
   }
   else if (keyboard_action == KEYBOARD_PERIOD)
   {
      if (config->active_device.has_volume)
      {
         int new_volume = config->volume + 5;

         if (!config->is_muted)
         {
            if (new_volume > 100)
            {
               new_volume = 100;
            }

            hrmp_alsa_set_volume(new_volume);

            if (config->developer)
            {
               k = hrmp_append(k, " Volume: ");
               k = hrmp_append_int(k, new_volume);
            }
         }
      }
   }
   else if (keyboard_action == KEYBOARD_M)
   {
      if (config->active_device.has_volume)
      {
         int new_volume;

         if (config->is_muted)
         {
            new_volume = config->prev_volume;
            config->is_muted = false;

            if (config->developer)
            {
               k = hrmp_append(k, " Volume: ");
               k = hrmp_append_int(k, new_volume);
            }
         }
         else
         {
            new_volume = 0;
            config->is_muted = true;

            if (config->developer)
            {
               k = hrmp_append(k, " Volume: 0");
            }
         }

         hrmp_alsa_set_volume(new_volume);
      }
   }
   else if (keyboard_action == KEYBOARD_SLASH)
   {
      if (config->active_device.has_volume)
      {
         int new_volume = 100;

         config->is_muted = false;
         hrmp_alsa_set_volume(new_volume);

         if (config->developer)
         {
            k = hrmp_append(k, " Volume: 100");
         }
      }
   }
   else
   {
      if (config->active_device.is_paused)
      {
         SLEEP_AND_GOTO(10000L, keyboard);
      }
   }

   *print = k;

   return 0;
}
