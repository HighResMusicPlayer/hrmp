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

/* hrmp */
#include <hrmp.h>
#include <alsa.h>
#include <configuration.h>
#include <devices.h>
#include <dlist.h>
#include <files.h>
#include <flac.h>
#include <logging.h>
#include <playback.h>
#include <shmem.h>
#include <utils.h>

/* system */
#include <err.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ACTION_HELP          0
#define ACTION_VERSION       1
#define ACTION_SAMPLE_CONFIG 2
#define ACTION_PLAY          3

static void
version(void)
{
   printf("hrmp %s\n", VERSION);
}

static void
usage(void)
{
   printf("hrmp %s\n", VERSION);
   printf("  High resolution music player\n");
   printf("\n");

   printf("Usage:\n");
   printf("  hrmp <FILES>\n");
   printf("\n");
   printf("Options:\n");
   printf("  -c, --config CONFIG_FILE   Set the path to the hrmp.conf file\n");
   printf("                             Default: $HOME/.hrmp/hrmp.conf\n");
   printf("  -I, --sample-configuration Generate a sample configuration\n");
   printf("  -V, --version              Display version information\n");
   printf("  -?, --help                 Display help\n");
   printf("\n");
   printf("hrmp: %s\n", HRMP_HOMEPAGE);
   printf("Report bugs: %s\n", HRMP_ISSUES);
}

int
main(int argc, char** argv)
{
   char* configuration_path = NULL;
   char* cp = NULL;
   size_t shmem_size;
   struct configuration* config = NULL;
   int ret;
   int c;
   char message[MISC_LENGTH];
   /* int vol; */
   int files_index = 1;
   int action = ACTION_PLAY;
   struct dlist* files = NULL;

   while (1)
   {
      static struct option long_options[] =
      {
         {"config", required_argument, 0, 'c'},
         {"sample-configuration", no_argument, 0, 'I'},
         {"version", no_argument, 0, 'V'},
         {"help", no_argument, 0, '?'}
      };
      int option_index = 0;

      c = getopt_long (argc, argv, "IV?c:",
                       long_options, &option_index);

      if (c == -1)
      {
         break;
      }

      switch (c)
      {
         case 'c':
            configuration_path = optarg;
            files_index += 2;
            break;
         case 'I':
            action = ACTION_SAMPLE_CONFIG;
            break;
         case 'V':
            action = ACTION_VERSION;
            break;
         case '?':
            action = ACTION_HELP;
            break;
         default:
            break;
      }
   }

   shmem_size = sizeof(struct configuration);
   if (hrmp_create_shared_memory(shmem_size, HUGEPAGE_TRY, &shmem))
   {
      errx(1, "Error in creating shared memory");
      goto error;
   }

   hrmp_init_configuration(shmem);
   config = (struct configuration*)shmem;

   if (configuration_path != NULL)
   {
      cp = hrmp_append(cp, configuration_path);
   }
   else
   {
      cp = hrmp_append(cp, hrmp_get_home_directory());
      cp = hrmp_append(cp, "/.hrmp/hrmp.conf");
   }

   if ((ret = hrmp_read_configuration(shmem, cp, true)) != HRMP_CONFIGURATION_STATUS_OK)
   {
      // the configuration has some problem, build up a descriptive message
      if (ret == HRMP_CONFIGURATION_STATUS_FILE_NOT_FOUND)
      {
         snprintf(message, MISC_LENGTH, "Configuration file not found");
      }
      else if (ret == HRMP_CONFIGURATION_STATUS_FILE_TOO_BIG)
      {
         snprintf(message, MISC_LENGTH, "Too many sections");
      }
      else if (ret == HRMP_CONFIGURATION_STATUS_KO)
      {
         snprintf(message, MISC_LENGTH, "Invalid configuration file");
      }
      else if (ret > 0)
      {
         snprintf(message, MISC_LENGTH, "%d problematic or duplicated section%c",
                  ret,
                  ret > 1 ? 's' : ' ');
      }

      errx(1, "%s (file <%s>)", message, cp);
      goto error;
   }

   memcpy(&config->configuration_path[0], cp, MIN(strlen(cp), MAX_PATH - 1));

   if (hrmp_init_logging())
   {
      errx(1, "Failed to init logging");
      goto error;
   }

   if (hrmp_start_logging())
   {
      errx(1, "Failed to start logging");
      goto error;
   }

   if (hrmp_validate_configuration(shmem))
   {
      errx(1, "Invalid configuration");
      goto error;
   }

   if (action == ACTION_HELP)
   {
      usage();
   }
   else if (action == ACTION_VERSION)
   {
      version();
   }
   else if (action == ACTION_SAMPLE_CONFIG)
   {
      hrmp_sample_configuration();
   }
   else
   {
      hrmp_check_devices();
      hrmp_print_devices();

      if (files_index >= argc)
      {
         printf("No files\n");
         goto error;
      }

      if (hrmp_dlist_create(&files))
      {
         printf("Error creating files list\n");
         goto error;
      }

      for (int i = files_index; i < argc; i++)
      {
         if (hrmp_is_file_supported(argv[i]))
         {
            hrmp_dlist_append(files, argv[i]);
         }
      }

      /* Set the master volume */
      /* hrmp_set_master_volume(config->volume); */
   }

   for (int i = 0; i < hrmp_dlist_size(files); i++)
   {
      char* fn = hrmp_dlist_get(files, i);
      bool is_supported = false;
      int type = hrmp_is_file_supported(fn);
      int active_device = -1;

      active_device = hrmp_active_device(config->device);

      if (type == TYPE_FLAC)
      {
         struct file_metadata* fm = NULL;
         hrmp_flac_get_metadata(fn, &fm);

         if (active_device >= 0)
         {
            if (hrmp_is_file_metadata_supported(active_device, fm))
            {
               is_supported = true;
               hrmp_print_file_metadata(fm);
               hrmp_playback_flac(fn, active_device, fm);
            }
         }

         if (!is_supported)
         {
            hrmp_print_file_metadata(fm);
         }

         free(fm);
      }

      if (active_device < 0)
      {
         hrmp_log_error("No active devices for %s", fn);
      }
      else if (!is_supported)
      {
         hrmp_log_warn("File '%s' isn't supported", fn);
      }
   }

   hrmp_stop_logging();
   hrmp_destroy_shared_memory(shmem, shmem_size);

   hrmp_dlist_destroy(files);

   free(cp);

   return 0;

error:

   hrmp_stop_logging();
   hrmp_destroy_shared_memory(shmem, shmem_size);

   hrmp_dlist_destroy(files);

   free(cp);

   return 1;
}
