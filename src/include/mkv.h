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
