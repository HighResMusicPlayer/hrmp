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

#ifndef HRMP_WAV_H
#define HRMP_WAV_H

#include <hrmp.h>
#include <files.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @struct wav_header
 * The WAV header
 */
struct wav_header
{
   char chunk_id[4];         /**< The chunk identifier */
   uint32_t chunk_size;      /**< The chunk size */
   char format[4];           /**< The format */
   uint32_t subchunk1_id;    /**< The subchunk1 identifier */
   uint32_t subchunk1_size;  /**< The subchunk1 size */
   uint16_t audio_format;    /**< The audio format */
   uint16_t channels;        /**< The number of channels */
   uint32_t sample_rate;     /**< The sample rate */
   uint32_t byte_rate;       /**< The byte rate */
   uint16_t block_align;     /**< The block align */
   uint16_t bits_per_sample; /**< The bits per sample */
   uint32_t subchunk2_id;    /**< The subchunk2 identifier */
   uint32_t subchunk2_size;  /**< The subchunk2 size */
};

/** @enum wav_channel_format
 * The WAV channel format
 */
enum wav_channel_format
{
   WAV_INTERLEAVED, /**< [LRLRLRLR] */
   WAV_INLINE,      /**< [LLLLRRRR] */
   WAV_SPLIT        /**< [[LLLL],[RRRR]] */
};

/** @enum wav_sample_format
 * The WAV sample format
 */
enum wav_sample_format
{
   WAV_INT16 = 2,  /**< Two byte signed integer */
   WAV_FLOAT32 = 4 /**< Four byte IEEE float */
};

/** @struct wav
 * The WAV structure
 */
struct wav
{
   FILE* file;                             /**< The WAV file */
   struct wav_header header;               /**< The WAV header */
   int32_t number_of_frames;               /**< The number of frames */
   uint32_t total_frames_read;             /**< The number of frames read */
   enum wav_channel_format channel_format; /**< The channel format */
   enum wav_sample_format sample_format;   /**< The sample format */
   void* buffer;                           /**< The buffer */
   size_t buffer_size;                     /**< The buffer size */
};

/**
 * WAV: Get metadata for a file
 * @param filename The file name
 * @param file_metadata The file metadata
 * @return 0 upon success, 1 is failure
 */
int
hrmp_wav_get_metadata(char* filename, struct file_metadata** file_metadata);

/**
 * WAV: Open a WAV file
 * @param filename The file name
 * @param chanfmt The channel format
 * @param w The WAV structure
 * @return 0 upon success, 1 is failure
 */
int
hrmp_wav_open(char* filename, enum wav_channel_format chanfmt, struct wav** w);

/**
 * WAV: Close a WAV file
 * @param wav The WAV structure
 */
void
hrmp_wav_close(struct wav* wav);

#ifdef __cplusplus
}
#endif

#endif
