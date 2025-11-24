/*
 * Copyright (C) 2025 The HighResMusicPlayer community
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

#ifndef HRMP_DEVICES_H
#define HRMP_DEVICES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <hrmp.h>

#include <stdbool.h>
#include <stdlib.h>

/**
 * Check if devices are active
 */
void
hrmp_check_devices(void);

/**
 * Is the device known in the configuration
 * @param name The device name
 * @return True if known, otherwise false
 */
bool
hrmp_is_device_known(char* name);

/**
 * Return the active device
 * @param name Device hint
 * @return The active device, or -1
 */
int
hrmp_active_device(char* name);

/**
 * Print devices
 */
void
hrmp_print_devices(void);

/**
 * Print a sample configuration
 */
void
hrmp_sample_configuration(void);

#ifdef __cplusplus
}
#endif

#endif
