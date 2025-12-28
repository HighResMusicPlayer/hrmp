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

/* hrmp */
#include <hrmp.h>
#include <memory.h>
#include <utils.h>

/* system */
#ifdef DEBUG
#include <assert.h>
#endif
#include <stdlib.h>
#include <string.h>

void
hrmp_memory_stream_buffer_init(struct stream_buffer** buffer)
{
   struct stream_buffer* b = malloc(sizeof(struct stream_buffer));

   if (b == NULL)
   {
      *buffer = NULL;
      return;
   }

   b->size = DEFAULT_BUFFER_SIZE;
   b->start = b->end = b->cursor = 0;
   b->buffer = aligned_alloc((size_t)ALIGNMENT_SIZE, DEFAULT_BUFFER_SIZE);
   if (b->buffer == NULL)
   {
      free(b);
      *buffer = NULL;
      return;
   }
   *buffer = b;
}

int
hrmp_memory_stream_buffer_enlarge(struct stream_buffer* buffer, int bytes_needed)
{
   size_t new_size = 0;
   void* new_buffer = NULL;

   if (buffer->size + bytes_needed < buffer->size + DEFAULT_BUFFER_SIZE)
   {
      new_size = hrmp_get_aligned_size(buffer->size + DEFAULT_BUFFER_SIZE);
   }
   else
   {
      new_size = hrmp_get_aligned_size(buffer->size + bytes_needed);
   }

   if (buffer->size > new_size)
   {
      return 0;
   }

   new_buffer = aligned_alloc((size_t)ALIGNMENT_SIZE, new_size);

   if (new_buffer == NULL)
   {
      return 1;
   }

   memset(new_buffer, 0, new_size);
   memcpy(new_buffer, buffer->buffer, buffer->size);

   free(buffer->buffer);

   buffer->size = new_size;
   buffer->buffer = new_buffer;

   return 0;
}

void
hrmp_memory_stream_buffer_free(struct stream_buffer* buffer)
{
   if (buffer == NULL)
   {
      return;
   }
   if (buffer->buffer != NULL)
   {
      free(buffer->buffer);
      buffer->buffer = NULL;
   }
   free(buffer);
}
