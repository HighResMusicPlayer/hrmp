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

#ifndef HRMP_CONVERT_H
#define HRMP_CONVERT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/**
 * Build the default FLAC output path for a DSF source file.
 *
 * @param input_path Path to the input .dsf file
 * @param output_path Output buffer for the generated .flac path
 * @param output_path_size Size of the output buffer
 * @return 0 upon success, otherwise 1
 */
int
hrmp_convert_dsf_default_output_path(char* input_path, char* output_path, size_t output_path_size);

/**
 * Convert a .dsf file to a 24-bit .flac file.
 * If output_path is NULL or empty, the output path is derived from input_path
 * by replacing the .dsf suffix with .flac.
 *
 * @param input_path Path to the input .dsf file
 * @param output_path Optional output path for the generated .flac file
 * @return 0 upon success, otherwise 1
 */
int
hrmp_convert_dsf_to_flac(char* input_path, char* output_path);

#ifdef __cplusplus
}
#endif

#endif
