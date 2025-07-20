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

#ifndef HRMP_UTILS_H
#define HRMP_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <hrmp.h>

#include <stdbool.h>
#include <stdlib.h>

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
 * Remove whitespace from a string
 * @param orig The original string
 * @return The resulting string
 */
char*
hrmp_remove_whitespace(char* orig);

/**
 * Generate a backtrace in the log
 * @return 0 if success, otherwise 1
 */
int
hrmp_backtrace(void);

#ifdef __cplusplus
}
#endif

#endif
