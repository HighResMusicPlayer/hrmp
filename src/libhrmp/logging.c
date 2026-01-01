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
#include <logging.h>
#include <utils.h>

/* system */
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#define LINE_LENGTH 32
#define MAX_LENGTH  4096

FILE* log_file;

char current_log_path[MAX_PATH]; /* the current log file */

static int log_file_open(void);

static void output_log_line(char* l);

static char* levels[] = {
   "TRACE",
   "DEBUG",
   "INFO",
   "WARN",
   "ERROR",
   "FATAL"};

static char* colors[] =
   {
      "\x1b[37m",
      "\x1b[36m",
      "\x1b[32m",
      "\x1b[91m",
      "\x1b[31m",
      "\x1b[35m"};

/**
 *
 */
int
hrmp_start_logging(void)
{
   struct configuration* config;

   config = (struct configuration*)shmem;

   if (config->log_type == HRMP_LOGGING_TYPE_FILE && !log_file)
   {
      log_file_open();

      if (!log_file)
      {
         printf("Failed to open log file %s due to %s\n", strlen(config->log_path) > 0 ? config->log_path : "hrmp.log", strerror(errno));
         errno = 0;
         return 1;
      }
   }
   else if (config->log_type == HRMP_LOGGING_TYPE_SYSLOG)
   {
      openlog("hrmp", LOG_CONS | LOG_PERROR | LOG_PID, LOG_USER);
   }

   return 0;
}

/**
 *
 */
int
hrmp_stop_logging(void)
{
   struct configuration* config;

   config = (struct configuration*)shmem;

   if (config->log_type == HRMP_LOGGING_TYPE_FILE)
   {
      if (log_file != NULL)
      {
         return fclose(log_file);
      }
      else
      {
         return 1;
      }
   }
   else if (config->log_type == HRMP_LOGGING_TYPE_SYSLOG)
   {
      closelog();
   }

   return 0;
}

bool
hrmp_log_is_enabled(int level)
{
   struct configuration* config;

   config = (struct configuration*)shmem;

   if (level >= config->log_level)
   {
      return true;
   }

   return false;
}

