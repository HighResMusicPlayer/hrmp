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
#include <devices.h>
#include <files.h>
#include <logging.h>
#include <mkv.h>
#include <utils.h>

/* system */
#include <sndfile.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <alsa/asoundlib.h>

static int init_metadata(char* filename, int type, struct file_metadata** file_metadata);
static int get_metadata(char* filename, int type, struct file_metadata** file_metadata);
static int get_metadata_dsf(int device, char* filename, struct file_metadata** file_metadata);
static int get_metadata_dff(int device, char* filename, struct file_metadata** file_metadata);
static int get_metadata_mkv(int device, char* filename, struct file_metadata** file_metadata);
static bool metadata_supported(int device, struct file_metadata* fm);

static uint32_t id3_be_u32(const unsigned char b[4]);
static uint32_t id3_synchsafe32(const unsigned char b[4]);
static void id3_copy_text_utf8(char* dst, size_t dstsz, const unsigned char* data, size_t len);
static void id3_assign_text_frame(struct file_metadata* fm, const char id[4], const unsigned char* payload, uint32_t size);
static void parse_id3v2(FILE* f, uint64_t offset, struct file_metadata* fm);

static uint32_t
id3_synchsafe32(const unsigned char b[4])
{
   return ((uint32_t)(b[0] & 0x7F) << 21) | ((uint32_t)(b[1] & 0x7F) << 14) |
          ((uint32_t)(b[2] & 0x7F) << 7) | ((uint32_t)(b[3] & 0x7F));
}

static uint32_t
id3_be_u32(const unsigned char b[4])
{
   return ((uint32_t)b[0] << 24) |
          ((uint32_t)b[1] << 16) |
          ((uint32_t)b[2] << 8) |
          ((uint32_t)b[3]);
}

static void
id3_copy_text_utf8(char* dst, size_t dstsz, const unsigned char* data, size_t len)
{
   if (!dst || dstsz == 0)
   {
      return;
   }
   dst[0] = '\0';
   if (!data || len == 0)
   {
      return;
   }

   unsigned enc = data[0];
   const unsigned char* p = data + 1;
   size_t n = (len > 0) ? (len - 1) : 0;

   while (n > 0 && p[n - 1] == '\0')
   {
      n--;
   }

   if (enc == 3) /* UTF-8 */
   {
      size_t cpy = n < (dstsz - 1) ? n : (dstsz - 1);
      memcpy(dst, p, cpy);
      dst[cpy] = '\0';
      return;
   }
   else if (enc == 0) /* ISO-8859-1 -> UTF-8 */
   {
      size_t out = 0;
      for (size_t i = 0; i < n && out + 1 < dstsz; i++)
      {
         unsigned char c = p[i];
         if (c < 0x80)
         {
            dst[out++] = (char)c;
         }
         else
         {
            if (out + 2 >= dstsz)
            {
               break;
            }
            dst[out++] = (char)(0xC0 | (c >> 6));
            dst[out++] = (char)(0x80 | (c & 0x3F));
         }
      }
      if (out < dstsz)
      {
         dst[out] = '\0';
      }
      else
      {
         dst[dstsz - 1] = '\0';
      }
      return;
   }
   else
   {
      size_t out = 0;
      if (enc == 1 && n >= 2 && ((p[0] == 0xFF && p[1] == 0xFE) || (p[0] == 0xFE && p[1] == 0xFF)))
      {
         p += 2;
         n -= 2;
      }
      for (size_t i = 0; i + 1 < n && out + 1 < dstsz; i += 2)
      {
         unsigned char lo = p[i + 1];
         unsigned char hi = p[i];
         unsigned char ascii = lo ? lo : hi;
         if (ascii == 0)
         {
            continue;
         }
         dst[out++] = (char)ascii;
      }
      if (out < dstsz)
      {
         dst[out] = '\0';
      }
      else
      {
         dst[dstsz - 1] = '\0';
      }
      return;
   }
}

static void
id3_assign_text_frame(struct file_metadata* fm, const char id[4], const unsigned char* payload, uint32_t size)
{
   if (fm == NULL || payload == NULL || size == 0)
   {
      return;
   }

   char buf[512] = {0};
   id3_copy_text_utf8(buf, sizeof(buf), payload, size);

   if (strcmp(id, "TIT2") == 0)
   {
      strncpy(fm->title, buf, sizeof(fm->title) - 1);
      fm->title[sizeof(fm->title) - 1] = '\0';
   }
   else if (strcmp(id, "TPE1") == 0)
   {
      strncpy(fm->artist, buf, sizeof(fm->artist) - 1);
      fm->artist[sizeof(fm->artist) - 1] = '\0';
   }
   else if (strcmp(id, "TALB") == 0)
   {
      strncpy(fm->album, buf, sizeof(fm->album) - 1);
      fm->album[sizeof(fm->album) - 1] = '\0';
   }
   else if (strcmp(id, "TCON") == 0)
   {
      strncpy(fm->genre, buf, sizeof(fm->genre) - 1);
      fm->genre[sizeof(fm->genre) - 1] = '\0';
   }
   else if (strcmp(id, "TYER") == 0 || strcmp(id, "TDRC") == 0)
   {
      strncpy(fm->date, buf, sizeof(fm->date) - 1);
      fm->date[sizeof(fm->date) - 1] = '\0';
   }
   else if (strcmp(id, "TRCK") == 0)
   {
      int track = 0;
      if (sscanf(buf, "%d", &track) == 1 && track > 0)
      {
         fm->track = track;
      }
   }
   else if (strcmp(id, "TPOS") == 0)
   {
      int disc = 0;
      if (sscanf(buf, "%d", &disc) == 1 && disc > 0)
      {
         fm->disc = disc;
      }
   }
}

