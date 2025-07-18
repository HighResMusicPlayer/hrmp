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

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wav_header
{
   uint32_t chunk_id;
   uint32_t chunk_size;
   uint32_t format;
   uint32_t subchunk1_id;
   uint32_t subchunk1_size;
   uint16_t audio_format;
   uint16_t num_channels;
   uint32_t sample_rate;
   uint32_t byte_rate;
   uint16_t block_align;
   uint16_t bits_per_sample;
   uint32_t subchunk2_id;
   uint32_t subchunk2_size;
};

enum wav_channel_format
{
   WAV_INTERLEAVED, /* [LRLRLRLR] */
   WAV_INLINE,      /* [LLLLRRRR] */
   WAV_SPLIT        /* [[LLLL],[RRRR]] */
};

enum wav_sample_format
{
   WAV_INT16 = 2,  /* two byte signed integer */
   WAV_FLOAT32 = 4 /* four byte IEEE float */
};

struct wav
{
   FILE* file;
   struct wav_header header;
   int32_t number_of_frames;
   uint32_t total_frames_read;
   enum wav_channel_format channel_format;
   enum wav_sample_format sample_format;
};

/**
 * Open a WAV file for reading.
 * @param path     The path of the file to read.
 * @param chanfmt  The channel format
 * @return 0 upon success, other an error code
 */
int
hrmp_wav_open(char* path, enum wav_channel_format chanfmt, struct wav** w);

/**
 * Read sample data from the file.
 * @param wav   The WAV structure
 * @param data  A pointer to the data structure to read to
 * @param len   The number of frames to read
 * @return The number of frames (samples per channel) read from file
 */
int
hrmp_wav_read(struct wav* wav, void* data, int len);

/**
 * Close the WAV File
 * @param wav The WAV structure
 */
void
hrmp_wav_close(struct wav* wav);

#ifdef __cplusplus
}
#endif

#endif
