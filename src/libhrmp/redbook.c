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

#include <hrmp.h>
#include <logging.h>
#include <redbook.h>
#include <sndfile-64.h>
#include <utils.h>

#include <inttypes.h>
#include <sndfile.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <cdio/cdio.h>

#define HRMP_CDDA_SECTORS_PER_READ 16

static CdIo_t* hrmp_redbook_open_device(char* device);
static int hrmp_redbook_extract_track(CdIo_t* cdio, track_t track);

int
hrmp_extract_redbook(char* d)
{
   CdIo_t* cdio = hrmp_redbook_open_device(d);
   if (cdio == NULL)
   {
      return 1;
   }

   track_t first_track = cdio_get_first_track_num(cdio);
   track_t last_track = cdio_get_last_track_num(cdio);

   if (first_track == CDIO_INVALID_TRACK || last_track == CDIO_INVALID_TRACK)
   {
      hrmp_log_error("CDDA: invalid track range");
      cdio_destroy(cdio);
      return 1;
   }

   bool any_audio = false;
   int result = 0;

   for (track_t t = first_track; t <= last_track; t++)
   {
      track_format_t fmt = cdio_get_track_format(cdio, t);
      if (fmt != TRACK_FORMAT_AUDIO)
      {
         continue;
      }

      any_audio = true;
      if (hrmp_redbook_extract_track(cdio, t) != 0)
      {
         result = 1;
         break;
      }
   }

   if (!any_audio)
   {
      hrmp_log_error("CDDA: no audio tracks found on %s", d);
      result = 1;
   }

   cdio_destroy(cdio);
   return result;
}

static CdIo_t*
hrmp_redbook_open_device(char* device)
{
   if (device == NULL || device[0] == '\0')
   {
      hrmp_log_error("CDDA: invalid device");
      goto error;
   }

   CdIo_t* cdio = cdio_open(device, DRIVER_UNKNOWN);
   if (cdio == NULL)
   {
      hrmp_log_error("CDDA: unable to open device %s", device);
      goto error;
   }

   return cdio;

error:

   return NULL;
}

static int
hrmp_redbook_extract_track(CdIo_t* cdio, track_t track)
{
   char filename[MAX_PATH];
   SF_INFO sfinfo;
   SNDFILE* sf = NULL;
   lba_t start_lba = cdio_get_track_lba(cdio, track);
   lba_t sectors = cdio_get_track_sec_count(cdio, track);
   uint8_t sector_buf[HRMP_CDDA_SECTORS_PER_READ * CDIO_CD_FRAMESIZE_RAW];
   int16_t pcm_buf[HRMP_CDDA_SECTORS_PER_READ * CDIO_CD_FRAMESIZE_RAW / 2];
   lba_t current = start_lba;
   lba_t end_lba = start_lba + sectors;

   if (sectors <= 0)
   {
      return 0;
   }

   hrmp_snprintf(filename, sizeof(filename), "%02u.flac", (unsigned)track);

   memset(&sfinfo, 0, sizeof(sfinfo));
   sfinfo.samplerate = 44100;
   sfinfo.channels = 2;
   sfinfo.format = SF_FORMAT_FLAC | SF_FORMAT_PCM_16;

   sf = sf_open(filename, SFM_WRITE, &sfinfo);
   if (sf == NULL)
   {
      hrmp_log_error("CDDA: unable to open output file %s: %s", filename, sf_strerror(NULL));
      goto error;
   }

   while (current < end_lba)
   {
      lba_t remaining = end_lba - current;
      unsigned nsectors = (unsigned)(remaining > HRMP_CDDA_SECTORS_PER_READ
                                        ? HRMP_CDDA_SECTORS_PER_READ
                                        : remaining);

      if (cdio_read_audio_sectors(cdio, sector_buf, current, nsectors) != DRIVER_OP_SUCCESS)
      {
         hrmp_log_error("CDDA: read error at LBA %" PRIu32 " (track %u)", (uint32_t)current, (unsigned)track);
         goto error;
      }

      size_t bytes = (size_t)nsectors * (size_t)CDIO_CD_FRAMESIZE_RAW;
      size_t samples = bytes / 2u;

      for (size_t i = 0; i < samples; i++)
      {
         uint8_t hi = sector_buf[2u * i];
         uint8_t lo = sector_buf[2u * i + 1u];
         uint16_t u = (uint16_t)((uint16_t)hi << 8) | (uint16_t)lo;

         pcm_buf[i] = (int16_t)u;
      }

      sf_count_t frames = (sf_count_t)(samples / 2u);
      sf_count_t written = sf_writef_short(sf, pcm_buf, frames);
      if (written != frames)
      {
         hrmp_log_error("CDDA: write error for %s: %s", filename, sf_strerror(sf));
         goto error;
      }

      current += nsectors;
   }

   sf_close(sf);

   return 0;

error:

   if (sf != NULL)
   {
      sf_close(sf);
   }

   return 1;
}
