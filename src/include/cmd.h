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

#ifndef CMD_H
#define CMD_H

#include <hrmp.h>

#include <stdbool.h>

/**
 * @struct cli_option
 * Struct to hold option definition
 */
typedef struct
{
   char* short_name;  /**< Short option name */
   char* long_name;   /**< Long option name */
   bool requires_arg; /**< Whether this option requires an argument */
} cli_option;

/**
 * @struct cli_result
 * Struct to hold parsed option result
 */
typedef struct
{
   char* option_name; /**< The matched option name (short or long) */
   char* argument;    /**< Argument value if applicable, NULL otherwise */
} cli_result;

/**
 * Parse command line arguments based on the provided options
 *
 * @param argc Number of arguments
 * @param argv Array of argument strings
 * @param options Array of option definitions
 * @param num_options Number of options in the array
 * @param results Output array for results
 * @param num_results Maximum number of results to store
 * @param use_last_arg_as_filename Whether to use the last argument as a filename
 * @param filename Output parameter for filename if requested
 * @param optind Output parameter for the index of the first non-option argument
 *
 * @return Number of results found, or -1 on error
 */
int cmd_parse(
   int argc,
   char** argv,
   cli_option* options,
   int num_options,
   cli_result* results,
   int num_results,
   bool use_last_arg_as_filename,
   char** filename,
   int* optind);

#endif
