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

#ifndef HRMP_PLAYBACK_H
#define HRMP_PLAYBACK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <hrmp.h>
#include <files.h>

#include <sndfile.h>
#include <stdio.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>

/** @struct playback
 * Defines a playback
 */
struct playback
{
   int device;                    /**< The device */
   size_t file_size;              /**< The file size */
   int file_number;               /**< The file number */
   int total_number;              /**< The total number */
   char identifier[MISC_LENGTH];  /**< The file identifier */
   unsigned long current_samples; /**< The total number of samples */
   snd_pcm_t* pcm_handle;         /**< The PCM handle */
   struct file_metadata* fm;      /**< The file metadata */
};

/**
 * Play back a file
 * @param device The device
 * @param number The file number
 * @param total The total number of files
 * @param fm The file metadata
 * @return 0 upon success, otherwise 1
 */
int
hrmp_playback(int device, int number, int total, struct file_metadata* fm);

#ifdef __cplusplus
}
#endif

#endif
