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

#ifndef HRMP_RINGBUFFER_H
#define HRMP_RINGBUFFER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HRMP_RINGBUFFER_MIN_BYTES (4u * 1024u * 1024u)
#define HRMP_RINGBUFFER_MAX_BYTES (256u * 1024u * 1024u)

struct ringbuffer
{
   uint8_t *buf; /**< The buffer */
   size_t cap;   /**< The capacity */
   size_t min;   /**< The minimum size */
   size_t max;   /**< The maximum size */
   size_t r;     /**< The read position */
   size_t w;     /**< The write position */
   size_t size;  /**< The size */
};

/**
 * Create a ringbuffer
 * @param min_size The minimum size
 * @param initial_size The initial size
 * @param max_size The maximum size
 * @param out The ringbuffer
 * @return 0 upon success, otherwise 1
 */
int
hrmp_ringbuffer_create(size_t min_size, size_t initial_size, size_t max_size, struct ringbuffer **out);

/**
 * Destroy a ringbuffer
 * @param rb The ringbuffer
 * @return 0 upon success, otherwise 1
 */
void
hrmp_ringbuffer_destroy(struct ringbuffer *rb);

/**
 * Reset a ringbuffer
 * @param rb The ringbuffer
 * @return 0 upon success, otherwise 1
 */
void
hrmp_ringbuffer_reset(struct ringbuffer *rb);

/**
 * Get the capacity of a ringbuffer
 * @param rb The ringbuffer
 * @return The capacity
 */
size_t
hrmp_ringbuffer_capacity(struct ringbuffer *rb);

/**
 * Get the ringbuffer size
 * @param rb The ringbuffer
 * @return The size
 */
size_t hrmp_ringbuffer_size(struct ringbuffer *rb);

/**
 * Ensure the capacity of a ringbuffer
 * @param rb The ringbuffer
 * @param n The size
 * @return 0 upon success, otherwise 1
 */
int
hrmp_ringbuffer_ensure_write(struct ringbuffer *rb, size_t n);

/**
 * Peek into a ringbuffer
 * @param rb The ringbuffer
 * @param ptr The pointer
 * @return The remaining size
 */
size_t
hrmp_ringbuffer_peek(struct ringbuffer *rb, void **ptr);

/**
 * Consume from a ringbuffer
 * @param rb The ringbuffer
 * @param n The size
 */
void
hrmp_ringbuffer_consume(struct ringbuffer *rb, size_t n);

/**
 * Play back a file
 * @param number The file number
 * @param total The total number of files
 * @param fm The file metadata
 * @param next Are going forward or backward
 * @return 0 upon success, otherwise 1
 */
size_t
hrmp_ringbuffer_get_write_span(struct ringbuffer *rb, void **ptr);

/**
 * Produce the ringbuffer
 * @param rb The ringbuffer
 * @param n The size
 * @return 0 upon success, otherwise 1
 */
int
hrmp_ringbuffer_produce(struct ringbuffer *rb, size_t n);

#ifdef __cplusplus
}
#endif

#endif
