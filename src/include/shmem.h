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

#ifndef HRMP_SHMEM_H
#define HRMP_SHMEM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

/**
 * Create a shared memory segment
 * @param size The size of the segment
 * @parma shmem The shared memory segment
 * @return 0 upon success, otherwise 1
 */
int
hrmp_create_shared_memory(size_t size, void** shmem);

/**
 * Destroy a shared memory segment
 * @param shmem The shared memory segment
 * @param size The size
 * @return 0 upon success, otherwise 1
 */
int
hrmp_destroy_shared_memory(void* shmem, size_t size);

#ifdef __cplusplus
}
#endif

#endif
