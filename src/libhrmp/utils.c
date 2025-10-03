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
#include <deque.h>
#include <files.h>
#include <logging.h>
#include <utils.h>
#include <value.h>

/* system */
#include <dirent.h>
#include <err.h>
#include <errno.h>
#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif
#include <pwd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#define BUFFER_SIZE 8192

static int string_compare(const void* a, const void* b);

extern char** environ;
#if defined(HAVE_LINUX) || defined(HAVE_OSX)
static bool env_changed = false;
static int max_process_title_size = 0;
#endif

uint64_t
hrmp_read_le_u64(FILE* f)
{
   uint8_t b[8];
   uint64_t v = 0;

   if (fread(b, 1, 8, f) != 8)
   {
      return 0;
   }

   for (int i = 0; i < 8; i++)
   {
      v |= ((uint64_t)b[i]) << (8 * i);
   }

   return v;
}

uint32_t
hrmp_read_le_u32(FILE* f)
{
   uint8_t b[4];
   if (fread(b, 1, 4, f) != 4)
   {
      return 0;
   }
   return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) |
          ((uint32_t)b[3] << 24);
}

uint64_t
hrmp_read_le_u64_buffer(uint8_t* buffer)
{
   return (uint64_t)buffer[0] | ((uint64_t)buffer[1] << 8) | ((uint64_t)buffer[2] << 16) |
          ((uint64_t)buffer[3] << 24) | ((uint64_t)buffer[4] << 32) |
          ((uint64_t)buffer[5] << 40) | ((uint64_t)buffer[6] << 48) |
          ((uint64_t)buffer[7] << 56);
}

uint32_t
hrmp_read_le_u32_buffer(uint8_t* buffer)
{
   return (uint32_t)buffer[0] | ((uint32_t)buffer[1] << 8) | ((uint32_t)buffer[2] << 16) |
          ((uint32_t)buffer[3] << 24);
}

char*
hrmp_get_home_directory(void)
{
   struct passwd* pw = getpwuid(getuid());

   if (pw == NULL)
   {
      return NULL;
   }

   return pw->pw_dir;
}

bool
hrmp_is_directory(char* directory)
{
   struct stat statbuf;

   memset(&statbuf, 0, sizeof(struct stat));

   if (!lstat(directory, &statbuf))
   {
      if (S_ISDIR(statbuf.st_mode))
      {
         return true;
      }
   }

   return false;
}

bool
hrmp_is_file(char* file)
{
   struct stat statbuf;

   memset(&statbuf, 0, sizeof(struct stat));

   if (!lstat(file, &statbuf))
   {
      if (S_ISREG(statbuf.st_mode))
      {
         return true;
      }
   }

   return false;
}

int
hrmp_file_to_buffer(char* filename, size_t* buffer_size, void** buffer)
{
   FILE* f = NULL;
   size_t bs = 0;
   void* b = NULL;
   char readbuf[BUFFER_SIZE];
   size_t total = 0;

   *buffer_size = 0;
   *buffer = NULL;

   if (filename == NULL)
   {
      goto error;
   }

   f = fopen(filename, "rb");

   if (f == NULL)
   {
      goto error;
   }

   fseek(f, 0, SEEK_END);
   bs = ftell(f);
   rewind(f);

   b = malloc(bs);
   if (buffer == NULL)
   {
      goto error;
   }

   memset(b, 0, bs);

   while (1)
   {
      size_t n = fread(&readbuf[0], 1, BUFFER_SIZE, f);

      if (n > 0)
      {
         memcpy(b + total, &readbuf[0], n);
         total += n;
      }

      if (n < BUFFER_SIZE)
      {
         if (ferror(f))
         {
            goto error;
         }

         break;
      }
   }

   *buffer_size = bs;
   *buffer = b;

   fclose (f);

   return 0;

error:

   free(b);

   if (f != NULL)
   {
      fclose(f);
   }

   return 1;
}

bool
hrmp_exists(char* f)
{
   if (f != NULL)
   {
      if (access(f, F_OK) == 0)
      {
         return true;
      }
   }

   return false;
}

