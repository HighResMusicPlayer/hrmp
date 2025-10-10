/*
 * Copyright (C) 2025 The HighResMusicPlayer community
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
#include <cmd.h>
#include <configuration.h>
#include <deque.h>
#include <devices.h>
#include <files.h>
#include <keyboard.h>
#include <logging.h>
#include <playback.h>
#include <shmem.h>
#include <utils.h>
#include <value.h>

/* system */
#include <err.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ACTION_NOTHING       0
#define ACTION_HELP          1
#define ACTION_VERSION       2
#define ACTION_SAMPLE_CONFIG 3
#define ACTION_STATUS        4
#define ACTION_PLAY          5

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
   printf("  -D, --device               Set the device name\n");
   printf("  -R, --recursive            Add files recursive of the directory\n");
   printf("  -I, --sample-configuration Generate a sample configuration\n");
   printf("  -s, --status               Status of the devices\n");
   printf("      --dop                  Use DSD over PCM\n");
   printf("  -q, --quiet                Quiet the player\n");
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
   char* device_name = NULL;
   char* cp = NULL;
   size_t shmem_size;
   struct configuration* config = NULL;
   int ret;
   char message[MISC_LENGTH];
   bool recursive = false;
   bool q = false;
   bool e = false;
   bool d = false;
   bool dop = false;
   int files_index = 1;
   int action = ACTION_NOTHING;
   int active_device = -1;
   char* filepath = NULL;
   int optind = 0;
   int num_options = 0;
   int num_results = 0;
   int num_files = 0;
   struct deque* files = NULL;
   struct deque_iterator* files_iterator = NULL;

   cli_option options[] =
   {
      {"c", "config", true},
      {"D", "device", true},
      {"R", "recursive", false},
      {"I", "sample-configuration", false},
      {"s", "status", false},
      {"", "dop", false},
      {"q", "quiet", false},
      {"V", "version", false},
      {"", "experimental", false},
      {"", "developer", false},
      {"?", "help", false}
   };

   // Disable stdout buffering (i.e. write to stdout immediatelly).
   setbuf(stdout, NULL);

   num_options = sizeof(options) / sizeof(options[0]);

   cli_result results[num_options];

   num_results = cmd_parse(argc, argv, options, num_options, results,
                           num_options, false, &filepath, &optind);

   if (num_results < 0)
   {
      return 1;
   }

   for (int i = 0; i < num_results; i++)
   {
      char* optname = results[i].option_name;
      char* optarg = results[i].argument;

      if (optname == NULL)
      {
         break;
      }
      else if (!strcmp(optname, "c") || !strcmp(optname, "config"))
      {
         configuration_path = optarg;
         files_index += 2;
      }
      else if (!strcmp(optname, "D") || !strcmp(optname, "device"))
      {
         device_name = optarg;
         files_index += 2;
      }
      else if (!strcmp(optname, "R") || !strcmp(optname, "recursive"))
      {
         recursive = true;
         files_index += 1;
      }
      else if (!strcmp(optname, "I") || !strcmp(optname, "sample-configuration"))
      {
         action = ACTION_SAMPLE_CONFIG;
         files_index += 1;
      }
      else if (!strcmp(optname, "s") || !strcmp(optname, "status"))
      {
         action = ACTION_STATUS;
         files_index += 1;
      }
      else if (!strcmp(optname, "dop"))
      {
         dop = true;
         files_index += 1;
      }
      else if (!strcmp(optname, "q") || !strcmp(optname, "quiet"))
      {
         q = true;
         files_index += 1;
      }
      else if (!strcmp(optname, "V") || !strcmp(optname, "version"))
      {
         version();
         exit(0);
      }
      else if (!strcmp(optname, "experimental"))
      {
         e = true;
         files_index += 1;
      }
      else if (!strcmp(optname, "developer"))
      {
         d = true;
         files_index += 1;
      }
      else if (!strcmp(optname, "?") || !strcmp(optname, "help"))
      {
         usage();
         exit(0);
      }
   }

   if (argc == 1)
   {
      usage();
      exit(0);
   }

   shmem_size = sizeof(struct configuration);
   if (hrmp_create_shared_memory(shmem_size, &shmem))
   {
      errx(1, "Error in creating shared memory");
      goto error;
   }

   memset(shmem, 0, sizeof(struct configuration));

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

   memcpy(&config->configuration_path[0], cp, MIN(strlen(cp), (size_t)MAX_PATH - 1));

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

   config->quiet = q;
   config->experimental = e;
   config->developer = d;
   config->dop = dop;

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
   else if (action == ACTION_STATUS)
   {
      hrmp_check_devices();
#ifdef DEBUG
      hrmp_print_devices();
#endif
   }
   else
   {
      action = ACTION_PLAY;
   }

   if (action == ACTION_PLAY)
   {
      if (config->developer)
      {
         printf("hrmp %s\n", VERSION);
      }

      hrmp_check_devices();

#ifdef DEBUG
      hrmp_print_devices();
#endif

      if (device_name != NULL)
      {
         if (hrmp_is_device_known(device_name))
         {
            active_device = hrmp_active_device(device_name);
         }
         else
         {
            printf("Unknown device '%s'\n", device_name);
         }
      }
      else
      {
         active_device = hrmp_active_device(config->device);
      }

      if (active_device >= 0)
      {
         if (hrmp_deque_create(false, &files))
         {
            printf("Error creating files list\n");
            goto error;
         }

         for (int i = files_index; i < argc; i++)
         {
            if (hrmp_is_directory(argv[i]))
            {
               hrmp_get_files(active_device, argv[i], recursive, files);
            }
            else
            {
               bool added = false;

               if (hrmp_exists(argv[i]))
               {
                  struct file_metadata* fm = NULL;

                  if (hrmp_file_metadata(active_device, argv[i], &fm) == 0)
                  {
                     hrmp_deque_add(files, NULL, (uintptr_t)fm, ValueMem);
                     added = true;
                  }
               }

               if (!added)
               {
                  if (!config->quiet)
                  {
                     if (!hrmp_exists(argv[i]))
                     {
                        printf("File not found '%s'\n", argv[i]);
                     }
                  }
               }
            }
         }

         /* Keyboard */
         hrmp_keyboard_mode(true);

         if (config->developer && !config->quiet)
         {
            if (hrmp_deque_iterator_create(files, &files_iterator))
            {
               goto error;
            }

            while (hrmp_deque_iterator_next(files_iterator))
            {
               printf("Queued: %s\n", (char*)hrmp_value_data(files_iterator->value));
            }

            hrmp_deque_iterator_destroy(files_iterator);
            files_iterator = NULL;
         }

         if (hrmp_deque_iterator_create(files, &files_iterator))
         {
            goto error;
         }

         num_files = 0;
         while (hrmp_deque_iterator_next(files_iterator))
         {
            struct file_metadata* fm = (struct file_metadata*)hrmp_value_data(files_iterator->value);

            hrmp_set_proc_title(argc, argv, fm->name);
            hrmp_playback(active_device, num_files + 1, files->size, fm);

            num_files++;
         }

         hrmp_keyboard_mode(false);
      }
   }

   hrmp_stop_logging();
   hrmp_destroy_shared_memory(shmem, shmem_size);

   hrmp_deque_iterator_destroy(files_iterator);
   hrmp_deque_destroy(files);

   free(cp);

   return 0;

error:

   hrmp_stop_logging();
   hrmp_destroy_shared_memory(shmem, shmem_size);

   hrmp_deque_iterator_destroy(files_iterator);
   hrmp_deque_destroy(files);

   free(cp);

   return 1;
}