static void
parse_id3v2(FILE* f, uint64_t offset, struct file_metadata* fm)
{
   if (f == NULL || fm == NULL)
   {
      return;
   }

   long save_pos = ftell(f);
   if (fseek(f, (long)offset, SEEK_SET) != 0)
   {
      return;
   }

   unsigned char hdr[10];
   if (fread(hdr, 1, 10, f) != 10)
   {
      goto done;
   }

   if (!(hdr[0] == 'I' && hdr[1] == 'D' && hdr[2] == '3'))
   {
      goto done;
   }

   unsigned ver_major = hdr[3];
   unsigned flags = hdr[5];
   uint32_t tag_size = id3_synchsafe32(&hdr[6]);
   uint32_t bytes_read = 0;

   if (flags & 0x40)
   {
      unsigned char ex[4];
      if (fread(ex, 1, 4, f) != 4)
      {
         goto done;
      }
      uint32_t ex_size = (ver_major >= 4) ? id3_synchsafe32(ex) : id3_be_u32(ex);
      if (ver_major >= 4)
      {
         if (fseek(f, (long)ex_size, SEEK_CUR) != 0)
         {
            goto done;
         }
         bytes_read += 4 + ex_size;
      }
      else
      {
         uint32_t remaining = (ex_size >= 4) ? (ex_size - 4) : 0;
         if (fseek(f, (long)remaining, SEEK_CUR) != 0)
         {
            goto done;
         }
         bytes_read += ex_size;
      }
   }

   while (bytes_read + 10 <= tag_size)
   {
      unsigned char fh[10];
      if (fread(fh, 1, 10, f) != 10)
      {
         break;
      }

      char frame_id[5] = {0};
      memcpy(frame_id, fh, 4);
      if (frame_id[0] == 0)
      {
         break;
      }
      uint32_t frame_size = (ver_major >= 4) ? id3_synchsafe32(&fh[4]) : id3_be_u32(&fh[4]);
      bytes_read += 10;

      if (frame_size == 0 || bytes_read + frame_size > tag_size)
      {
         break;
      }

      unsigned char* payload = (unsigned char*)malloc(frame_size);
      if (payload == NULL)
      {
         break;
      }

      size_t got = fread(payload, 1, frame_size, f);
      if (got != frame_size)
      {
         free(payload);
         break;
      }

      if (frame_id[0] == 'T')
      {
         id3_assign_text_frame(fm, frame_id, payload, frame_size);
      }

      free(payload);
      bytes_read += frame_size;
   }

done:
   if (save_pos >= 0)
   {
      fseek(f, save_pos, SEEK_SET);
   }
}

int
hrmp_file_metadata(int device, char* f, struct file_metadata** fm)
{
   int type = TYPE_UNKNOWN;
   struct file_metadata* m = NULL;
   struct configuration* config = NULL;

   config = (struct configuration*)shmem;

   *fm = NULL;

   if (hrmp_ends_with(f, ".wav"))
   {
      type = TYPE_WAV;
   }
   else if (hrmp_ends_with(f, ".flac"))
   {
      type = TYPE_FLAC;
   }
   else if (hrmp_ends_with(f, ".mp3"))
   {
      type = TYPE_MP3;
   }
   else if (hrmp_ends_with(f, ".dsf"))
   {
      type = TYPE_DSF;
   }
   else if (hrmp_ends_with(f, ".dff"))
   {
      type = TYPE_DFF;
   }
   else if (hrmp_ends_with(f, ".mkv") || hrmp_ends_with(f, ".mka") || hrmp_ends_with(f, ".webm"))
   {
      type = TYPE_MKV;
   }

   if (type == TYPE_WAV || type == TYPE_FLAC || type == TYPE_MP3)
   {
      if (get_metadata(f, type, &m))
      {
         if (!config->quiet)
         {
            printf("Unsupported metadata for %s\n", f);
         }
         goto error;
      }
   }
   else if (type == TYPE_DSF)
   {
      if (get_metadata_dsf(device, f, &m))
      {
         if (!config->quiet)
         {
            printf("Unsupported metadata for %s\n", f);
         }
         goto error;
      }
   }
   else if (type == TYPE_DFF)
   {
      if (get_metadata_dff(device, f, &m))
      {
         if (!config->quiet)
         {
            printf("Unsupported metadata for %s\n", f);
         }
         goto error;
      }
   }
   else if (type == TYPE_MKV)
   {
      if (get_metadata_mkv(device, f, &m))
      {
         if (!config->quiet)
         {
            printf("Unsupported metadata for %s\n", f);
         }
         goto error;
      }
   }
   else
   {
      if (!config->quiet)
      {
         printf("Unsupported file extension for %s\n", f);
      }
      goto error;
   }

   if (!metadata_supported(device, m))
   {
      if (!config->quiet)
      {
         printf("Unsupported file: %s/ch%d/%dHz/%dbits\n", f, m->channels,
                m->sample_rate, m->bits_per_sample);
      }
      goto error;
   }

   *fm = m;

   return 0;

error:

   free(m);

   return 1;
}

