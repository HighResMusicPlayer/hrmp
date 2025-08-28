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
#include <configuration.h>
#include <logging.h>
#include <shmem.h>
#include <stddef.h>
#include <utils.h>

/* system */
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#define LINE_LENGTH 512

static int extract_key_value(char* str, char** key, char** value);
static int as_int(char* str, int* i);
static int as_logging_type(char* str);
static int as_logging_level(char* str);
static int as_logging_mode(char* str);

static unsigned int as_update_process_title(char* str, unsigned int* policy, unsigned int default_policy);

static bool is_empty_string(char* s);
static bool is_same_device(struct device* d1, struct device* d2);

static bool key_in_section(char* wanted, char* section, char* key, bool global, bool* unknown);
static bool is_comment_line(char* line);
static bool section_line(char* line, char* section);

static int hrmp_write_device_config_value(char* buffer, char* device_name, char* config_key, size_t buffer_size);

static int to_string(char* where, char* value, size_t max_length);
static int to_update_process_title(char* where, int value);
static int to_log_mode(char* where, int value);
static int to_log_level(char* where, int value);
static int to_log_type(char* where, int value);

/**
 * This struct is going to store the metadata
 * about which sections have been parsed during
 * the configuration read.
 * This can be used to seek for duplicated sections
 * at different positions in the configuration file.
 */
struct config_section
{
   char name[LINE_LENGTH];  /**< The name of the section */
   unsigned int lineno;     /**< The line number for this section */
   bool main;               /**< Is this the main configuration section or a server one? */
};

/**
 *
 */
int
hrmp_init_configuration(void* shm)
{
   struct configuration* config;

   config = (struct configuration*)shm;

   config->log_type = HRMP_LOGGING_TYPE_CONSOLE;
   config->log_level = HRMP_LOGGING_LEVEL_INFO;
   config->log_mode = HRMP_LOGGING_MODE_APPEND;
   atomic_init(&config->log_lock, STATE_FREE);

   config->update_process_title = UPDATE_PROCESS_TITLE_VERBOSE;

   return 0;
}

/**
 *
 */
