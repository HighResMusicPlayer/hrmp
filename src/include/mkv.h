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

#ifndef HRMP_MKV_H
#define HRMP_MKV_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MkvDemuxer MkvDemuxer;

typedef enum {
   MKV_CODEC_UNKNOWN = 0,
   MKV_CODEC_VORBIS,
   MKV_CODEC_OPUS,
   MKV_CODEC_FLAC,
   MKV_CODEC_AAC,
   MKV_CODEC_PCM_INT,
   MKV_CODEC_PCM_FLOAT
} MkvCodecId;

typedef struct
{
   MkvCodecId codec;
   char codec_id_str[64];
   double sample_rate;
   uint8_t channels;
   uint8_t bit_depth;
   uint8_t* codec_private;
   size_t codec_private_size;
   uint64_t track_number;
   uint64_t timecode_scale_ns;
} MkvAudioInfo;

typedef struct
{
   uint8_t* data;
   size_t size;
   int64_t pts_ns;
   int keyframe;
} MkvPacket;

int hrmp_mkv_open(FILE* fp, MkvDemuxer** out);

int hrmp_mkv_open_path(const char* path, MkvDemuxer** out);

void hrmp_mkv_close(MkvDemuxer* m);

int hrmp_mkv_get_audio_info(MkvDemuxer* m, MkvAudioInfo* out_info);

int hrmp_mkv_read_packet(MkvDemuxer* m, MkvPacket* packet);

void
hrmp_mkv_free_packet(MkvPacket* packet);

#ifdef __cplusplus
}
#endif

#endif /* HRMP_MKV_H */
