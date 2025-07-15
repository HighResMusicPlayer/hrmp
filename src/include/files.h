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

#ifndef HRMP_FILES_H
#define HRMP_FILES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <hrmp.h>

#include <stdbool.h>
#include <stdlib.h>

#define TYPE_UNKNOWN 0
#define TYPE_WAV     1
#define TYPE_FLAC    2

/** @struct
 * Defines a file metadata
 */
struct file_metadata
{
   int type;                     /**< The type of file */
   char name[MISC_LENGTH];       /**< The name of the file */
   int format;                   /**< The format */
   unsigned int sample_rate;     /**< The sample rate */
   unsigned int channels;        /**< The number of channels */
   unsigned int bits_per_sample; /**< The bits per sample */
   unsigned long total_samples;  /**< The total number of samples */
   double duration;              /**< The number of seconds */
   unsigned int min_blocksize;   /**< The minimum blocksize */
   unsigned int max_blocksize;   /**< The maximum blocksize */
   unsigned int min_framesize;   /**< The minimum framesize */
   unsigned int max_framesize;   /**< The maximum framesize */
} __attribute__((aligned(64)));

/**
 * Is the file supported
 * @param f The file
 * @return The result type
 */
int
hrmp_is_file_supported(char* f);

/**
 * Is the file metadata supported
 * @param device The device
 * @param fm The file metadata
 * @return The result
 */
bool
hrmp_is_file_metadata_supported(int device, struct file_metadata* fm);

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
