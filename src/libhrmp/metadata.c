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

#include <files.h>
#include <metadata.h>
#include <sndfile.h>
#include <utils.h>

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <sndfile.h>

static int
parse_number_token(const char* s)
{
   if (!s)
   {
      return -1;
   }
   while (isspace((unsigned char)*s))
   {
      s++;
   }
   if (*s == '\0')
   {
      return -1;
   }
   char* end = NULL;
   long v = strtol(s, &end, 10);
   if (s == end)
   {
      return -1;
   }
   if (v < -2147483648L || v > 2147483647L)
   {
      return -1;
   }
   return (int)v;
}

static int
parse_x_of_y(const char* s)
{
   if (!s)
   {
      return -1;
   }
   char buf[64];
   size_t n = strlen(s);
   if (n >= sizeof(buf))
   {
      n = sizeof(buf) - 1;
   }
   memcpy(buf, s, n);
   buf[n] = '\0';
   char* slash = strchr(buf, '/');
   if (slash)
   {
      *slash = '\0';
   }
   return parse_number_token(buf);
}

static char*
iso_8859_1_to_utf8(const unsigned char* in, size_t len)
{
   size_t out_cap = len * 2 + 1;
   char* out = (char*)malloc(out_cap);
   if (!out)
   {
      return NULL;
   }
   size_t j = 0;
   for (size_t i = 0; i < len; i++)
   {
      unsigned char c = in[i];
      if (c < 0x80)
      {
         out[j++] = (char)c;
      }
      else
      {
         out[j++] = (char)(0xC0 | (c >> 6));
         out[j++] = (char)(0x80 | (c & 0x3F));
      }
   }
   out[j] = '\0';
   return out;
}

static void
utf8_emit(char** out, size_t* j, size_t* cap, unsigned int cp)
{
   if (*j + 4 >= *cap)
   {
      size_t new_cap = (*cap < 64 ? 128 : (*cap * 2));
      char* tmp = (char*)realloc(*out, new_cap);
      if (!tmp)
      {
         return;
      }
      *out = tmp;
      *cap = new_cap;
   }
   if (cp <= 0x7F)
   {
      (*out)[(*j)++] = (char)cp;
   }
   else if (cp <= 0x7FF)
   {
      (*out)[(*j)++] = (char)(0xC0 | (cp >> 6));
      (*out)[(*j)++] = (char)(0x80 | (cp & 0x3F));
   }
   else if (cp <= 0xFFFF)
   {
      (*out)[(*j)++] = (char)(0xE0 | (cp >> 12));
      (*out)[(*j)++] = (char)(0x80 | ((cp >> 6) & 0x3F));
      (*out)[(*j)++] = (char)(0x80 | (cp & 0x3F));
   }
   else
   {
      (*out)[(*j)++] = (char)(0xF0 | (cp >> 18));
      (*out)[(*j)++] = (char)(0x80 | ((cp >> 12) & 0x3F));
      (*out)[(*j)++] = (char)(0x80 | ((cp >> 6) & 0x3F));
      (*out)[(*j)++] = (char)(0x80 | (cp & 0x3F));
   }
}

static char*
utf16_to_utf8(const unsigned char* in, size_t len_bytes, int big_endian)
{
   if (!in || (len_bytes & 1))
   {
      return NULL;
   }
   size_t cap = len_bytes * 2 + 1;
   char* out = (char*)malloc(cap);
   if (!out)
   {
      return NULL;
   }
   size_t j = 0;

   for (size_t i = 0; i + 1 < len_bytes; )
   {
      unsigned int w1 = big_endian ? ((in[i] << 8) | in[i + 1]) : (in[i] | (in[i + 1] << 8));
      i += 2;
      if (w1 >= 0xD800 && w1 <= 0xDBFF)
      {
         if (i + 1 >= len_bytes)
         {
            break;
         }
         unsigned int w2 = big_endian ? ((in[i] << 8) | in[i + 1]) : (in[i] | (in[i + 1] << 8));
         i += 2;
         if (w2 >= 0xDC00 && w2 <= 0xDFFF)
         {
            unsigned int cp = 0x10000 + (((w1 - 0xD800) << 10) | (w2 - 0xDC00));
            utf8_emit(&out, &j, &cap, cp);
         }
         else
         {
            utf8_emit(&out, &j, &cap, 0xFFFD);
            i -= 2;
         }
      }
      else if (w1 >= 0xDC00 && w1 <= 0xDFFF)
      {
         utf8_emit(&out, &j, &cap, 0xFFFD);
      }
      else
      {
         utf8_emit(&out, &j, &cap, w1);
      }
   }
   out[j] = '\0';
   return out;
}

