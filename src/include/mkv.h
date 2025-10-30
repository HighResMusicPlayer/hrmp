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

#include <hrmp.h>

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @struct MkvDemuxer
 * Defines a MKV demuxer
 */
typedef struct MkvDemuxer MkvDemuxer;

/** @enum MkvCodecId
 * Defines the MKV codec
 */
typedef enum {
   MKV_CODEC_UNKNOWN = 0, /**< Unknown */
   MKV_CODEC_VORBIS,      /**< Vorbis */
   MKV_CODEC_OPUS,        /**< Opus */
   MKV_CODEC_FLAC,        /**< FLAC */
   MKV_CODEC_AAC,         /**< AAC */
   MKV_CODEC_PCM_INT,     /**< PCM/int */
   MKV_CODEC_PCM_FLOAT    /**< PCM/float */
} MkvCodecId;

/** @struct MkvAudioInfo
 * Defines the information of the audio
 */
typedef struct
{
   MkvCodecId codec;           /**< The codec */
   char codec_id_str[64];      /**< The codec string */
   double sample_rate;         /**< Sample rate */
   uint8_t channels;           /**< Number of channels */
   uint8_t bit_depth;          /**< The bit depth */
   uint8_t* codec_private;     /**< Private codec */
   size_t codec_private_size;  /**< The size of the private codec */
   uint64_t track_number;      /**< The track number */
   uint64_t timecode_scale_ns; /**< The time code */
} MkvAudioInfo;

/** @struct MkvPacket
 * Defines a MKV packet
 */
typedef struct
{
   uint8_t* data;  /**< The data */
   size_t size;    /**< The size of the data */
   int64_t pts_ns; /**< The PTS */
   int keyframe;   /**< The key frame */
} MkvPacket;

/**
 * Open a MKV file
 * @param fp The file
 * @param out The demuxer
 * @return The result
 */
int
hrmp_mkv_open(FILE* fp, MkvDemuxer** out);

/**
 * Open a MKV file
 * @param path The file
 * @param out The demuxer
 * @return The result
 */
int
hrmp_mkv_open_path(const char* path, MkvDemuxer** out);

/**
 * Close a MKV file
 * @param m The demuxer
 */
void
hrmp_mkv_close(MkvDemuxer* m);

/**
 * Get the audio information
 * @param m The demuxer
 * @param out_info The information
 * @return The result
 */
int
hrmp_mkv_get_audio_info(MkvDemuxer* m, MkvAudioInfo* out_info);

/**
 * Read a packet
 * @param m The demuxer
 * @param packet The packet
 * @return The result
 */
int
hrmp_mkv_read_packet(MkvDemuxer* m, MkvPacket* packet);

/**
 * Free a packet
 * @param packet The packet
 */
void
hrmp_mkv_free_packet(MkvPacket* packet);

#ifdef __cplusplus
}
#endif

#endif