void
hrmp_log_line(int level, char* file, int line, char* fmt, ...)
{
   FILE* output = NULL;
   signed char isfree;
   struct configuration* config;

   config = (struct configuration*)shmem;

   if (config == NULL)
   {
      return;
   }

   if (level >= config->log_level)
   {
      if (config->log_type == HRMP_LOGGING_TYPE_CONSOLE)
      {
         output = stdout;
      }
      else if (config->log_type == HRMP_LOGGING_TYPE_FILE)
      {
         output = log_file;
      }

retry:
      isfree = STATE_FREE;

      if (atomic_compare_exchange_strong(&config->log_lock, &isfree, STATE_IN_USE))
      {
         char buf[1024];
         va_list vl;
         struct tm* tm;
         time_t t;
         char* filename;

         t = time(NULL);
         tm = localtime(&t);

         filename = strrchr(file, '/');
         if (filename != NULL)
         {
            filename = filename + 1;
         }
         else
         {
            filename = file;
         }

         if (strlen(config->log_line_prefix) == 0)
         {
            memcpy(config->log_line_prefix, HRMP_LOGGING_DEFAULT_LOG_LINE_PREFIX, strlen(HRMP_LOGGING_DEFAULT_LOG_LINE_PREFIX));
         }

         memset(&buf[0], 0, sizeof(buf));

#ifdef DEBUG
         if (level > 4)
         {
            char* bt = NULL;
            hrmp_backtrace_string(&bt);
            if (bt != NULL)
            {
               fprintf(output, "%s", bt);
               fflush(output);
            }
            free(bt);
            memset(&buf[0], 0, sizeof(buf));
         }
#endif

         va_start(vl, fmt);

         if (config->log_type == HRMP_LOGGING_TYPE_CONSOLE)
         {
            buf[strftime(buf, sizeof(buf), config->log_line_prefix, tm)] = '\0';
            fprintf(output, "%s %s%-5s\x1b[0m \x1b[90m%s:%d\x1b[0m ",
                    buf, colors[level - 1], levels[level - 1],
                    filename, line);
            vfprintf(output, fmt, vl);
            fprintf(output, "\n");
            fflush(output);
         }
         else if (config->log_type == HRMP_LOGGING_TYPE_FILE)
         {
            buf[strftime(buf, sizeof(buf), config->log_line_prefix, tm)] = '\0';
            fprintf(output, "%s %-5s %s:%d ",
                    buf, levels[level - 1], filename, line);
            vfprintf(output, fmt, vl);
            fprintf(output, "\n");
            fflush(output);
         }
         else if (config->log_type == HRMP_LOGGING_TYPE_SYSLOG)
         {
            switch (level)
            {
               case HRMP_LOGGING_LEVEL_DEBUG5:
                  vsyslog(LOG_DEBUG, fmt, vl);
                  break;
               case HRMP_LOGGING_LEVEL_DEBUG1:
                  vsyslog(LOG_DEBUG, fmt, vl);
                  break;
               case HRMP_LOGGING_LEVEL_INFO:
                  vsyslog(LOG_INFO, fmt, vl);
                  break;
               case HRMP_LOGGING_LEVEL_WARN:
                  vsyslog(LOG_WARNING, fmt, vl);
                  break;
               case HRMP_LOGGING_LEVEL_ERROR:
                  vsyslog(LOG_ERR, fmt, vl);
                  break;
               case HRMP_LOGGING_LEVEL_FATAL:
                  vsyslog(LOG_CRIT, fmt, vl);
                  break;
               default:
                  vsyslog(LOG_INFO, fmt, vl);
                  break;
            }
         }

         va_end(vl);

         atomic_store(&config->log_lock, STATE_FREE);
      }
      else
         SLEEP_AND_GOTO(1000000L, retry)
   }
}