static char*
id3_text_to_utf8(const unsigned char* data, size_t size)
{
   if (!data || size == 0)
   {
      return NULL;
   }
   uint8_t enc = data[0];
   const unsigned char* p = data + 1;
   size_t n = (size > 1) ? (size - 1) : 0;

   switch (enc)
   {
      case 0x00:
         return iso_8859_1_to_utf8(p, n);
      case 0x03: {
         char* out = (char*)malloc(n + 1);
         if (!out)
         {
            return NULL;
         }
         memcpy(out, p, n);
         out[n] = '\0';
         return out;
      }
      case 0x01: {
         if (n < 2)
         {
            return NULL;
         }
         int be = 0;
         if (p[0] == 0xFE && p[1] == 0xFF)
         {
            be = 1;
         }
         else if (p[0] == 0xFF && p[1] == 0xFE)
         {
            be = 0;
         }
         else
         {
            be = 1;
         }
         size_t off = (p[0] == 0xFE && p[1] == 0xFF) || (p[0] == 0xFF && p[1] == 0xFE) ? 2 : 0;
         return utf16_to_utf8(p + off, (n > off) ? (n - off) : 0, be);
      }
      case 0x02: {
         return utf16_to_utf8(p, n, 1);
      }
   }
   return NULL;
}

static int
is_alpha4(const char id[4])
{
   for (int i = 0; i < 4; i++)
   {
      if (!((id[i] >= 'A' && id[i] <= 'Z') || (id[i] >= '0' && id[i] <= '9')))
      {
         return 0;
      }
   }
   return 1;
}

static uint32_t
id3_synchsafe_to_u32(const unsigned char s[4])
{
   return ((s[0] & 0x7F) << 21) | ((s[1] & 0x7F) << 14) | ((s[2] & 0x7F) << 7) | (s[3] & 0x7F);
}

static uint32_t
be_u32(const unsigned char s[4])
{
   return ((uint32_t)s[0] << 24) | ((uint32_t)s[1] << 16) | ((uint32_t)s[2] << 8) | (uint32_t)s[3];
}

static size_t
id3_text_field_terminator_len(uint8_t enc)
{
   return (enc == 0x00 || enc == 0x03) ? 1 : 2;
}