int
hrmp_get_files(int device, char* base, bool recursive, struct deque* files)
{
   DIR* dir = NULL;
   struct dirent* entry;

   if (base == NULL)
   {
      goto error;
   }

   if (files == NULL)
   {
      goto error;
   }

   if (!(dir = opendir(base)))
   {
      goto error;
   }

   while ((entry = readdir(dir)) != NULL)
   {
      char* d = NULL;

      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      {
         continue;
      }

      d = hrmp_append(d, base);
      if (!hrmp_ends_with(d, "/"))
      {
         d = hrmp_append_char(d, '/');
      }
      d = hrmp_append(d, entry->d_name);

      if (hrmp_is_file(d))
      {
         struct file_metadata* fm = NULL;

         if (hrmp_file_metadata(device, d, &fm) == 0)
         {
            char* v = NULL;

            v = hrmp_append(v, d);
            hrmp_deque_add(files, NULL, (uintptr_t)v, ValueString);
         }

         free(fm);
      }
      else
      {
         if (recursive && hrmp_is_directory(d))
         {
            hrmp_get_files(device, d, recursive, files);
         }
      }

      free(d);
      d = NULL;
   }

   closedir(dir);
   dir = NULL;

   return 0;

error:

   if (dir != NULL)
   {
      closedir(dir);
   }

   return 1;
}

bool
hrmp_starts_with(char* str, char* prefix)
{
   if (str == NULL)
   {
      return false;
   }
   return strncmp(prefix, str, strlen(prefix)) == 0;
}

bool
hrmp_ends_with(char* str, char* suffix)
{
   int str_len;
   int suffix_len;

   if (str != NULL && suffix != NULL)
   {
      str_len = strlen(str);
      suffix_len = strlen(suffix);

      return (str_len >= suffix_len) && (strcmp(str + (str_len - suffix_len), suffix) == 0);
   }

   return false;
}

size_t
hrmp_get_file_size(char* file_path)
{
   struct stat file_stat;

   if (stat(file_path, &file_stat) != 0)
   {
      hrmp_log_warn("hrmp_get_file_size: %s (%s)", file_path, strerror(errno));
      errno = 0;
      return 0;
   }

   return file_stat.st_size;
}

bool
hrmp_contains(char* str, char* s)
{
   return strstr(str, s) != NULL;
}

char*
hrmp_remove_first(char* str)
{
   char* new_str = NULL;

   new_str = (char*)malloc(strlen(str));

   if (new_str == NULL)
   {
      goto error;
   }

   memset(new_str, 0, strlen(str));
   memcpy(new_str, str + 1, strlen(str) - 1);

   free(str);

   return new_str;

error:

   return NULL;
}

char*
hrmp_remove_last(char* str)
{
   char* new_str = NULL;

   new_str = (char*)malloc(strlen(str));

   if (new_str == NULL)
   {
      goto error;
   }

   memset(new_str, 0, strlen(str));
   memcpy(new_str, str, strlen(str) - 1);

   free(str);

   return new_str;

error:

   return NULL;
}

void
hrmp_sort(size_t size, char** array)
{
   if (array != NULL)
   {
      qsort(array, size, sizeof(const char*), string_compare);
   }
}

static int
string_compare(const void* a, const void* b)
{
   return strcmp(*(char**)a, *(char**)b);
}

char*
hrmp_append(char* orig, char* s)
{
   size_t orig_length;
   size_t s_length;
   char* n = NULL;

   if (s == NULL)
   {
      return orig;
   }

   s_length = strlen(s);
   if (s_length == 0)
   {
      return orig;
   }

   if (orig != NULL)
   {
      orig_length = strlen(orig);
   }
   else
   {
      orig_length = 0;
   }

   n = (char*)realloc(orig, orig_length + s_length + 1);

   memcpy(n + orig_length, s, s_length);

   n[orig_length + s_length] = '\0';

   return n;
}

char*
hrmp_append_char(char* orig, char c)
{
   char str[2];

   memset(&str[0], 0, sizeof(str));
   snprintf(&str[0], 2, "%c", c);
   orig = hrmp_append(orig, str);

   return orig;
}

char*
hrmp_append_int(char* orig, int i)
{
   char number[12];

   memset(&number[0], 0, sizeof(number));
   snprintf(&number[0], 11, "%d", i);
   orig = hrmp_append(orig, number);

   return orig;
}

char*
hrmp_remove_whitespace(char* orig)
{
   size_t length;
   char c = 0;
   char* result = NULL;

   if (orig == NULL || strlen(orig) == 0)
   {
      return orig;
   }

   length = strlen(orig);

   for (size_t i = 0; i < length; i++)
   {
      c = *(orig + i);
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
      {
         /* Skip */
      }
      else
      {
         result = hrmp_append_char(result, c);
      }
   }

   return result;
}