static bool
metadata_supported(int device, struct file_metadata* fm)
{
   struct configuration* config = NULL;

   config = (struct configuration*)shmem;

   if (device >= 0 && fm != NULL)
   {
      if (fm->channels == 0 || fm->channels > 6)
      {
         return false;
      }

      if (fm->bits_per_sample == 16)
      {
         if (config->devices[device].capabilities.s16_le)
         {
            switch (fm->sample_rate)
            {
               case 44100:
               case 48000:
               case 88200:
               case 96000:
               case 176400:
               case 192000:
               case 352800:
               case 384000:
                  return true;
                  break;
               default:
                  if (config->experimental)
                  {
                     switch (fm->sample_rate)
                     {
                        case 705600:
                        case 768000:
                           return true;
                           break;
                        default:
                           break;
                     }
                  }
            }
         }
      }
      else if (fm->bits_per_sample == 24)
      {
         if (config->devices[device].capabilities.s24_3le ||
             config->devices[device].capabilities.s32_le)
         {
            switch (fm->sample_rate)
            {
               case 44100:
               case 48000:
               case 88200:
               case 96000:
               case 176400:
               case 192000:
               case 352800:
               case 384000:
                  return true;
                  break;
               default:
                  if (config->experimental)
                  {
                     switch (fm->sample_rate)
                     {
                        case 705600:
                        case 768000:
                           return true;
                           break;
                        default:
                           break;
                     }
                  }
            }
         }
      }
      else if (fm->bits_per_sample == 32)
      {
         if (config->devices[device].capabilities.s24_3le ||
             config->devices[device].capabilities.s32_le)
         {
            if (fm->type == TYPE_FLAC)
            {
               return false;
            }

            switch (fm->sample_rate)
            {
               case 44100:
               case 48000:
               case 88200:
               case 96000:
               case 176400:
               case 192000:
               case 352800:
               case 384000:
                  return true;
                  break;
               default:
                  if (config->experimental)
                  {
                     switch (fm->sample_rate)
                     {
                        case 705600:
                        case 768000:
                           return true;
                           break;
                        default:
                           break;
                     }
                  }
            }
         }
      }
      else if (fm->bits_per_sample == 1)
      {
         if (config->devices[device].capabilities.dsd_u8 ||
             config->devices[device].capabilities.dsd_u16_le ||
             config->devices[device].capabilities.dsd_u16_be ||
             config->devices[device].capabilities.dsd_u32_le ||
             config->devices[device].capabilities.dsd_u32_be)
         {
            switch (fm->sample_rate)
            {
               case 2822400:
               case 5644800:
               case 11289600:
                  return true;
                  break;
               case 22579200:
                  if (config->dop)
                  {
                     return false;
                  }
                  else
                  {
                     return true;
                  }
                  break;
               default:
                  if (config->experimental)
                  {
                     switch (fm->sample_rate)
                     {
                        case 45158400:
                           return true;
                           break;
                        default:
                           break;
                     }
                  }
                  break;
            }
         }
      }
   }

   return false;
}