static int
parse_id3v2_in_dsf(FILE* fp, uint64_t offset, struct metadata* m)
{
   if (fseeko(fp, (off_t)offset, SEEK_SET) != 0)
   {
      return 0;
   }

   unsigned char hdr[10];
   if (fread(hdr, 1, 10, fp) != 10)
   {
      return 0;
   }
   if (memcmp(hdr, "ID3", 3) != 0)
   {
      return 0;
   }
   uint8_t ver = hdr[3];
   uint8_t flags = hdr[5];
   uint32_t tag_size = id3_synchsafe_to_u32(&hdr[6]);

   if ((flags & 0x40) != 0)
   {
      if (fread(hdr, 1, 4, fp) != 4)
      {
         return 0;
      }
      uint32_t ext_size = (ver == 4) ? id3_synchsafe_to_u32(hdr) : be_u32(hdr);
      if (ext_size < 4)
      {
         return 0;
      }
      if (fseeko(fp, ext_size - 4, SEEK_CUR) != 0)
      {
         return 0;
      }
   }

   size_t remaining = tag_size;
   int updated = 0;

   while (remaining >= 10)
   {
      unsigned char fh[10];
      if (fread(fh, 1, 10, fp) != 10)
      {
         break;
      }

      char fid[5];
      memcpy(fid, fh, 4);
      fid[4] = '\0';

      if (!is_alpha4(fid))
      {
         break;
      }

      uint32_t fsize = (ver == 4) ? id3_synchsafe_to_u32(&fh[4]) : be_u32(&fh[4]);
      if (fsize == 0 || fsize > remaining - 10)
      {
         break;
      }

      unsigned char* buf = (unsigned char*)malloc(fsize);
      if (!buf)
      {
         break;
      }
      if (fread(buf, 1, fsize, fp) != fsize)
      {
         free(buf);
         break;
      }

      if ((fid[0] == 'T') && strcmp(fid, "TXXX") != 0)
      {
         char* val = id3_text_to_utf8(buf, fsize);
         if (val && val[0])
         {
            if (strcmp(fid, "TIT2") == 0 && !m->title)
            {
               m->title = val; updated = 1; val = NULL;
            }
            else if (strcmp(fid, "TPE1") == 0 && !m->artist)
            {
               m->artist = val; updated = 1; val = NULL;
            }
            else if (strcmp(fid, "TALB") == 0 && !m->album)
            {
               m->album = val; updated = 1; val = NULL;
            }
            else if (strcmp(fid, "TCON") == 0 && !m->genre)
            {
               m->genre = val; updated = 1; val = NULL;
            }
            else if ((strcmp(fid, "TDRC") == 0 || strcmp(fid, "TYER") == 0) && !m->date)
            {
               m->date = val; updated = 1; val = NULL;
            }
            else if (strcmp(fid, "TRCK") == 0 && m->track < 0)
            {
               m->track = parse_x_of_y(val); updated = 1;
            }
            else if (strcmp(fid, "TPOS") == 0 && m->disc < 0)
            {
               m->disc = parse_x_of_y(val); updated = 1;
            }
            if (val)
            {
               free(val);
            }
         }
         else if (val)
         {
            free(val);
         }
      }
      else if (strcmp(fid, "COMM") == 0 && !m->comment && fsize >= 5)
      {
         uint8_t enc = buf[0];
         const unsigned char* p = buf + 1;
         size_t n = fsize - 1;
         if (n >= 3)
         {
            p += 3; n -= 3;
            size_t term = id3_text_field_terminator_len(enc);
            size_t i = 0;
            while (i + term <= n)
            {
               int is_term = 1;
               for (size_t k = 0; k < term; k++)
               {
                  if (p[i + k] != 0x00)
                  {
                     is_term = 0; break;
                  }
               }
               if (is_term)
               {
                  i += term; break;
               }
               i++;
            }
            if (i <= n)
            {
               const unsigned char* text = p + i;
               size_t text_len = n - i;
               unsigned char* payload = (unsigned char*)malloc(text_len + 1);
               if (payload)
               {
                  payload[0] = enc;
                  memcpy(payload + 1, text, text_len);
                  char* val = id3_text_to_utf8(payload, text_len + 1);
                  free(payload);
                  if (val && val[0])
                  {
                     m->comment = val;
                     updated = 1;
                  }
                  else if (val)
                  {
                     free(val);
                  }
               }
            }
         }
      }

      free(buf);
      if (fsize + 10 > remaining)
      {
         break;
      }
      remaining -= (fsize + 10);
   }

   return updated;
}

static int
parse_dsf_tags_into_metadata(const char* path, struct metadata* m)
{
   FILE* fp = fopen(path, "rb");
   if (!fp)
   {
      return 0;
   }

   char sig[4];
   if (fread(sig, 1, 4, fp) != 4 || memcmp(sig, "DSD ", 4) != 0)
   {
      fclose(fp);
      return 0;
   }

   uint32_t header_size = hrmp_read_le_u32(fp);
   (void)header_size;
   uint64_t file_size = hrmp_read_le_u64(fp);
   (void)file_size;
   uint64_t metadata_offset = hrmp_read_le_u64(fp);

   int ok = 0;
   if (metadata_offset != 0)
   {
      ok = parse_id3v2_in_dsf(fp, metadata_offset, m);
   }
   fclose(fp);
   return ok;
}

static void
fill_strings_from_sndfile(SNDFILE* sf, struct metadata* m)
{
   char* v;

   if (!m->title)
   {
      v = (char*)sf_get_string(sf, SF_STR_TITLE);
      if (v)
      {
         m->title = hrmp_copy_string(v);
      }
   }
   if (!m->artist)
   {
      v = (char*)sf_get_string(sf, SF_STR_ARTIST);
      if (v)
      {
         m->artist = hrmp_copy_string(v);
      }
   }
   if (!m->album)
   {
      v = (char*)sf_get_string(sf, SF_STR_ALBUM);
      if (v)
      {
         m->album = hrmp_copy_string(v);
      }
   }
   if (!m->genre)
   {
      v = (char*)sf_get_string(sf, SF_STR_GENRE);
      if (v)
      {
         m->genre = hrmp_copy_string(v);
      }
   }
   if (!m->comment)
   {
      v = (char*)sf_get_string(sf, SF_STR_COMMENT);
      if (v)
      {
         m->comment = hrmp_copy_string(v);
      }
   }
   if (!m->date)
   {
      v = (char*)sf_get_string(sf, SF_STR_DATE);
      if (v)
      {
         m->date = hrmp_copy_string(v);
      }
   }
}

