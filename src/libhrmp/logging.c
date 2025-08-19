/*
 * Copyright (C) 2025 The HighResMusicPlayer community
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* hrmp */
#include <hrmp.h>
#include <logging.h>

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

FILE* log_file;
char current_log_path[MAX_PATH]; /* the current log file */

static int log_file_open(void);

static const char *levels[] =
{
   "TRACE",
   "DEBUG",
   "INFO",
   "WARN",
   "ERROR",
   "FATAL"
};

static const char* colors[] =
{
   "\x1b[37m",
   "\x1b[36m",
   "\x1b[32m",
   "\x1b[91m",
   "\x1b[31m",
   "\x1b[35m"
};

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

void
hrmp_log_line(int level, char* file, int line, char* fmt, ...)
{
   signed char isfree;
   struct configuration* config;

   config = (struct configuration*)shmem;

   if (config == NULL)
   {
      return;
   }

   if (level >= config->log_level)
   {
retry:
      isfree = STATE_FREE;

      if (atomic_compare_exchange_strong(&config->log_lock, &isfree, STATE_IN_USE))
      {
         char buf[256];
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

         va_start(vl, fmt);

         if (config->log_type == HRMP_LOGGING_TYPE_CONSOLE)
         {
            buf[strftime(buf, sizeof(buf), config->log_line_prefix, tm)] = '\0';
            fprintf(stdout, "%s %s%-5s\x1b[0m \x1b[90m%s:%d\x1b[0m ",
                    buf, colors[level - 1], levels[level - 1],
                    filename, line);
            vfprintf(stdout, fmt, vl);
            fprintf(stdout, "\n");
            fflush(stdout);
         }
         else if (config->log_type == HRMP_LOGGING_TYPE_FILE)
         {
            buf[strftime(buf, sizeof(buf), config->log_line_prefix, tm)] = '\0';
            fprintf(log_file, "%s %-5s %s:%d ",
                    buf, levels[level - 1], filename, line);
            vfprintf(log_file, fmt, vl);
            fprintf(log_file, "\n");
            fflush(log_file);
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
        SLEEP_AND_GOTO(1000000L,retry)
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

   if (config->log_level == HRMP_LOGGING_LEVEL_DEBUG5 &&
       size > 0 &&
       (config->log_type == HRMP_LOGGING_TYPE_CONSOLE || config->log_type == HRMP_LOGGING_TYPE_FILE))
   {
retry:
      isfree = STATE_FREE;

      if (atomic_compare_exchange_strong(&config->log_lock, &isfree, STATE_IN_USE))
      {
         char buf[(3 * size) + (2 * ((size / LINE_LENGTH) + 1)) + 1 + 1];
         int j = 0;
         int k = 0;

         memset(&buf, 0, sizeof(buf));

         for (int i = 0; i < size; i++)
         {
            if (k == LINE_LENGTH)
            {
               buf[j] = '\n';
               j++;
               k = 0;
            }
            sprintf(&buf[j], "%02X", (signed char) *((char*)data + i));
            j += 2;
            k++;
         }

         buf[j] = '\n';
         j++;
         k = 0;

         for (int i = 0; i < size; i++)
         {
            signed char c = (signed char) *((char*)data + i);
            if (k == LINE_LENGTH)
            {
               buf[j] = '\n';
               j++;
               k = 0;
            }
            if (c >= 32 && c <= 127)
            {
               buf[j] = c;
            }
            else
            {
               buf[j] = '?';
            }
            j++;
            k++;
         }

         if (config->log_type == HRMP_LOGGING_TYPE_CONSOLE)
         {
            fprintf(stdout, "%s", buf);
            fprintf(stdout, "\n");
            fflush(stdout);
         }
         else if (config->log_type == HRMP_LOGGING_TYPE_FILE)
         {
            fprintf(log_file, "%s", buf);
            fprintf(log_file, "\n");
            fflush(log_file);
         }

         atomic_store(&config->log_lock, STATE_FREE);
      }
      else
        SLEEP_AND_GOTO(1000000L,retry)
   }
}
