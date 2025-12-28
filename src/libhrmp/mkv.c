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
#include <mkv.h>
#include <ringbuffer.h>
#include <utils.h>

/* system */
#include <neaacdec.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <opus/opus.h>
#include <opus/opus_multistream.h>

typedef struct
{
   FILE* fp;
   struct ringbuffer* rb;
   uint64_t pos;
   uint64_t file_size;
   uint64_t* bytes_left;
} EbmlReader;

static void
ebml_reader_init(EbmlReader* r, FILE* fp, struct ringbuffer* rb, uint64_t file_size, uint64_t* bytes_left)
{
   r->fp = fp;
   r->rb = rb;
#if defined(_WIN32) || defined(_WIN64)
   r->pos = (uint64_t)_ftelli64(fp);
#else
   r->pos = (uint64_t)ftello(fp);
#endif
   r->file_size = file_size;
   r->bytes_left = bytes_left;
   if (r->bytes_left != NULL && r->file_size > 0)
   {
      *r->bytes_left = (r->pos <= r->file_size) ? (r->file_size - r->pos) : 0;
   }
}

static uint64_t
ebml_tell(EbmlReader* r)
{
   return r->pos;
}

static int
ebml_seek(EbmlReader* r, uint64_t pos)
{
#if defined(_WIN32) || defined(_WIN64)
   if (_fseeki64(r->fp, (long long)pos, SEEK_SET) != 0)
   {
      return -1;
   }
#else
   if (fseeko(r->fp, (off_t)pos, SEEK_SET) != 0)
   {
      return -1;
   }
#endif

   r->pos = pos;
   if (r->rb != NULL)
   {
      hrmp_ringbuffer_reset(r->rb);
   }
   if (r->bytes_left != NULL && r->file_size > 0)
   {
      *r->bytes_left = (r->pos <= r->file_size) ? (r->file_size - r->pos) : 0;
   }

   return 0;
}

static long
ebml_read(EbmlReader* r, void* dst, size_t size)
{
   if (size == 0)
   {
      return 0;
   }

   size_t n = 0;

   if (r->rb == NULL)
   {
      n = fread(dst, 1, size, r->fp);
      if (n != size && ferror(r->fp))
      {
         return -1;
      }
   }
   else
   {
      uint8_t* out = (uint8_t*)dst;
      while (n < size)
      {
         void* rp = NULL;
         size_t have = hrmp_ringbuffer_peek(r->rb, &rp);
         if (have > 0)
         {
            size_t take = (size - n) < have ? (size - n) : have;
            memcpy(out + n, rp, take);
            hrmp_ringbuffer_consume(r->rb, take);
            n += take;
            continue;
         }

         if (hrmp_ringbuffer_ensure_write(r->rb, 1))
         {
            return -1;
         }

         void* wp = NULL;
         size_t span = hrmp_ringbuffer_get_write_span(r->rb, &wp);
         if (span == 0)
         {
            if (hrmp_ringbuffer_ensure_write(r->rb, hrmp_ringbuffer_capacity(r->rb) / 2u))
            {
               return -1;
            }
            continue;
         }

         size_t got = fread(wp, 1, span, r->fp);
         if (got == 0)
         {
            if (ferror(r->fp))
            {
               return -1;
            }
            break;
         }
         if (hrmp_ringbuffer_produce(r->rb, got))
         {
            return -1;
         }
      }
   }

   r->pos += (uint64_t)n;
   if (r->bytes_left != NULL && r->file_size > 0)
   {
      *r->bytes_left = (r->pos <= r->file_size) ? (r->file_size - r->pos) : 0;
   }

   return (long)n;
}

static int
ebml_skip(EbmlReader* r, uint64_t size)
{
   if (size == 0)
   {
      return 0;
   }
   if (ebml_seek(r, ebml_tell(r) + size) != 0)
   {
      return -1;
   }
   return 0;
}

static int
ebml_read_u8(EbmlReader* r)
{
   unsigned char b;
   long n = ebml_read(r, &b, 1);
   if (n != 1)
   {
      return -1;
   }
   return (int)b;
}

static int
clz8(uint8_t b)
{
   int n = 0;
   for (int i = 7; i >= 0; --i)
   {
      if ((b >> i) & 1)
      {
         break;
      }
      n++;
   }
   return n;
}

/* Read an EBML variable-length integer (VINT) used for sizes. */
static int
ebml_read_vint(EbmlReader* r, uint64_t* value, int allow_unknown)
{
   int b0 = ebml_read_u8(r);
   if (b0 < 0)
   {
      return -1;
   }

   int lz = clz8((uint8_t)b0);
   if (lz >= 8)
   {
      return -1;
   }
   int length = lz + 1;

   uint64_t mask = ((uint64_t)1 << (8 - length)) - 1;
   uint64_t val = (uint8_t)b0 & mask;

   for (int i = 1; i < length; ++i)
   {
      int bi = ebml_read_u8(r);
      if (bi < 0)
      {
         return -1;
      }
      val = (val << 8) | (uint8_t)bi;
   }

   if (allow_unknown)
   {
      uint64_t all_ones = 0;
      for (int i = 0; i < (8 - length); ++i)
      {
         all_ones = (all_ones << 1) | 1u;
      }
      if (val == all_ones)
      {
         *value = (uint64_t)-1;
         return length;
      }
   }

   *value = val;
   return length;
}

/* Element header: read ID (1..4 bytes VINT-like) and size (VINT) */
static int
ebml_read_element_header(EbmlReader* r, uint32_t* id, uint64_t* size)
{
   /* Read ID (1..4 bytes; includes leading bit, not masked like sizes) */
   int b0 = ebml_read_u8(r);
   if (b0 < 0)
   {
      return -1;
   }
   int lz = clz8((uint8_t)b0);
   int id_len = lz + 1;
   if (id_len < 1 || id_len > 4)
   {
      return -1;
   }

   uint32_t id_val = (uint8_t)b0;
   for (int i = 1; i < id_len; ++i)
   {
      int bi = ebml_read_u8(r);
      if (bi < 0)
      {
         return -1;
      }
      id_val = (id_val << 8) | (uint8_t)bi;
   }

   uint64_t size_val;
   int size_len = ebml_read_vint(r, &size_val, 1);
   if (size_len < 0)
   {
      return -1;
   }

   *id = id_val;
   *size = size_val;
   return id_len + size_len;
}

