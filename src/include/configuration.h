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
#define HRMP_MAIN_INI_SECTION "hrmp"

#define HRMP_CONFIGURATION_STATUS_OK 0
#define HRMP_CONFIGURATION_STATUS_FILE_NOT_FOUND -1
#define HRMP_CONFIGURATION_STATUS_FILE_TOO_BIG -2
#define HRMP_CONFIGURATION_STATUS_KO -3

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

/**
 * Sample configuration
 */
void
hrmp_sample_configuration(void);

#ifdef __cplusplus
}
#endif

#endif