__attribute__((unused))
static bool
calculate_offset(uint64_t addr, uint64_t* offset, char** filepath)
{
#if defined(HAVE_LINUX) && defined(HAVE_EXECINFO_H)
   char line[256];
   char* start, * end, * base_offset, * filepath_ptr;
   uint64_t start_addr, end_addr, base_offset_value;
   FILE* fp;
   bool success = false;

   fp = fopen("/proc/self/maps", "r");
   if (fp == NULL)
   {
      goto error;
   }

   while (fgets(line, sizeof(line), fp) != NULL)
   {
      // exmaple line:
      // 7fb60d1ea000-7fb60d20c000 r--p 00000000 103:02 120327460 /usr/lib/libc.so.6
      start = strtok(line, "-");
      end = strtok(NULL, " ");
      strtok(NULL, " "); // skip the next token
      base_offset = strtok(NULL, " ");
      strtok(NULL, " "); // skip the next token
      strtok(NULL, " "); // skip the next token
      filepath_ptr = strtok(NULL, " \n");
      if (start != NULL && end != NULL && base_offset != NULL && filepath_ptr != NULL)
      {
         start_addr = strtoul(start, NULL, 16);
         end_addr = strtoul(end, NULL, 16);
         if (addr >= start_addr && addr < end_addr)
         {
            success = true;
            break;
         }
      }
   }
   if (!success)
   {
      goto error;
   }

   base_offset_value = strtoul(base_offset, NULL, 16);
   *offset = addr - start_addr + base_offset_value;
   *filepath = hrmp_append(*filepath, filepath_ptr);
   if (fp != NULL)
   {
      fclose(fp);
   }
   return 0;

error:
   if (fp != NULL)
   {
      fclose(fp);
   }
   return 1;

#else
   return 1;

#endif
}

bool
hrmp_compare_string(const char* str1, const char* str2)
{
   if (str1 == NULL && str2 == NULL)
   {
      return true;
   }
   if ((str1 == NULL && str2 != NULL) || (str1 != NULL && str2 == NULL))
   {
      return false;
   }
   return strcmp(str1, str2) == 0;
}

char*
hrmp_indent(char* str, char* tag, int indent)
{
   for (int i = 0; i < indent; i++)
   {
      str = hrmp_append(str, " ");
   }
   if (tag != NULL)
   {
      str = hrmp_append(str, tag);
   }
   return str;
}

char*
hrmp_escape_string(char* str)
{
   char* translated_ec_string = NULL;
   int len = 0;
   int idx = 0;
   size_t translated_len = 0;

   if (str == NULL)
   {
      return NULL;
   }

   len = strlen(str);
   for (int i = 0; i < len; i++)
   {
      if (str[i] == '\"' || str[i] == '\\' || str[i] == '\n' || str[i] == '\t' || str[i] == '\r')
      {
         translated_len++;
      }
      translated_len++;
   }
   translated_ec_string = (char*)malloc(translated_len + 1);

   for (int i = 0; i < len; i++, idx++)
   {
      switch (str[i])
      {
         case '\\':
         case '\"':
            translated_ec_string[idx] = '\\';
            idx++;
            translated_ec_string[idx] = str[i];
            break;
         case '\n':
            translated_ec_string[idx] = '\\';
            idx++;
            translated_ec_string[idx] = 'n';
            break;
         case '\t':
            translated_ec_string[idx] = '\\';
            idx++;
            translated_ec_string[idx] = 't';
            break;
         case '\r':
            translated_ec_string[idx] = '\\';
            idx++;
            translated_ec_string[idx] = 'r';
            break;
         default:
            translated_ec_string[idx] = str[i];
            break;
      }
   }
   translated_ec_string[idx] = '\0'; // terminator

   return translated_ec_string;
}

size_t
hrmp_get_aligned_size(size_t size)
{
   size_t allocate = 0;

   allocate = size / 512;

   if (size % 512 != 0)
   {
      allocate += 1;
   }

   allocate *= 512;

   return allocate;
}