static int
ebml_read_uint(EbmlReader* r, uint64_t size, uint64_t* out)
{
   if (size == (uint64_t)-1 || size == 0 || size > 8)
   {
      return -1;
   }
   uint64_t v = 0;
   for (uint64_t i = 0; i < size; ++i)
   {
      int b = ebml_read_u8(r);
      if (b < 0)
      {
         return -1;
      }
      v = (v << 8) | (uint8_t)b;
   }
   *out = v;
   return 0;
}

static int
ebml_read_float(EbmlReader* r, uint64_t size, double* out)
{
   if (size == 4)
   {
      uint8_t buf[4];
      if (ebml_read(r, buf, 4) != 4)
      {
         return -1;
      }
      uint32_t u = ((uint32_t)buf[0] << 24) |
                   ((uint32_t)buf[1] << 16) |
                   ((uint32_t)buf[2] << 8) |
                   (uint32_t)buf[3];
      float f;
      memcpy(&f, &u, sizeof(f));
      *out = (double)f;
      return 0;
   }
   else if (size == 8)
   {
      uint8_t buf[8];
      if (ebml_read(r, buf, 8) != 8)
      {
         return -1;
      }
      uint64_t u = ((uint64_t)buf[0] << 56) |
                   ((uint64_t)buf[1] << 48) |
                   ((uint64_t)buf[2] << 40) |
                   ((uint64_t)buf[3] << 32) |
                   ((uint64_t)buf[4] << 24) |
                   ((uint64_t)buf[5] << 16) |
                   ((uint64_t)buf[6] << 8) |
                   (uint64_t)buf[7];
      double d;
      memcpy(&d, &u, sizeof(d));
      *out = d;
      return 0;
   }
   return -1;
}

static int
ebml_read_string(EbmlReader* r, uint64_t size, char* buf, size_t bufsize)
{
   if (size == (uint64_t)-1)
   {
      return -1;
   }
   if (bufsize < size + 1)
   {
      return -1;
   }
   long n = ebml_read(r, buf, (size_t)size);
   if (n < 0 || (uint64_t)n != size)
   {
      return -1;
   }
   buf[size] = '\0';
   return 0;
}

static int
ebml_read_binary(EbmlReader* r, uint64_t size, uint8_t** out)
{
   if (size == (uint64_t)-1)
   {
      return -1;
   }
   uint8_t* p = (uint8_t*)malloc((size_t)size);
   if (!p)
   {
      return -1;
   }
   long n = ebml_read(r, p, (size_t)size);
   if (n < 0 || (uint64_t)n != size)
   {
      free(p);
      return -1;
   }
   *out = p;
   return 0;
}

#define ID_EBML            0x1A45DFA3u
#define ID_SEGMENT         0x18538067u
#define ID_INFO            0x1549A966u
#define ID_TIMECODESCALE   0x2AD7B1u
#define ID_TRACKS          0x1654AE6Bu
#define ID_TRACKENTRY      0xAEu
#define ID_TRACKNUMBER     0xD7u
#define ID_TRACKTYPE       0x83u
#define ID_CODECID         0x86u
#define ID_CODECPRIVATE    0x63A2u
#define ID_AUDIO           0xE1u
#define ID_SAMPLINGFREQ    0xB5u
#define ID_CHANNELS        0x9Fu
#define ID_BITDEPTH        0x6264u

#define ID_CLUSTER         0x1F43B675u
#define ID_CLUSTERTIMECODE 0xE7u
#define ID_SIMPLEBLOCK     0xA3u
#define ID_BLOCKGROUP      0xA0u
#define ID_BLOCK           0xA1u

#define TRACK_TYPE_VIDEO   1
#define TRACK_TYPE_AUDIO   2

typedef struct
{
   uint8_t* data;
   size_t size;
   int64_t pts_ns;
   int keyframe;
} PendingPacket;

typedef struct
{
   PendingPacket* items;
   size_t count;
   size_t cap;
   size_t head; /* index of next item to pop */
} PacketQueue;

static void
pq_init(PacketQueue* q)
{
   q->items = NULL;
   q->count = q->cap = q->head = 0;
}

static void
pq_free(PacketQueue* q)
{
   if (q->items)
   {
      for (size_t i = q->head; i < q->count; ++i)
      {
         free(q->items[i].data);
      }
      free(q->items);
   }
   q->items = NULL;
   q->count = q->cap = q->head = 0;
}

static int
pq_push(PacketQueue* q, uint8_t* data, size_t size, int64_t pts_ns, int keyframe)
{
   if (q->count == q->cap)
   {
      size_t new_cap = q->cap ? q->cap * 2 : 4;
      PendingPacket* ni = (PendingPacket*)realloc(q->items, new_cap * sizeof(PendingPacket));
      if (!ni)
      {
         return -1;
      }
      q->items = ni;
      q->cap = new_cap;
   }
   q->items[q->count].data = data;
   q->items[q->count].size = size;
   q->items[q->count].pts_ns = pts_ns;
   q->items[q->count].keyframe = keyframe;
   q->count++;
   return 0;
}

static int
pq_pop(PacketQueue* q, MkvPacket* out)
{
   if (q->head >= q->count)
   {
      return 0;
   }
   PendingPacket* pp = &q->items[q->head++];
   out->data = pp->data;
   out->size = pp->size;
   out->pts_ns = pp->pts_ns;
   out->keyframe = pp->keyframe;
   if (q->head == q->count)
   {
      /* Reset to empty to avoid growth. */
      q->head = q->count = 0;
   }
   return 1;
}

#define OPUS_OUTPUT_HZ         48000
#define OPUS_MAX_FRAME_SAMPLES (5760) /* 120ms @ 48kHz */

#define AAC_OUTPUT_BITS        16