void
hrmp_log_mem(void* data, size_t size)
{
   signed char isfree;
   struct configuration* config;

   config = (struct configuration*)shmem;

   if (config == NULL)
   {
      return;
   }

   if (size > 0)
   {
      if (config->log_level == HRMP_LOGGING_LEVEL_DEBUG5 &&
          (config->log_type == HRMP_LOGGING_TYPE_CONSOLE || config->log_type == HRMP_LOGGING_TYPE_FILE))
      {
retry:
         isfree = STATE_FREE;

         if (atomic_compare_exchange_strong(&config->log_lock, &isfree, STATE_IN_USE))
         {
            if (size > MAX_LENGTH)
            {
               int index = 0;
               size_t count = 0;

               /* Display the first 1024 bytes */
               index = 0;
               count = 1024;
               while (count > 0)
               {
                  char* t = NULL;
                  char* n = NULL;
                  char* l = NULL;

                  for (int i = 0; i < LINE_LENGTH; i++)
                  {
                     signed char c;
                     char buf[3] = {0};

                     c = (signed char)*((char*)data + index + i);
                     hrmp_snprintf(&buf[0], sizeof(buf), "%02X", c);

                     l = hrmp_append(l, &buf[0]);

                     if (c >= 32)
                     {
                        n = hrmp_append_char(n, c);
                     }
                     else
                     {
                        n = hrmp_append_char(n, '?');
                     }
                  }

                  t = hrmp_append(t, l);
                  t = hrmp_append_char(t, ' ');
                  t = hrmp_append(t, n);

                  output_log_line(t);

                  free(t);
                  t = NULL;

                  free(l);
                  l = NULL;

                  free(n);
                  n = NULL;

                  count -= LINE_LENGTH;
                  index += LINE_LENGTH;
               }

               output_log_line("---------------------------------------------------------------- --------------------------------");

               /* Display the last 1024 bytes */
               index = size - 1024;
               count = 1024;
               while (count > 0)
               {
                  char* t = NULL;
                  char* n = NULL;
                  char* l = NULL;

                  for (int i = 0; i < LINE_LENGTH; i++)
                  {
                     signed char c;
                     char buf[3] = {0};

                     c = (signed char)*((char*)data + index + i);
                     hrmp_snprintf(&buf[0], sizeof(buf), "%02X", c);

                     l = hrmp_append(l, &buf[0]);

                     if (c >= 32)
                     {
                        n = hrmp_append_char(n, c);
                     }
                     else
                     {
                        n = hrmp_append_char(n, '?');
                     }
                  }

                  t = hrmp_append(t, l);
                  t = hrmp_append_char(t, ' ');
                  t = hrmp_append(t, n);

                  output_log_line(t);

                  free(t);
                  t = NULL;

                  free(l);
                  l = NULL;

                  free(n);
                  n = NULL;

                  count -= LINE_LENGTH;
                  index += LINE_LENGTH;
               }
            }
            else
            {
               size_t offset = 0;
               size_t remaining = size;
               bool full_line = false;

               while (remaining > 0)
               {
                  char* t = NULL;
                  char* n = NULL;
                  char* l = NULL;
                  size_t count = MIN((int)remaining, (int)LINE_LENGTH);

                  for (size_t i = 0; i < count; i++)
                  {
                     signed char c;
                     char buf[3] = {0};

                     c = (signed char)*((char*)data + offset + i);
                     hrmp_snprintf(&buf[0], sizeof(buf), "%02X", c);

                     l = hrmp_append(l, &buf[0]);

                     if (c >= 32)
                     {
                        n = hrmp_append_char(n, c);
                     }
                     else
                     {
                        n = hrmp_append_char(n, '?');
                     }
                  }

                  if (strlen(l) == LINE_LENGTH * 2)
                  {
                     full_line = true;
                  }
                  else if (full_line)
                  {
                     if (strlen(l) < LINE_LENGTH * 2)
                     {
                        int chars_missing = (LINE_LENGTH * 2) - strlen(l);
                        for (int i = 0; i < chars_missing; i++)
                        {
                           l = hrmp_append_char(l, ' ');
                        }
                     }
                  }

                  t = hrmp_append(t, l);
                  t = hrmp_append_char(t, ' ');
                  t = hrmp_append(t, n);

                  output_log_line(t);

                  free(t);
                  t = NULL;

                  free(l);
                  l = NULL;

                  free(n);
                  n = NULL;

                  remaining -= count;
                  offset += count;
               }
            }

            atomic_store(&config->log_lock, STATE_FREE);
         }
         else
         {
            SLEEP_AND_GOTO(1000000L, retry)
         }
      }
   }
}

static void
output_log_line(char* l)
{
   struct configuration* config;

   config = (struct configuration*)shmem;

   if (config->log_type == HRMP_LOGGING_TYPE_CONSOLE)
   {
      fprintf(stdout, "%s", l);
      fprintf(stdout, "\n");
      fflush(stdout);
   }
   else if (config->log_type == HRMP_LOGGING_TYPE_FILE)
   {
      fprintf(log_file, "%s", l);
      fprintf(log_file, "\n");
      fflush(log_file);
   }
}

static int
log_file_open(void)
{
   struct configuration* config;
   time_t htime;
   struct tm* tm;

   config = (struct configuration*)shmem;

   if (config->log_type == HRMP_LOGGING_TYPE_FILE)
   {
      htime = time(NULL);
      if (!htime)
      {
         log_file = NULL;
         return 1;
      }

      tm = localtime(&htime);
      if (tm == NULL)
      {
         log_file = NULL;
         return 1;
      }

      if (strftime(current_log_path, sizeof(current_log_path), config->log_path, tm) <= 0)
      {
         // cannot parse the format string, fallback to default logging
         memcpy(current_log_path, "hrmp.log", strlen("hrmp.log"));
      }

      log_file = fopen(current_log_path, config->log_mode == HRMP_LOGGING_MODE_APPEND ? "a" : "w");

      if (!log_file)
      {
         return 1;
      }

      return 0;
   }

   return 1;
}