void
hrmp_set_proc_title(int argc, char** argv, char* s)
{
#if defined(HAVE_LINUX) || defined(HAVE_OSX)
   char title[MAX_PROCESS_TITLE_LENGTH];
   size_t size;
   char** env = environ;
   int es = 0;
   struct configuration* config;

   config = (struct configuration*)shmem;

   // sanity check: if the user does not want to
   // update the process title, do nothing
   if (config->update_process_title == UPDATE_PROCESS_TITLE_NEVER)
   {
      return;
   }

   if (!env_changed)
   {
      for (int i = 0; env[i] != NULL; i++)
      {
         es++;
      }

      environ = (char**)malloc(sizeof(char*) * (es + 1));
      if (environ == NULL)
      {
         return;
      }

      for (int i = 0; env[i] != NULL; i++)
      {
         size = strlen(env[i]);
         environ[i] = (char*)calloc(1, size + 1);

         if (environ[i] == NULL)
         {
            return;
         }
         memcpy(environ[i], env[i], size);
      }
      environ[es] = NULL;
      env_changed = true;
   }

   // compute how long was the command line
   // when the application was started
   if (max_process_title_size == 0)
   {
      for (int i = 0; i < argc; i++)
      {
         max_process_title_size += strlen(argv[i]) + 1;
      }
   }

   // compose the new title
   memset(&title, 0, sizeof(title));
   snprintf(title, sizeof(title) - 1, "hrmp: %s", s != NULL ? s : "");

   // nuke the command line info
   memset(*argv, 0, max_process_title_size);

   // copy the new title over argv checking
   // the update_process_title policy
   if (config->update_process_title == UPDATE_PROCESS_TITLE_STRICT)
   {
      size = max_process_title_size;
   }
   else
   {
      // here we can set the title to a full description
      size = strlen(title) + 1;
   }

   memcpy(*argv, title, size);
   memset(*argv + size, 0, 1);

   // keep track of how long is now the title
   max_process_title_size = size;

#else
   setproctitle("-hrmp: %s", s1 != NULL ? s1 : "");

#endif
}

int
hrmp_backtrace(void)
{
   char* s = NULL;
   int ret = 0;

   ret = hrmp_backtrace_string(&s);

   if (s != NULL)
   {
      hrmp_log_debug(s);
   }

   free(s);

   return ret;
}

int
hrmp_backtrace_string(char** s)
{
#ifdef  HAVE_EXECINFO_H
   void* bt[1024];
   char* log_str = NULL;
   size_t bt_size;

   *s = NULL;

   bt_size = backtrace(bt, 1024);
   if (bt_size == 0)
   {
      goto error;
   }

   log_str = hrmp_append(log_str, "Backtrace:\n");

   // the first element is ___interceptor_backtrace, so we skip it
   for (size_t i = 1; i < bt_size; i++)
   {
      uint64_t addr = (uint64_t)bt[i];
      uint64_t offset;
      char* filepath = NULL;
      char cmd[256], buffer[256], log_buffer[64];
      bool found_main = false;
      FILE* pipe;

      if (calculate_offset(addr, &offset, &filepath))
      {
         continue;
      }

      snprintf(cmd, sizeof(cmd), "addr2line -e %s -fC 0x%lx", filepath, offset);
      free(filepath);
      filepath = NULL;

      pipe = popen(cmd, "r");
      if (pipe == NULL)
      {
         hrmp_log_debug("Failed to run command: %s, reason: %s", cmd, strerror(errno));
         continue;
      }

      if (fgets(buffer, sizeof(buffer), pipe) == NULL)
      {
         hrmp_log_debug("Failed to read from command output: %s", strerror(errno));
         pclose(pipe);
         continue;
      }
      buffer[strlen(buffer) - 1] = '\0'; // Remove trailing newline
      if (strcmp(buffer, "main") == 0)
      {
         found_main = true;
      }
      snprintf(log_buffer, sizeof(log_buffer), "#%zu  0x%lx in ", i - 1, addr);
      log_str = hrmp_append(log_str, log_buffer);
      log_str = hrmp_append(log_str, buffer);
      log_str = hrmp_append(log_str, "\n");

      if (fgets(buffer, sizeof(buffer), pipe) == NULL)
      {
         log_str = hrmp_append(log_str, "\tat ???:??\n");
      }
      else
      {
         buffer[strlen(buffer) - 1] = '\0'; // Remove trailing newline
         log_str = hrmp_append(log_str, "\tat ");
         log_str = hrmp_append(log_str, buffer);
         log_str = hrmp_append(log_str, "\n");
      }

      pclose(pipe);
      if (found_main)
      {
         break;
      }
   }

   *s = log_str;

   return 0;

error:
   if (log_str != NULL)
   {
      free(log_str);
   }
   return 1;
#else
   return 1;
#endif
}
