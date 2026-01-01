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

#ifndef HRMP_PLAYBACK_H
#define HRMP_PLAYBACK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <hrmp.h>
#include <files.h>
#include <ringbuffer.h>

#include <sndfile.h>
#include <stdio.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>

/** @struct playback
 * Defines a playback
 */
struct playback
{
   size_t file_size;              /**< The file size */
   int file_number;               /**< The file number */
   int total_number;              /**< The total number */
   char identifier[MISC_LENGTH];  /**< The file identifier */
   unsigned long current_samples; /**< The total number of samples */
   snd_pcm_t* pcm_handle;         /**< The PCM handle */
   struct file_metadata* fm;      /**< The file metadata */
   struct ringbuffer* rb;         /**< Optional ringbuffer for file-backed reads */
   uint64_t bytes_left;           /**< Bytes left in current file segment (if known) */
};

/**
 * Play back a file
 * @param number The file number
 * @param total The total number of files
 * @param fm The file metadata
 * @param next Are going forward or backward
 * @return 0 upon success, otherwise 1
 */
int
hrmp_playback(int number, int total, struct file_metadata* fm, bool* next);

#ifdef __cplusplus
}
#endif

#endif