int
hrmp_print_file_metadata(struct file_metadata* fm)
{
   if (fm != NULL)
   {
      printf("%s\n", fm->name);
      if (fm->type == TYPE_UNKNOWN)
      {
         printf("  Type: TYPE_UNKNOWN\n");
      }
      else if (fm->type == TYPE_WAV)
      {
         printf("  Type: TYPE_WAV\n");
      }
      else if (fm->type == TYPE_FLAC)
      {
         printf("  Type: TYPE_FLAC\n");
      }
      else if (fm->type == TYPE_MP3)
      {
         printf("  Type: TYPE_MP3\n");
      }
      else if (fm->type == TYPE_DSF)
      {
         printf("  Type: TYPE_DSF\n");
      }
      else if (fm->type == TYPE_DFF)
      {
         printf("  Type: TYPE_DFF\n");
      }
      else if (fm->type == TYPE_MKV)
      {
         printf("  Type: TYPE_MKV\n");
      }
      else
      {
         printf("  Type: TYPE_UNKNOWN (%d)\n", fm->type);
      }
      if (fm->format == FORMAT_1)
      {
         printf("  Format: FORMAT_1\n");
      }
      else if (fm->format == FORMAT_16)
      {
         printf("  Format: FORMAT_16\n");
      }
      else if (fm->format == FORMAT_24)
      {
         printf("  Format: FORMAT_24\n");
      }
      else if (fm->format == FORMAT_32)
      {
         printf("  Format: FORMAT_32\n");
      }
      else
      {
         printf("  Format: FORMAT_UNKNOWN (%d)\n", fm->format);
      }
      printf("  Bits: %d\n", fm->bits_per_sample);
      printf("  Container: %d\n", fm->container);
      printf("  Channels: %d\n", fm->channels);
      printf("  Size: %zu\n", fm->file_size);
      printf("  Rate: %d Hz\n", fm->sample_rate);
      printf("  PCM: %d Hz\n", fm->pcm_rate);

      if (fm->alsa_snd == SND_PCM_FORMAT_S16)
      {
         printf("  ALSA: SND_PCM_FORMAT_S16\n");
      }
      else if (fm->alsa_snd == SND_PCM_FORMAT_S16_LE)
      {
         printf("  ALSA: SND_PCM_FORMAT_S16_LE\n");
      }
      else if (fm->alsa_snd == SND_PCM_FORMAT_S16_BE)
      {
         printf("  ALSA: SND_PCM_FORMAT_S16_BE\n");
      }
      else if (fm->alsa_snd == SND_PCM_FORMAT_S24)
      {
         printf("  ALSA: SND_PCM_FORMAT_S24\n");
      }
      else if (fm->alsa_snd == SND_PCM_FORMAT_S24_3LE)
      {
         printf("  ALSA: SND_PCM_FORMAT_S24_3LE\n");
      }
      else if (fm->alsa_snd == SND_PCM_FORMAT_S24_LE)
      {
         printf("  ALSA: SND_PCM_FORMAT_S24_LE\n");
      }
      else if (fm->alsa_snd == SND_PCM_FORMAT_S24_3BE)
      {
         printf("  ALSA: SND_PCM_FORMAT_S24_3BE\n");
      }
      else if (fm->alsa_snd == SND_PCM_FORMAT_S24_BE)
      {
         printf("  ALSA: SND_PCM_FORMAT_S24_BE\n");
      }
      else if (fm->alsa_snd == SND_PCM_FORMAT_S32)
      {
         printf("  ALSA: SND_PCM_FORMAT_S32\n");
      }
      else if (fm->alsa_snd == SND_PCM_FORMAT_S32_LE)
      {
         printf("  ALSA: SND_PCM_FORMAT_S32_LE\n");
      }
      else if (fm->alsa_snd == SND_PCM_FORMAT_S32_BE)
      {
         printf("  ALSA: SND_PCM_FORMAT_S32_BE\n");
      }
      else if (fm->alsa_snd == SND_PCM_FORMAT_DSD_U8)
      {
         printf("  ALSA: SND_PCM_FORMAT_DSD_U8\n");
      }
      else if (fm->alsa_snd == SND_PCM_FORMAT_DSD_U16_LE)
      {
         printf("  ALSA: SND_PCM_FORMAT_DSD_U16_LE\n");
      }
      else if (fm->alsa_snd == SND_PCM_FORMAT_DSD_U32_LE)
      {
         printf("  ALSA: SND_PCM_FORMAT_DSD_U32_LE\n");
      }
      else if (fm->alsa_snd == SND_PCM_FORMAT_DSD_U16_BE)
      {
         printf("  ALSA: SND_PCM_FORMAT_DSD_U16_BE\n");
      }
      else if (fm->alsa_snd == SND_PCM_FORMAT_DSD_U32_BE)
      {
         printf("  ALSA: SND_PCM_FORMAT_DSD_U32_BE\n");
      }
      else
      {
         printf("  ALSA: UNKNOWN (%d)\n", fm->alsa_snd);
      }
      printf("  Samples: %lu\n", fm->total_samples);
      printf("  Duration: %lf\n", fm->duration);
      printf("  Block size: %d\n", fm->block_size);
      printf("  Data size: %lu\n", fm->data_size);

      if (strlen(fm->title) > 0)
      {
         printf("  Title: %s\n", fm->title);
      }
      if (strlen(fm->artist) > 0)
      {
         printf("  Artist: %s\n", fm->artist);
      }
      if (strlen(fm->album) > 0)
      {
         printf("  Album: %s\n", fm->album);
      }
      if (strlen(fm->genre) > 0)
      {
         printf("  Genre: %s\n", fm->genre);
      }
      if (strlen(fm->date) > 0)
      {
         printf("  Date: %s\n", fm->date);
      }
      if (fm->track > 0)
      {
         printf("  Track: %d\n", fm->track);
      }
      if (fm->disc > 0)
      {
         printf("  Disc: %d\n", fm->disc);
      }
   }

   return 0;
}

