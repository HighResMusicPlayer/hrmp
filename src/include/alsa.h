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

#ifndef HRMP_ALSA_H
#define HRMP_ALSA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <hrmp.h>
#include <files.h>

#include <stdbool.h>
#include <stdlib.h>

#include <alsa/asoundlib.h>

/**
 * Initialize the ALSA handle
 * @param fm The file metadata
 * @param handle The resulting handle
 * @return 0 upon success, 1 is failure
 */
int
hrmp_alsa_init_handle(struct file_metadata* fm, snd_pcm_t** handle);

/**
 * Reset the ALSA handle
 * @param handle The handle
 * @return 0 upon success, 1 is failure
 */
int
hrmp_alsa_reset_handle(snd_pcm_t* handle);

/**
 * Close the ALSA handle
 * @param handle The resulting handle
 * @return 0 upon success, 1 is failure
 */
int
hrmp_alsa_close_handle(snd_pcm_t* handle);

/**
 * Initialize the volume for the active device
 * @return 0 upon success, 1 is failure
 */
int
hrmp_alsa_init_volume(void);

/**
 * Get the volume for the active device
 * @param volume The volume
 * @return 0 upon success, 1 is failure
 */
int
hrmp_alsa_get_volume(int* volume);

/**
 * Set the volume for the active device
 * @param volume The volume
 * @return 0 upon success, 1 is failure
 */
int
hrmp_alsa_set_volume(int volume);

#ifdef __cplusplus
}
#endif

#endif
