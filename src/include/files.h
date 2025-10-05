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

#ifndef HRMP_FILES_H
#define HRMP_FILES_H

#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

#include <hrmp.h>

#include <stdbool.h>
#include <stdlib.h>

#define TYPE_UNKNOWN 0
#define TYPE_WAV     1
#define TYPE_FLAC    2
#define TYPE_MP3     3
#define TYPE_DSF     4

#define FORMAT_UNKNOWN 0
#define FORMAT_16      1
#define FORMAT_24      2
#define FORMAT_32      3
#define FORMAT_1       4

/** @struct file_metadata
 * Defines a file metadata
 */
struct file_metadata
{
   int type;                     /**< The type of file */
   char name[MAX_PATH];          /**< The name of the file */
   int format;                   /**< The format of the file */
   size_t file_size;             /**< The file size */
   unsigned int sample_rate;     /**< The sample rate */
   unsigned int pcm_rate;        /**< The PCM rate */
   unsigned int channels;        /**< The number of channels */
   unsigned int bits_per_sample; /**< The bits per sample */
   unsigned long total_samples;  /**< The total number of samples */
   double duration;              /**< The number of seconds */
   int alsa_snd;                 /**< The ALSA sound identifier */
   int container;                /**< The container size */
   unsigned int block_size;      /**< The block size */
};

/**
 * Get the file metadata
 * @param device The device
 * @param f The file
 * @param fm The file metadata
 * @return The result or NULL if not supported
 */
int
hrmp_file_metadata(int device, char* f, struct file_metadata** fm);

/**
 * Print the file metadata
 * @param fm The file metadata
 * @return The result
 */
int
hrmp_print_file_metadata(struct file_metadata* fm);

#ifdef __cplusplus
}
#endif

#endif