int
hrmp_read_configuration(void* shm, char* filename, bool emitWarnings)
{
   FILE* file;
   char section[LINE_LENGTH];
   char line[LINE_LENGTH];
   char* key = NULL;
   char* value = NULL;
   size_t max;
   int idx_device = 0;
   struct device drv;
   struct configuration* config;
   bool has_main_section = false;

   // the max number of sections allowed in the configuration
   // file is done by the max number of devices plus the main `hrmp`
   // configuration section
   struct config_section sections[NUMBER_OF_DEVICES + 1];
   int idx_sections = 0;
   int lineno = 0;
   int return_value = 0;

   file = fopen(filename, "r");

   if (!file)
   {
      return HRMP_CONFIGURATION_STATUS_FILE_NOT_FOUND;
   }

   memset(&section, 0, LINE_LENGTH);
   memset(&sections, 0, sizeof(struct config_section) * NUMBER_OF_DEVICES + 1);
   config = (struct configuration*)shm;

   while (fgets(line, sizeof(line), file))
   {
      lineno++;

      if (!is_empty_string(line) && !is_comment_line(line))
      {
         if (section_line(line, section))
         {
            // check we don't overflow the number of available sections
            if (idx_sections >= NUMBER_OF_DEVICES + 1)
            {
               warnx("Max number of sections (%d) in configuration file <%s> reached!",
                     NUMBER_OF_DEVICES + 1,
                     filename);
               return HRMP_CONFIGURATION_STATUS_FILE_TOO_BIG;
            }

            // initialize the section structure
            memset(sections[idx_sections].name, 0, LINE_LENGTH);
            memcpy(sections[idx_sections].name, section, strlen(section));
            sections[idx_sections].lineno = lineno;
            sections[idx_sections].main = !strncmp(section, HRMP_MAIN_INI_SECTION, LINE_LENGTH);
            if (sections[idx_sections].main)
            {
               has_main_section = true;
            }

            idx_sections++;

            if (strcmp(section, HRMP_MAIN_INI_SECTION))
            {
               if (idx_device > 0 && idx_device <= NUMBER_OF_DEVICES)
               {
                  memcpy(&(config->devices[idx_device - 1]), &drv, sizeof(struct device));
               }
               else if (idx_device > NUMBER_OF_DEVICES)
               {
                  printf("Maximum number of devices exceeded\n");
               }

               memset(&drv, 0, sizeof(struct device));
               drv.active = false;
               memset(&drv.name, 0, sizeof(drv.name));
               memcpy(&drv.name, &section, strlen(section));
               idx_device++;
            }
         }
         else
         {
            extract_key_value(line, &key, &value);

            if (key && value)
            {
               bool unknown = false;

               if (key_in_section("device", section, key, true, NULL))
               {
                  max = strlen(value);
                  if (max > MISC_LENGTH - 1)
                  {
                     max = MISC_LENGTH - 1;
                  }
                  memcpy(config->device, value, max);
               }
               else if (key_in_section("device", section, key, false, &unknown))
               {
                  max = strlen(section);
                  if (max > MISC_LENGTH - 1)
                  {
                     max = MISC_LENGTH - 1;
                  }
                  memset(&drv.name, 0, sizeof(drv.name));
                  memcpy(&drv.name, section, max);
                  max = strlen(value);
                  if (max > MISC_LENGTH - 1)
                  {
                     max = MISC_LENGTH - 1;
                  }
                  memcpy(&drv.device, value, max);
                  drv.active = false;
               }
               else if (key_in_section("description", section, key, false, &unknown))
               {
                  max = strlen(section);
                  if (max > MISC_LENGTH - 1)
                  {
                     max = MISC_LENGTH - 1;
                  }
                  memset(&drv.name, 0, sizeof(drv.name));
                  memcpy(&drv.name, section, max);
                  max = strlen(value);
                  if (max > MISC_LENGTH - 1)
                  {
                     max = MISC_LENGTH - 1;
                  }
                  memset(&drv.description, 0, sizeof(drv.description));
                  memcpy(&drv.description, value, max);
                  drv.active = false;
               }
               else if (key_in_section("log_type", section, key, true, &unknown))
               {
                  config->log_type = as_logging_type(value);
               }
               else if (key_in_section("log_level", section, key, true, &unknown))
               {
                  config->log_level = as_logging_level(value);
               }
               else if (key_in_section("log_path", section, key, true, &unknown))
               {
                  max = strlen(value);
                  if (max > MISC_LENGTH - 1)
                  {
                     max = MISC_LENGTH - 1;
                  }
                  memcpy(config->log_path, value, max);
               }
               else if (key_in_section("log_line_prefix", section, key, true, &unknown))
               {
                  max = strlen(value);
                  if (max > MISC_LENGTH - 1)
                  {
                     max = MISC_LENGTH - 1;
                  }

                  memcpy(config->log_line_prefix, value, max);
               }
               else if (key_in_section("log_mode", section, key, true, &unknown))
               {
                  config->log_mode = as_logging_mode(value);
               }
               else if (key_in_section("update_process_title", section, key, true, &unknown))
               {
                  if (as_update_process_title(value, &config->update_process_title, UPDATE_PROCESS_TITLE_VERBOSE))
                  {
                     unknown = false;
                  }
               }
               else
               {
                  unknown = true;
               }

               if (unknown && emitWarnings)
               {
                  // we cannot use logging here...
                  // if we have a section, the key is not known,
                  // otherwise it is outside of a section at all
                  if (strlen(section) > 0)
                  {
                     warnx("Unknown key <%s> with value <%s> in section [%s] (line %d of file <%s>)",
                           key,
                           value,
                           section,
                           lineno,
                           filename);
                  }
                  else
                  {
                     warnx("Key <%s> with value <%s> out of any section (line %d of file <%s>)",
                           key,
                           value,
                           lineno,
                           filename);
                  }
               }

               free(key);
               free(value);
               key = NULL;
               value = NULL;
            }
         }
      }
   }

   if (strlen(drv.name) > 0)
   {
      memcpy(&(config->devices[idx_device - 1]), &drv, sizeof(struct device));
   }

   config->number_of_devices = idx_device;

   fclose(file);

   // check there is at least one main section
   if (!has_main_section)
   {
      warnx("No main configuration section [%s] found in file <%s>",
            HRMP_MAIN_INI_SECTION,
            filename);
      return HRMP_CONFIGURATION_STATUS_KO;
   }

   // validate the sections:
   // do a nested loop to scan over all the sections that have a duplicated
   // name and warn the user about them.
   for (int i = 0; i < NUMBER_OF_DEVICES + 1; i++)
   {
      for (int j = i + 1; j < NUMBER_OF_DEVICES + 1; j++)
      {
         // skip uninitialized sections
         if (!strlen(sections[i].name) || !strlen(sections[j].name))
         {
            continue;
         }

         if (!strncmp(sections[i].name, sections[j].name, LINE_LENGTH))
         {
            // cannot log here ...
            warnx("%s section [%s] duplicated at lines %d and %d of file <%s>",
                  sections[i].main ? "Main" : "Server",
                  sections[i].name,
                  sections[i].lineno,
                  sections[j].lineno,
                  filename);
            return_value++;    // this is an error condition!
         }
      }
   }

   return return_value;
}