static void
fill_format_from_sfinfo(SNDFILE* sf, const SF_INFO* info, struct metadata* m)
{
   SF_FORMAT_INFO finfo;

   memset(&finfo, 0, sizeof(finfo));
   finfo.format = info->format;

   if (sf_command(sf, SFC_GET_FORMAT_INFO, &finfo, sizeof(finfo)) == SF_TRUE)
   {
      if (!m->format_name && finfo.name)
      {
         m->format_name = hrmp_copy_string((char*)finfo.name);
      }
      if (!m->codec_name && finfo.extension)
      {
         m->codec_name = hrmp_copy_string((char*)finfo.extension);
      }
   }

   if (info->samplerate > 0)
   {
      m->sample_rate = info->samplerate;
   }
   if (info->channels > 0)
   {
      m->channels = info->channels;
   }

   if (info->frames > 0 && info->samplerate > 0)
   {
      double ms = (1000.0 * (double)info->frames) / (double)info->samplerate;
      if (ms >= 0.0 && ms < 2147483647.0)
      {
         m->duration_ms = (int)(ms + 0.5);
      }
   }
}

void
hrmp_metadata_destroy(struct metadata* m)
{
   if (m != NULL)
   {
      free(m->path);
      free(m->title);
      free(m->artist);
      free(m->album);
      free(m->genre);
      free(m->comment);
      free(m->date);
      free(m->format_name);
      free(m->codec_name);
      free(m);
   }
}

void
hrmp_metadata_print(struct metadata* metadata)
{
   if (metadata != NULL)
   {
      if (metadata->artist)
      {
         printf("Artist     : %s\n", metadata->artist);
      }
      if (metadata->title)
      {
         printf("Title      : %s\n", metadata->title);
      }
      if (metadata->album)
      {
         printf("Album      : %s\n", metadata->album);
      }
      if (metadata->genre)
      {
         printf("Genre      : %s\n", metadata->genre);
      }
      if (metadata->comment)
      {
         printf("Comment    : %s\n", metadata->comment);
      }
      if (metadata->date)
      {
         printf("Date       : %s\n", metadata->date);
      }
      if (metadata->track >= 0)
      {
         printf("Track      : %d\n", metadata->track);
      }
      if (metadata->disc >= 0)
      {
         printf("Disc       : %d\n", metadata->disc);
      }
      if (metadata->format_name)
      {
         printf("Format     : %s\n", metadata->format_name);
      }
      if (metadata->codec_name)
      {
         printf("Codec      : %s\n", metadata->codec_name);
      }
      if (metadata->duration_ms >= 0)
      {
         printf("Duration   : %d ms\n", metadata->duration_ms);
      }
      if (metadata->sample_rate >= 0)
      {
         printf("SampleRate : %d Hz\n", metadata->sample_rate);
      }
      if (metadata->channels >= 0)
      {
         printf("Channels   : %d\n", metadata->channels);
      }
      if (metadata->bit_rate >= 0)
      {
         printf("Bitrate    : %d bps\n", metadata->bit_rate);
      }
      if (metadata->path)
      {
         printf("Source     : %s\n", metadata->path);
      }
   }
}

int
hrmp_metadata_create(struct file_metadata* fm, struct metadata** metadata)
{
   SF_INFO sfinfo;
   SNDFILE* sf = NULL;
   struct metadata* m = NULL;

   *metadata = NULL;

   m = (struct metadata*)calloc(1, sizeof(struct metadata));
   if (m == NULL)
   {
      goto error;
   }

   m->track = -1;
   m->disc = -1;
   m->duration_ms = -1;
   m->sample_rate = -1;
   m->channels = -1;
   m->bit_rate = -1;

   m->path = hrmp_copy_string(fm->name);

   if (fm->type != TYPE_DSF)
   {
      memset(&sfinfo, 0, sizeof(sfinfo));
      sf = sf_open(fm->name, SFM_READ, &sfinfo);

      if (sf == NULL)
      {
         goto error;
      }

      fill_format_from_sfinfo(sf, &sfinfo, m);
      fill_strings_from_sndfile(sf, m);
   }
   else
   {
      parse_dsf_tags_into_metadata(fm->name, m);

      if (m->codec_name == NULL)
      {
         m->codec_name = hrmp_copy_string("DSD");
      }
      if (m->format_name == NULL)
      {
         m->format_name = hrmp_copy_string("DSF (DSD Stream File)");
      }
   }

   *metadata = m;

   if (sf != NULL)
   {
      sf_close(sf);
   }

   return 0;

error:

   hrmp_metadata_destroy(m);

   if (sf != NULL)
   {
      sf_close(sf);
   }

   *metadata = NULL;

   return 1;
}
