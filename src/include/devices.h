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

#ifndef HRMP_DEVICES_H
#define HRMP_DEVICES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <hrmp.h>

#include <stdbool.h>
#include <stdlib.h>

/**
 * Check if IEC598 devices are active
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
 * Initialize a device
 * @param device The device
 * @return 0 upon success, otherwise 1
 */
int
hrmp_init_device(struct device* device);

/**
 * Create an active device
 * @param device_name The device name
 * @return 0 upon success, otherwise 1
 */
int
hrmp_create_active_device(char* device_name);

/**
 * Activate a device
 * @param name The device name
 * @return 0 upon success, otherwise 1
 */
int
hrmp_activate_device(char* name);

/**
 * Print devices
 */
void
hrmp_print_devices(void);

/**
 * Print a device
 * @param device The device
 */
void
hrmp_print_device(struct device* device);

/**
 * Print a sample configuration
 */
void
hrmp_sample_configuration(void);

/**
 * List fallback devices
 */
void
hrmp_list_fallback_devices(void);

#ifdef __cplusplus
}
#endif

#endif