struct OpusState
{
   int is_multistream;
   int channels;
   int streams;
   int coupled_streams;
   unsigned char mapping[255];
   int pre_skip; /* samples to drop at start */
   int pre_skip_remaining;
   OpusDecoder* dec;
   OpusMSDecoder* msdec;
   opus_int16* decode_buf; /* reusable decode buffer */
   size_t decode_cap;      /* samples total capacity */
};

struct AacState
{
   NeAACDecHandle h;
   int initialized; /* 0 until Init or Init2 succeeds */
   int channels;    /* as configured */
   int sample_rate; /* as configured */
};

struct MkvDemuxer
{
   EbmlReader r;
   int opened;

   /* Segment parsing */
   uint64_t segment_start;
   uint64_t segment_size; /* may be (uint64_t)-1 (unknown) */

   /* Info */
   uint64_t timecode_scale_ns; /* default 1ms = 1,000,000 ns */

   /* Selected audio track */
   uint64_t track_number; /* match against Block track number */
   MkvAudioInfo ainfo;

   /* Cluster state */
   uint64_t current_cluster_tc; /* base timecode, in raw "ticks" */

   /* Pending packets (from laced block or decoded audio) */
   PacketQueue q;

   /* FILE* ownership */
   int own_fp;

   /* Codec states */
   struct OpusState opus;
   struct AacState aac;
};

static int parse_header_and_segment(MkvDemuxer* m);
static int parse_info(MkvDemuxer* m, uint64_t elem_end);
static int parse_tracks(MkvDemuxer* m, uint64_t elem_end);
static int parse_cluster(MkvDemuxer* m, uint64_t elem_end);
static int read_block_into_queue(MkvDemuxer* m, uint8_t* block, uint64_t block_size, int simple_block);
static int read_vint_from_mem(const uint8_t* p, const uint8_t* end, uint64_t* val, int allow_unknown, int* length);
static int64_t clamp_i128_to_i64(__int128 v);
static int enqueue_pcm_bytes(MkvDemuxer* m, const uint8_t* pcm, size_t bytes, int64_t pts_ns, int keyframe);

static MkvCodecId
codec_from_id(const char* cid)
{
   if (!cid)
   {
      return MKV_CODEC_UNKNOWN;
   }
   if (strcmp(cid, "A_VORBIS") == 0)
   {
      return MKV_CODEC_VORBIS;
   }
   if (strcmp(cid, "A_OPUS") == 0)
   {
      return MKV_CODEC_OPUS;
   }
   if (strcmp(cid, "A_FLAC") == 0)
   {
      return MKV_CODEC_FLAC;
   }
   if (strncmp(cid, "A_AAC", 5) == 0)
   {
      return MKV_CODEC_AAC;
   }
   if (strcmp(cid, "A_PCM/INT/LIT") == 0)
   {
      return MKV_CODEC_PCM_INT;
   }
   if (strcmp(cid, "A_PCM/FLOAT/IEEE") == 0)
   {
      return MKV_CODEC_PCM_FLOAT;
   }
   return MKV_CODEC_UNKNOWN;
}

