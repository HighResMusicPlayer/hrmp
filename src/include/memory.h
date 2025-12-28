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
   size_t size;   /**< allocated buffer size */
   size_t start;  /**< offset to the first unconsumed data in buffer */
   size_t end;    /**< offset to the first position after available data */
   size_t cursor; /**< next byte to consume */
} __attribute__((aligned(64)));

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