static int
init_metadata(char* filename, int type, struct file_metadata** file_metadata)
{
   struct file_metadata* fm = NULL;

   *file_metadata = NULL;

   fm = (struct file_metadata*)malloc(sizeof(struct file_metadata));
   if (fm == NULL)
   {
      goto error;
   }
   memset(fm, 0, sizeof(struct file_metadata));

   fm->type = type;
   memcpy(fm->name, filename, strlen(filename));
   fm->format = FORMAT_UNKNOWN;
   fm->file_size = hrmp_get_file_size(filename);
   fm->sample_rate = 0;
   fm->pcm_rate = 0;
   fm->channels = 0;
   fm->bits_per_sample = 0;
   fm->total_samples = 0;
   fm->duration = 0.0;
   fm->alsa_snd = 0;

   *file_metadata = fm;

   return 0;

error:

   free(fm);

   return 1;
}

static int
get_metadata(char* filename, int type, struct file_metadata** file_metadata)
{
   const char* s;
   SNDFILE* f = NULL;
   SF_INFO* info = NULL;
   struct file_metadata* fm = NULL;
   struct configuration* config = NULL;

   *file_metadata = NULL;

   config = (struct configuration*)shmem;

   if (init_metadata(filename, type, &fm))
   {
      goto error;
   }

   if (type == TYPE_WAV || type == TYPE_FLAC || type == TYPE_MP3)
   {
      info = (SF_INFO*)malloc(sizeof(SF_INFO));
      if (info == NULL)
      {
         goto error;
      }
      memset(info, 0, sizeof(SF_INFO));

      f = sf_open(filename, SFM_READ, info);
      if (f == NULL)
      {
         printf("%s (Unsupported due to %s)\n", filename, sf_strerror(f));
         goto error;
      }

      if (config->developer)
      {
         /* https://libsndfile.github.io/libsndfile/api.html */
         hrmp_log_debug("Info format: %X", info->format);
      }

      if (type == TYPE_WAV)
      {
         if (!(info->format & SF_FORMAT_WAV))
         {
            goto error;
         }
      }
      else if (type == TYPE_FLAC)
      {
         if (!(info->format & SF_FORMAT_FLAC))
         {
            goto error;
         }
      }
      else if (type == TYPE_MP3)
      {
         fm->format = FORMAT_16;
         fm->bits_per_sample = 16;

         if (!(info->format & SF_FORMAT_MPEG_LAYER_III))
         {
            goto error;
         }
      }
      else
      {
         goto error;
      }

      fm->sample_rate = info->samplerate;
      fm->pcm_rate = info->samplerate;
      fm->channels = info->channels;

      if (fm->channels == 0 || fm->channels > 6)
      {
         if (config->experimental)
         {
            printf("Unsupported number of channels for '%s' (%d channels)\n", filename, fm->channels);
         }
         goto error;
      }

      if ((info->format & 0xFF) == SF_FORMAT_PCM_16)
      {
         fm->format = FORMAT_16;
         fm->bits_per_sample = 16;
      }
      else if ((info->format & 0xFF) == SF_FORMAT_PCM_24)
      {
         fm->format = FORMAT_24;
         fm->bits_per_sample = 24;
      }
      else if ((info->format & 0xFF) == SF_FORMAT_PCM_32)
      {
         fm->format = FORMAT_32;
         fm->bits_per_sample = 32;
      }

      fm->total_samples = info->frames;
      if (fm->sample_rate > 0)
      {
         fm->duration = (double)((double)fm->total_samples / fm->sample_rate);
      }
      else
      {
         fm->duration = 0.0;
      }

      s = sf_get_string(f, SF_STR_TITLE);
      if (s != NULL && strlen(s) > 0)
      {
         memcpy(fm->title, s, strlen(s));
      }
      s = sf_get_string(f, SF_STR_ARTIST);
      if (s != NULL && strlen(s) > 0)
      {
         memcpy(fm->artist, s, strlen(s));
      }
      s = sf_get_string(f, SF_STR_ALBUM);
      if (s != NULL && strlen(s) > 0)
      {
         memcpy(fm->album, s, strlen(s));
      }
      s = sf_get_string(f, SF_STR_GENRE);
      if (s != NULL && strlen(s) > 0)
      {
         memcpy(fm->genre, s, strlen(s));
      }
      s = sf_get_string(f, SF_STR_DATE);
      if (s != NULL && strlen(s) > 0)
      {
         memcpy(fm->date, s, strlen(s));
      }
   }
   fm->file_size = hrmp_get_file_size(filename);

   *file_metadata = fm;

   if (f != NULL)
   {
      sf_close(f);
   }

   free(info);

   return 0;

error:

   if (f != NULL)
   {
      sf_close(f);
   }

   free(info);
   free(fm);

   return 1;
}

