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

#ifndef HRMP_LOGGING_H
#define HRMP_LOGGING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

#define HRMP_LOGGING_TYPE_CONSOLE 0
#define HRMP_LOGGING_TYPE_FILE    1
#define HRMP_LOGGING_TYPE_SYSLOG  2

#define HRMP_LOGGING_LEVEL_DEBUG5  1
#define HRMP_LOGGING_LEVEL_DEBUG4  1
#define HRMP_LOGGING_LEVEL_DEBUG3  1
#define HRMP_LOGGING_LEVEL_DEBUG2  1
#define HRMP_LOGGING_LEVEL_DEBUG1  2
#define HRMP_LOGGING_LEVEL_INFO    3
#define HRMP_LOGGING_LEVEL_WARN    4
#define HRMP_LOGGING_LEVEL_ERROR   5
#define HRMP_LOGGING_LEVEL_FATAL   6

#define HRMP_LOGGING_MODE_CREATE 0
#define HRMP_LOGGING_MODE_APPEND 1

#define HRMP_LOGGING_DEFAULT_LOG_LINE_PREFIX "%Y-%m-%d %H:%M:%S"

#define hrmp_log_trace(...) hrmp_log_line(HRMP_LOGGING_LEVEL_DEBUG5, __FILE__, __LINE__, __VA_ARGS__)
#define hrmp_log_debug(...) hrmp_log_line(HRMP_LOGGING_LEVEL_DEBUG1, __FILE__, __LINE__, __VA_ARGS__)
#define hrmp_log_info(...)  hrmp_log_line(HRMP_LOGGING_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define hrmp_log_warn(...)  hrmp_log_line(HRMP_LOGGING_LEVEL_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define hrmp_log_error(...) hrmp_log_line(HRMP_LOGGING_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define hrmp_log_fatal(...) hrmp_log_line(HRMP_LOGGING_LEVEL_FATAL, __FILE__, __LINE__, __VA_ARGS__)

/**
 * Initialize the logging system
 * @return 0 upon success, otherwise 1
 */
int
hrmp_init_logging(void);

/**
 * Start the logging system
 * @return 0 upon success, otherwise 1
 */
int
hrmp_start_logging(void);

/**
 * Stop the logging system
 * @return 0 upon success, otherwise 1
 */
int
hrmp_stop_logging(void);

/**
 * Log a line
 * @param level The level
 * @param file The file
 * @param line The line number
 * @param fmt The formatting code
 * @return 0 upon success, otherwise 1
 */
void
hrmp_log_line(int level, char* file, int line, char* fmt, ...);

/**
 * Log a memory segment
 * @param data The data
 * @param size The size
 * @return 0 upon success, otherwise 1
 */
void
hrmp_log_mem(void* data, size_t size);

/**
 * Opens the log file defined in the configuration.
 * Works only for a real log file, i.e., the configuration
 * must be set up to log to a file, not console.
 *
 * The function considers the settings in the configuration
 * to determine the mode (append, create) and the filename
 * to open.
 *
 * It sets the global variable 'log_file'.
 *
 * If it succeed in opening the log file, it calls
 * the log_rotation_set_next_rotation_age() function to
 * determine the next instant at which the log file
 * must be rotated. Calling such function is safe
 * because if the log rotation is disabled, the function
 * does nothing.
 *
 * @return 0 on success, 1 on error.
 */
int
log_file_open(void);

#ifdef __cplusplus
}
#endif

#endif