/**
 *
 */
int
hrmp_validate_configuration(void* shm)
{
   struct configuration* config;

   config = (struct configuration*)shm;

   if (config->number_of_devices <= 0)
   {
      hrmp_log_fatal("hrmp: No devices defined");
      return 1;
   }

   for (int i = 0; i < config->number_of_devices; i++)
   {
      if (strlen(config->devices[i].device) == 0)
      {
         hrmp_log_fatal("hrmp: No device defined for %s", config->devices[i].name);
         return 1;
      }
   }

   for (int i = 0; i < config->number_of_devices; i++)
   {
      for (int j = i + 1; j < config->number_of_devices; j++)
      {
         if (is_same_device(&config->devices[i], &config->devices[j]))
         {
            hrmp_log_fatal("hrmp: Devices [%s] and [%s] are duplicated!",
                           config->devices[i].name,
                           config->devices[j].name);
            return 1;
         }
      }
   }

   return 0;
}

/**
 * Given a line of text extracts the key part and the value.
 * Valid lines must have the form <key> = <value>.
 *
 * The key must be unquoted and cannot have any spaces
 * in front of it.
 *
 * Comments on the right side of a value are allowed.
 *
 * The value can be quoted, and this allows for inserting spaces
 * and comment signs. Quotes are '""' and '\''.
 * Example of valid lines are:
 * <code>
 * foo = bar
 * foo=bar
 * foo=  bar
 * foo = "bar"
 * foo = 'bar'
 * foo = "#bar"
 * foo = '#bar'
 * foo = bar # bar set!
 * foo = bar# bar set!
 * </code>
 *
 * @param str the line of text incoming from the configuration file
 * @param key the pointer to where to store the key extracted from the line
 * @param value the pointer to where to store the value (unquoted)
 * @returns 1 if unable to parse the line, 0 if everything is ok
 */
