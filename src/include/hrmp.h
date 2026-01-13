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

#ifndef HRMP_H
#define HRMP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>

#define VERSION                      "0.13.1"

#define HRMP_HOMEPAGE                "https://hrmp.github.io/"
#define HRMP_ISSUES                  "https://github.com/HighResMusicPlayer/hrmp/issues"

#define STATE_NOTINIT                -2
#define STATE_INIT                   -1
#define STATE_FREE                   0
#define STATE_IN_USE                 1

#define MAX_PROCESS_TITLE_LENGTH     256

#define UPDATE_PROCESS_TITLE_NEVER   0
#define UPDATE_PROCESS_TITLE_STRICT  1
#define UPDATE_PROCESS_TITLE_MINIMAL 2
#define UPDATE_PROCESS_TITLE_VERBOSE 3

#define DEFAULT_BUFFER_SIZE          131072
#define ALIGNMENT_SIZE               512

#define INDENT_PER_LEVEL             2
#define FORMAT_JSON                  0
#define FORMAT_TEXT                  1
#define FORMAT_JSON_COMPACT          2
#define BULLET_POINT                 "- "

#define MESSAGE_STATUS_ZERO          0
#define MESSAGE_STATUS_OK            1
#define MESSAGE_STATUS_ERROR         2

#define NUMBER_OF_DEVICES            8

#define MISC_LENGTH                  512
#define MAX_PATH                     1024

#define HRMP_DEFAULT_OUTPUT_FORMAT   "[%n/%N] %d: %f [%i] (%t/%T) (%p)"

/**
 * The shared memory segment
 */
extern void* shmem;

#define MAX(a, b) \
   ({ __typeof__ (a) _a = (a);  \
           __typeof__ (b) _b = (b);  \
           _a > _b ? _a : _b; })

#define MIN(a, b) \
   ({ __typeof__ (a) _a = (a);  \
           __typeof__ (b) _b = (b);  \
           _a < _b ? _a : _b; })

/*
 * Common piece of code to perform a sleeping.
 *
 * @param zzz the amount of time to
 * sleep, expressed as nanoseconds.
 *
 * Example
   SLEEP(5000000L)
 *
 */
#define SLEEP(zzz)                  \
   do                               \
   {                                \
      struct timespec ts_private;   \
      ts_private.tv_sec = 0;        \
      ts_private.tv_nsec = zzz;     \
      nanosleep(&ts_private, NULL); \
   }                                \
   while (0);

/*
 * Commonly used block of code to sleep
 * for a specified amount of time and
 * then jump back to a specified label.
 *
 * @param zzz how much time to sleep (as long nanoseconds)
 * @param goto_to the label to which jump to
 *
 * Example:
 *
     ...
     else
       SLEEP_AND_GOTO(100000L, retry)
 */
#define SLEEP_AND_GOTO(zzz, goto_to) \
   do                                \
   {                                 \
      struct timespec ts_private;    \
      ts_private.tv_sec = 0;         \
      ts_private.tv_nsec = zzz;      \
      nanosleep(&ts_private, NULL);  \
      goto goto_to;                  \
   }                                 \
   while (0);

/** @struct capabilities
 * Defines the capabilities of a device
 */
struct capabilities
{
   bool s16;    /**< Support signed 16bit decoding */
   bool s16_le; /**< Support signed 16bit (LE) decoding */
   bool s16_be; /**< Support signed 16bit (BE) decoding */
   bool u16;    /**< Support unsigned 16bit decoding */
   bool u16_le; /**< Support unsigned 16bit (LE) decoding */
   bool u16_be; /**< Support unsigned 16bit (BE) decoding */

   bool s24;     /**< Support signed 24bit decoding */
   bool s24_3le; /**< Support signed 24bit 3-packed (LE) decoding */
   bool s24_le;  /**< Support signed 24bit (LE) decoding */
   bool s24_be;  /**< Support signed 24bit (BE) decoding */
   bool u24;     /**< Support unsigned 24bit decoding */
   bool u24_le;  /**< Support unsigned 24bit (LE) decoding */
   bool u24_be;  /**< Support unsigned 24bit (BE) decoding */

   bool s32;    /**< Support signed 32bit decoding */
   bool s32_le; /**< Support signed 32bit (LE) decoding */
   bool s32_be; /**< Support signed 32bit (BE) decoding */
   bool u32;    /**< Support unsigned 32bit decoding */
   bool u32_le; /**< Support unsigned 32bit (LE) decoding */
   bool u32_be; /**< Support unsigned 32bit (BE) decoding */

   bool dsd_u8;     /**< Support DSD unsigned 8bit decoding */
   bool dsd_u16_le; /**< Support DSD unsigned 16bit (LE) decoding */
   bool dsd_u16_be; /**< Support DSD unsigned 16bit (BE) decoding */
   bool dsd_u32_le; /**< Support DSD unsigned 32bit (LE) decoding */
   bool dsd_u32_be; /**< Support DSD unsigned 32bit (BE) decoding */
};

/** @struct device
 * Defines a device
 */
struct device
{
   char name[MISC_LENGTH];           /**< The full name of the device */
   char device[MISC_LENGTH];         /**< The device */
   char description[MISC_LENGTH];    /**< The description of the device */
   int hardware;                     /**< The hardware number of the device */
   char selem[MISC_LENGTH];          /**< The hardware selem of the device */
   struct capabilities capabilities; /**< The capabilities of the device */
   bool active;                      /**< Is the device active ? */
   bool has_volume;                  /**< Has volume control */
   int volume;                       /**< The current volume */
   bool is_paused;                   /**< Is the active device paused ? */
};

/** @struct configuration
 * Defines the configuration and state of hrmp
 */
struct configuration
{
   char configuration_path[MAX_PATH]; /**< The configuration path */

   char device[MISC_LENGTH]; /**< The name of the default device */
   char output[MISC_LENGTH]; /**< The output format */

   struct device active_device; /**< The active device */

   bool quiet; /**< Quiet the output */

   int volume;      /**< The current volume */
   int prev_volume; /**< The previous volume */
   bool is_muted;   /**< Is muted */

   size_t cache_size; /**< The cache size */

   bool metadata; /**< Display metadata about files */

   bool experimental; /**< Allow experimental features */
   bool developer;    /**< Enable developer features */
   bool fallback;     /**< Enable fallback features */

   bool dop; /**< DoP mode */

   int log_type;                      /**< The logging type */
   int log_level;                     /**< The logging level */
   char log_path[MISC_LENGTH];        /**< The logging path */
   int log_mode;                      /**< The logging mode */
   char log_line_prefix[MISC_LENGTH]; /**< The logging prefix */
   atomic_schar log_lock;             /**< The logging lock */

   unsigned int update_process_title; /**< Behaviour for updating the process title */

   int number_of_devices; /**< The number of devices */

   struct device devices[NUMBER_OF_DEVICES]; /**< The IEC598 devices */
};

#ifdef __cplusplus
}
#endif

#endif
