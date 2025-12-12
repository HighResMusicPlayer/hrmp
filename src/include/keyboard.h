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

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <hrmp.h>

#include <stdbool.h>
#include <stdio.h>

#define KEYBOARD_IGNORE     0
#define KEYBOARD_Q          1
#define KEYBOARD_UP         2
#define KEYBOARD_DOWN       3
#define KEYBOARD_LEFT       4
#define KEYBOARD_RIGHT      5
#define KEYBOARD_ENTER      6
#define KEYBOARD_SPACE      7
#define KEYBOARD_COMMA      8
#define KEYBOARD_PERIOD     9
#define KEYBOARD_M         10
#define KEYBOARD_SLASH     11
#define KEYBOARD_BACKSLASH 12

/**
 * Get a keyboard command
 * @return The command, otherwise empty
 */
int
hrmp_keyboard_mode(bool enable);

/**
 * Get a keyboard command
 * @param keyboard_code Debug information
 * @return The command, otherwise KEYBOARD_IGNORE
 */
int
hrmp_keyboard_get(char** keyboard_code);

#endif
