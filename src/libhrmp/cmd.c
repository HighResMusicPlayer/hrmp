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

#include <cmd.h>

#include <err.h>
#include <stdlib.h>
#include <string.h>

static bool
option_requires_arg(char* option_name, cli_option* options, int num_options, bool is_long_option)
{
   for (int j = 0; j < num_options; j++)
   {
      cli_option* option = &options[j];
      bool matches = false;

      if (is_long_option)
      {
         matches = (strcmp(option_name, option->long_name) == 0);
      }
      else
      {
         matches = (strcmp(option_name, option->short_name) == 0);
      }

      if (matches)
      {
         return option->requires_arg;
      }
   }
   return false;
}

int
cmd_parse(
   int argc,
   char** argv,
   cli_option* options,
   int num_options,
   cli_result* results,
   int num_results,
   bool use_last_arg_as_filename,
   char** filename,
   int* optind
   )
{
   int result_count = 0;
   *filename = NULL;
   char** sorted_argv = malloc(argc * sizeof(char*));
   int sorted_idx = 0;
   char* arg = NULL;

   if (!sorted_argv)
   {
      warnx("Memory allocation error");
      return -1;
   }

   sorted_argv[sorted_idx++] = argv[0];

   /* First pass: collect flag options (options without arguments) */
   for (int i = 1; i < argc; i++)
   {
      arg = argv[i];

      if (arg[0] == '-')
      {
         bool is_long_option = (arg[1] == '-');
         char* option_text = arg + (is_long_option ? 2 : 1);

         /* Check if this option has an argument */
         bool has_equals = (strchr(option_text, '=') != NULL);

         /* If the option doesn't have an equals sign, check if it requires an argument */
         if (!has_equals)
         {
            bool requires_arg = option_requires_arg(option_text, options, num_options, is_long_option);

            if (!requires_arg)
            {
               /* This is a flag, add it to the sorted array */
               sorted_argv[sorted_idx++] = argv[i];
            }
         }
      }
   }

   /* Second pass: collect options with arguments */
   for (int i = 1; i < argc; i++)
   {
      arg = argv[i];

      if (arg[0] == '-')
      {
         bool is_long_option = (arg[1] == '-');
         char* option_text = arg + (is_long_option ? 2 : 1);

         /* Check if this option has an argument */
         bool has_equals = (strchr(option_text, '=') != NULL);

         if (has_equals)
         {
            /* Option with equals sign already includes its argument */
            sorted_argv[sorted_idx++] = argv[i];
         }
         else
         {
            bool requires_arg = option_requires_arg(option_text, options, num_options, is_long_option);

            if (requires_arg)
            {
               sorted_argv[sorted_idx++] = argv[i];

               if (i + 1 < argc && argv[i + 1][0] != '-')
               {
                  sorted_argv[sorted_idx++] = argv[i + 1];
                  i++;
               }
            }
         }
      }
   }

   /* Third pass: collect non-option arguments */
   for (int i = 1; i < argc; i++)
   {
      arg = argv[i];

      if (arg[0] != '-')
      {
         if (i > 1 && argv[i - 1][0] == '-')
         {
            bool is_long_option = (argv[i - 1][1] == '-');
            char* prev_option = argv[i - 1] + (is_long_option ? 2 : 1);

            if (!strchr(prev_option, '=') && option_requires_arg(prev_option, options, num_options, is_long_option))
            {
               continue;
            }
         }

         sorted_argv[sorted_idx++] = argv[i];
      }
   }

   for (int i = 0; i < sorted_idx; i++)
   {
      argv[i] = sorted_argv[i];
   }

   free(sorted_argv);

   int i = 1;
   for (; i < argc && result_count < num_results; i++)
   {
      arg = argv[i];

      if (arg[0] == '-')
      {
         bool is_long_option = (arg[1] == '-');
         char* option_text = arg + (is_long_option ? 2 : 1);
         bool option_matched = false;

         for (int j = 0; j < num_options; j++)
         {
            cli_option* option = &options[j];
            bool matches = false;

            if (is_long_option)
            {
               /* Match long option (--option) */
               matches = (strcmp(option_text, option->long_name) == 0);
            }
            else
            {
               /* Match short option (-X) */
               matches = (strcmp(option_text, option->short_name) == 0);
            }

            if (matches)
            {
               option_matched = true;

               results[result_count].option_name = is_long_option ?
                                                   option->long_name :
                                                   option->short_name;

               if (option->requires_arg)
               {
                  /* Handle option arguments in two ways:
                   * 1. As the next argument (--option value or -o value)
                   * 2. Joined with equals sign (--option=value or -o=value)
                   */
                  char* equals = strchr(option_text, '=');

                  if (equals && (is_long_option || equals == option_text + 1))
                  {
                     /* Option with argument in the form --option=value or -o=value */
                     results[result_count].argument = (char*)(equals + 1);
                  }
                  else if (i + 1 < argc && argv[i + 1][0] != '-')
                  {
                     /* Option with argument as the next parameter */
                     results[result_count].argument = (char*)argv[i + 1];
                     i++;
                  }
                  else
                  {
                     warnx("Error: Option %s requires an argument", arg);
                     return -1;
                  }
               }
               else
               {
                  results[result_count].argument = NULL;
               }

               result_count++;
               break;
            }
         }

         if (!option_matched)
         {
            /* Found an unknown option, we'll stop parsing here */
            warnx("Error: Unknown option %s", arg);
            // valid = false;
            break;
         }
      }
      else
      {
         /* We've found a non-option argument, stop parsing options */

         break;
      }
   }

   /* Set optind to the index of the first non-option argument */
   *optind = i;

   /* If we're configured to treat the last non-option argument as a filename
    * and there are remaining arguments, use the last one as filename */
   if (use_last_arg_as_filename && i < argc && argc - i == 1)
   {
      *filename = argv[i];
   }

   return result_count;
}
