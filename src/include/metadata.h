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

#ifndef HRMP_METADATA_H
#define HRMP_METADATA_H

#include <hrmp.h>
#include <files.h>

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @struct metadata
 * Defines metadata
 */
struct metadata
{
   char* path;        /**< The file path */

   char* title;       /**< The title */
   char* artist;      /**< The artist */
   char* album;       /**< The album */
   char* genre;       /**< The genre */
   char* comment;     /**< The comment */
   char* date;        /**< The date */

   int track;         /**< The track */
   int disc;          /**< The disc */

   char* format_name; /**< The format */
   char* codec_name;  /**< The codec */
   int duration_ms;   /**< The duration in ms */
   int sample_rate;   /**< The sample rate */
   int channels;      /**< The number of channels */
   int bit_rate;      /**< The bit rate */
};

/**
 * Create the metadata for a file
 * @param fm The file metadata
 * @param metadata The track metadata
 * @return 0 upon success, otherwise 1
 */
int
hrmp_metadata_create(struct file_metadata* fm, struct metadata** metadata);

/**
 * Print metadata
 * @param metadata The metadata
 */
void
hrmp_metadata_print(struct metadata* metadata);

/**
 * Destroy metadata
 * @param metadata The metadata
 */
void
hrmp_metadata_destroy(struct metadata* metadata);

#ifdef __cplusplus
}
#endif

#endif