static int
extract_key_value(char* str, char** key, char** value)
{
   char* equal = NULL;
   char* end = NULL;
   char* ptr = NULL;
   char left[MISC_LENGTH];
   char right[MISC_LENGTH];
   bool start_left = false;
   bool start_right = false;
   int idx = 0;
   int i = 0;
   char c = 0;
   char* k = NULL;
   char* v = NULL;

   *key = NULL;
   *value = NULL;

   memset(left, 0, sizeof(left));
   memset(right, 0, sizeof(right));

   equal = strchr(str, '=');

   if (equal != NULL)
   {
      i = 0;
      while (true)
      {
         ptr = str + i;
         if (ptr != equal)
         {
            c = *(str + i);
            if (c == '\t' || c == ' ' || c == '\"' || c == '\'')
            {
               /* Skip */
            }
            else
            {
               start_left = true;
            }

            if (start_left)
            {
               left[idx] = c;
               idx++;
            }
         }
         else
         {
            break;
         }
         i++;
      }

      end = strchr(str, '\n');
      idx = 0;

      for (size_t i = 0; i < strlen(equal); i++)
      {
         ptr = equal + i;
         if (ptr != end)
         {
            c = *(ptr);
            if (c == '=' || c == ' ' || c == '\t' || c == '\"' || c == '\'')
            {
               /* Skip */
            }
            else
            {
               start_right = true;
            }

            if (start_right)
            {
               if (c != '#')
               {
                  right[idx] = c;
                  idx++;
               }
               else
               {
                  break;
               }
            }
         }
         else
         {
            break;
         }
      }

      for (int i = strlen(left); i >= 0; i--)
      {
         if (left[i] == '\t' || left[i] == ' ' || left[i] == '\0' || left[i] == '\"' || left[i] == '\'')
         {
            left[i] = '\0';
         }
         else
         {
            break;
         }
      }

      for (int i = strlen(right); i >= 0; i--)
      {
         if (right[i] == '\t' || right[i] == ' ' || right[i] == '\0' || right[i] == '\r' || right[i] == '\"' || right[i] == '\'')
         {
            right[i] = '\0';
         }
         else
         {
            break;
         }
      }

      k = calloc(1, strlen(left) + 1);

      if (k == NULL)
      {
         goto error;
      }

      v = calloc(1, strlen(right) + 1);

      if (v == NULL)
      {
         goto error;
      }

      memcpy(k, left, strlen(left));
      memcpy(v, right, strlen(right));

      *key = k;
      *value = v;
   }

   return 0;

error:

   free(k);
   free(v);

   return 1;
}

static int
as_int(char* str, int* i)
{
   char* endptr;
   long val;

   errno = 0;
   val = strtol(str, &endptr, 10);

   if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) ||
       (errno != 0 && val == 0))
   {
      goto error;
   }

   if (str == endptr)
   {
      goto error;
   }

   if (*endptr != '\0')
   {
      goto error;
   }

   *i = (int)val;

   return 0;

error:

   errno = 0;

   return 1;
}

static int
as_logging_type(char* str)
{
   if (!strcasecmp(str, "console"))
   {
      return HRMP_LOGGING_TYPE_CONSOLE;
   }

   if (!strcasecmp(str, "file"))
   {
      return HRMP_LOGGING_TYPE_FILE;
   }

   if (!strcasecmp(str, "syslog"))
   {
      return HRMP_LOGGING_TYPE_SYSLOG;
   }

   return HRMP_LOGGING_TYPE_CONSOLE;
}

static int
as_logging_level(char* str)
{
   size_t size = 0;
   int debug_level = 1;
   char* debug_value = NULL;

   if (!strncasecmp(str, "debug", strlen("debug")))
   {
      if (strlen(str) > strlen("debug"))
      {
         size = strlen(str) - strlen("debug");
         debug_value = (char*)malloc(size + 1);
         memset(debug_value, 0, size + 1);
         memcpy(debug_value, str + 5, size);
         if (as_int(debug_value, &debug_level))
         {
            // cannot parse, set it to 1
            debug_level = 1;
         }
         free(debug_value);
      }

      if (debug_level <= 1)
      {
         return HRMP_LOGGING_LEVEL_DEBUG1;
      }
      else if (debug_level == 2)
      {
         return HRMP_LOGGING_LEVEL_DEBUG2;
      }
      else if (debug_level == 3)
      {
         return HRMP_LOGGING_LEVEL_DEBUG3;
      }
      else if (debug_level == 4)
      {
         return HRMP_LOGGING_LEVEL_DEBUG4;
      }
      else if (debug_level >= 5)
      {
         return HRMP_LOGGING_LEVEL_DEBUG5;
      }
   }

   if (!strcasecmp(str, "info"))
   {
      return HRMP_LOGGING_LEVEL_INFO;
   }

   if (!strcasecmp(str, "warn"))
   {
      return HRMP_LOGGING_LEVEL_WARN;
   }

   if (!strcasecmp(str, "error"))
   {
      return HRMP_LOGGING_LEVEL_ERROR;
   }

   if (!strcasecmp(str, "fatal"))
   {
      return HRMP_LOGGING_LEVEL_FATAL;
   }

   return HRMP_LOGGING_LEVEL_INFO;
}

