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
#include <devices.h>
#include <logging.h>
#include <playback.h>
#include <wav.h>

/* system */
/* #include <errno.h> */
/* #include <stdlib.h> */
/* #include <string.h> */
/* #include <sys/mman.h> */

#include <alsa/asoundlib.h>

int
hrmp_playback_wav(char* path)
{
   /* int err; */
   /* snd_pcm_t* pcm = NULL; */
   /* /\* struct device* device = NULL; *\/ */
   /* struct wav* wav = NULL; */

   /* /\* device = hrmp_get_device(); *\/ */

   /* if ((err = snd_pcm_open(&pcm, device->device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) */
   /* { */
   /*    hrmp_log_fatal("Can not play %s due to %s", path, snd_strerror(err)); */
   /*    goto error; */
   /* } */

   /* if (hrmp_wav_open(path, WAV_INTERLEAVED, &wav)) */
   /* { */
   /*    hrmp_log_fatal("Could not open %s", path); */
   /*    goto error; */
   /* } */

   /* if ((err = snd_pcm_close(pcm)) < 0) */
   /* { */
   /*    hrmp_log_debug("Error in closing PCM: %s", snd_strerror(err)); */
   /* } */

   return 0;

/* error: */

/*    if (pcm != NULL) */
/*    { */
/*       if ((err = snd_pcm_close(pcm)) < 0) */
/*       { */
/*          hrmp_log_debug("Error in closing PCM: %s", snd_strerror(err)); */
/*       } */
/*    } */

/*    return 1; */
}