static int
get_metadata_dsf(int device, char* filename, struct file_metadata** file_metadata)
{
   FILE* f = NULL;
   char id4[5] = {0};
   uint32_t channel_number = 0;
   uint32_t srate = 0;
   uint32_t bps = 0;
   uint64_t samples = 0;
   uint32_t block_size = 0;
   uint64_t data_size = 0;
   uint64_t metadata_chunk = 0;
   struct file_metadata* fm = NULL;
   struct configuration* config = NULL;

   config = (struct configuration*)shmem;

   *file_metadata = NULL;

   if (init_metadata(filename, TYPE_DSF, &fm))
   {
      goto error;
   }

   f = fopen(filename, "rb");
   if (f == NULL)
   {
      hrmp_log_error("fopen input: %s", filename);
      goto error;
   }

   memset(&id4[0], 0, sizeof(id4));
   if (fread(id4, 1, 4, f) != 4)
   {
      hrmp_log_error("Failed to read file id\n");
      goto error;
   }

   if (strncmp(id4, "DSD ", 4) != 0)
   {
      hrmp_log_error("Not a DSF file (missing 'DSD ' header). Read: '%.4s'\n", id4);
      goto error;
   }

   hrmp_read_le_u64(f); /* chunk_size */
   hrmp_read_le_u64(f); /* file_size */
   metadata_chunk = hrmp_read_le_u64(f); /* metadata_chunk */

   memset(&id4[0], 0, sizeof(id4));
   if (fread(id4, 1, 4, f) != 4)
   {
      hrmp_log_error("Failed to read file format\n");
      goto error;
   }

   if (strncmp(id4, "fmt ", 4) != 0)
   {
      hrmp_log_error("Not a format header (missing 'fmt ' header). Read: '%.4s'\n", id4);
      goto error;
   }

   hrmp_read_le_u64(f); /* format_chunk */
   hrmp_read_le_u32(f); /* format_version */
   hrmp_read_le_u32(f); /* format_id */
   hrmp_read_le_u32(f); /* channel_type */
   channel_number = hrmp_read_le_u32(f);
   srate = hrmp_read_le_u32(f);
   bps = hrmp_read_le_u32(f);
   samples = hrmp_read_le_u64(f);
   block_size = hrmp_read_le_u32(f);
   hrmp_read_le_u32(f); /* reserved */

   hrmp_read_le_u32(f); /* DATA */
   data_size = hrmp_read_le_u64(f); /* data_size */

   if (srate % 16)
   {
      hrmp_log_error("Error: DSD sample rate is not divisible by 16 (%lu)", srate);
      goto error;
   }

   fm->format = FORMAT_1;
   fm->file_size = hrmp_get_file_size(filename);
   fm->sample_rate = srate;
   if (config->dop && (config->devices[device].capabilities.s32 ||
                       config->devices[device].capabilities.s32_le))
   {
      fm->pcm_rate = srate / 16;
   }
   else
   {
      fm->pcm_rate = srate / 32;
   }
   fm->channels = channel_number;
   fm->bits_per_sample = bps;
   fm->total_samples = samples;
   fm->duration = (double)((double)samples / srate);
   fm->block_size = block_size;
   fm->data_size = data_size;

   if (metadata_chunk != 0)
   {
      parse_id3v2(f, metadata_chunk, fm);
   }

   *file_metadata = fm;

   fclose(f);

   return 0;

error:

   if (f != NULL)
   {
      fclose(f);
   }

   free(fm);

   return 1;
}