static int
as_logging_mode(char* str)
{
   if (!strcasecmp(str, "a") || !strcasecmp(str, "append"))
   {
      return HRMP_LOGGING_MODE_APPEND;
   }

   if (!strcasecmp(str, "c") || !strcasecmp(str, "create"))
   {
      return HRMP_LOGGING_MODE_CREATE;
   }

   return HRMP_LOGGING_MODE_APPEND;
}

/**
 * Checks if the configuration of the first server
 * is the same as the configuration of the second server.
 * So far it tests for the same connection string, meaning
 * that the hostname and the port must be the same (i.e.,
 * pointing to the same endpoint).
 * It does not resolve the hostname, therefore 'localhost' and '127.0.0.1'
 * are considered as different hosts.
 * @return true if the server configurations look the same
 */
static bool
is_same_device(struct device* d1, struct device* d2)
{
   if (!strncmp(d1->device, d2->device, MISC_LENGTH))
   {
      return true;
   }
   else
   {
      return false;
   }
}

static bool
is_empty_string(char* s)
{
   if (s == NULL)
   {
      return true;
   }

   if (!strcmp(s, ""))
   {
      return true;
   }

   for (size_t i = 0; i < strlen(s); i++)
   {
      if (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')
      {
         /* Ok */
      }
      else
      {
         return false;
      }
   }

   return true;
}

/**
 * Function to check if the specified key belongs to the right section.
 * The idea is to pass all the values read from the configuration file,
 * and a boolean parameter to check if the section the parameter belongs is global or not.
 * A global section is the main `hrmp` section, while a local section
 * is a custom user section, i.e., a server section.
 *
 * @param wanted the key we want to match against
 * @param section the section in which the key has been found
 * @param key the key read from the configuration file
 * @param global true if the `section` has to be `hrmp`
 * @param unknown set to true if the key does match but the section does not.
 *        For instance the key `host` found in a local section while required
 *        to be global will set `unknown` to true.
 *        This parameter can be omitted.
 *
 * @returns true if the key matches and the section is of the specified type.
 *
 * Example:
 *  key_in_section("host", section, key, true, &unknown); // search for [hrmp] -> host
 *  key_in_section("port", section, key, false, &unknown); // search for server section -> port
 */
static bool
key_in_section(char* wanted, char* section, char* key, bool global, bool* unknown)
{

   // first of all, look for a key match
   if (strncmp(wanted, key, MISC_LENGTH))
   {
      // no match at all
      return false;
   }

   // if here there is a match on the key, ensure the section is
   // appropriate
   if (global && !strncmp(section, HRMP_MAIN_INI_SECTION, MISC_LENGTH))
   {
      return true;
   }
   else if (!global && strlen(section) > 0)
   {
      return true;
   }
   else
   {
      if (unknown)
      {
         *unknown = true;
      }

      return false;
   }
}

/**
 * Function to see if the specified line is a comment line
 * and has to be ignored.
 * A comment line is a line that starts with '#' or ';' or
 * with spaces (or tabs) and a comment sign.
 *
 * @param line the line read from the file
 * @return true if the line is a full comment line
 */
static bool
is_comment_line(char* line)
{
   int c = 0;
   int length = strlen(line);

   while (c < length)
   {
      if (line[c] == '#' || line[c] == ';')
      {
         return true;
      }
      else if (line[c] != ' ' && line[c] != '\t')
      {
         break;
      }

      c++;
   }

   return false;
}

/**
 * Function to inspect a configuration line and detect if it handles a section.
 * If the line handles a section name, like `[hrmp]` the function does set
 * the `section` argument, otherwise it does nothing.
 *
 * @param line the line to inspect
 * @param section the pointer to the string that will contain
 *        the section name, only if the line handles a section, otherwise
 *        the pointer will not be changed.
 *
 * @returns true if the line handles a section and the `section` pointer
 *          has been changed
 */
static bool
section_line(char* line, char* section)
{
   size_t max;
   char* ptr = NULL;

   // if does not appear to be a section line do nothing!
   if (line[0] != '[')
   {
      return false;
   }

   ptr = strchr(line, ']');
   if (ptr)
   {
      memset(section, 0, LINE_LENGTH);
      max = ptr - line - 1;
      if (max > MISC_LENGTH - 1)
      {
         max = MISC_LENGTH - 1;
      }
      memcpy(section, line + 1, max);
      return true;
   }

   return false;

}

/**
 * Utility function to understand the setting for updating
 * the process title.
 *
 * @param str the value obtained by the configuration parsing
 * @param policy the pointer to the value where the setting will be stored
 * @param default_policy a value to set when the configuration cannot be
 * understood
 *
 * @return 0 on success, 1 on error. In any case the `policy` variable is set to
 * `default_policy`.
 */
static unsigned int
as_update_process_title(char* str, unsigned int* policy, unsigned int default_policy)
{
   if (is_empty_string(str))
   {
      *policy = default_policy;
      return 1;
   }

   if (!strncmp(str, "never", MISC_LENGTH) || !strncmp(str, "off", MISC_LENGTH))
   {
      *policy = UPDATE_PROCESS_TITLE_NEVER;
      return 0;
   }
   else if (!strncmp(str, "strict", MISC_LENGTH))
   {
      *policy = UPDATE_PROCESS_TITLE_STRICT;
      return 0;
   }
   else if (!strncmp(str, "minimal", MISC_LENGTH))
   {
      *policy = UPDATE_PROCESS_TITLE_MINIMAL;
      return 0;
   }
   else if (!strncmp(str, "verbose", MISC_LENGTH) || !strncmp(str, "full", MISC_LENGTH))
   {
      *policy = UPDATE_PROCESS_TITLE_VERBOSE;
      return 0;
   }
   else
   {
      // not a valid setting
      *policy = default_policy;
      return 1;
   }

}

int
hrmp_write_config_value(char* buffer, char* config_key, size_t buffer_size)
{
   struct configuration* config;

   char section[MISC_LENGTH];
   char context[MISC_LENGTH];
   char key[MISC_LENGTH];
   int begin = -1, end = -1;
   bool main_section;

   config = (struct configuration*)shmem;

   memset(section, 0, MISC_LENGTH);
   memset(context, 0, MISC_LENGTH);
   memset(key, 0, MISC_LENGTH);

   for (size_t i = 0; i < strlen(config_key); i++)
   {
      if (config_key[i] == '.')
      {
         if (!strlen(section))
         {
            memcpy(section, &config_key[begin], end - begin + 1);
            section[end - begin + 1] = '\0';
            begin = end = -1;
            continue;
         }
         else if (!strlen(context))
         {
            memcpy(context, &config_key[begin], end - begin + 1);
            context[end - begin + 1] = '\0';
            begin = end = -1;
            continue;
         }
         else if (!strlen(key))
         {
            memcpy(key, &config_key[begin], end - begin + 1);
            key[end - begin + 1] = '\0';
            begin = end = -1;
            continue;
         }

      }

      if (begin < 0)
      {
         begin = i;
      }

      end = i;

   }

   // if the key has not been found, since there is no ending dot,
   // try to extract it from the string
   if (!strlen(key))
   {
      memcpy(key, &config_key[begin], end - begin + 1);
      key[end - begin + 1] = '\0';
   }

   // force the main section, i.e., global parameters, if and only if
   // there is no section or section is 'hrmp' without any subsection
   main_section = (!strlen(section) || !strncmp(section, "hrmp", MISC_LENGTH))
                  && !strlen(context);

   if (!strncmp(section, "device", MISC_LENGTH))
   {
      return hrmp_write_device_config_value(buffer, context, key, buffer_size);
   }
   else if (main_section)
   {
      if (!strncmp(key, "log_type", MISC_LENGTH))
      {
         return to_log_type(buffer, config->log_type);
      }
      else if (!strncmp(key, "log_mode", MISC_LENGTH))
      {
         return to_log_mode(buffer, config->log_mode);
      }
      else if (!strncmp(key, "log_line_prefix", MISC_LENGTH))
      {
         return to_string(buffer, config->log_line_prefix, buffer_size);
      }

      else if (!strncmp(key, "log_level", MISC_LENGTH))
      {
         return to_log_level(buffer, config->log_level);
      }
      else if (!strncmp(key, "log_path", MISC_LENGTH))
      {
         return to_string(buffer, config->log_path, buffer_size);
      }
      else if (!strncmp(key, "update_process_title", MISC_LENGTH))
      {
         return to_update_process_title(buffer, config->update_process_title);
      }
      else
      {
         goto error;
      }

   } // end of global configuration settings
   else
   {
      goto error;
   }

   return 0;
error:
   hrmp_log_debug("Unknown configuration key <%s>", config_key);
   return 1;

}

/**
 * Function to extract a configuration value for a specific server.
 * @param device_name the name of the device
 * @param config_key one of the configuration keys allowed in the server section
 * @param buffer the buffer where to write the stringified version of the value
 * @param buffer_size the max size of the buffer where the result will be stored
 * @return 0 on success
 */
static int
hrmp_write_device_config_value(char* buffer, char* device_name, char* config_key, size_t buffer_size)
{
   int device_index = -1;
   struct configuration* config;

   config = (struct configuration*)shmem;

   for (int i = 0; i < NUMBER_OF_DEVICES; i++)
   {
      if (!strncmp(config->devices[i].name, device_name, MISC_LENGTH))
      {
         /* this is the right server */
         device_index = i;
         break;
      }
   }

   if (device_index < 0 || device_index > NUMBER_OF_DEVICES)
   {
      hrmp_log_debug("Unable to find a device named <%s> in the current configuration", device_name);
      goto error;
   }

   return 0;

error:
   return 1;
}

/**
 * An utility function to place a string into another string.
 *
 * In the case the string has inner spaces, such spaces are quoted. The function
 * tries to be as smart as possible identifying if there is the need for
 * single or double quotes.
 *
 * The function accepts the size of the destination string, and before writing
 * into such a string the result, it zero fills it. This means it is not mandatory
 * to zero fill the destination string before calling this function.
 * Also please note that if the string that is copied into the destination string
 * has a length bigger than that specified, the function will not copy any data
 * (and will not zero set the destination string, that will remain untouched!)
 *
 * @param where the string where to print the value, must be already allocated
 * @param value the value to convert into a string
 * @param max_length the max length of the 'where' destination string
 * @return 0 on success, 1 otherwise
 */
static int
to_string(char* where, char* value, size_t max_length)
{
   bool needs_quotes = false;
   bool has_double_quotes = false;
   bool has_single_quotes = false;
   char quoting_char = '\0';
   int index = 0;

   if (!where || !value || strlen(value) >= max_length)
   {
      return 1;
   }

   // assume strings with spaces must be quoted
   for (size_t i = 0; i < strlen(value); i++)
   {
      if (value[i] == ' ')
      {
         needs_quotes = true;
      }
      else if (value[i] == '"')
      {
         has_double_quotes = true;
      }
      else if (value[i] == '\'')
      {
         has_single_quotes = true;
      }

   }

   needs_quotes = needs_quotes || has_double_quotes || has_single_quotes;

   if (needs_quotes)
   {
      // there must be space for quotes
      if (strlen(value) > max_length - 2 - 1)
      {
         return 1;
      }

      if (!has_single_quotes)
      {
         quoting_char = '\'';
      }
      else if (!has_double_quotes)
      {
         quoting_char = '"';
      }

   }

   // if here, the size of the string is appropriate,
   // so do the copy
   memset(where, 0, max_length);

   if (needs_quotes)
   {
      memcpy(&where[index], &quoting_char, sizeof(quoting_char));
      index += sizeof(quoting_char);
   }

   memcpy(&where[index], value, strlen(value));
   index += strlen(value);

   if (needs_quotes)
   {
      memcpy(&where[index], &quoting_char, sizeof(quoting_char));
      index += sizeof(quoting_char);
   }

   where[index] = '\0';

   return 0;
}

/**
 * An utility function to convert the enumeration of values for the update_process_title
 * into one of its possible string descriptions.
 *
 * @param where the buffer used to store the stringy thing
 * @param value the config->update_process_title setting
 * @return 0 on success, 1 otherwise
 */
static int
to_update_process_title(char* where, int value)
{
   if (!where || value < 0)
   {
      return 1;
   }

   switch (value)
   {
      case UPDATE_PROCESS_TITLE_VERBOSE:
         snprintf(where, MISC_LENGTH, "%s", "verbose");
         break;
      case UPDATE_PROCESS_TITLE_MINIMAL:
         snprintf(where, MISC_LENGTH, "%s", "minimal");

         break;
      case UPDATE_PROCESS_TITLE_STRICT:
         snprintf(where, MISC_LENGTH, "%s", "strict");
         break;
      case UPDATE_PROCESS_TITLE_NEVER:
         snprintf(where, MISC_LENGTH, "%s", "never");
         break;
   }
   return 0;
}

/**
 * An utility function to convert the enumeration of values for the log_level setting
 * into one of its possible string descriptions.
 *
 * @param where the buffer used to store the stringy thing
 * @param value the config->log_level setting
 * @return 0 on success, 1 otherwise
 */
static int
to_log_level(char* where, int value)
{
   if (!where || value < 0)
   {
      return 1;
   }

   switch (value)
   {

      case HRMP_LOGGING_LEVEL_DEBUG2:
         snprintf(where, MISC_LENGTH, "%s", "debug2");
         break;
      case HRMP_LOGGING_LEVEL_DEBUG1:
         snprintf(where, MISC_LENGTH, "%s", "debug");
         break;
      case HRMP_LOGGING_LEVEL_INFO:
         snprintf(where, MISC_LENGTH, "%s", "info");
         break;
      case HRMP_LOGGING_LEVEL_WARN:
         snprintf(where, MISC_LENGTH, "%s", "warn");
         break;
      case HRMP_LOGGING_LEVEL_ERROR:
         snprintf(where, MISC_LENGTH, "%s", "error");
         break;
      case HRMP_LOGGING_LEVEL_FATAL:
         snprintf(where, MISC_LENGTH, "%s", "fatal");
         break;

   }

   return 0;
}

/**
 * An utility function to convert the enumeration of values for the log_level setting
 * into one of its possible string descriptions.
 *
 * @param where the buffer used to store the stringy thing
 * @param value the config->log_mode setting
 * @return 0 on success, 1 otherwise
 */
static int
to_log_mode(char* where, int value)
{
   if (!where || value < 0)
   {
      return 1;
   }

   switch (value)
   {

      case HRMP_LOGGING_MODE_CREATE:
         snprintf(where, MISC_LENGTH, "%s", "create");
         break;
      case HRMP_LOGGING_MODE_APPEND:
         snprintf(where, MISC_LENGTH, "%s", "append");
         break;
   }

   return 0;
}

/**
 * An utility function to convert the enumeration of values for the log_type setting
 * into one of its possible string descriptions.
 *
 * @param where the buffer used to store the stringy thing
 * @param value the config->log_type setting
 * @return 0 on success, 1 otherwise
 */
static int
to_log_type(char* where, int value)
{
   if (!where || value < 0)
   {
      return 1;
   }

   switch (value)
   {
      case HRMP_LOGGING_TYPE_CONSOLE:
         snprintf(where, MISC_LENGTH, "%s", "console");
         break;
      case HRMP_LOGGING_TYPE_FILE:
         snprintf(where, MISC_LENGTH, "%s", "file");
         break;
      case HRMP_LOGGING_TYPE_SYSLOG:
         snprintf(where, MISC_LENGTH, "%s", "syslog");
         break;

   }

   return 0;
}
