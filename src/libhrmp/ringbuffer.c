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

#include <ringbuffer.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static size_t
clamp_size(size_t v, size_t lo, size_t hi)
{
   if (v < lo)
   {
      return lo;
   }
   if (v > hi)
   {
      return hi;
   }
   return v;
}

static int
resize_to(struct ringbuffer* rb, size_t newcap)
{
   if (!rb)
   {
      return 1;
   }

   newcap = clamp_size(newcap, rb->min, rb->max);
   if (newcap == rb->cap)
   {
      return 0;
   }
   if (rb->size > newcap)
   {
      return 1;
   }

   uint8_t* nb = (uint8_t*)malloc(newcap);
   if (!nb)
   {
      return 1;
   }

   if (rb->size)
   {
      size_t first = rb->cap - rb->r;
      if (first > rb->size)
      {
         first = rb->size;
      }
      memcpy(nb, rb->buf + rb->r, first);
      if (first < rb->size)
      {
         memcpy(nb + first, rb->buf, rb->size - first);
      }
   }

   free(rb->buf);
   rb->buf = nb;
   rb->cap = newcap;
   rb->r = 0;
   rb->w = rb->size;
   if (rb->w >= rb->cap)
   {
      rb->w = 0;
   }

   return 0;
}

int
hrmp_ringbuffer_create(size_t min_size, size_t initial_size, size_t max_size, struct ringbuffer** out)
{
   struct ringbuffer* rb = NULL;

   *out = NULL;

   if (min_size == 0 || max_size == 0 || min_size > max_size)
   {
      goto error;
   }

   rb = (struct ringbuffer*)calloc(1, sizeof(struct ringbuffer));
   if (!rb)
   {
      goto error;
   }

   rb->min = min_size;
   rb->max = max_size;

   if (initial_size <= min_size)
   {
      initial_size = min_size;
   }
   rb->cap = clamp_size(initial_size, min_size, max_size);

   rb->buf = (uint8_t*)malloc(rb->cap);
   if (!rb->buf)
   {
      goto error;
   }

   *out = rb;
   return 0;

error:

   free(rb);
   return 1;
}

void
hrmp_ringbuffer_destroy(struct ringbuffer* rb)
{
   if (rb != NULL)
   {
      free(rb->buf);
      free(rb);
   }

   rb = NULL;
}

void
hrmp_ringbuffer_reset(struct ringbuffer* rb)
{
   if (rb != NULL)
   {
      rb->r = 0;
      rb->w = 0;
      rb->size = 0;

      if (rb->cap > rb->min)
      {
         resize_to(rb, rb->min);
      }
   }
}

size_t
hrmp_ringbuffer_capacity(struct ringbuffer* rb)
{
   return rb ? rb->cap : 0;
}

size_t
hrmp_ringbuffer_size(struct ringbuffer* rb)
{
   return rb ? rb->size : 0;
}

int
hrmp_ringbuffer_ensure_write(struct ringbuffer* rb, size_t n)
{
   if (rb == NULL || n > rb->max)
   {
      goto error;
   }

   size_t free_space = rb->cap - rb->size;
   if (free_space >= n)
   {
      return 0;
   }

   size_t need_total = rb->size + n;
   if (need_total > rb->max)
   {
      goto error;
   }

   size_t newcap = rb->cap;
   while (newcap < need_total)
   {
      if (newcap >= rb->max)
      {
         newcap = rb->max;
         break;
      }
      size_t doubled = newcap * 2u;
      newcap = (doubled < newcap) ? rb->max : doubled;
      if (newcap > rb->max)
      {
         newcap = rb->max;
      }
   }

   return resize_to(rb, newcap);

error:

   return 1;
}

size_t
hrmp_ringbuffer_peek(struct ringbuffer* rb, void** ptr)
{
   if (!rb || !ptr)
   {
      return 0;
   }
   *ptr = NULL;

   if (rb->size == 0)
   {
      return 0;
   }

   size_t n = rb->cap - rb->r;
   if (n > rb->size)
   {
      n = rb->size;
   }

   *ptr = rb->buf + rb->r;
   return n;
}

void
hrmp_ringbuffer_consume(struct ringbuffer* rb, size_t n)
{
   if (rb == NULL || rb->size == 0)
   {
      return;
   }

   if (n > rb->size)
   {
      n = rb->size;
   }

   rb->r = (rb->r + n) % rb->cap;
   rb->size -= n;

   if (rb->size == 0)
   {
      rb->r = 0;
      rb->w = 0;
   }
}

size_t
hrmp_ringbuffer_get_write_span(struct ringbuffer* rb, void** ptr)
{
   if (!rb || !ptr)
   {
      return 0;
   }
   *ptr = NULL;

   size_t free_space = rb->cap - rb->size;
   if (free_space == 0)
   {
      return 0;
   }

   size_t n = rb->cap - rb->w;
   if (n > free_space)
   {
      n = free_space;
   }

   *ptr = rb->buf + rb->w;
   return n;
}

int
hrmp_ringbuffer_produce(struct ringbuffer* rb, size_t n)
{
   if (rb == NULL)
   {
      goto error;
   }

   size_t free_space = rb->cap - rb->size;
   if (n > free_space)
   {
      goto error;
   }

   rb->w = (rb->w + n) % rb->cap;
   rb->size += n;
   return 0;

error:

   return 1;
}
