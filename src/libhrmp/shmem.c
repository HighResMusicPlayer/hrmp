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

/* hrmp */
#include <hrmp.h>
#include <shmem.h>

/* system */
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

void* shmem = NULL;

int
hrmp_create_shared_memory(size_t size, void** shmem)
{
   void* s = NULL;
   int protection = PROT_READ | PROT_WRITE;
   int visibility = MAP_ANONYMOUS | MAP_SHARED;

   *shmem = NULL;

   s = mmap(NULL, size, protection, visibility, -1, 0);
   if (s == (void*)-1)
   {
      errno = 0;
      s = NULL;
   }

   if (s == NULL)
   {
      visibility = MAP_ANONYMOUS | MAP_SHARED;
      s = mmap(NULL, size, protection, visibility, 0, 0);

      if (s == (void*)-1)
      {
         errno = 0;
         return 1;
      }
   }

   memset(s, 0, size);

   *shmem = s;

   return 0;
}

int
hrmp_destroy_shared_memory(void* shmem, size_t size)
{
   return munmap(shmem, size);
}
