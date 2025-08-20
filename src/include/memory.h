/*
 * Copyright (C) 2025 The hrmp community
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

#ifndef HRMP_MEMORY_H
#define HRMP_MEMORY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <hrmp.h>

#include <stdlib.h>

/** @struct stream_buffer
 * Defines a streaming buffer
 */
struct stream_buffer
{
   char* buffer;  /**< allocated buffer holding streaming data */
   size_t size;      /**< allocated buffer size */
   int start;     /**< offset to the first unconsumed data in buffer */
   int end;       /**< offset to the first position after available data */
   int cursor;    /**< next byte to consume */
} __attribute__ ((aligned (64)));

/**
 * Initialize a stream buffer
 * @param buffer The stream buffer to be initialized
 */
void
hrmp_memory_stream_buffer_init(struct stream_buffer** buffer);

/**
 * Enlarge the buffer, doesn't guarantee success
 * @param buffer The stream buffer
 * @param bytes_needed The number of bytes needed
 * @return 0 upon success, otherwise 1
 */
int
hrmp_memory_stream_buffer_enlarge(struct stream_buffer* buffer, int bytes_needed);

/**
 * Free a stream buffer
 * @param buffer The stream buffer to be freed
 */
void
hrmp_memory_stream_buffer_free(struct stream_buffer* buffer);

#ifdef __cplusplus
}
#endif

#endif
