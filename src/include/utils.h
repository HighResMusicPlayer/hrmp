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

#ifndef HRMP_UTILS_H
#define HRMP_UTILS_H

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#include <hrmp.h>
#include <list.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * Read an unit64_t in little-endian format
 * @param f The file
 * @return The result
 */
uint64_t
hrmp_read_le_u64(FILE* f);

/**
 * Read an unit32_t in little-endian format
 * @param f The file
 * @return The result
 */
uint32_t
hrmp_read_le_u32(FILE* f);

/**
 * Read an uint64_t in big-endian format
 * @param f The file
 * @return The result
 */
uint64_t
hrmp_read_be_u64(FILE* f);

/**
 * Read an uint32_t in big-endian format
 * @param f The file
 * @return The result
 */
uint32_t
hrmp_read_be_u32(FILE* f);

/**
 * Read an uint16_t in big-endian format
 * @param f The file
 * @return The result
 */
uint16_t
hrmp_read_be_u16(FILE* f);

/**
 * Read an unit64_t in little-endian format
 * @param buffer The buffer
 * @return The result
 */
uint64_t
hrmp_read_le_u64_buffer(uint8_t* buffer);

/**
 * Read an unit32_t in little-endian format
 * @param f The file
 * @return The result
 */
uint32_t
hrmp_read_le_u32_buffer(uint8_t* buffer);

/**
 * printf-style formatter that builds the result using hrmp_append helpers.
 * The output is clamped to the smaller of (HRMP_SNPRINTF_MAX_LENGTH) and (n-1).
 * Returns the number of characters that would have been written (excluding the
 * NUL byte), similar to the C library formatter. If buf is not NULL and n > 0, the output is
 * NUL-terminated.
 *
 * Supported format specifiers: %% %s %c %d %i %u %ld %lu %lld %llu %zu %zd %x
 * %X %p %f %g
 *
 * @param buf The destination buffer (may be NULL if n == 0)
 * @param n The size of the destination buffer
 * @param fmt The format string
 * @param ... The format arguments
 * @return Number of characters that would have been written (excluding the NUL
 * byte)
 */
int
hrmp_snprintf(char* buf, size_t n, const char* fmt, ...);

/**
 * Copy a string
 * @param s The string
 * @return The result
 */
char*
hrmp_copy_string(char* s);

/**
 * Get the home directory of the user
 * @return The result
 */
char*
hrmp_get_home_directory(void);

/**
 * Get the size of the file
 * @param file_path The path of the file
 * @return The size or 0 if it doesn't exists
 */
size_t
hrmp_get_file_size(char* file_path);

/**
 * File/directory exists
 * @param f The file/directory
 * @return The result
 */
bool
hrmp_exists(char* f);

/**
 * Is the path a directory
 * @param directory The directory
 * @return The result
 */
bool
hrmp_is_directory(char* directory);

/**
 * Is the path a file
 * @param file The file
 * @return The result
 */
bool
hrmp_is_file(char* file);

/**
 * Get the files of a directory
 * @param base The directory
 * @param recursive Should we recurse down
 * @param files The files
 * @return The result
 */
int
hrmp_get_files(char* base, bool recursive, struct list* files);

/**
 * Does a string start with another string
 * @param str The string
 * @param prefix The prefix
 * @return The result
 */
bool
hrmp_starts_with(char* str, char* prefix);

/**
 * Does a string end with another string
 * @param str The string
 * @param suffix The suffix
 * @return The result
 */
bool
hrmp_ends_with(char* str, char* suffix);

/**
 * Does a string contain another string
 * @param str The string
 * @param s The search string
 * @return The result
 */
bool
hrmp_contains(char* str, char* s);

/**
 * Remove the first character of a string
 * @param str The string
 * @return The result
 */
char*
hrmp_remove_first(char* str);

/**
 * Remove the last character of a string
 * @param str The string
 * @return The result
 */
char*
hrmp_remove_last(char* str);

/**
 * Sort a string array
 * @param size The size of the array
 * @param array The array
 * @return The result
 */
void
hrmp_sort(size_t size, char** array);

/**
 * Append a string
 * @param orig The original string
 * @param s The string
 * @return The resulting string
 */
char*
hrmp_append(char* orig, char* s);

/**
 * Append a char
 * @param orig The original string
 * @param s The string
 * @return The resulting string
 */
char*
hrmp_append_char(char* orig, char c);

/**
 * Append an integer
 * @param orig The original string
 * @param i The integer
 * @return The resulting string
 */
char*
hrmp_append_int(char* orig, int i);

/**
 * Remove whitespace from a string
 * @param orig The original string
 * @return The resulting string
 */
char*
hrmp_remove_whitespace(char* orig);

/**
 * Compare two strings
 * @param str1 The first string
 * @param str2 The second string
 * @return true if the strings are the same, otherwise false
 */
bool
hrmp_compare_string(const char* str1, const char* str2);

/**
 * Indent a string
 * @param str The string
 * @param tag [Optional] The tag, which will be applied after indentation if not
 * NULL
 * @param indent The indent
 * @return The indented string
 */
char*
hrmp_indent(char* str, char* tag, int indent);

/**
 * Escape a string
 * @param str The original string
 * @return The escaped string
 */
char*
hrmp_escape_string(char* str);

/**
 * Get a memory aligned size
 * @param size The requested size
 * @return The aligned size
 */
size_t
hrmp_get_aligned_size(size_t size);

/**
 * Set process title.
 *
 * The function will autonomously check the update policy set
 * via the configuration option `update_process_title` and
 * will do nothing if the setting is `never`.
 * In the case the policy is set to `strict`, the process title
 * will not overflow the initial command line length (i.e., strlen(argv[*]))
 * otherwise it will do its best to set the title to the desired string.
 *
 * The policies `strict` and `minimal` will be honored only on Linux platforms
 * where a native call to set the process title is not available.
 *
 * The resulting process title will be set to either `s1` or `s1/s2` if there
 * both strings and the length is allowed by the policy.
 *
 * @param argc The number of arguments
 * @param argv The argv pointer
 * @param s The string
 */
void
hrmp_set_proc_title(int argc, char** argv, char* s);

/**
 * Generate a backtrace in the log
 * @return 0 if success, otherwise 1
 */
int
hrmp_backtrace(void);

/**
 * Get the backtrace
 * @param s The backtrace
 * @return 0 if success, otherwise 1
 */
int
hrmp_backtrace_string(char** s);

#ifdef __cplusplus
}
#endif

#endif
