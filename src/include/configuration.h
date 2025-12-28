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

#ifndef HRMP_CONFIGURATION_H
#define HRMP_CONFIGURATION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdlib.h>

/*
 * The main section that must be present in the `hrmp.conf`
 * configuration file.
 */
#define HRMP_MAIN_INI_SECTION                    "hrmp"

#define HRMP_CONFIGURATION_STATUS_OK             0
#define HRMP_CONFIGURATION_STATUS_FILE_NOT_FOUND -1
#define HRMP_CONFIGURATION_STATUS_FILE_TOO_BIG   -2
#define HRMP_CONFIGURATION_STATUS_KO             -3

/**
 * Initialize the configuration structure
 * @param shmem The shared memory segment
 * @return 0 upon success, otherwise 1
 */
int
hrmp_init_configuration(void* shmem);

/**
 * Read the configuration from a file
 * @param shmem The shared memory segment
 * @param filename The file name
 * @param emitWarnings Emit warnings
 * @return 0 upon success, otherwise 1
 */
int
hrmp_read_configuration(void* shmem, char* filename, bool emitWarnings);

/**
 * Validate the configuration
 * @param shmem The shared memory segment
 * @return 0 upon success, otherwise 1
 */
int
hrmp_validate_configuration(void* shmem);

#ifdef __cplusplus
}
#endif

#endif
