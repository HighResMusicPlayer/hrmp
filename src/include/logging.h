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

#ifndef HRMP_LOGGING_H
#define HRMP_LOGGING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <hrmp.h>

#include <stdbool.h>
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

#define HRMP_LOGGING_ROTATION_DISABLED 0

#define HRMP_LOGGING_DEFAULT_LOG_LINE_PREFIX "%Y-%m-%d %H:%M:%S"

#define hrmp_log_trace(...) hrmp_log_line(HRMP_LOGGING_LEVEL_DEBUG5, __FILE__, __LINE__, __VA_ARGS__)
#define hrmp_log_debug(...) hrmp_log_line(HRMP_LOGGING_LEVEL_DEBUG1, __FILE__, __LINE__, __VA_ARGS__)
#define hrmp_log_info(...)  hrmp_log_line(HRMP_LOGGING_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define hrmp_log_warn(...)  hrmp_log_line(HRMP_LOGGING_LEVEL_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define hrmp_log_error(...) hrmp_log_line(HRMP_LOGGING_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define hrmp_log_fatal(...) hrmp_log_line(HRMP_LOGGING_LEVEL_FATAL, __FILE__, __LINE__, __VA_ARGS__)

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
 * Is the logging level enabled
 * @param level The level
 * @return True if enabled, otherwise false
 */
bool
hrmp_log_is_enabled(int level);

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

#ifdef __cplusplus
}
#endif

#endif