static uint16_t
rd_le_u16(const uint8_t* p)
{
   return (uint16_t)(p[0] | (p[1] << 8));
}
static uint32_t
rd_le_u32(const uint8_t* p)
{
   return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

static int
init_opus_from_codec_private(MkvDemuxer* m, const uint8_t* cp, size_t cpsize)
{
   if (!cp || cpsize < 19)
   {
      return -1; /* minimal OpusHead */
   }
   if (memcmp(cp, "OpusHead", 8) != 0)
   {
      return -1;
   }

   const uint8_t* p = cp + 8;
   size_t left = cpsize - 8;

   if (left < 1 + 1 + 2 + 4 + 2 + 1)
   {
      return -1;
   }
   uint8_t version = p[0];
   (void)version;
   uint8_t ch = p[1];
   uint16_t pre_skip = rd_le_u16(p + 2);
   uint32_t input_rate = rd_le_u32(p + 4);
   (void)input_rate;
   int16_t output_gain = (int16_t)rd_le_u16(p + 8);
   (void)output_gain;
   uint8_t mapping_family = p[10];
   p += 11;
   left -= 11;

   struct OpusState* os = &m->opus;
   memset(os, 0, sizeof(*os));
   os->channels = (int)ch;
   os->pre_skip = (int)pre_skip;
   os->pre_skip_remaining = os->pre_skip;
   os->decode_buf = NULL;
   os->decode_cap = 0;
   os->dec = NULL;
   os->msdec = NULL;
   int err = OPUS_OK;

   if (mapping_family == 0)
   {
      os->is_multistream = 0;
      os->streams = 1;
      os->coupled_streams = (ch == 2) ? 1 : 0;
      /* Init simple decoder */
      os->dec = opus_decoder_create(OPUS_OUTPUT_HZ, ch, &err);
      if (err != OPUS_OK || !os->dec)
      {
         return -1;
      }
   }
   else
   {
      if (left < 2)
      {
         return -1;
      }
      uint8_t streams = p[0];
      uint8_t coupled = p[1];
      p += 2;
      left -= 2;
      if ((int)left < os->channels)
      {
         return -1;
      }
      memset(os->mapping, 0, sizeof(os->mapping));
      for (int i = 0; i < os->channels; ++i)
      {
         os->mapping[i] = p[i];
      }

      os->is_multistream = 1;
      os->streams = streams;
      os->coupled_streams = coupled;

      err = OPUS_OK;
      os->msdec = opus_multistream_decoder_create(OPUS_OUTPUT_HZ,
                                                  os->channels,
                                                  os->streams,
                                                  os->coupled_streams,
                                                  os->mapping,
                                                  &err);
      if (err != OPUS_OK || !os->msdec)
      {
         return -1;
      }
   }

   m->ainfo.sample_rate = OPUS_OUTPUT_HZ;
   m->ainfo.channels = (uint8_t)os->channels;
   m->ainfo.bit_depth = 16;

   return 0;
}

static int
opus_decode_packet(MkvDemuxer* m, const uint8_t* pkt, size_t pkt_sz,
                   uint8_t** out, size_t* out_bytes)
{
   struct OpusState* os = &m->opus;
   if (!pkt || pkt_sz == 0 || !out || !out_bytes)
   {
      return -1;
   }

   int max_samples = OPUS_MAX_FRAME_SAMPLES;
   size_t need_cap = (size_t)max_samples * (size_t)os->channels;
   if (os->decode_cap < need_cap)
   {
      opus_int16* nb = (opus_int16*)realloc(os->decode_buf, need_cap * sizeof(opus_int16));
      if (!nb)
      {
         return -1;
      }
      os->decode_buf = nb;
      os->decode_cap = need_cap;
   }

   int decoded = 0;
   if (os->is_multistream)
   {
      decoded = opus_multistream_decode(os->msdec,
                                        pkt, (opus_int32)pkt_sz,
                                        os->decode_buf, max_samples, 0);
   }
   else
   {
      decoded = opus_decode(os->dec,
                            pkt, (opus_int32)pkt_sz,
                            os->decode_buf, max_samples, 0);
   }
   if (decoded < 0)
   {
      return -1;
   }

   int samples = decoded;
   int drop = 0;
   if (os->pre_skip_remaining > 0)
   {
      drop = (samples < os->pre_skip_remaining) ? samples : os->pre_skip_remaining;
      os->pre_skip_remaining -= drop;
      samples -= drop;
   }
   if (samples <= 0)
   {
      *out = NULL;
      *out_bytes = 0;
      return 0;
   }

   opus_int16* src = os->decode_buf + drop * os->channels;
   size_t total_samples = (size_t)samples * (size_t)os->channels;
   size_t bytes = total_samples * sizeof(opus_int16);

   uint8_t* buf = (uint8_t*)malloc(bytes);
   if (!buf)
   {
      return -1;
   }
   memcpy(buf, src, bytes);

   *out = buf;
   *out_bytes = bytes;
   return 0;
}

static int
init_aac_decoder(MkvDemuxer* m)
{
   if (m->aac.h)
   {
      return 0;
   }
   m->aac.h = NeAACDecOpen();
   if (!m->aac.h)
   {
      return -1;
   }

   NeAACDecConfigurationPtr cfg = NeAACDecGetCurrentConfiguration(m->aac.h);
   if (!cfg)
   {
      return -1;
   }
   cfg->outputFormat = FAAD_FMT_16BIT;
   cfg->downMatrix = 0;
   if (!NeAACDecSetConfiguration(m->aac.h, cfg))
   {
      return -1;
   }
   m->aac.initialized = 0;
   return 0;
}

/* Initialize from AudioSpecificConfig (CodecPrivate) if present */
static int
init_aac_from_codec_private(MkvDemuxer* m, const uint8_t* cp, size_t cpsize)
{
   if (init_aac_decoder(m) < 0)
   {
      return -1;
   }
   if (!cp || cpsize == 0)
   {
      return 0;
   }
   unsigned long sr = 0;
   unsigned char ch = 0;
   long rc = NeAACDecInit2(m->aac.h, (unsigned char*)cp, (unsigned long)cpsize, &sr, &ch);
   if (rc < 0)
   {
      return -1;
   }

   m->aac.initialized = 1;
   m->aac.sample_rate = (int)sr;
   m->aac.channels = (int)ch;

   if (m->ainfo.sample_rate <= 0)
   {
      m->ainfo.sample_rate = (double)sr;
   }
   if (m->ainfo.channels == 0)
   {
      m->ainfo.channels = (uint8_t)ch;
   }
   m->ainfo.bit_depth = AAC_OUTPUT_BITS;
   return 0;
}

static int
aac_decode_packet(MkvDemuxer* m, const uint8_t* pkt, size_t pkt_sz,
                  uint8_t** out, size_t* out_bytes)
{
   if (!pkt || pkt_sz == 0 || !out || !out_bytes)
   {
      return -1;
   }

   if (!m->aac.h)
   {
      if (init_aac_decoder(m) < 0)
      {
         return -1;
      }
   }

   if (!m->aac.initialized)
   {
      unsigned long sr = 0;
      unsigned char ch = 0;
      long consumed = NeAACDecInit(m->aac.h, (unsigned char*)pkt, (unsigned long)pkt_sz, &sr, &ch);
      if (consumed < 0)
      {
         return -1;
      }
      m->aac.initialized = 1;
      m->aac.sample_rate = (int)sr;
      m->aac.channels = (int)ch;
      if (m->ainfo.sample_rate <= 0)
      {
         m->ainfo.sample_rate = (double)sr;
      }
      if (m->ainfo.channels == 0)
      {
         m->ainfo.channels = (uint8_t)ch;
      }
      m->ainfo.bit_depth = AAC_OUTPUT_BITS;
      if ((size_t)consumed < pkt_sz)
      {
         pkt += consumed;
         pkt_sz -= (size_t)consumed;
      }
      else
      {
         *out = NULL;
         *out_bytes = 0;
         return 0;
      }
   }

   NeAACDecFrameInfo info;
   void* pcm = NeAACDecDecode(m->aac.h, &info, (unsigned char*)pkt, (unsigned long)pkt_sz);
   if (info.error != 0)
   {
      *out = NULL;
      *out_bytes = 0;
      return 0;
   }

   if (!pcm || info.samples == 0 || info.channels == 0)
   {
      *out = NULL;
      *out_bytes = 0;
      return 0;
   }

   size_t total_samples = (size_t)info.samples * (size_t)info.channels;
   size_t bytes = total_samples * sizeof(int16_t);

   uint8_t* buf = (uint8_t*)malloc(bytes);
   if (!buf)
   {
      return -1;
   }
   memcpy(buf, pcm, bytes);

   *out = buf;
   *out_bytes = bytes;
   return 0;
}

int
hrmp_mkv_open(FILE* fp, MkvDemuxer** out)
{
   if (!fp || !out)
   {
      return -1;
   }
   MkvDemuxer* m = (MkvDemuxer*)calloc(1, sizeof(*m));
   if (!m)
   {
      return -1;
   }
   ebml_reader_init(&m->r, fp, NULL, 0, NULL);
   m->timecode_scale_ns = 1000000ULL;
   pq_init(&m->q);
   m->own_fp = 1;

   if (parse_header_and_segment(m) < 0 || m->track_number == 0)
   {
      pq_free(&m->q);
      if (m->own_fp && m->r.fp)
      {
         fclose(m->r.fp);
      }
      free(m);
      return -1;
   }

   m->opened = 1;
   *out = m;
   return 0;
}

int
hrmp_mkv_open_path(const char* path, MkvDemuxer** out)
{
   if (!path || !out)
   {
      return -1;
   }
   FILE* fp = fopen(path, "rb");
   if (!fp)
   {
      return -1;
   }
   int rc = hrmp_mkv_open(fp, out);
   if (rc < 0)
   {
      return rc;
   }
   return 0;
}

int
hrmp_mkv_open_path_rb(const char* path, struct ringbuffer* rb, uint64_t file_size, uint64_t* bytes_left, MkvDemuxer** out)
{
   if (!path || !out)
   {
      return -1;
   }
   FILE* fp = fopen(path, "rb");
   if (!fp)
   {
      return -1;
   }
   if (rb != NULL)
   {
      hrmp_ringbuffer_reset(rb);
   }

   MkvDemuxer* m = (MkvDemuxer*)calloc(1, sizeof(*m));
   if (!m)
   {
      fclose(fp);
      return -1;
   }

   ebml_reader_init(&m->r, fp, rb, file_size, bytes_left);
   m->timecode_scale_ns = 1000000ULL;
   pq_init(&m->q);
   m->own_fp = 1;

   if (parse_header_and_segment(m) < 0 || m->track_number == 0)
   {
      pq_free(&m->q);
      if (m->own_fp && m->r.fp)
      {
         fclose(m->r.fp);
      }
      free(m);
      return -1;
   }

   m->opened = 1;
   *out = m;
   return 0;
}

void
hrmp_mkv_close(MkvDemuxer* m)
{
   if (m != NULL)
   {
      pq_free(&m->q);
      if (m->ainfo.codec_private)
      {
         free(m->ainfo.codec_private);
      }
      if (m->opus.dec)
      {
         opus_decoder_destroy(m->opus.dec);
      }
      if (m->opus.msdec)
      {
         opus_multistream_decoder_destroy(m->opus.msdec);
      }
      free(m->opus.decode_buf);
      if (m->aac.h)
      {
         NeAACDecClose(m->aac.h);
      }
      if (m->own_fp && m->r.fp)
      {
         fclose(m->r.fp);
      }
      free(m);
   }
}

int
hrmp_mkv_get_audio_info(MkvDemuxer* m, MkvAudioInfo* out_info)
{
   if (!m || !m->opened || !out_info)
   {
      return -1;
   }
   *out_info = m->ainfo;
   return 0;
}

void
hrmp_mkv_free_packet(MkvPacket* packet)
{
   if (!packet)
   {
      return;
   }
   free(packet->data);
   packet->data = NULL;
   packet->size = 0;
}

int
hrmp_mkv_read_packet(MkvDemuxer* m, MkvPacket* packet)
{
   if (!m || !packet)
   {
      return -1;
   }

   int qrc = pq_pop(&m->q, packet);
   if (qrc != 0)
   {
      return qrc;
   }

   while (1)
   {
      uint32_t id;
      uint64_t size;
      int hdr = ebml_read_element_header(&m->r, &id, &size);
      if (hdr < 0)
      {
         /* EOF or error */
         return 0;
      }
      uint64_t elem_start = ebml_tell(&m->r);
      uint64_t elem_end = (size == (uint64_t)-1) ? 0 : elem_start + size;

      if (id == ID_CLUSTER)
      {
         if (parse_cluster(m, elem_end) < 0)
         {
            return -1;
         }
         /* Now pop one */
         qrc = pq_pop(&m->q, packet);
         if (qrc != 0)
         {
            return qrc;
         }
      }
      else
      {
         if (size == (uint64_t)-1)
         {
            return 0;
         }
         if (ebml_skip(&m->r, size) < 0)
         {
            return -1;
         }
      }
   }
}

static int
parse_info(MkvDemuxer* m, uint64_t elem_end)
{
   while (1)
   {
      if (elem_end && ebml_tell(&m->r) >= elem_end)
      {
         break;
      }
      uint32_t id;
      uint64_t size;
      if (ebml_read_element_header(&m->r, &id, &size) < 0)
      {
         return -1;
      }
      if (id == ID_TIMECODESCALE)
      {
         uint64_t scale = 0;
         if (ebml_read_uint(&m->r, size, &scale) < 0)
         {
            return -1;
         }
         if (scale > 0)
         {
            m->timecode_scale_ns = scale;
         }
      }
      else
      {
         if (size == (uint64_t)-1)
         {
            return 0;
         }
         if (ebml_skip(&m->r, size) < 0)
         {
            return -1;
         }
      }
   }
   return 0;
}

static int
read_vint_from_mem(const uint8_t* p, const uint8_t* end, uint64_t* val, int allow_unknown, int* length)
{
   if (p >= end)
   {
      return -1;
   }
   uint8_t b0 = *p++;
   int lz = clz8(b0);
   int len = lz + 1;
   if (len < 1 || len > 8)
   {
      return -1;
   }
   if (p + (len - 1) > end)
   {
      return -1;
   }

   uint64_t mask = ((uint64_t)1 << (8 - len)) - 1;
   uint64_t v = b0 & mask;
   for (int i = 1; i < len; ++i)
   {
      v = (v << 8) | *p++;
   }

   if (allow_unknown)
   {
      uint64_t all_ones = 0;
      for (int i = 0; i < (8 - len); ++i)
      {
      }
      all_ones = 0;
      for (int i = 0; i < (8 - len); ++i)
      {
         all_ones = (all_ones << 1) | 1u;
      }

      if (v == all_ones)
      {
         *val = (uint64_t)-1;
         if (length)
         {
            *length = len;
         }
         return 0;
      }
   }

   *val = v;
   if (length)
   {
      *length = len;
   }
   return 0;
}

static int64_t
clamp_i128_to_i64(__int128 v)
{
   if (v > (__int128)INT64_MAX)
   {
      return INT64_MAX;
   }
   if (v < (__int128)INT64_MIN)
   {
      return INT64_MIN;
   }
   return (int64_t)v;
}

static int
enqueue_pcm_bytes(MkvDemuxer* m, const uint8_t* pcm, size_t bytes, int64_t pts_ns, int keyframe)
{
   if (!pcm || bytes == 0)
   {
      return 0;
   }
   uint8_t* buf = (uint8_t*)malloc(bytes);
   if (!buf)
   {
      return -1;
   }
   memcpy(buf, pcm, bytes);
   return pq_push(&m->q, buf, bytes, pts_ns, keyframe);
}

static int
read_block_into_queue(MkvDemuxer* m, uint8_t* block, uint64_t block_size, int simple_block)
{
   (void)simple_block;
   const uint8_t* p = block;
   const uint8_t* end = block + block_size;

   uint64_t track_no = 0;
   int tlen = 0;
   if (read_vint_from_mem(p, end, &track_no, 0, &tlen) < 0)
   {
      return -1;
   }
   p += tlen;
   if (p + 3 > end)
   {
      return -1;
   }

   int16_t rel_tc = (int16_t)((p[0] << 8) | p[1]);
   p += 2;

   /* Flags */
   uint8_t flags = *p++;
   int lacing = (flags & 0x06) >> 1;
   int keyframe = (flags & 0x80) ? 1 : 0;

   if (track_no != m->track_number)
   {
      return 0;
   }

   int64_t block_timecode = (int64_t)m->current_cluster_tc + (int64_t)rel_tc;
#if defined(__GNUC__) || defined(__clang__)
   __int128 prod = (__int128)block_timecode * (__int128)m->timecode_scale_ns;
   int64_t base_pts_ns = clamp_i128_to_i64(prod);
#else
   long double prod = (long double)block_timecode * (long double)m->timecode_scale_ns;
   if (prod > (long double)INT64_MAX)
   {
      prod = (long double)INT64_MAX;
   }
   if (prod < (long double)INT64_MIN)
   {
      prod = (long double)INT64_MIN;
   }
   int64_t base_pts_ns = (int64_t)prod;
#endif

   if (m->ainfo.codec == MKV_CODEC_OPUS)
   {
      if (lacing == 0)
      {
         size_t pkt_sz = (size_t)(end - p);
         if (pkt_sz == 0)
         {
            return 0;
         }
         uint8_t* pcm = NULL;
         size_t pcm_bytes = 0;
         if (opus_decode_packet(m, p, pkt_sz, &pcm, &pcm_bytes) < 0)
         {
            return -1;
         }
         if (pcm && pcm_bytes)
         {
            if (enqueue_pcm_bytes(m, pcm, pcm_bytes, base_pts_ns, keyframe) < 0)
            {
               free(pcm);
               return -1;
            }
            free(pcm);
         }
         return 0;
      }
      else if (lacing == 1)
      {
         if (p >= end)
         {
            return -1;
         }
         uint8_t lace_count = *p++;
         int frames = 1 + lace_count;
         if (frames <= 0)
         {
            return -1;
         }
         size_t* sizes = (size_t*)calloc((size_t)frames, sizeof(size_t));
         if (!sizes)
         {
            return -1;
         }

         size_t total = 0;
         for (int i = 0; i < frames - 1; ++i)
         {
            size_t sz = 0;
            while (p < end)
            {
               uint8_t b = *p++;
               sz += b;
               if (b != 0xFF)
               {
                  break;
               }
            }
            sizes[i] = sz;
            total += sz;
         }
         if (p > end)
         {
            free(sizes);
            return -1;
         }
         size_t remaining = (size_t)(end - p);
         if (remaining < total)
         {
            free(sizes);
            return -1;
         }
         sizes[frames - 1] = remaining - total;

         const uint8_t* q = p;
         for (int i = 0; i < frames; ++i)
         {
            size_t sz = sizes[i];
            if (q + sz > end)
            {
               free(sizes);
               return -1;
            }
            uint8_t* pcm = NULL;
            size_t pcm_bytes = 0;
            if (opus_decode_packet(m, q, sz, &pcm, &pcm_bytes) < 0)
            {
               free(sizes);
               return -1;
            }
            if (pcm && pcm_bytes)
            {
               if (enqueue_pcm_bytes(m, pcm, pcm_bytes, base_pts_ns, keyframe) < 0)
               {
                  free(pcm);
                  free(sizes);
                  return -1;
               }
               free(pcm);
            }
            q += sz;
         }
         free(sizes);
         return 0;
      }
      else
      {
         return 0;
      }
   }
   else if (m->ainfo.codec == MKV_CODEC_AAC)
   {
      if (lacing == 0)
      {
         size_t pkt_sz = (size_t)(end - p);
         if (pkt_sz == 0)
         {
            return 0;
         }
         uint8_t* pcm = NULL;
         size_t pcm_bytes = 0;
         if (aac_decode_packet(m, p, pkt_sz, &pcm, &pcm_bytes) < 0)
         {
            return -1;
         }
         if (pcm && pcm_bytes)
         {
            if (enqueue_pcm_bytes(m, pcm, pcm_bytes, base_pts_ns, keyframe) < 0)
            {
               free(pcm);
               return -1;
            }
            free(pcm);
         }
         return 0;
      }
      else if (lacing == 1)
      {
         if (p >= end)
         {
            return -1;
         }
         uint8_t lace_count = *p++;
         int frames = 1 + lace_count;
         if (frames <= 0)
         {
            return -1;
         }
         size_t* sizes = (size_t*)calloc((size_t)frames, sizeof(size_t));
         if (!sizes)
         {
            return -1;
         }

         size_t total = 0;
         for (int i = 0; i < frames - 1; ++i)
         {
            size_t sz = 0;
            while (p < end)
            {
               uint8_t b = *p++;
               sz += b;
               if (b != 0xFF)
               {
                  break;
               }
            }
            sizes[i] = sz;
            total += sz;
         }
         if (p > end)
         {
            free(sizes);
            return -1;
         }
         size_t remaining = (size_t)(end - p);
         if (remaining < total)
         {
            free(sizes);
            return -1;
         }
         sizes[frames - 1] = remaining - total;

         const uint8_t* q = p;
         for (int i = 0; i < frames; ++i)
         {
            size_t sz = sizes[i];
            if (q + sz > end)
            {
               free(sizes);
               return -1;
            }
            uint8_t* pcm = NULL;
            size_t pcm_bytes = 0;
            if (aac_decode_packet(m, q, sz, &pcm, &pcm_bytes) < 0)
            {
               free(sizes);
               return -1;
            }
            if (pcm && pcm_bytes)
            {
               if (enqueue_pcm_bytes(m, pcm, pcm_bytes, base_pts_ns, keyframe) < 0)
               {
                  free(pcm);
                  free(sizes);
                  return -1;
               }
               free(pcm);
            }
            q += sz;
         }
         free(sizes);
         return 0;
      }
      else
      {
         return 0;
      }
   }
   else
   {
      if (lacing == 0)
      {
         size_t sz = (size_t)(end - p);
         if (sz == 0)
         {
            return 0;
         }
         uint8_t* buf = (uint8_t*)malloc(sz);
         if (!buf)
         {
            return -1;
         }
         memcpy(buf, p, sz);
         if (pq_push(&m->q, buf, sz, base_pts_ns, keyframe) < 0)
         {
            free(buf);
            return -1;
         }
         return 0;
      }
      else if (lacing == 1)
      {
         if (p >= end)
         {
            return -1;
         }
         uint8_t lace_count = *p++;
         int frames = 1 + lace_count;
         if (frames <= 0)
         {
            return -1;
         }
         size_t* sizes = (size_t*)calloc((size_t)frames, sizeof(size_t));
         if (!sizes)
         {
            return -1;
         }

         size_t total = 0;
         for (int i = 0; i < frames - 1; ++i)
         {
            size_t sz = 0;
            while (p < end)
            {
               uint8_t b = *p++;
               sz += b;
               if (b != 0xFF)
               {
                  break;
               }
            }
            sizes[i] = sz;
            total += sz;
         }
         if (p > end)
         {
            free(sizes);
            return -1;
         }
         size_t remaining = (size_t)(end - p);
         if (remaining < total)
         {
            free(sizes);
            return -1;
         }
         sizes[frames - 1] = remaining - total;

         const uint8_t* q = p;
         for (int i = 0; i < frames; ++i)
         {
            size_t sz = sizes[i];
            if (q + sz > end)
            {
               free(sizes);
               return -1;
            }
            uint8_t* buf = (uint8_t*)malloc(sz);
            if (!buf)
            {
               free(sizes);
               return -1;
            }
            memcpy(buf, q, sz);
            q += sz;
            if (pq_push(&m->q, buf, sz, base_pts_ns, keyframe) < 0)
            {
               free(buf);
               free(sizes);
               return -1;
            }
         }
         free(sizes);
         return 0;
      }
      else
      {
         return 0;
      }
   }
}

static int
parse_cluster(MkvDemuxer* m, uint64_t elem_end)
{
   m->current_cluster_tc = 0;

   while (1)
   {
      if (elem_end && ebml_tell(&m->r) >= elem_end)
      {
         break;
      }
      uint32_t id;
      uint64_t size;
      int hdr = ebml_read_element_header(&m->r, &id, &size);
      if (hdr < 0)
      {
         return -1;
      }

      if (id == ID_CLUSTERTIMECODE)
      {
         uint64_t tc = 0;
         if (ebml_read_uint(&m->r, size, &tc) < 0)
         {
            return -1;
         }
         m->current_cluster_tc = tc;
      }
      else if (id == ID_SIMPLEBLOCK)
      {
         uint8_t* buf = NULL;
         if (ebml_read_binary(&m->r, size, &buf) < 0)
         {
            return -1;
         }
         int rc = read_block_into_queue(m, buf, size, 1);
         free(buf);
         if (rc < 0)
         {
            return -1;
         }
      }
      else if (id == ID_BLOCKGROUP)
      {
         uint64_t bg_end = (size == (uint64_t)-1) ? 0 : ebml_tell(&m->r) + size;
         while (1)
         {
            if (bg_end && ebml_tell(&m->r) >= bg_end)
            {
               break;
            }
            uint32_t bid;
            uint64_t bsz;
            if (ebml_read_element_header(&m->r, &bid, &bsz) < 0)
            {
               return -1;
            }
            if (bid == ID_BLOCK)
            {
               uint8_t* buf = NULL;
               if (ebml_read_binary(&m->r, bsz, &buf) < 0)
               {
                  return -1;
               }
               int rc = read_block_into_queue(m, buf, bsz, 0);
               free(buf);
               if (rc < 0)
               {
                  return -1;
               }
            }
            else
            {
               if (bsz == (uint64_t)-1)
               {
                  break;
               }
               if (ebml_skip(&m->r, bsz) < 0)
               {
                  return -1;
               }
            }
         }
      }
      else
      {
         if (size == (uint64_t)-1)
         {
            break;
         }
         if (ebml_skip(&m->r, size) < 0)
         {
            return -1;
         }
      }
   }

   return 0;
}

static int
parse_tracks(MkvDemuxer* m, uint64_t elem_end)
{
   while (1)
   {
      if (elem_end && ebml_tell(&m->r) >= elem_end)
      {
         break;
      }
      uint32_t id;
      uint64_t size;
      if (ebml_read_element_header(&m->r, &id, &size) < 0)
      {
         return -1;
      }
      if (id != ID_TRACKENTRY)
      {
         if (size == (uint64_t)-1)
         {
            return 0;
         }
         if (ebml_skip(&m->r, size) < 0)
         {
            return -1;
         }
         continue;
      }
      uint64_t entry_end = (size == (uint64_t)-1) ? 0 : ebml_tell(&m->r) + size;
      uint64_t track_number = 0;
      int track_type = 0;
      char codec_id[64] = {0};
      uint8_t* codec_priv = NULL;
      size_t codec_priv_sz = 0;
      double sampling = 0.0;
      uint64_t channels = 0;
      uint64_t bitdepth = 0;

      while (1)
      {
         if (entry_end && ebml_tell(&m->r) >= entry_end)
         {
            break;
         }
         uint32_t cid;
         uint64_t csz;
         if (ebml_read_element_header(&m->r, &cid, &csz) < 0)
         {
            return -1;
         }

         if (cid == ID_TRACKNUMBER)
         {
            if (ebml_read_uint(&m->r, csz, &track_number) < 0)
            {
               return -1;
            }
         }
         else if (cid == ID_TRACKTYPE)
         {
            uint64_t tt = 0;
            if (ebml_read_uint(&m->r, csz, &tt) < 0)
            {
               return -1;
            }
            track_type = (int)tt;
         }
         else if (cid == ID_CODECID)
         {
            if (ebml_read_string(&m->r, csz, codec_id, sizeof(codec_id)) < 0)
            {
               return -1;
            }
         }
         else if (cid == ID_CODECPRIVATE)
         {
            if (ebml_read_binary(&m->r, csz, &codec_priv) < 0)
            {
               return -1;
            }
            codec_priv_sz = (size_t)csz;
         }
         else if (cid == ID_AUDIO)
         {
            uint64_t audio_end = (csz == (uint64_t)-1) ? 0 : ebml_tell(&m->r) + csz;
            while (1)
            {
               if (audio_end && ebml_tell(&m->r) >= audio_end)
               {
                  break;
               }
               uint32_t aid;
               uint64_t asz;
               if (ebml_read_element_header(&m->r, &aid, &asz) < 0)
               {
                  return -1;
               }
               if (aid == ID_SAMPLINGFREQ)
               {
                  double f = 0.0;
                  if (ebml_read_float(&m->r, asz, &f) < 0)
                  {
                     return -1;
                  }
                  sampling = f;
               }
               else if (aid == ID_CHANNELS)
               {
                  if (ebml_read_uint(&m->r, asz, &channels) < 0)
                  {
                     return -1;
                  }
               }
               else if (aid == ID_BITDEPTH)
               {
                  if (ebml_read_uint(&m->r, asz, &bitdepth) < 0)
                  {
                     return -1;
                  }
               }
               else
               {
                  if (asz == (uint64_t)-1)
                  {
                     break;
                  }
                  if (ebml_skip(&m->r, asz) < 0)
                  {
                     return -1;
                  }
               }
            }
         }
         else
         {
            if (csz == (uint64_t)-1)
            {
               break;
            }
            if (ebml_skip(&m->r, csz) < 0)
            {
               return -1;
            }
         }
      }

      if (track_type == TRACK_TYPE_AUDIO && m->track_number == 0)
      {
         m->track_number = track_number;
         m->ainfo.track_number = track_number;
         m->ainfo.codec = codec_from_id(codec_id);
         hrmp_snprintf(m->ainfo.codec_id_str, sizeof(m->ainfo.codec_id_str), "%s", codec_id);
         m->ainfo.codec_private = codec_priv;
         m->ainfo.codec_private_size = codec_priv_sz;

         if (m->ainfo.codec == MKV_CODEC_PCM_INT || m->ainfo.codec == MKV_CODEC_PCM_FLOAT)
         {
            m->ainfo.sample_rate = sampling;
            m->ainfo.channels = (uint8_t)(channels > 255 ? 255 : channels);
            m->ainfo.bit_depth = (uint8_t)(bitdepth > 255 ? 255 : bitdepth);
         }
         else if (m->ainfo.codec == MKV_CODEC_OPUS)
         {
            if (init_opus_from_codec_private(m, codec_priv, codec_priv_sz) < 0)
            {
               return -1;
            }
         }
         else if (m->ainfo.codec == MKV_CODEC_AAC)
         {
            if (init_aac_from_codec_private(m, codec_priv, codec_priv_sz) < 0)
            {
               if (init_aac_decoder(m) < 0)
               {
                  return -1;
               }
            }
            if (sampling > 0.0)
            {
               m->ainfo.sample_rate = sampling;
            }
            if (channels > 0)
            {
               m->ainfo.channels = (uint8_t)(channels > 255 ? 255 : channels);
            }
            m->ainfo.bit_depth = AAC_OUTPUT_BITS;
         }
         else
         {
            m->ainfo.sample_rate = sampling;
            m->ainfo.channels = (uint8_t)(channels > 255 ? 255 : channels);
            m->ainfo.bit_depth = 0;
         }

         m->ainfo.timecode_scale_ns = m->timecode_scale_ns;
      }
      else
      {
         free(codec_priv);
      }
   }
   return 0;
}

static int
parse_header_and_segment(MkvDemuxer* m)
{
   {
      uint32_t id;
      uint64_t size;
      if (ebml_read_element_header(&m->r, &id, &size) < 0)
      {
         return -1;
      }
      if (id != ID_EBML)
      {
         return -1;
      }
      if (size != (uint64_t)-1)
      {
         if (ebml_skip(&m->r, size) < 0)
         {
            return -1;
         }
      }
      else
      {
         return -1;
      }
   }

   {
      uint32_t id;
      uint64_t size;
      if (ebml_read_element_header(&m->r, &id, &size) < 0)
      {
         return -1;
      }
      if (id != ID_SEGMENT)
      {
         return -1;
      }
      m->segment_start = ebml_tell(&m->r);
      m->segment_size = size;
   }

   int got_info = 0, got_tracks = 0;
   while (!(got_info && got_tracks))
   {
      uint32_t id;
      uint64_t size;
      int hdr = ebml_read_element_header(&m->r, &id, &size);
      if (hdr < 0)
      {
         return -1;
      }

      uint64_t elem_start = ebml_tell(&m->r);
      uint64_t elem_end = (size == (uint64_t)-1) ? 0 : elem_start + size;

      if (id == ID_INFO)
      {
         if (parse_info(m, elem_end) < 0)
         {
            return -1;
         }
         got_info = 1;
      }
      else if (id == ID_TRACKS)
      {
         if (parse_tracks(m, elem_end) < 0)
         {
            return -1;
         }
         got_tracks = 1;
      }
      else
      {
         if (size == (uint64_t)-1)
         {
            /* Unknown-sized non-cluster not expected; stop here. */
            break;
         }
         if (ebml_skip(&m->r, size) < 0)
         {
            return -1;
         }
      }
   }

   return (m->track_number != 0) ? 0 : -1;
}
