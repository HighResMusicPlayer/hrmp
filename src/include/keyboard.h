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

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <hrmp.h>

#include <stdbool.h>
#include <stdio.h>

#define KEYBOARD_IGNORE  0
#define KEYBOARD_Q       1
#define KEYBOARD_UP      2
#define KEYBOARD_DOWN    3
#define KEYBOARD_LEFT    4
#define KEYBOARD_RIGHT   5
#define KEYBOARD_ENTER   6
#define KEYBOARD_SPACE   7
#define KEYBOARD_COMMA   8
#define KEYBOARD_PERIOD  9
#define KEYBOARD_M      10
#define KEYBOARD_SLASH  11

/**
 * Get a keyboard command
 * @return The command, otherwise empty
 */
int
hrmp_keyboard_mode(bool enable);

/**
 * Get a keyboard command
 * @return The command, otherwise KEYBOARD_IGNORE
 */
int
hrmp_keyboard_get(void);

#endif
