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

/* hrmp */
#include <hrmp.h>
#include <alsa.h>
#include <cmd.h>
#include <configuration.h>
#include <devices.h>
#include <files.h>
#include <interactive.h>
#include <keyboard.h>
#include <list.h>
#include <logging.h>
#include <playback.h>
#include <playlist.h>
#include <shmem.h>
#include <utils.h>

/* system */
#include <err.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static void shuffle_files(struct list** files);
static int update_ringbuffer_cache(struct list* playbacks, struct list_entry* current, struct configuration* config);
static void free_playback_entry(void* value);
static void version(void);
static void usage(void);

#define ACTION_NOTHING       0
#define ACTION_HELP          1
#define ACTION_VERSION       2
#define ACTION_SAMPLE_CONFIG 3
#define ACTION_STATUS        4
#define ACTION_PLAY          5

typedef enum {
   HRMP_PLAYBACK_MODE_ONCE,
   HRMP_PLAYBACK_MODE_REPEAT,
   HRMP_PLAYBACK_MODE_SHUFFLE
} playback_mode;

int
main(int argc, char** argv)
{
   char* configuration_path = NULL;
   char* device_name = NULL;
   char* playlist_path = NULL;
   char* cp = NULL;
   size_t shmem_size;
   struct configuration* config = NULL;
   int ret;
   char message[MISC_LENGTH];
   bool recursive = false;
   bool q = false;
   bool e = false;
   bool d = false;
   bool f = false;
   bool m = false;
   bool dop = false;
   bool interactive = false;
   playback_mode mode = HRMP_PLAYBACK_MODE_ONCE;
   int files_index = 1;
   int action = ACTION_NOTHING;
   char* ad = NULL;
   char* filepath = NULL;
   int optind = 0;
   int num_options = 0;
   int num_results = 0;
   int num_files = 0;
   struct list* files = NULL;
   struct list_entry* files_entry = NULL;

   cli_option options[] = {
      {"c", "config", true},
      {"D", "device", true},
      {"p", "playlist", true},
      {"R", "recursive", false},
      {"M", "mode", true},
      {"I", "sample-configuration", false},
      {"i", "interactive", false},
      {"m", "metadata", false},
      {"s", "status", false},
      {"", "dop", false},
      {"q", "quiet", false},
      {"V", "version", false},
      {"", "experimental", false},
      {"", "developer", false},
      {"", "fallback", false},
      {"?", "help", false}};

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
      else if (!strcmp(optname, "p") || !strcmp(optname, "playlist"))
      {
         playlist_path = optarg;
         files_index += 2;
      }
      else if (!strcmp(optname, "R") || !strcmp(optname, "recursive"))
      {
         recursive = true;
         files_index += 1;
      }
      else if (!strcmp(optname, "M") || !strcmp(optname, "mode"))
      {
         if (!strcmp(optarg, "once"))
         {
            mode = HRMP_PLAYBACK_MODE_ONCE;
         }
         else if (!strcmp(optarg, "repeat"))
         {
            mode = HRMP_PLAYBACK_MODE_REPEAT;
         }
         else if (!strcmp(optarg, "shuffle"))
         {
            mode = HRMP_PLAYBACK_MODE_SHUFFLE;
         }
         else
         {
            printf("Invalid --mode '%s'\n", optarg);
            usage();
            exit(1);
         }

         files_index += 2;
      }
      else if (!strcmp(optname, "I") || !strcmp(optname, "sample-configuration"))
      {
         action = ACTION_SAMPLE_CONFIG;
         files_index += 1;
      }
      else if (!strcmp(optname, "i") || !strcmp(optname, "interactive"))
      {
         interactive = true;
         files_index += 1;
      }
      else if (!strcmp(optname, "m") || !strcmp(optname, "metadata"))
      {
         m = true;
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
      else if (!strcmp(optname, "fallback"))
      {
         f = true;
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

   config->quiet = q;
   config->metadata = m;
   config->experimental = e;
   config->developer = d;
   config->fallback = f;
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
   else
   {
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
            hrmp_snprintf(message, MISC_LENGTH, "Configuration file not found");
         }
         else if (ret == HRMP_CONFIGURATION_STATUS_FILE_TOO_BIG)
         {
            hrmp_snprintf(message, MISC_LENGTH, "Too many sections");
         }
         else if (ret == HRMP_CONFIGURATION_STATUS_KO)
         {
            hrmp_snprintf(message, MISC_LENGTH, "Invalid configuration file");
         }
         else if (ret > 0)
         {
            hrmp_snprintf(message, MISC_LENGTH, "%d problematic or duplicated section%c",
                          ret,
                          ret > 1 ? 's' : ' ');
         }

         errx(1, "%s (%s)", message, cp);
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

      if (action == ACTION_STATUS)
      {
         hrmp_check_devices();
         hrmp_print_devices();
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

         if (config->developer)
         {
            hrmp_print_devices();
         }

         if (device_name != NULL)
         {
            if (hrmp_is_device_known(device_name))
            {
               hrmp_activate_device(device_name);
            }
         }
         else
         {
            hrmp_activate_device(config->device);
         }

         if (strlen(config->active_device.device) == 0)
         {
            if (config->fallback)
            {
               if (device_name != NULL)
               {
                  if (config->developer)
                  {
                     printf("\n");
                     hrmp_list_fallback_devices();
                  }
                  hrmp_create_active_device(device_name);
               }
               else
               {
                  hrmp_list_fallback_devices();
                  printf("Fallback requires a device name\n");
               }
            }
         }

         if (config->developer)
         {
            printf("\nActive device: ");
            hrmp_print_device(&config->active_device);
         }

         if (strlen(config->active_device.device) > 0)
         {
            hrmp_alsa_init_volume();

            if (hrmp_list_create(&files))
            {
               printf("Error creating files list\n");
               goto error;
            }

            if (playlist_path != NULL)
            {
               if (hrmp_playlist_load(playlist_path, files, config->quiet))
               {
                  printf("Error reading playlist '%s'\n", playlist_path);
                  goto error;
               }
            }

            int play_from_index = 0;
            if (interactive)
            {
               if (hrmp_interactive_ui(files, filepath, &play_from_index))
               {
                  printf("Error in interactive UI\n");
                  goto error;
               }

               if (play_from_index < 0)
               {
                  struct list_entry* e = files->head;
                  while (e != NULL)
                  {
                     struct list_entry* next = e->next;
                     free(e);
                     e = next;
                  }
                  files->head = NULL;
                  files->tail = NULL;
                  files->size = 0;
                  play_from_index = 0;
               }
            }
            else
            {
               for (int i = files_index; i < argc; i++)
               {
                  if (hrmp_is_directory(argv[i]))
                  {
                     if (recursive)
                     {
                        hrmp_get_files(argv[i], recursive, files);
                     }
                  }
                  else
                  {
                     bool added = false;

                     if (hrmp_exists(argv[i]))
                     {
                        if (hrmp_list_append(files, argv[i]) == 0)
                        {
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
            }

            /* Filter unsupported files: display them, but don't keep them in the list. */
            struct list* supported_files = NULL;
            if (hrmp_list_create(&supported_files))
            {
               printf("Error creating files list\n");
               goto error;
            }

            for (files_entry = hrmp_list_head(files);
                 files_entry != NULL;
                 files_entry = hrmp_list_next(files_entry))
            {
               struct file_metadata* fm = NULL;

               if (hrmp_file_metadata((char*)files_entry->value, &fm))
               {
                  continue;
               }

               if (hrmp_list_append(supported_files, (const char*)files_entry->value))
               {
                  free(fm);
                  hrmp_list_destroy(supported_files);
                  printf("Error creating files list\n");
                  goto error;
               }

               free(fm);
            }

            hrmp_list_destroy(files);
            files = supported_files;

            if (mode == HRMP_PLAYBACK_MODE_SHUFFLE)
            {
               srand((unsigned)time(NULL));
               shuffle_files(&files);
               play_from_index = 0;
            }

            struct list* playbacks = NULL;
            if (hrmp_list_create(&playbacks))
            {
               printf("Error creating playback list\n");
               goto error;
            }

            num_files = 0;
            for (files_entry = hrmp_list_head(files);
                 files_entry != NULL;
                 files_entry = hrmp_list_next(files_entry))
            {
               struct playback* pb = NULL;
               struct file_metadata* fm = NULL;

               if (hrmp_file_metadata((char*)files_entry->value, &fm))
               {
                  continue;
               }

               if (hrmp_playback_init(num_files + 1, files->size, fm, &pb))
               {
                  free(fm);
                  hrmp_list_destroy_with(playbacks, free_playback_entry);
                  printf("Error creating playback\n");
                  goto error;
               }

               if (hrmp_list_append_owned(playbacks, pb))
               {
                  free(fm);
                  free_playback_entry(pb);
                  hrmp_list_destroy_with(playbacks, free_playback_entry);
                  printf("Error creating playback list\n");
                  goto error;
               }

               num_files++;
            }

            /* Keyboard */
            hrmp_keyboard_mode(true);

            if (config->developer && !config->quiet)
            {
               for (files_entry = hrmp_list_head(files);
                    files_entry != NULL;
                    files_entry = hrmp_list_next(files_entry))
               {
                   printf("Queued: %s\n", (const char*)files_entry->value);
               }

               printf("Number of files: %ld\n", hrmp_list_size(files));
            }

            num_files = 0;
            files_entry = hrmp_list_head(playbacks);
            for (int i = 0; i < play_from_index && files_entry != NULL; i++)
            {
               files_entry = hrmp_list_next(files_entry);
               num_files++;
            }
            while (files_entry != NULL)
            {
               bool next = true;
               struct playback* pb = (struct playback*)files_entry->value;

               hrmp_set_proc_title(argc, argv, pb->fm->name);
               if (update_ringbuffer_cache(playbacks, files_entry, config))
               {
                  hrmp_list_destroy_with(playbacks, free_playback_entry);
                  printf("Error preparing cache\n");
                  goto error;
               }
               hrmp_playback(pb, &next);

               if (next)
               {
                  files_entry = hrmp_list_next(files_entry);
                  num_files++;

                  if (mode == HRMP_PLAYBACK_MODE_REPEAT && files_entry == NULL && !hrmp_list_empty(playbacks))
                  {
                     files_entry = hrmp_list_head(playbacks);
                     num_files = 0;
                  }
               }
               else
               {
                  files_entry = hrmp_list_prev(files_entry);
                  num_files--;
                  if (num_files < 0)
                  {
                     num_files = 0;
                  }
               }
            }

            hrmp_list_destroy_with(playbacks, free_playback_entry);

            hrmp_keyboard_mode(false);
         }
      }
   }
   hrmp_stop_logging();
   hrmp_destroy_shared_memory(shmem, shmem_size);

   hrmp_list_destroy(files);

   free(ad);
   free(cp);

   return 0;

error:

   hrmp_stop_logging();
   hrmp_destroy_shared_memory(shmem, shmem_size);

   hrmp_list_destroy(files);

   free(ad);
   free(cp);

   return 1;
}





static void
shuffle_files(struct list** files)
{
   if (files == NULL || *files == NULL)
   {
      return;
   }

   size_t n = hrmp_list_size(*files);
   if (n < 2)
   {
      return;
   }

   char** a = malloc(n * sizeof(char*));
   if (a == NULL)
   {
      return;
   }

   size_t i = 0;
   for (struct list_entry* e = hrmp_list_head(*files); e != NULL; e = hrmp_list_next(e))
   {
      a[i++] = (char*)e->value;
   }

   for (i = n - 1; i > 0; i--)
   {
      size_t j = (size_t)(rand() % (i + 1));
      char* tmp = a[i];
      a[i] = a[j];
      a[j] = tmp;
   }

   struct list* shuffled = NULL;
   if (hrmp_list_create(&shuffled))
   {
      free(a);
      return;
   }

   for (i = 0; i < n; i++)
   {
      hrmp_list_append(shuffled, a[i]);
   }

   hrmp_list_destroy(*files);
   *files = shuffled;

   free(a);
}

static int
update_ringbuffer_cache(struct list* playbacks, struct list_entry* current, struct configuration* config)
{
   if (playbacks == NULL || current == NULL || config == NULL)
   {
      return 1;
   }

   if (config->cache_size == 0)
   {
      for (struct list_entry* e = hrmp_list_head(playbacks); e != NULL; e = hrmp_list_next(e))
      {
         struct playback* pb = (struct playback*)e->value;
         if (pb->rb != NULL)
         {
            hrmp_ringbuffer_destroy(pb->rb);
            pb->rb = NULL;
         }
      }
      return 0;
   }

   if (config->cache_files == HRMP_CACHE_FILES_ALL)
   {
      for (struct list_entry* e = hrmp_list_head(playbacks); e != NULL; e = hrmp_list_next(e))
      {
         struct playback* pb = (struct playback*)e->value;
         if (hrmp_playback_prepare_ringbuffer(pb))
         {
            return 1;
         }
         if (e != current && pb->rb != NULL)
         {
            hrmp_ringbuffer_reset(pb->rb);
         }
      }
      return 0;
   }

   struct list_entry* prev = hrmp_list_prev(current);
   struct list_entry* next = hrmp_list_next(current);

   for (struct list_entry* e = hrmp_list_head(playbacks); e != NULL; e = hrmp_list_next(e))
   {
      struct playback* pb = (struct playback*)e->value;
      bool keep = (e == current);

      if (config->cache_files == HRMP_CACHE_FILES_MINIMAL)
      {
         keep = keep || (e == prev) || (e == next);
      }

      if (keep)
      {
         if (hrmp_playback_prepare_ringbuffer(pb))
         {
            return 1;
         }
      }
      else if (pb->rb != NULL)
      {
         hrmp_ringbuffer_destroy(pb->rb);
         pb->rb = NULL;
      }
   }

   return 0;
}

static void
free_playback_entry(void* value)
{
   struct playback* pb = (struct playback*)value;
   if (pb == NULL)
   {
      return;
   }

   hrmp_ringbuffer_destroy(pb->rb);
   free(pb->fm);
   free(pb);
}

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
   printf("  -p, --playlist PLAYLIST    Load a playlist (.hrmp)\n");
   printf("  -R, --recursive            Add files recursive of the directory\n");
   printf("  -M, --mode MODE            Playback mode: once, repeat, shuffle\n");
   printf("  -I, --sample-configuration Generate a sample configuration\n");
   printf("  -i, --interactive          Text UI mode\n");
   printf("  -m, --metadata             Display metadata of the files\n");
   printf("  -s, --status               Status of the devices\n");
   printf("      --dop                  Use DSD over PCM\n");
   printf("  -q, --quiet                Quiet the player\n");
   printf("  -V, --version              Display version information\n");
   printf("  -?, --help                 Display help\n");
   printf("\n");
   printf("hrmp: %s\n", HRMP_HOMEPAGE);
   printf("Report bugs: %s\n", HRMP_ISSUES);
}
