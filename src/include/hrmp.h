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

#ifndef HRMP_H
#define HRMP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdatomic.h>
#include <stdbool.h>

#define VERSION "0.1.0"

#define HRMP_HOMEPAGE "https://hrmp.github.io/"
#define HRMP_ISSUES "https://github.com/HighResMusicPlayer/hrmp/issues"

#define STATE_NOTINIT -2
#define STATE_INIT    -1
#define STATE_FREE     0
#define STATE_IN_USE   1

#define UPDATE_PROCESS_TITLE_NEVER   0
#define UPDATE_PROCESS_TITLE_STRICT  1
#define UPDATE_PROCESS_TITLE_MINIMAL 2
#define UPDATE_PROCESS_TITLE_VERBOSE 3

#define NUMBER_OF_DEVICES 8

#define MISC_LENGTH  512
#define MAX_PATH    1024

/**
 * The shared memory segment
 */
extern void* shmem;

#define MAX(a, b)               \
        ({ __typeof__ (a) _a = (a);  \
           __typeof__ (b) _b = (b);  \
           _a > _b ? _a : _b; })

#define MIN(a, b)               \
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
        } while (0);

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
#define SLEEP_AND_GOTO(zzz, goto_to)    \
        do                                   \
        {                                    \
           struct timespec ts_private;       \
           ts_private.tv_sec = 0;            \
           ts_private.tv_nsec = zzz;         \
           nanosleep(&ts_private, NULL);     \
           goto goto_to;                     \
        } while (0);

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

   bool s24;    /**< Support signed 24bit decoding */
   bool s24_le; /**< Support signed 24bit (LE) decoding */
   bool s24_be; /**< Support signed 24bit (BE) decoding */
   bool u24;    /**< Support unsigned 24bit decoding */
   bool u24_le; /**< Support unsigned 24bit (LE) decoding */
   bool u24_be; /**< Support unsigned 24bit (BE) decoding */

   bool s32;    /**< Support signed 32bit decoding */
   bool s32_le; /**< Support signed 32bit (LE) decoding */
   bool s32_be; /**< Support signed 32bit (BE) decoding */
   bool u32;    /**< Support unsigned 32bit decoding */
   bool u32_le; /**< Support unsigned 32bit (LE) decoding */
   bool u32_be; /**< Support unsigned 32bit (BE) decoding */

   bool dsd_u16_le; /**< Support DSD unsigned 16bit (LE) decoding */
   bool dsd_u16_be; /**< Support DSD unsigned 16bit (BE) decoding */
   bool dsd_u32_le; /**< Support DSD unsigned 32bit (LE) decoding */
   bool dsd_u32_be; /**< Support DSD unsigned 32bit (BE) decoding */
} __attribute__((aligned(64)));

/** @struct device
 * Defines a device
 */
struct device
{
   char id[MISC_LENGTH];             /**< The identifier of the device */
   char name[MISC_LENGTH];           /**< The full name of the device */
   char device[MISC_LENGTH];         /**< The device */
   char description[MISC_LENGTH];    /**< The description of the device */
   struct capabilities capabilities; /**< The capabilities of the device */
   bool active;                      /**< Is the device active ? */
} __attribute__((aligned(64)));

/** @struct configuration
 * Defines the configuration and state of hrmp
 */
struct configuration
{
   char configuration_path[MAX_PATH]; /**< The configuration path */

   char device[MISC_LENGTH];          /**< The name of the default device */

   bool quiet;                        /**< The quiet the output */

   bool experimental;                 /**< Allow experimental features */

   int log_type;                      /**< The logging type */
   int log_level;                     /**< The logging level */
   char log_path[MISC_LENGTH];        /**< The logging path */
   int log_mode;                      /**< The logging mode */
   char log_line_prefix[MISC_LENGTH]; /**< The logging prefix */
   atomic_schar log_lock;             /**< The logging lock */

   unsigned int update_process_title; /**< Behaviour for updating the process title */

   int number_of_devices;             /**< The number of devices */

   struct device devices[NUMBER_OF_DEVICES]; /**< The devices */
} __attribute__((aligned(64)));

#ifdef __cplusplus
}
#endif

#endif
