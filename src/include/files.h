/*
 * Copyright (C) 2026 The HighResMusicPlayer community
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

#ifndef HRMP_FILES_H
#define HRMP_FILES_H

#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

#include <hrmp.h>

#include <stdbool.h>
#include <stdlib.h>

#define TYPE_UNKNOWN   0
#define TYPE_WAV       1
#define TYPE_FLAC      2
#define TYPE_MP3       3
#define TYPE_DSF       4
#define TYPE_DFF       5
#define TYPE_MKV       6

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
   unsigned long data_size;      /**< The data size */

   char title[MISC_LENGTH];  /**< Title */
   char artist[MISC_LENGTH]; /**< Artist */
   char album[MISC_LENGTH];  /**< Album */
   char genre[MISC_LENGTH];  /**< Genre */
   char date[MISC_LENGTH];   /**< Date */

   int track; /**< Track number (0 if unknown) */
   int disc;  /**< Disc number (0 if unknown) */
};

/**
 * Get the file metadata
 * @param f The file
 * @param fm The file metadata
 * @return The result or NULL if not supported
 */
int
hrmp_file_metadata(char* f, struct file_metadata** fm);

/**
 * Print the file metadata
 * @param fm The file metadata
 * @return The result
 */
int
hrmp_print_file_metadata(struct file_metadata* fm);

/**
 * Check if a file extension is supported by hrmp.
 * This only checks the filename extension.
 *
 * @param filename The filename
 * @return true if supported, otherwise false
 */
bool
hrmp_file_is_supported(char* filename);

#ifdef __cplusplus
}
#endif

#endif