static int
get_metadata_dff(int device, char* filename, struct file_metadata** file_metadata)
{
   FILE* f = NULL;
   char id4[5] = {0};
   uint32_t channel_number = 0;
   uint32_t srate = 0;
   uint64_t data_size = 0;
   uint64_t prop_chunk_end = 0;
   bool saw_prop = false;
   bool saw_dsd_data = false;
   bool uncompressed_dsd = false;
   char cmpr_fourcc[5] = {0};
   struct file_metadata* fm = NULL;
   struct configuration* config = NULL;

   config = (struct configuration*)shmem;

   *file_metadata = NULL;

   if (init_metadata(filename, TYPE_DFF, &fm))
   {
      goto error;
   }

   f = fopen(filename, "rb");
   if (f == NULL)
   {
      hrmp_log_error("fopen input: %s", filename);
      goto error;
   }

   /* 'FRM8' chunk */
   if (fread(id4, 1, 4, f) != 4)
   {
      hrmp_log_error("Failed to read file id\n");
      goto error;
   }
   if (strncmp(id4, "FRM8", 4) != 0)
   {
      hrmp_log_error("Not a DFF file (missing 'FRM8'). Read: '%.4s'\n", id4);
      goto error;
   }

   /* 64-bit BE size of the FORM; skip */
   hrmp_read_be_u64(f);

   /* Form type must be 'DSD ' */
   if (fread(id4, 1, 4, f) != 4)
   {
      hrmp_log_error("Failed to read form type\n");
      goto error;
   }

   if (strncmp(id4, "DSD ", 4) != 0)
   {
      hrmp_log_error("Not a DSD form type in DFF. Read: '%.4s'\n", id4);
      goto error;
   }

   /* Iterate chunks to find PROP/SND info and DSD data */
   while (fread(id4, 1, 4, f) == 4)
   {
      uint64_t chunk_size = hrmp_read_be_u64(f);
      long chunk_data_start = ftell(f);

      if (strncmp(id4, "PROP", 4) == 0)
      {
         char snd[4] = {0};
         if (fread(snd, 1, 4, f) != 4)
         {
            hrmp_log_error("Failed to read PROP type\n");
            goto error;
         }
         if (strncmp(snd, "SND ", 4) != 0)
         {
            hrmp_log_error("Unsupported PROP type '%.4s'\n", snd);
            goto error;
         }

         prop_chunk_end = (uint64_t)chunk_data_start + chunk_size;
         saw_prop = true;

         /* Parse sub-chunks inside PROP */
         while ((uint64_t)ftell(f) + 12 <= prop_chunk_end)
         {
            char pid[5] = {0};
            uint64_t psize = 0;
            if (fread(pid, 1, 4, f) != 4)
            {
               break;
            }
            psize = hrmp_read_be_u64(f);

            if (strncmp(pid, "FS  ", 4) == 0 && psize == 4)
            {
               srate = hrmp_read_be_u32(f);
            }
            else if (strncmp(pid, "CHNL", 4) == 0 && psize >= 2)
            {
               uint16_t chn = hrmp_read_be_u16(f);
               uint64_t expected_min = 2ULL + 4ULL * (uint64_t)chn;
               if (psize < expected_min)
               {
                  hrmp_log_error("Invalid CHNL size (psize=%" PRIu64 ", expected_min=%" PRIu64 ", ch=%u)",
                                 psize, expected_min, (unsigned)chn);
                  goto error;
               }

               channel_number = (uint32_t)chn;

               if (fseek(f, (long)(psize - 2ULL), SEEK_CUR) != 0)
               {
                  hrmp_log_error("fseek failed (CHNL skip)\n");
                  goto error;
               }
            }
            else if (strncmp(pid, "CMPR", 4) == 0 && psize >= 4)
            {
               char ctype[5] = {0};
               if (fread(ctype, 1, 4, f) != 4)
               {
                  hrmp_log_error("Failed to read CMPR type\n");
                  goto error;
               }
               uncompressed_dsd = (strncmp(ctype, "DSD ", 4) == 0);
               memcpy(cmpr_fourcc, ctype, 4);
               cmpr_fourcc[4] = '\0';

               /* skip the rest of CMPR (name string), if any */
               if (psize > 4)
               {
                  if (fseek(f, (long)(psize - 4), SEEK_CUR) != 0)
                  {
                     hrmp_log_error("fseek failed (CMPR)\n");
                     goto error;
                  }
               }
            }
            else
            {
               /* skip unknown PROP subchunk */
               if (fseek(f, (long)psize, SEEK_CUR) != 0)
               {
                  hrmp_log_error("fseek failed (PROP sub)\n");
                  goto error;
               }
            }
         }

         /* Seek to end of PROP if not already */
         if (fseek(f, (long)prop_chunk_end, SEEK_SET) != 0)
         {
            hrmp_log_error("fseek failed (end PROP)\n");
            goto error;
         }
      }
      else if (strncmp(id4, "DSD ", 4) == 0)
      {
         data_size = chunk_size;
         saw_dsd_data = true;

         if (fseek(f, (long)chunk_size, SEEK_CUR) != 0)
         {
            hrmp_log_error("fseek failed (skip DSD data)\n");
            goto error;
         }
      }
      else
      {
         /* Skip other chunks */
         if (fseek(f, (long)chunk_size, SEEK_CUR) != 0)
         {
            hrmp_log_error("fseek failed (skip chunk '%.4s')\n", id4);
            goto error;
         }
      }
   }

   if (!saw_prop || !saw_dsd_data || !uncompressed_dsd || srate == 0 || channel_number == 0)
   {
      if (!uncompressed_dsd && cmpr_fourcc[0] != '\0')
      {
         hrmp_log_error("Unsupported DFF compression '%.4s' (only 'DSD ' uncompressed is supported)",
                        cmpr_fourcc);
      }
      else
      {
         hrmp_log_error("Incomplete or unsupported DFF (PROP/DSD/CMPR/FS/CHNL)\n");
      }
      goto error;
   }

   if (srate % 16)
   {
      hrmp_log_error("Error: DSD sample rate is not divisible by 16 (%u)", srate);
      goto error;
   }

   fm->format = FORMAT_1;
   fm->file_size = hrmp_get_file_size(filename);
   fm->sample_rate = srate;
   fm->channels = (int)channel_number;
   fm->bits_per_sample = 1;

   if (channel_number > 0)
   {
      uint64_t total_bits = data_size * 8ULL;
      fm->total_samples = (uint64_t)(total_bits / channel_number);
      if (fm->sample_rate > 0)
      {
         fm->duration = (double)((double)fm->total_samples / fm->sample_rate);
      }
      else
      {
         fm->duration = 0.0;
      }
   }

   if (config->dop && (config->devices[device].capabilities.s32 ||
                       config->devices[device].capabilities.s32_le))
   {
      fm->pcm_rate = srate / 16;
   }
   else
   {
      fm->pcm_rate = 0;
   }

   fm->data_size = (unsigned long)data_size;
   fm->block_size = 0;

   *file_metadata = fm;

   fclose(f);
   return 0;

error:
   if (f != NULL)
   {
      fclose(f);
   }
   free(fm);
   return 1;
}

static int
get_metadata_mkv(int device, char* filename, struct file_metadata** file_metadata)
{
   struct file_metadata* fm = NULL;
   MkvDemuxer* demux = NULL;
   MkvAudioInfo ai;
   uint64_t total_bytes = 0;

   *file_metadata = NULL;

   if (init_metadata(filename, TYPE_MKV, &fm))
   {
      goto error;
   }

   if (hrmp_mkv_open_path(filename, &demux) < 0 || !demux)
   {
      hrmp_log_error("MKV: failed to open '%s'", filename);
      goto error;
   }
   if (hrmp_mkv_get_audio_info(demux, &ai) < 0)
   {
      hrmp_log_error("MKV: failed to read audio info '%s'", filename);
      goto error;
   }

   /* Accept PCM INT in MKV directly, Opus and AAC decoded to PCM by mkv.c */
   if (ai.codec == MKV_CODEC_PCM_INT)
   {
      fm->channels = (unsigned)ai.channels;
      fm->sample_rate = (unsigned)(ai.sample_rate > 0 ? (unsigned)(ai.sample_rate + 0.5) : 0);
      fm->bits_per_sample = ai.bit_depth;
      if (fm->bits_per_sample == 16)
      {
         fm->format = FORMAT_16;
      }
      else if (fm->bits_per_sample == 24)
      {
         fm->format = FORMAT_24;
      }
      else if (fm->bits_per_sample == 32)
      {
         fm->format = FORMAT_32;
      }
      else
      {
         hrmp_log_error("MKV: unsupported PCM bit depth: %u", fm->bits_per_sample);
         goto error;
      }
   }
   else if (ai.codec == MKV_CODEC_OPUS)
   {
      /* Decoded S16LE @ 48kHz */
      fm->channels = (unsigned)(ai.channels ? ai.channels : 2);
      fm->sample_rate = 48000;
      fm->bits_per_sample = 16;
      fm->format = FORMAT_16;
   }
   else if (ai.codec == MKV_CODEC_AAC)
   {
      /* Decoded S16LE; sample rate/channels from track info (or decoder) */
      fm->channels = (unsigned)(ai.channels ? ai.channels : 2);
      fm->sample_rate = (unsigned)(ai.sample_rate > 0 ? (unsigned)(ai.sample_rate + 0.5) : 48000);
      fm->bits_per_sample = 16;
      fm->format = FORMAT_16;
   }
   else
   {
      hrmp_log_error("MKV: unsupported codec '%s' (PCM/Opus/AAC supported)", ai.codec_id_str);
      goto error;
   }

   if (fm->channels == 0 || fm->channels > 6)
   {
      hrmp_log_error("MKV: unsupported number of channels (%u), only 1..6 supported", fm->channels);
      goto error;
   }

   fm->pcm_rate = fm->sample_rate;

   /* Pre-scan to compute duration and total samples (post-decode for Opus/AAC). */
   while (1)
   {
      MkvPacket pkt = {0};
      int got = hrmp_mkv_read_packet(demux, &pkt);
      if (got < 0)
      {
         hrmp_log_error("MKV: error during metadata scan");
         goto error;
      }
      if (got == 0)
      {
         break; /* end */
      }
      total_bytes += (uint64_t)pkt.size;
      hrmp_mkv_free_packet(&pkt);
   }

   if (fm->channels > 0 && fm->bits_per_sample > 0)
   {
      unsigned bps8 = fm->bits_per_sample / 8u;
      uint64_t bytes_per_frame = (uint64_t)fm->channels * (uint64_t)bps8;
      if (bytes_per_frame == 0)
      {
         goto error;
      }
      fm->total_samples = (unsigned long)(total_bytes / bytes_per_frame);
      fm->duration = (fm->sample_rate > 0)
                     ? (double)((double)fm->total_samples / (double)fm->sample_rate)
                     : 0.0;
      fm->data_size = (unsigned long)total_bytes;
   }

   fm->file_size = hrmp_get_file_size(filename);

   hrmp_mkv_close(demux);
   demux = NULL;

   *file_metadata = fm;
   return 0;

error:
   if (demux)
   {
      hrmp_mkv_close(demux);
   }
   free(fm);
   return 1;
}
